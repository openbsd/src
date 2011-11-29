/*
 * Public Domain 2011 Kurt Miller
 *
 * $OpenBSD: prog2.C,v 1.1 2011/11/29 04:36:15 kurt Exp $
 */

#include <iostream>
#include <dlfcn.h>
#include <string.h>

int
main()
{
	void *handle1;

	handle1 = dlopen("libaa.so", DL_LAZY);
	if (handle1 == NULL) {
		std::cout << "handle1 open libaa failed\n";
		return (1);
	}
	dlclose(handle1);

	return 0;
}
