// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "common/entry.h"
#include "common/printf.h"
#include "common/sched.h"
#include "common/smp.h"
#include "common/spinlock.h"

#define DEBUG 1
#ifdef DEBUG

#define _LOG_COMMON(level, fmt, ...)					\
	do {								\
		spin_lock(&smp_print_lock);				\
		if (get_current_cpu_data()->current) {			\
			printf("%s[%d]: ", (level),			\
			get_current_cpu_data()->current->vmid);		\
		} else {						\
			printf("%s[?]: ", (level));			\
		}							\
		printf(fmt "\n", ##__VA_ARGS__);			\
		spin_unlock(&smp_print_lock);				\
	} while (0)

#define INFO(fmt, ...) _LOG_COMMON("INFO", fmt, ##__VA_ARGS__)
#define WARN(fmt, ...) _LOG_COMMON("WARN", fmt, ##__VA_ARGS__)

#else

#define _LOG_COMMON(level, fmt, ...)
#define INFO(fmt, ...)
#define WARN(fmt, ...)

#endif

#define PANIC(fmt, ...)							\
	do {								\
		_LOG_COMMON("!!! PANIC", fmt, ##__VA_ARGS__);		\
		if (get_current_cpu_data()->current)			\
			stop_vcpu();					\
		else							\
			err_hang();					\
	} while (0)
