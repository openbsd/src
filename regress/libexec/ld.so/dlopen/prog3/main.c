/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: main.c,v 1.2 2005/09/18 19:58:50 drahn Exp $
 */
#include <stdio.h>
#include <dlfcn.h>

int
main()
{
	void *handle;
	int ret = 0;

	handle = dlopen("libac.so.0.0", RTLD_LAZY);
	if (handle == NULL) {
		printf("failed to open libac and it's dependancies\n");
		ret = 1;
	}
	return ret;
}
