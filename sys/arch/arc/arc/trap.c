/*	$OpenBSD: trap.c,v 1.16 1998/03/16 09:38:33 pefo Exp $	*/

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

#include "ppp.h"

#include <sys/param.h>
#include <sys/systm.h>
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
#include <machine/reg.h>
#include <machine/cpu.h>
#include <machine/pio.h>
#include <machine/intr.h>
#include <machine/autoconf.h>
#include <machine/pte.h>
#include <machine/pmap.h>
#include <machine/mips_opcode.h>
#include <machine/frame.h>

#include <vm/vm.h>
#include <vm/vm_kern.h>
#include <vm/vm_page.h>

#include <arc/pica/pica.h>
#include <mips/archtype.h>

#ifdef DDB
#include <mips/db_machdep.h>
#include <ddb/db_sym.h>
#endif

#include <sys/cdefs.h>
#include <sys/syslog.h>

struct	proc *machFPCurProcPtr;		/* pointer to last proc to use FP */

extern void MipsKernGenException __P((void));
extern void MipsUserGenException __P((void));
extern void MipsKernIntr __P((void));
extern void MipsUserIntr __P((void));
extern void MipsTLBModException __P((void));
extern void MipsTLBInvalidException __P((void));

void (*machExceptionTable[])(void) = {
/*
 * The kernel exception handlers.
 */
	MipsKernIntr,			/* external interrupt */
	MipsKernGenException,		/* TLB modification */
	MipsTLBInvalidException,	/* TLB miss (load or instr. fetch) */
	MipsTLBInvalidException,	/* TLB miss (store) */
	MipsKernGenException,		/* address error (load or I-fetch) */
	MipsKernGenException,		/* address error (store) */
	MipsKernGenException,		/* bus error (I-fetch) */
	MipsKernGenException,		/* bus error (load or store) */
	MipsKernGenException,		/* system call */
	MipsKernGenException,		/* breakpoint */
	MipsKernGenException,		/* reserved instruction */
	MipsKernGenException,		/* coprocessor unusable */
	MipsKernGenException,		/* arithmetic overflow */
	MipsKernGenException,		/* trap exception */
	MipsKernGenException,		/* viritual coherence exception inst */
	MipsKernGenException,		/* floating point exception */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* watch exception */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* reserved */
	MipsKernGenException,		/* viritual coherence exception data */
/*
 * The user exception handlers.
 */
	MipsUserIntr,			/*  0 */
	MipsUserGenException,		/*  1 */
	MipsUserGenException,		/*  2 */
	MipsUserGenException,		/*  3 */
	MipsUserGenException,		/*  4 */
	MipsUserGenException,		/*  5 */
	MipsUserGenException,		/*  6 */
	MipsUserGenException,		/*  7 */
	MipsUserGenException,		/*  8 */
	MipsUserGenException,		/*  9 */
	MipsUserGenException,		/* 10 */
	MipsUserGenException,		/* 11 */
	MipsUserGenException,		/* 12 */
	MipsUserGenException,		/* 13 */
	MipsUserGenException,		/* 14 */
	MipsUserGenException,		/* 15 */
	MipsUserGenException,		/* 16 */
	MipsUserGenException,		/* 17 */
	MipsUserGenException,		/* 18 */
	MipsUserGenException,		/* 19 */
	MipsUserGenException,		/* 20 */
	MipsUserGenException,		/* 21 */
	MipsUserGenException,		/* 22 */
	MipsUserGenException,		/* 23 */
	MipsUserGenException,		/* 24 */
	MipsUserGenException,		/* 25 */
	MipsUserGenException,		/* 26 */
	MipsUserGenException,		/* 27 */
	MipsUserGenException,		/* 28 */
	MipsUserGenException,		/* 29 */
	MipsUserGenException,		/* 20 */
	MipsUserGenException,		/* 31 */
};

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
	"viritual coherency instruction",
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
	"viritual coherency data",
};

struct {
	int	int_mask;
	int	(*int_hand)(u_int, struct clockframe *);
} cpu_int_tab[8];

int cpu_int_mask;	/* External cpu interrupt mask */

#ifdef DDB
#define TRAPSIZE	10
struct trapdebug {		/* trap history buffer for debugging */
	u_int	status;
	u_int	cause;
	u_int	vadr;
	u_int	pc;
	u_int	ra;
	u_int	sp;
	u_int	code;
} trapdebug[TRAPSIZE], *trp = trapdebug;

void trapDump __P((char *));
#endif	/* DDB */

#ifdef DEBUG	/* stack trace code, also useful for DDB one day */
void stacktrace __P((int *));
void logstacktrace __P((int *));
int  kdbpeek __P((int *));
int  kdb_trap __P((int, db_regs_t *));

/* extern functions printed by name in stack backtraces */
extern void idle __P((void));
extern void MipsTLBMiss __P((void));
#endif	/* DEBUG */

extern const struct callback *callv;
extern u_long intrcnt[];
extern void MipsSwitchFPState __P((struct proc *, int *));
extern void MipsFPTrap __P((u_int, u_int, u_int));
extern void arpintr __P((void));
extern void ipintr __P((void));
extern void pppintr __P((void));

u_int trap __P((u_int, u_int, u_int, u_int, struct trap_frame));
void interrupt __P((u_int, u_int, u_int, u_int, u_int));
void softintr __P((u_int, u_int));
int cpu_singlestep __P((struct proc *));
u_int MipsEmulateBranch __P((int *, int, int, u_int));

/*
 * Handle an exception.
 * Called from MipsKernGenException() or MipsUserGenException()
 * when a processor trap occurs.
 * In the case of a kernel trap, we return the pc where to resume if
 * ((struct pcb *)UADDR)->pcb_onfault is set, otherwise, return old pc.
 */
u_int
trap(statusReg, causeReg, vadr, pc, f)
	u_int statusReg;	/* status register at time of the exception */
	u_int causeReg;		/* cause register at time of exception */
	u_int vadr;		/* address (if any) the fault occured on */
	u_int pc;		/* program counter where to continue */
	struct trap_frame f;
{
	int type, i;
	unsigned ucode = 0;
	struct proc *p = curproc;
	u_quad_t sticks;
	vm_prot_t ftype;
	extern unsigned onfault_table[];
	int typ = 0;
	union sigval sv;

#ifdef DDB
	trp->status = statusReg;
	trp->cause = causeReg;
	trp->vadr = vadr;
	trp->pc = pc;
	trp->ra = !USERMODE(statusReg) ? f.reg[PC] :
		p->p_md.md_regs[RA];
	trp->sp = (int)&f;
	trp->code = 0;
	if (++trp == &trapdebug[TRAPSIZE])
		trp = trapdebug;
#endif

	type = (causeReg & CR_EXC_CODE) >> CR_EXC_CODE_SHIFT;
	if (USERMODE(statusReg)) {
		type |= T_USER;
		sticks = p->p_sticks;
	}

	/*
	 * Enable hardware interrupts if they were on before.
	 * We only respond to software interrupts when returning to user mode.
	 */
	if (statusReg & SR_INT_ENAB)
		splx((statusReg & HARD_INT_MASK) | SR_INT_ENAB);

	switch (type) {
	case T_TLB_MOD:
		/* check for kernel address */
		if ((int)vadr < 0) {
			pt_entry_t *pte;
			unsigned int entry;
			vm_offset_t pa;

			pte = kvtopte(vadr);
			entry = pte->pt_entry;
#ifdef DIAGNOSTIC
			if (!(entry & PG_V) || (entry & PG_M))
				panic("trap: ktlbmod: invalid pte");
#endif
			if (pmap_is_page_ro(pmap_kernel(), mips_trunc_page(vadr), entry)) {
				/* write to read only page in the kernel */
				ftype = VM_PROT_WRITE;
				goto kernel_fault;
			}
			entry |= PG_M;
			pte->pt_entry = entry;
			vadr &= ~PGOFSET;
			R4K_TLBUpdate(vadr, entry);
			pa = pfn_to_vad(entry);
#ifdef ATTR
			pmap_attributes[atop(pa)] |= PMAP_ATTR_MOD;
#else
			if (!IS_VM_PHYSADDR(pa))
				panic("trap: ktlbmod: unmanaged page");
			PHYS_TO_VM_PAGE(pa)->flags &= ~PG_CLEAN;
#endif
			return (pc);
		}
		/* FALLTHROUGH */

	case T_TLB_MOD+T_USER:
	    {
		pt_entry_t *pte;
		unsigned int entry;
		vm_offset_t pa;
		pmap_t pmap = &p->p_vmspace->vm_pmap;

		if (!(pte = pmap_segmap(pmap, vadr)))
			panic("trap: utlbmod: invalid segmap");
		pte += (vadr >> PGSHIFT) & (NPTEPG - 1);
		entry = pte->pt_entry;
#ifdef DIAGNOSTIC
		if (!(entry & PG_V) || (entry & PG_M)) {
			panic("trap: utlbmod: invalid pte");
		}
#endif
		if (pmap_is_page_ro(pmap, (vm_offset_t)mips_trunc_page(vadr), entry)) {
			/* write to read only page */
			ftype = VM_PROT_WRITE;
			goto dofault;
		}
		entry |= PG_M;
		pte->pt_entry = entry;
		vadr = (vadr & ~PGOFSET) | (pmap->pm_tlbpid << VMTLB_PID_SHIFT);
		R4K_TLBUpdate(vadr, entry);
		pa = pfn_to_vad(entry);
#ifdef ATTR
		pmap_attributes[atop(pa)] |= PMAP_ATTR_MOD;
#else
		if (!IS_VM_PHYSADDR(pa)) {
			panic("trap: utlbmod: unmanaged page");
		}
		PHYS_TO_VM_PAGE(pa)->flags &= ~PG_CLEAN;
#endif
		if (!USERMODE(statusReg))
			return (pc);
		goto out;
	    }

	case T_TLB_LD_MISS:
	case T_TLB_ST_MISS:
		ftype = (type == T_TLB_ST_MISS) ? VM_PROT_WRITE : VM_PROT_READ;
		/* check for kernel address */
		if ((int)vadr < 0) {
			vm_offset_t va;
			int rv;

		kernel_fault:
			va = trunc_page((vm_offset_t)vadr);
			rv = vm_fault(kernel_map, va, ftype, FALSE);
			if (rv == KERN_SUCCESS)
				return (pc);
			if ((i = ((struct pcb *)UADDR)->pcb_onfault) != 0) {
				((struct pcb *)UADDR)->pcb_onfault = 0;
				return (onfault_table[i]);
			}
			goto err;
		}
		/*
		 * It is an error for the kernel to access user space except
		 * through the copyin/copyout routines.
		 */
		if ((i = ((struct pcb *)UADDR)->pcb_onfault) == 0)
			goto err;
		/* check for fuswintr() or suswintr() getting a page fault */
		if (i == 4)
			return (onfault_table[i]);
		goto dofault;

	case T_TLB_LD_MISS+T_USER:
		ftype = VM_PROT_READ;
		goto dofault;

	case T_TLB_ST_MISS+T_USER:
		ftype = VM_PROT_WRITE;
	dofault:
	    {
		vm_offset_t va;
		struct vmspace *vm;
		vm_map_t map;
		int rv;

		vm = p->p_vmspace;
		map = &vm->vm_map;
		va = trunc_page((vm_offset_t)vadr);
		rv = vm_fault(map, va, ftype, FALSE);
#ifdef VMFAULT_TRACE
		printf("vm_fault(%x (pmap %x), %x (%x), %x, %d) -> %x at pc %x\n",
		       map, &vm->vm_pmap, va, vadr, ftype, FALSE, rv, pc);
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

				nss = clrnd(btoc(USRSTACK-(unsigned)va));
				if (nss > vm->vm_ssize)
					vm->vm_ssize = nss;
			} else if (rv == KERN_PROTECTION_FAILURE)
				rv = KERN_INVALID_ADDRESS;
		}
		if (rv == KERN_SUCCESS) {
			if (!USERMODE(statusReg))
				return (pc);
			goto out;
		}
		if (!USERMODE(statusReg)) {
			if ((i = ((struct pcb *)UADDR)->pcb_onfault) != 0) {
				((struct pcb *)UADDR)->pcb_onfault = 0;
				return (onfault_table[i]);
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
		ucode = 0;		/* XXX should be VM_PROT_something */
		i = SIGBUS;
		typ = BUS_ADRALN;
		break;
	case T_BUS_ERR_IFETCH+T_USER:	/* BERR asserted to cpu */
	case T_BUS_ERR_LD_ST+T_USER:	/* BERR asserted to cpu */
		ucode = 0;		/* XXX should be VM_PROT_something */
		i = SIGBUS;
		typ = BUS_OBJERR;
		break;

	case T_SYSCALL+T_USER:
	    {
		int *locr0 = p->p_md.md_regs;
		struct sysent *callp;
		unsigned int code;
		int numsys;
		struct args {
			int i[8];
		} args;
		int rval[2];

		cnt.v_syscall++;
		/* compute next PC after syscall instruction */
		if ((int)causeReg < 0) {	/* Check BD bit */
			locr0[PC] = MipsEmulateBranch(locr0, pc, 0, 0);
		}
		else {
			locr0[PC] += 4;
		}
		callp = p->p_emul->e_sysent;
		numsys = p->p_emul->e_nsysent;
		code = locr0[V0];
		switch (code) {
		case SYS_syscall:
			/*
			 * Code is first argument, followed by actual args.
			 */
			code = locr0[A0];
			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code;
			i = callp->sy_argsize / sizeof(int);
			args.i[0] = locr0[A1];
			args.i[1] = locr0[A2];
			args.i[2] = locr0[A3];
			if (i > 3) {
				i = copyin((caddr_t)(locr0[SP] +
						4 * sizeof(int)),
					(caddr_t)&args.i[3],
					(u_int)(i - 3) * sizeof(int));
				if (i) {
					locr0[V0] = i;
					locr0[A3] = 1;
#ifdef SYSCALL_DEBUG
					scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p->p_tracep, code,
							callp->sy_argsize,
							args.i);
#endif
					goto done;
				}
			}
			break;

		case SYS___syscall:
			/*
			 * Like syscall, but code is a quad, so as to maintain
			 * quad alignment for the rest of the arguments.
			 */
			code = locr0[A0 + _QUAD_LOWWORD];
			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code; 
			i = callp->sy_argsize / sizeof(int);
			args.i[0] = locr0[A2];
			args.i[1] = locr0[A3];
			if (i > 2) {
				i = copyin((caddr_t)(locr0[SP] +
						4 * sizeof(int)),
					(caddr_t)&args.i[2],
					(u_int)(i - 2) * sizeof(int));
				if (i) {
					locr0[V0] = i;
					locr0[A3] = 1;
#ifdef SYSCALL_DEBUG
					scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p->p_tracep, code,
							callp->sy_argsize,
							args.i);
#endif
					goto done;
				}
			}
			break;

		default:
			if (code >= numsys)
				callp += p->p_emul->e_nosys; /* (illegal) */
			else
				callp += code;
			i = callp->sy_narg;
			args.i[0] = locr0[A0];
			args.i[1] = locr0[A1];
			args.i[2] = locr0[A2];
			args.i[3] = locr0[A3];
			if (i > 4) {
				i = copyin((caddr_t)(locr0[SP] +
						4 * sizeof(int)),
					(caddr_t)&args.i[4],
					(u_int)(i - 4) * sizeof(int));
				if (i) {
					locr0[V0] = i;
					locr0[A3] = 1;
#ifdef SYSCALL_DEBUG
					scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p->p_tracep, code,
							callp->sy_argsize,
							args.i);
#endif
					goto done;
				}
			}
		}
#ifdef SYSCALL_DEBUG
		scdebug_call(p, code, args.i);
#endif
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSCALL))
			ktrsyscall(p->p_tracep, code, callp->sy_argsize, args.i);
#endif
		rval[0] = 0;
		rval[1] = locr0[V1];
#ifdef DDB
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
#ifdef DDB
		{ int s;
		s = splhigh();
		trp->status = statusReg;
		trp->cause = causeReg;
		trp->vadr = locr0[SP];
		trp->pc = locr0[PC];
		trp->ra = locr0[RA];
		trp->code = -code;
		if (++trp == &trapdebug[TRAPSIZE])
			trp = trapdebug;
		splx(s);
		}
#endif
		switch (i) {
		case 0:
			locr0[V0] = rval[0];
			locr0[V1] = rval[1];
			locr0[A3] = 0;
			break;

		case ERESTART:
			locr0[PC] = pc;
			break;

		case EJUSTRETURN:
			break;	/* nothing to do */

		default:
			locr0[V0] = i;
			locr0[A3] = 1;
		}
		if(code == SYS_ptrace)
			R4K_FlushCache();
	done:
#ifdef SYSCALL_DEBUG
		scdebug_ret(p, code, i, rval);
#endif
#ifdef KTRACE
		if (KTRPOINT(p, KTR_SYSRET))
			ktrsysret(p->p_tracep, code, i, rval[0]);
#endif
		goto out;
	    }

#ifdef DDB
	case T_BREAK:
		kdb_trap(type, &f);
		return(f.reg[PC]);
#endif

	case T_BREAK+T_USER:
	    {
		unsigned int va, instr;
		struct uio uio;
		struct iovec iov;

		/* compute address of break instruction */
		va = pc;
		if ((int)causeReg < 0)
			va += 4;

		/* read break instruction */
		instr = fuiword((caddr_t)va);
#if 0
		printf("trap: %s (%d) breakpoint %x at %x: (adr %x ins %x)\n",
			p->p_comm, p->p_pid, instr, pc,
			p->p_md.md_ss_addr, p->p_md.md_ss_instr); /* XXX */
#endif
		if (p->p_md.md_ss_addr != va || instr != BREAK_SSTEP) {
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
		uio.uio_offset = (off_t)va;
		uio.uio_resid = sizeof(int);
		uio.uio_segflg = UIO_SYSSPACE;
		uio.uio_rw = UIO_WRITE;
		uio.uio_procp = curproc;
		i = procfs_domem(p, p, NULL, &uio);
		R4K_FlushCache();

		if (i < 0)
			printf("Warning: can't restore instruction at %x: %x\n",
				p->p_md.md_ss_addr, p->p_md.md_ss_instr);

		p->p_md.md_ss_addr = 0;
		i = SIGTRAP;
		typ = TRAP_BRKPT;
		break;
	    }

	case T_RES_INST+T_USER:
		i = SIGILL;
		typ = ILL_ILLOPC;
		break;

	case T_COP_UNUSABLE+T_USER:
		if ((causeReg & CR_COP_ERR) != 0x10000000) {
			i = SIGILL;	/* only FPU instructions allowed */
			typ = ILL_ILLOPC;
			break;
		}
		MipsSwitchFPState(machFPCurProcPtr, p->p_md.md_regs);
		machFPCurProcPtr = p;
		p->p_md.md_regs[PS] |= SR_COP_1_BIT;
		p->p_md.md_flags |= MDP_FPUSED;
		goto out;

	case T_FPE:
#ifdef DDB
		trapDump("fpintr");
#else
		printf("FPU Trap: PC %x CR %x SR %x\n",
			pc, causeReg, statusReg);
		goto err;
#endif

	case T_FPE+T_USER:
		MipsFPTrap(statusReg, causeReg, pc);
		goto out;

	case T_OVFLOW+T_USER:
		i = SIGFPE;
		typ = FPE_FLTOVF;
		break;

	case T_ADDR_ERR_LD:	/* misaligned access */
	case T_ADDR_ERR_ST:	/* misaligned access */
	case T_BUS_ERR_LD_ST:	/* BERR asserted to cpu */
		if ((i = ((struct pcb *)UADDR)->pcb_onfault) != 0) {
			((struct pcb *)UADDR)->pcb_onfault = 0;
			return (onfault_table[i]);
		}
		/* FALLTHROUGH */

	default:
	err:
#ifdef DEBUG
		stacktrace(!USERMODE(statusReg) ? f.reg : p->p_md.md_regs);
		trapDump("trap");
#endif
		panic("trap");
	}
	p->p_md.md_regs[PC] = pc;
	p->p_md.md_regs[CAUSE] = causeReg;
	p->p_md.md_regs[BADVADDR] = vadr;
	sv.sival_int = vadr;
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
		int s;

		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we put ourselves on the run queue
		 * but before we switched, we might not be on the queue
		 * indicated by our priority.
		 */
		s = splstatclock();
		setrunqueue(p);
		p->p_stats->p_ru.ru_nivcsw++;
		mi_switch();
		splx(s);
		while ((i = CURSIG(p)) != 0)
			postsig(i);
	}

	/*
	 * If profiling, charge system time to the trapped pc.
	 */
	if (p->p_flag & P_PROFIL) {
		extern int psratio;

		addupc_task(p, pc, (int)(p->p_sticks - sticks) * psratio);
	}

	curpriority = p->p_priority;
	return (pc);
}

/*
 * Handle an interrupt.
 * Called from MipsKernIntr() or MipsUserIntr()
 * Note: curproc might be NULL.
 */
void
interrupt(statusReg, causeReg, what, pc, args)
	u_int statusReg;	/* status register at time of the exception */
	u_int causeReg;		/* cause register at time of exception */
	u_int what;
	u_int pc;		/* program counter where to continue */
	u_int args;
{
	unsigned mask;
	int i;
	struct clockframe cf;

	cnt.v_trap++;
#ifdef DDB
	trp->status = statusReg;
	trp->cause = causeReg;
	trp->vadr = 0;
	trp->pc = pc;
	trp->ra = 0;
	trp->sp = (int)&args;
	trp->code = 0;
	if (++trp == &trapdebug[TRAPSIZE])
		trp = trapdebug;
#endif

	cnt.v_intr++;
	mask = causeReg & statusReg;	/* pending interrupts & enable mask */
	cf.pc = pc;
	cf.sr = statusReg;
	cf.cr = causeReg;

	/*
	 *  Check off all enabled interrupts. Called interrupt routine
	 *  returns mask of interrupts to reenable.
	 */
	for(i = 0; i < 8; i++) {
		if(cpu_int_tab[i].int_mask & mask) {
			causeReg &= (*cpu_int_tab[i].int_hand)(mask, &cf);
		}
	}
	/*
	 *  Reenable all non served hardware levels.
	 */
	splx((statusReg & ~causeReg & HARD_INT_MASK) | SR_INT_ENAB);


	if (mask & SOFT_INT_MASK_0) {
		clearsoftclock();
		cnt.v_soft++;
		softclock();
	}
	/*
	 *  Process network interrupt if we trapped or will very soon
	 */
	if ((mask & SOFT_INT_MASK_1) ||
	    (netisr && (statusReg & SOFT_INT_MASK_1))) {
		clearsoftnet();
		cnt.v_soft++;
		intrcnt[1]++;
#ifdef INET
		if (netisr & (1 << NETISR_ARP)) {
			netisr &= ~(1 << NETISR_ARP);
			arpintr();
		}
		if (netisr & (1 << NETISR_IP)) {
			netisr &= ~(1 << NETISR_IP);
			ipintr();
		}
#endif
#ifdef NETATALK
		if (netisr & (1 << NETISR_ATALK)) {
			netisr &= ~(1 << NETISR_ATALK);
			atintr();
		}
#endif
#ifdef NS
		if (netisr & (1 << NETISR_NS)) {
			netisr &= ~(1 << NETISR_NS);
			nsintr();
		}
#endif
#ifdef ISO
		if (netisr & (1 << NETISR_ISO)) {
			netisr &= ~(1 << NETISR_ISO);
			clnlintr();
		}
#endif
#if NPPP > 0
		if(netisr & (1 << NETISR_PPP)) {
			netisr &= ~(1 << NETISR_PPP);
			pppintr();
		}
#endif
	}
	if (mask & SOFT_INT_MASK_0) {
		clearsoftclock();
		intrcnt[0]++;
		cnt.v_soft++;
		softclock();
	}
}

/*
 *	Set up handler for external interrupt events.
 *	Events are checked in priority order.
 */
void
set_intr(mask, int_hand, prio)
	int	mask;
	int	(*int_hand)(u_int, struct clockframe *);
	int	prio;
{
	if(prio >= 8)
		panic("set_intr: to high priority");

	if(cpu_int_tab[prio].int_mask != 0 &&
	   (cpu_int_tab[prio].int_mask != mask ||
	    cpu_int_tab[prio].int_hand != int_hand)) {
		panic("set_intr: int already set %x", prio);
	}

	cpu_int_tab[prio].int_hand = int_hand;
	cpu_int_tab[prio].int_mask = mask;
	cpu_int_mask |= mask >> 10;

	/*
	 *  Update external interrupt mask but don't enable clock.
	 */
	switch(system_type) {
	case ACER_PICA_61:
	case MAGNUM:
		out32(R4030_SYS_EXT_IMASK, cpu_int_mask & (~INT_MASK_4 >> 10));
		break;

	case DESKSTATION_TYNE:
		break;
	case DESKSTATION_RPC44:
		break;
	case ALGOR_P4032:
	case ALGOR_P5064:
		break;
	}
}

/*
 * This is called from MipsUserIntr() if astpending is set.
 * This is very similar to the tail of trap().
 */
void
softintr(statusReg, pc)
	unsigned statusReg;	/* status register at time of the exception */
	unsigned pc;		/* program counter where to continue */
{
	struct proc *p = curproc;
	int sig;

	cnt.v_soft++;
	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	p->p_priority = p->p_usrpri;
	astpending = 0;
	if (p->p_flag & P_OWEUPC) {
		p->p_flag &= ~P_OWEUPC;
		ADDUPROF(p);
	}
	if (want_resched) {
		int s;

		/*
		 * Since we are curproc, clock will normally just change
		 * our priority without moving us from one queue to another
		 * (since the running process is not on a queue.)
		 * If that happened after we put ourselves on the run queue
		 * but before we switched, we might not be on the queue
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
	curpriority = p->p_priority;
}

#ifdef DDB
void
trapDump(msg)
	char *msg;
{
	int i;
	int s;

	s = splhigh();
	printf("trapDump(%s)\n", msg);
	for (i = 0; i < TRAPSIZE; i++) {
		if (trp == trapdebug)
			trp = &trapdebug[TRAPSIZE - 1];
		else
			trp--;
		if (trp->cause == 0)
			break;
		printf("%s: ADR %x PC %x CR %x SR %x\n",
			trap_type[(trp->cause & CR_EXC_CODE) >>
				CR_EXC_CODE_SHIFT],
			trp->vadr, trp->pc, trp->cause, trp->status);
		printf("   RA %x SP %x code %d\n", trp->ra, trp->sp, trp->code);
	}
	splx(s);
}
#endif

#if 0
/*
 *----------------------------------------------------------------------
 *
 * MemErrorInterrupts --
 *   arc_errintr - for the ACER PICA_61
 *
 *	Handler an interrupt for the control register.
 *
 * Results:
 *	None.
 *
 * Side effects:
 *	None.
 *
 *----------------------------------------------------------------------
 */
static void
arc_errintr()
{
	volatile u_short *sysCSRPtr =
		(u_short *)PHYS_TO_UNCACHED(KN01_SYS_CSR);
	u_short csr;

	csr = *sysCSRPtr;

	if (csr & KN01_CSR_MERR) {
		printf("Memory error at 0x%x\n",
			*(unsigned *)PHYS_TO_UNCACHED(KN01_SYS_ERRADR));
		panic("Mem error interrupt");
	}
	*sysCSRPtr = (csr & ~KN01_CSR_MBZ) | 0xff;
}
#endif


/*
 * Return the resulting PC as if the branch was executed.
 */
unsigned
MipsEmulateBranch(regsPtr, instPC, fpcCSR, instptr)
	int *regsPtr;
	int instPC;
	int fpcCSR;
	u_int instptr;
{
	InstFmt inst;
	unsigned retAddr;
	int condition;

#define GetBranchDest(InstPtr, inst) \
	((unsigned)InstPtr + 4 + ((short)inst.IType.imm << 2))


	if(instptr) {
		inst = *(InstFmt *)&instptr;
	}
	else {
		inst = *(InstFmt *)instPC;
	}
#if 0
	printf("regsPtr=%x PC=%x Inst=%x fpcCsr=%x\n", regsPtr, instPC,
		inst.word, fpcCSR); /* XXX */
#endif
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

		default:
			panic("MipsEmulateBranch: Bad branch cond");
		}
		break;

	case OP_J:
	case OP_JAL:
		retAddr = (inst.JType.target << 2) | 
			((unsigned)instPC & 0xF0000000);
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
	int *locr0 = p->p_md.md_regs;
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
	uio.uio_offset = (off_t)locr0[PC];
	uio.uio_resid = sizeof(int);
	uio.uio_segflg = UIO_SYSSPACE;
	uio.uio_rw = UIO_READ;
	uio.uio_procp = curproc;
	procfs_domem(curproc, p, NULL, &uio);

	/* compute next address after current location */
	if(curinstr != 0) {
		va = MipsEmulateBranch(locr0, locr0[PC], locr0[FSR], curinstr);
	}
	else {
		va = locr0[PC] + 4;
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
	R4K_FlushCache();

	if (i < 0)
		return (EFAULT);
#if 0
	printf("SS %s (%d): breakpoint set at %x: %x (pc %x) br %x\n",
		p->p_comm, p->p_pid, p->p_md.md_ss_addr,
		p->p_md.md_ss_instr, locr0[PC], curinstr); /* XXX */
#endif
	return (0);
}

#ifdef DDB
#define MIPS_JR_RA	0x03e00008	/* instruction code for jr ra */

/* forward */
char *fn_name(unsigned addr);
void stacktrace_subr __P((int *, int (*)(const char*, ...)));

/*
 * Print a stack backtrace.
 */
void
stacktrace(regs)
	int *regs;
{
	stacktrace_subr(regs, printf);
}

void
logstacktrace(regs)
	int *regs;
{
	stacktrace_subr(regs, addlog);
}

void
stacktrace_subr(regs, printfn)
	int *regs;
	int (*printfn) __P((const char*, ...));
{
	unsigned pc, sp, fp, ra, va, subr;
	unsigned a0, a1, a2, a3;
	unsigned instr, mask;
	InstFmt i;
	int more, stksize;
	extern char edata[];
	unsigned int frames =  0;

	/* get initial values from the exception frame */
	sp = regs[SP];
	pc = regs[PC];
	fp = regs[S8];
	ra = regs[RA];		/* May be a 'leaf' function */
	a0 = regs[A0];
	a1 = regs[A1];
	a2 = regs[A2];
	a3 = regs[A3];

/* Jump here when done with a frame, to start a new one */
loop:

/* Jump here after a nonstandard (interrupt handler) frame */
specialframe:
	stksize = 0;
	subr = 0;
	if	(frames++ > 100) {
		(*printfn)("\nstackframe count exceeded\n");
		/* return breaks stackframe-size heuristics with gcc -O2 */
		goto finish;	/*XXX*/
	}

	/* check for bad SP: could foul up next frame */
	if (sp & 3 || sp < 0x80000000) {
		(*printfn)("SP 0x%x: not in kernel\n", sp);
		ra = 0;
		subr = 0;
		goto done;
	}

	/* Backtraces should contine through interrupts from kernel mode */
	if (pc >= (unsigned)MipsKernIntr && pc < (unsigned)MipsUserIntr) {
		(*printfn)("MipsKernIntr+%x: (%x, %x ,%x) -------\n",
		       pc-(unsigned)MipsKernIntr, a0, a1, a2);
		regs = (int *)(sp + STAND_ARG_SIZE);
		a0 = kdbpeek(&regs[A0]);
		a1 = kdbpeek(&regs[A1]);
		a2 = kdbpeek(&regs[A2]);
		a3 = kdbpeek(&regs[A3]);

		pc = kdbpeek(&regs[PC]); /* exc_pc - pc at time of exception */
		ra = kdbpeek(&regs[RA]); /* ra at time of exception */
		sp = kdbpeek(&regs[SP]);
		goto specialframe;
	}


# define Between(x, y, z) \
		( ((x) <= (y)) && ((y) < (z)) )
# define pcBetween(a,b) \
		Between((unsigned)a, pc, (unsigned)b)

	/*
	 * Check for current PC in  exception handler code that don't
	 * have a preceding "j ra" at the tail of the preceding function. 
	 * Depends on relative ordering of functions in locore.
	 */
	if (pcBetween(MipsKernGenException, MipsUserGenException))
		subr = (unsigned) MipsKernGenException;
	else if (pcBetween(MipsUserGenException,MipsKernIntr))
		subr = (unsigned) MipsUserGenException;
	else if (pcBetween(MipsKernIntr, MipsUserIntr))
		subr = (unsigned) MipsKernIntr;
	else if (pcBetween(MipsUserIntr, MipsTLBInvalidException))
		subr = (unsigned) MipsUserIntr;
	else if (pcBetween(splx, wbflush))
		subr = (unsigned) splx;
	else if (pcBetween(cpu_switch, fuword))
		subr = (unsigned) cpu_switch;
	else if (pcBetween(idle, cpu_switch))	{
		subr = (unsigned) idle;
		ra = 0;
		goto done;
	}
	else if (pc >= (unsigned)MipsTLBMiss && pc < (unsigned)setsoftclock) {
		(*printfn)("<<locore>>");
		goto done;
	}

	/* check for bad PC */
	if (pc & 3 || pc < 0x80000000 || pc >= (unsigned)edata) {
		(*printfn)("PC 0x%x: not in kernel\n", pc);
		ra = 0;
		goto done;
	}

	/*
	 * Find the beginning of the current subroutine by scanning backwards
	 * from the current PC for the end of the previous subroutine.
	 */
	if (!subr) {
		va = pc - sizeof(int);
		while ((instr = kdbpeek((int *)va)) != MIPS_JR_RA)
		va -= sizeof(int);
		va += 2 * sizeof(int);	/* skip back over branch & delay slot */
		/* skip over nulls which might separate .o files */
		while ((instr = kdbpeek((int *)va)) == 0)
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
		instr = kdbpeek((int *)va);
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
			/* look for saved registers on the stack */
			if (i.IType.rs != 29)
				break;
			/* only restore the first one */
			if (mask & (1 << i.IType.rt))
				break;
			mask |= (1 << i.IType.rt);
			switch (i.IType.rt) {
			case 4: /* a0 */
				a0 = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 5: /* a1 */
				a1 = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 6: /* a2 */
				a2 = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 7: /* a3 */
				a3 = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 30: /* fp */
				fp = kdbpeek((int *)(sp + (short)i.IType.imm));
				break;

			case 31: /* ra */
				ra = kdbpeek((int *)(sp + (short)i.IType.imm));
			}
			break;

		case OP_ADDI:
		case OP_ADDIU:
			/* look for stack pointer adjustment */
			if (i.IType.rs != 29 || i.IType.rt != 29)
				break;
			stksize = - ((short)i.IType.imm);
		}
	}

done:
	(*printfn)("%s+%x (%x,%x,%x,%x) ra %x sz %d\n",
		fn_name(subr), pc - subr, a0, a1, a2, a3, ra, stksize);

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
finish:
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
	Name(interrupt),
	Name(trap),
	Name(MipsKernGenException),
	Name(MipsUserGenException),
	Name(MipsKernIntr),
	Name(MipsUserIntr),
	Name(splx),
	Name(idle),
	Name(cpu_switch),
	{0, 0}
};

/*
 * Map a function address to a string name, if known; or a hex string.
 */
char *
fn_name(unsigned addr)
{
	static char buf[17];
	int i = 0;

	for (i = 0; names[i].name; i++)
		if (names[i].addr == (void*)addr)
			return (names[i].name);
	sprintf(buf, "%x", addr);
	return (buf);
}

#endif /* DDB */
