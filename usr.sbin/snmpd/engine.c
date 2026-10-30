/*	$OpenBSD: engine.c,v 1.2 2026/10/30 16:40:40 martijn Exp $	*/

/*
 * Copyright (c) 2025 Martijn van Duren <martijn@openbsd.org>
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
#include <sys/time.h>

#include <stdint.h>
#include <string.h>

#include "snmp.h"
#include "snmpd.h"

static int engine_in_timewindow_local(struct engine *, int32_t, int32_t);
static int engine_in_timewindow_peer(struct engine *, int32_t, int32_t);
static void engine_update(struct engine *);

int
engine_islocal(struct engine *engine)
{
	return engineid_cmp(&engine->id, &snmpd_env->sc_engine.id) == 0;
}

int
engine_in_timewindow(struct engine *engine, int32_t boots, int32_t time)
{
	struct engine now = *engine;

	engine_update(&now);
	if (now.boots == INT32_MAX || boots == INT32_MAX)
		return 0;

	if (engine_islocal(engine))
		return engine_in_timewindow_local(&now, boots, time);
	else
		return engine_in_timewindow_peer(&now, boots, time);
}

/*
 * Only use current boots in local calculations; Rollovers happen every
 * ~68 years and are not supported.
 */
static int
engine_in_timewindow_local(struct engine *now, int32_t boots, int32_t time)
{
	int32_t min, max;

	if (now->boots != boots)
		return 0;
	min = now->time < SNMP_MAX_TIMEWINDOW ?
	    0 : now->time - SNMP_MAX_TIMEWINDOW;
	max = INT32_MAX - now->time < SNMP_MAX_TIMEWINDOW ?
	    INT32_MAX : now->time + SNMP_MAX_TIMEWINDOW;

	return time >= min && time <= max;
}

/*
 * We don't control remote systems, so once established allow for both rollover
 * and at max 1 restart per second. If a system gets a greater discrepency the
 * admin needs to fix this manually.
 */
static int
engine_in_timewindow_peer(struct engine *now, int32_t boots, int32_t time)
{
	struct engine min, max;
	int32_t maxboots, calcboots;

	if (now->time < SNMP_MAX_TIMEWINDOW) {
		if (now->boots == 1) {
			min.boots = 1;
			min.time = 0;
		} else {
			min.boots = now->boots - 1;
			min.time = INT32_MAX -
			    (SNMP_MAX_TIMEWINDOW - now->time);
		}
	} else {
		min.boots = now->boots;
		min.time = now->time - SNMP_MAX_TIMEWINDOW;
	}

	if (INT32_MAX - now->time < SNMP_MAX_TIMEWINDOW) {
		max.boots = now->boots + 1;
		max.time = SNMP_MAX_TIMEWINDOW - (INT32_MAX - now->time);
	} else {
		max.boots = now->boots;
		max.time = now->time + SNMP_MAX_TIMEWINDOW;
	}
	maxboots = INT32_MAX - 1 - max.boots < SNMP_MAX_TIMEWINDOW ?
	    INT32_MAX - 1 : max.boots + SNMP_MAX_TIMEWINDOW;
	calcboots = boots >= max.boots ? boots : max.boots;

	if (boots < min.boots || (boots == min.boots && time < min.time))
		return 0;

	if (boots > maxboots ||
	    ((boots >= max.boots && boots <= maxboots) &&
	    time > max.time - (calcboots - max.boots)))
		return 0;

	return 1;
}

/* Calculate the current time in a bounded manner */
static void
engine_update(struct engine *engine)
{
	struct timespec now, diff;
	int rollovers;

	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecsub(&now, &engine->measure, &diff);

	rollovers = diff.tv_sec	/ INT32_MAX;
	if (INT32_MAX - (diff.tv_sec % INT32_MAX) < engine->time) {
		rollovers++;
		engine->time = diff.tv_sec - (INT32_MAX - engine->time);
	} else
		engine->time += diff.tv_sec % INT32_MAX;

	if (INT32_MAX - engine->boots < rollovers) {
		engine->boots = INT32_MAX;
		engine->time = 0;
		return;
	} else
		engine->boots += rollovers;
	engine->measure = now;
}

int32_t
engine_boots(struct engine *engine)
{
	struct engine now = *engine;

	engine_update(&now);
	return now.boots;
}

int32_t
engine_time(struct engine *engine)
{
	struct engine now = *engine;

	engine_update(&now);
	return now.time;
}

int
engineid_cmp(struct engineid *e1, struct engineid *e2)
{
	size_t min;
	int r;

	min = e1->length < e2->length ? e1->length : e2->length;
	if ((r = memcmp(e1->value, e2->value, min)) != 0)
		return r;
	if (e1->length == e2->length)
		return 0;
	return e1->length < e2->length ? -1 : 1;
}
