/* $OpenBSD: acpi_wakecode.S,v 1.7 2009/11/26 00:14:11 mlarkin Exp $ */
/*
 * Copyright (c) 2001 Takanori Watanabe <takawata@jp.freebsd.org>
 * Copyright (c) 2001 Mitsuru IWASAKI <iwasaki@jp.freebsd.org>
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
/*
 * Copyright (c) 2008, 2009 Mike Larkin <mlarkin@openbsd.org>
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

#include "acpi.h"

#if NACPI > 0
#ifndef SMALL_KERNEL

#define _ACPI_WAKECODE

#include "assym.h"
#include <machine/asm.h>
#include <machine/specialreg.h>
#include <machine/param.h>
#include <machine/segments.h>
#include <dev/acpi/acpivar.h>

#define _ACPI_TRMP_LABEL(a) a = . - _C_LABEL(acpi_real_mode_resume) + ACPI_TRAMPOLINE
#define _ACPI_TRMP_OFFSET(a) a = . - _C_LABEL(acpi_real_mode_resume)
#define _ACPI_RM_SEGMENT (ACPI_TRAMPOLINE >> 4)

/*
 * On wakeup, we'll start executing at acpi_real_mode_resume.
 * This is based on the wakeup vector previously stored with
 * ACPI before we went to sleep. ACPI's wakeup vector is a
 * physical address - in our case, it's calculated and mapped
 * by the kernel and stuffed into a low page early in the boot
 * process. 
 *
 * We wakeup in real mode, at some phys addr based on the ACPI
 * specification (cs = phys>>8, ip = phys & 0xF). For example,
 * if our phys addr is 0x4000, we'd have cs=0x0400,ip=0
 *
 * The wakeup code needs to do the following:
 *     1. Reenable the video display
 *     2. Enter 32 bit protected mode
 *     3. Reenable paging
 *     4. Enter long mode
 *     5. Restore saved CPU registers
 */

	.text
	.code16
	.align 4,0
	.global _C_LABEL(acpi_real_mode_resume)
	.global _C_LABEL(acpi_protected_mode_resume)
	.global _C_LABEL(acpi_long_mode_resume)
	.global _C_LABEL(acpi_resume_end)
	.global _C_LABEL(acpi_pdirpa)
_C_LABEL(acpi_real_mode_resume):
	nop
	cli
	cld

	/* Some BIOS vendors screw up the gdt, make sure we clean it */
	movw	$0x10,	%cx
	lgdtl	%cs:clean_gdt
	movl	%cr0,	%eax
	orb	$(CR0_PE), %al
	movl	%eax,	%cr0
	jmp	1f
1:	ljmpw	$0x8, $clean1

_ACPI_TRMP_OFFSET(clean1)
	movw	%cx,	%ds	
	movw	%cx,	%es
	movw	%cx,	%ss
	movw	%cx,	%fs
	movw	%cx,	%gs
	
	andb	$~(CR0_PE), %al
	movl	%eax,	%cr0
	ljmpw	$_ACPI_RM_SEGMENT , $clean2

_ACPI_TRMP_OFFSET(clean2)

	/*
	 * Set up segment registers for real mode. 
	 * We'll only be in real mode for a moment, and we don't have
	 * ant real dependencies on data or stack, so we'll just use
	 * the code segment for data and stack (eg, a 64k memory space).
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%ss
	lidtl	clean_idt

	/*
	 * Set up stack to grow down from offset 0x0FFE.
	 * We will only be doing a few push/pops and no calls in real 
	 * mode, so as long as the real mode code in the segment 
	 * plus stack doesn't exceed 0x0FFE (4094) bytes, we'll be ok.
	 */
	movw	$0x0FFE,%sp

	/*
	 * Clear flags
	 */
	pushl	$0
	popfl

	/*
	 * Reset the video hardware (as best as we can).
	 * We call the video bios at c000:0003, similar to
	 * what the BIOS does on a machine restart.
	 * Note that this will only reset the video card, 
	 * and may not enable LCDs or other attached displays.
	 *
	 * This will also put the hardware in "factory default"
	 * display mode, which may not match what we had
	 * when we went to sleep. On many machines (specifically
	 * laptops), we might not restore the proper VGA mode
	 * on resume. Caveat emptor.
	 */
	/* jmp	nobiosreset */ 	/* XXX make this a tunable */
	lcall	$0xc000,$3

	/*
	 * Restore our segment registers in case the call to 
	 * reset the video hardware clobbered them.
	 */
	movw	%cs,%ax
	movw	%ax,%ds
	movw	%ax,%ss
nobiosreset:

	/*
	 * Set up esi to point to start of current routine's CS.
	 */
	xorl    %esi,%esi
	movw    %cs,%si
	shll    $4,%esi

	/*
	 * Flush instruction prefetch queue
	 */
	jmp     1f
1:	jmp     1f
1:

	/*
	 * We're about to enter protected mode, so we need a GDT for that.
	 * Set up a temporary GDT describing 2 segments, one for code
	 * extending from 0x00000000-0xffffffff and one for data
	 * with the same range. This GDT will only be in use for a short
	 * time, until we restore the saved GDT that we had when we went
	 * to sleep (although on i386, the saved GDT will most likely
	 * represent something similar based on machine/segment.h).
	 */ 
	data32 addr32 lgdt	tmp_gdt

	/*
	 * Enable protected mode by setting the PE bit in CR0
	 */
	mov	%cr0,%eax
	orl	$(CR0_PE),%eax
	mov	%eax,%cr0

	/*
	 * Force CPU into protected mode 
	 * by making an intersegment jump (to ourselves, just a few lines
	 * down from here. We rely on the kernel to fixup the jump
	 * target addres previously. 
	 *
	 */
	ljmpl	$0x8, $acpi_protected_mode_trampoline

	.code32
	.align 16
_ACPI_TRMP_LABEL(acpi_protected_mode_trampoline)
_C_LABEL(acpi_protected_mode_resume):
	nop

	/*
	 * We're in protected mode now, without paging enabled.
	 *
	 * Set up segment selectors for protected mode.
	 * We've already set up our cs via the intersegment jump earlier,
	 * but we need to set ds,es,fs,gs,ss to all point to the 
	 * 4GB flat data segment we defined earlier.
	 */
	movw	$GSEL(GDATA_SEL,SEL_KPL),%ax
	movw	%ax,%ds
	movw	%ax,%es
	movw	%ax,%gs
	movw	%ax,%ss
	movw	%ax,%fs

	/*
	 * Reset ESP based on protected mode. We can do this here
	 * because we haven't put anything on the stack via a
	 * call or push that we haven't cleaned up already.
	 */
	movl    %esi, %esp
	addl    $0x0FFE, %esp

	/* Set CR4 to something sane for entry into long mode */
	mov	$(CR4_PAE|CR4_OSFXSR|CR4_OSXMMEXCPT|CR4_PSE),%eax
	mov	%eax,%cr4

	/*
	 * Stash away our previously saved EFER in EBX.
	 * We have to make sure we don't write to any
	 * of the EFER reserved bits, so we zero those
	 * out here.
	 */
	movl	acpi_saved_efer, %ebx
	andl 	$(EFER_LME | EFER_LMA | EFER_NXE | EFER_SCE), %ebx	

	/*
	 * Set up a temporary long mode GDT describing 2
	 * segments, one for code and one for data.
	 */
	lgdt 	tmp_gdt64	

	/* Prepare to enter long mode by enabling LME in EFER */
	movl	$MSR_EFER, %ecx
	rdmsr
	orl	$EFER_LME, %eax
	wrmsr

	/* Reenable paging using temporary cr3 */
	movl	$acpi_pdirpa, %eax
	movl	(%eax), %eax
	movl	%eax, %cr3

	/* Flush the prefetch queue again */
	jmp 	1f
1:	jmp	1f
1:
	/* Reenable paging by setting the appropriate bits in CR0 */
	movl    %cr0,%eax
	orl     $(CR0_PE|CR0_PG|CR0_NE|CR0_TS|CR0_MP|CR0_WP),%eax
	movl    %eax,%cr0

	/* Flush the prefetch queue again */
	jmp 	1f
1:	jmp	1f
1:

	/* Enter long mode by making another intersegment jump */
	ljmp 	$0x8, $acpi_long_mode_trampoline

	.code64
	.align 16
_ACPI_TRMP_LABEL(acpi_long_mode_trampoline)
_C_LABEL(acpi_long_mode_resume):
	/* Restore the stashed copy of EFER we set aside earlier */	
	movl	%ebx, %eax
	movl	$MSR_EFER, %ecx
	wrmsr

	/* Load GDT based on our saved copy */
	lgdt	acpi_saved_gdt
	
	/* Reset segment registers */
	movw    $GSEL(GDATA_SEL, SEL_KPL),%ax
	movw    %ax,%ds
	movw    %ax,%es
	movw    %ax,%ss

	xorw	%ax, %ax
	movw	%ax, %fs
	movw	%ax, %gs

	/* Restore registers - start with the MSRs */
	movl	$MSR_STAR, %ecx
	movl	acpi_saved_star, %eax
	movl	acpi_saved_star+4, %edx
	wrmsr

	movl	$MSR_LSTAR, %ecx
	movl	acpi_saved_lstar, %eax
	movl	acpi_saved_lstar+4, %edx
	wrmsr

	movl	$MSR_CSTAR, %ecx
	movl	acpi_saved_cstar, %eax
	movl	acpi_saved_cstar+4, %edx
	wrmsr

	movl	$MSR_SFMASK, %ecx
	movl	acpi_saved_sfmask, %eax
	movl	acpi_saved_sfmask+4, %edx
	wrmsr

	movl	$MSR_FSBASE, %ecx
	movl	acpi_saved_fsbase, %eax
	movl	acpi_saved_fsbase+4, %edx
	wrmsr

	movl	$MSR_GSBASE, %ecx
	movl	acpi_saved_gsbase, %eax
	movl	acpi_saved_gsbase+4, %edx
	wrmsr

	movl	$MSR_KERNELGSBASE, %ecx
	movl	acpi_saved_kgs, %eax
	movl	acpi_saved_kgs+4, %edx
	wrmsr

	/* Restore control registers */
	movq	acpi_saved_cr8, %rax
	movq	%rax, %cr8
	movq	acpi_saved_cr4, %rax
	movq	%rax, %cr4
	movq	acpi_saved_cr3, %rax
	movq	%rax, %cr3

	/* Flush the prefetch queue again */
	jmp 	1f
1:	jmp	1f
1:
	movq	acpi_saved_cr2, %rax
	movq	%rax, %cr2
	movq	acpi_saved_cr0,	%rax
	movq	%rax, %cr0

	/* Flush the prefetch queue again */
	jmp 	1f
1:	jmp	1f
1:
	
	lldt	acpi_saved_ldt
	lidt	acpi_saved_idt

	/* Restore the saved task register */
	xorq	%rcx, %rcx
	movw	acpi_saved_tr, %cx
	movq	acpi_saved_gdt+2, %rax
	andb	$0xF9, 5(%rax,%rcx)
	ltr	%cx

	pushq 	acpi_saved_fl
	popfq

	movq    acpi_saved_rbx, %rbx
	movq    acpi_saved_rcx, %rcx
	movq    acpi_saved_rdx, %rdx
	movq    acpi_saved_rbp, %rbp
	movq    acpi_saved_rsi, %rsi
	movq    acpi_saved_rdi, %rdi
	movq    acpi_saved_rsp, %rsp

	movq    acpi_saved_r8, %r8
	movq    acpi_saved_r9, %r9
	movq    acpi_saved_r10, %r10
	movq    acpi_saved_r11, %r11
	movq    acpi_saved_r12, %r12
	movq    acpi_saved_r13, %r13
	movq    acpi_saved_r14, %r14
	movq    acpi_saved_r15, %r15

	/* Poke CR3 one more time. Might not be necessary */	
	movq	acpi_saved_cr3, %rax
	movq	%rax, %cr3

	xorq	%rax, %rax	
	jmp   	*acpi_saved_ret

	.align 8
_ACPI_TRMP_OFFSET(tmp_gdt)
	.word	tmp_gdt_end - tmp_gdtable
	.long	tmp_gdtable

	.align 8,0
_ACPI_TRMP_LABEL(tmp_gdtable)
	/*
	 * null
	 */
	.word	0, 0
	.byte	0, 0, 0, 0
	/*
	 * Code
	 * Limit: 0xffffffff
	 * Base: 0x00000000
	 * Descriptor Type: Code
	 * Segment Type: CRA 
	 * Present: True
	 * Priv: 0
	 * AVL: False
	 * 64-bit: False
	 * 32-bit: True
	 *
	 */ 
	.word	0xffff, 0
	.byte	0, 0x9f, 0xcf, 0

	/*
	 * Data
	 * Limit: 0xffffffff
	 * Base: 0x00000000
	 * Descriptor Type: 
	 * Segment Type: W
	 * Present: True
	 * Priv: 0
	 * AVL: False
	 * 64-bit: False
	 * 32-bit: True
	 *
	 */ 
	.word	0xffff, 0
	.byte	0, 0x93, 0xcf, 0
_ACPI_TRMP_LABEL(tmp_gdt_end)

	.align 8, 0
_ACPI_TRMP_OFFSET(clean_idt)
	.word	0xffff
	.long	0
	.word	0

_ACPI_TRMP_OFFSET(clean_gdt)
	.word	clean_gdt_end - clean_gdtable
	.long	clean_gdtable

	.align 8, 0
_ACPI_TRMP_LABEL(clean_gdtable)
	.word	0, 0
	.byte	0, 0, 0, 0

	.word	0xffff, ACPI_TRAMPOLINE
	.byte	0, 0x9b, 0, 0

	.word	0xffff, ACPI_TRAMPOLINE
	.byte	0, 0x93, 0, 0
_ACPI_TRMP_LABEL(clean_gdt_end)

	.align 8
_ACPI_TRMP_LABEL(tmp_gdt64)
	.word 	tmp_gdt64_end - tmp_gdtable64
	.long	tmp_gdtable64

	.align 8,0
_ACPI_TRMP_LABEL(tmp_gdtable64)
	.quad	0x0000000000000000
	.quad	0x00af9a000000ffff
	.quad	0x00cf92000000ffff
_ACPI_TRMP_LABEL(tmp_gdt64_end)

	.align 8
_ACPI_TRMP_LABEL(acpi_saved_rbx)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_rcx)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_rdx)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_rbp)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_rsi)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_rdi)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_rsp)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_rsp)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_r8)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_r9)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_r10)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_r11)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_r12)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_r13)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_r14)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_r15)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_fl)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_cr0)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_cr2)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_cr3)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_cr4)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_cr8)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_ret)
	.quad 0

	.align 8
_ACPI_TRMP_LABEL(acpi_saved_idt)
	.space 10

	.align 8
_ACPI_TRMP_LABEL(acpi_saved_gdt)
	.space 10

	.align 8
_ACPI_TRMP_LABEL(acpi_saved_ldt)	
	.space 10
	
_ACPI_TRMP_LABEL(acpi_saved_tr)
	.short 0

	.align 4
_ACPI_TRMP_LABEL(acpi_saved_efer)
	.long 0

	.align 8
_ACPI_TRMP_LABEL(acpi_saved_fsbase)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_gsbase)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_kgs)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_star)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_lstar)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_cstar)
	.quad 0
_ACPI_TRMP_LABEL(acpi_saved_sfmask)
	.quad 0

	.align 4
_ACPI_TRMP_LABEL(acpi_pdirpa)
	.long 0

	/*
	 * End of resume code (code copied to ACPI_TRAMPOLINE)
	 */
_C_LABEL(acpi_resume_end):

	/*
	 * acpi_savecpu saves the processor's registers and flags
	 * for use during the ACPI suspend/resume process.
	 */

NENTRY(acpi_savecpu)
	movq	(%rsp), %rax
	movq	%rax, acpi_saved_ret

	movq	%rbx, acpi_saved_rbx
	movq	%rcx, acpi_saved_rcx
	movq	%rdx, acpi_saved_rdx
	movq	%rbp, acpi_saved_rbp
	movq	%rsi, acpi_saved_rsi
	movq	%rdi, acpi_saved_rdi
	movq	%rsp, acpi_saved_rsp
	addq	$0x8, acpi_saved_rsp

	movq	%r8, acpi_saved_r8
	movq	%r9, acpi_saved_r9
	movq	%r10, acpi_saved_r10
	movq	%r11, acpi_saved_r11
	movq	%r12, acpi_saved_r12
	movq	%r13, acpi_saved_r13
	movq	%r14, acpi_saved_r14
	movq	%r15, acpi_saved_r15

	pushfq
	popq	acpi_saved_fl

	movq	%cr0, %rax
	movq	%rax, acpi_saved_cr0
	movq	%cr2, %rax
	movq	%rax, acpi_saved_cr2
	movq	%cr3, %rax
	movq	%rax, acpi_saved_cr3
	movq	%cr4, %rax
	movq	%rax, acpi_saved_cr4
	movq	%cr8, %rax
	movq	%rax, acpi_saved_cr8
	
	pushq	%rcx
	pushq	%rdx
	movl	$MSR_STAR, %ecx
	rdmsr
	movl	%eax, acpi_saved_star
	movl	%edx, acpi_saved_star+4

	movl	$MSR_CSTAR, %ecx
	rdmsr
	movl	%eax, acpi_saved_cstar
	movl	%edx, acpi_saved_cstar+4

	movl	$MSR_LSTAR, %ecx
	rdmsr
	movl	%eax, acpi_saved_lstar
	movl	%edx, acpi_saved_lstar+4

	movl	$MSR_SFMASK, %ecx
	rdmsr
	movl	%eax, acpi_saved_sfmask
	movl	%edx, acpi_saved_sfmask+4

	movl	$MSR_FSBASE, %ecx
	rdmsr
	movl	%eax, acpi_saved_fsbase
	movl	%edx, acpi_saved_fsbase+4

	movl	$MSR_GSBASE, %ecx
	rdmsr
	movl	%eax, acpi_saved_gsbase
	movl	%edx, acpi_saved_gsbase+4

	movl	$MSR_KERNELGSBASE, %ecx
	rdmsr
	movl	%eax, acpi_saved_kgs
	movl	%edx, acpi_saved_kgs+4
	
	movl	$MSR_EFER, %ecx
	rdmsr
	movl	%eax, acpi_saved_efer
	popq 	%rdx
	popq	%rcx

	sgdt	acpi_saved_gdt
	sidt	acpi_saved_idt
	sldt	acpi_saved_ldt
	str	acpi_saved_tr

	movl	$1, %eax
	ret
#endif /* SMALL_KERNEL */
#endif /* NACPI > 0 */
