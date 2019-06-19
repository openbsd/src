/*	$OpenBSD: ntohs.S,v 1.4 2018/07/03 23:14:05 mortimer Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(ntohs)
	RETGUARD_SETUP(ntohs, r11)
	movl    %edi,%eax
	xchgb   %ah,%al
	RETGUARD_CHECK(ntohs, r11)
	ret
END(ntohs)
