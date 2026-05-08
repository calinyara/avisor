// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "boards/raspi/cpu.h"
#include "common/console.h"
#include "common/printf.h"
#include "common/sched.h"
#include "common/board.h"
#include "common/debug.h"
#include "common/irq.h"
#include "common/mm.h"
#include "common/vcpu.h"
#include "common/utils.h"

void _schedule(void)
{
	int next, c;
	per_cpu_data_t *cpu_data = get_current_cpu_data();

	struct avisor_vcpu *vcpu;

	while (1) {
		c = -1;
		next = 0;

		for (int i = 0; i < MAX_VCPUS; i++) {
			vcpu = cpu_data->vcpu[i];

			if (vcpu && vcpu->state == VCPU_RUNNING && vcpu->counter > c) {
				c = vcpu->counter;
				next = i;
			}
		}

		if (c)
			break;

		for (int i = 0; i < MAX_VCPUS; i++) {
			vcpu = cpu_data->vcpu[i];

			if (vcpu)
				vcpu->counter = (vcpu->counter >> 1) + vcpu->priority;
		}
	}

	switch_to(cpu_data->vcpu[next]);
}

void schedule(void)
{
	get_current_cpu_data()->current->counter = 0;
	_schedule();
}

int is_cntv_irq_pending(void)
{
	uint64_t cntv_ctl;
	asm volatile("mrs %0, cntv_ctl_el0" : "=r"(cntv_ctl));
	return (cntv_ctl & 0x7) == 0x5; /* ENABLE && !IMASK && ISTATUS */
}

void set_cpu_virtual_interrupt(struct avisor_vcpu *vcpu)
{
	int virq = 0;

	if (HAVE_FUNC(vcpu->board_ops, is_irq_asserted) &&
	    vcpu->board_ops->is_irq_asserted(vcpu))
		virq = 1;

	if (is_cntv_irq_pending())
		virq = 1;

	{
		extern volatile uint32_t ipi_mbox[4][4];
		unsigned long cpu = get_current_cpu_id();
		if (cpu < 4) {
			for (int m = 0; m < 4; m++) {
				if (ipi_mbox[cpu][m]) {
					virq = 1;
					break;
				}
			}
		}
	}

	if (virq)
		assert_virq();
	else
		clear_virq();

	if (HAVE_FUNC(vcpu->board_ops, is_fiq_asserted) &&
	    vcpu->board_ops->is_fiq_asserted(vcpu))
		assert_vfiq();
	else
		clear_vfiq();
}

void switch_to(struct avisor_vcpu *next)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();

	if (cpu_data->current == next)
		return;

	struct avisor_vcpu *prev = cpu_data->current;
	cpu_data->current = next;

	cpu_switch_to(prev, next);
}

void timer_tick(void)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();

	--(cpu_data->current->counter);

	if (cpu_data->current->counter > 0)
		return;

	cpu_data->current->counter = 0;
	_schedule();
}

void stop_vcpu(void)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();

	for (int i = 0; i < MAX_VCPUS; i++) {
		if (cpu_data->vcpu[i] == cpu_data->current) {
			cpu_data->vcpu[i]->state = VCPU_ZOMBIE;
			break;
		}
	}

	schedule();
}

void set_cpu_sysregs(struct avisor_vcpu *vcpu)
{
	set_stage2_pgd(vcpu->vm->mm.first_table, vcpu->vmid);
	restore_sysregs(&vcpu->cpu_sysregs);
}

void vm_entering_work(void)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();

	if (HAVE_FUNC(cpu_data->current->board_ops, entering_vm))
		cpu_data->current->board_ops->entering_vm(cpu_data->current);

	flush_console(&cpu_data->current->vm->console);

	set_cpu_sysregs(cpu_data->current);
	set_cpu_virtual_interrupt(cpu_data->current);
}

void vm_leaving_work(void)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();

	save_sysregs(&cpu_data->current->cpu_sysregs);

	if (HAVE_FUNC(cpu_data->current->board_ops, leaving_vm))
		cpu_data->current->board_ops->leaving_vm(cpu_data->current);

	flush_console(&cpu_data->current->vm->console);
}

const char *vcpu_state_str[] = {
	"RUNNING",
	"ZOMBIE",
	"STOPPED",
};

void show_vcpu_list(void)
{
	struct avisor_vm* vm;
	struct avisor_vcpu* vcpu;

	printf("%4s %4s %4s %12s %8s %7s %18s %7s %7s %7s %7s %7s\n", "vm",
	       "pcpu", "vcpu", "image", "state", "pages", "saved-pc", "wfx",
	       "hvc", "sysreg", "pf", "mmio");

	for (int i = 0; i < get_nr_vm_created(); i++) {
		vm = get_avisor_vm(i);
		for (int j = 0; j < vm->hw.created_vcpu; j++) {
			vcpu = vm->hw.vcpu_array[j];
			printf("%4d %4d %4d %12s %8s %7d 0x%016lx %7d %7d %7d %7d %7d\n",
				i, vcpu->pcpu_id, vcpu->vcpu_id,
				vcpu->vm->name ? vcpu->vm->name : "", vcpu_state_str[vcpu->state],
				vcpu->vm->mm.user_pages_count, vcpu_pt_regs(vcpu)->pc,
				vcpu->stat.wfx_trap_count, vcpu->stat.hvc_trap_count,
				vcpu->stat.sysreg_trap_count, vcpu->stat.pf_count,
				vcpu->stat.mmio_count);
		}
	}
}
