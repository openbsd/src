/*	$OpenBSD: fpu.c,v 1.23 2011/07/06 21:41:37 art Exp $	*/
/*	$NetBSD: fpu.c,v 1.1 2003/04/26 18:39:28 fvdl Exp $	*/

/*-
 * Copyright (c) 1994, 1995, 1998 Charles M. Hannum.  All rights reserved.
 * Copyright (c) 1990 William Jolitz.
 * Copyright (c) 1991 The Regents of the University of California.
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
 * 3. Neither the name of the University nor the names of its contributors
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
 *	@(#)npx.c	7.2 (Berkeley) 5/12/91
 */

/*
 * XXXfvdl update copyright notice. this started out as a stripped isa/npx.c
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/ioctl.h>
#include <sys/device.h>
#include <sys/vmmeter.h>
#include <sys/signalvar.h>

#include <uvm/uvm_extern.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>
#include <machine/cpufunc.h>
#include <machine/pcb.h>
#include <machine/trap.h>
#include <machine/specialreg.h>
#include <machine/fpu.h>
#include <machine/lock.h>

#include <dev/isa/isareg.h>
#include <dev/isa/isavar.h>

/*
 * We do lazy initialization and switching using the TS bit in cr0 and the
 * MDP_USEDFPU bit in mdproc.
 *
 * DNA exceptions are handled like this:
 *
 * 1) If there is no FPU, return and go to the emulator.
 * 2) If someone else has used the FPU, save its state into that process' PCB.
 * 3a) If MDP_USEDFPU is not set, set it and initialize the FPU.
 * 3b) Otherwise, reload the process' previous FPU state.
 *
 * When a process is created or exec()s, its saved cr0 image has the TS bit
 * set and the MDP_USEDFPU bit clear.  The MDP_USEDFPU bit is set when the
 * process first gets a DNA and the FPU is initialized.  The TS bit is turned
 * off when the FPU is used, and turned on again later when the process' FPU
 * state is saved.
 */

#define	fninit()		__asm("fninit")
#define fwait()			__asm("fwait")
#define fnclex()		__asm("fnclex")
#define	fxsave(addr)		__asm("fxsave %0" : "=m" (*addr))
#define	fxrstor(addr)		__asm("fxrstor %0" : : "m" (*addr))
#define	ldmxcsr(addr)		__asm("ldmxcsr %0" : : "m" (*addr))
#define fldcw(addr)		__asm("fldcw %0" : : "m" (*addr))
#define	clts()			__asm("clts")
#define	stts()			lcr0(rcr0() | CR0_TS)

void fpudna(struct cpu_info *);
static int x86fpflags_to_siginfo(u_int32_t);

/*
 * The mxcsr_mask for this host, taken from fxsave() on the primary CPU
 */
uint32_t	fpu_mxcsr_mask;

/*
 * Init the FPU.
 */
void
fpuinit(struct cpu_info *ci)
{
	lcr0(rcr0() & ~(CR0_EM|CR0_TS));
	fninit();
	if (fpu_mxcsr_mask == 0) {
		struct fxsave64 fx __attribute__((aligned(16)));

		bzero(&fx, sizeof(fx));
		fxsave(&fx);
		if (fx.fx_mxcsr_mask)
			fpu_mxcsr_mask = fx.fx_mxcsr_mask;
		else
			fpu_mxcsr_mask = __INITIAL_MXCSR_MASK__;
	}
	lcr0(rcr0() | (CR0_TS));
}

/*
 * Record the FPU state and reinitialize it all except for the control word.
 * Then generate a SIGFPE.
 *
 * Reinitializing the state allows naive SIGFPE handlers to longjmp without
 * doing any fixups.
 */

void
fputrap(struct trapframe *frame)
{
	struct proc *p = curcpu()->ci_fpcurproc;
	struct savefpu *sfp = &p->p_addr->u_pcb.pcb_savefpu;
	u_int32_t mxcsr, statbits;
	u_int16_t cw;
	int code;
	union sigval sv;

#ifdef DIAGNOSTIC
	/*
	 * At this point, fpcurproc should be curproc.  If it wasn't,
	 * the TS bit should be set, and we should have gotten a DNA exception.
	 */
	if (p != curproc)
		panic("fputrap: wrong proc");
#endif

	fxsave(sfp);
	if (frame->tf_trapno == T_XMM) {
		mxcsr = sfp->fp_fxsave.fx_mxcsr;
	  	statbits = mxcsr;
		mxcsr &= ~0x3f;
		ldmxcsr(&mxcsr);
	} else {
		fninit();
		fwait();
		cw = sfp->fp_fxsave.fx_fcw;
		fldcw(&cw);
		fwait();
		statbits = sfp->fp_fxsave.fx_fsw;
	}
	sfp->fp_ex_tw = sfp->fp_fxsave.fx_ftw;
	sfp->fp_ex_sw = sfp->fp_fxsave.fx_fsw;
	code = x86fpflags_to_siginfo (statbits);
	sv.sival_ptr = (void *)frame->tf_rip;	/* XXX - ? */
	KERNEL_LOCK();
	trapsignal(p, SIGFPE, frame->tf_err, code, sv);
	KERNEL_UNLOCK();
}

static int
x86fpflags_to_siginfo(u_int32_t flags)
{
        int i;
        static int x86fp_siginfo_table[] = {
                FPE_FLTINV, /* bit 0 - invalid operation */
                FPE_FLTRES, /* bit 1 - denormal operand */
                FPE_FLTDIV, /* bit 2 - divide by zero   */
                FPE_FLTOVF, /* bit 3 - fp overflow      */
                FPE_FLTUND, /* bit 4 - fp underflow     */
                FPE_FLTRES, /* bit 5 - fp precision     */
                FPE_FLTINV, /* bit 6 - stack fault      */
        };

        for (i = 0; i < nitems(x86fp_siginfo_table); i++) {
                if (flags & (1 << i))
                        return (x86fp_siginfo_table[i]);
        }
        /* punt if flags not set */
        return (FPE_FLTINV);
}

/*
 * Implement device not available (DNA) exception
 *
 * If we were the last process to use the FPU, we can simply return.
 * Otherwise, we save the previous state, if necessary, and restore our last
 * saved state.
 */
void
fpudna(struct cpu_info *ci)
{
	struct savefpu *sfp;
	struct proc *p;
	int s;

	if (ci->ci_fpsaving) {
		printf("recursive fpu trap; cr0=%x\n", rcr0());
		return;
	}

	s = splipi();

#ifdef MULTIPROCESSOR
	p = ci->ci_curproc;
#else
	p = curproc;
#endif

	/*
	 * Initialize the FPU state to clear any exceptions.  If someone else
	 * was using the FPU, save their state.
	 */
	if (ci->ci_fpcurproc != NULL && ci->ci_fpcurproc != p) {
		fpusave_cpu(ci, ci->ci_fpcurproc != &proc0);
		uvmexp.fpswtch++;
	}
	splx(s);

	if (p == NULL) {
		clts();
		return;
	}

	KDASSERT(ci->ci_fpcurproc == NULL);
#ifndef MULTIPROCESSOR
	KDASSERT(p->p_addr->u_pcb.pcb_fpcpu == NULL);
#else
	if (p->p_addr->u_pcb.pcb_fpcpu != NULL)
		fpusave_proc(p, 1);
#endif

	p->p_addr->u_pcb.pcb_cr0 &= ~CR0_TS;
	clts();

	s = splipi();
	ci->ci_fpcurproc = p;
	p->p_addr->u_pcb.pcb_fpcpu = ci;
	splx(s);

	sfp = &p->p_addr->u_pcb.pcb_savefpu;

	if ((p->p_md.md_flags & MDP_USEDFPU) == 0) {
		fninit();
		bzero(&sfp->fp_fxsave, sizeof(sfp->fp_fxsave));
		sfp->fp_fxsave.fx_fcw = __INITIAL_NPXCW__;
		sfp->fp_fxsave.fx_mxcsr = __INITIAL_MXCSR__;
		fxrstor(&sfp->fp_fxsave);
		p->p_md.md_flags |= MDP_USEDFPU;
	} else {
		static double	zero = 0.0;

		/*
		 * amd fpu does not restore fip, fdp, fop on fxrstor
		 * thus leaking other process's execution history.
		 */
		fnclex();
		__asm __volatile("ffree %%st(7)\n\tfld %0" : : "m" (zero));
		fxrstor(sfp);
	}
}


void
fpusave_cpu(struct cpu_info *ci, int save)
{
	struct proc *p;
	int s;

	KDASSERT(ci == curcpu());

	p = ci->ci_fpcurproc;
	if (p == NULL)
		return;

	if (save) {
#ifdef DIAGNOSTIC
		if (ci->ci_fpsaving != 0)
			panic("fpusave_cpu: recursive save!");
#endif
		 /*
		  * Set ci->ci_fpsaving, so that any pending exception will be
		  * thrown away.  (It will be caught again if/when the FPU
		  * state is restored.)
		  */
		clts();
		ci->ci_fpsaving = 1;
		fxsave(&p->p_addr->u_pcb.pcb_savefpu);
		ci->ci_fpsaving = 0;
	}

	stts();
	p->p_addr->u_pcb.pcb_cr0 |= CR0_TS;

	s = splipi();
	p->p_addr->u_pcb.pcb_fpcpu = NULL;
	ci->ci_fpcurproc = NULL;
	splx(s);
}

/*
 * Save p's FPU state, which may be on this processor or another processor.
 */
void
fpusave_proc(struct proc *p, int save)
{
	struct cpu_info *ci = curcpu();
	struct cpu_info *oci;

	KDASSERT(p->p_addr != NULL);

	oci = p->p_addr->u_pcb.pcb_fpcpu;
	if (oci == NULL)
		return;

#if defined(MULTIPROCESSOR)
	if (oci == ci) {
		int s = splipi();
		fpusave_cpu(ci, save);
		splx(s);
	} else {
		oci->ci_fpsaveproc = p;
		x86_send_ipi(oci,
	    	    save ? X86_IPI_SYNCH_FPU : X86_IPI_FLUSH_FPU);
		while (p->p_addr->u_pcb.pcb_fpcpu != NULL)
			SPINLOCK_SPIN_HOOK;
	}
#else
	KASSERT(ci->ci_fpcurproc == p);
	fpusave_cpu(ci, save);
#endif
}

void
fpu_kernel_enter(void)
{
	struct cpu_info	*ci = curcpu();
	uint32_t	 cw;
	int		 s;

	/*
	 * Fast path.  If the kernel was using the FPU before, there
	 * is no work to do besides clearing TS.
	 */
	if (ci->ci_fpcurproc == &proc0) {
		clts();
		return;
	}

	s = splipi();

	if (ci->ci_fpcurproc != NULL) {
		fpusave_cpu(ci, 1);
		uvmexp.fpswtch++;
	}

	/* Claim the FPU */
	ci->ci_fpcurproc = &proc0;

	splx(s);

	/* Disable DNA exceptions */
	clts();

	/* Initialize the FPU */
	fninit();
	cw = __INITIAL_NPXCW__;
	fldcw(&cw);
	cw = __INITIAL_MXCSR__;
	ldmxcsr(&cw);
}

void
fpu_kernel_exit(void)
{
	/* Enable DNA exceptions */
	stts();
}
