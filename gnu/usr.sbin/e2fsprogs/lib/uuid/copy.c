/*
 * copy.c --- copy UUIDs
 * 
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "uuidP.h"

void uuid_copy(uuid_t uu1, uuid_t uu2)
{
	unsigned char 	*cp1, *cp2;
	int		i;

	for (i=0, cp1 = uu1, cp2 = uu2; i < 16; i++)
		*cp1++ = *cp2++;
}
