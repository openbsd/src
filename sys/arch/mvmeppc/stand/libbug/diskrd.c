/*	$OpenBSD: diskrd.c,v 1.2 2001/07/04 08:31:34 niklas Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

/* returns 0: success, nonzero: error */
int
mvmeprom_diskrd(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;

	asm volatile ("mr 3, %0" :: "r"(arg));
	MVMEPROM_CALL(MVMEPROM_NETRD);
	asm volatile ("mr %0, 3" :  "=r" (ret));
	return ((ret & 0x8));
}
