/*	$NetBSD: db_machdep.c,v 1.2 1995/07/05 09:54:09 ragge Exp $	*/

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
 *
 *	db_interface.c,v 2.4 1991/02/05 17:11:13 mrt (CMU)
 */

/*
 * Interface to new debugger.
 * Taken from i386 port and modified for vax.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h> /* just for boothowto --eichin */

#include <ddb/db_variables.h>

#include <vm/vm.h>

#include <machine/db_machdep.h>
#include <machine/trap.h>
#include <machine/../vax/gencons.h>

#include <setjmp.h>

extern jmp_buf	*db_recover;

int	db_active = 0;

/*
 *  kdb_trap - field a TRACE or BPT trap
 */
int
kdb_trap(frame)
	struct trapframe *frame;
{
	int s;

#if 0
	if ((boothowto&RB_KDB) == 0)
		return(0);
#endif

	switch (frame->trap) {
	case T_BPTFLT:	/* breakpoint */
	case T_TRCTRAP:	/* single_step */
		break;
	default:
		kdbprinttrap(frame->trap, frame->code);
		if (db_recover != 0) {
			db_error("Faulted in DDB; continuing...\n");
			/*NOTREACHED*/
		}
	}
	bcopy(frame, &ddb_regs, sizeof(struct trapframe));

	/* XXX Should switch to interrupt stack here. */


	s = splddb();
	mtpr(0, PR_RXCS);
	mtpr(0, PR_TXCS);
	db_active++;
	db_trap(frame->trap, frame->code);
	db_active--;
	mtpr(GC_RIE, PR_RXCS);
	mtpr(GC_TIE, PR_TXCS);
	splx(s);

	bcopy(&ddb_regs, frame, sizeof(struct trapframe));

	return (1);
}

extern char *traptypes[];
extern int no_traps;

/*
 * Print trap reason.
 */
kdbprinttrap(type, code)
	int type, code;
{
	db_printf("kernel: ");
	if (type >= no_traps || type < 0)
		db_printf("type %d", type);
	else
		db_printf("%s", traptypes[type]);
	db_printf(" trap, code=%x\n", code);
}

/*
 * Read bytes from kernel address space for debugger.
 */
void
db_read_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*src;

	src = (char *)addr;
	while (--size >= 0)
		*data++ = *src++;
}

/*
 * Write bytes to kernel address space for debugger.
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*dst;

	dst = addr;
	for (;size;size--)
		*dst++ = *data++;
}

int
Debugger()
{
	int s = splx(0xe); /* Is this good? We must lower anyway... */
	mtpr(0xf, PR_SIRR); /* beg for debugger */
	splx(s);
}

/*
 * Machine register set.
 * XXX - lost stackpointer.
 */
struct db_variable db_regs[] = {
	"r0",	&ddb_regs.r0,	FCN_NULL,
	"r1",	&ddb_regs.r1,	FCN_NULL,
	"r2",	&ddb_regs.r2,	FCN_NULL,
	"r3",	&ddb_regs.r3,	FCN_NULL,
	"r4",	&ddb_regs.r4,	FCN_NULL,
	"r5",	&ddb_regs.r5,	FCN_NULL,
	"r6",	&ddb_regs.r6,	FCN_NULL,
	"r7",	&ddb_regs.r7,	FCN_NULL,
	"r8",	&ddb_regs.r8,	FCN_NULL,
	"r9",	&ddb_regs.r9,	FCN_NULL,
	"r10",	&ddb_regs.r10,	FCN_NULL,
	"r11",	&ddb_regs.r11,	FCN_NULL,
	"ap",	&ddb_regs.ap,	FCN_NULL,
	"fp",	&ddb_regs.fp,	FCN_NULL,
	"pc",	&ddb_regs.pc,	FCN_NULL,
	"psl",	&ddb_regs.psl,	FCN_NULL,
};
struct db_variable *db_eregs = db_regs + sizeof(db_regs)/sizeof(db_regs[0]);

void
db_stack_trace_cmd(addr, have_addr, count, modif)
        db_expr_t       addr;
        boolean_t       have_addr;
        db_expr_t       count;
        char            *modif;
{
	printf("db_stack_trace_cmd - addr %x, have_addr %x, count %x, modif %x\n",addr, have_addr, count, modif);
}

static int ddbescape = 0;

int
kdbrint(tkn)
	int tkn;
{

	if (ddbescape && ((tkn & 0x7f) == 'D')) {
		mtpr(0xf, PR_SIRR);
		return 1;
	}

	if ((ddbescape == 0) && ((tkn & 0x7f) == 27)) {
		ddbescape = 1;
		return 1;
	}

	if (ddbescape) {
		ddbescape = 0;
		return 2;
	}
	
	ddbescape = 0;
	return 0;
}


