/*
 * Copyright (c) 1993 Martin Birgmeier
 * All rights reserved.
 *
 * You may redistribute unmodified or modified versions of this source
 * code provided that the above copyright notice and this and the
 * following conditions are retained.
 *
 * This software is provided ``as is'', and comes with no warranties
 * of any kind. I shall in no event be liable for anything that happens
 * to anyone/anything when using this software.
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char rcsid[] = "$OpenBSD: lrand48.c,v 1.2 1996/08/19 08:33:36 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include "rand48.h"

extern unsigned short __rand48_seed[3];

long
lrand48(void)
{
	__dorand48(__rand48_seed);
	return ((long) __rand48_seed[2] << 15) + ((long) __rand48_seed[1] >> 1);
}
