/*	$OpenBSD: ofwreal.S,v 1.5 1999/07/05 20:56:26 rahnds Exp $	*/
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
.lcomm	srsave,16*4,4 	/* 16 words of 4 bytes to swap OF segment registers*/
	.globl _C_LABEL(ofmsr)
_C_LABEL(ofmsr):	.long	0	/* area to store msr for openfirmware*/

	.text
_ENTRY(_C_LABEL(ofw_init))
	mflr	31			/* save return address */

	mr	13,6			/* save args (only pointer used) */
	lis	8,ofentry@ha
	stw	5,ofentry@l(8)		/* save virtual mode firmware entry */

	lis	4,fwcall@ha		/* call ofw directly until vm setup */
	stw	5,fwcall@l(4)

	mfmsr	5
	lis	4,_C_LABEL(ofmsr)@ha	/* save msr from openfirmware */
	stw	5,_C_LABEL(ofmsr)@l(4)
#if 0
	lis	0,(0x80001ffe)@ha
	addi	0,0,(0x80001ffe)@l
	mtdbatu 0,0
	lis	0,(0x80000022)@ha
	addi	0,0,(0x80000022)@l
	mtdbatl 0,0
#endif

	lis	3,fwsave@ha	/* save the mmu values of the firmware */
	addi	3,3,fwsave@l
	lis	4,fwbatsave@ha
	addi	4,4,fwbatsave@l
	bl	savemmu

	/* save openfirmware address mappings */
	bl	_C_LABEL(save_ofw_mapping)

#if 0
	/* dont really need the bats from firmware saved, 0 to disable */
	lis	3,fwbatsave@ha
	addi	3,3,fwbatsave@l
	li	4,64
	li	5,0
1:	subi	4,4,4
	stwx	5,4,3
	cmpi	4,0,0
	bne	1b
#endif

	mr	6,13			/* restore args pointer */
	mtlr	31			/* restore return address */
	blr

/*
 * Save everyting related to the mmu to the saveare pointed to by r3.
 */
	.type	savemmu,@function
savemmu:

	mr	6,4	/* r4 holds pointer to BAT save area */

	li	4,0			/* save SRs */
1:
	addis	4,4,-0x10000000@ha
	or.	4,4,4
	mfsrin	5,4
	stwu	5,4(3)
	bne	1b

	mfibatl	4,0			/* save BATs */
	stw	4,0(6)
	mfibatu	4,0
	stw	4,4(6)
	mfibatl	4,1
	stw	4,8(6)
	mfibatu	4,1
	stw	4,0xc(6)
	mfibatl	4,2
	stw	4,0x10(6)
	mfibatu	4,2
	stw	4,0x14(6)
	mfibatl	4,3
	stw	4,0x18(6)
	mfibatu	4,3
	stw	4,0x1c(6)
	mfdbatl	4,0
	stw	4,0x20(6)
	mfdbatu	4,0
	stw	4,0x24(6)
	mfdbatl	4,1
	stw	4,0x28(6)
	mfdbatu	4,1
	stw	4,0x2c(6)
	mfdbatl	4,2
	stw	4,0x30(6)
	mfdbatu	4,2
	stw	4,0x34(6)
	mfdbatl	4,3
	stw	4,0x38(6)
	mfdbatu	4,3
	stw	4,0x3c(6)

	mfsprg	4,0			/* save SPRGs */
	stw	4,4(3)
	mfsprg	4,1
	stw	4,8(3)
	mfsprg	4,2
	stw	4,12(3)
	mfsprg	4,3
	stw	4,16(3)

	mfsdr1	4			/* save SDR1 */
	stw	4,20(3)

	addi	4,3,24

	mfmsr	4
	stw	4,24(3)

	sync
	isync

	blr

/*
 * Restore everyting related to the mmu from the savearea pointed to by r3.
 * and bats pointed to by r4.
 */
	.type	restoremmu,@function
restoremmu:

	li	0,0
	mtmsr	0
	mr	6,4			/* pointer to sr to restore */
	li	4,0			/* restore SRs */
1:
	lwzu	5,4(3)
	addis	4,4,-0x10000000@ha
	or.	4,4,4
	mtsrin	5,4
	bne	1b

	mfmsr	4
	lis	5,(PSL_IR|PSL_DR)@h	/* turn off MMU */
	ori	5,5,(PSL_IR|PSL_DR)@l	/* turn off MMU */
	andc	4,4,5			/* turn off MMU */
	mtmsr	4
	isync

	li	4,0			/* first, invalidate BATs */
	mtibatu	0,4
	mtibatu	1,4
	mtibatu	2,4
	mtibatu	3,4
	mtdbatu	0,4
	mtdbatu	1,4
	mtdbatu	2,4
	mtdbatu	3,4

	lwz	4,0(6)
	mtibatl	0,4			/* restore BATs */
	lwz	4,4(6)
	mtibatu	0,4
	lwz	4,8(6)
	mtibatl	1,4
	lwz	4,12(6)
	mtibatu	1,4
	lwz	4,16(6)
	mtibatl	2,4
	lwz	4,20(6)
	mtibatu	2,4
	lwz	4,24(6)
	mtibatl	3,4
	lwz	4,28(6)
	mtibatu	3,4
	lwz	4,32(6)
	mtdbatl	0,4
	lwz	4,36(6)
	mtdbatu	0,4
	lwz	4,40(6)
	mtdbatl	1,4
	lwz	4,44(6)
	mtdbatu	1,4
	lwz	4,48(6)
	mtdbatl	2,4
	lwz	4,52(6)
	mtdbatu	2,4
	lwz	4,56(6)
	mtdbatl	3,4
	lwz	4,60(6)
	mtdbatu	3,4

	lwz	4,4(3)
	mtsprg	0,4			/* restore SPRGs */
	lwz	4,8(3)
	mtsprg	1,4
	lwz	4,12(3)
	mtsprg	2,4
	lwz	4,16(3)
	mtsprg	3,4

	sync				/* remove everything from tlb */
	lis	4,0x40000@ha
	li	5,0x1000
1:
	subf.	4,5,4
	tlbie	4
	bne	1b

	sync
	tlbsync
	sync

	lwz	4,20(3)
	sync
	mtsdr1	4			/* restore SDR1 */


	/* tlbia */
	sync
	li	5,0x40
	mtctr	5
	li	4,0
    1:
	tlbie	4
	addi	4,4,0x1000
	bdnz	1b
	sync
	tlbsync
	sync

	lwz	4,24(3)
	mtmsr	4
	isync

	blr


_ENTRY(_C_LABEL(fwentry))
	stwu	1,-16(1)
	mflr	4
	stw	4,20(1)
	stw	3,12(1)			/* save arg */

	lis	3,clsave@ha		/* save mmu values of client */
	addi	3,3,clsave@l
	lis	4,clbatsave@ha		/* save mmu values of client */
	addi	4,4,clbatsave@l
	bl	savemmu

	lis	3,fwsave@ha		/* restore mmu values of firmware */
	addi	3,3,fwsave@l
	lis	4,fwbatsave@ha
	addi	4,4,fwbatsave@l
	bl	restoremmu

	lis	3,ofentry@ha
	lwz	3,ofentry@l(3)		/* get actual firmware entry */
	mtlr	3

	mfmsr	4
	ori	4,4,PSL_IR|PSL_DR	/* turn on MMU */
	mtmsr	4
	isync

	lwz	3,12(1)			/* restore arg */
	blrl				/* do actual firmware call */

	stw	3,12(1)			/* save return value */

	lis	3,fwsave@ha		/* save mmu values of firmare */
	addi	3,3,fwsave@l		/* (might not be necessary, but... */
	lis	4,fwbatsave@ha
	addi	4,4,fwbatsave@l
	bl	savemmu

	lis	3,clsave@ha		/* restore mmu values of client */
	addi	3,3,clsave@l
	lis	4,clbatsave@ha		/* save mmu values of client */
	addi	4,4,clbatsave@l
	bl	restoremmu

	lwz	4,20(1)
	lwz	3,12(1)			/* restore return value */

	mtlr	4
	addi	1,1,16
	blr

/*
 * OpenFirmware entry point
 */
_ENTRY(_C_LABEL(openfirmware))
	stwu	1,-16(1)
	mflr	0			/* save return address */
	stw	0,20(1)

	lis	4,fwcall@ha
	lwz	4,fwcall@l(4)

	mtlr	4
	blrl

	lwz	0,20(1)
	mtlr	0
	lwz	1,0(1)
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
	mfmsr	8			/* turn off interrupts */
	andi.	0,8,~(PSL_EE|PSL_RI)@l
	mtmsr	0
	stw	8,4(1)			/* abuse return address slot */

	lwz	5,0(1)			/* get length of stack frame */
	subf	5,1,5

	lis	7,firmstk+NBPG-8@ha
	addi	7,7,firmstk+NBPG-8@l
	li	6,0xf
	andc	7,7,6
	lis	6,ofw_back@ha
	addi	6,6,ofw_back@l
	subf	4,5,7			/* make room for stack frame on new stack */
	stwu	1,-16(7)
	stw	6,4(7)			/* setup return pointer */
	
	stw	7,-16(4)

	addi	3,1,8
	addi	1,4,-16
	subi	5,5,8
	subi	4,4,8

	b	_C_LABEL(ofbcopy)		/* and copy it */

	.type ofw_back,@function
ofw_back:
	lwz	1,0(1)			/* get callers original stack pointer */

	lwz	0,4(1)			/* get saved msr from abused slot */
	mtmsr	0
	
	lwz	1,0(1)			/* return */
	lwz	0,4(1)
	mtlr	0
	blr

