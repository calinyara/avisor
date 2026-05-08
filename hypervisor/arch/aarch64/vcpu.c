// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "arch/aarch64/vm.h"
#include "common/board.h"
#include "common/debug.h"
#include "common/entry.h"
#include "common/fifo.h"
#include "common/loader.h"
#include "common/mm.h"
#include "common/sched.h"
#include "common/smp.h"
#include "common/spinlock.h"
#include "common/vcpu.h"
#include "common/utils.h"
#include "emulator/raspi/bcm2837.h"
#include "config.h"

int uart_forwarded_vm = 0;
bool uart_forwarded_hv = true;

struct pt_regs *vcpu_pt_regs(struct avisor_vcpu *vcpu)
{
	unsigned long p =
		(unsigned long)vcpu + THREAD_SIZE - sizeof(struct pt_regs);
	return (struct pt_regs *)p;
}

static void prepare_initial_sysregs(void)
{
	static int is_first_call = 1;
	per_cpu_data_t *cpu_data;

	if (!is_first_call)
		return;

	for (int i = 0; i < MAX_PCPU; i++) {
		cpu_data = get_cpu_data(i);
		get_all_sysregs(cpu_data->initial_sysregs);
		/*
		 * Enter Linux with a clean EL1 state:
		 * - MMU off (M=0)
		 * - D-cache off (C=0)
		 * - I-cache off (I=0)
		 *
		 * We must not inherit host cache/MMU state into the guest, otherwise
		 * early Linux mappings (e.g. fixmap_remap_fdt) can observe stale
		 * cache lines (we see DTB magic turning into 0x0 at the point of read).
		 */
		cpu_data->initial_sysregs->sctlr_el1 &= ~(1UL << 0);  /* M */
		cpu_data->initial_sysregs->sctlr_el1 &= ~(1UL << 2);  /* C */
		cpu_data->initial_sysregs->sctlr_el1 &= ~(1UL << 12); /* I */
		/* MPIDR will be set per VCPU in create_vcpu */
	}

	is_first_call = 0;
}

static void prepare_vcpu(loader_func_t loader, void *arg)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();
	struct pt_regs *regs = vcpu_pt_regs(cpu_data->current);
	struct avisor_hv *hv = get_avisor_hv();

	regs->pstate = PSR_MODE_EL1h;
	/* interrupt mask */
	regs->pstate |= (0xf << 6);

	INFO("loading...");
	if (loader(arg, regs) < 0)
		PANIC("failed to load");
	INFO("loaded");

	spin_lock(&hv->nr_vm_ready_lock);
	hv->nr_vm_ready++;
	spin_unlock(&hv->nr_vm_ready_lock);
}

void increment_current_pc(int ilen)
{
	struct pt_regs *regs = vcpu_pt_regs(get_current_cpu_data()->current);
	regs->pc += ilen;
}

int create_vcpu(struct avisor_vm *vm, loader_func_t loader, void *arg, uint64_t pcpu_id, uint64_t vcpu_id)
{
	struct avisor_vcpu *vcpu;
	per_cpu_data_t *cpu_data = get_cpu_data(pcpu_id);
	struct vm_config *config_args = arg;

	vcpu = (struct avisor_vcpu *)allocate_page();
	struct pt_regs *childregs = vcpu_pt_regs(vcpu);

	if (!vcpu)
		return -1;

	vm->hw.vcpu_array[vcpu_id] = vcpu;
	vcpu->vm = vm;
	vcpu->vmid = vm->vmid;
	vcpu->cpu_context.x19 = (unsigned long)prepare_vcpu;
	vcpu->cpu_context.x20 = (unsigned long)loader;
	vcpu->cpu_context.x21 = (unsigned long)arg;
	vcpu->flags = 0;
	vcpu->priority = cpu_data->current->priority;
	/* Primary VCPU (vcpu_id == 0) starts running, secondary VCPUs start stopped */
	vcpu->state = (vcpu_id == 0) ? VCPU_RUNNING : VCPU_STOPPED;
	vcpu->counter = vcpu->priority;

	vcpu->board_ops = &bcm2837_board_ops;
	if (HAVE_FUNC(vcpu->board_ops, initialize))
		vcpu->board_ops->initialize(vcpu);

	prepare_initial_sysregs();
	memcpy(&vcpu->cpu_sysregs, cpu_data->initial_sysregs,
		sizeof(struct cpu_sysregs));
	/* Set MPIDR_EL1 to vcpu_id so Linux can detect multiple CPUs */
	vcpu->cpu_sysregs.mpidr_el1 = vcpu_id;

	/* For primary VCPU, set up normal startup path */
	if (vcpu_id == 0) {
		vcpu->cpu_context.pc = (unsigned long)switch_from_kthread;
		vcpu->cpu_context.sp = (unsigned long)childregs;
	} else {
		/* Secondary VCPUs will be started via PSCI CPU_ON */
		/* Initialize registers but keep them in stopped state */
		struct pt_regs *regs = vcpu_pt_regs(vcpu);
		/* These will be set by PSCI CPU_ON */
		regs->pc = 0;
		regs->sp = 0;
		regs->pstate = PSR_MODE_EL1h;
		regs->pstate |= (0xf << 6); /* interrupt mask */
		/* Set cpu_context.pc to special entry point for secondary VCPUs */
		extern void switch_to_secondary_vcpu(void);
		vcpu->cpu_context.pc = (unsigned long)switch_to_secondary_vcpu;
		vcpu->cpu_context.sp = (unsigned long)childregs;
	}
	int pid = cpu_data->nr_vcpus++;
	vcpu->pid = pid;
	vcpu->pcpu_id = pcpu_id;
	vcpu->vcpu_id = vcpu_id;
	cpu_data->vcpu[pid] = vcpu;
	vcpu->vm->hw.created_vcpu++;

	return pid;
}

