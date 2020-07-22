/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/mesh/whistle_cli.h>
#include "model_utils.h"

static int bt_mesh_whistle_cli_init(struct bt_mesh_model *model)
{
	struct bt_mesh_whistle_cli *cli = model->user_data;

	cli->model = model;
	net_buf_simple_init(cli->pub.msg, 0);
	return 0;
}

const struct bt_mesh_model_cb _bt_mesh_whistle_cli_cb = {
	.init = bt_mesh_whistle_cli_init,
};

int bt_mesh_whistle_cli_lvl_set(struct bt_mesh_whistle_cli *cli,
				struct bt_mesh_msg_ctx *ctx, uint16_t lvl)
{
	BT_MESH_MODEL_BUF_DEFINE(msg, BT_MESH_WHISTLE_OP_LVL_SET,
				 BT_MESH_WHISTLE_MSG_LEN_SET);
	bt_mesh_model_msg_init(&msg, BT_MESH_WHISTLE_OP_LVL_SET);

	net_buf_simple_add_le16(&msg, lvl);
	return model_send(cli->model, ctx, &msg);
}
