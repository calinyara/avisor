// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "arch/aarch64/vm.h"
#include "common/sched.h"
#include "common/types.h"
#include "config.h"

/*
 * PSR bits
 */
#define PSR_MODE_EL0t 0x00000000
#define PSR_MODE_EL1t 0x00000004
#define PSR_MODE_EL1h 0x00000005
#define PSR_MODE_EL2t 0x00000008
#define PSR_MODE_EL2h 0x00000009
#define PSR_MODE_EL3t 0x0000000c
#define PSR_MODE_EL3h 0x0000000d

extern int uart_forwarded_vm;
extern bool uart_forwarded_hv;

/* Don't forget update S_FRAME_SIZE
 * S_FRAME_SIZE = sizeof(struct pt_regs)
 */
struct pt_regs {
	unsigned long regs[31];
	unsigned long sp;
	unsigned long pc;
	unsigned long pstate;
	unsigned long sp_el0;
	unsigned long sp_el1;
};

typedef int (*loader_func_t)(void *, struct pt_regs *regs);

struct pt_regs *vcpu_pt_regs(struct avisor_vcpu *vcpu);
int create_vcpu(struct avisor_vm *vm, loader_func_t loader, void *arg, uint64_t pcpu_id, uint64_t vcpu_id);
int is_uart_forwarded_vm(struct avisor_vm *vm);
void increment_current_pc(int);

