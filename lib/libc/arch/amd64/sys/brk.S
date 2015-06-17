/*	$OpenBSD: brk.S,v 1.5 2015/06/17 03:04:50 uebayasi Exp $	*/
/*	$NetBSD: brk.S,v 1.2 2002/06/03 18:30:33 fvdl Exp $	*/

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
 *	from: @(#)brk.s	5.2 (Berkeley) 12/17/90
 */

#include <machine/asm.h>

#include "SYS.h"

	.globl	_end
	.globl	_C_LABEL(__minbrk)
	.globl	_C_LABEL(minbrk)
	.globl	__curbrk

#ifdef WEAK_ALIAS
WEAK_ALIAS(brk, _brk)
#endif

	.data
_C_LABEL(minbrk):
_C_LABEL(__minbrk):
	.quad	_end
	.text

SYSENTRY(_brk)
#ifdef __PIC__
	movq	PIC_GOT(_C_LABEL(__minbrk)),%rdx
	cmpq	%rdi,(%rdx)
	jb	1f
	movq	(%rdx),%rdi
1:
	SYSTRAP(break)
	jc	err
	movq	PIC_GOT(__curbrk),%rdx	# set up GOT addressing
	movq	%rdi,(%rdx)
	xorl	%eax,%eax
	ret
err:
	movq	PIC_GOT(CERROR),%rdx
	jmp	*%rdx
#else
	cmpq	%rdi,_C_LABEL(__minbrk)(%rip)
	jb	1f
	movq	_C_LABEL(__minbrk)(%rip),%rdi
1:
	SYSTRAP(break)
	jc	err
	movq	%rdi,__curbrk(%rip)
	xorl	%eax,%eax
	ret
err:
	jmp	CERROR
#endif
SYSEXIT(_brk)
