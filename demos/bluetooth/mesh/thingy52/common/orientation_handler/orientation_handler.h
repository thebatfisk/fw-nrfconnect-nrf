/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

/** @file
 *  @defgroup bt_mesh_thingy52_orientation_handler Thingy52 Orientation Handler
 *  @{
 *  @brief API for the Thingy52 Thingy52 Orientation Handler.
 */

#ifndef ORIENT_HANDLER_H__
#define ORIENT_HANDLER_H__

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Orientation states */
typedef enum {
	THINGY_ORIENT_X_UP = 0,
	THINGY_ORIENT_Y_UP,
	THINGY_ORIENT_Z_UP,
	THINGY_ORIENT_X_DOWN,
	THINGY_ORIENT_Y_DOWN,
	THINGY_ORIENT_Z_DOWN,
	THINGY_ORIENT_NONE,
} orientation_t;

/** @brief Get current orientation of the Thingy:52 device.
 *
 * @param[in] dev Pointer to the device structure of low power accelerometer;
 *
 * @returns Current orientation of the Thingy:52 device.
 */
orientation_t orientation_get(struct device *dev);

#ifdef __cplusplus
}
#endif

/** @} */

#endif /* ORIENT_HANDLER_H__ */
