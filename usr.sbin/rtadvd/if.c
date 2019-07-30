/*	$OpenBSD: if.c,v 1.46 2017/08/12 07:38:26 florian Exp $	*/
/*	$KAME: if.c,v 1.17 2001/01/21 15:27:30 itojun Exp $	*/

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

#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/queue.h>
#include <sys/ioctl.h>
#include <net/if.h>
#include <net/if_types.h>
#include <ifaddrs.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <netinet/in.h>
#include <netinet/icmp6.h>
#include <netinet/if_ether.h>
#include <unistd.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <event.h>

#include "rtadvd.h"
#include "if.h"
#include "log.h"

#define ROUNDUP(a, size) \
	(((a) & ((size)-1)) ? (1 + ((a) | ((size)-1))) : (a))

#define NEXT_SA(ap) (ap) = (struct sockaddr *) \
	((char *)(ap) + ((ap)->sa_len ? ROUNDUP((ap)->sa_len,\
						 sizeof(u_long)) :\
			  			 sizeof(u_long)))

struct if_msghdr **iflist;
static void get_iflist(char **buf, size_t *size);
static void parse_iflist(struct if_msghdr ***ifmlist_p, char *buf,
    size_t bufsize);

extern int ioctl_sock;

static void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			NEXT_SA(sa);
		}
		else
			rti_info[i] = NULL;
	}
}

struct sockaddr_dl *
if_nametosdl(char *name)
{
	struct ifaddrs *ifap, *ifa;
	struct sockaddr_dl *sdl;

	if (getifaddrs(&ifap) != 0)
		return (NULL);

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strcmp(ifa->ifa_name, name) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_LINK)
			continue;

		sdl = malloc(ifa->ifa_addr->sa_len);
		if (!sdl)
			continue;	/*XXX*/

		memcpy(sdl, ifa->ifa_addr, ifa->ifa_addr->sa_len);
		freeifaddrs(ifap);
		return (sdl);
	}

	freeifaddrs(ifap);
	return (NULL);
}

int
if_getmtu(char *name)
{
	struct ifreq	ifr;
	u_long		mtu = 0;

	memset(&ifr, 0, sizeof(ifr));
	ifr.ifr_addr.sa_family = AF_INET6;
	if (strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name)) >=
	    sizeof(ifr.ifr_name))
		fatalx("strlcpy");
	if (ioctl(ioctl_sock, SIOCGIFMTU, (char *)&ifr) >= 0)
		mtu = ifr.ifr_mtu;
	else
		log_warn("s: %d", ioctl_sock);
	return (mtu);
}

/* give interface index and its old flags, then new flags returned */
int
if_getflags(int ifindex, int oifflags)
{
	struct ifreq ifr;

	if_indextoname(ifindex, ifr.ifr_name);
	if (ioctl(ioctl_sock, SIOCGIFFLAGS, (char *)&ifr) < 0) {
		log_warn("ioctl:SIOCGIFFLAGS: failed for %s", ifr.ifr_name);
		return (oifflags & ~IFF_UP);
	}
	return (ifr.ifr_flags);
}

#define ROUNDUP8(a) (1 + (((a) - 1) | 7))
int
lladdropt_length(struct sockaddr_dl *sdl)
{
	switch (sdl->sdl_type) {
	case IFT_CARP:
	case IFT_ETHER:
		return(ROUNDUP8(ETHER_ADDR_LEN + 2));
	default:
		return(0);
	}
}

void
lladdropt_fill(struct sockaddr_dl *sdl, struct nd_opt_hdr *ndopt)
{
	char *addr;

	ndopt->nd_opt_type = ND_OPT_SOURCE_LINKADDR; /* fixed */

	switch (sdl->sdl_type) {
	case IFT_CARP:
	case IFT_ETHER:
		ndopt->nd_opt_len = (ROUNDUP8(ETHER_ADDR_LEN + 2)) >> 3;
		addr = (char *)(ndopt + 1);
		memcpy(addr, LLADDR(sdl), ETHER_ADDR_LEN);
		break;
	default:
		fatalx("unsupported link type(%d)", sdl->sdl_type);
	}
}

#define SIN6(s) ((struct sockaddr_in6 *)(s))
int
validate_msg(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct ifa_msghdr *ifam;
	struct sockaddr *sa, *dst, *ifa, *rti_info[RTAX_MAX];

	/* just for safety */
	if (!rtm->rtm_msglen) {
		log_warnx("rtm_msglen is 0 (rtm=%p)", rtm);
		return -1;
	}
	if (rtm->rtm_version != RTM_VERSION)
		return -1;

	switch (rtm->rtm_type) {
	case RTM_ADD:
	case RTM_DELETE:
		if (rtm->rtm_tableid != 0)
			return -1;

		/* address related checks */
		sa = (struct sockaddr *)((char *)rtm + rtm->rtm_hdrlen);
		get_rtaddrs(rtm->rtm_addrs, sa, rti_info);
		if ((dst = rti_info[RTAX_DST]) == NULL ||
		    dst->sa_family != AF_INET6)
			return -1;

		if (IN6_IS_ADDR_LINKLOCAL(&SIN6(dst)->sin6_addr) ||
		    IN6_IS_ADDR_MULTICAST(&SIN6(dst)->sin6_addr))
			return -1;

		if (rti_info[RTAX_NETMASK] == NULL)
			return -1;

		/* found */
		return 0;
		/* NOTREACHED */
	case RTM_NEWADDR:
	case RTM_DELADDR:
		ifam = (struct ifa_msghdr *)rtm;

		/* address related checks */
		sa = (struct sockaddr *)((char *)rtm + rtm->rtm_hdrlen);
		get_rtaddrs(ifam->ifam_addrs, sa, rti_info);
		if ((ifa = rti_info[RTAX_IFA]) == NULL ||
		    (ifa->sa_family != AF_INET &&
		    ifa->sa_family != AF_INET6))
			return -1;

		if (ifa->sa_family == AF_INET6 &&
		    (IN6_IS_ADDR_LINKLOCAL(&SIN6(ifa)->sin6_addr) ||
		    IN6_IS_ADDR_MULTICAST(&SIN6(ifa)->sin6_addr)))
			return -1;

		/* found */
		return 0;
		/* NOTREACHED */
	case RTM_IFINFO:
		/* found */
		return 0;
		/* NOTREACHED */
	}
	return -1;
}

struct in6_addr *
get_addr(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct sockaddr *sa, *rti_info[RTAX_MAX];

	sa = (struct sockaddr *)(buf + rtm->rtm_hdrlen);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	return(&SIN6(rti_info[RTAX_DST])->sin6_addr);
}

int
get_rtm_ifindex(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;

	return rtm->rtm_index;
}

int
get_ifm_ifindex(char *buf)
{
	struct if_msghdr *ifm = (struct if_msghdr *)buf;

	return ((int)ifm->ifm_index);
}

int
get_ifam_ifindex(char *buf)
{
	struct ifa_msghdr *ifam = (struct ifa_msghdr *)buf;

	return ((int)ifam->ifam_index);
}

int
get_ifm_flags(char *buf)
{
	struct if_msghdr *ifm = (struct if_msghdr *)buf;

	return (ifm->ifm_flags);
}

int
get_prefixlen(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;
	struct sockaddr *sa, *rti_info[RTAX_MAX];
	u_char *p, *lim;

	sa = (struct sockaddr *)(buf + rtm->rtm_hdrlen);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);
	sa = rti_info[RTAX_NETMASK];

	p = (u_char *)(&SIN6(sa)->sin6_addr);
	lim = (u_char *)sa + sa->sa_len;
	return prefixlen(p, lim);
}

int
prefixlen(u_char *p, u_char *lim)
{
	int masklen;

	for (masklen = 0; p < lim; p++) {
		switch (*p) {
		case 0xff:
			masklen += 8;
			break;
		case 0xfe:
			masklen += 7;
			break;
		case 0xfc:
			masklen += 6;
			break;
		case 0xf8:
			masklen += 5;
			break;
		case 0xf0:
			masklen += 4;
			break;
		case 0xe0:
			masklen += 3;
			break;
		case 0xc0:
			masklen += 2;
			break;
		case 0x80:
			masklen += 1;
			break;
		case 0x00:
			break;
		default:
			return(-1);
		}
	}

	return(masklen);
}

int
rtmsg_type(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;

	return(rtm->rtm_type);
}

int
rtmsg_len(char *buf)
{
	struct rt_msghdr *rtm = (struct rt_msghdr *)buf;

	return(rtm->rtm_msglen);
}

/*
 * alloc buffer and get if_msghdrs block from kernel,
 * and put them into the buffer
 */
static void
get_iflist(char **buf, size_t *size)
{
	int mib[6];

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET6;
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;
	while (1) {
		if (sysctl(mib, 6, NULL, size, NULL, 0) == -1)
			fatal("sysctl: iflist size get failed");
		if (*size == 0)
			break;
		if ((*buf = realloc(*buf, *size)) == NULL)
			fatal(NULL);
		if (sysctl(mib, 6, *buf, size, NULL, 0) == -1) {
			if (errno == ENOMEM)
				continue;
			fatal("sysctl: iflist get failed");
		}
		break;
	}
}

/*
 * alloc buffer and parse if_msghdrs block passed as arg,
 * and init the buffer as list of pointers ot each of the if_msghdr.
 */
static void
parse_iflist(struct if_msghdr ***ifmlist_p, char *buf, size_t bufsize)
{
	int iflentry_size, malloc_size;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	char *lim;

	/*
	 * Estimate least size of an iflist entry, to be obtained from kernel.
	 * Should add sizeof(sockaddr) ??
	 */
	iflentry_size = sizeof(struct if_msghdr);
	/* roughly estimate max list size of pointers to each if_msghdr */
	malloc_size = (bufsize/iflentry_size) * sizeof(size_t);
	if ((*ifmlist_p = malloc(malloc_size)) == NULL)
		fatal(NULL);

	lim = buf + bufsize;
	for (ifm = (struct if_msghdr *)buf; ifm < (struct if_msghdr *)lim;) {
		if (ifm->ifm_msglen == 0) {
			log_warnx("ifm_msglen is 0 (buf=%p lim=%p ifm=%p)",
			    buf, lim, ifm);
			return;
		}
		if (ifm->ifm_type == RTM_IFINFO) {
			if (ifm->ifm_version == RTM_VERSION)
				(*ifmlist_p)[ifm->ifm_index] = ifm;
		} else {
			fatalx("out of sync parsing NET_RT_IFLIST,"
			    " expected %d, got %d, msglen = %d,"
			    " buf:%p, ifm:%p, lim:%p",
			    RTM_IFINFO, ifm->ifm_type, ifm->ifm_msglen,
			    buf, ifm, lim);
		}
		for (ifam = (struct ifa_msghdr *)
			((char *)ifm + ifm->ifm_msglen);
		     ifam < (struct ifa_msghdr *)lim;
		     ifam = (struct ifa_msghdr *)
		     	((char *)ifam + ifam->ifam_msglen)) {
			/* just for safety */
			if (!ifam->ifam_msglen) {
				log_warnx("ifa_msglen is 0 "
				    "(buf=%p lim=%p ifam=%p)",
				    buf, lim, ifam);
				return;
			}
			if (ifam->ifam_type != RTM_NEWADDR)
				break;
		}
		ifm = (struct if_msghdr *)ifam;
	}
}

void
init_iflist(void)
{
	static size_t ifblock_size;
	static char *ifblock;

	if (ifblock) {
		free(ifblock);
		ifblock_size = 0;
	}
	free(iflist);
	/* get iflist block from kernel */
	get_iflist(&ifblock, &ifblock_size);

	/* make list of pointers to each if_msghdr */
	parse_iflist(&iflist, ifblock, ifblock_size);
}
