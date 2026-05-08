// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "common/sched.h"
#include "common/smp.h"
#include "common/types.h"
#include "common/utils.h"

spinlock_t smp_print_lock = 0;

static struct avisor_vcpu init_vcpu[MAX_PCPU] = {
	 [0 ... MAX_PCPU - 1] = INIT_VCPU
};

static per_cpu_data_t per_cpu_data[MAX_PCPU] = {0};
static struct cpu_sysregs initial_sysregs[MAX_PCPU];

static void set_entry(uint64_t address, uint64_t value)
{
	*(uint64_t *) address = value;
}

void start_secondary_cores(uint8_t nr, void (*func)(uint8_t nr))
{
	set_entry((uint64_t)(smp_cores + nr) , (uint64_t)func);
	asm volatile ("sev");
}

void clear_secondary_cores(uint8_t nr)
{
	set_entry((uint64_t)(smp_cores + nr), 0);
}

void init_per_cpu_data(void)
{
	uint8_t i;

	for(i = 0; i < MAX_PCPU; i++) {
		per_cpu_data[i].nr_vcpus = 1; // 0 is the hypervisor
		per_cpu_data[i].current = &(init_vcpu[i]);
		per_cpu_data[i].vcpu[0] = &(init_vcpu[i]);
		per_cpu_data[i].initial_sysregs = &(initial_sysregs[i]);
	}
}

per_cpu_data_t *get_current_cpu_data(void)
{
	uint64_t cpu_id = get_current_cpu_id();
	return &(per_cpu_data[cpu_id]);
}

per_cpu_data_t *get_cpu_data(uint64_t cpu_id)
{
	return &(per_cpu_data[cpu_id]);
}

