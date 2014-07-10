/*	$OpenBSD: posix_madvise.c,v 1.2 2014/07/10 12:46:28 tedu Exp $ */
/*
 * Ted Unangst wrote this file and placed it into the public domain.
 */
#include <sys/mman.h>

int _thread_sys_madvise(void *addr, size_t len, int behav);

int
posix_madvise(void *addr, size_t len, int behav)
{
	return (_thread_sys_madvise(addr, len, behav));
}
