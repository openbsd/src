/*	$OpenBSD: asm.h,v 1.7 2002/11/05 18:04:36 miod Exp $ */
/*	$NetBSD: asm.h,v 1.9 1999/01/15 13:31:28 bouyer Exp $ */
/*
 * Copyright (c) 1982, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)DEFS.h	8.1 (Berkeley) 6/4/93
 */

#ifndef _MACHINE_ASM_H_
#define _MACHINE_ASM_H_

#define R0	0x001
#define R1	0x002
#define R2	0x004
#define R3	0x008
#define R4	0x010
#define R5	0x020
#define R6	0x040
#define R7 	0x080
#define R8	0x100
#define R9	0x200
#define R10	0x400
#define R11	0x800

#ifdef __ELF__
# define _C_LABEL(x)	x
#else
# ifdef __STDC__
#  define _C_LABEL(x)	_ ## x
# else
#  define _C_LABEL(x)	_/**/x
# endif
#endif

#define	_ASM_LABEL(x)	x

#ifdef __STDC__
# define __CONCAT(x,y)	x ## y
# define __STRING(x)	#x
#else
# define __CONCAT(x,y)	x/**/y
# define __STRING(x)	"x"
#endif

/* let kernels and others override entrypoint alignment */
#ifndef _ALIGN_TEXT
# ifdef __ELF__
#  define _ALIGN_TEXT .align 4
# else
#  define _ALIGN_TEXT .align 2
# endif
#endif

#define	_ENTRY(x, regs) \
	.text; _ALIGN_TEXT; .globl x; .type x,@function; x: .word regs

#ifdef GPROF
# ifdef __ELF__
#  define _PROF_PROLOGUE	\
	.data; 1:; .long 0; .text; moval 1b,r0; jsb _ASM_LABEL(__mcount)
# else 
#  define _PROF_PROLOGUE	\
	.data; 1:; .long 0; .text; moval 1b,r0; jsb _ASM_LABEL(mcount)
# endif
#else
# define _PROF_PROLOGUE
#endif

#define ENTRY(x, regs)		_ENTRY(_C_LABEL(x), regs); _PROF_PROLOGUE
#define NENTRY(x, regs)		_ENTRY(_C_LABEL(x), regs)
#define ASENTRY(x, regs)	_ENTRY(_ASM_LABEL(x), regs); _PROF_PROLOGUE

#define ALTENTRY(x)		.globl _C_LABEL(x); _C_LABEL(x):
#define RCSID(x)		.text; .asciz x

#ifdef	__ELF__
#define	WEAK_ALIAS(alias,sym)						\
	.weak alias;							\
	alias = sym
#else
#ifdef	__STDC__
#define	WEAK_ALIAS(alias,sym)						\
	.weak _##alias;							\
	_##alias = _##sym
#else
#define	WEAK_ALIAS(alias,sym)						\
	.weak _/**/alias;						\
	_/**/alias = _/**/sym
#endif
#endif

#ifdef __STDC__
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg ## ,30,0,0,0 ;					\
	.stabs __STRING(_C_LABEL(sym)) ## ,1,0,0,0
#else
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg,30,0,0,0 ;						\
	.stabs __STRING(_C_LABEL(sym)),1,0,0,0
#endif /* __STDC__ */

#endif /* _MACHINE_ASM_H_ */
