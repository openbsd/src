/*	$OpenBSD: regerror.c,v 1.2 1996/07/24 05:39:11 downsj Exp $	*/
#ifndef lint
static char *rcsid = "$OpenBSD: regerror.c,v 1.2 1996/07/24 05:39:11 downsj Exp $";
#endif /* not lint */

#include <regexp.h>
#include <stdio.h>

void
v8_regerror(s)
const char *s;
{
	warnx(s);
	return;
}
