/* $OpenBSD: flt_rounds.S,v 1.3 2005/08/07 16:40:14 espie Exp $ */
/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#include <machine/asm.h>

	.text
	.even
_map:
	.byte 1		/* round to nearest */
	.byte 0		/* round to zero */
	.byte 3		/* round to negative infinity */
	.byte 2		/* round to positive infinity */

ENTRY(__flt_rounds)
	fmovel fpcr,d0
	bfextu d0{#26:#2},d0
	lea _map,a0
	moveb a0@(d0:l:1),d0
	rts
