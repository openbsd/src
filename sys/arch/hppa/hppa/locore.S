/*	$OpenBSD: locore.S,v 1.1 1998/12/05 17:44:48 mickey Exp $	*/

/*
 * Copyright (c) 1998 Michael Shalayeff
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
 *	This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
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
/*
 *  (c) Copyright 1988 HEWLETT-PACKARD COMPANY
 *
 *  To anyone who acknowledges that this file is provided "AS IS"
 *  without any express or implied warranty:
 *      permission to use, copy, modify, and distribute this file
 *  for any purpose is hereby granted without fee, provided that
 *  the above copyright notice and this notice appears in all
 *  copies, and that the name of Hewlett-Packard Company not be
 *  used in advertising or publicity pertaining to distribution
 *  of the software without specific, written prior permission.
 *  Hewlett-Packard Company makes no representations about the
 *  suitability of this software for any purpose.
 */
/*
 * Copyright (c) 1990,1991,1992,1994 The University of Utah and
 * the Computer Systems Laboratory (CSL).  All rights reserved.
 *
 * THE UNIVERSITY OF UTAH AND CSL PROVIDE THIS SOFTWARE IN ITS "AS IS"
 * CONDITION, AND DISCLAIM ANY LIABILITY OF ANY KIND FOR ANY DAMAGES
 * WHATSOEVER RESULTING FROM ITS USE.
 *
 * CSL requests users of this software to return to csl-dist@cs.utah.edu any
 * improvements that they make and grant CSL redistribution rights.
 *
 * 	Utah $Hdr: locore.s 1.62 94/12/15$
 */

#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/break.h>
#include <machine/iomod.h>
#include <machine/pdc.h>
#include <machine/intr.h>
#ifdef	GPROF
#include <machine/gprof.h>
#endif
#include "assym.h"

	.import istackptr, data
	.import tmp_saved_state, data
	.import fpu_zero, data
	.import	fpu_pcb, data
	.import pdc, data
	.import	boothowto, data
	.import	bootdev, data
	.import istackptr, data

/*
 * Declare data sections
 */

	.space  $PRIVATE$
	.subspa $DATA$

/*
 * allocate the interrupt stack and a page after it for a red zone...
 */
	.align  NBPG
	.export intstack_base,data
intstack_base

	.blockz INTSTACK_SIZE

	.export intstack_top,data
intstack_top

	/* 
	 * interrupt stack red zone
	 */
	.blockz NBPG
	.align  NBPG

dumpstk
recoverstack
panic_stack
	.export panic_stack,data
	.blockz 2 * NBPG
	.align  NBPG

#ifdef	GPROF
	/*
	 * We want these on 1 cache line to make the interrupt handler
	 * as fast as possible.
	 */
	.align	32
	.export profiling,data
	.export s_lowpc,data
	.export kcount,data
	.export s_textsize,data
profiling
	.word	PROFILING_OFF
s_lowpc
	.word	0
kcount
	.word	0
s_textsize
	.word	0
#endif

/*
 * This is the starting location for the kernel
 */
ENTRY(__start)
/*
 *	start(pdc, boothowto, bootdev, bootapiver, argv, argc)
 *
 *	pdc - PDC entry point (not used, HP-UX compatibility)
 *	boothowto - boot flags (see "reboot.h")
 *	bootdev - boot device (index into bdevsw)
 *	bootapiver - /boot API version
 *	argv - options block passed from /boot
 *	argc - the length of the block
 */

	/*
	 * save the pdc, boothowto and bootdev arguments
	 */
	ldil	L%pdc,r1
	stw	arg0,R%pdc(r1)
	ldil	L%boothowto,r1
	stw	arg1,R%boothowto(r1)
	ldil	L%bootdev,r1
	stw	arg2,R%bootdev(r1)

	/*
	 * disable interrupts and turn off all bits in the psw so that
	 * we start in a known state.
	 */
	rsm	RESET_PSW,r0

	/*
	 * to keep the spl() routines consistent we need to put the correct
	 * spl level into eiem
	 */
	ldi	SPLHIGH,r1
	mtctl	r1,eiem

	/*
	 * set up the dp pointer so that we can do quick references off of it
	 */
	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/*
	 * establish an interrupt stack
	 */
	ldil	L%intstack_base,sp
	ldo	R%intstack_base(sp),sp

	/*
	 * clear intstackptr to indicate that we are on the interrupt stack
	 */
	ldil	L%istackptr,t1
	stw	r0,R%istackptr(t1)

	/*
	 * clear GCC frame pointer to avoid back-tracing off the end
	 */
	copy	r0,r4

	/*
	 * We need to set the Q bit so that we can take TLB misses after we
	 * turn on virtual memory.
	 */
	mtctl	r0,pcsq
	mtctl	r0,pcsq
	ldil	L%$qisnowon,t1
	ldo	R%$qisnowon(t1),t1
	mtctl	t1,pcoq	
	ldo	4(t1),t1
	mtctl	t1,pcoq	
	ldi	PSW_Q|PSW_I,t1
	mtctl	t1,ipsw
	rfi
	nop

$qisnowon
	
	/* 
	 * Initialize the external interrupt request register
	 */
	ldi    	-1,r1
	mtctl   r1,eirr

	/*
	 * load address of interrupt vector table
	 */
	ldil	L%$ivaaddr,r2
	ldo     R%$ivaaddr(r2),r2
	mtctl   r2,iva

	/*
	 * Create a stack frame for us to call C with. Clear out the previous
	 * sp marker to mark that this is the first frame on the stack.
	 */
	ldo     TF_SIZE(sp),sp
	stw     r0,FM_PSP(sp)

	/*
	 * disable all coprocessors
	 */
	mtctl   r0,ccr

	/*
	 * call C routine hppa_init() to initialize VM
	 */
	.import hppa_init, code
	ldil	L%hppa_init,r1
	ldo	R%hppa_init(r1),r1
	.call
	blr	r0,rp
	bv,n	(r1)
	nop

	/*
	 * go to virtual mode...
	 * get things ready for the kernel to run in virtual mode
	 */
	ldi	HPPA_PID_KERNEL,r1
	mtctl	r1,cr8
	mtctl	r1,cr9
	mtctl	r1,cr12
	mtctl	r1,cr13
	mtsp	r0,sr0
	mtsp	r0,sr1
	mtsp	r0,sr2
	mtsp	r0,sr3
	mtsp	r0,sr4
	mtsp	r0,sr5
	mtsp	r0,sr6
	mtsp	r0,sr7

	/*
	 * Cannot change the queues or IPSW with the Q-bit on
	 */
	rsm	RESET_PSW,r0

	/*
	 * We need to do an rfi to get the C bit set
	 */
	mtctl	r0,pcsq
	mtctl	r0,pcsq
	ldil	L%$virtual_mode,t1
	ldo	R%$virtual_mode(t1),t1
	mtctl	t1,pcoq	
	ldo	4(t1),t1
	mtctl	t1,pcoq	
	ldil	L%KERNEL_PSW,t1
	ldo	R%KERNEL_PSW(t1),t1
	mtctl	t1,ipsw
	rfi
	nop

$virtual_mode

#ifdef DDB_DDB
	.import	Debugger, code
	/* have to call debugger from here, from virtual mode */
	ldil	L%boothowto, r1
	ldo	R%boothowto(r1), r1
	bb,>=	r1,25,$noddb

	ldil	L%Debugger, r1
	ldo	R%Debugger(r1), r1
	.call
	blr     r0,rp
	bv,n    (r1)
	nop
$noddb
#endif

	.import main,code
	ldil	L%main, r1
	ldo	R%main(r1), r1
	.call
	blr     r0, rp
	bv,n    (r1)
	nop

	/* should never return... */
EXIT(__start)

ENTRY(set_psw)
	mtctl	r0,pcsq
	mtctl	r0,pcsq
	ldil	L%$set_psw,t1
	ldo	R%$set_psw(t1),t1
	mtctl	t1,pcoq	
	ldo	4(t1),t1
	mtctl	t1,pcoq	
	mtctl	arg0,ipsw
	rfi
	nop
$set_psw

EXIT(set_psw)

/*
 * Kernel Gateway Page (must be at known address)
 *    System Call Gate
 *    Signal Return Gate
 *
 * GATEway instructions have to be at a fixed known locations
 * because their addresses are hard coded in routines such as
 * those in the C library.
 */
	.align  NBPG
$gateway_page
	nop				/* @ 0.C0000000 (Nothing)  */
	gate,n	$bsd_syscall,r0         /* @ 0.C0000004 (HPUX/BSD) */
	nop				/* @ 0.C0000008 (HPOSF UNIX) */
	nop				/* @ 0.C000000C (HPOSF Mach) */
	nop
	nop
	nop
	nop

$bsd_syscall
	/*
	 * set up a space register and a protection register so that 
	 * we can access kernel memory
	 */
	mtsp	r0, sr1
	ldi	HPPA_PID_KERNEL, r1
	mtctl	r1, pidr4

	/*
	 * now call the syscall handler
	 */
	.import $syscall,code
	.call
	ldil    L%$syscall,r1
	be,n    R%$syscall(sr1,r1)
	nop

	/*
	 * Don't let anything else get on the gateway page.
	 */
	.align  NBPG

/*
 * interrupt vector table
 */
#define	TLABEL(name)	$trap$name
#define TRAP(reason) \
	.import TLABEL(all), code	! \
	mtctl	r1, tr7			! \
	ldil	L%TLABEL(all), r1	! \
	.call				! \
	be	R%TLABEL(all)(sr4, r1)	! \
	ldi	reason, r1

	/* 8 bytes per entry */
#define	ATRAP(name, reason) \
	.export	TLABEL(name), entry	! \
	.label	TLABEL(name)		! \
	TRAP(reason)			! \
	nop ! .word 0, 0, 0

#define	CTRAP(name, reason) \
	.import	TLABEL(name), code	! \
	nop				! \
	mtctl	r1, tr7			! \
	ldil	L%TLABEL(name), r1	! \
	.call				! \
	be	R%TLABEL(name)(sr4, r1)	! \
	ldi	reason, r1		! \
	nop ! .word 0, 0

#define	ITRAP(name, reason) \
	.import	TLABEL(name), code	! \
	nop				! \
	mtctl	r1, tr7			! \
	ldil	L%TLABEL(name), r1	! \
	.call				! \
	be	R%TLABEL(name)(sr4, r1)	! \
	ldi	reason, r1		! \
	nop ! .word 0, 0

	.align NBPG
	.export $ivaaddr, code
$ivaaddr
	ATRAP(nexist,T_NONEXIST)/*  0. invalid interrupt vector */
	CTRAP(hpmc,T_HPMC)	/*  1. high priority machine check */
	ATRAP(power,T_POWERFAIL)/*  2. power failure */
	ATRAP(recnt,T_RECOVERY)	/*  3. recovery counter trap */
	ATRAP(int,T_INTERRUPT)	/*  4. external interrupt */
	ATRAP(lpmc,T_LPMC)	/*  5. low-priority machine check */
	CTRAP(itlb,T_ITLBMISS)	/*  6. instruction TLB miss fault */
	ATRAP(iprot,T_IPROT)	/*  7. instruction protection trap */
	ATRAP(illegal,T_ILLEGAL)/*  8. Illegal instruction trap */
	ATRAP(ibreak,T_IBREAK)	/*  9. break instruction trap */
	ATRAP(privop,T_PRIV_OP)	/* 10. privileged operation trap */
	ATRAP(privr,T_PRIV_REG)	/* 11. privileged register trap */
	ATRAP(ovrfl,T_OVERFLOW)	/* 12. overflow trap */
	ATRAP(cond,T_CONDITION)	/* 13. conditional trap */
	ATRAP(excpt,T_EXCEPTION)/* 14. assist exception trap */
	CTRAP(dtlb,T_DTLBMISS)	/* 15. data TLB miss fault */
	CTRAP(itlb,T_ITLBMISS)	/* 16. ITLB non-access miss fault */
	CTRAP(dtlb,T_DTLBMISS)	/* 17. DTLB non-access miss fault */
	ATRAP(dprot,T_DPROT)	/* 18. data protection trap
				       unalligned data reference trap */
	ATRAP(dbreak,T_DBREAK)	/* 19. data break trap */
	CTRAP(tlbd,T_TLB_DIRTY)	/* 20. TLB dirty bit trap */
	ATRAP(pageref,T_PAGEREF)/* 21. page reference trap */
	ATRAP(emu,T_EMULATION)	/* 22. assist emulation trap */
	ATRAP(hpl,T_HIGHERPL)	/* 23. higher-privelege transfer trap */
	ATRAP(lpl,T_LOWERPL)	/* 24. lower-privilege transfer trap */
	ATRAP(tknbr,T_TAKENBR)	/* 25. taken branch trap */
	ATRAP(datacc,T_DATACC)	/* 26. data access rights trap (T-chip) */
	ATRAP(datapid,T_DATAPID)/* 27. data protection ID trap (T-chip) */
	ATRAP(datal,T_DATALIGN)	/* 28. unaligned data ref trap (T-chip) */
	ATRAP(unk29,29)
	ATRAP(unk30,30)
	ATRAP(unk31,31)
				/* 32 */
	.align	32*32


	.export	$trap$hpmc, code
TLABEL(hpmc)
	copy	%r0, arg0
	b	boot
	addi,tr 0,r0,arg0		; Skip first instr at target.
	break	0,0


	/* Compute the hpt entry ptr */
#if 1
#define	HPTENT ! \
	extru	r9,19,20,r16  		/* r16 = (offset >> 12) */	! \
	zdep	r8,26,16,r24	     	/* r24 = (space << 5) */	! \
	mfctl	hptmask,r17		/* r17 = sizeof(HPT)-1 */	! \
	xor	r16,r24,r24		/* r24 ^= r16 */		! \
	and	r17,r24,r24		/* r24 &= r17 */		! \
	mfctl	vtop,r16		/* r16 = address of HPT table */! \
	zdep	r24,27,28,r17		/* r24 <<= 4 */			! \
	add	r17,r16,r24		/* r24 = HPT entry */
#else
#define	HPTENT ! \
	mfctl	cr28, r24
#endif

	.export	TLABEL(tlbd), code
TLABEL(tlbd)
	mfctl	ior,r9		/* Offset */
	mfctl	isr,r8		/* Space  */

	mfsp	sr1, r25	/* Need a space reg, so save it in a shadow */
	mtsp	r8, sr1		/* Put the space id where we can use it */

	depi    0,31,12,r9	/* Clear the byte offset into the page */

	HPTENT

	/* Construct the virtual address tag. */
	extru	r9,14,15,r16		   /* r16 = off[0..14] */
	zdep	r16,15,15,r16		   /* r16 = tag[1..15] */
	or	r16,r8,r16		   /* or in the space id */
	depi	1,0,1,r16		   /* and set the valid bit */

	mtctl	r24, cr28
	mtctl	r16, cr29
	
	/*
	 * Chase the list of entries for this hash bucket until we find
	 * the correct mapping or NULL.
	 */
	ldw	hpt_entry(r24),r24
$hash_loop_tlbd
	comb,=	r0,r24,$pageflt
	mtsp	r25, sr1
	ldw     pv_va(r24),r16
	comb,<>,n r9,r16,$hash_loop_tlbd
	ldw	pv_hash(r24),r24
	ldw     pv_space(r24),r16
	comb,<>,n r8,r16,$hash_loop_tlbd
	ldw	pv_hash(r24),r24

	/*
	 * Set the dirty bit for this physical page.
	 */
	ldw	pv_tlbprot(r24), r16
	ldw	pv_tlbpage(r24), r17
	depi	1,TLB_DIRTY_POS,1, r16

	b	$tlb_inshpt
	stw	r16, pv_tlbprot(r24)
	

	.export	TLABEL(itlb), code
TLABEL(itlb)
	mfctl	pcoq,r9		/* Offset */
	b	$tlbmiss
	mfctl	pcsq,r8		/* Space  */

	.export	TLABEL(dtlb), code
TLABEL(dtlb)
	mfctl	ior,r9		/* Offset */
	mfctl	isr,r8		/* Space  */
	/* FALL THROUGH */

$tlbmiss
	/* r1 is the trap type
	 * r8 is the space of the address that had the TLB miss
	 * r9 is the offset of the address that had the TLB miss
	 */

	mfsp	sr1, r25	/* Need a space reg, so save it in a shadow */
	mtsp	r8, sr1		/* Put the space id where we can use it */

	depi    0,31,12,r9	/* Clear the byte offset into the page */

	HPTENT

	/* Construct the virtual address tag. */
	extru	r9,14,15,r16		   /* r16 = off[0..14] */
	zdep	r16,15,15,r16		   /* r16 = tag[1..15] */
	or	r16,r8,r16		   /* or in the space id */
	depi	1,0,1,r16		   /* and set the valid bit */

	/* Compare the tag against the HPT entry.
	   If it matches, then do the TLB insertion. */
	ldw	hpt_tag(r24),r17
	comb,=,n r16,r17,$tlb_gothpt

	/*
	 * Okay, so we missed in the HPT cache. Stash away the HPT entry
	 * pointer and the virtual tag for later ...
	 *
	 * Switch r24 to point to the corresponding VTOP table entry so
	 * we can move onto chasing the hash chain.
	 */
	mtctl	r24,cr28
	mtctl	r16,cr29

	/*
	 * Chase the list of entries for this hash bucket until we find
	 * the correct mapping or NULL.
	 */
	ldw	hpt_entry(r24),r24
$hash_loop
	comb,=	r0,r24,$pageflt
	mtsp	r25, sr1
	ldw     pv_va(r24),r16
	comb,<>,n r9,r16,$hash_loop
	ldw	pv_hash(r24),r24
	ldw     pv_space(r24),r16
	comb,<>,n r8,r16,$hash_loop
	ldw	pv_hash(r24),r24

	/* got it, set software flags */
	copy	r0, r16
	depi	1,TLB_DCACHE_POS,1,r16
	depi	1,TLB_ICACHE_POS,1,r16	
	stw	r16,pv_tlbsw(r24)

	/* Now set things up to enter the real mapping that we want */
	ldw	pv_tlbprot(r24),r16
	depi	1,TLB_REF_POS,1,r16
	stw	r16, pv_tlbprot(r24)
	ldw	pv_tlbpage(r24),r17

	/*
	 * Load the HPT cache with the miss information for next the time.
	 * The HTP entry address and virtual tag were saved above in
	 * control registers.
	 */
$tlb_inshpt
	mfctl	cr28,r24
	mfctl	cr29,r8

	stw	r16,hpt_tlbprot(r24)
	stw	r17,hpt_tlbpage(r24)
	b	$tlb_gothptld
	stw	r8,hpt_tag(r24)

$tlb_gothpt
	ldw	hpt_tlbpage(r24),r17
	ldw	hpt_tlbprot(r24),r16

$tlb_gothptld
	comib,=,n T_ITLBMISS,r1,$itlb
	idtlba	r17,(sr1, r9)
	idtlbp	r16,(sr1, r9)
	b,n	$tlbret
$itlb
	iitlba	r17,(sr1, r9)
	iitlbp	r16,(sr1, r9)

	/* And return ... */
$tlbret
	mtsp	r25, sr1
	rfir
	nop

	.align 32
$pageflt
	/* r1 still has trap type */
	/* FALL THROUGH */
	.export	TLABEL(all), code
TLABEL(all)
	/*
	 * at this point we have:
	 *	psw copied into ipsw
	 *	psw = E(default), M(1 if HPMC, else 0)
	 *	PL = 0
	 *	r1, r8, r9, r16, r17, r24, r25 shadowed
	 *	trap number in r1 (old r1 is saved in cr31)
	 */

	mtctl	arg0, tr2
	mtctl	sp, tr3
	copy	r1, arg0
	mtctl	t1, tr4
	mtctl	t2, tr5

	copy	sp, t2
	ldo	TF_SIZE+FM_SIZE(sp), sp
	stw	t2, FM_PSP(sp)

	/*
	 * now start saving the temporary registers into the saved state.
	 * These four get us out of temporary registers
	 */

	mfctl	tr2,t1
	stw	t1,TF_R26(t2)

	mfctl	tr3,t1
	stw	t1,TF_R30(t2)

	mfctl	tr4,t1
	stw	t1,TF_R22(t2)

	mfctl	tr5,t1
	stw	t1,TF_R21(t2)

	/*
	 * Now, save away other volatile state that prevents us from turning
	 * the PC queue back on, namely, the pc queue and ipsw, and the
	 * interrupt information.
	 */

	mfctl	pcoq,t1
	stw	t1,TF_IIOQH(t2)

	mtctl	r0,pcoq

	mfctl	pcoq,t1
	stw	t1,TF_IIOQT(t2)

	mfctl	pcsq,t1
	stw	t1,TF_IISQH(t2)

	mtctl	r0,pcsq

	mfctl	pcsq,t1
	stw	t1,TF_IISQT(t2)

	mfctl	ipsw,t1
	stw	t1,TF_CR22(t2)

	mfctl	eiem,t1
	stw	t1,TF_CR15(t2)

	mfctl	ior,t1
	stw	t1,TF_CR21(t2)

	mfctl	isr,t1
	stw	t1,TF_CR20(t2)

	mfctl	iir,t1
	stw	t1,TF_CR19(t2)

	/*
	 * Now we're about to turn on the PC queue.  We'll also go to virtual
	 * mode in the same step. Save the space registers sr4 - sr7 and
	 * point them to the kernel space
	 */

	mfsp	sr4,t1
	stw	t1,TF_SR4(t2)

	mfsp	sr5,t1
	stw	t1,TF_SR5(t2)

	mfsp	sr6,t1
	stw	t1,TF_SR6(t2)

	mfsp	sr7,t1
	stw	t1,TF_SR7(t2)

	mtsp	r0,sr4
	mtsp	r0,sr5
	mtsp	r0,sr6
	mtsp	r0,sr7

	/*
	 * save the protection ID registers. We will keep the first one
	 * with the protection of the user's area and set the remaining
	 * ones to be the kernel.
	 */

	mfctl	pidr2,t1
	stw	t1,TF_CR9(t2)

	mfctl	pidr3,t1
	stw	t1,TF_CR12(t2)

	mfctl	pidr4,t1
	stw	t1,TF_CR13(t2)

	ldi	HPPA_PID_KERNEL,t1
	mtctl	t1,pidr2
	mtctl	t1,pidr3
	mtctl	t1,pidr4

	copy	r0, t1

	/*
	 * Save trap flags
	 */
	mtctl	t1,tr6

	/*
	 * load the space queue
	 */

	mtctl	r0,pcsq
	mtctl	r0,pcsq

	/*
	 * set the new psw to be data and code translation, interrupts
	 * disabled, protection enabled, Q bit on
	 */

	ldil	L%KERNEL_PSW,t1
	ldo	R%KERNEL_PSW(t1),t1
	mtctl	t1,ipsw

	/*
	 * Load up a real value into eiem to reflect an spl level of splhigh.
	 * Right now interrupts are still off.
	 */
	ldi	SPLHIGH,t1
	mtctl	t1,eiem

	/*
	 * load in the address to "return" to with the rfir instruction
	 */

	ldil	L%$trapnowvirt,t1
	ldo	R%$trapnowvirt(t1),t1

	/*
	 * load the offset queue
	 */

	mtctl	t1,pcoq
	ldo	4(t1),t1
	mtctl	t1,pcoq

	/*
	 * Restore trap flags
	 */

	mfctl	tr6,t1

	/*
	 * Must do rfir not rfi since we may be called from tlbmiss routine
	 * (to handle page fault) and it uses the shadowed registers.
	 */
	rfir
	nop

$trapnowvirt
	/*
	 * t2 contains the virtual address of the saved status area
	 * t1 contains the trap flags
	 * sp contains the virtual address of the stack pointer
	 */

	stw	t1,TF_FLAGS(t2)

	/*
	 * Save all general registers that we haven't saved already
	 */

	stw	r1,TF_R1(t2)
	stw	r2,TF_R2(t2)
	stw	r3,TF_R3(t2)
	stw	r4,TF_R4(t2)
	stw	r5,TF_R5(t2)
	stw	r6,TF_R6(t2)
	stw	r7,TF_R7(t2)
	stw	r8,TF_R8(t2)
	stw	r9,TF_R9(t2)
	stw	r10,TF_R10(t2)
	stw	r11,TF_R11(t2)
	stw	r12,TF_R12(t2)
	stw	r13,TF_R13(t2)
	stw	r14,TF_R14(t2)
	stw	r15,TF_R15(t2)
	stw	r16,TF_R16(t2)
	stw	r17,TF_R17(t2)
	stw	r18,TF_R18(t2)
	stw	r19,TF_R19(t2)
	stw	r20,TF_R20(t2)
	/*	r21 already saved
	 *	r22 already saved */
	stw	r23,TF_R23(t2)
	stw	r24,TF_R24(t2)
	stw	r25,TF_R25(t2)
	/*	r26 already saved */
	stw	r27,TF_R27(t2)
	stw	r28,TF_R28(t2)
	stw	r29,TF_R29(t2)
 	/*	r30 already saved */
	stw	r31,TF_R31(t2)

	/*
	 * Save the space registers.
	 */

	mfsp	sr0,t1
	stw	t1,TF_SR0(t2)

	mfsp	sr1,t1
	stw	t1,TF_SR1(t2)

	mfsp	sr2,t1
	stw	t1,TF_SR2(t2)

	mfsp	sr3,t1
	stw	t1,TF_SR3(t2)

	/*
	 * Save the necessary control registers that were not already saved.
	 */

	mfctl	rctr,t1
	stw	t1,TF_CR0(t2)

	mfctl	pidr1,t1
	stw	t1,TF_CR8(t2)

	mfctl	sar,t1
	stw	t1,TF_CR11(t2)

	/*
	 * load the global pointer for the kernel
	 */

	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/*
	 * call the C routine trap. Interrupt type (arg0) was setup back before
	 * the call to thandler.
	 */
	copy	t2, arg1
	copy	t2, r4

#if KGDB
	/*
	 * Artificially create another frame so that gdb will
	 * show real trapped routine.
	 */
	stw	r2,FM_CRP-(FM_SIZE+ARG_SIZE)(sp)
	stw	r3,-(FM_SIZE+ARG_SIZE)(sp)	/* this overwrites ARG11 */
	ldo	-(FM_SIZE+ARG_SIZE)(sp),r3
#endif

	.import	trap, code
	ldil	L%trap,t1
	ldo	R%trap(t1),t1
	.call
	blr     r0,rp
	bv,n    0(t1)

	copy	r4, t3
	/*
	 * Restore most of the state, up to the point where we need to turn
	 * off the PC queue. Going backwards, starting with control regs.
	 */

	ldw	TF_CR15(t3),t1
	mtctl	t1,eiem

	ldw	TF_CR11(t3),t1
	mtctl	t1,sar

	ldw	TF_CR8(t3),t1
	mtctl	t1,pidr1

	ldw	TF_CR0(t3),t1
	mtctl	t1,rctr

	/*
	 * Restore the lower space registers, we'll restore sr4 - sr7 after
	 * we have turned off translations
	 */

	ldw	TF_SR0(t3),t1
	mtsp	t1,sr0

	ldw	TF_SR1(t3),t1
	mtsp	t1,sr1

	ldw	TF_SR2(t3),t1
	mtsp	t1,sr2

	ldw	TF_SR3(t3),t1
	mtsp	t1,sr3

	/*
	 * restore most of the general registers
	 */

	ldw	TF_R1(t3),r1
	ldw	TF_R2(t3),r2
	ldw	TF_R3(t3),r3
	ldw	TF_R4(t3),r4
	ldw	TF_R5(t3),r5
	ldw	TF_R6(t3),r6
	ldw	TF_R7(t3),r7
	ldw	TF_R8(t3),r8
	ldw	TF_R9(t3),r9
	ldw	TF_R10(t3),r10
	ldw	TF_R11(t3),r11
	ldw	TF_R12(t3),r12
	ldw	TF_R13(t3),r13
	ldw	TF_R14(t3),r14
	ldw	TF_R15(t3),r15
	ldw	TF_R16(t3),r16
	ldw	TF_R17(t3),r17
	ldw	TF_R18(t3),r18
	ldw	TF_R19(t3),r19
	/*	r20(t3) is used as a temporary and will be restored later */
	ldw	TF_R21(t3),r21
	/*	r22(t1) is used as a temporary and will be restored later */
	ldw	TF_R23(t3),r23
	ldw	TF_R24(t3),r24
	ldw	TF_R25(t3),r25
	ldw	TF_R26(t3),r26
	ldw	TF_R27(t3),r27
	ldw	TF_R28(t3),r28
	ldw	TF_R29(t3),r29
	/*	r30 (sp) will be restored later */
	ldw	TF_R31(t3),r31

	/*
	 * clear the system mask, this puts us back into physical mode.
	 *
	 * N.B: Better not be any code translation traps from this point
	 * on. Of course, we know this routine could never be *that* big.
	 */
	rsm	RESET_PSW,r0

	/*
	 * restore the protection ID registers
	 */

	ldw	TF_CR9(t3),t1
	mtctl	t1,pidr2

	ldw	TF_CR12(t3),t1
	mtctl	t1,pidr3

	ldw	TF_CR13(t3),t1
	mtctl	t1,pidr4

	/*
	 * restore the space registers
	 */

	ldw	TF_SR4(t3),t1
	mtsp	t1,sr4

	ldw	TF_SR5(t3),t1
	mtsp	t1,sr5

	ldw	TF_SR6(t3),t1
	mtsp	t1,sr6

	ldw	TF_SR7(t3),t1
	mtsp	t1,sr7

	/*
	 * finally we can restore the space and offset queues and the ipsw
	 */

	ldw	TF_IIOQH(t3),t1
	mtctl	t1,pcoq

	ldw	TF_IIOQT(t3),t1
	mtctl	t1,pcoq

	ldw	TF_IISQH(t3),t1
	mtctl	t1,pcsq

	ldw	TF_IISQT(t3),t1
	mtctl	t1,pcsq

	ldw	TF_CR22(t3),t1
	mtctl	t1,ipsw

	/*
	 * restore the last registers,r30, r22, and finally r21(t2)
	 */
	ldw	TF_R30(t3),sp
	ldw	TF_R22(t3),t1
	ldw	TF_R20(t3),t3

	rfi
	nop

	.align NBPG	

ENTRY(setjmp)
/*
 * Save the other general registers whose contents are expected to remain
 * across function calls.  According to the "HP9000 Series 800 Assembly
 * Language Reference Manual", procedures can use general registers 19-26,
 * 28, 29, 1, and 31 without restoring them.  Hence, we do not save these.
 */
	stwm	r3,4(arg0)
	stwm	r4,4(arg0)
	stwm	r5,4(arg0)
	stwm	r6,4(arg0)
	stwm	r7,4(arg0)
	stwm	r8,4(arg0)
	stwm	r9,4(arg0)
	stwm	r10,4(arg0)
	stwm	r11,4(arg0)
	stwm	r12,4(arg0)
	stwm	r13,4(arg0)
	stwm	r14,4(arg0)
	stwm	r15,4(arg0)
	stwm	r16,4(arg0)
	stwm	r17,4(arg0)
	stwm	r18,4(arg0)
	stwm	r27,4(arg0)	/* Good idea to save the data pointer (dp) */
	stwm    rp,4(arg0)	/* Save the return pointer */
	stwm	sp,4(arg0)	/* Save the original stack pointer */

	or      r0,r0,ret0
EXIT(setjmp)

ENTRY(longjmp)
/*
 * Restore general registers.
 */
	ldwm	4(arg0),r3
	ldwm	4(arg0),r4
	ldwm	4(arg0),r5
	ldwm	4(arg0),r6
	ldwm	4(arg0),r7
	ldwm	4(arg0),r8
	ldwm	4(arg0),r9
	ldwm	4(arg0),r10
	ldwm	4(arg0),r11
	ldwm	4(arg0),r12
	ldwm	4(arg0),r13
	ldwm	4(arg0),r14
	ldwm	4(arg0),r15
	ldwm	4(arg0),r16
	ldwm	4(arg0),r17
	ldwm	4(arg0),r18
	ldwm	4(arg0),r27
	ldwm	4(arg0),rp	/* Restore return address pointer, */
	ldwm	4(arg0),sp	/* stack pointer, */

	or	arg1,r0,ret0	/* Move return value to where it belongs. */
EXIT(longjmp)

/*
 * cpu_switch()
 * Find the highest priority process and resume it.
 */
	ENTRY(cpu_switch)
	EXIT(cpu_switch)

/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 */
	ENTRY(sigcode)

	ALTENTRY(esigcode)
	EXIT(sigcode)

	/* Leave this down here.  It avoids a nasty, hard-to-fix bug in gas. */
	.space $PRIVATE$
#ifndef	BFD_SUBSPA_BUG
	.subspa $GLOBAL$
#endif

	.export $global$,data
$global$

	.end
