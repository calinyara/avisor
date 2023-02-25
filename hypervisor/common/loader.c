// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#include <inttypes.h>

#include "common/debug.h"
#include "common/loader.h"
#include "common/mm.h"
#include "common/sched.h"
#include "common/utils.h"
#include "fs/ff.h"

// va should be page-aligned.
int load_file_to_memory(struct task_struct *tsk, const char *name,
			unsigned long va)
{
	unsigned long gva = va & PAGE_MASK;
	uint8_t *buf;
	FRESULT r;
	UINT br;
	FIL f;

	r = f_open(&f, name, FA_READ);
	if (r) {
		PANIC("Can't open the file: %s, err=%d\n", name, r);
		return -r;
	}

	for (;;) {
		buf = allocate_task_page(tsk, gva);
		r = f_read(&f, buf, PAGE_SIZE, &br);
		if (br == 0) { /* error or eof */
			deallocate_page(buf);
			break;
		}
		gva += PAGE_SIZE;
	}

	f_close(&f);
	INFO("file: %s loaded", name);

	return -r;
}

int raw_binary_loader(void *arg, struct pt_regs *regs)
{
	struct raw_binary_loader_args *loader_args = arg;

	if (load_file_to_memory(current, loader_args->filename,
				loader_args->load_addr) < 0)
		return -1;
	current->name = loader_args->filename;

	regs->pc = loader_args->entry_point;
	regs->sp = loader_args->sp;
	regs->regs[0] = 0;
	regs->regs[1] = 0;
	regs->regs[2] = 0;

	return 0;
}
