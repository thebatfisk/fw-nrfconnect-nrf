/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <stdlib.h>
#include <bluetooth/mesh/whistle_srv.h>
#include "model_utils.h"

#ifdef CONFIG_BT_MESH_WHISTLE_SRV_RELAY
static int send_relay(struct bt_mesh_whistle_srv *srv, uint16_t lvl)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_WHISTLE_OP_LVL_RELAY,
				 BT_MESH_WHISTLE_MSG_LEN_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_WHISTLE_OP_LVL_RELAY);

	net_buf_simple_add_le16(&msg, lvl);
	return model_send(srv->model, NULL, &msg);
}
#endif
static void handle_lvl_set(struct bt_mesh_model *model,
			   struct bt_mesh_msg_ctx *ctx,
			   struct net_buf_simple *buf)
{
	struct bt_mesh_whistle_srv *srv = model->user_data;
	uint16_t lvl = net_buf_simple_pull_le16(buf);

	srv->msg_callbacks->lvl_set_handler(srv, ctx, lvl);
#ifdef CONFIG_BT_MESH_WHISTLE_SRV_RELAY
	if ((CONFIG_BT_MESH_WHISTLE_SRV_RELAY_SCALE_FACTOR < 0) &&
	    (abs(CONFIG_BT_MESH_WHISTLE_SRV_RELAY_SCALE_FACTOR) >= lvl)) {
		lvl = 0;
	} else if ((CONFIG_BT_MESH_WHISTLE_SRV_RELAY_SCALE_FACTOR + lvl) >
		   UINT16_MAX) {
		lvl = UINT16_MAX;
	} else {
		lvl += CONFIG_BT_MESH_WHISTLE_SRV_RELAY_SCALE_FACTOR;
	}
	send_relay(srv, lvl);
#endif
}

static void handle_rgb_set(struct bt_mesh_model *model,
			   struct bt_mesh_msg_ctx *ctx,
			   struct net_buf_simple *buf)
{
	struct bt_mesh_whistle_srv *srv = model->user_data;
        struct bt_mesh_whistle_rgb_msg rgb;

        rgb.red = net_buf_simple_pull_u8(buf);
        rgb.green = net_buf_simple_pull_u8(buf);
        rgb.blue = net_buf_simple_pull_u8(buf);

	srv->msg_callbacks->rgb_set_handler(srv, ctx, rgb);
}

static void handle_attention_set(struct bt_mesh_model *model,
			   struct bt_mesh_msg_ctx *ctx,
			   struct net_buf_simple *buf)
{
	struct bt_mesh_whistle_srv *srv = model->user_data;

        srv->attention_state = net_buf_simple_pull_u8(buf);

        srv->msg_callbacks->attention_set_handler(srv, ctx, srv->attention_state);

}

const struct bt_mesh_model_op _bt_mesh_whistle_srv_op[] = {
	{ BT_MESH_WHISTLE_OP_ATTENTION_SET, BT_MESH_WHISTLE_MSG_LEN_ATTENTION_SET,
	  handle_attention_set },
	{ BT_MESH_WHISTLE_OP_RGB_SET, BT_MESH_WHISTLE_MSG_LEN_RGB_SET,
	  handle_rgb_set },
	{ BT_MESH_WHISTLE_OP_LVL_SET, BT_MESH_WHISTLE_MSG_LEN_SET,
	  handle_lvl_set },
	{ BT_MESH_WHISTLE_OP_LVL_RELAY, BT_MESH_WHISTLE_MSG_LEN_SET,
	  handle_lvl_set },
	BT_MESH_MODEL_OP_END,
};

static int bt_mesh_whistle_srv_init(struct bt_mesh_model *model)
{
	struct bt_mesh_whistle_srv *srv = model->user_data;

	srv->model = model;
	net_buf_simple_init(model->pub->msg, 0);
	return 0;
}

const struct bt_mesh_model_cb _bt_mesh_whistle_srv_cb = {
	.init = bt_mesh_whistle_srv_init
};
