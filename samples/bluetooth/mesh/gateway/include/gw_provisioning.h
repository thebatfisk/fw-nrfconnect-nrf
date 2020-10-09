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

int provision_device(uint8_t dev_num);

int configure_device(void);

void blink_device(uint8_t dev_num);

uint8_t get_unprov_dev_num(void);

bool ready_to_blink(void);

#ifdef __cplusplus
}
#endif

#endif /* GW_PROVISIONING_H__ */
