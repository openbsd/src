/*	$OpenBSD: difftime.c,v 1.11 2015/02/10 00:58:28 tedu Exp $ */
/* This file is placed in the public domain by Ted Unangst. */

#include "private.h"

double
difftime(time_t time1, time_t time0)
{
	return time1 - (double)time0;
}
