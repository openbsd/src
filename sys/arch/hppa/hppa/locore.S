/*	$OpenBSD: locore.S,v 1.25 2000/01/12 04:44:14 mickey Exp $	*/

/*
 * Copyright (c) 1998,1999 Michael Shalayeff
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
 *      This product includes software developed by Michael Shalayeff.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * Portitions of this file are derived from other sources, see
 * the copyrights and acknowledgements below.
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

#include <sys/errno.h>
#include <machine/param.h>
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/iomod.h>
#include <machine/pdc.h>
#include <machine/intr.h>
#include <machine/frame.h>
#ifdef	GPROF
#include <machine/gprof.h>
#endif
#include "assym.h"


	/* this macro determines when to use cr28 (ie HVT works) */
#define	USECR28	((defined(HP7100LC_CPU) || defined(HP7300LC_CPU)) && \
	!(defined(HP71000_CPU) || defined(HP7200_CPU) || defined(HP7300_CPU)))

	.import	$global$, data
	.import pdc, data
	.import	boothowto, data
	.import	bootdev, data
	.import	esym, data
	.import	curproc, data
	.import	want_resched, data
	.import virtual_avail, data
	.import	proc0, data
	.import	proc0paddr, data
	.import	kpsw, data
	.import	panic, code

	.space $PRIVATE$
	.subspa $BSS$
	.export	pdc_stack, data
pdc_stack
	.block	3*NBPG
kernelmapped			/* set when kernel is mapped */
	.block	4
	.export fpu_csw, data
fpu_csw
	.block	4
	.export	fpu_curpcb, data
fpu_curpcb
	.block	4

/*
 * This is the starting location for the kernel
 */
ENTRY($start)
/*
 *	start(pdc, boothowto, bootdev, esym, bootapiver, argv, argc)
 *
 *	pdc - PDC entry point (not used, HP-UX compatibility)
 *	boothowto - boot flags (see "reboot.h")
 *	bootdev - boot device (index into bdevsw)
 *	esym - end of symbol table (or &end if not present)
 *	bootapiver - /boot API version
 *	argv - options block passed from /boot
 *	argc - the length of the block
 */

	/*
	 * save the pdc, boothowto, bootdev and esym arguments
	 */
	ldil	L%pdc,r1
	stw	arg0,R%pdc(r1)
	ldil	L%boothowto,r1
	stw	arg1,R%boothowto(r1)
	ldil	L%bootdev,r1
	stw	arg2,R%bootdev(r1)
	ldil	L%esym,r1
	stw	arg3,R%esym(r1)

	/* Align arg3, which is the start of available memory */
	ldo	NBPG-1(arg3), arg3
	dep	r0, 31, PGSHIFT, arg3

	/*
	 * disable interrupts and turn off all bits in the psw so that
	 * we start in a known state.
	 */
	rsm	RESET_PSW,r0

	/*
	 * to keep the spl() routines consistent we need to put the correct
	 * spl level into eiem
	 */
	ldi	IPL_NONE,r1
	mtctl	r1,eiem

	/*
	 * set up the dp pointer so that we can do quick references off of it
	 */
	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/* zero fake trapframe and proc0 u-area */
	ldo	NBPG+TF_SIZE(arg3), t1
	copy	arg3, t2
$start_zero_tf
	stw	r0, 0(t2)
	stw	r0, 4(t2)
	comb,<,n t2, t1, $start_zero_tf
	ldo	8(t2), t2

	/*
	 * kernel stack lives here (arg3 is page-aligned esym)
	 * initialize the pcb
	 * arg0 will be available space for hppa_init()
	 */
	ldo	NBPG(arg3), sp
	mtctl	arg3, cr30
	stw	r0, u_pcb+pcb_onfault(arg3)
	stw	r0, u_pcb+pcb_space(arg3)	/* XXX HPPA_SID_KERNEL == 0 */
	stw	arg3, u_pcb+pcb_uva(arg3)
	ldil	L%USPACE, arg0
	add	arg3, arg0, arg0
	ldil	L%proc0paddr, t1
	stw	arg3, R%proc0paddr(t1)
	ldil	L%proc0, t2
	stw	arg3, R%proc0+p_addr(t2)

	/* create a fake trapframe */
	ldil	L%TFF_LAST, t1
	stw	t1, TF_FLAGS(sp)
	/* accomodate the trapframe, once we created one */
	ldo	TF_SIZE(sp), sp

	/*
	 * We need to set the Q bit so that we can take TLB misses after we
	 * turn on virtual memory.
	 */
	mtctl	r0, pcsq
	mtctl	r0, pcsq
	ldil	L%$qisnowon, t1
	ldo	R%$qisnowon(t1), t1
	mtctl	t1, pcoq	
	ldo	4(t1),t1
	mtctl	t1, pcoq	
	ldi	PSW_Q|PSW_I, t1
	mtctl	t1, ipsw
	rfi
	nop

$qisnowon
	
	/* 
	 * Initialize the external interrupt request register
	 */
	/* ldi    	-1,r1 */
	mtctl   r0,eirr

	/*
	 * load address of interrupt vector table
	 */
	ldil	L%$ivaaddr,t2
	ldo	R%$ivaaddr(t2),t2
	mtctl	t2,iva

	/*
	 * Create a stack frame for us to call C with. Clear out the previous
	 * sp marker to mark that this is the first frame on the stack.
	 */
	copy	sp, t1
	stwm	r0, HPPA_FRAME_SIZE(sp)
	copy	sp, r3
	stwm	t1, HPPA_FRAME_SIZE(sp)

	/*
	 * disable all coprocessors
	 */
	mtctl	r0, ccr

	/*
	 * call C routine hppa_init() to initialize VM
	 */
	.import hppa_init, code
	ldil	L%hppa_init, r1
	ldo	R%hppa_init(r1), r1
	.call
	blr	r0, rp
	bv,n	(r1)
	nop

	/*
	 * go to virtual mode...
	 * get things ready for the kernel to run in virtual mode
	 */
	ldi	HPPA_PID_KERNEL, r1
	mtctl	r1, pidr1
	mtctl	r1, pidr2
#if pbably_not_worth_it
	mtctl	r0, pidr3
	mtctl	r0, pidr4
#endif
	mtsp	r0, sr0
	mtsp	r0, sr1
	mtsp	r0, sr2
	mtsp	r0, sr3
	mtsp	r0, sr4
	mtsp	r0, sr5
	mtsp	r0, sr6
	mtsp	r0, sr7

	/*
	 * Cannot change the queues or IPSW with the Q-bit on
	 */
	rsm	RESET_PSW, r0

	/*
	 * We need to do an rfi to get the C bit set
	 */
	mtctl	r0, pcsq
	mtctl	r0, pcsq
	ldil	L%$virtual_mode, t1
	ldo	R%$virtual_mode(t1), t1
	mtctl	t1, pcoq	
	ldo	4(t1), t1
	mtctl	t1, pcoq	
	ldil	L%kpsw, t1
	ldw	R%kpsw(t1), t2
	mtctl	t2, ipsw
	rfi
	nop

$virtual_mode
	ldil	L%kernelmapped, t1
	stw	t1, R%kernelmapped(t1)

#ifdef DDB
	.import	Debugger, code
	/* have to call debugger from here, from virtual mode */
	ldil	L%boothowto, r1
	ldw	R%boothowto(r1), r1
	bb,>=	r1, 25, $noddb
	nop

	break	HPPA_BREAK_KERNEL, HPPA_BREAK_KGDB
	nop
$noddb
#endif

	.import main,code
	ldil	L%main, r1
	ldo	R%main(r1), r1
$callmain
	.call
	blr     r0, rp
	bv,n    (r1)
	nop

	/* should never return... */
	bv	(rp)
	nop
EXIT($start)

/* int
 * pdc_call(func, pdc_flag, ...)
 *	iodcio_t func;
 *	int pdc_flag;
 */
ENTRY(pdc_call)

	mfctl	eiem, t1
	mtctl	r0, eiem
	stw	t1, HPPA_FRAME_ARG(0)(sp)
	stw	rp, HPPA_FRAME_CRP(sp)
	copy	arg0, r31
	copy	sp, ret1

	ldil	L%kernelmapped, ret0
	ldw	R%kernelmapped(ret0), ret0
	comb,=	r0, ret0, pdc_call_unmapped1
	nop
	ldil	L%pdc_stack, ret1

pdc_call_unmapped1
	ldo	HPPA_FRAME_SIZE+24*4(ret1), ret1

	stw	sp, HPPA_FRAME_PSP(ret1)
	copy	arg2, arg0
	copy	arg3, arg1
	ldw	HPPA_FRAME_ARG(4)(sp), arg2
	ldw	HPPA_FRAME_ARG(5)(sp), arg3
	ldw	HPPA_FRAME_ARG(6)(sp), t1
	ldw	HPPA_FRAME_ARG(7)(sp), t2
	ldw	HPPA_FRAME_ARG(8)(sp), t3
	ldw	HPPA_FRAME_ARG(9)(sp), t4
	stw	t1, HPPA_FRAME_ARG(4)(ret1)
	stw	t2, HPPA_FRAME_ARG(5)(ret1)
	stw	t3, HPPA_FRAME_ARG(6)(ret1)
	stw	t4, HPPA_FRAME_ARG(7)(ret1)
	ldw	HPPA_FRAME_ARG(10)(sp), t1
	ldw	HPPA_FRAME_ARG(11)(sp), t2
	ldw	HPPA_FRAME_ARG(12)(sp), t3
	ldw	HPPA_FRAME_ARG(13)(sp), t4
	stw	t1, HPPA_FRAME_ARG(8)(ret1)
	stw	t2, HPPA_FRAME_ARG(9)(ret1)
	stw	t3, HPPA_FRAME_ARG(10)(ret1)
	stw	t4, HPPA_FRAME_ARG(11)(ret1)

	/* save temp control regs */
	mfctl	cr24, t1
	mfctl	cr25, t2
	mfctl	cr26, t3
	mfctl	cr27, t4	
	stw	t1, HPPA_FRAME_ARG(12)(ret1)
	stw	t2, HPPA_FRAME_ARG(13)(ret1)
	stw	t3, HPPA_FRAME_ARG(14)(ret1)
	stw	t4, HPPA_FRAME_ARG(15)(ret1)
	mfctl	cr28, t1
	mfctl	cr29, t2
	mfctl	cr30, t3
	mfctl	cr31, t4	
	stw	t1, HPPA_FRAME_ARG(16)(ret1)
	stw	t2, HPPA_FRAME_ARG(17)(ret1)
	stw	t3, HPPA_FRAME_ARG(18)(ret1)
	stw	t4, HPPA_FRAME_ARG(19)(ret1)

	comb,=	r0, ret0, pdc_call_unmapped2
	nop

	copy	arg0, t4
	mfctl	eiem, t1
	mtctl	r0, eiem

	ldi	PSW_Q, arg0 /* (!pdc_flag && args[0] == PDC_PIM)? PSW_M:0) */
	break	HPPA_BREAK_KERNEL, HPPA_BREAK_SET_PSW
	nop
	stw	t1, HPPA_FRAME_ARG(22)(ret1)
	stw	ret0, HPPA_FRAME_ARG(23)(ret1)
	copy	t4, arg0

pdc_call_unmapped2
	copy	ret1, sp
	.call
	blr     r0, rp
	bv,n    (r31)
	nop

	/* load temp control regs */
	ldw	HPPA_FRAME_ARG(12)(sp), t1
	ldw	HPPA_FRAME_ARG(13)(sp), t2
	ldw	HPPA_FRAME_ARG(14)(sp), t3
	ldw	HPPA_FRAME_ARG(15)(sp), t4
	mtctl	t1, cr24
	mtctl	t2, cr25
	mtctl	t3, cr26
	mtctl	t4, cr27
	ldw	HPPA_FRAME_ARG(16)(sp), t1
	ldw	HPPA_FRAME_ARG(17)(sp), t2
	ldw	HPPA_FRAME_ARG(18)(sp), t3
	ldw	HPPA_FRAME_ARG(19)(sp), t4
	mtctl	t1, cr28
	mtctl	t2, cr29
	mtctl	t3, cr30
	mtctl	t4, cr31

	copy	sp, t4	
	ldw	HPPA_FRAME_PSP(t4), sp

	ldil	L%kernelmapped, t1
	ldw	R%kernelmapped(t1), t1
	comb,=	r0, t1, pdc_call_unmapped3
	nop

	copy	ret0, t3
	ldw	HPPA_FRAME_ARG(22)(t4), t1
	ldw	HPPA_FRAME_ARG(23)(t4), arg0
	break	HPPA_BREAK_KERNEL, HPPA_BREAK_SET_PSW
	nop
	copy	t3, ret0
	mtctl	t1, eiem

pdc_call_unmapped3
	ldw	HPPA_FRAME_ARG(0)(sp), t1
	ldw	HPPA_FRAME_CRP(sp), rp
	bv	r0(rp)
	mtctl	t1, eiem
EXIT(pdc_call)

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
	.export	gateway_page, entry
gateway_page
	nop				/* @ 0.C0000000 (Nothing)  */
	gate,n	$bsd_syscall,r0         /* @ 0.C0000004 (HPUX/BSD) */
#ifdef COMPAT_OSF1
	bl,n	$osf_syscall,r0
	bl,n	$osf_syscall,r0
#else
	nop				/* @ 0.C0000008 (HPOSF UNIX) */
	nop				/* @ 0.C000000C (HPOSF Mach) */
#endif
	nop
	nop
	nop
	nop

#ifdef COMPAT_OSF1
$osf_syscall
	/*
	 * Ripped screaming from OSF/MkLinux:
	 *
	 * Convert HPOSF system call to a BSD one by stashing arg4 and arg5
	 * back into the frame, and moving the system call number into r22.
	 * Fortunately, the HPOSF compiler has a bigger stack frame, which
	 * allows this horrible hack.
	 *
	 * We also need to save r29 (aka ret1) for the emulator since it may
	 * get clobbered between here and there.
	 */
	stw	r22, HPPA_FRAME_ARG(4)(sp)
	stw	r21, HPPA_FRAME_ARG(5)(sp)
	stw	r29, HPPA_FRAME_SL(sp)
	gate	$bsd_syscall,r0
	copy	r1, r22
#endif /* COMPAT_OSF1 */

$bsd_syscall
	/*
	 * set up a space register and a protection id so that 
	 * we can access kernel memory
	 */
	mtsp	r0, sr1
	mfctl	pidr1, r28
	ldi	HPPA_PID_KERNEL, t2
	mtctl	t2, pidr1

	/*
	 * now call the syscall handler
	 */
	.import $syscall,code
	.call
	ldil    L%$syscall,r1
	be,n    R%$syscall(sr7,r1)
	nop

	.align	NBPG
	.export	gateway_page_end, entry
gateway_page_end

$trap_tmp_save
	.block	TF_PHYS		/* XXX must be aligned to 64 */

	.import	syscall,code
        .export $syscall,entry
	.proc
	.callinfo calls
	.entry
$syscall
	/*
	 *
	 * t1:	curproc
	 * t2:	user
	 * t3:	args
	 * t4:	user stack
	 */
	ldil	L%curproc, t3
	ldw	R%curproc(sr1, t3), t3
	ldw	p_addr(sr1, t3), t2

	/* calculate kernel sp, load, create kernel stack frame */
	copy	sp, t4
	ldo	NBPG(t2), t2	
	ldo	TF_SIZE(t2), sp
	stw	t1, TF_R22 (sr1, t2)
	stw	t4, TF_R30 (sr1, t2)

	stw	r2 , TF_R2 (sr1, t2)
	stw	r3 , TF_R3 (sr1, t2)
	stw	r4 , TF_R4 (sr1, t2)
	stw	r5 , TF_R5 (sr1, t2)
	stw	r6 , TF_R6 (sr1, t2)
	stw	r7 , TF_R7 (sr1, t2)
	stw	r8 , TF_R8 (sr1, t2)
	stw	r9 , TF_R9 (sr1, t2)
	stw	r10, TF_R10(sr1, t2)
	stw	r11, TF_R11(sr1, t2)
	stw	r12, TF_R12(sr1, t2)
	stw	r13, TF_R13(sr1, t2)
	stw	r14, TF_R14(sr1, t2)
	stw	r15, TF_R15(sr1, t2)
	stw	r16, TF_R16(sr1, t2)
	stw	r17, TF_R17(sr1, t2)
	stw	r18, TF_R18(sr1, t2)
	stw	r27, TF_R27(sr1, t2)	/* dp */

	/* copy arguments */
	ldo	0(sp), t3
	ldo	HPPA_FRAME_SIZE+HPPA_FRAME_MAXARGS(sp), sp
	copy	t3, r1
	stw	arg0,  0(sr1, t3)
	stw	arg1,  4(sr1, t3)
	stw	arg2,  8(sr1, t3)
	stw	arg3, 12(sr1, t3)
	ldw	HPPA_FRAME_ARG( 4)(t4), arg0
	ldw	HPPA_FRAME_ARG( 5)(t4), arg1
	ldw	HPPA_FRAME_ARG( 6)(t4), arg2
	ldw	HPPA_FRAME_ARG( 7)(t4), arg3
	stw	arg0, 16(sr1, t3)
	stw	arg1, 20(sr1, t3)
	stw	arg2, 24(sr1, t3)
	stw	arg3, 28(sr1, t3)
	ldw	HPPA_FRAME_ARG( 8)(t4), arg0
	ldw	HPPA_FRAME_ARG( 9)(t4), arg1
	ldw	HPPA_FRAME_ARG(10)(t4), arg2
	ldw	HPPA_FRAME_ARG(11)(t4), arg3
	stw	arg0, 32(sr1, t3)
	stw	arg1, 36(sr1, t3)
	stw	arg2, 40(sr1, t3)
	stw	arg3, 44(sr1, t3)

	/*
	 * Save the rest of the CPU context
	 */
	
	ldo	-4(r31), arg1
	stw	r31, TF_IIOQH(sr1, t2)
	stw	arg1, TF_IIOQT(sr1, t2)

	mfsp	sr0, arg0
	mfsp	sr0, arg1
	stw	arg0, TF_IISQH(sr1, t2)
	stw	arg1, TF_IISQT(sr1, t2)

	mfctl	eiem, arg0
	mfctl	ipsw, arg1
	stw	arg0, TF_CR15(sr1, t2)
	stw	arg1, TF_CR22(sr1, t2)

	mfsp	sr3, arg0
	copy	r28, arg1
	stw	arg0, TF_SR3(sr1, t2)
	stw	arg1, TF_CR8(sr1, t2)
	
	mfctl	isr, arg0
	mfctl	ior, arg1
	stw	arg0, TF_CR20(sr1, t2)
	stw	arg1, TF_CR21(sr1, t2)

	mfctl	iir, arg0
	stw	arg0, TF_CR19(sr1, t2)
	stw	arg1, TF_FLAGS(sr1, t2)

	mfsp	sr0, arg0
	mfsp	sr0, arg1
	mfsp	sr2, arg2
	mfsp	sr4, arg3
	stw	arg0, TF_SR0(sr1, t2)
	stw	arg1, TF_SR1(sr1, t2)
	stw	arg2, TF_SR2(sr1, t2)
	stw	arg3, TF_SR4(sr1, t2)

	mfsp	sr5, arg0
	mfsp	sr6, arg1
	mfsp	sr7, arg2
	mfctl	pidr2, arg3
	stw	arg0, TF_SR5(sr1, t2)
	stw	arg1, TF_SR6(sr1, t2)
	stw	arg2, TF_SR7(sr1, t2)
	stw	arg3, TF_CR9 (sr1, t2)

	mfctl	pidr3, arg2
	mfctl	pidr4, arg3
	stw	arg2, TF_CR12(sr1, t2)
	stw	arg3, TF_CR13(sr1, t2)

#ifdef DDB
	/*
	 * Save hpt mask and v2p translation table pointer
	 */
	mfctl	eirr, arg0
	mfctl	hptmask, arg1
	stw	arg0, TF_CR23(sr1, t2)
	stw	arg1, TF_CR24(sr1, t2)

	mfctl	vtop, arg0
	mfctl	cr28, arg1
	stw	arg0, TF_CR25(sr1, t2)
	stw	arg1, TF_CR28(sr1, t2)
#endif

	/* setup kernel context */
	mtsp	r0, sr0
	mtsp	r0, sr1
	mtsp	r0, sr2
	mtsp	r0, sr3
	mtsp	r0, sr4
	mtsp	r0, sr5
	mtsp	r0, sr6
	mtsp	r0, sr7

	/* setup frame */
	stw	r0, HPPA_FRAME_PSP(sp)
	stw	r0, HPPA_FRAME_CRP(sp)

	copy	t2, arg0
	copy	r1, arg1

	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/* do a syscall */
	ldil	L%syscall, r1
	ldo	R%syscall(r1), r1
	.call
	blr	r0, rp
	bv,n	0(r1)

	ldil	L%curproc, r1
	ldw	R%curproc(r1), r1
	ldw	p_md(r1), t3

	.exit
	.procend
	/* FALLTHROUGH */

	.export	$syscall_return, entry
	.proc
	.callinfo no_calls
	.entry
$syscall_return
	/* t3 == VA trapframe */
	/* check for AST ? XXX */

	/*
	 * Copy partially saved state from the store into the frame
	 */
	ldi	$trap_tmp_save, arg1
	copy	t3, arg0
	ldo	TF_PHYS(arg0), arg2
$syscall_return_copy_loop
	ldw	0(arg0), t1
	ldw	4(arg0), t2
	stw	t1, 0(arg1)
	stw	t2, 4(arg1)
	ldo	8(arg0), arg0
	comb,<,n arg0, arg2, $syscall_return_copy_loop
	ldo	8(arg1), arg1

	/* 1. restore most of the general registers */
	ldw	TF_CR11(t3), t1
	ldw	TF_R1(t3), r1
	mtctl	t1, sar
	ldw	TF_R2(t3), r2
	ldw	TF_R3(t3), r3
	ldw	TF_R4(t3), r4
	ldw	TF_R5(t3), r5
	ldw	TF_R6(t3), r6
	ldw	TF_R7(t3), r7
	ldw	TF_R8(t3), r8
	ldw	TF_R9(t3), r9
	ldw	TF_R10(t3), r10
	ldw	TF_R11(t3), r11
	ldw	TF_R12(t3), r12
	ldw	TF_R13(t3), r13
	ldw	TF_R14(t3), r14
	ldw	TF_R15(t3), r15
	ldw	TF_R16(t3), r16
	ldw	TF_R17(t3), r17
	ldw	TF_R18(t3), r18
	ldw	TF_R19(t3), r19
	/*	r20(t3) is used as a temporary and will be restored later */
	/*	r21(t3) is used as a temporary and will be restored later */
	/*	r22(t3) is used as a temporary and will be restored later */
	ldw	TF_R23(t3), r23
	ldw	TF_R24(t3), r24
	ldw	TF_R25(t3), r25
	ldw	TF_R26(t3), r26
	ldw	TF_R27(t3), r27
	ldw	TF_R28(t3), r28
	ldw	TF_R29(t3), r29
	/*	r30 (sp) will be restored later */
	ldw	TF_R31(t3), r31

	/* 2. restore all the space regs and pid regs, except sr3, pidr1 */
	ldw	TF_SR0(t3), t1
	ldw	TF_SR1(t3), t2
	mtsp	t1, sr0
	mtsp	t2, sr1

	ldw	TF_SR2(sr3, t3), t1
	ldw	TF_SR4(sr3, t3), t2
	mtsp	t1, sr2
	mtsp	t2, sr4
       
	ldw	TF_SR5(sr3, t3), t1
	ldw	TF_SR6(sr3, t3), t2
	mtsp	t1, sr5
	mtsp	t2, sr6
       
	ldw	TF_SR7(sr3, t3), t1
	ldw	TF_CR9(sr3, t3), t2
	mtsp	t1, sr7
	mtctl	t2, pidr2

#if pbably_not_worth_it
	ldw	TF_CR12(sr3, t3), t1
	ldw	TF_CR13(sr3, t3), t2
	mtctl	t1, pidr3
	mtctl	t2, pidr4
#endif
	ldw	TF_CR30(sr3, t3), t1
	mtctl	t1, cr30

	/*
	 * clear the system mask, this puts us back into physical mode.
	 * reload trapframe pointer w/ correspondent PA value.
	 * sp will be left in virtual until restored from trapframe,
	 * since we don't use it anyway.
	 */
	rsm	RESET_PSW, r0
	nop ! nop ! nop ! nop ! nop ! nop ! nop ! nop
$syscall_return_phys
	
	ldi	$trap_tmp_save, t3

	/* finally we can restore the space and offset queues and the ipsw */
	ldw	TF_IISQH(t3), t1
	ldw	TF_IISQT(t3), t2
	mtctl	t1, pcsq
	mtctl	t2, pcsq

	ldw	TF_IIOQH(t3), t1
	ldw	TF_IIOQT(t3), t2
	mtctl	t1, pcoq
	mtctl	t2, pcoq

	ldw	TF_CR15(t3), t1
	ldw	TF_CR22(t3), t2
	mtctl	t1, eiem
	mtctl	t2, ipsw

	ldw	TF_SR3(t3), t1
	ldw	TF_CR8(t3), t2
	mtsp	t1, sr3
	mtctl	t2, pidr1

	ldw	TF_R22(t3), t1
	ldw	TF_R21(t3), t2
	ldw	TF_R30(t3), sp
	ldw	TF_R20(t3), t3

	rfi
	nop
	.exit
	.procend
$syscall_end

/*
 * interrupt vector table
 */
#define	TLABEL(name)	$trap$name
#define TRAP(name,num) \
	.import TLABEL(name), code	! \
	mtctl	r1, tr7			! \
	ldil	L%TLABEL(name), r1	! \
	.call				! \
	be	R%TLABEL(name)(sr4, r1)	! \
	ldi	num, r1			! \
	.align	32

#define	ATRAP(name,num) \
	.export	TLABEL(name)$num, entry	! \
	.label	TLABEL(name)$num	! \
	TRAP(all,num)

#define	CTRAP(name,num,pre) \
	.export	TLABEL(name)$num, entry	! \
	.label	TLABEL(name)$num	! \
	pre				! \
	TRAP(name,num)

#if USECR28
#define	LDCR28	mfctl	cr28, r24
#else
#define	LDCR28	/* nop */
#endif
#define	ITLBPRE \
	mfctl	pcoq,r9		/* Offset */			! \
	mfctl	pcsq,r8		/* Space  */			! \
	depi	0,31,PGSHIFT,r9	/* align offset to page */
#define	DTLBPRE \
	mfctl	ior, r9		/* Offset */			! \
	mfctl	isr, r8		/* Space  */			! \
	depi    0,31,PGSHIFT,r9	/* align offset to page */	! \
	LDCR28

	.align NBPG
	.export $ivaaddr, entry
$ivaaddr
	ATRAP(null,T_NONEXIST)		/*  0. invalid interrupt vector */
	CTRAP(hpmc,T_HPMC,)		/*  1. high priority machine check */
	ATRAP(power,T_POWERFAIL)	/*  2. power failure */
	ATRAP(recnt,T_RECOVERY)		/*  3. recovery counter trap */
	ATRAP(intr,T_INTERRUPT)		/*  4. external interrupt */
	ATRAP(lpmc,T_LPMC)		/*  5. low-priority machine check */
	CTRAP(itlb,T_ITLBMISS,ITLBPRE)	/*  6. instruction TLB miss fault */
	ATRAP(iprot,T_IPROT)		/*  7. instruction protection trap */
	ATRAP(ill,T_ILLEGAL)		/*  8. Illegal instruction trap */
	CTRAP(ibrk,T_IBREAK,)		/*  9. break instruction trap */
	ATRAP(privop,T_PRIV_OP)		/* 10. privileged operation trap */
	ATRAP(privr,T_PRIV_REG)		/* 11. privileged register trap */
	ATRAP(ovrfl,T_OVERFLOW)		/* 12. overflow trap */
	ATRAP(cond,T_CONDITION)		/* 13. conditional trap */
#ifdef FPEMUL
	CTRAP(excpt,T_EXCEPTION,)	/* 14. assist exception trap */
#else
	ATRAP(excpt,T_EXCEPTION)
#endif
	CTRAP(dtlb,T_DTLBMISS,DTLBPRE)	/* 15. data TLB miss fault */
	CTRAP(itlb,T_ITLBMISSNA,ITLBPRE)/* 16. ITLB non-access miss fault */
	CTRAP(dtlb,T_DTLBMISSNA,DTLBPRE)/* 17. DTLB non-access miss fault */
	ATRAP(dprot,T_DPROT)		/* 18. data protection trap
					      unalligned data reference trap */
	ATRAP(dbrk,T_DBREAK)		/* 19. data break trap */
	CTRAP(tlbd,T_TLB_DIRTY,DTLBPRE)	/* 20. TLB dirty bit trap */
	ATRAP(pgref,T_PAGEREF)		/* 21. page reference trap */
	CTRAP(emu,T_EMULATION,)		/* 22. assist emulation trap */
	ATRAP(hpl,T_HIGHERPL)		/* 23. higher-privelege transfer trap*/
	ATRAP(lpl,T_LOWERPL)		/* 24. lower-privilege transfer trap */
	ATRAP(tknbr,T_TAKENBR)		/* 25. taken branch trap */
	ATRAP(dacc,T_DATACC)		/* 26. data access rights trap */
	ATRAP(dpid,T_DATAPID)		/* 27. data protection ID trap */
	ATRAP(dalgn,T_DATALIGN)		/* 28. unaligned data ref trap */
	ATRAP(unk29,29)
	ATRAP(unk30,30)
	ATRAP(unk31,31)
	ATRAP(unk32,32)
	ATRAP(unk33,33)
	ATRAP(unk34,34)
	ATRAP(unk35,35)
	ATRAP(unk36,36)
	ATRAP(unk37,37)
	ATRAP(unk38,38)
	ATRAP(unk39,39)
	ATRAP(unk40,40)
	ATRAP(unk41,41)
	ATRAP(unk42,42)
	ATRAP(unk43,43)
	ATRAP(unk44,44)
	ATRAP(unk45,45)
	ATRAP(unk46,46)
	ATRAP(unk47,47)
	ATRAP(unk48,48)
	ATRAP(unk49,49)
	ATRAP(unk50,50)
	ATRAP(unk51,51)
	ATRAP(unk52,52)
	ATRAP(unk53,53)
	ATRAP(unk54,54)
	ATRAP(unk55,55)
	ATRAP(unk56,56)
	ATRAP(unk57,57)
	ATRAP(unk58,58)
	ATRAP(unk59,59)
	ATRAP(unk60,60)
	ATRAP(unk61,61)
	ATRAP(unk62,62)
	ATRAP(unk63,63)
					/* 64 */

	.export	TLABEL(hpmc), entry
TLABEL(hpmc)
	break	0, 0

	.export TLABEL(emu), entry
TLABEL(emu)
	/* restore %r1 from CTRAP() */
	mfctl	tr7, r1

	/*
	 * Switch FPU/SFU context
	 *
	 * isr:ior - data address
	 * iir - instruction to emulate
	 * iisq:iioq - address of instruction to emulate
	 *
	 * note: ISR and IOR contain valid data only if the
	 *	 instruction is a coprocessor load or store.
	 *
	 */
	mtctl	t1, tr2
	mtctl	t2, tr3
	mtctl	t3, tr5

	ldil	L%fpu_curpcb, t1
	ldw	R%fpu_curpcb(t1), t1

	mfctl	ccr, t3
	mfctl	cr30, t2
	
	/* enable coprocessor */
	depi	3, 25, 2, t3
	mtctl	t3, ccr

	comb,=,n t1, t2, $fpusw_done
	comb,=,n r0, t1, $fpusw_nosave

	/* ldo	pcb_fpregs+u_pcb(t1), t1 */

	fstds,ma fr0 , 8(t3)	/* fr0 must be saved first */
	fstds,ma fr1 , 8(t3)
	fstds,ma fr2 , 8(t3)
	fstds,ma fr3 , 8(t3)
	fstds,ma fr4 , 8(t3)
	fstds,ma fr5 , 8(t3)
	fstds,ma fr6 , 8(t3)
	fstds,ma fr7 , 8(t3)
	fstds,ma fr8 , 8(t3)
	fstds,ma fr9 , 8(t3)
	fstds,ma fr10, 8(t3)
	fstds,ma fr11, 8(t3)
	fstds,ma fr12, 8(t3)
	fstds,ma fr13, 8(t3)
	fstds,ma fr14, 8(t3)
	fstds,ma fr15, 8(t3)
	fstds,ma fr16, 8(t3)
	fstds,ma fr17, 8(t3)
	fstds,ma fr18, 8(t3)
	fstds,ma fr19, 8(t3)
	fstds,ma fr20, 8(t3)
	fstds,ma fr21, 8(t3)
	fstds,ma fr22, 8(t3)
	fstds,ma fr23, 8(t3)
	fstds,ma fr24, 8(t3)
	fstds,ma fr25, 8(t3)
	fstds,ma fr26, 8(t3)
	fstds,ma fr27, 8(t3)
	fstds,ma fr28, 8(t3)
	fstds,ma fr29, 8(t3)
	fstds,ma fr30, 8(t3)
	fstds    fr31, 0(t3)

$fpusw_nosave

	/* count switches */
	ldil	L%fpu_csw, t1
	ldw	R%fpu_csw(t1), t3
	ldo	1(t3), t3
	stw	t3, R%fpu_csw(t1)

	ldo	31*8+pcb_fpregs+u_pcb(t2), t3

	fldds,ma -8(t3), fr31
	fldds,ma -8(t3), fr30
	fldds,ma -8(t3), fr29
	fldds,ma -8(t3), fr28
	fldds,ma -8(t3), fr27
	fldds,ma -8(t3), fr26
	fldds,ma -8(t3), fr25
	fldds,ma -8(t3), fr24
	fldds,ma -8(t3), fr23
	fldds,ma -8(t3), fr22
	fldds,ma -8(t3), fr21
	fldds,ma -8(t3), fr20
	fldds,ma -8(t3), fr19
	fldds,ma -8(t3), fr18
	fldds,ma -8(t3), fr17
	fldds,ma -8(t3), fr16
	fldds,ma -8(t3), fr15
	fldds,ma -8(t3), fr14
	fldds,ma -8(t3), fr13
	fldds,ma -8(t3), fr12
	fldds,ma -8(t3), fr11
	fldds,ma -8(t3), fr10
	fldds,ma -8(t3), fr9
	fldds,ma -8(t3), fr8
	fldds,ma -8(t3), fr7
	fldds,ma -8(t3), fr6
	fldds,ma -8(t3), fr5
	fldds,ma -8(t3), fr4
	fldds,ma -8(t3), fr3
	fldds,ma -8(t3), fr2
	fldds,ma -8(t3), fr1
	fldds     0(t3), fr0	/* fr0 must be restored last */

	ldil	L%fpu_curpcb, t1
	stw	t2, R%fpu_curpcb(t1)

$fpusw_done
	mfctl	tr5, t3
	mfctl	tr3, t2
	mfctl	tr2, t1
	rfi
	nop

#ifdef FPEMUL
	.export TLABEL(excpt), entry
	/*
	 * Emulate FPU/SFU if none/disabled
	 *
	 * iisq:iioq - exception triggered instruction
	 */
TLABEL(excpt)
	mtctl	sp, tr3
	mtctl	r31, tr2

	.import	$fpemu_stack, data
	ldil	L%$fpemu_stack, r31
	ldo	R%$fpemu_stack(r31), r31
	ldo	R%TF_SIZE+HPPA_FRAME_SIZE(r31), sp

	stw	r1 , TF_R1 (r31)
	stw	r2 , TF_R2 (r31)
	stw	r19, TF_R19(r31)
	stw	r20, TF_R20(r31)
	stw	r21, TF_R21(r31)
	stw	r22, TF_R22(r31)
	stw	r23, TF_R23(r31)
	stw	r24, TF_R24(r31)
	stw	r25, TF_R25(r31)
	stw	r26, TF_R26(r31)
	stw	r27, TF_R27(r31)
	stw	r28, TF_R28(r31)
	stw	r29, TF_R29(r31)
	mfctl	sar, r1
	mfctl	iir, arg0
	stw	r1,  TF_CR11(r31)

	extru,<> arg0, 10, 1, r0
	extru,=  arg0, 11, 1, r0
	or,tr    r0, r0, r0
	bl,n	$sfu_emu, rp

	.import	fpu_emulate, code
	ldil	L%fpu_emulate,t1
	ldo	R%fpu_emulate(t1),t1
	mfctl	iir, arg0
	/* arg3 -- regs */
	.call
	blr	r0,rp
	bv,n	0(t1)
	nop

	ldil	L%$fpemu_stack, r31
	ldo	R%$fpemu_stack(r31), r31

	ldw	TF_CR11(r31), r1
	mtsar	r1
	ldw	TF_R29(r31), r29
	ldw	TF_R28(r31), r27
	mtctl	r27, tr5
	ldw	TF_R27(r31), r27
	ldw	TF_R26(r31), r26
	ldw	TF_R25(r31), r25
	ldw	TF_R24(r31), r24
	ldw	TF_R23(r31), r23
	ldw	TF_R22(r31), r22
	ldw	TF_R21(r31), r21
	ldw	TF_R20(r31), r20
	ldw	TF_R19(r31), r19
	ldw	TF_R2 (r31), r2
	mfctl	tr3, sp
	mfctl	tr2, r31

	comb,<>	r0, ret0, TLABEL(all)
	mfctl	tr5, ret0

	mfctl	tr7, r1
	rfi
	nop
	
	.export	$sfu_emu, entry
$sfu_emu
	bv	r0(rp)
	ldo	1(r0), ret0	/* none supported by now */

#endif /* FPEMUL */

	/* Compute the hpt entry ptr */
#define	HPTENT \
	extru	r9, 23, 24, r16  	/* r17 = (offset >> 8) */	! \
	zdep	r8, 26, 16, r24	     	/* r24 = (space << 5) */	! \
	mfctl	hptmask, r17		/* r17 = sizeof(HPT)-1 */	! \
	xor	r16, r24, r24		/* r24 ^= r16 */		! \
	and	r17, r24, r24		/* r24 &= r17 */		! \
	mfctl	vtop, r16		/* r16 = address of HPT table */! \
	or	r16, r24, r24		/* r24 = HPT entry */		! \
	mtctl	r24, cr28

	/* Construct the virtual address tag. */
#define	VTAG ! \
	shd	r0, r9, 1, r16		/* r16[1..15] = off[0..14] */	! \
	dep	r8, 31, 16, r16		/* put in the space id */	! \
	depi	1, 0, 1, r16		/* and set the valid bit */


	.align	64
	.export	TLABEL(tlbd), entry
TLABEL(tlbd)
#if ! USECR28
	HPTENT	/* will update cr28 */
#endif
	
	/*
	 * Chase the list of entries for this hash bucket until we find
	 * the correct mapping or NULL.
	 */
	ldw	hpt_entry(r24), r24
$hash_loop_tlbd
	comb,=,n r0, r24, TLABEL(all)
	ldw     pv_va(r24), r25
	ldw     pv_space(r24), r17
	comb,<>,n r9, r25, $hash_loop_tlbd
	ldw	pv_hash(r24), r24
	comb,<>,n r8, r17, $hash_loop_tlbd
	ldw	pv_hash(r24), r24

#if ! USECR28
	VTAG	/* (r8,r9) -> r16 */
#endif
	/* Set the dirty bit for this physical page. */
	ldw	pv_tlbprot(r24), r25
	b	$tlb_inshpt
	depi	1, TLB_DIRTY_POS, 1, r25
	
	.align	32
	.export	TLABEL(itlb), entry
TLABEL(itlb)
#if USECR28
	HPTENT	/* will update cr28 */
#endif
	depi	1, TFF_ITLB_POS, 1, r1	/* mark for ITLB insert */
	/* FALLTHROUGH */

	.export	TLABEL(dtlb), entry
TLABEL(dtlb)

$tlbmiss
	/* r1 is the trap type
	 * r8 is the space of the address that had the TLB miss
	 * r9 is the offset of the address that had the TLB miss
	 * r24 is the correspondent HPT entry pointer
	 */

#if ! USECR28
	HPTENT	/* will update cr28 */
	
	VTAG	/* (r8,r9) -> r16 */
	ldw	hpt_tag(r24),r17

	/* Compare the tag against the HPT entry.
	   If it matches, then do the TLB insertion. */
	comb,<>,n r16, r17, $tlb_gottalook

	ldw	hpt_tlbpage(r24), r17
	b	$tlb_gothpt
	ldw	hpt_tlbprot(r24), r25

$tlb_gottalook
#endif
	/*
	 * Chase the list of entries for this hash bucket until we find
	 * the correct mapping or NULL.
	 */
	ldw	hpt_entry(r24),r24
$hash_loop
	comb,=,n r0, r24, TLABEL(all)
	ldw     pv_va(r24),r25
	ldw     pv_space(r24),r17
	comb,<>,n r9,r25,$hash_loop
	ldw	pv_hash(r24),r24
	comb,<>,n r8,r17,$hash_loop
	ldw	pv_hash(r24),r24

	/* Now set things up to enter the real mapping that we want */
	ldw	pv_tlbprot(r24),r25
	depi	1, TLB_REF_POS, 1, r25

	/*
	 * Load the HPT cache with the miss information for the next time.
	 * The HPT entry address was saved by the HPTENT
	 */
$tlb_inshpt
	stw	r25, pv_tlbprot(r24)
#if USECR28
	VTAG	/* (r8,r9) -> r16 */
#endif
	ldw	pv_tlbpage(r24),r17
	mfctl	cr28, r24

	stw	r16, hpt_tag(r24)
	stw	r25, hpt_tlbprot(r24)
	stw	r17, hpt_tlbpage(r24)

$tlb_gothpt
	mfsp	sr1, r16
	bb,<	r1, TFF_ITLB_POS, $tlb_itlb
	mtsp	r8, sr1

	idtlba	r17,(sr1, r9)
	idtlbp	r25,(sr1, r9)
	mtsp	r16, sr1
	rfir
	nop

$tlb_itlb
	iitlba	r17,(sr1, r9)
	iitlbp	r25,(sr1, r9)
	mtsp	r16, sr1
	rfir
	nop
	.export	$tlb_missend, entry
$tlb_missend

	.align	32
	.export	TLABEL(ibrk), entry
TLABEL(ibrk)
	mtctl	t1, tr2
	mtctl	t2, tr3

	/* If called by a user process then always pass it to trap() */
	mfctl	pcoq, t1
	extru,=	t1, 31, 2, r0
	b,n	$ibrk_bad

	/* don't accept breaks from data segments */
	.import etext
	ldil	L%etext, t2
	ldo	R%etext(t2), t2
	comb,>>=,n t1, t2, $ibrk_bad

	mfctl	iir, t1
	extru	t1, 31, 5, t2
	comib,<>,n HPPA_BREAK_KERNEL, t2, $ibrk_bad

	/* now process all those `break' calls we make */
	extru	t1, 18, 13, t2
	comib,=,n HPPA_BREAK_GET_PSW, t2, $ibrk_getpsw
	comib,=,n HPPA_BREAK_SET_PSW, t2, $ibrk_setpsw

$ibrk_bad
	/* illegal (unimplemented) break entry point */
	mfctl	tr3, t2
	b	TLABEL(all)
	mfctl	tr2, t1

$ibrk_getpsw
	b	$ibrk_exit
	mfctl	ipsw, ret0

$ibrk_setpsw
	mfctl	ipsw, ret0
	b	$ibrk_exit
	mtctl	arg0, ipsw

$ibrk_setpsw_tovirt

	b	$ibrk_exit
	ldw	HPPA_FRAME_PSP(sp), sp

	/* insert other fast breaks here */
	nop ! nop

$ibrk_exit
	/* skip the break */
	mtctl	r0, pcoq
	mfctl	pcoq, t1
	mtctl	t1, pcoq
	ldo	4(t1), t1
	mtctl	t1, pcoq
	mfctl	tr3, t2
	mfctl	tr2, t1
	mfctl	tr7, r1
	rfi
	nop

	.align	64
	.export	TLABEL(all), entry
TLABEL(all)
	/* r1 still has trap type */

	/*
	 * at this point we have:
	 *	psw copied into ipsw
	 *	psw = E(default), M(1 if HPMC, else 0)
	 *	PL = 0
	 *	r1, r8, r9, r16, r17, r24, r25 shadowed (maybe)
	 *	trap number in r1 (old r1 is saved in tr7)
	 */

	/* do not overwrite tr4(cr28) */
	mtctl	t1, tr2
	mtctl	t2, tr3
	mtctl	t3, tr5

	mfctl	pcoq, t2
	bb,>=	t2, 31, $trap_from_kernel
	nop

	/* if trapped from user space load proc's ksp pa */
	mfctl	cr30, t1
	depi	1, T_USER_POS, 1, r1
	depi	1, TFF_LAST_POS, 1, r1
	ldw	u_pcb+pcb_uva(t1), t3
	b	$trap_make_frame
	ldo	NBPG(t3), t3

$trap_from_kernel
	/* align stack */
	ldo	TF_PHYS-1(sp), t3
	dep	r0, 31, 6, t3

$trap_make_frame	
	/* t3 is va, t1 is pa of (struct trapframe *) */
	mfctl	tr3, t2
	mtctl	t3, tr3
	ldi	$trap_tmp_save, t3	/* we know it's in the low mem XXX */
	mfctl	tr2, t1
	stw	t1, TF_R22(t3)
	stw	t2, TF_R21(t3)

	mfctl	tr5, t1
	stw	sp, TF_R30(t3)	/* sp */
	stw	t1, TF_R20(t3)	/* t3 */

	/*
	 * Now, save away other volatile state that prevents us from turning
	 * the PC queue back on, namely, the pc queue and ipsw, and the
	 * interrupt information.
	 */

	mfctl	pcsq, t1
	mtctl	r0, pcsq
	mfctl	pcsq, t2
	stw	t1, TF_IISQH(t3)
	stw	t2, TF_IISQT(t3)
	mtctl	r0, pcsq

	mfctl	pcoq, t1
	mtctl	r0, pcoq
	mfctl	pcoq, t2
	stw	t1, TF_IIOQH(t3)
	stw	t2, TF_IIOQT(t3)

	mfctl	eiem, t1
	mfctl	ipsw, t2
	stw	t1, TF_CR15(t3)
	stw	t2, TF_CR22(t3)

	mfsp	sr3, t1
	mfctl	pidr1, t2
	stw	t1, TF_SR3(t3)
	stw	t2, TF_CR8(t3)

	mfctl	isr, t1
	mfctl	ior, t2
	stw	t1, TF_CR20(t3)
	stw	t2, TF_CR21(t3)

	mfctl	iir, t2
	stw	t2, TF_CR19(t3)
	stw	r1, TF_FLAGS(t3)
	mfctl	tr7, r1

	/*
	 * Setup kernel context
	 */

	ldi	HPPA_PID_KERNEL,t1
	mtctl	t1, pidr1
	mtsp	r0, sr3

	/* load the space queue */
	mtctl	r0, pcsq
	mtctl	r0, pcsq

	/* this will enable interrupts after `cold' */
	ldil	L%kpsw, t1
	ldw	R%kpsw(t1), t2
	mtctl	r0, eiem
	mtctl	t2, ipsw

	/* load in the address to "return" to with the rfir instruction */
	ldil	L%$trapnowvirt, t1
	ldo	R%$trapnowvirt(t1), t1

	/*
	 * load the offset queue, space queue was loaded as a side effect of
	 * saving the space queue above
	 */
	mtctl	t1, pcoq
	ldo	4(t1), t1
	mtctl	t1, pcoq

	/*
	 * Must do rfir not rfi since we may be called from tlbmiss routine
	 * (to handle page fault) and it uses the shadowed registers.
	 *
	 * Also translate the t3 (trapframe) back into va
	 */
	mfctl	tr3, t3
	ldo	HPPA_FRAME_SIZE+TF_SIZE(t3), sp
	rfir
	nop

$trapnowvirt
	/*
	 * t3 contains the physical address of the trapframe
	 * sp is loaded w/ the right VA (we did not need it being physical)
	 */

	mfsp	sr0, t1
	mfsp	sr1, t2
	stw	t1, TF_SR0(sr3, t3)
	stw	t2, TF_SR1(sr3, t3)

	mfsp	sr2, t1
	mfsp	sr4, t2
	stw	t1, TF_SR2(sr3, t3)
	stw	t2, TF_SR4(sr3, t3)

	mfsp	sr5, t2
	mfsp	sr6, t1
	stw	t2, TF_SR5(sr3, t3)
	stw	t1, TF_SR6(sr3, t3)

	mfsp	sr7, t1
	mfctl	pidr2, t2
	stw	t1, TF_SR7(sr3, t3)
	stw	t2, TF_CR9(sr3, t3)

	mtsp	r0, sr0
	mtsp	r0, sr1
	mtsp	r0, sr2
	mtsp	r0, sr4
	mtsp	r0, sr5
	mtsp	r0, sr6
	mtsp	r0, sr7

#if pbably_not_worth_it
	mfctl	pidr3, t1
	mfctl	pidr4, t2
	stw	t1, TF_CR12(t3)
	stw	t2, TF_CR13(t3)
#endif

	/*
	 * Save all general registers that we haven't saved already
	 */

	mfctl	sar, t1
	stw	t1, TF_CR11(t3)
	stw	r1,TF_R1(t3)
	stw	r2,TF_R2(t3)
#ifdef DDB
	stw	rp, HPPA_FRAME_CRP(sp)
	stw	r3, -HPPA_FRAME_SIZE(sp)
#endif
	stw	r3,TF_R3(t3)
	stw	r4,TF_R4(t3)
	stw	r5,TF_R5(t3)
	stw	r6,TF_R6(t3)
	stw	r7,TF_R7(t3)
	stw	r8,TF_R8(t3)
	stw	r9,TF_R9(t3)
	stw	r10,TF_R10(t3)
	stw	r11,TF_R11(t3)
	stw	r12,TF_R12(t3)
	stw	r13,TF_R13(t3)
	stw	r14,TF_R14(t3)
	stw	r15,TF_R15(t3)
	stw	r16,TF_R16(t3)
	stw	r17,TF_R17(t3)
	stw	r18,TF_R18(t3)
	stw	r19,TF_R19(t3)
	/*	r20 already saved (t1)
	 *	r21 already saved (t2)
	 *	r22 already saved (t3) */
	stw	r23,TF_R23(t3)
	stw	r24,TF_R24(t3)
	stw	r25,TF_R25(t3)
	stw	r26,TF_R26(t3)
	stw	r27,TF_R27(t3)
	stw	r28,TF_R28(t3)
	stw	r29,TF_R29(t3)
 	/*	r30 already saved (sp) */
	stw	r31,TF_R31(t3)

	/*
	 * Copy partially saved state from the store into the frame
	 */
	ldi	$trap_tmp_save, arg0
	copy	t3, arg1
	ldo	TF_PHYS(arg0), arg2
$trap_copy_loop
	ldw	0(arg0), t1
	ldw	4(arg0), t2
	ldo	8(arg0), arg0
	stw	t1, 0(arg1)
	stw	t2, 4(arg1)
	comb,<,n arg0, arg2, $trap_copy_loop
	ldo	8(arg1), arg1

	/*
	 * Save the necessary control registers that were not already saved.
	 */

	mfctl	rctr, t1
	stw	t1, TF_CR0(t3)
	/* XXX save ccr here w/ rctr */
	
#ifdef DDB
	/*
	 * Save hpt mask and v2p translation table pointer
	 */
	mfctl	eirr, t1
	mfctl	hptmask, t2
	stw	t1, TF_CR23(t3)
	stw	t2, TF_CR24(t3)

	mfctl	vtop, t1
	mfctl	cr28, t2
	stw	t1, TF_CR25(t3)
	stw	t2, TF_CR28(t3)
#endif
	mfctl	cr30, t1
	stw	t1, TF_CR30(t3)

	/*
	 * load the global pointer for the kernel
	 */

	ldil	L%$global$, dp
	ldo	R%$global$(dp), dp

	/*
	 * call the C routine trap().
	 * form trap type in the first argument to trap()
	 */
	ldw	TF_FLAGS(t3), arg0
	dep	r0, 26, 27, arg0
	copy	t3, arg1

#ifdef DDB
	ldo	-HPPA_FRAME_SIZE(sp), r3
#endif
	.import	trap, code
	ldil	L%trap,t1
	ldo	R%trap(t1),t1
	copy	t3, r5
	.call
	blr	r0,rp
	bv,n	r0(t1)
	nop

	/* see if curproc have changed */
	ldw	TF_FLAGS(r5), arg0
	bb,>=,n	arg0, TFF_LAST_POS, $syscall_return
	copy	r5, t3

	/* see if curproc have really changed */
	ldil	L%curproc, t1
	ldw	R%curproc(t1), t2
	comb,=,n r0, t2, $syscall_return
	copy	r5, t3

	/* means curproc have actually changed */
	b	$syscall_return
	ldw	p_md(t2), t3

	.export	$trap$all$end, entry
$trap$all$end

	.align 32

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
	stwm	rp,4(arg0)	/* Save the return pointer */
	stwm	sp,4(arg0)	/* Save the original stack pointer */

	bv	0(rp)
	copy	r0, ret0
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

	bv	0(rp)
	copy	arg1,ret0	/* Move return value to where it belongs. */
EXIT(longjmp)


	.align	NBPG	/* let's fit 'em on a single page */

#define	FUSUX(name)				  \
ENTRY(name)					! \
	ldil	L%VM_MAXUSER_ADDRESS, t1	! \
	comb,>>= arg0, t1, fusubadaddr		! \
	ldil	L%curproc, t1			! \
	ldw	R%curproc(t1), t1		! \
	ldw	p_addr(t1), t1			! \
	ldil	L%fusufault, t2			! \
	ldo	R%fusufault(t2), t2		! \
	ldw	u_pcb+pcb_onfault(t1), t3	! \
	stw	t2, u_pcb+pcb_onfault(t1)	! \
	ldw	u_pcb+pcb_space(t1), t2		! \
	mtsp	t2, sr1

#define	FUX(name,insn)				  \
	FUSUX(name)				! \
	insn	0(sr1, arg0), ret0		! \
	bv	r0(rp)				! \
	stw	r0, u_pcb+pcb_onfault(t1)	! \
EXIT(name)

#define	SUX(name,insn)				  \
	FUSUX(name)				! \
	insn	arg1, 0(sr1, arg0)		! \
	bv	r0(rp)				! \
	stw	r0, u_pcb+pcb_onfault(t1)	! \
EXIT(name)

ENTRY(fusufault)
	stw	r0, u_pcb+pcb_onfault(t1)
ALTENTRY(fusubadaddr)
	bv	0(rp)
	ldi	-1, ret0
EXIT(fusuexit)

FUX(fubyte,   ldb)
FUX(fusword,  ldh)
FUX(fuword,   ldw)
FUX(fuswintr, ldh)
SUX(subyte,   stb)
SUX(susword,  sth)
SUX(suword,   stw)
SUX(suswintr, sth)

	.align	64

ENTRY(copy_on_fault)
	bv	0(rp)
	ldi	EFAULT, %ret0
EXIT(copy_on_fault)

/*
 * int spstrcpy (pa_space_t ssp, const void *src, pa_space_t dsp, void *dst,
 *		 size_t size, size_t *rsize)
 * do a space to space strncpy, return actual copy size in the rsize;
 */
ENTRY(spstrcpy)
	/* setup fault handler */
	ldil	L%curproc, r31
	ldw	R%curproc(r31), r31
	ldw	p_addr(r31), r31
	ldil	L%copy_on_fault, t2
	ldo	R%copy_on_fault(t2), t2
	stw	t2, pcb_onfault+u_pcb(r31)

	ldw	HPPA_FRAME_ARG(4)(sp), ret1
	mfsp	sr2, ret0	/* XXX need this? */
	mtsp	arg0, sr1
	mtsp	arg2, sr2
	add	ret1, arg1, ret1

$spstrcpy_loop
	ldbs,ma	1(sr1, arg1), t1
	comb,=	ret1, arg1, $spstrcpy_exit
	stbs,ma	t1, 1(sr2, arg3)
	comb,<>,n r0, t1, $spstrcpy_loop

$spstrcpy_exit
	/* reset fault handler */
	stw	r0, pcb_onfault+u_pcb(r31)
	copy	r0, ret0
	ldw	HPPA_FRAME_ARG(4)(sp), t1
	mtsp	ret0, sr2
	sub	ret1, t1, ret1
	bv	0(rp)
	stw	ret1, HPPA_FRAME_ARG(5)(sp)
EXIT(spstrcpy)


/*
 * adjust the time value
 * XXX: do it the easy way, later we will calculate actual fuzz from itr
 */
ENTRY(microtime)

	.import time, data
	ldil	L%-1000000, t3
	ldil	L%time, t1
	ldo	R%-1000000(t3), t3

	/* t4 = splhigh() */
	mfctl	eiem, t4
	mtctl	r0, eiem

	ldw	R%time+4(t1), t2
	ldw	R%time(t1), t1

	/* splx(t4) */
	mtctl	t4, eiem

	add	1, t2, t2
	addb,<	t2, t3, microtime_no
	add	1, t1, t1

	copy	t3, t2

microtime_no
	stwm	t1, 4(arg0)
	bv	(rp)
	stw	t2, 0(arg0)
	
EXIT(microtime)

	.import	whichqs, data
	.import	qs, data
/*
 * setrunqueue(struct proc *p);
 * Insert a process on the appropriate queue.  Should be called at splclock().
 */
	.align	32
ENTRY(setrunqueue)
#ifdef DIAGNOSTIC
	ldw	p_back(arg0), t1
	comb,<>,n r0, t1, $setrunqueue_panic
	ldw	p_wchan(arg0), t1
	comb,<>,n r0, t1, $setrunqueue_panic
	ldb	p_stat(arg0), t1
	comib,=,n SRUN, t1, $setrunqueue_ok
$setrunqueue_panic
	copy	arg0, arg1
	ldil	L%panic, r1
	ldil	L%Lsrqpstr, arg0
	ldo	R%panic(r1), r1
	ldo	R%Lsrqpstr(arg0), arg0
	.call
	blr	%r0, rp
	bv,n	%r0(r1)
	nop
Lsrqpstr
	.asciz	"setrunqueue(%p)"
	.align	8
$setrunqueue_ok
#endif

	ldb	p_priority(arg0), t2
	ldil	L%qs, t4
	extru	t2, 29, 5, t1
	ldo	R%qs(t4), t4
	sh3add	t1, t4, t4
	ldil	L%whichqs, t2
	ldw	R%whichqs(t2), t3
	mtctl	t1, sar
	vdepi	1, 1, t3
	stw	t3, R%whichqs(t2)

#if 0
	/* this actually trashes all the regs we use, be advised ;) */
	copy	t1, arg1
	copy	t4, arg2
	ldil	L%printf, r1
	ldil	L%Lsrqfmt, arg0
	ldo	R%printf(r1), r1
	ldo	R%Lsrqfmt(arg0), arg0
	.call
	blr	%r0, rp
	bv,n	%r0(r1)
	nop
#endif
	ldw	p_back(t4), t2
	stw	t4, p_forw(arg0)
	stw	arg0, p_back(t4)
	stw	arg0, p_forw(t2)
	bv	0(rp)
	stw	t2, p_back(arg0)
Lsrqfmt
	.asciz	"setrunqueue: bit=%x, qs=%p\n"
	.align	8
EXIT(setrunqueue)

/*
 * remrunqueue(struct proc *p);
 * Remove a process from its queue.  Should be called at splclock().
 */
	.align	32
ENTRY(remrunqueue)
	ldb	p_priority(arg0), t2
	extru	t2, 29, 5, t1
	mtsar	t1
	ldil	L%whichqs, t2
	ldw	R%whichqs(t2), t3

#ifdef DIAGNOSTIC
	bvb,<,n	t3, remrunqueue_ok

Lremrunqueue_panic
	copy	arg0, arg1
	copy	t1, arg2
	ldil    L%panic, r1
	ldil	L%Lrrqpstr, arg0
	ldo     R%panic(r1), r1
	ldo	R%Lrrqpstr(arg0), arg0
	.call
	blr     %r0, rp
	bv,n    %r0(r1)

Lrrqpstr
	.asciz	"remrunqueue(%p), bit=%x"
	.align	8
remrunqueue_ok
#endif
	ldw	p_back(arg0), t4
	stw	r0, p_back(arg0)
	ldw	p_forw(arg0), arg0
	stw	arg0, p_forw(t4)
	stw	t4, p_back(arg0)
	comb,<>	t4, arg0, Lqnempty
	nop

	vdepi	1, 1, t3
	stw	t3, R%whichqs(t2)
Lqnempty
	bv	0(rp)
	nop
EXIT(remrunqueue)

/*
 * cpu_switch()
 * Find the highest priority process and resume it.
 */
	.align	32
ENTRY(cpu_switch)

	/*
	 * Clear curproc so that we don't accumulate system time while idle.
	 */
	ldil	L%curproc, t1
	ldw	R%curproc(t1), arg2
	stw	r0, R%curproc(t1)
	/* remain on the old (curproc)'s stack until we have better choice */

	/*
	 * arg3: spl
	 * t1:	&whichqs
	 * t2:	old curproc
	 *
	 */

switch_search
	/* arg3 = splhigh() */
	mfctl	eiem, arg3
	ldil	L%whichqs, t1
	ldi	-1, t2
idle_loop
	mtctl	r0, eiem
	ldw	R%whichqs(t1), t3

	comb,<>	r0, t3, gotprocs
	nop

	mtctl	t2, eiem

	/* XXX do idle work here */
	nop ! nop ! nop ! nop ! nop ! nop ! nop ! nop

	b	idle_loop
	nop

gotprocs
	ldi	0, t4
getbit
	mtsar	t4
	bvb,>=,n t3, getbit
	ldo	1(t4), t4

	ldil	L%qs, t2
	ldo	R%qs(t2), t2
	sh3add	t4, t2, t2

	ldw	p_forw(t2), arg1
#ifdef DIAGNOSTIC
	comb,<>	t2, arg1, link_ok
	nop
switch_error
	copy	t4, arg1
	copy	t2, arg2
	ldil    L%panic, r1
	ldil	L%Lcspstr, arg0
	ldo     R%panic(r1), r1
	ldo	R%Lcspstr(arg0), arg0
	.call
	blr     %r0, rp
	bv,n    %r0(r1)
	nop
Lcspstr
	.asciz	"cpu_switch: bit=%x, q/p=%p"
	.align	8
link_ok
#endif
	ldw	p_forw(arg1), arg0
	stw	arg0, p_forw(t2)
	stw	t2, p_back(arg0)
	stw	r0, p_back(arg1)

	comb,<> arg0, t2, sw_qnempty
	nop

	vdepi	0, 1, t3
	stw	t3, R%whichqs(t1)

	/* don't need &whichqs (t1) starting here */
sw_qnempty
	ldil	L%want_resched, t3
	stw	r0, R%want_resched(t3)

#ifdef DIAGNOSTIC
	ldw	p_wchan(arg1), t1
	comb,<>,n r0, t1, switch_error
	copy	arg1, t2
	ldb	p_stat(arg1), t1
	comib,<>,n SRUN, t1, switch_error
	copy	arg1, t2
#endif
	ldil	L%curproc, t1
	stw	arg1, R%curproc(t1)

	/* Skip context switch if same process. */
	comb,=,n arg1, arg2, switch_return

	/* If old process exited, don't bother. */
	comb,=,n r0, arg2, switch_exited

	/*
	 * 2. save old proc context
	 *
	 * arg2: old proc
	 */
	ldw	p_md(arg2), t1
	copy	sp, t2
	ldo	HPPA_FRAME_SIZE+16*4(sp), sp
	ldw	TF_R30(t1), t3
	stw	t2, HPPA_FRAME_PSP(sp)
	stw	rp, HPPA_FRAME_CRP(sp)
	stw	t3, HPPA_FRAME_ARG(0)(sp)
	stw	sp, TF_R30(t1)
	fdc	r0(t1)
	/* save callee-save registers */
	stw	r3,   0*4(t2)
	stw	r4,   1*4(t2)
	stw	r5,   2*4(t2)
	stw	r6,   3*4(t2)
	stw	r7,   4*4(t2)
	stw	r8,   5*4(t2)
	stw	r9,   6*4(t2)
	stw	r10,  7*4(t2)
	stw	r11,  8*4(t2)
	stw	r12,  9*4(t2)
	stw	r13, 10*4(t2)
	stw	r14, 11*4(t2)
	stw	r15, 12*4(t2)
	stw	r16, 13*4(t2)
	stw	r17, 14*4(t2)
	stw	r18, 15*4(t2)

	/* don't need old curproc (arg2) starting from here */
switch_exited
	/*
	 * 3. restore new proc context
	 *
	 * arg1: new proc
	 */
	ldw	p_md(arg1), t1
	ldw	TF_CR30(t1), t2
	ldw	TF_R30(t1), sp
	ldw	TF_CR9(t1), t3
	mtctl	t3, pidr2
	mtctl	t2, cr30
	ldw	HPPA_FRAME_ARG(0)(sp), t3
	ldw	HPPA_FRAME_CRP(sp), rp
	ldw	HPPA_FRAME_PSP(sp), t2
	stw	t3, TF_R30(t1)
	fdc	r0(t1)
	ldw	 0*4(t2), r3
	ldw	 1*4(t2), r4
	ldw	 2*4(t2), r5
	ldw	 3*4(t2), r6
	ldw	 4*4(t2), r7
	ldw	 5*4(t2), r8
	ldw	 6*4(t2), r9
	ldw	 7*4(t2), r10
	ldw	 8*4(t2), r11
	ldw	 9*4(t2), r12
	ldw	10*4(t2), r13
	ldw	11*4(t2), r14
	ldw	12*4(t2), r15
	ldw	13*4(t2), r16
	ldw	14*4(t2), r17
	ldw	15*4(t2), r18
	copy	t2, sp

switch_return
	bv	0(rp)
	mtctl	arg3, eiem

EXIT(cpu_switch)

/*
 * switch_exit(struct proc *p)
 * restore proc0 context and go into cpu_switch to select the next runable
 * process.
 */
	.import	kernel_map, data
	.import	uvmspace_free, code
	.import	uvm_km_free, code
ENTRY(switch_exit)

	ldil	L%proc0, t2
	ldo	R%proc0(t2), t2

	ldw	p_md(t2), t1
	ldw	TF_CR30(t1), t1
	mtctl	t1, cr30

	/* setup kernel context */
	mtctl	r0, sr0
	mtctl	r0, sr1
	mtctl	r0, sr2
	mtctl	r0, sr3
	mtctl	r0, sr4
	mtctl	r0, sr5
	mtctl	r0, sr6
	mtctl	r0, sr7

	/* leave pidr2 in user space so copy* work */
	ldi	HPPA_PID_KERNEL, t4
	mtctl	t4, pidr1

	b	switch_search
	nop
EXIT(switch_exit)

ENTRY(switch_trampoline)
	/* XXX load curproc here? */
	ldw	HPPA_FRAME_ARG(1)(sp), t1
	ldw	HPPA_FRAME_ARG(2)(sp), arg0
	.call
	blr     %r0, rp
	bv,n    %r0(t1)
	nop
	ldil	L%curproc, t1
	ldw	R%curproc(t1), t2
	b	$syscall_return
	ldw	p_md(t2), t3
EXIT(switch_trampoline)

/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 */
ENTRY(sigcode)
	/* TODO signal trampoline */
	bv	0(rp)
	nop
ALTENTRY(esigcode)
EXIT(sigcode)

#ifdef COMPAT_LINUX
ENTRY(linux_sigcode)

	/* TODO linux signal trampoline */
	bv	0(rp)
	nop
ALTENTRY(linux_esigcode)
EXIT(linix_sigcode)
#endif /* COMPAT_LINUX */

	.end
