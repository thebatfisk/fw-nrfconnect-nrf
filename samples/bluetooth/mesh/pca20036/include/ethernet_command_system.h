/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ETHERNET_COMMAND_SYSTEM_H
#define ETHERNET_COMMAND_SYSTEM_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>

enum ethernet_ctrl_cmd {
	CMD_RESET = 0x01,
	CMD_I_AM_ALIVE = 0x02,
	CMD_DFU = 0x03,
	CMD_LED = 0x04,
};

struct __attribute((packed)) ethernet_reset_package {
	bool is_broadcast;
	uint8_t target_mac[6];
};

struct __attribute((packed)) ethernet_i_am_alive_package {
	uint8_t ip[4];
	uint8_t mac[6];
	uint8_t version;
};

struct __attribute((packed)) ethernet_dfu_package {
	bool is_broadcast;
	uint8_t target_mac[6];
};

struct __attribute((packed)) ethernet_led_package {
	bool is_broadcast;
	bool on_off;
	uint8_t target_mac[6];
};

struct __attribute((packed)) command_system_package {
	uint32_t identifier;
	uint8_t opcode;
	uint8_t mac[6];
	union {
		struct ethernet_reset_package reset_package;
		struct ethernet_i_am_alive_package i_am_alive_package;
		struct ethernet_dfu_package dfu_package;
		struct ethernet_led_package led_package;
	} payload;
};

void ethernet_command_system_rx(void);

void send_over_ethernet(uint8_t *payload_package, uint8_t opcode);

void ethernet_command_system_init(void);

#endif // ETHERNET_COMMAND_SYSTEM_H
