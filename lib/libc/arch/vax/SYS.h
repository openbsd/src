/*	$OpenBSD: SYS.h,v 1.9 2002/11/03 20:36:43 marc Exp $ */
/*	$NetBSD: SYS.h,v 1.4 1997/05/02 18:15:32 kleink Exp $ */

/*
 * Copyright (c) 1983, 1993
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
 */

#include <machine/asm.h>
#include <sys/syscall.h>

#ifdef __STDC__
#define	_CAT(x,y)	x##y
#define	__ENTRY(p,x)	ENTRY(p##x,0)
#define	__DO_SYSCALL(x)	chmk $ SYS_ ## x
#else
#define	_CAT(x,y)	x/**/y
#define	__ENTRY(p,x)	ENTRY(p/**/x,0)
#define	__DO_SYSCALL(x)	chmk $ SYS_/**/x
#endif

#define	__SYSCALL(p,x,y)						\
	err:	jmp cerror;						\
	__ENTRY(p,x);							\
		__DO_SYSCALL(y);					\
		jcs err

#define	__PSEUDO(p,x,y)							\
	err:	jmp cerror;						\
	__ENTRY(p,x);							\
		__DO_SYSCALL(y);					\
		jcs err;						\
		ret

#define	__PSEUDO_NOERROR(p,x,y)						\
	__ENTRY(p,x);							\
		__DO_SYSCALL(y);					\
		ret

/*
 * For the thread_safe versions, we prepend _thread_sys_ to the function
 * name so that the 'C' wrapper can go around the real name.
 */
#define	SYSCALL(x)		__SYSCALL(_thread_sys_,x,x)
#define	RSYSCALL(x)		__PSEUDO(_thread_sys_,x,x)
#define	PSEUDO(x,y)		__PSEUDO(_thread_sys_,x,y)
#define	PSEUDO_NOERROR(x,y)	__PSEUDO_NOERROR(_thread_sys_,x,y)
#define	SYSENTRY(x)		__ENTRY(_thread_sys_,x)
#define	SYSNAME(x)		_CAT(__thread_sys_,x)

	.globl	cerror
