/*
 * Copyright (c) 2020 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#include <zephyr.h>
#include <nfc_t2t_lib.h>
#include <nfc/ndef/msg.h>
#include <nfc/ndef/text_rec.h>

#define MAX_REC_COUNT		3
#define NDEF_MSG_BUF_SIZE	128

/* UUID NDEF text message */
static uint8_t uuid_payload[16];
static const uint8_t uuid_code[] = {0xaa, 0xaa};

/* Buffer used to hold an NFC NDEF message */
static uint8_t ndef_msg_buf[NDEF_MSG_BUF_SIZE];


static void nfc_callback(void *context,
			 enum nfc_t2t_event event,
			 const uint8_t *data,
			 size_t data_length)
{
	ARG_UNUSED(context);
	ARG_UNUSED(data);
	ARG_UNUSED(data_length);

	switch (event) {
	case NFC_T2T_EVENT_FIELD_ON:
        printk("T2T event field ON\n");
		break;
	case NFC_T2T_EVENT_FIELD_OFF:
        printk("T2T event field OFF\n");
		break;
	default:
		break;
	}
}

static int mesh_msg_encode(uint8_t *buffer, uint32_t *len)
{
	int err;

	/* Using NDEF text record */
	NFC_NDEF_TEXT_RECORD_DESC_DEF(nfc_en_text_rec,
				      UTF_8,
				      uuid_code,
				      sizeof(uuid_code),
				      uuid_payload,
				      sizeof(uuid_payload));

	/* Create NFC NDEF message description, capacity - MAX_REC_COUNT
	 * records
	 */
	NFC_NDEF_MSG_DEF(nfc_text_msg, MAX_REC_COUNT);

	/* Add text records to NDEF text message */
	err = nfc_ndef_msg_record_add(&NFC_NDEF_MSG(nfc_text_msg),
				   &NFC_NDEF_TEXT_RECORD_DESC(nfc_en_text_rec));
	if (err < 0) {
		printk("Cannot add first record\n");
		return err;
	}

	err = nfc_ndef_msg_encode(&NFC_NDEF_MSG(nfc_text_msg),
				      buffer,
				      len);
	if (err < 0) {
		printk("Cannot encode message\n");
	}

	return err;
}

int mesh_nfc_init(const uint8_t *uuid)
{
	uint32_t len = sizeof(ndef_msg_buf);

    memcpy(uuid_payload, uuid, 16);

	printk("Initiating mesh NFC\n");

	if (nfc_t2t_setup(nfc_callback, NULL) < 0) {
		printk("Cannot setup NFC T2T library\n");
		return -EIO;
	}

	if (mesh_msg_encode(ndef_msg_buf, &len) < 0) {
		printk("Cannot encode mesh NFC message\n");
		return -EIO;
	}

	if (nfc_t2t_payload_set(ndef_msg_buf, len) < 0) {
		printk("Cannot set payload\n");
		return -EIO;
	}

	if (nfc_t2t_emulation_start() < 0) {
		printk("Cannot start emulation\n");
		return -EIO;
	}

	return 0;
}
