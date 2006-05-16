/*	$OpenBSD: putchar.c,v 1.4 2006/05/16 22:51:30 miod Exp $ */

/*
 * putchar: easier to do this with outstr than to add more macros to
 * handle byte passing on the stack
 */

#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "prom.h"

void
putchar(c)
	int c;
{
	char ca;
	ca = (char)c & 0xFF;
	if (ca == '\n')
		putchar('\r');
	asm  volatile ("or r2,r0,%0" : : "r" (ca));
	MVMEPROM_CALL(MVMEPROM_OUTCHR);
}
