/*	$OpenBSD: trap.c,v 1.8 2004/09/17 19:19:08 miod Exp $	*/
/* tracked to 1.23 */

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

/*
 *		THIS CODE SHOULD BE REWRITTEN!
 */

#include "ppp.h"
#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/exec.h>
#include <sys/proc.h>
#include <sys/kernel.h>
#include <sys/signalvar.h>
#include <sys/syscall.h>
#include <sys/user.h>
#include <sys/buf.h>
#include <sys/device.h>
#ifdef KTRACE
#include <sys/ktrace.h>
#endif
#include <net/netisr.h>
#include <miscfs/procfs/procfs.h>

#include <machine/trap.h>
#include <machine/psl.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/intr.h>
#include <machine/autoconf.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/mips_opcode.h>
#include <machine/frame.h>
#include <machine/regnum.h>

#include <machine/rm7000.h>

#include <mips64/archtype.h>

#ifdef DDB
#include <mips64/db_machdep.h>
#include <ddb/db_sym.h>
#endif

#include <sys/cdefs.h>
#include <sys/syslog.h>

struct	proc *machFPCurProcPtr;		/* pointer to last proc to use FP */

char	*trap_type[] = {
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
	"virtual coherency data",
};

#if defined(DDB) || defined(DEBUG)
extern register_t *tlbtrcptr;
struct trapdebug trapdebug[TRAPSIZE], *trp = trapdebug;

void stacktrace(struct trap_frame *);
void logstacktrace(struct trap_frame *);
int  kdbpeek(void *);
/* extern functions printed by name in stack backtraces */
extern void idle __P((void));
#endif	/* DDB || DEBUG */

#if defined(DDB)
int  kdb_trap(int, db_regs_t *);
#endif

extern u_long intrcnt[];
extern void MipsSwitchFPState(struct proc *, struct trap_frame *);
extern void MipsSwitchFPState16(struct proc *, struct trap_frame *);
extern void MipsFPTrap(u_int, u_int, u_int);

u_int trap(struct trap_frame *);
int cpu_singlestep(struct proc *);
u_long MipsEmulateBranch(struct trap_frame *, long, int, long);

/*
 * Handle an exception.
 * In the case of a kernel trap, we return the pc where to resume if
 * pcb_onfault is set, otherwise, return old pc.
 */
unsigned
trap(trapframe)
	struct trap_frame *trapframe;
{
	int type, i;
	unsigned ucode = 0;
	struct proc *p = curproc;
	u_quad_t sticks;
	vm_prot_t ftype;
	extern vaddr_t onfault_table[];
	int typ = 0;
	union sigval sv;

	trapdebug_enter(trapframe, -1);

	type = (trapframe->cause & CR_EXC_CODE) >> CR_EXC_CODE_SHIFT;
	if (USERMODE(trapframe->sr)) {
		type |= T_USER;
		sticks = p->p_sticks;
	}

	/*
	 * Enable hardware interrupts if they were on before the trap.
	 * If it was off disable all (splhigh) so we don't accidently
	 * enable it when doing a spllower().
	 */
/*XXX do in locore? */
	if (trapframe->sr & SR_INT_ENAB) {
#ifndef IMASK_EXTERNAL
		updateimask(trapframe->cpl);
#endif
		enableintr();
	} else
		splhigh();


	switch (type) {
	case T_TLB_MOD:
		/* check for kernel address */
		if (trapframe->badvaddr < 0) {
			pt_entry_t *pte;
			unsigned int entry;
			paddr_t pa;
			vm_page_t pg;

			pte = kvtopte(trapframe->badvaddr);
			entry = pte->pt_entry;
#ifdef DIAGNOSTIC
			if (!(entry & PG_V) || (entry & PG_M))
				panic("trap: ktlbmod: invalid pte");
#endif
			if (pmap_is_page_ro(pmap_kernel(),
			    mips_trunc_page(trapframe->badvaddr), entry)) {
				/* write to read only page in the kernel */
				ftype = VM_PROT_WRITE;
				goto kernel_fault;
			}
			entry |= PG_M;
			pte->pt_entry = entry;
			trapframe->badvaddr &= ~PGOFSET;
			tlb_update(trapframe->badvaddr, entry);
			pa = pfn_to_pad(entry);
			pg = PHYS_TO_VM_PAGE(pa);
			if (pg == NULL)
				panic("trap: ktlbmod: unmanaged page");
			pmap_set_modify(pg);
			return (trapframe->pc);
		}
		/* FALLTHROUGH */

	case T_TLB_MOD+T_USER:
	    {
		pt_entry_t *pte;
		unsigned int entry;
		paddr_t pa;
		vm_page_t pg;
		pmap_t pmap = p->p_vmspace->vm_map.pmap;

		if (!(pte = pmap_segmap(pmap, trapframe->badvaddr)))
			panic("trap: utlbmod: invalid segmap");
		pte += (trapframe->badvaddr >> PGSHIFT) & (NPTEPG - 1);
		entry = pte->pt_entry;
#ifdef DIAGNOSTIC
		if (!(entry & PG_V) || (entry & PG_M))
			panic("trap: utlbmod: invalid pte");
#endif
		if (pmap_is_page_ro(pmap,
		    mips_trunc_page(trapframe->badvaddr), entry)) {
			/* write to read only page */
			ftype = VM_PROT_WRITE;
			goto dofault;
		}
		entry |= PG_M;
		pte->pt_entry = entry;
		trapframe->badvaddr = (trapframe->badvaddr & ~PGOFSET) |
		    (pmap->pm_tlbpid << VMTLB_PID_SHIFT);
		tlb_update(trapframe->badvaddr, entry);
		pa = pfn_to_pad(entry);
		pg = PHYS_TO_VM_PAGE(pa);
		if (pg == NULL)
			panic("trap: utlbmod: unmanaged page");
		pmap_set_modify(pg);
		if (!USERMODE(trapframe->sr))
			return (trapframe->pc);
		goto out;
	    }

	case T_TLB_LD_MISS:
	case T_TLB_ST_MISS:
		ftype = (type == T_TLB_ST_MISS) ? VM_PROT_WRITE : VM_PROT_READ;
		/* check for kernel address */
		if (trapframe->badvaddr < 0) {
			vaddr_t va;
			int rv;

		kernel_fault:
			va = trunc_page((vaddr_t)trapframe->badvaddr);
			rv = uvm_fault(kernel_map, trunc_page(va), 0, ftype);
			if (rv == KERN_SUCCESS)
				return (trapframe->pc);
			if ((i = p->p_addr->u_pcb.pcb_onfault) != 0) {
				p->p_addr->u_pcb.pcb_onfault = 0;
				return (onfault_table[i]);
			}
			goto err;
		}
		/*
		 * It is an error for the kernel to access user space except
		 * through the copyin/copyout routines. However we allow
		 * accesses to the top of user stack for compat emul data.
		 */
#define szsigcode ((long)(p->p_emul->e_esigcode - p->p_emul->e_sigcode))
		if (trapframe->badvaddr < VM_MAXUSER_ADDRESS &&
		    trapframe->badvaddr >= (long)STACKGAPBASE)
			goto dofault;

		if ((i = p->p_addr->u_pcb.pcb_onfault) == 0) {
			goto dofault;
		}
#undef szsigcode
		goto dofault;

	case T_TLB_LD_MISS+T_USER:
		ftype = VM_PROT_READ;
		goto dofault;

	case T_TLB_ST_MISS+T_USER:
		ftype = VM_PROT_WRITE;
	dofault:
	    {
		vaddr_t va;
		struct vmspace *vm;
		vm_map_t map;
		int rv;

		vm = p->p_vmspace;
		map = &vm->vm_map;
		va = trunc_page((vaddr_t)trapframe->badvaddr);
		rv = uvm_fault(map, trunc_page(va), 0, ftype);
#if defined(VMFAULT_TRACE)
		printf("vm_fault(%p (pmap %p), %p (%p), %x, %d) -> %x at pc %p\n",
		    map, &vm->vm_map.pmap, va, trapframe->badvaddr, ftype, FALSE, rv, trapframe->pc);
printf("sp %p\n", trapframe->sp);
#endif
		/*
		 * If this was a stack access we keep track of the maximum
		 * accessed stack size.  Also, if vm_fault gets a protection
		 * failure it is due to accessing the stack region outside
		 * the current limit and we need to reflect that as an access
		 * error.
		 */
		if ((caddr_t)va >= vm->vm_maxsaddr) {
			if (rv == KERN_SUCCESS) {
				unsigned nss;

				nss = btoc(USRSTACK-(unsigned)va);
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}
		if (rv == KERN_SUCCESS) {
			if (!USERMODE(trapframe->sr))
				return (trapframe->pc);
			goto out;
		}
		if (!USERMODE(trapframe->sr)) {
			if ((i = p->p_addr->u_pcb.pcb_onfault) != 0) {
				p->p_addr->u_pcb.pcb_onfault = 0;
				return (onfault_table[i]);
			}
			goto err;
		}

#ifdef ADEBUG
printf("SIG-SEGV @%p pc %p, ra %p\n", trapframe->badvaddr, trapframe->pc, trapframe->ra);
#endif
		ucode = ftype;
		i = SIGSEGV;
		typ = SEGV_MAPERR;
		break;
	    }

	case T_ADDR_ERR_LD+T_USER:	/* misaligned or kseg access */
	case T_ADDR_ERR_ST+T_USER:	/* misaligned or kseg access */
		ucode = 0;		/* XXX should be VM_PROT_something */
		i = SIGBUS;
		typ = BUS_ADRALN;
#ifdef ADEBUG
printf("SIG-BUSA @%p pc %p, ra %p\n", trapframe->badvaddr, trapframe->pc, trapframe->ra);
#endif
		break;
	case T_BUS_ERR_IFETCH+T_USER:	/* BERR asserted to cpu */
	case T_BUS_ERR_LD_ST+T_USER:	/* BERR asserted to cpu */
		ucode = 0;		/* XXX should be VM_PROT_something */
		i = SIGBUS;
		typ = BUS_OBJERR;
#ifdef ADEBUG
printf("SIG-BUSB @%p pc %p, ra %p\n", trapframe->badvaddr, trapframe->pc, trapframe->ra);
#endif
		break;

	case T_SYSCALL+T_USER:
	    {
		struct trap_frame *locr0 = p->p_md.md_regs;
		struct sysent *callp;
		unsigned int code;
		unsigned long tpc;
		int numsys;
		struct args {
			register_t i[8];
		} args;
		register_t rval[2];

		uvmexp.syscalls++;

		/* compute next PC after syscall instruction */
		tpc = trapframe->pc; /* Remember if restart */
		if ((int)trapframe->cause & CR_BR_DELAY) {
			locr0->pc = MipsEmulateBranch(locr0, trapframe->pc, 0, 0);
		}
		else {
			locr0->pc += 4;
		}
		callp = p->p_emul->e_sysent;
		numsys = p->p_emul->e_nsysent;
		code = locr0->v0;
		switch (code) {
		case SYS_syscall:
			/*
			 * Code is first argument, followed by actual args.
			 */
			code = locr0->a0;
			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code;
			i = callp->sy_argsize / sizeof(register_t);
			args.i[0] = locr0->a1;
			args.i[1] = locr0->a2;
			args.i[2] = locr0->a3;
			if (i > 3) {
				if (p->p_md.md_flags & MDP_O32) {
					int32_t p[5];

					i = copyin((int32_t *)locr0->sp + 4,
						p, 5 * sizeof(int32_t));
					args.i[3] = p[0];
					args.i[4] = p[1];
					args.i[5] = p[2];
					args.i[6] = p[3];
					args.i[7] = p[4];
				} else {
					args.i[3] = locr0->a4;
					args.i[4] = locr0->a5;
					args.i[5] = locr0->a6;
					args.i[6] = locr0->a7;
					i = copyin((void *)locr0->sp,
					    &args.i[7], sizeof(register_t));
				}
			}
			break;

		case SYS___syscall:
			/*
			 * Like syscall, but code is a quad, so as to maintain
			 * quad alignment for the rest of the arguments.
			 */
			if (p->p_md.md_flags & MDP_O32) {
				if (_QUAD_LOWWORD == 0) {
					code = locr0->a0;
				} else {
					code = locr0->a1;
				}
				args.i[0] = locr0->a2;
				args.i[1] = locr0->a3;
			} else {
				code = locr0->a0;
				args.i[0] = locr0->a1;
				args.i[1] = locr0->a2;
				args.i[2] = locr0->a3;
			}

			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code;
			i = callp->sy_argsize / sizeof(int);
			if (i > 2 && p->p_md.md_flags & MDP_O32) {
					int32_t p[6];

					i = copyin((int32_t *)locr0->sp + 4,
						p, 6 * sizeof(int32_t));
					args.i[2] = p[0];
					args.i[3] = p[1];
					args.i[4] = p[2];
					args.i[5] = p[3];
					args.i[6] = p[4];
					args.i[7] = p[5];
			} else if (i > 3) {
				args.i[3] = locr0->a4;
				args.i[4] = locr0->a5;
				args.i[5] = locr0->a6;
				args.i[6] = locr0->a7;
				i = copyin((void *)locr0->sp, &args.i[7],
				    sizeof(register_t));
			}
			break;

		default:
			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code;

			i = callp->sy_narg;
			args.i[0] = locr0->a0;
			args.i[1] = locr0->a1;
			args.i[2] = locr0->a2;
			args.i[3] = locr0->a3;
			if (i > 4) {
				if (p->p_md.md_flags & MDP_O32) {
					int32_t p[4];

					i = copyin((int32_t *)locr0->sp + 4,
						p, 4 * sizeof(int32_t));
					args.i[4] = p[0];
					args.i[5] = p[1];
					args.i[6] = p[2];
					args.i[7] = p[3];
				} else {
					args.i[4] = locr0->a4;
					args.i[5] = locr0->a5;
					args.i[6] = locr0->a6;
					args.i[7] = locr0->a7;
				}
			}
		}
#ifdef SYSCALL_DEBUG
		scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p, code, callp->sy_argsize, args.i);
#endif
		rval[0] = 0;
		rval[1] = locr0->v1;
#if defined(DDB) || defined(DEBUG)
		if (trp == trapdebug)
			trapdebug[TRAPSIZE - 1].code = code;
		else
			trp[-1].code = code;
#endif
		i = (*callp->sy_call)(p, &args, rval);
		/*
		 * Reinitialize proc pointer `p' as it may be different
		 * if this is a child returning from fork syscall.
		 */
		p = curproc;
		locr0 = p->p_md.md_regs;

		trapdebug_enter(locr0, -code);

		switch (i) {
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
			locr0->v0 = i;
			locr0->a3 = 1;
		}
		if (code == SYS_ptrace)
			Mips_SyncCache();
#ifdef SYSCALL_DEBUG
		scdebug_ret(p, code, i, rval);
#endif
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSRET))
			ktrsysret(p, code, i, rval[0]);
#endif
		goto out;
	    }

#ifdef DDB
	case T_BREAK:
		kdb_trap(type, trapframe);
		return(trapframe->pc);
#endif

	case T_BREAK+T_USER:
	    {
		caddr_t va;
		u_int32_t instr;
		struct uio uio;
		struct iovec iov;

		/* compute address of break instruction */
		va = (caddr_t)trapframe->pc;
		if ((int)trapframe->cause & CR_BR_DELAY)
			va += 4;

		/* read break instruction */
		copyin(&instr, va, sizeof(int32_t));
#if 0
		printf("trap: %s (%d) breakpoint %x at %x: (adr %x ins %x)\n",
			p->p_comm, p->p_pid, instr, trapframe->pc,
			p->p_md.md_ss_addr, p->p_md.md_ss_instr); /* XXX */
#endif
		if (p->p_md.md_ss_addr != (long)va || instr != BREAK_SSTEP) {
			i = SIGTRAP;
			typ = TRAP_TRACE;
			break;
		}

		/*
		 * Restore original instruction and clear BP
		 */
		iov.iov_base = (caddr_t)&p->p_md.md_ss_instr;
		iov.iov_len = sizeof(int);
		uio.uio_iov = &iov;
		uio.uio_iovcnt = 1;
		uio.uio_offset = (off_t)(long)va;
		uio.uio_resid = sizeof(int);
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw = UIO_WRITE;
		uio.uio_procp = curproc;
		i = procfs_domem(p, p, NULL, &uio);
		Mips_SyncCache();

		if (i < 0)
			printf("Warning: can't restore instruction at %x: %x\n",
				p->p_md.md_ss_addr, p->p_md.md_ss_instr);

		p->p_md.md_ss_addr = 0;
		i = SIGTRAP;
		typ = TRAP_BRKPT;
		break;
	    }

	case T_IWATCH+T_USER:
	case T_DWATCH+T_USER:
	    {
		caddr_t va;
		/* compute address of trapped instruction */
		va = (caddr_t)trapframe->pc;
		if ((int)trapframe->cause & CR_BR_DELAY)
			va += 4;
		printf("watch exception @ %p\n", va);
		if (rm7k_watchintr(trapframe)) {
			/* Return to user, don't add any more overhead */
			return (trapframe->pc);
		}
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
		if ((int)trapframe->cause & CR_BR_DELAY)
			va += 4;
		/* read break instruction */
		copyin(&instr, va, sizeof(int32_t));

		if ((int)trapframe->cause & CR_BR_DELAY) {
			locr0->pc = MipsEmulateBranch(locr0, trapframe->pc, 0, 0);
		}
		else {
			locr0->pc += 4;
		}
		if (instr == 0x040c0000) { /* Performance cntr trap */
			int result;

			result = rm7k_perfcntr(trapframe->a0, trapframe->a1,
						trapframe->a2, trapframe->a3);
			locr0->v0 = -result;
			/* Return to user, don't add any more overhead */
			return (trapframe->pc);
		}
		else {
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
		if ((trapframe->cause & CR_COP_ERR) != 0x10000000) {
			i = SIGILL;	/* only FPU instructions allowed */
			typ = ILL_ILLOPC;
			break;
		}

		if (p->p_md.md_regs->sr & SR_FR_32)
			MipsSwitchFPState(machFPCurProcPtr, p->p_md.md_regs);
		else
			MipsSwitchFPState16(machFPCurProcPtr, p->p_md.md_regs);

		machFPCurProcPtr = p;
		p->p_md.md_regs->sr |= SR_COP_1_BIT;
		p->p_md.md_flags |= MDP_FPUSED;
		goto out;

	case T_FPE:
		printf("FPU Trap: PC %x CR %x SR %x\n",
			trapframe->pc, trapframe->cause, trapframe->sr);
		goto err;

	case T_FPE+T_USER:
		MipsFPTrap(trapframe->sr, trapframe->cause, trapframe->pc);
		goto out;

	case T_OVFLOW+T_USER:
		i = SIGFPE;
		typ = FPE_FLTOVF;
		break;

	case T_ADDR_ERR_LD:	/* misaligned access */
	case T_ADDR_ERR_ST:	/* misaligned access */
	case T_BUS_ERR_LD_ST:	/* BERR asserted to cpu */
		if ((i = p->p_addr->u_pcb.pcb_onfault) != 0) {
			p->p_addr->u_pcb.pcb_onfault = 0;
			return (onfault_table[i]);
		}
		/* FALLTHROUGH */

	default:
	err:
		disableintr();
#ifndef DDB
		trapDump("trap");
#endif
		printf("\nTrap cause = %d Frame %p\n", type, trapframe);
		printf("Trap PC %p RA %p\n", trapframe->pc, trapframe->ra);
		stacktrace(!USERMODE(trapframe->sr) ? trapframe : p->p_md.md_regs);
#ifdef DDB
		kdb_trap(type, trapframe);
#endif
		panic("trap");
	}
	p->p_md.md_regs->pc = trapframe->pc;
	p->p_md.md_regs->cause = trapframe->cause;
	p->p_md.md_regs->badvaddr = trapframe->badvaddr;
	sv.sival_int = trapframe->badvaddr;
	trapsignal(p, i, ucode, typ, sv);
out:
	/*
	 * Note: we should only get here if returning to user mode.
	 */
	/* take pending signals */
	while ((i = CURSIG(p)) != 0)
		postsig(i);
	p->p_priority = p->p_usrpri;
	astpending = 0;
	if (want_resched) {
		preempt(NULL);
		while ((i = CURSIG(p)) != 0)
			postsig(i);
	}

	/*
	 * If profiling, charge system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, trapframe->pc, (int)(p->p_sticks - sticks) * psratio);
	}

	curpriority = p->p_priority;
	return (trapframe->pc);
}

void
child_return(arg)
	void *arg;
{
	struct proc *p = arg;
	struct trap_frame *trapframe;
	int i;

	trapframe = p->p_md.md_regs;
	trapframe->v0 = 0;
	trapframe->v1 = 1;
	trapframe->a3 = 0;

	/* take pending signals */
	while ((i = CURSIG(p)) != 0)
		postsig(i);
	p->p_priority = p->p_usrpri;
	astpending = 0;
	if (want_resched) {
		preempt(NULL);
		while ((i = CURSIG(p)) != 0)
			postsig(i);
	}

#if 0 /* Need sticks */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, trapframe->pc, (int)(p->p_sticks - sticks) * psratio);
	}
#endif

	curpriority = p->p_priority;

#ifdef KTRACE
	if (KTRPOINT(p, KTR_SYSRET))
		ktrsysret(p, SYS_fork, 0, 0);
#endif
}

#if defined(DDB) || defined(DEBUG)
void
trapDump(msg)
	char *msg;
{
	int i;
	int s;

	s = splhigh();
	printf("trapDump(%s)\n", msg);
	for (i = 0; i < TRAPSIZE; i++) {
		if (trp == trapdebug) {
			trp = &trapdebug[TRAPSIZE - 1];
		}
		else {
			trp--;
		}

		if (trp->cause == 0)
			break;

		printf("%s: PC %p CR 0x%x SR 0x%x\n",
		    trap_type[(trp->cause & CR_EXC_CODE) >> CR_EXC_CODE_SHIFT],
		    trp->pc, trp->cause, trp->status);

		printf("  RA %p SP %p ADR %p\n", trp->ra, trp->sp, trp->vadr);
	}

#ifdef TLBTRACE
	if (tlbtrcptr != NULL) {
		register_t *next;

		printf("tlbtrace:\n");
		next = tlbtrcptr;
		do {
			if (next[0] != NULL) {
				printf("pc %p, va %p segtab %p pte %p\n",
				    next[0], next[1], next[2], next[3]);
			}
			next +=  4;
			next = (register_t *)((long)next & ~0x100);
		} while (next != tlbtrcptr);
	}
#endif

	splx(s);
}
#endif


/*
 * Return the resulting PC as if the branch was executed.
 */
unsigned long
MipsEmulateBranch(framePtr, instPC, fpcCSR, instptr)
	struct trap_frame *framePtr;
	long instPC;
	int fpcCSR;
	long instptr;
{
	InstFmt inst;
	unsigned long retAddr;
	int condition;
	register_t *regsPtr = (register_t *)framePtr;

#define GetBranchDest(InstPtr, inst) \
	((unsigned long)InstPtr + 4 + ((short)inst.IType.imm << 2))


	if (instptr) {
		inst = *(InstFmt *)&instptr;
	}
	else {
		inst = *(InstFmt *)instPC;
	}
#if 0
	printf("regsPtr=%x PC=%x Inst=%x fpcCsr=%x\n", regsPtr, instPC,
		inst.word, fpcCSR); /* XXX */
#endif
	regsPtr[ZERO] = 0;	/* Make sure zero is 0x0 */

	switch ((int)inst.JType.op) {
	case OP_SPECIAL:
		switch ((int)inst.RType.func) {
		case OP_JR:
		case OP_JALR:
			retAddr = regsPtr[inst.RType.rs];
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
			if ((int)(regsPtr[inst.RType.rs]) < 0)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;

		case OP_BGEZ:
		case OP_BGEZL:
		case OP_BGEZAL:
		case OP_BGEZALL:
			if ((int)(regsPtr[inst.RType.rs]) >= 0)
				retAddr = GetBranchDest(instPC, inst);
			else
				retAddr = instPC + 8;
			break;

		case OP_TGEI:
		case OP_TGEIU:
		case OP_TLTI:
		case OP_TLTIU:
		case OP_TEQI:
		case OP_TNEI:
			retAddr = instPC + 4;	/* Like syscall... */
			break;

		default:
			panic("MipsEmulateBranch: Bad branch cond");
		}
		break;

	case OP_J:
	case OP_JAL:
		retAddr = (inst.JType.target << 2) | (instPC & ~0x0fffffff);
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
		if ((int)(regsPtr[inst.RType.rs]) <= 0)
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_BGTZ:
	case OP_BGTZL:
		if ((int)(regsPtr[inst.RType.rs]) > 0)
			retAddr = GetBranchDest(instPC, inst);
		else
			retAddr = instPC + 8;
		break;

	case OP_COP1:
		switch (inst.RType.rs) {
		case OP_BCx:
		case OP_BCy:
			if ((inst.RType.rt & COPz_BC_TF_MASK) == COPz_BC_TRUE)
				condition = fpcCSR & FPC_COND_BIT;
			else
				condition = !(fpcCSR & FPC_COND_BIT);
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
	return (retAddr);
}

/*
 * This routine is called by procxmt() to single step one instruction.
 * We do this by storing a break instruction after the current instruction,
 * resuming execution, and then restoring the old instruction.
 */
int
cpu_singlestep(p)
	struct proc *p;
{
	unsigned va;
	struct trap_frame *locr0 = p->p_md.md_regs;
	int i;
	int bpinstr = BREAK_SSTEP;
	int curinstr;
	struct uio uio;
	struct iovec iov;

	/*
	 * Fetch what's at the current location.
	 */
	iov.iov_base = (caddr_t)&curinstr;
	iov.iov_len = sizeof(int);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)locr0->pc;
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = curproc;
	procfs_domem(curproc, p, NULL, &uio);

	/* compute next address after current location */
	if (curinstr != 0) {
		va = MipsEmulateBranch(locr0, locr0->pc, locr0->fsr, curinstr);
	}
	else {
		va = locr0->pc + 4;
	}
	if (p->p_md.md_ss_addr) {
		printf("SS %s (%d): breakpoint already set at %x (va %x)\n",
			p->p_comm, p->p_pid, p->p_md.md_ss_addr, va); /* XXX */
		return (EFAULT);
	}
	p->p_md.md_ss_addr = va;
	/*
	 * Fetch what's at the current location.
	 */
	iov.iov_base = (caddr_t)&p->p_md.md_ss_instr;
	iov.iov_len = sizeof(int);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)va;
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = curproc;
	procfs_domem(curproc, p, NULL, &uio);

	/*
	 * Store breakpoint instruction at the "next" location now.
	 */
	iov.iov_base = (caddr_t)&bpinstr;
	iov.iov_len = sizeof(int);
	uio.uio_iov = &iov;
	uio.uio_iovcnt = 1;
	uio.uio_offset = (off_t)va;
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_WRITE;
	uio.uio_procp = curproc;
	i = procfs_domem(curproc, p, NULL, &uio);
	Mips_SyncCache();

	if (i < 0)
		return (EFAULT);
#if 0
	printf("SS %s (%d): breakpoint set at %x: %x (pc %x) br %x\n",
		p->p_comm, p->p_pid, p->p_md.md_ss_addr,
		p->p_md.md_ss_instr, locr0[PC], curinstr); /* XXX */
#endif
	return (0);
}

#if defined(DDB) || defined(DEBUG)
#define MIPS_JR_RA	0x03e00008	/* instruction code for jr ra */

/* forward */
char *fn_name(long addr);
void stacktrace_subr __P((struct trap_frame *, int (*)(const char*, ...)));

/*
 * Print a stack backtrace.
 */
void
stacktrace(regs)
	struct trap_frame *regs;
{
	stacktrace_subr(regs, printf);
}

void
logstacktrace(regs)
	struct trap_frame *regs;
{
	stacktrace_subr(regs, addlog);
}

void
stacktrace_subr(regs, printfn)
	struct trap_frame *regs;
	int (*printfn) __P((const char*, ...));
{
	long pc, sp, fp, ra, va, subr;
	long a0, a1, a2, a3;
	unsigned instr, mask;
	InstFmt i;
	int more, stksize;
	extern char edata[];
	unsigned int frames =  0;

	/* get initial values from the exception frame */
	sp = regs->sp;
	pc = regs->pc;
	fp = regs->s8;
	ra = regs->ra;		/* May be a 'leaf' function */
	a0 = regs->a0;
	a1 = regs->a1;
	a2 = regs->a2;
	a3 = regs->a3;

/* Jump here when done with a frame, to start a new one */
loop:

/* Jump here after a nonstandard (interrupt handler) frame */
	stksize = 0;
	subr = 0;
	if	(frames++ > 6) {
		(*printfn)("stackframe count exceeded\n");
		return;
	}

	/* check for bad SP: could foul up next frame */
	if (sp & 3 || sp < KSEG0_BASE) {
		(*printfn)("SP %p: not in kernel\n", sp);
		ra = 0;
		subr = 0;
		goto done;
	}

#if 0
	/* Backtraces should contine through interrupts from kernel mode */
	if (pc >= (unsigned)MipsKernIntr && pc < (unsigned)MipsUserIntr) {
		(*printfn)("MipsKernIntr+%x: (%x, %x ,%x) -------\n",
		       pc-(unsigned)MipsKernIntr, a0, a1, a2);
		regs = (struct trap_frame *)(sp + STAND_ARG_SIZE);
		a0 = kdbpeek(&regs->a0);
		a1 = kdbpeek(&regs->a1);
		a2 = kdbpeek(&regs->a2);
		a3 = kdbpeek(&regs->a3);

		pc = kdbpeek(&regs->pc); /* exc_pc - pc at time of exception */
		ra = kdbpeek(&regs->ra); /* ra at time of exception */
		sp = kdbpeek(&regs->sp);
		goto specialframe;
	}
#endif


# define Between(x, y, z) \
		( ((x) <= (y)) && ((y) < (z)) )
# define pcBetween(a,b) \
		Between((unsigned)a, pc, (unsigned)b)

	/* check for bad PC */
	if (pc & 3 || pc < KSEG0_BASE || pc >= (unsigned)edata) {
		(*printfn)("PC %p: not in kernel\n", pc);
		ra = 0;
		goto done;
	}

	/*
	 * Find the beginning of the current subroutine by scanning backwards
	 * from the current PC for the end of the previous subroutine.
	 */
	if (!subr) {
		va = pc - sizeof(int);
		while ((instr = kdbpeek((void *)va)) != MIPS_JR_RA)
		va -= sizeof(int);
		va += 2 * sizeof(int);	/* skip back over branch & delay slot */
		/* skip over nulls which might separate .o files */
		while ((instr = kdbpeek((void *)va)) == 0)
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
		instr = kdbpeek((void *)va);
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
			case OP_BCx:
			case OP_BCy:
				more = 2; /* stop after next instruction */
			};
			break;

		case OP_SW:
		case OP_SD:
			/* look for saved registers on the stack */
			if (i.IType.rs != 29)
				break;
			/* only restore the first one */
			if (mask & (1 << i.IType.rt))
				break;
			mask |= (1 << i.IType.rt);
			switch (i.IType.rt) {
			case 4: /* a0 */
				a0 = kdbpeek((void *)(sp + (short)i.IType.imm));
				break;

			case 5: /* a1 */
				a1 = kdbpeek((void *)(sp + (short)i.IType.imm));
				break;

			case 6: /* a2 */
				a2 = kdbpeek((void *)(sp + (short)i.IType.imm));
				break;

			case 7: /* a3 */
				a3 = kdbpeek((void *)(sp + (short)i.IType.imm));
				break;

			case 30: /* fp */
				fp = kdbpeek((void *)(sp + (short)i.IType.imm));
				break;

			case 31: /* ra */
				ra = kdbpeek((void *)(sp + (short)i.IType.imm));
			}
			break;

		case OP_ADDI:
		case OP_ADDIU:
		case OP_DADDI:
		case OP_DADDIU:
			/* look for stack pointer adjustment */
			if (i.IType.rs != 29 || i.IType.rt != 29)
				break;
			stksize = - ((short)i.IType.imm);
		}
	}

done:
	(*printfn)("%s+%x ra %p sp %p (%p,%p,%p,%p)\n",
		fn_name(subr), pc - subr, ra, sp, a0, a1, a2, a3);
#if defined(_LP64)
	a0 = a1 = a2 = a3 = 0x00dead0000dead00;
#else
	a0 = a1 = a2 = a3 = 0x00dead00;
#endif

	if (ra) {
		if (pc == ra && stksize == 0)
			(*printfn)("stacktrace: loop!\n");
		else {
			pc = ra;
			sp += stksize;
			ra = 0;
			goto loop;
		}
	} else {
		if (curproc)
			(*printfn)("User-level: pid %d\n", curproc->p_pid);
		else
			(*printfn)("User-level: curproc NULL\n");
	}
}

/*
 * Functions ``special'' enough to print by name
 */
#ifdef __STDC__
#define Name(_fn)  { (void*)_fn, # _fn }
#else
#define Name(_fn) { _fn, "_fn"}
#endif
static struct { void *addr; char *name;} names[] = {
	Name(trap),
	{0, 0}
};

/*
 * Map a function address to a string name, if known; or a hex string.
 */
char *
fn_name(long addr)
{
	static char buf[17];
	int i = 0;

	for (i = 0; names[i].name; i++)
		if (names[i].addr == (void*)addr)
			return (names[i].name);
	snprintf(buf, sizeof(buf), "%x", addr);
	return (buf);
}

#endif /* DDB */
