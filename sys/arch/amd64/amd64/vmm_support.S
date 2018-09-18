/*	$OpenBSD: vmm_support.S,v 1.14 2018/09/18 16:02:08 mlarkin Exp $	*/
/*
 * Copyright (c) 2014 Mike Larkin <mlarkin@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "assym.h"
#include <machine/param.h>
#include <machine/asm.h>
#include <machine/psl.h>
#include <machine/specialreg.h>

/*
 * XXX duplicated in vmmvar.h due to song-and-dance with sys/rwlock.h inclusion
 * here 
 */
#define VMX_FAIL_LAUNCH_UNKNOWN 1
#define VMX_FAIL_LAUNCH_INVALID_VMCS 2
#define VMX_FAIL_LAUNCH_VALID_VMCS 3

	.global _C_LABEL(vmxon)
	.global _C_LABEL(vmxoff)
	.global _C_LABEL(vmclear)
	.global _C_LABEL(vmptrld)
	.global _C_LABEL(vmptrst)
	.global _C_LABEL(vmwrite)
	.global _C_LABEL(vmread)
	.global _C_LABEL(invvpid)
	.global _C_LABEL(invept)
	.global _C_LABEL(vmx_enter_guest)
	.global _C_LABEL(vmm_dispatch_intr)
	.global _C_LABEL(svm_enter_guest)

	.text
	.code64
	.align 16,0xcc
_C_LABEL(vmm_dispatch_intr):
	movq	%rsp, %r11	/* r11 = temporary register */
	andq    $0xFFFFFFFFFFFFFFF0, %rsp
	movw	%ss, %ax
	pushq   %rax
	pushq	%r11
	pushfq
	movw	%cs, %ax
	pushq   %rax
	cli
	callq	*%rdi
	ret

_C_LABEL(vmxon):
	RETGUARD_SETUP(vmxon, r11)
	vmxon	(%rdi)
	jz	failed_on
	jc	failed_on
	xorq	%rax, %rax
	RETGUARD_CHECK(vmxon, r11)
	ret
failed_on:
	movq	$0x01, %rax
	RETGUARD_CHECK(vmxon, r11)
	ret

_C_LABEL(vmxoff):
	RETGUARD_SETUP(vmxoff, r11)
	vmxoff
	jz	failed_off
	jc	failed_off
	xorq	%rax, %rax
	RETGUARD_CHECK(vmxoff, r11)
	ret
failed_off:
	movq	$0x01, %rax
	RETGUARD_CHECK(vmxoff, r11)
	ret

_C_LABEL(vmclear):
	RETGUARD_SETUP(vmclear, r11)
	vmclear	(%rdi)
	jz	failed_clear
	jc	failed_clear
	xorq	%rax, %rax
	RETGUARD_CHECK(vmclear, r11)
	ret
failed_clear:
	movq	$0x01, %rax
	RETGUARD_CHECK(vmclear, r11)
	ret

_C_LABEL(vmptrld):
	RETGUARD_SETUP(vmptrld, r11)
	vmptrld	(%rdi)
	jz	failed_ptrld
	jc	failed_ptrld
	xorq	%rax, %rax
	RETGUARD_CHECK(vmptrld, r11)
	ret
failed_ptrld:
	movq	$0x01, %rax
	RETGUARD_CHECK(vmptrld, r11)
	ret

_C_LABEL(vmptrst):
	RETGUARD_SETUP(vmptrst, r11)
	vmptrst	(%rdi)
	jz	failed_ptrst
	jc	failed_ptrst
	xorq	%rax, %rax
	RETGUARD_CHECK(vmptrst, r11)
	ret
failed_ptrst:
	movq	$0x01, %rax
	RETGUARD_CHECK(vmptrst, r11)
	ret

_C_LABEL(vmwrite):
	RETGUARD_SETUP(vmwrite, r11)
	vmwrite	%rsi, %rdi
	jz	failed_write
	jc	failed_write
	xorq	%rax, %rax
	RETGUARD_CHECK(vmwrite, r11)
	ret
failed_write:
	movq	$0x01, %rax
	RETGUARD_CHECK(vmwrite, r11)
	ret

_C_LABEL(vmread):
	RETGUARD_SETUP(vmread, r11)
	vmread	%rdi, (%rsi)
	jz	failed_read
	jc	failed_read
	xorq	%rax, %rax
	RETGUARD_CHECK(vmread, r11)
	ret
failed_read:
	movq	$0x01, %rax
	RETGUARD_CHECK(vmread, r11)
	ret

_C_LABEL(invvpid):
	RETGUARD_SETUP(invvpid, r11)
	invvpid (%rsi), %rdi
	RETGUARD_CHECK(invvpid, r11)
	ret

_C_LABEL(invept):
	RETGUARD_SETUP(invept, r11)
	invept (%rsi), %rdi
	RETGUARD_CHECK(invept, r11)
	ret

_C_LABEL(vmx_enter_guest):
	RETGUARD_SETUP(vmx_enter_guest, r11)
	movq	%rdx, %r8	/* resume flag */
	movq	%rcx, %r9	/* L1DF MSR support */
	testq	%r8, %r8
	jnz skip_init

	/*
	 * XXX make vmx_exit_handler a global and put this in the per-vcpu
	 * init code
	 */
	movq	$VMCS_HOST_IA32_RIP, %rdi
	movq	$vmx_exit_handler_asm, %rax
	vmwrite %rax, %rdi	/* Host RIP */

skip_init:
	/*
	 * XXX use msr list here for restore instead of all this
	 * stack jiggery-pokery
	 */

	pushfq
	popq	%rax
	andq	$(~PSL_I), %rax
	pushq	%rax

	/*
	 * Save (possibly) lazy-switched selectors
	 */
	movw	%es, %ax
	pushw	%ax
	movw	%ds, %ax
	pushw	%ax
	movw	%ss, %ax
	pushw	%ax

	movq	$MSR_FSBASE, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx
	pushw	%fs
	movq	$MSR_GSBASE, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx
	pushw	%gs
	movq	$MSR_KERNELGSBASE, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	/*
	 * Save various MSRs
	 */
	movq	$MSR_STAR, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	movq	$MSR_LSTAR, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	/* XXX - unused? */
	movq	$MSR_CSTAR, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	movq	$MSR_SFMASK, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	RETGUARD_PUSH(r11)

	/* Preserve callee-preserved registers as per AMD64 ABI */
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbp
	pushq	%rbx
	pushq	%rsi		/* Guest Regs Pointer */

	movq	$VMCS_HOST_IA32_RSP, %rdi
	movq	%rsp, %rax
	vmwrite	%rax, %rdi	/* Host RSP */

	/*
	 * Intel L1TF vulnerability fix
	 *
	 * Certain Intel CPUs are broken and allow guest VMs to bypass
	 * EPT entirely as their address harvesting logic treats guest
	 * PTEs as host physical addresses. Flush L1 Dcache to prevent
	 * information leakage by command MSR or manually reading a
	 * bunch of junk in order to fill sizeof(L1 Dcache)*2.
	 *
	 * %r9 (inherited from parameter 4 in %rcx earlier)
	 * determines the flushing requirements
	 *  0 - use manual "junk read" flush
	 *  1 - use MSR command
	 *  2 (VMX_SKIP_L1D_FLUSH) - no flush required on this CPU
	 */
	cmpq	$VMX_SKIP_L1D_FLUSH, %r9
	je	done_flush

	testq	%r9, %r9
	jz	no_l1df_msr

	/* CPU has command MSR */
	movq	$MSR_FLUSH_CMD, %rcx
	xorq	%rdx, %rdx
	movq	$FLUSH_CMD_L1D_FLUSH, %rax
	wrmsr
	jmp	done_flush

no_l1df_msr:
	xorq	%r9, %r9
l1df_tlb_loop:
	/* XXX get the right L1 size from cpuid */
	cmpq	$VMX_L1D_FLUSH_SIZE, %r9
	je	l1df_tlb_done
	movb	l1tf_flush_region(%r9), %al
	addq	$PAGE_SIZE, %r9
	jmp	l1df_tlb_loop

l1df_tlb_done:
	/*
	 * Serialize: ensure previous TLB loads don't pull PTDs
	 * or other PA-containing data into the L1D.
	 */
	xorq	%rax, %rax
	cpuid

	xorq	%r9, %r9
l1df_load_cache:
	movb	l1tf_flush_region(%r9), %al
	/* XXX get the right cacheline size from cpuid */
	addq	$0x40, %r9
	cmpq	$VMX_L1D_FLUSH_SIZE, %r9
	jne	l1df_load_cache	
	lfence
	
done_flush:
	testq	%r8, %r8
	jnz	do_resume

	/* Restore guest registers */
	movq	0x78(%rsi), %rax
	movq	%rax, %cr2
	movq	0x70(%rsi), %r15
	movq	0x68(%rsi), %r14
	movq	0x60(%rsi), %r13
	movq	0x58(%rsi), %r12
	movq	0x50(%rsi), %r11
	movq	0x48(%rsi), %r10
	movq	0x40(%rsi), %r9
	movq	%rsi, %r8
	/* XXX get the right cacheline size from cpuid */
	addq	$0x40, %r8
	clflush	(%r8)
	movq	0x38(%rsi), %r8
	movq	0x30(%rsi), %rbp
	movq	0x28(%rsi), %rdi
	movq	0x20(%rsi), %rdx
	movq	0x18(%rsi), %rcx
	movq	0x10(%rsi), %rbx
	movq	0x08(%rsi), %rax
	clflush	(%rsi)
	movq	0x00(%rsi), %rsi

	vmlaunch
	jmp	fail_launch_or_resume
do_resume:
	/* Restore guest registers */
	movq	0x78(%rsi), %rax
	movq	%rax, %cr2
	movq	0x70(%rsi), %r15
	movq	0x68(%rsi), %r14
	movq	0x60(%rsi), %r13
	movq	0x58(%rsi), %r12
	movq	0x50(%rsi), %r11
	movq	0x48(%rsi), %r10
	movq	0x40(%rsi), %r9
	movq	%rsi, %r8
	/* XXX get the right cacheline size from cpuid */
	addq	$0x40, %r8
	clflush	(%r8)
	movq	0x38(%rsi), %r8
	movq	0x30(%rsi), %rbp
	movq	0x28(%rsi), %rdi
	movq	0x20(%rsi), %rdx
	movq	0x18(%rsi), %rcx
	movq	0x10(%rsi), %rbx
	movq	0x08(%rsi), %rax
	clflush	(%rsi)
	movq	0x00(%rsi), %rsi

	vmresume
fail_launch_or_resume:
	RET_STACK_REFILL_WITH_RCX

	/* Failed launch/resume (fell through) */
	jc fail_launch_invalid_vmcs	/* Invalid VMCS */
	jz fail_launch_valid_vmcs	/* Valid VMCS, failed launch/resume */

	/* Unknown failure mode (not documented as per Intel SDM) */
fail_launch_unknown:
	movq	$VMX_FAIL_LAUNCH_UNKNOWN, %rdi
	popq	%rsi
	jmp	restore_host

fail_launch_invalid_vmcs:
	movq	$VMX_FAIL_LAUNCH_INVALID_VMCS, %rdi
	popq	%rsi
	jmp	restore_host

fail_launch_valid_vmcs:
	movq	$VMCS_INSTRUCTION_ERROR, %rdi
	popq	%rsi
	vmread	%rdi, %rax
	/* XXX check failure of vmread */
	movl	%eax, 0x80(%rsi)
	movq	$VMX_FAIL_LAUNCH_VALID_VMCS, %rdi
	jmp	restore_host

vmx_exit_handler_asm:
	/* Preserve guest registers not saved in VMCS */
	pushq	%rsi
	pushq	%rdi
	movq	0x10(%rsp), %rdi
	movq	0x8(%rsp), %rsi
	movq	%rsi, (%rdi)
	popq	%rdi
	popq	%rsi	/* discard */

	popq	%rsi
	movq	%rax, 0x8(%rsi)
	movq	%rbx, 0x10(%rsi)
	movq	%rcx, 0x18(%rsi)
	movq	%rdx, 0x20(%rsi)
	movq	%rdi, 0x28(%rsi)
	movq	%rbp, 0x30(%rsi)
	movq	%r8, 0x38(%rsi)
	movq	%r9, 0x40(%rsi)
	movq	%r10, 0x48(%rsi)
	movq	%r11, 0x50(%rsi)
	movq	%r12, 0x58(%rsi)
	movq	%r13, 0x60(%rsi)
	movq	%r14, 0x68(%rsi)
	movq	%r15, 0x70(%rsi)
	movq	%cr2, %rax
	movq	%rax, 0x78(%rsi)

	/* %rdi = 0 means we took an exit */
	xorq	%rdi, %rdi

	RET_STACK_REFILL_WITH_RCX

restore_host:
	popq	%rbx
	popq	%rbp
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15

	RETGUARD_POP(r11)
	
	/*
	 * Restore saved MSRs
	 */
	popq	%rdx
	popq	%rax
	movq	$MSR_SFMASK, %rcx
	wrmsr

	/* XXX - unused? */
	popq	%rdx
	popq	%rax
	movq	$MSR_CSTAR, %rcx
	wrmsr
	
	popq	%rdx
	popq	%rax
	movq	$MSR_LSTAR, %rcx
	wrmsr

	popq	%rdx
	popq	%rax
	movq	$MSR_STAR, %rcx
	wrmsr

	/*
	 * popw %gs will reset gsbase to 0, so preserve it
	 * first. This is to accommodate possibly lazy-switched
	 * selectors from above
	 */
	popq	%rdx
	popq	%rax
	movq	$MSR_KERNELGSBASE, %rcx
	wrmsr

	popw	%gs
	popq	%rdx
	popq	%rax
	movq	$MSR_GSBASE, %rcx
	wrmsr	

	popw	%fs
	popq	%rdx
	popq	%rax
	movq	$MSR_FSBASE, %rcx
	wrmsr

	popw	%ax
	movw	%ax, %ss
	popw	%ax
	movw	%ax, %ds
	popw	%ax
	movw	%ax, %es

	popfq

	movq	%rdi, %rax
	RETGUARD_CHECK(vmx_enter_guest, r11)
	ret	
	
_C_LABEL(svm_enter_guest):
	RETGUARD_SETUP(svm_enter_guest, r11)
	clgi
	movq	%rdi, %r8
	pushfq

	pushq	%rdx	/* gdt pointer */

	/*
	 * Save (possibly) lazy-switched selectors
	 */
	strw	%ax
	pushw	%ax
	movw	%es, %ax
	pushw	%ax
	movw	%ds, %ax
	pushw	%ax
	movw	%ss, %ax
	pushw	%ax

	movq	$MSR_FSBASE, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx
	pushw	%fs
	movq	$MSR_GSBASE, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx
	pushw	%gs
	movq	$MSR_KERNELGSBASE, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	/*
	 * Save various MSRs
	 */
	movq	$MSR_STAR, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	movq	$MSR_LSTAR, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	/* XXX - unused? */
	movq	$MSR_CSTAR, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	movq	$MSR_SFMASK, %rcx
	rdmsr
	pushq	%rax
	pushq	%rdx

	RETGUARD_PUSH(r11)

	/* Preserve callee-preserved registers as per AMD64 ABI */
	pushq	%r15
	pushq	%r14
	pushq	%r13
	pushq	%r12
	pushq	%rbp
	pushq	%rbx
	pushq	%rsi		/* Guest Regs Pointer */

	/* Restore guest registers */
	movq	%r8, %rax	/* rax = vmcb pa */
	movq	0x78(%rsi), %r8
	movq	%r8, %cr2
	movq	0x70(%rsi), %r15
	movq	0x68(%rsi), %r14
	movq	0x60(%rsi), %r13
	movq	0x58(%rsi), %r12
	movq	0x50(%rsi), %r11
	movq	0x48(%rsi), %r10
	movq	0x40(%rsi), %r9
	movq	0x38(%rsi), %r8
	movq	0x30(%rsi), %rbp
	movq	0x28(%rsi), %rdi
	movq	0x20(%rsi), %rdx
	movq	0x18(%rsi), %rcx
	movq	0x10(%rsi), %rbx
	/* %rax at 0x08(%rsi) is not needed in SVM */
	movq	0x00(%rsi), %rsi

	vmload	%rax
	vmrun	%rax
	vmsave	%rax

	/* Preserve guest registers not saved in VMCB */
	pushq	%rsi
	pushq	%rdi
	movq	0x10(%rsp), %rdi
	movq	0x8(%rsp), %rsi
	movq	%rsi, (%rdi)
	popq	%rdi
	popq	%rsi	/* discard */

	popq	%rsi
	/* %rax at 0x08(%rsi) is not needed in SVM */
	movq	%rbx, 0x10(%rsi)
	movq	%rcx, 0x18(%rsi)
	movq	%rdx, 0x20(%rsi)
	movq	%rdi, 0x28(%rsi)
	movq	%rbp, 0x30(%rsi)
	movq	%r8, 0x38(%rsi)
	movq	%r9, 0x40(%rsi)
	movq	%r10, 0x48(%rsi)
	movq	%r11, 0x50(%rsi)
	movq	%r12, 0x58(%rsi)
	movq	%r13, 0x60(%rsi)
	movq	%r14, 0x68(%rsi)
	movq	%r15, 0x70(%rsi)
	movq	%cr2, %rax
	movq	%rax, 0x78(%rsi)

	/* %rdi = 0 means we took an exit */
	xorq	%rdi, %rdi

restore_host_svm:
	popq	%rbx
	popq	%rbp
	popq	%r12
	popq	%r13
	popq	%r14
	popq	%r15

	RETGUARD_POP(r11)
	
	/*
	 * Restore saved MSRs
	 */
	popq	%rdx
	popq	%rax
	movq	$MSR_SFMASK, %rcx
	wrmsr

	/* XXX - unused? */
	popq	%rdx
	popq	%rax
	movq	$MSR_CSTAR, %rcx
	wrmsr
	
	popq	%rdx
	popq	%rax
	movq	$MSR_LSTAR, %rcx
	wrmsr

	popq	%rdx
	popq	%rax
	movq	$MSR_STAR, %rcx
	wrmsr

	/*
	 * popw %gs will reset gsbase to 0, so preserve it
	 * first. This is to accommodate possibly lazy-switched
	 * selectors from above
	 */
	cli
	popq	%rdx
	popq	%rax
	movq	$MSR_KERNELGSBASE, %rcx
	wrmsr

	popw	%gs
	popq	%rdx
	popq	%rax
	movq	$MSR_GSBASE, %rcx
	wrmsr	

	popw	%fs
	popq	%rdx
	popq	%rax
	movq	$MSR_FSBASE, %rcx
	wrmsr

	popw	%ax
	movw	%ax, %ss
	popw	%ax
	movw	%ax, %ds
	popw	%ax
	movw	%ax, %es

	xorq	%rax, %rax
	lldtw	%ax		/* Host LDT is always 0 */

	popw	%ax		/* ax = saved TR */

	popq	%rdx
	addq	$0x2, %rdx
	movq	(%rdx), %rdx

	/* rdx = GDTR base addr */
	andb	$0xF9, 5(%rdx, %rax)
	
	ltrw	%ax

	popfq

	movq	%rdi, %rax

	RETGUARD_CHECK(svm_enter_guest, r11)
	ret	
