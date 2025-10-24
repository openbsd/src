/*	$OpenBSD: ffsll.c,v 1.1 2025/10/24 11:30:06 claudio Exp $	*/

/*
 * Public domain.
 * Written by Claudio Jeker.
 */

#include <strings.h>

/*
 * ffs -- find the first (least significant) bit set
 */
int
ffsll(long long mask)
{
	return (mask ? __builtin_ctzll(mask) + 1 : 0);
}
