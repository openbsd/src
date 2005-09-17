/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: main.c,v 1.1 2005/09/17 02:58:55 drahn Exp $
 */
#include <stdio.h>
#include <dlfcn.h>

int
main()
{
	int ret = 0;
	void *handle;

	handle = dlopen("libac.so.0.0", RTLD_LAZY);
	if (handle != NULL) {
		printf("found libaa, dependancy of libac, not expected\n");
		ret = 1;
	}

	return ret;
}
