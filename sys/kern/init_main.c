/*	$OpenBSD: init_main.c,v 1.47 2000/02/28 18:04:08 provos Exp $	*/
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
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/resourcevar.h>
#include <sys/signalvar.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/vnode.h>
#include <sys/tty.h>
#include <sys/conf.h>
#include <sys/buf.h>
#ifdef REAL_CLISTS
#include <sys/clist.h>
#endif
#include <sys/device.h>
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

#include <sys/syscall.h>
#include <sys/syscallargs.h>

#include <ufs/ufs/quota.h>

#include <machine/cpu.h>

#include <vm/vm.h>
#include <vm/vm_pageout.h>

#if defined(UVM)
#include <uvm/uvm.h>
#endif

#include <net/if.h>
#include <net/raw_cb.h>

#if defined(NFSSERVER) || defined(NFSCLIENT)
extern void nfs_init __P((void));
#endif

#ifndef MIN
#define MIN(a,b)	(((a)<(b))?(a):(b))
#endif

char	copyright[] =
"Copyright (c) 1982, 1986, 1989, 1991, 1993\n"
"\tThe Regents of the University of California.  All rights reserved.\n"
"Copyright (c) 1995-2000 OpenBSD. All rights reserved.  http://www.OpenBSD.org\n";

/* Components of the first process -- never freed. */
struct	session session0;
struct	pgrp pgrp0;
struct	proc proc0;
struct	pcred cred0;
struct	filedesc0 filedesc0;
struct	plimit limit0;
struct	vmspace vmspace0;
struct	proc *curproc = &proc0;
struct	proc *initproc;

int	cmask = CMASK;
extern	struct user *proc0paddr;

void	(*md_diskconf) __P((void)) = NULL;
struct	vnode *rootvp, *swapdev_vp;
int	boothowto;
struct	timeval boottime;
struct	timeval runtime;

/* XXX return int so gcc -Werror won't complain */
int	main __P((void *));
void	check_console __P((struct proc *));
void	start_init __P((void *));
void	start_pagedaemon __P((void *));
void	start_update __P((void *));

#ifdef cpu_set_init_frame
void *initframep;				/* XXX should go away */
#endif

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
};

/*
 * System startup; initialize the world, create process 0, mount root
 * filesystem, and fork to create init and pagedaemon.  Most of the
 * hard work is done in the lower-level initialization routines including
 * startup(), which does memory initialization and autoconfiguration.
 */
/* XXX return int, so gcc -Werror won't complain */
int
main(framep)
	void *framep;				/* XXX should go away */
{
	register struct proc *p;
	register struct pdevinit *pdev;
	struct timeval rtv;
	register int i;
	int s;
	register_t rval[2];
	extern struct pdevinit pdevinit[];
	extern void roundrobin __P((void *));
	extern void schedcpu __P((void *));
	extern void disk_init __P((void));

	/*
	 * Initialize the current process pointer (curproc) before
	 * any possible traps/probes to simplify trap processing.
	 */
	p = &proc0;
	curproc = p;

	/*
	 * Attempt to find console and initialize
	 * in case of early panic or other messages.
	 */
	config_init();		/* init autoconfiguration data structures */
	consinit();
	printf(copyright);
	printf("\n");

#if defined(UVM)
	uvm_init();
#else
	vm_mem_init();
	kmeminit();
#if defined(MACHINE_NEW_NONCONTIG)
	vm_page_physrehash();
#endif
#endif /* UVM */
	disk_init();		/* must come before autoconfiguration */
	tty_init();		/* initialise tty's */
	cpu_startup();

	/*
	 * Initialize process and pgrp structures.
	 */
	procinit();

	/*
	 * Create process 0 (the swapper).
	 */
	LIST_INSERT_HEAD(&allproc, p, p_list);
	p->p_pgrp = &pgrp0;
	LIST_INSERT_HEAD(PGRPHASH(0), &pgrp0, pg_hash);
	LIST_INIT(&pgrp0.pg_members);
	LIST_INSERT_HEAD(&pgrp0.pg_members, p, p_pglist);

	pgrp0.pg_session = &session0;
	session0.s_count = 1;
	session0.s_leader = p;

	p->p_flag = P_INMEM | P_SYSTEM | P_NOCLDWAIT;
	p->p_stat = SRUN;
	p->p_nice = NZERO;
	p->p_emul = &emul_native;
	bcopy("swapper", p->p_comm, sizeof ("swapper"));

	/* Create credentials. */
	cred0.p_refcnt = 1;
	p->p_cred = &cred0;
	p->p_ucred = crget();
	p->p_ucred->cr_ngroups = 1;	/* group 0 */

	/* Create the file descriptor table. */
	p->p_fd = &filedesc0.fd_fd;
	filedesc0.fd_fd.fd_refcnt = 1;
	filedesc0.fd_fd.fd_cmask = cmask;
	filedesc0.fd_fd.fd_ofiles = filedesc0.fd_dfiles;
	filedesc0.fd_fd.fd_ofileflags = filedesc0.fd_dfileflags;
	filedesc0.fd_fd.fd_nfiles = NDFILE;
	filedesc0.fd_fd.fd_himap = filedesc0.fd_dhimap;
	filedesc0.fd_fd.fd_lomap = filedesc0.fd_dlomap;

	/* Create the limits structures. */
	p->p_limit = &limit0;
	for (i = 0; i < sizeof(p->p_rlimit)/sizeof(p->p_rlimit[0]); i++)
		limit0.pl_rlimit[i].rlim_cur =
		    limit0.pl_rlimit[i].rlim_max = RLIM_INFINITY;
	limit0.pl_rlimit[RLIMIT_NOFILE].rlim_cur = NOFILE;
	limit0.pl_rlimit[RLIMIT_NOFILE].rlim_max = MIN(NOFILE_MAX,
	    (maxfiles - NOFILE > NOFILE) ?  maxfiles - NOFILE : NOFILE);
	limit0.pl_rlimit[RLIMIT_NPROC].rlim_cur = MAXUPRC;
#if defined(UVM)
	i = ptoa(uvmexp.free);
#else
	i = ptoa(cnt.v_free_count);
#endif /* UVM */
	limit0.pl_rlimit[RLIMIT_RSS].rlim_max = i;
	limit0.pl_rlimit[RLIMIT_MEMLOCK].rlim_max = i;
	limit0.pl_rlimit[RLIMIT_MEMLOCK].rlim_cur = i / 3;
	limit0.p_refcnt = 1;

	/* Allocate a prototype map so we have something to fork. */
#if defined(UVM)
	uvmspace_init(&vmspace0, pmap_kernel(), round_page(VM_MIN_ADDRESS),
	    trunc_page(VM_MAX_ADDRESS), TRUE);
	p->p_vmspace = &vmspace0;
#else
	p->p_vmspace = &vmspace0;
	vmspace0.vm_refcnt = 1;
	pmap_pinit(&vmspace0.vm_pmap);
	vm_map_init(&p->p_vmspace->vm_map, round_page(VM_MIN_ADDRESS),
	    trunc_page(VM_MAX_ADDRESS), TRUE);
	vmspace0.vm_map.pmap = &vmspace0.vm_pmap;
#endif /* UVM */

	p->p_addr = proc0paddr;				/* XXX */

	/*
	 * We continue to place resource usage info and signal
	 * actions in the user struct so they're pageable.
	 */
	p->p_stats = &p->p_addr->u_stats;
	p->p_sigacts = &p->p_addr->u_sigacts;

	/*
	 * Charge root for one process.
	 */
	(void)chgproccnt(0, 1);

	rqinit();

	/* Configure virtual memory system, set vm rlimits. */
#if defined(UVM)
	uvm_init_limits(p);
#else
	vm_init_limits(p);
#endif

	/* Initialize the file systems. */
#if defined(NFSSERVER) || defined(NFSCLIENT)
	nfs_init();			/* initialize server/shared data */
#endif
	vfsinit();

	/* Start real time and statistics clocks. */
	initclocks();

	/* Initialize mbuf's. */
	mbinit();

#ifdef REAL_CLISTS
	/* Initialize clists. */
	clist_init();
#endif

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
		(*pdev->pdev_attach)(pdev->pdev_count);

	/*
	 * Initialize protocols.  Block reception of incoming packets
	 * until everything is ready.
	 */
	s = splimp();
	ifinit();
	domaininit();
	splx(s);

#ifdef GPROF
	/* Initialize kernel profiling. */
	kmstartup();
#endif

	/* Kick off timeout driven events by calling first time. */
	roundrobin(NULL);
	schedcpu(NULL);

	/* Configure root/swap devices */
	if (md_diskconf)
		(*md_diskconf)();

	/* Mount the root file system. */
	if (vfs_mountroot())
		panic("cannot mount root");
	mountlist.cqh_first->mnt_flag |= MNT_ROOTFS;

	/* Get the vnode for '/'.  Set filedesc0.fd_fd.fd_cdir to reference it. */
	if (VFS_ROOT(mountlist.cqh_first, &rootvnode))
		panic("cannot find root vnode");
	filedesc0.fd_fd.fd_cdir = rootvnode;
	VREF(filedesc0.fd_fd.fd_cdir);
	VOP_UNLOCK(rootvnode, 0, p);
	filedesc0.fd_fd.fd_rdir = NULL;
#if defined(UVM)
	uvm_swap_init();
#else
	swapinit();
#endif

	/*
	 * Now can look at time, having had a chance to verify the time
	 * from the file system.  Reset p->p_rtime as it may have been
	 * munched in mi_switch() after the time got set.
	 */
	p->p_stats->p_start = runtime = mono_time = boottime = time;
	p->p_rtime.tv_sec = p->p_rtime.tv_usec = 0;

	/* Initialize signal state for process 0. */
	siginit(p);

	/* Create process 1 (init(8)). */
	if (fork1(p, FORK_FORK, NULL, 0, rval))
		panic("fork init");
#ifdef cpu_set_init_frame			/* XXX should go away */
	if (rval[1]) {
		/*
		 * Now in process 1.
		 */
		initframep = framep;
		start_init(curproc);
		return (0);
	}
#else
	cpu_set_kpc(pfind(rval[0]), start_init, pfind(rval[0]));
#endif

	/* Create process 2, the pageout daemon kernel thread. */
	if (kthread_create(start_pagedaemon, NULL, NULL, "pagedaemon"))
		panic("fork pagedaemon");

	/* Create process 3, the update daemon kernel thread. */
	if (kthread_create(start_update, NULL, NULL, "update")) {
#ifdef DIAGNOSTIC
		panic("fork update");
#endif
	}

	/* Create any other deferred kernel threads. */
	kthread_run_deferred_queue();

	microtime(&rtv);
	srandom((u_long)(rtv.tv_sec ^ rtv.tv_usec));

	randompid = 1;
	/* The scheduler is an infinite loop. */
#if defined(UVM)
	uvm_scheduler();
#else
	scheduler();
#endif
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
check_console(p)
	struct proc *p;
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
start_init(arg)
	void *arg;
{
	struct proc *p = arg;
	vaddr_t addr;
	struct sys_execve_args /* {
		syscallarg(char *) path;
		syscallarg(char **) argp;
		syscallarg(char **) envp;
	} */ args;
	int options, i, error;
	register_t retval[2];
	char flags[4], *flagsp;
	char **pathp, *path, *ucp, **uap, *arg0, *arg1 = NULL;

	/*
	 * Now in process 1.
	 */
	initproc = p;

#ifdef cpu_set_init_frame			/* XXX should go away */
	/*
	 * We need to set the system call frame as if we were entered through
	 * a syscall() so that when we call sys_execve() below, it will be able
	 * to set the entry point (see setregs) when it tries to exec.  The
	 * startup code in "locore.s" has allocated space for the frame and
	 * passed a pointer to that space as main's argument.
	 */
	cpu_set_init_frame(p, initframep);
#endif

	check_console(p);

	/*
	 * Need just enough stack to hold the faked-up "execve()" arguments.
	 */
#ifdef MACHINE_STACK_GROWS_UP
	addr = USRSTACK;
#else
	addr = USRSTACK - PAGE_SIZE;
#endif
#if defined(UVM)
	if (uvm_map(&p->p_vmspace->vm_map, &addr, PAGE_SIZE, 
                    NULL, UVM_UNKNOWN_OFFSET, 
                    UVM_MAPFLAG(UVM_PROT_ALL, UVM_PROT_ALL, UVM_INH_COPY,
		    UVM_ADV_NORMAL,
                    UVM_FLAG_FIXED|UVM_FLAG_OVERLAY|UVM_FLAG_COPYONW))
		!= KERN_SUCCESS)
		panic("init: couldn't allocate argument space");
#else
	if (vm_allocate(&p->p_vmspace->vm_map, &addr, (vsize_t)PAGE_SIZE,
	    FALSE) != 0)
		panic("init: couldn't allocate argument space");
#endif
#ifdef MACHINE_STACK_GROWS_UP
	p->p_vmspace->vm_maxsaddr = (caddr_t)addr + PAGE_SIZE;
#else
	p->p_vmspace->vm_maxsaddr = (caddr_t)addr;
#endif

	for (pathp = &initpaths[0]; (path = *pathp) != NULL; pathp++) {
		ucp = (char *)(addr + PAGE_SIZE);

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
			(void)copyout((caddr_t)flags, (caddr_t)(ucp -= i), i);
			arg1 = ucp;
		}

		/*
		 * Move out the file name (also arg 0).
		 */
		i = strlen(path) + 1;
#ifdef DEBUG
		printf("init: copying out path `%s' %d\n", path, i);
#endif
		(void)copyout((caddr_t)path, (caddr_t)(ucp -= i), i);
		arg0 = ucp;

		/*
		 * Move out the arg pointers.
		 */
		uap = (char **)((long)ucp & ~ALIGNBYTES);
		(void)suword((caddr_t)--uap, 0);	/* terminator */
		if (options != 0)
			(void)suword((caddr_t)--uap, (long)arg1);
		(void)suword((caddr_t)--uap, (long)arg0);

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
		if ((error = sys_execve(p, &args, retval)) == 0)
			return;
		if (error != ENOENT)
			printf("exec %s: error %d\n", path, error);
	}
	printf("init: not found\n");
	panic("no init");
}

void
start_pagedaemon(arg)
	void *arg;
{
#if defined(UVM)
	uvm_pageout();
#else
	vm_pageout();
#endif
	/* NOTREACHED */
}

void
start_update(arg)
	void *arg;
{
	sched_sync(curproc);
	/* NOTREACHED */
}
