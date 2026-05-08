// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "arch/aarch64/mmu.h"
#include "arch/aarch64/sysregs.h"
#include "boards/raspi/base.h"
#include "common/mm.h"

.section ".text.boot"

.globl _start
_start:
	mrs x0, mpidr_el1
	and x0, x0, #3
	mov x1, #VA_START
	add x2, x1, #LOW_MEMORY
	lsl x0, x0, #18		// 256KB stack
	sub x3, x2, x0
	mov sp, x3

	ldr x0, =SCTLR_VALUE_MMU_DISABLED
	msr sctlr_el2, x0

	/* Initial EL for Real HW is 3, for QEMU is 2
	 * Change EL from 3 to 2
	 */
	mrs x0, CurrentEL
	lsr x0, x0, #2
	cmp x0, #3
	beq el3

	ldr x0, =HCR_VALUE
	msr hcr_el2, x0

	ldr x0, =SPSR_VALUE
	msr spsr_el2, x0

	mrs x0, mpidr_el1
	and x0, x0, #3
	cbz x0, bsp0

	adr x0, secondary_cpu_entry
	msr elr_el2, x0
	eret

bsp0:
	adr x0, el2_entry
	msr elr_el2, x0
	eret

el3:
	ldr x0, =HCR_VALUE
	msr hcr_el2, x0

	ldr x0, =SCR_VALUE
	msr scr_el3, x0

	ldr x0, =SPSR_VALUE
	msr spsr_el3, x0

	mrs x0, mpidr_el1
	and x0, x0, #3
	cbz x0, bsp1

	adr x0, secondary_cpu_entry
	msr elr_el2, x0
	eret

bsp1:
	adr x0, el2_entry
	msr elr_el3, x0
	eret

el2_entry:
	adr x0, bss_begin
	adr x1, bss_end
	sub x1, x1, x0
	bl memzero

	bl __create_page_tables

	adrp x0, pg_dir
	msr ttbr0_el2, x0

	ldr x0, =(TCR_VALUE)
	msr tcr_el2, x0

	ldr x0, =(VTCR_VALUE)
	msr vtcr_el2, x0

	ldr x0, =(MAIR_VALUE)
	msr mair_el2, x0

	/* clear TLB */
	tlbi alle1

/* bsp to start the core 1, 2, 3 */
.equ spin_cpu1, 0xe0
.equ spin_cpu2, 0xe8
.equ spin_cpu3, 0xf0
	ldr x2, =_start
	mov x1, #spin_cpu1
	str	x2, [x1]
	mov x1, #spin_cpu2
	str	x2, [x1]
	mov x1, #spin_cpu3
	str	x2, [x1]
	sev

	ldr x2, =hypervisor_main

	mov x0, #SCTLR_MMU_ENABLED
	dsb ish
	isb
	msr sctlr_el2, x0
	isb

	br x2

	.macro create_pgd_entry, tbl, virt, tmp1, tmp2
	create_table_entry \tbl, \virt, PGD_SHIFT, \tmp1, \tmp2
	create_table_entry \tbl, \virt, PUD_SHIFT, \tmp1, \tmp2
	.endm

	.macro create_table_entry, tbl, virt, shift, tmp1, tmp2
	lsr \tmp1, \virt, #\shift
	and \tmp1, \tmp1, #PTRS_PER_TABLE - 1		/* table index */
	add \tmp2, \tbl, #PAGE_SIZE
	orr \tmp2, \tmp2, #MM_TYPE_PAGE_TABLE
	str \tmp2, [\tbl, \tmp1, lsl #3]
	add \tbl, \tbl, #PAGE_SIZE			/* next level table page */
	.endm

	.macro create_block_map, tbl, phys, start, end, flags, tmp1
	lsr \start, \start, #SECTION_SHIFT
	and \start, \start, #PTRS_PER_TABLE - 1		/* table index */
	lsr \end, \end, #SECTION_SHIFT
	and \end, \end, #PTRS_PER_TABLE - 1		/* table end index */
	lsr \phys, \phys, #SECTION_SHIFT
	mov \tmp1, #\flags
	orr \phys, \tmp1, \phys, lsl #SECTION_SHIFT	/* table entry */
1:	str \phys, [\tbl, \start, lsl #3]		/* store the entry */
	add \start, \start, #1				/* next entry */
	add \phys, \phys, #SECTION_SIZE			/* next block */
	cmp \start, \end
	b.ls 1b
	.endm

__create_page_tables:
	/* save return address */
	mov x29, x30

	adrp x0, pg_dir
	mov x1, #PG_DIR_SIZE
	bl memzero

	adrp x0, pg_dir
	mov x1, #VA_START
	create_pgd_entry x0, x1, x2, x3

	/* Fill up the second PUD entry for another 1GB */
	adrp x0, pg_dir
	add x0, x0, #PAGE_SIZE
	ldr x1, =(VA_START + PHYS_MEMORY_SIZE)
	lsr x2, x1, #PUD_SHIFT
	and x2, x2, #PTRS_PER_TABLE - 1
	add x3, x0, #PAGE_SIZE
	add x3, x3, #PAGE_SIZE
	orr x3, x3, #MM_TYPE_PAGE_TABLE
	str x3, [x0, x2, lsl #3]
	add x0, x0, #PAGE_SIZE

	/* Mapping kernel and init stack */
	mov x1, xzr						/* start mapping from physical offset 0 */
	mov x2, #VA_START					/* first virtual address */
	ldr x3, =(VA_START + DEVICE_BASE - SECTION_SIZE)	/* last virtual address */
	create_block_map x0, x1, x2, x3, MMU_FLAGS, x4

	/* Mapping device memory */
	mov x1, #DEVICE_BASE					/* start mapping from device base address */
	ldr x2, =(VA_START + DEVICE_BASE)			/* first virtual address */
	ldr x3, =(VA_START + PHYS_MEMORY_SIZE - SECTION_SIZE)	/* last virtual address */
	create_block_map x0, x1, x2, x3, MMU_DEVICE_FLAGS, x4

	/* Mapping the ARM timer, IRQs, mailboxes...
	 * According to QA7
	 * 0x0000_0000 .. 0x3FFF_FFFF:      GPU access
	 *     0x3E00_0000 .. 0x3FFF_FFFF:  GPU peripheral access
	 * 0x4000_0000 .. 0xFFFF_FFFF:      Local peripherals
	 *     0x4000_0000 .. 0x4001_FFFF:  ARM timer, IRQs, mailboxes
	 *     0x4002_0000 .. 0x4002_FFFF:  Debug ROM
	 *     0x4003_0000 .. 0x4003_FFFF:  DAP
	 *     0x4004_0000 .. 0xFFFF_FFFF:  <Unused>
	 */
	adrp x0, pg_dir
	add x0, x0, #PAGE_SIZE
	add x0, x0, #PAGE_SIZE
	add x0, x0, #PAGE_SIZE
	ldr x1, =(PHYS_MEMORY_SIZE)
	ldr x2, =(VA_START + PHYS_MEMORY_SIZE)
	ldr x3, =(VA_START + PHYS_MEMORY_SIZE)
	create_block_map x0, x1, x2, x3, MMU_DEVICE_FLAGS, x4

	/* restore return address */
	mov x30, x29
	ret

secondary_cpu_entry:
	mrs x0, mpidr_el1
	and x0, x0, #3
	ldr x5, =smp_cores
2:  wfe							/* APs will stop at here until the bsp starts them */
	ldr x4, [x5, x0, lsl #3]
	cbz x4, 2b

	adrp x0, pg_dir
	msr ttbr0_el2, x0

	ldr x0, =(TCR_VALUE)
	msr tcr_el2, x0

	ldr x0, =(VTCR_VALUE)
	msr vtcr_el2, x0

	ldr x0, =(MAIR_VALUE)
	msr mair_el2, x0

	tlbi alle1

	mov x0, #SCTLR_MMU_ENABLED
	dsb ish
	isb
	msr sctlr_el2, x0
	isb

	mrs x0, mpidr_el1
	and x0, x0, #3
	mov x1, #0
	mov x2, #0
	mov x3, #0
	br  x4

hang:
	wfi
	b hang

.ltorg

.globl smp_cores
smp_cores:
	.quad 0
	.quad 0
	.quad 0
	.quad 0

