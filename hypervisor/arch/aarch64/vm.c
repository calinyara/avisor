// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "arch/aarch64/vm.h"
#include "boards/raspi/base.h"
#include "common/console.h"
#include "common/loader.h"
#include "common/utils.h"
#include "config.h"

static uint8_t nr_vm = 0;
static struct avisor_vm vm_array[MAX_VM];
static struct avisor_hv hv;

static void init_stage2_pgtable(struct avisor_vm *vm)
{
	unsigned long begin = DEVICE_BASE;
	unsigned long end = PHYS_MEMORY_SIZE;
	for (; begin < end; begin += PAGE_SIZE) {
		set_vcpu_page_notaccessable(vm, begin);
	}
	/* BCM2836 local interrupt controller at 0x40000000 */
	set_vcpu_page_notaccessable(vm, 0x40000000UL);
}

int32_t create_vm(uint8_t vmid, struct vm_config *config)
{
	struct avisor_vm *vm = NULL;
	int32_t ret;
	uint64_t vcpu_id;
	uint64_t pcpu_id;

	vm = &vm_array[vmid];
	vm->vmid = vmid;
	vm->hw.created_vcpu = 0U;
	nr_vm++;

	init_stage2_pgtable(vm);
	init_console(&vm->console);
	(void)strncpy(vm->name, config->filename, 36);

	/* Create multiple VCPUs for multi-core Linux support */
	for (vcpu_id = 0; vcpu_id < MAX_VCPUS_PER_VM; vcpu_id++) {
		/* Distribute VCPUs across physical cores */
		pcpu_id = (config->core_id + vcpu_id) % MAX_PCPU;
		ret = create_vcpu(vm, raw_binary_loader, config, pcpu_id, vcpu_id);
		if (ret < 0) {
			return ret;
		}
	}

	return 0;
}

uint8_t get_nr_vm_created(void)
{
	return nr_vm;
}

struct avisor_vm* get_avisor_vm(uint8_t vm_id)
{
	return &vm_array[vm_id];
}

struct avisor_hv *get_avisor_hv(void)
{
	return &hv;
}

