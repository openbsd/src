/*	$OpenBSD: gethostnamadr.c,v 1.5 2013/04/04 17:49:33 eric Exp $	*/
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "asr.h"

static struct hostent *_gethostbyname(const char *, int);
static void _fillhostent(const struct hostent *, struct hostent *, char *buf,
    size_t);

static struct hostent	 _hostent;
static char		 _entbuf[4096];

static char *_empty[] = { NULL, };

static void
_fillhostent(const struct hostent *h, struct hostent *r, char *buf, size_t len)
{
	char	**ptr, *end, *pos;
	size_t	n, i;
	int	naliases, naddrs;

	bzero(buf, len);
	bzero(r, sizeof(*r));
	r->h_aliases = _empty;
	r->h_addr_list = _empty;

	end = buf + len;
	ptr = (char **)ALIGN(buf);

	if ((char *)ptr >= end)
		return;

	for (naliases = 0; h->h_aliases[naliases]; naliases++)
		;
	for (naddrs = 0; h->h_addr_list[naddrs]; naddrs++)
		;

	pos = (char *)(ptr + (naliases + 1) + (naddrs + 1));
	if (pos >= end)
		return;

	r->h_name = NULL;
	r->h_addrtype = h->h_addrtype;
	r->h_length = h->h_length;
	r->h_aliases = ptr;
	r->h_addr_list = ptr + naliases + 1;

	n = strlcpy(pos, h->h_name, end - pos);
	if (n >= end - pos)
		return;
	r->h_name = pos;
	pos += n + 1;

	for (i = 0; i < naliases; i++) {
		n = strlcpy(pos, h->h_aliases[i], end - pos);
		if (n >= end - pos)
			return;
		r->h_aliases[i] = pos;
		pos += n + 1;
	}

	pos = (char*)ALIGN(pos);
	if (pos >= end)
		return;

	for (i = 0; i < naddrs; i++) {
		if (r->h_length > end - pos)
			return;
		memmove(pos, h->h_addr_list[i], r->h_length);
		r->h_addr_list[i] = pos;
		pos += r->h_length;
	}
}

static struct hostent *
_gethostbyname(const char *name, int af)
{
	struct async		*as;
	struct async_res	 ar;

	if (af == -1)
		as = gethostbyname_async(name, NULL);
	else
		as = gethostbyname2_async(name, af, NULL);

	if (as == NULL) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}

	async_run_sync(as, &ar);

	errno = ar.ar_errno;
	h_errno = ar.ar_h_errno;
	if (ar.ar_hostent == NULL)
		return (NULL);

	_fillhostent(ar.ar_hostent, &_hostent, _entbuf, sizeof(_entbuf));
	free(ar.ar_hostent);

	return (&_hostent);
}

struct hostent *
gethostbyname(const char *name)
{
	return _gethostbyname(name, -1);
}

struct hostent *
gethostbyname2(const char *name, int af)
{
	return _gethostbyname(name, af);
}

struct hostent *
gethostbyaddr(const void *addr, socklen_t len, int af)
{
	struct async	*as;
	struct async_res ar;

	as = gethostbyaddr_async(addr, len, af, NULL);
	if (as == NULL) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}

	async_run_sync(as, &ar);

	errno = ar.ar_errno;
	h_errno = ar.ar_h_errno;
	if (ar.ar_hostent == NULL)
		return (NULL);

	_fillhostent(ar.ar_hostent, &_hostent, _entbuf, sizeof(_entbuf));
	free(ar.ar_hostent);

	return (&_hostent);
}
