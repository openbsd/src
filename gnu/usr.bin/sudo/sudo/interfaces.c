/*	$OpenBSD: interfaces.c,v 1.8 1998/12/07 21:32:39 millert Exp $	*/

/*
 *  CU sudo version 1.5.7
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 1, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *  Please send bugs, changes, problems to sudo-bugs@courtesan.com
 *
 *******************************************************************
 *
 *  This module contains load_interfaces() a function that
 *  fills the interfaces global with a list of active ip
 *  addresses and their associated netmasks.
 *
 *  Todd C. Miller  Mon May  1 20:48:43 MDT 1995
 */

#include "config.h"

#include <stdio.h>
#ifdef STDC_HEADERS
#include <stdlib.h>
#endif /* STDC_HEADERS */
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif /* HAVE_UNISTD_H */
#ifdef HAVE_STRING_H
#include <string.h>
#endif /* HAVE_STRING_H */
#ifdef HAVE_STRINGS_H
#include <strings.h>
#endif /* HAVE_STRINGS_H */
#if defined(HAVE_MALLOC_H) && !defined(STDC_HEADERS)
#include <malloc.h>   
#endif /* HAVE_MALLOC_H && !STDC_HEADERS */
#include <netdb.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <sys/ioctl.h>
#if defined(HAVE_SYS_SOCKIO_H) && !defined(SIOCGIFCONF)
#include <sys/sockio.h>
#endif
#ifdef _ISC
#include <sys/stream.h>
#include <sys/sioctl.h>
#include <sys/stropts.h>
#include <net/errno.h>
#define STRSET(cmd, param, len)	{strioctl.ic_cmd=(cmd);\
				 strioctl.ic_dp=(param);\
				 strioctl.ic_timout=0;\
				 strioctl.ic_len=(len);}
#endif /* _ISC */
#ifdef _MIPS
#include <net/soioctl.h>
#endif /* _MIPS */
#include <netinet/in.h>
#include <arpa/inet.h>
#include <net/if.h>

#include "sudo.h"
#include "version.h"

#if !defined(STDC_HEADERS) && !defined(__GNUC__)
extern char *malloc	__P((size_t));
extern char *realloc	__P((VOID *, size_t));
#endif /* !STDC_HEADERS && !__GNUC__ */

#ifndef lint
static const char rcsid[] = "$From: interfaces.c,v 1.46 1998/12/07 21:16:00 millert Exp $";
#endif /* lint */

/*
 * Globals
 */
struct interface *interfaces;
int num_interfaces = 0;
extern int Argc;
extern char **Argv;


#if defined(SIOCGIFCONF) && !defined(STUB_LOAD_INTERFACES)
/**********************************************************************
 *
 *  load_interfaces()
 *
 *  This function sets the interfaces global variable
 *  and sets the constituent ip addrs and netmasks.
 */

void load_interfaces()
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
    if (sock < 0) {
	perror("socket");
	exit(1);
    }

    /*
     * get interface configuration or return (leaving interfaces NULL)
     */
    for (;;) {
	ifconf_buf = ifconf_buf ? realloc(ifconf_buf, len) : malloc(len);
	if (ifconf_buf == NULL) {
	    (void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	    exit(1);
	}
	ifconf = (struct ifconf *) ifconf_buf;
	ifconf->ifc_len = len - sizeof(struct ifconf);
	ifconf->ifc_buf = (caddr_t) (ifconf_buf + sizeof(struct ifconf));

	/* networking may not be installed in kernel */
#ifdef _ISC
	STRSET(SIOCGIFCONF, (caddr_t) ifconf, len);
	if (ioctl(sock, I_STR, (caddr_t) &strioctl) < 0) {
#else
	if (ioctl(sock, SIOCGIFCONF, (caddr_t) ifconf) < 0) {
#endif /* _ISC */
	    (void) free(ifconf_buf);
	    (void) close(sock);
	    return;
	}

	/* break out of loop if we have a big enough buffer */
	if (ifconf->ifc_len + sizeof(struct ifreq) < len)
	    break;
	len += BUFSIZ;
    }

    /*
     * get the maximum number of interfaces that *could* exist.
     */
    n = ifconf->ifc_len / sizeof(struct ifreq);

    /*
     * malloc() space for interfaces array
     */
    interfaces = (struct interface *) malloc(sizeof(struct interface) * n);
    if (interfaces == NULL) {
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }

    /*
     * for each interface, store the ip address and netmask
     */
    for (i = 0; i < ifconf->ifc_len; ) {
	/* get a pointer to the current interface */
	ifr = (struct ifreq *) &ifconf->ifc_buf[i];

	/* set i to the subscript of the next interface */
	i += sizeof(struct ifreq);
#ifdef HAVE_SA_LEN
	if (ifr->ifr_addr.sa_len > sizeof(ifr->ifr_addr))
	    i += ifr->ifr_addr.sa_len - sizeof(struct sockaddr);
#endif /* HAVE_SA_LEN */

	/* skip duplicates and interfaces with NULL addresses */
	sin = (struct sockaddr_in *) &ifr->ifr_addr;
	if (sin->sin_addr.s_addr == 0 ||
	    strncmp(previfname, ifr->ifr_name, sizeof(ifr->ifr_name) - 1) == 0)
	    continue;

	/* skip non-ip things */
	if (ifr->ifr_addr.sa_family != AF_INET)
		continue;

	/*
	 * make sure the interface is up, skip if not.
	 */
#ifdef SIOCGIFFLAGS
	memset(&ifr_tmp, 0, sizeof(ifr_tmp));
	strncpy(ifr_tmp.ifr_name, ifr->ifr_name, sizeof(ifr_tmp.ifr_name) - 1);
	if (ioctl(sock, SIOCGIFFLAGS, (caddr_t) &ifr_tmp) < 0)
#endif
	    ifr_tmp = *ifr;
	
	/* skip interfaces marked "down" and "loopback" */
	if (!(ifr_tmp.ifr_flags & IFF_UP) || (ifr_tmp.ifr_flags & IFF_LOOPBACK))
		continue;

	/* store the ip address */
	sin = (struct sockaddr_in *) &ifr->ifr_addr;
	interfaces[num_interfaces].addr.s_addr = sin->sin_addr.s_addr;

	/* stash the name of the interface we saved */
	previfname = ifr->ifr_name;

	/* get the netmask */
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

	    /* store the netmask */
	    interfaces[num_interfaces].netmask.s_addr = sin->sin_addr.s_addr;
	} else {
#else
	{
#endif /* SIOCGIFNETMASK */
	    if (IN_CLASSC(interfaces[num_interfaces].addr.s_addr))
		interfaces[num_interfaces].netmask.s_addr = htonl(IN_CLASSC_NET);
	    else if (IN_CLASSB(interfaces[num_interfaces].addr.s_addr))
		interfaces[num_interfaces].netmask.s_addr = htonl(IN_CLASSB_NET);
	    else
		interfaces[num_interfaces].netmask.s_addr = htonl(IN_CLASSA_NET);
	}

	/* only now can we be sure it was a good/interesting interface */
	num_interfaces++;
    }

    /* if there were bogus entries, realloc the array */
    if (n != num_interfaces) {
	/* it is unlikely that num_interfaces will be 0 but who knows... */
	if (num_interfaces != 0) {
	    interfaces = (struct interface *) realloc(interfaces,
		sizeof(struct interface) * num_interfaces);
	    if (interfaces == NULL) {
		perror("realloc");
		(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
		exit(1);
	    }
	} else {
	    (void) free(interfaces);
	}
    }
    (void) free(ifconf_buf);
    (void) close(sock);
}

#else /* !SIOCGIFCONF || STUB_LOAD_INTERFACES */

/**********************************************************************
 *
 *  load_interfaces()
 *
 *  Stub function for those without SIOCGIFCONF
 */

void load_interfaces()
{
    return;
}

#endif /* SIOCGIFCONF && !STUB_LOAD_INTERFACES */
