/*	$OpenBSD: kroute.c,v 1.197 2021/03/28 17:25:21 krw Exp $	*/

/*
 * Copyright 2012 Kenneth R Westerback <krw@openbsd.org>
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
 */

#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/sysctl.h>

#include <arpa/inet.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <errno.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <limits.h>
#include <poll.h>
#include <resolv.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "dhcp.h"
#include "dhcpd.h"
#include "log.h"
#include "privsep.h"

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

#define	CIDR_MAX_BITS	32

int		 delete_addresses(char *, int, struct in_addr, struct in_addr);
void		 set_address(char *, int, struct in_addr, struct in_addr);
void		 delete_address(char *, int, struct in_addr);

char		*get_routes(int, size_t *);
void		 get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
unsigned int	 route_pos(struct rt_msghdr *, uint8_t *, unsigned int,
    struct in_addr);
void		 flush_routes(int, int, int, uint8_t *, unsigned int,
    struct in_addr);
void		 discard_route(uint8_t *, unsigned int);
void		 add_route(char *, int, int, struct in_addr, struct in_addr,
		    struct in_addr, struct in_addr, int);
void		 set_routes(char *, int, int, int, struct in_addr,
    struct in_addr, uint8_t *, unsigned int);

int		 default_route_index(int, int);
char		*resolv_conf_tail(void);
char		*set_resolv_conf(char *, char *, struct unwind_info *);

void		 set_mtu(char *, int, uint16_t);

/*
 * delete_addresses() removes all inet addresses on the named interface, except
 * for newaddr/newnetmask.
 *
 * If newaddr/newmask is already present, return 1, else 0.
 */
int
delete_addresses(char *name, int ioctlfd, struct in_addr newaddr,
    struct in_addr newnetmask)
{
	struct in_addr			 addr, netmask;
	struct ifaddrs			*ifap, *ifa;
	int				 found;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	found = 0;
	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) != 0 ||
		    (ifa->ifa_flags & IFF_POINTOPOINT) != 0 ||
		    ((ifa->ifa_flags & IFF_UP) == 0) ||
		    (ifa->ifa_addr->sa_family != AF_INET) ||
		    (strcmp(name, ifa->ifa_name) != 0))
			continue;

		memcpy(&addr,
		    &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
		    sizeof(addr));
		memcpy(&netmask,
		    &((struct sockaddr_in *)ifa->ifa_netmask)->sin_addr,
		    sizeof(netmask));

		if (addr.s_addr == newaddr.s_addr &&
		    netmask.s_addr == newnetmask.s_addr) {
			found = 1;
		} else {
			delete_address(name, ioctlfd, addr);
		}
	}

	freeifaddrs(ifap);
	return found;
}

/*
 * set_address() is the equivalent of
 *
 *	ifconfig <if> inet <addr> netmask <mask> broadcast <addr>
 */
void
set_address(char *name, int ioctlfd, struct in_addr addr,
    struct in_addr netmask)
{
	struct ifaliasreq	 ifaliasreq;
	struct sockaddr_in	*in;

	if (delete_addresses(name, ioctlfd, addr, netmask) == 1)
		return;

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, name, sizeof(ifaliasreq.ifra_name));

	/* The actual address in ifra_addr. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = addr.s_addr;

	/* And the netmask in ifra_mask. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_mask;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_mask);
	in->sin_addr.s_addr = netmask.s_addr;

	/* No need to set broadcast address. Kernel can figure it out. */

	if (ioctl(ioctlfd, SIOCAIFADDR, &ifaliasreq) == -1)
		log_warn("%s: SIOCAIFADDR %s", log_procname,
		    inet_ntoa(addr));
}

void
delete_address(char *name, int ioctlfd, struct in_addr addr)
{
	struct ifaliasreq	 ifaliasreq;
	struct sockaddr_in	*in;

	/*
	 * Delete specified address on specified interface.
	 *
	 * Deleting the address also clears out arp entries.
	 */

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, name, sizeof(ifaliasreq.ifra_name));

	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = addr.s_addr;

	/* SIOCDIFADDR will result in a RTM_DELADDR message we must catch! */
	if (ioctl(ioctlfd, SIOCDIFADDR, &ifaliasreq) == -1) {
		if (errno != EADDRNOTAVAIL)
			log_warn("%s: SIOCDIFADDR %s", log_procname,
			    inet_ntoa(addr));
	}
}

/*
 * get_routes() returns all relevant routes currently configured, and the
 * length of the buffer being returned.
 */
char *
get_routes(int rdomain, size_t *len)
{
	int		 mib[7];
	char		*buf, *bufp, *errmsg = NULL;
	size_t		 needed;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;	/* PF_ROUTE (not AF_ROUTE) for sysctl(2)! */
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_STATIC;
	mib[6] = rdomain;

	buf = NULL;
	errmsg = NULL;
	for (;;) {
		if (sysctl(mib, 7, NULL, &needed, NULL, 0) == -1) {
			errmsg = "sysctl size of routes:";
			break;
		}
		if (needed == 0) {
			free(buf);
			return NULL;
		}
		if ((bufp = realloc(buf, needed)) == NULL) {
			errmsg = "routes buf realloc:";
			break;
		}
		buf = bufp;
		if (sysctl(mib, 7, buf, &needed, NULL, 0) == -1) {
			if (errno == ENOMEM)
				continue;
			errmsg = "sysctl retrieval of routes:";
			break;
		}
		break;
	}

	if (errmsg != NULL) {
		log_warn("%s: get_routes - %s (msize=%zu)", log_procname,
		    errmsg, needed);
		free(buf);
		buf = NULL;
	}

	*len = needed;
	return buf;
}

/*
 * get_rtaddrs() populates rti_info with pointers to the
 * sockaddr's contained in a rtm message.
 */
void
get_rtaddrs(int addrs, struct sockaddr *sa, struct sockaddr **rti_info)
{
	int	i;

	for (i = 0; i < RTAX_MAX; i++) {
		if (addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}
/*
 * route_pos() finds the position of the *rtm route within
 * routes.
 *
 * If the *rtm route is not in routes, return routes_len.
 */
unsigned int
route_pos(struct rt_msghdr *rtm, uint8_t *routes, unsigned int routes_len,
    struct in_addr address)
{
	struct sockaddr		*rti_info[RTAX_MAX];
	struct sockaddr		*dst, *netmask, *gateway;
	in_addr_t		 dstaddr, netmaskaddr, gatewayaddr;
	in_addr_t		 routesdstaddr, routesnetmaskaddr;
	in_addr_t		 routesgatewayaddr;
	unsigned int		 i, len;

	get_rtaddrs(rtm->rtm_addrs,
	    (struct sockaddr *)((char *)(rtm) + rtm->rtm_hdrlen),
	    rti_info);

	dst = rti_info[RTAX_DST];
	netmask = rti_info[RTAX_NETMASK];
	gateway = rti_info[RTAX_GATEWAY];

	if (dst == NULL || netmask == NULL || gateway == NULL)
		return routes_len;

	if (dst->sa_family != AF_INET || netmask->sa_family != AF_INET ||
	    gateway->sa_family != AF_INET)
		return routes_len;

	dstaddr = ((struct sockaddr_in *)dst)->sin_addr.s_addr;
	netmaskaddr = ((struct sockaddr_in *)netmask)->sin_addr.s_addr;
	gatewayaddr = ((struct sockaddr_in *)gateway)->sin_addr.s_addr;

	dstaddr &= netmaskaddr;
	i = 0;
	while (i < routes_len)  {
		len = extract_route(&routes[i], routes_len - i, &routesdstaddr,
		    &routesnetmaskaddr, &routesgatewayaddr);
		if (len == 0)
			break;

		/* Direct route in routes:
		 *
		 * dst=1.2.3.4 netmask=255.255.255.255 gateway=0.0.0.0
		 *
		 * direct route in rtm:
		 *
		 * dst=1.2.3.4 netmask=255.255.255.255 gateway = address
		 *
		 * So replace 0.0.0.0 with address for comparison.
		 */
		if (routesgatewayaddr == INADDR_ANY)
			routesgatewayaddr = address.s_addr;
		routesdstaddr &= routesnetmaskaddr;

		if (dstaddr == routesdstaddr &&
		    netmaskaddr == routesnetmaskaddr &&
		    gatewayaddr == routesgatewayaddr)
			return i;

		i += len;
	}

	return routes_len;
}

void
flush_routes(int index, int routefd, int rdomain, uint8_t *routes,
    unsigned int routes_len, struct in_addr address)
{
	static int			 seqno;
	char				*lim, *buf, *next;
	struct rt_msghdr		*rtm;
	size_t				 len;
	ssize_t				 rlen;
	unsigned int			 pos;

	buf = get_routes(rdomain, &len);
	if (buf == NULL)
		return;

	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;
		if (rtm->rtm_index != index)
			continue;
		if (rtm->rtm_tableid != rdomain)
			continue;
		if ((rtm->rtm_flags & RTF_STATIC) == 0)
			continue;
		if ((rtm->rtm_flags & (RTF_LOCAL|RTF_BROADCAST)) != 0)
			continue;

		pos = route_pos(rtm, routes, routes_len, address);
		if (pos < routes_len) {
			discard_route(routes + pos, routes_len - pos);
			continue;
		}

		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno++;

		rlen = write(routefd, (char *)rtm, rtm->rtm_msglen);
		if (rlen == -1) {
			if (errno != ESRCH)
				log_warn("%s: write(RTM_DELETE)", log_procname);
		} else if (rlen < (int)rtm->rtm_msglen)
			log_warnx("%s: write(RTM_DELETE): %zd of %u bytes",
			    log_procname, rlen, rtm->rtm_msglen);
	}

	free(buf);
}

void
discard_route(uint8_t *routes, unsigned int routes_len)
{
	unsigned int		len;

	len = 1 + sizeof(struct in_addr) + (routes[0] + 7) / 8;
	memmove(routes, routes + len, routes_len - len);
	routes[routes_len - len] = CIDR_MAX_BITS + 1;
}

/*
 * add_route() adds a single route to the routing table.
 */
void
add_route(char *name, int rdomain, int routefd, struct in_addr dest,
    struct in_addr netmask, struct in_addr gateway, struct in_addr address,
    int flags)
{
	char			 destbuf[INET_ADDRSTRLEN];
	char			 maskbuf[INET_ADDRSTRLEN];
	struct iovec		 iov[5];
	struct sockaddr_in	 sockaddr_in[4];
	struct rt_msghdr	 rtm;
	int			 i, iovcnt = 0;

	memset(&rtm, 0, sizeof(rtm));
	rtm.rtm_index = if_nametoindex(name);
	if (rtm.rtm_index == 0)
		return;

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_ADD;
	rtm.rtm_tableid = rdomain;
	rtm.rtm_priority = RTP_NONE;
	rtm.rtm_flags = flags;

	iov[0].iov_base = &rtm;
	iov[0].iov_len = sizeof(rtm);

	memset(sockaddr_in, 0, sizeof(sockaddr_in));
	for (i = 0; i < 4; i++) {
		sockaddr_in[i].sin_len = sizeof(sockaddr_in[i]);
		sockaddr_in[i].sin_family = AF_INET;
		iov[i+1].iov_base = &sockaddr_in[i];
		iov[i+1].iov_len = sizeof(sockaddr_in[i]);
	}

	/* Order of sockaddr_in's is mandatory! */
	sockaddr_in[0].sin_addr = dest;
	sockaddr_in[1].sin_addr = gateway;
	sockaddr_in[2].sin_addr = netmask;
	sockaddr_in[3].sin_addr = address;
	if (address.s_addr == INADDR_ANY) {
		rtm.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK;
		iovcnt = 4;
	} else {
		rtm.rtm_addrs = RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFA;
		iovcnt = 5;
	}

	for (i = 0; i < iovcnt; i++)
		rtm.rtm_msglen += iov[i].iov_len;

	if (writev(routefd, iov, iovcnt) == -1) {
		if (errno != EEXIST || log_getverbose() != 0) {
			strlcpy(destbuf, inet_ntoa(dest), sizeof(destbuf));
			strlcpy(maskbuf, inet_ntoa(netmask),sizeof(maskbuf));
			log_warn("%s: add route %s/%s via %s", log_procname,
			    destbuf, maskbuf, inet_ntoa(gateway));
		}
	}
}

/*
 * set_routes() adds the routes contained in 'routes' to the routing table.
 */
void
set_routes(char *name, int index, int rdomain, int routefd, struct in_addr addr,
    struct in_addr addrmask, uint8_t *routes, unsigned int routes_len)
{
	const struct in_addr	 any = { INADDR_ANY };
	const struct in_addr	 broadcast = { INADDR_BROADCAST };
	struct in_addr		 dest, gateway, netmask;
	in_addr_t		 addrnet, gatewaynet;
	unsigned int		 i, len;

	flush_routes(index, routefd, rdomain, routes, routes_len, addr);

	addrnet = addr.s_addr & addrmask.s_addr;

	/* Add classless static routes. */
	i = 0;
	while (i < routes_len) {
		len = extract_route(&routes[i], routes_len - i,
		    &dest.s_addr, &netmask.s_addr, &gateway.s_addr);
		if (len == 0)
			return;
		i += len;

		if (gateway.s_addr == INADDR_ANY) {
			/*
			 * DIRECT ROUTE
			 *
			 * route add -net $dest -netmask $netmask -cloning
			 *     -iface $addr
			 */
			add_route(name, rdomain, routefd, dest, netmask,
			    addr, any, RTF_STATIC | RTF_CLONING);
		} else if (netmask.s_addr == INADDR_ANY) {
			/*
			 * DEFAULT ROUTE
			 */
			gatewaynet = gateway.s_addr & addrmask.s_addr;
			if (gatewaynet != addrnet) {
				/*
				 * DIRECT ROUTE TO DEFAULT GATEWAY
				 *
				 * route add -net $gateway
				 *	-netmask 255.255.255.255
				 *	-cloning -iface $addr
				 *
				 * If the default route gateway is not reachable
				 * via the IP assignment then add a cloning
				 * direct route for the gateway. Deals with
				 * weird configs seen in the wild.
				 *
				 * e.g. add the route if we were given a /32 IP
				 * assignment. a.k.a. "make Google Cloud DHCP
				 * work".
				 *
				 */
				add_route(name, rdomain, routefd, gateway,
				    broadcast, addr, any,
				    RTF_STATIC | RTF_CLONING);
			}

			if (memcmp(&gateway, &addr, sizeof(addr)) == 0) {
				/*
				 * DEFAULT ROUTE IS A DIRECT ROUTE
				 *
				 * route add default -iface $addr
				 */
				add_route(name, rdomain, routefd, any, any,
				    gateway, any, RTF_STATIC);
			} else {
				/*
				 * DEFAULT ROUTE IS VIA GATEWAY
				 *
				 * route add default $gateway -ifa $addr
				 *
				 */
				add_route(name, rdomain, routefd, any, any,
				    gateway, addr, RTF_STATIC | RTF_GATEWAY);
			}
		} else {
			/*
			 * NON-DIRECT, NON-DEFAULT ROUTE
			 *
			 * route add -net $dest -netmask $netmask $gateway
			 */
			add_route(name, rdomain, routefd, dest, netmask,
			    gateway, any, RTF_STATIC | RTF_GATEWAY);
		}
	}
}

/*
 * default_route_index() returns the interface index of the current
 * default route (a.k.a. 0.0.0.0/0).
 */
int
default_route_index(int rdomain, int routefd)
{
	struct pollfd		 fds[1];
	struct timespec		 now, stop, timeout;
	int			 nfds;
	struct iovec		 iov[3];
	struct sockaddr_in	 sin;
	struct {
		struct rt_msghdr	m_rtm;
		char			m_space[512];
	} m_rtmsg;
	pid_t			 pid;
	ssize_t			 len;
	int			 seq;

	memset(&m_rtmsg, 0, sizeof(m_rtmsg));
	m_rtmsg.m_rtm.rtm_version = RTM_VERSION;
	m_rtmsg.m_rtm.rtm_type = RTM_GET;
	m_rtmsg.m_rtm.rtm_tableid = rdomain;
	m_rtmsg.m_rtm.rtm_seq = seq = arc4random();
	m_rtmsg.m_rtm.rtm_addrs = RTA_DST | RTA_NETMASK;
	m_rtmsg.m_rtm.rtm_msglen = sizeof(struct rt_msghdr) +
	    2 * sizeof(struct sockaddr_in);

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;

	iov[0].iov_base = &m_rtmsg.m_rtm;
	iov[0].iov_len = sizeof(m_rtmsg.m_rtm);
	iov[1].iov_base = &sin;
	iov[1].iov_len = sizeof(sin);
	iov[2].iov_base = &sin;
	iov[2].iov_len = sizeof(sin);

	pid = getpid();
	clock_gettime(CLOCK_MONOTONIC, &now);
	timespecclear(&timeout);
	timeout.tv_sec = 3;
	timespecadd(&now, &timeout, &stop);

	if (writev(routefd, iov, 3) == -1) {
		if (errno == ESRCH)
			log_debug("%s: writev(RTM_GET) - no default route",
			    log_procname);
		else
			log_warn("%s: writev(RTM_GET)", log_procname);
		return 0;
	}

	for (;;) {
		clock_gettime(CLOCK_MONOTONIC, &now);
		if (timespeccmp(&stop, &now, <=))
			break;
		timespecsub(&stop, &now, &timeout);

		fds[0].fd = routefd;
		fds[0].events = POLLIN;
		nfds = ppoll(fds, 1, &timeout, NULL);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			log_warn("%s: ppoll(routefd)", log_procname);
			break;
		}
		if ((fds[0].revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
			log_warnx("%s: routefd: ERR|HUP|NVAL", log_procname);
			break;
		}
		if (nfds == 0 || (fds[0].revents & POLLIN) == 0)
			continue;

		len = read(routefd, &m_rtmsg, sizeof(m_rtmsg));
		if (len == -1) {
			log_warn("%s: read(RTM_GET)", log_procname);
			break;
		} else if (len == 0) {
			log_warnx("%s: read(RTM_GET): 0 bytes", log_procname);
			break;
		}

		if (m_rtmsg.m_rtm.rtm_version == RTM_VERSION &&
		    m_rtmsg.m_rtm.rtm_type == RTM_GET &&
		    m_rtmsg.m_rtm.rtm_pid == pid &&
		    m_rtmsg.m_rtm.rtm_seq == seq &&
		    (m_rtmsg.m_rtm.rtm_flags & RTF_UP) == RTF_UP) {
			if (m_rtmsg.m_rtm.rtm_errno != 0) {
				log_warnx("%s: read(RTM_GET): %s", log_procname,
				    strerror(m_rtmsg.m_rtm.rtm_errno));
				break;
			}
			return m_rtmsg.m_rtm.rtm_index;
		}
	}

	return 0;
}

/*
 * resolv_conf_tail() returns the contents of /etc/resolv.conf.tail, if
 * any. NULL is returned if there is no such file, the file is emtpy
 * or any errors are encounted in reading the file.
 */
char *
resolv_conf_tail(void)
{
	struct stat		 sb;
	const char		*tail_path = "/etc/resolv.conf.tail";
	char			*tailcontents = NULL;
	ssize_t			 tailn;
	int			 tailfd;

	tailfd = open(tail_path, O_RDONLY);
	if (tailfd == -1) {
		if (errno != ENOENT)
			log_warn("%s: open(%s)", log_procname, tail_path);
	} else if (fstat(tailfd, &sb) == -1) {
		log_warn("%s: fstat(%s)", log_procname, tail_path);
	} else if (sb.st_size > 0 && sb.st_size < LLONG_MAX) {
		tailcontents = calloc(1, sb.st_size + 1);
		if (tailcontents == NULL)
			fatal("%s contents", tail_path);
		tailn = read(tailfd, tailcontents, sb.st_size);
		if (tailn == -1)
			log_warn("%s: read(%s)", log_procname,
			    tail_path);
		else if (tailn == 0)
			log_warnx("%s: got no data from %s",
			    log_procname,tail_path);
		else if (tailn != sb.st_size)
			log_warnx("%s: short read of %s",
			    log_procname, tail_path);
		else {
			close(tailfd);
			return tailcontents;
		}

		close(tailfd);
		free(tailcontents);
	}

	return NULL;
}

/*
 * set_resolv_conf() creates a string that are the resolv.conf contents
 * that should be used when IMSG_WRITE_RESOLV_CONF messages are received.
 */
char *
set_resolv_conf(char *name, char *search, struct unwind_info *ns_info)
{
	char		*ns, *p, *tail;
	struct in_addr	 addr;
	unsigned int	 i;
	int		 rslt;

	ns = NULL;
	for (i = 0; i < ns_info->count; i++) {
		addr.s_addr = ns_info->ns[i];
		rslt = asprintf(&p, "%snameserver %s\n",
		    (ns == NULL) ? "" : ns, inet_ntoa(addr));
		if (rslt == -1)
			fatal("nameserver");
		free(ns);
		ns = p;
	}

	if (search == NULL && ns == NULL)
		return NULL;

	tail = resolv_conf_tail();

	rslt = asprintf(&p, "# Generated by %s dhclient\n%s%s%s", name,
	    (search == NULL) ? "" : search,
	    (ns == NULL) ? "" : ns,
	    (tail == NULL) ? "" : tail);
	if (rslt == -1)
		fatal("resolv.conf");

	free(tail);
	free(ns);

	return p;
}

/*
 * set_mtu() is the equivalent of
 *
 *      ifconfig <if> mtu <mtu>
 */
void
set_mtu(char *name, int ioctlfd, uint16_t mtu)
{
	struct ifreq	 ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));

	if (ioctl(ioctlfd, SIOCGIFMTU, &ifr) == -1) {
		log_warn("%s: SIOCGIFMTU", log_procname);
		return;
	}
	if (ifr.ifr_mtu == mtu)
		return;	/* Avoid unnecessary RTM_IFINFO! */

	ifr.ifr_mtu = mtu;
	if (ioctl(ioctlfd, SIOCSIFMTU, &ifr) == -1)
		log_warn("%s: SIOCSIFMTU %u", log_procname, mtu);
}

/*
 * extract_route() decodes the route pointed to by routes into its
 * {destination, netmask, gateway} and returns the number of bytes consumed
 * from routes.
 */
unsigned int
extract_route(uint8_t *routes, unsigned int routes_len, in_addr_t *dest,
    in_addr_t *netmask, in_addr_t *gateway)
{
	unsigned int	 bits, bytes, len;

	if (routes[0] > CIDR_MAX_BITS)
		return 0;

	bits = routes[0];
	bytes = (bits + 7) / 8;
	len = 1 + bytes + sizeof(*gateway);
	if (len > routes_len)
		return 0;

	if (dest != NULL)
		memcpy(dest, &routes[1], bytes);

	if (netmask != NULL) {
		if (bits == 0)
			*netmask = INADDR_ANY;
		else
			*netmask = htonl(0xffffffff << (CIDR_MAX_BITS - bits));
		if (dest != NULL)
			*dest &= *netmask;
	}

	if (gateway != NULL)
		memcpy(gateway, &routes[1 +  bytes], sizeof(*gateway));

	return len;
}

/*
 * [priv_]write_resolv_conf write out a new resolv.conf.
 */
void
write_resolv_conf(void)
{
	int	 rslt;

	rslt = imsg_compose(unpriv_ibuf, IMSG_WRITE_RESOLV_CONF,
	    0, 0, -1, NULL, 0);
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_WRITE_RESOLV_CONF)",
		    log_procname);
}

void
priv_write_resolv_conf(int index, int routefd, int rdomain, char *contents,
    int *lastidx)
{
	char		 ifname[IF_NAMESIZE];
	const char	*path = "/etc/resolv.conf";
	ssize_t		 n;
	size_t		 sz;
	int		 fd, retries, newidx;

	if (contents == NULL)
		return;

	retries = 0;
	do {
		newidx = default_route_index(rdomain, routefd);
		retries++;
	} while (newidx == 0 && retries < 3);

	if (newidx == 0) {
		log_debug("%s: %s not updated, no default route is UP",
		    log_procname, path);
		return;
	} else if (newidx != index) {
		*lastidx = newidx;
		if (if_indextoname(newidx, ifname) == NULL) {
			memset(ifname, 0, sizeof(ifname));
			strlcat(ifname, "<unknown>", sizeof(ifname));
		}
		log_debug("%s: %s not updated, default route on %s",
		    log_procname, path, ifname);
		return;
	} else if (newidx == *lastidx) {
		log_debug("%s: %s not updated, same as last write",
		    log_procname, path);
		return;
	}

	*lastidx = newidx;
	log_debug("%s: %s updated", log_procname, path);

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd == -1) {
		log_warn("%s: open(%s)", log_procname, path);
		return;
	}

	sz = strlen(contents);
	n = write(fd, contents, sz);
	if (n == -1)
		log_warn("%s: write(%s)", log_procname, path);
	else if ((size_t)n < sz)
		log_warnx("%s: write(%s): %zd of %zu bytes", log_procname,
		    path, n, sz);

	close(fd);
}

/*
 * [priv_]propose implements a proposal.
 */
void
propose(struct proposal *proposal)
{
	struct option_data	 opt;
	int			 rslt;

	log_debug("%s: proposing address %s netmask 0x%08x", log_procname,
	    inet_ntoa(proposal->address), ntohl(proposal->netmask.s_addr));

	opt.data = (u_int8_t *)proposal + sizeof(struct proposal);
	opt.len = proposal->routes_len;
	if (opt.len > 0)
		log_debug("%s: proposing static route(s) %s", log_procname,
		    pretty_print_option(DHO_CLASSLESS_STATIC_ROUTES, &opt, 0));

	opt.data += opt.len;
	opt.len = proposal->domains_len;
	if (opt.len > 0)
		log_debug("%s: proposing search domain(s) %s", log_procname,
		    pretty_print_option(DHO_DOMAIN_SEARCH, &opt, 0));

	opt.data += opt.len;
	opt.len = proposal->ns_len;
	if (opt.len > 0)
		log_debug("%s: proposing DNS server(s) %s", log_procname,
		    pretty_print_option(DHO_DOMAIN_NAME_SERVERS, &opt, 0));

	if (proposal->mtu != 0)
		log_debug("%s: proposing mtu %u", log_procname, proposal->mtu);

	rslt = imsg_compose(unpriv_ibuf, IMSG_PROPOSE, 0, 0, -1, proposal,
	    sizeof(*proposal) + proposal->routes_len +
	    proposal->domains_len + proposal->ns_len);
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_PROPOSE)", log_procname);
}

void
priv_propose(char *name, int ioctlfd, struct proposal *proposal,
    size_t sz, char **resolv_conf, int routefd, int rdomain, int index,
    int *lastidx)
{
	struct unwind_info	 unwind_info;
	uint8_t			*dns, *domains, *routes;
	char			*search = NULL;
	int			 rslt;

	if (sz != proposal->routes_len + proposal->domains_len +
	    proposal->ns_len) {
		log_warnx("%s: bad IMSG_PROPOSE data", log_procname);
		return;
	}

	routes = (uint8_t *)proposal + sizeof(struct proposal);
	domains = routes + proposal->routes_len;
	dns = domains + proposal->domains_len;

	memset(&unwind_info, 0, sizeof(unwind_info));
	if (proposal->ns_len >= sizeof(in_addr_t)) {
		if (proposal->ns_len > sizeof(unwind_info.ns)) {
			memcpy(unwind_info.ns, dns, sizeof(unwind_info.ns));
			unwind_info.count = sizeof(unwind_info.ns) /
			    sizeof(in_addr_t);
		} else {
			memcpy(unwind_info.ns, dns, proposal->ns_len);
			unwind_info.count = proposal->ns_len /
			    sizeof(in_addr_t);
		}
	}

	if (proposal->domains_len > 0) {
		rslt = asprintf(&search, "search %.*s\n",
		    proposal->domains_len, domains);
		if (rslt == -1)
			search = NULL;
	}

	free(*resolv_conf);
	*resolv_conf = set_resolv_conf(name, search, &unwind_info);
	free(search);

	if (proposal->mtu != 0) {
		if (proposal->mtu < 68)
			log_warnx("%s: mtu size %d < 68: ignored", log_procname,
			    proposal->mtu);
		else
			set_mtu(name, ioctlfd, proposal->mtu);
	}

	set_address(name, ioctlfd, proposal->address, proposal->netmask);

	set_routes(name, index, rdomain, routefd, proposal->address,
	    proposal->netmask, routes, proposal->routes_len);

	*lastidx = 0;
	priv_write_resolv_conf(index, routefd, rdomain, *resolv_conf, lastidx);
}

/*
 * [priv_]revoke_proposal de-configures a proposal.
 */
void
revoke_proposal(struct proposal *proposal)
{
	int			 rslt;

	if (proposal == NULL)
		return;

	rslt = imsg_compose(unpriv_ibuf, IMSG_REVOKE, 0, 0, -1, proposal,
	    sizeof(*proposal));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_REVOKE)", log_procname);
}

void
priv_revoke_proposal(char *name, int ioctlfd, struct proposal *proposal,
    char **resolv_conf)
{
	free(*resolv_conf);
	*resolv_conf = NULL;

	delete_address(name, ioctlfd, proposal->address);
}

/*
 * [priv_]tell_unwind sends out inforation unwind may be intereted in.
 */
void
tell_unwind(struct unwind_info *unwind_info, int ifi_flags)
{
	struct	unwind_info	 	 noinfo;
	int				 rslt;

	if ((ifi_flags & IFI_IN_CHARGE) == 0)
		return;

	if (unwind_info != NULL)
		rslt = imsg_compose(unpriv_ibuf, IMSG_TELL_UNWIND, 0, 0, -1,
		    unwind_info, sizeof(*unwind_info));
	else {
		memset(&noinfo, 0, sizeof(noinfo));
		rslt = imsg_compose(unpriv_ibuf, IMSG_TELL_UNWIND, 0, 0, -1,
		    &noinfo, sizeof(noinfo));
	}

	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_TELL_UNWIND)", log_procname);
}

void
priv_tell_unwind(int index, int routefd, int rdomain,
    struct unwind_info *unwind_info)
{
	struct rt_msghdr		 rtm;
	struct sockaddr_rtdns		 rtdns;
	struct iovec			 iov[3];
	long				 pad = 0;
	int				 iovcnt = 0, padlen;

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_PROPOSAL;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_tableid = rdomain;
	rtm.rtm_index = index;
	rtm.rtm_seq = arc4random();
	rtm.rtm_priority = RTP_PROPOSAL_DHCLIENT;
	rtm.rtm_addrs = RTA_DNS;
	rtm.rtm_flags = RTF_UP;

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	memset(&rtdns, 0, sizeof(rtdns));
	rtdns.sr_family = AF_INET;

	rtdns.sr_len = 2 + unwind_info->count * sizeof(in_addr_t);
	memcpy(rtdns.sr_dns, unwind_info->ns,
	    unwind_info->count * sizeof(in_addr_t));

	iov[iovcnt].iov_base = &rtdns;
	iov[iovcnt++].iov_len = sizeof(rtdns);
	rtm.rtm_msglen += sizeof(rtdns);
	padlen = ROUNDUP(sizeof(rtdns)) - sizeof(rtdns);
	if (padlen > 0) {
		iov[iovcnt].iov_base = &pad;
		iov[iovcnt++].iov_len = padlen;
		rtm.rtm_msglen += padlen;
	}

	if (writev(routefd, iov, iovcnt) == -1)
		log_warn("failed to tell unwind");
}
