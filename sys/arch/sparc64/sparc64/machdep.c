/*	$OpenBSD: machdep.c,v 1.97 2007/10/02 00:59:12 krw Exp $	*/
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
 *	@(#)machdep.c	8.6 (Berkeley) 1/14/94
 */

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
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/mount.h>
#include <sys/msgbuf.h>
#include <sys/syscallargs.h>
#include <sys/exec.h>

#include <uvm/uvm.h>

#include <sys/sysctl.h>
#include <sys/exec_elf.h>
#include <dev/rndvar.h>

#ifdef SYSVMSG
#include <sys/msg.h>
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

int     _bus_dmamap_create(bus_dma_tag_t, bus_dma_tag_t, bus_size_t, int,
	    bus_size_t, bus_size_t, int, bus_dmamap_t *);
void    _bus_dmamap_destroy(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t);
int     _bus_dmamap_load(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t, void *,
            bus_size_t, struct proc *, int);
int     _bus_dmamap_load_mbuf(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
            struct mbuf *, int);
int     _bus_dmamap_load_uio(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
            struct uio *, int);
int     _bus_dmamap_load_raw(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
            bus_dma_segment_t *, int, bus_size_t, int);
void    _bus_dmamap_unload(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t);
void    _bus_dmamap_sync(bus_dma_tag_t, bus_dma_tag_t, bus_dmamap_t,
	    bus_addr_t, bus_size_t, int);

int     _bus_dmamem_alloc(bus_dma_tag_t, bus_dma_tag_t tag, bus_size_t size,
            bus_size_t alignment, bus_size_t boundary,
            bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags);

void    _bus_dmamem_free(bus_dma_tag_t tag, bus_dma_tag_t,
	    bus_dma_segment_t *segs, int nsegs);
int     _bus_dmamem_map(bus_dma_tag_t tag, bus_dma_tag_t,
	    bus_dma_segment_t *segs, int nsegs, size_t size, caddr_t *kvap,
	    int flags);
void    _bus_dmamem_unmap(bus_dma_tag_t tag, bus_dma_tag_t, caddr_t kva,
            size_t size);
paddr_t _bus_dmamem_mmap(bus_dma_tag_t tag, bus_dma_tag_t,
	    bus_dma_segment_t *segs, int nsegs, off_t off, int prot, int flags);

int     _bus_dmamem_alloc_range(bus_dma_tag_t tag, bus_dma_tag_t,
	    bus_size_t size, bus_size_t alignment, bus_size_t boundary,
            bus_dma_segment_t *segs, int nsegs, int *rsegs, int flags,
            vaddr_t low, vaddr_t high);

/*
 * The "bus_space_debug" flags used by macros elsewhere.
 * A good set of flags to use when first debugging something is:
 * int bus_space_debug = BSDB_ACCESS | BSDB_ASSERT | BSDB_MAP;
 */
int bus_space_debug = 0;

struct vm_map *exec_map = NULL;
extern vaddr_t avail_end;

/*
 * Declare these as initialized data so we can patch them.
 */
#ifndef BUFCACHEPERCENT
#define BUFCACHEPERCENT 10
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

int sparc_led_blink;
int kbd_reset;

#ifdef APERTURE
#ifdef INSECURE
int allowaperture = 1;
#else
int allowaperture = 0;
#endif
#endif

extern int ceccerrs;
extern int64_t cecclast;

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

void blink_led_timeout(void *);
caddr_t	allocsys(caddr_t);
void	dumpsys(void);
void	stackdump(void);

/*
 * Machine-dependent startup code
 */
void
cpu_startup()
{
	caddr_t v;
	long sz;
#ifdef DEBUG
	extern int pmapdebug;
	int opmapdebug = pmapdebug;
#endif
	vaddr_t minaddr, maxaddr;
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
	printf("real mem = %lu (%luMB)\n", ptoa((psize_t)physmem),
	    ptoa((psize_t)physmem)/1024/1024);
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
	 * Determine how many buffers to allocate.
	 * We allocate bufcachepercent% of memory for buffer space.
	 */
	if (bufpages == 0)
		bufpages = physmem * bufcachepercent / 100;

	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	minaddr = vm_map_min(kernel_map);
	exec_map = uvm_km_suballoc(kernel_map, &minaddr, &maxaddr,
	    16*NCARGS, VM_MAP_PAGEABLE, FALSE, NULL);

#ifdef DEBUG
	pmapdebug = opmapdebug;
#endif
	printf("avail mem = %lu (%luMB)\n", ptoa((psize_t)uvmexp.free),
	    ptoa((psize_t)uvmexp.free)/1024/1024);

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
#ifdef SYSVMSG
	valloc(msgpool, char, msginfo.msgmax);
	valloc(msgmaps, struct msgmap, msginfo.msgseg);
	valloc(msghdrs, struct msg, msginfo.msgtql);
	valloc(msqids, struct msqid_ds, msginfo.msgmni);
#endif

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

	/* 
	 * Setup the process StackGhost cookie which will be XORed into
	 * the return pointer as register windows are over/underflowed.
	 */
	p->p_addr->u_pcb.pcb_wcookie = ((u_int64_t)arc4random() << 32) |
	    arc4random();

	/* The cookie needs to guarantee invalid alignment after the XOR. */
	switch (p->p_addr->u_pcb.pcb_wcookie % 3) {
	case 0: /* Two lsb's already both set except if the cookie is 0. */
		p->p_addr->u_pcb.pcb_wcookie |= 0x3;
		break;
	case 1: /* Set the lsb. */
		p->p_addr->u_pcb.pcb_wcookie = 1 |
		    (p->p_addr->u_pcb.pcb_wcookie & ~0x3);
		break;
	case 2: /* Set the second most lsb. */
		p->p_addr->u_pcb.pcb_wcookie = 2 |
		    (p->p_addr->u_pcb.pcb_wcookie & ~0x3);
		break;
	}

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
	int oldval, ret;

	/* all sysctl names are this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	case CPU_LED_BLINK:
		oldval = sparc_led_blink;
		ret = sysctl_int(oldp, oldlenp, newp, newlen,
		    &sparc_led_blink);
		/*
		 * If we were false and are now true, call start the timer.
		 */
		if (!oldval && sparc_led_blink > oldval)
			blink_led_timeout(NULL);
		return (ret);
	case CPU_ALLOWAPERTURE:
#ifdef APERTURE
		if (securelevel > 0)
			return (sysctl_int_lower(oldp, oldlenp, newp, newlen,
			    &allowaperture));
		else
			return (sysctl_int(oldp, oldlenp, newp, newlen,
			    &allowaperture));
#else
		return (sysctl_rdint(oldp, oldlenp, newp, 0));
#endif
	case CPU_CPUTYPE:
		return (sysctl_rdint(oldp, oldlenp, newp, CPU_SUN4U));
	case CPU_CECCERRORS:
		return (sysctl_rdint(oldp, oldlenp, newp, ceccerrs));
	case CPU_CECCLAST:
		return (sysctl_rdquad(oldp, oldlenp, newp, cecclast));
	case CPU_KBDRESET:
		if (securelevel > 0)
			return (sysctl_rdint(oldp, oldlenp, newp, kbd_reset));
		return (sysctl_int(oldp, oldlenp, newp, newlen, &kbd_reset));
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
		printf("sendsig: stack was trashed trying to send sig %d, "
		    "sending SIGILL\n", sig);
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
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext sc, *scp;
	struct trapframe64 *tf;
	int error = EINVAL;

	/* First ensure consistent stack state (see sendsig). */
	write_user_windows();

	if (rwindow_save(p)) {
#ifdef DEBUG
		printf("sigreturn: rwindow_save(%p) failed, sending SIGILL\n",
		    p);
#endif
		sigexit(p, SIGILL);
	}
	scp = SCARG(uap, sigcntxp);
 	if ((vaddr_t)scp & 3 ||
	    (error = copyin((caddr_t)scp, &sc, sizeof sc)) != 0) {
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

/*
 * Notify the current process (p) that it has a signal pending,
 * process as soon as possible.
 */
void
signotify(struct proc *p)
{
	aston(p);
#ifdef MULTIPROCESSOR
	/* XXX Send IPI if necessary. */
#endif
}

int	waittime = -1;
struct pcb dumppcb;

void
boot(howto)
	int howto;
{
	int i;
	static char str[128];

	/* If system is cold, just halt. */
	if (cold) {
		/* (Unless the user explicitly asked for reboot.) */
		if ((howto & RB_USERREQ) == 0)
			howto |= RB_HALT;
		goto haltsys;
	}

	fb_unblank();
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
dumpconf(void)
{
	int nblks, dumpblks;

	if (dumpdev == NODEV ||
	    (nblks = (bdevsw[major(dumpdev)].d_psize)(dumpdev)) == 0)
		return;
	if (nblks <= ctod(1))
		return;

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
	int psize;
	daddr64_t blkno;
	int (*dump)(dev_t, daddr64_t, caddr_t, size_t);
	int error = 0;
	struct mem_region *mp;
	extern struct mem_region *mem;

	/* copy registers to memory */
	snapshot(&dumppcb);
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
printf("starting dump, blkno %lld\n", blkno);
	for (mp = mem; mp->size; mp++) {
		u_int64_t i = 0, n;
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
			printf("%llx(%llx, %llx, %llx, %llx, %llx, %llx, %llx) "
			    "fp = %llx\n",
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
			printf("  pc = %x  args = (%x, %x, %x, %x, %x, %x, %x) "
			    "fp = %x\n", fp->fr_pc, fp->fr_arg[0],
			    fp->fr_arg[1], fp->fr_arg[2], fp->fr_arg[3],
			    fp->fr_arg[4], fp->fr_arg[5], fp->fr_arg[6],
			    fp->fr_fp);
			fp = (struct frame32*)(u_long)(u_short)fp->fr_fp;
		}
	}
}


/*
 * Common function for DMA map creation.  May be called by bus-specific
 * DMA map creation functions.
 */
int
_bus_dmamap_create(t, t0, size, nsegments, maxsegsz, boundary, flags, dmamp)
	bus_dma_tag_t t, t0;
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
	if ((mapstore = malloc(mapsize, M_DEVBUF, (flags & BUS_DMA_NOWAIT) ?
	    (M_NOWAIT | M_ZERO) : (M_WAITOK | M_ZERO))) == NULL)
		return (ENOMEM);

	map = (struct sparc_bus_dmamap *)mapstore;
	map->_dm_size = size;
	map->_dm_segcnt = nsegments;
	map->_dm_maxsegsz = maxsegsz;
	map->_dm_boundary = boundary;
	map->_dm_flags = flags & ~(BUS_DMA_WAITOK | BUS_DMA_NOWAIT |
	    BUS_DMA_COHERENT | BUS_DMA_NOWRITE | BUS_DMA_NOCACHE);
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
_bus_dmamap_destroy(t, t0, map)
	bus_dma_tag_t t, t0;
	bus_dmamap_t map;

{
	/*
	 * Unload the map if it is still loaded.  This is required
	 * by the specification (well, the manpage).  Higher level
	 * drivers, if any, should do this too.  By the time the
	 * system gets here, the higher level "destroy" functions
	 * would probably already have clobbered the data needed
	 * to do a proper unload.
	 */
	if (map->dm_nsegs)
		bus_dmamap_unload(t0, map);

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
_bus_dmamap_load(t, t0, map, buf, buflen, p, flags)
	bus_dma_tag_t t, t0;
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
	/* Is this what the above comment calls "one segment"? */
	map->dm_nsegs = i;

	/* Mapping is bus dependent */
	return (0);
}

/*
 * Like _bus_dmamap_load(), but for mbufs.
 */
int
_bus_dmamap_load_mbuf(t, t0, map, m, flags)
	bus_dma_tag_t t, t0;
	bus_dmamap_t map;
	struct mbuf *m;
	int flags;
{
	bus_dma_segment_t segs[MAX_DMA_SEGS];
	int i;
	size_t len;

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
 	map->dm_mapsize = 0;
 	map->dm_nsegs = 0;

	if (m->m_pkthdr.len > map->_dm_size)
		return (EINVAL);

	/* Record mbuf for *_unload */
	map->_dm_type = _DM_TYPE_MBUF;
	map->_dm_source = m;

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

			if (pmap_extract(pmap_kernel(), vaddr, &pa) == FALSE) {
#ifdef DIAGNOSTIC
				printf("_bus_dmamap_load_mbuf: pmap_extract failed %lx\n",
					vaddr);
				map->_dm_type = 0;
				map->_dm_source = NULL;
#endif
				return EINVAL;
			}

			buflen -= incr;
			vaddr += incr;

			if (i > 0 && pa == (segs[i - 1].ds_addr +
			    segs[i - 1].ds_len) && ((segs[i - 1].ds_len + incr)
			    < map->_dm_maxsegsz)) {
				/* Hey, waddyaknow, they're contiguous */
				segs[i - 1].ds_len += incr;
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

	return (bus_dmamap_load_raw(t0, map, segs, i,
			    (bus_size_t)len, flags));
}

/*
 * Like _bus_dmamap_load(), but for uios.
 */
int
_bus_dmamap_load_uio(t, t0, map, uio, flags)
	bus_dma_tag_t t, t0;
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

	/*
	 * Make sure that on error condition we return "no valid mappings".
	 */
 	map->dm_mapsize = 0;
 	map->dm_nsegs = 0;

	if (uio->uio_resid > map->_dm_size)
		return (EINVAL);

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

			if (i > 0 && pa == (segs[i - 1].ds_addr +
			    segs[i - 1].ds_len) && ((segs[i - 1].ds_len + incr)
			    < map->_dm_maxsegsz)) {
				/* Hey, waddyaknow, they're contiguous */
				segs[i - 1].ds_len += incr;
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

	return (bus_dmamap_load_raw(t0, map, segs, i, (bus_size_t)len, flags));
}

/*
 * Like _bus_dmamap_load(), but for raw memory allocated with
 * bus_dmamem_alloc().
 */
int
_bus_dmamap_load_raw(t, t0, map, segs, nsegs, size, flags)
	bus_dma_tag_t t, t0;
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
_bus_dmamap_unload(t, t0, map)
	bus_dma_tag_t t, t0;
	bus_dmamap_t map;
{
	/* Mark the mappings as invalid. */
	map->dm_mapsize = 0;
	map->dm_nsegs = 0;

}

/*
 * Common function for DMA map synchronization.  May be called
 * by bus-specific DMA map synchronization functions.
 */
void
_bus_dmamap_sync(t, t0, map, offset, len, ops)
	bus_dma_tag_t t, t0;
	bus_dmamap_t map;
	bus_addr_t offset;
	bus_size_t len;
	int ops;
{
	if (ops & (BUS_DMASYNC_PREWRITE | BUS_DMASYNC_POSTREAD))
		membar(MemIssue);
}

extern paddr_t   vm_first_phys, vm_num_phys;
/*
 * Common function for DMA-safe memory allocation.  May be called
 * by bus-specific DMA memory allocation functions.
 */
int
_bus_dmamem_alloc(t, t0, size, alignment, boundary, segs, nsegs, rsegs, flags)
	bus_dma_tag_t t, t0;
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
_bus_dmamem_free(t, t0, segs, nsegs)
	bus_dma_tag_t t, t0;
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
_bus_dmamem_map(t, t0, segs, nsegs, size, kvap, flags)
	bus_dma_tag_t t, t0;
	bus_dma_segment_t *segs;
	int nsegs;
	size_t size;
	caddr_t *kvap;
	int flags;
{
	vaddr_t va, sva;
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
		uvm_unmap(kernel_map, sva, va);
	if (va + size != sva + oversize)
		uvm_unmap(kernel_map, va + size, sva + oversize);


	*kvap = (caddr_t)va;

	return (0);
}

/*
 * Common function for unmapping DMA-safe memory.  May be called by
 * bus-specific DMA memory unmapping functions.
 */
void
_bus_dmamem_unmap(t, t0, kva, size)
	bus_dma_tag_t t, t0;
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
 * Common function for mmap(2)'ing DMA-safe memory.  May be called by
 * bus-specific DMA mmap(2)'ing functions.
 */
paddr_t
_bus_dmamem_mmap(t, t0, segs, nsegs, off, prot, flags)
	bus_dma_tag_t t, t0;
	bus_dma_segment_t *segs;
	int nsegs;
	off_t off;
	int prot, flags;
{
	int i;

	for (i = 0; i < nsegs; i++) {
#ifdef DIAGNOSTIC
		if (off & PGOFSET)
			panic("_bus_dmamem_mmap: offset unaligned");
		if (segs[i].ds_addr & PGOFSET)
			panic("_bus_dmamem_mmap: segment unaligned");
		if (segs[i].ds_len & PGOFSET)
			panic("_bus_dmamem_mmap: segment size not multiple"
					" of page size");
#endif
		if (off >= segs[i].ds_len) {
			off -= segs[i].ds_len;
			continue;
		}

		return (atop(segs[i].ds_addr + off));
	}

	/* Page not found. */
	return (-1);
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
int sparc_bus_map(bus_space_tag_t, bus_space_tag_t, bus_addr_t, bus_size_t,
    int, bus_space_handle_t *);
int sparc_bus_protect(bus_space_tag_t, bus_space_tag_t, bus_space_handle_t,
    bus_size_t, int);
int sparc_bus_unmap(bus_space_tag_t, bus_space_tag_t, bus_space_handle_t,
    bus_size_t);
bus_addr_t sparc_bus_addr(bus_space_tag_t, bus_space_tag_t,
    bus_space_handle_t);
int sparc_bus_subregion(bus_space_tag_t, bus_space_tag_t,  bus_space_handle_t,
    bus_size_t, bus_size_t, bus_space_handle_t *);
paddr_t sparc_bus_mmap(bus_space_tag_t, bus_space_tag_t, bus_addr_t, off_t,
    int, int);
void *sparc_mainbus_intr_establish(bus_space_tag_t, bus_space_tag_t, int, int,
    int, int (*)(void *), void *, const char *);
void sparc_bus_barrier(bus_space_tag_t, bus_space_tag_t,  bus_space_handle_t,
    bus_size_t, bus_size_t, int);
int sparc_bus_alloc(bus_space_tag_t, bus_space_tag_t, bus_addr_t, bus_addr_t,
    bus_size_t, bus_size_t, bus_size_t, int, bus_addr_t *,
    bus_space_handle_t *);
void sparc_bus_free(bus_space_tag_t, bus_space_tag_t, bus_space_handle_t,
    bus_size_t);

int
sparc_bus_map(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t	addr,
    bus_size_t size, int flags, bus_space_handle_t *hp)
{
	vaddr_t va;
	u_int64_t pa;
	paddr_t	pm_flags = 0;
	vm_prot_t pm_prot = VM_PROT_READ;

	if (flags & BUS_SPACE_MAP_PROMADDRESS) {
		hp->bh_ptr = addr;
		return (0);
	}

	if (size == 0) {
		char buf[80];
		bus_space_render_tag(t0, buf, sizeof buf);
		printf("\nsparc_bus_map: zero size on %s", buf);
		return (EINVAL);
	}

	if ( (LITTLE_ASI(t0->asi) && LITTLE_ASI(t0->sasi)) ||
	    (PHYS_ASI(t0->asi) != PHYS_ASI(t0->sasi)) ) {
		char buf[80];
		bus_space_render_tag(t0, buf, sizeof buf);
		printf("\nsparc_bus_map: mismatched ASIs on %s: asi=%x sasi=%x",
		    buf, t0->asi, t0->sasi);
	}

	if (PHYS_ASI(t0->asi)) {
#ifdef BUS_SPACE_DEBUG
		char buf[80];
		bus_space_render_tag(t0, buf, sizeof buf);
		BUS_SPACE_PRINTF(BSDB_MAP,
		    ("\nsparc_bus_map: physical tag %s asi %x sasi %x flags %x "
		    "paddr %016llx size %016llx",
		    buf,
		    (int)t0->asi, (int)t0->sasi, (int)flags,
		    (unsigned long long)addr, (unsigned long long)size));
#endif /* BUS_SPACE_DEBUG */
		if (flags & BUS_SPACE_MAP_LINEAR) {
			char buf[80];
			bus_space_render_tag(t0, buf, sizeof buf);
			printf("\nsparc_bus_map: linear mapping requested on physical bus %s", buf);
			return (EINVAL);
		}

		hp->bh_ptr = addr;
		return (0);
	}

	size = round_page(size);

	if (LITTLE_ASI(t0->sasi) && !LITTLE_ASI(t0->asi))
		pm_flags |= PMAP_LITTLE;

	if ((flags & BUS_SPACE_MAP_CACHEABLE) == 0)
		pm_flags |= PMAP_NC;

	va = uvm_km_valloc(kernel_map, size);
	if (va == 0)
		return (ENOMEM);

	/* note: preserve page offset */
	hp->bh_ptr = va | (addr & PGOFSET);

	pa = trunc_page(addr);
	if ((flags & BUS_SPACE_MAP_READONLY) == 0)
		pm_prot |= VM_PROT_WRITE;

#ifdef BUS_SPACE_DEBUG
	{ /* scope */
		char buf[80];
		bus_space_render_tag(t0, buf, sizeof buf);
		BUS_SPACE_PRINTF(BSDB_MAP, ("\nsparc_bus_map: tag %s type %x "
		    "flags %x addr %016llx size %016llx virt %llx paddr "
		    "%016llx", buf, (int)t->default_type, (int) flags,
		    (unsigned long long)addr, (unsigned long long)size,
		    (unsigned long long)hp->bh_ptr, (unsigned long long)pa));
	}
#endif /* BUS_SPACE_DEBUG */

	do {
		BUS_SPACE_PRINTF(BSDB_MAPDETAIL, ("\nsparc_bus_map: phys %llx "
		    "virt %p hp->bh_ptr %llx", (unsigned long long)pa,
		    (char *)v, (unsigned long long)hp->bh_ptr));
		pmap_enter(pmap_kernel(), va, pa | pm_flags, pm_prot,
			pm_prot|PMAP_WIRED);
		va += PAGE_SIZE;
		pa += PAGE_SIZE;
	} while ((size -= PAGE_SIZE) > 0);
	pmap_update(pmap_kernel());
	return (0);
}

int
sparc_bus_subregion(bus_space_tag_t tag, bus_space_tag_t tag0,
    bus_space_handle_t handle, bus_size_t offset, bus_size_t size,
    bus_space_handle_t *nhandlep)
{
	*nhandlep = handle;
	nhandlep->bh_ptr += offset;
	return (0);
}

/* stolen from uvm_chgkprot() */
/*
 * Change protections on kernel pages from addr to addr+len
 * (presumably so debugger can plant a breakpoint).
 *
 * We force the protection change at the pmap level.  If we were
 * to use vm_map_protect a change to allow writing would be lazily-
 * applied meaning we would still take a protection fault, something
 * we really don't want to do.  It would also fragment the kernel
 * map unnecessarily.  We cannot use pmap_protect since it also won't
 * enforce a write-enable request.  Using pmap_enter is the only way
 * we can ensure the change takes place properly.
 */
int
sparc_bus_protect(bus_space_tag_t t, bus_space_tag_t t0, bus_space_handle_t h,
    bus_size_t size, int flags)
{
        vm_prot_t prot;
	paddr_t	pm_flags = 0;
        paddr_t pa;
        vaddr_t sva, eva;
	void* addr = bus_space_vaddr(t0, h);

	if (addr == 0) {
		printf("\nsparc_bus_protect: null address");
		return (EINVAL);
	}

	if (PHYS_ASI(t0->asi)) {
		printf("\nsparc_bus_protect: physical ASI");
		return (EINVAL);
	}

        prot = (flags & BUS_SPACE_MAP_READONLY) ?
	    VM_PROT_READ : VM_PROT_READ | VM_PROT_WRITE;
	if ((flags & BUS_SPACE_MAP_CACHEABLE) == 0)
	    pm_flags |= PMAP_NC;

        eva = round_page((vaddr_t)addr + size);
        for (sva = trunc_page((vaddr_t)addr); sva < eva; sva += PAGE_SIZE) {
                /*
                 * Extract physical address for the page.
                 * We use a cheezy hack to differentiate physical
                 * page 0 from an invalid mapping, not that it
                 * really matters...
                 */
                if (pmap_extract(pmap_kernel(), sva, &pa) == FALSE)
                        panic("bus_space_protect(): invalid page");
                pmap_enter(pmap_kernel(), sva, pa | pm_flags, prot, prot | PMAP_WIRED);
        }
	pmap_update(pmap_kernel());

	return (0);
}

int
sparc_bus_unmap(bus_space_tag_t t, bus_space_tag_t t0, bus_space_handle_t bh,
    bus_size_t size)
{
	vaddr_t va = trunc_page((vaddr_t)bh.bh_ptr);
	vaddr_t endva = va + round_page(size);

	if (PHYS_ASI(t0->asi))
		return (0);

	pmap_remove(pmap_kernel(), va, endva);
	pmap_update(pmap_kernel());
	uvm_km_free(kernel_map, va, endva - va);

	return (0);
}

paddr_t
sparc_bus_mmap(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t paddr,
    off_t off, int prot, int flags)
{
	if (PHYS_ASI(t0->asi)) {
		printf("\nsparc_bus_mmap: physical ASI");
		return (NULL);
	}

	/* Devices are un-cached... although the driver should do that */
	return ((paddr + off) | PMAP_NC);
}

bus_addr_t
sparc_bus_addr(bus_space_tag_t t, bus_space_tag_t t0, bus_space_handle_t h)
{
	paddr_t addr;

	if (PHYS_ASI(t0->asi))
		return h.bh_ptr;

	if (!pmap_extract(pmap_kernel(), h.bh_ptr, &addr))
		return (-1);
	return addr;
}

void *
bus_intr_allocate(bus_space_tag_t t, int (*handler)(void *), void *arg,
    int number, int pil,
    volatile u_int64_t *mapper, volatile u_int64_t *clearer,
    const char *what)
{
	struct intrhand *ih;

	ih = (struct intrhand *)malloc(sizeof(struct intrhand), M_DEVBUF, M_NOWAIT);
	if (ih == NULL)
		return (NULL);

	memset(ih, 0, sizeof(struct intrhand));

	ih->ih_fun = handler;
	ih->ih_arg = arg;
	ih->ih_number = number;
	ih->ih_pil = pil;
	ih->ih_map = mapper;
	ih->ih_clr = clearer;
	ih->ih_bus = t;
	strlcpy(ih->ih_name, what, sizeof(ih->ih_name));

	return (ih);
}

void
bus_intr_free(void *arg)
{
	free(arg, M_DEVBUF);
}

void *
sparc_mainbus_intr_establish(bus_space_tag_t t, bus_space_tag_t t0, int number,
    int pil, int flags, int (*handler)(void *), void *arg, const char *what)
{
	struct intrhand *ih;

	ih = bus_intr_allocate(t0, handler, arg, number, pil, NULL, NULL, what);
	if (ih == NULL)
		return (NULL);

	intr_establish(ih->ih_pil, ih);

	return (ih);
}

void
sparc_bus_barrier(bus_space_tag_t t, bus_space_tag_t t0, bus_space_handle_t h,
    bus_size_t offset, bus_size_t size, int flags)
{
	/* 
	 * We have lots of alternatives depending on whether we're
	 * synchronizing loads with loads, loads with stores, stores
	 * with loads, or stores with stores.  The only ones that seem
	 * generic are #Sync and #MemIssue.  I'll use #Sync for safety.
	 */
	if (flags == (BUS_SPACE_BARRIER_READ|BUS_SPACE_BARRIER_WRITE))
		membar(Sync);
	else if (flags == BUS_SPACE_BARRIER_READ)
		membar(Sync);
	else if (flags == BUS_SPACE_BARRIER_WRITE)
		membar(Sync);
	else
		printf("sparc_bus_barrier: unknown flags\n");
	return;
}

int
sparc_bus_alloc(bus_space_tag_t t, bus_space_tag_t t0, bus_addr_t rs,
    bus_addr_t re, bus_size_t s, bus_size_t a, bus_size_t b, int f,
    bus_addr_t *ap, bus_space_handle_t *hp)
{
	return (ENOTTY);
}

void
sparc_bus_free(bus_space_tag_t t, bus_space_tag_t t0, bus_space_handle_t h,
    bus_size_t s)
{
	return;
}

static const struct sparc_bus_space_tag _mainbus_space_tag = {
	NULL,				/* cookie */
	NULL,				/* parent bus tag */
	UPA_BUS_SPACE,			/* type */
	ASI_PRIMARY,
	ASI_PRIMARY,
	"mainbus",
	sparc_bus_alloc,
	sparc_bus_free,
	sparc_bus_map,			/* bus_space_map */
	sparc_bus_protect,		/* bus_space_protect */
	sparc_bus_unmap,		/* bus_space_unmap */
	sparc_bus_subregion,		/* bus_space_subregion */
	sparc_bus_barrier,		/* bus_space_barrier */
	sparc_bus_mmap,			/* bus_space_mmap */
	sparc_mainbus_intr_establish,	/* bus_intr_establish */
	sparc_bus_addr			/* bus_space_addr */
};
const bus_space_tag_t mainbus_space_tag = &_mainbus_space_tag;

struct cfdriver mainbus_cd = {
	NULL, "mainbus", DV_DULL
};

#define _BS_PRECALL(t,f)		\
        while (t->f == NULL)		\
                t = t->parent;
#define _BS_POSTCALL

#define _BS_CALL(t,f)			\
        (*(t)->f)

int
bus_space_alloc(bus_space_tag_t t, bus_addr_t rs, bus_addr_t re, bus_size_t s,
    bus_size_t a, bus_size_t b, int f, bus_addr_t *ap, bus_space_handle_t *hp)
{
        const bus_space_tag_t t0 = t;
        int ret;

        _BS_PRECALL(t, sparc_bus_alloc);
        ret = _BS_CALL(t, sparc_bus_alloc)(t, t0, rs, re, s, a, b, f, ap, hp);
        _BS_POSTCALL;
        return ret;
}

void
bus_space_free(bus_space_tag_t t, bus_space_handle_t h, bus_size_t s)
{
	const bus_space_tag_t t0 = t;

	_BS_PRECALL(t, sparc_bus_free);
	_BS_CALL(t, sparc_bus_free)(t, t0, h, s);
	_BS_POSTCALL;
}

int
bus_space_map(bus_space_tag_t t, bus_addr_t a, bus_size_t s, int f,
    bus_space_handle_t *hp)
{
	const bus_space_tag_t t0 = t;
	int ret;

	_BS_PRECALL(t, sparc_bus_map);
	ret = _BS_CALL(t, sparc_bus_map)(t, t0, a, s, f, hp);
	_BS_POSTCALL;
#ifdef BUS_SPACE_DEBUG
	if(s == 0) {
		char buf[128];
		bus_space_render_tag(t, buf, sizeof buf);
		printf("\n********** bus_space_map: requesting "
		    "zero-length mapping on bus %p:%s",
		    t, buf);
	}
	hp->bh_flags = 0;
	if (ret == 0) {
		hp->bh_size = s;
		hp->bh_tag = t0;
	} else {
		hp->bh_size = 0;
		hp->bh_tag = NULL;
	}
#endif /* BUS_SPACE_DEBUG */
	return (ret);
}

int
bus_space_protect(bus_space_tag_t t, bus_space_handle_t h, bus_size_t s, int f)
{
	const bus_space_tag_t t0 = t;
	int ret;

	_BS_PRECALL(t, sparc_bus_protect);
	ret = _BS_CALL(t, sparc_bus_protect)(t, t0, h, s, f);
	_BS_POSTCALL;

	return (ret);
}

int
bus_space_unmap(bus_space_tag_t t, bus_space_handle_t h, bus_size_t s)
{
	const bus_space_tag_t t0 = t;
	int ret;

	_BS_PRECALL(t, sparc_bus_unmap);
	BUS_SPACE_ASSERT(t0, h, 0, 1);
#ifdef BUS_SPACE_DEBUG
	if(h.bh_size != s) {
		char buf[128];
		bus_space_render_tag(t0, buf, sizeof buf);
		printf("\n********* bus_space_unmap: %p:%s, map/unmap "
		    "size mismatch (%llx != %llx)",
		    t, buf, h.bh_size, s);
	}
#endif /* BUS_SPACE_DEBUG */
	ret = _BS_CALL(t, sparc_bus_unmap)(t, t0, h, s);
	_BS_POSTCALL;
	return (ret);
}

int
bus_space_subregion(bus_space_tag_t t, bus_space_handle_t h, bus_size_t o,
    bus_size_t s, bus_space_handle_t *hp)
{
	const bus_space_tag_t t0 = t;
	int ret;

	_BS_PRECALL(t, sparc_bus_subregion);
	BUS_SPACE_ASSERT(t0, h, o, 1);
#ifdef BUS_SPACE_DEBUG
	if(h.bh_size < o + s) {
		char buf[128];
		bus_space_render_tag(t0, buf, sizeof buf);
		printf("\n********** bus_space_subregion: "
		    "%p:%s, %llx < %llx + %llx", 
		    t0, buf, h.bh_size, o, s);
		hp->bh_size = 0;
		hp->bh_tag = NULL;
		return (EINVAL);
	}
#endif /* BUS_SPACE_DEBUG */
	ret = _BS_CALL(t, sparc_bus_subregion)(t, t0, h, o, s, hp);
	_BS_POSTCALL;
#ifdef BUS_SPACE_DEBUG
	if (ret == 0) {
		hp->bh_size = s;
		hp->bh_tag = t0;
	} else {
		hp->bh_size = 0;
		hp->bh_tag = NULL;
	}
#endif /* BUS_SPACE_DEBUG */
	return (ret);
}

paddr_t
bus_space_mmap(bus_space_tag_t t, bus_addr_t a, off_t o, int p, int f)
{
	const bus_space_tag_t t0 = t;
	paddr_t ret;

	_BS_PRECALL(t, sparc_bus_mmap);
	ret = _BS_CALL(t, sparc_bus_mmap)(t, t0, a, o, p, f);
	_BS_POSTCALL;
	return (ret);
}

void *
bus_intr_establish(bus_space_tag_t t, int p, int l, int f, int (*h)(void *),
    void *a, const char *w)
{
	const bus_space_tag_t t0 = t;
	void *ret;

	_BS_PRECALL(t, sparc_intr_establish);
	ret = _BS_CALL(t, sparc_intr_establish)(t, t0, p, l, f, h, a, w);
	_BS_POSTCALL;
	return (ret);
}

/* XXXX Things get complicated if we use unmapped register accesses. */
void *
bus_space_vaddr(bus_space_tag_t t, bus_space_handle_t h)
{
	BUS_SPACE_ASSERT(t, h, 0, 1);
        if(t->asi == ASI_PRIMARY || t->asi == ASI_PRIMARY_LITTLE) 
		return 	((void *)(vaddr_t)(h.bh_ptr));

#ifdef BUS_SPACE_DEBUG
	{ /* Scope */
		char buf[64];
		bus_space_render_tag(t, buf, sizeof buf);
		printf("\nbus_space_vaddr: no vaddr for %p:%s (asi=%x)",
			t, buf, t->asi);
	}
#endif

	return (NULL);
}

void
bus_space_render_tag(bus_space_tag_t t, char* buf, size_t len)
{
	if (t == NULL) {
		strlcat(buf, "<NULL>", len);
		return;
	}
	buf[0] = '\0';
	if (t->parent)
		bus_space_render_tag(t->parent, buf, len);

	strlcat(buf, "/", len);
	strlcat(buf, t->name, len);
}

#ifdef BUS_SPACE_DEBUG

void
bus_space_assert(bus_space_tag_t t, const bus_space_handle_t *h, bus_size_t o,
    int n)
{
        if (h->bh_tag != t) {
		char buf1[128];
		char buf2[128];
		bus_space_render_tag(t, buf1, sizeof buf1);
		bus_space_render_tag(h->bh_tag, buf2, sizeof buf2);
                printf("\n********** bus_space_assert: wrong tag (%p:%s, "
		    "expecting %p:%s) ", t, buf1, h->bh_tag, buf2);
	}

        if (o >= h->bh_size) {
		char buf[128];
		bus_space_render_tag(t, buf, sizeof buf);
                printf("\n********** bus_space_assert: bus %p:%s, offset "
		    "(%llx) out of mapping range (%llx) ", t, buf, o,
		    h->bh_size);
	}

	if (o & (n - 1)) {
		char buf[128];
		bus_space_render_tag(t, buf, sizeof buf);
                printf("\n********** bus_space_assert: bus %p:%s, offset "
		    "(%llx) incorrect alignment (%d) ", t, buf, o, n);
	}
}

#endif /* BUS_SPACE_DEBUG */

struct blink_led_softc {
	SLIST_HEAD(, blink_led) bls_head;
	int bls_on;
	struct timeout bls_to;
} blink_sc = { SLIST_HEAD_INITIALIZER(bls_head), 0 };

void
blink_led_register(struct blink_led *l)
{
	if (SLIST_EMPTY(&blink_sc.bls_head)) {
		timeout_set(&blink_sc.bls_to, blink_led_timeout, &blink_sc);
		blink_sc.bls_on = 0;
		if (sparc_led_blink)
			timeout_add(&blink_sc.bls_to, 1);
	}
	SLIST_INSERT_HEAD(&blink_sc.bls_head, l, bl_next);
}

void
blink_led_timeout(void *vsc)
{
	struct blink_led_softc *sc = &blink_sc;
	struct blink_led *l;
	int t;

	if (SLIST_EMPTY(&sc->bls_head))
		return;

	SLIST_FOREACH(l, &sc->bls_head, bl_next) {
		(*l->bl_func)(l->bl_arg, sc->bls_on);
	}
	sc->bls_on = !sc->bls_on;

	if (!sparc_led_blink)
		return;

	/*
	 * Blink rate is:
	 *      full cycle every second if completely idle (loadav = 0)
	 *      full cycle every 2 seconds if loadav = 1
	 *      full cycle every 3 seconds if loadav = 2
	 * etc.
	 */
	t = (((averunnable.ldavg[0] + FSCALE) * hz) >> (FSHIFT + 1));
	timeout_add(&sc->bls_to, t);
}
