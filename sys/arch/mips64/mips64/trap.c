/*	$OpenBSD: trap.c,v 1.107 2015/07/19 17:00:39 visa Exp $	*/

/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1992, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department and Ralph Campbell.
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
 * from: Utah Hdr: trap.c 1.32 91/04/06
 *
 *	from: @(#)trap.c	8.5 (Berkeley) 1/11/94
 */


#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/syscall_mi.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/atomic.h>
#ifdef PTRACE
#include <sys/ptrace.h>
#endif

#include <uvm/uvm_extern.h>

#include <machine/autoconf.h>
#include <machine/cpu.h>
#include <mips64/mips_cpu.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/mips_opcode.h>
#include <machine/regnum.h>
#include <machine/trap.h>

#include <mips64/rm7000.h>

#ifdef DDB
#include <mips64/db_machdep.h>
#include <ddb/db_output.h>
#include <ddb/db_sym.h>
#endif

#include <sys/syslog.h>

#define	USERMODE(ps)	(((ps) & SR_KSU_MASK) == SR_KSU_USER)

const char *trap_type[] = {
	"external interrupt",
	"TLB modification",
	"TLB miss (load or instr. fetch)",
	"TLB miss (store)",
	"address error (load or I-fetch)",
	"address error (store)",
	"bus error (I-fetch)",
	"bus error (load or store)",
	"system call",
	"breakpoint",
	"reserved instruction",
	"coprocessor unusable",
	"arithmetic overflow",
	"trap",
	"virtual coherency instruction",
	"floating point",
	"reserved 16",
	"reserved 17",
	"reserved 18",
	"reserved 19",
	"reserved 20",
	"reserved 21",
	"reserved 22",
	"watch",
	"reserved 24",
	"reserved 25",
	"reserved 26",
	"reserved 27",
	"reserved 28",
	"reserved 29",
	"reserved 30",
	"virtual coherency data"
};

#if defined(DDB) || defined(DEBUG)
struct trapdebug trapdebug[MAXCPUS * TRAPSIZE];
uint trppos[MAXCPUS];

void	stacktrace(struct trap_frame *);
uint32_t kdbpeek(vaddr_t);
uint64_t kdbpeekd(vaddr_t);
#endif	/* DDB || DEBUG */

#if defined(DDB)
extern int kdb_trap(int, db_regs_t *);
#endif

void	ast(void);
extern void interrupt(struct trap_frame *);
void	itsa(struct trap_frame *, struct cpu_info *, struct proc *, int);
void	trap(struct trap_frame *);
#ifdef PTRACE
int	ptrace_read_insn(struct proc *, vaddr_t, uint32_t *);
int	ptrace_write_insn(struct proc *, vaddr_t, uint32_t);
int	process_sstep(struct proc *, int);
#endif

/*
 * Handle an AST for the current process.
 */
void
ast(void)
{
	struct cpu_info *ci = curcpu();
	struct proc *p = ci->ci_curproc;

	p->p_md.md_astpending = 0;

	atomic_inc_int(&uvmexp.softs);
	mi_ast(p, ci->ci_want_resched);
	userret(p);
}

/*
 * Handle an exception.
 * In the case of a kernel trap, we return the pc where to resume if
 * pcb_onfault is set, otherwise, return old pc.
 */
void
trap(struct trap_frame *trapframe)
{
	struct cpu_info *ci = curcpu();
	struct proc *p = ci->ci_curproc;
	int type;

	type = (trapframe->cause & CR_EXC_CODE) >> CR_EXC_CODE_SHIFT;

#if defined(CPU_R8000) && !defined(DEBUG_INTERRUPT)
	if (type != T_INT)
#endif
		trapdebug_enter(ci, trapframe, -1);

#ifdef CPU_R8000
	if (type != T_INT && type != T_SYSCALL)
#else
	if (type != T_SYSCALL)
#endif
		atomic_inc_int(&uvmexp.traps);
	if (USERMODE(trapframe->sr)) {
		type |= T_USER;
		refreshcreds(p);
	}

	/*
	 * Enable hardware interrupts if they were on before the trap;
	 * enable IPI interrupts only otherwise.
	 */
	switch (type) {
#ifdef CPU_R8000
	case T_INT:
	case T_INT | T_USER:
#endif
	case T_BREAK:
		break;
	default:
		if (ISSET(trapframe->sr, SR_INT_ENAB))
			enableintr();
		else {
#ifdef MULTIPROCESSOR
			ENABLEIPI();
#endif
		}
		break;
	}

#ifdef CPU_R8000
	/*
	 * Some exception causes on R8000 are actually detected by external
	 * circuitry, and as such are reported as external interrupts.
	 * On R8000 kernels, external interrupts vector to trap() instead of
	 * interrupt(), so that we can process these particular exceptions
	 * as if they were triggered as regular exceptions.
	 */
	if ((type & ~T_USER) == T_INT) {
		/*
		 * Similar reality check as done in interrupt(), in case
		 * an interrupt occured between a write to COP_0_STATUS_REG
		 * and it taking effect.
		 */
		if (!ISSET(trapframe->sr, SR_INT_ENAB))
			return;

		if (trapframe->cause & CR_VCE) {
#ifndef DEBUG_INTERRUPT
			trapdebug_enter(ci, trapframe, -1);
#endif
			panic("VCE or TLBX");
		}
		if (trapframe->cause & CR_FPE) {
#ifndef DEBUG_INTERRUPT
			trapdebug_enter(ci, trapframe, -1);
#endif
			itsa(trapframe, ci, p, T_FPE | (type & T_USER));
			cp0_reset_cause(CR_FPE);
		}
		if (trapframe->cause & CR_INT_MASK)
			interrupt(trapframe);

		return;	/* no userret */
	} else
#endif
		itsa(trapframe, ci, p, type);

	if (type & T_USER)
		userret(p);
}

/*
 * Handle a single exception.
 */
void
itsa(struct trap_frame *trapframe, struct cpu_info *ci, struct proc *p,
    int type)
{
	int i;
	unsigned ucode = 0;
	vm_prot_t ftype;
	extern vaddr_t onfault_table[];
	int onfault;
	int typ = 0;
	union sigval sv;
	struct pcb *pcb;

	switch (type) {
	case T_TLB_MOD:
		/* check for kernel address */
		if (trapframe->badvaddr < 0) {
			pt_entry_t *pte, entry;
			paddr_t pa;
			vm_page_t pg;

			pte = kvtopte(trapframe->badvaddr);
			entry = *pte;
#ifdef DIAGNOSTIC
			if (!(entry & PG_V) || (entry & PG_M))
				panic("trap: ktlbmod: invalid pte");
#endif
			if (pmap_is_page_ro(pmap_kernel(),
			    trunc_page(trapframe->badvaddr), entry)) {
				/* write to read only page in the kernel */
				ftype = PROT_WRITE;
				pcb = &p->p_addr->u_pcb;
				goto kernel_fault;
			}
			entry |= PG_M;
			*pte = entry;
			KERNEL_LOCK();
			pmap_update_kernel_page(trapframe->badvaddr & ~PGOFSET,
			    entry);
			pa = pfn_to_pad(entry);
			pg = PHYS_TO_VM_PAGE(pa);
			if (pg == NULL)
				panic("trap: ktlbmod: unmanaged page");
			pmap_set_modify(pg);
			KERNEL_UNLOCK();
			return;
		}
		/* FALLTHROUGH */

	case T_TLB_MOD+T_USER:
	    {
		pt_entry_t *pte, entry;
		paddr_t pa;
		vm_page_t pg;
		pmap_t pmap = p->p_vmspace->vm_map.pmap;

		if (!(pte = pmap_segmap(pmap, trapframe->badvaddr)))
			panic("trap: utlbmod: invalid segmap");
		pte += uvtopte(trapframe->badvaddr);
		entry = *pte;
#ifdef DIAGNOSTIC
		if (!(entry & PG_V) || (entry & PG_M))
			panic("trap: utlbmod: invalid pte");
#endif
		if (pmap_is_page_ro(pmap,
		    trunc_page(trapframe->badvaddr), entry)) {
			/* write to read only page */
			ftype = PROT_WRITE;
			pcb = &p->p_addr->u_pcb;
			goto fault_common_no_miss;
		}
		entry |= PG_M;
		*pte = entry;
		KERNEL_LOCK();
		pmap_update_user_page(pmap, (trapframe->badvaddr & ~PGOFSET), 
		    entry);
		pa = pfn_to_pad(entry);
		pg = PHYS_TO_VM_PAGE(pa);
		if (pg == NULL)
			panic("trap: utlbmod: unmanaged page");
		pmap_set_modify(pg);
		KERNEL_UNLOCK();
		return;
	    }

	case T_TLB_LD_MISS:
	case T_TLB_ST_MISS:
		ftype = (type == T_TLB_ST_MISS) ? PROT_WRITE : PROT_READ;
		pcb = &p->p_addr->u_pcb;
		/* check for kernel address */
		if (trapframe->badvaddr < 0) {
			vaddr_t va;
			int rv;

	kernel_fault:
			va = trunc_page((vaddr_t)trapframe->badvaddr);
			onfault = pcb->pcb_onfault;
			pcb->pcb_onfault = 0;
			KERNEL_LOCK();
			rv = uvm_fault(kernel_map, va, 0, ftype);
			KERNEL_UNLOCK();
			pcb->pcb_onfault = onfault;
			if (rv == 0)
				return;
			if (onfault != 0) {
				pcb->pcb_onfault = 0;
				trapframe->pc = onfault_table[onfault];
				return;
			}
			goto err;
		}
		/*
		 * It is an error for the kernel to access user space except
		 * through the copyin/copyout routines.
		 */
		if (pcb->pcb_onfault != 0) {
			/*
			 * We want to resolve the TLB fault before invoking
			 * pcb_onfault if necessary.
			 */
			goto fault_common;
		} else {
			goto err;
		}

	case T_TLB_LD_MISS+T_USER:
		ftype = PROT_READ;
		pcb = &p->p_addr->u_pcb;
		goto fault_common;

	case T_TLB_ST_MISS+T_USER:
		ftype = PROT_WRITE;
		pcb = &p->p_addr->u_pcb;
fault_common:

#ifdef CPU_R4000
		if (r4000_errata != 0) {
			if (eop_tlb_miss_handler(trapframe, ci, p) != 0)
				return;
		}
#endif

fault_common_no_miss:

#ifdef CPU_R4000
		if (r4000_errata != 0) {
			eop_cleanup(trapframe, p);
		}
#endif

	    {
		vaddr_t va;
		struct vmspace *vm;
		vm_map_t map;
		int rv;

		vm = p->p_vmspace;
		map = &vm->vm_map;
		va = trunc_page((vaddr_t)trapframe->badvaddr);

		onfault = pcb->pcb_onfault;
		pcb->pcb_onfault = 0;
		KERNEL_LOCK();

		rv = uvm_fault(map, va, 0, ftype);
		pcb->pcb_onfault = onfault;

		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if ((caddr_t)va >= vm->vm_maxsaddr) {
			if (rv == 0)
				uvm_grow(p, va);
			else if (rv == EACCES)
				rv = EFAULT;
		}
		KERNEL_UNLOCK();
		if (rv == 0)
			return;
		if (!USERMODE(trapframe->sr)) {
			if (onfault != 0) {
				pcb->pcb_onfault = 0;
				trapframe->pc =  onfault_table[onfault];
				return;
			}
			goto err;
		}

		ucode = ftype;
		i = SIGSEGV;
		typ = SEGV_MAPERR;
		break;
	    }

	case T_ADDR_ERR_LD+T_USER:	/* misaligned or kseg access */
	case T_ADDR_ERR_ST+T_USER:	/* misaligned or kseg access */
		ucode = 0;		/* XXX should be PROT_something */
		i = SIGBUS;
		typ = BUS_ADRALN;
		break;
	case T_BUS_ERR_IFETCH+T_USER:	/* BERR asserted to cpu */
	case T_BUS_ERR_LD_ST+T_USER:	/* BERR asserted to cpu */
		ucode = 0;		/* XXX should be PROT_something */
		i = SIGBUS;
		typ = BUS_OBJERR;
		break;

	case T_SYSCALL+T_USER:
	    {
		struct trap_frame *locr0 = p->p_md.md_regs;
		struct sysent *callp;
		unsigned int code;
		register_t tpc;
		int numsys, error;
		struct args {
			register_t i[8];
		} args;
		register_t rval[2];

		atomic_inc_int(&uvmexp.syscalls);

		/* compute next PC after syscall instruction */
		tpc = trapframe->pc; /* Remember if restart */
		if (trapframe->cause & CR_BR_DELAY)
			locr0->pc = MipsEmulateBranch(locr0,
			    trapframe->pc, 0, 0);
		else
			locr0->pc += 4;
		callp = p->p_p->ps_emul->e_sysent;
		numsys = p->p_p->ps_emul->e_nsysent;
		code = locr0->v0;
		switch (code) {
		case SYS_syscall:
		case SYS___syscall:
			/*
			 * Code is first argument, followed by actual args.
			 * __syscall provides the code as a quad to maintain
			 * proper alignment of 64-bit arguments on 32-bit
			 * platforms, which doesn't change anything here.
			 */
			code = locr0->a0;
			if (code >= numsys)
				callp += p->p_p->ps_emul->e_nosys; /* (illegal) */
			else
				callp += code;
			i = callp->sy_argsize / sizeof(register_t);
			args.i[0] = locr0->a1;
			args.i[1] = locr0->a2;
			args.i[2] = locr0->a3;
			if (i > 3) {
				args.i[3] = locr0->a4;
				args.i[4] = locr0->a5;
				args.i[5] = locr0->a6;
				args.i[6] = locr0->a7;
				if (i > 7)
					if ((error = copyin((void *)locr0->sp,
					    &args.i[7], sizeof(register_t))))
						goto bad;
			}
			break;
		default:
			if (code >= numsys)
				callp += p->p_p->ps_emul->e_nosys; /* (illegal) */
			else
				callp += code;

			i = callp->sy_narg;
			args.i[0] = locr0->a0;
			args.i[1] = locr0->a1;
			args.i[2] = locr0->a2;
			args.i[3] = locr0->a3;
			if (i > 4) {
				args.i[4] = locr0->a4;
				args.i[5] = locr0->a5;
				args.i[6] = locr0->a6;
				args.i[7] = locr0->a7;
			}
		}

		rval[0] = 0;
		rval[1] = locr0->v1;

#if defined(DDB) || defined(DEBUG)
		trapdebug[TRAPSIZE * ci->ci_cpuid + (trppos[ci->ci_cpuid] == 0 ?
		    TRAPSIZE : trppos[ci->ci_cpuid]) - 1].code = code;
#endif

		error = mi_syscall(p, code, callp, args.i, rval);

		switch (error) {
		case 0:
			locr0->v0 = rval[0];
			locr0->v1 = rval[1];
			locr0->a3 = 0;
			break;

		case ERESTART:
			locr0->pc = tpc;
			break;

		case EJUSTRETURN:
			break;	/* nothing to do */

		default:
		bad:
			locr0->v0 = error;
			locr0->a3 = 1;
		}

		mi_syscall_return(p, code, error, rval);

		return;
	    }

	case T_BREAK:
#ifdef DDB
		kdb_trap(type, trapframe);
#endif
		/* Reenable interrupts if necessary */
		if (trapframe->sr & SR_INT_ENAB) {
			enableintr();
		}
		return;

	case T_BREAK+T_USER:
	    {
		caddr_t va;
		u_int32_t instr;
		struct trap_frame *locr0 = p->p_md.md_regs;

		/* compute address of break instruction */
		va = (caddr_t)trapframe->pc;
		if (trapframe->cause & CR_BR_DELAY)
			va += 4;

		/* read break instruction */
		copyin(va, &instr, sizeof(int32_t));

		switch ((instr & BREAK_VAL_MASK) >> BREAK_VAL_SHIFT) {
		case 6:	/* gcc range error */
			i = SIGFPE;
			typ = FPE_FLTSUB;
			/* skip instruction */
			if (trapframe->cause & CR_BR_DELAY)
				locr0->pc = MipsEmulateBranch(locr0,
				    trapframe->pc, 0, 0);
			else
				locr0->pc += 4;
			break;
		case 7:	/* gcc3 divide by zero */
			i = SIGFPE;
			typ = FPE_INTDIV;
			/* skip instruction */
			if (trapframe->cause & CR_BR_DELAY)
				locr0->pc = MipsEmulateBranch(locr0,
				    trapframe->pc, 0, 0);
			else
				locr0->pc += 4;
			break;
#ifdef PTRACE
		case BREAK_SSTEP_VAL:
			if (p->p_md.md_ss_addr == (long)va) {
#ifdef DEBUG
				printf("trap: %s (%d): breakpoint at %p "
				    "(insn %08x)\n",
				    p->p_comm, p->p_pid,
				    (void *)p->p_md.md_ss_addr,
				    p->p_md.md_ss_instr);
#endif

				/* Restore original instruction and clear BP */
				KERNEL_LOCK();
				process_sstep(p, 0);
				KERNEL_UNLOCK();
				typ = TRAP_BRKPT;
			} else {
				typ = TRAP_TRACE;
			}
			i = SIGTRAP;
			break;
#endif
#ifdef FPUEMUL
		case BREAK_FPUEMUL_VAL:
			/*
			 * If this is a genuine FP emulation break,
			 * resume execution to our branch destination.
			 */
			if ((p->p_md.md_flags & MDP_FPUSED) != 0 &&
			    p->p_md.md_fppgva + 4 == (vaddr_t)va) {
				struct vm_map *map = &p->p_vmspace->vm_map;

				p->p_md.md_flags &= ~MDP_FPUSED;
				locr0->pc = p->p_md.md_fpbranchva;

				/*
				 * Prevent access to the relocation page.
				 * XXX needs to be fixed to work with rthreads
				 */
				uvm_fault_unwire(map, p->p_md.md_fppgva,
				    p->p_md.md_fppgva + PAGE_SIZE);
				(void)uvm_map_protect(map, p->p_md.md_fppgva,
				    p->p_md.md_fppgva + PAGE_SIZE,
				    PROT_NONE, FALSE);
				return;
			}
			/* FALLTHROUGH */
#endif
		default:
			typ = TRAP_TRACE;
			i = SIGTRAP;
			break;
		}
		break;
	    }

	case T_IWATCH+T_USER:
	case T_DWATCH+T_USER:
	    {
		caddr_t va;
		/* compute address of trapped instruction */
		va = (caddr_t)trapframe->pc;
		if (trapframe->cause & CR_BR_DELAY)
			va += 4;
		printf("watch exception @ %p\n", va);
#ifdef RM7K_PERFCNTR
		if (rm7k_watchintr(trapframe)) {
			/* Return to user, don't add any more overhead */
			return;
		}
#endif
		i = SIGTRAP;
		typ = TRAP_BRKPT;
		break;
	    }

	case T_TRAP+T_USER:
	    {
		caddr_t va;
		u_int32_t instr;
		struct trap_frame *locr0 = p->p_md.md_regs;

		/* compute address of trap instruction */
		va = (caddr_t)trapframe->pc;
		if (trapframe->cause & CR_BR_DELAY)
			va += 4;
		/* read break instruction */
		copyin(va, &instr, sizeof(int32_t));

		if (trapframe->cause & CR_BR_DELAY)
			locr0->pc = MipsEmulateBranch(locr0,
			    trapframe->pc, 0, 0);
		else
			locr0->pc += 4;
#ifdef RM7K_PERFCNTR
		if (instr == 0x040c0000) { /* Performance cntr trap */
			int result;

			result = rm7k_perfcntr(trapframe->a0, trapframe->a1,
						trapframe->a2, trapframe->a3);
			locr0->v0 = -result;
			/* Return to user, don't add any more overhead */
			return;
		} else
#endif
		/*
		 * GCC 4 uses teq with code 7 to signal divide by
	 	 * zero at runtime. This is one instruction shorter
		 * than the BEQ + BREAK combination used by gcc 3.
		 */
		if ((instr & 0xfc00003f) == 0x00000034 /* teq */ &&
		    (instr & 0x001fffc0) == ((ZERO << 16) | (7 << 6))) {
			i = SIGFPE;
			typ = FPE_INTDIV;
		} else {
			i = SIGEMT;	/* Stuff it with something for now */
			typ = 0;
		}
		break;
	    }

	case T_RES_INST+T_USER:
		i = SIGILL;
		typ = ILL_ILLOPC;
		break;

	case T_COP_UNUSABLE+T_USER:
		/*
		 * Note MIPS IV COP1X instructions issued with FPU
		 * disabled correctly report coprocessor 1 as the
		 * unusable coprocessor number.
		 */
		if ((trapframe->cause & CR_COP_ERR) != CR_COP1_ERR) {
			i = SIGILL;	/* only FPU instructions allowed */
			typ = ILL_ILLOPC;
			break;
		}
#ifdef FPUEMUL
		MipsFPTrap(trapframe);
#else
		enable_fpu(p);
#endif
		return;

	case T_FPE:
		printf("FPU Trap: PC %lx CR %lx SR %lx\n",
			trapframe->pc, trapframe->cause, trapframe->sr);
		goto err;

	case T_FPE+T_USER:
		MipsFPTrap(trapframe);
		return;

	case T_OVFLOW+T_USER:
		i = SIGFPE;
		typ = FPE_FLTOVF;
		break;

	case T_ADDR_ERR_LD:	/* misaligned access */
	case T_ADDR_ERR_ST:	/* misaligned access */
	case T_BUS_ERR_LD_ST:	/* BERR asserted to cpu */
		pcb = &p->p_addr->u_pcb;
		if ((onfault = pcb->pcb_onfault) != 0) {
			pcb->pcb_onfault = 0;
			trapframe->pc = onfault_table[onfault];
			return;
		}
		goto err;

#ifdef CPU_R10000
	case T_BUS_ERR_IFETCH:
		/*
		 * At least R16000 processor have been found triggering
		 * reproduceable bus error on instruction fetch in the
		 * kernel code, which are trivially recoverable (and
		 * look like an obscure errata to me).
		 *
		 * Thus, ignore these exceptions if the faulting address
		 * is in the kernel.
		 */
	    {
		extern void *kernel_text;
		extern void *etext;
		vaddr_t va;

		va = (vaddr_t)trapframe->pc;
		if (trapframe->cause & CR_BR_DELAY)
			va += 4;
		if (va > (vaddr_t)&kernel_text && va < (vaddr_t)&etext)
			return;
	    }
		goto err;
#endif

	default:
	err:
		disableintr();
#if !defined(DDB) && defined(DEBUG)
		trapDump("trap", printf);
#endif
		printf("\nTrap cause = %d Frame %p\n", type, trapframe);
		printf("Trap PC %p RA %p fault %p\n",
		    (void *)trapframe->pc, (void *)trapframe->ra,
		    (void *)trapframe->badvaddr);
#ifdef DDB
		stacktrace(!USERMODE(trapframe->sr) ? trapframe : p->p_md.md_regs);
		kdb_trap(type, trapframe);
#endif
		panic("trap");
	}

#ifdef FPUEMUL
	/*
	 * If a relocated delay slot causes an exception, blame the
	 * original delay slot address - userland is not supposed to
	 * know anything about emulation bowels.
	 */
	if ((p->p_md.md_flags & MDP_FPUSED) != 0 &&
	    trapframe->badvaddr == p->p_md.md_fppgva)
		trapframe->badvaddr = p->p_md.md_fpslotva;
#endif
	p->p_md.md_regs->pc = trapframe->pc;
	p->p_md.md_regs->cause = trapframe->cause;
	p->p_md.md_regs->badvaddr = trapframe->badvaddr;
	sv.sival_ptr = (void *)trapframe->badvaddr;
	KERNEL_LOCK();
	trapsignal(p, i, ucode, typ, sv);
	KERNEL_UNLOCK();
}

void
child_return(void *arg)
{
	struct proc *p = arg;
	struct trap_frame *trapframe;

	trapframe = p->p_md.md_regs;
	trapframe->v0 = 0;
	trapframe->v1 = 1;
	trapframe->a3 = 0;

	KERNEL_UNLOCK();

	mi_child_return(p);
}

#if defined(DDB) || defined(DEBUG)
void
trapDump(const char *msg, int (*pr)(const char *, ...))
{
#ifdef MULTIPROCESSOR
	CPU_INFO_ITERATOR cii;
#endif
	struct cpu_info *ci;
	struct trapdebug *base, *ptrp;
	int i;
	uint pos;
	int s;

	s = splhigh();
	(*pr)("trapDump(%s)\n", msg);
#ifndef MULTIPROCESSOR
	ci = curcpu();
#else
	CPU_INFO_FOREACH(cii, ci)
#endif
	{
#ifdef MULTIPROCESSOR
		(*pr)("cpu%d\n", ci->ci_cpuid);
#endif
		/* walk in reverse order */
		pos = trppos[ci->ci_cpuid];
		base = trapdebug + ci->ci_cpuid * TRAPSIZE;
		for (i = TRAPSIZE - 1; i >= 0; i--) {
			if (pos + i >= TRAPSIZE)
				ptrp = base + pos + i - TRAPSIZE;
			else
				ptrp = base + pos + i;

			if (ptrp->cause == 0)
				break;

#ifdef CPU_R8000
			(*pr)("%s: PC %p CR 0x%016lx SR 0x%011lx\n",
			    trap_type[(ptrp->cause & CR_EXC_CODE) >>
			      CR_EXC_CODE_SHIFT],
			    ptrp->pc, ptrp->cause, ptrp->status);
#else
			(*pr)("%s: PC %p CR 0x%08lx SR 0x%08lx\n",
			    trap_type[(ptrp->cause & CR_EXC_CODE) >>
			      CR_EXC_CODE_SHIFT],
			    ptrp->pc, ptrp->cause & 0xffffffff,
			    ptrp->status & 0xffffffff);
#endif
			(*pr)(" RA %p SP %p ADR %p\n",
			    ptrp->ra, ptrp->sp, ptrp->vadr);
		}
	}

	splx(s);
}
#endif


/*
 * Return the resulting PC as if the branch was executed.
 */
register_t
MipsEmulateBranch(struct trap_frame *tf, vaddr_t instPC, uint32_t fsr,
    uint32_t curinst)
{
	register_t *regsPtr = (register_t *)tf;
	InstFmt inst;
	vaddr_t retAddr;
	int condition;
	uint cc;

#define	GetBranchDest(InstPtr, inst) \
	    (InstPtr + 4 + ((short)inst.IType.imm << 2))

	if (curinst != 0)
		inst = *(InstFmt *)&curinst;
	else
		inst = *(InstFmt *)instPC;

	regsPtr[ZERO] = 0;	/* Make sure zero is 0x0 */

	switch ((int)inst.JType.op) {
	case OP_SPECIAL:
		switch ((int)inst.RType.func) {
		case OP_JR:
		case OP_JALR:
			retAddr = (vaddr_t)regsPtr[inst.RType.rs];
			break;
		default:
			retAddr = instPC + 4;
			break;
		}
		break;
	case OP_BCOND:
		switch ((int)inst.IType.rt) {
		case OP_BLTZ:
		case OP_BLTZL:
		case OP_BLTZAL:
		case OP_BLTZALL:
			if ((int64_t)(regsPtr[inst.RType.rs]) < 0)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;
		case OP_BGEZ:
		case OP_BGEZL:
		case OP_BGEZAL:
		case OP_BGEZALL:
			if ((int64_t)(regsPtr[inst.RType.rs]) >= 0)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;
		default:
			retAddr = instPC + 4;
			break;
		}
		break;
	case OP_J:
	case OP_JAL:
		retAddr = (inst.JType.target << 2) | (instPC & ~0x0fffffffUL);
		break;
	case OP_BEQ:
	case OP_BEQL:
		if (regsPtr[inst.RType.rs] == regsPtr[inst.RType.rt])
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;
	case OP_BNE:
	case OP_BNEL:
		if (regsPtr[inst.RType.rs] != regsPtr[inst.RType.rt])
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;
	case OP_BLEZ:
	case OP_BLEZL:
		if ((int64_t)(regsPtr[inst.RType.rs]) <= 0)
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;
	case OP_BGTZ:
	case OP_BGTZL:
		if ((int64_t)(regsPtr[inst.RType.rs]) > 0)
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;
	case OP_COP1:
		switch (inst.RType.rs) {
		case OP_BC:
			cc = (inst.RType.rt & COPz_BC_CC_MASK) >>
			    COPz_BC_CC_SHIFT;
			if ((inst.RType.rt & COPz_BC_TF_MASK) == COPz_BC_TRUE)
				condition = fsr & FPCSR_CONDVAL(cc);
			else
				condition = !(fsr & FPCSR_CONDVAL(cc));
			if (condition)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;
		default:
			retAddr = instPC + 4;
		}
		break;
	default:
		retAddr = instPC + 4;
	}

	return (register_t)retAddr;
#undef	GetBranchDest
}

#ifdef PTRACE

int
ptrace_read_insn(struct proc *p, vaddr_t va, uint32_t *insn)
{
	struct iovec iov;
	struct uio uio;

	iov.iov_base = (caddr_t)insn;
	iov.iov_len = sizeof(uint32_t);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)va;
	uio.uio_resid = sizeof(uint32_t);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = p;
	return process_domem(p, p, &uio, PT_READ_I);
}

int
ptrace_write_insn(struct proc *p, vaddr_t va, uint32_t insn)
{
	struct iovec iov;
	struct uio uio;

	iov.iov_base = (caddr_t)&insn;
	iov.iov_len = sizeof(uint32_t);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)va;
	uio.uio_resid = sizeof(uint32_t);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = p;
	return process_domem(p, p, &uio, PT_WRITE_I);
}

/*
 * This routine is called by procxmt() to single step one instruction.
 * We do this by storing a break instruction after the current instruction,
 * resuming execution, and then restoring the old instruction.
 */
int
process_sstep(struct proc *p, int sstep)
{
	struct trap_frame *locr0 = p->p_md.md_regs;
	int rc;
	uint32_t curinstr;
	vaddr_t va;

	if (sstep == 0) {
		/* clear the breakpoint */
		if (p->p_md.md_ss_addr != 0) {
			rc = ptrace_write_insn(p, p->p_md.md_ss_addr,
			    p->p_md.md_ss_instr);
#ifdef DIAGNOSTIC
			if (rc != 0)
				printf("WARNING: %s (%d): can't restore "
				    "instruction at %p: %08x\n",
				    p->p_comm, p->p_pid,
				    (void *)p->p_md.md_ss_addr,
				    p->p_md.md_ss_instr);
#endif
			p->p_md.md_ss_addr = 0;
		} else
			rc = 0;
		return rc;
	}

	/* read current instruction */
	rc = ptrace_read_insn(p, locr0->pc, &curinstr);
	if (rc != 0)
		return rc;

	/* compute next address after current location */
	if (curinstr != 0 /* nop */)
		va = (vaddr_t)MipsEmulateBranch(locr0,
		    locr0->pc, locr0->fsr, curinstr);
	else
		va = locr0->pc + 4;
#ifdef DIAGNOSTIC
	/* should not happen */
	if (p->p_md.md_ss_addr != 0) {
		printf("WARNING: %s (%d): breakpoint request "
		    "at %p, already set at %p\n",
		    p->p_comm, p->p_pid, (void *)va, (void *)p->p_md.md_ss_addr);
		return EFAULT;
	}
#endif

	/* read next instruction */
	rc = ptrace_read_insn(p, va, &p->p_md.md_ss_instr);
	if (rc != 0)
		return rc;

	/* replace with a breakpoint instruction */
	rc = ptrace_write_insn(p, va, BREAK_SSTEP);
	if (rc != 0)
		return rc;

	p->p_md.md_ss_addr = va;

#ifdef DEBUG
	printf("%s (%d): breakpoint set at %p: %08x (pc %p %08x)\n",
		p->p_comm, p->p_pid, (void *)p->p_md.md_ss_addr,
		p->p_md.md_ss_instr, (void *)locr0->pc, curinstr);
#endif
	return 0;
}

#endif /* PTRACE */

#if defined(DDB) || defined(DEBUG)
#define MIPS_JR_RA	0x03e00008	/* instruction code for jr ra */

/* forward */
#if !defined(DDB)
const char *fn_name(vaddr_t);
#endif
void stacktrace_subr(struct trap_frame *, int, int (*)(const char*, ...));

/*
 * Print a stack backtrace.
 */
void
stacktrace(struct trap_frame *regs)
{
	stacktrace_subr(regs, 6, printf);
}

#ifdef CPU_R8000
#define	VALID_ADDRESS(va) \
	(((va) >= VM_MIN_KERNEL_ADDRESS && (va) < VM_MAX_KERNEL_ADDRESS) || \
	 IS_XKPHYS(va))
#else
#define	VALID_ADDRESS(va) \
	(((va) >= VM_MIN_KERNEL_ADDRESS && (va) < VM_MAX_KERNEL_ADDRESS) || \
	 IS_XKPHYS(va) || ((va) >= CKSEG0_BASE && (va) < CKSEG1_BASE))
#endif

void
stacktrace_subr(struct trap_frame *regs, int count,
    int (*pr)(const char*, ...))
{
	vaddr_t pc, sp, ra, va, subr;
	register_t a0, a1, a2, a3;
	uint32_t instr, mask;
	InstFmt i;
	int more, stksize;
	extern char k_intr[];
	extern char k_general[];
#ifdef DDB
	db_expr_t diff;
	db_sym_t sym;
	char *symname;
#endif

	/* get initial values from the exception frame */
	sp = (vaddr_t)regs->sp;
	pc = (vaddr_t)regs->pc;
	ra = (vaddr_t)regs->ra;		/* May be a 'leaf' function */
	a0 = regs->a0;
	a1 = regs->a1;
	a2 = regs->a2;
	a3 = regs->a3;

/* Jump here when done with a frame, to start a new one */
loop:
#ifdef DDB
	symname = NULL;
#endif
	subr = 0;
	stksize = 0;

	if (count-- == 0) {
		ra = 0;
		goto end;
	}

	/* check for bad SP: could foul up next frame */
	if (sp & 3 || !VALID_ADDRESS(sp)) {
		(*pr)("SP %p: not in kernel\n", sp);
		ra = 0;
		goto end;
	}

	/* check for bad PC */
	if (pc & 3 || !VALID_ADDRESS(pc)) {
		(*pr)("PC %p: not in kernel\n", pc);
		ra = 0;
		goto end;
	}

#ifdef DDB
	/*
	 * Dig out the function from the symbol table.
	 * Watch out for function tail optimizations.
	 */
	sym = db_search_symbol(pc, DB_STGY_ANY, &diff);
	if (sym != DB_SYM_NULL && diff == 0) {
		instr = kdbpeek(pc - 2 * sizeof(int));
		i.word = instr;
		if (i.JType.op == OP_JAL) {
			sym = db_search_symbol(pc - sizeof(int),
			    DB_STGY_ANY, &diff);
			if (sym != DB_SYM_NULL && diff != 0)
				diff += sizeof(int);
		}
	}
	if (sym != DB_SYM_NULL) {
		db_symbol_values(sym, &symname, 0);
		subr = pc - (vaddr_t)diff;
	}
#endif

	/*
	 * Find the beginning of the current subroutine by scanning backwards
	 * from the current PC for the end of the previous subroutine.
	 */
	if (!subr) {
		va = pc - sizeof(int);
		while ((instr = kdbpeek(va)) != MIPS_JR_RA)
			va -= sizeof(int);
		va += 2 * sizeof(int);	/* skip back over branch & delay slot */
		/* skip over nulls which might separate .o files */
		while ((instr = kdbpeek(va)) == 0)
			va += sizeof(int);
		subr = va;
	}

	/*
	 * Jump here for locore entry points for which the preceding
	 * function doesn't end in "j ra"
	 */
	/* scan forwards to find stack size and any saved registers */
	stksize = 0;
	more = 3;
	mask = 0;
	for (va = subr; more; va += sizeof(int),
	    more = (more == 3) ? 3 : more - 1) {
		/* stop if hit our current position */
		if (va >= pc)
			break;
		instr = kdbpeek(va);
		i.word = instr;
		switch (i.JType.op) {
		case OP_SPECIAL:
			switch (i.RType.func) {
			case OP_JR:
			case OP_JALR:
				more = 2; /* stop after next instruction */
				break;

			case OP_SYSCALL:
			case OP_BREAK:
				more = 1; /* stop now */
			};
			break;

		case OP_BCOND:
		case OP_J:
		case OP_JAL:
		case OP_BEQ:
		case OP_BNE:
		case OP_BLEZ:
		case OP_BGTZ:
			more = 2; /* stop after next instruction */
			break;

		case OP_COP0:
		case OP_COP1:
		case OP_COP2:
		case OP_COP3:
			switch (i.RType.rs) {
			case OP_BC:
				more = 2; /* stop after next instruction */
			};
			break;

		case OP_SD:
			/* look for saved registers on the stack */
			if (i.IType.rs != SP)
				break;
			/* only restore the first one */
			if (mask & (1 << i.IType.rt))
				break;
			mask |= (1 << i.IType.rt);
			switch (i.IType.rt) {
			case A0:
				a0 = kdbpeekd(sp + (int16_t)i.IType.imm);
				break;
			case A1:
				a1 = kdbpeekd(sp + (int16_t)i.IType.imm);
				break;
			case A2:
				a2 = kdbpeekd(sp + (int16_t)i.IType.imm);
				break;
			case A3:
				a3 = kdbpeekd(sp + (int16_t)i.IType.imm);
				break;
			case RA:
				ra = kdbpeekd(sp + (int16_t)i.IType.imm);
				break;
			}
			break;

		case OP_DADDI:
		case OP_DADDIU:
			/* look for stack pointer adjustment */
			if (i.IType.rs != SP || i.IType.rt != SP)
				break;
			stksize = -((int16_t)i.IType.imm);
		}
	}

#ifdef DDB
	if (symname == NULL)
		(*pr)("%p ", subr);
	else
		(*pr)("%s+%p ", symname, diff);
#else
	(*pr)("%s+%p ", fn_name(subr), pc - subr);
#endif
	(*pr)("(%llx,%llx,%llx,%llx) ", a0, a1, a2, a3);
	(*pr)(" ra %p sp %p, sz %d\n", ra, sp, stksize);

	if (subr == (vaddr_t)k_intr || subr == (vaddr_t)k_general) {
		if (subr == (vaddr_t)k_general)
			(*pr)("(KERNEL TRAP)\n");
		else
			(*pr)("(KERNEL INTERRUPT)\n");
		sp = *(register_t *)sp;
		pc = ((struct trap_frame *)sp)->pc;
		ra = ((struct trap_frame *)sp)->ra;
		sp = ((struct trap_frame *)sp)->sp;
		goto loop;
	}

end:
	if (ra) {
		extern void *kernel_text;
		extern void *etext;

		if (pc == ra && stksize == 0)
			(*pr)("stacktrace: loop!\n");
		else if (ra < (vaddr_t)&kernel_text || ra > (vaddr_t)&etext)
			(*pr)("stacktrace: ra corrupted!\n");
		else {
			pc = ra;
			sp += stksize;
			ra = 0;
			goto loop;
		}
	} else {
		if (curproc)
			(*pr)("User-level: pid %d\n", curproc->p_pid);
		else
			(*pr)("User-level: curproc NULL\n");
	}
}

#undef	VALID_ADDRESS

#if !defined(DDB)
/*
 * Functions ``special'' enough to print by name
 */
#ifdef __STDC__
#define Name(_fn)  { (void*)_fn, # _fn }
#else
#define Name(_fn) { _fn, "_fn"}
#endif
static const struct { void *addr; const char *name;} names[] = {
	Name(trap),
	{ 0, NULL }
};

/*
 * Map a function address to a string name, if known; or a hex string.
 */
const char *
fn_name(vaddr_t addr)
{
	static char buf[19];
	int i = 0;

	for (i = 0; names[i].name != NULL; i++)
		if (names[i].addr == (void*)addr)
			return (names[i].name);
	snprintf(buf, sizeof(buf), "%p", addr);
	return (buf);
}
#endif	/* !DDB */

#endif /* DDB || DEBUG */

#ifdef FPUEMUL
/*
 * Set up a successful branch emulation.
 * The delay slot instruction is copied to a reserved page, followed by a
 * trap instruction to get control back, and resume at the branch
 * destination.
 */
int
fpe_branch_emulate(struct proc *p, struct trap_frame *tf, uint32_t insn,
    vaddr_t dest)
{
	struct vm_map *map = &p->p_vmspace->vm_map;
	InstFmt inst;
	int rc;

	/*
	 * Check the delay slot instruction: since it will run as a
	 * non-delay slot instruction, we want to reject branch instructions
	 * (which behaviour, when in a delay slot, is undefined anyway).
	 */

	inst = *(InstFmt *)&insn;
	rc = 0;
	switch ((int)inst.JType.op) {
	case OP_SPECIAL:
		switch ((int)inst.RType.func) {
		case OP_JR:
		case OP_JALR:
			rc = EINVAL;
			break;
		}
		break;
	case OP_BCOND:
		switch ((int)inst.IType.rt) {
		case OP_BLTZ:
		case OP_BLTZL:
		case OP_BLTZAL:
		case OP_BLTZALL:
		case OP_BGEZ:
		case OP_BGEZL:
		case OP_BGEZAL:
		case OP_BGEZALL:
			rc = EINVAL;
			break;
		}
		break;
	case OP_J:
	case OP_JAL:
	case OP_BEQ:
	case OP_BEQL:
	case OP_BNE:
	case OP_BNEL:
	case OP_BLEZ:
	case OP_BLEZL:
	case OP_BGTZ:
	case OP_BGTZL:
		rc = EINVAL;
		break;
	case OP_COP1:
		if (inst.RType.rs == OP_BC)	/* oh the irony */
			rc = EINVAL;
		break;
	}

	if (rc != 0) {
#ifdef DEBUG
		printf("%s: bogus delay slot insn %08x\n", __func__, insn);
#endif
		return rc;
	}

	/*
	 * Temporarily change protection over the page used to relocate
	 * the delay slot, and fault it in.
	 */

	rc = uvm_map_protect(map, p->p_md.md_fppgva,
	    p->p_md.md_fppgva + PAGE_SIZE, PROT_MASK, FALSE);
	if (rc != 0) {
#ifdef DEBUG
		printf("%s: uvm_map_protect on %p failed: %d\n",
		    __func__, (void *)p->p_md.md_fppgva, rc);
#endif
		return rc;
	}
	KERNEL_LOCK();
	rc = uvm_fault_wire(map, p->p_md.md_fppgva,
	    p->p_md.md_fppgva + PAGE_SIZE, PROT_MASK);
	KERNEL_UNLOCK();
	if (rc != 0) {
#ifdef DEBUG
		printf("%s: uvm_fault_wire on %p failed: %d\n",
		    __func__, (void *)p->p_md.md_fppgva, rc);
#endif
		goto err2;
	}

	rc = copyout(&insn, (void *)p->p_md.md_fppgva, sizeof insn);
	if (rc != 0) {
#ifdef DEBUG
		printf("%s: copyout %p failed %d\n",
		    __func__, (void *)p->p_md.md_fppgva, rc);
#endif
		goto err;
	}
	insn = BREAK_FPUEMUL;
	rc = copyout(&insn, (void *)(p->p_md.md_fppgva + 4), sizeof insn);
	if (rc != 0) {
#ifdef DEBUG
		printf("%s: copyout %p failed %d\n",
		    __func__, (void *)(p->p_md.md_fppgva + 4), rc);
#endif
		goto err;
	}

	(void)uvm_map_protect(map, p->p_md.md_fppgva,
	    p->p_md.md_fppgva + PAGE_SIZE, PROT_READ | PROT_EXEC, FALSE);
	p->p_md.md_fpbranchva = dest;
	p->p_md.md_fpslotva = (vaddr_t)tf->pc + 4;
	p->p_md.md_flags |= MDP_FPUSED;
	tf->pc = p->p_md.md_fppgva;
	pmap_proc_iflush(p, tf->pc, 2 * 4);

	return 0;

err:
	uvm_fault_unwire(map, p->p_md.md_fppgva, p->p_md.md_fppgva + PAGE_SIZE);
err2:
	(void)uvm_map_protect(map, p->p_md.md_fppgva,
	    p->p_md.md_fppgva + PAGE_SIZE, PROT_NONE, FALSE);
	return rc;
}
#endif
