// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "arch/aarch64/vm.h"
#include "common/console.h"
#include "common/fifo.h"
#include "common/printf.h"
#include "common/spinlock.h"

void init_console(struct avisor_console *c)
{
	c->in_fifo = create_fifo();
	c->out_fifo = create_fifo();
	c->lock = 0;
}

void flush_console(struct avisor_console *c)
{
	struct fifo *outfifo = c->out_fifo;
	unsigned long val;
	char ch;

	spin_lock(&c->lock);
	while (dequeue_fifo(outfifo, &val) == 0) {
		ch = (char)(val & 0xff);
		_putchar(ch);
	}
	spin_unlock(&c->lock);
}

