/* $OpenBSD: ifaddr.c,v 1.3 2000/02/16 22:34:21 kjell Exp $ */
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <net/if.h>
#include <netinet/in.h>
#include <err.h>
#include <stdlib.h>
#include <unistd.h>
#include "ifaddr.h"


/*
 * if_addr():
 *      given a string containing an interface name (e.g. "ppp0")
 *      return the IP address it represents
 *
 * The OpenBSD community considers this feature to be quite useful and
 * suggests inclusion into other platforms. The closest alternative is
 * to define /etc/networks with suitable values.
 */
int     if_addr(name, ap)
char            *name;
struct in_addr  *ap;
{
        struct ifconf ifc;
        struct ifreq ifreq, *ifr;
        char *inbuf = NULL;
        int s, i, len = 8192;

        if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
                warn("socket");
                return 0;
        }

        while (1) {
                ifc.ifc_len = len;
                ifc.ifc_buf = inbuf = realloc(inbuf, len);
                if (inbuf == NULL)
                        err(1, "malloc");
                if (ioctl(s, SIOCGIFCONF, &ifc) < 0) {
                        warn("SIOCGIFCONF");
                        goto if_addr_lose;
                }
                if (ifc.ifc_len + sizeof(ifreq) < len)
                        break;
                len *= 2;
        }
        ifr = ifc.ifc_req;
        ifreq.ifr_name[0] = '\0';
        for (i = 0; i < ifc.ifc_len; ) {
                ifr = (struct ifreq *)((caddr_t)ifc.ifc_req + i);
                i += sizeof(ifr->ifr_name) +
                        (ifr->ifr_addr.sa_len > sizeof(struct sockaddr)
                                ? ifr->ifr_addr.sa_len
                                : sizeof(struct sockaddr));
                ifreq = *ifr;
                if (ioctl(s, SIOCGIFADDR, (caddr_t)ifr) < 0)
                        continue;
                if (ifr->ifr_addr.sa_family != AF_INET)
                        continue;
                if (!strcmp(name, ifr->ifr_name)) {
                        struct sockaddr_in *sin;
                        close(s);
                        free(inbuf);
                        sin = (struct sockaddr_in *)&ifr->ifr_addr;
                        *ap = sin->sin_addr;
                        return (1);
                }
        }

if_addr_lose:
        close(s);
        free(inbuf);
        return 0;
}
