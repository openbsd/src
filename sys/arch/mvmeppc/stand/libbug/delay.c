/*	$OpenBSD: delay.c,v 1.3 2004/01/24 21:12:38 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>

#include "libbug.h"

/* BUG - timing routine */
void
mvmeprom_delay(msec)
	int msec; /* This is r3 */
{
	asm volatile ("mr 3, %0" :: "r"(msec));
	MVMEPROM_CALL(MVMEPROM_DELAY);
}
