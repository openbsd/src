/*	$OpenBSD: db_trace.c,v 1.1 2001/06/26 21:57:39 smurph Exp $	*/

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
        { "r0",  (long *)&ddb_regs.r[0],  FCN_NULL },
        { "r1",  (long *)&ddb_regs.r[1],  FCN_NULL },
        { "r2",  (long *)&ddb_regs.r[2],  FCN_NULL },
        { "r3",  (long *)&ddb_regs.r[3],  FCN_NULL },
        { "r4",  (long *)&ddb_regs.r[4],  FCN_NULL },
        { "r5",  (long *)&ddb_regs.r[5],  FCN_NULL },
        { "r6",  (long *)&ddb_regs.r[6],  FCN_NULL },
        { "r7",  (long *)&ddb_regs.r[7],  FCN_NULL },
        { "r8",  (long *)&ddb_regs.r[8],  FCN_NULL },
        { "r9",  (long *)&ddb_regs.r[9],  FCN_NULL },
        { "r10", (long *)&ddb_regs.r[10], FCN_NULL },
        { "r11", (long *)&ddb_regs.r[11], FCN_NULL },
        { "r12", (long *)&ddb_regs.r[12], FCN_NULL },
        { "r13", (long *)&ddb_regs.r[13], FCN_NULL },
        { "r14", (long *)&ddb_regs.r[14], FCN_NULL },
        { "r15", (long *)&ddb_regs.r[15], FCN_NULL },
        { "r16", (long *)&ddb_regs.r[16], FCN_NULL },
        { "r17", (long *)&ddb_regs.r[17], FCN_NULL },
        { "r18", (long *)&ddb_regs.r[18], FCN_NULL },
        { "r19", (long *)&ddb_regs.r[19], FCN_NULL },
        { "r20", (long *)&ddb_regs.r[20], FCN_NULL },
        { "r21", (long *)&ddb_regs.r[21], FCN_NULL },
        { "r22", (long *)&ddb_regs.r[22], FCN_NULL },
        { "r23", (long *)&ddb_regs.r[23], FCN_NULL },
        { "r24", (long *)&ddb_regs.r[24], FCN_NULL },
        { "r25", (long *)&ddb_regs.r[25], FCN_NULL },
        { "r26", (long *)&ddb_regs.r[26], FCN_NULL },
        { "r27", (long *)&ddb_regs.r[27], FCN_NULL },
        { "r28", (long *)&ddb_regs.r[28], FCN_NULL },
        { "r29", (long *)&ddb_regs.r[29], FCN_NULL },
        { "r30", (long *)&ddb_regs.r[30], FCN_NULL },
        { "r31", (long *)&ddb_regs.r[31], FCN_NULL },
        { "iar", (long *)&ddb_regs.iar,   FCN_NULL },
        { "msr", (long *)&ddb_regs.msr,   FCN_NULL },
};

struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

extern label_t	*db_recover;

/*
 * this is probably hackery.
 */
void
db_save_regs(struct trapframe *frame)
{
	bcopy(frame->fixreg, DDB_REGS->r, 32 * sizeof(u_int32_t));
	DDB_REGS->iar = frame->srr0;
	DDB_REGS->msr = frame->srr1;
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
        db_expr_t addr;
        int have_addr;
        db_expr_t count;
        char *modif;
{
        db_addr_t frame, lr, caller;
        db_expr_t diff;
        db_sym_t sym;
        char *symname;
        boolean_t kernel_only = TRUE;
        boolean_t trace_thread = FALSE;
        boolean_t full = FALSE;

        {
                register char *cp = modif;
                register char c;

                while ((c = *cp++) != 0) {
                        if (c == 't')
                                trace_thread = TRUE;
                        if (c == 'u')
                                kernel_only = FALSE;
                        if (c == 'f')
                                full = TRUE;
                }
        }

        frame = (db_addr_t)ddb_regs.r[1];
        while ((frame = *(db_addr_t *)frame) && count--) {
                db_addr_t *args = (db_addr_t *)(frame + 8);

                lr = *(db_addr_t *)(frame + 4) - 4;
                if ((lr & 3) || (lr < 0x10000)) {
                        printf("saved LR(0x%x) is invalid.", lr);
                        break;
                }
                if ((caller = (db_addr_t)vtophys(lr)) == 0)
                        caller = lr;


                diff = 0;
                symname = NULL;
                sym = db_search_symbol(caller, DB_STGY_ANY, &diff);
                db_symbol_values(sym, &symname, 0);
                if (symname == NULL)
                        printf("%p ", caller);
                else
                        printf("%s+%x ", symname, diff);
                if (full)
                        /* Print all the args stored in that stackframe. */
                        printf(" (%lx, %lx, %lx, %lx, %lx, %lx, %lx, %lx) %lx ",
                                args[0], args[1], args[2], args[3],
                                args[4], args[5], args[6], args[7], frame);
		printf("\n");
        }
}

