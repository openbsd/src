/*	$OpenBSD: db_interface.c,v 1.3 2016/03/05 17:24:27 mpi Exp $	*/
/*      $NetBSD: db_interface.c,v 1.12 2001/07/22 11:29:46 wiz Exp $ */

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <dev/cons.h>

#include <machine/db_machdep.h>
#include <ddb/db_extern.h>

int db_trap_glue(struct trapframe *frame); /* called from locore */

void
Debugger()
{
	ddb_trap();
}

int
db_trap_glue(struct trapframe *frame)
{
	if (!(frame->srr1 & PSL_PR)
	    && (frame->exc == EXC_TRC
		|| (frame->exc == EXC_PGM && (frame->srr1 & 0x20000))
		|| frame->exc == EXC_BPT)) {

		bcopy(frame->fixreg, DDB_REGS->fixreg,
			32 * sizeof(u_int32_t));
		DDB_REGS->srr0 = frame->srr0;
		DDB_REGS->srr1 = frame->srr1;

		cnpollc(TRUE);
		db_trap(T_BREAKPOINT, 0);
		cnpollc(FALSE);

		bcopy(DDB_REGS->fixreg, frame->fixreg,
			32 * sizeof(u_int32_t));

		return 1;
	}
	return 0;
}
