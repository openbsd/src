/*	$OpenBSD: srt0.S,v 1.1 1997/03/31 03:12:05 weingart Exp $	*/
/*	$NetBSD: srt0.c,v 1.3 1994/10/27 04:21:59 cgd Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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
 *	@(#)srt0.c	5.3 (Berkeley) 4/28/91
 */

/*
 * Startup code for standalone system
 * Non-relocating version -- for programs which are loaded by boot
 * Relocating version for boot
 * Small relocating version for "micro" boot
 */

#include <machine/psl.h>
#define _LOCORE
#include <machine/segments.h>
#undef _LOCORE

	.globl	_end
	.globl	_edata
	.globl	_boot
	.globl	__rtt

	.data
	.globl	_Gdtr
	.align	2
_Gdtr:	.word	4 * 8 - 1
	.long	2f
	.align	3
2:		/* 0x00 : null */
	.long	0, 0
		/* 0x08 : flat code */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMERAC | 0 | 0x80	# RWXAC, dpl = 0, present
	.byte	0xf | 0 | 0x40 | 0x80	# hilimit, xx, 32bit, 4k granularity
	.byte	0			# hibase
		/* 0x10 : flat data */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMRWA | 0 | 0x80	# RWA, dpl = 0, present
	.byte	0xf | 0 | 0x40 | 0x80	# hilimit, xx, 32bit, 4k granularity
	.byte	0			# hibase
		/* 0x18 : 16 bit code */
	.word	0xFFFF			# lolimit
	.word	0			# lobase
	.byte	0			# midbase
	.byte	SDT_MEMERC | 0 | 0x80	# RWXAC, dpl = 0, present
	.byte	0x0 | 0 | 0 | 0		# hilimit, xx, 16bit, byte granularity
	.byte	0			# hibase

	.globl	_codeseg
	.globl	_bootdev
	.globl	_boothowto
	.globl	_cyloffset
_codeseg:	.long	0
_bootdev:	.long	0
_boothowto:	.long	0
_cyloffset:	.long	0
_esym:		.long	0

	.text
text_start:

#define	NOP	inb $0x84,%al ; inb $0x84,%al

	.globl	start_boot

start_boot:
	pushl	%ebp
	movl	%esp, %ebp

#ifdef	REL
	/* relocate code+data */
	call	1f
1:	popl	%esi
	subl	$1b, %esi
	addl	$text_start, %esi	/* %esi = %eip - (1b - text_start) */
	subl	$0x20, %esi			/* XXX - Fudge factor for header */
	movl	$REL, %edi
	movl	$_edata, %ecx
	subl	$text_start, %ecx
	shrl	$2, %ecx
	incl	%ecx
	cld
	rep
	movsl
#endif /* REL */

	lgdt	_Gdtr

	/* The following should *not* be moved before the lgdt.
	 * Trust me, bad things will happen!
	 * Start executing from relocated code
	 */
	movl	$0x10, %eax
	movl	%eax, %ds
	movl	%eax, %es
	movl	%eax, %fs
	movl	%eax, %gs
	movl	%eax, %ss
	ljmp	$0x8,$docs
docs:
	nop

	/* XXX - I don't know if the following is right */
	movl    8(%ebp), %eax
	movl	%eax, _boothowto
	movl    12(%ebp),%eax
	movl    %eax, _bootdev
	movl    16(%ebp),%eax
	movl    %eax, _cyloffset
	movl    20(%ebp),%eax
	movl    %eax, _esym

	/* save old stack state */
	movl	%esp,savearea
	movl	%ebp,savearea+4

	/* setup stack pointer */
#ifdef	REL
	movl	$0xfffc, %esp
#else	
	movl	$_end, %eax
	addl	$10000, %eax
	movl	%eax, %esp
#endif

	/* clear memory as needed */
	movl	%esp,%esi
	movl	$_end, %eax
	subl	$_edata,%eax
	pushl	%eax
	pushl	$0
	pushl	$_edata
	call	_memset
	#call	_kbdreset	/* resets keyboard and gatea20 brain damage */
	movl	%esi,%esp

	pushl	__rtt
	ljmp	$0x8, $_boot	/* Jmp to boot code */

	.data

savearea:	.long	0,0	# sp & bp to return to

	.text

__rtt:
	movl	$-7,%eax
#ifdef	REL
	movw	$0x1234,%ax
	movw	%ax,0x472	# warm boot
	movl	$0,%esp		# segment violation
	ret
#else
	movl	savearea,%esp
	movl	savearea+4,%ebp
	ret
#endif

	.globl	_inb
_inb:	movl	4(%esp),%edx
	subl	%eax,%eax	# clr eax
	NOP
	inb	%dx,%al
	ret

	.globl	_outb
_outb:	movl	4(%esp),%edx
	NOP
	movl	8(%esp),%eax
	outb	%al,%dx
	ret

	.globl ___udivsi3
___udivsi3:
	movl 4(%esp),%eax
	xorl %edx,%edx
	divl 8(%esp)
	ret

	.globl ___divsi3
___divsi3:
	movl 4(%esp),%eax
	xorl %edx,%edx
	cltd
	idivl 8(%esp)
	ret

	.globl	_insw
_insw:
	pushl	%edi
	movw	8(%esp),%dx
	movl	12(%esp),%edi
	movl	16(%esp),%ecx
	NOP
	cld
	nop
	.byte 0x66,0xf2,0x6d	# rep insw
	nop
	movl	%edi,%eax
	popl	%edi
	ret

	# outsw(port,addr,cnt)
	.globl	_outsw
_outsw:
	pushl	%esi
	movw	8(%esp),%dx
	movl	12(%esp),%esi
	movl	16(%esp),%ecx
	NOP
	cld
	nop
	.byte 0x66,0xf2,0x6f	# rep outsw
	nop
	movl	%esi,%eax
	popl	%esi
	ret

