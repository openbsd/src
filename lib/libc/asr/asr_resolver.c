/*	$OpenBSD: asr_resolver.c,v 1.3 2012/07/08 17:01:06 eric Exp $	*/
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

/*
 * XXX this function is actually internal to asr, but we use it here to force
 * the creation a default resolver context in res_init().
 */
struct asr_ctx *asr_use_resolver(struct asr *);
void asr_ctx_unref(struct asr_ctx *);

static struct hostent *_gethostbyname(const char *, int);
static struct hostent *_mkstatichostent(struct hostent *);
static struct netent *_mkstaticnetent(struct netent *);


/* in res_init.c */
struct __res_state _res;
struct __res_state_ext _res_ext;

/* in res_query.c */ 
int h_errno;


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

	as = getrrsetbyname_async(name, class, type, flags, NULL);
	if (as == NULL)
		return (errno == ENOMEM) ? ERRSET_NOMEMORY : ERRSET_FAIL;

	async_run_sync(as, &ar);

	if (ar.ar_errno)
		errno = ar.ar_errno;

	*res = ar.ar_rrsetinfo;
	return (ar.ar_rrset_errno);
}

#define MAXALIASES	16
#define MAXADDRS	16

/* XXX bound checks are incorrect */
static struct hostent *
_mkstatichostent(struct hostent *h)
{
	static struct hostent r;
	static char buf[4096];
	static char *aliases[MAXALIASES+1];
	static uint64_t addrbuf[64];
	static char *addr_list[MAXADDRS + 1];

	char	*pos, **c;
	size_t	left, n;
	int	naliases = 0, naddrs = 0;

	r.h_addrtype = h->h_addrtype;
	r.h_length = h->h_length;
	r.h_name = buf;
	r.h_aliases = aliases;
	r.h_addr_list = addr_list;

	pos = buf;
	left = sizeof(buf);
	n = strlcpy(pos, h->h_name, left);
	pos += n + 1;
	left -= n + 1;

	for(c = h->h_aliases; left && *c && naliases < MAXALIASES; c++) {
		n = strlcpy(pos, *c, left);
		if (n >= left + 1)
			break;
		aliases[naliases++] = pos;
		pos += n + 1;
		left -= n + 1;
	}
	aliases[naliases] = NULL;

	pos = (char*)addrbuf;
	left = sizeof(addrbuf);
	for(c = h->h_addr_list; *c && naddrs < MAXADDRS; c++) {
		memmove(pos, *c,  r.h_length);
		addr_list[naddrs++] = pos;
		pos += r.h_length;
		left -= r.h_length;
        }
	addr_list[naddrs] = NULL;

	return (&r);
}

static struct hostent *
_gethostbyname(const char *name, int af)
{
	struct async	*as;
	struct async_res ar;
	struct hostent	*h;

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

	h = _mkstatichostent(ar.ar_hostent);
	freehostent(ar.ar_hostent);

	return (h);
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
	struct hostent	*h;

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

	h = _mkstatichostent(ar.ar_hostent);
	freehostent(ar.ar_hostent);

	return (h);
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

/* XXX bound checks are incorrect */
static struct netent *
_mkstaticnetent(struct netent *n)
{
	static struct netent r;
	static char buf[4096];
	static char *aliases[MAXALIASES+1];

	char	*pos, **c;
	size_t	left, s;
	int	naliases = 0;

	r.n_addrtype = n->n_addrtype;
	r.n_net = n->n_net;

	r.n_name = buf;
	r.n_aliases = aliases;

	pos = buf;
	left = sizeof(buf);
	s = strlcpy(pos, n->n_name, left);
	pos += s + 1;
	left -= s + 1;

	for(c = n->n_aliases; left && *c && naliases < MAXALIASES; c++) {
		s = strlcpy(pos, *c, left);
		if (s >= left + 1)
			break;
		aliases[naliases++] = pos;
		pos += s + 1;
		left -= s + 1;
	}
	aliases[naliases] = NULL;

	return (&r);
}

struct netent *
getnetbyname(const char *name)
{
	struct async	*as;
	struct async_res ar;
	struct netent	*n;

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

	n = _mkstaticnetent(ar.ar_netent);
	freenetent(ar.ar_netent);

	return (n);
}

struct netent *
getnetbyaddr(in_addr_t net, int type)
{
	struct async	*as;
	struct async_res ar;
	struct netent	*n;

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

	n = _mkstaticnetent(ar.ar_netent);
	freenetent(ar.ar_netent);

	return (n);
}

int
getaddrinfo(const char *hostname, const char *servname,
    const struct addrinfo *hints, struct addrinfo **res)
{
	struct async	*as;
	struct async_res ar;

	as = getaddrinfo_async(hostname, servname, hints, NULL);
	if (as == NULL)
		return ((errno == ENOMEM) ? EAI_MEMORY : EAI_SYSTEM);

	async_run_sync(as, &ar);

	errno = ar.ar_errno;
	h_errno = ar.ar_h_errno;
	*res = ar.ar_addrinfo;

	return (ar.ar_gai_errno);
}

int
getnameinfo(const struct sockaddr *sa, socklen_t salen, char *host,
    size_t hostlen, char *serv, size_t servlen, int flags)
{
	struct async	*as;
	struct async_res ar;

	as = getnameinfo_async(sa, salen, host, hostlen, serv, servlen, flags,
	    NULL);
	if (as == NULL)
		return ((errno == ENOMEM) ? EAI_MEMORY : EAI_SYSTEM);

	async_run_sync(as, &ar);

	errno = ar.ar_errno;
	h_errno = ar.ar_h_errno;

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
