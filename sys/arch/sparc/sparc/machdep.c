/*	$OpenBSD: machdep.c,v 1.33 1998/03/25 07:54:59 jason Exp $	*/
/*	$NetBSD: machdep.c,v 1.85 1997/09/12 08:55:02 pk Exp $ */

/*
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This software was developed by the Computer Systems Engineering group
 * at Lawrence Berkeley Laboratory under DARPA contract BG 91-66 and
 * contributed to Berkeley.
 *
 * All advertising materials mentioning features or use of this software
 * must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Lawrence Berkeley Laboratory.
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
 *	@(#)machdep.c	8.6 (Berkeley) 1/14/94
 */

#include <sys/param.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/map.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif
#include <sys/exec.h>
#include <sys/sysctl.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <machine/autoconf.h>
#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/oldmon.h>
#include <machine/bsd_openprom.h>

#include <sparc/sparc/asm.h>
#include <sparc/sparc/cache.h>
#include <sparc/sparc/vaddrs.h>

#ifdef SUN4M
#include <sparc/dev/power.h>
#include "power.h"
#endif

#include "auxreg.h"

#ifdef SUN4
#include <sparc/dev/led.h>
#include "led.h"
#endif

vm_map_t buffer_map;
extern vm_offset_t avail_end;

/*
 * Declare these as initialized data so we can patch them.
 */
int	nswbuf = 0;
#ifdef	NBUF
int	nbuf = NBUF;
#else
int	nbuf = 0;
#endif
#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif

int	physmem;

extern struct msgbuf msgbuf;
struct	msgbuf *msgbufp = &msgbuf;
int	msgbufmapped = 0;	/* not mapped until pmap_bootstrap */

/* sysctl settable */
int	sparc_led_blink = 0;

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

/*
 * dvmamap is used to manage DVMA memory. Note: this coincides with
 * the memory range in `phys_map' (which is mostly a place-holder).
 */
vm_offset_t dvma_base, dvma_end;
struct map *dvmamap;
static int ndvmamap;	/* # of entries in dvmamap */

caddr_t allocsys __P((caddr_t));
void	dumpsys __P((void));
void	stackdump __P((void));

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
	register unsigned i;
	register caddr_t v;
	register int sz;
	int base, residual;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;
#endif
	vm_offset_t minaddr, maxaddr;
	vm_size_t size;
	extern struct user *proc0paddr;

#ifdef DEBUG
	pmapdebug = 0;
#endif

	proc0.p_addr = proc0paddr;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	/*identifycpu();*/
#ifndef MACHINE_NONCONTIG
	physmem = btoc(avail_end);
#endif
	printf("real mem = %d\n", ctob(physmem));

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);

	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");

	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;

	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
	    &maxaddr, size, TRUE);

	minaddr = (vm_offset_t)buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
			&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");

	base = bufpages / nbuf;
	residual = bufpages % nbuf;

	for (i = 0; i < nbuf; i++) {
		vm_size_t curbufsize;
		vm_offset_t curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.  The rest get (base) physical pages.
		 *
		 * The rest of each buffer occupies virtual space,
		 * but has no physical memory allocated for it.
		 */
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base+1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf+curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
				 16*NCARGS, TRUE);

	/*
	 * Allocate a map for physio.  Others use a submap of the kernel
	 * map, but we want one completely separate, even though it uses
	 * the same pmap.
	 */
	dvma_base = CPU_ISSUN4M ? DVMA4M_BASE : DVMA_BASE;
	dvma_end = CPU_ISSUN4M ? DVMA4M_END : DVMA_END;
	phys_map = vm_map_create(pmap_kernel(), dvma_base, dvma_end, 1);
	if (phys_map == NULL)
		panic("unable to create DVMA map");
	/*
	 * Allocate DVMA space and dump into a privately managed
	 * resource map for double mappings which is usable from
	 * interrupt contexts.
	 */
	if (kmem_alloc_wait(phys_map, (dvma_end-dvma_base)) != dvma_base)
		panic("unable to allocate from DVMA map");
	rminit(dvmamap, btoc((dvma_end-dvma_base)),
		vtorc(dvma_base), "dvmamap", ndvmamap);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS+CLBYTES/MCLBYTES,
				   M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS+CLBYTES/MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
			       VM_MBUF_SIZE, FALSE);
	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i-1].c_next = &callout[i];
	callout[i-1].c_next = NULL;

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %ld\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
		nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.  The cpu code will turn on the cache.
	 */
	configure();

	/*
	 * Re-zero proc0's user area, to nullify the effect of the
	 * stack running into it during auto-configuration.
	 * XXX - should fix stack usage.
	 * XXX - there's a race here, as interrupts are enabled
	 */
	bzero(proc0paddr, sizeof(struct user));

	/*
	 * fix message buffer mapping, note phys addr of msgbuf is 0
	 */

	pmap_enter(pmap_kernel(), MSGBUF_VA, 0x0, VM_PROT_READ|VM_PROT_WRITE, 1);
	if (CPU_ISSUN4)
		msgbufp = (struct msgbuf *)(MSGBUF_VA + 4096);
	else
		msgbufp = (struct msgbuf *)MSGBUF_VA;
	pmap_redzone();
}

/*
 * Allocate space for system data structures.  We are given
 * a starting virtual address and we return a final virtual
 * address; along the way we set each data structure pointer.
 *
 * You call allocsys() with 0 to find out how much space we want,
 * allocate that much and fill it with zeroes, and then call
 * allocsys() again with the correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{

#define	valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))
	valloc(callout, struct callout, ncallout);
	valloc(swapmap, struct map, nswapmap = maxproc * 2);
#ifdef SYSVSHM
	valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	/* This is pretty disgusting! */
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif
	/*
	 * Determine how many buffers to allocate (enough to
	 * hold 5% of total physical memory, but at least 16).
	 * Allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		bufpages = (physmem / ((100/BUFCACHEPERCENT) / CLSIZE));
	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nbuf > 200)
		nbuf = 200;	/* or we run out of PMEGS */
	/* Restrict to at most 70% filled kvm */
	if (nbuf * MAXBSIZE >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / CLBYTES)
		bufpages = nbuf * MAXBSIZE / CLBYTES;

	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) &~ 1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	/*
	 * Allocate DVMA slots for 1/4 of the number of i/o buffers
	 * and one for each process too (PHYSIO).
	 */
	valloc(dvmamap, struct map, ndvmamap = maxproc + ((nbuf / 4) &~ 1));
	return (v);
}

/*
 * Set up registers on exec.
 *
 * XXX this entire mess must be fixed
 */
/* ARGSUSED */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	u_long stack;
	register_t *retval;
{
	register struct trapframe *tf = p->p_md.md_tf;
	register struct fpstate *fs;
	register int psr;

	/* Don't allow misaligned code by default */
	p->p_md.md_flags &= ~MDP_FIXALIGN;

	/*
	 * The syscall will ``return'' to npc or %g7 or %g2; set them all.
	 * Set the rest of the registers to 0 except for %o6 (stack pointer,
	 * built in exec()) and psr (retain CWP and PSR_S bits).
	 */
	psr = tf->tf_psr & (PSR_S | PSR_CWP);
	if ((fs = p->p_md.md_fpstate) != NULL) {
		/*
		 * We hold an FPU state.  If we own *the* FPU chip state
		 * we must get rid of it, and the only way to do that is
		 * to save it.  In any case, get rid of our FPU state.
		 */
		if (p == fpproc) {
			savefpstate(fs);
			fpproc = NULL;
		}
		free((void *)fs, M_SUBPROC);
		p->p_md.md_fpstate = NULL;
	}
	bzero((caddr_t)tf, sizeof *tf);
	tf->tf_psr = psr;
	tf->tf_npc = pack->ep_entry & ~3;
	tf->tf_global[1] = (int)PS_STRINGS;
	tf->tf_global[2] = tf->tf_global[7] = tf->tf_npc;
	stack -= sizeof(struct rwindow);
	tf->tf_out[6] = stack;
	retval[1] = 0;
}

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

struct sigframe {
	int	sf_signo;		/* signal number */
	siginfo_t *sf_sip;		/* points to siginfo_t */
#ifdef COMPAT_SUNOS
	struct	sigcontext *sf_scp;	/* points to user addr of sigcontext */
#else
	int	sf_xxx;			/* placeholder */
#endif
	caddr_t	sf_addr;		/* SunOS compat */
	struct	sigcontext sf_sc;	/* actual sigcontext */
	siginfo_t sf_si;
};

/*
 * machine dependent system variables.
 */
int
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int *name;
	u_int namelen;
	void *oldp;
	size_t *oldlenp;
	void *newp;
	size_t newlen;
	struct proc *p;
{
#if (NAUXREG > 0) || (NLED > 0)
	int ret, oldval;
#endif

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	case CPU_LED_BLINK:
#if (NLED > 0) || (NAUXREG > 0)
		oldval = sparc_led_blink;
		ret = sysctl_int(oldp, oldlenp, newp, newlen,
		    &sparc_led_blink);

		/*
		 * If we were false and are now true, call led_blink().
		 * led_blink() itself will catch the other case.
		 */
		if (!oldval && sparc_led_blink > oldval) {
#if NAUXREG > 0
			led_blink((caddr_t *)0);
#endif
#if NLED > 0
			led_sun4_cycle((caddr_t *)0);
#endif
		}

		return (ret);
#else
		return (EOPNOTSUPP);
#endif
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	register struct proc *p = curproc;
	register struct sigacts *psp = p->p_sigacts;
	register struct sigframe *fp;
	register struct trapframe *tf;
	register int caddr, oonstack, oldsp, newsp;
	struct sigframe sf;
	extern char sigcode[], esigcode[];
#define	szsigcode	(esigcode - sigcode)
#ifdef COMPAT_SUNOS
	extern struct emul emul_sunos;
#endif

	tf = p->p_md.md_tf;
	oldsp = tf->tf_out[6];
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	/*
	 * Compute new user stack addresses, subtract off
	 * one signal frame, and align.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(psp->ps_sigstk.ss_sp +
					 psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)oldsp;
	fp = (struct sigframe *)((int)(fp - 1) & ~7);

#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig: %s[%d] sig %d newusp %p scp %p\n",
		    p->p_comm, p->p_pid, sig, fp, &fp->sf_sc);
#endif
	/*
	 * Now set up the signal frame.  We build it in kernel space
	 * and then copy it out.  We probably ought to just build it
	 * directly in user space....
	 */
	sf.sf_signo = sig;
	sf.sf_sip = NULL;
#ifdef COMPAT_SUNOS
	if (p->p_emul == &emul_sunos) {
		sf.sf_sip = (void *)code;	/* SunOS has "int code" */
		sf.sf_scp = &fp->sf_sc;
		sf.sf_addr = val.sival_ptr;
	}
#endif

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_onstack = oonstack;
	sf.sf_sc.sc_mask = mask;
	sf.sf_sc.sc_sp = oldsp;
	sf.sf_sc.sc_pc = tf->tf_pc;
	sf.sf_sc.sc_npc = tf->tf_npc;
	sf.sf_sc.sc_psr = tf->tf_psr;
	sf.sf_sc.sc_g1 = tf->tf_global[1];
	sf.sf_sc.sc_o0 = tf->tf_out[0];

	if (psp->ps_siginfo & sigmask(sig)) {
		sf.sf_sip = &fp->sf_si;
		initsiginfo(&sf.sf_si, sig, code, type, val);
	}

	/*
	 * Put the stack in a consistent state before we whack away
	 * at it.  Note that write_user_windows may just dump the
	 * registers into the pcb; we need them in the process's memory.
	 * We also need to make sure that when we start the signal handler,
	 * its %i6 (%fp), which is loaded from the newly allocated stack area,
	 * joins seamlessly with the frame it was in when the signal occurred,
	 * so that the debugger and _longjmp code can back up through it.
	 */
	newsp = (int)fp - sizeof(struct rwindow);
	write_user_windows();
	/* XXX do not copyout siginfo if not needed */
	if (rwindow_save(p) || copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf) ||
	    suword(&((struct rwindow *)newsp)->rw_in[6], oldsp)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig: window save or copyout error\n");
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig: %s[%d] sig %d scp %p\n",
		       p->p_comm, p->p_pid, sig, &fp->sf_sc);
#endif
	/*
	 * Arrange to continue execution at the code copied out in exec().
	 * It needs the function to call in %g1, and a new stack pointer.
	 */
#ifdef COMPAT_SUNOS
	if (psp->ps_usertramp & sigmask(sig)) {
		caddr = (int)catcher;	/* user does his own trampolining */
	} else
#endif
	{
		caddr = (int)PS_STRINGS - szsigcode;
		tf->tf_global[1] = (int)catcher;
	}
	tf->tf_pc = caddr;
	tf->tf_npc = caddr + 4;
	tf->tf_out[6] = newsp;
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig: about to return to catcher\n");
#endif
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above),
 * and return to the given trap frame (if there is one).
 * Check carefully to make sure that the user has not
 * modified the state to gain improper privileges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys_sigreturn(p, v, retval)
	register struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	register struct sigcontext *scp;
	register struct trapframe *tf;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();
	if (rwindow_save(p))
		sigexit(p, SIGILL);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: %s[%d], sigcntxp %p\n",
		    p->p_comm, p->p_pid, SCARG(uap, sigcntxp));
#endif
	scp = SCARG(uap, sigcntxp);
	if ((int)scp & 3 || useracc((caddr_t)scp, sizeof *scp, B_WRITE) == 0)
		return (EINVAL);
	tf = p->p_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((scp->sc_pc | scp->sc_npc) & 3) != 0)
		return (EINVAL);
	/* take only psr ICC field */
	tf->tf_psr = (tf->tf_psr & ~PSR_ICC) | (scp->sc_psr & PSR_ICC);
	tf->tf_pc = scp->sc_pc;
	tf->tf_npc = scp->sc_npc;
	tf->tf_global[1] = scp->sc_g1;
	tf->tf_out[0] = scp->sc_o0;
	tf->tf_out[6] = scp->sc_sp;
	if (scp->sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask & ~sigcantmask;
	return (EJUSTRETURN);
}

int	waittime = -1;

void
boot(howto)
	register int howto;
{
	int i;
	static char str[4];	/* room for "-sd\0" */
	extern int cold;

	if (cold) {
		printf("halted\n\n");
		romhalt();
	}

	fb_unblank();
	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;

		/* XXX protect against curproc->p_stats.foo refs in sync() */
		if (curproc == NULL)
			curproc = &proc0;
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now unless
		 * the system was sitting in ddb.
		 */
		if ((howto & RB_TIMEBAD) == 0) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}
	(void) splhigh();		/* ??? */
	if ((howto & RB_HALT) || (howto & RB_POWERDOWN)) {
		doshutdownhooks();
#if defined(SUN4M)
		if (howto & RB_POWERDOWN) {
#if NPOWER > 0
			printf("attempting to power down...\n");
			powerdown();
#else
			printf("WARNING: power not configured!\n");
#endif
		}
#endif
		printf("halted\n\n");
		romhalt();
	}
	if (howto & RB_DUMP)
		dumpsys();

	doshutdownhooks();
	printf("rebooting\n\n");
	i = 1;
	if (howto & RB_SINGLE)
		str[i++] = 's';
	if (howto & RB_KDB)
		str[i++] = 'd';
	if (i > 1) {
		str[0] = '-';
		str[i] = 0;
	} else
		str[0] = 0;
	romboot(str);
	/*NOTREACHED*/
}

/* XXX - dumpmag not eplicitly used, savecore may search for it to get here */
u_long	dumpmag = 0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

void
dumpconf()
{
	register int nblks, dumpblks;

	if (dumpdev == NODEV || bdevsw[major(dumpdev)].d_psize == 0)
		/* No usable dump device */
		return;

	nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);

	dumpblks = ctod(physmem) + ctod(pmap_dumpsize());
	if (dumpblks > (nblks - ctod(1)))
		/*
		 * dump size is too big for the partition.
		 * Note, we safeguard a click at the front for a
		 * possible disk label.
		 */
		return;

	/* Put the dump at the end of the partition */
	dumplo = nblks - dumpblks;

	/*
	 * savecore(8) expects dumpsize to be the number of pages
	 * of actual core dumped (i.e. excluding the MMU stuff).
	 */
	dumpsize = physmem;
}

#define	BYTES_PER_DUMP	(32 * 1024)	/* must be a multiple of pagesize */
static vm_offset_t dumpspace;

/*
 * Allocate the dump i/o buffer area during kernel memory allocation
 */
caddr_t
reserve_dumppages(p)
	caddr_t p;
{

	dumpspace = (vm_offset_t)p;
	return (p + BYTES_PER_DUMP);
}

/*
 * Write a crash dump.
 */
void
dumpsys()
{
	register int psize;
	daddr_t blkno;
	register int (*dump)	__P((dev_t, daddr_t, caddr_t, size_t));
	int error = 0;
	register struct memarr *mp;
	register int nmem;
	extern struct memarr pmemarr[];
	extern int npmemarr;

	/* copy registers to memory */
	snapshot(cpcb);
	stackdump();

	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo <= 0)
		return;
	printf("\ndumping to dev(%d,%d), at offset %ld blocks\n",
	    major(dumpdev), minor(dumpdev), dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;

	printf("mmu ");
	error = pmap_dumpmmu(dump, blkno);
	blkno += ctod(pmap_dumpsize());

	printf("memory ");
	for (mp = pmemarr, nmem = npmemarr; --nmem >= 0 && error == 0; mp++) {
		register unsigned i = 0, n;
		register unsigned maddr = mp->addr;

		/* XXX - what's so special about PA 0 that we can't dump it? */
		if (maddr == 0) {
			/* Skip first page at physical address 0 */
			maddr += NBPG;
			i += NBPG;
			blkno += btodb(NBPG);
		}

		printf("@%p:",maddr);

		for (; i < mp->len; i += n) {
			n = mp->len - i;
			if (n > BYTES_PER_DUMP)
				 n = BYTES_PER_DUMP;

			/* print out which MBs we are dumping */
			if (i % (1024*1024) <= NBPG)
				printf("%d ", i / (1024*1024));

			(void) pmap_map(dumpspace, maddr, maddr + n,
					VM_PROT_READ);
			error = (*dump)(dumpdev, blkno,
					(caddr_t)dumpspace, (int)n);
			pmap_remove(pmap_kernel(), dumpspace, dumpspace + n);
			if (error)
				break;
			maddr += n;
			blkno += btodb(n);
		}
	}

	switch (error) {

	case ENXIO:
		printf("device bad\n");
		break;

	case EFAULT:
		printf("device not ready\n");
		break;

	case EINVAL:
		printf("area improper\n");
		break;

	case EIO:
		printf("i/o error\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
}

/*
 * get the fp and dump the stack as best we can.  don't leave the
 * current stack page
 */
void
stackdump()
{
	struct frame *fp = getfp(), *sfp;

	sfp = fp;
	printf("Frame pointer is at %p\n", fp);
	printf("Call traceback:\n");
	while (fp && ((u_long)fp >> PGSHIFT) == ((u_long)sfp >> PGSHIFT)) {
		printf("  pc = 0x%x  args = (0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x) fp = %p\n",
		    fp->fr_pc, fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
		    fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5], fp->fr_arg[6],
		    fp->fr_fp);
		fp = fp->fr_fp;
	}
}

/*
 * Map an I/O device given physical address and size in bytes, e.g.,
 *
 *	mydev = (struct mydev *)mapdev(myioaddr, 0,
 *				       0, sizeof(struct mydev));
 *
 * See also machine/autoconf.h.
 */
void *
mapdev(phys, virt, offset, size)
	register struct rom_reg *phys;
	register int offset, virt, size;
{
	register vm_offset_t v;
	register vm_offset_t pa;
	register void *ret;
	static vm_offset_t iobase;
	unsigned int pmtype;

	if (iobase == NULL)
		iobase = IODEV_BASE;

	size = round_page(size);
	if (size == 0) panic("mapdev: zero size");

	if (virt)
		v = trunc_page(virt);
	else {
		v = iobase;
		iobase += size;
		if (iobase > IODEV_END)	/* unlikely */
			panic("mapiodev");
	}
	ret = (void *)(v | (((u_long)phys->rr_paddr + offset) & PGOFSET));
			/* note: preserve page offset */

	pa = trunc_page(phys->rr_paddr + offset);
	pmtype = PMAP_IOENC(phys->rr_iospace);

	do {
		pmap_enter(pmap_kernel(), v, pa | pmtype | PMAP_NC,
			   VM_PROT_READ | VM_PROT_WRITE, 1);
		v += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((size -= PAGE_SIZE) > 0);
	return (ret);
}

int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int error = ENOEXEC;

#ifdef COMPAT_SUNOS
	extern int sunos_exec_aout_makecmds __P((struct proc *, struct exec_package *));
	if ((error = sunos_exec_aout_makecmds(p, epp)) == 0)
		return 0;
#endif
	return error;
}

#ifdef SUN4
void
oldmon_w_trace(va)
	u_long va;
{
	u_long stop;
	struct frame *fp;

	if (curproc)
		printf("curproc = %p, pid %d\n", curproc, curproc->p_pid);
	else
		printf("no curproc\n");

	printf("cnt: swtch %d, trap %d, sys %d, intr %d, soft %d, faults %d\n",
	    cnt.v_swtch, cnt.v_trap, cnt.v_syscall, cnt.v_intr, cnt.v_soft,
	    cnt.v_faults);
	write_user_windows();

#define round_up(x) (( (x) + (NBPG-1) ) & (~(NBPG-1)) )

	printf("\nstack trace with sp = 0x%lx\n", va);
	stop = round_up(va);
	printf("stop at 0x%lx\n", stop);
	fp = (struct frame *) va;
	while (round_up((u_long) fp) == stop) {
		printf("  0x%x(0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x, 0x%x) fp %p\n", fp->fr_pc,
		    fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2], fp->fr_arg[3],
		    fp->fr_arg[4], fp->fr_arg[5], fp->fr_arg[6], fp->fr_fp);
		fp = fp->fr_fp;
		if (fp == NULL)
			break;
	}
	printf("end of stack trace\n");
}

void
oldmon_w_cmd(va, ar)
	u_long va;
	char *ar;
{
	switch (*ar) {
	case '\0':
		switch (va) {
		case 0:
			panic("g0 panic");
		case 4:
			printf("w: case 4\n");
			break;
		default:
			printf("w: unknown case %ld\n", va);
			break;
		}
		break;
	case 't':
		oldmon_w_trace(va);
		break;
	default:
		printf("w: arg not allowed\n");
	}
}
#endif /* SUN4 */

int
ldcontrolb(addr)
caddr_t addr;
{
	struct pcb *xpcb;
	extern struct user *proc0paddr;
	u_long saveonfault;
	int res;
	int s;

	if (CPU_ISSUN4M) {
		printf("warning: ldcontrolb called in sun4m\n");
		return 0;
	}

	s = splhigh();
	if (curproc == NULL)
		xpcb = (struct pcb *)proc0paddr;
	else
		xpcb = &curproc->p_addr->u_pcb;

	saveonfault = (u_long)xpcb->pcb_onfault;
        res = xldcontrolb(addr, xpcb);
	xpcb->pcb_onfault = (caddr_t)saveonfault;

	splx(s);
	return (res);
}

void
wzero(vb, l)
	void *vb;
	u_int l;
{
	u_char *b = vb;
	u_char *be = b + l;
	u_short *sp;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b & 1)
		*b++ = 0;

	/* back, */
	if (b != be && ((u_long)be & 1) != 0) {
		be--;
		*be = 0;
	}

	/* and middle. */
	sp = (u_short *)b;
	while (sp != (u_short *)be)
		*sp++ = 0;
}

void
wcopy(vb1, vb2, l)
	const void *vb1;
	void *vb2;
	u_int l;
{
	const u_char *b1e, *b1 = vb1;
	u_char *b2 = vb2;
	u_short *sp;
	int bstore = 0;

	if (l == 0)
		return;

	/* front, */
	if ((u_long)b1 & 1) {
		*b2++ = *b1++;
		l--;
	}

	/* middle, */
	sp = (u_short *)b1;
	b1e = b1 + l;
	if (l & 1)
		b1e--;
	bstore = (u_long)b2 & 1;

	while (sp < (u_short *)b1e) {
		if (bstore) {
			b2[1] = *sp & 0xff;
			b2[0] = *sp >> 8;
		} else
			*((short *)b2) = *sp;
		sp++;
		b2 += 2;
	}

	/* and back. */
	if (l & 1)
		*b2 = *b1e;
}
