/*	$OpenBSD: route.c,v 1.69 2004/06/12 09:40:49 claudio Exp $	*/
/*	$NetBSD: route.c,v 1.16 1996/04/15 18:27:05 cgd Exp $	*/

/*
 * Copyright (c) 1983, 1989, 1991, 1993
 *	The Regents of the University of California.  All rights reserved.
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

#ifndef lint
static const char copyright[] =
"@(#) Copyright (c) 1983, 1989, 1991, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
#if 0
static const char sccsid[] = "@(#)route.c	8.3 (Berkeley) 3/19/94";
#else
static const char rcsid[] = "$OpenBSD: route.c,v 1.69 2004/06/12 09:40:49 claudio Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mbuf.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netns/ns.h>
#include <netipx/ipx.h>
#include <netiso/iso.h>
#include <netccitt/x25.h>
#include <arpa/inet.h>
#include <netdb.h>

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <ctype.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <err.h>

#include "keywords.h"
#include "show.h"

union	sockunion {
	struct	sockaddr sa;
	struct	sockaddr_in sin;
#ifdef INET6
	struct	sockaddr_in6 sin6;
#endif
	struct	sockaddr_ns sns;
	struct	sockaddr_ipx sipx;
	struct	sockaddr_iso siso;
	struct	sockaddr_dl sdl;
	struct	sockaddr_x25 sx25;
	struct	sockaddr_rtin rtin;
} so_dst, so_gate, so_mask, so_genmask, so_ifa, so_ifp, so_src, so_srcmask;

typedef union sockunion *sup;
pid_t	pid;
int	rtm_addrs, s;
int	forcehost, forcenet, doflush, nflag, af, qflag, tflag, keyword(char *);
int	Sflag, iflag, verbose, aflen = sizeof (struct sockaddr_in);
int	locking, lockrest, debugonly;
struct	rt_metrics rt_metrics;
u_long  rtm_inits;
uid_t	uid;

void	 flushroutes(int, char **);
int	 newroute(int, char **);
void	 show(int, char *[]);
void	 monitor(void);
int	 prefixlen(char *);
void	 sockaddr(char *, struct sockaddr *);
void	 sodump(sup, char *);
void	 print_getmsg(struct rt_msghdr *, int);
void	 print_rtmsg(struct rt_msghdr *, int);
void	 pmsg_common(struct rt_msghdr *);
void	 pmsg_addrs(char *, int);
void	 bprintf(FILE *, int, u_char *);
void	 mask_addr(union sockunion *, union sockunion *, int which);
#ifdef INET6
static int inet6_makenetandmask(struct sockaddr_in6 *);
#endif
int	 getaddr(int, char *, struct hostent **);
int	 rtmsg(int, int);
int	 x25_makemask(void);
__dead void usage(char *);
void	quit(char *);
void	set_metric(char *, int);
void	inet_makenetandmask(u_int32_t, struct sockaddr_in *, int, int);
void	interfaces(void);

__dead void
usage(char *cp)
{
	if (cp)
		(void) fprintf(stderr, "route: botched keyword: %s\n", cp);
	(void) fprintf(stderr,
	    "usage: route [ -nqSv ] cmd [[ -<modifiers> ] args ]\n");
	(void) fprintf(stderr,
	    "keywords: get, add, change, delete, show, flush, monitor.\n");
	exit(1);
	/* NOTREACHED */
}

void
quit(char *s)
{
	int sverrno = errno;

	(void) fprintf(stderr, "route: ");
	if (s)
		(void) fprintf(stderr, "%s: ", s);
	(void) fprintf(stderr, "%s\n", strerror(sverrno));
	exit(1);
	/* NOTREACHED */
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

int
main(int argc, char **argv)
{
	int ch;
	int rval = 0;

	if (argc < 2)
		usage(NULL);

	while ((ch = getopt(argc, argv, "nqdtvS")) != -1)
		switch(ch) {
		case 'n':
			nflag = 1;
			break;
		case 'q':
			qflag = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		case 't':
			tflag = 1;
			break;
		case 'd':
			debugonly = 1;
			break;
		case 'S':
			Sflag = 1;
			break;
		default:
			usage(NULL);
		}
	argc -= optind;
	argv += optind;

	pid = getpid();
	uid = geteuid();
	if (tflag)
		s = open(_PATH_DEVNULL, O_WRONLY);
	else
		s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		quit("socket");
	if (*argv == NULL)
		goto no_cmd;
	switch (keyword(*argv)) {
	case K_GET:
		uid = 0;
		/* FALLTHROUGH */
	case K_CHANGE:
	case K_ADD:
	case K_DELETE:
		rval = newroute(argc, argv);
		break;
	case K_SHOW:
		uid = 0;
		show(argc, argv);
		break;
	case K_MONITOR:
		monitor();
		break;
	case K_FLUSH:
		flushroutes(argc, argv);
		break;
	no_cmd:
	default:
		usage(*argv);
	}
	exit(rval);
}

/*
 * Purge all entries in the routing tables not
 * associated with network interfaces.
 */
void
flushroutes(int argc, char **argv)
{
	size_t needed;
	int mib[6], rlen, seqno;
	char *buf = NULL, *next, *lim = NULL;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;

	if (uid) {
		errno = EACCES;
		quit("must be root to alter routing table");
	}
	shutdown(s, 0); /* Don't want to read back our messages */
	if (argc > 1) {
		argv++;
		if (argc == 2 && **argv == '-')
		    switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
#ifdef INET6
			case K_INET6:
				af = AF_INET6;
				break;
#endif
			case K_XNS:
				af = AF_NS;
				break;
			case K_IPX:
				af = AF_IPX;
				break;
			case K_LINK:
				af = AF_LINK;
				break;
			case K_ISO:
			case K_OSI:
				af = AF_ISO;
				break;
			case K_X25:
				af = AF_CCITT;
				break;
			default:
				goto bad;
		} else
bad:			usage(*argv);
	}
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		quit("route-sysctl-estimate");
	if (needed) {
		if ((buf = malloc(needed)) == NULL)
			quit("malloc");
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
			quit("actual retrieval of routing table");
		lim = buf + needed;
	}
	if (verbose) {
		(void) printf("Examining routing table from sysctl\n");
		 if (af)
			printf("(address family %s)\n", (*argv + 1));
	}
	if (buf == NULL)
		return;

	seqno = 0;		/* ??? */
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (verbose)
			print_rtmsg(rtm, rtm->rtm_msglen);
		if ((rtm->rtm_flags & (RTF_GATEWAY|RTF_STATIC|RTF_LLINFO)) == 0)
			continue;
		sa = (struct sockaddr *)(rtm + 1);
		if (af) {
			if (sa->sa_family != af)
				continue;
		}
		if (sa->sa_family == AF_KEY)
			continue;  /* Don't flush SPD */
		if (debugonly)
			continue;
		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno;
		rlen = write(s, next, rtm->rtm_msglen);
		if (rlen < (int)rtm->rtm_msglen) {
			(void) fprintf(stderr,
			    "route: write to routing socket: %s\n",
			    strerror(errno));
			(void) printf("got only %d for rlen\n", rlen);
			break;
		}
		seqno++;
		if (qflag)
			continue;
		if (verbose)
			print_rtmsg(rtm, rlen);
		else {
			struct sockaddr *sa = (struct sockaddr *)(rtm + 1);
			(void) printf("%-20.20s ", rtm->rtm_flags & RTF_HOST ?
			    routename(sa) : netname(sa, NULL)); /* XXX extract
								   netmask */
			sa = (struct sockaddr *)(ROUNDUP(sa->sa_len) + (char *)sa);
			(void) printf("%-20.20s ", routename(sa));
			(void) printf("done\n");
		}
	}
	free(buf);
}

void
set_metric(char *value, int key)
{
	int flag = 0;
	u_long noval, *valp = &noval;

	switch (key) {
#define caseof(x, y, z)	case x: valp = &rt_metrics.z; flag = y; break
	caseof(K_MTU, RTV_MTU, rmx_mtu);
	caseof(K_HOPCOUNT, RTV_HOPCOUNT, rmx_hopcount);
	caseof(K_EXPIRE, RTV_EXPIRE, rmx_expire);
	caseof(K_RECVPIPE, RTV_RPIPE, rmx_recvpipe);
	caseof(K_SENDPIPE, RTV_SPIPE, rmx_sendpipe);
	caseof(K_SSTHRESH, RTV_SSTHRESH, rmx_ssthresh);
	caseof(K_RTT, RTV_RTT, rmx_rtt);
	caseof(K_RTTVAR, RTV_RTTVAR, rmx_rttvar);
	}
	rtm_inits |= flag;
	if (lockrest || locking)
		rt_metrics.rmx_locks |= flag;
	if (locking)
		locking = 0;
	*valp = atoi(value);
}

int
newroute(int argc, char **argv)
{
	char *cmd, *dest = "", *source = "", *gateway = "", *err;
	int ishost = 0, ret = 0, attempts, oerrno, flags = RTF_STATIC;
	int key;
	struct hostent *hp = 0;

	if (uid) {
		errno = EACCES;
		quit("must be root to alter routing table");
	}
	cmd = argv[0];
	if (*cmd != 'g')
		shutdown(s, 0); /* Don't want to read back our messages */
	while (--argc > 0) {
		if (**(++argv)== '-') {
			switch (key = keyword(1 + *argv)) {
			case K_LINK:
				af = AF_LINK;
				aflen = sizeof(struct sockaddr_dl);
				break;
			case K_OSI:
			case K_ISO:
				af = AF_ISO;
				aflen = sizeof(struct sockaddr_iso);
				break;
			case K_INET:
				af = AF_INET;
				aflen = sizeof(struct sockaddr_in);
				break;
#ifdef INET6
			case K_INET6:
				af = AF_INET6;
				aflen = sizeof(struct sockaddr_in6);
				break;
#endif
			case K_X25:
				af = AF_CCITT;
				aflen = sizeof(struct sockaddr_x25);
				break;
			case K_SA:
				af = PF_ROUTE;
				aflen = sizeof(union sockunion);
				break;
			case K_XNS:
				af = AF_NS;
				aflen = sizeof(struct sockaddr_ns);
				break;
			case K_IPX:
				af = AF_IPX;
				aflen = sizeof(struct sockaddr_ipx);
				break;
			case K_IFACE:
			case K_INTERFACE:
				iflag++;
				break;
			case K_NOSTATIC:
				flags &= ~RTF_STATIC;
				break;
			case K_LLINFO:
				flags |= RTF_LLINFO;
				break;
			case K_LOCK:
				locking = 1;
				break;
			case K_LOCKREST:
				lockrest = 1;
				break;
			case K_HOST:
				forcehost++;
				break;
			case K_REJECT:
				flags |= RTF_REJECT;
				break;
			case K_BLACKHOLE:
				flags |= RTF_BLACKHOLE;
				break;
			case K_PROTO1:
				flags |= RTF_PROTO1;
				break;
			case K_PROTO2:
				flags |= RTF_PROTO2;
				break;
			case K_CLONING:
				flags |= RTF_CLONING;
				break;
			case K_XRESOLVE:
				flags |= RTF_XRESOLVE;
				break;
			case K_STATIC:
				flags |= RTF_STATIC;
				break;
			case K_IFA:
				if (!--argc)
					usage(1+*argv);
				(void) getaddr(RTA_IFA, *++argv, 0);
				break;
			case K_IFP:
				if (!--argc)
					usage(1+*argv);
				(void) getaddr(RTA_IFP, *++argv, 0);
				break;
			case K_GENMASK:
				if (!--argc)
					usage(1+*argv);
				(void) getaddr(RTA_GENMASK, *++argv, 0);
				break;
			case K_GATEWAY:
				if (!--argc)
					usage(1+*argv);
				(void) getaddr(RTA_GATEWAY, *++argv, 0);
				gateway = *argv;
				break;
			case K_DST:
				if (!--argc)
					usage(1+*argv);
				ishost = getaddr(RTA_DST, *++argv, &hp);
				dest = *argv;
				break;
			case K_SRC:
				if (!--argc)
					usage(1+*argv);
				(void) getaddr(RTA_SRC, *++argv, 0);
				source = *argv;
				break;
			case K_SRCMASK:
				if (!--argc)
					usage(1+*argv);
				(void) getaddr(RTA_SRCMASK, *++argv, 0);
				break;
			case K_NETMASK:
				if (!--argc)
					usage(1+*argv);
				(void) getaddr(RTA_NETMASK, *++argv, 0);
				/* FALLTHROUGH */
			case K_NET:
				forcenet++;
				break;
			case K_PREFIXLEN:
				if (!--argc)
					usage(1+*argv);
				ishost = prefixlen(*++argv);
				break;
			case K_MTU:
			case K_HOPCOUNT:
			case K_EXPIRE:
			case K_RECVPIPE:
			case K_SENDPIPE:
			case K_SSTHRESH:
			case K_RTT:
			case K_RTTVAR:
				if (!--argc)
					usage(1+*argv);
				set_metric(*++argv, key);
				break;
			default:
				usage(1+*argv);
			}
		} else {
			if ((rtm_addrs & (RTA_DST|RTA_SRC)) == 0) {
				dest = *argv;
				ishost = getaddr(RTA_DST, *argv, &hp);
			} else if ((rtm_addrs & RTA_GATEWAY) == 0) {
				gateway = *argv;
				(void) getaddr(RTA_GATEWAY, *argv, &hp);
			} else {
				int hops = atoi(*argv);

				if (hops == 0) {
				    if (!qflag && strcmp(*argv, "0") == 0)
					printf("%s,%s",
					    "old usage of trailing 0",
					    "assuming route to if\n");
				    else
					usage(NULL);
				    iflag = 1;
				    continue;
				} else if (hops > 0 && hops < 10) {
				    if (!qflag) {
					printf("old usage of trailing digit, ");
					printf("assuming route via gateway\n");
				    }
				    iflag = 0;
				    continue;
				}
				(void) getaddr(RTA_NETMASK, *argv, 0);
			}
		}
	}
	if (forcehost)
		ishost = 1;
	if (forcenet)
		ishost = 0;
	flags |= RTF_UP;
	if (ishost)
		flags |= RTF_HOST;
	if (iflag == 0)
		flags |= RTF_GATEWAY;
	if (rtm_addrs & (RTA_SRC|RTA_SRCMASK)) {
		if (!(rtm_addrs & RTA_DST))
			getaddr(RTA_DST, "default", &hp);
		flags |= RTF_SOURCE;
	}
	for (attempts = 1; ; attempts++) {
		errno = 0;
		if ((ret = rtmsg(*cmd, flags)) == 0)
			break;
		if (errno != ENETUNREACH && errno != ESRCH)
			break;
		if (af == AF_INET && *gateway && hp && hp->h_addr_list[1]) {
			hp->h_addr_list++;
			memcpy(&so_gate.sin.sin_addr, hp->h_addr_list[0],
			    hp->h_length);
		} else
			break;
	}
	if (*cmd == 'g')
		exit(0);
	oerrno = errno;
	if (!qflag) {
		(void) printf("%s %s %s", cmd, ishost? "host" : "net", dest);
		if (*source)
			(void) printf(": source %s", source);
		if (*gateway) {
			(void) printf(": gateway %s", gateway);
			if (attempts > 1 && ret == 0 && af == AF_INET)
			    (void) printf(" (%s)",
				inet_ntoa(so_gate.sin.sin_addr));
		}
		if (ret == 0)
			(void) printf("\n");
		if (ret != 0) {
			switch (oerrno) {
			case ESRCH:
				err = "not in table";
				break;
			case EBUSY:
				err = "entry in use";
				break;
			case ENOBUFS:
				err = "routing table overflow";
				break;
			default:
				err = strerror(oerrno);
				break;
			}
			(void) printf(": %s\n", err);
		}
	}
	return (ret != 0);
}

void
show(int argc, char *argv[])
{
	int	af = 0;

        if (argc > 1) {
                argv++;
                if (argc == 2 && **argv == '-')
                    switch (keyword(*argv + 1)) {
                        case K_INET:
                                af = AF_INET;
                                break;
#ifdef INET6
                        case K_INET6:
                                af = AF_INET6;
                                break;
#endif
                        case K_XNS:
                                af = AF_NS;
                                break;
                        case K_IPX:
                                af = AF_IPX;
                                break;
                        case K_LINK:
                                af = AF_LINK;
                                break;
                        case K_ISO:
                        case K_OSI:
                                af = AF_ISO;
                                break;
                        case K_X25:
                                af = AF_CCITT;
                                break;
                        default:
                                goto bad;
                } else
bad:                    usage(*argv);
        }
	
	p_rttables(af, 0, Sflag);
}

void
inet_makenetandmask(u_int32_t net, struct sockaddr_in *sin, int bits, int which)
{
	u_int32_t addr, mask = 0;
	char *cp;

	rtm_addrs |= (which == RTA_DST) ? RTA_NETMASK : RTA_SRCMASK;
	if (net == 0)
		mask = addr = 0;
	else if (bits) {
		addr = net;
		mask = 0xffffffff << (32 - bits);
	} else if (net < 128) {
		addr = net << IN_CLASSA_NSHIFT;
		mask = IN_CLASSA_NET;
	} else if (net < 65536) {
		addr = net << IN_CLASSB_NSHIFT;
		mask = IN_CLASSB_NET;
	} else if (net < 16777216L) {
		addr = net << IN_CLASSC_NSHIFT;
		mask = IN_CLASSC_NET;
	} else {
		addr = net;
		if ((addr & IN_CLASSA_HOST) == 0)
			mask =  IN_CLASSA_NET;
		else if ((addr & IN_CLASSB_HOST) == 0)
			mask =  IN_CLASSB_NET;
		else if ((addr & IN_CLASSC_HOST) == 0)
			mask =  IN_CLASSC_NET;
		else
			mask = -1;
	}
	addr &= mask;
	sin->sin_addr.s_addr = htonl(addr);
	sin = (which == RTA_DST) ? &so_mask.sin : &so_srcmask.sin;
	sin->sin_addr.s_addr = htonl(mask);
	sin->sin_len = 0;
	sin->sin_family = 0;
	cp = (char *)(&sin->sin_addr + 1);
	while (*--cp == 0 && cp > (char *)sin)
		;
	sin->sin_len = 1 + cp - (char *)sin;
}

#ifdef INET6
/*
 * XXX the function may need more improvement...
 */
static int
inet6_makenetandmask(struct sockaddr_in6 *sin6)
{
	char *plen = NULL;
	struct in6_addr in6;

	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr) &&
	    sin6->sin6_scope_id == 0) {
		plen = "0";
	} else if ((sin6->sin6_addr.s6_addr[0] & 0xe0) == 0x20) {
		/* aggregatable global unicast - RFC2374 */
		memset(&in6, 0, sizeof(in6));
		if (!memcmp(&sin6->sin6_addr.s6_addr[8], &in6.s6_addr[8], 8))
			plen = "64";
	}

	if (!plen || strcmp(plen, "128") == 0)
		return 1;
	else {
		rtm_addrs |= RTA_NETMASK;
		(void)prefixlen(plen);
		return 0;
	}
}
#endif

/*
 * Interpret an argument as a network address of some kind,
 * returning 1 if a host address, 0 if a network address.
 */
int
getaddr(int which, char *s, struct hostent **hpp)
{
	sup su = NULL;
	struct ccitt_addr *ccitt_addr(char *, struct sockaddr_x25 *);
	struct hostent *hp;
	struct netent *np;
	int afamily, bits;

	if (af == 0) {
		af = AF_INET;
		aflen = sizeof(struct sockaddr_in);
	}
	afamily = af;	/* local copy of af so we can change it */

	rtm_addrs |= which;
	switch (which) {
	case RTA_DST:
		su = &so_dst;
		break;
	case RTA_GATEWAY:
		su = &so_gate;
		break;
	case RTA_NETMASK:
		su = &so_mask;
		break;
	case RTA_GENMASK:
		su = &so_genmask;
		break;
	case RTA_IFP:
		su = &so_ifp;
		afamily = AF_LINK;
		break;
	case RTA_IFA:
		su = &so_ifa;
		break;
	case RTA_SRC:
		su = &so_src;
		break;
	case RTA_SRCMASK:
		su = &so_srcmask;
		break;
	default:
		errx(1, "internal error");
	}
	su->sa.sa_len = aflen;
	su->sa.sa_family = afamily;

	if (strcmp(s, "default") == 0) {
		switch (which) {
		case RTA_DST:
			forcenet++;
			getaddr(RTA_NETMASK, s, 0);
			break;
		case RTA_NETMASK:
		case RTA_GENMASK:
			su->sa.sa_len = 0;
		}
		return (0);
	}

	switch (afamily) {
#ifdef INET6
	case AF_INET6:
	    {
		struct addrinfo hints, *res;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = afamily;	/*AF_INET6*/
		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
		if (getaddrinfo(s, "0", &hints, &res) != 0) {
			hints.ai_flags = 0;
			if (getaddrinfo(s, "0", &hints, &res) != 0) {
				(void) fprintf(stderr, "%s: bad value\n", s);
				exit(1);
			}
		}
		if (sizeof(su->sin6) != res->ai_addrlen) {
			(void) fprintf(stderr, "%s: bad value\n", s);
			exit(1);
		}
		if (res->ai_next) {
			(void) fprintf(stderr,
			    "%s: resolved to multiple values\n", s);
			exit(1);
		}
		memcpy(&su->sin6, res->ai_addr, sizeof(su->sin6));
		freeaddrinfo(res);
#ifdef __KAME__
		if ((IN6_IS_ADDR_LINKLOCAL(&su->sin6.sin6_addr) ||
		     IN6_IS_ADDR_MC_LINKLOCAL(&su->sin6.sin6_addr)) &&
		    su->sin6.sin6_scope_id) {
			*(u_int16_t *)&su->sin6.sin6_addr.s6_addr[2] =
				htons(su->sin6.sin6_scope_id);
			su->sin6.sin6_scope_id = 0;
		}
#endif
		if (hints.ai_flags == AI_NUMERICHOST) {
			if (which == RTA_DST)
				return (inet6_makenetandmask(&su->sin6));
			return (0);
		} else
			return (1);
	    }
#endif

	case AF_NS:
		if (which == RTA_DST) {
			extern short ns_bh[3];
			struct sockaddr_ns *sms = &(so_mask.sns);
			memset(sms, 0, sizeof(*sms));
			sms->sns_family = 0;
			sms->sns_len = 6;
			sms->sns_addr.x_net = *(union ns_net *)ns_bh;
			rtm_addrs |= RTA_NETMASK;
		}
		su->sns.sns_addr = ns_addr(s);
		return (!ns_nullhost(su->sns.sns_addr));

	case AF_IPX:
		if (which == RTA_DST) {
			extern short ipx_bh[3];
			struct sockaddr_ipx *sms = &(so_mask.sipx);
			memset(sms, 0, sizeof(*sms));
			sms->sipx_family = 0;
			sms->sipx_len = 6;
			sms->sipx_addr.ipx_net = *(union ipx_net *)ipx_bh;
			rtm_addrs |= RTA_NETMASK;
		}
		su->sipx.sipx_addr = ipx_addr(s);
		return (!ipx_nullhost(su->sipx.sipx_addr));

	case AF_OSI:
		su->siso.siso_addr = *iso_addr(s);
		if (which == RTA_NETMASK || which == RTA_GENMASK) {
			char *cp = (char *)TSEL(&su->siso);
			su->siso.siso_nlen = 0;
			do {
				--cp;
			} while ((cp > (char *)su) && (*cp == 0));
			su->siso.siso_len = 1 + cp - (char *)su;
		}
		return (1);

	case AF_LINK:
		link_addr(s, &su->sdl);
		return (1);

	case AF_CCITT:
		ccitt_addr(s, &su->sx25);
		return (which == RTA_DST ? x25_makemask() : 1);

	case PF_ROUTE:
		su->sa.sa_len = sizeof(*su);
		sockaddr(s, &su->sa);
		return (1);

	case AF_INET:
		if (hpp != NULL)
			*hpp = NULL;
		if ((which == RTA_DST || which == RTA_SRC) && !forcehost) {
			bits = inet_net_pton(AF_INET, s, &su->sin.sin_addr,
			    sizeof(su->sin.sin_addr));
			if (bits == 32) {
				if (forcenet)
					errx(1, "%s: not a network", s);
				return (1);
			}
			if (bits >= 0) {
				inet_makenetandmask(ntohl(
				    su->sin.sin_addr.s_addr),
				    &su->sin, bits, which);
				return (0);
			}
			np = getnetbyname(s);
			if (np != NULL && np->n_net != 0) {
				inet_makenetandmask(np->n_net, &su->sin, 0,
				    which);
				return (0);
			}
			if (forcenet)
				errx(1, "%s: not a network", s);
		}
		if (inet_pton(AF_INET, s, &su->sin.sin_addr) == 1)
			return (1);
		hp = gethostbyname(s);
		if (hp != NULL) {
			if (hpp != NULL)
				*hpp = hp;
			su->sin.sin_addr = *(struct in_addr *)hp->h_addr;
			return (1);
		}
		errx(1, "%s: bad address", s);

	default:
		errx(1, "%d: bad address family", afamily);
	}
}

int
prefixlen(char *s)
{
	int len = atoi(s), q, r;
	int max;

	switch (af) {
	case AF_INET:
		max = sizeof(struct in_addr) * 8;
		break;
#ifdef INET6
	case AF_INET6:
		max = sizeof(struct in6_addr) * 8;
		break;
#endif
	default:
		(void) fprintf(stderr,
		    "prefixlen is not supported with af %d\n", af);
		exit(1);
	}

	rtm_addrs |= RTA_NETMASK;
	if (len < -1 || len > max) {
		(void) fprintf(stderr, "%s: bad value\n", s);
		exit(1);
	}

	q = len >> 3;
	r = len & 7;
	switch (af) {
	case AF_INET:
		memset(&so_mask, 0, sizeof(so_mask));
		so_mask.sin.sin_family = AF_INET;
		so_mask.sin.sin_len = sizeof(struct sockaddr_in);
		so_mask.sin.sin_addr.s_addr = htonl(0xffffffff << (32 - len));
		break;
#ifdef INET6
	case AF_INET6:
		so_mask.sin6.sin6_family = AF_INET6;
		so_mask.sin6.sin6_len = sizeof(struct sockaddr_in6);
		memset((void *)&so_mask.sin6.sin6_addr, 0,
			sizeof(so_mask.sin6.sin6_addr));
		if (q > 0)
			memset((void *)&so_mask.sin6.sin6_addr, 0xff, q);
		if (r > 0)
			*((u_char *)&so_mask.sin6.sin6_addr + q) =
			    (0xff00 >> r) & 0xff;
		break;
#endif
	}
	return (len == max);
}

int
x25_makemask(void)
{
	char *cp;

	if ((rtm_addrs & RTA_NETMASK) == 0) {
		rtm_addrs |= RTA_NETMASK;
		for (cp = (char *)&so_mask.sx25.x25_net;
		     cp < &so_mask.sx25.x25_opts.op_flags; cp++)
			*cp = -1;
		so_mask.sx25.x25_len = (u_char)&(((sup)0)->sx25.x25_opts);
	}
	return (0);
}

void
interfaces(void)
{
	size_t needed;
	int mib[6];
	char *buf = NULL, *lim, *next;
	struct rt_msghdr *rtm;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;		/* no flags */
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		quit("route-sysctl-estimate");
	if (needed) {
		if ((buf = malloc(needed)) == NULL)
			quit("malloc");
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
			quit("actual retrieval of interface table");
		lim = buf + needed;
		for (next = buf; next < lim; next += rtm->rtm_msglen) {
			rtm = (struct rt_msghdr *)next;
			print_rtmsg(rtm, rtm->rtm_msglen);
		}
		free(buf);
	}
}

void
monitor(void)
{
	int n;
	char msg[2048];

	verbose = 1;
	if (debugonly) {
		interfaces();
		exit(0);
	}
	for(;;) {
		time_t now;
		n = read(s, msg, 2048);
		now = time(NULL);
		(void) printf("got message of size %d on %s", n, ctime(&now));
		print_rtmsg((struct rt_msghdr *)msg, n);
	}
}

struct {
	struct	rt_msghdr m_rtm;
	char	m_space[512];
} m_rtmsg;

int
rtmsg(int cmd, int flags)
{
	static int seq;
	int rlen;
	char *cp = m_rtmsg.m_space;
	int l;

#define NEXTADDR(w, u) \
	if (rtm_addrs & (w)) {\
	    l = ROUNDUP(u.sa.sa_len); memcpy(cp, &(u), l); cp += l;\
	    if (verbose) sodump(&(u),#u);\
	}

	errno = 0;
	memset(&m_rtmsg, 0, sizeof(m_rtmsg));
	if (cmd == 'a')
		cmd = RTM_ADD;
	else if (cmd == 'c')
		cmd = RTM_CHANGE;
	else if (cmd == 'g') {
		cmd = RTM_GET;
		if (so_ifp.sa.sa_family == 0) {
			so_ifp.sa.sa_family = AF_LINK;
			so_ifp.sa.sa_len = sizeof(struct sockaddr_dl);
			rtm_addrs |= RTA_IFP;
		}
	} else
		cmd = RTM_DELETE;
#define rtm m_rtmsg.m_rtm
	rtm.rtm_type = cmd;
	rtm.rtm_flags = flags;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++seq;
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;

	if (rtm_addrs & RTA_NETMASK)
		mask_addr(&so_dst, &so_mask, RTA_DST);
	if (rtm_addrs & RTA_SRCMASK)
		mask_addr(&so_src, &so_srcmask, RTA_SRC);
	NEXTADDR(RTA_DST, so_dst);
	NEXTADDR(RTA_GATEWAY, so_gate);
	NEXTADDR(RTA_NETMASK, so_mask);
	NEXTADDR(RTA_GENMASK, so_genmask);
	NEXTADDR(RTA_IFP, so_ifp);
	NEXTADDR(RTA_IFA, so_ifa);
	NEXTADDR(RTA_SRC, so_src);
	NEXTADDR(RTA_SRCMASK, so_srcmask);
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;
	if (verbose)
		print_rtmsg(&rtm, l);
	if (debugonly)
		return (0);
	if ((rlen = write(s, (char *)&m_rtmsg, l)) < 0) {
		if (qflag == 0)
			perror("writing to routing socket");
		return (-1);
	}
	if (cmd == RTM_GET) {
		do {
			l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
		} while (l > 0 && (rtm.rtm_seq != seq || rtm.rtm_pid != pid));
		if (l < 0)
			(void) fprintf(stderr,
			    "route: read from routing socket: %s\n",
			    strerror(errno));
		else
			print_getmsg(&rtm, l);
	}
#undef rtm
	return (0);
}

void
mask_addr(union sockunion *addr, union sockunion *mask, int which)
{
	int olen = mask->sa.sa_len;
	char *cp1 = olen + (char *)mask, *cp2;

	for (mask->sa.sa_len = 0; cp1 > (char *)mask; )
		if (*--cp1 != 0) {
			mask->sa.sa_len = 1 + cp1 - (char *)mask;
			break;
		}
	if ((rtm_addrs & which) == 0)
		return;
	switch (addr->sa.sa_family) {
	case AF_NS:
	case AF_IPX:
	case AF_INET:
#ifdef INET6
	case AF_INET6:
#endif
	case AF_CCITT:
	case 0:
		return;
	case AF_ISO:
		olen = MIN(addr->siso.siso_nlen,
			   MAX(mask->sa.sa_len - 6, 0));
		break;
	}
	cp1 = mask->sa.sa_len + 1 + (char *)addr;
	cp2 = addr->sa.sa_len + 1 + (char *)addr;
	while (cp2 > cp1)
		*--cp2 = 0;
	cp2 = mask->sa.sa_len + 1 + (char *)mask;
	while (cp1 > addr->sa.sa_data)
		*--cp1 &= *--cp2;
	switch (addr->sa.sa_family) {
	case AF_ISO:
		addr->siso.siso_nlen = olen;
		break;
	}
}

char *msgtypes[] = {
	"",
	"RTM_ADD: Add Route",
	"RTM_DELETE: Delete Route",
	"RTM_CHANGE: Change Metrics or flags",
	"RTM_GET: Report Metrics",
	"RTM_LOSING: Kernel Suspects Partitioning",
	"RTM_REDIRECT: Told to use different route",
	"RTM_MISS: Lookup failed on this address",
	"RTM_LOCK: fix specified metrics",
	"RTM_OLDADD: caused by SIOCADDRT",
	"RTM_OLDDEL: caused by SIOCDELRT",
	"RTM_RESOLVE: Route created by cloning",
	"RTM_NEWADDR: address being added to iface",
	"RTM_DELADDR: address being removed from iface",
	"RTM_IFINFO: iface status change",
	"RTM_IFANNOUNCE: iface arrival/departure",
	0,
};

char metricnames[] =
"\011pksent\010rttvar\7rtt\6ssthresh\5sendpipe\4recvpipe\3expire\2hopcount\1mtu";
char routeflags[] =
"\1UP\2GATEWAY\3HOST\4REJECT\5DYNAMIC\6MODIFIED\7DONE\010MASK_PRESENT\011CLONING\012XRESOLVE\013LLINFO\014STATIC\015BLACKHOLE\016PROTO3\017PROTO2\020PROTO1\021CLONED\022SOURCE";
char ifnetflags[] =
"\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5PTP\6NOTRAILERS\7RUNNING\010NOARP\011PPROMISC\012ALLMULTI\013OACTIVE\014SIMPLEX\015LINK0\016LINK1\017LINK2\020MULTICAST";
char addrnames[] =
"\1DST\2GATEWAY\3NETMASK\4GENMASK\5IFP\6IFA\7AUTHOR\010BRD\011SRC\012SRCMASK";

void
print_rtmsg(struct rt_msghdr *rtm, int msglen)
{
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct if_announcemsghdr *ifan;
	const char *state = "unknown";

	if (verbose == 0)
		return;
	if (rtm->rtm_version != RTM_VERSION) {
		(void) printf("routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	(void)printf("%s: len %d, ", msgtypes[rtm->rtm_type], rtm->rtm_msglen);
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		(void) printf("if# %d, ", ifm->ifm_index);
		switch (ifm->ifm_data.ifi_link_state) {
		case LINK_STATE_DOWN:
			state = "down";
			break;
		case LINK_STATE_UP:
			state = "up";
			break;
		}
		(void) printf("link: %s, flags:", state);
		bprintf(stdout, ifm->ifm_flags, ifnetflags);
		pmsg_addrs((char *)(ifm + 1), ifm->ifm_addrs);
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		ifam = (struct ifa_msghdr *)rtm;
		(void) printf("metric %d, flags:", ifam->ifam_metric);
		bprintf(stdout, ifam->ifam_flags, routeflags);
		pmsg_addrs((char *)(ifam + 1), ifam->ifam_addrs);
		break;
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		(void) printf("if# %d, name %s, what: ",
		    ifan->ifan_index, ifan->ifan_name);
		switch (ifan->ifan_what) {
		case IFAN_ARRIVAL:
			printf("arrival");
			break;
		case IFAN_DEPARTURE:
			printf("departure");
			break;
		default:
			printf("#%d", ifan->ifan_what);
			break;
		}
		printf("\n");
		break;
	default:
		(void) printf("pid: %ld, seq %d, errno %d, flags:",
			(long)rtm->rtm_pid, rtm->rtm_seq, rtm->rtm_errno);
		bprintf(stdout, rtm->rtm_flags, routeflags);
		pmsg_common(rtm);
	}
}

void
print_getmsg(struct rt_msghdr *rtm, int msglen)
{
	struct sockaddr *dst = NULL, *gate = NULL, *mask = NULL;
	struct sockaddr *src = NULL, *srcmask = NULL;
	struct sockaddr_dl *ifp = NULL;
	struct sockaddr *sa;
	char *cp;
	int i;

	(void) printf("   route to: %s\n", routename(&so_dst.sa));
	if (rtm->rtm_version != RTM_VERSION) {
		(void)fprintf(stderr,
		    "routing message version %d not understood\n",
		    rtm->rtm_version);
		return;
	}
	if (rtm->rtm_msglen > msglen) {
		(void)fprintf(stderr,
		    "message length mismatch, in packet %d, returned %d\n",
		    rtm->rtm_msglen, msglen);
	}
	if (rtm->rtm_errno)  {
		(void) fprintf(stderr, "RTM_GET: %s (errno %d)\n",
		    strerror(rtm->rtm_errno), rtm->rtm_errno);
		return;
	}
	cp = ((char *)(rtm + 1));
	if (rtm->rtm_addrs)
		for (i = 1; i; i <<= 1)
			if (i & rtm->rtm_addrs) {
				sa = (struct sockaddr *)cp;
				switch (i) {
				case RTA_DST:
					dst = sa;
					break;
				case RTA_GATEWAY:
					gate = sa;
					break;
				case RTA_NETMASK:
					mask = sa;
					break;
				case RTA_SRC:
					src = sa;
					break;
				case RTA_SRCMASK:
					srcmask = sa;
					break;
				case RTA_IFP:
					if (sa->sa_family == AF_LINK &&
					   ((struct sockaddr_dl *)sa)->sdl_nlen)
						ifp = (struct sockaddr_dl *)sa;
					break;
				}
				ADVANCE(cp, sa);
			}
	if (dst && mask)
		mask->sa_family = dst->sa_family;	/* XXX */
	if (dst)
		(void)printf("destination: %s\n", routename(dst));
	if (mask) {
		int savenflag = nflag;

		nflag = 1;
		(void)printf("       mask: %s\n", routename(mask));
		nflag = savenflag;
	}
	if (src && srcmask)
		srcmask->sa_family = src->sa_family;	/* XXX */
	if (src)
		(void)printf("     source: %s\n", routename(src));
	if (srcmask) {
		int savenflag = nflag;

		nflag = 1;
		(void)printf("       mask: %s\n", routename(mask));
		nflag = savenflag;
	}
	if (gate && rtm->rtm_flags & RTF_GATEWAY)
		(void)printf("    gateway: %s\n", routename(gate));
	if (ifp)
		(void)printf("  interface: %.*s\n",
		    ifp->sdl_nlen, ifp->sdl_data);
	(void)printf("      flags: ");
	bprintf(stdout, rtm->rtm_flags, routeflags);

#define lock(f)	((rtm->rtm_rmx.rmx_locks & __CONCAT(RTV_,f)) ? 'L' : ' ')
#define msec(u)	(((u) + 500) / 1000)		/* usec to msec */

	(void) printf("\n%s\n", "\
 recvpipe  sendpipe  ssthresh  rtt,msec    rttvar  hopcount      mtu     expire");
	printf("%8d%c ", (int)rtm->rtm_rmx.rmx_recvpipe, lock(RPIPE));
	printf("%8d%c ", (int)rtm->rtm_rmx.rmx_sendpipe, lock(SPIPE));
	printf("%8d%c ", (int)rtm->rtm_rmx.rmx_ssthresh, lock(SSTHRESH));
	printf("%8d%c ", (int)msec(rtm->rtm_rmx.rmx_rtt), lock(RTT));
	printf("%8d%c ", (int)msec(rtm->rtm_rmx.rmx_rttvar), lock(RTTVAR));
	printf("%8d%c ", (int)rtm->rtm_rmx.rmx_hopcount, lock(HOPCOUNT));
	printf("%8d%c ", (int)rtm->rtm_rmx.rmx_mtu, lock(MTU));
	if (rtm->rtm_rmx.rmx_expire)
		rtm->rtm_rmx.rmx_expire -= time(0);
	printf("%8d%c\n", (int)rtm->rtm_rmx.rmx_expire, lock(EXPIRE));
#undef lock
#undef msec
#define	RTA_IGN	(RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_SRC|RTA_SRCMASK| \
		    RTA_IFP|RTA_IFA|RTA_BRD)
	if (verbose)
		pmsg_common(rtm);
	else if (rtm->rtm_addrs &~ RTA_IGN) {
		(void) printf("sockaddrs: ");
		bprintf(stdout, rtm->rtm_addrs, addrnames);
		putchar('\n');
	}
#undef	RTA_IGN
}

void
pmsg_common(struct rt_msghdr *rtm)
{
	(void) printf("\nlocks: ");
	bprintf(stdout, rtm->rtm_rmx.rmx_locks, metricnames);
	(void) printf(" inits: ");
	bprintf(stdout, rtm->rtm_inits, metricnames);
	pmsg_addrs(((char *)(rtm + 1)), rtm->rtm_addrs);
}

void
pmsg_addrs(char *cp, int addrs)
{
	struct sockaddr *sa;
	int i;

	if (addrs != 0) {
		(void) printf("\nsockaddrs: ");
		bprintf(stdout, addrs, addrnames);
		(void) putchar('\n');
		for (i = 1; i; i <<= 1)
			if (i & addrs) {
				sa = (struct sockaddr *)cp;
				(void) printf(" %s", routename(sa));
				ADVANCE(cp, sa);
			}
	}
	(void) putchar('\n');
	(void) fflush(stdout);
}

void
bprintf(FILE *fp, int b, u_char *s)
{
	int i;
	int gotsome = 0;

	if (b == 0)
		return;
	while ((i = *s++)) {
		if ((b & (1 << (i-1)))) {
			if (gotsome == 0)
				i = '<';
			else
				i = ',';
			(void) putc(i, fp);
			gotsome = 1;
			for (; (i = *s) > 32; s++)
				(void) putc(i, fp);
		} else
			while (*s > 32)
				s++;
	}
	if (gotsome)
		(void) putc('>', fp);
}

int
keyword(char *cp)
{
	struct keytab *kt = keywords;

	while (kt->kt_cp && strcmp(kt->kt_cp, cp))
		kt++;
	return (kt->kt_i);
}

void
sodump(sup su, char *which)
{
#ifdef INET6
	char ntop_buf[NI_MAXHOST];	/*for inet_ntop()*/
#endif

	switch (su->sa.sa_family) {
	case AF_LINK:
		(void) printf("%s: link %s; ",
		    which, link_ntoa(&su->sdl));
		break;
	case AF_ISO:
		(void) printf("%s: iso %s; ",
		    which, iso_ntoa(&su->siso.siso_addr));
		break;
	case AF_INET:
		(void) printf("%s: inet %s; ",
		    which, inet_ntoa(su->sin.sin_addr));
		break;
#ifdef INET6
	case AF_INET6:
		(void) printf("%s: inet6 %s; ",
		    which, inet_ntop(AF_INET6, &su->sin6.sin6_addr,
				     ntop_buf, sizeof(ntop_buf)));
		break;
#endif
	case AF_NS:
		(void) printf("%s: xns %s; ",
		    which, ns_ntoa(su->sns.sns_addr));
		break;
	case AF_IPX:
		(void) printf("%s: ipx %s; ",
		    which, ipx_ntoa(su->sipx.sipx_addr));
		break;
	}
	(void) fflush(stdout);
}

/* States*/
#define VIRGIN	0
#define GOTONE	1
#define GOTTWO	2
/* Inputs */
#define	DIGIT	(4*0)
#define	END	(4*1)
#define DELIM	(4*2)

void
sockaddr(char *addr, struct sockaddr *sa)
{
	char *cp = (char *)sa;
	int size = sa->sa_len;
	char *cplim = cp + size;
	int byte = 0, state = VIRGIN, new = 0;

	memset(cp, 0, size);
	cp++;
	do {
		if ((*addr >= '0') && (*addr <= '9')) {
			new = *addr - '0';
		} else if ((*addr >= 'a') && (*addr <= 'f')) {
			new = *addr - 'a' + 10;
		} else if ((*addr >= 'A') && (*addr <= 'F')) {
			new = *addr - 'A' + 10;
		} else if (*addr == 0)
			state |= END;
		else
			state |= DELIM;
		addr++;
		switch (state /* | INPUT */) {
		case GOTTWO | DIGIT:
			*cp++ = byte; /*FALLTHROUGH*/
		case VIRGIN | DIGIT:
			state = GOTONE; byte = new; continue;
		case GOTONE | DIGIT:
			state = GOTTWO; byte = new + (byte << 4); continue;
		default: /* | DELIM */
			state = VIRGIN; *cp++ = byte; byte = 0; continue;
		case GOTONE | END:
		case GOTTWO | END:
			*cp++ = byte; /* FALLTHROUGH */
		case VIRGIN | END:
			break;
		}
		break;
	} while (cp < cplim);
	sa->sa_len = cp - (char *)sa;
}
