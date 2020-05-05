/*
 * Copyright (c) 2019 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>
#include "model_handler.h"

// extern struct bt_mesh_sensor temperature_sensor;
// extern struct bt_mesh_sensor motion_sensor;
// extern struct bt_mesh_sensor light_sensor;

static struct device *dev;

static int temp_get(struct bt_mesh_sensor *sensor,
                    struct bt_mesh_msg_ctx *ctx,
                    struct sensor_value *rsp)
{
    sensor_sample_fetch(dev);
    return sensor_channel_get(dev, SENSOR_CHAN_DIE_TEMP, rsp);
}

struct bt_mesh_sensor temperature_sensor = {
    .type = &bt_mesh_sensor_present_dev_op_temp,
    .get = temp_get,
};

static struct bt_mesh_sensor* const sensors[] = {
    &temperature_sensor,
    // &motion_sensor,
    // &light_sensor,
};

static struct bt_mesh_sensor_srv sensor_srv = BT_MESH_SENSOR_SRV_INIT(sensors, ARRAY_SIZE(sensors));

/** Configuration server definition */
static struct bt_mesh_cfg_srv cfg_srv = { // *** Configuration server definition
	.relay = IS_ENABLED(CONFIG_BT_MESH_RELAY),
	.beacon = BT_MESH_BEACON_ENABLED,
	.frnd = IS_ENABLED(CONFIG_BT_MESH_FRIEND),
	.gatt_proxy = IS_ENABLED(CONFIG_BT_MESH_GATT_PROXY),
	.default_ttl = 7,

	/* 3 transmissions with 20ms interval */
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(2, 20),
};

/* Set up a repeating delayed work to blink the DK's LEDs when attention is
 * requested.
 */
static struct k_delayed_work attention_blink_work; // *** The delayed work context?

static void attention_blink(struct k_work *work) // *** The attention blink *handler* function used by the kernel delayed work
{
	static int idx;
	const u8_t pattern[] = {
		BIT(0) | BIT(1),
		BIT(1) | BIT(2),
		BIT(2) | BIT(3),
		BIT(3) | BIT(0),
	};
	dk_set_leds(pattern[idx++ % ARRAY_SIZE(pattern)]);
	k_delayed_work_submit(&attention_blink_work, K_MSEC(30));
}

static void attention_on(struct bt_mesh_model *mod) // *** Attention blinking *used by health server*
{
	k_delayed_work_submit(&attention_blink_work, K_NO_WAIT);
}

static void attention_off(struct bt_mesh_model *mod) // *** Attention blinking *used by health server*
{
	k_delayed_work_cancel(&attention_blink_work);
	dk_set_leds(DK_NO_LEDS_MSK);
}

static const struct bt_mesh_health_srv_cb health_srv_cb = { // *** Healt server callback - defined in health server model context
	.attn_on = attention_on,
	.attn_off = attention_off,
};

static struct bt_mesh_health_srv health_srv = { // *** Health server model context
	.cb = &health_srv_cb,
};

BT_MESH_HEALTH_PUB_DEFINE(health_pub, 0); // *** Health model boiler plate?

static struct bt_mesh_elem elements[] = { // *** The mesh elements of the node
	BT_MESH_ELEM(
		1, BT_MESH_MODEL_LIST(
			BT_MESH_MODEL_CFG_SRV(&cfg_srv),
			BT_MESH_MODEL_HEALTH_SRV(&health_srv, &health_pub),
			BT_MESH_MODEL_SENSOR_SRV(&sensor_srv)),
		BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = { // *** The composition data used by bt_mesh_init()
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

const struct bt_mesh_comp *model_handler_init(void) // *** Returns the composition data to bt_mesh_init() after initiating the attention blink delayed work
{
	k_delayed_work_init(&attention_blink_work, attention_blink);

	dev = device_get_binding(DT_INST_0_NORDIC_NRF_TEMP_LABEL);

	if (dev == NULL) {
		printk("Could not get device\n");
		return;
	}

	printk("*** Sensor reading test - Device: %s ***\n", dev->config->name);

	return &comp;
}
