/*	$OpenBSD: putchar.c,v 1.1 2001/06/26 21:58:05 smurph Exp $ */

/*
 * putchar: easier to do this with outstr than to add more macros to
 * handle byte passing on the stack
 */

#include <sys/types.h>
#include <machine/prom.h>

#include "stand.h"
#include "libbug.h"

void
putchar(c)
	int c;
{
	char ca;
	ca = (char)c & 0xFF;
	if (ca == '\n')
		putchar('\r');
	
	asm  volatile ("mr 3, %0" :: "r" (ca));
	MVMEPROM_CALL(MVMEPROM_OUTCHR);
}
