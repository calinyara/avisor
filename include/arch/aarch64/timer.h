// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 * 
 * rdtsc: is using System counter
 *
 *   usage:
 *	t0 = rdtsc();
 *	...
 *	t1 = rdtsc();
 *	time = t1 - t0;
 *
 * rdtsc0: is useing Performance Monitors registers, which provide better precision
 *	   than rdtsc.
 *
 *   usage:
 *	call enable_pmu_pmccntr() once to enable the timer.
 *	t0 = rdtsc0();
 *	...
 *	t1 = rdtsc0();
 *	time = t1 - t0;
 */

#pragma once

#include "common/types.h"

#define CORE0_TIMER_IRQCNTL	0x40000040
#define CORE1_TIMER_IRQCNTL	0x40000044
#define CORE2_TIMER_IRQCNTL	0x40000048
#define CORE3_TIMER_IRQCNTL	0x4000004C
#define CORE0_TIMER_IRQSOURCE	0x40000060
#define CORE1_TIMER_IRQSOURCE	0x40000064
#define CORE2_TIMER_IRQSOURCE	0x40000068
#define CORE3_TIMER_IRQSOURCE	0x4000006C

extern uint64_t core0_tick_count;
extern uint64_t core1_tick_count;
extern uint64_t core2_tick_count;
extern uint64_t core3_tick_count;

void init_hv_timer(uint8_t core_id);
void tick_handler(uint32_t id);

static inline uint64_t read_cntvct(void)
{
	uint64_t tsc;
	asm volatile("mrs %0, cntvct_el0" : "=r"(tsc));
	return tsc;
}

static inline uint64_t read_cntfrq(void)
{
	uint64_t freq;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	return freq;
}

static inline uint64_t rdtsc(void)
{
	return read_cntvct();
}

static inline uint64_t read_pmccntr(void)
{
	uint64_t tsc;
	asm volatile("mrs %0, pmccntr_el0" : "=r"(tsc));
	return tsc;
}

static inline uint64_t rdtsc0(void)
{
	return read_pmccntr();
}

static inline void enable_pmu_pmccntr(void)
{
	uint64_t val = 0;
	/* Disable cycle counter overflow interrupt */
	asm volatile("msr pmintenset_el1, %0" : : "r"((uint64_t)(0 << 31)));
	/* Enable cycle counter */
	asm volatile("msr pmcntenset_el0, %0" ::"r"((uint64_t)(1 << 31)));
	/* Enable user-mode access to cycle counters. */
	asm volatile("msr pmuserenr_el0, %0"
		     :
		     : "r"((uint64_t)(1 << 0) | (uint64_t)(1 << 2)));
	/* Clear cycle counter and start */
	asm volatile("mrs %0, pmcr_el0" : "=r"(val));
	val |= ((uint64_t)(1 << 0) | (uint64_t)(1 << 2));
	asm volatile("isb");
	asm volatile("msr pmcr_el0, %0" : : "r"(val));
	val = (1 << 27);
	asm volatile("msr pmccfiltr_el0, %0" ::"r"(val));
}

static inline uint64_t tsc_2_microsec(uint64_t tsc)
{
	register uint64_t f;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(f));
	return (tsc * 1000000 / f);
}

static inline uint32_t read_cnthp_tval(void)
{
	uint32_t val;
	asm volatile ("mrs %0, cnthp_tval_el2" : "=r" (val));
	return val;
}

static inline void write_cnthp_tval(uint32_t val)
{
	asm volatile ("msr cnthp_tval_el2, %0" :: "r" (val));
	return;
}

static inline void enable_cnthp(void)
{
	uint32_t cnthp_ctl;
	cnthp_ctl = 1;
	asm volatile ("msr cnthp_ctl_el2, %0" :: "r" (cnthp_ctl));
}

static inline void write_cntv_tval(uint32_t val)
{
	asm volatile ("msr cntv_tval_el0, %0" :: "r" (val));
	return;
}

static inline void enable_cntv(void)
{
	uint32_t cntv_ctl;
	cntv_ctl = 1;
	asm volatile ("msr cntv_ctl_el0, %0" :: "r" (cntv_ctl));
}

