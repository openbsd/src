/*	$OpenBSD: traceroute.c,v 1.26 1998/07/09 06:32:27 deraadt Exp $	*/
/*	$NetBSD: traceroute.c,v 1.10 1995/05/21 15:50:45 mycroft Exp $	*/

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
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
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

#ifndef lint
static char copyright[] =
"@(#) Copyright (c) 1990, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static char sccsid[] = "@(#)traceroute.c	8.1 (Berkeley) 6/6/93";*/
#else
static char rcsid[] = "$NetBSD: traceroute.c,v 1.10 1995/05/21 15:50:45 mycroft Exp $";
#endif
#endif /* not lint */

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 *
 * Attempt to trace the route an ip packet would follow to some
 * internet host.  We find out intermediate hops by launching probe
 * packets with a small ttl (time to live) then listening for an
 * icmp "time exceeded" reply from a gateway.  We start our probes
 * with a ttl of one and increase by one until we get an icmp "port
 * unreachable" (which means we got to "host") or hit a max (which
 * defaults to 30 hops & can be changed with the -m flag).  Three
 * probes (change with -q flag) are sent at each ttl setting and a
 * line is printed showing the ttl, address of the gateway and
 * round trip time of each probe.  If the probe answers come from
 * different gateways, the address of each responding system will
 * be printed.  If there is no response within a 5 sec. timeout
 * interval (changed with the -w flag), a "*" is printed for that
 * probe.
 *
 * Probe packets are UDP format.  We don't want the destination
 * host to process them so the destination port is set to an
 * unlikely value (if some clod on the destination is using that
 * value, it can be changed with the -p flag).
 *
 * A sample use might be:
 *
 *     [yak 71]% traceroute nis.nsf.net.
 *     traceroute to nis.nsf.net (35.1.1.48), 30 hops max, 56 byte packet
 *      1  helios.ee.lbl.gov (128.3.112.1)  19 ms  19 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  39 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  40 ms  59 ms  59 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  59 ms
 *      8  129.140.70.13 (129.140.70.13)  99 ms  99 ms  80 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  239 ms  319 ms
 *     10  129.140.81.7 (129.140.81.7)  220 ms  199 ms  199 ms
 *     11  nic.merit.edu (35.1.1.48)  239 ms  239 ms  239 ms
 *
 * Note that lines 2 & 3 are the same.  This is due to a buggy
 * kernel on the 2nd hop system -- lbl-csam.arpa -- that forwards
 * packets with a zero ttl.
 *
 * A more interesting example is:
 *
 *     [yak 72]% traceroute allspice.lcs.mit.edu.
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 30 hops max
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  19 ms  19 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  19 ms  39 ms  39 ms
 *      5  ccn-nerif22.Berkeley.EDU (128.32.168.22)  20 ms  39 ms  39 ms
 *      6  128.32.197.4 (128.32.197.4)  59 ms  119 ms  39 ms
 *      7  131.119.2.5 (131.119.2.5)  59 ms  59 ms  39 ms
 *      8  129.140.70.13 (129.140.70.13)  80 ms  79 ms  99 ms
 *      9  129.140.71.6 (129.140.71.6)  139 ms  139 ms  159 ms
 *     10  129.140.81.7 (129.140.81.7)  199 ms  180 ms  300 ms
 *     11  129.140.72.17 (129.140.72.17)  300 ms  239 ms  239 ms
 *     12  * * *
 *     13  128.121.54.72 (128.121.54.72)  259 ms  499 ms  279 ms
 *     14  * * *
 *     15  * * *
 *     16  * * *
 *     17  * * *
 *     18  ALLSPICE.LCS.MIT.EDU (18.26.0.115)  339 ms  279 ms  279 ms
 *
 * (I start to see why I'm having so much trouble with mail to
 * MIT.)  Note that the gateways 12, 14, 15, 16 & 17 hops away
 * either don't send ICMP "time exceeded" messages or send them
 * with a ttl too small to reach us.  14 - 17 are running the
 * MIT C Gateway code that doesn't send "time exceeded"s.  God
 * only knows what's going on with 12.
 *
 * The silent gateway 12 in the above may be the result of a bug in
 * the 4.[23]BSD network code (and its derivatives):  4.x (x <= 3)
 * sends an unreachable message using whatever ttl remains in the
 * original datagram.  Since, for gateways, the remaining ttl is
 * zero, the icmp "time exceeded" is guaranteed to not make it back
 * to us.  The behavior of this bug is slightly more interesting
 * when it appears on the destination system:
 *
 *      1  helios.ee.lbl.gov (128.3.112.1)  0 ms  0 ms  0 ms
 *      2  lilac-dmc.Berkeley.EDU (128.32.216.1)  39 ms  19 ms  39 ms
 *      3  lilac-dmc.Berkeley.EDU (128.32.216.1)  19 ms  39 ms  19 ms
 *      4  ccngw-ner-cc.Berkeley.EDU (128.32.136.23)  39 ms  40 ms  19 ms
 *      5  ccn-nerif35.Berkeley.EDU (128.32.168.35)  39 ms  39 ms  39 ms
 *      6  csgw.Berkeley.EDU (128.32.133.254)  39 ms  59 ms  39 ms
 *      7  * * *
 *      8  * * *
 *      9  * * *
 *     10  * * *
 *     11  * * *
 *     12  * * *
 *     13  rip.Berkeley.EDU (128.32.131.22)  59 ms !  39 ms !  39 ms !
 *
 * Notice that there are 12 "gateways" (13 is the final
 * destination) and exactly the last half of them are "missing".
 * What's really happening is that rip (a Sun-3 running Sun OS3.5)
 * is using the ttl from our arriving datagram as the ttl in its
 * icmp reply.  So, the reply will time out on the return path
 * (with no notice sent to anyone since icmp's aren't sent for
 * icmp's) until we probe with a ttl that's at least twice the path
 * length.  I.e., rip is really only 7 hops away.  A reply that
 * returns with a ttl of 1 is a clue this problem exists.
 * Traceroute prints a "!" after the time if the ttl is <= 1.
 * Since vendors ship a lot of obsolete (DEC's Ultrix, Sun 3.x) or
 * non-standard (HPUX) software, expect to see this problem
 * frequently and/or take care picking the target host of your
 * probes.
 *
 * Other possible annotations after the time are !H, !N, !P (got a host,
 * network or protocol unreachable, respectively), !S or !F (source
 * route failed or fragmentation needed -- neither of these should
 * ever occur and the associated gateway is busted if you see one).  If
 * almost all the probes result in some kind of unreachable, traceroute
 * will give up and exit.
 *
 * Notes
 * -----
 * This program must be run by root or be setuid.  (I suggest that
 * you *don't* make it setuid -- casual use could result in a lot
 * of unnecessary traffic on our poor, congested nets.)
 *
 * This program requires a kernel mod that does not appear in any
 * system available from Berkeley:  A raw ip socket using proto
 * IPPROTO_RAW must interpret the data sent as an ip datagram (as
 * opposed to data to be wrapped in a ip datagram).  See the README
 * file that came with the source to this program for a description
 * of the mods I made to /sys/netinet/raw_ip.c.  Your mileage may
 * vary.  But, again, ANY 4.x (x < 4) BSD KERNEL WILL HAVE TO BE
 * MODIFIED TO RUN THIS PROGRAM.
 *
 * The udp port usage may appear bizarre (well, ok, it is bizarre).
 * The problem is that an icmp message only contains 8 bytes of
 * data from the original datagram.  8 bytes is the size of a udp
 * header so, if we want to associate replies with the original
 * datagram, the necessary information must be encoded into the
 * udp header (the ip id could be used but there's no way to
 * interlock with the kernel's assignment of ip id's and, anyway,
 * it would have taken a lot more kernel hacking to allow this
 * code to set the ip id).  So, to allow two or more users to
 * use traceroute simultaneously, we use this task's pid as the
 * source port (the high bit is set to move the port number out
 * of the "likely" range).  To keep track of which probe is being
 * replied to (so times and/or hop counts don't get confused by a
 * reply that was delayed in transit), we increment the destination
 * port number before each probe.
 *
 * Don't use this as a coding example.  I was trying to find a
 * routing problem and this code sort-of popped out after 48 hours
 * without sleep.  I was amazed it ever compiled, much less ran.
 *
 * I stole the idea for this program from Steve Deering.  Since
 * the first release, I've learned that had I attended the right
 * IETF working group meetings, I also could have stolen it from Guy
 * Almes or Matt Mathis.  I don't know (or care) who came up with
 * the idea first.  I envy the originators' perspicacity and I'm
 * glad they didn't keep the idea a secret.
 *
 * Tim Seaver, Ken Adelman and C. Philip Wood provided bug fixes and/or
 * enhancements to the original distribution.
 *
 * I've hacked up a round-trip-route version of this that works by
 * sending a loose-source-routed udp datagram through the destination
 * back to yourself.  Unfortunately, SO many gateways botch source
 * routing, the thing is almost worthless.  Maybe one day...
 *
 *  -- Van Jacobson (van@helios.ee.lbl.gov)
 *     Tue Dec 20 03:50:13 PST 1988
 */

#include <sys/param.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <sys/file.h>
#include <sys/ioctl.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>

#include <arpa/inet.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define Fprintf (void)fprintf
#define Sprintf (void)sprintf
#define Printf (void)printf

#define	HEADERSIZE	(sizeof(struct ip) + lsrrlen + sizeof(struct udphdr) + sizeof(struct packetdata))
#define	MAX_LSRR	((MAX_IPOPTLEN - 4) / 4)

/*
 * Format of the data in a (udp) probe packet.
 */
struct packetdata {
	u_char seq;		/* sequence number of this packet */
	u_char ttl;		/* ttl packet left with */
	u_int32_t sec;		/* time packet left */
	u_int32_t usec;
};

struct in_addr gateway[MAX_LSRR + 1];
int lsrrlen = 0;
int32_t sec_perturb;
int32_t usec_perturb;

u_char packet[512], *outpacket;	/* last inbound (icmp) packet */

int wait_for_reply __P((int, struct sockaddr_in *, struct timeval *));
void send_probe __P((int, int, struct sockaddr_in *));
int packet_ok __P((u_char *, int, struct sockaddr_in *, int));
void print __P((u_char *, int, struct sockaddr_in *));
char *inetname __P((struct in_addr));
void usage __P((void));

int s;				/* receive (icmp) socket file descriptor */
int sndsock;			/* send (udp) socket file descriptor */
struct timezone tz;		/* leftover */

int datalen;			/* How much data */

char *source = 0;
char *hostname;

int nprobes = 3;
int max_ttl = 30;
u_short ident;
u_short port = 32768+666;	/* start udp dest port # for probe packets */
u_char	proto = IPPROTO_UDP;
int options;			/* socket options */
int verbose;
int waittime = 5;		/* time to wait for response (in seconds) */
int nflag;			/* print addresses numerically */
int dump;

int
main(argc, argv)
	int argc;
	char *argv[];
{
	struct hostent *hp;
	struct protoent *pe;
	struct sockaddr_in from, to;
	int ch, i, lsrr, on, probe, seq, tos, ttl, ttl_flag;
	struct ip *ip;
	u_int32_t tmprnd;

	if ((pe = getprotobyname("icmp")) == NULL) {
		Fprintf(stderr, "icmp: unknown protocol\n");
		exit(10);
	}
	if ((s = socket(AF_INET, SOCK_RAW, pe->p_proto)) < 0)
		err(5, "icmp socket");
	if ((sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		err(5, "raw socket");

	/* revoke privs */
	seteuid(getuid());
	setuid(getuid());

	ttl_flag = 0;
	lsrr = 0;
	on = 1;
	seq = tos = 0;
	while ((ch = getopt(argc, argv, "dDg:m:np:q:rs:t:w:vlP:")) != -1)
		switch (ch) {
		case 'd':
			options |= SO_DEBUG;
			break;
		case 'D':
			dump = 1;
			break;
		case 'g':
			if (lsrr >= MAX_LSRR)
				errx(1, "too many gateways; max %d", MAX_LSRR);
			if (inet_aton(optarg, &gateway[lsrr]) == 0) {
				hp = gethostbyname(optarg);
				if (hp == 0)
					errx(1, "unknown host %s", optarg);
				memcpy(&gateway[lsrr], hp->h_addr, hp->h_length);
			}
			if (++lsrr == 1)
				lsrrlen = 4;
			lsrrlen += 4;
			break;
		case 'l':
			ttl_flag++;
			break;
		case 'm':
			max_ttl = atoi(optarg);
			if (max_ttl < 1 || max_ttl > MAXTTL)
				errx(1, "max ttl must be 1 to %d.", MAXTTL);
			break;
		case 'n':
			nflag++;
			break;
		case 'p':
			port = atoi(optarg);
			if (port < 1)
				errx(1, "port must be >0.");
			break;
		case 'P':
			proto = atoi(optarg);
			if (proto <= 1) {
				struct protoent *pent;
				pent = getprotobyname(optarg);
				if (pent)
					proto = pent->p_proto;
				else
					errx(1, "proto must be >=0, or a name.");
			}
			break;
		case 'q':
			nprobes = atoi(optarg);
			if (nprobes < 1)
				errx(1, "nprobes must be >0.");
			break;
		case 'r':
			options |= SO_DONTROUTE;
			break;
		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;
		case 't':
			tos = atoi(optarg);
			if (tos < 0 || tos > 255)
				errx(1, "tos must be 0 to 255.");
			break;
		case 'v':
			verbose++;
			break;
		case 'w':
			waittime = atoi(optarg);
			if (waittime <= 1)
				errx(1, "wait must be >1 sec.");
			break;
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1)
		usage();

	setlinebuf (stdout);

	(void) memset(&to, 0, sizeof(struct sockaddr));
	to.sin_family = AF_INET;
	if (inet_aton(*argv, &to.sin_addr) != 0)
		hostname = *argv;
	else {
		hp = gethostbyname(*argv);
		if (hp == 0)
			errx(1, "unknown host %s", *argv);
		to.sin_family = hp->h_addrtype;
		memcpy(&to.sin_addr, hp->h_addr, hp->h_length);
		if ((hostname = strdup(hp->h_name)) == NULL)
			err(1, "malloc");
	}
	if (*++argv)
		datalen = atoi(*argv);
	if (datalen < 0 || datalen > IP_MAXPACKET - HEADERSIZE)
		errx(1, "packet size must be 0 to %d.",
		    IP_MAXPACKET - HEADERSIZE);
	datalen += HEADERSIZE;

	outpacket = (u_char *)malloc(datalen);
	if (outpacket == 0)
		err(1, "malloc");
	(void) memset(outpacket, 0, datalen);

	ip = (struct ip *)outpacket;
	if (lsrr != 0) {
		u_char *p;
		p = (u_char *)(ip + 1);
		*p++ = IPOPT_NOP;
		*p++ = IPOPT_LSRR;
		*p++ = lsrrlen - 1;
		*p++ = IPOPT_MINOFF;
		gateway[lsrr] = to.sin_addr;
		for (i = 1; i <= lsrr; i++)
			*((struct in_addr *)p)++ = gateway[i];
		ip->ip_dst = gateway[0];
	} else
		ip->ip_dst = to.sin_addr;
	ip->ip_off = htons(0);
	ip->ip_hl = (sizeof(struct ip) + lsrrlen) >> 2;
	ip->ip_p = proto;
	ip->ip_v = IPVERSION;
	ip->ip_tos = tos;

	ident = (getpid() & 0xffff) | 0x8000;
	tmprnd = arc4random();
	sec_perturb = (tmprnd & 0x80000000) ? -(tmprnd & 0x7ff) :
						(tmprnd & 0x7ff);
	usec_perturb = arc4random();

	if (options & SO_DEBUG)
		(void) setsockopt(s, SOL_SOCKET, SO_DEBUG,
				  (char *)&on, sizeof(on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(s, SOL_SOCKET, SO_DONTROUTE,
				  (char *)&on, sizeof(on));
#ifdef SO_SNDBUF
	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&datalen,
		       sizeof(datalen)) < 0)
		err(6, "SO_SNDBUF");
#endif SO_SNDBUF
#ifdef IP_HDRINCL
	if (setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *)&on,
		       sizeof(on)) < 0)
		err(6, "IP_HDRINCL");
#endif IP_HDRINCL
	if (options & SO_DEBUG)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG,
				  (char *)&on, sizeof(on));
	if (options & SO_DONTROUTE)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DONTROUTE,
				  (char *)&on, sizeof(on));

	if (source) {
		(void) memset(&from, 0, sizeof(struct sockaddr));
		from.sin_family = AF_INET;
		if (inet_aton(source, &from.sin_addr) == 0)
			errx(1, "unknown host %s", source);
		ip->ip_src = from.sin_addr;
#ifndef IP_HDRINCL
		if (bind(sndsock, (struct sockaddr *)&from, sizeof(from)) < 0)
			err(1, "bind");
#endif IP_HDRINCL
	}

	Fprintf(stderr, "traceroute to %s (%s)", hostname,
		inet_ntoa(to.sin_addr));
	if (source)
		Fprintf(stderr, " from %s", source);
	Fprintf(stderr, ", %d hops max, %d byte packets\n", max_ttl, datalen);
	(void) fflush(stderr);

	for (ttl = 1; ttl <= max_ttl; ++ttl) {
		in_addr_t lastaddr = 0;
		int got_there = 0;
		int unreachable = 0;
		int timeout = 0;
		quad_t dt;

		Printf("%2d ", ttl);
		for (probe = 0; probe < nprobes; ++probe) {
			int cc;
			struct timeval t1, t2;
			struct timezone tz;
			int code;

			(void) gettimeofday(&t1, &tz);
			send_probe(++seq, ttl, &to);
			while (cc = wait_for_reply(s, &from, &t1)) {
				(void) gettimeofday(&t2, &tz);
				if (t2.tv_sec - t1.tv_sec > waittime) {
					cc = 0;
					break;
				}
				i = packet_ok(packet, cc, &from, seq);
				/* Skip short packet */
				if (i == 0)
					continue;
				if (from.sin_addr.s_addr != lastaddr) {
					print(packet, cc, &from);
					lastaddr = from.sin_addr.s_addr;
				}
				dt = (quad_t)(t2.tv_sec - t1.tv_sec) * 1000000
					+ (quad_t)(t2.tv_usec - t1.tv_usec);
				Printf("  %u", (u_int)(dt / 1000));
				if (dt % 1000)
					Printf(".%u", (u_int)(dt % 1000));
				Printf(" ms");
				ip = (struct ip *)packet;
				if (ttl_flag)
					Printf(" (%d)", ip->ip_ttl);
				if (i == -2) {
#ifndef ARCHAIC
					ip = (struct ip *)packet;
					if (ip->ip_ttl <= 1)
						Printf(" !");
#endif
					++got_there;
					break;
				}
				/* time exceeded in transit */
				if (i == -1)
					break;
				code = i - 1;
				switch (code) {
				case ICMP_UNREACH_PORT:
#ifndef ARCHAIC
					ip = (struct ip *)packet;
					if (ip->ip_ttl <= 1)
						Printf(" !");
#endif ARCHAIC
					++got_there;
					break;
				case ICMP_UNREACH_NET:
					++unreachable;
					Printf(" !N");
					break;
				case ICMP_UNREACH_HOST:
					++unreachable;
					Printf(" !H");
					break;
				case ICMP_UNREACH_PROTOCOL:
					++got_there;
					Printf(" !P");
					break;
				case ICMP_UNREACH_NEEDFRAG:
					++unreachable;
					Printf(" !F");
					break;
				case ICMP_UNREACH_SRCFAIL:
					++unreachable;
					Printf(" !S");
					break;
				case ICMP_UNREACH_FILTER_PROHIB:
				case ICMP_UNREACH_NET_PROHIB: /*misuse*/
					++unreachable;
					Printf(" !A");
					break;
				case ICMP_UNREACH_HOST_PROHIB:
					++unreachable;
					Printf(" !C");
					break;
				case ICMP_UNREACH_NET_UNKNOWN:
				case ICMP_UNREACH_HOST_UNKNOWN:
					++unreachable;
					Printf(" !U");
					break;
				case ICMP_UNREACH_ISOLATED:
					++unreachable;
					Printf(" !I");
					break;
				case ICMP_UNREACH_TOSNET:
				case ICMP_UNREACH_TOSHOST:
					++unreachable;
					Printf(" !T");
					break;
				default:
					++unreachable;
					Printf(" !<%d>", i - 1);
					break;
				}
				break;
			}
			if (cc == 0) {
				Printf(" *");
				timeout++;
			}
			(void) fflush(stdout);
		}
		putchar('\n');
		if (got_there || (unreachable && (unreachable + timeout) >= nprobes))
			exit(0);
	}
}

int
wait_for_reply(sock, from, sent)
	int sock;
	struct sockaddr_in *from;
	struct timeval *sent;
{
	struct timeval now, wait;
	int cc = 0, fdsn;
	int fromlen = sizeof (*from);
	fd_set *fdsp;

	fdsn = howmany(sock+1, NFDBITS) * sizeof(fd_mask);
	if ((fdsp = (fd_set *)malloc(fdsn)) == NULL)
		err(1, "malloc");
	memset(fdsp, 0, fdsn);
	FD_SET(sock, fdsp);
	gettimeofday(&now, NULL);
	wait.tv_sec = (sent->tv_sec + waittime) - now.tv_sec;
	wait.tv_usec =  sent->tv_usec - now.tv_usec;
	if (wait.tv_usec < 0) {
		wait.tv_usec += 1000000;
		wait.tv_sec--;
	}
	if (wait.tv_sec < 0)
		wait.tv_sec = wait.tv_usec = 0;

	if (select(sock+1, fdsp, (fd_set *)0, (fd_set *)0, &wait) > 0)
		cc=recvfrom(s, (char *)packet, sizeof(packet), 0,
			    (struct sockaddr *)from, &fromlen);

	free(fdsp);
	return(cc);
}

void
dump_packet()
{
	u_char *p;
	int i;

	Fprintf(stderr, "packet data:");
	for (p = outpacket, i = 0; i < datalen; i++) {
		if ((i % 24) == 0)
			Fprintf(stderr, "\n ");
		Fprintf(stderr, " %02x", *p++);
	}
	Fprintf(stderr, "\n");
}

void
send_probe(seq, ttl, to)
	int seq, ttl;
	struct sockaddr_in *to;
{
	struct ip *ip = (struct ip *)outpacket;
	u_char *p = (u_char *)(ip + 1);
	struct udphdr *up = (struct udphdr *)(p + lsrrlen);
	struct packetdata *op = (struct packetdata *)(up + 1);
	int i;
	struct timeval tv;

	ip->ip_len = htons(datalen);
	ip->ip_ttl = ttl;
	ip->ip_id = htons(ident+seq);

	up->uh_sport = htons(ident);
	up->uh_dport = htons(port+seq);
	up->uh_ulen = htons((u_short)(datalen - sizeof(struct ip) - lsrrlen));
	up->uh_sum = 0;

	op->seq = seq;
	op->ttl = ttl;
	(void) gettimeofday(&tv, &tz);

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
	(void) gettimeofday(&tv, &tz);
	op->sec = htonl(tv.tv_sec + sec_perturb);
	op->usec = htonl((tv.tv_usec + usec_perturb) % 1000000);

	if (dump)
		dump_packet();

	i = sendto(sndsock, outpacket, datalen, 0, (struct sockaddr *)to,
		   sizeof(struct sockaddr_in));
	if (i < 0 || i != datalen)  {
		if (i<0)
			perror("sendto");
		Printf("traceroute: wrote %s %d chars, ret=%d\n", hostname,
			datalen, i);
		(void) fflush(stdout);
	}
}

/*
 * Convert an ICMP "type" field to a printable string.
 */
char *
pr_type(t)
	u_char t;
{
	static char *ttab[] = {
	"Echo Reply",	"ICMP 1",	"ICMP 2",	"Dest Unreachable",
	"Source Quench", "Redirect",	"ICMP 6",	"ICMP 7",
	"Echo",		"Router Advert",	"Router Solicit",	"Time Exceeded",
	"Param Problem", "Timestamp",	"Timestamp Reply", "Info Request",
	"Info Reply", "Mask Request", "Mask Reply"
	};

	if (t > 18)
		return("OUT-OF-RANGE");

	return(ttab[t]);
}


int
packet_ok(buf, cc, from, seq)
	u_char *buf;
	int cc;
	struct sockaddr_in *from;
	int seq;
{
	register struct icmp *icp;
	u_char type, code;
	int hlen;
#ifndef ARCHAIC
	struct ip *ip;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			Printf("packet too short (%d bytes) from %s\n", cc,
				inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
#else
	icp = (struct icmp *)buf;
#endif ARCHAIC
	type = icp->icmp_type; code = icp->icmp_code;
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH) {
		struct ip *hip;
		struct udphdr *up;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;
		up = (struct udphdr *)((u_char *)hip + hlen);
		if (hlen + 12 <= cc && hip->ip_p == proto &&
		    up->uh_sport == htons(ident) &&
		    up->uh_dport == htons(port+seq))
			return (type == ICMP_TIMXCEED? -1 : code+1);
	}
#ifndef ARCHAIC
	if (verbose) {
		int i;
		in_addr_t *lp = (in_addr_t *)&icp->icmp_ip;

		Printf("\n%d bytes from %s", cc, inet_ntoa(from->sin_addr));
		Printf(" to %s", inet_ntoa(ip->ip_dst));
		Printf(": icmp type %d (%s) code %d\n", type, pr_type(type),
		       icp->icmp_code);
		for (i = 4; i < cc ; i += sizeof(in_addr_t))
			Printf("%2d: x%8.8lx\n", i, *lp++);
	}
#endif ARCHAIC
	return(0);
}


void
print(buf, cc, from)
	u_char *buf;
	int cc;
	struct sockaddr_in *from;
{
	struct ip *ip;
	int hlen;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	cc -= hlen;

	if (nflag)
		Printf(" %s", inet_ntoa(from->sin_addr));
	else
		Printf(" %s (%s)", inetname(from->sin_addr),
		       inet_ntoa(from->sin_addr));

	if (verbose)
		Printf (" %d bytes to %s", cc, inet_ntoa (ip->ip_dst));
}


#ifdef notyet
/*
 * Checksum routine for Internet Protocol family headers (C Version)
 */
u_short
in_cksum(addr, len)
	u_short *addr;
	int len;
{
	register int nleft = len;
	register u_short *w = addr;
	register u_short answer;
	register int sum = 0;

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
#endif notyet

/*
 * Construct an Internet address representation.
 * If the nflag has been supplied, give
 * numeric value, otherwise try for symbolic name.
 */
char *
inetname(in)
	struct in_addr in;
{
	register char *cp;
	register struct hostent *hp;
	static int first = 1;
	static char domain[MAXHOSTNAMELEN], line[MAXHOSTNAMELEN];

	if (first && !nflag) {
		first = 0;
		if (gethostname(domain, sizeof domain) == 0 &&
		    (cp = strchr(domain, '.')) != NULL) {
			(void)strncpy(domain, cp + 1, sizeof(domain) - 1);
			domain[sizeof(domain) - 1] = '\0';
		}
	}
	if (!nflag && in.s_addr != INADDR_ANY) {
		hp = gethostbyaddr((char *)&in, sizeof(in), AF_INET);
		if (hp != NULL) {
			if ((cp = strchr(hp->h_name, '.')) != NULL &&
			    strcmp(cp + 1, domain) == 0)
				*cp = '\0';
			(void)strncpy(line, hp->h_name, sizeof(line) - 1);
			line[sizeof(line) - 1] = '\0';
			return (line);
		}
	}
	return (inet_ntoa(in));
}

void
usage()
{
	(void)fprintf(stderr,
"usage: traceroute [-dDnrv] [-g gateway_addr] ... [-m max_ttl] [-p port#]\n\t\
[-P proto] [-q nqueries] [-s src_addr] [-t tos]\n\t\
[-w wait] host [data size]\n");
	exit(1);
}
