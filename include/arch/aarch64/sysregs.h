// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

// ***************************************
// SCTLR_EL2, System Control Register (EL2)
// ***************************************

#define SCTLR_EE	       (0 << 25)
#define SCTLR_I_CACHE_DISABLED (0 << 12)
#define SCTLR_D_CACHE_DISABLED (0 << 2)
#define SCTLR_MMU_DISABLED     (0 << 0)
#define SCTLR_MMU_ENABLED      (1 << 0)

#define SCTLR_VALUE_MMU_DISABLED                                      \
	(SCTLR_EE | SCTLR_I_CACHE_DISABLED | SCTLR_D_CACHE_DISABLED | \
	 SCTLR_MMU_DISABLED)

// ***************************************
// HCR_EL2, Hypervisor Configuration Register (EL2)
// ***************************************

// trap related
#define HCR_TACR (1 << 21)
#define HCR_TID3 (1 << 18)
#define HCR_TID2 (1 << 17)
#define HCR_TID1 (1 << 16)
#define HCR_TWE	 (1 << 14)
#define HCR_TWI	 (1 << 13)
// others
#define HCR_E2H	 (0 << 34)
#define HCR_RW	 (1 << 31)
#define HCR_TGE	 (0 << 27)
#define HCR_AMO	 (1 << 5) // routing to EL2
#define HCR_IMO	 (1 << 4) // routing to EL2
#define HCR_FMO	 (1 << 3) // routing to EL2
#define HCR_SWIO (1 << 1)
#define HCR_VM	 (1 << 0) // stage 2 translation enable

#define HCR_VALUE                                                              \
	(HCR_TACR | HCR_TID3 | HCR_TID2 | HCR_TID1 | HCR_TWE | HCR_TWI |       \
	 HCR_E2H | HCR_RW | HCR_TGE | HCR_AMO | HCR_IMO | HCR_FMO | HCR_SWIO | \
	 HCR_VM)

// SCR_EL3, Secure Configuration Register (EL3)
// ***************************************

#define SCR_RESERVED (3 << 4)
#define SCR_RW	     (1 << 10)
#define SCR_HCE	     (1 << 8)
#define SCR_NS	     (1 << 0)

#define SCR_VALUE (SCR_RESERVED | SCR_RW | SCR_HCE | SCR_NS)

// ***************************************
// SPSR_EL3, Saved Program Status Register (EL3)
// ***************************************

#define SPSR_MASK_ALL (7 << 6)
#define SPSR_EL2h     (9 << 0)

#define SPSR_VALUE (SPSR_MASK_ALL | SPSR_EL2h)

// ***************************************
// VTCR_EL2,  Virtualization Translation Control Register (EL2)
// ***************************************

#define VTCR_NSA   (1 << 30)
#define VTCR_NSW   (1 << 29)
#define VTCR_VS	   (0 << 19)
#define VTCR_PS	   (2 << 16)
#define VTCR_TG0   (0 << 14) // 4KB
#define VTCR_SH0   (3 << 12)
#define VTCR_ORGN0 (0 << 10)
#define VTCR_IRGN0 (0 << 8)
#define VTCR_SL0   (1 << 6)
#define VTCR_T0SZ  (64 - 38)

#define VTCR_VALUE                                                       \
	(VTCR_NSA | VTCR_NSW | VTCR_VS | VTCR_PS | VTCR_TG0 | VTCR_SH0 | \
	 VTCR_ORGN0 | VTCR_IRGN0 | VTCR_SL0 | VTCR_T0SZ)

/* Flags for get_page_from_gva, gvirt_to_maddr etc */
#define GV2M_READ  (0u<<0)
#define GV2M_WRITE (1u<<0)
#define GV2M_EXEC  (1u<<1)

/* Indirect stringification.  Doing two levels allows the parameter to be a
 * macro itself.  For example, compile with -DFOO=bar, __stringify(FOO)
 * converts to "bar".
 */	
#define __stringify_1(x...) #x
#define __stringify(x...)   __stringify_1(x)

/* Access to system registers */
#define READ_SYSREG32(name) ((uint32_t)READ_SYSREG64(name))

#define WRITE_SYSREG32(v, name) WRITE_SYSREG64((uint64_t)v, name)

#define WRITE_SYSREG64(v, name) do {                    \
   	uint64_t _r = v;                                    \
    asm volatile("msr "__stringify(name)", %0" : : "r" (_r));       \
} while (0)
#define READ_SYSREG64(name) ({                          \
    uint64_t _r;                                        \
    asm volatile("mrs  %0, "__stringify(name) : "=r" (_r));         \
    _r; })

#define READ_SYSREG(name)     READ_SYSREG64(name)
#define WRITE_SYSREG(v, name) WRITE_SYSREG64(v, name)

