/*
 * Portions Copyright (C) 2002  Internet Software Consortium.
 * Portions Copyright (C) 1997  Niels Provos.
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

/* $OpenBSD: lcg.c,v 1.1 2003/01/20 21:24:41 jakob Exp $ */

#include <config.h>

#include <stdlib.h>

#include <isc/lcg.h>
#include <isc/random.h>
#include <isc/time.h>
#include <isc/util.h>

#define VALID_LCG(x)	(x != NULL)

#define RU_OUT  180             /* Time after wich will be reseeded */
#define RU_MAX	30000		/* Uniq cycle, avoid blackjack prediction */
#define RU_GEN	2		/* Starting generator */
#define RU_N	32749		/* RU_N-1 = 2*2*3*2729 */
#define RU_AGEN	7               /* determine ru_a as RU_AGEN^(2*rand) */
#define RU_M	31104           /* RU_M = 2^7*3^5 - don't change */

#define PFAC_N 3
static const isc_uint16_t pfacts[PFAC_N] = {
	2, 
	3,
	2729
};

/*
 * Do a fast modular exponation, returned value will be in the range
 * of 0 - (mod-1)
 */
static isc_uint16_t
pmod(isc_uint16_t gen, isc_uint16_t exp, isc_uint16_t mod)
{
	isc_uint16_t s, t, u;

	s = 1;
	t = gen;
	u = exp;

	while (u) {
		if (u & 1)
			s = (s*t) % mod;
		u >>= 1;
		t = (t*t) % mod;
	}
	return (s);
}

/* 
 * Initializes the seed and chooses a suitable generator. Also toggles 
 * the msb flag. The msb flag is used to generate two distinct
 * cycles of random numbers and thus avoiding reuse of ids.
 *
 * This function is called from isc_lcg_generate() when needed, an
 * application does not have to worry about it.
 */
static void 
reseed(isc_lcg_t *lcg)
{
	isc_time_t isctime;
	isc_boolean_t noprime = ISC_TRUE;
	isc_uint16_t j, i;

	isc_random_get(&lcg->random);
	lcg->ru_x = (lcg->random & 0xFFFF) % RU_M;

	/* 15 bits of random seed */
	lcg->ru_seed = (lcg->random >> 16) & 0x7FFF;
	isc_random_get(&lcg->random);
	lcg->ru_seed2 = lcg->random & 0x7FFF;

	isc_random_get(&lcg->random);

	/* Determine the LCG we use */
	lcg->ru_b = (lcg->random & 0xfffe) | 1;
	lcg->ru_a = pmod(RU_AGEN, (lcg->random >> 16) & 0xfffe, RU_M);
	while (lcg->ru_b % 3 == 0)
		lcg->ru_b += 2;
	
	isc_random_get(&lcg->random);
	j = lcg->random % RU_N;
	lcg->random = lcg->random >> 16;

	/* 
	 * Do a fast gcd(j,RU_N-1), so we can find a j with
	 * gcd(j, RU_N-1) == 1, giving a new generator for
	 * RU_GEN^j mod RU_N
	 */
	while (noprime == ISC_TRUE) {
		for (i=0; i<PFAC_N; i++)
			if (j % pfacts[i] == 0)
				break;

		if (i >= PFAC_N)
			noprime = ISC_FALSE;
		else 
			j = (j+1) % RU_N;
	}

	lcg->ru_g = pmod(RU_GEN, j, RU_N);
	lcg->ru_counter = 0;

	isc_time_now(&isctime);
	lcg->ru_reseed = isc_time_seconds(&isctime) + RU_OUT;
	lcg->ru_msb = lcg->ru_msb == 0x8000 ? 0 : 0x8000; 
}

void
isc_lcg_init(isc_lcg_t *lcg)
{
	REQUIRE(VALID_LCG(lcg));

	lcg->ru_x = 0;
	lcg->ru_seed = 0;
	lcg->ru_seed2 = 0;
	lcg->ru_a = 0;
	lcg->ru_b = 0;
	lcg->ru_g = 0;
	lcg->ru_counter = 0;
	lcg->ru_msb = 0;
	lcg->ru_reseed = 0;
	lcg->random = 0;
}

isc_uint16_t
isc_lcg_generate16(isc_lcg_t *lcg)
{
	isc_time_t isctime;
        int i, n;

	REQUIRE(VALID_LCG(lcg));

	isc_time_now(&isctime);
	if (lcg->ru_counter >= RU_MAX ||
	    isc_time_seconds(&isctime) > lcg->ru_reseed)
		reseed(lcg);

	if (! lcg->random)
		isc_random_get(&lcg->random);

	/* Skip a random number of ids */
	n = lcg->random & 0x7; lcg->random = lcg->random >> 3;
	if (lcg->ru_counter + n >= RU_MAX)
                reseed(lcg);

	for (i=0; i<=n; i++)
	        /* Linear Congruential Generator */
	        lcg->ru_x = (lcg->ru_a*lcg->ru_x + lcg->ru_b) % RU_M;

	lcg->ru_counter += i;

	return (lcg->ru_seed ^
	    pmod(lcg->ru_g, lcg->ru_seed2 ^ lcg->ru_x, RU_N)) | lcg->ru_msb;
}
