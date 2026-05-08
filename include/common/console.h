// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#include "arch/aarch64/vm.h"

void init_console(struct avisor_console *c);
void flush_console(struct avisor_console *c);

