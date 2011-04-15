/*	$OpenBSD: sig_machdep.c,v 1.11 2011/04/15 04:52:39 guenther Exp $	*/
/*
 * Copyright (c) 1998, 1999, 2000, 2001 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/errno.h>

#include <machine/reg.h>

#include <uvm/uvm_extern.h>

struct sigstate {
	int		 ss_flags;	/* which of the following are valid */
	struct trapframe ss_frame;	/* original exception frame */
};

/*
 * WARNING: sigcode() in subr.s assumes sf_scp is the first field of the
 * sigframe.
 */
struct sigframe {
	struct sigcontext	*sf_scp;	/* context ptr for handler */
	struct sigcontext	 sf_sc;		/* actual context */
	siginfo_t		 sf_si;
};

#ifdef DEBUG
int sigdebug = 0;
int sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(sig_t catcher, int sig, int mask, unsigned long code, int type,
    union sigval val)
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigacts *psp = p->p_sigacts;
	struct sigframe *fp;
	int oonstack, fsize;
	struct sigframe sf;
	vaddr_t addr;

	tf = p->p_md.md_tf;
	oonstack = p->p_sigstk.ss_flags & SS_ONSTACK;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in data space, the
	 * call to grow() is a nop, and the copyout()
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	fsize = sizeof(struct sigframe);
	if ((p->p_sigstk.ss_flags & SS_DISABLE) == 0 &&
	    (p->p_sigstk.ss_flags & SS_ONSTACK) == 0 &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sigframe *)(p->p_sigstk.ss_sp +
					 p->p_sigstk.ss_size - fsize);
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sigframe *)(tf->tf_r[31] - fsize);

	/* make sure the frame is aligned on a 8 byte boundary */
	if (((vaddr_t)fp & 0x07) != 0)
		fp = (struct sigframe *)((vaddr_t)fp & ~0x07);

	if ((vaddr_t)fp <= USRSTACK - ptoa(p->p_vmspace->vm_ssize))
		(void)uvm_grow(p, (vaddr_t)fp);

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid))
		printf("sendsig(%d): sig %d ssp %x usp %x scp %x\n",
		       p->p_pid, sig, &oonstack, fp, &fp->sf_sc);
#endif
	/*
	 * Build the signal context to be used by sigreturn.
	 */
	sf.sf_scp = &fp->sf_sc;
	sf.sf_sc.sc_onstack = oonstack;
	sf.sf_sc.sc_mask = mask;

	if (psp->ps_siginfo & sigmask(sig)) {
		initsiginfo(&sf.sf_si, sig, code, type, val);
	}

	/*
	 * Copy the whole user context into signal context that we
	 * are building.
	 */
	bcopy((const void *)&tf->tf_regs, (void *)&sf.sf_sc.sc_regs,
	    sizeof(sf.sf_sc.sc_regs));

	if (copyout((caddr_t)&sf, (caddr_t)fp, sizeof sf)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Set up registers for the signal handler invocation.
	 */
	tf->tf_r[1] = p->p_sigcode;		/* return to sigcode */
	tf->tf_r[2] = sig;			/* first arg is signo */
	tf->tf_r[3] = psp->ps_siginfo & sigmask(sig) ? (vaddr_t)&fp->sf_si : 0;
	tf->tf_r[4] = (vaddr_t)&fp->sf_sc;
	tf->tf_r[31] = (vaddr_t)fp;
	addr = (vaddr_t)catcher;		/* and resume in the handler */
#ifdef M88100
	if (CPU_IS88100) {
		tf->tf_snip = (addr & NIP_ADDR) | NIP_V;
		tf->tf_sfip = (tf->tf_snip + 4) | FIP_V;
	}
#endif
#ifdef M88110
	if (CPU_IS88110) {
		tf->tf_exip = (addr & XIP_ADDR);
	}
#endif

#ifdef DEBUG
	if ((sigdebug & SDB_FOLLOW) ||
	    ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid))
		printf("sendsig(%d): sig %d returns\n", p->p_pid, sig);
#endif
}

/*
 * System call to cleanup state after a signal has been taken.  Reset signal
 * mask and stack state from context left by sendsig (above).  Return to
 * previous pc and psl as specified by context left by sendsig.  Check
 * carefully to make sure that the user has not modified the psl to gain
 * improper privileges or to cause a machine fault.
 */

/* ARGSUSED */
int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigreturn_args /* {
	   syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp;
	struct trapframe *tf;
	struct sigcontext ksc;

	scp = (struct sigcontext *)SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	if (((vaddr_t)scp & 3) != 0 ||
	    copyin((caddr_t)scp, (caddr_t)&ksc, sizeof(struct sigcontext)))
		return (EINVAL);

	tf = p->p_md.md_tf;
	scp = &ksc;

	if ((scp->sc_regs.epsr ^ tf->tf_regs.epsr) & PSR_USERSTATIC)
		return (EINVAL);

	bcopy((const void *)&scp->sc_regs, (caddr_t)&tf->tf_regs,
	    sizeof(scp->sc_regs));

	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & SS_ONSTACK)
		p->p_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask & ~sigcantmask;

	/*
	 * We really want to return to the instruction pointed to by
	 * the sigcontext.  However, due to the way exceptions work
	 * on 88110, returning EJUSTRETURN will cause m88110_syscall()
	 * to skip one instruction.  We avoid this by returning
	 * ERESTART, which will indeed cause the instruction pointed
	 * to by exip to be run.
	 */
	return (CPU_IS88100 ? EJUSTRETURN : ERESTART);
}
