/*	$OpenBSD: delay.c,v 1.2 1996/05/16 02:25:41 chuck Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "libbug.h"

/* BUG - timing routine */
void
mvmeprom_delay(msec)
	int msec;
{
	MVMEPROM_ARG1(msec);
	MVMEPROM_CALL(MVMEPROM_DELAY);
}
