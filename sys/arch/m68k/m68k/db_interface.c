/*	$NetBSD: db_interface.c,v 1.16 1995/06/09 20:03:05 leo Exp $	*/

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
 * Interface to new debugger.
 */
#include <sys/param.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/systm.h> /* just for boothowto --eichin */
#include <setjmp.h>

#include <vm/vm.h>

#include <machine/trap.h>
#include <machine/db_machdep.h>

short	exframesize[];
extern jmp_buf	*db_recover;

int	db_active = 0;
int ddb_regs_ssp;	/* system stack pointer */

/*
 * Received keyboard interrupt sequence.
 */
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
kdb_trap(type, regs)
	int	type;
	register struct mc68020_saved_state *regs;
{
	int fsize;

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
		return (0);
	}

	/* XXX - Should switch to kdb's own stack here. */

	ddb_regs = *regs;

	/* Get System Stack Pointer (SSP) */
	ddb_regs_ssp = (int)(&regs[1]);
	fsize = exframesize[regs->stkfmt];
	if (fsize > 0)
		ddb_regs_ssp += fsize;

	db_active++;
	cnpollc(TRUE);	/* set polling mode, unblank video */

	db_trap(type, 0);	/* where the work happens */

	cnpollc(FALSE);	/* resume interrupt mode */
	db_active--;

	/* Can't easily honor change in ssp.  Oh well. */

	*regs = ddb_regs;

	/*
	 * Indicate that single_step is for KDB.
	 * But lock out interrupts to prevent TRACE_KDB from setting the
	 * trace bit in the current SR (and trapping while exiting KDB).
	 */
	(void) spl7();
#if 0
	if (!USERMODE(regs->sr) && (regs->sr & SR_T1) && (current_thread())) {
		current_thread()->pcb->pcb_flag |= TRACE_KDB;
	}
	if ((regs->sr & SR_T1) && (current_thread())) {
		current_thread()->pcb->flag |= TRACE_KDB;
	}
#endif

	return(1);
}

extern char *trap_type[];
extern int trap_types;

/*
 * Print trap reason.
 */
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

int
Debugger()
{
	asm ("trap #15");
}

#if !defined(sun3) && !defined(amiga) && !defined(atari)

/*
 * Read bytes from kernel address space for debugger.
 * XXX - Each port should provide one of these...
 * (See arch/sun3/sun3/db_machdep.c for example.)
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
 * XXX - Each port should provide one of these...
 * (See arch/sun3/sun3/db_machdep.c for example.)
 */
void
db_write_bytes(addr, size, data)
	vm_offset_t	addr;
	register int	size;
	register char	*data;
{
	register char	*dst;

	int		oldmap0 = 0;
	int		oldmap1 = 0;
	vm_offset_t	addr1;
	extern char	etext;

	dst = (char *)addr;
	while (--size >= 0)
		*dst++ = *data++;

}
#endif	/* !defined(sun3) && !defined(amiga) && !defined(atari) */
