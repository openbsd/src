/*	$OpenBSD: fpgetround.S,v 1.2 2004/02/01 05:40:52 drahn Exp $	*/
/* BROKEN */

#include <machine/asm.h>

ENTRY(fpgetround)
	mov	r0, #0x0
	mov	pc, lr		/* return */
