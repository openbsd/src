/*	$OpenBSD: netbsd_signal.h,v 1.1 1999/09/14 01:05:25 kstailey Exp $	*/
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
 *	@(#)signal.h	8.4 (Berkeley) 5/4/95
 */

#ifndef	_NETBSD_SYS_SIGNAL_H_
#define	_NETBSD_SYS_SIGNAL_H_


typedef struct {
	u_int32_t	__bits[4];
} netbsd_sigset_t;

/*
 * Signal vector "template" used in sigaction call.
 */
struct	netbsd_sigaction {
	void	(*netbsd_sa_handler) __P((int));/* signal handler */
	netbsd_sigset_t netbsd_sa_mask;		/* signal mask to apply */
	int	netbsd_sa_flags;		/* see signal options below */
};

typedef struct netbsd_sigaltstack {
	void	*netbsd_ss_sp;		/* signal stack base */
	size_t	netbsd_ss_size;		/* signal stack length */
	int	netbsd_ss_flags;	/* SS_DISABLE and/or SS_ONSTACK */
} netbsd_stack_t;

#endif	/* !_NETBSD_SYS_SIGNAL_H_ */
