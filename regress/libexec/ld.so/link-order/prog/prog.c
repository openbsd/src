#include <sys/param.h>
#include <err.h>
#include <dlfcn.h>

#ifdef __ELF__
#define C_LABEL(x) x
#else
#define C_LABEL(x) "_" ## x
#endif

int
main()
{
	void *handle = dlopen("libtest.so", DL_LAZY);
	void (*version)(void);

	if (handle == NULL)
		errx(1, "could not dynamically link libtest");
	version = dlsym(handle, C_LABEL("version"));
	if (version == NULL)
		errx(2, "libtest did not define version()");
	version();
	dlclose(handle);
	return 0;
}
