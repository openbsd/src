/*	$OpenBSD: db_interface.c,v 1.5 1996/05/09 22:30:10 niklas Exp $	*/
/*	$NetBSD: db_interface.c,v 1.20 1996/04/29 20:50:28 leo Exp $	*/

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

/*
 * Interface to the "ddb" kernel debugger.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h> /* just for boothowto --eichin */

#include <vm/vm.h>

#include <dev/cons.h>

#include <machine/trap.h>
#include <machine/db_machdep.h>

#include <ddb/db_command.h>
#include <ddb/db_sym.h>
#include <ddb/db_extern.h>


extern label_t	*db_recover;

int	db_active = 0;

static void kdbprinttrap __P((int, int));

/*
 * Received keyboard interrupt sequence.
 */
void
kdb_kintr(regs)
	register struct mc68020_saved_state *regs;
{
	if (db_active == 0 && (boothowto & RB_KDB)) {
		printf("\n\nkernel: keyboard interrupt\n");
		kdb_trap(-1, regs);
	}
}

/*
 * kdb_trap - field a TRACE or BPT trap
 * Return non-zero if we "handled" the trap.
 */
int
kdb_trap(type, regs)
	int	type;
	register struct mc68020_saved_state *regs;
{

	switch (type) {
	case T_TRACE:		/* single-step */
	case T_BREAKPOINT:	/* breakpoint */
/*      case T_WATCHPOINT:*/
		break;
	case -1:
		break;
	default:
		kdbprinttrap(type, 0);
		if (db_recover != 0) {
			/* This will longjmp back to db_command_loop */
			db_error("Caught exception in ddb.\n");
			/*NOTREACHED*/
		}
		/*
		 * Tell caller "We did NOT handle the trap."
		 * Caller should panic or whatever.
		 */
		return (0);
	}

	/*
	 * We'd like to be on a separate debug stack here, but
	 * that's easier to do in locore.s before we get here.
	 * See sun3/locore.s:T_TRACE for stack switch code.
	 */

	ddb_regs = *regs;

	db_active++;
	cnpollc(TRUE);	/* set polling mode, unblank video */

	db_trap(type, 0);	/* where the work happens */

	cnpollc(FALSE);	/* resume interrupt mode */
	db_active--;

	*regs = ddb_regs;

	/*
	 * Indicate that single_step is for KDB.
	 * But lock out interrupts to prevent TRACE_KDB from setting the
	 * trace bit in the current SR (and trapping while exiting KDB).
	 */
	(void) spl7();

	/*
	 * Tell caller "We HAVE handled the trap."
	 * Caller will return to locore and rte.
	 */
	return(1);
}

extern char *trap_type[];
extern int trap_types;

/*
 * Print trap reason.
 */
static void
kdbprinttrap(type, code)
	int	type, code;
{
	printf("kernel: ");
	if (type >= trap_types || type < 0)
		printf("type %d", type);
	else
		printf("%s", trap_type[type]);
	printf(" trap\n");
}

void
Debugger()
{
	asm ("trap #15");
}

