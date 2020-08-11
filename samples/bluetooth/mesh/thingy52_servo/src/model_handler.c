/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"
#include <drivers/pwm.h>

#define PWM_PERIOD 20000
#define SERVO_PIN 2
#define SERVO_ARM_MIN (PWM_PERIOD - 470)
#define SERVO_ARM_MID (PWM_PERIOD - 1180)
#define SERVO_ARM_MAX (PWM_PERIOD - 2200)

struct device *servo_pwm;
struct k_delayed_work servo_work;

static void servo_work_handler(struct k_work *work)
{
	pwm_pin_set_usec(servo_pwm, 2, PWM_PERIOD, PWM_PERIOD - 1180, 0);
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
	dk_set_leds(DK_ALL_LEDS_MSK);
}

static void attention_off(struct bt_mesh_model *mod)
{
	dk_set_leds(DK_NO_LEDS_MSK);
}

static const struct bt_mesh_health_srv_cb health_srv_cb = {
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = {
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0);

void rgb_set_handler(struct bt_mesh_whistle_srv *srv,
		     struct bt_mesh_msg_ctx *ctx,
		     struct bt_mesh_whistle_rgb_msg rgb)
{
	pwm_pin_set_usec(servo_pwm, 2, PWM_PERIOD, PWM_PERIOD - 470, 0);
	k_delayed_work_submit(&servo_work, K_MSEC(1000));
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

const struct bt_mesh_comp *model_handler_init(void)
{
	servo_pwm = device_get_binding(DT_PROP(DT_NODELABEL(pwm0), label));
	k_delayed_work_init(&servo_work, servo_work_handler);
	return &comp;
}
