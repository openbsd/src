/*	$OpenBSD: main.c,v 1.1.1.1 2002/02/10 22:51:41 fgsch Exp $	*/

/*
 * Public domain. 2002, Federico Schwindt <fgsch@openbsd.org>.
 */

#include <err.h>
#include "defs.h"

int
main(int argc, char **argv)
{
	if (weak_func() != WEAK_REF)
		err(1, "error calling weak reference");

	if (func() != STRONG_REF)
		err(1, "error calling strong reference");

	return (0);
}
