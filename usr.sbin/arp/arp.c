/*	$OpenBSD: arp.c,v 1.31 2005/01/04 10:57:23 pascoe Exp $ */
/*	$NetBSD: arp.c,v 1.12 1995/04/24 13:25:18 cgd Exp $ */

/*
 * Copyright (c) 1984, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Sun Microsystems, Inc.
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
static char copyright[] =
"@(#) Copyright (c) 1984, 1993\n\
	The Regents of the University of California.  All rights reserved.\n";
#endif /* not lint */

#ifndef lint
/*static char sccsid[] = "from: @(#)arp.c	8.2 (Berkeley) 1/2/94";*/
static char *rcsid = "$OpenBSD: arp.c,v 1.31 2005/01/04 10:57:23 pascoe Exp $";
#endif /* not lint */

/*
 * arp - display, set, and delete arp table entries
 */

#include <sys/param.h>
#include <sys/file.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <netdb.h>
#include <errno.h>
#include <err.h>
#include <nlist.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <paths.h>
#include <unistd.h>

int delete(const char *, const char *);
void search(in_addr_t addr, void (*action)(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm));
void print_entry(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm);
void nuke_entry(struct sockaddr_dl *sdl,
	struct sockaddr_inarp *sin, struct rt_msghdr *rtm);
void ether_print(const char *);
int file(char *);
int get(const char *);
int getinetaddr(const char *, struct in_addr *);
void getsocket(void);
int rtmsg(int);
int set(int, char **);
void usage(void);

static pid_t pid;
static int nflag;	/* no reverse dns lookups */
static int aflag;	/* do it for all entries */
static int s = -1;

extern int h_errno;

/* ROUNDUP() is nasty, but it is identical to what's in the kernel. */
#define ROUNDUP(a)					\
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

/* which function we're supposed to do */
#define F_GET		1
#define F_SET		2
#define F_FILESET	3
#define F_DELETE	4

int
main(int argc, char *argv[])
{
	int ch, func, rtn;

	pid = getpid();
	opterr = 0;
	func = 0;

	while ((ch = getopt(argc, argv, "andsf")) != -1) {
		switch ((char)ch) {
		case 'a':
			aflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'd':
			if (func)
				usage();
			func = F_DELETE;
			break;
		case 's':
			if (func)
				usage();
			func = F_SET;
			break;
		case 'f':
			if (func)
				usage();
			func = F_FILESET;
			break;
		default:
			usage();
			break;
		}
	}
	argc -= optind;
	argv += optind;

	if (!func)
		func = F_GET;
	rtn = 0;

	switch (func) {
	case F_GET:
		if (aflag && argc == 0)
			search(0, print_entry);
		else if (!aflag && argc == 1)
			rtn = get(argv[0]);
		else
			usage();
		break;
	case F_SET:
		if (argc < 2 || argc > 5)
			usage();
		rtn = set(argc, argv) ? 1 : 0;
		break;
	case F_DELETE:
		if (aflag && argc == 0)
			search(0, nuke_entry);
		else if (!aflag && argc == 1)
			rtn = delete(argv[0], argv[1]);
		else
			usage();
		break;
	case F_FILESET:
		if (argc != 1)
			usage();
		rtn = file(argv[0]);
		break;
	}
	return (rtn);
}

/*
 * Process a file to set standard arp entries
 */
int
file(char *name)
{
	char line[100], arg[5][50], *args[5];
	int i, retval;
	FILE *fp;

	if ((fp = fopen(name, "r")) == NULL)
		err(1, "cannot open %s", name);
	args[0] = &arg[0][0];
	args[1] = &arg[1][0];
	args[2] = &arg[2][0];
	args[3] = &arg[3][0];
	args[4] = &arg[4][0];
	retval = 0;
	while (fgets(line, 100, fp) != NULL) {
		i = sscanf(line, "%49s %49s %49s %49s %49s", arg[0], arg[1], arg[2],
		    arg[3], arg[4]);
		if (i < 2) {
			warnx("bad line: %s", line);
			retval = 1;
			continue;
		}
		if (set(i, args))
			retval = 1;
	}
	fclose(fp);
	return (retval);
}

void
getsocket(void)
{
	if (s >= 0)
		return;
	s = socket(PF_ROUTE, SOCK_RAW, 0);
	if (s < 0)
		err(1, "socket");
}

struct	sockaddr_in so_mask = {8, 0, 0, { 0xffffffff}};
struct	sockaddr_inarp blank_sin = {sizeof(blank_sin), AF_INET }, sin_m;
struct	sockaddr_dl blank_sdl = {sizeof(blank_sdl), AF_LINK }, sdl_m;
int	expire_time, flags, export_only, doing_proxy, found_entry;
struct	{
	struct	rt_msghdr m_rtm;
	char	m_space[512];
}	m_rtmsg;

/*
 * Set an individual arp entry
 */
int
set(int argc, char *argv[])
{
	struct sockaddr_inarp *sin;
	struct sockaddr_dl *sdl;
	struct rt_msghdr *rtm;
	char *eaddr;
	struct ether_addr *ea;
	char *host = argv[0];

	sin = &sin_m;
	rtm = &(m_rtmsg.m_rtm);
	eaddr = argv[1];

	getsocket();
	argc -= 2;
	argv += 2;
	sdl_m = blank_sdl;		/* struct copy */
	sin_m = blank_sin;		/* struct copy */
	if (getinetaddr(host, &sin->sin_addr) == -1)
		return (1);
	ea = ether_aton(eaddr);
	if (ea == NULL) 
		errx(1, "invalid ethernet address: %s", eaddr);
	memcpy(LLADDR(&sdl_m), ea, sizeof(*ea));
	sdl_m.sdl_alen = 6;
	doing_proxy = flags = export_only = expire_time = 0;
	while (argc-- > 0) {
		if (strncmp(argv[0], "temp", 4) == 0) {
			struct timeval time;
			(void)gettimeofday(&time, 0);
			expire_time = time.tv_sec + 20 * 60;
			if (flags & RTF_PERMANENT_ARP) {
				/* temp or permanent, not both */
				usage();
				return (0);
			}
		}
		else if (strncmp(argv[0], "pub", 3) == 0) {
			flags |= RTF_ANNOUNCE;
			doing_proxy = SIN_PROXY;
		}
		else if (strncmp(argv[0], "permanent", 9) == 0) {
			flags |= RTF_PERMANENT_ARP;
			if (expire_time != 0) {
				/* temp or permanent, not both */
				usage();
				return (0);
			}
		} else if (strncmp(argv[0], "trail", 5) == 0) {
			(void)printf(
			    "%s: Sending trailers is no longer supported\n",
			     host);
		}
		argv++;
	}
tryagain:
	if (rtmsg(RTM_GET) < 0) {
		warn("%s", host);
		return (1);
	}
	sin = (struct sockaddr_inarp *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin_len) + (char *)sin);
	if (sin->sin_addr.s_addr == sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) {
			switch (sdl->sdl_type) {
			case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
			case IFT_ISO88024: case IFT_ISO88025: case IFT_CARP:
				goto overwrite;
			}
		}
		if (doing_proxy == 0) {
			(void)printf("set: can only proxy for %s\n", host);
			return (1);
		}
		if (sin_m.sin_other & SIN_PROXY) {
			(void)printf(
			    "set: proxy entry exists for non 802 device\n");
			return (1);
		}
		sin_m.sin_other = SIN_PROXY;
		export_only = 1;
		goto tryagain;
	}
overwrite:
	if (sdl->sdl_family != AF_LINK) {
		(void)printf("cannot intuit interface index and type for %s\n",
		    host);
		return (1);
	}
	sdl_m.sdl_type = sdl->sdl_type;
	sdl_m.sdl_index = sdl->sdl_index;
	return (rtmsg(RTM_ADD));
}

/*
 * Display an individual arp entry
 */
int
get(const char *host)
{
	struct sockaddr_inarp *sin;

	sin = &sin_m;
	sin_m = blank_sin;		/* struct copy */
	if (getinetaddr(host, &sin->sin_addr) == -1)
		exit(1);
	search(sin->sin_addr.s_addr, print_entry);
	if (found_entry == 0) {
		(void)printf("%s (%s) -- no entry\n", host,
		    inet_ntoa(sin->sin_addr));
		return (1);
	}
	return (0);
}

/*
 * Delete an arp entry
 */
int
delete(const char *host, const char *info)
{
	struct sockaddr_inarp *sin;
	struct rt_msghdr *rtm;
	struct sockaddr_dl *sdl;

	sin = &sin_m;
	rtm = &m_rtmsg.m_rtm;

	if (info && strncmp(info, "pro", 3) )
		export_only = 1;
	getsocket();
	sin_m = blank_sin;		/* struct copy */
	if (getinetaddr(host, &sin->sin_addr) == -1)
		return (1);
tryagain:
	if (rtmsg(RTM_GET) < 0) {
		warn("%s", host);
		return (1);
	}
	sin = (struct sockaddr_inarp *)(rtm + 1);
	sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin_len) + (char *)sin);
	if (sin->sin_addr.s_addr == sin_m.sin_addr.s_addr) {
		if (sdl->sdl_family == AF_LINK &&
		    (rtm->rtm_flags & RTF_LLINFO) &&
		    !(rtm->rtm_flags & RTF_GATEWAY)) {
			switch (sdl->sdl_type) {
			case IFT_ETHER: case IFT_FDDI: case IFT_ISO88023:
			case IFT_ISO88024: case IFT_ISO88025: case IFT_CARP:
				goto delete;
			}
		}
	}
	if (sin_m.sin_other & SIN_PROXY) {
		warnx("delete: can't locate %s", host);
		return (1);
	} else {
		sin_m.sin_other = SIN_PROXY;
		goto tryagain;
	}
delete:
	if (sdl->sdl_family != AF_LINK) {
		(void)printf("cannot locate %s\n", host);
		return (1);
	}
	if (rtmsg(RTM_DELETE))
		return (1);
	(void)printf("%s (%s) deleted\n", host, inet_ntoa(sin->sin_addr));
	return (0);
}

/*
 * Search the entire arp table, and do some action on matching entries.
 */
void
search(in_addr_t addr, void (*action)(struct sockaddr_dl *sdl,
    struct sockaddr_inarp *sin, struct rt_msghdr *rtm))
{
	int mib[6];
	size_t needed;
	char *lim, *buf, *next;
	struct rt_msghdr *rtm;
	struct sockaddr_inarp *sin;
	struct sockaddr_dl *sdl;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_LLINFO;
	if (sysctl(mib, 6, NULL, &needed, NULL, 0) < 0)
		err(1, "route-sysctl-estimate");
	if (needed == 0)
		return;
	if ((buf = malloc(needed)) == NULL)
		err(1, "malloc");
	if (sysctl(mib, 6, buf, &needed, NULL, 0) < 0)
		err(1, "actual retrieval of routing table");
	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		sin = (struct sockaddr_inarp *)(rtm + 1);
		sdl = (struct sockaddr_dl *)(sin + 1);
		if (addr) {
			if (addr != sin->sin_addr.s_addr)
				continue;
			found_entry = 1;
		}
		(*action)(sdl, sin, rtm);
	}
	free(buf);
}

/*
 * Display an arp entry
 */
void
print_entry(struct sockaddr_dl *sdl, struct sockaddr_inarp *sin,
    struct rt_msghdr *rtm)
{
	char *host;
	struct hostent *hp;
	char ifname[IF_NAMESIZE];

	if (nflag == 0)
		hp = gethostbyaddr((caddr_t)&(sin->sin_addr),
		    sizeof(sin->sin_addr), AF_INET);
	else
		hp = 0;
	if (hp)
		host = hp->h_name;
	else {
		host = "?";
		if (h_errno == TRY_AGAIN)
			nflag = 1;
	}
	(void)printf("%s (%s) at ", host, inet_ntoa(sin->sin_addr));
	if (sdl->sdl_alen)
		ether_print(LLADDR(sdl));
	else
		(void)printf("(incomplete)");
	if (if_indextoname(sdl->sdl_index, ifname) != NULL)
		printf(" on %s", ifname);
	if (rtm->rtm_flags & RTF_PERMANENT_ARP)
		(void)printf(" permanent");
	if (rtm->rtm_rmx.rmx_expire == 0)
		(void)printf(" static");
	if (sin->sin_other & SIN_PROXY)
		(void)printf(" published (proxy only)");
	if (rtm->rtm_addrs & RTA_NETMASK) {
		sin = (struct sockaddr_inarp *)
			(ROUNDUP(sdl->sdl_len) + (char *)sdl);
		if (sin->sin_addr.s_addr == 0xffffffff)
			(void)printf(" published");
		if (sin->sin_len != 8)
			(void)printf("(weird %d)", sin->sin_len);
	}
	printf("\n");
}

/*
 * Nuke an arp entry
 */
void
nuke_entry(struct sockaddr_dl *sdl, struct sockaddr_inarp *sin,
    struct rt_msghdr *rtm)
{
	char ip[20];

	strlcpy(ip, inet_ntoa(sin->sin_addr), sizeof(ip));
	delete(ip, NULL);
}

void
ether_print(const char *scp)
{
	const u_char *cp = (u_char *)scp;

	(void)printf("%02x:%02x:%02x:%02x:%02x:%02x",
	    cp[0], cp[1], cp[2], cp[3], cp[4], cp[5]);
}

void
usage(void)
{
	(void)fprintf(stderr, "usage: arp [-n] hostname\n");
	(void)fprintf(stderr, "usage: arp [-n] -a\n");
	(void)fprintf(stderr, "usage: arp -d hostname\n");
	(void)fprintf(stderr, "usage: arp -d -a\n");
	(void)fprintf(stderr,
	    "usage: arp -s hostname ether_addr [temp | permanent] [pub]\n");
	(void)fprintf(stderr, "usage: arp -f filename\n");
	exit(1);
}

int
rtmsg(int cmd)
{
	static int seq;
	struct rt_msghdr *rtm;
	char *cp;
	int l;

	rtm = &m_rtmsg.m_rtm;
	cp = m_rtmsg.m_space;
	errno = 0;

	if (cmd == RTM_DELETE)
		goto doit;
	(void)memset(&m_rtmsg, 0, sizeof(m_rtmsg));
	rtm->rtm_flags = flags;
	rtm->rtm_version = RTM_VERSION;

	switch (cmd) {
	default:
		errx(1, "internal wrong cmd");
		/*NOTREACHED*/
	case RTM_ADD:
		rtm->rtm_addrs |= RTA_GATEWAY;
		rtm->rtm_rmx.rmx_expire = expire_time;
		rtm->rtm_inits = RTV_EXPIRE;
		rtm->rtm_flags |= (RTF_HOST | RTF_STATIC);
		sin_m.sin_other = 0;
		if (doing_proxy) {
			if (export_only)
				sin_m.sin_other = SIN_PROXY;
			else {
				rtm->rtm_addrs |= RTA_NETMASK;
				rtm->rtm_flags &= ~RTF_HOST;
			}
		}
		/* FALLTHROUGH */
	case RTM_GET:
		rtm->rtm_addrs |= RTA_DST;
	}

#define NEXTADDR(w, s)					\
	if (rtm->rtm_addrs & (w)) {			\
		(void)memcpy(cp, &s, sizeof(s));	\
		cp += ROUNDUP(sizeof(s));		\
	}

	NEXTADDR(RTA_DST, sin_m);
	NEXTADDR(RTA_GATEWAY, sdl_m);
	NEXTADDR(RTA_NETMASK, so_mask);

	rtm->rtm_msglen = cp - (char *)&m_rtmsg;
doit:
	l = rtm->rtm_msglen;
	rtm->rtm_seq = ++seq;
	rtm->rtm_type = cmd;
	if (write(s, (char *)&m_rtmsg, l) < 0) {
		if (errno != ESRCH || cmd != RTM_DELETE) {
			warn("writing to routing socket");
			return (-1);
		}
	}
	do {
		l = read(s, (char *)&m_rtmsg, sizeof(m_rtmsg));
	} while (l > 0 && (rtm->rtm_seq != seq || rtm->rtm_pid != pid));
	if (l < 0)
		warn("read from routing socket");
	return (0);
}

int
getinetaddr(const char *host, struct in_addr *inap)
{
	struct hostent *hp;

	if (inet_aton(host, inap) == 1)
		return (0);
	if ((hp = gethostbyname(host)) == NULL) {
		warnx("%s: %s", host, hstrerror(h_errno));
		return (-1);
	}
	(void)memcpy(inap, hp->h_addr, sizeof(*inap));
	return (0);
}
