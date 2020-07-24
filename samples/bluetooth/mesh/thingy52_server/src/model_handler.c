/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <drivers/pwm.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

static struct device *io_expander;
static struct device *gpio_0;

static struct device *spkr_pwm;
static u32_t pwm_frequency = 880;

static void led_init(void)
{
	int err;

	io_expander = device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));

	if (io_expander == NULL) {
		printk("Could not initiate I/O expander\n");
	}

	err = gpio_pin_configure(io_expander, GREEN_LED, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure green LED pin\n");
	}

	err = gpio_pin_configure(io_expander, BLUE_LED, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure blue LED pin\n");
	}

	err = gpio_pin_configure(io_expander, RED_LED, GPIO_OUTPUT);

	if (err) {
		printk("Could not configure red LED pin\n");
	}
}

static void speaker_init(void)
{
	int err;
	u32_t pwm_period = 1000000U / pwm_frequency;

	gpio_0 = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));

	if (gpio_0 == NULL) {
		printk("Could not initiate GPIO 0\n");
		return;
	}

	err = gpio_pin_configure(gpio_0, 29, GPIO_OUTPUT | GPIO_PULL_UP);

	if (err) {
		printk("Could not configure speaker power pin\n");
		return;
	} else {
		printk("Speaker power pin configured\n");
	}

	spkr_pwm = device_get_binding(DT_PROP(DT_NODELABEL(pwm0), label));

	if (spkr_pwm == NULL) {
		printk("Could not initiate speaker PWM\n");
		return;
	} else {
		printk("Speaker PWM (%s) initiated\n", spkr_pwm->name);
	}

	err = pwm_pin_set_usec(spkr_pwm, 27, pwm_period, pwm_period / 2U, 0);

	if (err) {
		printk("Error %d: failed to set PWM values\n", err);
		return;
	}
}

/** Configuration server definition */
static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = IS_ENABLED(CONFIG_BT_MESH_RELAY),
	.beacon = BT_MESH_BEACON_ENABLED,
	.frnd = IS_ENABLED(CONFIG_BT_MESH_FRIEND),
	.gatt_proxy = IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY),
	.default_ttl = 7,

	/* 3 transmissions with 20ms interval */
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(2, 20),
};

static void attention_on(struct bt_mesh_model *mod)
{
	int err;

	err = gpio_pin_set_raw(gpio_0, SPKR_PWR, 1);

	if (err) {
		printk("Could not set speaker power pin\n");
		return;
	} else {
		printk("Speaker power on\n");
	}
}

static void attention_off(struct bt_mesh_model *mod)
{
	int err;

	err = gpio_pin_set_raw(gpio_0, SPKR_PWR, 0);

	if (err) {
		printk("Could not set speaker power pin\n");
		return;
	} else {
		printk("Speaker power off\n");
	}
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV(&cfg_srv),
					BT_MESH_MODEL_HEALTH_SRV(&health_srv,
								 &health_pub)),
		     BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void)
{
	led_init();
	speaker_init();

	return &comp;
}
