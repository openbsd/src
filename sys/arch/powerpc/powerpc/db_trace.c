/*	$OpenBSD: db_trace.c,v 1.4 1998/08/22 17:54:27 rahnds Exp $	*/
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
#include <machine/signal.h>

#include <ddb/db_access.h>
#include <ddb/db_sym.h>
#include <ddb/db_variables.h>

struct db_variable db_regs[] = { 
	{ "r0",  (long *)&(DDB_REGS->tf.fixreg[0]),	FCN_NULL },
	{ "r1",  (long *)&(DDB_REGS->tf.fixreg[1]),	FCN_NULL },
	{ "r2",  (long *)&(DDB_REGS->tf.fixreg[2]),	FCN_NULL },
	{ "r3",  (long *)&(DDB_REGS->tf.fixreg[3]),	FCN_NULL },
	{ "r4",  (long *)&(DDB_REGS->tf.fixreg[4]),	FCN_NULL },
	{ "r5",  (long *)&(DDB_REGS->tf.fixreg[5]),	FCN_NULL },
	{ "r6",  (long *)&(DDB_REGS->tf.fixreg[6]),	FCN_NULL },
	{ "r7",  (long *)&(DDB_REGS->tf.fixreg[7]),	FCN_NULL },
	{ "r8",  (long *)&(DDB_REGS->tf.fixreg[8]),	FCN_NULL },
	{ "r9",  (long *)&(DDB_REGS->tf.fixreg[9]),	FCN_NULL },
	{ "r10", (long *)&(DDB_REGS->tf.fixreg[10]),	FCN_NULL },
	{ "r11", (long *)&(DDB_REGS->tf.fixreg[11]),	FCN_NULL },
	{ "r12", (long *)&(DDB_REGS->tf.fixreg[12]),	FCN_NULL },
	{ "r13", (long *)&(DDB_REGS->tf.fixreg[13]),	FCN_NULL },
	{ "r14", (long *)&(DDB_REGS->tf.fixreg[13]),	FCN_NULL },
	{ "r15", (long *)&(DDB_REGS->tf.fixreg[13]),	FCN_NULL },
	{ "r16", (long *)&(DDB_REGS->tf.fixreg[13]),	FCN_NULL },
	{ "r17", (long *)&(DDB_REGS->tf.fixreg[17]),	FCN_NULL },
	{ "r18", (long *)&(DDB_REGS->tf.fixreg[18]),	FCN_NULL },
	{ "r19", (long *)&(DDB_REGS->tf.fixreg[19]),	FCN_NULL },
	{ "r20", (long *)&(DDB_REGS->tf.fixreg[20]),	FCN_NULL },
	{ "r21", (long *)&(DDB_REGS->tf.fixreg[21]),	FCN_NULL },
	{ "r22", (long *)&(DDB_REGS->tf.fixreg[22]),	FCN_NULL },
	{ "r23", (long *)&(DDB_REGS->tf.fixreg[23]),	FCN_NULL },
	{ "r24", (long *)&(DDB_REGS->tf.fixreg[24]),	FCN_NULL },
	{ "r25", (long *)&(DDB_REGS->tf.fixreg[25]),	FCN_NULL },
	{ "r26", (long *)&(DDB_REGS->tf.fixreg[26]),	FCN_NULL },
	{ "r27", (long *)&(DDB_REGS->tf.fixreg[27]),	FCN_NULL },
	{ "r28", (long *)&(DDB_REGS->tf.fixreg[28]),	FCN_NULL },
	{ "r29", (long *)&(DDB_REGS->tf.fixreg[29]),	FCN_NULL },
	{ "r30", (long *)&(DDB_REGS->tf.fixreg[30]),	FCN_NULL },
	{ "r31", (long *)&(DDB_REGS->tf.fixreg[31]),	FCN_NULL },
	{ "lr", (long *)&(DDB_REGS->tf.lr),	FCN_NULL },
	{ "cr", (long *)&(DDB_REGS->tf.cr),	FCN_NULL },
	{ "xer", (long *)&(DDB_REGS->tf.xer),	FCN_NULL },
	{ "ctr", (long *)&(DDB_REGS->tf.ctr),	FCN_NULL },
	{ "iar", (long *)&(DDB_REGS->tf.srr0),	FCN_NULL },
	{ "msr", (long *)&(DDB_REGS->tf.srr1),	FCN_NULL },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

extern label_t	*db_recover;

/*
 * this is probably hackery.
 */
void
db_save_regs(struct trapframe *frame)
{
	printf ("%x %x %d\n", frame, &(ddb_regs.tf), sizeof (struct trapframe));
	bcopy(frame, &(ddb_regs.tf), sizeof (struct trapframe));
}


db_expr_t
db_dumpframe (u_int32_t pframe)
{
	u_int32_t nextframe;
	u_int32_t lr;
	u_int32_t *access;
	int error;

	access = (u_int32_t *)(pframe);
	nextframe = *access;

	access = (u_int32_t *)(pframe+4);
	lr = *access;

	db_printf("lr %x fp %x nfp %x\n", lr, pframe, nextframe);

	return nextframe;
}
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

	while (1) {
		addr = db_dumpframe(addr);
		if (addr == 0) {
			break;
		}
	}
	for (i = count; i > 0 ; i--) {
	}
}
