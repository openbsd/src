/*	$OpenBSD: check_icmp.c,v 1.2 2006/12/16 11:59:12 reyk Exp $	*/

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

#include "hostated.h"

int check_icmp6(struct host *, int, int);
int check_icmp4(struct host *, int, int);
int in_cksum(u_short *, int);

int check_icmp(struct host *host, int s, int s6, int timeout)
{
	if (host->ss.ss_family == AF_INET)
		return (check_icmp4(host, s, timeout));
	else
		return (check_icmp6(host, s6, timeout));
}

int check_icmp6(struct host *host, int s, int timeout)
{
	struct sockaddr		*to;
	struct icmp6_hdr	*icp;
	int			 ident;
	ssize_t			 i;
	int			 cc;
	int			 datalen = (64 - 8);
	u_char			 packet[datalen];
	fd_set			 fdset;
	socklen_t		 len;
	struct timeval		 tv;

	to = (struct sockaddr *)&host->ss;
	ident = getpid() & 0xFFFF;
	len = sizeof(struct sockaddr_in6);

	bzero(&packet, sizeof(packet));
	icp = (struct icmp6_hdr *)packet;
	icp->icmp6_type = ICMP6_ECHO_REQUEST;
	icp->icmp6_code = 0;
	icp->icmp6_seq = 1;
	icp->icmp6_id = ident;

	memset((packet + sizeof(*icp)), 'X', datalen);
	cc = datalen + 8;

	i = sendto(s, packet, cc, 0, to, len);

	if (i < 0 || i != cc) {
		log_warn("check_icmp6: cannot send ping");
		return (HOST_UNKNOWN);
	}

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = timeout % 1000;
	FD_ZERO(&fdset);
	FD_SET(s, &fdset);
	switch (select(s + 1, &fdset, NULL, NULL, &tv)) {
	case -1:
		if (errno == EINTR) {
			log_warnx("check_icmp6: interrupted");
			return (HOST_UNKNOWN);
		} else
			fatal("check_icmp6: select");
	case 0:
		log_debug("check_icmp6: timeout");
		return (HOST_DOWN);
	default:
		bzero(&packet, sizeof(packet));
		i = recvfrom(s, packet, cc, 0, to, &len);
		if (i < 0 || i != cc) {
			log_warn("check_icmp6: did not receive valid ping");
			return (HOST_DOWN);
		}
		icp = (struct icmp6_hdr *)(packet);
		if (icp->icmp6_id != ident) {
			log_warnx("check_icmp6: did not receive valid ident");
			return (HOST_DOWN);
		}
		break;
	}
	return (HOST_UP);
}

int check_icmp4(struct host *host, int s, int timeout)
{
	struct sockaddr		*to;
	struct icmp		*icp;
	int			 ident;
	ssize_t			 i;
	int			 cc;
	int			 datalen = (64 - 8);
	u_char			 packet[datalen];
	fd_set			 fdset;
	socklen_t		 len;
	struct timeval		 tv;

	to = (struct sockaddr *)&host->ss;
	ident = getpid() & 0xFFFF;
	len = sizeof(struct sockaddr_in);

	bzero(&packet, sizeof(packet));
	icp = (struct icmp *)packet;
	icp->icmp_type = htons(ICMP_ECHO);
	icp->icmp_code = 0;
	icp->icmp_seq = htons(1);
	icp->icmp_id = htons(ident);
	icp->icmp_cksum = 0;

	memset(icp->icmp_data, 'X', datalen);
	cc = datalen + 8;
	icp->icmp_cksum = in_cksum((u_short *)icp, cc);

	i = sendto(s, packet, cc, 0, to, len);

	if (i < 0 || i != cc) {
		log_warn("check_icmp4: cannot send ping");
		return (HOST_UNKNOWN);
	}

	tv.tv_sec = timeout / 1000;
	tv.tv_usec = timeout % 1000;
	FD_ZERO(&fdset);
	FD_SET(s, &fdset);
	switch (select(s + 1, &fdset, NULL, NULL, &tv)) {
	case -1:
		if (errno == EINTR) {
			log_warnx("check_icmp4: ping interrupted");
			return (HOST_UNKNOWN);
		} else
			fatal("check_icmp4: select");
	case 0:
		log_debug("check_icmp4: timeout");
		return (HOST_DOWN);
	default:
		bzero(&packet, sizeof(packet));
		i = recvfrom(s, packet, cc, 0, to, &len);
		if (i < 0 || i != cc) {
			log_warn("check_icmp4: did not receive valid ping");
			return (HOST_DOWN);
		}
		icp = (struct icmp *)(packet + sizeof(struct ip));
		if (ntohs(icp->icmp_id) != ident) {
			log_warnx("check_icmp4: did not receive valid ident");
			return (HOST_DOWN);
		}
		break;
	}
	return (HOST_UP);
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
