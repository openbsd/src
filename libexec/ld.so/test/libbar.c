/*	$OpenBSD: libbar.c,v 1.2 2001/01/28 19:34:29 niklas Exp $	*/

#include <stdio.h>

extern void dltest(const char *);
extern void dep(const char *s);

const char *const libname = "libbar.so";

void bar(const char *s)
{
	dltest("called from libbar.");
	printf("libbar: %s\n", s);
	dep("!olleH!");
}

