/*	$OpenBSD: show.c,v 1.36 2004/09/22 01:07:10 jaredy Exp $	*/
/*	$NetBSD: show.c,v 1.1 1996/11/15 18:01:41 gwr Exp $	*/

/*
 * Copyright (c) 1983, 1988, 1993
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
#if 0
static char sccsid[] = "from: @(#)route.c	8.3 (Berkeley) 3/9/94";
#else
static const char rcsid[] = "$OpenBSD: show.c,v 1.36 2004/09/22 01:07:10 jaredy Exp $";
#endif
#endif /* not lint */

#include <sys/param.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/mbuf.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netipx/ipx.h>
#include <netinet/if_ether.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>

#include <sys/sysctl.h>

#include <netdb.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "show.h"

#define PLEN  (LONG_BIT / 4 + 2) /* XXX this is also defined in netstat.h */

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))
#define ADVANCE(x, n) (x += ROUNDUP((n)->sa_len))

/*
 * Definitions for showing gateway flags.
 */
struct bits {
	int	b_mask;
	char	b_val;
};
static const struct bits bits[] = {
	{ RTF_UP,	'U' },
	{ RTF_GATEWAY,	'G' },
	{ RTF_HOST,	'H' },
	{ RTF_REJECT,	'R' },
	{ RTF_BLACKHOLE, 'B' },
	{ RTF_DYNAMIC,	'D' },
	{ RTF_MODIFIED,	'M' },
	{ RTF_DONE,	'd' }, /* Completed -- for routing messages only */
	{ RTF_MASK,	'm' }, /* Mask Present -- for routing messages only */
	{ RTF_CLONING,	'C' },
	{ RTF_XRESOLVE,	'X' },
	{ RTF_LLINFO,	'L' },
	{ RTF_STATIC,	'S' },
	{ RTF_PROTO1,	'1' },
	{ RTF_PROTO2,	'2' },
	{ RTF_PROTO3,	'3' },
	{ RTF_CLONED,	'c' },
	{ 0 }
};

void	 pr_rthdr(int, int);
void	 p_rtentry(struct rt_msghdr *, int);
void	 pr_family(int);
void	 p_sockaddr(struct sockaddr *, struct sockaddr *, int, int);
void	 p_flags(int, char *);
char	*routename4(in_addr_t);
#ifdef INET6
char	*routename6(struct sockaddr_in6 *);
#endif
char	*any_ntoa(const struct sockaddr *);

/*
 * Print routing tables.
 */
void
p_rttables(int af, int Aflag)
{
	struct rt_msghdr *rtm;
	char *buf = NULL, *next, *lim = NULL;
	size_t needed;
	int mib[6];
        struct sockaddr *sa;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = af;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)	{
		perror("route-sysctl-estimate");
		exit(1);
	}
	if (needed > 0) {
		if ((buf = malloc(needed)) == 0) {
			printf("out of space\n");
			exit(1);
		}
		if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0) {
			perror("sysctl of routing table");
			exit(1);
		}
		lim  = buf + needed;
	}

	printf("Routing tables\n");

	if (buf) {
		for (next = buf; next < lim; next += rtm->rtm_msglen) {
			rtm = (struct rt_msghdr *)next;
			sa = (struct sockaddr *)(rtm + 1);
			if (af != AF_UNSPEC && sa->sa_family != af)
				continue;
			p_rtentry(rtm, Aflag);
		}
		free(buf);
	}
}

/* column widths; each followed by one space */
#ifndef INET6
#define	WID_DST(af)	18	/* width of destination column */
#define	WID_GW(af)	18	/* width of gateway column */
#else
/* width of destination/gateway column */
#if 1
/* strlen("fe80::aaaa:bbbb:cccc:dddd@gif0") == 30, strlen("/128") == 4 */
#define	WID_DST(af)	((af) == AF_INET6 ? (nflag ? 34 : 18) : 18)
#define	WID_GW(af)	((af) == AF_INET6 ? (nflag ? 30 : 18) : 18)
#else
/* strlen("fe80::aaaa:bbbb:cccc:dddd") == 25, strlen("/128") == 4 */
#define	WID_DST(af)	((af) == AF_INET6 ? (nflag ? 29 : 18) : 18)
#define	WID_GW(af)	((af) == AF_INET6 ? (nflag ? 25 : 18) : 18)
#endif
#endif /* INET6 */

/*
 * Print header for routing table columns.
 */
void
pr_rthdr(int af, int Aflag)
{
	if (Aflag)
		printf("%-*.*s ", PLEN, PLEN, "Address");
	if (af != PF_KEY)
		printf("%-*.*s %-*.*s %-6.6s %6.6s %8.8s %6.6s  %s\n",
		    WID_DST(af), WID_DST(af), "Destination",
		    WID_GW(af), WID_GW(af), "Gateway",
		    "Flags", "Refs", "Use", "Mtu", "Interface");
	else
		printf("%-18s %-5s %-18s %-5s %-5s %-22s\n",
		    "Source", "Port", "Destination",
		    "Port", "Proto", "SA(Address/Proto/Type/Direction)");
}

static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}

/*
 * Print a routing table entry.
 */
void
p_rtentry(rtm, Aflag)
	struct rt_msghdr *rtm;
	int		  Aflag;
{
	static int	 old_af = -1;
	struct sockaddr	*sa = (struct sockaddr *)(rtm + 1);
	struct sockaddr	*mask, *rti_info[RTAX_MAX];
	char		 ifbuf[IF_NAMESIZE];


	if (old_af != sa->sa_family) {
		old_af = sa->sa_family;
		pr_family(sa->sa_family);
		pr_rthdr(sa->sa_family, Aflag);
	}
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	mask = rti_info[RTAX_NETMASK];
	if ((sa = rti_info[RTAX_DST]) == NULL)
		return;
	
	p_sockaddr(sa, mask, rtm->rtm_flags, WID_DST(sa->sa_family));
	p_sockaddr(rti_info[RTAX_GATEWAY], 0, RTF_HOST, WID_GW(sa->sa_family));
	p_flags(rtm->rtm_flags, "%-6.6s ");
	printf("%6d %8ld ", 0, rtm->rtm_rmx.rmx_pksent);
	if (rtm->rtm_rmx.rmx_mtu)
		printf("%6ld ", rtm->rtm_rmx.rmx_mtu);
	else
		printf("%6s ", "-");
	putchar((rtm->rtm_rmx.rmx_locks & RTV_MTU) ? 'L' : ' ');
	printf(" %.16s", if_indextoname(rtm->rtm_index, ifbuf));
	putchar('\n');
}

/*
 * Print address family header before a section of the routing table.
 */
void
pr_family(af)
	int af;
{
	char *afname;

	switch (af) {
	case AF_INET:
		afname = "Internet";
		break;
#ifdef INET6
	case AF_INET6:
		afname = "Internet6";
		break;
#endif /* INET6 */
	case AF_NS:
		afname = "XNS";
		break;
	case AF_IPX:
		afname = "IPX";
		break;
	case AF_CCITT:
		afname = "X.25";
		break;
	case PF_KEY:
		afname = "Encap";
		break;
	case AF_APPLETALK:
		afname = "AppleTalk";
		break;
	default:
		afname = NULL;
		break;
	}
	if (afname)
		printf("\n%s:\n", afname);
	else
		printf("\nProtocol Family %d:\n", af);
}

void
p_sockaddr(struct sockaddr *sa, struct sockaddr *mask, int flags, int width)
{
	char *cp;

	switch (sa->sa_family) {
#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sa;
#ifdef __KAME__
		struct in6_addr *in6 = &sa6->sin6_addr;

		/*
		 * XXX: This is a special workaround for KAME kernels.
		 * sin6_scope_id field of SA should be set in the future.
		 */
		if (IN6_IS_ADDR_LINKLOCAL(in6) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(in6)) {
			/* XXX: override is ok? */
			sa6->sin6_scope_id = (u_int32_t)ntohs(*(u_short *)
			    &in6->s6_addr[2]);
			*(u_short *)&in6->s6_addr[2] = 0;
		}
#endif
		if (flags & RTF_HOST)
			cp = routename((struct sockaddr *)sa6);
		else
			cp = netname((struct sockaddr *)sa6, mask);
		break;
	    }
#endif
	default:
	    	if ((flags & RTF_HOST) || mask == NULL)
			cp = routename(sa);
		else
			cp = netname(sa, mask);;
	}
	if (width < 0 )
		printf("%s ", cp);
	else {
		if (nflag)
			printf("%-*s ", width, cp);
		else
			printf("%-*.*s ", width, width, cp);
	}
}

void
p_flags(int f, char *format)
{
	char name[33], *flags;
	const struct bits *p = bits;

	for (flags = name; p->b_mask && flags < &name[sizeof name-2]; p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	printf(format, name);
}

static char line[MAXHOSTNAMELEN];
static char domain[MAXHOSTNAMELEN];

char *
routename(struct sockaddr *sa)
{
	char *cp = NULL;
	static int first = 1;

	if (first) {
		first = 0;
		if (gethostname(domain, sizeof domain) == 0 &&
		    (cp = strchr(domain, '.')))
			(void) strlcpy(domain, cp + 1, sizeof domain);
		else
			domain[0] = 0;
		cp = NULL;
	}

	if (sa->sa_len == 0)
		(void) strlcpy(line, "default", sizeof line);
	else switch (sa->sa_family) {
	case AF_INET:
		return
		    (routename4(((struct sockaddr_in *)sa)->sin_addr.s_addr));

#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 sin6;

		memset(&sin6, 0, sizeof(sin6));
		memcpy(&sin6, sa, sa->sa_len);
		sin6.sin6_len = sizeof(struct sockaddr_in6);
		sin6.sin6_family = AF_INET6;
#ifdef __KAME__
		if (sa->sa_len == sizeof(struct sockaddr_in6) &&
		    (IN6_IS_ADDR_LINKLOCAL(&sin6.sin6_addr) ||
		     IN6_IS_ADDR_MC_LINKLOCAL(&sin6.sin6_addr)) &&
		    sin6.sin6_scope_id == 0) {
			sin6.sin6_scope_id =
			    ntohs(*(u_int16_t *)&sin6.sin6_addr.s6_addr[2]);
			sin6.sin6_addr.s6_addr[2] = 0;
			sin6.sin6_addr.s6_addr[3] = 0;
		}
#endif
		return (routename6(&sin6));
	    }
#endif

	case AF_IPX:
		return (ipx_print(sa));

	case AF_LINK:
		return (link_print(sa));

	case AF_UNSPEC:
		if (sa->sa_len == sizeof(struct sockaddr_rtlabel)) {
			static char name[RTLABEL_LEN];
			struct sockaddr_rtlabel *sr;

			sr = (struct sockaddr_rtlabel *)sa;
			(void) strlcpy(name, sr->sr_label, sizeof name);
			return (name);
		}
		/* FALLTHROUGH */
	default:
		(void) snprintf(line, sizeof line, "(%d) %s",
		    sa->sa_family, any_ntoa(sa));
		break;
	}
	return (line);
}

char *
routename4(in_addr_t in)
{
	char		*cp = NULL;
	struct in_addr	 ina;
	struct hostent	*hp;

	if (in == INADDR_ANY)
		cp = "default";
	if (!cp && !nflag) {
		if ((hp = gethostbyaddr((char *)&in,
		    sizeof (in), AF_INET)) != NULL) {
			if ((cp = strchr(hp->h_name, '.')) &&
			    !strcmp(cp + 1, domain))
				*cp = 0;
			cp = hp->h_name;
		}
	}
	ina.s_addr = in;
	strlcpy(line, cp ? cp : inet_ntoa(ina), sizeof line);

	return (line);
}

#ifdef INET6
char *
routename6(struct sockaddr_in6 *sin6)
{
	int	 niflags;

#ifdef NI_WITHSCOPEID
	niflags = NI_WITHSCOPEID;
#else
	niflags = 0;
#endif
	if (nflag)
		niflags |= NI_NUMERICHOST;
	else
		niflags |= NI_NOFQDN;

	if (getnameinfo((struct sockaddr *)sin6, sin6->sin6_len,
	    line, sizeof(line), NULL, 0, niflags) != 0)
		strncpy(line, "invalid", sizeof(line));

	return (line);
}
#endif

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname4(in_addr_t in, struct sockaddr_in *maskp)
{
	char *cp = NULL;
	struct netent *np = NULL;
	in_addr_t mask;
	int mbits;

	in = ntohl(in);
	mask = maskp ? ntohl(maskp->sin_addr.s_addr) : 0;
	if (!nflag && in != INADDR_ANY) {
		if ((np = getnetbyaddr(in, AF_INET)) != NULL)
			cp = np->n_name;
	}
	if (in == INADDR_ANY)
		cp = "default";
	mbits = mask ? 33 - ffs(mask) : 0;
	if (cp)
		strlcpy(line, cp, sizeof(line));
#define C(x)	((x) & 0xff)
	else if (mbits < 9)
		snprintf(line, sizeof line, "%u/%d", C(in >> 24), mbits);
	else if (mbits < 17)
		snprintf(line, sizeof line, "%u.%u/%d",
		    C(in >> 24) , C(in >> 16), mbits);
	else if (mbits < 25)
		snprintf(line, sizeof line, "%u.%u.%u/%d",
		    C(in >> 24), C(in >> 16), C(in >> 8), mbits);
	else
		snprintf(line, sizeof line, "%u.%u.%u.%u/%d", C(in >> 24),
		    C(in >> 16), C(in >> 8), C(in), mbits);
#undef C
	return (line);
}

#ifdef INET6
char *
netname6(struct sockaddr_in6 *sa6, struct sockaddr_in6 *mask)
{
	struct sockaddr_in6 sin6;
	u_char *p;
	int masklen, final = 0, illegal = 0;
	int i, lim;
	char hbuf[NI_MAXHOST];
#ifdef NI_WITHSCOPEID
	int flag = NI_WITHSCOPEID;
#else
	int flag = 0;
#endif
	int error;

	sin6 = *sa6;

	masklen = 0;
	if (mask) {
		lim = mask->sin6_len - offsetof(struct sockaddr_in6, sin6_addr);
		lim = lim < sizeof(struct in6_addr) ?
		    lim : sizeof(struct in6_addr);
		for (p = (u_char *)&mask->sin6_addr, i = 0; i < lim; p++) {
			if (final && *p) {
				illegal++;
				sin6.sin6_addr.s6_addr[i++] = 0x00;
				continue;
			}

			switch (*p & 0xff) {
			case 0xff:
				masklen += 8;
				break;
			case 0xfe:
				masklen += 7;
				final++;
				break;
			case 0xfc:
				masklen += 6;
				final++;
				break;
			case 0xf8:
				masklen += 5;
				final++;
				break;
			case 0xf0:
				masklen += 4;
				final++;
				break;
			case 0xe0:
				masklen += 3;
				final++;
				break;
			case 0xc0:
				masklen += 2;
				final++;
				break;
			case 0x80:
				masklen += 1;
				final++;
				break;
			case 0x00:
				final++;
				break;
			default:
				final++;
				illegal++;
				break;
			}

			if (!illegal)
				sin6.sin6_addr.s6_addr[i++] &= *p;
			else
				sin6.sin6_addr.s6_addr[i++] = 0x00;
		}
		while (i < sizeof(struct in6_addr))
			sin6.sin6_addr.s6_addr[i++] = 0x00;
	} else
		masklen = 128;

	if (masklen == 0 && IN6_IS_ADDR_UNSPECIFIED(&sin6.sin6_addr))
		return("default");

	if (illegal)
		fprintf(stderr, "illegal prefixlen\n");

	if (nflag)
		flag |= NI_NUMERICHOST;
	error = getnameinfo((struct sockaddr *)&sin6, sin6.sin6_len,
	    hbuf, sizeof(hbuf), NULL, 0, flag);
	if (error)
		snprintf(hbuf, sizeof(hbuf), "invalid");

	snprintf(line, sizeof(line), "%s/%d", hbuf, masklen);
	return line;
}
#endif

/*
 * Return the name of the network whose address is given.
 * The address is assumed to be that of a net or subnet, not a host.
 */
char *
netname(struct sockaddr *sa, struct sockaddr *mask)
{
	switch (sa->sa_family) {

	case AF_INET:
		return netname4(((struct sockaddr_in *)sa)->sin_addr.s_addr,
		    (struct sockaddr_in *)mask);
#ifdef INET6
	case AF_INET6:
		return netname6((struct sockaddr_in6 *)sa,
		    (struct sockaddr_in6 *)mask);
#endif

	case AF_IPX:
		return (ipx_print(sa));

	case AF_LINK:
		return (link_print(sa));

	default:
		snprintf(line, sizeof line, "af %d: %s",
		    sa->sa_family, any_ntoa(sa));
		break;
	}
	return (line);
}

static const char hexlist[] = "0123456789abcdef";

char *
any_ntoa(const struct sockaddr *sa)
{
	static char obuf[240];
	const char *in = sa->sa_data;
	char *out = obuf;
	int len = sa->sa_len - offsetof(struct sockaddr, sa_data);

	*out++ = 'Q';
	do {
		*out++ = hexlist[(*in >> 4) & 15];
		*out++ = hexlist[(*in++)    & 15];
		*out++ = '.';
	} while (--len > 0 && (out + 3) < &obuf[sizeof obuf-1]);
	out[-1] = '\0';
	return (obuf);
}

short ipx_nullh[] = {0,0,0};
short ipx_bh[] = {-1,-1,-1};

char *
ipx_print(struct sockaddr *sa)
{
	struct sockaddr_ipx *sipx = (struct sockaddr_ipx *)sa;
	struct ipx_addr work;
	union { union ipx_net net_e; u_int32_t long_e; } net;
	u_short port;
	static char mybuf[50+MAXHOSTNAMELEN], cport[10], chost[25];
	char *host = "";
	char *p;
	u_char *q;

	work = sipx->sipx_addr;
	port = ntohs(work.ipx_port);
	work.ipx_port = 0;
	net.net_e  = work.ipx_net;
	if (ipx_nullhost(work) && net.long_e == 0) {
		if (!port)
			return ("*.*");
		(void) snprintf(mybuf, sizeof mybuf, "*.0x%XH", port);
		return (mybuf);
	}

	if (memcmp(ipx_bh, work.ipx_host.c_host, 6) == 0)
		host = "any";
	else if (memcmp(ipx_nullh, work.ipx_host.c_host, 6) == 0)
		host = "*";
	else {
		q = work.ipx_host.c_host;
		(void) snprintf(chost, sizeof chost, "%02X%02X%02X%02X%02X%02XH",
			q[0], q[1], q[2], q[3], q[4], q[5]);
		for (p = chost; *p == '0' && p < chost + 12; p++)
			/* void */;
		host = p;
	}
	if (port)
		(void) snprintf(cport, sizeof cport, ".%XH", htons(port));
	else
		*cport = 0;

	(void) snprintf(mybuf, sizeof mybuf, "%XH.%s%s",
	    ntohl(net.long_e), host, cport);
	return (mybuf);
}

char *
link_print(struct sockaddr *sa)
{
	struct sockaddr_dl	*sdl = (struct sockaddr_dl *)sa;
	u_char			*lla = (u_char *)sdl->sdl_data + sdl->sdl_nlen;

	if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 &&
	    sdl->sdl_slen == 0) {
		(void) snprintf(line, sizeof line,
				"link#%d", sdl->sdl_index);
		return (line);
	} else switch (sdl->sdl_type) {
	case IFT_ETHER:
		return (ether_ntoa((struct ether_addr *)lla));
	default:
		break;
	}
	return (link_ntoa(sdl));
}

