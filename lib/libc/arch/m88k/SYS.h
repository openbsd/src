/* *	$OpenBSD: SYS.h,v 1.6 2003/01/02 20:25:29 miod Exp $*/
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
 *	from: @(#)SYS.h	5.5 (Berkeley) 5/7/91
 */

#include <sys/syscall.h>
#include <machine/asm.h>

#ifdef __STDC__

#define	_SYSCALL(x,y)	align 8; \
			ENTRY(x); \
			ld r10,r31,32; \
			ld r11,r31,36; \
			ld r12,r31,40; \
			or r13,r0, SYS_ ## y; \
			tb0 0, r0, 128; \
			br __cerror
#define	SYSCALL(x)	_SYSCALL(x,x)
#define	RSYSCALL(x)	SYSCALL(x) ;\
			jmp r1
#define	PSEUDO(x,y)	_SYSCALL(x,y); \
			jmp r1
#define	PSEUDO_NOERROR(x,y)	ENTRY(x); ;\
			or r13,r0, SYS_ ## y; \
			tb0 0,r0,128; or r0,r0,r0;jmp r1

#else /* !__STDC__ */

#define	_SYSCALL(x,y)	align 8; \
			ENTRY(x); \
			ld r10,r31,32; \
			ld r11,r31,36; \
			ld r12,r31,40; \
			or r13,r0, SYS_/**/y; \
			tb0 0, r0, 128; \
			br __cerror
#define	SYSCALL(x)	_SYSCALL(x,x)
#define	RSYSCALL(x)	SYSCALL(x); \
			jmp r1
#define	PSEUDO(x,y)	_SYSCALL(x,y); \
			jmp r1
#define	PSEUDO_NOERROR(x,y)	ENTRY(x); \
			or r13,r0, SYS_/**/y; \
			tb0 0,r0,128; or r0,r0,r0; jmp r1
#endif /* !__STDC__ */

#define	ASMSTR		.asciz

	.globl	__cerror
