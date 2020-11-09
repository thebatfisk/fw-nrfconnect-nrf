/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef MESH_NFC_H__
#define MESH_NFC_H__

#include <bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

int mesh_nfc_init(const uint8_t *uuid);

#ifdef __cplusplus
}
#endif

#endif /* MESH_NFC_H__ */
