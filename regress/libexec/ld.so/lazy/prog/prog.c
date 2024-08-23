/*	$OpenBSD: prog.c,v 1.4 2024/08/23 12:56:26 anton Exp $ */
/* Public Domain, 2008, Matthieu Herrb */

#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <err.h>

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
		errx(1, "dlopen: %s: %s", FOO, dlerror());
	}
	printf("loaded: %s\n", FOO);
	printf("looking up foo\n");
	foo = (foofunc)dlsym(handle, "foo");
	printf("found %p - calling it\n", foo);
	i = foo();
	printf("done.\n");
	exit(i);
}
