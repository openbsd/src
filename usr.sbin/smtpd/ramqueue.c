/*	$OpenBSD: ramqueue.c,v 1.3 2011/04/14 22:36:09 gilles Exp $	*/

/*
 * Copyright (c) 2011 Gilles Chehade <gilles@openbsd.org>
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
#include <sys/stat.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <event.h>
#include <fcntl.h>
#include <imsg.h>
#include <libgen.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "smtpd.h"
#include "log.h"

int	ramqueue_expire(struct smtpd *, struct message *, time_t);
void	ramqueue_insert(struct ramqueue *, struct message *, time_t);
time_t	ramqueue_next_schedule(struct message *, time_t);
int	ramqueue_host_cmp(struct ramqueue_host *, struct ramqueue_host *);
struct ramqueue_host *ramqueue_get_host(struct ramqueue *, char *);
struct ramqueue_batch *ramqueue_get_batch(struct ramqueue *,
    struct ramqueue_host *, struct message *);
void	ramqueue_put_host(struct ramqueue *, struct ramqueue_host *);
void	ramqueue_put_batch(struct ramqueue *, struct ramqueue_batch *);
int	ramqueue_load_offline(struct ramqueue *);


void
ramqueue_init(struct smtpd *env, struct ramqueue *rqueue)
{
	bzero(rqueue, sizeof (*rqueue));
	TAILQ_INIT(&rqueue->queue);
	rqueue->env = env;
	rqueue->current_evp = NULL;
}

int
ramqueue_is_empty(struct ramqueue *rqueue)
{
	return TAILQ_FIRST(&rqueue->queue) == NULL;
}

int
ramqueue_batch_is_empty(struct ramqueue_batch *rq_batch)
{
	return TAILQ_FIRST(&rq_batch->envelope_queue) == NULL;
}

int
ramqueue_host_is_empty(struct ramqueue_host *rq_host)
{
	return TAILQ_FIRST(&rq_host->batch_queue) == NULL;
}

struct ramqueue_envelope *
ramqueue_first_envelope(struct ramqueue *rqueue)
{
	return TAILQ_FIRST(&rqueue->queue);
}

struct ramqueue_envelope *
ramqueue_next_envelope(struct ramqueue *rqueue)
{
	if (rqueue->current_evp == NULL)
		rqueue->current_evp = TAILQ_FIRST(&rqueue->queue);
	else
		rqueue->current_evp = TAILQ_NEXT(rqueue->current_evp, queue_entry);
	return rqueue->current_evp;
}

struct ramqueue_envelope *
ramqueue_batch_first_envelope(struct ramqueue_batch *rq_batch)
{
	return TAILQ_FIRST(&rq_batch->envelope_queue);
}

int
ramqueue_load_offline(struct ramqueue *rqueue)
{
	char		 path[MAXPATHLEN];
	struct smtpd	*env = rqueue->env;
	static struct qwalk    *q = NULL;

	log_debug("ramqueue: offline queue loading in progress");
	if (q == NULL)
		q = qwalk_new(PATH_OFFLINE);
	while (qwalk(q, path)) {
		imsg_compose_event(env->sc_ievs[PROC_QUEUE],
		    IMSG_PARENT_ENQUEUE_OFFLINE, PROC_PARENT, 0, -1, path,
		    strlen(path) + 1);
		log_debug("ramqueue: offline queue loading interrupted");
		return 0;
	}
	qwalk_close(q);
	q = NULL;
	log_debug("ramqueue: offline queue loading over");
	return 1;
}

int
ramqueue_load(struct ramqueue *rqueue, time_t *nsched)
{
	char			path[MAXPATHLEN];
	time_t			curtm;
	struct message		envelope;
	static struct qwalk    *q = NULL;
	struct ramqueue_envelope *rq_evp;



	log_debug("ramqueue: queue loading in progress");

	if (q == NULL)
		q = qwalk_new(PATH_QUEUE);
	while (qwalk(q, path)) {
		curtm = time(NULL);

		if (! queue_envelope_load(rqueue->env, Q_QUEUE,
			basename(path), &envelope))
			continue;
		if (ramqueue_expire(rqueue->env, &envelope, curtm))
			continue;
		ramqueue_insert(rqueue, &envelope, curtm);

		rq_evp = TAILQ_FIRST(&rqueue->queue);
		*nsched = rq_evp->sched;

		if (rq_evp->sched <= *nsched) {
			log_debug("ramqueue: loading interrupted");
			return (0);
		}
	}
	qwalk_close(q);
	q = NULL;
	log_debug("ramqueue: loading over");
	return (1);
}

void
ramqueue_insert(struct ramqueue *rqueue, struct message *envelope, time_t curtm)
{
	struct ramqueue_envelope *rq_evp;
	struct ramqueue_envelope *evp;

	rq_evp = calloc(1, sizeof (*rq_evp));
	if (rq_evp == NULL)
		fatal("calloc");
	strlcpy(rq_evp->id, envelope->message_uid, sizeof(rq_evp->id));
	rq_evp->sched = ramqueue_next_schedule(envelope, curtm);
	rq_evp->host = ramqueue_get_host(rqueue, envelope->recipient.domain);
	rq_evp->batch = ramqueue_get_batch(rqueue, rq_evp->host, envelope);

	TAILQ_INSERT_TAIL(&rq_evp->batch->envelope_queue, rq_evp,
	    batchqueue_entry);

	/* sorted insert */
	TAILQ_FOREACH(evp, &rqueue->queue, queue_entry) {
		if (evp->sched >= rq_evp->sched) {
			TAILQ_INSERT_BEFORE(evp, rq_evp, queue_entry);
			break;
		}
	}
	if (evp == NULL)
		TAILQ_INSERT_TAIL(&rqueue->queue, rq_evp, queue_entry);

	rqueue->env->stats->ramqueue.envelopes++;
	SET_IF_GREATER(rqueue->env->stats->ramqueue.envelopes,
	    rqueue->env->stats->ramqueue.envelopes_max);
}

void
ramqueue_remove(struct ramqueue *rqueue, struct ramqueue_envelope *rq_evp)
{
	struct ramqueue_batch *rq_batch = rq_evp->batch;

	if (rq_evp == rqueue->current_evp)
		rqueue->current_evp = TAILQ_NEXT(rqueue->current_evp, queue_entry);

	TAILQ_REMOVE(&rq_batch->envelope_queue, rq_evp, batchqueue_entry);
	TAILQ_REMOVE(&rqueue->queue, rq_evp, queue_entry);
	rqueue->env->stats->ramqueue.envelopes--;
}

int
ramqueue_expire(struct smtpd *env, struct message *envelope, time_t curtm)
{
	struct message bounce;

	if (curtm - envelope->creation >= envelope->expire) {
		message_set_errormsg(envelope,
		    "message expired after sitting in queue for %d days",
		    envelope->expire / 60 / 60 / 24);
		bounce_record_message(env, envelope, &bounce);
		ramqueue_insert(&env->sc_rqueue, &bounce, time(NULL));
		queue_envelope_delete(env, Q_QUEUE, envelope);
		return 1;
	}
	return 0;
}

time_t
ramqueue_next_schedule(struct message *envelope, time_t curtm)
{
	time_t delay;

	if (envelope->lasttry == 0)
		return curtm;

	delay = SMTPD_QUEUE_MAXINTERVAL;

	if (envelope->type == T_MDA_MESSAGE ||
	    envelope->type == T_BOUNCE_MESSAGE) {
		if (envelope->retry < 5)
			return curtm;
			
		if (envelope->retry < 15)
			delay = (envelope->retry * 60) + arc4random_uniform(60);
	}

	if (envelope->type == T_MTA_MESSAGE) {
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

struct ramqueue_host *
ramqueue_get_host(struct ramqueue *rqueue, char *hostname)
{
	struct ramqueue_host *rq_host, key;

	strlcpy(key.hostname, hostname, sizeof(key.hostname));
	rq_host = RB_FIND(hosttree, &rqueue->hosttree, &key);
	if (rq_host == NULL) {
		rq_host = calloc(1, sizeof (*rq_host));
		if (rq_host == NULL)
			fatal("calloc");
		rq_host->h_id = generate_uid();
		strlcpy(rq_host->hostname, hostname, sizeof(rq_host->hostname));
		TAILQ_INIT(&rq_host->batch_queue);
		RB_INSERT(hosttree, &rqueue->hosttree, rq_host);
		rqueue->env->stats->ramqueue.hosts++;
		SET_IF_GREATER(rqueue->env->stats->ramqueue.hosts,
		    rqueue->env->stats->ramqueue.hosts_max);
	}

	return rq_host;
}

void
ramqueue_put_host(struct ramqueue *rqueue, struct ramqueue_host *host)
{
	TAILQ_INIT(&host->batch_queue);
	RB_INSERT(hosttree, &rqueue->hosttree, host);
}

struct ramqueue_batch *
ramqueue_get_batch(struct ramqueue *rqueue, struct ramqueue_host *host,
    struct message *envelope)
{
	struct ramqueue_batch *rq_batch;

	TAILQ_FOREACH(rq_batch, &host->batch_queue, batch_entry) {
		if (strcmp(rq_batch->m_id, envelope->message_id) == 0)
			return rq_batch;
	}

	rq_batch = calloc(1, sizeof (*rq_batch));
	if (rq_batch == NULL)
		fatal("calloc");
	rq_batch->b_id = generate_uid();
	rq_batch->type = envelope->type;
	rq_batch->rule = envelope->recipient.rule;
	strlcpy(rq_batch->m_id, envelope->message_id, sizeof(rq_batch->m_id));
	TAILQ_INIT(&rq_batch->envelope_queue);
	TAILQ_INSERT_TAIL(&host->batch_queue, rq_batch, batch_entry);

	rqueue->env->stats->ramqueue.batches++;
	SET_IF_GREATER(rqueue->env->stats->ramqueue.batches,
	    rqueue->env->stats->ramqueue.batches_max);
	return rq_batch;
}

void
ramqueue_put_batch(struct ramqueue *rqueue, struct ramqueue_batch *rq_batch)
{
	struct ramqueue_host *rq_host;

	TAILQ_INIT(&rq_batch->envelope_queue);
	RB_FOREACH(rq_host, hosttree, &rqueue->hosttree) {
		if (rq_host->h_id == rq_batch->h_id) {
			TAILQ_INSERT_TAIL(&rq_host->batch_queue, rq_batch,
			    batch_entry);
			return;
		}
	}
}

void
ramqueue_remove_batch(struct ramqueue_host *rq_host, struct ramqueue_batch *rq_batch)
{
	TAILQ_REMOVE(&rq_host->batch_queue, rq_batch, batch_entry);
}

void
ramqueue_remove_host(struct ramqueue *rqueue, struct ramqueue_host *rq_host)
{
	RB_REMOVE(hosttree, &rqueue->hosttree, rq_host);
}

int
ramqueue_host_cmp(struct ramqueue_host *h1, struct ramqueue_host *h2)
{
	return strcmp(h1->hostname, h2->hostname);
}

RB_GENERATE(hosttree, ramqueue_host, host_entry, ramqueue_host_cmp);
