/*	$OpenBSD: client.c,v 1.15 2004/07/07 06:57:13 henning Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2004 Alexander Guy <alexander.guy@andern.org>
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
#include <time.h>
#include <unistd.h>

#include "ntpd.h"

int
client_peer_init(struct ntp_peer *p)
{
	struct sockaddr_in	*sa_in;
	struct sockaddr_in6	*sa_in6;

	if ((p->query = calloc(1, sizeof(struct ntp_query))) == NULL)
		fatal("client_query calloc");

	switch (p->ss.ss_family) {
	case AF_INET:
		sa_in = (struct sockaddr_in *)&p->ss;
		if (ntohs(sa_in->sin_port) == 0)
			sa_in->sin_port = htons(123);
		break;
	case AF_INET6:
		sa_in6 = (struct sockaddr_in6 *)&p->ss;
		if (ntohs(sa_in6->sin6_port) == 0)
			sa_in6->sin6_port = htons(123);
		break;
	default:
		fatal("king bula sez: wrong AF in client_peer_init");
		/* not reached */
	}

	if ((p->query->fd = socket(p->ss.ss_family, SOCK_DGRAM, 0)) == -1)
		fatal("client_query socket");

	p->query->msg.status = MODE_CLIENT | (NTP_VERSION << 3);
	p->state = STATE_NONE;
	p->next = time(NULL);
	p->shift = 0;
	p->trustlevel = TRUSTLEVEL_PATHETIC;

	return (0);
}

int
client_query(struct ntp_peer *p)
{
	/*
	 * Send out a random 64-bit number as our transmit time.  The NTP
	 * server will copy said number into the originate field on the
	 * response that it sends us.  This is totally legal per the SNTP spec.
	 *
	 * The impact of this is two fold: we no longer send out the current
	 * system time for the world to see (which may aid an attacker), and
	 * it gives us a (not very secure) way of knowing that we're not
	 * getting spoofed by an attacker that can't capture our traffic
	 * but can spoof packets from the NTP server we're communicating with.
	 *
	 * Save the real transmit timestamp locally.
	 */

	p->query->msg.xmttime.int_part = arc4random();
	p->query->msg.xmttime.fraction = arc4random();
	p->query->xmttime = gettime();

	ntp_sendmsg(p->query->fd, (struct sockaddr *)&p->ss, &p->query->msg,
	    NTP_MSGSIZE_NOAUTH, 0);
	p->state = STATE_QUERY_SENT;
	p->next = 0;
	p->deadline = time(NULL) + QUERYTIME_MAX;

	return (0);
}

int
client_dispatch(struct ntp_peer *p)
{
	struct sockaddr_storage	 fsa;
	socklen_t		 fsa_len;
	char			 buf[NTP_MSGSIZE];
	ssize_t			 size;
	struct ntp_msg		 msg;
	double			 T1, T2, T3, T4;

	fsa_len = sizeof(fsa);
	if ((size = recvfrom(p->query->fd, &buf, sizeof(buf), 0,
	    (struct sockaddr *)&fsa, &fsa_len)) == -1)
		fatal("recvfrom");

	T4 = gettime();

	ntp_getmsg(buf, size, &msg);

	if (msg.orgtime.int_part != p->query->msg.xmttime.int_part ||
	    msg.orgtime.fraction != p->query->msg.xmttime.fraction)
		return (0);

	/*
	 * From RFC 2030:
	 *
	 *      Timestamp Name          ID   When Generated
	 *     ------------------------------------------------------------
	 *     Originate Timestamp     T1   time request sent by client
	 *     Receive Timestamp       T2   time request received by server
	 *     Transmit Timestamp      T3   time reply sent by server
	 *     Destination Timestamp   T4   time reply received by client
	 *
	 *  The roundtrip delay d and local clock offset t are defined as
	 *
	 *    d = (T4 - T1) - (T2 - T3)     t = ((T2 - T1) + (T3 - T4)) / 2.
	 */

	T1 = p->query->xmttime;
	T2 = lfp_to_d(msg.rectime);
	T3 = lfp_to_d(msg.xmttime);

	p->reply[p->shift].offset = ((T2 - T1) + (T3 - T4)) / 2;
	p->reply[p->shift].delay = (T4 - T1) - (T2 - T3);
	p->reply[p->shift].error = (T2 - T1) - (T3 - T4);
	p->reply[p->shift].rcvd = time(NULL);
	p->reply[p->shift].good = 1;

	if (p->trustlevel < TRUSTLEVEL_PATHETIC)
		p->next = time(NULL) + INTERVAL_QUERY_PATHETIC;
	else if (p->trustlevel < TRUSTLEVEL_AGRESSIVE)
		p->next = time(NULL) + INTERVAL_QUERY_AGRESSIVE;
	else
		p->next = time(NULL) + INTERVAL_QUERY_NORMAL;

	p->deadline = 0;
	p->state = STATE_REPLY_RECEIVED;

	/* every received reply which we do not discard increases trust */
	if (p->trustlevel < 10) {
		if (p->trustlevel < TRUSTLEVEL_BADPEER &&
		    p->trustlevel + 1 >= TRUSTLEVEL_BADPEER)
			log_info("peer %s now valid",
			    log_sockaddr((struct sockaddr *)&fsa));
		p->trustlevel++;
	}

	if (++p->shift >= OFFSET_ARRAY_SIZE)
		p->shift = 0;

	return (0);
}
