
#include <err.h>
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef LIBNAME
#error "LIBNAME undefined"
#endif

#ifndef SYMBOL
#define SYMBOL "function"
#endif

int
checksym(const char *name)
{
	void *sym = dlsym(RTLD_DEFAULT, name);
	
	if (sym != NULL) {
		printf("symbol present: %s\n", name);
		return 1;
	} else {
		printf("symbol absent: %s\n", name);
		return 0;
	}
}

int
main(int argc, char *argv[])
{
	void *h1;
	void *h2;

	/* symbol should not be here at startup */
	if (checksym(SYMBOL) == 1)
		errx(1, "symbol found: %s", SYMBOL);
	
	printf("opening\n");
	if ((h1 = dlopen(LIBNAME, RTLD_GLOBAL)) == NULL)
		errx(1, "dlopen: h1: %s: %s", LIBNAME, dlerror());
	if ((h2 = dlopen(LIBNAME, RTLD_GLOBAL|RTLD_NODELETE)) == NULL)
		errx(1, "dlopen: h2: %s: %s", LIBNAME, dlerror());

	/* symbol should be here after loading */
	if (checksym(SYMBOL) == 0)
		errx(1, "symbol not found: %s", SYMBOL);

	printf("closing\n");
	if (dlclose(h2) != 0)
		errx(1, "dlclose: h2: %s", dlerror());
	if (dlclose(h1) != 0)
		errx(1, "dlclose: h1: %s", dlerror());

	/* symbol should be still here, as one dlopen(3) had RTLD_NODELETE */
	if (checksym(SYMBOL) == 0)
		errx(1, "symbol not found: %s", SYMBOL);

	return 0;
}
