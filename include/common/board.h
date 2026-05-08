// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "common/sched.h"

#define HAVE_FUNC(ops, func, ...) ((ops) && ((ops)->func))

struct board_ops {
	void (*initialize)(struct avisor_vcpu *);
	unsigned long (*mmio_read)(struct avisor_vcpu *, unsigned long);
	void (*mmio_write)(struct avisor_vcpu *, unsigned long, unsigned long);
	void (*entering_vm)(struct avisor_vcpu *);
	void (*leaving_vm)(struct avisor_vcpu *);
	int (*is_irq_asserted)(struct avisor_vcpu *);
	int (*is_fiq_asserted)(struct avisor_vcpu *);
	void (*debug)(struct avisor_vcpu *);
};
