/*	$OpenBSD: netbsd_machdep.c,v 1.8 2002/07/20 19:24:55 art Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/kernel.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/user.h>
#include <machine/cpu.h>

#include <uvm/uvm_extern.h>

#include <compat/netbsd/netbsd_types.h>
#include <compat/netbsd/netbsd_signal.h>
#include <compat/netbsd/netbsd_syscallargs.h>

#include <machine/netbsd_machdep.h>
#include <machine/signal.h>
#include <machine/reg.h>

#ifdef DEBUG
extern int sigdebug;
extern int sigpid;
#define SDB_FOLLOW      0x01
#define SDB_KSTACK      0x02
#endif

static void netbsd_to_openbsd_sigcontext(struct netbsd_sigcontext *,
	struct sigcontext *);

static void openbsd_to_netbsd_sigcontext(struct sigcontext *,
	struct netbsd_sigcontext *);

static void
netbsd_to_openbsd_sigcontext(nbsc, obsc)
	struct netbsd_sigcontext *nbsc;
	struct sigcontext *obsc;
{
	bzero(obsc, sizeof(obsc));
	obsc->sc_onstack = nbsc->sc_onstack;
	bcopy(&nbsc->sc_mask.__bits[0], &obsc->sc_mask, sizeof(sigset_t));
	obsc->sc_pc = nbsc->sc_pc;
	obsc->sc_ps = nbsc->sc_ps;
	bcopy(nbsc->sc_regs, obsc->sc_regs, sizeof(obsc->sc_regs));
	obsc->sc_ownedfp = nbsc->sc_ownedfp;
	bcopy(nbsc->sc_fpregs, obsc->sc_fpregs, sizeof(obsc->sc_fpregs));
	obsc->sc_fpcr = nbsc->sc_fpcr;
	obsc->sc_fp_control = nbsc->sc_fp_control;
}

static void
openbsd_to_netbsd_sigcontext(obsc, nbsc)
	struct sigcontext *obsc;
	struct netbsd_sigcontext *nbsc;
{
	bzero(nbsc, sizeof(nbsc));
	nbsc->sc_onstack = obsc->sc_onstack;
	nbsc->__sc_mask13 = obsc->sc_mask;
	bcopy(&obsc->sc_mask, &nbsc->sc_mask.__bits[0], sizeof(sigset_t));
	nbsc->sc_pc = obsc->sc_pc;
	nbsc->sc_ps = obsc->sc_ps;
	bcopy(obsc->sc_regs, nbsc->sc_regs, sizeof(obsc->sc_regs));
	nbsc->sc_ownedfp = obsc->sc_ownedfp;
	bcopy(obsc->sc_fpregs, nbsc->sc_fpregs, sizeof(obsc->sc_fpregs));
	nbsc->sc_fpcr = obsc->sc_fpcr;
	nbsc->sc_fp_control = obsc->sc_fp_control;
}

/*
 * Send an interrupt to process.
 */
void
netbsd_sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	struct proc *p = curproc;
	struct sigcontext *scp, ksc;
	struct trapframe *frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack, fsize, rndfsize;
	extern char netbsd_sigcode[], netbsd_esigcode[];
	struct netbsd_sigcontext nbsc;

	frame = p->p_md.md_tf;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	fsize = sizeof(nbsc);
	rndfsize = ((fsize + 15) / 16) * 16;
	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		scp = (struct sigcontext *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - rndfsize);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		scp = (struct sigcontext *)(alpha_pal_rdusp() - rndfsize);
	if ((u_long)scp <= USRSTACK - ctob(p->p_vmspace->vm_ssize))
		(void)uvm_grow(p, (u_long)scp);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("netbsd_sendsig(%d): sig %d ssp %p usp %p scp %p\n",
		    p->p_pid, sig, &oonstack, alpha_pal_rdusp(), scp);
#endif
	if (uvm_useracc((caddr_t)scp, fsize, B_WRITE) == 0) {
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("netbsd_sendsig(%d): useracc failed on sig %d\n",
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

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	ksc.sc_onstack = oonstack;
	ksc.sc_mask = mask;
	ksc.sc_pc = frame->tf_regs[FRAME_PC];
	ksc.sc_ps = frame->tf_regs[FRAME_PS];

	/* copy the registers. */
	frametoreg(frame, (struct reg *)ksc.sc_regs);
	ksc.sc_regs[R_ZERO] = 0xACEDBADE;		/* magic number */
	ksc.sc_regs[R_SP] = alpha_pal_rdusp();

	/* save the floating-point state, if necessary, then copy it. */
	if (p == fpcurproc) {
		alpha_pal_wrfen(1);
		savefpstate(&p->p_addr->u_pcb.pcb_fp);
		alpha_pal_wrfen(0);
		fpcurproc = NULL;
	}
	ksc.sc_ownedfp = p->p_md.md_flags & MDP_FPUSED;
	bcopy(&p->p_addr->u_pcb.pcb_fp, (struct fpreg *)ksc.sc_fpregs,
	    sizeof(struct fpreg));
	ksc.sc_fp_control = 0;					/* XXX ? */
	bzero(ksc.sc_reserved, sizeof ksc.sc_reserved);		/* XXX */
	bzero(ksc.sc_xxx, sizeof ksc.sc_xxx);			/* XXX */

	/*
	 * copy the frame out to userland.
	 */
	openbsd_to_netbsd_sigcontext(&ksc, &nbsc);
	(void) copyout((caddr_t)&nbsc, (caddr_t)scp, sizeof(nbsc));
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("netbsd_sendsig(%d): sig %d scp %p code %lx\n",
		    p->p_pid, sig, scp, code);
#endif

	/*
	 * Set up the registers to return to netbsd_sigcode.
	 */
	frame->tf_regs[FRAME_PC] = p->p_sigcode;
	frame->tf_regs[FRAME_A0] = sig;
	frame->tf_regs[FRAME_A1] = code;
	frame->tf_regs[FRAME_A2] = (u_int64_t)scp;
	frame->tf_regs[FRAME_T12] = (u_int64_t)catcher;		/* t12 is pv */
	alpha_pal_wrusp((unsigned long)scp);

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("netbsd_sendsig(%d): pc %lx, catcher %lx\n", p->p_pid,
		    frame->tf_regs[FRAME_PC], frame->tf_regs[FRAME_T12]);
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("netbsd_sendsig(%d): sig %d returns\n", p->p_pid, sig);
#endif
}

/* ARGSUSED */
int
netbsd_sys___sigreturn14(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct netbsd_sys___sigreturn14_args /* {
		syscallarg(struct netbsd_sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext ksc;
	struct netbsd_sigcontext *nbscp, nbsc;

	nbscp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
	    printf("sigreturn: pid %d, nbscp %p\n", p->p_pid, nbscp);
#endif
	if (ALIGN(nbscp) != (u_int64_t)nbscp)
		return (EINVAL);

	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	if (uvm_useracc((caddr_t)nbscp, sizeof (*nbscp), B_WRITE) == 0 ||
	    copyin((caddr_t)nbscp, (caddr_t)&nbsc, sizeof (nbsc)))
		return (EFAULT);

	netbsd_to_openbsd_sigcontext(&nbsc, &ksc);

	if (ksc.sc_regs[R_ZERO] != 0xACEDBADE)		/* magic number */
		return (EINVAL);
	/*
	 * Restore the user-supplied information
	 */
	if (ksc.sc_onstack)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = ksc.sc_mask &~ sigcantmask;

	p->p_md.md_tf->tf_regs[FRAME_PC] = ksc.sc_pc;
	p->p_md.md_tf->tf_regs[FRAME_PS] =
	    (ksc.sc_ps | ALPHA_PSL_USERSET) & ~ALPHA_PSL_USERCLR;

	regtoframe((struct reg *)ksc.sc_regs, p->p_md.md_tf);
	alpha_pal_wrusp(ksc.sc_regs[R_SP]);

	/* XXX ksc.sc_ownedfp ? */
	if (p == fpcurproc)
		fpcurproc = NULL;
	bcopy((struct fpreg *)ksc.sc_fpregs, &p->p_addr->u_pcb.pcb_fp,
	    sizeof(struct fpreg));
	/* XXX ksc.sc_fp_control ? */

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sigreturn(%d): returns\n", p->p_pid);
#endif
	return (EJUSTRETURN);
}
