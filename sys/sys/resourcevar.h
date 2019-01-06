/*	$OpenBSD: resourcevar.h,v 1.19 2019/01/06 12:59:45 visa Exp $	*/
/*	$NetBSD: resourcevar.h,v 1.12 1995/11/22 23:01:53 cgd Exp $	*/

/*
 * Copyright (c) 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)resourcevar.h	8.3 (Berkeley) 2/22/94
 */

#ifndef	_SYS_RESOURCEVAR_H_
#define	_SYS_RESOURCEVAR_H_

#include <sys/timeout.h>

/*
 * Kernel shareable process resource limits.  Because this structure
 * is moderately large but changes infrequently, it is shared
 * copy-on-write after forks.
 */
struct plimit {
	struct	rlimit pl_rlimit[RLIM_NLIMITS];
	int	p_refcnt;		/* number of references */
};

/* add user profiling from AST */
#define	ADDUPROF(p)							\
do {									\
	atomic_clearbits_int(&(p)->p_flag, P_OWEUPC);			\
	addupc_task((p), (p)->p_prof_addr, (p)->p_prof_ticks);		\
	(p)->p_prof_ticks = 0;						\
} while (0)

#ifdef _KERNEL
void	 addupc_intr(struct proc *, u_long);
void	 addupc_task(struct proc *, u_long, u_int);
void	 tuagg_unlocked(struct process *, struct proc *);
void	 tuagg(struct process *, struct proc *);
struct tusage;
void	 calctsru(struct tusage *, struct timespec *, struct timespec *,
	    struct timespec *);
void	 calcru(struct tusage *, struct timeval *, struct timeval *,
	    struct timeval *);
struct plimit *limcopy(struct plimit *);
void	limfree(struct plimit *);

void	 ruadd(struct rusage *, struct rusage *);
void	 rucheck(void *);
#define RUCHECK_INTERVAL	1000	/* check interval in msec */
#endif
#endif	/* !_SYS_RESOURCEVAR_H_ */
