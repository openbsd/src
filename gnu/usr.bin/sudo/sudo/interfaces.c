/*	$OpenBSD: interfaces.c,v 1.5 1998/03/31 06:41:02 millert Exp $	*/

/*
 *  CU sudo version 1.5.5
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

#ifndef lint
static char rcsid[] = "Id: interfaces.c,v 1.34 1998/03/31 05:05:37 millert Exp $";
#endif /* lint */

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
#ifdef HAVE_SYS_SOCKIO_H
#include <sys/sockio.h>
#else
#include <sys/ioctl.h>
#endif /* HAVE_SYS_SOCKIO_H */
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
#include <sys/time.h>
#include <net/if.h>

#include "sudo.h"
#include <options.h>
#include "version.h"

#if !defined(STDC_HEADERS) && !defined(__GNUC__)
extern char *malloc	__P((size_t));
extern char *realloc	__P((VOID *, size_t));
#endif /* !STDC_HEADERS && !__GNUC__ */

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
    struct ifreq ifreq, *ifr;
    struct sockaddr_in *sin;
    unsigned int localhost_mask;
    int sock, n, i;
    size_t len = sizeof(struct ifconf) + BUFSIZ;
    char *ifconf_buf = NULL;
#ifdef _ISC
    struct strioctl strioctl;
#endif /* _ISC */

    /* so we can skip localhost and its ilk */
    localhost_mask = inet_addr("127.0.0.0");

    sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
	perror("socket");
	exit(1);
    }

    /*
     * get interface configuration or return (leaving interfaces NULL)
     */
    for (;;) {
	if (ifconf_buf == NULL)
	    ifconf_buf = (char *) malloc(len);
	else
	    ifconf_buf = (char *) realloc(ifconf_buf, len);
	if (ifconf_buf == NULL) {
	    perror("malloc");
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
	perror("malloc");
	(void) fprintf(stderr, "%s: cannot allocate memory!\n", Argv[0]);
	exit(1);
    }

    /*
     * for each interface, get the ip address and netmask
     */
    for (ifreq.ifr_name[0] = '\0', i = 0; i < ifconf->ifc_len; ) {
	/* get a pointer to the current interface */
	ifr = (struct ifreq *) ((caddr_t) ifconf->ifc_req + i);

	/* set i to the subscript of the next interface */
#ifdef HAVE_SA_LEN
	if (ifr->ifr_addr.sa_len > sizeof(ifr->ifr_addr))
	    i += sizeof(ifr->ifr_name) + ifr->ifr_addr.sa_len;
	else
#endif /* HAVE_SA_LEN */
	    i += sizeof(struct ifreq);

	/* skip duplicates and interfaces with NULL addresses */
	sin = (struct sockaddr_in *) &ifr->ifr_addr;
	if (sin->sin_addr.s_addr == 0 ||
	    strncmp(ifr->ifr_name, ifreq.ifr_name, sizeof(ifr->ifr_name)) == 0)
	    continue;

	/* make a working copy... */
	ifreq = *ifr;

	/* get the ip address */
#ifdef _ISC
	STRSET(SIOCGIFADDR, (caddr_t) &ifreq, sizeof(ifreq));
	if (ioctl(sock, I_STR, (caddr_t) &strioctl) < 0) {
#else
	if (ioctl(sock, SIOCGIFADDR, (caddr_t) &ifreq)) {
#endif /* _ISC */
	    /* non-fatal error if interface is down or not supported */
	    if (errno == EADDRNOTAVAIL || errno == ENXIO || errno == EAFNOSUPPORT)
		continue;

	    (void) fprintf(stderr, "%s: Error, ioctl: SIOCGIFADDR ", Argv[0]);
	    perror("");
	    exit(1);
	}
	sin = (struct sockaddr_in *) &ifreq.ifr_addr;

	/* store the ip address */
	interfaces[num_interfaces].addr.s_addr = sin->sin_addr.s_addr;

	/* get the netmask */
#ifdef SIOCGIFNETMASK
#ifdef _ISC
	STRSET(SIOCGIFNETMASK, (caddr_t) &ifreq, sizeof(ifreq));
	if (ioctl(sock, I_STR, (caddr_t) &strioctl) == 0) {
#else
	if (ioctl(sock, SIOCGIFNETMASK, (caddr_t) &ifreq) == 0) {
#endif /* _ISC */
	    sin = (struct sockaddr_in *) &ifreq.ifr_addr;

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

	/* avoid localhost and friends */
	if ((interfaces[num_interfaces].addr.s_addr &
	    interfaces[num_interfaces].netmask.s_addr) == localhost_mask)
	    continue;

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
