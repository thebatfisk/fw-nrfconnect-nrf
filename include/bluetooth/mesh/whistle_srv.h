/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef BT_MESH_WHISTLE_SRV_H__
#define BT_MESH_WHISTLE_SRV_H__

#include <bluetooth/mesh/whistle.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_whistle_srv;

/** @def BT_MESH_WHISTLE_SRV_INIT
 *
 * @brief Init parameters for a @ref bt_mesh_whistle_srv instance.
 *
 * @param[in] _cb_handlers Level handler to use in the model instance.
 */
#define BT_MESH_WHISTLE_SRV_INIT(_cb_handlers)                                 \
	{                                                                      \
		.msg_callbacks = _cb_handlers,                                 \
		.pub = {                                                       \
			.msg = NET_BUF_SIMPLE(BT_MESH_MODEL_BUF_LEN(           \
				BT_MESH_ONOFF_OP_STATUS,                       \
				BT_MESH_ONOFF_MSG_MAXLEN_STATUS)),             \
		},                                                             \
	}

/** @def BT_MESH_MODEL_WHISTLE_SRV
 *
 * @brief Whistle Server model composition data entry.
 *
 * @param[in] _srv Pointer to a @ref bt_mesh_whistle_srv instance.
 */
#define BT_MESH_MODEL_WHISTLE_SRV(_srv)                                        \
	BT_MESH_MODEL_CB(BT_MESH_MODEL_ID_GEN_ONOFF_SRV,                       \
			 _bt_mesh_whistle_srv_op, &(_srv)->pub,                \
			 BT_MESH_MODEL_USER_DATA(struct bt_mesh_whistle_srv,   \
						 _srv),                        \
			 &_bt_mesh_whistle_srv_cb)

struct bt_mesh_whistle_cb {
	/** Attention message handler. */
	void (*const attention_set_handler)(struct bt_mesh_whistle_srv *srv,
					    struct bt_mesh_msg_ctx *ctx,
					    bool onoff);

	/** Level message handler. */
	void (*const lvl_set_handler)(struct bt_mesh_whistle_srv *srv,
				      struct bt_mesh_msg_ctx *ctx,
				      uint16_t lvl);

	/** RGB message handler. */
	void (*const rgb_set_handler)(struct bt_mesh_whistle_srv *srv,
				      struct bt_mesh_msg_ctx *ctx,
				      struct bt_mesh_whistle_rgb_msg rgb);
};

/**
 * Whistle Server instance. Should primarily be initialized with the
 * @ref BT_MESH_WHISTLE_SRV_INIT macro.
 */
struct bt_mesh_whistle_srv {
	/** Message callbacks */
	struct bt_mesh_whistle_cb *msg_callbacks;

	/** Attention state */
	bool attention_state;

	/** Access model pointer. */
	struct bt_mesh_model *model;
	/** Publish parameters. */
	struct bt_mesh_model_pub pub;
};

/** @cond INTERNAL_HIDDEN */
extern const struct bt_mesh_model_op _bt_mesh_whistle_srv_op[];
extern const struct bt_mesh_model_cb _bt_mesh_whistle_srv_cb;
/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_WHISTLE_SRV_H__ */
