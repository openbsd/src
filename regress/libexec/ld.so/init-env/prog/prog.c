/*
 * Public Domain 2016 Philip Guenther <guenther@openbsd.org>
 *
 * $OpenBSD: prog.c,v 1.1 2016/03/20 05:13:22 guenther Exp $
 */

#include <stdio.h>
#include <stdlib.h>

extern char **environ;

int
main(int argc, char **argv, char **env)
{
	int ret = 0;

	if (env == environ)
		printf("OK: main's 3rd arg == environ\n");
	else {
		ret = 1;
		printf("FAILED: main's 3rd arg isn't environ\n");
	}
	if (getenv("INIT_ENV_REGRESS_TEST") != NULL)
		printf("OK: env var set by .so init function set\n");
	else {
		ret = 1;
		printf("FAILED: env var set by .so init function not set\n");
	}

	return ret;
}
