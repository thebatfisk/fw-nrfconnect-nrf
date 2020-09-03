/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>

#include "ethernet_command_system.h"
#include "pca20036_ethernet.h"
#include "w5500.h"
#include "socket.h"
#include "ethernet_utils.h"
#include "hp_led.h"
#include "ethernet_dfu.h"

static struct k_delayed_work i_am_alive_work;

static uint8_t received_data[200];
static struct command_system_package received_package;

static uint16_t cmd_rx_port = RX_PORT;
static uint8_t own_mac[6];
static uint8_t own_ip[4];
static uint8_t server_ip[4];

static void i_am_alive_work_handler(struct k_work *work)
{
	struct ethernet_i_am_alive_package i_am_alive_package;

	get_own_ip((uint8_t *)&i_am_alive_package.ip);
	get_own_mac((uint8_t *)&i_am_alive_package.mac);
	i_am_alive_package.version = DFU_APP_VERSION;

	send_over_ethernet((uint8_t *)&i_am_alive_package, CMD_I_AM_ALIVE);

	k_delayed_work_submit(&i_am_alive_work, K_MSEC(5000));
}

static void i_am_alive_work_init_start(void)
{
	k_delayed_work_init(&i_am_alive_work, i_am_alive_work_handler);
	k_delayed_work_submit(&i_am_alive_work, K_NO_WAIT);
}

void ethernet_command_system_rx(void)
{
	while (getSn_RX_RSR(SOCKET_RX) != 0x00) {
		get_own_ip(own_ip);

		int32_t recv_len = recvfrom(SOCKET_RX, received_data,
					    sizeof(received_data), server_ip,
					    &cmd_rx_port);

		if (recv_len >= 4) // 4 because of the first memcpy
		{
			uint32_t identifier;
			memcpy(&identifier, received_data, 4);

			if (identifier == 0xDEADFACE) {
				memcpy(&received_package, received_data,
				       sizeof(received_package));

				switch (received_package.opcode) {
				case CMD_RESET:
					if (received_package.payload
						    .reset_package
						    .is_broadcast ||
					    mac_addresses_are_equal(
						    own_mac,
						    received_package.payload
							    .reset_package
							    .target_mac)) {
						printk("CMD: Reset node\n");
						NVIC_SystemReset();
					}
					break;
				case CMD_DFU:
					if (received_package.payload.led_package
						    .is_broadcast ||
					    mac_addresses_are_equal(
						    own_mac,
						    received_package.payload
							    .led_package
							    .target_mac)) {
						ethernet_dfu_start(server_ip);
					}
					break;
				case CMD_LED:
					if (received_package.payload.led_package
						    .is_broadcast ||
					    mac_addresses_are_equal(
						    own_mac,
						    received_package.payload
							    .led_package
							    .target_mac)) {
						if (received_package.payload
							    .led_package
							    .on_off) {
							printk("CMD: HP LED ON\n");
							hp_led_on();
						} else {
							printk("CMD: HP LED OFF\n");
							hp_led_off();
						}
					}
					break;
				default:
					printk("CMD: Unrecognized control command: %d\n",
					       received_package.opcode);
					break;
				}
			}
		}
	}
}

void send_over_ethernet(uint8_t *payload_package, uint8_t opcode)
{
	struct command_system_package package;
	package.identifier = 0xDEADFACE;
	package.opcode = opcode;
	get_own_mac((uint8_t *)&package.mac);

	bool send_package = true;

	switch (opcode) {
	case CMD_I_AM_ALIVE: {
		struct ethernet_i_am_alive_package i_am_alive_package;
		memcpy(&i_am_alive_package, payload_package,
		       sizeof(struct ethernet_i_am_alive_package));
		package.payload.i_am_alive_package = i_am_alive_package;
		break;
	}
	default:
		printk("send_over_ethernet: Unknown package type chosen\n");
		send_package = false;
		break;
	}

	if (send_package) {
		int32_t err = sendto(SOCKET_TX, (uint8_t *)&package,
				     sizeof(struct command_system_package),
				     (uint8_t *)TX_IP, TX_PORT);

		if (err < 0) {
			printk("Error sending packet - error code: %d\n", err);
		}
	}
}

void ethernet_command_system_init(void)
{
	get_own_mac(own_mac);
	i_am_alive_work_init_start();
}
