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
static char rcsid[] = "$OpenBSD: jrand48.c,v 1.2 1996/08/19 08:33:33 tholo Exp $";
#endif /* LIBC_SCCS and not lint */

#include "rand48.h"

long
jrand48(unsigned short xseed[3])
{
	__dorand48(xseed);
	return ((long) xseed[2] << 16) + (long) xseed[1];
}
