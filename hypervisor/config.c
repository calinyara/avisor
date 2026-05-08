// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "config.h"

static struct vm_config config[] = {
	{
		.load_addr = 0x8000000,
		.entry_point = 0x8000000,
		.sp = 0x0,
		.filename = "Image",
		.core_id = CORE0,
	},
};

const char *logo =
    "     __      ___	                \n"
    "     \\ \\    / (_)	        \n"
    "   __ \\ \\  / / _ ___  ___  _ __  \n"
    "  / _` \\ \\/ / | / __|/ _ \\| '__|\n"
    " | (_| |\\  /  | \\__ \\ (_) | |	\n"
    "  \\__,_| \\/   |_|___/\\___/|_|	\n";

struct vm_config *get_avisor_config(int nr)
{
	return &config[nr];
}

int get_avisor_config_amount(void)
{
	return sizeof(config) / sizeof(struct vm_config);
}

