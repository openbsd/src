/* $OpenBSD: signal.h,v 1.1 2016/12/17 23:38:33 patrick Exp $ */
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
 *	@(#)signal.h	8.1 (Berkeley) 6/10/93
 */

#ifndef _MACHINE_SIGNAL_H_
#define _MACHINE_SIGNAL_H_

#include <sys/cdefs.h>

#if !defined(__LANGUAGE_ASSEMBLY)
#include <sys/types.h>

/*
 * Machine-dependent signal definitions
 */
typedef int sig_atomic_t;

#if __BSD_VISIBLE || __XPG_VISIBLE >= 420
/*
 * Information pushed on stack when a signal is delivered.
 * This is used by the kernel to restore state following
 * execution of the signal handler.  It is also made available
 * to the handler to allow it to restore state properly if
 * a non-standard exit is performed.
 */
struct	sigcontext {
	int	__sc_unused;
	int	sc_mask;		/* signal mask to restore (old style) */

	uint64_t sc_sp;
	uint64_t sc_lr;
	uint64_t sc_elr;
	uint64_t sc_spsr;
	uint64_t sc_x[30];
};
#endif /* __BSD_VISIBLE || __XPG_VISIBLE >= 420 */

#endif /* !__LANGUAGE_ASSEMBLY */

#endif	/* !_MACHINE_SIGNAL_H_ */
