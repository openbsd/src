/*	$OpenBSD: asm.h,v 1.3 2002/08/11 12:13:16 art Exp $	*/
/*	$NetBSD: asm.h,v 1.5 1997/07/16 15:16:43 christos Exp $ */

/*
 * Copyright (c) 1994 Allen Briggs
 * All rights reserved.
 *
 * Gleaned from locore.s and sun3 asm.h which had the following copyrights:
 * locore.s:
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * sun3/include/asm.h:
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1990 The Regents of the University of California.
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
 */

#ifndef _ASM_H_
#define _ASM_H_

#ifdef __ELF__
#define _C_LABEL(name)		name
#else
#ifdef __STDC__
#define _C_LABEL(name)		_ ## name
#else
#define _C_LABEL(name)		_/**/name
#endif
#endif
#define	_ASM_LABEL(name)	name

/*
 * WEAK_ALIAS: create a weak alias (ELF only)
 */
#ifdef __ELF__
#define WEAK_ALIAS(alias,sym)		\
	.weak alias;			\
	alias = sym
#endif

/*
 * WARN_REFERENCES: create a warning if the specified symbol is referenced
 * (ELF only).
 */
#ifdef __ELF__
#define WARN_REFERENCES(_sym,_msg)	\
	.section .gnu.warning. ## _sym ; .ascii _msg ; .text
#endif /* __ELF__ */


#ifdef PIC
/*
 * PIC_PROLOGUE() is akin to the compiler generated function prologue for
 * PIC code. It leaves the address of the Global Offset Table in DEST,
 * clobbering register TMP in the process. Using the temporary enables us
 * to work without a stack frame (doing so requires saving %o7) .
 */
#define PIC_PROLOGUE(dest,tmp) \
	mov %o7,tmp; 3: call 4f; nop; 4: \
	sethi %hi(_C_LABEL(_GLOBAL_OFFSET_TABLE_)-(3b-.)),dest; \
	or dest,%lo(_C_LABEL(_GLOBAL_OFFSET_TABLE_)-(3b-.)),dest; \
	add dest,%o7,dest; mov tmp,%o7

/*
 * PICCY_SET() does the equivalent of a `set var, %dest' instruction in
 * a PIC-like way, but without involving the Global Offset Table. This
 * only works for VARs defined in the same file *and* in the text segment.
 */
#define PICCY_SET(var,dest,tmp) \
	mov %o7,tmp; 3: call 4f; nop; 4: \
	add %o7,(var-3b),dest; mov tmp,%o7
#else
#define PIC_PROLOGUE(dest,tmp)
#define PICCY_OFFSET(var,dest,tmp)
#endif

#define FTYPE(x)		.type x,@function
#define OTYPE(x)		.type x,@object

#define	_ENTRY(name) \
	.align 4; .globl name; .proc 1; FTYPE(name); name:

#ifdef GPROF
#define _PROF_PROLOGUE \
	.data; .align 4; 1: .long 0; \
	.text; save %sp,-96,%sp; sethi %hi(1b),%o0; call mcount; \
	or %o0,%lo(1b),%o0; restore
#else
#define _PROF_PROLOGUE
#endif

#define ENTRY(name)		_ENTRY(_C_LABEL(name)); _PROF_PROLOGUE
#define	ASENTRY(name)		_ENTRY(_ASM_LABEL(name)); _PROF_PROLOGUE
#define	FUNC(name)		ASENTRY(name)


#define ASMSTR			.asciz

#define RCSID(name)		.asciz name

#endif /* _ASM_H_ */
