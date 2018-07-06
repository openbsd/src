/*	$OpenBSD: trap.c,v 1.73 2018/07/06 02:43:01 guenther Exp $	*/
/*	$NetBSD: trap.c,v 1.2 2003/05/04 23:51:56 fvdl Exp $	*/

/*-
 * Copyright (c) 1998, 2000 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

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

/*
 * amd64 Trap and System call handling
 */
#undef	TRAP_SIGDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/signal.h>
#include <sys/syscall.h>
#include <sys/syscall_mi.h>

#include <uvm/uvm_extern.h>

#include <machine/cpu.h>
#include <machine/cpufunc.h>
#include <machine/fpu.h>
#include <machine/psl.h>
#include <machine/trap.h>
#ifdef DDB
#include <machine/db_machdep.h>
#endif

#include "isa.h"

int	pageflttrap(struct trapframe *, int _usermode);
void	kerntrap(struct trapframe *);
void	usertrap(struct trapframe *);
void	ast(struct trapframe *);
void	syscall(struct trapframe *);

const char * const trap_type[] = {
	"privileged instruction fault",		/*  0 T_PRIVINFLT */
	"breakpoint trap",			/*  1 T_BPTFLT */
	"arithmetic trap",			/*  2 T_ARITHTRAP */
	"reserved trap",			/*  3 T_RESERVED */
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
	"machine check",			/* 18 T_MCA */
	"SSE FP exception",			/* 19 T_XMM */
};
const int	trap_types = nitems(trap_type);

#ifdef DEBUG
int	trapdebug = 0;
#endif

static void trap_print(struct trapframe *, int _type);
static inline void frame_dump(struct trapframe *_tf, struct proc *_p,
    const char *_sig, uint64_t _cr2);
static inline void verify_smap(const char *_func);
static inline void debug_trap(struct trapframe *_frame, struct proc *_p,
    long _type);
static inline void check_stack(struct proc *_p, long _type);

/*
 * pageflttrap(frame, usermode): page fault handler
 * Returns non-zero if the fault was handled (possibly by generating
 * a signal).  Returns zero, possibly still holding the kernel lock,
 * if something was so broken that we should panic.
 */
int
pageflttrap(struct trapframe *frame, int usermode)
{
	struct proc *p = curproc;
	struct pcb *pcb;
	int error;
	uint64_t cr2;
	vaddr_t va;
	struct vm_map *map;
	vm_prot_t ftype;

	if (p == NULL || p->p_addr == NULL || p->p_vmspace == NULL)
		return 0;

	map = &p->p_vmspace->vm_map;
	pcb = &p->p_addr->u_pcb;
	cr2 = rcr2();
	va = trunc_page((vaddr_t)cr2);

	KERNEL_LOCK();

	if (!usermode) {
		extern struct vm_map *kernel_map;

		/* This will only trigger if SMEP is enabled */
		if (cr2 <= VM_MAXUSER_ADDRESS && frame->tf_err & PGEX_I)
			panic("attempt to execute user address %p "
			    "in supervisor mode", (void *)cr2);
		/* This will only trigger if SMAP is enabled */
		if (pcb->pcb_onfault == NULL && cr2 <= VM_MAXUSER_ADDRESS &&
		    frame->tf_err & PGEX_P)
			panic("attempt to access user address %p "
			    "in supervisor mode", (void *)cr2);

		/*
		 * It is only a kernel address space fault iff:
		 *	1. when running in ring0  and
		 *	2. pcb_onfault not set or
		 *	3. pcb_onfault set but supervisor space fault
		 * The last can occur during an exec() copyin where the
		 * argument space is lazy-allocated.
		 */
		if (va >= VM_MIN_KERNEL_ADDRESS)
			map = kernel_map;
	}

	if (frame->tf_err & PGEX_W)
		ftype = PROT_WRITE;
	else if (frame->tf_err & PGEX_I)
		ftype = PROT_EXEC;
	else
		ftype = PROT_READ;

	if (curcpu()->ci_inatomic == 0 || map == kernel_map) {
		/* Fault the original page in. */
		caddr_t onfault = pcb->pcb_onfault;

		pcb->pcb_onfault = NULL;
		error = uvm_fault(map, va, frame->tf_err & PGEX_P ?
		    VM_FAULT_PROTECT : VM_FAULT_INVALID, ftype);
		pcb->pcb_onfault = onfault;
	} else
		error = EFAULT;

	if (error == 0) {
		if (map != kernel_map)
			uvm_grow(p, va);
	} else if (!usermode) {
		if (pcb->pcb_onfault != 0) {
			KERNEL_UNLOCK();
			frame->tf_rip = (u_int64_t)pcb->pcb_onfault;
			return 1;
		} else {
			/*
			 * Bad memory access in the kernel; save the fault
			 * info for DDB and retain the kernel lock to keep
			 * faultbuf from being overwritten by another CPU.
			 */
			static char faultbuf[512];
			snprintf(faultbuf, sizeof faultbuf,
			    "uvm_fault(%p, 0x%llx, 0, %d) -> %x",
			    map, cr2, ftype, error);
			printf("%s\n", faultbuf);
			faultstr = faultbuf;
			return 0;
		}
	} else {
		union sigval sv;
		int signal, sicode;

		signal = SIGSEGV;
		sicode = SEGV_MAPERR;
		if (error == ENOMEM) {
			printf("UVM: pid %d (%s), uid %d killed:"
			    " out of swap\n", p->p_p->ps_pid, p->p_p->ps_comm,
			    p->p_ucred ? (int)p->p_ucred->cr_uid : -1);
			signal = SIGKILL;
		} else {
			frame_dump(frame, p, "SEGV", cr2);
			if (error == EACCES)
				sicode = SEGV_ACCERR;
			else if (error == EIO) {
				signal = SIGBUS;
				sicode = BUS_OBJERR;
			}
		}
		sv.sival_ptr = (void *)cr2;
		trapsignal(p, signal, T_PAGEFLT, sicode, sv);
	}

	KERNEL_UNLOCK();

	return 1;
}


/*
 * kerntrap(frame):
 *	Exception, fault, and trap interface to BSD kernel. This
 * common code is called from assembly language IDT gate entry
 * routines that prepare a suitable stack frame, and restore this
 * frame after the exception has been processed.
 */
void
kerntrap(struct trapframe *frame)
{
	int type = (int)frame->tf_trapno;

	verify_smap(__func__);
	uvmexp.traps++;
	debug_trap(frame, curproc, type);

	switch (type) {

	default:
	we_re_toast:
#ifdef DDB
		if (db_ktrap(type, 0, frame))
			return;
#endif
		trap_print(frame, type);
		panic("trap type %d, code=%llx, pc=%llx",
		    type, frame->tf_err, frame->tf_rip);
		/*NOTREACHED*/

	case T_PROTFLT:
	case T_SEGNPFLT:
	case T_ALIGNFLT:
	case T_TSSFLT:
		goto we_re_toast;

	case T_PAGEFLT:			/* allow page faults in kernel mode */
		if (pageflttrap(frame, 0))
			return;
		goto we_re_toast;

	case T_TRCTRAP:
		goto we_re_toast;

#if NISA > 0
	case T_NMI:
#ifdef DDB
		/* NMI can be hooked up to a pushbutton for debugging */
		printf ("NMI ... going to debugger\n");
		if (db_ktrap(type, 0, frame))
			return;
#endif
		/* machine/parity/power fail/"kitchen sink" faults */

		if (x86_nmi() != 0)
			goto we_re_toast;
		else
			return;
#endif /* NISA > 0 */
	}
}


/*
 * usertrap(frame): handler for exceptions, faults, and traps from userspace
 *	This is called from the assembly language IDT gate entries
 * which prepare a suitable stack frame and restores the CPU state
 * after the fault has been processed.
 */
void
usertrap(struct trapframe *frame)
{
	struct proc *p = curproc;
	int type = (int)frame->tf_trapno;
	union sigval sv;
	int sig, code;

	verify_smap(__func__);
	uvmexp.traps++;
	debug_trap(frame, p, type);

	p->p_md.md_regs = frame;
	refreshcreds(p);
	check_stack(p, type);

	switch (type) {
	case T_PROTFLT:			/* protection fault */
	case T_TSSFLT:
	case T_SEGNPFLT:
	case T_STKFLT:
		frame_dump(frame, p, "BUS", 0);
		sig = SIGBUS;
		code = BUS_OBJERR;
		break;
	case T_ALIGNFLT:
		sig = SIGBUS;
		code = BUS_ADRALN;
		break;
	case T_PRIVINFLT:		/* privileged instruction fault */
		sig = SIGILL;
		code = ILL_PRVOPC;
		break;
	case T_DIVIDE:
		sig = SIGFPE;
		code = FPE_INTDIV;
		break;
	case T_ARITHTRAP:
	case T_XMM:			/* real arithmetic exceptions */
		sig = SIGFPE;
		code = fputrap(type);
		break;
	case T_BPTFLT:			/* bpt instruction fault */
	case T_TRCTRAP:			/* trace trap */
		sig = SIGTRAP;
		code = TRAP_BRKPT;
		break;

	case T_PAGEFLT:			/* page fault */
		if (pageflttrap(frame, 1))
			goto out;
		/* FALLTHROUGH */

	default:
		trap_print(frame, type);
		panic("impossible trap");
	}

	sv.sival_ptr = (void *)frame->tf_rip;
	KERNEL_LOCK();
	trapsignal(p, sig, type, code, sv);
	KERNEL_UNLOCK();

out:
	userret(p);
}


static void
trap_print(struct trapframe *frame, int type)
{
	if (type < trap_types)
		printf("fatal %s", trap_type[type]);
	else
		printf("unknown trap %d", type);
	printf(" in %s mode\n", KERNELMODE(frame->tf_cs, frame->tf_rflags) ?
	    "supervisor" : "user");
	printf("trap type %d code %llx rip %llx cs %llx rflags %llx cr2 "
	       " %llx cpl %x rsp %llx\n",
	    type, frame->tf_err, frame->tf_rip, frame->tf_cs,
	    frame->tf_rflags, rcr2(), curcpu()->ci_ilevel, frame->tf_rsp);
	printf("gsbase %p  kgsbase %p\n",
	    (void *)rdmsr(MSR_GSBASE), (void *)rdmsr(MSR_KERNELGSBASE));
}


static inline void
frame_dump(struct trapframe *tf, struct proc *p, const char *sig, uint64_t cr2)
{
#ifdef TRAP_SIGDEBUG
	printf("pid %d (%s): %s at rip %llx addr %llx\n",
	    p->p_p->ps_pid, p->p_p->ps_comm, sig, tf->tf_rip, cr2);
	printf("rip %p  cs 0x%x  rfl %p  rsp %p  ss 0x%x\n",
	    (void *)tf->tf_rip, (unsigned)tf->tf_cs & 0xffff,
	    (void *)tf->tf_rflags,
	    (void *)tf->tf_rsp, (unsigned)tf->tf_ss & 0xffff);
	printf("err 0x%llx  trapno 0x%llx\n",
	    tf->tf_err, tf->tf_trapno);
	printf("rdi %p  rsi %p  rdx %p\n",
	    (void *)tf->tf_rdi, (void *)tf->tf_rsi, (void *)tf->tf_rdx);
	printf("rcx %p  r8  %p  r9  %p\n",
	    (void *)tf->tf_rcx, (void *)tf->tf_r8, (void *)tf->tf_r9);
	printf("r10 %p  r11 %p  r12 %p\n",
	    (void *)tf->tf_r10, (void *)tf->tf_r11, (void *)tf->tf_r12);
	printf("r13 %p  r14 %p  r15 %p\n",
	    (void *)tf->tf_r13, (void *)tf->tf_r14, (void *)tf->tf_r15);
	printf("rbp %p  rbx %p  rax %p\n",
	    (void *)tf->tf_rbp, (void *)tf->tf_rbx, (void *)tf->tf_rax);
#endif
}

static inline void
verify_smap(const char *func)
{
#ifdef DIAGNOSTIC
	if (curcpu()->ci_feature_sefflags_ebx & SEFF0EBX_SMAP) {
		u_long rf = read_rflags();
		if (rf & PSL_AC) {
			write_rflags(rf & ~PSL_AC);
			panic("%s: AC set on entry", func);
		}
	}
#endif
}

static inline void
debug_trap(struct trapframe *frame, struct proc *p, long type)
{
#ifdef DEBUG
	if (trapdebug) {
		printf("trap %ld code %llx rip %llx cs %llx rflags %llx "
		       "cr2 %llx cpl %x\n",
		    type, frame->tf_err, frame->tf_rip, frame->tf_cs,
		    frame->tf_rflags, rcr2(), curcpu()->ci_ilevel);
		printf("curproc %p\n", (void *)p);
		if (p != NULL)
			printf("pid %d\n", p->p_p->ps_pid);
	}
#endif
}

static inline void
check_stack(struct proc *p, long type)
{
	vaddr_t sp = PROC_STACK(p);

	if (p->p_vmspace->vm_map.serial != p->p_spserial ||
	    p->p_spstart == 0 || sp < p->p_spstart || sp >= p->p_spend) {
		KERNEL_LOCK();
		if (!uvm_map_check_stack_range(p, sp)) {
			union sigval sv;

			printf("trap [%s]%d/%d type %ld: sp %lx not inside"
			    " %lx-%lx\n", p->p_p->ps_comm, p->p_p->ps_pid,
			    p->p_tid, type, sp, p->p_spstart, p->p_spend);
			sv.sival_ptr = (void *)PROC_PC(p);
			trapsignal(p, SIGSEGV, type, SEGV_ACCERR, sv);
		}
		KERNEL_UNLOCK();
	}
}


/*
 * ast(frame):
 *	AST handler.  This is called from assembly language stubs when
 *	returning to userspace after a syscall or interrupt.
 */
void
ast(struct trapframe *frame)
{
	struct proc *p = curproc;

	uvmexp.traps++;
	KASSERT(!KERNELMODE(frame->tf_cs, frame->tf_rflags));
	p->p_md.md_regs = frame;
	refreshcreds(p);
	uvmexp.softs++;
	mi_ast(p, curcpu()->ci_want_resched);
	userret(p);
}


/*
 * syscall(frame):
 *	System call request from POSIX system call gate interface to kernel.
 */
void
syscall(struct trapframe *frame)
{
	caddr_t params;
	const struct sysent *callp;
	struct proc *p;
	int error;
	int nsys;
	size_t argsize, argoff;
	register_t code, args[9], rval[2], *argp;

	verify_smap(__func__);
	uvmexp.syscalls++;
	p = curproc;

	code = frame->tf_rax;
	callp = p->p_p->ps_emul->e_sysent;
	nsys = p->p_p->ps_emul->e_nsysent;
	argp = &args[0];
	argoff = 0;

	switch (code) {
	case SYS_syscall:
	case SYS___syscall:
		/*
		 * Code is first argument, followed by actual args.
		 */
		code = frame->tf_rdi;
		argp = &args[1];
		argoff = 1;
		break;
	default:
		break;
	}

	if (code < 0 || code >= nsys)
		callp += p->p_p->ps_emul->e_nosys;
	else
		callp += code;

	argsize = (callp->sy_argsize >> 3) + argoff;
	if (argsize) {
		switch (MIN(argsize, 6)) {
		case 6:
			args[5] = frame->tf_r9;
		case 5:
			args[4] = frame->tf_r8;
		case 4:
			args[3] = frame->tf_r10;
		case 3:
			args[2] = frame->tf_rdx;
		case 2:	
			args[1] = frame->tf_rsi;
		case 1:
			args[0] = frame->tf_rdi;
			break;
		default:
			panic("impossible syscall argsize");
		}
		if (argsize > 6) {
			argsize -= 6;
			params = (caddr_t)frame->tf_rsp + sizeof(register_t);
			if ((error = copyin(params, &args[6], argsize << 3)))
				goto bad;
		}
	}

	rval[0] = 0;
	rval[1] = frame->tf_rdx;

	error = mi_syscall(p, code, callp, argp, rval);

	switch (error) {
	case 0:
		frame->tf_rax = rval[0];
		frame->tf_rdx = rval[1];
		frame->tf_rflags &= ~PSL_C;	/* carry bit */
		break;
	case ERESTART:
		/* Back up over the syscall instruction (2 bytes) */
		frame->tf_rip -= 2;
		break;
	case EJUSTRETURN:
		/* nothing to do */
		break;
	default:
	bad:
		frame->tf_rax = error;
		frame->tf_rflags |= PSL_C;	/* carry bit */
		break;
	}

	mi_syscall_return(p, code, error, rval);
}

void
child_return(void *arg)
{
	struct proc *p = arg;
	struct trapframe *tf = p->p_md.md_regs;

	tf->tf_rax = 0;
	tf->tf_rdx = 1;
	tf->tf_rflags &= ~PSL_C;

	KERNEL_UNLOCK();

	mi_child_return(p);
}

