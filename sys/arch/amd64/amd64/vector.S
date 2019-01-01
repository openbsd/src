/*	$OpenBSD: vector.S,v 1.78 2019/01/01 03:19:37 guenther Exp $	*/
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
#include "xen.h"
#include "hyperv.h"

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

IDTVEC(trap00)
	ZTRAP(T_DIVIDE)
IDTVEC(trap01)
	ZTRAP(T_TRCTRAP)

/*
 * NMIs can happen at any time, so there's no simple way to tell
 * which GS.base is in place at the time of the interrupt.  Instead,
 * borrow a couple ideas from FreeBSD and put the CPU's kernel
 * GS.base in the memory right above the stack, storing the current
 * one in a pair of callee-saved registers (%r12/13).  We save the
 * current %cr3 in a callee-saved register too (%r15).
 * Note: we don't unblock interrupts because a nested normal interrupt
 * would also reenable NMIs.
 */
IDTVEC(trap02)
	pushq	$0
	pushq	$T_NMI
calltrap_specstk:			# special stack path
	INTR_ENTRY_KERN
	INTR_SAVE_MOST_GPRS_NO_ADJ
	INTR_CLEAR_GPRS
	movl	$MSR_GSBASE,%ecx	# save current GS.base...
	rdmsr
	movq	%rax,%r12		# ...in %r12 and %r13
	movq	%rdx,%r13
	movq	FRAMESIZE(%rsp),%rax	# get kernel GS.base
	movq	%rax,%rdx
	shrq	$32,%rdx
	wrmsr				# switch to it
	movq	%cr3,%r15		# save current %cr3 in %r15
	movq	CPUVAR(KERN_CR3),%rax	# switch to kernel page tables
	testq	%rax,%rax
	jz	INTRENTRY_LABEL(calltrap_specstk)
	movq	%rax,%cr3
	jmp	INTRENTRY_LABEL(calltrap_specstk)
	.text
	.globl	INTRENTRY_LABEL(calltrap_specstk)
INTRENTRY_LABEL(calltrap_specstk):
	cld
	SMAP_CLAC
	movq	%rsp,%rdi
	call	_C_LABEL(kerntrap)
	movl	$MSR_GSBASE,%ecx	# restore GS.base
	movq	%r12,%rax
	movq	%r13,%rdx
	wrmsr
	popq	%rdi
	popq	%rsi
	popq	%rdx
	popq	%rcx
	popq	%r8
	popq	%r9
	popq	%r10
	popq	%r11
	popq	%r12
	popq	%r13
	popq	%r14
	jmp	calltrap_specstk_tramp
KUENTRY(calltrap_specstk_tramp)
	movq	%r15,%cr3		# restore %cr3
	popq	%r15
	addq	$8,%rsp			# ignore tf_err
	popq	%rbx
	popq	%rax
	addq	$8,%rsp			# ignore tf_trapno
	popq	%rbp
	iretq

IDTVEC(trap03)
#if defined(GPROF) || !defined(DDBPROF)
	ZTRAP(T_BPTFLT)
#else /* !defined(GPROF) && defined(DDBPROF) */
	pushq	$0
	pushq	$T_BPTFLT
	testb	$SEL_RPL,24(%rsp)
	je	INTRENTRY_LABEL(trap03)
	jmp	alltraps
	.text
	.global	INTRENTRY_LABEL(trap03)
INTRENTRY_LABEL(trap03):
	INTR_ENTRY_KERN
	INTR_SAVE_MOST_GPRS_NO_ADJ
	sti
	cld
	SMAP_CLAC
	movq	%rsp, %rdi
	call	_C_LABEL(db_prof_hook)
	cmpl	$1, %eax
	jne	.Lreal_kern_trap

	cli
	movq	TF_RDI(%rsp),%rdi
	movq	TF_RSI(%rsp),%rsi
	movq	TF_R8(%rsp),%r8
	movq	TF_R9(%rsp),%r9
	movq	TF_R10(%rsp),%r10
	movq	TF_R12(%rsp),%r12
	movq	TF_R13(%rsp),%r13
	movq	TF_R14(%rsp),%r14
	movq	TF_R15(%rsp),%r15
	movq	TF_RBP(%rsp),%rbp
	movq	TF_RBX(%rsp),%rbx
	movq	TF_RDX(%rsp),%rdx
	movq	TF_RCX(%rsp),%rcx
	movq	TF_R11(%rsp),%r11
	/* %rax restored below, after being used to shift the stack */

	/*
	 * We are returning from a probe trap so we need to fix the
	 * stack layout and emulate the patched instruction.
	 * Reserve enough room to emulate "pushq %rbp".
	 */
	subq	$16, %rsp

	/* Shift hardware-saved registers. */
	movq	(TF_RIP + 16)(%rsp), %rax
	movq	%rax, TF_RIP(%rsp)
	movq	(TF_CS + 16)(%rsp), %rax
	movq	%rax, TF_CS(%rsp)
	movq	(TF_RFLAGS + 16)(%rsp), %rax
	movq	%rax, TF_RFLAGS(%rsp)
	movq	(TF_RSP + 16)(%rsp), %rax
	movq	%rax, TF_RSP(%rsp)
	movq	(TF_SS + 16)(%rsp), %rax
	movq	%rax, TF_SS(%rsp)

	/* Pull 8 bytes off the stack and store %rbp in the expected location.*/
	movq	TF_RSP(%rsp), %rax
	subq	$8, %rax
	movq	%rax, TF_RSP(%rsp)
	movq	%rbp, (%rax)

	/* Finally restore %rax */
	movq	(TF_RAX + 16)(%rsp),%rax

	addq	$TF_RIP,%rsp
	iretq
#endif /* !defined(GPROF) && defined(DDBPROF) */

IDTVEC(trap04)
	ZTRAP(T_OFLOW)	# impossible: INTO instruction invalid in amd64
IDTVEC(trap05)
	ZTRAP(T_BOUND)	# impossible: BOUND instruction invalid in amd64
IDTVEC(trap06)
	ZTRAP(T_PRIVINFLT)
IDTVEC(trap07)
	ZTRAP(T_DNA)		# impossible: we don't do lazy FPU
IDTVEC(trap08)
	pushq	$T_DOUBLEFLT
	jmp	calltrap_specstk
IDTVEC(trap09)
	ZTRAP(T_FPOPFLT)	# impossible: not generated on amd64
IDTVEC(trap0a)
	TRAP(T_TSSFLT)
IDTVEC(trap0b)
	TRAP(T_SEGNPFLT)
IDTVEC(trap0c)
	TRAP(T_STKFLT)

/*
 * The #GP (general protection fault) handler has a couple weird cases
 * to handle:
 *  - trapping in iretq to userspace and
 *  - trapping in xrstor in the kernel.
 * We detect both of these by examining the %rip in the iretq_frame.
 * Handling them is done by updating %rip in the iretq_frame to point
 * to a stub handler of some sort and then iretq'ing to it.  For the
 * iretq fault we resume in a stub which acts like we got a fresh #GP.
 * For the xrstor fault we resume to a stub which returns an error to
 * the routine that requested the xrstor.
 */
IDTVEC(trap0d)
	pushq	%rdx
	pushq	%rcx
	movq	24(%rsp),%rdx		/* over %r[cd]x and err to %rip */
	leaq	doreti_iret(%rip),%rcx
	cmpq	%rcx,%rdx
	je	.Lhandle_doreti
	leaq	xrstor_fault(%rip),%rcx
	cmpq	%rcx,%rdx
	je	.Lhandle_xrstor
	leaq	xsetbv_fault(%rip),%rcx
	cmpq	%rcx,%rdx
	je	.Lhandle_xsetbv
	popq	%rcx
	popq	%rdx
	TRAP(T_PROTFLT)

.Lhandle_xrstor:
	/* xrstor faulted; just resume in xrstor_resume */
	leaq	xrstor_resume(%rip),%rcx
	jmp	1f

.Lhandle_xsetbv:
	/* xsetbv faulted; just resume in xsetbv_resume */
	leaq	xsetbv_resume(%rip),%rcx
	jmp	1f

.Lhandle_doreti:
	/* iretq faulted; resume in a stub that acts like we got a #GP */
	leaq	.Lhandle_doreti_resume(%rip),%rcx
1:	movq	%rcx,24(%rsp)		/* over %r[cd]x and err to %rip */
	popq	%rcx
	popq	%rdx
	addq	$8,%rsp			/* pop the err code */
	jmp	doreti_iret
.Lhandle_doreti_resume:
	ZTRAP(T_PROTFLT)

IDTVEC(trap0e)
	TRAP(T_PAGEFLT)
IDTVEC(intrspurious)
IDTVEC_NOALIGN(trap0f)
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
IDTVEC_NOALIGN(trap15)
IDTVEC_NOALIGN(trap16)
IDTVEC_NOALIGN(trap17)
IDTVEC_NOALIGN(trap18)
IDTVEC_NOALIGN(trap19)
IDTVEC_NOALIGN(trap1a)
IDTVEC_NOALIGN(trap1b)
IDTVEC_NOALIGN(trap1c)
IDTVEC_NOALIGN(trap1d)
IDTVEC_NOALIGN(trap1e)
IDTVEC_NOALIGN(trap1f)
	/* 20 - 31 reserved for future exp */
	ZTRAP(T_RESERVED)

	.section .rodata
	.globl	Xexceptions
Xexceptions:
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
 * All traps go through here.  Figure out whether we're
 * a) coming from usermode and need the Meltdown mitigation before
 *    jumping to user trap handling followed by AST and
 *    return-to-userspace handling, or
 * b) coming from supervisor mode and can directly jump to kernel
 *    trap handling before returning sans AST or other handling.
 */
KUTEXT_PAGE_START
 	.align	NBPG, 0xcc
	/*
	 * This is the Meltdown alltraps page, which is mapped into
	 * the U-K page tables at the same location as alltraps
	 * below.  For this, the Meltdown case, we must be coming from
	 * userspace so we skip the SEL_RPL test and go directly to
	 * the swapgs+use-scratch+change-cr3 sequence.  Switching to
	 * the kernel page tables (thank you, Intel) will make us
	 * continue at the "movq CPUVAR(KERN_RSP),%rax" after alltraps
	 * below.  In case the CPU speculates past the mov to cr3,
	 * we put a retpoline-style pause-jmp-to-pause loop.
	 */
Xalltraps:
	swapgs
	movq	%rax,CPUVAR(SCRATCH)
	movq	CPUVAR(KERN_CR3),%rax
	movq	%rax,%cr3
0:	pause
	lfence
	jmp	0b
KUTEXT_PAGE_END

KTEXT_PAGE_START
	.align	NBPG, 0xcc
GENTRY(alltraps)
	testb	$SEL_RPL,24(%rsp)
	je	alltraps_kern
	swapgs
	movq	%rax,CPUVAR(SCRATCH)
	.space	(0b - Xalltraps) - (. - alltraps), 0x90

	/*
	 * Traps from userspace
	 */
	INTR_ENTRY_USER
	INTR_SAVE_MOST_GPRS_NO_ADJ
	INTR_CLEAR_GPRS
	sti
	cld
	SMAP_CLAC
	.globl	recall_trap
recall_trap:
	movq	%rsp, %rdi
	call	_C_LABEL(usertrap)
	cli
	jmp	intr_user_exit
END(alltraps)

/*
 * Traps from supervisor mode (kernel)
 */
	_ALIGN_TRAPS
GENTRY(alltraps_kern)
	INTR_ENTRY_KERN
	INTR_SAVE_MOST_GPRS_NO_ADJ
	sti
	cld
	SMAP_CLAC
.Lreal_kern_trap:
#ifdef DIAGNOSTIC
	movl	CPUVAR(ILEVEL),%ebx
#endif /* DIAGNOSTIC */
	movq	%rsp, %rdi
	call	_C_LABEL(kerntrap)
2:	cli
#ifndef DIAGNOSTIC
	INTRFASTEXIT
#else /* DIAGNOSTIC */
	cmpl	CPUVAR(ILEVEL),%ebx
	jne	3f
	INTRFASTEXIT
3:	sti
	leaq	spl_lowered(%rip),%rdi
	movl	CPUVAR(ILEVEL),%esi
	movl	%ebx,%edx
	xorq	%rax,%rax
	call	_C_LABEL(printf)
#ifdef DDB
	int	$3
#endif /* DDB */
	movl	%ebx,CPUVAR(ILEVEL)
	jmp	2b

	.pushsection .rodata
spl_lowered:
	.asciz	"WARNING: SPL NOT LOWERED ON TRAP EXIT %x %x\n"
	.popsection
#endif /* DIAGNOSTIC */
END(alltraps_kern)
KTEXT_PAGE_END


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
 * decide the ipending case with less overhead
 */

/* XXX See comment in locore.s */
#define	XINTR(name,num)		Xintr_##name##num

KUENTRY(x2apic_eoi)
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
KIDTVEC(recurse_lapic_ipi)
	INTR_RECURSE_HWFRAME
	pushq	$0
	subq	$8,%rsp			/* if_ppl filled in below */
	INTR_REENTRY
	jmp	1f
IDTVEC(intr_lapic_ipi)
	pushq	$0
	subq	$8,%rsp			/* if_ppl filled in below */
	INTRENTRY(intr_lapic_ipi)
	CODEPATCH_START
	movl	$0,_C_LABEL(local_apic)+LAPIC_EOI
	CODEPATCH_END(CPTAG_EOI)
	movl	CPUVAR(ILEVEL),%ebx
	cmpl	$IPL_IPI,%ebx
	jae	2f
KIDTVEC_FALLTHROUGH(resume_lapic_ipi)
1:
	incl	CPUVAR(IDEPTH)
	movl	$IPL_IPI,CPUVAR(ILEVEL)
	sti
	cld
	SMAP_CLAC
	movq	%rbx,IF_PPL(%rsp)
	call	_C_LABEL(x86_ipi_handler)
	jmp	_C_LABEL(Xdoreti)
2:
	movq	$(1 << LIR_IPI),%rax
	orq	%rax,CPUVAR(IPENDING)
	INTRFASTEXIT

/*
 * "Fast" IPI handlers.  These are the IPIs which are handled without
 * unblocking interrupts, so no need for 'recurse' or 'resume' entry points
 */
/* invalidate the entire TLB, no PCIDs version */
IDTVEC(ipi_invltlb)
	pushq	%rax

	ioapic_asm_ack()

	movq	%cr3, %rax
	movq	%rax, %cr3

	lock
	decq	tlb_shoot_wait

	popq	%rax
	iretq

/* invalidate a single page, no PCIDs version */
IDTVEC(ipi_invlpg)
	pushq	%rax

	ioapic_asm_ack()

	movq	tlb_shoot_addr1, %rax
	invlpg	(%rax)

	lock
	decq	tlb_shoot_wait

	popq	%rax
	iretq

/* invalidate a range of pages, no PCIDs version */
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

/*
 * Invalidate the userspace PCIDs.
 */
IDTVEC(ipi_invltlb_pcid)
	pushq	%rax

	ioapic_asm_ack()

	/* set the type */
	movl	$INVPCID_PCID,%eax

	/* finish getting space for the INVPCID descriptor */
#if INVPCID_PCID == PCID_PROC
	pushq	%rax
#else
	pushq	$PCID_PROC
#endif

	invpcid	(%rsp),%rax

	/* bump the pcid in the descriptor and invpcid again */
	movl	$PCID_PROC_INTEL,(%rsp)
	invpcid	(%rsp),%rax

	lock
	decq	tlb_shoot_wait

	/* restore the stack */
	popq	%rax
	popq	%rax
	iretq

/*
 * Invalidate a VA in two PCIDs.  Kernel VAs are present in PCIDs 0 and 1,
 * while userspace VAs are present in PCIDs 1 and 2.
 */
IDTVEC(ipi_invlpg_pcid)
	pushq	%rax

	ioapic_asm_ack()

	/* space for the INVPCID descriptor */
	subq	$16,%rsp

	/* set the PCID in the descriptor */
	movl	tlb_shoot_first_pcid,%eax
	movq	%rax,(%rsp)

	/* set the address in the descriptor */
	movq	tlb_shoot_addr1,%rax
	movq	%rax,8(%rsp)

	/* set the type to zero, and invpcid */
	xorl	%eax,%eax
	invpcid	(%rsp),%rax

	/* bump the pcid in the descriptor and invpcid again */
	addl	$1,(%rsp)
	invpcid	(%rsp),%rax

	lock
	decq	tlb_shoot_wait

	/* restore the stack */
	addq	$16,%rsp
	popq	%rax
	iretq

/*
 * Invalidate a range of VA in two PCIDs.  Kernel VAs are present in
 * PCIDs 0 and 1, while userspace VAs are present in PCIDs 1 and 2.
 */
IDTVEC(ipi_invlrange_pcid)
	pushq	%rax
	pushq	%rdx
	pushq	%rcx

	ioapic_asm_ack()

	/* space for the INVPCID descriptor */
	subq	$16,%rsp

	/* set the PCID in the descriptor */
	movl	tlb_shoot_first_pcid,%eax
	movq	%rax,(%rsp)

	/* set up for the loop: load the limit and set the type to zero */
	movq	tlb_shoot_addr2,%rdx
	xorl	%ecx,%ecx

	/* set the address in the descriptor and loop the invalidate */
	movq	tlb_shoot_addr1,%rax
1:	movq	%rax,8(%rsp)
	invpcid	(%rsp),%rcx
	addl	$1,(%rsp)
	invpcid	(%rsp),%rcx
	subl	$1,(%rsp)
	addq	$PAGE_SIZE,%rax
	cmpq	%rdx,%rax
	jb	1b

	lock
	decq	tlb_shoot_wait

	/* restore the stack */
	addq	$16,%rsp
	popq	%rcx
	popq	%rdx
	popq	%rax
	iretq

#endif /* MULTIPROCESSOR */

	/*
	 * Interrupt from the local APIC timer.
	 */
KIDTVEC(recurse_lapic_ltimer)
	INTR_RECURSE_HWFRAME
	pushq	$0
	subq	$8,%rsp			/* if_ppl filled in below */
	INTR_REENTRY
	jmp	1f
IDTVEC(intr_lapic_ltimer)
	pushq	$0
	subq	$8,%rsp			/* if_ppl filled in below */
	INTRENTRY(intr_lapic_ltimer)
	CODEPATCH_START
	movl	$0,_C_LABEL(local_apic)+LAPIC_EOI
	CODEPATCH_END(CPTAG_EOI)
	movl	CPUVAR(ILEVEL),%ebx
	cmpl	$IPL_CLOCK,%ebx
	jae	2f
KIDTVEC_FALLTHROUGH(resume_lapic_ltimer)
1:
	incl	CPUVAR(IDEPTH)
	movl	$IPL_CLOCK,CPUVAR(ILEVEL)
	sti
	cld
	SMAP_CLAC
	movq	%rbx,IF_PPL(%rsp)
	xorq	%rdi,%rdi
	call	_C_LABEL(lapic_clockintr)
	jmp	_C_LABEL(Xdoreti)
2:
	movq	$(1 << LIR_TIMER),%rax
	orq	%rax,CPUVAR(IPENDING)
	INTRFASTEXIT

#if NXEN > 0
/*
 * Xen event channel upcall interrupt handler.
 * Only used when the hypervisor supports direct vector callbacks.
 */
KIDTVEC(recurse_xen_upcall)
	INTR_RECURSE_HWFRAME
	pushq	$0
	subq	$8,%rsp			/* if_ppl filled in below */
	INTR_REENTRY
	jmp	1f
IDTVEC(intr_xen_upcall)
	pushq	$0
	subq	$8,%rsp			/* if_ppl filled in below */
	INTRENTRY(intr_xen_upcall)
	call	_C_LABEL(xen_intr_ack)
	movl	CPUVAR(ILEVEL),%ebx
	cmpl	$IPL_NET,%ebx
	jae	2f
KIDTVEC_FALLTHROUGH(resume_xen_upcall)
1:
	incl	CPUVAR(IDEPTH)
	movl	$IPL_NET,CPUVAR(ILEVEL)
	sti
	cld
	SMAP_CLAC
	movq	%rbx,IF_PPL(%rsp)
	call	_C_LABEL(xen_intr)
	jmp	_C_LABEL(Xdoreti)
2:
	movq	$(1 << LIR_XEN),%rax
	orq	%rax,CPUVAR(IPENDING)
	INTRFASTEXIT
#endif /* NXEN > 0 */

#if NHYPERV > 0
/*
 * Hyperv event channel upcall interrupt handler.
 * Only used when the hypervisor supports direct vector callbacks.
 */
KIDTVEC(recurse_hyperv_upcall)
	INTR_RECURSE_HWFRAME
	pushq	$0
	subq	$8,%rsp			/* if_ppl filled in below */
	INTR_REENTRY
	jmp	1f
IDTVEC(intr_hyperv_upcall)
	pushq	$0
	subq	$8,%rsp			/* if_ppl filled in below */
	INTRENTRY(intr_hyperv_upcall)
	movl	CPUVAR(ILEVEL),%ebx
	cmpl	$IPL_NET,%ebx
	jae	2f
KIDTVEC_FALLTHROUGH(resume_hyperv_upcall)
1:
	incl	CPUVAR(IDEPTH)
	movl	$IPL_NET,CPUVAR(ILEVEL)
	sti
	cld
	SMAP_CLAC
	movq	%rbx,IF_PPL(%rsp)
	call	_C_LABEL(hv_intr)
	jmp	_C_LABEL(Xdoreti)
2:
	movq	$(1 << LIR_HYPERV),%rax
	orq	%rax,CPUVAR(IPENDING)
	INTRFASTEXIT
#endif /* NHYPERV > 0 */
#endif /* NLAPIC > 0 */

#define voidop(num)


/*
 * This macro defines the generic stub code. Its arguments modify it
 * for specific PICs.
 */

#define	INTRSTUB(name, num, early_ack, late_ack, mask, unmask, level_mask) \
KIDTVEC(recurse_##name##num)						;\
	INTR_RECURSE_HWFRAME						;\
	subq	$16,%rsp		/* space for if_{ppl,err} */	;\
	INTR_REENTRY							;\
KIDTVEC_FALLTHROUGH(resume_##name##num)					\
	movq	$IREENT_MAGIC,TF_ERR(%rsp)				;\
	movl	%ebx,%r13d						;\
	movq	CPUVAR(ISOURCES) + (num) * 8, %r14			;\
	movl	IS_MAXLEVEL(%r14),%ebx					;\
	jmp	1f							;\
IDTVEC(intr_##name##num)						;\
	pushq	$0			/* dummy error code */		;\
	subq	$8,%rsp			/* if_ppl filled in below */	;\
	INTRENTRY(intr_##name##num)					;\
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
	movq	%r13,IF_PPL(%rsp)					;\
	movl	%ebx,CPUVAR(ILEVEL)					;\
	sti								;\
	cld								;\
	SMAP_CLAC							;\
	incl	CPUVAR(IDEPTH)						;\
	movq	IS_HANDLERS(%r14),%rbx					;\
6:	/* loop, walking chain of handlers */				\
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
5:	/* successfully handled */					\
	cli								;\
	unmask(num)			/* unmask it in hardware */	;\
	late_ack(num)							;\
	sti								;\
	jmp	_C_LABEL(Xdoreti)	/* lower spl and do ASTs */	;\
7:	/* current IPL > handler's ih_level */				\
	cli								;\
	movq	$(1 << num),%rax					;\
	orq     %rax,CPUVAR(IPENDING)					;\
	level_mask(num)							;\
	late_ack(num)							;\
	sti								;\
	jmp	_C_LABEL(Xdoreti)	/* lower spl and do ASTs */	;\
10:	/* currently masked */						\
	cli								;\
	movq	$(1 << num),%rax					;\
	orq	%rax,CPUVAR(IPENDING)					;\
	level_mask(num)							;\
	late_ack(num)							;\
	INTRFASTEXIT							;\
9:	/* spurious interrupt */					\
	unmask(num)							;\
	late_ack(num)							;\
	testb	$SEL_RPL,TF_CS(%rsp)					;\
	jnz	intr_user_exit						;\
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

	.section .rodata

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

/*
 * Soft interrupt handlers
 */
NENTRY(retpoline_r13)
	JMP_RETPOLINE(r13)

KIDTVEC(softtty)
	movl	$IPL_SOFTTTY, CPUVAR(ILEVEL)
	sti
	incl	CPUVAR(IDEPTH)
	movl	$X86_SOFTINTR_SOFTTTY,%edi
	call	_C_LABEL(softintr_dispatch)
	decl	CPUVAR(IDEPTH)
	jmp	retpoline_r13

KIDTVEC(softnet)
	movl	$IPL_SOFTNET, CPUVAR(ILEVEL)
	sti
	incl	CPUVAR(IDEPTH)
	movl	$X86_SOFTINTR_SOFTNET,%edi
	call	_C_LABEL(softintr_dispatch)
	decl	CPUVAR(IDEPTH)
	jmp	retpoline_r13

KIDTVEC(softclock)
	movl	$IPL_SOFTCLOCK, CPUVAR(ILEVEL)
	sti
	incl	CPUVAR(IDEPTH)
	movl	$X86_SOFTINTR_SOFTCLOCK,%edi
	call	_C_LABEL(softintr_dispatch)
	decl	CPUVAR(IDEPTH)
	jmp	retpoline_r13
