/*	$OpenBSD: hostaddr_async.c,v 1.3 2012/09/05 15:56:13 eric Exp $	*/
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
static int addrinfo_add(struct async *, int, const void *, size_t, const char *);
static int addrinfo_from_file(struct async *, int,  FILE *);
static int addrinfo_from_pkt(struct async *, char *, size_t);

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

	as->as.ai.hints.ai_flags = flags;
	as->as.ai.hints.ai_family = family;
	as->as.ai.hostname = strdup(buf);
	if (as->as.ai.hostname == NULL)
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
	char		 *name;
	int		 n, family, type, r;
	FILE		*file;

    next:
	switch(as->as_state) {

	case ASR_STATE_INIT:

		if (as->as.ai.hints.ai_family != AF_INET &&
		    as->as.ai.hints.ai_family != AF_INET6 &&
		    as->as.ai.hints.ai_family != AF_UNSPEC) {
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
		if (as->as.ai.hints.ai_family != AF_UNSPEC ||
		    AS_FAMILY(as) == -1) {
			/* The family was specified, or we have tried all
			 * families with this DB.
			 */
			if (as->as_count) {
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
		/* query the current DB again. */
		switch(AS_DB(as)) {
		case ASR_DB_DNS:
			family = (as->as.ai.hints.ai_family == AF_UNSPEC) ?
			    AS_FAMILY(as) : as->as.ai.hints.ai_family;
			type = (family == AF_INET6) ? T_AAAA : T_A;
			name = as->as.ai.hostname;
			as->as.ai.subq = res_query_async_ctx(name, C_IN,
			    type, NULL, 0, as->as_ctx);
			if (as->as.ai.subq == NULL) {
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
			file = fopen(as->as_ctx->ac_hostfile, "r");
			if (file == NULL) {
				async_set_state(as, ASR_STATE_NEXT_DB);
				break;
			}
			family = (as->as.ai.hints.ai_family == AF_UNSPEC) ?
			    AS_FAMILY(as) : as->as.ai.hints.ai_family;

			n = addrinfo_from_file(as, family, file);
			if (n == -1) {
				if (errno == ENOMEM)
					ar->ar_gai_errno = EAI_MEMORY;
				else
					ar->ar_gai_errno = EAI_FAIL;
				async_set_state(as, ASR_STATE_HALT);
			} else
				async_set_state(as, ASR_STATE_NEXT_FAMILY);
			fclose(file);
			break;

		default:
			async_set_state(as, ASR_STATE_NEXT_DB);
		}
		break;

	case ASR_STATE_SUBQUERY:
		if ((r = async_run(as->as.ai.subq, ar)) == ASYNC_COND)
			return (ASYNC_COND);
		as->as.ai.subq = NULL;

		if (ar->ar_datalen == -1) {
			async_set_state(as, ASR_STATE_NEXT_DB);
			break;
		}

		r = addrinfo_from_pkt(as, ar->ar_data, ar->ar_datalen);
		if (r == -1) {
			if (errno == ENOMEM)
				ar->ar_gai_errno = EAI_MEMORY;
			else
				ar->ar_gai_errno = EAI_FAIL;
			async_set_state(as, ASR_STATE_HALT);
		} else
			async_set_state(as, ASR_STATE_NEXT_FAMILY);
		free(ar->ar_data);
		break;

	case ASR_STATE_NOT_FOUND:
		/* XXX the exact error depends on what query/send returned */
		ar->ar_gai_errno = EAI_NODATA;
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_HALT:
		if (ar->ar_gai_errno == 0) {
			ar->ar_count = as->as_count;
			ar->ar_addrinfo = as->as.ai.aifirst;
			as->as.ai.aifirst = NULL;
		} else {
			ar->ar_count = 0;
			ar->ar_addrinfo = NULL;
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
addrinfo_add(struct async *as, int family, const void *addr, size_t addrlen,
    const char *name)
{
	struct addrinfo	*ai;

	if ((ai = calloc(1, sizeof (*ai) + addrlen)) == NULL)
		return (-1);

	ai->ai_family = family;
	ai->ai_addrlen = addrlen;
	ai->ai_addr = (void*)(ai + 1);
	if (name && (ai->ai_canonname = strdup(name)) == NULL) {
		free(ai);
		return (-1);
	}
	memmove(ai->ai_addr, addr, addrlen);

	if (!as->as.ai.aifirst)
		as->as.ai.aifirst = ai;
	if (as->as.ai.ailast)
		as->as.ai.ailast->ai_next = ai;
	as->as.ai.ailast = ai;
	as->as_count += 1;

	return (0);
}

static int
addrinfo_from_file(struct async *as, int family, FILE *f)
{
	char		*tokens[MAXTOKEN], *c;
	int		 n, i;
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sain;
		struct sockaddr_in6	sain6;
	} u;

	for(;;) {
		n = asr_parse_namedb_line(f, tokens, MAXTOKEN);
		if (n == -1)
			break; /* ignore errors reading the file */

		for (i = 1; i < n; i++) {
			if (strcasecmp(as->as.ai.hostname, tokens[i]))
				continue;
			if (sockaddr_from_str(&u.sa, family, tokens[0]) == -1)
				continue;
			break;
		}
		if (i == n)
			continue;

		if (as->as.ai.hints.ai_flags & AI_CANONNAME)
			c = tokens[1];
		else if (as->as.ai.hints.ai_flags & AI_FQDN)
			c = as->as.ai.hostname;
		else
			c = NULL;

		if (addrinfo_add(as, u.sa.sa_family, &u.sa, u.sa.sa_len, c))
			return (-1); /* errno set */
	}
	return (0);
}

static int
addrinfo_from_pkt(struct async *as, char *pkt, size_t pktlen)
{
	struct packed	 p;
	struct header	 h;
	struct query	 q;
	struct rr	 rr;
	int		 i;
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sain;
		struct sockaddr_in6	sain6;
	} u;
	char		 buf[MAXDNAME], *c;

	packed_init(&p, pkt, pktlen);
	unpack_header(&p, &h);
	for(; h.qdcount; h.qdcount--)
		unpack_query(&p, &q);

	for (i = 0; i < h.ancount; i++) {
		unpack_rr(&p, &rr);
		if (rr.rr_type != q.q_type ||
		    rr.rr_class != q.q_class)
			continue;

		memset(&u, 0, sizeof u);
		if (rr.rr_type == T_A) {
			u.sain.sin_len = sizeof u.sain;
			u.sain.sin_family = AF_INET;
			u.sain.sin_addr = rr.rr.in_a.addr;
			u.sain.sin_port = 0;
		} else if (rr.rr_type == T_AAAA) {
			u.sain6.sin6_len = sizeof u.sain6;
			u.sain6.sin6_family = AF_INET6;
 			u.sain6.sin6_addr = rr.rr.in_aaaa.addr6;
			u.sain6.sin6_port = 0;
		} else
			continue;

		if (as->as.ai.hints.ai_flags & AI_CANONNAME) {
			asr_strdname(rr.rr_dname, buf, sizeof buf);
			buf[strlen(buf) - 1] = '\0';
			c = buf;
		} else if (as->as.ai.hints.ai_flags & AI_FQDN)
			c = as->as.ai.hostname;
		else
			c = NULL;

		if (addrinfo_add(as, u.sa.sa_family, &u.sa, u.sa.sa_len, c))
			return (-1); /* errno set */
	}
	return (0);
}
