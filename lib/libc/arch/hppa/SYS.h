/*	$OpenBSD: SYS.h,v 1.21 2015/09/10 13:29:09 guenther Exp $	*/

/*
 * Copyright (c) 1998-2002 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF MIND
 * USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/syscall.h>
#include <machine/asm.h>
#undef _LOCORE
#define _LOCORE
#include <machine/frame.h>
#include <machine/vmparam.h>
#undef _LOCORE

/*
 * We define a hidden alias with the prefix "_libc_" for each global symbol
 * that may be used internally.  By referencing _libc_x instead of x, other
 * parts of libc prevent overriding by the application and avoid unnecessary
 * relocations.
 */
#define _HIDDEN(x)		_libc_##x
#define _HIDDEN_ALIAS(x,y)			\
	STRONG_ALIAS(_HIDDEN(x),y)		!\
	.hidden _HIDDEN(x)
#define _HIDDEN_FALIAS(x,y)			\
	_HIDDEN_ALIAS(x,y)			!\
	.type _HIDDEN(x),@function

/*
 * For functions implemented in ASM that aren't syscalls.
 *   EXIT_STRONG(x)	Like DEF_STRONG() in C; for standard/reserved C names
 *   EXIT_WEAK(x)	Like DEF_WEAK() in C; for non-ISO C names
 *   ALTEXIT_STRONG(x) and ALTEXIT_WEAK()
 *			Matching macros for ALTENTRY functions
 */
#define	ALTEXIT_STRONG(x)					\
			_HIDDEN_FALIAS(x,x)			!\
			.size _HIDDEN(x), . - _HIDDEN(x)
#define	ALTEXIT_WEAK(x)	ALTEXIT_STRONG(x)			!\
			.weak x
#define	EXIT_STRONG(x)	EXIT(x)					!\
			ALTEXIT_STRONG(x)
#define	EXIT_WEAK(x)	EXIT_STRONG(x)				!\
			.weak x
 

#define SYSENTRY(x)				!\
LEAF_ENTRY(__CONCAT(_thread_sys_,x))		!\
	WEAK_ALIAS(x,__CONCAT(_thread_sys_,x))
#define SYSENTRY_HIDDEN(x)			!\
LEAF_ENTRY(__CONCAT(_thread_sys_,x))
#define	SYSEXIT(x)				!\
	SYSEXIT_HIDDEN(x)			!\
	.size x, . - x
#define	SYSEXIT_HIDDEN(x)			!\
	EXIT(__CONCAT(_thread_sys_,x))		!\
	_HIDDEN_FALIAS(x,_thread_sys_##x)	!\
	.size _HIDDEN(x), . - _HIDDEN(x)

#define	SYSCALL(x)				!\
	stw	rp, HPPA_FRAME_ERP(sr0,sp)	!\
	ldil	L%SYSCALLGATE, r1		!\
	ble	4(sr7, r1)			!\
	ldi	__CONCAT(SYS_,x), t1		!\
	.import	__cerror, code			!\
	comb,<>	r0, t1, __cerror		!\
	ldw	HPPA_FRAME_ERP(sr0,sp), rp

#define	PSEUDO(x,y)				!\
SYSENTRY(x)					!\
	SYSCALL(y)				!\
	bv	r0(rp)				!\
	nop					!\
SYSEXIT(x)
#define	PSEUDO_HIDDEN(x,y)			!\
SYSENTRY_HIDDEN(x)				!\
	SYSCALL(y)				!\
	bv	r0(rp)				!\
	nop					!\
SYSEXIT_HIDDEN(x)

#define	PSEUDO_NOERROR(x,y)			!\
SYSENTRY(x)					!\
	stw	rp, HPPA_FRAME_ERP(sr0,sp)	!\
	ldil	L%SYSCALLGATE, r1		!\
	ble	4(sr7, r1)			!\
	ldi	__CONCAT(SYS_,y), t1		!\
	ldw	HPPA_FRAME_ERP(sr0,sp), rp	!\
	bv	r0(rp)				!\
	nop					!\
SYSEXIT(x)

#define	RSYSCALL(x)		PSEUDO(x,x)
#define	RSYSCALL_HIDDEN(x)	PSEUDO_HIDDEN(x,x)

