/*	$OpenBSD: mcontext.h,v 1.2 2008/06/26 05:42:09 ray Exp $	*/
/*	$NetBSD: mcontext.h,v 1.1 2003/04/26 18:39:44 fvdl Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Klaus Klein.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _AMD64_MCONTEXT_H_
#define _AMD64_MCONTEXT_H_

/*
 * Layout of mcontext_t according to the System V Application Binary Interface,
 * Intel386(tm) Architecture Processor Supplement, Fourth Edition.
 */  

/*
 * General register state
 */
#define _NGREG		26
typedef	long		__greg_t;
typedef	__greg_t	__gregset_t[_NGREG];

/*
 * This is laid out to match trapframe and intrframe (see <machine/frame.h>).
 * Hence, memcpy between gregs and a trapframe is possible.
 */
#define _REG_RDI	0
#define _REG_RSI	1
#define _REG_RDX	2
#define _REG_RCX	3
#define _REG_R8		4
#define _REG_R9		5
#define _REG_R10	6
#define _REG_R11	7
#define _REG_R12	8
#define _REG_R13	9
#define _REG_R14	10
#define _REG_R15	11
#define _REG_RBP	12
#define _REG_RBX	13
#define _REG_RAX	14
#define _REG_GS		15
#define _REG_FS		16
#define _REG_ES		17
#define _REG_DS		18
#define _REG_TRAPNO	19
#define _REG_ERR	20
#define _REG_RIP	21
#define _REG_CS		22
#define _REG_RFL	23
#define _REG_URSP	24
#define _REG_SS		25

/*
 * Floating point register state
 */
typedef char __fpregset_t[512];

/*
 * The padding below is to make __fpregs have a 16-byte aligned offset
 * within ucontext_t.
 */

typedef struct {
	__gregset_t	__gregs;
	long 		__pad;
	__fpregset_t	__fpregs;
} mcontext_t;

#define _UC_UCONTEXT_ALIGN	(~0xf)

#ifdef _KERNEL
#define _UC_MACHINE_SP(uc)	((uc)->uc_mcontext.__gregs[_REG_URSP])
#endif

#endif	/* !_AMD64_MCONTEXT_H_ */
