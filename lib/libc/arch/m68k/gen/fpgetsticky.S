/* $OpenBSD: fpgetsticky.S,v 1.3 2005/08/07 16:40:14 espie Exp $ */
/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(fpgetsticky)
	fmovel fpsr,d0
	bfextu d0{#24:#5},d0
	rts
