/*	$OpenBSD: mquery.c,v 1.1 2003/04/14 04:53:50 art Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> Public Domain
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>

/*
 * This function provides 64-bit offset padding.
 */
int
mquery(int flags, void **addr, size_t size, int fd, off_t off)
{
	return(__syscall((quad_t)SYS_mquery, flags, addr, size, fd, off));
}
