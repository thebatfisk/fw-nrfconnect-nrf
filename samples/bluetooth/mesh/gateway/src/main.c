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
#include "gw_nfc.h"

K_SEM_DEFINE(sem_button, 0, 1);

enum modes
{
	m_provision = 0,
	m_configure,
	m_testing,
};

static int mode = -1;

static void button_handler(uint32_t pressed, uint32_t changed)
{
	if ((pressed & BIT(0))) {
		mode = m_provision;
		k_sem_give(&sem_button);
	} else if ((pressed & BIT(1))) {
		mode = m_configure;
		k_sem_give(&sem_button);
	} else if ((pressed & BIT(2))) {
		mode = m_testing;
		k_sem_give(&sem_button);
	}
}

void nfc_rx(struct gw_nfc_rx_data data)
{
	uint8_t test_buf[20];

	// if length == 16 -> memcpy to uuid variable

	printk("NFC NDEF message:\n");

	for (int j = 0; j < data.length; j++) {
		test_buf[j] = *(data.value + j);
		printk("0x%x\n", test_buf[j]);
	}
}

struct gw_nfc_cb nfc_cb = { .rx = nfc_rx };

void main(void)
{
	int err;

	printk("Bluetooth Mesh Gateway initializing...\n");

	dk_buttons_init(button_handler);

	gw_nfc_init();

	gw_nfc_register_cb(&nfc_cb);

	/* Initialize the Bluetooth Subsystem */
	err = bt_enable(NULL);
	if (err) {
		printk("Bluetooth init failed (err %d)\n", err);
		return;
	}

	printk("Bluetooth initialized\n");

	bt_ready();

	while (1) {
		k_sem_take(&sem_button, K_FOREVER);

		if (mode == m_provision) {
			provision_device(15);
		} else if (mode == m_configure) {
			configure_device();
		} else if (mode == m_testing) {
			testing();
		}
	}
}
