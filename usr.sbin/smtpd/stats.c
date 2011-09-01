/*
 * Copyright (c) 2011 Eric Faurot <eric@openbsd.org>
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
#include <sys/socket.h>
#include <sys/queue.h>
#include <sys/tree.h>
#include <sys/param.h>

#include <event.h>
#include <imsg.h>
#include <stdio.h>

#include "smtpd.h"

static struct stat_counter	*counters = NULL;
static int			 ncounter = 0;

void
stat_init(struct stat_counter *c, int n)
{
	counters = c;
	ncounter = n;
}

size_t
stat_increment(int stat)
{
	if (stat < 0 || stat >= ncounter)
		return (-1);

	counters[stat].count++;
	if (++counters[stat].active > counters[stat].maxactive)
		counters[stat].maxactive = counters[stat].active;

	return (counters[stat].active);
}

size_t
stat_decrement(int stat)
{
	if (stat < 0 || stat >= ncounter)
		return (-1);

	counters[stat].active--;

	return (counters[stat].active);
}

size_t
stat_get(int stat, int what)
{
	if (stat < 0 || stat >= ncounter)
		return (-1);

	switch (what) {
	case STAT_COUNT:
		return counters[stat].count;
	case STAT_ACTIVE:
		return counters[stat].active;
	case STAT_MAXACTIVE:
		return counters[stat].maxactive;
	default:
		return (-1);
	}
}
