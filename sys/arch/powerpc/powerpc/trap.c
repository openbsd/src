/*	$OpenBSD: trap.c,v 1.8 1998/05/29 04:15:41 rahnds Exp $	*/
/*	$NetBSD: trap.c,v 1.3 1996/10/13 03:31:37 christos Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/ktrace.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>

#include <machine/cpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>

/* These definitions should probably be somewhere else				XXX */
#define	FIRSTARG	3		/* first argument is in reg 3 */
#define	NARGREG		8		/* 8 args are in registers */
#define	MOREARGS(sp)	((caddr_t)((int)(sp) + 8)) /* more args go here */

volatile int want_resched;

void
trap(frame)
	struct trapframe *frame;
{
	struct proc *p = curproc;
	int type = frame->exc;
	u_quad_t sticks;
	union sigval sv;

	if (frame->srr1 & PSL_PR) {
		type |= EXC_USER;
		sticks = p->p_sticks;
	}

	switch (type) {
	case EXC_TRC|EXC_USER:		/* Temporarily!					XXX */
		printf("TRC: %x\n", frame->srr0);
		break;
	case EXC_DSI:
		{
			vm_map_t map;
			vm_offset_t va;
			int ftype;
			faultbuf *fb;
			
			map = kernel_map;
			va = frame->dar;
			if ((va >> ADDR_SR_SHFT) == USER_SR) {
				sr_t user_sr;
				
				asm ("mfsr %0, %1"
				     : "=r"(user_sr) : "K"(USER_SR));
				va &= ADDR_PIDX | ADDR_POFF;
				va |= user_sr << ADDR_SR_SHFT;
				map = &p->p_vmspace->vm_map;
			}
			if (frame->dsisr & DSISR_STORE)
				ftype = VM_PROT_READ | VM_PROT_WRITE;
			else
				ftype = VM_PROT_READ;
			if (vm_fault(map, trunc_page(va), ftype, FALSE)
			    == KERN_SUCCESS)
				break;
			if (fb = p->p_addr->u_pcb.pcb_onfault) {
				frame->srr0 = (*fb)[0];
				frame->fixreg[1] = (*fb)[1];
				frame->cr = (*fb)[2];
				bcopy(&(*fb)[3], &frame->fixreg[13], 19);
				return;
			}
			map = kernel_map;
		}
printf("kern dsi on addr %x iar %x\n", frame->dar, frame->srr0);
		goto brain_damage;
	case EXC_DSI|EXC_USER:
		{
			int ftype, vftype;
			
			if (frame->dsisr & DSISR_STORE) {
				ftype = VM_PROT_READ | VM_PROT_WRITE;
				vftype = VM_PROT_WRITE;
			} else
				vftype = ftype = VM_PROT_READ;
			if (vm_fault(&p->p_vmspace->vm_map,
				     trunc_page(frame->dar), ftype, FALSE)
			    == KERN_SUCCESS)
				break;
printf("dsi on addr %x iar %x\n", frame->dar, frame->srr0);
			sv.sival_int = frame->dar;
			trapsignal(p, SIGSEGV, vftype, SEGV_MAPERR, sv);
		}
		break;
	case EXC_ISI|EXC_USER:
		{
			int ftype;
			
			ftype = VM_PROT_READ | VM_PROT_EXECUTE;
			if (vm_fault(&p->p_vmspace->vm_map,
				     trunc_page(frame->srr0), ftype, FALSE)
			    == KERN_SUCCESS)
				break;
		}
printf("isi iar %x\n", frame->srr0);
		sv.sival_int = frame->srr0;
		trapsignal(p, SIGSEGV, VM_PROT_EXECUTE, SEGV_MAPERR, sv);
		break;
	case EXC_SC|EXC_USER:
		{
			struct sysent *callp;
			size_t argsize;
			register_t code, error;
			register_t *params, rval[2];
			int nsys, n;
			register_t args[10];
			
			cnt.v_syscall++;
			
			nsys = p->p_emul->e_nsysent;
			callp = p->p_emul->e_sysent;
			
			code = frame->fixreg[0];
			params = frame->fixreg + FIRSTARG;
			
			switch (code) {
			case SYS_syscall:
				/*
				 * code is first argument,
				 * followed by actual args.
				 */
				code = *params++;
				break;
			case SYS___syscall:
				/*
				 * Like syscall, but code is a quad,
				 * so as to maintain quad alignment
				 * for the rest of the args.
				 */
				if (callp != sysent)
					break;
				params++;
				code = *params++;
				break;
			default:
				break;
			}
			if (code < 0 || code >= nsys)
				callp += p->p_emul->e_nosys;
			else
				callp += code;
			argsize = callp->sy_argsize;
			n = NARGREG - (params - (frame->fixreg + FIRSTARG));
			if (argsize > n * sizeof(register_t)) {
				bcopy(params, args, n * sizeof(register_t));
				if (error = copyin(MOREARGS(frame->fixreg[1]),
						   args + n,
						   argsize - n * sizeof(register_t))) {
#ifdef	KTRACE
					/* Can't get all the arguments! */
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p->p_tracep, code,
							   argsize, args);
#endif
					goto syscall_bad;
				}
				params = args;
			}
#ifdef	KTRACE
			if (KTRPOINT(p, KTR_SYSCALL))
				ktrsyscall(p->p_tracep, code, argsize, params);
#endif
			rval[0] = 0;
			rval[1] = frame->fixreg[FIRSTARG + 1];

#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, params);
#endif

			
			switch (error = (*callp->sy_call)(p, params, rval)) {
			case 0:
				frame->fixreg[0] = error;
				frame->fixreg[FIRSTARG] = rval[0];
				frame->fixreg[FIRSTARG + 1] = rval[1];
				frame->cr &= ~0x10000000;
				break;
			case ERESTART:
				/*
				 * Set user's pc back to redo the system call.
				 */
				frame->srr0 -= 4;
				break;
			case EJUSTRETURN:
				/* nothing to do */
				break;
			default:
syscall_bad:
				if (p->p_emul->e_errno)
					error = p->p_emul->e_errno[error];
				frame->fixreg[0] = error;
				frame->fixreg[FIRSTARG] = error;
				frame->cr |= 0x10000000;
				break;
			}
#ifdef SYSCALL_DEBUG
        scdebug_ret(p, code, error, rval); 
#endif  
#ifdef	KTRACE
			if (KTRPOINT(p, KTR_SYSRET))
				ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
		}
		break;

	case EXC_FPU|EXC_USER:
		if (fpuproc)
			save_fpu(fpuproc);
		fpuproc = p;
		enable_fpu(p);
		break;

	default:
	
brain_damage:
		printf("trap type %x at %x\n", type, frame->srr0);
/*
mpc_print_pci_stat();
*/
		panic("trap");

	case EXC_PGM|EXC_USER:
printf("pgm iar %x\n", frame->srr0);
		sv.sival_int = frame->srr0;
		trapsignal(p, SIGILL, 0, ILL_ILLOPC, sv);
		break;
	case EXC_AST|EXC_USER:
		/* This is just here that we trap */
		break;
	}

	astpending = 0;		/* we are about to do it */

	cnt.v_soft++;

	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}

	/* take pending signals */
	{
		int sig;

		while (sig = CURSIG(p))
			postsig(sig);
	}

	p->p_priority = p->p_usrpri;
	if (want_resched) {
		int s, sig;

		/*
		 * Since we are curproc, a clock interrupt could
		 * change our priority without changing run queues
		 * (the running process is not kept on a run queue).
		 * If this happened after we setrunqueue ourselves but
		 * before switch()'ed, we might not be on the queue
		 * indicated by our priority.
		 */
		s = splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while (sig = CURSIG(p))
			postsig(sig);
	}

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, frame->srr0,
			    (int)(p->p_sticks - sticks) * psratio);
	}
	/*
	 * If someone stole the fpu while we were away, disable it
	 */
	if (p != fpuproc)
		frame->srr1 &= ~PSL_FP;
	curpriority = p->p_priority;
}

void
child_return(p)
	struct proc *p;
{
	struct trapframe *tf = trapframe(p);

	tf->fixreg[0] = 0;
	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 1;
	tf->cr &= ~0x10000000;
	tf->srr1 &= ~PSL_FP;	/* Disable FPU, as we can't be fpuproc */
#ifdef	KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
	/* Profiling?							XXX */
	curpriority = p->p_priority;
}

static inline void
setusr(content)
	int content;
{
	asm volatile ("isync; mtsr %0,%1; isync"
		      :: "n"(USER_SR), "r"(content));
}

int
copyin(udaddr, kaddr, len)
	const void *udaddr;
	void *kaddr;
	size_t len;
{
	void *p;
	size_t l;
	faultbuf env;

	if (setfault(env))
		return EACCES;
	while (len > 0) {
		p = USER_ADDR + ((u_int)udaddr & ~SEGMENT_MASK);
		l = (USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		setusr(curpcb->pcb_pm->pm_sr[(u_int)udaddr >> ADDR_SR_SHFT]);
		bcopy(p, kaddr, l);
		udaddr += l;
		kaddr += l;
		len -= l;
	}
	curpcb->pcb_onfault = 0;
	return 0;
}

int
copyout(kaddr, udaddr, len)
	const void *kaddr;
	void *udaddr;
	size_t len;
{
	void *p;
	size_t l;
	faultbuf env;

	if (setfault(env))
		return EACCES;
	while (len > 0) {
		p = USER_ADDR + ((u_int)udaddr & ~SEGMENT_MASK);
		l = (USER_ADDR + SEGMENT_LENGTH) - p;
		if (l > len)
			l = len;
		setusr(curpcb->pcb_pm->pm_sr[(u_int)udaddr >> ADDR_SR_SHFT]);
		bcopy(kaddr, p, l);
		udaddr += l;
		kaddr += l;
		len -= l;
	}
	curpcb->pcb_onfault = 0;
	return 0;
}
