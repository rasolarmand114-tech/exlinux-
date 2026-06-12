/* SPDX-License-Identifier: GPL-2.0 */
/* arg/arch/arm64/cache.S — Cache Maintenance + TLB + Prefetch
 * Cortex-A55 cache line = 64 bytes
 */
#include <linux/linkage.h>
#include <asm/assembler.h>
	.text
	.align 2

/* void arg_arm64_prefetch_r(const void *addr) */
ENTRY(arg_arm64_prefetch_r)
	prfm	pldl1keep, [x0]
	ret
ENDPROC(arg_arm64_prefetch_r)

/* void arg_arm64_prefetch_w(const void *addr) */
ENTRY(arg_arm64_prefetch_w)
	prfm	pstl1keep, [x0]
	ret
ENDPROC(arg_arm64_prefetch_w)

/* void arg_arm64_dc_clean(void *start, size_t len)
 * Clean (write-back) D-cache range by VA to PoC.
 * x0=start  x1=len */
ENTRY(arg_arm64_dc_clean)
	cbz	x1, 2f
	add	x1, x0, x1
	bic	x0, x0, #63
1:	dc	cvac, x0
	add	x0, x0, #64
	cmp	x0, x1
	b.lo	1b
	dsb	ish
2:	ret
ENDPROC(arg_arm64_dc_clean)

/* void arg_arm64_dc_inval(void *start, size_t len)
 * Clean + Invalidate D-cache range (DC CIVAC). */
ENTRY(arg_arm64_dc_inval)
	cbz	x1, 2f
	add	x1, x0, x1
	bic	x0, x0, #63
1:	dc	civac, x0
	add	x0, x0, #64
	cmp	x0, x1
	b.lo	1b
	dsb	ish
2:	ret
ENDPROC(arg_arm64_dc_inval)

/* void arg_arm64_tlb_flush(void) — Invalidate all EL1 TLBs, IS */
ENTRY(arg_arm64_tlb_flush)
	dsb	ishst
	tlbi	vmalle1is
	dsb	ish
	isb
	ret
ENDPROC(arg_arm64_tlb_flush)
