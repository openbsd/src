/*	$OpenBSD: config.c,v 1.14 2004/08/30 12:02:59 henning Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/stat.h>

#include <netinet/in.h>
#include <arpa/nameser.h>

#include <errno.h>
#include <resolv.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"

struct ntp_addr	*host_v4(const char *);
struct ntp_addr	*host_v6(const char *);

static u_int32_t		 maxid = 0;

int
host(const char *s, struct ntp_addr **hn)
{
	struct ntp_addr	*h = NULL;

	if (!strcmp(s, "*"))
		if ((h = calloc(1, sizeof(struct ntp_addr))) == NULL)
			fatal(NULL);

	/* IPv4 address? */
	if (h == NULL)
		h = host_v4(s);

	/* IPv6 address? */
	if (h == NULL)
		h = host_v6(s);

	*hn = h;
	if (h == NULL)
		return (0);
	return (1);
}

struct ntp_addr	*
host_v4(const char *s)
{
	struct in_addr		 ina;
	struct sockaddr_in	*sa_in;
	struct ntp_addr		*h;

	bzero(&ina, sizeof(struct in_addr));
	if (inet_pton(AF_INET, s, &ina) != 1)
		return (NULL);

	if ((h = calloc(1, sizeof(struct ntp_addr))) == NULL)
		fatal(NULL);
	sa_in = (struct sockaddr_in *)&h->ss;
	sa_in->sin_len = sizeof(struct sockaddr_in);
	sa_in->sin_family = AF_INET;
	sa_in->sin_addr.s_addr = ina.s_addr;

	return (h);
}

struct ntp_addr	*
host_v6(const char *s)
{
	struct addrinfo		 hints, *res;
	struct sockaddr_in6	*sa_in6;
	struct ntp_addr		*h = NULL;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(s, "0", &hints, &res) == 0) {
		if ((h = calloc(1, sizeof(struct ntp_addr))) == NULL)
			fatal(NULL);
		sa_in6 = (struct sockaddr_in6 *)&h->ss;
		sa_in6->sin6_len = sizeof(struct sockaddr_in6);
		sa_in6->sin6_family = AF_INET6;
		memcpy(&sa_in6->sin6_addr,
		    &((struct sockaddr_in6 *)res->ai_addr)->sin6_addr,
		    sizeof(sa_in6->sin6_addr));
		sa_in6->sin6_scope_id =
		    ((struct sockaddr_in6 *)res->ai_addr)->sin6_scope_id;

		freeaddrinfo(res);
	}

	return (h);
}

int
host_dns(const char *s, struct ntp_addr **hn)
{
	struct addrinfo		 hints, *res0, *res;
	int			 error, cnt = 0;
	struct sockaddr_in	*sa_in;
	struct sockaddr_in6	*sa_in6;
	struct ntp_addr		*h, *hh = NULL;

	bzero(&hints, sizeof(hints));
	hints.ai_family = PF_UNSPEC;
	hints.ai_socktype = SOCK_DGRAM; /* DUMMY */
	res_init();	/* XXX */
	error = getaddrinfo(s, NULL, &hints, &res0);
	if (error) {
		log_warnx("could not parse \"%s\": %s", s,
		    gai_strerror(error));
		if (error == EAI_AGAIN || error == EAI_NODATA ||
		    error == EAI_NONAME)
			return (0);
		else
			return (-1);
	}

	for (res = res0; res; res = res->ai_next) {
		if (res->ai_family != AF_INET &&
		    res->ai_family != AF_INET6)
			continue;
		if ((h = calloc(1, sizeof(struct ntp_addr))) == NULL)
			fatal(NULL);
		h->ss.ss_family = res->ai_family;
		if (res->ai_family == AF_INET) {
			sa_in = (struct sockaddr_in *)&h->ss;
			sa_in->sin_len = sizeof(struct sockaddr_in);
			sa_in->sin_addr.s_addr = ((struct sockaddr_in *)
			    res->ai_addr)->sin_addr.s_addr;
		} else {
			sa_in6 = (struct sockaddr_in6 *)&h->ss;
			sa_in6->sin6_len = sizeof(struct sockaddr_in6);
			memcpy(&sa_in6->sin6_addr, &((struct sockaddr_in6 *)
			    res->ai_addr)->sin6_addr, sizeof(struct in6_addr));
		}

		h->next = hh;
		hh = h;
		cnt++;
	}
	freeaddrinfo(res0);

	*hn = hh;
	return (cnt);
}

struct ntp_peer *
new_peer(void)
{
	struct ntp_peer	*p;

	if ((p = calloc(1, sizeof(struct ntp_peer))) == NULL)
		fatal("conf_main server calloc");
	p->id = ++maxid;

	return (p);
}
