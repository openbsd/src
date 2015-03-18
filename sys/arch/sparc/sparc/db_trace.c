/*	$OpenBSD: db_trace.c,v 1.8 2015/03/18 20:49:10 miod Exp $	*/
/*	$NetBSD: db_trace.c,v 1.9 1997/07/29 09:42:00 fair Exp $ */

/*
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
 * CARNEGIE MELLON ALLOWS FREE USE OF THIS SOFTWARE IN ITS
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
#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_interface.h>
#include <ddb/db_output.h>

#define INKERNEL(va)	(((vaddr_t)(va)) >= VM_MIN_KERNEL_ADDRESS)
#define INKERNELTEXT(va)	(((vaddr_t)(va)) >= KERNBASE)

void
db_stack_trace_print(addr, have_addr, count, modif, pr)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
	int		(*pr)(const char *, ...);
{
	struct frame	*frame;
	boolean_t	kernel_only = TRUE;

	{
		char c, *cp = modif;
		while ((c = *cp++) != 0)
			if (c == 'u')
				kernel_only = FALSE;
	}

	if (count == -1)
		count = 65535;

	if (!have_addr)
		frame = (struct frame *)DDB_TF->tf_out[6];
	else
		frame = (struct frame *)addr;

	while (count--) {
		int		i;
		db_expr_t	offset;
		char		*name;
		db_addr_t	pc;

		pc = frame->fr_pc;
		if (!INKERNELTEXT(pc))
			break;

		/*
		 * Switch to frame that contains arguments
		 */
		frame = frame->fr_fp;
		if (!INKERNEL(frame))
			break;

		db_find_sym_and_offset(pc, &name, &offset);
		if (name == NULL)
			name = "?";

		(*pr)("%s(", name);

		/*
		 * Print %i0..%i5, hope these still reflect the
		 * actual arguments somewhat...
		 */
		for (i=0; i < 5; i++)
			(*pr)("0x%x, ", frame->fr_arg[i]);
		(*pr)("0x%x) at ", frame->fr_arg[i]);
		db_printsym(pc, DB_STGY_PROC, pr);
		(*pr)("\n");

	}
}
