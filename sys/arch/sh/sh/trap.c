/*	$OpenBSD: trap.c,v 1.1.1.1 2006/10/06 21:02:55 miod Exp $	*/
/*	$NetBSD: exception.c,v 1.32 2006/09/04 23:57:52 uwe Exp $	*/
/*	$NetBSD: syscall.c,v 1.6 2006/03/07 07:21:50 thorpej Exp $	*/

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc. All rights reserved.
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the University of Utah, and William Jolitz.
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
 *	@(#)trap.c	7.4 (Berkeley) 5/13/91
 */

/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the University of Utah, and William Jolitz.
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
 *	@(#)trap.c	7.4 (Berkeley) 5/13/91
 */

/*
 * SH3 Trap and System call handling
 *
 * T.Horiuchi 1998.06.8
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/pool.h>
#include <sys/user.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#include <sys/syscall.h>

#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include "systrace.h"
#if NSYSTRACE > 0
#include <dev/systrace.h>
#endif

#include <uvm/uvm_extern.h>

#include <sh/cpu.h>
#include <sh/mmu.h>
#include <sh/trap.h>
#include <sh/userret.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif

const char * const exp_type[] = {
	"--",					/* 0x000 (reset vector) */
	"--",					/* 0x020 (reset vector) */
	"TLB miss/invalid (load)",		/* 0x040 EXPEVT_TLB_MISS_LD */
	"TLB miss/invalid (store)",		/* 0x060 EXPEVT_TLB_MISS_ST */
	"initial page write",			/* 0x080 EXPEVT_TLB_MOD */
	"TLB protection violation (load)",	/* 0x0a0 EXPEVT_TLB_PROT_LD */
	"TLB protection violation (store)",	/* 0x0c0 EXPEVT_TLB_PROT_ST */
	"address error (load)",			/* 0x0e0 EXPEVT_ADDR_ERR_LD */
	"address error (store)",		/* 0x100 EXPEVT_ADDR_ERR_ST */
	"FPU",					/* 0x120 EXPEVT_FPU */
	"--",					/* 0x140 (reset vector) */
	"unconditional trap (TRAPA)",		/* 0x160 EXPEVT_TRAPA */
	"reserved instruction code exception",	/* 0x180 EXPEVT_RES_INST */
	"illegal slot instruction exception",	/* 0x1a0 EXPEVT_SLOT_INST */
	"--",					/* 0x1c0 (external interrupt) */
	"user break point trap",		/* 0x1e0 EXPEVT_BREAK */
};
const int exp_types = sizeof exp_type / sizeof exp_type[0];

void general_exception(struct proc *, struct trapframe *, uint32_t);
void tlb_exception(struct proc *, struct trapframe *, uint32_t);
void ast(struct proc *, struct trapframe *);
void syscall(struct proc *, struct trapframe *);

/*
 * void general_exception(struct proc *p, struct trapframe *tf):
 *	p  ... curproc when exception occured.
 *	tf ... full user context.
 *	va ... fault va for user mode EXPEVT_ADDR_ERR_{LD,ST}
 */
void
general_exception(struct proc *p, struct trapframe *tf, uint32_t va)
{
	int expevt = tf->tf_expevt;
	int tra;
	boolean_t usermode = !KERNELMODE(tf->tf_ssr);
	union sigval sv;

	uvmexp.traps++;

	if (p == NULL)
 		goto do_panic;

	if (usermode) {
		KDASSERT(p->p_md.md_regs == tf); /* check exception depth */
		expevt |= EXP_USER;
	}

	switch (expevt) {
	case EXPEVT_TRAPA:
		/* Check for ddb request */
		tra = _reg_read_4(SH_(TRA));
		if (tra == (_SH_TRA_BREAK << 2)) {
			kdb_trap(expevt, tra, tf);
			goto out;
		} else
			goto do_panic;
	case EXPEVT_TRAPA | EXP_USER:
		/* Check for debugger break */
		tra = _reg_read_4(SH_(TRA));
		switch (tra) {
		case _SH_TRA_BREAK << 2:
			tf->tf_spc -= 2; /* back to the breakpoint address */
			sv.sival_ptr = (void *)tf->tf_spc;
			trapsignal(p, SIGTRAP, expevt & ~EXP_USER, TRAP_BRKPT,
			    sv);
			goto out;
		case _SH_TRA_SYSCALL << 2:
			syscall(p, tf);
			return;
		default:
			sv.sival_ptr = (void *)tf->tf_spc;
			trapsignal(p, SIGILL, expevt & ~EXP_USER, ILL_ILLTRP,
			    sv);
			goto out;
		}
		break;

	case EXPEVT_ADDR_ERR_LD: /* FALLTHROUGH */
	case EXPEVT_ADDR_ERR_ST:
		KDASSERT(p->p_md.md_pcb->pcb_onfault != NULL);
		tf->tf_spc = (int)p->p_md.md_pcb->pcb_onfault;
		if (tf->tf_spc == 0)
			goto do_panic;
		break;

	case EXPEVT_ADDR_ERR_LD | EXP_USER: /* FALLTHROUGH */
	case EXPEVT_ADDR_ERR_ST | EXP_USER:
		sv.sival_ptr = (void *)va;
		if (((int)va) < 0)
			trapsignal(p, SIGSEGV, expevt & ~EXP_USER, SEGV_ACCERR,
			    sv);
		else
			trapsignal(p, SIGBUS, expevt & ~EXP_USER, BUS_ADRALN,
			    sv);
		goto out;

	case EXPEVT_RES_INST | EXP_USER: /* FALLTHROUGH */
	case EXPEVT_SLOT_INST | EXP_USER:
		sv.sival_ptr = (void *)tf->tf_spc;
		trapsignal(p, SIGILL, expevt & ~EXP_USER, ILL_ILLOPC, sv);
		goto out;

	case EXPEVT_BREAK | EXP_USER:
		sv.sival_ptr = (void *)tf->tf_spc;
		trapsignal(p, SIGTRAP, expevt & ~EXP_USER, TRAP_TRACE, sv);
		goto out;

	default:
		goto do_panic;
	}

	if (!usermode)
		return;
out:
	userret(p);
	return;

do_panic:
	if ((expevt >> 5) < exp_types)
		printf("fatal %s", exp_type[expevt >> 5]);
	else
		printf("EXPEVT 0x%03x", expevt);
	printf(" in %s mode\n", expevt & EXP_USER ? "user" : "kernel");
	printf(" spc %x ssr %x \n", tf->tf_spc, tf->tf_ssr);

	panic("general_exception");
	/* NOTREACHED */
}


/*
 * void tlb_exception(struct proc *p, struct trapframe *tf, uint32_t va):
 *	p  ... curproc when exception occured.
 *	tf ... full user context.
 *	va ... fault address.
 */
void
tlb_exception(struct proc *p, struct trapframe *tf, uint32_t va)
{
	struct vm_map *map;
	pmap_t pmap;
	union sigval sv;
	boolean_t usermode;
	int err, track, ftype;
	const char *panic_msg;

#define TLB_ASSERT(assert, msg)				\
		do {					\
			if (!(assert)) {		\
				panic_msg =  msg;	\
				goto tlb_panic;		\
			}				\
		} while(/*CONSTCOND*/0)


	usermode = !KERNELMODE(tf->tf_ssr);
	if (usermode) {
		KDASSERT(p->p_md.md_regs == tf);
	} else {
		KDASSERT(p == NULL ||		/* idle */
		    p == &proc0 ||		/* kthread */
		    p->p_md.md_regs != tf);	/* other */
	}

	switch (tf->tf_expevt) {
	case EXPEVT_TLB_MISS_LD:
		track = PVH_REFERENCED;
		ftype = VM_PROT_READ;
		break;
	case EXPEVT_TLB_MISS_ST:
		track = PVH_REFERENCED;
		ftype = VM_PROT_WRITE;
		break;
	case EXPEVT_TLB_MOD:
		track = PVH_REFERENCED | PVH_MODIFIED;
		ftype = VM_PROT_WRITE;
		break;
	case EXPEVT_TLB_PROT_LD:
		TLB_ASSERT((int)va > 0,
		    "kernel virtual protection fault (load)");
		if (usermode) {
			sv.sival_ptr = (void *)va;
			trapsignal(p, SIGSEGV, tf->tf_expevt, SEGV_ACCERR, sv);
			goto user_fault;
		} else {
			TLB_ASSERT(p->p_md.md_pcb->pcb_onfault != NULL,
			    "no copyin/out fault handler (load protection)");
			tf->tf_spc = (int)p->p_md.md_pcb->pcb_onfault;
		}
		return;

	case EXPEVT_TLB_PROT_ST:
		track = 0;	/* call uvm_fault first. (COW) */
		ftype = VM_PROT_WRITE;
		break;

	default:
		TLB_ASSERT(0, "impossible expevt");
	}

	/* Select address space */
	if (usermode) {
		TLB_ASSERT(p != NULL, "no curproc");
		map = &p->p_vmspace->vm_map;
		pmap = map->pmap;
	} else {
		if ((int)va < 0) {
			map = kernel_map;
			pmap = pmap_kernel();
		} else {
			TLB_ASSERT(p != NULL &&
			    p->p_md.md_pcb->pcb_onfault != NULL,
			    "invalid user-space access from kernel mode");
			if (va == 0) {
				tf->tf_spc = (int)p->p_md.md_pcb->pcb_onfault;
				return;
			}
			map = &p->p_vmspace->vm_map;
			pmap = map->pmap;
		}
	}

	/* Lookup page table. if entry found, load it. */
	if (track && __pmap_pte_load(pmap, va, track)) {
		if (usermode)
			userret(p);
		return;
	}

	/* Page not found. call fault handler */
	if (!usermode && pmap != pmap_kernel() &&
	    p->p_md.md_pcb->pcb_faultbail) {
		TLB_ASSERT(p->p_md.md_pcb->pcb_onfault != NULL,
		    "no copyin/out fault handler (interrupt context)");
		tf->tf_spc = (int)p->p_md.md_pcb->pcb_onfault;
		return;
	}

	err = uvm_fault(map, va, 0, ftype);

	/* User stack extension */
	if (map != kernel_map &&
	    (va >= (vaddr_t)p->p_vmspace->vm_maxsaddr) &&
	    (va < USRSTACK)) {
		if (err == 0) {
			struct vmspace *vm = p->p_vmspace;
			uint32_t nss;
			nss = btoc(USRSTACK - va);
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (err == EACCES) {
			err = EFAULT;
		}
	}

	/* Page in. load PTE to TLB. */
	if (err == 0) {
		boolean_t loaded = __pmap_pte_load(pmap, va, track);
		TLB_ASSERT(loaded, "page table entry not found");
		if (usermode)
			userret(p);
		return;
	}

	/* Page not found. */
	if (usermode) {
		sv.sival_ptr = (void *)va;
		if (err == ENOMEM) {
			printf("UVM: pid %d (%s), uid %d killed: out of swap\n",
			    p->p_pid, p->p_comm,
			    p->p_cred && p->p_ucred ?
				(int)p->p_ucred->cr_uid : -1);
			trapsignal(p, SIGKILL, tf->tf_expevt, SEGV_MAPERR, sv);
		} else
			trapsignal(p, SIGSEGV, tf->tf_expevt, SEGV_MAPERR, sv);
		goto user_fault;
	} else {
		TLB_ASSERT(p->p_md.md_pcb->pcb_onfault,
		    "no copyin/out fault handler (page not found)");
		tf->tf_spc = (int)p->p_md.md_pcb->pcb_onfault;
	}
	return;

user_fault:
	userret(p);
	ast(p, tf);
	return;

tlb_panic:
	panic("tlb_exception: %s\n"
	      "expevt=%x va=%08x ssr=%08x spc=%08x proc=%p onfault=%p",
	      panic_msg, tf->tf_expevt, va, tf->tf_ssr, tf->tf_spc,
	      p, p ? p->p_md.md_pcb->pcb_onfault : NULL);
#undef	TLB_ASSERT
}


/*
 * void ast(struct proc *p, struct trapframe *tf):
 *	p  ... curproc when exception occured.
 *	tf ... full user context.
 *	This is called upon exception return. if return from kernel to user,
 *	handle asynchronous software traps and context switch if needed.
 */
void
ast(struct proc *p, struct trapframe *tf)
{
	if (KERNELMODE(tf->tf_ssr))
		return;
	KDASSERT(p != NULL);
	KDASSERT(p->p_md.md_regs == tf);

	while (p->p_md.md_astpending) {
		uvmexp.softs++;
		p->p_md.md_astpending = 0;

		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}

		if (want_resched) {
			/* We are being preempted. */
			preempt(NULL);
		}

		userret(p);
	}
}

void
syscall(struct proc *p, struct trapframe *tf)
{
	caddr_t params;
	const struct sysent *callp;
	int error, oerror, opc, nsys;
	size_t argsize;
	register_t code, args[8], rval[2], ocode;

	uvmexp.syscalls++;

	opc = tf->tf_spc;
	ocode = code = tf->tf_r0;

	nsys = p->p_emul->e_nsysent;
	callp = p->p_emul->e_sysent;

	params = (caddr_t)tf->tf_r15;

	switch (code) {
	case SYS_syscall:
		/*
		 * Code is first argument, followed by actual args.
		 */
	        code = tf->tf_r4;
		break;
	case SYS___syscall:
		/*
		 * Like syscall, but code is a quad, so as to maintain
		 * quad alignment for the rest of the arguments.
		 */
		if (callp != sysent)
			break;
#if _BYTE_ORDER == BIG_ENDIAN
		code = tf->tf_r5;
#else
		code = tf->tf_r4;
#endif
		break;
	default:
		break;
	}
	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;		/* illegal */
	else
		callp += code;
	argsize = callp->sy_argsize;
#ifdef DIAGNOSTIC
	if (argsize > sizeof args) {
		callp += p->p_emul->e_nosys - code;
		goto bad;
	}
#endif

	switch (ocode) {
	case SYS_syscall:
		if (argsize) {
			args[0] = tf->tf_r5;
			args[1] = tf->tf_r6;
			args[2] = tf->tf_r7;
			if (argsize > 3 * sizeof(int)) {
				argsize -= 3 * sizeof(int);
				error = copyin(params, (caddr_t)&args[3],
					       argsize);
			} else
				error = 0;
		} else
			error = 0;
		break;
	case SYS___syscall:
		if (argsize) {
			args[0] = tf->tf_r6;
			args[1] = tf->tf_r7;
			if (argsize > 2 * sizeof(int)) {
				argsize -= 2 * sizeof(int);
				error = copyin(params, (caddr_t)&args[2],
					       argsize);
			} else
				error = 0;
		} else
			error = 0;
		break;
	default:
		if (argsize) {
			args[0] = tf->tf_r4;
			args[1] = tf->tf_r5;
			args[2] = tf->tf_r6;
			args[3] = tf->tf_r7;
			if (argsize > 4 * sizeof(int)) {
				argsize -= 4 * sizeof(int);
				error = copyin(params, (caddr_t)&args[4],
					       argsize);
			} else
				error = 0;
		} else
			error = 0;
		break;
	}

	if (error)
		goto bad;

#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p, code, callp->sy_argsize, args);
#endif

	rval[0] = 0;
	rval[1] = tf->tf_r1;
#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE))
		error = systrace_redirect(code, p, args, rval);
	else
#endif
		error = (*callp->sy_call)(p, args, rval);

	switch (oerror = error) {
	case 0:
		tf->tf_r0 = rval[0];
		tf->tf_r1 = rval[1];
		tf->tf_ssr |= PSL_TBIT;	/* T bit */
		break;
	case ERESTART:
		/* 2 = TRAPA instruction size */
		tf->tf_spc = opc - 2;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		tf->tf_r0 = error;
		tf->tf_ssr &= ~PSL_TBIT;	/* T bit */
		break;
	}

#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, oerror, rval);
#endif
	userret(p);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, code, oerror, rval[0]);
#endif
}

/*
 * void child_return(void *arg):
 *
 *	uvm_fork sets this routine to proc_trampoline's service function.
 *	when returning from here, jump to userland.
 */
void
child_return(void *arg)
{
	struct proc *p = arg;
	struct trapframe *tf = p->p_md.md_regs;

	tf->tf_r0 = 0;
	tf->tf_ssr |= PSL_TBIT; /* This indicates no error. */

	userret(p);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p,
		    (p->p_flag & P_PPWAIT) ? SYS_vfork : SYS_fork, 0, 0);
#endif
}

