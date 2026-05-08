// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * aVisor Hypervisor
 *
 * A Tiny Hypervisor for IoT Development
 *
 * Copyright (c) 2023 Deng Jie (mr.dengjie@gmail.com).
 */

#include "common/sync_exc.h"
#include "arch/aarch64/sysregs.h"
#include "arch/aarch64/mmu.h"
#include "arch/aarch64/page.h"
#include "common/debug.h"
#include "common/irq.h"
#include "common/mm.h"
#include "common/sched.h"
#include "common/smp.h"
#include "common/vcpu.h"
#include "common/printf.h"
#include "common/utils.h"
#include "boards/raspi/raspi3b.h"

#ifdef AVISOR_GUEST_PATCH_DEBUG
extern volatile paddr_t avisor_debug_dtb_page_pa;
extern volatile vaddr_t avisor_debug_dtb_page_ipa;
extern volatile int avisor_debug_dtb_guard_enabled;
#endif

const char *sync_error_reasons[] = {
	"Unknown reason.",
	"Trapped WFI or WFE instruction execution.",
	"(unknown)",
	"Trapped MCR or MRC access with (coproc==0b1111).",
	"Trapped MCRR or MRRC access with (coproc==0b1111).",
	"Trapped MCR or MRC access with (coproc==0b1110).",
	"Trapped LDC or STC access.",
	"Access to SVE, Advanced SIMD, or floating-point functionality trapped by CPACR_EL1.FPEN, CPTR_EL2.FPEN, CPTR_EL2.TFP, or CPTR_EL3.TFP control.",
	"Trapped VMRS access, from ID group trap.",
	"Trapped use of a Pointer authentication instruction because HCR_EL2.API == 0 || SCR_EL3.API == 0.",
	"(unknown)",
	"(unknown)",
	"Trapped MRRC access with (coproc==0b1110).",
	"Branch Target Exception.",
	"Illegal Execution state.",
	"(unknown)",
	"(unknown)",
	"SVC instruction execution in AArch32 state.",
	"HVC instruction execution in AArch32 state.",
	"SMC instruction execution in AArch32 state.",
	"(unknown)",
	"SVC instruction execution in AArch64 state.",
	"HVC instruction execution in AArch64 state.",
	"SMC instruction execution in AArch64 state.",
	"Trapped MSR, MRS or System instruction execution in AArch64 state.",
	"Access to SVE functionality trapped as a result of CPACR_EL1.ZEN, CPTR_EL2.ZEN, CPTR_EL2.TZ, or CPTR_EL3.EZ.",
	"Trapped ERET, ERETAA, or ERETAB instruction execution.",
	"(unknown)",
	"Exception from a Pointer Authentication instruction authentication failure.",
	"(unknown)",
	"(unknown)",
	"(unknown)",
	"Instruction Abort from a lower Exception level.",
	"Instruction Abort taken without a change in Exception level.",
	"PC alignment fault exception.",
	"(unknown)",
	"Data Abort from a lower Exception level.",
	"Data Abort without a change in Exception level, or Data Aborts taken to EL2 as a result of accesses generated associated with VNCR_EL2 as part of nested virtualization support.",
	"SP alignment fault exception.",
	"(unknown)",
	"Trapped floating-point exception taken from AArch32 state.",
	"(unknown)",
	"(unknown)",
	"(unknown)",
	"Trapped floating-point exception taken from AArch64 state.",
	"(unknown)",
	"(unknown)",
	"SError interrupt.",
	"Breakpoint exception from a lower Exception level.",
	"Breakpoint exception taken without a change in Exception level.",
	"Software Step exception from a lower Exception level.",
	"Software Step exception taken without a change in Exception level.",
	"Watchpoint from a lower Exception level.",
	"Watchpoint exceptions without a change in Exception level, or Watchpoint exceptions taken to EL2 as a result of accesses generated associated with VNCR_EL2 as part of nested virtualization support.",
	"(unknown)",
	"(unknown)",
	"BKPT instruction execution in AArch32 state.",
	"(unknown)",
	"Vector Catch exception from AArch32 state.",
	"(unknown)",
	"BRK instruction execution in AArch64 state.",
};

void handle_trap_wfx()
{
	schedule();
	increment_current_pc(4);
}

/* PSCI function IDs (SMC32 convention) */
#define PSCI_VERSION           0x84000000
#define PSCI_CPU_SUSPEND       0x84000001
#define PSCI_CPU_OFF           0x84000002
#define PSCI_CPU_ON            0x84000003
#define PSCI_AFFINITY_INFO     0x84000004
/* SMC64 variants (bit 30 set) - used by aarch64 Linux */
#define PSCI_CPU_SUSPEND_64    0xC4000001
#define PSCI_CPU_ON_64         0xC4000003
#define PSCI_AFFINITY_INFO_64  0xC4000004

/* PSCI return codes */
#define PSCI_SUCCESS           0
#define PSCI_NOT_SUPPORTED     -1
#define PSCI_INVALID_PARAMS   -2
#define PSCI_ALREADY_ON        -3
#define PSCI_ON_PENDING        -4
#define PSCI_INTERNAL_FAILURE  -5
#define PSCI_NOT_PRESENT       -6
#define PSCI_DISABLED          -7

static int handle_psci_cpu_on(struct avisor_vcpu *vcpu, unsigned long target_cpu,
			       unsigned long entry_point, unsigned long context_id)
{
	struct avisor_vm *vm = vcpu->vm;
	uint64_t vcpu_id;
	struct avisor_vcpu *target_vcpu;
	struct pt_regs *regs;

	/* Extract vcpu_id from target_cpu (MPIDR Aff0 field) */
	vcpu_id = target_cpu & 0xff;

	/* Validate vcpu_id */
	if (vcpu_id >= MAX_VCPUS_PER_VM || vcpu_id == 0) {
		return PSCI_INVALID_PARAMS;
	}

	target_vcpu = vm->hw.vcpu_array[vcpu_id];
	if (!target_vcpu) {
		return PSCI_INVALID_PARAMS;
	}

	/* Check if CPU is already on */
	if (target_vcpu->state == VCPU_RUNNING) {
		return PSCI_ALREADY_ON;
	}

	/* Start the secondary VCPU */
	target_vcpu->state = VCPU_RUNNING;
	/* Ensure counter is set so the VCPU can be scheduled */
	if (target_vcpu->counter <= 0) {
		target_vcpu->counter = target_vcpu->priority;
		if (target_vcpu->counter <= 0) {
			target_vcpu->counter = 1; /* Minimum counter value */
		}
	}

	/* Set up the entry point and context */
	regs = vcpu_pt_regs(target_vcpu);
	regs->pc = entry_point;
	regs->regs[0] = context_id;
	regs->pstate = PSR_MODE_EL1h;
	regs->pstate |= (0xf << 6);

	/*
	 * Wake up parked secondary physical cores that might be waiting in WFE.
	 * Ensure the entry point/context are visible before we signal.
	 */
	asm volatile("dsb ishst; sev" ::: "memory");

	return PSCI_SUCCESS;
}

static int handle_psci_affinity_info(struct avisor_vcpu *vcpu, unsigned long target_affinity,
				     unsigned long lowest_affinity_level)
{
	struct avisor_vm *vm = vcpu->vm;
	uint64_t vcpu_id;
	struct avisor_vcpu *target_vcpu;

	/* Extract vcpu_id from target_affinity (MPIDR Aff0 field) */
	vcpu_id = target_affinity & 0xff;

	/* Validate vcpu_id */
	if (vcpu_id >= MAX_VCPUS_PER_VM) {
		return PSCI_INVALID_PARAMS;
	}

	if (vcpu_id == 0) {
		/* Primary CPU is always on */
		return 0; /* ON */
	}

	target_vcpu = vm->hw.vcpu_array[vcpu_id];
	if (!target_vcpu) {
		return PSCI_NOT_PRESENT;
	}

	/* Return CPU state: 0 = ON, 1 = OFF, 2 = ON_PENDING */
	if (target_vcpu->state == VCPU_RUNNING) {
		return 0; /* ON */
	} else if (target_vcpu->state == VCPU_STOPPED) {
		return 1; /* OFF */
	} else {
		return 1; /* OFF */
	}
}

/*
 * Return from an HVC / SMC handler.
 *
 * For HVC and SMC exceptions taken from AArch64, the ARM architecture
 * sets ELR_EL2 to the instruction AFTER the HVC/SMC (the preferred
 * return address).  So we must NOT advance the PC — it is already
 * correct in regs->pc.
 */
static void psci_return(struct pt_regs *regs)
{
	(void)regs;
}

static void handle_system_call(unsigned long call_nr, int is_smc)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();
	struct avisor_vcpu *vcpu = cpu_data->current;
	struct pt_regs *regs = vcpu_pt_regs(vcpu);
	unsigned long function_id = regs->regs[0];
	unsigned long arg1 = regs->regs[1];
	unsigned long arg2 = regs->regs[2];
	unsigned long arg3 = regs->regs[3];
	int ret;
	const char *call_type = is_smc ? "SMC" : "HVC";

	/* Debug breakpoint: emulate `ldr w1, [x25, x21]` in Linux fixmap_remap_fdt(). */
#ifdef AVISOR_GUEST_PATCH_DEBUG
	if (!is_smc && call_nr == 0xBEEF) {
		unsigned long x0 = regs->regs[0];
		unsigned long x21 = regs->regs[21];
		unsigned long x25 = regs->regs[25];
		unsigned long gva = x21 + x25;

		uint64_t sctlr = READ_SYSREG64(sctlr_el1);
		uint64_t ttbr1 = READ_SYSREG64(ttbr1_el1);
		uint64_t tcr = READ_SYSREG64(tcr_el1);

		/* Stage-1 only: what IPA does Linux think this VA maps to? */
		uint64_t par_s1 = translate_el1((unsigned long)gva);

		paddr_t maddr = 0;
		uint64_t par_s12 = gvirt_to_maddr((vaddr_t)gva, &maddr, GV2M_READ);

		if (par_s12 != 0) {
			printf("HVC#BEEF: emu ldr failed: x0=0x%lx x21=0x%lx x25=0x%lx gva=0x%lx par_s1=0x%lx par_s12=0x%lx sctlr_el1=0x%lx ttbr1_el1=0x%lx tcr_el1=0x%lx\n",
			       x0, x21, x25, gva, par_s1, par_s12, sctlr, ttbr1, tcr);
			regs->regs[1] = 0;
		} else {
			/*
			 * NOTE: gvirt_to_maddr() returns a *machine/physical* address (PA),
			 * not a pointer in EL2 VA space. Convert via TO_VADDR() before deref.
			 */
			volatile uint8_t *b = (volatile uint8_t *)TO_VADDR(maddr);
			/* Ensure we are not reading stale EL2 cache lines. */
			dcache_invalidate_range((unsigned long)b, 64);
			uint32_t w1 = *(volatile uint32_t *)b;
			printf("HVC#BEEF: dump @maddr: %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x  %02x %02x %02x %02x\n",
			       b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7],
			       b[8], b[9], b[10], b[11], b[12], b[13], b[14], b[15]);
			if ((par_s1 & 0x1) == 0) {
				uint64_t ipa = (par_s1 & PADDR_MASK & PAGE_MASK) | ((uint64_t)gva & ~PAGE_MASK);
				printf("HVC#BEEF: stage1 ipa=0x%lx (expect DTB IPA)\n", ipa);
			}
			printf("HVC#BEEF: emu ldr ok: x0=0x%lx x21=0x%lx x25=0x%lx gva=0x%lx maddr=0x%lx val=0x%08x sctlr_el1=0x%lx ttbr1_el1=0x%lx tcr_el1=0x%lx\n",
			       x0, x21, x25, gva, (unsigned long)maddr, w1, sctlr, ttbr1, tcr);
			regs->regs[1] = (uint64_t)w1;
		}

		/* HVC: ELR_EL2 already past the HVC, no PC bump needed */
		return;
	}

	/* Debug trap for the "hang pc" we patch at 0x81e2da4 */
	if (!is_smc && call_nr == 0xDEAD) {
		static unsigned int dead_seen;
		unsigned long elr = (unsigned long)READ_SYSREG64(elr_el2);
		unsigned long gva = elr; /* kernel VA */

		uint64_t sctlr = READ_SYSREG64(sctlr_el1);
		uint64_t ttbr1 = READ_SYSREG64(ttbr1_el1);
		uint64_t tcr = READ_SYSREG64(tcr_el1);

		uint64_t par_s1 = translate_el1((unsigned long)gva);
		paddr_t maddr = 0;
		uint64_t par_s12 = gvirt_to_maddr((vaddr_t)gva, &maddr, GV2M_READ);

		/*
		 * Self-heal: if this PC is stuck in a tight loop (you observed `0x17ffffff` = b .-4),
		 * patch the current HVC and the following branch to NOP and perform cache maintenance
		 * so the guest actually fetches the new instructions even with I-cache enabled.
		 */
		if (par_s12 == 0) {
			volatile uint32_t *w = (volatile uint32_t *)maddr;
			uint32_t insn0 = w[0];
			uint32_t insn1 = w[1];
			uint32_t hvc_dead = 0xD4000002u | ((uint32_t)0xDEADu << 5);
			uint32_t nop = 0xD503201Fu;

			/* local cache sync helper (64B line) */
			auto void cache_sync_range(void *addr, unsigned long size) {
				unsigned long start = ((unsigned long)addr) & ~63UL;
				unsigned long end = (((unsigned long)addr) + size + 63UL) & ~63UL;
				for (unsigned long p = start; p < end; p += 64)
					__asm__ volatile("dc cvau, %0" :: "r"(p) : "memory");
				__asm__ volatile("dsb ish" ::: "memory");
				for (unsigned long p = start; p < end; p += 64)
					__asm__ volatile("ic ivau, %0" :: "r"(p) : "memory");
				__asm__ volatile("dsb ish; isb" ::: "memory");
			}

			/* Patch current insn if still HVC#DEAD */
			if (insn0 == hvc_dead) {
				w[0] = nop;
				insn0 = nop;
			}
			/* Patch the classic tight loop back-edge (b .-4) right after */
			if (insn1 == 0x17ffffffu) {
				w[1] = nop;
				insn1 = nop;
			}
			cache_sync_range((void *)w, 8);

			/* Print only a few times so the console remains usable (vml). */
			if (dead_seen < 5) {
				printf("HVC#DEAD: hang-pc hit: elr=0x%lx gva=0x%lx par_s1=0x%lx par_s12=0x%lx maddr=0x%lx sctlr_el1=0x%lx ttbr1_el1=0x%lx tcr_el1=0x%lx\n",
				       elr, gva, par_s1, par_s12, (unsigned long)maddr, sctlr, ttbr1, tcr);
				printf("HVC#DEAD: insn@pc: %08x %08x %08x %08x\n",
				       w[0], w[1], w[2], w[3]);
			}
		} else {
			if (dead_seen < 5) {
				printf("HVC#DEAD: hang-pc hit (no maddr): elr=0x%lx gva=0x%lx par_s1=0x%lx par_s12=0x%lx sctlr_el1=0x%lx ttbr1_el1=0x%lx tcr_el1=0x%lx\n",
				       elr, gva, par_s1, par_s12, sctlr, ttbr1, tcr);
			}
		}

		dead_seen++;
		/* HVC: ELR_EL2 already past the HVC, no PC bump needed */
		return;
	}
#endif



	/* ARM Architecture Service Calls (0x8000_0000 - 0x8000_FFFF) */
	if ((function_id & 0xFFFF0000) == 0x80000000) {
		if (function_id == 0x80000000) {
			regs->regs[0] = 0x10000; /* SMCCC v1.0 */
		} else {
			regs->regs[0] = -1; /* NOT_SUPPORTED */
		}
		psci_return(regs);
		return;
	}

	/* Check if this is a PSCI call (SMC32: 0x84xxxxxx or SMC64: 0xC4xxxxxx) */
	uint32_t fn_base = function_id & ~(1UL << 30);
	if ((fn_base & 0xfff00000) == 0x84000000) {
		switch (fn_base) {
		case PSCI_VERSION:
			regs->regs[0] = 0x00000002;
			psci_return(regs);
			return;

		case PSCI_CPU_ON:
			ret = handle_psci_cpu_on(vcpu, arg1, arg2, arg3);
			regs->regs[0] = ret;
			psci_return(regs);
			return;

		case PSCI_CPU_OFF:
			regs->regs[0] = PSCI_SUCCESS;
			psci_return(regs);
			return;

		case PSCI_AFFINITY_INFO:
			ret = handle_psci_affinity_info(vcpu, arg1, arg2);
			regs->regs[0] = ret;
			psci_return(regs);
			return;

		case PSCI_CPU_SUSPEND:
			regs->regs[0] = PSCI_NOT_SUPPORTED;
			psci_return(regs);
			return;

		default:
			regs->regs[0] = PSCI_NOT_SUPPORTED;
			psci_return(regs);
			return;
		}
	}

	WARN("Unhandled %s #%lu (function_id: 0x%lx)", call_type, call_nr, function_id);
	regs->regs[0] = -1;
	psci_return(regs);
}

void handle_trap_system(unsigned long esr)
{
	per_cpu_data_t *cpu_data = get_current_cpu_data();

/* msr(reg) */
#define DEFINE_SYSREG_MSR(name, _op1, _crn, _crm, _op2)				\
	do {									\
		if (op1 == (_op1) && crn == (_crn) && crm == (_crm) &&		\
		    op2 == (_op2)) {						\
			cpu_data->current->cpu_sysregs.name = regs->regs[rt];	\
			goto sys_fin;						\
		}								\
	} while (0)

/* mrs */
#define DEFINE_SYSREG_MRS(name, _op1, _crn, _crm, _op2)				\
	do {									\
		if (op1 == (_op1) && crn == (_crn) && crm == (_crm) &&		\
		    op2 == (_op2)) {						\
			regs->regs[rt] = cpu_data->current->cpu_sysregs.name;	\
			goto sys_fin;						\
		}								\
	} while (0)

	struct pt_regs *regs = vcpu_pt_regs(cpu_data->current);

	unsigned int op0 = (esr >> 20) & 0x3;
	unsigned int op2 = (esr >> 17) & 0x7;
	unsigned int op1 = (esr >> 14) & 0x7;
	unsigned int crn = (esr >> 10) & 0xf;
	unsigned int rt = (esr >> 5) & 0x1f;
	unsigned int crm = (esr >> 1) & 0xf;
	unsigned int dir = esr & 0x1;

	// INFO("trap_system: op0=%u,op2=%u,op1=%u,crn=%u,rt=%u,crm=%u,dir=%u",
	//	op0, op2, op1, crn, rt, crm, dir);

	if ((op0 & 2) && dir == 0) {
		/* MSR (write) operations */
		DEFINE_SYSREG_MSR(actlr_el1, 0, 1, 0, 1);
		DEFINE_SYSREG_MSR(csselr_el1, 1, 0, 0, 0);
	} else if ((op0 & 2) && dir == 1) {
		/* MRS (read) operations */
		DEFINE_SYSREG_MRS(actlr_el1, 0, 1, 0, 1);
		DEFINE_SYSREG_MRS(id_pfr0_el1, 0, 0, 1, 0);
		DEFINE_SYSREG_MRS(id_pfr1_el1, 0, 0, 1, 1);
		DEFINE_SYSREG_MRS(id_mmfr0_el1, 0, 0, 1, 4);
		DEFINE_SYSREG_MRS(id_mmfr1_el1, 0, 0, 1, 5);
		DEFINE_SYSREG_MRS(id_mmfr2_el1, 0, 0, 1, 6);
		DEFINE_SYSREG_MRS(id_mmfr3_el1, 0, 0, 1, 7);
		DEFINE_SYSREG_MRS(id_isar0_el1, 0, 0, 2, 0);
		DEFINE_SYSREG_MRS(id_isar1_el1, 0, 0, 2, 1);
		DEFINE_SYSREG_MRS(id_isar2_el1, 0, 0, 2, 2);
		DEFINE_SYSREG_MRS(id_isar3_el1, 0, 0, 2, 3);
		DEFINE_SYSREG_MRS(id_isar4_el1, 0, 0, 2, 4);
		DEFINE_SYSREG_MRS(id_isar5_el1, 0, 0, 2, 5);
		DEFINE_SYSREG_MRS(mvfr0_el1, 0, 0, 3, 0);
		DEFINE_SYSREG_MRS(mvfr1_el1, 0, 0, 3, 1);
		DEFINE_SYSREG_MRS(mvfr2_el1, 0, 0, 3, 2);
		DEFINE_SYSREG_MRS(id_aa64pfr0_el1, 0, 0, 4, 0);
		DEFINE_SYSREG_MRS(id_aa64pfr1_el1, 0, 0, 4, 1);
		DEFINE_SYSREG_MRS(id_aa64dfr0_el1, 0, 0, 5, 0);
		DEFINE_SYSREG_MRS(id_aa64dfr1_el1, 0, 0, 5, 1);
		DEFINE_SYSREG_MRS(id_aa64isar0_el1, 0, 0, 6, 0);
		DEFINE_SYSREG_MRS(id_aa64isar1_el1, 0, 0, 6, 1);
		DEFINE_SYSREG_MRS(id_aa64mmfr0_el1, 0, 0, 7, 0);
		DEFINE_SYSREG_MRS(id_aa64mmfr1_el1, 0, 0, 7, 1);
		DEFINE_SYSREG_MRS(id_aa64afr0_el1, 0, 0, 5, 4);
		DEFINE_SYSREG_MRS(id_aa64afr1_el1, 0, 0, 5, 5);
		DEFINE_SYSREG_MRS(ctr_el0, 3, 0, 0, 1);
		DEFINE_SYSREG_MRS(ccsidr_el1, 1, 0, 0, 0);
		DEFINE_SYSREG_MRS(clidr_el1, 1, 0, 0, 1);
		DEFINE_SYSREG_MRS(csselr_el1, 2, 0, 0, 0);
		DEFINE_SYSREG_MRS(aidr_el1, 1, 0, 0, 7);
		DEFINE_SYSREG_MRS(revidr_el1, 0, 0, 0, 6);
		/* Additional registers that are commonly accessed */
		/* mpidr_el1: op0=3, op1=0, crn=0, crm=5, op2=0 */
		DEFINE_SYSREG_MRS(mpidr_el1, 0, 0, 5, 0);
		/* midr_el1: op0=3, op1=0, crn=0, crm=4, op2=0 */
		DEFINE_SYSREG_MRS(midr_el1, 0, 0, 4, 0);
	}

	/* Handle op0=3 cases separately if not caught above */
	/* Note: Most op0=3 registers are already handled in the (op0 & 2) branch above */
	/* But we add explicit checks here as a fallback for op0=3 specific cases */
	if (op0 == 3 && dir == 1) {
		/* ctr_el0: op0=3, op1=3, crn=0, crm=0, op2=1 - already handled above */
		/* mpidr_el1: op0=3, op1=0, crn=0, crm=5, op2=0 - already handled above */
		/* midr_el1: op0=3, op1=0, crn=0, crm=4, op2=0 - already handled above */
		/* id_aa64mmfr0_el1: op0=3, op1=0, crn=0, crm=7, op2=0 - already handled above */
		/* If we reach here, it means the register wasn't matched above */
		/* This should not happen, but we keep this as a fallback */
	}

	if (dir == 1 && rt < 31) {
		regs->regs[rt] = 0;
	}
sys_fin:
	increment_current_pc(4);
	return;
}

#define ESR_EL2_EC_SHIFT 26

#define ESR_EL2_EC_TRAP_WFX    1
#define ESR_EL2_EC_TRAP_FP_REG 7
#define ESR_EL2_EC_HVC64       22
#define ESR_EL2_EC_SMC64       23
#define ESR_EL2_EC_TRAP_SYSTEM 24
#define ESR_EL2_EC_TRAP_SVE    25
#define ESR_EL2_EC_IABT_LOW    32
#define ESR_EL2_EC_DABT_LOW    36

void handle_sync_exception(void)
{
	unsigned long esr = READ_SYSREG64(esr_el2);
	unsigned long elr = READ_SYSREG64(elr_el2);
	unsigned long far = READ_SYSREG64(far_el2);
	int eclass = (esr >> ESR_EL2_EC_SHIFT) & 0x3f;
	unsigned long ec = esr & 0xffff; /* Extract HVC/SMC immediate from ESR bits [15:0] */
	per_cpu_data_t *cpu_data = get_current_cpu_data();
	static unsigned int dead_sync_prints;

	disable_irq();

	switch (eclass) {
	case ESR_EL2_EC_TRAP_WFX:
		cpu_data->current->stat.wfx_trap_count++;
		handle_trap_wfx();
		break;
	case ESR_EL2_EC_TRAP_FP_REG:
		WARN("TRAP_FP_REG is not implemented.");
		break;
	case ESR_EL2_EC_HVC64:
		cpu_data->current->stat.hvc_trap_count++;
		handle_system_call(ec, false);
		break;
	case ESR_EL2_EC_SMC64:
		/* SMC64 is treated the same as HVC64 for PSCI */
		cpu_data->current->stat.hvc_trap_count++;
		handle_system_call(ec, true);
		break;
	case ESR_EL2_EC_TRAP_SYSTEM:
		cpu_data->current->stat.sysreg_trap_count++;
		handle_trap_system(esr);
		break;
	case ESR_EL2_EC_TRAP_SVE:
		WARN("TRAP_SVE is not implemented.");
		break;
	case ESR_EL2_EC_IABT_LOW:
		if (handle_mem_abort(far, esr) < 0)
			PANIC("handle_mem_abort() (IABT) failed.");
		break;
	case ESR_EL2_EC_DABT_LOW:
		if (handle_mem_abort(far, esr) < 0)
			PANIC("handle_mem_abort() failed.");
		break;
	default:
		PANIC("uncaught synchronous exception:\n%s\nesr: %x, address: %x",
		      sync_error_reasons[eclass], esr, elr);
		break;
	}
}
