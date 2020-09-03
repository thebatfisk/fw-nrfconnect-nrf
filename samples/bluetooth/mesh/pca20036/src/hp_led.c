/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <drivers/gpio.h>
#include "hp_led.h"

static struct device *gpio_0;

void hp_led_on(void)
{
	gpio_pin_set_raw(gpio_0, HP_LED_PIN, 1);
}

void hp_led_off(void)
{
	gpio_pin_set_raw(gpio_0, HP_LED_PIN, 0);
}

int hp_led_init(void)
{
	int err;

	gpio_0 = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));

	if (gpio_0 == NULL) {
		return -ENODEV;
	}

	err = gpio_pin_configure(gpio_0, HP_LED_PIN, GPIO_OUTPUT);

	return err;
}
