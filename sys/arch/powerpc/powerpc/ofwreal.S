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
 * TODO: handle set-callback specially
 */
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/trap.h>

#define	CACHELINE	32		/* Note that this value is really hardwired */

	.data
ofentry:	.long	0		/* actual entry to firmware in virtual mode */

#define	BATSIZE		(8*8)
#define	SRSIZE		(16*4)
#define	SPRGSIZE	(4*4)
#define	SDR1SIZE	4
#define	SI1SIZE		(2*256)
#define	SI2SIZE		(3*256)
#define	SVSIZE		(BATSIZE+SRSIZE+SPRGSIZE+SDR1SIZE+SI1SIZE+SI2SIZE)

.lcomm	fwsave,SVSIZE,8
.lcomm	clsave,SVSIZE,8

_ENTRY(_C_LABEL(ofwr_init))
	mflr	31			/* save return address */

	mr	13,6			/* save args (only pointer used) */
	lis	8,ofentry@ha
	stw	5,ofentry@l(8)		/* save virtual mode firmware entry */

	lis	3,fwsave@ha		/* save the mmu values of the firmware */
	addi	3,3,fwsave@l
	bl	savemmu

	lis	5,fwentry@ha		/* get new firmware entry */
	addi	5,5,fwentry@l

	mr	6,13			/* restore args pointer */
	mtlr	31			/* restore return address */
	blr

/*
 * Emulated firmware entry.
 */
fwentry:
	mflr	0			/* save return address */
	stw	0,4(1)
	stwu	1,-16(1)		/* setup stack frame */
	stw	3,8(1)			/* save arg */

	lis	3,clsave@ha		/* save mmu values of client */
	addi	3,3,clsave@l
	bl	savemmu

	lis	3,fwsave@ha		/* restore mmu values of firmware */
	addi	3,3,fwsave@l
	bl	restoremmu

	lis	3,ofentry@ha
	lwz	3,ofentry@l(3)		/* get actual firmware entry */
	mtlr	3

	mfmsr	4
	ori	4,4,PSL_IR|PSL_DR	/* turn on MMU */
	mtmsr	4
	isync

	lwz	3,8(1)			/* restore arg */
	blrl				/* do actual firmware call */

	stw	3,8(1)			/* save return value */

	mfmsr	4
	andi.	4,4,~(PSL_IR|PSL_DR)	/* turn off MMU */
	mtmsr	4
	isync

	lis	3,fwsave@ha		/* save mmu values of firmare */
	addi	3,3,fwsave@l		/* (might not be necessary, but... */
	bl	savemmu

	lis	3,clsave@ha		/* restore mmu values of client */
	addi	3,3,clsave@l
	bl	restoremmu

	lwz	3,8(1)			/* restore return value */
	lwz	1,0(1)			/* and return */
	lwz	0,4(1)
	mtlr	0
	blr

/*
 * Save everyting related to the mmu to the saveare pointed to by r3.
 */
savemmu:

	mfibatl	4,0			/* save BATs */
	stw	4,0(3)
	mfibatu	4,0
	stw	4,4(3)
	mfibatl	4,1
	stw	4,8(3)
	mfibatu	4,1
	stw	4,12(3)
	mfibatl	4,2
	stw	4,16(3)
	mfibatu	4,2
	stw	4,20(3)
	mfibatl	4,3
	stw	4,24(3)
	mfibatu	4,3
	stw	4,28(3)
	mfdbatl	4,0
	stw	4,32(3)
	mfdbatu	4,0
	stw	4,36(3)
	mfdbatl	4,1
	stw	4,40(3)
	mfdbatu	4,1
	stw	4,44(3)
	mfdbatl	4,2
	stw	4,48(3)
	mfdbatu	4,2
	stw	4,52(3)
	mfdbatl	4,3
	stw	4,56(3)
	mfdbatu	4,3
	stwu	4,60(3)

	li	4,0			/* save SRs */
1:
	addis	4,4,-0x10000000@ha
	or.	4,4,4
	mfsrin	5,4
	stwu	5,4(3)
	bne	1b

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
	mflr	11
	li	3,EXC_DSI		/* save DSI/ISI trap vectors */
	li	5,SI1SIZE
	bl	copy

	mtlr	11
	li	3,EXC_IMISS		/* save MISS trap vectors */
	li	5,SI2SIZE

copy:
	li	6,CACHELINE
1:
	lwz	7,0(3)
	lwz	8,4(3)
	lwz	9,8(3)
	lwz	10,12(3)
	stw	7,0(4)
	stw	8,4(4)
	stw	9,8(4)
	stw	10,12(4)
	lwz	7,16(3)
	lwz	8,20(3)
	lwz	9,24(3)
	lwz	10,28(3)
	stw	7,16(4)
	stw	8,20(4)
	stw	9,24(4)
	stw	10,28(4)
	dcbst	0,4
	icbi	0,4
	add	3,3,6
	add	4,4,6
	subf.	5,6,5
	bgt	1b

	dcbst	0,4
	icbi	0,4

	sync
	isync

	blr

/*
 * Restore everyting related to the mmu from the saveare pointed to by r3.
 */
restoremmu:

	li	4,0			/* first, invalidate BATs */
	mtibatu	0,4
	mtibatu	1,4
	mtibatu	2,4
	mtibatu	3,4
	mtdbatu	0,4
	mtdbatu	1,4
	mtdbatu	2,4
	mtdbatu	3,4

	lwz	4,0(3)
	mtibatl	0,4			/* restore BATs */
	lwz	4,4(3)
	mtibatu	0,4
	lwz	4,8(3)
	mtibatl	1,4
	lwz	4,12(3)
	mtibatu	1,4
	lwz	4,16(3)
	mtibatl	2,4
	lwz	4,20(3)
	mtibatu	2,4
	lwz	4,24(3)
	mtibatl	3,4
	lwz	4,28(3)
	mtibatu	3,4
	lwz	4,32(3)
	mtdbatl	0,4
	lwz	4,36(3)
	mtdbatu	0,4
	lwz	4,40(3)
	mtdbatl	1,4
	lwz	4,44(3)
	mtdbatu	1,4
	lwz	4,48(3)
	mtdbatl	2,4
	lwz	4,52(3)
	mtdbatu	2,4
	lwz	4,56(3)
	mtdbatl	3,4
	lwzu	4,60(3)
	mtdbatu	3,4

	li	4,0			/* restore SRs */
1:
	lwzu	5,4(3)
	addis	4,4,-0x10000000@ha
	or.	4,4,4
	mtsrin	5,4
	bne	1b

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

	addi	3,3,24
	mflr	11
	li	4,EXC_DSI		/* restore DSI/ISI trap vectors */
	li	5,SI1SIZE
	bl	copy

	li	4,EXC_IMISS		/* restore MISS trap vectors */
	li	5,SI2SIZE
	bl	copy

	/* tlbia */
	sync
	li	3,0x40
	mtctr	3
	li	4,0
    1:
	tlbie	4
	addi	4,4,0x1000
	bdnz	1b
	sync
	tlbsync
	sync

	mtlr	11
	blr
