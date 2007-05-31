/*	$OpenBSD: init_main.c,v 1.141 2007/05/31 18:16:59 dlg Exp $	*/
/*	$NetBSD: init_main.c,v 1.84.4.1 1996/06/02 09:08:06 mrg Exp $	*/

/*
 * Copyright (c) 1995 Christopher G. Demetriou.  All rights reserved.
 * Copyright (c) 1982, 1986, 1989, 1991, 1992, 1993
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
 *	@(#)init_main.c	8.9 (Berkeley) 1/21/94
 */

#include <sys/param.h>
#include <sys/filedesc.h>
#include <sys/file.h>
#include <sys/errno.h>
#include <sys/exec.h>
#include <sys/kernel.h>
#include <sys/kthread.h>
#include <sys/mount.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/socketvar.h>
#include <sys/lockf.h>
#include <sys/protosw.h>
#include <sys/reboot.h>
#include <sys/user.h>
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#include <sys/domain.h>
#include <sys/mbuf.h>
#include <sys/pipe.h>
#include <sys/workq.h>

#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include <dev/rndvar.h>

#include <ufs/ufs/quota.h>

#include <machine/cpu.h>

#include <uvm/uvm.h>

#include <net/if.h>
#include <net/raw_cb.h>

#if defined(CRYPTO)
#include <crypto/cryptodev.h>
#include <crypto/cryptosoft.h>
#endif

#if defined(NFSSERVER) || defined(NFSCLIENT)
extern void nfs_init(void);
#endif

#include "softraid.h"

const char	copyright[] =
"Copyright (c) 1982, 1986, 1989, 1991, 1993\n"
"\tThe Regents of the University of California.  All rights reserved.\n"
"Copyright (c) 1995-2007 OpenBSD. All rights reserved.  http://www.OpenBSD.org\n";

/* Components of the first process -- never freed. */
struct	session session0;
struct	pgrp pgrp0;
struct	proc proc0;
struct	process process0;
struct	pcred cred0;
struct	plimit limit0;
struct	vmspace vmspace0;
struct	sigacts sigacts0;
struct	proc *initproc;

int	cmask = CMASK;
extern	struct user *proc0paddr;

void	(*md_diskconf)(void) = NULL;
struct	vnode *rootvp, *swapdev_vp;
int	boothowto;
struct	timeval boottime;
int	ncpus =  1;
__volatile int start_init_exec;		/* semaphore for start_init() */

#if !defined(NO_PROPOLICE)
long	__guard[8];
#endif

/* XXX return int so gcc -Werror won't complain */
int	main(void *);
void	check_console(struct proc *);
void	start_init(void *);
void	start_cleaner(void *);
void	start_update(void *);
void	start_reaper(void *);
void	start_crypto(void *);
void	init_exec(void);
void	kqueue_init(void);
void	workq_init(void);

extern char sigcode[], esigcode[];
#ifdef SYSCALL_DEBUG
extern char *syscallnames[];
#endif

struct emul emul_native = {
	"native",
	NULL,
	sendsig,
	SYS_syscall,
	SYS_MAXSYSCALL,
	sysent,
#ifdef SYSCALL_DEBUG
	syscallnames,
#else
	NULL,
#endif
	0,
	copyargs,
	setregs,
	NULL,
	sigcode,
	esigcode,
	EMUL_ENABLED | EMUL_NATIVE,
};


/*
 * System startup; initialize the world, create process 0, mount root
 * filesystem, and fork to create init and pagedaemon.  Most of the
 * hard work is done in the lower-level initialization routines including
 * startup(), which does memory initialization and autoconfiguration.
 */
/* XXX return int, so gcc -Werror won't complain */
int
main(void *framep)
{
	struct proc *p;
	struct pdevinit *pdev;
	struct timeval rtv;
	quad_t lim;
	int s, i;
	extern struct pdevinit pdevinit[];
	extern void scheduler_start(void);
	extern void disk_init(void);
	extern void endtsleep(void *);
	extern void realitexpire(void *);

	/*
	 * Initialize the current process pointer (curproc) before
	 * any possible traps/probes to simplify trap processing.
	 */
	curproc = p = &proc0;
	p->p_cpu = curcpu();

	/*
	 * Initialize timeouts.
	 */
	timeout_startup();

	/*
	 * Attempt to find console and initialize
	 * in case of early panic or other messages.
	 */
	config_init();		/* init autoconfiguration data structures */
	consinit();

	printf("%s\n", copyright);

	KERNEL_LOCK_INIT();

	uvm_init();
	disk_init();		/* must come before autoconfiguration */
	tty_init();		/* initialise tty's */
	cpu_startup();

	/*
	 * Initialize mbuf's.  Do this now because we might attempt to
	 * allocate mbufs or mbuf clusters during autoconfiguration.
	 */
	mbinit();

	/* Initialize sockets. */
	soinit();

	/*
	 * Initialize process and pgrp structures.
	 */
	procinit();

	/* Initialize file locking. */
	lf_init();

	/*
	 * Initialize filedescriptors.
	 */
	filedesc_init();

	/*
	 * Initialize pipes.
	 */
	pipe_init();

	/*
	 * Initialize kqueues.
	 */
	kqueue_init();

	/*
	 * Create process 0 (the swapper).
	 */

	process0.ps_mainproc = p;
	TAILQ_INIT(&process0.ps_threads);
	TAILQ_INSERT_TAIL(&process0.ps_threads, p, p_thr_link);
	p->p_p = &process0;

	LIST_INSERT_HEAD(&allproc, p, p_list);
	p->p_pgrp = &pgrp0;
	LIST_INSERT_HEAD(PIDHASH(0), p, p_hash);
	LIST_INSERT_HEAD(PGRPHASH(0), &pgrp0, pg_hash);
	LIST_INIT(&pgrp0.pg_members);
	LIST_INSERT_HEAD(&pgrp0.pg_members, p, p_pglist);

	pgrp0.pg_session = &session0;
	session0.s_count = 1;
	session0.s_leader = p;

	atomic_setbits_int(&p->p_flag, P_SYSTEM | P_NOCLDWAIT);
	p->p_stat = SONPROC;
	p->p_nice = NZERO;
	p->p_emul = &emul_native;
	bcopy("swapper", p->p_comm, sizeof ("swapper"));

	/* Init timeouts. */
	timeout_set(&p->p_sleep_to, endtsleep, p);
	timeout_set(&p->p_realit_to, realitexpire, p);

	/* Create credentials. */
	cred0.p_refcnt = 1;
	p->p_cred = &cred0;
	p->p_ucred = crget();
	p->p_ucred->cr_ngroups = 1;	/* group 0 */

	/* Initialize signal state for process 0. */
	signal_init();
	p->p_sigacts = &sigacts0;
	siginit(p);

	/* Create the file descriptor table. */
	p->p_fd = fdinit(NULL);

	TAILQ_INIT(&p->p_selects);

	/* Create the limits structures. */
	p->p_p->ps_limit = &limit0;
	for (i = 0; i < sizeof(p->p_rlimit)/sizeof(p->p_rlimit[0]); i++)
		limit0.pl_rlimit[i].rlim_cur =
		    limit0.pl_rlimit[i].rlim_max = RLIM_INFINITY;
	limit0.pl_rlimit[RLIMIT_NOFILE].rlim_cur = NOFILE;
	limit0.pl_rlimit[RLIMIT_NOFILE].rlim_max = MIN(NOFILE_MAX,
	    (maxfiles - NOFILE > NOFILE) ?  maxfiles - NOFILE : NOFILE);
	limit0.pl_rlimit[RLIMIT_NPROC].rlim_cur = MAXUPRC;
	lim = ptoa(uvmexp.free);
	limit0.pl_rlimit[RLIMIT_RSS].rlim_max = lim;
	limit0.pl_rlimit[RLIMIT_MEMLOCK].rlim_max = lim;
	limit0.pl_rlimit[RLIMIT_MEMLOCK].rlim_cur = lim / 3;
	limit0.p_refcnt = 1;

	/* Allocate a prototype map so we have something to fork. */
	uvmspace_init(&vmspace0, pmap_kernel(), round_page(VM_MIN_ADDRESS),
	    trunc_page(VM_MAX_ADDRESS), TRUE);
	p->p_vmspace = &vmspace0;

	p->p_addr = proc0paddr;				/* XXX */

	/*
	 * We continue to place resource usage info in the
	 * user struct so they're pageable.
	 */
	p->p_stats = &p->p_addr->u_stats;

	/*
	 * Charge root for one process.
	 */
	(void)chgproccnt(0, 1);

	/* Initialize run queues */
	rqinit();

	/* Initialize work queues */
	workq_init();

	/* Configure the devices */
	cpu_configure();

	/* Configure virtual memory system, set vm rlimits. */
	uvm_init_limits(p);

	/* Initialize the file systems. */
#if defined(NFSSERVER) || defined(NFSCLIENT)
	nfs_init();			/* initialize server/shared data */
#endif
	vfsinit();

	/* Start real time and statistics clocks. */
	initclocks();

	/* Lock the kernel on behalf of proc0. */
	KERNEL_PROC_LOCK(p);

#ifdef SYSVSHM
	/* Initialize System V style shared memory. */
	shminit();
#endif

#ifdef SYSVSEM
	/* Initialize System V style semaphores. */
	seminit();
#endif

#ifdef SYSVMSG
	/* Initialize System V style message queues. */
	msginit();
#endif

	/* Attach pseudo-devices. */
	randomattach();
	for (pdev = pdevinit; pdev->pdev_attach != NULL; pdev++)
		if (pdev->pdev_count > 0)
			(*pdev->pdev_attach)(pdev->pdev_count);

#ifdef CRYPTO
	swcr_init();
#endif /* CRYPTO */
	
	/*
	 * Initialize protocols.  Block reception of incoming packets
	 * until everything is ready.
	 */
	s = splnet();
	ifinit();
	domaininit();
	if_attachdomain();
	splx(s);

#ifdef GPROF
	/* Initialize kernel profiling. */
	kmstartup();
#endif

#if !defined(NO_PROPOLICE)
	{
		volatile long newguard[8];
		int i;

		arc4random_bytes((long *)newguard, sizeof(newguard));

		for (i = sizeof(__guard)/sizeof(__guard[0]) - 1; i; i--)
			__guard[i] = newguard[i];
	}
#endif

	/* init exec and emul */
	init_exec();

	/* Start the scheduler */
	scheduler_start();

	/*
	 * Create process 1 (init(8)).  We do this now, as Unix has
	 * historically had init be process 1, and changing this would
	 * probably upset a lot of people.
	 *
	 * Note that process 1 won't immediately exec init(8), but will
	 * wait for us to inform it that the root file system has been
	 * mounted.
	 */
	if (fork1(p, SIGCHLD, FORK_FORK, NULL, 0, start_init, NULL, NULL,
	    &initproc))
		panic("fork init");

	/*
	 * Create any kernel threads whose creation was deferred because
	 * initproc had not yet been created.
	 */
	kthread_run_deferred_queue();

	/*
	 * Now that device driver threads have been created, wait for
	 * them to finish any deferred autoconfiguration.  Note we don't
	 * need to lock this semaphore, since we haven't booted any
	 * secondary processors, yet.
	 */
	while (config_pending)
		(void) tsleep((void *)&config_pending, PWAIT, "cfpend", 0);

	dostartuphooks();

#if NSOFTRAID > 0
	config_rootfound("softraid", NULL);
#endif

	/* Configure root/swap devices */
	if (md_diskconf)
		(*md_diskconf)();

	/* Mount the root file system. */
	if (vfs_mountroot())
		panic("cannot mount root");
	CIRCLEQ_FIRST(&mountlist)->mnt_flag |= MNT_ROOTFS;

	/* Get the vnode for '/'.  Set p->p_fd->fd_cdir to reference it. */
	if (VFS_ROOT(CIRCLEQ_FIRST(&mountlist), &rootvnode))
		panic("cannot find root vnode");
	p->p_fd->fd_cdir = rootvnode;
	VREF(p->p_fd->fd_cdir);
	VOP_UNLOCK(rootvnode, 0, p);
	p->p_fd->fd_rdir = NULL;

	/*
	 * Now that root is mounted, we can fixup initproc's CWD
	 * info.  All other processes are kthreads, which merely
	 * share proc0's CWD info.
	 */
	initproc->p_fd->fd_cdir = rootvnode;
	VREF(initproc->p_fd->fd_cdir);
	initproc->p_fd->fd_rdir = NULL;

	/*
	 * Now can look at time, having had a chance to verify the time
	 * from the file system.  Reset p->p_rtime as it may have been
	 * munched in mi_switch() after the time got set.
	 */
#ifdef __HAVE_TIMECOUNTER
	microtime(&boottime);
#else
	boottime = mono_time = time;	
#endif
	LIST_FOREACH(p, &allproc, p_list) {
		p->p_stats->p_start = boottime;
		microuptime(&p->p_cpu->ci_schedstate.spc_runtime);
		p->p_rtime.tv_sec = p->p_rtime.tv_usec = 0;
	}

	uvm_swap_init();

	/* Create the pageout daemon kernel thread. */
	if (kthread_create(uvm_pageout, NULL, NULL, "pagedaemon"))
		panic("fork pagedaemon");

	/* Create the reaper daemon kernel thread. */
	if (kthread_create(start_reaper, NULL, NULL, "reaper"))
		panic("fork reaper");

	/* Create the cleaner daemon kernel thread. */
	if (kthread_create(start_cleaner, NULL, NULL, "cleaner"))
		panic("fork cleaner");

	/* Create the update daemon kernel thread. */
	if (kthread_create(start_update, NULL, NULL, "update"))
		panic("fork update");

	/* Create the aiodone daemon kernel thread. */ 
	if (kthread_create(uvm_aiodone_daemon, NULL, NULL, "aiodoned"))
		panic("fork aiodoned");

#ifdef CRYPTO
	/* Create the crypto kernel thread. */
	if (kthread_create(start_crypto, NULL, NULL, "crypto"))
		panic("crypto thread");
#endif /* CRYPTO */

	microtime(&rtv);
	srandom((u_long)(rtv.tv_sec ^ rtv.tv_usec));

	randompid = 1;

#if defined(MULTIPROCESSOR)
	/* Boot the secondary processors. */
	cpu_boot_secondary_processors();
#endif

	domountroothooks();

	/*
	 * Okay, now we can let init(8) exec!  It's off to userland!
	 */
	start_init_exec = 1;
	wakeup((void *)&start_init_exec);

	/* The scheduler is an infinite loop. */
	uvm_scheduler();
	/* NOTREACHED */
}

/*
 * List of paths to try when searching for "init".
 */
static char *initpaths[] = {
	"/sbin/init",
	"/sbin/oinit",
	"/sbin/init.bak",
	NULL,
};

void
check_console(struct proc *p)
{
	struct nameidata nd;
	int error;

	NDINIT(&nd, LOOKUP, FOLLOW, UIO_SYSSPACE, "/dev/console", p);
	error = namei(&nd);
	if (error) {
		if (error == ENOENT)
			printf("warning: /dev/console does not exist\n");
		else
			printf("warning: /dev/console error %d\n", error);
	} else
		vrele(nd.ni_vp);
}

/*
 * Start the initial user process; try exec'ing each pathname in "initpaths".
 * The program is invoked with one argument containing the boot flags.
 */
void
start_init(void *arg)
{
	struct proc *p = arg;
	vaddr_t addr;
	struct sys_execve_args /* {
		syscallarg(const char *) path;
		syscallarg(char *const *) argp;
		syscallarg(char *const *) envp;
	} */ args;
	int options, error;
	long i;
	register_t retval[2];
	char flags[4], *flagsp;
	char **pathp, *path, *ucp, **uap, *arg0, *arg1 = NULL;

	/*
	 * Now in process 1.
	 */

	/*
	 * Wait for main() to tell us that it's safe to exec.
	 */
	while (start_init_exec == 0)
		(void) tsleep((void *)&start_init_exec, PWAIT, "initexec", 0);

	check_console(p);

	/*
	 * Need just enough stack to hold the faked-up "execve()" arguments.
	 */
#ifdef MACHINE_STACK_GROWS_UP
	addr = USRSTACK;
#else
	addr = USRSTACK - PAGE_SIZE;
#endif
	if (uvm_map(&p->p_vmspace->vm_map, &addr, PAGE_SIZE, 
	    NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(UVM_PROT_RW, UVM_PROT_ALL, UVM_INH_COPY,
	    UVM_ADV_NORMAL, UVM_FLAG_FIXED|UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW)))
		panic("init: couldn't allocate argument space");
	p->p_vmspace->vm_maxsaddr = (caddr_t)addr;

	for (pathp = &initpaths[0]; (path = *pathp) != NULL; pathp++) {
#ifdef MACHINE_STACK_GROWS_UP
		ucp = (char *)addr;
#else
		ucp = (char *)(addr + PAGE_SIZE);
#endif
		/*
		 * Construct the boot flag argument.
		 */
		flagsp = flags;
		*flagsp++ = '-';
		options = 0;

		if (boothowto & RB_SINGLE) {
			*flagsp++ = 's';
			options = 1;
		}
#ifdef notyet
		if (boothowto & RB_FASTBOOT) {
			*flagsp++ = 'f';
			options = 1;
		}
#endif

		/*
		 * Move out the flags (arg 1), if necessary.
		 */
		if (options != 0) {
			*flagsp++ = '\0';
			i = flagsp - flags;
#ifdef DEBUG
			printf("init: copying out flags `%s' %d\n", flags, i);
#endif
#ifdef MACHINE_STACK_GROWS_UP
			arg1 = ucp;
			(void)copyout((caddr_t)flags, (caddr_t)ucp, i);
			ucp += i;
#else
			(void)copyout((caddr_t)flags, (caddr_t)(ucp -= i), i);
			arg1 = ucp;
#endif
		}

		/*
		 * Move out the file name (also arg 0).
		 */
		i = strlen(path) + 1;
#ifdef DEBUG
		printf("init: copying out path `%s' %d\n", path, i);
#endif
#ifdef MACHINE_STACK_GROWS_UP
		arg0 = ucp;
		(void)copyout((caddr_t)path, (caddr_t)ucp, i);
		ucp += i;
		ucp = (caddr_t)ALIGN((u_long)ucp);
		uap = (char **)ucp + 3;
#else
		(void)copyout((caddr_t)path, (caddr_t)(ucp -= i), i);
		arg0 = ucp;
		uap = (char **)((u_long)ucp & ~ALIGNBYTES);
#endif

		/*
		 * Move out the arg pointers.
		 */
		i = 0;
		copyout(&i, (caddr_t)--uap, sizeof(register_t)); /* terminator */
		if (options != 0)
			copyout(&arg1, (caddr_t)--uap, sizeof(register_t));
		copyout(&arg0, (caddr_t)--uap, sizeof(register_t));

		/*
		 * Point at the arguments.
		 */
		SCARG(&args, path) = arg0;
		SCARG(&args, argp) = uap;
		SCARG(&args, envp) = NULL;

		/*
		 * Now try to exec the program.  If can't for any reason
		 * other than it doesn't exist, complain.
		 */
		if ((error = sys_execve(p, &args, retval)) == 0) {
			KERNEL_PROC_UNLOCK(p);
			return;
		}
		if (error != ENOENT)
			printf("exec %s: error %d\n", path, error);
	}
	printf("init: not found\n");
	panic("no init");
}

void
start_update(void *arg)
{
	sched_sync(curproc);
	/* NOTREACHED */
}

void
start_cleaner(void *arg)
{
	buf_daemon(curproc);
	/* NOTREACHED */
}

void
start_reaper(void *arg)
{
	reaper();
	/* NOTREACHED */
}

#ifdef CRYPTO
void
start_crypto(void *arg)
{
	crypto_thread();
	/* NOTREACHED */
}
#endif /* CRYPTO */
