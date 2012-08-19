/*	$OpenBSD: asr_resolver.c,v 1.10 2012/08/19 17:59:15 eric Exp $	*/
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

#include <arpa/nameser.h> /* for MAXDNAME */
#include <errno.h>
#include <resolv.h>
#include <string.h>

#include "asr.h"
#include "asr_private.h"

/*
 * XXX this function is actually internal to asr, but we use it here to force
 * the creation a default resolver context in res_init().
 */
struct asr_ctx *asr_use_resolver(struct asr *);
void asr_ctx_unref(struct asr_ctx *);

static struct hostent *_gethostbyname(const char *, int);
static void _fillhostent(const struct hostent *, struct hostent *, char *buf,
    size_t);
static void _fillnetent(const struct netent *, struct netent *, char *buf,
    size_t);

/* in res_init.c */
struct __res_state _res;
struct __res_state_ext _res_ext;

/* in res_query.c */ 
int h_errno;

static struct hostent	 _hostent;
static struct netent	 _netent;
static char		 _entbuf[4096];

static char *_empty[] = { NULL, };

int
res_init(void)
{
	async_resolver_done(NULL);
	asr_ctx_unref(asr_use_resolver(NULL));

	return (0);
}

int
res_send(const u_char *buf, int buflen, u_char *ans, int anslen)
{
	struct async	*as;
	struct async_res ar;

	if (ans == NULL || anslen <= 0) {
		errno = EINVAL;
		return (-1);
	}

	as = res_send_async(buf, buflen, ans, anslen, NULL);
	if (as == NULL)
		return (-1); /* errno set */

	async_run_sync(as, &ar);

	if (ar.ar_errno) {
		errno = ar.ar_errno;
		return (-1);
	}

	return (ar.ar_datalen);
}

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

/* This function is not documented, but used by sendmail. */
int
res_querydomain(const char *name,
    const char *domain,
    int class,
    int type,
    u_char *answer,
    int anslen)
{
	char	fqdn[MAXDNAME], ndom[MAXDNAME];
	size_t	n;

	/* we really want domain to end with a dot for now */
	if (domain && (n = strlen(domain)) == 0 || domain[n - 1 ] != '.') {
		domain = ndom;
		strlcpy(ndom, domain, sizeof ndom);
		strlcat(ndom, ".", sizeof ndom);
	}

	if (asr_make_fqdn(name, domain, fqdn, sizeof fqdn) == 0) {
		h_errno = NO_RECOVERY;
		errno = EINVAL;
		return (-1);
	}

	return (res_query(fqdn, class, type, answer, anslen));
}

/* This function is apparently needed by some ports. */
int
res_mkquery(int op, const char *dname, int class, int type,
    const unsigned char *data, int datalen, const unsigned char *newrr,
    unsigned char *buf, int buflen)
{
	struct asr_ctx	*ac;
	struct packed	 p;
	struct header	 h;
	char		 fqdn[MAXDNAME];
	char		 dn[MAXDNAME];

	/* we currently only support QUERY */
	if (op != QUERY || data)
		return (-1);

	if (dname[0] == '\0' || dname[strlen(dname) - 1] != '.') {
		strlcpy(fqdn, dname, sizeof fqdn);
		if (strlcat(fqdn, ".", sizeof fqdn) >= sizeof fqdn)
			return (-1);
		dname = fqdn;
	}

	if (dname_from_fqdn(dname, dn, sizeof(dn)) == -1)
		return (-1);

	ac = asr_use_resolver(NULL);

	memset(&h, 0, sizeof h);
	h.id = res_randomid();
	if (ac->ac_options & RES_RECURSE)
		h.flags |= RD_MASK;
	h.qdcount = 1;

	packed_init(&p, buf, buflen);
	pack_header(&p, &h);
	pack_query(&p, type, class, dn);

	asr_ctx_unref(ac);

	if (p.err)
		return (-1);

	return (p.offset);
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

int
getrrsetbyname(const char *name, unsigned int class, unsigned int type,
    unsigned int flags, struct rrsetinfo **res)
{
	struct async	*as;
	struct async_res ar;
	int		 r, saved_errno = errno;

	as = getrrsetbyname_async(name, class, type, flags, NULL);
	if (as == NULL) {
		r = (errno == ENOMEM) ? ERRSET_NOMEMORY : ERRSET_FAIL;
		errno = saved_errno;
		return (r);
	}

	async_run_sync(as, &ar);

	*res = ar.ar_rrsetinfo;

	return (ar.ar_rrset_errno);
}

void
_fillhostent(const struct hostent *h, struct hostent *r, char *buf, size_t len)
{
	char	**ptr, *end, *pos;
	size_t	n, i;
	int	naliases, naddrs;

	end = buf + len;
	ptr = (char**)buf; /* XXX align */

	for (naliases = 0; h->h_aliases[naliases]; naliases++)
		;
	for (naddrs = 0; h->h_addr_list[naddrs]; naddrs++)
		;

	r->h_name = NULL;
	r->h_addrtype = h->h_addrtype;
	r->h_length = h->h_length;
	r->h_aliases = ptr;
	r->h_addr_list = ptr + naliases + 1;

	pos = (char*)(ptr + (naliases + 1) + (naddrs + 1));
	if (pos > end) {
		r->h_aliases = _empty;
		r->h_addr_list = _empty;
		return;
	}
	bzero(ptr, pos - (char*)ptr);

	n = strlcpy(pos, h->h_name, end - pos);
	if (n >= end - pos)
		return;
	r->h_name = pos;
	pos += n + 1;

	for(i = 0; i < naliases; i++) {
		n = strlcpy(pos, h->h_aliases[i], end - pos);
		if (n >= end - pos)
			return;
		r->h_aliases[i] = pos;
		pos += n + 1;
	}

	for(i = 0; i < naddrs; i++) {
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

/* XXX These functions do nothing for now. */
void
sethostent(int stayopen)
{
}

void
endhostent(void)
{
}

struct hostent *
gethostent(void)
{
	h_errno = NETDB_INTERNAL;
	return (NULL);
}

void
_fillnetent(const struct netent *e, struct netent *r, char *buf, size_t len)
{
	char	**ptr, *end, *pos;
	size_t	n, i;
	int	naliases;

	end = buf + len;
	ptr = (char**)buf; /* XXX align */

	for (naliases = 0; e->n_aliases[naliases]; naliases++)
		;

	r->n_name = NULL;
	r->n_addrtype = e->n_addrtype;
	r->n_net = e->n_net;
	r->n_aliases = ptr;

	pos = (char *)(ptr + (naliases + 1));
	if (pos > end) {
		r->n_aliases = _empty;
		return;
	}
	bzero(ptr, pos - (char *)ptr);

	n = strlcpy(pos, e->n_name, end - pos);
	if (n >= end - pos)
		return;
	r->n_name = pos;
	pos += n + 1;

	for(i = 0; i < naliases; i++) {
		n = strlcpy(pos, e->n_aliases[i], end - pos);
		if (n >= end - pos)
			return;
		r->n_aliases[i] = pos;
		pos += n + 1;
	}
}

struct netent *
getnetbyname(const char *name)
{
	struct async	*as;
	struct async_res ar;

	as = getnetbyname_async(name, NULL);
	if (as == NULL) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}

	async_run_sync(as, &ar);

	errno = ar.ar_errno;
	h_errno = ar.ar_h_errno;
	if (ar.ar_netent == NULL)
		return (NULL);

	_fillnetent(ar.ar_netent, &_netent, _entbuf, sizeof(_entbuf));
	free(ar.ar_netent);

	return (&_netent);
}

struct netent *
getnetbyaddr(in_addr_t net, int type)
{
	struct async	*as;
	struct async_res ar;

	as = getnetbyaddr_async(net, type, NULL);
	if (as == NULL) {
		h_errno = NETDB_INTERNAL;
		return (NULL);
	}

	async_run_sync(as, &ar);

	errno = ar.ar_errno;
	h_errno = ar.ar_h_errno;
	if (ar.ar_netent == NULL)
		return (NULL);

	_fillnetent(ar.ar_netent, &_netent, _entbuf, sizeof(_entbuf));
	free(ar.ar_netent);

	return (&_netent);
}

int
getaddrinfo(const char *hostname, const char *servname,
    const struct addrinfo *hints, struct addrinfo **res)
{
	struct async	*as;
	struct async_res ar;
	int		 saved_errno = errno;

	as = getaddrinfo_async(hostname, servname, hints, NULL);
	if (as == NULL) {
		if (errno == ENOMEM) {
			errno = saved_errno;
			return (EAI_MEMORY);
		}
		return (EAI_SYSTEM);
	}

	async_run_sync(as, &ar);

	*res = ar.ar_addrinfo;
	if (ar.ar_gai_errno == EAI_SYSTEM)
		errno = ar.ar_errno;

	return (ar.ar_gai_errno);
}

int
getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
    size_t hostlen, char *serv, size_t servlen, int flags)
{
	struct async	*as;
	struct async_res ar;
	int		 saved_errno = errno;

	as = getnameinfo_async(sa, salen, host, hostlen, serv, servlen, flags,
	    NULL);
	if (as == NULL) {
		if (errno == ENOMEM) {
			errno = saved_errno;
			return (EAI_MEMORY);
		}
		return (EAI_SYSTEM);
	}

	async_run_sync(as, &ar);
	if (ar.ar_gai_errno == EAI_SYSTEM)
		errno = ar.ar_errno;

	return (ar.ar_gai_errno);
}

/* from getrrsetbyname.c */
void
freerrset(struct rrsetinfo *rrset)
{
	u_int16_t i;

	if (rrset == NULL)
		return;

	if (rrset->rri_rdatas) {
		for (i = 0; i < rrset->rri_nrdatas; i++) {
			if (rrset->rri_rdatas[i].rdi_data == NULL)
				break;
			free(rrset->rri_rdatas[i].rdi_data);
		}
		free(rrset->rri_rdatas);
	}

	if (rrset->rri_sigs) {
		for (i = 0; i < rrset->rri_nsigs; i++) {
			if (rrset->rri_sigs[i].rdi_data == NULL)
				break;
			free(rrset->rri_sigs[i].rdi_data);
		}
		free(rrset->rri_sigs);
	}

	if (rrset->rri_name)
		free(rrset->rri_name);
	free(rrset);
}
