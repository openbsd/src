/*	$OpenBSD: palm_start.S,v 1.1 2009/09/05 01:22:11 marex Exp $	*/
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
#include <arm/pmap.h>		/* for PMAP_DOMAIN_KERNEL */
#include <machine/palm_var.h>

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
 * Kernel start routine for DBPXA250 (Lubbock)
 * this code is excuted at the very first after the kernel is loaded
 * by RedBoot.
 */
	.text

	.global	_C_LABEL(palm_start)
_C_LABEL(palm_start):
	/* Are we running on ROM ? */
	cmp     pc, #0x06000000
	bhi     palm_start_ram

	/* move me to RAM
	 * XXX: we can use memcpy if it is PIC
	 */
	ldr r1, Lcopy_size
	adr r0, _C_LABEL(palm_start)
	add r1, r1, #3
	mov r1, r1, LSR #2
	mov r2, #SDRAM_START
	add r2, r2, #0x00200000
	mov r4, r2

5:	ldr r3,[r0],#4
	str r3,[r2],#4
	subs r1,r1,#1
	bhi 5b

	cmp	pc, r5
	/* Jump to RAM */
	ldr r0, Lstart_off

	blo 1f
	/* if we were running out of virtual mapped space, disable mmu */
	mov	r2, #0
	mov     r1, #(CPU_CONTROL_32BP_ENABLE | CPU_CONTROL_32BD_ENABLE)
	mcr     15, 0, r1, c1, c0, 0
	mcrne   15, 0, r2, c8, c7, 0    /* nail I+D TLB on ARMv4 and greater */
	
1:
	add pc, r4, r0

Lcopy_size:	.word _edata-_C_LABEL(palm_start)
Lstart_off:	.word palm_start_ram-_C_LABEL(palm_start)

palm_start_ram:	
	/*
	 *  Kernel is loaded in SDRAM (0xa0200000..), and is expected to run
	 *  in VA 0xc0200000..
	 */

	mrc	p15, 0, r0, c2, c0, 0		/* get ttb prepared by redboot */
	adr	r4, mmu_init_table2
	
#define BUILD_STARTUP_PAGETABLE
#ifdef BUILD_STARTUP_PAGETABLE
	mrc	p15, 0, r2, c1, c0, 0
	mov	r2, #0
	tst	r2, #CPU_CONTROL_MMU_ENABLE		/* we already have a page table? */
	bne	3f

	/* build page table from scratch */
	ldr	r0, Lstartup_pagetable
	adr	r4, mmu_init_table
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
#endif

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

	/* Jump to kernel code in TRUE VA */
	adr	r0, Lstart
	ldr	pc, [r0]

Lstart:
	.word	start

#define MMU_INIT(va,pa,n_sec,attr) \
	.word	n_sec					    ; \
	.word	4*((va)>>L1_S_SHIFT)			    ; \
	.word	(pa)|(attr)				    ;
	
#ifdef BUILD_STARTUP_PAGETABLE
#ifndef STARTUP_PAGETABLE_ADDR
#define STARTUP_PAGETABLE_ADDR 0xa0004000
#endif
Lstartup_pagetable:	.word	STARTUP_PAGETABLE_ADDR
mmu_init_table:	
	/* fill all table VA==PA */
	MMU_INIT(0x00000000, 0x00000000, 1<<(32-L1_S_SHIFT), L1_TYPE_S|L1_S_AP(AP_KRW))
	/* map SDRAM VA==PA, WT cacheable */
	MMU_INIT(SDRAM_START, SDRAM_START, SDRAM_SIZE_MB, L1_TYPE_S|L1_S_C|L1_S_AP(AP_KRW))
#endif
mmu_init_table2:	
	/* map VA 0xc0000000..0xc3ffffff to PA 0xa0000000..0xa3ffffff */
	MMU_INIT(0xc0000000, SDRAM_START, SDRAM_SIZE_MB, L1_TYPE_S|L1_S_C|L1_S_AP(AP_KRW))

	.word 0	/* end of table */
