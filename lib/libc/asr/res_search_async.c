/*	$OpenBSD: res_search_async.c,v 1.1 2012/04/14 09:24:18 eric Exp $	*/
/*
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
#include <sys/uio.h>

#include <arpa/nameser.h>
        
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* 
 * TODO:
 *
 * - make it possible to reuse ibuf if it was NULL when first called,
 *   to avoid reallocating buffers everytime.
 */

#include "asr.h"
#include "asr_private.h"

static int res_search_async_run(struct async *, struct async_res *);

/*
 * Unlike res_query_async(), this function returns a valid packet only if
 * h_errno is NETDB_SUCCESS.
 */
struct async *
res_search_async(const char *name, int class, int type, unsigned char *ans,
	int anslen, struct asr *asr)
{
	struct asr_ctx	*ac;
	struct async	*as;
#ifdef DEBUG
	asr_printf("asr: res_search_async(\"%s\", %i, %i)\n", name, class, type);
#endif
	ac = asr_use_resolver(asr);
	as = res_search_async_ctx(name, class, type, ans, anslen, ac);
	asr_ctx_unref(ac);

	return (as);
}

struct async *
res_search_async_ctx(const char *name, int class, int type, unsigned char *ans,
	int anslen, struct asr_ctx *ac)
{
	struct async	*as;

#ifdef DEBUG
	asr_printf("asr: res_search_async_ctx(\"%s\", %i, %i)\n",
	    name, class, type);
#endif

	if ((as = async_new(ac, ASR_SEARCH)) == NULL)
		goto err; /* errno set */
	as->as_run  = res_search_async_run;
	if ((as->as.search.name = strdup(name)) == NULL)
		goto err; /* errno set */

	if (ans) {
		as->as.search.flags |= ASYNC_EXTIBUF;
		as->as.search.ibuf = ans;
		as->as.search.ibufsize = anslen;
	} else {
		as->as.search.ibuf = NULL;
		as->as.search.ibufsize = 0;
	}
	as->as.search.ibuflen = 0;

	as->as.search.class = class;
	as->as.search.type = type;

	return (as);
    err:
	if (as)
		async_free(as);
	return (NULL);
}

#define HERRNO_UNSET	-2

static int
res_search_async_run(struct async *as, struct async_res *ar)
{
	int	r;
	char	fqdn[MAXDNAME];

    next:
	switch(as->as_state) {

	case ASR_STATE_INIT:

		as->as.search.saved_h_errno = HERRNO_UNSET;
		async_set_state(as, ASR_STATE_NEXT_DOMAIN);
		break;

	case ASR_STATE_NEXT_DOMAIN:

		/* Reset flags to be able to identify the case in STATE_SUBQUERY. */
		as->as_dom_flags = 0;

		r = asr_iter_domain(as, as->as.search.name, fqdn, sizeof(fqdn));
		if (r == -1) {
			async_set_state(as, ASR_STATE_NOT_FOUND);
			break;
		}
		if (r > sizeof(fqdn)) {
			ar->ar_errno = EINVAL;
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_datalen = -1;
			ar->ar_data = NULL;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}
		as->as.search.subq = res_query_async_ctx(fqdn,
		    as->as.search.class, as->as.search.type, 
		    as->as.search.ibuf, as->as.search.ibufsize, as->as_ctx);
		if (as->as.search.subq == NULL) {
			ar->ar_errno = errno;
			if (errno == EINVAL)
				ar->ar_h_errno = NO_RECOVERY;
			else
				ar->ar_h_errno = NETDB_INTERNAL;
			ar->ar_datalen = -1;
			ar->ar_data = NULL;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}
		async_set_state(as, ASR_STATE_SUBQUERY);
		break;

	case ASR_STATE_SUBQUERY:

		if ((r = async_run(as->as.search.subq, ar)) == ASYNC_COND)
			return (ASYNC_COND);
		as->as.search.subq = NULL;

		if (ar->ar_h_errno == NETDB_SUCCESS) {
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		/*
		 * The original res_search() does this in the domain search
		 * loop, but only for ECONNREFUSED. I think we can do better
		 * because technically if we get an errno, it means
		 * we couldn't reach any nameserver, so there is no point
		 * in trying further.
		 */
		if (ar->ar_errno) {
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		/*
		 * If we don't use an external buffer, the packet was allocated
		 * by the subquery and it must be freed now.
		 */
		if ((as->as.search.flags & ASYNC_EXTIBUF) == 0)
			free(ar->ar_data);

		/*
		 * The original resolver does something like this, to 
		 */
		if (as->as_dom_flags & (ASYNC_DOM_NDOTS | ASYNC_DOM_ASIS))
			as->as.search.saved_h_errno = ar->ar_h_errno;

		if (as->as_dom_flags & ASYNC_DOM_DOMAIN) {
			if (ar->ar_h_errno == NO_DATA)
				as->as.search.flags |= ASYNC_NODATA;
			else if (ar->ar_h_errno == TRY_AGAIN)
				as->as.search.flags |= ASYNC_AGAIN;
		}

		async_set_state(as, ASR_STATE_NEXT_DOMAIN);
		break;

	case ASR_STATE_NOT_FOUND:

		if (as->as.search.saved_h_errno != HERRNO_UNSET)
			ar->ar_h_errno = as->as.search.saved_h_errno;
		else if (as->as.search.flags & ASYNC_NODATA)
			ar->ar_h_errno = NO_DATA;
		else if (as->as.search.flags & ASYNC_AGAIN)
			ar->ar_h_errno = TRY_AGAIN;
		/* 
		 * Else, we got the ar_h_errno value set by res_query_async()
		 * for the last domain.
		 */
		ar->ar_datalen = -1;
		ar->ar_data = NULL;
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_HALT:

		return (ASYNC_DONE);

	default:
		ar->ar_errno = EOPNOTSUPP;
		ar->ar_h_errno = NETDB_INTERNAL;
		async_set_state(as, ASR_STATE_HALT);
                break;
	}
	goto next;
}
