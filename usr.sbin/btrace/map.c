/*	$OpenBSD: map.c,v 1.11 2020/09/18 19:19:38 jasper Exp $ */

/*
 * Copyright (c) 2020 Martin Pieuchot <mpi@openbsd.org>
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

/*
 * Associative array implemented with RB-Tree.
 */

#include <sys/queue.h>
#include <sys/tree.h>

#include <assert.h>
#include <err.h>
#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bt_parser.h"
#include "btrace.h"

#ifndef MIN
#define	MIN(_a,_b) ((_a) < (_b) ? (_a) : (_b))
#endif

#ifndef MAX
#define	MAX(_a,_b) ((_a) > (_b) ? (_a) : (_b))
#endif

RB_HEAD(map, mentry);

struct mentry {
	RB_ENTRY(mentry)	 mlink;
	char			 mkey[KLEN];
	struct bt_arg		*mval;
};

int		 mcmp(struct mentry *, struct mentry *);
struct mentry	*mget(struct map *, const char *);

RB_GENERATE(map, mentry, mlink, mcmp);

int
mcmp(struct mentry *me0, struct mentry *me1)
{
	return strncmp(me0->mkey, me1->mkey, KLEN - 1);
}

struct mentry *
mget(struct map *map, const char *key)
{
	struct mentry me, *mep;

	strlcpy(me.mkey, key, KLEN);
	mep = RB_FIND(map, map, &me);
	if (mep == NULL) {
		mep = calloc(1, sizeof(struct mentry));
		if (mep == NULL)
			err(1, "mentry: calloc");

		strlcpy(mep->mkey, key, KLEN);
		RB_INSERT(map, map, mep);
	}

	return mep;
}

void
map_clear(struct map *map)
{
	struct mentry *mep;

	while ((mep = RB_MIN(map, map)) != NULL) {
		RB_REMOVE(map, map, mep);
		free(mep);
	}

	assert(RB_EMPTY(map));
	free(map);
}

void
map_delete(struct map *map, const char *key)
{
	struct mentry me, *mep;

	strlcpy(me.mkey, key, KLEN);
	mep = RB_FIND(map, map, &me);
	if (mep != NULL) {
		RB_REMOVE(map, map, mep);
		free(mep);
	}
}

struct bt_arg *
map_get(struct map *map, const char *key)
{
	struct mentry *mep;

	mep = mget(map, key);
	if (mep->mval == NULL)
		mep->mval = ba_new(0, B_AT_LONG);

	return mep->mval;
}

struct map *
map_insert(struct map *map, const char *key, struct bt_arg *bval,
    struct dt_evt *dtev)
{
	struct mentry *mep;
	long val;

	if (map == NULL) {
		map = calloc(1, sizeof(struct map));
		if (map == NULL)
			err(1, "map: calloc");
	}

	mep = mget(map, key);
	switch (bval->ba_type) {
	case B_AT_STR:
	case B_AT_LONG:
		free(mep->mval);
		mep->mval = bval;
		break;
	case B_AT_BI_RETVAL:
		free(mep->mval);
		mep->mval = ba_new(ba2long(bval, dtev), B_AT_LONG);
		break;
	case B_AT_MF_COUNT:
		if (mep->mval == NULL)
			mep->mval = ba_new(0, B_AT_LONG);
		val = (long)mep->mval->ba_value;
		val++;
		mep->mval->ba_value = (void *)val;
		break;
	case B_AT_MF_MAX:
		if (mep->mval == NULL)
			mep->mval = ba_new(0, B_AT_LONG);
		val = (long)mep->mval->ba_value;
		val = MAX(val, ba2long(bval->ba_value, dtev));
		mep->mval->ba_value = (void *)val;
		break;
	case B_AT_MF_MIN:
		if (mep->mval == NULL)
			mep->mval = ba_new(0, B_AT_LONG);
		val = (long)mep->mval->ba_value;
		val = MIN(val, ba2long(bval->ba_value, dtev));
		mep->mval->ba_value = (void *)val;
		break;
	case B_AT_MF_SUM:
		if (mep->mval == NULL)
			mep->mval = ba_new(0, B_AT_LONG);
		val = (long)mep->mval->ba_value;
		val += ba2long(bval->ba_value, dtev);
		mep->mval->ba_value = (void *)val;
		break;
	default:
		errx(1, "no insert support for type %d", bval->ba_type);
	}

	return map;
}

static struct bt_arg nullba = BA_INITIALIZER(0, B_AT_LONG);
static struct bt_arg maxba = BA_INITIALIZER(LONG_MAX, B_AT_LONG);

/* Print at most `top' entries of the map ordered by value. */
void
map_print(struct map *map, size_t top, const char *name)
{
	struct mentry *mep, *mcur;
	struct bt_arg *bhigh, *bprev;
	size_t i;

	if (map == NULL)
		return;

	bprev = &maxba;
	for (i = 0; i < top; i++) {
		mcur = NULL;
		bhigh = &nullba;
		RB_FOREACH(mep, map, map) {
			if (bacmp(mep->mval, bhigh) >= 0 &&
			    bacmp(mep->mval, bprev) < 0 &&
			    mep->mval != bprev) {
				mcur = mep;
				bhigh = mcur->mval;
			}
		}
		if (mcur == NULL)
			break;
		printf("@%s[%s]: %s\n", name, mcur->mkey,
		    ba2str(mcur->mval, NULL));
		bprev = mcur->mval;
	}
}

void
map_zero(struct map *map)
{
	struct mentry *mep;

	RB_FOREACH(mep, map, map) {
		mep->mval->ba_value = 0;
		mep->mval->ba_type = B_AT_LONG;
	}
}

/*
 * Histogram implemented with map.
 */

struct hist {
	struct map	hmap;
	int		hstep;
};

struct hist *
hist_increment(struct hist *hist, const char *key, long step)
{
	static struct bt_arg incba = BA_INITIALIZER(NULL, B_AT_MF_COUNT);

	if (hist == NULL) {
		hist = calloc(1, sizeof(struct hist));
		if (hist == NULL)
			err(1, "hist: calloc");
		hist->hstep = step;
	}
	assert(hist->hstep == step);

	return (struct hist *)map_insert(&hist->hmap, key, &incba, NULL);
}

long
hist_get_bin_suffix(long bin, char **suffix)
{
#define GIGA	(MEGA * 1024)
#define MEGA	(KILO * 1024)
#define KILO	(1024)

	*suffix = "";
	if (bin >= GIGA) {
		bin /= GIGA;
		*suffix = "G";
	}
	if (bin >= MEGA) {
		bin /= MEGA;
		*suffix = "M";
	}
	if (bin >= KILO) {
		bin /= KILO;
		*suffix = "K";
	}

	return bin;
#undef MEGA
#undef KILO
}

/*
 * Print bucket header where `upb' is the upper bound of the interval
 * and `hstep' the width of the interval.
 */
static inline int
hist_print_bucket(char *buf, size_t buflen, long upb, long hstep)
{
	int l;

	if (hstep != 0) {
		/* Linear histogram */
		l = snprintf(buf, buflen, "[%lu, %lu)", upb - hstep, upb);
	} else {
		/* Power-of-two histogram */
		if (upb < 0) {
			l = snprintf(buf, buflen, "(..., 0)");
		} else if (upb == 0) {
			l = snprintf(buf, buflen, "[%lu]", upb);
		} else {
			long lob = upb / 2;
			char *lsuf, *usuf;

			upb = hist_get_bin_suffix(upb, &usuf);
			lob = hist_get_bin_suffix(lob, &lsuf);

			l = snprintf(buf, buflen, "[%lu%s, %lu%s)",
			    lob, lsuf, upb, usuf);
		}
	}

	if (l < 0 || (size_t)l > buflen)
		warn("string too long %d > %lu", l, sizeof(buf));

	return l;
}

void
hist_print(struct hist *hist, const char *name)
{
	struct map *map = &hist->hmap;
	static char buf[80];
	struct mentry *mep, *mcur;
	long bmin, bprev, bin, val, max = 0;
	int i, l, length = 52;

	if (map == NULL)
		return;

	bprev = 0;
	RB_FOREACH(mep, map, map) {
		val = ba2long(mep->mval, NULL);
		if (val > max)
			max = val;
	}
	printf("@%s:\n", name);

	/*
	 * Sort by ascending key.
	 */
	bprev = -1;
	for (;;) {
		mcur = NULL;
		bmin = LONG_MAX;

		RB_FOREACH(mep, map, map) {
			bin = atol(mep->mkey);
			if ((bin <= bmin) && (bin > bprev)) {
				mcur = mep;
				bmin = bin;
			}
		}
		if (mcur == NULL)
			break;

		bin = atol(mcur->mkey);
		val = ba2long(mcur->mval, NULL);
		i = (length * val) / max;
		l = hist_print_bucket(buf, sizeof(buf), bin, hist->hstep);
		snprintf(buf + l, sizeof(buf) - l, "%*ld |%.*s%*s|",
		    20 - l, val,
		    i, "@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@@",
		    length - i, "");
		printf("%s\n", buf);

		bprev = bin;
	}
}
