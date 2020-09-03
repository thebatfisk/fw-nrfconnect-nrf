/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef PCA20036_ETHERNET_H
#define PCA20036_ETHERNET_H

#define SOCKET_DHCP 2
#define SOCKET_TFTP 3
#define SOCKET_TX 4
#define SOCKET_RX 5

#define TX_IP ((const uint8_t[]){ 255, 255, 255, 255 })
#define RX_PORT 10000
#define TX_PORT 10001
#define TX_FLAGS 0x00
#define TX_BUF_SIZE 2048

void get_own_ip(uint8_t *p_ip);

void get_own_mac(uint8_t *p_mac);

bool get_ethernet_initiated_flag(void);

int pca20036_ethernet_init(void);

#endif // PCA20036_ETHERNET_H
