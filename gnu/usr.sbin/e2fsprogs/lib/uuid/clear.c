/*
 * clear.c -- Clear a UUID
 * 
 * Copyright (C) 1996, 1997 Theodore Ts'o.
 *
 * %Begin-Header%
 * This file may be redistributed under the terms of the GNU Public
 * License.
 * %End-Header%
 */

#include "string.h"

#include "uuidP.h"

void uuid_clear(uuid_t uu)
{
	memset(uu, 0, 16);
}

