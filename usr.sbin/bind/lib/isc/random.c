/*
 * Copyright (C) 1999-2001  Internet Software Consortium.
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND INTERNET SOFTWARE CONSORTIUM
 * DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL
 * INTERNET SOFTWARE CONSORTIUM BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING
 * FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT,
 * NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/* $ISC: random.c,v 1.15 2001/01/09 21:56:22 bwelling Exp $ */

#include <config.h>

#include <stdlib.h>
#include <time.h>		/* Required for time(). */

#include <isc/mutex.h>
#include <isc/once.h>
#include <isc/random.h>
#include <isc/string.h>
#include <isc/util.h>

static isc_once_t once = ISC_ONCE_INIT;

static void
initialize_rand(void)
{
	srand(time(NULL));
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

	srand(seed);
}

void
isc_random_get(isc_uint32_t *val)
{
	REQUIRE(val != NULL);

	initialize();

	*val = rand();
}

isc_uint32_t
isc_random_jitter(isc_uint32_t max, isc_uint32_t jitter) {
	REQUIRE(jitter < max);
	if (jitter == 0)
		return (max);
	else
		return (max - rand() % jitter);
}
