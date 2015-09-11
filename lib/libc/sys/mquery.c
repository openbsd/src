/*	$OpenBSD: mquery.c,v 1.8 2015/09/11 13:26:20 guenther Exp $	*/
/*
 *	Written by Artur Grabowski <art@openbsd.org> Public Domain
 */

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/syscall.h>

void	*__syscall(quad_t, ...);
PROTO_NORMAL(__syscall);

DEF_SYS(mquery);

/*
 * This function provides 64-bit offset padding.
 */
void *
mquery(void *addr, size_t len, int prot, int flags, int fd, off_t offset)
{
	return (__syscall(SYS_mquery, addr, len, prot, flags, fd, 0, offset));
}
DEF_WEAK(mquery);
