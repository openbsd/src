/*	$OpenBSD: regerror.c,v 1.7 2009/10/27 23:59:28 deraadt Exp $	*/
#include <err.h>
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
