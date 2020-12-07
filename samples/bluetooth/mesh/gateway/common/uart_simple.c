/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <drivers/uart.h>
#include <net/buf.h>
#include <sys/crc.h>
#include <errno.h>
#include "slip.h"
#include "uart_simple.h"

#define CHECKSUM_SIZE 2
#define UART_PACKET_RECIEVED_SUCCESS 0
#define CHANNEL_ID_SIZE 1
#define CHANNEL_CTX_MAX 4
#define SLIP_BUF_SIZE 512
#define UART_RX_THREAD_STACK_SIZE 4096
#define UART_RX_THREAD_PRIORITY 7
#define UART_RX_POOL_SIZE 2048
#define UART_RX_POOL_MAX_ENTRIES 16

K_MUTEX_DEFINE(uart_send_mtx);

/* Uart device */
static struct device *uart_dev;
static rx_cb rx_callback;

static uint8_t channel_cnt;
static struct uart_channel_ctx *channel_list[CHANNEL_CTX_MAX];

/* Tx thread for Uart */
static K_THREAD_STACK_DEFINE(rx_thread_stack, UART_RX_THREAD_STACK_SIZE);
static struct k_thread rx_thread_data;
static K_FIFO_DEFINE(tx_queue);

NET_BUF_POOL_VAR_DEFINE(uart_pool, UART_RX_POOL_MAX_ENTRIES, UART_RX_POOL_SIZE,
			NULL);

static void uart_isr(struct device *unused, void *user_data)
{
	static uint8_t slip_buffer[SLIP_BUF_SIZE];
	static slip_t slip = { .state = SLIP_STATE_DECODING,
			       .p_buffer = slip_buffer,
			       .current_index = 0,
			       .buffer_len = sizeof(slip_buffer) };

	ARG_UNUSED(unused);
	ARG_UNUSED(user_data);

	while (uart_irq_update(uart_dev) && uart_irq_is_pending(uart_dev)) {
		if (!uart_irq_rx_ready(uart_dev)) {
			if (uart_irq_tx_ready(uart_dev)) {
				printk("transmit ready\n");
			} else {
				printk("spurious interrupt\n");
			}
			/* Only the UART RX path is interrupt-enabled */
			break;
		}

		uint8_t byte;

		uart_fifo_read(uart_dev, &byte, sizeof(byte));
		// printk("Byte recieved on uart: %d\n", byte);
		int ret_code = slip_decode_add_byte(&slip, byte);

		switch (ret_code) {
		case UART_PACKET_RECIEVED_SUCCESS: {
			uint16_t checksum_local =
				crc16_ansi(slip.p_buffer,
					   slip.current_index - CHECKSUM_SIZE);
			uint16_t checksum_incoming;

			memcpy(&checksum_incoming,
			       slip.p_buffer +
				       (slip.current_index - CHECKSUM_SIZE),
			       sizeof(checksum_incoming));

			if (checksum_incoming == checksum_local) {
				struct net_buf *buf = net_buf_alloc_len(
					&uart_pool,
					slip.current_index - CHECKSUM_SIZE,
					K_NO_WAIT);
				net_buf_add_mem(buf, slip.p_buffer,
						slip.current_index -
							CHECKSUM_SIZE);
				net_buf_put(&tx_queue, buf);
			} else {
				printk("Invalid Checksum\n");
			}
		}
			// fall through
		case -ENOMEM:
			slip.current_index = 0;
			slip.state = SLIP_STATE_DECODING;
			break;

		default:
			break;
		}
	}
}

static void uart_irq_init(void)
{
	uart_irq_rx_disable(uart_dev);
	uart_irq_tx_disable(uart_dev);
	uart_irq_callback_set(uart_dev, uart_isr);
	uart_irq_rx_enable(uart_dev);
}

void uart_simple_send(struct uart_channel_ctx *channel_ctx, uint8_t *data,
		      uint16_t len)
{
	k_mutex_lock(&uart_send_mtx, K_FOREVER);
	printk("Ctx %d ENTER\n", channel_ctx->channel_id);
	uint8_t buf[len + CHECKSUM_SIZE + CHANNEL_ID_SIZE];
	uint8_t buf_idx = 0;

	memcpy(buf + buf_idx, &channel_ctx->channel_id, CHANNEL_ID_SIZE);
	buf_idx += CHANNEL_ID_SIZE;

	memcpy(buf + buf_idx, data, len);
	buf_idx += len;

	/* Add Data and Data length to outgoing packet buffer*/
	uint16_t checksum = crc16_ansi(buf, sizeof(buf) - CHECKSUM_SIZE);

	memcpy(buf + buf_idx, &checksum, sizeof(checksum));

	/* Wrap outgoing packet with SLIP */
	uint8_t slip_buf[sizeof(buf) * 2];
	uint32_t slip_buf_len;

	slip_encode(slip_buf, buf, sizeof(buf), &slip_buf_len);

	/* Send packet over UART */
	for (int i = 0; i < slip_buf_len; i++) {
		uart_poll_out(uart_dev, slip_buf[i]);
	}
	printk("Ctx %d ENDS\n", channel_ctx->channel_id);
	k_mutex_unlock(&uart_send_mtx);
}

static void uart_channel_clear(void)
{
	struct uart_channel_ctx dummy_ctx = {
		.channel_id = 0,
		.rx_cb = NULL,
	};
	uint8_t dummy_array[] = "FF";

	uart_simple_send(&dummy_ctx, dummy_array, sizeof(dummy_array));
}

void uart_simple_channel_create(struct uart_channel_ctx *channel_ctx)
{
	if (channel_cnt >= CHANNEL_CTX_MAX) {
		printk("Uart channels are maxed out\n");
		return;
	}
	for (size_t i = 0; i < channel_cnt; i++) {
		if (channel_list[i]->channel_id == channel_ctx->channel_id) {
			printk("Uart channel %d is already in use\n",
			       channel_ctx->channel_id);
			return;
		}
	}

	channel_list[channel_cnt] = channel_ctx;
	channel_cnt++;
}

static void rx_thread(void *p1, void *p2, void *p3)
{
	while (1) {
		struct net_buf *get_buf = net_buf_get(&tx_queue, K_FOREVER);
		uint8_t id = net_buf_pull_u8(get_buf);

		for (size_t i = 0; i < channel_cnt; i++) {
			if (channel_list[i]->channel_id == id) {
				channel_list[i]->rx_cb(get_buf);
				goto end_chan;
			}
		}

		if (rx_callback) {
			rx_callback(get_buf);
		} else {
			printk("Incoming Uart packet thrown away\n");
		}

	end_chan:
		net_buf_unref(get_buf);
		k_yield();
	}
}

static void rx_thread_create(void)
{
	k_thread_create(&rx_thread_data, rx_thread_stack,
			K_THREAD_STACK_SIZEOF(rx_thread_stack), rx_thread, NULL,
			NULL, NULL, K_PRIO_COOP(UART_RX_THREAD_PRIORITY), 0, K_NO_WAIT);
	k_thread_name_set(&rx_thread_data, "Uart Simple Rx");
}

void uart_simple_init(rx_cb default_cb)
{
	printk("Uart Simple started\n");

	if (default_cb) {
		rx_callback = default_cb;
	}

	uart_dev = device_get_binding("UART_1");

	uart_irq_init();
	rx_thread_create();
	uart_channel_clear();
}
