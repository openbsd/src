/*	$OpenBSD: ping6.c,v 1.191 2016/09/12 15:47:58 florian Exp $	*/

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

/*
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

/*
 * Using the InterNet Control Message Protocol (ICMP) "ECHO" facility,
 * measure round-trip-delays and packet loss across network paths.
 *
 * Author -
 *	Mike Muuss
 *	U. S. Army Ballistic Research Laboratory
 *	December, 1983
 *
 * Status -
 *	Public Domain.  Distribution Unlimited.
 * Bugs -
 *	More statistics could always be gathered.
 *	This program has to run SUID to ROOT to access the ICMP socket.
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/uio.h>

#include <netinet/in.h>
#include <netinet/ip6.h>
#include <netinet/icmp6.h>
#include <netinet/ip_ah.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <poll.h>
#include <signal.h>
#include <siphash.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

struct tv64 {
	u_int64_t	tv64_sec;
	u_int64_t	tv64_nsec;
};

struct payload {
	struct tv64	tv64;
	u_int8_t	mac[SIPHASH_DIGEST_LENGTH];
};

#define	ECHOLEN		8	/* icmp echo header len excluding time */
#define	ECHOTMLEN	sizeof(struct payload)
#define	DEFDATALEN	(64 - ECHOLEN)		/* default data length */
#define	IP6LEN		40
#define	EXTRA		256	/* for AH and various other headers. weird. */
#define	MAXPAYLOAD	IPV6_MAXPACKET - IP6LEN - ECHOLEN
#define	MAXWAIT_DEFAULT	10			/* secs to wait for response */

#define	A(bit)		rcvd_tbl[(bit)>>3]	/* identify byte in array */
#define	B(bit)		(1 << ((bit) & 0x07))	/* identify bit in byte */
#define	SET(bit)	(A(bit) |= B(bit))
#define	CLR(bit)	(A(bit) &= (~B(bit)))
#define	TST(bit)	(A(bit) & B(bit))

/* various options */
int options;
#define	F_FLOOD		0x0001
#define	F_INTERVAL	0x0002
#define	F_HOSTNAME	0x0004
#define	F_PINGFILLED	0x0008
#define	F_QUIET		0x0010
/*			0x0020 */
#define	F_SO_DEBUG	0x0040
/*			0x0080 */
#define	F_VERBOSE	0x0100
/*			0x0200 */
/*			0x0400 */
/*			0x0800 */
/*			0x1000 */
#define	F_AUD_RECV	0x2000
#define	F_AUD_MISS	0x4000

/* multicast options */
int moptions;
#define	MULTICAST_NOLOOP	0x001

#define DUMMY_PORT	10101

/*
 * MAX_DUP_CHK is the number of bits in received table, i.e. the maximum
 * number of received sequence numbers we can keep track of.  Change 128
 * to 8192 for complete accuracy...
 */
#define	MAX_DUP_CHK	(8 * 8192)
int mx_dup_ck = MAX_DUP_CHK;
char rcvd_tbl[MAX_DUP_CHK / 8];

struct sockaddr_in6 dst;	/* who to ping6 */

int datalen = DEFDATALEN;
int s;				/* socket file descriptor */
u_char outpack[IPV6_MAXPACKET];
char BSPACE = '\b';		/* characters written for flood */
char DOT = '.';
char *hostname;
int ident;			/* process id to identify our packets */
int hoplimit = -1;		/* hoplimit */

/* counters */
int64_t npackets;		/* max packets to transmit */
int64_t nreceived;		/* # of packets we got back */
int64_t nrepeats;		/* number of duplicates */
int64_t ntransmitted;		/* sequence # for outbound packets = #sent */
int64_t nmissedmax = 1;		/* max value of ntransmitted - nreceived - 1 */
struct timeval interval = {1, 0}; /* interval between packets */

/* timing */
int timing;			/* flag to do timing */
int timinginfo;
unsigned int maxwait = MAXWAIT_DEFAULT;	/* max seconds to wait for response */
double tmin = 999999999.0;	/* minimum round trip time */
double tmax = 0.0;		/* maximum round trip time */
double tsum = 0.0;		/* sum of all times, for doing average */
double tsumsq = 0.0;		/* sum of all times squared, for std. dev. */

struct tv64 tv64_offset;
SIPHASH_KEY mac_key;

struct msghdr smsghdr;
struct iovec smsgiov;

volatile sig_atomic_t seenalrm;
volatile sig_atomic_t seenint;
volatile sig_atomic_t seeninfo;

void			 fill(char *, char *);
void			 summary(void);
void			 onsignal(int);
void			 retransmit(void);
int			 pinger(void);
const char		*pr_addr(struct sockaddr *, socklen_t);
void			 pr_pack(u_char *, int, struct msghdr *);
__dead void		 usage(void);

int			 get_hoplim(struct msghdr *);
int			 get_pathmtu(struct msghdr *);
void			 pr_icmph(struct icmp6_hdr *, u_char *);
void			 pr_iph(struct ip6_hdr *);
void			 pr_exthdrs(struct msghdr *);
void			 pr_ip6opt(void *);
void			 pr_rthdr(void *);
void			 pr_retip(struct ip6_hdr *, u_char *);

int
main(int argc, char *argv[])
{
	struct addrinfo hints, *res;
	struct itimerval itimer;
	struct sockaddr_in6 from, from6;
	struct cmsghdr *scmsg = NULL;
	struct in6_pktinfo *pktinfo = NULL;
	socklen_t maxsizelen;
	int64_t preload;
	int ch, i, optval = 1, packlen, maxsize, error;
	u_char *datap, *packet, loop = 1;
	char *e, *target, hbuf[NI_MAXHOST], *source = NULL;
	const char *errstr;
	double intval;
	int mflag = 0;
	uid_t uid;
	u_int rtableid = 0;

	if ((s = socket(AF_INET6, SOCK_RAW, IPPROTO_ICMPV6)) < 0)
		err(1, "socket");

	/* revoke privs */
	uid = getuid();
	if (setresuid(uid, uid, uid) == -1)
		err(1, "setresuid");

	preload = 0;
	datap = &outpack[ECHOLEN + ECHOTMLEN];
	while ((ch = getopt(argc, argv,
	    "c:dEefHh:I:i:Ll:mNnp:qS:s:V:vw:")) != -1) {
		switch (ch) {
		case 'c':
			npackets = strtonum(optarg, 0, INT64_MAX, &errstr);
			if (errstr)
				errx(1,
				    "number of packets to transmit is %s: %s",
				    errstr, optarg);
			break;
		case 'd':
			options |= F_SO_DEBUG;
			break;
		case 'E':
			options |= F_AUD_MISS;
			break;
		case 'e':
			options |= F_AUD_RECV;
			break;
		case 'f':
			if (getuid())
				errx(1, "%s", strerror(EPERM));
			options |= F_FLOOD;
			setvbuf(stdout, NULL, _IONBF, 0);
			break;
		case 'H':
			options |= F_HOSTNAME;
			break;
		case 'h':		/* hoplimit */
			hoplimit = strtonum(optarg, 0, IPV6_MAXHLIM, &errstr);
			if (errstr)
				errx(1, "hoplimit is %s: %s", errstr, optarg);
			break;
		case 'I':
		case 'S':	/* deprecated */
			source = optarg;
			break;
		case 'i':		/* wait between sending packets */
			intval = strtod(optarg, &e);
			if (*optarg == '\0' || *e != '\0')
				errx(1, "illegal timing interval %s", optarg);
			if (intval < 1 && getuid()) {
				errx(1, "%s: only root may use interval < 1s",
				    strerror(EPERM));
			}
			interval.tv_sec = (time_t)intval;
			interval.tv_usec =
			    (long)((intval - interval.tv_sec) * 1000000);
			if (interval.tv_sec < 0)
				errx(1, "illegal timing interval %s", optarg);
			/* less than 1/Hz does not make sense */
			if (interval.tv_sec == 0 && interval.tv_usec < 10000) {
				warnx("too small interval, raised to 0.01");
				interval.tv_usec = 10000;
			}
			options |= F_INTERVAL;
			break;
		case 'L':
			moptions |= MULTICAST_NOLOOP;
			loop = 0;
			break;
		case 'l':
			if (getuid())
				errx(1, "%s", strerror(EPERM));
			preload = strtonum(optarg, 1, INT64_MAX, &errstr);
			if (errstr)
				errx(1, "preload value is %s: %s", errstr,
				    optarg);
			break;
		case 'm':
			mflag++;
			break;
		case 'n':
			options &= ~F_HOSTNAME;
			break;
		case 'p':		/* fill buffer with user pattern */
			options |= F_PINGFILLED;
			fill((char *)datap, optarg);
				break;
		case 'q':
			options |= F_QUIET;
			break;
		case 's':		/* size of packet to send */
			datalen = strtonum(optarg, 0, MAXPAYLOAD, &errstr);
			if (errstr)
				errx(1, "packet size is %s: %s", errstr,
				    optarg);
			break;
		case 'V':
			rtableid = strtonum(optarg, 0, RT_TABLEID_MAX, &errstr);
			if (errstr)
				errx(1, "rtable value is %s: %s", errstr,
				    optarg);
			if (setsockopt(s, SOL_SOCKET, SO_RTABLE, &rtableid,
			    sizeof(rtableid)) == -1)
				err(1, "setsockopt SO_RTABLE");
			break;
		case 'v':
			options |= F_VERBOSE;
			break;
		case 'w':
			maxwait = strtonum(optarg, 1, INT_MAX, &errstr);
			if (errstr)
				errx(1, "maxwait value is %s: %s",
				    errstr, optarg);
			break;
		default:
			usage();
		}
	}

	argc -= optind;
	argv += optind;

	if (argc != 1)
		usage();

	memset(&dst, 0, sizeof(dst));

#if 0
	if (inet_aton(*argv, &dst.sin_addr) != 0) {
		hostname = *argv;
		if ((target = strdup(inet_ntoa(dst.sin_addr))) == NULL)
			errx(1, "malloc");
	} else
#endif
		target = *argv;

	memset(&hints, 0, sizeof(hints));
	hints.ai_family = PF_INET6;
	hints.ai_socktype = SOCK_RAW;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_CANONNAME;
	if ((error = getaddrinfo(target, NULL, &hints, &res)))
		errx(1, "%s", gai_strerror(error));

	switch (res->ai_family) {
	case AF_INET6:
		if (res->ai_addrlen != sizeof(dst))
		    errx(1, "size of sockaddr mismatch");
		break;
	case AF_INET:
	default:
		errx(1, "unsupported AF: %d", res->ai_family);
		break;
	}

	memcpy(&dst, res->ai_addr, res->ai_addrlen);

	if (!hostname) {
		hostname = res->ai_canonname ? strdup(res->ai_canonname) :
		    target;
		if (!hostname)
			errx(1, "malloc");
	}

	if (res->ai_next) {
		if (getnameinfo(res->ai_addr, res->ai_addrlen, hbuf,
		    sizeof(hbuf), NULL, 0, NI_NUMERICHOST) != 0)
			strlcpy(hbuf, "?", sizeof(hbuf));
		warnx("Warning: %s has multiple "
		    "addresses; using %s", hostname, hbuf);
	}
	freeaddrinfo(res);

	if (source) {
		memset(&hints, 0, sizeof(struct addrinfo));
		hints.ai_flags = AI_NUMERICHOST; /* allow hostname? */
		hints.ai_family = AF_INET6;
		hints.ai_socktype = SOCK_RAW;
		hints.ai_protocol = IPPROTO_ICMPV6;

		error = getaddrinfo(source, NULL, &hints, &res);
		if (error)
			errx(1, "invalid source address: %s", 
			     gai_strerror(error));

		if (res->ai_family != AF_INET6 || res->ai_addrlen !=
		    sizeof(from6))
			errx(1, "invalid source address");
		memcpy(&from6, res->ai_addr, sizeof(from6));
		freeaddrinfo(res);
		if (bind(s, (struct sockaddr *)&from6, sizeof(from6)) != 0)
			err(1, "bind");
	}

	/*
	 * let the kernel pass extension headers of incoming packets,
	 * for privileged socket options
	 */
	if ((options & F_VERBOSE) != 0) {
		int opton = 1;

		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVHOPOPTS, &opton,
		    (socklen_t)sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVHOPOPTS)");
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVDSTOPTS, &opton,
		    (socklen_t)sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVDSTOPTS)");
	}

	if ((options & F_FLOOD) && (options & F_INTERVAL))
		errx(1, "-f and -i incompatible options");

	if ((options & F_FLOOD) && (options & (F_AUD_RECV | F_AUD_MISS)))
		warnx("No audible output for flood pings");

	if ((moptions & MULTICAST_NOLOOP) &&
	    setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_LOOP, &loop,
	    sizeof(loop)) < 0)
		err(1, "setsockopt IP6_MULTICAST_LOOP");

	if (datalen >= sizeof(struct payload)) {
		/* we can time transfer */
		timing = 1;
	} else
		timing = 0;
	/* in F_VERBOSE case, we may get non-echoreply packets*/
	if (options & F_VERBOSE && datalen < 2048)
		packlen = 2048 + IP6LEN + ECHOLEN + EXTRA; /* XXX 2048? */
	else
		packlen = datalen + IP6LEN + ECHOLEN + EXTRA;

	if (!(packet = malloc(packlen)))
		err(1, "Unable to allocate packet");

	/*
	 * When trying to send large packets, you must increase the
	 * size of both the send and receive buffers...
	 */
	maxsizelen = sizeof maxsize;
	if (getsockopt(s, SOL_SOCKET, SO_SNDBUF, &maxsize, &maxsizelen) < 0)
		err(1, "getsockopt");
	if (maxsize < packlen &&
	    setsockopt(s, SOL_SOCKET, SO_SNDBUF, &packlen, sizeof(maxsize)) < 0)
		err(1, "setsockopt");

	if (getsockopt(s, SOL_SOCKET, SO_RCVBUF, &maxsize, &maxsizelen) < 0)
		err(1, "getsockopt");
	if (maxsize < packlen &&
	    setsockopt(s, SOL_SOCKET, SO_RCVBUF, &packlen, sizeof(maxsize)) < 0)
		err(1, "setsockopt");

	if (!(options & F_PINGFILLED))
		for (i = ECHOLEN; i < packlen; ++i)
			*datap++ = i;

	ident = getpid() & 0xFFFF;

	optval = 1;

	if (options & F_SO_DEBUG)
		(void)setsockopt(s, SOL_SOCKET, SO_DEBUG, &optval,
		    (socklen_t)sizeof(optval));
	optval = IPV6_DEFHLIM;
	if (IN6_IS_ADDR_MULTICAST(&dst.sin6_addr))
		if (setsockopt(s, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
		    &optval, (socklen_t)sizeof(optval)) == -1)
			err(1, "IPV6_MULTICAST_HOPS");
	if (mflag != 1) {
		optval = mflag > 1 ? 0 : 1;

		if (setsockopt(s, IPPROTO_IPV6, IPV6_USE_MIN_MTU,
		    &optval, (socklen_t)sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_USE_MIN_MTU)");
	} else {
		optval = 1;
		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPATHMTU,
		    &optval, sizeof(optval)) == -1)
			err(1, "setsockopt(IPV6_RECVPATHMTU)");
	}


    {
	struct icmp6_filter filt;
	if (!(options & F_VERBOSE)) {
		ICMP6_FILTER_SETBLOCKALL(&filt);
		ICMP6_FILTER_SETPASS(ICMP6_ECHO_REPLY, &filt);
	} else {
		ICMP6_FILTER_SETPASSALL(&filt);
	}
	if (setsockopt(s, IPPROTO_ICMPV6, ICMP6_FILTER, &filt,
	    (socklen_t)sizeof(filt)) < 0)
		err(1, "setsockopt(ICMP6_FILTER)");
    }

	/* let the kernel pass extension headers of incoming packets */
	if ((options & F_VERBOSE) != 0) {
		int opton = 1;

		if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVRTHDR, &opton,
		    sizeof(opton)))
			err(1, "setsockopt(IPV6_RECVRTHDR)");
	}

	if (hoplimit != -1) {
		/* set IP6 packet options */
		if ((scmsg = malloc( CMSG_SPACE(sizeof(int)))) == NULL)
			errx(1, "can't allocate enough memory");
		smsghdr.msg_control = (caddr_t)scmsg;
		smsghdr.msg_controllen =  CMSG_SPACE(sizeof(int));

		scmsg->cmsg_len = CMSG_LEN(sizeof(int));
		scmsg->cmsg_level = IPPROTO_IPV6;
		scmsg->cmsg_type = IPV6_HOPLIMIT;
		*(int *)(CMSG_DATA(scmsg)) = hoplimit;
	}

	if (!source && options & F_VERBOSE) {
		/*
		 * get the source address. XXX since we revoked the root
		 * privilege, we cannot use a raw socket for this.
		 */
		int dummy;
		socklen_t len = sizeof(from6);

		if ((dummy = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			err(1, "UDP socket");

		from6.sin6_family = AF_INET6;
		from6.sin6_addr = dst.sin6_addr;
		from6.sin6_port = ntohs(DUMMY_PORT);
		from6.sin6_scope_id = dst.sin6_scope_id;

		if (pktinfo &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_PKTINFO,
		    (void *)pktinfo, sizeof(*pktinfo)))
			err(1, "UDP setsockopt(IPV6_PKTINFO)");

		if (hoplimit != -1 &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
		    (void *)&hoplimit, sizeof(hoplimit)))
			err(1, "UDP setsockopt(IPV6_UNICAST_HOPS)");

		if (hoplimit != -1 &&
		    setsockopt(dummy, IPPROTO_IPV6, IPV6_MULTICAST_HOPS,
		    (void *)&hoplimit, sizeof(hoplimit)))
			err(1, "UDP setsockopt(IPV6_MULTICAST_HOPS)");

		if (rtableid > 0 &&
		    setsockopt(dummy, SOL_SOCKET, SO_RTABLE, &rtableid,
		    sizeof(rtableid)) < 0)
			err(1, "setsockopt(SO_RTABLE)");

		if (connect(dummy, (struct sockaddr *)&from6, len) < 0)
			err(1, "UDP connect");

		if (getsockname(dummy, (struct sockaddr *)&from6, &len) < 0)
			err(1, "getsockname");

		close(dummy);
	}

	optval = 1;
	if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVPKTINFO, &optval,
	    (socklen_t)sizeof(optval)) < 0)
		warn("setsockopt(IPV6_RECVPKTINFO)"); /* XXX err? */
	if (setsockopt(s, IPPROTO_IPV6, IPV6_RECVHOPLIMIT, &optval,
	    (socklen_t)sizeof(optval)) < 0)
		warn("setsockopt(IPV6_RECVHOPLIMIT)"); /* XXX err? */

	if (options & F_HOSTNAME) {
		if (pledge("stdio inet dns", NULL) == -1)
			err(1, "pledge");
	} else {
		if (pledge("stdio inet", NULL) == -1)
			err(1, "pledge");
	}

	arc4random_buf(&tv64_offset, sizeof(tv64_offset));
	arc4random_buf(&mac_key, sizeof(mac_key));

	printf("PING6 %s (", hostname);
	if (options & F_VERBOSE)
		printf("%s --> ", pr_addr((struct sockaddr *)&from6,
		    sizeof(from6)));
	printf("%s): %d data bytes\n", pr_addr((struct sockaddr *)&dst,
	    sizeof(dst)), datalen);

	smsghdr.msg_name = &dst;
	smsghdr.msg_namelen = sizeof(dst);
	smsgiov.iov_base = (caddr_t)outpack;
	smsghdr.msg_iov = &smsgiov;
	smsghdr.msg_iovlen = 1;

	while (preload--)		/* Fire off them quickies. */
		pinger();

	(void)signal(SIGINT, onsignal);
	(void)signal(SIGINFO, onsignal);

	if ((options & F_FLOOD) == 0) {
		(void)signal(SIGALRM, onsignal);
		itimer.it_interval = interval;
		itimer.it_value = interval;
		(void)setitimer(ITIMER_REAL, &itimer, NULL);
		if (ntransmitted == 0)
			retransmit();
	}

	seenalrm = seenint = 0;
	seeninfo = 0;

	for (;;) {
		struct msghdr	m;
		union {
			struct cmsghdr hdr;
			u_char buf[CMSG_SPACE(1024)];
		}		cmsgbuf;
		struct iovec	iov[1];
		struct pollfd	pfd;
		ssize_t		cc;
		int		timeout;

		/* signal handling */
		if (seenint)
			break;
		if (seenalrm) {
			retransmit();
			seenalrm = 0;
			if (ntransmitted - nreceived - 1 > nmissedmax) {
				nmissedmax = ntransmitted - nreceived - 1;
				if (!(options & F_FLOOD) &&
				    (options & F_AUD_MISS))
					(void)fputc('\a', stderr);
			}
			continue;
		}
		if (seeninfo) {
			summary();
			seeninfo = 0;
			continue;
		}

		if (options & F_FLOOD) {
			(void)pinger();
			timeout = 10;
		} else
			timeout = INFTIM;

		pfd.fd = s;
		pfd.events = POLLIN;

		if (poll(&pfd, 1, timeout) <= 0)
			continue;

		m.msg_name = &from;
		m.msg_namelen = sizeof(from);
		memset(&iov, 0, sizeof(iov));
		iov[0].iov_base = (caddr_t)packet;
		iov[0].iov_len = packlen;
		m.msg_iov = iov;
		m.msg_iovlen = 1;
		m.msg_control = (caddr_t)&cmsgbuf.buf;
		m.msg_controllen = sizeof(cmsgbuf.buf);

		cc = recvmsg(s, &m, 0);
		if (cc < 0) {
			if (errno != EINTR) {
				warn("recvmsg");
				sleep(1);
			}
			continue;
		} else if (cc == 0) {
			int mtu;

			/*
			 * receive control messages only. Process the
			 * exceptions (currently the only possibility is
			 * a path MTU notification.)
			 */
			if ((mtu = get_pathmtu(&m)) > 0) {
				if ((options & F_VERBOSE) != 0) {
					printf("new path MTU (%d) is "
					    "notified\n", mtu);
				}
			}
			continue;
		} else
			pr_pack(packet, cc, &m);

		if (npackets && nreceived >= npackets)
			break;
	}
	summary();
	exit(nreceived == 0);
}

void
onsignal(int sig)
{
	switch (sig) {
	case SIGALRM:
		seenalrm++;
		break;
	case SIGINT:
		seenint++;
		break;
	case SIGINFO:
		seeninfo++;
		break;
	}
}

void
fill(char *bp, char *patp)
{
	int ii, jj, kk;
	int pat[16];
	char *cp;

	for (cp = patp; *cp; cp++)
		if (!isxdigit((unsigned char)*cp))
			errx(1, "patterns must be specified as hex digits");
	ii = sscanf(patp,
	    "%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x%2x",
	    &pat[0], &pat[1], &pat[2], &pat[3], &pat[4], &pat[5], &pat[6],
	    &pat[7], &pat[8], &pat[9], &pat[10], &pat[11], &pat[12],
	    &pat[13], &pat[14], &pat[15]);

	if (ii > 0)
		for (kk = 0;
		    kk <= MAXPAYLOAD - (ECHOLEN + ECHOTMLEN + ii);
		    kk += ii)
			for (jj = 0; jj < ii; ++jj)
				bp[jj + kk] = pat[jj];
	if (!(options & F_QUIET)) {
		(void)printf("PATTERN: 0x");
		for (jj = 0; jj < ii; ++jj)
			(void)printf("%02x", bp[jj] & 0xFF);
		(void)printf("\n");
	}
}

void
summary(void)
{
	printf("\n--- %s ping6 statistics ---\n", hostname);
	printf("%lld packets transmitted, ", ntransmitted);
	printf("%lld packets received, ", nreceived);

	if (nrepeats)
		printf("%lld duplicates, ", nrepeats);
	if (ntransmitted) {
		if (nreceived > ntransmitted)
			printf("-- somebody's duplicating packets!");
		else
			printf("%.1f%% packet loss",
			    ((((double)ntransmitted - nreceived) * 100) /
			    ntransmitted));
	}
	printf("\n");
	if (timinginfo) {
		/* Only display average to microseconds */
		double num = nreceived + nrepeats;
		double avg = tsum / num;
		double dev = sqrt(fmax(0, tsumsq / num - avg * avg));
		printf("round-trip min/avg/max/std-dev = %.3f/%.3f/%.3f/%.3f ms\n",
		    tmin, avg, tmax, dev);
	}
}

/*
 * pr_addr --
 *	Return address in numeric form or a host name
 */
const char *
pr_addr(struct sockaddr *addr, socklen_t addrlen)
{
	static char buf[NI_MAXHOST];
	int flag = 0;

	if ((options & F_HOSTNAME) == 0)
		flag |= NI_NUMERICHOST;

	if (getnameinfo(addr, addrlen, buf, sizeof(buf), NULL, 0, flag) == 0)
		return (buf);
	else
		return "?";
}

/*
 * retransmit --
 *	This routine transmits another ping6.
 */
void
retransmit(void)
{
	struct itimerval itimer;
	static int last_time = 0;

	if (last_time) {
		seenint = 1;	/* break out of ping event loop */
		return;
	}

	if (pinger() == 0)
		return;

	/*
	 * If we're not transmitting any more packets, change the timer
	 * to wait two round-trip times if we've received any packets or
	 * maxwait seconds if we haven't.
	 */
	if (nreceived) {
		itimer.it_value.tv_sec =  2 * tmax / 1000;
		if (itimer.it_value.tv_sec == 0)
			itimer.it_value.tv_sec = 1;
	} else
		itimer.it_value.tv_sec = maxwait;
	itimer.it_interval.tv_sec = 0;
	itimer.it_interval.tv_usec = 0;
	itimer.it_value.tv_usec = 0;
	(void)setitimer(ITIMER_REAL, &itimer, NULL);

	/* When the alarm goes off we are done. */
	last_time = 1;
}

/*
 * pinger --
 *	Compose and transmit an ICMP ECHO REQUEST packet.  The IP packet
 * will be added on by the kernel.  The ID field is our UNIX process ID,
 * and the sequence number is an ascending integer.  The first 8 bytes
 * of the data portion are used to hold a UNIX "timeval" struct in VAX
 * byte-order, to compute the round-trip time.
 */
int
pinger(void)
{
	struct icmp6_hdr *icp;
	int cc, i;
	u_int16_t seq;

	if (npackets && ntransmitted >= npackets)
		return(-1);	/* no more transmission */

	seq = htons(ntransmitted++);

	icp = (struct icmp6_hdr *)outpack;
	memset(icp, 0, sizeof(*icp));
	icp->icmp6_cksum = 0;
	icp->icmp6_type = ICMP6_ECHO_REQUEST;
	icp->icmp6_code = 0;
	icp->icmp6_id = htons(ident);
	icp->icmp6_seq = seq;

	CLR(ntohs(seq) % mx_dup_ck);

	if (timing) {
		SIPHASH_CTX ctx;
		struct timespec ts;
		struct payload payload;
		struct tv64 *tv64 = &payload.tv64;

		if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
			err(1, "clock_gettime(CLOCK_MONOTONIC)");
		tv64->tv64_sec = htobe64((u_int64_t)ts.tv_sec +
		    tv64_offset.tv64_sec);
		tv64->tv64_nsec = htobe64((u_int64_t)ts.tv_nsec +
		    tv64_offset.tv64_nsec);

		SipHash24_Init(&ctx, &mac_key);
		SipHash24_Update(&ctx, tv64, sizeof(*tv64));
		SipHash24_Update(&ctx, &ident, sizeof(ident));
		SipHash24_Update(&ctx, &seq, sizeof(seq));
		SipHash24_Final(&payload.mac, &ctx);

		memcpy(&outpack[ECHOLEN], &payload, sizeof(payload));
	}
	cc = ECHOLEN + datalen;

	smsgiov.iov_len = cc;

	i = sendmsg(s, &smsghdr, 0);

	if (i < 0 || i != cc)  {
		if (i < 0)
			warn("sendmsg");
		printf("ping6: wrote %s %d chars, ret=%d\n", hostname, cc, i);
	}
	if (!(options & F_QUIET) && options & F_FLOOD)
		(void)write(STDOUT_FILENO, &DOT, 1);

	return (0);
}

#define MINIMUM(a,b) (((a)<(b))?(a):(b))

/*
 * pr_pack --
 *	Print out the packet, if it came from us.  This logic is necessary
 * because ALL readers of the ICMP socket get a copy of ALL ICMP packets
 * which arrive ('tis only fair).  This permits multiple copies of this
 * program to be run without having intermingled output (or statistics!).
 */
void
pr_pack(u_char *buf, int cc, struct msghdr *mhdr)
{
	struct icmp6_hdr *icp;
	struct timespec ts, tp;
	struct payload payload;
	struct sockaddr *from;
	socklen_t fromlen;
	double triptime = 0;
	int i, dupflag;
	int hoplim;
	u_int16_t seq;
	u_char *cp = NULL, *dp, *end = buf + cc;

	if (clock_gettime(CLOCK_MONOTONIC, &ts) == -1)
		err(1, "clock_gettime(CLOCK_MONOTONIC)");

	if (!mhdr || !mhdr->msg_name ||
	    mhdr->msg_namelen != sizeof(struct sockaddr_in6) ||
	    ((struct sockaddr *)mhdr->msg_name)->sa_family != AF_INET6) {
		if (options & F_VERBOSE)
			warnx("invalid peername");
		return;
	}
	from = (struct sockaddr *)mhdr->msg_name;
	fromlen = mhdr->msg_namelen;
	if (cc < sizeof(struct icmp6_hdr)) {
		if (options & F_VERBOSE)
			warnx("packet too short (%d bytes) from %s", cc,
			    pr_addr(from, fromlen));
		return;
	}
	icp = (struct icmp6_hdr *)buf;

	if ((hoplim = get_hoplim(mhdr)) == -1) {
		warnx("failed to get receiving hop limit");
		return;
	}

	if (icp->icmp6_type == ICMP6_ECHO_REPLY) {
		if (ntohs(icp->icmp6_id) != ident)
			return;			/* 'Twas not our ECHO */
		seq = icp->icmp6_seq;
		++nreceived;
		if (cc >= ECHOLEN + ECHOTMLEN) {
			SIPHASH_CTX ctx;
			struct tv64 *tv64;
			u_int8_t mac[SIPHASH_DIGEST_LENGTH];

			memcpy(&payload, icp + 1, sizeof(payload));
			tv64 = &payload.tv64;

			SipHash24_Init(&ctx, &mac_key);
			SipHash24_Update(&ctx, tv64, sizeof(*tv64));
			SipHash24_Update(&ctx, &ident, sizeof(ident));
			SipHash24_Update(&ctx, &seq, sizeof(seq));
			SipHash24_Final(mac, &ctx);

			if (timingsafe_memcmp(mac, &payload.mac,
			    sizeof(mac)) != 0) {
				(void)printf("signature mismatch!\n");
				return;
			}
			timinginfo=1;

			tp.tv_sec = betoh64(tv64->tv64_sec) -
			    tv64_offset.tv64_sec;
			tp.tv_nsec = betoh64(tv64->tv64_nsec) -
			    tv64_offset.tv64_nsec;

			timespecsub(&ts, &tp, &ts);
			triptime = ((double)ts.tv_sec) * 1000.0 +
			    ((double)ts.tv_nsec) / 1000000.0;
			tsum += triptime;
			tsumsq += triptime * triptime;
			if (triptime < tmin)
				tmin = triptime;
			if (triptime > tmax)
				tmax = triptime;
		}

		if (TST(ntohs(seq) % mx_dup_ck)) {
			++nrepeats;
			--nreceived;
			dupflag = 1;
		} else {
			SET(ntohs(seq) % mx_dup_ck);
			dupflag = 0;
		}

		if (options & F_QUIET)
			return;

		if (options & F_FLOOD)
			(void)write(STDOUT_FILENO, &BSPACE, 1);
		else {
			(void)printf("%d bytes from %s: icmp_seq=%u", cc,
			    pr_addr(from, fromlen), ntohs(seq));
			(void)printf(" hlim=%d", hoplim);
			if (timinginfo)
				(void)printf(" time=%.3f ms", triptime);
			if (dupflag)
				(void)printf(" (DUP!)");
			/* check the data */
			cp = buf + ECHOLEN + ECHOTMLEN;
			dp = outpack + ECHOLEN + ECHOTMLEN;
			if (cc != ECHOLEN + datalen) {
				int delta = cc - (datalen + ECHOLEN);

				(void)printf(" (%d bytes %s)",
				    abs(delta), delta > 0 ? "extra" : "short");
				end = buf + MINIMUM(cc, ECHOLEN + datalen);
			}
			for (i = ECHOLEN; cp < end; ++i, ++cp, ++dp) {
				if (*cp != *dp) {
					(void)printf("\nwrong data byte #%d "
					    "should be 0x%x but was 0x%x",
					    i, *dp, *cp);
					break;
				}
			}
		}
	} else {
		/* We've got something other than an ECHOREPLY */
		if (!(options & F_VERBOSE))
			return;
		(void)printf("%d bytes from %s: ", cc, pr_addr(from, fromlen));
		pr_icmph(icp, end);
	}

	if (!(options & F_FLOOD)) {
		(void)putchar('\n');
		if (options & F_VERBOSE)
			pr_exthdrs(mhdr);
		(void)fflush(stdout);
		if (options & F_AUD_RECV)
			(void)fputc('\a', stderr);
	}
}

void
pr_exthdrs(struct msghdr *mhdr)
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_level != IPPROTO_IPV6)
			continue;

		switch (cm->cmsg_type) {
		case IPV6_HOPOPTS:
			printf("  HbH Options: ");
			pr_ip6opt(CMSG_DATA(cm));
			break;
		case IPV6_DSTOPTS:
		case IPV6_RTHDRDSTOPTS:
			printf("  Dst Options: ");
			pr_ip6opt(CMSG_DATA(cm));
			break;
		case IPV6_RTHDR:
			printf("  Routing: ");
			pr_rthdr(CMSG_DATA(cm));
			break;
		}
	}
}

void
pr_ip6opt(void *extbuf)
{
	struct ip6_hbh *ext;
	int currentlen;
	u_int8_t type;
	size_t extlen;
	socklen_t len;
	void *databuf;
	u_int16_t value2;
	u_int32_t value4;

	ext = (struct ip6_hbh *)extbuf;
	extlen = (ext->ip6h_len + 1) * 8;
	printf("nxt %u, len %u (%lu bytes)\n", ext->ip6h_nxt,
	    (unsigned int)ext->ip6h_len, (unsigned long)extlen);

	currentlen = 0;
	while (1) {
		currentlen = inet6_opt_next(extbuf, extlen, currentlen,
		    &type, &len, &databuf);
		if (currentlen == -1)
			break;
		switch (type) {
		/*
		 * Note that inet6_opt_next automatically skips any padding
		 * options.
		 */
		case IP6OPT_JUMBO:
			inet6_opt_get_val(databuf, 0, &value4, sizeof(value4));
			printf("    Jumbo Payload Opt: Length %u\n",
			    (u_int32_t)ntohl(value4));
			break;
		case IP6OPT_ROUTER_ALERT:
			inet6_opt_get_val(databuf, 0, &value2, sizeof(value2));
			printf("    Router Alert Opt: Type %u\n",
			    ntohs(value2));
			break;
		default:
			printf("    Received Opt %u len %lu\n",
			    type, (unsigned long)len);
			break;
		}
	}
	return;
}

void
pr_rthdr(void *extbuf)
{
	struct in6_addr *in6;
	char ntopbuf[INET6_ADDRSTRLEN];
	struct ip6_rthdr *rh = (struct ip6_rthdr *)extbuf;
	int i, segments;

	/* print fixed part of the header */
	printf("nxt %u, len %u (%d bytes), type %u, ", rh->ip6r_nxt,
	    rh->ip6r_len, (rh->ip6r_len + 1) << 3, rh->ip6r_type);
	if ((segments = inet6_rth_segments(extbuf)) >= 0)
		printf("%d segments, ", segments);
	else
		printf("segments unknown, ");
	printf("%d left\n", rh->ip6r_segleft);

	for (i = 0; i < segments; i++) {
		in6 = inet6_rth_getaddr(extbuf, i);
		if (in6 == NULL)
			printf("   [%d]<NULL>\n", i);
		else {
			if (!inet_ntop(AF_INET6, in6, ntopbuf,
			    sizeof(ntopbuf)))
				strncpy(ntopbuf, "?", sizeof(ntopbuf));
			printf("   [%d]%s\n", i, ntopbuf);
		}
	}

	return;

}

int
get_hoplim(struct msghdr *mhdr)
{
	struct cmsghdr *cm;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(-1);

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			return(*(int *)CMSG_DATA(cm));
	}

	return(-1);
}

int
get_pathmtu(struct msghdr *mhdr)
{
	struct cmsghdr *cm;
	struct ip6_mtuinfo *mtuctl = NULL;

	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(mhdr); cm;
	     cm = (struct cmsghdr *)CMSG_NXTHDR(mhdr, cm)) {
		if (cm->cmsg_len == 0)
			return(0);

		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PATHMTU &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct ip6_mtuinfo))) {
			mtuctl = (struct ip6_mtuinfo *)CMSG_DATA(cm);

			/*
			 * If the notified destination is different from
			 * the one we are pinging, just ignore the info.
			 * We check the scope ID only when both notified value
			 * and our own value have non-0 values, because we may
			 * have used the default scope zone ID for sending,
			 * in which case the scope ID value is 0.
			 */
			if (!IN6_ARE_ADDR_EQUAL(&mtuctl->ip6m_addr.sin6_addr,
						&dst.sin6_addr) ||
			    (mtuctl->ip6m_addr.sin6_scope_id &&
			     dst.sin6_scope_id &&
			     mtuctl->ip6m_addr.sin6_scope_id !=
			     dst.sin6_scope_id)) {
				if ((options & F_VERBOSE) != 0) {
					printf("path MTU for %s is notified. "
					       "(ignored)\n",
					   pr_addr((struct sockaddr *)&mtuctl->ip6m_addr,
					   sizeof(mtuctl->ip6m_addr)));
				}
				return(0);
			}

			/*
			 * Ignore an invalid MTU. XXX: can we just believe
			 * the kernel check?
			 */
			if (mtuctl->ip6m_mtu < IPV6_MMTU)
				return(0);

			/* notification for our destination. return the MTU. */
			return((int)mtuctl->ip6m_mtu);
		}
	}
	return(0);
}

/*
 * pr_icmph --
 *	Print a descriptive string about an ICMP header.
 */
void
pr_icmph(struct icmp6_hdr *icp, u_char *end)
{
	char ntop_buf[INET6_ADDRSTRLEN];
	struct nd_redirect *red;

	switch (icp->icmp6_type) {
	case ICMP6_DST_UNREACH:
		switch (icp->icmp6_code) {
		case ICMP6_DST_UNREACH_NOROUTE:
			(void)printf("No Route to Destination\n");
			break;
		case ICMP6_DST_UNREACH_ADMIN:
			(void)printf("Destination Administratively "
			    "Unreachable\n");
			break;
		case ICMP6_DST_UNREACH_BEYONDSCOPE:
			(void)printf("Destination Unreachable Beyond Scope\n");
			break;
		case ICMP6_DST_UNREACH_ADDR:
			(void)printf("Destination Host Unreachable\n");
			break;
		case ICMP6_DST_UNREACH_NOPORT:
			(void)printf("Destination Port Unreachable\n");
			break;
		default:
			(void)printf("Destination Unreachable, Bad Code: %d\n",
			    icp->icmp6_code);
			break;
		}
		/* Print returned IP header information */
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_PACKET_TOO_BIG:
		(void)printf("Packet too big mtu = %d\n",
		    (int)ntohl(icp->icmp6_mtu));
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_TIME_EXCEEDED:
		switch (icp->icmp6_code) {
		case ICMP6_TIME_EXCEED_TRANSIT:
			(void)printf("Time to live exceeded\n");
			break;
		case ICMP6_TIME_EXCEED_REASSEMBLY:
			(void)printf("Frag reassembly time exceeded\n");
			break;
		default:
			(void)printf("Time exceeded, Bad Code: %d\n",
			    icp->icmp6_code);
			break;
		}
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_PARAM_PROB:
		(void)printf("Parameter problem: ");
		switch (icp->icmp6_code) {
		case ICMP6_PARAMPROB_HEADER:
			(void)printf("Erroneous Header ");
			break;
		case ICMP6_PARAMPROB_NEXTHEADER:
			(void)printf("Unknown Nextheader ");
			break;
		case ICMP6_PARAMPROB_OPTION:
			(void)printf("Unrecognized Option ");
			break;
		default:
			(void)printf("Bad code(%d) ", icp->icmp6_code);
			break;
		}
		(void)printf("pointer = 0x%02x\n",
		    (u_int32_t)ntohl(icp->icmp6_pptr));
		pr_retip((struct ip6_hdr *)(icp + 1), end);
		break;
	case ICMP6_ECHO_REQUEST:
		(void)printf("Echo Request");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_ECHO_REPLY:
		(void)printf("Echo Reply");
		/* XXX ID + Seq + Data */
		break;
	case ICMP6_MEMBERSHIP_QUERY:
		(void)printf("Listener Query");
		break;
	case ICMP6_MEMBERSHIP_REPORT:
		(void)printf("Listener Report");
		break;
	case ICMP6_MEMBERSHIP_REDUCTION:
		(void)printf("Listener Done");
		break;
	case ND_ROUTER_SOLICIT:
		(void)printf("Router Solicitation");
		break;
	case ND_ROUTER_ADVERT:
		(void)printf("Router Advertisement");
		break;
	case ND_NEIGHBOR_SOLICIT:
		(void)printf("Neighbor Solicitation");
		break;
	case ND_NEIGHBOR_ADVERT:
		(void)printf("Neighbor Advertisement");
		break;
	case ND_REDIRECT:
		red = (struct nd_redirect *)icp;
		(void)printf("Redirect\n");
		if (!inet_ntop(AF_INET6, &red->nd_rd_dst, ntop_buf,
		    sizeof(ntop_buf)))
			strncpy(ntop_buf, "?", sizeof(ntop_buf));
		(void)printf("Destination: %s", ntop_buf);
		if (!inet_ntop(AF_INET6, &red->nd_rd_target, ntop_buf,
		    sizeof(ntop_buf)))
			strncpy(ntop_buf, "?", sizeof(ntop_buf));
		(void)printf(" New Target: %s", ntop_buf);
		break;
	default:
		(void)printf("Bad ICMP type: %d", icp->icmp6_type);
	}
}

/*
 * pr_iph --
 *	Print an IP6 header.
 */
void
pr_iph(struct ip6_hdr *ip6)
{
	u_int32_t flow = ip6->ip6_flow & IPV6_FLOWLABEL_MASK;
	u_int8_t tc;
	char ntop_buf[INET6_ADDRSTRLEN];

	tc = *(&ip6->ip6_vfc + 1); /* XXX */
	tc = (tc >> 4) & 0x0f;
	tc |= (ip6->ip6_vfc << 4);

	printf("Vr TC  Flow Plen Nxt Hlim\n");
	printf(" %1x %02x %05x %04x  %02x   %02x\n",
	    (ip6->ip6_vfc & IPV6_VERSION_MASK) >> 4, tc, (u_int32_t)ntohl(flow),
	    ntohs(ip6->ip6_plen), ip6->ip6_nxt, ip6->ip6_hlim);
	if (!inet_ntop(AF_INET6, &ip6->ip6_src, ntop_buf, sizeof(ntop_buf)))
		strncpy(ntop_buf, "?", sizeof(ntop_buf));
	printf("%s->", ntop_buf);
	if (!inet_ntop(AF_INET6, &ip6->ip6_dst, ntop_buf, sizeof(ntop_buf)))
		strncpy(ntop_buf, "?", sizeof(ntop_buf));
	printf("%s\n", ntop_buf);
}

/*
 * pr_retip --
 *	Dump some info on a returned (via ICMPv6) IPv6 packet.
 */
void
pr_retip(struct ip6_hdr *ip6, u_char *end)
{
	u_char *cp = (u_char *)ip6, nh;
	int hlen;

	if (end - (u_char *)ip6 < sizeof(*ip6)) {
		printf("IP6");
		goto trunc;
	}
	pr_iph(ip6);
	hlen = sizeof(*ip6);

	nh = ip6->ip6_nxt;
	cp += hlen;
	while (end - cp >= 8) {
		switch (nh) {
		case IPPROTO_HOPOPTS:
			printf("HBH ");
			hlen = (((struct ip6_hbh *)cp)->ip6h_len+1) << 3;
			nh = ((struct ip6_hbh *)cp)->ip6h_nxt;
			break;
		case IPPROTO_DSTOPTS:
			printf("DSTOPT ");
			hlen = (((struct ip6_dest *)cp)->ip6d_len+1) << 3;
			nh = ((struct ip6_dest *)cp)->ip6d_nxt;
			break;
		case IPPROTO_FRAGMENT:
			printf("FRAG ");
			hlen = sizeof(struct ip6_frag);
			nh = ((struct ip6_frag *)cp)->ip6f_nxt;
			break;
		case IPPROTO_ROUTING:
			printf("RTHDR ");
			hlen = (((struct ip6_rthdr *)cp)->ip6r_len+1) << 3;
			nh = ((struct ip6_rthdr *)cp)->ip6r_nxt;
			break;
		case IPPROTO_AH:
			printf("AH ");
			hlen = (((struct ah *)cp)->ah_hl+2) << 2;
			nh = ((struct ah *)cp)->ah_nh;
			break;
		case IPPROTO_ICMPV6:
			printf("ICMP6: type = %d, code = %d\n",
			    *cp, *(cp + 1));
			return;
		case IPPROTO_ESP:
			printf("ESP\n");
			return;
		case IPPROTO_TCP:
			printf("TCP: from port %u, to port %u (decimal)\n",
			    (*cp * 256 + *(cp + 1)),
			    (*(cp + 2) * 256 + *(cp + 3)));
			return;
		case IPPROTO_UDP:
			printf("UDP: from port %u, to port %u (decimal)\n",
			    (*cp * 256 + *(cp + 1)),
			    (*(cp + 2) * 256 + *(cp + 3)));
			return;
		default:
			printf("Unknown Header(%d)\n", nh);
			return;
		}

		if ((cp += hlen) >= end)
			goto trunc;
	}
	if (end - cp < 8)
		goto trunc;

	putchar('\n');
	return;

  trunc:
	printf("...\n");
	return;
}

__dead void
usage(void)
{
	(void)fprintf(stderr,
	    "usage: ping6 [-dEefHLmnqv] [-c count] [-h hoplimit] "
	    "[-I sourceaddr]\n\t[-i wait] [-l preload] [-p pattern] "
	    "[-s packetsize] [-V rtable]\n\t[-w maxwait] host\n");
	exit(1);
}
