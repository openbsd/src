/*	$OpenBSD: inchr.c,v 1.1 2001/06/26 21:58:04 smurph Exp $ */

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
	asm volatile ("mr %0, 3" :  "=r" (ret));
	return ret;
}
