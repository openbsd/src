/*	$OpenBSD: trap.c,v 1.85 2011/07/06 21:41:37 art Exp $	*/
/*	$NetBSD: trap.c,v 1.3 1996/10/13 03:31:37 christos Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <sys/systm.h>
#include <sys/user.h>
#include <sys/ktrace.h>
#include <sys/pool.h>

#include <dev/cons.h>

#include <machine/cpu.h>
#include <machine/fpu.h>
#include <machine/frame.h>
#include <machine/pcb.h>
#include <machine/pmap.h>
#include <machine/psl.h>
#include <machine/trap.h>
#include <machine/db_machdep.h>

#include "systrace.h"
#include <dev/systrace.h>

#include <uvm/uvm_extern.h>

#include <ddb/db_extern.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>

static int fix_unaligned(struct proc *p, struct trapframe *frame);
int badaddr(char *addr, u_int32_t len);
static __inline void userret(struct proc *);
void trap(struct trapframe *frame);

/* These definitions should probably be somewhere else				XXX */
#define	FIRSTARG	3		/* first argument is in reg 3 */
#define	NARGREG		8		/* 8 args are in registers */
#define	MOREARGS(sp)	((caddr_t)((int)(sp) + 8)) /* more args go here */

#ifdef DDB
void ppc_dumpbt(struct trapframe *frame);

void
ppc_dumpbt(struct trapframe *frame)
{
	u_int32_t addr;
	/* dumpframe is defined in db_trace.c */
	addr=frame->fixreg[1];
	while (addr != 0)
		addr = db_dumpframe(addr, db_printf);
	return;
}
#endif

#ifdef ALTIVEC
/*
 * Save state of the vector processor, This is done lazily in the hope
 * that few processes in the system will be using the vector unit
 * and that the exception time taken to switch them will be less than
 * the necessary time to save the vector on every context switch.
 *
 * Also note that in this version, the VRSAVE register is saved with
 * the state of the current process holding the vector processor,
 * and the contents of that register are not used to optimize the save.
 *
 * This can lead to VRSAVE corruption, data passing between processes,
 * because this register is accessable without the MSR[VEC] bit set.
 * To store/restore this cleanly a processor identifier bit would need
 * to be saved and this register saved on every context switch.
 * Since we do not use the information, we may be able to get by
 * with not saving it rigorously.
 */
void
save_vec(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct vreg *pcb_vr = pcb->pcb_vr;
	u_int32_t oldmsr, msr;

	if (p == NULL)
		return;

	/* first we enable vector so that we dont throw an exception
	 * in kernel mode
	 */
	oldmsr = ppc_mfmsr();
	msr = (oldmsr & ~PSL_EE) | PSL_VEC;
	ppc_mtmsr(msr);
	__asm__ volatile ("sync;isync");

	pcb->pcb_vr->vrsave = ppc_mfvrsave();

#define STR(x) #x
#define SAVE_VEC_REG(reg, addr)   \
	__asm__ volatile ("stvxl %0, 0, %1" :: "n"(reg),"r" (addr));

	SAVE_VEC_REG(0,&pcb_vr->vreg[0]);
	SAVE_VEC_REG(1,&pcb_vr->vreg[1]);
	SAVE_VEC_REG(2,&pcb_vr->vreg[2]);
	SAVE_VEC_REG(3,&pcb_vr->vreg[3]);
	SAVE_VEC_REG(4,&pcb_vr->vreg[4]);
	SAVE_VEC_REG(5,&pcb_vr->vreg[5]);
	SAVE_VEC_REG(6,&pcb_vr->vreg[6]);
	SAVE_VEC_REG(7,&pcb_vr->vreg[7]);
	SAVE_VEC_REG(8,&pcb_vr->vreg[8]);
	SAVE_VEC_REG(9,&pcb_vr->vreg[9]);
	SAVE_VEC_REG(10,&pcb_vr->vreg[10]);
	SAVE_VEC_REG(11,&pcb_vr->vreg[11]);
	SAVE_VEC_REG(12,&pcb_vr->vreg[12]);
	SAVE_VEC_REG(13,&pcb_vr->vreg[13]);
	SAVE_VEC_REG(14,&pcb_vr->vreg[14]);
	SAVE_VEC_REG(15,&pcb_vr->vreg[15]);
	SAVE_VEC_REG(16,&pcb_vr->vreg[16]);
	SAVE_VEC_REG(17,&pcb_vr->vreg[17]);
	SAVE_VEC_REG(18,&pcb_vr->vreg[18]);
	SAVE_VEC_REG(19,&pcb_vr->vreg[19]);
	SAVE_VEC_REG(20,&pcb_vr->vreg[20]);
	SAVE_VEC_REG(21,&pcb_vr->vreg[21]);
	SAVE_VEC_REG(22,&pcb_vr->vreg[22]);
	SAVE_VEC_REG(23,&pcb_vr->vreg[23]);
	SAVE_VEC_REG(24,&pcb_vr->vreg[24]);
	SAVE_VEC_REG(25,&pcb_vr->vreg[25]);
	SAVE_VEC_REG(26,&pcb_vr->vreg[26]);
	SAVE_VEC_REG(27,&pcb_vr->vreg[27]);
	SAVE_VEC_REG(28,&pcb_vr->vreg[28]);
	SAVE_VEC_REG(29,&pcb_vr->vreg[29]);
	SAVE_VEC_REG(30,&pcb_vr->vreg[30]);
	SAVE_VEC_REG(31,&pcb_vr->vreg[31]);
	__asm__ volatile ("mfvscr 0");
	SAVE_VEC_REG(0,&pcb_vr->vscr);

	curcpu()->ci_vecproc = NULL;
	pcb->pcb_veccpu = NULL;

	/* fix kernel msr back */
	ppc_mtmsr(oldmsr);
}

/*
 * Copy the context of a given process into the vector registers.
 */
void
enable_vec(struct proc *p)
{
	struct pcb *pcb = &p->p_addr->u_pcb;
	struct vreg *pcb_vr = pcb->pcb_vr;
	struct cpu_info *ci = curcpu();
	u_int32_t oldmsr, msr;

	/* If this is the very first altivec instruction executed
	 * by this process, create a context.
	 */
	if (pcb->pcb_vr == NULL)
		pcb->pcb_vr = pool_get(&ppc_vecpl, PR_WAITOK | PR_ZERO);

	if (curcpu()->ci_vecproc != NULL || pcb->pcb_veccpu != NULL)
		printf("attempting to restore vector in use vecproc %x"
		    " veccpu %x\n", curcpu()->ci_vecproc, pcb->pcb_veccpu);

	/* first we enable vector so that we dont throw an exception
	 * in kernel mode
	 */
	oldmsr = ppc_mfmsr();
	msr = (oldmsr & ~PSL_EE) | PSL_VEC;
	ppc_mtmsr(msr);
	__asm__ volatile ("sync;isync");
	ci->ci_vecproc = p;
	pcb->pcb_veccpu = ci;

#define LOAD_VEC_REG(reg, addr)   \
	__asm__ volatile ("lvxl %0, 0, %1" :: "n"(reg), "r" (addr));

	LOAD_VEC_REG(0, &pcb_vr->vscr);
	__asm__ volatile ("mtvscr 0");
	ppc_mtvrsave(pcb_vr->vrsave);

	LOAD_VEC_REG(0, &pcb_vr->vreg[0]);
	LOAD_VEC_REG(1, &pcb_vr->vreg[1]);
	LOAD_VEC_REG(2, &pcb_vr->vreg[2]);
	LOAD_VEC_REG(3, &pcb_vr->vreg[3]);
	LOAD_VEC_REG(4, &pcb_vr->vreg[4]);
	LOAD_VEC_REG(5, &pcb_vr->vreg[5]);
	LOAD_VEC_REG(6, &pcb_vr->vreg[6]);
	LOAD_VEC_REG(7, &pcb_vr->vreg[7]);
	LOAD_VEC_REG(8, &pcb_vr->vreg[8]);
	LOAD_VEC_REG(9, &pcb_vr->vreg[9]);
	LOAD_VEC_REG(10, &pcb_vr->vreg[10]);
	LOAD_VEC_REG(11, &pcb_vr->vreg[11]);
	LOAD_VEC_REG(12, &pcb_vr->vreg[12]);
	LOAD_VEC_REG(13, &pcb_vr->vreg[13]);
	LOAD_VEC_REG(14, &pcb_vr->vreg[14]);
	LOAD_VEC_REG(15, &pcb_vr->vreg[15]);
	LOAD_VEC_REG(16, &pcb_vr->vreg[16]);
	LOAD_VEC_REG(17, &pcb_vr->vreg[17]);
	LOAD_VEC_REG(18, &pcb_vr->vreg[18]);
	LOAD_VEC_REG(19, &pcb_vr->vreg[19]);
	LOAD_VEC_REG(20, &pcb_vr->vreg[20]);
	LOAD_VEC_REG(21, &pcb_vr->vreg[21]);
	LOAD_VEC_REG(22, &pcb_vr->vreg[22]);
	LOAD_VEC_REG(23, &pcb_vr->vreg[23]);
	LOAD_VEC_REG(24, &pcb_vr->vreg[24]);
	LOAD_VEC_REG(25, &pcb_vr->vreg[25]);
	LOAD_VEC_REG(26, &pcb_vr->vreg[26]);
	LOAD_VEC_REG(27, &pcb_vr->vreg[27]);
	LOAD_VEC_REG(28, &pcb_vr->vreg[28]);
	LOAD_VEC_REG(29, &pcb_vr->vreg[29]);
	LOAD_VEC_REG(30, &pcb_vr->vreg[30]);
	LOAD_VEC_REG(31, &pcb_vr->vreg[31]);

	/* fix kernel msr back */
	ppc_mtmsr(oldmsr);
}
#endif /* ALTIVEC */

static __inline void
userret(struct proc *p)
{
	int sig;

	/* take pending signals */
	while ((sig = CURSIG(p)) != 0)
		postsig(sig);
	curcpu()->ci_schedstate.spc_curpriority = p->p_priority;
}

void
trap(struct trapframe *frame)
{
	struct cpu_info *ci = curcpu();
	struct proc *p = curproc;
	int type = frame->exc;
	union sigval sv;
	char *name;
	db_expr_t offset;

	if (frame->srr1 & PSL_PR) {
		type |= EXC_USER;
	}

	switch (type) {
	case EXC_TRC|EXC_USER:		
		{
			sv.sival_int = frame->srr0;
			KERNEL_LOCK();
			trapsignal(p, SIGTRAP, type, TRAP_TRACE, sv);
			KERNEL_UNLOCK();
		}
		break;

	case EXC_MCHK:
		{
			faultbuf *fb;

			if ((fb = p->p_addr->u_pcb.pcb_onfault)) {
				p->p_addr->u_pcb.pcb_onfault = 0;
				frame->srr0 = fb->pc;		/* PC */
				frame->srr1 = fb->sr;		/* SR */
				frame->fixreg[1] = fb->sp;	/* SP */
				frame->fixreg[3] = 1;		/* != 0 */
				frame->cr = fb->cr;
				bcopy(&fb->regs[0], &frame->fixreg[13], 19*4);
				return;
			}
		}
		goto brain_damage;

	case EXC_DSI:
		{
			struct vm_map *map;
			vaddr_t va;
			int ftype;
			faultbuf *fb;
			
			map = kernel_map;
			va = frame->dar;
			if ((va >> ADDR_SR_SHIFT) == PPC_USER_SR) {
				sr_t user_sr;
				
				asm ("mfsr %0, %1"
				     : "=r"(user_sr) : "K"(PPC_USER_SR));
				va &= ADDR_PIDX | ADDR_POFF;
				va |= user_sr << ADDR_SR_SHIFT;
				map = &p->p_vmspace->vm_map;
				if (pte_spill_v(map->pmap, va, frame->dsisr, 0))
					return;
			}
			if (frame->dsisr & DSISR_STORE)
				ftype = VM_PROT_READ | VM_PROT_WRITE;
			else
				ftype = VM_PROT_READ;
			KERNEL_LOCK();
			if (uvm_fault(map, trunc_page(va), 0, ftype) == 0) {
				KERNEL_UNLOCK();
				return;
			}
			KERNEL_UNLOCK();

			if ((fb = p->p_addr->u_pcb.pcb_onfault)) {
				p->p_addr->u_pcb.pcb_onfault = 0;
				frame->srr0 = fb->pc;		/* PC */
				frame->fixreg[1] = fb->sp;	/* SP */
				frame->fixreg[3] = 1;		/* != 0 */
				frame->cr = fb->cr;
				bcopy(&fb->regs[0], &frame->fixreg[13], 19*4);
				return;
			}
			map = kernel_map;
		}
printf("kern dsi on addr %x iar %x\n", frame->dar, frame->srr0);
		goto brain_damage;
	case EXC_DSI|EXC_USER:
		{
			int ftype, vftype;
			
			/* Try spill handler first */
			if (pte_spill_v(p->p_vmspace->vm_map.pmap,
			    frame->dar, frame->dsisr, 0))
				break;

			KERNEL_LOCK();
			if (frame->dsisr & DSISR_STORE) {
				ftype = VM_PROT_READ | VM_PROT_WRITE;
				vftype = VM_PROT_WRITE;
			} else
				vftype = ftype = VM_PROT_READ;
			if (uvm_fault(&p->p_vmspace->vm_map,
				     trunc_page(frame->dar), 0, ftype) == 0) {
				uvm_grow(p, trunc_page(frame->dar));
				KERNEL_UNLOCK();
				break;
			}

#if 0
printf("dsi on addr %x iar %x lr %x\n", frame->dar, frame->srr0,frame->lr);
#endif
/*
 * keep this for later in case we want it later.
*/
			sv.sival_int = frame->dar;
			trapsignal(p, SIGSEGV, vftype, SEGV_MAPERR, sv);
			KERNEL_UNLOCK();
		}
		break;
	case EXC_ISI|EXC_USER:
		{
			int ftype;
			
			/* Try spill handler */
			if (pte_spill_v(p->p_vmspace->vm_map.pmap,
			    frame->srr0, 0, 1))
				break;

			KERNEL_LOCK();
			ftype = VM_PROT_READ | VM_PROT_EXECUTE;
			if (uvm_fault(&p->p_vmspace->vm_map,
			    trunc_page(frame->srr0), 0, ftype) == 0) {
				uvm_grow(p, trunc_page(frame->srr0));
				KERNEL_UNLOCK();
				break;
			}
			KERNEL_UNLOCK();
		}
#if 0
printf("isi iar %x lr %x\n", frame->srr0, frame->lr);
#endif
		/* FALLTHROUGH */
	case EXC_MCHK|EXC_USER:
/* XXX Likely that returning from this trap is bogus... */
/* XXX Have to make sure that sigreturn does the right thing. */
		sv.sival_int = frame->srr0;
		KERNEL_LOCK();
		trapsignal(p, SIGSEGV, VM_PROT_EXECUTE, SEGV_MAPERR, sv);
		KERNEL_UNLOCK();
		break;
	case EXC_SC|EXC_USER:
		{
			struct sysent *callp;
			size_t argsize;
			register_t code, error;
			register_t *params, rval[2];
			int nsys, n, nolock;
			register_t args[10];
			
			uvmexp.syscalls++;
			
			nsys = p->p_emul->e_nsysent;
			callp = p->p_emul->e_sysent;
			
			code = frame->fixreg[0];
			params = frame->fixreg + FIRSTARG;
			
			switch (code) {
			case SYS_syscall:
				/*
				 * code is first argument,
				 * followed by actual args.
				 */
				code = *params++;
				break;
			case SYS___syscall:
				/*
				 * Like syscall, but code is a quad,
				 * so as to maintain quad alignment
				 * for the rest of the args.
				 */
				if (callp != sysent)
					break;
				params++;
				code = *params++;
				break;
			default:
				break;
			}
			if (code < 0 || code >= nsys)
				callp += p->p_emul->e_nosys;
			else
				callp += code;
			argsize = callp->sy_argsize;
			n = NARGREG - (params - (frame->fixreg + FIRSTARG));
			if (argsize > n * sizeof(register_t)) {
				bcopy(params, args, n * sizeof(register_t));

				error = copyin(MOREARGS(frame->fixreg[1]),
				   args + n, argsize - n * sizeof(register_t));
				if (error) {
#ifdef	KTRACE
					/* Can't get all the arguments! */
					if (KTRPOINT(p, KTR_SYSCALL))
						ktrsyscall(p, code,
							   argsize, args);
#endif
					goto syscall_bad;
				}
				params = args;
			}

#ifdef	KTRACE
			if (KTRPOINT(p, KTR_SYSCALL)) {
				KERNEL_LOCK();
				ktrsyscall(p, code, argsize, params);
				KERNEL_UNLOCK();
			}
#endif
			rval[0] = 0;
			rval[1] = frame->fixreg[FIRSTARG + 1];

#ifdef SYSCALL_DEBUG
			KERNEL_LOCK();
			scdebug_call(p, code, params);
			KERNEL_UNLOCK();
#endif

			
#if NSYSTRACE > 0
			if (ISSET(p->p_flag, P_SYSTRACE)) {
				KERNEL_LOCK();
				error = systrace_redirect(code, p, params,
				    rval);
				KERNEL_UNLOCK();
			} else
#endif
			{
				nolock = (callp->sy_flags & SY_NOLOCK);
				if (!nolock)
					KERNEL_LOCK();
				error = (*callp->sy_call)(p, params, rval);
				if (!nolock)
					KERNEL_UNLOCK();
			}
			switch (error) {
			case 0:
				frame->fixreg[0] = error;
				frame->fixreg[FIRSTARG] = rval[0];
				frame->fixreg[FIRSTARG + 1] = rval[1];
				frame->cr &= ~0x10000000;
				break;
			case ERESTART:
				/*
				 * Set user's pc back to redo the system call.
				 */
				frame->srr0 -= 4;
				break;
			case EJUSTRETURN:
				/* nothing to do */
				break;
			default:
syscall_bad:
				if (p->p_emul->e_errno)
					error = p->p_emul->e_errno[error];
				frame->fixreg[0] = error;
				frame->fixreg[FIRSTARG] = error;
				frame->fixreg[FIRSTARG + 1] = rval[1];
				frame->cr |= 0x10000000;
				break;
			}
#ifdef SYSCALL_DEBUG
			KERNEL_LOCK();
			scdebug_ret(p, code, error, rval); 
			KERNEL_UNLOCK();
#endif  
#ifdef	KTRACE
			if (KTRPOINT(p, KTR_SYSRET)) {
				KERNEL_LOCK();
				ktrsysret(p, code, error, rval[0]);
				KERNEL_UNLOCK();
			}
#endif
		}
		break;

	case EXC_FPU|EXC_USER:
		if (ci->ci_fpuproc)
			save_fpu();
		uvmexp.fpswtch++;
		enable_fpu(p);
		break;

	case EXC_ALI|EXC_USER:
		/* alignment exception 
		 * we check to see if this can be fixed up
		 * by the code that fixes the typical gcc misaligned code
		 * then kill the process if not.
		 */
		if (fix_unaligned(p, frame) == 0)
			frame->srr0 += 4;
		else {
			sv.sival_int = frame->srr0;
			KERNEL_LOCK();
			trapsignal(p, SIGSEGV, VM_PROT_EXECUTE, SEGV_MAPERR,
				sv);
			KERNEL_UNLOCK();
		}
		break;

	default:
	
brain_damage:
/*
mpc_print_pci_stat();
*/

#ifdef DDB
		/* set up registers */
		db_save_regs(frame);
		db_find_sym_and_offset(frame->srr0, &name, &offset);
#else
		name = NULL;
#endif
		if (!name) {
			name = "0";
			offset = frame->srr0;
		}
		panic ("trap type %x at %x (%s+0x%lx) lr %x",
			type, frame->srr0, name, offset, frame->lr);


	case EXC_PGM|EXC_USER:
	{
#if 0
		char *errstr[8];
		int errnum = 0;

		if (frame->srr1 & (1<<(31-11))) { 
			/* floating point enabled program exception */
			errstr[errnum] = "floating point";
			errnum++;
		} 
		if (frame->srr1 & (1<<(31-12))) {
			/* illegal instruction program exception */
			errstr[errnum] = "illegal instruction";
			errnum++;
		}
		if (frame->srr1 & (1<<(31-13))) {
			/* privileged instruction exception */
			errstr[errnum] = "privileged instr";
			errnum++;
		}
#endif
		if (frame->srr1 & (1<<(31-14))) {
#if 0
			errstr[errnum] = "trap instr";
			errnum++;
#endif
			sv.sival_int = frame->srr0;
			KERNEL_LOCK();
			trapsignal(p, SIGTRAP, type, TRAP_BRKPT, sv);
			KERNEL_UNLOCK();
			break;
		}
#if 0
		if (frame->srr1 & (1<<(31-15))) {
			errstr[errnum] = "previous address";
			errnum++;
		}
#endif
#if 0
printf("pgm iar %x srr1 %x\n", frame->srr0, frame->srr1);
{ 
int i;
for (i = 0; i < errnum; i++) {
	printf("\t[%s]\n", errstr[i]);
}
}
#endif
		sv.sival_int = frame->srr0;
		KERNEL_LOCK();
		trapsignal(p, SIGILL, 0, ILL_ILLOPC, sv);
		KERNEL_UNLOCK();
		break;
	}
	case EXC_PGM:
		/* should check for correct byte here or panic */
#ifdef DDB
		db_save_regs(frame);
		cnpollc(TRUE);
		db_trap(T_BREAKPOINT, 0);
		cnpollc(FALSE);
#else
		panic("trap EXC_PGM");
#endif
		break;

	/* This is not really a perf exception, but is an ALTIVEC unavail
	 * if we do not handle it, kill the process with illegal instruction.
	 */
	case EXC_PERF|EXC_USER:
#ifdef ALTIVEC 
	case EXC_VEC|EXC_USER:
		if (ci->ci_vecproc)
			save_vec(ci->ci_vecproc);

		enable_vec(p);
		break;
#else  /* ALTIVEC */
		sv.sival_int = frame->srr0;
		KERNEL_LOCK();
		trapsignal(p, SIGILL, 0, ILL_ILLOPC, sv);
		KERNEL_UNLOCK();
		break;
#endif

	case EXC_VECAST|EXC_USER:
		KERNEL_LOCK();
		trapsignal(p, SIGFPE, 0, FPE_FLTRES, sv);
		KERNEL_UNLOCK();
		break;

	case EXC_AST|EXC_USER:
		uvmexp.softs++;
		p->p_md.md_astpending = 0;	/* we are about to do it */
		if (p->p_flag & P_OWEUPC) {
			KERNEL_LOCK();
			ADDUPROF(p);
			KERNEL_UNLOCK();
		}
		if (ci->ci_want_resched)
			preempt(NULL);
		break;
	}

	userret(p);

	/*
	 * If someone stole the fpu while we were away, disable it
	 */
	if (p != ci->ci_fpuproc)
		frame->srr1 &= ~PSL_FP;
	else if (p->p_addr->u_pcb.pcb_flags & PCB_FPU)
		frame->srr1 |= PSL_FP;

#ifdef ALTIVEC
	/*
	 * If someone stole the vector unit while we were away, disable it
	 */
	if (p == ci->ci_vecproc)
		frame->srr1 |= PSL_VEC;
	else 
		frame->srr1 &= ~PSL_VEC;
#endif /* ALTIVEC */
}

void
child_return(void *arg)
{
	struct proc *p = (struct proc *)arg;
	struct trapframe *tf = trapframe(p);

	tf->fixreg[0] = 0;
	tf->fixreg[FIRSTARG] = 0;
	tf->fixreg[FIRSTARG + 1] = 1;
	tf->cr &= ~0x10000000;
	/* Disable FPU, VECT, as we can't be fpuproc */
	tf->srr1 &= ~(PSL_FP|PSL_VEC);

	KERNEL_UNLOCK();

	userret(p);

#ifdef	KTRACE
	if (KTRPOINT(p, KTR_SYSRET)) {
		KERNEL_LOCK();
		ktrsysret(p,
		    (p->p_flag & P_THREAD) ? SYS_rfork :
		    (p->p_p->ps_flags & PS_PPWAIT) ? SYS_vfork : SYS_fork,
		    0, 0);
		KERNEL_UNLOCK();
	}
#endif
}

int
badaddr(char *addr, u_int32_t len)
{
	faultbuf env;
	u_int32_t v;
	void *oldh = curpcb->pcb_onfault;

	if (setfault(&env)) {
		curpcb->pcb_onfault = oldh;
		return EFAULT;
	}
	switch(len) {
	case 4:
		v = *((volatile u_int32_t *)addr);
		break;
	case 2:
		v = *((volatile u_int16_t *)addr);
		break;
	default:
		v = *((volatile u_int8_t *)addr);
		break;
	}
	/* Make sure all loads retire before turning off fault handling!! */
	__asm__ volatile ("sync");
	curpcb->pcb_onfault = oldh;
	return(0);
}


/*
 * For now, this only deals with the particular unaligned access case
 * that gcc tends to generate.  Eventually it should handle all of the
 * possibilities that can happen on a 32-bit PowerPC in big-endian mode.
 */

static int
fix_unaligned(struct proc *p, struct trapframe *frame)
{
	int indicator = EXC_ALI_OPCODE_INDICATOR(frame->dsisr);
	struct cpu_info *ci = curcpu();

	switch (indicator) {
	case EXC_ALI_LFD:
	case EXC_ALI_STFD:
		{
			int reg = EXC_ALI_RST(frame->dsisr);
			double *fpr = &p->p_addr->u_pcb.pcb_fpu.fpr[reg];

			/* Juggle the FPU to ensure that we've initialized
			 * the FPRs, and that their current state is in
			 * the PCB.
			 */
			if (ci->ci_fpuproc != p) {
				if (ci->ci_fpuproc)
					save_fpu();
				enable_fpu(p);
			}
			save_fpu();

			if (indicator == EXC_ALI_LFD) {
				if (copyin((void *)frame->dar, fpr,
				    sizeof(double)) != 0)
					return -1;
			} else {
				if (copyout(fpr, (void *)frame->dar,
				    sizeof(double)) != 0)
					return -1;
			}
			enable_fpu(p);
			return 0;
		}
		break;
	}
	return -1;
}
