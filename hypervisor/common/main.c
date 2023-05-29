// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#include <stddef.h>
#include <stdint.h>

#include "common/debug.h"
#include "common/irq.h"
#include "common/loader.h"
#include "common/mini_uart.h"
#include "common/mm.h"
#include "common/printf.h"
#include "common/sched.h"
#include "common/sd.h"
#include "common/shell.h"
#include "common/task.h"
#include "common/timer.h"
#include "common/utils.h"
#include "fs/diskio.h"
#include "fs/ff.h"

FATFS fatfs;

void hypervisor_main()
{
	uart_init();
	shell_init();
	printf("=== aVisor Hypervisor ===\n");

	init_task_console(current);
	init_initial_task();
	irq_vector_init();
	timer_init();
	disable_irq();
	enable_interrupt_controller();

	f_mount(&fatfs, "/", 0);

	struct raw_binary_loader_args bl_args1 = {
		.load_addr = 0x0,
		.entry_point = 0x0,
		.sp = 0x100000,
		.filename = "lrtos.bin",
	};

	if (create_task(raw_binary_loader, &bl_args1) < 0) {
		printf("error while starting task\n");
		return;
	}

	struct raw_binary_loader_args bl_args2 = {
		.load_addr = 0x0,
		.entry_point = 0x0,
		.sp = 0x100000,
		.filename = "echo.bin",
	};

	if (create_task(raw_binary_loader, &bl_args2) < 0) {
		printf("error while starting task\n");
		return;
	}

	struct raw_binary_loader_args bl_args3 = {
		.load_addr = 0x80000,
		.entry_point = 0x80000,
		.sp = 0x0,
		.filename = "uboot.bin",
	};

	if (create_task(raw_binary_loader, &bl_args3) < 0) {
		printf("error while starting task\n");
		return;
	}

	struct raw_binary_loader_args bl_args4 = {
		.load_addr = 0x80000,
		.entry_point = 0x80000,
		.sp = 0x0,
		.filename = "freertos.bin",
	};

	if (create_task(raw_binary_loader, &bl_args4) < 0) {
		printf("error while starting task\n");
		return;
	}

	while (1) {
		disable_irq();
		schedule();
		enable_irq();
	}
}
