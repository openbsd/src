/*	$OpenBSD: dtors.c,v 1.2 2002/01/31 16:47:02 art Exp $	*/
/*
 * Written by Artur Grabowski <art@openbsd.org> Public Domain.
 */
#include <sys/types.h>
#include <sys/mman.h>
#include <sys/wait.h>

#include <stdio.h>
#include <string.h>
#include <err.h>

void zap(void) __attribute__((destructor));

void *destarea;

#define MAGIC "destructed"

void
zap(void)
{
	memcpy(destarea, MAGIC, sizeof(MAGIC));
}

/*
 * XXX - horrible abuse of exit(), minherit and fork().
 */
int
main()
{
	int status;

	destarea = mmap(NULL, getpagesize(), PROT_READ|PROT_WRITE, MAP_ANON,
	    -1, 0);
	if (destarea == MAP_FAILED)
		err(1, "mmap");

	if (minherit(destarea, getpagesize(), MAP_INHERIT_SHARE) != 0)
		err(1, "minherit");

	memset(destarea, 0, sizeof(MAGIC));

	switch(fork()) {
	case -1:
		err(1, "fork");
	case 0:
		/*
		 * Yes, it's exit(), not _exit(). We _want_ to run the
		 * destructors in the child.
		 */
		exit(0);
	}

	if (wait(&status) < 0)
		err(1, "wait");		/* XXX uses exit() */

	if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
		err(1, "child error");	/* XXX uses exit() */

	_exit(memcmp(destarea, MAGIC, sizeof(MAGIC)) != 0);
}	
