/*	$OpenBSD: sig_machdep.c,v 1.2 2004/02/16 15:40:00 miod Exp $	*/
/*	$NetBSD: sig_machdep.c,v 1.22 2003/10/08 00:28:41 thorpej Exp $	*/

/*
 * Copyright (c) 1994-1998 Mark Brinicombe.
 * Copyright (c) 1994 Brini.
 * All rights reserved.
 *
 * This code is derived from software written for Brini by Mark Brinicombe
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
 *	This product includes software developed by Mark Brinicombe
 *	for the NetBSD Project.
 * 4. The name of the company nor the name of the author may be used to
 *    endorse or promote products derived from this software without specific
 *    prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED
 * WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Machine dependant functions for kernel setup
 *
 * Created      : 17/09/94
 */

#include <sys/param.h>

#include <sys/mount.h>		/* XXX only needed by syscallargs.h */
#include <sys/proc.h>
#include <sys/signal.h>
#include <sys/signalvar.h>
#include <sys/syscallargs.h>
#include <sys/systm.h>
#include <sys/user.h>

#include <arm/armreg.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#ifndef acorn26
#include <arm/cpufunc.h>
#endif

static __inline struct trapframe *
process_frame(struct proc *p)
{

	return p->p_addr->u_pcb.pcb_tf;
}

void *getframe(struct proc *p, int sig, int *onstack);


/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user specified pc.
 */
void
sendsig(sig_t catcher, int sig, int returnmask, u_long code, int type,
   union sigval val)
{
	struct proc *p = curproc;
	struct trapframe *tf;
	struct sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;
	int onstack = 0;

	tf = process_frame(p);

	/* Do we need to jump onto the signal stack? */

	/* Allocate space for the signal handler context. */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		onstack = 1;
		fp = (struct sigframe *)((caddr_t)psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size);
	} else
		fp = (struct sigframe *)tf->tf_usr_sp;
	/* make room on the stack */
	fp--;

	/* make the stack aligned */
	fp = (void *)STACKALIGN(fp);

	/* Build stack frame for signal trampoline. */
	frame.sf_signum = sig;
	frame.sf_sip = NULL;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_handler = catcher;

	/* Save register context. */
	frame.sf_sc.sc_r0     = tf->tf_r0;
	frame.sf_sc.sc_r1     = tf->tf_r1;
	frame.sf_sc.sc_r2     = tf->tf_r2;
	frame.sf_sc.sc_r3     = tf->tf_r3;
	frame.sf_sc.sc_r4     = tf->tf_r4;
	frame.sf_sc.sc_r5     = tf->tf_r5;
	frame.sf_sc.sc_r6     = tf->tf_r6;
	frame.sf_sc.sc_r7     = tf->tf_r7;
	frame.sf_sc.sc_r8     = tf->tf_r8;
	frame.sf_sc.sc_r9     = tf->tf_r9;
	frame.sf_sc.sc_r10    = tf->tf_r10;
	frame.sf_sc.sc_r11    = tf->tf_r11;
	frame.sf_sc.sc_r12    = tf->tf_r12;
	frame.sf_sc.sc_usr_sp = tf->tf_usr_sp;
	frame.sf_sc.sc_usr_lr = tf->tf_usr_lr;
	frame.sf_sc.sc_svc_lr = tf->tf_svc_lr;
	frame.sf_sc.sc_pc     = tf->tf_pc;
	frame.sf_sc.sc_spsr   = tf->tf_spsr;

	/* Save signal stack. */
	frame.sf_sc.sc_onstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/* Save signal mask. */
	frame.sf_sc.sc_mask = returnmask;

	if (psp->ps_siginfo & sigmask(sig)) {
		frame.sf_sip = &fp->sf_si;
		initsiginfo(&frame.sf_si, sig, code, type, val);
	}

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.  We invoke the handler
	 * directly, only returning via the trampoline.  Note the
	 * trampoline version numbers are coordinated with machine-
	 * dependent code in libc.
	 */

	/*
	 * this was all in the switch below, seemed daft to duplicate it, if
	 * we do a new trampoline version it might change then
	 */
	tf->tf_r0 = sig;
	tf->tf_r1 = (int)frame.sf_sip;
	tf->tf_r2 = (int)frame.sf_scp;
	tf->tf_pc = (int)frame.sf_handler;
	tf->tf_usr_sp = (int)fp;
	
	tf->tf_usr_lr = (int)p->p_sigcode;
	/* XXX This should not be needed. */
	cpu_icache_sync_all();

	/* Remember that we're now on the signal stack. */
	if (onstack)
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
}

#if 0
void *
getframe(struct proc *p, int sig, int *onstack)
{
	struct sigctx *ctx = &p->p_sigctx;
	struct trapframe *tf = process_frame(l);

	/* Do we need to jump onto the signal stack? */
	*onstack = (ctx->ps_sigstk.ss_flags & (SS_DISABLE | SS_ONSTACK)) == 0
	    && (SIGACTION(p, sig).sa_flags & SA_ONSTACK) != 0;
	if (*onstack)
		return (char *)ctx->ps_sigstk.ss_sp + ctx->ps_sigstk.ss_size;
	return (void *)tf->tf_usr_sp;
}
#endif


/*
 * System call to cleanup state after a signal
 * has been taken.  Reset signal mask and
 * stack state from context left by sendsig (above).
 * Return to previous pc and psl as specified by
 * context left by sendsig. Check carefully to
 * make sure that the user has not modified the
 * psr to gain improper privileges or to cause
 * a machine fault.
 */

int
sys_sigreturn(struct proc *p, void *v, register_t *retval)
{
	struct sys_sigreturn_args /* {
		syscallarg(struct sigcontext *) sigcntxp;
	} */ *uap = v;
	struct sigcontext *scp, context;
	struct trapframe *tf;
	struct sigacts *psp = p->p_sigacts;

	/*
	 * we do a rather scary test in userland
	 */
	if (v == NULL)
		return (EFAULT);
	
	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, sigcntxp);
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/*
	 * Make sure the processor mode has not been tampered with and
	 * interrupts have not been disabled.
	 */
#ifdef __PROG32
	if ((context.sc_spsr & PSR_MODE) != PSR_USR32_MODE ||
	    (context.sc_spsr & (I32_bit | F32_bit)) != 0)
		return (EINVAL);
#else /* __PROG26 */
	if ((context.sc_pc & R15_MODE) != R15_MODE_USR ||
	    (context.sc_pc & (R15_IRQ_DISABLE | R15_FIQ_DISABLE)) != 0)
		return EINVAL;
#endif

	/* Restore register context. */
	tf = process_frame(p);
	tf->tf_r0    = context.sc_r0;
	tf->tf_r1    = context.sc_r1;
	tf->tf_r2    = context.sc_r2;
	tf->tf_r3    = context.sc_r3;
	tf->tf_r4    = context.sc_r4;
	tf->tf_r5    = context.sc_r5;
	tf->tf_r6    = context.sc_r6;
	tf->tf_r7    = context.sc_r7;
	tf->tf_r8    = context.sc_r8;
	tf->tf_r9    = context.sc_r9;
	tf->tf_r10   = context.sc_r10;
	tf->tf_r11   = context.sc_r11;
	tf->tf_r12   = context.sc_r12;
	tf->tf_usr_sp = context.sc_usr_sp;
	tf->tf_usr_lr = context.sc_usr_lr;
	tf->tf_svc_lr = context.sc_svc_lr;
	tf->tf_pc    = context.sc_pc;
	tf->tf_spsr  = context.sc_spsr;

	/* Restore signal stack. */
	if (context.sc_onstack & SS_ONSTACK)
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		psp->ps_sigstk.ss_flags &= ~SS_ONSTACK;

	/* Restore signal mask. */
#if 0
	(void) sigprocmask1(p, SIG_SETMASK, &context.sc_mask, 0);
#else
	p->p_sigmask = context.sc_mask & ~sigcantmask;
#endif

	return (EJUSTRETURN);
}

#if 0
void
cpu_getmcontext(p, mcp, flags)
	struct proc *p;
	mcontext_t *mcp;
	unsigned int *flags;
{
	struct trapframe *tf = process_frame(p);
	__greg_t *gr = mcp->__gregs;
	__greg_t ras_pc;

	/* Save General Register context. */
	gr[_REG_R0]   = tf->tf_r0;
	gr[_REG_R1]   = tf->tf_r1;
	gr[_REG_R2]   = tf->tf_r2;
	gr[_REG_R3]   = tf->tf_r3;
	gr[_REG_R4]   = tf->tf_r4;
	gr[_REG_R5]   = tf->tf_r5;
	gr[_REG_R6]   = tf->tf_r6;
	gr[_REG_R7]   = tf->tf_r7;
	gr[_REG_R8]   = tf->tf_r8;
	gr[_REG_R9]   = tf->tf_r9;
	gr[_REG_R10]  = tf->tf_r10;
	gr[_REG_R11]  = tf->tf_r11;
	gr[_REG_R12]  = tf->tf_r12;
	gr[_REG_SP]   = tf->tf_usr_sp;
	gr[_REG_LR]   = tf->tf_usr_lr;
	gr[_REG_PC]   = tf->tf_pc;
	gr[_REG_CPSR] = tf->tf_spsr;

	if ((ras_pc = (__greg_t)ras_lookup(l->l_proc,
	    (caddr_t) gr[_REG_PC])) != -1)
		gr[_REG_PC] = ras_pc;

	*flags |= _UC_CPU;

#ifdef ARMFPE
	/* Save Floating Point Register context. */
	arm_fpe_getcontext(p, (struct fpreg *)(void *)&mcp->fpregs);
	*flags |= _UC_FPU;
#endif
}

int
cpu_setmcontext(p, mcp, flags)
	struct proc *p;
	const mcontext_t *mcp;
	unsigned int flags;
{
	struct trapframe *tf = process_frame(l);
	__greg_t *gr = mcp->__gregs;

	if ((flags & _UC_CPU) != 0) {
		/* Restore General Register context. */
		/* Make sure the processor mode has not been tampered with. */
#ifdef PROG32
		if ((gr[_REG_CPSR] & PSR_MODE) != PSR_USR32_MODE ||
		    (gr[_REG_CPSR] & (I32_bit | F32_bit)) != 0)
			return (EINVAL);
#else /* PROG26 */
		if ((gr[_REG_PC] & R15_MODE) != R15_MODE_USR ||
		    (gr[_REG_PC] & (R15_IRQ_DISABLE | R15_FIQ_DISABLE)) != 0)
			return (EINVAL);
#endif

		tf->tf_r0     = gr[_REG_R0];
		tf->tf_r1     = gr[_REG_R1];
		tf->tf_r2     = gr[_REG_R2];
		tf->tf_r3     = gr[_REG_R3];
		tf->tf_r4     = gr[_REG_R4];
		tf->tf_r5     = gr[_REG_R5];
		tf->tf_r6     = gr[_REG_R6];
		tf->tf_r7     = gr[_REG_R7];
		tf->tf_r8     = gr[_REG_R8];
		tf->tf_r9     = gr[_REG_R9];
		tf->tf_r10    = gr[_REG_R10];
		tf->tf_r11    = gr[_REG_R11];
		tf->tf_r12    = gr[_REG_R12];
		tf->tf_usr_sp = gr[_REG_SP];
		tf->tf_usr_lr = gr[_REG_LR];
		tf->tf_pc     = gr[_REG_PC];
		tf->tf_spsr   = gr[_REG_CPSR];
	}

#ifdef ARMFPE
	if ((flags & _UC_FPU) != 0) {
		/* Restore Floating Point Register context. */
		arm_fpe_setcontext(p, (struct fpreg *)(void *)&mcp->__fpregs);
	}
#endif
	if (flags & _UC_SETSTACK)
		l->l_proc->p_sigctx.ps_sigstk.ss_flags |= SS_ONSTACK;
	if (flags & _UC_CLRSTACK)
		l->l_proc->p_sigctx.ps_sigstk.ss_flags &= ~SS_ONSTACK;

	return (0);
}
#endif
