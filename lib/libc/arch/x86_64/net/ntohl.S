/*	$OpenBSD: ntohl.S,v 1.2 2004/02/10 00:08:11 drahn Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(_C_LABEL(ntohl))
	movl    %edi,%eax
	bswapl  %eax
	ret
