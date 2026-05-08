// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "common/mm.h"
#include "arch/aarch64/sysregs.h"
#include "arch/aarch64/mmu.h"
#include "boards/raspi/base.h"
#include "boards/raspi/raspi3b.h"
#include "common/board.h"
#include "common/debug.h"
#include "common/printf.h"
#include "common/smp.h"
#include "common/vcpu.h"
#include "common/utils.h"

#ifdef AVISOR_GUEST_PATCH_DEBUG
/* Optional debug guard to catch accidental reuse of a "special" backing page. */
volatile paddr_t avisor_debug_dtb_page_pa = 0;
volatile vaddr_t avisor_debug_dtb_page_ipa = 0;
volatile int avisor_debug_dtb_guard_enabled = 0;
#endif

paddr_t get_free_page(struct page_pool *pool);
void free_page(struct page_pool *pool, paddr_t p);

void *allocate_page()
{
	paddr_t page = get_free_page(get_rasp3b_page_pool());

	if (page == 0) {
		return 0;
	}

	return (void *)TO_VADDR(page);
}

void deallocate_page(void *page)
{
	free_page(get_rasp3b_page_pool(), TO_PADDR(page));
}

void *allocate_vcpu_page(struct avisor_vm *vm, vaddr_t va)
{
	paddr_t page = get_free_page(get_rasp3b_page_pool());

	if (page == 0) {
		return 0;
	}

	map_stage2_page(vm, va, page, MMU_STAGE2_PAGE_FLAGS);
	return (void *)TO_VADDR(page);
}

void set_vcpu_page_notaccessable(struct avisor_vm *vm, vaddr_t va)
{
	map_stage2_page(vm, va, 0, MMU_STAGE2_MMIO_PAGE_FLAGS);
}

paddr_t get_free_page(struct page_pool *pool)
{
	uint64_t i, index;
	paddr_t page;

	spin_lock(&pool->lock);
	for (i = pool->last_page_id; i < (pool->last_page_id + pool->page_nr);
	     i++) {
		index = i % pool->page_nr;
		if (pool->memap[index] == 0) {
			pool->memap[index] = 1;
			page = pool->start_addr + index * PAGE_SIZE;
#ifdef AVISOR_GUEST_PATCH_DEBUG
			if (avisor_debug_dtb_guard_enabled &&
			    avisor_debug_dtb_page_pa &&
			    ((page & PAGE_MASK) == (avisor_debug_dtb_page_pa & PAGE_MASK))) {
				spin_unlock(&pool->lock);
				PANIC("DTB backing page was allocated again! page=0x%lx\n", (unsigned long)page);
			}
#endif
			memzero((void *)TO_VADDR(page), PAGE_SIZE);
			pool->last_page_id = index;
			spin_unlock(&pool->lock);
			return page;
		}
	}

	spin_unlock(&pool->lock);
	PANIC("no free pages!\n");
	return 0;
}

void free_page(struct page_pool *pool, paddr_t p)
{
	spin_lock(&pool->lock);
	pool->memap[(p - pool->start_addr) / PAGE_SIZE] = 0;
	spin_unlock(&pool->lock);
}

void map_stage2_table_entry(vaddr_t pte, vaddr_t va, paddr_t pa, uint64_t flags)
{
	uint64_t index = va >> PAGE_SHIFT;

	index = index & (PTRS_PER_TABLE - 1);
	uint64_t entry = pa | flags;

	((uint64_t *)pte)[index] = entry;
}

static paddr_t map_stage2_table(vaddr_t table, uint64_t shift, vaddr_t va,
			 int *new_table)
{
	uint64_t index = va >> shift;

	index = index & (PTRS_PER_TABLE - 1);

	if (!((uint64_t *)table)[index]) {
		*new_table = 1;
		paddr_t next_level_table =
			get_free_page(get_rasp3b_page_pool());
		uint64_t entry = next_level_table | MM_TYPE_PAGE_TABLE;

		((uint64_t *)table)[index] = entry;
		return next_level_table;
	} else {
		*new_table = 0;
	}

	return ((uint64_t *)table)[index] & PAGE_MASK;
}

bool check_vcpu_page_mapped(struct avisor_vm *vm, vaddr_t va)
{
	paddr_t lv1_table;

	if (!vm->mm.first_table) {
		vm->mm.first_table = get_free_page(get_rasp3b_page_pool());
		vm->mm.kernel_pages_count++;
	}

	lv1_table = vm->mm.first_table;
	int new_table;
	paddr_t lv2_table = map_stage2_table(TO_VADDR(lv1_table), LV1_SHIFT, va,
					     &new_table);

	if (new_table) {
		vm->mm.kernel_pages_count++;
		return false;
	}

	map_stage2_table(TO_VADDR(lv2_table), LV2_SHIFT, va, &new_table);

	if (new_table) {
		vm->mm.kernel_pages_count++;
		return false;
	}

	return true;
}

void map_stage2_page(struct avisor_vm *vm, vaddr_t va, paddr_t page, uint64_t flags)
{
	paddr_t lv1_table;

	if (!vm->mm.first_table) {
		vm->mm.first_table = get_free_page(get_rasp3b_page_pool());
		vm->mm.kernel_pages_count++;
	}

	lv1_table = vm->mm.first_table;
	int new_table;
	paddr_t lv2_table = map_stage2_table(TO_VADDR(lv1_table), LV1_SHIFT, va,
					     &new_table);

	if (new_table) {
		vm->mm.kernel_pages_count++;
	}

	paddr_t lv3_table = map_stage2_table(TO_VADDR(lv2_table), LV2_SHIFT, va,
					     &new_table);

	if (new_table) {
		vm->mm.kernel_pages_count++;
	}

	map_stage2_table_entry(TO_VADDR(lv3_table), va, page, flags);
	vm->mm.user_pages_count++;
}

paddr_t get_ipa(vaddr_t va)
{
	paddr_t ipa = translate_el1(va);

	ipa &= 0xFFFFFFFFF000;
	ipa |= va & 0xFFF;
	return ipa;
}

#define ISS_ABORT_DFSC_MASK 0x3f

int handle_mem_abort(vaddr_t addr, uint64_t esr)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();
	struct pt_regs *regs = vcpu_pt_regs(cpu_data->current);
	uint64_t dfsc = esr & ISS_ABORT_DFSC_MASK;

	/*
	 * Use HPFAR_EL2 for the faulting IPA - more reliable than
	 * doing an AT S1E1R translation which can give stale results.
	 */
	unsigned long hpfar = READ_SYSREG64(hpfar_el2);
	unsigned long ipa = (hpfar << 8) | (addr & 0xFFF);

	if (dfsc >> 2 == 0x1) {
		paddr_t page = get_free_page(get_rasp3b_page_pool());

		if (page == 0) {
			return -1;
		}

		map_stage2_page(cpu_data->current->vm, ipa & PAGE_MASK, page,
				MMU_STAGE2_PAGE_FLAGS);
		asm volatile("dsb ish; isb" ::: "memory");
		cpu_data->current->stat.pf_count++;
		return 0;
	} else if (dfsc >> 2 == 0x3) {

		if (ipa < DEVICE_BASE) {
			paddr_t page = get_free_page(get_rasp3b_page_pool());
			if (page == 0)
				return -1;
			map_stage2_page(cpu_data->current->vm, ipa & PAGE_MASK,
					page, MMU_STAGE2_PAGE_FLAGS);
			asm volatile("dsb ish; isb" ::: "memory");
			cpu_data->current->stat.pf_count++;
			return 0;
		}

		const struct board_ops *ops = cpu_data->current->board_ops;

		unsigned int srt = (esr >> 16) & 0x1f;
		unsigned int wnr = (esr >> 6) & 0x1;

		if (wnr == 0) {
			if (HAVE_FUNC(ops, mmio_read))
				regs->regs[srt] =
					ops->mmio_read(cpu_data->current, ipa);
		} else {
			if (HAVE_FUNC(ops, mmio_write))
				ops->mmio_write(cpu_data->current, ipa,
						regs->regs[srt]);
		}

		increment_current_pc(4);
		cpu_data->current->stat.mmio_count++;
		return 0;
	}

	return -1;
}
