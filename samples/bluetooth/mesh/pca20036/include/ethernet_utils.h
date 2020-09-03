/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ETHERNET_UTILS_H
#define ETHERNET_UTILS_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

bool arrays_are_equal(uint8_t *p_arr1, uint8_t *p_arr2, uint8_t n, uint8_t m);

bool mac_addresses_are_equal(uint8_t *ip1, uint8_t *ip2);

bool ip_addresses_are_equal(uint8_t *ip1, uint8_t *ip2);

#endif // ETHERNET_UTILS_H
