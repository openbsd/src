/*	$OpenBSD: trap.c,v 1.60 2003/12/23 00:40:02 miod Exp $	*/
/*
 * Copyright (c) 1998 Steve Murphree, Jr.
 * Copyright (c) 1996 Nivas Madhur
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
 *      This product includes software developed by Nivas Madhur.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */
/*
 * Mach Operating System
 * Copyright (c) 1991 Carnegie Mellon University
 * Copyright (c) 1991 OMRON Corporation
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/ktrace.h>

#include "systrace.h"
#include <dev/systrace.h>

#include <uvm/uvm_extern.h>

#include <machine/asm_macro.h>   /* enable/disable interrupts */
#include <machine/cpu.h>
#include <machine/locore.h>
#ifdef M88100
#include <machine/m88100.h>		/* DMT_xxx */
#include <machine/m8820x.h>		/* CMMU_PFSR_xxx */
#endif
#ifdef M88110
#include <machine/m88110.h>
#endif
#include <machine/pcb.h>		/* FIP_E, etc. */
#include <machine/psl.h>		/* FIP_E, etc. */
#include <machine/trap.h>

#include <machine/db_machdep.h>
#ifdef DDB
#include <ddb/db_output.h>		/* db_printf()		*/
#endif /* DDB */
#define SSBREAKPOINT (0xF000D1F8U) /* Single Step Breakpoint */

#define TRAPTRACE

#if defined(TRAPTRACE)
unsigned traptrace = 0;
#endif

#ifdef DDB
#define DEBUG_MSG(x) db_printf x
#else
#define DEBUG_MSG(x)
#endif /* DDB */

#define USERMODE(PSR)   (((PSR) & PSR_MODE) == 0)
#define SYSTEMMODE(PSR) (((PSR) & PSR_MODE) != 0)

/* sigh */
extern int procfs_domem(struct proc *, struct proc *, void *, struct uio *);

extern void regdump(struct trapframe *f);
__dead void error_fatal(struct m88100_saved_state *frame);

char  *trap_type[] = {
	"Reset",
	"Interrupt Exception",
	"Instruction Access",
	"Data Access Exception",
	"Misaligned Access",
	"Unimplemented Opcode",
	"Privilege Violation"
	"Bounds Check Violation",
	"Illegal Integer Divide",
	"Integer Overflow",
	"Error Exception",
	"Non-Maskable Exception",
};

char  *pbus_exception_type[] = {
	"Success (No Fault)",
	"unknown 1",
	"unknown 2",
	"Bus Error",
	"Segment Fault",
	"Page Fault",
	"Supervisor Violation",
	"Write Violation",
};

int   trap_types = sizeof trap_type / sizeof trap_type[0];

static inline void
userret(struct proc *p, struct m88100_saved_state *frame, u_quad_t oticks)
{
	int sig;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;

	if (want_resched) {
		/*
		 * We're being preempted.
		 */
		preempt(NULL);
		while ((sig = CURSIG(p)) != 0)
			postsig(sig);
	}

	/*
	 * If profiling, charge recent system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, frame->sxip & XIP_ADDR,
		    (int)(p->p_sticks - oticks) * psratio);
	}
	curpriority = p->p_priority;
}

__dead void
panictrap(int type, struct m88100_saved_state *frame)
{
#ifdef DDB
	static int panicing = 0;

	if (panicing++ == 0) {
		if (type == 2 && cputyp == CPU_88100) {
			/* instruction exception */
			db_printf("\nInstr access fault (%s) v = %x, frame %p\n",
				  pbus_exception_type[(frame->ipfsr >> 16) & 0x7],
				  frame->sxip & XIP_ADDR, frame);
		} else if (type == 3 && cputyp == CPU_88100) {
			/* data access exception */
			db_printf("\nData access fault (%s) v = %x, frame %p\n",
				  pbus_exception_type[(frame->dpfsr >> 16) & 0x7],
				  frame->sxip & XIP_ADDR, frame);
		} else
			db_printf("\ntrap type %d, v = %x, frame %p\n", type, frame->sxip & XIP_ADDR, frame);
		regdump(frame);
	}
#endif
	if ((u_int)type < trap_types)
		panic(trap_type[type]);
	else
		panic("trap %d", type);
	/*NOTREACHED*/
}

#ifdef M88100
unsigned last_trap[4] = {0,0,0,0};
unsigned last_vector = 0;

void
m88100_trap(unsigned type, struct m88100_saved_state *frame)
{
	struct proc *p;
	u_quad_t sticks = 0;
	struct vm_map *map;
	vaddr_t va;
	vm_prot_t ftype;
	int fault_type, pbus_type;
	u_long fault_code;
	unsigned nss, fault_addr;
	struct vmspace *vm;
	union sigval sv;
	int result;
#ifdef DDB
	int s;
#endif
	int sig = 0;
	unsigned pc = PC_REGS(frame);  /* get program counter (sxip) */

	extern struct vm_map *kernel_map;
	extern caddr_t guarded_access_start;
	extern caddr_t guarded_access_end;
	extern caddr_t guarded_access_bad;

	if (type != last_trap[3]) {
		last_trap[0] = last_trap[1];
		last_trap[1] = last_trap[2];
		last_trap[2] = last_trap[3];
		last_trap[3] = type;
	}
	uvmexp.traps++;
	if ((p = curproc) == NULL)
		p = &proc0;

	if (USERMODE(frame->epsr)) {
		sticks = p->p_sticks;
		type += T_USER;
		p->p_md.md_tf = frame;	/* for ptrace/signals */
	}
	fault_type = 0;
	fault_code = 0;
	fault_addr = frame->sxip & XIP_ADDR;

	switch (type) {
	default:
		panictrap(frame->vector, frame);
		break;
		/*NOTREACHED*/

#if defined(DDB)
	case T_KDB_BREAK:
		s = splhigh();
		db_enable_interrupt();
		ddb_break_trap(T_KDB_BREAK, (db_regs_t*)frame);
		db_disable_interrupt();
		splx(s);
		return;
	case T_KDB_ENTRY:
		s = splhigh();
		db_enable_interrupt();
		ddb_entry_trap(T_KDB_ENTRY, (db_regs_t*)frame);
		db_disable_interrupt();
		splx(s);
		return;
#endif /* DDB */
	case T_ILLFLT:
		DEBUG_MSG(("Unimplemented opcode!\n"));
		panictrap(frame->vector, frame);
		break;
	case T_INT:
	case T_INT+T_USER:
		/* This function pointer is set in machdep.c
		   It calls m188_ext_int or sbc_ext_int depending
		   on the value of brdtyp - smurph */
		(*md.interrupt_func)(T_INT, frame);
		return;

	case T_MISALGNFLT:
		DEBUG_MSG(("kernel misaligned "
			  "access exception @ 0x%08x\n", frame->sxip));
		panictrap(frame->vector, frame);
		break;

	case T_INSTFLT:
		/* kernel mode instruction access fault.
		 * Should never, never happen for a non-paged kernel.
		 */
		DEBUG_MSG(("kernel mode instruction "
			  "page fault @ 0x%08x\n", frame->sxip));
		panictrap(frame->vector, frame);
		break;

	case T_DATAFLT:
		/* kernel mode data fault */

		/* data fault on the user address? */
		if ((frame->dmt0 & DMT_DAS) == 0) {
			type = T_DATAFLT + T_USER;
			goto user_fault;
		}

		fault_addr = frame->dma0;
		if (frame->dmt0 & (DMT_WRITE|DMT_LOCKBAR)) {
			ftype = VM_PROT_READ|VM_PROT_WRITE;
			fault_code = VM_PROT_WRITE;
		} else {
			ftype = VM_PROT_READ;
			fault_code = VM_PROT_READ;
		}

		va = trunc_page((vaddr_t)fault_addr);
		if (va == 0) {
			panic("trap: bad kernel access at %x", fault_addr);
		}

		vm = p->p_vmspace;
		map = kernel_map;

		pbus_type = (frame->dpfsr >> 16) & 0x07;
#ifdef DEBUG
		printf("Kernel Data access fault #%d (%s) v = 0x%x, frame 0x%x cpu %d\n",
		       pbus_type, pbus_exception_type[pbus_type],
		       fault_addr, frame, frame->cpu);
#endif

		switch (pbus_type) {
		case CMMU_PFSR_BERROR:
			/*
		 	 * If it is a guarded access, bus error is OK.
		 	 */
			if ((frame->sxip & XIP_ADDR) >=
			      (unsigned)&guarded_access_start &&
			    (frame->sxip & XIP_ADDR) <=
			      (unsigned)&guarded_access_end) {
				frame->snip =
				  ((unsigned)&guarded_access_bad    ) | NIP_V;
				frame->sfip =
				  ((unsigned)&guarded_access_bad + 4) | FIP_V;
				frame->sxip = 0;
				/* We sort of resolved the fault ourselves
				 * because we know where it came from
				 * [guarded_access()]. But we must still think
				 * about the other possible transactions in
				 * dmt1 & dmt2.  Mark dmt0 so that
				 * data_access_emulation skips it.  XXX smurph
				 */
				frame->dmt0 |= DMT_SKIP;
				data_access_emulation((unsigned *)frame);
				frame->dpfsr = 0;
				frame->dmt0 = 0;
				return;
			}
			break;
		case CMMU_PFSR_SUCCESS:
			/*
			 * The fault was resolved. Call data_access_emulation
			 * to drain the data unit pipe line and reset dmt0
			 * so that trap won't get called again.
			 */
			data_access_emulation((unsigned *)frame);
			frame->dpfsr = 0;
			frame->dmt0 = 0;
			return;
		case CMMU_PFSR_SFAULT:
		case CMMU_PFSR_PFAULT:
			result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
			if (result == 0) {
				/*
				 * We could resolve the fault. Call
				 * data_access_emulation to drain the data
				 * unit pipe line and reset dmt0 so that trap
				 * won't get called again.
				 */
				data_access_emulation((unsigned *)frame);
				frame->dpfsr = 0;
				frame->dmt0 = 0;
				return;
			}
			break;
		}
#ifdef DEBUG
		printf ("PBUS Fault %d (%s) va = 0x%x\n", pbus_type,
			pbus_exception_type[pbus_type], va);
#endif
		/*
		 * if still the fault is not resolved ...
		 */
		if (p->p_addr->u_pcb.pcb_onfault == 0)
			panictrap(frame->vector, frame);

		frame->snip =
		    ((unsigned)p->p_addr->u_pcb.pcb_onfault    ) | FIP_V;
		frame->sfip =
		    ((unsigned)p->p_addr->u_pcb.pcb_onfault + 4) | FIP_V;
		frame->sxip = 0;
		/* We sort of resolved the fault ourselves because
		 * we know where it came from [copyxxx()]
		 * But we must still think about the other possible
		 * transactions in dmt1 & dmt2.  Mark dmt0 so that
		 * data_access_emulation skips it. XXX smurph
		 */
		frame->dmt0 |= DMT_SKIP;
		data_access_emulation((unsigned *)frame);
		frame->dpfsr = 0;
		frame->dmt0 = 0;
		return;
	case T_INSTFLT+T_USER:
		/* User mode instruction access fault */
		/* FALLTHROUGH */
	case T_DATAFLT+T_USER:
user_fault:
		if (type == T_INSTFLT + T_USER) {
			pbus_type = (frame->ipfsr >> 16) & 0x07;
		} else {
			fault_addr = frame->dma0;
			pbus_type = (frame->dpfsr >> 16) & 0x07;
		}
#ifdef DEBUG
		printf("User Data access fault #%d (%s) v = 0x%x, frame 0x%x cpu %d\n",
		       pbus_type, pbus_exception_type[pbus_type],
		       fault_addr, frame, frame->cpu);
#endif

		if (frame->dmt0 & (DMT_WRITE | DMT_LOCKBAR)) {
			ftype = VM_PROT_READ | VM_PROT_WRITE;
			fault_code = VM_PROT_WRITE;
		} else {
			ftype = VM_PROT_READ;
			fault_code = VM_PROT_READ;
		}

		va = trunc_page((vaddr_t)fault_addr);

		vm = p->p_vmspace;
		map = &vm->vm_map;

		/* Call uvm_fault() to resolve non-bus error faults */
		switch (pbus_type) {
		case CMMU_PFSR_SUCCESS:
			result = 0;
			break;
		case CMMU_PFSR_BERROR:
			result = EACCES;
			break;
		default:
			result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
			break;
		}

		if ((caddr_t)va >= vm->vm_maxsaddr) {
			if (result == 0) {
				nss = btoc(USRSTACK - va);/* XXX check this */
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (result == EACCES)
				result = EFAULT;
		}

		if (result == 0) {
			if (type == T_DATAFLT+T_USER) {
				/*
			 	 * We could resolve the fault. Call
			 	 * data_access_emulation to drain the data unit
			 	 * pipe line and reset dmt0 so that trap won't
			 	 * get called again.
			 	 */
				data_access_emulation((unsigned *)frame);
				frame->dpfsr = 0;
				frame->dmt0 = 0;
			} else {
				/*
				 * back up SXIP, SNIP,
				 * clearing the Error bit
				 */
				frame->sfip = frame->snip & ~FIP_E;
				frame->snip = frame->sxip & ~NIP_E;
				frame->ipfsr = 0;
			}
		} else {
			sig = result == EACCES ? SIGBUS : SIGSEGV;
			fault_type = result == EACCES ?
				BUS_ADRERR : SEGV_MAPERR;
		}
		break;
	case T_MISALGNFLT+T_USER:
		sig = SIGBUS;
		fault_type = BUS_ADRALN;
		break;
	case T_PRIVINFLT+T_USER:
	case T_ILLFLT+T_USER:
#ifndef DDB
	case T_KDB_BREAK:
	case T_KDB_ENTRY:
#endif
	case T_KDB_BREAK+T_USER:
	case T_KDB_ENTRY+T_USER:
	case T_KDB_TRACE:
	case T_KDB_TRACE+T_USER:
		sig = SIGILL;
		break;
	case T_BNDFLT+T_USER:
		sig = SIGFPE;
		break;
	case T_ZERODIV+T_USER:
		sig = SIGFPE;
		fault_type = FPE_INTDIV;
		break;
	case T_OVFFLT+T_USER:
		sig = SIGFPE;
		fault_type = FPE_INTOVF;
		break;
	case T_FPEPFLT+T_USER:
	case T_FPEIFLT+T_USER:
		sig = SIGFPE;
		break;
	case T_SIGSYS+T_USER:
		sig = SIGSYS;
		break;
	case T_SIGTRAP+T_USER:
		sig = SIGTRAP;
		fault_type = TRAP_TRACE;
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
		{
			unsigned va;
			unsigned instr;
			struct uio uio;
			struct iovec iov;

			/* read break instruction */
			copyin((caddr_t)pc, &instr, sizeof(unsigned));
#if 0
			printf("trap: %s (%d) breakpoint %x at %x: (adr %x ins %x)\n",
			       p->p_comm, p->p_pid, instr, pc,
			       p->p_md.md_ss_addr, p->p_md.md_ss_instr); /* XXX */
#endif
			/* check and see if we got here by accident */
			if ((p->p_md.md_ss_addr != pc &&
			     p->p_md.md_ss_taken_addr != pc) ||
			    instr != SSBREAKPOINT) {
				sig = SIGTRAP;
				fault_type = TRAP_TRACE;
				break;
			}
			/* restore original instruction and clear BP  */
			instr = p->p_md.md_ss_instr;
			va = p->p_md.md_ss_addr;
			if (va != 0) {
				iov.iov_base = (caddr_t)&instr;
				iov.iov_len = sizeof(int);
				uio.uio_iov = &iov;
				uio.uio_iovcnt = 1;
				uio.uio_offset = (off_t)va;
				uio.uio_resid = sizeof(int);
				uio.uio_segflg = UIO_SYSSPACE;
				uio.uio_rw = UIO_WRITE;
				uio.uio_procp = curproc;
				procfs_domem(p, p, NULL, &uio);
			}

			/* branch taken instruction */
			instr = p->p_md.md_ss_taken_instr;
			va = p->p_md.md_ss_taken_addr;
			if (instr != 0) {
				iov.iov_base = (caddr_t)&instr;
				iov.iov_len = sizeof(int);
				uio.uio_iov = &iov;
				uio.uio_iovcnt = 1;
				uio.uio_offset = (off_t)va;
				uio.uio_resid = sizeof(int);
				uio.uio_segflg = UIO_SYSSPACE;
				uio.uio_rw = UIO_WRITE;
				uio.uio_procp = curproc;
				procfs_domem(p, p, NULL, &uio);
			}
#if 1
			frame->sfip = frame->snip;    /* set up next FIP */
			frame->snip = pc;    /* set up next NIP */
			frame->snip |= 2;	  /* set valid bit   */
#endif
			p->p_md.md_ss_addr = 0;
			p->p_md.md_ss_instr = 0;
			p->p_md.md_ss_taken_addr = 0;
			p->p_md.md_ss_taken_instr = 0;
			sig = SIGTRAP;
			fault_type = TRAP_BRKPT;
		}
		break;

	case T_USERBPT+T_USER:
		/*
		 * This trap is meant to be used by debuggers to implement
		 * breakpoint debugging.  When we get this trap, we just
		 * return a signal which gets caught by the debugger.
		 */
		frame->sfip = frame->snip;    /* set up the next FIP */
		frame->snip = frame->sxip;    /* set up the next NIP */
		sig = SIGTRAP;
		fault_type = TRAP_BRKPT;
		break;

	case T_ASTFLT+T_USER:
		uvmexp.softs++;
		want_ast = 0;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		break;
	}

	/*
	 * If trap from supervisor mode, just return
	 */
	if (type < T_USER)
		return;

	if (sig) {
		sv.sival_int = fault_addr;
		trapsignal(p, sig, fault_code, fault_type, sv);
		/*
		 * don't want multiple faults - we are going to
		 * deliver signal.
		 */
		frame->dmt0 = 0;
		frame->ipfsr = frame->dpfsr = 0;
	}

	userret(p, frame, sticks);
}
#endif /* m88100 */

#ifdef M88110
void
m88110_trap(unsigned type, struct m88100_saved_state *frame)
{
	struct proc *p;
	u_quad_t sticks = 0;
	struct vm_map *map;
	vaddr_t va;
	vm_prot_t ftype;
	int fault_type;
	u_long fault_code;
	unsigned nss, fault_addr;
	struct vmspace *vm;
	union sigval sv;
	int result;
#ifdef DDB
        int s; /* IPL */
#endif
	int sig = 0;
	unsigned pc = PC_REGS(frame);  /* get program counter (exip) */
	pt_entry_t *pte;

	extern struct vm_map *kernel_map;
	extern unsigned guarded_access_start;
	extern unsigned guarded_access_end;
	extern unsigned guarded_access_bad;
	extern pt_entry_t *pmap_pte(pmap_t, vaddr_t);

	uvmexp.traps++;
	if ((p = curproc) == NULL)
		p = &proc0;

#ifdef DEBUG
	if (type != T_INT && type != T_ASTFLT
#ifdef DDB
	    && type != T_KDB_ENTRY
#endif
	   ) {
		printf("m88110_trap: %d %s\n", type, frame->vector < trap_types ? trap_type[frame->vector] : "unknown");
	}
#endif

	if (USERMODE(frame->epsr)) {
		sticks = p->p_sticks;
		type += T_USER;
		p->p_md.md_tf = frame;	/* for ptrace/signals */
	}
	fault_type = 0;
	fault_code = 0;
	fault_addr = frame->exip & XIP_ADDR;

	switch (type) {
	default:
		panictrap(frame->vector, frame);
		break;
		/*NOTREACHED*/

	case T_197_READ+T_USER:
	case T_197_READ:
		DEBUG_MSG(("DMMU read miss: Hardware Table Searches should be enabled!\n"));
		panictrap(frame->vector, frame);
		break;
		/*NOTREACHED*/
	case T_197_WRITE+T_USER:
	case T_197_WRITE:
		DEBUG_MSG(("DMMU write miss: Hardware Table Searches should be enabled!\n"));
		panictrap(frame->vector, frame);
		break;
		/*NOTREACHED*/
	case T_197_INST+T_USER:
	case T_197_INST:
		DEBUG_MSG(("IMMU miss: Hardware Table Searches should be enabled!\n"));
		panictrap(frame->vector, frame);
		break;
		/*NOTREACHED*/
#ifdef DDB
	case T_KDB_TRACE:
		s = splhigh();
		db_enable_interrupt();
		ddb_break_trap(T_KDB_TRACE, (db_regs_t*)frame);
		db_disable_interrupt();
		splx(s);
		return;
	case T_KDB_BREAK:
		s = splhigh();
		db_enable_interrupt();
		ddb_break_trap(T_KDB_BREAK, (db_regs_t*)frame);
		db_disable_interrupt();
		splx(s);
		return;
	case T_KDB_ENTRY:
		s = splhigh();
		db_enable_interrupt();
		ddb_entry_trap(T_KDB_ENTRY, (db_regs_t*)frame);
		db_disable_interrupt();
		if (frame->enip) {
			frame->exip = frame->enip;
		} else {
			frame->exip += 4;
		}
		splx(s);
		return;
#if 0
	case T_ILLFLT:
		s = splhigh();
		db_enable_interrupt();
		ddb_error_trap(type == T_ILLFLT ? "unimplemented opcode" :
		       "error fault", (db_regs_t*)frame);
		db_disable_interrupt();
		splx(s);
		return;
#endif /* 0 */
#endif /* DDB */
	case T_ILLFLT:
		DEBUG_MSG(("Unimplemented opcode!\n"));
		panictrap(frame->vector, frame);
		break;
	case T_NON_MASK:
	case T_NON_MASK+T_USER:
		/* This function pointer is set in machdep.c
		   It calls m197_ext_int - smurph */
		(*md.interrupt_func)(T_NON_MASK, frame);
		return;
	case T_INT:
	case T_INT+T_USER:
		(*md.interrupt_func)(T_INT, frame);
		return;
	case T_MISALGNFLT:
		DEBUG_MSG(("kernel mode misaligned "
			  "access exception @ 0x%08x\n", frame->exip));
		panictrap(frame->vector, frame);
		break;
		/*NOTREACHED*/

	case T_INSTFLT:
		/* kernel mode instruction access fault.
		 * Should never, never happen for a non-paged kernel.
		 */
		DEBUG_MSG(("kernel mode instruction "
			  "page fault @ 0x%08x\n", frame->exip));
		panictrap(frame->vector, frame);
		break;
		/*NOTREACHED*/

	case T_DATAFLT:
		/* kernel mode data fault */

		/* data fault on the user address? */
		if ((frame->dsr & CMMU_DSR_SU) == 0) {
			type = T_DATAFLT + T_USER;
			goto m88110_user_fault;
		}

		fault_addr = frame->dlar;
		if (frame->dsr & CMMU_DSR_RW) {
			ftype = VM_PROT_READ;
			fault_code = VM_PROT_READ;
		} else {
			ftype = VM_PROT_READ|VM_PROT_WRITE;
			fault_code = VM_PROT_WRITE;
		}

		va = trunc_page((vaddr_t)fault_addr);
		if (va == 0) {
			panic("trap: bad kernel access at %x", fault_addr);
		}

		vm = p->p_vmspace;
		map = kernel_map;

		if (frame->dsr & CMMU_DSR_BE) {
			/*
			 * If it is a guarded access, bus error is OK.
			 */
			if ((frame->exip & XIP_ADDR) >=
			      (unsigned)&guarded_access_start &&
			    (frame->exip & XIP_ADDR) <=
			      (unsigned)&guarded_access_end) {
				frame->exip = (unsigned)&guarded_access_bad;
				return;
			}
		}
		if (frame->dsr & (CMMU_DSR_SI | CMMU_DSR_PI)) {
			frame->dsr &= ~CMMU_DSR_WE;	/* undefined */
			/*
			 * On a segment or a page fault, call uvm_fault() to
			 * resolve the fault.
			 */
			result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
			if (result == 0)
				return;
		}
		if (frame->dsr & CMMU_DSR_WE) {	/* write fault  */
			/*
			 * This could be a write protection fault or an
			 * exception to set the used and modified bits
			 * in the pte. Basically, if we got a write error,
			 * then we already have a pte entry that faulted
			 * in from a previous seg fault or page fault.
			 * Get the pte and check the status of the
			 * modified and valid bits to determine if this
			 * indeed a real write fault.  XXX smurph
			 */
			pte = pmap_pte(map->pmap, va);
			if (pte == PT_ENTRY_NULL)
				panic("NULL pte on write fault??");
			if (!(*pte & PG_M) && !(*pte & PG_RO)) {
				/* Set modified bit and try the write again. */
				*pte |= PG_M;
				return;
#if 1	/* shouldn't happen */
			} else {
				/* must be a real wp fault */
#ifdef DEBUG
				printf("Kernel Write protect???? pte %x\n",
				    *pte);
#endif
				result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
				if (result == 0)
					return;
#endif
			}
		}

		/*
		 * if still the fault is not resolved ...
		 */
		if (!p->p_addr->u_pcb.pcb_onfault)
			panictrap(frame->vector, frame);

		frame->exip = ((unsigned)p->p_addr->u_pcb.pcb_onfault);
		return;
	case T_INSTFLT+T_USER:
		/* User mode instruction access fault */
		/* FALLTHROUGH */
	case T_DATAFLT+T_USER:
m88110_user_fault:
		if (type == T_INSTFLT+T_USER) {
			ftype = VM_PROT_READ;
			fault_code = VM_PROT_READ;
		} else {
			fault_addr = frame->dlar;
			if (frame->dsr & CMMU_DSR_RW) {
				ftype = VM_PROT_READ;
				fault_code = VM_PROT_READ;
			} else {
				ftype = VM_PROT_READ|VM_PROT_WRITE;
				fault_code = VM_PROT_WRITE;
			}
		}

		va = trunc_page((vaddr_t)fault_addr);

		vm = p->p_vmspace;
		map = &vm->vm_map;

		/*
		 * Call uvm_fault() to resolve non-bus error faults
		 * whenever possible.
		 */
		if (type == T_DATAFLT+T_USER) {
			/* data faults */
			if (frame->dsr & CMMU_DSR_BE) {
				/* bus error */
				result = EACCES;
			} else
			if (frame->dsr & (CMMU_DSR_SI | CMMU_DSR_PI)) {
				/* segment or page fault */
				result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
#ifdef DEBUG
				if (result != 0)
					printf("Data Access Error @ 0x%x\n", va);
#endif
			} else
			if (frame->dsr & (CMMU_DSR_CP | CMMU_DSR_WA)) {
				/* copyback or write allocate error */
				result = 0;
			} else
			if (frame->dsr & CMMU_DSR_WE) {
				/* write fault  */
				/* This could be a write protection fault or an
				 * exception to set the used and modified bits
				 * in the pte. Basically, if we got a write
				 * error, then we already have a pte entry that
				 * faulted in from a previous seg fault or page
				 * fault.
				 * Get the pte and check the status of the
				 * modified and valid bits to determine if this
				 * indeed a real write fault.  XXX smurph
				 */
				pte = pmap_pte(vm_map_pmap(map), va);
				if (pte == PT_ENTRY_NULL)
					panic("NULL pte on write fault??");
				if (!(*pte & PG_M) && !(*pte & PG_RO)) {
					/*
					 * Set modified bit and try the
					 * write again.
					 */
					*pte |= PG_M;
					/*
					 * invalidate ATCs to force
					 * table search
					 */
					set_dcmd(CMMU_DCMD_INV_UATC);
					return;
				} else {
					/* must be a real wp fault */
#ifdef DEBUG
					printf("Write protect???? pte %x\n",
					    *pte);
#endif
					result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
				}
			} else {
#ifdef DEBUG
				printf("unexpected data fault dsr %x\n",
				    frame->dsr);
#endif
				result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
			}
		} else {
			/* instruction faults */
			if (frame->isr & (CMMU_ISR_SI | CMMU_ISR_PI)) {
				/* segment or page fault */
				result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
			} else
			if (frame->isr &
			    (CMMU_ISR_BE | CMMU_ISR_SP | CMMU_ISR_TBE)) {
				/* bus error, supervisor protection */
				result = EACCES;
			} else {
#ifdef DEBUG
				printf("unexpected instr fault dsr %x\n",
				    frame->isr);
#endif
				result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
			}
		}

		if ((caddr_t)va >= vm->vm_maxsaddr) {
			if (result == 0) {
				nss = btoc(USRSTACK - va);/* XXX check this */
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (result == EACCES)
				result = EFAULT;
		}

		if (result != 0) {
			sig = result == EACCES ? SIGBUS : SIGSEGV;
			fault_type = result == EACCES ?
			    BUS_ADRERR : SEGV_MAPERR;
		}
		break;
	case T_MISALGNFLT+T_USER:
		sig = SIGBUS;
		fault_type = BUS_ADRALN;
		break;
	case T_PRIVINFLT+T_USER:
	case T_ILLFLT+T_USER:
#ifndef DDB
	case T_KDB_BREAK:
	case T_KDB_ENTRY:
	case T_KDB_TRACE:
#endif
	case T_KDB_BREAK+T_USER:
	case T_KDB_ENTRY+T_USER:
	case T_KDB_TRACE+T_USER:
		sig = SIGILL;
		break;
	case T_BNDFLT+T_USER:
		sig = SIGFPE;
		break;
	case T_ZERODIV+T_USER:
		sig = SIGFPE;
		fault_type = FPE_INTDIV;
		break;
	case T_OVFFLT+T_USER:
		sig = SIGFPE;
		fault_type = FPE_INTOVF;
		break;
	case T_FPEPFLT+T_USER:
	case T_FPEIFLT+T_USER:
		sig = SIGFPE;
		break;
	case T_SIGSYS+T_USER:
		sig = SIGSYS;
		break;
	case T_SIGTRAP+T_USER:
		sig = SIGTRAP;
		fault_type = TRAP_TRACE;
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
		{
			unsigned instr;
			struct uio uio;
			struct iovec iov;

			/* read break instruction */
			copyin((caddr_t)pc, &instr, sizeof(unsigned));
#if 0
			printf("trap: %s (%d) breakpoint %x at %x: (adr %x ins %x)\n",
			       p->p_comm, p->p_pid, instr, pc,
			       p->p_md.md_ss_addr, p->p_md.md_ss_instr); /* XXX */
#endif
			/* check and see if we got here by accident */
#ifdef notyet
			if (p->p_md.md_ss_addr != pc || instr != SSBREAKPOINT) {
				sig = SIGTRAP;
				fault_type = TRAP_TRACE;
				break;
			}
#endif
			/* restore original instruction and clear BP  */
			/*sig = suiword((caddr_t)pc, p->p_md.md_ss_instr);*/
			instr = p->p_md.md_ss_instr;
			if (instr != 0) {
				iov.iov_base = (caddr_t)&instr;
				iov.iov_len = sizeof(int);
				uio.uio_iov = &iov;
				uio.uio_iovcnt = 1;
				uio.uio_offset = (off_t)pc;
				uio.uio_resid = sizeof(int);
				uio.uio_segflg = UIO_SYSSPACE;
				uio.uio_rw = UIO_WRITE;
				uio.uio_procp = curproc;
			}

			p->p_md.md_ss_addr = 0;
			sig = SIGTRAP;
			fault_type = TRAP_BRKPT;
			break;
		}
	case T_USERBPT+T_USER:
		/*
		 * This trap is meant to be used by debuggers to implement
		 * breakpoint debugging.  When we get this trap, we just
		 * return a signal which gets caught by the debugger.
		 */
		sig = SIGTRAP;
		fault_type = TRAP_BRKPT;
		break;

	case T_ASTFLT+T_USER:
		uvmexp.softs++;
		want_ast = 0;
		if (p->p_flag & P_OWEUPC) {
			p->p_flag &= ~P_OWEUPC;
			ADDUPROF(p);
		}
		break;
	}

	/*
	 * If trap from supervisor mode, just return
	 */
	if (type < T_USER)
		return;

	if (sig) {
		sv.sival_int = fault_addr;
		trapsignal(p, sig, fault_code, fault_type, sv);
		/*
		 * don't want multiple faults - we are going to
		 * deliver signal.
		 */
		frame->dsr = frame->isr = 0;
	}

	userret(p, frame, sticks);
}

#endif /* MVME197 */

__dead void
error_fatal(struct m88100_saved_state *frame)
{
#ifdef DDB
	switch (frame->vector) {
	case 0:
		db_printf("\n[RESET EXCEPTION (Really Bad News[tm]) frame %8p]\n", frame);
		db_printf("This is usually caused by a branch to a NULL function pointer.\n");
		db_printf("e.g. jump to address 0.  Use the debugger trace command to track it down.\n");
		break;
	default:
		db_printf("\n[ERROR EXCEPTION (Bad News[tm]) frame %p]\n", frame);
		db_printf("This is usually an exception within an exception.  The trap\n");
		db_printf("frame shadow registers you are about to see are invalid.\n");
		db_printf("(read totaly useless)  But R1 to R31 might be interesting.\n");
		break;
	}
	regdump((struct trapframe*)frame);
#ifdef M88100
	db_printf("trap trace %d -> %d -> %d -> %d  ", last_trap[0], last_trap[1], last_trap[2], last_trap[3]);
	db_printf("last exception vector = %d\n", last_vector);
#endif
	Debugger();
#endif /* DDB */
	panic("unrecoverable exception %d", frame->vector);
}

#ifdef M88100
void
m88100_syscall(register_t code, struct m88100_saved_state *tf)
{
	int i, nsys, nap;
	struct sysent *callp;
	struct proc *p;
	int error;
	register_t args[11], rval[2], *ap;
	u_quad_t sticks;
#ifdef DIAGNOSTIC
	extern struct pcb *curpcb;
#endif

	uvmexp.syscalls++;

	p = curproc;

	callp = p->p_emul->e_sysent;
	nsys  = p->p_emul->e_nsysent;

#ifdef DIAGNOSTIC
	if (USERMODE(tf->epsr) == 0)
		panic("syscall");
	if (curpcb != &p->p_addr->u_pcb)
		panic("syscall curpcb/ppcb");
	if (tf != (struct trapframe *)&curpcb->user_state)
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
	nap = 11; /* r2-r12 */

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
		i = callp->sy_argsize / sizeof(register_t);
		if (i > nap)
			panic("syscall nargs");
		/*
		 * just copy them; syscall stub made sure all the
		 * args are moved from user stack to registers.
		 */
		bcopy((caddr_t)ap, (caddr_t)args, i * sizeof(register_t));
	}

#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p, code, callp->sy_argsize, args);
#endif
	rval[0] = 0;
	rval[1] = 0;
#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE))
		error = systrace_redirect(code, p, args, rval);
	else
#endif
		error = (*callp->sy_call)(p, args, rval);
	/*
	 * system call will look like:
	 *	 ld r10, r31, 32; r10,r11,r12 might be garbage.
	 *	 ld r11, r31, 36
	 *	 ld r12, r31, 40
	 *	 or r13, r0, <code>
	 *       tb0 0, r0, <128> <- sxip
	 *	 br err 	  <- snip
	 *       jmp r1 	  <- sfip
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
	 * 3. If the system call code returned ERESTART,
	 *    we need to rexecute the trap instruction. Back up the pipe
	 *    line.
	 *     fip = nip, nip = xip
	 * 4. If the system call returned EJUSTRETURN, don't need to adjust
	 *    any pointers.
	 */

	switch (error) {
	case 0:
		/*
		 * If fork succeeded and we are the child, our stack
		 * has moved and the pointer tf is no longer valid,
		 * and p is wrong.  Compute the new trapframe pointer.
		 * (The trap frame invariably resides at the
		 * tippity-top of the u. area.)
		 */
		p = curproc;
		tf = USER_REGS(p);
		tf->r[2] = rval[0];
		tf->r[3] = rval[1];
		tf->epsr &= ~PSR_C;
		tf->snip = tf->sfip & ~NIP_E;
		tf->sfip = tf->snip + 4;
		break;
	case ERESTART:
		/*
		 * If (error == ERESTART), back up the pipe line. This
		 * will end up reexecuting the trap.
		 */
		tf->epsr &= ~PSR_C;
		tf->sfip = tf->snip & ~FIP_E;
		tf->snip = tf->sxip & ~NIP_E;
		break;
	case EJUSTRETURN:
		/* if (error == EJUSTRETURN), leave the ip's alone */
		tf->epsr &= ~PSR_C;
		break;
	default:
		/* error != ERESTART && error != EJUSTRETURN*/
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		tf->r[2] = error;
		tf->epsr |= PSR_C;   /* fail */
		tf->snip = tf->snip & ~NIP_E;
		tf->sfip = tf->sfip & ~FIP_E;
		break;
	}
#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
	userret(p, tf, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, code, error, rval[0]);
#endif
}
#endif /* M88100 */

#ifdef M88110

/* Instruction pointers operate differently on mc88110 */
void
m88110_syscall(register_t code, struct m88100_saved_state *tf)
{
	int i, nsys, nap;
	struct sysent *callp;
	struct proc *p;
	int error;
	register_t args[11], rval[2], *ap;
	u_quad_t sticks;
#ifdef DIAGNOSTIC
	extern struct pcb *curpcb;
#endif

	uvmexp.syscalls++;

	p = curproc;

	callp = p->p_emul->e_sysent;
	nsys  = p->p_emul->e_nsysent;

#ifdef DIAGNOSTIC
	if (USERMODE(tf->epsr) == 0)
		panic("syscall");
	if (curpcb != &p->p_addr->u_pcb)
		panic("syscall curpcb/ppcb");
	if (tf != (struct trapframe *)&curpcb->user_state)
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
	nap = 11;	/* r2-r12 */

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
#ifdef DEBUG
	printf("syscall code is %d\n", code);
#endif
	if (code < 0 || code >= nsys)
		callp += p->p_emul->e_nosys;
	else {
		callp += code;
		i = callp->sy_argsize / sizeof(register_t);
		if (i > nap)
			panic("syscall nargs");
		/*
		 * just copy them; syscall stub made sure all the
		 * args are moved from user stack to registers.
		 */
		bcopy((caddr_t)ap, (caddr_t)args, i * sizeof(register_t));
	}
#ifdef SYSCALL_DEBUG
	scdebug_call(p, code, args);
#endif
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSCALL))
		ktrsyscall(p, code, callp->sy_argsize, args);
#endif
	rval[0] = 0;
	rval[1] = 0;
#if NSYSTRACE > 0
	if (ISSET(p->p_flag, P_SYSTRACE))
		error = systrace_redirect(code, p, args, rval);
	else
#endif
		error = (*callp->sy_call)(p, args, rval);
	/*
	 * system call will look like:
	 *	 ld r10, r31, 32; r10,r11,r12 might be garbage.
	 *	 ld r11, r31, 36
	 *	 ld r12, r31, 40
	 *	 or r13, r0, <code>
	 *       tb0 0, r0, <128> <- exip
	 *	 br err 	  <- enip
	 *       jmp r1
	 *  err: or.u r3, r0, hi16(errno)
	 *	 st r2, r3, lo16(errno)
	 *	 subu r2, r0, 1
	 *	 jmp r1
	 *
	 * So, when we take syscall trap, exip/enip will be as
	 * shown above.
	 * Given this,
	 * 1. If the system call returned 0, need to jmp r1.
	 *	   exip += 8
	 * 2. If the system call returned an errno > 0, increment
	 *    exip += 4 and plug the value in r2. This will have us
	 *    executing "br err" on return to user space.
	 * 3. If the system call code returned ERESTART,
	 *    we need to rexecute the trap instruction. leave exip as is.
	 * 4. If the system call returned EJUSTRETURN, just return.
	 *    exip += 4
	 */

	switch (error) {
	case 0:
		/*
		 * If fork succeeded and we are the child, our stack
		 * has moved and the pointer tf is no longer valid,
		 * and p is wrong.  Compute the new trapframe pointer.
		 * (The trap frame invariably resides at the
		 * tippity-top of the u. area.)
		 */
		p = curproc;
		tf = USER_REGS(p);
		tf->r[2] = rval[0];
		tf->r[3] = rval[1];
		tf->epsr &= ~PSR_C;
		tf->exip += 4 + 4;
		tf->exip &= XIP_ADDR;
		break;
	case ERESTART:
		/*
		 * Reexecute the trap.
		 * exip is already at the trap instruction, so
		 * there is nothing to do.
		 */
		tf->epsr &= ~PSR_C;
		break;
	case EJUSTRETURN:
		tf->epsr &= ~PSR_C;
		tf->exip += 4;
		tf->exip &= XIP_ADDR;
		break;
	default:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		tf->r[2] = error;
		tf->epsr |= PSR_C;   /* fail */
		tf->exip += 4;
		tf->exip &= XIP_ADDR;
		break;
	}

#ifdef SYSCALL_DEBUG
	scdebug_ret(p, code, error, rval);
#endif
	userret(p, tf, sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, code, error, rval[0]);
#endif
}
#endif	/* MVME197 */

/*
 * Set up return-value registers as fork() libc stub expects,
 * and do normal return-to-user-mode stuff.
 */
void
child_return(arg)
	void *arg;
{
	struct proc *p = arg;
	struct trapframe *tf;

	tf = USER_REGS(p);
	tf->r[2] = 0;
	tf->r[3] = 0;
	tf->epsr &= ~PSR_C;
	if (cputyp != CPU_88110) {
		tf->snip = tf->sfip & XIP_ADDR;
		tf->sfip = tf->snip + 4;
	} else {
		tf->exip += 4 + 4;
		tf->exip &= XIP_ADDR;
	}

	userret(p, tf, p->p_sticks);
#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, SYS_fork, 0, 0);
#endif
}

/************************************\
* User Single Step Debugging Support *
\************************************/

unsigned
ss_get_value(struct proc *p, unsigned addr, int size)
{
	struct uio uio;
	struct iovec iov;
	unsigned value;

	iov.iov_base = (caddr_t)&value;
	iov.iov_len = size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = curproc;
	procfs_domem(curproc, p, NULL, &uio);
	return value;
}

int
ss_put_value(struct proc *p, unsigned addr, unsigned value, int size)
{
	struct uio uio;
	struct iovec iov;
	int i;

	iov.iov_base = (caddr_t)&value;
	iov.iov_len = size;
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)addr;
	uio.uio_resid = size;
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = curproc;
	i = procfs_domem(curproc, p, NULL, &uio);
	return i;
}

/*
 * ss_branch_taken(instruction, program counter, func, func_data)
 *
 * instruction will be a control flow instruction location at address pc.
 * Branch taken is supposed to return the address to which the instruction
 * would jump if the branch is taken. Func can be used to get the current
 * register values when invoked with a register number and func_data as
 * arguments.
 *
 * If the instruction is not a control flow instruction, panic.
 */
unsigned
ss_branch_taken(
	       unsigned inst,
	       unsigned pc,
	       unsigned (*func)(unsigned int, struct trapframe *),
	       struct trapframe *func_data)  /* 'opaque' */
{

	/* check if br/bsr */
	if ((inst & 0xf0000000U) == 0xc0000000U) {
		/* signed 26 bit pc relative displacement, shift left two bits */
		inst = (inst & 0x03ffffffU)<<2;
		/* check if sign extension is needed */
		if (inst & 0x08000000U)
			inst |= 0xf0000000U;
		return pc + inst;
	}

	/* check if bb0/bb1/bcnd case */
	switch ((inst & 0xf8000000U)) {
	case 0xd0000000U: /* bb0 */
	case 0xd8000000U: /* bb1 */
	case 0xe8000000U: /* bcnd */
		/* signed 16 bit pc relative displacement, shift left two bits */
		inst = (inst & 0x0000ffffU)<<2;
		/* check if sign extension is needed */
		if (inst & 0x00020000U)
			inst |= 0xfffc0000U;
		return pc + inst;
	}

	/* check jmp/jsr case */
	/* check bits 5-31, skipping 10 & 11 */
	if ((inst & 0xfffff3e0U) == 0xf400c000U)
		return (*func)(inst & 0x1f, func_data);	 /* the register value */

	return 0; /* keeps compiler happy */
}

/*
 * ss_getreg_val - handed a register number and an exception frame.
 *              Returns the value of the register in the specified
 *              frame. Only makes sense for general registers.
 */
unsigned
ss_getreg_val(unsigned regno, struct trapframe *tf)
{
	if (regno == 0)
		return 0;
	else if (regno < 31)
		return tf->r[regno];
	else {
		panic("bad register number to ss_getreg_val.");
		return 0;/*to make compiler happy */
	}
}

int
ss_inst_branch(unsigned ins)
{
	/* check high five bits */

	switch (ins >> (32-5)) {
	case 0x18: /* br */
	case 0x1a: /* bb0 */
	case 0x1b: /* bb1 */
	case 0x1d: /* bcnd */
		return TRUE;
		break;
	case 0x1e: /* could be jmp */
		if ((ins & 0xfffffbe0U) == 0xf400c000U)
			return TRUE;
	}

	return FALSE;
}

/* ss_inst_delayed - this instruction is followed by a delay slot. Could be
   br.n, bsr.n bb0.n, bb1.n, bcnd.n or jmp.n or jsr.n */

int
ss_inst_delayed(unsigned ins)
{
	/* check the br, bsr, bb0, bb1, bcnd cases */
	switch ((ins & 0xfc000000U)>>(32-6)) {
	case 0x31: /* br */
	case 0x33: /* bsr */
	case 0x35: /* bb0 */
	case 0x37: /* bb1 */
	case 0x3b: /* bcnd */
		return TRUE;
	}

	/* check the jmp, jsr cases */
	/* mask out bits 0-4, bit 11 */
	return ((ins & 0xfffff7e0U) == 0xf400c400U) ? TRUE : FALSE;
}

unsigned
ss_next_instr_address(struct proc *p, unsigned pc, unsigned delay_slot)
{
	if (delay_slot == 0)
		return pc + 4;
	else {
		if (ss_inst_delayed(ss_get_value(p, pc, sizeof(int))))
			return pc + 4;
		else
			return pc;
	}
}

int
cpu_singlestep(p)
register struct proc *p;
{
	struct trapframe *sstf = USER_REGS(p); /*p->p_md.md_tf;*/
	unsigned pc, brpc;
	int i;
	int bpinstr = SSBREAKPOINT;
	unsigned curinstr;

	pc = PC_REGS(sstf);
	/*
	 * User was stopped at pc, e.g. the instruction
	 * at pc was not executed.
	 * Fetch what's at the current location.
	 */
	curinstr = ss_get_value(p, pc, sizeof(int));

	/* compute next address after current location */
	if (curinstr != 0) {
		if (ss_inst_branch(curinstr) || inst_call(curinstr) || inst_return(curinstr)) {
			brpc = ss_branch_taken(curinstr, pc, ss_getreg_val, sstf);
			if (brpc != pc) {   /* self-branches are hopeless */
#if 0
				printf("SS %s (%d): next taken breakpoint set at %x\n",
				       p->p_comm, p->p_pid, brpc);
#endif
				p->p_md.md_ss_taken_addr = brpc;
				p->p_md.md_ss_taken_instr = ss_get_value(p, brpc, sizeof(int));
				/* Store breakpoint instruction at the "next" location now. */
				i = ss_put_value(p, brpc, bpinstr, sizeof(int));
				if (i < 0) return (EFAULT);
			}
		}
		pc = ss_next_instr_address(p, pc, 0);
#if 0
		printf("SS %s (%d): next breakpoint set at %x\n",
		       p->p_comm, p->p_pid, pc);
#endif
	} else {
		pc = PC_REGS(sstf) + 4;
#if 0
		printf("SS %s (%d): next breakpoint set at %x\n",
		       p->p_comm, p->p_pid, pc);
#endif
	}

	if (p->p_md.md_ss_addr) {
#if 0
		printf("SS %s (%d): breakpoint already set at %x (va %x)\n",
		       p->p_comm, p->p_pid, p->p_md.md_ss_addr, pc); /* XXX */
#endif
		return (EFAULT);
	}

	p->p_md.md_ss_addr = pc;

	/* Fetch what's at the "next" location. */
	p->p_md.md_ss_instr = ss_get_value(p, pc, sizeof(int));

	/* Store breakpoint instruction at the "next" location now. */
	i = ss_put_value(p, pc, bpinstr, sizeof(int));

	if (i < 0) return (EFAULT);
	return (0);
}


#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int oldipl;

	oldipl = getipl();

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
		/*
		 * If the splassert_ctl is set to not panic, raise the ipl
		 * in a feeble attempt to reduce damage.
		 */
		setipl(wantipl);
	}
}
#endif
