/*	$OpenBSD: regerror.c,v 1.5 2003/06/26 23:19:32 deraadt Exp $	*/
#ifndef lint
static char *rcsid = "$OpenBSD: regerror.c,v 1.5 2003/06/26 23:19:32 deraadt Exp $";
#endif /* not lint */

#include <regexp.h>
#include <stdio.h>

static void (*_new_regerror)(const char *) = NULL;

void
v8_regerror(const char *s)
{
	if (_new_regerror != NULL)
		_new_regerror(s);
	else
		warnx("%s", s);
	return;
}

void
v8_setregerror(void (*f)(const char *))
{
	_new_regerror = f;
}
