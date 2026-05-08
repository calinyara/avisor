// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "boards/raspi/timer.h"
#include "common/board.h"
#include "common/debug.h"
#include "common/sched.h"
#include "common/shell.h"
#include "common/utils.h"
#include "common/vcpu.h"

const unsigned int interval = 400000;
const unsigned int crystal_freq = 1000000; // 1s

void init_misc_timer(void)
{
	put32(TIMER_C1, get32(TIMER_CLO) + crystal_freq);
}

void handle_timer1_irq(void)
{
	// /* for vcpu switch */
	//put32(TIMER_C1, get32(TIMER_CLO) + interval);
	//put32(TIMER_CS, TIMER_CS_M1);
	//timer_tick();

	struct avisor_hv *hv = get_avisor_hv();
	uint8_t nr_config_vm = get_avisor_config_amount();
	uint8_t nr_vm_ready = hv->nr_vm_ready;

	if (nr_vm_ready == nr_config_vm) {
		/* for the first prompt str */
		put32(TIMER_CS, TIMER_CS_M1);
		printf("\n"SHELL_PROMPT_STR);
	} else {
		put32(TIMER_C1, get32(TIMER_CLO) + interval);
		put32(TIMER_CS, TIMER_CS_M1);
	}
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
