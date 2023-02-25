// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#pragma once

#include "common/mm.h"
#include "common/types.h"

struct page_pool {
	paddr_t start_addr;
	// spinlock_t lock;	// TODO: need a lock
	uint64_t page_nr;
	uint8_t *memap;
	uint64_t last_page_id;
};

struct page_pool *get_rasp3b_page_pool(void);
