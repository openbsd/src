/*-
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *      $OpenBSD: SYS.h,v 1.7 2015/04/07 01:27:06 guenther Exp $ 
 */

#include <sys/syscall.h>
#include <machine/asm.h>

#define CERROR		_C_LABEL(__cerror)
#define _CERROR		_C_LABEL(___cerror)

# define __ENTRY(p,x)		ENTRY(p ## x)

# define __DO_SYSCALL(x)				\
				li	v0,SYS_ ## x;	\
				syscall

# define __LEAF2(p,x,sz)	LEAF(p ## x, sz) \
				WEAK_ALIAS(x, p ## x);

# define __END2(p,x)		END(p ## x)

# define __CLABEL2(p,x)		_C_LABEL(p ## x)

#define __PSEUDO_NOERROR(p,x,y)				\
		__LEAF2(p,x, 0);			\
			__DO_SYSCALL(y);		\
			j	ra;			\
		__END2(p,x)

#define __PSEUDO(p,x,y)   				\
		__LEAF2(p,x,32);			\
			PTR_SUBU sp,32;			\
			SETUP_GP64(16,__CLABEL2(p,x));	\
			__DO_SYSCALL(y);		\
			bne	a3,zero,err;		\
			RESTORE_GP64;			\
			PTR_ADDU sp,32;			\
			j	ra;			\
		err:	LA	t9,CERROR;		\
			RESTORE_GP64;			\
			PTR_ADDU sp,32;			\
			jr	t9;			\
		__END2(p,x)
#define __PSEUDO_HIDDEN(p,x,y)   			\
		LEAF(p ## x,32);			\
			PTR_SUBU sp,32;			\
			SETUP_GP64(16,__CLABEL2(p,x));	\
			__DO_SYSCALL(y);		\
			bne	a3,zero,err;		\
			RESTORE_GP64;			\
			PTR_ADDU sp,32;			\
			j	ra;			\
		err:	LA	t9,CERROR;		\
			RESTORE_GP64;			\
			PTR_ADDU sp,32;			\
			jr	t9;			\
		END(p ## x)


#define RSYSCALL(x)		__PSEUDO(_thread_sys_,x,x)
#define RSYSCALL_HIDDEN(x)	__PSEUDO_HIDDEN(_thread_sys_,x,x)
#define PSEUDO(x,y)		__PSEUDO(_thread_sys_,x,y)
#define PSEUDO_NOERROR(x,y)	__PSEUDO_NOERROR(_thread_sys_,x,y)

#define	SYSLEAF(x, sz)		__LEAF2(_thread_sys_,x, sz)
#define	SYSEND(x)		__END2(_thread_sys_,x)

