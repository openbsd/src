/*	$OpenBSD: biosmem.S,v 1.2 1997/04/09 08:39:24 mickey Exp $	*/

#include <machine/asm.h>
#define	_LOCORE
#include "biosdev.h"
#undef	_LOCORE

ENTRY(biosmem)
	BIOSINT(0x12)
	ret
