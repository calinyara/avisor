// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

#ifndef __ASSEMBLER__

typedef volatile int spinlock_t;

void spin_lock(spinlock_t* lock);
void spin_unlock(spinlock_t* lock);

#endif // __ASSEMBLER__

