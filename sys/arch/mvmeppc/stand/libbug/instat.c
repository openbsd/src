/*	$OpenBSD: instat.c,v 1.3 2004/01/24 21:12:38 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>

#include "libbug.h"

/* returns 0 if no characters ready to read */
int
mvmeprom_instat()
{
	int ret;

	MVMEPROM_CALL(MVMEPROM_INSTAT);
	asm volatile ("mr %0, 3" :  "=r" (ret));
	return (!(ret & 0x4));
}
