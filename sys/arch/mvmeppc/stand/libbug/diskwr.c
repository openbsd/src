/*	$OpenBSD: diskwr.c,v 1.3 2004/01/24 21:12:38 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>

#include "libbug.h"

/* returns 0: success, nonzero: error */
int
mvmeprom_diskwr(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;

	asm volatile ("mr 3, %0" :: "r"(arg));
	MVMEPROM_CALL(MVMEPROM_DSKWR);
	asm volatile ("mr %0, 3" :  "=r" (ret));
	return ((ret & 0x8));
}
