/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef BT_MESH_WHISTLE_H__
#define BT_MESH_WHISTLE_H__

#include <bluetooth/mesh/model_types.h>

#ifdef __cplusplus
extern "C" {
#endif

struct bt_mesh_whistle_rgb_msg {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

/** @cond INTERNAL_HIDDEN */

#define BT_MESH_NORDIC_SEMI_COMPANY_ID 0x0059
#define BT_MESH_MODEL_ID_WHISTLE_SRV 0x0005
#define BT_MESH_MODEL_ID_WHISTLE_CLI 0x0006

#define BT_MESH_WHISTLE_OP_ATTENTION_SET                                       \
	BT_MESH_MODEL_OP_3(0x82, BT_MESH_NORDIC_SEMI_COMPANY_ID)
#define BT_MESH_WHISTLE_OP_RGB_SET                                             \
	BT_MESH_MODEL_OP_3(0x83, BT_MESH_NORDIC_SEMI_COMPANY_ID)
#define BT_MESH_WHISTLE_OP_LVL_SET                                             \
	BT_MESH_MODEL_OP_3(0x84, BT_MESH_NORDIC_SEMI_COMPANY_ID)
#define BT_MESH_WHISTLE_OP_LVL_RELAY                                           \
	BT_MESH_MODEL_OP_3(0x85, BT_MESH_NORDIC_SEMI_COMPANY_ID)

#define BT_MESH_WHISTLE_MSG_LEN_ATTENTION_SET 1
#define BT_MESH_WHISTLE_MSG_LEN_RGB_SET 3
#define BT_MESH_WHISTLE_MSG_LEN_SET 2

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_WHISTLE_H__ */
