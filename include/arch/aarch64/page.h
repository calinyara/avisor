// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#pragma once

#include "common/types.h"
#include "common/mm.h"
#include "arch/aarch64/sysregs.h"

#define PADDR_MASK (((1ULL) << 52) - 1)

static inline uint64_t gva_to_ma_par(vaddr_t va, unsigned int flags)
{
	uint64_t par, tmp = READ_SYSREG64(PAR_EL1);

	if ((flags & GV2M_WRITE) == GV2M_WRITE)
		asm volatile("at s12e1w, %0;" : : "r"(va));
	else
		asm volatile("at s12e1r, %0;" : : "r"(va));

	asm volatile("isb" : : : "memory");
	par = READ_SYSREG64(PAR_EL1);
	WRITE_SYSREG64(tmp, PAR_EL1);
	return par;
}

static inline uint64_t gvirt_to_maddr(vaddr_t va, paddr_t *pa,
                                      unsigned int flags)
{
    uint64_t par = gva_to_ma_par(va, flags);
    if ( par & 0x1 )
        return par;
    *pa = (par & PADDR_MASK & PAGE_MASK) | ((unsigned long) va & ~PAGE_MASK);
    return 0;
}

