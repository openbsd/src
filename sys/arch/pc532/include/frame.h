/*	$NetBSD: frame.h,v 1.4 1994/10/26 08:24:28 cgd Exp $	*/

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
 *	@(#)frame.h	5.2 (Berkeley) 1/18/91
 */

#ifndef _MACHINE_FRAME_H_
#define _MACHINE_FRAME_H_

#include <sys/signal.h>
#include <machine/reg.h>

/*
 * System stack frames.
 */

/*
 * Exception/Trap Stack Frame
 */

struct trapframe {
	long	tf_msr;		/* For abt.  0 for others. */
	long	tf_tear;	/* For abt.  0 for others. */
	long	tf_trapno;
	long    tf_reg[8];	/* R7 - R0 from enter */
	long	tf_usp;
	long	tf_sb;
	long    tf_fp;		/* From enter */
	/* below portion defined in 532 hardware */
	long	tf_pc;
	u_short	tf_mod;		/* Not used in direct excption mode. */
	u_short	tf_psr;
};

/* Interrupt stack frame */

struct intrframe {
	long	if_vec;
	long	if_pl;		/* the "processor level" for clock. */
	long    if_reg[8];	/* R7 - R0 from enter */
	long	if_usp;
	long	if_sb;
	long    if_fp;		/* From enter */
	/* below portion defined in 532 hardware */
	long	if_pc;
	u_short	if_mod;		/* Not used in direct excption mode. */
	u_short	if_psr;
};

/*
 * System Call Stack Frame
 */

struct syscframe {
	long    sf_reg[8];	/* R7 - R0 from enter */
	long	sf_usp;
	long	sf_sb;
	long    sf_fp;		/* From enter */
	/* below portion defined in 532 hardware */
	long	sf_pc;
	u_short	sf_mod;		/* Not used in direct excption mode. */
	u_short	sf_psr;
};

/*
 * Signal frame
 */
struct sigframe {
	int	sf_signum;
	int	sf_code;
	struct	sigcontext *sf_scp;
	sig_t	sf_handler;
	struct	sigcontext sf_sc;
} ;

#endif
