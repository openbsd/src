/*
 * Public Domain 2003 Dale Rahn
 *
 * $OpenBSD: prog1.C,v 1.3 2005/09/18 19:58:50 drahn Exp $
 */
#include <iostream>
#include <dlfcn.h>
#include <string.h>
typedef char * (cp_func)(void);
int a;
int
main()
{
	void *handle1;
	void *handle2;
	char **libname;
	char *str;
	cp_func *func;
	int ret = 0;

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

	func = (cp_func*)dlsym(handle1, "lib_entry");
	str = (*func)();
	if (strcmp(str, "libaa:aa") != 0) {
		printf("func should have returned libaa:aa returned %s\n", str);
		ret = 1;
	}

	func = (cp_func*)dlsym(handle2, "lib_entry");
	str = (*func)();
	if (strcmp(str, "libab:ab") != 0) {
		printf("func should have returned libab:ab returned %s\n", str);
		ret = 1;
	}

	dlclose(handle1);
	dlclose(handle2);

	return ret;
}
