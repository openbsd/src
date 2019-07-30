/*	$OpenBSD: htonl.S,v 1.2 2015/05/29 09:25:28 uebayasi Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(htonl)
	movl    %edi,%eax
	bswapl  %eax
	ret
END(htonl)
