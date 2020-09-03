/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ETHERNET_DFU_H
#define ETHERNET_DFU_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

void ethernet_dfu_start(uint8_t *server_ip);

void ethernet_dfu_confirm_image(void);

void ethernet_dfu_init(void);

#endif // ETHERNET_DFU_H
