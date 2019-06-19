/*	$OpenBSD: htons.S,v 1.4 2018/07/03 23:14:05 mortimer Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(htons)
	RETGUARD_SETUP(htons, r11)
	movl    %edi,%eax
	xchgb   %ah,%al
	RETGUARD_CHECK(htons, r11)
	ret
END(htons)
