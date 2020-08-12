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

struct bt_mesh_whistle_rgb {
	uint8_t red;
	uint8_t green;
	uint8_t blue;
};

struct bt_mesh_whistle_rgb_msg {
	uint16_t ttl;
	uint16_t delay;
	struct bt_mesh_whistle_rgb color;
	bool speaker_on;
};

/** @cond INTERNAL_HIDDEN */

#define BT_MESH_NORDIC_SEMI_COMPANY_ID 0x0059
#define BT_MESH_MODEL_ID_WHISTLE_SRV 0x0005
#define BT_MESH_MODEL_ID_WHISTLE_CLI 0x0006

#define BT_MESH_WHISTLE_OP_RGB_SET                                             \
	BT_MESH_MODEL_OP_3(0x83, BT_MESH_NORDIC_SEMI_COMPANY_ID)

#define BT_MESH_WHISTLE_MSG_LEN_RGB_SET 7

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_WHISTLE_H__ */
