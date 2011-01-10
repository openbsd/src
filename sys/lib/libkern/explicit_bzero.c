/*	$OpenBSD: explicit_bzero.c,v 1.1 2011/01/10 23:23:56 tedu Exp $ */
/*
 * Public domain.
 * Written by Ted Unangst
 */

#if !defined(_KERNEL) && !defined(_STANDALONE)
#include <string.h>
#else
#include <lib/libkern/libkern.h>
#endif

/*
 * explicit_bzero - don't let the compiler optimize away bzero
 */
void
explicit_bzero(void *p, size_t n)
{
	bzero(p, n);
}
