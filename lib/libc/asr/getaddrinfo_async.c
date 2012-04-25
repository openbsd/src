/*	$OpenBSD: getaddrinfo_async.c,v 1.2 2012/04/25 20:28:25 eric Exp $	*/
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

struct match {
	int family;
	int socktype;
	int protocol;
};

static int getaddrinfo_async_run(struct async *, struct async_res *);
static int get_port(const char *, const char *, int);
static int iter_family(struct async *, int);
static int add_sockaddr(struct async *, struct sockaddr *, const char *);

static const struct match matches[] = {
	{ PF_INET,	SOCK_DGRAM,	IPPROTO_UDP	},
	{ PF_INET,	SOCK_STREAM,	IPPROTO_TCP	},
	{ PF_INET,	SOCK_RAW,	0		},
	{ PF_INET6,	SOCK_DGRAM,	IPPROTO_UDP	},
	{ PF_INET6,	SOCK_STREAM,	IPPROTO_TCP	},
	{ PF_INET6,	SOCK_RAW,	0		},
	{ -1, 		0, 		0, 		},
};

#define MATCH_FAMILY(a, b) ((a) == matches[(b)].family || (a) == PF_UNSPEC)
#define MATCH_PROTO(a, b) ((a) == matches[(b)].protocol || (a) == 0)
/* Do not match SOCK_RAW unless explicitely specified */
#define MATCH_SOCKTYPE(a, b) ((a) == matches[(b)].socktype || ((a) == 0 && \
				matches[(b)].socktype != SOCK_RAW))

struct async *
getaddrinfo_async(const char *hostname, const char *servname,
	const struct addrinfo *hints, struct asr *asr)
{
	struct asr_ctx	*ac;
	struct async	*as;

	ac = asr_use_resolver(asr);
	if ((as = async_new(ac, ASR_GETADDRINFO)) == NULL)
		goto abort; /* errno set */
	as->as_run = getaddrinfo_async_run;

	if (hostname && (as->as.ai.hostname = strdup(hostname)) == NULL)
		goto abort; /* errno set */
	if (servname && (as->as.ai.servname = strdup(servname)) == NULL)
		goto abort; /* errno set */
	if (hints)
		memmove(&as->as.ai.hints, hints, sizeof *hints);
	else {
		memset(&as->as.ai.hints, 0, sizeof as->as.ai.hints);
		as->as.ai.hints.ai_family = PF_UNSPEC;
	}

	asr_ctx_unref(ac);
	return (as);
    abort:
	if (as)
		async_free(as);
	asr_ctx_unref(ac);
	return (NULL);
}

static int
getaddrinfo_async_run(struct async *as, struct async_res *ar)
{
	const char	*str;
	struct addrinfo	*ai;
	int		 i, family, r;
	char		 fqdn[MAXDNAME];
	union {
		struct sockaddr		sa;
		struct sockaddr_in	sain;
		struct sockaddr_in6	sain6;
	} sa;

    next:
	switch(as->as_state) {

	case ASR_STATE_INIT:

		/*
		 * First, make sure the parameters are valid.
		 */

		as->as_count = 0;
		async_set_state(as, ASR_STATE_HALT);
		ar->ar_errno = 0;
		ar->ar_h_errno = NETDB_SUCCESS;
		ar->ar_gai_errno = 0;

		if (as->as.ai.hostname == NULL &&
		    as->as.ai.servname == NULL) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_NONAME;
			break;
		}

		ai = &as->as.ai.hints;

		if (ai->ai_addrlen ||
		    ai->ai_canonname ||
		    ai->ai_addr ||
		    ai->ai_next) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_BADHINTS;
			break;
		}

		if (ai->ai_flags & ~AI_MASK ||
		    (ai->ai_flags & AI_CANONNAME && ai->ai_flags & AI_FQDN)) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_BADFLAGS;
			break;
		}

		if (ai->ai_family != PF_UNSPEC &&
		    ai->ai_family != PF_INET &&
		    ai->ai_family != PF_INET6) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_FAMILY;
			break;
		}

		if (ai->ai_socktype &&
		    ai->ai_socktype != SOCK_DGRAM  &&
		    ai->ai_socktype != SOCK_STREAM &&
		    ai->ai_socktype != SOCK_RAW) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_SOCKTYPE;
			break;
		}

		if (ai->ai_protocol &&
		    ai->ai_protocol != IPPROTO_UDP  &&
		    ai->ai_protocol != IPPROTO_TCP) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_PROTOCOL;
			break;
		}

		if (ai->ai_socktype == SOCK_RAW &&
		    as->as.ai.servname != NULL) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_SERVICE;
			break;
		}

		/* Make sure there is at least a valid combination */
		for (i = 0; matches[i].family != -1; i++)
			if (MATCH_FAMILY(ai->ai_family, i) &&
			    MATCH_SOCKTYPE(ai->ai_socktype, i) &&
			    MATCH_PROTO(ai->ai_protocol, i))
				break;
		if (matches[i].family == -1) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_BADHINTS;
			break;
		}

		if (as->as.ai.servname) {
			as->as.ai.port_udp = get_port(as->as.ai.servname,
			    "udp", as->as.ai.hints.ai_flags & AI_NUMERICSERV);
			as->as.ai.port_tcp = get_port(as->as.ai.servname,
			    "tcp", as->as.ai.hints.ai_flags & AI_NUMERICSERV);
			if (as->as.ai.port_tcp < 0 || as->as.ai.port_udp < 0) {
				ar->ar_h_errno = NO_RECOVERY;
				ar->ar_gai_errno = EAI_SERVICE;
				break;
			}
		}

		/* If hostname is NULL, use local address */
		if (as->as.ai.hostname == NULL) {
			for(family = iter_family(as, 1);
			    family != -1;
			    family = iter_family(as, 0)) {
				/*
				 * We could use statically built sockaddrs for
				 * those, rather than parsing over and over.
				 */
				if (family == PF_INET)
					str = (ai->ai_flags & AI_PASSIVE) ? \
						"0.0.0.0" : "127.0.0.1";
				else /* PF_INET6 */
					str = (ai->ai_flags & AI_PASSIVE) ? \
						"::" : "::1";
				 /* This can't fail */
				sockaddr_from_str(&sa.sa, family, str);
				if ((r = add_sockaddr(as, &sa.sa, NULL))) {
					ar->ar_errno = errno;
					ar->ar_h_errno = NETDB_INTERNAL;
					ar->ar_gai_errno = r;
					async_set_state(as, ASR_STATE_HALT);
					break;
				}
			}
			if (ar->ar_gai_errno == 0 && as->as_count == 0) {
				ar->ar_h_errno = NO_DATA;
				ar->ar_gai_errno = EAI_NODATA;
			}
			break;
		}

		/* Try numeric addresses first */
		for(family = iter_family(as, 1);
		    family != -1;
		    family = iter_family(as, 0)) {

			if (sockaddr_from_str(&sa.sa, family,
					      as->as.ai.hostname) == -1)
				continue;

			if ((r = add_sockaddr(as, &sa.sa, NULL))) {
				ar->ar_errno = errno;
				ar->ar_h_errno = NETDB_INTERNAL;
				ar->ar_gai_errno = r;
				async_set_state(as, ASR_STATE_HALT);
				break;
			}

			async_set_state(as, ASR_STATE_HALT);
			break;
		}
		if (ar->ar_gai_errno || as->as_count)
			break;

		if (ai->ai_flags & AI_NUMERICHOST) {
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_FAIL;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		/* Starting domain lookup */
		async_set_state(as, ASR_STATE_SEARCH_DOMAIN);
		break;

	case ASR_STATE_SEARCH_DOMAIN:

		r = asr_iter_domain(as, as->as.ai.hostname, fqdn, sizeof(fqdn));
		if (r == -1) {
			async_set_state(as, ASR_STATE_NOT_FOUND);
			break;
		}
		if (r > (int)sizeof(fqdn)) {
			ar->ar_errno = EINVAL;
			ar->ar_h_errno = NO_RECOVERY;
			ar->ar_gai_errno = EAI_OVERFLOW;
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		/*
		 * Create a subquery to lookup the host addresses.
		 * We use the special hostaddr_async() API, which has the
		 * nice property of honoring the "lookup" and "family" keyword
		 * in the configuration, thus returning the right address
		 * families in the right order, and thus fixing the current
		 * getaddrinfo() feature documented in the BUGS section of
		 * resolver.conf(5).
		 */
		as->as.ai.subq = hostaddr_async_ctx(fqdn,
		    as->as.ai.hints.ai_family, as->as.ai.hints.ai_flags,
		    as->as_ctx);
		if (as->as.ai.subq == NULL) {
			ar->ar_errno = errno;
			if (errno == EINVAL) {
				ar->ar_h_errno = NO_RECOVERY;
				ar->ar_gai_errno = EAI_FAIL;
			} else {
				ar->ar_h_errno = NETDB_INTERNAL;
				ar->ar_gai_errno = EAI_MEMORY;
			}
			async_set_state(as, ASR_STATE_HALT);
			break;
		}
		async_set_state(as, ASR_STATE_LOOKUP_DOMAIN);
		break;

	case ASR_STATE_LOOKUP_DOMAIN:

		/* Run the subquery */
		if ((r = async_run(as->as.ai.subq, ar)) == ASYNC_COND)
			return (ASYNC_COND);

		/* Got one more address, use it to extend the result list. */
		if (r == ASYNC_YIELD) {
			if ((r = add_sockaddr(as, &ar->ar_sa.sa,
			    ar->ar_cname))) {
				ar->ar_errno = errno;
				ar->ar_h_errno = NETDB_INTERNAL;
				ar->ar_gai_errno = r;
				async_set_state(as, ASR_STATE_HALT);
			}
			if (ar->ar_cname)
				free(ar->ar_cname);
			break;
		}

		/*
		 * The subquery is done. Stop there if we have at least one
		 * answer.
		 */
		as->as.ai.subq = NULL;
		if (ar->ar_count) {
			async_set_state(as, ASR_STATE_HALT);
			break;
		}

		/*
		 * No anwser for this domain, but we might be suggested to
		 * try again later, so remember this. Then search the next
		 * domain.
		 */
		if (ar->ar_gai_errno == EAI_AGAIN)
			as->as.ai.flags |= ASYNC_AGAIN;
		async_set_state(as, ASR_STATE_SEARCH_DOMAIN);
		break;

	case ASR_STATE_NOT_FOUND:

		/*
		 * No result found. Maybe we can try again.
		 */
		ar->ar_errno = 0;
		if (as->as.ai.flags & ASYNC_AGAIN) {
			ar->ar_h_errno = TRY_AGAIN;
			ar->ar_gai_errno = EAI_AGAIN;
		} else {
			ar->ar_h_errno = NO_DATA;
			ar->ar_gai_errno = EAI_NODATA;
		}
		async_set_state(as, ASR_STATE_HALT);
		break;

	case ASR_STATE_HALT:

		/* Set the results. */

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
		ar->ar_h_errno = NETDB_INTERNAL;
		ar->ar_gai_errno = EAI_SYSTEM;
		async_set_state(as, ASR_STATE_HALT);
                break;
	}
	goto next;
}

/*
 * Retreive the port number for the service name "servname" and
 * the protocol "proto".
 */
static int
get_port(const char *servname, const char *proto, int numonly)
{
	struct servent		se;
	struct servent_data	sed;
	int			port, r;
	const char*		e;

	if (servname == NULL)
		return (0);

	e = NULL;
	port = strtonum(servname, 0, USHRT_MAX, &e);
	if (e == NULL)
		return (port);
	if (errno == ERANGE)
		return (-2); /* invalid */
	if (numonly)
		return (-2);

	memset(&sed, 0, sizeof(sed));
	r = getservbyname_r(servname, proto, &se, &sed);
	port = ntohs(se.s_port);
	endservent_r(&sed);

	if (r == -1)
		return (-1); /* not found */

	return (port);
}

/*
 * Iterate over the address families that are to be queried. Use the
 * list on the async context, unless a specific family was given in hints.
 */
static int
iter_family(struct async *as, int first)
{
	if (first) {
		as->as_family_idx = 0;
		if (as->as.ai.hints.ai_family != PF_UNSPEC)
			return as->as.ai.hints.ai_family;
		return AS_FAMILY(as);
	}

	if (as->as.ai.hints.ai_family != PF_UNSPEC)
		return (-1);

	as->as_family_idx++;

	return AS_FAMILY(as);
}

/*
 * Use the sockaddr at "sa" to extend the result list on the "as" context,
 * with the specified canonical name "cname". This function adds one
 * entry per protocol/socktype match.
 */
static int
add_sockaddr(struct async *as, struct sockaddr *sa, const char *cname)
{
	struct addrinfo		*ai;
	int			 i, port;

	for(i = 0; matches[i].family != -1; i++) {
		if (matches[i].family != sa->sa_family ||
		    !MATCH_SOCKTYPE(as->as.ai.hints.ai_socktype, i) ||
		    !MATCH_PROTO(as->as.ai.hints.ai_protocol, i))
			continue;

		if (matches[i].protocol == IPPROTO_TCP)
			port = as->as.ai.port_tcp;
		else if (matches[i].protocol == IPPROTO_UDP)
			port = as->as.ai.port_udp;
		else
			port = 0;

		ai = calloc(1, sizeof(*ai) + sa->sa_len);
		if (ai == NULL)
			return (EAI_MEMORY);
		ai->ai_family = sa->sa_family;
		ai->ai_socktype = matches[i].socktype;
		ai->ai_protocol = matches[i].protocol;
		ai->ai_addrlen = sa->sa_len;
		ai->ai_addr = (void*)(ai + 1);
		if (cname &&
		    as->as.ai.hints.ai_flags & (AI_CANONNAME | AI_FQDN)) {
			if ((ai->ai_canonname = strdup(cname)) == NULL) {
				free(ai);
				return (EAI_MEMORY);
			}
		}
		memmove(ai->ai_addr, sa, sa->sa_len);
		if (sa->sa_family == PF_INET)
			((struct sockaddr_in *)ai->ai_addr)->sin_port =
			    htons(port);
		else if (sa->sa_family == PF_INET6)
			((struct sockaddr_in6 *)ai->ai_addr)->sin6_port =
			    htons(port);

		if (as->as.ai.aifirst == NULL)
			as->as.ai.aifirst = ai;
		if (as->as.ai.ailast)
			as->as.ai.ailast->ai_next = ai;
		as->as.ai.ailast = ai;
		as->as_count += 1;
	}

	return (0);
}
