/*	$OpenBSD: route.c,v 1.141 2009/12/01 16:21:46 reyk Exp $	*/
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
#include <net/if_media.h>
#include <netmpls/mpls.h>

#include "keywords.h"
#include "show.h"

const struct if_status_description
			if_status_descriptions[] = LINK_STATE_DESCRIPTIONS;

union	sockunion {
	struct sockaddr		sa;
	struct sockaddr_in	sin;
	struct sockaddr_in6	sin6;
	struct sockaddr_dl	sdl;
	struct sockaddr_rtlabel	rtlabel;
	struct sockaddr_mpls	smpls;
} so_dst, so_gate, so_mask, so_genmask, so_ifa, so_ifp, so_label, so_src;

typedef union sockunion *sup;
pid_t	pid;
int	rtm_addrs, s;
int	forcehost, forcenet, Fflag, nflag, af, qflag, tflag;
int	iflag, verbose, aflen = sizeof(struct sockaddr_in);
int	locking, lockrest, debugonly;
u_long	mpls_flags = MPLS_OP_LOCAL;
u_long	rtm_inits;
uid_t	uid;
u_int	tableid = 0;

struct rt_metrics	rt_metrics;

void	 flushroutes(int, char **);
int	 newroute(int, char **);
void	 show(int, char *[]);
int	 keyword(char *);
void	 monitor(int, char *[]);
int	 prefixlen(char *);
void	 sockaddr(char *, struct sockaddr *);
void	 sodump(sup, char *);
char	*priorityname(u_int8_t);
void	 print_getmsg(struct rt_msghdr *, int);
void	 print_rtmsg(struct rt_msghdr *, int);
void	 pmsg_common(struct rt_msghdr *);
void	 pmsg_addrs(char *, int);
void	 bprintf(FILE *, int, char *);
void	 mask_addr(union sockunion *, union sockunion *, int);
int	 inet6_makenetandmask(struct sockaddr_in6 *);
int	 getaddr(int, char *, struct hostent **);
void	 getmplslabel(char *, int);
int	 rtmsg(int, int, int, u_char);
__dead void usage(char *);
void	 set_metric(char *, int);
void	 inet_makenetandmask(u_int32_t, struct sockaddr_in *, int);
void	 interfaces(void);
void	 getlabel(char *);
int	 gettable(const char *);
int	 rdomain(int, int, char **);

__dead void
usage(char *cp)
{
	extern char *__progname;

	if (cp)
		warnx("botched keyword: %s", cp);
	fprintf(stderr,
	    "usage: %s [-dnqtv] [-T tableid] command [[modifiers] args]\n",
	    __progname);
	fprintf(stderr,
	    "commands: add, change, delete, exec, flush, get, monitor, show\n");
	exit(1);
}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

int
main(int argc, char **argv)
{
	int ch;
	int rval = 0;
	int rtableid = 1;
	int kw;

	if (argc < 2)
		usage(NULL);

	while ((ch = getopt(argc, argv, "dnqtT:v")) != -1)
		switch (ch) {
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
		case 'T':
			rtableid = gettable(optarg);
			break;
		case 'd':
			debugonly = 1;
			break;
		default:
			usage(NULL);
			/* NOTREACHED */
		}
	argc -= optind;
	argv += optind;

	pid = getpid();
	uid = geteuid();
	if (*argv == NULL)
		usage(NULL);

	kw = keyword(*argv);
	switch (kw) {
	case K_EXEC:
		break;
	case K_MONITOR:
		monitor(argc, argv);
		break;
	default:
		if (tflag)
			s = open(_PATH_DEVNULL, O_WRONLY);
		else
			s = socket(PF_ROUTE, SOCK_RAW, 0);
		if (s == -1)
			err(1, "socket");
		break;
	}
	switch (kw) {
	case K_EXEC:
		rval = rdomain(rtableid, argc - 1, argv + 1);
		break;
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
		/* handled above */
		break;
	case K_FLUSH:
		flushroutes(argc, argv);
		break;
	default:
		usage(*argv);
		/* NOTREACHED */
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
	const char *errstr;
	size_t needed;
	int mib[7], rlen, seqno;
	char *buf = NULL, *next, *lim = NULL;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;
	u_char prio = 0;
	unsigned int ifindex = 0;

	if (uid)
		errx(1, "must be root to alter routing table");
	shutdown(s, 0); /* Don't want to read back our messages */
	while (--argc > 0) {
		if (**(++argv) == '-')
			switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
			case K_INET6:
				af = AF_INET6;
				break;
			case K_LINK:
				af = AF_LINK;
				break;
			case K_MPLS:
				af = AF_MPLS;
				break;
			case K_IFACE:
			case K_INTERFACE:
				if (!--argc)
					usage(1+*argv);
				ifindex = if_nametoindex(*++argv);
				if (ifindex == 0)
					errx(1, "no such interface %s", *argv);
				break;
			case K_PRIORITY:
				if (!--argc)
					usage(1+*argv);
				prio = strtonum(*++argv, 0, RTP_MAX, &errstr);
				if (errstr)
					errx(1, "priority is %s: %s", errstr,
					    *argv);
				break;
			default:
				usage(*argv);
				/* NOTREACHED */
			}
		else
			usage(*argv);
	}
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;		/* protocol */
	mib[3] = 0;		/* wildcard address family */
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;		/* no flags */
	mib[6] = tableid;
	if (sysctl(mib, 7, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if (needed) {
		if ((buf = malloc(needed)) == NULL)
			err(1, "malloc");
		if (sysctl(mib, 7, buf, &needed, NULL, 0) < 0)
			err(1, "actual retrieval of routing table");
		lim = buf + needed;
	}
	if (verbose) {
		printf("Examining routing table from sysctl\n");
		if (af)
			printf("(address family %s)\n", (*argv + 1));
	}
	if (buf == NULL)
		return;

	seqno = 0;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		if (verbose)
			print_rtmsg(rtm, rtm->rtm_msglen);
		if ((rtm->rtm_flags & (RTF_GATEWAY|RTF_STATIC|RTF_LLINFO)) == 0)
			continue;
		sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
		if (af && sa->sa_family != af)
			continue;
		if (ifindex && rtm->rtm_index != ifindex)
			continue;
		if (prio && rtm->rtm_priority != prio)
			continue;
		if (sa->sa_family == AF_KEY)
			continue;  /* Don't flush SPD */
		if (debugonly)
			continue;
		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno;
		rtm->rtm_tableid = tableid;
		rlen = write(s, next, rtm->rtm_msglen);
		if (rlen < (int)rtm->rtm_msglen) {
			warn("write to routing socket");
			printf("got only %d for rlen\n", rlen);
			break;
		}
		seqno++;
		if (qflag)
			continue;
		if (verbose)
			print_rtmsg(rtm, rlen);
		else {
			struct sockaddr *sa = (struct sockaddr *)(next +
			    rtm->rtm_hdrlen);
			printf("%-20.20s ", rtm->rtm_flags & RTF_HOST ?
			    routename(sa) : netname(sa, NULL)); /* XXX extract
								   netmask */
			sa = (struct sockaddr *)
			    (ROUNDUP(sa->sa_len) + (char *)sa);
			printf("%-20.20s ", routename(sa));
			printf("done\n");
		}
	}
	free(buf);
}

void
set_metric(char *value, int key)
{
	int flag = 0;
	u_int noval, *valp = &noval;
	const char *errstr;

	switch (key) {
	case K_MTU:
		valp = &rt_metrics.rmx_mtu;
		flag = RTV_MTU;
		break;
	case K_EXPIRE:
		valp = &rt_metrics.rmx_expire;
		flag = RTV_EXPIRE;
		break;
	case K_HOPCOUNT:
	case K_RECVPIPE:
	case K_SENDPIPE:
	case K_SSTHRESH:
	case K_RTT:
	case K_RTTVAR:
		/* no longer used, only for compatibility */
		return;
	default:
		errx(1, "king bula sez: set_metric with invalid key");
	}
	rtm_inits |= flag;
	if (lockrest || locking)
		rt_metrics.rmx_locks |= flag;
	if (locking)
		locking = 0;
	*valp = strtonum(value, 0, UINT_MAX, &errstr);
	if (errstr)
		errx(1, "set_metric: %s is %s", value, errstr);
}

int
newroute(int argc, char **argv)
{
	const char *errstr;
	char *cmd, *dest = "", *gateway = "", *error;
	int ishost = 0, ret = 0, attempts, oerrno, flags = RTF_STATIC;
	int fmask = 0;
	int key;
	u_char prio = 0;
	struct hostent *hp = NULL;

	if (uid)
		errx(1, "must be root to alter routing table");
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
			case K_INET:
				af = AF_INET;
				aflen = sizeof(struct sockaddr_in);
				break;
			case K_INET6:
				af = AF_INET6;
				aflen = sizeof(struct sockaddr_in6);
				break;
			case K_SA:
				af = PF_ROUTE;
				aflen = sizeof(union sockunion);
				break;
			case K_MPLS:
				af = AF_MPLS;
				aflen = sizeof(struct sockaddr_mpls);
				break;
			case K_MPLSLABEL:
				if (!--argc)
					usage(1+*argv);
				if (af != AF_INET && af != AF_INET6)
					errx(1, "-mplslabel requires " 
					    "-inet or -inet6");
				getmplslabel(*++argv, 0);
				mpls_flags = MPLS_OP_PUSH;
				break;
			case K_IN:
				if (!--argc)
					usage(1+*argv);
				if (af != AF_MPLS)
					errx(1, "-in requires -mpls");
				getmplslabel(*++argv, 1);
				break;
			case K_OUT:
				if (!--argc)
					usage(1+*argv);
				if (af != AF_MPLS)
					errx(1, "-out requires -mpls");
				if (mpls_flags == MPLS_OP_LOCAL)
					errx(1, "-out requires -push, -pop, "
					    "-swap");
				getmplslabel(*++argv, 0);
				break;
			case K_POP:
				if (af != AF_MPLS)
					errx(1, "-pop requires -mpls");
				mpls_flags = MPLS_OP_POP;
				break;
			case K_PUSH:
				if (af != AF_MPLS)
					errx(1, "-push requires -mpls");
				mpls_flags = MPLS_OP_PUSH;
				break;
			case K_SWAP:
				if (af != AF_MPLS)
					errx(1, "-swap requires -mpls");
				mpls_flags = MPLS_OP_SWAP;
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
				getaddr(RTA_IFA, *++argv, NULL);
				break;
			case K_IFP:
				if (!--argc)
					usage(1+*argv);
				getaddr(RTA_IFP, *++argv, NULL);
				break;
			case K_GENMASK:
				if (!--argc)
					usage(1+*argv);
				getaddr(RTA_GENMASK, *++argv, NULL);
				break;
			case K_GATEWAY:
				if (!--argc)
					usage(1+*argv);
				getaddr(RTA_GATEWAY, *++argv, NULL);
				gateway = *argv;
				break;
			case K_DST:
				if (!--argc)
					usage(1+*argv);
				ishost = getaddr(RTA_DST, *++argv, &hp);
				dest = *argv;
				break;
			case K_LABEL:
				if (!--argc)
					usage(1+*argv);
				getlabel(*++argv);
				break;
			case K_NETMASK:
				if (!--argc)
					usage(1+*argv);
				getaddr(RTA_NETMASK, *++argv, NULL);
				/* FALLTHROUGH */
			case K_NET:
				forcenet++;
				break;
			case K_PREFIXLEN:
				if (!--argc)
					usage(1+*argv);
				ishost = prefixlen(*++argv);
				break;
			case K_MPATH:
				flags |= RTF_MPATH;
				break;
			case K_JUMBO:
				flags |= RTF_JUMBO;
				fmask |= RTF_JUMBO;
				break;
			case K_NOJUMBO:
				flags &= ~RTF_JUMBO;
				fmask |= RTF_JUMBO;
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
			case K_PRIORITY:
				if (!--argc)
					usage(1+*argv);
				prio = strtonum(*++argv, 0, RTP_MAX, &errstr);
				if (errstr)
					errx(1, "priority is %s: %s", errstr,
					    *argv);
				break;
			default:
				usage(1+*argv);
				/* NOTREACHED */
			}
		} else {
			if ((rtm_addrs & RTA_DST) == 0) {
				dest = *argv;
				ishost = getaddr(RTA_DST, *argv, &hp);
			} else if ((rtm_addrs & RTA_GATEWAY) == 0) {
				gateway = *argv;
				getaddr(RTA_GATEWAY, *argv, &hp);
			} else
				usage(NULL);
		}
	}
	if (forcehost)
		ishost = 1;
	if (forcenet)
		ishost = 0;
	if (forcenet && !(rtm_addrs & RTA_NETMASK))
		errx(1, "netmask missing");
	flags |= RTF_UP;
	if (ishost)
		flags |= RTF_HOST;
	if (iflag == 0)
		flags |= RTF_GATEWAY;
	for (attempts = 1; ; attempts++) {
		errno = 0;
		if ((ret = rtmsg(*cmd, flags, fmask, prio)) == 0)
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
		printf("%s %s %s", cmd, ishost ? "host" : "net", dest);
		if (*gateway) {
			printf(": gateway %s", gateway);
			if (attempts > 1 && ret == 0 && af == AF_INET)
			    printf(" (%s)", inet_ntoa(so_gate.sin.sin_addr));
		}
		if (ret == 0)
			printf("\n");
		if (ret != 0) {
			switch (oerrno) {
			case ESRCH:
				error = "not in table";
				break;
			case EBUSY:
				error = "entry in use";
				break;
			case ENOBUFS:
				error = "routing table overflow";
				break;
			default:
				error = strerror(oerrno);
				break;
			}
			printf(": %s\n", error);
		}
	}
	return (ret != 0);
}

void
show(int argc, char *argv[])
{
	int	af = 0;

	while (--argc > 0) {
		if (**(++argv)== '-')
			switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
			case K_INET6:
				af = AF_INET6;
				break;
			case K_LINK:
				af = AF_LINK;
				break;
			case K_MPLS:
				af = AF_MPLS;
				break;
			case K_ENCAP:
				af = PF_KEY;
				break;
			case K_GATEWAY:
				Fflag = 1;
				break;
			default:
				usage(*argv);
				/* NOTREACHED */
			}
		else
			usage(*argv);
	}

	p_rttables(af, tableid);
}

void
inet_makenetandmask(u_int32_t net, struct sockaddr_in *sin, int bits)
{
	u_int32_t addr, mask = 0;
	char *cp;

	rtm_addrs |= RTA_NETMASK;
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
			mask = IN_CLASSA_NET;
		else if ((addr & IN_CLASSB_HOST) == 0)
			mask = IN_CLASSB_NET;
		else if ((addr & IN_CLASSC_HOST) == 0)
			mask = IN_CLASSC_NET;
		else
			mask = 0xffffffff;
	}
	addr &= mask;
	sin->sin_addr.s_addr = htonl(addr);
	sin = &so_mask.sin;
	sin->sin_addr.s_addr = htonl(mask);
	sin->sin_len = 0;
	sin->sin_family = 0;
	cp = (char *)(&sin->sin_addr + 1);
	while (*--cp == '\0' && cp > (char *)sin)
		;
	sin->sin_len = 1 + cp - (char *)sin;
}

/*
 * XXX the function may need more improvement...
 */
int
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
		return (1);
	else {
		rtm_addrs |= RTA_NETMASK;
		prefixlen(plen);
		return (0);
	}
}

/*
 * Interpret an argument as a network address of some kind,
 * returning 1 if a host address, 0 if a network address.
 */
int
getaddr(int which, char *s, struct hostent **hpp)
{
	sup su = NULL;
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
	default:
		errx(1, "internal error");
		/* NOTREACHED */
	}
	su->sa.sa_len = aflen;
	su->sa.sa_family = afamily;

	if (strcmp(s, "default") == 0) {
		switch (which) {
		case RTA_DST:
			forcenet++;
			getaddr(RTA_NETMASK, s, NULL);
			break;
		case RTA_NETMASK:
		case RTA_GENMASK:
			su->sa.sa_len = 0;
		}
		return (0);
	}

	switch (afamily) {
	case AF_INET6:
	    {
		struct addrinfo hints, *res;

		memset(&hints, 0, sizeof(hints));
		hints.ai_family = afamily;	/*AF_INET6*/
		hints.ai_flags = AI_NUMERICHOST;
		hints.ai_socktype = SOCK_DGRAM;		/*dummy*/
		if (getaddrinfo(s, "0", &hints, &res) != 0) {
			hints.ai_flags = 0;
			if (getaddrinfo(s, "0", &hints, &res) != 0)
				errx(1, "%s: bad value", s);
		}
		if (sizeof(su->sin6) != res->ai_addrlen)
			errx(1, "%s: bad value", s);
		if (res->ai_next)
			errx(1, "%s: resolved to multiple values", s);
		memcpy(&su->sin6, res->ai_addr, sizeof(su->sin6));
		freeaddrinfo(res);
		if ((IN6_IS_ADDR_LINKLOCAL(&su->sin6.sin6_addr) ||
		     IN6_IS_ADDR_MC_LINKLOCAL(&su->sin6.sin6_addr) ||
		     IN6_IS_ADDR_MC_INTFACELOCAL(&su->sin6.sin6_addr)) &&
		    su->sin6.sin6_scope_id) {
			*(u_int16_t *)&su->sin6.sin6_addr.s6_addr[2] =
				htons(su->sin6.sin6_scope_id);
			su->sin6.sin6_scope_id = 0;
		}
		if (hints.ai_flags == AI_NUMERICHOST) {
			if (which == RTA_DST)
				return (inet6_makenetandmask(&su->sin6));
			return (0);
		} else
			return (1);
	    }

	case AF_LINK:
		link_addr(s, &su->sdl);
		return (1);
	case AF_MPLS:
		errx(1, "mpls labels require -in or -out switch");
	case PF_ROUTE:
		su->sa.sa_len = sizeof(*su);
		sockaddr(s, &su->sa);
		return (1);

	case AF_INET:
		if (hpp != NULL)
			*hpp = NULL;
		if (which == RTA_DST && !forcehost) {
			bits = inet_net_pton(AF_INET, s, &su->sin.sin_addr,
			    sizeof(su->sin.sin_addr));
			if (bits == 32)
				return (1);
			if (bits >= 0) {
				inet_makenetandmask(ntohl(
				    su->sin.sin_addr.s_addr),
				    &su->sin, bits);
				return (0);
			}
			np = getnetbyname(s);
			if (np != NULL && np->n_net != 0) {
				inet_makenetandmask(np->n_net, &su->sin, 0);
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
		/* NOTREACHED */

	default:
		errx(1, "%d: bad address family", afamily);
		/* NOTREACHED */
	}
}

void
getmplslabel(char *s, int in)
{
	sup su = NULL;
	const char *errstr;
	u_int32_t label;

	label = strtonum(s, 0, 0x000fffff, &errstr);
	if (errstr)
		errx(1, "bad label: %s is %s", s, errstr);
	if (in) {
		rtm_addrs |= RTA_DST;
		su = &so_dst;
		su->smpls.smpls_label = htonl(label << MPLS_LABEL_OFFSET);
	} else {
		rtm_addrs |= RTA_SRC;
		su = &so_src;
		su->smpls.smpls_label = htonl(label << MPLS_LABEL_OFFSET);
	}

	su->sa.sa_len = sizeof(struct sockaddr_mpls);
	su->sa.sa_family = AF_MPLS;
}

int
prefixlen(char *s)
{
	const char *errstr;
	int len, q, r;
	int max;

	switch (af) {
	case AF_INET:
		max = sizeof(struct in_addr) * 8;
		break;
	case AF_INET6:
		max = sizeof(struct in6_addr) * 8;
		break;
	default:
		errx(1, "prefixlen is not supported with af %d", af);
		/* NOTREACHED */
	}

	rtm_addrs |= RTA_NETMASK;
	len = strtonum(s, 0, max, &errstr);
	if (errstr)
		errx(1, "prefixlen %s is %s", s, errstr);

	q = len >> 3;
	r = len & 7;
	switch (af) {
	case AF_INET:
		memset(&so_mask, 0, sizeof(so_mask));
		so_mask.sin.sin_family = AF_INET;
		so_mask.sin.sin_len = sizeof(struct sockaddr_in);
		so_mask.sin.sin_addr.s_addr = htonl(0xffffffff << (32 - len));
		break;
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
	}
	return (len == max);
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
		err(1, "route-sysctl-estimate");
	if (needed) {
		if ((buf = malloc(needed)) == NULL)
			err(1, "malloc");
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
			err(1, "actual retrieval of interface table");
		lim = buf + needed;
		for (next = buf; next < lim; next += rtm->rtm_msglen) {
			rtm = (struct rt_msghdr *)next;
			print_rtmsg(rtm, rtm->rtm_msglen);
		}
		free(buf);
	}
}

void
monitor(int argc, char *argv[])
{
	int af = 0;
	unsigned int filter = 0;
	int n;
	char msg[2048];
	time_t now;

	while (--argc > 0) {
		if (**(++argv)== '-')
			switch (keyword(*argv + 1)) {
			case K_INET:
				af = AF_INET;
				break;
			case K_INET6:
				af = AF_INET6;
				break;
			case K_IFACE:
				filter = ROUTE_FILTER(RTM_IFINFO) |
				    ROUTE_FILTER(RTM_IFANNOUNCE);
				break;
			default:
				usage(*argv);
				/* NOTREACHED */
			}
		else
			usage(*argv);
	}

	s = socket(PF_ROUTE, SOCK_RAW, af);
	if (s == -1)
		err(1, "socket");

	if (setsockopt(s, AF_ROUTE, ROUTE_MSGFILTER, &filter,
	    sizeof(filter)) == -1)
		err(1, "setsockopt");

	verbose = 1;
	if (debugonly) {
		interfaces();
		exit(0);
	}
	for (;;) {
		if ((n = read(s, msg, sizeof(msg))) == -1) {
			if (errno == EINTR)
				continue;
			err(1, "read");
		}
		now = time(NULL);
		printf("got message of size %d on %s", n, ctime(&now));
		print_rtmsg((struct rt_msghdr *)msg, n);
	}
}

struct {
	struct rt_msghdr	m_rtm;
	char			m_space[512];
} m_rtmsg;

int
rtmsg(int cmd, int flags, int fmask, u_char prio)
{
	static int seq;
	char *cp = m_rtmsg.m_space;
	int l;

#define NEXTADDR(w, u)				\
	if (rtm_addrs & (w)) {			\
		l = ROUNDUP(u.sa.sa_len);	\
		memcpy(cp, &(u), l);		\
		cp += l;			\
		if (verbose)			\
			sodump(&(u), #u);	\
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
	rtm.rtm_fmask = fmask;
	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_seq = ++seq;
	rtm.rtm_addrs = rtm_addrs;
	rtm.rtm_rmx = rt_metrics;
	rtm.rtm_inits = rtm_inits;
	rtm.rtm_tableid = tableid;
	rtm.rtm_priority = prio;
	rtm.rtm_mpls = mpls_flags;
	rtm.rtm_hdrlen = sizeof(rtm);

	if (rtm_addrs & RTA_NETMASK)
		mask_addr(&so_dst, &so_mask, RTA_DST);
	NEXTADDR(RTA_DST, so_dst);
	NEXTADDR(RTA_GATEWAY, so_gate);
	NEXTADDR(RTA_NETMASK, so_mask);
	NEXTADDR(RTA_GENMASK, so_genmask);
	NEXTADDR(RTA_IFP, so_ifp);
	NEXTADDR(RTA_IFA, so_ifa);
	NEXTADDR(RTA_LABEL, so_label);
	NEXTADDR(RTA_SRC, so_src);
	rtm.rtm_msglen = l = cp - (char *)&m_rtmsg;
	if (verbose)
		print_rtmsg(&rtm, l);
	if (debugonly)
		return (0);
	if (write(s, &m_rtmsg, l) != l) {
		if (qflag == 0)
			warn("writing to routing socket");
		return (-1);
	}
	if (cmd == RTM_GET) {
		do {
			l = read(s, &m_rtmsg, sizeof(m_rtmsg));
		} while (l > 0 && (rtm.rtm_version != RTM_VERSION ||
		    rtm.rtm_seq != seq || rtm.rtm_pid != pid));
		if (l == -1)
			warn("read from routing socket");
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
		if (*--cp1 != '\0') {
			mask->sa.sa_len = 1 + cp1 - (char *)mask;
			break;
		}
	if ((rtm_addrs & which) == 0)
		return;
	switch (addr->sa.sa_family) {
	case AF_INET:
	case AF_INET6:
	case 0:
		return;
	}
	cp1 = mask->sa.sa_len + 1 + (char *)addr;
	cp2 = addr->sa.sa_len + 1 + (char *)addr;
	while (cp2 > cp1)
		*--cp2 = '\0';
	cp2 = mask->sa.sa_len + 1 + (char *)mask;
	while (cp1 > addr->sa.sa_data)
		*--cp1 &= *--cp2;
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
	NULL
};

char metricnames[] =
"\011priority\010rttvar\7rtt\6ssthresh\5sendpipe\4recvpipe\3expire\2hopcount\1mtu";
char routeflags[] =
"\1UP\2GATEWAY\3HOST\4REJECT\5DYNAMIC\6MODIFIED\7DONE\010MASK_PRESENT\011CLONING"
"\012XRESOLVE\013LLINFO\014STATIC\015BLACKHOLE\016PROTO3\017PROTO2\020PROTO1\021CLONED\022SOURCE\023MPATH\024JUMBO\025MPLS";
char ifnetflags[] =
"\1UP\2BROADCAST\3DEBUG\4LOOPBACK\5PTP\6NOTRAILERS\7RUNNING\010NOARP\011PPROMISC"
"\012ALLMULTI\013OACTIVE\014SIMPLEX\015LINK0\016LINK1\017LINK2\020MULTICAST";
char addrnames[] =
"\1DST\2GATEWAY\3NETMASK\4GENMASK\5IFP\6IFA\7AUTHOR\010BRD\013LABEL";

const char *
get_linkstate(int mt, int link_state)
{
	const struct if_status_description *p;
	static char buf[8];

	for (p = if_status_descriptions; p->ifs_string != NULL; p++) {
		if (LINK_STATE_DESC_MATCH(p, mt, link_state))
			return (p->ifs_string);
	}
	snprintf(buf, sizeof(buf), "[#%d]", link_state);
	return buf;
}

void
print_rtmsg(struct rt_msghdr *rtm, int msglen)
{
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct if_announcemsghdr *ifan;
	char ifname[IF_NAMESIZE];

	if (verbose == 0)
		return;
	if (rtm->rtm_version != RTM_VERSION) {
		warnx("routing message version %d not understood",
		    rtm->rtm_version);
		return;
	}
	printf("%s: len %d, ", msgtypes[rtm->rtm_type], rtm->rtm_msglen);
	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		(void) printf("if# %d, ", ifm->ifm_index);
		if (if_indextoname(ifm->ifm_index, ifname) != NULL)
			printf("name: %s, ", ifname);
		printf("link: %s, flags:",
		    get_linkstate(ifm->ifm_data.ifi_type,
		    ifm->ifm_data.ifi_link_state));
		bprintf(stdout, ifm->ifm_flags, ifnetflags);
		pmsg_addrs((char *)ifm + ifm->ifm_hdrlen, ifm->ifm_addrs);
		break;
	case RTM_NEWADDR:
	case RTM_DELADDR:
		ifam = (struct ifa_msghdr *)rtm;
		printf("metric %d, flags:", ifam->ifam_metric);
		bprintf(stdout, ifam->ifam_flags, routeflags);
		pmsg_addrs((char *)ifam + ifam->ifam_hdrlen, ifam->ifam_addrs);
		break;
	case RTM_IFANNOUNCE:
		ifan = (struct if_announcemsghdr *)rtm;
		printf("if# %d, name %s, what: ",
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
		printf("priority %d, ", rtm->rtm_priority);
		printf("table %u, pid: %ld, seq %d, errno %d\nflags:",
		    rtm->rtm_tableid, (long)rtm->rtm_pid, rtm->rtm_seq,
		    rtm->rtm_errno);
		bprintf(stdout, rtm->rtm_flags, routeflags);
		if (verbose) {
#define lock(f)	((rtm->rtm_rmx.rmx_locks & __CONCAT(RTV_,f)) ? 'L' : ' ')
			if (rtm->rtm_rmx.rmx_expire)
				rtm->rtm_rmx.rmx_expire -= time(NULL);
			printf("\nuse: %8llu   mtu: %8u%c   expire: %8d%c",
			    rtm->rtm_rmx.rmx_pksent,
			    rtm->rtm_rmx.rmx_mtu, lock(MTU),
			    rtm->rtm_rmx.rmx_expire, lock(EXPIRE));
#undef lock
		}
		pmsg_common(rtm);
	}
}

char *
priorityname(u_int8_t prio)
{
	switch (prio) {
	case RTP_NONE:
		return ("none");
	case RTP_CONNECTED:
		return ("connected");
	case RTP_STATIC:
		return ("static");
	case RTP_OSPF:
		return ("ospf");
	case RTP_ISIS:
		return ("is-is");
	case RTP_RIP:
		return ("rip");
	case RTP_BGP:
		return ("bgp");
	case RTP_DEFAULT:
		return ("default");
	default:
		return ("");
	}
}

void
print_getmsg(struct rt_msghdr *rtm, int msglen)
{
	struct sockaddr *dst = NULL, *gate = NULL, *mask = NULL, *ifa = NULL;
	struct sockaddr_dl *ifp = NULL;
	struct sockaddr_rtlabel *sa_rl = NULL;
	struct sockaddr *sa;
	char *cp;
	int i;

	printf("   route to: %s\n", routename(&so_dst.sa));
	if (rtm->rtm_version != RTM_VERSION) {
		warnx("routing message version %d not understood",
		    rtm->rtm_version);
		return;
	}
	if (rtm->rtm_msglen > msglen)
		warnx("message length mismatch, in packet %d, returned %d",
		    rtm->rtm_msglen, msglen);
	if (rtm->rtm_errno) {
		warnx("RTM_GET: %s (errno %d)",
		    strerror(rtm->rtm_errno), rtm->rtm_errno);
		return;
	}
	cp = ((char *)rtm + rtm->rtm_hdrlen);
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
				case RTA_IFA:
					ifa = sa;
					break;
				case RTA_IFP:
					if (sa->sa_family == AF_LINK &&
					   ((struct sockaddr_dl *)sa)->sdl_nlen)
						ifp = (struct sockaddr_dl *)sa;
					break;
				case RTA_LABEL:
					sa_rl = (struct sockaddr_rtlabel *)sa;
					break;
				}
				ADVANCE(cp, sa);
			}
	if (dst && mask)
		mask->sa_family = dst->sa_family;	/* XXX */
	if (dst)
		printf("destination: %s\n", routename(dst));
	if (mask) {
		int savenflag = nflag;

		nflag = 1;
		printf("       mask: %s\n", routename(mask));
		nflag = savenflag;
	}
	if (gate && rtm->rtm_flags & RTF_GATEWAY)
		printf("    gateway: %s\n", routename(gate));
	if (ifp)
		printf("  interface: %.*s\n",
		    ifp->sdl_nlen, ifp->sdl_data);
	if (ifa)
		printf(" if address: %s\n", routename(ifa));
	printf("   priority: %u (%s)\n", rtm->rtm_priority,
	   priorityname(rtm->rtm_priority)); 
	printf("      flags: ");
	bprintf(stdout, rtm->rtm_flags, routeflags);
	printf("\n");
	if (sa_rl != NULL)
		printf("      label: %s\n", sa_rl->sr_label);

#define lock(f)	((rtm->rtm_rmx.rmx_locks & __CONCAT(RTV_,f)) ? 'L' : ' ')
	printf("%s\n", "     use       mtu    expire");
	printf("%8llu  ", rtm->rtm_rmx.rmx_pksent);
	printf("%8u%c ", rtm->rtm_rmx.rmx_mtu, lock(MTU));
	if (rtm->rtm_rmx.rmx_expire)
		rtm->rtm_rmx.rmx_expire -= time(NULL);
	printf("%8d%c\n", rtm->rtm_rmx.rmx_expire, lock(EXPIRE));
#undef lock
#define	RTA_IGN	(RTA_DST|RTA_GATEWAY|RTA_NETMASK|RTA_IFP|RTA_IFA|RTA_BRD)
	if (verbose)
		pmsg_common(rtm);
	else if (rtm->rtm_addrs &~ RTA_IGN) {
		printf("sockaddrs: ");
		bprintf(stdout, rtm->rtm_addrs, addrnames);
		putchar('\n');
	}
#undef	RTA_IGN
}

void
pmsg_common(struct rt_msghdr *rtm)
{
	printf("\nlocks: ");
	bprintf(stdout, rtm->rtm_rmx.rmx_locks, metricnames);
	printf(" inits: ");
	bprintf(stdout, rtm->rtm_inits, metricnames);
	pmsg_addrs(((char *)rtm + rtm->rtm_hdrlen), rtm->rtm_addrs);
}

void
pmsg_addrs(char *cp, int addrs)
{
	struct sockaddr *sa;
	int family = AF_UNSPEC;
	int i;
	char *p;

	if (addrs != 0) {
		printf("\nsockaddrs: ");
		bprintf(stdout, addrs, addrnames);
		putchar('\n');
		/* first run, search for address family */
		p = cp;
		for (i = 1; i; i <<= 1)
			if (i & addrs) {
				sa = (struct sockaddr *)p;
				if (family == AF_UNSPEC)
					switch (i) {
					case RTA_DST:
					case RTA_IFA:
						family = sa->sa_family;
					}
				ADVANCE(p, sa);
			}
		/* second run, set address family for mask and print */
		p = cp;
		for (i = 1; i; i <<= 1)
			if (i & addrs) {
				sa = (struct sockaddr *)p;
				if (family != AF_UNSPEC)
					switch (i) {
					case RTA_NETMASK:
					case RTA_GENMASK:
						sa->sa_family = family;
					}
				printf(" %s", routename(sa));
				ADVANCE(p, sa);
			}
	}
	putchar('\n');
	fflush(stdout);
}

void
bprintf(FILE *fp, int b, char *s)
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
			putc(i, fp);
			gotsome = 1;
			for (; (i = *s) > 32; s++)
				putc(i, fp);
		} else
			while (*s > 32)
				s++;
	}
	if (gotsome)
		putc('>', fp);
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
	switch (su->sa.sa_family) {
	case AF_LINK:
		printf("%s: link %s; ", which, link_ntoa(&su->sdl));
		break;
	case AF_INET:
		printf("%s: inet %s; ", which, inet_ntoa(su->sin.sin_addr));
		break;
	case AF_INET6:
	    {
		char ntop_buf[NI_MAXHOST];

		printf("%s: inet6 %s; ",
		    which, inet_ntop(AF_INET6, &su->sin6.sin6_addr,
		    ntop_buf, sizeof(ntop_buf)));
		break;
	    }
	}
	fflush(stdout);
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
		} else if (*addr == '\0')
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

void
getlabel(char *name)
{
	so_label.rtlabel.sr_len = sizeof(so_label.rtlabel);
	so_label.rtlabel.sr_family = AF_UNSPEC;
	if (strlcpy(so_label.rtlabel.sr_label, name,
	    sizeof(so_label.rtlabel.sr_label)) >=
	    sizeof(so_label.rtlabel.sr_label))
		errx(1, "label too long");
	rtm_addrs |= RTA_LABEL;
}

int
gettable(const char *s)
{
	const char	*errstr;

	tableid = strtonum(s, 0, RT_TABLEID_MAX, &errstr);
	if (errstr)
		errx(1, "invalid table id: %s", errstr);
	return (tableid);
}

int
rdomain(int rtableid, int argc, char **argv)
{
	if (!argc)
		usage(NULL);
	if (setrdomain(rtableid) == -1)
		err(1, "setrdomain");
	execvp(*argv, argv);
	return (errno == ENOENT ? 127 : 126);
}
