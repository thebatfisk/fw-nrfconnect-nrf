/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <string.h>
#include <drivers/flash.h>
#include <storage/flash_map.h>
#include <dfu/mcuboot.h>

#include "pca20036_ethernet.h"
#include "w5500.h"
#include "socket.h"
#include "tftp.h"

#define FLASH_IMG_1_OFFSET FLASH_AREA_OFFSET(image_1)
#define FLASH_BLOCK_SIZE 512
#define FLASH_PAGE_SIZE 4096

static struct device *flash_dev;

static struct k_delayed_work tftp_work;

static uint8_t tftp_rcv_buf[MAX_MTU_SIZE];

static void tftp_work_handler(struct k_work *work)
{
	static uint8_t tick = 0;
	uint32_t ret;
	int err;

	ret = TFTP_run();

	if (ret == TFTP_PROGRESS) {
		if (tick < 100) {
			tick++;
		} else {
			tftp_timeout_handler();
			tick = 0;
		}

		k_delayed_work_submit(&tftp_work, K_MSEC(10));
	} else {
		if (ret == 2) {
			printk("TFTP finished successfully\n");

			err = boot_request_upgrade(BOOT_UPGRADE_TEST);

			if (err) {
				printk("Error requesting firmware upgrade\n");
			} else {
				NVIC_SystemReset();
			}
		} else {
			printk("TFTP failed\n");
			tick = 0;
		}
	}
}

static void tftp_work_init_start(void)
{
	k_delayed_work_init(&tftp_work, tftp_work_handler);
	k_delayed_work_submit(&tftp_work, K_MSEC(10));

	printk("TFTP transfer started\n");
}

static void flash_init(void)
{
	flash_dev = device_get_binding(
		DT_PROP(DT_NODELABEL(flash_controller), label));

	if (!flash_dev) {
		printk("Flash init: Error getting device\n");
	} else {
		printk("Flash initiated - Image 1 offset: 0x%x\n",
		       FLASH_IMG_1_OFFSET);
	}
}

extern void save_data(uint8_t *data, uint32_t data_len, uint16_t block_number)
{
	static uint8_t block_counter = 0;
	static uint16_t page_counter = 0;
	int err;

	// printk("TFTP block number %d\n", block_number);

	err = flash_write_protection_set(flash_dev, false);
	if (err) {
		printk("Error removing flash write protection\n");
		return;
	}

	if (block_counter == 0) {
		err = flash_erase(flash_dev,
				  FLASH_IMG_1_OFFSET +
					  (page_counter * FLASH_PAGE_SIZE),
				  FLASH_PAGE_SIZE);

		if (err) {
			printk("Error erasing flash (%d)\n", err);
			return;
		}
	}

	err = flash_write(flash_dev,
			  FLASH_IMG_1_OFFSET +
				  (page_counter * FLASH_PAGE_SIZE) +
				  (block_counter * FLASH_BLOCK_SIZE),
			  data, data_len);

	if (err) {
		printk("Error writing to flash (%d)\n", err);
		return;
	}

	if (block_counter >= 7) {
		block_counter = 0;
		page_counter++;
	} else {
		block_counter++;
	}

	flash_write_protection_set(flash_dev, true);
	if (err) {
		printk("Error setting flash write protection\n");
	}
}

/******* Interface functions *******/

void ethernet_dfu_start(uint8_t *server_ip)
{
	if (get_ethernet_initiated_flag()) {
		uint32_t tftp_server;
		uint8_t *filename;

		printk("Starting DFU\n");

		TFTP_init(SOCKET_TFTP, tftp_rcv_buf);

		tftp_server = (*(server_ip) << 24) | (*(server_ip + 1) << 16) |
			      (*(server_ip + 2) << 8) | (*(server_ip + 3));
		filename = (uint8_t *)"app_update.bin";

		TFTP_read_request(tftp_server, filename);

		tftp_work_init_start();
	} else {
		printk("Ethernet not initiated - Can not start DFU\n");
	}
}

void ethernet_dfu_confirm_image(void)
{
	boot_write_img_confirmed();
}

void ethernet_dfu_init(void)
{
	flash_init();
}
