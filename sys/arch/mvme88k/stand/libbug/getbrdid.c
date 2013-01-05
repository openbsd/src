/*	$OpenBSD: getbrdid.c,v 1.4 2013/01/05 11:20:56 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>
#include "prom.h"

/* BUG - query board routines */
struct mvmeprom_brdid *
mvmeprom_brdid()
{
	struct mvmeprom_brdid *id;

	MVMEPROM_CALL(MVMEPROM_GETBRDID);
	asm volatile ("or %0,%%r0,%%r2": "=r" (id):);
	return (id);
}
