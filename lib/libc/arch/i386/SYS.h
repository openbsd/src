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
 *	$OpenBSD: SYS.h,v 1.6 2000/01/06 08:50:35 d Exp $
 */

#include <machine/asm.h>
#include <sys/syscall.h>

/*
 * Design note:
 *
 * System calls entry points are really named _thread_sys_{syscall},
 * and weakly aliased to the name {syscall}. This allows the thread
 * library to replace system calls at link time.
 */

#ifdef _NO_WEAK_ALIASES

#ifdef _THREAD_SAFE
/* Use _thread_sys_{syscall} when compiled with -D_THREAD_SAFE */
#ifdef __STDC__
#define	SYSENTRY(x)	ENTRY(_thread_sys_ ## x)
#else /* ! __STDC__ */
#define	SYSENTRY(x)	ENTRY(_thread_sys_ /**/ x)
#endif /* ! __STDC__ */
#else /* ! _THREAD_SAFE */
/* Use {syscall} when compiling without -D_THREAD_SAFE */
#define SYSENTRY(x)	ENTRY(x)
#endif /* ! _THREAD_SAFE */

#else /* WEAK_ALIASES */

/* Use both _thread_sys_{syscall} and [weak] {syscall}. */

#ifdef __STDC__
#define	SYSENTRY(x)					\
			ENTRY(_thread_sys_ ## x)	\
			.weak _C_LABEL(x);		\
			_C_LABEL(x) = _C_LABEL(_thread_sys_ ## x)
#else /* ! __STDC__ */
#define	SYSENTRY(x)					\
			ENTRY(_thread_sys_/**/x)	\
			.weak _C_LABEL(x);		\
			_C_LABEL(x) = _C_LABEL(_thread_sys_/**/x)
#endif /* ! __STDC__ */
#endif /* WEAK_ALIASES */

#define	__DO_SYSCALL(x)					\
			movl $(__CONCAT(SYS_,x)),%eax;	\
			int $0x80

/* perform a syscall, set errno */
#define	SYSCALL(x)					\
			.text;				\
			.align 2;			\
		2:					\
			jmp PIC_PLT(cerror);		\
		SYSENTRY(x);				\
			__DO_SYSCALL(x);		\
			jc 2b

/* perform a syscall, set errno, return */
#define	RSYSCALL(x)					\
			SYSCALL(x); 			\
			ret

/* perform a syscall, return */
#define	PSEUDO(x,y)					\
		SYSENTRY(x);				\
			__DO_SYSCALL(y);		\
			ret

	.globl	cerror
