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
 * Copyright (c) 2008 Mike Larkin <mlarkin@openbsd.org>
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

#define _ACPI_WAKECODE

#include "assym.h"
#include <machine/asm.h>
#ifdef HIBERNATE
#include <machine/hibernate_var.h>
#endif /* HIBERNATE */
#include <machine/specialreg.h>
#include <machine/param.h>
#include <machine/segments.h>
#include <dev/acpi/acpivar.h>

#define _ACPI_TRMP_LABEL(a) a = . - _C_LABEL(acpi_real_mode_resume) + ACPI_TRAMPOLINE
#define _ACPI_TRMP_OFFSET(a) a = . - _C_LABEL(acpi_real_mode_resume)
#define _ACPI_RM_SEGMENT (ACPI_TRAMPOLINE >> 4)

#ifdef HIBERNATE
#define HIBERNATE_STACK_OFFSET 0x0F00
#endif

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
 * if our phys addr is 0x13000, we'd have cs=0x1300,ip=0
 *
 * The wakeup code needs to do the following:
 *     1. Reenable the video display
 *     2. Enter 32 bit protected mode
 *     3. Reenable paging
 *     4. Restore saved CPU registers
 */

	.text
	.code16
	.align 4
	.global _C_LABEL(acpi_real_mode_resume)
	.global _C_LABEL(acpi_protected_mode_resume)
	.global _C_LABEL(acpi_resume_end)
_C_LABEL(acpi_real_mode_resume):
_ACPI_TRMP_OFFSET(acpi_s3_vector_real)
	nop
	cli
	cld

	/*
	 * Set up segment registers for real mode.
	 * We'll only be in real mode for a moment, and we don't have
	 * want real dependencies on data or stack, so we'll just use
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
	 * Set up esi to point to start of current routine's CS.
	 */
	xorl    %esi,%esi
	movw    %cs,%si
	shll    $4,%esi

	/*
	 * Flush instruction prefetch queue
	 */
	jmp	1f
1:	jmp	1f
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


	/*
	 * Reset our page size extension (via restoring cr4) to what
	 * it was before we suspended. If we don't do this, cr4 might
	 * contain garbage in the PSE bit, leading to pages that
	 * are incorrectly interpreted as the wrong size
	 * CR4 was added in i586, so there is
	 * an implicit assumption here that this code will execute on
	 * i586 or later.
	 */
	mov	acpi_saved_cr4,%eax
	mov	%eax,%cr4

	/*
	 * Re-enable paging, using the CR3 we stored before suspend
	 * as our new page table base location. Restore CR0 after
	 * that.
	 */
	movl	acpi_saved_cr3,%eax
	movl	%eax,%cr3
	movl	acpi_saved_cr0, %eax
	movl	%eax, %cr0

	/*
	 * Flush the prefetch queue in order to enforce usage
	 * of the new (old) page tables we just re-enabled
	 */
	jmp	1f
1:	jmp	1f
1:
	nop

	/*
	 * Restore CPU segment descriptor registers
	 */
	lgdt	acpi_saved_gdt
	lidt	acpi_saved_idt
	lldt	acpi_saved_ldt

	mov	acpi_saved_cr2,%eax
	mov	%eax,%cr2

	/*
	 * It is highly likely that the selectors we already loaded into
	 * these registers are already accurate, but we reload them
	 * again, for consistency.
	 */
	movw	acpi_saved_es,%ax
	movw	%ax,%es
	movw	acpi_saved_fs,%ax
	movw	%ax,%fs
	movw	acpi_saved_gs,%ax
	movw	%ax,%gs
	movw	acpi_saved_ss,%ax
	movw	%ax,%ss
	movw	acpi_saved_ds,%ax
	movw	%ax,%ds

	/*
	 * Shortly, we'll restore the TSS for the task that was running
	 * immediately before suspend occured. Since that task was the
	 * running task, it's TSS busy flag will have been set. We need
	 * to clear that bit (since we're effectively "restarting" the OS)
	 * in order to convince the processor that the task is no longer
	 * running (which is true, now). If we don't do this, when the
	 * OS resumes and resumes this task, it will assume we're trying
	 * to recurse into an already active task, which would cause
	 * a GP violation (and probably, a crash).
	 *
	 * We accomplish this by changing the TSS descriptor from
	 * BUSY (0x0B) to AVAILABLE (0x09). We keep the other
	 * high 4 bits intact.
	 */
	movl	acpi_saved_gdt+2,%ebx
	xorl	%ecx, %ecx
	movw	acpi_saved_tr,%cx
	leal	(%ebx,%ecx),%eax
	andb	$0xF9,5(%eax)

	ltr	acpi_saved_tr

	/*
	 * Everything is almost reset back to the way it was immediately before
	 * suspend. There are a few more registers to restore - this is
	 * done in acpi_restorecpu. We'll jump to that routine, and after
	 * that, jump back to the OS. There's still some things
	 * to do there, like re-enable interrupts, resume devices, APICs,
	 * etc.
	 */
	movl	acpi_saved_ebx, %ebx
	movl	acpi_saved_ecx, %ecx
	movl	acpi_saved_edx, %edx
	movl	acpi_saved_ebp, %ebp
	movl	acpi_saved_esi, %esi
	movl	acpi_saved_edi, %edi
	movl	acpi_saved_esp, %esp
	push	acpi_saved_fl
	popfl

	/* Poke CR3 one more time. Might not be necessary */
	movl	acpi_saved_cr3,%eax
	movl	%eax,%cr3

	/*
	 * Return to the OS. We've previously saved the resume
	 * address in acpi_saved_ret (via a call to acpi_savecpu
	 * before we went to sleep.)
	 */
	xorl  %eax, %eax
	jmp	*acpi_saved_ret

#ifdef HIBERNATE
	/*
	 * hibernate_resume_machdep drops to real mode and
	 * restarts the OS using the saved S3 resume vector
	 */
	.code32
NENTRY(hibernate_resume_machdep)
	cli
	/* Jump to the identity mapped version of ourself */
	mov	$hibernate_resume_vector_2, %eax
	jmp	*%eax
_ACPI_TRMP_LABEL(hibernate_resume_vector_2)

	/* Get out of 32 bit CS */
	lgdt	gdt_16
	ljmp	$0x8, $hibernate_resume_vector_3

_ACPI_TRMP_LABEL(hibernate_resume_vector_3)
	.code16
	movl	%cr0, %eax
	/* Disable CR0.PG - no paging */
	andl	$(~CR0_PG), %eax
	/* Disable CR0.PE - real mode */
	andl	$(~CR0_PE), %eax
	movl	%eax, %cr0

	/* Flush TLB */
	xorl	%eax, %eax
	movl	%eax, %cr3

	/* Set up real mode segment selectors */
	movw	$0x1300, %ax
	movw	%ax, %ds
	movw	%ax, %es
	movw	%ax, %fs
	movw	%ax, %gs
	movl	$0x0FFE, %esp
	lidtl	clean_idt

	/* Jump to the S3 resume vector */
	ljmp	$0x1300, $acpi_s3_vector_real

	.code32
	/* Switch to hibernate resume pagetable */
NENTRY(hibernate_activate_resume_pt_machdep)
	/* Enable large pages */
	movl	%cr4, %eax
	orl	$(CR4_PSE), %eax

	/* Disable global pages */
	andl	$(~CR4_PGE), %eax
	movl	%eax, %cr4

	wbinvd
	movl	$HIBERNATE_PD_PAGE, %eax
	movl	%eax,	%cr3
	jmp	1f

1:	nop
	ret

	/*
	 * Switch to the private resume-time hibernate stack
	 */
NENTRY(hibernate_switch_stack_machdep)
	movl	(%esp), %eax
	movl    %eax, HIBERNATE_STACK_PAGE + HIBERNATE_STACK_OFFSET
	movl    $(HIBERNATE_STACK_PAGE + HIBERNATE_STACK_OFFSET), %eax
	movl    %eax, %esp

	/* On our own stack from here onward */
	ret

NENTRY(hibernate_flush)
	invlpg  HIBERNATE_INFLATE_PAGE
	ret
#endif /* HIBERNATE */

	.code16
	.align 8
_ACPI_TRMP_OFFSET(tmp_gdt)
	.word	tmp_gdt_end - tmp_gdtable
	.long	tmp_gdtable

	.align 8
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

	.align 8
_ACPI_TRMP_OFFSET(clean_idt)
	.word	0xffff
	.long	0
	.word	0

	/*
	 * gdt_16 is the gdt used when returning to real mode for bios
	 * reads/writes (sets up a 16 bit segment)
	 */
	.align 8
_ACPI_TRMP_LABEL(gdt_16)
	.word   gdt_16_end - gdt_16_table
	.long   gdt_16_table

	.align 8
_ACPI_TRMP_LABEL(gdt_16_table)
	/*
	 * null
	 */
	.word   0, 0
	.byte   0, 0, 0, 0
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
	 * 32-bit: False
	 *
	 */
	.word   0xffff, 0
	.byte   0, 0x9f, 0x8f, 0

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
	 * 32-bit: False
	 *
	 */
	.word   0xffff, 0
	.byte   0, 0x93, 0x8f, 0

_ACPI_TRMP_LABEL(gdt_16_end)

	.align 4
_ACPI_TRMP_LABEL(acpi_saved_ebx)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_ecx)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_edx)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_ebp)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_esi)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_edi)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_esp)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_fl)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_cr0)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_cr2)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_cr3)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_cr4)
	.long 0
_ACPI_TRMP_LABEL(acpi_saved_ret)
	.long 0

	.align 16
_ACPI_TRMP_LABEL(acpi_saved_idt)
	.space 6

	.align 16
_ACPI_TRMP_LABEL(acpi_saved_gdt)
	.space 6

	.align 16
_ACPI_TRMP_LABEL(acpi_saved_ldt)
	.short 0
_ACPI_TRMP_LABEL(acpi_saved_cs)
	.short 0
_ACPI_TRMP_LABEL(acpi_saved_ds)
	.short 0
_ACPI_TRMP_LABEL(acpi_saved_es)
	.short 0
_ACPI_TRMP_LABEL(acpi_saved_fs)
	.short 0
_ACPI_TRMP_LABEL(acpi_saved_gs)
	.short 0
_ACPI_TRMP_LABEL(acpi_saved_ss)
	.short 0
_ACPI_TRMP_LABEL(acpi_saved_tr)
	.short 0

	/*
	 * End of resume code (code copied to ACPI_TRAMPOLINE)
	 */
_C_LABEL(acpi_resume_end):

	/*
	 * acpi_savecpu saves the processor's registers and flags
	 * for use during the ACPI suspend/resume process.
	 */

	.code32
NENTRY(acpi_savecpu)
	movl	(%esp), %eax
	movl	%eax, acpi_saved_ret

	movw	%cs, acpi_saved_cs
	movw	%ds, acpi_saved_ds
	movw	%es, acpi_saved_es
	movw	%fs, acpi_saved_fs
	movw	%gs, acpi_saved_gs
	movw	%ss, acpi_saved_ss

	movl	%ebx, acpi_saved_ebx
	movl	%ecx, acpi_saved_ecx
	movl	%edx, acpi_saved_edx
	movl	%ebp, acpi_saved_ebp
	movl	%esi, acpi_saved_esi
	movl	%edi, acpi_saved_edi
	movl	%esp, acpi_saved_esp

	pushfl
	popl	acpi_saved_fl

	movl	%cr0, %eax
	movl	%eax, acpi_saved_cr0
	movl	%cr2, %eax
	movl	%eax, acpi_saved_cr2
	movl	%cr3, %eax
	movl	%eax, acpi_saved_cr3
	movl	%cr4, %eax
	movl	%eax, acpi_saved_cr4

	sgdt	acpi_saved_gdt
	sidt	acpi_saved_idt
	sldt	acpi_saved_ldt
	str	acpi_saved_tr

	movl	$1, %eax
	ret
