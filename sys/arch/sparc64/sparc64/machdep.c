/*	$OpenBSD: machdep.c,v 1.47 2002/07/20 19:24:57 art Exp $	*/
/*	$NetBSD: machdep.c,v 1.108 2001/07/24 19:30:14 eeh Exp $ */

/*-
 * Copyright (c) 1996, 1997, 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Jason R. Thorpe of the Numerical Aerospace Simulation Facility,
 * NASA Ames Research Center.
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
 *	This product includes software developed by the NetBSD
 *	Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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

#include "auxio.h"

#include <sys/param.h>
#include <sys/extent.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/reboot.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>

#include <uvm/uvm.h>

#include <sys/sysctl.h>
#include <sys/exec_elf.h>

#ifdef SYSVMSG
#include <sys/msg.h>
#endif
#ifdef SYSVSEM
#include <sys/sem.h>
#endif
#ifdef SYSVSHM
#include <sys/shm.h>
#endif

#define _SPARC_BUS_DMA_PRIVATE
#include <machine/autoconf.h>
#include <machine/bus.h>
#include <machine/frame.h>
#include <machine/cpu.h>
#include <machine/pmap.h>
#include <machine/openfirm.h>
#include <machine/sparc64.h>

#include <sparc64/sparc64/cache.h>

#include "pckbc.h"
#include "pckbd.h"
#if (NPCKBC > 0) && (NPCKBD == 0)
#include <dev/ic/pckbcvar.h>
#endif

/* #include "fb.h" */

int bus_space_debug = 0; /* This may be used by macros elsewhere. */
#ifdef DEBUG
#define DPRINTF(l, s)   do { if (bus_space_debug & l) printf s; } while (0)
#else
#define DPRINTF(l, s)
#endif

struct vm_map *exec_map = NULL;
extern vaddr_t avail_end;

#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 5
#endif

#ifdef	BUFPAGES
int	bufpages = BUFPAGES;
#else
int	bufpages = 0;
#endif
int	bufcachepercent = BUFCACHEPERCENT;

int	physmem;
u_long	_randseed;
extern	caddr_t msgbufaddr;

#if NAUXIO > 0
#include <sparc64/dev/auxiovar.h>
int sparc_led_blink;
#endif

#ifdef APERTURE
#ifdef INSECURE
int allowaperture = 1;
#else
int allowaperture = 0;
#endif
#endif

/*
 * Maximum number of DMA segments we'll allow in dmamem_load()
 * routines.  Can be overridden in config files, etc.
 */
#ifndef MAX_DMA_SEGS
#define MAX_DMA_SEGS	20
#endif

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int   safepri = 0;

caddr_t	allocsys(caddr_t);
void	dumpsys(void);
void	stackdump(void);

/* 
 * This is the table that tells us how to access different bus space types.
 */ 
#define BUS_BYPASS_ACCESS_ENABLED 0
#if BUS_BYPASS_ACCESS_ENABLED == 1
/*
 * Bypass access 
 */
int bus_type_asi[] = {
	ASI_PHYS_NON_CACHED,			/* UPA */
	ASI_PHYS_NON_CACHED,			/* SBUS */
	ASI_PHYS_NON_CACHED_LITTLE,		/* PCI configuration space */
	ASI_PHYS_NON_CACHED_LITTLE,		/* PCI memory space */
	ASI_PHYS_NON_CACHED_LITTLE,		/* PCI I/O space */
	0
};

int bus_stream_asi[] = {
	ASI_PHYS_NON_CACHED,			/* UPA */
	ASI_PHYS_NON_CACHED,			/* SBUS */
	ASI_PHYS_NON_CACHED,			/* PCI configuration space */
	ASI_PHYS_NON_CACHED,			/* PCI memory space */
	ASI_PHYS_NON_CACHED,			/* PCI I/O space */
	0
};
#else
/*
 * MMU access - we want to use the MMU for all this..
 */
int bus_type_asi[] = {
	ASI_PRIMARY,				/* UPA */
	ASI_PRIMARY,				/* SBUS */
	ASI_PHYS_NON_CACHED_LITTLE,		/* PCI configuration space */
	ASI_PRIMARY,				/* PCI memory space */
	ASI_PRIMARY,				/* PCI I/O space */
	0
};

int bus_stream_asi[] = {
	ASI_PRIMARY,				/* UPA */
	ASI_PRIMARY,				/* SBUS */
	ASI_PHYS_NON_CACHED,			/* PCI configuration space */
	ASI_PRIMARY_LITTLE,			/* PCI memory space */
	ASI_PRIMARY_LITTLE,			/* PCI I/O space */
	0
};
#endif

#if (NPCKBC > 0) && (NPCKBD == 0)
/*
 * This is called by the pckbc driver if no pckbd is configured.
 * On the i386, it is used to glue in the old, deprecated console
 * code.  On the sparc64, it does nothing.
 */
int
pckbc_machdep_cnattach(kbctag, kbcslot)
	pckbc_tag_t kbctag;
	pckbc_slot_t kbcslot;
{
	return (ENXIO);
}
#endif

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
	unsigned i;
	caddr_t v;
	long sz;
	int base, residual;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;
#endif
	vaddr_t minaddr, maxaddr;
	vsize_t size;
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
	printf("total memory = %ld\n", (long)physmem * PAGE_SIZE);

	/*
	 * Find out how much space we need, allocate it,
	 * and then give everything true virtual addresses.
	 */
	sz = (long)allocsys(NULL);
	if ((v = (caddr_t)uvm_km_alloc(kernel_map, round_page(sz))) == 0)
		panic("startup: no room for %lx bytes of tables", sz);
	if (allocsys(v) - v != sz)
		panic("startup: table size inconsistency");

        /*
         * allocate virtual and physical memory for the buffers.
         */
        size = MAXBSIZE * nbuf;         /* # bytes for buffers */

        /* allocate VM for buffers... area is not managed by VM system */
        if (uvm_map(kernel_map, (vaddr_t *) &buffers, round_page(size),
                    NULL, UVM_UNKNOWN_OFFSET, 0,
                    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
                                UVM_ADV_NORMAL, 0)) != 0)
        	panic("cpu_startup: cannot allocate VM for buffers");

        minaddr = (vaddr_t) buffers;
        if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
        	bufpages = btoc(MAXBSIZE) * nbuf; /* do not overallocate RAM */
        }
        base = bufpages / nbuf;
        residual = bufpages % nbuf;

        /* now allocate RAM for buffers */
	for (i = 0 ; i < nbuf ; i++) {
		vaddr_t curbuf;
		vsize_t curbufsize;
		struct vm_page *pg;

		/*
		 * each buffer has MAXBSIZE bytes of VM space allocated.  of
		 * that MAXBSIZE space we allocate and map (base+1) pages
		 * for the first "residual" buffers, and then we allocate
		 * "base" pages for the rest.
		 */
		curbuf = (vaddr_t) buffers + (i * MAXBSIZE);
		curbufsize = NBPG * ((i < residual) ? (base+1) : base);

		while (curbufsize) {
			pg = uvm_pagealloc(NULL, 0, NULL, 0);
			if (pg == NULL)
				panic("cpu_startup: "
				    "not enough RAM for buffer cache");
			pmap_enter(kernel_map->pmap, curbuf,
			    VM_PAGE_TO_PHYS(pg), VM_PROT_READ|VM_PROT_WRITE,
			    VM_PROT_READ|VM_PROT_WRITE|PMAP_WIRED);
			curbuf += PAGE_SIZE;
			curbufsize -= PAGE_SIZE;
		}
	}
	pmap_update(pmap_kernel());

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
        exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
                                 16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail memory = %ld\n", (long)uvmexp.free * PAGE_SIZE);
	printf("using %d buffers containing %ld of memory\n", nbuf,
		(long)bufpages * PAGE_SIZE);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

#if 0
	pmap_redzone();
#endif
}

caddr_t
allocsys(caddr_t v)
{
#define valloc(name, type, num) \
	    v = (caddr_t)(((name) = (type *)v) + (num))
#ifdef SYSVSHM
	shminfo.shmmax = shmmaxpgs;
	shminfo.shmall = shmmaxpgs;
	shminfo.shmseg = shmseg;
        valloc(shmsegs, struct shmid_ds, shminfo.shmmni);
#endif
#ifdef SYSVSEM
	valloc(sema, struct semid_ds, seminfo.semmni);
	valloc(sem, struct sem, seminfo.semmns);
	valloc(semu, int, (seminfo.semmnu * seminfo.semusz) / sizeof(int));
#endif
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

        /*
	 * Determine how many buffers to allocate (enough to
	 * hold 5% of total physical memory, but at least 16).
	 * Allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	 if (bufpages == 0)
	 	bufpages = physmem * bufcachepercent / 100;
	 if (nbuf == 0) {
	 	nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	/* Restrict to at most 30% filled kvm */
	if (nbuf * MAXBSIZE >
	    (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) * 3 / 10)
		nbuf = (VM_MAX_KERNEL_ADDRESS-VM_MIN_KERNEL_ADDRESS) /
		    MAXBSIZE * 3 / 10;

	/* More buffer pages than fits into the buffers is senseless.  */
	if (bufpages > nbuf * MAXBSIZE / PAGE_SIZE)
		bufpages = nbuf * MAXBSIZE / PAGE_SIZE;

	valloc(buf, struct buf, nbuf);

	return (v);
}

/*
 * Set up registers on exec.
 */

#define STACK_OFFSET	BIAS
#define CPOUTREG(l,v)	copyout(&(v), (l), sizeof(v))
#undef CCFSZ
#define CCFSZ	CC64FSZ

/* ARGSUSED */
void
setregs(p, pack, stack, retval)
	struct proc *p;
	struct exec_package *pack;
	vaddr_t stack;
	register_t *retval;
{
	struct trapframe64 *tf = p->p_md.md_tf;
	struct fpstate64 *fs;
	int64_t tstate;
	int pstate = PSTATE_USER;
	Elf_Ehdr *eh = pack->ep_hdr;

	/* Don't allow misaligned code by default */
	p->p_md.md_flags &= ~MDP_FIXALIGN;

	/*
	 * Set the registers to 0 except for:
	 *	%o6: stack pointer, built in exec())
	 *	%tstate: (retain icc and xcc and cwp bits)
	 *	%g1: address of PS_STRINGS (used by crt0)
	 *	%tpc,%tnpc: entry point of program
	 */
	/* Check what memory model is requested */
	switch ((eh->e_flags & EF_SPARCV9_MM)) {
	default:
		printf("Unknown memory model %d\n", 
		       (eh->e_flags & EF_SPARCV9_MM));
		/* FALLTHROUGH */
	case EF_SPARCV9_TSO:
		pstate = PSTATE_MM_TSO|PSTATE_IE;
		break;
	case EF_SPARCV9_PSO:
		pstate = PSTATE_MM_PSO|PSTATE_IE;
		break;
	case EF_SPARCV9_RMO:
		pstate = PSTATE_MM_RMO|PSTATE_IE;
		break;
	}

	tstate = (ASI_PRIMARY_NO_FAULT<<TSTATE_ASI_SHIFT) |
		((pstate)<<TSTATE_PSTATE_SHIFT) | 
		(tf->tf_tstate & TSTATE_CWP);
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
	tf->tf_tstate = tstate;
	tf->tf_global[1] = (u_long)PS_STRINGS;
	/* %g4 needs to point to the start of the data segment */
	tf->tf_global[4] = 0; 
	tf->tf_pc = pack->ep_entry & ~3;
	tf->tf_npc = tf->tf_pc + 4;
	tf->tf_global[2] = tf->tf_global[7] = tf->tf_pc;
	stack -= sizeof(struct rwindow);
	tf->tf_out[6] = stack - STACK_OFFSET;
	tf->tf_out[7] = NULL;
#ifdef NOTDEF_DEBUG
	printf("setregs: setting tf %p sp %p pc %p\n", (long)tf, 
	       (long)tf->tf_out[6], (long)tf->tf_pc);
#endif
	retval[1] = 0;
}

#ifdef DEBUG
/* See sigdebug.h */
#include <sparc64/sparc64/sigdebug.h>
int sigdebug = 0x0;
int sigpid = 0;
#endif

struct sigframe {
	int	sf_signo;		/* signal number */
	int	sf_code;		/* signal code (unused) */
	siginfo_t *sf_sip;		/* points to siginfo_t */
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
#if NAUXIO > 0
	int oldval, ret;
#endif
	u_int chosen;
	char bootargs[256];
	char *cp = NULL;

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	case CPU_BOOTED_KERNEL:
		if (((chosen = OF_finddevice("/chosen")) != -1) &&
		    ((OF_getprop(chosen, "bootargs", bootargs, sizeof bootargs))
		      >= 0)) {
			/*
			 * bootargs is of the form: [kernelname] [args...]
			 * It can be the empty string if we booted from the default
			 * kernel name.
			 */
			for (cp = bootargs; 
			     *cp && *cp != ' ' && *cp != '\t' && *cp != '\n';
			     cp++);
			*cp = 0;
			/* Now we've separated out the kernel name from the args */
			cp = bootargs;
			if (*cp == 0 || *cp == '-') 
				/*
				 * We can leave it NULL && let userland handle
				 * the failure or set it to the default name,
				 * `bsd' 
				 */
				cp = "bsd";
		}
		if (cp == NULL || cp[0] == '\0')
			return (ENOENT);
		return (sysctl_rdstring(oldp, oldlenp, newp, cp));
	case CPU_LED_BLINK:
#if NAUXIO > 0
		oldval = sparc_led_blink;
		ret = sysctl_int(oldp, oldlenp, newp, newlen,
		    &sparc_led_blink);

		/*
		 * If we were false and are now true, call auxio_led_blink().
		 * auxio_led_blink() will catch the other case itself.
		 */
		if (!oldval && sparc_led_blink > oldval)
			auxio_led_blink(NULL);
		return (ret);
#else
		return (EOPNOTSUPP);
#endif
	case CPU_ALLOWAPERTURE:
#ifdef APERTURE
		if (securelevel > 0)
			return (sysctl_rdint(oldp, oldlenp, newp,
			    allowaperture));
		else
			return (sysctl_int(oldp, oldlenp, newp, newlen,
			    &allowaperture));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	case CPU_CPUTYPE:
		return (sysctl_rdint(oldp, oldlenp, newp, CPU_SUN4U));
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
	struct proc *p = curproc;
	struct sigacts *psp = p->p_sigacts;
	struct sigframe *fp;
	struct trapframe64 *tf;
	vaddr_t addr; 
	struct rwindow *oldsp, *newsp;
	struct sigframe sf;
	int onstack;

	tf = p->p_md.md_tf;
	oldsp = (struct rwindow *)(u_long)(tf->tf_out[6] + STACK_OFFSET);

	/*
	 * Compute new user stack addresses, subtract off
	 * one signal frame, and align.
	 */
	onstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	if ((psp->ps_flags & SAS_ALTSTACK) && !onstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)((caddr_t)psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)oldsp;
	/* Allocate an aligned sigframe */
	fp = (struct sigframe *)((long)(fp - 1) & ~0x0f);

	/*
	 * Now set up the signal frame.  We build it in kernel space
	 * and then copy it out.  We probably ought to just build it
	 * directly in user space....
	 */
	sf.sf_signo = sig;
	sf.sf_sip = NULL;

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_sc.sc_onstack = onstack;
	sf.sf_sc.sc_mask = mask;
	/* Save register context. */
	sf.sf_sc.sc_sp = (long)tf->tf_out[6];
	sf.sf_sc.sc_pc = tf->tf_pc;
	sf.sf_sc.sc_npc = tf->tf_npc;
	sf.sf_sc.sc_tstate = tf->tf_tstate; /* XXX */
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
	newsp = (struct rwindow *)((vaddr_t)fp - sizeof(struct rwindow));
	write_user_windows();

	/* XXX do not copyout siginfo if not needed */
	if (rwindow_save(p) || copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf) || 
	    CPOUTREG(&(((struct rwindow *)newsp)->rw_in[6]), tf->tf_out[6])) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		printf("sendsig: stack was trashed trying to send sig %d, sending SIGILL\n", sig);
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW) {
		printf("sendsig: %s[%d] sig %d scp %p\n",
		       p->p_comm, p->p_pid, sig, &fp->sf_sc);
	}
#endif

	/*
	 * Arrange to continue execution at the code copied out in exec().
	 * It needs the function to call in %g1, and a new stack pointer.
	 */
	addr = p->p_sigcode;
	tf->tf_global[1] = (vaddr_t)catcher;
	tf->tf_pc = addr;
	tf->tf_npc = addr + 4;
	tf->tf_out[6] = (vaddr_t)newsp - STACK_OFFSET;
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
	struct sigcontext sc, *scp;
	register struct trapframe64 *tf;
	int error = EINVAL;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();

	if (rwindow_save(p)) {
#ifdef DEBUG
		printf("sigreturn: rwindow_save(%p) failed, sending SIGILL\n", p);
#endif
		sigexit(p, SIGILL);
	}
	scp = SCARG(uap, sigcntxp);
 	if ((vaddr_t)scp & 3 ||
	    (error = copyin((caddr_t)scp, &sc, sizeof sc) != 0)) {
#ifdef DEBUG
		printf("sigreturn: copyin failed: scp=%p\n", scp);
#endif
		return (error);
	}
	scp = &sc;

	tf = p->p_md.md_tf;
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((sc.sc_pc | sc.sc_npc) & 3) != 0 ||
	    (sc.sc_pc == 0) || (sc.sc_npc == 0)) {
#ifdef DEBUG
		printf("sigreturn: pc %p or npc %p invalid\n",
		   (void *)(unsigned long)sc.sc_pc,
		   (void *)(unsigned long)sc.sc_npc);
#endif
		return (EINVAL);
	}

	/* take only psr ICC field */
	tf->tf_tstate = (u_int64_t)(tf->tf_tstate & ~TSTATE_CCR) | (scp->sc_tstate & TSTATE_CCR);
	tf->tf_pc = (u_int64_t)scp->sc_pc;
	tf->tf_npc = (u_int64_t)scp->sc_npc;
	tf->tf_global[1] = (u_int64_t)scp->sc_g1;
	tf->tf_out[0] = (u_int64_t)scp->sc_o0;
	tf->tf_out[6] = (u_int64_t)scp->sc_sp;

	/* Restore signal stack. */
	if (sc.sc_onstack & SS_ONSTACK)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
	p->p_sigmask = scp->sc_mask & ~sigcantmask;

	return (EJUSTRETURN);
}

int	waittime = -1;

void
boot(howto)
	int howto;
{
	int i;
	static char str[128];

	/* If system is cold, just halt. */
	if (cold) {
		howto |= RB_HALT;
		goto haltsys;
	}

#if NFB > 0
	fb_unblank();
#endif
	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		extern struct proc proc0;
		extern int sparc_clock_time_is_ok;

		/* XXX protect against curproc->p_stats.foo refs in sync() */
		if (curproc == NULL)
			curproc = &proc0;
		waittime = 0;
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 * Do this only if the TOD clock has already been read out
		 * successfully by inittodr() or set by an explicit call
		 * to resettodr() (e.g. from settimeofday()).
		 */
		if ((howto & RB_TIMEBAD) == 0 && sparc_clock_time_is_ok) {
			resettodr();
		} else {
			printf("WARNING: not updating battery clock\n");
		}
	}
	(void) splhigh();		/* ??? */

	/* If rebooting and a dump is requested, do it. */
	if (howto & RB_DUMP)
		dumpsys();

haltsys:
	/* Run any shutdown hooks. */
	doshutdownhooks();

	/* If powerdown was requested, do it. */
	if ((howto & RB_POWERDOWN) == RB_POWERDOWN) {
		/* Let the OBP do the work. */
		OF_poweroff();
		printf("WARNING: powerdown failed!\n");
		/*
		 * RB_POWERDOWN implies RB_HALT... fall into it...
		 */
	}

	if (howto & RB_HALT) {
		printf("halted\n\n");
		OF_exit();
		panic("PROM exit failed");
	}

	printf("rebooting\n\n");
#if 0
	if (user_boot_string && *user_boot_string) {
		i = strlen(user_boot_string);
		if (i > sizeof(str))
			OF_boot(user_boot_string);	/* XXX */
		bcopy(user_boot_string, str, i);
	} else
#endif
	{
		i = 1;
		str[0] = '\0';
	}
			
	if (howto & RB_SINGLE)
		str[i++] = 's';
	if (howto & RB_KDB)
		str[i++] = 'd';
	if (i > 1) {
		if (str[0] == '\0')
			str[0] = '-';
		str[i] = 0;
	} else
		str[0] = 0;
	OF_boot(str);
	panic("cpu_reboot -- failed");
	/*NOTREACHED*/
}

u_long	dumpmag = 0x8fca0101;	/* magic number for savecore */
int	dumpsize = 0;		/* also for savecore */
long	dumplo = 0;

void
dumpconf()
{
	int nblks, dumpblks;

	if (dumpdev == NODEV || bdevsw[major(dumpdev)].d_psize == 0)
		/* No usable dump device */
		return;

	nblks = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);

	dumpblks = ctod(physmem) + pmap_dumpsize();
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

#define	BYTES_PER_DUMP	(NBPG)	/* must be a multiple of pagesize */
static vaddr_t dumpspace;

caddr_t
reserve_dumppages(p)
	caddr_t p;
{

	dumpspace = (vaddr_t)p;
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
	register int (*dump)(dev_t, daddr_t, caddr_t, size_t);
	int error = 0;
	register struct mem_region *mp;
	extern struct mem_region *mem;

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
	if (!dumpspace) {
		printf("\nno address space available, dump not possible\n");
		return;
	}
	if (dumplo <= 0) {
		printf("\ndump to dev %u,%u not possible\n", major(dumpdev),
		    minor(dumpdev));
		return;
	}
	printf("\ndumping to dev %u,%u offset %ld\n", major(dumpdev),
	    minor(dumpdev), dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize)(dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable\n");
		return;
	}
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;

	error = pmap_dumpmmu(dump, blkno);
	blkno += pmap_dumpsize();
printf("starting dump, blkno %d\n", blkno);
	for (mp = mem; mp->size; mp++) {
		unsigned i = 0, n;
		paddr_t maddr = mp->start;

#if 0
		/* Remind me: why don't we dump page 0 ? */
		if (maddr == 0) {
			/* Skip first page at physical address 0 */
			maddr += NBPG;
			i += NBPG;
			blkno += btodb(NBPG);
		}
#endif
		for (; i < mp->size; i += n) {
			n = mp->size - i;
			if (n > BYTES_PER_DUMP)
				 n = BYTES_PER_DUMP;

			/* print out how many MBs we have dumped */
			if (i && (i % (1024*1024)) == 0)
				printf("%d ", i / (1024*1024));
			(void) pmap_enter(pmap_kernel(), dumpspace, maddr,
					VM_PROT_READ, VM_PROT_READ|PMAP_WIRED);
			pmap_update(pmap_kernel());
			error = (*dump)(dumpdev, blkno,
					(caddr_t)dumpspace, (int)n);
			pmap_remove(pmap_kernel(), dumpspace, dumpspace + n);
			pmap_update(pmap_kernel());
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

void trapdump(struct trapframe64*);
/*
 * dump out a trapframe.
 */
void
trapdump(tf)
	struct trapframe64* tf;
{
	printf("TRAPFRAME: tstate=%llx pc=%llx npc=%llx y=%x\n",
	       (unsigned long long)tf->tf_tstate, (unsigned long long)tf->tf_pc,
	       (unsigned long long)tf->tf_npc, (unsigned)tf->tf_y);
	printf("%%g1-7: %llx %llx %llx %llx %llx %llx %llx\n",
	       (unsigned long long)tf->tf_global[1],
	       (unsigned long long)tf->tf_global[2],
	       (unsigned long long)tf->tf_global[3], 
	       (unsigned long long)tf->tf_global[4],
	       (unsigned long long)tf->tf_global[5],
	       (unsigned long long)tf->tf_global[6], 
	       (unsigned long long)tf->tf_global[7]);
	printf("%%o0-7: %llx %llx %llx %llx\n %llx %llx %llx %llx\n",
	       (unsigned long long)tf->tf_out[0],
	       (unsigned long long)tf->tf_out[1],
	       (unsigned long long)tf->tf_out[2],
	       (unsigned long long)tf->tf_out[3], 
	       (unsigned long long)tf->tf_out[4],
	       (unsigned long long)tf->tf_out[5],
	       (unsigned long long)tf->tf_out[6],
	       (unsigned long long)tf->tf_out[7]);
}
/*
 * get the fp and dump the stack as best we can.  don't leave the
 * current stack page
 */
void
stackdump()
{
	struct frame32 *fp = (struct frame32 *)getfp(), *sfp;
	struct frame64 *fp64;

	sfp = fp;
	printf("Frame pointer is at %p\n", fp);
	printf("Call traceback:\n");
	while (fp && ((u_long)fp >> PGSHIFT) == ((u_long)sfp >> PGSHIFT)) {
		if( ((long)fp) & 1 ) {
			fp64 = (struct frame64*)(((char *)fp)+BIAS);
			/* 64-bit frame */
			printf("%llx(%llx, %llx, %llx, %llx, %llx, %llx, %llx) fp = %llx\n",
			       (unsigned long long)fp64->fr_pc,
			       (unsigned long long)fp64->fr_arg[0],
			       (unsigned long long)fp64->fr_arg[1],
			       (unsigned long long)fp64->fr_arg[2],
			       (unsigned long long)fp64->fr_arg[3],
			       (unsigned long long)fp64->fr_arg[4],
			       (unsigned long long)fp64->fr_arg[5],	
			       (unsigned long long)fp64->fr_arg[6],
			       (unsigned long long)fp64->fr_fp);
			fp = (struct frame32 *)(u_long)fp64->fr_fp;
		} else {
			/* 32-bit frame */
			printf("  pc = %x  args = (%x, %x, %x, %x, %x, %x, %x) fp = %x\n",
			       fp->fr_pc, fp->fr_arg[0], fp->fr_arg[1], fp->fr_arg[2],
			       fp->fr_arg[3], fp->fr_arg[4], fp->fr_arg[5], fp->fr_arg[6],
			       fp->fr_fp);
			fp = (struct frame32*)(u_long)(u_short)fp->fr_fp;
		}
	}
}


int
cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	return (ENOEXEC);
}

/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
_bus_dmamap_create(t, size, nsegments, maxsegsz, boundary, flags, dmamp)
	bus_dma_tag_t t;
	bus_size_t size;
	int nsegments;
	bus_size_t maxsegsz;
	bus_size_t boundary;
	int flags;
	bus_dmamap_t *dmamp;
{
	struct sparc_bus_dmamap *map;
	void *mapstore;
	size_t mapsize;

	/*
	 * Allocate and initialize the DMA map.  The end of the map
	 * is a variable-sized array of segments, so we allocate enough
	 * room for them in one shot.
	 *
	 * Note we don't preserve the WAITOK or NOWAIT flags.  Preservation
	 * of ALLOCNOW notifies others that we've reserved these resources,
	 * and they are not to be freed.
	 *
	 * The bus_dmamap_t includes one bus_dma_segment_t, hence
	 * the (nsegments - 1).
	 */
	mapsize = sizeof(struct sparc_bus_dmamap) +
	    (sizeof(bus_dma_segment_t) * (nsegments - 1));
	if ((mapstore = malloc(mapsize, M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	bzero(mapstore, mapsize);
	map = (struct sparc_bus_dmamap *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK|BUS_DMA_NOWAIT|BUS_DMA_COHERENT|
				   BUS_DMA_NOWRITE|BUS_DMA_NOCACHE);
	map->dm_mapsize = 0;		/* no valid mappings */
	map->dm_nsegs = 0;

	*dmamp = map;
	return (0);
}

/*
 * Common function for DMA map destruction.  May be called by bus-specific
 * DMA map destruction functions.
 */
void
_bus_dmamap_destroy(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{

	free(map, M_DEVBUF);
}

/*
 * Common function for loading a DMA map with a linear buffer.  May
 * be called by bus-specific DMA map load functions.
 *
 * Most SPARCs have IOMMUs in the bus controllers.  In those cases
 * they only need one segment and will use virtual addresses for DVMA.
 * Those bus controllers should intercept these vectors and should
 * *NEVER* call _bus_dmamap_load() which is used only by devices that
 * bypass DVMA.
 */
int
_bus_dmamap_load(t, map, buf, buflen, p, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	void *buf;
	bus_size_t buflen;
	struct proc *p;
	int flags;
{
	bus_size_t sgsize;
	vaddr_t vaddr = (vaddr_t)buf;
	int i;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
	map->dm_nsegs = 0;

	if (buflen > map->_dm_size)
		return (EFBIG);

	sgsize = round_page(buflen + ((int)vaddr & PGOFSET));

	/*
	 * We always use just one segment.
	 */
	map->dm_mapsize = buflen;
	i = 0;
	map->dm_segs[i].ds_addr = NULL;
	map->dm_segs[i].ds_len = 0;
	while (sgsize > 0 && i < map->_dm_segcnt) {
		paddr_t pa;

		(void) pmap_extract(pmap_kernel(), vaddr, &pa);
		sgsize -= NBPG;
		vaddr += NBPG;
		if (map->dm_segs[i].ds_len == 0)
			map->dm_segs[i].ds_addr = pa;
		if (pa == (map->dm_segs[i].ds_addr + map->dm_segs[i].ds_len)
		    && ((map->dm_segs[i].ds_len + NBPG) < map->_dm_maxsegsz)) {
			/* Hey, waddyaknow, they're contiguous */
			map->dm_segs[i].ds_len += NBPG;
			continue;
		}
		map->dm_segs[++i].ds_addr = pa;
		map->dm_segs[i].ds_len = NBPG;
	}
	map->dm_nsegs = i;
	/* Mapping is bus dependent */
	return (0);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
_bus_dmamap_load_mbuf(t, map, m, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{
	bus_dma_segment_t segs[MAX_DMA_SEGS];
	int i;
	size_t len;

	/* Record mbuf for *_unload */
	map->_dm_type = _DM_TYPE_MBUF;
	map->_dm_source = (void *)m;

	i = 0;
	len = 0;
	while (m) {
		vaddr_t vaddr = mtod(m, vaddr_t);
		long buflen = (long)m->m_len;

		len += buflen;
		while (buflen > 0 && i < MAX_DMA_SEGS) {
			paddr_t pa;
			long incr;

			incr = min(buflen, NBPG);
			(void) pmap_extract(pmap_kernel(), vaddr, &pa);
			buflen -= incr;
			vaddr += incr;

			if (i > 0 && pa == (segs[i-1].ds_addr + segs[i-1].ds_len)
			    && ((segs[i-1].ds_len + incr) < map->_dm_maxsegsz)) {
				/* Hey, waddyaknow, they're contiguous */
				segs[i-1].ds_len += incr;
				continue;
			}
			segs[i].ds_addr = pa;
			segs[i].ds_len = incr;
			segs[i]._ds_boundary = 0;
			segs[i]._ds_align = 0;
			segs[i]._ds_mlist = NULL;
			i++;
		}
		m = m->m_next;
		if (m && i >= MAX_DMA_SEGS) {
			/* Exceeded the size of our dmamap */
			map->_dm_type = 0;
			map->_dm_source = NULL;
			return (EFBIG);
		}
	}

	return (bus_dmamap_load_raw(t, map, segs, i,
			    (bus_size_t)len, flags));
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
_bus_dmamap_load_uio(t, map, uio, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	struct uio *uio;
	int flags;
{
	/*
	 * XXXXXXX The problem with this routine is that it needs to 
	 * lock the user address space that is being loaded, but there
	 * is no real way for us to unlock it during the unload process.
	 * As a result, only UIO_SYSSPACE uio's are allowed for now.
	 */
	bus_dma_segment_t segs[MAX_DMA_SEGS];
	int i, j;
	size_t len;

	if (uio->uio_segflg != UIO_SYSSPACE)
		return (EOPNOTSUPP);

	/* Record for *_unload */
	map->_dm_type = _DM_TYPE_UIO;
	map->_dm_source = (void *)uio;

	i = j = 0;
	len = 0;
	while (j < uio->uio_iovcnt) {
		vaddr_t vaddr = (vaddr_t)uio->uio_iov[j].iov_base;
		long buflen = (long)uio->uio_iov[j].iov_len;

		len += buflen;
		while (buflen > 0 && i < MAX_DMA_SEGS) {
			paddr_t pa;
			long incr;

			incr = min(buflen, NBPG);
			(void) pmap_extract(pmap_kernel(), vaddr, &pa);
			buflen -= incr;
			vaddr += incr;

			if (i > 0 && pa == (segs[i-1].ds_addr + segs[i-1].ds_len)
			    && ((segs[i-1].ds_len + incr) < map->_dm_maxsegsz)) {
				/* Hey, waddyaknow, they're contiguous */
				segs[i-1].ds_len += incr;
				continue;
			}
			segs[i].ds_addr = pa;
			segs[i].ds_len = incr;
			segs[i]._ds_boundary = 0;
			segs[i]._ds_align = 0;
			segs[i]._ds_mlist = NULL;
			i++;
		}
		j++;
		if ((uio->uio_iovcnt - j) && i >= MAX_DMA_SEGS) {
			/* Exceeded the size of our dmamap */
			map->_dm_type = 0;
			map->_dm_source = NULL;
			return (EFBIG);
		}
	}

	return (bus_dmamap_load_raw(t, map, segs, i, (bus_size_t)len, flags));
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
_bus_dmamap_load_raw(t, map, segs, nsegs, size, flags)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_dma_segment_t *segs;
	int nsegs;
	bus_size_t size;
	int flags;
{

	panic("_bus_dmamap_load_raw: not implemented");
}

/*
 * Common function for unloading a DMA map.  May be called by
 * bus-specific DMA map unload functions.
 */
void
_bus_dmamap_unload(t, map)
	bus_dma_tag_t t;
	bus_dmamap_t map;
{
	int i;
	struct vm_page *m;
	struct pglist *mlist;
	paddr_t pa;

	for (i=0; i<map->dm_nsegs; i++) {
		if ((mlist = map->dm_segs[i]._ds_mlist) == NULL) {
			/* 
			 * We were asked to load random VAs and lost the 
			 * PA info so just blow the entire cache away.
			 */
			blast_vcache();
			break;
		}
		for (m = TAILQ_FIRST(mlist); m != NULL;
		     m = TAILQ_NEXT(m,pageq)) {
			pa = VM_PAGE_TO_PHYS(m);
			/* 
			 * We should be flushing a subrange, but we
			 * don't know where the segments starts.
			 */
			dcache_flush_page(pa);
		}
	}
	/* Mark the mappings as invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
void
_bus_dmamap_sync(t, map, offset, len, ops)
	bus_dma_tag_t t;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{
	int i;
	struct vm_page *m;
	struct pglist *mlist;

	/*
	 * We sync out our caches, but the bus must do the same.
	 *
	 * Actually a #Sync is expensive.  We should optimize.
	 */
	if ((ops & BUS_DMASYNC_PREREAD) || (ops & BUS_DMASYNC_PREWRITE)) {
		/* 
		 * Don't really need to do anything, but flush any pending
		 * writes anyway. 
		 */
		__asm("membar #Sync" : );
	}
	if (ops & BUS_DMASYNC_POSTREAD) {
		/* Invalidate the vcache */
		for (i=0; i<map->dm_nsegs; i++) {
			if ((mlist = map->dm_segs[i]._ds_mlist) == NULL)
				/* Should not really happen. */
				continue;
			for (m = TAILQ_FIRST(mlist);
			     m != NULL; m = TAILQ_NEXT(m,pageq)) {
				paddr_t start;
				psize_t size = NBPG;

				if (offset < NBPG) {
					start = VM_PAGE_TO_PHYS(m) + offset;
					size = NBPG;
					if (size > len)
						size = len;
					cache_flush_phys(start, size, 0);
					len -= size;
					continue;
				}
				offset -= size;
			}
		}
	}
	if (ops & BUS_DMASYNC_POSTWRITE) {
		/* Nothing to do.  Handled by the bus controller. */
	}
}

extern paddr_t   vm_first_phys, vm_num_phys;
/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
int
_bus_dmamem_alloc(t, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t;
	bus_size_t size, alignment, boundary;
	bus_dma_segment_t *segs;
	int nsegs;
	int *rsegs;
	int flags;
{
	vaddr_t low, high;
	struct pglist *mlist;
	int error;

	/* Always round the size. */
	size = round_page(size);
	low = vm_first_phys;
	high = vm_first_phys + vm_num_phys - PAGE_SIZE;

	if ((mlist = malloc(sizeof(*mlist), M_DEVBUF,
	    (flags & BUS_DMA_NOWAIT) ? M_NOWAIT : M_WAITOK)) == NULL)
		return (ENOMEM);

	/*
	 * If the bus uses DVMA then ignore boundary and alignment.
	 */
	segs[0]._ds_boundary = boundary;
	segs[0]._ds_align = alignment;
	if (flags & BUS_DMA_DVMA) {
		boundary = 0;
		alignment = 0;
	}

	/*
	 * Allocate pages from the VM system.
	 */
	TAILQ_INIT(mlist);
	error = uvm_pglistalloc(size, low, high,
	    alignment, boundary, mlist, nsegs, (flags & BUS_DMA_NOWAIT) == 0);
	if (error)
		return (error);

	/*
	 * Compute the location, size, and number of segments actually
	 * returned by the VM code.
	 */
	segs[0].ds_addr = NULL; /* UPA does not map things */
	segs[0].ds_len = size;
	*rsegs = 1;

	/*
	 * Simply keep a pointer around to the linked list, so
	 * bus_dmamap_free() can return it.
	 *
	 * NOBODY SHOULD TOUCH THE pageq FIELDS WHILE THESE PAGES
	 * ARE IN OUR CUSTODY.
	 */
	segs[0]._ds_mlist = mlist;

	/* The bus driver should do the actual mapping */
	return (0);
}

/*
 * Common function for freeing DMA-safe memory.  May be called by
 * bus-specific DMA memory free functions.
 */
void
_bus_dmamem_free(t, segs, nsegs)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
{

	if (nsegs != 1)
		panic("bus_dmamem_free: nsegs = %d", nsegs);

	/*
	 * Return the list of pages back to the VM system.
	 */
	uvm_pglistfree(segs[0]._ds_mlist);
	free(segs[0]._ds_mlist, M_DEVBUF);
}

/*
 * Common function for mapping DMA-safe memory.  May be called by
 * bus-specific DMA memory map functions.
 */
int
_bus_dmamem_map(t, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vaddr_t va, sva;
	struct pglist *mlist;
	int r, cbit;
	size_t oversize;
	u_long align;

	if (nsegs != 1)
		panic("_bus_dmamem_map: nsegs = %d", nsegs);

	cbit = PMAP_NC;
	align = PAGE_SIZE;

	size = round_page(size);

	/*
	 * Find a region of kernel virtual addresses that can accommodate
	 * our aligment requirements.
	 */
	oversize = size + align - PAGE_SIZE;
	r = uvm_map(kernel_map, &sva, oversize, NULL, UVM_UNKNOWN_OFFSET, 0,
	    UVM_MAPFLAG(UVM_PROT_NONE, UVM_PROT_NONE, UVM_INH_NONE,
	    UVM_ADV_NORMAL, 0));
	if (r != 0)
		return (ENOMEM);

	/* Compute start of aligned region */
	va = sva;
	va += ((segs[0].ds_addr & (align - 1)) + align - va) & (align - 1);

	/* Return excess virtual addresses */
	if (va != sva)
		(void)uvm_unmap(kernel_map, sva, va);
	if (va + size != sva + oversize)
		(void)uvm_unmap(kernel_map, va + size, sva + oversize);


	*kvap = (caddr_t)va;
	mlist = segs[0]._ds_mlist;

	return (0);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
_bus_dmamem_unmap(t, kva, size)
	bus_dma_tag_t t;
	caddr_t kva;
	size_t size;
{

#ifdef DIAGNOSTIC
	if ((u_long)kva & PAGE_MASK)
		panic("_bus_dmamem_unmap");
#endif

	size = round_page(size);
	uvm_unmap(kernel_map, (vaddr_t)kva, (vaddr_t)kva + size);
}

/*
 * Common functin for mmap(2)'ing DMA-safe memory.  May be called by
 * bus-specific DMA mmap(2)'ing functions.
 */
paddr_t
_bus_dmamem_mmap(t, segs, nsegs, off, prot, flags)
	bus_dma_tag_t t;
	bus_dma_segment_t *segs;
	int nsegs;
	off_t off;
	int prot, flags;
{

	panic("_bus_dmamem_mmap: not implemented");
}


struct sparc_bus_dma_tag mainbus_dma_tag = {
	NULL,
	NULL,
	_bus_dmamap_create,
	_bus_dmamap_destroy,
	_bus_dmamap_load,
	_bus_dmamap_load_mbuf,
	_bus_dmamap_load_uio,
	_bus_dmamap_load_raw,
	_bus_dmamap_unload,
	_bus_dmamap_sync,

	_bus_dmamem_alloc,
	_bus_dmamem_free,
	_bus_dmamem_map,
	_bus_dmamem_unmap,
	_bus_dmamem_mmap
};


/*
 * Base bus space handlers.
 */
static int	sparc_bus_map( bus_space_tag_t, bus_type_t, bus_addr_t,
				    bus_size_t, int, vaddr_t,
				    bus_space_handle_t *);
static int	sparc_bus_unmap(bus_space_tag_t, bus_space_handle_t,
				     bus_size_t);
static int	sparc_bus_subregion(bus_space_tag_t, bus_space_handle_t,
					 bus_size_t, bus_size_t,
					 bus_space_handle_t *);
static paddr_t	sparc_bus_mmap(bus_space_tag_t, bus_addr_t, off_t, int, int);
static void	*sparc_mainbus_intr_establish(bus_space_tag_t, int, int,
						   int, int (*)(void *),
						   void *);
static void	sparc_bus_barrier(bus_space_tag_t, bus_space_handle_t,
					  bus_size_t, bus_size_t, int);
static int	sparc_bus_alloc(bus_space_tag_t, bus_addr_t, bus_addr_t,
					bus_size_t, bus_size_t, bus_size_t, int,
					bus_addr_t *, bus_space_handle_t *);
static void	sparc_bus_free(bus_space_tag_t, bus_space_handle_t,
				       bus_size_t);

vaddr_t iobase = IODEV_BASE;
struct extent *io_space = NULL;

int
sparc_bus_map(t, iospace, addr, size, flags, vaddr, hp)
	bus_space_tag_t t;
	bus_type_t	iospace;
	bus_addr_t	addr;
	bus_size_t	size;
	vaddr_t	vaddr;
	bus_space_handle_t *hp;
{
	vaddr_t v;
	u_int64_t pa;
	paddr_t	pm_flags = 0;
	vm_prot_t pm_prot = VM_PROT_READ;

	t->type = iospace;
	if (iobase == NULL)
		iobase = IODEV_BASE;
	if (io_space == NULL)
		/*
		 * And set up IOSPACE extents.
		 */
		io_space = extent_create("IOSPACE",
					 (u_long)IODEV_BASE, (u_long)IODEV_END,
					 M_DEVBUF, 0, 0, EX_NOWAIT);


	size = round_page(size);
	if (size == 0) {
		printf("sparc_bus_map: zero size\n");
		return (EINVAL);
	}
	switch (iospace) {
	case PCI_CONFIG_BUS_SPACE:
		/* 
		 * PCI config space is special.
		 *
		 * It's really big and seldom used.  In order not to run
		 * out of IO mappings, config space will not be mapped in,
		 * rather it will be accessed through MMU bypass ASI accesses.
		 */
		if (flags & BUS_SPACE_MAP_LINEAR) return (-1);
		*hp = (bus_space_handle_t)addr;
		if (!vaddr) return (0);
		/* FALLTHROUGH */
	case PCI_IO_BUS_SPACE:
		pm_flags = PMAP_LITTLE;
		break;
	case PCI_MEMORY_BUS_SPACE:
		pm_flags = PMAP_LITTLE;
		break;
	default:
		pm_flags = 0;
		break;
	}

	if (!(flags & BUS_SPACE_MAP_CACHEABLE)) pm_flags |= PMAP_NC;

	if (vaddr)
		v = trunc_page(vaddr);
	else {
		int err;
		if ((err = extent_alloc(io_space, size, NBPG,
					0, 0, EX_NOWAIT|EX_BOUNDZERO, 
					(u_long *)&v)))
			panic("sparc_bus_map: cannot allocate io_space: %d\n", err);
	}

	/* note: preserve page offset */
	*hp = (bus_space_handle_t)(v | ((u_long)addr & PGOFSET));

	pa = addr & ~PAGE_MASK; /* = trunc_page(addr); Will drop high bits */
	if (!(flags&BUS_SPACE_MAP_READONLY)) pm_prot |= VM_PROT_WRITE;

	DPRINTF(BSDB_MAP, ("\nsparc_bus_map: type %x flags %x "
		"addr %016llx size %016llx virt %llx paddr %016llx\n",
		(int)iospace, (int) flags, (unsigned long long)addr,
		(unsigned long long)size, (unsigned long long)*hp,
		(unsigned long long)pa));

	do {
		DPRINTF(BSDB_MAP, ("sparc_bus_map: phys %llx virt %p hp %llx\n", 
			(unsigned long long)pa, (char *)v,
			(unsigned long long)*hp));
		pmap_enter(pmap_kernel(), v, pa | pm_flags, pm_prot,
			pm_prot|PMAP_WIRED);
		v += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((size -= PAGE_SIZE) > 0);
	pmap_update(pmap_kernel());
	return (0);
}

int
sparc_bus_subregion(tag, handle, offset, size, nhandlep)
	bus_space_tag_t		tag;
	bus_space_handle_t	handle;
	bus_size_t		offset;
	bus_size_t		size;
	bus_space_handle_t	*nhandlep;
{
	*nhandlep = handle + offset;
	return (0);
}

int
sparc_bus_unmap(t, bh, size)
	bus_space_tag_t t;
	bus_size_t	size;
	bus_space_handle_t bh;
{
	vaddr_t va = trunc_page((vaddr_t)bh);
	vaddr_t endva = va + round_page(size);

	int error = extent_free(io_space, va, size, EX_NOWAIT);
	if (error) printf("sparc_bus_unmap: extent free says %d\n", error);

	pmap_remove(pmap_kernel(), va, endva);
	return (0);
}

paddr_t
sparc_bus_mmap(t, paddr, off, prot, flags)
	bus_space_tag_t t;
	bus_addr_t paddr;
	off_t off;
	int prot;
	int flags;
{
	/* Devices are un-cached... although the driver should do that */
	return ((paddr+off)|PMAP_NC);
}

/*
 * Establish a temporary bus mapping for device probing.  */
int
bus_space_probe(tag, btype, paddr, size, offset, flags, callback, arg)
	bus_space_tag_t tag;
	bus_type_t	btype;
	bus_addr_t	paddr;
	bus_size_t	size;
	size_t		offset;
	int		flags;
	int		(*callback)(void *, void *);
	void		*arg;
{
	bus_space_handle_t bh;
	paddr_t tmp;
	int result;

	if (bus_space_map2(tag, btype, paddr, size, flags, NULL, &bh) != 0)
		return (0);

	tmp = (paddr_t)bh;
	result = (probeget(tmp + offset, bus_type_asi[tag->type], size) != -1);
	if (result && callback != NULL)
		result = (*callback)((char *)(u_long)tmp, arg);
	bus_space_unmap(tag, bh, size);
	return (result);
}


void *
sparc_mainbus_intr_establish(t, pil, level, flags, handler, arg)
	bus_space_tag_t t;
	int	pil;
	int	level;
	int	flags;
	int	(*handler)(void *);
	void	*arg;
{
	struct intrhand *ih;

	ih = (struct intrhand *)
		malloc(sizeof(struct intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (NULL);

	ih->ih_fun = handler;
	ih->ih_arg = arg;
	intr_establish(pil, ih);
	return (ih);
}

void
sparc_bus_barrier (t, h, offset, size, flags)
	bus_space_tag_t	t;
	bus_space_handle_t h;
	bus_size_t	offset;
	bus_size_t	size;
	int		flags;
{
	/* 
	 * We have lots of alternatives depending on whether we're
	 * synchronizing loads with loads, loads with stores, stores
	 * with loads, or stores with stores.  The only ones that seem
	 * generic are #Sync and #MemIssue.  I'll use #Sync for safety.
	 */
	if (flags == (BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE))
		__asm("membar #Sync" : );
	else if (flags == BUS_SPACE_BARRIER_READ)
		__asm("membar #Sync" : );
	else if (flags == BUS_SPACE_BARRIER_WRITE)
		__asm("membar #Sync" : );
	else
		printf("sparc_bus_barrier: unknown flags\n");
	return;
}

int
sparc_bus_alloc(t, rs, re, s, a, b, f, ap, hp)
	bus_space_tag_t	t;
	bus_addr_t	rs;
	bus_addr_t	re;
	bus_size_t	s;
	bus_size_t	a;
	bus_size_t	b;
	int		f;
	bus_addr_t	*ap;
	bus_space_handle_t *hp;
{
	return (ENOTTY);
}

void
sparc_bus_free(t, h, s)
	bus_space_tag_t	t;
	bus_space_handle_t	h;
	bus_size_t	s;
{
	return;
}


struct sparc_bus_space_tag mainbus_space_tag = {
	NULL,				/* cookie */
	NULL,				/* parent bus tag */
	UPA_BUS_SPACE,			/* type */
	sparc_bus_alloc,
	sparc_bus_free,
	sparc_bus_map,			/* bus_space_map */
	sparc_bus_unmap,		/* bus_space_unmap */
	sparc_bus_subregion,		/* bus_space_subregion */
	sparc_bus_barrier,		/* bus_space_barrier */
	sparc_bus_mmap,			/* bus_space_mmap */
	sparc_mainbus_intr_establish	/* bus_intr_establish */
};

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};
