/*	$OpenBSD: getbrdid.c,v 1.2 2001/07/04 08:09:28 niklas Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

/* BUG - query board routines */
struct mvmeprom_brdid *
mvmeprom_brdid()
{
	struct mvmeprom_brdid *id;

	MVMEPROM_CALL(MVMEPROM_GETBRDID);
	asm volatile ("or %0,r0,r2": "=r" (id):);
	return (id);
}
