/* $Id: cache.c,v 1.3 2008/12/07 02:56:06 canacar Exp $ */
/*
 * Copyright (c) 2001, 2007 Can Erkin Acar <canacar@openbsd.org>
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>

#include <net/if.h>
#include <netinet/in.h>

#include <netinet/tcp_fsm.h>
#ifdef TEST_COMPAT
#include "pfvarmux.h"
#else
#include <net/pfvar.h>
#endif
#include <arpa/inet.h>

#include <stdio.h>
#include <stdlib.h>

#include <assert.h>

#include "cache.h"

/* prototypes */
void update_state(struct sc_ent *, struct pfsync_state *, double);
struct sc_ent *cache_state(struct pfsync_state *);
static __inline int sc_cmp(struct sc_ent *s1, struct sc_ent *s2);

/* initialize the tree and queue */
RB_HEAD(sc_tree, sc_ent) sctree;
TAILQ_HEAD(sc_queue, sc_ent) scq1, scq2, scq_free;
RB_GENERATE(sc_tree, sc_ent, tlink, sc_cmp)

struct sc_queue *scq_act = NULL;
struct sc_queue *scq_exp = NULL;

int cache_max = 0;
int cache_size = 0;

struct sc_ent *sc_store = NULL;

/* preallocate the cache and insert into the 'free' queue */
int
cache_init(int max)
{
	int n;
	static int initialized = 0;

	if (max < 0 || initialized)
		return (1);

	if (max == 0) {
		sc_store = NULL;
	} else {
		sc_store = malloc(max * sizeof(struct sc_ent));
		if (sc_store == NULL)
			return (1);
	}

	RB_INIT(&sctree);
	TAILQ_INIT(&scq1);
	TAILQ_INIT(&scq2);
	TAILQ_INIT(&scq_free);

	scq_act = &scq1;
	scq_exp = &scq2;

	for (n = 0; n < max; n++)
		TAILQ_INSERT_HEAD(&scq_free, sc_store + n, qlink);

	cache_size = cache_max = max;
	initialized++;

	return (0);
}

void
update_state(struct sc_ent *prev, struct pfsync_state *new, double rate)
{
	assert (prev != NULL && new != NULL);
	prev->t = time(NULL);
	prev->rate = rate;
	prev->bytes = COUNTER(new->bytes[0]) + COUNTER(new->bytes[1]);
	if (prev->peak < rate)
		prev->peak = rate;
}

void
add_state(struct pfsync_state *st)
{
	struct sc_ent *ent;
	assert(st != NULL);

	if (cache_max == 0)
		return;

	if (TAILQ_EMPTY(&scq_free))
		return;

	ent = TAILQ_FIRST(&scq_free);
	TAILQ_REMOVE(&scq_free, ent, qlink);

	cache_size--;

	ent->id[0] = st->id[0];
	ent->id[1] = st->id[1];
	ent->bytes = COUNTER(st->bytes[0]) + COUNTER(st->bytes[1]);
	ent->peak = 0;
	ent->rate = 0;
	ent->t = time(NULL);

	RB_INSERT(sc_tree, &sctree, ent);
	TAILQ_INSERT_HEAD(scq_act, ent, qlink);
}

/* must be called only once for each state before cache_endupdate */
struct sc_ent *
cache_state(struct pfsync_state *st)
{
	struct sc_ent ent, *old;
	double sd, td, r;

	if (cache_max == 0)
		return (NULL);

	ent.id[0] = st->id[0];
	ent.id[1] = st->id[1];
	old = RB_FIND(sc_tree, &sctree, &ent);

	if (old == NULL) {
		add_state(st);
		return (NULL);
	}

	if (COUNTER(st->bytes[0]) + COUNTER(st->bytes[1]) < old->bytes)
		return (NULL);

	sd = COUNTER(st->bytes[0]) + COUNTER(st->bytes[1]) - old->bytes;
	td = time(NULL) - old->t;

	if (td > 0) {
		r = sd/td;
		update_state(old, st, r);		
	}

	/* move to active queue */
	TAILQ_REMOVE(scq_exp, old, qlink);
	TAILQ_INSERT_HEAD(scq_act, old, qlink);

	return (old);
}

/* remove the states that are not updated in this cycle */
void
cache_endupdate(void)
{
	struct sc_queue *tmp;
	struct sc_ent *ent;

	while (! TAILQ_EMPTY(scq_exp)) {
		ent = TAILQ_FIRST(scq_exp);
		TAILQ_REMOVE(scq_exp, ent, qlink);
		RB_REMOVE(sc_tree, &sctree, ent);
		TAILQ_INSERT_HEAD(&scq_free, ent, qlink);
		cache_size++;
	}

	tmp = scq_act;
	scq_act = scq_exp;
	scq_exp = tmp;
}

static __inline int
sc_cmp(struct sc_ent *a, struct sc_ent *b)
{
	if (a->id[0] > b->id[0])
		return (1);
	if (a->id[0] < b->id[0])
		return (-1);
	if (a->id[1] > b->id[1])
		return (1);
	if (a->id[1] < b->id[1])
		return (-1);
	return (0);
}
