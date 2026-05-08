// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include <inttypes.h>

#include "arch/aarch64/sysregs.h"
#include "arch/aarch64/vm.h"
#include "boards/raspi/irq.h"
#include "boards/raspi/gpio.h"
#include "boards/raspi/mini_uart.h"
#include "boards/raspi/timer.h"
#include "common/board.h"
#include "common/sched.h"
#include "common/console.h"
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
		uint32_t imsc;
	} pl011;

	struct {
		uint64_t last_physical_count;
		uint64_t offset;
		uint32_t cs;
		uint32_t c0;
		uint32_t c1;
		uint32_t c2;
		uint32_t c3;
		uint64_t c0_64;
		uint64_t c1_64;
		uint64_t c2_64;
		uint64_t c3_64;
	} systimer;
};

const struct bcm2837_state initial_state = {
  .intctrl = {
    .fiq_control        = 0x0,
    .irqs_1_enabled     = 0x0,
    .irqs_2_enabled     = 0x0,
    .basic_irqs_enabled = 0x0,
  },
  .pl011 = {
    .imsc = 0,
  },
  .aux = {
    .mu_rx_overrun  = 0,
    .aux_enables    = 0x1,
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

#define PL011_BASE   0x3f201000UL
#define PL011_DR     (PL011_BASE + 0x00)
#define PL011_FR     (PL011_BASE + 0x18)
#define PL011_IBRD   (PL011_BASE + 0x24)
#define PL011_FBRD   (PL011_BASE + 0x28)
#define PL011_LCRH   (PL011_BASE + 0x2c)
#define PL011_CR     (PL011_BASE + 0x30)
#define PL011_IFLS   (PL011_BASE + 0x34)
#define PL011_IMSC   (PL011_BASE + 0x38)
#define PL011_RIS    (PL011_BASE + 0x3c)
#define PL011_MIS    (PL011_BASE + 0x40)
#define PL011_ICR    (PL011_BASE + 0x44)
#define PL011_END    (PL011_BASE + 0x1000)
#define ADDR_IN_PL011(a) ((a) >= PL011_BASE && (a) < PL011_END)

#define BCM2836_LOCAL_BASE          0x40000000UL
#define BCM2836_LOCAL_IRQ_PEND0     (BCM2836_LOCAL_BASE + 0x60)
#define BCM2836_LOCAL_MBOX_SET0     (BCM2836_LOCAL_BASE + 0x80)
#define BCM2836_LOCAL_MBOX_RDCLR0   (BCM2836_LOCAL_BASE + 0xC0)
#define BCM2836_LOCAL_END           (BCM2836_LOCAL_BASE + 0x100)
#define ADDR_IN_LOCAL_INTC(a) ((a) >= BCM2836_LOCAL_BASE && (a) < BCM2836_LOCAL_END)

volatile uint32_t ipi_mbox[4][4]; /* [core][mailbox_nr] */

#define ADDR_IN_INTCTRL(a) \
	((a) >= IRQ_BASIC_PENDING && (a) <= DISABLE_BASIC_IRQS)
#define ADDR_IN_AUX(a)	    ((a) >= AUX_IRQ && (a) <= AUX_MU_BAUD_REG)
#define ADDR_IN_AUX_MU(a)   ((a) >= AUX_MU_IO_REG && (a) <= AUX_MU_BAUD_REG)
#define ADDR_IN_SYSTIMER(a) ((a) >= TIMER_CS && (a) <= TIMER_C3)
#define ADDR_IN_GPIO(a)     ((a) >= GPFSEL0 && (a) <= GPPUDCLK1)

void bcm2837_initialize(struct avisor_vcpu *vcpu)
{
	struct bcm2837_state *s = (struct bcm2837_state *)allocate_page();
	*s = initial_state;

	s->systimer.last_physical_count = get_physical_timer_count();

	vcpu->board_data = s;
}

unsigned long handle_aux_read(struct avisor_vcpu *vcpu, unsigned long);
int bcm2837_is_irq_asserted(struct avisor_vcpu *vcpu);
int bcm2837_pl011_irq_pending(struct avisor_vcpu *vcpu);
unsigned long handle_intctrl_read(struct avisor_vcpu *vcpu, unsigned long addr)
{
#define BIT(v, n) ((v) & (1 << (n)))
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
	switch (addr) {
	case IRQ_BASIC_PENDING: {
		int pending1 = handle_intctrl_read(vcpu, IRQ_PENDING_1) != 0;
		int pending2 = handle_intctrl_read(vcpu, IRQ_PENDING_2) != 0;
		return (pending1 << 8) | (pending2 << 9);
	}
	case IRQ_PENDING_1: {
		unsigned long systimer_match1 =
			BIT(s->intctrl.irqs_1_enabled, 1) &&
			(s->systimer.cs & 0x2);
		unsigned long systimer_match3 =
			BIT(s->intctrl.irqs_1_enabled, 3) &&
			(s->systimer.cs & 0x8);
		unsigned long aux_int =
			BIT(s->intctrl.irqs_1_enabled, 29) &&
			(handle_aux_read(vcpu, AUX_IRQ) & 0x1);
		return (systimer_match1 << 1) | (systimer_match3 << 3) |
		       (aux_int << 29);
	}
	case IRQ_PENDING_2: {
		unsigned long pl011_int =
			BIT(s->intctrl.irqs_2_enabled, 25) &&
			bcm2837_pl011_irq_pending(vcpu);
		return (pl011_int << 25);
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

void handle_intctrl_write(struct avisor_vcpu *vcpu, unsigned long addr,
			  unsigned long val)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
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

unsigned long handle_aux_read(struct avisor_vcpu *vcpu, unsigned long addr)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;

	if ((s->aux.aux_enables & 1) == 0 && ADDR_IN_AUX_MU(addr)) {
		return 0;
	}

	switch (addr) {
	case AUX_IRQ: {
		int mu_pending = (s->aux.aux_enables & 0x1) &&
				 ~(handle_aux_read(vcpu, AUX_MU_IIR_REG) & 0x1);
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
			dequeue_fifo(vcpu->vm->console.in_fifo, &data);
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
			     is_empty_fifo(vcpu->vm->console.out_fifo);
		int rx_int = (s->aux.aux_mu_ier & 0x1) &&
			     !is_empty_fifo(vcpu->vm->console.in_fifo);
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
		int dready = !is_empty_fifo(vcpu->vm->console.in_fifo);
		int rx_overrun = s->aux.mu_rx_overrun;
		int tx_empty = !is_full_fifo(vcpu->vm->console.out_fifo);
		int tx_idle = is_empty_fifo(vcpu->vm->console.out_fifo);
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
		int sym_avail = !is_empty_fifo(vcpu->vm->console.in_fifo);
		int space_avail = !is_full_fifo(vcpu->vm->console.out_fifo);
		int rx_idle = is_empty_fifo(vcpu->vm->console.in_fifo);
		int tx_idle = !is_empty_fifo(vcpu->vm->console.out_fifo);
		int rx_overrun = s->aux.mu_rx_overrun;
		int tx_full = !space_avail;
		int tx_empty = is_empty_fifo(vcpu->vm->console.out_fifo);
		int tx_done = rx_idle & tx_empty;
		int rx_fifo_level = MIN(used_of_fifo(vcpu->vm->console.in_fifo), 8);
		int tx_fifo_level = MIN(used_of_fifo(vcpu->vm->console.out_fifo), 8);
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

void handle_aux_write(struct avisor_vcpu *vcpu, unsigned long addr,
		      unsigned long val)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;

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
			int ret = enqueue_fifo(vcpu->vm->console.out_fifo, val & 0xff);
			if (ret < 0) {
				flush_console(&vcpu->vm->console);
				enqueue_fifo(vcpu->vm->console.out_fifo, val & 0xff);
			}
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
			clear_fifo(vcpu->vm->console.in_fifo);
		if (val & 0x4)
			clear_fifo(vcpu->vm->console.out_fifo);
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

unsigned long handle_systimer_read(struct avisor_vcpu *vcpu, unsigned long addr)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
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

static uint64_t calc_stc_64(struct avisor_vcpu *vcpu, uint64_t val)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
	uint32_t current_clo = handle_systimer_read(vcpu, TIMER_CLO);
	uint64_t stc;

	if (val > current_clo) {
		stc = (TO_VIRTUAL_COUNT(s, get_physical_timer_count()) &
			0xffffffff00000000) + val;
	} else {
		stc = (TO_VIRTUAL_COUNT(s, get_physical_timer_count()) &
			0xffffffff00000000) + val + 0x100000000;
	}

	return stc;
}

void handle_systimer_write(struct avisor_vcpu *vcpu, unsigned long addr,
			   unsigned long val)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;

	switch (addr) {
	case TIMER_CS:
		s->systimer.cs &= ~val;
		break;
	case TIMER_C0:
		s->systimer.c0 = val;
		s->systimer.c0_64 = calc_stc_64(vcpu, val);
		break;
	case TIMER_C1:
		s->systimer.c1 = val;
		s->systimer.c1_64 = calc_stc_64(vcpu, val);
		break;
	case TIMER_C2:
		s->systimer.c2 = val;
		s->systimer.c2_64 = calc_stc_64(vcpu, val);
		break;
	case TIMER_C3:
		s->systimer.c3 = val;
		s->systimer.c3_64 = calc_stc_64(vcpu, val);
		break;
	}
}

unsigned long handle_gpio_read(struct avisor_vcpu *vcpu, unsigned long addr)
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

static unsigned long pl011_ris(struct avisor_vcpu *vcpu)
{
	unsigned long ris = 0;
	if (!is_empty_fifo(vcpu->vm->console.in_fifo))
		ris |= (1 << 4);  /* RXRIS */
	if (!is_full_fifo(vcpu->vm->console.out_fifo))
		ris |= (1 << 5);  /* TXRIS */
	return ris;
}

int bcm2837_pl011_irq_pending(struct avisor_vcpu *vcpu)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
	return (pl011_ris(vcpu) & s->pl011.imsc) != 0;
}

static unsigned long handle_pl011_read(struct avisor_vcpu *vcpu, unsigned long addr)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
	struct fifo *in = vcpu->vm->console.in_fifo;
	switch (addr) {
	case PL011_DR: {
		unsigned long data = 0;
		dequeue_fifo(in, &data);
		return data & 0xff;
	}
	case PL011_FR: {
		int rxfe = is_empty_fifo(in) ? 1 : 0;
		int txfe = is_empty_fifo(vcpu->vm->console.out_fifo) ? 1 : 0;
		return (rxfe << 4) | (txfe << 7);
	}
	case PL011_CR:
		return 0x301;
	case PL011_LCRH:
		return 0x60;
	case PL011_RIS:
		return pl011_ris(vcpu);
	case PL011_MIS:
		return pl011_ris(vcpu) & s->pl011.imsc;
	case PL011_IMSC:
		return s->pl011.imsc;
	}
	return 0;
}

static void handle_pl011_write(struct avisor_vcpu *vcpu, unsigned long addr,
			       unsigned long val)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
	switch (addr) {
	case PL011_DR: {
		int ret = enqueue_fifo(vcpu->vm->console.out_fifo, val & 0xff);
		if (ret < 0) {
			flush_console(&vcpu->vm->console);
			enqueue_fifo(vcpu->vm->console.out_fifo, val & 0xff);
		}
		break;
	}
	case PL011_IMSC:
		s->pl011.imsc = val;
		break;
	case PL011_ICR:
		break;
	case PL011_CR:
	case PL011_LCRH:
	case PL011_IBRD:
	case PL011_FBRD:
	case PL011_IFLS:
		break;
	}
}

static unsigned long handle_local_intc_read(struct avisor_vcpu *vcpu,
					    unsigned long addr)
{
	if (addr >= BCM2836_LOCAL_IRQ_PEND0 &&
	    addr < BCM2836_LOCAL_IRQ_PEND0 + 16) {
		int core = (addr - BCM2836_LOCAL_IRQ_PEND0) >> 2;
		unsigned long pending = 0;
		if (bcm2837_is_irq_asserted(vcpu))
			pending |= (1 << 8);
		if (is_cntv_irq_pending())
			pending |= (1 << 3);
		for (int m = 0; m < 4; m++) {
			if (ipi_mbox[core][m])
				pending |= (1 << (4 + m));
		}
		return pending;
	}

	if (addr >= BCM2836_LOCAL_MBOX_RDCLR0 &&
	    addr < BCM2836_LOCAL_MBOX_RDCLR0 + 64) {
		unsigned int off = addr - BCM2836_LOCAL_MBOX_RDCLR0;
		int core = off >> 4;
		int mbox = (off >> 2) & 3;
		return ipi_mbox[core][mbox];
	}

	return 0;
}

static void handle_local_intc_write(struct avisor_vcpu *vcpu,
				    unsigned long addr, unsigned long val)
{
	(void)vcpu;

	if (addr >= BCM2836_LOCAL_MBOX_SET0 &&
	    addr < BCM2836_LOCAL_MBOX_SET0 + 64) {
		unsigned int off = addr - BCM2836_LOCAL_MBOX_SET0;
		int core = off >> 4;
		int mbox = (off >> 2) & 3;
		ipi_mbox[core][mbox] |= (uint32_t)val;
		asm volatile("dsb ish" ::: "memory");
		return;
	}

	if (addr >= BCM2836_LOCAL_MBOX_RDCLR0 &&
	    addr < BCM2836_LOCAL_MBOX_RDCLR0 + 64) {
		unsigned int off = addr - BCM2836_LOCAL_MBOX_RDCLR0;
		int core = off >> 4;
		int mbox = (off >> 2) & 3;
		ipi_mbox[core][mbox] &= ~(uint32_t)val;
		asm volatile("dsb ish" ::: "memory");
		return;
	}
}

unsigned long bcm2837_mmio_read(struct avisor_vcpu *vcpu, unsigned long addr)
{
	if (ADDR_IN_PL011(addr)) {
		return handle_pl011_read(vcpu, addr);
	} else if (ADDR_IN_LOCAL_INTC(addr)) {
		return handle_local_intc_read(vcpu, addr);
	} else if (ADDR_IN_INTCTRL(addr)) {
		return handle_intctrl_read(vcpu, addr);
	} else if (ADDR_IN_AUX(addr)) {
		return handle_aux_read(vcpu, addr);
	} else if (ADDR_IN_SYSTIMER(addr)) {
		return handle_systimer_read(vcpu, addr);
	} else if (is_mbox_addr(addr)) {
		return handle_mbox_read(vcpu, addr);
	} else if (ADDR_IN_GPIO(addr)) {
		return handle_gpio_read(vcpu, addr);
	}
	return 0;
}

void bcm2837_mmio_write(struct avisor_vcpu *vcpu, unsigned long addr,
			unsigned long val)
{
	if (ADDR_IN_PL011(addr)) {
		handle_pl011_write(vcpu, addr, val);
	} else if (ADDR_IN_LOCAL_INTC(addr)) {
		handle_local_intc_write(vcpu, addr, val);
	} else if (ADDR_IN_INTCTRL(addr)) {
		handle_intctrl_write(vcpu, addr, val);
	} else if (ADDR_IN_AUX(addr)) {
		handle_aux_write(vcpu, addr, val);
	} else if (ADDR_IN_SYSTIMER(addr)) {
		handle_systimer_write(vcpu, addr, val);
	} else if (is_mbox_addr(addr)) {
		handle_mbox_write(vcpu, addr, val);
	}
}

static int check_expiration(uint64_t stc64, uint64_t cvt)
{
	/* It's the time, if the current virtual time goes beyond 
	 * the system timer compare.
	 */
	if (cvt >= stc64)
		return 1;
	else
		return 0;
}

void bcm2837_entering_vm(struct avisor_vcpu *vcpu)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
	uint64_t current_virtual_count;
	uint64_t c0x;
	uint64_t c1x;
	uint64_t c2x;
	uint64_t c3x;

	// update systimer's offset
	unsigned long current_physical_count = get_physical_timer_count();
	uint64_t lapse =
		current_physical_count - s->systimer.last_physical_count;
	s->systimer.offset += lapse;

	current_virtual_count = TO_VIRTUAL_COUNT(s, get_physical_timer_count());

	// update cs register
	int matched = (check_expiration(s->systimer.c0_64, current_virtual_count)) |
		      (check_expiration(s->systimer.c1_64, current_virtual_count) << 1) |
		      (check_expiration(s->systimer.c2_64, current_virtual_count) << 2) |
		      (check_expiration(s->systimer.c3_64, current_virtual_count) << 3);

	// update (physical) timer compare value for upcoming timer match
	uint32_t upcoming = 0xffffffff;
	c0x = s->systimer.c0_64 - current_virtual_count;
	c1x = s->systimer.c1_64 - current_virtual_count;
	c2x = s->systimer.c2_64 - current_virtual_count;
	c3x = s->systimer.c3_64 - current_virtual_count;
	if (s->systimer.c0_64 > current_virtual_count && upcoming > c0x)
		upcoming = (uint32_t)c0x;
	if (s->systimer.c1_64 > current_virtual_count && upcoming > c1x)
		upcoming = (uint32_t)c1x;
	if (s->systimer.c2_64 > current_virtual_count && upcoming > c2x)
		upcoming = (uint32_t)c2x;
	if (s->systimer.c3_64 > current_virtual_count && upcoming > c3x)
		upcoming = (uint32_t)c3x;

	if (upcoming != 0xffffffff)
		put32(TIMER_C3, get32(TIMER_CLO) + upcoming);

	int fired = (~s->systimer.cs) & matched;
	s->systimer.cs |= fired;
}

void bcm2837_leaving_vm(struct avisor_vcpu *vcpu)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;
	s->systimer.last_physical_count = get_physical_timer_count();
}

int bcm2837_is_irq_asserted(struct avisor_vcpu *vcpu)
{
	return handle_intctrl_read(vcpu, IRQ_BASIC_PENDING) != 0;
}

int bcm2837_is_fiq_asserted(struct avisor_vcpu *vcpu)
{
	struct bcm2837_state *s = (struct bcm2837_state *)vcpu->board_data;

	if ((s->intctrl.fiq_control & 0x80) == 0)
		return 0;

	int source = s->intctrl.fiq_control & 0x7f;
	if (source >= 0 && source <= 31) {
		int pending = handle_intctrl_read(vcpu, IRQ_PENDING_1);
		return (pending & (1 << source)) != 0;
	} else if (source >= 32 && source <= 63) {
		int pending = handle_intctrl_read(vcpu, IRQ_PENDING_2);
		return (pending & (1 << (source - 32))) != 0;
	} else if (source >= 64 && source <= 71) {
		int pending = handle_intctrl_read(vcpu, IRQ_BASIC_PENDING);
		return (pending & (1 << (source - 64))) != 0;
	}

	return 0;
}

void bcm2837_debug(struct avisor_vcpu *vcpu)
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
