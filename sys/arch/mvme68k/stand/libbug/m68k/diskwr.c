/*	$OpenBSD: diskwr.c,v 1.2 1996/04/28 10:48:38 deraadt Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

/* returns 0: success, nonzero: error */
int
mvmeprom_diskwr(arg)
	struct mvmeprom_dskio *arg;
{
	int ret;

	asm volatile ("movel %0, sp@-"::"d" (arg));
	MVMEPROM_CALL(MVMEPROM_DSKWR);
	asm volatile ("movew ccr,%0": "=d" (ret));
	return (!(ret & 0x4));
}
