/* $OpenBSD: fpgetmask.S,v 1.4 2013/02/02 13:29:14 miod Exp $ */
/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(fpgetmask)
	fmovel %fpcr,%d0
	bfextu %d0{#18:#5},%d0
	rts
