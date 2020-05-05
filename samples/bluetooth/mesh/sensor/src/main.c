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

static struct device *dev;
static struct sensor_value sensor_val;

void main(void)
{
	dk_leds_init();
	dk_buttons_init(NULL);

	dev = device_get_binding(DT_INST_0_NORDIC_NRF_TEMP_LABEL);

	if (dev == NULL) {
		printk("Could not get device\n");
		return;
	}

	printk("*** Sensor reading test - Device: %s ***\n", dev->config->name);

	while (true) {
		sensor_sample_fetch(dev);
		sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, &sensor_val);

		printk("*** Sensor value: %d.%02d ***\n", sensor_val.val1, sensor_val.val2);

		k_sleep(K_MSEC(1000));
	}
}
