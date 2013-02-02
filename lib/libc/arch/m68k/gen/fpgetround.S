/* $OpenBSD: fpgetround.S,v 1.4 2013/02/02 13:29:14 miod Exp $ */
/*
 * Written by J.T. Conklin, Apr 6, 1995
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(fpgetround)
	fmovel %fpcr,%d0
	bfextu %d0{#26:#2},%d0
	rts
