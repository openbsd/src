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
 *	$OpenBSD: SYS.h,v 1.4 1998/11/20 11:18:29 d Exp $
 */

#include <machine/asm.h>
#include <sys/syscall.h>

#ifdef __STDC__
# define    __ENTRY(p,x)	ENTRY(p##x)
# define    __DO_SYSCALL(x)				\
				movl $(SYS_##x),%eax;	\
				int $0x80
# define    __LABEL2(p,x)	_C_LABEL(p##x)
#else
# define    __ENTRY(p,x)	ENTRY(p/**/x)
# define    __DO_SYSCALL(x)				\
				movl $(SYS_/**/x),%eax;	\
				int $0x80
# define    __LABEL2(p,x)	_C_LABEL(p/**/x)
#endif

/* perform a syscall, set errno */
#define	    __SYSCALL(p,x)				\
			.text;				\
			.align 2;			\
		2:					\
			jmp PIC_PLT(cerror);		\
		__ENTRY(p,x);				\
			__DO_SYSCALL(x);		\
			jc 2b

/* perform a syscall, set errno, return */
# define    __RSYSCALL(p,x)	__SYSCALL(p,x); ret

/* perform a syscall, return */
# define    __PSEUDO(p,x,y)				\
		__ENTRY(p,x);				\
			__DO_SYSCALL(y);		\
			ret

/* jump to the real syscall */
/* XXX shouldn't be here */
# define    __PASSTHRU(p,x)				\
			.globl __LABEL2(p,x);		\
		ENTRY(x);				\
			jmp PIC_PLT(__LABEL2(p,x))

/*
 * Design note:
 *
 * When the syscalls need to be renamed so they can be handled
 * specially by the threaded library, these macros insert `_thread_sys_'
 * in front of their name. This avoids the need to #ifdef _THREAD_SAFE 
 * everywhere that the renamed function needs to be called.
 * The PASSTHRU macro is later used for system calls that don't need
 * wrapping. (XXX its a shame the loader can't do this aliasing)
 */
#ifdef _THREAD_SAFE
/*
 * For the thread_safe versions, we prepend _thread_sys_ to the function
 * name so that the 'C' wrapper can go around the real name.
 */
# define SYSCALL(x)	__SYSCALL(_thread_sys_,x)
# define RSYSCALL(x)	__RSYSCALL(_thread_sys_,x)
# define PSEUDO(x,y)	__PSEUDO(_thread_sys_,x,y)
# define SYSENTRY(x)	__ENTRY(_thread_sys_,x)
# define PASSTHRU(x)	__PASSTHRU(_thread_sys_,x)
#else _THREAD_SAFE
/*
 * The non-threaded library defaults to traditional syscalls where
 * the function name matches the syscall name.
 */
# define SYSCALL(x)	__SYSCALL(,x)
# define RSYSCALL(x)	__RSYSCALL(,x)
# define PSEUDO(x,y)	__PSEUDO(,x,y)
# define SYSENTRY(x)	__ENTRY(,x)
#endif _THREAD_SAFE
	.globl	cerror
