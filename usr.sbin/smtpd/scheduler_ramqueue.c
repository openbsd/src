/*	$OpenBSD: scheduler_ramqueue.c,v 1.3 2012/01/30 10:02:55 chl Exp $	*/

/*
 * Copyright (c) 2012 Gilles Chehade <gilles@openbsd.org>
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

#include "smtpd.h"
#include "log.h"


struct ramqueue_host {
	RB_ENTRY(ramqueue_host)		hosttree_entry;
	TAILQ_HEAD(,ramqueue_batch)	batch_queue;
	u_int64_t			h_id;
	char				hostname[MAXHOSTNAMELEN];
};
struct ramqueue_batch {
	enum delivery_type		type;
	TAILQ_ENTRY(ramqueue_batch)	batch_entry;
	TAILQ_HEAD(,ramqueue_envelope)	envelope_queue;
	u_int64_t			h_id;
	u_int64_t			b_id;
	u_int32_t      			msgid;
};
struct ramqueue_envelope {
	TAILQ_ENTRY(ramqueue_envelope)	 queue_entry;
	TAILQ_ENTRY(ramqueue_envelope)	 batchqueue_entry;
	RB_ENTRY(ramqueue_envelope)	 evptree_entry;
	struct ramqueue_batch		*rq_batch;
	struct ramqueue_message		*rq_msg;
	struct ramqueue_host		*rq_host;
	u_int64_t      			 evpid;
	time_t				 sched;
};
struct ramqueue_message {
	RB_ENTRY(ramqueue_message)		msgtree_entry;
	RB_HEAD(evptree, ramqueue_envelope)	evptree;
	u_int32_t				msgid;
};
struct ramqueue {
	RB_HEAD(hosttree, ramqueue_host)	hosttree;
	RB_HEAD(msgtree, ramqueue_message)	msgtree;
	TAILQ_HEAD(,ramqueue_envelope)		queue;
};

RB_PROTOTYPE(hosttree, ramqueue_host, hosttree_entry, ramqueue_host_cmp);
RB_PROTOTYPE(msgtree,  ramqueue_message, msg_entry, ramqueue_msg_cmp);
RB_PROTOTYPE(evptree,  ramqueue_envelope, evp_entry, ramqueue_evp_cmp);

enum ramqueue_iter_type {
	RAMQUEUE_ITER_HOST,
	RAMQUEUE_ITER_BATCH,
	RAMQUEUE_ITER_MESSAGE,
	RAMQUEUE_ITER_QUEUE
};

struct ramqueue_iter {
	enum ramqueue_iter_type		type;
	union {
		struct ramqueue_host		*host;
		struct ramqueue_batch		*batch;
		struct ramqueue_message		*message;
	} u;
};


static int ramqueue_host_cmp(struct ramqueue_host *, struct ramqueue_host *);
static int ramqueue_msg_cmp(struct ramqueue_message *, struct ramqueue_message *);
static int ramqueue_evp_cmp(struct ramqueue_envelope *, struct ramqueue_envelope *);
static struct ramqueue_host *ramqueue_lookup_host(char *);
static struct ramqueue_host *ramqueue_insert_host(char *);
static void ramqueue_remove_host(struct ramqueue_host *);
static struct ramqueue_batch *ramqueue_lookup_batch(struct ramqueue_host *,
    u_int32_t);
static struct ramqueue_batch *ramqueue_insert_batch(struct ramqueue_host *,
    u_int32_t);
static void ramqueue_remove_batch(struct ramqueue_host *, struct ramqueue_batch *);
static struct ramqueue_message *ramqueue_lookup_message(u_int32_t);
static struct ramqueue_message *ramqueue_insert_message(u_int32_t);
static void ramqueue_remove_message(struct ramqueue_message *);

static struct ramqueue_envelope *ramqueue_lookup_envelope(u_int64_t);


/*NEEDSFIX*/
static int ramqueue_expire(struct envelope *, time_t);
static time_t ramqueue_next_schedule(struct envelope *, time_t);

static void  scheduler_ramqueue_init(void);
static int   scheduler_ramqueue_setup(time_t, time_t);
static int   scheduler_ramqueue_next(u_int64_t *, time_t *);
static void  scheduler_ramqueue_insert(struct envelope *);
static void  scheduler_ramqueue_remove(u_int64_t);
static void *scheduler_ramqueue_host(char *);
static void *scheduler_ramqueue_message(u_int32_t);
static void *scheduler_ramqueue_batch(u_int64_t);
static void *scheduler_ramqueue_queue(void);
static void  scheduler_ramqueue_close(void *);
static int   scheduler_ramqueue_fetch(void *, u_int64_t *);
static int   scheduler_ramqueue_schedule(u_int64_t);
static void  scheduler_ramqueue_display(void);

struct scheduler_backend scheduler_backend_ramqueue = {
	scheduler_ramqueue_init,
	scheduler_ramqueue_setup,
	scheduler_ramqueue_next,
	scheduler_ramqueue_insert,
	scheduler_ramqueue_remove,
	scheduler_ramqueue_host,
	scheduler_ramqueue_message,
	scheduler_ramqueue_batch,
	scheduler_ramqueue_queue,
	scheduler_ramqueue_close,
	scheduler_ramqueue_fetch,
	scheduler_ramqueue_schedule,
	scheduler_ramqueue_display
};
static struct ramqueue	ramqueue;

static void
scheduler_ramqueue_display_hosttree(void)
{
	struct ramqueue_host		*rq_host;
	struct ramqueue_batch		*rq_batch;
	struct ramqueue_envelope	*rq_evp;

	log_debug("\tscheduler_ramqueue: hosttree display");
	RB_FOREACH(rq_host, hosttree, &ramqueue.hosttree) {
		log_debug("\t\thost: [%p] %s", rq_host, rq_host->hostname);
		TAILQ_FOREACH(rq_batch, &rq_host->batch_queue, batch_entry) {
			log_debug("\t\t\tbatch: [%p] %016x",
			    rq_batch, rq_batch->msgid);
			TAILQ_FOREACH(rq_evp, &rq_batch->envelope_queue,
			    batchqueue_entry) {
				log_debug("\t\t\t\tevpid: [%p] %016"PRIx64,
				    rq_evp, rq_evp->evpid);
			}
		}
	}
}

static void
scheduler_ramqueue_display_msgtree(void)
{
	struct ramqueue_message		*rq_msg;
	struct ramqueue_envelope	*rq_evp;

	log_debug("\tscheduler_ramqueue: msgtree display");
	RB_FOREACH(rq_msg, msgtree, &ramqueue.msgtree) {
		log_debug("\t\tmsg: [%p] %016x", rq_msg, rq_msg->msgid);
		RB_FOREACH(rq_evp, evptree, &rq_msg->evptree) {
			log_debug("\t\t\tevp: [%p] %016"PRIx64,
			    rq_evp, rq_evp->evpid);
		}
	}
}

static void
scheduler_ramqueue_display_queue(void)
{
	struct ramqueue_envelope *rq_evp;

	log_debug("\tscheduler_ramqueue: queue display");
	TAILQ_FOREACH(rq_evp, &ramqueue.queue, queue_entry) {
		log_debug("\t\tevpid: [%p] [batch: %p], %016"PRIx64,
		    rq_evp, rq_evp->rq_batch, rq_evp->evpid);
	}
}

static void
scheduler_ramqueue_display(void)
{
	log_debug("scheduler_ramqueue: display");
	scheduler_ramqueue_display_hosttree();
	scheduler_ramqueue_display_msgtree();
	scheduler_ramqueue_display_queue();
}

static void
scheduler_ramqueue_init(void)
{
	log_debug("scheduler_ramqueue: init");
	bzero(&ramqueue, sizeof (ramqueue));
	TAILQ_INIT(&ramqueue.queue);
	RB_INIT(&ramqueue.hosttree);
	RB_INIT(&ramqueue.msgtree);
}

static int
scheduler_ramqueue_setup(time_t curtm, time_t nsched)
{
	struct envelope		envelope;
	static struct qwalk    *q = NULL;
	u_int64_t	evpid;
	time_t		sched;

	log_debug("scheduler_ramqueue: load");

	log_info("scheduler_ramqueue: queue loading in progress");
	if (q == NULL)
		q = qwalk_new(Q_QUEUE, 0);

	while (qwalk(q, &evpid)) {
		if (! queue_envelope_load(Q_QUEUE, evpid, &envelope)) {
			log_debug("scheduler_ramqueue: evp -> /corrupt");
			queue_message_corrupt(Q_QUEUE, evpid_to_msgid(evpid));
			continue;
		}
		if (ramqueue_expire(&envelope, curtm))
			continue;
		scheduler_ramqueue_insert(&envelope);
		
		if (! scheduler_ramqueue_next(&evpid, &sched))
			continue;

		if (sched <= nsched)
			nsched = sched;

		if (nsched <= curtm) {
			log_debug("ramqueue: loading interrupted");
			return (0);
		}
	}
	qwalk_close(q);
	q = NULL;
	log_debug("ramqueue: loading over");
	return (1);
}

static int
scheduler_ramqueue_next(u_int64_t *evpid, time_t *sched)
{
	struct ramqueue_envelope *rq_evp = NULL;

	log_debug("scheduler_ramqueue: next");
	TAILQ_FOREACH(rq_evp, &ramqueue.queue, queue_entry) {
		if (rq_evp->rq_batch->type == D_MDA)
			if (env->sc_opts & SMTPD_MDA_PAUSED)
				continue;
		if (rq_evp->rq_batch->type == D_MTA)
			if (env->sc_opts & SMTPD_MTA_PAUSED)
				continue;
		if (evpid)
			*evpid = rq_evp->evpid;
		if (sched)
			*sched = rq_evp->sched;
		log_debug("scheduler_ramqueue: next: found");
		return 1;
	}

	log_debug("scheduler_ramqueue: next: nothing schedulable");
	return 0;
}

static void
scheduler_ramqueue_insert(struct envelope *envelope)
{
	struct ramqueue_host *rq_host;
	struct ramqueue_message *rq_msg;
	struct ramqueue_batch *rq_batch;
	struct ramqueue_envelope *rq_evp, *evp;
	u_int32_t msgid;
	time_t curtm = time(NULL);

	log_debug("scheduler_ramqueue: insert");
	msgid = evpid_to_msgid(envelope->id);
	rq_msg = ramqueue_lookup_message(msgid);
	if (rq_msg == NULL)
		rq_msg = ramqueue_insert_message(msgid);

	rq_host = ramqueue_lookup_host(envelope->dest.domain);
	if (rq_host == NULL)
		rq_host = ramqueue_insert_host(envelope->dest.domain);

	rq_batch = ramqueue_lookup_batch(rq_host, msgid);
	if (rq_batch == NULL)
		rq_batch = ramqueue_insert_batch(rq_host, msgid);
		
	rq_evp = calloc(1, sizeof (*rq_evp));
	if (rq_evp == NULL)
		fatal("calloc");
	rq_evp->evpid = envelope->id;
	rq_evp->sched = ramqueue_next_schedule(envelope, curtm);
	rq_evp->rq_host = rq_host;
	rq_evp->rq_batch = rq_batch;
	rq_evp->rq_msg = rq_msg;

	RB_INSERT(evptree, &rq_msg->evptree, rq_evp);
	TAILQ_INSERT_TAIL(&rq_batch->envelope_queue, rq_evp,
	    batchqueue_entry);

	/* sorted insert */
	TAILQ_FOREACH(evp, &ramqueue.queue, queue_entry) {
		if (evp->sched >= rq_evp->sched) {
			TAILQ_INSERT_BEFORE(evp, rq_evp, queue_entry);
			break;
		}
	}
	if (evp == NULL)
		TAILQ_INSERT_TAIL(&ramqueue.queue, rq_evp, queue_entry);

	stat_increment(STATS_RAMQUEUE_ENVELOPE);
}

static void
scheduler_ramqueue_remove(u_int64_t evpid)
{
	struct ramqueue_batch *rq_batch;
	struct ramqueue_message *rq_msg;
	struct ramqueue_envelope *rq_evp;
	struct ramqueue_host *rq_host;

	log_debug("scheduler_ramqueue: remove");
	rq_evp = ramqueue_lookup_envelope(evpid);
	rq_msg = rq_evp->rq_msg;
	rq_batch = rq_evp->rq_batch;
	rq_host = rq_evp->rq_host;

	RB_REMOVE(evptree, &rq_msg->evptree, rq_evp);
	TAILQ_REMOVE(&rq_batch->envelope_queue, rq_evp, batchqueue_entry);
	TAILQ_REMOVE(&ramqueue.queue, rq_evp, queue_entry);
	stat_decrement(STATS_RAMQUEUE_ENVELOPE);

	/* check if we are the last of a batch */
	if (TAILQ_FIRST(&rq_batch->envelope_queue) == NULL)
		ramqueue_remove_batch(rq_host, rq_batch);

	/* check if we are the last of a message */
	if (RB_ROOT(&rq_msg->evptree) == NULL)
		ramqueue_remove_message(rq_msg);

	/* check if we are the last of a host */
	if (TAILQ_FIRST(&rq_host->batch_queue) == NULL)
		ramqueue_remove_host(rq_host);

	free(rq_evp);
}

static void *
scheduler_ramqueue_host(char *host)
{
	struct ramqueue_iter *iter;
	struct ramqueue_host *rq_host;

	rq_host = ramqueue_lookup_host(host);
	if (rq_host == NULL)
		return NULL;

	iter = calloc(1, sizeof *iter);
	if (iter == NULL)
		err(1, "calloc");

	iter->type = RAMQUEUE_ITER_HOST;
	iter->u.host = rq_host;

	return iter;
}

static void *
scheduler_ramqueue_batch(u_int64_t evpid)
{
	struct ramqueue_iter *iter;
	struct ramqueue_envelope *rq_evp;

	rq_evp = ramqueue_lookup_envelope(evpid);
	if (rq_evp == NULL)
		return NULL;

	iter = calloc(1, sizeof *iter);
	if (iter == NULL)
		err(1, "calloc");

	iter->type = RAMQUEUE_ITER_BATCH;
	iter->u.batch = rq_evp->rq_batch;

	return iter;
}

static void *
scheduler_ramqueue_message(u_int32_t msgid)
{
	struct ramqueue_iter *iter;
	struct ramqueue_message *rq_msg;

	rq_msg = ramqueue_lookup_message(msgid);
	if (rq_msg == NULL)
		return NULL;

	iter = calloc(1, sizeof *iter);
	if (iter == NULL)
		err(1, "calloc");

	iter->type = RAMQUEUE_ITER_MESSAGE;
	iter->u.message = rq_msg;

	return iter;
}

static void *
scheduler_ramqueue_queue(void)
{
	struct ramqueue_iter *iter;

	iter = calloc(1, sizeof *iter);
	if (iter == NULL)
		err(1, "calloc");

	iter->type = RAMQUEUE_ITER_QUEUE;

	return iter;
}

static void
scheduler_ramqueue_close(void *hdl)
{
	free(hdl);
}

int
scheduler_ramqueue_fetch(void *hdl, u_int64_t *evpid)
{
	struct ramqueue_iter		*iter = hdl;
	struct ramqueue_envelope	*rq_evp;
	struct ramqueue_batch		*rq_batch;

	switch (iter->type) {
	case RAMQUEUE_ITER_HOST: {
		rq_batch = TAILQ_FIRST(&iter->u.host->batch_queue);
		if (rq_batch == NULL)
			break;
		rq_evp = TAILQ_FIRST(&rq_batch->envelope_queue);
		if (rq_evp == NULL)
			break;
		*evpid = rq_evp->evpid;
		return 1;
	}

	case RAMQUEUE_ITER_BATCH:
		rq_evp = TAILQ_FIRST(&iter->u.batch->envelope_queue);
		if (rq_evp == NULL)
			break;
		*evpid = rq_evp->evpid;
		return 1;

	case RAMQUEUE_ITER_MESSAGE:
		rq_evp = RB_ROOT(&iter->u.message->evptree);
		if (rq_evp == NULL)
			break;
		*evpid = rq_evp->evpid;
		return 1;

	case RAMQUEUE_ITER_QUEUE:
		rq_evp = TAILQ_FIRST(&ramqueue.queue);
		if (rq_evp == NULL)
			break;
		*evpid = rq_evp->evpid;
		return 1;
	}

	return 0;
}

static int
scheduler_ramqueue_schedule(u_int64_t id)
{
	struct ramqueue_envelope	*rq_evp;
	struct ramqueue_message		*rq_msg;
	int	ret;

	/* schedule *all* */
	if (id == 0) {
		ret = 0;
		TAILQ_FOREACH(rq_evp, &ramqueue.queue, queue_entry) {
			rq_evp->sched = 0;
			ret++;
		}
		return ret;
	}

	/* scheduling by evpid */
	if (id > 0xffffffffL) {
		rq_evp = ramqueue_lookup_envelope(id);
		if (rq_evp == NULL)
			return 0;

		rq_evp->sched = 0;
		TAILQ_REMOVE(&ramqueue.queue, rq_evp, queue_entry);
		TAILQ_INSERT_HEAD(&ramqueue.queue, rq_evp, queue_entry);
		return 1;
	}

	rq_msg = ramqueue_lookup_message(id);
	if (rq_msg == NULL)
		return 0;

	/* scheduling by msgid */
	ret = 0;
	RB_FOREACH(rq_evp, evptree, &rq_msg->evptree) {
		rq_evp->sched = 0;
		TAILQ_REMOVE(&ramqueue.queue, rq_evp, queue_entry);
		TAILQ_INSERT_HEAD(&ramqueue.queue, rq_evp, queue_entry);
		ret++;
	}
	return ret;
}

static struct ramqueue_host *
ramqueue_lookup_host(char *host)
{
	struct ramqueue_host hostkey;

	strlcpy(hostkey.hostname, host, sizeof(hostkey.hostname));
	return RB_FIND(hosttree, &ramqueue.hosttree, &hostkey);
}

static struct ramqueue_message *
ramqueue_lookup_message(u_int32_t msgid)
{
	struct ramqueue_message msgkey;

	msgkey.msgid = msgid;
	return RB_FIND(msgtree, &ramqueue.msgtree, &msgkey);
}

static struct ramqueue_envelope *
ramqueue_lookup_envelope(u_int64_t evpid)
{
	struct ramqueue_message *rq_msg;
	struct ramqueue_envelope evpkey;

	rq_msg = ramqueue_lookup_message(evpid_to_msgid(evpid));
	if (rq_msg == NULL)
		return NULL;

	evpkey.evpid = evpid;
	return RB_FIND(evptree, &rq_msg->evptree, &evpkey);
}

static struct ramqueue_batch *
ramqueue_lookup_batch(struct ramqueue_host *rq_host, u_int32_t msgid)
{
	struct ramqueue_batch *rq_batch;

	TAILQ_FOREACH(rq_batch, &rq_host->batch_queue, batch_entry) {
		if (rq_batch->msgid == msgid)
			return rq_batch;
	}

	return NULL;
}

static int
ramqueue_expire(struct envelope *envelope, time_t curtm)
{
	struct envelope bounce;

	if (curtm - envelope->creation >= envelope->expire) {
		envelope_set_errormsg(envelope,
		    "message expired after sitting in queue for %d days",
		    envelope->expire / 60 / 60 / 24);
		bounce_record_message(envelope, &bounce);
		scheduler_ramqueue_insert(&bounce);
		log_debug("#### %s: queue_envelope_delete: %016" PRIx64,
		    __func__, envelope->id);
		queue_envelope_delete(Q_QUEUE, envelope);
		return 1;
	}
	return 0;
}

static time_t
ramqueue_next_schedule(struct envelope *envelope, time_t curtm)
{
	time_t delay;

	if (envelope->lasttry == 0)
		return curtm;

	delay = SMTPD_QUEUE_MAXINTERVAL;
	
	if (envelope->type == D_MDA ||
	    envelope->type == D_BOUNCE) {
		if (envelope->retry < 5)
			return curtm;
			
		if (envelope->retry < 15)
			delay = (envelope->retry * 60) + arc4random_uniform(60);
	}

	if (envelope->type == D_MTA) {
		if (envelope->retry < 3)
			delay = SMTPD_QUEUE_INTERVAL;
		else if (envelope->retry <= 7) {
			delay = SMTPD_QUEUE_INTERVAL * (1 << (envelope->retry - 3));
			if (delay > SMTPD_QUEUE_MAXINTERVAL)
				delay = SMTPD_QUEUE_MAXINTERVAL;
		}
	}

	if (curtm >= envelope->lasttry + delay)
		return curtm;

	return curtm + delay;
}

static struct ramqueue_message *
ramqueue_insert_message(u_int32_t msgid)
{
	struct ramqueue_message *rq_msg;

	rq_msg = calloc(1, sizeof (*rq_msg));
	if (rq_msg == NULL)
		fatal("calloc");
	rq_msg->msgid = msgid;
	RB_INSERT(msgtree, &ramqueue.msgtree, rq_msg);
	RB_INIT(&rq_msg->evptree);
	stat_increment(STATS_RAMQUEUE_MESSAGE);

	return rq_msg;
}

static struct ramqueue_host *
ramqueue_insert_host(char *host)
{
	struct ramqueue_host *rq_host;

	rq_host = calloc(1, sizeof (*rq_host));
	if (rq_host == NULL)
		fatal("calloc");
	rq_host->h_id = generate_uid();
	strlcpy(rq_host->hostname, host, sizeof(rq_host->hostname));
	TAILQ_INIT(&rq_host->batch_queue);
	RB_INSERT(hosttree, &ramqueue.hosttree, rq_host);
	stat_increment(STATS_RAMQUEUE_HOST);

	return rq_host;
}

static struct ramqueue_batch *
ramqueue_insert_batch(struct ramqueue_host *rq_host, u_int32_t msgid)
{
	struct ramqueue_batch *rq_batch;

	rq_batch = calloc(1, sizeof (*rq_batch));
	if (rq_batch == NULL)
		fatal("calloc");
	rq_batch->b_id = generate_uid();
	rq_batch->msgid = msgid;

	TAILQ_INIT(&rq_batch->envelope_queue);
	TAILQ_INSERT_TAIL(&rq_host->batch_queue, rq_batch, batch_entry);

	stat_increment(STATS_RAMQUEUE_BATCH);

	return rq_batch;
}

static void
ramqueue_remove_host(struct ramqueue_host *rq_host)
{
	RB_REMOVE(hosttree, &ramqueue.hosttree, rq_host);
	free(rq_host);
	stat_decrement(STATS_RAMQUEUE_HOST);
}

static void
ramqueue_remove_message(struct ramqueue_message *rq_msg)
{
	RB_REMOVE(msgtree, &ramqueue.msgtree, rq_msg);
	free(rq_msg);
	stat_decrement(STATS_RAMQUEUE_MESSAGE);
}


static void
ramqueue_remove_batch(struct ramqueue_host *rq_host,
    struct ramqueue_batch *rq_batch)
{
	TAILQ_REMOVE(&rq_host->batch_queue, rq_batch, batch_entry);
	free(rq_batch);
	stat_decrement(STATS_RAMQUEUE_BATCH);
}

static int
ramqueue_host_cmp(struct ramqueue_host *h1, struct ramqueue_host *h2)
{
	return strcmp(h1->hostname, h2->hostname);
}


static int
ramqueue_msg_cmp(struct ramqueue_message *m1, struct ramqueue_message *m2)
{
	return (m1->msgid < m2->msgid ? -1 : m1->msgid > m2->msgid);
}

static int
ramqueue_evp_cmp(struct ramqueue_envelope *e1, struct ramqueue_envelope *e2)
{
	return (e1->evpid < e2->evpid ? -1 : e1->evpid > e2->evpid);
}

RB_GENERATE(hosttree, ramqueue_host, hosttree_entry, ramqueue_host_cmp);
RB_GENERATE(msgtree, ramqueue_message, msgtree_entry, ramqueue_msg_cmp);
RB_GENERATE(evptree, ramqueue_envelope, evptree_entry, ramqueue_evp_cmp);
