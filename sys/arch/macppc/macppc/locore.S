/*	$OpenBSD: locore.S,v 1.41 2008/07/28 19:08:46 miod Exp $	*/
/*	$NetBSD: locore.S,v 1.2 1996/10/16 19:33:09 ws Exp $	*/

/*
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

#include <sys/syscall.h>

#include <machine/asm.h>
#include <machine/param.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>

#define GET_CPUINFO(r)	mfsprg r,0

#define	INTSTK		(8*1024)	/* 8K interrupt stack */
#define	SPILLSTK	(1*1024)
#define	DDBSTK		(7*1024)

/*
 * Globals
 */
	.globl	_C_LABEL(esym),_C_LABEL(proc0paddr)
	.type	_C_LABEL(esym),@object
	.type	_C_LABEL(proc0paddr),@object
	.data
_C_LABEL(esym):		.long	0	/* end of symbol table */
_C_LABEL(proc0paddr):	.long	0	/* proc0 p_addr */

fwargsave:
			.long 0
			.long 0

       .globl  _C_LABEL(hid0_idle),
       .type   _C_LABEL(hid0_idle),@object
_C_LABEL(hid0_idle):   .long   0       /* hid0 bits to set on idle, DOZE/NAP *
/*
 * Startup entry
 */
_ENTRY(_C_LABEL(kernel_text))
_ENTRY(_ASM_LABEL(start))
/* arguments to start
 * r1 - stack provided by firmware/bootloader
 * r3 - unused
 * r4 - unused
 * r5 - firmware pointer (NULL for PPC1bug)
 * r6 - arg list
 * r7 - length
 */
	.globl	start
	.type start,@function
start:
	lis	%r3, fwargsave@ha
	stw	%r6, fwargsave@l(%r3)
	stw	%r7, fwargsave@l+4(%r3)
	bl	ofw_init
	li	%r0,0
	mtmsr	%r0			/* Disable FPU/MMU/exceptions */
	isync

/* compute end of kernel memory */
	lis	%r8,_end@ha
	addi	%r8,%r8,_end@l
	lis	%r3, fwargsave@ha
	lwz	%r6, fwargsave@l(%r3)
	lwz	%r7, fwargsave@l+4(%r3)
#if defined(DDB)
	cmpwi	%r6, 0
	beq	1f
	add	%r9,%r6,%r7
	lwz	%r9, -4(%r9)
	cmpwi   %r9,0
	beq	1f
	lis	%r8,_C_LABEL(esym)@ha
	stw	%r9,_C_LABEL(esym)@l(%r8)
	mr	%r8, %r9
1:
#endif
	li	%r9,PGOFSET
	add	%r8,%r8,%r9
	andc	%r8,%r8,%r9
	lis	%r9,_C_LABEL(cpu_info)@ha
	addi	%r9,%r9,_C_LABEL(cpu_info)@l
	mtsprg	0,%r9
	addi	%r8,%r8,INTSTK
	stw	%r8,CI_INTSTK(%r9)
	li	%r0,-1
	stw	%r0,CI_INTRDEPTH(%r9)
	addi	%r8,%r8,SPILLSTK+DDBSTK	/* leave room for spillstk and ddbstk */
	lis	%r9,_C_LABEL(proc0paddr)@ha
	stw	%r8,_C_LABEL(proc0paddr)@l(%r9)
	addi	%r1,%r8,USPACE-FRAMELEN	/* stackpointer for proc0 */
	mr	%r4,%r1			/* end of mem reserved for kernel */
	li	%r0,0
	stwu	%r0,-16(%r1)		/* end of stack chain */

	lis	%r3,start@ha
	addi	%r3,%r3,start@l
	mr	%r5,%r6			/* args string */
	bl	_C_LABEL(initppc)
	bl	_C_LABEL(main)
	b	_C_LABEL(OF_exit)

#ifdef MULTIPROCESSOR
_ENTRY(_C_LABEL(cpu_spinup_trampoline))
	lis	%r3,_C_LABEL(cpu_hatch_stack)@ha
	lwz	%r1,_C_LABEL(cpu_hatch_stack)@l(%r3)

	b	_C_LABEL(cpu_hatch)
	/* NOTREACHED */
#endif

/*
 * void cpu_switchto(struct proc *old, struct proc *new)
 * Switch from "old" proc to "new".
 */
_ENTRY(_C_LABEL(cpu_switchto_asm))
	mflr	%r0			/* save lr */
	stw	%r0,4(%r1)
	stwu	%r1,-16(%r1)
	stw	%r31,12(%r1)
	stw	%r30,8(%r1)

/*
 * r3 - old proc
 * r4 -	new proc
 * r5 - cpuinfo
 */
	GET_CPUINFO(%r5)

	li	%r31,SONPROC
	stb	%r31,P_STAT(%r4)

	or.	%r3,%r3,%r3		/* old process was exiting? */
	beq	switch_exited

	mfsr	%r10,PPC_USER_SR	/* save PPC_USER_SR for copyin/copyout*/
	mfcr	%r11			/* save cr */
	mr	%r12,%r2		/* save r2 */
	stwu	%r1,-SFRAMELEN(%r1)	/* still running on old stack */
	stmw	%r10,8(%r1)
	lwz	%r31,P_ADDR(%r3)
	stw	%r1,PCB_SP(%r31)	/* save SP */

switch_exited:
	/* disable interrupts while actually switching */
	mfmsr	%r30
	andi.	%r30,%r30,~PSL_EE@l
	mtmsr	%r30

	stw	%r4,CI_CURPROC(%r5)	/* record new process */

#ifdef MULTIPROCESSOR
	stw	%r5,P_CPU(%r4)
#endif

	lwz	%r31,P_ADDR(%r4)
	stw	%r31,CI_CURPCB(%r5)	/* indicate new pcb */

	lwz	%r6,PCB_PMR(%r31)

	/* save real pmap pointer for spill fill */
	stwu	%r6,CI_CURPM(%r5)
	stwcx.	%r6,%r0,%r5		/* clear possible reservation */

	addic.	%r6,%r6,64
	li	%r5,0

	lwz	%r1,PCB_SP(%r31)	/* get new procs SP */

	ori	%r30,%r30,PSL_EE	/* interrupts are okay again */
	mtmsr	%r30

	lmw	%r10,8(%r1)		/* get other regs */
	lwz	%r1,0(%r1)		/* get saved SP */
	mr	%r2,%r12		/* get saved r2 */
	mtcr	%r11			/* get saved cr */
	isync
	mtsr	PPC_USER_SR,%r10	/* get saved PPC_USER_SR */
	isync

	lwz	%r31,12(%r1)
	lwz	%r30,8(%r1)
	addi	%r1,%r1,16
	lwz	%r0,4(%r1)
	mtlr	%r0
	blr

_ENTRY(_C_LABEL(cpu_idle_enter))
	/* must disable external interrupts during idle queue checking */
	mfmsr	%r3
	andi.	%r3,%r3,~PSL_EE@l
	mtmsr	%r3
	blr

_ENTRY(_C_LABEL(cpu_idle_cycle))
	/*
	 * interrupts were disabled in cpu_idle_enter, but must be enabled
	 * for sleeping
	 */
	lis	%r4, _C_LABEL(hid0_idle)@ha
	lwz	%r4, _C_LABEL(hid0_idle)@l(%r4)
	mfmsr	%r3
	//* if hid0_idle is 0, no low power */
	cmpwi	%r4, 0
	beq	idledone
	/* set HID0_(HID|NAP|DOZ) as appropriate in hid0 */
	mfspr	%r5, 1008
	or	%r5, %r5, %r4
	mtspr	1008,	%r5
	isync
	/* enable interrupts, required before setting POW */
	ori	%r5,%r3,PSL_EE@l
	mtmsr	%r5
	oris	%r5, %r5, PSL_POW@h
	sync
	/* low power mode */
	mtmsr	%r5
	sync
	isync

	/*
	 * restore interrupts to disabled, so hid0 is only modified with
	 * interrupts disabled.
	 */
	mtmsr %r3
	isync

	/* clear HID0_(HID|NAP|DOZ) since sleeping is over */
	mfspr	%r5, 1008
	andc	%r5, %r5, %r4
	mtspr	1008, %r5
idledone:
	/*
	 * enable interrupts for a small window here to catch machines
	 * that do not have any sleep mode
	 */
	ori     %r5,%r3,PSL_EE@l
	mtmsr	%r5
	isync
	mtmsr	%r3
	blr


_ENTRY(_C_LABEL(cpu_idle_leave))
	/* enable interrupts disabled in cpu_idle_enter. */
	mfmsr	%r3
	ori	%r3,%r3,PSL_EE@l
	mtmsr	%r3
	blr


/*
 * This code gets copied to all the trap vectors
 * except ISI/DSI, ALI, and the interrupts
 */
	.text
	.globl	_C_LABEL(trapcode),_C_LABEL(trapsize)
	.type	_C_LABEL(trapcode),@function
	.type	_C_LABEL(trapsize),@object
_C_LABEL(trapcode):
	mtsprg	1,%r1			/* save SP */
nop32_1s:
	mfmsr	%r1
	clrldi	%r1,%r1,1
	mtmsrd	%r1
nop32_1e:
	GET_CPUINFO(%r1)
	stmw	%r28,CI_TEMPSAVE(%r1)	/* free r28-r31 */
	mflr	%r28			/* save LR */
	mfcr	%r29			/* save CR */
	mfsprg	%r1,1			/* restore SP */

	/* Test whether we already had PR set */
	mfsrr1	%r31
	mtcr	%r31
	bc	4,17,1f			/* branch if PSL_PR is clear */
	GET_CPUINFO(%r1)
	lwz	%r1,CI_CURPCB(%r1)
	addi	%r1,%r1,USPACE		/* stack is top of user struct */
1:
	bla	s_trap
_C_LABEL(trapsize) =	.-_C_LABEL(trapcode)

/*
 * For ALI: has to save DSISR and DAR
 */
	.globl	_C_LABEL(alitrap),_C_LABEL(alisize)
_C_LABEL(alitrap):
	mtsprg	1,%r1			/* save SP */
nop32_2s:
	mfmsr	%r1
	clrldi	%r1,%r1,1
	mtmsrd	%r1
nop32_2e:
	GET_CPUINFO(%r1)
	stmw	%r28,CI_TEMPSAVE(%r1)	/* free r28-r31 */
	mfdar	%r30
	mfdsisr	%r31
	stmw	%r30,CI_TEMPSAVE+16(%r1)
	mflr	%r28			/* save LR */
	mfcr	%r29			/* save CR */
	mfsprg	%r1,1			/* restore SP */

	/* Test whether we already had PR set */
	mfsrr1	%r31
	mtcr	%r31
	bc	4,17,1f			/* branch if PSL_PR is clear */
	GET_CPUINFO(%r1)
	lwz	%r1,CI_CURPCB(%r1)
	addi	%r1,%r1,USPACE		/* stack is top of user struct */
1:
	bla	s_trap
_C_LABEL(alisize) =	.-_C_LABEL(alitrap)

/*
 * Similar to the above for DSI
 * Has to handle BAT spills
 * and standard pagetable spills
 */
	.globl	_C_LABEL(dsitrap),_C_LABEL(dsisize)
	.type	_C_LABEL(dsitrap),@function
	.type	_C_LABEL(dsisize),@object
_C_LABEL(dsitrap):
	mtsprg	1,%r1
	GET_CPUINFO(%r1)
	stmw	%r28,CI_DISISAVE(%r1)	/* free r28-r31 */
nop32_3s:
	mfmsr	%r28
	clrldi	%r28,%r28,1
	mtmsrd	%r28
nop32_3e:
	mfsprg	%r1,1
	mfcr	%r29			/* save CR */
	mfxer	%r30			/* save XER */
	mtsprg	2,%r30			/* in SPRG2 */
	mfsrr1	%r31			/* test kernel mode */
nop64_1s:
	mtcr	%r31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfdar	%r31			/* get fault address */
	rlwinm	%r31,%r31,7,25,28	/* get segment * 8 */
	addis	%r31,%r31,_C_LABEL(battable)@ha
	lwz	%r30,_C_LABEL(battable)@l(%r31)	/* get batu */
	mtcr	%r30
	bc	4,30,1f			/* branch if supervisor valid is false */
	lwz	%r31,_C_LABEL(battable)+4@l(%r31)	/* get batl */
/* We randomly use the highest two bat registers here */
	mftb	%r28
	andi.	%r28,%r28,1
	bne	2f
	mtdbatu	2,%r30
	mtdbatl	2,%r31
	b	3f
2:
	mtdbatu	3,%r30
	mtdbatl	3,%r31
3:
	mfsprg	%r30,2			/* restore XER */
	mtxer	%r30
	mtcr	%r29			/* restore CR */
	mtsprg	1,%r1
	GET_CPUINFO(%r1)
	lmw	%r28,CI_DISISAVE(%r1)	/* restore r28-r31 */
	mfsprg	1,%r1
	rfi				/* return to trapped code */
1:
nop64_1e:
	mflr	%r28			/* save LR */
	bla	s_dsitrap
_C_LABEL(dsisize) =	.-_C_LABEL(dsitrap)

/*
 * Similar to the above for ISI
 */
	.globl	_C_LABEL(isitrap),_C_LABEL(isisize)
	.type	_C_LABEL(isitrap),@function
	.type	_C_LABEL(isisize),@object
_C_LABEL(isitrap):
	mtsprg	1,%r1                   /* save SP */
nop32_4s:
	mfmsr	%r1
	clrldi	%r1,%r1,1
	mtmsrd	%r1
nop32_4e:
	GET_CPUINFO(%r1)
	stmw	%r28,CI_DISISAVE(%r1)	/* free r28-r31 */
	mflr	%r28			/* save LR */
	mfcr	%r29			/* save CR */
	mfsrr1	%r31			/* test kernel mode */
	mfsprg	%r1,1                   /* restore SP */
nop64_2s:
	mtcr	%r31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfsrr0	%r31			/* get fault address */
	rlwinm	%r31,%r31,7,25,28		/* get segment * 8 */
	addis	%r31,%r31,_C_LABEL(battable)@ha
	lwz	%r30,_C_LABEL(battable)@l(%r31)	/* get batu */
	mtcr	%r30
	bc	4,30,1f			/* branch if supervisor valid is false */
	mtibatu	3,%r30
	lwz	%r30,_C_LABEL(battable)+4@l(%r31)	/* get batl */
	mtibatl	3,%r30
	mtcr	%r29			/* restore CR */
	mtsprg	1,%r1
	GET_CPUINFO(%r1)
	lmw	%r28,CI_DISISAVE(%r1)	/* restore r28-r31 */
	mfsprg	%r1,1
	rfi				/* return to trapped code */
1:
nop64_2e:
	bla	s_isitrap
_C_LABEL(isisize) =	.-_C_LABEL(isitrap)

/*
 * This one for the external interrupt handler.
 */
	.globl	_C_LABEL(extint),_C_LABEL(extsize)
	.type	_C_LABEL(extint),@function
	.type	_C_LABEL(extsize),@object
_C_LABEL(extint):
	mtsprg	1,%r1			/* save SP */
nop32_5s:
	mfmsr	%r1
	clrldi	%r1,%r1,1
	mtmsrd	%r1
nop32_5e:
	GET_CPUINFO(%r1)
	stmw	%r28,CI_TEMPSAVE(%r1)	/* free r28-r31 */
	mflr	%r28			/* save LR */
	mfcr	%r29			/* save CR */
	mfxer	%r30			/* save XER */
	lwz	%r31,CI_INTRDEPTH(%r1)	/* were we already running on intstk? */
	addic.	%r31,%r31,1
	stw	%r31,CI_INTRDEPTH(%r1)
	lwz	%r1,CI_INTSTK(%r1)	/* get interrupt stack */
	beq	1f
	mfsprg	%r1,1			/* yes, get old SP */
1:
	ba	extintr
_C_LABEL(extsize) =	.-_C_LABEL(extint)

/*
 * And this one for the decrementer interrupt handler.
 */
	.globl	_C_LABEL(decrint),_C_LABEL(decrsize)
	.type	_C_LABEL(decrint),@function
	.type	_C_LABEL(decrsize),@object
_C_LABEL(decrint):
	mtsprg	1,%r1			/* save SP */
nop32_6s:
	mfmsr	%r1
	clrldi	%r1,%r1,1
	mtmsrd	%r1
nop32_6e:
	GET_CPUINFO(%r1)
	stmw	%r28,CI_TEMPSAVE(%r1)	/* free r28-r31 */
	mflr	%r28			/* save LR */
	mfcr	%r29			/* save CR */
	mfxer	%r30			/* save XER */
	lwz	%r31,CI_INTRDEPTH(%r1)	/* were we already running on intstk? */
	addic.	%r31,%r31,1
	stw	%r31,CI_INTRDEPTH(%r1)
	lwz	%r1,CI_INTSTK(%r1)	/* get interrupt stack */
	beq	1f
	mfsprg	%r1,1			/* yes, get old SP */
1:
	ba	decrintr
_C_LABEL(decrsize) =	.-_C_LABEL(decrint)

/*
 * Now the tlb software load for 603 processors:
 * (Code essentially from the 603e User Manual, Chapter 5)
 */
#define	DMISS	976
#define	DCMP	977
#define	HASH1	978
#define	HASH2	979
#define	IMISS	980
#define	ICMP	981
#define	RPA	982

#define	bdneq	bdnzf 2,
#define	tlbli	.long	0x7c0007e4+0x800*
#define	tlbld	.long	0x7c0007a4+0x800*

	.globl	_C_LABEL(tlbimiss),_C_LABEL(tlbimsize)
	.type	_C_LABEL(tlbimiss),@function
	.type	_C_LABEL(tlbimsize),@object
_C_LABEL(tlbimiss):
	mfspr	%r2,HASH1		/* get first pointer */
	li	%r1,8
	mfctr	%r0			/* save counter */
	mfspr	%r3,ICMP		/* get first compare value */
	addi	%r2,%r2,-8		/* predec pointer */
1:
	mtctr	%r1			/* load counter */
2:
	lwzu	%r1,8(%r2)		/* get next pte */
	cmpl	0,%r1,%r3		/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	%r1,4(%r2)		/* load tlb entry lower word */
	andi.	%r3,%r1,8		/* check G-bit */
	bne	4f			/* if guarded, take ISI */
	mtctr	%r0			/* restore counter */
	mfspr	%r0,IMISS		/* get the miss address for the tlbli */
	mfsrr1	%r3			/* get the saved cr0 bits */
	mtcrf	0x80,%r3		/* and restore */
	ori	%r1,%r1,0x100		/* set the reference bit */
	mtspr	RPA,%r1			/* set the pte */
	srwi	%r1,%r1,8		/* get byte 7 of pte */
	tlbli	0			/* load the itlb */
	stb	%r1,6(%r2)		/* update page table */
	rfi

3:	/* not found in pteg */
	andi.	%r1,%r3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	%r2,HASH2		/* get the second pointer */
	ori	%r3,%r3,0x40		/* change the compare value */
	li	%r1,8
	addi	%r2,%r2,-8		/* predec pointer */
	b	1b
4:	/* guarded */
	mfsrr1	%r3
	andi.	%r2,%r3,0xffff		/* clean upper srr1 */
	addis	%r2,%r2,0x800		/* set srr<4> to flag prot violation */
	b	6f
5:	/* not found anywhere */
	mfsrr1	%r3
	andi.	%r2,%r3,0xffff		/* clean upper srr1 */
	addis	%r2,%r2,0x4000		/* set srr1<1> to flag pte not found */
6:
	mtctr	%r0			/* restore counter */
	mtsrr1	%r2
	mfmsr	%r0
	xoris	%r0,%r0,2		/* flip the msr<tgpr> bit */
	mtcrf	0x80,%r3		/* restore cr0 */
	mtmsr	%r0			/* now with native gprs */
	isync
	ba	EXC_ISI
_C_LABEL(tlbimsize) =	.-_C_LABEL(tlbimiss)

	.globl	_C_LABEL(tlbdlmiss),_C_LABEL(tlbdlmsize)
	.type	_C_LABEL(tlbdlmiss),@function
	.type	_C_LABEL(tlbdlmsize),@object
_C_LABEL(tlbdlmiss):
	mfspr	%r2,HASH1		/* get first pointer */
	li	%r1,8
	mfctr	%r0			/* save counter */
	mfspr	%r3,DCMP		/* get first compare value */
	addi	%r2,%r2,-8		/* predec pointer */
1:
	mtctr	%r1			/* load counter */
2:
	lwzu	%r1,8(%r2)		/* get next pte */
	cmpl	0,%r1,%r3		/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	%r1,4(%r2)		/* load tlb entry lower word */
	mtctr	%r0			/* restore counter */
	mfspr	%r0,DMISS		/* get the miss address for the tlbld */
	mfsrr1	%r3			/* get the saved cr0 bits */
	mtcrf	0x80,%r3		/* and restore */
	ori	%r1,%r1,0x100		/* set the reference bit */
	mtspr	RPA,%r1			/* set the pte */
	srwi	%r1,%r1,8		/* get byte 7 of pte */
	tlbld	0			/* load the dtlb */
	stb	%r1,6(%r2)		/* update page table */
	rfi

3:	/* not found in pteg */
	andi.	%r1,%r3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	%r2,HASH2		/* get the second pointer */
	ori	%r3,%r3,0x40		/* change the compare value */
	li	%r1,8
	addi	%r2,%r2,-8		/* predec pointer */
	b	1b
5:	/* not found anywhere */
	mfsrr1	%r3
	lis	%r1,0x4000		/* set dsisr<1> to flag pte not found */
	mtctr	%r0			/* restore counter */
	andi.	%r2,%r3,0xffff		/* clean upper srr1 */
	mtsrr1	%r2
	mtdsisr	%r1			/* load the dsisr */
	mfspr	%r1,DMISS		/* get the miss address */
	mtdar	%r1			/* put in dar */
	mfmsr	%r0
	xoris	%r0,%r0,2		/* flip the msr<tgpr> bit */
	mtcrf	0x80,%r3		/* restore cr0 */
	mtmsr	%r0			/* now with native gprs */
	isync
	ba	EXC_DSI
_C_LABEL(tlbdlmsize) =	.-_C_LABEL(tlbdlmiss)

	.globl	_C_LABEL(tlbdsmiss),_C_LABEL(tlbdsmsize)
	.type	_C_LABEL(tlbdsmiss),@function
	.type	_C_LABEL(tlbdsmsize),@object
_C_LABEL(tlbdsmiss):
	mfspr	%r2,HASH1		/* get first pointer */
	li	%r1,8
	mfctr	%r0			/* save counter */
	mfspr	%r3,DCMP		/* get first compare value */
	addi	%r2,%r2,-8		/* predec pointer */
1:
	mtctr	%r1			/* load counter */
2:
	lwzu	%r1,8(%r2)		/* get next pte */
	cmpl	0,%r1,%r3		/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	%r1,4(%r2)		/* load tlb entry lower word */
	andi.	%r3,%r1,0x80		/* check the C-bit */
	beq	4f
5:
	mtctr	%r0			/* restore counter */
	mfspr	%r0,DMISS		/* get the miss address for the tlbld */
	mfsrr1	%r3			/* get the saved cr0 bits */
	mtcrf	0x80,%r3		/* and restore */
	mtspr	RPA,%r1			/* set the pte */
	tlbld	0			/* load the dtlb */
	rfi

3:	/* not found in pteg */
	andi.	%r1,%r3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	%r2,HASH2		/* get the second pointer */
	ori	%r3,%r3,0x40		/* change the compare value */
	li	%r1,8
	addi	%r2,%r2,-8		/* predec pointer */
	b	1b
4:	/* found, but C-bit = 0 */
	rlwinm.	%r3,%r1,30,0,1		/* test PP */
	bge-	7f
	andi.	%r3,%r1,1
	beq+	8f
9:	/* found, but protection violation (PP==00)*/
	mfsrr1	%r3
	lis	%r1,0xa00	/* indicate protection violation on store */
	b	1f
7:	/* found, PP=1x */
	mfspr	%r3,DMISS		/* get the miss address */
	mfsrin	%r1,%r3			/* get the segment register */
	mfsrr1	%r3
	rlwinm	%r3,%r3,18,31,31	/* get PR-bit */
	rlwnm.	%r2,%r2,3,1,1		/* get the key */
	bne-	9b			/* protection violation */
8:	/* found, set reference/change bits */
	lwz	%r1,4(%r2)		/* reload tlb entry */
	ori	%r1,%r1,0x180
	sth	%r1,6(%r2)
	b	5b
5:	/* not found anywhere */
	mfsrr1	%r3
	lis	%r1,0x4200		/* set dsisr<1> to flag pte not found */
					/* dsisr<6> to flag store */
1:
	mtctr	%r0			/* restore counter */
	andi.	%r2,%r3,0xffff		/* clean upper srr1 */
	mtsrr1	%r2
	mtdsisr	%r1			/* load the dsisr */
	mfspr	%r1,DMISS		/* get the miss address */
	mtdar	%r1			/* put in dar */
	mfmsr	%r0
	xoris	%r0,%r0,2		/* flip the msr<tgpr> bit */
	mtcrf	0x80,%r3		/* restore cr0 */
	mtmsr	%r0			/* now with native gprs */
	isync
	ba	EXC_DSI
_C_LABEL(tlbdsmsize) =	.-_C_LABEL(tlbdsmiss)

#ifdef DDB
/*
 * In case of DDB we want a separate trap catcher for it
 */
	.globl	_C_LABEL(ddblow),_C_LABEL(ddbsize)
_C_LABEL(ddblow):
	mtsprg	1,%r1			/* save SP */
nop32_7s:
	mfmsr	%r1
	clrldi	%r1,%r1,1
	mtmsrd	%r1
nop32_7e:
	GET_CPUINFO(%r1)
	stmw	%r28,CI_DDBSAVE(%r1)	/* free r28-r31 */
	mflr	%r28			/* save LR */
	mfcr	%r29			/* save CR */
	GET_CPUINFO(%r30)
	lwz	%r30,CI_INTSTK(%r30)	/* get interrupt stack */
	addi	%r1,%r30,(SPILLSTK+DDBSTK)
	bla	ddbtrap
_C_LABEL(ddbsize) =	.-_C_LABEL(ddblow)
#endif  /* DDB */

#define HID_IDLE_CHECK(sr1,sr2,sr3,rSRR0)				\
	/* detect if interrupt occurred from idle */			\
	mfspr	sr1, 1008;						\
	lis	sr2,_C_LABEL(hid0_idle)@ha;				\
	lwz	sr2,_C_LABEL(hid0_idle)@l(sr2);				\
	and.	sr3,sr1,sr2;						\
	andc	sr1,sr1,sr2;						\
	beq+	1f;							\
	mtspr	1008, sr1;						\
	lis	rSRR0,_C_LABEL(idledone)@ha;				\
	addi	rSRR0,rSRR0,_C_LABEL(idledone)@l;			\
1:

/*
 * FRAME_SETUP assumes:
 *	SPRG1		SP (1)
 *	savearea	r28-r31,DAR,DSISR	(DAR & DSISR only for DSI traps)
 *	28		LR
 *	29		CR
 *	1		kernel stack
 *	LR		trap type
 *	SRR0/1		as at start of trap
 */
#define	FRAME_SETUP(savearea)						\
/* Have to enable translation to allow access of kernel stack: */	\
	GET_CPUINFO(%r31);						\
	mfsrr0	%r30;							\
	stw	%r30,savearea+24(%r31);					\
	mfsrr1	%r30;							\
	stw	%r30,savearea+28(%r31);					\
	/* load all kernel segment registers. */			\
	lis	%r31,_C_LABEL(kernel_pmap_)@ha;				\
	addi	%r31,%r31,_C_LABEL(kernel_pmap_)@l;			\
	lwz	%r30,0(%r31);	mtsr	0,%r30;				\
	lwz	%r30,4(%r31);	mtsr	1,%r30;				\
	lwz	%r30,8(%r31);	mtsr	2,%r30;				\
	lwz	%r30,12(%r31);	mtsr	3,%r30;				\
	lwz	%r30,16(%r31);	mtsr	4,%r30;				\
	lwz	%r30,20(%r31);	mtsr	5,%r30;				\
	lwz	%r30,24(%r31);	mtsr	6,%r30;				\
	lwz	%r30,28(%r31);	mtsr	7,%r30;				\
	lwz	%r30,32(%r31);	mtsr	8,%r30;				\
	lwz	%r30,36(%r31);	mtsr	9,%r30;				\
	lwz	%r30,40(%r31);	mtsr	10,%r30;			\
	lwz	%r30,44(%r31);	mtsr	11,%r30;			\
	lwz	%r30,48(%r31);	mtsr	12,%r30;			\
/*	lwz	%r30,52(%r31);	mtsr 13,%r30; - dont load user SR - XXX? */ \
	lwz	%r30,56(%r31);	mtsr	14,%r30;			\
	lwz	%r30,60(%r31);	mtsr	15,%r30;			\
	mfmsr	%r30;							\
	ori	%r30,%r30,(PSL_DR|PSL_IR);				\
	mtmsr	%r30;							\
	isync;								\
	mfsprg	%r31,1;							\
	stwu	%r31,-FRAMELEN(%r1);					\
	stw	%r0,FRAME_0+8(%r1);					\
	stw	%r31,FRAME_1+8(%r1);					\
	stw	%r2,FRAME_2+8(%r1);					\
	stw	%r28,FRAME_LR+8(%r1);					\
	stw	%r29,FRAME_CR+8(%r1);					\
	GET_CPUINFO(%r2);						\
	lmw	%r28,savearea(%r2);					\
	stmw	%r3,FRAME_3+8(%r1);					\
	lmw	%r28,savearea+16(%r2);					\
	mfxer	%r3;							\
	mfctr	%r4;							\
	mflr	%r5;							\
	andi.	%r5,%r5,0xff00;						\
	stw	%r3,FRAME_XER+8(%r1);					\
	stw	%r4,FRAME_CTR+8(%r1);					\
	stw	%r5,FRAME_EXC+8(%r1);					\
	stw	%r28,FRAME_DAR+8(%r1);					\
	stw	%r29,FRAME_DSISR+8(%r1);				\
	HID_IDLE_CHECK(%r5,%r6,%r0,%r30)				\
	stw	%r30,FRAME_SRR0+8(%r1);					\
	stw	%r31,FRAME_SRR1+8(%r1)

#define	FRAME_LEAVE(savearea)						\
/* Now restore regs: */							\
	lwz	%r2,FRAME_SRR0+8(%r1);					\
	lwz	%r3,FRAME_SRR1+8(%r1);					\
	lwz	%r4,FRAME_CTR+8(%r1);					\
	lwz	%r5,FRAME_XER+8(%r1);					\
	lwz	%r6,FRAME_LR+8(%r1);					\
	GET_CPUINFO(%r7);						\
	stw	%r2,savearea(%r7);					\
	stw	%r3,savearea+4(%r7);					\
	lwz	%r7,FRAME_CR+8(%r1);					\
	mtctr	%r4;							\
	mtxer	%r5;							\
	mtlr	%r6;							\
	mtsprg	1,%r7;			/* save cr */			\
	lmw	%r2,FRAME_2+8(%r1);					\
	lwz	%r0,FRAME_0+8(%r1);					\
	lwz	%r1,FRAME_1+8(%r1);					\
	mtsprg	2,%r2;			/* save r2 & r3 */		\
	mtsprg	3,%r3;							\
/* Disable translation, machine check and recoverability: */		\
	mfmsr	%r2;							\
	lis	%r3,(PSL_DR|PSL_IR|PSL_ME|PSL_RI)@ha;			\
	addi	%r3,%r3,(PSL_DR|PSL_IR|PSL_ME|PSL_RI)@l;		\
	andc	%r2,%r2,%r3;						\
	mtmsr	%r2;							\
	isync;								\
/* Decide whether we return to user mode: */				\
	GET_CPUINFO(%r2);						\
	lwz	%r3,savearea+4(%r2);					\
	mtcr	%r3;							\
	bc	4,17,1f;		/* branch if PSL_PR is false */	\
/* Restore user & kernel access SR: */					\
	lwz	%r2,CI_CURPM(%r2);	/* get real address of pmap */	\
	lwz	%r3,0(%r2);	mtsr	0,%r3;				\
	lwz	%r3,4(%r2);	mtsr	1,%r3;				\
	lwz	%r3,8(%r2);	mtsr	2,%r3;				\
	lwz	%r3,12(%r2);	mtsr	3,%r3;				\
	lwz	%r3,16(%r2);	mtsr	4,%r3;				\
	lwz	%r3,20(%r2);	mtsr	5,%r3;				\
	lwz	%r3,24(%r2);	mtsr	6,%r3;				\
	lwz	%r3,28(%r2);	mtsr	7,%r3;				\
	lwz	%r3,32(%r2);	mtsr	8,%r3;				\
	lwz	%r3,36(%r2);	mtsr	9,%r3;				\
	lwz	%r3,40(%r2);	mtsr	10,%r3;				\
	lwz	%r3,44(%r2);	mtsr	11,%r3;				\
	lwz	%r3,48(%r2);	mtsr	12,%r3;				\
	lwz	%r3,52(%r2);	mtsr	13,%r3;				\
	lwz	%r3,56(%r2);	mtsr	14,%r3;				\
	lwz	%r3,60(%r2);	mtsr	15,%r3;				\
1:	mfsprg	%r2,1;			/* restore cr */		\
	mtcr	%r2;							\
	GET_CPUINFO(%r2);						\
	lwz	%r3,savearea(%r2);					\
	mtsrr0	%r3;							\
	lwz	%r3,savearea+4(%r2);					\
	mtsrr1	%r3;							\
	mfsprg	%r2,2;			/* restore r2 & r3 */		\
	mfsprg	%r3,3

/*
 * Preamble code for DSI/ISI traps
 */
disitrap:
	GET_CPUINFO(%r1)
	lmw	%r30,CI_DISISAVE(%r1)
	stmw	%r30,CI_TEMPSAVE(%r1)
	lmw	%r30,CI_DISISAVE+8(%r1)
	stmw	%r30,CI_TEMPSAVE+8(%r1)
	mfdar	%r30
	mfdsisr	%r31
	stmw	%r30,CI_TEMPSAVE+16(%r1)
realtrap:
	/* Test whether we already had PR set */
	mfsrr1	%r1
	mtcr	%r1
	/* restore SP (might have been overwritten) */
	mfsprg	%r1,1
	bc	4,17,s_trap		/* branch if PSL_PR is false */
	GET_CPUINFO(%r1)
	lwz	%r1,CI_CURPCB(%r1)
	addi	%r1,%r1,USPACE		/* stack is top of user struct */
/*
 * Now the common trap catching code.
 */
s_trap:
	FRAME_SETUP(CI_TEMPSAVE)
/* Now we can recover interrupts again: */
	mfmsr	%r7
	mfsrr1	%r31
	andi.	%r31,%r31,PSL_EE	/* restore EE from previous context */
	or	%r7,%r7,%r31
	ori	%r7,%r7,(PSL_ME|PSL_RI)
	mtmsr	%r7
	isync
/* Call C trap code: */
trapagain:
	addi	%r3,%r1,8
	bl	_C_LABEL(trap)
trapexit:
/* Disable interrupts: */
	mfmsr	%r3
	andi.	%r3,%r3,~PSL_EE@l
	mtmsr	%r3
/* Test AST pending: */
	lwz	%r5,FRAME_SRR1+8(%r1)
	mtcr	%r5
	bc	4,17,1f			/* branch if PSL_PR is false */
	GET_CPUINFO(%r3)
	lwz	%r4,CI_CURPROC(%r3)
	lwz	%r4,P_MD_ASTPENDING(%r4)
	andi.	%r4,%r4,1
	beq	1f
	li	%r6,EXC_AST
	stw	%r6,FRAME_EXC+8(%r1)
	b	trapagain
1:
	FRAME_LEAVE(CI_TEMPSAVE)
rfi1:	rfi

/*
 * Child comes here at the end of a fork.
 * Mostly similar to the above.
 */
	.globl	_C_LABEL(fork_trampoline)
	.type	_C_LABEL(fork_trampoline),@function
_C_LABEL(fork_trampoline):
#ifdef MULTIPROCESSOR
	bl	_C_LABEL(proc_trampoline_mp)
#endif
	li	%r3,0
	bl	_C_LABEL(lcsplx)
	mtlr	%r31
	mr	%r3,%r30
	blrl				/* jump indirect to r31 */
	b	trapexit

/*
 * DSI second stage fault handler
 */
s_dsitrap:
	mfdsisr	%r31			/* test if this is spill fault */
	mtcr	%r31
	mtsprg	1,%r1			/* save SP */
	bc	4,1,disitrap		/* branch if table miss is false */
	GET_CPUINFO(%r30)
	lwz	%r30,CI_INTSTK(%r30)	/* get interrupt stack */
	addi	%r1,%r30,SPILLSTK
	stwu	%r1,-52(%r1)
	stw	%r0,48(%r1)		/* save non-volatile registers */
	stw	%r3,44(%r1)
	stw	%r4,40(%r1)
	stw	%r5,36(%r1)
	stw	%r6,32(%r1)
	stw	%r7,28(%r1)
	stw	%r8,24(%r1)
	stw	%r9,20(%r1)
	stw	%r10,16(%r1)
	stw	%r11,12(%r1)
	stw	%r12,8(%r1)
	mfxer	%r30			/* save XER */
	mtsprg	2,%r30
	mflr	%r30			/* save trap type */
	mfctr	%r31			/* & CTR */
	mfdar	%r7
	mfsrr1	%r4
	mfdsisr	%r5
	li	%r6, 0
s_pte_spill:
	andi.	%r0,%r4,PSL_PR
	li	%r3,0
	bne	1f
	mr	%r3,%r7
	bl	_C_LABEL(pte_spill_r)	/* try a spill */
1:
	cmpwi	0,%r3,0
	mtctr	%r31			/* restore CTR */
	mtlr	%r30			/* and trap type */
	mfsprg	%r31,2			/* get saved XER */
	mtxer	%r31			/* restore XER */
	lwz	%r12,8(%r1)		/* restore non-volatile registers */
	lwz	%r11,12(%r1)
	lwz	%r10,16(%r1)
	lwz	%r9,20(%r1)
	lwz	%r8,24(%r1)
	lwz	%r7,28(%r1)
	lwz	%r6,32(%r1)
	lwz	%r5,36(%r1)
	lwz	%r4,40(%r1)
	lwz	%r3,44(%r1)
	lwz	%r0,48(%r1)
	beq	disitrap
	mtcr	%r29			/* restore CR */
	mtlr	%r28			/* restore LR */
	GET_CPUINFO(%r1)
	lmw	%r28,CI_DISISAVE(%r1)	/* restore r28-r31 */
	mfsprg	%r1,1			/* restore SP */
rfi2:	rfi				/* return to trapped code */

/*
 * ISI second stage fault handler
 */
s_isitrap:
	mfsrr1	%r31			/* test if this may be a spill fault */
	mtcr	%r31
	mtsprg	1,%r1			/* save SP */
	bc	4,%r1,disitrap		/* branch if table miss is false */
	GET_CPUINFO(%r30)
	lwz	%r30,CI_INTSTK(%r30)	/* get interrupt stack */
	addi	%r1,%r30,SPILLSTK
	stwu	%r1,-52(%r1)
	stw	%r0,48(%r1)		/* save non-volatile registers */
	stw	%r3,44(%r1)
	stw	%r4,40(%r1)
	stw	%r5,36(%r1)
	stw	%r6,32(%r1)
	stw	%r7,28(%r1)
	stw	%r8,24(%r1)
	stw	%r9,20(%r1)
	stw	%r10,16(%r1)
	stw	%r11,12(%r1)
	stw	%r12,8(%r1)
	mfxer	%r30			/* save XER */
	mtsprg	2,%r30
	mflr	%r30			/* save trap type */
	mfctr	%r31			/* & ctr */
	mfsrr0	%r7
	mfsrr1	%r4
	li	%r5, 0
	li	%r6, 1
	b	s_pte_spill		/* above */

/*
 * External interrupt second level handler
 */
#define	INTRENTER							\
/* Save non-volatile registers: */					\
	stwu	%r1,-88(%r1);		/* temporarily */		\
	stw	%r0,84(%r1);						\
	mfsprg	%r0,1;			/* get original SP */		\
	stw	%r0,0(%r1);		/* and store it */		\
	stw	%r3,80(%r1);						\
	stw	%r4,76(%r1);						\
	stw	%r5,72(%r1);						\
	stw	%r6,68(%r1);						\
	stw	%r7,64(%r1);						\
	stw	%r8,60(%r1);						\
	stw	%r9,56(%r1);						\
	stw	%r10,52(%r1);						\
	stw	%r11,48(%r1);						\
	stw	%r12,44(%r1);						\
	stw	%r28,40(%r1);		/* saved LR */			\
	stw	%r29,36(%r1);		/* saved CR */			\
	stw	%r30,32(%r1);		/* saved XER */			\
	GET_CPUINFO(%r4);						\
	lmw	%r28,CI_TEMPSAVE(%r4);	/* restore r28-r31 */		\
	mfctr	%r6;							\
	lwz	%r5,CI_INTRDEPTH(%r4);			\
	mfsrr0	%r4;							\
	mfsrr1	%r3;							\
	stw	%r6,28(%r1);						\
	stw	%r5,20(%r1);						\
	stw	%r4,12(%r1);						\
	stw	%r3,8(%r1);						\
/* load all kernel segment registers. */				\
	lis	3,_C_LABEL(kernel_pmap_)@ha;                            \
	addi	3,3,_C_LABEL(kernel_pmap_)@l;                           \
	lwz	%r5,0(%r3);	mtsr 0,%r5;				\
	lwz	%r5,4(%r3);	mtsr 1,%r5;				\
	lwz	%r5,8(%r3);	mtsr 2,%r5;				\
	lwz	%r5,12(%r3);	mtsr 3,%r5;				\
	lwz	%r5,16(%r3);	mtsr 4,%r5;				\
	lwz	%r5,20(%r3);	mtsr 5,%r5;				\
	lwz	%r5,24(%r3);	mtsr 6,%r5;				\
	lwz	%r5,28(%r3);	mtsr 7,%r5;				\
	lwz	%r5,32(%r3);	mtsr 8,%r5;				\
	lwz	%r5,36(%r3);	mtsr 9,%r5;				\
	lwz	%r5,40(%r3);	mtsr 10,%r5;				\
	lwz	%r5,44(%r3);	mtsr 11,%r5;				\
	lwz	%r5,48(%r3);	mtsr 12,%r5;				\
/*	lwz	%r5,52(%r3);	mtsr 13,%r5;	- dont load user SR - XXX? */ \
	lwz	%r5,56(%r3);	mtsr 14,%r5;				\
	lwz	%r5,60(%r3);	mtsr 15,%r5;				\
/* interrupts are recoverable here, and enable translation */		\
	mfmsr	%r5;							\
	ori	%r5,%r5,(PSL_IR|PSL_DR|PSL_RI);				\
	mtmsr	%r5;							\
	isync

	.globl	_C_LABEL(extint_call)
	.type	_C_LABEL(extint_call),@function
extintr:
	INTRENTER
_C_LABEL(extint_call):
	bl	_C_LABEL(extint_call)		/* to be filled in later */
intr_exit:
/* Disable interrupts (should already be disabled) and MMU here: */
	mfmsr	%r3
	andi.	%r3,%r3,~(PSL_EE|PSL_ME|PSL_RI|PSL_DR|PSL_IR)@l
	mtmsr	%r3
	isync
/* restore possibly overwritten registers: */
	lwz	%r12,44(%r1)
	lwz	%r11,48(%r1)
	lwz	%r10,52(%r1)
	lwz	%r9,56(%r1)
	lwz	%r8,60(%r1)
	lwz	%r7,64(%r1)
	lwz	%r6,8(%r1)
	lwz	%r5,12(%r1)
	lwz	%r4,28(%r1)
	lwz	%r3,32(%r1)
	mtsrr1	%r6
	mtsrr0	%r5
	mtctr	%r4
	mtxer	%r3

	GET_CPUINFO(%r5)
	lwz	%r4,CI_INTRDEPTH(%r5)
	addi	%r4,%r4,-1		/* adjust reentrancy count */
	stw	%r4,CI_INTRDEPTH(%r5)

/* Returning to user mode? */
	mtcr	%r6			/* saved SRR1 */
	bc	4,17,1f			/* branch if PSL_PR is false */
	lwz	%r3,CI_CURPM(%r5)	/* get current pmap real address */
	/* reload all segment registers. */
	lwz	%r4,0(3);	mtsr 0,%r4;
	lwz	%r4,4(3);	mtsr 1,%r4;
	lwz	%r4,8(3);	mtsr 2,%r4;
	lwz	%r4,12(3);	mtsr 3,%r4;
	lwz	%r4,16(3);	mtsr 4,%r4;
	lwz	%r4,20(3);	mtsr 5,%r4;
	lwz	%r4,24(3);	mtsr 6,%r4;
	lwz	%r4,28(3);	mtsr 7,%r4;
	lwz	%r4,32(3);	mtsr 8,%r4;
	lwz	%r4,36(3);	mtsr 9,%r4;
	lwz	%r4,40(3);	mtsr 10,%r4;
	lwz	%r4,44(3);	mtsr 11,%r4;
	lwz	%r4,48(3);	mtsr 12,%r4;
	lwz	%r4,52(3);	mtsr 13,%r4;
	lwz	%r4,56(3);	mtsr 14,%r4;
	lwz	%r4,60(3);	mtsr 15,%r4;
	lwz	%r4,CI_CURPROC(%r5)
	lwz	%r4,P_MD_ASTPENDING(%r4)	/* Test AST pending */
	andi.	%r4,%r4,1
	beq	1f
/* Setup for entry to realtrap: */
	lwz	%r3,0(%r1)		/* get saved SP */
	mtsprg	1,%r3
	li	%r6,EXC_AST
	stmw	%r28,CI_TEMPSAVE(%r5)	/* establish tempsave again */
	mtlr	%r6
	lwz	%r28,40(%r1)		/* saved LR */
	lwz	%r29,36(%r1)		/* saved CR */
	lwz	%r6,68(%r1)
	lwz	%r5,72(%r1)
	lwz	%r4,76(%r1)
	lwz	%r3,80(%r1)
	lwz	%r0,84(%r1)
	b	realtrap
1:
/* Here is the normal exit of extintr: */
	lwz	%r5,36(%r1)
	lwz	%r6,40(%r1)
	mtcr	%r5
	mtlr	%r6
	lwz	%r6,68(%r1)
	lwz	%r5,72(%r1)
	lwz	%r4,76(%r1)
	lwz	%r3,80(%r1)
	lwz	%r0,84(%r1)
	lwz	%r1,0(%r1)
rfi3:	rfi

/*
 * Decrementer interrupt second level handler
 */
decrintr:
	INTRENTER
	addi	%r3,%r1,8			/* intr frame */
	bl	_C_LABEL(decr_intr)
	b	intr_exit


/*
 * int setfault()
 *
 * Similar to setjmp to setup for handling faults on accesses to user memory.
 * Any routine using this may only call bcopy, either the form below,
 * or the (currently used) C code optimized, so it doesn't use any non-volatile
 * registers.
 */
	.globl	_C_LABEL(setfault)
	.type	_C_LABEL(setfault),@function
_C_LABEL(setfault):
	mflr	%r0
	mfcr	%r12
	mfmsr	%r2
	GET_CPUINFO(%r4)
	lwz	%r4,CI_CURPCB(%r4)
	stw	%r3,PCB_FAULT(%r4)
	stw	%r0,0(%r3)
	stw	%r2,4(%r3)
	stw	%r1,8(%r3)
	stmw	%r12,12(%r3)
	li	%r3,0
	blr

/*
 * The following code gets copied to the top of the user stack on process
 * execution.  It does signal trampolining on signal delivery.
 *
 * On entry r1 points to a struct sigframe at bottom of current stack.
 * All other registers are unchanged.
 */
	.globl	_C_LABEL(sigcode),_C_LABEL(esigcode)
	.type	_C_LABEL(sigcode),@function
	.type	_C_LABEL(esigcode),@function
_C_LABEL(sigcode):
	addi	%r1,%r1,-((16+FPSIG_SIZEOF+15)& ~0xf)		/* reserved space for callee */
	addi	%r6,%r1,8
	stfd	%f0,0(%r6)
	stfd	%f1,8(%r6)
	stfd	%f2,16(%r6)
	stfd	%f3,24(%r6)
	stfd	%f4,32(%r6)
	stfd	%f5,40(%r6)
	stfd	%f6,48(%r6)
	stfd	%f7,56(%r6)
	stfd	%f8,64(%r6)
	stfd	%f9,72(%r6)
	stfd	%f10,80(%r6)
	stfd	%f11,88(%r6)
	stfd	%f12,96(%r6)
	stfd	%f13,104(%r6)
	mffs	%f0
	stfd	%f0,112(%r6)
	lfd	%f0,0(%r6)	/* restore the clobbered register */
	blrl
	addi	%r6,%r1,8
	lfd	%f0,112(%r6)
	mtfsf	0xff,%f0
	lfd	%f0,0(%r6)
	lfd	%f1,8(%r6)
	lfd	%f2,16(%r6)
	lfd	%f3,24(%r6)
	lfd	%f4,32(%r6)
	lfd	%f5,40(%r6)
	lfd	%f6,48(%r6)
	lfd	%f7,56(%r6)
	lfd	%f8,64(%r6)
	lfd	%f9,72(%r6)
	lfd	%f10,80(%r6)
	lfd	%f11,88(%r6)
	lfd	%f12,96(%r6)
	lfd	%f13,104(%r6)
	addi	%r3,%r1,((16+FPSIG_SIZEOF+15)&~0xf)+SF_SC	/* compute &sf_sc */
	li	%r0,SYS_sigreturn
	sc				/* sigreturn(scp) */
	li	%r0,SYS_exit
	sc				/* exit(errno) */
_C_LABEL(esigcode):


#ifdef DDB
/*
 * Deliberate entry to ddbtrap
 */
	.globl	_C_LABEL(ddb_trap)
_C_LABEL(ddb_trap):
	mtsprg	1,%r1
	mfmsr	%r3
	mtsrr1	%r3
	andi.	%r3,%r3,~(PSL_EE|PSL_ME)@l
	mtmsr	%r3				/* disable interrupts */
	isync
	GET_CPUINFO(%r3)
	stmw	%r28,CI_DDBSAVE(%r3)
	mflr	%r28
	li	%r29,EXC_BPT
	mtlr	%r29
	mfcr	%r29
	mtsrr0	%r28

/*
 * Now the ddb trap catching code.
 */
ddbtrap:
	FRAME_SETUP(CI_DDBSAVE)
/* Call C trap code: */
	addi	%r3,%r1,8
	bl	_C_LABEL(ddb_trap_glue)
	or.	%r3,%r3,%r3
	bne	ddbleave
/* This wasn't for DDB, so switch to real trap: */
	lwz	%r3,FRAME_EXC+8(%r1)	/* save exception */
	GET_CPUINFO(%r4)
	stw	%r3,CI_DDBSAVE+8(%r4)
	FRAME_LEAVE(CI_DDBSAVE)
	mtsprg	1,%r1			/* prepare for entrance to realtrap */
	GET_CPUINFO(%r1)
	stmw	%r28,CI_TEMPSAVE(%r1)
	mflr	%r28
	mfcr	%r29
	lwz	%r31,CI_DDBSAVE+8(%r1)
	mtlr	%r31
	b	realtrap
ddbleave:
	FRAME_LEAVE(CI_DDBSAVE)
rfi4:	rfi
#endif /* DDB */

	.globl  _C_LABEL(rfi_inst)
_C_LABEL(rfi_inst):
	rfi
	.globl  _C_LABEL(rfid_inst)
_C_LABEL(rfid_inst):
	rfid
	.globl  _C_LABEL(nop_inst)
 _C_LABEL(nop_inst):
	nop

	.globl  _C_LABEL(rfi_start)
_C_LABEL(rfi_start):
	.long	rfi1, rfi1 + 4
	.long	rfi2, rfi2 + 4
	.long	rfi3, rfi3 + 4
	.long	rfi4, rfi4 + 4
	.long	0, 0


	.globl  _C_LABEL(nop64_start)
_C_LABEL(nop64_start):
	.long	nop64_1s, nop64_1e
	.long	nop64_2s, nop64_2e
	.long	0, 0

	.globl  _C_LABEL(nop32_start)
_C_LABEL(nop32_start):
	.long	nop32_1s, nop32_1e
	.long	nop32_2s, nop32_2e
	.long	nop32_3s, nop32_3e
	.long	nop32_4s, nop32_4e
	.long	nop32_5s, nop32_5e
	.long	nop32_6s, nop32_6e
	.long	nop32_7s, nop32_7e
	.long	0, 0
