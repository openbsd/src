/*	$OpenBSD: worker.c,v 1.5 2017/08/03 17:36:06 florian Exp $	*/
/*	$NetBSD: traceroute.c,v 1.10 1995/05/21 15:50:45 mycroft Exp $	*/

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson.
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

#include <arpa/inet.h>
#include <arpa/nameser.h>
#include <endian.h>
#include <err.h>
#include <limits.h>
#include <netdb.h>
#include <netinet/icmp6.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip6.h>
#include <netinet/ip_icmp.h>
#include <netinet/udp.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <time.h>
#include <unistd.h>

#include "traceroute.h"

static u_int8_t	icmp_type = ICMP_ECHO; /* default ICMP code/type */

void
print_exthdr(u_char *buf, int cc)
{
	struct icmp_ext_hdr exthdr;
	struct icmp_ext_obj_hdr objhdr;
	struct ip *ip;
	struct icmp *icp;
	int hlen, first;
	u_int32_t label;
	u_int16_t off, olen;
	u_int8_t type;

	ip = (struct ip *)buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN)
		return;
	icp = (struct icmp *)(buf + hlen);
	cc -= hlen + ICMP_MINLEN;
	buf += hlen + ICMP_MINLEN;

	type = icp->icmp_type;
	if (type != ICMP_TIMXCEED && type != ICMP_UNREACH &&
	    type != ICMP_PARAMPROB)
		/* Wrong ICMP type for extension */
		return;

	off = icp->icmp_length * sizeof(u_int32_t);
	if (off == 0)
		/*
		 * rfc 4884 Section 5.5: traceroute MUST try to parse
		 * broken ext headers. Again IETF bent over to please
		 * idotic corporations.
		 */
		off = ICMP_EXT_OFFSET;
	else if (off < ICMP_EXT_OFFSET)
		/* rfc 4884 requires an offset of at least 128 bytes */
		return;

	/* make sure that at least one extension is present */
	if (cc < off + sizeof(exthdr) + sizeof(objhdr))
		/* Not enough space for ICMP extensions */
		return;

	cc -= off;
	buf += off;
	memcpy(&exthdr, buf, sizeof(exthdr));

	/* verify version */
	if ((exthdr.ieh_version & ICMP_EXT_HDR_VMASK) != ICMP_EXT_HDR_VERSION)
		return;

	/* verify checksum */
	if (exthdr.ieh_cksum && in_cksum((u_short *)buf, cc))
		return;

	buf += sizeof(exthdr);
	cc -= sizeof(exthdr);

	while (cc > sizeof(objhdr)) {
		memcpy(&objhdr, buf, sizeof(objhdr));
		olen = ntohs(objhdr.ieo_length);

		/* Sanity check the length field */
		if (olen < sizeof(objhdr) || olen > cc)
			return;

		cc -= olen;

		/* Move past the object header */
		buf += sizeof(objhdr);
		olen -= sizeof(objhdr);

		switch (objhdr.ieo_cnum) {
		case ICMP_EXT_MPLS:
			/* RFC 4950: ICMP Extensions for MPLS */
			switch (objhdr.ieo_ctype) {
			case 1:
				first = 0;
				while (olen >= sizeof(u_int32_t)) {
					memcpy(&label, buf, sizeof(u_int32_t));
					label = htonl(label);
					buf += sizeof(u_int32_t);
					olen -= sizeof(u_int32_t);

					if (first == 0) {
						printf(" [MPLS Label ");
						first++;
					} else
						printf(", ");
					printf("%d", MPLS_LABEL(label));
					if (MPLS_EXP(label))
						printf(" (Exp %x)",
						    MPLS_EXP(label));
				}
				if (olen > 0) {
					printf("|]");
					return;
				}
				if (first != 0)
					printf("]");
				break;
			default:
				buf += olen;
				break;
			}
			break;
		case ICMP_EXT_IFINFO:
		default:
			buf += olen;
			break;
		}
	}
}

void
check_tos(struct ip *ip, int *last_tos)
{
	struct icmp *icp;
	struct ip *inner_ip;

	icp = (struct icmp *) (((u_char *)ip)+(ip->ip_hl<<2));
	inner_ip = (struct ip *) (((u_char *)icp)+8);

	if (inner_ip->ip_tos != *last_tos)
		printf (" (TOS=%d!)", inner_ip->ip_tos);

	*last_tos = inner_ip->ip_tos;
}

int
wait_for_reply(int sock, struct msghdr *mhdr, int curwaittime)
{
	struct pollfd pfd[1];
	int cc = 0;

	pfd[0].fd = sock;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;

	if (poll(pfd, 1, curwaittime) > 0)
		cc = recvmsg(rcvsock, mhdr, 0);

	return (cc);
}

void
dump_packet(void)
{
	u_char *p;
	int i;

	fprintf(stderr, "packet data:");
	for (p = outpacket, i = 0; i < datalen; i++) {
		if ((i % 24) == 0)
			fprintf(stderr, "\n ");
		fprintf(stderr, " %02x", *p++);
	}
	fprintf(stderr, "\n");
}

void
build_probe4(struct tr_conf *conf, int seq, u_int8_t ttl, int iflag)
{
	struct ip *ip = (struct ip *)outpacket;
	u_char *p = (u_char *)(ip + 1);
	struct udphdr *up = (struct udphdr *)(p + conf->lsrrlen);
	struct icmp *icmpp = (struct icmp *)(p + conf->lsrrlen);
	struct packetdata *op;
	struct timeval tv;

	ip->ip_len = htons(datalen);
	ip->ip_ttl = ttl;
	ip->ip_id = htons(conf->ident+seq);

	switch (conf->proto) {
	case IPPROTO_ICMP:
		icmpp->icmp_type = icmp_type;
		icmpp->icmp_code = ICMP_CODE;
		icmpp->icmp_seq = htons(seq);
		icmpp->icmp_id = htons(conf->ident);
		op = (struct packetdata *)(icmpp + 1);
		break;
	case IPPROTO_UDP:
		up->uh_sport = htons(conf->ident);
		if (iflag)
			up->uh_dport = htons(conf->port+seq);
		else
			up->uh_dport = htons(conf->port);
		up->uh_ulen = htons((u_short)(datalen - sizeof(struct ip) -
		    conf->lsrrlen));
		up->uh_sum = 0;
		op = (struct packetdata *)(up + 1);
		break;
	default:
		op = (struct packetdata *)(ip + 1);
		break;
	}
	op->seq = seq;
	op->ttl = ttl;
	gettime(&tv);

	/*
	 * We don't want hostiles snooping the net to get any useful
	 * information about us. Send the timestamp in network byte order,
	 * and perturb the timestamp enough that they won't know our
	 * real clock ticker. We don't want to perturb the time by too
	 * much: being off by a suspiciously large amount might indicate
	 * OpenBSD.
	 *
	 * The timestamps in the packet are currently unused. If future
	 * work wants to use them they will have to subtract out the
	 * perturbation first.
	 */
	gettime(&tv);
	op->sec = htonl(tv.tv_sec + sec_perturb);
	op->usec = htonl((tv.tv_usec + usec_perturb) % 1000000);

	if (conf->proto == IPPROTO_ICMP && icmp_type == ICMP_ECHO) {
		icmpp->icmp_cksum = 0;
		icmpp->icmp_cksum = in_cksum((u_short *)icmpp,
		    datalen - sizeof(struct ip) - conf->lsrrlen);
		if (icmpp->icmp_cksum == 0)
			icmpp->icmp_cksum = 0xffff;
	}
}

void
build_probe6(struct tr_conf *conf, int seq, u_int8_t hops, int iflag,
    struct sockaddr *to)
{
	struct timeval tv;
	struct packetdata *op;
	int i;

	i = hops;
	if (setsockopt(sndsock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
	    (char *)&i, sizeof(i)) < 0)
		warn("setsockopt IPV6_UNICAST_HOPS");

	if (iflag)
		((struct sockaddr_in6*)to)->sin6_port = htons(conf->port + seq);
	else
		((struct sockaddr_in6*)to)->sin6_port = htons(conf->port);
	gettime(&tv);

	if (conf->proto == IPPROTO_ICMP) {
		struct icmp6_hdr *icp = (struct icmp6_hdr *)outpacket;

		icp->icmp6_type = ICMP6_ECHO_REQUEST;
		icp->icmp6_code = 0;
		icp->icmp6_cksum = 0;
		icp->icmp6_id = conf->ident;
		icp->icmp6_seq = htons(seq);
		op = (struct packetdata *)(outpacket +
		    sizeof(struct icmp6_hdr));
	} else
		op = (struct packetdata *)outpacket;
	op->seq = seq;
	op->ttl = hops;
	op->sec = htonl(tv.tv_sec);
	op->usec = htonl(tv.tv_usec);
}

void
send_probe(struct tr_conf *conf, int seq, u_int8_t ttl, int iflag,
	struct sockaddr *to)
{
	int i;

	switch (to->sa_family) {
	case AF_INET:
		build_probe4(conf, seq, ttl, iflag);
		break;
	case AF_INET6:
		build_probe6(conf, seq, ttl, iflag, to);
		break;
	default:
		errx(1, "unsupported AF: %d", to->sa_family);
		break;
	}

	if (conf->dump)
		dump_packet();

	i = sendto(sndsock, outpacket, datalen, 0, to, to->sa_len);
	if (i < 0 || i != datalen)  {
		if (i < 0)
			warn("sendto");
		printf("%s: wrote %s %d chars, ret=%d\n", __progname, hostname,
		    datalen, i);
		(void) fflush(stdout);
	}
}

double
deltaT(struct timeval *t1p, struct timeval *t2p)
{
	double dt;

	dt = (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 +
	    (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
	return (dt);
}

static char *ttab[] = {
	"Echo Reply",
	"ICMP 1",
	"ICMP 2",
	"Dest Unreachable",
	"Source Quench",
	"Redirect",
	"ICMP 6",
	"ICMP 7",
	"Echo",
	"Router Advert",
	"Router Solicit",
	"Time Exceeded",
	"Param Problem",
	"Timestamp",
	"Timestamp Reply",
	"Info Request",
	"Info Reply",
	"Mask Request",
	"Mask Reply"
};

/*
 * Convert an ICMP "type" field to a printable string.
 */
char *
pr_type(u_int8_t t)
{
	if (t > 18)
		return ("OUT-OF-RANGE");
	return (ttab[t]);
}

int
packet_ok(struct tr_conf *conf, int af, struct msghdr *mhdr, int cc, int seq,
    int iflag)
{
	switch (af) {
	case AF_INET:
		return packet_ok4(conf, mhdr, cc, seq, iflag);
		break;
	case AF_INET6:
		return packet_ok6(conf, mhdr, cc, seq, iflag);
		break;
	default:
		errx(1, "unsupported AF: %d", af);
		break;
	}
}

int
packet_ok4(struct tr_conf *conf, struct msghdr *mhdr, int cc,int seq, int iflag)
{
	struct sockaddr_in *from = (struct sockaddr_in *)mhdr->msg_name;
	struct icmp *icp;
	u_char code;
	char *buf = (char *)mhdr->msg_iov[0].iov_base;
	u_int8_t type;
	int hlen;
	struct ip *ip;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (conf->verbose)
			printf("packet too short (%d bytes) from %s\n", cc,
			    inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
	type = icp->icmp_type;
	code = icp->icmp_code;
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH || type == ICMP_ECHOREPLY) {
		struct ip *hip;
		struct udphdr *up;
		struct icmp *icmpp;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;

		switch (conf->proto) {
		case IPPROTO_ICMP:
			if (icmp_type == ICMP_ECHO &&
			    type == ICMP_ECHOREPLY &&
			    icp->icmp_id == htons(conf->ident) &&
			    icp->icmp_seq == htons(seq))
				return (-2); /* we got there */

			icmpp = (struct icmp *)((u_char *)hip + hlen);
			if (hlen + 8 <= cc && hip->ip_p == IPPROTO_ICMP &&
			    icmpp->icmp_id == htons(conf->ident) &&
			    icmpp->icmp_seq == htons(seq))
				return (type == ICMP_TIMXCEED? -1 : code + 1);
			break;

		case IPPROTO_UDP:
			up = (struct udphdr *)((u_char *)hip + hlen);
			if (hlen + 12 <= cc && hip->ip_p == conf->proto &&
			    up->uh_sport == htons(conf->ident) &&
			    ((iflag && up->uh_dport == htons(conf->port +
			    seq)) ||
			    (!iflag && up->uh_dport == htons(conf->port))))
				return (type == ICMP_TIMXCEED? -1 : code + 1);
			break;
		default:
			/* this is some odd, user specified proto,
			 * how do we check it?
			 */
			if (hip->ip_p == conf->proto)
				return (type == ICMP_TIMXCEED? -1 : code + 1);
		}
	}
	if (conf->verbose) {
		int i;
		in_addr_t *lp = (in_addr_t *)&icp->icmp_ip;

		printf("\n%d bytes from %s", cc, inet_ntoa(from->sin_addr));
		printf(" to %s", inet_ntoa(ip->ip_dst));
		printf(": icmp type %u (%s) code %d\n", type, pr_type(type),
		    icp->icmp_code);
		for (i = 4; i < cc ; i += sizeof(in_addr_t))
			printf("%2d: x%8.8lx\n", i, (unsigned long)*lp++);
	}
	return (0);
}

int
packet_ok6(struct tr_conf *conf, struct msghdr *mhdr, int cc, int seq,
    int iflag)
{
	struct icmp6_hdr *icp;
	struct sockaddr_in6 *from = (struct sockaddr_in6 *)mhdr->msg_name;
	u_char type, code;
	char *buf = (char *)mhdr->msg_iov[0].iov_base;
	struct cmsghdr *cm;
	int *hlimp;
	char hbuf[NI_MAXHOST];
	int useicmp = (conf->proto == IPPROTO_ICMP);

	if (cc < sizeof(struct icmp6_hdr)) {
		if (conf->verbose) {
			if (getnameinfo((struct sockaddr *)from, from->sin6_len,
			    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
				strlcpy(hbuf, "invalid", sizeof(hbuf));
			printf("data too short (%d bytes) from %s\n", cc, hbuf);
		}
		return(0);
	}
	icp = (struct icmp6_hdr *)buf;
	/* get optional information via advanced API */
	rcvpktinfo = NULL;
	hlimp = NULL;
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	    cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len ==
		    CMSG_LEN(sizeof(struct in6_pktinfo)))
			rcvpktinfo = (struct in6_pktinfo *)(CMSG_DATA(cm));

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
	}
	if (rcvpktinfo == NULL || hlimp == NULL) {
		warnx("failed to get received hop limit or packet info");
		rcvhlim = 0;	/*XXX*/
	} else
		rcvhlim = *hlimp;

	type = icp->icmp6_type;
	code = icp->icmp6_code;
	if ((type == ICMP6_TIME_EXCEEDED && code == ICMP6_TIME_EXCEED_TRANSIT)
	    || type == ICMP6_DST_UNREACH) {
		struct ip6_hdr *hip;
		struct udphdr *up;

		hip = (struct ip6_hdr *)(icp + 1);
		if ((up = get_udphdr(conf, hip, (u_char *)(buf + cc))) ==
		    NULL) {
			if (conf->verbose)
				warnx("failed to get upper layer header");
			return(0);
		}
		if (useicmp &&
		    ((struct icmp6_hdr *)up)->icmp6_id == conf->ident &&
		    ((struct icmp6_hdr *)up)->icmp6_seq == htons(seq))
			return (type == ICMP6_TIME_EXCEEDED ? -1 : code + 1);
		else if (!useicmp &&
		    up->uh_sport == htons(srcport) &&
		    ((iflag && up->uh_dport == htons(conf->port + seq)) ||
		    (!iflag && up->uh_dport == htons(conf->port))))
			return (type == ICMP6_TIME_EXCEEDED ? -1 : code + 1);
	} else if (useicmp && type == ICMP6_ECHO_REPLY) {
		if (icp->icmp6_id == conf->ident &&
		    icp->icmp6_seq == htons(seq))
			return (ICMP6_DST_UNREACH_NOPORT + 1);
	}
	if (conf->verbose) {
		char sbuf[NI_MAXHOST], dbuf[INET6_ADDRSTRLEN];
		u_int8_t *p;
		int i;

		if (getnameinfo((struct sockaddr *)from, from->sin6_len,
		    sbuf, sizeof(sbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(sbuf, "invalid", sizeof(sbuf));
		printf("\n%d bytes from %s to %s", cc, sbuf,
		    rcvpktinfo ? inet_ntop(AF_INET6, &rcvpktinfo->ipi6_addr,
		    dbuf, sizeof(dbuf)) : "?");
		printf(": icmp type %d (%s) code %d\n", type, pr_type(type),
		    icp->icmp6_code);
		p = (u_int8_t *)(icp + 1);
#define WIDTH	16
		for (i = 0; i < cc; i++) {
			if (i % WIDTH == 0)
				printf("%04x:", i);
			if (i % 4 == 0)
				printf(" ");
			printf("%02x", p[i]);
			if (i % WIDTH == WIDTH - 1)
				printf("\n");
		}
		if (cc % WIDTH != 0)
			printf("\n");
	}
	return(0);
}

void
print(struct tr_conf *conf, struct sockaddr *from, int cc, const char *to)
{
	char hbuf[NI_MAXHOST];
	if (getnameinfo(from, from->sa_len,
	    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
		strlcpy(hbuf, "invalid", sizeof(hbuf));
	if (conf->nflag)
		printf(" %s", hbuf);
	else
		printf(" %s (%s)", inetname(from), hbuf);

	if (conf->Aflag)
		print_asn((struct sockaddr_storage *)from);

	if (conf->verbose)
		printf(" %d bytes to %s", cc, to);
}

/*
 * Increment pointer until find the UDP or ICMP header.
 */
struct udphdr *
get_udphdr(struct tr_conf *conf, struct ip6_hdr *ip6, u_char *lim)
{
	u_char *cp = (u_char *)ip6, nh;
	int hlen;
	int useicmp = (conf->proto == IPPROTO_ICMP);

	if (cp + sizeof(*ip6) >= lim)
		return(NULL);

	nh = ip6->ip6_nxt;
	cp += sizeof(struct ip6_hdr);

	while (lim - cp >= 8) {
		switch (nh) {
		case IPPROTO_ESP:
		case IPPROTO_TCP:
			return(NULL);
		case IPPROTO_ICMPV6:
			return(useicmp ? (struct udphdr *)cp : NULL);
		case IPPROTO_UDP:
			return(useicmp ? NULL : (struct udphdr *)cp);
		case IPPROTO_FRAGMENT:
			hlen = sizeof(struct ip6_frag);
			nh = ((struct ip6_frag *)cp)->ip6f_nxt;
			break;
		case IPPROTO_AH:
			hlen = (((struct ip6_ext *)cp)->ip6e_len + 2) << 2;
			nh = ((struct ip6_ext *)cp)->ip6e_nxt;
			break;
		default:
			hlen = (((struct ip6_ext *)cp)->ip6e_len + 1) << 3;
			nh = ((struct ip6_ext *)cp)->ip6e_nxt;
			break;
		}

		cp += hlen;
	}

	return(NULL);
}

void
icmp_code(int af, int code, int *got_there, int *unreachable)
{
	switch (af) {
	case AF_INET:
		icmp4_code(code, got_there, unreachable);
		break;
	case AF_INET6:
		icmp6_code(code, got_there, unreachable);
		break;
	default:
		errx(1, "unsupported AF: %d", af);
		break;
	}
}

void
icmp4_code(int code, int *got_there, int *unreachable)
{
	struct ip *ip = (struct ip *)packet;

	switch (code) {
	case ICMP_UNREACH_PORT:
		if (ip->ip_ttl <= 1)
			printf(" !");
		++(*got_there);
		break;
	case ICMP_UNREACH_NET:
		++(*unreachable);
		printf(" !N");
		break;
	case ICMP_UNREACH_HOST:
		++(*unreachable);
		printf(" !H");
		break;
	case ICMP_UNREACH_PROTOCOL:
		++(*got_there);
		printf(" !P");
		break;
	case ICMP_UNREACH_NEEDFRAG:
		++(*unreachable);
		printf(" !F");
		break;
	case ICMP_UNREACH_SRCFAIL:
		++(*unreachable);
		printf(" !S");
		break;
	case ICMP_UNREACH_FILTER_PROHIB:
		++(*unreachable);
		printf(" !X");
		break;
	case ICMP_UNREACH_NET_PROHIB: /*misuse*/
		++(*unreachable);
		printf(" !A");
		break;
	case ICMP_UNREACH_HOST_PROHIB:
		++(*unreachable);
		printf(" !C");
		break;
	case ICMP_UNREACH_NET_UNKNOWN:
	case ICMP_UNREACH_HOST_UNKNOWN:
		++(*unreachable);
		printf(" !U");
		break;
	case ICMP_UNREACH_ISOLATED:
		++(*unreachable);
		printf(" !I");
		break;
	case ICMP_UNREACH_TOSNET:
	case ICMP_UNREACH_TOSHOST:
		++(*unreachable);
		printf(" !T");
		break;
	default:
		++(*unreachable);
		printf(" !<%d>", code);
		break;
	}
}

void
icmp6_code(int code, int *got_there, int *unreachable)
{
	switch (code) {
	case ICMP6_DST_UNREACH_NOROUTE:
		++(*unreachable);
		printf(" !N");
		break;
	case ICMP6_DST_UNREACH_ADMIN:
		++(*unreachable);
		printf(" !P");
		break;
	case ICMP6_DST_UNREACH_BEYONDSCOPE:
		++(*unreachable);
		printf(" !S");
		break;
	case ICMP6_DST_UNREACH_ADDR:
		++(*unreachable);
		printf(" !A");
		break;
	case ICMP6_DST_UNREACH_NOPORT:
		if (rcvhlim >= 0 && rcvhlim <= 1)
			printf(" !");
		++(*got_there);
		break;
	default:
		++(*unreachable);
		printf(" !<%d>", code);
		break;
	}
}

/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */
u_short
in_cksum(u_short *addr, int len)
{
	u_short *w = addr, answer;
	int nleft = len, sum = 0;

	/*
	 *  Our algorithm is simple, using a 32 bit accumulator (sum),
	 *  we add sequential 16 bit words to it, and at the end, fold
	 *  back all the carry bits from the top 16 bits into the lower
	 *  16 bits.
	 */
	while (nleft > 1)  {
		sum += *w++;
		nleft -= 2;
	}

	/* mop up an odd byte, if necessary */
	if (nleft == 1)
		sum += *(u_char *)w;

	/*
	 * add back carry outs from top 16 bits to low 16 bits
	 */
	sum = (sum >> 16) + (sum & 0xffff);	/* add hi 16 to low 16 */
	sum += (sum >> 16);			/* add carry */
	answer = ~sum;				/* truncate to 16 bits */
	return (answer);
}

/*
 * Construct an Internet address representation.
 */
const char *
inetname(struct sockaddr *sa)
{
	static char line[NI_MAXHOST], domain[HOST_NAME_MAX + 1];
	static int first = 1;
	char *cp;

	if (first) {
		first = 0;
		if (gethostname(domain, sizeof(domain)) == 0 &&
		    (cp = strchr(domain, '.')) != NULL)
			memmove(domain, cp + 1, strlen(cp + 1) + 1);
		else
			domain[0] = 0;
	}
	if (getnameinfo(sa, sa->sa_len, line, sizeof(line), NULL, 0,
	    NI_NAMEREQD) == 0) {
		if ((cp = strchr(line, '.')) != NULL && strcmp(cp + 1,
		    domain) == 0)
			*cp = '\0';
		return (line);
	}

	if (getnameinfo(sa, sa->sa_len, line, sizeof(line), NULL, 0,
	    NI_NUMERICHOST) != 0)
		return ("invalid");
	return (line);
}

void
print_asn(struct sockaddr_storage *ss)
{
	struct rrsetinfo *answers = NULL;
	int counter;
	const u_char *uaddr;
	char qbuf[MAXDNAME];

	switch (ss->ss_family) {
	case AF_INET:
		uaddr = (const u_char *)&((struct sockaddr_in *) ss)->sin_addr;
		if (snprintf(qbuf, sizeof qbuf, "%u.%u.%u.%u."
		    "origin.asn.cymru.com",
		    (uaddr[3] & 0xff), (uaddr[2] & 0xff),
		    (uaddr[1] & 0xff), (uaddr[0] & 0xff)) >= sizeof (qbuf))
			return;
		break;
	case AF_INET6:
		uaddr = (const u_char *)&((struct sockaddr_in6 *) ss)->sin6_addr;
		if (snprintf(qbuf, sizeof qbuf,
		    "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		    "%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x.%x."
		    "origin6.asn.cymru.com",
		    (uaddr[15] & 0x0f), ((uaddr[15] >>4)& 0x0f),
		    (uaddr[14] & 0x0f), ((uaddr[14] >>4)& 0x0f),
		    (uaddr[13] & 0x0f), ((uaddr[13] >>4)& 0x0f),
		    (uaddr[12] & 0x0f), ((uaddr[12] >>4)& 0x0f),
		    (uaddr[11] & 0x0f), ((uaddr[11] >>4)& 0x0f),
		    (uaddr[10] & 0x0f), ((uaddr[10] >>4)& 0x0f),
		    (uaddr[9] & 0x0f), ((uaddr[9] >>4)& 0x0f),
		    (uaddr[8] & 0x0f), ((uaddr[8] >>4)& 0x0f),
		    (uaddr[7] & 0x0f), ((uaddr[7] >>4)& 0x0f),
		    (uaddr[6] & 0x0f), ((uaddr[6] >>4)& 0x0f),
		    (uaddr[5] & 0x0f), ((uaddr[5] >>4)& 0x0f),
		    (uaddr[4] & 0x0f), ((uaddr[4] >>4)& 0x0f),
		    (uaddr[3] & 0x0f), ((uaddr[3] >>4)& 0x0f),
		    (uaddr[2] & 0x0f), ((uaddr[2] >>4)& 0x0f),
		    (uaddr[1] & 0x0f), ((uaddr[1] >>4)& 0x0f),
		    (uaddr[0] & 0x0f), ((uaddr[0] >>4)& 0x0f)) >= sizeof (qbuf))
			return;
		break;
	default:
		return;
	}

	if (getrrsetbyname(qbuf, C_IN, T_TXT, 0, &answers) != 0)
		return;
	for (counter = 0; counter < answers->rri_nrdatas; counter++) {
		char *p, *as = answers->rri_rdatas[counter].rdi_data;
		as++; /* skip first byte, it contains length */
		if ((p = strchr(as,'|'))) {
			printf(counter ? ", " : " [");
			p[-1] = 0;
			printf("AS%s", as);
		}
	}
	if (counter)
		printf("]");

	freerrset(answers);
}

int
map_tos(char *s, int *val)
{
	/* DiffServ Codepoints and other TOS mappings */
	const struct toskeywords {
		const char	*keyword;
		int		 val;
	} *t, toskeywords[] = {
		{ "af11",		IPTOS_DSCP_AF11 },
		{ "af12",		IPTOS_DSCP_AF12 },
		{ "af13",		IPTOS_DSCP_AF13 },
		{ "af21",		IPTOS_DSCP_AF21 },
		{ "af22",		IPTOS_DSCP_AF22 },
		{ "af23",		IPTOS_DSCP_AF23 },
		{ "af31",		IPTOS_DSCP_AF31 },
		{ "af32",		IPTOS_DSCP_AF32 },
		{ "af33",		IPTOS_DSCP_AF33 },
		{ "af41",		IPTOS_DSCP_AF41 },
		{ "af42",		IPTOS_DSCP_AF42 },
		{ "af43",		IPTOS_DSCP_AF43 },
		{ "critical",		IPTOS_PREC_CRITIC_ECP },
		{ "cs0",		IPTOS_DSCP_CS0 },
		{ "cs1",		IPTOS_DSCP_CS1 },
		{ "cs2",		IPTOS_DSCP_CS2 },
		{ "cs3",		IPTOS_DSCP_CS3 },
		{ "cs4",		IPTOS_DSCP_CS4 },
		{ "cs5",		IPTOS_DSCP_CS5 },
		{ "cs6",		IPTOS_DSCP_CS6 },
		{ "cs7",		IPTOS_DSCP_CS7 },
		{ "ef",			IPTOS_DSCP_EF },
		{ "inetcontrol",	IPTOS_PREC_INTERNETCONTROL },
		{ "lowdelay",		IPTOS_LOWDELAY },
		{ "netcontrol",		IPTOS_PREC_NETCONTROL },
		{ "reliability",	IPTOS_RELIABILITY },
		{ "throughput",		IPTOS_THROUGHPUT },
		{ NULL,			-1 },
	};

	for (t = toskeywords; t->keyword != NULL; t++) {
		if (strcmp(s, t->keyword) == 0) {
			*val = t->val;
			return (1);
		}
	}

	return (0);
}

void
gettime(struct timeval *tv)
{
	struct timespec ts;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
		err(1, "clock_gettime(CLOCK_MONOTONIC)");

	TIMESPEC_TO_TIMEVAL(tv, &ts);
}
