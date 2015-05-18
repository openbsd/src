/*	$OpenBSD: armv7_start.S,v 1.3 2015/05/18 23:56:47 jsg Exp $	*/
/*	$NetBSD: lubbock_start.S,v 1.1 2003/06/18 10:51:15 bsh Exp $ */

/*
 * Copyright (c) 2002, 2003  Genetec Corporation.  All rights reserved.
 * Written by Hiroyuki Bessho for Genetec Corporation.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of Genetec Corporation may not be used to endorse or 
 *    promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY GENETEC CORPORATION ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL GENETEC CORPORATION
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

#undef	DOMAIN_CLIENT	/* XXX */
#include "assym.h"

/*
 * CPWAIT -- Canonical method to wait for CP15 update.
 * NOTE: Clobbers the specified temp reg.
 * copied from arm/arm/cpufunc_asm_xscale.S
 * XXX: better be in a common header file.
 */
#define	CPWAIT_BRANCH							 \
	sub	pc, pc, #4

#define	CPWAIT(tmp)							 \
	mrc	p15, 0, tmp, c2, c0, 0	/* arbitrary read of CP15 */	;\
	mov	tmp, tmp		/* wait for it to complete */	;\
	CPWAIT_BRANCH			/* branch to next insn */
	
/*
 * Kernel start routine for OMAP
 * this code is excuted at the very first after the kernel is loaded
 * by U-Boot.
 *
 * This code makes a number of assumptions.
 * 1) it is running in RAM.
 * 2) it is run at a fairly well known offset from the beginning of
 *    a ram section.
 * 3) the memory region around the kernel will be at least about 32MB
 * 4) memory just below the kernel can be used by the kernel
 * 5) memory at the start of the ram section may not be useable
 *    it may contain bootloader/u-boot
 * 6) the fdt/kernel parameters in arg r0-r2 may point outside the
 *    32MB of ram (it likely should be copied very early)
 *
 */
	.text

	.global	_C_LABEL(bootstrap_start)
_C_LABEL(bootstrap_start):
	/* Save U-Boot arguments */
	mov	r6, r0
	mov	r7, r1
	mov	r8, r2

	/*
	 *  Kernel is loaded in SDRAM (0xX0200000..), and is expected to run
	 *  in VA 0xc0200000..
	 *
	 * which base memory address the kernel is located is unknown
	 * however the kernel should be located at 0x00300000 offset
	 * from that ram area. copy the PC into a register and strip
	 * the low bits
	 */

	mov r9, pc
	and r9, r9, #0xf0000000	// Leave the memory base in r9

	/* create the bootstrap MMU table at offset 0x00200000 */

	/* build page table from scratch */
	orr	r0, r9, #0x00200000
	adr	r4, mmu_init_table

	mov	r2, r9, lsr #18
	ldr	r3, [r4, #8]
	bic	r3, r3, #0xf0000000
	orr	r3, r3, r9
	str	r2, [r4, #4]
	str	r3, [r4, #8]
	str	r3, [r4, #0x14] // ram address for 0xc0000000
	add	r3, r3, #0x01000000
	str	r3, [r4, #0x20] // ram address for 0x00000000 (vectors)

	/*
	 * the first entry has two fields that need to be updated for
	 * specific ram configuration of this board.
	 */
	b	3f

2:
	str	r3, [r0, r2]
	add	r2, r2, #4
	add	r3, r3, #(L1_S_SIZE)
	adds	r1, r1, #-1
	bhi	2b
3:	
	ldmia	r4!, {r1,r2,r3}   /* # of sections, PA|attr, VA */
	cmp	r1, #0
	bne	2b	

	mcr	p15, 0, r0, c2, c0, 0	/* Set TTB */
	mcr	p15, 0, r0, c8, c7, 0	/* Flush TLB */

	/* Set the Domain Access register.  Very important! */
        mov     r0, #((DOMAIN_CLIENT << (PMAP_DOMAIN_KERNEL*2)) | DOMAIN_CLIENT)
	mcr	p15, 0, r0, c3, c0, 0

	/* Enable MMU */
	mrc	p15, 0, r0, c1, c0, 0
	orr	r0, r0, #CPU_CONTROL_MMU_ENABLE
	mcr	p15, 0, r0, c1, c0, 0
	CPWAIT(r0)

	/* Restore U-Boot arguments */
	mov	r0, r6
	mov	r1, r7
	mov	r2, r8

	/* Jump to kernel code in TRUE VA */
	adr	r6, Lstart
	ldr	pc, [r6]

Lstart:
	.word	start

#define MMU_INIT(va,pa,n_sec,attr) \
	.word	n_sec					    ; \
	.word	4*((va)>>L1_S_SHIFT)			    ; \
	.word	(pa)|(attr)				    ;

mmu_init_table:
	/* map SDRAM VA==PA, WT cacheable */
	MMU_INIT(0x00000000, 0x00000000, 64, L1_TYPE_S|L1_S_C|L1_S_V7_AP(AP_KRW))
mmu_init_table2:	
	/* map VA 0xc0000000..0xc3ffffff to PA 0xa0000000..0xa3ffffff */
	MMU_INIT(0xc0000000, 0x00000000, 64, L1_TYPE_S|L1_S_C|L1_S_V7_AP(AP_KRW))
	MMU_INIT(0x00000000, 0x00000000, 1, L1_TYPE_S|L1_S_C|L1_S_V7_AP(AP_KRW))

	.word 0	/* end of table */

	.text

/*-
 * Copyright (c) 2011
 *	Ben Gray <ben.r.gray@gmail.com>.
 * All rights reserved.
 *
 * Based heavily on the Linux OMAP secure API
 *    Copyright (C) 2010 Texas Instruments, Inc.
 *    Written by Santosh Shilimkar <santosh.shilimkar@ti.com>
 *
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 *	omap4_smc_call - issues a secure monitor API call
 *	@r0: contains the monitor API number
 *	@r1: contains the value to set
 * 
 *	This function will send a secure monitor call to the internal rom code in
 *	the trust mode. The rom code expects that r0 will contain the value and 
 *	r12 will contain the API number, so internally the function swaps the
 *	register settings around.  The trust mode code may also alter all the cpu
 *	registers so everything (including the lr register) is saved on the stack
 *	before the call.
 *
 *	RETURNS:
 *	nothing
 */
ENTRY(omap4_smc_call)
	stmfd   sp!, {r2-r12, lr}
	mov     r12, r0
	mov     r0, r1
#if 0
	dsb
#else
	.word	0xf57ff040
#endif
#if 0
	smc     #0	/* XXX or #1 ??? */
#else
	.word	0xe1600070
#endif
	ldmfd   sp!, {r2-r12, pc}

