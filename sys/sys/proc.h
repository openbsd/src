/*	$OpenBSD: proc.h,v 1.348 2023/08/29 16:19:34 claudio Exp $	*/
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
 *	@(#)proc.h	8.8 (Berkeley) 1/21/94
 */

#ifndef _SYS_PROC_H_
#define	_SYS_PROC_H_

#include <machine/proc.h>		/* Machine-dependent proc substruct. */
#include <sys/selinfo.h>		/* For struct selinfo */
#include <sys/syslimits.h>		/* For LOGIN_NAME_MAX */
#include <sys/queue.h>
#include <sys/timeout.h>		/* For struct timeout */
#include <sys/event.h>			/* For struct klist */
#include <sys/mutex.h>			/* For struct mutex */
#include <sys/resource.h>		/* For struct rusage */
#include <sys/rwlock.h>			/* For struct rwlock */
#include <sys/sigio.h>			/* For struct sigio */

#ifdef _KERNEL
#include <sys/atomic.h>
#define __need_process
#endif

/*
 * One structure allocated per session.
 */
struct process;
struct	session {
	int	s_count;		/* Ref cnt; pgrps in session. */
	struct	process *s_leader;	/* Session leader. */
	struct	vnode *s_ttyvp;		/* Vnode of controlling terminal. */
	struct	tty *s_ttyp;		/* Controlling terminal. */
	char	s_login[LOGIN_NAME_MAX];	/* Setlogin() name. */
	pid_t	s_verauthppid;
	uid_t	s_verauthuid;
	struct timeout s_verauthto;
};

void zapverauth(/* struct session */ void *);

/*
 * One structure allocated per process group.
 */
struct	pgrp {
	LIST_ENTRY(pgrp) pg_hash;	/* Hash chain. */
	LIST_HEAD(, process) pg_members;/* Pointer to pgrp members. */
	struct	session *pg_session;	/* Pointer to session. */
	struct	sigiolst pg_sigiolst;	/* List of sigio structures. */
	pid_t	pg_id;			/* Pgrp id. */
	int	pg_jobc;	/* # procs qualifying pgrp for job control */
};

/*
 * time usage: accumulated times in ticks
 * Once a second, each thread's immediate counts (p_[usi]ticks) are
 * accumulated into these.
 */
struct tusage {
	struct	timespec tu_runtime;	/* Realtime. */
	uint64_t	tu_uticks;	/* Statclock hits in user mode. */
	uint64_t	tu_sticks;	/* Statclock hits in system mode. */
	uint64_t	tu_iticks;	/* Statclock hits processing intr. */
};

/*
 * Description of a process.
 *
 * These structures contain the information needed to manage a thread of
 * control, known in UN*X as a process; it has references to substructures
 * containing descriptions of things that the process uses, but may share
 * with related processes.
 *
 * struct process is the higher level process containing information
 * shared by all threads in a process, while struct proc contains the
 * run-time information needed by threads.
 */
#ifdef __need_process
struct futex;
LIST_HEAD(futex_list, futex);
struct proc;
struct tslpentry;
TAILQ_HEAD(tslpqueue, tslpentry);
struct unveil;

/*
 * Locks used to protect struct members in this file:
 *	I	immutable after creation
 *	a	atomic operations
 *	K	kernel lock
 *	m	this process' `ps_mtx'
 *	p	this process' `ps_lock'
 *	R	rlimit_lock
 *	S	scheduler lock
 *	T	itimer_mtx
 */
struct process {
	/*
	 * ps_mainproc is the original thread in the process.
	 * It's only still special for the handling of
	 * some signal and ptrace behaviors that need to be fixed.
	 */
	struct	proc *ps_mainproc;
	struct	ucred *ps_ucred;	/* Process owner's identity. */

	LIST_ENTRY(process) ps_list;	/* List of all processes. */
	TAILQ_HEAD(,proc) ps_threads;	/* [K|S] Threads in this process. */

	LIST_ENTRY(process) ps_pglist;	/* List of processes in pgrp. */
	struct	process *ps_pptr; 	/* Pointer to parent process. */
	LIST_ENTRY(process) ps_sibling;	/* List of sibling processes. */
	LIST_HEAD(, process) ps_children;/* Pointer to list of children. */
	LIST_ENTRY(process) ps_hash;    /* Hash chain. */

	/*
	 * An orphan is the child that has been re-parented to the
	 * debugger as a result of attaching to it.  Need to keep
	 * track of them for parent to be able to collect the exit
	 * status of what used to be children.
	 */
	LIST_ENTRY(process) ps_orphan;	/* List of orphan processes. */
	LIST_HEAD(, process) ps_orphans;/* Pointer to list of orphans. */

	struct	sigiolst ps_sigiolst;	/* List of sigio structures. */
	struct	sigacts *ps_sigacts;	/* [I] Signal actions, state */
	struct	vnode *ps_textvp;	/* Vnode of executable. */
	struct	filedesc *ps_fd;	/* Ptr to open files structure */
	struct	vmspace *ps_vmspace;	/* Address space */
	pid_t	ps_pid;			/* Process identifier. */

	struct	futex_list ps_ftlist;	/* futexes attached to this process */
	struct	tslpqueue ps_tslpqueue;	/* [p] queue of threads in thrsleep */
	struct	rwlock	ps_lock;	/* per-process rwlock */
	struct  mutex	ps_mtx;		/* per-process mutex */

/* The following fields are all zeroed upon creation in process_new. */
#define	ps_startzero	ps_klist
	struct	klist ps_klist;		/* knotes attached to this process */
	u_int	ps_flags;		/* [a] PS_* flags. */
	int	ps_siglist;		/* Signals pending for the process. */

	struct	proc *ps_single;	/* [S] Thread for single-threading. */
	u_int	ps_singlecount;		/* [a] Not yet suspended threads. */

	int	ps_traceflag;		/* Kernel trace points. */
	struct	vnode *ps_tracevp;	/* Trace to vnode. */
	struct	ucred *ps_tracecred;	/* Creds for writing trace */

	u_int	ps_xexit;		/* Exit status for wait */
	int	ps_xsig;		/* Stopping or killing signal */

	pid_t	ps_ppid;		/* [a] Cached parent pid */
	pid_t	ps_oppid;	 	/* [a] Save parent pid during ptrace. */
	int	ps_ptmask;		/* Ptrace event mask */
	struct	ptrace_state *ps_ptstat;/* Ptrace state */

	struct	rusage *ps_ru;		/* sum of stats for dead threads. */
	struct	tusage ps_tu;		/* accumulated times. */
	struct	rusage ps_cru;		/* sum of stats for reaped children */
	struct	itimerspec ps_timer[3];	/* [m] ITIMER_REAL timer */
					/* [T] ITIMER_{VIRTUAL,PROF} timers */
	struct	timeout ps_rucheck_to;	/* [] resource limit check timer */
	time_t	ps_nextxcpu;		/* when to send next SIGXCPU, */
					/* in seconds of process runtime */

	u_int64_t ps_wxcounter;

	struct unveil *ps_uvpaths;	/* unveil vnodes and names */
	ssize_t	ps_uvvcount;		/* count of unveil vnodes held */
	size_t	ps_uvncount;		/* count of unveil names allocated */
	int	ps_uvdone;		/* no more unveil is permitted */

/* End area that is zeroed on creation. */
#define	ps_endzero	ps_startcopy

/* The following fields are all copied upon creation in process_new. */
#define	ps_startcopy	ps_limit
	struct	plimit *ps_limit;	/* [m,R] Process limits. */
	struct	pgrp *ps_pgrp;		/* Pointer to process group. */

	char	ps_comm[_MAXCOMLEN];	/* command name, incl NUL */

	vaddr_t	ps_strings;		/* User pointers to argv/env */
	vaddr_t	ps_auxinfo;		/* User pointer to auxinfo */
	vaddr_t ps_timekeep; 		/* User pointer to timekeep */
	vaddr_t	ps_sigcode;		/* [I] User pointer to signal code */
	vaddr_t ps_sigcoderet;		/* [I] User ptr to sigreturn retPC */
	u_long	ps_sigcookie;		/* [I] */
	u_int	ps_rtableid;		/* [a] Process routing table/domain. */
	char	ps_nice;		/* Process "nice" value. */

	struct uprof {			/* profile arguments */
		caddr_t	pr_base;	/* buffer base */
		size_t  pr_size;	/* buffer size */
		u_long	pr_off;		/* pc offset */
		u_int   pr_scale;	/* pc scaling */
	} ps_prof;

	u_int32_t	ps_acflag;	/* Accounting flags. */

	uint64_t ps_pledge;		/* [m] pledge promises */
	uint64_t ps_execpledge;		/* [m] execpledge promises */

	int64_t ps_kbind_cookie;	/* [m] */
	u_long  ps_kbind_addr;		/* [m] */
/* an address that can't be in userspace or kernelspace */
#define	BOGO_PC	(u_long)-1

/* End area that is copied on creation. */
#define ps_endcopy	ps_threadcnt
	u_int	ps_threadcnt;		/* Number of threads. */

	struct	timespec ps_start;	/* starting uptime. */
	struct	timeout ps_realit_to;	/* [m] ITIMER_REAL timeout */
};

#define	ps_session	ps_pgrp->pg_session
#define	ps_pgid		ps_pgrp->pg_id

#endif /* __need_process */

/*
 * These flags are kept in ps_flags.
 */
#define	PS_CONTROLT	0x00000001	/* Has a controlling terminal. */
#define	PS_EXEC		0x00000002	/* Process called exec. */
#define	PS_INEXEC	0x00000004	/* Process is doing an exec right now */
#define	PS_EXITING	0x00000008	/* Process is exiting. */
#define	PS_SUGID	0x00000010	/* Had set id privs since last exec. */
#define	PS_SUGIDEXEC	0x00000020	/* last execve() was set[ug]id */
#define	PS_PPWAIT	0x00000040	/* Parent waits for exec/exit. */
#define	PS_ISPWAIT	0x00000080	/* Is parent of PPWAIT child. */
#define	PS_PROFIL	0x00000100	/* Has started profiling. */
#define	PS_TRACED	0x00000200	/* Being ptraced. */
#define	PS_WAITED	0x00000400	/* Stopped proc was waited for. */
#define	PS_COREDUMP	0x00000800	/* Busy coredumping */
#define	PS_SINGLEEXIT	0x00001000	/* Other threads must die. */
#define	PS_SINGLEUNWIND	0x00002000	/* Other threads must unwind. */
#define	PS_NOZOMBIE	0x00004000	/* No signal or zombie at exit. */
#define	PS_STOPPED	0x00008000	/* Just stopped, need sig to parent. */
#define	PS_SYSTEM	0x00010000	/* No sigs, stats or swapping. */
#define	PS_EMBRYO	0x00020000	/* New process, not yet fledged */
#define	PS_ZOMBIE	0x00040000	/* Dead and ready to be waited for */
#define	PS_NOBROADCASTKILL 0x00080000	/* Process excluded from kill -1. */
#define	PS_PLEDGE	0x00100000	/* Has called pledge(2) */
#define	PS_WXNEEDED	0x00200000	/* Process allowed to violate W^X */
#define	PS_EXECPLEDGE	0x00400000	/* Has exec pledges */
#define	PS_ORPHAN	0x00800000	/* Process is on an orphan list */
#define	PS_CHROOT	0x01000000	/* Process is chrooted */
#define	PS_NOBTCFI	0x02000000	/* No Branch Target CFI */
#define	PS_ITIMER	0x04000000	/* Virtual interval timers running */

#define	PS_BITS \
    ("\20" "\01CONTROLT" "\02EXEC" "\03INEXEC" "\04EXITING" "\05SUGID" \
     "\06SUGIDEXEC" "\07PPWAIT" "\010ISPWAIT" "\011PROFIL" "\012TRACED" \
     "\013WAITED" "\014COREDUMP" "\015SINGLEEXIT" "\016SINGLEUNWIND" \
     "\017NOZOMBIE" "\020STOPPED" "\021SYSTEM" "\022EMBRYO" "\023ZOMBIE" \
     "\024NOBROADCASTKILL" "\025PLEDGE" "\026WXNEEDED" "\027EXECPLEDGE" \
     "\030ORPHAN" "\031CHROOT" "\032NOBTCFI" "\033ITIMER")


struct kcov_dev;
struct lock_list_entry;
struct kqueue;

struct p_inentry {
	u_long	 ie_serial;
	vaddr_t	 ie_start;
	vaddr_t	 ie_end;
};

/*
 *  Locks used to protect struct members in this file:
 *	I	immutable after creation
 *	S	scheduler lock
 *	U	uidinfolk
 *	l	read only reference, see lim_read_enter()
 *	o	owned (read/modified only) by this thread
 */
struct proc {
	TAILQ_ENTRY(proc) p_runq;	/* [S] current run/sleep queue */
	LIST_ENTRY(proc) p_list;	/* List of all threads. */

	struct	process *p_p;		/* [I] The process of this thread. */
	TAILQ_ENTRY(proc) p_thr_link;	/* Threads in a process linkage. */

	TAILQ_ENTRY(proc) p_fut_link;	/* Threads in a futex linkage. */
	struct	futex	*p_futex;	/* Current sleeping futex. */

	/* substructures: */
	struct	filedesc *p_fd;		/* copy of p_p->ps_fd */
	struct	vmspace *p_vmspace;	/* [I] copy of p_p->ps_vmspace */
	struct	p_inentry p_spinentry;	/* [o] cache for SP check */
	struct	p_inentry p_pcinentry;	/* [o] cache for PC check */

	int	p_flag;			/* P_* flags. */
	u_char	p_spare;		/* unused */
	char	p_stat;			/* [S] S* process status. */
	u_char	p_runpri;		/* [S] Runqueue priority */
	u_char	p_descfd;		/* if not 255, fdesc permits this fd */

	pid_t	p_tid;			/* Thread identifier. */
	LIST_ENTRY(proc) p_hash;	/* Hash chain. */

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_dupfd
	int	p_dupfd;	 /* Sideways return value from filedescopen. XXX */

	/* scheduling */
	int	p_cpticks;	 /* Ticks of cpu time. */
	const volatile void *p_wchan;	/* [S] Sleep address. */
	struct	timeout p_sleep_to;/* timeout for tsleep() */
	const char *p_wmesg;		/* [S] Reason for sleep. */
	fixpt_t	p_pctcpu;		/* [S] %cpu for this thread */
	u_int	p_slptime;		/* [S] Time since last blocked. */
	u_int	p_uticks;		/* Statclock hits in user mode. */
	u_int	p_sticks;		/* Statclock hits in system mode. */
	u_int	p_iticks;		/* Statclock hits processing intr. */
	struct	cpu_info * volatile p_cpu; /* [S] CPU we're running on. */

	struct	rusage p_ru;		/* Statistics */
	struct	tusage p_tu;		/* accumulated times. */

	struct	plimit	*p_limit;	/* [l] read ref. of p_p->ps_limit */
	struct	kcov_dev *p_kd;		/* kcov device handle */
	struct	lock_list_entry *p_sleeplocks;	/* WITNESS lock tracking */ 
	struct	kqueue *p_kq;		/* [o] select/poll queue of evts */
	unsigned long p_kq_serial;	/* [o] to check against enqueued evts */

	int	 p_siglist;		/* [a] Signals arrived & not delivered*/

/* End area that is zeroed on creation. */
#define	p_endzero	p_startcopy

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_sigmask
	sigset_t p_sigmask;		/* [a] Current signal mask */

	char	p_name[_MAXCOMLEN];	/* thread name, incl NUL */
	u_char	p_slppri;		/* [S] Sleeping priority */
	u_char	p_usrpri;	/* [S] Priority based on p_estcpu & ps_nice */
	u_int	p_estcpu;		/* [S] Time averaged val of p_cpticks */
	int	p_pledge_syscall;	/* Cache of current syscall */

	struct	ucred *p_ucred;		/* [o] cached credentials */
	struct	sigaltstack p_sigstk;	/* sp & on stack state variable */

	u_long	p_prof_addr;	/* tmp storage for profiling addr until AST */
	u_long	p_prof_ticks;	/* tmp storage for profiling ticks until AST */

/* End area that is copied on creation. */
#define	p_endcopy	p_addr
	struct	user *p_addr;	/* Kernel virtual addr of u-area */
	struct	mdproc p_md;	/* Any machine-dependent fields. */

	sigset_t p_oldmask;	/* Saved mask from before sigpause */
	int	p_sisig;	/* For core dump/debugger XXX */
	union sigval p_sigval;	/* For core dump/debugger XXX */
	long	p_sitrapno;	/* For core dump/debugger XXX */
	int	p_sicode;	/* For core dump/debugger XXX */
};

/* Status values. */
#define	SIDL	1		/* Thread being created by fork. */
#define	SRUN	2		/* Currently runnable. */
#define	SSLEEP	3		/* Sleeping on an address. */
#define	SSTOP	4		/* Debugging or suspension. */
#define	SZOMB	5		/* unused */
#define	SDEAD	6		/* Thread is almost gone */
#define	SONPROC	7		/* Thread is currently on a CPU. */

#define	P_ZOMBIE(p)	((p)->p_stat == SDEAD)
#define	P_HASSIBLING(p)	((p)->p_p->ps_threadcnt > 1)

/*
 * These flags are per-thread and kept in p_flag
 */
#define	P_INKTR		0x00000001	/* In a ktrace op, don't recurse */
#define	P_PROFPEND	0x00000002	/* SIGPROF needs to be posted */
#define	P_ALRMPEND	0x00000004	/* SIGVTALRM needs to be posted */
#define	P_SIGSUSPEND	0x00000008	/* Need to restore before-suspend mask*/
#define	P_CANTSLEEP	0x00000010	/* insomniac thread */
#define	P_WSLEEP	0x00000020	/* Working on going to sleep. */
#define	P_SINTR		0x00000080	/* Sleep is interruptible. */
#define	P_SYSTEM	0x00000200	/* No sigs, stats or swapping. */
#define	P_TIMEOUT	0x00000400	/* Timing out during sleep. */
#define	P_WEXIT		0x00002000	/* Working on exiting. */
#define	P_OWEUPC	0x00008000	/* Owe proc an addupc() at next ast. */
#define	P_SUSPSINGLE	0x00080000	/* Need to stop for single threading. */
#define P_CONTINUED	0x00800000	/* Proc has continued from a stopped state. */
#define	P_THREAD	0x04000000	/* Only a thread, not a real process */
#define	P_SUSPSIG	0x08000000	/* Stopped from signal. */
#define	P_SOFTDEP	0x10000000	/* Stuck processing softdep worklist */
#define P_CPUPEG	0x40000000	/* Do not move to another cpu. */

#define	P_BITS \
    ("\20" "\01INKTR" "\02PROFPEND" "\03ALRMPEND" "\04SIGSUSPEND" \
     "\05CANTSLEEP" "\06WSLEEP" "\010SINTR" "\012SYSTEM" "\013TIMEOUT" \
     "\016WEXIT" "\020OWEUPC" "\024SUSPSINGLE" "\027XX" \
     "\030CONTINUED" "\033THREAD" "\034SUSPSIG" "\035SOFTDEP" "\037CPUPEG")

#define	THREAD_PID_OFFSET	100000

#ifdef _KERNEL

struct uidinfo {
	LIST_ENTRY(uidinfo) ui_hash;	/* [U] */
	uid_t   ui_uid;			/* [I] */
	long    ui_proccnt;		/* [U] proc structs */
	long	ui_lockcnt;		/* [U] lockf structs */
};

struct uidinfo *uid_find(uid_t);
void uid_release(struct uidinfo *);

/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 * We set PID_MAX to 99999 to keep it in 5 columns in ps
 * When exposed to userspace, thread IDs have THREAD_PID_OFFSET
 * added to keep them from overlapping the PID range.  For them,
 * we use a * a (0 .. 2^n] range for cheapness, picking 'n' such
 * that 2^n + THREAD_PID_OFFSET and THREAD_PID_OFFSET have
 * the same number of columns when printed.
 */
#define	PID_MAX			99999
#define	TID_MASK		0x7ffff

#define	NO_PID		(PID_MAX+1)

#define SESS_LEADER(pr)	((pr)->ps_session->s_leader == (pr))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s) do {						\
	if (--(s)->s_count == 0) {					\
		timeout_del(&(s)->s_verauthto);			\
		pool_put(&session_pool, (s));				\
	}								\
} while (/* CONSTCOND */ 0)

/*
 * Flags to fork1().
 */
#define FORK_FORK	0x00000001
#define FORK_VFORK	0x00000002
#define FORK_IDLE	0x00000004
#define FORK_PPWAIT	0x00000008
#define FORK_SHAREFILES	0x00000010
#define FORK_SYSTEM	0x00000020
#define FORK_NOZOMBIE	0x00000040
#define FORK_SHAREVM	0x00000080
#define FORK_PTRACE	0x00000400

#define EXIT_NORMAL		0x00000001
#define EXIT_THREAD		0x00000002
#define EXIT_THREAD_NOCHECK	0x00000003

#define	TIDHASH(tid)	(&tidhashtbl[(tid) & tidhash])
extern LIST_HEAD(tidhashhead, proc) *tidhashtbl;
extern u_long tidhash;

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, process) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

extern struct proc proc0;		/* Process slot for swapper. */
extern struct process process0;		/* Process slot for kernel threads. */
extern int nprocesses, maxprocess;	/* Cur and max number of processes. */
extern int nthreads, maxthread;		/* Cur and max number of threads. */

LIST_HEAD(proclist, proc);
LIST_HEAD(processlist, process);
extern struct processlist allprocess;	/* List of all processes. */
extern struct processlist zombprocess;	/* List of zombie processes. */
extern struct proclist allproc;		/* List of all threads. */

extern struct process *initprocess;	/* Process slot for init. */
extern struct proc *reaperproc;		/* Thread slot for reaper. */
extern struct proc *syncerproc;		/* filesystem syncer daemon */

extern struct pool process_pool;	/* memory pool for processes */
extern struct pool proc_pool;		/* memory pool for procs */
extern struct pool rusage_pool;		/* memory pool for zombies */
extern struct pool ucred_pool;		/* memory pool for ucreds */
extern struct pool session_pool;	/* memory pool for sessions */
extern struct pool pgrp_pool;		/* memory pool for pgrps */

void	freepid(pid_t);

struct process *prfind(pid_t);	/* Find process by id. */
struct process *zombiefind(pid_t); /* Find zombie process by id. */
struct proc *tfind(pid_t);	/* Find thread by id. */
struct pgrp *pgfind(pid_t);	/* Find process group by id. */
struct proc *tfind_user(pid_t, struct process *);
				/* Find thread by userspace id. */
void	proc_printit(struct proc *p, const char *modif,
    int (*pr)(const char *, ...));

int	chgproccnt(uid_t uid, int diff);
void	enternewpgrp(struct process *, struct pgrp *, struct session *);
void	enterthispgrp(struct process *, struct pgrp *);
int	inferior(struct process *, struct process *);
void	leavepgrp(struct process *);
void	killjobc(struct process *);
void	preempt(void);
void	procinit(void);
void	setpriority(struct proc *, uint32_t, uint8_t);
void	setrunnable(struct proc *);
void	endtsleep(void *);
int	wakeup_proc(struct proc *, const volatile void *, int);
void	unsleep(struct proc *);
void	reaper(void *);
__dead void exit1(struct proc *, int, int, int);
void	exit2(struct proc *);
void	cpu_fork(struct proc *_curp, struct proc *_child, void *_stack,
	    void *_tcb, void (*_func)(void *), void *_arg);
void	cpu_exit(struct proc *);
void	process_initialize(struct process *, struct proc *);
int	fork1(struct proc *_curp, int _flags, void (*_func)(void *),
	    void *_arg, register_t *_retval, struct proc **_newprocp);
int	thread_fork(struct proc *_curp, void *_stack, void *_tcb,
	    pid_t *_tidptr, register_t *_retval);
int	groupmember(gid_t, struct ucred *);
void	dorefreshcreds(struct process *, struct proc *);
void	dosigsuspend(struct proc *, sigset_t);

static inline void
refreshcreds(struct proc *p)
{
	struct process *pr = p->p_p;

	/* this is an unlocked access to ps_ucred, but the result is benign */
	if (pr->ps_ucred != p->p_ucred)
		dorefreshcreds(pr, p);
}

enum single_thread_mode {
	SINGLE_SUSPEND,		/* other threads to stop wherever they are */
	SINGLE_UNWIND,		/* other threads to unwind and stop */
	SINGLE_EXIT		/* other threads to unwind and then exit */
};
int	single_thread_set(struct proc *, enum single_thread_mode, int);
int	single_thread_wait(struct process *, int);
void	single_thread_clear(struct proc *, int);
int	single_thread_check(struct proc *, int);

void	child_return(void *);

int	proc_cansugid(struct proc *);

struct cond {
	unsigned int	c_wait;		/* [a] initialized and waiting */
};

#define COND_INITIALIZER()		{ .c_wait = 1 }

#if defined(MULTIPROCESSOR)
void	proc_trampoline_mp(void);	/* XXX */
#endif

/*
 * functions to handle sets of cpus.
 *
 * For now we keep the cpus in ints so that we can use the generic
 * atomic ops.
 */
#define CPUSET_ASIZE(x) (((x) - 1)/32 + 1)
#define CPUSET_SSIZE CPUSET_ASIZE(MAXCPUS)
struct cpuset {
	int cs_set[CPUSET_SSIZE];
};

void cpuset_init_cpu(struct cpu_info *);

void cpuset_clear(struct cpuset *);
void cpuset_add(struct cpuset *, struct cpu_info *);
void cpuset_del(struct cpuset *, struct cpu_info *);
int cpuset_isset(struct cpuset *, struct cpu_info *);
void cpuset_add_all(struct cpuset *);
void cpuset_copy(struct cpuset *, struct cpuset *);
void cpuset_union(struct cpuset *, struct cpuset *, struct cpuset *);
void cpuset_intersection(struct cpuset *t, struct cpuset *, struct cpuset *);
void cpuset_complement(struct cpuset *, struct cpuset *, struct cpuset *);
int cpuset_cardinality(struct cpuset *);
struct cpu_info *cpuset_first(struct cpuset *);

#endif	/* _KERNEL */
#endif	/* !_SYS_PROC_H_ */

