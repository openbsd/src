/*	$OpenBSD: reg.h,v 1.1 2004/01/28 01:39:39 mickey Exp $	*/
/*	$NetBSD: reg.h,v 1.1 2003/04/26 18:39:47 fvdl Exp $	*/

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
 *	@(#)reg.h	5.5 (Berkeley) 1/18/91
 */

#ifndef _AMD64_REG_H_
#define _AMD64_REG_H_

#include <machine/mcontext.h>
#include <machine/fpu.h>

/*
 * XXX
 * The #defines aren't used in the kernel, but some user-level code still
 * expects them.
 */

/* When referenced during a trap/exception, registers are at these offsets */

#define tR15	0
#define tR14	1
#define tR13	2
#define tR12	3
#define tR11	4
#define tR10	5
#define tR9	6
#define tR8	7
#define	tRDI	8
#define	tRSI	9
#define	tRBP	10
#define	tRBX	11
#define	tRDX	12
#define	tRCX	13
#define	tRAX	14

#define	tRIP	17
#define	tCS	18
#define	tRFLAGS	19
#define	tRSP	20
#define	tSS	21

/*
 * Registers accessible to ptrace(2) syscall for debugger use.
 * Same as mcontext.__gregs and struct trapframe, they must
 * remain synced (XXX should use common structure).
 */
struct reg {
	long	regs[_NGREG];
};

struct fpreg {
	struct fxsave64 fxstate;
};

#define fp_fcw		fxstate.fx_fcw
#define fp_fsw		fxstate.fx_fsw
#define fp_ftw		fxstate.fx_ftw
#define fp_fop		fxstate.fx_fop
#define fp_rip		fxstate.fx_rip
#define fp_rdp		fxstate.fx_rdp
#define fp_mxcsr	fxstate.fx_mxcsr
#define fp_mxcsr_mask	fxstate.fx_mxcsr_mask
#define fp_st		fxstate.fx_st
#define fp_xmm		fxstate.fx_xmm

#endif /* !_AMD64_REG_H_ */
