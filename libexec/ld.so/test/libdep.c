/*	$OpenBSD: libdep.c,v 1.2 2001/01/28 19:34:29 niklas Exp $	*/

#include <stdio.h>
const char *const libname = "libdep.so";


void
dep(const char *s)
{
	const char *saved = s;

	printf("libdep: ");
	for(; *s; s++);
	for(s--; s>= saved; s--) {
		putchar(*s);
		putchar('*');
	}
	putchar('\n');
}
