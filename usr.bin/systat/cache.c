/* $Id: cache.c,v 1.1 2008/06/12 22:26:01 canacar Exp $ */
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
void update_state(struct sc_ent *, pf_state_t *, double);
struct sc_ent *cache_state(pf_state_t *);
static __inline int sc_cmp(struct sc_ent *s1, struct sc_ent *s2);

/* initialize the tree and queue */
RB_HEAD(sc_tree, sc_ent) sctree;
TAILQ_HEAD(sc_queue, sc_ent) scq1, scq2, scq_free;
RB_GENERATE(sc_tree, sc_ent, tlink, sc_cmp);

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
update_state(struct sc_ent *prev, pf_state_t *new, double rate)
{
	assert (prev != NULL && new != NULL);
	prev->t = time(NULL);
	prev->rate = rate;
#ifdef HAVE_INOUT_COUNT
	prev->bytes = COUNTER(new->bytes[0]) + COUNTER(new->bytes[1]);
#else
	prev->bytes = COUNTER(new->bytes);
#endif
	if (prev->peak < rate)
		prev->peak = rate;
}

void
add_state(pf_state_t *st)
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

#ifdef HAVE_PFSYNC_STATE
	ent->id[0] = st->id[0];
	ent->id[1] = st->id[1];
#else
	ent->addr[0] = st->lan.addr;
	ent->port[0] = st->lan.port;
	ent->addr[1] = st->ext.addr;
	ent->port[1] = st->ext.port;
	ent->af = st->af;
	ent->proto = st->proto;
#endif
#ifdef HAVE_INOUT_COUNT
	ent->bytes = COUNTER(st->bytes[0]) + COUNTER(st->bytes[1]);
#else
	ent->bytes = st->bytes;
#endif
	ent->peak = 0;
	ent->rate = 0;
	ent->t = time(NULL);

	RB_INSERT(sc_tree, &sctree, ent);
	TAILQ_INSERT_HEAD(scq_act, ent, qlink);
}

/* must be called only once for each state before cache_endupdate */
struct sc_ent *
cache_state(pf_state_t *st)
{
	struct sc_ent ent, *old;
	double sd, td, r;

	if (cache_max == 0)
		return (NULL);

#ifdef HAVE_PFSYNC_STATE
	ent.id[0] = st->id[0];
	ent.id[1] = st->id[1];
#else
	ent.addr[0] = st->lan.addr;
	ent.port[0] = st->lan.port;
	ent.addr[1] = st->ext.addr;
	ent.port[1] = st->ext.port;
	ent.af = st->af;
	ent.proto = st->proto;
#endif
	old = RB_FIND(sc_tree, &sctree, &ent);

	if (old == NULL) {
		add_state(st);
		return (NULL);
	}

#ifdef HAVE_INOUT_COUNT
	if (COUNTER(st->bytes[0]) + COUNTER(st->bytes[1]) < old->bytes)
		return (NULL);

	sd = COUNTER(st->bytes[0]) + COUNTER(st->bytes[1]) - old->bytes;
#else
	if (st->bytes < old->bytes)
		return (NULL);

	sd = st->bytes - old->bytes;
#endif

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
#ifdef HAVE_PFSYNC_STATE
	if (a->id[0] > b->id[0])
		return (1);
	if (a->id[0] < b->id[0])
		return (-1);
	if (a->id[1] > b->id[1])
		return (1);
	if (a->id[1] < b->id[1])
		return (-1);
#else	
       	int diff;

	if ((diff = a->proto - b->proto) != 0)
		return (diff);
	if ((diff = a->af - b->af) != 0)
		return (diff);
	switch (a->af) {
	case AF_INET:
		if (a->addr[0].addr32[0] > b->addr[0].addr32[0])
			return (1);
		if (a->addr[0].addr32[0] < b->addr[0].addr32[0])
			return (-1);
		if (a->addr[1].addr32[0] > b->addr[1].addr32[0])
			return (1);
		if (a->addr[1].addr32[0] < b->addr[1].addr32[0])
			return (-1);
		break;
	case AF_INET6:
		if (a->addr[0].addr32[0] > b->addr[0].addr32[0])
			return (1);
		if (a->addr[0].addr32[0] < b->addr[0].addr32[0])
			return (-1);
		if (a->addr[0].addr32[1] > b->addr[0].addr32[1])
			return (1);
		if (a->addr[0].addr32[1] < b->addr[0].addr32[1])
			return (-1);
		if (a->addr[0].addr32[2] > b->addr[0].addr32[2])
			return (1);
		if (a->addr[0].addr32[2] < b->addr[0].addr32[2])
			return (-1);
		if (a->addr[0].addr32[3] > b->addr[0].addr32[3])
			return (1);
		if (a->addr[0].addr32[3] < b->addr[0].addr32[3])
			return (-1);
		if (a->addr[1].addr32[0] > b->addr[1].addr32[0])
			return (1);
		if (a->addr[1].addr32[0] < b->addr[1].addr32[0])
			return (-1);
		if (a->addr[1].addr32[1] > b->addr[1].addr32[1])
			return (1);
		if (a->addr[1].addr32[1] < b->addr[1].addr32[1])
			return (-1);
		if (a->addr[1].addr32[2] > b->addr[1].addr32[2])
			return (1);
		if (a->addr[1].addr32[2] < b->addr[1].addr32[2])
			return (-1);
		if (a->addr[1].addr32[3] > b->addr[1].addr32[3])
			return (1);
		if (a->addr[1].addr32[3] < b->addr[1].addr32[3])
			return (-1);
		break;
		default:
			return 1;
	}

	if ((diff = a->port[0] - b->port[0]) != 0)
		return (diff);
	if ((diff = a->port[1] - b->port[1]) != 0)
		return (diff);
#endif
	return (0);
}
