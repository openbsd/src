/*	$OpenBSD: getbrdid.c,v 1.3 2004/01/24 21:12:38 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>

#include "libbug.h"

/* BUG - query board routines */
struct mvmeprom_brdid *
mvmeprom_brdid()
{
	struct mvmeprom_brdid *id;

	MVMEPROM_CALL(MVMEPROM_BRD_ID);
	asm volatile ("mr %0, 3": "=r" (id):);
	return (id);
}
