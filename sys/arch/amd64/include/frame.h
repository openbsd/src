/*	$OpenBSD: frame.h,v 1.5 2011/03/23 16:54:34 pirofti Exp $	*/
/*	$NetBSD: frame.h,v 1.1 2003/04/26 18:39:40 fvdl Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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

/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)frame.h	5.2 (Berkeley) 1/18/91
 */

/*
 * Adapted for NetBSD/amd64 by fvdl@wasabisystems.com
 */

#ifndef _MACHINE_FRAME_H_
#define _MACHINE_FRAME_H_

#include <sys/signal.h>
#include <machine/fpu.h>

/*
 * System stack frames.
 */

/*
 * Exception/Trap Stack Frame
 */
struct trapframe {
	int64_t	tf_rdi;
	int64_t	tf_rsi;
	int64_t	tf_rdx;
	int64_t	tf_rcx;
	int64_t tf_r8;
	int64_t tf_r9;
	int64_t tf_r10;
	int64_t tf_r11;
	int64_t tf_r12;
	int64_t tf_r13;
	int64_t tf_r14;
	int64_t tf_r15;
	int64_t	tf_rbp;
	int64_t	tf_rbx;
	int64_t	tf_rax;
	int64_t	tf_gs;
	int64_t	tf_fs;
	int64_t	tf_es;
	int64_t	tf_ds;
	int64_t	tf_trapno;
	/* below portion defined in hardware */
	int64_t	tf_err;
	int64_t	tf_rip;
	int64_t	tf_cs;
	int64_t	tf_rflags;
	/* These are pushed unconditionally on the x86-64 */
	int64_t	tf_rsp;
	int64_t	tf_ss;
};

/*
 * Interrupt stack frame
 */
struct intrframe {
	int64_t	if_ppl;
	int64_t	if_rdi;
	int64_t	if_rsi;
	int64_t	if_rdx;
	int64_t	if_rcx;
	int64_t if_r8;
	int64_t if_r9;
	int64_t if_r10;
	int64_t if_r11;
	int64_t if_r12;
	int64_t if_r13;
	int64_t if_r14;
	int64_t if_r15;
	int64_t	if_rbp;
	int64_t	if_rbx;
	int64_t	if_rax;
	int64_t	tf_gs;
	int64_t	tf_fs;
	int64_t	tf_es;
	int64_t	tf_ds;
	u_int64_t __if_trapno; /* for compat with trap frame - trapno */
	u_int64_t __if_err;	/* for compat with trap frame - err */
	/* below portion defined in hardware */
	int64_t	if_rip;
	int64_t	if_cs;
	int64_t	if_rflags;
	/* These are pushed unconditionally on the x86-64 */
	int64_t	if_rsp;
	int64_t	if_ss;
};

/*
 * Stack frame inside cpu_switch()
 */
struct switchframe {
	int64_t	sf_r15;
	int64_t	sf_r14;
	int64_t	sf_r13;
	int64_t	sf_r12;
	int64_t sf_rbp;
	int64_t	sf_rbx;
	int64_t	sf_rip;
};

#endif  /* _MACHINE_FRAME_H_ */
