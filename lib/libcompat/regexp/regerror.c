/*	$OpenBSD: regerror.c,v 1.4 2000/06/30 16:00:06 millert Exp $	*/
#ifndef lint
static char *rcsid = "$OpenBSD: regerror.c,v 1.4 2000/06/30 16:00:06 millert Exp $";
#endif /* not lint */

#include <regexp.h>
#include <stdio.h>

static void (*_new_regerror)() = NULL;

void
v8_regerror(s)
	const char *s;
{
	if (_new_regerror != NULL)
		_new_regerror(s);
	else
		warnx("%s", s);
	return;
}

void
v8_setregerror(f)
	void (*f)();
{
	_new_regerror = f;
}
