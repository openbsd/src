/*	$NetBSD: reg.h,v 1.10 1995/08/29 22:40:59 phil Exp $	*/

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

/* Modified for the pc532... 2/1/93 by Phil Nelson
 */

#ifndef _MACHINE_REG_H_
#define _MACHINE_REG_H_

/*
 * Location of the users' stored
 * registers within appropriate frame of 'trap' and 'syscall', relative to
 * base of stack frame.
 * Normal usage is u.u_ar0[XX] in kernel.
 */

/* When referenced during a trap/exception and a syscall,
   registers are at these offsets from p-p_regs*/

#define	REG_R0	(7)
#define	REG_R1	(6)
#define	REG_R2	(5)
#define	REG_R3	(4)
#define	REG_R4	(3)
#define	REG_R5	(2)
#define	REG_R6	(1)
#define	REG_R7	(0)

#define	REG_SP	(8)
#define REG_SB	(9)
#define	REG_FP	(10)
#define	REG_PC	(11)
#define	REG_PSR	(12)

/* The reg struct .. in the order of above. */

struct reg {
	int	r_r7;
	int	r_r6;
	int	r_r5;
	int	r_r4;
	int	r_r3;
	int	r_r2;
	int	r_r1;
	int	r_r0;

	int	r_sp;
	int	r_sb;
	int	r_fp;
	int	r_pc;
	short	r_mod;
	short	r_psr;
};

struct fpreg {
	int    r_fsr;
	double r_freg[8];
};
#endif /* _MACHINE_REG_H_ */
