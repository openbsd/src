/*	$OpenBSD: db_trace.c,v 1.15 2023/06/17 08:13:56 kettenis Exp $	*/
/*	$NetBSD: db_trace.c,v 1.8 2003/01/17 22:28:48 thorpej Exp $	*/

/*
 * Copyright (c) 2000, 2001 Ben Harris
 * Copyright (c) 1996 Scott K. Stevens
 *
 * Mach Operating System
 * Copyright (c) 1991,1990 Carnegie Mellon University
 * All Rights Reserved.
 *
 * Permission to use, copy, modify and distribute this software and its
 * documentation is hereby granted, provided that both the copyright
 * notice and this permission notice appear in all copies of the
 * software, derivative works or modified versions, and any portions
 * thereof, and that both notices appear in supporting documentation.
 *
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS "AS IS"
 * CONDITION.  CARNEGIE MELLON DISCLAIMS ANY LIABILITY OF ANY KIND FOR
 * ANY DAMAGES WHATSOEVER RESULTING FROM THE USE OF THIS SOFTWARE.
 *
 * Carnegie Mellon requests users of this software to return to
 *
 *  Software Distribution Coordinator  or  Software.Distribution@CS.CMU.EDU
 *  School of Computer Science
 *  Carnegie Mellon University
 *  Pittsburgh PA 15213-3890
 *
 * any improvements or extensions that they make and grant Carnegie the
 * rights to redistribute these changes.
 */

#include <sys/param.h>
#include <sys/systm.h>

#include <sys/proc.h>
#include <sys/stacktrace.h>
#include <sys/user.h>
#include <arm64/armreg.h>
#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_interface.h>
#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>

db_regs_t ddb_regs;

#define INKERNEL(va)	(((vaddr_t)(va)) & (1ULL << 63))

void
db_stack_trace_print(db_expr_t addr, int have_addr, db_expr_t count,
    char *modif, int (*pr)(const char *, ...))
{
	vaddr_t		frame, lastframe, lr, lastlr, sp;
	char		c, *cp = modif;
	db_expr_t	offset;
	Elf_Sym *	sym;
	char		*name;
	int		kernel_only = 1;
	int		trace_thread = 0;

	while ((c = *cp++) != 0) {
		if (c == 'u')
			kernel_only = 0;
		if (c == 't')
			trace_thread = 1;
	}

	if (!have_addr) {
		sp = ddb_regs.tf_sp;
		lr = ddb_regs.tf_lr;
		lastlr = ddb_regs.tf_elr;
		frame = ddb_regs.tf_x[29];
	} else {
		if (trace_thread) {
			struct proc *p = tfind((pid_t)addr);
			if (p == NULL) {
				(*pr)("not found\n");
				return;
			}
			frame = p->p_addr->u_pcb.pcb_tf->tf_x[29];
			sp =  p->p_addr->u_pcb.pcb_tf->tf_sp;
			lr =  p->p_addr->u_pcb.pcb_tf->tf_lr;
			lastlr =  p->p_addr->u_pcb.pcb_tf->tf_elr;
		} else {
			sp = addr;
			db_read_bytes(sp, sizeof(vaddr_t),
			    (char *)&frame);
			db_read_bytes(sp + 8, sizeof(vaddr_t),
			    (char *)&lr);
			lastlr = 0;
		}
	}

	while (count-- && frame != 0) {
		lastframe = frame;

		sym = db_search_symbol(lastlr, DB_STGY_ANY, &offset);
		db_symbol_values(sym, &name, NULL);

		if (name == NULL || strcmp(name, "end") == 0) {
			(*pr)("%llx at 0x%lx", lastlr, lr - 4);
		} else {
			(*pr)("%s() at ", name);
			db_printsym(lr - 4, DB_STGY_PROC, pr);
		}
		(*pr)("\n");

		// can we detect traps ?
		db_read_bytes(frame, sizeof(vaddr_t), (char *)&frame);
		if (frame == 0)
			break;
		lastlr = lr;
		db_read_bytes(frame + 8, sizeof(vaddr_t), (char *)&lr);

		if (name != NULL) {
			if ((strcmp (name, "handle_el0_irq") == 0) ||
			    (strcmp (name, "handle_el1_irq") == 0)) {
				(*pr)("--- interrupt ---\n");
			} else if (
			    (strcmp (name, "handle_el0_sync") == 0) ||
			    (strcmp (name, "handle_el1_sync") == 0)) {
				(*pr)("--- trap ---\n");
			}
		}
		if (INKERNEL(frame)) {
			if (frame <= lastframe) {
				(*pr)("Bad frame pointer: 0x%lx\n", frame);
				break;
			}
		} else {
			if (kernel_only)
				break;
		}
		--count;
	}
}

void
stacktrace_save_at(struct stacktrace *st, unsigned int skip)
{
	struct callframe *frame, *lastframe, *limit;
	struct proc *p = curproc;

	st->st_count = 0;

	if (p == NULL)
		return;

	frame = __builtin_frame_address(0);
	KASSERT(INKERNEL(frame));
	limit = (struct callframe *)STACKALIGN(p->p_addr + USPACE -
	    sizeof(struct trapframe) - 0x10);

	while (st->st_count < STACKTRACE_MAX) {
		if (skip == 0)
			st->st_pc[st->st_count++] = frame->f_lr;
		else
			skip--;

		lastframe = frame;
		frame = frame->f_frame;

		if (frame <= lastframe)
			break;
		if (frame >= limit)
			break;
		if (!INKERNEL(frame->f_lr))
			break;
	}
}

void
stacktrace_save_utrace(struct stacktrace *st)
{
	st->st_count = 0;
}
