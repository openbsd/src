/*	$OpenBSD: trap.c,v 1.24 2005/12/01 22:24:52 miod Exp $	*/
/*
 * Copyright (c) 2004, Miodrag Vallat.
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
#include <machine/cmmu.h>
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

#define SSBREAKPOINT (0xF000D1F8U) /* Single Step Breakpoint */

#define USERMODE(PSR)   (((PSR) & PSR_MODE) == 0)
#define SYSTEMMODE(PSR) (((PSR) & PSR_MODE) != 0)

/* sigh */
extern int procfs_domem(struct proc *, struct proc *, void *, struct uio *);

__dead void panictrap(int, struct trapframe *);
__dead void error_fatal(struct trapframe *);
int double_reg_fixup(struct trapframe *);
int ss_put_value(struct proc *, vaddr_t, u_int);

extern void regdump(struct trapframe *f);

const char *trap_type[] = {
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
const int trap_types = sizeof trap_type / sizeof trap_type[0];

const char *pbus_exception_type[] = {
	"Success (No Fault)",
	"unknown 1",
	"unknown 2",
	"Bus Error",
	"Segment Fault",
	"Page Fault",
	"Supervisor Violation",
	"Write Violation",
};

static inline void
userret(struct proc *p, struct trapframe *frame, u_quad_t oticks)
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

		addupc_task(p, frame->tf_sxip & XIP_ADDR,
		    (int)(p->p_sticks - oticks) * psratio);
	}
	curpriority = p->p_priority;
}

__dead void
panictrap(int type, struct trapframe *frame)
{
	static int panicing = 0;

	if (panicing++ == 0) {
#ifdef M88100
		if (CPU_IS88100) {
			if (type == 2) {
				/* instruction exception */
				printf("\nInstr access fault (%s) v = %x, "
				    "frame %p\n",
				    pbus_exception_type[
				      CMMU_PFSR_FAULT(frame->tf_ipfsr)],
				    frame->tf_sxip & XIP_ADDR, frame);
			} else if (type == 3) {
				/* data access exception */
				printf("\nData access fault (%s) v = %x, "
				    "frame %p\n",
				    pbus_exception_type[
				      CMMU_PFSR_FAULT(frame->tf_dpfsr)],
				    frame->tf_sxip & XIP_ADDR, frame);
			} else
				printf("\nTrap type %d, v = %x, frame %p\n",
				    type, frame->tf_sxip & XIP_ADDR, frame);
		}
#endif
#ifdef M88110
		if (CPU_IS88110) {
			printf("\nTrap type %d, v = %x, frame %p\n",
			    type, frame->tf_exip, frame);
		}
#endif
#ifdef DDB
		regdump(frame);
#endif
	}
	if ((u_int)type < trap_types)
		panic(trap_type[type]);
	else
		panic("trap %d", type);
	/*NOTREACHED*/
}

#ifdef M88100
void
m88100_trap(unsigned type, struct trapframe *frame)
{
	struct proc *p;
	u_quad_t sticks = 0;
	struct vm_map *map;
	vaddr_t va, pcb_onfault;
	vm_prot_t ftype;
	int fault_type, pbus_type;
	u_long fault_code;
	unsigned fault_addr;
	struct vmspace *vm;
	union sigval sv;
	int result;
#ifdef DDB
	int s;
	u_int psr;
#endif
	int sig = 0;

	extern struct vm_map *kernel_map;

	uvmexp.traps++;
	if ((p = curproc) == NULL)
		p = &proc0;

	if (USERMODE(frame->tf_epsr)) {
		sticks = p->p_sticks;
		type += T_USER;
		p->p_md.md_tf = frame;	/* for ptrace/signals */
	}
	fault_type = 0;
	fault_code = 0;
	fault_addr = frame->tf_sxip & XIP_ADDR;

	switch (type) {
	default:
		panictrap(frame->tf_vector, frame);
		break;
		/*NOTREACHED*/

#if defined(DDB)
	case T_KDB_BREAK:
		s = splhigh();
		db_enable_interrupt(psr);
		ddb_break_trap(T_KDB_BREAK, (db_regs_t*)frame);
		db_disable_interrupt(psr);
		splx(s);
		return;
	case T_KDB_ENTRY:
		s = splhigh();
		db_enable_interrupt(psr);
		ddb_entry_trap(T_KDB_ENTRY, (db_regs_t*)frame);
		db_disable_interrupt(psr);
		splx(s);
		return;
#endif /* DDB */
	case T_ILLFLT:
		printf("Unimplemented opcode!\n");
		panictrap(frame->tf_vector, frame);
		break;
	case T_INT:
	case T_INT+T_USER:
		/* This function pointer is set in machdep.c
		   It calls m188_ext_int or sbc_ext_int depending
		   on the value of brdtyp - smurph */
		intrdepth++;
		md_interrupt_func(T_INT, frame);
		intrdepth--;
		return;

	case T_MISALGNFLT:
		printf("kernel misaligned access exception @ 0x%08x\n",
		    frame->tf_sxip);
		panictrap(frame->tf_vector, frame);
		break;

	case T_INSTFLT:
		/* kernel mode instruction access fault.
		 * Should never, never happen for a non-paged kernel.
		 */
#ifdef TRAPDEBUG
		pbus_type = CMMU_PFSR_FAULT(frame->tf_ipfsr);
		printf("Kernel Instruction fault #%d (%s) v = 0x%x, frame 0x%x cpu %d\n",
		    pbus_type, pbus_exception_type[pbus_type],
		    fault_addr, frame, frame->tf_cpu);
#endif
		panictrap(frame->tf_vector, frame);
		break;

	case T_DATAFLT:
		/* kernel mode data fault */

		/* data fault on the user address? */
		if ((frame->tf_dmt0 & DMT_DAS) == 0) {
			type = T_DATAFLT + T_USER;
			goto user_fault;
		}

		fault_addr = frame->tf_dma0;
		if (frame->tf_dmt0 & (DMT_WRITE|DMT_LOCKBAR)) {
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

		pbus_type = CMMU_PFSR_FAULT(frame->tf_dpfsr);
#ifdef TRAPDEBUG
		printf("Kernel Data access fault #%d (%s) v = 0x%x, frame 0x%x cpu %d\n",
		    pbus_type, pbus_exception_type[pbus_type],
		    fault_addr, frame, frame->tf_cpu);
#endif

		switch (pbus_type) {
		case CMMU_PFSR_SUCCESS:
			/*
			 * The fault was resolved. Call data_access_emulation
			 * to drain the data unit pipe line and reset dmt0
			 * so that trap won't get called again.
			 */
			data_access_emulation((unsigned *)frame);
			frame->tf_dpfsr = 0;
			frame->tf_dmt0 = 0;
			return;
		case CMMU_PFSR_SFAULT:
		case CMMU_PFSR_PFAULT:
			if ((pcb_onfault = p->p_addr->u_pcb.pcb_onfault) != 0)
				p->p_addr->u_pcb.pcb_onfault = 0;
			result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
			p->p_addr->u_pcb.pcb_onfault = pcb_onfault;
			if (result == 0) {
				/*
				 * We could resolve the fault. Call
				 * data_access_emulation to drain the data
				 * unit pipe line and reset dmt0 so that trap
				 * won't get called again.
				 */
				data_access_emulation((unsigned *)frame);
				frame->tf_dpfsr = 0;
				frame->tf_dmt0 = 0;
				return;
			}
			break;
		}
#ifdef TRAPDEBUG
		printf("PBUS Fault %d (%s) va = 0x%x\n", pbus_type,
		    pbus_exception_type[pbus_type], va);
#endif
		panictrap(frame->tf_vector, frame);
		/* NOTREACHED */
	case T_INSTFLT+T_USER:
		/* User mode instruction access fault */
		/* FALLTHROUGH */
	case T_DATAFLT+T_USER:
user_fault:
		if (type == T_INSTFLT + T_USER) {
			pbus_type = CMMU_PFSR_FAULT(frame->tf_ipfsr);
#ifdef TRAPDEBUG
			printf("User Instruction fault #%d (%s) v = 0x%x, frame 0x%x cpu %d\n",
			    pbus_type, pbus_exception_type[pbus_type],
			    fault_addr, frame, frame->tf_cpu);
#endif
		} else {
			fault_addr = frame->tf_dma0;
			pbus_type = CMMU_PFSR_FAULT(frame->tf_dpfsr);
#ifdef TRAPDEBUG
			printf("User Data access fault #%d (%s) v = 0x%x, frame 0x%x cpu %d\n",
			    pbus_type, pbus_exception_type[pbus_type],
			    fault_addr, frame, frame->tf_cpu);
#endif
		}

		if (frame->tf_dmt0 & (DMT_WRITE | DMT_LOCKBAR)) {
			ftype = VM_PROT_READ | VM_PROT_WRITE;
			fault_code = VM_PROT_WRITE;
		} else {
			ftype = VM_PROT_READ;
			fault_code = VM_PROT_READ;
		}

		va = trunc_page((vaddr_t)fault_addr);

		vm = p->p_vmspace;
		map = &vm->vm_map;
		if ((pcb_onfault = p->p_addr->u_pcb.pcb_onfault) != 0)
			p->p_addr->u_pcb.pcb_onfault = 0;

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

		p->p_addr->u_pcb.pcb_onfault = pcb_onfault;

		if ((caddr_t)va >= vm->vm_maxsaddr) {
			if (result == 0)
				uvm_grow(p, va);
			else if (result == EACCES)
				result = EFAULT;
		}

		/*
		 * This could be a fault caused in copyin*()
		 * while accessing user space.
		 */
		if (result != 0 && pcb_onfault != 0) {
			frame->tf_snip = pcb_onfault | NIP_V;
			frame->tf_sfip = (pcb_onfault + 4) | FIP_V;
			frame->tf_sxip = 0;
			/*
			 * Continue as if the fault had been resolved, but
			 * do not try to complete the faulting access.
			 */
			frame->tf_dmt0 |= DMT_SKIP;
			result = 0;
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
				frame->tf_dpfsr = 0;
				frame->tf_dmt0 = 0;
			} else {
				/*
				 * back up SXIP, SNIP,
				 * clearing the Error bit
				 */
				frame->tf_sfip = frame->tf_snip & ~FIP_E;
				frame->tf_snip = frame->tf_sxip & ~NIP_E;
				frame->tf_ipfsr = 0;
			}
		} else {
			sig = result == EACCES ? SIGBUS : SIGSEGV;
			fault_type = result == EACCES ?
			    BUS_ADRERR : SEGV_MAPERR;
		}
		break;
	case T_MISALGNFLT+T_USER:
		/* Fix any misaligned ld.d or st.d instructions */
		sig = double_reg_fixup(frame);
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
#ifdef PTRACE
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
			u_int instr;
			vaddr_t pc = PC_REGS(&frame->tf_regs);

			/* read break instruction */
			copyin((caddr_t)pc, &instr, sizeof(u_int));

			/* check and see if we got here by accident */
			if ((p->p_md.md_bp0va != pc &&
			     p->p_md.md_bp1va != pc) ||
			    instr != SSBREAKPOINT) {
				sig = SIGTRAP;
				fault_type = TRAP_TRACE;
				break;
			}

			/* restore original instruction and clear breakpoint */
			if (p->p_md.md_bp0va == pc) {
				ss_put_value(p, pc, p->p_md.md_bp0save);
				p->p_md.md_bp0va = 0;
			}
			if (p->p_md.md_bp1va == pc) {
				ss_put_value(p, pc, p->p_md.md_bp1save);
				p->p_md.md_bp1va = 0;
			}

#if 1
			frame->tf_sfip = frame->tf_snip;
			frame->tf_snip = pc | NIP_V;
#endif
			sig = SIGTRAP;
			fault_type = TRAP_BRKPT;
		}
#else
		sig = SIGTRAP;
		fault_type = TRAP_TRACE;
#endif
		break;

	case T_USERBPT+T_USER:
		/*
		 * This trap is meant to be used by debuggers to implement
		 * breakpoint debugging.  When we get this trap, we just
		 * return a signal which gets caught by the debugger.
		 */
		frame->tf_sfip = frame->tf_snip;
		frame->tf_snip = frame->tf_sxip;
		sig = SIGTRAP;
		fault_type = TRAP_BRKPT;
		break;

	case T_ASTFLT+T_USER:
		uvmexp.softs++;
		p->p_md.md_astpending = 0;
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
		frame->tf_dmt0 = 0;
		frame->tf_ipfsr = frame->tf_dpfsr = 0;
	}

	userret(p, frame, sticks);
}
#endif /* M88100 */

#ifdef M88110
void
m88110_trap(unsigned type, struct trapframe *frame)
{
	struct proc *p;
	u_quad_t sticks = 0;
	struct vm_map *map;
	vaddr_t va, pcb_onfault;
	vm_prot_t ftype;
	int fault_type;
	u_long fault_code;
	unsigned fault_addr;
	struct vmspace *vm;
	union sigval sv;
	int result;
#ifdef DDB
        int s;
	u_int psr;
#endif
	int sig = 0;
	pt_entry_t *pte;

	extern struct vm_map *kernel_map;
	extern pt_entry_t *pmap_pte(pmap_t, vaddr_t);

	uvmexp.traps++;
	if ((p = curproc) == NULL)
		p = &proc0;

	if (USERMODE(frame->tf_epsr)) {
		sticks = p->p_sticks;
		type += T_USER;
		p->p_md.md_tf = frame;	/* for ptrace/signals */
	}
	fault_type = 0;
	fault_code = 0;
	fault_addr = frame->tf_exip & XIP_ADDR;

	switch (type) {
	default:
		panictrap(frame->tf_vector, frame);
		break;
		/*NOTREACHED*/

	case T_197_READ+T_USER:
	case T_197_READ:
		printf("DMMU read miss: Hardware Table Searches should be enabled!\n");
		panictrap(frame->tf_vector, frame);
		break;
		/*NOTREACHED*/
	case T_197_WRITE+T_USER:
	case T_197_WRITE:
		printf("DMMU write miss: Hardware Table Searches should be enabled!\n");
		panictrap(frame->tf_vector, frame);
		break;
		/*NOTREACHED*/
	case T_197_INST+T_USER:
	case T_197_INST:
		printf("IMMU miss: Hardware Table Searches should be enabled!\n");
		panictrap(frame->tf_vector, frame);
		break;
		/*NOTREACHED*/
#ifdef DDB
	case T_KDB_TRACE:
		s = splhigh();
		db_enable_interrupt(psr);
		ddb_break_trap(T_KDB_TRACE, (db_regs_t*)frame);
		db_disable_interrupt(psr);
		splx(s);
		return;
	case T_KDB_BREAK:
		s = splhigh();
		db_enable_interrupt(psr);
		ddb_break_trap(T_KDB_BREAK, (db_regs_t*)frame);
		db_disable_interrupt(psr);
		splx(s);
		return;
	case T_KDB_ENTRY:
		s = splhigh();
		db_enable_interrupt(psr);
		ddb_entry_trap(T_KDB_ENTRY, (db_regs_t*)frame);
		db_disable_interrupt(psr);
		/* skip one instruction */
		if (frame->tf_exip & 1)
			frame->tf_exip = frame->tf_enip;
		else
			frame->tf_exip += 4;
		splx(s);
		return;
#if 0
	case T_ILLFLT:
		s = splhigh();
		db_enable_interrupt(psr);
		ddb_error_trap(type == T_ILLFLT ? "unimplemented opcode" :
		       "error fault", (db_regs_t*)frame);
		db_disable_interrupt(psr);
		splx(s);
		return;
#endif /* 0 */
#endif /* DDB */
	case T_ILLFLT:
		printf("Unimplemented opcode!\n");
		panictrap(frame->tf_vector, frame);
		break;
	case T_NON_MASK:
	case T_NON_MASK+T_USER:
		intrdepth++;
		md_interrupt_func(T_NON_MASK, frame);
		intrdepth--;
		return;
	case T_INT:
	case T_INT+T_USER:
		intrdepth++;
		md_interrupt_func(T_INT, frame);
		intrdepth--;
		return;
	case T_MISALGNFLT:
		printf("kernel mode misaligned access exception @ 0x%08x\n",
		    frame->tf_exip);
		panictrap(frame->tf_vector, frame);
		break;
		/*NOTREACHED*/

	case T_INSTFLT:
		/* kernel mode instruction access fault.
		 * Should never, never happen for a non-paged kernel.
		 */
#ifdef TRAPDEBUG
		printf("Kernel Instruction fault exip %x isr %x ilar %x\n",
		    frame->tf_exip, frame->tf_isr, frame->tf_ilar);
#endif
		panictrap(frame->tf_vector, frame);
		break;
		/*NOTREACHED*/

	case T_DATAFLT:
		/* kernel mode data fault */

		/* data fault on the user address? */
		if ((frame->tf_dsr & CMMU_DSR_SU) == 0) {
			type = T_DATAFLT + T_USER;
			goto m88110_user_fault;
		}

#ifdef TRAPDEBUG
		printf("Kernel Data access fault exip %x dsr %x dlar %x\n",
		    frame->tf_exip, frame->tf_dsr, frame->tf_dlar);
#endif

		fault_addr = frame->tf_dlar;
		if (frame->tf_dsr & CMMU_DSR_RW) {
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

		if (frame->tf_dsr & (CMMU_DSR_SI | CMMU_DSR_PI)) {
			frame->tf_dsr &= ~CMMU_DSR_WE;	/* undefined */
			/*
			 * On a segment or a page fault, call uvm_fault() to
			 * resolve the fault.
			 */
			if ((pcb_onfault = p->p_addr->u_pcb.pcb_onfault) != 0)
				p->p_addr->u_pcb.pcb_onfault = 0;
			result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
			p->p_addr->u_pcb.pcb_onfault = pcb_onfault;
			if (result == 0)
				return;
		}
		if (frame->tf_dsr & CMMU_DSR_WE) {	/* write fault  */
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
#ifdef DEBUG
			if (pte == NULL)
				panic("NULL pte on write fault??");
#endif
			if (!(*pte & PG_M) && !(*pte & PG_RO)) {
				/* Set modified bit and try the write again. */
#ifdef TRAPDEBUG
				printf("Corrected kernel write fault, map %x pte %x\n",
				    map->pmap, *pte);
#endif
				*pte |= PG_M;
				return;
#if 1	/* shouldn't happen */
			} else {
				/* must be a real wp fault */
#ifdef TRAPDEBUG
				printf("Uncorrected kernel write fault, map %x pte %x\n",
				    map->pmap, *pte);
#endif
				if ((pcb_onfault = p->p_addr->u_pcb.pcb_onfault) != 0)
					p->p_addr->u_pcb.pcb_onfault = 0;
				result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
				p->p_addr->u_pcb.pcb_onfault = pcb_onfault;
				if (result == 0)
					return;
#endif
			}
		}
		panictrap(frame->tf_vector, frame);
		/* NOTREACHED */
	case T_INSTFLT+T_USER:
		/* User mode instruction access fault */
		/* FALLTHROUGH */
	case T_DATAFLT+T_USER:
m88110_user_fault:
		if (type == T_INSTFLT+T_USER) {
			ftype = VM_PROT_READ;
			fault_code = VM_PROT_READ;
#ifdef TRAPDEBUG
			printf("User Instruction fault exip %x isr %x ilar %x\n",
			    frame->tf_exip, frame->tf_isr, frame->tf_ilar);
#endif
		} else {
			fault_addr = frame->tf_dlar;
			if (frame->tf_dsr & CMMU_DSR_RW) {
				ftype = VM_PROT_READ;
				fault_code = VM_PROT_READ;
			} else {
				ftype = VM_PROT_READ|VM_PROT_WRITE;
				fault_code = VM_PROT_WRITE;
			}
#ifdef TRAPDEBUG
			printf("User Data access fault exip %x dsr %x dlar %x\n",
			    frame->tf_exip, frame->tf_dsr, frame->tf_dlar);
#endif
		}

		va = trunc_page((vaddr_t)fault_addr);

		vm = p->p_vmspace;
		map = &vm->vm_map;
		if ((pcb_onfault = p->p_addr->u_pcb.pcb_onfault) != 0)
			p->p_addr->u_pcb.pcb_onfault = 0;

		/*
		 * Call uvm_fault() to resolve non-bus error faults
		 * whenever possible.
		 */
		if (type == T_DATAFLT+T_USER) {
			/* data faults */
			if (frame->tf_dsr & CMMU_DSR_BE) {
				/* bus error */
				result = EACCES;
			} else
			if (frame->tf_dsr & (CMMU_DSR_SI | CMMU_DSR_PI)) {
				/* segment or page fault */
				result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
				p->p_addr->u_pcb.pcb_onfault = pcb_onfault;
			} else
			if (frame->tf_dsr & (CMMU_DSR_CP | CMMU_DSR_WA)) {
				/* copyback or write allocate error */
				result = EACCES;
			} else
			if (frame->tf_dsr & CMMU_DSR_WE) {
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
#ifdef DEBUG
				if (pte == NULL)
					panic("NULL pte on write fault??");
#endif
				if (!(*pte & PG_M) && !(*pte & PG_RO)) {
					/*
					 * Set modified bit and try the
					 * write again.
					 */
#ifdef TRAPDEBUG
					printf("Corrected userland write fault, map %x pte %x\n",
					    map->pmap, *pte);
#endif
					*pte |= PG_M;
					/*
					 * invalidate ATCs to force
					 * table search
					 */
					set_dcmd(CMMU_DCMD_INV_UATC);
					return;
				} else {
					/* must be a real wp fault */
#ifdef TRAPDEBUG
					printf("Uncorrected userland write fault, map %x pte %x\n",
					    map->pmap, *pte);
#endif
					result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
					p->p_addr->u_pcb.pcb_onfault = pcb_onfault;
				}
			} else {
#ifdef TRAPDEBUG
				printf("Unexpected Data access fault dsr %x\n",
				    frame->tf_dsr);
#endif
				panictrap(frame->tf_vector, frame);
			}
		} else {
			/* instruction faults */
			if (frame->tf_isr &
			    (CMMU_ISR_BE | CMMU_ISR_SP | CMMU_ISR_TBE)) {
				/* bus error, supervisor protection */
				result = EACCES;
			} else
			if (frame->tf_isr & (CMMU_ISR_SI | CMMU_ISR_PI)) {
				/* segment or page fault */
				result = uvm_fault(map, va, VM_FAULT_INVALID, ftype);
				p->p_addr->u_pcb.pcb_onfault = pcb_onfault;
			} else {
#ifdef TRAPDEBUG
				printf("Unexpected Instruction fault isr %x\n",
				    frame->tf_isr);
#endif
				panictrap(frame->tf_vector, frame);
			}
		}

		if ((caddr_t)va >= vm->vm_maxsaddr) {
			if (result == 0)
				uvm_grow(p, va);
			else if (result == EACCES)
				result = EFAULT;
		}

		/*
		 * This could be a fault caused in copyin*()
		 * while accessing user space.
		 */
		if (result != 0 && pcb_onfault != 0) {
			frame->tf_exip = pcb_onfault;
			/*
			 * Continue as if the fault had been resolved.
			 */
			result = 0;
		}

		if (result != 0) {
			sig = result == EACCES ? SIGBUS : SIGSEGV;
			fault_type = result == EACCES ?
			    BUS_ADRERR : SEGV_MAPERR;
		}
		break;
	case T_MISALGNFLT+T_USER:
		/* Fix any misaligned ld.d or st.d instructions */
		sig = double_reg_fixup(frame);
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
#ifdef PTRACE
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
			u_int instr;
			vaddr_t pc = PC_REGS(&frame->tf_regs);

			/* read break instruction */
			copyin((caddr_t)pc, &instr, sizeof(u_int));

			/* check and see if we got here by accident */
			if ((p->p_md.md_bp0va != pc &&
			     p->p_md.md_bp1va != pc) ||
			    instr != SSBREAKPOINT) {
				sig = SIGTRAP;
				fault_type = TRAP_TRACE;
				break;
			}

			/* restore original instruction and clear breakpoint */
			if (p->p_md.md_bp0va == pc) {
				ss_put_value(p, pc, p->p_md.md_bp0save);
				p->p_md.md_bp0va = 0;
			}
			if (p->p_md.md_bp1va == pc) {
				ss_put_value(p, pc, p->p_md.md_bp1save);
				p->p_md.md_bp1va = 0;
			}

			sig = SIGTRAP;
			fault_type = TRAP_BRKPT;
		}
#else
		sig = SIGTRAP;
		fault_type = TRAP_TRACE;
#endif
		break;
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
		p->p_md.md_astpending = 0;
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
	}

	userret(p, frame, sticks);
}
#endif /* M88110 */

__dead void
error_fatal(struct trapframe *frame)
{
	if (frame->tf_vector == 0)
		printf("\nReset Exception\n");
	else
		printf("\nError Exception\n");

#ifdef DDB
	regdump((struct trapframe*)frame);
#endif
	panic("unrecoverable exception %d", frame->tf_vector);
}

#ifdef M88100
void
m88100_syscall(register_t code, struct trapframe *tf)
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
	if (USERMODE(tf->tf_epsr) == 0)
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
	ap = &tf->tf_r[2];
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
	rval[1] = tf->tf_r[3];
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
		tf->tf_r[2] = rval[0];
		tf->tf_r[3] = rval[1];
		tf->tf_epsr &= ~PSR_C;
		tf->tf_snip = tf->tf_sfip & ~NIP_E;
		tf->tf_sfip = tf->tf_snip + 4;
		break;
	case ERESTART:
		/*
		 * If (error == ERESTART), back up the pipe line. This
		 * will end up reexecuting the trap.
		 */
		tf->tf_epsr &= ~PSR_C;
		tf->tf_sfip = tf->tf_snip & ~FIP_E;
		tf->tf_snip = tf->tf_sxip & ~NIP_E;
		break;
	case EJUSTRETURN:
		/* if (error == EJUSTRETURN), leave the ip's alone */
		tf->tf_epsr &= ~PSR_C;
		break;
	default:
		/* error != ERESTART && error != EJUSTRETURN*/
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		tf->tf_r[2] = error;
		tf->tf_epsr |= PSR_C;   /* fail */
		tf->tf_snip = tf->tf_snip & ~NIP_E;
		tf->tf_sfip = tf->tf_sfip & ~FIP_E;
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
m88110_syscall(register_t code, struct trapframe *tf)
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
	if (USERMODE(tf->tf_epsr) == 0)
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
	ap = &tf->tf_r[2];
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
	rval[1] = tf->tf_r[3];
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
	 *    exip += 8
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
		tf->tf_r[2] = rval[0];
		tf->tf_r[3] = rval[1];
		tf->tf_epsr &= ~PSR_C;
		/* skip two instructions */
		if (tf->tf_exip & 1)
			tf->tf_exip = tf->tf_enip + 4;
		else
			tf->tf_exip += 4 + 4;
		break;
	case ERESTART:
		/*
		 * Reexecute the trap.
		 * exip is already at the trap instruction, so
		 * there is nothing to do.
		 */
		tf->tf_epsr &= ~PSR_C;
		break;
	case EJUSTRETURN:
		tf->tf_epsr &= ~PSR_C;
		/* skip one instruction */
		if (tf->tf_exip & 1)
			tf->tf_exip = tf->tf_enip;
		else
			tf->tf_exip += 4;
		break;
	default:
		if (p->p_emul->e_errno)
			error = p->p_emul->e_errno[error];
		tf->tf_r[2] = error;
		tf->tf_epsr |= PSR_C;   /* fail */
		/* skip one instruction */
		if (tf->tf_exip & 1)
			tf->tf_exip = tf->tf_enip;
		else
			tf->tf_exip += 4;
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
#endif	/* M88110 */

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

	tf = (struct trapframe *)USER_REGS(p);
	tf->tf_r[2] = 0;
	tf->tf_r[3] = 0;
	tf->tf_epsr &= ~PSR_C;
#ifdef M88100
	if (CPU_IS88100) {
		tf->tf_snip = tf->tf_sfip & XIP_ADDR;
		tf->tf_sfip = tf->tf_snip + 4;
	}
#endif
#ifdef M88110
	if (CPU_IS88110) {
		/* skip two instructions */
		if (tf->tf_exip & 1)
			tf->tf_exip = tf->tf_enip + 4;
		else
			tf->tf_exip += 4 + 4;
	}
#endif

	userret(p, tf, p->p_sticks);

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p,
		    (p->p_flag & P_PPWAIT) ? SYS_vfork : SYS_fork, 0, 0);
#endif
}

#ifdef PTRACE

/*
 * User Single Step Debugging Support
 */

#include <sys/ptrace.h>

vaddr_t	ss_branch_taken(u_int, vaddr_t, u_int (*func)(u_int, struct reg *),
	    struct reg *);
u_int	ss_getreg_val(u_int, struct reg *);
int	ss_get_value(struct proc *, vaddr_t, u_int *);
int	ss_inst_branch(unsigned);
int	ss_inst_delayed(unsigned);
int	ss_put_breakpoint(struct proc *, vaddr_t, vaddr_t *, u_int *);

#define	SYSCALL_INSTR	0xf000d080	/* tb0 0,r0,128 */

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
	return (procfs_domem(curproc, p, NULL, &uio));
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
	return (procfs_domem(curproc, p, NULL, &uio));
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
vaddr_t
ss_branch_taken(u_int inst, vaddr_t pc, u_int (*func)(u_int, struct reg *),
    struct reg *func_data)
{
	/* check for system call */
	if (inst == SYSCALL_INSTR) {
		/*
		 * The regular (pc + 4) breakpoint will match the error
		 * return. Successfull system calls return at (pc + 8),
		 * so we'll set up a branch breakpoint there.
		 */
		return (pc + 8);
	}

	/* check if br/bsr */
	if ((inst & 0xf0000000) == 0xc0000000) {
		/* signed 26 bit pc relative displacement, shift left two bits */
		inst = (inst & 0x03ffffff) << 2;
		/* check if sign extension is needed */
		if (inst & 0x08000000)
			inst |= 0xf0000000;
		return (pc + inst);
	}

	/* check if bb0/bb1/bcnd case */
	switch (inst & 0xf8000000) {
	case 0xd0000000: /* bb0 */
	case 0xd8000000: /* bb1 */
	case 0xe8000000: /* bcnd */
		/* signed 16 bit pc relative displacement, shift left two bits */
		inst = (inst & 0x0000ffff) << 2;
		/* check if sign extension is needed */
		if (inst & 0x00020000)
			inst |= 0xfffc0000;
		return (pc + inst);
	}

	/* check jmp/jsr case */
	/* check bits 5-31, skipping 10 & 11 */
	if ((inst & 0xfffff3e0) == 0xf400c000)
		return (*func)(inst & 0x1f, func_data);	 /* the register value */

	/* can't happen */
	return (0);
}

/*
 * ss_getreg_val - handed a register number and an exception frame.
 *              Returns the value of the register in the specified
 *              frame. Only makes sense for general registers.
 */
u_int
ss_getreg_val(u_int regno, struct reg *regs)
{
	return (regno == 0 ? 0 : regs->r[regno]);
}

int
ss_inst_branch(u_int ins)
{
	/* check high five bits */

	switch (ins >> (32 - 5)) {
	case 0x18: /* br */
	case 0x1a: /* bb0 */
	case 0x1b: /* bb1 */
	case 0x1d: /* bcnd */
		return (TRUE);
		break;
	case 0x1e: /* could be jmp */
		if ((ins & 0xfffffbe0) == 0xf400c000)
			return (TRUE);
	}

	return (FALSE);
}

/* ss_inst_delayed - this instruction is followed by a delay slot. Could be
   br.n, bsr.n bb0.n, bb1.n, bcnd.n or jmp.n or jsr.n */

int
ss_inst_delayed(u_int ins)
{
	/* check the br, bsr, bb0, bb1, bcnd cases */
	switch ((ins & 0xfc000000) >> (32 - 6)) {
	case 0x31: /* br */
	case 0x33: /* bsr */
	case 0x35: /* bb0 */
	case 0x37: /* bb1 */
	case 0x3b: /* bcnd */
		return (TRUE);
	}

	/* check the jmp, jsr cases */
	/* mask out bits 0-4, bit 11 */
	return (((ins & 0xfffff7e0) == 0xf400c400) ? TRUE : FALSE);
}

int
ss_put_breakpoint(struct proc *p, vaddr_t va, vaddr_t *bpva, u_int *bpsave)
{
	int rc;

	/* Restore previous breakpoint if we did not trigger it. */
	if (*bpva != 0) {
		ss_put_value(p, *bpva, *bpsave);
		*bpva = 0;
	}

	/* Save instruction. */
	if ((rc = ss_get_value(p, va, bpsave)) != 0)
		return (rc);

	/* Store breakpoint instruction at the location now. */
	*bpva = va;
	return (ss_put_value(p, va, SSBREAKPOINT));
}

int
process_sstep(struct proc *p, int sstep)
{
	struct reg *sstf = USER_REGS(p);
	unsigned pc, brpc;
	unsigned instr;
	int rc;

	if (sstep == 0) {
		/* Restore previous breakpoints if any. */
		if (p->p_md.md_bp0va != 0) {
			ss_put_value(p, p->p_md.md_bp0va, p->p_md.md_bp0save);
			p->p_md.md_bp0va = 0;
		}
		if (p->p_md.md_bp1va != 0) {
			ss_put_value(p, p->p_md.md_bp1va, p->p_md.md_bp1save);
			p->p_md.md_bp1va = 0;
		}

		return (0);
	}

	/*
	 * User was stopped at pc, e.g. the instruction at pc was not executed.
	 * Fetch what's at the current location.
	 */
	pc = PC_REGS(sstf);
	if ((rc = ss_get_value(p, pc, &instr)) != 0)
		return (rc);

	/*
	 * Find if this instruction may cause a branch, and set up a breakpoint
	 * at the branch location.
	 */
	if (ss_inst_branch(instr) || inst_call(instr) || inst_return(instr) ||
	    instr == SYSCALL_INSTR) {
		brpc = ss_branch_taken(instr, pc, ss_getreg_val, sstf);

		/* self-branches are hopeless */
		if (brpc != pc && brpc != 0) {
			if ((rc = ss_put_breakpoint(p, brpc,
			    &p->p_md.md_bp1va, &p->p_md.md_bp1save)) != 0)
				return (rc);
		}
	}

	if ((rc = ss_put_breakpoint(p, pc + 4,
	    &p->p_md.md_bp0va, &p->p_md.md_bp0save)) != 0)
		return (rc);

	return (0);
}

#endif	/* PTRACE */

#ifdef DIAGNOSTIC
void
splassert_check(int wantipl, const char *func)
{
	int oldipl;

	/*
	 * This will raise the spl if too low,
	 * in a feeble attempt to reduce further damage.
	 */
	oldipl = raiseipl(wantipl);

	if (oldipl < wantipl) {
		splassert_fail(wantipl, oldipl, func);
	}
}
#endif

/*
 * ld.d and st.d instructions referencing long aligned but not long long
 * aligned addresses will trigger a misaligned address exception.
 *
 * This routine attempts to recover these (valid) statements, by simulating
 * the split form of the instruction. If it fails, it returns the appropriate
 * signal number to deliver.
 */
int
double_reg_fixup(struct trapframe *frame)
{
	u_int32_t pc, instr, value;
	int regno, store;
	vaddr_t addr;

	/*
	 * Decode the faulting instruction.
	 */

	pc = PC_REGS(&frame->tf_regs);
	if (copyin((void *)pc, &instr, sizeof(u_int32_t)) != 0)
		return SIGSEGV;

	switch (instr & 0xfc00ff00) {
	case 0xf4001000:	/* ld.d rD, rS1, rS2 */
		addr = frame->tf_r[(instr >> 16) & 0x1f]
		    + frame->tf_r[(instr & 0x1f)];
		store = 0;
		break;
	case 0xf4001200:	/* ld.d rD, rS1[rS2] */
		addr = frame->tf_r[(instr >> 16) & 0x1f]
		    + (frame->tf_r[(instr & 0x1f)] << 3);
		store = 0;
		break;
	case 0xf4002000:	/* st.d rD, rS1, rS2 */
		addr = frame->tf_r[(instr >> 16) & 0x1f]
		    + frame->tf_r[(instr & 0x1f)];
		store = 1;
		break;
	case 0xf4002200:	/* st.d rD, rS1[rS2] */
		addr = frame->tf_r[(instr >> 16) & 0x1f]
		    + (frame->tf_r[(instr & 0x1f)] << 3);
		store = 1;
		break;
	default:
		switch (instr & 0xfc000000) {
		case 0x10000000:	/* ld.d rD, rS, imm16 */
			addr = (instr & 0x0000ffff) +
			    frame->tf_r[(instr >> 16) & 0x1f];
			store = 0;
			break;
		case 0x20000000:	/* st.d rD, rS, imm16 */
			addr = (instr & 0x0000ffff) +
			    frame->tf_r[(instr >> 16) & 0x1f];
			store = 1;
			break;
		default:
			return SIGBUS;
		}
		break;
	}

	/* We only handle long but not long long aligned access here */
	if ((addr & 0x07) != 4)
		return SIGBUS;

	regno = (instr >> 21) & 0x1f;

	if (store) {
		/*
		 * Two word stores.
		 */
		value = frame->tf_r[regno++];
		if (copyout(&value, (void *)addr, sizeof(u_int32_t)) != 0)
			return SIGSEGV;
		if (regno == 32)
			value = 0;
		else
			value = frame->tf_r[regno];
		if (copyout(&value, (void *)(addr + 4), sizeof(u_int32_t)) != 0)
			return SIGSEGV;
	} else {
		/*
		 * Two word loads. r0 should be left unaltered, but the
		 * value should still be fetched even if it is discarded.
		 */
		if (copyin((void *)addr, &value, sizeof(u_int32_t)) != 0)
			return SIGSEGV;
		if (regno != 0)
			frame->tf_r[regno] = value;
		if (copyin((void *)(addr + 4), &value, sizeof(u_int32_t)) != 0)
			return SIGSEGV;
		if (regno != 31)
			frame->tf_r[regno + 1] = value;
	}

	return 0;
}

void
cache_flush(struct trapframe *tf)
{
	struct proc *p;
	struct pmap *pmap;
	u_quad_t sticks;

	if ((p = curproc) == NULL)
		p = &proc0;

	sticks = p->p_sticks;
	p->p_md.md_tf = tf;

	pmap = vm_map_pmap(&p->p_vmspace->vm_map);
	dma_cachectl(pmap, tf->tf_r[2], tf->tf_r[3], DMA_CACHE_SYNC);

	tf->tf_snip = tf->tf_snip & ~NIP_E;
	tf->tf_sfip = tf->tf_sfip & ~FIP_E;

	userret(p, tf, sticks);
}
