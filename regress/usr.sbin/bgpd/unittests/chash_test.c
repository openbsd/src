/*	$OpenBSD: chash_test.c,v 1.2 2025/12/13 19:27:32 claudio Exp $ */

/*
 * Copyright (c) 2025 Claudio Jeker <claudio@openbsd.org>
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
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "chash.h"

struct peer {
	uint32_t	id;
};

/*
 * Use fmix32() the finalization mix of MurmurHash3 as a 32bit hash function.
 */
__unused static inline uint32_t
hash(uint32_t h)
{
	h ^= h >> 16;
	h *= 0x85ebca6b;
	h ^= h >> 13;
	h *= 0xc2b2ae35;
	h ^= h >> 16;
	return h;
}

__unused static uint64_t
hash64(uint64_t h)
{
	h ^= h >> 33;
	h *= 0xff51afd7ed558ccdULL;
	h ^= h >> 33;
	h *= 0xc4ceb9fe1a85ec53ULL;
	h ^= h >> 33;
	return h;
}



static inline uint64_t
peer_hash(const struct peer *p)
{
	return hash64(p->id);
}

static inline int
peer_cmp(const struct peer *l, const struct peer *r)
{
	return l->id == r->id;
}


CH_HEAD(test, peer);
CH_PROTOTYPE(test, peer, peer_hash);

int
main(int argc, char **argv)
{
	struct peer	peers[11000], *pp;
	struct ch_iter	iter;
	uint32_t	i, sum;
	struct test	head = CH_INITIALIZER(head);

	for (i = 0; i < 11000; i++) {
		peers[i].id = i;
	}

	for (i = 0; i < 11000; i++) {
		if (CH_INSERT(test, &head, &peers[i], NULL) != 1)
			err(1, "insert %d failed", i);
	}
	printf("all inserted\n");

	for (i = 0; i < 11000; i++) {
		if ((pp = CH_FIND(test, &head, &peers[i])) != &peers[i])
			err(1, "lookup %d failed %p != %p", i, pp, &peers[i]);
	}
	printf("all found\n");

	for (i = 0; i < 11000; i++) {
		struct peer p;
		p.id = arc4random_uniform(1000) + 11000;
		if (CH_FIND(test, &head, &p) != NULL)
			errx(1, "random lookup for %d succeded", p.id);
	}
	printf("all missed\n");

	sum = 0;
	CH_FOREACH(pp, test, &head, &iter) {
		sum += pp->id;
	}
	printf("all walked: sum %d\n", sum);
	if (sum != (11000 * 10999 / 2))
		errx(1, "incorrect sum of peer ids, not %d", 11000 * 10999 / 2);

	CH_DESTROY(test, &head);

	/* check that after destroy it is possible to reinsert an element */
	if (CH_INSERT(test, &head, &peers[0], NULL) != 1)
		err(1, "insert %d failed", i);

	return 0;
}

CH_GENERATE(test, peer, peer_cmp, peer_hash);
