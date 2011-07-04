/*	$OpenBSD: locore.S,v 1.28 2011/07/04 17:07:27 kettenis Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

	.level	2.0w

#include <sys/reboot.h>
#include <machine/param.h>
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/iomod.h>
#include <machine/pdc.h>
#include <machine/frame.h>
#include <machine/reg.h>
#include <machine/pte.h>
#include "assym.h"

	.import __gp, data
	.import pdc, data
	.import boothowto, data
	.import bootdev, data
	.import esym, data
	.import proc0, data
	.import proc0paddr, data
	.import proc0fpstate, data

#define	EMRG_STACKSIZE	(1*PAGE_SIZE)
#define	FPEMU_STACKSIZE	(1*PAGE_SIZE)

	.data

	.align	16

	BSS(pdc_stack, 8)	/* temp stack for PDC call */
	BSS(emrg_stack, 8)	/* stack for HPMC/TOC/PWRF */
	BSS(fpemu_stack, 8)	/* stack for FPU emulation */

	.export fpu_enable, data
	BSS(fpu_enable, 4)      /* bits to set in the %cr10 to enable fpu */
	.export fpu_curpcb, data
	BSS(fpu_curpcb, 8)      /* pcb of the fpu owner */
	BSS(fpu_scratch, 16)    /* FPU scratch space, enough for a quad */

	.text

/*
 * $start(pdc, boothowto, bootdev, esym)
 */
	.export $start, entry
$start
	rsm	(PSL_R|PSL_Q|PSL_I|PSL_P|PSL_D), %r0
	nop ! nop ! nop ! nop

	ldil	L%__gp, %dp
	ldo	R%__gp(%dp), %dp

	ldi	HPPA_PID_KERNEL, %r1
	mtctl	%r1, %pidr1
	mtctl	%r1, %pidr2
	mtctl	%r0, %pidr3
	mtctl	%r0, %pidr4

	mtsp	%r0, %sr0
	mtsp	%r0, %sr1
	mtsp	%r0, %sr2
	mtsp	%r0, %sr3
	mtsp	%r0, %sr4
	mtsp	%r0, %sr5
	mtsp	%r0, %sr6
	mtsp	%r0, %sr7

	ldi	-1, %r1
	mtctl	%r0, %eiem
	mtctl	%r1, %eirr
	mtctl	%r0, %cr10	/* ccr */

	ldil	L%cpu0_info, %r1
	ldo	R%cpu0_info(%r1), %r1
	mtctl	%r1, %cr24
	ldil	L%(PSL_W|PSL_Q|PSL_P|PSL_C|PSL_D|PSL_S|PSL_O), %r8
	ldo	R%(PSL_W|PSL_Q|PSL_P|PSL_C|PSL_D|PSL_S|PSL_O)(%r8), %r8
	stw	%r8, CI_PSW(%r1)
	ldi	IPL_HIGH, %r8
	stw	%r8, CI_CPL(%r1)

	ldil	L%pdc, %r1
	std	%arg0, R%pdc(%r1)
	ldil	L%boothowto, %r1
	stw	%arg1, R%boothowto(%r1)
	ldil	L%bootdev, %r1
	stw	%arg2, R%bootdev(%r1)
	ldil	L%esym, %r1
	std	%arg3, R%esym(%r1)

	/* align esym */
	ldo	PAGE_MASK(%arg3), %arg3
	depw	%r0, 31, PAGE_SHIFT, %arg3

	.import	__kernel_pagedir, data
	ldil	L%__kernel_pagedir, %r1
	ldo	R%__kernel_pagedir(%r1), %r1
	mtctl	%r1, %cr25	/* vtop */

	/* setup kernel initial PIEs */
	ldo	PAGE_SIZE(%r1), %r31
	extrd,u	%r31, 51, 32, %arg0
	ldo	1(%arg0), %arg1
	ldo	2(%arg0), %arg2
	stw	%arg0, 0(%r1)
	stw	%arg1, 4(%r1)
		/* (VM_MIN_KERNEL_ADDRESS & PIE_MASK) >> PIE_SHIFT */
	ldo	1(%arg2), %arg0
	stw	%arg2, 0x20*4(%r1)
	stw	%arg0, 0xffc(%r1)

	.import	kernel_ptes, data
	ldil	L%kernel_ptes, %r1
	ldo	R%kernel_ptes(%r1), %r1

	ldd,ma	8(%r1), %r8
	ldd,ma	8(%r1), %r9
	ldd,ma	8(%r1), %r10
	ldd,ma	8(%r1), %r11

	/* fill eight pages at the same time */
	copy	%arg3, %arg0
	ldo	PAGE_SIZE(%arg0), %arg1
	extrd,u	%arg0, 51, 32, %r1
	stw,ma	%r1, 4(%r31)
	ldo	PAGE_SIZE(%arg1), %arg2
	extrd,u	%arg1, 51, 32, %r1
	stw,ma	%r1, 4(%r31)
	ldo	PAGE_SIZE(%arg2), %arg3
	extrd,u	%arg2, 51, 32, %r1
	stw,ma	%r1, 4(%r31)
	ldo	PAGE_SIZE(%arg3), arg4
	extrd,u	%arg3, 51, 32, %r1
	stw,ma	%r1, 4(%r31)
	ldo	PAGE_SIZE(arg4), arg5
	extrd,u	arg4, 51, 32, %r1
	stw,ma	%r1, 4(%r31)
	ldo	PAGE_SIZE(arg5), arg6
	extrd,u	arg5, 51, 32, %r1
	stw,ma	%r1, 4(%r31)
	ldo	PAGE_SIZE(arg6), arg7
	extrd,u	arg6, 51, 32, %r1
	stw,ma	%r1, 4(%r31)
	extrd,u	arg7, 51, 32, %r1
	stw,ma	%r1, 4(%r31)
	ldi	PAGE_SIZE, %r1
$start_set_ptes
	std,ma,bc %r8, 8(%arg0)
	std,ma,bc %r8, 8(%arg1)
	std,ma,bc %r9, 8(%arg2)
	std,ma,bc %r9, 8(%arg3)
	std,ma,bc %r10, 8(arg4)
	std,ma,bc %r10, 8(arg5)
	std,ma,bc %r11, 8(arg6)
	addib,>= -8, %r1, $start_set_ptes
	std,ma,bc %r11, 8(arg7)

	ldo	-8(arg7), %arg3

	/* assuming size being page-aligned */
#define	STACK_ALLOC(n,s)	\
	ldil	L%n, %r1	! \
	ldil	L%s, %r31	! \
	std	%arg3, R%n(%r1)	! \
	add	%arg3, %r31, %arg3

	STACK_ALLOC(pdc_stack, PDC_STACKSIZE)
	STACK_ALLOC(emrg_stack, EMRG_STACKSIZE)
	STACK_ALLOC(fpemu_stack, FPEMU_STACKSIZE)

	copy	%arg3, %arg0
	ldi	PAGE_SIZE+TRAPFRAME_SIZEOF, %r1
$start_zero_tf
	std,ma,bc %r0, 8(%arg0)
	addib,>= -16, %r1, $start_zero_tf
	std,ma,bc %r0, 8(%arg0)

	/* setup proc0/user0 */
	ldil	L%(USPACE+PAGE_SIZE), %arg0
	add	%arg0, %arg3, %arg0
	ldil	L%proc0paddr, %r1
	std	%arg3, R%proc0paddr(%r1)

	ldo	PAGE_SIZE(%arg3), %sp
	std	%r0, U_PCB+PCB_ONFAULT(%arg3)
	std	%r0, U_PCB+PCB_SPACE(%arg3)

	ldil	L%proc0, %r1
	ldo	R%proc0(%r1), %r1
	ldo	-TRAPFRAME_SIZEOF(%sp), %arg2
	std	%arg3, P_ADDR(%r1)
	std	%arg2, P_MD_REGS(%r1)

	ldil	TFF_LAST, %r1
	std	%r1, TF_FLAGS(%arg2)
	ldil	L%proc0fpstate, %r1
	ldo	R%proc0fpstate(%r1), %r1
	std	%r1, TF_CR30(%arg2)
	mtctl	%r1, %cr30

	mfctl	%cr24, %r1
	ldw	CI_PSW(%r1), arg7
	ldil	L%$start_virt, %r31
	ldo	R%$start_virt(%r31), %r31
	ldil	L%$ivaaddr, %r1
	ldo	R%$ivaaddr(%r1), %r1
	mtctl	%r1, %iva
	mtctl	%r0, %pcsq
	mtctl	%r0, %pcsq
	mtctl	%r31, %pcoq
	ldo	4(%r31), %r31
	mtctl	%r31, %pcoq
	mtctl	arg7, %ipsw
	rfi
	nop

$start_virt
	copy	%arg0, %r5
	copy	%sp, %r3
	std,ma	%r0, HPPA_FRAME_SIZE(%sp)
	.call
	b,l	hppa_init, %rp
	ldo	-16(%sp), ap

$start_callmain
	.import main, code
	.call
	b,l	main, %rp
	ldo	-TRAPFRAME_SIZEOF(%arg0), %r5
	.size	$start, .-$start

/* int
 * pdc_call(func, pdc_flag, ...)
 *      iodcio_t func;
 *      int pdc_flag;
 */
ENTRY(pdc_call,160)
	std	%rp, HPPA_FRAME_RP(%sp)
	std	%sp, HPPA_FRAME_PSP(%sp)
	copy	%r3, %r31
	copy	%sp, %ret0

	ldil	L%pdc_stack, %r1
	ldd	R%pdc_stack(%r1), %r3
	copy	%r3, %sp
	std,ma	%r31, 32+12*4+28*8(%sp)	/* old frame+args+save(64) */

	mfctl	%eiem, %r1
	mtctl	%r0, %eiem
	std	%r1, 1*8(%r3)
	std	%r4, 12*8(%r3)
	std	%r5, 13*8(%r3)
	std	%r6, 14*8(%r3)
	std	%r7, 15*8(%r3)
	std	%r8, 16*8(%r3)
	std	%r9, 17*8(%r3)
	std	%r10, 18*8(%r3)
	std	%r11, 19*8(%r3)
	std	%r12, 20*8(%r3)
	std	%r13, 21*8(%r3)
	std	%r14, 22*8(%r3)
	std	%r15, 23*8(%r3)
	std	%r16, 24*8(%r3)
	std	%r17, 25*8(%r3)
	std	%r18, 26*8(%r3)

	copy	%arg0, %r31
	/* copy arguments */
	copy	%arg2, %arg0
	copy	%arg3, %arg1
	copy	arg4, %arg2
	copy	arg5, %arg3

	ldd	0*8(ap), arg4
	ldd	1*8(ap), arg5
	stw	arg6, -32-(4+1)*4(%sp)
	stw	arg7, -32-(5+1)*4(%sp)
	stw	arg4, -32-(6+1)*4(%sp)
	stw	arg5, -32-(7+1)*4(%sp)
	ldd	2*8(ap), arg4
	ldd	3*8(ap), arg5
	ldd	4*8(ap), arg6
	ldd	5*8(ap), arg7
	stw	arg4, -32-(8+1)*4(%sp)
	stw	arg5, -32-(9+1)*4(%sp)
	stw	arg6, -32-(10+1)*4(%sp)
	stw	arg7, -32-(11+1)*4(%sp)

	mfctl	%cr24, arg4
	mfctl	%cr25, arg5
	mfctl	%cr26, arg6
	mfctl	%cr27, arg7
	std	arg4, 2*8(%r3)
	std	arg5, 3*8(%r3)
	std	arg6, 4*8(%r3)
	std	arg7, 5*8(%r3)
	mfctl	%cr28, arg4
	mfctl	%cr29, arg5
	mfctl	%cr30, arg6
	mfctl	%cr31, arg7
	std	arg4, 6*8(%r3)
	std	arg5, 7*8(%r3)
	std	arg6, 8*8(%r3)
	std	arg7, 9*8(%r3)

	copy	%arg0, %ret1
	std	%ret0, 11*8(%r3)	/* old %sp */
	ldi	PSL_Q, %arg0
	break	HPPA_BREAK_KERNEL, HPPA_BREAK_SET_PSW
	std	%ret0, 27*8(%r3)
	copy	%ret1, %arg0

	.call
	blr	%r0, %rp
	bv,n	(%r31)
	nop

	/* load temp control regs */
	ldd	2*8(%r3), arg4
	ldd	3*8(%r3), arg5
	ldd	4*8(%r3), arg6
	ldd	5*8(%r3), arg7
	mtctl	arg4, %cr24
	mtctl	arg5, %cr25
	mtctl	arg6, %cr26
	mtctl	arg7, %cr27
	ldd	6*8(%r3), arg4
	ldd	7*8(%r3), arg5
	ldd	8*8(%r3), arg6
	ldd	9*8(%r3), arg7
	mtctl	arg4, %cr28
	mtctl	arg5, %cr29
	mtctl	arg6, %cr30
	mtctl	arg7, %cr31

	ldd	27*8(%r3), %arg0
	copy	%ret0, %arg1
	break	HPPA_BREAK_KERNEL, HPPA_BREAK_SET_PSW
	copy	%arg1, %ret0

	ldd	1*8(%r3), arg5	/* %eiem */
	ldd	26*8(%r3), %r18
	ldd	25*8(%r3), %r17
	ldd	24*8(%r3), %r16
	ldd	23*8(%r3), %r15
	ldd	22*8(%r3), %r14
	ldd	21*8(%r3), %r13
	ldd	20*8(%r3), %r12
	ldd	19*8(%r3), %r11
	ldd	18*8(%r3), %r10
	ldd	17*8(%r3), %r9
	ldd	16*8(%r3), %r8
	ldd	15*8(%r3), %r7
	ldd	14*8(%r3), %r6
	ldd	13*8(%r3), %r5
	ldd	12*8(%r3), %r4
	ldd	11*8(%r3), %sp
	ldd	HPPA_FRAME_RP(%sp), %rp
	ldd	0(%r3), %r3

	bv	%r0(%rp)
	mtctl	arg5, %eiem
EXIT(pdc_call)

/*
 * the whole syscall() glue is fit on one page
 * both enter and return paths
 */
/* TODO has to be mapped w/a special perms */
	.align  PAGE_SIZE
	.export gateway_page, entry
gateway_page
	nop
	gate,n	$bsd_syscall,%r0
	nop

	.align	256

$bsd_syscall
	mfctl	%eiem, %r1
	mtctl	%r0, %eiem
	mtsp	%r0, %sr1
	mfctl	%pidr1, %ret0
	ldi	HPPA_PID_KERNEL, %ret0
	mtctl	%ret0, %pidr1

/* TODO prepare for syscall() */

	.import syscall,code
	b,l	syscall, %rp
	nop

	mfctl	%cr24, %arg1
	ldd	CI_CURPROC(%arg1), %r1
	ldw	P_MD_REGS(%r1), %arg0

$syscall_return
	/* %arg0 -- trapframe */

	mtctl	%r0, %eiem

	/*
	 * Store registers in physical trap save area for restore after the
	 * system mask has been reset and we are no longer in virtual mode.
	 */
	mfctl	%cr24, %r1
	ldo	CI_TRAP_SAVE(%r1), %r1

	ldd	TF_ARGS+4*8(%arg0), %arg2	/* arg3 */
	ldd	TF_ARGS+5*8(%arg0), %arg3	/* arg2 */
	std	%arg2, 0*8(%r1)
	std	%arg3, 1*8(%r1)

	ldd	TF_ARGS+6*8(%arg0), %arg2	/* arg1 */
	ldd	TF_ARGS+7*8(%arg0), %arg3	/* arg0 */
	std	%arg2, 2*8(%r1)
	std	%arg3, 3*8(%r1)

	ldd	TF_IIOQ+0(%arg0), %arg2		/* pcoq */
	ldd	TF_IIOQ+8(%arg0), %arg3
	std	%arg2, 4*8(%r1)
	std	%arg3, 5*8(%r1)

	ldd	TF_IISQ+0(%arg0), %arg2		/* pcsq */
	ldd	TF_IISQ+8(%arg0), %arg3
	std	%arg2, 6*8(%r1)
	std	%arg3, 7*8(%r1)

	ldd	TF_PIDR1(%arg0), %arg2		/* pidr1 */
	ldd	TF_PIDR2(%arg0), %arg3		/* pidr2 */
	std	%arg2, 8*8(%r1)
	std	%arg3, 9*8(%r1)

	ldd	TF_SR0(%arg0), %arg2		/* sr0 */
	ldd	TF_SR1(%arg0), %arg3		/* sr1 */
	std	%arg2, 10*8(%r1)
	std	%arg3, 11*8(%r1)

	ldd	TF_IPSW(%arg0), %arg2		/* ipsw */
	ldd	TF_EIEM(%arg0), %arg3		/* eiem */
	std	%arg2, 12*8(%r1)
	std	%arg3, 13*8(%r1)

	ldd	TF_SP(%arg0), %arg2		/* sp */
	std	%arg2, 14*8(%r1)

	/*
	 * Restore general registers.
	 */
	ldd	TF_R1(%arg0), %r1
	ldd	TF_RP(%arg0), %rp
	ldd	TF_R3(%arg0), %r3
	ldd	TF_R4(%arg0), %r4
	ldd	TF_R5(%arg0), %r5
	ldd	TF_R6(%arg0), %r6
	ldd	TF_R7(%arg0), %r7
	ldd	TF_R8(%arg0), %r8
	ldd	TF_R9(%arg0), %r9
	ldd	TF_R10(%arg0), %r10
	ldd	TF_R11(%arg0), %r11
	ldd	TF_R12(%arg0), %r12
	ldd	TF_R13(%arg0), %r13
	ldd	TF_R14(%arg0), %r14
	ldd	TF_R15(%arg0), %r15
	ldd	TF_R16(%arg0), %r16
	ldd	TF_R17(%arg0), %r17
	ldd	TF_R18(%arg0), %r18
	ldd	TF_ARGS+0*8(%arg0), %r19
	ldd	TF_ARGS+1*8(%arg0), %r20
	ldd	TF_ARGS+2*8(%arg0), %r21
	ldd	TF_ARGS+3*8(%arg0), %r22
	/* Restore arg3, arg2, arg1 and arg0 later. */
	ldd	TF_DP(%arg0), %r27
	ldd	TF_RET0(%arg0), %r28
	ldd	TF_RET1(%arg0), %r29
	/* Restore sp later. */
	ldd	TF_R31(%arg0), %r31

	/* Restore control registers. */
	ldd	TF_RCTR(%arg0), %arg2		/* rctr */
	ldd	TF_CCR(%arg0), %arg3		/* ccr */
	mtctl	%arg2, %cr0
	mtctl	%arg3, %cr10

	ldd	TF_IOR(%arg0), %arg2		/* ior */
	ldd	TF_ISR(%arg0), %arg3		/* isr */
	mtctl	%arg2, %cr27
	mtsp	%arg3, %sr6

	ldd	TF_SAR(%arg0), %arg2		/* sar */
	ldd	TF_VTOP(%arg0), %arg3		/* vtop */
	mtctl	%arg2, %sar
	mtctl	%arg3, %cr25

	ldd	TF_CR30(%arg0), %arg2		/* pa(u) */
	ldd	TF_CR27(%arg0), %arg3		/* user curthread */
	mtctl	%arg2, %cr30
	mtctl	%arg3, %cr27

	ldd	TF_SR2(%arg0), %arg2
	ldd	TF_SR3(%arg0), %arg3
	mtsp	%arg2, %sr2
	mtsp	%arg3, %sr3

	ldd	TF_SR6(%arg0), %arg2
	ldd	TF_SR7(%arg0), %arg3
	mtsp	%arg2, %sr6
	mtsp	%arg3, %sr7

	ldd	TF_SR4(%arg0), %arg2
	ldd	TF_SR5(%arg0), %arg3
	mtsp	%arg2, %sr4
	mtsp	%arg3, %sr5

	/* Clear system masks before restoring queues and space registers. */
	rsm	(PSL_R|PSL_Q|PSL_P|PSL_I|PSL_D), %r0
	nop ! nop ! nop ! nop

	mfctl	%cr24, %arg0
	ldo	CI_TRAP_SAVE(%arg0), %arg0

	ldd	4*8(%arg0), %arg2		/* pcoq */
	ldd	5*8(%arg0), %arg3
	mtctl	%arg2, %pcoq
	mtctl	%arg3, %pcoq

	ldd	6*8(%arg0), %arg2		/* pcsq */
	ldd	7*8(%arg0), %arg3
	mtctl	%arg2, %pcsq
	mtctl	%arg3, %pcsq

	ldd	8*8(%arg0), %arg2		/* pidr1 */
	ldd	9*8(%arg0), %arg3		/* pidr2 */
	mtctl	%arg2, %pidr1
	mtctl	%arg3, %pidr2

	ldd	10*8(%arg0), %arg2		/* sr0 */
	ldd	11*8(%arg0), %arg3		/* sr1 */
	mtsp	%arg2, %sr0
	mtsp	%arg3, %sr1

	ldd	12*8(%arg0), %arg2		/* ipsw */
	ldd	13*8(%arg0), %arg3		/* eiem */
	mtctl	%arg2, %ipsw
	mtctl	%arg3, %eiem

	ldd	14*8(%arg0), %sp		/* sp */

	/* Restore temporary registers. */
	ldd	0*8(%arg0), %arg3
	ldd	1*8(%arg0), %arg2
	ldd	2*8(%arg0), %arg1
	ldd	3*8(%arg0), %arg0

	rfi
	nop
	.size	$bsd_syscall, .-$bsd_syscall
	.size	$syscall_return, .-$syscall_return

gateway_end
	. = gateway_page + PAGE_SIZE
	.size	gateway_page, .-gateway_page

/*
 * interrupt vector table
 */
#define	TLABEL(name)	$trap$name
#define	TRAP(name,num,pre) \
	.export	TLABEL(name)$num, entry	! \
	.label	TLABEL(name)$num	! \
	pre				! \
	.call				! \
	.import TLABEL(name), code	! \
	b	TLABEL(name)		! \
	ldi	num, %r1		! \
	.align	32

#define	ITLBPRE				\
	mfctl	%pcsq, %r8		! \
	mfctl	%pcoq, %r9		! \
	extrd,u	%r8, 63, SID_SHIFT, %r1	! \
	depd	%r1, 31, SID_SHIFT, %r9	! \
	depd	%r0, 63, SID_SHIFT, %r8

#define	DTLBPRE				\
	mfctl	%isr, %r8		! \
	mfctl	%ior, %r9		! \
	extrd,u	%r8, 63, SID_SHIFT, %r1	! \
	depd	%r1, 31, SID_SHIFT, %r9	! \
	depd	%r0, 63, SID_SHIFT, %r8

#define	HPMCPRE	nop

#define	BRKPRE				\
	mfctl	%pcoq, %r8

	.align PAGE_SIZE	/* already */
	.export $ivaaddr, entry
	.export hpmc_v, entry
$ivaaddr
	TRAP(all,T_NONEXIST,)		/*  0. invalid interrupt vector */
hpmc_v
	TRAP(hpmc,T_HPMC,HPMCPRE)	/*  1. high priority machine check */
	TRAP(pfr,T_POWERFAIL,)		/*  2. power failure */
	TRAP(all,T_RECOVERY,)		/*  3. recovery counter trap */
	TRAP(all,T_INTERRUPT,)		/*  4. external interrupt */
	TRAP(all,T_LPMC,)		/*  5. low-priority machine check */
	TRAP(itlb,T_ITLBMISS,ITLBPRE)	/*  6. instruction TLB miss fault */
	TRAP(all,T_IPROT,)		/*  7. instruction protection trap */
	TRAP(all,T_ILLEGAL,)		/*  8. Illegal instruction trap */
	TRAP(ibrk,T_IBREAK,BRKPRE)	/*  9. break instruction trap */
	TRAP(all,T_PRIV_OP,)		/* 10. privileged operation trap */
	TRAP(all,T_PRIV_REG,)		/* 11. privileged register trap */
	TRAP(all,T_OVERFLOW,)		/* 12. overflow trap */
	TRAP(all,T_CONDITION,)		/* 13. conditional trap */
	TRAP(excpt,T_EXCEPTION,)	/* 14. assist exception trap */
	TRAP(dtlb,T_DTLBMISS,DTLBPRE)	/* 15. data TLB miss fault */
	TRAP(itlbna,T_ITLBMISSNA,DTLBPRE)/* 16. ITLB non-access miss fault */
	TRAP(dtlbna,T_DTLBMISSNA,DTLBPRE)/* 17. DTLB non-access miss fault */
	TRAP(all,T_DPROT,)		/* 18. data protection trap
					      unaligned data reference trap */
	TRAP(all,T_DBREAK,)		/* 19. data break trap */
	TRAP(tlbd,T_TLB_DIRTY,DTLBPRE)	/* 20. TLB dirty bit trap */
	TRAP(all,T_PAGEREF,)		/* 21. page reference trap */
	TRAP(emu,T_EMULATION,)		/* 22. assist emulation trap */
	TRAP(all,T_HIGHERPL,)		/* 23. higher-privelege transfer trap*/
	TRAP(all,T_LOWERPL,)		/* 24. lower-privilege transfer trap */
	TRAP(all,T_TAKENBR,)		/* 25. taken branch trap */
	TRAP(all,T_DATACC,)		/* 26. data access rights trap */
	TRAP(all,T_DATAPID,)		/* 27. data protection ID trap */
	TRAP(all,T_DATALIGN,)		/* 28. unaligned data ref trap */
	TRAP(all,29,)
	TRAP(all,30,)
	TRAP(all,31,)
	TRAP(all,32,)
	TRAP(all,33,)
	TRAP(all,34,)
	TRAP(all,35,)
	TRAP(all,36,)
	TRAP(all,37,)
	TRAP(all,38,)
	TRAP(all,39,)
	TRAP(all,40,)
	TRAP(all,41,)
	TRAP(all,42,)
	TRAP(all,43,)
	TRAP(all,44,)
	TRAP(all,45,)
	TRAP(all,46,)
	TRAP(all,47,)
	TRAP(all,48,)
	TRAP(all,49,)
	TRAP(all,50,)
	TRAP(all,51,)
	TRAP(all,52,)
	TRAP(all,53,)
	TRAP(all,54,)
	TRAP(all,55,)
	TRAP(all,56,)
	TRAP(all,57,)
	TRAP(all,58,)
	TRAP(all,59,)
	TRAP(all,60,)
	TRAP(all,61,)
	TRAP(all,62,)
	TRAP(all,63,)
					/* 64 */

/*
 * High Priority Machine Check Interrupt
 */
	.export TLABEL(hpmc), entry
ENTRY(TLABEL(hpmc),0)
	rfi
	nop

hpmc_never_dies
	b	hpmc_never_dies
	nop
EXIT(TLABEL(hpmc))

/*
 * transfer of control handler
 */
ENTRY(hppa_toc,0)
	rfi
	nop

ALTENTRY(hppa_toc_end)
	.word	0
EXIT(hppa_toc)

/*
 * power fail recovery handler
 */
ENTRY(TLABEL(pfr),0)
	rfi
	nop

ALTENTRY(hppa_pfr_end)
	.word	0
EXIT(hppa_pfr)

/*
 * system breaks
 */
	.export TLABEL(ibrk), entry
ENTRY(TLABEL(ibrk),0)
	/* If called by a user process then always pass it to trap() */
	extrd,u,*= %r8, 63, 2, %r0
	b,n	TLABEL(all)

	/* don't accept breaks from data segments */
	.import etext, code
	ldil	L%etext, %r9
	ldo	R%etext(%r9), %r9
	cmpb,*>>=,n %r8, %r9, TLABEL(all)

	mfctl	%iir, %r8
	extrd,u,*= %r8, 63, 5, %r0
	b,n	TLABEL(all)

	/* now process all those `break' calls we make */
	extrd,u	%r8, 50, 13, %r9
	comib,=,n HPPA_BREAK_GET_PSW, %r9, $ibrk_getpsw
	comib,=,n HPPA_BREAK_SET_PSW, %r9, $ibrk_setpsw

	b	TLABEL(all)
	nop

$ibrk_getpsw
	b	$ibrk_exit
	mfctl	%ipsw, %ret0

$ibrk_setpsw
	mfctl	%ipsw, %ret0
	b	$ibrk_exit
	mtctl	%arg0, %ipsw

	/* insert other fast breaks here */
	nop ! nop

$ibrk_exit
	/* skip the break */
	mtctl	%r0, %pcoq
	mfctl	%pcoq, %r9
	mtctl	%r9, %pcoq
	ldo	4(%r9), %r9
	mtctl	%r9, %pcoq

	rfi,r
	nop
EXIT(TLABEL(ibrk))

	.export TLABEL(all), entry
ENTRY(TLABEL(all),0)
	/* %r1 still has trap type */
	mfctl	%ipsw, %r9
	mtctl	%r9, %cr31

	mtsp	%r0, %sr3
	ldi	HPPA_PID_KERNEL, %r9
	mtctl	%r9, %pidr3

	mfctl	%isr, %r8
	mfctl	%ior, %r9
	extrd,u	%r8, 63, SID_SHIFT, %r16
	depd	%r16, 31, SID_SHIFT, %r9
	depd	%r0, 63, SID_SHIFT, %r8
	mtsp	%r8, %sr6
	mtctl	%r9, %cr27

	mfctl	%pcsq, %r24
	mtctl	%r0, %pcsq
	mfctl	%pcsq, %r25
	mtctl	%r0, %pcsq
	mtsp	%r24, %sr4
	mtsp	%r25, %sr5

	/* TODO fix pcsq:pcoq split */
	ldil	L%TLABEL(all_virt), %r9
	ldo	R%TLABEL(all_virt)(%r9), %r9
	mfctl	%pcoq, %r24
	mtctl	%r9, %pcoq
	mfctl	%pcoq, %r25
	ldo	4(%r9), %r9
	mtctl	%r24, %cr28
	mtctl	%r9, %pcoq
	mtctl	%r25, %cr29

	mfctl	%cr24, %r9
	ldw	CI_PSW(%r9), %r9
	depi	0, PSL_I_POS, 1, %r9	/* Leave interrupts disabled. */
	mtctl	%r9, %ipsw

	mfsp	%sr0, %r8
	extrd,u,*<> %r24, 63, 2, %r0	/* still pcoq head */
	cmpb,*= %r0, %r8, TLABEL(all_kern)
	copy	%sp, %r17

	mfctl	%cr24, %r9
	ldd	CI_CURPROC(%r9), %r9
	depdi	1, T_USER_POS, 1, %r1
	depdi	1, TFF_LAST_POS, 1, %r1
	ldd	P_ADDR(%r9), %sp
	ldo	PAGE_SIZE-TRAPFRAME_SIZEOF(%sp), %sp

TLABEL(all_kern)
	ldo	127(%sp), %sp
	depdi	0, 63, 7, %sp

	mfctl	%cr24, %r8
	ldo	CI_TRAP_SAVE(%r8), %r8
	std	%r17, 0(%r8)			/* sp */
	fdc	%r0(%r8)
	ldo	8(%r8), %r8

	mfctl	%iir, %r16
	std	%r16, 0(%r8)			/* iir */
	fdc	%r0(%r8)

	mtctl	%r1, %cr26
	rfi,r
	nop ! nop ! nop ! nop ! nop ! nop ! nop ! nop
TLABEL(all_virt)

	ldo	8(%sp), %sp	/* space for flags */
	std,ma	%r1, 8(%sr3,%sp)
	mfctl	%cr26, %r1		/* sar */
	std	%r1, -16(%sr3,%sp)
	std,ma	%rp, 8(%sr3,%sp)
	std,ma	%r3, 8(%sr3,%sp)
	std,ma	%r4, 8(%sr3,%sp)
	std,ma	%r5, 8(%sr3,%sp)
	std,ma	%r6, 8(%sr3,%sp)
	std,ma	%r7, 8(%sr3,%sp)
	std,ma	%r8, 8(%sr3,%sp)
	std,ma	%r9, 8(%sr3,%sp)
	std,ma	%r10, 8(%sr3,%sp)
	std,ma	%r11, 8(%sr3,%sp)
	std,ma	%r12, 8(%sr3,%sp)
	std,ma	%r13, 8(%sr3,%sp)
	std,ma	%r14, 8(%sr3,%sp)
	std,ma	%r15, 8(%sr3,%sp)
	std,ma	%r16, 8(%sr3,%sp)
	std,ma	%r17, 8(%sr3,%sp)
	std,ma	%r18, 8(%sr3,%sp)
	std,ma	%r19, 8(%sr3,%sp)
	std,ma	%r20, 8(%sr3,%sp)
	std,ma	%r21, 8(%sr3,%sp)
	std,ma	%r22, 8(%sr3,%sp)
	std,ma	%r23, 8(%sr3,%sp)
	std,ma	%r24, 8(%sr3,%sp)
	std,ma	%r25, 8(%sr3,%sp)
	std,ma	%r26, 8(%sr3,%sp)
	std,ma	%r27, 8(%sr3,%sp)
	std,ma	%r28, 8(%sr3,%sp)
	std,ma	%r29, 8(%sr3,%sp)
	ldo	-30*8(%sp), %arg1
	ldo	8(%sp), %sp		/* %sp */
	std,ma	%r31, 8(%sr3,%sp)
	copy	%r1, %arg0

	mfsp	%sr0, %arg2
	mfsp	%sr1, %arg3
	std,ma	%arg2, 8(%sr3,%sp)	/* %sr0 */
	std,ma	%arg3, 8(%sr3,%sp)	/* %sr1 */

	mfsp	%sr2, %arg2
	mfsp	%sr3, %arg3
	std,ma	%arg2, 8(%sr3,%sp)	/* %sr2 */
	std,ma	%arg3, 8(%sr3,%sp)	/* %sr3 */

	mfsp	%sr4, %arg2
	mfsp	%sr5, %arg3
	std,ma	%arg2, 8(%sr3,%sp)	/* %sr4 */
	std,ma	%arg3, 8(%sr3,%sp)	/* %sr5 */

	mfsp	%sr6, %arg2
	mfsp	%sr7, %arg3
	std,ma	%arg2, 8(%sr3,%sp)	/* %sr6 */
	std,ma	%arg3, 8(%sr3,%sp)	/* %sr7 */

	mfctl	%cr0, %arg3
	mfctl	%cr10, %arg2
	std,ma	%arg3, 8(%sr3,%sp)	/* rctr */
	std,ma	%arg2, 8(%sr3,%sp)	/* ccr */

	mfctl	%cr28, %arg2		/* pcoq */
	mfctl	%cr29, %arg3
	std,ma	%arg2, 8(%sr3,%sp)
	std,ma	%arg3, 8(%sr3,%sp)

	mfsp	%sr4, %arg2		/* pcsq */
	mfsp	%sr5, %arg3
	std,ma	%arg2, 8(%sr3,%sp)
	std,ma	%arg3, 8(%sr3,%sp)

	mfctl	%pidr1, %arg2
	mfctl	%pidr2, %arg3
	std,ma	%arg2, 8(%sr3,%sp)	/* pidr1 */
	std,ma	%arg3, 8(%sr3,%sp)	/* pidr2 */

	mfctl	%pidr3, %r1
	mtctl	%r1, %pidr1
	mtctl	%r0, %pidr3

	mfctl	%eiem, %arg2
	mfctl	%eirr, %arg3
	std,ma	%arg2, 8(%sr3,%sp)	/* eiem */
	std,ma	%arg3, 8(%sr3,%sp)	/* eirr */

	mfctl	%cr27, %arg2
	mfsp	%sr6, %arg3
	std,ma	%arg2, 8(%sr3,%sp)	/* ior */
	std,ma	%arg3, 8(%sr3,%sp)	/* isr */

	ldo	8(%sp), %sp		/* iir */
	mfctl	%cr31, %arg3
	std,ma	%arg3, 8(%sr3,%sp)	/* ipsw */

	mfctl	%cr24, %arg2
	mfctl	%cr25, %arg3
	std,ma	%arg2, 8(%sr3,%sp)	/* curcpuinfo */
	std,ma	%arg3, 8(%sr3,%sp)	/* vtop */

	mfctl	%cr30, %arg2
	mfctl	%cr27, %arg3		/* XXX */
	std,ma	%arg2, 8(%sr3,%sp)
	std,ma	%arg3, 8(%sr3,%sp)	/* user curthread */

	mfctl	%sar, %arg2
	std,ma	%arg2, 8(%sr3,%sp)	/* sar */

	mtsp	%r0, %sr0
	mtsp	%r0, %sr1
	mtsp	%r0, %sr2
	mtsp	%r0, %sr3
	mtsp	%r0, %sr4
	mtsp	%r0, %sr5
	mtsp	%r0, %sr6
	mtsp	%r0, %sr7

	/* Copy sp and iir from physical trap save area. */
	mfctl	%cr24, %r1
	ldo	CI_TRAP_SAVE(%r1), %r1
	ldd	0*8(%r1), %arg2
	ldd	1*8(%r1), %arg3
	std	%arg2, TF_SP(%arg1)
	std	%arg3, TF_IIR(%arg1)

	ldo	TRAPFRAME_SIZEOF(%arg1), %sp
	copy	%sp, %r3
	std,ma	%r0, HPPA_FRAME_SIZE(%sp)

	ldil	L%__gp, %dp
	ldo	R%__gp(%dp), %dp
	copy	%arg1, %r5

	depd	%r0, T_USER_POS+1, T_USER_POS, %arg0
	.import trap, code
	b,l	trap, %rp
	copy	%arg0, %r4

	/* non-terminal frames return to the same proc */
	bb,*>=,n %r4, TFF_LAST_POS, $syscall_return
	copy	%r5, %arg0

	/* see if curproc have really changed */
	mfctl	%cr24, %arg1
	ldd	CI_CURPROC(%arg1), %r1
	sub,*=	%r0, %r1, %r0
	ldd	P_MD_REGS(%r1), %r5

	b	$syscall_return
	copy	%r5, %arg0
EXIT(TLABEL(all))

#define	PTPULL(bits,lbl)			\
		/* space:offset -- %r8:%r9 */	! \
	ldil	L%__kernel_pagedir, %r24	! \
	or,*=	%r8, %r8, %r0			! \
	mfctl	%cr25, %r24	/* vtop */	! \
	ldo	PAGE_SIZE(%r24), %r25		! \
	extrd,u	%r25, 51, 32, %r25		! \
	extrd,u,*= %r9, 32, 10, %r17		! \
	ldw,s	%r17(%r24), %r25		! \
	extrd,u %r9, 42, 10, %r17		! \
	depd,z,*<> %r25, 51, 32, %r25		! \
	b,n	lbl				! \
	ldw,s	%r17(%r25), %r25		! \
	extrd,u %r9, 51, 9, %r24		! \
	depd,z,*<> %r25, 51, 32, %r25		! \
	b,n	lbl				! \
	ldd,s	%r24(%r25), %r16		! \
	or,*<>	%r16, %r0, %r17			! \
	b,n	lbl				! \
	depdi	bits, 2+bits, 1+bits, %r17	! \
	shladd,l %r24, 3, %r25, %r25		! \
	sub,*=	%r16, %r17, %r0			! \
	std	%r17, 0(%r25)			! \
	extrd,s	%r16, 63, 37, %r16		! \
	depd	%r8, 62, 47, %r17		! \
	depdi	2, 63, 2, %r17

TLABEL(itlb)
	PTPULL(0, TLABEL(all))
	extrd,u,*= %r17, 5, 1, %r0	/* gateway page needs to be public */
	depdi	0, 62, 31, %r17
	iitlbt	%r16, %r17
	rfi,r
	nop

TLABEL(tlbd)
	PTPULL(1, TLABEL(all))
	idtlbt	%r16, %r17
	rfi,r
	nop

TLABEL(itlbna)
TLABEL(dtlbna)
	PTPULL(0, TLABEL(dtlbna_fake))
	idtlbt	%r16, %r17
	rfi,r
	nop
TLABEL(dtlbna_fake)
	/* parse probei?,[rw] insns, decent to trap() to set regs proper */
	mfctl	%iir, %r16
	extrd,u	%r16, 38, 6, %r24
	comib,=,n 1, %r24, TLABEL(all)
	extrd,u	%r16, 56, 6, %r24
	subi,<>	0x23, %r24, %r0
	b	TLABEL(all)
	subi,<>	0x63, %r24, %r0
	b	TLABEL(all)
	/* otherwise generate a flush-only tlb entry */
	depd,z	%r8, 62, 47, %r17
	depdi	-13, 11, 7, %r17
	ldo	2(%r17), %r17	/* 3? */
	idtlbt	%r0, %r17
	rfi,r
	nop

TLABEL(dtlb)
	PTPULL(0, TLABEL(all))
	idtlbt	%r16, %r17
	rfi,r
	nop

	.export TLABEL(excpt), entry
ENTRY(TLABEL(excpt),0)
	/* assume we never get this one w/o fpu [enabled] */
	copy	%rp, %r1
	copy	%arg0, %r8
	mfctl	%cr30, %r9
#if FP_REGS != 0
	ldo	HFP_REGS(%r9), %r9
#endif
	.import	fpu_save, code
	.call
	b,l	fpu_save, %rp
	copy	%r9, %arg0
	copy	%r1, %rp
	copy	%r8, %arg0
	mtctl	%r0, %cr10	/* reset ccr: cause a reload after exception */
	ldil	L%fpu_curpcb, %r1
	std	%r0, R%fpu_curpcb(%r1)

	/* now, check for trap */
	ldw	0(%r9), %r1
	bb,>=,n	%r1, HPPA_FPU_T_POS, excpt_notrap
	ldw	1*4(%r9), %r1
	comb,<>,n %r0, %r1, excpt_emulate
	ldw	2*4(%r9), %r1
	comb,<>,n %r0, %r1, excpt_emulate
	ldw	3*4(%r9), %r1
	comb,<>,n %r0, %r1, excpt_emulate
	ldw	4*4(%r9), %r1
	comb,<>,n %r0, %r1, excpt_emulate
	ldw	5*4(%r9), %r1
	comb,<>,n %r0, %r1, excpt_emulate
	ldw	6*4(%r9), %r1
	comb,<>,n %r0, %r1, excpt_emulate
	ldw	7*4(%r9), %r1

excpt_emulate
	bb,*>=,n %r1, 37, excpt_notrap	/* HPPA_FPU_UNMPL not set */

#if TODO_emulate
	ldw	0(%r9), %r16
	depi	0, HPPA_FPU_T_POS, 1, %r16
	.import	$fpu_emulate, code
	b	$fpu_emulate
	stw	%r16, 0(%r9)
#endif

excpt_notrap
	sync
	b	TLABEL(all)
	ldi	T_EXCEPTION, %r1
EXIT(TLABEL(excpt))

	.export TLABEL(emu), entry
ENTRY(TLABEL(emu),0)
	/*
	 * Switch FPU/SFU context
	 *
	 * %isr:%ior - data address
	 * %iir - instruction to emulate
	 * iisq:iioq - address of instruction to emulate
	 *
	 * note: ISR and IOR contain valid data only if the
	 *	 instruction is a coprocessor load or store.
	 *
	 */

	mfctl	%iir, %r8
	extrd,u	%r8, 37, 6, %r9	/* no sfu implementation right now */
	comib,=	4, %r9, TLABEL(all)
	ldi	T_ILLEGAL, %r1

	/*
	 * pass through for all coprocessors now and
	 * do not check the uid here.
	 * in case that piece does not exist emulate
	 * or the trap will be generted later.
	 */

	/* if we are already enabled and hit again, emulate */
	mfctl	%cr10, %r1	/* ccr */
	extru,<> %r1, 25, 2, %r0
	b,n	$fpusw_set
	nop

$fpusw_emu
	mtctl	%r0, %cr10	/* reset ccr: cause a reload after exception */
	std	%r0, R%fpu_curpcb(%r1)
#if TODO_emulate
	/* here we emulate the fld/fst */
	mfctl	%iir, %r1
	extrd,u	%r1, 37, 6, %r1
	comib,=	0xb, %r9, TLABEL(all)
	ldi	T_ILLEGAL, %r1

	mfctl	%iir, %r1
	extrd,u	%r1, 37, 6, %r1
	comib,=	0x9, %r9, TLABEL(all)
	ldi	T_ILLEGAL, %r1

	mfctl	%iir, %r1
	.import	$fpu_emulate, code
	b	$fpu_emulate
	nop
#else
	b	TLABEL(all)
	ldi	T_ILLEGAL, %r1
#endif
$fpusw_set
	/* enable coprocessor */
	depdi	3, 37, 2, %r1
	mtctl	%r1, %cr10	/* ccr */

	mfctl	%cr24, %r16
	mfctl	%cr30, %r9
	ldd	CI_FPU_STATE(%r16), %r16

	comb,=,n %r16, %r0, $fpusw_nosave
	comb,=,n %r16, %r9, $fpusw_done

	copy	%arg0, %r17
	copy	%rp, %r1
#if HFP_REGS != 0
	ldo	HFP_REGS(%r16), %r16
#endif
	.import	fpu_save, code
	.call
	b,l	fpu_save, %rp
	copy	%r16, %arg0
	copy	%r1, %rp
	copy	%r17, %arg0

$fpusw_nosave
	/* count switches */
	.import	umvexp, data
	ldil	L%(uvmexp+FPSWTCH), %r1
	ldw	R%(uvmexp+FPSWTCH)(%r1), %r16
	ldo	31*8+HFP_REGS(%r9), %r17
	ldo	1(%r16), %r16
	stw	%r16, R%(uvmexp+FPSWTCH)(%r1)

	fldds,ma -8(%r17), %fr31
	fldds,ma -8(%r17), %fr30
	fldds,ma -8(%r17), %fr29
	fldds,ma -8(%r17), %fr28
	fldds,ma -8(%r17), %fr27
	fldds,ma -8(%r17), %fr26
	fldds,ma -8(%r17), %fr25
	fldds,ma -8(%r17), %fr24
	fldds,ma -8(%r17), %fr23
	fldds,ma -8(%r17), %fr22
	fldds,ma -8(%r17), %fr21
	fldds,ma -8(%r17), %fr20
	fldds,ma -8(%r17), %fr19
	fldds,ma -8(%r17), %fr18
	fldds,ma -8(%r17), %fr17
	fldds,ma -8(%r17), %fr16
	fldds,ma -8(%r17), %fr15
	fldds,ma -8(%r17), %fr14
	fldds,ma -8(%r17), %fr13
	fldds,ma -8(%r17), %fr12
	fldds,ma -8(%r17), %fr11
	fldds,ma -8(%r17), %fr10
	fldds,ma -8(%r17), %fr9
	fldds,ma -8(%r17), %fr8
	fldds,ma -8(%r17), %fr7
	fldds,ma -8(%r17), %fr6
	fldds,ma -8(%r17), %fr5
	fldds,ma -8(%r17), %fr4
	fldds,ma -8(%r17), %fr3
	fldds,ma -8(%r17), %fr2
	fldds,ma -8(%r17), %fr1
	fldds     0(%r17), %fr0	/* fr0 must be restored last */

	mfctl	%cr24, %r1
	std	%r9, CI_FPU_STATE(%r1)
	std	%r1, HFP_CPU(%r9)
	sync

$fpusw_done
	rfir
	nop
EXIT(TLABEL(emu))

LEAF_ENTRY(fpu_exit)
	/* enable coprocessor XXX */
	depi    3, 25, 2, %r1
	mtctl   %r1, %cr10

	ldil    L%fpu_scratch, %r25
	ldo     R%fpu_scratch(%r25), %r25
	fstds   %fr0, 0(%r25)
	sync
	bv      %r0(%rp)
	mtctl   %r0, %cr10
EXIT(fpu_exit)

LEAF_ENTRY(fpu_save)
	fstds,ma %fr0 , 8(%arg0)
	fstds,ma %fr1 , 8(%arg0)
	fstds,ma %fr2 , 8(%arg0)
	fstds,ma %fr3 , 8(%arg0)
	fstds,ma %fr4 , 8(%arg0)
	fstds,ma %fr5 , 8(%arg0)
	fstds,ma %fr6 , 8(%arg0)
	fstds,ma %fr7 , 8(%arg0)
	fstds,ma %fr8 , 8(%arg0)
	fstds,ma %fr9 , 8(%arg0)
	fstds,ma %fr10, 8(%arg0)
	fstds,ma %fr11, 8(%arg0)
	fstds,ma %fr12, 8(%arg0)
	fstds,ma %fr13, 8(%arg0)
	fstds,ma %fr14, 8(%arg0)
	fstds,ma %fr15, 8(%arg0)
	fstds,ma %fr16, 8(%arg0)
	fstds,ma %fr17, 8(%arg0)
	fstds,ma %fr18, 8(%arg0)
	fstds,ma %fr19, 8(%arg0)
	fstds,ma %fr20, 8(%arg0)
	fstds,ma %fr21, 8(%arg0)
	fstds,ma %fr22, 8(%arg0)
	fstds,ma %fr23, 8(%arg0)
	fstds,ma %fr24, 8(%arg0)
	fstds,ma %fr25, 8(%arg0)
	fstds,ma %fr26, 8(%arg0)
	fstds,ma %fr27, 8(%arg0)
	fstds,ma %fr28, 8(%arg0)
	fstds,ma %fr29, 8(%arg0)
	fstds,ma %fr30, 8(%arg0)
	fstds    %fr31, 0(%arg0)
	ldo	-24(%arg0), %arg0
	ldi	-32, %r25	/* gotta be free for all callers */
	fdc,m	%r25(%arg0)
	fdc,m	%r25(%arg0)
	fdc,m	%r25(%arg0)
	fdc,m	%r25(%arg0)
	fdc,m	%r25(%arg0)
	fdc,m	%r25(%arg0)
	fdc,m	%r25(%arg0)
	fdc,m	%r25(%arg0)
	bv	%r0(%rp)
	sync
EXIT(fpu_save)

/*
 * void
 * mtctl(register_t val, int reg)
 */
LEAF_ENTRY(mtctl)
	b,l	mtctl_end, %r1
	extrd,u	%arg1, 63, 32, %arg1
	bv	%r0(%rp)
	mtctl	%arg0, %cr0
	bv	%r0(%rp)
	nop	/* mtctl	%arg0, %cr1 */
	bv	%r0(%rp)
	nop	/* mtctl	%arg0, %cr2 */
	bv	%r0(%rp)
	nop	/* mtctl	%arg0, %cr3 */
	bv	%r0(%rp)
	nop	/* mtctl	%arg0, %cr4 */
	bv	%r0(%rp)
	nop	/* mtctl	%arg0, %cr5 */
	bv	%r0(%rp)
	nop	/* mtctl	%arg0, %cr6 */
	bv	%r0(%rp)
	nop	/* mtctl	%arg0, %cr7 */
	bv	%r0(%rp)
	mtctl	%arg0, %cr8
	bv	%r0(%rp)
	mtctl	%arg0, %cr9
	bv	%r0(%rp)
	mtctl	%arg0, %cr10
	bv	%r0(%rp)
	mtctl	%arg0, %cr11
	bv	%r0(%rp)
	mtctl	%arg0, %cr12
	bv	%r0(%rp)
	mtctl	%arg0, %cr13
	bv	%r0(%rp)
	mtctl	%arg0, %cr14
	bv	%r0(%rp)
	mtctl	%arg0, %cr15
	bv	%r0(%rp)
	mtctl	%arg0, %cr16
	bv	%r0(%rp)
	mtctl	%arg0, %cr17
	bv	%r0(%rp)
	mtctl	%arg0, %cr18
	bv	%r0(%rp)
	mtctl	%arg0, %cr19
	bv	%r0(%rp)
	mtctl	%arg0, %cr20
	bv	%r0(%rp)
	mtctl	%arg0, %cr21
	bv	%r0(%rp)
	mtctl	%arg0, %cr22
	bv	%r0(%rp)
	mtctl	%arg0, %cr23
	bv	%r0(%rp)
	mtctl	%arg0, %cr24
	bv	%r0(%rp)
	mtctl	%arg0, %cr25
	bv	%r0(%rp)
	mtctl	%arg0, %cr26
	bv	%r0(%rp)
	mtctl	%arg0, %cr27
	bv	%r0(%rp)
	mtctl	%arg0, %cr28
	bv	%r0(%rp)
	mtctl	%arg0, %cr29
	bv	%r0(%rp)
	mtctl	%arg0, %cr30
	bv	%r0(%rp)
	mtctl	%arg0, %cr31
mtctl_end
	subi,<< 31, %arg1, %r0
	bv,n	%arg1(%r1)
	bv	%r0(%rp)
	nop
EXIT(mtctl)

/*
 * void  
 * pdtlb(pa_space_t sp, vaddr_t va)
 */
LEAF_ENTRY(pdtlb)
	mfsp	%sr1, %arg2
	mtsp	%arg0, %sr1
	pdtlb	%r0(%sr1, %arg1)
	bv	%r0(%rp)
	mtsp	%arg2, %sr1
EXIT(pdtlb)

/*
 * void  
 * pitlb(pa_space_t sp, vaddr_t va)
 */
LEAF_ENTRY(pitlb)
	mfsp	%sr1, %arg2
	mtsp	%arg0, %sr1
	pitlb	%r0(%sr1, %arg1)
	bv	%r0(%rp)
	mtsp	%arg2, %sr1
EXIT(pitlb)

/*
 * register_t
 * mfctl(int reg)
 */
LEAF_ENTRY(mfctl)
	b,l	mfctl_end, %r1
	extrd,u	%arg0, 63, 32, %arg0
	bv	%r0(%rp)
	mfctl	%cr0, %ret0
	bv	%r0(%rp)
	nop	/* mfctl	%cr1, %ret0 */
	bv	%r0(%rp)
	nop	/* mfctl	%cr2, %ret0 */
	bv	%r0(%rp)
	nop	/* mfctl	%cr3, %ret0 */
	bv	%r0(%rp)
	nop	/* mfctl	%cr4, %ret0 */
	bv	%r0(%rp)
	nop	/* mfctl	%cr5, %ret0 */
	bv	%r0(%rp)
	nop	/* mfctl	%cr6, %ret0 */
	bv	%r0(%rp)
	nop	/* mfctl	%cr7, %ret0 */
	bv	%r0(%rp)
	mfctl	%cr8, %ret0
	bv	%r0(%rp)
	mfctl	%cr9, %ret0
	bv	%r0(%rp)
	mfctl	%cr10, %ret0
	bv	%r0(%rp)
	mfctl	%cr11, %ret0
	bv	%r0(%rp)
	mfctl	%cr12, %ret0
	bv	%r0(%rp)
	mfctl	%cr13, %ret0
	bv	%r0(%rp)
	mfctl	%cr14, %ret0
	bv	%r0(%rp)
	mfctl	%cr15, %ret0
	bv	%r0(%rp)
	mfctl	%cr16, %ret0
	bv	%r0(%rp)
	mfctl	%cr17, %ret0
	bv	%r0(%rp)
	mfctl	%cr18, %ret0
	bv	%r0(%rp)
	mfctl	%cr19, %ret0
	bv	%r0(%rp)
	mfctl	%cr20, %ret0
	bv	%r0(%rp)
	mfctl	%cr21, %ret0
	bv	%r0(%rp)
	mfctl	%cr22, %ret0
	bv	%r0(%rp)
	mfctl	%cr23, %ret0
	bv	%r0(%rp)
	mfctl	%cr24, %ret0
	bv	%r0(%rp)
	mfctl	%cr25, %ret0
	bv	%r0(%rp)
	mfctl	%cr26, %ret0
	bv	%r0(%rp)
	mfctl	%cr27, %ret0
	bv	%r0(%rp)
	mfctl	%cr28, %ret0
	bv	%r0(%rp)
	mfctl	%cr29, %ret0
	bv	%r30(%rp)
	mfctl	%cr30, %ret0
	bv	%r30(%rp)
	mfctl	%cr31, %ret0

mfctl_end
	subi,<< 31, %arg0, %r0
	bv,n	%arg0(%r1)
	bv	%r0(%rp)
	nop
EXIT(mfctl)

#define	CACHELOOP(sp,addr,len,step,insn)			\
	mtsp	sp, %sr1					! \
	ldi	-1, arg7					! \
	sh3add	step, arg7, %r1					! \
	andcm	len, %r1, %r1					! \
	add	addr, %r1, %r1					! \
	b	__CONCAT(insn,_loop)				! \
	add	addr, len, len					! \
	.label	__CONCAT(insn,_loop8)				! \
	insn,m	step(%sr1, addr)				! \
	insn,m	step(%sr1, addr)				! \
	insn,m	step(%sr1, addr)				! \
	insn,m	step(%sr1, addr)				! \
	insn,m	step(%sr1, addr)				! \
	insn,m	step(%sr1, addr)				! \
	insn,m	step(%sr1, addr)				! \
	.label	__CONCAT(insn,_loop)				! \
	comb,<<	addr, %r1, __CONCAT(insn,_loop8)		! \
	insn,m	step(%sr1, addr)				! \
	.label	__CONCAT(insn,_loop1)				! \
	comb,<<,n addr, len, __CONCAT(insn,_loop1)		! \
	insn,m	step(%sr1, addr)


LEAF_ENTRY(fdcache)
	ldil	L%dcache_stride, %r1
	ldw	R%dcache_stride(%r1), %arg3
	CACHELOOP(%arg0,%arg1,%arg2,%arg3,fdc)
	bv	%r0(%rp)
	sync
EXIT(fdcache)

LEAF_ENTRY(pdcache)
	ldil	L%dcache_stride, %r1
	ldw	R%dcache_stride(%r1), %arg3
	CACHELOOP(%arg0,%arg1,%arg2,%arg3,pdc)
	bv	%r0(%rp)
	sync
EXIT(pdcache)

LEAF_ENTRY(ficache)
	ldil	L%icache_stride, %r1
	ldw	R%icache_stride(%r1), %arg3
	CACHELOOP(%arg0,%arg1,%arg2,%arg3,fic)
	bv	%r0(%rp)
	sync
EXIT(ficache)

LEAF_ENTRY(copy_on_fault)
	mtsp	%r0, %sr1
	mtsp	%r0, %sr2
	std	%r1, PCB_ONFAULT+U_PCB(%rp)
	ldo	-HPPA_FRAME_SIZE(%sp), %sp
	ldd	HPPA_FRAME_RP(%sp), %rp
	bv	%r0(%rp)
	ldi	EFAULT, %ret0
EXIT(copy_on_fault)

LEAF_ENTRY(sync_caches)
	sync
	syncdma
	sync
	nop
	nop
	nop
	nop
	nop
	nop
	bv	%r0(%rp)
	nop
EXIT(sync_caches)

/*
 * int spstrcpy (pa_space_t ssp, const void *src, pa_space_t dsp, void *dst,
 *		 size_t size, size_t *rsize)
 * do a space to space strncpy, return actual copy size in the rsize;
 */
LEAF_ENTRY(spstrcpy)
	std	%rp, HPPA_FRAME_RP(%sp)
	ldo	HPPA_FRAME_SIZE(%sp), %sp
	add	arg4, %arg1, arg4
	/* setup fault handler */
	mfctl	%cr24, %r1
	ldd	CI_CURPROC(%r1), %r1
	ldil	L%copy_on_fault, arg7
	ldd	P_ADDR(%r1), %rp
	ldo	R%copy_on_fault(arg7), arg7
	ldd	PCB_ONFAULT+U_PCB(%rp), %r1
	std	arg7, PCB_ONFAULT+U_PCB(%rp)

	mtsp	%arg0, %sr1
	mtsp	%arg2, %sr2
	copy	%arg1, %arg0
	copy	%r0, %ret0

$spstrcpy_loop
	ldbs,ma	1(%sr1, %arg1), arg7
	comb,=	%r0, arg7, $spstrcpy_exit
	stbs,ma	arg7, 1(%sr2, %arg3)
	comb,<>,n arg4, %arg1, $spstrcpy_loop
	nop
	ldi	ENAMETOOLONG, %ret0

$spstrcpy_exit
	ldo	-HPPA_FRAME_SIZE(%sp), %sp
	mtsp	%r0, %sr1
	mtsp	%r0, %sr2
	std	%r1, PCB_ONFAULT+U_PCB(%rp)
	sub	%arg1, %arg0, %arg1
	ldd	HPPA_FRAME_RP(%sp), %rp
	sub,=	%r0, arg5, %r0
	std	%arg1, 0(arg5)
	bv	0(%rp)
	nop
EXIT(spstrcpy)

LEAF_ENTRY(setjmp)
	std,ma	%rp, 8(%arg0)
	std,ma	%sp, 8(%arg0)
	std,ma	%r3, 8(%arg0)
	std,ma	%r4, 8(%arg0)
	std,ma	%r5, 8(%arg0)
	std,ma	%r6, 8(%arg0)
	std,ma	%r7, 8(%arg0)
	std,ma	%r8, 8(%arg0)
	std,ma	%r9, 8(%arg0)
	std,ma	%r10, 8(%arg0)
	std,ma	%r11, 8(%arg0)
	std,ma	%r12, 8(%arg0)
	std,ma	%r13, 8(%arg0)
	std,ma	%r14, 8(%arg0)
	std,ma	%r15, 8(%arg0)
	std,ma	%r16, 8(%arg0)
	std,ma	%r17, 8(%arg0)
	std,ma	%r18, 8(%arg0)
	std,ma	%r19, 8(%arg0)
	std,ma	%r27, 8(%arg0)

	bv	%r0(%rp)
	copy	%r0, %ret0
EXIT(setjmp)

LEAF_ENTRY(longjmp)
	ldd,ma	8(%arg0), %rp
	ldd,ma	8(%arg0), %sp
	ldd,ma	8(%arg0), %r3
	ldd,ma	8(%arg0), %r4
	ldd,ma	8(%arg0), %r5
	ldd,ma	8(%arg0), %r6
	ldd,ma	8(%arg0), %r7
	ldd,ma	8(%arg0), %r8
	ldd,ma	8(%arg0), %r9
	ldd,ma	8(%arg0), %r10
	ldd,ma	8(%arg0), %r11
	ldd,ma	8(%arg0), %r12
	ldd,ma	8(%arg0), %r13
	ldd,ma	8(%arg0), %r14
	ldd,ma	8(%arg0), %r15
	ldd,ma	8(%arg0), %r16
	ldd,ma	8(%arg0), %r17
	ldd,ma	8(%arg0), %r18
	ldd,ma	8(%arg0), %r19
	ldd,ma	8(%arg0), %r27

	bv	%r0(%rp)
	ldi	1, %ret0
EXIT(longjmp)

/*
 * int cpu_switchto(struct proc *old, struct proc *new)
 * Switch from "old" proc to "new".
 */
	.align	32
ENTRY(cpu_switchto,128)
	copy	%r3, %r1
	std	%rp, HPPA_FRAME_RP(%sp)
	copy	%sp, %r3
	std,ma	%r1, HPPA_FRAME_SIZE+20*8(%sp)

	ldo	-(HPPA_FRAME_SIZE)(%sp), ap

#ifdef DIAGNOSTIC
	b	kstack_check
	nop
switch_error
	copy	%arg1, %arg2
	copy	%arg0, %arg1
	ldil	L%Lcspstr, %arg0
	.call
	b,l	panic, %rp
	ldo	R%Lcspstr(%arg0), %arg0
Lcspstr
	.asciz	"cpu_switchto: old=%p, new=%p"
	.align	8
kstack_check
	ldd	P_WCHAN(%arg1), %arg3
	comb,<>,n %r0, %arg3, switch_error
	nop
	ldb	P_STAT(%arg1), %arg3
	comib,<>,n SRUN, %arg3, switch_error
	nop
	/*
	 * The new process' kernel stack must be reasonable.
	 */
	ldd     P_ADDR(%arg1), %arg2
	ldd	U_PCB+PCB_KSP(%arg2), %arg3
	ldo     NBPG(%arg2), %arg2
	comb,>>,n %arg2, %arg3, switch_error
	nop
	sub     %arg3, %arg2, %arg3
	ldil    L%USPACE, %arg2
	ldo     R%USPACE(%arg2), %arg2
	comb,<<=,n %arg2, %arg3, switch_error
	nop
kstack_ok
#endif

	/* Record new proc. */
	ldi	SONPROC, %r1
	stb	%r1, P_STAT(%arg1)
	mfctl	%cr24, %r1
	std	%arg1, CI_CURPROC(%r1)

	/* If old process exited, don't bother. */
	comb,=,n %r0, %arg2, switch_exited

	/*
	 * 2. save old proc context
	 *
	 * arg2: old proc
	 */
	ldd	P_ADDR(%arg0), %r1
	/* save callee-save registers */
	std	%r4,   1*8(%r3)
	std	%sp, U_PCB+PCB_KSP(%r1)
	std	%r5,   2*8(%r3)
	std	%r6,   3*8(%r3)
	std	%r7,   4*8(%r3)
	std	%r8,   5*8(%r3)
	std	%r9,   6*8(%r3)
	std	%r10,  7*8(%r3)
	std	%r11,  8*8(%r3)
	std	%r12,  9*8(%r3)
	std	%r13, 10*8(%r3)
	std	%r14, 11*8(%r3)
	std	%r15, 12*8(%r3)
	std	%r16, 13*8(%r3)
	std	%r17, 14*8(%r3)
	std	%r18, 15*8(%r3)
	fdc	%r0(%r1)
	std	%r0, 1*8(ap)	/* say no trampoline */
	sync

	/* don't need old curproc (arg2) starting from here */
switch_exited
	/*
	 * 3. restore new proc context
	 *
	 * arg1: new proc
	 */
	ldd	P_ADDR(%arg1), %arg3
	ldd	P_MD_REGS(%arg1), %r1
	ldd	U_PCB+PCB_KSP(%arg3), %sp
	mtctl	%r0, %cr10		/* ccr */
	ldd	TF_CR30(%r1), %arg2
	ldd	TF_PIDR2(%r1), %arg3
	mtctl	%arg2, %cr30
	mtctl	%arg3, %pidr2
	ldd	TF_VTOP(%r1), %arg2
	mtctl	%arg2, %cr25
	ldo	-(HPPA_FRAME_SIZE+20*8)(%sp), %r3
	ldo	-(HPPA_FRAME_SIZE)(%sp), ap
	ldd	0*8(ap), %arg0
	ldd	1*8(ap), %arg3		/* in case we're on trampoline */
	sub,*=	%r0, %arg3, %r0
	b,n	switch_return
	ldd	 1*8(%r3), %r4
	ldd	 2*8(%r3), %r5
	ldd	 3*8(%r3), %r6
	ldd	 4*8(%r3), %r7
	ldd	 5*8(%r3), %r8
	ldd	 6*8(%r3), %r9
	ldd	 7*8(%r3), %r10
	ldd	 8*8(%r3), %r11
	ldd	 9*8(%r3), %r12
	ldd	10*8(%r3), %r13
	ldd	11*8(%r3), %r14
	ldd	12*8(%r3), %r15
	ldd	13*8(%r3), %r16
	ldd	14*8(%r3), %r17
	ldd	15*8(%r3), %r18

switch_return
	mfctl	%cr24, %r1
	std	%arg1, CI_CURPROC(%r1)
	ldd	HPPA_FRAME_RP(%r3), %rp
	bv	0(%rp)
	ldd,mb	-(HPPA_FRAME_SIZE+20*8)(%sp), %r3
EXIT(cpu_switchto)

LEAF_ENTRY(cpu_idle_enter)
	bv	%r0(%rp)
	nop
EXIT(cpu_idle_enter)

LEAF_ENTRY(cpu_idle_cycle)
	bv	%r0(%rp)
	nop
EXIT(cpu_idle_cycle)

LEAF_ENTRY(cpu_idle_leave)
	bv	%r0(%rp)
	nop
EXIT(cpu_idle_leave)

	.align	8
	.export	switch_tramp_p, code
switch_tramp_p
	.dword	switch_trampoline

ENTRY(switch_trampoline,0)
	mfctl	%cr24, %r1
	stw	%r0, CI_CPL(%r1)

	/* ldd	0x18(%arg3), %r19	but we know we are in kernel! */
	ldd	0x10(%arg3), %arg3
	.call
	ble	0(%sr0, %arg3)
	copy	%r31, %rp

	mfctl	%cr24, %arg1
	ldd	CI_CURPROC(%arg1), %r1
	b	$syscall_return
	ldd	P_MD_REGS(%r1), %arg0
EXIT(switch_trampoline)

/*
 * Signal "trampoline" code. Invoked from RTE setup by sendsig().
 */
ENTRY(sigcode,0)
	ldd	0x18(%arg3), %r19
	ldd	0x10(%arg3), %arg3
	.call
	ble	0(%sr0, %arg3)
	copy	%r31, %rp

	ldil	L%SYSCALLGATE, %r1
	copy	%r4, %arg0
	.call
	ble	4(%sr7, %r1)
	ldi	SYS_sigreturn, %r1

	ldil	L%SYSCALLGATE, %r1
	copy	%ret0, %arg0
	.call
	ble	4(%sr7, %r1)
	ldi	SYS_exit, %r1
ALTENTRY(esigcode)
EXIT(sigcode)

/* 
 * struct cpu_info *curcpu(void);
 */
LEAF_ENTRY(curcpu)
	bv	%r0(%rp)
	mfctl	%cr24, %ret0
EXIT(curcpu)

/*
 * int splraise(int newcpl);
 */
LEAF_ENTRY(splraise)
	extrd,u	%arg0, 63, 32, %arg0
#ifdef DEBUG
	cmpib,>,n NIPL, %arg0, splraise_ok
	copy	%arg0, %arg1
	ldil	L%splraise_fmt, %arg0
	b,l	panic, %rp
	ldo	R%splraise_fmt(%arg0), %arg0
	bv	%r0(%rp)
	ldi	IPL_HIGH, %ret0
splraise_fmt
	.asciz	"splraise(%d)"
	.align	8
splraise_ok
#endif
	sync
	mfctl	%eiem, %arg2
	mtctl	%r0, %eiem
	mfctl	%cr24, %r1
	ldw	CI_CPL(%r1), %ret0
	ldil	L%imask, %arg3
	ldo	R%imask(%arg3), %arg3
	sub,<=	%arg0, %ret0, %r0
	ldd,s	%arg0(%arg3), %arg2
	sub,<=	%arg0, %ret0, %r0
	stw	%arg0, CI_CPL(%r1)
	sync
	bv	%r0(%rp)
	mtctl	%arg2, %eiem
EXIT(splraise)

/*
 * int spllower(int newcpl);
 */
LEAF_ENTRY(spllower)
	extrd,u	%arg0, 63, 32, %arg0
#ifdef DEBUG
	cmpib,>,n NIPL, %arg0, spllower_ok
	copy	%arg0, %arg1
	ldil	L%spllower_fmt, %arg0
	b,l	panic, %rp
	ldo	R%spllower_fmt(%arg0), %arg0
	bv	%r0(%rp)
	ldi	IPL_HIGH, %ret0
spllower_fmt
	.asciz	"spllower(%d)"
	.align	8
spllower_ok
#endif
	sync
	mfctl	%eiem, %arg2
	mtctl	%r0, %eiem
	mfctl	%cr24, %r1
	ldw	CI_CPL(%r1), %ret0
	ldil	L%imask, %arg3
	ldo	R%imask(%arg3), %arg3
	sub,>=	%arg0, %ret0, %r0
	ldd,s	%arg0(%arg3), %arg2
	sub,>=	%arg0, %ret0, %r0
	stw	%arg0, CI_CPL(%r1)
	sync
	bv	%r0(%rp)
	mtctl	%arg2, %eiem
EXIT(spllower)

	.end
