/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef GW_NFC_H__
#define GW_NFC_H__

#include <bluetooth/bluetooth.h>

#ifdef __cplusplus
extern "C" {
#endif

struct gw_nfc_rx_data {
    const uint8_t *value;
    uint16_t length;
};

struct gw_nfc_cb {
	void (*rx)(struct gw_nfc_rx_data);
};

int gw_nfc_init(void);

void gw_nfc_register_cb(struct gw_nfc_cb *cb);

int gw_nfc_start(void);

int gw_nfc_stop(void);

#ifdef __cplusplus
}
#endif

#endif /* GW_NFC_H__ */
