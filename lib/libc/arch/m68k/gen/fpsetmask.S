/* $OpenBSD: fpsetmask.S,v 1.3 2005/08/07 16:40:14 espie Exp $ */
/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public Domain.
 */

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
