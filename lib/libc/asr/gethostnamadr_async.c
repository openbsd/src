/*	$OpenBSD: gethostnamadr_async.c,v 1.2 2012/04/25 20:28:25 eric Exp $	*/
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
#include <sys/socket.h>

#include <netinet/in.h>
#include <arpa/inet.h>
#include <arpa/nameser.h>
        
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asr.h"
#include "asr_private.h"


#define MAXALIASES	16
#define MAXADDRS	16

ssize_t addr_as_fqdn(const char *, int, char *, size_t);

static int gethostnamadr_async_run(struct async *, struct async_res *);
static struct hostent *hostent_alloc(int);
static int hostent_set_cname(struct hostent *, const char *, int);
static int hostent_add_alias(struct hostent *, const char *, int);
static int hostent_add_addr(struct hostent *, const void *, int);
static int hostent_file_match(FILE *, int, int, int, const char *,
    char *, char **, int);
static int hostent_from_packet(struct hostent *, int, char *, size_t);

struct async *
gethostbyname_async(const char *name, struct asr *asr)
{
	return gethostbyname2_async(name, AF_INET, asr);
}

struct async *
gethostbyname2_async(const char *name, int af, struct asr *asr)
{
	struct asr_ctx	*ac;
	struct async	*as;

	/* the original segfaults */
	if (name == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	ac = asr_use_resolver(asr);
	if ((as = async_new(ac, ASR_GETHOSTBYNAME)) == NULL)
		goto abort; /* errno set */
	as->as_run = gethostnamadr_async_run;

	as->as.hostnamadr.family = af;
	if (af == AF_INET)
		as->as.hostnamadr.addrlen = INADDRSZ;
	else if (af == AF_INET6)
		as->as.hostnamadr.addrlen = IN6ADDRSZ;
	as->as.hostnamadr.name = strdup(name);
	if (as->as.hostnamadr.name == NULL)
		goto abort; /* errno set */

	asr_ctx_unref(ac);
	return (as);

    abort:
	if (as)
		async_free(as);
	asr_ctx_unref(ac);
	return (NULL);
}

struct async *
gethostbyaddr_async(const void *addr, socklen_t len, int af, struct asr *asr)
{
	struct asr_ctx	*ac;
	struct async	*as;

	ac = asr_use_resolver(asr);
	as = gethostbyaddr_async_ctx(addr, len, af, ac);
	asr_ctx_unref(ac);

	return (as);
}

struct async *
gethostbyaddr_async_ctx(const void *addr, socklen_t len, int af,
    struct asr_ctx *ac)
{
	struct async	*as;

	if ((as = async_new(ac, ASR_GETHOSTBYADDR)) == NULL)
		goto abort; /* errno set */
	as->as_run = gethostnamadr_async_run;

	as->as.hostnamadr.family = af;
	as->as.hostnamadr.addrlen = len;
	if (len > 0)
		memmove(as->as.hostnamadr.addr, addr, (len > 16) ? 16 : len);

	return (as);

    abort:
	if (as)
		async_free(as);
	return (NULL);
}

static int
gethostnamadr_async_run(struct async *as, struct async_res *ar)
{
	struct hostent	*e;
	int	i, n, r, type;
	FILE	*f;
	char	*toks[MAXTOKEN], addr[16], dname[MAXDNAME], *data;

    next:
	switch(as->as_state) {

	case ASR_STATE_INIT:

		if (as->as.hostnamadr.family != AF_INET &&
		    as->as.hostnamadr.family != AF_INET6) {
			ar->ar_h_errno = NETDB_INTERNAL;
			ar->ar_errno = EAFNOSUPPORT;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		if ((as->as.hostnamadr.family == AF_INET &&
		     as->as.hostnamadr.addrlen != INADDRSZ) ||
		    (as->as.hostnamadr.family == AF_INET6 &&
		     as->as.hostnamadr.addrlen != IN6ADDRSZ)) {
			ar->ar_h_errno = NETDB_INTERNAL;
			ar->ar_errno = EINVAL;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		if (as->as_type == ASR_GETHOSTBYNAME)
			async_set_state(as, ASR_STATE_NEXT_DOMAIN);
		else
			async_set_state(as, ASR_STATE_NEXT_DB);
		break;

	case ASR_STATE_NEXT_DOMAIN:

		r = asr_iter_domain(as, as->as.hostnamadr.name, dname, sizeof(dname));
		if (r == -1) {
			async_set_state(as, ASR_STATE_NOT_FOUND);
			break;
		}

		if (as->as.hostnamadr.dname)
			free(as->as.hostnamadr.dname);
		if ((as->as.hostnamadr.dname = strdup(dname)) == NULL) {
			ar->ar_h_errno = NETDB_INTERNAL;
			ar->ar_errno = errno;
			async_set_state(as, ASR_STATE_HALT);
		}

		as->as_db_idx = 0;
		async_set_state(as, ASR_STATE_NEXT_DB);
		break;

	case ASR_STATE_NEXT_DB:

		if (asr_iter_db(as) == -1) {
			if (as->as_type == ASR_GETHOSTBYNAME)
				async_set_state(as, ASR_STATE_NEXT_DOMAIN);
			else
				async_set_state(as, ASR_STATE_NOT_FOUND);
			break;
		}

		switch(AS_DB(as)) {

		case ASR_DB_DNS:

			/* Create a subquery to do the DNS lookup */

			if (as->as_type == ASR_GETHOSTBYNAME) {
				type = (as->as.hostnamadr.family == AF_INET) ?
				    T_A : T_AAAA;
				as->as.hostnamadr.subq = res_query_async_ctx(
				    as->as.hostnamadr.dname,
				    C_IN, type, NULL, 0, as->as_ctx);
			} else {
				addr_as_fqdn(as->as.hostnamadr.addr,
				    as->as.hostnamadr.family,
				    dname, sizeof(dname));
				as->as.hostnamadr.subq = res_query_async_ctx(
				    dname, C_IN, T_PTR, NULL, 0, as->as_ctx);
			}

			if (as->as.hostnamadr.subq == NULL) {
				ar->ar_errno = errno;
				ar->ar_h_errno = NETDB_INTERNAL;
				async_set_state(as, ASR_STATE_HALT);
				break;
			}

			async_set_state(as, ASR_STATE_SUBQUERY);
			break;

		case ASR_DB_FILE:

			/* Try to find a match in the host file */

			if ((f = fopen(as->as_ctx->ac_hostfile, "r")) == NULL)
				break;

			if (as->as_type == ASR_GETHOSTBYNAME)
				data = as->as.hostnamadr.dname;
			else
				data = as->as.hostnamadr.addr;

			if (( n = hostent_file_match(f, as->as_type,
			    as->as.hostnamadr.family,
			    as->as.hostnamadr.addrlen, data, addr,
			    toks, MAXTOKEN)) == -1) {
				fclose(f);
				break;
			}
			e = hostent_alloc(as->as.hostnamadr.family);
			if (e == NULL) {
				ar->ar_errno = errno;
				ar->ar_h_errno = NETDB_INTERNAL;
				async_set_state(as, ASR_STATE_HALT);
				break;
			}
			hostent_set_cname(e, toks[1], 0);
			for (i = 2; i < n; i ++)
				hostent_add_alias(e, toks[i], 0);
			hostent_add_addr(e, addr, e->h_length);
			fclose(f);

			ar->ar_h_errno = NETDB_SUCCESS;
			ar->ar_hostent = e;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}
		break;

	case ASR_STATE_SUBQUERY:

		/* Run the DNS subquery. */

		if ((r = async_run(as->as.hostnamadr.subq, ar)) == ASYNC_COND)
			return (ASYNC_COND);

		/* Done. */
		as->as.hostnamadr.subq = NULL;

		if (ar->ar_datalen == -1) {
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		/* If we got a packet but no anwser, use the next DB. */
		if (ar->ar_count == 0) {
			free(ar->ar_data);
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		/* Read the hostent from the packet. */
		if ((e = hostent_alloc(as->as.hostnamadr.family)) == NULL) {
			ar->ar_errno = errno;
			ar->ar_h_errno = NETDB_INTERNAL;
			free(ar->ar_data);
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		if (as->as_type == ASR_GETHOSTBYADDR) {
			e->h_addr_list[0] = malloc(as->as.hostnamadr.addrlen);
			if (e->h_addr_list[0])
				memmove(e->h_addr_list[0],
				    as->as.hostnamadr.addr,
				    as->as.hostnamadr.addrlen);
		}

		hostent_from_packet(e, as->as_type, ar->ar_data,
		    ar->ar_datalen);
		free(ar->ar_data);

		/*
		 * No address found in the dns packet. The blocking version
		 * reports this as an error.
		 */
		if (as->as_type == ASR_GETHOSTBYNAME &&
		    e->h_addr_list[0] == NULL) {
			freehostent(e);
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		ar->ar_h_errno = NETDB_SUCCESS;
		ar->ar_hostent = e;
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_NOT_FOUND:
		ar->ar_errno = 0;
		ar->ar_h_errno = HOST_NOT_FOUND;
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_HALT:
		if (ar->ar_h_errno)
			ar->ar_hostent = NULL;
		else
			ar->ar_errno = 0;
		return (ASYNC_DONE);

	default:
		ar->ar_errno = EOPNOTSUPP;
		ar->ar_h_errno = NETDB_INTERNAL;
		ar->ar_gai_errno = EAI_SYSTEM;
		async_set_state(as, ASR_STATE_HALT);
                break;
	}
	goto next;
}

/*
 * Lookup the first matching entry in the hostfile, either by address or by
 * name. Split the matching line into tokens in the "token" array and return
 * the number of tokens.
 */
static int
hostent_file_match(FILE *f, int type, int family, int len, const char *data,
    char *addr, char **tokens, int ntokens)
{
	int	n, i;

	for(;;) {
		n = asr_parse_namedb_line(f, tokens, MAXTOKEN);
		if (n == -1)
			return (-1);

		if (type == ASR_GETHOSTBYNAME) {
			for (i = 1; i < n; i++) {
				if (strcasecmp(data, tokens[i]))
					continue;
				if (inet_pton(family, tokens[0], addr) == 1)
					return (n);
			}
			continue;
		}

		if (inet_pton(family, tokens[0], addr) == 1)
			if (memcmp(addr, data, len) == 0)
				return (n);
	}
}

/*
 * Fill the hostent from the given DNS packet.
 */
static int
hostent_from_packet(struct hostent *h, int action, char *pkt, size_t pktlen)
{
	struct packed	 p;
	struct header	 hdr;
	struct query	 q;
	struct rr	 rr;
	int		 r;

	packed_init(&p, pkt, pktlen);
	unpack_header(&p, &hdr);
	for(; hdr.qdcount; hdr.qdcount--)
		unpack_query(&p, &q);
	for(; hdr.ancount; hdr.ancount--) {
		unpack_rr(&p, &rr);
		if (rr.rr_class != C_IN)
			continue;
		switch (rr.rr_type) {

		case T_CNAME:
			if (action == ASR_GETHOSTBYNAME)
				r = hostent_add_alias(h, rr.rr_dname, 1);
			else
				r = hostent_set_cname(h, rr.rr_dname, 1);
			break;

		case T_PTR:
			if (action != ASR_GETHOSTBYADDR)
				continue;
			r = hostent_set_cname(h, rr.rr.ptr.ptrname, 1);
			/* XXX See if we need MULTI_PTRS_ARE_ALIASES */
			break;

		case T_A:
			if (h->h_addrtype != AF_INET)
				break;
			r = hostent_set_cname(h, rr.rr_dname, 1);
			r = hostent_add_addr(h, &rr.rr.in_a.addr, 4);
			break;

		case T_AAAA:
			if (h->h_addrtype != AF_INET6)
				break;
			r = hostent_set_cname(h, rr.rr_dname, 1);
			r = hostent_add_addr(h, &rr.rr.in_aaaa.addr6, 16);
			break;
		}
	}

	return (0);
}

static struct hostent *
hostent_alloc(int family)
{
	struct hostent	*h;

	h = calloc(1, sizeof *h);
	if (h == NULL)
		return (NULL);

	h->h_aliases = calloc(MAXALIASES, sizeof *h->h_aliases);
	h->h_addr_list = calloc(MAXADDRS, sizeof *h->h_addr_list);
	if (h->h_aliases == NULL || h->h_addr_list == NULL) {
		freehostent(h);
		return (NULL);
	}
	h->h_addrtype = family;
	h->h_length = (family == AF_INET) ? 4 : 16;

	return (h);
}

static int
hostent_set_cname(struct hostent *h, const char *name, int isdname)
{
	char	buf[MAXDNAME];

	if (h->h_name)
		return (0);

	if (isdname) {
		asr_strdname(name, buf, sizeof buf);
		buf[strlen(buf) - 1] = '\0';
		h->h_name = strdup(buf);
	} else {
		h->h_name = strdup(name);
	}
	if (h->h_name == NULL)
		return (-1);

	return (0);
}

static int
hostent_add_alias(struct hostent *h, const char *name, int isdname)
{
	char	buf[MAXDNAME];
	size_t	i;

	for (i = 0; i < MAXALIASES; i++)
		if (h->h_aliases[i] == NULL)
			break;
	if (i == MAXALIASES)
		return (0);

	if (isdname) {
		asr_strdname(name, buf, sizeof buf);
		buf[strlen(buf)-1] = '\0';
		h->h_aliases[i] = strdup(buf);
	} else {
		h->h_aliases[i] = strdup(name);
	}
	if (h->h_aliases[i] == NULL)
		return (-1);

	return (0);
}

static int
hostent_add_addr(struct hostent *h, const void *addr, int size)
{
	int	i;

	for (i = 0; i < MAXADDRS; i++)
		if (h->h_addr_list[i] == NULL)
			break;
	if (i == MAXADDRS)
		return (0);

	h->h_addr_list[i] = malloc(size);
	if (h->h_addr_list[i] == NULL)
		return (-1);
	memmove(h->h_addr_list[i], addr, size);

	return (0);
}

void
freehostent(struct hostent *h)
{
	char **c;

	free(h->h_name);
	for (c = h->h_aliases; *c; c++)
		free(*c);
	free(h->h_aliases);
	for (c = h->h_addr_list; *c; c++)
		free(*c);
	free(h->h_addr_list);
	free(h);
}


ssize_t
addr_as_fqdn(const char *addr, int family, char *dst, size_t max)
{
	const struct in6_addr	*in6_addr;
	in_addr_t		 in_addr;
	
	switch (family) {
	case AF_INET:
		in_addr = ntohl(*((const in_addr_t *)addr));
		snprintf(dst, max,
		    "%d.%d.%d.%d.in-addr.arpa.",
		    in_addr & 0xff,
		    (in_addr >> 8) & 0xff,
		    (in_addr >> 16) & 0xff,
		    (in_addr >> 24) & 0xff);
		break;
	case AF_INET6:
		in6_addr = (const struct in6_addr *)addr;
		snprintf(dst, max,
		    "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		    "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		    "ip6.arpa.",
		    in6_addr->s6_addr[15] & 0xf,
		    (in6_addr->s6_addr[15] >> 4) & 0xf,
		    in6_addr->s6_addr[14] & 0xf,
		    (in6_addr->s6_addr[14] >> 4) & 0xf,
		    in6_addr->s6_addr[13] & 0xf,
		    (in6_addr->s6_addr[13] >> 4) & 0xf,
		    in6_addr->s6_addr[12] & 0xf,
		    (in6_addr->s6_addr[12] >> 4) & 0xf,
		    in6_addr->s6_addr[11] & 0xf,
		    (in6_addr->s6_addr[11] >> 4) & 0xf,
		    in6_addr->s6_addr[10] & 0xf,
		    (in6_addr->s6_addr[10] >> 4) & 0xf,
		    in6_addr->s6_addr[9] & 0xf,
		    (in6_addr->s6_addr[9] >> 4) & 0xf,
		    in6_addr->s6_addr[8] & 0xf,
		    (in6_addr->s6_addr[8] >> 4) & 0xf,
		    in6_addr->s6_addr[7] & 0xf,
		    (in6_addr->s6_addr[7] >> 4) & 0xf,
		    in6_addr->s6_addr[6] & 0xf,
		    (in6_addr->s6_addr[6] >> 4) & 0xf,
		    in6_addr->s6_addr[5] & 0xf,
		    (in6_addr->s6_addr[5] >> 4) & 0xf,
		    in6_addr->s6_addr[4] & 0xf,
		    (in6_addr->s6_addr[4] >> 4) & 0xf,
		    in6_addr->s6_addr[3] & 0xf,
		    (in6_addr->s6_addr[3] >> 4) & 0xf,
		    in6_addr->s6_addr[2] & 0xf,
		    (in6_addr->s6_addr[2] >> 4) & 0xf,
		    in6_addr->s6_addr[1] & 0xf,
		    (in6_addr->s6_addr[1] >> 4) & 0xf,
		    in6_addr->s6_addr[0] & 0xf,
		    (in6_addr->s6_addr[0] >> 4) & 0xf);
		break;
	default:
		return (-1);
	}
	return (0);
}
