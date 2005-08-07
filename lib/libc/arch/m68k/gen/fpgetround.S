/* $OpenBSD: fpgetround.S,v 1.3 2005/08/07 16:40:14 espie Exp $ */
/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(fpgetround)
	fmovel fpcr,d0
	bfextu d0{#26:#2},d0
	rts
