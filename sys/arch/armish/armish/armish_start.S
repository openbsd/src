/*	$OpenBSD: armish_start.S,v 1.2 2006/05/29 17:30:26 drahn Exp $	*/
/*	$NetBSD: iq80321_start.S,v 1.4 2002/10/14 22:32:54 bjh21 Exp $	*/

/*
 * Copyright (c) 2002 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Jason R. Thorpe for Wasabi Systems, Inc.
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

#include <machine/asm.h>
#include <arm/armreg.h>
#include <arm/pte.h>

	.section .start,"ax",%progbits

	.global	_C_LABEL(iq80321_start)
_C_LABEL(iq80321_start):
	/*
	 * We will go ahead and disable the MMU here so that we don't
	 * have to worry about flushing caches, etc.
	 *
	 * Note that we may not currently be running VA==PA, which means
	 * we'll need to leap to the next insn after disabing the MMU.
	 */
	adr	r8, Lunmapped
	bic	r8, r8, #0xff000000	/* clear upper 8 bits */
	orr	r8, r8, #0xa0000000	/* OR in physical base address */

	mrc	p15, 0, r2, c1, c0, 0
	bic	r2, r2, #CPU_CONTROL_MMU_ENABLE
	mcr	p15, 0, r2, c1, c0, 0

	nop
	nop
	nop

	mov	pc, r8			/* Heave-ho! */

Lunmapped:
	/*
	 * We want to construct a memory map that maps us
	 * VA==PA (SDRAM at 0xa0000000) and also double-maps
	 * that space at 0xc0000000 (where the kernel address
	 * space starts).  We create these mappings uncached
	 * and unbuffered to be safe.
	 *
	 * We also want to map the various devices we want to
	 * talk to VA==PA during bootstrap.
	 *
	 * We just use section mappings for all of this to make it easy.
	 *
	 * We will put the L1 table to do all this at 0xa0004000, which
	 * is also where RedBoot puts it.
	 */

	/*
	 * Step 1: Map the entire address space VA==PA.
	 */
	adr	r0, Ltable
	ldr	r0, [r0]			/* r0 = &l1table */

	mov	r3, #(L1_S_AP(AP_KRW))
	orr	r3, r3, #(L1_TYPE_S)
	mov	r2, #0x100000			/* advance by 1MB */
	mov	r1, #0x1000			/* 4096MB */
1:
	str	r3, [r0], #0x04
	add	r3, r3, r2
	subs	r1, r1, #1
	bgt	1b

	/*
	 * Step 2: Map VA 0xc0000000->0xc3ffffff to PA 0xa0000000->0xa3ffffff.
	 */
	adr	r0, Ltable			/* r0 = &l1table */
	ldr	r0, [r0]

	mov	r3, #(L1_S_AP(AP_KRW))
	orr	r3, r3, #(L1_TYPE_S)
	orr	r3, r3, #0xa0000000
	add	r0, r0, #(0xc00 * 4)		/* offset to 0xc00xxxxx */
	mov	r1, #0x40			/* 64MB */
1:
	str	r3, [r0], #0x04
	add	r3, r3, r2
	subs	r1, r1, #1
	bgt	1b

	/* OK!  Page table is set up.  Give it to the CPU. */
	adr	r0, Ltable
	ldr	r0, [r0]
	mcr	p15, 0, r0, c2, c0, 0

	/* Flush the old TLBs, just in case. */
	mcr	p15, 0, r0, c8, c7, 0

	/* Set the Domain Access register.  Very important! */
	mov	r0, #1
	mcr	p15, 0, r0, c3, c0, 0

	/* Get ready to jump to the "real" kernel entry point... */
	ldr	r0, Lstart

	/* OK, let's enable the MMU. */
	mrc	p15, 0, r2, c1, c0, 0
	orr	r2, r2, #CPU_CONTROL_MMU_ENABLE
	mcr	p15, 0, r2, c1, c0, 0

	nop
	nop
	nop

	/* CPWAIT sequence to make sure the MMU is on... */
	mrc	p15, 0, r2, c2, c0, 0	/* arbitrary read of CP15 */
	mov	r2, r2			/* force it to complete */
	mov	pc, r0			/* leap to kernel entry point! */

Ltable:
	.word	0xa0004000

Lstart:
	.word	start
