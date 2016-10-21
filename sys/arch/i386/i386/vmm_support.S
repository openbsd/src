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
#include <machine/asm.h>
#include <machine/specialreg.h>

/*
 * XXX duplicated in vmmvar.h due to song-and-dance with sys/rwlock.h inclusion
 * here 
 */
#define VMX_FAIL_LAUNCH_UNKNOWN 1
#define VMX_FAIL_LAUNCH_INVALID_VMCS 2
#define VMX_FAIL_LAUNCH_VALID_VMCS 3

	.text
	.code32
	.align 16
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

_C_LABEL(vmm_dispatch_intr):
	movl	%esp, %eax
	andl	$0xFFFFFFF0, %esp
	pushl	%ss
	pushl	%eax
	pushfl
	pushl	%cs
	cli
	movl	4(%eax), %eax
	calll	*%eax
	addl	$0x8, %esp
	ret

_C_LABEL(vmxon):
	movl	4(%esp), %eax
	vmxon	(%eax)
	jz	failed_on
	jc	failed_on
	xorl	%eax, %eax
	ret
failed_on:
	movl	$0x01, %eax
	ret

_C_LABEL(vmxoff):
	vmxoff
	jz	failed_off
	jc	failed_off
	xorl	%eax, %eax
	ret
failed_off:
	movl	$0x01, %eax
	ret

_C_LABEL(vmclear):
	movl	0x04(%esp), %eax
	vmclear	(%eax)
	jz	failed_clear
	jc	failed_clear
	xorl	%eax, %eax
	ret
failed_clear:
	movl	$0x01, %eax
	ret

_C_LABEL(vmptrld):
	movl	4(%esp), %eax
	vmptrld	(%eax)
	jz	failed_ptrld
	jc	failed_ptrld
	xorl	%eax, %eax
	ret
failed_ptrld:
	movl	$0x01, %eax
	ret

_C_LABEL(vmptrst):
	movl	0x04(%esp), %eax
	vmptrst	(%eax)
	jz	failed_ptrst
	jc	failed_ptrst
	xorl	%eax, %eax
	ret
failed_ptrst:
	movl	$0x01, %eax
	ret

_C_LABEL(vmwrite):
	movl	0x04(%esp), %eax
	vmwrite	0x08(%esp), %eax
	jz	failed_write
	jc	failed_write
	xorl	%eax, %eax
	ret
failed_write:
	movl	$0x01, %eax
	ret

_C_LABEL(vmread):
	pushl	%ebx
	movl	0x08(%esp), %ebx
	movl	0x0c(%esp), %eax
	vmread	%ebx, (%eax)
	jz	failed_read
	jc	failed_read
	popl	%ebx
	xorl	%eax, %eax
	ret
failed_read:
	popl	%ebx
	movl	$0x01, %eax
	ret

_C_LABEL(invvpid):
	pushl	%ebx
	movl	0x08(%esp), %eax
	movl	0x0c(%esp), %ebx	
	invvpid (%ebx), %eax
	popl	%ebx
	ret

_C_LABEL(invept):
	movl	0x04(%esp), %eax
	invept	0x08(%esp), %eax
	ret

_C_LABEL(vmx_enter_guest):
	pushl	%ebx
	pushl	%ecx
	pushl	%edx
	movl	0x14(%esp), %edx	/* Guest Regs Pointer */
	movl	0x18(%esp), %ebx	/* resume flag */
	testl	%ebx, %ebx
	jnz	skip_init

	/*
	 * XXX make vmx_exit_handler a global and put this in the per-vcpu
	 * init code
	 */
	movl	$VMCS_HOST_IA32_RIP,	%eax
	movl	$vmx_exit_handler_asm,	%ecx
	vmwrite	%ecx, %eax

skip_init:
	pushfl

	strw	%ax
	pushw	%ax
	movw	%es, %ax
	pushw	%ax
	movw	%ds, %ax
	pushw	%ax
	movw	%ss, %ax
	pushw	%ax
	pushw	%fs
	pushw	%gs

	pushl	%ebp
	pushl	%esi
	pushl	%edi
	pushl	%edx			/* Guest Regs Pointer */

	movl	$VMCS_HOST_IA32_RSP, %edi
	movl	%esp, %eax
	vmwrite %eax, %edi

	testl	%ebx, %ebx
	jnz	do_resume

	/* Restore guest registers */
	movl	0x1c(%edx), %eax
	movl	%eax, %cr2
	movl	0x18(%edx), %ebp
	movl	0x14(%edx), %edi
	movl	0x0c(%edx), %ecx
	movl	0x08(%edx), %ebx
	movl	0x04(%edx), %eax
	movl	(%edx), %esi
	movl	0x10(%edx), %edx

	vmlaunch
	jmp	fail_launch_or_resume
do_resume:
	/* Restore guest registers */
	movl	0x1c(%edx), %eax
	movl	%eax, %cr2
	movl	0x18(%edx), %ebp
	movl	0x14(%edx), %edi
	movl	0x0c(%edx), %ecx
	movl	0x08(%edx), %ebx
	movl	0x04(%edx), %eax
	movl	(%edx), %esi
	movl	0x10(%edx), %edx
	vmresume
fail_launch_or_resume:
	/* Failed launch/resume (fell through) */
	jc	fail_launch_invalid_vmcs	/* Invalid VMCS */
	jz	fail_launch_valid_vmcs		/* Valid VMCS, failed launch/resume */

	/* Unknown failure mode (not documented as per Intel SDM) */
	movl	$VMX_FAIL_LAUNCH_UNKNOWN, %eax
	popl	%edx
	jmp	restore_host

fail_launch_invalid_vmcs:
	movl	$VMX_FAIL_LAUNCH_INVALID_VMCS, %eax
	popl	%edx
	jmp	restore_host

fail_launch_valid_vmcs:
	movl	$VMCS_INSTRUCTION_ERROR, %edi
	popl	%edx
	vmread	%edi, %eax
	/* XXX check failure of vmread */
	movl	%eax, 0x20(%edx)
	movl	$VMX_FAIL_LAUNCH_VALID_VMCS, %eax
	jmp	restore_host

vmx_exit_handler_asm:
	/* Preserve guest registers not saved in VMCS */
	pushl	%esi
	pushl	%edi
	movl	0x8(%esp), %edi
	movl	0x4(%esp), %esi
	movl	%esi, (%edi)
	popl	%edi
	popl	%esi	/* discard */

	popl	%esi
	movl	%eax,	0x4(%esi)
	movl	%ebx,	0x8(%esi)
	movl	%ecx,	0xc(%esi)
	movl	%edx,	0x10(%esi)
	movl	%edi,	0x14(%esi)
	movl	%ebp,	0x18(%esi)
	movl	%cr2,	%eax
	movl	%eax,	0x1c(%esi)

restore_host:
	popl	%edi
	popl	%esi
	popl	%ebp

	popw	%gs
	popw	%fs
	popw	%ax
	movw	%ax, %ss
	popw	%ax
	movw	%ax, %ds
	popw	%ax
	movw	%ax, %es
	xorl	%ecx, %ecx
	popw	%cx

	popfl

	movl	0x1c(%esp), %ebx
	leal	(%ebx, %ecx), %eax
	andb	$0xF9, 5(%eax)
	ltr	%cx

	popl	%edx
	popl	%ecx
	popl	%ebx

	xorl	%eax, %eax

	ret
