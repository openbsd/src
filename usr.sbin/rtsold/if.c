/*	$OpenBSD: if.c,v 1.28 2014/11/18 20:54:29 krw Exp $	*/
/*	$KAME: if.c,v 1.18 2002/05/31 10:10:03 itojun Exp $	*/

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

#include <sys/param.h>
#include <sys/socket.h>
#include <sys/sysctl.h>
#include <sys/ioctl.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/icmp6.h>

#include <netinet6/in6_var.h>

#include <err.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <ifaddrs.h>
#include <netdb.h>

#include "rtsold.h"

extern int rssock;
static int ifsock;

static int get_llflag(const char *);

int
ifinit(void)
{
	ifsock = rssock;

	return(0);
}

int
get_rdomain(char *name)
{
	int rv = 0, s;
	struct ifreq ifr;

	if ((s = socket(AF_INET6, SOCK_DGRAM, 0)) == -1)
		warnmsg(LOG_ERR, __func__,
		    "get_rdomain socket: %s", strerror(errno));

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(s, SIOCGIFRDOMAIN, (caddr_t)&ifr) != -1)
	    rv = ifr.ifr_rdomainid;

	close(s);
	return rv;
}


int
interface_up(char *name)
{
	struct ifreq ifr;
	int llflag;

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(ifsock, SIOCGIFXFLAGS, (caddr_t)&ifr) < 0)
		warn("SIOCGIFXFLAGS");
	ifr.ifr_flags &= ~IFXF_NOINET6;
	ifr.ifr_flags |= IFXF_AUTOCONF6;
	if (ioctl(ifsock, SIOCSIFXFLAGS, (caddr_t)&ifr) < 0)
		warn("SIOCSIFXFLAGS");

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(ifsock, SIOCGIFFLAGS, (caddr_t)&ifr) < 0) {
		warnmsg(LOG_WARNING, __func__, "ioctl(SIOCGIFFLAGS): %s",
		    strerror(errno));
		return(-1);
	}
	if (!(ifr.ifr_flags & IFF_UP)) {
		ifr.ifr_flags |= IFF_UP;
		if (ioctl(ifsock, SIOCSIFFLAGS, (caddr_t)&ifr) < 0)
			warnmsg(LOG_ERR, __func__,
			    "ioctl(SIOCSIFFLAGS): %s", strerror(errno));
		return(-1);
	}

	warnmsg(LOG_DEBUG, __func__, "checking if %s is ready...", name);

	llflag = get_llflag(name);
	if (llflag < 0) {
		warnmsg(LOG_WARNING, __func__,
		    "get_llflag() failed, anyway I'll try");
		return 0;
	}

	if (!(llflag & IN6_IFF_NOTREADY)) {
		warnmsg(LOG_DEBUG, __func__, "%s is ready", name);
		return(0);
	} else {
		if (llflag & IN6_IFF_TENTATIVE) {
			warnmsg(LOG_DEBUG, __func__, "%s is tentative",
			    name);
			return IFS_TENTATIVE;
		}
		if (llflag & IN6_IFF_DUPLICATED)
			warnmsg(LOG_DEBUG, __func__, "%s is duplicated",
			    name);
		return -1;
	}
}

int
interface_status(struct ifinfo *ifinfo)
{
	char *ifname = ifinfo->ifname;
	struct ifreq ifr;
	struct ifmediareq ifmr;

	/* get interface flags */
	memset(&ifr, 0, sizeof(ifr));
	strncpy(ifr.ifr_name, ifname, sizeof(ifr.ifr_name));
	if (ioctl(ifsock, SIOCGIFFLAGS, &ifr) < 0) {
		warnmsg(LOG_ERR, __func__, "ioctl(SIOCGIFFLAGS) on %s: %s",
		    ifname, strerror(errno));
		return(-1);
	}
	/*
	 * if one of UP and RUNNING flags is dropped,
	 * the interface is not active.
	 */
	if ((ifr.ifr_flags & (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		goto inactive;
	}

	/* Next, check carrier on the interface, if possible */
	if (!ifinfo->mediareqok)
		goto active;
	memset(&ifmr, 0, sizeof(ifmr));
	strncpy(ifmr.ifm_name, ifname, sizeof(ifmr.ifm_name));

	if (ioctl(ifsock, SIOCGIFMEDIA, (caddr_t)&ifmr) < 0) {
		if (errno != EINVAL) {
			warnmsg(LOG_DEBUG, __func__,
			    "ioctl(SIOCGIFMEDIA) on %s: %s",
			    ifname, strerror(errno));
			return(-1);
		}
		/*
		 * EINVAL simply means that the interface does not support
		 * the SIOCGIFMEDIA ioctl. We regard it alive.
		 */
		ifinfo->mediareqok = 0;
		goto active;
	}

	if (ifmr.ifm_status & IFM_AVALID) {
		switch (ifmr.ifm_active & IFM_NMASK) {
		case IFM_ETHER:
		case IFM_IEEE80211:
			if (ifmr.ifm_status & IFM_ACTIVE)
				goto active;
			else
				goto inactive;
			break;
		default:
			goto inactive;
		}
	}

  inactive:
	return(0);

  active:
	return(1);
}

#define ROUNDUP8(a) (1 + (((a) - 1) | 7))

int
lladdropt_length(struct sockaddr_dl *sdl)
{
	switch (sdl->sdl_type) {
	case IFT_ETHER:
#ifdef IFT_IEEE80211
	case IFT_IEEE80211:
#endif
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
	case IFT_ETHER:
#ifdef IFT_IEEE80211
	case IFT_IEEE80211:
#endif
		ndopt->nd_opt_len = (ROUNDUP8(ETHER_ADDR_LEN + 2)) >> 3;
		addr = (char *)(ndopt + 1);
		memcpy(addr, LLADDR(sdl), ETHER_ADDR_LEN);
		break;
	default:
		warnmsg(LOG_ERR, __func__,
		    "unsupported link type(%d)", sdl->sdl_type);
		exit(1);
	}

	return;
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
getinet6sysctl(int proto, int code)
{
	int mib[] = { CTL_NET, PF_INET6, proto, 0 };
	int value;
	size_t size;

	mib[3] = code;
	size = sizeof(value);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &value, &size, NULL, 0) < 0)
		return -1;
	else
		return value;
}

int
setinet6sysctl(int proto, int code, int newval)
{
	int mib[] = { CTL_NET, PF_INET6, proto, 0 };
	int value;
	size_t size;

	mib[3] = code;
	size = sizeof(value);
	if (sysctl(mib, sizeof(mib)/sizeof(mib[0]), &value, &size,
	    &newval, sizeof(newval)) < 0)
		return -1;
	else
		return value;
}

/* get ia6_flags for link-local addr on if.  returns -1 on error. */
static int
get_llflag(const char *name)
{
	struct ifaddrs *ifap, *ifa;
	struct in6_ifreq ifr6;
	struct sockaddr_in6 *sin6;
	int s;

	if ((s = socket(PF_INET6, SOCK_DGRAM, 0)) < 0) {
		warnmsg(LOG_ERR, __func__, "socket(SOCK_DGRAM): %s",
		    strerror(errno));
		exit(1);
	}
	if (getifaddrs(&ifap) != 0) {
		warnmsg(LOG_ERR, __func__, "getifaddrs: %s",
		    strerror(errno));
		exit(1);
	}

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if (strlen(ifa->ifa_name) != strlen(name) ||
		    strncmp(ifa->ifa_name, name, strlen(name)) != 0)
			continue;
		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
			continue;

		memset(&ifr6, 0, sizeof(ifr6));
		strncpy(ifr6.ifr_name, name, sizeof(ifr6.ifr_name));
		memcpy(&ifr6.ifr_ifru.ifru_addr, sin6, sin6->sin6_len);
		if (ioctl(s, SIOCGIFAFLAG_IN6, &ifr6) < 0) {
			warnmsg(LOG_ERR, __func__,
			    "ioctl(SIOCGIFAFLAG_IN6): %s", strerror(errno));
			exit(1);
		}

		freeifaddrs(ifap);
		close(s);
		return ifr6.ifr_ifru.ifru_flags6;
	}

	freeifaddrs(ifap);
	close(s);
	return -1;
}
