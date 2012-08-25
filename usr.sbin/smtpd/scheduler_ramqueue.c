/*	$OpenBSD: scheduler_ramqueue.c,v 1.19 2012/08/25 10:23:12 gilles Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@openbsd.org>
 * Copyright (c) 2012 Eric Faurot <eric@openbsd.org>
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
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>
#include <sys/socket.h>

#include <ctype.h>
#include <err.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "smtpd.h"
#include "log.h"

TAILQ_HEAD(evplist, rq_envelope);

struct rq_message {
	uint32_t		 msgid;
	struct tree		 envelopes;
};

struct rq_envelope {
	TAILQ_ENTRY(rq_envelope) entry;

	uint64_t		 evpid;
	int			 type;

#define	RQ_ENVELOPE_INFLIGHT	 0x01
#define	RQ_ENVELOPE_EXPIRED	 0x02
	uint8_t			 flags;

	time_t			 sched;
	time_t			 expire;

	struct rq_message	*message;
	struct evplist		*queue;
};

struct rq_queue {
	struct tree		 messages;
	struct evplist		 mda;
	struct evplist		 mta;
	struct evplist		 bounce;
	struct evplist		 inflight;
	struct tree		 expired;
	struct tree		 removed;
};

static void scheduler_ramqueue_init(void);
static void scheduler_ramqueue_insert(struct scheduler_info *);
static void scheduler_ramqueue_commit(uint32_t);
static void scheduler_ramqueue_rollback(uint32_t);
static void scheduler_ramqueue_update(struct scheduler_info *);
static void scheduler_ramqueue_delete(uint64_t);
static void scheduler_ramqueue_batch(int, time_t, struct scheduler_batch *);
static void scheduler_ramqueue_schedule(uint64_t);
static void scheduler_ramqueue_remove(uint64_t);

static int  scheduler_ramqueue_next(int, uint64_t *, time_t *);
static void sorted_insert(struct evplist *, struct rq_envelope *);
static void sorted_merge(struct evplist *, struct evplist *);

static void rq_queue_init(struct rq_queue *);
static void rq_queue_merge(struct rq_queue *, struct rq_queue *);
static void rq_queue_dump(struct rq_queue *, const char *, time_t);
static void rq_envelope_delete(struct rq_queue *, struct rq_envelope *);
static const char *rq_envelope_to_text(struct rq_envelope *, time_t);

struct scheduler_backend scheduler_backend_ramqueue = {
	scheduler_ramqueue_init,

	scheduler_ramqueue_insert,
	scheduler_ramqueue_commit,
	scheduler_ramqueue_rollback,

	scheduler_ramqueue_update,
	scheduler_ramqueue_delete,

	scheduler_ramqueue_batch,

	scheduler_ramqueue_schedule,
	scheduler_ramqueue_remove,
};

static struct rq_queue		ramqueue;
static struct tree		updates;

extern int verbose;

static void
scheduler_ramqueue_init(void)
{
	rq_queue_init(&ramqueue);
	tree_init(&updates);
}

static void
scheduler_ramqueue_insert(struct scheduler_info *si)
{
	uint32_t		 msgid;
	struct rq_queue		*update;
	struct rq_message	*message;
	struct rq_envelope	*envelope;

	msgid = evpid_to_msgid(si->evpid);

	/* find/prepare a ramqueue update */
	if ((update = tree_get(&updates, msgid)) == NULL) {
		update = xcalloc(1, sizeof *update, "scheduler_insert");
		stat_increment("scheduler.ramqueue.update", 1);
		rq_queue_init(update);
		tree_xset(&updates, msgid, update);
	}

	/* find/prepare the msgtree message in ramqueue update */
	if ((message = tree_get(&update->messages, msgid)) == NULL) {
		message = xcalloc(1, sizeof *message, "scheduler_insert");
		message->msgid = msgid;
		tree_init(&message->envelopes);
		tree_xset(&update->messages, msgid, message);
		stat_increment("scheduler.ramqueue.message", 1);
	}

	/* create envelope in ramqueue message */
	envelope = xcalloc(1, sizeof *envelope, "scheduler_insert");
	envelope->evpid = si->evpid;
	envelope->type = si->type;
	envelope->message = message;
	envelope->sched = scheduler_compute_schedule(si);
	envelope->expire = si->creation + si->expire;

	stat_increment("scheduler.ramqueue.envelope", 1);

	if (envelope->expire < envelope->sched) {
		envelope->flags |= RQ_ENVELOPE_EXPIRED;
		tree_xset(&update->expired, envelope->evpid, envelope);
	}

	tree_xset(&message->envelopes, envelope->evpid, envelope);

	if (si->type == D_BOUNCE)
		envelope->queue = &update->bounce;
	else if (si->type == D_MDA)
		envelope->queue = &update->mda;
	else if (si->type == D_MTA)
		envelope->queue = &update->mta;
	else
		errx(1, "bad type");

	sorted_insert(envelope->queue, envelope);

	if (verbose & TRACE_SCHEDULER)
		rq_queue_dump(update, "inserted", time(NULL));
}

static void
scheduler_ramqueue_commit(uint32_t msgid)
{
	struct rq_queue	*update;
	time_t		 now;

	update = tree_xpop(&updates, msgid);

	if (verbose & TRACE_SCHEDULER) {
		now = time(NULL);
		rq_queue_dump(update, "commit update", now);
		rq_queue_dump(&ramqueue, "before commit", now);
	}
	rq_queue_merge(&ramqueue, update);

	if (verbose & TRACE_SCHEDULER)
		rq_queue_dump(&ramqueue, "after commit", time(NULL));

	free(update);
	stat_decrement("scheduler.ramqueue.update", 1);
}

static void
scheduler_ramqueue_rollback(uint32_t msgid)
{
	struct rq_queue		*update;
	struct rq_envelope	*envelope;

	update = tree_xpop(&updates, msgid);

	while ((envelope = TAILQ_FIRST(&update->bounce)))
		rq_envelope_delete(update, envelope);
	while ((envelope = TAILQ_FIRST(&update->mda)))
		rq_envelope_delete(update, envelope);
	while ((envelope = TAILQ_FIRST(&update->mta)))
		rq_envelope_delete(update, envelope);

	free(update);
	stat_decrement("scheduler.ramqueue.update", 1);
}

static void
scheduler_ramqueue_update(struct scheduler_info *si)
{
	struct rq_message	*message;
	struct rq_envelope	*envelope;
	uint32_t		 msgid;

	msgid = evpid_to_msgid(si->evpid);
	message = tree_xget(&ramqueue.messages, msgid);
	envelope = tree_xget(&message->envelopes, si->evpid);

	/* it *should* be in-flight */
	if (!(envelope->flags & RQ_ENVELOPE_INFLIGHT))
		log_warnx("evp:%016" PRIx64 " not in-flight", si->evpid);

	envelope->flags &= ~RQ_ENVELOPE_INFLIGHT;
	envelope->sched = scheduler_compute_schedule(si);
	if (envelope->expire < envelope->sched) {
		envelope->flags |= RQ_ENVELOPE_EXPIRED;
		tree_xset(&ramqueue.expired, envelope->evpid, envelope);
	}

	TAILQ_REMOVE(envelope->queue, envelope, entry);
	if (si->type == D_BOUNCE)
		envelope->queue = &ramqueue.bounce;
	else if (si->type == D_MDA)
		envelope->queue = &ramqueue.mda;
	else if (si->type == D_MTA)
		envelope->queue = &ramqueue.mta;

	sorted_insert(envelope->queue, envelope);
}

static void
scheduler_ramqueue_delete(uint64_t evpid)
{
	struct rq_message	*message;
	struct rq_envelope	*envelope;
	uint32_t		 msgid;

	msgid = evpid_to_msgid(evpid);
	message = tree_xget(&ramqueue.messages, msgid);
	envelope = tree_xget(&message->envelopes, evpid);

	/* it *must* be in-flight */
	if (!(envelope->flags & RQ_ENVELOPE_INFLIGHT))
		errx(1, "evp:%016" PRIx64 " not in-flight", evpid);

	rq_envelope_delete(&ramqueue, envelope);
}

static int
scheduler_ramqueue_next(int typemask, uint64_t *evpid, time_t *sched)
{
	struct rq_envelope	*evp_mda = NULL;
	struct rq_envelope	*evp_mta = NULL;
	struct rq_envelope	*evp_bounce = NULL;
	struct rq_envelope	*envelope = NULL;

	if (verbose & TRACE_SCHEDULER)
		rq_queue_dump(&ramqueue, "next", time(NULL));

	*sched = 0;

	if (typemask & SCHED_REMOVE && tree_root(&ramqueue.removed, evpid, NULL))
		return (1);
	if (typemask & SCHED_EXPIRE && tree_root(&ramqueue.expired, evpid, NULL))
		return (1);

	/* fetch first envelope from each queue */
	if (typemask & SCHED_BOUNCE)
		evp_bounce = TAILQ_FIRST(&ramqueue.bounce);
	if (typemask & SCHED_MDA)
		evp_mda = TAILQ_FIRST(&ramqueue.mda);
	if (typemask & SCHED_MTA)
		evp_mta = TAILQ_FIRST(&ramqueue.mta);

	/* set current envelope to either one */
	if (evp_bounce)
		envelope = evp_bounce;
	else if (evp_mda)
		envelope = evp_mda;
	else if (evp_mta)
		envelope = evp_mta;
	else
		return (0);

	/* figure out which one should be scheduled first */
	if (evp_bounce && evp_bounce->sched < envelope->sched)
		envelope = evp_bounce;
	if (evp_mda && evp_mda->sched < envelope->sched)
		envelope = evp_mda;
	if (evp_mta && evp_mta->sched < envelope->sched)
		envelope = evp_mta;

	*evpid = envelope->evpid;
	*sched = envelope->sched;

	return (1);
}

static void
scheduler_ramqueue_batch(int typemask, time_t curr, struct scheduler_batch *ret)
{
	struct rq_message	*message;
	struct rq_envelope	*envelope;
	struct id_list		*item;
	uint64_t		 evpid;
	void			*i;
	int			 type;
	time_t			 sched;

	ret->evpids = NULL;

	if (!scheduler_ramqueue_next(typemask, &evpid, &sched)) {
		ret->type = SCHED_NONE;
		return;
	}

	if (tree_get(&ramqueue.removed, evpid)) {
		ret->type = SCHED_REMOVE;
		while (tree_poproot(&ramqueue.removed, &evpid, NULL)) {
			item = xmalloc(sizeof *item, "schedule_batch");
			item->id = evpid;
			item->next = ret->evpids;
			ret->evpids = item;
		}
		return;
	}

	message = tree_xget(&ramqueue.messages, evpid_to_msgid(evpid));
	envelope = tree_xget(&message->envelopes, evpid);

	/* if the envelope has expired, return the expired list */
	if (envelope->flags & RQ_ENVELOPE_EXPIRED) {
		ret->type = SCHED_EXPIRE;
		while (tree_poproot(&ramqueue.expired, &evpid, (void**)&envelope)) {
			TAILQ_REMOVE(envelope->queue, envelope, entry);
			TAILQ_INSERT_TAIL(&ramqueue.inflight, envelope, entry);
			envelope->flags |= RQ_ENVELOPE_INFLIGHT;
			item = xmalloc(sizeof *item, "schedule_batch");
			item->id = evpid;
			item->next = ret->evpids;
			ret->evpids = item;
		}
		return;
	}

	if (sched > curr) {
		ret->type = SCHED_DELAY;
		ret->delay = sched - curr;
		return;
	}

	type = envelope->type;
	if (type == D_BOUNCE)
		ret->type = SCHED_BOUNCE;
	else if (type == D_MDA)
		ret->type = SCHED_MDA;
	else if (type == D_MTA)
		ret->type = SCHED_MTA;

	i = NULL;
	while((tree_iter(&message->envelopes, &i, &evpid, (void*)&envelope))) {
		if (envelope->type != type)
			continue;
		if (envelope->sched > curr)
			continue;
		if (envelope->flags & RQ_ENVELOPE_INFLIGHT)
			continue;
		if (envelope->flags & RQ_ENVELOPE_EXPIRED)
			continue;
		TAILQ_REMOVE(envelope->queue, envelope, entry);
		TAILQ_INSERT_TAIL(&ramqueue.inflight, envelope, entry);
		envelope->queue = &ramqueue.inflight;
		envelope->flags |= RQ_ENVELOPE_INFLIGHT;
		item = xmalloc(sizeof *item, "schedule_batch");
		item->id = evpid;
		item->next = ret->evpids;
		ret->evpids = item;
	}
}


static void
scheduler_ramqueue_schedule(uint64_t evpid)
{
	struct rq_message	*message;
	struct rq_envelope	*envelope;
	uint32_t		 msgid;
	void			*i, *j;

	if (evpid == 0) {
		j = NULL;
		while (tree_iter(&ramqueue.messages, &j, NULL,
		    (void*)(&message))) {

			i = NULL;
			while (tree_iter(&message->envelopes, &i, &evpid,
			    (void*)(&envelope))) {
				if (envelope->flags & RQ_ENVELOPE_INFLIGHT)
					continue;

				envelope->sched = time(NULL);
				TAILQ_REMOVE(envelope->queue, envelope, entry);
				sorted_insert(envelope->queue, envelope);
			}
		}
	}
	else if (evpid > 0xffffffff) {
		msgid = evpid_to_msgid(evpid);
		if ((message = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;
		if ((envelope = tree_get(&message->envelopes, evpid)) == NULL)
			return;
		if (envelope->flags & RQ_ENVELOPE_INFLIGHT)
			return;
       
		envelope->sched = time(NULL);
		TAILQ_REMOVE(envelope->queue, envelope, entry);
		sorted_insert(envelope->queue, envelope);
	}
	else {
		msgid = evpid;
		if ((message = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;

		i = NULL;
		while (tree_iter(&message->envelopes, &i, &evpid,
		    (void*)(&envelope))) {
			if (envelope->flags & RQ_ENVELOPE_INFLIGHT)
				continue;

			envelope->sched = time(NULL);
			TAILQ_REMOVE(envelope->queue, envelope, entry);
			sorted_insert(envelope->queue, envelope);
		}
	}

}

static void
scheduler_ramqueue_remove(uint64_t evpid)
{
	struct rq_message	*message;
	struct rq_envelope	*envelope;
	uint32_t		 msgid;
	struct evplist		 rmlist;
	void			*i;

	if (evpid > 0xffffffff) {
		msgid = evpid_to_msgid(evpid);
		if ((message = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;
		if ((envelope = tree_get(&message->envelopes, evpid)) == NULL)
			return;
		if (envelope->flags & RQ_ENVELOPE_INFLIGHT)
			return;
		rq_envelope_delete(&ramqueue, envelope);
		tree_xset(&ramqueue.removed, evpid, &ramqueue);
	}
	else {
		msgid = evpid;
		if ((message = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;

		TAILQ_INIT(&rmlist);
		i = NULL;
		while (tree_iter(&message->envelopes, &i, &evpid,
		    (void*)(&envelope))) {
			if (envelope->flags & RQ_ENVELOPE_INFLIGHT)
				continue;
			tree_xset(&ramqueue.removed, evpid, &ramqueue);
			TAILQ_REMOVE(envelope->queue, envelope, entry);
			envelope->queue = &rmlist;
			TAILQ_INSERT_HEAD(&rmlist, envelope, entry);
		}
		while((envelope = TAILQ_FIRST(&rmlist)))
			rq_envelope_delete(&ramqueue, envelope);
	}
}

static void
sorted_insert(struct evplist *list, struct rq_envelope *evp)
{
	struct rq_envelope	*item;

	TAILQ_FOREACH(item, list, entry) {
		if (evp->sched < item->sched) {
			TAILQ_INSERT_BEFORE(item, evp, entry);
			return;
		}
	}
	TAILQ_INSERT_TAIL(list, evp, entry);
}

static void
sorted_merge(struct evplist *list, struct evplist *from)
{
	struct rq_envelope	*e;

	/* XXX this is O(not good enough) */
	while ((e = TAILQ_LAST(from, evplist))) {
		TAILQ_REMOVE(from, e, entry);
		sorted_insert(list, e);
		e->queue = list;
	}
}

static void
rq_queue_init(struct rq_queue *rq)
{
	bzero(rq, sizeof *rq);

	tree_init(&rq->messages);
	tree_init(&rq->expired);
	tree_init(&rq->removed);
	TAILQ_INIT(&rq->mda);
	TAILQ_INIT(&rq->mta);
	TAILQ_INIT(&rq->bounce);
	TAILQ_INIT(&rq->inflight);
}

static void
rq_queue_merge(struct rq_queue *rq, struct rq_queue *update)
{
	struct rq_message	*message, *tomessage;
	struct rq_envelope	*envelope;
	uint64_t		 id;
	void			*i;

	while (tree_poproot(&update->messages, &id, (void*)&message)) {
		if ((tomessage = tree_get(&rq->messages, id)) == NULL) {
			/* message does not exist. re-use structure */
			tree_xset(&rq->messages, id, message);
			continue;
		}
		/* need to re-link all envelopes before merging them */
		i = NULL;
		while((tree_iter(&message->envelopes, &i, &id,
		    (void*)&envelope)))
			envelope->message = tomessage;
		tree_merge(&tomessage->envelopes, &message->envelopes);
		free(message);
	}

	sorted_merge(&rq->bounce, &update->bounce);
	sorted_merge(&rq->mda, &update->mda);
	sorted_merge(&rq->mta, &update->mta);

	tree_merge(&rq->expired, &update->expired);
	tree_merge(&rq->removed, &update->removed);
}

static void
rq_envelope_delete(struct rq_queue *rq, struct rq_envelope *envelope)
{
	struct rq_message	*message;
	uint32_t		 msgid;

	if (envelope->flags & RQ_ENVELOPE_EXPIRED)
		tree_pop(&rq->expired, envelope->evpid);

	TAILQ_REMOVE(envelope->queue, envelope, entry);
	message = envelope->message;
	msgid = message->msgid;

	tree_xpop(&message->envelopes, envelope->evpid);
	if (tree_empty(&message->envelopes)) {
		tree_xpop(&rq->messages, msgid);
		free(message);
		stat_decrement("scheduler.ramqueue.message", 1);
	}

	free(envelope);
	stat_decrement("scheduler.ramqueue.envelope", 1);
}

static const char *
rq_envelope_to_text(struct rq_envelope *e, time_t tref)
{
	static char	buf[256];
	char		t[64];

	snprintf(buf, sizeof buf, "evp:%016" PRIx64 " [", e->evpid);

	if (e->type == D_BOUNCE)
		strlcat(buf, "bounce", sizeof buf);
	else if (e->type == D_MDA)
		strlcat(buf, "mda", sizeof buf);
	else if (e->type == D_MTA)
		strlcat(buf, "mta", sizeof buf);

	snprintf(t, sizeof t, ",sched=%s", duration_to_text(e->sched - tref));
	strlcat(buf, t, sizeof buf);
	snprintf(t, sizeof t, ",exp=%s", duration_to_text(e->expire - tref));
	strlcat(buf, t, sizeof buf);

	if (e->flags & RQ_ENVELOPE_EXPIRED)
		strlcat(buf, ",expired", sizeof buf);
	if (e->flags & RQ_ENVELOPE_INFLIGHT)
		strlcat(buf, ",in-flight", sizeof buf);

	strlcat(buf, "]", sizeof buf);

	return (buf);
}

static void
rq_queue_dump(struct rq_queue *rq, const char * name, time_t tref)
{
	struct rq_message	*message;
	struct rq_envelope	*envelope;
	void			*i, *j;
	uint64_t		 id;

	log_debug("/--- ramqueue: %s", name);

	i = NULL;
	while((tree_iter(&rq->messages, &i, &id, (void*)&message))) {
		log_debug("| msg:%08" PRIx32, message->msgid);
		j = NULL;
		while((tree_iter(&message->envelopes, &j, &id,
		    (void*)&envelope)))
			log_debug("|   %s", rq_envelope_to_text(envelope, tref));
	}

	log_debug("| bounces:");
	TAILQ_FOREACH(envelope, &rq->bounce, entry)
		log_debug("|   %s", rq_envelope_to_text(envelope, tref));
	log_debug("| mda:");
	TAILQ_FOREACH(envelope, &rq->mda, entry)
		log_debug("|   %s", rq_envelope_to_text(envelope, tref));
	log_debug("| mta:");
	TAILQ_FOREACH(envelope, &rq->mta, entry)
		log_debug("|   %s", rq_envelope_to_text(envelope, tref));
	log_debug("| in-flight:");
	TAILQ_FOREACH(envelope, &rq->inflight, entry)
		log_debug("|   %s", rq_envelope_to_text(envelope, tref));
	log_debug("\\---");
}
