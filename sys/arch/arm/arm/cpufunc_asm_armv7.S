/* $OpenBSD: cpufunc_asm_armv7.S,v 1.1 2009/05/09 02:28:22 drahn Exp $ */
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


/* STUBS - implement these */
ENTRY(armv7_icache_sync_all)
ENTRY(armv7_icache_sync_range)
ENTRY(armv7_dcache_wbinv_all)
ENTRY(armv7_dcache_wbinv_range)
ENTRY(armv7_dcache_wb_range)
ENTRY(armv7_idcache_wbinv_all)
ENTRY(armv7_idcache_wbinv_range)
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

	.align 2
C_OBJECT(armv7_dcache_sets_max)
	.word	0
C_OBJECT(armv7_dcache_index_max)
	.word	0
C_OBJECT(armv7_dcache_sets_inc)
	.word	0
C_OBJECT(armv7_dcache_index_inc)
	.word	0
