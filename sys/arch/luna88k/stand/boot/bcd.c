/* $OpenBSD: bcd.c,v 1.1 2013/10/28 22:13:12 miod Exp $ */
/* $NetBSD: bcd.c,v 1.1 2006/03/11 15:40:07 kleink Exp $ */

/*
 * Convert a single byte between (unsigned) packed bcd and binary.
 * Public domain.
 */

#include <lib/libkern/libkern.h>

unsigned int
bcdtobin(unsigned int bcd)
{

        return (((bcd >> 4) & 0x0f) * 10 + (bcd & 0x0f));
}

unsigned int
bintobcd(unsigned int bin)
{

	return ((((bin / 10) << 4) & 0xf0) | (bin % 10));
}
