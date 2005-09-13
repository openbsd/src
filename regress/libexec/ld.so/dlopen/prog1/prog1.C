/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: prog1.C,v 1.1.1.1 2005/09/13 20:51:39 drahn Exp $
 */
#include <iostream>
#include <dlfcn.h>
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
		std::cout << "handle1 failed\n";
	}
	handle2 = dlopen("libab.so.0.0", DL_LAZY);
	if (handle2 == NULL) {
		std::cout << "handle1 failed\n";
	}
	std::cout << "loaded \n";
	libname = (char **)dlsym(handle1, "libname");
	std::cout << "handle1 is " << *libname << "\n";
	libname = (char **)dlsym(handle2, "libname");
	std::cout << "handle2 is " << *libname << "\n";
	func = (v_func*)dlsym(handle1, "lib_entry");
	(*func)();
	func = (v_func*)dlsym(handle2, "lib_entry");
	(*func)();

	std::cout << "closing \n";
	dlclose(handle1);
	dlclose(handle2);
	std::cout << "all done \n";

	return 0;
}
