/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: fpgetsticky.S,v 1.2 1996/08/19 08:14:19 tholo Exp $"
#endif /* LIBC_SCCS */

#include <machine/asm.h>

ENTRY(fpgetsticky)
	fmovel fpsr,d0
	bfextu d0{#24:#5},d0
	rts
