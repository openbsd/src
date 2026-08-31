#include <sys/types.h>
#include <sys/auxv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>
#include <stdio.h>
#include <err.h>

int
main(int argc, char *argv[])
{
	char path[PATH_MAX];

	if (argc < 1)
		errx(1, "usage: getexecpath expected-path");

	if (getexecpath(path, sizeof path) == -1)
		err(1, "getexepath: on path");
	if (strcmp(path, argv[1]) != 0)
		exit(1);
	printf("getexecpath(3) is working\n");
	exit(0);
}
