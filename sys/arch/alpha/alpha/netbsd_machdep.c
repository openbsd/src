/*	$OpenBSD: netbsd_machdep.c,v 1.2 1999/09/16 13:20:06 kstailey Exp $	*/

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
#include <sys/proc.h>
#include <sys/buf.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>
#include <sys/user.h>

#include <vm/vm.h>

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

static void netbsd_to_openbsd_sigcontext __P ((struct netbsd_sigcontext *,
	struct sigcontext *));

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
	struct sigcontext *scp, ksc;
	extern struct proc *fpcurproc;
	struct netbsd_sigcontext *nbscp, nbsc;

	nbscp = SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
	    printf("sigreturn: pid %d, scp %p\n", p->p_pid, scp);
#endif

	if (ALIGN(nbscp) != (u_int64_t)nbscp)
		return (EINVAL);

	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	if (useracc((caddr_t)nbscp, sizeof (*nbscp), B_WRITE) == 0 ||
	    copyin((caddr_t)nbscp, (caddr_t)&nbsc, sizeof (nbsc)))
		return (EINVAL);

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
