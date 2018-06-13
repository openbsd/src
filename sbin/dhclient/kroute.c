/*	$OpenBSD: kroute.c,v 1.156 2018/06/13 01:37:54 krw Exp $	*/

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

void		 get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		 add_route(struct in_addr, struct in_addr, struct in_addr, int);
void		 flush_routes(uint8_t *, unsigned int);
int		 delete_addresses(char *, struct in_addr, struct in_addr);
char		*get_routes(int, size_t *);
unsigned int	 route_in_rtstatic(struct rt_msghdr *, uint8_t *, unsigned int);

char *
get_routes(int rdomain, size_t *len)
{
	int		 mib[7];
	char		*buf, *bufp, *errmsg = NULL;
	size_t		 needed;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_STATIC | RTF_GATEWAY | RTF_LLINFO;
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
 * [priv_]flush_routes do the equivalent of
 *
 *	route -q -T $rdomain -n flush -inet -iface $interface
 *	arp -dan
 */
void
flush_routes(uint8_t *rtstatic, unsigned int rtstatic_len)
{
	struct	imsg_flush_routes	 imsg;
	int				 rslt;

	if (rtstatic_len > sizeof(imsg.rtstatic))
		return;

	imsg.rtstatic_len = rtstatic_len;
	memcpy(&imsg.rtstatic, rtstatic, rtstatic_len);

	rslt = imsg_compose(unpriv_ibuf, IMSG_FLUSH_ROUTES, 0, 0, -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_FLUSH_ROUTES)", log_procname);
}

void
priv_flush_routes(int index, int routefd, int rdomain,
    struct imsg_flush_routes *imsg)
{
	static int			 seqno;
	char				*lim, *buf = NULL, *next;
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
		if ((rtm->rtm_flags & (RTF_GATEWAY|RTF_STATIC|RTF_LLINFO)) == 0)
			continue;
		if ((rtm->rtm_flags & (RTF_LOCAL|RTF_BROADCAST)) != 0)
			continue;

		/* Don't bother deleting a route we're going to add. */
		pos = route_in_rtstatic(rtm, imsg->rtstatic,
		    imsg->rtstatic_len);
		if (pos < imsg->rtstatic_len)
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

void
set_routes(struct in_addr addr, struct in_addr addrmask, uint8_t *rtstatic,
    unsigned int rtstatic_len)
{
	const struct in_addr	 any = { INADDR_ANY };
	const struct in_addr	 broadcast = { INADDR_BROADCAST };
	struct in_addr		 dest, gateway, netmask;
	in_addr_t		 addrnet, gatewaynet;
	unsigned int		 i, len;

	flush_routes(rtstatic, rtstatic_len);

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
			add_route(dest, netmask, addr,
			    RTF_STATIC | RTF_CLONING);
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
				add_route(gateway, broadcast, addr,
				    RTF_STATIC | RTF_CLONING);
			}

			if (memcmp(&gateway, &addr, sizeof(addr)) == 0) {
				/*
				 * DEFAULT ROUTE IS A DIRECT ROUTE
				 *
				 * route add default -iface $addr
				 */
				add_route(any, any, gateway, RTF_STATIC);
			} else {
				/*
				 * DEFAULT ROUTE IS VIA GATEWAY
				 *
				 * route add default $gateway
				 */
				add_route(any, any, gateway,
				    RTF_STATIC | RTF_GATEWAY);
			}
		} else {
			/*
			 * NON-DIRECT, NON-DEFAULT ROUTE
			 *
			 * route add -net $dest -netmask $netmask $gateway
			 */
			add_route(dest, netmask, gateway,
			    RTF_STATIC | RTF_GATEWAY);
		}
	}
}

/*
 * [priv_]add_route() add a single route to the routing table.
 */
void
add_route(struct in_addr dest, struct in_addr netmask, struct in_addr gateway,
    int flags)
{
	struct imsg_add_route	 imsg;
	int			 rslt;

	imsg.dest = dest;
	imsg.gateway = gateway;
	imsg.netmask = netmask;
	imsg.flags = flags;

	rslt = imsg_compose(unpriv_ibuf, IMSG_ADD_ROUTE, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_ADD_ROUTE)", log_procname);
}

void
priv_add_route(char *name, int rdomain, int routefd,
    struct imsg_add_route *imsg)
{
	char			 destbuf[INET_ADDRSTRLEN];
	char			 maskbuf[INET_ADDRSTRLEN];
	struct iovec		 iov[5];
	struct rt_msghdr	 rtm;
	struct sockaddr_in	 dest, gateway, mask;
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
	rtm.rtm_flags = imsg->flags;

	rtm.rtm_msglen = sizeof(rtm);
	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	/* Add the destination address. */
	memset(&dest, 0, sizeof(dest));
	dest.sin_len = sizeof(dest);
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = imsg->dest.s_addr;

	rtm.rtm_msglen += sizeof(dest);
	iov[iovcnt].iov_base = &dest;
	iov[iovcnt++].iov_len = sizeof(dest);

	/* Add the gateways address. */
	memset(&gateway, 0, sizeof(gateway));
	gateway.sin_len = sizeof(gateway);
	gateway.sin_family = AF_INET;
	gateway.sin_addr.s_addr = imsg->gateway.s_addr;

	rtm.rtm_msglen += sizeof(gateway);
	iov[iovcnt].iov_base = &gateway;
	iov[iovcnt++].iov_len = sizeof(gateway);

	/* Add the network mask. */
	memset(&mask, 0, sizeof(mask));
	mask.sin_len = sizeof(mask);
	mask.sin_family = AF_INET;
	mask.sin_addr.s_addr = imsg->netmask.s_addr;

	rtm.rtm_msglen += sizeof(mask);
	iov[iovcnt].iov_base = &mask;
	iov[iovcnt++].iov_len = sizeof(mask);

	if (writev(routefd, iov, iovcnt) == -1) {
		if (errno != EEXIST || log_getverbose() != 0) {
			strlcpy(destbuf, inet_ntoa(imsg->dest),
			    sizeof(destbuf));
			strlcpy(maskbuf, inet_ntoa(imsg->netmask),
			    sizeof(maskbuf));
			log_warn("%s: add route %s/%s via %s", log_procname,
			    destbuf, maskbuf, inet_ntoa(imsg->gateway));
		}
	}
}

/*
 * delete_addresses() deletes existing inet addresses on the named interface,
 * leaving in place newaddr/newnetmask.
 *
 * Return 1 if newaddr/newnetmask is seen while deleting addresses, 0 otherwise.
 */
int
delete_addresses(char *name, struct in_addr newaddr, struct in_addr newnetmask)
{
	struct in_addr		 addr, netmask;
	struct ifaddrs		*ifap, *ifa;
	int			 found = 0;

	if (getifaddrs(&ifap) != 0)
		fatal("getifaddrs");

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
		    netmask.s_addr == newnetmask.s_addr)
			found = 1;
		else
			delete_address(addr);
	}

	freeifaddrs(ifap);
	return (found);
}

/*
 * [priv_]delete_address is the equivalent of
 *
 *	ifconfig <ifname> inet <addr> delete
 */
void
delete_address(struct in_addr addr)
{
	struct imsg_delete_address	 imsg;
	int				 rslt;

	imsg.addr = addr;

	rslt = imsg_compose(unpriv_ibuf, IMSG_DELETE_ADDRESS, 0, 0 , -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_DELETE_ADDRESS)", log_procname);
}

void
priv_delete_address(char *name, int ioctlfd, struct imsg_delete_address *imsg)
{
	struct ifaliasreq	 ifaliasreq;
	struct sockaddr_in	*in;

	/*
	 * Delete specified address on specified interface.
	 */

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, name, sizeof(ifaliasreq.ifra_name));

	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = imsg->addr.s_addr;

	/* SIOCDIFADDR will result in a RTM_DELADDR message we must catch! */
	if (ioctl(ioctlfd, SIOCDIFADDR, &ifaliasreq) == -1) {
		if (errno != EADDRNOTAVAIL)
			log_warn("%s: SIOCDIFADDR %s", log_procname,
			    inet_ntoa(imsg->addr));
	}
}

/*
 * [priv_]set_mtu is the equivalent of
 *
 *      ifconfig <if> mtu <mtu>
 */
void
set_mtu(int inits, uint16_t mtu)
{
	struct imsg_set_mtu	 imsg;
	int			 rslt;

	if ((inits & RTV_MTU) == 0)
		return;

	if (mtu < 68) {
		log_warnx("%s: mtu size %u < 68: ignored", log_procname, mtu);
		return;
	}
	imsg.mtu = mtu;

	rslt = imsg_compose(unpriv_ibuf, IMSG_SET_MTU, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_SET_MTU)", log_procname);
}

void
priv_set_mtu(char *name, int ioctlfd, struct imsg_set_mtu *imsg)
{
	struct ifreq	 ifr;

	memset(&ifr, 0, sizeof(ifr));

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = imsg->mtu;

	if (ioctl(ioctlfd, SIOCSIFMTU, &ifr) == -1)
		log_warn("%s: SIOCSIFMTU %d", log_procname, imsg->mtu);
}

/*
 * [priv_]set_address is the equivalent of
 *
 *	ifconfig <if> inet <addr> netmask <mask> broadcast <addr>
 */
void
set_address(char *name, struct in_addr addr, struct in_addr netmask)
{
	struct imsg_set_address	 imsg;
	int			 rslt;

	/* Deleting the addresses also clears out arp entries. */
	if (delete_addresses(name, addr, netmask) != 0)
		return;

	imsg.addr = addr;
	imsg.mask = netmask;

	rslt = imsg_compose(unpriv_ibuf, IMSG_SET_ADDRESS, 0, 0, -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_SET_ADDRESS)", log_procname);
}

void
priv_set_address(char *name, int ioctlfd, struct imsg_set_address *imsg)
{
	struct ifaliasreq	 ifaliasreq;
	struct sockaddr_in	*in;

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, name, sizeof(ifaliasreq.ifra_name));

	/* The actual address in ifra_addr. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = imsg->addr.s_addr;

	/* And the netmask in ifra_mask. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_mask;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_mask);
	memcpy(&in->sin_addr, &imsg->mask, sizeof(in->sin_addr));

	/* No need to set broadcast address. Kernel can figure it out. */

	if (ioctl(ioctlfd, SIOCAIFADDR, &ifaliasreq) == -1)
		log_warn("%s: SIOCAIFADDR %s", log_procname,
		    inet_ntoa(imsg->addr));
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
priv_write_resolv_conf(char *contents)
{
	const char	*path = "/etc/resolv.conf";
	ssize_t		 n;
	size_t		 sz;
	int		 fd;

	if (contents == NULL)
		return;

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
 * default_route_index returns the index of the interface which the
 * default route (a.k.a. 0.0.0.0/0) is on.
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
 * set_resolv_conf creates a string that are the resolv.conf contents
 * that should be used when the interface is determined to be the one to
 * create /etc/resolv.conf
 */
void
set_resolv_conf(char *name, uint8_t *rtsearch, unsigned int rtsearch_len,
    uint8_t *rtdns, unsigned int rtdns_len)
{
	char		*dn, *nss[MAXNS], *contents, *courtesy;
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
	if (len > 0 && config->resolv_tail != NULL)
		len += strlen(config->resolv_tail);

	if (len == 0) {
		free(dn);
		contents = NULL;
		goto done;
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

	if (config->resolv_tail != NULL)
		strlcat(contents, config->resolv_tail, len);

done:
	rslt = imsg_compose(unpriv_ibuf, IMSG_SET_RESOLV_CONF,
	    0, 0, -1, contents, len);
	if (rslt == -1)
		log_warn("%s: imsg_compose(IMSG_SET_RESOLV_CONF)",
		    log_procname);
}

/*
 * get_rtaddrs populates the rti_info with pointers to the
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

unsigned int
route_in_rtstatic(struct rt_msghdr *rtm, uint8_t *rtstatic,
    unsigned int rtstatic_len)
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

		if (dstaddr == rtstaticdstaddr &&
		    netmaskaddr == rtstaticnetmaskaddr &&
		    gatewayaddr == rtstaticgatewayaddr)
			return i;

		i += len;
	}

	return rtstatic_len;
}
