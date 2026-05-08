// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "common/types.h"
#include "common/sched.h"

#define MAX_VCPUS_PER_VM	4

struct avisor_hv {
	struct avisor_console console;
	uint8_t nr_vm_ready;
	spinlock_t nr_vm_ready_lock;
};

struct vm_hw_info {
	struct avisor_vcpu *vcpu_array[MAX_VCPUS_PER_VM];
	uint64_t cpu_affinity;
	uint16_t created_vcpu;
};

struct avisor_vm {
	struct vm_hw_info hw;
	struct mm_struct mm;
	struct avisor_console console;
	uint8_t vmid;
	char name[36];
};

int32_t create_vm(uint8_t vmid, struct vm_config *config);
uint8_t get_nr_vm_created(void);
struct avisor_vm* get_avisor_vm(uint8_t vm_id);
struct avisor_hv *get_avisor_hv(void);

