/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <dk_buttons_and_leds.h>
#include "pca20036_ethernet.h"
#include "ethernet_command_system.h"
#include "ethernet_dfu.h"
#include "hp_led.h"

static struct k_delayed_work ethernet_rx_work;

static void button_handler(uint32_t button_state, uint32_t has_changed)
{
	static bool onoff = true;

	if (button_state & BIT(0)) {
		printk("Button 0 pressed\n");
		onoff = !onoff;
	}
}

static void ethernet_rx_work_handler(struct k_work *work)
{
	ethernet_command_system_rx();
	k_delayed_work_submit(&ethernet_rx_work, K_MSEC(10));
}

static void ethernet_rx_work_init_start(void)
{
	k_delayed_work_init(&ethernet_rx_work, ethernet_rx_work_handler);
	k_delayed_work_submit(&ethernet_rx_work, K_NO_WAIT);
}

void main(void)
{
	int err;

	printk("- PCA20036 sample -\n");
	printk("- Version: 1 -\n");

	err = hp_led_init();

	if (err) {
		printk("Error initializing HP LED\n");
		return;
	}

	err = dk_buttons_init(button_handler);

	if (err) {
		printk("Error initializing buttons\n");
		return;
	}

	err = pca20036_ethernet_init();

	if (err) {
		printk("Error initializing buttons\n");
		return;
	}

	ethernet_rx_work_init_start();

	printk("- Initiated -\n");
}
