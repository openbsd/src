/*	$OpenBSD: rfmem-stack.c,v 1.5 2003/08/02 01:24:37 david Exp $	*/
/*
 * Written by Artur Grabowski <art@openbsd.org>, 2002 Public Domain.
 */
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#define MAGIC "inherited"

int
main(int argc, char *argv[])
{
	char *map, *map2;
	int status;

	map = alloca(sizeof(MAGIC));
	memset(map, 0, sizeof(MAGIC));

	map2 = alloca(sizeof(MAGIC));
	memset(map2, 0, sizeof(MAGIC));

	switch(rfork(RFFDG|RFPROC|RFMEM)) {
	case -1:
		err(1, "fork");
	case 0:
		memcpy(map, MAGIC, sizeof(MAGIC));
		sleep(1);
		if (memcmp(map2, MAGIC, sizeof(MAGIC)) == 0) {
			write(2, "child stack polluted\n", 21);
			_exit(1);
		}
		_exit(0);
	}

	memcpy(map2, MAGIC, sizeof(MAGIC));

	if (wait(&status) < 0)
		err(1, "wait");

	if (!WIFEXITED(status))
		err(1, "child error");

	if (memcmp(map, MAGIC, sizeof(MAGIC)) == 0)
		return 1;

	return WEXITSTATUS(status) != 0;
}
