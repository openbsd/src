/*	$OpenBSD: locore.S,v 1.94 2002/10/25 22:02:19 mickey Exp $	*/

/*
 * Copyright (c) 1998-2002 Michael Shalayeff
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
 *	Utah $Hdr: locore.s 1.62 94/12/15$
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
#include <sys/reboot.h>
#include <machine/param.h>
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/iomod.h>
#include <machine/pdc.h>
#include <machine/intr.h>
#include <machine/frame.h>
#include <machine/reg.h>
#ifdef	GPROF
#include <machine/gprof.h>
#endif
#include "assym.h"

/*
 * hv-specific instructions
 */
#define	DR_PAGE0	.word	0x14001200
#define	DR_PAGE1	.word	0x14001240
#define	MTCPU_T(x,t)	.word	0x14001400 | ((t) << 21) | ((x) << 16)
#define	MFCPU_T(r,x)	.word	0x14001400 | ((r) << 21) | (x)
#define	MTCPU_C(x,t)	.word	0x14000240 | ((t) << 21) | ((x) << 16)
#define	MFCPU_C(r,x)	.word	0x14000600 | ((r) << 21) | ((x) << 16)

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

#define	EXIT_STACKSIZE	(2*NBPG)
#define	EMRG_STACKSIZE	(2*NBPG)
#define	FPEMU_STACKSIZE	(2*NBPG)

	.data
$trap_tmp_save			/* XXX assumed to be aligned on 2048 */
	.block	TF_PHYS		/* XXX must be aligned to 64 */

	.globl	intrcnt, eintrcnt, intrnames, eintrnames
intrcnt
	.block	16 * 4
eintrcnt
intrnames
	.asciz	"spur"
	.asciz	"lev1"
	.asciz	"lev2"
	.asciz	"lev3"
	.asciz	"lev4"
	.asciz	"lev5"
	.asciz	"lev6"
	.asciz	"lev7"
	.asciz	"lev8"
	.asciz	"lev9"
	.asciz	"lev10"
	.asciz	"lev11"
	.asciz	"lev12"
	.asciz	"lev13"
	.asciz	"lev14"
	.asciz	"lev15"
	.asciz	"clock"
eintrnames
	.align	NBPG

	BSS(pdc_stack, 4)	/* temp stack for PDC call */
	BSS(emrg_stack, 4)	/* stack for HPMC/TOC/PWRF */
	BSS(fpemu_stack, 4)	/* stack for FPU emulation */

	.export	kernelmapped, data
	BSS(kernelmapped, 4)	/* set when kernel is mapped */
	.export fpu_csw, data
	BSS(fpu_csw, 4)		/* count fpu context switches */
	.export	fpu_curpcb, data
	BSS(fpu_curpcb, 4)	/* pcb of the fpu owner */
	.export fpu_enable, data
	BSS(fpu_enable, 4)	/* bits to set in the ccr to enable fpu */
	.export hppa_vtop, data
	BSS(hppa_vtop, 4)	/* a vtop translation table addr (pa=va) */
	.export netisr, data
	.align 16
netisr
	.word	0
	.export sir, data
	.align 16
sir
	.word	0

	.text
	.import	$kernel_setup, entry

/*
 * This is the starting location for the kernel
 */
ENTRY($start,0)
/*
 *	start(pdc, boothowto, bootdev, esym, bootapiver, argv, argc)
 *
 *	pdc - PDC entry point
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

	/* assuming size being page-aligned */
#define	STACK_ALLOC(n,s)	\
	ldil	L%(n), t1	! \
	ldil	L%(s), t2	! \
	stw	arg3, R%(n)(t1)	! \
	add	arg3, t2, arg3

	STACK_ALLOC(pdc_stack, PDC_STACKSIZE)
	STACK_ALLOC(emrg_stack, EMRG_STACKSIZE)
	STACK_ALLOC(fpemu_stack, FPEMU_STACKSIZE)

#undef	STACK_ALLOC

	/* zero fake trapframe and proc0 u-area */
	copy	arg3, t2
	ldi	NBPG+TRAPFRAME_SIZEOF, t1
$start_zero_tf
	stws,ma r0, 4(t2)
	addib,>= -8, t1, $start_zero_tf
	stws,ma r0, 4(t2)	/* XXX could use ,bc here, but gas is broken */

	/*
	 * kernel stack lives here (arg3 is page-aligned esym)
	 * initialize the pcb
	 * arg0 will be available space for hppa_init()
	 */
	ldo	NBPG+TRAPFRAME_SIZEOF(arg3), sp
	mtctl	arg3, cr30
	stw	r0, U_PCB+PCB_ONFAULT(arg3)
	stw	r0, U_PCB+PCB_SPACE(arg3)	/* XXX HPPA_SID_KERNEL == 0 */
	stw	arg3, U_PCB+PCB_UVA(arg3)
	ldil	L%(USPACE+NBPG), arg0		/* normal U plus red zone */
	add	arg0, arg3, arg0
	ldil	L%proc0paddr, t1
	stw	arg3, R%proc0paddr(t1)
	ldil	L%proc0, t2
	stw	arg3, R%proc0+P_ADDR(t2)
	ldo	-TRAPFRAME_SIZEOF(sp), t3
	stw	t3, R%proc0+P_MD_REGS(t2)

	ldil	L%TFF_LAST, t1
	stw	t1, TF_FLAGS-TRAPFRAME_SIZEOF(sp)
	stw	arg3, TF_CR30-TRAPFRAME_SIZEOF(sp)

	/*
	 * disable all coprocessors
	 */
	mtctl	r0, ccr

	copy	sp, arg1
	ldil	L%$qisnowon, rp
	ldo	R%$qisnowon(rp), rp
	b	$kernel_setup
	ldi	PSL_Q|PSL_I, arg2

$qisnowon
	/*
	 * call C routine hppa_init() to initialize VM
	 */
	ldil	L%hppa_init, r1
	ldo	R%hppa_init(r1), r1
	.import hppa_init, code
	.call
	blr	r0, rp
	bv,n	(r1)
	nop

	/*
	 * Cannot change the queues or IPSW with the Q-bit on
	 */
	rsm	RESET_PSL, r0
	nop ! nop ! nop ! nop ! nop ! nop ! nop

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
	blr	r0, rp
	bv,n	(r1)
	nop

	/* should never return... */
	bv	(rp)
	nop
EXIT($start)

LEAF_ENTRY($kernel_setup)

	/*
	 * disable interrupts and turn off all bits in the psw so that
	 * we start in a known state.
	 */
	rsm	RESET_PSL, r0
	nop ! nop ! nop ! nop ! nop ! nop

	/* get things ready for the kernel to run in virtual mode */
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
	 * to keep the spl() routines consistent we need to put the correct
	 * spl level into eiem, and reset any pending interrupts
	 */
	ldi	-1, r1
	mtctl	r0, eiem	/* IPL_NONE */
	mtctl	r1, eirr

	/*
	 * load address of interrupt vector table
	 */
	ldil	L%$ivaaddr, t2
	ldo	R%$ivaaddr(t2), t2
	mtctl	t2, iva

	/*
	 * set up the dp pointer so that we can do quick references off of it
	 */
	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/*
	 * Create a stack frame for us to call C with. Clear out the previous
	 * sp marker to mark that this is the first frame on the stack.
	 */
	copy	arg1, sp
	ldo	0(arg1), r3
	stw,ma	r0, HPPA_FRAME_SIZE(sp)
	stw	r0, HPPA_FRAME_CRP(sp)
	stw	r0, HPPA_FRAME_PSP(sp)

	/*
	 * We need to set the Q bit so that we can take TLB misses after we
	 * turn on virtual memory.
	 */
	mtctl	r0, pcsq
	mtctl	r0, pcsq
	mtctl	rp, pcoq
	ldo	4(rp), rp
	mtctl	rp, pcoq
	mtctl	arg2, ipsw
	rfi
	nop
	nop
EXIT($kernel_setup)

/* int
 * pdc_call(func, pdc_flag, ...)
 *	iodcio_t func;
 *	int pdc_flag;
 */
ENTRY(pdc_call,160)

	mfctl	eiem, t1
	mtctl	r0, eiem
	stw	rp, HPPA_FRAME_CRP(sp)
	copy	arg0, r31
	copy	sp, ret1

	ldil	L%kernelmapped, ret0
	ldw	R%kernelmapped(ret0), ret0
	comb,=	r0, ret0, pdc_call_unmapped1
	nop
	ldil	L%pdc_stack, ret1
	ldw	R%pdc_stack(ret1), ret1

pdc_call_unmapped1
	copy	sp, r1
	ldo	HPPA_FRAME_SIZE+24*4(ret1), sp

	stw	r1, HPPA_FRAME_PSP(sp)

	/* save kernelmapped and eiem */
	stw	ret0, HPPA_FRAME_ARG(21)(sp)
	stw	t1, HPPA_FRAME_ARG(22)(sp)

	/* copy arguments */
	copy	arg2, arg0
	copy	arg3, arg1
	ldw	HPPA_FRAME_ARG(4)(r1), arg2
	ldw	HPPA_FRAME_ARG(5)(r1), arg3
	ldw	HPPA_FRAME_ARG(6)(r1), t1
	ldw	HPPA_FRAME_ARG(7)(r1), t2
	ldw	HPPA_FRAME_ARG(8)(r1), t3
	ldw	HPPA_FRAME_ARG(9)(r1), t4
	stw	t1, HPPA_FRAME_ARG(4)(sp)	/* XXX can use ,bc */
	stw	t2, HPPA_FRAME_ARG(5)(sp)
	stw	t3, HPPA_FRAME_ARG(6)(sp)
	stw	t4, HPPA_FRAME_ARG(7)(sp)
	ldw	HPPA_FRAME_ARG(10)(r1), t1
	ldw	HPPA_FRAME_ARG(11)(r1), t2
	ldw	HPPA_FRAME_ARG(12)(r1), t3
	ldw	HPPA_FRAME_ARG(13)(r1), t4
	stw	t1, HPPA_FRAME_ARG(8)(sp)
	stw	t2, HPPA_FRAME_ARG(9)(sp)
	stw	t3, HPPA_FRAME_ARG(10)(sp)
	stw	t4, HPPA_FRAME_ARG(11)(sp)

	/* save temp control regs */
	mfctl	cr24, t1
	mfctl	cr25, t2
	mfctl	cr26, t3
	mfctl	cr27, t4
	stw	t1, HPPA_FRAME_ARG(12)(sp)	/* XXX can use ,bc */
	stw	t2, HPPA_FRAME_ARG(13)(sp)
	stw	t3, HPPA_FRAME_ARG(14)(sp)
	stw	t4, HPPA_FRAME_ARG(15)(sp)
	mfctl	cr28, t1
	mfctl	cr29, t2
	mfctl	cr30, t3
	mfctl	cr31, t4
	stw	t1, HPPA_FRAME_ARG(16)(sp)
	stw	t2, HPPA_FRAME_ARG(17)(sp)
	stw	t3, HPPA_FRAME_ARG(18)(sp)
	stw	t4, HPPA_FRAME_ARG(19)(sp)

	comb,=	r0, ret0, pdc_call_unmapped2
	nop

	copy	arg0, t4
	ldi	PSL_Q, arg0 /* (!pdc_flag && args[0] == PDC_PIM)? PSL_M:0) */
	break	HPPA_BREAK_KERNEL, HPPA_BREAK_SET_PSW
	nop
	stw	ret0, HPPA_FRAME_ARG(23)(sp)
	copy	t4, arg0

pdc_call_unmapped2
	.call
	blr	r0, rp
	bv,n	(r31)
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

	ldw	HPPA_FRAME_ARG(21)(sp), t1
	ldw	HPPA_FRAME_ARG(22)(sp), t2
	comb,=	r0, t1, pdc_call_unmapped3
	nop

	copy	ret0, t3
	ldw	HPPA_FRAME_ARG(23)(sp), arg0
	break	HPPA_BREAK_KERNEL, HPPA_BREAK_SET_PSW
	nop
	copy	t3, ret0

pdc_call_unmapped3
	ldw	HPPA_FRAME_PSP(sp), sp
	ldw	HPPA_FRAME_CRP(sp), rp
	bv	r0(rp)
	mtctl	t2, eiem
EXIT(pdc_call)

/*
 * Kernel Gateway Page (must be at known address)
 *	System Call Gate
 *	Signal Return Gate
 *
 * GATEway instructions have to be at a fixed known locations
 * because their addresses are hard coded in routines such as
 * those in the C library.
 */
	.align	NBPG
	.export	gateway_page, entry
gateway_page
	nop				/* @ 0.C0000000 (Nothing)  */
	gate,n	$bsd_syscall,r0		/* @ 0.C0000004 (HPUX/BSD) */
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
	mfctl	eiem, r1
	mtctl	r0, eiem
	mtsp	r0, sr1
	mfctl	pidr1, r28
	ldi	HPPA_PID_KERNEL, t2
	mtctl	t2, pidr1

	.import $syscall,code
	.call
	ldil	L%$syscall, t2
	be	R%$syscall(sr1, t2)
	nop ! nop ! nop ! nop

	.align	NBPG
	.export	gateway_page_end, entry
gateway_page_end

	.export $syscall,entry
	.proc
	.callinfo calls
	.entry
$syscall
	/*
	 *
	 * t1:	syscall number
	 * t2:	user
	 * t3:	args
	 * t4:	user stack
	 *
	 */
	ldil	L%curproc, t2
	ldw	R%curproc(sr1, t2), t2
	ldw	P_ADDR(sr1, t2), t3	/* XXX can use ,sl */

	/* calculate kernel sp, load, create kernel stack frame */
	ldo	NBPG(t3), t3
	stw	t3, P_MD_REGS(sr1, t2)
	ldo	TRAPFRAME_SIZEOF(t3), t3
	stw	t4, TF_R19 -TRAPFRAME_SIZEOF(sr1, t3)	/* t4 for vfork() */
	stw	t1, TF_R22 -TRAPFRAME_SIZEOF(sr1, t3)	/* syscall # */
	/* gotta save the args, in case we gonna restart */
	stw	arg3, TF_R23-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg2, TF_R24-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg1, TF_R25-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg0, TF_R26-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r27, TF_R27-TRAPFRAME_SIZEOF(sr1, t3)	/* dp */
	stw	sp, TF_R30 -TRAPFRAME_SIZEOF(sr1, t3)	/* user stack */
	copy	t3, sp
	stw,ma	r0, HPPA_FRAME_SIZE+HPPA_FRAME_MAXARGS(sr1, sp)
	stw	r0, HPPA_FRAME_CRP(sr1, sp)
	ldil	L%kpsw, t1
	ldw	R%kpsw(sr1, t1), t1
	stw	r1, TF_CR15-TRAPFRAME_SIZEOF(sr1, t3)	/* eiem ,bc */
	stw	t1, TF_CR22-TRAPFRAME_SIZEOF(sr1, t3)	/* ipsw */
	mfsp	sr3, t1
	stw	t1, TF_SR3-TRAPFRAME_SIZEOF(sr1, t3)
	stw	ret0, TF_CR8-TRAPFRAME_SIZEOF(sr1, t3)	/* pidr1 */
	/* now we can allow interrupts to happen */
	mtctl	r1, eiem

	/*
	 * we believe that any callee-save registers
	 * will be saved accordingly in either syscall()
	 * or deeper called functions and caller-save
	 * are saved in userland.
	 */
	stw	r2 , TF_R2 -TRAPFRAME_SIZEOF(sr1, t3)
	stw	r3 , TF_R3 -TRAPFRAME_SIZEOF(sr1, t3)
	copy	t3, r3
#ifdef DDB
	/* save callee-save registers */
	stw	r4 , TF_R4 -TRAPFRAME_SIZEOF(sr1, t3)
	stw	r5 , TF_R5 -TRAPFRAME_SIZEOF(sr1, t3)
	stw	r6 , TF_R6 -TRAPFRAME_SIZEOF(sr1, t3)
	stw	r7 , TF_R7 -TRAPFRAME_SIZEOF(sr1, t3)
	stw	r8 , TF_R8 -TRAPFRAME_SIZEOF(sr1, t3)
	stw	r9 , TF_R9 -TRAPFRAME_SIZEOF(sr1, t3)
	stw	r10, TF_R10-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r11, TF_R11-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r12, TF_R12-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r13, TF_R13-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r14, TF_R14-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r15, TF_R15-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r16, TF_R16-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r17, TF_R17-TRAPFRAME_SIZEOF(sr1, t3)
	stw	r18, TF_R18-TRAPFRAME_SIZEOF(sr1, t3)
#endif
	/*
	 * Save the rest of the CPU context
	 */
	mfsp	sr0, arg0				/* use ,bc */
	stw	arg0, TF_IISQH-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg0, TF_IISQT-TRAPFRAME_SIZEOF(sr1, t3)

	ldo	4(r31), arg1
	stw	r31, TF_IIOQH-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg1, TF_IIOQT-TRAPFRAME_SIZEOF(sr1, t3)

	stw	arg0, TF_CR20-TRAPFRAME_SIZEOF(sr1, t3)	/* use ,bc */
	stw	r31, TF_CR21-TRAPFRAME_SIZEOF(sr1, t3)

	ldil	L%TFF_LAST|TFF_SYS, arg1
	stw	r0, TF_CR19-TRAPFRAME_SIZEOF(sr1, t3)	/* iir */
	stw	arg1, TF_FLAGS-TRAPFRAME_SIZEOF(sr1, t3)

	mfsp	sr2, arg2
	mfsp	sr4, arg3
	stw	arg0, TF_SR0-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg0, TF_SR1-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg2, TF_SR2-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg3, TF_SR4-TRAPFRAME_SIZEOF(sr1, t3)

	mfsp	sr5, arg0
	mfsp	sr6, arg1
	mfsp	sr7, arg2
	mfctl	pidr2, arg3
	stw	arg0, TF_SR5-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg1, TF_SR6-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg2, TF_SR7-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg3, TF_CR9-TRAPFRAME_SIZEOF(sr1, t3)

#if pbably_not_worth_it
	mfctl	pidr3, arg2
	mfctl	pidr4, arg3
	stw	arg2, TF_CR12-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg3, TF_CR13-TRAPFRAME_SIZEOF(sr1, t3)
#endif

#ifdef DDB
	/*
	 * Save hpt mask and v2p translation table pointer
	 */
	mfctl	eirr, arg0
	mfctl	hptmask, arg1
	stw	arg0, TF_CR23-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg1, TF_CR24-TRAPFRAME_SIZEOF(sr1, t3)

	mfctl	vtop, arg0
	mfctl	cr28, arg1
	stw	arg0, TF_CR25-TRAPFRAME_SIZEOF(sr1, t3)
	stw	arg1, TF_CR28-TRAPFRAME_SIZEOF(sr1, t3)
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

	ldo	-TRAPFRAME_SIZEOF(t3), arg0
	ldo	4(t3), arg1

	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/* do a syscall */
	.import	syscall,code
	ldil	L%syscall, r1
	ldo	R%syscall(r1), r1
	.call
	blr	r0, rp
	bv,n	0(r1)
	nop

	ldil	L%curproc, r1
	ldw	R%curproc(r1), r1
	ldw	P_MD_REGS(r1), t3

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

	/* splhigh(), just in case */
	mtctl	r0, eiem

	/*
	 * 1a. Copy a `phys' part of the frame into temp store
	 *	(see a note for trapall)
	 *	hopefully no page fault would happen on or after the copy,
	 *	and interrupts are disabled.
	 */
	ldil	L%$trap_tmp_save, t2
	/* use ,bc each cache line */
	ldw  0(t3), r1 ! ldw  4(t3), t1 ! stw r1,  0(t2) ! stw t1,  4(t2)
	ldw  8(t3), r1 ! ldw 12(t3), t1 ! stw r1,  8(t2) ! stw t1, 12(t2)
	ldw 16(t3), r1 ! ldw 20(t3), t1 ! stw r1, 16(t2) ! stw t1, 20(t2)
	ldw 24(t3), r1 ! ldw 28(t3), t1 ! stw r1, 24(t2) ! stw t1, 28(t2)
	ldw 32(t3), r1 ! ldw 36(t3), t1 ! stw r1, 32(t2) ! stw t1, 36(t2)
	ldw 40(t3), r1 ! ldw 44(t3), t1 ! stw r1, 40(t2) ! stw t1, 44(t2)
	ldw 48(t3), r1 ! ldw 52(t3), t1 ! stw r1, 48(t2) ! stw t1, 52(t2)
	ldw 56(t3), r1 ! ldw 60(t3), t1 ! stw r1, 56(t2) ! stw t1, 60(t2)

	/* 1b. restore most of the general registers */
	ldw	TF_CR11(t3), t1
	mtctl	t1, sar
	ldw	TF_R1(t3), r1
	ldw	TF_R2(t3), r2
	ldw	TF_R3(t3), r3
#ifdef DDB
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
#endif
	ldw	TF_R19(t3), t4
	/*	r20(t3) is used as a temporary and will be restored later */
	/*	r21(t2) is used as a temporary and will be restored later */
	/*	r22(t1) is used as a temporary and will be restored later */
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
	ldw	TF_CR0(sr3, t3), t1
	mtctl	t1, rctr
	ldw	TF_CR30(sr3, t3), t1
	mtctl	t1, cr30

	/*
	 * clear the system mask, this puts us back into physical mode.
	 * reload trapframe pointer w/ correspondent PA value.
	 * sp will be left in virtual until restored from trapframe,
	 * since we don't use it anyway.
	 */
	rsm	RESET_PSL, r0
	nop ! nop ! nop ! nop ! nop ! nop ! nop
$syscall_return_phys

	ldil	L%$trap_tmp_save, t3

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
#define	TELABEL(num)	__CONCAT(trap_ep_,num)
#define TRAP(name,num) \
	.import TLABEL(name), code	! \
	mtctl	r1, tr7			! \
	ldil	L%TLABEL(name), r1	! \
	.call				! \
	be	R%TLABEL(name)(sr7, r1)	! \
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

#define	STRAP(name,num,pre) \
	.export	TLABEL(name)$num, entry	! \
	.label	TLABEL(name)$num	! \
	pre				! \
	mtctl	r1, tr7			! \
	.export	TELABEL(num), entry	! \
	.label	TELABEL(num)		! \
	ldil	0,r1			! \
	ldo	0(r1), r1		! \
	.call				! \
	bv	0(r1)			! \
	ldi	num, r1

#define	LDILDO(name)			! \
	.export	name, entry		! \
	.label	name			! \
	ldil	L%$name,%r1		! \
	ldo	R%$name(%r1), %r1

#ifdef HP7000_CPU
LDILDO(itlb_x)
LDILDO(itlbna_x)
LDILDO(dtlb_x)
LDILDO(dtlbna_x)
LDILDO(tlbd_x)
#endif

#ifdef HP7100_CPU
LDILDO(itlb_s)
LDILDO(itlbna_s)
LDILDO(dtlb_s)
LDILDO(dtlbna_s)
LDILDO(tlbd_s)
#endif

#ifdef HP7200_CPU
LDILDO(itlb_t)
LDILDO(itlbna_t)
LDILDO(dtlb_t)
LDILDO(dtlbna_t)
LDILDO(tlbd_t)
#endif

#ifdef HP7100LC_CPU
LDILDO(itlb_l)
LDILDO(itlbna_l)
LDILDO(dtlb_l)
LDILDO(dtlbna_l)
LDILDO(tlbd_l)
#endif

#define	ITLBPRE \
	mfctl	pcoq,r9		/* Offset */			! \
	mfctl	pcsq,r8		/* Space  */			! \
	depi	0,31,PGSHIFT,r9	/* align offset to page */
#define	DTLBPRE \
	mfctl	ior, r9		/* Offset */			! \
	mfctl	isr, r8		/* Space  */			! \
	depi	0,31,PGSHIFT,r9	/* align offset to page */
	/* CR28XXX according to a popular belief cr28 should be read here */
#define	HPMCPRE	nop

	.align NBPG
	.export $ivaaddr, entry
	.export hpmc_v, entry
$ivaaddr
	ATRAP(null,T_NONEXIST)		/*  0. invalid interrupt vector */
hpmc_v
	CTRAP(hpmc,T_HPMC,HPMCPRE)	/*  1. high priority machine check */
	ATRAP(power,T_POWERFAIL)	/*  2. power failure */
	ATRAP(recnt,T_RECOVERY)		/*  3. recovery counter trap */
	ATRAP(intr,T_INTERRUPT)		/*  4. external interrupt */
	ATRAP(lpmc,T_LPMC)		/*  5. low-priority machine check */
	STRAP(itlb,T_ITLBMISS,ITLBPRE)	/*  6. instruction TLB miss fault */
	ATRAP(iprot,T_IPROT)		/*  7. instruction protection trap */
	ATRAP(ill,T_ILLEGAL)		/*  8. Illegal instruction trap */
	CTRAP(ibrk,T_IBREAK,)		/*  9. break instruction trap */
	ATRAP(privop,T_PRIV_OP)		/* 10. privileged operation trap */
	ATRAP(privr,T_PRIV_REG)		/* 11. privileged register trap */
	ATRAP(ovrfl,T_OVERFLOW)		/* 12. overflow trap */
	ATRAP(cond,T_CONDITION)		/* 13. conditional trap */
	CTRAP(excpt,T_EXCEPTION,)	/* 14. assist exception trap */
	STRAP(dtlb,T_DTLBMISS,DTLBPRE)	/* 15. data TLB miss fault */
	STRAP(itlbna,T_ITLBMISSNA,DTLBPRE)/* 16. ITLB non-access miss fault */
	STRAP(dtlbna,T_DTLBMISSNA,DTLBPRE)/* 17. DTLB non-access miss fault */
	ATRAP(dprot,T_DPROT)		/* 18. data protection trap
					      unalligned data reference trap */
	ATRAP(dbrk,T_DBREAK)		/* 19. data break trap */
	STRAP(tlbd,T_TLB_DIRTY,DTLBPRE)	/* 20. TLB dirty bit trap */
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

	.export TLABEL(excpt), entry
ENTRY(TLABEL(excpt),0)
	/* assume we never get this one w/o fpu [enabled] */
	copy	rp, r1
	mtctl	arg0, tr5
	.import	fpu_save, code
	.call
	bl	fpu_save, rp
	mfctl	cr30, arg0
	copy	r1, rp
	mtctl	r0, ccr		/* cause a reload after exception */
	ldil	L%fpu_curpcb, r1
	mfctl	cr30, arg0
	stw	r0, R%fpu_curpcb(r1)

	/* now, check for trap */
	ldw	0(arg0), r1
	bb,>=,n	r1, HPPA_FPU_T_POS, excpt_notrap
	ldw	8(arg0), r1
	extru	r1, 5, 6, r1
	comib,<>,n HPPA_FPU_UNMPL, r1, excpt_notrap

	ldw	0(arg0), r1
	depi	0, HPPA_FPU_T_POS, 1, r1
	stw	r1, 0(arg0)
	ldw	8(arg0), r1
	stw	r0, 8(arg0)
	fdc	r0(arg0)
	.import	$fpu_emulate, code
	b	$fpu_emulate
	mfctl	tr5, arg0

excpt_notrap
	fdc	r0(arg0)
	mfctl	tr5, arg0
	sync
	b	TLABEL(all)
	ldi	T_EXCEPTION, r1
EXIT(TLABEL(excpt))

	.export TLABEL(emu), entry
ENTRY(TLABEL(emu),0)

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

	mfctl	iir, r8
	extru	r8, 5, 6, r9	/* no sfu implementation right now */
	comib,=	4, r9, TLABEL(all)
	ldi	T_ILLEGAL, r1

	extru	r8, 25, 3, r9	/* only fpu coprocessor emulation now */
	comib,<< 4, r9, TLABEL(all)
	ldi	T_ILLEGAL, r1

	/* if we are already enabled and hit again, emulate */
	mfctl	ccr, r1
	extru,<> r1, 25, 2, r0
	b,n	$fpusw_set
	nop

#if 0
	/* here we emulate the fld/fst */
	mfctl	iir, r1
	extru	r1, 5, 6, r1
	comib,=	0xb, r9, TLABEL(all)
	ldi	T_ILLEGAL, r1

	mfctl	iir, r1
	extru	r1, 5, 6, r1
	comib,=	0x9, r9, TLABEL(all)
	ldi	T_ILLEGAL, r1
#endif
	.import	$fpu_emulate, code
	b	$fpu_emulate
	nop

$fpusw_set
	/* enable coprocessor */
	depi	3, 25, 2, r1
	mtctl	r1, ccr

	ldil	L%fpu_curpcb, r16
	mfctl	cr30, r9
	ldw	R%fpu_curpcb(r16), r16

	comb,=,n r16, r9, $fpusw_done
	comb,=,n r16, r0, $fpusw_nosave

	copy	arg0, r17
	copy	rp, r1
	copy	r16, arg0
	.import	fpu_save, code
	.call
	bl	fpu_save, rp
	ldo	PCB_FPREGS+U_PCB(arg0), arg0
	copy	r1, rp
	copy	r17, arg0

$fpusw_nosave

	/* count switches */
	ldil	L%fpu_csw, r1
	ldw	R%fpu_csw(r1), r16
	ldo	1(r16), r16
	ldo	31*8+PCB_FPREGS+U_PCB(r9), r17
	stw	r16, R%fpu_csw(r1)

	fldds,ma -8(r17), fr31
	fldds,ma -8(r17), fr30
	fldds,ma -8(r17), fr29
	fldds,ma -8(r17), fr28
	fldds,ma -8(r17), fr27
	fldds,ma -8(r17), fr26
	fldds,ma -8(r17), fr25
	fldds,ma -8(r17), fr24
	fldds,ma -8(r17), fr23
	fldds,ma -8(r17), fr22
	fldds,ma -8(r17), fr21
	fldds,ma -8(r17), fr20
	fldds,ma -8(r17), fr19
	fldds,ma -8(r17), fr18
	fldds,ma -8(r17), fr17
	fldds,ma -8(r17), fr16
	fldds,ma -8(r17), fr15
	fldds,ma -8(r17), fr14
	fldds,ma -8(r17), fr13
	fldds,ma -8(r17), fr12
	fldds,ma -8(r17), fr11
	fldds,ma -8(r17), fr10
	fldds,ma -8(r17), fr9
	fldds,ma -8(r17), fr8
	fldds,ma -8(r17), fr7
	fldds,ma -8(r17), fr6
	fldds,ma -8(r17), fr5
	fldds,ma -8(r17), fr4
	fldds,ma -8(r17), fr3
	fldds,ma -8(r17), fr2
	fldds,ma -8(r17), fr1
	fldds     0(r17), fr0	/* fr0 must be restored last */

	ldil	L%fpu_curpcb, r1
	stw	r17, R%fpu_curpcb(r1)

$fpusw_done
	rfir
	nop
EXIT(TLABEL(emu))

	/* Compute the hpt entry ptr */
#define	HPTENT \
	extru	r9, 23, 24, r16		/* r16 = (offset >> 8) */	! \
	zdep	r8, 22, 16, r24		/* r24 = (space << 9) */	! \
	mfctl	hptmask, r17		/* r17 = sizeof(HPT)-1 */	! \
	xor	r16, r24, r24		/* r24 ^= r16 */		! \
	and	r17, r24, r24		/* r24 &= r17 */		! \
	mfctl	vtop, r16		/* r16 = address of HPT table */! \
	or	r16, r24, r24		/* r24 = HPT entry */

	/* Construct the virtual address tag. */
#define	VTAG ! \
	shd	r0, r9, 1, r16		/* r16[1..15] = off[0..14] */	! \
	dep	r8, 31, 16, r16		/* put in the space id */	! \
	depi	1, 0, 1, r16		/* and set the valid bit */

#if 1
	.export	dtlb_c, data
	BSS(dtlb_c, 8)
	.export	dtlb_c, data
	BSS(tlbd_c, 8)
	.export	dtlb_c, data
	BSS(itlb_c, 8)

	.text
	/* XXX this touches tr5, which it should not, perhaps */

#define	TLB_STATS_PRE(t) \
	mfctl	itmr, r17	! \
	mtctl	r17, tr5
#define	TLB_STATS_AFT(t) \
	mfctl	itmr, r16			! \
	mfctl	tr5, r17			! \
	ldil	L%__CONCAT(t,_c), r25		! \
	ldo	R%__CONCAT(t,_c)(r25), r25	! \
	sub	r16, r17, r16			! \
	ldw	0(r25), r24			! \
	ldw	4(r25), r17			! \
	ldo	1(r24), r24			! \
	ldo	-2(r16), r16 /* for mtctl */	! \
	add	r16, r17, r17			! \
	stw	r24, 0(r25)			! \
	stw	r17, 4(r25)

#else
#define	TLB_STATS_PRE(t)	/**/
#define	TLB_STATS_AFT(t)	/**/
#endif

#if defined(HP7000_CPU) || defined(HP7100_CPU) || defined(HP7200_CPU)
#define	TLB_PULL(bits)							! \
	/* space:pgaddr -- r8:r9 */					! \
	mfctl	vtop, r16						! \
	ldwax,s	r8(r16), r17		/* space -> page directory */	! \
	extru	r9, 9, 10, r25						! \
	combt,=,n r0, r17, TLABEL(all)					! \
	ldwax,s	r25(r17), r24		/* page -> page table */	! \
	extru	r9, 19, 10, r16						! \
	combt,=,n r0, r24, TLABEL(all)					! \
	ldwax,s	r16(r24), r17		/* va -> pa:prot */		! \
	sh2addl	r16, r24, r25						! \
	combt,=,n r0, r17, TLABEL(all)					! \
	depi	(bits), 21+bits, 1+bits, r17				! \
	mfctl	tr7, r1							! \
	stwas	r17, 0(r25)		/* store back w/ the bits */	! \
	shd	r17, r0, 13, r25					! \
	dep	r8, 30, 15, r25		/* mix0r the pid from the sid */! \
	dep	r0, 31, 12, r17		/* needed ? */			! \
	addi	2, r25, r25						! \
	extru	r17, 24, 25, r17

	.align	32
$tlbd_x
$tlbd_s
$tlbd_t
	TLB_STATS_PRE(tlbd)
	TLB_PULL(1)
	mfsp	sr1, r16
	mtsp	r8, sr1
	idtlba	r17,(sr1, r9)
	idtlbp	r25,(sr1, r9)
	mtsp	r16, sr1
	TLB_STATS_AFT(tlbd)
	rfir
	nop

$itlb_x
$itlbna_x
$itlb_s
$itlbna_s
$itlb_t
$itlbna_t
	TLB_STATS_PRE(itlb)
	TLB_PULL(0)
	extru,=	r25, 5, 1, r0	/* gate needs a kernel pid */
	depi	0, 30, 15, r25
	mfsp	sr1, r16
	mtsp	r8, sr1
	iitlba	r17,(sr1, r9)
	iitlbp	r25,(sr1, r9)
	mtsp	r16, sr1
	TLB_STATS_AFT(itlb)
	rfir
	nop

$dtlb_x
$dtlbna_x
$dtlb_s
$dtlbna_s
	/*
	 * from the 7100lc ers, pg.6:
	 * pa7100 provides cr28 for dtlb traps only
	 */
$dtlb_t
$dtlbna_t
	TLB_STATS_PRE(dtlb)
	TLB_PULL(0)
	mfsp	sr1, r16
	mtsp	r8, sr1
	idtlba	r17,(sr1, r9)
	idtlbp	r25,(sr1, r9)
	mtsp	r16, sr1
	TLB_STATS_AFT(dtlb)
	rfir
	nop
#endif /*  defined(HP7000_CPU) || defined(HP7100_CPU) || defined(HP7200_CPU) */

#ifdef HP7100LC_CPU

#define	IITLBAF(r)	.word	0x04000440 | ((r) << 16)
#define	IITLBPF(r)	.word	0x04000400 | ((r) << 16)
#define	IDTLBAF(r)	.word	0x04001440 | ((r) << 16)
#define	IDTLBPF(r)	.word	0x04001400 | ((r) << 16)

/*
 * possible optimizations:
 *	change pte to reduce number of shifts
 *	reorder to reduce stalls
 */
#define	TLB_PULL_L(bits)						! \
	/* space:pgaddr -- r8:r9 */					! \
	mfctl	vtop, r16						! \
	ldwx,s	r8(r16), r17		/* space -> page directory */	! \
	extru	r9, 9, 10, r25						! \
	combt,=,n r0, r17, TLABEL(all)					! \
	ldwx,s	r25(r17), r24		/* page -> page table */	! \
	extru	r9, 19, 10, r16						! \
	combt,=,n r0, r24, TLABEL(all)					! \
	ldwx,s	r16(r24), r17		/* va -> pa:prot */		! \
	sh2addl	r16, r24, r25						! \
	combt,=,n r0, r17, TLABEL(all)					! \
	copy	r17, r16						! \
	depi	(bits), 21+bits, 1+bits, r17				! \
	mfctl	tr7, r1							! \
	sub,=	r16, r17, r0		/* do not store if unchanged */	! \
	stws	r17, 0(r25)		/* store back w/ the bits */	! \
	shd	r17, r0, 13, r25					! \
	dep	r8, 30, 15, r25		/* mix0r the pid from the sid */! \
	dep	r0, 31, 12, r17		/* needed ? */			! \
	addi	2, r25, r25						! \
	extru	r17, 24, 25, r17					! \
	sync

	.align	32
$tlbd_l
	TLB_STATS_PRE(tlbd)
	TLB_PULL_L(1)
	IDTLBAF(17)
	IDTLBPF(25)
	TLB_STATS_AFT(tlbd)
	rfir
	nop

	/*
	 * from 7100lc ers, pg.6:
	 * we found a post-silicon bug that makes cr28
	 * unreliable for the itlb miss handler
	 */
$itlb_l
$itlbna_l
	TLB_STATS_PRE(itlb)
	TLB_PULL_L(0)
	extru,=	r25, 5, 1, r0	/* gate needs a kernel pid */
	depi	0, 30, 15, r25
	IITLBAF(17)
	IITLBPF(25)
	TLB_STATS_AFT(itlb)
	rfir
	nop
$dtlbna_l
$dtlb_l
	TLB_STATS_PRE(dtlb)
	TLB_PULL_L(0)
	IDTLBAF(17)
	IDTLBPF(25)
	TLB_STATS_AFT(dtlb)
	rfir
	nop
#endif /* HP7100LC_CPU */

	.export	$tlb_missend, entry
$tlb_missend

	.align	64
	.export	TLABEL(all), entry
ENTRY(TLABEL(all),0)
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
	mtctl	t3, tr2

	ldil	L%$trap_tmp_save, t3
	stw	t1, TF_R22(t3)		/* use ,bc */
	stw	t2, TF_R21(t3)

	mfctl	tr2, t1
	stw	sp, TF_R30(t3)	/* sp */
	stw	t1, TF_R20(t3)	/* t3 */

	/*
	 * Now, save away other volatile state that prevents us from turning
	 * the PC queue back on, namely, the pc queue and ipsw, and the
	 * interrupt information.
	 */

	mfctl	eiem, t1
	mfctl	ipsw, t2
	stw	t1, TF_CR15(t3)		/* use ,bc */
	stw	t2, TF_CR22(t3)

	mfsp	sr3, t1
	mfctl	pidr1, t2
	stw	t1, TF_SR3(t3)
	stw	t2, TF_CR8(t3)

	/*
	 * Setup kernel context
	 */

	ldi	HPPA_PID_KERNEL,t1
	mtctl	t1, pidr1
	mtsp	r0, sr3

	/* this will enable interrupts after `cold' */
	ldil	L%kpsw, t1
	ldw	R%kpsw(t1), t2
	mtctl	r0, eiem
	mtctl	t2, ipsw

	mfctl	pcsq, t1
	mtctl	r0, pcsq
	mfctl	pcsq, t2
	stw	t1, TF_IISQH(t3)	/* use ,bc */
	stw	t2, TF_IISQT(t3)
	mtctl	r0, pcsq

	/*
	 * Set up the kernel stack pointer.  If the trap happened
	 * while we were in unprivileged code, or in privileged
	 * code in the SYSCALLGATE page, move to the kernel stack
	 * in curproc's PCB; otherwise, start a new stack frame
	 * on whatever kernel stack we're already on.
	 *
	 * This used to check only for a trap while we were in
	 * unprivileged code, but this ignored the possibility
	 * that a trap could come in during the period between
	 * a gateway instruction to raise privilege and the
	 * disabling of interrupts.  During this period we're
	 * still on the user's stack, and we must move to the
	 * kernel stack.
	 *
	 * - fredette@
	 */
	mfctl	pcoq, t1
	ldil	L%SYSCALLGATE, t2
	ldo	TF_PHYS-1(sp), sp
	dep	t1, 31, PGSHIFT, t2
	dep,<>	t1, 31, 2, r0
	comb,<>	t1, t2, $trap_from_kernel
	dep	r0, 31, 6, sp

	mfctl	cr30, t2
	depi	1, T_USER_POS, 1, r1
	depi	1, TFF_LAST_POS, 1, r1
	ldw	U_PCB+PCB_UVA(t2), sp
	ldo	NBPG(sp), sp

$trap_from_kernel
	ldil	L%$trapnowvirt, t2
	ldo	R%$trapnowvirt(t2), t2
	mtctl	t2, pcoq
	stw	t1, TF_IIOQH(t3)
	ldo	4(t2), t2
	mfctl	pcoq, t1
	stw	t1, TF_IIOQT(t3)
	mtctl	t2, pcoq

	mfctl	isr, t1
	mfctl	ior, t2
	stw	t1, TF_CR20(t3)		/* use ,bc */
	stw	t2, TF_CR21(t3)

	mfctl	iir, t2
	stw	t2, TF_CR19(t3)
	stw	r1, TF_FLAGS(t3)
	mfctl	tr7, r1

	copy	sp, t3
	ldo	HPPA_FRAME_SIZE+TRAPFRAME_SIZEOF(sp), sp
	rfir
	nop ! nop ! nop ! nop ! nop ! nop ! nop ! nop
$trapnowvirt
	/*
	 * t3 contains the virtual address of the trapframe
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

	mfctl	sar, t1			/* use ,bc each cache line */
	stw	t1, TF_CR11(t3)
	stw	r1, TF_R1(t3)
	stw	r2, TF_R2(t3)
	stw	r3, TF_R3(t3)

	copy	sp, r3
	stw,mb	r0, -HPPA_FRAME_SIZE(r3)

	/*
	 * Copy partially saved state from the store into the frame
	 */
	ldil	L%$trap_tmp_save, t2
	/* use ,bc each cache line */
	ldw  0(t2), r1 ! ldw  4(t2), t1 ! stw r1,  0(t3) ! stw t1,  4(t3)
	ldw  8(t2), r1 ! ldw 12(t2), t1 ! stw r1,  8(t3) ! stw t1, 12(t3)
	ldw 16(t2), r1 ! ldw 20(t2), t1 ! stw r1, 16(t3) ! stw t1, 20(t3)
	ldw 24(t2), r1 ! ldw 28(t2), t1 ! stw r1, 24(t3) ! stw t1, 28(t3)
	ldw 32(t2), r1 ! ldw 36(t2), t1 ! stw r1, 32(t3) ! stw t1, 36(t3)
	ldw 40(t2), r1 ! ldw 44(t2), t1 ! stw r1, 40(t3) ! stw t1, 44(t3)
	ldw 48(t2), r1 ! ldw 52(t2), t1 ! stw r1, 48(t3) ! stw t1, 52(t3)
	ldw 56(t2), r1 ! ldw 60(t2), t1 ! stw r1, 56(t3) ! stw t1, 60(t3)

	/*
	 * hmm, we don't need to save all the regs, only caller-save
	 * (except when DDB)
	 */
#ifdef DDB
	stw	r4, TF_R4(t3)
	stw	r5, TF_R5(t3)
	stw	r6, TF_R6(t3)
	stw	r7, TF_R7(t3)
	stw	r8, TF_R8(t3)
	stw	r9, TF_R9(t3)
	stw	r10, TF_R10(t3)
	stw	r11, TF_R11(t3)
	stw	r12, TF_R12(t3)
	stw	r13, TF_R13(t3)
	stw	r14, TF_R14(t3)
	stw	r15, TF_R15(t3)
	stw	r16, TF_R16(t3)
	stw	r17, TF_R17(t3)
	stw	r18, TF_R18(t3)
#endif
	stw	r19, TF_R19(t3)	/* t4 */
	stw	r23, TF_R23(t3)
	stw	r24, TF_R24(t3)
	stw	r25, TF_R25(t3)
	stw	r26, TF_R26(t3)
	stw	r27, TF_R27(t3)
	stw	r28, TF_R28(t3)
	stw	r29, TF_R29(t3)
	stw	r31, TF_R31(t3)

	/*
	 * Save the necessary control registers that have not already saved.
	 */

	mfctl	rctr, t1
	mfctl	ccr, t2
	stw	t1, TF_CR0(t3)
	stw	t2, TF_CR10(t3)

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
	dep	r0, 24, 25, arg0
	copy	t3, arg1

	.import	trap, code
	ldil	L%trap,t1
	ldo	R%trap(t1),t1
	.call
	blr	r0,rp
	bv,n	r0(t1)
	nop

	/* see if curproc have changed */
	ldw	-HPPA_FRAME_SIZE-TRAPFRAME_SIZEOF+TF_FLAGS(sp), arg0
	bb,>=,n	arg0, TFF_LAST_POS, $syscall_return
	ldo	-HPPA_FRAME_SIZE-TRAPFRAME_SIZEOF(sp), t3

	/* see if curproc have really changed */
	ldil	L%curproc, t1
	ldw	R%curproc(t1), t2
	comb,=,n r0, t2, $syscall_return
	ldo	-HPPA_FRAME_SIZE-TRAPFRAME_SIZEOF(sp), t3

	/* means curproc have actually changed */
	b	$syscall_return
	ldw	P_MD_REGS(t2), t3

	.export	$trap$all$end, entry
$trap$all$end
EXIT(TLABEL(all))

#if defined(HP7000_CPU) || defined(HP7100_CPU)
/*
 * void desidhash_s(void)
 */
LEAF_ENTRY(desidhash_s)
ALTENTRY(desidhash_x)
	sync
	MFCPU_T(DR_CPUCFG,22)	/* t1 */
	MFCPU_T(DR_CPUCFG,22)
	nop
	nop
	depi	0, DR0_PCXS_DHE, 3, t1	/* 3 4 DR0_PCXS_DOMAIN|DR0_PCXS_IHE */
	depi	1, DR0_PCXS_EQWSTO, 1, t1
	depi	0, DR0_PCXS_DHPMC, 1, t1
	depi	0, DR0_PCXS_ILPMC, 1, t1
	sync
	MTCPU_T(22,DR_CPUCFG)
	MTCPU_T(22,DR_CPUCFG)
	nop
	nop
	bv	0(rp)
	extru	t1, 4, 5, ret0	/* return chip revision */
EXIT(desidhash_s)
#endif /* HP7000_CPU || HP7100_CPU */

#ifdef HP7200_CPU
/*
 * void desidhash_t(void)
 */
LEAF_ENTRY(desidhash_t)
	sync
	MFCPU_T(DR_CPUCFG,22)	/* t1 */
	MFCPU_T(DR_CPUCFG,22)
	nop
	nop
	depi	0, DR0_PCXT_IHE, 1, t1
	depi	0, DR0_PCXT_DHE, 1, t1
	depi	0, DR0_PCXT_DHPMC, 1, t1
	depi	0, DR0_PCXT_ILPMC, 1, t1
	sync
	MTCPU_T(22,DR_CPUCFG)
	MTCPU_T(22,DR_CPUCFG)
	nop
	nop
	bv	0(rp)
	extru	t1, 4, 5, ret0	/* return chip revision */
EXIT(desidhash_t)
#endif /* HP7200_CPU */

#ifdef HP7100LC_CPU

/*
 * int
 * ibtlb_l(int i, pa_space_t sp, vaddr_t va, paddr_t pa, vsize_t sz, u_int prot)
 */
LEAF_ENTRY(ibtlb_l)
	rsm	(PSL_R|PSL_I), t4
	nop ! nop ! nop ! nop ! nop ! nop ! nop

	bv	0(rp)
	mtsm	t4
EXIT(ibtlb_l)

/* hpti_l(addr,size) */
LEAF_ENTRY(hpti_l)
	ldo	-1(arg1), arg1
	depi	0, 31, 12, arg1
	ldi	0x1c0, t1		/* cache size assumed 128k XXX */
	or	arg0, t1, arg0
	sync
	MTCPU_C(26,DR0_PCXL2_HTLB_ADDR)
	MTCPU_C(25,DR0_PCXL2_HTLB_CFG)
	nop
	nop
	bv,n	r0(rp)
	nop
EXIT(hpti_l)

/*
 * int
 * pbtlb_l(int i)
 */
LEAF_ENTRY(pbtlb_l)
	; DR_PAGE0
	rsm	(PSL_R|PSL_I), t4
	nop ! nop ! nop ! nop
	ldil	L%0xc041, t1
	ldo	R%0xc041(t1), t1
	dep	arg0, 30, 3, t1
	sync
	MTCPU_T(22,DR_DTLB)	/* t1 */
	nop
	nop
	mtsp	r0, sr1
	idtlba	r0,(sr1,r0)
	idtlbp	r0,(sr1,r0)
	zdepi	-1, 18, 1, t1
	nop
	sync
	MTCPU_T(22,DR_DTLB)
	nop
	nop
	bv	0(rp)
	mtsm	t4
EXIT(pbtlb_l)

/*
 * int desidhash_l(void)
 */
LEAF_ENTRY(desidhash_l)
	MFCPU_C(DR_CPUCFG,22)	/* t1 */
	nop
	nop
	depi	0, DR0_PCXL_L2IHASH_EN, 2, t1	/* + DR0_PCXL_L2DHASH_EN */
	depi	0, DR0_PCXL_L2IHPMC, 1, t1	/* don't reset */
	depi	0, DR0_PCXL_L2DHPMC, 1, t1	/* don't reset */
	depi	0, DR0_PCXL_L1IHPMC, 1, t1	/* don't reset */
	depi	0, DR0_PCXL_L2PARERR,1, t1	/* don't reset */
	sync
	MTCPU_C(22,DR_CPUCFG)
	nop
	nop
	bv	0(rp)
	extru	t1, 4, 5, ret0	/* return chip revision */
EXIT(desidhash_l)

#endif /* HP7100LC_CPU */

/*
 * High Priority Machine Check Interrupt
 */
	.export	TLABEL(hpmc), entry
ENTRY(TLABEL(hpmc),0)

	mtsp	r0, sr0
	ldil	L%hppa_vtop, t1
	ldw	R%hppa_vtop(t1), t1
	mtctl	t1, CR_VTOP

	.import	hpmc_dump, code
	ldil	L%hpmc_dump, rp
	ldo	R%hpmc_dump(rp), rp
	ldil	L%kpsw, %arg2
	ldw	R%kpsw(%arg2), %arg2
	depi	0, PSL_I_POS, 1, %arg2
	stw	%arg2, R%kpsw(t1)
	ldil	L%emrg_stack, arg1
	b	$kernel_setup
	ldw	R%emrg_stack(arg1), arg1

	/* never returns, but still */
	ldil	L%GBCAST_ADDR, t1
	ldi	CMD_RESET, t2
	stw	t2, R%GBCAST_ADDR(t1)
hpmc_never_dies
	b	hpmc_never_dies
	nop
EXIT(TLABEL(hpmc))

/*
 * transfer of control handler
 */
ENTRY(hppa_toc,0)

	mtsp	r0, sr0
	ldil	L%hppa_vtop, t1
	ldw	R%hppa_vtop(t1), t1
	mtctl	t1, CR_VTOP

	/* TODO reload btlb */

	.import	boot, code
	ldil	L%boot, rp
	ldo	R%boot(rp), rp
	ldil	L%kpsw, %arg2
	ldw	R%kpsw(%arg2), %arg2
	depi	0, PSL_I_POS, 1, %arg2
	stw	%arg2, R%kpsw(t1)
	ldi	0, arg0
	ldil	L%emrg_stack, arg1
	b	$kernel_setup
	ldw	R%emrg_stack(arg1), arg1

ALTENTRY(hppa_toc_end)
	.word	0
EXIT(hppa_toc)

/*
 * power fail recovery handler
 */
ENTRY(hppa_pfr,0)

	mtsp	r0, sr0
	ldil	L%hppa_vtop, t1
	ldw	R%hppa_vtop(t1), t1
	mtctl	t1, CR_VTOP

	/* TODO reload btlb */

	.import	boot, code
	ldil	L%boot, rp
	ldo	R%boot(rp), rp
	ldil	L%kpsw, %arg2
	ldw	R%kpsw(%arg2), %arg2
	depi	0, PSL_I_POS, 1, %arg2
	stw	%arg2, R%kpsw(t1)
	ldi	RB_HALT|RB_POWERDOWN, arg0
	ldil	L%emrg_stack, arg1
	b	$kernel_setup
	ldw	R%emrg_stack(arg1), arg1

ALTENTRY(hppa_pfr_end)
	.word	0
EXIT(hppa_pfr)

	.align	32
	.export	TLABEL(ibrk), entry
ENTRY(TLABEL(ibrk),0)
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
	rfir
	nop
EXIT(TLABEL(ibrk))

LEAF_ENTRY(fpu_save)
	fstds,ma %fr0 , 8(arg0)
	fstds,ma %fr1 , 8(arg0)
	fstds,ma %fr2 , 8(arg0)
	fstds,ma %fr3 , 8(arg0)
	fstds,ma %fr4 , 8(arg0)
	fstds,ma %fr5 , 8(arg0)
	fstds,ma %fr6 , 8(arg0)
	fstds,ma %fr7 , 8(arg0)
	fstds,ma %fr8 , 8(arg0)
	fstds,ma %fr9 , 8(arg0)
	fstds,ma %fr10, 8(arg0)
	fstds,ma %fr11, 8(arg0)
	fstds,ma %fr12, 8(arg0)
	fstds,ma %fr13, 8(arg0)
	fstds,ma %fr14, 8(arg0)
	fstds,ma %fr15, 8(arg0)
	fstds,ma %fr16, 8(arg0)
	fstds,ma %fr17, 8(arg0)
	fstds,ma %fr18, 8(arg0)
	fstds,ma %fr19, 8(arg0)
	fstds,ma %fr20, 8(arg0)
	fstds,ma %fr21, 8(arg0)
	fstds,ma %fr22, 8(arg0)
	fstds,ma %fr23, 8(arg0)
	fstds,ma %fr24, 8(arg0)
	fstds,ma %fr25, 8(arg0)
	fstds,ma %fr26, 8(arg0)
	fstds,ma %fr27, 8(arg0)
	fstds,ma %fr28, 8(arg0)
	fstds,ma %fr29, 8(arg0)
	fstds,ma %fr30, 8(arg0)
	fstds    %fr31, 0(arg0)
	fdc	r0(arg0)
	ldo	-60(arg0), arg0
	fdc	r0(arg0)
	ldo	-32(arg0), arg0
	fdc	r0(arg0)
	ldo	-32(arg0), arg0
	fdc	r0(arg0)
	ldo	-32(arg0), arg0
	fdc	r0(arg0)
	ldo	-32(arg0), arg0
	fdc	r0(arg0)
	ldo	-32(arg0), arg0
	fdc	r0(arg0)
	ldo	-32(arg0), arg0
	fdc	r0(arg0)
	bv	r0(rp)
	sync
EXIT(fpu_save)

#ifdef FPEMUL
	/*
	 * Emulate FPU
	 *
	 * iisq:iioq - exception triggered instruction
	 */
ENTRY($fpu_emulate,320)
	mtctl	sp, tr3
	mtctl	r31, tr2

	ldil	L%fpemu_stack, r31
	ldw	R%fpemu_stack(r31), r31

	stw	r2 , TF_R2 (r31)
	stw	r3 , TF_R3 (r31)
#ifdef DDB
	stw	r4 , TF_R4 (r31)
	stw	r5 , TF_R5 (r31)
	stw	r6 , TF_R6 (r31)
	stw	r7 , TF_R7 (r31)
	stw	r8 , TF_R8 (r31)
	stw	r9 , TF_R9 (r31)
	stw	r10, TF_R10(r31)
	stw	r11, TF_R11(r31)
	stw	r12, TF_R12(r31)
	stw	r13, TF_R13(r31)
	stw	r14, TF_R14(r31)
	stw	r15, TF_R15(r31)
	stw	r16, TF_R16(r31)
	stw	r17, TF_R17(r31)
	stw	r18, TF_R18(r31)
#endif
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
	mfctl	tr2, t2
	mfctl	tr3, t1
	stw	t1, TF_R30(r31)
	stw	t2, TF_R31(r31)
	copy	r1, arg0
	mfctl	sar, r1
	stw	r1, TF_CR11(r31)
	stw	arg0, TF_CR19(r31)

	ldo	TRAPFRAME_SIZEOF(r31), r3
	ldo	TRAPFRAME_SIZEOF+HPPA_FRAME_SIZE(r31), sp

	ldil	L%$global$, dp
	ldo	R%$global$(dp), dp

	.import	fpu_emulate, code
	ldil	L%fpu_emulate,t1
	ldo	R%fpu_emulate(t1),t1
	mfctl	cr30, arg2
	.call
	blr	r0,rp
	bv,n	0(t1)
	nop

	mfctl	cr30, arg0
	fdc	r0(arg0)
	ldo	32(arg0), arg0
	fdc	r0(arg0)
	ldo	32(arg0), arg0
	fdc	r0(arg0)
	ldo	32(arg0), arg0
	fdc	r0(arg0)
	ldo	32(arg0), arg0
	fdc	r0(arg0)
	ldo	32(arg0), arg0
	fdc	r0(arg0)
	ldo	32(arg0), arg0
	fdc	r0(arg0)
	ldo	32(arg0), arg0
	fdc	r0(arg0)

	ldil	L%fpemu_stack, r31
	ldw	R%fpemu_stack(r31), r31

	ldw	TF_CR11(r31), r1
	mtsar	r1
	ldw	TF_R2 (r31), r2
	ldw	TF_R3 (r31), r3
	copy	ret0, r1
#ifdef DDB
	ldw	TF_R4 (r31), r4
	ldw	TF_R5 (r31), r5
	ldw	TF_R6 (r31), r6
	ldw	TF_R7 (r31), r7
	ldw	TF_R8 (r31), r8
	ldw	TF_R9 (r31), r9
	ldw	TF_R10(r31), r10
	ldw	TF_R11(r31), r11
	ldw	TF_R12(r31), r12
	ldw	TF_R13(r31), r13
	ldw	TF_R14(r31), r14
	ldw	TF_R15(r31), r15
	ldw	TF_R16(r31), r16
	ldw	TF_R17(r31), r17
	ldw	TF_R18(r31), r18
#endif
	ldw	TF_R19(r31), r19
	ldw	TF_R20(r31), r20
	ldw	TF_R21(r31), r21
	ldw	TF_R22(r31), r22
	ldw	TF_R23(r31), r23
	ldw	TF_R24(r31), r24
	ldw	TF_R25(r31), r25
	ldw	TF_R26(r31), r26
	ldw	TF_R27(r31), r27
	ldw	TF_R28(r31), r28
	ldw	TF_R29(r31), r29
	ldw	TF_R30(r31), r30
	ldw	TF_R31(r31), r31

	comb,<>	r0, r1, TLABEL(all)
	ldi	T_EXCEPTION, r1

	rfir
	nop
EXIT($fpu_emulate)

#endif /* FPEMUL */

	.import	dcache_stride, data
LEAF_ENTRY(fdcache)
	ldil	L%dcache_stride,t1
	ldw	R%dcache_stride(t1), arg3

	mtsp	arg0, sr1		/* move the space register to sr1 */
	add	arg1, arg2, arg0	/* get the last byte to flush in arg0 */

	zdep	arg3, 27, 28, t1	/* get size of a 16X loop in t1 */
	comb,<	arg2, t1, fdc_short	/* check for count < 16 * stride */
	addi	-1, t1, t1		/* compute size of large loop - 1 */

	andcm	arg2, t1, t1		/* L = count - (count mod lenbigloop) */
	add	arg1, t1, t1		/* ub for big loop is lb + L */

	fdc,m	arg3(sr1, arg1)		/* Start flushing first cache line. */
fdc_long
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	fdc,m	arg3(sr1, arg1)
	comb,<<,n arg1, t1, fdc_long
	fdc,m	arg3(sr1, arg1)
fdc_short				/* flush one line at a time */
	comb,<<,n arg1, arg0, fdc_short
	fdc,m	arg3(sr1, arg1)

	addi	-1, arg0, arg1
	fdc	r0(sr1, arg1)

	sync
	syncdma
	bv	r0(r2)
	nop
EXIT(fdcache)

	.import	dcache_stride, data
LEAF_ENTRY(pdcache)
	ldil	L%dcache_stride,t1
	ldw	R%dcache_stride(t1), arg3

	mtsp	arg0, sr1		/* move the space register to sr1 */
	add	arg1, arg2, arg0	/* get the last byte to flush in arg0 */

	zdep	arg3, 27, 28, t1	/* get size of a 16X loop in t1 */
	comb,<	arg2, t1, pdc_short	/* check for count < 16 * stride */
	addi	-1, t1, t1		/* compute size of large loop - 1 */

	andcm	arg2, t1, t1		/* L = count - (count mod lenbigloop) */
	add	arg1, t1, t1		/* ub for big loop is lb + L */

	pdc,m	arg3(sr1, arg1)		/* Start flushing first cache line. */
pdc_long
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	pdc,m	arg3(sr1, arg1)
	comb,<<,n arg1, t1, pdc_long
	pdc,m	arg3(sr1, arg1)
pdc_short				/* flush one line at a time */
	comb,<<,n arg1, arg0, pdc_short
	pdc,m	arg3(sr1, arg1)

	addi	-1, arg0, arg1
	pdc	r0(sr1, arg1)

	sync
	syncdma
	bv	r0(r2)
	nop
EXIT(pdcache)

	.import	icache_stride, data
LEAF_ENTRY(ficache)
	ldil	L%icache_stride,t1
	ldw	R%icache_stride(t1), arg3

	mtsp	arg0, sr1		/* move the space register to sr1 */
	add	arg1, arg2, arg0	/* get the last byte to flush in arg0 */

	zdep	arg3, 27, 28, t1	/* get size of a 16X loop in t1 */
	comb,<	arg2, t1, fic_short	/* check for count < 16 * stride */
	addi	-1, t1, t1		/* compute size of large loop - 1 */

	andcm	arg2, t1, t1		/* L = count - (count mod lenbigloop) */
	add	arg1, t1, t1		/* ub for big loop is lb + L */

	fic,m	arg3(sr1, arg1)		/* Start flushing first cache line. */
fic_long
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	fic,m	arg3(sr1, arg1)
	comb,<<,n arg1, t1, fic_long
	fic,m	arg3(sr1, arg1)
fic_short				/* flush one line at a time */
	comb,<<,n arg1, arg0, fic_short
	fic,m	arg3(sr1, arg1)

	addi	-1, arg0, arg1
	fic	r0(sr1, arg1)

	sync
	syncdma
	bv	r0(r2)
	nop
EXIT(ficache)


LEAF_ENTRY(setjmp)
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

LEAF_ENTRY(longjmp)
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
LEAF_ENTRY(name)				! \
	ldil	L%VM_MAXUSER_ADDRESS, t1	! \
	comb,>>= arg0, t1, fusubadaddr		! \
	ldil	L%curproc, t1			! \
	ldw	R%curproc(t1), t1		! \
	ldw	P_ADDR(t1), t1			! \
	ldil	L%fusufault, t2			! \
	ldo	R%fusufault(t2), t2		! \
	ldw	U_PCB+PCB_ONFAULT(t1), r1	! \
	stw	t2, U_PCB+PCB_ONFAULT(t1)	! \
	ldw	U_PCB+PCB_SPACE(t1), t2		! \
	mtsp	t2, sr1

#define	FUX(name,insn)				  \
	FUSUX(name)				! \
	insn	0(sr1, arg0), ret0		! \
	mtsp	r0, sr1				! \
	bv	r0(rp)				! \
	stw	r1, U_PCB+PCB_ONFAULT(t1)	! \
EXIT(name)

#define	SUX(name,insn)				  \
	FUSUX(name)				! \
	insn	arg1, 0(sr1, arg0)		! \
	mtsp	r0, sr1				! \
	bv	r0(rp)				! \
	stw	r1, U_PCB+PCB_ONFAULT(t1)	! \
EXIT(name)

LEAF_ENTRY(fusufault)
	mtsp	r0, sr1
	stw	r1, U_PCB+PCB_ONFAULT(t1)
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

LEAF_ENTRY(copy_on_fault)
	mtsp	r0, sr1
	mtsp	r0, sr2
	stw	r1, PCB_ONFAULT+U_PCB(r2)
	ldw	HPPA_FRAME_CRP(sp), rp
	ldo	-64(sp), sp
	bv	0(rp)
	ldi	EFAULT, %ret0
EXIT(copy_on_fault)

/*
 * int spstrcpy (pa_space_t ssp, const void *src, pa_space_t dsp, void *dst,
 *		 size_t size, size_t *rsize)
 * do a space to space strncpy, return actual copy size in the rsize;
 */
LEAF_ENTRY(spstrcpy)
	ldw	HPPA_FRAME_ARG(4)(sp), t2
	ldo	64(sp), sp
	add	t2, arg1, t2
	stw	rp, HPPA_FRAME_CRP(sp)
	/* setup fault handler */
	ldil	L%curproc, t1
	ldw	R%curproc(t1), t3
	ldil	L%copy_on_fault, t4
	ldw	P_ADDR(t3), r2
	ldo	R%copy_on_fault(t4), t4
	ldw	PCB_ONFAULT+U_PCB(r2), r1
	stw	t4, PCB_ONFAULT+U_PCB(r2)

	mtsp	arg0, sr1
	mtsp	arg2, sr2
	copy	arg1, arg0

$spstrcpy_loop
	ldbs,ma	1(sr1, arg1), t1
	comb,=	t2, arg1, $spstrcpy_exit
	stbs,ma	t1, 1(sr2, arg3)
	comb,<>,n r0, t1, $spstrcpy_loop
	nop

$spstrcpy_exit
	mtsp	r0, sr1
	mtsp	r0, sr2
	stw	r1, PCB_ONFAULT+U_PCB(r2)
	ldw	HPPA_FRAME_CRP(sp), rp
	sub	arg1, arg0, arg1
	ldo	-64(sp), sp
	ldw	HPPA_FRAME_ARG(5)(sp), arg0
	sub,=	r0, arg0, r0
	stw	arg1, 0(arg0)
	bv	0(rp)
	copy	r0, ret0
EXIT(spstrcpy)

/*
 * adjust the time value
 * XXX: do it the easy way, later we will calculate actual fuzz from itmr
 */
LEAF_ENTRY(microtime)

	/* t4 = splhigh() */
	mfctl	eiem, t4
	mtctl	r0, eiem

	.import time, data
	ldil	L%time, t3
	ldo	R%time(t3), t3
	ldw	0(t3), t1
	ldw	4(t3), t2

	/* splx(t4) */
	mtctl	t4, eiem

	ldil	L%-1000000, t3
	ldo	R%-1000000(t3), t3

	addi	1, t2, t2
	addb,<,n t2, t3, microtime_no

	addi	1, t1, t1
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
ENTRY(setrunqueue,0)
#ifdef DIAGNOSTIC
	ldw	P_BACK(arg0), t1
	comb,<>,n r0, t1, Lsetrunqueue_panic
	ldw	P_WCHAN(arg0), t1
	comb,<>,n r0, t1, Lsetrunqueue_panic
	ldb	P_STAT(arg0), t1
	comib,=,n SRUN, t1, Lsetrunqueue_ok
Lsetrunqueue_panic
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
Lsetrunqueue_ok
#endif

	ldb	P_PRIORITY(arg0), t2
	ldil	L%qs, t4
	extru	t2, 29, 5, t1
	ldo	R%qs(t4), t4
	sh3add	t1, t4, t4
	ldil	L%whichqs, arg3
	ldw	P_BACK(t4), t2
	stw	t4, P_FORW(arg0)
	stw	arg0, P_BACK(t4)
	ldw	R%whichqs(arg3), t3
	stw	arg0, P_FORW(t2)
	mtctl	t1, sar
	stw	t2, P_BACK(arg0)
	vdepi	1, 1, t3
	bv	0(rp)
	stw	t3, R%whichqs(arg3)
EXIT(setrunqueue)

/*
 * remrunqueue(struct proc *p);
 * Remove a process from its queue.  Should be called at splclock().
 */
	.align	32
ENTRY(remrunqueue,0)
	ldb	P_PRIORITY(arg0), t2
	extru	t2, 29, 5, arg2
	ldil	L%whichqs, t2
	mtsar	arg2
	ldw	R%whichqs(t2), t3

#ifdef DIAGNOSTIC
	bvb,<,n	t3, remrunqueue_ok

Lremrunqueue_panic
	copy	arg0, arg1
	ldil	L%panic, r1
	ldil	L%Lrrqpstr, arg0
	ldo	R%panic(r1), r1
	ldo	R%Lrrqpstr(arg0), arg0
	.call
	blr	%r0, rp
	bv,n	%r0(r1)
	nop

Lrrqpstr
	.asciz	"remrunqueue(%p), bit=%x"
	.align	8
remrunqueue_ok
#endif
	ldw	P_BACK(arg0), t4
	stw	r0, P_BACK(arg0)
	ldw	P_FORW(arg0), arg0
	stw	arg0, P_FORW(t4)
	vdepi	0, 1, t3
	sub,<>	t4, arg0, r0
	stw	t3, R%whichqs(t2)
	bv	0(rp)
	stw	t4, P_BACK(arg0)
EXIT(remrunqueue)

/*
 * cpu_switch()
 * Find the highest priority process and resume it.
 */
	.align	32
ENTRY(cpu_switch,128)

	/*
	 * Clear curproc so that we don't accumulate system time while idle.
	 */
	ldil	L%curproc, t1
	ldw	R%curproc(t1), arg2
	stw	r0, R%curproc(t1)
	/* remain on the old (curproc)'s stack until we have a better choice */

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

	ldw	P_FORW(t2), arg1
#ifdef DIAGNOSTIC
	comb,<>	t2, arg1, link_ok
	nop
switch_error
	copy	t4, arg1
	copy	t2, arg2
	ldil	L%panic, r1
	ldil	L%Lcspstr, arg0
	ldo	R%panic(r1), r1
	ldo	R%Lcspstr(arg0), arg0
	.call
	blr	%r0, rp
	bv,n	%r0(r1)
	nop
Lcspstr
	.asciz	"cpu_switch: bit=%x, q/p=%p"
	.align	8
link_ok
#endif
	ldil	L%want_resched, t4
	stw	r0, R%want_resched(t4)

	ldw	P_FORW(arg1), arg0
	stw	arg0, P_FORW(t2)
	stw	t2, P_BACK(arg0)
	stw	r0, P_BACK(arg1)

	vdepi	0, 1, t3
	sub,<>	t2, arg0, r0
	stw	t3, R%whichqs(t1)

	/* don't need &whichqs (t1) starting here */
#ifdef DIAGNOSTIC
	ldw	P_WCHAN(arg1), t1
	comb,<>,n r0, t1, switch_error
	copy	arg1, t2
	ldb	P_STAT(arg1), t1
	comib,<>,n SRUN, t1, switch_error
	copy	arg1, t2
	/*
	 * Either we must be switching to the same process, or
	 * the the new process' kernel stack must be reasonable.
	 */
	comb,=,n arg1, arg2, kstack_ok
	ldw     P_MD_REGS(arg1), t1
	ldw     P_ADDR(arg1), arg0
	ldw     TF_R30(t1), t1
	ldo     NBPG(arg0), arg0
	comb,>>,n arg0, t1, switch_error
	copy    arg1, t2
	sub     t1, arg0, t1
	ldil    L%USPACE, arg0
	ldo     R%USPACE(arg0), arg0
	comb,<<=,n arg0, t1, switch_error
	copy    arg1, t2
kstack_ok
#endif
	/* Skip context switch if same process. */
	comb,=,n arg1, arg2, switch_return

	/* If old process exited, don't bother. */
	comb,=,n r0, arg2, switch_exited

	/*
	 * 2. save old proc context
	 *
	 * arg2: old proc
	 */
	ldw	P_MD_REGS(arg2), t1
	stw	rp, HPPA_FRAME_CRP(sp)
	ldw	TF_R30(t1), t3
	copy	sp, t2
	stw,ma	r3, HPPA_FRAME_SIZE+20*4(sp)
	stw	sp, TF_R30(t1)
	stw	t2, HPPA_FRAME_PSP(sp)
	stw	t3, HPPA_FRAME_ARG(0)(sp)	/* real user's stack */
	fdc	r0(t1)
	/* save callee-save registers */
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
	mtctl	r0, ccr			/* disable FPU */
	ldw	P_MD_REGS(arg1), t1
	ldw	TF_CR30(t1), t2
	ldw	TF_R30(t1), sp
	ldw	TF_CR9(t1), t3
	mtctl	t2, cr30
	mtctl	t3, pidr2
	ldw	HPPA_FRAME_PSP(sp), t2
	ldw	HPPA_FRAME_ARG(0)(sp), t3
	ldw	HPPA_FRAME_ARG(1)(sp), t4 /* in case we're on trampoline */
	ldw	HPPA_FRAME_ARG(2)(sp), arg0
	ldw	HPPA_FRAME_CRP(t2), rp
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
	stw	t3, TF_R30(t1)
	copy	t2, sp

switch_return
	mtctl	arg3, eiem
	ldil	L%curproc, t1
	bv	0(rp)
	stw	arg1, R%curproc(t1)
EXIT(cpu_switch)

ENTRY(switch_trampoline,0)
	.call
	blr	r0, rp
	bv,n	r0(t4)
	nop
	ldil	L%curproc, t1
	ldw	R%curproc(t1), t2
	.call
	b	$syscall_return
	ldw	P_MD_REGS(t2), t3
EXIT(switch_trampoline)

/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 */
ENTRY(sigcode,0)
	.call
	blr	r0, rp
	bv,n	r0(arg3)
	nop
	copy	r3, arg0
	ldil	L%SYSCALLGATE, r1
	.call
	ble	4(sr7, r1)
	ldi	SYS_sigreturn, t1
	copy	ret0, arg0
	ldil	L%SYSCALLGATE, r1
	.call
	ble	4(sr7, r1)
	ldi	SYS_exit, t1
ALTENTRY(esigcode)
EXIT(sigcode)

#ifdef COMPAT_LINUX
ENTRY(linux_sigcode,0)

	/* TODO linux signal trampoline */
	bv	0(rp)
	nop
ALTENTRY(linux_esigcode)
EXIT(linix_sigcode)
#endif /* COMPAT_LINUX */

	.end
