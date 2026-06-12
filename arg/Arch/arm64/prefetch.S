/* SPDX-License-Identifier: GPL-2.0 */
/* arg/arch/arm64/prefetch.S — Extended Prefetch Variants
 * L2 and non-temporal prefetch for large data structures.
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
	.text
	.align 2

/* void arg_arm64_pld_l2(const void *addr) — L2 prefetch for load */
ENTRY(arg_arm64_pld_l2)
	prfm	pldl2keep, [x0]
	ret
ENDPROC(arg_arm64_pld_l2)

/* void arg_arm64_pst_l2(const void *addr) — L2 prefetch for store */
ENTRY(arg_arm64_pst_l2)
	prfm	pstl2keep, [x0]
	ret
ENDPROC(arg_arm64_pst_l2)

/* void arg_arm64_pld_nt(const void *addr) — non-temporal (streaming) */
ENTRY(arg_arm64_pld_nt)
	prfm	pldl1strm, [x0]
	ret
ENDPROC(arg_arm64_pld_nt)
