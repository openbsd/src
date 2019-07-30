/*	$OpenBSD: asm.h,v 1.9 2018/02/21 19:24:15 guenther Exp $	*/
/*	$NetBSD: asm.h,v 1.2 2003/05/02 18:05:47 yamt Exp $	*/

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
 *	@(#)asm.h	5.5 (Berkeley) 5/7/91
 */

#ifndef _MACHINE_ASM_H_
#define _MACHINE_ASM_H_

#ifdef __PIC__
#define PIC_PLT(x)	x@PLT
#define PIC_GOT(x)	x@GOTPCREL(%rip)
#else
#define PIC_PLT(x)	x
#define PIC_GOT(x)	x
#endif

# define _C_LABEL(x)	x
#define	_ASM_LABEL(x)	x

#define CVAROFF(x,y)		(_C_LABEL(x)+y)(%rip)

#ifdef __STDC__
# define __CONCAT(x,y)	x ## y
# define __STRING(x)	#x
#else
# define __CONCAT(x,y)	x/**/y
# define __STRING(x)	"x"
#endif

/* let kernels and others override entrypoint alignment */
#ifndef _ALIGN_TEXT
#define _ALIGN_TEXT .align 16, 0x90
#endif

#define _ENTRY(x) \
	.text; _ALIGN_TEXT; .globl x; .type x,@function; x:

#ifdef _KERNEL
#define	KUTEXT	.section .kutext, "ax"
/*#define	KUTEXT	.text */

/* XXX Can't use __CONCAT() here, as it would be evaluated incorrectly. */
#define	IDTVEC(name) \
	KUTEXT; ALIGN_TEXT; \
	.globl X ## name; .type X ## name,@function; X ## name:
#define	KIDTVEC(name) \
	.text; ALIGN_TEXT; \
	.globl X ## name; .type X ## name,@function; X ## name:
#define KUENTRY(x) \
	KUTEXT; _ALIGN_TEXT; .globl x; .type x,@function; x:

#endif /* _KERNEL */

#ifdef __STDC__
#define CPUVAR(off)	%gs:CPU_INFO_ ## off
#else
#define CPUVAR(off)     %gs:CPU_INFO_/**/off
#endif


#if defined(PROF) || defined(GPROF)
# define _PROF_PROLOGUE	\
	pushq %rbp; leaq (%rsp),%rbp; call PIC_PLT(__mcount); popq %rbp
#else
# define _PROF_PROLOGUE
#endif

#define	ENTRY(y)	_ENTRY(_C_LABEL(y)); _PROF_PROLOGUE
#define	NENTRY(y)	_ENTRY(_C_LABEL(y))
#define	ASENTRY(y)	_ENTRY(_ASM_LABEL(y)); _PROF_PROLOGUE
#define	END(y)		.size y, . - y

#define	STRONG_ALIAS(alias,sym)						\
	.global alias;							\
	alias = sym
#define	WEAK_ALIAS(alias,sym)						\
	.weak alias;							\
	alias = sym

/* XXXfvdl do not use stabs here */
#ifdef __STDC__
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg ## ,30,0,0,0 ;					\
	.stabs __STRING(_C_LABEL(sym)) ## ,1,0,0,0
#else
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg,30,0,0,0 ;						\
	.stabs __STRING(sym),1,0,0,0
#endif /* __STDC__ */

#endif /* !_MACHINE_ASM_H_ */
