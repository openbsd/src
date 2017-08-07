/*	$OpenBSD: foo.c,v 1.3 2017/08/07 16:33:52 bluhm Exp $ */
/* Public domain. 2008, Matthieu Herrb */

#include <dlfcn.h>
#include <stdio.h>
#include <err.h>

static void *h = NULL;

extern int bar(void);

void
foo_init(void)
{
	printf("loading %s\n", BAR);
	h = dlopen(BAR, RTLD_LAZY|RTLD_GLOBAL);
	if (h == NULL)
		errx(1, "dlopen %s: %s\n", BAR, dlerror());
	printf("loaded: %s\n", BAR);
}

int
foo(void)
{
	if (h == NULL)
		foo_init();
	
	return bar();
}
