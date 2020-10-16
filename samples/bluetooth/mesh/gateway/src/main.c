/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <bluetooth/bluetooth.h>
#include <bluetooth/mesh.h>
#include <settings/settings.h>
// #include <bluetooth/mesh/models.h>
// #include <bluetooth/mesh/dk_prov.h>
#include "gw_prov_conf.h"
#include "gw_nfc.h"
#include "gw_display_shield.h"

K_SEM_DEFINE(sem_gw_system_state, 0, 1);
K_SEM_DEFINE(sem_choose_room, 0, 1);

enum gw_system_states
{
	s_start = 0,
	s_idle,
	s_blink_dev,
	s_prov_conf_dev,
};

// TODO: Unique node names
const struct room_info rooms[] = {
	[0] = { .name = "Living room    ", .group_addr = BASE_GROUP_ADDR },
	[1] = { .name = "Kitchen        ", .group_addr = BASE_GROUP_ADDR + 1 },
	[2] = { .name = "Bedroom        ", .group_addr = BASE_GROUP_ADDR + 2 },
	[3] = { .name = "Bathroom       ", .group_addr = BASE_GROUP_ADDR + 3 },
	[4] = { .name = "Hallway        ", .group_addr = BASE_GROUP_ADDR + 4 }
};

static int gw_system_state = s_start;
static bool take_sem = true;

static uint8_t curr_but_pressed;
static bool update_unprov_devs;
static uint8_t chosen_dev;
static uint8_t room_iterator;
static bool room_chosen;

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

		if (gw_system_state == s_blink_dev) {
			if (!prov_link_active()) {
				if (chosen_dev > 0) {
					chosen_dev--;
					k_sem_give(&sem_gw_system_state);
				}
			}
		} else if (gw_system_state == s_prov_conf_dev) {
			if (room_iterator > 0) {
				room_iterator--;
				k_sem_give(&sem_choose_room);
			}
		}
	} else if (button_state & BUTTON_RIGHT && (curr_but_pressed != BUTTON_RIGHT)) {
		printk("BUTTON RIGHT\n");
		curr_but_pressed = BUTTON_RIGHT;

		if (gw_system_state == s_blink_dev) {
			if (!prov_link_active()) {
				if (chosen_dev < (get_unprov_dev_num() - 1)) {
					chosen_dev++;
					k_sem_give(&sem_gw_system_state);
				}
			}
		} else if (gw_system_state == s_prov_conf_dev) {
			if (room_iterator < (sizeof(rooms) / sizeof(rooms[0]) - 1)) {
				room_iterator++;
				k_sem_give(&sem_choose_room);
			}
		}
	} else if (button_state & BUTTON_SELECT && (curr_but_pressed != BUTTON_SELECT)) {
		printk("BUTTON SELECT\n");
		curr_but_pressed = BUTTON_SELECT;

		if (gw_system_state == s_start) {
			gw_system_state = s_idle;
			k_sem_give(&sem_gw_system_state);
		} else if (gw_system_state == s_idle) {
			if (get_unprov_dev_num() > 0) {
				gw_system_state = s_blink_dev;
				k_sem_give(&sem_gw_system_state);
			}
		} else if (gw_system_state == s_blink_dev) {
			if (!prov_link_active()) {
				gw_system_state = s_prov_conf_dev;
				k_sem_give(&sem_gw_system_state);
			}
		} else if (gw_system_state == s_prov_conf_dev) {
			room_chosen = true;
			k_sem_give(&sem_choose_room);
		}
	} else if (!button_state) {
		curr_but_pressed = 0;
	}

	k_delayed_work_submit(&button_work, K_MSEC(100));
}

static struct k_delayed_work unprov_devs_work;

static void unprov_devs_work_handler(struct k_work *work)
{
	uint8_t num;

	num = get_unprov_dev_num();

	if (update_unprov_devs) {
		display_set_cursor(0, 0);
		display_write_number(num);
	}

	k_delayed_work_submit(&unprov_devs_work, K_MSEC(100));
}

void nfc_rx(struct gw_nfc_rx_data data)
{
	if (data.length == 16) {
		provision_devce_uuid(data.value);
	}
}

struct gw_nfc_cb nfc_cb = { .rx = nfc_rx };

void main(void)
{
	int err;

	printk("Bluetooth Mesh Gateway initializing...\n");

	gw_nfc_register_cb(&nfc_cb);
	gw_nfc_init();

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

	k_delayed_work_init(&unprov_devs_work, unprov_devs_work_handler);

	display_set_cursor(0, 0);
	display_write_string("BT Mesh Gateway");
	display_set_cursor(0, 1);
	display_write_string("Press SELECT");

	while (1) {
		if (take_sem) {
			k_sem_take(&sem_gw_system_state, K_FOREVER);
		}

		take_sem = true;

		if (gw_system_state == s_idle) {
			update_unprov_devs = false;

			display_clear();
			display_set_cursor(4, 0);
			display_write_string("unprov devs");
			display_set_cursor(0, 1);
			display_write_string("Press SELECT");
			update_unprov_devs = true;
			k_delayed_work_submit(&unprov_devs_work, K_NO_WAIT);

		} else if (gw_system_state == s_blink_dev) {
			update_unprov_devs = false;

			display_clear();
			display_set_cursor(4, 0);
			display_write_string("unprov devs");
			display_set_cursor(0, 1);
			display_write_string("Blinking dev ");
			display_write_number(chosen_dev);

			blink_device(chosen_dev);

			update_unprov_devs = true;
			k_delayed_work_submit(&unprov_devs_work, K_NO_WAIT);

		} else if (gw_system_state == s_prov_conf_dev) {
			k_delayed_work_cancel(&unprov_devs_work);
			update_unprov_devs = false;

			display_clear();
			display_set_cursor(0, 0);
			display_write_string("Provisioning...");

			provision_device(chosen_dev);

			struct model_info mod_inf = { .srv_count = 0, .cli_count = 0 };
			get_model_info(&mod_inf);
			room_iterator = 0;

			for (int i = 0; i < mod_inf.srv_count; i++) {
				room_chosen = false;
				display_clear();
				display_set_cursor(0, 0);
				display_write_string("Light ");
				display_write_number(i);
				while (!room_chosen)
				{
					display_set_cursor(0, 1);
					display_write_string(rooms[room_iterator].name);
					k_sem_take(&sem_choose_room, K_FOREVER);
				}

				configure_server(i, rooms[room_iterator].group_addr);
			}
			
			for (int i = 0; i < mod_inf.cli_count; i++) {
				room_chosen = false;
				display_clear();
				display_set_cursor(0, 0);
				display_write_string("Light Switch ");
				display_write_number(i);
				while (!room_chosen)
				{
					display_set_cursor(0, 1);
					display_write_string(rooms[room_iterator].name);
					k_sem_take(&sem_choose_room, K_FOREVER);
				}

				configure_client(i, rooms[room_iterator].group_addr);
			}

			gw_system_state = s_idle;
			take_sem = false;
		}
	}
}
