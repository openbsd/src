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
 *	$OpenBSD: SYS.h,v 1.13 2002/10/06 23:24:13 art Exp $
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

#ifdef __STDC__
#define	__DO_SYSCALL(x)					\
			movl $(SYS_ ## x),%eax;		\
			int $0x80
#else /* ! __STDC__ */
#define	__DO_SYSCALL(x)					\
			movl $(SYS_/**/x),%eax;		\
			int $0x80
#endif /* ! __STDC__ */

/* perform a syscall */
#define	_SYSCALL_NOERROR(x,y)				\
		SYSENTRY(x);				\
			__DO_SYSCALL(y);

#define	SYSCALL_NOERROR(x)				\
		_SYSCALL_NOERROR(x,x)

/* perform a syscall, set errno */
#define	_SYSCALL(x,y)					\
			.text;				\
			.align 2;			\
		2:					\
			jmp PIC_PLT(__cerror);		\
		_SYSCALL_NOERROR(x,y)			\
			jc 2b

#define	SYSCALL(x)					\
		_SYSCALL(x,x)

/* perform a syscall, return */
#define	PSEUDO_NOERROR(x,y)				\
		_SYSCALL_NOERROR(x,y);			\
			ret

/* perform a syscall, set errno, return */
#define	PSEUDO(x,y)					\
		_SYSCALL(x,y);				\
			ret

/* perform a syscall with the same name, set errno, return */
#define	RSYSCALL(x)					\
			PSEUDO(x,x);

	.globl	__cerror
