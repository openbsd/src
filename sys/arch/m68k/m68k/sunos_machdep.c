/*	$NetBSD: sunos_machdep.c,v 1.8 1996/01/04 22:22:12 jtc Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1986, 1990 The Regents of the University of California.
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
 *
 * from: Utah $Hdr: machdep.c 1.63 91/04/24$
 *
 *	@(#)machdep.c	7.16 (Berkeley) 6/3/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/namei.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/filedesc.h>
#include <sys/ioctl.h>
#include <sys/mount.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/malloc.h>
#include <sys/buf.h>

#include <sys/syscallargs.h>
#include <compat/sunos/sunos_syscallargs.h>

#include <machine/reg.h>

#ifdef DEBUG
extern int sigdebug;
extern int sigpid;
#define SDB_FOLLOW      0x01
#define SDB_KSTACK      0x02
#define SDB_FPSTATE     0x04
#endif

/* sigh.. I guess it's too late to change now, but "our" sigcontext
   is plain vax, not very 68000 (ap, for example..) */
struct sunos_sigcontext {
	int 	sc_onstack;		/* sigstack state to restore */
	int	sc_mask;		/* signal mask to restore */
	int	sc_sp;			/* sp to restore */
	int	sc_pc;			/* pc to restore */
	int	sc_ps;			/* psl to restore */
};
struct sunos_sigframe {
	int	sf_signum;		/* signo for handler */
	int	sf_code;		/* additional info for handler */
	struct sunos_sigcontext *sf_scp;/* context pointer for handler */
	u_int	sf_addr;		/* even more info for handler */
	struct sunos_sigcontext sf_sc;	/* I don't know if that's what 
					   comes here */
};
/*
 * much simpler sendsig() for SunOS processes, as SunOS does the whole
 * context-saving in usermode. For now, no hardware information (ie.
 * frames for buserror etc) is saved. This could be fatal, so I take 
 * SIG_DFL for "dangerous" signals.
 */
void
sunos_sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	register struct proc *p = curproc;
	register struct sunos_sigframe *fp;
	struct sunos_sigframe kfp;
	register struct frame *frame;
	register struct sigacts *psp = p->p_sigacts;
	register short ft;
	int oonstack, fsize;

	frame = (struct frame *)p->p_md.md_regs;
	ft = frame->f_format;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * if this is a hardware fault (ft >= FMT9), sunos_sendsig
	 * can't currently handle it. Reset signal actions and
	 * have the process die unconditionally. 
	 */
	if (ft >= FMT9) {
		SIGACTION(p, sig) = SIG_DFL;
		p->p_sigignore &= ~sig;
		p->p_sigcatch &= ~sig;
		p->p_sigmask &= ~sig;
		psignal(p, sig);
		return;
	}

	/*
	 * Allocate and validate space for the signal handler
	 * context. Note that if the stack is in P0 space, the
	 * call to grow() is a nop, and the useracc() check
	 * will fail if the process has not already allocated
	 * the space with a `brk'.
	 */
	fsize = sizeof(struct sunos_sigframe);
	if ((psp->ps_flags & SAS_ALTSTACK) && oonstack == 0 &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct sunos_sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct sunos_sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else
		fp = (struct sunos_sigframe *)frame->f_regs[SP] - 1;
	if ((unsigned)fp <= USRSTACK - ctob(p->p_vmspace->vm_ssize)) 
		(void)grow(p, (unsigned)fp);
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sunos_sendsig(%d): sig %d ssp %x usp %x scp %x ft %d\n",
		       p->p_pid, sig, &oonstack, fp, &fp->sf_sc, ft);
#endif
	if (useracc((caddr_t)fp, fsize, B_WRITE) == 0) {
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("sunos_sendsig(%d): useracc failed on sig %d\n",
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
	 * Build the argument list for the signal handler.
	 */
	kfp.sf_signum = sig;
	kfp.sf_code = code;
	kfp.sf_scp = &fp->sf_sc;
	kfp.sf_addr = ~0;		/* means: not computable */

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	kfp.sf_sc.sc_onstack = oonstack;
	kfp.sf_sc.sc_mask = mask;
	kfp.sf_sc.sc_sp = frame->f_regs[SP];
	kfp.sf_sc.sc_pc = frame->f_pc;
	kfp.sf_sc.sc_ps = frame->f_sr;

	if (copyout(&kfp, fp, fsize) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */ 
	}

	frame->f_regs[SP] = (int)fp;
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sunos_sendsig(%d): sig %d scp %x sc_sp %x\n",
		       p->p_pid, sig, kfp.sf_sc.sc_sp);
#endif

	/* have the user-level trampoline code sort out what registers it
	   has to preserve. */
	frame->f_pc = (u_int) catcher;
#ifdef DEBUG
	if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
		printf("sunos_sendsig(%d): sig %d returns\n",
		       p->p_pid, sig);
#endif
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
int
sunos_sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct sunos_sys_sigreturn_args *uap = v;
	register struct sunos_sigcontext *scp;
	register struct frame *frame;
	struct sunos_sigcontext tsigc;

	scp = (struct sunos_sigcontext *) SCARG(uap, sigcntxp);
#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("sunos_sigreturn: pid %d, scp %x\n", p->p_pid, scp);
#endif
	if ((int)scp & 1)
		return (EINVAL);
	/*
	 * Test and fetch the context structure.
	 * We grab it all at once for speed.
	 */
	if (useracc((caddr_t)scp, sizeof(*scp), B_WRITE) == 0 ||
	    copyin((caddr_t)scp, (caddr_t)&tsigc, sizeof(tsigc)))
		return (EINVAL);
	scp = &tsigc;
	if ((scp->sc_ps & (PSL_MBZ|PSL_IPL|PSL_S)) != 0)
		return (EINVAL);
	/*
	 * Restore the user supplied information
	 */
	if (scp->sc_onstack & 1)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = scp->sc_mask &~ sigcantmask;
	frame = (struct frame *) p->p_md.md_regs;
	frame->f_regs[SP] = scp->sc_sp;
	frame->f_pc = scp->sc_pc;
	frame->f_sr = scp->sc_ps;

	return EJUSTRETURN;
}
