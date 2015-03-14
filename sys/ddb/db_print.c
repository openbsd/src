/*	$OpenBSD: db_print.c,v 1.15 2015/03/14 03:38:46 jsg Exp $	*/
/*	$NetBSD: db_print.c,v 1.5 1996/02/05 01:57:11 christos Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1993,1992,1991,1990 Carnegie Mellon University
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
 * any improvements or extensions that they make and grant Carnegie Mellon
 * the rights to redistribute these changes.
 *
 * 	Author: David B. Golub, Carnegie Mellon University
 *	Date:	7/90
 */

/*
 * Miscellaneous printing.
 */
#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>

#include <ddb/db_variables.h>
#include <ddb/db_sym.h>
#include <ddb/db_output.h>
#include <ddb/db_extern.h>

/*ARGSUSED*/
void
db_show_regs(db_expr_t addr, int have_addr, db_expr_t count, char *modif)
{
	struct db_variable *regp;
	db_expr_t	value, offset;
	char *		name;
	char		tmpfmt[28];

	for (regp = db_regs; regp < db_eregs; regp++) {
	    db_read_variable(regp, &value);
	    db_printf("%-12s%s", regp->name, db_format(tmpfmt, sizeof tmpfmt,
	      (long)value, DB_FORMAT_N, 1, sizeof(long) * 3));
	    db_find_xtrn_sym_and_offset((db_addr_t)value, &name, &offset);
	    if (name != 0 && offset <= db_maxoff && offset != value) {
		db_printf("\t%s", name);
		if (offset != 0)
		    db_printf("+%s", db_format(tmpfmt, sizeof tmpfmt,
		      (long)offset, DB_FORMAT_R, 1, 0));
	    }
	    db_printf("\n");
	}
	db_print_loc_and_inst(PC_REGS(DDB_REGS));
}
