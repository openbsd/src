/* $Id: cache.h,v 1.2 2008/07/16 10:23:39 canacar Exp $ */
/*
 * Copyright (c) 2001, 2007 Can Erkin Acar <canacar@openbsd.org>
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

#ifndef _CACHE_H_
#define _CACHE_H_

#include <sys/queue.h>
#include <sys/tree.h>
#include <net/pfvar.h>

struct sc_ent {
        RB_ENTRY(sc_ent)    tlink;
	TAILQ_ENTRY(sc_ent) qlink;
	u_int32_t	    id[2];
	double		    peak;
	double		    rate;
	time_t		    t;
	u_int32_t	    bytes;
};

int cache_init(int);
void cache_endupdate(void);
struct sc_ent *cache_state(struct pfsync_state *);
extern int cache_max, cache_size;

#define COUNTER(c) ((((u_int64_t) ntohl(c[0]))<<32) + ntohl(c[1]))


#endif
