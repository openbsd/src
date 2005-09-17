/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: prog1.C,v 1.2 2005/09/17 01:55:23 drahn Exp $
 */
#include <iostream>
#include <dlfcn.h>
#include <string.h>
typedef void (v_func)(void);
int a;
int
main()
{
	void *handle1;
	void *handle2;
	char **libname;
	v_func *func;

	std::cout << "main\n";
	handle1 = dlopen("libaa.so.0.0", DL_LAZY);
	if (handle1 == NULL) {
		std::cout << "handle1 open libaa failed\n";
		return (1);
	}
	handle2 = dlopen("libab.so.0.0", DL_LAZY);
	if (handle2 == NULL) {
		std::cout << "handle2 open libab failed\n";
		return (1);
	}

	libname = (char **)dlsym(handle1, "libname");
	if (strcmp(*libname, "libaa") != 0) {
		std::cout << "handle1 is " << *libname << "\n";
		return (1);
	}

	libname = (char **)dlsym(handle2, "libname");
	if (strcmp(*libname, "libab") != 0) {
		std::cout << "handle2 is " << *libname << "\n";
		return (1);
	}

	func = (v_func*)dlsym(handle1, "lib_entry");
	(*func)();

	func = (v_func*)dlsym(handle2, "lib_entry");
	(*func)();

	dlclose(handle1);
	dlclose(handle2);

	return 0;
}
