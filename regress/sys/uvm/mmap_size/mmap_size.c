/*	$OpenBSD: mmap_size.c,v 1.1 2005/01/06 10:14:20 otto Exp $	*/

/*
 * Public domain. 2005, Otto Moerbeek <otto@drijf.net>
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>

void
f(size_t sz)
{
	char *p;
	p = mmap(NULL, sz, PROT_READ|PROT_WRITE,
                    MAP_ANON|MAP_PRIVATE, -1, (off_t)0);	

	if (p == MAP_FAILED)
		return;

	if (sz > 0) {
		p[0] = 0;
		p[sz / 2] = 0;
		p[sz - 1] = 0;
	}
	munmap(p, sz);
}

int
main()
{
	size_t i;

	for (i = 0; i < 0x2000; i += 0x100) {
		f(i);
		f(-i);
		f(SIZE_T_MAX/2 - 0x1000);
	}	
	return (0);
}

	
