/*	$OpenBSD: outln.c,v 1.2 1996/04/28 10:48:44 deraadt Exp $ */

/*
 * bug routines -- assumes that the necessary sections of memory
 * are preserved.
 */
#include <sys/types.h>
#include <machine/prom.h>

void
mvmeprom_outln(start, end)
	char *start, *end;
{
	asm volatile ("movl %0, sp@-" : "=a" (start));
	asm volatile ("movl %0, sp@-" : "=a" (end));
	MVMEPROM_CALL(MVMEPROM_OUTSTRCRLF);
}
