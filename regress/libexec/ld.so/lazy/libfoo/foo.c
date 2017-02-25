/*	$OpenBSD: foo.c,v 1.2 2017/02/25 07:28:32 jsg Exp $ */
/* Public domain. 2008, Matthieu Herrb */

#include <dlfcn.h>
#include <stdio.h>
#include <err.h>

static void *h = NULL;

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
