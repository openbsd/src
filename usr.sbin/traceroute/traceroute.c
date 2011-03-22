/*	$OpenBSD: traceroute.c,v 1.74 2011/03/22 10:16:23 okan Exp $	*/
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

/*
 * traceroute host  - trace the route ip packets follow going to "host".
 *
 * Attempt to trace the route an ip packet would follow to some
 * internet host.  We find out intermediate hops by launching probe
 * packets with a small ttl (time to live) then listening for an
 * icmp "time exceeded" reply from a gateway.  We start our probes
 * with a ttl of one and increase by one until we get an icmp "port
 * unreachable" (which means we got to "host") or hit a max (which
 * defaults to 64 hops & can be changed with the -m flag).  Three
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
 *     traceroute to nis.nsf.net (35.1.1.48), 64 hops max, 56 byte packet
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
 *     traceroute to allspice.lcs.mit.edu (18.26.0.115), 64 hops max
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
#include <sys/sysctl.h>

#include <netinet/in_systm.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_icmp.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>

#include <arpa/inet.h>

#include <netmpls/mpls.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define MAX_LSRR		((MAX_IPOPTLEN - 4) / 4)

#define MPLS_LABEL(m)		((m & MPLS_LABEL_MASK) >> MPLS_LABEL_OFFSET)
#define MPLS_EXP(m)		((m & MPLS_EXP_MASK) >> MPLS_EXP_OFFSET)

/*
 * Format of the data in a (udp) probe packet.
 */
struct packetdata {
	u_char seq;		/* sequence number of this packet */
	u_int8_t ttl;		/* ttl packet left with */
	u_char pad[2];
	u_int32_t sec;		/* time packet left */
	u_int32_t usec;
} __packed;

struct in_addr gateway[MAX_LSRR + 1];
int lsrrlen = 0;
int32_t sec_perturb;
int32_t usec_perturb;

u_char packet[512], *outpacket;	/* last inbound (icmp) packet */

int wait_for_reply(int, struct sockaddr_in *, struct timeval *);
void send_probe(int, u_int8_t, int, struct sockaddr_in *);
int packet_ok(u_char *, int, struct sockaddr_in *, int, int);
void print_exthdr(u_char *, int);
void print(u_char *, int, struct sockaddr_in *);
char *inetname(struct in_addr);
u_short in_cksum(u_short *, int);
void usage(void);

int s;				/* receive (icmp) socket file descriptor */
int sndsock;			/* send (udp) socket file descriptor */

int datalen;			/* How much data */
int headerlen;			/* How long packet's header is */

char *source = 0;
char *hostname;

int nprobes = 3;
u_int8_t max_ttl = IPDEFTTL;
u_int8_t first_ttl = 1;
u_short ident;
u_short port = 32768+666;	/* start udp dest port # for probe packets */
u_char	proto = IPPROTO_UDP;
u_int8_t  icmp_type = ICMP_ECHO; /* default ICMP code/type */
u_char  icmp_code = 0;
int options;			/* socket options */
int verbose;
int waittime = 5;		/* time to wait for response (in seconds) */
int nflag;			/* print addresses numerically */
int dump;
int xflag;			/* show ICMP extension header */

int
main(int argc, char *argv[])
{
	int mib[4] = { CTL_NET, PF_INET, IPPROTO_IP, IPCTL_DEFTTL };
	int ttl_flag = 0, incflag = 1, protoset = 0, sump = 0;
	int ch, i, lsrr = 0, on = 1, probe, seq = 0, tos = 0;
	size_t size = sizeof(max_ttl);
	struct sockaddr_in from, to;
	struct hostent *hp;
	u_int32_t tmprnd;
	struct ip *ip;
	u_int8_t ttl;
	char *ep;
	const char *errstr;
	long l;
	uid_t uid;
	u_int rtableid;

	if ((s = socket(AF_INET, SOCK_RAW, IPPROTO_ICMP)) < 0)
		err(5, "icmp socket");
	if ((sndsock = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
		err(5, "raw socket");

	/* revoke privs */
	uid = getuid();
	if (setresuid(uid, uid, uid) == -1)
		err(1, "setresuid");

	(void) sysctl(mib, sizeof(mib)/sizeof(mib[0]), &max_ttl, &size,
	    NULL, 0);

	while ((ch = getopt(argc, argv, "cDdf:g:Ilm:nP:p:q:rSs:t:V:vw:x"))
			!= -1)
		switch (ch) {
		case 'S':
			sump = 1;
			break;
		case 'f':
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l < 1 || l > max_ttl)
				errx(1, "min ttl must be 1 to %u.", max_ttl);
			first_ttl = (u_int8_t)l;
			break;
		case 'c':
			incflag = 0;
			break;
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
		case 'I':
			if (protoset)
				errx(1, "protocol already set with -P");
			protoset = 1;
			proto = IPPROTO_ICMP;
			break;
		case 'l':
			ttl_flag++;
			break;
		case 'm':
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l < first_ttl ||
			    l > MAXTTL)
				errx(1, "max ttl must be %u to %u.", first_ttl,
				    MAXTTL);
			max_ttl = (u_int8_t)l;
			break;
		case 'n':
			nflag++;
			break;
		case 'p':
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l <= 0 || l >= 65536)
				errx(1, "port must be >0, <65536.");
			port = (int)l;
			break;
		case 'P':
			if (protoset)
				errx(1, "protocol already set with -I");
			protoset = 1;
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l < 1 ||
			    l >= IPPROTO_MAX) {
				struct protoent *pent;

				pent = getprotobyname(optarg);
				if (pent)
					proto = pent->p_proto;
				else
					errx(1, "proto must be >=1, or a name.");
			} else
				proto = (int)l;
			break;
		case 'q':
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l < 1 || l > INT_MAX)
				errx(1, "nprobes must be >0.");
			nprobes = (int)l;
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
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l < 0 || l > 255)
				errx(1, "tos must be 0 to 255.");
			tos = (int)l;
			break;
		case 'v':
			verbose++;
			break;
		case 'V':
			rtableid = (unsigned int)strtonum(optarg, 0,
			    RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rtable value is %s: %s",
				    errstr, optarg);
			if (setsockopt(sndsock, IPPROTO_IP, SO_RTABLE,
			    &rtableid, sizeof(rtableid)) == -1)
				err(1, "setsockopt SO_RTABLE");
			if (setsockopt(s, IPPROTO_IP, SO_RTABLE,
			    &rtableid, sizeof(rtableid)) == -1)
				err(1, "setsockopt SO_RTABLE");
			break;
		case 'w':
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l <= 1 || l > INT_MAX)
				errx(1, "wait must be >1 sec.");
			waittime = (int)l;
			break;
		case 'x':
			xflag = 1;
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
		if (hp->h_addr_list[1] != NULL)
			warnx("Warning: %s has multiple addresses; using %s",
			    hostname, inet_ntoa(to.sin_addr));
	}
	if (*++argv) {
		errno = 0;
		ep = NULL;
		l = strtol(*argv, &ep, 10);
		if (errno || !*argv || *ep || l < 0 || l > INT_MAX)
			errx(1, "datalen out of range");
		datalen = (int)l;
	}

	switch (proto) {
	case IPPROTO_UDP:
		headerlen = (sizeof(struct ip) + lsrrlen +
		    sizeof(struct udphdr) + sizeof(struct packetdata));
		break;
	case IPPROTO_ICMP:
		headerlen = (sizeof(struct ip) + lsrrlen +
		    sizeof(struct icmp) + sizeof(struct packetdata));
		break;
	default:
		headerlen = (sizeof(struct ip) + lsrrlen +
		    sizeof(struct packetdata));
	}

	if (datalen < 0 || datalen > IP_MAXPACKET - headerlen)
		errx(1, "packet size must be 0 to %d.",
		    IP_MAXPACKET - headerlen);

	datalen += headerlen;

	outpacket = malloc(datalen);
	if (outpacket == 0)
		err(1, "malloc");
	(void) memset(outpacket, 0, datalen);

	ip = (struct ip *)outpacket;
	if (lsrr != 0) {
		u_char *p = (u_char *)(ip + 1);

		*p++ = IPOPT_NOP;
		*p++ = IPOPT_LSRR;
		*p++ = lsrrlen - 1;
		*p++ = IPOPT_MINOFF;
		gateway[lsrr] = to.sin_addr;
		for (i = 1; i <= lsrr; i++) {
			memcpy(p, &gateway[i], sizeof(struct in_addr));
			p += sizeof(struct in_addr);
		}
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
#ifdef SO_SNDBUF
	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&datalen,
	    sizeof(datalen)) < 0)
		err(6, "SO_SNDBUF");
#endif /* SO_SNDBUF */
#ifdef IP_HDRINCL
	if (setsockopt(sndsock, IPPROTO_IP, IP_HDRINCL, (char *)&on,
	    sizeof(on)) < 0)
		err(6, "IP_HDRINCL");
#endif /* IP_HDRINCL */
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
		if (getuid() != 0 &&
		    (ntohl(from.sin_addr.s_addr) & 0xff000000U) == 0x7f000000U &&
		    (ntohl(to.sin_addr.s_addr) & 0xff000000U) != 0x7f000000U)
			errx(1, "source is on 127/8, destination is not");

		if (getuid() &&
		    bind(sndsock, (struct sockaddr *)&from, sizeof(from)) < 0)
			err(1, "bind");
	}

	fprintf(stderr, "traceroute to %s (%s)", hostname,
		inet_ntoa(to.sin_addr));
	if (source)
		fprintf(stderr, " from %s", source);
	fprintf(stderr, ", %u hops max, %d byte packets\n", max_ttl, datalen);
	(void) fflush(stderr);

	if (first_ttl > 1)
		printf("Skipping %u intermediate hops\n", first_ttl - 1);

	for (ttl = first_ttl; ttl && ttl <= max_ttl; ++ttl) {
		int got_there = 0, unreachable = 0, timeout = 0, loss;
		in_addr_t lastaddr = 0;
		quad_t dt;

		printf("%2u ", ttl);
		for (probe = 0, loss = 0; probe < nprobes; ++probe) {
			int cc;
			struct timeval t1, t2;
			int code;

			(void) gettimeofday(&t1, NULL);
			send_probe(++seq, ttl, incflag, &to);
			while ((cc = wait_for_reply(s, &from, &t1))) {
				(void) gettimeofday(&t2, NULL);
				if (t2.tv_sec - t1.tv_sec > waittime) {
					cc = 0;
					break;
				}
				i = packet_ok(packet, cc, &from, seq, incflag);
				/* Skip short packet */
				if (i == 0)
					continue;
				if (from.sin_addr.s_addr != lastaddr) {
					print(packet, cc, &from);
					lastaddr = from.sin_addr.s_addr;
				}
				dt = (quad_t)(t2.tv_sec - t1.tv_sec) * 1000000 +
				    (quad_t)(t2.tv_usec - t1.tv_usec);
				printf("  %u", (u_int)(dt / 1000));
				if (dt % 1000)
					printf(".%u", (u_int)(dt % 1000));
				printf(" ms");
				ip = (struct ip *)packet;
				if (ttl_flag)
					printf(" (%u)", ip->ip_ttl);
				if (i == -2) {
#ifndef ARCHAIC
					ip = (struct ip *)packet;
					if (ip->ip_ttl <= 1)
						printf(" !");
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
						printf(" !");
#endif /* ARCHAIC */
					++got_there;
					break;
				case ICMP_UNREACH_NET:
					++unreachable;
					printf(" !N");
					break;
				case ICMP_UNREACH_HOST:
					++unreachable;
					printf(" !H");
					break;
				case ICMP_UNREACH_PROTOCOL:
					++got_there;
					printf(" !P");
					break;
				case ICMP_UNREACH_NEEDFRAG:
					++unreachable;
					printf(" !F");
					break;
				case ICMP_UNREACH_SRCFAIL:
					++unreachable;
					printf(" !S");
					break;
				case ICMP_UNREACH_FILTER_PROHIB:
					++unreachable;
					printf(" !X");
					break;
				case ICMP_UNREACH_NET_PROHIB: /*misuse*/
					++unreachable;
					printf(" !A");
					break;
				case ICMP_UNREACH_HOST_PROHIB:
					++unreachable;
					printf(" !C");
					break;
				case ICMP_UNREACH_NET_UNKNOWN:
				case ICMP_UNREACH_HOST_UNKNOWN:
					++unreachable;
					printf(" !U");
					break;
				case ICMP_UNREACH_ISOLATED:
					++unreachable;
					printf(" !I");
					break;
				case ICMP_UNREACH_TOSNET:
				case ICMP_UNREACH_TOSHOST:
					++unreachable;
					printf(" !T");
					break;
				default:
					++unreachable;
					printf(" !<%d>", i - 1);
					break;
				}
				break;
			}
			if (cc == 0) {
				printf(" *");
				timeout++;
				loss++;
			} else if (cc && probe == nprobes - 1 &&
			    (xflag || verbose))
				print_exthdr(packet, cc);
			(void) fflush(stdout);
		}
		if (sump)
			printf(" (%d%% loss)", (loss * 100) / nprobes);
		putchar('\n');
		if (got_there || (unreachable && (unreachable + timeout) >= nprobes))
			break;
	}
	exit(0);
}

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

int
wait_for_reply(int sock, struct sockaddr_in *from, struct timeval *sent)
{
	socklen_t fromlen = sizeof (*from);
	struct timeval now, wait;
	int cc = 0, fdsn;
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
		timerclear(&wait);

	if (select(sock+1, fdsp, (fd_set *)0, (fd_set *)0, &wait) > 0)
		cc = recvfrom(s, (char *)packet, sizeof(packet), 0,
		    (struct sockaddr *)from, &fromlen);

	free(fdsp);
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
send_probe(int seq, u_int8_t ttl, int iflag, struct sockaddr_in *to)
{
	struct ip *ip = (struct ip *)outpacket;
	u_char *p = (u_char *)(ip + 1);
	struct udphdr *up = (struct udphdr *)(p + lsrrlen);
	struct icmp *icmpp = (struct icmp *)(p + lsrrlen);
	struct packetdata *op;
	struct timeval tv;
	int i;

	ip->ip_len = htons(datalen);
	ip->ip_ttl = ttl;
	ip->ip_id = htons(ident+seq);

	switch (proto) {
	case IPPROTO_ICMP:
		icmpp->icmp_type = icmp_type;
		icmpp->icmp_code = icmp_code;
		icmpp->icmp_seq = htons(seq);
		icmpp->icmp_id = htons(ident);
		op = (struct packetdata *)(icmpp + 1);
		break;
	case IPPROTO_UDP:
		up->uh_sport = htons(ident);
		if (iflag)
			up->uh_dport = htons(port+seq);
		else
			up->uh_dport = htons(port);
		up->uh_ulen = htons((u_short)(datalen - sizeof(struct ip) -
		    lsrrlen));
		up->uh_sum = 0;
		op = (struct packetdata *)(up + 1);
		break;
	default:
		op = (struct packetdata *)(ip + 1);
		break;
	}
	op->seq = seq;
	op->ttl = ttl;
	(void) gettimeofday(&tv, NULL);

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
	(void) gettimeofday(&tv, NULL);
	op->sec = htonl(tv.tv_sec + sec_perturb);
	op->usec = htonl((tv.tv_usec + usec_perturb) % 1000000);

	if (proto == IPPROTO_ICMP && icmp_type == ICMP_ECHO) {
		icmpp->icmp_cksum = 0;
		icmpp->icmp_cksum = in_cksum((u_short *)icmpp,
		    datalen - sizeof(struct ip) - lsrrlen);
		if (icmpp->icmp_cksum == 0)
			icmpp->icmp_cksum = 0xffff;
	}

	if (dump)
		dump_packet();

	i = sendto(sndsock, outpacket, datalen, 0, (struct sockaddr *)to,
	    sizeof(struct sockaddr_in));
	if (i < 0 || i != datalen)  {
		if (i < 0)
			perror("sendto");
		printf("traceroute: wrote %s %d chars, ret=%d\n", hostname,
		    datalen, i);
		(void) fflush(stdout);
	}
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
packet_ok(u_char *buf, int cc, struct sockaddr_in *from, int seq, int iflag)
{
	struct icmp *icp;
	u_char code;
	u_int8_t type;
	int hlen;
#ifndef ARCHAIC
	struct ip *ip;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	if (cc < hlen + ICMP_MINLEN) {
		if (verbose)
			printf("packet too short (%d bytes) from %s\n", cc,
			    inet_ntoa(from->sin_addr));
		return (0);
	}
	cc -= hlen;
	icp = (struct icmp *)(buf + hlen);
#else
	icp = (struct icmp *)buf;
#endif /* ARCHAIC */
	type = icp->icmp_type;
	code = icp->icmp_code;
	if ((type == ICMP_TIMXCEED && code == ICMP_TIMXCEED_INTRANS) ||
	    type == ICMP_UNREACH || type == ICMP_ECHOREPLY) {
		struct ip *hip;
		struct udphdr *up;
		struct icmp *icmpp;

		hip = &icp->icmp_ip;
		hlen = hip->ip_hl << 2;

		switch (proto) {
		case IPPROTO_ICMP:
			if (icmp_type == ICMP_ECHO &&
			    type == ICMP_ECHOREPLY &&
			    icp->icmp_id == htons(ident) &&
			    icp->icmp_seq == htons(seq))
				return (-2); /* we got there */

			icmpp = (struct icmp *)((u_char *)hip + hlen);
			if (hlen + 8 <= cc && hip->ip_p == IPPROTO_ICMP &&
			    icmpp->icmp_id == htons(ident) &&
			    icmpp->icmp_seq == htons(seq))
				return (type == ICMP_TIMXCEED? -1 : code + 1);
			break;

		case IPPROTO_UDP:
			up = (struct udphdr *)((u_char *)hip + hlen);
			if (hlen + 12 <= cc && hip->ip_p == proto &&
			    up->uh_sport == htons(ident) &&
			    ((iflag && up->uh_dport == htons(port + seq)) ||
			    (!iflag && up->uh_dport == htons(port))))
				return (type == ICMP_TIMXCEED? -1 : code + 1);
			break;
		default:
			/* this is some odd, user specified proto,
			 * how do we check it?
			 */
			if (hip->ip_p == proto)
				return (type == ICMP_TIMXCEED? -1 : code + 1);
		}
	}
#ifndef ARCHAIC
	if (verbose) {
		int i;
		in_addr_t *lp = (in_addr_t *)&icp->icmp_ip;

		printf("\n%d bytes from %s", cc, inet_ntoa(from->sin_addr));
		printf(" to %s", inet_ntoa(ip->ip_dst));
		printf(": icmp type %u (%s) code %d\n", type, pr_type(type),
		    icp->icmp_code);
		for (i = 4; i < cc ; i += sizeof(in_addr_t))
			printf("%2d: x%8.8lx\n", i, (unsigned long)*lp++);
	}
#endif /* ARCHAIC */
	return (0);
}

void
print(u_char *buf, int cc, struct sockaddr_in *from)
{
	struct ip *ip;
	int hlen;

	ip = (struct ip *) buf;
	hlen = ip->ip_hl << 2;
	cc -= hlen;

	if (nflag)
		printf(" %s", inet_ntoa(from->sin_addr));
	else
		printf(" %s (%s)", inetname(from->sin_addr),
		    inet_ntoa(from->sin_addr));

	if (verbose)
		printf(" %d bytes to %s", cc, inet_ntoa(ip->ip_dst));
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
char *
inetname(struct in_addr in)
{
	static char domain[MAXHOSTNAMELEN], line[MAXHOSTNAMELEN];
	static int first = 1;
	struct hostent *hp;
	char *cp;

	if (first) {
		first = 0;
		if (gethostname(domain, sizeof domain) == 0 &&
		    (cp = strchr(domain, '.')) != NULL) {
			strlcpy(domain, cp + 1, sizeof(domain));
		}
	}
	if (in.s_addr != INADDR_ANY) {
		hp = gethostbyaddr((char *)&in, sizeof(in), AF_INET);
		if (hp != NULL) {
			if ((cp = strchr(hp->h_name, '.')) != NULL &&
			    strcmp(cp + 1, domain) == 0)
				*cp = '\0';
			strlcpy(line, hp->h_name, sizeof(line));
			return (line);
		}
	}
	return (inet_ntoa(in));
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-cDdIlnrSvx] [-f first_ttl] [-g gateway_addr] [-m max_ttl]\n"
	    "\t[-P proto] [-p port] [-q nqueries] [-s src_addr] [-t tos]\n"
	    "\t[-V rtable] [-w waittime] host [packetsize]\n", __progname);
	exit(1);
}
