/*	$OpenBSD: trap.c,v 1.10 2020/06/21 13:23:59 kettenis Exp $	*/

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
#include <sys/user.h>
#include <sys/syscall.h>
#include <sys/syscall_mi.h>
#include <sys/systm.h>

#include <uvm/uvm_extern.h>

#ifdef DDB
#include <machine/db_machdep.h>
#endif
#include <machine/trap.h>

void	decr_intr(struct trapframe *); /* clock.c */
void	hvi_intr(struct trapframe *);  /* intr.c */
void	syscall(struct trapframe *);   /* syscall.c */

void
trap(struct trapframe *frame)
{
	struct cpu_info *ci = curcpu();
	struct proc *p = curproc;
	int type = frame->exc;
	struct vm_map *map;
	vaddr_t va;
	int ftype;

	switch (type) {
	case EXC_DECR:
		decr_intr(frame);
		return;
	case EXC_HVI:
		hvi_intr(frame);
		return;
	}

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
		if (curpcb->pcb_onfault && va < VM_MAXUSER_ADDRESS) {
			map = &p->p_vmspace->vm_map;
		}
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

		if (curpcb->pcb_onfault)
			printf("DSI onfault\n");

		printf("dar 0x%lx dsisr 0x%lx\n", frame->dar, frame->dsisr);
		goto fatal;

	case EXC_DSE:
		if (curpcb->pcb_onfault)
			printf("DSE onfault\n");

		printf("dar 0x%lx dsisr 0x%lx\n", frame->dar, frame->dsisr);
		goto fatal;

	case EXC_SC|EXC_USER:
		intr_enable();
		syscall(frame);
		return;

	case EXC_AST|EXC_USER:
		p->p_md.md_astpending = 0;
		intr_enable();
		uvmexp.softs++;
		mi_ast(p, ci->ci_want_resched);
		break;

	default:
	fatal:
		panic("trap type %lx srr1 %lx at %lx lr %lx",
		    frame->exc, frame->srr1, frame->srr0, frame->lr);
	}

	userret(p);
}
