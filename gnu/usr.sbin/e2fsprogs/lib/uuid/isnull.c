/*
 * isnull.c --- Check whether or not the UUID is null
 * 
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "uuidP.h"

/* Returns 1 if the uuid is the NULL uuid */
int uuid_is_null(uuid_t uu)
{
	unsigned char 	*cp;
	int		i;

	for (i=0, cp = uu; i < 16; i++)
		if (*cp++)
			return 0;
	return 1;
}

