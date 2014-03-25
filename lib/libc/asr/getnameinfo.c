/*	$OpenBSD: getnameinfo.c,v 1.4 2014/03/25 19:48:11 eric Exp $	*/
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

#include "asr.h"

int
getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
    size_t hostlen, char *serv, size_t servlen, int flags)
{
	struct asr_query *as;
	struct asr_result ar;
	int saved_errno = errno;

	res_init();

	as = getnameinfo_async(sa, salen, host, hostlen, serv, servlen, flags,
	    NULL);
	if (as == NULL) {
		if (errno == ENOMEM) {
			errno = saved_errno;
			return (EAI_MEMORY);
		}
		return (EAI_SYSTEM);
	}

	asr_run_sync(as, &ar);
	if (ar.ar_gai_errno == EAI_SYSTEM)
		errno = ar.ar_errno;

	return (ar.ar_gai_errno);
}
