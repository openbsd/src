/*	$OpenBSD: instat.c,v 1.1 1996/05/07 11:25:09 deraadt Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

/* returns 0 if no characters ready to read */
int
mvmeprom_instat()
{
	int ret;

	MVMEPROM_CALL(MVMEPROM_INSTAT);
	MVMEPROM_STATRET(ret);
}
