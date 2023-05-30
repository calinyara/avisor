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

#define IRQ_BASIC_PENDING  (PBASE + 0x0000B200)
#define IRQ_PENDING_1	   (PBASE + 0x0000B204)
#define IRQ_PENDING_2	   (PBASE + 0x0000B208)
#define FIQ_CONTROL	   (PBASE + 0x0000B20C)
#define ENABLE_IRQS_1	   (PBASE + 0x0000B210)
#define ENABLE_IRQS_2	   (PBASE + 0x0000B214)
#define ENABLE_BASIC_IRQS  (PBASE + 0x0000B218)
#define DISABLE_IRQS_1	   (PBASE + 0x0000B21C)
#define DISABLE_IRQS_2	   (PBASE + 0x0000B220)
#define DISABLE_BASIC_IRQS (PBASE + 0x0000B224)

#define SYSTEM_TIMER_IRQ_0_BIT (1 << 0)
#define SYSTEM_TIMER_IRQ_1_BIT (1 << 1)
#define SYSTEM_TIMER_IRQ_2_BIT (1 << 2)
#define SYSTEM_TIMER_IRQ_3_BIT (1 << 3)
#define AUX_IRQ_BIT	       (1 << 29)
