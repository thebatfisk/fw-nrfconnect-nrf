/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 *  @brief Nordic Mesh light sample
 */
// #include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

static struct sensor_value sensor_val;
static struct device *dev;

void my_work_handler(struct k_work *work)
{
    sensor_sample_fetch(dev);
	sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &sensor_val);

	printk("*** Sensor value: %d.%02d ***\n", sensor_val.val1, sensor_val.val2);
}

K_WORK_DEFINE(my_work, my_work_handler);

void my_timer_handler(struct k_timer *dummy)
{
	k_work_submit(&my_work);
}

K_TIMER_DEFINE(my_timer, my_timer_handler, NULL);

void button_handler(u32_t button_state, u32_t has_changed)
{
	u32_t button = button_state & has_changed;

	if (button & DK_BTN1_MSK) 
	{
	k_work_submit(&my_work);
	}
	
}

void main(void)
{
	// int err;

	dk_leds_init();
	dk_buttons_init(button_handler);

	// *** Sensor

	dev = device_get_binding(DT_INST_0_NORDIC_NRF_TEMP_LABEL);

	if (dev == NULL) {
		printk("Could not get device\n");
	}

	printk("*** Sensor reading test - Device: %s ***\n", dev->config->name);

	// *** Timer
	
	/* start periodic timer that expires once every second */
	k_timer_start(&my_timer, K_SECONDS(1), K_SECONDS(5));
	
	// *** Mesh ***
	
	// err = bt_enable(NULL);
	// if (err) {
	// 	printk("Bluetooth init failed (err %d)\n", err);
	// 	return;
	// }

	// err = bt_mesh_init(bt_mesh_dk_prov_init(), model_handler_init());
	// if (err) {
	// 	printk("Initializing mesh failed (err %d)\n", err);
	// 	return;
	// }

	// if (IS_ENABLED(CONFIG_SETTINGS)) {
	// 	settings_load(); // "DSM"
	// }

	// /* This will be a no-op if settings_load() loaded provisioning info */
	// bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

	// printk("Mesh initialized\n");

	// ************

	while (true) {
		
	}
}
