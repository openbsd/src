/*	$OpenBSD: ptrace.h,v 1.2 1996/03/03 12:12:09 niklas Exp $	*/
/*	$NetBSD: ptrace.h,v 1.21 1996/02/09 18:25:26 christos Exp $	*/

/*-
 * Copyright (c) 1984, 1993
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
 *	@(#)ptrace.h	8.2 (Berkeley) 1/4/94
 */

#ifndef	_SYS_PTRACE_H_
#define	_SYS_PTRACE_H_

#define	PT_TRACE_ME	0	/* child declares it's being traced */
#define	PT_READ_I	1	/* read word in child's I space */
#define	PT_READ_D	2	/* read word in child's D space */
#define	PT_WRITE_I	4	/* write word in child's I space */
#define	PT_WRITE_D	5	/* write word in child's D space */
#define	PT_CONTINUE	7	/* continue the child */
#define	PT_KILL		8	/* kill the child process */
#define	PT_ATTACH	9	/* attach to running process */
#define	PT_DETACH	10	/* detach from running process */

#define	PT_FIRSTMACH	32	/* for machine-specific requests */
#include <machine/ptrace.h>	/* machine-specific requests, if any */

#ifdef _KERNEL

#if defined(PT_GETREGS) || defined(PT_SETREGS)
struct reg;
#endif
#if defined(PT_GETFPREGS) || defined(PT_SETFPREGS)
struct fpreg;
#endif

void	proc_reparent __P((struct proc *child, struct proc *newparent));
#ifdef PT_GETFPREGS
int	process_read_fpregs __P((struct proc *p, struct fpreg *regs));
#endif
#ifdef PT_GETREGS
int	process_read_regs __P((struct proc *p, struct reg *regs));
#endif
int	process_set_pc __P((struct proc *p, caddr_t addr));
int	process_sstep __P((struct proc *p, int sstep));
#ifdef PT_SETFPREGS
int	process_write_fpregs __P((struct proc *p, struct fpreg *regs));
#endif
#ifdef PT_SETREGS
int	process_write_regs __P((struct proc *p, struct reg *regs));
#endif

#ifndef FIX_SSTEP
#define FIX_SSTEP(p)
#endif

int	trace_req __P((struct proc *));

#else /* !_KERNEL */

#include <sys/cdefs.h>

__BEGIN_DECLS
int	ptrace __P((int _request, pid_t _pid, caddr_t _addr, int _data));
__END_DECLS

#endif /* !_KERNEL */

#endif	/* !_SYS_PTRACE_H_ */
