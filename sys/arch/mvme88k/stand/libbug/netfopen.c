/*	$OpenBSD: netfopen.c,v 1.1 2004/01/28 00:27:44 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "prom.h"

/* returns 0: success, nonzero: error */
int
mvmeprom_netfopen(arg)
	struct mvmeprom_netfopen *arg;
{
	asm volatile ("or r2,r0,%0": : "r" (arg));
	MVMEPROM_CALL(MVMEPROM_NETFOPEN);
	return (arg->status);
}
