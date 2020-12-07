/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "uart_simple.h"
#include "mqtt_serial.h"
#include "prov_conf_common.h"
#include "gw_prov_conf.h"

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <bluetooth/mesh/models.h>
#include <dk_buttons_and_leds.h>

#include <random/rand32.h>
#include <cJSON.h>
#include <cJSON_os.h>

#define ON_STRING "ON"
#define OFF_STRING "OFF"
#define SET_IDENTIFIER "btm/set/"
#define TOPIC_MAX_SIZE 64
#define SERIAL_BUF_SIZE 256
#define HEX_RADIX 16
#define HASS_DISCOVERY_REQUEST 0

void mqtt_rx_callback(struct net_buf *get_buf);
void prov_conf_rx_callback(struct net_buf *get_buf);

struct room_info rooms[] = {
	[0] = { .name = "Living room", .group_addr = BASE_GROUP_ADDR },
	[1] = { .name = "Kitchen", .group_addr = BASE_GROUP_ADDR + 1 },
	[2] = { .name = "Bedroom", .group_addr = BASE_GROUP_ADDR + 2 },
	[3] = { .name = "Bathroom", .group_addr = BASE_GROUP_ADDR + 3 },
	[4] = { .name = "Hallway", .group_addr = BASE_GROUP_ADDR + 4 }
};

struct light_struct {
	bool on_off;
	uint8_t brightness;
};

static struct uart_channel_ctx mqtt_serial_chan = {
	.channel_id = 1,
	.rx_cb = mqtt_rx_callback,
};

static struct uart_channel_ctx prov_conf_serial_chan = {
	.channel_id = 2,
	.rx_cb = prov_conf_rx_callback,
};

static int json_add_str(cJSON *parent, const char *str, const char *item)
{
	cJSON *json_str = cJSON_CreateString(item);
	if (json_str == NULL) {
		return -ENOMEM;
	}

	cJSON_AddItemToObject(parent, str, json_str);
	return 0;
}

static int json_add_bool(cJSON *parent, const char *str, bool boolean)
{
	cJSON *json_bool = cJSON_CreateBool(boolean);
	if (json_bool == NULL) {
		return -ENOMEM;
	}

	cJSON_AddItemToObject(parent, str, json_bool);
	return 0;
}

static int json_add_number(cJSON *parent, const char *str, double item)
{
	cJSON *json_num = cJSON_CreateNumber(item);
	if (json_num == NULL) {
		return -ENOMEM;
	}

	cJSON_AddItemToObject(parent, str, json_num);
	return 0;
}

static void mqtt_serial_send(uint8_t *topic, uint16_t topic_len, uint8_t *data,
			     uint16_t data_len)
{
	uint8_t outgoing_buf[SERIAL_BUF_SIZE];
	uint16_t outgoing_msg_len;
	struct mqtt_publish_param param = {
		.message.topic.topic.utf8 = topic,
		.message.topic.topic.size = topic_len,
		.message.payload.data = data,
		.message.payload.len = data_len,
		.message.topic.qos = 0,
		.message_id = sys_rand32_get(),
		.dup_flag = 0,
		.retain_flag = 0,
	};

	int err = mqtt_serial_msg_encode(
		&param, outgoing_buf, sizeof(outgoing_buf), &outgoing_msg_len);

	if (!err) {
		uart_simple_send(&mqtt_serial_chan, outgoing_buf,
				 outgoing_msg_len);
	}
}

static void mqtt_state_reply_send(uint16_t model_id, uint16_t addr,
				  uint8_t *data, uint16_t data_len)
{
	char topic[TOPIC_MAX_SIZE];
	int topic_len = sprintf(topic, "btm/state/%04x%04x", model_id, addr);

	mqtt_serial_send(topic, topic_len, data, data_len);
}

static void discovery_light_create(uint16_t model_id, uint16_t addr, char *name)
{
	char topic[TOPIC_MAX_SIZE], cmd[TOPIC_MAX_SIZE], state[TOPIC_MAX_SIZE];
	int topic_len = sprintf(topic, "homeassistant/light/%s/config", name);

	sprintf(cmd, "btm/set/%04x%04x", model_id, addr);
	sprintf(state, "btm/state/%04x%04x", model_id, addr);

	cJSON *json_struct = cJSON_CreateObject();
	if (json_struct == NULL) {
		printk("No memory to create JSON object\n");
		return;
	}

	json_add_str(json_struct, "name", name);
	json_add_str(json_struct, "unique_id", name);
	json_add_str(json_struct, "cmd_t", cmd);
	json_add_str(json_struct, "stat_t", state);
	json_add_str(json_struct, "schema", "json");
	json_add_bool(json_struct, "brightness", true);

	char *json_packed = cJSON_Print(json_struct);
	if (json_packed == NULL) {
		printk("No memory to print JSON output\n");
		return;
	}

	cJSON_Delete(json_struct);
	mqtt_serial_send(topic, topic_len, json_packed, strlen(json_packed));
	cJSON_FreeString(json_packed);
}

static void discovery_binary_sensor_create(uint16_t model_id, uint16_t addr,
					   char *name)
{
	char topic[TOPIC_MAX_SIZE], state[TOPIC_MAX_SIZE];
	int topic_len =
		sprintf(topic, "homeassistant/binary_sensor/%s/config", name);

	sprintf(state, "btm/state/%04x%04x", model_id, addr);

	cJSON *json_struct = cJSON_CreateObject();
	if (json_struct == NULL) {
		printk("No memory to create JSON object\n");
		return;
	}

	json_add_str(json_struct, "name", name);
	json_add_str(json_struct, "unique_id", name);
	json_add_str(json_struct, "stat_t", state);
	json_add_str(json_struct, "value_template", "{{ value_json.state}}");

	char *json_packed = cJSON_Print(json_struct);
	if (json_packed == NULL) {
		printk("No memory to print JSON output\n");
		return;
	}
	cJSON_Delete(json_struct);

	mqtt_serial_send(topic, topic_len, json_packed, strlen(json_packed));
	cJSON_FreeString(json_packed);
}

void light_packet_decode(uint8_t *data, uint16_t data_len,
			 struct light_struct *struct_out)
{
	struct cJSON *root_obj, *on_off, *brightness;

	root_obj = cJSON_Parse(data);
	if (root_obj == NULL) {
		printk("Could not parse properties object\n");
		return;
	}

	on_off = cJSON_GetObjectItem(root_obj, "state");
	brightness = cJSON_GetObjectItem(root_obj, "brightness");

	if (!strcmp(ON_STRING, on_off->valuestring)) {
		struct_out->on_off = true;
		struct_out->brightness =
			brightness ? brightness->valueint : UINT8_MAX;
	} else {
		struct_out->on_off = false;
		struct_out->brightness = 0;
	}
	cJSON_Delete(root_obj);
}

static void gen_onoff_cli_status_cb(struct bt_mesh_onoff_cli *cli,
				    struct bt_mesh_msg_ctx *ctx,
				    const struct bt_mesh_onoff_status *status)
{
	cJSON *json_struct = cJSON_CreateObject();

	if (json_struct == NULL) {
		printk("No memory to create JSON object\n");
		return;
	}

	json_add_str(json_struct, "state",
		     status->target_on_off ? ON_STRING : OFF_STRING);
	json_add_number(json_struct, "brightness",
			status->target_on_off ? UINT8_MAX : 0);

	char *json_packed = cJSON_Print(json_struct);

	if (json_packed == NULL) {
		printk("No memory to print JSON output\n");
		return;
	}

	mqtt_state_reply_send(BT_MESH_MODEL_ID_GEN_LEVEL_SRV, ctx->addr,
			      json_packed, strlen(json_packed));
	cJSON_Delete(json_struct);
	cJSON_FreeString(json_packed);
}

static void gen_lvl_status_cb(struct bt_mesh_lvl_cli *cli,
			      struct bt_mesh_msg_ctx *ctx,
			      const struct bt_mesh_lvl_status *status)
{
	cJSON *json_struct = cJSON_CreateObject();

	if (json_struct == NULL) {
		printk("No memory to create JSON object\n");
		return;
	}

	json_add_str(json_struct, "state",
		     (status->target != INT16_MIN) ? ON_STRING : OFF_STRING);
	json_add_number(json_struct, "brightness",
			(((UINT16_MAX / 2) + status->target) * UINT8_MAX) /
				UINT16_MAX);

	char *json_packed = cJSON_Print(json_struct);

	if (json_packed == NULL) {
		printk("No memory to print JSON output\n");
		return;
	}

	mqtt_state_reply_send(BT_MESH_MODEL_ID_GEN_LEVEL_SRV, ctx->addr,
			      json_packed, strlen(json_packed));
	cJSON_FreeString(json_packed);
	cJSON_Delete(json_struct);
}

static void gen_onoff_srv_set_cb(struct bt_mesh_onoff_srv *srv,
				 struct bt_mesh_msg_ctx *ctx,
				 const struct bt_mesh_onoff_set *set,
				 struct bt_mesh_onoff_status *rsp)
{
	cJSON *json_struct = cJSON_CreateObject();

	if (json_struct == NULL) {
		printk("No memory to create JSON object\n");
		return;
	}

	json_add_str(json_struct, "state",
		     set->on_off ? ON_STRING : OFF_STRING);

	char *json_packed = cJSON_Print(json_struct);

	if (json_packed == NULL) {
		printk("No memory to print JSON output\n");
		return;
	}

	mqtt_state_reply_send(BT_MESH_MODEL_ID_GEN_ONOFF_CLI, ctx->addr,
			      json_packed, strlen(json_packed));
	cJSON_Delete(json_struct);
	cJSON_FreeString(json_packed);

	rsp->present_on_off = set->on_off;
	rsp->target_on_off = set->on_off;
}

static void gen_onoff_srv_get_cb(struct bt_mesh_onoff_srv *srv,
				 struct bt_mesh_msg_ctx *ctx,
				 struct bt_mesh_onoff_status *rsp)
{
}

struct bt_mesh_onoff_srv_handlers gen_on_off_cb = {
	.set = gen_onoff_srv_set_cb,
	.get = gen_onoff_srv_get_cb,
};

struct bt_mesh_onoff_cli gen_onoff_cli =
	BT_MESH_ONOFF_CLI_INIT(gen_onoff_cli_status_cb);
struct bt_mesh_onoff_srv gen_onoff_srv = BT_MESH_ONOFF_SRV_INIT(&gen_on_off_cb);
struct bt_mesh_lvl_cli gen_lvl_cli = BT_MESH_LVL_CLI_INIT(gen_lvl_status_cb);

static struct bt_mesh_model gateway_models[] = {
	BT_MESH_MODEL_LVL_CLI(&gen_lvl_cli),
	BT_MESH_MODEL_ONOFF_CLI(&gen_onoff_cli),
	BT_MESH_MODEL_ONOFF_SRV(&gen_onoff_srv),
};

static struct bt_mesh_cfg_srv cfg_srv = {
	.relay = BT_MESH_RELAY_ENABLED,
	.beacon = BT_MESH_BEACON_DISABLED,
	.frnd = BT_MESH_FRIEND_NOT_SUPPORTED,
	.default_ttl = 7,

	/* 3 transmissions with 20ms interval */
	.net_transmit = BT_MESH_TRANSMIT(2, 20),
	.relay_retransmit = BT_MESH_TRANSMIT(3, 20),
};

static struct bt_mesh_cfg_cli cfg_cli = {};

static void health_current_status(struct bt_mesh_health_cli *cli, uint16_t addr,
				  uint8_t test_id, uint16_t cid,
				  uint8_t *faults, size_t fault_count)
{
	size_t i;

	printk("Health Current Status from 0x%04x\n", addr);

	if (!fault_count) {
		printk("Health Test ID 0x%02x Company ID 0x%04x: no faults\n",
		       test_id, cid);
		return;
	}

	printk("Health Test ID 0x%02x Company ID 0x%04x Fault Count %zu:\n",
	       test_id, cid, fault_count);

	for (i = 0; i < fault_count; i++) {
		printk("\t0x%02x\n", faults[i]);
	}
}

static struct bt_mesh_health_cli health_cli = {
	.current_status = health_current_status,
};

static struct bt_mesh_model root_models[] = {
	BT_MESH_MODEL_CFG_SRV(&cfg_srv),
	BT_MESH_MODEL_CFG_CLI(&cfg_cli),
	BT_MESH_MODEL_HEALTH_CLI(&health_cli),
};

static struct bt_mesh_elem elements[] = {
	BT_MESH_ELEM(0, root_models, BT_MESH_MODEL_NONE),
	BT_MESH_ELEM(1, gateway_models, BT_MESH_MODEL_NONE),
};

static const struct bt_mesh_comp comp = {
	.cid = CONFIG_BT_COMPANY_ID,
	.elem = elements,
	.elem_count = ARRAY_SIZE(elements),
};

static void add_element_to_hass(uint16_t model_id, uint16_t addr, uint16_t group_addr, uint16_t room_num, const char *name)
{
	char merge_string[15];
	char name_string[30];

	strcpy(name_string, name);

	if (model_id == BT_MESH_MODEL_ID_GEN_ONOFF_SRV) {
		if (!rooms[room_num].light_group_added_to_hass) {
			discovery_light_create(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, group_addr, name);
			rooms[room_num].light_group_added_to_hass = true;
		}

		sprintf(merge_string, " light %d", rooms[room_num].light_count);
		rooms[room_num].light_count++;
		strcat(name_string, merge_string);
		discovery_light_create(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, addr, name_string);
	} else if (model_id == BT_MESH_MODEL_ID_GEN_ONOFF_CLI) {
		if (!rooms[room_num].switch_group_added_to_hass) {
			discovery_binary_sensor_create(BT_MESH_MODEL_ID_GEN_ONOFF_CLI, group_addr, name);
			rooms[room_num].switch_group_added_to_hass = true;
		}

		sprintf(merge_string, " switch %d", rooms[room_num].switch_count);
		rooms[room_num].switch_count++;
		strcat(name_string, merge_string);
		discovery_binary_sensor_create(BT_MESH_MODEL_ID_GEN_ONOFF_CLI, addr, name_string);
	}
}

void mqtt_msg_handler(uint16_t model_id, uint16_t addr, uint8_t *data,
		      uint16_t data_len)
{
	switch (model_id) {
	case HASS_DISCOVERY_REQUEST:
		/* Dummy discovery */
		discovery_light_create(0x1002, 0x0004, "McWong");
		discovery_light_create(0x1002, 0x0007, "McWong2");
		discovery_light_create(0x1002, 0x000e, "McWong3");
		discovery_binary_sensor_create(0x1001, 0x0009, "nrfSwitch1");
		break;

	case BT_MESH_MODEL_ID_GEN_ONOFF_SRV: {
		struct light_struct recived;
		light_packet_decode(data, data_len, &recived);

		struct bt_mesh_onoff_set set_msg = {
			.on_off = recived.on_off,
			.transition = NULL,
		};

		struct bt_mesh_msg_ctx ctx = {
			.app_idx = gen_lvl_cli.pub.key,
			.addr = addr,
			.send_rel = gen_lvl_cli.pub.send_rel,
			.send_ttl = gen_lvl_cli.pub.ttl,
		};

		bt_mesh_onoff_cli_set(&gen_onoff_cli, &ctx, &set_msg, NULL);
		break;
	}

	case BT_MESH_MODEL_ID_GEN_LEVEL_SRV: {
		struct light_struct recived;
		light_packet_decode(data, data_len, &recived);

		struct bt_mesh_lvl_set set_msg = {
			.new_transaction = true,
			.transition = NULL,
			.lvl = recived.on_off ? (((int8_t)(recived.brightness -
							   INT8_MAX) *
						  INT16_MAX) /
						 INT8_MAX) :
						INT16_MIN,
		};

		struct bt_mesh_msg_ctx ctx = {
			.app_idx = gen_lvl_cli.pub.key,
			.addr = addr,
			.send_rel = gen_lvl_cli.pub.send_rel,
			.send_ttl = gen_lvl_cli.pub.ttl,
		};

		bt_mesh_lvl_cli_set(&gen_lvl_cli, &ctx, &set_msg, NULL);
		break;
	}
	default:
		break;
	}
}

void mqtt_rx_callback(struct net_buf *get_buf)
{
	char model_id_str[] = "0000";
	char addr_str[] = "0000";

	struct mqtt_publish_param param;
	mqtt_serial_msg_decode(get_buf, &param);

	if (strncmp(SET_IDENTIFIER, param.message.topic.topic.utf8,
		    strlen(SET_IDENTIFIER))) {
		/* Not a set message */
		return;
	}
	strncpy(model_id_str,
		param.message.topic.topic.utf8 + strlen(SET_IDENTIFIER),
		strlen(model_id_str));
	strncpy(addr_str,
		param.message.topic.topic.utf8 + strlen(SET_IDENTIFIER) +
			strlen(model_id_str),
		strlen(addr_str));

	uint16_t model_id = strtol(model_id_str, NULL, HEX_RADIX);
	uint16_t addr = strtol(addr_str, NULL, HEX_RADIX);

	mqtt_msg_handler(model_id, addr, param.message.payload.data,
			 param.message.payload.len);
}

void prov_conf_rx_callback(struct net_buf *get_buf)
{
	NET_BUF_SIMPLE_DEFINE(serial_net_buf, 20);
	uint8_t serial_message[20];
	uint8_t opcode = net_buf_pull_u8(get_buf);
	static struct model_info mod_inf;
	int err;

	printk("Serial opcode: %d\n", opcode);

	if (opcode == oc_prov_link_active) {
		serial_message[0] = oc_prov_link_active;
		serial_message[1] = (uint8_t)prov_link_active();
		uart_simple_send(&prov_conf_serial_chan, serial_message, 2);
	} else if (opcode == oc_unprov_dev_num) {
		serial_message[0] = oc_unprov_dev_num;
		serial_message[1] = get_unprov_dev_num();
		uart_simple_send(&prov_conf_serial_chan, serial_message, 2);
	} else if (opcode == oc_get_model_info) {
		mod_inf.srv_count = 0;
		mod_inf.cli_count = 0;
		get_model_info(&mod_inf);

		printk("Server count: %d - Client cout: %d\n", mod_inf.srv_count, mod_inf.cli_count);

		net_buf_simple_reset(&serial_net_buf);
		net_buf_simple_add_u8(&serial_net_buf, oc_get_model_info);
		net_buf_simple_add_le16(&serial_net_buf, mod_inf.srv_count);
		net_buf_simple_add_le16(&serial_net_buf, mod_inf.cli_count);
		uart_simple_send(&prov_conf_serial_chan, serial_net_buf.data, serial_net_buf.len);
	} else if (opcode == oc_blink_device) {
		uint8_t device = net_buf_pull_u8(get_buf);
		blink_device(device);
	} else if (opcode == oc_provision_device) {
		uint8_t dev_num = net_buf_pull_u8(get_buf);

		if (dev_num == 0xff) {
			uint8_t uuid[16];
			memcpy(uuid, get_buf->data, 16);
			provision_device(dev_num, uuid);
		} else {
			provision_device(dev_num, NULL);
		}

		serial_message[0] = oc_device_added;
		uart_simple_send(&prov_conf_serial_chan, serial_message, 1);
	} else if (opcode == oc_configure_server) {
		uint8_t elem_num = net_buf_pull_u8(get_buf);
		uint16_t group_addr = net_buf_pull_be16(get_buf);
		printk("Server group address: 0x%04x\n", group_addr);
		err = configure_server(elem_num, group_addr);

		if (err) {
			printk("Error configuring server\n");
		} else {
			for (int i = 0; i < 5; i++) {
				if (rooms[i].group_addr == group_addr) {
					add_element_to_hass(BT_MESH_MODEL_ID_GEN_ONOFF_SRV, (get_added_node_addr() + elem_num), group_addr, i, rooms[i].name);
					break;
				}
			}
		}
	} else if (opcode == oc_configure_client) {
		uint8_t elem_num = net_buf_pull_u8(get_buf);
		uint16_t group_addr = net_buf_pull_be16(get_buf);
		printk("Client group address: 0x%04x\n", group_addr);
		err = configure_client(elem_num, group_addr);

		if (err) {
			printk("Error configuring client\n");
		} else {
			for (int i = 0; i < 5; i++) {
				if (rooms[i].group_addr == group_addr) {
					add_element_to_hass(BT_MESH_MODEL_ID_GEN_ONOFF_CLI, (get_added_node_addr() + elem_num), group_addr, i, rooms[i].name);
					break;
				}
			}
		}
	}
}

void main(void)
{
	int err;

	printk("MQTT bridge nrf52840 started\n");

	cJSON_Init();
	uart_simple_init(NULL);
	uart_simple_channel_create(&mqtt_serial_chan);
	uart_simple_channel_create(&prov_conf_serial_chan);

	set_comp_data(comp);

	err = bt_enable(bt_ready);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
	}
	discovery_light_create(1, 2, "anders");
	discovery_light_create(1, 2, "anders");
	discovery_light_create(1, 2, "anders");
	discovery_light_create(1, 2, "anders");
}
