/*	$OpenBSD: inchr.c,v 1.2 2003/09/07 21:35:35 miod Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "prom.h"

/* returns 0 if no characters ready to read */
int
getchar()
{
	int ret;

	MVMEPROM_CALL(MVMEPROM_INCHR);
	asm volatile ("or %0,r0,r2" :  "=r" (ret));
	return ret;
}
