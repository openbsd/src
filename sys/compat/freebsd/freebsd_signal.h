/*	$OpenBSD: freebsd_signal.h,v 1.3 2003/06/02 23:28:00 millert Exp $	*/
/*	$NetBSD: signal.h,v 1.42 1998/12/21 10:35:00 drochner Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
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
 *	@(#)signal.h	8.4 (Berkeley) 5/4/95
 */

#ifndef	_FREEBSD_SYS_SIGNAL_H_
#define	_FREEBSD_SYS_SIGNAL_H_

typedef struct {
	u_int32_t	__bits[4];
} freebsd_sigset_t;

union freebsd_sigval {
    /* Members as suggested by Annex C of POSIX 1003.1b. */
    int     freebsd_sigval_int;
    void    *freebsd_sigval_ptr;
};

typedef struct __freebsd_siginfo {
    int     freebsd_si_signo;               /* signal number */
    int     freebsd_si_errno;               /* errno association */
    /*
     * Cause of signal, one of the SI_ macros or signal-specific
     * values, i.e. one of the FPE_... values for SIGFPE. This
     * value is equivalent to the second argument to an old-style
     * FreeBSD signal handler.
     */
    int     freebsd_si_code;                /* signal code */
    int     freebsd_si_pid;                 /* sending process */
    unsigned int freebsd_si_uid;            /* sender's ruid */
    int     freebsd_si_status;              /* exit value */
    void    *freebsd_si_addr;               /* faulting instruction */
    union freebsd_sigval freebsd_si_value;  /* signal value */
    long    freebsd_si_band;                /* band event for SIGPOLL */
    int     __spare__[7];                   /* gimme some slack */
} freebsd_siginfo_t;

/*
 * Signal vector "template" used in sigaction call.
 */
struct freebsd_sigaction {
    union {
	void    (*__freebsd_sa_handler)(int);
	void    (*__freebsd_sa_sigaction)(int, struct __freebsd_siginfo *, void *);
    } __freebsd_sigaction_u;                /* signal handler */
    int     freebsd_sa_flags;               /* see signal options below */
    freebsd_sigset_t freebsd_sa_mask;               /* signal mask to apply */
};
#define freebsd_sa_handler	__freebsd_sigaction_u.__freebsd_sa_handler
#define freebsd_sa_sigaction	__freebsd_sigaction_u.__freebsd_sa_sigaction

#endif	/* !_FREEBSD_SYS_SIGNAL_H_ */
