/*
 * Copyright (c) 1996, 1998-2005 Todd C. Miller <Todd.Miller@courtesan.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 * Sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F39502-99-1-0512.
 */

/*
 * Supress a warning w/ gcc on Digital UN*X.
 * The system headers should really do this....
 */
#if defined(__osf__) && !defined(__cplusplus)
struct mbuf;
struct rtentry;
#endif

#include <config.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#if defined(HAVE_SYS_SOCKIO_H) && !defined(SIOCGIFCONF)
# include <sys/sockio.h>
#endif
#include <stdio.h>
#ifdef STDC_HEADERS
# include <stdlib.h>
# include <stddef.h>
#else
# ifdef HAVE_STDLIB_H
#  include <stdlib.h>
# endif
#endif /* STDC_HEADERS */
#ifdef HAVE_STRING_H
# if defined(HAVE_MEMORY_H) && !defined(STDC_HEADERS)
#  include <memory.h>
# endif
# include <string.h>
#else
# ifdef HAVE_STRINGS_H
#  include <strings.h>
# endif
#endif /* HAVE_STRING_H */
#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_ERR_H
# include <err.h>
#else
# include "emul/err.h"
#endif /* HAVE_ERR_H */
#include <netdb.h>
#include <errno.h>
#ifdef _ISC
# include <sys/stream.h>
# include <sys/sioctl.h>
# include <sys/stropts.h>
# define STRSET(cmd, param, len) {strioctl.ic_cmd=(cmd);\
				 strioctl.ic_dp=(param);\
				 strioctl.ic_timout=0;\
				 strioctl.ic_len=(len);}
#endif /* _ISC */
#ifdef _MIPS
# include <net/soioctl.h>
#endif /* _MIPS */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>
#ifdef HAVE_GETIFADDRS
# include <ifaddrs.h>
#endif

#include "sudo.h"
#include "interfaces.h"

#ifndef lint
__unused static const char rcsid[] = "$Sudo: interfaces.c,v 1.72.2.6 2007/08/14 15:19:25 millert Exp $";
#endif /* lint */


#ifdef HAVE_GETIFADDRS

/*
 * Allocate and fill in the interfaces global variable with the
 * machine's ip addresses and netmasks.
 */
void
load_interfaces()
{
    struct ifaddrs *ifa, *ifaddrs;
    struct sockaddr_in *sin;
#ifdef AF_INET6
    struct sockaddr_in6 *sin6;
#endif
    int i;

    if (getifaddrs(&ifaddrs))
	return;

    /* Allocate space for the interfaces list. */
    for (ifa = ifaddrs; ifa != NULL; ifa = ifa -> ifa_next) {
	/* Skip interfaces marked "down" and "loopback". */
	if (ifa->ifa_addr == NULL || !ISSET(ifa->ifa_flags, IFF_UP) ||
	    ISSET(ifa->ifa_flags, IFF_LOOPBACK))
	    continue;

	switch(ifa->ifa_addr->sa_family) {
	    case AF_INET:
#ifdef AF_INET6
	    case AF_INET6:
#endif
		num_interfaces++;
		break;
	}
    }
    if (num_interfaces == 0)
	return;
    interfaces =
	(struct interface *) emalloc2(num_interfaces, sizeof(struct interface));

    /* Store the ip addr / netmask pairs. */
    for (ifa = ifaddrs, i = 0; ifa != NULL; ifa = ifa -> ifa_next) {
	/* Skip interfaces marked "down" and "loopback". */
	if (ifa->ifa_addr == NULL || !ISSET(ifa->ifa_flags, IFF_UP) ||
	    ISSET(ifa->ifa_flags, IFF_LOOPBACK))
		continue;

	switch(ifa->ifa_addr->sa_family) {
	    case AF_INET:
		sin = (struct sockaddr_in *)ifa->ifa_addr;
		memcpy(&interfaces[i].addr, &sin->sin_addr,
		    sizeof(struct in_addr));
		sin = (struct sockaddr_in *)ifa->ifa_netmask;
		memcpy(&interfaces[i].netmask, &sin->sin_addr,
		    sizeof(struct in_addr));
		interfaces[i].family = AF_INET;
		i++;
		break;
#ifdef AF_INET6
	    case AF_INET6:
		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		memcpy(&interfaces[i].addr, &sin6->sin6_addr,
		    sizeof(struct in6_addr));
		sin6 = (struct sockaddr_in6 *)ifa->ifa_netmask;
		memcpy(&interfaces[i].netmask, &sin6->sin6_addr,
		    sizeof(struct in6_addr));
		interfaces[i].family = AF_INET6;
		i++;
		break;
#endif /* AF_INET6 */
	}
    }
#ifdef HAVE_FREEIFADDRS
    freeifaddrs(ifaddrs);
#else
    efree(ifaddrs);
#endif
}

#elif defined(SIOCGIFCONF) && !defined(STUB_LOAD_INTERFACES)

/*
 * Allocate and fill in the interfaces global variable with the
 * machine's ip addresses and netmasks.
 */
void
load_interfaces()
{
    struct ifconf *ifconf;
    struct ifreq *ifr, ifr_tmp;
    struct sockaddr_in *sin;
    int sock, n, i;
    size_t len = sizeof(struct ifconf) + BUFSIZ;
    char *previfname = "", *ifconf_buf = NULL;
#ifdef _ISC
    struct strioctl strioctl;
#endif /* _ISC */

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
	err(1, "cannot open socket");

    /*
     * Get interface configuration or return (leaving num_interfaces == 0)
     */
    for (;;) {
	ifconf_buf = erealloc(ifconf_buf, len);
	ifconf = (struct ifconf *) ifconf_buf;
	ifconf->ifc_len = len - sizeof(struct ifconf);
	ifconf->ifc_buf = (caddr_t) (ifconf_buf + sizeof(struct ifconf));

#ifdef _ISC
	STRSET(SIOCGIFCONF, (caddr_t) ifconf, len);
	if (ioctl(sock, I_STR, (caddr_t) &strioctl) < 0) {
#else
	/* Note that some kernels return EINVAL if the buffer is too small */
	if (ioctl(sock, SIOCGIFCONF, (caddr_t) ifconf) < 0 && errno != EINVAL) {
#endif /* _ISC */
	    efree(ifconf_buf);
	    (void) close(sock);
	    return;
	}

	/* Break out of loop if we have a big enough buffer. */
	if (ifconf->ifc_len + sizeof(struct ifreq) < len)
	    break;
	len += BUFSIZ;
    }

    /* Allocate space for the maximum number of interfaces that could exist. */
    if ((n = ifconf->ifc_len / sizeof(struct ifreq)) == 0)
	return;
    interfaces = (struct interface *) emalloc2(n, sizeof(struct interface));

    /* For each interface, store the ip address and netmask. */
    for (i = 0; i < ifconf->ifc_len; ) {
	/* Get a pointer to the current interface. */
	ifr = (struct ifreq *) &ifconf->ifc_buf[i];

	/* Set i to the subscript of the next interface. */
	i += sizeof(struct ifreq);
#ifdef HAVE_SA_LEN
	if (ifr->ifr_addr.sa_len > sizeof(ifr->ifr_addr))
	    i += ifr->ifr_addr.sa_len - sizeof(struct sockaddr);
#endif /* HAVE_SA_LEN */

	/* Skip duplicates and interfaces with NULL addresses. */
	sin = (struct sockaddr_in *) &ifr->ifr_addr;
	if (sin->sin_addr.s_addr == 0 ||
	    strncmp(previfname, ifr->ifr_name, sizeof(ifr->ifr_name) - 1) == 0)
	    continue;

	if (ifr->ifr_addr.sa_family != AF_INET)
		continue;

#ifdef SIOCGIFFLAGS
	memset(&ifr_tmp, 0, sizeof(ifr_tmp));
	strncpy(ifr_tmp.ifr_name, ifr->ifr_name, sizeof(ifr_tmp.ifr_name) - 1);
	if (ioctl(sock, SIOCGIFFLAGS, (caddr_t) &ifr_tmp) < 0)
#endif
	    ifr_tmp = *ifr;
	
	/* Skip interfaces marked "down" and "loopback". */
	if (!ISSET(ifr_tmp.ifr_flags, IFF_UP) ||
	    ISSET(ifr_tmp.ifr_flags, IFF_LOOPBACK))
		continue;

	sin = (struct sockaddr_in *) &ifr->ifr_addr;
	interfaces[num_interfaces].addr.ip4.s_addr = sin->sin_addr.s_addr;

	/* Stash the name of the interface we saved. */
	previfname = ifr->ifr_name;

	/* Get the netmask. */
	(void) memset(&ifr_tmp, 0, sizeof(ifr_tmp));
	strncpy(ifr_tmp.ifr_name, ifr->ifr_name, sizeof(ifr_tmp.ifr_name) - 1);
#ifdef SIOCGIFNETMASK
#ifdef _ISC
	STRSET(SIOCGIFNETMASK, (caddr_t) &ifr_tmp, sizeof(ifr_tmp));
	if (ioctl(sock, I_STR, (caddr_t) &strioctl) == 0) {
#else
	if (ioctl(sock, SIOCGIFNETMASK, (caddr_t) &ifr_tmp) == 0) {
#endif /* _ISC */
	    sin = (struct sockaddr_in *) &ifr_tmp.ifr_addr;

	    interfaces[num_interfaces].netmask.ip4.s_addr = sin->sin_addr.s_addr;
	} else {
#else
	{
#endif /* SIOCGIFNETMASK */
	    if (IN_CLASSC(interfaces[num_interfaces].addr.ip4.s_addr))
		interfaces[num_interfaces].netmask.ip4.s_addr = htonl(IN_CLASSC_NET);
	    else if (IN_CLASSB(interfaces[num_interfaces].addr.ip4.s_addr))
		interfaces[num_interfaces].netmask.ip4.s_addr = htonl(IN_CLASSB_NET);
	    else
		interfaces[num_interfaces].netmask.ip4.s_addr = htonl(IN_CLASSA_NET);
	}

	/* Only now can we be sure it was a good/interesting interface. */
	interfaces[num_interfaces].family = AF_INET;
	num_interfaces++;
    }

    /* If the expected size < real size, realloc the array. */
    if (n != num_interfaces) {
	if (num_interfaces != 0)
	    interfaces = (struct interface *) erealloc3(interfaces,
		num_interfaces, sizeof(struct interface));
	else
	    efree(interfaces);
    }
    efree(ifconf_buf);
    (void) close(sock);
}

#else /* !SIOCGIFCONF || STUB_LOAD_INTERFACES */

/*
 * Stub function for those without SIOCGIFCONF
 */
void
load_interfaces()
{
    return;
}

#endif /* SIOCGIFCONF && !STUB_LOAD_INTERFACES */

void
dump_interfaces()
{
    int i;
#ifdef AF_INET6
    char addrbuf[INET6_ADDRSTRLEN], maskbuf[INET6_ADDRSTRLEN];
#endif

    puts("Local IP address and netmask pairs:");
    for (i = 0; i < num_interfaces; i++) {
	switch(interfaces[i].family) {
	    case AF_INET:
		printf("\t%s / ", inet_ntoa(interfaces[i].addr.ip4));
		puts(inet_ntoa(interfaces[i].netmask.ip4));
		break;
#ifdef AF_INET6
	    case AF_INET6:
		inet_ntop(AF_INET6, &interfaces[i].addr.ip6,
		    addrbuf, sizeof(addrbuf));
		inet_ntop(AF_INET6, &interfaces[i].netmask.ip6,
		    maskbuf, sizeof(maskbuf));
		printf("\t%s / %s\n", addrbuf, maskbuf);
		break;
#endif /* AF_INET6 */
	}
    }
}
