/* SPDX-License-Identifier: GPL-2.0 */
/* arg/arch/arm64/regs.S — System Register Reads (MRS)
 * Called from Rust cluster.rs for feature detection.
 * "Assembly executes." — each function is a pure MRS + ret.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
	.text
	.align 2

/* u64 arg_arm64_read_pmccntr(void) — CPU cycle counter */
ENTRY(arg_arm64_read_pmccntr)
	mrs	x0, pmccntr_el0
	ret
ENDPROC(arg_arm64_read_pmccntr)

/* u64 arg_arm64_read_mpidr(void) — Multiprocessor Affinity Register
 * Aff2[23:16]=cluster  Aff1[15:8]=core (big.LITTLE detection) */
ENTRY(arg_arm64_read_mpidr)
	mrs	x0, mpidr_el1
	ret
ENDPROC(arg_arm64_read_mpidr)

/* u64 arg_arm64_read_isar0(void) — ID_AA64ISAR0_EL1
 * Bits: AES[7:4] SHA1[11:8] SHA2[15:12] CRC32[19:16]
 *       Atomic/LSE[23:20]  PMULL[51:48] */
ENTRY(arg_arm64_read_isar0)
	mrs	x0, id_aa64isar0_el1
	ret
ENDPROC(arg_arm64_read_isar0)

/* u64 arg_arm64_read_pfr0(void) — ID_AA64PFR0_EL1
 * FP[19:16]=0xF means absent  SVE[35:32]>=1 means present */
ENTRY(arg_arm64_read_pfr0)
	mrs	x0, id_aa64pfr0_el1
	ret
ENDPROC(arg_arm64_read_pfr0)
