/* $OpenBSD: cpufunc_asm.S,v 1.5 2018/08/12 17:15:10 mortimer Exp $ */
/*-
 * Copyright (c) 2014 Robin Randhawa
 * Copyright (c) 2015 The FreeBSD Foundation
 * All rights reserved.
 *
 * Portions of this software were developed by Andrew Turner
 * under sponsorship from the FreeBSD Foundation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <machine/asm.h>
#include <machine/param.h>
//__FBSDID("$FreeBSD: head/sys/arm64/arm64/cpufunc_asm.S 305546 2016-09-07 16:46:54Z andrew $");

/*
 * FIXME:
 * Need big.LITTLE awareness at some point.
 * Using [id]cache_line_size may not be the best option.
 * Need better SMP awareness.
 */
	.text
	.align	2

/*
 * Macro to handle the cache. This takes the start address in x0, length
 * in x1. It will corrupt x0, x1, x2, and x3.
 */
.macro cache_handle_range dcop = 0, ic = 0, icop = 0
.if \ic == 0
	ldr	x3, =dcache_line_size	/* Load the D cache line size */
.else
	ldr	x3, =idcache_line_size	/* Load the I & D cache line size */
.endif
	ldr	x3, [x3]
	sub	x4, x3, #1		/* Get the address mask */
	and	x2, x0, x4		/* Get the low bits of the address */
	add	x1, x1, x2		/* Add these to the size */
	bic	x0, x0, x4		/* Clear the low bit of the address */
1:
	dc	\dcop, x0
	dsb	ish
.if \ic != 0
	ic	\icop, x0
	dsb	ish
.endif
	add	x0, x0, x3		/* Move to the next line */
	subs	x1, x1, x3		/* Reduce the size */
	b.hi	1b			/* Check if we are done */
.if \ic != 0
	isb
.endif
.endm

/*
 * Generic functions to read/modify/write the internal coprocessor registers
 */

ENTRY(cpu_setttb)
	RETGUARD_SETUP(cpu_setttb, x15)
	mrs	x2, ttbr1_el1
	bfi	x2, x0, #48, #16
	msr	ttbr1_el1, x2
	isb
	msr	ttbr0_el1, x1
	isb
	RETGUARD_CHECK(cpu_setttb, x15)
	ret
END(cpu_setttb)

ENTRY(cpu_tlb_flush)
	RETGUARD_SETUP(cpu_tlb_flush, x15)
	dsb	ishst
	tlbi	vmalle1is
	dsb	ish
	isb
	RETGUARD_CHECK(cpu_tlb_flush, x15)
	ret
END(cpu_tlb_flush)

ENTRY(cpu_tlb_flush_asid)
	RETGUARD_SETUP(cpu_tlb_flush_asid, x15)
	dsb	ishst
	tlbi	vae1is, x0
	dsb	ish
	isb
	RETGUARD_CHECK(cpu_tlb_flush_asid, x15)
	ret
END(cpu_tlb_flush_asid)

ENTRY(cpu_tlb_flush_all_asid)
	RETGUARD_SETUP(cpu_tlb_flush_all_asid, x15)
	dsb	ishst
	tlbi	vaale1is, x0
	dsb	ish
	isb
	RETGUARD_CHECK(cpu_tlb_flush_all_asid, x15)
	ret
END(cpu_tlb_flush_all_asid)

ENTRY(cpu_tlb_flush_asid_all)
	RETGUARD_SETUP(cpu_tlb_flush_asid_all, x15)
	dsb	ishst
	tlbi	aside1is, x0
	dsb	ish
	isb
	RETGUARD_CHECK(cpu_tlb_flush_asid_all, x15)
	ret
END(cpu_tlb_flush_asid_all)

/*
 * void cpu_dcache_wb_range(vaddr_t, vsize_t)
 */
ENTRY(cpu_dcache_wb_range)
	RETGUARD_SETUP(cpu_dcache_wb_range, x15)
	cache_handle_range	dcop = cvac
	RETGUARD_CHECK(cpu_dcache_wb_range, x15)
	ret
END(cpu_dcache_wb_range)

/*
 * void cpu_dcache_wbinv_range(vaddr_t, vsize_t)
 */
ENTRY(cpu_dcache_wbinv_range)
	RETGUARD_SETUP(cpu_dcache_wbinv_range, x15)
	cache_handle_range	dcop = civac
	RETGUARD_CHECK(cpu_dcache_wbinv_range, x15)
	ret
END(cpu_dcache_wbinv_range)

/*
 * void cpu_dcache_inv_range(vaddr_t, vsize_t)
 *
 * Note, we must not invalidate everything.  If the range is too big we
 * must use wb-inv of the entire cache.
 */
ENTRY(cpu_dcache_inv_range)
	RETGUARD_SETUP(cpu_dcache_inv_range, x15)
	cache_handle_range	dcop = ivac
	RETGUARD_CHECK(cpu_dcache_inv_range, x15)
	ret
END(cpu_dcache_inv_range)

/*
 * void cpu_idcache_wbinv_range(vaddr_t, vsize_t)
 */
ENTRY(cpu_idcache_wbinv_range)
	RETGUARD_SETUP(cpu_idcache_wbinv_range, x15)
	cache_handle_range	dcop = civac, ic = 1, icop = ivau
	RETGUARD_CHECK(cpu_idcache_wbinv_range, x15)
	ret
END(cpu_idcache_wbinv_range)

/*
 * void cpu_icache_sync_range(vaddr_t, vsize_t)
 */
ENTRY(cpu_icache_sync_range)
	RETGUARD_SETUP(cpu_icache_sync_range, x15)
	cache_handle_range	dcop = cvau, ic = 1, icop = ivau
	RETGUARD_CHECK(cpu_icache_sync_range, x15)
	ret
END(cpu_icache_sync_range)
