/*	$OpenBSD: return.c,v 1.1 1996/05/07 11:25:11 deraadt Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

/* BUG - return to bug routine */
void
mvmeprom_return()
{
	MVMEPROM_CALL(MVMEPROM_EXIT);
	/*NOTREACHED*/
}
