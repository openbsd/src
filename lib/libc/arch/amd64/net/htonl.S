/*	$OpenBSD: htonl.S,v 1.3 2018/07/03 23:14:05 mortimer Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(htonl)
	RETGUARD_SETUP(htonl, r11)
	movl    %edi,%eax
	bswapl  %eax
	RETGUARD_CHECK(htonl, r11)
	ret
END(htonl)
