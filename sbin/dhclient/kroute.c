/*	$OpenBSD: kroute.c,v 1.172 2019/11/22 22:45:52 krw Exp $	*/

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

int		 delete_addresses(char *, int, struct in_addr, struct in_addr);
void		 set_address(char *, int, struct in_addr, struct in_addr);
void		 delete_address(char *, int, struct in_addr);

char		*get_routes(int, size_t *);
void		 get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
unsigned int	 route_in_rtstatic(struct rt_msghdr *, uint8_t *, unsigned int,
    struct in_addr);
void		 flush_routes(int, int, int, uint8_t *, unsigned int,
    struct in_addr);
void		 add_route(char *, int, int, struct in_addr, struct in_addr,
		    struct in_addr, struct in_addr, int);
void		 set_routes(char *, int, int, int, struct in_addr,
    struct in_addr, uint8_t *, unsigned int);

int		 default_route_index(int, int);
char		*resolv_conf_tail(void);
char		*set_resolv_conf(char *, uint8_t *, unsigned int,
    uint8_t *, unsigned int);

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
 * route_in_rtstatic() finds the position of the route in *rtm withing
 * the list of routes in rtstatic.
 *
 * If the route is not contained in rtstatic, return rtstatic_len.
 */
unsigned int
route_in_rtstatic(struct rt_msghdr *rtm, uint8_t *rtstatic,
    unsigned int rtstatic_len, struct in_addr ifa)
{
	struct sockaddr		*rti_info[RTAX_MAX];
	struct sockaddr		*dst, *netmask, *gateway;
	in_addr_t		 dstaddr, netmaskaddr, gatewayaddr;
	in_addr_t		 rtstaticdstaddr, rtstaticnetmaskaddr;
	in_addr_t		 rtstaticgatewayaddr;
	unsigned int		 i, len;

	get_rtaddrs(rtm->rtm_addrs,
	    (struct sockaddr *)((char *)(rtm) + rtm->rtm_hdrlen),
	    rti_info);

	dst = rti_info[RTAX_DST];
	netmask = rti_info[RTAX_NETMASK];
	gateway = rti_info[RTAX_GATEWAY];

	if (dst == NULL || netmask == NULL || gateway == NULL)
		return rtstatic_len;

	if (dst->sa_family != AF_INET || netmask->sa_family != AF_INET ||
	    gateway->sa_family != AF_INET)
		return rtstatic_len;

	dstaddr = ((struct sockaddr_in *)dst)->sin_addr.s_addr;
	netmaskaddr = ((struct sockaddr_in *)netmask)->sin_addr.s_addr;
	gatewayaddr = ((struct sockaddr_in *)gateway)->sin_addr.s_addr;

	dstaddr &= netmaskaddr;
	i = 0;
	while (i < rtstatic_len)  {
		len = extract_classless_route(&rtstatic[i], rtstatic_len - i,
		    &rtstaticdstaddr, &rtstaticnetmaskaddr,
		    &rtstaticgatewayaddr);
		if (len == 0)
			break;

		/* Direct route in rtstatic:
		 *
		 * dst=1.2.3.4 netmask=255.255.255.255 gateway=0.0.0.0
		 *
		 * direct route in rtm:
		 *
		 * dst=1.2.3.4 netmask=255.255.255.255 gateway = ifa
		 *
		 * So replace 0.0.0.0 with ifa for comparison.
		 */
		if (rtstaticgatewayaddr == INADDR_ANY)
			rtstaticgatewayaddr = ifa.s_addr;
		rtstaticdstaddr &= rtstaticnetmaskaddr;

		if (dstaddr == rtstaticdstaddr &&
		    netmaskaddr == rtstaticnetmaskaddr &&
		    gatewayaddr == rtstaticgatewayaddr)
			return i;

		i += len;
	}

	return rtstatic_len;
}

/*
 * flush_routes() does the equivalent of
 *
 *	route -q -T $rdomain -n flush -inet -iface $interface
 *	arp -dan
 */
void
flush_routes(int index, int routefd, int rdomain, uint8_t *rtstatic,
    unsigned int rtstatic_len, struct in_addr ifa)
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

		/* Don't bother deleting a route we're going to add. */
		pos = route_in_rtstatic(rtm, rtstatic, rtstatic_len, ifa);
		if (pos < rtstatic_len)
			continue;

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

/*
 * add_route() adds a single route to the routing table.
 */
void
add_route(char *name, int rdomain, int routefd, struct in_addr dest,
    struct in_addr netmask, struct in_addr gateway, struct in_addr ifa,
    int flags)
{
	char			 destbuf[INET_ADDRSTRLEN];
	char			 maskbuf[INET_ADDRSTRLEN];
	struct iovec		 iov[5];
	struct rt_msghdr	 rtm;
	struct sockaddr_in	 sadest, sagateway, samask, saifa;
	int			 index, iovcnt = 0;

	index = if_nametoindex(name);
	if (index == 0)
		return;

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_ADD;
	rtm.rtm_index = index;
	rtm.rtm_tableid = rdomain;
	rtm.rtm_priority = RTP_NONE;
	rtm.rtm_addrs =	RTA_DST | RTA_NETMASK | RTA_GATEWAY;
	rtm.rtm_flags = flags;

	rtm.rtm_msglen = sizeof(rtm);
	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	/* Add the destination address. */
	memset(&sadest, 0, sizeof(sadest));
	sadest.sin_len = sizeof(sadest);
	sadest.sin_family = AF_INET;
	sadest.sin_addr.s_addr = dest.s_addr;

	rtm.rtm_msglen += sizeof(sadest);
	iov[iovcnt].iov_base = &sadest;
	iov[iovcnt++].iov_len = sizeof(sadest);

	/* Add the gateways address. */
	memset(&sagateway, 0, sizeof(sagateway));
	sagateway.sin_len = sizeof(sagateway);
	sagateway.sin_family = AF_INET;
	sagateway.sin_addr.s_addr = gateway.s_addr;

	rtm.rtm_msglen += sizeof(sagateway);
	iov[iovcnt].iov_base = &sagateway;
	iov[iovcnt++].iov_len = sizeof(sagateway);

	/* Add the network mask. */
	memset(&samask, 0, sizeof(samask));
	samask.sin_len = sizeof(samask);
	samask.sin_family = AF_INET;
	samask.sin_addr.s_addr = netmask.s_addr;

	rtm.rtm_msglen += sizeof(samask);
	iov[iovcnt].iov_base = &samask;
	iov[iovcnt++].iov_len = sizeof(samask);

	if (ifa.s_addr != INADDR_ANY) {
		/* Add the ifa */
		memset(&saifa, 0, sizeof(saifa));
		saifa.sin_len = sizeof(saifa);
		saifa.sin_family = AF_INET;
		saifa.sin_addr.s_addr = ifa.s_addr;

		rtm.rtm_msglen += sizeof(saifa);
		iov[iovcnt].iov_base = &saifa;
		iov[iovcnt++].iov_len = sizeof(saifa);
		rtm.rtm_addrs |= RTA_IFA;
	}

	if (writev(routefd, iov, iovcnt) == -1) {
		if (errno != EEXIST || log_getverbose() != 0) {
			strlcpy(destbuf, inet_ntoa(dest),
			    sizeof(destbuf));
			strlcpy(maskbuf, inet_ntoa(netmask),
			    sizeof(maskbuf));
			log_warn("%s: add route %s/%s via %s", log_procname,
			    destbuf, maskbuf, inet_ntoa(gateway));
		}
	}
}

/*
 * set_routes() adds the routes contained in rtstatic to the routing table.
 */
void
set_routes(char *name, int index, int rdomain, int routefd, struct in_addr addr,
    struct in_addr addrmask, uint8_t *rtstatic, unsigned int rtstatic_len)
{
	const struct in_addr	 any = { INADDR_ANY };
	const struct in_addr	 broadcast = { INADDR_BROADCAST };
	struct in_addr		 dest, gateway, netmask;
	in_addr_t		 addrnet, gatewaynet;
	unsigned int		 i, len;

	if (rtstatic_len <= RTLEN)
		flush_routes(index, routefd, rdomain, rtstatic, rtstatic_len, addr);

	addrnet = addr.s_addr & addrmask.s_addr;

	/* Add classless static routes. */
	i = 0;
	while (i < rtstatic_len) {
		len = extract_classless_route(&rtstatic[i], rtstatic_len - i,
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
	time_t			 start_time, cur_time;
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
	if (time(&start_time) == -1)
		fatal("start time");

	if (writev(routefd, iov, 3) == -1) {
		if (errno == ESRCH)
			log_debug("%s: writev(RTM_GET) - no default route",
			    log_procname);
		else
			log_warn("%s: writev(RTM_GET)", log_procname);
		return 0;
	}

	do {
		if (time(&cur_time) == -1)
			fatal("current time");
		fds[0].fd = routefd;
		fds[0].events = POLLIN;
		nfds = poll(fds, 1, 3);
		if (nfds == -1) {
			if (errno == EINTR)
				continue;
			log_warn("%s: poll(routefd)", log_procname);
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
		    m_rtmsg.m_rtm.rtm_seq == seq) {
			if (m_rtmsg.m_rtm.rtm_errno != 0) {
				log_warnx("%s: read(RTM_GET): %s", log_procname,
				    strerror(m_rtmsg.m_rtm.rtm_errno));
				break;
			}
			return m_rtmsg.m_rtm.rtm_index;
		}
	} while ((cur_time - start_time) <= 3);

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
set_resolv_conf(char *name, uint8_t *rtsearch, unsigned int rtsearch_len,
    uint8_t *rtdns, unsigned int rtdns_len)
{
	char		*dn, *nss[MAXNS], *contents, *courtesy, *resolv_tail;
	struct in_addr	*addr;
	size_t		 len;
	unsigned int	 i, servers;
	int		 rslt;

	memset(nss, 0, sizeof(nss));
	len = 0;

	if (rtsearch_len != 0) {
		rslt = asprintf(&dn, "search %.*s\n", rtsearch_len,
		    rtsearch);
		if (rslt == -1)
			dn = NULL;
	} else
		dn = strdup("");
	if (dn == NULL)
		fatal("domainname");
	len += strlen(dn);

	if (rtdns_len != 0) {
		addr = (struct in_addr *)rtdns;
		servers = rtdns_len / sizeof(addr->s_addr);
		if (servers > MAXNS)
			servers = MAXNS;
		for (i = 0; i < servers; i++) {
			rslt = asprintf(&nss[i], "nameserver %s\n",
			    inet_ntoa(*addr));
			if (rslt == -1)
				fatal("nameserver");
			len += strlen(nss[i]);
			addr++;
		}
	}

	/*
	 * XXX historically dhclient-script did not overwrite
	 *     resolv.conf when neither search nor dns info
	 *     was provided. Is that really what we want?
	 */
	if (len > 0) {
		resolv_tail = resolv_conf_tail();
		if (resolv_tail != NULL)
			len += strlen(resolv_tail);
	}
	if (len == 0) {
		free(dn);
		return NULL;
	}

	rslt = asprintf(&courtesy, "# Generated by %s dhclient\n", name);
	if (rslt == -1)
		fatal("resolv.conf courtesy line");
	len += strlen(courtesy);

	len++; /* Need room for terminating NUL. */
	contents = calloc(1, len);
	if (contents == NULL)
		fatal("resolv.conf contents");

	strlcat(contents, courtesy, len);
	free(courtesy);

	strlcat(contents, dn, len);
	free(dn);

	for (i = 0; i < MAXNS; i++) {
		if (nss[i] != NULL) {
			strlcat(contents, nss[i], len);
			free(nss[i]);
		}
	}

	if (resolv_tail != NULL) {
		strlcat(contents, resolv_tail, len);
		free(resolv_tail);
	}

	return contents;
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
 * extract_classless_route() extracts the encoded route pointed to by rtstatic.
 */
unsigned int
extract_classless_route(uint8_t *rtstatic, unsigned int rtstatic_len,
    in_addr_t *dest, in_addr_t *netmask, in_addr_t *gateway)
{
	unsigned int	 bits, bytes, len;

	if (rtstatic[0] > 32)
		return 0;

	bits = rtstatic[0];
	bytes = (bits + 7) / 8;
	len = 1 + bytes + sizeof(*gateway);
	if (len > rtstatic_len)
		return 0;

	if (dest != NULL)
		memcpy(dest, &rtstatic[1], bytes);

	if (netmask != NULL) {
		if (bits == 0)
			*netmask = INADDR_ANY;
		else
			*netmask = htonl(0xffffffff << (32 - bits));
		if (dest != NULL)
			*dest &= *netmask;
	}

	if (gateway != NULL)
		memcpy(gateway, &rtstatic[1 +  bytes], sizeof(*gateway));

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

	if (newidx != index) {
		*lastidx = newidx;
		log_debug("%s priv_write_resolv_conf: not my problem "
		    "(%d != %d)", log_procname, newidx, index);
		return;
	} else if (newidx == *lastidx) {
		log_debug("%s priv_write_resolv_conf: already written",
		    log_procname);
		return;
	} else {
		*lastidx = newidx;
		log_debug("%s priv_write_resolv_conf: writing", log_procname);
	}

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
	struct	imsg_propose	 imsg;
	int			 rslt;

	memcpy(&imsg.proposal, proposal, sizeof(imsg.proposal));

	rslt = imsg_compose(unpriv_ibuf, IMSG_PROPOSE, 0, 0, -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_PROPOSE)", log_procname);
}

void
priv_propose(char *name, int ioctlfd, struct imsg_propose *imsg,
    char **resolv_conf, int routefd, int rdomain, int index)
{
	struct proposal		*proposal = &imsg->proposal;
	struct ifreq		 ifr;

	memset(&ifr, 0, sizeof(ifr));
	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	if (ioctl(ioctlfd, SIOCGIFXFLAGS, (caddr_t)&ifr) < 0)
		fatal("SIOGIFXFLAGS");
	if ((ifr.ifr_flags & IFXF_AUTOCONF4) == 0)
		return;

	free(*resolv_conf);
	*resolv_conf = set_resolv_conf(name,
	    proposal->rtsearch,
	    proposal->rtsearch_len,
	    proposal->rtdns,
	    proposal->rtdns_len);

	if ((proposal->inits & RTV_MTU) != 0) {
		if (proposal->mtu < 68)
			log_warnx("%s: mtu size %u < 68: ignored", log_procname,
			    proposal->mtu);
		else
			set_mtu(name, ioctlfd, proposal->mtu);
	}

	set_address(name, ioctlfd, proposal->ifa, proposal->netmask);

	set_routes(name, index, rdomain, routefd, proposal->ifa,
	    proposal->netmask, proposal->rtstatic, proposal->rtstatic_len);
}

/*
 * [priv_]revoke_proposal de-configures a proposal.
 */
void
revoke_proposal(struct proposal *proposal)
{
	struct	imsg_revoke	 imsg;
	int			 rslt;

	if (proposal == NULL)
		return;

	memcpy(&imsg.proposal, proposal, sizeof(imsg.proposal));

	rslt = imsg_compose(unpriv_ibuf, IMSG_REVOKE, 0, 0, -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_REVOKE)", log_procname);
}

void
priv_revoke_proposal(char *name, int ioctlfd, struct imsg_revoke *imsg,
    char **resolv_conf)
{
	struct proposal		*proposal = &imsg->proposal;

	free(*resolv_conf);
	*resolv_conf = NULL;

	delete_address(name, ioctlfd, proposal->ifa);
}

/*
 * [priv_]tell_unwind sends out inforation unwind may be intereted in.
 */
void
tell_unwind(struct unwind_info *unwind_info, int ifi_flags)
{
	struct	imsg_tell_unwind	 imsg;
	int				 rslt;

	if ((ifi_flags & IFI_AUTOCONF) == 0 ||
	    (ifi_flags & IFI_IN_CHARGE) == 0)
		return;

	memset(&imsg, 0, sizeof(imsg));
	if (unwind_info != NULL)
		memcpy(&imsg.unwind_info, unwind_info, sizeof(imsg.unwind_info));

	rslt = imsg_compose(unpriv_ibuf, IMSG_TELL_UNWIND, 0, 0, -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_TELL_UNWIND)", log_procname);
}

void
priv_tell_unwind(int index, int routefd, int rdomain, struct imsg_tell_unwind *imsg)
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

	rtdns.sr_len = 2 + imsg->unwind_info.count * sizeof(in_addr_t);
	memcpy(rtdns.sr_dns, imsg->unwind_info.ns,
	    imsg->unwind_info.count * sizeof(in_addr_t));

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
