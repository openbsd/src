/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#if defined(LIBC_SCCS)
	.text
	.asciz "$OpenBSD: fpgetround.S,v 1.2 1996/08/19 08:14:18 tholo Exp $"
#endif /* LIBC_SCCS */

#include <machine/asm.h>

ENTRY(fpgetround)
	fmovel fpcr,d0
	bfextu d0{#26:#2},d0
	rts
