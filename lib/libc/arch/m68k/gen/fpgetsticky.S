/* $OpenBSD: fpgetsticky.S,v 1.4 2013/02/02 13:29:14 miod Exp $ */
/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(fpgetsticky)
	fmovel %fpsr,%d0
	bfextu %d0{#24:#5},%d0
	rts
