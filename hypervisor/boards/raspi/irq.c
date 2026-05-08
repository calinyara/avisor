// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "boards/raspi/irq.h"
#include "arch/aarch64/sysregs.h"
#include "arch/aarch64/timer.h"
#include "common/debug.h"
#include "common/entry.h"
#include "common/irq.h"
#include "common/mini_uart.h"
#include "common/sched.h"
#include "common/timer.h"
#include "common/utils.h"

const char *entry_error_messages[] = {
	"SYNC_INVALID_EL2",
	"IRQ_INVALID_EL2",
	"FIQ_INVALID_EL2",
	"ERROR_INVALID_EL2",

	"SYNC_INVALID_EL01_64",
	"IRQ_INVALID_EL01_64",
	"FIQ_INVALID_EL01_64",
	"ERROR_INVALID_EL01_64",

	"SYNC_INVALID_EL01_32",
	"IRQ_INVALID_EL01_32",
	"FIQ_INVALID_EL01_32",
	"ERROR_INVALID_EL01_32",
};

void enable_interrupt_controller()
{
	put32(ENABLE_IRQS_1, SYSTEM_TIMER_IRQ_1_BIT);
	put32(ENABLE_IRQS_1, SYSTEM_TIMER_IRQ_3_BIT);
	put32(ENABLE_IRQS_1, AUX_IRQ_BIT);
}

void show_invalid_entry_message(int type, unsigned long esr, unsigned long elr,
				unsigned long far)
{
	PANIC("uncaught exception(%s) esr: %x, elr: %x, far: %x",
	      entry_error_messages[type], esr, elr, far);
}

void handle_irq(uint64_t core_id)
{
	uint32_t int_id = 0;
	unsigned int irq;
	disable_irq();

	if (core_id == CORE0) {
		irq = get32(IRQ_PENDING_1);
		if (irq & SYSTEM_TIMER_IRQ_1_BIT) {
			irq &= ~SYSTEM_TIMER_IRQ_1_BIT;
			handle_timer1_irq();
		}

		if (irq & SYSTEM_TIMER_IRQ_3_BIT) {
			irq &= ~SYSTEM_TIMER_IRQ_3_BIT;
			handle_timer3_irq();
		}

		if (irq & AUX_IRQ_BIT) {
			irq &= ~AUX_IRQ_BIT;
			handle_uart_irq();
		}

		if (irq)
			WARN("unknown pending irq: %x", irq);
	}

	switch (core_id) {
		case 0:
			int_id = mmio_read32(CORE0_TIMER_IRQSOURCE) & 0xFFFUL;
			break;
		case 1:
			int_id = mmio_read32(CORE1_TIMER_IRQSOURCE) & 0xFFFUL;
			break;
		case 2:
			int_id = mmio_read32(CORE2_TIMER_IRQSOURCE) & 0xFFFUL;
			break;
		case 3:
			int_id = mmio_read32(CORE3_TIMER_IRQSOURCE) & 0xFFFUL;
			break;
		default:
			WARN("wrong core id: %u", core_id);
	}

	if (int_id & (1 << 2)) {
		tick_handler(core_id);
	}
}
