#include <stdio.h>
#include <err.h>
#include <dlfcn.h>

int
main(void)
{
	void *handle = dlopen("libtest.so", DL_LAZY);
	void (*version)(void);

	if (handle == NULL)
		errx(1, "could not dynamically link libtest");
	version = dlsym(handle, "version");
	if (version == NULL)
		errx(2, "libtest did not define version()");
	version();
	dlclose(handle);
	return 0;
}
