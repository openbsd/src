/*
 * Written by Charles M. Hannum, Apr 9, 1995
 * Public domain.
 */

#include <machine/asm.h>

ENTRY(fpgetsticky)
	fmovel fpsr,d0
	bfextu d0{#24:#5},d0
	rts
