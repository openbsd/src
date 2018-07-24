/*	$OpenBSD: asm.h,v 1.17 2018/07/24 02:42:25 guenther Exp $	*/
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
#define _ALIGN_TEXT	.align	16, 0x90
#endif
#define _ALIGN_TRAPS	.align	16, 0xcc

#define	_GENTRY(x)	.globl x; .type x,@function; x:
#define _ENTRY(x) \
	.text; _ALIGN_TRAPS; _GENTRY(x)
#define _NENTRY(x) \
	.text; _ALIGN_TEXT; _GENTRY(x)

#ifdef _KERNEL
#define	KUTEXT	.section .kutext, "ax", @progbits

#define	KUTEXT_PAGE_START	.pushsection .kutext.page, "a", @progbits
#define	KTEXT_PAGE_START	.pushsection .ktext.page, "ax", @progbits
#define	KUTEXT_PAGE_END		.popsection
#define	KTEXT_PAGE_END		.popsection

#define	IDTVEC(name) \
	KUTEXT; _ALIGN_TRAPS; IDTVEC_NOALIGN(name)
#define	IDTVEC_NOALIGN(name)	_GENTRY(X ## name)
#define	GENTRY(x)		_GENTRY(x)
#define	KIDTVEC(name) \
	.text; _ALIGN_TRAPS; IDTVEC_NOALIGN(name)
#define	KIDTVEC_FALLTHROUGH(name) \
	_ALIGN_TEXT; IDTVEC_NOALIGN(name)
#define KUENTRY(x) \
	KUTEXT; _ALIGN_TRAPS; _GENTRY(x)

/* Return stack refill, to prevent speculation attacks on natural returns */
#define	RET_STACK_REFILL_WITH_RCX	\
		mov	$8,%rcx		; \
		_ALIGN_TEXT		; \
	3:	call	5f		; \
	4:	pause			; \
		lfence			; \
		call	4b		; \
		_ALIGN_TRAPS		; \
	5:	call	7f		; \
	6:	pause			; \
		lfence			; \
		call	6b		; \
		_ALIGN_TRAPS		; \
	7:	loop	3b		; \
		add	$(16*8),%rsp

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

#if defined(_RET_PROTECTOR)
# define RETGUARD_SETUP_OFF(x, reg, off) \
	RETGUARD_SYMBOL(x); \
	movq (__retguard_ ## x)(%rip), %reg; \
	xorq off(%rsp), %reg
# define RETGUARD_SETUP(x, reg) \
	RETGUARD_SETUP_OFF(x, reg, 0)
# define RETGUARD_CHECK(x, reg) \
	xorq (%rsp), %reg; \
	cmpq (__retguard_ ## x)(%rip), %reg; \
	je 66f; \
	int3; int3; \
66:
# define RETGUARD_PUSH(reg) \
	pushq %reg
# define RETGUARD_POP(reg) \
	popq %reg
# define RETGUARD_SYMBOL(x) \
	.ifndef __retguard_ ## x; \
	.hidden __retguard_ ## x; \
	.type   __retguard_ ## x,@object; \
	.pushsection .openbsd.randomdata.retguard,"aw",@progbits; \
	.weak   __retguard_ ## x; \
	.p2align 3; \
	__retguard_ ## x: ; \
	.quad 0; \
	.size __retguard_ ## x, 8; \
	.popsection; \
	.endif
#else
# define RETGUARD_SETUP_OFF(x, reg, off)
# define RETGUARD_SETUP(x, reg)
# define RETGUARD_CHECK(x, reg)
# define RETGUARD_PUSH(reg)
# define RETGUARD_POP(reg)
# define RETGUARD_SYMBOL(x)
#endif

#define	ENTRY(y)	_ENTRY(_C_LABEL(y)); _PROF_PROLOGUE
#define	NENTRY(y)	_NENTRY(_C_LABEL(y))
#define	ASENTRY(y)	_NENTRY(_ASM_LABEL(y)); _PROF_PROLOGUE
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

/* generic retpoline ("return trampoline") generator */
#define	JMP_RETPOLINE(reg)		\
		call	69f		; \
	68:	pause			; \
		lfence			; \
		jmp	68b		; \
		_ALIGN_TRAPS		; \
	69:	mov	%reg,(%rsp)	; \
		ret

#endif /* !_MACHINE_ASM_H_ */
