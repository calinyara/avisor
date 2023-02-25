// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (jiedeng@alumni.sjtu.edu.cn).
 */

#include "boards/raspi/phys2bus.h"

unsigned long phys_to_bus(unsigned long phys)
{
	return 0xc0000000 | phys;
}

unsigned long bus_to_phys(unsigned long bus)
{
	return bus & ~0xc0000000;
}
