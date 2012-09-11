/*	$OpenBSD: scheduler_ramqueue.c,v 1.21 2012/09/11 08:37:52 eric Exp $	*/

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
	struct rq_message	*sched_next;
	struct rq_envelope	*sched_mta;
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

	struct rq_envelope	*sched_next;
	time_t			 t_inflight;
	time_t			 t_scheduled;
};

struct rq_queue {
	struct tree		 messages;

	struct evplist		 pending;

	struct rq_message	*sched_mta;
	struct rq_envelope	*sched_mda;
	struct rq_envelope	*sched_bounce;
	struct rq_envelope	*sched_expired;
	struct rq_envelope	*sched_removed;

};

static void scheduler_ramqueue_init(void);
static void scheduler_ramqueue_insert(struct scheduler_info *);
static void scheduler_ramqueue_commit(uint32_t);
static void scheduler_ramqueue_rollback(uint32_t);
static void scheduler_ramqueue_update(struct scheduler_info *);
static void scheduler_ramqueue_delete(uint64_t);
static void scheduler_ramqueue_batch(int, struct scheduler_batch *);
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

	scheduler_ramqueue_schedule,
	scheduler_ramqueue_remove,
};

static struct rq_queue	ramqueue;
static struct tree	updates;

static time_t		currtime;	

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

	stat_increment("scheduler.ramqueue.envelope", 1);

	envelope->flags = RQ_ENVELOPE_PENDING;
	sorted_insert(&update->pending, envelope);
}

static void
scheduler_ramqueue_commit(uint32_t msgid)
{
	struct rq_queue	*update;

	currtime = time(NULL);

	update = tree_xpop(&updates, msgid);

	if (verbose & TRACE_SCHEDULER)
		rq_queue_dump(update, "update to commit");

	rq_queue_merge(&ramqueue, update);
	rq_queue_schedule(&ramqueue);

	free(update);

	stat_decrement("scheduler.ramqueue.update", 1);
}

static void
scheduler_ramqueue_rollback(uint32_t msgid)
{
	struct rq_queue		*update;
	struct rq_envelope	*evp;

	currtime = time(NULL);

	if ((update = tree_pop(&updates, msgid)) == NULL)
		return;

	while ((evp = TAILQ_FIRST(&update->pending))) {
		TAILQ_REMOVE(&update->pending, evp, entry);
		rq_envelope_delete(update, evp);
	}

	free(update);
	stat_decrement("scheduler.ramqueue.update", 1);
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

	evp->flags &= ~RQ_ENVELOPE_INFLIGHT;
	evp->flags |= RQ_ENVELOPE_PENDING;
	sorted_insert(&ramqueue.pending, evp);
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

	evp->flags &= ~RQ_ENVELOPE_INFLIGHT;
	rq_envelope_delete(&ramqueue, evp);
}

static void
scheduler_ramqueue_batch(int typemask, struct scheduler_batch *ret)
{
	struct rq_envelope	*evp, *tmp, **batch;
	struct id_list		*item;

	currtime = time(NULL);

	rq_queue_schedule(&ramqueue);
	if (verbose & TRACE_SCHEDULER)
		rq_queue_dump(&ramqueue, "scheduler_ramqueue_batch()");

	if (typemask & SCHED_REMOVE && ramqueue.sched_removed) {
		batch = &ramqueue.sched_removed;
		ret->type = SCHED_REMOVE;
	}
	else if (typemask & SCHED_EXPIRE && ramqueue.sched_expired) {
		batch = &ramqueue.sched_expired;
		ret->type = SCHED_EXPIRE;
	}
	else if (typemask & SCHED_BOUNCE && ramqueue.sched_bounce) {
		batch = &ramqueue.sched_bounce;
		ret->type = SCHED_BOUNCE;
	}
	else if (typemask & SCHED_MDA && ramqueue.sched_mda) {
		batch = &ramqueue.sched_mda;
		ret->type = SCHED_MDA;
	}
	else if (typemask & SCHED_MTA && ramqueue.sched_mta) {
		batch = &ramqueue.sched_mta->sched_mta;
		ramqueue.sched_mta = ramqueue.sched_mta->sched_next;
		ret->type = SCHED_MTA;
	}
	else if ((evp = TAILQ_FIRST(&ramqueue.pending))) {
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
	for(evp = *batch; evp; evp = tmp) {
		tmp = evp->sched_next;

		/* consistency check */ 
		if (!(evp->flags & RQ_ENVELOPE_SCHEDULED))
			errx(1, "evp:%016" PRIx64 " not scheduled", evp->evpid);

		item = xmalloc(sizeof *item, "schedule_batch");
		item->id = evp->evpid;
		item->next = ret->evpids;
		ret->evpids = item;
		evp->sched_next = NULL;
		if (ret->type == SCHED_REMOVE || ret->type == SCHED_EXPIRE)
			rq_envelope_delete(&ramqueue, evp);
		else {
			evp->flags &= ~RQ_ENVELOPE_SCHEDULED;
			evp->flags |= RQ_ENVELOPE_INFLIGHT;
			evp->t_inflight = currtime;
		}
	}

	*batch = NULL;
}

static void
scheduler_ramqueue_schedule(uint64_t evpid)
{
	struct rq_message	*msg;
	struct rq_envelope	*evp;
	uint32_t		 msgid;
	void			*i, *j;

	currtime = time(NULL);

	if (evpid == 0) {
		j = NULL;
		while (tree_iter(&ramqueue.messages, &j, NULL, (void*)(&msg))) {
			i = NULL;
			while (tree_iter(&msg->envelopes, &i, NULL,
			    (void*)(&evp)))
				rq_envelope_schedule(&ramqueue, evp);
		}
	}
	else if (evpid > 0xffffffff) {
		msgid = evpid_to_msgid(evpid);
		if ((msg = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;
		if ((evp = tree_get(&msg->envelopes, evpid)) == NULL)
			return;
		rq_envelope_schedule(&ramqueue, evp);
	}
	else {
		msgid = evpid;
		if ((msg = tree_get(&ramqueue.messages, msgid)) == NULL)
			return;
		i = NULL;
		while (tree_iter(&msg->envelopes, &i, NULL, (void*)(&evp)))
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
	TAILQ_INIT(&rq->pending);
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

	sorted_merge(&rq->pending, &update->pending);
}

static void
rq_queue_schedule(struct rq_queue *rq)
{
	struct rq_envelope	*evp;

	while ((evp = TAILQ_FIRST(&rq->pending))) {
		if (evp->sched > currtime && evp->expire > currtime)
			break;

		/* it *must* be pending */
		if (evp->flags != RQ_ENVELOPE_PENDING)
			errx(1, "evp:%016" PRIx64 " flags=0x%x", evp->evpid,
			    evp->flags);

		if (evp->expire <= currtime) {
			TAILQ_REMOVE(&rq->pending, evp, entry);
			evp->flags &= ~RQ_ENVELOPE_PENDING;
			evp->flags |= RQ_ENVELOPE_EXPIRED;
			evp->flags |= RQ_ENVELOPE_SCHEDULED;
			evp->t_scheduled = currtime;
			evp->sched_next = rq->sched_expired;
			rq->sched_expired = evp;
			continue;
		}
		rq_envelope_schedule(rq, evp);
	}
}

static void
rq_envelope_schedule(struct rq_queue *rq, struct rq_envelope *evp)
{
	if (evp->flags & (RQ_ENVELOPE_SCHEDULED | RQ_ENVELOPE_INFLIGHT))
		return;

	if (evp->flags & RQ_ENVELOPE_PENDING)
		TAILQ_REMOVE(&rq->pending, evp, entry);

	if (evp->type == D_MTA) {
		if (evp->message->sched_mta == NULL) {
			evp->message->sched_next = rq->sched_mta;
			rq->sched_mta = evp->message;
		}
		evp->sched_next = evp->message->sched_mta;
		evp->message->sched_mta = evp;
	}
	else if (evp->type == D_MDA) {
		evp->sched_next = rq->sched_mda;
		rq->sched_mda = evp;
	}
	else if (evp->type == D_BOUNCE) {
		evp->sched_next = rq->sched_bounce;
		rq->sched_bounce = evp;
	}
	evp->flags &= ~RQ_ENVELOPE_PENDING;
	evp->flags |= RQ_ENVELOPE_SCHEDULED;
	evp->t_scheduled = currtime;
}

static void
rq_envelope_remove(struct rq_queue *rq, struct rq_envelope *evp)
{
	if (!(evp->flags & (RQ_ENVELOPE_PENDING)))
		return;

	TAILQ_REMOVE(&rq->pending, evp, entry);
	evp->sched_next = rq->sched_removed;
	rq->sched_removed = evp;
	evp->flags &= ~RQ_ENVELOPE_PENDING;
	evp->flags |= RQ_ENVELOPE_REMOVED;
	evp->flags |= RQ_ENVELOPE_SCHEDULED;
	evp->t_scheduled = currtime;
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

	snprintf(t, sizeof t, ",expire=%s", duration_to_text(e->expire - currtime));
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

	log_debug("/--- ramqueue: %s", name);

	i = NULL;
	while((tree_iter(&rq->messages, &i, &id, (void*)&message))) {
		log_debug("| msg:%08" PRIx32, message->msgid);
		j = NULL;
		while((tree_iter(&message->envelopes, &j, &id,
		    (void*)&envelope)))
			log_debug("|   %s",
			    rq_envelope_to_text(envelope));
	}
	log_debug("\\---");
}
