/*	$Openbsd: $	*/
/*
 * Copyright (c) 2015 YASUOKA Masahiko <yasuoka@yasuoka.net>
 * Copyright (c) 2025 Hans-Joerg Hoexer <hshoexer@yerbouti.franken.de>
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

/* kexec trampoline, based on sys/arch/amd64/stand/efiboot/run_i386.S */

#include <machine/asm.h>
#include <machine/param.h>
#include <machine/specialreg.h>
#include <machine/kexec.h>

#define CODE_SEGMENT	0x10
#define DATA_SEGMENT	0x18

	.globl	kexec_size
kexec_size:
	.long	kexec_end - kexec_tramp

	.align	NBPG,0xcc
	.text
	.globl	kexec_tramp
kexec_tramp:
	/*
	 * %rdi: VA of old kernel to be overwritten
	 * %rsi: VA of new kernel
	 * %rdx: entry point of new kernel (PA)
	 * %rcx: kernel size
	 * %r8: VA stack pointer
	 */

	/* use %r8 as "anchor" */
	leaq	-7(%rip), %r9	/* size of leaq instruction is 7 */

	/* save entry PA on local stack */
	movq	%rdx, (kexec_end - kexec_tramp - 0x8)(%r9)

	/* about to overwrite boot kernel, thus disable interrupts */
	cli

	/*
	 * %rsi: from, VA new kernel
	 * %rdi: to, VA old kernel
	 * %rcx: count
	 */
	cld
	movq	%rcx, (kexec_end - kexec_tramp - 0x10)(%r9)
	shrq	$3,%rcx
	rep
	movsq			/* copy 64bit words */

	movq	(kexec_end - kexec_tramp - 0x10)(%r9),%rcx
	andl	$7,%ecx		/* any bytes left? */
	rep
	movsb			/* copy remaining bytes */

	/* boot kernel is now overwritten */

	/* get new kernel entry PA from local stack */
	movq	(kexec_end - kexec_tramp - 0x8)(%r9), %rsi

	/* patch in ljmp target */
	movq	$KEXEC_TRAMPOLINE, %rax
	lea	(kexec32a - kexec_tramp)(%rax), %rax
	movl	%eax, (kexec32r - kexec_tramp)(%r9)

	/* boot arguments are provided on the stack*/
	movq	%r8, %rsp

	lea	(gdt - kexec_tramp)(%r9),%rax
	mov	%rax, (gdtrr - kexec_tramp)(%r9)
	lgdt	(gdtr - kexec_tramp)(%r9)

	ljmp	*(kexec32r - kexec_tramp)(%r9)

	.align	4
kexec32a:
	.code32				/* back in compatibility mode */
	movl	$DATA_SEGMENT, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %fs
	movl	%eax, %gs
	movl	%eax, %ss

	movl	%cr0, %eax
	andl	$(~CR0_PG), %eax
	movl	%eax, %cr0		/* clears LMA (long mode active) */

	movl	%cr4, %eax
	andl	$(~CR4_PAE), %eax
	movl	%eax, %cr4

	movl	$MSR_EFER, %ecx
	rdmsr
	andl	$(~EFER_LME), %eax	/* disable long mode */
	wrmsr

	jmp	kexec32b
kexec32b:
	.code32				/* back in legacy mode */

	call	*%esi			/* jump into new kernel */

	.align	4
kexec32r:
	.long	0
	.long	CODE_SEGMENT
	.align	4
gdt:
	.long	0, 0
	.long	0, 0
	/* 32bit code segement */
	.byte	0xff, 0xff, 0x00, 0x00, 0x00, 0x9f, 0xcf, 0x00
	/* 32bit data segement */
	.byte	0xff, 0xff, 0x00, 0x00, 0x00, 0x93, 0xcf, 0x00
gdtr:
	.word	gdtr - gdt
gdtrr:
	.quad	0

	.align	8
	/* local stack */
	.space	1024
kexec_end:
