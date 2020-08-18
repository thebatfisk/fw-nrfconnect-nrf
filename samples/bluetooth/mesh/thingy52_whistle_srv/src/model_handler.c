/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <drivers/gpio.h>
#include <drivers/gpio/gpio_sx1509b.h>
#include <drivers/pwm.h>
#include <dk_buttons_and_leds.h>
#include <thingy52_orientation_handler.h>
#include "model_handler.h"

#define SPEAKER_PWM_FREQ 880

struct devices {
	struct device *io_expander;
	struct device *gpio_0;
	struct device *spkr_pwm;
};
static struct devices dev;

struct rgb_work {
	struct k_delayed_work work;
	struct bt_mesh_whistle_rgb_msg rgb_msg;
};
static struct rgb_work rgb_work[5];

static struct bt_mesh_whistle_rgb cur_rgb_vals;

static inline int nrfx_err_code_check(nrfx_err_t nrfx_err)
{
	return NRFX_ERROR_BASE_NUM - nrfx_err ? true : false;
}

static int bind_devices(void)
{
	dev.spkr_pwm = device_get_binding(DT_PROP(DT_NODELABEL(pwm0), label));
	dev.gpio_0 = device_get_binding(DT_PROP(DT_NODELABEL(gpio0), label));
	dev.io_expander =
		device_get_binding(DT_PROP(DT_NODELABEL(sx1509b), label));

	if ((dev.gpio_0 == NULL) || (dev.spkr_pwm == NULL) ||
	    (dev.io_expander == NULL)) {
		return -1;
	}

	return 0;
}

static struct k_delayed_work led_fade_work;

static void led_fade(struct k_work *work)
{
	if (!cur_rgb_vals.red && !cur_rgb_vals.green && !cur_rgb_vals.blue) {
		printk("Led fade complete\n");
		return;
	}

	cur_rgb_vals.red *= 0.9;
	cur_rgb_vals.green *= 0.9;
	cur_rgb_vals.blue *= 0.9;

	sx1509b_set_pwm_val(dev.io_expander, RED_LED, cur_rgb_vals.red);
	sx1509b_set_pwm_val(dev.io_expander, GREEN_LED, cur_rgb_vals.green);
	sx1509b_set_pwm_val(dev.io_expander, BLUE_LED, cur_rgb_vals.blue);

	k_delayed_work_submit(&led_fade_work, K_MSEC(20));
}

static void set_rgb_led(struct bt_mesh_whistle_rgb rgb)
{
	sx1509b_set_pwm_val(dev.io_expander, RED_LED, rgb.red);
	sx1509b_set_pwm_val(dev.io_expander, GREEN_LED, rgb.green);
	sx1509b_set_pwm_val(dev.io_expander, BLUE_LED, rgb.blue);

	cur_rgb_vals = rgb;
}

static struct k_delayed_work device_attention_work;

static void device_attention(struct k_work *work)
{
	static uint8_t idx;
	const struct bt_mesh_whistle_rgb colors[2] = {
		{ .red = 0, .green = 0, .blue = 0 },
		{ .red = 255, .green = 255, .blue = 255 },

	};

	set_rgb_led(colors[idx % (sizeof(colors) /
				  sizeof(struct bt_mesh_whistle_rgb))]);
	idx++;
	k_delayed_work_submit(&device_attention_work, K_MSEC(400));
}

static void button_handler_cb(u32_t pressed, u32_t changed)
{
	if (pressed & BIT(0)) {
		orientation_t orr = thingy52_orientation_get();
		printk("Orientation: %d\n", orr);

		switch (orr) {
		case THINGY_ORIENT_X_UP:
			break;
		case THINGY_ORIENT_Y_UP:
			break;
		case THINGY_ORIENT_Y_DOWN:
			break;
		case THINGY_ORIENT_X_DOWN:
			break;
		case THINGY_ORIENT_Z_DOWN:
			break;
		case THINGY_ORIENT_Z_UP:
			break;
		default:
			return;
		}
	}
}

static struct button_handler button_handler = {
	.cb = button_handler_cb,
};

static void button_and_led_init(void)
{
	int err = 0;

	err |= dk_buttons_init(NULL);
	dk_button_handler_add(&button_handler);

	for (int i = GREEN_LED; i <= RED_LED; i++) {
		err |= sx1509b_pwm_pin_configure(dev.io_expander, i);
	}

	k_delayed_work_init(&led_fade_work, led_fade);

	if (err) {
		printk("Initializing buttons and leds failed.\n");
	}
}

static void speaker_init(u32_t pwm_frequency)
{
	int err = 0;
	u32_t pwm_period = 1000000U / pwm_frequency;

	err |= gpio_pin_configure(dev.gpio_0, SPKR_PWR,
				  GPIO_OUTPUT | GPIO_PULL_UP);
	err |= pwm_pin_set_usec(dev.spkr_pwm, SPKR_PWM, pwm_period,
				pwm_period / 2U, 0);

	if (err) {
		printk("Initializing speaker failed.\n");
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
	k_delayed_work_submit(&device_attention_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod)
{
	k_delayed_work_cancel(&device_attention_work);
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

static uint8_t next_rgb_work_idx_get(void)
{
	uint8_t idx = 0;
	uint16_t top_delay = 0xFFFF;

	for (int i = 0; i < ARRAY_SIZE(rgb_work); ++i) {
		int32_t rem = k_delayed_work_remaining_ticks(&rgb_work[i].work);

		if ((rem != 0) && (rgb_work[i].rgb_msg.delay < top_delay)) {
			idx = i;
			top_delay = rgb_work[i].rgb_msg.delay;
		}
	}

	return idx;
}

static void rgb_work_output_set(void)
{
	uint8_t idx = next_rgb_work_idx_get();

	set_rgb_led(rgb_work[idx].rgb_msg.color);

	if (rgb_work[idx].rgb_msg.speaker_on) {
		gpio_pin_set_raw(dev.gpio_0, SPKR_PWR, 1);
	} else {
		gpio_pin_set_raw(dev.gpio_0, SPKR_PWR, 0);
	}
}

void rgb_set_handler(struct bt_mesh_whistle_srv *srv,
		     struct bt_mesh_msg_ctx *ctx,
		     struct bt_mesh_whistle_rgb_msg rgb)
{
	if (rgb.delay == 0xFFFF) {
		set_rgb_led(rgb.color);
		k_delayed_work_submit(&led_fade_work, K_NO_WAIT);

		return;
	}

	for (int i = 0; i < ARRAY_SIZE(rgb_work); ++i) {
		int32_t rem = k_delayed_work_remaining_ticks(&rgb_work[i].work);

		if (rem == 0) {
			memcpy(&rgb_work[i].rgb_msg, &rgb,
			       sizeof(struct bt_mesh_whistle_rgb_msg));
			k_delayed_work_submit(
				&rgb_work[i].work,
				K_MSEC(rgb_work[i].rgb_msg.delay));
			rgb_work_output_set();

			return;
		}
	}
}

struct bt_mesh_whistle_cb handlers = {
	.rgb_set_handler = rgb_set_handler,
};

static struct bt_mesh_whistle_srv whistle_srv =
	BT_MESH_WHISTLE_SRV_INIT(&handlers);

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(1,
		     BT_MESH_MODEL_LIST(BT_MESH_MODEL_CFG_SRV(&cfg_srv),
					BT_MESH_MODEL_HEALTH_SRV(&health_srv,
								 &health_pub)),
		     BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(
		2, BT_MESH_MODEL_NONE,
		BT_MESH_MODEL_LIST(BT_MESH_MODEL_WHISTLE_SRV(&whistle_srv))),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

static void rgb_msg_timeout(struct k_work *work)
{
	struct rgb_work *rgb = CONTAINER_OF(work, struct rgb_work, work.work);
	uint8_t buffer_idx = rgb - &rgb_work[0];

	if (rgb_work[buffer_idx].rgb_msg.ttl) {
		rgb_work[buffer_idx].rgb_msg.ttl--;
		bt_mesh_whistle_srv_rgb_set(&whistle_srv, NULL,
					    &rgb_work[buffer_idx].rgb_msg);
	}

	memset(&rgb_work[buffer_idx].rgb_msg, 0,
	       sizeof(struct bt_mesh_whistle_rgb_msg));
	rgb_work_output_set();
}

const struct bt_mesh_comp *model_handler_init(void)
{
	if (bind_devices()) {
		printk("Failure occured while binding devices\n");
		return &comp;
	}

	k_delayed_work_init(&device_attention_work, device_attention);
	for (int i = 0; i < ARRAY_SIZE(rgb_work); ++i) {
		k_delayed_work_init(&rgb_work[i].work, rgb_msg_timeout);
	}

	button_and_led_init();
	thingy52_orientation_handler_init();
	speaker_init(SPEAKER_PWM_FREQ);

	return &comp;
}
