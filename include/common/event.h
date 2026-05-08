// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#pragma once

typedef struct {
	volatile int event_flag;
} event_notifier;

void init_event(event_notifier* en);
void wait_for_event(event_notifier* en);
void set_event_and_notify(event_notifier* en);

