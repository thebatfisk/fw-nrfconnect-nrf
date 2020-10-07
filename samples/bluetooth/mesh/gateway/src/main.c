/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <settings/settings.h>
// #include <bluetooth/mesh/models.h>
// #include <bluetooth/mesh/dk_prov.h>
#include <dk_buttons_and_leds.h>
#include "gw_provisioning.h"
#include "gw_display_shield.h"

K_SEM_DEFINE(sem_system_state, 0, 1);

enum system_states
{
	s_idle = 0,
	s_prov_dev,
	s_conf_dev,
};

static int system_state = s_idle;
static uint8_t curr_but_pressed;

static struct k_delayed_work button_work;

static void button_work_handler(struct k_work *work)
{
	uint8_t button_state;

	button_state = display_read_buttons();

	if ((button_state & BUTTON_UP) && (curr_but_pressed != BUTTON_UP)) {
		printk("BUTTON UP\n");
		curr_but_pressed = BUTTON_UP;
	} else if (button_state & BUTTON_DOWN && (curr_but_pressed != BUTTON_DOWN)) {
		printk("BUTTON DOWN\n");
		curr_but_pressed = BUTTON_DOWN;
	} else if (button_state & BUTTON_LEFT && (curr_but_pressed != BUTTON_LEFT)) {
		printk("BUTTON LEFT\n");
		curr_but_pressed = BUTTON_LEFT;
	} else if (button_state & BUTTON_RIGHT && (curr_but_pressed != BUTTON_RIGHT)) {
		printk("BUTTON RIGHT\n");
		curr_but_pressed = BUTTON_RIGHT;
	} else if (button_state & BUTTON_SELECT && (curr_but_pressed != BUTTON_SELECT)) {
		printk("BUTTON SELECT\n");
		curr_but_pressed = BUTTON_SELECT;

		if (system_state == s_idle) {
			system_state = s_prov_dev;
			k_sem_give(&sem_system_state);
		} else if (system_state == s_prov_dev) {
			system_state = s_conf_dev;
			k_sem_give(&sem_system_state);
		}
	} else if (!button_state) {
		curr_but_pressed = 0;
	}

	k_delayed_work_submit(&button_work, K_MSEC(100));
}

void main(void)
{
	int err;

	printk("Bluetooth Mesh Gateway initializing...\n");

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_ready();

	display_shield_init();

	k_delayed_work_init(&button_work, button_work_handler);
	k_delayed_work_submit(&button_work, K_NO_WAIT);

	display_set_cursor(0, 1);
	display_write_string("Idle state");

	while (1) {
		k_sem_take(&sem_system_state, K_FOREVER);

		if (system_state == s_idle) {
			display_clear();
			display_set_cursor(0, 1);
			display_write_string("Idle state");
		} else if (system_state == s_prov_dev) {
			display_clear();
			display_set_cursor(0, 1);
			display_write_string("Provisioning...");

			provision_device(15);
		} else if (system_state == s_conf_dev) {
			display_clear();
			display_set_cursor(0, 1);
			display_write_string("Configuring...");

			configure_device();
		}
	}
}
