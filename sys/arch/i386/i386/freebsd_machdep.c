/*	$OpenBSD: freebsd_machdep.c,v 1.13 2002/07/20 19:24:56 art Exp $	*/
/*	$NetBSD: freebsd_machdep.c,v 1.10 1996/05/03 19:42:05 christos Exp $	*/

/*-
 * Copyright (c) 1993, 1994, 1995, 1996 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1992 Terrence R. Lambert.
 * Copyright (c) 1982, 1987, 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 *	@(#)machdep.c	7.4 (Berkeley) 6/3/91
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/signalvar.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/exec.h>
#include <sys/mount.h>

#include <uvm/uvm_extern.h>

#include <machine/cpufunc.h>
#include <machine/npx.h>
#include <machine/reg.h>
#include <machine/vm86.h>
#include <machine/freebsd_machdep.h>

#include <compat/freebsd/freebsd_signal.h>
#include <compat/freebsd/freebsd_syscallargs.h>
#include <compat/freebsd/freebsd_exec.h>
#include <compat/freebsd/freebsd_ptrace.h>

/*
 * signal support
 */

/*
 * Send an interrupt to process.
 *
 * Stack is set up to allow sigcode stored
 * in u. to call routine, followed by kcall
 * to sigreturn routine below.  After sigreturn
 * resets the signal mask, the stack, and the
 * frame pointer, it returns to the user
 * specified pc, psl.
 */
void
freebsd_sendsig(catcher, sig, mask, code, type, val)
	sig_t catcher;
	int sig, mask;
	u_long code;
	int type;
	union sigval val;
{
	register struct proc *p = curproc;
	register struct trapframe *tf;
	struct freebsd_sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;

	/* 
	 * Build the argument list for the signal handler.
	 */
	frame.sf_signum = sig;

	tf = p->p_md.md_regs;
	oonstack = psp->ps_sigstk.ss_flags & SS_ONSTACK;

	/*
	 * Allocate space for the signal handler context.
	 */
	if ((psp->ps_flags & SAS_ALTSTACK) && !oonstack &&
	    (psp->ps_sigonstack & sigmask(sig))) {
		fp = (struct freebsd_sigframe *)(psp->ps_sigstk.ss_sp +
		    psp->ps_sigstk.ss_size - sizeof(struct freebsd_sigframe));
		psp->ps_sigstk.ss_flags |= SS_ONSTACK;
	} else {
		fp = (struct freebsd_sigframe *)tf->tf_esp - 1;
	}

	frame.sf_code = code;
	frame.sf_scp = &fp->sf_sc;
	frame.sf_addr = (char *)rcr2();
	frame.sf_handler = catcher;

	/*
	 * Build the signal context to be used by sigreturn.
	 */
	frame.sf_sc.sc_onstack = oonstack;
	frame.sf_sc.sc_mask = mask;
#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		frame.sf_sc.sc_es = tf->tf_vm86_es;
		frame.sf_sc.sc_ds = tf->tf_vm86_ds;
		frame.sf_sc.sc_eflags = get_vflags(p);
	} else
#endif
	{
		frame.sf_sc.sc_es = tf->tf_es;
		frame.sf_sc.sc_ds = tf->tf_ds;
		frame.sf_sc.sc_eflags = tf->tf_eflags;
	}
	frame.sf_sc.sc_edi = tf->tf_edi;
	frame.sf_sc.sc_esi = tf->tf_esi;
	frame.sf_sc.sc_ebp = tf->tf_ebp;
	frame.sf_sc.sc_isp = 0; /* don't have to pass kernel sp to user. */
	frame.sf_sc.sc_ebx = tf->tf_ebx;
	frame.sf_sc.sc_edx = tf->tf_edx;
	frame.sf_sc.sc_ecx = tf->tf_ecx;
	frame.sf_sc.sc_eax = tf->tf_eax;
	frame.sf_sc.sc_eip = tf->tf_eip;
	frame.sf_sc.sc_cs = tf->tf_cs;
	frame.sf_sc.sc_esp = tf->tf_esp;
	frame.sf_sc.sc_ss = tf->tf_ss;

	if (copyout(&frame, fp, sizeof(frame)) != 0) {
		/*
		 * Process has trashed its stack; give it an illegal
		 * instruction to halt it in its tracks.
		 */
		sigexit(p, SIGILL);
		/* NOTREACHED */
	}

	/*
	 * Build context to run handler in.
	 */
	tf->tf_es = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_ds = GSEL(GUDATA_SEL, SEL_UPL);
	tf->tf_eip = p->p_sigcode;
	tf->tf_cs = GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_eflags &= ~(PSL_T|PSL_VM|PSL_AC);
	tf->tf_esp = (int)fp;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);
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
int
freebsd_sys_sigreturn(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct freebsd_sys_sigreturn_args /* {
		syscallarg(struct freebsd_sigcontext *) scp;
	} */ *uap = v;
	struct freebsd_sigcontext *scp, context;
	register struct trapframe *tf;

	tf = p->p_md.md_regs;

	/*
	 * The trampoline code hands us the context.
	 * It is unsafe to keep track of it ourselves, in the event that a
	 * program jumps out of a signal handler.
	 */
	scp = SCARG(uap, scp);
	if (copyin((caddr_t)scp, &context, sizeof(*scp)) != 0)
		return (EFAULT);

	/*
	 * Restore signal context.
	 */
#ifdef VM86
	if (context.sc_eflags & PSL_VM) {
		tf->tf_vm86_es = context.sc_es;
		tf->tf_vm86_ds = context.sc_ds;
		set_vflags(p, context.sc_eflags);
	} else
#endif
	{
		/*
		 * Check for security violations.  If we're returning to
		 * protected mode, the CPU will validate the segment registers
		 * automatically and generate a trap on violations.  We handle
		 * the trap, rather than doing all of the checking here.
		 */
		if (((context.sc_eflags ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
		    !USERMODE(context.sc_cs, context.sc_eflags))
			return (EINVAL);

		tf->tf_es = context.sc_es;
		tf->tf_ds = context.sc_ds;
		tf->tf_eflags = context.sc_eflags;
	}
	tf->tf_edi = context.sc_edi;
	tf->tf_esi = context.sc_esi;
	tf->tf_ebp = context.sc_ebp;
	/* FreeBSD's context.sc_isp is useless. (`popal' ignores it.) */
	tf->tf_ebx = context.sc_ebx;
	tf->tf_edx = context.sc_edx;
	tf->tf_ecx = context.sc_ecx;
	tf->tf_eax = context.sc_eax;
	tf->tf_eip = context.sc_eip;
	tf->tf_cs = context.sc_cs;
	tf->tf_esp = context.sc_esp;
	tf->tf_ss = context.sc_ss;

	if (context.sc_onstack & 01)
		p->p_sigacts->ps_sigstk.ss_flags |= SS_ONSTACK;
	else
		p->p_sigacts->ps_sigstk.ss_flags &= ~SS_ONSTACK;
	p->p_sigmask = context.sc_mask & ~sigcantmask;

	return (EJUSTRETURN);
}


/*
 * freebsd_ptrace(2) support
 */

void
netbsd_to_freebsd_ptrace_regs(nregs, nfpregs, fregs)
	struct reg *nregs;
	struct fpreg *nfpregs;
	struct freebsd_ptrace_reg *fregs;
{
	struct save87 *nframe = (struct save87 *)nfpregs;

	fregs->freebsd_ptrace_regs.tf_es = nregs->r_es;
	fregs->freebsd_ptrace_regs.tf_ds = nregs->r_ds;
	fregs->freebsd_ptrace_regs.tf_edi = nregs->r_edi;
	fregs->freebsd_ptrace_regs.tf_esi = nregs->r_esi;
	fregs->freebsd_ptrace_regs.tf_ebp = nregs->r_ebp;
	fregs->freebsd_ptrace_regs.tf_isp = 0;
	fregs->freebsd_ptrace_regs.tf_ebx = nregs->r_ebx;
	fregs->freebsd_ptrace_regs.tf_edx = nregs->r_edx;
	fregs->freebsd_ptrace_regs.tf_ecx = nregs->r_ecx;
	fregs->freebsd_ptrace_regs.tf_eax = nregs->r_eax;
	fregs->freebsd_ptrace_regs.tf_trapno = 0;

	fregs->freebsd_ptrace_regs.tf_err = 0;
	fregs->freebsd_ptrace_regs.tf_eip = nregs->r_eip;
	fregs->freebsd_ptrace_regs.tf_cs = nregs->r_cs;
	fregs->freebsd_ptrace_regs.tf_eflags = nregs->r_eflags;

	fregs->freebsd_ptrace_regs.tf_esp = nregs->r_esp;
	fregs->freebsd_ptrace_regs.tf_ss = nregs->r_ss;

	fregs->freebsd_ptrace_fpregs.sv_env =
		*(struct freebsd_env87 *)&nframe->sv_env;
	bcopy(nframe->sv_ac, fregs->freebsd_ptrace_fpregs.sv_ac,
	      sizeof(fregs->freebsd_ptrace_fpregs.sv_ac));
	fregs->freebsd_ptrace_fpregs.sv_ex_sw = 
		nframe->sv_ex_sw;
#if 0
	/*
	 * fortunately, sizeof(freebsd_save87) >= sizeof(save87)
	 */
#ifdef DIAGNOSTIC
	if (sizeof(fregs->freebsd_ptrace_fpregs.sv_pad) <
	    sizeof(nframe->sv_ex_tw) + sizeof(nframe->sv_pad)) {
		panic("netbsd_to_freebsd_ptrace_regs: %s",
		      "sizeof(freebsd_save87) >= sizeof(save87)");
	}
#endif
#endif
	bcopy(&nframe->sv_ex_tw, fregs->freebsd_ptrace_fpregs.sv_pad,
	      sizeof(nframe->sv_ex_tw));
#if 0
	bcopy(nframe->sv_pad,
	      (caddr_t)fregs->freebsd_ptrace_fpregs.sv_pad +
	      sizeof(nframe->sv_ex_tw),
	      sizeof(nframe->sv_pad));
	bzero((caddr_t)fregs->freebsd_ptrace_fpregs.sv_pad +
	      sizeof(nframe->sv_ex_tw) + sizeof(nframe->sv_pad),
	      sizeof(fregs->freebsd_ptrace_fpregs.sv_pad) -
	      sizeof(nframe->sv_ex_tw) - sizeof(nframe->sv_pad));
#endif
}

void
freebsd_to_netbsd_ptrace_regs(fregs, nregs, nfpregs)
	struct freebsd_ptrace_reg *fregs;
	struct reg *nregs;
	struct fpreg *nfpregs;
{
	struct save87 *nframe = (struct save87 *)nfpregs;

	nregs->r_es = fregs->freebsd_ptrace_regs.tf_es;
	nregs->r_ds = fregs->freebsd_ptrace_regs.tf_ds;
	nregs->r_edi = fregs->freebsd_ptrace_regs.tf_edi;
	nregs->r_esi = fregs->freebsd_ptrace_regs.tf_esi;
	nregs->r_ebp = fregs->freebsd_ptrace_regs.tf_ebp;
	nregs->r_ebx = fregs->freebsd_ptrace_regs.tf_ebx;
	nregs->r_edx = fregs->freebsd_ptrace_regs.tf_edx;
	nregs->r_ecx = fregs->freebsd_ptrace_regs.tf_ecx;
	nregs->r_eax = fregs->freebsd_ptrace_regs.tf_eax;

	nregs->r_eip = fregs->freebsd_ptrace_regs.tf_eip;
	nregs->r_cs = fregs->freebsd_ptrace_regs.tf_cs;
	nregs->r_eflags = fregs->freebsd_ptrace_regs.tf_eflags;

	nregs->r_esp = fregs->freebsd_ptrace_regs.tf_esp;
	nregs->r_ss = fregs->freebsd_ptrace_regs.tf_ss;

	nframe->sv_env =
		*(struct env87 *)&fregs->freebsd_ptrace_fpregs.sv_env;
	bcopy(fregs->freebsd_ptrace_fpregs.sv_ac, nframe->sv_ac,
	      sizeof(nframe->sv_ac));
	nframe->sv_ex_sw =
		fregs->freebsd_ptrace_fpregs.sv_ex_sw;
	/*
	 * fortunately, sizeof(freebsd_save87) >= sizeof(save87)
	 */
	bcopy(fregs->freebsd_ptrace_fpregs.sv_pad, &nframe->sv_ex_tw,
	      sizeof(nframe->sv_ex_tw));
#if 0
	bcopy((caddr_t)fregs->freebsd_ptrace_fpregs.sv_pad +
	      sizeof(nframe->sv_ex_tw),
	      nframe->sv_pad, sizeof(nframe->sv_pad));
#endif
}

/* random value, except FREEBSD_U_AR0_OFFSET..., FREEBSD_U_SAVEFP_OFFSET... */
#define	FREEBSD_REGS_OFFSET 0x2000

int
freebsd_ptrace_getregs(fregs, addr, datap)
	struct freebsd_ptrace_reg *fregs;
	caddr_t addr;
	register_t *datap;
{
	vm_offset_t offset = (vm_offset_t)addr;

	if (offset == FREEBSD_U_AR0_OFFSET) {
		*datap = FREEBSD_REGS_OFFSET + FREEBSD_USRSTACK;
		return 0;
	} else if (offset >= FREEBSD_REGS_OFFSET &&
		   offset <= FREEBSD_REGS_OFFSET + 
		      sizeof(fregs->freebsd_ptrace_regs)-sizeof(register_t)) {
		*datap = *(register_t *)&((caddr_t)&fregs->freebsd_ptrace_regs)
			[(vm_offset_t) addr - FREEBSD_REGS_OFFSET];
		return 0;
	} else if (offset >= FREEBSD_U_SAVEFP_OFFSET &&
		   offset <= FREEBSD_U_SAVEFP_OFFSET + 
		      sizeof(fregs->freebsd_ptrace_fpregs)-sizeof(register_t)){
		*datap= *(register_t *)&((caddr_t)&fregs->freebsd_ptrace_fpregs)
			[offset - FREEBSD_U_SAVEFP_OFFSET];
		return 0;
	}
#ifdef DIAGNOSTIC
	printf("freebsd_ptrace_getregs: *(0x%08lx)\n", offset);
#endif
	return EFAULT;
}

int
freebsd_ptrace_setregs(fregs, addr, data)
	struct freebsd_ptrace_reg *fregs;
	caddr_t addr;
	int data;
{
	vm_offset_t offset = (vm_offset_t)addr;

	if (offset >= FREEBSD_REGS_OFFSET &&
	    offset <= FREEBSD_REGS_OFFSET +
			sizeof(fregs->freebsd_ptrace_regs) - sizeof(int)) {
		*(int *)&((caddr_t)&fregs->freebsd_ptrace_regs)
			[offset - FREEBSD_REGS_OFFSET] = data;
		return 0;
	} else if (offset >= FREEBSD_U_SAVEFP_OFFSET &&
		   offset <= FREEBSD_U_SAVEFP_OFFSET + 
			sizeof(fregs->freebsd_ptrace_fpregs) - sizeof(int)) {
		*(int *)&((caddr_t)&fregs->freebsd_ptrace_fpregs)
			[offset - FREEBSD_U_SAVEFP_OFFSET] = data;
		return 0;
	}
#ifdef DIAGNOSTIC
	printf("freebsd_ptrace_setregs: *(0x%08lx) = 0x%08x\n", offset, data);
#endif
	return EFAULT;
}
