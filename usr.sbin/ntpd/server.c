/*	$OpenBSD: server.c,v 1.1 2004/06/02 10:08:59 henning Exp $ */

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

#include <sys/param.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ntpd.h"
#include "ntp.h"

int
setup_listeners(struct servent *se, struct ntpd_conf *conf)
{
	struct listen_addr	*la;

	if (TAILQ_EMPTY(&conf->listen_addrs)) {
		if ((la = calloc(1, sizeof(struct listen_addr))) == NULL)
			fatal("setup_listeners calloc");
		la->sa.ss_len = sizeof(struct sockaddr_in);
		((struct sockaddr_in *)&la->sa)->sin_family = AF_INET;
		((struct sockaddr_in *)&la->sa)->sin_addr.s_addr =
		    htonl(INADDR_ANY);
		((struct sockaddr_in *)&la->sa)->sin_port = se->s_port;
		TAILQ_INSERT_TAIL(&conf->listen_addrs, la, entry);

		if ((la = calloc(1, sizeof(struct listen_addr))) == NULL)
			fatal("setup_listeners calloc");
		la->sa.ss_len = sizeof(struct sockaddr_in6);
		((struct sockaddr_in6 *)&la->sa)->sin6_family = AF_INET6;
		((struct sockaddr_in6 *)&la->sa)->sin6_port = se->s_port;
		TAILQ_INSERT_TAIL(&conf->listen_addrs, la, entry);
	}

	TAILQ_FOREACH(la, &conf->listen_addrs, entry) {
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

		if ((la->fd = socket(la->sa.ss_family, SOCK_DGRAM, 0)) == -1)
			fatal("socket");

		if (bind(la->fd, (struct sockaddr *)&la->sa, la->sa.ss_len) ==
		    -1)
			fatal("bind");
	}

	return (0);
}

int
ntp_reply(int fd, struct sockaddr *sa, struct ntp_msg *query, int auth)
{
	ssize_t			 len;
	struct l_fixedpt	 t;
	struct ntp_msg		 reply;

	if (auth)
		len = NTP_MSGSIZE;
	else
		len = NTP_MSGSIZE_NOAUTH;

	bzero(&reply, sizeof(reply));
	reply.status = 0 | (query->status & VERSIONMASK);
	if ((query->status & MODEMASK) == MODE_CLIENT)
		reply.status |= MODE_SERVER;
	else
		reply.status |= MODE_SYM_PAS;

	reply.stratum =	2;
	reply.ppoll = query->ppoll;
	reply.precision = 0;			/* XXX */
	reply.refid = htonl(t.fraction);	/* XXX */
	get_ts(&t);
	reply.reftime.int_part = htonl(t.int_part);	/* XXX */
	reply.reftime.fraction = htonl(t.fraction);	/* XXX */
	reply.rectime.int_part = htonl(t.int_part);
	reply.rectime.fraction = htonl(t.fraction);
	reply.xmttime.int_part = htonl(t.int_part);
	reply.xmttime.fraction = htonl(t.fraction);
	reply.orgtime.int_part = query->xmttime.int_part;
	reply.orgtime.fraction = query->xmttime.fraction;

	return (ntp_sendmsg(fd, sa, &reply, len, auth));
}
