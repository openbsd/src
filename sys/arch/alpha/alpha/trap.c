/*	$OpenBSD: trap.c,v 1.17 1997/11/17 01:26:38 deraadt Exp $	*/
/*	$NetBSD: trap.c,v 1.19 1996/11/27 01:28:30 cgd Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1996 Carnegie-Mellon University.
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
#include <sys/buf.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif

#include <machine/cpu.h>
#include <machine/reg.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif

#ifdef COMPAT_OSF1
#include <compat/osf1/osf1_syscall.h>
#endif

static __inline void userret __P((struct proc *, u_int64_t, u_quad_t));
void trap __P((const u_long, const u_long, const u_long, const u_long,
     struct trapframe *));
int unaligned_fixup __P((u_long, u_long, u_long, struct proc *));
void syscall __P((u_int64_t, struct trapframe *));
void child_return __P((struct proc *));
void ast __P((struct trapframe *));
u_long Sfloat_to_reg __P((u_int));
u_int reg_to_Sfloat __P((u_long));
u_long Tfloat_reg_cvt __P((u_long));

struct proc *fpcurproc;		/* current user of the FPU */

void		userret __P((struct proc *, u_int64_t, u_quad_t));

unsigned long	Sfloat_to_reg __P((unsigned int));
unsigned int	reg_to_Sfloat __P((unsigned long));
unsigned long	Tfloat_reg_cvt __P((unsigned long));
#ifdef FIX_UNALIGNED_VAX_FP
unsigned long	Ffloat_to_reg __P((unsigned int));
unsigned int	reg_to_Ffloat __P((unsigned long));
unsigned long	Gfloat_reg_cvt __P((unsigned long));
#endif

int		unaligned_fixup __P((unsigned long, unsigned long,
		    unsigned long, struct proc *));

/*
 * Define the code needed before returning to user mode, for
 * trap and syscall.
 */
void
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

char	*trap_type[] = {
	"interrupt",			/*  0 ALPHA_KENTRY_INT */
	"arithmetic trap",		/*  1 ALPHA_KENTRY_ARITH */
	"memory management fault",	/*  2 ALPHA_KENTRY_MM */
	"instruction fault",		/*  3 ALPHA_KENTRY_IF */
	"unaligned access fault",	/*  4 ALPHA_KENTRY_UNA */
	"system call",			/*  5 ALPHA_KENTRY_SYS */
};
int	trap_types = sizeof trap_type / sizeof trap_type[0];

/*
 * Trap is called from locore to handle most types of processor traps.
 * System calls are broken out for efficiency and ASTs are broken out
 * to make the code a bit cleaner and more representative of the
 * Alpha architecture.
 */
/*ARGSUSED*/
void
trap(a0, a1, a2, entry, framep)
	const unsigned long a0, a1, a2, entry;
	struct trapframe *framep;
{
	register struct proc *p;
	register int i;
	u_long ucode;
	u_quad_t sticks;
	caddr_t v;
	int user;
	int typ;

	cnt.v_trap++;
	p = curproc;
	v = 0;
	ucode = 0;
	user = (framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) != 0;
#ifdef DDB
	framep->tf_regs[FRAME_SP] = (long)framep + FRAME_SIZE*8;
#endif
	if (user)  {
		sticks = p->p_sticks;
		p->p_md.md_tf = framep;
	} else {
#ifdef DIAGNOSTIC
		sticks = 0xdeadbeef;		/* XXX for -Wuninitialized */
#endif
	}

	switch (entry) {
	case ALPHA_KENTRY_UNA:
		/*
		 * If user-land, do whatever fixups, printing, and
		 * signalling is appropriate (based on system-wide
		 * and per-process unaligned-access-handling flags).
		 */
		if (user) {
			if ((i = unaligned_fixup(a0, a1, a2, p)) == 0)
				goto out;

			ucode = VM_PROT_NONE;	/* XXX determine */
			v = (caddr_t)a0;
			if (i == SIGBUS)
				typ = BUS_ADRALN;
			else
				typ = SEGV_MAPERR;
			break;
		}

		/*
		 * Unaligned access from kernel mode is always an error,
		 * EVEN IF A COPY FAULT HANDLER IS SET!
		 *
		 * It's an error if a copy fault handler is set because
		 * the various routines which do user-initiated copies
		 * do so in a bcopy-like manner.  In other words, the
		 * kernel never assumes that pointers provided by the
		 * user are properly aligned, and so if the kernel
		 * does cause an unaligned access it's a kernel bug.
		 */
		goto we_re_toast;

	case ALPHA_KENTRY_ARITH:
		/* 
		 * If user-land, just give a SIGFPE.  Should do
		 * software completion and IEEE handling, if the
		 * user has requested that.
		 */
		if (user) {
sigfpe:			i = SIGFPE;
			v = NULL;		/* XXX determine */
			ucode = a0;		/* exception summary */
			typ = FPE_FLTINV;	/* XXX? */
			break;
		}

		/* Always fatal in kernel.  Should never happen. */
		goto we_re_toast;

	case ALPHA_KENTRY_IF:
		/*
		 * These are always fatal in kernel, and should never
		 * happen, unless they're breakpoints of course.
		 */
		if (!user)
			goto we_re_toast;

		switch (a0) {
		case ALPHA_IF_CODE_GENTRAP:
			if (framep->tf_regs[FRAME_A0] == -2) /* weird! */
				goto sigfpe;
		case ALPHA_IF_CODE_BPT:
		case ALPHA_IF_CODE_BUGCHK:
			/* XXX what is the address?  Guess on a1 for now */
			v = (caddr_t)a1;
			ucode = 0;		/* XXX determine */
			i = SIGTRAP;
			typ = TRAP_BRKPT;
			break;

		case ALPHA_IF_CODE_OPDEC:
			/* XXX what is the address?  Guess on a1 for now */
			v = (caddr_t)a1;
			ucode = 0;		/* XXX determine */
#ifdef NEW_PMAP
{
int instr;
printf("REAL SIGILL: PC = 0x%lx, RA = 0x%lx\n", framep->tf_regs[FRAME_PC], framep->tf_regs[FRAME_RA]);
printf("INSTRUCTION (%d) = 0x%lx\n", copyin((void*)framep->tf_regs[FRAME_PC] - 4, &instr, 4), instr);
regdump(framep);
panic("foo");
}
#endif
			i = SIGILL;
			typ = ILL_ILLOPC;
			break;

		case ALPHA_IF_CODE_FEN:
			/*
			 * on exit from the kernel, if proc == fpcurproc,
			 * FP is enabled.
			 */
			if (fpcurproc == p) {
				printf("trap: fp disabled for fpcurproc == %p",
				    p);
				goto we_re_toast;
			}
	
			alpha_pal_wrfen(1);
			if (fpcurproc)
				savefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
			fpcurproc = p;
			restorefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);
			alpha_pal_wrfen(0);

			p->p_md.md_flags |= MDP_FPUSED;
			goto out;

		default:
			printf("trap: unknown IF type 0x%lx\n", a0);
			goto we_re_toast;
		}
		break;

	case ALPHA_KENTRY_MM:
#ifdef NEW_PMAP
		printf("mmfault: 0x%lx, 0x%lx, %d\n", a0, a1, a2);
#endif
		switch (a1) {
		case ALPHA_MMCSR_FOR:
		case ALPHA_MMCSR_FOE:
#ifdef NEW_PMAP
			printf("mmfault for/foe in\n");
#endif
			pmap_emulate_reference(p, a0, user, 0);
#ifdef NEW_PMAP
			printf("mmfault for/foe out\n");
#endif
			goto out;

		case ALPHA_MMCSR_FOW:
#ifdef NEW_PMAP
			printf("mmfault fow in\n");
#endif
			pmap_emulate_reference(p, a0, user, 1);
#ifdef NEW_PMAP
			printf("mmfault fow out\n");
#endif
			goto out;

		case ALPHA_MMCSR_INVALTRANS:
		case ALPHA_MMCSR_ACCESS:
	    	{
			register vm_offset_t va;
			register struct vmspace *vm;
			register vm_map_t map;
			vm_prot_t ftype;
			int rv;
			extern vm_map_t kernel_map;

#ifdef NEW_PMAP
			printf("mmfault invaltrans/access in\n");
#endif
			/*
			 * If it was caused by fuswintr or suswintr,
			 * just punt.  Note that we check the faulting
			 * address against the address accessed by
			 * [fs]uswintr, in case another fault happens
			 * when they are running.
			 */
			if (!user &&
			    p != NULL &&
			    p->p_addr->u_pcb.pcb_onfault ==
			      (unsigned long)fswintrberr &&
			    p->p_addr->u_pcb.pcb_accessaddr == a0) {
#ifdef NEW_PMAP
				printf("mmfault nfintr in\n");
#endif
				framep->tf_regs[FRAME_PC] =
				    p->p_addr->u_pcb.pcb_onfault;
				p->p_addr->u_pcb.pcb_onfault = 0;
#ifdef NEW_PMAP
				printf("mmfault nfintr out\n");
#endif
				goto out;
			}

			/*
			 * It is only a kernel address space fault iff:
			 *	1. !user and
			 *	2. pcb_onfault not set or
			 *	3. pcb_onfault set but kernel space data fault
			 * The last can occur during an exec() copyin where the
			 * argument space is lazy-allocated.
			 */
			if (!user && (a0 >= VM_MIN_KERNEL_ADDRESS ||
			    p == NULL || p->p_addr->u_pcb.pcb_onfault == 0))
				map = kernel_map;
			else {
				vm = p->p_vmspace;
				map = &vm->vm_map;
			}
	
			switch (a2) {
			case -1:		/* instruction fetch fault */
			case 0:			/* load instruction */
				ftype = VM_PROT_READ;
				break;
			case 1:			/* store instruction */
				ftype = VM_PROT_WRITE;
				break;
#ifdef DIAGNOSTIC
			default:		/* XXX gcc -Wuninitialized */
				goto we_re_toast;
#endif
			}
	
			va = trunc_page((vm_offset_t)a0);
#ifdef NEW_PMAP
			printf("mmfault going to vm_fault\n");
#endif
			rv = vm_fault(map, va, ftype, FALSE);
#ifdef NEW_PMAP
			printf("mmfault back from vm_fault\n");
#endif
			/*
			 * If this was a stack access we keep track of the
			 * maximum accessed stack size.  Also, if vm_fault
			 * gets a protection failure it is due to accessing
			 * the stack region outside the current limit and
			 * we need to reflect that as an access error.
			 */
			if (map != kernel_map &&
			    (caddr_t)va >= vm->vm_maxsaddr) {
				if (rv == KERN_SUCCESS) {
					unsigned nss;
	
					nss = clrnd(btoc(USRSTACK -
					    (unsigned long)va));
					if (nss > vm->vm_ssize)
						vm->vm_ssize = nss;
				} else if (rv == KERN_PROTECTION_FAILURE)
					rv = KERN_INVALID_ADDRESS;
			}
			if (rv == KERN_SUCCESS) {
#ifdef NEW_PMAP
				printf("mmfault vm_fault success\n");
#endif
				goto out;
			}

			if (!user) {
#ifdef NEW_PMAP
				printf("mmfault check copyfault\n");
#endif
				/* Check for copyin/copyout fault */
				if (p != NULL &&
				    p->p_addr->u_pcb.pcb_onfault != 0) {
					framep->tf_regs[FRAME_PC] =
					    p->p_addr->u_pcb.pcb_onfault;
					p->p_addr->u_pcb.pcb_onfault = 0;
					goto out;
				}
				goto we_re_toast;
			}
			v = (caddr_t)a0;
			ucode = ftype;
			i = SIGSEGV;
			typ = SEGV_MAPERR;
			break;
		    }

		default:
			printf("trap: unknown MMCSR value 0x%lx\n", a1);
			goto we_re_toast;
		}
		break;

	default:
	we_re_toast:
#ifdef DDB
		if (kdb_trap(entry, a0, framep))
			return;
#endif
		goto dopanic;
	}

	trapsignal(p, i, ucode, typ, v);
out:
	if (user)
		userret(p, framep->tf_regs[FRAME_PC], sticks);
	return;

dopanic:
	{
		const char *entryname = "???";

		if (entry > 0 && entry < trap_types)
			entryname = trap_type[entry];

		printf("\n");
		printf("fatal %s trap:\n", user ? "user" : "kernel");
		printf("\n");
		printf("    trap entry = 0x%lx (%s)\n", entry, entryname);
		printf("    a0         = 0x%lx\n", a0);
		printf("    a1         = 0x%lx\n", a1);
		printf("    a2         = 0x%lx\n", a2);
		printf("    pc         = 0x%lx\n", framep->tf_regs[FRAME_PC]);
		printf("    ra         = 0x%lx\n", framep->tf_regs[FRAME_RA]);
		printf("    curproc    = %p\n", curproc);
		if (curproc != NULL)
			printf("        pid = %d, comm = %s\n", curproc->p_pid,
			    curproc->p_comm);
		printf("\n");
	}

	/* XXX dump registers */
	/* XXX kernel debugger */

	panic("trap");
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
	if ((framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) == 0) {
		panic("syscall");
#endif
	cnt.v_syscall++;
	p = curproc;
	p->p_md.md_tf = framep;
	opc = framep->tf_regs[FRAME_PC] - 4;
	sticks = p->p_sticks;

	callp = p->p_emul->e_sysent;
	numsys = p->p_emul->e_nsysent;

#ifdef COMPAT_OSF1
	if (p->p_emul == &emul_osf1) 
		switch (code) {
		case OSF1_SYS_syscall:
			/* OSF/1 syscall() */
			code = framep->tf_regs[FRAME_A0];
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
		code = framep->tf_regs[FRAME_A0];
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
		error = copyin((caddr_t)(alpha_pal_rdusp()), &args[6],
		    (nargs - 6) * sizeof(u_int64_t));
	case 6:	
		args[5] = framep->tf_regs[FRAME_A5];
	case 5:	
		args[4] = framep->tf_regs[FRAME_A4];
	case 4:	
		args[3] = framep->tf_regs[FRAME_A3];
	case 3:	
		args[2] = framep->tf_regs[FRAME_A2];
	case 2:	
		args[1] = framep->tf_regs[FRAME_A1];
	case 1:	
		args[0] = framep->tf_regs[FRAME_A0];
	case 0:
		break;
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_argsize, args + hidden);
#endif
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args + hidden);
#ifdef NEW_PMAP
	printf("called from 0x%lx, ra 0x%lx\n", framep->tf_regs[FRAME_PC], framep->tf_regs[FRAME_RA]);
#endif
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
		framep->tf_regs[FRAME_PC] = opc;
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
#ifdef NEW_PMAP
	printf("outgoing pc 0x%lx, ra 0x%lx\n", framep->tf_regs[FRAME_PC], framep->tf_regs[FRAME_RA]);
#endif
#endif

	userret(p, framep->tf_regs[FRAME_PC], sticks);
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

	userret(p, p->p_md.md_tf->tf_regs[FRAME_PC], 0);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, SYS_fork, 0, 0);
#endif
}

/*
 * Process an asynchronous software trap.
 * This is relatively easy.
 */
void
ast(framep)
	struct trapframe *framep;
{
	register struct proc *p;
	u_quad_t sticks;

	p = curproc;
	sticks = p->p_sticks;
	p->p_md.md_tf = framep;

	if ((framep->tf_regs[FRAME_PS] & ALPHA_PSL_USERMODE) == 0)
		panic("ast and not user");

	cnt.v_soft++;

	astpending = 0;
	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}

	userret(p, framep->tf_regs[FRAME_PC], sticks);
}

/*
 * Unaligned access handler.  It's not clear that this can get much slower...
 *
 */
const static int reg_to_framereg[32] = {
	FRAME_V0,	FRAME_T0,	FRAME_T1,	FRAME_T2,
	FRAME_T3,	FRAME_T4,	FRAME_T5,	FRAME_T6,
	FRAME_T7,	FRAME_S0,	FRAME_S1,	FRAME_S2,
	FRAME_S3,	FRAME_S4,	FRAME_S5,	FRAME_S6,
	FRAME_A0,	FRAME_A1,	FRAME_A2,	FRAME_A3,
	FRAME_A4,	FRAME_A5,	FRAME_T8,	FRAME_T9,
	FRAME_T10,	FRAME_T11,	FRAME_RA,	FRAME_T12,
	FRAME_AT,	FRAME_GP,	FRAME_SP,	-1,
};

#define	irp(p, reg)							\
	((reg_to_framereg[(reg)] == -1) ? NULL :			\
	    &(p)->p_md.md_tf->tf_regs[reg_to_framereg[(reg)]])

#define	frp(p, reg)							\
	(&(p)->p_addr->u_pcb.pcb_fp.fpr_regs[(reg)])

#define	dump_fp_regs()							\
	if (p == fpcurproc) {						\
		alpha_pal_wrfen(1);					\
		savefpstate(&fpcurproc->p_addr->u_pcb.pcb_fp);		\
		alpha_pal_wrfen(0);					\
		fpcurproc = NULL;					\
	}

#define	unaligned_load(storage, ptrf, mod)				\
	if (copyin((caddr_t)va, &(storage), sizeof (storage)) == 0 &&	\
	    (regptr = ptrf(p, reg)) != NULL)				\
		signal = 0;						\
	else								\
		break;							\
	*regptr = mod (storage);

#define	unaligned_store(storage, ptrf, mod)				\
	if ((regptr = ptrf(p, reg)) == NULL)				\
		break;							\
	(storage) = mod (*regptr);					\
	if (copyout(&(storage), (caddr_t)va, sizeof (storage)) == 0)	\
		signal = 0;						\
	else								\
		break;

#define	unaligned_load_integer(storage)					\
	unaligned_load(storage, irp, )

#define	unaligned_store_integer(storage)				\
	unaligned_store(storage, irp, )

#define	unaligned_load_floating(storage, mod)				\
	dump_fp_regs();							\
	unaligned_load(storage, frp, mod)

#define	unaligned_store_floating(storage, mod)				\
	dump_fp_regs();							\
	unaligned_store(storage, frp, mod)

unsigned long
Sfloat_to_reg(s)
	unsigned int s;
{
	unsigned long sign, expn, frac;
	unsigned long result;

	sign = (s & 0x80000000) >> 31;
	expn = (s & 0x7f800000) >> 23;
	frac = (s & 0x007fffff) >>  0;

	/* map exponent part, as appropriate. */
	if (expn == 0xff)
		expn = 0x7ff;
	else if ((expn & 0x80) != 0)
		expn = (0x400 | (expn & ~0x80));
	else if ((expn & 0x80) == 0 && expn != 0)
		expn = (0x380 | (expn & ~0x80));

	result = (sign << 63) | (expn << 52) | (frac << 29);
	return (result);
}

unsigned int
reg_to_Sfloat(r)
	unsigned long r;
{
	unsigned long sign, expn, frac;
	unsigned int result;

	sign = (r & 0x8000000000000000) >> 63;
	expn = (r & 0x7ff0000000000000) >> 52;
	frac = (r & 0x000fffffe0000000) >> 29;

	/* map exponent part, as appropriate. */
	expn = (expn & 0x7f) | ((expn & 0x400) != 0 ? 0x80 : 0x00);

	result = (sign << 31) | (expn << 23) | (frac << 0);
	return (result);
}

/*
 * Conversion of T floating datums to and from register format
 * requires no bit reordering whatsoever.
 */
unsigned long
Tfloat_reg_cvt(input)
	unsigned long input;
{

	return (input);
}

#ifdef FIX_UNALIGNED_VAX_FP
unsigned long
Ffloat_to_reg(f)
	unsigned int f;
{
	unsigned long sign, expn, frlo, frhi;
	unsigned long result;

	sign = (f & 0x00008000) >> 15;
	expn = (f & 0x00007f80) >>  7;
	frhi = (f & 0x0000007f) >>  0;
	frlo = (f & 0xffff0000) >> 16;

	/* map exponent part, as appropriate. */
	if ((expn & 0x80) != 0)
		expn = (0x400 | (expn & ~0x80));
	else if ((expn & 0x80) == 0 && expn != 0)
		expn = (0x380 | (expn & ~0x80));

	result = (sign << 63) | (expn << 52) | (frhi << 45) | (frlo << 29);
	return (result);
}

unsigned int
reg_to_Ffloat(r)
	unsigned long r;
{
	unsigned long sign, expn, frhi, frlo;
	unsigned int result;

	sign = (r & 0x8000000000000000) >> 63;
	expn = (r & 0x7ff0000000000000) >> 52;
	frhi = (r & 0x000fe00000000000) >> 45;
	frlo = (r & 0x00001fffe0000000) >> 29;

	/* map exponent part, as appropriate. */
	expn = (expn & 0x7f) | ((expn & 0x400) != 0 ? 0x80 : 0x00);

	result = (sign << 15) | (expn << 7) | (frhi << 0) | (frlo << 16);
	return (result);
}

/*
 * Conversion of G floating datums to and from register format is
 * symmetrical.  Just swap shorts in the quad...
 */
unsigned long
Gfloat_reg_cvt(input)
	unsigned long input;
{
	unsigned long a, b, c, d;
	unsigned long result;

	a = (input & 0x000000000000ffff) >> 0;
	b = (input & 0x00000000ffff0000) >> 16;
	c = (input & 0x0000ffff00000000) >> 32;
	d = (input & 0xffff000000000000) >> 48;

	result = (a << 48) | (b << 32) | (c << 16) | (d << 0);
	return (result);
}
#endif /* FIX_UNALIGNED_VAX_FP */

extern int	alpha_unaligned_print, alpha_unaligned_fix;
extern int	alpha_unaligned_sigbus;

int
unaligned_fixup(va, opcode, reg, p)
	unsigned long va, opcode, reg;
	struct proc *p;
{
	int doprint, dofix, dosigbus;
	int signal, size;
	const char *type;
	unsigned long *regptr, longdata;
	int intdata;		/* signed to get extension when storing */
	struct {
		const char *type;	/* opcode name */
		int size;		/* size, 0 if fixup not supported */
	} tab[0x10] = {
#ifdef FIX_UNALIGNED_VAX_FP
		{ "ldf",	4 },	{ "ldg",	8 },
#else
		{ "ldf",	0 },	{ "ldg",	0 },
#endif
		{ "lds",	4 },	{ "ldt",	8 },
#ifdef FIX_UNALIGNED_VAX_FP
		{ "stf",	4 },	{ "stg",	8 },
#else
		{ "stf",	0 },	{ "stg",	0 },
#endif
		{ "sts",	4 },	{ "stt",	8 },
		{ "ldl",	4 },	{ "ldq",	8 },
		{ "ldl_l",	0 },	{ "ldq_l",	0 },	/* can't fix */
		{ "stl",	4 },	{ "stq",	8 },
		{ "stl_c",	0 },	{ "stq_c",	0 },	/* can't fix */
	};
	int typ;

	/*
	 * Figure out what actions to take.
	 *
	 * XXX In the future, this should have a per-process component
	 * as well.
	 */
	doprint = alpha_unaligned_print;
	dofix = alpha_unaligned_fix;
	dosigbus = alpha_unaligned_sigbus;

	/*
	 * Find out which opcode it is.  Arrange to have the opcode
	 * printed if it's an unknown opcode.
	 */
	if (opcode >= 0x20 && opcode <= 0x2f) {
		type = tab[opcode - 0x20].type;
		size = tab[opcode - 0x20].size;
	} else {
		type = "0x%lx";
		size = 0;
	}

	/*
	 * See if the user can access the memory in question.
	 * Even if it's an unknown opcode, SEGV if the access
	 * should have failed.
	 */
	if (!useracc((caddr_t)va, size ? size : 1, B_WRITE)) {
		signal = SIGSEGV;
		goto out;
	}

	/*
	 * If we're supposed to be noisy, squawk now.
	 */
	if (doprint) {
		uprintf("pid %d (%s): unaligned access: va=0x%lx pc=0x%lx ra=0x%lx op=",
		    p->p_pid, p->p_comm, va, p->p_md.md_tf->tf_regs[FRAME_PC],
		    p->p_md.md_tf->tf_regs[FRAME_PC]);
		uprintf(type, opcode);
		uprintf("\n");
	}

	/*
	 * If we should try to fix it and know how, give it a shot.
	 *
	 * We never allow bad data to be unknowingly used by the
	 * user process.  That is, if we decide not to fix up an
	 * access we cause a SIGBUS rather than letting the user
	 * process go on without warning.
	 *
	 * If we're trying to do a fixup, we assume that things
	 * will be botched.  If everything works out OK, 
	 * unaligned_{load,store}_* clears the signal flag.
	 */
	signal = SIGBUS;
	typ = BUS_ADRALN;
	if (dofix && size != 0) {
		switch (opcode) {
#ifdef FIX_UNALIGNED_VAX_FP
		case 0x20:			/* ldf */
			unaligned_load_floating(intdata, Ffloat_to_reg);
			break;

		case 0x21:			/* ldg */
			unaligned_load_floating(longdata, Gfloat_reg_cvt);
			break;
#endif

		case 0x22:			/* lds */
			unaligned_load_floating(intdata, Sfloat_to_reg);
			break;

		case 0x23:			/* ldt */
			unaligned_load_floating(longdata, Tfloat_reg_cvt);
			break;

#ifdef FIX_UNALIGNED_VAX_FP
		case 0x24:			/* stf */
			unaligned_store_floating(intdata, reg_to_Ffloat);
			break;

		case 0x25:			/* stg */
			unaligned_store_floating(longdata, Gfloat_reg_cvt);
			break;
#endif

		case 0x26:			/* sts */
			unaligned_store_floating(intdata, reg_to_Sfloat);
			break;

		case 0x27:			/* stt */
			unaligned_store_floating(longdata, Tfloat_reg_cvt);
			break;

		case 0x28:			/* ldl */
			unaligned_load_integer(intdata);
			break;

		case 0x29:			/* ldq */
			unaligned_load_integer(longdata);
			break;

		case 0x2c:			/* stl */
			unaligned_store_integer(intdata);
			break;

		case 0x2d:			/* stq */
			unaligned_store_integer(longdata);
			break;

#ifdef DIAGNOSTIC
		default:
			panic("unaligned_fixup: can't get here");
#endif
		}
	} 

	/*
	 * Force SIGBUS if requested.
	 */
	if (dosigbus)
		signal = SIGBUS;

out:
	return (signal);
}
