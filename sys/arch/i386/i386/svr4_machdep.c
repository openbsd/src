/*	$OpenBSD: svr4_machdep.c,v 1.20 2003/05/13 03:49:04 art Exp $	*/
/*	$NetBSD: svr4_machdep.c,v 1.24 1996/05/03 19:42:26 christos Exp $	 */

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

#include <sys/syscallargs.h>
#include <compat/svr4/svr4_types.h>
#include <compat/svr4/svr4_ucontext.h>
#include <compat/svr4/svr4_syscallargs.h>
#include <compat/svr4/svr4_util.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/specialreg.h>
#include <machine/sysarch.h>
#include <machine/vm86.h>
#include <machine/svr4_machdep.h>

static void svr4_getsiginfo(union svr4_siginfo *, int, u_long, int, caddr_t);

void
svr4_getcontext(p, uc, mask, oonstack)
	struct proc *p;
	struct svr4_ucontext *uc;
	int mask, oonstack;
{
	struct trapframe *tf = p->p_md.md_regs;
	struct sigacts *psp = p->p_sigacts;
	svr4_greg_t *r = uc->uc_mcontext.greg;
	struct svr4_sigaltstack *s = &uc->uc_stack;
	struct sigaltstack *sf = &psp->ps_sigstk;

	bzero(uc, sizeof(struct svr4_ucontext));

	/*
	 * Set the general purpose registers
	 */
#ifdef VM86
	if (tf->tf_eflags & PSL_VM) {
		r[SVR4_X86_GS] = tf->tf_vm86_gs;
		r[SVR4_X86_FS] = tf->tf_vm86_fs;
		r[SVR4_X86_ES] = tf->tf_vm86_es;
		r[SVR4_X86_DS] = tf->tf_vm86_ds;
		r[SVR4_X86_EFL] = get_vflags(p);
	} else
#endif
	{
	        __asm("movl %%gs,%k0" : "=r" (r[SVR4_X86_GS]));
		__asm("movl %%fs,%k0" : "=r" (r[SVR4_X86_FS]));
		r[SVR4_X86_ES] = tf->tf_es;
		r[SVR4_X86_DS] = tf->tf_ds;
		r[SVR4_X86_EFL] = tf->tf_eflags;
	}
	r[SVR4_X86_EDI] = tf->tf_edi;
	r[SVR4_X86_ESI] = tf->tf_esi;
	r[SVR4_X86_EBP] = tf->tf_ebp;
	r[SVR4_X86_ESP] = tf->tf_esp;
	r[SVR4_X86_EBX] = tf->tf_ebx;
	r[SVR4_X86_EDX] = tf->tf_edx;
	r[SVR4_X86_ECX] = tf->tf_ecx;
	r[SVR4_X86_EAX] = tf->tf_eax;
	r[SVR4_X86_TRAPNO] = 0;
	r[SVR4_X86_ERR] = 0;
	r[SVR4_X86_EIP] = tf->tf_eip;
	r[SVR4_X86_CS] = tf->tf_cs;
	r[SVR4_X86_UESP] = 0;
	r[SVR4_X86_SS] = tf->tf_ss;

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

	/*
	 * XXX:
	 * Should we check the value of flags to determine what to restore?
	 * What to do with uc_link?
	 * What to do with floating point stuff?
	 * Should we bother with the rest of the registers that we
	 * set to 0 right now?
	 */

	tf = p->p_md.md_regs;

	/*
	 * Restore register context.
	 */
#ifdef VM86
	if (r[SVR4_X86_EFL] & PSL_VM) {
		tf->tf_vm86_gs = r[SVR4_X86_GS];
		tf->tf_vm86_fs = r[SVR4_X86_FS];
		tf->tf_vm86_es = r[SVR4_X86_ES];
		tf->tf_vm86_ds = r[SVR4_X86_DS];
		set_vflags(p, r[SVR4_X86_EFL]);
	} else
#endif
	{
		/*
		 * Check for security violations.  If we're returning to
		 * protected mode, the CPU will validate the segment registers
		 * automatically and generate a trap on violations.  We handle
		 * the trap, rather than doing all of the checking here.
		 */
		if (((r[SVR4_X86_EFL] ^ tf->tf_eflags) & PSL_USERSTATIC) != 0 ||
		    !USERMODE(r[SVR4_X86_CS], r[SVR4_X86_EFL]))
			return (EINVAL);

		/* %fs and %gs were restored by the trampoline. */
		tf->tf_es = r[SVR4_X86_ES];
		tf->tf_ds = r[SVR4_X86_DS];
		tf->tf_eflags = r[SVR4_X86_EFL];
	}
	tf->tf_edi = r[SVR4_X86_EDI];
	tf->tf_esi = r[SVR4_X86_ESI];
	tf->tf_ebp = r[SVR4_X86_EBP];
	tf->tf_ebx = r[SVR4_X86_EBX];
	tf->tf_edx = r[SVR4_X86_EDX];
	tf->tf_ecx = r[SVR4_X86_ECX];
	tf->tf_eax = r[SVR4_X86_EAX];
	tf->tf_eip = r[SVR4_X86_EIP];
	tf->tf_cs = r[SVR4_X86_CS];
	tf->tf_ss = r[SVR4_X86_SS];
	tf->tf_esp = r[SVR4_X86_ESP];

	/*
	 * restore signal stack
	 */
	svr4_to_bsd_sigaltstack(s, sf);

	/*
	 * restore signal mask
	 */
	svr4_to_bsd_sigset(&uc->uc_sigmask, &mask);
	p->p_sigmask = mask & ~sigcantmask;

	return EJUSTRETURN;
}


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
	struct pmap *pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	register struct trapframe *tf;
	struct svr4_sigframe *fp, frame;
	struct sigacts *psp = p->p_sigacts;
	int oonstack;

	tf = p->p_md.md_regs;
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
		fp = (struct svr4_sigframe *)tf->tf_esp - 1;
	}

	/* 
	 * Build the argument list for the signal handler.
	 * Notes:
	 * 	- we always build the whole argument list, even when we
	 *	  don't need to [when SA_SIGINFO is not set, we don't need
	 *	  to pass all sf_si and sf_uc]
	 *	- we don't pass the correct signal address [we need to
	 *	  modify many kernel files to enable that]
	 */

	svr4_getcontext(p, &frame.sf_uc, mask, oonstack);
	svr4_getsiginfo(&frame.sf_si, sig, code, type, val.sival_ptr);

	frame.sf_signum = frame.sf_si.svr4_si_signo;
	frame.sf_sip = &fp->sf_si;
	frame.sf_ucp = &fp->sf_uc;
	frame.sf_handler = catcher;
#ifdef DEBUG_SVR4
	printf("sig = %d, sip %p, ucp = %p, handler = %p\n", 
	       frame.sf_signum, frame.sf_sip, frame.sf_ucp, frame.sf_handler);
#endif

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
	tf->tf_cs = pmap->pm_hiexec > I386_MAX_EXE_ADDR ? 
	    GSEL(GUCODE1_SEL, SEL_UPL) : GSEL(GUCODE_SEL, SEL_UPL);
	tf->tf_eflags &= ~(PSL_T|PSL_VM|PSL_AC);
	tf->tf_esp = (int)fp;
	tf->tf_ss = GSEL(GUDATA_SEL, SEL_UPL);
}


/*
 * sysi86
 */
int
svr4_sys_sysarch(p, v, retval)
	struct proc *p;
	void *v;
	register_t *retval;
{
	struct svr4_sys_sysarch_args *uap = v;
	int error;
#ifdef USER_LDT
	caddr_t sg = stackgap_init(p->p_emul);
#endif
	*retval = 0;	/* XXX: What to do */

	switch (SCARG(uap, op)) {
	case SVR4_SYSARCH_FPHW:
		return 0;

	case SVR4_SYSARCH_DSCR:
#ifdef USER_LDT
		if (user_ldt_enable == 0)
			return (ENOSYS);
		else {
			struct i386_set_ldt_args sa, *sap;
			struct sys_sysarch_args ua;

			struct svr4_ssd ssd;
			union descriptor bsd;

			if ((error = copyin(SCARG(uap, a1), &ssd,
					    sizeof(ssd))) != 0) {
				printf("Cannot copy arg1\n");
				return error;
			}

			printf("s=%x, b=%x, l=%x, a1=%x a2=%x\n",
			       ssd.selector, ssd.base, ssd.limit,
			       ssd.access1, ssd.access2);

			/* We can only set ldt's for now. */
			if (!ISLDT(ssd.selector)) {
				printf("Not an ldt\n");
				return EPERM;
			}

			/* Oh, well we don't cleanup either */
			if (ssd.access1 == 0)
				return 0;

			bsd.sd.sd_lobase = ssd.base & 0xffffff;
			bsd.sd.sd_hibase = (ssd.base >> 24) & 0xff;

			bsd.sd.sd_lolimit = ssd.limit & 0xffff;
			bsd.sd.sd_hilimit = (ssd.limit >> 16) & 0xf;

			bsd.sd.sd_type = ssd.access1 & 0x1f;
			bsd.sd.sd_dpl =  (ssd.access1 >> 5) & 0x3;
			bsd.sd.sd_p = (ssd.access1 >> 7) & 0x1;

			bsd.sd.sd_xx = ssd.access2 & 0x3;
			bsd.sd.sd_def32 = (ssd.access2 >> 2) & 0x1;
			bsd.sd.sd_gran = (ssd.access2 >> 3)& 0x1;

			sa.start = IDXSEL(ssd.selector);
			sa.desc = stackgap_alloc(&sg, sizeof(union descriptor));
			sa.num = 1;
			sap = stackgap_alloc(&sg,
					     sizeof(struct i386_set_ldt_args));

			if ((error = copyout(&sa, sap, sizeof(sa))) != 0) {
				printf("Cannot copyout args\n");
				return error;
			}

			SCARG(&ua, op) = I386_SET_LDT;
			SCARG(&ua, parms) = (char *) sap;

			if ((error = copyout(&bsd, sa.desc, sizeof(bsd))) != 0) {
				printf("Cannot copyout desc\n");
				return error;
			}

			return sys_sysarch(p, &ua, retval);
		}
#endif
	case SVR4_SYSARCH_GOSF:
		{
				/* just as SCO Openserver 5.0 says */
			char features[] = {1,1,1,1,1,1,1,1,2,1,1,1};

			if ((error = copyout(features, SCARG(uap, a1),
					     sizeof(features))) != 0) {
				printf("Cannot copyout vector\n");
				return error;
			}

			return 0;
		}

	default:
		printf("svr4_sysarch(%d), a1 %p\n", SCARG(uap, op),
		       SCARG(uap, a1));
		return 0;
	}
}
