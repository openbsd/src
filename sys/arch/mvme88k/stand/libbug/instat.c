/*	$OpenBSD: instat.c,v 1.4 2013/01/05 11:20:56 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>
#include "prom.h"

/* returns 0 if no characters ready to read */
int
mvmeprom_instat()
{
	short ret;

	MVMEPROM_CALL(MVMEPROM_INSTAT);
	asm volatile ("or %0,%%r0,%%r2" :  "=r" (ret));
	return (!(ret & 0x4));
}
