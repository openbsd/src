/*	$OpenBSD: trap.c,v 1.17 2020/06/28 00:07:22 kettenis Exp $	*/

/*
 * Copyright (c) 2020 Mark Kettenis <kettenis@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/signalvar.h>
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/syscall_mi.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#include <machine/fpu.h>
#include <machine/trap.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif

void	decr_intr(struct trapframe *); /* clock.c */
void	hvi_intr(struct trapframe *);  /* intr.c */
void	syscall(struct trapframe *);   /* syscall.c */

void
trap(struct trapframe *frame)
{
	struct cpu_info *ci = curcpu();
	struct proc *p = curproc;
	int type = frame->exc;
	union sigval sv;
	struct vm_map *map;
	struct slb_desc *slbd;
	pmap_t pm;
	vaddr_t va;
	int ftype;
	int error, sig, code;

	/* Disable access to floating-point and vector registers. */
	mtmsr(mfmsr() & ~(PSL_FP|PSL_VEC|PSL_VSX));

	switch (type) {
	case EXC_DECR:
		decr_intr(frame);
		return;
	case EXC_HVI:
		hvi_intr(frame);
		return;
	}

	if (frame->srr1 & PSL_EE)
		intr_enable();

	if (frame->srr1 & PSL_PR) {
		type |= EXC_USER;
		p->p_md.md_regs = frame;
		refreshcreds(p);
	}

	switch (type) {
#ifdef DDB
	case EXC_PGM:
		/* At a trap instruction, enter the debugger. */
		if (frame->srr1 & EXC_PGM_TRAP) {
			/* Return from db_enter(). */
			if (frame->srr0 == (register_t)db_enter)
				frame->srr0 = frame->lr;
			db_ktrap(T_BREAKPOINT, frame);
			return;
		}
		break;
	case EXC_TRC:
		db_ktrap(T_BREAKPOINT, frame); /* single-stepping */
		return;
#endif

	case EXC_DSI:
		map = kernel_map;
		va = frame->dar;
		if (curpcb->pcb_onfault && va < VM_MAXUSER_ADDRESS)
			map = &p->p_vmspace->vm_map;
		if (frame->dsisr & DSISR_STORE)
			ftype = PROT_READ | PROT_WRITE;
		else
			ftype = PROT_READ;
		KERNEL_LOCK();
		if (uvm_fault(map, trunc_page(va), 0, ftype) == 0) {
			KERNEL_UNLOCK();
			return;
		}
		KERNEL_UNLOCK();

		if (curpcb->pcb_onfault) {
			frame->srr0 = curpcb->pcb_onfault;
			return;
		}

		printf("dar 0x%lx dsisr 0x%lx\n", frame->dar, frame->dsisr);
		goto fatal;

	case EXC_DSE:
		if (curpcb->pcb_onfault) {
			frame->srr0 = curpcb->pcb_onfault;
			return;
		}

		printf("dar 0x%lx dsisr 0x%lx\n", frame->dar, frame->dsisr);
		goto fatal;

	case EXC_DSE|EXC_USER:
		pm = p->p_vmspace->vm_map.pmap;
		slbd = pmap_slbd_lookup(pm, frame->dar);
		if (slbd) {
			pmap_slbd_cache(pm, slbd);
			break;
		}
		/* FALLTHROUGH */

	case EXC_DSI|EXC_USER:
		map = &p->p_vmspace->vm_map;
		va = frame->dar;
		if (frame->dsisr & DSISR_STORE)
			ftype = PROT_READ | PROT_WRITE;
		else
			ftype = PROT_READ;
		KERNEL_LOCK();
		error = uvm_fault(map, trunc_page(va), 0, ftype);
		KERNEL_UNLOCK();
		if (error) {
			printf("type %x dar 0x%lx dsisr 0x%lx\n",
			    type, frame->dar, frame->dsisr);
			for (int i = 0; i < 32; i++)
				printf("r%d 0x%lx\n", i, frame->fixreg[i]);
			printf("ctr 0x%lx\n", frame->ctr);
			printf("xer 0x%lx\n", frame->xer);
			printf("cr 0x%lx\n", frame->cr);
			printf("lr 0x%lx\n", frame->lr);

			if (error == ENOMEM) {
				sig = SIGKILL;
				code = 0;
			} else if (error == EIO) {
				sig = SIGBUS;
				code = BUS_OBJERR;
			} else if (error == EACCES) {
				sig = SIGSEGV;
				code = SEGV_ACCERR;
			} else {
				sig = SIGSEGV;
				code = SEGV_MAPERR;
			}
			sv.sival_ptr = (void *)va;
			KERNEL_LOCK();
			trapsignal(p, sig, 0, code, sv);
			KERNEL_UNLOCK();
		}
		break;

	case EXC_ISE|EXC_USER:
		pm = p->p_vmspace->vm_map.pmap;
		slbd = pmap_slbd_lookup(pm, frame->srr0);
		if (slbd) {
			pmap_slbd_cache(pm, slbd);
			break;
		}
		/* FALLTHROUGH */

	case EXC_ISI|EXC_USER:
		map = &p->p_vmspace->vm_map;
		va = frame->srr0;
		ftype = PROT_READ | PROT_EXEC;
		KERNEL_LOCK();
		error = uvm_fault(map, trunc_page(va), 0, ftype);
		KERNEL_UNLOCK();
		if (error) {
			printf("type %x srr0 0x%lx\n", type, frame->srr0);
			for (int i = 0; i < 32; i++)
				printf("r%d 0x%lx\n", i, frame->fixreg[i]);
			printf("ctr 0x%lx\n", frame->ctr);
			printf("xer 0x%lx\n", frame->xer);
			printf("cr 0x%lx\n", frame->cr);
			printf("lr 0x%lx\n", frame->lr);

			if (error == ENOMEM) {
				sig = SIGKILL;
				code = 0;
			} else if (error == EIO) {
				sig = SIGBUS;
				code = BUS_OBJERR;
			} else if (error == EACCES) {
				sig = SIGSEGV;
				code = SEGV_ACCERR;
			} else {
				sig = SIGSEGV;
				code = SEGV_MAPERR;
			}
			sv.sival_ptr = (void *)va;
			KERNEL_LOCK();
			trapsignal(p, sig, 0, code, sv);
			KERNEL_UNLOCK();
		}
		break;

	case EXC_SC|EXC_USER:
		syscall(frame);
		return;

	case EXC_AST|EXC_USER:
		p->p_md.md_astpending = 0;
		uvmexp.softs++;
		mi_ast(p, ci->ci_want_resched);
		break;

	case EXC_ALI|EXC_USER:
		sv.sival_ptr = (void *)frame->dar;
		KERNEL_LOCK();
		trapsignal(p, SIGBUS, 0, BUS_ADRALN, sv);
		KERNEL_UNLOCK();
		break;

	case EXC_PGM|EXC_USER:
		printf("type %x srr0 0x%lx\n", type, frame->srr0);
		for (int i = 0; i < 32; i++)
			printf("r%d 0x%lx\n", i, frame->fixreg[i]);
		printf("ctr 0x%lx\n", frame->ctr);
		printf("xer 0x%lx\n", frame->xer);
		printf("cr 0x%lx\n", frame->cr);
		printf("lr 0x%lx\n", frame->lr);

		sv.sival_ptr = (void *)frame->srr0;
		KERNEL_LOCK();
		trapsignal(p, SIGTRAP, 0, TRAP_BRKPT, sv);
		KERNEL_UNLOCK();
		break;

	case EXC_FPU|EXC_USER:
		restore_vsx(p);
		curpcb->pcb_flags |= PCB_FP;
		frame->srr1 |= PSL_FP;
		break;

	case EXC_VEC|EXC_USER:
		restore_vsx(p);
		curpcb->pcb_flags |= PCB_VEC;
		frame->srr1 |= PSL_VEC;
		break;

	default:
	fatal:
		panic("trap type %x srr1 %lx at %lx lr %lx",
		    type, frame->srr1, frame->srr0, frame->lr);
	}

	userret(p);
}
