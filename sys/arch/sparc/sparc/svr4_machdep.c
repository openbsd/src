/*	$OpenBSD: svr4_machdep.c,v 1.11 2002/07/20 19:24:57 art Exp $	*/
/*	$NetBSD: svr4_machdep.c,v 1.24 1997/07/29 10:04:45 fair Exp $	 */

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
#include <sys/exec.h>

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

static void svr4_getsiginfo(union svr4_siginfo *, int, u_long, int, caddr_t);

#ifdef DEBUG
extern int sigdebug;
extern int sigpid;
#define SDB_FOLLOW	0x01	/* XXX: dup from machdep.c */
#define SDB_KSTACK	0x02
#define SDB_FPSTATE	0x04
#endif

#ifdef DEBUG_SVR4
static void svr4_printcontext(const char *, struct svr4_ucontext *);

static void
svr4_printcontext(fun, uc)
	const char *fun;
	struct svr4_ucontext *uc;
{
	svr4_greg_t *r = uc->uc_mcontext.greg;
	struct svr4_sigaltstack *s = &uc->uc_stack;

	printf("%s at %p\n", fun, uc);

	printf("Regs: ");
	printf("PSR = 0x%x ", r[SVR4_SPARC_PSR]);
	printf("PC = 0x%x ",  r[SVR4_SPARC_PC]);
	printf("nPC = 0x%x ", r[SVR4_SPARC_nPC]);
	printf("Y = 0x%x ",   r[SVR4_SPARC_Y]);
	printf("G1 = 0x%x ",  r[SVR4_SPARC_G1]);
	printf("G2 = 0x%x ",  r[SVR4_SPARC_G2]);
	printf("G3 = 0x%x ",  r[SVR4_SPARC_G3]);
	printf("G4 = 0x%x ",  r[SVR4_SPARC_G4]);
	printf("G5 = 0x%x ",  r[SVR4_SPARC_G5]);
	printf("G6 = 0x%x ",  r[SVR4_SPARC_G6]);
	printf("G7 = 0x%x ",  r[SVR4_SPARC_G7]);
	printf("O0 = 0x%x ",  r[SVR4_SPARC_O0]);
	printf("O1 = 0x%x ",  r[SVR4_SPARC_O1]);
	printf("O2 = 0x%x ",  r[SVR4_SPARC_O2]);
	printf("O3 = 0x%x ",  r[SVR4_SPARC_O3]);
	printf("O4 = 0x%x ",  r[SVR4_SPARC_O4]);
	printf("O5 = 0x%x ",  r[SVR4_SPARC_O5]);
	printf("O6 = 0x%x ",  r[SVR4_SPARC_O6]);
	printf("O7 = 0x%x ",  r[SVR4_SPARC_O7]);
	printf("\n");

	printf("Signal Stack: sp %p, size %d, flags 0x%x\n",
	    s->ss_sp, s->ss_size, s->ss_flags);

	printf("Flags: 0x%lx\n", uc->uc_flags);
}
#endif

void
svr4_getcontext(p, uc, mask, oonstack)
	struct proc *p;
	struct svr4_ucontext *uc;
	int mask, oonstack;
{
	struct trapframe *tf = (struct trapframe *)p->p_md.md_tf;
	svr4_greg_t *r = uc->uc_mcontext.greg;
	struct svr4_sigaltstack *s = &uc->uc_stack;
#ifdef FPU_CONTEXT
	svr4_fregset_t *f = &uc->uc_mcontext.freg;
	struct fpstate *fps = p->p_md.md_fpstate;
#endif

	write_user_windows();
	if (rwindow_save(p))
		sigexit(p, SIGILL);

	bzero(uc, sizeof(struct svr4_ucontext));

	/*
	 * Get the general purpose registers
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

#ifdef FPU_CONTEXT
	/*
	 * Get the floating point registers
	 */
	bcopy(fps->fs_regs, f->fpu_regs, sizeof(fps->fs_regs));
	f->fp_nqsize = sizeof(struct fp_qentry);
	f->fp_nqel = fps->fs_qsize;
	f->fp_fsr = fps->fs_fsr;
	if (f->fp_q != NULL) {
		size_t sz = f->fp_nqel * f->fp_nqsize;
		if (sz > sizeof(fps->fs_queue)) {
#ifdef DIAGNOSTIC
			printf("getcontext: fp_queue too large\n");
#endif
			return;
		}
		if (copyout(fps->fs_queue, f->fp_q, sz) != 0) {
#ifdef DIAGNOSTIC
			printf("getcontext: copy of fp_queue failed %d\n",
			    error);
#endif
			return;
		}
	}
	f->fp_busy = 0;	/* XXX: How do we determine that? */
#endif

	/*
	 * Set the signal stack to something reasonable
	 */
	/* XXX: Don't really know what to do with this */
	s->ss_sp = (char *) ((r[SVR4_SPARC_SP] & ~0xfff) - 8192);
	s->ss_size = 8192;
	s->ss_flags = 0;

	/*
	 * Get the signal mask
	 */
	bsd_to_svr4_sigset(&mask, &uc->uc_sigmask);

	/*
	 * Get the flags
	 */
	uc->uc_flags = SVR4_UC_CPU|SVR4_UC_SIGMASK|SVR4_UC_STACK;

#ifdef DEBUG_SVR4
	svr4_printcontext("getcontext", uc);
#endif
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
	struct sigacts *psp = p->p_sigacts;
	register struct trapframe *tf;
	svr4_greg_t *r = uc->uc_mcontext.greg;
	struct svr4_sigaltstack *s = &uc->uc_stack;
	struct sigaltstack *sf = &psp->ps_sigstk;
	int mask;
#ifdef FPU_CONTEXT
	svr4_fregset_t *f = &uc->uc_mcontext.freg;
	struct fpstate *fps = p->p_md.md_fpstate;
#endif

#ifdef DEBUG_SVR4
	svr4_printcontext("setcontext", uc);
#endif

	write_user_windows();
	if (rwindow_save(p))
		sigexit(p, SIGILL);

#ifdef DEBUG
	if (sigdebug & SDB_FOLLOW)
		printf("svr4_setcontext: %s[%d], svr4_ucontext %p\n",
		    p->p_comm, p->p_pid, uc);
#endif

	tf = (struct trapframe *)p->p_md.md_tf;

	/*
	 * Restore register context.
	 */
	if (uc->uc_flags & SVR4_UC_CPU) {
		/*
		 * Only the icc bits in the psr are used, so it need not be
		 * verified.  pc and npc must be multiples of 4.  This is all
		 * that is required; if it holds, just do it.
		 */
		if (((r[SVR4_SPARC_PC] | r[SVR4_SPARC_nPC]) & 3) != 0) {
			printf("pc or npc are not multiples of 4!\n");
			return EINVAL;
		}

		/* take only psr ICC field */
		tf->tf_psr = (tf->tf_psr & ~PSR_ICC) |
		    (r[SVR4_SPARC_PSR] & PSR_ICC);
		tf->tf_pc = r[SVR4_SPARC_PC];
		tf->tf_npc = r[SVR4_SPARC_nPC];
		tf->tf_y = r[SVR4_SPARC_Y];

		/* Restore everything */
		tf->tf_global[1] = r[SVR4_SPARC_G1];
		tf->tf_global[2] = r[SVR4_SPARC_G2];
		tf->tf_global[3] = r[SVR4_SPARC_G3];
		tf->tf_global[4] = r[SVR4_SPARC_G4];
		tf->tf_global[5] = r[SVR4_SPARC_G5];
		tf->tf_global[6] = r[SVR4_SPARC_G6];
		tf->tf_global[7] = r[SVR4_SPARC_G7];

		tf->tf_out[0] = r[SVR4_SPARC_O0];
		tf->tf_out[1] = r[SVR4_SPARC_O1];
		tf->tf_out[2] = r[SVR4_SPARC_O2];
		tf->tf_out[3] = r[SVR4_SPARC_O3];
		tf->tf_out[4] = r[SVR4_SPARC_O4];
		tf->tf_out[5] = r[SVR4_SPARC_O5];
		tf->tf_out[6] = r[SVR4_SPARC_O6];
		tf->tf_out[7] = r[SVR4_SPARC_O7];
	}


#ifdef FPU_CONTEXT
	if (uc->uc_flags & SVR4_UC_FPU) {
		/*
		 * Set the floating point registers
		 */
		int error;
		size_t sz = f->fp_nqel * f->fp_nqsize;
		if (sz > sizeof(fps->fs_queue)) {
#ifdef DIAGNOSTIC
			printf("setcontext: fp_queue too large\n");
#endif
			return EINVAL;
		}
		bcopy(f->fpu_regs, fps->fs_regs, sizeof(fps->fs_regs));
		fps->fs_qsize = f->fp_nqel;
		fps->fs_fsr = f->fp_fsr;
		if (f->fp_q != NULL) {
			if ((error = copyin(f->fp_q, fps->fs_queue,
					    f->fp_nqel * f->fp_nqsize)) != 0) {
#ifdef DIAGNOSTIC
				printf("setcontext: copy of fp_queue failed\n");
#endif
				return error;
			}
		}
	}
#endif

	if (uc->uc_flags & SVR4_UC_STACK) {
		/*
		 * restore signal stack
		 */
		svr4_to_bsd_sigaltstack(s, sf);
	}

	if (uc->uc_flags & SVR4_UC_SIGMASK) {
		/*
		 * restore signal mask
		 */
		svr4_to_bsd_sigset(&uc->uc_sigmask, &mask);
		p->p_sigmask = mask & ~sigcantmask;
	}

	return EJUSTRETURN;
}

/*
 * map the native sig/type code into the svr4 siginfo as best we can
 */
static void
svr4_getsiginfo(si, sig, code, type, addr)
	union svr4_siginfo	*si;
	int			 sig;
	u_long			 code;
	int			 type;
	caddr_t			 addr;
{
	si->svr4_si_signo = bsd_to_svr4_sig[sig];
	si->svr4_si_errno = 0;
	si->svr4_si_addr  = addr;
	/*
	 * we can do this direct map as they are the same as all sparc
	 * architectures.
	 */
	si->svr4_si_trap = code;

	si->svr4_si_code = 0;
	si->svr4_si_trap = 0;

	switch (sig) {
	case SIGSEGV:
		switch (type) {
		case SEGV_ACCERR:
			si->svr4_si_code = SVR4_SEGV_ACCERR;
			si->svr4_si_trap = SVR4_T_PROTFLT;
			break;
		case SEGV_MAPERR:
			si->svr4_si_code = SVR4_SEGV_MAPERR;
			si->svr4_si_trap = SVR4_T_SEGNPFLT;
			break;
		}
		break;
	case SIGBUS:
		switch (type) {
		case BUS_ADRALN:
			si->svr4_si_code = SVR4_BUS_ADRALN;
			si->svr4_si_trap = SVR4_T_ALIGNFLT;
			break;
		}
		break;
	case SIGTRAP:
		switch (type) {
		case TRAP_BRKPT:
			si->svr4_si_code = SVR4_TRAP_BRKPT;
			si->svr4_si_trap = SVR4_T_BPTFLT;
			break;
		case TRAP_TRACE:
			si->svr4_si_code = SVR4_TRAP_TRACE;
			si->svr4_si_trap = SVR4_T_TRCTRAP;
			break;
		}
		break;
	case SIGEMT:
		switch (type) {
		}
		break;
	case SIGILL:
		switch (type) {
		case ILL_PRVOPC:
			si->svr4_si_code = SVR4_ILL_PRVOPC;
			si->svr4_si_trap = SVR4_T_PRIVINFLT;
			break;
		case ILL_BADSTK:
			si->svr4_si_code = SVR4_ILL_BADSTK;
			si->svr4_si_trap = SVR4_T_STKFLT;
			break;
		}
		break;
	case SIGFPE:
		switch (type) {
		case FPE_INTOVF:
			si->svr4_si_code = SVR4_FPE_INTOVF;
			si->svr4_si_trap = SVR4_T_DIVIDE;
			break;
		case FPE_FLTDIV:
			si->svr4_si_code = SVR4_FPE_FLTDIV;
			si->svr4_si_trap = SVR4_T_DIVIDE;
			break;
		case FPE_FLTOVF:
			si->svr4_si_code = SVR4_FPE_FLTOVF;
			si->svr4_si_trap = SVR4_T_DIVIDE;
			break;
		case FPE_FLTSUB:
			si->svr4_si_code = SVR4_FPE_FLTSUB;
			si->svr4_si_trap = SVR4_T_BOUND;
			break;
		case FPE_FLTINV:
			si->svr4_si_code = SVR4_FPE_FLTINV;
			si->svr4_si_trap = SVR4_T_FPOPFLT;
			break;
		}
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
svr4_sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	register struct proc *p = curproc;
	register struct trapframe *tf;
	struct svr4_sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack, oldsp, newsp, caddr;

	tf = (struct trapframe *)p->p_md.md_tf;
	oldsp = tf->tf_out[6];
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct svr4_sigframe *)(psp->ps_sigstk.ss_sp +
					      psp->ps_sigstk.ss_size);
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct svr4_sigframe *)oldsp;
	}

	/*
	 * Subtract off one signal frame and align.
	 */
	fp = (struct svr4_sigframe *) ((int) (fp - 1) & ~7);

	/*
	 * Build the argument list for the signal handler.
	 */
	svr4_getsiginfo(&frame.sf_si, sig, code, type, val.sival_ptr);
	svr4_getcontext(p, &frame.sf_uc, mask, oonstack);
	frame.sf_signum = frame.sf_si.svr4_si_signo;
	frame.sf_sip = &fp->sf_si;
	frame.sf_ucp = &fp->sf_uc;
	frame.sf_handler = catcher;

	DPRINTF(("svr4_sendsig signum=%d si = %x uc = %x handler = %x\n",
	         frame.sf_signum, frame.sf_sip,
		 frame.sf_ucp, frame.sf_handler));
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
	caddr = p->p_sigcode;
	tf->tf_global[1] = (int)catcher;

	tf->tf_pc = caddr;
	tf->tf_npc = caddr + 4;
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
		/*
		 * this list like gethrtime(3). To implement this
		 * correctly we need a timer that does not get affected
		 * adjtime(), or settimeofday(). For now we use
		 * microtime, and convert to nanoseconds...
		 */
		/*FALLTHROUGH*/
	case T_SVR4_GETHRVTIME:
		/*
		 * This is like gethrvtime(3). Since we don't have lwp
		 * we massage microtime() output
		 */
		{
			struct timeval  tv;

			microtime(&tv);
			tf->tf_out[0] = tv.tv_sec;
			tf->tf_out[1] = tv.tv_usec * 1000;
		}
		break;

	case T_SVR4_GETHRESTIME:
		{
			/* I assume this is like gettimeofday(3) */
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
