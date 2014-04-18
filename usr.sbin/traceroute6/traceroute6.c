/*	$OpenBSD: traceroute6.c,v 1.72 2014/04/18 16:02:08 florian Exp $	*/
/*	$KAME: traceroute6.c,v 1.63 2002/10/24 12:53:25 itojun Exp $	*/

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
#include <sys/uio.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/sysctl.h>

#include <netinet/in.h>

#include <arpa/inet.h>
#include <arpa/nameser.h>

#include <netdb.h>
#include <stdio.h>
#include <err.h>
#include <poll.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/udp.h>

#define DUMMY_PORT 10010

#define	MAXPACKET	65535	/* max ip packet size */

#ifndef HAVE_GETIPNODEBYNAME
#define getipnodebyname(x, y, z, u)	gethostbyname2((x), (y))
#define freehostent(x)
#endif

/*
 * Format of the data in a (udp) probe packet.
 */
struct packetdata {
	u_char seq;		/* sequence number of this packet */
	u_int8_t hops;		/* hop limit of the packet */
	u_char pad[2];
	u_int32_t sec;		/* time packet left */
	u_int32_t usec;
} __packed;

u_char	packet[512];		/* last inbound (icmp) packet */
u_char	*outpacket;		/* last output (udp) packet */

int	main(int, char *[]);
int	wait_for_reply(int, struct msghdr *);
void	dump_packet(void);
void	build_probe6(int, u_int8_t, int, struct sockaddr *);
void	send_probe(int, u_int8_t, int, struct sockaddr *);
struct udphdr *get_udphdr(struct ip6_hdr *, u_char *);
int	get_hoplim(struct msghdr *);
double	deltaT(struct timeval *, struct timeval *);
char	*pr_type(int);
int	packet_ok(struct msghdr *, int, int, int);
void	print(struct sockaddr *, int, const char *);
const char *inetname(struct sockaddr *);
void	print_asn(struct sockaddr_storage *);
void	usage(void);

int rcvsock;			/* receive (icmp) socket file descriptor */
int sndsock;			/* send (udp) socket file descriptor */

struct msghdr rcvmhdr;
struct iovec rcviov[2];
int rcvhlim;
struct in6_pktinfo *rcvpktinfo;

struct sockaddr_in6 Src, Dst, Rcv;
int datalen;			/* How much data */
#define	ICMP6ECHOLEN	8
/* XXX: 2064 = 127(max hops in type 0 rthdr) * sizeof(ip6_hdr) + 16(margin) */
char rtbuf[2064];
struct ip6_rthdr *rth;

char *source = 0;
char *hostname;

int nprobes = 3;
u_int8_t max_hops = IPV6_DEFHLIM;
u_int8_t first_hop = 1;
u_int16_t srcport;
u_int16_t port = 32768+666;	/* start udp dest port # for probe packets */
u_int16_t ident;
int options;			/* socket options */
int verbose;
int waittime = 5;		/* time to wait for response (in seconds) */
int nflag;			/* print addresses numerically */
int dump;
int useicmp;
int Aflag;			/* lookup ASN */

extern char *__progname;

int
main(int argc, char *argv[])
{
	int mib[4] = { CTL_NET, PF_INET6, IPPROTO_IPV6, IPV6CTL_DEFHLIM };
	int ttl_flag = 0, incflag = 1, sump = 0;
	char hbuf[NI_MAXHOST], src0[NI_MAXHOST], *ep;
	int ch, i, on = 1, seq, probe, rcvcmsglen, error, minlen;
	struct addrinfo hints, *res;
	static u_char *rcvcmsgbuf;
	struct hostent *hp;
	size_t size;
	u_int8_t hops;
	long l;
	uid_t uid;
	int rtableid = -1;
	const char *errstr;

	/*
	 * Receive ICMP
	 */
	if ((rcvsock = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0) {
		perror("socket(ICMPv6)");
		exit(5);
	}

	/* revoke privs */
	uid = getuid();
	if (setresuid(uid, uid, uid) == -1)
		err(1, "setresuid");

	size = sizeof(i);
	(void) sysctl(mib, sizeof(mib)/sizeof(mib[0]), &i, &size, NULL, 0);
	max_hops = i;

	/* specify to tell receiving interface */
	if (setsockopt(rcvsock, IPPROTO_IPV6, IPV6_RECVPKTINFO, &on,
	    sizeof(on)) < 0)
		err(1, "setsockopt(IPV6_RECVPKTINFO)");

	/* specify to tell value of hoplimit field of received IP6 hdr */
	if (setsockopt(rcvsock, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &on,
	    sizeof(on)) < 0)
		err(1, "setsockopt(IPV6_RECVHOPLIMIT)");

	seq = 0;

	while ((ch = getopt(argc, argv, "AcDdf:g:Ilm:np:q:Ss:w:vV:")) != -1)
		switch (ch) {
		case 'A':
			Aflag++;
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
		case 'f':
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l < 1 || l > max_hops)
				errx(1, "firsthop must be 1 to %u.", max_hops);
			first_hop = (u_int8_t)l;
			break;
		case 'g':
			hp = getipnodebyname(optarg, AF_INET6, 0, &h_errno);
			if (hp == NULL) {
				fprintf(stderr,
				    "traceroute6: unknown host %s\n", optarg);
				exit(1);
			}
			if (rth == NULL) {
				/*
				 * XXX: We can't detect the number of
				 * intermediate nodes yet.
				 */
				if ((rth = inet6_rth_init((void *)rtbuf,
				    sizeof(rtbuf), IPV6_RTHDR_TYPE_0,
				    0)) == NULL) {
					fprintf(stderr,
					    "inet6_rth_init failed.\n");
					exit(1);
				}
			}
			if (inet6_rth_add((void *)rth,
			    (struct in6_addr *)hp->h_addr)) {
				fprintf(stderr,
				    "inet6_rth_add failed for %s\n",
				    optarg);
				exit(1);
			}
			freehostent(hp);
			break;
		case 'I':
			useicmp++;
			ident = htons(getpid() & 0xffff); /* same as ping6 */
			break;
		case 'l':
			ttl_flag++;
			break;
		case 'm':
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l < first_hop ||
			    l > IPV6_MAXHLIM)
				errx(1, "hoplimit must be %u to %u.", first_hop,
				    IPV6_MAXHLIM);
			max_hops = (u_int8_t)l;
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
			port = (u_int16_t)l;
			break;
		case 'q':
			errno = 0;
			ep = NULL;
			l = strtol(optarg, &ep, 10);
			if (errno || !*optarg || *ep || l < 1 || l > INT_MAX)
				errx(1, "nprobes must be >0.");
			nprobes = (int)l;
			break;
		case 's':
			/*
			 * set the ip source address of the outbound
			 * probe (e.g., on a multi-homed host).
			 */
			source = optarg;
			break;
		case 'S':
			sump = 1;
			break;
		case 'v':
			verbose++;
			break;
		case 'V':
			rtableid = (int)strtonum(optarg, 0,
			    RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rtable value is %s: %s",
				    errstr, optarg);
			if (setsockopt(rcvsock, SOL_SOCKET, SO_RTABLE,
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
		default:
			usage();
		}
	argc -= optind;
	argv += optind;

	if (argc < 1 || argc > 2)
		usage();

	setvbuf(stdout, NULL, _IOLBF, 0);

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = IPPROTO_ICMPV6;
	hints.ai_flags = AI_CANONNAME;
	error = getaddrinfo(*argv, NULL, &hints, &res);
	if (error) {
		fprintf(stderr,
		    "traceroute6: %s\n", gai_strerror(error));
		exit(1);
	}
	if (res->ai_addrlen != sizeof(Dst)) {
		fprintf(stderr,
		    "traceroute6: size of sockaddr mismatch\n");
		exit(1);
	}
	memcpy(&Dst, res->ai_addr, res->ai_addrlen);
	hostname = res->ai_canonname ? strdup(res->ai_canonname) : *argv;
	if (!hostname) {
		fprintf(stderr, "traceroute6: not enough core\n");
		exit(1);
	}
	if (res->ai_next) {
		if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(hbuf, "?", sizeof(hbuf));
		fprintf(stderr, "traceroute6: Warning: %s has multiple "
		    "addresses; using %s\n", hostname, hbuf);
	}

	if (*++argv) {
		errno = 0;
		ep = NULL;
		l = strtol(*argv, &ep, 10);
		if (errno || !*argv || *ep || l < 0 || l > INT_MAX)
			errx(1, "datalen out of range");
		datalen = (int)l;
	}
	if (useicmp)
		minlen = ICMP6ECHOLEN + sizeof(struct packetdata);
	else
		minlen = sizeof(struct packetdata);
	if (datalen < minlen)
		datalen = minlen;
	else if (datalen >= MAXPACKET) {
		fprintf(stderr,
		    "traceroute6: packet size must be %d <= s < %ld.\n",
		    minlen, (long)MAXPACKET);
		exit(1);
	}

	if ((outpacket = calloc(1, datalen)) == NULL)
		err(1, "calloc");

	/* initialize msghdr for receiving packets */
	rcviov[0].iov_base = (caddr_t)packet;
	rcviov[0].iov_len = sizeof(packet);
	rcvmhdr.msg_name = (caddr_t)&Rcv;
	rcvmhdr.msg_namelen = sizeof(Rcv);
	rcvmhdr.msg_iov = rcviov;
	rcvmhdr.msg_iovlen = 1;
	rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	
	if ((rcvcmsgbuf = malloc(rcvcmsglen)) == NULL) {
		fprintf(stderr, "traceroute6: malloc failed\n");
		exit(1);
	}
	rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
	rcvmhdr.msg_controllen = rcvcmsglen;

	if (options & SO_DEBUG)
		(void) setsockopt(rcvsock, SOL_SOCKET, SO_DEBUG,
		    (char *)&on, sizeof(on));

	/*
	 * Send UDP or ICMP
	 */
	if (useicmp) {
		sndsock = rcvsock;
	} else {
		if ((sndsock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			perror("socket(SOCK_DGRAM)");
			exit(5);
		}
		if (rtableid >= 0 && setsockopt(sndsock, SOL_SOCKET, SO_RTABLE,
		    &rtableid, sizeof(rtableid)) == -1)
			err(1, "setsockopt SO_RTABLE");
	}
#ifdef SO_SNDBUF
	if (setsockopt(sndsock, SOL_SOCKET, SO_SNDBUF, (char *)&datalen,
	    sizeof(datalen)) < 0)
		err(6, "SO_SNDBUF");
#endif /* SO_SNDBUF */
	if (options & SO_DEBUG)
		(void) setsockopt(sndsock, SOL_SOCKET, SO_DEBUG,
		    (char *)&on, sizeof(on));
	if (rth) {/* XXX: there is no library to finalize the header... */
		rth->ip6r_len = rth->ip6r_segleft * 2;
		if (setsockopt(sndsock, IPPROTO_IPV6, IPV6_RTHDR,
		    (void *)rth, (rth->ip6r_len + 1) << 3)) {
			fprintf(stderr, "setsockopt(IPV6_RTHDR): %s\n",
			    strerror(errno));
			exit(1);
		}
	}

	/*
	 * Source selection
	 */
	bzero(&Src, sizeof(Src));
	if (source) {
		struct addrinfo hints, *res;
		int error;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_DGRAM;	/*dummy*/
		hints.ai_flags = AI_NUMERICHOST;
		error = getaddrinfo(source, "0", &hints, &res);
		if (error) {
			printf("traceroute6: %s: %s\n", source,
			    gai_strerror(error));
			exit(1);
		}
		if (res->ai_addrlen > sizeof(Src)) {
			printf("traceroute6: %s: %s\n", source,
			    gai_strerror(error));
			exit(1);
		}
		memcpy(&Src, res->ai_addr, res->ai_addrlen);
		freeaddrinfo(res);
	} else {
		struct sockaddr_in6 Nxt;
		int dummy;
		socklen_t len;

		Nxt = Dst;
		Nxt.sin6_port = htons(DUMMY_PORT);
		if ((dummy = socket(AF_INET6, SOCK_DGRAM, 0)) < 0) {
			perror("socket");
			exit(1);
		}
		if (connect(dummy, (struct sockaddr *)&Nxt, Nxt.sin6_len) < 0) {
			perror("connect");
			exit(1);
		}
		len = sizeof(Src);
		if (getsockname(dummy, (struct sockaddr *)&Src, &len) < 0) {
			perror("getsockname");
			exit(1);
		}
		if (getnameinfo((struct sockaddr *)&Src, Src.sin6_len,
		    src0, sizeof(src0), NULL, 0, NI_NUMERICHOST)) {
			fprintf(stderr, "getnameinfo failed for source\n");
			exit(1);
		}
		source = src0;
		close(dummy);
	}

	Src.sin6_port = htons(0);
	if (bind(sndsock, (struct sockaddr *)&Src, Src.sin6_len) < 0) {
		perror("bind sndsock");
		exit(1);
	}

	{
		socklen_t len;

		len = sizeof(Src);
		if (getsockname(sndsock, (struct sockaddr *)&Src, &len) < 0) {
			perror("getsockname");
			exit(1);
		}
		srcport = ntohs(Src.sin6_port);
	}

	/*
	 * Message to users
	 */
	if (getnameinfo((struct sockaddr *)&Dst, Dst.sin6_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST))
		strlcpy(hbuf, "(invalid)", sizeof(hbuf));
	fprintf(stderr, "traceroute6");
	fprintf(stderr, " to %s (%s)", hostname, hbuf);
	if (source)
		fprintf(stderr, " from %s", source);
	fprintf(stderr, ", %u hops max, %d byte packets\n",
	    max_hops, datalen);
	(void) fflush(stderr);

	if (first_hop > 1)
		printf("Skipping %u intermediate hops\n", first_hop - 1);

	/*
	 * Main loop
	 */
	for (hops = first_hop; hops <= max_hops; ++hops) {
		struct in6_addr lastaddr;
		int got_there = 0, unreachable = 0, timeout = 0, loss;

		printf("%2u ", hops);
		bzero(&lastaddr, sizeof(lastaddr));
		for (probe = 0, loss = 0; probe < nprobes; ++probe) {
			int cc;
			struct timeval t1, t2;

			(void) gettimeofday(&t1, NULL);
			send_probe(++seq, hops, incflag, (struct sockaddr*)
			    &Dst);
			while ((cc = wait_for_reply(rcvsock, &rcvmhdr))) {
				(void) gettimeofday(&t2, NULL);
				if ((i = packet_ok(&rcvmhdr, cc, seq,
				    incflag))) {
					if (!IN6_ARE_ADDR_EQUAL(&Rcv.sin6_addr,
					    &lastaddr)) {
						print((struct sockaddr *)
						    rcvmhdr.msg_name, cc,
						    rcvpktinfo ?  inet_ntop(
						    AF_INET6, &rcvpktinfo->
						    ipi6_addr, hbuf,
						    sizeof(hbuf)) : "?");
						lastaddr = Rcv.sin6_addr;
					}
					printf("  %g ms", deltaT(&t1, &t2));
					if (ttl_flag)
						printf(" (%u)", rcvhlim);
					switch (i - 1) {
					case ICMP6_DST_UNREACH_NOROUTE:
						++unreachable;
						printf(" !N");
						break;
					case ICMP6_DST_UNREACH_ADMIN:
						++unreachable;
						printf(" !P");
						break;
					case ICMP6_DST_UNREACH_NOTNEIGHBOR:
						++unreachable;
						printf(" !S");
						break;
					case ICMP6_DST_UNREACH_ADDR:
						++unreachable;
						printf(" !A");
						break;
					case ICMP6_DST_UNREACH_NOPORT:
						if (rcvhlim >= 0 &&
						    rcvhlim <= 1)
							printf(" !");
						++got_there;
						break;
					}
					break;
				}
			}
			if (cc == 0) {
				printf(" *");
				timeout++;
				loss++;
			}
			(void) fflush(stdout);
		}
		if (sump)
			printf(" (%d%% loss)", (loss * 100) / nprobes);
		putchar('\n');
		if (got_there ||
		    (unreachable && (unreachable + timeout) >= nprobes))
			break;
	}
	exit(0);
}

int
wait_for_reply(int sock, struct msghdr *mhdr)
{
	struct pollfd pfd[1];
	int cc = 0;

	pfd[0].fd = sock;
	pfd[0].events = POLLIN;
	pfd[0].revents = 0;

	if (poll(pfd, 1, waittime * 1000) > 0)
		cc = recvmsg(rcvsock, mhdr, 0);

	return(cc);
}

void
dump_packet(void)
{
	u_char *p;
	int i;

	fprintf(stderr, "packet data:");
	for (p = (u_char*)outpacket, i = 0; i < datalen; i++) {
		if ((i % 24) == 0)
			fprintf(stderr, "\n ");
		fprintf(stderr, " %02x", *p++);
	}
	fprintf(stderr, "\n");
}

void
build_probe6(int seq, u_int8_t hops, int iflag, struct sockaddr *to)
{
	struct timeval tv;
	struct packetdata *op;
	int i;

	i = hops;
	if (setsockopt(sndsock, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
	    (char *)&i, sizeof(i)) < 0) {
		perror("setsockopt IPV6_UNICAST_HOPS");
	}

	if (iflag)
		((struct sockaddr_in6*)to)->sin6_port = htons(port + seq);
	else
		((struct sockaddr_in6*)to)->sin6_port = htons(port);
	(void) gettimeofday(&tv, NULL);

	if (useicmp) {
		struct icmp6_hdr *icp = (struct icmp6_hdr *)outpacket;

		icp->icmp6_type = ICMP6_ECHO_REQUEST;
		icp->icmp6_code = 0;
		icp->icmp6_cksum = 0;
		icp->icmp6_id = ident;
		icp->icmp6_seq = htons(seq);
		op = (struct packetdata *)(outpacket + ICMP6ECHOLEN);
	} else
		op = (struct packetdata *)outpacket;
	op->seq = seq;
	op->hops = hops;
	op->sec = htonl(tv.tv_sec);
	op->usec = htonl(tv.tv_usec);
}

void
send_probe(int seq, u_int8_t hops, int iflag, struct sockaddr *to)
{
	int i;

	switch (to->sa_family) {
		case AF_INET6:
			build_probe6(seq, hops, iflag, to);
			break;
		default:
			errx(1, "unsupported AF: %d", to->sa_family);
	}

	if (dump)
		dump_packet();

	i = sendto(sndsock, outpacket, datalen, 0, to, to->sa_len);
	if (i < 0 || i != datalen)  {
		if (i < 0)
			perror("sendto");
		printf("%s: wrote %s %d chars, ret=%d\n", __progname, hostname,
		    datalen, i);
		(void) fflush(stdout);
	}
}

int
get_hoplim(struct msghdr *mhdr)
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	    cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			return(*(int *)CMSG_DATA(cm));
	}

	return(-1);
}

double
deltaT(struct timeval *t1p, struct timeval *t2p)
{
	double dt;

	dt = (double)(t2p->tv_sec - t1p->tv_sec) * 1000.0 +
	    (double)(t2p->tv_usec - t1p->tv_usec) / 1000.0;
	return (dt);
}

/*
 * Convert an ICMP "type" field to a printable string.
 */
char *
pr_type(int t0)
{
	u_char t = t0 & 0xff;
	char *cp;

	switch (t) {
	case ICMP6_DST_UNREACH:
		cp = "Destination Unreachable";
		break;
	case ICMP6_PACKET_TOO_BIG:
		cp = "Packet Too Big";
		break;
	case ICMP6_TIME_EXCEEDED:
		cp = "Time Exceeded";
		break;
	case ICMP6_PARAM_PROB:
		cp = "Parameter Problem";
		break;
	case ICMP6_ECHO_REQUEST:
		cp = "Echo Request";
		break;
	case ICMP6_ECHO_REPLY:
		cp = "Echo Reply";
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		cp = "Group Membership Query";
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		cp = "Group Membership Report";
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		cp = "Group Membership Reduction";
		break;
	case ND_ROUTER_SOLICIT:
		cp = "Router Solicitation";
		break;
	case ND_ROUTER_ADVERT:
		cp = "Router Advertisement";
		break;
	case ND_NEIGHBOR_SOLICIT:
		cp = "Neighbor Solicitation";
		break;
	case ND_NEIGHBOR_ADVERT:
		cp = "Neighbor Advertisement";
		break;
	case ND_REDIRECT:
		cp = "Redirect";
		break;
	default:
		cp = "Unknown";
		break;
	}
	return cp;
}

int
packet_ok(struct msghdr *mhdr, int cc, int seq, int iflag)
{
	struct icmp6_hdr *icp;
	struct sockaddr_in6 *from = (struct sockaddr_in6 *)mhdr->msg_name;
	u_char type, code;
	char *buf = (char *)mhdr->msg_iov[0].iov_base;
	struct cmsghdr *cm;
	int *hlimp;
	char hbuf[NI_MAXHOST];

	if (cc < sizeof(struct icmp6_hdr)) {
		if (verbose) {
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
		if ((up = get_udphdr(hip, (u_char *)(buf + cc))) == NULL) {
			if (verbose)
				warnx("failed to get upper layer header");
			return(0);
		}
		if (useicmp &&
		    ((struct icmp6_hdr *)up)->icmp6_id == ident &&
		    ((struct icmp6_hdr *)up)->icmp6_seq == htons(seq))
			return (type == ICMP6_TIME_EXCEEDED ? -1 : code + 1);
		else if (!useicmp &&
		    up->uh_sport == htons(srcport) &&
		    ((iflag && up->uh_dport == htons(port + seq)) ||
		    (!iflag && up->uh_dport == htons(port))))
			return (type == ICMP6_TIME_EXCEEDED ? -1 : code + 1);
	} else if (useicmp && type == ICMP6_ECHO_REPLY) {
		if (icp->icmp6_id == ident &&
		    icp->icmp6_seq == htons(seq))
			return (ICMP6_DST_UNREACH_NOPORT + 1);
	}
	if (verbose) {
		char sbuf[NI_MAXHOST+1], dbuf[INET6_ADDRSTRLEN];
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

/*
 * Increment pointer until find the UDP or ICMP header.
 */
struct udphdr *
get_udphdr(struct ip6_hdr *ip6, u_char *lim)
{
	u_char *cp = (u_char *)ip6, nh;
	int hlen;

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
print(struct sockaddr *from, int cc, const char *to)
{
	char hbuf[NI_MAXHOST];
	if (getnameinfo(from, from->sa_len,
	    hbuf, sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
		strlcpy(hbuf, "invalid", sizeof(hbuf));
	if (nflag)
		printf(" %s", hbuf);
	else
		printf(" %s (%s)", inetname(from), hbuf);

	if (Aflag)
		print_asn((struct sockaddr_storage *)from);

	if (verbose)
		printf(" %d bytes to %s", cc, to);
}

/*
 * Construct an Internet address representation.
 */
const char *
inetname(struct sockaddr *sa)
{
	static char line[NI_MAXHOST], domain[MAXHOSTNAMELEN + 1];
	static int first = 1;
	char *cp;

	if (first) {
		first = 0;
		if (gethostname(domain, sizeof(domain)) == 0 &&
		    (cp = strchr(domain, '.')) != NULL)
			(void) strlcpy(domain, cp + 1, sizeof(domain));
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
	char qbuf[MAXDNAME], *qp;

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

void
usage(void)
{

	fprintf(stderr,
"usage: traceroute6 [-AcDdIlnSv] [-f firsthop] [-g gateway] [-m hoplimit]\n"
"       [-p port] [-q probes] [-s src] [-V rtableid] [-w waittime]\n"
"       host [datalen]\n");
	exit(1);
}
