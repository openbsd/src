/*	$OpenBSD: server.c,v 1.8 2004/07/11 03:05:50 dtucker Exp $ */

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
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER
 * IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING
 * OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>
#include <ifaddrs.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"
#include "ntp.h"

int
setup_listeners(struct servent *se, struct ntpd_conf *conf, u_int *cnt)
{
	struct listen_addr	*la;
	struct ifaddrs		*ifap;
	struct sockaddr		*sa;
	u_int			 new_cnt = 0;

	if (conf->listen_all) {
		if (getifaddrs(&ifap) == -1)
			fatal("getifaddrs");

		for (; ifap != NULL; ifap = ifap->ifa_next) {
			sa = ifap->ifa_addr;

			if (sa->sa_family != AF_INET &&
			    sa->sa_family != AF_INET6)
				continue;

			if ((la = calloc(1, sizeof(struct listen_addr))) ==
			    NULL)
				fatal("setup_listeners calloc");

			memcpy(&la->sa, sa, SA_LEN(sa));
			TAILQ_INSERT_TAIL(&conf->listen_addrs, la, entry);
		}

		freeifaddrs(ifap);
	}

	TAILQ_FOREACH(la, &conf->listen_addrs, entry) {
		new_cnt++;

		switch (la->sa.ss_family) {
		case AF_INET:
			if (((struct sockaddr_in *)&la->sa)->sin_port == 0)
				((struct sockaddr_in *)&la->sa)->sin_port =
				    se->s_port;
			break;
		case AF_INET6:
			if (((struct sockaddr_in6 *)&la->sa)->sin6_port == 0)
				((struct sockaddr_in6 *)&la->sa)->sin6_port =
				    se->s_port;
			break;
		default:
			fatalx("king bula sez: af borked");
		}

		log_info("listening on %s",
		    log_sockaddr((struct sockaddr *)&la->sa));

		if ((la->fd = socket(la->sa.ss_family, SOCK_DGRAM, 0)) == -1)
			fatal("socket");

		if (bind(la->fd, (struct sockaddr *)&la->sa,
		    SA_LEN((struct sockaddr *)&la->sa)) == -1)
			fatal("bind");
	}

	*cnt = new_cnt;

	return (0);
}

int
server_dispatch(int fd)
{
	ssize_t			 size;
	double			 rectime;
	struct sockaddr_storage	 fsa;
	socklen_t		 fsa_len;
	struct ntp_msg		 query, reply;
	char			 buf[NTP_MSGSIZE];

	fsa_len = sizeof(fsa);
	if ((size = recvfrom(fd, &buf, sizeof(buf), 0,
	    (struct sockaddr *)&fsa, &fsa_len)) == -1)
		fatal("recvfrom");

	rectime = gettime();

	ntp_getmsg(buf, size, &query);

	bzero(&reply, sizeof(reply));
	reply.status = 0 | (query.status & VERSIONMASK);
	if ((query.status & MODEMASK) == MODE_CLIENT)
		reply.status |= MODE_SERVER;
	else
		reply.status |= MODE_SYM_PAS;

	reply.stratum =	2;			/* XXX */
	reply.ppoll = query.ppoll;
	reply.precision = 0;			/* XXX */
	reply.rectime = d_to_lfp(rectime);
	reply.reftime = reply.rectime;		/* XXX */
	reply.xmttime = d_to_lfp(gettime());
	reply.orgtime = query.xmttime;
	reply.refid = reply.xmttime.fraction;	/* XXX */

	return (ntp_sendmsg(fd, (struct sockaddr *)&fsa, &reply, size, 0));
}
