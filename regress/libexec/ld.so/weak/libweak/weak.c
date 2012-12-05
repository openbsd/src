/*	$OpenBSD: weak.c,v 1.3 2012/12/05 23:20:08 deraadt Exp $	*/

/*
 * Public domain. 2002, Federico Schwindt <fgsch@openbsd.org>.
 */

#include <sys/types.h>
#include "defs.h"

int
weak_func()
{
	return (WEAK_REF);
}

__weak_alias(func,weak_func);
