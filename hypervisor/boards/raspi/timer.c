// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#include "boards/raspi/timer.h"
#include "common/board.h"
#include "common/debug.h"
#include "common/sched.h"
#include "common/utils.h"

const unsigned int interval = 400000;

void timer_init(void)
{
	put32(TIMER_C1, get32(TIMER_CLO) + interval);
}

/* for task switch */
void handle_timer1_irq(void)
{
	put32(TIMER_C1, get32(TIMER_CLO) + interval);
	put32(TIMER_CS, TIMER_CS_M1);
	timer_tick();
}

/* for vm's interrupt */
void handle_timer3_irq(void)
{
	put32(TIMER_CS, TIMER_CS_M3);
}

unsigned long get_physical_timer_count(void)
{
	unsigned long clo = get32(TIMER_CLO);
	unsigned long chi = get32(TIMER_CHI);
	return clo | (chi << 32);
}

unsigned long get_system_timer(void)
{
	unsigned int h = -1, l;

	// we must read MMIO area as two separate 32 bit reads
	h = get32(TIMER_CHI);
	l = get32(TIMER_CLO);

	// we have to repeat it if high word changed during read
	if (h != get32(TIMER_CHI)) {
		h = get32(TIMER_CHI);
		l = get32(TIMER_CLO);
	}

	// compose long int value
	return ((unsigned long)h << 32) | l;
}

void show_systimer_info(void)
{
	printf("HI: %x\nLO: %x\nCS:%x\nC1: %x\nC3: %x\n", get32(TIMER_CHI),
	       get32(TIMER_CLO), get32(TIMER_CS), get32(TIMER_C1),
	       get32(TIMER_C3));
}
