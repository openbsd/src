/*	$OpenBSD: db_interface.c,v 1.1 2005/04/01 10:40:47 mickey Exp $	*/

/*
 * Copyright (c) 2005 Michael Shalayeff
 * All rights reserved.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define DDB_DEBUG

#include <sys/param.h>
#include <sys/systm.h>

#include <machine/db_machdep.h>
#include <machine/frame.h>

#include <ddb/db_access.h>
#include <ddb/db_command.h>
#include <ddb/db_output.h>
#include <ddb/db_run.h>
#include <ddb/db_sym.h>
#include <ddb/db_var.h>
#include <ddb/db_variables.h>
#include <ddb/db_extern.h>
#include <ddb/db_interface.h>

#include <dev/cons.h>

void kdbprinttrap(int, int);

extern char *trap_type[];
extern int trap_types;

db_regs_t	ddb_regs;
struct db_variable db_regs[] = {
	{ "flags", (long *)&ddb_regs.tf_flags,  FCN_NULL },
	{ "r1",    (long *)&ddb_regs.tf_r1,  FCN_NULL },
	{ "rp",    (long *)&ddb_regs.tf_rp,  FCN_NULL },
	{ "r3",    (long *)&ddb_regs.tf_r3,  FCN_NULL },
	{ "r4",    (long *)&ddb_regs.tf_r4,  FCN_NULL },
	{ "r5",    (long *)&ddb_regs.tf_r5,  FCN_NULL },
	{ "r6",    (long *)&ddb_regs.tf_r6,  FCN_NULL },
	{ "r7",    (long *)&ddb_regs.tf_r7,  FCN_NULL },
	{ "r8",    (long *)&ddb_regs.tf_r8,  FCN_NULL },
	{ "r9",    (long *)&ddb_regs.tf_r9,  FCN_NULL },
	{ "r10",   (long *)&ddb_regs.tf_r10, FCN_NULL },
	{ "r11",   (long *)&ddb_regs.tf_r11, FCN_NULL },
	{ "r12",   (long *)&ddb_regs.tf_r12, FCN_NULL },
	{ "r13",   (long *)&ddb_regs.tf_r13, FCN_NULL },
	{ "r14",   (long *)&ddb_regs.tf_r14, FCN_NULL },
	{ "r15",   (long *)&ddb_regs.tf_r15, FCN_NULL },
	{ "r16",   (long *)&ddb_regs.tf_r16, FCN_NULL },
	{ "r17",   (long *)&ddb_regs.tf_r17, FCN_NULL },
	{ "r18",   (long *)&ddb_regs.tf_r18, FCN_NULL },
	{ "r19",   (long *)&ddb_regs.tf_args[7], FCN_NULL },
	{ "r20",   (long *)&ddb_regs.tf_args[6], FCN_NULL },
	{ "r21",   (long *)&ddb_regs.tf_args[5], FCN_NULL },
	{ "r22",   (long *)&ddb_regs.tf_args[4], FCN_NULL },
	{ "r23",   (long *)&ddb_regs.tf_args[3], FCN_NULL },
	{ "r24",   (long *)&ddb_regs.tf_args[2], FCN_NULL },
	{ "r25",   (long *)&ddb_regs.tf_args[1], FCN_NULL },
	{ "r26",   (long *)&ddb_regs.tf_args[0], FCN_NULL },
	{ "r27",   (long *)&ddb_regs.tf_dp,   FCN_NULL },
	{ "r28",   (long *)&ddb_regs.tf_ret0, FCN_NULL },
	{ "r29",   (long *)&ddb_regs.tf_ret1, FCN_NULL },
	{ "r30",   (long *)&ddb_regs.tf_sp,   FCN_NULL },
	{ "r31",   (long *)&ddb_regs.tf_r31,  FCN_NULL },
	{ "sar",   (long *)&ddb_regs.tf_sar,  FCN_NULL },

	{ "rctr",  (long *)&ddb_regs.tf_rctr, FCN_NULL },
	{ "ccr",   (long *)&ddb_regs.tf_ccr,  FCN_NULL },
	{ "eirr",  (long *)&ddb_regs.tf_eirr, FCN_NULL },
	{ "eiem",  (long *)&ddb_regs.tf_eiem, FCN_NULL },
	{ "iir",   (long *)&ddb_regs.tf_iir,  FCN_NULL },
	{ "isr",   (long *)&ddb_regs.tf_isr,  FCN_NULL },
	{ "ior",   (long *)&ddb_regs.tf_ior,  FCN_NULL },
	{ "ipsw",  (long *)&ddb_regs.tf_ipsw, FCN_NULL },
	{ "iisqh", (long *)&ddb_regs.tf_iisq[0], FCN_NULL },
	{ "iioqh", (long *)&ddb_regs.tf_iioq[0], FCN_NULL },
	{ "iisqt", (long *)&ddb_regs.tf_iisq[1], FCN_NULL },
	{ "iioqt", (long *)&ddb_regs.tf_iioq[1], FCN_NULL },
	{ "ci",    (long *)&ddb_regs.tf_ci,   FCN_NULL },
	{ "vtop",  (long *)&ddb_regs.tf_vtop, FCN_NULL },
	{ "cr27",  (long *)&ddb_regs.tf_cr27, FCN_NULL },
	{ "cr30",  (long *)&ddb_regs.tf_cr30, FCN_NULL },

	{ "sr0",   (long *)&ddb_regs.tf_sr0,  FCN_NULL },
	{ "sr1",   (long *)&ddb_regs.tf_sr1,  FCN_NULL },
	{ "sr2",   (long *)&ddb_regs.tf_sr2,  FCN_NULL },
	{ "sr3",   (long *)&ddb_regs.tf_sr3,  FCN_NULL },
	{ "sr4",   (long *)&ddb_regs.tf_sr4,  FCN_NULL },
	{ "sr5",   (long *)&ddb_regs.tf_sr5,  FCN_NULL },
	{ "sr6",   (long *)&ddb_regs.tf_sr6,  FCN_NULL },
	{ "sr7",   (long *)&ddb_regs.tf_sr7,  FCN_NULL },

	{ "pidr1", (long *)&ddb_regs.tf_pidr1, FCN_NULL },
	{ "pidr2", (long *)&ddb_regs.tf_pidr2, FCN_NULL },
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);
int db_active = 0;

void
Debugger()
{
	__asm __volatile ("break %0, %1"
	    :: "i" (HPPA_BREAK_KERNEL), "i" (HPPA_BREAK_KGDB));
}

void
db_read_bytes(addr, size, data)
	vaddr_t addr;
	size_t size;
	char *data;
{
	register char *src = (char *)addr;

	while (size--)
		*data++ = *src++;
}

void
db_write_bytes(addr, size, data)
	vaddr_t addr;
	size_t size;
	char *data;
{
	register char *dst = (char *)addr;

	while (size--)
		*dst++ = *data++;

	/* unfortunately ddb does not provide any hooks for these */
	ficache(HPPA_SID_KERNEL, (vaddr_t)data, size);
	fdcache(HPPA_SID_KERNEL, (vaddr_t)data, size);
}


/*
 * Print trap reason.
 */
void
kdbprinttrap(type, code)
	int type, code;
{
	type &= ~T_USER;	/* just in case */
	db_printf("kernel: ");
	if (type >= trap_types || type < 0)
		db_printf("type 0x%x", type);
	else
		db_printf("%s", trap_type[type]);
	db_printf(" trap, code=0x%x\n", code);
}

/*
 *  kdb_trap - field a BPT trap
 */
int
kdb_trap(type, code, regs)
	int type, code;
	db_regs_t *regs;
{
	extern label_t *db_recover;
	int s;

	switch (type) {
	case T_IBREAK:
	case T_DBREAK:
	case -1:
		break;
	default:
		if (!db_panic)
			return (0);

		kdbprinttrap(type, code);
		if (db_recover != 0) {
			db_error("Caught exception in DDB; continuing...\n");
			/* NOT REACHED */
		}
	}

	/* XXX Should switch to kdb`s own stack here. */

	s = splhigh();
	bcopy(regs, &ddb_regs, sizeof(ddb_regs));
	db_active++;
	cnpollc(TRUE);
	db_trap(type, code);
	cnpollc(FALSE);
	db_active--;
	bcopy(&ddb_regs, regs, sizeof(*regs));
	splx(s);

	return (1);
}

/*
 *  Validate an address for use as a breakpoint.
 *  Any address is allowed for now.
 */
int
db_valid_breakpoint(addr)
	db_addr_t addr;
{
	return (1);
}

void
db_stack_trace_print(addr, have_addr, count, modif, pr)
	db_expr_t	addr;
	int		have_addr;
	db_expr_t	count;
	char		*modif;
	int		(*pr)(const char *, ...);
{
	register_t *fp, pc, rp, *argp;
	db_sym_t sym;
	db_expr_t off;
	char *name;
	char **argnp, *argnames[8];
	int nargs;

	if (count < 0)
		count = 65536;

	if (!have_addr) {
		fp = (register_t *)ddb_regs.tf_r3;
		pc = ddb_regs.tf_iioq[0];
		rp = ddb_regs.tf_rp;
	} else {
		fp = (register_t *)addr;
		pc = 0;
		rp = ((register_t *)fp)[-5];
	}

#ifdef DDB_DEBUG
	(*pr) (">> %p, 0x%lx, 0x%lx\t", fp, pc, rp);
#endif
	while (fp && count--) {

		if (USERMODE(pc))
			return;

		sym = db_search_symbol(pc, DB_STGY_ANY, &off);
		db_symbol_values (sym, &name, NULL);

		(*pr)("%s(", name);

		/* args */
		nargs = 8;
		argnp = NULL;
		if (db_sym_numargs(sym, &nargs, argnames))
			argnp = argnames;
		else
			nargs = 4;
		/*
		 * XXX first eight args are passed on registers, and may not
		 * be stored on stack, dunno how to recover their values yet
		 */
		for (argp = &fp[-9]; nargs--; argp--) {
			if (argnp)
				(*pr)("%s=", *argnp++);
			(*pr)("%x%s", db_get_value((long)argp, 8, FALSE),
				  nargs? ",":"");
		}
		(*pr)(") at ");
		db_printsym(pc, DB_STGY_PROC, pr);
		(*pr)("\n");

		/* TODO: print locals */

		/* next frame */
		pc = rp;
		rp = fp[-2];

		/* if a terminal frame and not a start of a page
		 * then skip the trapframe and the terminal frame */
		if (!fp[0]) {
			struct trapframe *tf;

			tf = (struct trapframe *)((char *)fp - sizeof(*tf));

			if (tf->tf_flags & TFF_SYS)
				(*pr)("-- syscall #%d(%lx, %lx, %lx, %lx, ...)\n",
				    tf->tf_r1, tf->tf_args[0], tf->tf_args[1],
				    tf->tf_args[2], tf->tf_args[3],
				    tf->tf_args[4], tf->tf_args[5],
				    tf->tf_args[6], tf->tf_args[7]);
			else
				(*pr)("-- trap #%d%s\n", tf->tf_flags & 0x3f,
				    (tf->tf_flags & T_USER)? " from user" : "");

			if (!(tf->tf_flags & TFF_LAST)) {
				fp = (register_t *)tf->tf_r3;
				pc = tf->tf_iioq[0];
				rp = tf->tf_rp;
			} else
				fp = 0;
		} else
			fp = (register_t *)fp[0];
#ifdef DDB_DEBUG
		(*pr) (">> %p, 0x%lx, 0x%lx\t", fp, pc, rp);
#endif
	}

	if (count && pc) {
		db_printsym(pc, DB_STGY_XTRN, pr);
		(*pr)(":\n");
	}
}
