/*	$NetBSD: trap.c,v 1.91 1995/12/09 05:00:27 mycroft Exp $	*/

#undef DEBUG
#define DEBUG
/*-
 * Copyright (c) 1995 Charles M. Hannum.  All rights reserved.
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
 * 386 Trap and System call handling
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#include <sys/signal.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <sys/syscall.h>

#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/psl.h>
#include <machine/reg.h>
#include <machine/trap.h>

#ifdef COMPAT_IBCS2
#include <compat/ibcs2/ibcs2_errno.h>
#include <compat/ibcs2/ibcs2_exec.h>
extern struct emul emul_ibcs2;
#endif
#ifdef COMPAT_LINUX
#include <sys/exec.h>
#include <compat/linux/linux_syscall.h>
extern struct emul emul_linux_aout, emul_linux_elf;
#endif
#ifdef COMPAT_FREEBSD
extern struct emul emul_freebsd;
#endif

#include "npx.h"

/*
 * Define the code needed before returning to user mode, for
 * trap and syscall.
 */
static inline void
userret(p, pc, oticks)
	register struct proc *p;
	int pc;
	u_quad_t oticks;
{
	int sig, s;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, a clock interrupt could
		 * change our priority without changing run queues
		 * (the running process is not kept on a run queue).
		 * If this happened after we setrunqueue ourselves but
		 * before we switch()'ed, we might not be on the queue
		 * indicated by our priority.
		 */
		s = splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) { 
		extern int psratio;

		addupc_task(p, pc, (int)(p->p_sticks - oticks) * psratio);
	}                   

	curpriority = p->p_priority;
}

char	*trap_type[] = {
	"privileged instruction fault",		/*  0 T_PRIVINFLT */
	"breakpoint trap",			/*  1 T_BPTFLT */
	"arithmetic trap",			/*  2 T_ARITHTRAP */
	"asynchronous system trap",		/*  3 T_ASTFLT */
	"protection fault",			/*  4 T_PROTFLT */
	"trace trap",				/*  5 T_TRCTRAP */
	"page fault",				/*  6 T_PAGEFLT */
	"alignment fault",			/*  7 T_ALIGNFLT */
	"integer divide fault",			/*  8 T_DIVIDE */
	"non-maskable interrupt",		/*  9 T_NMI */
	"overflow trap",			/* 10 T_OFLOW */
	"bounds check fault",			/* 11 T_BOUND */
	"FPU not available fault",		/* 12 T_DNA */
	"double fault",				/* 13 T_DOUBLEFLT */
	"FPU operand fetch fault",		/* 14 T_FPOPFLT */
	"invalid TSS fault",			/* 15 T_TSSFLT */
	"segment not present fault",		/* 16 T_SEGNPFLT */
	"stack fault",				/* 17 T_STKFLT */
	"reserved trap",			/* 18 T_RESERVED */
};
int	trap_types = sizeof trap_type / sizeof trap_type[0];

#ifdef DEBUG
int	trapdebug = 0;
#endif

/*
 * trap(frame):
 *	Exception, fault, and trap interface to BSD kernel. This
 * common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed. Note that the
 * effect is as if the arguments were passed call by reference.
 */
/*ARGSUSED*/
void
trap(frame)
	struct trapframe frame;
{
	register struct proc *p = curproc;
	int type = frame.tf_trapno;
	u_quad_t sticks;
	struct pcb *pcb;
	extern char fusubail[],
		    resume_iret[], resume_pop_ds[], resume_pop_es[];
	struct trapframe *vframe;
	int resume;

	cnt.v_trap++;

#ifdef DEBUG
	if (trapdebug) {
		printf("trap %d code %x eip %x cs %x eflags %x cr2 %x cpl %x\n",
		    frame.tf_trapno, frame.tf_err, frame.tf_eip, frame.tf_cs,
		    frame.tf_eflags, rcr2(), cpl);
		printf("curproc %x\n", curproc);
	}
#endif

	if (!KERNELMODE(frame.tf_cs, frame.tf_eflags)) {
		type |= T_USER;
		sticks = p->p_sticks;
		p->p_md.md_regs = &frame;
	}

	switch (type) {

	default:
	we_re_toast:
#ifdef DDB
		if (kdb_trap(type, 0, &frame))
			return;
#endif
		if (frame.tf_trapno < trap_types)
			printf("fatal %s", trap_type[frame.tf_trapno]);
		else
			printf("unknown trap %d", frame.tf_trapno);
		printf(" in %s mode\n", (type & T_USER) ? "user" : "supervisor");
		printf("trap type %d code %x eip %x cs %x eflags %x cr2 %x cpl %x\n",
		    type, frame.tf_err, frame.tf_eip, frame.tf_cs, frame.tf_eflags, rcr2(), cpl);

		panic("trap");
		/*NOTREACHED*/

	case T_PROTFLT:
	case T_SEGNPFLT:
	case T_ALIGNFLT:
		/* Check for copyin/copyout fault. */
		pcb = &p->p_addr->u_pcb;
		if (pcb->pcb_onfault != 0) {
		copyfault:
			frame.tf_eip = (int)pcb->pcb_onfault;
			return;
		}

		/*
		 * Check for failure during return to user mode.
		 *
		 * We do this by looking at the instruction we faulted on.  The
		 * specific instructions we recognize only happen when
		 * returning from a trap, syscall, or interrupt.
		 *
		 * XXX
		 * The heuristic used here will currently fail for the case of
		 * one of the 2 pop instructions faulting when returning from a
		 * a fast interrupt.  This should not be possible.  It can be
		 * fixed by rearranging the trap frame so that the stack format
		 * at this point is the same as on exit from a `slow'
		 * interrupt.
		 */
		switch (*(u_char *)frame.tf_eip) {
		case 0xcf:	/* iret */
			vframe = (void *)((int)&frame.tf_esp - 44);
			resume = (int)resume_iret;
			break;
		case 0x1f:	/* popl %ds */
			vframe = (void *)((int)&frame.tf_esp - 4);
			resume = (int)resume_pop_ds;
			break;
		case 0x07:	/* popl %es */
			vframe = (void *)((int)&frame.tf_esp - 0);
			resume = (int)resume_pop_es;
			break;
		default:
			goto we_re_toast;
		}
		if (KERNELMODE(vframe->tf_cs, vframe->tf_eflags))
			goto we_re_toast;

		frame.tf_eip = resume;
		return;

	case T_SEGNPFLT|T_USER:
	case T_STKFLT|T_USER:
	case T_PROTFLT|T_USER:		/* protection fault */
	case T_ALIGNFLT|T_USER:
		trapsignal(p, SIGBUS, type &~ T_USER);
		goto out;

	case T_PRIVINFLT|T_USER:	/* privileged instruction fault */
	case T_FPOPFLT|T_USER:		/* coprocessor operand fault */
		trapsignal(p, SIGILL, type &~ T_USER);
		goto out;

	case T_ASTFLT|T_USER:		/* Allow process switch */
		cnt.v_soft++;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		goto out;

	case T_DNA|T_USER: {
#ifdef MATH_EMULATE
		int rv;
		if ((rv = math_emulate(&frame)) == 0) {
			if (frame.tf_eflags & PSL_T)
				goto trace;
			return;
		}
		trapsignal(p, rv, type &~ T_USER);
		goto out;
#else
		printf("pid %d killed due to lack of floating point\n",
		    p->p_pid);
		trapsignal(p, SIGKILL, type &~ T_USER);
		goto out;
#endif
	}

	case T_BOUND|T_USER:
	case T_OFLOW|T_USER:
	case T_DIVIDE|T_USER:
		trapsignal(p, SIGFPE, type &~ T_USER);
		goto out;

	case T_ARITHTRAP|T_USER:
		trapsignal(p, SIGFPE, frame.tf_err);
		goto out;

	case T_PAGEFLT:			/* allow page faults in kernel mode */
		if (p == 0)
			goto we_re_toast;
		pcb = &p->p_addr->u_pcb;
		/*
		 * fusubail is used by [fs]uswintr() to prevent page faulting
		 * from inside the profiling interrupt.
		 */
		if (pcb->pcb_onfault == fusubail)
			goto copyfault;
#if 0
		/* XXX - check only applies to 386's and 486's with WP off */
		if (frame.tf_err & PGEX_P)
			goto we_re_toast;
#endif
		/* FALLTHROUGH */

	case T_PAGEFLT|T_USER: {	/* page fault */
		register vm_offset_t va;
		register struct vmspace *vm = p->p_vmspace;
		register vm_map_t map;
		int rv;
		vm_prot_t ftype;
		extern vm_map_t kernel_map;
		unsigned nss, v;

		va = trunc_page((vm_offset_t)rcr2());
		/*
		 * It is only a kernel address space fault iff:
		 *	1. (type & T_USER) == 0  and
		 *	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor space fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if (type == T_PAGEFLT && va >= KERNBASE)
			map = kernel_map;
		else
			map = &vm->vm_map;
		if (frame.tf_err & PGEX_W)
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;

#ifdef DIAGNOSTIC
		if (map == kernel_map && va == 0) {
			printf("trap: bad kernel access at %x\n", va);
			goto we_re_toast;
		}
#endif

		nss = 0;
		if ((caddr_t)va >= vm->vm_maxsaddr
		    && (caddr_t)va < (caddr_t)VM_MAXUSER_ADDRESS
		    && map != kernel_map) {
			nss = clrnd(btoc(USRSTACK-(unsigned)va));
			if (nss > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur)) {
				rv = KERN_FAILURE;
				goto nogo;
			}
		}

		/* check if page table is mapped, if not, fault it first */
		if ((PTD[pdei(va)] & PG_V) == 0) {
			v = trunc_page(vtopte(va));
			rv = vm_fault(map, v, ftype, FALSE);
			if (rv != KERN_SUCCESS)
				goto nogo;
			/* check if page table fault, increment wiring */
			vm_map_pageable(map, v, round_page(v+1), FALSE);
		} else
			v = 0;

		rv = vm_fault(map, va, ftype, FALSE);
		if (rv == KERN_SUCCESS) {
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
			va = trunc_page(vtopte(va));
			/* for page table, increment wiring as long as
			   not a page table fault as well */
			if (!v && map != kernel_map)
				vm_map_pageable(map, va, round_page(va+1),
				    FALSE);
			if (type == T_PAGEFLT)
				return;
			goto out;
		}

	nogo:
		if (type == T_PAGEFLT) {
			if (pcb->pcb_onfault != 0)
				goto copyfault;
			printf("vm_fault(%x, %x, %x, 0) -> %x\n",
			    map, va, ftype, rv);
			goto we_re_toast;
		}
		trapsignal(p, (rv == KERN_PROTECTION_FAILURE)
		    ? SIGBUS : SIGSEGV, T_PAGEFLT);
		break;
	}

#ifndef DDB
	/* XXX need to deal with this when DDB is present, too */
	case T_TRCTRAP:	/* kernel trace trap; someone single stepping lcall's */
			/* syscall has to turn off the trace bit itself */
		return;
#endif

	case T_BPTFLT|T_USER:		/* bpt instruction fault */
	case T_TRCTRAP|T_USER:		/* trace trap */
	trace:
		frame.tf_eflags &= ~PSL_T;
		trapsignal(p, SIGTRAP, type &~ T_USER);
		break;

#include "isa.h"
#if	NISA > 0
	case T_NMI:
	case T_NMI|T_USER:
#ifdef DDB
		/* NMI can be hooked up to a pushbutton for debugging */
		printf ("NMI ... going to debugger\n");
		if (kdb_trap (type, 0, &frame))
			return;
#endif
		/* machine/parity/power fail/"kitchen sink" faults */
		if (isa_nmi() == 0)
			return;
		else
			goto we_re_toast;
#endif
	}

	if ((type & T_USER) == 0)
		return;
out:
	userret(p, frame.tf_eip, sticks);
}

/*
 * Compensate for 386 brain damage (missing URKR)
 */
int
trapwrite(addr)
	unsigned addr;
{
	vm_offset_t va;
	unsigned nss;
	struct proc *p;
	struct vmspace *vm;

	va = trunc_page((vm_offset_t)addr);
	if (va >= VM_MAXUSER_ADDRESS)
		return 1;

	nss = 0;
	p = curproc;
	vm = p->p_vmspace;
	if ((caddr_t)va >= vm->vm_maxsaddr) {
		nss = clrnd(btoc(USRSTACK-(unsigned)va));
		if (nss > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur))
			return 1;
	}

	if (vm_fault(&vm->vm_map, va, VM_PROT_READ | VM_PROT_WRITE, FALSE)
	    != KERN_SUCCESS)
		return 1;

	if (nss > vm->vm_ssize)
		vm->vm_ssize = nss;

	return 0;
}

/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
/*ARGSUSED*/
void
syscall(frame)
	struct trapframe frame;
{
	register caddr_t params;
	register struct sysent *callp;
	register struct proc *p;
	int error, opc, nsys;
	size_t argsize;
	register_t code, args[8], rval[2];
	u_quad_t sticks;

	cnt.v_syscall++;
	if (!USERMODE(frame.tf_cs, frame.tf_eflags))
		panic("syscall");
	p = curproc;
	sticks = p->p_sticks;
	p->p_md.md_regs = &frame;
	opc = frame.tf_eip;
	code = frame.tf_eax;

	nsys = p->p_emul->e_nsysent;
	callp = p->p_emul->e_sysent;

#ifdef COMPAT_IBCS2
	if (p->p_emul == &emul_ibcs2)
		if (IBCS2_HIGH_SYSCALL(code))
			code = IBCS2_CVT_HIGH_SYSCALL(code);
#endif
	params = (caddr_t)frame.tf_esp + sizeof(int);

	switch (code) {
	case SYS_syscall:
#ifdef COMPAT_LINUX
		/* Linux has a special system setup call as number 0 */
		if (p->p_emul == &emul_linux_aout ||
		    p->p_emul == &emul_linux_elf)
			break;
#endif
		/*
		 * Code is first argument, followed by actual args.
		 */
		code = fuword(params);
		params += sizeof(int);
		break;
	case SYS___syscall:
		/*
		 * Like syscall, but code is a quad, so as to maintain
		 * quad alignment for the rest of the arguments.
		 */
#ifdef COMPAT_FREEBSD
		/* FreeBSD has a same function in SYS___syscall */
		if (callp != sysent && p->p_emul != &emul_freebsd)
#else
		if (callp != sysent)
#endif
			break;
		code = fuword(params + _QUAD_LOWWORD * sizeof(int));
		params += sizeof(quad_t);
		break;
	default:
		break;
	}
	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;		/* illegal */
	else
		callp += code;
	argsize = callp->sy_argsize;
#ifdef COMPAT_LINUX
	/* XXX extra if() for every emul type.. */
	if (p->p_emul == &emul_linux_aout || p->p_emul == &emul_linux_elf) {
		/*
		 * Linux passes the args in ebx, ecx, edx, esi, edi, in
		 * increasing order.
		 */
		switch (argsize) {
		case 20:
			args[4] = frame.tf_edi;
		case 16:
			args[3] = frame.tf_esi;
		case 12:
			args[2] = frame.tf_edx;
		case 8:
			args[1] = frame.tf_ecx;
		case 4:
			args[0] = frame.tf_ebx;
		case 0:
			break;
		default:
			panic("linux syscall with weird argument size %d",
			    argsize);
			break;
		}
		error = 0;
	}
	else
#endif
	if (argsize)
		error = copyin(params, (caddr_t)args, argsize);
	else
		error = 0;
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, argsize, args);
#endif
	if (error)
		goto bad;
	rval[0] = 0;
	rval[1] = frame.tf_edx;
	error = (*callp->sy_call)(p, args, rval);
	switch (error) {
	case 0:
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		frame.tf_eax = rval[0];
		frame.tf_edx = rval[1];
		frame.tf_eflags &= ~PSL_C;	/* carry bit */
		break;
	case ERESTART:
		/*
		 * The offset to adjust the PC by depends on whether we entered
		 * the kernel through the trap or call gate.  We pushed the
		 * size of the instruction into tf_err on entry.
		 */
		frame.tf_eip = opc - frame.tf_err;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
	bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		frame.tf_eax = error;
		frame.tf_eflags |= PSL_C;	/* carry bit */
		break;
	}

#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
	userret(p, frame.tf_eip, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

void
child_return(p, frame)
	struct proc *p;
	struct trapframe frame;
{

	frame.tf_eax = 0;
	frame.tf_eflags &= ~PSL_C;

	userret(p, frame.tf_eip, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
}
