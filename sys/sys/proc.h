/*	$OpenBSD: proc.h,v 1.26 2000/01/28 19:45:03 art Exp $	*/
/*	$NetBSD: proc.h,v 1.44 1996/04/22 01:23:21 christos Exp $	*/

/*-
 * Copyright (c) 1986, 1989, 1991, 1993
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
 *	@(#)proc.h	8.8 (Berkeley) 1/21/94
 */

#ifndef _SYS_PROC_H_
#define	_SYS_PROC_H_

#include <machine/proc.h>		/* Machine-dependent proc substruct. */
#include <sys/select.h>			/* For struct selinfo. */
#include <sys/queue.h>

/*
 * One structure allocated per session.
 */
struct	session {
	int	s_count;		/* Ref cnt; pgrps in session. */
	struct	proc *s_leader;		/* Session leader. */
	struct	vnode *s_ttyvp;		/* Vnode of controlling terminal. */
	struct	tty *s_ttyp;		/* Controlling terminal. */
	char	s_login[MAXLOGNAME];	/* Setlogin() name. */
};

/*
 * One structure allocated per process group.
 */
struct	pgrp {
	LIST_ENTRY(pgrp) pg_hash;	/* Hash chain. */
	LIST_HEAD(, proc) pg_members;	/* Pointer to pgrp members. */
	struct	session *pg_session;	/* Pointer to session. */
	pid_t	pg_id;			/* Pgrp id. */
	int	pg_jobc;	/* # procs qualifying pgrp for job control */
};

/*
 * One structure allocated per emulation.
 */
struct exec_package;
struct ps_strings;
union sigval;

struct	emul {
	char	e_name[8];		/* Symbolic name */
	int	*e_errno;		/* Errno array */
					/* Signal sending function */
	void	(*e_sendsig) __P((sig_t, int, int, u_long, int, union sigval));
	int	e_nosys;		/* Offset of the nosys() syscall */
	int	e_nsysent;		/* Number of system call entries */
	struct sysent *e_sysent;	/* System call array */
	char	**e_syscallnames;	/* System call name array */
	int	e_arglen;		/* Extra argument size in words */
					/* Copy arguments on the stack */
	void	*(*e_copyargs) __P((struct exec_package *, struct ps_strings *,
				    void *, void *));
					/* Set registers before execution */
	void	(*e_setregs) __P((struct proc *, struct exec_package *,
				  u_long, register_t *));
	int	(*e_fixup) __P((struct proc *, struct exec_package *));
	char	*e_sigcode;		/* Start of sigcode */
	char	*e_esigcode;		/* End of sigcode */
};

/*
 * Description of a process.
 *
 * This structure contains the information needed to manage a thread of
 * control, known in UN*X as a process; it has references to substructures
 * containing descriptions of things that the process uses, but may share
 * with related processes.  The process structure and the substructures
 * are always addressible except for those marked "(PROC ONLY)" below,
 * which might be addressible only on a processor on which the process
 * is running.
 */
struct	proc {
	struct	proc *p_forw;		/* Doubly-linked run/sleep queue. */
	struct	proc *p_back;
	LIST_ENTRY(proc) p_list;	/* List of all processes. */

	/* substructures: */
	struct	pcred *p_cred;		/* Process owner's identity. */
	struct	filedesc *p_fd;		/* Ptr to open files structure. */
	struct	pstats *p_stats;	/* Accounting/statistics (PROC ONLY). */
	struct	plimit *p_limit;	/* Process limits. */
	struct	vmspace *p_vmspace;	/* Address space. */
	struct	sigacts *p_sigacts;	/* Signal actions, state (PROC ONLY). */

#define	p_ucred		p_cred->pc_ucred
#define	p_rlimit	p_limit->pl_rlimit

	int	p_flag;			/* P_* flags. */
	u_char	p_os;			/* OS tag */
	char	p_stat;			/* S* process status. */
	char	p_pad1[2];

	pid_t	p_pid;			/* Process identifier. */
	LIST_ENTRY(proc) p_hash;	/* Hash chain. */
	LIST_ENTRY(proc) p_pglist;	/* List of processes in pgrp. */
	struct	proc *p_pptr;	 	/* Pointer to parent process. */
	LIST_ENTRY(proc) p_sibling;	/* List of sibling processes. */
	LIST_HEAD(, proc) p_children;	/* Pointer to list of children. */

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_oppid

	pid_t	p_oppid;	 /* Save parent pid during ptrace. XXX */
	int	p_dupfd;	 /* Sideways return value from filedescopen. XXX */

	/* scheduling */
	u_int	p_estcpu;	 /* Time averaged value of p_cpticks. */
	int	p_cpticks;	 /* Ticks of cpu time. */
	fixpt_t	p_pctcpu;	 /* %cpu for this process during p_swtime */
	void	*p_wchan;	 /* Sleep address. */
	char	*p_wmesg;	 /* Reason for sleep. */
	u_int	p_swtime;	 /* Time swapped in or out. */
	u_int	p_slptime;	 /* Time since last blocked. */

	struct	itimerval p_realtimer;	/* Alarm timer. */
	struct	timeval p_rtime;	/* Real time. */
	u_quad_t p_uticks;		/* Statclock hits in user mode. */
	u_quad_t p_sticks;		/* Statclock hits in system mode. */
	u_quad_t p_iticks;		/* Statclock hits processing intr. */

	int	p_traceflag;		/* Kernel trace points. */
	struct	vnode *p_tracep;	/* Trace to vnode. */

	int	p_siglist;		/* Signals arrived but not delivered. */

	struct	vnode *p_textvp;	/* Vnode of executable. */

	int	p_holdcnt;		/* If non-zero, don't swap. */
	struct	emul *p_emul;		/* Emulation information */

	long	p_spare[1];		/* pad to 256, avoid shifting eproc. */


/* End area that is zeroed on creation. */
#define	p_endzero	p_startcopy

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_sigmask

	sigset_t p_sigmask;	/* Current signal mask. */
	sigset_t p_sigignore;	/* Signals being ignored. */
	sigset_t p_sigcatch;	/* Signals being caught by user. */

	u_char	p_priority;	/* Process priority. */
	u_char	p_usrpri;	/* User-priority based on p_cpu and p_nice. */
	char	p_nice;		/* Process "nice" value. */
	char	p_comm[MAXCOMLEN+1];

	struct 	pgrp *p_pgrp;	/* Pointer to process group. */

/* End area that is copied on creation. */
#define	p_endcopy	p_thread

	void	*p_thread;	/* Id for this "thread"; Mach glue. XXX */
	struct	user *p_addr;	/* Kernel virtual addr of u-area (PROC ONLY). */
	struct	mdproc p_md;	/* Any machine-dependent fields. */

	u_short	p_xstat;	/* Exit status for wait; also stop signal. */
	u_short	p_acflag;	/* Accounting flags. */
	struct	rusage *p_ru;	/* Exit information. XXX */
};

#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

/* Status values. */
#define	SIDL	1		/* Process being created by fork. */
#define	SRUN	2		/* Currently runnable. */
#define	SSLEEP	3		/* Sleeping on an address. */
#define	SSTOP	4		/* Process debugging or suspension. */
#define	SZOMB	5		/* Awaiting collection by parent. */

/* These flags are kept in p_flag. */
#define	P_ADVLOCK	0x000001	/* Proc may hold a POSIX adv. lock. */
#define	P_CONTROLT	0x000002	/* Has a controlling terminal. */
#define	P_INMEM		0x000004	/* Loaded into memory. */
#define	P_NOCLDSTOP	0x000008	/* No SIGCHLD when children stop. */
#define	P_PPWAIT	0x000010	/* Parent waits for child exec/exit. */
#define	P_PROFIL	0x000020	/* Has started profiling. */
#define	P_SELECT	0x000040	/* Selecting; wakeup/waiting danger. */
#define	P_SINTR		0x000080	/* Sleep is interruptible. */
#define	P_SUGID		0x000100	/* Had set id privs since last exec. */
#define	P_SYSTEM	0x000200	/* No sigs, stats or swapping. */
#define	P_TIMEOUT	0x000400	/* Timing out during sleep. */
#define	P_TRACED	0x000800	/* Debugged process being traced. */
#define	P_WAITED	0x001000	/* Debugging proc has waited for child. */
#define	P_WEXIT		0x002000	/* Working on exiting. */
#define	P_EXEC		0x004000	/* Process called exec. */

/* Should be moved to machine-dependent areas. */
#define	P_OWEUPC	0x008000	/* Owe proc an addupc() at next ast. */

/* XXX Not sure what to do with these, yet. */
#define	P_FSTRACE	0x010000	/* tracing via fs (elsewhere?) */
#define	P_SSTEP		0x020000	/* proc needs single-step fixup ??? */
#define	P_SUGIDEXEC	0x040000	/* last execve() was set[ug]id */

#define	P_NOCLDWAIT	0x080000	/* Let pid 1 wait for my children */
#define	P_NOZOMBIE	0x100000	/* Pid 1 waits for me instead of dad */

/*
 * MOVE TO ucred.h?
 *
 * Shareable process credentials (always resident).  This includes a reference
 * to the current user credentials as well as real and saved ids that may be
 * used to change ids.
 */
struct	pcred {
	struct	ucred *pc_ucred;	/* Current credentials. */
	uid_t	p_ruid;			/* Real user id. */
	uid_t	p_svuid;		/* Saved effective user id. */
	gid_t	p_rgid;			/* Real group id. */
	gid_t	p_svgid;		/* Saved effective group id. */
	int	p_refcnt;		/* Number of references. */
};

#ifdef _KERNEL
/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 * We set PID_MAX to (SHRT_MAX - 1) so we don't break sys/compat.
 */
#define	PID_MAX		32766
#define	NO_PID		(PID_MAX+1)

#define SESS_LEADER(p)	((p)->p_session->s_leader == (p))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s) {							\
	if (--(s)->s_count == 0)					\
		FREE(s, M_SESSION);					\
}

#if defined(UVM)
#define	PHOLD(p) {							\
	if ((p)->p_holdcnt++ == 0 && ((p)->p_flag & P_INMEM) == 0)	\
		uvm_swapin(p);						\
}
#else
#define	PHOLD(p) {							\
	if ((p)->p_holdcnt++ == 0 && ((p)->p_flag & P_INMEM) == 0)	\
		swapin(p);						\
}
#endif
#define	PRELE(p)	(--(p)->p_holdcnt)

/*
 * Flags to fork1().
 */
#define FORK_FORK	0x00000001
#define FORK_VFORK	0x00000002
#define FORK_RFORK	0x00000004
#define FORK_PPWAIT	0x00000008
#define FORK_SHAREFILES	0x00000010
#define FORK_CLEANFILES	0x00000020
#define FORK_NOZOMBIE	0x00000040
#define FORK_SHAREVM	0x00000080

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, proc) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

extern struct proc *curproc;		/* Current running proc. */
extern struct proc proc0;		/* Process slot for swapper. */
extern int nprocs, maxproc;		/* Current and max number of procs. */
extern int randompid;			/* fork() should create random pid's */

LIST_HEAD(proclist, proc);
extern struct proclist allproc;		/* List of all processes. */
extern struct proclist zombproc;	/* List of zombie processes. */
struct proc *initproc;			/* Process slots for init, pager. */

#define	NQS	32			/* 32 run queues. */
int	whichqs;			/* Bit mask summary of non-empty Q's. */
struct	prochd {
	struct	proc *ph_link;		/* Linked list of running processes. */
	struct	proc *ph_rlink;
} qs[NQS];

struct proc *pfind __P((pid_t));	/* Find process by id. */
struct pgrp *pgfind __P((pid_t));	/* Find process group by id. */

int	chgproccnt __P((uid_t uid, int diff));
int	enterpgrp __P((struct proc *p, pid_t pgid, int mksess));
void	fixjobc __P((struct proc *p, struct pgrp *pgrp, int entering));
int	inferior __P((struct proc *p));
int	leavepgrp __P((struct proc *p));
void	mi_switch __P((void));
void	pgdelete __P((struct pgrp *pgrp));
void	procinit __P((void));
void	remrunqueue __P((struct proc *));
void	resetpriority __P((struct proc *));
void	setrunnable __P((struct proc *));
void	setrunqueue __P((struct proc *));
void	sleep __P((void *chan, int pri));
#if defined(UVM)
void	uvm_swapin __P((struct proc *));  /* XXX: uvm_extern.h? */
#else
void	swapin __P((struct proc *));
#endif
int	tsleep __P((void *chan, int pri, char *wmesg, int timo));
void	unsleep __P((struct proc *));
void	wakeup __P((void *chan));
void	exit1 __P((struct proc *, int));
int	fork1 __P((struct proc *, int, void *, size_t, register_t *));
void	kmeminit __P((void));
void	rqinit __P((void));
int	groupmember __P((gid_t, struct ucred *));
void	cpu_switch __P((struct proc *));
void	cpu_wait __P((struct proc *));
void	cpu_exit __P((struct proc *));
#endif	/* _KERNEL */
#endif	/* !_SYS_PROC_H_ */
