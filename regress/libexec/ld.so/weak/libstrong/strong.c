/*	$OpenBSD: strong.c,v 1.1.1.1 2002/02/10 22:51:41 fgsch Exp $	*/

/*
 * Public domain. 2002, Federico Schwindt <fgsch@openbsd.org>.
 */

#include <sys/cdefs.h>
#include "defs.h"

int
func()
{
	return (STRONG_REF);
}
