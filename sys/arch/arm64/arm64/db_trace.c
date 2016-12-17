/*	$OpenBSD: db_trace.c,v 1.1 2016/12/17 23:38:33 patrick Exp $	*/
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
#include <ddb/db_sym.h>
#include <ddb/db_output.h>

db_regs_t ddb_regs;

#define INKERNEL(va)	(((vaddr_t)(va)) >= VM_MIN_KERNEL_ADDRESS)

#ifndef __clang__
/*
 * Clang uses a different stack frame, which looks like the following.
 *
 *          return link value       [fp, #+4]
 *          return fp value         [fp]        <- fp points to here
 *
 */
#define FR_RFP	(0x0)
#define FR_RLV	(+0x4)
#endif /* !__clang__ */

void
db_stack_trace_print(addr, have_addr, count, modif, pr)
	db_expr_t       addr;
	int             have_addr;
	db_expr_t       count;
	char            *modif;
	int		(*pr) (const char *, ...);
{
	u_int32_t	frame, lastframe;
	char c, *cp = modif;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_thread = FALSE;
	//db_addr_t	scp = 0;
	int	scp_offset;

	while ((c = *cp++) != 0) {
		if (c == 'u')
			kernel_only = FALSE;
		if (c == 't')
			trace_thread = TRUE;
	}

	if (!have_addr) {
		// Implement
	} else {
		// Implement
	}
	lastframe = 0;
	//scp_offset = -get_pc_str_offset();
	scp_offset = -4;

	while (count-- && frame != 0) {
		break;
	// Implement
	}
}
