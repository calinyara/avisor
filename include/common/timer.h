// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

void timer_init(void);
void handle_timer1_irq(void);
void handle_timer3_irq(void);
unsigned long get_physical_timer_count(void);
unsigned long get_system_timer(void);
