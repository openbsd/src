/*	$OpenBSD: rfmem.c,v 1.2 2002/02/21 23:29:05 deraadt Exp $	*/
/*
 * Written by Artur Grabowski <art@openbsd.org>, 2002 Public Domain.
 */
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/mman.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>

#define MAGIC "inherited"

int
main()
{
	void *map;
	int page_size;
	int status;

	page_size = getpagesize();

	if ((map = mmap(NULL, page_size, PROT_READ|PROT_WRITE, MAP_ANON,
	    -1, 0)) == MAP_FAILED)
		err(1, "mmap");

	memset(map, 0, sizeof(MAGIC));

	switch(rfork(RFFDG|RFPROC|RFMEM)) {
	case -1:
		err(1, "fork");
	case 0:
		memcpy(map, MAGIC, sizeof(MAGIC));
		_exit(0);
	}

	if (wait(&status) < 0)
		err(1, "wait");

	if (!WIFEXITED(status))
		err(1, "child error");

	if (memcmp(map, MAGIC, sizeof(MAGIC)) != 0)
		return 1;

	return WEXITSTATUS(status) != 0;
}
