/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: main.c,v 1.1 2005/09/17 02:58:54 drahn Exp $
 */
#include <stdio.h>
#include <dlfcn.h>

int
main()
{
	void *handle;
	handle = dlopen("libac.so.0.0", RTLD_LAZY);
	printf("handle %p\n", handle);
	return 0;
}
