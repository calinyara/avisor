// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "common/event.h"

static void clear_event(event_notifier* en)
{
	en->event_flag = 0;
}

static void set_event(event_notifier* en)
{
	en->event_flag = 1;
}

void init_event(event_notifier* en)
{
	en->event_flag = 0;
}

void wait_for_event(event_notifier* en)
{
	while (!en->event_flag) {
		asm volatile("wfe");
	}
	clear_event(en);
}

void set_event_and_notify(event_notifier* en)
{
	set_event(en);
	asm volatile("sev");
}

