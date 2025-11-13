/*	$OpenBSD: getifaddrs.c,v 1.15 2025/11/13 10:34:32 deraadt Exp $	*/

/*
 * Copyright (c) 1995, 1999
 *	Berkeley Software Design, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * THIS SOFTWARE IS PROVIDED BY Berkeley Software Design, Inc. ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Berkeley Software Design, Inc. BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	BSDI getifaddrs.c,v 2.12 2000/02/23 14:51:59 dab Exp
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <net/if.h>
#include <net/route.h>
#include <sys/sysctl.h>
#include <net/if_dl.h>

#include <errno.h>
#include <ifaddrs.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdio.h>

#define roundup(x, y)	((((uintptr_t)(x)+((y)-1))/(y))*(y))
#define proundup(x, y)  (void *)roundup(x,y)

#define SA_RLEN(sa)	((sa)->sa_len ? \
			    roundup((sa)->sa_len, sizeof(long)) : \
			    roundup(1, sizeof(long)))

int
getifaddrs(struct ifaddrs **pif)
{
	int icnt = 1, dcnt = 0, ncnt = 0, mib[6], i;
	size_t needed;
	char *buf = NULL, *bufp, *data, *names, *next, *p, *p0;
	struct ifaddrs *cif = 0, *ifa, *ift;
	struct rt_msghdr *rtm;
	struct if_msghdr *ifm;
	struct ifa_msghdr *ifam;
	struct sockaddr_dl *dl;
	struct sockaddr *sa;
	u_short index = 0;
	size_t len, alen, dlen, dsize;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;             /* protocol */
	mib[3] = 0;             /* wildcard address family */
	mib[4] = NET_RT_IFLIST;
	mib[5] = 0;             /* no flags */
	while (1) {
		if (sysctl(mib, 6, NULL, &needed, NULL, 0) == -1) {
			free(buf);
			return (-1);
		}
		if (needed == 0)
			break;
		if ((bufp = realloc(buf, needed)) == NULL) {
			free(buf);
			return (-1);
		}
		buf = bufp;
		if (sysctl(mib, 6, buf, &needed, NULL, 0) == -1) {
			if (errno == ENOMEM)
				continue;
			free(buf);
			return (-1);
		}
		break;
	}

	/* Calculate data buffer size */
	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
		case RTM_IFINFO:
			ifm = (struct if_msghdr *)rtm;
			if (ifm->ifm_addrs & RTA_IFP) {
				index = ifm->ifm_index;
				++icnt;
				dl = (struct sockaddr_dl *)(next +
				    rtm->rtm_hdrlen);
				ncnt += dl->sdl_nlen + 1;

				/* sockaddr's need long alignment */
				dcnt = roundup(dcnt, sizeof(long));
				dcnt += SA_RLEN((struct sockaddr *)dl);

				/* ifm_data[] needs long long alignment */
				dcnt = roundup(dcnt, sizeof(long long));
				dcnt += sizeof(ifm->ifm_data);
			} else
				index = 0;
			break;

		case RTM_NEWADDR:
			ifam = (struct ifa_msghdr *)rtm;
			if (index && ifam->ifam_index != index)
				abort();	/* XXX abort illegal in library */

#define	RTA_MASKS	(RTA_NETMASK | RTA_IFA | RTA_BRD)
			if (index == 0 || (ifam->ifam_addrs & RTA_MASKS) == 0)
				break;
			p = next + rtm->rtm_hdrlen;
			++icnt;
			/* Scan to look for length of address */
			alen = 0;
			for (p0 = p, i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifam->ifam_addrs & (1 << i))
				    == 0)
					continue;
				sa = (struct sockaddr *)p;
				len = SA_RLEN(sa);
				if (i == RTAX_IFA) {
					alen = len;
					break;
				}
				p += len;
			}
			for (p = p0, i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifam->ifam_addrs & (1 << i))
				    == 0)
					continue;
				sa = (struct sockaddr *)p;
				len = SA_RLEN(sa);
				/* sockaddr's need long alignment */
				dcnt = roundup(dcnt, sizeof(long));
				if (i == RTAX_NETMASK && sa->sa_len == 0)
					dcnt += alen;
				else
					dcnt += len;
				p += len;
			}
			break;
		}
	}

	if (icnt + ncnt + dcnt == 1) {
		*pif = NULL;
		free(buf);
		return (0);
	}

	dsize = sizeof(struct ifaddrs) * icnt;
	dsize += ncnt;
	dsize = roundup(dsize, sizeof(long long));
	dsize += dcnt;

	data = calloc(dsize, 1);
	if (data == NULL) {
		free(buf);
		return(-1);
	}

	/* ifaddrs[], names, then if_data[] */
	ift = ifa = (struct ifaddrs *)data;
	data += sizeof(struct ifaddrs) * icnt;
	names = data;
	data += ncnt;
	data = proundup(data, sizeof(long long));

	index = 0;
	for (next = buf; next < buf + needed; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		switch (rtm->rtm_type) {
		case RTM_IFINFO:
			ifm = (struct if_msghdr *)rtm;
			if (ifm->ifm_addrs & RTA_IFP) {
				index = ifm->ifm_index;
				dl = (struct sockaddr_dl *)(next +
				    rtm->rtm_hdrlen);

				cif = ift;
				ift->ifa_name = names;
				ift->ifa_flags = (int)ifm->ifm_flags;
				memcpy(names, dl->sdl_data, dl->sdl_nlen);
				names[dl->sdl_nlen] = 0;
				names += dl->sdl_nlen + 1;

				data = proundup(data, sizeof(long));
				ift->ifa_addr = (struct sockaddr *)data;
				memcpy(data, dl,
				    ((struct sockaddr *)dl)->sa_len);
				data += SA_RLEN((struct sockaddr *)dl);

				/* if_data needs long long alignment */
				data = proundup(data, sizeof(long long));
				ift->ifa_data = data;
				dlen = rtm->rtm_hdrlen -
				    offsetof(struct if_msghdr, ifm_data);
				if (dlen > sizeof(ifm->ifm_data))
					dlen = sizeof(ifm->ifm_data);
				memcpy(data, &ifm->ifm_data, dlen);
				data += dlen;

				ift = (ift->ifa_next = ift + 1);
			} else
				index = 0;
			break;

		case RTM_NEWADDR:
			ifam = (struct ifa_msghdr *)rtm;
			if (index && ifam->ifam_index != index)
				abort();	/* XXX abort illegal in library */

			if (index == 0 || (ifam->ifam_addrs & RTA_MASKS) == 0)
				break;
			ift->ifa_name = cif->ifa_name;
			ift->ifa_flags = cif->ifa_flags;
			ift->ifa_data = NULL;
			p = next + rtm->rtm_hdrlen;
			/* Scan to look for length of address */
			alen = 0;
			for (p0 = p, i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifam->ifam_addrs & (1 << i))
				    == 0)
					continue;
				sa = (struct sockaddr *)p;
				len = SA_RLEN(sa);
				if (i == RTAX_IFA) {
					alen = len;
					break;
				}
				p += len;
			}
			for (p = p0, i = 0; i < RTAX_MAX; i++) {
				if ((RTA_MASKS & ifam->ifam_addrs & (1 << i))
				    == 0)
					continue;
				sa = (struct sockaddr *)p;
				len = SA_RLEN(sa);
				switch (i) {
				case RTAX_IFA:
					data = proundup(data, sizeof(long));
					ift->ifa_addr = (struct sockaddr *)data;
					memcpy(data, p, len);
					data += len;
					break;

				case RTAX_NETMASK:
					data = proundup(data, sizeof(long));
					ift->ifa_netmask =
					    (struct sockaddr *)data;
					if (sa->sa_len == 0) {
						memset(data, 0, alen);
						data += alen;
						break;
					}
					memcpy(data, p, len);
					data += len;
					break;

				case RTAX_BRD:
					data = proundup(data, sizeof(long));
					ift->ifa_broadaddr =
					    (struct sockaddr *)data;
					memcpy(data, p, len);
					data += len;
					break;
				}
				p += len;
			}


			ift = (ift->ifa_next = ift + 1);
			break;
		}
	}

	/* XXX temporary paranoia until we are sure it is bug free */
	if (dsize != (char *)data - (char *)ifa) {
		char buf[1024];

                /* <10> is LOG_CRIT */
		snprintf(buf, sizeof buf,
		    "<10>%s: getifaddrs: allocated %lu used %lu\n",
		    __progname, dsize, (char *)data - (char *)ifa);
		sendsyslog(buf, strlen(buf), LOG_CONS);
	}

	free(buf);
	if (--ift >= ifa) {
		ift->ifa_next = NULL;
		*pif = ifa;
	} else {
		*pif = NULL;
		free(ifa);
	}
	return (0);
}
DEF_WEAK(getifaddrs);

void
freeifaddrs(struct ifaddrs *ifp)
{
	free(ifp);
}
DEF_WEAK(freeifaddrs);
