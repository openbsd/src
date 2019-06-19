/*	$OpenBSD: ntohl.S,v 1.4 2018/07/03 23:14:05 mortimer Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(ntohl)
	RETGUARD_SETUP(ntohl, r11)
	movl    %edi,%eax
	bswapl  %eax
	RETGUARD_CHECK(ntohl, r11)
	ret
END(ntohl)
