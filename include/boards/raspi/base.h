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

#define DEVICE_BASE 0x3F000000
#define PBASE	    (VA_START + DEVICE_BASE)
