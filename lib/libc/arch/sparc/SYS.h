/*-
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
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
 *	$OpenBSD: SYS.h,v 1.12 2002/08/11 23:01:30 art Exp $
 */

#include <machine/asm.h>
#include <sys/syscall.h>
#include <machine/trap.h>

#ifdef __STDC__
#define _CAT(x,y) x##y
#else
#define _CAT(x,y) x/**/y
#endif

#define __ENTRY(p,x) ENTRY(_CAT(p,x)) ; .weak x ; x = _CAT(p,x)

/*
 * ERROR branches to cerror.
 */
#ifdef PIC
#define	ERROR() \
	PIC_PROLOGUE(%g1,%g2); \
	ld [%g1+_C_LABEL(__cerror)],%g2; jmp %g2; nop
#else
#define	ERROR() \
	sethi %hi(_C_LABEL(__cerror)),%g1; \
	or %lo(_C_LABEL(__cerror)),%g1,%g1; \
	jmp %g1; \
	nop
#endif

/*
 * SYSCALL is used when further action must be taken before returning.
 * Note that it adds a `nop' over what we could do, if we only knew what
 * came at label 1....
 */
#define	__SYSCALL(p,x) \
	__ENTRY(p,x); mov _CAT(SYS_,x),%g1; t ST_SYSCALL; bcc 1f; nop; ERROR(); 1:

/*
 * RSYSCALL is used when the system call should just return.  Here
 * we use the SYSCALL_G2RFLAG to put the `success' return address in %g2
 * and avoid a branch.
 */
#define	__RSYSCALL(p,x) \
	__ENTRY(p,x); mov (_CAT(SYS_,x))|SYSCALL_G2RFLAG,%g1; add %o7,8,%g2; \
	t ST_SYSCALL; ERROR()

/*
 * PSEUDO(x,y) is like RSYSCALL(y) except that the name is x.
 */
#define	__PSEUDO(p,x,y) \
	__ENTRY(p,x); mov (_CAT(SYS_,y))|SYSCALL_G2RFLAG,%g1; add %o7,8,%g2; \
	t ST_SYSCALL; ERROR()

/*
 * PSEUDO_NOERROR(x,y) is like PSEUDO(x,y) except that errno is not set.
 */
#define	__PSEUDO_NOERROR(p,x,y) \
	__ENTRY(p,x); mov (_CAT(SYS_,y))|SYSCALL_G2RFLAG,%g1; add %o7,8,%g2; \
	t ST_SYSCALL

# define SYSCALL(x)		__SYSCALL(_thread_sys_,x)
# define RSYSCALL(x)		__RSYSCALL(_thread_sys_,x)
# define PSEUDO(x,y)		__PSEUDO(_thread_sys_,x,y)
# define PSEUDO_NOERROR(x,y)	__PSEUDO_NOERROR(_thread_sys_,x,y)
# define SYSENTRY(x)		__ENTRY(_thread_sys_,x)

	.globl	_C_LABEL(__cerror)
