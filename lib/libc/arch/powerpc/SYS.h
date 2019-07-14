/*	$OpenBSD: SYS.h,v 1.23 2019/07/14 03:36:37 guenther Exp $	*/
/*-
 * Copyright (c) 1994
 *	Andrew Cagney.  All rights reserved.
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
 *	from: @(#)SYS.h	8.1 (Berkeley) 6/4/93
 */

#include <sys/syscall.h>

/* r0 will be a non zero errno if there was an error, while r3/r4 will
   contain the return value */

#include "machine/asm.h"


/* offsetof(struct tib, tib_errno) - offsetof(struct tib, __tib_tcb) */
#define	TCB_OFFSET_ERRNO	(-8)
/* from <powerpc/tcb.h>: TCB address == %r2 - TCB_OFFSET */
#define	TCB_OFFSET		0x7000

/* offset of errno from %r2 */
#define	R2_OFFSET_ERRNO		(-TCB_OFFSET + TCB_OFFSET_ERRNO)

/*
 * We define a hidden alias with the prefix "_libc_" for each global symbol
 * that may be used internally.  By referencing _libc_x instead of x, other
 * parts of libc prevent overriding by the application and avoid unnecessary
 * relocations.
 */
#define _HIDDEN(x)		_libc_##x
#define _HIDDEN_ALIAS(x,y)			\
	STRONG_ALIAS(_HIDDEN(x),y);		\
	.hidden _HIDDEN(x)
#define _HIDDEN_FALIAS(x,y)			\
	_HIDDEN_ALIAS(x,y);			\
	.type _HIDDEN(x),@function

/*
 * For functions implemented in ASM that aren't syscalls.
 *   END_STRONG(x)	Like DEF_STRONG() in C; for standard/reserved C names
 *   END_WEAK(x)	Like DEF_WEAK() in C; for non-ISO C names
 */
#define	END_STRONG(x)	END(x); _HIDDEN_FALIAS(x,x); END(_HIDDEN(x))
#define	END_WEAK(x)	END_STRONG(x); .weak x


#define _CONCAT(x,y)	x##y
#define PSEUDO_PREFIX(p,x,y)	\
			ENTRY(p##x) \
				li 0, SYS_##y ; \
				/* sc */
#define PSEUDO_SUFFIX		cmpwi 0, 0 ; \
				beqlr+ ; \
				stw	0, R2_OFFSET_ERRNO(2); \
				li	3, -1; \
				li	4, -1; /* for __syscall(lseek) */ \
				blr

#define PSEUDO_NOERROR_SUFFIX	blr

#define __END_HIDDEN(p,x)	END(p##x);			\
				_HIDDEN_FALIAS(x,p##x);		\
				END(_HIDDEN(x))
#define __END(p,x)		__END_HIDDEN(p,x); END(x)


#define ALIAS(x,y)		WEAK_ALIAS(y,_CONCAT(x,y));
		
#define PREFIX_HIDDEN(x)	PSEUDO_PREFIX(_thread_sys_,x,x)
#define PREFIX(x)		ALIAS(_thread_sys_,x) \
				PREFIX_HIDDEN(x)
#define	PSEUDO_NOERROR(x,y)	ALIAS(_thread_sys_,x) \
				PSEUDO_PREFIX(_thread_sys_,x,y) ; \
				sc ; \
				PSEUDO_NOERROR_SUFFIX; \
				__END(_thread_sys_,x)

#define	PSEUDO_HIDDEN(x,y)	PSEUDO_PREFIX(_thread_sys_,x,y) ; \
				sc ; \
				PSEUDO_SUFFIX; \
				__END_HIDDEN(_thread_sys_,x)
#define	PSEUDO(x,y)		ALIAS(_thread_sys_,x) \
				PSEUDO_HIDDEN(x,y); \
				END(x)

#define RSYSCALL(x)		PSEUDO(x,x)
#define RSYSCALL_HIDDEN(x)	PSEUDO_HIDDEN(x,x)
#define SYSCALL_END_HIDDEN(x)	__END_HIDDEN(_thread_sys_,x)
#define SYSCALL_END(x)		__END(_thread_sys_,x)

