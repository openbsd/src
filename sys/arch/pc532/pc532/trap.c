/*	$NetBSD: trap.c,v 1.13 1995/06/09 06:00:10 phil Exp $	*/

/*-
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
 * 532 Trap and System call handling
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/acct.h>
#include <sys/kernel.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <sys/syscall.h>

#include <vm/vm_param.h>
#include <vm/pmap.h>
#include <vm/vm_map.h>

#include <machine/cpu.h>
#include <machine/trap.h>
#include <machine/psl.h>



unsigned rcr2();
extern short cpl;

/*
 * trap(frame):
 *	Exception, fault, and trap interface to BSD kernel. This
 * common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed. Note that the
 * effect is as if the arguments were passed call by reference.
 */
/*ARGSUSED*/
trap(frame)
	struct trapframe frame;
{
	register int i;
	register struct proc *p = curproc;
	struct timeval sticks;
	int ucode, type, tear, msr;

	cnt.v_trap++;

	type = frame.tf_trapno;
	tear = frame.tf_tear;
	msr  = frame.tf_msr;

	if (curpcb->pcb_onfault && frame.tf_trapno != T_ABT) {
copyfault:
		frame.tf_pc = (int)curpcb->pcb_onfault;
		return;
	}

#ifdef DDB
	if (curpcb && curpcb->pcb_onfault) {
		if (frame.tf_trapno == T_BPTFLT
		    || frame.tf_trapno == T_TRCTRAP)
			if (kdb_trap (type, 0, &frame))
				return;
	}
#endif
	
	if (curpcb == 0 || curproc == 0) goto we_re_toast;

	if ((frame.tf_psr & PSL_USER) == PSL_USER) {
		type |= T_USER;
#ifdef notdef
		sticks = p->p_stime;
#endif
		p->p_md.md_regs = (int *)&(frame.tf_reg);
	}

	ucode = 0;
	
	switch (type) {

	default:
	we_re_toast:
#ifdef KDB
		if (kdb_trap(&psl))
			return;
#endif
#ifdef DDB
		if (kdb_trap (type, 0, &frame))
			return;
#endif

		printf("bad trap: type=%d, pc=0x%x, tear=0x%x, msr=0x%x\n",
			type, frame.tf_pc, frame.tf_tear, frame.tf_msr);
		panic("trap");
		/*NOTREACHED*/

	case T_ABT:	/* System level pagefault! */
		if (((msr & MSR_STT) == STT_SEQ_INS)
			|| ((msr & MSR_STT) == STT_NSQ_INS))
		  {
		    printf ("System pagefault: pc=0x%x, tear=0x%x, msr=0x%x\n",
			frame.tf_pc, frame.tf_tear, frame.tf_msr);
		    goto we_re_toast;
		  }

		/* fall into */
	case T_ABT | T_USER: 	/* User level pagefault! */
/*		if (type == (T_ABT | T_USER))
		  printf ("pagefault: pc=0x%x, tear=0x%x, msr=0x%x\n",
			frame.tf_pc, frame.tf_tear, frame.tf_msr); */
	    {
		register vm_offset_t va;
		register struct vmspace *vm = p->p_vmspace;
		register vm_map_t map;
		int rv;
		vm_prot_t ftype;
		extern vm_map_t kernel_map;
		unsigned nss,v;

		va = trunc_page((vm_offset_t)tear);
		/*
		 * Avoid even looking at pde_v(va) for high va's.   va's
		 * above VM_MAX_KERNEL_ADDRESS don't correspond to normal
		 * PDE's (half of them correspond to APDEpde and half to
		 * an unmapped kernel PDE).  va's betweeen 0xFEC00000 and
		 * VM_MAX_KERNEL_ADDRESS correspond to unmapped kernel PDE's
		 * (XXX - why are only 3 initialized when 6 are required to
		 * reach VM_MAX_KERNEL_ADDRESS?).  Faulting in an unmapped
		 * kernel page table would give inconsistent PTD's.
		 *
		 * XXX - faulting in unmapped page tables wastes a page if
		 * va turns out to be invalid.
		 *
		 * XXX - should "kernel address space" cover the kernel page
		 * tables?  Might have same problem with PDEpde as with
		 * APDEpde (or there may be no problem with APDEpde).
		 */
		if (va > 0xFEBFF000) {
			v = KERN_FAILURE;	/* becomes SIGSEGV */
			goto nogo;
		}
		/*
		 * It is only a kernel address space fault iff:
		 * 	1. (type & T_USER) == 0  and
		 * 	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor space fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if (type == T_ABT && va >= KERNBASE)
			map = kernel_map;
		else
			map = &vm->vm_map;
		if ((msr & MSR_DDT) == DDT_WRITE 
		     || (msr & MSR_STT) == STT_RMW)
			ftype = VM_PROT_READ | VM_PROT_WRITE;
		else
			ftype = VM_PROT_READ;

#ifdef DEBUG
		if (map == kernel_map && va == 0) {
			printf("trap: bad kernel access at %x\n", va);
			goto we_re_toast;
		}
#endif

		nss = 0;
		if ((caddr_t)va >= vm->vm_maxsaddr
		     && (caddr_t)va < (caddr_t)VM_MAXUSER_ADDRESS
		     && map != kernel_map) {
			nss = clrnd(btoc((unsigned)vm->vm_maxsaddr
				+ MAXSSIZ - (unsigned)va));
			if (nss > btoc(p->p_rlimit[RLIMIT_STACK].rlim_cur)) {
/*pg("trap rlimit %d, maxsaddr %x va %x ", nss, vm->vm_maxsaddr, va);*/
				rv = KERN_FAILURE;
				goto nogo;
			}
		}

		/* check if page table is mapped, if not, fault it first */
#define pde_v(v) (PTD[((v)>>PD_SHIFT)&1023].pd_v)
		if (!pde_v(va)) {
			v = trunc_page(vtopte(va));
			rv = vm_fault(map, v, ftype, FALSE);
			if (rv != KERN_SUCCESS) goto nogo;
			/* check if page table fault, increment wiring */
			vm_map_pageable(map, v, round_page(v+1), FALSE);
		} else v=0;
		rv = vm_fault(map, va, ftype, FALSE);

		if (rv == KERN_SUCCESS) {
			/*
			 * XXX: continuation of rude stack hack
			 */
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
			va = trunc_page(vtopte(va));
			/* for page table, increment wiring
			   as long as not a page table fault as well */
			if (!v && map != kernel_map)
			  vm_map_pageable(map, va, round_page(va+1), FALSE);
			if (type == T_ABT)
				return;
			goto out;
		}
nogo:
		if (type == T_ABT) {
			if (curpcb->pcb_onfault)
				goto copyfault;
			printf("vm_fault(0x%x, 0x%x, 0x%x, 0) -> 0x%x\n",
			       map, va, ftype, rv);
			printf("  type 0x%x, tear 0x%x msr 0x%x\n",
			       type, tear, msr);
			goto we_re_toast;
		}
		i = SIGSEGV;
		break;
	    }

	case T_UND | T_USER: 	/* undefined instruction */
	case T_ILL | T_USER: 	/* Illegal instruction! */
		ucode = type &~ T_USER;
		i = SIGILL;
		break;

	case T_NVI | T_USER: 	/* Non-vectored interrupt */
	case T_NMI | T_USER: 	/* non-maskable interrupt */
	case T_FLG | T_USER: 	/* flag instruction */
		goto we_re_toast;

	case T_NBE | T_USER: 	/* non-restartable bus error */
		ucode = type &~ T_USER;
		i = SIGBUS;
		break;

	case T_RBE | T_USER: 	/* restartable bus error */
		return;

	case T_SLAVE | T_USER: 	/* coprocessor trap */
		ucode = type &~ T_USER;
/*		ucode = FPE_INTDIV_TRAP; */
		i = SIGFPE;
		break;

	case T_DVZ | T_USER: 	/* divide by zero */
		ucode = type &~ T_USER;
/*		ucode = FPE_INTDIV_TRAP; */
		i = SIGFPE;
		break;

	case T_OVF | T_USER: 	/* integer overflow trap */
		ucode = type &~ T_USER;
/* 		ucode = FPE_INTOVF_TRAP; */
		i = SIGFPE;
		break;


	case T_TRC | T_USER: 	/* trace trap */
	case T_BPT | T_USER: 	/* breakpoint instruction */
	case T_DBG | T_USER: 	/* debug trap */
		frame.tf_psr &= ~PSL_P;
		i = SIGTRAP;
		break;

	case T_INTERRUPT | T_USER: 	/* Allow Process Switch */
/*		if ((p->p_flag & SOWEUPC) && p->p_stats->p_prof.pr_scale) {
			addupc(frame.tf_eip, &p->p_stats->p_prof, 1);
			p->p_flag &= ~SOWEUPC;
		} */
		goto out;

	}	/* End of switch */

	trapsignal(p, i, ucode);
	if ((type & T_USER) == 0)
		return;
out:
	while (i = CURSIG(p))
		postsig(i);
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrunqueue ourselves but
		 * before we switch()'ed, we might not be on the queue
		 * indicated by our priority.
		 */
		(void) splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		(void) splnone();
		while (i = CURSIG(p))
			postsig(i);
	}
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;

#ifdef YO_WHAT
		struct timeval *tv = &p->p_stime; 

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
#ifdef PROFTIMER
			extern int profscale;
			addupc(frame.tf_eip, &p->p_stats->p_prof,
			    ticks * profscale);
#else
/*			addupc(frame.tf_pc, &p->p_stats->p_prof, ticks); */
#endif
		}
#endif
	}
	curpriority = p->p_priority;
}


/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 * Like trap(), argument is call by reference.
 */
/*ARGSUSED*/
syscall(frame)
	volatile struct syscframe frame;
{

	register caddr_t params;
	register int i;
	register struct sysent *callp;
	register struct proc *p;
	struct timeval sticks;
	int error, opc, nsys;
	int args[8], rval[2];
	int code;

	cnt.v_syscall++;

	/* is this a user? */
	if ((frame.sf_psr & PSL_USER) != PSL_USER)
		panic("syscall - process not in user mode.");

	p = curproc;
#ifdef notdef
	sticks = p->p_stime;
#endif
	code = frame.sf_reg[REG_R0];
	p->p_md.md_regs = (int *) & (frame.sf_reg);
	params = (caddr_t)frame.sf_usp + sizeof (int) ;

	callp = p->p_emul->e_sysent;
	nsys = p->p_emul->e_nsysent;

	/* Set new return address and save old one. */
	opc = frame.sf_pc++;

	switch (code) {
	case SYS_syscall:
		code = fuword(params);
		params += sizeof(int);
		break;
	
	case SYS___syscall:
		code = fuword(params + _QUAD_LOWWORD * sizeof(int));
		params += sizeof(quad_t);
		break;

	default:
		/* do nothing by default */
		break;
	}

	/* Guard against bad sys call numbers! */
        if (code < 0 || code >= nsys)
                callp += p->p_emul->e_nosys;	/* indir (illegal) */
        else
                callp += code;

	if ((i = callp->sy_argsize) &&
	    (error = copyin(params, (caddr_t)args, (u_int)i))) {
		frame.sf_reg[REG_R0] = error;
		frame.sf_psr |= PSL_C;	
#ifdef SYSCALL_DEBUG
		scdebug_call(p, code, callp->sy_narg, i, args);
#endif
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, code, i, &args);
#endif
		goto done;
	}
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, callp->sy_narg, i, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, i, &args);
#endif
	rval[0] = 0;
	rval[1] = 0;
	error = (*callp->sy_call)(p, args, rval);
	if (error == ERESTART)
		frame.sf_pc = opc;
	else if (error != EJUSTRETURN) {
		if (error) {
			frame.sf_reg[REG_R0] = error;
			frame.sf_psr |= PSL_C;
		} else {
			frame.sf_reg[REG_R0] = rval[0];
			frame.sf_reg[REG_R1] = rval[1];
			frame.sf_psr &= ~PSL_C;
		}
	}
	/* else if (error == EJUSTRETURN) */
		/* nothing to do */
done:
	/*
	 * Reinitialize proc pointer `p' as it may be different
	 * if this is a child returning from fork syscall.
	 */
	p = curproc;
	while (i = CURSIG(p))
		postsig(i);
	p->p_priority = p->p_usrpri;
	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we setrunqeue ourselves but before
		 * we switch()'ed, we might not be on the queue indicated by
		 * our priority.
		 */
		(void) splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		(void) splnone();
		while (i = CURSIG(p))
			postsig(i);
	}
	if (p->p_stats->p_prof.pr_scale) {
		int ticks;
#ifdef YO_WHAT
		struct timeval *tv = &p->p_stime;

		ticks = ((tv->tv_sec - syst.tv_sec) * 1000 +
			(tv->tv_usec - syst.tv_usec) / 1000) / (tick / 1000);
		if (ticks) {
#ifdef PROFTIMER
			extern int profscale;
			addupc(frame.sf_pc, &p->p_stats->p_prof,
			    ticks * profscale);
#else
/*			addupc(frame.sf_pc, &p->p_stats->p_prof, ticks); */
#endif
		}
#endif
	}
	curpriority = p->p_priority;
#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval[0]);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif

}

/* For the child, do the stuff after mi_swtch() in syscall so
   low_level_fork does not have to rethread the kernel stack. */
void
ll_fork_sig()
{
	register struct proc *p = curproc;
	int i;

	(void) splnone();
	while (i = CURSIG(p))
		postsig(i);
}


/* #define dbg_user */
/* Other stuff.... */
int
check_user_write ( u_long addr, u_long size)
{
  int rv;
  vm_offset_t va;

#ifdef dbg_user
printf ("ck_ur_wr: addr=0x%x, size=0x%x", addr, size);
#endif
  /* check for all possible places! */
  va = trunc_page((vm_offset_t) addr);
  if (va > VM_MAXUSER_ADDRESS) return (1);

  while ((u_long)va < (addr + size)) {
    /* check for copy on write access. */
#ifdef dbg_user
printf (" (0x%x:%d)", va, vtopte(va)->pg_prot);
#endif
    if (!(vtopte(va)->pg_v) || vtopte(va)->pg_prot != 3 ) {
#ifdef dbg_user
printf (" fault");
#endif
	rv = vm_fault(&curproc->p_vmspace->vm_map, va,
		VM_PROT_READ | VM_PROT_WRITE, FALSE);
	if (rv != KERN_SUCCESS)
#ifdef dbg_user
{ printf (" bad\n");
#endif
		return(1);
#ifdef dbg_user
}
#endif
    }
    va += NBPG;
  }
#ifdef dbg_user
printf ("\n");
#endif

  return (0);
}
