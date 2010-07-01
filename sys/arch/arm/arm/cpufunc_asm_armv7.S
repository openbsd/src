/* $OpenBSD: cpufunc_asm_armv7.S,v 1.2 2010/07/01 22:40:10 drahn Exp $ */
/*
 * Copyright (c) 2008 Dale Rahn <drahn@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <machine/cpu.h>
#include <machine/asm.h>

ENTRY(armv7_cpu_sleep)
#if 0
	wfi
#else
	.long	0xe340f003;
#endif
	mov	pc, lr

ENTRY(armv7_drain_writebuf)
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(armv7_setttb)
	stmfd	sp!, {r0, lr}
	bl	_C_LABEL(armv7_idcache_wbinv_all)
	ldmfd	sp!, {r0, lr}

	mcr	p15, 0, r0, c2, c0, 0	/* load new TTB */

	mcr	p15, 0, r0, c8, c7, 0	/* invalidate I+D TLBs */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

/*
 * TLB functions
 */
ENTRY(armv7_tlb_flushID_SE)
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(armv7_tlb_flushI_SE)
	mcr	p15, 0, r0, c8, c5, 1	/* flush I tlb single entry */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

/*
 * TLB functions
 */
ENTRY(armv7_tlb_flushID)
	mcr	p15, 0, r0, c8, c7, 0	/* flush I+D tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(armv7_tlb_flushI)
	mcr	p15, 0, r0, c8, c5, 0	/* flush I tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(armv7_tlb_flushD)
	mcr	p15, 0, r0, c8, c6, 0	/* flush D tlb */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr

ENTRY(armv7_tlb_flushD_SE)
	mcr	p15, 0, r0, c8, c6, 1	/* flush D tlb single entry */
	mcr	p15, 0, r0, c7, c10, 4	/* drain write buffer */
	mov	pc, lr


/*
 * Cache operations.  For the entire cache we use the set/index
 * operations.
 */
	s_max	.req r0
	i_max	.req r1
	s_inc	.req r2
	i_inc	.req r3
ENTRY(armv7_icache_sync_range)
	ldr	ip, .Larmv7_line_size
	cmp	r1, #0x4000
	bcs	.Larmv7_icache_sync_all
	ldr	ip, [ip]
	sub	r1, r1, #1		/* Don't overrun */
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
1:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c10, 1	/* Clean D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bpl	1b
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	mov	pc, lr

ENTRY(armv7_icache_sync_all)
.Larmv7_icache_sync_all:
	/*
	 * We assume that the code here can never be out of sync with the
	 * dcache, so that we can safely flush the Icache and fall through
	 * into the Dcache cleaning code.
	 */
	mcr	p15, 0, r0, c7, c5, 0	/* Flush I cache */
	/* Fall through to clean Dcache. */

.Larmv7_dcache_wb:
	ldr	ip, .Larmv7_cache_data
	ldmia	ip, {s_max, i_max, s_inc, i_inc}
1:
	orr	ip, s_max, i_max
2:
	mcr	p15, 0, ip, c7, c10, 2	/* Clean D cache SE with Set/Index */
	sub	ip, ip, i_inc
	tst	ip, i_max		/* Index 0 is last one */
	bne	2b			/* Next index */
	mcr	p15, 0, ip, c7, c10, 2	/* Clean D cache SE with Set/Index */
	subs	s_max, s_max, s_inc
	bpl	1b			/* Next set */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	mov	pc, lr

.Larmv7_line_size:
	.word	_C_LABEL(arm_pdcache_line_size)

ENTRY(armv7_dcache_wb_range)
#if 1
	/* until pmap can invalidate before unmapping */
	/* XXX this also invalidates */
	b _C_LABEL(armv7_dcache_wbinv_all)
#else
	ldr	ip, .Larmv7_line_size
	cmp	r1, #0x4000
	bcs	.Larmv7_dcache_wb
	ldr	ip, [ip]
	sub	r1, r1, #1		/* Don't overrun */
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
1:
	mcr	p15, 0, r0, c7, c10, 1	/* Clean D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bpl	1b
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
#endif
	mov	pc, lr

ENTRY(armv7_idcache_wbinv_range)
	ldr	ip, .Larmv7_line_size
	cmp	r1, #0x4000
#if 0
	bcs	.Larmv7_idcache_wbinv_all
#else
	bcc	1f
	mov	r1, #0x4000
1:
#endif
	ldr	ip, [ip]
	sub	r1, r1, #1		/* Don't overrun */
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
1:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c14, 1	/* Purge D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bpl	1b
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	mov	pc, lr

ENTRY(armv7_dcache_wbinv_range)
	ldr	ip, .Larmv7_line_size
	cmp	r1, #0x4000
	bcs _C_LABEL(armv7_dcache_wbinv_all)
	ldr	ip, [ip]
	sub	r1, r1, #1		/* Don't overrun */
	sub	r3, ip, #1
	and	r2, r0, r3
	add	r1, r1, r2
	bic	r0, r0, r3
1:
	mcr	p15, 0, r0, c7, c5, 1	/* Invalidate I cache SE with VA */
	mcr	p15, 0, r0, c7, c14, 1	/* Purge D cache SE with VA */
	add	r0, r0, ip
	subs	r1, r1, ip
	bpl	1b
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	mov	pc, lr


/*
 * Context switch.
 *
 * These is the CPU-specific parts of the context switcher cpu_switch()
 * These functions actually perform the TTB reload.
 *
 * NOTE: Special calling convention
 *	r1, r4-r13 must be preserved
 */
ENTRY(armv7_context_switch)
	/*
	 * We can assume that the caches will only contain kernel addresses
	 * at this point.  So no need to flush them again.
	 */
	mcr	p15, 0, r0, c7, c10, 4	/* drain the write buffer */
	mcr	p15, 0, r0, c2, c0, 0	/* set the new TTB */
	mcr	p15, 0, r0, c8, c7, 0	/* and flush the I+D tlbs */

	/* Paranoia -- make sure the pipeline is empty. */
	nop
	nop
	nop
	mov	pc, lr

/* XXX The following macros should probably be moved to asm.h */
#define _DATA_OBJECT(x) .globl x; .type x,_ASM_TYPE_OBJECT; x:
#define C_OBJECT(x)	_DATA_OBJECT(_C_LABEL(x))

.Larmv7_cache_data:
	.word	_C_LABEL(armv7_dcache_sets_max)

	.align 2
C_OBJECT(armv7_dcache_sets_max)
	.word	0
C_OBJECT(armv7_dcache_index_max)
	.word	0
C_OBJECT(armv7_dcache_sets_inc)
	.word	0
C_OBJECT(armv7_dcache_index_inc)
	.word	0
