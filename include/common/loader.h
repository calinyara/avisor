// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "common/sched.h"
#include "common/task.h"

struct raw_binary_loader_args {
	unsigned long load_addr;
	unsigned long entry_point;
	unsigned long sp;
	const char *filename;
};

int raw_binary_loader(void *, struct pt_regs *regs);
