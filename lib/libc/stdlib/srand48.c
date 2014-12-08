/*	$OpenBSD: srand48.c,v 1.4 2014/12/08 21:45:20 deraadt Exp $ */
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

#include "rand48.h"

extern unsigned short __rand48_seed[3];
extern unsigned short __rand48_mult[3];
extern unsigned short __rand48_add;

int     __rand48_deterministic;

void
srand48(long seed)
{
	srand48_deterministic(seed);
	__rand48_deterministic = 0;
}

void
srand48_deterministic(long seed)
{
	__rand48_deterministic = 1;
	__rand48_seed[0] = RAND48_SEED_0;
	__rand48_seed[1] = (unsigned short) seed;
	__rand48_seed[2] = (unsigned short) (seed >> 16);
	__rand48_mult[0] = RAND48_MULT_0;
	__rand48_mult[1] = RAND48_MULT_1;
	__rand48_mult[2] = RAND48_MULT_2;
	__rand48_add = RAND48_ADD;
}
