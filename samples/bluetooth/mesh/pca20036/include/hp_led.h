/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef HP_LED_H
#define HP_LED_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#define HP_LED_PIN 13

void hp_led_on(void);

void hp_led_off(void);

bool hp_led_get_state(void);

int hp_led_init(void);

#endif // HP_LED_H
