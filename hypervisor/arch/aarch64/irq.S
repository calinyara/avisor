// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

.globl irq_vector_init
irq_vector_init:
	adr x0, vectors		/* load VBAR_EL2 with virtual address */
	msr vbar_el2, x0	/* vector table address */
	ret

.globl enable_irq
enable_irq:
	msr daifclr, #2
	ret

.globl disable_irq
disable_irq:
	msr daifset, #2
	ret
