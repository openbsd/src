/*	$NetBSD: asm.h,v 1.3 1994/11/20 20:53:55 deraadt Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)asm.h	8.1 (Berkeley) 6/11/93
 */

#ifndef _ASM_H_
#define _ASM_H_

/*
 * GCC __asm constructs for doing assembly stuff.
 */

/*
 * ``Routines'' to load and store from/to alternate address space.
 * The location can be a variable, the asi value (address space indicator)
 * must be a constant.
 *
 * N.B.: You can put as many special functions here as you like, since
 * they cost no kernel space or time if they are not used.
 *
 * These were static inline functions, but gcc screws up the constraints
 * on the address space identifiers (the "n"umeric value part) because
 * it inlines too late, so we have to use the funny valued-macro syntax.
 */

#ifndef __ASSEMBLER__
/* load byte from alternate address space */
#define	lduba(asi, loc) ({ \
	register int _lduba_v; \
	__asm __volatile("lduba [%1]%2,%0" : "=r" (_lduba_v) : \
	    "r" ((int)(loc)), "n" (asi)); \
	_lduba_v; \
})

/* load half-word from alternate address space */
#define	lduha(asi, loc) ({ \
	register int _lduha_v; \
	__asm __volatile("lduha [%1]%2,%0" : "=r" (_lduha_v) : \
	    "r" ((int)(loc)), "n" (asi)); \
	_lduha_v; \
})

/* load int from alternate address space */
#define	lda(asi, loc) ({ \
	register int _lda_v; \
	__asm __volatile("lda [%1]%2,%0" : "=r" (_lda_v) : \
	    "r" ((int)(loc)), "n" (asi)); \
	_lda_v; \
})

/* store byte to alternate address space */
#define	stba(asi, loc, value) ({ \
	__asm __volatile("stba %0,[%1]%2" : : \
	    "r" ((int)(value)), "r" ((int)(loc)), "n" (asi)); \
})

/* store half-word to alternate address space */
#define	stha(asi, loc, value) ({ \
	__asm __volatile("stha %0,[%1]%2" : : \
	    "r" ((int)(value)), "r" ((int)(loc)), "n" (asi)); \
})

/* store int to alternate address space */
#define	sta(asi, loc, value) ({ \
	__asm __volatile("sta %0,[%1]%2" : : \
	    "r" ((int)(value)), "r" ((int)(loc)), "n" (asi)); \
})
#endif /* !__ASSEMBLER__ */

#ifdef __STDC__
#define _C_LABEL(x)	_ ## x
#else
#define _C_LABEL(x)	_/**/x
#endif

#define	_ASM_LABEL(name)	name

#ifdef PIC
/*
 * PIC_PROLOGUE() is akin to the compiler generated function prologue for
 * PIC code. It leaves the address of the Global Offset Table in DEST,
 * clobbering register TMP in the process. Using the temporary enables us
 * to work without a stack frame (doing so requires saving %o7) .
 */
#define PIC_PROLOGUE(dest,tmp) \
	mov %o7,tmp; 3: call 4f; nop; 4: \
	sethi %hi(__GLOBAL_OFFSET_TABLE_-(3b-.)),dest; \
	or dest,%lo(__GLOBAL_OFFSET_TABLE_-(3b-.)),dest; \
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

#ifdef PROF
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

#endif /* _ASM_H_ */
