/*	$OpenBSD: posix_madvise.c,v 1.3 2014/07/10 13:42:53 guenther Exp $ */
/*
 * Ted Unangst wrote this file and placed it into the public domain.
 */
#include <sys/mman.h>
#include <errno.h>

int _thread_sys_madvise(void *addr, size_t len, int behav);

int
posix_madvise(void *addr, size_t len, int behav)
{
	return (_thread_sys_madvise(addr, len, behav) ? errno : 0);
}
