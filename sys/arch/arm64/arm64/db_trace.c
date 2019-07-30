/*	$OpenBSD: db_trace.c,v 1.5 2017/05/30 15:39:04 mpi Exp $	*/
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

#include <sys/proc.h>
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

#ifndef __clang__
/*
 * Clang uses a different stack frame, which looks like the following.
 *
 *          return link value       [fp, #+4]
 *          return fp value         [fp]        <- fp points to here
 *
 */
#define FR_RFP	(0x0)
#define FR_RLV	(0x4)
#endif /* !__clang__ */

void
db_stack_trace_print(db_expr_t addr, int have_addr, db_expr_t count,
    char *modif, int (*pr)(const char *, ...))
{
	u_int64_t	frame, lastframe, lr, lastlr, sp;
	char		c, *cp = modif;
	db_expr_t	offset;
	Elf_Sym *	sym;
	char		*name;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_thread = FALSE;
	//db_addr_t	scp = 0;

	while ((c = *cp++) != 0) {
		if (c == 'u')
			kernel_only = FALSE;
		if (c == 't')
			trace_thread = TRUE;
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
			db_read_bytes(sp+16, sizeof(db_addr_t),
			    (char *)&frame);
			db_read_bytes(sp + 8, sizeof(db_addr_t),
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
		db_read_bytes(frame, sizeof(db_addr_t), (char *)&frame);
		if (frame == 0)
			break;
		lastlr = lr;
		db_read_bytes(frame + 8, sizeof(db_addr_t), (char *)&lr);

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
