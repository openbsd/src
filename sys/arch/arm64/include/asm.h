/*	$OpenBSD: asm.h,v 1.3 2017/06/29 17:36:16 deraadt Exp $	*/
/*	$NetBSD: asm.h,v 1.4 2001/07/16 05:43:32 matt Exp $	*/

/*
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
 *	from: @(#)asm.h	5.5 (Berkeley) 5/7/91
 */

#ifndef _MACHINE_ASM_H_
#define _MACHINE_ASM_H_

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

#ifndef _ALIGN_TEXT
# define _ALIGN_TEXT .align 0
#endif

/*
 * gas/arm uses @ as a single comment character and thus cannot be used here
 * Instead it recognised the # instead of an @ symbols in .type directives
 * We define a couple of macros so that assembly code will not be dependant
 * on one or the other.
 */
#define _ASM_TYPE_FUNCTION	#function
#define _ASM_TYPE_OBJECT	#object
#define _ENTRY(x) \
	.text; _ALIGN_TEXT; .globl x; .type x,_ASM_TYPE_FUNCTION; x:

#if defined(PROF) || defined(GPROF)
#  define _PROF_PROLOGUE	\
	stp	x29, x30, [sp, #-16]!; \
	mov fp, sp;		\
	bl __mcount; 		\
	ldp	x29, x30, [sp], #16;
#else
# define _PROF_PROLOGUE
#endif

#define	ENTRY(y)	_ENTRY(_C_LABEL(y)); _PROF_PROLOGUE
#define	ENTRY_NP(y)	_ENTRY(_C_LABEL(y))
#define	ASENTRY(y)	_ENTRY(_ASM_LABEL(y)); _PROF_PROLOGUE
#define	ASENTRY_NP(y)	_ENTRY(_ASM_LABEL(y))
#define	END(y)		.size y, . - y
#define EENTRY(sym)	 .globl  sym; sym:
#define EEND(sym)

#if defined(__ELF__) && defined(__PIC__)
#ifdef __STDC__
#define	PIC_SYM(x,y)	x ## ( ## y ## )
#else
#define	PIC_SYM(x,y)	x/**/(/**/y/**/)
#endif
#else
#define	PIC_SYM(x,y)	x
#endif

#ifdef __ELF__
#define	STRONG_ALIAS(alias,sym)						\
	.global alias;							\
	alias = sym
#define	WEAK_ALIAS(alias,sym)						\
	.weak alias;							\
	alias = sym
#endif

#ifdef __STDC__
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg ## ,30,0,0,0 ;					\
	.stabs __STRING(_C_LABEL(sym)) ## ,1,0,0,0
#elif defined(__ELF__)
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg,30,0,0,0 ;						\
	.stabs __STRING(sym),1,0,0,0
#else
#define	WARN_REFERENCES(sym,msg)					\
	.stabs msg,30,0,0,0 ;						\
	.stabs __STRING(_/**/sym),1,0,0,0
#endif /* __STDC__ */


/*
 * Sets the trap fault handler. The exception handler will return to the
 * address in the handler register on a data abort or the xzr register to
 * clear the handler. The tmp parameter should be a register able to hold
 * the temporary data.
 */
#define SET_FAULT_HANDLER(handler, tmp)					\
	ldr	tmp, [x18, #CI_CURPCB];		/* Load the pcb */	\
	str	handler, [tmp, #PCB_ONFAULT]	/* Set the handler */

#endif /* !_MACHINE_ASM_H_ */
