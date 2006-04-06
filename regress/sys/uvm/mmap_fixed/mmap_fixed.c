/*	$OpenBSD: mmap_fixed.c,v 1.1.1.1 2006/04/06 20:39:03 kurt Exp $	*/

/*
 * Public domain. 2006, Kurt Miller <kurt@intricatesoftware.com>
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <err.h>

#define MEM_SIZE        1024*1024

/*
 * Repetitively call mmap with MMAP_FIXED on the same region of memory
 * to ensure process datasize is properly calculated.
 */

int
main(void)
{
	void *mem_area;
	int i;

	mem_area = mmap(0, MEM_SIZE, PROT_NONE, MAP_ANON, -1, 0);

	for (i = 0; i < 20000; i++) {
		if (mmap(mem_area, MEM_SIZE, PROT_READ|PROT_WRITE|PROT_EXEC,
		    MAP_ANON|MAP_FIXED, -1, 0) == MAP_FAILED)
			err(1, NULL);
	}

	return (0);
}
