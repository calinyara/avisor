// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#include <inttypes.h>

#include "boards/raspi/irq.h"
#include "boards/raspi/gpio.h"
#include "boards/raspi/mini_uart.h"
#include "boards/raspi/timer.h"
#include "common/board.h"
#include "common/debug.h"
#include "common/fifo.h"
#include "common/mm.h"
#include "common/timer.h"
#include "common/utils.h"
#include "emulator/raspi/bcm2837.h"
#include "emulator/raspi/vmbox.h"

struct bcm2837_state {
	struct {
		uint8_t irq_enabled[72]; // IRQ 0-64, ARM Timer, ARM Mailbox, ...
		uint8_t fiq_control;
		uint32_t irqs_1_enabled;
		uint32_t irqs_2_enabled;
		uint8_t basic_irqs_enabled;
	} intctrl;

	struct {
		int mu_rx_overrun;
		uint8_t aux_enables;
		uint8_t aux_mu_io;
		uint8_t aux_mu_ier;
		uint8_t aux_mu_lcr;
		uint8_t aux_mu_mcr;
		uint8_t aux_mu_msr;
		uint8_t aux_mu_scratch;
		uint8_t aux_mu_cntl;
		uint16_t aux_mu_baud;
	} aux;

	struct {
		uint64_t last_physical_count;
		uint64_t offset;
		uint32_t cs;
		uint32_t c0;
		uint32_t c1;
		uint32_t c2;
		uint32_t c3;
		uint32_t c0_expire;
		uint32_t c1_expire;
		uint32_t c2_expire;
		uint32_t c3_expire;
	} systimer;
};

const struct bcm2837_state initial_state = {
  .intctrl = {
    .fiq_control        = 0x0,
    .irqs_1_enabled     = 0x0,
    .irqs_2_enabled     = 0x0,
    .basic_irqs_enabled = 0x0,
  },
  .aux = {
    .mu_rx_overrun  = 0,
    .aux_enables    = 0x0,
    .aux_mu_io      = 0x0,
    .aux_mu_ier     = 0x0,
    .aux_mu_lcr     = 0x0,
    .aux_mu_mcr     = 0x0,
    .aux_mu_msr     = 0x10,
    .aux_mu_scratch = 0x0,
    .aux_mu_cntl    = 0x3,
    .aux_mu_baud    = 0x0,
  },
  .systimer = {
    .cs  = 0x0,
    .c0  = 0x0,
    .c1  = 0x0,
    .c2  = 0x0,
    .c3  = 0x0,
  },
};

#define ADDR_IN_INTCTRL(a) \
	((a) >= IRQ_BASIC_PENDING && (a) <= DISABLE_BASIC_IRQS)
#define ADDR_IN_AUX(a)	    ((a) >= AUX_IRQ && (a) <= AUX_MU_BAUD_REG)
#define ADDR_IN_AUX_MU(a)   ((a) >= AUX_MU_IO_REG && (a) <= AUX_MU_BAUD_REG)
#define ADDR_IN_SYSTIMER(a) ((a) >= TIMER_CS && (a) <= TIMER_C3)
#define ADDR_IN_GPIO(a)     ((a) >= GPFSEL0 && (a) <= GPPUDCLK1)

void bcm2837_initialize(struct task_struct *tsk)
{
	struct bcm2837_state *s = (struct bcm2837_state *)allocate_page();
	*s = initial_state;

	s->systimer.last_physical_count = get_physical_timer_count();

	tsk->board_data = s;

	unsigned long begin = DEVICE_BASE;
	unsigned long end = PHYS_MEMORY_SIZE - SECTION_SIZE;
	for (; begin < end; begin += PAGE_SIZE) {
		set_task_page_notaccessable(tsk, begin);
	}
}

unsigned long handle_aux_read(struct task_struct *, unsigned long);

unsigned long handle_intctrl_read(struct task_struct *tsk, unsigned long addr)
{
#define BIT(v, n) ((v) & (1 << (n)))
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;
	switch (addr) {
	case IRQ_BASIC_PENDING: {
		int pending1 = handle_intctrl_read(tsk, IRQ_PENDING_1) != 0;
		int pending2 = handle_intctrl_read(tsk, IRQ_PENDING_2) != 0;
		return (pending1 << 8) | (pending2 << 9);
	}
	case IRQ_PENDING_1: {
		unsigned long systimer_match1 =
			BIT(s->intctrl.irqs_1_enabled, 1) &&
			(s->systimer.cs & 0x2);
		unsigned long systimer_match3 =
			BIT(s->intctrl.irqs_1_enabled, 3) &&
			(s->systimer.cs & 0x8);
		return (systimer_match1 << 1) | (systimer_match3 << 3);
	}
	case IRQ_PENDING_2: {
		unsigned long uart_int =
			BIT(s->intctrl.irqs_1_enabled, (57 - 32)) &&
			(handle_aux_read(tsk, AUX_IRQ) & 0x1);
		return (uart_int << (57 - 32));
	}
	case FIQ_CONTROL:
		return s->intctrl.fiq_control;
	case ENABLE_IRQS_1:
		return s->intctrl.irqs_1_enabled;
	case ENABLE_IRQS_2:
		return s->intctrl.irqs_2_enabled;
	case ENABLE_BASIC_IRQS:
		return s->intctrl.basic_irqs_enabled;
	case DISABLE_IRQS_1:
		return ~s->intctrl.irqs_1_enabled;
	case DISABLE_IRQS_2:
		return ~s->intctrl.irqs_2_enabled;
	case DISABLE_BASIC_IRQS:
		return ~s->intctrl.basic_irqs_enabled;
	}
	return 0;
}

void handle_intctrl_write(struct task_struct *tsk, unsigned long addr,
			  unsigned long val)
{
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;
	switch (addr) {
	case FIQ_CONTROL:
		s->intctrl.fiq_control = val;
		break;
	case ENABLE_IRQS_1:
		s->intctrl.irqs_1_enabled |= val;
		break;
	case ENABLE_IRQS_2:
		s->intctrl.irqs_2_enabled |= val;
		break;
	case ENABLE_BASIC_IRQS:
		s->intctrl.basic_irqs_enabled |= val;
		break;
	case DISABLE_IRQS_1:
		s->intctrl.irqs_1_enabled &= ~val;
		break;
	case DISABLE_IRQS_2:
		s->intctrl.irqs_2_enabled &= ~val;
		break;
	case DISABLE_BASIC_IRQS:
		s->intctrl.basic_irqs_enabled &= ~val;
		break;
	}
}

#define LCR_DLAB 0x80

unsigned long handle_aux_read(struct task_struct *tsk, unsigned long addr)
{
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;

	if ((s->aux.aux_enables & 1) == 0 && ADDR_IN_AUX_MU(addr)) {
		return 0;
	}

	switch (addr) {
	case AUX_IRQ: {
		int mu_pending = (s->aux.aux_enables & 0x1) &&
				 ~(handle_aux_read(tsk, AUX_MU_IIR_REG) & 0x1);
		return mu_pending;
	}
	case AUX_ENABLES:
		return s->aux.aux_enables;
	case AUX_MU_IO_REG:
		if (s->aux.aux_mu_lcr & LCR_DLAB) {
			s->aux.aux_mu_lcr &= ~LCR_DLAB;
			return s->aux.aux_mu_baud & 0xff;
		} else {
			unsigned long data;
			dequeue_fifo(tsk->console.in_fifo, &data);
			return data & 0xff;
		}
	case AUX_MU_IER_REG:
		if (s->aux.aux_mu_lcr & LCR_DLAB) {
			return s->aux.aux_mu_baud >> 8;
		} else {
			return s->aux.aux_mu_ier;
		}
	case AUX_MU_IIR_REG: {
		int tx_int = (s->aux.aux_mu_ier & 0x2) &&
			     is_empty_fifo(tsk->console.out_fifo);
		int rx_int = (s->aux.aux_mu_ier & 0x1) &&
			     !is_empty_fifo(tsk->console.in_fifo);
		int int_id = tx_int | (rx_int << 1);
		if (int_id == 0x3)
			int_id = 0x1;
		return (!int_id) | (int_id << 1) | (0x3 << 6);
	}
	case AUX_MU_LCR_REG:
		return s->aux.aux_mu_lcr;
	case AUX_MU_MCR_REG:
		return s->aux.aux_mu_mcr;
	case AUX_MU_LSR_REG: {
		int dready = !is_empty_fifo(tsk->console.in_fifo);
		int rx_overrun = s->aux.mu_rx_overrun;
		int tx_empty = !is_full_fifo(tsk->console.out_fifo);
		int tx_idle = is_empty_fifo(tsk->console.out_fifo);
		s->aux.mu_rx_overrun = 0;
		return dready | (rx_overrun << 1) | (tx_empty << 5) |
		       (tx_idle << 6);
	}
	case AUX_MU_MSR_REG:
		return s->aux.aux_mu_msr;
	case AUX_MU_SCRATCH:
		return s->aux.aux_mu_scratch;
	case AUX_MU_CNTL_REG:
		return s->aux.aux_mu_cntl;
	case AUX_MU_STAT_REG: {
#define MIN(a, b) ((a) < (b) ? (a) : (b))
		int sym_avail = !is_empty_fifo(tsk->console.in_fifo);
		int space_avail = !is_full_fifo(tsk->console.out_fifo);
		int rx_idle = is_empty_fifo(tsk->console.in_fifo);
		int tx_idle = !is_empty_fifo(tsk->console.out_fifo);
		int rx_overrun = s->aux.mu_rx_overrun;
		int tx_full = !space_avail;
		int tx_empty = is_empty_fifo(tsk->console.out_fifo);
		int tx_done = rx_idle & tx_empty;
		int rx_fifo_level = MIN(used_of_fifo(tsk->console.in_fifo), 8);
		int tx_fifo_level = MIN(used_of_fifo(tsk->console.out_fifo), 8);
		return sym_avail | (space_avail << 1) | (rx_idle << 2) |
		       (tx_idle << 3) | (rx_overrun << 4) | (tx_full << 5) |
		       (tx_empty << 8) | (tx_done << 9) |
		       (rx_fifo_level << 16) | (tx_fifo_level << 24);
	}
	case AUX_MU_BAUD_REG:
		return s->aux.aux_mu_baud;
	}
	return 0;
}

void handle_aux_write(struct task_struct *tsk, unsigned long addr,
		      unsigned long val)
{
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;

	if ((s->aux.aux_enables & 1) == 0 && ADDR_IN_AUX_MU(addr)) {
		return;
	}

	switch (addr) {
	case AUX_ENABLES:
		s->aux.aux_enables = val;
		break;
	case AUX_MU_IO_REG:
		if (s->aux.aux_mu_lcr & LCR_DLAB) {
			s->aux.aux_mu_lcr &= ~LCR_DLAB;
			s->aux.aux_mu_baud = (s->aux.aux_mu_baud & 0xff00) |
					     (val & 0xff);
		} else {
			enqueue_fifo(tsk->console.out_fifo, val & 0xff);
		}
		break;
	case AUX_MU_IER_REG:
		if (s->aux.aux_mu_lcr & LCR_DLAB) {
			s->aux.aux_mu_baud = (s->aux.aux_mu_baud & 0xff) |
					     ((val & 0xff) << 8);
		} else {
			s->aux.aux_mu_ier = val;
		}
		break;
	case AUX_MU_IIR_REG:
		if (val & 0x2)
			clear_fifo(tsk->console.in_fifo);
		if (val & 0x4)
			clear_fifo(tsk->console.out_fifo);
		break;
	case AUX_MU_LCR_REG:
		s->aux.aux_mu_lcr = val;
		break;
	case AUX_MU_MCR_REG:
		s->aux.aux_mu_mcr = val;
		break;
	case AUX_MU_SCRATCH:
		s->aux.aux_mu_scratch = val;
		break;
	case AUX_MU_CNTL_REG:
		s->aux.aux_mu_cntl = val;
		break;
	case AUX_MU_BAUD_REG:
		s->aux.aux_mu_baud = val;
		break;
	}
}

#define TO_VIRTUAL_COUNT(s, p)	(p - (s)->systimer.offset)
#define TO_PHYSICAL_COUNT(s, v) (v + (s)->systimer.offset)

unsigned long handle_systimer_read(struct task_struct *tsk, unsigned long addr)
{
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;
	switch (addr) {
	case TIMER_CS:
		return s->systimer.cs;
	case TIMER_CLO:
		return TO_VIRTUAL_COUNT(s, get_physical_timer_count()) &
		       0xffffffff;
	case TIMER_CHI:
		return TO_VIRTUAL_COUNT(s, get_physical_timer_count()) >> 32;
	case TIMER_C0:
		return s->systimer.c0;
	case TIMER_C1:
		return s->systimer.c1;
	case TIMER_C2:
		return s->systimer.c2;
	case TIMER_C3:
		return s->systimer.c3;
	}
	return 0;
}

void handle_systimer_write(struct task_struct *tsk, unsigned long addr,
			   unsigned long val)
{
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;
	uint32_t current_clo = handle_systimer_read(tsk, TIMER_CLO);
	const uint32_t min_expire =
		10000; // if this value is too short, CLO exceeds this value (timing problem)

	switch (addr) {
	case TIMER_CS:
		s->systimer.cs &= ~val;
		break;
	case TIMER_C0:
		s->systimer.c0 = val;
		s->systimer.c0_expire = MAX(
			val > current_clo ? val - current_clo : 1, min_expire);
		break;
	case TIMER_C1:
		s->systimer.c1 = val;
		s->systimer.c1_expire = MAX(
			val > current_clo ? val - current_clo : 1, min_expire);
		break;
	case TIMER_C2:
		s->systimer.c2 = val;
		s->systimer.c2_expire = MAX(
			val > current_clo ? val - current_clo : 1, min_expire);
		break;
	case TIMER_C3:
		s->systimer.c3 = val;
		s->systimer.c3_expire = MAX(
			val > current_clo ? val - current_clo : 1, min_expire);
		break;
	}
}

unsigned long handle_gpio_read(struct task_struct *tsk, unsigned long addr)
{
	uint64_t ret = 0;
	switch (addr) {
		case GPFSEL1:
			ret = 0x12000;
			break;
		default:
			WARN("gpio_read_addr=%lx\n", addr);
	}

	return ret;
}

unsigned long bcm2837_mmio_read(struct task_struct *tsk, unsigned long addr)
{
	if (ADDR_IN_INTCTRL(addr)) {
		return handle_intctrl_read(tsk, addr);
	} else if (ADDR_IN_AUX(addr)) {
		return handle_aux_read(tsk, addr);
	} else if (ADDR_IN_SYSTIMER(addr)) {
		return handle_systimer_read(tsk, addr);
	} else if (is_mbox_addr(addr)) {
		return handle_mbox_read(tsk, addr);
	} else if (ADDR_IN_GPIO(addr)) {
		return handle_gpio_read(tsk, addr);
	}
	return 0;
}

void bcm2837_mmio_write(struct task_struct *tsk, unsigned long addr,
			unsigned long val)
{
	if (ADDR_IN_INTCTRL(addr)) {
		handle_intctrl_write(tsk, addr, val);
	} else if (ADDR_IN_AUX(addr)) {
		handle_aux_write(tsk, addr, val);
	} else if (ADDR_IN_SYSTIMER(addr)) {
		handle_systimer_write(tsk, addr, val);
	} else if (is_mbox_addr(addr)) {
		handle_mbox_write(tsk, addr, val);
	}

	// WARN("addr=0x%lx, val=0x%lx\n", addr, val);
}

static int check_expiration(uint32_t *expire, uint64_t lapse)
{
	if (*expire == 0)
		return 0;

	if (lapse >= *expire) {
		*expire = 0;
		return 1;
	} else {
		*expire -= lapse;
		return 0;
	}
}

void bcm2837_entering_vm(struct task_struct *tsk)
{
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;

	// update systimer's offset
	unsigned long current_physical_count = get_physical_timer_count();
	uint64_t lapse =
		current_physical_count - s->systimer.last_physical_count;
	s->systimer.offset += lapse;

	// update cs register
	int matched = (check_expiration(&s->systimer.c0_expire, lapse)) |
		      (check_expiration(&s->systimer.c1_expire, lapse) << 1) |
		      (check_expiration(&s->systimer.c2_expire, lapse) << 2) |
		      (check_expiration(&s->systimer.c3_expire, lapse) << 3);

	// update (physical) timer compare value for upcoming timer match
	uint32_t upcoming = 0xffffffff;
	if (s->systimer.c0_expire && upcoming > s->systimer.c0_expire)
		upcoming = s->systimer.c0_expire;
	if (s->systimer.c1_expire && upcoming > s->systimer.c1_expire)
		upcoming = s->systimer.c1_expire;
	if (s->systimer.c2_expire && upcoming > s->systimer.c2_expire)
		upcoming = s->systimer.c2_expire;
	if (s->systimer.c3_expire && upcoming > s->systimer.c3_expire)
		upcoming = s->systimer.c3_expire;

	if (upcoming != 0xffffffff)
		put32(TIMER_C3, get32(TIMER_CLO) + upcoming);

	int fired = (~s->systimer.cs) & matched;
	s->systimer.cs |= fired;
}

void bcm2837_leaving_vm(struct task_struct *tsk)
{
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;
	s->systimer.last_physical_count = get_physical_timer_count();
}

int bcm2837_is_irq_asserted(struct task_struct *tsk)
{
	return handle_intctrl_read(tsk, IRQ_BASIC_PENDING) != 0;
}

int bcm2837_is_fiq_asserted(struct task_struct *tsk)
{
	struct bcm2837_state *s = (struct bcm2837_state *)tsk->board_data;

	if ((s->intctrl.fiq_control & 0x80) == 0)
		return 0;

	int source = s->intctrl.fiq_control & 0x7f;
	if (source >= 0 && source <= 31) {
		int pending = handle_intctrl_read(tsk, IRQ_PENDING_1);
		return (pending & (1 << source)) != 0;
	} else if (source >= 32 && source <= 63) {
		int pending = handle_intctrl_read(tsk, IRQ_PENDING_2);
		return (pending & (1 << (source - 32))) != 0;
	} else if (source >= 64 && source <= 71) {
		int pending = handle_intctrl_read(tsk, IRQ_BASIC_PENDING);
		return (pending & (1 << (source - 64))) != 0;
	}

	return 0;
}

void bcm2837_debug(struct task_struct *tsk)
{
}

const struct board_ops bcm2837_board_ops = {
	.initialize = bcm2837_initialize,
	.mmio_read = bcm2837_mmio_read,
	.mmio_write = bcm2837_mmio_write,
	.entering_vm = bcm2837_entering_vm,
	.leaving_vm = bcm2837_leaving_vm,
	.is_irq_asserted = bcm2837_is_irq_asserted,
	.is_fiq_asserted = bcm2837_is_fiq_asserted,
	.debug = bcm2837_debug,
};
