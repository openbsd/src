/*	$OpenBSD: SYS.h,v 1.5 2023/12/11 22:29:24 deraadt Exp $	*/

/*
 * Copyright (c) 2002,2004 Dale Rahn
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
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/syscall.h>
#include <machine/asm.h>

#define PINSYSCALL(sysno, label)				\
	.pushsection .openbsd.syscalls,"",@progbits		;\
	.p2align 2						;\
	.long label						;\
	.long sysno						;\
	.popsection

#define	DL_SYSCALL(n)						\
	.global	__CONCAT(_dl_,n)				;\
	.type	__CONCAT(_dl_,n), @function			;\
	.align	16,0xcc						;\
__CONCAT(_dl_,n):						;\
	endbr64							;\
	RETGUARD_SETUP(_dl_##n, r11)				;\
	RETGUARD_PUSH(r11)					;\
	movl	$(__CONCAT(SYS_,n)), %eax			;\
	movq	%rcx, %r10					;\
99:	syscall							;\
	PINSYSCALL(__CONCAT(SYS_,n), 99b)			;\
	jnc	1f						;\
	neg	%rax						;\
1:	RETGUARD_POP(r11)					;\
	RETGUARD_CHECK(_dl_##n, r11)				;\
	ret
