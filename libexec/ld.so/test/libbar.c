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

