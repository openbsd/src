/* $OpenBSD: toeplitz.c,v 1.1 2020/06/16 04:46:49 dlg Exp $ */

/*
 * Copyright (c) 2009 The DragonFly Project.  All rights reserved.
 *
 * This code is derived from software contributed to The DragonFly Project
 * by Sepherosa Ziehau <sepherosa@gmail.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of The DragonFly Project nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific, prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Copyright (c) 2019 David Gwynne <dlg@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <net/toeplitz.h>

/*
 * symmetric toeplitz
 */

static stoeplitz_key		stoeplitz_keyseed = STOEPLITZ_KEYSEED;
static struct stoeplitz_cache	stoeplitz_syskey_cache;
const struct stoeplitz_cache *const
				stoeplitz_cache = &stoeplitz_syskey_cache; 

void
stoeplitz_init(void)
{
	stoeplitz_cache_init(&stoeplitz_syskey_cache, stoeplitz_keyseed);
}

#define NBSK (NBBY * sizeof(stoeplitz_key))

void
stoeplitz_cache_init(struct stoeplitz_cache *scache, stoeplitz_key skey)
{
	uint32_t key[NBBY];
	unsigned int j, b, shift, val;

	bzero(key, sizeof(key));

	/*
	 * Calculate 32bit keys for one byte; one key for each bit.
	 */
	for (b = 0; b < NBBY; ++b) {
		for (j = 0; j < 32; ++j) {
			unsigned int bit;

			bit = b + j;

			shift = NBSK - (bit % NBSK) - 1;
			if (skey & (1 << shift))
				key[b] |= 1 << (31 - j);
		}
	}

	/*
	 * Cache the results of all possible bit combination of
	 * one byte.
	 */
	for (val = 0; val < 256; ++val) {
		uint32_t res = 0;

		for (b = 0; b < NBBY; ++b) {
			shift = NBBY - b - 1;
			if (val & (1 << shift))
				res ^= key[b];
		}
		scache->bytes[val] = res;
	}
}

uint16_t
stoeplitz_hash_ip4(const struct stoeplitz_cache *scache,
    in_addr_t faddr, in_addr_t laddr)
{
	uint16_t lo, hi;

	lo  = stoeplitz_cache_entry(scache, faddr >> 0);
	lo ^= stoeplitz_cache_entry(scache, faddr >> 16);
	lo ^= stoeplitz_cache_entry(scache, laddr >> 0);
	lo ^= stoeplitz_cache_entry(scache, laddr >> 16);

	hi  = stoeplitz_cache_entry(scache, faddr >> 8);
	hi ^= stoeplitz_cache_entry(scache, faddr >> 24);
	hi ^= stoeplitz_cache_entry(scache, laddr >> 8);
	hi ^= stoeplitz_cache_entry(scache, laddr >> 24);

	return (swap16(lo) ^ hi);
}

uint16_t
stoeplitz_hash_ip4port(const struct stoeplitz_cache *scache,
    in_addr_t faddr, in_addr_t laddr, in_port_t fport, in_port_t lport)
{
	uint16_t hi, lo;

	lo  = stoeplitz_cache_entry(scache, faddr >> 0);
	lo ^= stoeplitz_cache_entry(scache, faddr >> 16);
	lo ^= stoeplitz_cache_entry(scache, laddr >> 0);
	lo ^= stoeplitz_cache_entry(scache, laddr >> 16);
	lo ^= stoeplitz_cache_entry(scache, fport >> 0);
	lo ^= stoeplitz_cache_entry(scache, lport >> 0);

	hi  = stoeplitz_cache_entry(scache, faddr >> 8);
	hi ^= stoeplitz_cache_entry(scache, faddr >> 24);
	hi ^= stoeplitz_cache_entry(scache, laddr >> 8);
	hi ^= stoeplitz_cache_entry(scache, laddr >> 24);
	hi ^= stoeplitz_cache_entry(scache, fport >> 8);
	hi ^= stoeplitz_cache_entry(scache, lport >> 8);

	return (swap16(lo) ^ hi);
}

#ifdef INET6
uint16_t
stoeplitz_hash_ip6(const struct stoeplitz_cache *scache,
    const struct in6_addr *faddr6, const struct in6_addr *laddr6)
{
	uint16_t hi = 0, lo = 0;
	size_t i;

	for (i = 0; i < nitems(faddr6->s6_addr32); i++) {
		uint32_t faddr = faddr6->s6_addr32[i];
		uint32_t laddr = laddr6->s6_addr32[i];

		lo ^= stoeplitz_cache_entry(scache, faddr >> 0);
		lo ^= stoeplitz_cache_entry(scache, faddr >> 16);
		lo ^= stoeplitz_cache_entry(scache, laddr >> 0);
		lo ^= stoeplitz_cache_entry(scache, laddr >> 16);

		hi ^= stoeplitz_cache_entry(scache, faddr >> 8);
		hi ^= stoeplitz_cache_entry(scache, faddr >> 24);
		hi ^= stoeplitz_cache_entry(scache, laddr >> 8);
		hi ^= stoeplitz_cache_entry(scache, laddr >> 24);
	}

	return (swap16(lo) ^ hi);
}

uint16_t
stoeplitz_hash_ip6port(const struct stoeplitz_cache *scache,
    const struct in6_addr *faddr6, const struct in6_addr * laddr6,
    in_port_t fport, in_port_t lport)
{
	uint16_t hi = 0, lo = 0;
	size_t i;

	for (i = 0; i < nitems(faddr6->s6_addr32); i++) {
		uint32_t faddr = faddr6->s6_addr32[i];
		uint32_t laddr = laddr6->s6_addr32[i];

		lo ^= stoeplitz_cache_entry(scache, faddr >> 0);
		lo ^= stoeplitz_cache_entry(scache, faddr >> 16);
		lo ^= stoeplitz_cache_entry(scache, laddr >> 0);
		lo ^= stoeplitz_cache_entry(scache, laddr >> 16);

		hi ^= stoeplitz_cache_entry(scache, faddr >> 8);
		hi ^= stoeplitz_cache_entry(scache, faddr >> 24);
		hi ^= stoeplitz_cache_entry(scache, laddr >> 8);
		hi ^= stoeplitz_cache_entry(scache, laddr >> 24);
	}

	lo ^= stoeplitz_cache_entry(scache, fport >> 0);
	lo ^= stoeplitz_cache_entry(scache, lport >> 0);

	hi ^= stoeplitz_cache_entry(scache, fport >> 8);
	hi ^= stoeplitz_cache_entry(scache, lport >> 8);

	return (swap16(lo) ^ hi);
}
#endif /* INET6 */

void
stoeplitz_to_key(uint8_t *k, size_t klen)
{
	uint16_t skey = htons(stoeplitz_keyseed);
	size_t i;

	KASSERT((klen % 2) == 0);

	for (i = 0; i < klen; i += sizeof(skey)) {
		k[i + 0] = skey >> 8;
		k[i + 1] = skey;
	}
}
