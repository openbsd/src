/*	$OpenBSD: difftime.c,v 1.12 2015/02/10 01:24:28 tedu Exp $ */
/* This file is placed in the public domain by Matthew Dempsky. */

#include "private.h"

#define HI(t) ((double)(t & 0xffffffff00000000LL))
#define LO(t) ((double)(t & 0x00000000ffffffffLL))

double
difftime(time_t t1, time_t t0)
{
	return (HI(t1) - HI(t0)) + (LO(t1) - LO(t0));
}
