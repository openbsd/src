/*
 * %%% copyright-cmetz-96-bsd
 * Copyright (c) 1996-1999, Craig Metz, All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by Craig Metz and
 *      by other contributors.
 * 4. Neither the name of the author nor the names of contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/* getaddrinfo() v1.38 */

/*
   I'd like to thank Matti Aarnio for finding some bugs in this code and
   sending me patches, as well as everyone else who has contributed to this
   code (by reporting bugs, suggesting improvements, and commented on its
   behavior and proposed changes).
*/

#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <string.h>
#include <stdio.h>
#include <sys/utsname.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>

#define GAIH_OKIFUNSPEC	0x0100
#define GAIH_EAI	~(GAIH_OKIFUNSPEC)

static struct addrinfo nullreq = {
	0, PF_UNSPEC, 0, 0, 0, NULL, NULL, NULL
};

struct gaih_service {
	char   *name;
	int     num;
};

struct gaih_servtuple {
	struct gaih_servtuple *next;
	int     socktype;
	int     protocol;
	int     port;
};

static struct gaih_servtuple nullserv = {
	NULL, 0, 0, 0
};

struct gaih_addrtuple {
	struct gaih_addrtuple *next;
	int     family;
	char    addr[16];
	char   *cname;
};

static struct gaih_typeproto {
	int     socktype;
	int     protocol;
	char   *name;
} gaih_inet_typeproto[] = {
	{ 0, 0, NULL },
	{ SOCK_STREAM, IPPROTO_TCP, "tcp" },
	{ SOCK_DGRAM, IPPROTO_UDP, "udp" },
	{ 0, 0, NULL }
};

static int 
netdb_lookup_addr(const char *name, int af,
    const struct addrinfo *req, struct gaih_addrtuple **pat)
{
	int     rval = 0, herrno, i;
	char   *prevcname = NULL;
	struct hostent *h;

	h = gethostbyname2(name, af);
	herrno = h_errno;

	if (!h) {
		switch (herrno) {
		case NETDB_INTERNAL:
			return -EAI_SYSTEM;
		case HOST_NOT_FOUND:
			return 1;
		case TRY_AGAIN:
			return -EAI_AGAIN;
		case NO_RECOVERY:
			return -EAI_FAIL;
		case NO_DATA:
			return 1;
		default:
			return -EAI_FAIL;
		}
	}

	for (i = 0; h->h_addr_list[i]; i++) {
		while (*pat)
			pat = &((*pat)->next);

		if (!(*pat = malloc(sizeof(struct gaih_addrtuple))))
			return -EAI_MEMORY;
		memset(*pat, 0, sizeof(struct gaih_addrtuple));

		switch ((*pat)->family = af) {
		case AF_INET:
			memcpy((*pat)->addr, h->h_addr_list[i],
			    sizeof(struct in_addr));
			break;
		case AF_INET6:
			memcpy((*pat)->addr, h->h_addr_list[i],
			    sizeof(struct in6_addr));
			break;
		default:
			return -EAI_FAIL;
		}

		if (req->ai_flags & AI_CANONNAME) {
			if (prevcname && !strcmp(prevcname, h->h_name))
				(*pat)->cname = prevcname;
			else
				prevcname = (*pat)->cname = strdup(h->h_name);
		}
		pat = &((*pat)->next);
	}
	return rval;
}

static int 
gaih_local(const char *name, const struct gaih_service *service,
    const struct addrinfo *req, struct addrinfo **pai)
{
	struct utsname utsname;
	struct addrinfo *ai;
	struct sockaddr_un *sun;
	int siz;

	if (name || (req->ai_flags & AI_CANONNAME))
		if (uname(&utsname) < 0)
			return (-EAI_SYSTEM);

	if (name && strcmp(name, "localhost") && strcmp(name, "local") &&
	    strcmp(name, "unix") && strcmp(name, utsname.nodename))
		return (GAIH_OKIFUNSPEC | -EAI_NONAME);

	siz = sizeof(struct addrinfo) + sizeof(struct sockaddr_un);
	if (req->ai_flags & AI_CANONNAME)
		siz += strlen(utsname.nodename) + 1;

	if (!(ai = malloc(siz)))
		return -EAI_MEMORY;

	*pai = ai;
	ai->ai_next = NULL;
	ai->ai_flags = req->ai_flags;
	ai->ai_family = AF_LOCAL;
	ai->ai_socktype = req->ai_socktype ? req->ai_socktype : SOCK_STREAM;
	ai->ai_protocol = req->ai_protocol;
	ai->ai_addrlen = sizeof(struct sockaddr_un);
	ai->ai_addr = (void *)ai + sizeof(struct addrinfo);

	sun = (struct sockaddr_un *)ai->ai_addr;
	sun->sun_len = sizeof(struct sockaddr_un);
	sun->sun_family = AF_LOCAL;
	memset(&sun->sun_path, 0, sizeof sun->sun_path);

	if (service) {
		char   *c;

		c = strchr(service->name, '/');
		if (c) {
			if (strlen(service->name) >= sizeof(sun->sun_path))
				return (GAIH_OKIFUNSPEC | -EAI_SERVICE);
			strlcpy(sun->sun_path, service->name, sizeof (sun->sun_path));
		} else {
			if (strlen(P_tmpdir "/") + strlen(service->name) + 1 >=
			    sizeof(sun->sun_path))
				return(GAIH_OKIFUNSPEC | -EAI_SERVICE);
			snprintf(sun->sun_path, sizeof(sun->sun_path), "%s/%s",
			    P_tmpdir, service->name);
		}
	} else {
		extern char *_mktemp __P((char *));
		char	tmpn[MAXPATHLEN], *c;

		snprintf(tmpn, sizeof tmpn, "%stmp.XXXXXXXXXXXXX", P_tmpdir);
		if (!(c = _mktemp(tmpn)))
			return (GAIH_OKIFUNSPEC | -EAI_SYSTEM);
		strlcpy(sun->sun_path, c, sizeof(sun->sun_path));
	}

	ai->ai_canonname = NULL;
	if (req->ai_flags & AI_CANONNAME) {
		ai->ai_canonname = (void *)sun + sizeof(struct sockaddr_un);
		strlcpy(ai->ai_canonname, utsname.nodename,
		    strlen(utsname.nodename)+1);
	}
	return 0;
}

static int 
gaih_inet_serv(char *servicename, struct gaih_typeproto *tp,
    struct gaih_servtuple **st)
{
	struct servent *s;

	s = getservbyname(servicename, tp->name);
	if (!s)
		return (GAIH_OKIFUNSPEC | -EAI_SERVICE);

	if (!(*st = malloc(sizeof(struct gaih_servtuple))))
		return (-EAI_MEMORY);

	(*st)->next = NULL;
	(*st)->socktype = tp->socktype;
	(*st)->protocol = tp->protocol;
	(*st)->port = s->s_port;
	return (0);
}

static int 
gaih_inet(const char *name, const struct gaih_service *service,
    const struct addrinfo*req, struct addrinfo **pai)
{
	struct gaih_typeproto *tp = gaih_inet_typeproto;
	struct gaih_servtuple *st = &nullserv;
	struct gaih_addrtuple *at = NULL;
	char   *prevcname = NULL;
	struct gaih_servtuple *st2;
	struct gaih_addrtuple *at2 = at;
	struct addrinfo *ai = NULL;
	int     canonlen, addrlen, rval = 0;

	if (req->ai_protocol || req->ai_socktype) {
		for (tp++; tp->name &&
		    ((req->ai_socktype != tp->socktype) || !req->ai_socktype) &&
		    ((req->ai_protocol != tp->protocol) || !req->ai_protocol); tp++);
		if (!tp->name) {
			rval = GAIH_OKIFUNSPEC | -EAI_SERVICE;
			if (req->ai_socktype)
				rval = GAIH_OKIFUNSPEC | -EAI_SOCKTYPE;
			goto ret;
		}
	}
	if (service && (service->num < 0)) {
		if (tp->name) {
			rval = gaih_inet_serv(service->name, tp, &st);
			if (rval)
				goto ret;
		} else {
			struct gaih_servtuple **pst = &st;
			for (tp++; tp->name; tp++) {
				rval = gaih_inet_serv(service->name, tp, pst);
				if (rval) {
					if (rval & GAIH_OKIFUNSPEC)
						continue;
					goto ret;
				}
				pst = &((*pst)->next);
			}
			if (st == &nullserv) {
				rval = GAIH_OKIFUNSPEC | -EAI_SERVICE;
				goto ret;
			}
		}
	} else {
		if (!(st = malloc(sizeof(struct gaih_servtuple)))) {
			rval = -EAI_MEMORY;
			goto ret;
		}

		st->next = NULL;
		st->socktype = tp->socktype;
		st->protocol = tp->protocol;
		if (service)
			st->port = htons(service->num);
		else
			st->port = 0;
	}

	if (!name) {
		if (!(at = malloc(sizeof(struct gaih_addrtuple)))) {
			rval = -EAI_MEMORY;
			goto ret;
		}

		memset(at, 0, sizeof(struct gaih_addrtuple));

		if (req->ai_family)
			at->family = req->ai_family;
		else {
			if (!(at->next = malloc(sizeof(struct gaih_addrtuple)))) {
				rval = -EAI_MEMORY;
				goto ret;
			}

			at->family = AF_INET6;

			memset(at->next, 0, sizeof(struct gaih_addrtuple));
			at->next->family = AF_INET;
		}

		goto build;
	}

	if (!req->ai_family || (req->ai_family == AF_INET)) {
		struct in_addr in_addr;
		if (inet_pton(AF_INET, name, &in_addr) > 0) {
			if (!(at = malloc(sizeof(struct gaih_addrtuple)))) {
				rval = -EAI_MEMORY;
				goto ret;
			}

			memset(at, 0, sizeof(struct gaih_addrtuple));

			at->family = AF_INET;
			memcpy(at->addr, &in_addr, sizeof(struct in_addr));
			goto build;
		}
	}

	if (!req->ai_family || (req->ai_family == AF_INET6)) {
		struct in6_addr in6_addr;
		if (inet_pton(AF_INET6, name, &in6_addr) > 0) {
			if (!(at = malloc(sizeof(struct gaih_addrtuple)))) {
				rval = -EAI_MEMORY;
				goto ret;
			}

			memset(at, 0, sizeof(struct gaih_addrtuple));

			at->family = AF_INET6;
			memcpy(at->addr, &in6_addr, sizeof(struct in6_addr));
			goto build;
		}
	}

	if (!(req->ai_flags & AI_NUMERICHOST)) {
		if (!req->ai_family || (req->ai_family == AF_INET6))
			if ((rval = netdb_lookup_addr(name, AF_INET6, req, &at)) < 0)
				goto ret;
		if (!req->ai_family || (req->ai_family == AF_INET))
			if ((rval = netdb_lookup_addr(name, AF_INET, req, &at)) < 0)
				goto ret;

		if (!rval)
			goto build;
	}

	if (!at) {
		rval = GAIH_OKIFUNSPEC | -EAI_NONAME;
		goto ret;
	}

build:
	while (at2) {
		if (req->ai_flags & AI_CANONNAME) {
			if (at2->cname)
				canonlen = strlen(at2->cname) + 1;
			else
				if (name)
					canonlen = strlen(name) + 1;
				else
					canonlen = 2;
		} else
			canonlen = 0;

		if (at2->family == AF_INET6)
			addrlen = sizeof(struct sockaddr_in6);
		else
			addrlen = sizeof(struct sockaddr_in);

		st2 = st;
		while (st2) {
			if (!(ai = malloc(sizeof(struct addrinfo) +
			    addrlen + canonlen))) {
				rval = -EAI_MEMORY;
				goto ret;
			}

			*pai = ai;
			memset(ai, 0, sizeof(struct addrinfo) + addrlen + canonlen);

			ai->ai_flags = req->ai_flags;
			ai->ai_family = at2->family;
			ai->ai_socktype = st2->socktype;
			ai->ai_protocol = st2->protocol;
			ai->ai_addrlen = addrlen;
			ai->ai_addr = (void *)ai + sizeof(struct addrinfo);
			ai->ai_addr->sa_len = addrlen;
			ai->ai_addr->sa_family = at2->family;

			switch (at2->family) {
			case AF_INET6:
			    {
				struct sockaddr_in6 *sin6 = (void *)ai->ai_addr;

				memcpy(&sin6->sin6_addr, at2->addr,
				    sizeof(sin6->sin6_addr));
				sin6->sin6_port = st2->port;
				break;
			    }
			default:
			    {
				struct sockaddr_in *sin = (void *)ai->ai_addr;

				memcpy(&sin->sin_addr, at2->addr,
				    sizeof(sin->sin_addr));
				sin->sin_port = st2->port;
				break;
			    }
			}

			if (canonlen) {
				ai->ai_canonname = (void *) ai->ai_addr + addrlen;

				if (at2->cname) {
					strcpy(ai->ai_canonname, at2->cname);
					if (prevcname != at2->cname) {
						if (prevcname)
							free(prevcname);
						prevcname = at2->cname;
					}
				} else
					strcpy(ai->ai_canonname, name ? name : "*");
			}
			pai = &(ai->ai_next);
			st2 = st2->next;
		}
		at2 = at2->next;
	}
	rval = 0;

ret:
	if (st != &nullserv) {
		struct gaih_servtuple *st2 = st;
		while (st) {
			st2 = st->next;
			free(st);
			st = st2;
		}
	}
	if (at) {
		struct gaih_addrtuple *at2 = at;
		while (at) {
			at2 = at->next;
			free(at);
			at = at2;
		}
	}
	return rval;
}

static struct gaih {
	int     family;
	char   *name;
	int     (*gaih) __P((const char *name,
		    const struct gaih_service *service,
	            const struct addrinfo *req, struct addrinfo **pai));
} gaih[] = {
	{ PF_INET6, "inet6", gaih_inet },
	{ PF_INET, "inet", gaih_inet },
	{ PF_LOCAL, "local", gaih_local },
	{ -1, NULL, NULL}
};

int 
getaddrinfo(const char *name, const char *service,
    const struct addrinfo *req, struct addrinfo **pai)
{
	int     rval = EAI_SYSTEM;	/* XXX */
	int     j = 0;
	struct addrinfo *p = NULL, **end;
	struct gaih *g = gaih, *pg = NULL;
	struct gaih_service gaih_service, *pservice;

	if (name && (name[0] == '*') && !name[1])
		name = NULL;

	if (service && service[0] == '*' && service[1] == '\0')
		service = NULL;

	if (!req)
		req = &nullreq;

	if (req->ai_flags & ~(AI_CANONNAME | AI_PASSIVE | AI_NUMERICHOST | AI_EXT))
		return (EAI_BADFLAGS);

	if (service && *service) {
		char   *c;

		gaih_service.num = strtoul(service, &c, 10);
		if (*c)
			gaih_service.num = -1;
		gaih_service.name = (void *)service;
		pservice = &gaih_service;
	} else
		pservice = NULL;

	if (pai)
		end = &p;
	else
		end = NULL;

	while (g->gaih) {
		if ((req->ai_family == g->family) || !req->ai_family) {
			j++;
			if (!((pg && (pg->gaih == g->gaih)))) {
				pg = g;
				rval = g->gaih(name, pservice, req, end);
				if (rval) {
					if (!req->ai_family && (rval & GAIH_OKIFUNSPEC))
						continue;

					if (p)
						freeaddrinfo(p);

					return -(rval & GAIH_EAI);
				}
				if (end)
					while (*end)
						end = &((*end)->ai_next);
			}
		}
		g++;
	}

	if (!j)
		return (EAI_FAMILY);

	if (p) {
		*pai = p;
		return (0);
	}
	if (!pai && !rval)
		return (0);
	return EAI_NONAME;
}
