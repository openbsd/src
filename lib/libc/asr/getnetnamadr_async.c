/*	$OpenBSD: getnetnamadr_async.c,v 1.20 2015/05/29 08:49:37 eric Exp $	*/
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
#include <err.h>
#include <errno.h>
#include <resolv.h> /* for res_hnok */
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "asr_private.h"

#define MAXALIASES	16

struct netent_ext {
	struct netent	 n;
	char		*aliases[MAXALIASES + 1];
	char		*end;
	char		*pos;
};

static int getnetnamadr_async_run(struct asr_query *, struct asr_result *);
static struct netent_ext *netent_alloc(int);
static int netent_set_cname(struct netent_ext *, const char *, int);
static int netent_add_alias(struct netent_ext *, const char *, int);
static struct netent_ext *netent_file_match(FILE *, int, const char *);
static struct netent_ext *netent_from_packet(int, char *, size_t);

struct asr_query *
getnetbyname_async(const char *name, void *asr)
{
	struct asr_ctx	 *ac;
	struct asr_query *as;

	/* The current resolver segfaults. */
	if (name == NULL) {
		errno = EINVAL;
		return (NULL);
	}

	ac = asr_use_resolver(asr);
	if ((as = asr_async_new(ac, ASR_GETNETBYNAME)) == NULL)
		goto abort; /* errno set */
	as->as_run = getnetnamadr_async_run;

	as->as.netnamadr.family = AF_INET;
	as->as.netnamadr.name = strdup(name);
	if (as->as.netnamadr.name == NULL)
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
getnetbyaddr_async(in_addr_t net, int family, void *asr)
{
	struct asr_ctx	 *ac;
	struct asr_query *as;

	ac = asr_use_resolver(asr);
	if ((as = asr_async_new(ac, ASR_GETNETBYADDR)) == NULL)
		goto abort; /* errno set */
	as->as_run = getnetnamadr_async_run;

	as->as.netnamadr.family = family;
	as->as.netnamadr.addr = net;

	asr_ctx_unref(ac);
	return (as);

    abort:
	if (as)
		asr_async_free(as);
	asr_ctx_unref(ac);
	return (NULL);
}

static int
getnetnamadr_async_run(struct asr_query *as, struct asr_result *ar)
{
	struct netent_ext	*n;
	int			 r, type, saved_errno;
	FILE			*f;
	char			 dname[MAXDNAME], *name, *data;
	in_addr_t		 in;

    next:
	switch (as->as_state) {

	case ASR_STATE_INIT:

		if (as->as.netnamadr.family != AF_INET) {
			ar->ar_h_errno = NETDB_INTERNAL;
			ar->ar_errno = EAFNOSUPPORT;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		if (as->as_type == ASR_GETNETBYNAME &&
		    as->as.netnamadr.name[0] == '\0') {
			ar->ar_h_errno = NO_DATA;
			async_set_state(as, ASR_STATE_HALT);
			break;
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

			if (as->as_type == ASR_GETNETBYNAME) {
				type = T_A;
				/*
				 * I think we want to do the former, but our
				 * resolver is doing the following, so let's
				 * preserve bugward-compatibility there.
				 */
				type = T_PTR;
				name = as->as.netnamadr.name;
				as->as.netnamadr.subq = res_search_async_ctx(
				    name, C_IN, type, as->as_ctx);
			} else {
				type = T_PTR;
				name = dname;

				in = htonl(as->as.netnamadr.addr);
				asr_addr_as_fqdn((char *)&in,
				    as->as.netnamadr.family,
				    dname, sizeof(dname));
				as->as.netnamadr.subq = res_query_async_ctx(
				    name, C_IN, type, as->as_ctx);
			}

			if (as->as.netnamadr.subq == NULL) {
				ar->ar_errno = errno;
				ar->ar_h_errno = NETDB_INTERNAL;
				async_set_state(as, ASR_STATE_HALT);
			}
			async_set_state(as, ASR_STATE_SUBQUERY);
			break;

		case ASR_DB_FILE:

			if ((f = fopen(_PATH_NETWORKS, "re")) == NULL)
				break;

			if (as->as_type == ASR_GETNETBYNAME)
				data = as->as.netnamadr.name;
			else
				data = (void *)&as->as.netnamadr.addr;

			n = netent_file_match(f, as->as_type, data);
			saved_errno = errno;
			fclose(f);
			errno = saved_errno;
			if (n == NULL) {
				if (errno) {
					ar->ar_errno = errno;
					ar->ar_h_errno = NETDB_INTERNAL;
					async_set_state(as, ASR_STATE_HALT);
				}
				/* otherwise not found */
				break;
			}

			ar->ar_netent = &n->n;
			ar->ar_h_errno = NETDB_SUCCESS;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}
		break;

	case ASR_STATE_SUBQUERY:

		if ((r = asr_run(as->as.netnamadr.subq, ar)) == ASYNC_COND)
			return (ASYNC_COND);
		as->as.netnamadr.subq = NULL;

		if (ar->ar_datalen == -1) {
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		/* Got packet, but no answer */
		if (ar->ar_count == 0) {
			free(ar->ar_data);
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		n = netent_from_packet(as->as_type, ar->ar_data,
		    ar->ar_datalen);
		free(ar->ar_data);
		if (n == NULL) {
			ar->ar_errno = errno;
			ar->ar_h_errno = NETDB_INTERNAL;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		if (as->as_type == ASR_GETNETBYADDR)
			n->n.n_net = as->as.netnamadr.addr;

		/*
		 * No valid hostname or address found in the dns packet.
		 * Ignore it.
		 */
		if ((as->as_type == ASR_GETNETBYNAME && n->n.n_net == 0) ||
		    n->n.n_name == NULL) {
			free(n);
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		ar->ar_netent = &n->n;
		ar->ar_h_errno = NETDB_SUCCESS;
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_NOT_FOUND:

		ar->ar_errno = 0;
		ar->ar_h_errno = HOST_NOT_FOUND;
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_HALT:

		if (ar->ar_h_errno)
			ar->ar_netent = NULL;
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

static struct netent_ext *
netent_file_match(FILE *f, int reqtype, const char *data)
{
	struct netent_ext	*e;
	char			*tokens[MAXTOKEN], buf[BUFSIZ + 1];
	int			 n, i;
	in_addr_t		 net;

	for (;;) {
		n = asr_parse_namedb_line(f, tokens, MAXTOKEN, buf, sizeof(buf));
		if (n == -1) {
			errno = 0; /* ignore errors reading the file */
			return (NULL);
		}

		/* there must be an address and at least one name */
		if (n < 2)
			continue;

		if (reqtype == ASR_GETNETBYADDR) {
			net = inet_network(tokens[1]);
			if (memcmp(&net, data, sizeof net) == 0)
				goto found;
		} else {
			for (i = 0; i < n; i++) {
				if (i == 1)
					continue;
				if (strcasecmp(data, tokens[i]))
					continue;
				goto found;
			}
		}
	}

found:
	if ((e = netent_alloc(AF_INET)) == NULL)
		return (NULL);
	if (netent_set_cname(e, tokens[0], 0) == -1)
		goto fail;
	for (i = 2; i < n; i ++)
		if (netent_add_alias(e, tokens[i], 0) == -1)
			goto fail;
	e->n.n_net = inet_network(tokens[1]);
	return (e);
fail:
	free(e);
	return (NULL);
}

static struct netent_ext *
netent_from_packet(int reqtype, char *pkt, size_t pktlen)
{
	struct netent_ext	*n;
	struct asr_unpack	 p;
	struct asr_dns_header	 hdr;
	struct asr_dns_query	 q;
	struct asr_dns_rr	 rr;

	if ((n = netent_alloc(AF_INET)) == NULL)
		return (NULL);

	asr_unpack_init(&p, pkt, pktlen);
	asr_unpack_header(&p, &hdr);
	for (; hdr.qdcount; hdr.qdcount--)
		asr_unpack_query(&p, &q);
	for (; hdr.ancount; hdr.ancount--) {
		asr_unpack_rr(&p, &rr);
		if (rr.rr_class != C_IN)
			continue;
		switch (rr.rr_type) {
		case T_CNAME:
			if (reqtype == ASR_GETNETBYNAME) {
				if (netent_add_alias(n, rr.rr_dname, 1) == -1)
					goto fail;
			} else {
				if (netent_set_cname(n, rr.rr_dname, 1) == -1)
					goto fail;
			}
			break;

		case T_PTR:
			if (reqtype != ASR_GETNETBYADDR)
				continue;
			if (netent_set_cname(n, rr.rr.ptr.ptrname, 1) == -1)
				goto fail;
			/* XXX See if we need to have MULTI_PTRS_ARE_ALIASES */
			break;

		case T_A:
			if (n->n.n_addrtype != AF_INET)
				break;
			if (netent_set_cname(n, rr.rr_dname, 1) ==  -1)
				goto fail;
			n->n.n_net = ntohl(rr.rr.in_a.addr.s_addr);
			break;
		}
	}

	return (n);
fail:
	free(n);
	return (NULL);
}

static struct netent_ext *
netent_alloc(int family)
{
	struct netent_ext	*n;
	size_t			 alloc;

	alloc = sizeof(*n) + 1024;
	if ((n = calloc(1, alloc)) == NULL)
		return (NULL);

	n->n.n_addrtype = family;
	n->n.n_aliases = n->aliases;
	n->pos = (char *)(n) + sizeof(*n);
	n->end = n->pos + 1024;

	return (n);
}

static int
netent_set_cname(struct netent_ext *n, const char *name, int isdname)
{
	char	buf[MAXDNAME];
	size_t	l;

	if (n->n.n_name)
		return (-1);

	if (isdname) {
		asr_strdname(name, buf, sizeof buf);
		buf[strlen(buf) - 1] = '\0';
		if (!res_hnok(buf))
			return (-1);
		name = buf;
	}

	l = strlen(name) + 1;
	if (n->pos + l >= n->end)
		return (-1);

	n->n.n_name = n->pos;
	memmove(n->pos, name, l);
	n->pos += l;

	return (0);
}

static int
netent_add_alias(struct netent_ext *n, const char *name, int isdname)
{
	char	buf[MAXDNAME];
	size_t	i, l;

	for (i = 0; i < MAXALIASES; i++)
		if (n->aliases[i] == NULL)
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

	l = strlen(name) + 1;
	if (n->pos + l >= n->end)
		return (-1);

	n->aliases[i] = n->pos;
	memmove(n->pos, name, l);
	n->pos += l;
	return (0);
}
