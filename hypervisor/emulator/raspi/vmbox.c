// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2022 Deng Jie (mr.dengjie@gmail.com).
 */

#include "emulator/raspi/vmbox.h"
#include "arch/aarch64/page.h"
#include "arch/aarch64/sysregs.h"
#include "boards/raspi/base.h"
#include "boards/raspi/phys2bus.h"
#include "common/errno.h"
#include "common/debug.h"

/*
 * TODO:
 *   1. mbox emulation should be per VM
 *   2. mbox emulation
 *      step 1: read from the real hardware
 *      step 2: return the info to the virtualized machine
 */

// The buffer must be 16-byte aligned as only the upper 28 bits of the address can be passed via the mailbox
// volatile uint32_t __attribute__((aligned(16))) mbox[36];
static volatile uint32_t *mbox;
static uint32_t mbox_val = 0;

enum {
	VIDEOCORE_MBOX = (DEVICE_BASE + 0x0000B880),
	MBOX_READ = (VIDEOCORE_MBOX + 0x0),
	MBOX_POLL = (VIDEOCORE_MBOX + 0x10),
	MBOX_SENDER = (VIDEOCORE_MBOX + 0x14),
	MBOX_STATUS = (VIDEOCORE_MBOX + 0x18),
	MBOX_CONFIG = (VIDEOCORE_MBOX + 0x1C),
	MBOX_WRITE = (VIDEOCORE_MBOX + 0x20),
	MBOX_RESPONSE = 0x80000000,
	MBOX_FULL = 0x80000000,
	MBOX_EMPTY = 0x40000000
};

static uint64_t emulate_mbox_read(struct task_struct *tsk)
{
	/*
         * Prepare the mbox[] and waiting for the read.
         * mbox[0] is the buffer size in bytes (including the header values,
         *         the end tag and padding)
         * mbox[1] is the type, request or response.
         * mbox[2] is the tag.
         * mbox[3] is the val buf size.
         * mbox[4] MSB indicate the type, remaining bits indicate buf size
         * mbox[5] and mbox[6] are val.
         * mbox[7] is end tag (= 0).
         */
	if ((mbox_val & 0xF) == MBOX_CHAN_TAGS && mbox[1] == MBOX_REQUEST) {
		switch (mbox[2]) {
		case MBOX_TAG_GET_ARM_MEMORY:
			mbox[1] = MBOX_RESPONSE;
			mbox[4] = MBOX_RESPONSE | mbox[3];
			mbox[5] = 0x0; /* RAM start addr */
			mbox[6] = 0x3c000000; /* RAM Length */
			break;
		case MBOX_TAG_GET_BOARD_SERIAL:
			mbox[1] = MBOX_RESPONSE;
			mbox[4] = MBOX_RESPONSE | mbox[3];
			mbox[5] = 0x0; /* serial number 0 */
			mbox[6] = 0x0; /* serial number 1 */
			break;
		case MBOX_TAG_SET_POWER_STATE:
			mbox[1] = MBOX_RESPONSE;
			mbox[4] = MBOX_RESPONSE | mbox[3];
			/* mbox[5] is the device id, keep the same */
			mbox[6] = 0x1; /* state bits */
			break;
		default:
			WARN("Unsupported mbox TAG:%x\n", mbox[2]);
		}
	}

	return mbox_val;
}

static uint64_t emulate_mbox_write(struct task_struct *tsk, uint64_t val)
{
	vaddr_t gva = (bus_to_phys(val) & ~0xF);
	paddr_t maddr = 0;
	uint64_t err;
	mbox_val = (uint32_t)val;

	/*
	 * Check if the gva mapped or not. It should always be mapped.
	 */
	if (!check_task_page_mapped(tsk, gva)) {
		PANIC("mbox buf not mapped!!!\n");
		return -EFAULT;
	}

	err = gvirt_to_maddr(gva, &maddr, GV2M_WRITE);
	mbox = (uint32_t *)maddr;

	return 0;
}

bool is_mbox_addr(uint64_t addr)
{
	return (addr >= VIDEOCORE_MBOX && addr <= MBOX_WRITE);
}

uint64_t handle_mbox_read(struct task_struct *tsk, uint64_t addr)
{
	uint64_t ret = 0;
	static uint64_t empty = MBOX_EMPTY;

	switch (addr) {
	case MBOX_STATUS:
		/*
                 * TODO
                 * This emulation is currently not accuracy.
                 *   uboot will drain the the stale responses.
                 *   check: https://elixir.bootlin.com/u-boot/v2022.10/source/arch/arm/mach-bcm283x/mbox.c#L31
                 */
		ret = empty;

		if (empty & MBOX_EMPTY)
			empty = 0;
		else
			empty = MBOX_EMPTY;

		break;
	case MBOX_READ:
		ret = emulate_mbox_read(tsk);
		break;
	default:
		WARN("Unsupported MBOX Read\n");
	}
	return ret;
}

uint64_t handle_mbox_write(struct task_struct *tsk, uint64_t addr, uint64_t val)
{
	uint64_t ret = 0;
	switch (addr) {
	case MBOX_WRITE:
		ret = emulate_mbox_write(tsk, val);
		break;
	default:
		WARN("Unsupported MBOX Write\n");
	}
	return ret;
}
