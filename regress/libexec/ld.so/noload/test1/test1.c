
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LIBANAME
#error "LIBANAME undefined"
#endif

#ifndef LIBBNAME
#error "LIBBNAME undefined"
#endif

int
main(int argc, char *argv[])
{
	void *handle;

	printf("opening\n");
	if ((handle = dlopen(LIBANAME, RTLD_NOW|RTLD_NOLOAD)))
		printf("%s found\n", LIBANAME);
	else if ((handle = dlopen(LIBBNAME, RTLD_NOW|RTLD_NOLOAD)))
		printf("%s found\n", LIBBNAME);

	return 0;
}
