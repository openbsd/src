/*	$OpenBSD: inchr.c,v 1.1 1998/08/22 07:39:55 smurph Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "libbug.h"

/* returns 0 if no characters ready to read */
int
getchar()
{
	int ret;

	MVMEPROM_CALL(MVMEPROM_INCHR);
	asm volatile ("or %0,r0,r2" :  "=r" (ret));
	return ret;
}
