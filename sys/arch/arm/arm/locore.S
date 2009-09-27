/*	$OpenBSD: locore.S,v 1.3 2009/09/27 18:14:52 miod Exp $	*/
/*	$NetBSD: locore.S,v 1.14 2003/04/20 16:21:40 thorpej Exp $	*/

/*
 * Copyright (C) 1994-1997 Mark Brinicombe
 * Copyright (C) 1994 Brini
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
 *	This product includes software developed by Brini.
 * 4. The name of Brini may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY BRINI ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL BRINI BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"
#include <sys/syscall.h>
#include <sys/errno.h>
#include <machine/asm.h>
#include <machine/cpu.h>
#include <machine/frame.h>

/* What size should this really be ? It is only used by init_arm() */
#define INIT_ARM_STACK_SIZE	2048

/*
 * This is for kvm_mkdb, and should be the address of the beginning
 * of the kernel text segment (not necessarily the same as kernbase).
 */

	.text
	.align	0

ENTRY_NP(kernel_text)

ASENTRY_NP(start)
	adr	r1, .Lstart
	ldmia	r1, {r1, r2, sp}	/* Set initial stack and */
	sub	r2, r2, r1		/* get zero init data */
	mov	r3, #0

.L1:
	str	r3, [r1], #0x0004	/* Zero the bss */
	subs	r2, r2, #4
	bgt	.L1

	mov	fp, #0x00000000		/* trace back starts here */
	bl	_C_LABEL(initarm)	/* Off we go */

	/* init arm will return the new stack pointer. */
	mov	sp, r0

	mov	fp, #0x00000000		/* trace back starts here */
	mov	ip, sp
	stmfd	sp!, {fp, ip, lr, pc}
	sub	fp, ip, #4

	bl	_C_LABEL(main)		/* call main()! */

	adr	r0, .Lmainreturned
	b	_C_LABEL(panic)
	/* NOTREACHED */

.Lstart:
	.word	_edata
	.word	_end
	.word	svcstk + INIT_ARM_STACK_SIZE

.Lmainreturned:
	.asciz	"main() returned"
	.align	0

	.bss
svcstk:
	.space	INIT_ARM_STACK_SIZE

	.text
	.align	0

#ifndef OFW
	/* OFW based systems will use OF_boot() */
	
.Lcpufuncs:	
	.word	_C_LABEL(cpufuncs)

ENTRY_NP(cpu_reset)
	mrs     r2, cpsr
	bic	r2, r2, #(PSR_MODE)
	orr     r2, r2, #(PSR_SVC32_MODE)
	orr	r2, r2, #(I32_bit | F32_bit)
	msr     cpsr_all, r2

	ldr	r4, .Lcpu_reset_address
	ldr	r4, [r4]

	ldr	r0, .Lcpufuncs
	mov	lr, pc
	ldr	pc, [r0, #CF_IDCACHE_WBINV_ALL]

	/*
	 * Load the cpu_reset_needs_v4_MMU_disable flag to determine if it's
	 * necessary.
	 */

	ldr	r1, .Lcpu_reset_needs_v4_MMU_disable
	ldr	r1, [r1]
	cmp	r1, #0
	mov	r2, #0

	/*
 	 * MMU & IDC off, 32 bit program & data space
	 * Hurl ourselves into the ROM
	 */
	mov	r0, #(CPU_CONTROL_32BP_ENABLE | CPU_CONTROL_32BD_ENABLE)
	mcr     15, 0, r0, c1, c0, 0
	mcrne   15, 0, r2, c8, c7, 0 	/* nail I+D TLB on ARMv4 and greater */
	mov     pc, r4

	/*
	 * _cpu_reset_address contains the address to branch to, to complete
	 * the cpu reset after turning the MMU off
	 * This variable is provided by the hardware specific code
	 */
.Lcpu_reset_address:
	.word	_C_LABEL(cpu_reset_address)

	/*
	 * cpu_reset_needs_v4_MMU_disable contains a flag that signals if the
	 * v4 MMU disable instruction needs executing... it is an illegal instruction
	 * on f.e. ARM6/7 that locks up the computer in an endless illegal
	 * instruction / data-abort / reset loop.
	 */
.Lcpu_reset_needs_v4_MMU_disable:
	.word	_C_LABEL(cpu_reset_needs_v4_MMU_disable)

#endif	/* OFW */

#ifdef IPKDB
/*
 * Execute(inst, psr, args, sp)
 *
 * Execute INSTruction with PSR and ARGS[0] - ARGS[3] making
 * available stack at SP for next undefined instruction trap.
 *
 * Move the instruction onto the stack and jump to it.
 */
ENTRY_NP(Execute)
	mov	ip, sp
	stmfd	sp!, {r2, r4-r7, fp, ip, lr, pc}
	sub	fp, ip, #4
	mov	ip, r3
	ldr	r7, .Lreturn
	stmfd	sp!, {r0, r7}
	adr	r7, #.LExec
	mov	r5, r1
	mrs	r4, cpsr
	ldmia	r2, {r0-r3}
	mov	r6, sp
	mov	sp, ip
	msr	cpsr_all, r5
	mov	pc, r6
.LExec:
	mrs	r5, cpsr
/* XXX Cannot switch thus easily back from user mode */
	msr	cpsr_all, r4
	add	sp, r6, #8
	ldmfd	sp!, {r6}
	stmia	r6, {r0-r3}
	mov	r0, r5
	ldmdb	fp, {r4-r7, fp, sp, pc}
.Lreturn:
	mov	pc, r7
#endif

/*
 * setjump + longjmp
 */
ENTRY(setjmp)
	stmia	r0, {r4-r14}
	mov	r0, #0x00000000
	mov	pc, lr

ENTRY(longjmp)
	ldmia	r0, {r4-r14}
	mov	r0, #0x00000001
	mov	pc, lr

	.data
	.global _C_LABEL(esym)
_C_LABEL(esym):	.word	_C_LABEL(end)

ENTRY_NP(abort)
	b	_C_LABEL(abort)


/* End of locore.S */
