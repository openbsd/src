/*	$OpenBSD: locore.S,v 1.6 1998/08/22 18:31:55 rahnds Exp $	*/
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

#include "machine/ipkdb.h"

#include "assym.h"

#include <sys/syscall.h>

#include <machine/asm.h>
#include <machine/param.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>

/*
 * Globals
 */
	.globl	_C_LABEL(esym),_C_LABEL(proc0paddr)
	.data
_C_LABEL(esym):	.long	0			/* end of symbol table */
_C_LABEL(proc0paddr):	.long	0		/* proc0 p_addr */
idle_u:	.long	0			/* fake uarea during idle after exit */
openfirmware_entry:	.long	0		/* openfirmware entry point */

/*
 * Startup entry
 */
_ENTRY(_C_LABEL(kernel_text))
_ENTRY(_ASM_LABEL(start))
	.globl	start
start:
#ifdef	FIREPOWERBUGS
	mfmsr	0
	andi.	0,0,PSL_IR|PSL_DR
	beq	1f

	bl	_C_LABEL(ofwr_init)
1:
#endif
	li	0,0
	mtmsr	0			/* Disable FPU/MMU/exceptions */
	isync

/* compute end of kernel memory */
	lis	8,_end@ha
	addi	8,8,_end@l
#if defined(DDB) || defined(KERNFS)
	lwz	9,0(8)			/* number of symbols */
	add	8,8,9
	lwzu	9,4(8)			/* length of string table */
	add	8,8,9
	lis	9,_C_LABEL(esym)@ha
	stw	8,_C_LABEL(esym)@l(9)		/* save for symbol handling */
#endif
	li	9,PGOFSET
	add	8,8,9
	andc	8,8,9
	lis	9,_C_LABEL(OF_buf)@ha
	stw	8,_C_LABEL(OF_buf)@l(9)
	addi	8,8,NBPG
	lis	9,idle_u@ha
	stw	8,idle_u@l(9)
	addi	8,8,USPACE		/* space for idle_u */
	lis	9,_C_LABEL(proc0paddr)@ha
	stw	8,_C_LABEL(proc0paddr)@l(9)
	addi	1,8,USPACE-FRAMELEN	/* stackpointer for proc0 */
	mr	4,1			/* end of mem reserved for kernel */
	xor	0,0,0
	stwu	0,-16(1)		/* end of stack chain */
	
	lis	8,openfirmware_entry@ha
	stw	5,openfirmware_entry@l(8)	/* save client interface handler */
	lis	3,start@ha
	addi	3,3,start@l
	mr	5,6			/* args string */
	bl	_C_LABEL(initppc)
	bl	_C_LABEL(main)
	b	_C_LABEL(OF_exit)

/*
 * OpenFirmware entry point
 */
_ENTRY(_C_LABEL(openfirmware))
	mflr	0			/* save return address */
	stw	0,4(1)
	stwu	1,-16(1)		/* setup stack frame */

	mfmsr	4			/* save msr */
	stw	4,8(1)

	lis	4,openfirmware_entry@ha	/* get firmware entry point */
	lwz	4,openfirmware_entry@l(4)
	mtlr	4

	li	0,0			/* turn off any ints/mmu/etc. */
	mtmsr	0
	isync

	blrl				/* call OpenFirmware */

	lwz	4,8(1)			/* restore msr */
	mtmsr	4
	isync

	lwz	1,0(1)			/* and return */
	lwz	0,4(1)
	mtlr	0
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
.lcomm	firmstk,NBPG,8

_ENTRY(_C_LABEL(ofw_stack))
	mfmsr	8			/* turn off interrupts */
	andi.	0,8,~(PSL_EE|PSL_RI)@l
	mtmsr	0
	stw	8,4(1)			/* abuse return address slot */

	lwz	5,0(1)			/* get length of stack frame */
	subf	5,1,5

	lis	7,firmstk+NBPG-8@ha
	addi	7,7,firmstk+NBPG-8@l
	lis	6,ofw_back@ha
	addi	6,6,ofw_back@l
	subf	4,5,7			/* make room for stack frame on new stack */
	stw	6,-4(7)			/* setup return pointer */
	stwu	1,-8(7)
	
	stw	7,-8(4)

	addi	3,1,8
	addi	1,4,-8
	subi	5,5,8

	b	_C_LABEL(ofbcopy)		/* and copy it */

ofw_back:
	lwz	1,0(1)			/* get callers original stack pointer */

	lwz	0,4(1)			/* get saved msr from abused slot */
	mtmsr	0
	
	lwz	1,0(1)			/* return */
	lwz	0,4(1)
	mtlr	0
	blr


/*
 * No processes are runnable, so loop waiting for one.
 * Separate label here for accounting purposes.
 */
_C_LABEL(idle):
	mfmsr	3
	andi.	3,3,~PSL_EE@l		/* disable interrupts while manipulating runque */
	mtmsr	3

	lis	8,_C_LABEL(whichqs)@ha
	lwz	9,_C_LABEL(whichqs)@l(8)

	or.	9,9,9
	bne-	_C_LABEL(sw1)			/* at least one queue non-empty */
	
	ori	3,3,PSL_EE		/* reenable ints again */
	mtmsr	3
	isync
	
/* May do some power saving here? */

	b	_C_LABEL(idle)

/*
 * switchexit gets called from cpu_exit to free the user structure
 * and kernel stack of the current process.
 */
_ENTRY(_C_LABEL(switchexit))
/* First switch to the idle pcb/kernel stack */
	lis	6,idle_u@ha
	lwz	6,idle_u@l(6)
	lis	7,_C_LABEL(curpcb)@ha
	stw	6,_C_LABEL(curpcb)@l(7)
	addi	1,6,USPACE-16		/* 16 bytes are reserved at stack top */
/* Now free the old user structure (args are already in r3, r4, r5) */
	bl	_C_LABEL(kmem_free)
/* Fall through to cpu_switch to actually select another proc */

/*
 * void cpu_switch(struct proc *p)
 * Find a runnable process and switch to it.
 */
_ENTRY(_C_LABEL(cpu_switch))
	mflr	0			/* save lr */
	stw	0,4(1)
	stwu	1,-16(1)
	stw	31,12(1)
	stw	30,8(1)

	mr	30,3
	lis	3,_C_LABEL(curproc)@ha
	xor	31,31,31
	stw	31,_C_LABEL(curproc)@l(3)	/* Zero to not accumulate cpu time */
	lis	3,_C_LABEL(curpcb)@ha
	lwz	31,_C_LABEL(curpcb)@l(3)

	xor	3,3,3
	bl	_C_LABEL(lcsplx)
	stw	3,PCB_SPL(31)		/* save spl */

/* Find a new process */
	mfmsr	3
	andi.	3,3,~PSL_EE@l		/* disable interrupts while manipulating runque */
	mtmsr	3
	isync

	lis	8,_C_LABEL(whichqs)@ha
	lwz	9,_C_LABEL(whichqs)@l(8)

	or.	9,9,9
	beq-	_C_LABEL(idle)			/* all queues empty */
_C_LABEL(sw1):
	cntlzw	10,9
	lis	4,_C_LABEL(qs)@ha
	addi	4,4,_C_LABEL(qs)@l
	slwi	3,10,3
	add	3,3,4			/* select queue */
	
	lwz	31,P_FORW(3)		/* unlink first proc from queue */
	lwz	4,P_FORW(31)
	stw	4,P_FORW(3)
	stw	3,P_BACK(4)

	cmpl	0,3,4			/* queue empty? */
	bne	1f

	lis	3,0x80000000@ha
	srw	3,3,10
	andc	9,9,3
	stw	9,_C_LABEL(whichqs)@l(8)		/* mark it empty */

1:
	xor	3,3,3
	lis	4,_C_LABEL(want_resched)@ha
	stw	3,_C_LABEL(want_resched)@l(4)	/* just did this resched thing */

	stw	3,P_BACK(31)		/* probably superfluous */

	lis	4,_C_LABEL(curproc)@ha
	stw	31,_C_LABEL(curproc)@l(4)	/* record new process */

	mfmsr	3
	ori	3,3,PSL_EE		/* Now we can interrupt again */
	mtmsr	3

	cmpl	0,31,30			/* is it the same process? */
	beq	switch_return

	or.	30,30,30		/* old process was exiting? */
	beq	switch_exited

	mfsr	10,USER_SR		/* save USER_SR for copyin/copyout */
	mfcr	11			/* save cr */
	mr	12,2			/* save r2 */
	stwu	1,-SFRAMELEN(1)		/* still running on old stack */
	stmw	10,8(1)
	lwz	3,P_ADDR(30)
	stw	1,PCB_SP(3)		/* save SP */

switch_exited:
	mfmsr	3
	andi.	3,3,~PSL_EE@l		/* disable interrupts while actually switching */
	mtmsr	3

	lwz	4,P_ADDR(31)
	lis	5,_C_LABEL(curpcb)@ha
	stw	4,_C_LABEL(curpcb)@l(5)		/* indicate new pcb */

	lwz	5,PCB_PMR(4)
	lis	6,_C_LABEL(curpm)@ha
	stwu	5,_C_LABEL(curpm)@l(6)		/* save real pmap pointer for spill fill */
	stwcx.	5,0,6			/* clear possible reservation */

	addic.	5,5,64
	li	6,0
	mfsr	8,KERNEL_SR		/* save kernel SR */
1:
	addis	6,6,-0x10000000@ha	/* set new procs segment registers */
	or.	6,6,6			/* This is done from the real address pmap */
	lwzu	7,-4(5)			/* so we don't have to worry */
	mtsrin	7,6			/* about accessibility */
	bne	1b
	mtsr	KERNEL_SR,8		/* restore kernel SR */
	isync

	lwz	1,PCB_SP(4)		/* get new procs SP */

	ori	3,3,PSL_EE		/* interrupts are okay again */
	mtmsr	3

	lmw	10,8(1)			/* get other regs */
	lwz	1,0(1)			/* get saved SP */
	mr	2,12			/* get saved r2 */
	mtcr	11			/* get saved cr */
	isync
	mtsr	USER_SR,10		/* get saved USER_SR */
	isync

switch_return:
	mr	30,7			/* save proc pointer */
	lwz	3,PCB_SPL(4)
	bl	_C_LABEL(lcsplx)

	mr	3,30			/* get curproc for special fork returns */

	lwz	31,12(1)
	lwz	30,8(1)
	addi	1,1,16
	lwz	0,4(1)
	mtlr	0
	blr


/*
 * Data used during primary/secondary traps/interrupts
 */
#define	tempsave	0x2e0		/* primary save area for trap handling */
#define	disisave	0x3e0		/* primary save area for dsi/isi traps */
#define	INTSTK	(8*1024)		/* 8K interrupt stack */
	.data
intstk:	.space	INTSTK			/* interrupt stack */
	.global _C_LABEL(intr_depth)
_C_LABEL(intr_depth):
	.long	-1			/* in-use marker */
#define	SPILLSTK 1024			/* 1K spill stack */
.lcomm	spillstk,SPILLSTK,8

/*
 * This code gets copied to all the trap vectors
 * (except ISI/DSI, the interrupts, and possibly the debugging traps when using IPKDB).
 */
	.text
	.globl	_C_LABEL(trapcode),_C_LABEL(trapsize)
_C_LABEL(trapcode):
	mtsprg	1,1			/* save SP */
	stmw	28,tempsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
/* Test whether we already had PR set */
	mfsrr1	31
	mtcr	31
	bc	4,17,1f			/* branch if PSL_PR is clear */
	lis	1,_C_LABEL(curpcb)@ha
	lwz	1,_C_LABEL(curpcb)@l(1)
	addi	1,1,USPACE		/* stack is top of user struct */
1:
	bla	s_trap
_C_LABEL(trapsize) =	.-_C_LABEL(trapcode)

/*
 * Similar to the above for DSI
 * Has to handle BAT spills
 * and standard pagetable spills
 */
	.globl	_C_LABEL(dsitrap),_C_LABEL(dsisize)
_C_LABEL(dsitrap):
	stmw	28,disisave(0)		/* free r28-r31 */
	mfcr	29			/* save CR */
	mfxer	30			/* save XER */
	mtsprg	2,30			/* in SPRG2 */
	mfsrr1	31			/* test kernel mode */
	mtcr	31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfdar	31			/* get fault address */
	rlwinm	31,31,7,25,28		/* get segment * 8 */
	addis	31,31,_C_LABEL(battable)@ha
	lwz	30,_C_LABEL(battable)@l(31)	/* get batu */
	mtcr	30
	bc	4,30,1f			/* branch if supervisor valid is false */
	lwz	31,_C_LABEL(battable)+4@l(31)	/* get batl */
/* We randomly use the highest two bat registers here */
	mftb	28
	andi.	28,28,1
	bne	2f
	mtdbatu	2,30
	mtdbatl	2,31
	b	3f
2:
	mtdbatu	3,30
	mtdbatl	3,31
3:
	mfsprg	30,2			/* restore XER */
	mtxer	30
	mtcr	29			/* restore CR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */
1:
	mflr	28			/* save LR */
	bla	s_dsitrap
_C_LABEL(dsisize) =	.-_C_LABEL(dsitrap)

/*
 * Similar to the above for ISI
 */
	.globl	_C_LABEL(isitrap),_C_LABEL(isisize)
_C_LABEL(isitrap):
	stmw	28,disisave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	mfsrr1	31			/* test kernel mode */
	mtcr	31
	bc	12,17,1f		/* branch if PSL_PR is set */
	mfsrr0	31			/* get fault address */
	rlwinm	31,31,7,25,28		/* get segment * 8 */
	addis	31,31,_C_LABEL(battable)@ha
	lwz	30,_C_LABEL(battable)@l(31)	/* get batu */
	mtcr	30
	bc	4,30,1f			/* branch if supervisor valid is false */
	mtibatu	3,30
	lwz	30,_C_LABEL(battable)+4@l(31)	/* get batl */
	mtibatl	3,30
	mtcr	29			/* restore CR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */
1:
	bla	s_isitrap
_C_LABEL(isisize) =	.-_C_LABEL(isitrap)

/*
 * This one for the external interrupt handler.
 */
	.globl	_C_LABEL(extint),_C_LABEL(extsize)
_C_LABEL(extint):
	mtsprg	1,1			/* save SP */
	stmw	28,tempsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	mfxer	30			/* save XER */
	lis	1,intstk+INTSTK@ha	/* get interrupt stack */
	addi	1,1,intstk+INTSTK@l
	lwz	31,0(1)			/* were we already running on intstk? */
	addic.	31,31,1
	stw	31,0(1)
	beq	1f
	mfsprg	1,1			/* yes, get old SP */
1:
	ba	extintr
_C_LABEL(extsize) =	.-_C_LABEL(extint)

/*
 * And this one for the decrementer interrupt handler.
 */
	.globl	_C_LABEL(decrint),_C_LABEL(decrsize)
_C_LABEL(decrint):
	mtsprg	1,1			/* save SP */
	stmw	28,tempsave(0)		/* free r28-r31 */
	mflr	28			/* save LR */
	mfcr	29			/* save CR */
	mfxer	30			/* save XER */
	lis	1,intstk+INTSTK@ha	/* get interrupt stack */
	addi	1,1,intstk+INTSTK@l
	lwz	31,0(1)			/* were we already running on intstk? */
	addic.	31,31,1
	stw	31,0(1)
	beq	1f
	mfsprg	1,1			/* yes, get old SP */
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
_C_LABEL(tlbimiss):
	mfspr	2,HASH1			/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,ICMP			/* get first compare value */
	addi	2,2,-8			/* predec pointer */
1:
	mtctr	1			/* load counter */
2:
	lwzu	1,8(2)			/* get next pte */
	cmpl	0,1,3			/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	1,4(2)			/* load tlb entry lower word */
	andi.	3,1,8			/* check G-bit */
	bne	4f			/* if guarded, take ISI */
	mtctr	0			/* restore counter */
	mfspr	0,IMISS			/* get the miss address for the tlbli */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	ori	1,1,0x100		/* set the reference bit */
	mtspr	RPA,1			/* set the pte */
	srwi	1,1,8			/* get byte 7 of pte */
	tlbli	0			/* load the itlb */
	stb	1,6(2)			/* update page table */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,HASH2			/* get the second pointer */
	ori	3,3,0x40		/* change the compare value */
	li	1,8
	addi	2,2,-8			/* predec pointer */
	b	1b
4:	/* guarded */
	mfsrr1	3
	andi.	2,3,0xffff		/* clean upper srr1 */
	addis	2,2,0x800		/* set srr<4> to flag prot violation */
	b	6f
5:	/* not found anywhere */
	mfsrr1	3
	andi.	2,3,0xffff		/* clean upper srr1 */
	addis	2,2,0x4000		/* set srr1<1> to flag pte not found */
6:
	mtctr	0			/* restore counter */
	mtsrr1	2
	mfmsr	0
	xoris	0,0,2			/* flip the msr<tgpr> bit */
	mtcrf	0x80,3			/* restore cr0 */
	mtmsr	0			/* now with native gprs */
	isync
	ba	EXC_ISI
_C_LABEL(tlbimsize) =	.-_C_LABEL(tlbimiss)

	.globl	_C_LABEL(tlbdlmiss),_C_LABEL(tlbdlmsize)
_C_LABEL(tlbdlmiss):
	mfspr	2,HASH1			/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,DCMP			/* get first compare value */
	addi	2,2,-8			/* predec pointer */
1:
	mtctr	1			/* load counter */
2:
	lwzu	1,8(2)			/* get next pte */
	cmpl	0,1,3			/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	1,4(2)			/* load tlb entry lower word */
	mtctr	0			/* restore counter */
	mfspr	0,DMISS			/* get the miss address for the tlbld */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	ori	1,1,0x100		/* set the reference bit */
	mtspr	RPA,1			/* set the pte */
	srwi	1,1,8			/* get byte 7 of pte */
	tlbld	0			/* load the dtlb */
	stb	1,6(2)			/* update page table */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,HASH2			/* get the second pointer */
	ori	3,3,0x40		/* change the compare value */
	li	1,8
	addi	2,2,-8			/* predec pointer */
	b	1b
5:	/* not found anywhere */
	mfsrr1	3
	lis	1,0x4000		/* set dsisr<1> to flag pte not found */
	mtctr	0			/* restore counter */
	andi.	2,3,0xffff		/* clean upper srr1 */
	mtsrr1	2
	mtdsisr	1			/* load the dsisr */
	mfspr	1,DMISS			/* get the miss address */
	mtdar	1			/* put in dar */
	mfmsr	0
	xoris	0,0,2			/* flip the msr<tgpr> bit */
	mtcrf	0x80,3			/* restore cr0 */
	mtmsr	0			/* now with native gprs */
	isync
	ba	EXC_DSI
_C_LABEL(tlbdlmsize) =	.-_C_LABEL(tlbdlmiss)

	.globl	_C_LABEL(tlbdsmiss),_C_LABEL(tlbdsmsize)
_C_LABEL(tlbdsmiss):
	mfspr	2,HASH1			/* get first pointer */
	li	1,8
	mfctr	0			/* save counter */
	mfspr	3,DCMP			/* get first compare value */
	addi	2,2,-8			/* predec pointer */
1:
	mtctr	1			/* load counter */
2:
	lwzu	1,8(2)			/* get next pte */
	cmpl	0,1,3			/* see if found pte */
	bdneq	2b			/* loop if not eq */
	bne	3f			/* not found */
	lwz	1,4(2)			/* load tlb entry lower word */
	andi.	3,1,0x80		/* check the C-bit */
	beq	4f
5:
	mtctr	0			/* restore counter */
	mfspr	0,DMISS			/* get the miss address for the tlbld */
	mfsrr1	3			/* get the saved cr0 bits */
	mtcrf	0x80,3			/* and restore */
	mtspr	RPA,1			/* set the pte */
	tlbld	0			/* load the dtlb */
	rfi

3:	/* not found in pteg */
	andi.	1,3,0x40		/* have we already done second hash? */
	bne	5f
	mfspr	2,HASH2			/* get the second pointer */
	ori	3,3,0x40		/* change the compare value */
	li	1,8
	addi	2,2,-8			/* predec pointer */
	b	1b
4:	/* found, but C-bit = 0 */
	rlwinm.	3,1,30,0,1		/* test PP */
	bge-	7f
	andi.	3,1,1
	beq+	8f
9:	/* found, but protection violation (PP==00)*/
	mfsrr1	3
	lis	1,0xa00			/* indicate protection violation on store */
	b	1f
7:	/* found, PP=1x */
	mfspr	3,DMISS			/* get the miss address */
	mfsrin	1,3			/* get the segment register */
	mfsrr1	3
	rlwinm	3,3,18,31,31		/* get PR-bit */
	rlwnm.	2,2,3,1,1		/* get the key */
	bne-	9b			/* protection violation */
8:	/* found, set reference/change bits */
	lwz	1,4(2)			/* reload tlb entry */
	ori	1,1,0x180
	sth	1,6(2)
	b	5b
5:	/* not found anywhere */
	mfsrr1	3
	lis	1,0x4200		/* set dsisr<1> to flag pte not found */
					/* dsisr<6> to flag store */
1:
	mtctr	0			/* restore counter */
	andi.	2,3,0xffff		/* clean upper srr1 */
	mtsrr1	2
	mtdsisr	1			/* load the dsisr */
	mfspr	1,DMISS			/* get the miss address */
	mtdar	1			/* put in dar */
	mfmsr	0
	xoris	0,0,2			/* flip the msr<tgpr> bit */
	mtcrf	0x80,3			/* restore cr0 */
	mtmsr	0			/* now with native gprs */
	isync
	ba	EXC_DSI
_C_LABEL(tlbdsmsize) =	.-_C_LABEL(tlbdsmiss)

#if NIPKDB > 0
#define	ipkdbsave	0xde0		/* primary save area for IPKDB */
/*
 * In case of IPKDB we want a separate trap catcher for it
 */
.lcomm	ipkdbstk,INTSTK			/* ipkdb stack */

	.globl	_C_LABEL(ipkdblow),_C_LABEL(ipkdbsize)
_C_LABEL(ipkdblow):
	mtsprg	1,1			/* save SP */
	stmw	28,ipkdbsave(0)		/* free r28-r31 */
	lis	1,ipkdbstk+INTSTK@ha	/* get new SP */
	addi	1,1,ipkdbstk+INTSTK@l
	mflr	28
	mfcr	29
	bla	ipkdbtrap
_C_LABEL(ipkdbsize) =	.-_C_LABEL(ipkdblow)
#endif	/* NIPKDB > 0 */

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
	mfsrr0	30;							\
	mfsrr1	31;							\
	stmw	30,savearea+24(0);					\
	mfmsr	30;							\
	ori	30,30,(PSL_DR|PSL_IR);					\
	mtmsr	30;							\
	isync;								\
	mfsprg	31,1;							\
	stwu	31,-FRAMELEN(1);					\
	stw	0,FRAME_0+8(1);						\
	stw	31,FRAME_1+8(1);					\
	stw	28,FRAME_LR+8(1);					\
	stw	29,FRAME_CR+8(1);					\
	lmw	28,savearea(0);						\
	stmw	2,FRAME_2+8(1);						\
	lmw	28,savearea+16(0);					\
	mfxer	3;							\
	mfctr	4;							\
	mflr	5;							\
	andi.	5,5,0xff00;						\
	stw	3,FRAME_XER+8(1);					\
	stw	4,FRAME_CTR+8(1);					\
	stw	5,FRAME_EXC+8(1);					\
	stw	28,FRAME_DAR+8(1);					\
	stw	29,FRAME_DSISR+8(1);					\
	stw	30,FRAME_SRR0+8(1);					\
	stw	31,FRAME_SRR1+8(1)

#define	FRAME_LEAVE(savearea)						\
/* Now restore regs: */							\
	lwz	2,FRAME_SRR0+8(1);					\
	lwz	3,FRAME_SRR1+8(1);					\
	lwz	4,FRAME_CTR+8(1);					\
	lwz	5,FRAME_XER+8(1);					\
	lwz	6,FRAME_LR+8(1);					\
	lwz	7,FRAME_CR+8(1);					\
	stw	2,savearea(0);						\
	stw	3,savearea+4(0);					\
	mtctr	4;							\
	mtxer	5;							\
	mtlr	6;							\
	mtsprg	1,7;			/* save cr */			\
	lmw	2,FRAME_2+8(1);						\
	lwz	0,FRAME_0+8(1);						\
	lwz	1,FRAME_1+8(1);						\
	mtsprg	2,2;			/* save r2 & r3 */		\
	mtsprg	3,3;							\
/* Disable translation, machine check and recoverability: */		\
	mfmsr	2;							\
	lis	3,(PSL_DR|PSL_IR|PSL_ME|PSL_RI)@ha;			\
	addi	3,3,(PSL_DR|PSL_IR|PSL_ME|PSL_RI)@l;			\
	andc	2,2,3;							\
	mtmsr	2;							\
	isync;								\
/* Decide whether we return to user mode: */				\
	lwz	3,savearea+4(0);					\
	mtcr	3;							\
	bc	4,17,1f;		/* branch if PSL_PR is false */	\
/* Restore user & kernel access SR: */					\
	lis	2,_C_LABEL(curpm)@ha;	/* get real address of pmap */	\
	lwz	2,_C_LABEL(curpm)@l(2);					\
	lwz	3,PM_USRSR(2);						\
	mtsr	USER_SR,3;						\
	lwz	3,PM_KERNELSR(2);					\
	mtsr	KERNEL_SR,3;						\
1:	mfsprg	2,1;			/* restore cr */		\
	mtcr	2;							\
	lwz	2,savearea(0);						\
	lwz	3,savearea+4(0);					\
	mtsrr0	2;							\
	mtsrr1	3;							\
	mfsprg	2,2;			/* restore r2 & r3 */		\
	mfsprg	3,3

/*
 * Preamble code for DSI/ISI traps
 */
disitrap:
	lmw	30,disisave(0)
	stmw	30,tempsave(0)
	lmw	30,disisave+8(0)
	stmw	30,tempsave+8(0)
	mfdar	30
	mfdsisr	31
	stmw	30,tempsave+16(0)
realtrap:
/* Test whether we already had PR set */
	mfsrr1	1
	mtcr	1
	mfsprg	1,1			/* restore SP (might have been overwritten) */
	bc	4,17,s_trap		/* branch if PSL_PR is false */
	lis	1,_C_LABEL(curpcb)@ha
	lwz	1,_C_LABEL(curpcb)@l(1)
	addi	1,1,USPACE		/* stack is top of user struct */
/*
 * Now the common trap catching code.
 */
s_trap:
/* First have to enable KERNEL mapping */
	lis	31,KERNEL_SEGMENT@ha
	addi	31,31,KERNEL_SEGMENT@l
	mtsr	KERNEL_SR,31
	FRAME_SETUP(tempsave)
/* Now we can recover interrupts again: */
	mfmsr	7
	ori	7,7,(PSL_EE|PSL_ME|PSL_RI)
	mtmsr	7
	isync
/* Call C trap code: */
trapagain:
	addi	3,1,8
	bl	_C_LABEL(trap)
trapexit:
/* Disable interrupts: */
	mfmsr	3
	andi.	3,3,~PSL_EE@l
	mtmsr	3
/* Test AST pending: */
	lwz	5,FRAME_SRR1+8(1)
	mtcr	5
	bc	4,17,1f			/* branch if PSL_PR is false */
	lis	3,_C_LABEL(astpending)@ha
	lwz	4,_C_LABEL(astpending)@l(3)
	andi.	4,4,1
	beq	1f
	li	6,EXC_AST
	stw	6,FRAME_EXC+8(1)
	b	trapagain
1:
	FRAME_LEAVE(tempsave)
	rfi

/*
 * Child comes here at the end of a fork.
 * Mostly similar to the above.
 */
	.globl	_C_LABEL(fork_trampoline)
_C_LABEL(fork_trampoline):
	xor	3,3,3
	bl	_C_LABEL(lcsplx)
	mtlr	31
	mr	3,30
	blrl				/* jump indirect to r31 */
	b	trapexit

/*
 * DSI second stage fault handler
 */
s_dsitrap:
	mfdsisr	31			/* test whether this may be a spill fault */
	mtcr	31
	mtsprg	1,1			/* save SP */
	bc	4,1,disitrap		/* branch if table miss is false */
	lis	1,spillstk+SPILLSTK@ha
	addi	1,1,spillstk+SPILLSTK@l	/* get spill stack */
	stwu	1,-52(1)
	stw	0,48(1)			/* save non-volatile registers */
	stw	3,44(1)
	stw	4,40(1)
	stw	5,36(1)
	stw	6,32(1)
	stw	7,28(1)
	stw	8,24(1)
	stw	9,20(1)
	stw	10,16(1)
	stw	11,12(1)
	stw	12,8(1)
	mflr	30			/* save trap type */
	mfctr	31			/* & CTR */
	mfdar	3
s_pte_spill:
	bl	_C_LABEL(pte_spill)		/* try a spill */
	or.	3,3,3
	mtctr	31			/* restore CTR */
	mtlr	30			/* and trap type */
	mfsprg	31,2			/* get saved XER */
	mtxer	31			/* restore XER */
	lwz	12,8(1)			/* restore non-volatile registers */
	lwz	11,12(1)
	lwz	10,16(1)
	lwz	9,20(1)
	lwz	8,24(1)
	lwz	7,28(1)
	lwz	6,32(1)
	lwz	5,36(1)
	lwz	4,40(1)
	lwz	3,44(1)
	lwz	0,48(1)
	beq	disitrap
	mfsprg	1,1			/* restore SP */
	mtcr	29			/* restore CR */
	mtlr	28			/* restore LR */
	lmw	28,disisave(0)		/* restore r28-r31 */
	rfi				/* return to trapped code */

/*
 * ISI second stage fault handler
 */
s_isitrap:
	mfsrr1	31			/* test whether this may be a spill fault */
	mtcr	31
	mtsprg	1,1			/* save SP */
	bc	4,1,disitrap		/* branch if table miss is false */
	lis	1,spillstk+SPILLSTK@ha
	addi	1,1,spillstk+SPILLSTK@l	/* get spill stack */
	stwu	1,-52(1)
	stw	0,48(1)			/* save non-volatile registers */
	stw	3,44(1)
	stw	4,40(1)
	stw	5,36(1)
	stw	6,32(1)
	stw	7,28(1)
	stw	8,24(1)
	stw	9,20(1)
	stw	10,16(1)
	stw	11,12(1)
	stw	12,8(1)
	mfxer	30			/* save XER */
	mtsprg	2,30
	mflr	30			/* save trap type */
	mfctr	31			/* & ctr */
	mfsrr0	3
	b	s_pte_spill		/* above */

/*
 * External interrupt second level handler
 */
#define	INTRENTER							\
/* Save non-volatile registers: */					\
	stwu	1,-88(1);		/* temporarily */		\
	stw	0,84(1);						\
	mfsprg	0,1;			/* get original SP */		\
	stw	0,0(1);			/* and store it */		\
	stw	3,80(1);						\
	stw	4,76(1);						\
	stw	5,72(1);						\
	stw	6,68(1);						\
	stw	7,64(1);						\
	stw	8,60(1);						\
	stw	9,56(1);						\
	stw	10,52(1);						\
	stw	11,48(1);						\
	stw	12,44(1);						\
	stw	28,40(1);		/* saved LR */			\
	stw	29,36(1);		/* saved CR */			\
	stw	30,32(1);		/* saved XER */			\
	lmw	28,tempsave(0);		/* restore r28-r31 */		\
	mfctr	6;							\
	lis	5,_C_LABEL(intr_depth)@ha;				\
	lwz	5,_C_LABEL(intr_depth)@l(5);				\
	mfsrr0	4;							\
	mfsrr1	3;							\
	stw	6,28(1);						\
	stw	5,20(1);						\
	stw	4,12(1);						\
	stw	3,8(1);							\
/* interrupts are recoverable here, and enable translation */		\
	lis	3,(KERNEL_SEGMENT|SR_SUKEY|SR_PRKEY)@ha;		\
	addi	3,3,(KERNEL_SEGMENT|SR_SUKEY|SR_PRKEY)@l;		\
	mtsr	KERNEL_SR,3;						\
	mfmsr	5;							\
	ori	5,5,(PSL_IR|PSL_DR|PSL_RI);				\
	mtmsr	5;							\
	isync

	.globl	_C_LABEL(extint_call)
extintr:
	INTRENTER
_C_LABEL(extint_call):
	bl	_C_LABEL(extint_call)		/* to be filled in later */
intr_exit:
/* Disable interrupts (should already be disabled) and MMU here: */
	mfmsr	3
	andi.	3,3,~(PSL_EE|PSL_ME|PSL_RI|PSL_DR|PSL_IR)@l
	mtmsr	3
	isync
/* restore possibly overwritten registers: */
	lwz	12,44(1)
	lwz	11,48(1)
	lwz	10,52(1)
	lwz	9,56(1)
	lwz	8,60(1)
	lwz	7,64(1)
	lwz	6,8(1)
	lwz	5,12(1)
	lwz	4,28(1)
	lwz	3,32(1)
	mtsrr1	6
	mtsrr0	5
	mtctr	4
	mtxer	3
/* Returning to user mode? */
	mtcr	6			/* saved SRR1 */
	bc	4,17,1f			/* branch if PSL_PR is false */
	lis	3,_C_LABEL(curpm)@ha	/* get current pmap real address */
	lwz	3,_C_LABEL(curpm)@l(3)
	lwz	3,PM_KERNELSR(3)
	mtsr	KERNEL_SR,3		/* Restore kernel SR */
	lis	3,_C_LABEL(astpending)@ha	/* Test AST pending */
	lwz	4,_C_LABEL(astpending)@l(3)
	andi.	4,4,1
	beq	1f
/* Setup for entry to realtrap: */
	lwz	3,0(1)			/* get saved SP */
	mtsprg	1,3
	li	6,EXC_AST
	stmw	28,tempsave(0)		/* establish tempsave again */
	mtlr	6
	lwz	28,40(1)		/* saved LR */
	lwz	29,36(1)		/* saved CR */
	lwz	6,68(1)
	lwz	5,72(1)
	lwz	4,76(1)
	lwz	3,80(1)
	lwz	0,84(1)
	lis	30,_C_LABEL(intr_depth)@ha	/* adjust reentrancy count */
	lwz	31,_C_LABEL(intr_depth)@l(30)
	addi	31,31,-1
	stw	31,_C_LABEL(intr_depth)@l(30)
	b	realtrap
1:
/* Here is the normal exit of extintr: */
	lwz	5,36(1)
	lwz	6,40(1)
	mtcr	5
	mtlr	6
	lwz	6,68(1)
	lwz	5,72(1)
	lis	3,_C_LABEL(intr_depth)@ha	/* adjust reentrancy count */
	lwz	4,_C_LABEL(intr_depth)@l(3)
	addi	4,4,-1
	stw	4,_C_LABEL(intr_depth)@l(3)
	lwz	4,76(1)
	lwz	3,80(1)
	lwz	0,84(1)
	lwz	1,0(1)
	rfi

/*
 * Decrementer interrupt second level handler
 */
decrintr:
	INTRENTER
	addi	3,1,8			/* intr frame */
	bl	_C_LABEL(decr_intr)
	b	intr_exit

#if NIPKDB > 0
/*
 * Deliberate entry to ipkdbtrap
 */
	.globl	_C_LABEL(ipkdb_trap)
_C_LABEL(ipkdb_trap):
	
	mtsprg	2,2
	mfmsr	3
	mtsrr1	3
	andi.	3,3,~(PSL_EE|PSL_ME)@l
	mtmsr	3			/* disable interrupts */
	isync
	stmw	28,ipkdbsave(0)
	mflr	28
	li	29,EXC_BPT
	mtlr	29
	mfcr	29
	mtsrr0	28

/*
 * Now the ipkdb trap catching code.
 */
ipkdbtrap:
	FRAME_SETUP(ipkdbsave)
/* Call C trap code: */
	addi	3,1,8
	bl	_C_LABEL(ipkdb_trap_glue)
	or.	3,3,3
	bne	ipkdbleave
/* This wasn't for IPKDB, so switch to real trap: */
	lwz	3,FRAME_EXC+8(1)	/* save exception */
	stw	3,ipkdbsave+8(0)
	FRAME_LEAVE(ipkdbsave)
	mtsprg	1,1			/* prepare for entrance to realtrap */
	stmw	28,tempsave(0)
	mflr	28
	mfcr	29
	lwz	31,ipkdbsave+8(0)
	mtlr	31
	b	realtrap
ipkdbleave:
	FRAME_LEAVE(ipkdbsave)
	rfi

ipkdbfault:
	ba	_C_LABEL(ipkdbfault)
_C_LABEL(ipkdbfault):
	mfsrr0	3
	addi	3,3,4
	mtsrr0	3
	li	3,-1
	rfi

/*
 * int ipkdbfbyte(unsigned char *p)
 */
	.globl	_C_LABEL(ipkdbfbyte)
_C_LABEL(ipkdbfbyte):
	li	9,EXC_DSI		/* establish new fault routine */
	lwz	5,0(9)
	lis	6,ipkdbfault@ha
	lwz	6,ipkdbfault@l(6)
	stw	6,0(9)
#ifdef	IPKDBUSERHACK
	lis	8,_C_LABEL(ipkdbsr)@ha
	lwz	8,_C_LABEL(ipkdbsr)@l(8)
	mtsr	USER_SR,8
	isync
#endif
	dcbst	0,9			/* flush data... */
	sync
	icbi	0,9			/* and instruction caches */
	lbz	3,0(3)			/* fetch data */
	stw	5,0(9)			/* restore previous fault handler */
	dcbst	0,9			/* and flush data... */
	sync
	icbi	0,9			/* and instruction caches */
	blr

/*
 * int ipkdbsbyte(unsigned char *p, int c)
 */
	.globl	_C_LABEL(ipkdbsbyte)
_C_LABEL(ipkdbsbyte):
	li	9,EXC_DSI		/* establish new fault routine */
	lwz	5,0(9)
	lis	6,ipkdbfault@ha
	lwz	6,ipkdbfault@l(6)
	stw	6,0(9)
#ifdef	IPKDBUSERHACK
	lis	8,_C_LABEL(ipkdbsr)@ha
	lwz	8,_C_LABEL(ipkdbsr)@l(8)
	mtsr	USER_SR,8
	isync
#endif
	dcbst	0,9			/* flush data... */
	sync
	icbi	0,9			/* and instruction caches */
	mr	6,3
	xor	3,3,3
	stb	4,0(6)
	dcbst	0,6			/* Now do appropriate flushes to data... */
	sync
	icbi	0,6			/* and instruction caches */
	stw	5,0(9)			/* restore previous fault handler */
	dcbst	0,9			/* and flush data... */
	sync
	icbi	0,9			/* and instruction caches */	
	blr
#endif	/* NIPKDB > 0 */
	
/*
 * int setfault()
 *
 * Similar to setjmp to setup for handling faults on accesses to user memory.
 * Any routine using this may only call bcopy, either the form below,
 * or the (currently used) C code optimized, so it doesn't use any non-volatile
 * registers.
 */
	.globl	_C_LABEL(setfault)
_C_LABEL(setfault):
	mflr	0
	mfcr	12
	lis	4,_C_LABEL(curpcb)@ha
	lwz	4,_C_LABEL(curpcb)@l(4)
	stw	3,PCB_FAULT(4)
	stw	0,0(3)
	stw	1,4(3)
	stw	2,8(3)
	stmw	12,12(3)
	xor	3,3,3
	blr

/*
 * The following code gets copied to the top of the user stack on process
 * execution.  It does signal trampolining on signal delivery.
 *
 * On entry r1 points to a struct sigframe at bottom of current stack.
 * All other registers are unchanged.
 */
	.globl	_C_LABEL(sigcode),_C_LABEL(esigcode)
_C_LABEL(sigcode):
	addi	1,1,-16			/* reserved space for callee */
	blrl
	addi	3,1,16+8		/* compute &sf_sc */
	li	0,SYS_sigreturn
	sc				/* sigreturn(scp) */
	li	0,SYS_exit
	sc				/* exit(errno) */
_C_LABEL(esigcode):



_C_LABEL(intrnames):
	.string "none"
_C_LABEL(eintrnames):
	.align 4
_C_LABEL(intrcnt):
	.long 0
_C_LABEL(eintrcnt):
