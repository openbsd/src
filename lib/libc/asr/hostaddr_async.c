/*	$OpenBSD: hostaddr_async.c,v 1.1 2012/04/14 09:24:18 eric Exp $	*/
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

#include "asr.h"
#include "asr_private.h"

static int hostaddr_async_run(struct async *, struct async_res *);
static int sockaddr_from_rr(struct sockaddr *, struct rr *);

/*
 * This API function allows to iterate over host addresses, for the given
 * family which, must be AF_INET, AF_INET6, or AF_UNSPEC (in which case
 * the family lookup list from the resolver is used). The strategy is to
 * return all addresses found for the first DB that returned at least one
 * address.
 *
 * Flags can be 0, or one of AI_CANONNAME or AI_FQDN. If set, the
 * canonical name will be returned along with the address.
 */
struct async *
hostaddr_async_ctx(const char *name, int family, int flags, struct asr_ctx *ac)
{
	struct async	*as;
	char		 buf[MAXDNAME];

#ifdef DEBUG
	asr_printf("asr: hostaddr_async_ctx(\"%s\", %i)\n", name, family);
#endif
	if (asr_domcat(name, NULL, buf, sizeof buf) == 0) {
		errno = EINVAL;
		return (NULL);
	}

	if ((as = async_new(ac, ASR_HOSTADDR)) == NULL)
		goto err; /* errno set */
	as->as_run = hostaddr_async_run;

	as->as.host.aiflags = flags;
	as->as.host.family = family;
	as->as.host.name = strdup(buf);
	if (as->as.host.name == NULL)
		goto err; /* errno set */

	return (as);
    err:
	if (as)
		async_free(as);
	return (NULL);
}

static int
hostaddr_async_run(struct async *as, struct async_res *ar)
{
	struct packed	 p;
	struct header	 h;
	struct query	 q;
	struct rr	 rr;
	char		 buf[MAXDNAME], *c, *name;
	int		 i, n, family, type, r;

    next:
	switch(as->as_state) {

	case ASR_STATE_INIT:

		if (as->as.host.family != AF_INET &&
		    as->as.host.family != AF_INET6 &&
		    as->as.host.family != AF_UNSPEC) {
			ar->ar_gai_errno = EAI_FAMILY;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		as->as_count = 0;
		as->as_db_idx = 0;
		async_set_state(as, ASR_STATE_NEXT_DB);
		break;

	case ASR_STATE_NEXT_FAMILY:

		as->as_family_idx += 1;
		if (as->as.host.family != AF_UNSPEC || AS_FAMILY(as) == -1) {
			/* The family was specified, or we have tried all
			 * families with this DB.
			 */
			if (as->as_count) {
				ar->ar_errno = 0;
				ar->ar_gai_errno = 0;
				async_set_state(as, ASR_STATE_HALT);
			} else
				async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		async_set_state(as, ASR_STATE_LOOKUP_FAMILY);
		break;

	case ASR_STATE_LOOKUP_FAMILY:

		async_set_state(as, ASR_STATE_SAME_DB);
		break;

	case ASR_STATE_NEXT_DB:

		if (asr_iter_db(as) == -1) {
			async_set_state(as, ASR_STATE_NOT_FOUND);
			break;
		}
		as->as_family_idx = 0;
		/* FALLTHROUGH */

	case ASR_STATE_SAME_DB:

		/* Query the current DB again. */

		switch(AS_DB(as)) {
		case ASR_DB_DNS:

			family = (as->as.host.family == AF_UNSPEC) ?
			    AS_FAMILY(as) : as->as.host.family;
			type = (family == AF_INET6) ? T_AAAA : T_A;
			name = as->as.host.name;
			as->as.host.subq = res_query_async_ctx(name, C_IN,
			    type, NULL, 0, as->as_ctx);
			if (as->as.host.subq == NULL) {
				ar->ar_errno = errno;
				ar->ar_h_errno = NETDB_INTERNAL;
				if (errno == ENOMEM)
					ar->ar_gai_errno = EAI_MEMORY;
				else
					ar->ar_gai_errno = EAI_FAIL;
				async_set_state(as, ASR_STATE_HALT);
				break;
			}
			async_set_state(as, ASR_STATE_SUBQUERY);
			break;

		case ASR_DB_FILE:

			as->as.host.file = fopen(as->as_ctx->ac_hostfile, "r");
			if (as->as.host.file == NULL)
				async_set_state(as, ASR_STATE_NEXT_DB);
			else
				async_set_state(as, ASR_STATE_READ_FILE);
			break;

		default:
			async_set_state(as, ASR_STATE_NEXT_DB);
		}
		break;

	case ASR_STATE_SUBQUERY:
		if ((r = async_run(as->as.host.subq, ar)) == ASYNC_COND)
			return (ASYNC_COND);
		as->as.host.subq = NULL;

		if (ar->ar_datalen == -1) {
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		as->as.host.pkt = ar->ar_data;
		as->as.host.pktlen = ar->ar_datalen;
		packed_init(&p, as->as.host.pkt, as->as.host.pktlen);
		unpack_header(&p, &h);
		for(; h.qdcount; h.qdcount--)
			unpack_query(&p, &q);
		as->as.host.pktpos = p.offset;
		as->as.host.ancount = h.ancount;
		as->as.host.class = q.q_class;
		as->as.host.type = q.q_type;
		async_set_state(as, ASR_STATE_READ_RR);
		break;

	case ASR_STATE_READ_RR:

		/* When done with this NS, try with next family */
		if (as->as.host.ancount == 0) {
			free(as->as.host.pkt);
			as->as.host.pkt = NULL;
			async_set_state(as, ASR_STATE_NEXT_FAMILY);
			break;
		}

		/* Continue reading the packet where we left it. */
		packed_init(&p, as->as.host.pkt, as->as.host.pktlen);
		p.offset = as->as.host.pktpos;
		unpack_rr(&p, &rr);
		as->as.host.pktpos = p.offset;
		as->as.host.ancount -= 1;
		if (rr.rr_type == as->as.host.type &&
		    rr.rr_class == as->as.host.class) {
			as->as_count += 1;
			ar->ar_count = as->as_count;
			sockaddr_from_rr(&ar->ar_sa.sa, &rr);
			if (as->as.host.aiflags & AI_CANONNAME)
				c = asr_strdname(rr.rr_dname, buf,
				    sizeof buf);
			else if (as->as.host.aiflags & AI_FQDN) {
				strlcpy(buf, as->as.host.name, sizeof buf);
				c = buf;
			} else
				c = NULL;
			if (c) {
				if (c[strlen(c) - 1] == '.')
					c[strlen(c) - 1] = '\0';
				ar->ar_cname = strdup(c);
			} else
				ar->ar_cname = NULL;
			return (ASYNC_YIELD);
		}
		break;

	case ASR_STATE_READ_FILE:

		/* When done with the file, try next family. */
		n = asr_parse_namedb_line(as->as.host.file, as->as.host.tokens,
		    MAXTOKEN);
		if (n == -1) {
			fclose(as->as.host.file);
			as->as.host.file = NULL;
			async_set_state(as, ASR_STATE_NEXT_FAMILY);
			break;
		}

		for (i = 1; i < n; i++) {
			if (strcasecmp(as->as.host.name,
			    as->as.host.tokens[i]))
				continue;

			family = as->as.host.family;
			if (family == AF_UNSPEC)
				family = AS_FAMILY(as);

			if (sockaddr_from_str(&ar->ar_sa.sa, family,
			    as->as.host.tokens[0]) == -1)
				continue;

			if (as->as.host.aiflags & AI_CANONNAME) {
				strlcpy(buf, as->as.host.tokens[1],
				    sizeof buf);
				c = buf;
			} else if (as->as.host.aiflags & AI_FQDN) {
				strlcpy(buf, as->as.host.name, sizeof buf);
				c = buf;
			} else
				c = NULL;
			if (c) {
				if (c[strlen(c) - 1] == '.')
					c[strlen(c) - 1] = '\0';
				ar->ar_cname = strdup(c);
			} else
				ar->ar_cname = NULL;
			as->as_count += 1;
			ar->ar_count = as->as_count;
			return (ASYNC_YIELD);
		}
		break;

	case ASR_STATE_NOT_FOUND:
		/* XXX the exact error depends on what query/send returned */
		ar->ar_errno = 0;
		ar->ar_gai_errno = EAI_NODATA;
		async_set_state(as, ASR_STATE_HALT);
		break;
	
	case ASR_STATE_HALT:

		ar->ar_count = as->as_count;
		if (ar->ar_count) {
			ar->ar_errno = 0;
			ar->ar_gai_errno = 0;
		}
		return (ASYNC_DONE);

	default:
		ar->ar_errno = EOPNOTSUPP;
		ar->ar_gai_errno = EAI_SYSTEM;
		async_set_state(as, ASR_STATE_HALT);
		break;
	}
	goto next;
}

static int
sockaddr_from_rr(struct sockaddr *sa, struct rr *rr)
{
	struct sockaddr_in	*sin;
	struct sockaddr_in6	*sin6;

	if (rr->rr_class != C_IN)
		return (-1);

	switch (rr->rr_type) {
	case T_A:
		sin = (struct sockaddr_in*)sa;
		memset(sin, 0, sizeof *sin);
		sin->sin_len = sizeof *sin;
		sin->sin_family = PF_INET;
		sin->sin_addr = rr->rr.in_a.addr;
		sin->sin_port = 0;
		return (0);
	case T_AAAA:
		sin6 = (struct sockaddr_in6*)sa;
		memset(sin6, 0, sizeof *sin6);
		sin6->sin6_len = sizeof *sin6;
		sin6->sin6_family = PF_INET6;
		sin6->sin6_addr = rr->rr.in_aaaa.addr6;
		sin6->sin6_port = 0;
		return (0);

	default:
		break;
	}

	return (-1);
}
