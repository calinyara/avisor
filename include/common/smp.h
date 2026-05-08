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
#include "common/spinlock.h"

typedef struct {
	struct avisor_vcpu *current;
	struct avisor_vcpu *vcpu[MAX_VCPUS];
	struct cpu_sysregs *initial_sysregs;
	uint16_t nr_vcpus;
} per_cpu_data_t;

extern uint64_t smp_cores[MAX_PCPU];
extern spinlock_t smp_print_lock;

void start_secondary_cores(uint8_t nr, void (*func)(uint8_t nr));
void clear_secondary_cores(uint8_t nr);
void init_per_cpu_data(void);
per_cpu_data_t *get_current_cpu_data(void);
per_cpu_data_t *get_cpu_data(uint64_t cpu_id);

