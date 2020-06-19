/* $OpenBSD: trap_subr.S,v 1.10 2020/06/19 17:29:33 kettenis Exp $ */
/* $NetBSD: trap_subr.S,v 1.20 2002/04/22 23:20:08 kleink Exp $	*/

/*-
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "assym.h"

#include <machine/psl.h>
#include <machine/trap.h>

	.abiversion 2

#define GET_CPUINFO(r) \
	mfsprg0  r

#define GET_TOCBASE(r) \
	bl	99f;							\
99:	mflr	r;							\
	addis	r, r, (.TOC. - 99b)@ha;					\
	addi	r, r, (.TOC. - 99b)@l;

/*
 * Restore SRs for a pmap
 *
 * Requires that r28-r31 be scratch, with r28 initialized to the SLB cache
 */

restore_kernsrs:
	GET_CPUINFO(%r28)
	addi	%r28, %r28, CI_KERNEL_SLB

	li	%r29, 0			/* Set the counter to zero */

	/* Invalidate entire SLB */
	slbia
	slbmfee	%r31, %r29
	clrrdi	%r31, %r31, 28
	slbie	%r31

1:	ld	%r31, 0(%r28)		/* Load SLBE */
	cmpdi	%r31, 0			/* If SLBE is not valid, stop */
	beqlr
	ld	%r30, 8(%r28)		/* Load SLBV  */
	slbmte	%r30, %r31		/* Install SLB entry */

	addi	%r28, %r28, 16		/* Advance pointer */
	addi	%r29, %r29, 1
	cmpdi	%r29, 31		/* Repeat if we are not at the end */
	blt	1b
	blr

/*
 * FRAME_SETUP assumes:
 *	SPRG1		SP (1)
 * 	SPRG3		trap type
 *	savearea	r27-r31,DAR,DSISR   (DAR & DSISR only for DSI traps)
 *	r28		LR
 *	r29		CR
 *	r30		scratch
 *	r31		scratch
 *	r1		kernel stack
 *	SRR0/1		as at start of trap
 *
 * NOTE: SPRG1 is never used while the MMU is on, making it safe to reuse
 * in any real-mode fault handler, including those handling double faults.
 */
#define	FRAME_SETUP(savearea)						\
/* Have to enable translation to allow access of kernel stack: */	\
	GET_CPUINFO(%r31);						\
	mfsrr0	%r30;							\
	std	%r30, (savearea+CPUSAVE_SRR0)(%r31);	/* save SRR0 */	\
	mfsrr1	%r30;							\
	std	%r30, (savearea+CPUSAVE_SRR1)(%r31);	/* save SRR1 */	\
	mfsprg1	%r31;			/* get saved SP (clears SPRG1) */ \
	mfmsr	%r30;							\
	ori	%r30, %r30, (PSL_DR|PSL_IR|PSL_RI)@l; /* relocation on */ \
	mtmsr	%r30;			/* stack can now be accessed */	\
	isync;								\
	stdu	%r31, -(FRAMELEN+288)(%r1); /* save it in the callframe */ \
	std	%r0, FRAME_0+48(%r1);	/* save r0 in the trapframe */	\
	std	%r31, FRAME_1+48(%r1);	/* save SP   "      "       */	\
	std	%r2, FRAME_2+48(%r1);	/* save r2   "      "       */	\
	std	%r28, FRAME_LR+48(%r1);	/* save LR   "      "       */	\
	std	%r29, FRAME_CR+48(%r1);	/* save CR   "      "       */	\
	GET_CPUINFO(%r2);						\
	ld	%r27, (savearea+CPUSAVE_R27)(%r2); /* get saved r27 */	\
	ld	%r28, (savearea+CPUSAVE_R28)(%r2); /* get saved r28 */	\
	ld	%r29, (savearea+CPUSAVE_R29)(%r2); /* get saved r29 */	\
	ld	%r30, (savearea+CPUSAVE_R30)(%r2); /* get saved r30 */	\
	ld	%r31, (savearea+CPUSAVE_R31)(%r2); /* get saved r31 */	\
	std	%r3, FRAME_3+48(%r1);	/* save r3-r31 */		\
	std	%r4, FRAME_4+48(%r1);					\
	std	%r5, FRAME_5+48(%r1);					\
	std	%r6, FRAME_6+48(%r1);					\
	std	%r7, FRAME_7+48(%r1);					\
	std	%r8, FRAME_8+48(%r1);					\
	std	%r9, FRAME_9+48(%r1);					\
	std	%r10, FRAME_10+48(%r1);					\
	std	%r11, FRAME_11+48(%r1);					\
	std	%r12, FRAME_12+48(%r1);					\
	std	%r13, FRAME_13+48(%r1);					\
	std	%r14, FRAME_14+48(%r1);					\
	std	%r15, FRAME_15+48(%r1);					\
	std	%r16, FRAME_16+48(%r1);					\
	std	%r17, FRAME_17+48(%r1);					\
	std	%r18, FRAME_18+48(%r1);					\
	std	%r19, FRAME_19+48(%r1);					\
	std	%r20, FRAME_20+48(%r1);					\
	std	%r21, FRAME_21+48(%r1);					\
	std	%r22, FRAME_22+48(%r1);					\
	std	%r23, FRAME_23+48(%r1);					\
	std	%r24, FRAME_24+48(%r1);					\
	std	%r25, FRAME_25+48(%r1);					\
	std	%r26, FRAME_26+48(%r1);					\
	std	%r27, FRAME_27+48(%r1);					\
	std	%r28, FRAME_28+48(%r1);					\
	std	%r29, FRAME_29+48(%r1);					\
	std	%r30, FRAME_30+48(%r1);					\
	std	%r31, FRAME_31+48(%r1);					\
	ld	%r28, (savearea+CPUSAVE_DAR)(%r2);  /* saved DAR */	\
	ld	%r29, (savearea+CPUSAVE_DSISR)(%r2);/* saved DSISR */	\
	ld	%r30, (savearea+CPUSAVE_SRR0)(%r2); /* saved SRR0 */	\
	ld	%r31, (savearea+CPUSAVE_SRR1)(%r2); /* saved SRR1 */	\
	mr	%r13, %r2;						\
	mfxer	%r3;							\
	mfctr	%r4;							\
	mfsprg3	%r5;							\
	std	%r3, FRAME_XER+48(%r1);	/* save xer/ctr/exc */		\
	std	%r4, FRAME_CTR+48(%r1);					\
	std	%r5, FRAME_EXC+48(%r1);					\
	std	%r28, FRAME_DAR+48(%r1);				\
	std	%r29, FRAME_DSISR+48(%r1); /* save dsisr/srr0/srr1 */	\
	std	%r30, FRAME_SRR0+48(%r1);				\
	std	%r31, FRAME_SRR1+48(%r1);

#define FRAME_LEAVE(savearea)						\
/* Disable exceptions: */						\
	mfmsr	%r2;							\
	andi.	%r2,%r2,~PSL_EE@l;					\
	mtmsr	%r2;							\
	isync;								\
/* Now restore regs: */							\
	ld	%r2, FRAME_SRR0+48(%r1);				\
	ld	%r3, FRAME_SRR1+48(%r1);				\
	ld	%r4, FRAME_CTR+48(%r1);					\
	ld	%r5, FRAME_XER+48(%r1);					\
	ld	%r6, FRAME_LR+48(%r1);					\
	GET_CPUINFO(%r7);						\
	std	%r2, (savearea+CPUSAVE_SRR0)(%r7); /* save SRR0 */	\
	std	%r3, (savearea+CPUSAVE_SRR1)(%r7); /* save SRR1 */	\
	ld	%r7, FRAME_CR+48(%r1);					\
	mtctr	%r4;							\
	mtxer	%r5;							\
	mtlr	%r6;							\
	mtsprg2	%r7;							\
	ld	%r31, FRAME_31+48(%r1);	/* restore r0-31 */		\
	ld	%r30, FRAME_30+48(%r1);					\
	ld	%r29, FRAME_29+48(%r1);					\
	ld	%r28, FRAME_28+48(%r1);					\
	ld	%r27, FRAME_27+48(%r1);					\
	ld	%r26, FRAME_26+48(%r1);					\
	ld	%r25, FRAME_25+48(%r1);					\
	ld	%r24, FRAME_24+48(%r1);					\
	ld	%r23, FRAME_23+48(%r1);					\
	ld	%r22, FRAME_22+48(%r1);					\
	ld	%r21, FRAME_21+48(%r1);					\
	ld	%r20, FRAME_20+48(%r1);					\
	ld	%r19, FRAME_19+48(%r1);					\
	ld	%r18, FRAME_18+48(%r1);					\
	ld	%r17, FRAME_17+48(%r1);					\
	ld	%r16, FRAME_16+48(%r1);					\
	ld	%r15, FRAME_15+48(%r1);					\
	ld	%r14, FRAME_14+48(%r1);					\
	ld	%r13, FRAME_13+48(%r1);					\
	ld	%r12, FRAME_12+48(%r1);					\
	ld	%r11, FRAME_11+48(%r1);					\
	ld	%r10, FRAME_10+48(%r1);					\
	ld	%r9, FRAME_9+48(%r1);					\
	ld	%r8, FRAME_8+48(%r1);					\
	ld	%r7, FRAME_7+48(%r1);					\
	ld	%r6, FRAME_6+48(%r1);					\
	ld	%r5, FRAME_5+48(%r1);					\
	ld	%r4, FRAME_4+48(%r1);					\
	ld	%r3, FRAME_3+48(%r1);					\
	ld	%r2, FRAME_2+48(%r1);					\
	ld	%r0, FRAME_0+48(%r1);					\
	ld	%r1, FRAME_1+48(%r1);					\
/* Can't touch %r1 from here on */					\
	mtsprg3	%r3;			/* save r3 */			\
/* Disable translation, machine check and recoverability: */		\
	mfmsr	%r3;							\
	andi.	%r3, %r3, ~(PSL_DR|PSL_IR|PSL_ME|PSL_RI)@l;		\
	mtmsr	%r3;							\
	isync;								\
1:	mfsprg2	%r3;			/* restore cr */		\
	mtcr	%r3;							\
	GET_CPUINFO(%r3);						\
	ld	%r3, (savearea+CPUSAVE_SRR0)(%r3); /* restore srr0 */	\
	mtsrr0	%r3;							\
	GET_CPUINFO(%r3);						\
	ld	%r3, (savearea+CPUSAVE_SRR1)(%r3); /* restore srr1 */	\
	mtsrr1	%r3;							\
	mfsprg3	%r3			/* restore r3 */


	.text

	.globl trapcode, trapcodeend
trapcode:
	mtsprg1	%r1
	mflr	%r1
	mtsprg2	%r1
	ld	%r1, TRAP_ENTRY(0)
	mtlr	%r1
	li	%r1, 0xe0
	blrl
trapcodeend:

	.globl hvtrapcode, hvtrapcodeend
hvtrapcode:
	mtsprg1	%r1
	mflr	%r1
	mtsprg2	%r1
	ld	%r1, TRAP_HVENTRY(0)
	mtlr	%r1
	li	%r1, 0xe0
	blrl
hvtrapcodeend:

/*
 * generichvtrap makes a hypervisor trap look like a normal trap.
 */

	.globl generichvtrap
generichvtrap:
	/* Move HSRR0/HSRR1 to SSR0/SRR1 */
	mtsprg3	%r1
	mfspr	%r1, 314	/* HSRR0 */
	mtsrr0	%r1
	mfspr	%r1, 315	/* HSRR1 */
	mtsrr1	%r1
	mfsprg3	%r1
	/* FALLTHROUGH */

/*
 * generictrap does some standard setup for trap handling to minimize
 * the code that need be installed in the actual vectors. It expects
 * the following conditions.
 * 
 * R1 - Trap vector = LR & (0xff00 | R1)
 * SPRG1 - Original R1 contents
 * SPRG2 - Original LR
 */

	.globl generictrap
generictrap:
	/* Save R1 for computing the exception vector */
	mtsprg3	%r1

	/* Save interesting registers */
	GET_CPUINFO(%r1)
	std	%r27, (CI_TEMPSAVE+CPUSAVE_R27)(%r1)	/* free r27-r31 */
	std	%r28, (CI_TEMPSAVE+CPUSAVE_R28)(%r1)
	std	%r29, (CI_TEMPSAVE+CPUSAVE_R29)(%r1)
	std	%r30, (CI_TEMPSAVE+CPUSAVE_R30)(%r1)
	std	%r31, (CI_TEMPSAVE+CPUSAVE_R31)(%r1)
	mfdar	%r30
	std	%r30, (CI_TEMPSAVE+CPUSAVE_DAR)(%r1)
	mfdsisr	%r30
	std	%r30, (CI_TEMPSAVE+CPUSAVE_DSISR)(%r1)
	mfsprg1	%r1			/* restore SP, in case of branch */
	mfsprg2	%r28			/* save LR */
	mfcr	%r29			/* save CR */

	/* Compute the exception vector from the link register */
	mfsprg3 %r31
	ori	%r31, %r31, 0xff00
	mflr	%r30
	addi	%r30, %r30, -4 /* The branch instruction, not the next */
	and	%r30, %r30, %r31
	mtsprg3	%r30

	/* XXX skip this unless we come for userland */
	mr	%r27, %r28
	mtsprg2	%r29
	bl	restore_kernsrs		/* enable kernel mapping */
	mfsprg2	%r29
	mr	%r28, %r27

	FRAME_SETUP(CI_TEMPSAVE)
	GET_TOCBASE(%r2)
trapagain:
	addi	%r3, %r1, 48
	bl	trap

	.globl	trapexit
trapexit:
/* Disable interrupts: */
	mfmsr	%r3
	andi.	%r3, %r3, ~PSL_EE@l
	mtmsr	%r3
	isync
/* Test AST pending: */
	ld	%r5,FRAME_SRR1+48(%r1)
	mtcr	%r5
	bf	17,1f			/* branch if PSL_PR is false */

	GET_CPUINFO(%r3)		/* get per-CPU pointer */
	ld	%r4, CI_CURPROC(%r3)
	lwz	%r4, P_MD_ASTPENDING(%r4)
	cmpwi	%r4, 0
	beq	1f
	li	%r6, EXC_AST
	std	%r6, FRAME_EXC+48(%r1)
	b	trapagain
1:
	FRAME_LEAVE(CI_TEMPSAVE)
	rfid
