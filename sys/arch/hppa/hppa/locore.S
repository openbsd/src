/*	$OpenBSD: locore.S,v 1.19 1999/08/14 03:11:31 mickey Exp $	*/

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
 *
 * Portitions of this file are derived from other sources, see
 * the copyrights and acknowledgements.
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
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/iomod.h>
#include <machine/pdc.h>
#include <machine/intr.h>
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
	.import	fpu_curproc, data
	.import	want_resched, data
	.import	proc0, data
	.import	proc0paddr, data
	.import	intr_recurse, data
	.import	panic, code

	.space $PRIVATE$
	.subspa $BSS$
	.export	proc0stack, data
	.export	proc0stack_end, data
proc0stack
	.block	4*NBPG
proc0stack_end

	.export	intr_stack, data
	.export	intr_stack_end, data
	.export	intr_stack_red, data
intr_stack
	.block	5*NBPG
intr_stack_end
	.block	2*NBPG
intr_stack_red
	.block	1*NBPG


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

	/*
	 * disable interrupts and turn off all bits in the psw so that
	 * we start in a known state.
	 */
	rsm	RESET_PSW,r0

	/*
	 * to keep the spl() routines consistent we need to put the correct
	 * spl level into eiem
	 */
	ldi	INT_NONE,r1
	mtctl	r1,eiem

	/*
	 * set up the dp pointer so that we can do quick references off of it
	 */
	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/*
	 * kernel stack lives here (arg3 is esym)
	 * arg0 will be available space for hppa_init()
	 */
	ldil	L%proc0stack, t2
	ldo	R%proc0stack(t2), sp
	ldil	L%proc0paddr, t1
	ldo	NBPG(arg3), arg0
	stw	arg3, R%proc0paddr(t1)
	ldil	L%proc0, t2
	stw	arg3, R%proc0+p_addr(t2)

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
	stwm	r0, FM_SIZE(sp)
	copy	sp, r3
	stwm	t1, FM_SIZE(sp)

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
	ldi	HPPA_PID_KERNEL,r1
	mtctl	r1,pidr1
	mtctl	r1,pidr2
	mtctl	r1,pidr3
	mtctl	r1,pidr4
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
	bb,>=,n	r1,25,$noddb

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
	bv,n	(rp)
EXIT(__start)

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
	stw	r22,VA_ARG4(sp)
	stw	r21,VA_ARG5(sp)
	stw	r29,FM_SL(sp)
	gate	$bsd_syscall,r0
	copy	r1,r22
#endif /* COMPAT_OSF1 */

$bsd_syscall
	/*
	 * set up a space register and a protection id so that 
	 * we can access kernel memory
	 */
	mtsp	r0, sr1
	mfctl	pidr2, r28
	ldi	HPPA_PID_KERNEL, r1
	mtctl	r1, pidr2

	/*
	 * now call the syscall handler
	 */
	.import $syscall,code
	.call
	ldil    L%$syscall,r1
	be,n    R%$syscall(sr1,r1)
	nop

	.export	gateway_page_end, entry
gateway_page_end

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
	ldil	L%curproc, t1
	ldw	R%curproc(sr1, t1), t1
	ldw	p_addr(sr1, t1), t2

	/* save sp first */
	stw	sp, TF_R30+pcb_tf+u_pcb(sr1, t2)
	copy	sp, t4

	/* calculate kernel sp, load, create kernel stack frame */
	ldo	FM_SIZE+ARG_SIZE+NBPG(t2), sp
	
	stw	r1 , TF_R1 +pcb_tf+u_pcb(sr1, t2)
	stw	r2 , TF_R2 +pcb_tf+u_pcb(sr1, t2)
	stw	r3 , TF_R3 +pcb_tf+u_pcb(sr1, t2)
	stw	r4 , TF_R4 +pcb_tf+u_pcb(sr1, t2)
	stw	r5 , TF_R5 +pcb_tf+u_pcb(sr1, t2)
	stw	r6 , TF_R6 +pcb_tf+u_pcb(sr1, t2)
	stw	r7 , TF_R7 +pcb_tf+u_pcb(sr1, t2)
	stw	r8 , TF_R8 +pcb_tf+u_pcb(sr1, t2)
	stw	r9 , TF_R9 +pcb_tf+u_pcb(sr1, t2)
	stw	r10, TF_R10+pcb_tf+u_pcb(sr1, t2)
	stw	r11, TF_R11+pcb_tf+u_pcb(sr1, t2)
	stw	r12, TF_R12+pcb_tf+u_pcb(sr1, t2)
	stw	r13, TF_R13+pcb_tf+u_pcb(sr1, t2)
	stw	r14, TF_R14+pcb_tf+u_pcb(sr1, t2)
	stw	r15, TF_R15+pcb_tf+u_pcb(sr1, t2)
	stw	r16, TF_R16+pcb_tf+u_pcb(sr1, t2)
	stw	r17, TF_R17+pcb_tf+u_pcb(sr1, t2)
	stw	r18, TF_R18+pcb_tf+u_pcb(sr1, t2)
	stw	r27, TF_R27+pcb_tf+u_pcb(sr1, t2)	/* dp */
	stw	r28, TF_CR8+pcb_tf+u_pcb(sr1, t2)	/* saved pidr2 */

	/* copy arguments */
	copy	t3, r1
	stwm	arg0, 4(sr1, t3)
	stwm	arg1, 4(sr1, t3)
	stwm	arg2, 4(sr1, t3)
	stwm	arg3, 4(sr1, t3)
	ldw	VA_ARG4(sr0, t4), arg0
	ldw	VA_ARG5(sr0, t4), arg1
	ldw	VA_ARG6(sr0, t4), arg2
	ldw	VA_ARG7(sr0, t4), arg3
	stwm	arg0, 4(sr1, t3)
	stwm	arg1, 4(sr1, t3)
	stwm	arg2, 4(sr1, t3)
	stwm	arg3, 4(sr1, t3)
	ldw	VA_ARG8 (sr0, t4), arg0
	ldw	VA_ARG9 (sr0, t4), arg1
	ldw	VA_ARG10(sr0, t4), arg2
	ldw	VA_ARG11(sr0, t4), arg3
	stwm	arg0, 4(sr1, t3)
	stwm	arg1, 4(sr1, t3)
	stwm	arg2, 4(sr1, t3)
	stwm	arg3, 4(sr1, t3)

	/* setup kernel context */
	mtctl	r0, sr0
	mtctl	r0, sr1
	mtctl	r0, sr2
	mtctl	r0, sr3
	mtctl	r0, sr4
	mtctl	r0, sr5
	mtctl	r0, sr6
	mtctl	r0, sr7

	/* leave pidr4 in user space so copy* work */
	ldi	HPPA_PID_KERNEL, t4
	mtctl	t4, pidr1
	mtctl	t4, pidr3

	/* setup frame */
	stw	r0, FM_PSP(sr1, sp)
	stw	r0, FM_CRP(sr1, sp)

	ldo	pcb_tf+u_pcb(t2), arg0
	copy	r1, arg1

	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/* do a syscall */
	ldil	L%syscall, ret0
	ldo	R%syscall(ret0), ret0
	.call
	blr	r0, rp
	bv,n	0(ret0)
	.exit
	.procend
/* fall through */
	.export	$syscall_return, entry
	.proc
	.callinfo no_calls
	.entry
$syscall_return
	/* check for AST ? XXX */
	/*
	 * t1:	curproc
	 * t2:	user
	 */
	ldil	L%curproc, t1
	ldw	R%curproc(t1), t1
	ldw	p_addr(t1), t4

	/* restore state */
	ldw	TF_R1 +pcb_tf+u_pcb(t4), r1
	ldw	TF_R2 +pcb_tf+u_pcb(t4), r2
	ldw	TF_R3 +pcb_tf+u_pcb(t4), r3
	ldw	TF_R4 +pcb_tf+u_pcb(t4), r4
	ldw	TF_R5 +pcb_tf+u_pcb(t4), r5
	ldw	TF_R6 +pcb_tf+u_pcb(t4), r6
	ldw	TF_R7 +pcb_tf+u_pcb(t4), r7
	ldw	TF_R8 +pcb_tf+u_pcb(t4), r8
	ldw	TF_R9 +pcb_tf+u_pcb(t4), r9
	ldw	TF_R10+pcb_tf+u_pcb(t4), r10
	ldw	TF_R11+pcb_tf+u_pcb(t4), r11
	ldw	TF_R12+pcb_tf+u_pcb(t4), r12
	ldw	TF_R13+pcb_tf+u_pcb(t4), r13
	ldw	TF_R14+pcb_tf+u_pcb(t4), r14
	ldw	TF_R15+pcb_tf+u_pcb(t4), r15
	ldw	TF_R16+pcb_tf+u_pcb(t4), r16
	ldw	TF_R17+pcb_tf+u_pcb(t4), r17
	ldw	TF_R18+pcb_tf+u_pcb(t4), r18

	ldw	TF_CR8+pcb_tf(sr1,t4), t1
	ldw	TF_CR9+pcb_tf(sr1,t4), t2
	ldw	TF_CR12+pcb_tf(sr1,t4), t3
	mtctl	t1, pidr1
	mtctl	t2, pidr2
	mtctl	t3, pidr3
	ldw	pcb_space+u_pcb(t4), t3
	mtctl	t3, sr0
	mtctl	t3, sr2
	mtctl	t3, sr3
	mtctl	t3, sr4
	mtctl	t3, sr5
	mtctl	t3, sr6

	ldw	TF_CR13+pcb_tf(sr1,t4), t4
	bv	0(rp)
	mtctl	t4, pidr4

	.exit
	.procend

$syscall_end
	.align NBPG

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
	depi	0,31,12,r9	/* align offset to page */
#define	DTLBPRE \
	mfctl	ior, r9		/* Offset */			! \
	mfctl	isr, r8		/* Space  */			! \
	depi    0,31,12,r9	/* align offset to page */	! \
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
#ifdef FPEMUL_notyet
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
					/* 32 */
	.align	32*32


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

	ldil	L%fpu_curproc, t1
	ldw	R%fpu_curproc(t1), t1

	mfctl	ccr, t3

	ldil	L%curproc, t2
	ldw	R%curproc(t2), t2

	/* enable coprocessor */
	depi	3, 25, 2, t3
	mtctl	t3, ccr

	comb,=,n t1, t2, $fpusw_done
	comb,=,n r0, t1, $fpusw_nosave

	ldw	p_addr(t1), t3
	ldo	pcb_fpregs+u_pcb(t3), t3

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
	fstds    fr31, 8(t3)

$fpusw_nosave

	ldw	p_addr(t2), t3
	ldo	31*8+pcb_fpregs+u_pcb(t3), t3

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

	ldil	L%fpu_curproc, t1
	stw	t2, R%fpu_curproc(t1)

$fpusw_done
	mfctl	tr5, t3
	mfctl	tr3, t2
	mfctl	tr2, t1
	rfi
	nop

#ifdef notyet
	.export TLABEL(excpt), entry
	/*
	 * Emulate FPU/SFU if none/disabled
	 *
	 * iisq:iioq - exception triggered instruction
	 */
TLABEL(excpt)
	mtctl	sp, tr3
	mtctl	r31, tr2

	.import	$fpemu_stack
	ldil	L%$fpemu_stack, r31
	ldo	R%$fpemu_stack(r31), r31
	ldo	R%TF_SIZE+FM_SIZE(r31), sp

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
#endif
	
	.export	$sfu_emu, entry
$sfu_emu
	rfi
	ldo	1(r0), ret0	/* none supported by now */

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
	depi	1, 0, 1, r1	/* mark for ITLB insert */
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
	bb,<	r1, 0, $tlb_itlb
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

	/* insert other fast breaks here */

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

	mtctl	sp, tr2	/* do not overwrite tr4(cr28) */
	mtctl	t1, tr3
	mtctl	t3, tr5

	ldil	L%intr_recurse, t1
	ldw	R%intr_recurse(t1), t3
	ldo	1(t3), t3
	comib,<> 1, t3, $trap_recurse
	stw	t3, R%intr_recurse(t1)

	ldil	L%intr_stack, sp
	ldo	R%intr_stack(sp), sp

$trap_recurse
	copy	sp, t3
	ldo	FM_SIZE+TF_SIZE(sp), sp

	/* t3 is (struct trapframe *) */
$trap_trap
	stw	arg0, TF_R26(t3)
	copy	r1, arg0
	mfctl	tr7, r1

	mfctl	tr2, t1
	stw	t1, TF_R30(t3)

	mfctl	tr5, t1
	stw	t1, TF_R20(t3) /* t3 */
	stw	t2, TF_R21(t3)

	mfctl	tr3, t1
	stw	t1, TF_R22(t3)

	/*
	 * Now, save away other volatile state that prevents us from turning
	 * the PC queue back on, namely, the pc queue and ipsw, and the
	 * interrupt information.
	 */

	mfctl	pcoq, t1
	mtctl	r0, pcoq
	mfctl	pcoq, t2
	stw	t1, TF_IIOQH(t3)
	stw	t2, TF_IIOQT(t3)

	mfctl	pcsq, t1
	mtctl	r0, pcsq
	mfctl	pcsq, t2
	stw	t1, TF_IISQH(t3)
	stw	t2, TF_IISQT(t3)

	mfctl	ior, t1
	mfctl	ipsw, t2
	stw	t1, TF_CR21(t3)
	stw	t2, TF_CR22(t3)

	mfctl	eiem, t1
	stw	t1, TF_CR15(t3)

	mfctl	iir, t1
	mfctl	isr, t2
	stw	t1, TF_CR19(t3)
	stw	t2, TF_CR20(t3)

	/*
	 * Now we're about to turn on the PC queue.  We'll also go to virtual
	 * mode in the same step. Save the space registers sr4 - sr7 and
	 * point them to the kernel space
	 */

	mfsp	sr4, t1
	mfsp	sr5, t2
	stw	t1, TF_SR4(t3)
	stw	t2, TF_SR5(t3)

	mfsp	sr6, t1
	mfsp	sr7, t2
	stw	t1, TF_SR6(t3)
	stw	t2, TF_SR7(t3)

	/* XXX HPPA_SID_KERNEL == 0 */
	mtsp	r0, sr4
	mtsp	r0, sr5
	mtsp	r0, sr6
	mtsp	r0, sr7

	/*
	 * save the protection ID registers. We will keep the last one
	 * with the protection of the user's area and set the remaining
	 * ones to be the kernel.
	 */

	mfctl	pidr1, t1
	mfctl	pidr2, t2
	stw	t1, TF_CR8(t3)
	stw	t2, TF_CR9(t3)

	mfctl	pidr3, t1
	mfctl	pidr4, t2
	stw	t1, TF_CR12(t3)
	stw	t2, TF_CR13(t3)

	ldi	HPPA_PID_KERNEL,t1
	mtctl	t1,pidr1
	mtctl	t1,pidr2
	mtctl	t1,pidr3

	/* load the space queue */
	mtctl	r0, pcsq
	mtctl	r0, pcsq

	/*
	 * set the new psw to be data and code translation, interrupts
	 * disabled, protection enabled, Q bit on
	 */

	ldil	L%KERNEL_PSW, t1
	ldo	R%KERNEL_PSW(t1), t1
	mtctl	t1, ipsw

	/*
	 * Load up a real value into eiem to reflect an spl level of splhigh.
	 * Right now interrupts are still off.
	 */
	ldi	INT_NONE, t1
	mtctl	t1, eiem

	/* load in the address to "return" to with the rfir instruction */
	ldil	L%$trapnowvirt, t1
	ldo	R%$trapnowvirt(t1), t1

	/* load the offset queue */
	mtctl	t1, pcoq
	ldo	4(t1), t1
	mtctl	t1, pcoq

	/*
	 * Must do rfir not rfi since we may be called from tlbmiss routine
	 * (to handle page fault) and it uses the shadowed registers.
	 */
	rfir
	nop

$trapnowvirt
	/*
	 * t3 contains the virtual address of the saved status area
	 * t1 contains the trap flags
	 * sp contains the virtual address of the stack pointer
	 */
#if 0
	ldil	L%curproc, t1
	ldw	R%curproc(t1), t3
	comb,=	r0, t3, $trap_recurse
	nop

	ldo	FM_SIZE(sp), sp
	b	$trap_trap
	ldo	p_md(t3), t3
#endif
	stw	t1, TF_FLAGS(t3) /* XXX not really */

	/*
	 * Save all general registers that we haven't saved already
	 */

	stw	r1,TF_R1(t3)
	stw	r2,TF_R2(t3)
#ifdef DDB
	stw	rp,FM_CRP(sp)
	stw	r3,FM_PSP(sp)
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
	/*	r20 already saved
	 *	r21 already saved
	 *	r22 already saved */
	stw	r23,TF_R23(t3)
	stw	r24,TF_R24(t3)
	stw	r25,TF_R25(t3)
	/*	r26 already saved */
	stw	r27,TF_R27(t3)
	stw	r28,TF_R28(t3)
	stw	r29,TF_R29(t3)
 	/*	r30 already saved */
	stw	r31,TF_R31(t3)

	/*
	 * Save the space registers.
	 */

	mfsp	sr0, t1
	mfsp	sr1, t2
	stw	t1, TF_SR0(t3)
	stw	t2, TF_SR1(t3)

	mfsp	sr2, t1
	mfsp	sr3, t2
	stw	t1, TF_SR2(t3)
	stw	t2, TF_SR3(t3)

	/*
	 * Save the necessary control registers that were not already saved.
	 */

	mfctl	rctr, t1
	stw	t1, TF_CR0(t3)

	mfctl	sar, t1
	stw	t1, TF_CR11(t3)

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

	/*
	 * load the global pointer for the kernel
	 */

	ldil	L%$global$,dp
	ldo	R%$global$(dp),dp

	/*
	 * call the C routine trap().
	 * Trap type (arg0) was setup back in the beginning of the handler
	 */
	copy	t3, arg1

#ifdef DDB
	ldo	-FM_SIZE(sp), r3
#endif
	.import	trap, code
	ldil	L%trap,t1
	ldo	R%trap(t1),t1
	copy	t3, r5
	.call
	blr	r0,rp
	bv,n	r0(t1)
	nop
#if 0
	/* see if context really changed */
	ldil	L%curproc, t1
	ldw	R%curproc(t1), t2
	comb,=	t2, r0, $curproc_zero
	copy	r5, t3

	ldo	p_md(t2), t3	
$curproc_zero
#else
	copy	r5, t3
#endif
	/*
	 * Restore most of the state, up to the point where we need to turn
	 * off the PC queue. Going backwards, starting with control regs.
	 */

	ldw	TF_CR0(t3), t1
	ldw	TF_CR15(t3), t2
	mtctl	t1, rctr
	mtctl	t2, eiem

	ldw	TF_CR11(t3),t1
	mtctl	t1,sar

	ldw	TF_CR8(t3),t1
	mtctl	t1,pidr1


	/*
	 * Restore the lower space registers, we'll restore sr4 - sr7 after
	 * we have turned off translations
	 */

	ldw	TF_SR0(t3), t1
	ldw	TF_SR1(t3), t2
	mtsp	t1, sr0
	mtsp	t2, sr1

	ldw	TF_SR2(t3), t1
	ldw	TF_SR3(t3), t2
	mtsp	t1, sr2
	mtsp	t2, sr3

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
	/*	r21(t3) is used as a temporary and will be restored later */
	/*	r22(t3) is used as a temporary and will be restored later */
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

	ldw	TF_CR8(t3),t1
	mtctl	t1,pidr1

	ldw	TF_CR9(t3),t1
	mtctl	t1,pidr2

	ldw	TF_CR12(t3),t1
	mtctl	t1,pidr3

	/*
	 * restore the space registers
	 */

	ldw	TF_SR4(t3),t1
	ldw	TF_SR5(t3),t2
	mtsp	t1,sr4
	mtsp	t2,sr5

	ldw	TF_SR6(t3), t1
	ldw	TF_SR7(t3), t2
	mtsp	t1, sr6
	mtsp	t2, sr7

	/*
	 * finally we can restore the space and offset queues and the ipsw
	 */

	ldw	TF_IISQH(t3), t1
	ldw	TF_IISQT(t3), t2
	mtctl	t1, pcsq
	mtctl	t2, pcsq

	ldw	TF_IIOQH(t3), t1
	ldw	TF_IIOQT(t3), t2
	mtctl	t1, pcoq
	mtctl	t2, pcoq

	ldw	TF_CR22(t3), t1
	mtctl	t1, ipsw

	/*
	 * restore the last registers,r30, r22, and finally r21(t2)
	 * decrement interrupt recursion level
	 */
	ldil	L%intr_recurse, t1
	ldw	R%intr_recurse(t1), t2
	addi	-1, t2, t2
	stw	t2, R%intr_recurse(t1)
	ldw	TF_R22(t3),t1
	ldw	TF_R21(t3),t2
	ldw	TF_R30(t3),sp
	ldw	TF_R20(t3),t3

	rfi
	nop
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
	comb,>=	arg0, t1, fusubadaddr		! \
	ldil	L%curproc, t1			! \
	ldw	p_addr(t1), t1			! \
	ldil	L%fusufault, t2			! \
	ldw	u_pcb+pcb_onfault(t1), t3	! \
	ldo	R%fusufault(t2), t2		! \
	stw	t2, u_pcb+pcb_onfault(t1)	! \
	ldw	u_pcb+pcb_space(t1), t2		! \
	mtsp	t2, sr1

#define	FUX(name,insn)				  \
	FUSUX(name)				! \
	insn	0(sr1, arg0), ret0		! \
	b,n	fusuexit			! \
EXIT(name)

#define	SUX(name,insn)				  \
	FUSUX(name)				! \
	insn	arg1, 0(sr1, arg0)		! \
	b,n	fusuexit			! \
EXIT(name)

ENTRY(fusuexit)
	bv	r0(rp)	/* execute stw in a delay slot */
ALTENTRY(fusufault)
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

	ldw	VA_ARG4(sp), ret1
	mfctl	sr2, ret0	/* XXX need this? */
	mtctl	arg0, sr1
	mtctl	arg2, sr2
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
	ldw	VA_ARG4(sp), t1
	mtctl	ret0, sr2
	sub	ret1, t1, ret1
	bv	0(rp)
	stw	ret1, VA_ARG5(sp)
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
	ldw	R%curproc(t1), t2
	stw	r0, R%curproc(t1)

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
	stw	r0, p_back(arg1)
	stw	arg1, R%curproc(t1)

	/* Skip context switch if same process. */
	comb,=,n arg1, t2, switch_return

	/* If old process exited, don't bother. */
	comb,=,n r0, t2, switch_exited

	/*
	 * 2. save old proc context
	 *
	 * t2: old proc
	 *
	 * nothing to save, everything needed to be done is already
	 * done on enter, wonderfull.
	 */

	/* don't need old curproc(t2) starting from here */
switch_exited
	/*
	 * 3. restore new proc context
	 *
	 * arg1: new proc
	 * arg2: new pcb
	 */

	ldw	p_addr(arg1), arg2
	/* only pidr needs restoring, so we can access user space */
	ldw	TF_CR13+pcb_tf+u_pcb(arg2), t1
	mtctl	t1, pidr4

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

	/* setup kernel context */
	mtctl	r0, sr0
	mtctl	r0, sr1
	mtctl	r0, sr2
	mtctl	r0, sr3
	mtctl	r0, sr4
	mtctl	r0, sr5
	mtctl	r0, sr6
	mtctl	r0, sr7

	/* leave pidr4 in user space so copy* work */
	ldi	HPPA_PID_KERNEL, t4
	mtctl	t4, pidr1
	mtctl	t4, pidr2
	mtctl	t4, pidr3

	b	switch_search
	nop
EXIT(switch_exit)

ENTRY(switch_trampoline)
	/* XXX load curproc here? */
	bv	0(arg0)
	copy	arg1, arg0
EXIT(switch_trampoline)

/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 */
ENTRY(sigcode)
	/* TODO signal trampoline */
ALTENTRY(esigcode)
	bv	0(rp)
	nop
EXIT(sigcode)

#ifdef COMPAT_LINUX
ENTRY(linux_sigcode)

ALTENTRY(linux_esigcode)
	/* TODO linux signal trampoline */
	bv	0(rp)
	nop
EXIT(linix_esigcode)
#endif /* COMPAT_LINUX */

	.end
