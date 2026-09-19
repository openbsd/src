/*	$OpenBSD: sched.c,v 1.5 2026/09/19 16:14:17 deraadt Exp $ */
/*
 * Copyright (c) 2025-2026 Ralph Covelli <rcovelli@he.net>
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
#include <sys/tree.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <signal.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "rtr_config.h"
#include "rtrd.h"

struct timeval now;

void
rtr_gettime(void)
{
	if (clock_gettime(CLOCK_REALTIME, (struct timespec *)&now) != 0) {
		logx(0, "couldnt get the clock\n");
		exit(1);
	}
	now.tv_usec /= 1000; /* nsec -> usec */
}

static inline int
schedcmp(struct sched *a, struct sched *b)
{
	if (a->event_time > b->event_time)
		return 1;
	if (a->event_time < b->event_time)
		return -1;

	if (a->type > b->type)
		return 1;
	if (a->type < b->type)
		return -1;

	if (a->rtr_socket > b->rtr_socket)
		return 1;
	if (a->rtr_socket < b->rtr_socket)
		return -1;

	return 0;
}

RB_GENERATE(sched_tree, sched, entry, schedcmp)

struct sched_tree scheduler;

/* 1 if error */
int
insert_sched(struct sched_tree *schedtree, struct sched *sched)
{
	struct sched *node;

	assert(schedtree);
	assert(sched);

	node = malloc(sizeof(struct sched));
	if (node == NULL)
		err(1, NULL);

	node->event_time = sched->event_time;
	node->type = sched->type;
	node->rtr_socket = sched->rtr_socket;
	if (RB_INSERT(sched_tree, schedtree, node) != NULL) {
		free(node);
		return 1;
	}

	return 0;
}

/* 1 if error */
int
remove_sched(struct sched_tree *schedtree, struct sched *sched)
{
	struct sched query, *node;

	assert(schedtree);
	assert(sched);

	query.event_time = sched->event_time;
	query.type = sched->type;
	query.rtr_socket = sched->rtr_socket;

	node = RB_FIND(sched_tree, schedtree, &query);
	if (!node)
		return 1;

	RB_REMOVE(sched_tree, schedtree, node);
	free(node);

	return 0;
}

void
remove_sched_type(struct sched_tree *schedtree, uint32_t type)
{
	struct sched *sched, *tmpsched;

	assert(schedtree);

	RB_FOREACH_SAFE(sched, sched_tree, &scheduler, tmpsched) {
		if (sched->type == type) {
			RB_REMOVE(sched_tree, &scheduler, sched);
			free(sched);
		}
	}
}

void
remove_sched_socket(struct sched_tree *schedtree, struct rtr_socket *rtr_socket)
{
	struct sched *sched, *tmpsched;

	assert(schedtree);
	assert(rtr_socket);

	RB_FOREACH_SAFE(sched, sched_tree, &scheduler, tmpsched) {
		if (sched->rtr_socket == rtr_socket) {
			RB_REMOVE(sched_tree, &scheduler, sched);
			free(sched);
		}
	}
}

void
remove_sched_type_and_socket(struct sched_tree *schedtree,
    uint32_t type, struct rtr_socket *rtr_socket)
{
	struct sched *sched, *tmpsched;

	assert(schedtree);
	assert(rtr_socket);

	RB_FOREACH_SAFE(sched, sched_tree, &scheduler, tmpsched) {
		if (sched->type == type &&
		    sched->rtr_socket == rtr_socket) {
			RB_REMOVE(sched_tree, &scheduler, sched);
			free(sched);
		}
	}
}

void
free_sched_tree(struct sched_tree *schedtree)
{
	struct sched *sched, *tmpsched;

	if (!schedtree)
		return;
	RB_FOREACH_SAFE(sched, sched_tree, schedtree, tmpsched) {
		RB_REMOVE(sched_tree, schedtree, sched);
		free(sched);
	}
}

void
sched_init(void)
{
	RB_INIT(&scheduler);
}

int
time_until_next_event(struct timeval present)
{
	struct sched *sched;
	struct timeval delay, tv_event;

	/* signal is waiting, dont wait */
	if (sigflags != 0)
		return 0;

	sched = RB_MIN(sched_tree, &scheduler);
	/* nothing scheduled, wait forever */
	if (sched == NULL)
		return -1;

	tv_event.tv_sec = sched->event_time;
	tv_event.tv_usec = 0;
	/* this event is not in the future, no delay */
	if (!timercmp(&tv_event, &present, >))
		return 0;

	timersub(&tv_event, &present, &delay);

	/* milliseconds */
	return (delay.tv_sec * 1000) + (delay.tv_usec / 1000);
}

int
schedule_cleanup(time_t present)
{
	struct sched sched;

	sched.event_time = present + CLEANUP_INTERVAL;
	sched.type = SCHED_TYPE_CLEANUP;
	sched.rtr_socket = NULL;
	return insert_sched(&scheduler, &sched);
}

int
schedule_handshake_timeout(time_t present, struct rtr_socket *s)
{
	struct sched sched;

	sched.event_time = present + HANDSHAKE_TIMEOUT;
	sched.type = SCHED_TYPE_HANDSHAKE_TIMEOUT;
	sched.rtr_socket = s;
	return insert_sched(&scheduler, &sched);
}

int
schedule_idle_timeout(time_t present, struct rtr_socket *s)
{
	struct sched sched;

	sched.event_time = present + IDLE_TIMEOUT;
	sched.type = SCHED_TYPE_IDLE_TIMEOUT;
	sched.rtr_socket = s;
	return insert_sched(&scheduler, &sched);
}

void
process_cleanup(time_t present)
{
	struct vrp4_tree vrp4s;
	struct vrp6_tree vrp6s;
	struct brk_tree brks;
	struct vap_tree vaps;
	struct cache_frame *cf;

	RB_INIT(&vrp4s);
	RB_INIT(&vrp6s);
	RB_INIT(&brks);
	RB_INIT(&vaps);

	logx(1, "Processing expirations\n");

	cf = get_latest_cache_frame(RTR_VERSION_2);

	if (cf) {
		vrp4_treecpy_expire(&cf->rtr_vrp4s->vrp4s, &vrp4s, present);
		vrp6_treecpy_expire(&cf->rtr_vrp6s->vrp6s, &vrp6s, present);
		brk_treecpy_expire(&cf->rtr_brks->brks, &brks, present);
		vap_treecpy_expire(&cf->rtr_vaps->vaps, &vaps, present);

		update_cache(&vrp4s, &vrp6s, &brks, &vaps);

		free_vrp4_tree(&vrp4s);
		free_vrp6_tree(&vrp6s);
		free_brk_tree(&brks);
		free_vap_tree(&vaps);
	}
	schedule_cleanup(present);
}

void
process_handshake_timeout(struct rtr_socket *s)
{
	if (s == NULL)
		return;

	if (s->name != NULL)
		logx(0, "Handshake timeout from %s\n", s->name);
	else
		logx(0, "Handshake timeout\n");

	SetSocketFlag(s, RTR_SOCKET_FLAG_CLOSED);
}

void
process_idle_timeout(struct rtr_socket *s)
{
	if (s == NULL)
		return;

	if (s->version >= RTR_VERSION_2) {
		senderrorto_one(s, TRANSPORT_ERROR, NULL, "Idle timeout");
	} else {
		if (s->name != NULL)
			logx(0, "Idle timeout from %s\n", s->name);
		else
			logx(0, "Idle timeout\n");
	}

	rtr_flush_write(s);

	SetSocketFlag(s, RTR_SOCKET_FLAG_CLOSED);
}

void
process_scheduler_events(time_t present)
{
	struct sched *sched, *tmpsched;

	RB_FOREACH_SAFE(sched, sched_tree, &scheduler, tmpsched) {
		if (present < sched->event_time)
			break;

		switch (sched->type) {
		case SCHED_TYPE_CLEANUP:
			process_cleanup(present);
			break;
		case SCHED_TYPE_HANDSHAKE_TIMEOUT:
			process_handshake_timeout(sched->rtr_socket);
			break;
		case SCHED_TYPE_IDLE_TIMEOUT:
			process_idle_timeout(sched->rtr_socket);
			break;
		}

		RB_REMOVE(sched_tree, &scheduler, sched);
		free(sched);
	}
}
