/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdio.h>
#include <stdlib.h>
#include <zephyr.h>
#include <device.h>
#include <drivers/sensor.h>
#include <thingy52_orientation_handler.h>

static struct device *sensor;

orientation_t thingy52_orientation_get(void)
{
	struct sensor_value accel[3];
	int rc = sensor_sample_fetch(sensor);

	if (rc == 0) {
		rc = sensor_channel_get(sensor, SENSOR_CHAN_ACCEL_XYZ, accel);
	}
	if (rc == 0) {
		orientation_t orr = THINGY_ORIENT_NONE;
		int32_t top_val = 0;

		for (size_t i = 0; i < 3; i++) {
			if (abs(accel[i].val1) > abs(top_val)) {
				top_val = accel[i].val1;
				if (top_val < 0) {
					orr = i + 3;
				} else {
					orr = i;
				}
			}
		}
		return orr;
	} else {
		return THINGY_ORIENT_NONE;
	}
}

void thingy52_orientation_handler_init(void)
{
	sensor = device_get_binding(DT_LABEL(DT_INST(0, st_lis2dh12)));

	if (sensor == NULL) {
		printk("Could not get %s device\n",
		       DT_LABEL(DT_INST(0, st_lis2dh12)));
	}
}
