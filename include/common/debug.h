// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "common/entry.h"
#include "common/printf.h"
#include "common/sched.h"

#define _LOG_COMMON(level, fmt, ...)                               \
	do {                                                       \
		if (current) {                                     \
			printf("%s[%d]: ", (level), current->pid); \
		} else {                                           \
			printf("%s[?]: ", (level));                \
		}                                                  \
		printf(fmt "\n", ##__VA_ARGS__);                   \
	} while (0)

#define INFO(fmt, ...) _LOG_COMMON("INFO", fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) _LOG_COMMON("WARN", fmt, ##__VA_ARGS__)

#define PANIC(fmt, ...)                                       \
	do {                                                  \
		_LOG_COMMON("!!! PANIC", fmt, ##__VA_ARGS__); \
		if (current)                                  \
			exit_task();                          \
		else                                          \
			err_hang();                           \
	} while (0)
