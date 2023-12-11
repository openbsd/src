/*	$OpenBSD: SYS.h,v 1.3 2023/12/11 22:29:24 deraadt Exp $	*/

/*
 * Copyright (c) 2004 Michael Shalayeff
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
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/syscall.h>
#include <machine/asm.h>
#define _LOCORE
#include <machine/frame.h>
#include <machine/vmparam.h>
#undef  _LOCORE

#define PINSYSCALL(sysno, label)				\
	.pushsection .openbsd.syscalls,"",@progbits		!\
	.p2align 2						!\
	.long label						!\
	.long sysno						!\
	.popsection

#define	DL_SYSCALL(x)						\
ENTRY(__CONCAT(_dl_,x),0)					!\
	stw	rp, HPPA_FRAME_ERP(sr0,sp)			!\
	ldil	L%SYSCALLGATE, r1				!\
99:	ble	4(sr7, r1)					!\
	PINSYSCALL(__CONCAT(SYS_,x), 99b)			!\
	 ldi	__CONCAT(SYS_,x), t1				!\
	comb,<>	r0, t1, _dl_sysexit				!\
	ldw	HPPA_FRAME_ERP(sr0,sp), rp			!\
	bv	r0(rp)						!\
	nop							!\
_dl_sysexit							!\
	bv	r0(rp)						!\
	sub	r0, ret0, ret0					!\
EXIT(__CONCAT(_dl_,x))
