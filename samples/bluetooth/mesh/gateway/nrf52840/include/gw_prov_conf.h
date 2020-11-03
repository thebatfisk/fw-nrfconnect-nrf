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

#define BASE_GROUP_ADDR 0xc000
#define MAX_UNPROV_DEVICES 30
#define UNPROV_BEAC_TIMEOUT 30

struct unprov_device {
	uint8_t uuid[16];
	uint16_t addr;
	uint16_t time;
};

struct unprov_devices {
	uint8_t number;
	struct unprov_device dev[MAX_UNPROV_DEVICES];
};

struct model_info {
    uint8_t srv_count;
    uint8_t cli_count;
};

struct room_info {
	const char *name;
	uint16_t group_addr;
};

int bt_ready(void);

int provision_device(uint8_t dev_num, const uint8_t *uuid);

int get_model_info(struct model_info *mod_inf);

int configure_server(uint8_t elem_num, uint16_t group_addr);

int configure_client(uint8_t elem_num, uint16_t group_addr);

void blink_device(uint8_t dev_num);

uint8_t get_unprov_dev_num(void);

bool prov_link_active(void);

#ifdef __cplusplus
}
#endif

#endif /* GW_PROVISIONING_H__ */
