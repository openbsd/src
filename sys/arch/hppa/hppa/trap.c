/*	$OpenBSD: trap.c,v 1.100 2007/09/15 14:55:30 krw Exp $	*/

/*
 * Copyright (c) 1998-2004 Michael Shalayeff
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
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/* #define TRAPDEBUG */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/syscall.h>
#include <sys/ktrace.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>

#include <net/netisr.h>

#include "systrace.h"
#include <dev/systrace.h>

#include <uvm/uvm.h>

#include <machine/autoconf.h>

#include <machine/db_machdep.h>	/* XXX always needed for inst_store() */
#ifdef DDB
#ifdef TRAPDEBUG
#include <ddb/db_output.h>
#endif
#endif

#ifdef PTRACE
void ss_clear_breakpoints(struct proc *p);
#endif

/* single-step breakpoint */
#define SSBREAKPOINT	(HPPA_BREAK_KERNEL | (HPPA_BREAK_SS << 13))

const char *trap_type[] = {
	"invalid",
	"HPMC",
	"power failure",
	"recovery counter",
	"external interrupt",
	"LPMC",
	"ITLB miss fault",
	"instruction protection",
	"Illegal instruction",
	"break instruction",
	"privileged operation",
	"privileged register",
	"overflow",
	"conditional",
	"assist exception",
	"DTLB miss",
	"ITLB non-access miss",
	"DTLB non-access miss",
	"data protection/rights/alignment",
	"data break",
	"TLB dirty",
	"page reference",
	"assist emulation",
	"higher-priv transfer",
	"lower-priv transfer",
	"taken branch",
	"data access rights",
	"data protection",
	"unaligned data ref",
};
int trap_types = sizeof(trap_type)/sizeof(trap_type[0]);

int want_resched, astpending;

#define	frame_regmap(tf,r)	(((u_int *)(tf))[hppa_regmap[(r)]])
u_char hppa_regmap[32] = {
	offsetof(struct trapframe, tf_pad[0]) / 4,	/* r0 XXX */
	offsetof(struct trapframe, tf_r1) / 4,
	offsetof(struct trapframe, tf_rp) / 4,
	offsetof(struct trapframe, tf_r3) / 4,
	offsetof(struct trapframe, tf_r4) / 4,
	offsetof(struct trapframe, tf_r5) / 4,
	offsetof(struct trapframe, tf_r6) / 4,
	offsetof(struct trapframe, tf_r7) / 4,
	offsetof(struct trapframe, tf_r8) / 4,
	offsetof(struct trapframe, tf_r9) / 4,
	offsetof(struct trapframe, tf_r10) / 4,
	offsetof(struct trapframe, tf_r11) / 4,
	offsetof(struct trapframe, tf_r12) / 4,
	offsetof(struct trapframe, tf_r13) / 4,
	offsetof(struct trapframe, tf_r14) / 4,
	offsetof(struct trapframe, tf_r15) / 4,
	offsetof(struct trapframe, tf_r16) / 4,
	offsetof(struct trapframe, tf_r17) / 4,
	offsetof(struct trapframe, tf_r18) / 4,
	offsetof(struct trapframe, tf_t4) / 4,
	offsetof(struct trapframe, tf_t3) / 4,
	offsetof(struct trapframe, tf_t2) / 4,
	offsetof(struct trapframe, tf_t1) / 4,
	offsetof(struct trapframe, tf_arg3) / 4,
	offsetof(struct trapframe, tf_arg2) / 4,
	offsetof(struct trapframe, tf_arg1) / 4,
	offsetof(struct trapframe, tf_arg0) / 4,
	offsetof(struct trapframe, tf_dp) / 4,
	offsetof(struct trapframe, tf_ret0) / 4,
	offsetof(struct trapframe, tf_ret1) / 4,
	offsetof(struct trapframe, tf_sp) / 4,
	offsetof(struct trapframe, tf_r31) / 4,
};

void
userret(struct proc *p)
{
	int sig;

	if (astpending) {
		astpending = 0;
		uvmexp.softs++;
		if (p->p_flag & P_OWEUPC) {
			ADDUPROF(p);
		}
		if (want_resched)
			preempt(NULL);
	}

	while ((sig = CURSIG(p)) != 0)
		postsig(sig);

	p->p_cpu->ci_schedstate.spc_curpriority = p->p_priority = p->p_usrpri;
}

void
trap(type, frame)
	int type;
	struct trapframe *frame;
{
	struct proc *p = curproc;
	vaddr_t va;
	struct vm_map *map;
	struct vmspace *vm;
	register vm_prot_t vftype;
	register pa_space_t space;
	union sigval sv;
	u_int opcode;
	int ret, trapnum;
	const char *tts;
	vm_fault_t fault = VM_FAULT_INVALID;
#ifdef DIAGNOSTIC
	int oldcpl = cpl;
#endif

	trapnum = type & ~T_USER;
	opcode = frame->tf_iir;
	if (trapnum <= T_EXCEPTION || trapnum == T_HIGHERPL ||
	    trapnum == T_LOWERPL || trapnum == T_TAKENBR ||
	    trapnum == T_IDEBUG || trapnum == T_PERFMON) {
		va = frame->tf_iioq_head;
		space = frame->tf_iisq_head;
		vftype = UVM_PROT_EXEC;
	} else {
		va = frame->tf_ior;
		space = frame->tf_isr;
		if (va == frame->tf_iioq_head)
			vftype = UVM_PROT_EXEC;
		else if (inst_store(opcode))
			vftype = UVM_PROT_WRITE;
		else
			vftype = UVM_PROT_READ;
	}

	if (frame->tf_flags & TFF_LAST)
		p->p_md.md_regs = frame;

	if (trapnum > trap_types)
		tts = "reserved";
	else
		tts = trap_type[trapnum];

#ifdef TRAPDEBUG
	if (trapnum != T_INTERRUPT && trapnum != T_IBREAK)
		db_printf("trap: %x, %s for %x:%x at %x:%x, fl=%x, fp=%p\n",
		    type, tts, space, va, frame->tf_iisq_head,
		    frame->tf_iioq_head, frame->tf_flags, frame);
	else if (trapnum  == T_IBREAK)
		db_printf("trap: break instruction %x:%x at %x:%x, fp=%p\n",
		    break5(opcode), break13(opcode),
		    frame->tf_iisq_head, frame->tf_iioq_head, frame);

	{
		extern int etext;
		if (frame < (struct trapframe *)&etext) {
			printf("trap: bogus frame ptr %p\n", frame);
			goto dead_end;
		}
	}
#endif
	if (trapnum != T_INTERRUPT) {
		uvmexp.traps++;
		mtctl(frame->tf_eiem, CR_EIEM);
	}

	switch (type) {
	case T_NONEXIST:
	case T_NONEXIST | T_USER:
		/* we've got screwed up by the central scrutinizer */
		printf("trap: elvis has just left the building!\n");
		goto dead_end;

	case T_RECOVERY:
	case T_RECOVERY | T_USER:
		/* XXX will implement later */
		printf("trap: handicapped");
		goto dead_end;

#ifdef DIAGNOSTIC
	case T_EXCEPTION:
		panic("FPU/SFU emulation botch");

		/* these just can't happen ever */
	case T_PRIV_OP:
	case T_PRIV_REG:
		/* these just can't make it to the trap() ever */
	case T_HPMC:
	case T_HPMC | T_USER:
#endif
	case T_IBREAK:
	case T_DATALIGN:
	case T_DBREAK:
	dead_end:
#ifdef DDB
		if (kdb_trap (type, va, frame)) {
			if (type == T_IBREAK) {
				/* skip break instruction */
				frame->tf_iioq_head = frame->tf_iioq_tail;
				frame->tf_iioq_tail += 4;
			}
			return;
		}
#else
		if (type == T_DATALIGN)
			panic ("trap: %s at 0x%x", tts, va);
		else
			panic ("trap: no debugger for \"%s\" (%d)", tts, type);
#endif
		break;

	case T_IBREAK | T_USER:
	case T_DBREAK | T_USER: {
		int code = TRAP_BRKPT;
#ifdef PTRACE
		ss_clear_breakpoints(p);
		if (opcode == SSBREAKPOINT)
			code = TRAP_TRACE;
#endif
		/* pass to user debugger */
		trapsignal(p, SIGTRAP, type &~ T_USER, code, sv);
		}
		break;

#ifdef PTRACE
	case T_TAKENBR | T_USER:
		ss_clear_breakpoints(p);

		/* pass to user debugger */
		trapsignal(p, SIGTRAP, type &~ T_USER, TRAP_TRACE, sv);
		break;
#endif

	case T_EXCEPTION | T_USER: {
		u_int64_t *fpp = (u_int64_t *)frame->tf_cr30;
		u_int32_t *pex;
		int i, flt;

		pex = (u_int32_t *)&fpp[0];
		for (i = 0, pex++; i < 7 && !*pex; i++, pex++);
		flt = 0;
		if (i < 7) {
			u_int32_t stat = HPPA_FPU_OP(*pex);
			if (stat & HPPA_FPU_UNMPL)
				flt = FPE_FLTINV;
			else if (stat & (HPPA_FPU_V << 1))
				flt = FPE_FLTINV;
			else if (stat & (HPPA_FPU_Z << 1))
				flt = FPE_FLTDIV;
			else if (stat & (HPPA_FPU_I << 1))
				flt = FPE_FLTRES;
			else if (stat & (HPPA_FPU_O << 1))
				flt = FPE_FLTOVF;
			else if (stat & (HPPA_FPU_U << 1))
				flt = FPE_FLTUND;
			/* still left: under/over-flow w/ inexact */

			/* cleanup exceptions (XXX deliver all ?) */
			while (i++ < 7)
				*pex++ = 0;
		}
		/* reset the trap flag, as if there was none */
		fpp[0] &= ~(((u_int64_t)HPPA_FPU_T) << 32);
		/* flush out, since load is done from phys, only 4 regs */
		fdcache(HPPA_SID_KERNEL, (vaddr_t)fpp, 8 * 4);

		sv.sival_int = va;
		trapsignal(p, SIGFPE, type &~ T_USER, flt, sv);
		}
		break;

	case T_EMULATION:
		panic("trap: emulation trap in the kernel");
		break;

	case T_EMULATION | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_COPROC, sv);
		break;

	case T_OVERFLOW | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGFPE, type &~ T_USER, FPE_INTOVF, sv);
		break;

	case T_CONDITION | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGFPE, type &~ T_USER, FPE_INTDIV, sv);
		break;

	case T_PRIV_OP | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_PRVOPC, sv);
		break;

	case T_PRIV_REG | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGILL, type &~ T_USER, ILL_PRVREG, sv);
		break;

		/* these should never got here */
	case T_HIGHERPL | T_USER:
	case T_LOWERPL | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGSEGV, vftype, SEGV_ACCERR, sv);
		break;

	case T_IPROT | T_USER:
	case T_DPROT | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGSEGV, vftype, SEGV_ACCERR, sv);
		break;

	case T_ITLBMISSNA:
	case T_ITLBMISSNA | T_USER:
	case T_DTLBMISSNA:
	case T_DTLBMISSNA | T_USER:
		if (space == HPPA_SID_KERNEL)
			map = kernel_map;
		else {
			vm = p->p_vmspace;
			map = &vm->vm_map;
		}

		if ((opcode & 0xfc003fc0) == 0x04001340) {
			/* lpa failure case */
			frame_regmap(frame, opcode & 0x1f) = 0;
			frame->tf_ipsw |= PSL_N;
		} else if ((opcode & 0xfc001f80) == 0x04001180) {
			int pl;

			/* dig probe[rw]i? insns */
			if (opcode & 0x2000)
				pl = (opcode >> 16) & 3;
			else
				pl = frame_regmap(frame,
				    (opcode >> 16) & 0x1f) & 3;

			if ((type & T_USER && space == HPPA_SID_KERNEL) ||
			    (frame->tf_iioq_head & 3) != pl ||
			    (type & T_USER && va >= VM_MAXUSER_ADDRESS) ||
			    uvm_fault(map, trunc_page(va), fault,
			     opcode & 0x40? UVM_PROT_WRITE : UVM_PROT_READ)) {
				frame_regmap(frame, opcode & 0x1f) = 0;
				frame->tf_ipsw |= PSL_N;
			}
		} else if (type & T_USER) {
			sv.sival_int = va;
			trapsignal(p, SIGILL, type & ~T_USER, ILL_ILLTRP, sv);
		} else
			panic("trap: %s @ 0x%x:0x%x for 0x%x:0x%x irr 0x%08x",
			    tts, frame->tf_iisq_head, frame->tf_iioq_head,
			    space, va, opcode);
		break;

	case T_TLB_DIRTY:
	case T_TLB_DIRTY | T_USER:
	case T_DATACC:
	case T_DATACC | T_USER:
		fault = VM_FAULT_PROTECT;
	case T_ITLBMISS:
	case T_ITLBMISS | T_USER:
	case T_DTLBMISS:
	case T_DTLBMISS | T_USER:
		/*
		 * it could be a kernel map for exec_map faults
		 */
		if (space == HPPA_SID_KERNEL)
			map = kernel_map;
		else {
			vm = p->p_vmspace;
			map = &vm->vm_map;
		}

		/*
		 * user faults out of user addr space are always a fail,
		 * this happens on va >= VM_MAXUSER_ADDRESS, where
		 * space id will be zero and therefore cause
		 * a misbehave lower in the code.
		 *
		 * also check that faulted space id matches the curproc.
		 */
		if ((type & T_USER && va >= VM_MAXUSER_ADDRESS) ||
		   (type & T_USER && map->pmap->pm_space != space)) {
			sv.sival_int = va;
			trapsignal(p, SIGSEGV, vftype, SEGV_MAPERR, sv);
			break;
		}

		ret = uvm_fault(map, trunc_page(va), fault, vftype);

		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if uvm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if (space != HPPA_SID_KERNEL &&
		    va < (vaddr_t)vm->vm_minsaddr) {
			if (ret == 0)
				uvm_grow(p, va);
			else if (ret == EACCES)
				ret = EFAULT;
		}

		if (ret != 0) {
			if (type & T_USER) {
				sv.sival_int = va;
				trapsignal(p, SIGSEGV, vftype,
				    ret == EACCES? SEGV_ACCERR : SEGV_MAPERR,
				    sv);
			} else {
				if (p && p->p_addr->u_pcb.pcb_onfault) {
					frame->tf_iioq_tail = 4 +
					    (frame->tf_iioq_head =
						p->p_addr->u_pcb.pcb_onfault);
#ifdef DDB
					frame->tf_iir = 0;
#endif
				} else {
					panic("trap: "
					    "uvm_fault(%p, %lx, %d, %d): %d",
					    map, va, fault, vftype, ret);
				}
			}
		}
		break;

	case T_DATALIGN | T_USER:
		sv.sival_int = va;
		trapsignal(p, SIGBUS, vftype, BUS_ADRALN, sv);
		break;

	case T_INTERRUPT:
	case T_INTERRUPT | T_USER:
		cpu_intr(frame);
		break;

	case T_CONDITION:
		panic("trap: divide by zero in the kernel");
		break;

	case T_ILLEGAL:
	case T_ILLEGAL | T_USER:
		/* see if it's a SPOP1,,0 */
		if ((opcode & 0xfffffe00) == 0x10000200) {
			frame_regmap(frame, opcode & 0x1f) = 0;
			frame->tf_ipsw |= PSL_N;
			break;
		}
		if (type & T_USER) {
			sv.sival_int = va;
			trapsignal(p, SIGILL, type &~ T_USER, ILL_ILLOPC, sv);
			break;
		}
		/* FALLTHROUGH */

	case T_LOWERPL:
	case T_DPROT:
	case T_IPROT:
	case T_OVERFLOW:
	case T_HIGHERPL:
	case T_TAKENBR:
	case T_POWERFAIL:
	case T_LPMC:
	case T_PAGEREF:
	case T_DATAPID:
	case T_DATAPID | T_USER:
		if (0 /* T-chip */) {
			break;
		}
		/* FALLTHROUGH to unimplemented */
	default:
#if 0
if (kdb_trap (type, va, frame))
	return;
#endif
		panic("trap: unimplemented \'%s\' (%d)", tts, trapnum);
	}

#ifdef DIAGNOSTIC
	if (cpl != oldcpl)
		printf("WARNING: SPL (%d) NOT LOWERED ON "
		    "TRAP (%d) EXIT\n", cpl, trapnum);
#endif

	if (trapnum != T_INTERRUPT)
		splx(cpl);	/* process softints */

	/*
	 * in case we were interrupted from the syscall gate page
	 * treat this as we were not really running user code no more
	 * for weird things start to happen on return to the userland
	 * and also see a note in locore.S:TLABEL(all)
	 */
	if ((type & T_USER) && !(frame->tf_iisq_head == HPPA_SID_KERNEL &&
	    (frame->tf_iioq_head & ~PAGE_MASK) == SYSCALLGATE))
		userret(p);
}

void
child_return(arg)
	void *arg;
{
	struct proc *p = (struct proc *)arg;
	struct trapframe *tf = p->p_md.md_regs;

	/*
	 * Set up return value registers as libc:fork() expects
	 */
	tf->tf_ret0 = 0;
	tf->tf_ret1 = 1;	/* ischild */
	tf->tf_t1 = 0;		/* errno */

	userret(p);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p,
		    (p->p_flag & P_PPWAIT) ? SYS_vfork : SYS_fork, 0, 0);
#endif
}

#ifdef PTRACE

#include <sys/ptrace.h>

int
ss_get_value(struct proc *p, vaddr_t addr, u_int *value)
{
	struct uio uio;
	struct iovec iov;

	iov.iov_base = (caddr_t)value;
	iov.iov_len = sizeof(u_int);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(u_int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = curproc;
	return (process_domem(curproc, p, &uio, PT_READ_I));
}

int
ss_put_value(struct proc *p, vaddr_t addr, u_int value)
{
	struct uio uio;
	struct iovec iov;

	iov.iov_base = (caddr_t)&value;
	iov.iov_len = sizeof(u_int);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = sizeof(u_int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = curproc;
	return (process_domem(curproc, p, &uio, PT_WRITE_I));
}

void
ss_clear_breakpoints(struct proc *p)
{
	/* Restore origional instructions. */
	if (p->p_md.md_bpva != 0) {
		ss_put_value(p, p->p_md.md_bpva, p->p_md.md_bpsave[0]);
		ss_put_value(p, p->p_md.md_bpva + 4, p->p_md.md_bpsave[1]);
		p->p_md.md_bpva = 0;
	}
}

int
process_sstep(struct proc *p, int sstep)
{
	int error;

	ss_clear_breakpoints(p);

	/* Don't touch the syscall gateway page. */
	if (sstep == 0 ||
	    (p->p_md.md_regs->tf_iioq_tail & ~PAGE_MASK) == SYSCALLGATE) {
		p->p_md.md_regs->tf_ipsw &= ~PSL_T;
		return (0);
	}

	p->p_md.md_bpva = p->p_md.md_regs->tf_iioq_tail & ~HPPA_PC_PRIV_MASK;

	/*
	 * Insert two breakpoint instructions; the first one might be
	 * nullified.  Of course we need to save two instruction
	 * first.
	 */

	error = ss_get_value(p, p->p_md.md_bpva, &p->p_md.md_bpsave[0]);
	if (error)
		return (error);
	error = ss_get_value(p, p->p_md.md_bpva + 4, &p->p_md.md_bpsave[1]);
	if (error)
		return (error);

	error = ss_put_value(p, p->p_md.md_bpva, SSBREAKPOINT);
	if (error)
		return (error);
	error = ss_put_value(p, p->p_md.md_bpva + 4, SSBREAKPOINT);
	if (error)
		return (error);

	p->p_md.md_regs->tf_ipsw |= PSL_T;
	return (0);
}

#endif	/* PTRACE */

/*
 * call actual syscall routine
 */
void
syscall(struct trapframe *frame)
{
	register struct proc *p = curproc;
	register const struct sysent *callp;
	int retq, nsys, code, argsize, argoff, oerror, error;
	register_t args[8], rval[2];
#ifdef DIAGNOSTIC
	int oldcpl = cpl;
#endif

	uvmexp.syscalls++;

	if (!USERMODE(frame->tf_iioq_head))
		panic("syscall");

	p->p_md.md_regs = frame;
	nsys = p->p_emul->e_nsysent;
	callp = p->p_emul->e_sysent;

	argoff = 4; retq = 0;
	switch (code = frame->tf_t1) {
	case SYS_syscall:
		code = frame->tf_arg0;
		args[0] = frame->tf_arg1;
		args[1] = frame->tf_arg2;
		args[2] = frame->tf_arg3;
		argoff = 3;
		break;
	case SYS___syscall:
		if (callp != sysent)
			break;
		/*
		 * this works, because quads get magically swapped
		 * due to the args being laid backwards on the stack
		 * and then copied in words
		 */
		code = frame->tf_arg0;
		args[0] = frame->tf_arg2;
		args[1] = frame->tf_arg3;
		argoff = 2;
		retq = 1;
		break;
	default:
		args[0] = frame->tf_arg0;
		args[1] = frame->tf_arg1;
		args[2] = frame->tf_arg2;
		args[3] = frame->tf_arg3;
		break;
	}

	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;	/* bad syscall # */
	else
		callp += code;

	oerror = error = 0;
	if ((argsize = callp->sy_argsize)) {
		int i;

		for (i = 0, argsize -= argoff * 4;
		    argsize > 0; i++, argsize -= 4) {
			error = copyin((void *)(frame->tf_sp +
			    HPPA_FRAME_ARG(i + 4)), args + i + argoff, 4);

			if (error)
				break;
		}

		/*
		 * coming from syscall() or __syscall we must be
		 * having one of those w/ a 64 bit arguments,
		 * which needs a word swap due to the order
		 * of the arguments on the stack.
		 * this assumes that none of 'em are called
		 * by their normal syscall number, maybe a regress
		 * test should be used, to watch the behaviour.
		 */
		if (!error && argoff < 4) {
			int t;

			i = 0;
			switch (code) {
			case SYS_lseek:		retq = 0;
			case SYS_truncate:
			case SYS_ftruncate:	i = 2;	break;
			case SYS_preadv:
			case SYS_pwritev:
			case SYS_pread:
			case SYS_pwrite:	i = 4;	break;
			case SYS_mquery:
			case SYS_mmap:		i = 6;	break;
			}

			if (i) {
				t = args[i];
				args[i] = args[i + 1];
				args[i + 1] = t;
			}
		}
	}

#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p, code, callp->sy_argsize, args);
#endif
	if (error)
		goto bad;

	rval[0] = 0;
	rval[1] = frame->tf_ret1;
#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE))
		oerror = error = systrace_redirect(code, p, args, rval);
	else
#endif
		oerror = error = (*callp->sy_call)(p, args, rval);
	switch (error) {
	case 0:
		frame->tf_ret0 = rval[0];
		frame->tf_ret1 = rval[!retq];
		frame->tf_t1 = 0;
		break;
	case ERESTART:
		frame->tf_iioq_head -= 12;
		frame->tf_iioq_tail -= 12;
	case EJUSTRETURN:
		break;
	default:
	bad:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		frame->tf_t1 = error;
		frame->tf_ret0 = error;
		frame->tf_ret1 = 0;
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
#ifdef DIAGNOSTIC
	if (cpl != oldcpl) {
		printf("WARNING: SPL (0x%x) NOT LOWERED ON "
		    "syscall(0x%x, 0x%x, 0x%x, 0x%x...) EXIT, PID %d\n",
		    cpl, code, args[0], args[1], args[2], p->p_pid);
		cpl = oldcpl;
	}
#endif
	splx(cpl);	/* process softints */
}
