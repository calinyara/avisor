// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
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

static inline uint64_t arm64_cntvct(void)
{
	uint64_t tsc;
	asm volatile("mrs %0, cntvct_el0" : "=r"(tsc));
	return tsc;
}

static inline uint64_t arm64_cntfrq(void)
{
	uint64_t freq;
	asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
	return freq;
}

static inline uint64_t rdtsc(void)
{
	return arm64_cntvct();
}

static inline uint64_t arm64_pmccntr(void)
{
	uint64_t tsc;
	asm volatile("mrs %0, pmccntr_el0" : "=r"(tsc));
	return tsc;
}

static inline uint64_t rdtsc0(void)
{
	return arm64_pmccntr();
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
