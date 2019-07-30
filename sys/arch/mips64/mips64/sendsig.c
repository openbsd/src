/*	$OpenBSD: sendsig.c,v 1.27 2016/05/21 00:56:43 deraadt Exp $ */

/*
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
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
 */
/*
 * Copyright (c) 2001 Opsycon AB  (www.opsycon.se)
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/exec.h>
#include <sys/mount.h>
#include <sys/syscallargs.h>

#include <uvm/uvm_extern.h>

#include <machine/regnum.h>
#include <mips64/mips_cpu.h>

/*
 * WARNING: code in locore.s assumes the layout shown for sf_signum
 * thru sf_handler so... don't screw with them!
 */
struct sigframe {
	int	sf_signum;		/* signo for handler */
	siginfo_t *sf_sip;		/* pointer to siginfo_t */
	struct	sigcontext *sf_scp;	/* context ptr for handler */
	sig_t	sf_handler;		/* handler addr for u_sigc */
	struct	sigcontext sf_sc;	/* actual context */
	siginfo_t sf_si;
};

#ifdef DEBUG
int sigdebug = 0;
pid_t sigpid = 0;
#define SDB_FOLLOW	0x01
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

/*
 * Send an interrupt to process.
 */
void
sendsig(sig_t catcher, int sig, int mask, u_long code, int type,
    union sigval val)
{
	struct cpu_info *ci = curcpu();
	struct proc *p = curproc;
	struct sigframe *fp;
	struct trapframe *regs;
	struct sigacts *psp = p->p_p->ps_sigacts;
	int fsize;
	struct sigcontext ksc;

	regs = p->p_md.md_regs;

	/*
	 * Allocate space for the signal handler context.
	 */
	fsize = sizeof(struct sigframe);
	if (!(psp->ps_siginfo & sigmask(sig)))
		fsize -= sizeof(siginfo_t);
	if ((p->p_sigstk.ss_flags & SS_DISABLE) == 0 &&
	    !sigonstack(regs->sp) && (psp->ps_sigonstack & sigmask(sig)))
		fp = (struct sigframe *)(p->p_sigstk.ss_sp +
					 p->p_sigstk.ss_size - fsize);
	else
		fp = (struct sigframe *)(regs->sp - fsize);
	/*
	 * Build the signal context to be used by sigreturn.
	 */
	bzero(&ksc, sizeof(ksc));
	ksc.sc_mask = mask;
	ksc.sc_pc = regs->pc;
	ksc.mullo = regs->mullo;
	ksc.mulhi = regs->mulhi;
	bcopy((caddr_t)&regs->ast, (caddr_t)&ksc.sc_regs[1],
		sizeof(ksc.sc_regs) - sizeof(register_t));
	ksc.sc_fpused = p->p_md.md_flags & MDP_FPUSED;
	if (ksc.sc_fpused) {
		/* if FPU has current state, save it first */
		if (p == ci->ci_fpuproc)
			save_fpu();

		bcopy((caddr_t)&p->p_md.md_regs->f0, (caddr_t)ksc.sc_fpregs,
			sizeof(ksc.sc_fpregs));
	}

	if (psp->ps_siginfo & sigmask(sig)) {
		siginfo_t si;

		initsiginfo(&si, sig, code, type, val);
		if (copyout((caddr_t)&si, (caddr_t)&fp->sf_si, sizeof si))
			goto bail;
	}

	ksc.sc_cookie = (long)&fp->sf_sc ^ p->p_p->ps_sigcookie;
	if (copyout((caddr_t)&ksc, (caddr_t)&fp->sf_sc, sizeof(ksc))) {
bail:
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}
	/*
	 * Build the argument list for the signal handler.
	 */
	regs->a0 = sig;
	regs->a1 = (psp->ps_siginfo & sigmask(sig)) ? (register_t)&fp->sf_si : 0;
	regs->a2 = (register_t)&fp->sf_sc;
	regs->a3 = (register_t)catcher;

	regs->pc = (register_t)catcher;
	regs->t9 = (register_t)catcher;
	regs->sp = (register_t)fp;

	regs->ra = p->p_p->ps_sigcode;
}

/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 */
/* ARGSUSED */
int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	struct cpu_info *ci = curcpu();
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext ksc, *scp = SCARG(uap, sigcntxp);
	struct trapframe *regs = p->p_md.md_regs;
	int error;

	if (PROC_PC(p) != p->p_p->ps_sigcoderet) {
		sigexit(p, SIGILL);
		return (EPERM);
	}

	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	error = copyin((caddr_t)scp, (caddr_t)&ksc, sizeof(ksc));
	if (error)
		return (error);

	if (ksc.sc_cookie != ((long)scp ^ p->p_p->ps_sigcookie)) {
		sigexit(p, SIGILL);
		return (EFAULT);
	}

	/* Prevent reuse of the sigcontext cookie */
	ksc.sc_cookie = 0;
	(void)copyout(&ksc.sc_cookie, (caddr_t)scp +
	    offsetof(struct sigcontext, sc_cookie), sizeof (ksc.sc_cookie));

	/*
	 * Restore the user supplied information
	 */
	p->p_sigmask = ksc.sc_mask &~ sigcantmask;
	regs->pc = ksc.sc_pc;
	regs->mullo = ksc.mullo;
	regs->mulhi = ksc.mulhi;
	regs->sr &= ~SR_COP_1_BIT;	/* Zap current FP state */
	if (p == ci->ci_fpuproc)
		ci->ci_fpuproc = NULL;
	bcopy((caddr_t)&ksc.sc_regs[1], (caddr_t)&regs->ast,
		sizeof(ksc.sc_regs) - sizeof(register_t));
	if (ksc.sc_fpused)
		bcopy((caddr_t)ksc.sc_fpregs, (caddr_t)&p->p_md.md_regs->f0,
			sizeof(ksc.sc_fpregs));
	return (EJUSTRETURN);
}
