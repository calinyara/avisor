// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include <inttypes.h>

#include "common/debug.h"
#include "common/loader.h"
#include "common/mm.h"
#include "common/sched.h"
#include "common/spinlock.h"
#include "common/utils.h"
#ifdef AVISOR_GUEST_PATCH_DEBUG
#include "arch/aarch64/mmu.h"
#endif
#include "fs/ff.h"

#ifdef AVISOR_GUEST_PATCH_DEBUG
/* Optional debug guard vars (defined in common/mm.c when DEBUG is enabled) */
extern volatile paddr_t avisor_debug_dtb_page_pa;
extern volatile int avisor_debug_dtb_guard_enabled;
#endif

static spinlock_t fs_lock = 0;

static bool is_dtb_filename(const char *name)
{
	/* Keep it simple: DTB is typically named "*.dtb". No libc here. */
	size_t len = strnlen(name, 256);
	if (len >= 4 && name[len - 4] == '.' &&
	    name[len - 3] == 'd' && name[len - 2] == 't' && name[len - 1] == 'b')
		return true;
	return false;
}

static uint32_t be32_to_cpu_u32(const uint8_t *p)
{
	return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) |
	       ((uint32_t)p[2] << 8) | ((uint32_t)p[3]);
}

#ifdef AVISOR_GUEST_PATCH_DEBUG
static bool stage2_translate_ipa_to_pa(struct avisor_vm *vm, vaddr_t ipa, paddr_t *out_pa)
{
	if (!vm || !vm->mm.first_table)
		return false;

	paddr_t lv1 = vm->mm.first_table;
	uint64_t *t1 = (uint64_t *)TO_VADDR(lv1);
	uint64_t idx1 = (ipa >> (LV1_SHIFT)) & (PTRS_PER_TABLE - 1);
	uint64_t e1 = t1[idx1];
	if ((e1 & 0x3) == 0)
		return false;

	paddr_t lv2 = (paddr_t)(e1 & PAGE_MASK);
	uint64_t *t2 = (uint64_t *)TO_VADDR(lv2);
	uint64_t idx2 = (ipa >> (LV2_SHIFT)) & (PTRS_PER_TABLE - 1);
	uint64_t e2 = t2[idx2];
	if ((e2 & 0x3) == 0)
		return false;

	paddr_t lv3 = (paddr_t)(e2 & PAGE_MASK);
	uint64_t *t3 = (uint64_t *)TO_VADDR(lv3);
	uint64_t idx3 = (ipa >> PAGE_SHIFT) & (PTRS_PER_TABLE - 1);
	uint64_t e3 = t3[idx3];
	if ((e3 & 0x3) == 0)
		return false;

	paddr_t page = (paddr_t)(e3 & PAGE_MASK);
	*out_pa = page | (ipa & ~PAGE_MASK);
	return true;
}

#define LINUX_FDT_POINTER_OFFSET_FROM_IMAGE_BASE 0x201398UL
#define LINUX_FIXMAP_REMAP_FDT_SCAN_START_OFFSET_FROM_IMAGE_BASE 0x001e4000UL
#define LINUX_FIXMAP_REMAP_FDT_SCAN_LEN 0x00004000UL /* 16KB window */
#define LINUX_FIXMAP_REMAP_FDT_LDR_MAGIC_INSN 0xb8756b21u
#define HVC_DEBUG_EMU_LDR_MAGIC_IMM 0xBEEF
#else
/* In non-debug builds we do not do any runtime patching of the guest kernel. */
#endif

/*
 * Linux/arm64 `setup_arch()` loads DTB physical address from `__fdt_pointer`.
 * In this kernel build, `misc/vmlinux.s` shows:
 *   __fdt_pointer @ 0xffffffc008201398
 * And the kernel linear mapping offset is:
 *   virt = phys + 0xffffffc000000000
 * Therefore, for an Image loaded at physical base 0x08000000, the physical
 * address of `__fdt_pointer` is 0x08201398, i.e. offset 0x201398 from base.
 *
 * If the early boot code fails to populate `__fdt_pointer` (e.g. due to boot
 * protocol mismatch), Linux will pass x0=0 to fixmap_remap_fdt() and hang in
 * the yield loop you observed at 0xffffffc0081e2da4.
 *
 * This is a pragmatic bring-up workaround to force the correct DTB address.
 */

// va should be page-aligned.
int load_file_to_memory(struct avisor_vcpu *vcpu, const char *name,
			unsigned long va)
{
	unsigned long gva = va & PAGE_MASK;
	unsigned long first_gva = gva;
	uint8_t *buf;
	FRESULT r;
	UINT br;
	FIL f;

	spin_lock(&fs_lock);
	r = f_open(&f, name, FA_READ);
	spin_unlock(&fs_lock);

	if (r) {
		PANIC("Can't open the file: %s, err=%d\n", name, r);
		return -r;
	}

	for (;;) {
		buf = allocate_vcpu_page(vcpu->vm, gva);

		spin_lock(&fs_lock);
		r = f_read(&f, buf, PAGE_SIZE, &br);
		spin_unlock(&fs_lock);

		if (br == 0) { /* error or eof */
			/*
			 * Correctness fix: we already mapped this page into stage-2 via
			 * allocate_vcpu_page(). If we free it back to the pool, we MUST
			 * also remove the stage-2 mapping, otherwise the guest retains a
			 * dangling mapping to a page that can be reallocated + memzero()'d later.
			 */
			set_vcpu_page_notaccessable(vcpu->vm, gva);
			deallocate_page(buf);
			break;
		}

		/*
		 * Non-invasive DTB sanity (no guest patching):
		 * Log DTB header from the *first* page as we load it.
		 */
		if (gva == first_gva && is_dtb_filename(name) && br >= 8) {
			uint32_t magic = be32_to_cpu_u32(&buf[0]);
			uint32_t totalsize = be32_to_cpu_u32(&buf[4]);
			paddr_t buf_pa = TO_PADDR((vaddr_t)buf);
			INFO("DTB header check: file=%s ipa=0x%lx pa=0x%lx magic=0x%08x totalsize=%u",
			     name, first_gva, (unsigned long)buf_pa, magic, totalsize);
		}

		/*
		 * Correct fix: ensure data written by EL2 is visible to the guest (EL1).
		 * We just filled this guest page via the EL2 mapping. Clean+invalidate to PoC
		 * to avoid alias/stale-cache issues when EL1 remaps the same PA very early.
		 */
		dcache_clean_invalidate_range((unsigned long)buf, PAGE_SIZE);

#ifdef AVISOR_GUEST_PATCH_DEBUG
		/* Extra debug: if this is the DTB's first page, dump what sits at its PA right now. */
		if (gva == first_gva && is_dtb_filename(name)) {
			paddr_t pa = TO_PADDR((vaddr_t)buf);
			volatile uint8_t *p = (volatile uint8_t *)TO_VADDR(pa);
			/* Force a real memory read (not a hot cache line) */
			dcache_invalidate_range((unsigned long)p, 64);
			INFO("DTB first-page PA dump: pa=0x%lx %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x",
			     (unsigned long)pa,
			     p[0], p[1], p[2], p[3], p[4], p[5], p[6], p[7],
			     p[8], p[9], p[10], p[11], p[12], p[13], p[14], p[15]);

			/* Enable allocator guard for the DTB first page PA (diagnostic). */
			avisor_debug_dtb_page_pa = pa & PAGE_MASK;
			avisor_debug_dtb_guard_enabled = 1;
		}
#endif

		gva += PAGE_SIZE;
	}

	spin_lock(&fs_lock);
	f_close(&f);
	spin_unlock(&fs_lock);

	INFO("file: %s loaded", name);

	return -r;
}

int raw_binary_loader(void *arg, struct pt_regs *regs)
{
	struct vm_config *config_args = arg;
	per_cpu_data_t *cpu_data = get_cpu_data(config_args->core_id);
	/*
	 * Place DTB high in RAM to avoid being clobbered by Linux early allocations
	 * before it successfully parses/reserves the FDT region.
	 *
	 * RPi3 memory size from mailbox emulation is 0x3c000000 (960MB), so keep DTB
	 * below that and 2MB-aligned (fixmap_remap_fdt maps 2MB chunks).
	 */
	unsigned long dtb_addr = 0x3b000000;     /* 944MB for DTB (2MB-aligned) */
	unsigned long initrd_addr = 0x2200000;   /* 34MB for initramfs */
	unsigned long initrd_size = 0;
	unsigned long boot_sp = config_args->sp;

	INFO("=== Loading Linux kernel and components ===\n");

	/* Load kernel */
	INFO("Loading kernel Image to 0x%lx", config_args->load_addr);
	if (load_file_to_memory(cpu_data->current, config_args->filename,
				config_args->load_addr) < 0) {
		PANIC("Failed to load kernel Image");
		return -1;
	}

#ifdef AVISOR_GUEST_PATCH_DEBUG
	/* Debug: scan+patch the real magic-load instruction to HVC. */
	{
		uint32_t hvc = 0xD4000002u | ((uint32_t)HVC_DEBUG_EMU_LDR_MAGIC_IMM << 5);
		unsigned long start = config_args->load_addr +
				      LINUX_FIXMAP_REMAP_FDT_SCAN_START_OFFSET_FROM_IMAGE_BASE;
		unsigned long end = start + LINUX_FIXMAP_REMAP_FDT_SCAN_LEN;
		unsigned long found_ipa = 0;

		for (unsigned long ipa = start; ipa < end; ipa += 4) {
			paddr_t host_pa;
			if (!stage2_translate_ipa_to_pa(cpu_data->current->vm, (vaddr_t)ipa, &host_pa))
				continue;
			uint32_t insn = *(volatile uint32_t *)TO_VADDR(host_pa);
			if (insn == LINUX_FIXMAP_REMAP_FDT_LDR_MAGIC_INSN) {
				found_ipa = ipa;
				break;
			}
		}

		if (!found_ipa) {
			WARN("patched ldr-magic: scan failed in [0x%lx, 0x%lx)", start, end);
		} else {
			paddr_t host_pa;
			if (!stage2_translate_ipa_to_pa(cpu_data->current->vm, (vaddr_t)found_ipa, &host_pa)) {
				WARN("patched ldr-magic: stage2 translate failed: ipa=0x%lx", found_ipa);
			} else {
				volatile uint32_t *p = (volatile uint32_t *)TO_VADDR(host_pa);
				uint32_t old = *p;
				*p = hvc;
				uint32_t verify = *p;
				INFO("patched ldr-magic: ipa=0x%lx pa=0x%lx old=0x%08x new=0x%08x verify=0x%08x",
				     found_ipa, (unsigned long)host_pa, old, hvc, verify);
			}
		}
	}

	/*
	 * Debug: patch the known hang PC (from `vml` saved-pc) to an HVC so we can
	 * dump state at 0x81e2da4 and step over the infinite loop.
	 */
	{
		unsigned long hang_ipa = 0x81e2da4UL;
		uint32_t nop = 0xD503201Fu; /* nop */
		paddr_t host_pa;

		if (!stage2_translate_ipa_to_pa(cpu_data->current->vm, (vaddr_t)hang_ipa, &host_pa)) {
			WARN("patched hang-pc: stage2 translate failed: ipa=0x%lx", hang_ipa);
		} else {
			volatile uint32_t *p = (volatile uint32_t *)TO_VADDR(host_pa);
			uint32_t old = *p;
			*p = nop;
			uint32_t verify = *p;
			INFO("patched hang-pc: ipa=0x%lx pa=0x%lx old=0x%08x new=0x%08x verify=0x%08x",
			     hang_ipa, (unsigned long)host_pa, old, nop, verify);
		}

		/*
		 * Also patch the very next instruction. Your trace shows it is `0x17ffffff`
		 * (b .-4) which immediately branches back to hang_ipa, creating a tight loop.
		 */
		if (!stage2_translate_ipa_to_pa(cpu_data->current->vm, (vaddr_t)(hang_ipa + 4), &host_pa)) {
			WARN("patched hang-loop: stage2 translate failed: ipa=0x%lx", hang_ipa + 4);
		} else {
			volatile uint32_t *p = (volatile uint32_t *)TO_VADDR(host_pa);
			uint32_t old = *p;
			*p = nop;
			uint32_t verify = *p;
			INFO("patched hang-loop: ipa=0x%lx pa=0x%lx old=0x%08x new=0x%08x verify=0x%08x",
			     hang_ipa + 4, (unsigned long)host_pa, old, nop, verify);
		}
	}
#endif

	/* Load DTB */
	INFO("Loading DTB to 0x%lx", dtb_addr);
	if (load_file_to_memory(cpu_data->current, "rasp3b.dtb", dtb_addr) < 0) {
		WARN("DTB rasp3b.dtb not found, trying bcm2710-rpi-3-b.dtb");
		if (load_file_to_memory(cpu_data->current, "bcm2710-rpi-3-b.dtb", dtb_addr) < 0) {
			PANIC("Failed to load DTB");
			return -1;
		}
	}

	/* Load initramfs - rootfs.cpio.gz is ~3.1MB */
	INFO("Loading initramfs rootfs.cpio.gz to 0x%lx", initrd_addr);
	if (load_file_to_memory(cpu_data->current, "rootfs.gz", initrd_addr) < 0) {
		WARN("Failed to load rootfs.cpio.gz - kernel will fail without rootfs!");
	} else {
		/* Estimate initrd size - actual size is ~3.1MB, round up to 4MB */
		initrd_size = 0x400000; /* 4MB */
		INFO("Initramfs loaded: start=0x%lx, end=0x%lx (size ~4MB)", 
		     initrd_addr, initrd_addr + initrd_size);
	}

	/* Set boot registers according to Linux/arm64 boot protocol:
	 *   x0 = physical address of FDT blob
	 *   x1/x2/x3 = 0 (reserved)
	 */
	regs->pc = config_args->entry_point;
	/*
	 * Provide a safe non-zero initial stack. Linux will switch to its own
	 * stacks very early, but entering EL1 with SP=0 is unsafe.
	 */
	if (boot_sp == 0)
		boot_sp = 0x80000;
	regs->sp = boot_sp;
	regs->sp_el0 = boot_sp;
	regs->sp_el1 = boot_sp;
	/*
	 * ARM64 Linux boot protocol (non-EFI): x0 = FDT physical address, x1-x3 = 0.
	 * Keep it strict for correctness; if a kernel needs a different convention,
	 * handle it explicitly (don't guess).
	 */
	regs->regs[0] = dtb_addr;       /* x0 = DTB physical address */
	regs->regs[1] = 0;              /* x1 = 0 (reserved) */
	regs->regs[2] = 0;              /* x2 = 0 (reserved) */
	regs->regs[3] = 0;              /* x3 = 0 (reserved) */

	INFO("Boot configuration:");
	INFO("  Kernel:    PC = 0x%lx", regs->pc);
	INFO("  Stack:     SP = 0x%lx", boot_sp);
	INFO("  DTB:       x0 = 0x%lx x1 = 0x%lx", dtb_addr, regs->regs[1]);
	INFO("  Initramfs:     0x%lx - 0x%lx", initrd_addr, initrd_addr + initrd_size);
	INFO("==========================================");

	return 0;
}
