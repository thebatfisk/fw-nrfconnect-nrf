/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 *  @brief Nordic Mesh light sample
 */
#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

static struct sensor_value sensor_val;

void main(void)
{
	int err;

	dk_leds_init();
	dk_buttons_init(NULL);

	// *** Mesh ***
	
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	err = bt_mesh_init(bt_mesh_dk_prov_init(), model_handler_init());
	if (err) {
		printk("Initializing mesh failed (err %d)\n", err);
		return;
	}

	if (IS_ENABLED(CONFIG_SETTINGS)) {
		settings_load(); // "DSM"
	}

	/* This will be a no-op if settings_load() loaded provisioning info */
	bt_mesh_prov_enable(BT_MESH_PROV_ADV | BT_MESH_PROV_GATT);

	printk("Mesh initialized\n");

	// ************

	// while (true) {
	// 	sensor_sample_fetch(dev);
	// 	sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &sensor_val);

	// 	printk("*** Sensor value: %d.%02d ***\n", sensor_val.val1, sensor_val.val2);

	// 	k_sleep(K_MSEC(1000));
	// }
}
