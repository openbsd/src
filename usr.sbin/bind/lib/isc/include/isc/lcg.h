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

/* $OpenBSD: lcg.h,v 1.2 2004/09/28 17:14:07 jakob Exp $ */

/*
 * Theo de Raadt <deraadt@openbsd.org> came up with the idea of using
 * such a mathematical system to generate more random (yet non-repeating)
 * ids to solve the resolver/named problem.  But Niels designed the
 * actual system based on the constraints.
 */

/*
 * seed = random 15bit
 * n = prime, g0 = generator to n,
 * j = random so that gcd(j,n-1) == 1
 * g = g0^j mod n will be a generator again.
 *
 * X[0] = random seed.
 * X[n] = a*X[n-1]+b mod m is a Linear Congruential Generator
 * with a = 7^(even random) mod m, 
 *      b = random with gcd(b,m) == 1
 *      m = 31104 and a maximal period of m-1.
 *
 * The transaction id is determined by:
 * id[n] = seed xor (g^X[n] mod n)
 *
 * Effectivly the id is restricted to the lower 15 bits, thus
 * yielding two different cycles by toggling the msb on and off.
 * This avoids reuse issues caused by reseeding.
 *
 * The 16 bit space is very small and brute force attempts are
 * entirly feasible, we skip a random number of transaction ids
 * so that an attacker will not get sequential ids.
 */


#ifndef ISC_LCG_H
#define ISC_LCG_H 1

#include <isc/lang.h>
#include <isc/types.h>

typedef struct isc_lcg isc_lcg_t;

struct isc_lcg {
	isc_uint16_t ru_x;
	isc_uint16_t ru_seed, ru_seed2;
	isc_uint16_t ru_a, ru_b;
	isc_uint16_t ru_g;
	isc_uint16_t ru_counter;
	isc_uint16_t ru_msb;
	isc_uint32_t ru_reseed;
	isc_uint32_t random;
};

ISC_LANG_BEGINDECLS

void
isc_lcg_init(isc_lcg_t *lcg);
/*
 * Initialize a Linear Congruential Generator
 *
 * Requires:
 *
 *	lcg != NULL
 */

isc_uint16_t
isc_lcg_generate16(isc_lcg_t *lcg);
/*
 * Get a random number from a Linear Congruential Generator
 *
 * Requires:
 *
 *	lcg be valid.
 *
 *	data != NULL.
 */

ISC_LANG_ENDDECLS

#endif /* ISC_LCG_H */
