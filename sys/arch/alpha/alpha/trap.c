/*	$NetBSD: trap.c,v 1.5 1995/11/23 02:34:37 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995 Carnegie-Mellon University.
 * All rights reserved.
 *
 * Author: Chris G. Demetriou
 * 
 * Permission to use, copy, modify and distribute this software and
 * its documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 * 
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS" 
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND 
 * FOR ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 * 
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syscall.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>
#include <machine/reg.h>
#include <machine/trap.h>

#ifdef COMPAT_OSF1
#include <compat/osf1/osf1_syscall.h>
#endif

struct proc *fpcurproc;		/* current user of the FPU */

/*
 * Define the code needed before returning to user mode, for
 * trap and syscall.
 */
static __inline void
userret(p, pc, oticks)
	register struct proc *p;
	u_int64_t pc;
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

/*
 * Trap is called from locore to handle most types of processor traps,
 * including events such as simulated software interrupts/AST's.
 * System calls are broken out for efficiency.
 */
/*ARGSUSED*/
trap(type, code, v, framep)
	unsigned long type;
	unsigned long code;
	register unsigned long v;
	struct trapframe *framep;
{
	extern char fswintr[];
	register struct proc *p;
	register int i;
	u_int64_t ucode;
	u_quad_t sticks;

	cnt.v_trap++;
	p = curproc;
	ucode = 0;
	if (USERMODE(framep->tf_ps)) {
		type |= T_USER;
		sticks = p->p_sticks;
		p->p_md.md_tf = framep;
	}
#ifdef DDB
	if (type == T_BPT) {
		if (kdb_trap(type, framep))
			return;
	}
#endif
	switch (type) {

	default:
dopanic:
		printf("trap type %ld, code = 0x%lx, v = 0x%lx\n", type,
		    code, v);
		printf("pc = 0x%lx\n", framep->tf_pc);
		printf("curproc = 0x%lx\n", curproc);
		if (curproc != NULL)
			printf("curproc->p_pid = 0x%d\n", curproc->p_pid);

#ifdef DDB
		if (kdb_trap(type, framep))
			return;
#endif
		regdump(framep);
		type &= ~T_USER;
#ifdef XXX
		if ((unsigned)type < trap_types)
			panic(trap_type[type]);
#endif
		panic("trap");

	case T_ASTFLT:
		/* oops.  this can't happen. */
		goto dopanic;

	case T_ASTFLT|T_USER:
		astpending = 0;
		cnt.v_soft++;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		goto out;

	case T_UNAFLT:			/* Always an error of some kind. */
		if (p == NULL || p->p_addr->u_pcb.pcb_onfault == NULL)
			goto dopanic;
		else {
			framep->tf_pc = (u_int64_t)p->p_addr->u_pcb.pcb_onfault;
			p->p_addr->u_pcb.pcb_onfault = NULL;
		}
		goto out;

	case T_UNAFLT|T_USER:		/* "Here, have a SIGBUS instead!" */
		i = SIGBUS;
		ucode = v;
		break;

	case T_ARITHFLT|T_USER:
sigfpe:		i = SIGFPE;
		ucode = v;
		break;

	case T_FPDISABLED|T_USER:
		/*
		 * on exit from the kernel, if proc == fpcurproc, FP is
		 * enabled.
		 */
		if (fpcurproc == p)
			panic("fp disabled for fpcurproc == %lx", p);

		pal_wrfen(1);
		if (fpcurproc)
			savefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
		fpcurproc = p;
		restorefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
		pal_wrfen(0);

		p->p_md.md_flags |= MDP_FPUSED;
		goto out;

	case T_GENTRAP|T_USER:
		if (framep->tf_a0 == -2)		/* weird! */
			goto sigfpe;
	case T_BPT|T_USER:
	case T_BUGCHK|T_USER:
		ucode = code;
		i = SIGTRAP;
		break;

	case T_OPDEC|T_USER:
		ucode = code;
		i = SIGILL;
		break;

	case T_INVALTRANS:
	case T_INVALTRANS|T_USER:
	case T_ACCESS:
	case T_ACCESS|T_USER:
	    {
		register vm_offset_t va;
		register struct vmspace *vm;
		register vm_map_t map;
		vm_prot_t ftype;
		int rv;
		extern int fswintrberr();
		extern vm_map_t kernel_map;

		/* if it was caused by fuswintr or suswintr, just punt. */
		if ((type & T_USER) == 0 && p != NULL &&
		    p->p_addr->u_pcb.pcb_onfault == (caddr_t)fswintrberr) {
			framep->tf_pc = (u_int64_t)p->p_addr->u_pcb.pcb_onfault;
			p->p_addr->u_pcb.pcb_onfault = NULL;
			goto out;
		}

		/*
		 * It is only a kernel address space fault iff:
		 *	1. (type & T_USER) == 0  and
		 *	2. pcb_onfault not set or
		 *	3. pcb_onfault set but kernel space data fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if ((type & T_USER) == 0 && (v >= VM_MIN_KERNEL_ADDRESS ||
		    p == NULL || p->p_addr->u_pcb.pcb_onfault == NULL))
			map = kernel_map;
		else {
			vm = p->p_vmspace;
			map = &vm->vm_map;
		}

		switch (code) {
		case -1:		/* instruction fetch fault */
		case 0:			/* load instruction */
			ftype = VM_PROT_READ;
			break;
		case 1:			/* store instruction */
			ftype = VM_PROT_WRITE;
			break;
		}

		va = trunc_page((vm_offset_t)v);
		rv = vm_fault(map, va, ftype, FALSE);
#ifdef VMFAULT_TRACE
		printf("vm_fault(0x%lx (pmap 0x%lx), 0x%lx (0x%lx), 0x%lx, %d) -> 0x%lx at pc 0x%lx\n",
		    map, map == kernel_map ? pmap_kernel() : &vm->vm_pmap,
		    va, v, ftype, FALSE, rv, framep->tf_pc);
#endif
		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if (map != kernel_map && (caddr_t)va >= vm->vm_maxsaddr) {
			if (rv == KERN_SUCCESS) {
				unsigned nss;

				nss = clrnd(btoc(USRSTACK-(unsigned)va));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}
		if (rv == KERN_SUCCESS)
			goto out;
		if (!USERMODE(framep->tf_ps)) {
			if (p != NULL &&
			    p->p_addr->u_pcb.pcb_onfault != NULL) {
				framep->tf_pc =
				    (u_int64_t)p->p_addr->u_pcb.pcb_onfault;
				p->p_addr->u_pcb.pcb_onfault = NULL;
				goto out;
			}
			goto dopanic;
		}
		ucode = v;
		i = (rv == KERN_PROTECTION_FAILURE) ? SIGBUS : SIGSEGV;
		break;
	    }

	case T_FOR:
	case T_FOR|T_USER:
	case T_FOE:
	case T_FOE|T_USER:
		pmap_emulate_reference(p, v, (type & T_USER) != 0, 0);
		goto out;

	case T_FOW:
	case T_FOW|T_USER:
		pmap_emulate_reference(p, v, (type & T_USER) != 0, 1);
		goto out;
	}

	trapsignal(p, i, ucode);
out:
	if ((type & T_USER) == 0)
		return;
	userret(p, framep->tf_pc, sticks);
}

/*
 * Process a system call.
 *
 * System calls are strange beasts.  They are passed the syscall number
 * in v0, and the arguments in the registers (as normal).  They return
 * an error flag in a3 (if a3 != 0 on return, the syscall had an error),
 * and the return value (if any) in v0.
 *
 * The assembly stub takes care of moving the call number into a register
 * we can get to, and moves all of the argument registers into their places
 * in the trap frame.  On return, it restores the callee-saved registers,
 * a3, and v0 from the frame before returning to the user process.
 */
void
syscall(code, framep)
	u_int64_t code;
	struct trapframe *framep;
{
	struct sysent *callp;
	struct proc *p;
	int error, numsys;
	u_int64_t opc;
	u_quad_t sticks;
	u_int64_t rval[2];
	u_int64_t args[10];					/* XXX */
	u_int hidden, nargs;
#ifdef COMPAT_OSF1
	extern struct emul emul_osf1;
#endif

#if notdef				/* can't happen, ever. */
	if (!USERMODE(framep->tf_ps))
		panic("syscall");
#endif
	cnt.v_syscall++;
	p = curproc;
	p->p_md.md_tf = framep;
	opc = framep->tf_pc - 4;
	sticks = p->p_sticks;

	callp = p->p_emul->e_sysent;
	numsys = p->p_emul->e_nsysent;


#ifdef COMPAT_OSF1
	if (p->p_emul == &emul_osf1) 
		switch (code) {
		case OSF1_SYS_syscall:
			/* OSF/1 syscall() */
			code = framep->tf_a0;
			hidden = 1;
			break;
		default:
			hidden = 0;
		}
	else
#endif
	switch(code) {
	case SYS_syscall:
	case SYS___syscall:
		/*
		 * syscall() and __syscall() are handled the same on
		 * the alpha, as everything is 64-bit aligned, anyway.
		 */
		code = framep->tf_a0;
		hidden = 1;
		break;
	default:
		hidden = 0;
	}

	error = 0;
	if (code < numsys)
		callp += code;
	else
		callp += p->p_emul->e_nosys;

	nargs = callp->sy_narg + hidden;
	switch (nargs) {
	default:
		if (nargs > 10)		/* XXX */
			panic("syscall: too many args (%d)", nargs);
		error = copyin((caddr_t)(framep->tf_regs[FRAME_SP]), &args[6],
		    (nargs - 6) * sizeof(u_int64_t));
	case 6:	
		args[5] = framep->tf_regs[FRAME_A5];
	case 5:	
		args[4] = framep->tf_regs[FRAME_A4];
	case 4:	
		args[3] = framep->tf_regs[FRAME_A3];
	case 3:	
		args[2] = framep->tf_a2;
	case 2:	
		args[1] = framep->tf_a1;
	case 1:	
		args[0] = framep->tf_a0;
	case 0:
		break;
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_argsize, args + hidden);
#endif
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args + hidden);
#endif
	if (error == 0) {
		rval[0] = 0;
		rval[1] = 0;
		error = (*callp->sy_call)(p, args + hidden, rval);
	}

	switch (error) {
	case 0:
		framep->tf_regs[FRAME_V0] = rval[0];
		framep->tf_regs[FRAME_A4] = rval[1];
		framep->tf_regs[FRAME_A3] = 0;
		break;
	case ERESTART:
		framep->tf_pc = opc;
		break;
	case EJUSTRETURN:
		break;
	default:
		framep->tf_regs[FRAME_V0] = error;
		framep->tf_regs[FRAME_A3] = 1;
		break;
	}

        /*
         * Reinitialize proc pointer `p' as it may be different
         * if this is a child returning from fork syscall.
         */
	p = curproc;
#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif

	userret(p, framep->tf_pc, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

/*
 * Process the tail end of a fork() for the child.
 */
void
child_return(p)
	struct proc *p;
{

	/*
	 * Return values in the frame set by cpu_fork().
	 */

	userret(p, p->p_md.md_tf->tf_pc, 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
}
