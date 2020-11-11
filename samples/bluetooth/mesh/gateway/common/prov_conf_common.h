/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef BT_MESH_PROV_CONF_SERIAL_H__
#define BT_MESH_PROV_CONF_SERIAL_H__

#ifndef BT_MESH_PROV_CONF_BGR
#define BT_MESH_PROV_CONF_BGR
#define BASE_GROUP_ADDR 0xc000
#endif /* BT_MESH_PROV_CONF_BGR */

enum prov_conf_serial_opcodes
{
	oc_prov_link_active = 0,
	oc_unprov_dev_num,
	oc_get_model_info,
    oc_blink_device,
	oc_provision_device,
	oc_device_added,
	oc_configure_server,
	oc_configure_client,
};

struct model_info {
    uint8_t srv_count;
    uint8_t cli_count;
};

struct room_info {
	const char *name;
	uint16_t group_addr;
	bool light_group_added_to_hass;
	bool switch_group_added_to_hass;
	uint16_t light_count;
	uint16_t switch_count;
};

#endif /* BT_MESH_PROV_CONF_SERIAL_H__ */
