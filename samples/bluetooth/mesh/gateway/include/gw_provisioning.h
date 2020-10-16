/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef GW_PROVISIONING_H__
#define GW_PROVISIONING_H__

#include <bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

int bt_ready(void);

int provision_devce_uuid(const uint8_t *uuid);

int provision_device(uint8_t prov_beac_timeout);

int configure_device(void);

void testing(void);

#ifdef __cplusplus
}
#endif

#endif /* GW_PROVISIONING_H__ */
