/*	$OpenBSD: exec_self.c,v 1.1 2002/08/17 22:58:59 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> 2002 Public Domain.
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>
#include <sys/types.h>
#include <sys/mman.h>

struct {
	const char pad1[256*1024];	/* avoid read-ahead. */
	const char string[256*1024];	/* at least one page */
	const char pad2[256*1024];	/* avoid read-behind. */
} const blob = {
	"padding1",
	"the_test",
	"padding2"
};

int
main(int argc, char **argv)
{
	int pgsz = getpagesize();
	vaddr_t va, off;

	if (argc > 1) {
		return (0);
	}
	va = (vaddr_t)&blob;
	off = va & (pgsz - 1);

	/* Make sure that nothing in the "blob" is cached. */
	if (madvise((void *)(va - off), sizeof(blob) + (off > 0 ? pgsz : 0),
	    MADV_FREE))
		err(1, "madvise");

	if (execl(argv[0], argv[0], &blob.string, NULL))
		err(1, "execl");

	/* NOTREACHED */
	return (1);
}
