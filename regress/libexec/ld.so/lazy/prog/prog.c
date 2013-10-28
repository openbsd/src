/*	$OpenBSD: prog.c,v 1.2 2013/10/28 08:40:05 jsg Exp $ */
/* Public Domain, 2008, Matthieu Herrb */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

void *handle = NULL;

typedef int (*foofunc)(void);

int
main(int argc, char *argv[])
{
	foofunc foo;
	int i;

	printf("loading: %s\n", FOO);
	handle = dlopen(FOO, RTLD_LAZY|RTLD_GLOBAL);
	if (handle == NULL) {
		errx(1, "dlopen: %s: %s\n", FOO, dlerror());
	}
	printf("loaded: %s\n", FOO);
	printf("looking up foo\n");
	foo = (foofunc)dlsym(handle, "foo");
	printf("found %p - calling it\n", foo);
	i = foo();
	printf("done.\n");
	exit(i);
}
