/*	$OpenBSD: clzdi2.c,v 1.1 2026/06/23 21:28:10 kirill Exp $	*/

/*
 * Public domain.
 */

#include <lib/libkern/libkern.h>

int
__clzdi2(long mask)
{
	if (mask == 0)
		return 0;
	return (sizeof(long) * 8 - flsl(mask));
}
