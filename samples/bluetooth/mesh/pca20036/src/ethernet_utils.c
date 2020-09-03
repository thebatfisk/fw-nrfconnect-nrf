/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include "ethernet_utils.h"

/* Returns true if arr1[0..n-1] and arr2[0..m-1] contain same elements */
bool arrays_are_equal(uint8_t *p_arr1, uint8_t *p_arr2, uint8_t n, uint8_t m)
{
	if (n != m)
		return false;

	for (uint8_t i = 0; i < n; i++)
		if (p_arr1[i] != p_arr2[i])
			return false;

	return true;
}

bool mac_addresses_are_equal(uint8_t *ip1, uint8_t *ip2)
{
	return arrays_are_equal(ip1, ip2, 6, 6);
}

bool ip_addresses_are_equal(uint8_t *ip1, uint8_t *ip2)
{
	return arrays_are_equal(ip1, ip2, 4, 4);
}
