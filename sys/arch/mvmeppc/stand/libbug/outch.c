/*	$OpenBSD: outch.c,v 1.3 2004/01/24 21:12:38 miod Exp $	*/

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>

#include "libbug.h"

void
mvmeprom_outchr(a)
	char a;
{
	asm volatile ("mr 3, %0" :  :"r" (a));
	MVMEPROM_CALL(MVMEPROM_OUTCHR);
}

