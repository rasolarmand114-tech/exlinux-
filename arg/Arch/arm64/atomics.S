/* SPDX-License-Identifier: GPL-2.0 */
/* arg/arch/arm64/atomics.S — LSE Atomic Instructions
 * Cortex-A55 r2p0+ supports FEAT_LSE.
 * These are called from Rust only when has_lse(cpu) == true.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
	.text
	.align 2
	.arch_extension lse

/* u32 arg_arm64_lse_add(u32 *ptr, u32 val)  — atomic add, returns new value
 * x0=ptr  w1=val → w0=new */
ENTRY(arg_arm64_lse_add)
	ldaddal	w1, w2, [x0]
	add	w0, w2, w1
	ret
ENDPROC(arg_arm64_lse_add)

/* u32 arg_arm64_lse_cas(u32 *ptr, u32 old, u32 new) — CAS, returns actual old
 * x0=ptr  w1=expected  w2=desired → w0=actual_old */
ENTRY(arg_arm64_lse_cas)
	mov	w3, w1
	casal	w3, w2, [x0]
	mov	w0, w3
	ret
ENDPROC(arg_arm64_lse_cas)

/* u32 arg_arm64_lse_swap(u32 *ptr, u32 new) — atomic swap, returns old
 * x0=ptr  w1=new → w0=old */
ENTRY(arg_arm64_lse_swap)
	swpal	w1, w0, [x0]
	ret
ENDPROC(arg_arm64_lse_swap)
