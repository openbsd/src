/*	$OpenBSD: check_icmp.c,v 1.6 2007/01/08 13:37:26 reyk Exp $	*/

/*
 * Copyright (c) 2006 Pierre-Yves Ritschard <pyr@spootnik.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/icmp6.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <limits.h>
#include <event.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <regex.h>

#include "hostated.h"

int	icmp6_checks_done(struct ctl_icmp_event *);
int	icmp4_checks_done(struct ctl_icmp_event *);
void	send_icmp6(struct ctl_icmp_event *, struct host *);
void	send_icmp4(struct ctl_icmp_event *, struct host *);
void	recv_icmp6(int, short, void *);
void	recv_icmp4(int, short, void *);
int	in_cksum(u_short *, int);

void
schedule_icmp(struct ctl_icmp_event *cie, struct table *table)
{
	struct host	*host;

	TAILQ_FOREACH(host, &table->hosts, entry) {
		if (host->flags & F_DISABLE)
			continue;
		host->last_up = host->up;
		host->flags &= ~F_CHECK_DONE;
		if (((struct sockaddr *)&host->ss)->sa_family == AF_INET) {
			send_icmp4(cie, host);
		} else {
			send_icmp6(cie, host);
		}
	}
}

void
check_icmp(struct ctl_icmp_event *cie)
{
	struct timeval	tv;

	if (gettimeofday(&cie->tv_start, NULL))
		fatal("check_icmp: gettimeofday");
	bcopy(&cie->env->timeout, &tv, sizeof(tv));
	if (cie->has_icmp4)
		event_once(cie->icmp_sock, EV_READ|EV_TIMEOUT,
		    recv_icmp4, cie, &tv);
	if (cie->has_icmp6)
		event_once(cie->icmp6_sock, EV_READ|EV_TIMEOUT,
		    recv_icmp6, cie, &tv);
}

int
icmp6_checks_done(struct ctl_icmp_event *cie)
{
	struct table	*table;
	struct host	*host;

	TAILQ_FOREACH(table, &cie->env->tables, entry) {
		if (table->flags & F_DISABLE || table->check != CHECK_ICMP)
			continue;
		TAILQ_FOREACH(host, &table->hosts, entry) {
			if (((struct sockaddr *)&host->ss)->sa_family !=
			    AF_INET6)
				continue;
			if (!(host->flags & F_CHECK_DONE))
				return (0);
		}
	}
	return (1);
}

int
icmp4_checks_done(struct ctl_icmp_event *cie)
{
	struct table	*table;
	struct host	*host;

	TAILQ_FOREACH(table, &cie->env->tables, entry) {
		if (table->flags & F_DISABLE || table->check != CHECK_ICMP)
			continue;
		TAILQ_FOREACH(host, &table->hosts, entry) {
			if (((struct sockaddr *)&host->ss)->sa_family !=
			    AF_INET)
				continue;
			if (!(host->flags & F_CHECK_DONE)) {
				return (0);
			}
		}
	}
	return (1);
}

void
send_icmp6(struct ctl_icmp_event *cie, struct host *host)
{
	struct sockaddr		*to;
	struct icmp6_hdr	*icp;
	ssize_t			 i;
	int			 datalen = (64 - 8);
	u_char			 packet[datalen];

	cie->has_icmp6 = 1;
	to = (struct sockaddr *)&host->ss;
	bzero(&packet, sizeof(packet));
	icp = (struct icmp6_hdr *)packet;
	icp->icmp6_type = ICMP6_ECHO_REQUEST;
	icp->icmp6_code = 0;
	icp->icmp6_seq = 1;
	icp->icmp6_id = getpid() & 0xffff;

	memcpy((packet + sizeof(*icp)), &host->id, sizeof(host->id));

	i = sendto(cie->icmp6_sock, packet, datalen + 8, 0, to,
	    sizeof(struct sockaddr_in6));
	if (i < 0 || i != datalen + 8) {
		host->up = HOST_DOWN;
		hce_notify_done(host, "send_icmp6: cannot send");
		return;
	}
}

void
send_icmp4(struct ctl_icmp_event *cie, struct host *host)
{
	struct sockaddr	*to;
	struct icmp	*icp;
	ssize_t		 i;
	int		 datalen = (64 - 8);
	u_char		 packet[datalen];

	cie->has_icmp4 = 1;
	to = (struct sockaddr *)&host->ss;
	bzero(&packet, sizeof(packet));
	icp = (struct icmp *)packet;
	icp->icmp_type = ICMP_ECHO;
	icp->icmp_code = 0;
	icp->icmp_seq = htons(1);
	icp->icmp_id = htons(getpid() & 0xffff);
	icp->icmp_cksum = 0;

	memcpy(icp->icmp_data, &host->id, sizeof(host->id));
	icp->icmp_cksum = in_cksum((u_short *)icp, datalen + 8);

	i = sendto(cie->icmp_sock, packet, datalen + 8, 0, to,
	    sizeof(struct sockaddr_in));
	if (i < 0 || i != datalen + 8) {
		host->up = HOST_DOWN;
		hce_notify_done(host, "send_icmp4: cannot send");
	}
}

void
recv_icmp6(int s, short event, void *arg)
{
	struct ctl_icmp_event	*cie = arg;
	int			 datalen = (64 - 8);
	u_char			 packet[datalen];
	socklen_t		 len;
	struct sockaddr_storage	 ss;
	struct icmp6_hdr	*icp;
	struct host		*host;
	struct table		*table;
	ssize_t			 i;
	objid_t			 id;
	struct timeval		 tv;
	struct timeval		 tv_now;

	if (event == EV_TIMEOUT) {
		/*
		 * mark all hosts which have not responded yet as down.
		 */
		TAILQ_FOREACH(table, &cie->env->tables, entry) {
			if (table->check != CHECK_ICMP ||
			    table->flags & F_DISABLE)
				continue;
			TAILQ_FOREACH(host, &table->hosts, entry) {
				if (host->flags & F_DISABLE)
					continue;
				if (((struct sockaddr *)&host->ss)->sa_family
				    != AF_INET6)
					continue;
				if (!(host->flags & F_CHECK_DONE)) {
					host->up = HOST_DOWN;
				}
			}
		}
		return;
	}
	bzero(&packet, sizeof(packet));
	bzero(&ss, sizeof(ss));
	len = sizeof(struct sockaddr_in6);
	i = recvfrom(s, packet, datalen + 8, 0, (struct sockaddr *)&ss, &len);
	if (i < 0 || i != datalen + 8) {
		log_warn("recv_icmp6: did not receive valid ping");
		return;
	}
	icp = (struct icmp6_hdr *)(packet);
	memcpy(&id, (packet + sizeof(*icp)), sizeof(id));
	host = host_find(cie->env, id);
	if (host == NULL)
		log_warn("recv_icmp6: ping for unknown host received");
	if (bcmp(&ss, &host->ss, len)) {
		log_warnx("recv_icmp6: forged icmp packet ?");
		return;
	}
	if (icp->icmp6_id != (getpid() & 0xffff)) {
		log_warnx("recv_icmp6: did not receive valid ident");
		host->up = HOST_DOWN;
	} else
		host->up = HOST_UP;
	hce_notify_done(host, "recv_icmp6: final");
	if (icmp6_checks_done(cie))
		return;
	if (gettimeofday(&tv_now, NULL))
		fatal("recv_icmp6: gettimeofday");
	bcopy(&cie->env->timeout, &tv, sizeof(tv));
	timersub(&tv_now, &cie->tv_start, &tv_now);
	timersub(&tv, &tv_now, &tv);
	event_once(cie->icmp6_sock, EV_READ|EV_TIMEOUT, recv_icmp6, cie, &tv);
}

void
recv_icmp4(int s, short event, void *arg)
{
	int			 datalen = (64 - 8);
	socklen_t		 len;
	struct icmp		*icp;
	struct ctl_icmp_event	*cie = arg;
	u_char			 packet[datalen];
	struct host		*host;
	struct table		*table;
	ssize_t			 i;
	objid_t			 id;
	struct timeval		 tv;
	struct timeval		 tv_now;
	struct sockaddr_storage	 ss;

	if (event == EV_TIMEOUT) {
		/*
		 * mark all hosts which have not responded yet as down.
		 */
		TAILQ_FOREACH(table, &cie->env->tables, entry) {
			if (table->check != CHECK_ICMP ||
			    table->flags & F_DISABLE)
				continue;
			TAILQ_FOREACH(host, &table->hosts, entry) {
				if (host->flags & F_DISABLE)
					continue;
				if (((struct sockaddr *)&host->ss)->sa_family
				    != AF_INET)
					continue;
				if (!(host->flags & F_CHECK_DONE)) {
					host->up = HOST_DOWN;
				}
			}
		}
		return;
	}

	len = sizeof(struct sockaddr_in);
	bzero(&packet, sizeof(packet));
	bzero(&ss, sizeof(ss));
	i = recvfrom(s, packet, datalen + 8, 0, (struct sockaddr *)&ss, &len);
	if (i < 0 || i != (datalen + 8)) {
		log_warn("recv_icmp4: did not receive valid ping");
		return;
	}

	icp = (struct icmp *)(packet + sizeof(struct ip));
	memcpy(&id, icp->icmp_data, sizeof(id));
	host = host_find(cie->env, id);
	if (host == NULL) {
		log_warnx("recv_icmp4: received ping for unknown host");
		return;
	}
	if (bcmp(&ss, &host->ss, len)) {
		log_warnx("recv_icmp4: forged icmp packet ?");
		return;
	}
	if (ntohs(icp->icmp_id) != (getpid() & 0xffff)) {
		log_warnx("recv_icmp4: did not receive valid ident");
		host->up = HOST_DOWN;
	} else
		host->up = HOST_UP;

	host->flags |= F_CHECK_DONE;
	if (icmp4_checks_done(cie)) {
		hce_notify_done(host, "recv_icmp4: all done");
		return;
	}
	hce_notify_done(host, "recv_icmp4: host");

	if (gettimeofday(&tv_now, NULL))
		fatal("recv_icmp4: gettimeofday");

	bcopy(&cie->env->timeout, &tv, sizeof(tv));
	timersub(&tv_now, &cie->tv_start, &tv_now);
	timersub(&tv, &tv_now, &tv);
	event_once(cie->icmp_sock, EV_READ|EV_TIMEOUT, recv_icmp4, cie, &tv);
}

/* in_cksum from ping.c --
 *	Checksum routine for Internet Protocol family headers (C Version)
 *
 * Copyright (c) 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Mike Muuss.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
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
int
in_cksum(u_short *addr, int len)
{
	int nleft = len;
	u_short *w = addr;
	int sum = 0;
	u_short answer = 0;

	/*
	 * Our algorithm is simple, using a 32 bit accumulator (sum), we add
	 * sequential 16 bit words to it, and at the end, fold back all the
	 * carry bits from the top 16 bits into the lower 16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1) {
		*(u_char *)(&answer) = *(u_char *)w ;
		sum += answer;
	}

	/* add back carry outs from top 16 bits to low 16 bits */
	sum = (sum >> 16) + (sum & 0xffff);     /* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */

	return (answer);
}
