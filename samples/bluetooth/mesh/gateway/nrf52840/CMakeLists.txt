cmake_minimum_required(VERSION 3.13.1)

find_package(Zephyr REQUIRED HINTS $ENV{ZEPHYR_BASE})
project(NONE)

target_sources(app PRIVATE
	src/main.c
	src/gw_prov_conf.c
	../common/slip.c
	../common/mqtt_serial.c
	../common/uart_simple.c)

target_include_directories(app PRIVATE include
	../common)
