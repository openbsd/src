/*	$NetBSD: db_trace.c,v 1.15 1996/02/22 23:23:41 gwr Exp $	*/

/* 
 * Mach Operating System
 * Copyright (c) 1992 Carnegie Mellon University
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
 */

#include <sys/param.h>
#include <sys/proc.h>

#include <machine/db_machdep.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

struct db_variable db_regs[] = { 
	{ "r1",  (int *) &ddb_regs.r1,	FCN_NULL },
	{ "r2",  (int *) &ddb_regs.r2,	FCN_NULL },
	{ "r3",  (int *) &ddb_regs.r3,	FCN_NULL },
	{ "r4",  (int *) &ddb_regs.r4,	FCN_NULL },
	{ "r5",  (int *) &ddb_regs.r5,	FCN_NULL },
	{ "r6",  (int *) &ddb_regs.r6,	FCN_NULL },
	{ "r7",  (int *) &ddb_regs.r7,	FCN_NULL },
	{ "r8",  (int *) &ddb_regs.r8,	FCN_NULL },
	{ "r9",  (int *) &ddb_regs.r9,	FCN_NULL },
	{ "r10", (int *) &ddb_regs.r10,	FCN_NULL },
	{ "r11", (int *) &ddb_regs.r11,	FCN_NULL },
	{ "r12", (int *) &ddb_regs.r12,	FCN_NULL },
	{ "r13", (int *) &ddb_regs.r13,	FCN_NULL },
	{ "r14", (int *) &ddb_regs.r13,	FCN_NULL },
	{ "r15", (int *) &ddb_regs.r13,	FCN_NULL },
	{ "r16", (int *) &ddb_regs.r13,	FCN_NULL },
	{ "r17", (int *) &ddb_regs.r17,	FCN_NULL },
	{ "r18", (int *) &ddb_regs.r18,	FCN_NULL },
	{ "r19", (int *) &ddb_regs.r19,	FCN_NULL },
	{ "r20", (int *) &ddb_regs.r20,	FCN_NULL },
	{ "r21", (int *) &ddb_regs.r21,	FCN_NULL },
	{ "r22", (int *) &ddb_regs.r22,	FCN_NULL },
	{ "r23", (int *) &ddb_regs.r23,	FCN_NULL },
	{ "r24", (int *) &ddb_regs.r24,	FCN_NULL },
	{ "r25", (int *) &ddb_regs.r25,	FCN_NULL },
	{ "r26", (int *) &ddb_regs.r26,	FCN_NULL },
	{ "r27", (int *) &ddb_regs.r27,	FCN_NULL },
	{ "r28", (int *) &ddb_regs.r28,	FCN_NULL },
	{ "r29", (int *) &ddb_regs.r29,	FCN_NULL },
	{ "r30", (int *) &ddb_regs.r30,	FCN_NULL },
	{ "r31", (int *) &ddb_regs.r31,	FCN_NULL },
	{ "r32", (int *) &ddb_regs.r32,	FCN_NULL },
	{ "iar", (int *) &ddb_regs.iar,	FCN_NULL },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

extern label_t	*db_recover;

/*
 *	Frame tracing.
 */
void
db_stack_trace_cmd(addr, have_addr, count, modif)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char		*modif;
{
	int i, val, nargs, spa;
	db_addr_t	regp;
	char *		name;
	boolean_t	kernel_only = TRUE;
	boolean_t	trace_thread = FALSE;

	db_printf("not supported");
}

