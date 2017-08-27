/*	$OpenBSD: SYS.h,v 1.1 2017/08/27 21:59:52 deraadt Exp $ */

/*
 * Copyright (c) 2006 Dale Rahn
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

#include <machine/asm.h>
#include <sys/syscall.h>

#define SYSTRAP(x)					\
	mov.l	903f, r0;				\
	.word	0xc380;	/* trapa #0x80; */		\
	bra	904f;					\
	 nop;						\
	.align	2;					\
 903:	.long	(SYS_ ## x);				\
 904:

#define DL_SYSCALL(n)					\
	.global		__CONCAT(_dl_,n)		;\
	.type		__CONCAT(_dl_,n)%function	;\
__CONCAT(_dl_,n):					;\
	SYSTRAP(n)					;\
	bf	.L_cerr					;\
	 nop						;\
	rts						;\
	 nop

.L_cerr:
	neg	r0, r0
	rts
	 nop
