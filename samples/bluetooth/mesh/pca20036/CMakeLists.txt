#
# Copyright (c) 2021 Nordic Semiconductor ASA
#
# SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
#
cmake_minimum_required(VERSION 3.8.2)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

target_sources(app PRIVATE
	src/main.c
	src/model_handler.c
	src/pca20036_ethernet.c
	src/ethernet_dfu.c
	src/ethernet_command_system.c
	src/ethernet_utils.c
	src/hp_led.c
	../../../../../modules/hal/wiznet/Callback/dhcp_cb.c
	../../../../../modules/hal/wiznet/Ethernet/W5500/w5500.c
	../../../../../modules/hal/wiznet/Ethernet/socket.c
	../../../../../modules/hal/wiznet/Ethernet/wizchip_conf.c
	../../../../../modules/hal/wiznet/Internet/DHCP/dhcp.c
	../../../../../modules/hal/wiznet/Internet/TFTP/tftp.c
	../../../../../modules/hal/wiznet/Internet/TFTP/netutil.c)
target_include_directories(app PRIVATE include
	../../../../../modules/hal/wiznet/Callback
	../../../../../modules/hal/wiznet/Ethernet
	../../../../../modules/hal/wiznet/Ethernet/W5500
	../../../../../modules/hal/wiznet/Internet/DHCP
	../../../../../modules/hal/wiznet/Internet/TFTP)
