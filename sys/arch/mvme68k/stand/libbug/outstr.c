/*	$OpenBSD: outstr.c,v 1.1 1996/05/07 11:25:10 deraadt Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

void
mvmeprom_outstr(start, end)
	char *start, *end;
{
	MVMEPROM_ARG1(start);
	MVMEPROM_ARG2(end);
	MVMEPROM_CALL(MVMEPROM_OUTSTR);
}
