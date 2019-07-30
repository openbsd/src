/*	$OpenBSD: ntohl.S,v 1.3 2015/05/29 09:25:28 uebayasi Exp $	*/

/* Written by Artur Grabowski. Public Domain */

#include <machine/asm.h>

ENTRY(ntohl)
	movl    %edi,%eax
	bswapl  %eax
	ret
END(ntohl)
