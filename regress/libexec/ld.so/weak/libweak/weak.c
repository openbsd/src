/*	$OpenBSD: weak.c,v 1.1.1.1 2002/02/10 22:51:41 fgsch Exp $	*/

/*
 * Public domain. 2002, Federico Schwindt <fgsch@openbsd.org>.
 */

#include <sys/cdefs.h>
#include "defs.h"

__weak_alias(func,weak_func);

int
weak_func()
{
	return (WEAK_REF);
}
