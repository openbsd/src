/* $OpenBSD: machdep.c,v 1.21 2000/03/23 09:59:56 art Exp $ */
/* $NetBSD: machdep.c,v 1.45 1997/07/26 10:12:49 ragge Exp $	 */

/*
 * Copyright (c) 1994 Ludd, University of Lule}, Sweden.
 * Copyright (c) 1993 Adam Glass
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
 * All rights reserved.
 * 
 * Changed for the VAX port (and for readability) /IC
 * 
 * This code is derived from software contributed to Berkeley by the Systems
 * Programming Group of the University of Utah Computer Science Department.
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
 * from: Utah Hdr: machdep.c 1.63 91/04/24
 * 
 * @(#)machdep.c	7.16 (Berkeley) 6/3/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/kernel.h>
#include <sys/reboot.h>
#include <sys/msgbuf.h>
#include <sys/buf.h>
#include <sys/mbuf.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/timeout.h>
#include <sys/device.h>
#include <sys/exec.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/ptrace.h>
#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#include <vm/vm_kern.h>

#include <net/netisr.h>
#include <net/if.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip_var.h>
#endif
#ifdef NS
#include <netns/ns_var.h>
#endif
#include "ppp.h"	/* For NERISR_PPP */
#include "bridge.h"	/* For NERISR_BRIDGE */
#if NPPP > 0
#include <net/ppp_defs.h>
#include <net/if_ppp.h>
#endif

#include <machine/sid.h>
#include <machine/pte.h>
#include <machine/mtpr.h>
#include <machine/cpu.h>
#include <machine/macros.h>
#include <machine/nexus.h>
#include <machine/trap.h>
#include <machine/reg.h>
#include <machine/db_machdep.h>
#include <vax/vax/gencons.h>

#ifdef DDB
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>
#endif

void	netintr __P((void));
void	machinecheck __P((caddr_t));
void	cmrerr __P((void));
int	bufcalc __P((void));

extern vm_offset_t avail_end;
extern vm_offset_t virtual_avail, virtual_end;

/*
 * We do these external declarations here, maybe they should be done
 * somewhere else...
 */
int		nmcr, nmba, numuba, cold = 1;
caddr_t		mcraddr[MAXNMCR];
int		astpending;
int		want_resched;
char		machine[] = MACHINE;		/* from <machine/param.h> */
char		machine_arch[] = MACHINE_ARCH;	/* from <machine/param.h> */
char		cpu_model[100];
int		msgbufmapped = 0;
struct msgbuf  *msgbufp;
int		physmem;
struct cfdriver nexuscd;
int		todrstopped = 0, glurg;
int		dumpsize = 0;

caddr_t allocsys __P((caddr_t));

#define valloclim(name, type, num, lim) \
    (name) = (type *)v; v = (caddr_t)((lim) = ((name) + (num)))

#ifdef	BUFPAGES
int		bufpages = BUFPAGES;
#else
int		bufpages = 0;
#endif
int		nswbuf = 0;
#ifdef	NBUF
int		nbuf = NBUF;
#else
int		nbuf = 0;
#endif

void
cpu_startup()
{
	caddr_t		v;
	extern char	version[];
	int		base, residual, i, sz;
	vm_offset_t	minaddr, maxaddr;
	vm_size_t	size;

	/*
	 * Initialize error message buffer.
	 */
	msgbufmapped = 1;

#if VAX750 || VAX650
	if (vax_cputype == VAX_750 || vax_cputype == VAX_650)
		if (!mfpr(PR_TODR))
			mtpr(todrstopped = 1, PR_TODR);
#endif
	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf("%s\n", version);
	printf("realmem = %d\n", avail_end);
	physmem = btoc(avail_end);
	panicstr = NULL;
	mtpr(AST_NO, PR_ASTLVL);
	spl0();

	dumpsize = physmem + 1;

	/*
	 * Find out how much space we need, allocate it, and then give
	 * everything true virtual addresses.
	 */
	sz = (int)allocsys((caddr_t)0);
	if ((v = (caddr_t)kmem_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for tables");
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.	 They are different than the above in
	 * that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *)&buffers,
	    &maxaddr, size, TRUE);
	minaddr = (vm_offset_t) buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t)0,
	    &minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* don't want to alloc more physical mem than needed */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
	base = bufpages / nbuf;
	residual = bufpages % nbuf;
	for (i = 0; i < nbuf; i++) {
		vm_size_t	curbufsize;
		vm_offset_t	curbuf;

		/*
		 * First <residual> buffers get (base+1) physical pages
		 * allocated for them.	The rest get (base) physical pages.
		 * 
		 * The rest of each buffer occupies virtual space, but has no
		 * physical memory allocated for it.
		 */
		curbuf = (vm_offset_t)buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base + 1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf + curbufsize,
		    FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}

	/*
	 * Allocate a submap for exec arguments.  This map effectively limits
	 * the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr, 16 * NCARGS,
	    TRUE);

	/*
	 * Finally, allocate mbuf pool.	 Since mclrefcnt is an off-size we
	 * use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *)malloc(NMBCLUSTERS + CLBYTES / MCLBYTES, M_MBUF,
	    M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS + CLBYTES / MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *)&mbutl, &maxaddr,
	    VM_MBUF_SIZE, FALSE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, TRUE);

	timeout_init();

	printf("avail mem = %d\n", (int)ptoa(cnt.v_free_count));
	printf("Using %d buffers containing %d bytes of memory.\n", nbuf,
	    bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.
	 */
	if (boothowto & RB_CONFIG) {
#ifdef BOOT_CONFIG
		user_config();
#else
		printf("kernel does not support -c; continuing..\n");
#endif
	}
	configure();
}

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

int
bufcalc()
{
	/*
	 * Determine how many buffers to allocate. By default we allocate
	 * the BSD standard of use 10% of memory for the first 2 Meg,
	 * 5% of remaining.  But this might cause systems with large
	 * core (32MB) to fail to boot due to small KVM space.  Reduce
	 * BUFCACHEPERCENT in this case.  Insure a minimum of 16 buffers.
	 */
	if (bufpages == 0) {
		/* We always have more than 2MB of memory. */
		bufpages = btoc(avail_end + 2 * 1024 * 1024) /
		    ((100 / BUFCACHEPERCENT) * CLSIZE);
	}
	if (nbuf == 0)
		nbuf = bufpages;

	/* Restrict to at most 70% filled kvm */
	if (nbuf * MAXBSIZE >
	    (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) * 7 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS - VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 7 / 10;

	/* We really need some buffers.  */
	if (nbuf < 16)
		nbuf = 16;

	/* More buffer pages than fits into the buffer is senseless. */
	if (bufpages > nbuf * MAXBSIZE / CLBYTES) {
		bufpages = nbuf * MAXBSIZE / CLBYTES;
		return (bufpages * CLSIZE);
	}
	return (nbuf * MAXBSIZE / NBPG);
}

/*
 * Allocate space for system data structures.  We are given a starting
 * virtual address and we return a final virtual address; along the way we
 * set each data structure pointer.
 * 
 * We call allocsys() with 0 to find out how much space we want, allocate that
 * much and fill it with zeroes, and then call allocsys() again with the
 * correct base virtual address.
 */
caddr_t
allocsys(v)
	register caddr_t v;
{
#define valloc(name, type, num) v = (caddr_t)(((name) = (type *)v) + (num))

#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
	valloc(timeouts, struct timeout, ntimeout);
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

	/* Allocate 1/2 as many swap buffer headers as file i/o buffers.  */
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) & ~1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;		/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);
	return (v);
}

long	dumplo = 0;
long	dumpmag = 0x8fca0101;

void
dumpconf()
{
	int		nblks;
	extern int	dumpdev;

	/*
	 * XXX include the final RAM page which is not included in physmem.
	 */
	dumpsize = physmem + 1;
	if (dumpdev != NODEV && bdevsw[major(dumpdev)].d_psize) {
		nblks = (*bdevsw[major(dumpdev)].d_psize) (dumpdev);
		if (dumpsize > btoc(dbtob(nblks - dumplo)))
			dumpsize = btoc(dbtob(nblks - dumplo));
		else if (dumplo == 0)
			dumplo = nblks - btodb(ctob(dumpsize));
	}
	/*
	 * Don't dump on the first CLBYTES (why CLBYTES?) in case the dump
	 * device includes a disk label.
	 */
	if (dumplo < btodb(CLBYTES))
		dumplo = btodb(CLBYTES);
}

void
cpu_initclocks()
{
	(*dep_call->cpu_clock)();
}

int
cpu_sysctl(a, b, c, d, e, f, g)
	int	*a;
	u_int	b;
	void	*c, *e;
	size_t	*d, f;
	struct	proc *g;
{
	return (EOPNOTSUPP);
}

void
setstatclockrate(hzrate)
	int hzrate;
{
	panic("setstatclockrate");
}

void
consinit()
{
	cninit();
#ifdef DDB
/*	db_machine_init(); */
	ddb_init();
#ifdef donotworkbyunknownreason
	if (boothowto & RB_KDB)
		Debugger();
#endif
#endif
}

int
sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct trapframe *scf;
	struct sigcontext *cntx;

	scf = p->p_addr->u_pcb.framep;
	cntx = SCARG(uap, sigcntxp);

	/* Compatibility mode? */
	if ((cntx->sc_ps & (PSL_IPL | PSL_IS)) ||
	    ((cntx->sc_ps & (PSL_U | PSL_PREVU)) != (PSL_U | PSL_PREVU)) ||
	    (cntx->sc_ps & PSL_CM)) {
		return (EINVAL);
	}
	if (cntx->sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = cntx->sc_mask & ~sigcantmask;

	scf->fp = cntx->sc_fp;
	scf->ap = cntx->sc_ap;
	scf->pc = cntx->sc_pc;
	scf->sp = cntx->sc_sp;
	scf->psl = cntx->sc_ps;
	return (EJUSTRETURN);
}

struct trampframe {
	unsigned	sig;	/* Signal number */
	unsigned	code;	/* Info code */
	unsigned	scp;	/* Pointer to struct sigcontext */
	unsigned	r0, r1, r2, r3, r4, r5; /* Registers saved when
						 * interrupt */
	unsigned	pc;	/* Address of signal handler */
	unsigned	arg;	/* Pointer to first (and only) sigreturn
				 * argument */
};

void
sendsig(catcher, sig, mask, code, type, val)
	sig_t		catcher;
	int		sig, mask;
	u_long		code;
	int		type;
	union sigval	val;
{
	struct proc *p = curproc;
	struct sigacts *psp = p->p_sigacts;
	struct trapframe *syscf;
	struct sigcontext *sigctx;
	struct trampframe *trampf;
	unsigned cursp;
	int oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	extern char sigcode[], esigcode[];
	int fsize, rndfsize, kscsize;
	siginfo_t *sip, ksi;

	/*
	 * Allocate and validate space for the signal handler context. Note
	 * that if the stack is in P0 space, the call to grow() is a nop, and
	 * the useracc() check will fail if the process has not already
	 * allocated the space with a `brk'. We shall allocate space on the
	 * stack for both struct sigcontext and struct calls...
	 */
	syscf = p->p_addr->u_pcb.framep;
	fsize = sizeof *sigctx;
	rndfsize = ((fsize + 7) / 8) * 8;		/* XXX */
	kscsize = rndfsize;
	if (psp->ps_siginfo & sigmask(sig)) {
		fsize += sizeof ksi;
		rndfsize = ((fsize + 7) / 8) * 8;	/* XXX */
	}

	/* First check what stack to work on */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		cursp = (int)(psp->ps_sigstk.ss_sp + psp->ps_sigstk.ss_size - 
		    rndfsize);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		cursp = syscf->sp - rndfsize;
	if (cursp <= USRSTACK - ctob(p->p_vmspace->vm_ssize))
		(void)grow(p, cursp);

	/* Set up positions for structs on stack */
	sigctx = (struct sigcontext *)(cursp - sizeof(struct sigcontext));
	trampf = (struct trampframe *)((unsigned)sigctx -
	    sizeof(struct trampframe));

	 /* Place for pointer to arg list in sigreturn */
	cursp = (unsigned)sigctx - 8;

	if (useracc((caddr_t)cursp, fsize, B_WRITE) == 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		SIGACTION(p, SIGILL) = SIG_DFL;
		sig = sigmask(SIGILL);
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, SIGILL);
		return;
	}

	/* Set up pointers for sigreturn args */
	trampf->arg = (int) sigctx;
	trampf->pc = (unsigned) catcher;
	trampf->scp = (int) sigctx;
	trampf->code = code;
	trampf->sig = sig;

	sigctx->sc_pc = syscf->pc;
	sigctx->sc_ps = syscf->psl;
	sigctx->sc_ap = syscf->ap;
	sigctx->sc_fp = syscf->fp;
	sigctx->sc_sp = syscf->sp;
	sigctx->sc_onstack = oonstack;
	sigctx->sc_mask = mask;

	syscf->pc = (unsigned) (((char *) PS_STRINGS) - (esigcode - sigcode));
	syscf->psl = PSL_U | PSL_PREVU;
	syscf->ap = cursp;
	syscf->sp = cursp;

	if (psp->ps_siginfo & sigmask(sig)) {
		initsiginfo(&ksi, sig, code, type, val);
		sip = (void *)cursp + kscsize;
		copyout((caddr_t)&ksi, (caddr_t)sip, fsize - kscsize);
	}
}

int	waittime = -1;
static	volatile int showto; /* Must be volatile to survive MM on -> MM off */

void
boot(howto /* , bootstr */)
	register howto;
/*	char *bootstr; */
{
	showto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;
		vfs_shutdown();
		/*
		 * If we've been adjusting the clock, the todr will be out of
		 * synch; adjust it now.
		 */
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
	splhigh();		/* extreme priority */
	if (howto & RB_HALT) {
		if (dep_call->cpu_halt)
			(*dep_call->cpu_halt)();
		printf("halting (in tight loop); hit\n\t^P\n\tHALT\n\n");
		for (;;)
			;
	} else {
		showto = howto;
#ifdef notyet
		/*
		 * If we are provided with a bootstring, parse it and send
		 * it to the boot program.
		 */
		if (b)
			while (*b) {
				showto |= (*b == 'a' ? RB_ASKBOOT :
				    (*b == 'd' ? RB_DEBUG :
				    (*b == 's' ? RB_SINGLE : 0)));
				b++;
			}
#endif
		/*
		 * Now it's time to:
		 *  0. Save some registers that are needed in new world.
		 *  1. Change stack to somewhere that will survive MM off.
		 * (RPB page is good page to save things in).
		 *  2. Actually turn MM off.
		 *  3. Dump away memory to disk, if asked.
		 *  4. Reboot as asked.
		 * The RPB page is _always_ first page in memory, we can
		 * rely on that.
		 */
#ifdef notyet
		__asm volatile("movl	sp, (0x80000200);"
		    "movl	0x80000200, sp;"
		    "mfpr	$0x10, -(sp)	# PR_PCBB;"
		    "mfpr	$0x11, -(sp)	# PR_SCBB;"
		    "mfpr	$0xc, -(sp)	# PR_SBR;"
		    "mfpr	$0xd, -(sp)	# PR_SLR;"
		    "mtpr	$0, $0x38	# PR_MAPEN;");
#endif

		if (showto & RB_DUMP)
			dumpsys();
		if (dep_call->cpu_reboot)
			(*dep_call->cpu_reboot)(showto);

		switch (vax_cputype) {
			int	state;

#if VAX750 || VAX780
		case VAX_780:
		case VAX_750:
			mtpr(GC_BOOT, PR_TXDB); /* boot command */
			break;
#endif
#if VAX8600
		case VAX_8600:
			state = mfpr(PR_TXCS);
			gencnputc(0, GC_LT | GC_WRT);
			mtpr(0x2, PR_TXDB); /* XXX */
			gencnputc(0, state | GC_WRT);
			break;
#endif
		}

	}

	/* How to boot */
	__asm volatile ("movl %0, r5" : : "g" (showto));
	__asm volatile ("movl %0, r11" : : "r" (showto));	/* ??? */
	__asm volatile ("halt");
	panic("Halt failed");
}

void
netintr()
{
#ifdef INET
	if (netisr & (1 << NETISR_ARP)) {
		netisr &= ~(1 << NETISR_ARP);
		arpintr();
	}
	if (netisr & (1 << NETISR_IP)) {
		netisr &= ~(1 << NETISR_IP);
		ipintr();
	}
#endif
#ifdef INET6
	if (netisr & (1 << NETISR_IPV6)) {
		netisr &= ~(1 << NETISR_IPV6);
		ip6intr();
	}
#endif
#ifdef NETATALK
	if (netisr & (1 << NETISR_ATALK)) {
		netisr &= ~(1 << NETISR_ATALK);
		atintr();
	}
#endif
#ifdef NS
	if (netisr & (1 << NETISR_NS)) {
		netisr &= ~(1 << NETISR_NS);
		nsintr();
	}
#endif
#ifdef ISO
	if (netisr & (1 << NETISR_ISO)) {
		netisr &= ~(1 << NETISR_ISO);
		clnlintr();
	}
#endif
#ifdef CCITT
	if (netisr & (1 << NETISR_CCITT)) {
		netisr &= ~(1 << NETISR_CCITT);
		ccittintr();
	}
#endif
#if NPPP > 0
	if (netisr & (1 << NETISR_PPP)) {
		pppintr();
	}
#endif
#if NBRIDGE > 0
	if (netisr & (1 << NETISR_BRIDGE)) {
		bridgeintr();
	}
#endif
}

void
machinecheck(frame)
	caddr_t frame;
{
	if ((*dep_call->cpu_mchk)(frame) == 0)
		return;
	(*dep_call->cpu_memerr)();
	panic("machine check");
}

void
dumpsys()
{
	extern int	dumpdev;

	msgbufmapped = 0;
	if (dumpdev == NODEV)
		return;
	/*
	 * For dumps during autoconfiguration, if dump device has already
	 * configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %d\n", dumpdev, (int)dumplo);
	printf("dump ");
	switch ((*bdevsw[major(dumpdev)].d_dump)(dumpdev, 0, 0, 0)) {

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

	default:
		printf("succeeded\n");
		break;
	}
}

int
fuswintr(addr)
	caddr_t addr;
{
	panic("fuswintr: need to be implemented");
	return (0);
}

int
suibyte(base, byte)
	void *base;
	short byte;
{
	panic("suibyte: need to be implemented");
	return (0);
}

int
suswintr(addr, cnt)
	caddr_t	addr;
	u_int cnt;
{
	panic("suswintr: need to be implemented");
	return (0);
}

int
process_read_regs(p, regs)
	struct proc    *p;
	struct reg     *regs;
{
	struct trapframe *tf = p->p_addr->u_pcb.framep;

	bcopy(&tf->r0, &regs->r0, 12 * sizeof(int));
	regs->ap = tf->ap;
	regs->fp = tf->fp;
	regs->sp = tf->sp;
	regs->pc = tf->pc;
	regs->psl = tf->psl;
	return (0);
}

int
process_write_regs(p, regs)
	struct proc    *p;
	struct reg     *regs;
{
	struct trapframe *tf = p->p_addr->u_pcb.framep;

	bcopy(&regs->r0, &tf->r0, 12 * sizeof(int));
	tf->ap = regs->ap;
	tf->fp = regs->fp;
	tf->sp = regs->sp;
	tf->pc = regs->pc;
	tf->psl = regs->psl;
	return (0);
}

int
process_set_pc(p, addr)
	struct	proc *p;
	caddr_t addr;
{
	struct	trapframe *tf;
	void	*ptr;

	if ((p->p_flag & P_INMEM) == 0)
		return (EIO);

	ptr = (char *)p->p_addr->u_pcb.framep;
	tf = ptr;

	tf->pc = (unsigned)addr;

	return (0);
}

int
process_sstep(p, sstep)
	struct proc    *p;
{
	void	       *ptr;
	struct trapframe *tf;

	if ((p->p_flag & P_INMEM) == 0)
		return (EIO);

	ptr = p->p_addr->u_pcb.framep;
	tf = ptr;

	if (sstep)
		tf->psl |= PSL_T;
	else
		tf->psl &= ~PSL_T;

	return (0);
}

void
cmrerr()
{
	(*dep_call->cpu_memerr)();
}
