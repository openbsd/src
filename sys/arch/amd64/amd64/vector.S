/*	$OpenBSD: vector.S,v 1.43 2015/07/17 15:37:58 guenther Exp $	*/
/*	$NetBSD: vector.S,v 1.5 2004/06/28 09:13:11 fvdl Exp $	*/

/*
 * Copyright (c) 2001 Wasabi Systems, Inc.
 * All rights reserved.
 *
 * Written by Frank van der Linden for Wasabi Systems, Inc.
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
 *      This product includes software developed for the NetBSD Project by
 *      Wasabi Systems, Inc.
 * 4. The name of Wasabi Systems, Inc. may not be used to endorse
 *    or promote products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY WASABI SYSTEMS, INC. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL WASABI SYSTEMS, INC
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#define ALIGN_TEXT	.align 16,0x90

#include <machine/param.h>
#include <machine/i8259.h>
#include <machine/i82093reg.h>
#include <machine/i82489reg.h>
#include <machine/asm.h>
#include <machine/frameasm.h>
#include <machine/segments.h>
#include <machine/trap.h>
#include <machine/intr.h>
#include <machine/psl.h>
#include <machine/codepatch.h>
#include <machine/specialreg.h>

#include "ioapic.h"
#include "lapic.h"
#include "assym.h"

/*****************************************************************************/

/*
 * Trap and fault vector routines
 *
 * On exit from the kernel to user mode, we always need to check for ASTs.  In
 * addition, we need to do this atomically; otherwise an interrupt may occur
 * which causes an AST, but it won't get processed until the next kernel entry
 * (possibly the next clock tick).  Thus, we disable interrupt before checking,
 * and only enable them again on the final `iret' or before calling the AST
 * handler.
 */ 

/*****************************************************************************/

#define	TRAP(a)		pushq $(a) ; jmp _C_LABEL(alltraps)
#define	ZTRAP(a)	pushq $0 ; TRAP(a)

#define	BPTTRAP(a)	ZTRAP(a)

	.text
IDTVEC(trap00)
	ZTRAP(T_DIVIDE)
IDTVEC(trap01)
	BPTTRAP(T_TRCTRAP)
IDTVEC(trap02)
	ZTRAP(T_NMI)
IDTVEC(trap03)
	BPTTRAP(T_BPTFLT)
IDTVEC(trap04)
	ZTRAP(T_OFLOW)
IDTVEC(trap05)
	ZTRAP(T_BOUND)
IDTVEC(trap06)
	ZTRAP(T_PRIVINFLT)
IDTVEC(trap07)
	pushq	$0			# dummy error code
	pushq	$T_DNA
	INTRENTRY
	sti
	cld
	movq	CPUVAR(SELF),%rdi
	call	_C_LABEL(fpudna)
	INTRFASTEXIT
IDTVEC(trap08)
	TRAP(T_DOUBLEFLT)
IDTVEC(trap09)
	ZTRAP(T_FPOPFLT)
IDTVEC(trap0a)
	TRAP(T_TSSFLT)
IDTVEC(trap0b)
	TRAP(T_SEGNPFLT)
IDTVEC(trap0c)
	TRAP(T_STKFLT)

	/*
	 * If iretq faults, we'll get a trap at doreti_iret with CPL==0 but
	 * the user's GS.base, which INTRENTRY wouldn't handle correctly
	 * (it would skip the swapgs), so locally expand both it and
	 * INTR_SAVE_GPRS, but add an extra test comparing %rip to doreti_iret
	 * so that we can do the necessary swapgs in that case.
	 */
IDTVEC(trap0d)
	subq	$TF_ERR,%rsp
	movl	$T_PROTFLT,TF_TRAPNO(%rsp)
	movq	%rdi,TF_RDI(%rsp)
	leaq	_C_LABEL(doreti_iret)(%rip),%rdi
	cmpq	%rdi,TF_RIP(%rsp)
	je	1f
	testq	$SEL_RPL,TF_CS(%rsp)
	jz	2f
1:	swapgs
2:	movq	%r15,TF_R15(%rsp)
	movq	%r14,TF_R14(%rsp)
	movq	%r13,TF_R13(%rsp)
	movq	%r12,TF_R12(%rsp)
	movq	%r11,TF_R11(%rsp)
	movq	%r10,TF_R10(%rsp)
	movq	%r9,TF_R9(%rsp)
	movq	%r8,TF_R8(%rsp)
	/*movq	%rdi,TF_RDI(%rsp)	done above */
	movq	%rsi,TF_RSI(%rsp)
	movq	%rbp,TF_RBP(%rsp)
	movq	%rbx,TF_RBX(%rsp)
	movq	%rdx,TF_RDX(%rsp)
	movq	%rcx,TF_RCX(%rsp)
	movq	%rax,TF_RAX(%rsp)
	sti
	jmp	calltrap

IDTVEC(trap0e)
	TRAP(T_PAGEFLT)
IDTVEC(intrspurious)
IDTVEC(trap0f)
	iretq
IDTVEC(trap10)
	ZTRAP(T_ARITHTRAP)
IDTVEC(trap11)
	ZTRAP(T_ALIGNFLT)
IDTVEC(trap12)
	ZTRAP(T_MCA)
IDTVEC(trap13)
	ZTRAP(T_XMM)
IDTVEC(trap14)
IDTVEC(trap15)
IDTVEC(trap16)
IDTVEC(trap17)
IDTVEC(trap18)
IDTVEC(trap19)
IDTVEC(trap1a)
IDTVEC(trap1b)
IDTVEC(trap1c)
IDTVEC(trap1d)
IDTVEC(trap1e)
IDTVEC(trap1f)
	/* 20 - 31 reserved for future exp */
	ZTRAP(T_RESERVED)

IDTVEC(exceptions)
	.quad	_C_LABEL(Xtrap00), _C_LABEL(Xtrap01)
	.quad	_C_LABEL(Xtrap02), _C_LABEL(Xtrap03)
	.quad	_C_LABEL(Xtrap04), _C_LABEL(Xtrap05)
	.quad	_C_LABEL(Xtrap06), _C_LABEL(Xtrap07)
	.quad	_C_LABEL(Xtrap08), _C_LABEL(Xtrap09)
	.quad	_C_LABEL(Xtrap0a), _C_LABEL(Xtrap0b)
	.quad	_C_LABEL(Xtrap0c), _C_LABEL(Xtrap0d)
	.quad	_C_LABEL(Xtrap0e), _C_LABEL(Xtrap0f)
	.quad	_C_LABEL(Xtrap10), _C_LABEL(Xtrap11)
	.quad	_C_LABEL(Xtrap12), _C_LABEL(Xtrap13)
	.quad	_C_LABEL(Xtrap14), _C_LABEL(Xtrap15)
	.quad	_C_LABEL(Xtrap16), _C_LABEL(Xtrap17)
	.quad	_C_LABEL(Xtrap18), _C_LABEL(Xtrap19)
	.quad	_C_LABEL(Xtrap1a), _C_LABEL(Xtrap1b)
	.quad	_C_LABEL(Xtrap1c), _C_LABEL(Xtrap1d)
	.quad	_C_LABEL(Xtrap1e), _C_LABEL(Xtrap1f)

/*
 * If an error is detected during trap, syscall, or interrupt exit, trap() will
 * change %rip to point to this label.  At that point, we'll be running with
 * the kernel GS.base, but the trap frame will be from CPL==3, so we can't
 * go through INTRENTRY as it would do the swapgs that we don't want/need.
 * So, locally expand INTRENTRY but without the swapgs: manually
 * clean up the stack and resume as if we were handling a general
 * protection fault.  This will cause the process to get a SIGBUS.
 */
NENTRY(resume_iret)
	pushq	$0
	pushq	$T_PROTFLT
	subq	$32,%rsp
	INTR_SAVE_GPRS
	sti
	jmp	calltrap

/*
 * All traps go through here. Call the generic trap handler, and
 * check for ASTs afterwards.
 */
NENTRY(alltraps)
	INTRENTRY
	sti
	.globl	calltrap
calltrap:
	cld
#ifdef DIAGNOSTIC
	movl	CPUVAR(ILEVEL),%ebx
#endif /* DIAGNOSTIC */
	movq	%rsp, %rdi
	call	_C_LABEL(trap)
2:	/* Check for ASTs on exit to user mode. */
	cli
	CHECK_ASTPENDING(%r11)
	je	1f
	testb	$SEL_RPL,TF_CS(%rsp)
	jz	1f
5:	CLEAR_ASTPENDING(%r11)
	sti
	movq	%rsp, %rdi
	call	_C_LABEL(ast)
	jmp	2b
#ifndef DIAGNOSTIC
1:	INTRFASTEXIT
#else /* DIAGNOSTIC */
1:	cmpl	CPUVAR(ILEVEL),%ebx
	jne	3f
	INTRFASTEXIT
3:	sti
	movabsq	$4f,%rdi
	movl	CPUVAR(ILEVEL),%esi
	movl	%ebx,%edx
	xorq	%rax,%rax
	call	_C_LABEL(printf)
#ifdef DDB
	int	$3
#endif /* DDB */
	movl	%ebx,CPUVAR(ILEVEL)
	jmp	2b
4:	.asciz	"WARNING: SPL NOT LOWERED ON TRAP EXIT %x %x\n"
#endif /* DIAGNOSTIC */


/*
 * Macros for interrupt entry, call to handler, and exit.
 *
 * XXX
 * The interrupt frame is set up to look like a trap frame.  This may be a
 * waste.  The only handler which needs a frame is the clock handler, and it
 * only needs a few bits.  Xdoreti() needs a trap frame for handling ASTs, but
 * it could easily convert the frame on demand.
 *
 * The direct costs of setting up a trap frame are two pushq's (error code and
 * trap number), an addl to get rid of these, and pushing and popping the
 * callee-saved registers %ebx, %ebp, and %r1[2-5] twice.
 *
 * If the interrupt frame is made more flexible,  INTR can push %eax first and
 * decide the ipending case with less overhead, e.g., by avoiding loading the
 * segment registers.
 *
 */

/* XXX See comment in locore.s */
#define	XINTR(name,num)		Xintr_##name##num

	.globl _C_LABEL(x2apic_eoi)
_C_LABEL(x2apic_eoi):
	pushq   %rax
	pushq   %rcx
	pushq   %rdx
	mov     $MSR_X2APIC_EOI,%ecx
	mov     $0,%eax
	mov     $0,%edx
	wrmsr
	popq    %rdx
	popq    %rcx
	popq    %rax
	ret

#if NLAPIC > 0
#ifdef MULTIPROCESSOR
IDTVEC(recurse_lapic_ipi)
	INTR_RECURSE_HWFRAME
	pushq	$0		
	subq	$8,%rsp			/* unused __if_trapno */
	INTRENTRY		
	jmp	1f
IDTVEC(intr_lapic_ipi)
	pushq	$0		
	subq	$8,%rsp			/* unused __if_trapno */
	INTRENTRY		
	CODEPATCH_START
	movl	$0,_C_LABEL(local_apic)+LAPIC_EOI
	CODEPATCH_END(CPTAG_EOI)
	movl	CPUVAR(ILEVEL),%ebx
	cmpl	$IPL_IPI,%ebx
	jae	2f
IDTVEC(resume_lapic_ipi)
1:
	incl	CPUVAR(IDEPTH)
	movl	$IPL_IPI,CPUVAR(ILEVEL)
        sti
	cld
	pushq	%rbx
	call	_C_LABEL(x86_ipi_handler)
	jmp	_C_LABEL(Xdoreti)
2:
	movq	$(1 << LIR_IPI),%rax
	orq	%rax,CPUVAR(IPENDING)
	INTRFASTEXIT

IDTVEC(ipi_invltlb)
	pushq	%rax

	ioapic_asm_ack()

	movq	%cr3, %rax
	movq	%rax, %cr3

	lock
	decq	tlb_shoot_wait

	popq	%rax
	iretq

IDTVEC(ipi_invlpg)
	pushq	%rax

	ioapic_asm_ack()

	movq	tlb_shoot_addr1, %rax
	invlpg	(%rax)

	lock
	decq	tlb_shoot_wait

	popq	%rax
	iretq

IDTVEC(ipi_invlrange)
	pushq	%rax
	pushq	%rdx

	ioapic_asm_ack()

	movq	tlb_shoot_addr1, %rax
	movq	tlb_shoot_addr2, %rdx
1:	invlpg	(%rax)
	addq	$PAGE_SIZE, %rax
	cmpq	%rdx, %rax
	jb	1b

	lock
	decq	tlb_shoot_wait

	popq	%rdx
	popq	%rax
	iretq

#endif /* MULTIPROCESSOR */
	
	/*
	 * Interrupt from the local APIC timer.
	 */
IDTVEC(recurse_lapic_ltimer)
	INTR_RECURSE_HWFRAME
	pushq	$0		
	subq	$8,%rsp			/* unused __if_trapno */
	INTRENTRY		
	jmp	1f
IDTVEC(intr_lapic_ltimer)
	pushq	$0		
	subq	$8,%rsp			/* unused __if_trapno */
	INTRENTRY		
	CODEPATCH_START
	movl	$0,_C_LABEL(local_apic)+LAPIC_EOI
	CODEPATCH_END(CPTAG_EOI)
	movl	CPUVAR(ILEVEL),%ebx
	cmpl	$IPL_CLOCK,%ebx
	jae	2f
IDTVEC(resume_lapic_ltimer)
1:
	incl	CPUVAR(IDEPTH)
	movl	$IPL_CLOCK,CPUVAR(ILEVEL)
	sti
	cld
	pushq	%rbx
	xorq	%rdi,%rdi
	call	_C_LABEL(lapic_clockintr)
	jmp	_C_LABEL(Xdoreti)
2:
	movq	$(1 << LIR_TIMER),%rax
	orq	%rax,CPUVAR(IPENDING)
	INTRFASTEXIT
#endif /* NLAPIC > 0 */


#define voidop(num)


/*
 * This macro defines the generic stub code. Its arguments modify it
 * for specific PICs.
 */

#define	INTRSTUB(name, num, early_ack, late_ack, mask, unmask, level_mask) \
IDTVEC(recurse_##name##num)						;\
	INTR_RECURSE_HWFRAME						;\
	subq	$16,%rsp		/* space for __if_{trapno,err} */;\
	INTRENTRY							;\
IDTVEC(resume_##name##num)						\
	movq	$IREENT_MAGIC,TF_ERR(%rsp)				;\
	movl	%ebx,%r13d						;\
	movq	CPUVAR(ISOURCES) + (num) * 8, %r14			;\
	movl	IS_MAXLEVEL(%r14),%ebx					;\
	jmp	1f							;\
IDTVEC(intr_##name##num)						;\
	pushq	$0			/* dummy error code */		;\
	subq	$8,%rsp			/* unused __if_trapno */	;\
	INTRENTRY							;\
	movq	CPUVAR(ISOURCES) + (num) * 8, %r14			;\
	mask(num)			/* mask it in hardware */	;\
	early_ack(num)			/* and allow other intrs */	;\
	incl	_C_LABEL(uvmexp)+V_INTR	/* statistical info */		;\
	testq	%r14,%r14						;\
	jz	9f			/* stray */			;\
	movl	IS_MAXLEVEL(%r14),%ebx					;\
	movl	CPUVAR(ILEVEL),%r13d					;\
	cmpl	%ebx,%r13d						;\
	jae	10f			/* currently masked; hold it */	;\
1:									\
	pushq	%r13							;\
	movl	%ebx,CPUVAR(ILEVEL)					;\
	sti								;\
	cld								;\
	incl	CPUVAR(IDEPTH)						;\
	movq	IS_HANDLERS(%r14),%rbx					;\
6:									\
	movl	IH_LEVEL(%rbx),%r12d					;\
	cmpl	%r13d,%r12d						;\
	jle	7f							;\
	movl	%r12d,CPUVAR(ILEVEL)					;\
	movq	%rbx, %rsi						;\
	movq	%rsp, %rdi						;\
	call	_C_LABEL(intr_handler)	/* call it */			;\
	orl	%eax,%eax		/* should it be counted? */	;\
	jz	4f			/* no, skip it */		;\
	incq	IH_COUNT(%rbx)		/* count the intrs */		;\
	cmpl	$0,_C_LABEL(intr_shared_edge)				;\
	jne	4f			/* if no shared edges ... */	;\
	orl	%eax,%eax		/* 1 means stop trying */	;\
	jns	5f							;\
4:	movq	IH_NEXT(%rbx),%rbx	/* next handler in chain */	;\
	testq	%rbx,%rbx						;\
	jnz	6b							;\
5:									\
	cli								;\
	unmask(num)			/* unmask it in hardware */	;\
	late_ack(num)							;\
	sti								;\
	jmp	_C_LABEL(Xdoreti)	/* lower spl and do ASTs */	;\
7:									\
	cli								;\
	movq	$(1 << num),%rax					;\
	orq     %rax,CPUVAR(IPENDING)					;\
	level_mask(num)							;\
	late_ack(num)							;\
	sti								;\
	jmp	_C_LABEL(Xdoreti)	/* lower spl and do ASTs */	;\
10:									\
	cli								;\
	movq	$(1 << num),%rax					;\
	orq	%rax,CPUVAR(IPENDING)					;\
	level_mask(num)							;\
	late_ack(num)							;\
	INTRFASTEXIT							;\
9:									\
	unmask(num)							;\
	late_ack(num)							;\
	INTRFASTEXIT

#define ICUADDR IO_ICU1

INTRSTUB(legacy,0,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,1,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,2,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,3,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,4,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,5,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,6,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,7,i8259_asm_ack1,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
#undef ICUADDR
#define ICUADDR IO_ICU2

INTRSTUB(legacy,8,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,9,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,10,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,11,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,12,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,13,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,14,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)
INTRSTUB(legacy,15,i8259_asm_ack2,voidop,i8259_asm_mask,i8259_asm_unmask,
    voidop)

#if NIOAPIC > 0

INTRSTUB(ioapic_edge,0,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,1,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,2,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,3,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,4,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,5,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,6,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,7,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,8,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,9,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,10,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,11,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,12,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,13,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,14,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,15,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,16,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,17,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,18,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,19,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,20,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,21,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,22,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,23,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,24,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,25,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,26,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,27,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,28,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,29,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,30,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,31,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,32,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,33,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,34,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,35,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,36,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,37,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,38,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,39,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,40,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,41,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,42,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,43,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,44,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,45,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,46,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,47,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,48,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,49,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,50,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,51,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,52,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,53,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,54,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,55,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,56,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,57,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,58,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,59,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,60,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,61,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,62,voidop,ioapic_asm_ack,voidop,voidop,voidop)
INTRSTUB(ioapic_edge,63,voidop,ioapic_asm_ack,voidop,voidop,voidop)

INTRSTUB(ioapic_level,0,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,1,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,2,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,3,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,4,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,5,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,6,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,7,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,8,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,9,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,10,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,11,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,12,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,13,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,14,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,15,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,16,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,17,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,18,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,19,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,20,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,21,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,22,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,23,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,24,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,25,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,26,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,27,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,28,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,29,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,30,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,31,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,32,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,33,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,34,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,35,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,36,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,37,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,38,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,39,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,40,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,41,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,42,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,43,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,44,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,45,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,46,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,47,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,48,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,49,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,50,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,51,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,52,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,53,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,54,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,55,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,56,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,57,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,58,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,59,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,60,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,61,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,62,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)
INTRSTUB(ioapic_level,63,voidop,ioapic_asm_ack,voidop,ioapic_unmask,ioapic_mask)

#endif

	.globl _C_LABEL(i8259_stubs)
_C_LABEL(i8259_stubs):
	.quad _C_LABEL(Xintr_legacy0), _C_LABEL(Xrecurse_legacy0)
	.quad _C_LABEL(Xresume_legacy0)
	.quad _C_LABEL(Xintr_legacy1), _C_LABEL(Xrecurse_legacy1)
	.quad _C_LABEL(Xresume_legacy1)
	.quad _C_LABEL(Xintr_legacy2), _C_LABEL(Xrecurse_legacy2)
	.quad _C_LABEL(Xresume_legacy2)
	.quad _C_LABEL(Xintr_legacy3), _C_LABEL(Xrecurse_legacy3)
	.quad _C_LABEL(Xresume_legacy3)
	.quad _C_LABEL(Xintr_legacy4), _C_LABEL(Xrecurse_legacy4)
	.quad _C_LABEL(Xresume_legacy4)
	.quad _C_LABEL(Xintr_legacy5), _C_LABEL(Xrecurse_legacy5)
	.quad _C_LABEL(Xresume_legacy5)
	.quad _C_LABEL(Xintr_legacy6), _C_LABEL(Xrecurse_legacy6)
	.quad _C_LABEL(Xresume_legacy6)
	.quad _C_LABEL(Xintr_legacy7), _C_LABEL(Xrecurse_legacy7)
	.quad _C_LABEL(Xresume_legacy7)
	.quad _C_LABEL(Xintr_legacy8), _C_LABEL(Xrecurse_legacy8)
	.quad _C_LABEL(Xresume_legacy8)
	.quad _C_LABEL(Xintr_legacy9), _C_LABEL(Xrecurse_legacy9)
	.quad _C_LABEL(Xresume_legacy9)
	.quad _C_LABEL(Xintr_legacy10), _C_LABEL(Xrecurse_legacy10)
	.quad _C_LABEL(Xresume_legacy10)
	.quad _C_LABEL(Xintr_legacy11), _C_LABEL(Xrecurse_legacy11)
	.quad _C_LABEL(Xresume_legacy11)
	.quad _C_LABEL(Xintr_legacy12), _C_LABEL(Xrecurse_legacy12)
	.quad _C_LABEL(Xresume_legacy12)
	.quad _C_LABEL(Xintr_legacy13), _C_LABEL(Xrecurse_legacy13)
	.quad _C_LABEL(Xresume_legacy13)
	.quad _C_LABEL(Xintr_legacy14), _C_LABEL(Xrecurse_legacy14)
	.quad _C_LABEL(Xresume_legacy14)
	.quad _C_LABEL(Xintr_legacy15), _C_LABEL(Xrecurse_legacy15)
	.quad _C_LABEL(Xresume_legacy15)

#if NIOAPIC > 0
	.globl _C_LABEL(ioapic_edge_stubs)
_C_LABEL(ioapic_edge_stubs):
	.quad _C_LABEL(Xintr_ioapic_edge0), _C_LABEL(Xrecurse_ioapic_edge0)
	.quad _C_LABEL(Xresume_ioapic_edge0)
	.quad _C_LABEL(Xintr_ioapic_edge1), _C_LABEL(Xrecurse_ioapic_edge1)
	.quad _C_LABEL(Xresume_ioapic_edge1)
	.quad _C_LABEL(Xintr_ioapic_edge2), _C_LABEL(Xrecurse_ioapic_edge2)
	.quad _C_LABEL(Xresume_ioapic_edge2)
	.quad _C_LABEL(Xintr_ioapic_edge3), _C_LABEL(Xrecurse_ioapic_edge3)
	.quad _C_LABEL(Xresume_ioapic_edge3)
	.quad _C_LABEL(Xintr_ioapic_edge4), _C_LABEL(Xrecurse_ioapic_edge4)
	.quad _C_LABEL(Xresume_ioapic_edge4)
	.quad _C_LABEL(Xintr_ioapic_edge5), _C_LABEL(Xrecurse_ioapic_edge5)
	.quad _C_LABEL(Xresume_ioapic_edge5)
	.quad _C_LABEL(Xintr_ioapic_edge6), _C_LABEL(Xrecurse_ioapic_edge6)
	.quad _C_LABEL(Xresume_ioapic_edge6)
	.quad _C_LABEL(Xintr_ioapic_edge7), _C_LABEL(Xrecurse_ioapic_edge7)
	.quad _C_LABEL(Xresume_ioapic_edge7)
	.quad _C_LABEL(Xintr_ioapic_edge8), _C_LABEL(Xrecurse_ioapic_edge8)
	.quad _C_LABEL(Xresume_ioapic_edge8)
	.quad _C_LABEL(Xintr_ioapic_edge9), _C_LABEL(Xrecurse_ioapic_edge9)
	.quad _C_LABEL(Xresume_ioapic_edge9)
	.quad _C_LABEL(Xintr_ioapic_edge10), _C_LABEL(Xrecurse_ioapic_edge10)
	.quad _C_LABEL(Xresume_ioapic_edge10)
	.quad _C_LABEL(Xintr_ioapic_edge11), _C_LABEL(Xrecurse_ioapic_edge11)
	.quad _C_LABEL(Xresume_ioapic_edge11)
	.quad _C_LABEL(Xintr_ioapic_edge12), _C_LABEL(Xrecurse_ioapic_edge12)
	.quad _C_LABEL(Xresume_ioapic_edge12)
	.quad _C_LABEL(Xintr_ioapic_edge13), _C_LABEL(Xrecurse_ioapic_edge13)
	.quad _C_LABEL(Xresume_ioapic_edge13)
	.quad _C_LABEL(Xintr_ioapic_edge14), _C_LABEL(Xrecurse_ioapic_edge14)
	.quad _C_LABEL(Xresume_ioapic_edge14)
	.quad _C_LABEL(Xintr_ioapic_edge15), _C_LABEL(Xrecurse_ioapic_edge15)
	.quad _C_LABEL(Xresume_ioapic_edge15)
	.quad _C_LABEL(Xintr_ioapic_edge16), _C_LABEL(Xrecurse_ioapic_edge16)
	.quad _C_LABEL(Xresume_ioapic_edge16)
	.quad _C_LABEL(Xintr_ioapic_edge17), _C_LABEL(Xrecurse_ioapic_edge17)
	.quad _C_LABEL(Xresume_ioapic_edge17)
	.quad _C_LABEL(Xintr_ioapic_edge18), _C_LABEL(Xrecurse_ioapic_edge18)
	.quad _C_LABEL(Xresume_ioapic_edge18)
	.quad _C_LABEL(Xintr_ioapic_edge19), _C_LABEL(Xrecurse_ioapic_edge19)
	.quad _C_LABEL(Xresume_ioapic_edge19)
	.quad _C_LABEL(Xintr_ioapic_edge20), _C_LABEL(Xrecurse_ioapic_edge20)
	.quad _C_LABEL(Xresume_ioapic_edge20)
	.quad _C_LABEL(Xintr_ioapic_edge21), _C_LABEL(Xrecurse_ioapic_edge21)
	.quad _C_LABEL(Xresume_ioapic_edge21)
	.quad _C_LABEL(Xintr_ioapic_edge22), _C_LABEL(Xrecurse_ioapic_edge22)
	.quad _C_LABEL(Xresume_ioapic_edge22)
	.quad _C_LABEL(Xintr_ioapic_edge23), _C_LABEL(Xrecurse_ioapic_edge23)
	.quad _C_LABEL(Xresume_ioapic_edge23)
	.quad _C_LABEL(Xintr_ioapic_edge24), _C_LABEL(Xrecurse_ioapic_edge24)
	.quad _C_LABEL(Xresume_ioapic_edge24)
	.quad _C_LABEL(Xintr_ioapic_edge25), _C_LABEL(Xrecurse_ioapic_edge25)
	.quad _C_LABEL(Xresume_ioapic_edge25)
	.quad _C_LABEL(Xintr_ioapic_edge26), _C_LABEL(Xrecurse_ioapic_edge26)
	.quad _C_LABEL(Xresume_ioapic_edge26)
	.quad _C_LABEL(Xintr_ioapic_edge27), _C_LABEL(Xrecurse_ioapic_edge27)
	.quad _C_LABEL(Xresume_ioapic_edge27)
	.quad _C_LABEL(Xintr_ioapic_edge28), _C_LABEL(Xrecurse_ioapic_edge28)
	.quad _C_LABEL(Xresume_ioapic_edge28)
	.quad _C_LABEL(Xintr_ioapic_edge29), _C_LABEL(Xrecurse_ioapic_edge29)
	.quad _C_LABEL(Xresume_ioapic_edge29)
	.quad _C_LABEL(Xintr_ioapic_edge30), _C_LABEL(Xrecurse_ioapic_edge30)
	.quad _C_LABEL(Xresume_ioapic_edge30)
	.quad _C_LABEL(Xintr_ioapic_edge31), _C_LABEL(Xrecurse_ioapic_edge31)
	.quad _C_LABEL(Xresume_ioapic_edge31)
	.quad _C_LABEL(Xintr_ioapic_edge32), _C_LABEL(Xrecurse_ioapic_edge32)
	.quad _C_LABEL(Xresume_ioapic_edge32)
	.quad _C_LABEL(Xintr_ioapic_edge33), _C_LABEL(Xrecurse_ioapic_edge33)
	.quad _C_LABEL(Xresume_ioapic_edge33)
	.quad _C_LABEL(Xintr_ioapic_edge34), _C_LABEL(Xrecurse_ioapic_edge34)
	.quad _C_LABEL(Xresume_ioapic_edge34)
	.quad _C_LABEL(Xintr_ioapic_edge35), _C_LABEL(Xrecurse_ioapic_edge35)
	.quad _C_LABEL(Xresume_ioapic_edge35)
	.quad _C_LABEL(Xintr_ioapic_edge36), _C_LABEL(Xrecurse_ioapic_edge36)
	.quad _C_LABEL(Xresume_ioapic_edge36)
	.quad _C_LABEL(Xintr_ioapic_edge37), _C_LABEL(Xrecurse_ioapic_edge37)
	.quad _C_LABEL(Xresume_ioapic_edge37)
	.quad _C_LABEL(Xintr_ioapic_edge38), _C_LABEL(Xrecurse_ioapic_edge38)
	.quad _C_LABEL(Xresume_ioapic_edge38)
	.quad _C_LABEL(Xintr_ioapic_edge39), _C_LABEL(Xrecurse_ioapic_edge39)
	.quad _C_LABEL(Xresume_ioapic_edge39)
	.quad _C_LABEL(Xintr_ioapic_edge40), _C_LABEL(Xrecurse_ioapic_edge40)
	.quad _C_LABEL(Xresume_ioapic_edge40)
	.quad _C_LABEL(Xintr_ioapic_edge41), _C_LABEL(Xrecurse_ioapic_edge41)
	.quad _C_LABEL(Xresume_ioapic_edge41)
	.quad _C_LABEL(Xintr_ioapic_edge42), _C_LABEL(Xrecurse_ioapic_edge42)
	.quad _C_LABEL(Xresume_ioapic_edge42)
	.quad _C_LABEL(Xintr_ioapic_edge43), _C_LABEL(Xrecurse_ioapic_edge43)
	.quad _C_LABEL(Xresume_ioapic_edge43)
	.quad _C_LABEL(Xintr_ioapic_edge44), _C_LABEL(Xrecurse_ioapic_edge44)
	.quad _C_LABEL(Xresume_ioapic_edge44)
	.quad _C_LABEL(Xintr_ioapic_edge45), _C_LABEL(Xrecurse_ioapic_edge45)
	.quad _C_LABEL(Xresume_ioapic_edge45)
	.quad _C_LABEL(Xintr_ioapic_edge46), _C_LABEL(Xrecurse_ioapic_edge46)
	.quad _C_LABEL(Xresume_ioapic_edge46)
	.quad _C_LABEL(Xintr_ioapic_edge47), _C_LABEL(Xrecurse_ioapic_edge47)
	.quad _C_LABEL(Xresume_ioapic_edge47)
	.quad _C_LABEL(Xintr_ioapic_edge48), _C_LABEL(Xrecurse_ioapic_edge48)
	.quad _C_LABEL(Xresume_ioapic_edge48)
	.quad _C_LABEL(Xintr_ioapic_edge49), _C_LABEL(Xrecurse_ioapic_edge49)
	.quad _C_LABEL(Xresume_ioapic_edge49)
	.quad _C_LABEL(Xintr_ioapic_edge50), _C_LABEL(Xrecurse_ioapic_edge50)
	.quad _C_LABEL(Xresume_ioapic_edge50)
	.quad _C_LABEL(Xintr_ioapic_edge51), _C_LABEL(Xrecurse_ioapic_edge51)
	.quad _C_LABEL(Xresume_ioapic_edge51)
	.quad _C_LABEL(Xintr_ioapic_edge52), _C_LABEL(Xrecurse_ioapic_edge52)
	.quad _C_LABEL(Xresume_ioapic_edge52)
	.quad _C_LABEL(Xintr_ioapic_edge53), _C_LABEL(Xrecurse_ioapic_edge53)
	.quad _C_LABEL(Xresume_ioapic_edge53)
	.quad _C_LABEL(Xintr_ioapic_edge54), _C_LABEL(Xrecurse_ioapic_edge54)
	.quad _C_LABEL(Xresume_ioapic_edge54)
	.quad _C_LABEL(Xintr_ioapic_edge55), _C_LABEL(Xrecurse_ioapic_edge55)
	.quad _C_LABEL(Xresume_ioapic_edge55)
	.quad _C_LABEL(Xintr_ioapic_edge56), _C_LABEL(Xrecurse_ioapic_edge56)
	.quad _C_LABEL(Xresume_ioapic_edge56)
	.quad _C_LABEL(Xintr_ioapic_edge57), _C_LABEL(Xrecurse_ioapic_edge57)
	.quad _C_LABEL(Xresume_ioapic_edge57)
	.quad _C_LABEL(Xintr_ioapic_edge58), _C_LABEL(Xrecurse_ioapic_edge58)
	.quad _C_LABEL(Xresume_ioapic_edge58)
	.quad _C_LABEL(Xintr_ioapic_edge59), _C_LABEL(Xrecurse_ioapic_edge59)
	.quad _C_LABEL(Xresume_ioapic_edge59)
	.quad _C_LABEL(Xintr_ioapic_edge60), _C_LABEL(Xrecurse_ioapic_edge60)
	.quad _C_LABEL(Xresume_ioapic_edge60)
	.quad _C_LABEL(Xintr_ioapic_edge61), _C_LABEL(Xrecurse_ioapic_edge61)
	.quad _C_LABEL(Xresume_ioapic_edge61)
	.quad _C_LABEL(Xintr_ioapic_edge62), _C_LABEL(Xrecurse_ioapic_edge62)
	.quad _C_LABEL(Xresume_ioapic_edge62)
	.quad _C_LABEL(Xintr_ioapic_edge63), _C_LABEL(Xrecurse_ioapic_edge63)
	.quad _C_LABEL(Xresume_ioapic_edge63)

	.globl _C_LABEL(ioapic_level_stubs)
_C_LABEL(ioapic_level_stubs):
	.quad _C_LABEL(Xintr_ioapic_level0), _C_LABEL(Xrecurse_ioapic_level0)
	.quad _C_LABEL(Xresume_ioapic_level0)
	.quad _C_LABEL(Xintr_ioapic_level1), _C_LABEL(Xrecurse_ioapic_level1)
	.quad _C_LABEL(Xresume_ioapic_level1)
	.quad _C_LABEL(Xintr_ioapic_level2), _C_LABEL(Xrecurse_ioapic_level2)
	.quad _C_LABEL(Xresume_ioapic_level2)
	.quad _C_LABEL(Xintr_ioapic_level3), _C_LABEL(Xrecurse_ioapic_level3)
	.quad _C_LABEL(Xresume_ioapic_level3)
	.quad _C_LABEL(Xintr_ioapic_level4), _C_LABEL(Xrecurse_ioapic_level4)
	.quad _C_LABEL(Xresume_ioapic_level4)
	.quad _C_LABEL(Xintr_ioapic_level5), _C_LABEL(Xrecurse_ioapic_level5)
	.quad _C_LABEL(Xresume_ioapic_level5)
	.quad _C_LABEL(Xintr_ioapic_level6), _C_LABEL(Xrecurse_ioapic_level6)
	.quad _C_LABEL(Xresume_ioapic_level6)
	.quad _C_LABEL(Xintr_ioapic_level7), _C_LABEL(Xrecurse_ioapic_level7)
	.quad _C_LABEL(Xresume_ioapic_level7)
	.quad _C_LABEL(Xintr_ioapic_level8), _C_LABEL(Xrecurse_ioapic_level8)
	.quad _C_LABEL(Xresume_ioapic_level8)
	.quad _C_LABEL(Xintr_ioapic_level9), _C_LABEL(Xrecurse_ioapic_level9)
	.quad _C_LABEL(Xresume_ioapic_level9)
	.quad _C_LABEL(Xintr_ioapic_level10), _C_LABEL(Xrecurse_ioapic_level10)
	.quad _C_LABEL(Xresume_ioapic_level10)
	.quad _C_LABEL(Xintr_ioapic_level11), _C_LABEL(Xrecurse_ioapic_level11)
	.quad _C_LABEL(Xresume_ioapic_level11)
	.quad _C_LABEL(Xintr_ioapic_level12), _C_LABEL(Xrecurse_ioapic_level12)
	.quad _C_LABEL(Xresume_ioapic_level12)
	.quad _C_LABEL(Xintr_ioapic_level13), _C_LABEL(Xrecurse_ioapic_level13)
	.quad _C_LABEL(Xresume_ioapic_level13)
	.quad _C_LABEL(Xintr_ioapic_level14), _C_LABEL(Xrecurse_ioapic_level14)
	.quad _C_LABEL(Xresume_ioapic_level14)
	.quad _C_LABEL(Xintr_ioapic_level15), _C_LABEL(Xrecurse_ioapic_level15)
	.quad _C_LABEL(Xresume_ioapic_level15)
	.quad _C_LABEL(Xintr_ioapic_level16), _C_LABEL(Xrecurse_ioapic_level16)
	.quad _C_LABEL(Xresume_ioapic_level16)
	.quad _C_LABEL(Xintr_ioapic_level17), _C_LABEL(Xrecurse_ioapic_level17)
	.quad _C_LABEL(Xresume_ioapic_level17)
	.quad _C_LABEL(Xintr_ioapic_level18), _C_LABEL(Xrecurse_ioapic_level18)
	.quad _C_LABEL(Xresume_ioapic_level18)
	.quad _C_LABEL(Xintr_ioapic_level19), _C_LABEL(Xrecurse_ioapic_level19)
	.quad _C_LABEL(Xresume_ioapic_level19)
	.quad _C_LABEL(Xintr_ioapic_level20), _C_LABEL(Xrecurse_ioapic_level20)
	.quad _C_LABEL(Xresume_ioapic_level20)
	.quad _C_LABEL(Xintr_ioapic_level21), _C_LABEL(Xrecurse_ioapic_level21)
	.quad _C_LABEL(Xresume_ioapic_level21)
	.quad _C_LABEL(Xintr_ioapic_level22), _C_LABEL(Xrecurse_ioapic_level22)
	.quad _C_LABEL(Xresume_ioapic_level22)
	.quad _C_LABEL(Xintr_ioapic_level23), _C_LABEL(Xrecurse_ioapic_level23)
	.quad _C_LABEL(Xresume_ioapic_level23)
	.quad _C_LABEL(Xintr_ioapic_level24), _C_LABEL(Xrecurse_ioapic_level24)
	.quad _C_LABEL(Xresume_ioapic_level24)
	.quad _C_LABEL(Xintr_ioapic_level25), _C_LABEL(Xrecurse_ioapic_level25)
	.quad _C_LABEL(Xresume_ioapic_level25)
	.quad _C_LABEL(Xintr_ioapic_level26), _C_LABEL(Xrecurse_ioapic_level26)
	.quad _C_LABEL(Xresume_ioapic_level26)
	.quad _C_LABEL(Xintr_ioapic_level27), _C_LABEL(Xrecurse_ioapic_level27)
	.quad _C_LABEL(Xresume_ioapic_level27)
	.quad _C_LABEL(Xintr_ioapic_level28), _C_LABEL(Xrecurse_ioapic_level28)
	.quad _C_LABEL(Xresume_ioapic_level28)
	.quad _C_LABEL(Xintr_ioapic_level29), _C_LABEL(Xrecurse_ioapic_level29)
	.quad _C_LABEL(Xresume_ioapic_level29)
	.quad _C_LABEL(Xintr_ioapic_level30), _C_LABEL(Xrecurse_ioapic_level30)
	.quad _C_LABEL(Xresume_ioapic_level30)
	.quad _C_LABEL(Xintr_ioapic_level31), _C_LABEL(Xrecurse_ioapic_level31)
	.quad _C_LABEL(Xresume_ioapic_level31)
	.quad _C_LABEL(Xintr_ioapic_level32), _C_LABEL(Xrecurse_ioapic_level32)
	.quad _C_LABEL(Xresume_ioapic_level32)
	.quad _C_LABEL(Xintr_ioapic_level33), _C_LABEL(Xrecurse_ioapic_level33)
	.quad _C_LABEL(Xresume_ioapic_level33)
	.quad _C_LABEL(Xintr_ioapic_level34), _C_LABEL(Xrecurse_ioapic_level34)
	.quad _C_LABEL(Xresume_ioapic_level34)
	.quad _C_LABEL(Xintr_ioapic_level35), _C_LABEL(Xrecurse_ioapic_level35)
	.quad _C_LABEL(Xresume_ioapic_level35)
	.quad _C_LABEL(Xintr_ioapic_level36), _C_LABEL(Xrecurse_ioapic_level36)
	.quad _C_LABEL(Xresume_ioapic_level36)
	.quad _C_LABEL(Xintr_ioapic_level37), _C_LABEL(Xrecurse_ioapic_level37)
	.quad _C_LABEL(Xresume_ioapic_level37)
	.quad _C_LABEL(Xintr_ioapic_level38), _C_LABEL(Xrecurse_ioapic_level38)
	.quad _C_LABEL(Xresume_ioapic_level38)
	.quad _C_LABEL(Xintr_ioapic_level39), _C_LABEL(Xrecurse_ioapic_level39)
	.quad _C_LABEL(Xresume_ioapic_level39)
	.quad _C_LABEL(Xintr_ioapic_level40), _C_LABEL(Xrecurse_ioapic_level40)
	.quad _C_LABEL(Xresume_ioapic_level40)
	.quad _C_LABEL(Xintr_ioapic_level41), _C_LABEL(Xrecurse_ioapic_level41)
	.quad _C_LABEL(Xresume_ioapic_level41)
	.quad _C_LABEL(Xintr_ioapic_level42), _C_LABEL(Xrecurse_ioapic_level42)
	.quad _C_LABEL(Xresume_ioapic_level42)
	.quad _C_LABEL(Xintr_ioapic_level43), _C_LABEL(Xrecurse_ioapic_level43)
	.quad _C_LABEL(Xresume_ioapic_level43)
	.quad _C_LABEL(Xintr_ioapic_level44), _C_LABEL(Xrecurse_ioapic_level44)
	.quad _C_LABEL(Xresume_ioapic_level44)
	.quad _C_LABEL(Xintr_ioapic_level45), _C_LABEL(Xrecurse_ioapic_level45)
	.quad _C_LABEL(Xresume_ioapic_level45)
	.quad _C_LABEL(Xintr_ioapic_level46), _C_LABEL(Xrecurse_ioapic_level46)
	.quad _C_LABEL(Xresume_ioapic_level46)
	.quad _C_LABEL(Xintr_ioapic_level47), _C_LABEL(Xrecurse_ioapic_level47)
	.quad _C_LABEL(Xresume_ioapic_level47)
	.quad _C_LABEL(Xintr_ioapic_level48), _C_LABEL(Xrecurse_ioapic_level48)
	.quad _C_LABEL(Xresume_ioapic_level48)
	.quad _C_LABEL(Xintr_ioapic_level49), _C_LABEL(Xrecurse_ioapic_level49)
	.quad _C_LABEL(Xresume_ioapic_level49)
	.quad _C_LABEL(Xintr_ioapic_level50), _C_LABEL(Xrecurse_ioapic_level50)
	.quad _C_LABEL(Xresume_ioapic_level50)
	.quad _C_LABEL(Xintr_ioapic_level51), _C_LABEL(Xrecurse_ioapic_level51)
	.quad _C_LABEL(Xresume_ioapic_level51)
	.quad _C_LABEL(Xintr_ioapic_level52), _C_LABEL(Xrecurse_ioapic_level52)
	.quad _C_LABEL(Xresume_ioapic_level52)
	.quad _C_LABEL(Xintr_ioapic_level53), _C_LABEL(Xrecurse_ioapic_level53)
	.quad _C_LABEL(Xresume_ioapic_level53)
	.quad _C_LABEL(Xintr_ioapic_level54), _C_LABEL(Xrecurse_ioapic_level54)
	.quad _C_LABEL(Xresume_ioapic_level54)
	.quad _C_LABEL(Xintr_ioapic_level55), _C_LABEL(Xrecurse_ioapic_level55)
	.quad _C_LABEL(Xresume_ioapic_level55)
	.quad _C_LABEL(Xintr_ioapic_level56), _C_LABEL(Xrecurse_ioapic_level56)
	.quad _C_LABEL(Xresume_ioapic_level56)
	.quad _C_LABEL(Xintr_ioapic_level57), _C_LABEL(Xrecurse_ioapic_level57)
	.quad _C_LABEL(Xresume_ioapic_level57)
	.quad _C_LABEL(Xintr_ioapic_level58), _C_LABEL(Xrecurse_ioapic_level58)
	.quad _C_LABEL(Xresume_ioapic_level58)
	.quad _C_LABEL(Xintr_ioapic_level59), _C_LABEL(Xrecurse_ioapic_level59)
	.quad _C_LABEL(Xresume_ioapic_level59)
	.quad _C_LABEL(Xintr_ioapic_level60), _C_LABEL(Xrecurse_ioapic_level60)
	.quad _C_LABEL(Xresume_ioapic_level60)
	.quad _C_LABEL(Xintr_ioapic_level61), _C_LABEL(Xrecurse_ioapic_level61)
	.quad _C_LABEL(Xresume_ioapic_level61)
	.quad _C_LABEL(Xintr_ioapic_level62), _C_LABEL(Xrecurse_ioapic_level62)
	.quad _C_LABEL(Xresume_ioapic_level62)
	.quad _C_LABEL(Xintr_ioapic_level63), _C_LABEL(Xrecurse_ioapic_level63)
	.quad _C_LABEL(Xresume_ioapic_level63)
#endif

	.data
/*
 * Soft interrupt handlers
 */
	.text
IDTVEC(softtty)
	movl	$IPL_SOFTTTY, CPUVAR(ILEVEL)
	sti
	incl	CPUVAR(IDEPTH)
	movl	$X86_SOFTINTR_SOFTTTY,%edi
	call	_C_LABEL(softintr_dispatch)
	decl	CPUVAR(IDEPTH)
	jmp	*%r13

IDTVEC(softnet)
	movl	$IPL_SOFTNET, CPUVAR(ILEVEL)
	sti
	incl	CPUVAR(IDEPTH)
	movl	$X86_SOFTINTR_SOFTNET,%edi
	call	_C_LABEL(softintr_dispatch)
	decl	CPUVAR(IDEPTH)
	jmp	*%r13

IDTVEC(softclock)
	movl	$IPL_SOFTCLOCK, CPUVAR(ILEVEL)
	sti
	incl	CPUVAR(IDEPTH)
	movl	$X86_SOFTINTR_SOFTCLOCK,%edi
	call	_C_LABEL(softintr_dispatch)
	decl	CPUVAR(IDEPTH)
	jmp	*%r13
