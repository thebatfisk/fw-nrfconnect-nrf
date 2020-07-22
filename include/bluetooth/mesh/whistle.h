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

/** @cond INTERNAL_HIDDEN */
#define BT_MESH_MODEL_ID_WHISTLE_SRV 0x0005
#define BT_MESH_MODEL_ID_WHISTLE_CLI 0x0006

#define BT_MESH_WHISTLE_OP_LVL_SET BT_MESH_MODEL_OP_2(0x82, 0xFE)
#define BT_MESH_WHISTLE_OP_LVL_RELAY BT_MESH_MODEL_OP_2(0x82, 0xFF)

#define BT_MESH_WHISTLE_MSG_LEN_SET 2

/** @endcond */

#ifdef __cplusplus
}
#endif

#endif /* BT_MESH_WHISTLE_H__ */
