/*	$OpenBSD: config.c,v 1.61 2015/07/16 18:26:04 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004, 2005 Henning Brauer <henning@openbsd.org>
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
#include <sys/mman.h>
#include <sys/ioctl.h>

#include <netmpls/mpls.h>

#include <errno.h>
#include <ifaddrs.h>
#include <netdb.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "session.h"

u_int32_t	get_bgpid(void);
int		host_v4(const char *, struct bgpd_addr *, u_int8_t *);
int		host_v6(const char *, struct bgpd_addr *);

struct bgpd_config *
new_config(void)
{
	struct bgpd_config *conf;

	if ((conf = calloc(1, sizeof(struct bgpd_config))) == NULL)
		fatal(NULL);

	conf->min_holdtime = MIN_HOLDTIME;
	conf->bgpid = get_bgpid();
	conf->fib_priority = RTP_BGP;

	if ((conf->csock = strdup(SOCKET_NAME)) == NULL)
		fatal(NULL);

	if ((conf->filters = calloc(1, sizeof(struct filter_head))) == NULL)
		fatal(NULL);
	if ((conf->listen_addrs = calloc(1, sizeof(struct listen_addrs))) ==
	    NULL)
		fatal(NULL);
	if ((conf->mrt = calloc(1, sizeof(struct mrt_head))) == NULL)
		fatal(NULL);

	/* init the various list for later */
	TAILQ_INIT(&conf->networks);
	SIMPLEQ_INIT(&conf->rdomains);

	TAILQ_INIT(conf->filters);
	TAILQ_INIT(conf->listen_addrs);
	LIST_INIT(conf->mrt);

	return (conf);
}

void
free_config(struct bgpd_config *conf)
{
	struct rdomain		*rd;
	struct network		*n;
	struct listen_addr	*la;
	struct mrt		*m;

	while ((rd = SIMPLEQ_FIRST(&conf->rdomains)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&conf->rdomains, entry);
		filterset_free(&rd->export);
		filterset_free(&rd->import);

		while ((n = TAILQ_FIRST(&rd->net_l)) != NULL) {
			TAILQ_REMOVE(&rd->net_l, n, entry);
			filterset_free(&n->net.attrset);
			free(n);
		}
		free(rd);
	}

	while ((n = TAILQ_FIRST(&conf->networks)) != NULL) {
		TAILQ_REMOVE(&conf->networks, n, entry);
		filterset_free(&n->net.attrset);
		free(n);
	}

	filterlist_free(conf->filters);

	while ((la = TAILQ_FIRST(conf->listen_addrs)) != NULL) {
		TAILQ_REMOVE(conf->listen_addrs, la, entry);
		free(la);
	}
	free(conf->listen_addrs);

	while ((m = LIST_FIRST(conf->mrt)) != NULL) {
		LIST_REMOVE(m, entry);
		free(m);
	}
	free(conf->mrt);

	free(conf->csock);
	free(conf->rcsock);

	free(conf);
}

int
merge_config(struct bgpd_config *xconf, struct bgpd_config *conf,
    struct peer *peer_l)
{
	struct listen_addr	*nla, *ola, *next;
	struct network		*n;

	/*
	 * merge the freshly parsed conf into the running xconf
	 */

	if (!conf->as) {
		log_warnx("configuration error: AS not given");
		return (1);
	}


	if ((conf->flags & BGPD_FLAG_REFLECTOR) && conf->clusterid == 0)
		conf->clusterid = conf->bgpid;



	/* adjust FIB priority if changed */
	/* if xconf is uninitalized we get RTP_NONE */
	if (xconf->fib_priority != conf->fib_priority) {
		kr_fib_decouple_all(xconf->fib_priority);
		kr_fib_update_prio_all(conf->fib_priority);
		kr_fib_couple_all(conf->fib_priority);
	}

	/* take over the easy config changes */
	xconf->flags = conf->flags;
	xconf->log = conf->log;
	xconf->bgpid = conf->bgpid;
	xconf->clusterid = conf->clusterid;
	xconf->as = conf->as;
	xconf->short_as = conf->short_as;
	xconf->holdtime = conf->holdtime;
	xconf->min_holdtime = conf->min_holdtime;
	xconf->connectretry = conf->connectretry;
	xconf->fib_priority = conf->fib_priority;

	/* clear old control sockets and use new */
	free(xconf->csock);
	free(xconf->rcsock);
	xconf->csock = conf->csock;
	xconf->rcsock = conf->rcsock;
	/* set old one to NULL so we don't double free */
	conf->csock = NULL;
	conf->rcsock = NULL;

	/* clear all current filters and take over the new ones */
	filterlist_free(xconf->filters);
	xconf->filters = conf->filters;	
	conf->filters = NULL;

	/* switch the network statements, but first remove the old ones */
	while ((n = TAILQ_FIRST(&xconf->networks)) != NULL) {
		TAILQ_REMOVE(&xconf->networks, n, entry);
		filterset_free(&n->net.attrset);
		free(n);
	}
	while ((n = TAILQ_FIRST(&conf->networks)) != NULL) {
		TAILQ_REMOVE(&conf->networks, n, entry);
		TAILQ_INSERT_TAIL(&xconf->networks, n, entry);
	}

	/*
	 * merge new listeners:
	 * -flag all existing ones as to be deleted
	 * -those that are in both new and old: flag to keep
	 * -new ones get inserted and flagged as to reinit
	 * -remove all that are still flagged for deletion
	 */

	TAILQ_FOREACH(nla, xconf->listen_addrs, entry)
		nla->reconf = RECONF_DELETE;

	/* no new listeners? preserve default ones */
	if (TAILQ_EMPTY(conf->listen_addrs))
		TAILQ_FOREACH(ola, xconf->listen_addrs, entry)
			if (ola->flags & DEFAULT_LISTENER)
				ola->reconf = RECONF_KEEP;
	/* else loop over listeners and merge configs */
	for (nla = TAILQ_FIRST(conf->listen_addrs); nla != NULL; nla = next) {
		next = TAILQ_NEXT(nla, entry);

		TAILQ_FOREACH(ola, xconf->listen_addrs, entry)
			if (!memcmp(&nla->sa, &ola->sa, sizeof(nla->sa)))
				break;

		if (ola == NULL) {
			/* new listener, copy over */
			TAILQ_REMOVE(conf->listen_addrs, nla, entry);
			TAILQ_INSERT_TAIL(xconf->listen_addrs, nla, entry);
			nla->reconf = RECONF_REINIT;
		} else		/* exists, just flag */
			ola->reconf = RECONF_KEEP;
	}
	/* finally clean up the original list and remove all stale entires */
	for (nla = TAILQ_FIRST(xconf->listen_addrs); nla != NULL; nla = next) {
		next = TAILQ_NEXT(nla, entry);
		if (nla->reconf == RECONF_DELETE) {
			TAILQ_REMOVE(xconf->listen_addrs, nla, entry);
			free(nla);
		}
	}

	/* conf is merged so free it */
	free_config(conf);

	return (0);
}

u_int32_t
get_bgpid(void)
{
	struct ifaddrs		*ifap, *ifa;
	u_int32_t		 ip = 0, cur, localnet;

	localnet = htonl(INADDR_LOOPBACK & IN_CLASSA_NET);

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;
		cur = ((struct sockaddr_in *)ifa->ifa_addr)->sin_addr.s_addr;
		if ((cur & localnet) == localnet)	/* skip 127/8 */
			continue;
		if (ntohl(cur) > ntohl(ip))
			ip = cur;
	}
	freeifaddrs(ifap);

	return (ip);
}

int
host(const char *s, struct bgpd_addr *h, u_int8_t *len)
{
	int			 done = 0;
	int			 mask;
	char			*p, *ps;
	const char		*errstr;

	if ((p = strrchr(s, '/')) != NULL) {
		mask = strtonum(p + 1, 0, 128, &errstr);
		if (errstr) {
			log_warnx("prefixlen is %s: %s", errstr, p + 1);
			return (0);
		}
		if ((ps = malloc(strlen(s) - strlen(p) + 1)) == NULL)
			fatal("host: malloc");
		strlcpy(ps, s, strlen(s) - strlen(p) + 1);
	} else {
		if ((ps = strdup(s)) == NULL)
			fatal("host: strdup");
		mask = 128;
	}

	bzero(h, sizeof(struct bgpd_addr));

	/* IPv4 address? */
	if (!done)
		done = host_v4(s, h, len);

	/* IPv6 address? */
	if (!done) {
		done = host_v6(ps, h);
		*len = mask;
	}

	free(ps);

	return (done);
}

int
host_v4(const char *s, struct bgpd_addr *h, u_int8_t *len)
{
	struct in_addr		 ina;
	int			 bits = 32;

	bzero(&ina, sizeof(struct in_addr));
	if (strrchr(s, '/') != NULL) {
		if ((bits = inet_net_pton(AF_INET, s, &ina, sizeof(ina))) == -1)
			return (0);
	} else {
		if (inet_pton(AF_INET, s, &ina) != 1)
			return (0);
	}

	h->aid = AID_INET;
	h->v4.s_addr = ina.s_addr;
	*len = bits;

	return (1);
}

int
host_v6(const char *s, struct bgpd_addr *h)
{
	struct addrinfo		 hints, *res;

	bzero(&hints, sizeof(hints));
	hints.ai_family = AF_INET6;
	hints.ai_socktype = SOCK_DGRAM; /*dummy*/
	hints.ai_flags = AI_NUMERICHOST;
	if (getaddrinfo(s, "0", &hints, &res) == 0) {
		sa2addr(res->ai_addr, h);
		freeaddrinfo(res);
		return (1);
	}

	return (0);
}

void
prepare_listeners(struct bgpd_config *conf)
{
	struct listen_addr	*la, *next;
	int			 opt = 1;

	if (TAILQ_EMPTY(conf->listen_addrs)) {
		if ((la = calloc(1, sizeof(struct listen_addr))) == NULL)
			fatal("setup_listeners calloc");
		la->fd = -1;
		la->flags = DEFAULT_LISTENER;
		la->reconf = RECONF_REINIT;
		la->sa.ss_len = sizeof(struct sockaddr_in);
		((struct sockaddr_in *)&la->sa)->sin_family = AF_INET;
		((struct sockaddr_in *)&la->sa)->sin_addr.s_addr =
		    htonl(INADDR_ANY);
		((struct sockaddr_in *)&la->sa)->sin_port = htons(BGP_PORT);
		TAILQ_INSERT_TAIL(conf->listen_addrs, la, entry);

		if ((la = calloc(1, sizeof(struct listen_addr))) == NULL)
			fatal("setup_listeners calloc");
		la->fd = -1;
		la->flags = DEFAULT_LISTENER;
		la->reconf = RECONF_REINIT;
		la->sa.ss_len = sizeof(struct sockaddr_in6);
		((struct sockaddr_in6 *)&la->sa)->sin6_family = AF_INET6;
		((struct sockaddr_in6 *)&la->sa)->sin6_port = htons(BGP_PORT);
		TAILQ_INSERT_TAIL(conf->listen_addrs, la, entry);
	}

	for (la = TAILQ_FIRST(conf->listen_addrs); la != NULL; la = next) {
		next = TAILQ_NEXT(la, entry);
		if (la->reconf != RECONF_REINIT)
			continue;

		if ((la->fd = socket(la->sa.ss_family,
		    SOCK_STREAM | SOCK_CLOEXEC | SOCK_NONBLOCK,
		    IPPROTO_TCP)) == -1) {
			if (la->flags & DEFAULT_LISTENER && (errno ==
			    EAFNOSUPPORT || errno == EPROTONOSUPPORT)) {
				TAILQ_REMOVE(conf->listen_addrs, la, entry);
				free(la);
				continue;
			} else
				fatal("socket");
		}

		opt = 1;
		if (setsockopt(la->fd, SOL_SOCKET, SO_REUSEADDR,
		    &opt, sizeof(opt)) == -1)
			fatal("setsockopt SO_REUSEADDR");

		if (bind(la->fd, (struct sockaddr *)&la->sa, la->sa.ss_len) ==
		    -1) {
			switch (la->sa.ss_family) {
			case AF_INET:
				log_warn("cannot bind to %s:%u",
				    log_sockaddr((struct sockaddr *)&la->sa),
				    ntohs(((struct sockaddr_in *)
				    &la->sa)->sin_port));
				break;
			case AF_INET6:
				log_warn("cannot bind to [%s]:%u",
				    log_sockaddr((struct sockaddr *)&la->sa),
				    ntohs(((struct sockaddr_in6 *)
				    &la->sa)->sin6_port));
				break;
			default:
				log_warn("cannot bind to %s",
				    log_sockaddr((struct sockaddr *)&la->sa));
				break;
			}
			close(la->fd);
			TAILQ_REMOVE(conf->listen_addrs, la, entry);
			free(la);
			continue;
		}
	}
}

int
get_mpe_label(struct rdomain *r)
{
	struct  ifreq	ifr;
	struct shim_hdr	shim;
	int		s;

	s = socket(AF_INET, SOCK_DGRAM, 0);
	if (s == -1)
		return (-1);

	bzero(&shim, sizeof(shim));
	bzero(&ifr, sizeof(ifr));
	strlcpy(ifr.ifr_name, r->ifmpe, sizeof(ifr.ifr_name));
	ifr.ifr_data = (caddr_t)&shim;

	if (ioctl(s, SIOCGETLABEL, (caddr_t)&ifr) == -1) {
		close(s);
		return (-1);
	}
	close(s);
	r->label = shim.shim_label;
	return (0);
}
