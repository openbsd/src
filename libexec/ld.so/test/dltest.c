/*	$OpenBSD: dltest.c,v 1.4 2001/09/25 21:51:16 drahn Exp $	*/

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <dlfcn.h>
#include <ctype.h>
#include <elf_abi.h>
#include <nlist.h>
#include "archdep.h"
#include "resolve.h"

typedef void (*func_t)(const char *);

#define DL_NOW 0
void
dltest(const char *s)
{
	printf("From dltest: ");
	for(;*s; s++) {
		putchar(toupper(*s));
	}
	putchar('\n');
}
struct elf_object *foo;

main(int argc, char **argv)
{
	void *handle;
	func_t fptr;
	char *libname = "libfoo.so";
	char **name = NULL;
	char *funcname = "foo";
	char *param = "Dynamic Loading Test";
	int ch;
	int mode;

	while((ch = getopt(argc, argv, "a:b:f:l:")) != EOF) {
		switch(ch) {
		case 'a':
			param = optarg;
			break;

		case 'b':
			switch(*optarg) {
			case 'l':
				mode = DL_LAZY;
				break;

			case 'n':
				mode = DL_NOW;
				break;
			}
			break;

		case 'l':
			libname = optarg;
			break;

		case 'f':
			funcname = optarg;
			break;
		}
	}

	printf("opening library %s for function %s\n", libname, funcname);
	handle = dlopen(libname, mode);
	if(handle == NULL) {
		fprintf(stderr, "%s: dlopen: '%s'\n", libname, dlerror());
		exit(1);
	}

	fptr = (func_t)dlsym(handle, funcname);
	if(fptr == NULL) {
		fprintf(stderr, "%s: dlsym: '%s'\n", funcname, dlerror());
		exit(1);
	}

	name = (char **)dlsym(handle, "libname");
	if(name == NULL) {
		fprintf(stderr, "libname: dlsym: '%s'\n", dlerror());
		exit(1);
	}

	printf("Call '%s' in '%s':\n", funcname, *name);

	(*fptr)(param);

#ifdef __mips__
	dlctl(handle, DL_DUMP_MAP, NULL);
#endif /* __mips__ */

	dlclose(handle);

	printf("After 'dlclose()'\n");
#ifdef __mips__
	dlctl(handle, DL_DUMP_MAP, NULL);
#endif /* __mips__ */

	return(0);
}


