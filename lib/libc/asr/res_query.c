/*	$OpenBSD: res_query.c,v 1.1 2012/09/08 11:08:21 eric Exp $	*/
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
#include <netinet/in.h>

#include <errno.h>
#include <resolv.h>
#include <string.h>

#include "asr.h"

int
res_query(const char *name, int class, int type, u_char *ans, int anslen)
{
	struct async	*as;
	struct async_res ar;

	if (ans == NULL || anslen <= 0) {
		h_errno = NO_RECOVERY;
		errno = EINVAL;
		return (-1);
	}

	as = res_query_async(name, class, type, ans, anslen, NULL);
	if (as == NULL) {
		if (errno == EINVAL)
			h_errno = NO_RECOVERY;
		else
			h_errno = NETDB_INTERNAL;
		return (-1); /* errno set */
	}

	async_run_sync(as, &ar);

	if (ar.ar_errno)
		errno = ar.ar_errno;
	h_errno = ar.ar_h_errno;

	if (ar.ar_h_errno != NETDB_SUCCESS)
		return (-1);

	return (ar.ar_datalen);
}

int
res_search(const char *name, int class, int type, u_char *ans, int anslen)
{
	struct async	*as;
	struct async_res ar;

	if (ans == NULL || anslen <= 0) {
		h_errno = NO_RECOVERY;
		errno = EINVAL;
		return (-1);
	}

	as = res_search_async(name, class, type, ans, anslen, NULL);
	if (as == NULL) {
		if (errno == EINVAL)
			h_errno = NO_RECOVERY;
		else
			h_errno = NETDB_INTERNAL;
		return (-1); /* errno set */
	}

	async_run_sync(as, &ar);

	if (ar.ar_errno)
		errno = ar.ar_errno;
	h_errno = ar.ar_h_errno;

	if (ar.ar_h_errno != NETDB_SUCCESS)
		return (-1);

	return (ar.ar_datalen);
}
