/*	$OpenBSD: scheduler_ramqueue.c,v 1.26 2013/01/26 09:37:23 gilles Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@poolp.org>
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
	struct rq_message	*q_next;
	struct evplist		 q_mta;
};

struct rq_envelope {
	TAILQ_ENTRY(rq_envelope) entry;

	uint64_t		 evpid;
	int			 type;

#define	RQ_ENVELOPE_PENDING	 0x01
#define	RQ_ENVELOPE_SCHEDULED	 0x02
#define	RQ_ENVELOPE_EXPIRED	 0x04
#define	RQ_ENVELOPE_REMOVED	 0x08
#define	RQ_ENVELOPE_INFLIGHT	 0x10
	uint8_t			 flags;

	time_t			 sched;
	time_t			 expire;

	struct rq_message	*message;

	time_t			 t_inflight;
	time_t			 t_scheduled;
};

struct rq_queue {
	size_t			 evpcount;
	struct tree		 messages;

	struct evplist		 q_pending;
	struct evplist		 q_inflight;

	struct rq_message	*q_mtabatch;
	struct evplist		 q_mda;
	struct evplist		 q_bounce;
	struct evplist		 q_expired;
	struct evplist		 q_removed;
};

static void scheduler_ramqueue_init(void);
static void scheduler_ramqueue_insert(struct scheduler_info *);
static size_t scheduler_ramqueue_commit(uint32_t);
static size_t scheduler_ramqueue_rollback(uint32_t);
static void scheduler_ramqueue_update(struct scheduler_info *);
static void scheduler_ramqueue_delete(uint64_t);
static void scheduler_ramqueue_batch(int, struct scheduler_batch *);
static size_t scheduler_ramqueue_messages(uint32_t, uint32_t *, size_t);
static size_t scheduler_ramqueue_envelopes(uint64_t, struct evpstate *, size_t);
static void scheduler_ramqueue_schedule(uint64_t);
static void scheduler_ramqueue_remove(uint64_t);

static void sorted_insert(struct evplist *, struct rq_envelope *);
static void sorted_merge(struct evplist *, struct evplist *);

static void rq_queue_init(struct rq_queue *);
static void rq_queue_merge(struct rq_queue *, struct rq_queue *);
static void rq_queue_dump(struct rq_queue *, const char *);
static void rq_queue_schedule(struct rq_queue *rq);
static void rq_envelope_schedule(struct rq_queue *, struct rq_envelope *);
static void rq_envelope_remove(struct rq_queue *, struct rq_envelope *);
static void rq_envelope_delete(struct rq_queue *, struct rq_envelope *);
static const char *rq_envelope_to_text(struct rq_envelope *);

struct scheduler_backend scheduler_backend_ramqueue = {
	scheduler_ramqueue_init,

	scheduler_ramqueue_insert,
	scheduler_ramqueue_commit,
	scheduler_ramqueue_rollback,

	scheduler_ramqueue_update,
	scheduler_ramqueue_delete,

	scheduler_ramqueue_batch,

	scheduler_ramqueue_messages,
	scheduler_ramqueue_envelopes,
	scheduler_ramqueue_schedule,
	scheduler_ramqueue_remove,
};

static struct rq_queue	ramqueue;
static struct tree	updates;

static time_t		currtime;

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

	currtime = time(NULL);

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
		TAILQ_INIT(&message->q_mta);
		tree_init(&message->envelopes);
		tree_xset(&update->messages, msgid, message);
		stat_increment("scheduler.ramqueue.message", 1);
	}

	/* create envelope in ramqueue message */
	envelope = xcalloc(1, sizeof *envelope, "scheduler_insert");
	envelope->evpid = si->evpid;
	envelope->type = si->type;
	envelope->message = message;
	envelope->expire = si->creation + si->expire;
	envelope->sched = scheduler_compute_schedule(si);
	tree_xset(&message->envelopes, envelope->evpid, envelope);

	update->evpcount++;
	stat_increment("scheduler.ramqueue.envelope", 1);

	envelope->flags = RQ_ENVELOPE_PENDING;
	sorted_insert(&update->q_pending, envelope);

	si->nexttry = envelope->sched;
}

static size_t
scheduler_ramqueue_commit(uint32_t msgid)
{
	struct rq_queue	*update;
	size_t		 r;

	currtime = time(NULL);

	update = tree_xpop(&updates, msgid);
	r = update->evpcount;

	if (verbose & TRACE_SCHEDULER)
		rq_queue_dump(update, "update to commit");

	rq_queue_merge(&ramqueue, update);

	if (verbose & TRACE_SCHEDULER)
		rq_queue_dump(&ramqueue, "resulting queue");

	rq_queue_schedule(&ramqueue);

	free(update);
	stat_decrement("scheduler.ramqueue.update", 1);

	return (r);
}

static size_t
scheduler_ramqueue_rollback(uint32_t msgid)
{
	struct rq_queue		*update;
	struct rq_envelope	*evp;
	size_t			 r;

	currtime = time(NULL);

	if ((update = tree_pop(&updates, msgid)) == NULL)
		return (0);
	r = update->evpcount;

	while ((evp = TAILQ_FIRST(&update->q_pending))) {
		TAILQ_REMOVE(&update->q_pending, evp, entry);
		rq_envelope_delete(update, evp);
	}

	free(update);
	stat_decrement("scheduler.ramqueue.update", 1);

	return (r);
}

static void
scheduler_ramqueue_update(struct scheduler_info *si)
{
	struct rq_message	*msg;
	struct rq_envelope	*evp;
	uint32_t		 msgid;

	currtime = time(NULL);

	msgid = evpid_to_msgid(si->evpid);
	msg = tree_xget(&ramqueue.messages, msgid);
	evp = tree_xget(&msg->envelopes, si->evpid);

	/* it *must* be in-flight */
	if (!(evp->flags & RQ_ENVELOPE_INFLIGHT))
		errx(1, "evp:%016" PRIx64 " not in-flight", si->evpid);

	while ((evp->sched = scheduler_compute_schedule(si)) <= currtime)
		si->retry += 1;

	TAILQ_REMOVE(&ramqueue.q_inflight, evp, entry);
	sorted_insert(&ramqueue.q_pending, evp);
	evp->flags &= ~RQ_ENVELOPE_INFLIGHT;
	evp->flags |= RQ_ENVELOPE_PENDING;

	si->nexttry = evp->sched;
}

static void
scheduler_ramqueue_delete(uint64_t evpid)
{
	struct rq_message	*msg;
	struct rq_envelope	*evp;
	uint32_t		 msgid;

	currtime = time(NULL);

	msgid = evpid_to_msgid(evpid);
	msg = tree_xget(&ramqueue.messages, msgid);
	evp = tree_xget(&msg->envelopes, evpid);

	/* it *must* be in-flight */
	if (!(evp->flags & RQ_ENVELOPE_INFLIGHT))
		errx(1, "evp:%016" PRIx64 " not in-flight", evpid);

	TAILQ_REMOVE(&ramqueue.q_inflight, evp, entry);
	evp->flags &= ~RQ_ENVELOPE_INFLIGHT;
	rq_envelope_delete(&ramqueue, evp);
}

static void
scheduler_ramqueue_batch(int typemask, struct scheduler_batch *ret)
{
	struct evplist		*q;
	struct rq_envelope	*evp;
	struct rq_message	*msg;
	struct id_list		*item;

	currtime = time(NULL);

	rq_queue_schedule(&ramqueue);
	if (verbose & TRACE_SCHEDULER)
		rq_queue_dump(&ramqueue, "scheduler_ramqueue_batch()");

	if (typemask & SCHED_REMOVE && TAILQ_FIRST(&ramqueue.q_removed)) {
		q = &ramqueue.q_removed;
		ret->type = SCHED_REMOVE;
	}
	else if (typemask & SCHED_EXPIRE && TAILQ_FIRST(&ramqueue.q_expired)) {
		q = &ramqueue.q_expired;
		ret->type = SCHED_EXPIRE;
	}
	else if (typemask & SCHED_BOUNCE && TAILQ_FIRST(&ramqueue.q_bounce)) {
		q = &ramqueue.q_bounce;
		ret->type = SCHED_BOUNCE;
	}
	else if (typemask & SCHED_MDA && TAILQ_FIRST(&ramqueue.q_mda)) {
		q = &ramqueue.q_mda;
		ret->type = SCHED_MDA;
	}
	else if (typemask & SCHED_MTA && ramqueue.q_mtabatch) {
		msg = ramqueue.q_mtabatch;
		ramqueue.q_mtabatch = msg->q_next;
		msg->q_next = NULL;
		q = &msg->q_mta;
		ret->type = SCHED_MTA;
	}
	else if ((evp = TAILQ_FIRST(&ramqueue.q_pending))) {
		ret->type = SCHED_DELAY;
		if (evp->sched < evp->expire)
			ret->delay = evp->sched - currtime;
		else
			ret->delay = evp->expire - currtime;
		return;
	}
	else {
		ret->type = SCHED_NONE;
		return;
	}

	ret->evpids = NULL;
	ret->evpcount = 0;

	while ((evp = TAILQ_FIRST(q))) {

		TAILQ_REMOVE(q, evp, entry);

		/* consistency check */ 
		if (!(evp->flags & RQ_ENVELOPE_SCHEDULED))
			errx(1, "evp:%016" PRIx64 " not scheduled", evp->evpid);

		item = xmalloc(sizeof *item, "schedule_batch");
		item->id = evp->evpid;
		item->next = ret->evpids;
		ret->evpids = item;

		if (ret->type == SCHED_REMOVE || ret->type == SCHED_EXPIRE)
			rq_envelope_delete(&ramqueue, evp);
		else {
			TAILQ_INSERT_TAIL(&ramqueue.q_inflight, evp, entry);
			evp->flags &= ~RQ_ENVELOPE_SCHEDULED;
			evp->flags |= RQ_ENVELOPE_INFLIGHT;
			evp->t_inflight = currtime;
		}
		ret->evpcount++;
	}
}

static void
scheduler_ramqueue_schedule(uint64_t evpid)
{
	struct rq_message	*msg;
	struct rq_envelope	*evp;
	uint32_t		 msgid;
	void			*i;

	currtime = time(NULL);

	if (evpid > 0xffffffff) {
		msgid = evpid_to_msgid(evpid);
		if ((msg = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;
		if ((evp = tree_get(&msg->envelopes, evpid)) == NULL)
			return;
		if (evp->flags & RQ_ENVELOPE_PENDING)
			rq_envelope_schedule(&ramqueue, evp);
	}
	else {
		msgid = evpid;
		if ((msg = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;
		i = NULL;
		while (tree_iter(&msg->envelopes, &i, NULL, (void*)(&evp)))
			if (evp->flags & RQ_ENVELOPE_PENDING)
				rq_envelope_schedule(&ramqueue, evp);
	}
}

static void
scheduler_ramqueue_remove(uint64_t evpid)
{
	struct rq_message	*msg;
	struct rq_envelope	*evp;
	uint32_t		 msgid;
	void			*i;

	currtime = time(NULL);

	if (evpid > 0xffffffff) {
		msgid = evpid_to_msgid(evpid);
		if ((msg = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;
		if ((evp = tree_get(&msg->envelopes, evpid)) == NULL)
			return;
		rq_envelope_remove(&ramqueue, evp);
	}
	else {
		msgid = evpid;
		if ((msg = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;
		i = NULL;
		while (tree_iter(&msg->envelopes, &i, NULL, (void*)(&evp)))
			rq_envelope_remove(&ramqueue, evp);
	}
}

static size_t
scheduler_ramqueue_messages(uint32_t from, uint32_t *dst, size_t size)
{
	uint64_t id;
	size_t	 n;
	void	*i;

	for (n = 0, i = NULL; n < size; n++) {
		if (tree_iterfrom(&ramqueue.messages, &i, from, &id, NULL) == 0)
			break;
		dst[n] = id;
	}

	return (n);
}

static size_t
scheduler_ramqueue_envelopes(uint64_t from, struct evpstate *dst, size_t size)
{
	struct rq_message	*msg;
	struct rq_envelope	*evp;
	void			*i;
	size_t			 n;

	if ((msg = tree_get(&ramqueue.messages, evpid_to_msgid(from))) == NULL)
		return (0);

	for (n = 0, i = NULL; n < size; ) {

		if (tree_iterfrom(&msg->envelopes, &i, from, NULL,
		    (void**)&evp) == 0)
			break;

		if (evp->flags & (RQ_ENVELOPE_REMOVED | RQ_ENVELOPE_EXPIRED))
			continue;

		dst[n].retry = 0;
		dst[n].evpid = evp->evpid;
		if (evp->flags & RQ_ENVELOPE_PENDING) {
			dst[n].time = evp->sched;
			dst[n].flags = EF_PENDING;
		}
		else if (evp->flags & RQ_ENVELOPE_SCHEDULED) {
			dst[n].time = evp->t_scheduled;
			dst[n].flags = EF_PENDING;
		}
		else if (evp->flags & RQ_ENVELOPE_INFLIGHT) {
			dst[n].time = evp->t_inflight;
			dst[n].flags = EF_INFLIGHT;
		}
		n++;
	}

	return (n);
}

static void
sorted_insert(struct evplist *list, struct rq_envelope *evp)
{
	struct rq_envelope	*item;
	time_t			 ref;

	TAILQ_FOREACH(item, list, entry) {
		ref = (evp->sched < evp->expire) ? evp->sched : evp->expire;
		if (ref <= item->expire && ref <= item->sched) {
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
	}
}

static void
rq_queue_init(struct rq_queue *rq)
{
	bzero(rq, sizeof *rq);
	tree_init(&rq->messages);
	TAILQ_INIT(&rq->q_pending);
	TAILQ_INIT(&rq->q_inflight);
	TAILQ_INIT(&rq->q_mda);
	TAILQ_INIT(&rq->q_bounce);
	TAILQ_INIT(&rq->q_expired);
	TAILQ_INIT(&rq->q_removed);
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
		while ((tree_iter(&message->envelopes, &i, &id,
		    (void*)&envelope)))
			envelope->message = tomessage;
		tree_merge(&tomessage->envelopes, &message->envelopes);
		free(message);
		stat_decrement("scheduler.ramqueue.message", 1);
	}

	sorted_merge(&rq->q_pending, &update->q_pending);
	rq->evpcount += update->evpcount;
}

static void
rq_queue_schedule(struct rq_queue *rq)
{
	struct rq_envelope	*evp;

	while ((evp = TAILQ_FIRST(&rq->q_pending))) {
		if (evp->sched > currtime && evp->expire > currtime)
			break;

		if (evp->flags != RQ_ENVELOPE_PENDING)
			errx(1, "evp:%016" PRIx64 " flags=0x%x", evp->evpid,
			    evp->flags);

		if (evp->expire <= currtime) {
			TAILQ_REMOVE(&rq->q_pending, evp, entry);
			TAILQ_INSERT_TAIL(&rq->q_expired, evp, entry);
			evp->flags &= ~RQ_ENVELOPE_PENDING;
			evp->flags |= RQ_ENVELOPE_EXPIRED;
			evp->flags |= RQ_ENVELOPE_SCHEDULED;
			evp->t_scheduled = currtime;
			continue;
		}
		rq_envelope_schedule(rq, evp);
	}
}

static void
rq_envelope_schedule(struct rq_queue *rq, struct rq_envelope *evp)
{
	struct evplist	*q = NULL;

	if (evp->type == D_MTA) {
		if (TAILQ_EMPTY(&evp->message->q_mta)) {
			evp->message->q_next = rq->q_mtabatch;
			rq->q_mtabatch = evp->message;
		}
		q = &evp->message->q_mta;
	}
	else if (evp->type == D_MDA)
		q = &rq->q_mda;
	else if (evp->type == D_BOUNCE)
		q = &rq->q_bounce;

	TAILQ_REMOVE(&rq->q_pending, evp, entry);
	TAILQ_INSERT_TAIL(q, evp, entry);
	evp->flags &= ~RQ_ENVELOPE_PENDING;
	evp->flags |= RQ_ENVELOPE_SCHEDULED;
	evp->t_scheduled = currtime;
}

static void
rq_envelope_remove(struct rq_queue *rq, struct rq_envelope *evp)
{
	struct rq_message	*m;
	struct evplist		*q = NULL;

	if (evp->flags & (RQ_ENVELOPE_REMOVED | RQ_ENVELOPE_EXPIRED))
		return;
	/*
	 * For now we just ignore it, but we could mark the envelope for
	 * removal and possibly send a cancellation to the agent.
	 */
	if (evp->flags & (RQ_ENVELOPE_INFLIGHT))
		return;

	if (evp->flags & RQ_ENVELOPE_SCHEDULED) {
		if (evp->type == D_MTA)
			q = &evp->message->q_mta;
		else if (evp->type == D_MDA)
			q = &rq->q_mda;
		else if (evp->type == D_BOUNCE)
			q = &rq->q_bounce;
	} else
		q = &rq->q_pending;

	TAILQ_REMOVE(q, evp, entry);
	TAILQ_INSERT_TAIL(&rq->q_removed, evp, entry);
	evp->flags &= ~RQ_ENVELOPE_PENDING;
	evp->flags |= RQ_ENVELOPE_REMOVED;
	evp->flags |= RQ_ENVELOPE_SCHEDULED;
	evp->t_scheduled = currtime;

	/*
	 * We might need to unschedule the message if it was the only
	 * scheduled envelope
	 */
	if (q == &evp->message->q_mta && TAILQ_EMPTY(q)) {
		if (rq->q_mtabatch == evp->message)
			rq->q_mtabatch = evp->message->q_next;
		else {
			for (m = rq->q_mtabatch; m->q_next; m = m->q_next)
				if (m->q_next == evp->message) {
					m->q_next = evp->message->q_next;
					break;
				}
		}
		evp->message->q_next = NULL;
	}
}

static void
rq_envelope_delete(struct rq_queue *rq, struct rq_envelope *evp)
{
	tree_xpop(&evp->message->envelopes, evp->evpid);
	if (tree_empty(&evp->message->envelopes)) {
		tree_xpop(&rq->messages, evp->message->msgid);
		free(evp->message);
		stat_decrement("scheduler.ramqueue.message", 1);
	}

	free(evp);
	rq->evpcount--;
	stat_decrement("scheduler.ramqueue.envelope", 1);
}

static const char *
rq_envelope_to_text(struct rq_envelope *e)
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

	snprintf(t, sizeof t, ",expire=%s",
	    duration_to_text(e->expire - currtime));
	strlcat(buf, t, sizeof buf);

	if (e->flags & RQ_ENVELOPE_PENDING) {
		snprintf(t, sizeof t, ",pending=%s",
		    duration_to_text(e->sched - currtime));
		strlcat(buf, t, sizeof buf);
	}
	if (e->flags & RQ_ENVELOPE_SCHEDULED) {
		snprintf(t, sizeof t, ",scheduled=%s",
		    duration_to_text(currtime - e->t_scheduled));
		strlcat(buf, t, sizeof buf);
	}
	if (e->flags & RQ_ENVELOPE_INFLIGHT) {
		snprintf(t, sizeof t, ",inflight=%s",
		    duration_to_text(currtime - e->t_inflight));
		strlcat(buf, t, sizeof buf);
	}
	if (e->flags & RQ_ENVELOPE_REMOVED)
		strlcat(buf, ",removed", sizeof buf);
	if (e->flags & RQ_ENVELOPE_EXPIRED)
		strlcat(buf, ",expired", sizeof buf);

	strlcat(buf, "]", sizeof buf);

	return (buf);
}

static void
rq_queue_dump(struct rq_queue *rq, const char * name)
{
	struct rq_message	*message;
	struct rq_envelope	*envelope;
	void			*i, *j;
	uint64_t		 id;

	log_debug("debug: /--- ramqueue: %s", name);

	i = NULL;
	while ((tree_iter(&rq->messages, &i, &id, (void*)&message))) {
		log_debug("debug: | msg:%08" PRIx32, message->msgid);
		j = NULL;
		while ((tree_iter(&message->envelopes, &j, &id,
		    (void*)&envelope)))
			log_debug("debug: |   %s",
			    rq_envelope_to_text(envelope));
	}
	log_debug("debug: \\---");
}
