/*	$OpenBSD: difftime.c,v 1.10 2015/02/09 08:36:53 tedu Exp $ */
/* This file is placed in the public domain by Ted Unangst. */

#include "private.h"

double
difftime(time_t time1, time_t time0)
{
	return time1 - time0;
}
