// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "boards/raspi/mini_uart.h"
#include "boards/raspi/gpio.h"
#include "common/console.h"
#include "common/fifo.h"
#include "common/printf.h"
#include "common/sched.h"
#include "common/shell.h"
#include "common/smp.h"
#include "common/vcpu.h"
#include "common/utils.h"

void uart_send(char c)
{
	while (1) {
		if (get32(AUX_MU_LSR_REG) & 0x20)
			break;
	}
	put32(AUX_MU_IO_REG, c);
}

char uart_recv(void)
{
	while (1) {
		if (get32(AUX_MU_LSR_REG) & 0x01)
			break;
	}

	char c = get32(AUX_MU_IO_REG) & 0xFF;

	return c;
}

int is_uart_forwarded_vm(struct avisor_vm *vm)
{
	return (vm->vmid == uart_forwarded_vm) &&
		(uart_forwarded_hv == false);
}

#define ESCAPE_CHAR '@'

void handle_uart_irq(void)
{
	int vm_id;

	if (uart_forwarded_hv == true) {
		shell_kick();
	} else {
		static int is_escaped = 0;
		char received = get32(AUX_MU_IO_REG) & 0xff;
		if (is_escaped) {
			is_escaped = 0;
			if (isdigit(received)) {
				vm_id = received - '0';
				if (vm_id > get_nr_vm_created())
					goto clear_int;

				printf("\n--> Switched to VM %d <--\n", vm_id);
				flush_console(&(get_avisor_vm(vm_id)->console));
				uart_forwarded_hv = false;
				uart_forwarded_vm = vm_id;
			} else if (received == 'l') {
				show_vcpu_list();
			} else if (received == 'c') {
				printf("\n--> Switched to HV Console <--\n");
				printf(SHELL_PROMPT_STR);
				uart_forwarded_hv = true;
				flush_console(&(get_avisor_hv()->console));
			} else if (received == ESCAPE_CHAR) {
				goto enqueue_char;
			}
		} else if (received == ESCAPE_CHAR) {
			is_escaped = 1;
		} else {
enqueue_char:
			enqueue_fifo(get_avisor_vm(uart_forwarded_vm)->console.in_fifo, received);
		}
	}

clear_int:
	/* clear interrupt */
	put32(AUX_MU_IIR_REG, 0x2);
}

void uart_init(void)
{
	unsigned int selector;

	selector = get32(GPFSEL1);
	selector &= ~(7 << 12); /* clean gpio14 */
	selector |= 2 << 12; /* set alt5 for gpio14 */
	selector &= ~(7 << 15); /* clean gpio15 */
	selector |= 2 << 15; /* set alt5 for gpio15 */
	put32(GPFSEL1, selector);

	put32(GPPUD, 0);
	delay(150);
	put32(GPPUDCLK0, (1 << 14) | (1 << 15));
	delay(150);
	put32(GPPUDCLK0, 0);

	put32(AUX_ENABLES,
	      1); /* Enable mini uart (this also enables access to it registers) */
	put32(AUX_MU_CNTL_REG,
	      0); /* Disable auto flow control and disable receiver */
	/* and transmitter (for now) */
	put32(AUX_MU_IER_REG, 1); /* Enable receive interrupt */
	put32(AUX_MU_LCR_REG, 3); /* Enable 8 bit mode */
	put32(AUX_MU_MCR_REG, 0); /* Set RTS line to be always high */
	put32(AUX_MU_BAUD_REG, 270); /* Set baud rate to 115200 */

	put32(AUX_MU_CNTL_REG,
	      3); /* Finally, enable transmitter and receiver */
}

/* This function is required by printf function */
void _putchar(char c)
{
	uart_send(c);
}
