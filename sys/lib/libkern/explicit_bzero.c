/*	$OpenBSD: explicit_bzero.c,v 1.2 2014/06/10 04:16:57 deraadt Exp $ */
/*
 * Public domain.
 * Written by Ted Unangst
 */

#include <lib/libkern/libkern.h>

/*
 * explicit_bzero - don't let the compiler optimize away bzero
 */
void
explicit_bzero(void *p, size_t n)
{
	bzero(p, n);
}
