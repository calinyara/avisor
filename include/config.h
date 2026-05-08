// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "boards/raspi/cpu.h"
#include "common/types.h"

#define MAX_VCPU_PER_CORE 	8
//@ #define MAX_VM			(MAX_VCPU_PER_CORE * MAX_PCPU)
#define MAX_VM			1

#ifndef __ASSEMBLER__

struct vm_config {
	uint64_t load_addr;
	uint64_t entry_point;
	uint64_t sp;
	char filename[36];
	uint64_t core_id;
};

extern const char *logo;

struct vm_config *get_avisor_config(int nr);
int get_avisor_config_amount(void);

#endif // __ASSEMBLER__

