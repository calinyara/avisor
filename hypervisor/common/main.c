// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include <stddef.h>
#include <stdint.h>

#include "arch/aarch64/timer.h"
#include "common/console.h"
#include "common/debug.h"
#include "common/irq.h"
#include "common/loader.h"
#include "common/mini_uart.h"
#include "common/mm.h"
#include "common/printf.h"
#include "common/sched.h"
#include "common/sd.h"
#include "common/shell.h"
#include "common/smp.h"
#include "common/vcpu.h"
#include "common/timer.h"
#include "common/utils.h"
#include "fs/diskio.h"
#include "fs/ff.h"
#include "config.h"

FATFS fatfs;
extern uint64_t smp_cores[MAX_PCPU];

static void init_hv(void)
{
	struct avisor_hv *hv = get_avisor_hv();
	init_console(&hv->console);
	hv->nr_vm_ready = 0;
	hv->nr_vm_ready_lock = 0;
}

void secondary_vcpu_entry_log(uint64_t core_id, uint64_t entry_pc)
{
	(void)core_id;
	(void)entry_pc;
}

void secondary_main(uint8_t core_id)
{
	irq_vector_init();
	init_hv_timer(core_id);
	disable_irq();

	while (1) {
		schedule();
	}
}

void hypervisor_main()
{
	init_per_cpu_data();
	uart_init();
	shell_init();
	printf("%s\n", logo);

	init_hv();
	irq_vector_init();
	init_misc_timer();
	init_hv_timer(0);
	disable_irq();
	enable_interrupt_controller();

	{
		FRESULT fr = f_mount(&fatfs, "/", 1);
		if (fr != FR_OK) {
			printf("f_mount failed: %d\n", fr);
		}
	}

	struct vm_config *config;
	for (int i = 0; i < get_avisor_config_amount(); i++) {
		config = get_avisor_config(i);
		if (create_vm(i, config) < 0) {
			printf("error while create vm %d\n", i);
			return;
		}
	}

	start_secondary_cores(1, secondary_main);
	start_secondary_cores(2, secondary_main);
	start_secondary_cores(3, secondary_main);

	while (1) {
		schedule();
	}
}

