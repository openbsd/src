/*	$OpenBSD: malloc_errno.c,v 1.1 2003/07/15 10:06:31 otto Exp $	*/
/*
 * Public domain.  2003, Otto Moerbeek
 */
#include <err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

static void
testerrno(size_t sz)
{
	void *p;

	errno = -1;
	p = malloc(sz);

	if (p == NULL && errno != ENOMEM)
		errx(1, "fail: %lx %p %d\n", (unsigned long)sz, p, errno);

	/* if alloc succeeded, test if errno did not change */
	if (p != NULL && errno != -1) 
		errx(1, "fail: %lx %p %d\n", (unsigned long)sz, p, errno);

	free(p);
}

/*
 * Provide some (silly) arguments to malloc(), and check if ERRNO is set
 * correctly.
 */
int
main()
{
	size_t i;

	testerrno(1);
	testerrno(100000);
	testerrno(-1);
	testerrno(-1000);
	testerrno(-10000);
	for (i = 0; i < 0x10; i++)
		testerrno(i * 0x10000000);
	return 0;
}
