/*	$OpenBSD: libfoo.c,v 1.2 2001/01/28 19:34:29 niklas Exp $	*/

#include <stdio.h>

extern void dltest(const char *s);
const char *const libname = "libfoo.so";

void
foo(const char *s)
{
	const char *saved = s;

	dltest("called from libfoo");
	printf("libfoo: ");
	for(; *s; s++);
	for(s--; s>= saved; s--) {
		putchar(*s);
	}
	putchar('\n');
}
