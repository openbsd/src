/*	$OpenBSD: htons.S,v 1.2 2004/02/10 00:08:11 drahn Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(_C_LABEL(htons))
	movl    %edi,%eax
	xchgb   %ah,%al
	ret

