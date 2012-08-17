#include <assert.h>
#include <dlfcn.h>
#include <stddef.h>

int
main()
{
	void *dso;
	long *guardptr;
	long guard;
	extern long __guard[];

	dso = dlopen("ld.so", RTLD_LOCAL|RTLD_LAZY);
	assert(dso != NULL);
	guardptr = dlsym(dso, "__guard");
	assert(guardptr != NULL);
	assert(guardptr != &__guard[0]);

	guard = *guardptr;
	assert(guard != 0);

	return (0);
}
