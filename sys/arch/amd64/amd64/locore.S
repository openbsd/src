/*	$OpenBSD: locore.S,v 1.96 2018/05/20 18:14:01 guenther Exp $	*/
/*	$NetBSD: locore.S,v 1.13 2004/03/25 18:33:17 drochner Exp $	*/

/*
 * Copyright-o-rama!
 */

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
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)locore.s	7.3 (Berkeley) 5/13/91
 */

#include "assym.h"
#include "lapic.h"
#include "ksyms.h"
#include "xen.h"
#include "hyperv.h"

#include <sys/syscall.h>

#include <machine/param.h>
#include <machine/psl.h>
#include <machine/segments.h>
#include <machine/specialreg.h>
#include <machine/frameasm.h>

#if NLAPIC > 0
#include <machine/i82489reg.h>
#endif

/*
 * override user-land alignment before including asm.h
 */
#define	ALIGN_DATA	.align	8,0xcc
#define ALIGN_TEXT	.align 16,0x90
#define _ALIGN_TEXT	ALIGN_TEXT

#include <machine/asm.h>

#define SET_CURPROC(proc,cpu)			\
	movq	CPUVAR(SELF),cpu	;	\
	movq	proc,CPUVAR(CURPROC)      ;	\
	movq	cpu,P_CPU(proc)

#define GET_CURPCB(reg)			movq	CPUVAR(CURPCB),reg      
#define SET_CURPCB(reg)			movq	reg,CPUVAR(CURPCB)


/*
 * Initialization
 */
	.data

#if NLAPIC > 0 
	.align  NBPG, 0xcc
	.globl _C_LABEL(local_apic), _C_LABEL(lapic_id), _C_LABEL(lapic_tpr)
_C_LABEL(local_apic):
	.space  LAPIC_ID
_C_LABEL(lapic_id):
	.long   0x00000000
	.space  LAPIC_TPRI-(LAPIC_ID+4)
_C_LABEL(lapic_tpr):
	.space  LAPIC_PPRI-LAPIC_TPRI
_C_LABEL(lapic_ppr):
	.space  LAPIC_ISR-LAPIC_PPRI 
_C_LABEL(lapic_isr):
	.space  NBPG-LAPIC_ISR
#endif

	.globl	_C_LABEL(cpu_id),_C_LABEL(cpu_vendor)
	.globl	_C_LABEL(cpuid_level),_C_LABEL(cpu_feature)
	.globl	_C_LABEL(cpu_ebxfeature)
	.globl	_C_LABEL(cpu_ecxfeature),_C_LABEL(ecpu_ecxfeature)
	.globl	_C_LABEL(cpu_perf_eax)
	.globl	_C_LABEL(cpu_perf_ebx)
	.globl	_C_LABEL(cpu_perf_edx)
	.globl	_C_LABEL(cpu_apmi_edx)
	.globl	_C_LABEL(ssym),_C_LABEL(esym),_C_LABEL(boothowto)
	.globl	_C_LABEL(bootdev)
	.globl	_C_LABEL(bootinfo), _C_LABEL(bootinfo_size), _C_LABEL(atdevbase)
	.globl	_C_LABEL(proc0paddr),_C_LABEL(PTDpaddr)
	.globl	_C_LABEL(biosbasemem)
	.globl	_C_LABEL(bootapiver)
	.globl	_C_LABEL(pg_nx)
	.globl	_C_LABEL(pg_g_kern)
	.globl	_C_LABEL(cpu_meltdown)
_C_LABEL(cpu_id):	.long	0	# saved from `cpuid' instruction
_C_LABEL(cpu_feature):	.long	0	# feature flags from 'cpuid'
					#   instruction
_C_LABEL(cpu_ebxfeature):.long	0	# ext. ebx feature flags from 'cpuid'
_C_LABEL(cpu_ecxfeature):.long	0	# ext. ecx feature flags from 'cpuid'
_C_LABEL(ecpu_ecxfeature):.long	0	# extended ecx feature flags
_C_LABEL(cpu_perf_eax):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_perf_ebx):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_perf_edx):	.long	0	# arch. perf. mon. flags from 'cpuid'
_C_LABEL(cpu_apmi_edx):	.long	0	# adv. power mgmt. info. from 'cpuid'
_C_LABEL(cpuid_level):	.long	-1	# max. level accepted by 'cpuid'
					#   instruction
_C_LABEL(cpu_vendor):	.space	16	# vendor string returned by `cpuid'
					#   instruction
_C_LABEL(ssym):		.quad	0	# ptr to start of syms
_C_LABEL(esym):		.quad	0	# ptr to end of syms
_C_LABEL(atdevbase):	.quad	0	# location of start of iomem in virtual
_C_LABEL(bootapiver):	.long	0	# /boot API version
_C_LABEL(bootdev):	.long	0	# device we booted from
_C_LABEL(proc0paddr):	.quad	0
_C_LABEL(PTDpaddr):	.quad	0	# paddr of PTD, for libkvm
#ifndef REALBASEMEM
_C_LABEL(biosbasemem):	.long	0	# base memory reported by BIOS
#else
_C_LABEL(biosbasemem):	.long	REALBASEMEM
#endif
#ifndef REALEXTMEM
_C_LABEL(biosextmem):	.long	0	# extended memory reported by BIOS
#else
_C_LABEL(biosextmem):	.long	REALEXTMEM
#endif
_C_LABEL(pg_nx):	.quad	0	# NX PTE bit (if CPU supports)
_C_LABEL(pg_g_kern):	.quad	0	# 0x100 if global pages should be used
					# in kernel mappings, 0 otherwise (for
					# insecure CPUs)
_C_LABEL(cpu_meltdown):	.long	0	# 1 if this CPU has Meltdown

#define	_RELOC(x)	((x) - KERNBASE)
#define	RELOC(x)	_RELOC(_C_LABEL(x))

	.globl	gdt64

gdt64:
	.word	gdt64_end-gdt64_start-1
	.quad	_RELOC(gdt64_start)
.align 64, 0xcc

gdt64_start:
	.quad 0x0000000000000000	/* always empty */
	.quad 0x00af9a000000ffff	/* kernel CS */
	.quad 0x00cf92000000ffff	/* kernel DS */
gdt64_end:

/*
 * Some hackage to deal with 64bit symbols in 32 bit mode.
 * This may not be needed if things are cleaned up a little.
 */

/*****************************************************************************/

/*
 * Signal trampoline; copied to a page mapped into userspace.
 * gdb's backtrace logic matches against the instructions in this.
 */
	.section .rodata
	.globl	_C_LABEL(sigcode)
_C_LABEL(sigcode):
	call	*%rax

	movq	%rsp,%rdi
	pushq	%rdi			/* fake return address */
	movq	$SYS_sigreturn,%rax
	syscall
	.globl	_C_LABEL(sigcoderet)
_C_LABEL(sigcoderet):
	movq	$SYS_exit,%rax
	syscall
	.globl	_C_LABEL(esigcode)
_C_LABEL(esigcode):

	.globl	_C_LABEL(sigfill)
_C_LABEL(sigfill):
	int3
_C_LABEL(esigfill):
	.globl	_C_LABEL(sigfillsiz)
_C_LABEL(sigfillsiz):
	.long	_C_LABEL(esigfill) - _C_LABEL(sigfill)

	.text
/*
 * void lgdt(struct region_descriptor *rdp);
 * Change the global descriptor table.
 */
NENTRY(lgdt)
	/* Reload the descriptor table. */
	movq	%rdi,%rax
	lgdt	(%rax)
	/* Flush the prefetch q. */
	jmp	1f
	nop
1:	/* Reload "stale" selectors. */
	movl	$GSEL(GDATA_SEL, SEL_KPL),%eax
	movl	%eax,%ds
	movl	%eax,%es
	movl	%eax,%ss
	/* Reload code selector by doing intersegment return. */
	popq	%rax
	pushq	$GSEL(GCODE_SEL, SEL_KPL)
	pushq	%rax
	lretq

ENTRY(setjmp)
	/*
	 * Only save registers that must be preserved across function
	 * calls according to the ABI (%rbx, %rsp, %rbp, %r12-%r15)
	 * and %rip.
	 */
	movq	%rdi,%rax
	movq	%rbx,(%rax)
	movq	%rsp,8(%rax)
	movq	%rbp,16(%rax)
	movq	%r12,24(%rax)
	movq	%r13,32(%rax)
	movq	%r14,40(%rax)
	movq	%r15,48(%rax)
	movq	(%rsp),%rdx
	movq	%rdx,56(%rax)
	xorl	%eax,%eax
	ret

ENTRY(longjmp)
	movq	%rdi,%rax
	movq	(%rax),%rbx
	movq	8(%rax),%rsp
	movq	16(%rax),%rbp
	movq	24(%rax),%r12
	movq	32(%rax),%r13
	movq	40(%rax),%r14
	movq	48(%rax),%r15
	movq	56(%rax),%rdx
	movq	%rdx,(%rsp)
	xorl	%eax,%eax
	incl	%eax
	ret

/*****************************************************************************/

/*
 * int cpu_switchto(struct proc *old, struct proc *new)
 * Switch from "old" proc to "new".
 */
ENTRY(cpu_switchto)
	pushq	%rbx
	pushq	%rbp
	pushq	%r12
	pushq	%r13
	pushq	%r14
	pushq	%r15

	movq	%rdi, %r13
	movq	%rsi, %r12

	/* Record new proc. */
	movb	$SONPROC,P_STAT(%r12)	# p->p_stat = SONPROC
	SET_CURPROC(%r12,%rcx)

	movl	CPUVAR(CPUID),%edi

	/* If old proc exited, don't bother. */
	testq	%r13,%r13
	jz	switch_exited

	/*
	 * Save old context.
	 *
	 * Registers:
	 *   %rax, %rcx - scratch
	 *   %r13 - old proc, then old pcb
	 *   %r12 - new proc
	 *   %edi - cpuid
	 */

	movq	P_ADDR(%r13),%r13

	/* clear the old pmap's bit for the cpu */
	movq	PCB_PMAP(%r13),%rcx
	lock
	btrq	%rdi,PM_CPUS(%rcx)

	/* Save stack pointers. */
	movq	%rsp,PCB_RSP(%r13)
	movq	%rbp,PCB_RBP(%r13)

switch_exited:
	/* did old proc run in userspace?  then reset the segment regs */
	btrl	$CPUF_USERSEGS_BIT, CPUVAR(FLAGS)
	jnc	restore_saved

	/* set %ds, %es, %fs, and %gs to expected value to prevent info leak */
	movw	$(GSEL(GUDATA_SEL, SEL_UPL)),%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%fs
	cli			/* block interrupts when on user GS.base */
	swapgs			/* switch from kernel to user GS.base */
	movw	%ax,%gs		/* set %gs to UDATA and GS.base to 0 */
	swapgs			/* back to kernel GS.base */

restore_saved:
	/*
	 * Restore saved context.
	 *
	 * Registers:
	 *   %rax, %rcx, %rdx - scratch
	 *   %r13 - new pcb
	 *   %r12 - new process
	 */

	/* No interrupts while loading new state. */
	cli
	movq	P_ADDR(%r12),%r13

	/* Restore stack pointers. */
	movq	PCB_RSP(%r13),%rsp
	movq	PCB_RBP(%r13),%rbp

	movq	PCB_CR3(%r13),%rax
	movq	%rax,%cr3			/* %rax used below too */

	/* Don't bother with the rest if switching to a system process. */
	testl	$P_SYSTEM,P_FLAG(%r12)
	jnz	switch_restored

	/* record the bits needed for future U-->K transition */
	movq	PCB_KSTACK(%r13),%rdx
	subq	$FRAMESIZE,%rdx
	movq	%rdx,CPUVAR(KERN_RSP)
	movq	PCB_PMAP(%r13),%rcx

	/*
	 * Meltdown: iff we're doing separate U+K and U-K page tables,
	 * then record them in cpu_info for easy access in syscall and
	 * interrupt trampolines.  XXX code patch this
	 */

	movq	PM_PDIRPA_INTEL(%rcx),%rdx
	testq	%rdx,%rdx
	jz	0f			/* yay, no intel suckiness */
	movq	%rax,CPUVAR(KERN_CR3)
	movq	%rdx,CPUVAR(USER_CR3)
0:

	/* set the new pmap's bit for the cpu */
	movl	CPUVAR(CPUID),%edi
	lock
	btsq	%rdi,PM_CPUS(%rcx)
#ifdef DIAGNOSTIC
	jc	_C_LABEL(switch_pmcpu_set)
#endif

switch_restored:
	/* Restore cr0 (including FPU state). */
	movl	PCB_CR0(%r13),%ecx
#ifdef MULTIPROCESSOR
	movq	PCB_FPCPU(%r13),%r8
	cmpq	CPUVAR(SELF),%r8
	jz	1f
	orl	$CR0_TS,%ecx
1:
#endif
	movq	%rcx,%cr0

	SET_CURPCB(%r13)

	/* Interrupts are okay again. */
	sti

switch_return:

	popq	%r15
	popq	%r14
	popq	%r13
	popq	%r12
	popq	%rbp
	popq	%rbx
	ret

ENTRY(cpu_idle_enter)
	movq	_C_LABEL(cpu_idle_enter_fcn),%rax
	cmpq	$0,%rax
	je	1f
	jmpq	*%rax
1:
	ret

ENTRY(cpu_idle_cycle)
	movq	_C_LABEL(cpu_idle_cycle_fcn),%rax
	cmpq	$0,%rax
	je	1f
	call	*%rax
	ret
1:
	sti
	hlt
	ret

ENTRY(cpu_idle_leave)
	movq	_C_LABEL(cpu_idle_leave_fcn),%rax
	cmpq	$0,%rax
	je	1f
	jmpq	*%rax
1:
	ret

	.globl	_C_LABEL(panic)

#ifdef DIAGNOSTIC
NENTRY(switch_pmcpu_set)
	leaq	switch_active(%rip),%rdi
	call	_C_LABEL(panic)
	/* NOTREACHED */

	.section .rodata
switch_active:
	.asciz	"activate already active pmap"
	.text
#endif /* DIAGNOSTIC */
/*
 * savectx(struct pcb *pcb);
 * Update pcb, saving current processor state.
 */
ENTRY(savectx)
	/* Save stack pointers. */
	movq	%rsp,PCB_RSP(%rdi)
	movq	%rbp,PCB_RBP(%rdi)

	ret

IDTVEC(syscall32)
	sysret		/* go away please */

/*
 * syscall insn entry.
 */
IDTVEC(syscall)
	/*
	 * Enter here with interrupts blocked; %rcx contains the caller's
	 * %rip and the original rflags has been copied to %r11.  %cs and
	 * %ss have been updated to the kernel segments, but %rsp is still
	 * the user-space value.
	 * First order of business is to swap to the kernel gs.base so that
	 * we can access our struct cpu_info and use the scratch space there
	 * to switch to the kernel page tables (thank you, Intel), then
	 * switch to our kernel stack.  Once that's in place we can
	 * unblock interrupts and save the rest of the syscall frame.
	 */
	swapgs
	movq	%r15,CPUVAR(SCRATCH)
	movq	CPUVAR(KERN_CR3),%r15
	testq	%r15,%r15
	jz	Xsyscall_untramp
	movq	%r15,%cr3
	jmp	Xsyscall_untramp

NENTRY(Xsyscall_untramp)
	movq	CPUVAR(KERN_RSP),%r15
	xchgq	%r15,%rsp
	sti

	/*
	 * XXX don't need this whole frame, split of the
	 * syscall frame and trapframe is needed.
	 * First, leave some room for the trapno, error,
	 * ss:rsp, etc, so that all GP registers can be
	 * saved. Then, fill in the rest.
	 */
	movq	$(GSEL(GUDATA_SEL, SEL_UPL)),TF_SS(%rsp)
	movq	%r15,TF_RSP(%rsp)
	movq	CPUVAR(SCRATCH),%r15
	INTR_SAVE_MOST_GPRS_NO_ADJ
	movq	%rcx,TF_RCX(%rsp)
	movq	%r11, TF_RFLAGS(%rsp)	/* old rflags from syscall insn */
	movq	$(GSEL(GUCODE_SEL, SEL_UPL)), TF_CS(%rsp)
	movq	%rcx,TF_RIP(%rsp)
	movq	%rax,TF_ERR(%rsp)	/* stash syscall # for SPL check */

	movq	CPUVAR(CURPROC),%r14
	movq	%rsp,P_MD_REGS(%r14)	# save pointer to frame
	andl	$~MDP_IRET,P_MD_FLAGS(%r14)
	movq	%rsp,%rdi
	call	_C_LABEL(syscall)

.Lsyscall_check_asts:
	/* Check for ASTs on exit to user mode. */
	cli
	CHECK_ASTPENDING(%r11)
	je	2f
	CLEAR_ASTPENDING(%r11)
	sti
	movq	%rsp,%rdi
	call	_C_LABEL(ast)
	jmp	.Lsyscall_check_asts

2:
#ifdef DIAGNOSTIC
	cmpl	$IPL_NONE,CPUVAR(ILEVEL)
	jne	.Lsyscall_spl_not_lowered
#endif /* DIAGNOSTIC */

	/* Could registers have been changed that require an iretq? */
	testl	$MDP_IRET, P_MD_FLAGS(%r14)
	jne	intr_fast_exit

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

	/* Restore FS.base if it's not already in the CPU */
	btsl	$CPUF_USERSEGS_BIT,CPUVAR(FLAGS)
	jc	99f
	movq	CPUVAR(CURPCB),%rdx
	movq	PCB_FSBASE(%rdx),%rax
	movq	%rax,%rdx
	shrq	$32,%rdx
	movl	$MSR_FSBASE,%ecx
	wrmsr
99:

	/*
	 * We need to finish reading from the trapframe, then switch
	 * to the user page tables, swapgs, and return.  We need
	 * to get the final value for the register that was used
	 * for the mov to %cr3 from somewhere accessible on the
	 * user page tables, so save it in CPUVAR(SCRATCH) across
	 * the switch.
	 */
	movq	TF_RDX(%rsp),%rdx
	movq	TF_RAX(%rsp),%rax
	movq	%rax,CPUVAR(SCRATCH)
	movq	CPUVAR(USER_CR3),%rax

	movq	TF_RIP(%rsp),%rcx
	movq	TF_RFLAGS(%rsp),%r11
	movq	TF_RSP(%rsp),%rsp
	testq	%rax,%rax
	jz	1f
	jmp	syscall_trampback

KUENTRY(syscall_trampback)
	movq	%rax,%cr3
1:	movq	CPUVAR(SCRATCH),%rax
	swapgs
	sysretq

	.text

#ifdef DIAGNOSTIC
.Lsyscall_spl_not_lowered:
	leaq	spl_lowered(%rip), %rdi
	movl	TF_ERR(%rsp),%esi	/* syscall # stashed above */
	movl	TF_RDI(%rsp),%edx
	movl	%ebx,%ecx
	movl	CPUVAR(ILEVEL),%r8d
	xorq	%rax,%rax
	call	_C_LABEL(printf)
#ifdef DDB
	int	$3
#endif /* DDB */
	movl	$IPL_NONE,CPUVAR(ILEVEL)
	jmp	.Lsyscall_check_asts

	.section .rodata
spl_lowered:
	.asciz	"WARNING: SPL NOT LOWERED ON SYSCALL %d %d EXIT %x %x\n"
	.text
#endif

NENTRY(proc_trampoline)
#ifdef MULTIPROCESSOR
	call	_C_LABEL(proc_trampoline_mp)
#endif
	movl	$IPL_NONE,CPUVAR(ILEVEL)
	movq	%r13,%rdi
	call	*%r12
	movq	CPUVAR(CURPROC),%r14
	jmp	.Lsyscall_check_asts


/*
 * Return via iretq, for real interrupts and signal returns
 */
NENTRY(intr_fast_exit)
#ifdef DIAGNOSTIC
	pushfq
	popq	%rdx
	testq	$PSL_I,%rdx
	jnz	.Lintr_exit_not_blocked
#endif /* DIAGNOSTIC */
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

	testq	$SEL_RPL,TF_CS(%rsp)
	je	intr_exit_recurse		/* returning back to kernel? */

	/* returning to userspace.  XXX fix up iret frame here */

	/* restore FS.base if it's not already in the CPU */
	btsl	$CPUF_USERSEGS_BIT,CPUVAR(FLAGS)
	jc	99f
	movq	CPUVAR(CURPCB),%rdx		/* for below */
	movq	PCB_FSBASE(%rdx),%rax
	movq	%rax,%rdx
	shrq	$32,%rdx
	movl	$MSR_FSBASE,%ecx
	wrmsr
99:
	/*
	 * Returning to userspace.  We need to go things in this order:
	 *  - update the iret frame from the trapframe
	 *  - finish reading from the trapframe
	 *  - switch to the trampoline stack
	 *  - jump to the .kutext segment
	 *  - switch to the user page tables
	 *  - swapgs
	 *  - iretq
	 * To get the final value for the register that was used
	 * for the mov to %cr3, we need access to somewhere accessible
	 * on the user page tables, so we save it in CPUVAR(SCRATCH)
	 * across the switch.
	 */
	/* update iret frame */
	movq	CPUVAR(INTR_RSP),%rdx
	movq	$(GSEL(GUCODE_SEL,SEL_UPL)),IRETQ_CS(%rdx)
	movq	TF_RIP(%rsp),%rax
	movq	%rax,IRETQ_RIP(%rdx)
	movq	TF_RFLAGS(%rsp),%rax
	movq	%rax,IRETQ_RFLAGS(%rdx)
	movq	TF_RSP(%rsp),%rax
	movq	%rax,IRETQ_RSP(%rdx)
	movq	$(GSEL(GUDATA_SEL,SEL_UPL)),IRETQ_SS(%rdx)
	/* finish with the trap frame */
	movq	TF_RAX(%rsp),%rax
	movq	%rax,CPUVAR(SCRATCH)
	movq	TF_RCX(%rsp),%rcx
	movq	TF_R11(%rsp),%r11
	/* switch to the trampoline stack */
	xchgq	%rdx,%rsp
	movq	TF_RDX(%rdx),%rdx
	movq	CPUVAR(USER_CR3),%rax
	testq	%rax,%rax
	jz	1f
	jmp	iretq_tramp

KUENTRY(iretq_tramp)
	movq	%rax,%cr3
1:	movq	CPUVAR(SCRATCH),%rax
	swapgs

	.globl	_C_LABEL(doreti_iret)
_C_LABEL(doreti_iret):
	iretq

NENTRY(intr_exit_recurse)
	movq	TF_RDX(%rsp),%rdx
	movq	TF_RCX(%rsp),%rcx
	movq	TF_R11(%rsp),%r11
	movq	TF_RAX(%rsp),%rax

#if !defined(GPROF) && defined(DDBPROF)
	/*
	 * If we are returning from a probe trap we need to fix the
	 * stack layout and emulate the patched instruction.
	 *
	 * The code below does that by trashing %rax, so it MUST be
	 * restored afterward.
	 */
	cmpl	$INTR_FAKE_TRAP, TF_ERR(%rsp)
	je	.Lprobe_fixup
#endif /* !defined(GPROF) && defined(DDBPROF) */

	addq	$TF_RIP,%rsp
	iretq


#if !defined(GPROF) && defined(DDBPROF)
.Lprobe_fixup:
	/* Reserve enough room to emulate "pushq %rbp". */
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

	/* Write back overwritten %rax */
	movq	(TF_RAX + 16)(%rsp),%rax

	addq	$TF_RIP,%rsp
	iretq
#endif /* !defined(GPROF) && defined(DDBPROF) */
	.text

#ifdef DIAGNOSTIC
.Lintr_exit_not_blocked:
	xchgw	%bx, %bx
	movl	warn_once(%rip),%edi
	testl	%edi,%edi
	jnz	1f
	incl	%edi
	movl	%edi,warn_once(%rip)
	leaq	.Lnot_blocked(%rip),%rdi
	call	_C_LABEL(printf)
#ifdef DDB
	int	$3
#endif /* DDB */
1:	cli
	jmp	intr_fast_exit

	.data
.global warn_once
warn_once:
	.long	0
	.section .rodata
.Lnot_blocked:
	.asciz	"WARNING: INTERRUPTS NOT BLOCKED ON INTERRUPT RETURN: 0x%x 0x%x\n"
	.text
#endif

ENTRY(xrstor_user)
	movq	%rsi, %rdx
	movl	%esi, %eax
	shrq	$32, %rdx
	.globl	xrstor_fault
xrstor_fault:
	xrstor	(%rdi)
	xorl	%eax, %eax
	ret
NENTRY(xrstor_resume)
	movl	$1, %eax
	ret

ENTRY(pagezero)
	movq    $-PAGE_SIZE,%rdx
	subq    %rdx,%rdi
	xorq    %rax,%rax
1:
	movnti  %rax,(%rdi,%rdx)
	movnti  %rax,8(%rdi,%rdx)
	movnti  %rax,16(%rdi,%rdx)
	movnti  %rax,24(%rdi,%rdx)
	addq    $32,%rdx
	jne     1b
	sfence
	ret

#if NXEN > 0
	/* Hypercall page needs to be page aligned */
	.text
	.align	NBPG, 0xcc
	.globl	_C_LABEL(xen_hypercall_page)
_C_LABEL(xen_hypercall_page):
	.skip	0x1000, 0xcc
#endif /* NXEN > 0 */

#if NHYPERV > 0
	/* Hypercall page needs to be page aligned */
	.text
	.align	NBPG, 0xcc
	.globl	_C_LABEL(hv_hypercall_page)
_C_LABEL(hv_hypercall_page):
	.skip	0x1000, 0xcc
#endif /* NXEN > 0 */
