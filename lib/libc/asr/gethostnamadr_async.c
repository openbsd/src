/*	$OpenBSD: gethostnamadr_async.c,v 1.33 2015/01/14 23:36:15 deraadt Exp $	*/
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
#include <netdb.h>

#include <asr.h>
#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <resolv.h> /* for res_hnok */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <limits.h>

#ifdef YP
#include <rpc/rpc.h>
#include <rpcsvc/yp.h>
#include <rpcsvc/ypclnt.h>
#include "ypinternal.h"
#endif

#include "asr_private.h"

#define MAXALIASES	16
#define MAXADDRS	16

struct hostent_ext {
	struct hostent	 h;
	char		*aliases[MAXALIASES + 1];
	char		*addrs[MAXADDRS + 1];
	char		*end;
	char		*pos;
};

static int gethostnamadr_async_run(struct asr_query *, struct asr_result *);
static struct hostent_ext *hostent_alloc(int);
static int hostent_set_cname(struct hostent_ext *, const char *, int);
static int hostent_add_alias(struct hostent_ext *, const char *, int);
static int hostent_add_addr(struct hostent_ext *, const void *, size_t);
static struct hostent_ext *hostent_from_addr(int, const char *, const char *);
static struct hostent_ext *hostent_file_match(FILE *, int, int, const char *,
    int);
static struct hostent_ext *hostent_from_packet(int, int, char *, size_t);
#ifdef YP
static struct hostent_ext *_yp_gethostnamadr(int, const void *);
static struct hostent_ext *hostent_from_yp(int, char *);
#endif

struct asr_query *
gethostbyname_async(const char *name, void *asr)
{
	return gethostbyname2_async(name, AF_INET, asr);
}

struct asr_query *
gethostbyname2_async(const char *name, int af, void *asr)
{
	struct asr_ctx	 *ac;
	struct asr_query *as;

	/* the original segfaults */
	if (name == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	ac = asr_use_resolver(asr);
	if ((as = asr_async_new(ac, ASR_GETHOSTBYNAME)) == NULL)
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
		asr_async_free(as);
	asr_ctx_unref(ac);
	return (NULL);
}

struct asr_query *
gethostbyaddr_async(const void *addr, socklen_t len, int af, void *asr)
{
	struct asr_ctx	 *ac;
	struct asr_query *as;

	ac = asr_use_resolver(asr);
	as = gethostbyaddr_async_ctx(addr, len, af, ac);
	asr_ctx_unref(ac);

	return (as);
}

struct asr_query *
gethostbyaddr_async_ctx(const void *addr, socklen_t len, int af,
    struct asr_ctx *ac)
{
	struct asr_query *as;

	if ((as = asr_async_new(ac, ASR_GETHOSTBYADDR)) == NULL)
		goto abort; /* errno set */
	as->as_run = gethostnamadr_async_run;

	as->as.hostnamadr.family = af;
	as->as.hostnamadr.addrlen = len;
	if (len > 0)
		memmove(as->as.hostnamadr.addr, addr, (len > 16) ? 16 : len);

	return (as);

    abort:
	if (as)
		asr_async_free(as);
	return (NULL);
}

static int
gethostnamadr_async_run(struct asr_query *as, struct asr_result *ar)
{
	struct hostent_ext	*h;
	int			 r, type, saved_errno;
	FILE			*f;
	char			 name[MAXDNAME], *data, addr[16], *c;

    next:
	switch (as->as_state) {

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

		if (as->as_type == ASR_GETHOSTBYNAME) {

			if (as->as.hostnamadr.name[0] == '\0') {
				ar->ar_h_errno = NO_DATA;
				async_set_state(as, ASR_STATE_HALT);
				break;
			}

			/* Name might be an IP address string */
			for (c = as->as.hostnamadr.name; *c; c++)
				if (!isdigit((unsigned char)*c) &&
				     *c != '.' && *c != ':')
					break;
			if (*c == 0 &&
			    inet_pton(as->as.hostnamadr.family,
			    as->as.hostnamadr.name, addr) == 1) {
				h = hostent_from_addr(as->as.hostnamadr.family,
				    as->as.hostnamadr.name, addr);
				if (h == NULL) {
					ar->ar_errno = errno;
					ar->ar_h_errno = NETDB_INTERNAL;
				}
				else {
					ar->ar_hostent = &h->h;
					ar->ar_h_errno = NETDB_SUCCESS;
				}
				async_set_state(as, ASR_STATE_HALT);
				break;
			}
		}
		async_set_state(as, ASR_STATE_NEXT_DB);
		break;

	case ASR_STATE_NEXT_DB:

		if (asr_iter_db(as) == -1) {
			async_set_state(as, ASR_STATE_NOT_FOUND);
			break;
		}

		switch (AS_DB(as)) {

		case ASR_DB_DNS:

			/* Create a subquery to do the DNS lookup */

			if (as->as_type == ASR_GETHOSTBYNAME) {
				type = (as->as.hostnamadr.family == AF_INET) ?
				    T_A : T_AAAA;
				as->as.hostnamadr.subq = res_search_async_ctx(
				    as->as.hostnamadr.name,
				    C_IN, type, as->as_ctx);
			} else {
				asr_addr_as_fqdn(as->as.hostnamadr.addr,
				    as->as.hostnamadr.family,
				    name, sizeof(name));
				as->as.hostnamadr.subq = res_query_async_ctx(
				    name, C_IN, T_PTR, as->as_ctx);
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

			if ((f = fopen(as->as_ctx->ac_hostfile, "re")) == NULL)
				break;

			if (as->as_type == ASR_GETHOSTBYNAME) {
				data = asr_hostalias(as->as_ctx,
				    as->as.hostnamadr.name, name, sizeof(name));
				if (data == NULL)
					data = as->as.hostnamadr.name;
			}
			else
				data = as->as.hostnamadr.addr;

			h = hostent_file_match(f, as->as_type,
			    as->as.hostnamadr.family, data,
			    as->as.hostnamadr.addrlen);
			saved_errno = errno;
			fclose(f);
			errno = saved_errno;

			if (h == NULL) {
				if (errno) {
					ar->ar_errno = errno;
					ar->ar_h_errno = NETDB_INTERNAL;
					async_set_state(as, ASR_STATE_HALT);
				}
				/* otherwise not found */
				break;
			}
			ar->ar_hostent = &h->h;
			ar->ar_h_errno = NETDB_SUCCESS;
			async_set_state(as, ASR_STATE_HALT);
			break;
#ifdef YP
		case ASR_DB_YP:
			/* IPv4 only */
			if (as->as.hostnamadr.family != AF_INET)
				break;
			if (as->as_type == ASR_GETHOSTBYNAME) {
				data = asr_hostalias(as->as_ctx,
				    as->as.hostnamadr.name, name, sizeof(name));
				if (data == NULL)
					data = as->as.hostnamadr.name;
			}
			else
				data = as->as.hostnamadr.addr;
			h = _yp_gethostnamadr(as->as_type, data);
			if (h == NULL) {
				if (errno) {
					ar->ar_errno = errno;
					ar->ar_h_errno = NETDB_INTERNAL;
					async_set_state(as, ASR_STATE_HALT);
				}
				/* otherwise not found */
				break;
			}
			ar->ar_hostent = &h->h;
			ar->ar_h_errno = NETDB_SUCCESS;
			async_set_state(as, ASR_STATE_HALT);
			break;
#endif
		}
		break;

	case ASR_STATE_SUBQUERY:

		/* Run the DNS subquery. */

		if ((r = asr_run(as->as.hostnamadr.subq, ar)) == ASYNC_COND)
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
			as->as.hostnamadr.subq_h_errno = ar->ar_h_errno;
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		/* Read the hostent from the packet. */

		h = hostent_from_packet(as->as_type,
		    as->as.hostnamadr.family, ar->ar_data, ar->ar_datalen);
		free(ar->ar_data);
		if (h == NULL) {
			ar->ar_errno = errno;
			ar->ar_h_errno = NETDB_INTERNAL;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		if (as->as_type == ASR_GETHOSTBYADDR) {
			if (hostent_add_addr(h, as->as.hostnamadr.addr,
			    as->as.hostnamadr.addrlen) == -1) {
				free(h);
				ar->ar_errno = errno;
				ar->ar_h_errno = NETDB_INTERNAL;
				async_set_state(as, ASR_STATE_HALT);
				break;
			}
		}

		/*
		 * No valid hostname or address found in the dns packet.
		 * Ignore it.
		 */
		if ((as->as_type == ASR_GETHOSTBYNAME &&
		     h->h.h_addr_list[0] == NULL) ||
		    h->h.h_name == NULL) {
			free(h);
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		ar->ar_hostent = &h->h;
		ar->ar_h_errno = NETDB_SUCCESS;
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_NOT_FOUND:
		ar->ar_errno = 0;
		if (as->as.hostnamadr.subq_h_errno)
			ar->ar_h_errno = as->as.hostnamadr.subq_h_errno;
		else
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
 * Create a hostent from a numeric address string.
 */
static struct hostent_ext *
hostent_from_addr(int family, const char *name, const char *addr)
{
	struct	 hostent_ext *h;

	if ((h = hostent_alloc(family)) == NULL)
		return (NULL);
	if (hostent_set_cname(h, name, 0) == -1)
		goto fail;
	if (hostent_add_addr(h, addr, h->h.h_length) == -1)
		goto fail;
	return (h);
fail:
	free(h);
	return (NULL);
}

/*
 * Lookup the first matching entry in the hostfile, either by address or by
 * name depending on reqtype, and build a hostent from the line.
 */
static struct hostent_ext *
hostent_file_match(FILE *f, int reqtype, int family, const char *data,
    int datalen)
{
	char	*tokens[MAXTOKEN], addr[16];
	struct	 hostent_ext *h;
	int	 n, i;

	for (;;) {
		n = asr_parse_namedb_line(f, tokens, MAXTOKEN);
		if (n == -1) {
			errno = 0; /* ignore errors reading the file */
			return (NULL);
		}

		/* there must be an address and at least one name */
		if (n < 2)
			continue;

		if (reqtype == ASR_GETHOSTBYNAME) {
			for (i = 1; i < n; i++) {
				if (strcasecmp(data, tokens[i]))
					continue;
				if (inet_pton(family, tokens[0], addr) == 1)
					goto found;
			}
		} else {
			if (inet_pton(family, tokens[0], addr) == 1 &&
			    memcmp(addr, data, datalen) == 0)
				goto found;
		}
	}

found:
	if ((h = hostent_alloc(family)) == NULL)
		return (NULL);
	if (hostent_set_cname(h, tokens[1], 0) == -1)
		goto fail;
	for (i = 2; i < n; i ++)
		if (hostent_add_alias(h, tokens[i], 0) == -1)
			goto fail;
	if (hostent_add_addr(h, addr, h->h.h_length) == -1)
		goto fail;
	return (h);
fail:
	free(h);
	return (NULL);
}

/*
 * Fill the hostent from the given DNS packet.
 */
static struct hostent_ext *
hostent_from_packet(int reqtype, int family, char *pkt, size_t pktlen)
{
	struct hostent_ext	*h;
	struct asr_unpack	 p;
	struct asr_dns_header	 hdr;
	struct asr_dns_query	 q;
	struct asr_dns_rr	 rr;
	char			 dname[MAXDNAME];

	if ((h = hostent_alloc(family)) == NULL)
		return (NULL);

	asr_unpack_init(&p, pkt, pktlen);
	asr_unpack_header(&p, &hdr);
	for (; hdr.qdcount; hdr.qdcount--)
		asr_unpack_query(&p, &q);
	strlcpy(dname, q.q_dname, sizeof(dname));

	for (; hdr.ancount; hdr.ancount--) {
		asr_unpack_rr(&p, &rr);
		if (rr.rr_class != C_IN)
			continue;
		switch (rr.rr_type) {

		case T_CNAME:
			if (reqtype == ASR_GETHOSTBYNAME) {
				if (hostent_add_alias(h, rr.rr_dname, 1) == -1)
					goto fail;
			} else {
				if (strcasecmp(rr.rr_dname, dname) == 0)
					strlcpy(dname, rr.rr.cname.cname,
					    sizeof(dname));
			}
			break;

		case T_PTR:
			if (reqtype != ASR_GETHOSTBYADDR)
				break;
			if (strcasecmp(rr.rr_dname, dname) != 0)
				continue;
			if (hostent_set_cname(h, rr.rr.ptr.ptrname, 1) == -1)
				hostent_add_alias(h, rr.rr.ptr.ptrname, 1);
			break;

		case T_A:
			if (reqtype != ASR_GETHOSTBYNAME)
				break;
			if (family != AF_INET)
				break;
			if (hostent_set_cname(h, rr.rr_dname, 1) == -1)
				;
			if (hostent_add_addr(h, &rr.rr.in_a.addr, 4) == -1)
				goto fail;
			break;

		case T_AAAA:
			if (reqtype != ASR_GETHOSTBYNAME)
				break;
			if (family != AF_INET6)
				break;
			if (hostent_set_cname(h, rr.rr_dname, 1) == -1)
				;
			if (hostent_add_addr(h, &rr.rr.in_aaaa.addr6, 16) == -1)
				goto fail;
			break;
		}
	}

	return (h);
fail:
	free(h);
	return (NULL);
}

static struct hostent_ext *
hostent_alloc(int family)
{
	struct hostent_ext	*h;
	size_t			alloc;

	alloc = sizeof(*h) + 1024;
	if ((h = calloc(1, alloc)) == NULL)
		return (NULL);

	h->h.h_addrtype = family;
	h->h.h_length = (family == AF_INET) ? 4 : 16;
	h->h.h_aliases = h->aliases;
	h->h.h_addr_list = h->addrs;
	h->pos = (char *)(h) + sizeof(*h);
	h->end = h->pos + 1024;

	return (h);
}

static int
hostent_set_cname(struct hostent_ext *h, const char *name, int isdname)
{
	char	buf[MAXDNAME];
	size_t	n;

	if (h->h.h_name)
		return (-1);

	if (isdname) {
		asr_strdname(name, buf, sizeof buf);
		buf[strlen(buf) - 1] = '\0';
		if (!res_hnok(buf))
			return (-1);
		name = buf;
	}

	n = strlen(name) + 1;
	if (h->pos + n >= h->end)
		return (-1);

	h->h.h_name = h->pos;
	memmove(h->pos, name, n);
	h->pos += n;
	return (0);
}

static int
hostent_add_alias(struct hostent_ext *h, const char *name, int isdname)
{
	char	buf[MAXDNAME];
	size_t	i, n;

	for (i = 0; i < MAXALIASES; i++)
		if (h->aliases[i] == NULL)
			break;
	if (i == MAXALIASES)
		return (-1);

	if (isdname) {
		asr_strdname(name, buf, sizeof buf);
		buf[strlen(buf)-1] = '\0';
		if (!res_hnok(buf))
			return (-1);
		name = buf;
	}

	n = strlen(name) + 1;
	if (h->pos + n >= h->end)
		return (-1);

	h->aliases[i] = h->pos;
	memmove(h->pos, name, n);
	h->pos += n;
	return (0);
}

static int
hostent_add_addr(struct hostent_ext *h, const void *addr, size_t size)
{
	int	i;

	for (i = 0; i < MAXADDRS; i++)
		if (h->addrs[i] == NULL)
			break;
	if (i == MAXADDRS)
		return (-1);

	if (h->pos + size >= h->end)
		return (-1);

	h->addrs[i] = h->pos;
	memmove(h->pos, addr, size);
	h->pos += size;
	return (0);
}

#ifdef YP
static struct hostent_ext *
_yp_gethostnamadr(int type, const void *data)
{
	static char		*domain = NULL;
	struct hostent_ext	*h = NULL;
	const char		*name;
	char			 buf[MAXHOSTNAMELEN];
	char			*res = NULL;
	int			 r, len;

	if (!domain && _yp_check(&domain) == 0) {
		errno = 0; /* ignore yp_bind errors */
		return (NULL);
	}

	if (type == ASR_GETHOSTBYNAME) {
		name = data;
		len = strlen(name);
		r = yp_match(domain, "hosts.byname", name, len, &res, &len);
	}
	else {
		if (inet_ntop(AF_INET, data, buf, sizeof buf) == NULL)
			return (NULL);
		len = strlen(buf);
		r = yp_match(domain, "hosts.byaddr", buf, len, &res, &len);
	}
	if (r == 0) {
		h = hostent_from_yp(AF_INET, res);
	} else {
		errno = 0; /* ignore error if not found */
	}
	if (res)
		free(res);
	return (h);
}

static int
strsplit(char *line, char **tokens, int ntokens)
{
	int	ntok;
	char	*cp, **tp;

	for (cp = line, tp = tokens, ntok = 0;
	    ntok < ntokens && (*tp = strsep(&cp, " \t")) != NULL; )
		if (**tp != '\0') {
			tp++;
			ntok++;
		}

	return (ntok);
}

static struct hostent_ext *
hostent_from_yp(int family, char *line)
{
	struct hostent_ext	*h;
	char			*next, *tokens[10], addr[IN6ADDRSZ];
	int			 i, ntok;

	if ((h = hostent_alloc(family)) == NULL)
		return (NULL);

	for (next = line; line; line = next) {
		if ((next = strchr(line, '\n'))) {
			*next = '\0';
			next += 1;
		}
		ntok = strsplit(line, tokens, 10);
		if (ntok < 2)
			continue;
		if (inet_pton(family, tokens[0], addr) == 1)
			hostent_add_addr(h, addr, family == AF_INET ?
			    INADDRSZ : IN6ADDRSZ);
		i = 2;
		if (h->h.h_name == NULL)
			hostent_set_cname(h, tokens[1], 0);
		else if (strcmp(h->h.h_name, tokens[1]))
			i = 1;
		for (; i < ntok; i++)
			hostent_add_alias(h, tokens[i], 0);
	}

	if (h->h.h_name == NULL) {
		free(h);
		return (NULL);
	}

	return (h);
}
#endif
