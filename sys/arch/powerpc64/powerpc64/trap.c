/*	$OpenBSD: trap.c,v 1.9 2020/06/19 21:24:01 kettenis Exp $	*/

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
		panic("trap type %lx srr1 %lx at %lx lr %lx",
		    frame->exc, frame->srr1, frame->srr0, frame->lr);
	}

	userret(p);
}
