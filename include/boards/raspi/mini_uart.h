// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "boards/raspi/base.h"

#define AUX_IRQ		(PBASE + 0x00215000)
#define AUX_ENABLES	(PBASE + 0x00215004)
#define AUX_MU_IO_REG	(PBASE + 0x00215040)
#define AUX_MU_IER_REG	(PBASE + 0x00215044)
#define AUX_MU_IIR_REG	(PBASE + 0x00215048)
#define AUX_MU_LCR_REG	(PBASE + 0x0021504C)
#define AUX_MU_MCR_REG	(PBASE + 0x00215050)
#define AUX_MU_LSR_REG	(PBASE + 0x00215054)
#define AUX_MU_MSR_REG	(PBASE + 0x00215058)
#define AUX_MU_SCRATCH	(PBASE + 0x0021505C)
#define AUX_MU_CNTL_REG (PBASE + 0x00215060)
#define AUX_MU_STAT_REG (PBASE + 0x00215064)
#define AUX_MU_BAUD_REG (PBASE + 0x00215068)
