/*
 * Mach Operating System
 * Copyright (c) 1993-1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON AND OMRON ALLOW FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON AND OMRON DISCLAIM ANY LIABILITY OF ANY KIND
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

#include <sys/types.h>
#include <vm/vm.h>
#include <vm/vm_kern.h>			/* kernel_map */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/ktrace.h>
#include <machine/cpu.h>		/* DMT_VALID, etc. */
#include <machine/m88100.h>		/* DMT_VALID, etc. */
#include <machine/trap.h>
#include <machine/psl.h>		/* FIP_E, etc. */

#include <sys/systm.h>

#if (DDB)
#include <machine/db_machdep.h>
#endif /* DDB */

int stop_on_user_memory_error = 0;

#define TRAPTRACE
#if defined(TRAPTRACE)
unsigned traptrace = 0;
#endif

#if DDB
#define DEBUG_MSG db_printf
#else
#define DEBUG_MSG printf
#endif /* DDB */

#ifdef JEFF_DEBUG
# undef  DEBUG_MSG
# define DEBUG_MSG raw_printf
#endif

#define USERMODE(PSR)   (((struct psr*)&(PSR))->psr_mode == 0)
#define SYSTEMMODE(PSR) (((struct psr*)&(PSR))->psr_mode != 0)

/* XXX MAJOR CLEANUP REQUIRED TO PORT TO BSD */

char	*trap_type[] = {
	"Reset",
	"Interrupt Exception",
	"Instruction Access",
	"Data Access Exception",
	"Misaligned Access",
	"Unimplemented Opcode",
	"Privileg Violation",
	"Bounds Check Violation",
	"Illegal Integer Divide",
	"Integer Overflow",
	"Error Exception",
};

int	trap_types = sizeof trap_type / sizeof trap_type[0];

static inline void
userret(struct proc *p, struct m88100_saved_state *frame, u_quad_t oticks)
{
	int sig;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;

	if (want_ast) {
		want_ast = 0;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
	}

	if (want_resched) {
		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we put ourselves on the run queue
		 * but before we switched, we might not be on the queue
		 * indicated by our priority.
		 */
		(void) splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		(void) spl0();
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL)
		addupc_task(p, frame->sxip & ~3,
			 (int)(p->p_sticks - oticks));

	curpriority = p->p_priority;
}

void
panictrap(int type, struct m88100_saved_state *frame)
{
	static int panicing = 0;
	if (panicing++ == 0) {
		printf("trap type %d, v = %x, frame %x\n", type, frame->sxip & ~3, frame);
		regdump(frame);
	}
	if ((u_int)type < trap_types)
		panic(trap_type[type]);
	panic("trap");
	/*NOTREACHED*/
}

/*ARGSUSED*/
void
trap(unsigned type, struct m88100_saved_state *frame)
{
    struct proc *p;
    u_quad_t sticks = 0;
    vm_map_t map;
    vm_offset_t va;
    vm_prot_t ftype;
    unsigned nss, fault_addr;
    struct vmspace *vm;
    int result;
    int sig = 0;

    extern vm_map_t kernel_map;
    extern int fubail(), subail();

    cnt.v_trap++;
    if ((p = curproc) == NULL)
	p = &proc0;

    if (USERMODE(frame->epsr)) {
	sticks = p->p_sticks;
	type += T_USER;
	p->p_md.md_tf = frame;	/* for ptrace/signals */
    }

    switch(type)
    {
    default:
	panictrap(frame->vector, frame);
	/*NOTREACHED*/

#if defined(DDB)
    case T_KDB_BREAK:
	/*FALLTHRU*/
    case T_KDB_BREAK+T_USER:
    {
        int s = db_splhigh();
        db_enable_interrupt(); /* turn interrupts on */
        ddb_break_trap(T_KDB_BREAK,(db_regs_t*)frame);
        db_disable_interrupt(); /* shut them back off */
        db_splx(s);
        return;
    }
    case T_KDB_ENTRY:
	/*FALLTHRU*/
    case T_KDB_ENTRY+T_USER:
    {
        int s = db_splhigh();
        db_enable_interrupt(); /* turn interrupts on */
        ddb_entry_trap(T_KDB_ENTRY,(db_regs_t*)frame);
        db_disable_interrupt(); /* shut them back off */
        db_splx(s);
        return;
    }

#if 0
    case T_ILLFLT:
    {
        int s = db_splhigh();
        db_enable_interrupt(); /* turn interrupts on */
        ddb_error_trap(type == T_ILLFLT ? "unimplemented opcode" :
            "error fault", (db_regs_t*)frame);
        db_disable_interrupt(); /* shut them back off */
        db_splx(s);
        return;
    }
#endif /* 0 */
#endif /* DDB */

    case T_MISALGNFLT:
        DEBUG_MSG("kernel misalgined "
		"access exception @ 0x%08x\n", frame->sxip);
	panictrap(frame->vector, frame);
	break;

    case T_INSTFLT:
	/* kernel mode instruction access fault */
	/* XXX I think this should be illegal, but not sure. Will leave
	 * the way it is for now. Should never,never happen for a non-paged
	 * kernel
	 */
	/*FALLTHRU*/
    case T_DATAFLT:
	/* kernel mode data fault */
	/*
	 * if the faulting address is in user space, handle it in
	 * the context of the user process. Else, use kernel map.
	 */

	if (type == T_DATAFLT) {
		fault_addr = frame->dma0;
		if (frame->dmt0 & (DMT_WRITE|DMT_LOCKBAR))
		    ftype = VM_PROT_READ|VM_PROT_WRITE;
		else
		    ftype = VM_PROT_READ;
	} else {
		fault_addr = frame->sxip & XIP_ADDR;
		ftype = VM_PROT_READ;
	}

	va = trunc_page((vm_offset_t)fault_addr);

	vm = p->p_vmspace;
	map = &vm->vm_map;

	/* if instruction fault or data fault on a kernel address... */
	if ((type == T_INSTFLT) || (frame->dmt0 & DMT_DAS))
		map = kernel_map;
	
	/* 
	 * We don't want to call vm_fault() if it is fuwintr() or
	 * suwintr(). These routines are for copying from interrupt
	 * context and vm_fault() can potentially sleep.
	 */

	if (p->p_addr->u_pcb.pcb_onfault == (int)fubail ||
		p->p_addr->u_pcb.pcb_onfault == (int)subail)
		goto outtahere;

	result = vm_fault(map, va, ftype, FALSE); 

        if (result == KERN_SUCCESS) {
		/*
		 * We could resolve the fault. Call data_access_emulation
		 * to drain the data unit pipe line and reset dmt0 so that
		 * trap won't get called again. For inst faults, back up
		 * the pipe line.
		 */
		if (type == T_DATAFLT) {
		    data_access_emulation(frame);
		    frame->dmt0 = 0;
		} else {
		    frame->sfip = frame->snip & ~FIP_E;
		    frame->snip = frame->sxip & ~NIP_E;
		}	
		return;
	}

	/* XXX Is this right? */
	if (type == T_DATAFLT && (frame->dmt0 & DMT_DAS) == 0)
		goto user_fault;

	/*
	 * if still the fault is not resolved ...
	 */
	if (!p->p_addr->u_pcb.pcb_onfault)
		panictrap(frame->vector, frame);

    outtahere:
	frame->snip = ((unsigned)p->p_addr->u_pcb.pcb_onfault    ) | FIP_V;
	frame->sfip = ((unsigned)p->p_addr->u_pcb.pcb_onfault + 4) | FIP_V;
	frame->sxip = 0;
	frame->dmt0 = 0;	/* XXX what about other trans. in data unit */
	return;

    case T_INSTFLT+T_USER:
	/* User mode instruction access fault */
	/*FALLTHRU*/
    case T_DATAFLT+T_USER:
    user_fault:
	sig = SIGILL;
	if (type == T_INSTFLT+T_USER)
		fault_addr = frame->sxip & XIP_ADDR;
	else
		fault_addr = frame->dma0;
	if (frame->dmt0 & (DMT_WRITE|DMT_LOCKBAR))
	    ftype = VM_PROT_READ|VM_PROT_WRITE;
	else
	    ftype = VM_PROT_READ;

	va = trunc_page((vm_offset_t)fault_addr);

	vm = p->p_vmspace;
	map = &vm->vm_map;

	result = vm_fault(map, va, ftype, FALSE); 

	if ((caddr_t)va >= vm->vm_maxsaddr) {
		if (result == KERN_SUCCESS) {
			nss = clrnd(USRSTACK - va);/* XXX check this */
			if (nss > vm->vm_ssize)
				vm->vm_ssize = nss;
		} else if (result == KERN_PROTECTION_FAILURE)
			result = KERN_INVALID_ADDRESS;
	}

        if (result == KERN_SUCCESS) {
		if (type == T_DATAFLT+T_USER) {
			/*
			 * We could resolve the fault. Call
			 * data_access_emulation to drain the data unit
			 * pipe line and reset dmt0 so that trap won't
			 * get called again.
			 */
			data_access_emulation(frame);
			frame->dmt0 = 0;
		} else {
		    /* back up SXIP, SNIP clearing the the Error bit */
		    frame->sfip = frame->snip & ~FIP_E;
		    frame->snip = frame->sxip & ~NIP_E;
		}
	} else {
		sig = result == SIGSEGV;
	}

	break;

    case T_MISALGNFLT+T_USER:
	sig = SIGBUS;
	break;

    case T_PRIVINFLT+T_USER:
    case T_ILLFLT+T_USER:
	sig = SIGILL;
	break;

    case T_BNDFLT+T_USER:
    case T_ZERODIV+T_USER:
    case T_OVFFLT+T_USER:
	sig = SIGBUS;
	break;

    case T_FPEPFLT+T_USER:
    case T_FPEIFLT+T_USER:
	sig = SIGFPE;
	break;

    case T_ASTFLT+T_USER:
	want_ast = 0;
	(void) spl0();
	if (ssir & SIR_NET) {
		siroff(SIR_NET);
		cnt.v_soft++;
		netintr();
	}
	if (ssir & SIR_CLOCK) {
		siroff(SIR_CLOCK);
		cnt.v_soft++;
		/* XXXX softclock(&frame.f_stackadj); */
		softclock();
	}
	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}
	break;

    case T_SIGTRAP+T_USER:
	break;

    case T_STEPBPT+T_USER:
	/*
	 * This trap is used by the kernel to support single-step
	 * debugging (although any user could generate this trap
	 * which should probably be handled differently). When a
	 * process is continued by a debugger with the PT_STEP
	 * function of ptrace (single step), the kernel inserts
	 * one or two breakpoints in the user process so that only
	 * one instruction (or two in the case of a delayed branch)
	 * is executed.  When this breakpoint is hit, we get the
	 * T_STEPBPT trap.
	 */
	frame->sfip = frame->snip;    /* set up next FIP */
	frame->snip = frame->sxip;    /* set up next NIP */
	break;

    case T_USERBPT+T_USER:
	/*
	 * This trap is meant to be used by debuggers to implement
	 * breakpoint debugging.  When we get this trap, we just
	 * return a signal which gets caught by the debugger.
	 */

	frame->sfip = frame->snip;    /* set up the next FIP */
	frame->snip = frame->sxip;    /* set up the next NIP */
	break;

    }

    /*
     * If trap from supervisor mode, just return
     */
    if (SYSTEMMODE(frame->epsr))
	 return;

    if (sig) {
	trapsignal(p, sig, frame->vector);
	/*
         * don't want multiple faults - we are going to
	 * deliver signal.
	 */
	frame->dmt0 = 0;
    }

    userret(p, frame, sticks);
}

void error_fault(struct m88100_saved_state *frame)
{
    DEBUG_MSG("\n[ERROR FAULT (Bad News[tm]) frame 0x%08x]\n", frame);
#if DDB
    gimmeabreak();
    DEBUG_MSG("[you really can't restart after an error fault.]\n");
    gimmeabreak();
#endif /* DDB */
}

syscall(u_int code, struct m88100_saved_state *tf)
{
	register int i, nsys, *ap, nap;
	register struct sysent *callp;
	register struct proc *p;
	int error, new;
	struct args {
		int i[8];
	} args;
	int rval[2];
	u_quad_t sticks;
	extern struct pcb *curpcb;

	cnt.v_syscall++;

	callp = p->p_emul->e_sysent;
	nsys  = p->p_emul->e_nsysent;

	p = curproc;
#ifdef DIAGNOSTIC
	if (USERMODE(tf->epsr) == 0)
		panic("syscall");
	if (curpcb != &p->p_addr->u_pcb)
		panic("syscall curpcb/ppcb");
	if (tf != (struct trapframe *)((caddr_t)curpcb))
		panic("syscall trapframe");
#endif

	sticks = p->p_sticks;
	p->p_md.md_tf = tf;

	/*
	 * For 88k, all the arguments are passed in the registers (r2-r12)
	 * For syscall (and __syscall), r2 (and r3) has the actual code.
	 * __syscall  takes a quad syscall number, so that other
	 * arguments are at their natural alignments.
	 */
	ap = &tf->r[2];
	nap = 6;

	switch (code) {
	case SYS_syscall:
		code = *ap++;
		nap--;
		break;
	case SYS___syscall:
		if (callp != sysent)
			break;
		code = ap[_QUAD_LOWWORD];
		ap += 2;
		nap -= 2;
		break;
	}

	/* Callp currently points to syscall, which returns ENOSYS. */
 
	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;
	else {
		callp += code;
		i = callp->sy_narg;
		if (i > 8)
			panic("syscall nargs");
		/*
		 * just copy them; syscall stub made sure all the
		 * args are moved from user stack to registers.
		 */
		bcopy((caddr_t)ap, (caddr_t)args.i, i * 4);
	}
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p->p_tracep, code, callp->sy_narg, args.i);
#endif
	rval[0] = 0;
	rval[1] = 0;	/* doesn't seem to be used any where */
	error = (*callp->sy_call)(p, &args, rval);
	/*
	 * system call will look like:
	 *	 ld r10, r31, 32; r10,r11,r12 might be garbage.
	 *	 ld r11, r31, 36
	 *	 ld r12, r31, 40
	 *	 or r13, r0, <code>
	 *       tb0 0, r0, <128> <- xip
	 *	 br err 	  <- nip
	 *       jmp r1 	  <- fip
	 *  err: or.u r3, r0, hi16(errno)
	 *	 st r2, r3, lo16(errno)
	 *	 subu r2, r0, 1 
	 *	 jmp r1
	 *
	 * So, when we take syscall trap, sxip/snip/sfip will be as
	 * shown above.
	 * Given this,
	 * 1. If the system call returned 0, need to skip nip.
	 *	nip = fip, fip += 4
	 *    (doesn't matter what fip + 4 will be but we will never
	 *    execute this since jmp r1 at nip will change the execution flow.)
	 * 2. If the system call returned an errno > 0, plug the value
	 *    in r2, and leave nip and fip unchanged. This will have us
	 *    executing "br err" on return to user space.
	 * 3. If the system call code returned ERESTART or EJUSTRETURN,
	 *    we need to rexecute the trap instruction. Back up the pipe
	 *    line.
	 *     fip = nip, nip = xip
	 */

	if (error == 0) {
		/*
		 * If fork succeeded and we are the child, our stack
		 * has moved and the pointer tf is no longer valid,
		 * and p is wrong.  Compute the new trapframe pointer.
		 * (The trap frame invariably resides at the
		 * tippity-top of the u. area.)
		 */
		p = curproc;
		tf = USER_REGS(p);
		tf->r[2] = 0;
		tf->epsr &= ~PSR_C;
		tf->snip = tf->sfip & ~3;
		tf->sfip = tf->snip + 4;
	} else if (error > 0 /*error != ERESTART && error != EJUSTRETURN*/) {
bad:
		tf->r[2] = error;
		tf->epsr |= PSR_C;	/* fail */
		tf->snip = tf->snip & ~3;
		tf->sfip = tf->sfip & ~3;
	} else {
	/* if (error == ERESTART || error == EJUSTRETURN) 
		back up the pipe line */
		tf->sfip = tf->snip & ~3;
		tf->snip = tf->sxip & ~3;
	}
	userret(p, tf, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p->p_tracep, code, error, rval[0]);
#endif
}

#if     MACH_PCSAMPLE > 0
#include "mach_pcsample.h"
/*
 * return saved state for interrupted user thread
 */
unsigned interrupted_pc(p)
proc *p;
{
    struct m88100_saved_state *frame = &p->pcb->user_state;
    unsigned sxip = frame->sxip;
    unsigned PC = sxip & ~3; /* clear lower bits which are flags... */
    return PC;
}
#endif  /* MACH_PCSAMPLE > 0*/
