/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS) 
	.text
	.asciz "$OpenBSD: flt_rounds.S,v 1.2 1996/08/19 08:14:15 tholo Exp $"
#endif /* LIBC_SCCS */

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
