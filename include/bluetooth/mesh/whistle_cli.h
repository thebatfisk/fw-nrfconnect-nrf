/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef BT_MESH_WHISTLE_CLI_H__
#define BT_MESH_WHISTLE_CLI_H__

#include <bluetooth/mesh/whistle.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_whistle_cli;

/** @def BT_MESH_WHISTLE_CLI_INIT
 *
 * @brief Initialization parameters for a @ref bt_mesh_whistle_cli instance.
 */
#define BT_MESH_WHISTLE_CLI_INIT                                               \
	{                                                                      \
		.pub = {.msg = NET_BUF_SIMPLE(BT_MESH_MODEL_BUF_LEN(           \
				BT_MESH_WHISTLE_OP_LVL_SET,                    \
				BT_MESH_WHISTLE_MSG_LEN_SET)) }                \
	}

/** @def BT_MESH_MODEL_WHISTLE_CLI
 *
 * @brief Whistle Client model composition data entry.
 *
 * @param[in] _cli Pointer to a @ref bt_mesh_whistle_cli instance.
 */
#define BT_MESH_MODEL_WHISTLE_CLI(_cli)                                        \
	BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_GEN_ONOFF_CLI, NULL, &(_cli)->pub,   \
			 BT_MESH_MODEL_USER_DATA(struct bt_mesh_whistle_cli,   \
						 _cli),                        \
			 &_bt_mesh_whistle_cli_cb)

/**
 * Whistle Client structure.
 *
 * Should be initialized with the @ref BT_MESH_WHISTLE_CLI_INIT macro.
 */
struct bt_mesh_whistle_cli {
	/** Response context for tracking acknowledged messages. */
	/** Publish parameters. */
	struct bt_mesh_model_pub pub;
	/** Access model pointer. */
	struct bt_mesh_model *model;
};

int bt_mesh_whistle_cli_attention_set(struct bt_mesh_whistle_cli *cli,
				      struct bt_mesh_msg_ctx *ctx, bool onoff);

int bt_mesh_whistle_cli_rgb_set(struct bt_mesh_whistle_cli *cli,
				struct bt_mesh_msg_ctx *ctx,
				struct bt_mesh_whistle_rgb_msg *rgb);

int bt_mesh_whistle_cli_lvl_set(struct bt_mesh_whistle_cli *cli,
				struct bt_mesh_msg_ctx *ctx, uint16_t lvl);

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_cb _bt_mesh_whistle_cli_cb;
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_WHISTLE_CLI_H__ */
