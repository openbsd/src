/*	$OpenBSD: ofwreal.S,v 1.3 2003/10/16 05:03:22 deraadt Exp $	*/
/*	$NetBSD: ofwreal.S,v 1.1 1996/09/30 16:34:51 ws Exp $	*/

/*
 * Copyright (C) 1996 Wolfgang Solfrank.
 * Copyright (C) 1996 TooLs GmbH.
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

/*
 * This file provides a real-mode client interface on machines, that
 * (incorrectly) only implement virtual mode client interface.
 *
 * It assumes though, that any actual memory in the machine is
 * mapped 1:1 even by the virtual mode OpenFirmware.
 * Furthermore it assumes that addresses returned by OpenFirmware are not
 * accessed by the client.
 *
 */
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/param.h>

#define	CACHELINE	32		/* Note that this value is really hardwired */

	.data
ofentry:	.long	0		/* actual entry to firmware in virtual mode */

#define	SRSIZE		(16*4+4)
#define	SPRGSIZE	(4*4)
#define	SDR1SIZE	4
#define MSRSIZE		4
#define	SVSIZE		(SRSIZE+SPRGSIZE+SDR1SIZE+MSRSIZE)
#define BATSIZE		(16*4)

	.global _C_LABEL(fwcall)
_C_LABEL(fwcall): .long 0

.lcomm	fwsave,SVSIZE,8
.lcomm	fwbatsave,BATSIZE,8
.lcomm	clsave,SVSIZE,8
.lcomm	clbatsave,BATSIZE,8
.lcomm	ofsrsave,16*4,4	/* 16 words of 4 bytes to store OF segment registers */
.lcomm	srsave,16*4,4	/* 16 words of 4 bytes to swap OF segment registers*/
	.globl _C_LABEL(ofmsr)
_C_LABEL(ofmsr):	.long	0	/* area to store msr for openfirmware*/

	.text
_ENTRY(_C_LABEL(ofw_init))
	mflr	%r31			/* save return address */

	mr	%r13,%r6		/* save args (only pointer used) */
	lis	%r8,ofentry@ha
	stw	%r5,ofentry@l(%r8)	/* save virtual mode firmware entry */

	lis	%r4,fwcall@ha		/* call ofw directly until vm setup */
	stw	%r5,fwcall@l(%r4)

	mfmsr	%r5
	lis	%r4,_C_LABEL(ofmsr)@ha	/* save msr from openfirmware */
	stw	%r5,_C_LABEL(ofmsr)@l(%r4)
#if 0
	lis	%r0,(0x80001ffe)@ha
	addi	%r0,%r0,(0x80001ffe)@l
	mtdbatu 0,%r0
	lis	%r0,(0x80000022)@ha
	addi	%r0,%r0,(0x80000022)@l
	mtdbatl 0,%r0
#endif

	lis	%r3,fwsave@ha	/* save the mmu values of the firmware */
	addi	%r3,%r3,fwsave@l
	lis	%r4,fwbatsave@ha
	addi	%r4,%r4,fwbatsave@l
	bl	savemmu

	/* save openfirmware address mappings */
	bl	_C_LABEL(save_ofw_mapping)

#if 0
	/* dont really need the bats from firmware saved, 0 to disable */
	lis	%r3,fwbatsave@ha
	addi	%r3,%r3,fwbatsave@l
	li	%r4,64
	li	%r5,0
1:	subi	%r4,%r4,%r4
	stwx	%r5,%r4,%r3
	cmpi	4,0,0
	bne	1b
#endif

	mr	%r6,%r13		/* restore args pointer */
	mtlr	%r31			/* restore return address */
	blr

/*
 * Save everyting related to the mmu to the saveare pointed to by r3.
 */
	.type	savemmu,@function
savemmu:

	mr	%r6,%r4	/* r4 holds pointer to BAT save area */

	li	%r4,0			/* save SRs */
1:
	addis	%r4,%r4,-0x10000000@ha
	or.	%r4,%r4,%r4
	mfsrin	%r5,%r4
	stwu	%r5,4(%r3)
	bne	1b

	mfibatl	%r4,0			/* save BATs */
	stw	%r4,0(%r6)
	mfibatu	%r4,0
	stw	%r4,4(%r6)
	mfibatl	%r4,1
	stw	%r4,8(%r6)
	mfibatu	%r4,1
	stw	%r4,0xc(%r6)
	mfibatl	%r4,2
	stw	%r4,0x10(%r6)
	mfibatu	%r4,2
	stw	%r4,0x14(%r6)
	mfibatl	%r4,3
	stw	%r4,0x18(%r6)
	mfibatu	%r4,3
	stw	%r4,0x1c(%r6)
	mfdbatl	%r4,0
	stw	%r4,0x20(%r6)
	mfdbatu	%r4,0
	stw	%r4,0x24(%r6)
	mfdbatl	%r4,1
	stw	%r4,0x28(%r6)
	mfdbatu	%r4,1
	stw	%r4,0x2c(%r6)
	mfdbatl	%r4,2
	stw	%r4,0x30(%r6)
	mfdbatu	%r4,2
	stw	%r4,0x34(%r6)
	mfdbatl	%r4,3
	stw	%r4,0x38(%r6)
	mfdbatu	%r4,3
	stw	%r4,0x3c(%r6)

	mfsprg	%r4,0			/* save SPRGs */
	stw	%r4,4(%r3)
	mfsprg	%r4,1
	stw	%r4,8(%r3)
	mfsprg	%r4,2
	stw	%r4,12(%r3)
	mfsprg	%r4,3
	stw	%r4,16(%r3)

	mfsdr1	%r4			/* save SDR1 */
	stw	%r4,20(%r3)

	addi	%r4,%r3,24

	mfmsr	%r4
	stw	%r4,24(%r3)

	sync
	isync

	blr

/*
 * Restore everyting related to the mmu from the savearea pointed to by r3.
 * and bats pointed to by r4.
 */
	.type	restoremmu,@function
restoremmu:

	li	%r0,0
	mtmsr	%r0
	mr	%r6,%r4			/* pointer to sr to restore */
	li	%r4,0			/* restore SRs */
1:
	lwzu	%r5,4(%r3)
	addis	%r4,%r4,-0x10000000@ha
	or.	%r4,%r4,%r4
	mtsrin	%r5,%r4
	bne	1b

	mfmsr	%r4
	lis	%r5,(PSL_IR|PSL_DR)@h		/* turn off MMU */
	ori	%r5,%r5,(PSL_IR|PSL_DR)@l	/* turn off MMU */
	andc	%r4,%r4,%r5				/* turn off MMU */
	mtmsr	%r4
	isync

	li	%r4,0			/* first, invalidate BATs */
	mtibatu	0,%r4
	mtibatu	1,%r4
	mtibatu	2,%r4
	mtibatu	3,%r4
	mtdbatu	0,%r4
	mtdbatu	1,%r4
	mtdbatu	2,%r4
	mtdbatu	3,%r4

	lwz	%r4,0(%r6)
	mtibatl	0,%r4			/* restore BATs */
	lwz	%r4,4(%r6)
	mtibatu	0,%r4
	lwz	%r4,8(%r6)
	mtibatl	1,%r4
	lwz	%r4,12(%r6)
	mtibatu	1,%r4
	lwz	%r4,16(%r6)
	mtibatl	2,%r4
	lwz	%r4,20(%r6)
	mtibatu	2,%r4
	lwz	%r4,24(%r6)
	mtibatl	3,%r4
	lwz	%r4,28(%r6)
	mtibatu	3,%r4
	lwz	%r4,32(%r6)
	mtdbatl	0,%r4
	lwz	%r4,36(%r6)
	mtdbatu	0,%r4
	lwz	%r4,40(%r6)
	mtdbatl	1,%r4
	lwz	%r4,44(%r6)
	mtdbatu	1,%r4
	lwz	%r4,48(%r6)
	mtdbatl	2,%r4
	lwz	%r4,52(%r6)
	mtdbatu	2,%r4
	lwz	%r4,56(%r6)
	mtdbatl	3,%r4
	lwz	%r4,60(%r6)
	mtdbatu	3,%r4

	lwz	%r4,4(%r3)
	mtsprg	0,4			/* restore SPRGs */
	lwz	%r4,8(%r3)
	mtsprg	1,4
	lwz	%r4,12(%r3)
	mtsprg	2,4
	lwz	%r4,16(%r3)
	mtsprg	3,4

	sync				/* remove everything from tlb */
	lis	%r4,0x40000@ha
	li	%r5,0x1000
1:
	subf.	%r4,%r5,%r4
	tlbie	%r4
	bne	1b

	sync
	tlbsync
	sync

	lwz	%r4,20(%r3)
	sync
	mtsdr1	%r4			/* restore SDR1 */


	/* tlbia */
	sync
	li	%r5,0x40
	mtctr	%r5
	li	%r4,0
    1:
	tlbie	%r4
	addi	%r4,%r4,0x1000
	bdnz	1b
	sync
	tlbsync
	sync

	lwz	%r4,24(%r3)
	mtmsr	%r4
	isync

	blr


_ENTRY(_C_LABEL(fwentry))
	stwu	%r1,-16(%r1)
	mflr	%r4
	stw	%r4,20(%r1)
	stw	%r3,12(%r1)			/* save arg */

	lis	%r3,clsave@ha		/* save mmu values of client */
	addi	%r3,%r3,clsave@l
	lis	%r4,clbatsave@ha	/* save mmu values of client */
	addi	%r4,%r4,clbatsave@l
	bl	savemmu

	lis	%r3,fwsave@ha		/* restore mmu values of firmware */
	addi	%r3,%r3,fwsave@l
	lis	%r4,fwbatsave@ha
	addi	%r4,%r4,fwbatsave@l
	bl	restoremmu

	lis	%r3,ofentry@ha
	lwz	%r3,ofentry@l(%r3)	/* get actual firmware entry */
	mtlr	%r3

	mfmsr	%r4
	ori	%r4,%r4,PSL_IR|PSL_DR	/* turn on MMU */
	mtmsr	%r4
	isync

	lwz	%r3,12(%r1)		/* restore arg */
	blrl				/* do actual firmware call */

	stw	%r3,12(%r1)		/* save return value */

	lis	%r3,fwsave@ha		/* save mmu values of firmare */
	addi	%r3,%r3,fwsave@l	/* (might not be necessary, but... */
	lis	%r4,fwbatsave@ha
	addi	%r4,%r4,fwbatsave@l
	bl	savemmu

	lis	%r3,clsave@ha		/* restore mmu values of client */
	addi	%r3,%r3,clsave@l
	lis	%r4,clbatsave@ha	/* save mmu values of client */
	addi	%r4,%r4,clbatsave@l
	bl	restoremmu

	lwz	%r4,20(%r1)
	lwz	%r3,12(%r1)		/* restore return value */

	mtlr	%r4
	addi	%r1,%r1,16
	blr

/*
 * OpenFirmware entry point
 */
_ENTRY(_C_LABEL(openfirmware))
	stwu	%r1,-16(%r1)
	mflr	%r0			/* save return address */
	stw	%r0,20(%r1)

	lis	%r4,fwcall@ha
	lwz	%r4,fwcall@l(%r4)

	mtlr	%r4
	blrl

	lwz	%r0,20(%r1)
	mtlr	%r0
	lwz	%r1,0(%r1)
	blr

/*
 * Switch to/from OpenFirmware real mode stack
 *
 * Note: has to be called as the very first thing in OpenFirmware interface routines.
 * E.g.:
 * int
 * OF_xxx(arg1, arg2)
 * type arg1, arg2;
 * {
 *	static struct {
 *		char *name;
 *		int nargs;
 *		int nreturns;
 *		char *method;
 *		int arg1;
 *		int arg2;
 *		int ret;
 *	} args = {
 *		"xxx",
 *		2,
 *		1,
 *	};
 *
 *	ofw_stack();
 *	args.arg1 = arg1;
 *	args.arg2 = arg2;
 *	if (openfirmware(&args) < 0)
 *		return -1;
 *	return args.ret;
 * }
 */
.lcomm	firmstk,NBPG,16
.comm	_C_LABEL(OF_buf),NBPG,PGOFSET

_ENTRY(_C_LABEL(ofw_stack))
	mfmsr	%r8			/* turn off interrupts */
	andi.	%r0,%r8,~(PSL_EE|PSL_RI)@l
	mtmsr	%r0
	stw	%r8,4(%r1)		/* abuse return address slot */

	lwz	%r5,0(%r1)		/* get length of stack frame */
	subf	%r5,%r1,%r5

	lis	%r7,firmstk+NBPG-8@ha
	addi	%r7,%r7,firmstk+NBPG-8@l
	li	%r6,0xf
	andc	%r7,%r7,%r6
	lis	%r6,ofw_back@ha
	addi	%r6,%r6,ofw_back@l
	subf	%r4,%r5,%r7		/* make room for stack frame on new stack */
	stwu	%r1,-16(%r7)
	stw	%r6,4(%r7)		/* setup return pointer */

	stw	%r7,-16(%r4)

	addi	%r3,%r1,%r8
	addi	%r1,%r4,-16
	subi	%r5,%r5,%r8
	subi	%r4,%r4,%r8

	b	_C_LABEL(ofbcopy)	/* and copy it */

	.type ofw_back,@function
ofw_back:
	lwz	%r1,0(%r1)		/* get callers original stack pointer */

	lwz	%r0,4(%r1)		/* get saved msr from abused slot */
	mtmsr	%r0

	lwz	%r1,0(%r1)		/* return */
	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

