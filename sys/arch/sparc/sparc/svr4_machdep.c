/*	$NetBSD: svr4_machdep.c,v 1.12 1996/01/04 22:22:49 jtc Exp $	 */

/*
 * Copyright (c) 1994 Christos Zoulas
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
 * 3. The name of the author may not be used to endorse or promote products
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
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_ucontext.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>

#include <machine/cpu.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/trap.h>
#include <machine/svr4_machdep.h>

static void svr4_getsiginfo __P((union svr4_siginfo *, int, u_long, caddr_t));

#ifdef DEBUG
extern int sigdebug;
extern int sigpid;
#define SDB_FOLLOW	0x01	/* XXX: dup from machdep.c */
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

void
svr4_getcontext(p, uc, mask, oonstack)
	struct proc *p;
	struct svr4_ucontext *uc;
	int mask, oonstack;
{
	struct trapframe *tf = (struct trapframe *)p->p_md.md_tf;
	struct sigacts *psp = p->p_sigacts;
	svr4_greg_t *r = uc->uc_mcontext.greg;
	struct svr4_sigaltstack *s = &uc->uc_stack;
	struct sigaltstack *sf = &psp->ps_sigstk;

	bzero(uc, sizeof(struct svr4_ucontext));

	/*
	 * Set the general purpose registers
	 */
	r[SVR4_SPARC_PSR] = tf->tf_psr;
	r[SVR4_SPARC_PC] = tf->tf_pc;
	r[SVR4_SPARC_nPC] = tf->tf_npc;
	r[SVR4_SPARC_Y] = tf->tf_y;
	r[SVR4_SPARC_G1] = tf->tf_global[1];
	r[SVR4_SPARC_G2] = tf->tf_global[2];
	r[SVR4_SPARC_G3] = tf->tf_global[3];
	r[SVR4_SPARC_G4] = tf->tf_global[4];
	r[SVR4_SPARC_G5] = tf->tf_global[5];
	r[SVR4_SPARC_G6] = tf->tf_global[6];
	r[SVR4_SPARC_G7] = tf->tf_global[7];
	r[SVR4_SPARC_O0] = tf->tf_out[0];
	r[SVR4_SPARC_O1] = tf->tf_out[1];
	r[SVR4_SPARC_O2] = tf->tf_out[2];
	r[SVR4_SPARC_O3] = tf->tf_out[3];
	r[SVR4_SPARC_O4] = tf->tf_out[4];
	r[SVR4_SPARC_O5] = tf->tf_out[5];
	r[SVR4_SPARC_O6] = tf->tf_out[6];
	r[SVR4_SPARC_O7] = tf->tf_out[7];

	/*
	 * Set the signal stack
	 */
	bsd_to_svr4_sigaltstack(sf, s);

	/*
	 * Set the signal mask
	 */
	bsd_to_svr4_sigset(&mask, &uc->uc_sigmask);

	/*
	 * Set the flags
	 */
	uc->uc_flags = SVR4_UC_ALL;
}


/*
 * Set to ucontext specified.
 * has been taken.  Reset signal mask and
 * stack state from context.
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psl to gain improper privileges or to cause
 * a machine fault.
 * This is almost like sigreturn() and it shows.
 */
int
svr4_setcontext(p, uc)
	struct proc *p;
	struct svr4_ucontext *uc;
{
	struct sigcontext *scp, context;
	struct sigacts *psp = p->p_sigacts;
	register struct trapframe *tf;
	svr4_greg_t *r = uc->uc_mcontext.greg;
	struct svr4_sigaltstack *s = &uc->uc_stack;
	struct sigaltstack *sf = &psp->ps_sigstk;
	int mask;

	/*
	 * XXX:
	 * Should we check the value of flags to determine what to restore?
	 */

	write_user_windows();
	if (rwindow_save(p))
		sigexit(p, SIGILL);

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("svr4_setcontext: %s[%d], svr4_ucontext %x\n",
		    p->p_comm, p->p_pid, uc);
#endif

	if ((int)uc & 3 || useracc((caddr_t)uc, sizeof *uc, B_WRITE) == 0)
		return EINVAL;

	tf = (struct trapframe *)p->p_md.md_tf;

	/*
	 * restore signal stack
	 */
	svr4_to_bsd_sigaltstack(s, sf);

	/*
	 * restore signal mask
	 */
	svr4_to_bsd_sigset(&uc->uc_sigmask, &mask);
	p->p_sigmask = mask & ~sigcantmask;

	/*
	 * Restore register context.
	 */
	/*
	 * Only the icc bits in the psr are used, so it need not be
	 * verified.  pc and npc must be multiples of 4.  This is all
	 * that is required; if it holds, just do it.
	 */
	if (((r[SVR4_SPARC_PC] | r[SVR4_SPARC_nPC]) & 3) != 0)
		return EINVAL;

	/* take only psr ICC field */
	tf->tf_psr = (tf->tf_psr & ~PSR_ICC) | (r[SVR4_SPARC_PSR] & PSR_ICC);
	tf->tf_pc = r[SVR4_SPARC_PC];
	tf->tf_npc = r[SVR4_SPARC_nPC];
	tf->tf_y = r[SVR4_SPARC_Y];
	tf->tf_out[0] = r[SVR4_SPARC_O0];
	tf->tf_out[6] = r[SVR4_SPARC_O6];
#if 0
	/* I don't think that we need to restore those */
	tf->tf_global[1] = r[SVR4_SPARC_G1];
	tf->tf_global[2] = r[SVR4_SPARC_G2];
	tf->tf_global[3] = r[SVR4_SPARC_G3];
	tf->tf_global[4] = r[SVR4_SPARC_G4];
	tf->tf_global[5] = r[SVR4_SPARC_G5];
	tf->tf_global[6] = r[SVR4_SPARC_G6];
	tf->tf_global[7] = r[SVR4_SPARC_G7];

	tf->tf_out[1] = r[SVR4_SPARC_O1];
	tf->tf_out[2] = r[SVR4_SPARC_O2];
	tf->tf_out[3] = r[SVR4_SPARC_O3];
	tf->tf_out[4] = r[SVR4_SPARC_O4];
	tf->tf_out[5] = r[SVR4_SPARC_O5];
	tf->tf_out[7] = r[SVR4_SPARC_O7];
#endif

	return EJUSTRETURN;
}

/*
 * map the trap code into the svr4 siginfo as best we can
 */
static void
svr4_getsiginfo(si, sig, code, addr)
	union svr4_siginfo	*si;
	int			 sig;
	u_long			 code;
	caddr_t			 addr;
{
	si->si_signo = bsd_to_svr4_sig[sig];
	si->si_errno = 0;
	si->si_addr  = addr;
	/*
	 * we can do this direct map as they are the same as all sparc
	 * architectures.
	 */
	si->si_trap = code;

	switch (code) {
	case T_TEXTFAULT:
		si->si_code = SVR4_BUS_ADRALN;
		break;

	case T_ILLINST:
		si->si_code = SVR4_ILL_ILLOPC;
		break;

	case T_PRIVINST:
		si->si_code = SVR4_ILL_PRVOPC;
		break;

	case T_FPDISABLED:
		si->si_code = SVR4_FPE_FLTINV;
		break;

	case T_ALIGN:
		si->si_code = SVR4_BUS_ADRALN;
		break;

	case T_FPE:
		si->si_code = SVR4_FPE_FLTINV;
		break;

	case T_DATAFAULT:
		si->si_code = SVR4_BUS_ADRALN;
		break;

	case T_TAGOF:
		si->si_code = SVR4_EMT_TAGOVF;
		break;

	case T_CPDISABLED:
		si->si_code = SVR4_FPE_FLTINV;
		break;

	case T_CPEXCEPTION:
		si->si_code = SVR4_FPE_FLTINV;
		break;

	case T_DIV0:
		si->si_code = SVR4_FPE_INTDIV;
		break;

	case T_INTOF:
		si->si_code = SVR4_FPE_INTOVF;
		break;

	case T_BREAKPOINT:
		si->si_code = SVR4_TRAP_BRKPT;
		break;

	/*
	 * XXX - hardware traps with unknown code
	 */
	case T_WINOF:
	case T_WINUF:
	case T_L1INT:
	case T_L2INT:
	case T_L3INT:
	case T_L4INT:
	case T_L5INT:
	case T_L6INT:
	case T_L7INT:
	case T_L8INT:
	case T_L9INT:
	case T_L10INT:
	case T_L11INT:
	case T_L12INT:
	case T_L13INT:
	case T_L14INT:
	case T_L15INT:
		si->si_code = 0;
		break;

	/*
	 * XXX - software traps with unknown code
	 */
	case T_SUN_SYSCALL:
	case T_FLUSHWIN:
	case T_CLEANWIN:
	case T_RANGECHECK:
	case T_FIXALIGN:
	case T_SVR4_SYSCALL:
	case T_BSD_SYSCALL:
	case T_KGDB_EXEC:
		si->si_code = 0;
		break;

	default:
		si->si_code = 0;
#ifdef DIAGNOSTIC
		printf("sig %d code %d\n", sig, code);
		panic("svr4_getsiginfo");
#endif
		break;
	}
}

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine. After the handler is
 * done svr4 will call setcontext for us
 * with the user context we just set up, and we
 * will return to the user pc, psl.
 */
void
svr4_sendsig(catcher, sig, mask, code)
	sig_t catcher;
	int sig, mask;
	u_long code;
{
	register struct proc *p = curproc;
	register struct trapframe *tf;
	struct svr4_sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack, oldsp, newsp, addr;
	svr4_greg_t* r = frame.sf_uc.uc_mcontext.greg;
	extern char svr4_sigcode[], svr4_esigcode[];


	tf = (struct trapframe *)p->p_md.md_tf;
	oldsp = tf->tf_out[6];
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct svr4_sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct svr4_sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct svr4_sigframe *)oldsp;
	}

	/* 
	 * Build the argument list for the signal handler.
	 */
	svr4_getsiginfo(&frame.sf_si, sig, code, (caddr_t) tf->tf_pc);
	svr4_getcontext(p, &frame.sf_uc, mask, oonstack);
	frame.sf_signum = frame.sf_si.si_signo;
	frame.sf_sip = &fp->sf_si;
	frame.sf_ucp = &fp->sf_uc;
	frame.sf_handler = catcher;
	/*
	 * Modify the signal context to be used by sigreturn.
	 */
	frame.sf_uc.uc_mcontext.greg[SVR4_SPARC_SP] = oldsp;

	newsp = (int)fp - sizeof(struct rwindow);
	write_user_windows();

	if (rwindow_save(p) || copyout(&frame, fp, sizeof(frame)) != 0 ||
	    suword(&((struct rwindow *)newsp)->rw_in[6], oldsp)) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
#ifdef DEBUG
		if ((sigdebug & SDB_KSTACK) && p->p_pid == sigpid)
			printf("svr4_sendsig: window save or copyout error\n");
#endif
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	addr = (int)PS_STRINGS - (svr4_esigcode - svr4_sigcode);
	tf->tf_global[1] = (int)catcher;
	tf->tf_pc = addr;
	tf->tf_npc = addr + 4;
	tf->tf_out[6] = newsp;
}


#define	ADVANCE (n = tf->tf_npc, tf->tf_pc = n, tf->tf_npc = n + 4)
int
svr4_trap(type, p)
	int	type;
	struct proc *p;
{
	int n;
	struct trapframe *tf = p->p_md.md_tf;
	extern struct emul emul_svr4;

	if (p->p_emul != &emul_svr4)
		return 0;

	switch (type) {
	case T_SVR4_GETCC:
		uprintf("T_SVR4_GETCC\n");
		break;

	case T_SVR4_SETCC:
		uprintf("T_SVR4_SETCC\n");
		break;

	case T_SVR4_GETPSR:
		tf->tf_out[0] = tf->tf_psr;
		break;

	case T_SVR4_SETPSR:
		uprintf("T_SVR4_SETPSR\n");
		break;

	case T_SVR4_GETHRTIME:
		uprintf("T_SVR4_GETHRTIME\n");
		break;

	case T_SVR4_GETHRVTIME:
		uprintf("T_SVR4_GETHRVTIME\n");
		break;

	case T_SVR4_GETHRESTIME:
		{
			struct timeval  tv;

			microtime(&tv);
			tf->tf_out[0] = tv.tv_sec;
			tf->tf_out[1] = tv.tv_usec;
		}
		break;

	default:
		return 0;
	}

	ADVANCE;
	return 1;
}

/*
 */
int
svr4_sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_sysarch_args *uap = v;

	switch (SCARG(uap, op)) {
	default:
		printf("(sparc) svr4_sysarch(%d)\n", SCARG(uap, op));
		return EINVAL;
	}
}
