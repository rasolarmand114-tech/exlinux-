/* SPDX-License-Identifier: GPL-2.0 */
/* arg/arch/arm64/barriers.S — Memory Barrier Primitives
 * "Assembly executes. Never contains decision logic."
 * Target: AArch64 · Cortex-A55 (Exynos 850)
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
	.text
	.align 2

/* void arg_arm64_dmb_ish(void) — Data Memory Barrier, Inner Shareable */
ENTRY(arg_arm64_dmb_ish)
	dmb	ish
	ret
ENDPROC(arg_arm64_dmb_ish)

/* void arg_arm64_dsb_ish(void) — Data Synchronization Barrier, IS */
ENTRY(arg_arm64_dsb_ish)
	dsb	ish
	ret
ENDPROC(arg_arm64_dsb_ish)

/* void arg_arm64_isb(void) — Instruction Synchronization Barrier */
ENTRY(arg_arm64_isb)
	isb
	ret
ENDPROC(arg_arm64_isb)

/* void arg_arm64_dsb_sy(void) — Full system DSB (use sparingly) */
ENTRY(arg_arm64_dsb_sy)
	dsb	sy
	ret
ENDPROC(arg_arm64_dsb_sy)
