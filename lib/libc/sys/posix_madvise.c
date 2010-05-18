/*	$OpenBSD: posix_madvise.c,v 1.1 2010/05/18 22:24:55 tedu Exp $ */
/*
 * Ted Unangst wrote this file and placed it into the public domain.
 */
#include <sys/mman.h>

int
posix_madvise(void *addr, size_t len, int behav)
{
	return (_thread_sys_madvise(addr, len, behav));
}
