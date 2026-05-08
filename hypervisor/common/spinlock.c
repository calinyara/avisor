// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "common/spinlock.h"

// sucess 0, fail non 0
static inline int try_lock(spinlock_t* lock)
{
	int tmp, res;
	asm volatile(
		"ldaxr   %w0, [%2]\n"
		"cbnz    %w0, 1f\n"
		"stxr    %w1, %w3, [%2]\n"
		"1:"
		: "=&r" (tmp), "=&r" (res)
		: "r" (lock), "r" (1)
		: "memory");
	return res;
}

void spin_lock(spinlock_t* lock)
{
	while (try_lock(lock)) {
		asm volatile ("wfe");
	}
}

void spin_unlock(spinlock_t* lock)
{
	asm volatile(
		"stlr    wzr, [%0]"
		:: "r" (lock)
		: "memory");
}
