/*	$OpenBSD: show.c,v 1.27 2004/06/06 17:08:23 cedric Exp $	*/
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
static char *rcsid = "$OpenBSD: show.c,v 1.27 2004/06/06 17:08:23 cedric Exp $";
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
#include <netns/ns.h>
#include <netinet/ip_ipsp.h>
#include <arpa/inet.h>

#include <sys/sysctl.h>

#include "keywords.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* XXX: things from route.c */
extern char *routename(struct sockaddr *);
extern char *netname(struct sockaddr *);
extern char *ns_print(struct sockaddr_ns *);
extern int nflag, Sflag;

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
	{ RTF_SOURCE,	's' },
	{ 0 }
};

static void p_rtentry(struct rt_msghdr *);
static void p_sockaddr(struct sockaddr *, int, int);
static void p_flags(int, char *);
static void pr_rthdr(int);
static void pr_family(int);

int	keyword(char *);
void	usage(char *);
void	show(int argc, char *argv[]);

/*
 * Print routing tables.
 */
void
show(int argc, char *argv[])
{
	struct rt_msghdr *rtm;
	char *buf = NULL, *next, *lim = NULL;
	size_t needed;
	int mib[6], af = 0;
        struct sockaddr *sa;

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
	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
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
			if (af && sa->sa_family != af)
				continue;
			p_rtentry(rtm);
		}
		free(buf);
	}
}

/* column widths; each followed by one space */
#define	WID_DST		16	/* width of destination column */
#define WID_SRC		16	/* width of source column */
#define	WID_GW		18	/* width of gateway column */

/*
 * Print header for routing table columns.
 */
static void
pr_rthdr(int af)
{
	if (af == AF_INET && Sflag) {
		printf("%-*.*s %-*.*s %-*.*s %-6.6s\n",
		    WID_SRC, WID_SRC, "Source",
		    WID_DST, WID_DST, "Destination",
		    WID_GW, WID_GW, "Gateway",
		    "Flags");
	} else {
		printf("%-*.*s %-*.*s %-6.6s\n",
		    WID_DST, WID_DST, "Destination",
		    WID_GW, WID_GW, "Gateway",
		    "Flags");
	}
}

/*
 * Print a routing table entry.
 */
static void
p_rtentry(rtm)
	struct rt_msghdr *rtm;
{
	struct sockaddr *sa = (struct sockaddr *)(rtm + 1), *sa2;
#ifdef notdef
	static int masks_done, banner_printed;
#endif
	static int old_af;
	int i, af = 0, interesting = RTF_UP | RTF_GATEWAY | RTF_HOST |
	    RTF_MASK | RTF_SOURCE;

#ifdef notdef
	/* for the moment, netmasks are skipped over */
	if (!banner_printed) {
		printf("Netmasks:\n");
		banner_printed = 1;
	}
	if (masks_done == 0) {
		if (rtm->rtm_addrs != RTA_DST ) {
			masks_done = 1;
			af = sa->sa_family;
		}
	} else
#endif
		af = sa->sa_family;
	if (old_af != af) {
		old_af = af;
		pr_family(af);
		pr_rthdr(af);
	}
	if (af == AF_INET && Sflag) {
		if (rtm->rtm_addrs & RTA_SRC) {
			for (sa2 = sa, i = 1; i < RTA_SRC; i <<= 1)
				if (rtm->rtm_addrs & i)
					sa2 = (struct sockaddr *)(ROUNDUP(
					    sa2->sa_len) + (char *)sa2);
			p_sockaddr(sa2, 0, 16);
		} else
			printf("%-*s ", 16, "default");
	}
	if (!(rtm->rtm_addrs & RTA_GATEWAY))
		p_sockaddr(sa, 0, 36);
	else {
		p_sockaddr(sa, rtm->rtm_flags, 16);
		sa = (struct sockaddr *)(ROUNDUP(sa->sa_len) + (char *)sa);
		p_sockaddr(sa, 0, 18);
	}
	p_flags(rtm->rtm_flags & interesting, "%-6.6s ");
	putchar('\n');
}

/*
 * Print address family header before a section of the routing table.
 */
static void
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
	case AF_ISO:
		afname = "ISO";
		break;
	case AF_CCITT:
		afname = "X.25";
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


static void
p_sockaddr(sa, flags, width)
	struct sockaddr *sa;
	int flags, width;
{
	char workbuf[128], *cplim;
	char *cp = workbuf;
	int len = sizeof workbuf;

	switch(sa->sa_family) {

	case AF_LINK:
	    {
		struct sockaddr_dl *sdl = (struct sockaddr_dl *)sa;

		if (sdl->sdl_nlen == 0 && sdl->sdl_alen == 0 &&
		    sdl->sdl_slen == 0)
			(void) snprintf(workbuf, sizeof workbuf,
			    "link#%d", sdl->sdl_index);
		else {
			switch (sdl->sdl_type) {
			case IFT_ETHER:
			    {
				int i;
				u_char *lla = (u_char *)sdl->sdl_data +
				    sdl->sdl_nlen;

				cplim = "";
				for (i = 0; i < sdl->sdl_alen; i++, lla++) {
					snprintf(cp, len, "%s%x", cplim, *lla);
					len -= strlen(cp);
					cp += strlen(cp);
					if (len <= 0)
						break;	/* overflow */
					cplim = ":";
				}
				cp = workbuf;
				break;
			    }
			default:
				cp = link_ntoa(sdl);
				break;
			}
		}
		break;
	    }

	case AF_INET:
	    {
		struct sockaddr_in *sin = (struct sockaddr_in *)sa;

		if (sin->sin_addr.s_addr == 0)
			cp = "default";
		else
			cp = (flags & RTF_HOST) ? routename(sa) : netname(sa);
		break;
	    }

#ifdef INET6
	case AF_INET6:
	    {
		struct sockaddr_in6 *sin = (struct sockaddr_in6 *)sa;

		cp = IN6_IS_ADDR_UNSPECIFIED(&sin->sin6_addr) ? "default" :
			((flags & RTF_HOST) ?
			routename(sa) :	netname(sa));
		/* make sure numeric address is not truncated */
		if (strchr(cp, ':') != NULL && strlen(cp) > width)
			width = strlen(cp);
		break;
	    }
#endif /* INET6 */

	case AF_NS:
		cp = ns_print((struct sockaddr_ns *)sa);
		break;

	default:
	    {
		u_char *s = (u_char *)sa->sa_data, *slim;

		slim =  sa->sa_len + (u_char *) sa;
		cplim = cp + sizeof(workbuf) - 6;
		snprintf(cp, len, "(%d)", sa->sa_family);
		len -= strlen(cp);
		cp += strlen(cp);
		if (len <= 0) {
			cp = workbuf;
			break;		/* overflow */
		}
		while (s < slim && cp < cplim) {
			snprintf(cp, len, " %02x", *s++);
			len -= strlen(cp);
			cp += strlen(cp);
			if (len <= 0)
				break;		/* overflow */
			if (s < slim) {
				snprintf(cp, len, "%02x", *s++);
				len -= strlen(cp);
				cp += strlen(cp);
				if (len <= 0)
					break;		/* overflow */
			}
		}
		cp = workbuf;
	    }
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

static void
p_flags(f, format)
	int f;
	char *format;
{
	char name[33], *flags;
	const struct bits *p = bits;

	for (flags = name; p->b_mask && flags < &name[sizeof name-2]; p++)
		if (p->b_mask & f)
			*flags++ = p->b_val;
	*flags = '\0';
	printf(format, name);
}
