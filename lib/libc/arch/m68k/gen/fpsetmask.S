/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public Domain.
 */

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: fpsetmask.S,v 1.2 1996/08/19 08:14:20 tholo Exp $"
#endif /* LIBC_SCCS */

#include <machine/asm.h>

ENTRY(fpsetmask)
	movel d2,sp@-
	fmovel fpcr,d1
	movel sp@(8),d2
	bfextu d1{#18:#5},d0
	bfins d2,d1{#18:#5}
	movel sp@+,d2
	fmovel d1,fpcr
	rts
