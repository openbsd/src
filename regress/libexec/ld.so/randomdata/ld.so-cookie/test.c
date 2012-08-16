#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>

int
main()
{
	void *dso;
	long *guardptr;
	long guard;

	dso = dlopen("/usr/libexec/ld.so", RTLD_LOCAL|RTLD_LAZY);
	assert(dso != NULL);
	guardptr = dlsym(dso, "__guard");
	assert(guardptr != NULL);

	guard = *guardptr;
	assert(guard != 0);

	return (0);
}
