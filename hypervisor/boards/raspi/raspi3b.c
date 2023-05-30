// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#include "boards/raspi/raspi3b.h"
#include "common/mm.h"
#include "common/types.h"

static uint8_t rasp3b_page_memap[PAGING_PAGES] = { 0 };

static struct page_pool rasp3b_page_pool = {
	.start_addr = LOW_MEMORY,
	.page_nr = PAGING_PAGES,
	.memap = rasp3b_page_memap,
	.last_page_id = 0,
};

struct page_pool *get_rasp3b_page_pool(void)
{
	return &rasp3b_page_pool;
}
