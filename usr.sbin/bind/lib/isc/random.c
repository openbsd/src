/*
 * Copyright (C) 2004, 2005  Internet Systems Consortium, Inc. ("ISC")
 * Copyright (C) 1999-2003  Internet Software Consortium.
 * Copyright (C) 2008 Damien Miller
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND ISC DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS.  IN NO EVENT SHALL ISC BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE
 * OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: random.c,v 1.21.18.2 2005/04/29 00:16:48 marka Exp $ */

/*! \file */

#include <config.h>

#include <stdlib.h>
#include <time.h>		/* Required for time(). */
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <isc/mutex.h>
#include <isc/once.h>
#include <isc/random.h>
#include <isc/string.h>
#include <isc/util.h>

static isc_once_t once = ISC_ONCE_INIT;

static void
initialize_rand(void)
{
#ifndef HAVE_ARC4RANDOM
	unsigned int pid = getpid();
	
	/*
	 * The low bits of pid generally change faster.
	 * Xor them with the high bits of time which change slowly.
	 */
	pid = ((pid << 16) & 0xffff0000) | ((pid >> 16) & 0xffff);

	srand(time(NULL) ^ pid);
#endif
}

static void
initialize(void)
{
	RUNTIME_CHECK(isc_once_do(&once, initialize_rand) == ISC_R_SUCCESS);
}

void
isc_random_seed(isc_uint32_t seed)
{
	initialize();

#ifndef HAVE_ARC4RANDOM
	srand(seed);
#else
	arc4random_addrandom((u_char *) &seed, sizeof(isc_uint32_t));
#endif
}

void
isc_random_get(isc_uint32_t *val)
{
	REQUIRE(val != NULL);

	initialize();

#ifndef HAVE_ARC4RANDOM
	/*
	 * rand()'s lower bits are not random.
	 * rand()'s upper bit is zero.
	 */
	*val = ((rand() >> 4) & 0xffff) | ((rand() << 12) & 0xffff0000);
#else
	*val = arc4random();
#endif
}

isc_uint32_t
isc_random_uniform(isc_uint32_t upper_bound)
{
	isc_uint32_t r, min;

	/*
	 * Uniformity is achieved by generating new random numbers until
	 * the one returned is outside the range [0, 2**32 % upper_bound).
	 * This guarantees the selected random number will be inside
	 * [2**32 % upper_bound, 2**32) which maps back to [0, upper_bound)
	 * after reduction modulo upper_bound.
	 */

	if (upper_bound < 2)
		return 0;

#if (ULONG_MAX > 0xffffffffUL)
	min = 0x100000000UL % upper_bound;
#else
	/* Calculate (2**32 % upper_bound) avoiding 64-bit math */
	if (upper_bound > 0x80000000)
		min = 1 + ~upper_bound;		/* 2**32 - upper_bound */
	else {
		/* (2**32 - x) % x == 2**32 % x when x <= 2**31 */
		min = ((0xffffffff - upper_bound) + 1) % upper_bound;
	}
#endif

	/*
	 * This could theoretically loop forever doing this, but each retry
	 * has p > 0.5 (worst case, usually far better) of selecting a
	 * number inside the range we need, so it should rarely need to
	 * re-roll.
	 */
	for (;;) {
		isc_random_get(&r);
		if (r >= min)
			break;
	}

	return r % upper_bound;
}

isc_uint32_t
isc_random_jitter(isc_uint32_t max, isc_uint32_t jitter) {
	REQUIRE(jitter < max);
	if (jitter == 0)
		return (max);
	else
		return max - isc_random_uniform(jitter);
}

