/*	$OpenBSD: inchr.c,v 1.2 2004/01/24 21:12:38 miod Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>

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
