/*	$OpenBSD: outstr.c,v 1.2 1996/04/28 10:48:45 deraadt Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

void
mvmeprom_outstr(start, end)
	char *start, *end;
{
	asm volatile ("movl %0, sp@-" : "=a" (start));
	asm volatile ("movl %0, sp@-" : "=a" (end));
	MVMEPROM_CALL(MVMEPROM_OUTSTR);
}
