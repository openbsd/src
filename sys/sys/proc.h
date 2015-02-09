/*	$OpenBSD: proc.h,v 1.197 2015/02/09 04:06:13 dlg Exp $	*/
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
#include <machine/atomic.h>

#ifdef _KERNEL
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
};

/*
 * One structure allocated per process group.
 */
struct	pgrp {
	LIST_ENTRY(pgrp) pg_hash;	/* Hash chain. */
	LIST_HEAD(, process) pg_members;/* Pointer to pgrp members. */
	struct	session *pg_session;	/* Pointer to session. */
	pid_t	pg_id;			/* Pgrp id. */
	int	pg_jobc;	/* # procs qualifying pgrp for job control */
};

/*
 * One structure allocated per emulation.
 */
struct exec_package;
struct proc;
struct ps_strings;
struct uvm_object;
union sigval;

struct	emul {
	char	e_name[8];		/* Symbolic name */
	int	*e_errno;		/* Errno array */
					/* Signal sending function */
	void	(*e_sendsig)(void (*)(int), int, int, u_long, int, union sigval);
	int	e_nosys;		/* Offset of the nosys() syscall */
	int	e_nsysent;		/* Number of system call entries */
	struct sysent *e_sysent;	/* System call array */
	char	**e_syscallnames;	/* System call name array */
	int	e_arglen;		/* Extra argument size in words */
					/* Copy arguments on the stack */
	void	*(*e_copyargs)(struct exec_package *, struct ps_strings *,
				    void *, void *);
					/* Set registers before execution */
	void	(*e_setregs)(struct proc *, struct exec_package *,
				  u_long, register_t *);
	int	(*e_fixup)(struct proc *, struct exec_package *);
	int	(*e_coredump)(struct proc *, void *cookie);
	char	*e_sigcode;		/* Start of sigcode */
	char	*e_esigcode;		/* End of sigcode */
	int	e_flags;		/* Flags, see below */
	struct uvm_object *e_sigobject;	/* shared sigcode object */
					/* Per-process hooks */
	void	(*e_proc_exec)(struct proc *, struct exec_package *);
	void	(*e_proc_fork)(struct proc *p, struct proc *parent);
	void	(*e_proc_exit)(struct proc *);
};
/* Flags for e_flags */
#define	EMUL_ENABLED	0x0001		/* Allow exec to continue */
#define	EMUL_NATIVE	0x0002		/* Always enabled */

extern struct emul *emulsw[];		/* All emuls in system */
extern int nemuls;			/* Number of emuls */

/*
 * time usage: accumulated times in ticks
 * One a second, each thread's immediate counts (p_[usi]ticks) are
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
struct process {
	/*
	 * ps_mainproc is the main thread in the process.
	 * Ultimately, we shouldn't need that, threads should be able to exit
	 * at will. Unfortunately until the pid is moved into struct process
	 * we'll have to remember the main threads and abuse its pid as the
	 * the pid of the process. This is gross, but considering the horrible
	 * pid semantics we have right now, it's unavoidable.
	 */
	struct	proc *ps_mainproc;
	struct	ucred *ps_ucred;	/* Process owner's identity. */

	LIST_ENTRY(process) ps_list;	/* List of all processes. */
	TAILQ_HEAD(,proc) ps_threads;	/* Threads in this process. */

	LIST_ENTRY(process) ps_pglist;	/* List of processes in pgrp. */
	struct	process *ps_pptr; 	/* Pointer to parent process. */
	LIST_ENTRY(process) ps_sibling;	/* List of sibling processes. */
	LIST_HEAD(, process) ps_children;/* Pointer to list of children. */

	struct	sigacts *ps_sigacts;	/* Signal actions, state */
	struct	vnode *ps_textvp;	/* Vnode of executable. */
	struct	filedesc *ps_fd;	/* Ptr to open files structure */
	struct	vmspace *ps_vmspace;	/* Address space */

/* The following fields are all zeroed upon creation in process_new. */
#define	ps_startzero	ps_klist
	struct	klist ps_klist;		/* knotes attached to this process */
	int	ps_flags;		/* PS_* flags. */

	struct	proc *ps_single;	/* Single threading to this thread. */
	int	ps_singlecount;		/* Not yet suspended threads. */

	int	ps_traceflag;		/* Kernel trace points. */
	struct	vnode *ps_tracevp;	/* Trace to vnode. */
	struct	ucred *ps_tracecred;	/* Creds for writing trace */

	pid_t	ps_oppid;	 	/* Save parent pid during ptrace. */
	int	ps_ptmask;		/* Ptrace event mask */
	struct	ptrace_state *ps_ptstat;/* Ptrace state */

	struct	rusage *ps_ru;		/* sum of stats for dead threads. */
	struct	tusage ps_tu;		/* accumulated times. */
	struct	rusage ps_cru;		/* sum of stats for reaped children */
	struct	itimerval ps_timer[3];	/* timers, indexed by ITIMER_* */

/* End area that is zeroed on creation. */
#define	ps_endzero	ps_startcopy

/* The following fields are all copied upon creation in process_new. */
#define	ps_startcopy	ps_limit
	struct	plimit *ps_limit;	/* Process limits. */
	struct	pgrp *ps_pgrp;		/* Pointer to process group. */
	struct	emul *ps_emul;		/* Emulation information */
	vaddr_t	ps_strings;		/* User pointers to argv/env */
	vaddr_t	ps_stackgap;		/* User pointer to the "stackgap" */
	vaddr_t	ps_sigcode;		/* User pointer to the signal code */
	u_int	ps_rtableid;		/* Process routing table/domain. */
	char	ps_nice;		/* Process "nice" value. */

	struct uprof {			/* profile arguments */
		caddr_t	pr_base;	/* buffer base */
		size_t  pr_size;	/* buffer size */
		u_long	pr_off;		/* pc offset */
		u_int   pr_scale;	/* pc scaling */
	} ps_prof;

	u_short	ps_acflag;		/* Accounting flags. */

/* End area that is copied on creation. */
#define ps_endcopy	ps_refcnt
	int	ps_refcnt;		/* Number of references. */

	struct	timespec ps_start;	/* starting time. */
	struct	timeout ps_realit_to;	/* real-time itimer trampoline. */
};

#define	ps_pid		ps_mainproc->p_pid
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
#define	PS_WAITED	0x00000400	/* Stopped proc has waited for. */
#define	PS_COREDUMP	0x00000800	/* Busy coredumping */
#define	PS_SINGLEEXIT	0x00001000	/* Other threads must die. */
#define	PS_SINGLEUNWIND	0x00002000	/* Other threads must unwind. */
#define	PS_NOZOMBIE	0x00004000	/* No signal or zombie at exit. */
#define	PS_STOPPED	0x00008000	/* Just stopped, need sig to parent. */
#define	PS_SYSTEM	0x00010000	/* No sigs, stats or swapping. */
#define	PS_EMBRYO	0x00020000	/* New process, not yet fledged */
#define	PS_ZOMBIE	0x00040000	/* Dead and ready to be waited for */
#define	PS_NOBROADCASTKILL 0x00080000	/* Process excluded from kill -1. */

#define	PS_BITS \
    ("\20" "\01CONTROLT" "\02EXEC" "\03INEXEC" "\04EXITING" "\05SUGID" \
     "\06SUGIDEXEC" "\07PPWAIT" "\010ISPWAIT" "\011PROFIL" "\012TRACED" \
     "\013WAITED" "\014COREDUMP" "\015SINGLEEXIT" "\016SINGLEUNWIND" \
     "\017NOZOMBIE" "\020STOPPED" "\021SYSTEM" "\022EMBRYO" "\023ZOMBIE" \
     "\024NOBROADCASTKILL")


struct proc {
	TAILQ_ENTRY(proc) p_runq;
	LIST_ENTRY(proc) p_list;	/* List of all threads. */

	struct	process *p_p;		/* The process of this thread. */
	TAILQ_ENTRY(proc) p_thr_link;/* Threads in a process linkage. */

	/* substructures: */
	struct	filedesc *p_fd;		/* copy of p_p->ps_fd */
	struct	vmspace *p_vmspace;	/* copy of p_p->ps_vmspace */
#define	p_rlimit	p_p->ps_limit->pl_rlimit

	int	p_flag;			/* P_* flags. */
	u_char	p_spare;		/* unused */
	char	p_stat;			/* S* process status. */
	char	p_pad1[1];
	u_char	p_descfd;		/* if not 255, fdesc permits this fd */

	pid_t	p_pid;			/* Process identifier. */
	LIST_ENTRY(proc) p_hash;	/* Hash chain. */

/* The following fields are all zeroed upon creation in fork. */
#define	p_startzero	p_dupfd
	int	p_dupfd;	 /* Sideways return value from filedescopen. XXX */

	long 	p_thrslpid;	/* for thrsleep syscall */

	/* scheduling */
	u_int	p_estcpu;	 /* Time averaged value of p_cpticks. */
	int	p_cpticks;	 /* Ticks of cpu time. */
	const volatile void *p_wchan;/* Sleep address. */
	struct	timeout p_sleep_to;/* timeout for tsleep() */
	const char *p_wmesg;	 /* Reason for sleep. */
	fixpt_t	p_pctcpu;	 /* %cpu for this thread */
	u_int	p_slptime;	 /* Time since last blocked. */
	u_int	p_uticks;		/* Statclock hits in user mode. */
	u_int	p_sticks;		/* Statclock hits in system mode. */
	u_int	p_iticks;		/* Statclock hits processing intr. */
	struct	cpu_info * volatile p_cpu; /* CPU we're running on. */

	struct	rusage p_ru;		/* Statistics */
	struct	tusage p_tu;		/* accumulated times. */
	struct	timespec p_rtime;	/* Real time. */

	void	*p_systrace;		/* Back pointer to systrace */

	void	*p_emuldata;		/* Per-process emulation data, or */
					/* NULL. Malloc type M_EMULDATA */
	int	 p_siglist;		/* Signals arrived but not delivered. */

/* End area that is zeroed on creation. */
#define	p_endzero	p_startcopy

/* The following fields are all copied upon creation in fork. */
#define	p_startcopy	p_sigmask
	sigset_t p_sigmask;	/* Current signal mask. */

	u_char	p_priority;	/* Process priority. */
	u_char	p_usrpri;	/* User-priority based on p_cpu and ps_nice. */
	char	p_comm[MAXCOMLEN+1];

#ifndef	__HAVE_MD_TCB
	void	*p_tcb;		/* user-space thread-control-block address */
# define TCB_SET(p, addr)	((p)->p_tcb = (addr))
# define TCB_GET(p)		((p)->p_tcb)
#endif

	struct	ucred *p_ucred;		/* cached credentials */
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

	u_short	p_xstat;	/* Exit status for wait; also stop signal. */
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

/*
 * These flags are per-thread and kept in p_flag
 */
#define	P_INKTR		0x000001	/* In a ktrace op, don't recurse */
#define	P_PROFPEND	0x000002	/* SIGPROF needs to be posted */
#define	P_ALRMPEND	0x000004	/* SIGVTALRM needs to be posted */
#define	P_SIGSUSPEND	0x000008	/* Need to restore before-suspend mask*/
#define	P_CANTSLEEP	0x000010	/* insomniac thread */
#define	P_SELECT	0x000040	/* Selecting; wakeup/waiting danger. */
#define	P_SINTR		0x000080	/* Sleep is interruptible. */
#define	P_SYSTEM	0x000200	/* No sigs, stats or swapping. */
#define	P_TIMEOUT	0x000400	/* Timing out during sleep. */
#define	P_WEXIT		0x002000	/* Working on exiting. */
#define	P_OWEUPC	0x008000	/* Owe proc an addupc() at next ast. */
#define	P_SUSPSINGLE	0x080000	/* Need to stop for single threading. */
#define P_SYSTRACE	0x400000	/* Process system call tracing active*/
#define P_CONTINUED	0x800000	/* Proc has continued from a stopped state. */
#define	P_THREAD	0x4000000	/* Only a thread, not a real process */
#define	P_SUSPSIG	0x8000000	/* Stopped from signal. */
#define	P_SOFTDEP	0x10000000	/* Stuck processing softdep worklist */
#define P_CPUPEG	0x40000000	/* Do not move to another cpu. */

#define	P_BITS \
    ("\20" "\01INKTR" "\02PROFPEND" "\03ALRMPEND" "\04SIGSUSPEND" \
     "\05CANTSLEEP" "\07SELECT" "\010SINTR" "\012SYSTEM" "\013TIMEOUT" \
     "\016WEXIT" "\020OWEUPC" "\024SUSPSINGLE" "\027SYSTRACE" \
     "\030CONTINUED" "\033THREAD" "\034SUSPSIG" \035SOFTDEP" "\037CPUPEG")

#define	THREAD_PID_OFFSET	1000000

#ifdef _KERNEL

struct uidinfo {
	LIST_ENTRY(uidinfo) ui_hash;
	uid_t   ui_uid;
	long    ui_proccnt;	/* proc structs */
	long	ui_lockcnt;	/* lockf structs */
};

struct uidinfo *uid_find(uid_t);

/*
 * We use process IDs <= PID_MAX; PID_MAX + 1 must also fit in a pid_t,
 * as it is used to represent "no process group".
 * We set PID_MAX to (SHRT_MAX - 1) so we don't break sys/compat.
 */
#define	PID_MAX		32766
#define	NO_PID		(PID_MAX+1)

#define SESS_LEADER(pr)	((pr)->ps_session->s_leader == (pr))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s) do {						\
	if (--(s)->s_count == 0)					\
		pool_put(&session_pool, (s));				\
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
#define FORK_TFORK	0x00000100
#define FORK_SIGHAND	0x00000200
#define FORK_PTRACE	0x00000400
#define FORK_THREAD	0x00000800

#define EXIT_NORMAL		0x00000001
#define EXIT_THREAD		0x00000002
#define EXIT_THREAD_NOCHECK	0x00000003

#define	PIDHASH(pid)	(&pidhashtbl[(pid) & pidhash])
extern LIST_HEAD(pidhashhead, proc) *pidhashtbl;
extern u_long pidhash;

#define	PGRPHASH(pgid)	(&pgrphashtbl[(pgid) & pgrphash])
extern LIST_HEAD(pgrphashhead, pgrp) *pgrphashtbl;
extern u_long pgrphash;

extern struct proc proc0;		/* Process slot for swapper. */
extern struct process process0;		/* Process slot for kernel threads. */
extern int nprocesses, maxprocess;	/* Cur and max number of processes. */
extern int nthreads, maxthread;		/* Cur and max number of threads. */
extern int randompid;			/* fork() should create random pid's */

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

int	ispidtaken(pid_t);
pid_t	allocpid(void);
void	freepid(pid_t);

struct process *prfind(pid_t);	/* Find process by id. */
struct proc *pfind(pid_t);	/* Find thread by id. */
struct pgrp *pgfind(pid_t);	/* Find process group by id. */
void	proc_printit(struct proc *p, const char *modif,
    int (*pr)(const char *, ...));

int	chgproccnt(uid_t uid, int diff);
int	enterpgrp(struct process *, pid_t, struct pgrp *, struct session *);
void	fixjobc(struct process *, struct pgrp *, int);
int	inferior(struct process *, struct process *);
void	leavepgrp(struct process *);
void	preempt(struct proc *);
void	pgdelete(struct pgrp *);
void	procinit(void);
void	resetpriority(struct proc *);
void	setrunnable(struct proc *);
void	endtsleep(void *);
void	unsleep(struct proc *);
void	reaper(void);
void	exit1(struct proc *, int, int);
void	exit2(struct proc *);
int	dowait4(struct proc *, pid_t, int *, int, struct rusage *,
	    register_t *);
void	cpu_exit(struct proc *);
int	fork1(struct proc *, int, void *, pid_t *, void (*)(void *),
	    void *, register_t *, struct proc **);
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
	SINGLE_PTRACE,		/* other threads to stop but don't wait */
	SINGLE_UNWIND,		/* other threads to unwind and stop */
	SINGLE_EXIT		/* other threads to unwind and then exit */
};
int	single_thread_set(struct proc *, enum single_thread_mode, int);
void	single_thread_wait(struct process *);
void	single_thread_clear(struct proc *, int);
int	single_thread_check(struct proc *, int);

void	child_return(void *);

int	proc_cansugid(struct proc *);
void	proc_finish_wait(struct proc *, struct proc *);
void	process_zap(struct process *);
void	proc_free(struct proc *);

struct sleep_state {
	int sls_s;
	int sls_catch;
	int sls_do_sleep;
	int sls_sig;
};

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
struct cpu_info *cpuset_first(struct cpuset *);

#endif	/* _KERNEL */
#endif	/* !_SYS_PROC_H_ */

