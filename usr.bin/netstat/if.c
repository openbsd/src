/*	$OpenBSD: if.c,v 1.72 2015/02/09 12:25:03 claudio Exp $	*/
/*	$NetBSD: if.c,v 1.16.4.2 1996/06/07 21:46:46 thorpej Exp $	*/

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

#include <sys/param.h>	/* roundup() */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>

#include <err.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "netstat.h"

static void print_addr(struct sockaddr *, struct sockaddr **, struct if_data *);
static void sidewaysintpr(u_int, int);
static void catchalarm(int);
static void get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
static void fetchifs(void);

/*
 * Print a description of the network interfaces.
 * NOTE: ifnetaddr is the location of the kernel global "ifnet",
 * which is a TAILQ_HEAD.
 */
void
intpr(int interval, int repeatcount)
{
	struct if_msghdr ifm;
	int mib[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
	char name[IFNAMSIZ + 1];	/* + 1 for the '*' */
	char *buf = NULL, *next, *lim, *cp;
	struct rt_msghdr *rtm;
	struct ifa_msghdr *ifam;
	struct if_data *ifd;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	struct sockaddr_dl *sdl;
	u_int64_t total = 0;
	size_t len;

	if (interval) {
		sidewaysintpr((unsigned)interval, repeatcount);
		return;
	}

	len = get_sysctl(mib, 6, &buf);

	printf("%-7.7s %-5.5s %-11.11s %-17.17s ",
	    "Name", "Mtu", "Network", "Address");
	if (bflag)
		printf("%10.10s %10.10s", "Ibytes", "Obytes");
	else
		printf("%8.8s %5.5s %8.8s %5.5s %5.5s",
		    "Ipkts", "Ierrs", "Opkts", "Oerrs", "Colls");
	if (tflag)
		printf(" %s", "Time");
	if (dflag)
		printf(" %s", "Drop");
	putchar('\n');

	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
		case RTM_IFINFO:
			total = 0;
			bcopy(next, &ifm, sizeof ifm);
			ifd = &ifm.ifm_data;

			sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
			get_rtaddrs(ifm.ifm_addrs, sa, rti_info);

			sdl = (struct sockaddr_dl *)rti_info[RTAX_IFP];
			if (sdl == NULL || sdl->sdl_family != AF_LINK)
				continue;
			bzero(name, sizeof(name));
			if (sdl->sdl_nlen >= IFNAMSIZ)
				memcpy(name, sdl->sdl_data, IFNAMSIZ - 1);
			else if (sdl->sdl_nlen > 0) 
				memcpy(name, sdl->sdl_data, sdl->sdl_nlen);

			if (interface != 0 && strcmp(name, interface) != 0)
				continue;

			/* mark inactive interfaces with a '*' */
			cp = strchr(name, '\0');
			if ((ifm.ifm_flags & IFF_UP) == 0)
				*cp++ = '*';
			*cp = '\0';

			if (qflag) {
				total = ifd->ifi_ibytes + ifd->ifi_obytes +
				    ifd->ifi_ipackets + ifd->ifi_ierrors +
				    ifd->ifi_opackets + ifd->ifi_oerrors +
				    ifd->ifi_collisions;
				if (tflag)
					total += 0; // XXX ifnet.if_timer;
				if (dflag)
					total += 0; // XXX ifnet.if_snd.ifq_drops;
				if (total == 0)
					continue;
			}

			printf("%-7s %-5d ", name, ifd->ifi_mtu);
			print_addr(rti_info[RTAX_IFP], rti_info, ifd);
			break;
		case RTM_NEWADDR:
			if (qflag && total == 0)
				continue;
			if (interface != 0 && strcmp(name, interface) != 0)
				continue;

			ifam = (struct ifa_msghdr *)next;
			if ((ifam->ifam_addrs & (RTA_NETMASK | RTA_IFA |
			    RTA_BRD)) == 0)
				break;

			sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
			get_rtaddrs(ifam->ifam_addrs, sa, rti_info);

			printf("%-7s %-5d ", name, ifd->ifi_mtu);
			print_addr(rti_info[RTAX_IFA], rti_info, ifd);
			break;
		}
	}
	free(buf);
}

static void
print_addr(struct sockaddr *sa, struct sockaddr **rtinfo, struct if_data *ifd)
{
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sin;
	struct sockaddr_in6 *sin6;
	char *cp;
	int m, n;

	switch (sa->sa_family) {
	case AF_UNSPEC:
		printf("%-11.11s ", "none");
		printf("%-17.17s ", "none");
		break;
	case AF_INET:
		sin = (struct sockaddr_in *)sa;
		cp = netname4(sin->sin_addr.s_addr,
		    ((struct sockaddr_in *)rtinfo[RTAX_NETMASK])->sin_addr.s_addr);
		if (vflag)
			n = strlen(cp) < 11 ? 11 : strlen(cp);
		else
			n = 11;
		printf("%-*.*s ", n, n, cp);
		cp = routename4(sin->sin_addr.s_addr);
		if (vflag)
			n = strlen(cp) < 17 ? 17 : strlen(cp);
		else
			n = 17;
		printf("%-*.*s ", n, n, cp);

		break;
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)sa;
#ifdef __KAME__
		if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
			sin6->sin6_scope_id =
			    ntohs(*(u_int16_t *)
			    &sin6->sin6_addr.s6_addr[2]);
			sin6->sin6_addr.s6_addr[2] = 0;
			sin6->sin6_addr.s6_addr[3] = 0;
		}
#endif
		cp = netname6(sin6,
		    (struct sockaddr_in6 *)rtinfo[RTAX_NETMASK]);
		if (vflag)
			n = strlen(cp) < 11 ? 11 : strlen(cp);
		else
			n = 11;
		printf("%-*.*s ", n, n, cp);
		cp = routename6(sin6);
		if (vflag)
			n = strlen(cp) < 17 ? 17 : strlen(cp);
		else
			n = 17;
		printf("%-*.*s ", n, n, cp);
		break;
	case AF_LINK:
		sdl = (struct sockaddr_dl *)sa;
		m = printf("%-11.11s ", "<Link>");
		if (sdl->sdl_type == IFT_ETHER ||
		    sdl->sdl_type == IFT_CARP ||
		    sdl->sdl_type == IFT_FDDI ||
		    sdl->sdl_type == IFT_ISO88025)
			printf("%-17.17s ",
			    ether_ntoa((struct ether_addr *)LLADDR(sdl)));
		else {
			cp = (char *)LLADDR(sdl);
			n = sdl->sdl_alen;
			goto hexprint;
		}
		break;
	default:
		m = printf("(%d)", sa->sa_family);
		for (cp = sa->sa_len + (char *)sa;
			--cp > sa->sa_data && (*cp == 0);) {}
		n = cp - sa->sa_data + 1;
		cp = sa->sa_data;
hexprint:
		while (--n >= 0)
			m += printf("%x%c", *cp++ & 0xff,
				    n > 0 ? '.' : ' ');
		m = 30 - m;
		while (m-- > 0)
			putchar(' ');
		break;
	}
	if (bflag) {
		if (hflag) {
			char ibytes[FMT_SCALED_STRSIZE];
			char obytes[FMT_SCALED_STRSIZE];
			fmt_scaled(ifd->ifi_ibytes, ibytes);
			fmt_scaled(ifd->ifi_obytes, obytes);
			printf("%10s %10s", ibytes, obytes);
		} else
			printf("%10llu %10llu",
			    ifd->ifi_ibytes, ifd->ifi_obytes);
	} else
		printf("%8llu %5llu %8llu %5llu %5llu",
		    ifd->ifi_ipackets, ifd->ifi_ierrors,
		    ifd->ifi_opackets, ifd->ifi_oerrors,
		    ifd->ifi_collisions);
	if (tflag)
		printf(" %4d", 0 /* XXX ifnet.if_timer */);
	if (dflag)
		printf(" %4d", 0 /* XXX ifnet.if_snd.ifq_drops */);
	putchar('\n');
}

struct	iftot {
	char	ift_name[IFNAMSIZ];	/* interface name */
	u_int64_t ift_ip;		/* input packets */
	u_int64_t ift_ib;		/* input bytes */
	u_int64_t ift_ie;		/* input errors */
	u_int64_t ift_op;		/* output packets */
	u_int64_t ift_ob;		/* output bytes */
	u_int64_t ift_oe;		/* output errors */
	u_int64_t ift_co;		/* collisions */
	u_int64_t ift_dr;		/* drops */
} ip_cur, ip_old, sum_cur, sum_old;

volatile sig_atomic_t signalled;	/* set if alarm goes off "early" */

/*
 * Print a running summary of interface statistics.
 * Repeat display every interval seconds, showing statistics
 * collected over that interval.  Assumes that interval is non-zero.
 * First line printed at top of screen is always cumulative.
 */
static void
sidewaysintpr(unsigned int interval, int repeatcount)
{
	sigset_t emptyset;
	int line;
	char ibytes[FMT_SCALED_STRSIZE];
	char obytes[FMT_SCALED_STRSIZE];

	fetchifs();
	if (ip_cur.ift_name[0] == '\0') {
		fprintf(stderr, "%s: %s: unknown interface\n",
		    __progname, interface);
		exit(1);
	}

	(void)signal(SIGALRM, catchalarm);
	signalled = 0;
	(void)alarm(interval);
banner:
	if (bflag)
		printf("%7.7s in %8.8s %6.6s out %5.5s",
		    ip_cur.ift_name, " ",
		    ip_cur.ift_name, " ");
	else
		printf("%5.5s in %5.5s%5.5s out %5.5s %5.5s",
		    ip_cur.ift_name, " ",
		    ip_cur.ift_name, " ", " ");
	if (dflag)
		printf(" %5.5s", " ");

	if (bflag)
		printf("  %7.7s in %8.8s %6.6s out %5.5s",
		    "total", " ", "total", " ");
	else
		printf("  %5.5s in %5.5s%5.5s out %5.5s %5.5s",
		    "total", " ", "total", " ", " ");
	if (dflag)
		printf(" %5.5s", " ");
	putchar('\n');
	if (bflag)
		printf("%10.10s %8.8s %10.10s %5.5s",
		    "bytes", " ", "bytes", " ");
	else
		printf("%8.8s %5.5s %8.8s %5.5s %5.5s",
		    "packets", "errs", "packets", "errs", "colls");
	if (dflag)
		printf(" %5.5s", "drops");

	if (bflag)
		printf("%10.10s %8.8s %10.10s %5.5s",
		    "bytes", " ", "bytes", " ");
	else
		printf("  %8.8s %5.5s %8.8s %5.5s %5.5s",
		    "packets", "errs", "packets", "errs", "colls");
	if (dflag)
		printf(" %5.5s", "drops");
	putchar('\n');
	fflush(stdout);
	line = 0;
	bzero(&ip_old, sizeof(ip_old));
	bzero(&sum_old, sizeof(sum_old));
loop:
	bzero(&sum_cur, sizeof(sum_cur));

	fetchifs();

	if (bflag) {
		if (hflag) {
			fmt_scaled(ip_cur.ift_ib - ip_old.ift_ib, ibytes);
			fmt_scaled(ip_cur.ift_ob - ip_old.ift_ob, obytes);
			printf("%10s %8.8s %10s %5.5s",
			    ibytes, " ", obytes, " ");
		} else
			printf("%10llu %8.8s %10llu %5.5s",
			    ip_cur.ift_ib - ip_old.ift_ib, " ",
			    ip_cur.ift_ob - ip_old.ift_ob, " ");
	} else
		printf("%8llu %5llu %8llu %5llu %5llu",
		    ip_cur.ift_ip - ip_old.ift_ip,
		    ip_cur.ift_ie - ip_old.ift_ie,
		    ip_cur.ift_op - ip_old.ift_op,
		    ip_cur.ift_oe - ip_old.ift_oe,
		    ip_cur.ift_co - ip_old.ift_co);
	if (dflag)
		printf(" %5llu",
		    /* XXX ifnet.if_snd.ifq_drops - ip->ift_dr); */
		    0LL);

	ip_old = ip_cur;

	if (bflag) {
		if (hflag) {
			fmt_scaled(sum_cur.ift_ib - sum_old.ift_ib, ibytes);
			fmt_scaled(sum_cur.ift_ob - sum_old.ift_ob, obytes);
			printf("  %10s %8.8s %10s %5.5s",
			    ibytes, " ", obytes, " ");
		} else
			printf("  %10llu %8.8s %10llu %5.5s",
			    sum_cur.ift_ib - sum_old.ift_ib, " ",
			    sum_cur.ift_ob - sum_old.ift_ob, " ");
	} else
		printf("  %8llu %5llu %8llu %5llu %5llu",
		    sum_cur.ift_ip - sum_old.ift_ip,
		    sum_cur.ift_ie - sum_old.ift_ie,
		    sum_cur.ift_op - sum_old.ift_op,
		    sum_cur.ift_oe - sum_old.ift_oe,
		    sum_cur.ift_co - sum_old.ift_co);
	if (dflag)
		printf(" %5llu", sum_cur.ift_dr - sum_old.ift_dr);

	sum_old = sum_cur;

	putchar('\n');
	fflush(stdout);
	if (repeatcount && --repeatcount == 0)
		return;
	line++;
	sigemptyset(&emptyset);
	if (!signalled)
		sigsuspend(&emptyset);
	signalled = 0;
	(void)alarm(interval);
	if (line == 21 && isatty(STDOUT_FILENO))
		goto banner;
	goto loop;
}

/*
 * Called if an interval expires before sidewaysintpr has completed a loop.
 * Sets a flag to not wait for the alarm.
 */
/* ARGSUSED */
static void
catchalarm(int signo)
{
	signalled = 1;
}

static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{   
	int i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    roundup(sa->sa_len, sizeof(long)));
		} else 
			rti_info[i] = NULL;
	}
}


static int
isegress(char *name)
{
	static int s = -1;
	int len;
	struct ifgroupreq ifgr;
	struct ifg_req *ifg;
	int rv = 0;

	if (s == -1) {
		if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			return 0;
	}

	memset(&ifgr, 0, sizeof(ifgr));
	strlcpy(ifgr.ifgr_name, name, IFNAMSIZ);

	if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1) {
		return 0;
	}

	len = ifgr.ifgr_len;
	ifgr.ifgr_groups = calloc(len, 1);
	if (ifgr.ifgr_groups == NULL)
		err(1, "getifgroups");
	if (ioctl(s, SIOCGIFGROUP, (caddr_t)&ifgr) == -1)
		err(1, "SIOCGIFGROUP");

	ifg = ifgr.ifgr_groups;
	for (; ifg && len >= sizeof(struct ifg_req); ifg++) {
		len -= sizeof(struct ifg_req);
		if (strcmp(ifg->ifgrq_group, IFG_EGRESS) == 0)
			rv = 1;
	}

	free(ifgr.ifgr_groups);
	return rv;
}

static void
fetchifs(void)
{
	struct if_msghdr ifm;
	int mib[6] = { CTL_NET, PF_ROUTE, 0, 0, NET_RT_IFLIST, 0 };
	struct rt_msghdr *rtm;
	struct if_data *ifd;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	struct sockaddr_dl *sdl;
	char *buf, *next, *lim;
	char name[IFNAMSIZ];
	size_t len;
	int takeit = 0;
	int foundone = 0;

	len = get_sysctl(mib, 6, &buf);

	memset(&ip_cur, 0, sizeof(ip_cur));
	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
		case RTM_IFINFO:
			bcopy(next, &ifm, sizeof ifm);
			ifd = &ifm.ifm_data;

			sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
			get_rtaddrs(ifm.ifm_addrs, sa, rti_info);

			sdl = (struct sockaddr_dl *)rti_info[RTAX_IFP];
			if (sdl == NULL || sdl->sdl_family != AF_LINK)
				continue;
			bzero(name, sizeof(name));
			if (sdl->sdl_nlen >= IFNAMSIZ)
				memcpy(name, sdl->sdl_data, IFNAMSIZ - 1);
			else if (sdl->sdl_nlen > 0) 
				memcpy(name, sdl->sdl_data, sdl->sdl_nlen);

			if (interface != NULL && !strcmp(name, interface)) {
				takeit = 1;
			} else if (interface == NULL && foundone == 0 &&
			    isegress(name)) {
				takeit = 1;
				foundone = 1;
			} else
				takeit = 0;
			if (takeit) {
				strlcpy(ip_cur.ift_name, name,
				    sizeof(ip_cur.ift_name));
				ip_cur.ift_ip = ifd->ifi_ipackets;
				ip_cur.ift_ib = ifd->ifi_ibytes;
				ip_cur.ift_ie = ifd->ifi_ierrors;
				ip_cur.ift_op = ifd->ifi_opackets;
				ip_cur.ift_ob = ifd->ifi_obytes;
				ip_cur.ift_oe = ifd->ifi_oerrors;
				ip_cur.ift_co = ifd->ifi_collisions;
				ip_cur.ift_dr = 0;
				    /* XXX ifnet.if_snd.ifq_drops */
			}

			sum_cur.ift_ip += ifd->ifi_ipackets;
			sum_cur.ift_ib += ifd->ifi_ibytes;
			sum_cur.ift_ie += ifd->ifi_ierrors;
			sum_cur.ift_op += ifd->ifi_opackets;
			sum_cur.ift_ob += ifd->ifi_obytes;
			sum_cur.ift_oe += ifd->ifi_oerrors;
			sum_cur.ift_co += ifd->ifi_collisions;
			sum_cur.ift_dr += 0; /* XXX ifnet.if_snd.ifq_drops */
			break;
		}
	}
	if (interface == NULL && foundone == 0) {
		strlcpy(ip_cur.ift_name, name,
		    sizeof(ip_cur.ift_name));
		ip_cur.ift_ip = ifd->ifi_ipackets;
		ip_cur.ift_ib = ifd->ifi_ibytes;
		ip_cur.ift_ie = ifd->ifi_ierrors;
		ip_cur.ift_op = ifd->ifi_opackets;
		ip_cur.ift_ob = ifd->ifi_obytes;
		ip_cur.ift_oe = ifd->ifi_oerrors;
		ip_cur.ift_co = ifd->ifi_collisions;
		ip_cur.ift_dr = 0;
		    /* XXX ifnet.if_snd.ifq_drops */
	}
	free(buf);
}
