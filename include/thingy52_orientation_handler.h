/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef THINGY52_ORIENT_HANDLER_H__
#define THINGY52_ORIENT_HANDLER_H__

#include <zephyr/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Orientation states.*/
typedef enum {
    THINGY_ORIENT_X_UP=0,
    THINGY_ORIENT_Y_UP,
    THINGY_ORIENT_Z_UP,
    THINGY_ORIENT_X_DOWN,
    THINGY_ORIENT_Y_DOWN,
    THINGY_ORIENT_Z_DOWN,
    THINGY_ORIENT_NONE,
} orientation_t;

/**
 * @brief Get the current orientation of the accelerometer on the thingy52.
 *
 * @return The current orientation.
 */
orientation_t thingy52_orientation_get(void);

/**
 * @brief Initialize the Thingy52 accelerometer orientation handler.
 */
void thingy52_orientation_handler_init(void);

#ifdef __cplusplus
}
#endif


#endif /* THINGY52_ORIENT_HANDLER_H__ */
