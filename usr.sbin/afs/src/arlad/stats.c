/*
 * Copyright (c) 2001 - 2002 Kungliga Tekniska Högskolan
 * (Royal Institute of Technology, Stockholm, Sweden).
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 
 * 3. Neither the name of the Institute nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "arla_local.h"
RCSID("$arla: stats.c,v 1.10 2002/09/07 01:30:02 lha Exp $");

#define MIN_FETCH_BLOCKSIZE 65536

int fetch_block_size = MIN_FETCH_BLOCKSIZE;

#define HISTOGRAM_SLOTS 32
#define STATHASHSIZE 997

struct time_statistics {
    uint32_t measure_type;
    uint32_t host;
    uint32_t partition;
    uint32_t measure_items; /* normed by get_histgram_slots */
    uint32_t count[HISTOGRAM_SLOTS];    
    int64_t measure_items_total[HISTOGRAM_SLOTS];
    int64_t elapsed_time[HISTOGRAM_SLOTS];
};

static unsigned
statistics_hash (void *p)
{
    struct time_statistics *stats = (struct time_statistics*)p;

    return stats->measure_type + stats->host +
	stats->partition * 32 * 32 + stats->measure_items * 32;
}

/*
 * Compare two entries. Return 0 if and only if the same.
 */

static int
statistics_cmp (void *a, void *b)
{
    struct time_statistics *f1 = (struct time_statistics*)a;
    struct time_statistics *f2 = (struct time_statistics*)b;

    return f1->measure_type  != f2->measure_type
	|| f1->host          != f2->host
	|| f1->partition     != f2->partition
	|| f1->measure_items != f2->measure_items;
}

static Hashtab *statistics;

static int
get_histogram_slot(uint32_t value)
{
    int i;

    for (i = HISTOGRAM_SLOTS - 1; i > 0; i--) {
	if (value >> i)
	    return i;
    }
    return 0;
}

static void
add_time_statistics(uint32_t measure_type, uint32_t host,
		    uint32_t partition, uint32_t measure_items,
		    int64_t elapsed_time)
{
    uint32_t time_slot;
    struct time_statistics *ts;
    struct time_statistics *ts2;

    ts = malloc(sizeof(*ts));

    time_slot = get_histogram_slot(elapsed_time);
    ts->measure_type = measure_type;
    ts->measure_items = get_histogram_slot(measure_items);
    ts->host = host;
    ts->partition = partition;
    ts2 = hashtabsearch (statistics, (void*)(ts));
    if (ts2) {
	ts2->count[time_slot]++;
	ts2->elapsed_time[time_slot] += elapsed_time;
	ts2->measure_items_total[time_slot] += measure_items;
	free(ts);
    } else {
	memset(ts->count, 0, sizeof(ts->count));
	memset(ts->measure_items_total, 0, sizeof(ts->measure_items_total));
	memset(ts->elapsed_time, 0, sizeof(ts->elapsed_time));
	ts->count[time_slot]++;
	ts->elapsed_time[time_slot] += elapsed_time;
	ts->measure_items_total[time_slot] += measure_items;
	hashtabadd(statistics, ts);
    }
}

void
collectstats_init (void)
{
    statistics = hashtabnewf(STATHASHSIZE, 
			     statistics_cmp, statistics_hash, HASHTAB_GROW);

    if (statistics == NULL)
	arla_err(1, ADEBINIT, errno, "collectstats_init: cannot malloc");
}

void
collectstats_start (struct collect_stat *p)
{
    struct timeval starttime;

    gettimeofday(&starttime, NULL);
    p->starttime = starttime.tv_sec * 1000000LL + starttime.tv_usec;
}

void
collectstats_stop (struct collect_stat *p,
		   FCacheEntry *entry,
		   ConnCacheEntry *conn,
		   long partition,
		   int measure_type, int measure_items)
{
    struct timeval stoptime;
    int64_t elapsed_time;

    if (partition == -1)
	return;

    gettimeofday(&stoptime, NULL);

    elapsed_time = stoptime.tv_sec * 1000000LL + stoptime.tv_usec;
    elapsed_time -= p->starttime;
    add_time_statistics(measure_type, conn->host, partition,
			measure_items, elapsed_time);
}

struct hostpart {
    uint32_t host;
    uint32_t part;
};

static unsigned
hostpart_hash (void *p)
{
    struct hostpart *h = (struct hostpart*)p;

    return h->host * 256 + h->part;
}

static int
hostpart_cmp (void *a, void *b)
{
    struct hostpart *h1 = (struct hostpart*)a;
    struct hostpart *h2 = (struct hostpart*)b;

    return h1->host != h2->host ||
	h1->part != h2->part;
}

static Bool
hostpart_addhash (void *ptr, void *arg)
{
    Hashtab *hostparthash = (Hashtab *) arg;
    struct time_statistics *s = (struct time_statistics *) ptr;
    struct hostpart *h;
    
    h = malloc(sizeof(*h));
    h->host = s->host;
    h->part = s->partition;

    hashtabaddreplace(hostparthash, h);
    return FALSE;
}

struct hostpart_collect_args {
    uint32_t *host;
    uint32_t *part;
    int i;
    int max;
};

static Bool
hostpart_collect (void *ptr, void *arg)
{
    struct hostpart_collect_args *collect_args =
	(struct hostpart_collect_args *) arg;
    struct hostpart *h = (struct hostpart *) ptr;

    if (collect_args->i >= collect_args->max)
	return TRUE;

    collect_args->host[collect_args->i] = h->host;
    collect_args->part[collect_args->i] = h->part;
    ++collect_args->i;

    return FALSE;
}

int
collectstats_hostpart(uint32_t *host, uint32_t *part, int *n)
{
    Hashtab *hostparthash;
    struct hostpart_collect_args collect_args;

    hostparthash = hashtabnewf(101, hostpart_cmp, hostpart_hash, HASHTAB_GROW);

    hashtabforeach(statistics, hostpart_addhash, hostparthash);

    collect_args.host = host;
    collect_args.part = part;
    collect_args.i = 0;
    collect_args.max = *n;
    hashtabforeach(hostparthash, hostpart_collect, &collect_args);
    *n = collect_args.i;

    hashtabrelease(hostparthash);

    return 0;
}

int
collectstats_getentry(uint32_t host, uint32_t part, uint32_t type,
		      uint32_t items_slot, uint32_t *count,
		      int64_t *items_total, int64_t *total_time)
{
    struct time_statistics ts;
    struct time_statistics *ts2;

    ts.measure_type = type;
    ts.measure_items = items_slot;
    ts.host = host;
    ts.partition = part;
    ts2 = hashtabsearch (statistics, (void*)(&ts));
    if (ts2 == NULL) {
	memset(count, 0, 4 * HISTOGRAM_SLOTS);
	memset(items_total, 0, 8 * HISTOGRAM_SLOTS);
	memset(total_time, 0, 8 * HISTOGRAM_SLOTS);
    } else {
	memcpy(count, ts2->count, 4 * HISTOGRAM_SLOTS);
	memcpy(items_total, ts2->measure_items_total, 8 * HISTOGRAM_SLOTS);
	memcpy(total_time, ts2->elapsed_time, 8 * HISTOGRAM_SLOTS);
    }

    return 0;
}

void
stats_set_prefetch(size_t sz)
{
    if (sz < MIN_FETCH_BLOCKSIZE)
	sz = MIN_FETCH_BLOCKSIZE;
}

size_t
stats_prefetch(ConnCacheEntry *conn, uint32_t part)
{
    return fetch_block_size;
}

size_t
stats_fetch_round(ConnCacheEntry *conn, uint32_t part, size_t size)
{
    return fetch_block_size - (size % fetch_block_size) + size;
}
