/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 *	$OpenBSD: SYS.h,v 1.10 2002/11/03 20:36:43 marc Exp $
 */

#include <sys/syscall.h>
#include <machine/asm.h>

#define _IMMEDIATE_	#

#ifdef __STDC__
# define	__ENTRY(p,x)	ENTRY(p##x)
# define	__DO_SYSCALL(x)					\
				movl _IMMEDIATE_ SYS_##x, d0;	\
				trap _IMMEDIATE_ 0
# define	__LABEL2(p,x)	_C_LABEL(p##x)
#else
# define	__ENTRY(p,x)	ENTRY(p/**/x)
# define	__DO_SYSCALL(x)					\
				movl _IMMEDIATE_ SYS_/**/x, d0;	\
				trap _IMMEDIATE_ 0
# define	__LABEL2(p,x)	_C_LABEL(p/**/x)
#endif

/* perform a syscall */

#define		__SYSCALL_NOERROR(p,x,y)			\
			__ENTRY(p,x);				\
				__DO_SYSCALL(y)

/* perform a syscall, set errno */

#define		__SYSCALL(p,x,y)				\
				.even;				\
			err:	jra	cerror;			\
			__SYSCALL_NOERROR(p,x,y);		\
				jcs err

/* perform a syscall, return */

#define		__PSEUDO_NOERROR(p,x,y)				\
			__SYSCALL_NOERROR(p,x,y);		\
				rts

/* perform a syscall, set errno, return */

#define		__PSEUDO(p,x,y)					\
			__SYSCALL(p,x,y);			\
				rts


/*
 * For the thread_safe versions, we prepend _thread_sys_ to the function
 * name so that the 'C' wrapper can go around the real name.
 */
# define SYSCALL(x)     	__SYSCALL(_thread_sys_,x,x)
# define RSYSCALL(x)    	__PSEUDO(_thread_sys_,x,x)
# define PSEUDO(x,y)    	__PSEUDO(_thread_sys_,x,y)
# define PSEUDO_NOERROR(x,y)	__PSEUDO_NOERROR(_thread_sys_,x,y)
# define SYSENTRY(x)    	__ENTRY(_thread_sys_,x)

#define	ASMSTR		.asciz

	.globl	cerror
