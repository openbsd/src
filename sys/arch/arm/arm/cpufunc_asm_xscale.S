/*	$OpenBSD: cpufunc_asm_xscale.S,v 1.8 2016/04/25 04:46:56 jsg Exp $ */
/*	$NetBSD: cpufunc_asm_xscale.S,v 1.16 2002/08/17 16:36:32 thorpej Exp $	*/

/*
 * Copyright (c) 2001, 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Allen Briggs and Jason R. Thorpe for Wasabi Systems, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the NetBSD Project by
 *	Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2001 Matt Thomas.
 * Copyright (c) 1997,1998 Mark Brinicombe.
 * Copyright (c) 1997 Causality Limited
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Causality Limited.
 * 4. The name of Causality Limited may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CAUSALITY LIMITED ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL CAUSALITY LIMITED BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * XScale assembly functions for CPU / MMU / TLB specific operations
 */
 
#include <machine/cpu.h>
#include <machine/asm.h>
#include <arm/sysreg.h>

/*
 * Size of the XScale core caches.
 */
#define	CACHE_SIZE		0x8000

/*
 * CPWAIT -- Canonical method to wait for CP15 update.
 * From: Intel 80200 manual, section 2.3.3.
 *
 * NOTE: Clobbers the specified temp reg.
 */
#define	CPWAIT_BRANCH							 \
	sub	pc, pc, #4

#define	CPWAIT(tmp)							 \
	mrc	CP15_TTBR0(tmp)		/* arbitrary read of CP15 */	;\
	mov	tmp, tmp		/* wait for it to complete */	;\
	CPWAIT_BRANCH			/* branch to next insn */

#define	CPWAIT_AND_RETURN_SHIFTER	lsr #32

#define	CPWAIT_AND_RETURN(tmp)						 \
	mrc	CP15_TTBR0(tmp)		/* arbitrary read of CP15 */	;\
	/* Wait for it to complete and branch to the return address */	 \
	sub	pc, lr, tmp, CPWAIT_AND_RETURN_SHIFTER

ENTRY(xscale_cpwait)
	CPWAIT_AND_RETURN(r0)

/*
 * We need a separate cpu_control() entry point, since we have to
 * invalidate the Branch Target Buffer in the event the BPRD bit
 * changes in the control register.
 */
ENTRY(xscale_control)
	mrc	CP15_SCTLR(r3)		/* Read the control register */
	bic	r2, r3, r0		/* Clear bits */
	eor	r2, r2, r1		/* XOR bits */

	teq	r2, r3			/* Only write if there was a change */
	mcrne	CP15_BPIALL		/* Invalidate the BTB */
	mcrne	CP15_SCTLR(r2)		/* Write new control register */
	mov	r0, r3			/* Return old value */

	CPWAIT_AND_RETURN(r1)

/*
 * Functions to set the MMU Translation Table Base register
 *
 * We need to clean and flush the cache as it uses virtual
 * addresses that are about to change.
 */
ENTRY(xscale_setttb)
	mrs	r3, cpsr
	orr	r1, r3, #(PSR_I | PSR_F)
	msr	cpsr_c, r1

	stmfd	sp!, {r0-r3, lr}
	bl	_C_LABEL(xscale_cache_cleanID)
	mcr	CP15_ICIALLU		/* invalidate I$ and BTB */
	mcr	CP15_CP15DSB(r0)	/* drain write and fill buffer */

	CPWAIT(r0)

	ldmfd	sp!, {r0-r3, lr}

	/* Write the TTB */ 
	mcr	CP15_TTBR0(r0)	

	/* If we have updated the TTB we must flush the TLB */
	mcr	CP15_TLBIALL(r0)	/* invalidate I+D TLB */

	/* The cleanID above means we only need to flush the I cache here */
	mcr	CP15_ICIALLU		/* invalidate I$ and BTB */

	CPWAIT(r0)

	msr	cpsr_c, r3
	mov	pc, lr

/*
 * TLB functions
 *
 * Note: We don't need to worry about issuing a CPWAIT after
 * TLB operations, because we expect a pmap_update() to follow.
 */
ENTRY(xscale_tlb_flushID_SE)
	mcr	CP15_DTLBIMVA		/* flush D tlb single entry */
	mcr	CP15_ITLBIMVA		/* flush I tlb single entry */
	mov	pc, lr

/*
 * Cache functions
 */
ENTRY(xscale_cache_flushID)
	mcr	p15, 0, r0, c7, c7, 0	/* flush I+D cache */
	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_flushI)
	mcr	CP15_ICIALLU		/* flush I cache */
	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_flushD)
	mcr	p15, 0, r0, c7, c6, 0	/* flush D cache */
	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_flushI_SE)
	mcr	CP15_ICIMVAU(r0)	/* flush I cache single entry */
	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_flushD_SE)
	/*
	 * Errata (rev < 2): Must clean-dcache-line to an address
	 * before invalidate-dcache-line to an address, or dirty
	 * bits will not be cleared in the dcache array.
	 */
	mcr	CP15_DCCMVAC(r0)
	mcr	CP15_DCIMVAC(r0)	/* flush D cache single entry */
	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_cleanD_E)
	mcr	CP15_DCCMVAC(r0)	/* clean D cache entry */
	CPWAIT_AND_RETURN(r0)

/*
 * Information for the XScale cache clean/purge functions:
 *
 *	* Virtual address of the memory region to use
 *	* Size of memory region
 *
 * Note the virtual address for the Data cache clean operation
 * does not need to be backed by physical memory, since no loads
 * will actually be performed by the allocate-line operation.
 *
 * Note that the Mini-Data cache MUST be cleaned by executing
 * loads from memory mapped into a region reserved exclusively
 * for cleaning of the Mini-Data cache.
 */
	.data

	.global	_C_LABEL(xscale_cache_clean_addr)
_C_LABEL(xscale_cache_clean_addr):
	.word	0x00000000

	.global	_C_LABEL(xscale_cache_clean_size)
_C_LABEL(xscale_cache_clean_size):
	.word	CACHE_SIZE

#	.global	_C_LABEL(xscale_minidata_clean_addr)
#_C_LABEL(xscale_minidata_clean_addr):
#	.word	0x00000000

#	.global	_C_LABEL(xscale_minidata_clean_size)
#_C_LABEL(xscale_minidata_clean_size):
#	.word	0x00000800

	.text

.Lxscale_cache_clean_addr:
	.word	_C_LABEL(xscale_cache_clean_addr)
.Lxscale_cache_clean_size:
	.word	_C_LABEL(xscale_cache_clean_size)

.Lxscale_minidata_clean_addr:
	.word	_C_LABEL(xscale_minidata_clean_addr)
.Lxscale_minidata_clean_size:
	.word	_C_LABEL(xscale_minidata_clean_size)

#define	XSCALE_CACHE_CLEAN_BLOCK					\
	mrs	r3, cpsr					;	\
	orr	r0, r3, #(PSR_I | PSR_F)			;	\
	msr	cpsr_c, r0

#define	XSCALE_CACHE_CLEAN_UNBLOCK					\
	msr	cpsr_c, r3

#define	XSCALE_CACHE_CLEAN_PROLOGUE					\
	XSCALE_CACHE_CLEAN_BLOCK				;	\
	ldr	r2, .Lxscale_cache_clean_addr			;	\
	ldmia	r2, {r0, r1}					;	\
	/*								\
	 * BUG ALERT!							\
	 *								\
	 * The XScale core has a strange cache eviction bug, which	\
	 * requires us to use 2x the cache size for the cache clean	\
	 * and for that area to be aligned to 2 * cache size.		\
	 *								\
	 * The work-around is to use 2 areas for cache clean, and to	\
	 * alternate between them whenever this is done.  No one knows	\
	 * why the work-around works (mmm!).				\
	 */								\
	eor	r0, r0, #(CACHE_SIZE)				;	\
	str	r0, [r2]					;	\
	add	r0, r0, r1

#define	XSCALE_CACHE_CLEAN_EPILOGUE					\
	XSCALE_CACHE_CLEAN_UNBLOCK

ENTRY_NP(xscale_cache_syncI)
ENTRY_NP(xscale_cache_purgeID)
	mcr	CP15_ICIALLU		/* flush I cache (D cleaned below) */
ENTRY_NP(xscale_cache_cleanID)
ENTRY_NP(xscale_cache_purgeD)
ENTRY(xscale_cache_cleanD)
	XSCALE_CACHE_CLEAN_PROLOGUE

1:	subs	r0, r0, #32
	mcr	p15, 0, r0, c7, c2, 5	/* allocate cache line */
	subs	r1, r1, #32
	bne	1b

	CPWAIT(r0)

	mcr	CP15_CP15DSB(r0)	/* drain write buffer */

	CPWAIT(r0)

	XSCALE_CACHE_CLEAN_EPILOGUE
	mov	pc, lr

/*
 * Clean the mini-data cache.
 *
 * It's expected that we only use the mini-data cache for
 * kernel addresses, so there is no need to purge it on
 * context switch, and no need to prevent userspace access
 * while we clean it.
 */
ENTRY(xscale_cache_clean_minidata)
	ldr	r2, .Lxscale_minidata_clean_addr
	ldmia	r2, {r0, r1}
1:	ldr	r3, [r0], #32
	subs	r1, r1, #32
	bne	1b

	mcr	CP15_CP15DSB(r0)	/* drain write buffer */

	CPWAIT_AND_RETURN(r1)

ENTRY(xscale_cache_purgeID_E)
	mcr	CP15_DCCMVAC(r0)	/* clean D cache entry */
	CPWAIT(r1)
	mcr	CP15_CP15DSB(r0)	/* drain write buffer */
	mcr	CP15_ICIMVAU(r0)	/* flush I cache single entry */
	mcr	CP15_DCIMVAC(r0)	/* flush D cache single entry */
	CPWAIT_AND_RETURN(r1)

ENTRY(xscale_cache_purgeD_E)
	mcr	CP15_DCCMVAC(r0)	/* clean D cache entry */
	CPWAIT(r1)
	mcr	CP15_CP15DSB(r0)	/* drain write buffer */
	mcr	CP15_DCIMVAC(r0)	/* flush D cache single entry */
	CPWAIT_AND_RETURN(r1)

/*
 * Soft functions
 */
/* xscale_cache_syncI is identical to xscale_cache_purgeID */

ENTRY(xscale_cache_cleanID_rng)
ENTRY(xscale_cache_cleanD_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(xscale_cache_cleanID)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	CP15_DCCMVAC(r0)	/* clean D cache entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	CPWAIT(r0)

	mcr	CP15_CP15DSB(r0)	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_purgeID_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(xscale_cache_purgeID)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	CP15_DCCMVAC(r0)	/* clean D cache entry */
	mcr	CP15_DCIMVAC(r0)	/* flush D cache single entry */
	mcr	CP15_ICIMVAU(r0)	/* flush I cache single entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	CPWAIT(r0)

	mcr	CP15_CP15DSB(r0)	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_purgeD_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(xscale_cache_purgeD)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	CP15_DCCMVAC(r0)	/* clean D cache entry */
	mcr	CP15_DCIMVAC(r0)	/* flush D cache single entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	CPWAIT(r0)

	mcr	CP15_CP15DSB(r0)	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_syncI_rng)
	cmp	r1, #0x4000
	bcs	_C_LABEL(xscale_cache_syncI)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	CP15_DCCMVAC(r0)	/* clean D cache entry */
	mcr	CP15_ICIMVAU(r0)	/* flush I cache single entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	CPWAIT(r0)

	mcr	CP15_CP15DSB(r0)	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)

ENTRY(xscale_cache_flushD_rng)
	cmp	r1, #(CACHE_SIZE)
	movcs	r1, #(CACHE_SIZE)

	and	r2, r0, #0x1f
	add	r1, r1, r2
	bic	r0, r0, #0x1f

1:	mcr	CP15_DCIMVAC(r0)	/* flush D cache single entry */
	add	r0, r0, #32
	subs	r1, r1, #32
	bhi	1b

	mcr	CP15_CP15DSB(r0)	/* drain write buffer */

	CPWAIT_AND_RETURN(r0)

/*
 * Context switch.
 *
 * These is the CPU-specific parts of the context switcher cpu_switch()
 * These functions actually perform the TTB reload.
 *
 * NOTE: Special calling convention
 *	r1, r4-r13 must be preserved
 */
ENTRY(xscale_context_switch)
	/*
	 * CF_CACHE_PURGE_ID will *ALWAYS* be called prior to this.
	 * Thus the data cache will contain only kernel data and the
	 * instruction cache will contain only kernel code, and all
	 * kernel mappings are shared by all processes.
	 */

	/* Write the TTB */
	mcr	CP15_TTBR0(r0)	

	/* If we have updated the TTB we must flush the TLB */
	mcr	CP15_TLBIALL(r0)	/* flush the I+D tlb */

	CPWAIT_AND_RETURN(r0)

/*
 * xscale_cpu_sleep
 *
 * This is called when there is nothing on any of the run queues.
 * We go into IDLE mode so that any IRQ or FIQ will awaken us.
 *
 * If this is called with anything other than ARM_SLEEP_MODE_IDLE,
 * ignore it.
 */
ENTRY(xscale_cpu_sleep)
	tst	r0, #0x00000000
	bne	1f
	mov	r0, #0x1
	mcr	p14, 0, r0, c7, c0, 0

1:
	mov	pc, lr
