/*	$OpenBSD: client.c,v 1.94 2015/01/04 01:19:46 bcook Exp $ */

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
#include <md5.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "ntpd.h"

int	client_update(struct ntp_peer *);
void	set_deadline(struct ntp_peer *, time_t);

void
set_next(struct ntp_peer *p, time_t t)
{
	p->next = getmonotime() + t;
	p->deadline = 0;
	p->poll = t;
}

void
set_deadline(struct ntp_peer *p, time_t t)
{
	p->deadline = getmonotime() + t;
	p->next = 0;
}

int
client_peer_init(struct ntp_peer *p)
{
	if ((p->query = calloc(1, sizeof(struct ntp_query))) == NULL)
		fatal("client_peer_init calloc");
	p->query->fd = -1;
	p->query->msg.status = MODE_CLIENT | (NTP_VERSION << 3);
	p->state = STATE_NONE;
	p->shift = 0;
	p->trustlevel = TRUSTLEVEL_PATHETIC;
	p->lasterror = 0;
	p->senderrors = 0;

	return (client_addr_init(p));
}

int
client_addr_init(struct ntp_peer *p)
{
	struct sockaddr_in	*sa_in;
	struct sockaddr_in6	*sa_in6;
	struct ntp_addr		*h;

	for (h = p->addr; h != NULL; h = h->next) {
		switch (h->ss.ss_family) {
		case AF_INET:
			sa_in = (struct sockaddr_in *)&h->ss;
			if (ntohs(sa_in->sin_port) == 0)
				sa_in->sin_port = htons(123);
			p->state = STATE_DNS_DONE;
			break;
		case AF_INET6:
			sa_in6 = (struct sockaddr_in6 *)&h->ss;
			if (ntohs(sa_in6->sin6_port) == 0)
				sa_in6->sin6_port = htons(123);
			p->state = STATE_DNS_DONE;
			break;
		default:
			fatalx("king bula sez: wrong AF in client_addr_init");
			/* not reached */
		}
	}

	p->query->fd = -1;
	set_next(p, 0);

	return (0);
}

int
client_nextaddr(struct ntp_peer *p)
{
	if (p->query->fd != -1) {
		close(p->query->fd);
		p->query->fd = -1;
	}

	if (p->state == STATE_DNS_INPROGRESS)
		return (-1);

	if (p->addr_head.a == NULL) {
		priv_host_dns(p->addr_head.name, p->id);
		p->state = STATE_DNS_INPROGRESS;
		return (-1);
	}

	if ((p->addr = p->addr->next) == NULL)
		p->addr = p->addr_head.a;

	p->shift = 0;
	p->trustlevel = TRUSTLEVEL_PATHETIC;

	return (0);
}

#define SCALE_MAX(a, b) ((a) > (b) ? (a) : (b))

int
client_query(struct ntp_peer *p)
{
	int	val;

	if (p->addr == NULL && client_nextaddr(p) == -1) {
		set_next(p, SCALE_MAX(SETTIME_TIMEOUT,
		    scale_interval(INTERVAL_QUERY_AGGRESSIVE)));
		return (0);
	}

	if (p->state < STATE_DNS_DONE || p->addr == NULL)
		return (-1);

	if (p->query->fd == -1) {
		struct sockaddr *sa = (struct sockaddr *)&p->addr->ss;

		if ((p->query->fd = socket(p->addr->ss.ss_family, SOCK_DGRAM,
		    0)) == -1)
			fatal("client_query socket");

		if (p->rtable != -1 &&
		    setsockopt(p->query->fd, SOL_SOCKET, SO_RTABLE,
		    &p->rtable, sizeof(p->rtable)) == -1)
			fatal("client_query setsockopt SO_RTABLE");
		if (connect(p->query->fd, sa, SA_LEN(sa)) == -1) {
			if (errno == ECONNREFUSED || errno == ENETUNREACH ||
			    errno == EHOSTUNREACH || errno == EADDRNOTAVAIL) {
				client_nextaddr(p);
				set_next(p, SCALE_MAX(SETTIME_TIMEOUT,
				    scale_interval(INTERVAL_QUERY_AGGRESSIVE)));
				return (-1);
			} else
				fatal("client_query connect");
		}
		val = IPTOS_LOWDELAY;
		if (p->addr->ss.ss_family == AF_INET && setsockopt(p->query->fd,
		    IPPROTO_IP, IP_TOS, &val, sizeof(val)) == -1)
			log_warn("setsockopt IPTOS_LOWDELAY");
		val = 1;
		if (setsockopt(p->query->fd, SOL_SOCKET, SO_TIMESTAMP,
		    &val, sizeof(val)) == -1)
			fatal("setsockopt SO_TIMESTAMP");
	}

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

	p->query->msg.xmttime.int_partl = arc4random();
	p->query->msg.xmttime.fractionl = arc4random();
	p->query->xmttime = gettime_corrected();

	if (ntp_sendmsg(p->query->fd, NULL, &p->query->msg) == -1) {
		p->senderrors++;
		set_next(p, INTERVAL_QUERY_PATHETIC);
		p->trustlevel = TRUSTLEVEL_PATHETIC;
		return (-1);
	}

	p->senderrors = 0;
	p->state = STATE_QUERY_SENT;
	set_deadline(p, QUERYTIME_MAX);

	return (0);
}

int
client_dispatch(struct ntp_peer *p, u_int8_t settime)
{
	struct ntp_msg		 msg;
	struct msghdr		 somsg;
	struct iovec		 iov[1];
	struct timeval		 tv;
	char			 buf[NTP_MSGSIZE];
	union {
		struct cmsghdr	hdr;
		char		buf[CMSG_SPACE(sizeof(tv))];
	} cmsgbuf;
	struct cmsghdr		*cmsg;
	ssize_t			 size;
	double			 T1, T2, T3, T4;
	time_t			 interval;

	bzero(&somsg, sizeof(somsg));
	iov[0].iov_base = buf;
	iov[0].iov_len = sizeof(buf);
	somsg.msg_iov = iov;
	somsg.msg_iovlen = 1;
	somsg.msg_control = cmsgbuf.buf;
	somsg.msg_controllen = sizeof(cmsgbuf.buf);

	T4 = getoffset();
	if ((size = recvmsg(p->query->fd, &somsg, 0)) == -1) {
		if (errno == EHOSTUNREACH || errno == EHOSTDOWN ||
		    errno == ENETUNREACH || errno == ENETDOWN ||
		    errno == ECONNREFUSED || errno == EADDRNOTAVAIL ||
		    errno == ENOPROTOOPT || errno == ENOENT) {
			client_log_error(p, "recvmsg", errno);
			set_next(p, error_interval());
			return (0);
		} else
			fatal("recvfrom");
	}

	if (somsg.msg_flags & MSG_TRUNC) {
		client_log_error(p, "recvmsg packet", EMSGSIZE);
		set_next(p, error_interval());
		return (0);
	}

	if (somsg.msg_flags & MSG_CTRUNC) {
		client_log_error(p, "recvmsg control data", E2BIG);
		set_next(p, error_interval());
		return (0);
	}

	if (p->rtable != -1 &&
	    setsockopt(p->query->fd, SOL_SOCKET, SO_RTABLE, &p->rtable,
	    sizeof(p->rtable)) == -1)
		fatal("client_dispatch setsockopt SO_RTABLE");

	for (cmsg = CMSG_FIRSTHDR(&somsg); cmsg != NULL;
	    cmsg = CMSG_NXTHDR(&somsg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
		    cmsg->cmsg_type == SCM_TIMESTAMP) {
			memcpy(&tv, CMSG_DATA(cmsg), sizeof(tv));
			T4 += tv.tv_sec + JAN_1970 + 1.0e-6 * tv.tv_usec;
			break;
		}
	}

	if (T4 < JAN_1970) {
		client_log_error(p, "recvmsg control format", EBADF);
		set_next(p, error_interval());
		return (0);
	}

	ntp_getmsg((struct sockaddr *)&p->addr->ss, buf, size, &msg);

	if (msg.orgtime.int_partl != p->query->msg.xmttime.int_partl ||
	    msg.orgtime.fractionl != p->query->msg.xmttime.fractionl)
		return (0);

	if ((msg.status & LI_ALARM) == LI_ALARM || msg.stratum == 0 ||
	    msg.stratum > NTP_MAXSTRATUM) {
		char s[16];

		if ((msg.status & LI_ALARM) == LI_ALARM) {
			strlcpy(s, "alarm", sizeof(s));
		} else if (msg.stratum == 0) {
			/* Kiss-o'-Death (KoD) packet */
			strlcpy(s, "KoD", sizeof(s));
		} else if (msg.stratum > NTP_MAXSTRATUM) {
			snprintf(s, sizeof(s), "stratum %d", msg.stratum);
		}
		interval = error_interval();
		set_next(p, interval);
		log_info("reply from %s: not synced (%s), next query %llds",
		    log_sockaddr((struct sockaddr *)&p->addr->ss), s,
			(long long)interval);
		return (0);
	}

	/*
	 * From RFC 2030 (with a correction to the delay math):
	 *
	 *     Timestamp Name          ID   When Generated
	 *     ------------------------------------------------------------
	 *     Originate Timestamp     T1   time request sent by client
	 *     Receive Timestamp       T2   time request received by server
	 *     Transmit Timestamp      T3   time reply sent by server
	 *     Destination Timestamp   T4   time reply received by client
	 *
	 *  The roundtrip delay d and local clock offset t are defined as
	 *
	 *    d = (T4 - T1) - (T3 - T2)     t = ((T2 - T1) + (T3 - T4)) / 2.
	 */

	T1 = p->query->xmttime;
	T2 = lfp_to_d(msg.rectime);
	T3 = lfp_to_d(msg.xmttime);

	/*
	 * XXX workaround: time_t / tv_sec must never wrap.
	 * around 2020 we will need a solution (64bit time_t / tv_sec).
	 * consider every answer with a timestamp beyond january 2030 bogus.
	 */
	if (T2 > JAN_2030 || T3 > JAN_2030) {
		set_next(p, error_interval());
		return (0);
	}

	p->reply[p->shift].offset = ((T2 - T1) + (T3 - T4)) / 2;
	p->reply[p->shift].delay = (T4 - T1) - (T3 - T2);
	p->reply[p->shift].status.stratum = msg.stratum;
	if (p->reply[p->shift].delay < 0) {
		interval = error_interval();
		set_next(p, interval);
		log_info("reply from %s: negative delay %fs, "
		    "next query %llds",
		    log_sockaddr((struct sockaddr *)&p->addr->ss),
		    p->reply[p->shift].delay, (long long)interval);
		return (0);
	}
	p->reply[p->shift].error = (T2 - T1) - (T3 - T4);
	p->reply[p->shift].rcvd = getmonotime();
	p->reply[p->shift].good = 1;

	p->reply[p->shift].status.leap = (msg.status & LIMASK);
	p->reply[p->shift].status.precision = msg.precision;
	p->reply[p->shift].status.rootdelay = sfp_to_d(msg.rootdelay);
	p->reply[p->shift].status.rootdispersion = sfp_to_d(msg.dispersion);
	p->reply[p->shift].status.refid = msg.refid;
	p->reply[p->shift].status.reftime = lfp_to_d(msg.reftime);
	p->reply[p->shift].status.poll = msg.ppoll;

	if (p->addr->ss.ss_family == AF_INET) {
		p->reply[p->shift].status.send_refid =
		    ((struct sockaddr_in *)&p->addr->ss)->sin_addr.s_addr;
	} else if (p->addr->ss.ss_family == AF_INET6) {
		MD5_CTX		context;
		u_int8_t	digest[MD5_DIGEST_LENGTH];

		MD5Init(&context);
		MD5Update(&context, ((struct sockaddr_in6 *)&p->addr->ss)->
		    sin6_addr.s6_addr, sizeof(struct in6_addr));
		MD5Final(digest, &context);
		memcpy((char *)&p->reply[p->shift].status.send_refid, digest,
		    sizeof(u_int32_t));
	} else
		p->reply[p->shift].status.send_refid = msg.xmttime.fractionl;

	if (p->trustlevel < TRUSTLEVEL_PATHETIC)
		interval = scale_interval(INTERVAL_QUERY_PATHETIC);
	else if (p->trustlevel < TRUSTLEVEL_AGGRESSIVE)
		interval = scale_interval(INTERVAL_QUERY_AGGRESSIVE);
	else
		interval = scale_interval(INTERVAL_QUERY_NORMAL);

	set_next(p, interval);
	p->state = STATE_REPLY_RECEIVED;

	/* every received reply which we do not discard increases trust */
	if (p->trustlevel < TRUSTLEVEL_MAX) {
		if (p->trustlevel < TRUSTLEVEL_BADPEER &&
		    p->trustlevel + 1 >= TRUSTLEVEL_BADPEER)
			log_info("peer %s now valid",
			    log_sockaddr((struct sockaddr *)&p->addr->ss));
		p->trustlevel++;
	}

	log_debug("reply from %s: offset %f delay %f, "
	    "next query %llds %s",
	    log_sockaddr((struct sockaddr *)&p->addr->ss),
	    p->reply[p->shift].offset, p->reply[p->shift].delay,
	    (long long)interval, print_rtable(p->rtable));

	client_update(p);
	if (settime)
		priv_settime(p->reply[p->shift].offset);

	if (++p->shift >= OFFSET_ARRAY_SIZE)
		p->shift = 0;

	return (0);
}

int
client_update(struct ntp_peer *p)
{
	int	i, best = 0, good = 0;

	/*
	 * clock filter
	 * find the offset which arrived with the lowest delay
	 * use that as the peer update
	 * invalidate it and all older ones
	 */

	for (i = 0; good == 0 && i < OFFSET_ARRAY_SIZE; i++)
		if (p->reply[i].good) {
			good++;
			best = i;
		}

	for (; i < OFFSET_ARRAY_SIZE; i++)
		if (p->reply[i].good) {
			good++;
			if (p->reply[i].delay < p->reply[best].delay)
				best = i;
		}

	if (good < 8)
		return (-1);

	memcpy(&p->update, &p->reply[best], sizeof(p->update));
	if (priv_adjtime() == 0) {
		for (i = 0; i < OFFSET_ARRAY_SIZE; i++)
			if (p->reply[i].rcvd <= p->reply[best].rcvd)
				p->reply[i].good = 0;
	}
	return (0);
}

void
client_log_error(struct ntp_peer *peer, const char *operation, int error)
{
	const char *address;

	address = log_sockaddr((struct sockaddr *)&peer->addr->ss);
	if (peer->lasterror == error) {
		log_debug("%s %s: %s", operation, address, strerror(error));
		return;
	}
	peer->lasterror = error;
	log_warn("%s %s", operation, address);
}
