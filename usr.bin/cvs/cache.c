/*	$OpenBSD: cache.c,v 1.3 2004/12/15 05:09:06 jfb Exp $	*/
/*
 * Copyright (c) 2004 Jean-Francois Brousseau <jfb@openbsd.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL  DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "log.h"
#include "rcs.h"

#define RCS_CACHE_BUCKETS    256


struct rcs_cachent {
	u_int           rc_hits;
	u_int8_t        rc_hash;
	RCSFILE        *rc_rfp;
	struct timeval  rc_atime;

	TAILQ_ENTRY(rcs_cachent) rc_list;
	TAILQ_ENTRY(rcs_cachent) rc_lru;
};


static u_int8_t rcs_cache_hash (const char *);


TAILQ_HEAD(rcs_cachebkt, rcs_cachent) rcs_cache[RCS_CACHE_BUCKETS];

TAILQ_HEAD(rcs_lruhead, rcs_cachent) rcs_cache_lru;

u_int rcs_cache_maxent;
u_int rcs_cache_nbent;



/*
 * rcs_cache_init()
 *
 * Initialize the RCS file data cache.
 * Returns 0 on success, -1 on failure.
 */
int
rcs_cache_init(u_int maxent)
{
	u_int i;

	for (i = 0; i < RCS_CACHE_BUCKETS; i++)
		TAILQ_INIT(&rcs_cache[i]);

	TAILQ_INIT(&rcs_cache_lru);

	rcs_cache_maxent = maxent;
	rcs_cache_nbent = 0;

	return (0);
}


/*
 * rcs_cache_destroy()
 */
void
rcs_cache_destroy(void)
{
}


/*
 * rcs_cache_fetch()
 *
 */
RCSFILE*
rcs_cache_fetch(const char *path)
{
	u_int8_t hash;
	struct rcs_cachent *rcp;
	RCSFILE *rfp;

	rfp = NULL;
	hash = rcs_cache_hash(path);

	TAILQ_FOREACH(rcp, &(rcs_cache[hash]), rc_list) {
		if (strcmp(path, rcp->rc_rfp->rf_path) == 0) {
			rfp = rcp->rc_rfp;
			break;
		}
	}

	if (rcp != NULL) {
		(void)gettimeofday(&(rcp->rc_atime), NULL);
		rcp->rc_hits++;

		/* move this item back at the end of the LRU */
		TAILQ_REMOVE(&rcs_cache_lru, rcp, rc_lru);
		TAILQ_INSERT_TAIL(&rcs_cache_lru, rcp, rc_lru);

		/* increment reference count for caller */
		rfp->rf_ref++;
	}

	return (rfp);
}


/*
 * rcs_cache_store()
 *
 * Store the RCSFILE <rfp> in the RCS file cache.  By storing the file, its
 * reference count gets incremented for the cache reference, so the caller
 * should still rcs_close() the file once they are done with it.
 * Returns 0 on success, or -1 on failure.
 */
int
rcs_cache_store(RCSFILE *rfp)
{
	struct rcs_cachent *rcp, *old_rcp;

	/* don't store the same element twice */
	if (rcs_cache_fetch(rfp->rf_path) != NULL)
		return (0);

	rcp = (struct rcs_cachent *)malloc(sizeof(*rcp));
	if (rcp == NULL) {
		cvs_log(LP_ERRNO, "failed to allocate RCS cache entry");
		return (-1);
	}

	rcp->rc_hits = 0;

	rcp->rc_rfp = rfp;
	rfp->rf_ref++;

	rcp->rc_hash = rcs_cache_hash(rfp->rf_path);

	rcs_cache_nbent++;
	if (rcs_cache_nbent == rcs_cache_maxent) {
		/* ditch the oldest entry in the LRU */
		old_rcp = TAILQ_FIRST(&rcs_cache_lru);
		TAILQ_REMOVE(&rcs_cache_lru, old_rcp, rc_lru);
		TAILQ_REMOVE(&(rcs_cache[old_rcp->rc_hash]), old_rcp, rc_list);

		/* free our reference */
		rcs_close(old_rcp->rc_rfp);
		free(old_rcp);
	}

	TAILQ_INSERT_TAIL(&(rcs_cache[rcp->rc_hash]), rcp, rc_list);
	TAILQ_INSERT_TAIL(&rcs_cache_lru, rcp, rc_lru);

	return (0);
}


/*
 * rcs_cache_hash()
 *
 * Hash the <path> string.
 */
static u_int8_t
rcs_cache_hash(const char *path)
{
	const char *sp;
	u_int8_t hash;

	hash = 0;
	for (sp = path; *sp != '\0'; sp++)
		hash ^= (*sp << 3) ^ (*sp >> 2);

	return (hash);
}
