/*	$OpenBSD: signal.h,v 1.2 2004/08/10 20:28:13 deraadt Exp $	*/

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Ralph Campbell.
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
 *	@(#)signal.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _MIPS_SIGNAL_H_
#define _MIPS_SIGNAL_H_

#if !defined(__LANGUAGE_ASSEMBLY)
#include <sys/types.h>

/*
 * Machine-dependent signal definitions
 */
typedef int sig_atomic_t;

#ifndef _ANSI_SOURCE

/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */
struct	sigcontext {
	long	sc_onstack;	/* sigstack state to restore */
	long	 sc_mask;	/* signal mask to restore */
	register_t sc_pc;	/* pc at time of signal */
	register_t sc_regs[32];	/* processor regs 0 to 31 */
	register_t mullo;	/* mullo and mulhi registers... */
	register_t mulhi;	/* mullo and mulhi registers... */
	f_register_t sc_fpregs[33]; /* fp regs 0 to 31 and csr */
	long	sc_fpused;	/* fp has been used */
	long	sc_fpc_eir;	/* floating point exception instruction reg */
	long	xxx[8];		/* XXX reserved */
};
#endif	/* !_ANSI_SOURCE */

#else /* __LANGUAGE_ASSEMBLY */
#define SC_ONSTACK	(0 * REGSZ)
#define	SC_MASK		(1 * REGSZ)
#define	SC_PC		(2 * REGSZ)
#define	SC_REGS		(3 * REGSZ)
#define	SC_MULLO	(35 * REGSZ)
#define	SC_MULHI	(36 * REGSZ)
#define	SC_FPREGS	(37 * REGSZ)
#define	SC_FPUSED	(70 * REGSZ)
#define	SC_FPC_EIR	(71 * REGSZ)
#endif /* __LANGUAGE_ASSEMBLY */

#endif	/* !_MIPS_SIGNAL_H_ */
