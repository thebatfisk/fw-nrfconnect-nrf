/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <string.h>
#include <drivers/spi.h>
#include <drivers/gpio.h>

#include "pca20036_ethernet.h"
#include "w5500.h"
#include "socket.h"
#include "dhcp.h"
#include "dhcp_cb.h"
#include "ethernet_dfu.h"
#include "ethernet_command_system.h"

K_MUTEX_DEFINE(wiz_mtx);

static const struct device *spi_0;
static const struct device *gpio_1;

static struct k_delayed_work dhcp_time_work;
static struct k_delayed_work dhcp_lease_work;

static uint8_t dhcp_tx_buf[TX_BUF_SIZE];
static uint8_t own_mac[6];
static uint8_t own_ip[4];
static bool dhcp_leased;

static wiz_NetInfo gWIZNETINFO;

static const struct spi_config spi_cfg = {
	.operation = SPI_WORD_SET(8) | SPI_TRANSFER_MSB | SPI_MODE_CPOL |
		     SPI_MODE_CPHA,
	.frequency = 4000000,
	.slave = 0,
};

static void print_network_info(void)
{
	int8_t err;
	uint8_t tmp_str[6];

	err = ctlnetwork(CN_GET_NETINFO, (void *)&gWIZNETINFO);
	if (err == -1) {
		printk("Error printing network info\n");
		return;
	}

	err = ctlwizchip(CW_GET_ID, (void *)tmp_str);
	if (err == -1) {
		printk("Error printing network info\n");
		return;
	}

	printk("=== %s NET CONF ===\n", (char *)tmp_str);
	printk("MAC:\t %02X:%02X:%02X:%02X:%02X:%02X\n", gWIZNETINFO.mac[0],
	       gWIZNETINFO.mac[1], gWIZNETINFO.mac[2], gWIZNETINFO.mac[3],
	       gWIZNETINFO.mac[4], gWIZNETINFO.mac[5]);
	printk("SIP:\t %d.%d.%d.%d\n", gWIZNETINFO.ip[0], gWIZNETINFO.ip[1],
	       gWIZNETINFO.ip[2], gWIZNETINFO.ip[3]);
	printk("GAR:\t %d.%d.%d.%d\n", gWIZNETINFO.gw[0], gWIZNETINFO.gw[1],
	       gWIZNETINFO.gw[2], gWIZNETINFO.gw[3]);
	printk("SUB:\t %d.%d.%d.%d\n", gWIZNETINFO.sn[0], gWIZNETINFO.sn[1],
	       gWIZNETINFO.sn[2], gWIZNETINFO.sn[3]);
	printk("DNS:\t %d.%d.%d.%d\n", gWIZNETINFO.dns[0], gWIZNETINFO.dns[1],
	       gWIZNETINFO.dns[2], gWIZNETINFO.dns[3]);
	printk("=======================\n");
}

static uint8_t wizchip_read(void)
{
	uint8_t read_buf;
	const struct spi_buf rx_buf = { .buf = &read_buf,
					.len = sizeof(read_buf) };
	const struct spi_buf_set rx = { .buffers = &rx_buf, .count = 1 };

	if (spi_transceive(spi_0, &spi_cfg, NULL, &rx)) {
		printk("SPI read failed\n");
	}

	return read_buf;
}

static void wizchip_write(uint8_t write_buf)
{
	const struct spi_buf tx_buf = { .buf = &write_buf,
					.len = sizeof(write_buf) };
	const struct spi_buf_set tx = { .buffers = &tx_buf, .count = 1 };

	if (spi_transceive(spi_0, &spi_cfg, &tx, NULL)) {
		printk("SPI write failed\n");
	}
}

static void critical_section_enter(void)
{
	k_mutex_lock(&wiz_mtx, K_FOREVER);
}

static void critical_section_exit(void)
{
	k_mutex_unlock(&wiz_mtx);
}

static void wizchip_select(void)
{
	gpio_pin_set_raw(gpio_1, 9, 0);
}

static void wizchip_deselect(void)
{
	gpio_pin_set_raw(gpio_1, 9, 1);
}

static void w5500_spi_init(void)
{
	spi_0 = device_get_binding(DT_PROP(DT_NODELABEL(spi0), label));
	gpio_1 = device_get_binding(DT_PROP(DT_NODELABEL(gpio1), label));

	if (spi_0 == NULL || gpio_1 == NULL) {
		printk("SPI init: Error getting devices\n");
		return;
	}

	gpio_pin_configure(gpio_1, 9, GPIO_OUTPUT | GPIO_OUTPUT_INIT_LOW);
}

static void dhcp_time_work_handler(struct k_work *work)
{
	DHCP_time_handler();
	k_delayed_work_submit(&dhcp_time_work, K_MSEC(1000));
}

static void dhcp_time_work_init_start(void)
{
	k_delayed_work_init(&dhcp_time_work, dhcp_time_work_handler);
	k_delayed_work_submit(&dhcp_time_work, K_NO_WAIT);
}

static void dhcp_lease_work_handler(struct k_work *work)
{
	static uint8_t dhcp_retry = 0;
	uint32_t ret;

	ret = DHCP_run();

	if (ret == DHCP_RUNNING) {
		k_delayed_work_submit(&dhcp_lease_work, K_MSEC(50));
	} else if (ret == DHCP_IP_LEASED) {
		getSHAR(own_mac);
		getIPfromDHCP(own_ip);
		print_network_info();
		dhcp_leased = true;

		ethernet_command_system_init();

		// Maybe move ethernet_dfu_confirm_image() to a better place
		ethernet_dfu_confirm_image();
	} else if (ret == DHCP_FAILED && dhcp_retry > 50) { // 2.5 seconds
		printk("DHCP failed to lease\n");

		// Maybe move NVIC_SystemReset() to a better place
		NVIC_SystemReset();
	} else if (ret == DHCP_FAILED) {
		dhcp_retry++;
		k_delayed_work_submit(&dhcp_lease_work, K_MSEC(50));
	} else {
		printk("Unknown DHCP return value: %d\n", ret);
	}
}

static void dhcp_lease_work_init_start(void)
{
	k_delayed_work_init(&dhcp_lease_work, dhcp_lease_work_handler);
	k_delayed_work_submit(&dhcp_lease_work, K_MSEC(10));
}

static void ethernet_dhcp_init(void)
{
	dhcp_time_work_init_start();
	DHCP_init(SOCKET_DHCP, dhcp_tx_buf);
	reg_dhcp_cbfunc(w5500_dhcp_assign, w5500_dhcp_assign,
			w5500_dhcp_conflict);
	dhcp_lease_work_init_start();
}

static void sockets_init(void)
{
	socket(SOCKET_TX, Sn_MR_UDP, TX_PORT, TX_FLAGS);
	socket(SOCKET_RX, Sn_MR_UDP, RX_PORT, SF_IO_NONBLOCK);
}

/******* Interface functions *******/

void get_own_ip(uint8_t *p_ip)
{
	memcpy(p_ip, own_ip, 4);
}

void get_own_mac(uint8_t *p_mac)
{
	memcpy(p_mac, own_mac, 6);
}

bool get_dhcp_leased_flag(void)
{
	return dhcp_leased;
}

int pca20036_ethernet_init(void)
{
	volatile uint8_t tmp;
	int8_t err;
	uint8_t phy_link_counter = 0;
	uint8_t memsize[2][8] = { { 2, 2, 2, 2, 2, 2, 2, 2 },
				  { 2, 2, 2, 2, 2, 2, 2, 2 } };
	wiz_NetTimeout timeout_info;

	printk("*W5500 ethernet init*\n");

	w5500_spi_init();

	gWIZNETINFO.mac[0] = (0xB0) & 0xFF;
	gWIZNETINFO.mac[1] = (NRF_FICR->DEVICEADDR[0] >> 8) & 0xFF;
	gWIZNETINFO.mac[2] = (NRF_FICR->DEVICEADDR[0] >> 16) & 0xFF;
	gWIZNETINFO.mac[3] = (NRF_FICR->DEVICEADDR[0] >> 24);
	gWIZNETINFO.mac[4] = (NRF_FICR->DEVICEADDR[1]) & 0xFF;
	gWIZNETINFO.mac[5] = (NRF_FICR->DEVICEADDR[1] >> 8) & 0xFF;

	reg_wizchip_cris_cbfunc(critical_section_enter, critical_section_exit);
	reg_wizchip_cs_cbfunc(wizchip_select, wizchip_deselect);
	reg_wizchip_spi_cbfunc(wizchip_read, wizchip_write);

	err = ctlwizchip(CW_INIT_WIZCHIP, (void *)memsize);
	if (err == -1) {
		printk("W5500 memory initialization failed\n");
		return -1;
	}

	do {
		if (phy_link_counter > 10) {
			printk("Unable to get physical link\n");
			return -1;
		}

		err = ctlwizchip(CW_GET_PHYLINK, (void *)&tmp);

		if (err == -1) {
			printk("Unknown PHY link stauts (counter: %d)\n",
			       phy_link_counter);
		}

		if (tmp != PHY_LINK_ON) {
			printk("Pyhisical link is not established (status: %d)\n",
			       tmp);
		}

		phy_link_counter++;
		k_sleep(K_MSEC(500));
	} while (tmp != PHY_LINK_ON);

	printk("W5500 PHY link status is ON\n");

	timeout_info.retry_cnt = 1;
	timeout_info.time_100us = 1000; // timeout value = 10ms
	wizchip_settimeout(&timeout_info);

	err = ctlnetwork(CN_SET_NETINFO, (void *)&gWIZNETINFO);
	if (err == -1) {
		printk("Error setting network info\n");
		return -1;
	}

	ethernet_dhcp_init();

	ethernet_dfu_init();

	sockets_init();

	return 0;
}
