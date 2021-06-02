/*
 * Copyright (c) 2021 Nordic Semiconductor ASA
 *
 * SPDX-License-Identifier: LicenseRef-BSD-5-Clause-Nordic
 */

#ifndef ETHERNET_LOGGING_H
#define ETHERNET_LOGGING_H

#define ETHERNET_LOGGING_THREAD_STACKSIZE 1024

enum ext_log_level {
	EXT_LOG_CRITICAL,
	EXT_LOG_ERROR,
	EXT_LOG_WARNING,
	EXT_LOG_NOTICE,
	EXT_LOG_INFO,
	EXT_LOG_DEBUG
};

typedef void (*ext_log_handler)(enum ext_log_level level, const char *format,
				...);

#define ext_log(level, ...) log_handler(level, __VA_ARGS__)

#endif // ETHERNET_LOGGING_H
