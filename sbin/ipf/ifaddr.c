/*	$OpenBSD: ifaddr.c,v 1.6 2001/01/17 05:00:58 fgsch Exp $	*/

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <netinet/in.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <string.h>
#include <err.h>

/*
 * if_addr():
 *      given a string containing an interface name (e.g. "ppp0")
 *      return the IP address it represents
 *
 * The OpenBSD community considers this feature to be quite useful and
 * suggests inclusion into other platforms. The closest alternative is
 * to define /etc/networks with suitable values.
 */
int if_addr(name, ap)
	char *name;
	struct in_addr *ap;
{
	struct sockaddr_in *sin;
	struct ifreq ifr;
	int s;

	if ((s = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
		warn("socket");
		return (0);
	}

	strncpy(ifr.ifr_name, name, IFNAMSIZ);
	ifr.ifr_name[IFNAMSIZ - 1] = '\0';

	if (ioctl(s, SIOCGIFADDR, &ifr) < 0)
		return (0);

	sin = (struct sockaddr_in *)&ifr.ifr_addr;
	*ap = sin->sin_addr;

	return (1);
}
