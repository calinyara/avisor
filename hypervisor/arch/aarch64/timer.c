// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "arch/aarch64/timer.h"
#include "common/debug.h"
#include "common/sched.h"
#include "common/utils.h"

#define TICK_RATE_HZ 10

static uint32_t timer_cntfrq = 0;
static uint32_t ticks = 0;

uint64_t core0_tick_count = 0;
uint64_t core1_tick_count = 0;
uint64_t core2_tick_count = 0;
uint64_t core3_tick_count = 0;

void init_hv_timer(uint8_t core_id)
{
	timer_cntfrq = read_cntfrq(); /* 62500000 */
	ticks = timer_cntfrq / TICK_RATE_HZ ;
	write_cnthp_tval(timer_cntfrq);

	/* Route nCNTHP (hypervisor physical timer) to COREn IRQ. */
	switch (core_id) {
		case 0:
			mmio_write32(CORE0_TIMER_IRQCNTL, 1 << 2);
			break;
		case 1:
			mmio_write32(CORE1_TIMER_IRQCNTL, 1 << 2);
			break;
		case 2:
			mmio_write32(CORE2_TIMER_IRQCNTL, 1 << 2);
			break;
		case 3:
			mmio_write32(CORE3_TIMER_IRQCNTL, 1 << 2);
			break;
	}

	enable_cnthp();
}

void tick_handler(uint32_t core_id)
{
	write_cnthp_tval(ticks);

	switch (core_id) {
		case 0:
			core0_tick_count++;
			break;
		case 1:
			core1_tick_count++;
			break;
		case 2:
			core2_tick_count++;
			break;
		case 3:
			core3_tick_count++;
			break;
	}

	timer_tick();
}
