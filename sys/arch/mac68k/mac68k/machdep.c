/*	$NetBSD: machdep.c,v 1.80 1995/10/07 06:25:48 mycroft Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
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
 */
/*-
 * Copyright (C) 1993	Allen K. Briggs, Chris P. Caputo,
 *			Michael L. Finch, Bradley A. Grantham, and
 *			Lawrence A. Kesteloot
 * All rights reserved.
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
 *	This product includes software developed by the Alice Group.
 * 4. The names of the Alice Group or any of its members may not be used
 *    to endorse or promote products derived from this software without
 *    specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE ALICE GROUP ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE ALICE GROUP BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * from: Utah $Hdr: machdep.c 1.63 91/04/24$
 *
 *	@(#)machdep.c	7.16 (Berkeley) 6/3/91
 */

#include <param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/map.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/exec.h>
#include <sys/vnode.h>
#include <sys/reboot.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/clist.h>
#include <sys/callout.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/msgbuf.h>
#include <sys/user.h>
#include <sys/sysctl.h>
#include <sys/mount.h>
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

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/psl.h>
#include <machine/pte.h>
#include <net/netisr.h>

#define	MAXMEM	64*1024*CLSIZE	/* XXX - from cmap.h */
#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>
#include <vm/vm_object.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <dev/cons.h>

#include "via.h"
#include "macrom.h"
#include "ether.h"

/* The following is used externally (sysctl_hw) */
char    machine[] = "mac68k";	/* cpu "architecture" */

struct mac68k_machine_S mac68k_machine;

volatile u_char *Via1Base, *Via2Base;
u_long  NuBusBase = NBBASE;
u_long  IOBase;

vm_offset_t SCSIBase;

/* These are used to map kernel space: */
extern int numranges;
extern u_long low[8];
extern u_long high[8];

/* These are used to map NuBus space: */
#define	NBMAXRANGES	16
int     nbnumranges;		/* = 0 == don't use the ranges */
u_long  nbphys[NBMAXRANGES];	/* Start physical addr of this range */
u_long  nblog[NBMAXRANGES];	/* Start logical addr of this range */
long    nblen[NBMAXRANGES];	/* Length of this range If the length is */
				/* negative, all phys addrs are the same. */

extern u_long videoaddr;	/* Addr used in kernel for video. */
extern u_long videorowbytes;	/* Used in kernel for video. */

/*
 * Values for IIvx-like internal video
 * -- should be zero if it is not used (usual case).
 */
u_int32_t mac68k_vidlog;	/* logical addr */
u_int32_t mac68k_vidphys;	/* physical addr */
u_int32_t mac68k_vidlen;	/* mem length */

vm_map_t buffer_map;

/*
 * Declare these as initialized data so we can patch them.
 */
int     nswbuf = 0;
#ifdef	NBUF
int     nbuf = NBUF;
#else
int     nbuf = 0;
#endif
#ifdef	BUFPAGES
int     bufpages = BUFPAGES;
#else
int     bufpages = 0;
#endif

int     msgbufmapped;		/* set when safe to use msgbuf */
int     maxmem;			/* max memory per process */
int     physmem = MAXMEM;	/* max supported memory, changes to actual */

/*
 * safepri is a safe priority for sleep to set for a spin-wait
 * during autoconfiguration or after a panic.
 */
int     safepri = PSL_LOWIPL;

/*
 * For the fpu emulation and fpu driver.
 */
int     fpu_type;

static void identifycpu __P((void));
void dumpsys __P((void));

/*
 * Console initialization: called early on from main,
 * before vm init or startup.  Do enough configuration
 * to choose and initialize a console.
 */
void
consinit(void)
{
	/*
	 * Generic console: sys/dev/cons.c
	 *	Initializes either ite or ser as console.
	 *	Can be called from locore.s and init_main.c.
	 */
	static int init;	/* = 0 */

	if (!init) {
		cninit();
#ifdef  DDB
		/*
		 * Initialize kernel debugger, if compiled in.
		 */
		ddb_init();
#endif
		init = 1;
	}
}

#define CURRENTBOOTERVER	108

/*
 * cpu_startup: allocate memory for variable-sized tables,
 * initialize cpu, and do autoconfiguration.
 */
void
cpu_startup(void)
{
	extern struct map	*useriomap;
	extern long		Usrptsize;
	register caddr_t	v, firstaddr;
	register unsigned	i;
	int     	vers;
	int     	base, residual;
	vm_offset_t	minaddr, maxaddr;
	vm_size_t	size = 0;	/* To avoid compiler warning */
	int     	delay;

	/*
	 * Initialize error message buffer (at end of core).
	 * high[numranges-1] was decremented in pmap_bootstrap.
	 */
	for (i = 0; i < btoc(sizeof(struct msgbuf)); i++)
		pmap_enter(pmap_kernel(), (vm_offset_t) msgbufp,
		    high[numranges - 1] + i * NBPG, VM_PROT_ALL, TRUE);
	msgbufmapped = 1;

	/*
	 * Good {morning,afternoon,evening,night}.
	 */
	printf(version);
	identifycpu();

	vers = mac68k_machine.booter_version;
	if (vers < CURRENTBOOTERVER) {
		/* fix older booters with indicies, not versions */
		if (vers < 100)
			vers += 99;

		printf("\nYou booted with booter version %d.%d.\n",
		    vers / 100, vers % 100);
		printf("Booter version %d.%d is necessary to fully support\n",
		    CURRENTBOOTERVER / 100, CURRENTBOOTERVER % 100);
		printf("this kernel.\n\n");
		for (delay = 0; delay < 1000000; delay++);
	}
	printf("real mem = %d\n", ctob(physmem));

	/*
	 * Allocate space for system data structures.
	 * The first available real memory address is in "firstaddr".
	 * The first available kernel virtual address is in "v".
	 * As pages of kernel virtual memory are allocated, "v" is incremented.
	 * As pages of memory are allocated and cleared,
	 * "firstaddr" is incremented.
	 * An index into the kernel page table corresponding to the
	 * virtual memory address maintained in "v" is kept in "mapaddr".
	 */
	/*
	 * Make two passes.  The first pass calculates how much memory is
	 * needed and allocates it.  The second pass assigns virtual
	 * addresses to the various data structures.
	 */
	firstaddr = 0;
again:
	v = (caddr_t) firstaddr;

#define	valloc(name, type, num) \
	    (name) = (type *)v; v = (caddr_t)((name)+(num))
#define	valloclim(name, type, num, lim) \
	    (name) = (type *)v; v = (caddr_t)((lim) = ((name)+(num)))
#ifdef REAL_CLISTS
	valloc(cfree, struct cblock, nclist);
#endif
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
	/*
	 * Determine how many buffers to allocate.
	 * Use 10% of memory for the first 2 Meg, 5% of the remaining
	 * memory. Insure a minimum of 16 buffers.
	 * We allocate 1/2 as many swap buffer headers as file i/o buffers.
	 */
	if (bufpages == 0)
		if (physmem < btoc(2 * 1024 * 1024))
			bufpages = physmem / 10 / CLSIZE;
		else
			bufpages = (btoc(2 * 1024 * 1024) + physmem) / 20 / CLSIZE;

	bufpages = min(NKMEMCLUSTERS * 2 / 5, bufpages);

	if (nbuf == 0) {
		nbuf = bufpages;
		if (nbuf < 16)
			nbuf = 16;
	}
	if (nswbuf == 0) {
		nswbuf = (nbuf / 2) & ~1;	/* force even */
		if (nswbuf > 256)
			nswbuf = 256;	/* sanity */
	}
	valloc(swbuf, struct buf, nswbuf);
	valloc(buf, struct buf, nbuf);

	/*
	 * End of first pass, size has been calculated so allocate memory
	 */
	if (firstaddr == 0) {
		size = (vm_size_t) (v - firstaddr);
		firstaddr = (caddr_t) kmem_alloc(kernel_map, round_page(size));
		if (firstaddr == 0)
			panic("startup: no room for tables");
		goto again;
	}
	/*
	 * End of second pass, addresses have been assigned
	 */
	if ((vm_size_t) (v - firstaddr) != size)
		panic("startup: table size inconsistency");

	/*
	 * Now allocate buffers proper.  They are different than the above
	 * in that they usually occupy more virtual memory than physical.
	 */
	size = MAXBSIZE * nbuf;
	buffer_map = kmem_suballoc(kernel_map, (vm_offset_t *) & buffers,
	    &maxaddr, size, TRUE);
	minaddr = (vm_offset_t) buffers;
	if (vm_map_find(buffer_map, vm_object_allocate(size), (vm_offset_t) 0,
		&minaddr, size, FALSE) != KERN_SUCCESS)
		panic("startup: cannot allocate buffers");
	if ((bufpages / nbuf) >= btoc(MAXBSIZE)) {
		/* Don't want to alloc more physical mem than needed. */
		bufpages = btoc(MAXBSIZE) * nbuf;
	}
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
		curbuf = (vm_offset_t) buffers + i * MAXBSIZE;
		curbufsize = CLBYTES * (i < residual ? base + 1 : base);
		vm_map_pageable(buffer_map, curbuf, curbuf + curbufsize, FALSE);
		vm_map_simplify(buffer_map, curbuf);
	}
	/*
	 * Allocate a submap for exec arguments.  This map effectively
	 * limits the number of processes exec'ing at any time.
	 */
	exec_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
	    16 * NCARGS, TRUE);

	/*
	 * Allocate a submap for physio
	 */
	phys_map = kmem_suballoc(kernel_map, &minaddr, &maxaddr,
	    VM_PHYS_SIZE, TRUE);

	/*
	 * Finally, allocate mbuf pool.  Since mclrefcnt is an off-size
	 * we use the more space efficient malloc in place of kmem_alloc.
	 */
	mclrefcnt = (char *) malloc(NMBCLUSTERS + CLBYTES / MCLBYTES,
	    M_MBUF, M_NOWAIT);
	bzero(mclrefcnt, NMBCLUSTERS + CLBYTES / MCLBYTES);
	mb_map = kmem_suballoc(kernel_map, (vm_offset_t *) & mbutl, &maxaddr,
	    VM_MBUF_SIZE, FALSE);

	/*
	 * Initialize callouts
	 */
	callfree = callout;
	for (i = 1; i < ncallout; i++)
		callout[i - 1].c_next = &callout[i];

	printf("avail mem = %d\n", ptoa(cnt.v_free_count));
	printf("using %d buffers containing %d bytes of memory\n",
	    nbuf, bufpages * CLBYTES);

	/*
	 * Set up buffers, so they can be used to read disk labels.
	 */
	bufinit();

	/*
	 * Configure the system.
	 */
	configure();

	if (current_mac_model->class == MACH_CLASSII) {
		/*
		 * For the bloody Mac II ROMs, we have to map this space
		 * so that the PRam functions will work.
		 * Gee, Apple, is that a hard-coded hardware address in
		 * your code?  I think so! (_ReadXPRam + 0x0062)  We map
		 * the first 
		 */
#ifdef DIAGNOSTIC
		printf("I/O map kludge for old ROMs that use hardware %s",
			"addresses directly.\n");
#endif
		pmap_map(0x50f00000, 0x50f00000, 0x50f00000 + 0x4000,
			 VM_PROT_READ|VM_PROT_WRITE);
	}
}

/*
 * Set registers on exec.
 * XXX Should clear registers except sp, pc,
 * but would break init; should be fixed soon.
 */
void
setregs(p, pack, sp, retval)
	register struct proc *p;
	struct exec_package *pack;
	u_long  sp;
	register_t *retval;
{
	struct frame *frame;

	frame = (struct frame *) p->p_md.md_regs;
	frame->f_pc = pack->ep_entry & ~1;
	frame->f_regs[SP] = sp;
	frame->f_regs[A2] = (int) PS_STRINGS;

	/* restore a null state frame */
	p->p_addr->u_pcb.pcb_fpregs.fpf_null = 0;

	if (fpu_type) {
		m68881_restore(&p->p_addr->u_pcb.pcb_fpregs);
	}
}

#define SS_RTEFRAME	1
#define SS_FPSTATE	2
#define SS_USERREGS	4

struct sigstate {
	int     ss_flags;	/* which of the following are valid */
	struct frame ss_frame;	/* original exception frame */
	struct fpframe ss_fpstate;	/* 68881/68882 state info */
};
/*
 * WARNING: code in locore.s assumes the layout shown for sf_signum
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int     sf_signum;	/* signo for handler */
	int     sf_code;	/* additional info for handler */
	struct sigcontext *sf_scp;	/* context ptr for handler */
	sig_t   sf_handler;	/* handler addr for u_sigc */
	struct sigstate sf_state;	/* state of the hardware */
	struct sigcontext sf_sc;/* actual context */
};
#ifdef DEBUG
int     sigdebug = 0x0;
int     sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(catcher, sig, mask, code)
	sig_t   catcher;
	int     sig, mask;
	u_long  code;
{
	extern short	exframesize[];
	extern char	sigcode[], esigcode[];
	register struct proc *p = curproc;
	register struct sigframe *fp, *kfp;
	register struct frame *frame;
	register struct sigacts *psp = p->p_sigacts;
	register short	ft;
	int     oonstack;

	frame = (struct frame *) p->p_md.md_regs;
	ft = frame->f_format;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && oonstack == 0 &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *) (psp->ps_sigstk.ss_base +
		    psp->ps_sigstk.ss_size - sizeof(struct sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *) frame->f_regs[SP] - 1;
	if ((unsigned) fp <= USRSTACK - ctob(p->p_vmspace->vm_ssize))
		(void) grow(p, (unsigned) fp);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d ssp %x usp %x scp %x ft %d\n",
		    p->p_pid, sig, &oonstack, fp, &fp->sf_sc, ft);
#endif
	if (useracc((caddr_t) fp, sizeof(struct sigframe), B_WRITE) == 0) {
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sendsig(%d): useracc failed on sig %d\n",
			    p->p_pid, sig);
#endif
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
	kfp = malloc(sizeof(struct sigframe), M_TEMP, M_WAITOK);
	/* Build the argument list for the signal handler. */
	kfp->sf_signum = sig;
	kfp->sf_code = code;
	kfp->sf_scp = &fp->sf_sc;
	kfp->sf_handler = catcher;
	/*
	 * Save necessary hardware state.  Currently this includes:
	 *	- general registers
	 *	- original exception frame (if not a "normal" frame)
	 *	- FP coprocessor state
	 */
	kfp->sf_state.ss_flags = SS_USERREGS;
	bcopy((caddr_t) frame->f_regs,
	    (caddr_t) kfp->sf_state.ss_frame.f_regs, sizeof frame->f_regs);
	if (ft >= FMT9) {
#ifdef DEBUG
		if (ft != FMT9 && ft != FMTA && ft != FMTB)
			panic("sendsig: bogus frame type");
#endif
		kfp->sf_state.ss_flags |= SS_RTEFRAME;
		kfp->sf_state.ss_frame.f_format = frame->f_format;
		kfp->sf_state.ss_frame.f_vector = frame->f_vector;
		bcopy((caddr_t) & frame->F_u,
		    (caddr_t) & kfp->sf_state.ss_frame.F_u, exframesize[ft]);
		/*
		 * Leave an indicator that we need to clean up the kernel
		 * stack.  We do this by setting the "pad word" above the
		 * hardware stack frame to the amount the stack must be
		 * adjusted by.
		 *
		 * N.B. we increment rather than just set f_stackadj in
		 * case we are called from syscall when processing a
		 * sigreturn.  In that case, f_stackadj may be non-zero.
		 */
		frame->f_stackadj += exframesize[ft];
		frame->f_format = frame->f_vector = 0;
#ifdef DEBUG
		if (sigdebug & SDB_FOLLOW)
			printf("sendsig(%d): copy out %d of frame %d\n",
			    p->p_pid, exframesize[ft], ft);
#endif
	}
	if (fpu_type) {
		kfp->sf_state.ss_flags |= SS_FPSTATE;
		m68881_save(&kfp->sf_state.ss_fpstate);
	}
#ifdef DEBUG
	if ((sigdebug & SDB_FPSTATE) && *(char *) &kfp->sf_state.ss_fpstate)
		printf("sendsig(%d): copy out FP state (%x) to %x\n",
		    p->p_pid, *(u_int *) & kfp->sf_state.ss_fpstate,
		    &kfp->sf_state.ss_fpstate);
#endif

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	kfp->sf_sc.sc_onstack = oonstack;
	kfp->sf_sc.sc_mask = mask;
	kfp->sf_sc.sc_sp = frame->f_regs[SP];
	kfp->sf_sc.sc_fp = frame->f_regs[A6];
	kfp->sf_sc.sc_ap = (int) &fp->sf_state;
	kfp->sf_sc.sc_pc = frame->f_pc;
	kfp->sf_sc.sc_ps = frame->f_sr;
	(void) copyout((caddr_t) kfp, (caddr_t) fp, sizeof(struct sigframe));
	frame->f_regs[SP] = (int) fp;
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sendsig(%d): sig %d scp %x fp %x sc_sp %x sc_ap %x\n",
		    p->p_pid, sig, kfp->sf_scp, fp,
		    kfp->sf_sc.sc_sp, kfp->sf_sc.sc_ap);
#endif
	/*
	 * Signal trampoline code is at base of user stack.
	 */
	frame->f_pc = (int) (((char *) PS_STRINGS) - (esigcode - sigcode));
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sendsig(%d): sig %d returns\n",
		    p->p_pid, sig);
#endif
	free((caddr_t) kfp, M_TEMP);
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper priviledges or to cause
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
	extern short	exframesize[];
	struct sigcontext *scp, context;
	struct frame	*frame;
	struct sigstate	tstate;
	int     rf, flags;

	scp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	if ((int) scp & 1)
		return (EINVAL);
	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	if (useracc((caddr_t) scp, sizeof(*scp), B_WRITE) == 0 ||
	    copyin(scp, &context, sizeof(context)))
		return (EINVAL);
	scp = &context;
	if ((scp->sc_ps & (PSL_MBZ | PSL_IPL | PSL_S)) != 0)
		return (EINVAL);
	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask & ~sigcantmask;
	frame = (struct frame *) p->p_md.md_regs;
	frame->f_regs[SP] = scp->sc_sp;
	frame->f_regs[A6] = scp->sc_fp;
	frame->f_pc = scp->sc_pc;
	frame->f_sr = scp->sc_ps;

	/*
	 * Grab pointer to hardware state information.
	 * If zero, the user is probably doing a longjmp.
	 */
	if ((rf = scp->sc_ap) == 0)
		return (EJUSTRETURN);
	/*
	 * See if there is anything to do before we go to the
	 * expense of copying in close to 1/2K of data
	 */
	flags = fuword((caddr_t) rf);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn(%d): sc_ap %x flags %x\n",
		    p->p_pid, rf, flags);
#endif
	/*
	 * fuword failed (bogus sc_ap value).
	 */
	if (flags == -1)
		return (EINVAL);
	if (flags == 0 || copyin((caddr_t) rf, (caddr_t) & tstate, sizeof tstate))
		return (EJUSTRETURN);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sigreturn(%d): ssp %x usp %x scp %x ft %d\n",
		    p->p_pid, &flags, scp->sc_sp, SCARG(uap, sigcntxp),
		    (flags & SS_RTEFRAME) ? tstate.ss_frame.f_format : -1);
#endif
	/*
	 * Restore most of the users registers except for A6 and SP
	 * which were handled above.
	 */
	if (flags & SS_USERREGS)
		bcopy((caddr_t) tstate.ss_frame.f_regs,
		    (caddr_t) frame->f_regs, sizeof(frame->f_regs) - 2 * NBPW);
	/*
	 * Restore long stack frames.  Note that we do not copy
	 * back the saved SR or PC, they were picked up above from
	 * the sigcontext structure.
	 */
	if (flags & SS_RTEFRAME) {
		register int sz;

		/* grab frame type and validate */
		sz = tstate.ss_frame.f_format;
		if (sz > 15 || (sz = exframesize[sz]) < 0)
			return (EINVAL);
		frame->f_stackadj -= sz;
		frame->f_format = tstate.ss_frame.f_format;
		frame->f_vector = tstate.ss_frame.f_vector;
		bcopy((caddr_t) & tstate.ss_frame.F_u, (caddr_t) & frame->F_u, sz);
#ifdef DEBUG
		if (sigdebug & SDB_FOLLOW)
			printf("sigreturn(%d): copy in %d of frame type %d\n",
			    p->p_pid, sz, tstate.ss_frame.f_format);
#endif
	}
	/*
	 * Finally we restore the original FP context
	 */
	if (flags & SS_FPSTATE)
		m68881_restore(&tstate.ss_fpstate);
#ifdef DEBUG
	if ((sigdebug & SDB_FPSTATE) && *(char *) &tstate.ss_fpstate)
		printf("sigreturn(%d): copied in FP state (%x) at %x\n",
		    p->p_pid, *(u_int *) & tstate.ss_fpstate,
		    &tstate.ss_fpstate);
	if ((sigdebug & SDB_FOLLOW) ||
	    ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid))
		printf("sigreturn(%d): returns\n", p->p_pid);
#endif
	return (EJUSTRETURN);
}

int     waittime = -1;
struct pcb dumppcb;

void
boot(howto)
	register int howto;
{
	extern u_long MacOSROMBase;

	/* take a snap shot before clobbering any registers */
	if (curproc)
		savectx(curproc->p_addr);

	boothowto = howto;
	if ((howto & RB_NOSYNC) == 0 && waittime < 0) {
		waittime = 0;

		/*
		 * Release inodes, sync and unmount the filesystems.
		 */
		vfs_shutdown();

		/*
		 * If we've been adjusting the clock, the todr
		 * will be out of synch; adjust it now.
		 */
		resettodr();
	}
	splhigh();		/* extreme priority */
	if (howto & RB_HALT) {
		printf("halted\n\n");
		via_shutdown();
	} else {
		if (howto & RB_DUMP) {
			savectx(&dumppcb);
			dumpsys();
		}
		/*
		 * Map ROM where the MacOS likes it, so we can reboot,
		 * hopefully.
		 */
		pmap_map(MacOSROMBase, MacOSROMBase,
			 MacOSROMBase + 4 * 1024 * 1024,
			 VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);
		doboot();
		/* NOTREACHED */
	}
	printf("            The system is down.\n");
	printf("You may reboot or turn the machine off, now.\n");
	for (;;);		/* Foil the compiler... */
	/* NOTREACHED */
}

/*
 * These variables are needed by /sbin/savecore
 */
u_long  dumpmag = 0x8fca0101;	/* magic number */
int     dumpsize = 0;		/* pages */
long    dumplo = 0;		/* blocks */

static int
get_max_page()
{
	int     i, max = 0;

	for (i = 0; i < numranges; i++) {
		if (high[i] > max)
			max = high[i];
	}
	return max;
}

/*
 * This is called by configure to set dumplo and dumpsize.
 * Dumps always skip the first CLBYTES of disk space in
 * case there might be a disk label stored there.  If there
 * is extra space, put dump at the end to reduce the chance
 * that swapping trashes it.
 */
void
dumpconf()
{
	int     nblks;
	int     maj;

	if (dumpdev == NODEV)
		return;

	maj = major(dumpdev);
	if (maj < 0 || maj >= nblkdev)
		panic("dumpconf: bad dumpdev=0x%x", dumpdev);
	if (bdevsw[maj].d_psize == NULL)
		return;
	nblks = (*bdevsw[maj].d_psize) (dumpdev);
	if (nblks <= ctod(1))
		return;

	dumpsize = btoc(get_max_page());

	/* Always skip the first CLBYTES, in case there is a label there. */
	if (dumplo < ctod(1))
		dumplo = ctod(1);

	/* Put dump at end of partition, and make it fit. */
	if (dumpsize > dtoc(nblks - dumplo))
		dumpsize = dtoc(nblks - dumplo);
	if (dumplo < nblks - ctod(dumpsize))
		dumplo = nblks - ctod(dumpsize);
}

/*
 * Doadump comes here after turning off memory management and
 * getting on the dump stack, either when called above, or by
 * the auto-restart code.
 */
#define BYTES_PER_DUMP NBPG	/* Must be a multiple of pagesize XXX small */
static vm_offset_t dumpspace;

vm_offset_t
reserve_dumppages(p)
	vm_offset_t p;
{
	dumpspace = p;
	return (p + BYTES_PER_DUMP);
}

static int
find_range(pa)
	vm_offset_t pa;
{
	int     i, max = 0;

	for (i = 0; i < numranges; i++) {
		if (low[i] <= pa && pa < high[i])
			return i;
	}
	return -1;
}

static int
find_next_range(pa)
	vm_offset_t pa;
{
	int     i, near, best, t;

	near = -1;
	best = 0x7FFFFFFF;
	for (i = 0; i < numranges; i++) {
		if (low[i] <= pa && pa < high[i])
			return i;
		t = low[i] - pa;
		if (t > 0) {
			if (t < best) {
				near = i;
				best = t;
			}
		}
	}
	return near;
}

void
dumpsys()
{
	unsigned bytes, i, n;
	int     range;
	int     maddr, psize;
	daddr_t blkno;
	int     (*dump) __P((dev_t, daddr_t, caddr_t, size_t));
	int     error = 0;
	int     c;

	msgbufmapped = 0;	/* don't record dump msgs in msgbuf */
	if (dumpdev == NODEV)
		return;

	/*
	 * For dumps during autoconfiguration,
	 * if dump device has already configured...
	 */
	if (dumpsize == 0)
		dumpconf();
	if (dumplo < 0)
		return;
	printf("\ndumping to dev %x, offset %d\n", dumpdev, dumplo);

	psize = (*bdevsw[major(dumpdev)].d_psize) (dumpdev);
	printf("dump ");
	if (psize == -1) {
		printf("area unavailable.\n");
		return;
	}
	bytes = get_max_page();
	maddr = 0;
	range = find_range(0);
	blkno = dumplo;
	dump = bdevsw[major(dumpdev)].d_dump;
	for (i = 0; i < bytes; i += n) {
		/*
		 * Avoid dumping "holes."
		 */
		if ((range == -1) || (i >= high[range])) {
			range = find_next_range(i);
			if (range == -1) {
				error = EIO;
				break;
			}
			n = low[range] - i;
			maddr += n;
			blkno += btodb(n);
			continue;
		}
		/* Print out how many MBs we have to go. */
		n = bytes - i;
		if (n && (n % (1024 * 1024)) == 0)
			printf("%d ", n / (1024 * 1024));

		/* Limit size for next transfer. */
		if (n > BYTES_PER_DUMP)
			n = BYTES_PER_DUMP;

		(void) pmap_map(dumpspace, maddr, maddr + n, VM_PROT_READ);
		error = (*dump) (dumpdev, blkno, (caddr_t) dumpspace, n);
		if (error)
			break;
		maddr += n;
		blkno += btodb(n);	/* XXX? */
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

	case EINTR:
		printf("aborted from console\n");
		break;

	case 0:
		printf("succeeded\n");
		break;

	default:
		printf("error %d\n", error);
		break;
	}
	printf("\n\n");
	delay(5000000);		/* 5 seconds */
}

/*
 * Return the best possible estimate of the time in the timeval
 * to which tvp points.  We do this by returning the current time
 * plus the amount of time since the last clock interrupt (clock.c:clkread).
 *
 * Check that this time is no less than any previously-reported time,
 * which could happen around the time of a clock adjustment.  Just for fun,
 * we guarantee that the time will be greater than the value obtained by a
 * previous call.
 */
void
microtime(tvp)
	register struct timeval *tvp;
{
	int     s = splhigh();
	static struct timeval lasttime;

	*tvp = time;
	tvp->tv_usec += clkread();
	while (tvp->tv_usec > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	if (tvp->tv_sec == lasttime.tv_sec &&
	    tvp->tv_usec <= lasttime.tv_usec &&
	    (tvp->tv_usec = lasttime.tv_usec + 1) > 1000000) {
		tvp->tv_sec++;
		tvp->tv_usec -= 1000000;
	}
	lasttime = *tvp;
	splx(s);
}

straytrap(pc, evec)
	int     pc;
	int     evec;
{
	printf("unexpected trap; vector offset 0x%x from 0x%x.\n",
	    (int) (evec & 0xfff), pc);
}

int    *nofault;

badaddr(addr)
	register caddr_t addr;
{
	register int i;
	label_t faultbuf;

#ifdef lint
	i = *addr;
	if (i)
		return (0);
#endif
	nofault = (int *) &faultbuf;
	if (setjmp((label_t *) nofault)) {
		nofault = (int *) 0;
		return (1);
	}
	i = *(volatile short *) addr;
	nofault = (int *) 0;
	return (0);
}

badbaddr(addr)
	register caddr_t addr;
{
	register int i;
	label_t faultbuf;

#ifdef lint
	i = *addr;
	if (i)
		return (0);
#endif
	nofault = (int *) &faultbuf;
	if (setjmp((label_t *) nofault)) {
		nofault = (int *) 0;
		return (1);
	}
	i = *(volatile char *) addr;
	nofault = (int *) 0;
	return (0);
}

netintr()
{
#ifdef INET
#if NETHER
	if (netisr & (1 << NETISR_ARP)) {
		netisr &= ~(1 << NETISR_ARP);
		arpintr();
	}
#endif
	if (netisr & (1 << NETISR_IP)) {
		netisr &= ~(1 << NETISR_IP);
		ipintr();
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
#include "ppp.h"
#if NPPP > 0
	if (netisr & (1 << NETISR_PPP)) {
		netisr &= ~(1 << NETISR_PPP);
		pppintr();
	}
#endif
}

#if defined(DEBUG) && !defined(PANICBUTTON)
#define PANICBUTTON
#endif

#ifdef PANICBUTTON
int     panicbutton = 1;	/* non-zero if panic buttons are enabled */
int     crashandburn = 0;
int     candbdelay = 50;	/* give em half a second */

candbtimer()
{
	crashandburn = 0;
}
#endif

/*
 * Level 7 interrupts can be caused by the keyboard or parity errors.
 */
void
nmihand(frame)
	struct frame frame;
{
	static int nmihanddeep = 0;

	if (nmihanddeep++)
		return;
/*	regdump(&frame, 128);
	dumptrace(); */
#if DDB
	printf("Panic switch: PC is 0x%x.\n", frame.f_pc);
	Debugger();
#endif
	nmihanddeep = 0;
}

regdump(frame, sbytes)
	struct frame *frame;
	int     sbytes;
{
	static int doingdump = 0;
	register int i;
	int     s;

	if (doingdump)
		return;
	s = splhigh();
	doingdump = 1;
	printf("pid = %d, pc = 0x%08x, ", curproc->p_pid, frame->f_pc);
	printf("ps = 0x%08x, ", frame->f_sr);
	printf("sfc = 0x%08x, ", getsfc());
	printf("dfc = 0x%08x\n", getdfc());
	printf("Registers:\n     ");
	for (i = 0; i < 8; i++)
		printf("        %d", i);
	printf("\ndreg:");
	for (i = 0; i < 8; i++)
		printf(" %08x", frame->f_regs[i]);
	printf("\nareg:");
	for (i = 0; i < 8; i++)
		printf(" %08x", frame->f_regs[i + 8]);
	if (sbytes > 0) {
		if (1) {	/* (frame->f_sr & PSL_S) *//* BARF - BG */
			printf("\n\nKernel stack (%08x):",
			    (int) (((int *) frame) - 1));
			dumpmem(((int *) frame) - 1, sbytes);
		} else {
			printf("\n\nUser stack (%08x):", frame->f_regs[15]);
			dumpmem((int *) frame->f_regs[15], sbytes);
		}
	}
	doingdump = 0;
	splx(s);
}

dumpmem(ptr, sz)
	register u_int *ptr;
	int     sz;
{
	register int i, val, same;

	sz /= 4;
	for (i = 0; i < sz; i++) {
		if ((i & 7) == 0)
			printf("\n%08x: ", (u_int) ptr);
		else
			printf(" ");
		printf("%08x ", *ptr++);
	}
	printf("\n");
}

/*
 * It should be possible to probe for the top of RAM, but Apple has
 * memory structured so that in at least some cases, it's possible
 * for RAM to be aliased across all memory--or for it to appear that
 * there is more RAM than there really is.
 */
int
get_top_of_ram()
{
	u_long  search = 0xb00bfade;
	u_long  i, found, store;
	char   *p, *zero;

	return ((mac68k_machine.mach_memsize * (1024 * 1024)) - 4096);
}

/*
 * machine dependent system variables.
 */
cpu_sysctl(name, namelen, oldp, oldlenp, newp, newlen, p)
	int    *name;
	u_int   namelen;
	void   *oldp;
	size_t *oldlenp;
	void   *newp;
	size_t  newlen;
	struct proc *p;
{
	dev_t   consdev;

	/* all sysctl names at this level are terminal */
	if (namelen != 1)
		return (ENOTDIR);	/* overloaded */

	switch (name[0]) {
	case CPU_CONSDEV:
		if (cn_tab != NULL)
			consdev = cn_tab->cn_dev;
		else
			consdev = NODEV;
		return (sysctl_rdstruct(oldp, oldlenp, newp, &consdev,
			sizeof consdev));
	default:
		return (EOPNOTSUPP);
	}
	/* NOTREACHED */
}

cpu_exec_aout_makecmds(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	int     error = ENOEXEC;
	struct exec *execp = epp->ep_hdr;

#ifdef COMPAT_NOMID
	if (execp->a_midmag == ZMAGIC)	/* i.e., MID == 0. */
		return cpu_exec_prep_oldzmagic(p, epp);
#endif

#ifdef COMPAT_SUNOS
	{
		extern sunos_exec_aout_makecmds __P((struct proc *,
			        struct exec_package *));
		if ((error = sunos_exec_aout_makecmds(p, epp)) == 0)
			return 0;
	}
#endif
	return error;
}

#ifdef COMPAT_NOMID
int
cpu_exec_prep_oldzmagic(p, epp)
	struct proc *p;
	struct exec_package *epp;
{
	struct exec *execp = epp->ep_hdr;
	struct exec_vmcmd *ccmdp;

	epp->ep_taddr = 0;
	epp->ep_tsize = execp->a_text;
	epp->ep_daddr = epp->ep_taddr + execp->a_text;
	epp->ep_dsize = execp->a_data + execp->a_bss;
	epp->ep_entry = execp->a_entry;

	/* check if vnode is in open for writing, because we want to
	 * demand-page out of it.  if it is, don't do it, for various reasons */
	if ((execp->a_text != 0 || execp->a_data != 0) &&
	    epp->ep_vp->v_writecount != 0) {
#ifdef DIAGNOSTIC
		if (epp->ep_vp->v_flag & VTEXT)
			panic("exec: a VTEXT vnode has writecount != 0\n");
#endif
		return ETXTBSY;
	}
	epp->ep_vp->v_flag |= VTEXT;

	/* set up command for text segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_text,
	    epp->ep_taddr, epp->ep_vp, NBPG,	/* should NBPG be CLBYTES? */
	    VM_PROT_READ | VM_PROT_EXECUTE);

	/* set up command for data segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_pagedvn, execp->a_data,
	    epp->ep_daddr, epp->ep_vp,
	    execp->a_text + NBPG,	/* should NBPG be CLBYTES? */
	    VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);

	/* set up command for bss segment */
	NEW_VMCMD(&epp->ep_vmcmds, vmcmd_map_zero, execp->a_bss,
	    epp->ep_daddr + execp->a_data, NULLVP, 0,
	    VM_PROT_READ | VM_PROT_WRITE | VM_PROT_EXECUTE);

	return exec_aout_setup_stack(p, epp);
}
#endif				/* COMPAT_NOMID */

static char *envbuf = NULL;

void
initenv(flag, buf)
	u_long  flag;
	char   *buf;
{
	/*
         * If flag & 0x80000000 == 0, then we're booting with the old booter
         * and we should freak out.
         */

	if ((flag & 0x80000000) == 0) {
		/* Freak out; print something if that becomes available */
	} else {
		envbuf = buf;
	}
}

static char
toupper(c)
	char    c;
{
	if (c >= 'a' && c <= 'z') {
		return c - 'a' + 'A';
	} else {
		return c;
	}
}

static long
getenv(str)
	char   *str;
{
	/*
         * Returns the value of the environment variable "str".
         *
         * Format of the buffer is "var=val\0var=val\0...\0var=val\0\0".
         *
         * Returns 0 if the variable is not there, and 1 if the variable is there
         * without an "=val".
         */

	char   *s, *s1, *s2;
	int     val, base;

	s = envbuf;
	while (1) {
		for (s1 = str; *s1 && *s && *s != '='; s1++, s++) {
			if (toupper(*s1) != toupper(*s)) {
				break;
			}
		}
		if (*s1) {	/* No match */
			while (*s) {
				s++;
			}
			s++;
			if (*s == '\0') {	/* Not found */
				/* Boolean flags are FALSE (0) if not there */
				return 0;
			}
			continue;
		}
		if (*s == '=') {/* Has a value */
			s++;
			val = 0;
			base = 10;
			if (*s == '0' && (*(s + 1) == 'x' || *(s + 1) == 'X')) {
				base = 16;
				s += 2;
			} else
				if (*s == '0') {
					base = 8;
				}
			while (*s) {
				if (toupper(*s) >= 'A' && toupper(*s) <= 'F') {
					val = val * base + toupper(*s) - 'A' + 10;
				} else {
					val = val * base + (*s - '0');
				}
				s++;
			}
			return val;
		} else {	/* TRUE (1) */
			return 1;
		}
	}
}

/*
 *ROM Vector information for calling drivers in ROMs
 */
static romvec_t romvecs[] =
{
	/* Vectors verified for II, IIx, IIcx, SE/30 */
	{			/* 0 */
		"Mac II class ROMs",
		(caddr_t) 0x40807002,	/* where does ADB interrupt */
		0,		/* PM interrupt (?) */
		(caddr_t) 0x4080a4d8,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x40807778,	/* CountADBs */
		(caddr_t) 0x40807792,	/* GetIndADB */
		(caddr_t) 0x408077be,	/* GetADBInfo */
		(caddr_t) 0x408077c4,	/* SetADBInfo */
		(caddr_t) 0x40807704,	/* ADBReInit */
		(caddr_t) 0x408072fa,	/* ADBOp */
		0,		/* PMgrOp */
		(caddr_t) 0x4080d6d0,	/* WriteParam */
		(caddr_t) 0x4080d6fa,	/* SetDateTime */
		(caddr_t) 0x4080dbe8,	/* InitUtil */
		(caddr_t) 0x4080dd78,	/* ReadXPRam */
		(caddr_t) 0x4080dd82,	/* WriteXPRam */
		(caddr_t) 0x4080ddd6,	/* jClkNoMem */
		0,			/* ADBAlternateInit */
		0,			/* InitEgret */
	},
	/*
	 * Vectors verified for PB 140, PB 170
	 * (PB 100?)
	 */
	{			/* 1 */
		"Powerbook class ROMs",
		(caddr_t) 0x4088ae5e,	/* ADB interrupt */
		(caddr_t) 0x408885ec,	/* PB ADB interrupt */
		(caddr_t) 0x4088ae0e,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x408888ec,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x4080b1e4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/*
	 * Vectors verified for IIsi, IIvx, IIvi
	 */
	{			/* 2 */
		"Mac IIsi class ROMs",
		(caddr_t) 0x40814912,	/* ADB interrupt */
		(caddr_t) 0,	/* PM ADB interrupt */
		(caddr_t) 0x408150f0,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x4080b1e4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/*
	 * Vectors verified for Mac Classic II and LC II
	 * (LC III?  Other LC's?  680x0 Performas?)
	 */
	{			/* 3 */
		"Mac Classic II ROMs",
		(caddr_t) 0x40a14912,	/* ADB interrupt */
		(caddr_t) 0,	/* PM ADB interrupt */
		(caddr_t) 0x40a150f0,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x40a0a360,	/* CountADBs */
		(caddr_t) 0x40a0a37a,	/* GetIndADB */
		(caddr_t) 0x40a0a3a6,	/* GetADBInfo */
		(caddr_t) 0x40a0a3ac,	/* SetADBInfo */
		(caddr_t) 0x40a0a752,	/* ADBReInit */
		(caddr_t) 0x40a0a3dc,	/* ADBOp */
		(caddr_t) 0,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b39b6,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/*
	 * Vectors verified for IIci, Q700
	 */
	{			/* 4 */
		"Mac IIci/Q700 ROMs",
		(caddr_t) 0x4080a700,	/* ADB interrupt */
		(caddr_t) 0,	/* PM ADB interrupt */
		(caddr_t) 0x4080a5aa,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x4080b1e4,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/*
	 * Vectors verified for Duo 230, PB 180, PB 165
	 * (Duo 210?  PB 165?  PB 160?  Duo 250?  Duo 270?)
	 */
	{			/* 5 */
		"2nd Powerbook class ROMs",
		(caddr_t) 0x408b2eec,	/* ADB interrupt */
		(caddr_t) 0x408885ec,	/* PB ADB interrupt */
		(caddr_t) 0x408b2e76,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x408888ec,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x0,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/*
	 * Quadra, Centris merged table (C650, 610, Q800?)
	 */
	{			/* 6 */
		"Quadra/Centris ROMs",
		(caddr_t) 0x408b2dea,	/* ADB int */
		0,			/* PM intr */
 		(caddr_t) 0x408b2c72,	/* ADBBase + 130 */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x40809ae6,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x408b39b6,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/*
	 * Quadra 840AV (but ADBBase + 130 intr is unknown)
	 * (PM intr is known to be 0, PMgrOp is guessed to be 0)
	 */
	{			/* 7 */
		"Quadra AV ROMs",
		(caddr_t) 0x4080cac6,	/* ADB int */
		0,		/* PM int */
		 /* !?! */ 0,	/* ADBBase + 130 */
		(caddr_t) 0x40839600,	/* CountADBs */
		(caddr_t) 0x4083961a,	/* GetIndADB */
		(caddr_t) 0x40839646,	/* GetADBInfo */
		(caddr_t) 0x4083964c,	/* SetADBInfo */
		(caddr_t) 0x408397b8,	/* ADBReInit */
		(caddr_t) 0x4083967c,	/* ADBOp */
		0,		/* PMgrOp */
		(caddr_t) 0x0,	/* WriteParam */
		(caddr_t) 0x0,	/* SetDateTime */
		(caddr_t) 0x0,	/* InitUtil */
		(caddr_t) 0x0,	/* ReadXPRam */
		(caddr_t) 0x0,	/* WriteXPRam */
		(caddr_t) 0x0,	/* jClkNoMem */
		0,			/* ADBAlternateInit */
		0,			/* InitEgret */
	},
	/*
	 * PB 540 (but ADBBase + 130 intr and PMgrOp is unknown)
	 * (PB 520?  Duo 280?)
	 */
	{			/* 8 */
		"68040 PowerBook ROMs",
		(caddr_t) 0x400b2efc,	/* ADB int */
		(caddr_t) 0x400d8e66,	/* PM int */
		 /* !?! */ 0,	/* ADBBase + 130 */
		(caddr_t) 0x4000a360,	/* CountADBs */
		(caddr_t) 0x4000a37a,	/* GetIndADB */
		(caddr_t) 0x4000a3a6,	/* GetADBInfo */
		(caddr_t) 0x4000a3ac,	/* SetADBInfo */
		(caddr_t) 0x4000a752,	/* ADBReInit */
		(caddr_t) 0x4000a3dc,	/* ADBOp */
		 /* !?! */ 0,	/* PmgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x0,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/*
	 * Q 605 (but guessing at ADBBase + 130, based on Q 650)
	 */
	{			/* 9 */
		"Quadra/Centris 605 ROMs",
		(caddr_t) 0x408a9b56,	/* ADB int */
		0,		/* PM int */
		(caddr_t) 0x408a99de,	/* ADBBase + 130 */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		0,		/* PmgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x0,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/*
	 * Vectors verified for Duo 270c
	 */
	{			/* 10 */
		"Duo 270C ROMs",
		(caddr_t) 0x408b2efc,	/* ADB interrupt */
		(caddr_t) 0x408885ec,	/* PB ADB interrupt */
		(caddr_t) 0x408b2e86,	/* ADBBase + 130 interrupt; whatzit? */
		(caddr_t) 0x4080a360,	/* CountADBs */
		(caddr_t) 0x4080a37a,	/* GetIndADB */
		(caddr_t) 0x4080a3a6,	/* GetADBInfo */
		(caddr_t) 0x4080a3ac,	/* SetADBInfo */
		(caddr_t) 0x4080a752,	/* ADBReInit */
		(caddr_t) 0x4080a3dc,	/* ADBOp */
		(caddr_t) 0x408888ec,	/* PMgrOp */
		(caddr_t) 0x4080c05c,	/* WriteParam */
		(caddr_t) 0x4080c086,	/* SetDateTime */
		(caddr_t) 0x4080c5cc,	/* InitUtil */
		(caddr_t) 0x4080b186,	/* ReadXPRam */
		(caddr_t) 0x4080b190,	/* WriteXPRam */
		(caddr_t) 0x0,	/* jClkNoMem */
		(caddr_t) 0x4080a818,	/* ADBAlternateInit */
		(caddr_t) 0x408147c4,	/* InitEgret */
	},
	/* Please fill these in! -BG */
};


struct cpu_model_info cpu_models[] = {

/* The first four. */
	{MACH_MACII, "II ", "", MACH_CLASSII, &romvecs[0]},
	{MACH_MACIIX, "IIx ", "", MACH_CLASSII, &romvecs[0]},
	{MACH_MACIICX, "IIcx ", "", MACH_CLASSII, &romvecs[0]},
	{MACH_MACSE30, "SE/30 ", "", MACH_CLASSII, &romvecs[0]},

/* The rest of the II series... */
	{MACH_MACIICI, "IIci ", "", MACH_CLASSIIci, &romvecs[4]},
	{MACH_MACIISI, "IIsi ", "", MACH_CLASSIIsi, &romvecs[2]},
	{MACH_MACIIVI, "IIvi ", "", MACH_CLASSIIvx, &romvecs[2]},
	{MACH_MACIIVX, "IIvx ", "", MACH_CLASSIIvx, &romvecs[2]},
	{MACH_MACIIFX, "IIfx ", "", MACH_CLASSIIfx, NULL},

/* The Centris/Quadra series. */
	{MACH_MACQ700, "Quadra", " 700 ", MACH_CLASSQ, &romvecs[4]},
	{MACH_MACQ900, "Quadra", " 900 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ950, "Quadra", " 950 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ800, "Quadra", " 800 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ650, "Quadra", " 650 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACC650, "Centris", " 650 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ605, "Quadra", " 605", MACH_CLASSQ, &romvecs[9]},
	{MACH_MACC610, "Centris", " 610 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACQ610, "Quadra", " 610 ", MACH_CLASSQ, &romvecs[6]},
	{MACH_MACC660AV, "Centris", " 660AV ", MACH_CLASSQ, &romvecs[7]},
	{MACH_MACQ840AV, "Quadra", " 840AV ", MACH_CLASSQ, &romvecs[7]},

/* The Powerbooks/Duos... */
	{MACH_MACPB100, "PowerBook", " 100 ", MACH_CLASSPB, &romvecs[1]},
	/* PB 100 has no MMU! */
	{MACH_MACPB140, "PowerBook", " 140 ", MACH_CLASSPB, &romvecs[1]},
	{MACH_MACPB145, "PowerBook", " 145 ", MACH_CLASSPB, &romvecs[1]},
	{MACH_MACPB160, "PowerBook", " 160 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB165, "PowerBook", " 165 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB165C, "PowerBook", " 165c ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB170, "PowerBook", " 170 ", MACH_CLASSPB, &romvecs[1]},
	{MACH_MACPB180, "PowerBook", " 180 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB180C, "PowerBook", " 180c ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB210, "PowerBook", " 210 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB230, "PowerBook", " 230 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB250, "PowerBook", " 250 ", MACH_CLASSPB, &romvecs[5]},
	{MACH_MACPB270, "PowerBook", " 270 ", MACH_CLASSPB, &romvecs[10]},

/* The Performas... */
	{MACH_MACP600, "Performa", " 600 ", MACH_CLASSIIvx, &romvecs[2]},
	{MACH_MACP460, "Performa", " 460 ", MACH_CLASSLC, &romvecs[3]},
	{MACH_MACP550, "Performa", " 550 ", MACH_CLASSLC, &romvecs[3]},

/* The LCs... */
	{MACH_MACLCII,  "LC", " II ",  MACH_CLASSLC, &romvecs[3]},
	{MACH_MACLCIII, "LC", " III ", MACH_CLASSLC, &romvecs[3]},
	{MACH_MACLC475, "LC", " 475 ", MACH_CLASSLC, &romvecs[3]},
	{MACH_MACLC520, "LC", " 520 ", MACH_CLASSLC, &romvecs[3]},
	{MACH_MACLC575, "LC", " 575 ", MACH_CLASSLC, &romvecs[3]},
	{MACH_MACCCLASSIC, "Color Classic ", "", MACH_CLASSLC, &romvecs[3]},
/* Does this belong here? */
	{MACH_MACCLASSICII, "Classic", " II ", MACH_CLASSLC, &romvecs[3]},

/* The hopeless ones... */
	{MACH_MACTV, "TV ", "", MACH_CLASSH, NULL},

/* The unknown one and the end... */
	{0, "Unknown", "", MACH_CLASSII, NULL},
	{0, NULL, NULL, 0, NULL},
};				/* End of cpu_models[] initialization. */

/*
 * Missing Mac Models:
 *	PowerMac 6100
 *	PowerMac 7100
 *	PowerMac 8100
 *	PowerBook 540
 *	PowerBook 520
 *	PowerBook 150
 *	Duo 280
 *	Quadra	630
 *	Performa 6000s
 * 	...?
 */

char    cpu_model[120];		/* for sysctl() */

int
mach_cputype()
{
	return (mac68k_machine.mach_processor);
}

static void
identifycpu()
{
	char   *proc;

	switch (mac68k_machine.mach_processor) {
	case MACH_68020:
		proc = ("(68020)");
		break;
	case MACH_68030:
		proc = ("(68030)");
		break;
	case MACH_68040:
		proc = ("(68040)");
		break;
	case MACH_PENTIUM:
	default:
		proc = ("(unknown processor)");
		break;
	}
	sprintf(cpu_model, "Apple Macintosh %s%s %s",
	    cpu_models[mac68k_machine.cpu_model_index].model_major,
	    cpu_models[mac68k_machine.cpu_model_index].model_minor,
	    proc);
	printf("%s\n", cpu_model);
}

static void
get_machine_info()
{
	char   *proc;
	int     i;

	for (i = 0; cpu_models[i].model_major; i++) {
		if (mac68k_machine.machineid == cpu_models[i].machineid)
			break;
	}

	if (cpu_models[i].model_major == NULL)
		i--;

	mac68k_machine.cpu_model_index = i;
}

/*
 * getenvvars: Grab a few useful variables
 */
extern void
getenvvars()
{
	extern u_long locore_dodebugmarks;
	extern u_long bootdev, videobitdepth, videosize;
	extern u_long end, esym;
	extern u_long macos_boottime, MacOSROMBase;
	extern long macos_gmtbias;
	int     root_scsi_id;

	root_scsi_id = getenv("ROOT_SCSI_ID");
	/*
         * For now, we assume that the boot device is off the first controller.
         */
	if (bootdev == 0) {
		bootdev = (root_scsi_id << 16) | 4;
	}

	if (boothowto == 0) {
		boothowto = getenv("SINGLE_USER");
	}

	/* These next two should give us mapped video & serial */
	/* We need these for pre-mapping graybars & echo, but probably */
	/* only on MacII or LC.  --  XXX */
	/* videoaddr = getenv("MACOS_VIDEO"); */
	/* sccaddr = getenv("MACOS_SCC"); */

	/*
         * The following are not in a structure so that they can be
         * accessed more quickly.
         */
	videoaddr = getenv("VIDEO_ADDR");
	videorowbytes = getenv("ROW_BYTES");
	videobitdepth = getenv("SCREEN_DEPTH");
	videosize = getenv("DIMENSIONS");

	/*
         * More misc stuff from booter.
         */
	mac68k_machine.machineid = getenv("MACHINEID");
	switch (mac68k_machine.mach_processor = getenv("PROCESSOR")) {
	case MACH_68040:
		mmutype = MMU_68040;
		break;
	default:;
	}
	mac68k_machine.mach_memsize = getenv("MEMSIZE");
	mac68k_machine.do_graybars = getenv("GRAYBARS");
	locore_dodebugmarks = mac68k_machine.do_graybars;
	mac68k_machine.serial_boot_echo = getenv("SERIALECHO");
	mac68k_machine.serial_console = getenv("SERIALCONSOLE");
	/* Should probably check this and fail if old */
	mac68k_machine.booter_version = getenv("BOOTERVER");

	/*
         * Get end of symbols for kernel debugging
         */
	esym = getenv("END_SYM");
#ifndef SYMTAB_SPACE
	if (esym == 0)
#endif
		esym = (long) &end;

	/* Get MacOS time */
	macos_boottime = getenv("BOOTTIME");

	/* Save GMT BIAS saved in Booter parameters dialog box */
	macos_gmtbias = getenv("GMTBIAS");

	/*
         * Save globals stolen from MacOS
         */

	ROMBase = (caddr_t) getenv("ROMBASE");
	if (ROMBase == (caddr_t) 0) {
		ROMBase = (caddr_t) ROMBASE;
	}
	MacOSROMBase = (unsigned long) ROMBase;
	TimeDBRA = getenv("TIMEDBRA");
	ADBDelay = (u_short) getenv("ADBDELAY");
	HwCfgFlags  = getenv("HWCFGFLAGS");
	HwCfgFlags2 = getenv("HWCFGFLAG2");
	HwCfgFlags3 = getenv("HWCFGFLAG3");
 	ADBReInit_JTBL = getenv("ADBREINIT_JTBL");
 	mrg_ADBIntrPtr = (caddr_t) getenv("ADBINTERRUPT");
}

struct cpu_model_info *current_mac_model;

/*
 * Sets a bunch of machine-specific variables
 */
void
setmachdep()
{
	static int firstpass = 1;
	struct cpu_model_info *cpui;

	/*
	 * First, set things that need to be set on the first pass only
	 * Ideally, we'd only call this once, but for some reason, the
	 * VIAs need interrupts turned off twice !?
	 */
	if (firstpass) {
		get_machine_info();

		load_addr = 0;
	}
	cpui = &(cpu_models[mac68k_machine.cpu_model_index]);
	current_mac_model = cpui;

	if (firstpass == 0)
		return;

	/*
	 * Set up current ROM Glue vectors
	 */
	if ((mac68k_machine.serial_console & 0x01) == 0)
		mrg_setvectors(cpui->rom_vectors);

	/*
	 * Set up any machine specific stuff that we have to before
	 * ANYTHING else happens
	 */
	switch (cpui->class) {	/* Base this on class of machine... */
	case MACH_CLASSII:
		VIA2 = 1;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */
		break;
	case MACH_CLASSPB:
		VIA2 = 1;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.sccClkConst = 115200;
		/* Disable everything but PM; we need it. */
		via_reg(VIA1, vIER) = 0x6f;	/* disable VIA1 int */
		/* Are we disabling something important? */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */
		break;
	case MACH_CLASSQ:
		VIA2 = 1;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi96 = 1;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, vIER) = 0x7f;	/* disable VIA2 int */
		break;
	case MACH_CLASSIIci:
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.sccClkConst = 122400;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSIIsi:
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.sccClkConst = 122400;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSIIvx:
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.sccClkConst = 122400;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	case MACH_CLASSLC:
		VIA2 = 0x13;
		IOBase = 0x50f00000;
		Via1Base = (volatile u_char *) IOBase;
		mac68k_machine.scsi80 = 1;
		mac68k_machine.sccClkConst = 115200;
		via_reg(VIA1, vIER) = 0x7f;	/* disable VIA1 int */
		via_reg(VIA2, rIER) = 0x7f;	/* disable RBV int */
		break;
	default:
	case MACH_CLASSH:
	case MACH_CLASSIIfx:
		break;
	}
	firstpass = 0;
}

/*
 * Set IO offsets.
 */
void
mac68k_set_io_offsets(base)
	vm_offset_t base;
{
	extern volatile u_char *sccA;
	extern volatile u_char *ASCBase;

	switch (current_mac_model->class) {
	case MACH_CLASSII:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4000;
		ASCBase = (volatile u_char *) base + 0x14000;
		SCSIBase = base;
		break;
	case MACH_CLASSPB:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4000;
		ASCBase = (volatile u_char *) base + 0x14000;
		SCSIBase = base;
		break;
	case MACH_CLASSQ:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0xc000;
		ASCBase = (volatile u_char *) base + 0x14000;
		SCSIBase = base;
		break;
	case MACH_CLASSIIci:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4000;
		ASCBase = (volatile u_char *) base + 0x14000;
		SCSIBase = base;
		break;
	case MACH_CLASSIIsi:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4000;
		ASCBase = (volatile u_char *) base + 0x14000;
		SCSIBase = base;
		break;
	case MACH_CLASSIIvx:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4000;
		ASCBase = (volatile u_char *) base + 0x14000;
		SCSIBase = base;
		break;
	case MACH_CLASSLC:
		Via1Base = (volatile u_char *) base;
		sccA = (volatile u_char *) base + 0x4000;
		ASCBase = (volatile u_char *) base + 0x14000;
		SCSIBase = base;
		break;
	default:
	case MACH_CLASSH:
	case MACH_CLASSIIfx:
		panic("Unknown/unsupported machine class.");
		break;
	}
	Via2Base = Via1Base + 0x2000 * VIA2;
}

#if GRAYBARS
static u_long gray_nextaddr = 0;

void
gray_bar()
{
	static int i = 0;
	static int flag = 0;

/* MF basic premise as I see it:
	1) Save the scratch regs as they are not saved by the compilier.
   	2) Check to see if we want gray bars, if so,
		display some lines of gray,
		a couple of lines of white(about 8),
		and loop to slow this down.
   	3) restore regs
*/

	asm("movl a0, sp@-");
	asm("movl a1, sp@-");
	asm("movl d0, sp@-");
	asm("movl d1, sp@-");

/* check to see if gray bars are turned off */
	if (mac68k_machine.do_graybars) {
		/* MF the 10*rowbytes/4 is done lots, but we want this to be
		 * slow */
		for (i = 0; i < 10 * videorowbytes / 4; i++)
			((u_long *) videoaddr)[gray_nextaddr++] = 0xaaaaaaaa;
		for (i = 0; i < 2 * videorowbytes / 4; i++)
			((u_long *) videoaddr)[gray_nextaddr++] = 0x00000000;
	}
	asm("movl sp@+, d1");
	asm("movl sp@+, d0");
	asm("movl sp@+, a1");
	asm("movl sp@+, a0");
}
#endif

/* in locore */
extern int get_pte(u_int addr, u_long pte[2], u_short * psr);

/*
 * LAK (7/24/94): given a logical address, puts the physical address
 *  in *phys and return 1, or returns 0 on failure.  This is intended
 *  to look through MacOS page tables.
 */

u_long
get_physical(u_int addr, u_long * phys)
{
	u_long  pte[2], ph;
	u_short psr;
	int     i, numbits;
	extern u_int macos_tc;

	i = get_pte(addr, pte, &psr);

	switch (i) {
	case -1:
		return 0;
	case 0:
		ph = pte[0] & 0xFFFFFF00;
		break;
	case 1:
		ph = pte[1] & 0xFFFFFF00;
		break;
	default:
		panic("get_physical(): bad get_pte()");
	}

	/*
	 * We must now figure out how many levels down we went and
	 * mask the bits appropriately -- the returned value may only
	 * be the upper n bits, and we've got to take the rest from addr.
	 */

	numbits = 0;
	psr &= 0x0007;		/* Number of levels we went */
	for (i = 0; i < psr; i++) {
		numbits += (macos_tc >> (12 - i * 4)) & 0x0f;
	}

	/*
	 * We have to take the most significant "numbits" from
	 * the returned value "ph", and the rest from our addr.
	 * Assume that the lower (32-numbits) bits of ph are
	 * already zero.  Also assume numbits != 0.  Also, notice
	 * that this is an addition, not an "or".
	 */

	*phys = ph + (addr & ((1 << (32 - numbits)) - 1));

	return 1;
}

void
check_video(id, limit, maxm)
	char    *id;
	u_long  limit, maxm;
{
	u_long  addr, phys;

	if (!get_physical(videoaddr, &phys))
		printf("get_mapping(): %s.  False start.\n", id);
	else {
		mac68k_vidlog = videoaddr;
		mac68k_vidphys = phys;
		mac68k_vidlen = 32768;
		addr = videoaddr + 32768;
		while (get_physical(addr, &phys)) {
			if ((phys - mac68k_vidphys)
			    != mac68k_vidlen)
				break;
			if (mac68k_vidlen + 32768 > limit) {
				printf("mapping: %s.  Does it never end?\n",
				    id);
				printf("               Forcing VRAM size ");
				printf("to a conservative %dK.\n", maxm / 1024);
				mac68k_vidlen = maxm;
				break;
			}
			mac68k_vidlen += 32768;
			addr += 32768;
		}
		printf("  %s internal video at addr 0x%x (phys 0x%x), ",
		    id, mac68k_vidlog, mac68k_vidphys);
		printf("len 0x%x.\n", mac68k_vidlen);
	}
}

/*
 * Find out how MacOS has mapped itself so we can do the same thing.
 * Returns the address of logical 0 so that locore can map the kernel
 * properly.
 */
u_int
get_mapping(void)
{
	int     i, same;
	u_long  addr, lastpage, phys, len;

	numranges = 0;
	for (i = 0; i < 8; i++) {
		low[i] = 0;
		high[i] = 0;
	}

	lastpage = get_top_of_ram();

	get_physical(0, &load_addr);

	for (addr = 0; addr <= lastpage && get_physical(addr, &phys);
	    addr += NBPG) {
		if (numranges > 0 && phys == high[numranges - 1]) {
			high[numranges - 1] += NBPG;
		} else {
			numranges++;
			low[numranges - 1] = phys;
			high[numranges - 1] = phys + NBPG;
		}
	}
#if 1
	printf("System RAM: %d bytes in %d pages.\n", addr, addr / NBPG);
	for (i = 0; i < numranges; i++) {
		printf("     Low = 0x%x, high = 0x%x\n", low[i], high[i]);
	}
#endif

	/*
	 * We should now look through all of NuBus space to find where
	 * the internal video is being mapped.  Just to be sure we handle
	 * all the cases, we simply map our NuBus space exactly how
	 * MacOS did it.  As above, we find a bunch of ranges that are
	 * contiguously mapped.  Since there are a lot of pages that
	 * are all mapped to 0, we handle that as a special case where
	 * the length is negative.  We search in increments of 32768
	 * because that's the page size that MacOS uses.
	 */

	nbnumranges = 0;
	for (i = 0; i < NBMAXRANGES; i++) {
		nbphys[i] = 0;
		nblog[i] = 0;
		nblen[i] = 0;
	}

	same = 0;
	for (addr = 0xF9000000; addr < 0xFF000000; addr += 32768) {
		if (!get_physical(addr, &phys)) {
			continue;
		}
		len = nblen[nbnumranges - 1];

		/* printf ("0x%x --> 0x%x\n", addr, phys); */
		if (nbnumranges > 0
		    && addr == nblog[nbnumranges - 1] + len
		    && phys == nbphys[nbnumranges - 1]) {	/* Same as last one */
			nblen[nbnumranges - 1] += 32768;
			same = 1;
		} else {
			if (nbnumranges > 0
			    && !same
			    && addr == nblog[nbnumranges - 1] + len
			    && phys == nbphys[nbnumranges - 1] + len) {
				nblen[nbnumranges - 1] += 32768;
			} else {
				if (same) {
					nblen[nbnumranges - 1] = -len;
					same = 0;
				}
				if (nbnumranges == NBMAXRANGES) {
					printf("get_mapping(): Too many NuBus "
					    "ranges.\n");
					break;
				}
				nbnumranges++;
				nblog[nbnumranges - 1] = addr;
				nbphys[nbnumranges - 1] = phys;
				nblen[nbnumranges - 1] = 32768;
			}
		}
	}
	if (same) {
		nblen[nbnumranges - 1] = -nblen[nbnumranges - 1];
		same = 0;
	}
#if 1
	printf("Non-system RAM (nubus, etc.):\n");
	for (i = 0; i < nbnumranges; i++) {
		printf("     Log = 0x%x, Phys = 0x%x, Len = 0x%x (%ud)\n",
		    nblog[i], nbphys[i], nblen[i], nblen[i]);
	}
#endif

	/*
	 * We must now find the logical address of internal video in the
	 * ranges we made above.  Internal video is at physical 0, but
	 * a lot of pages map there.  Instead, we look for the logical
	 * page that maps to 32768 and go back one page.
	 */

	for (i = 0; i < nbnumranges; i++) {
		if (nblen[i] > 0
		    && nbphys[i] <= 32768
		    && 32768 <= nbphys[i] + nblen[i]) {
			mac68k_vidlog = nblog[i] - nbphys[i];
			mac68k_vidlen = nblen[i] + nbphys[i];
			mac68k_vidphys = 0;
			break;
		}
	}
	if (i == nbnumranges) {
		if (0x60000000 <= videoaddr && videoaddr < 0x70000000) {
			/*
			 * Kludge for IIvx internal video (60b0 0000).
			 * PB 520 (6000 0000)
			 */
			check_video("IIvx/PB kludge", 1 * 1024 * 1024,
						   1 * 1024 * 1024);
		} else if (0x50F40000 <= videoaddr && videoaddr < 0x50FBFFFF) {
			/*
			 * Kludge for LC internal video
			 */
			check_video("LC video kludge", 512 * 1024, 512 * 1024);
		} else if (0x90000000 <= videoaddr && videoaddr < 0xF0000000) {
			/*
			 * Kludge for NuBus Superspace video
			 */
			check_video("NuBus Super kludge",
				    4 * 1024 * 1024, 1 * 1024 * 1024);
		} else {
			printf( "  no internal video at address 0 -- "
				"videoaddr is 0x%x.\n", videoaddr);
		}
	} else {
		printf("  Video address = 0x%x\n", videoaddr);
		printf("  Int video starts at 0x%x\n",
		    mac68k_vidlog);
		printf("  Length = 0x%x (%d) bytes\n",
		    mac68k_vidlen, mac68k_vidlen);
	}

	return low[0];		/* Return physical address of logical 0 */
}

/*
 * Debugging code for locore page-traversal routine.
 */
void
printstar(void)
{
	/*
	 * Be careful as we assume that no registers are clobbered
	 * when we call this from assembly.
	 */
	asm("movl a0, sp@-");
	asm("movl a1, sp@-");
	asm("movl d0, sp@-");
	asm("movl d1, sp@-");

	/* printf("*"); */

	asm("movl sp@+, d1");
	asm("movl sp@+, d0");
	asm("movl sp@+, a1");
	asm("movl sp@+, a0");
}
