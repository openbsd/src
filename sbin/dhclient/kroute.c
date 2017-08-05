/*	$OpenBSD: kroute.c,v 1.131 2017/08/05 13:39:17 krw Exp $	*/

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

/*
 * flush_unpriv_ibuf makes sure queued messages are delivered to the
 * imsg socket.
 */
void
flush_unpriv_ibuf(const char *who)
{
	while (unpriv_ibuf->w.queued) {
		if (msgbuf_write(&unpriv_ibuf->w) <= 0) {
			if (errno == EAGAIN)
				break;
			if (quit == 0)
				quit = INTERNALSIG;
			if (errno != EPIPE && errno != 0)
				log_warn("%s: msgbuf_write", who);
			break;
		}
	}
}

int	create_route_label(struct sockaddr_rtlabel *);
int	check_route_label(struct sockaddr_rtlabel *);
void	populate_rti_info(struct sockaddr **, struct rt_msghdr *);
void	delete_route(int, struct rt_msghdr *);
void	add_route(struct in_addr, struct in_addr, struct in_addr, int);
void	flush_routes(void);
int	delete_addresses(char *, struct in_addr, struct in_addr);
char	*get_routes(int, int, size_t *);

#define	ROUTE_LABEL_NONE		1
#define	ROUTE_LABEL_NOT_DHCLIENT	2
#define	ROUTE_LABEL_DHCLIENT_OURS	3
#define	ROUTE_LABEL_DHCLIENT_UNKNOWN	4
#define	ROUTE_LABEL_DHCLIENT_LIVE	5
#define	ROUTE_LABEL_DHCLIENT_DEAD	6

char *
get_routes(int rdomain, int flags, size_t *len)
{
	int		 mib[7];
	char		*buf, *bufp, *errmsg = NULL;
	size_t		 needed;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = flags;
	mib[6] = rdomain;

	buf = NULL;
	errmsg = NULL;
	while (1) {
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
		log_warn("get_routes - %s (msize=%zu)", errmsg, needed);
		free(buf);
		buf = NULL;
	}

	*len = needed;
	return buf;
}

/*
 * check_route_label examines the label associated with a route and
 * returns a value indicating that there was no label (ROUTE_LABEL_NONE),
 * that the route was created by the current process
 * (ROUTE_LABEL_DHCLIENT_OURS), a dead process (ROUTE_LABEL_DHCLIENT_DEAD), or
 * an indeterminate process (ROUTE_LABEL_DHCLIENT_UNKNOWN).
 */
int
check_route_label(struct sockaddr_rtlabel *label)
{
	pid_t pid;

	if (label == NULL)
		return ROUTE_LABEL_NONE;

	if (strncmp("DHCLIENT ", label->sr_label, 9) != 0)
		return ROUTE_LABEL_NOT_DHCLIENT;

	pid = (pid_t)strtonum(label->sr_label + 9, 1, INT_MAX, NULL);
	if (pid <= 0)
		return ROUTE_LABEL_DHCLIENT_UNKNOWN;

	if (pid == getpid())
		return ROUTE_LABEL_DHCLIENT_OURS;

	if (kill(pid, 0) == -1) {
		if (errno == ESRCH)
			return ROUTE_LABEL_DHCLIENT_DEAD;
		else
			return ROUTE_LABEL_DHCLIENT_UNKNOWN;
	}

	return ROUTE_LABEL_DHCLIENT_LIVE;
}

/*
 * [priv_]flush_routes do the equivalent of
 *
 *	route -q $rdomain -n flush -inet -iface $interface
 *	arp -dan
 */
void
flush_routes(void)
{
	int	 rslt;

	rslt = imsg_compose(unpriv_ibuf, IMSG_FLUSH_ROUTES, 0, 0, -1, NULL, 0);
	if (rslt == -1)
		log_warn("flush_routes: imsg_compose");

	flush_unpriv_ibuf("flush_routes");
}

void
priv_flush_routes(char *name, int routefd, int rdomain)
{
	char				 ifname[IF_NAMESIZE];
	struct sockaddr			*rti_info[RTAX_MAX];
	char				*lim, *buf = NULL, *next;
	struct rt_msghdr		*rtm;
	struct sockaddr_in		*sa_in;
	struct sockaddr_rtlabel		*sa_rl;
	size_t				 len;

	buf = get_routes(rdomain, RTF_STATIC, &len);
	if (buf == NULL)
		return;

	lim = buf + len;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		populate_rti_info(rti_info, rtm);

		sa_rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
		sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];

		switch (check_route_label(sa_rl)) {
		case ROUTE_LABEL_DHCLIENT_OURS:
		case ROUTE_LABEL_DHCLIENT_DEAD:
			delete_route(routefd, rtm);
			break;
		case ROUTE_LABEL_DHCLIENT_LIVE:
		case ROUTE_LABEL_DHCLIENT_UNKNOWN:
			/* Another dhclient's responsibility. */
			break;
		case ROUTE_LABEL_NONE:
		case ROUTE_LABEL_NOT_DHCLIENT:
			/* Delete default routes on our interface. */
			if (if_indextoname(rtm->rtm_index, ifname) &&
			    sa_in &&
			    sa_in->sin_addr.s_addr == INADDR_ANY &&
			    rtm->rtm_tableid == rdomain &&
			    strcmp(name, ifname) == 0)
				delete_route(routefd, rtm);
			break;
		default:
			break;
		}
	}

	free(buf);
}

/*
 * delete_route deletes a single route from the routing table.
 */
void
delete_route(int s, struct rt_msghdr *rtm)
{
	static int	 seqno;
	ssize_t		 rlen;

	rtm->rtm_type = RTM_DELETE;
	rtm->rtm_seq = seqno++;

	rlen = write(s, (char *)rtm, rtm->rtm_msglen);
	if (rlen == -1) {
		if (errno != ESRCH)
			fatal("RTM_DELETE write");
	} else if (rlen < (int)rtm->rtm_msglen)
		fatalx("short RTM_DELETE write (%zd)\n", rlen);
}

/*
 * create_route_label constructs a short string that can be uses to label
 * a route so that subsequent route examinations can find routes added by
 * dhclient. The label includes the pid so that routes can be further
 * identified as coming from a particular dhclient instance.
 */
int
create_route_label(struct sockaddr_rtlabel *label)
{
	int	 len;

	memset(label, 0, sizeof(*label));

	label->sr_len = sizeof(label);
	label->sr_family = AF_UNSPEC;

	len = snprintf(label->sr_label, sizeof(label->sr_label), "DHCLIENT %d",
	    (int)getpid());

	if (len == -1 || (unsigned int)len >= sizeof(label->sr_label)) {
		log_warn("could not create route label");
		return 1;
	}

	return 0;
}

void
set_routes(struct in_addr addr, struct in_addr addrmask, uint8_t *rtstatic,
    unsigned int rtstatic_len)
{
	const struct in_addr	 any = { INADDR_ANY };
	struct in_addr		 dest, gateway, netmask;
	unsigned int		 i, bits, bytes;

	flush_routes();

	/* Add classless static routes. */
	i = 0;
	while (i < rtstatic_len) {
		bits = rtstatic[i++];
		bytes = (bits + 7) / 8;

		if (bytes > sizeof(struct in_addr))
			return;
		else if (i + bytes > rtstatic_len)
			return;

		if (bits != 0)
			netmask.s_addr = htonl(0xffffffff << (32 - bits));
		else
			netmask.s_addr = INADDR_ANY;

		memcpy(&dest, &rtstatic[i], bytes);
		dest.s_addr = dest.s_addr & netmask.s_addr;
		i += bytes;

		if (i + sizeof(gateway) > rtstatic_len)
			return;
		memcpy(&gateway, &rtstatic[i], sizeof(gateway));
		i += sizeof(gateway);

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
			if (addrmask.s_addr == INADDR_BROADCAST) {
				/*
				 * DIRECT ROUTE TO DEFAULT GATEWAY
				 *
				 * To be compatible with ISC DHCP behavior on
				 * Linux, if we were given a /32 IP assignment
				 * then add a /32 direct route for the gateway
				 * to make it routable.
				 *
				 * route add -net $gateway -netmask $addrmask
				 *     -cloning -iface $addr
				 */
				add_route(gateway, addrmask, addr,
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
		log_warn("add_route: imsg_compose");

	flush_unpriv_ibuf("add_route");
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
	struct sockaddr_rtlabel	 label;
	int			 i, index, iovcnt = 0;

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

	/* Add our label so we can identify the route as our creation. */
	if (create_route_label(&label) == 0) {
		rtm.rtm_addrs |= RTA_LABEL;
		rtm.rtm_msglen += sizeof(label);
		iov[iovcnt].iov_base = &label;
		iov[iovcnt++].iov_len = sizeof(label);
	}

	/* Check for EEXIST since other dhclient may not be done. */
	for (i = 0; i < 5; i++) {
		if (writev(routefd, iov, iovcnt) != -1)
			break;
		if (i == 4) {
			strlcpy(destbuf, inet_ntoa(imsg->dest), sizeof(destbuf));
			strlcpy(maskbuf, inet_ntoa(imsg->netmask), sizeof(maskbuf));
			log_warn("failed to add route (%s/%s via %s)",
			    destbuf, maskbuf, inet_ntoa(imsg->gateway));
		} else if (errno == EEXIST || errno == ENETUNREACH)
			sleep(1);
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
		fatal("delete_addresses getifaddrs");

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
		log_warn("delete_address: imsg_compose");

	flush_unpriv_ibuf("delete_address");
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
			log_warn("SIOCDIFADDR failed (%s)",
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
		log_warnx("mtu size %u < 68: ignored", mtu);
		return;
	}
	imsg.mtu = mtu;

	rslt = imsg_compose(unpriv_ibuf, IMSG_SET_MTU, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		log_warn("set_mtu: imsg_compose");

	flush_unpriv_ibuf("set_mtu");
}

void
priv_set_mtu(char *name, int ioctlfd, struct imsg_set_mtu *imsg)
{
	struct ifreq	 ifr;

	memset(&ifr, 0, sizeof(ifr));

	strlcpy(ifr.ifr_name, name, sizeof(ifr.ifr_name));
	ifr.ifr_mtu = imsg->mtu;

	if (ioctl(ioctlfd, SIOCSIFMTU, &ifr) == -1)
		log_warn("SIOCSIFMTU failed (%d)", imsg->mtu);
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
		log_warn("set_address: imsg_compose");

	flush_unpriv_ibuf("set_address");
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
		log_warn("SIOCAIFADDR failed (%s)", inet_ntoa(imsg->addr));
}

/*
 * [priv_]write_resolv_conf write out a new resolv.conf.
 */
void
write_resolv_conf(uint8_t *contents, size_t sz)
{
	int	 rslt;

	rslt = imsg_compose(unpriv_ibuf, IMSG_WRITE_RESOLV_CONF,
	    0, 0, -1, contents, sz);
	if (rslt == -1)
		log_warn("write_resolv_conf: imsg_compose");

	flush_unpriv_ibuf("write_resolv_conf");
}

void
priv_write_resolv_conf(uint8_t *contents, size_t sz)
{
	const char	*path = "/etc/resolv.conf";
	ssize_t		 n;
	int		 fd;

	fd = open(path, O_WRONLY | O_CREAT | O_TRUNC,
	    S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);

	if (fd == -1) {
		log_warn("Couldn't open '%s'", path);
		return;
	}

	n = write(fd, contents, sz);
	if (n == -1)
		log_warn("Couldn't write contents to '%s'", path);
	else if ((size_t)n < sz)
		log_warnx("Short contents write to '%s' (%zd vs %zu)",
		    path, n, sz);

	close(fd);
}

/*
 * resolv_conf_priority decides if the interface is the best one to
 * suppy the contents of the resolv.conf file.
 */
int
resolv_conf_priority(int rdomain, int routefd)
{
	struct iovec		 iov[3];
	struct sockaddr_in	 sin;
	struct {
		struct rt_msghdr	m_rtm;
		char			m_space[512];
	}			 m_rtmsg;
	struct sockaddr		*rti_info[RTAX_MAX];
	struct sockaddr_rtlabel *sa_rl;
	pid_t			 pid;
	ssize_t			 len;
	int			 seq, rslt, iovcnt = 0;

	rslt = 0;

	/* Build RTM header */

	memset(&m_rtmsg, 0, sizeof(m_rtmsg));

	m_rtmsg.m_rtm.rtm_version = RTM_VERSION;
	m_rtmsg.m_rtm.rtm_type = RTM_GET;
	m_rtmsg.m_rtm.rtm_msglen = sizeof(m_rtmsg.m_rtm);
	m_rtmsg.m_rtm.rtm_flags = RTF_STATIC | RTF_GATEWAY | RTF_UP;
	m_rtmsg.m_rtm.rtm_seq = seq = arc4random();
	m_rtmsg.m_rtm.rtm_tableid = rdomain;

	iov[iovcnt].iov_base = &m_rtmsg.m_rtm;
	iov[iovcnt++].iov_len = sizeof(m_rtmsg.m_rtm);

	/* Set destination & netmask addresses of all zeros. */

	m_rtmsg.m_rtm.rtm_addrs = RTA_DST | RTA_NETMASK;

	memset(&sin, 0, sizeof(sin));
	sin.sin_len = sizeof(sin);
	sin.sin_family = AF_INET;

	iov[iovcnt].iov_base = &sin;
	iov[iovcnt++].iov_len = sizeof(sin);
	iov[iovcnt].iov_base = &sin;
	iov[iovcnt++].iov_len = sizeof(sin);

	m_rtmsg.m_rtm.rtm_msglen += 2 * sizeof(sin);

	if (writev(routefd, iov, iovcnt) == -1) {
		if (errno != ESRCH)
			log_warn("RTM_GET of default route");
		goto done;
	}

	pid = getpid();

	do {
		len = read(routefd, &m_rtmsg, sizeof(m_rtmsg));
		if (len == -1) {
			log_warn("get default route read");
			goto done;
		} else if (len == 0) {
			log_warnx("no data from default route read");
			goto done;
		}
		if (m_rtmsg.m_rtm.rtm_version != RTM_VERSION)
			continue;
		if (m_rtmsg.m_rtm.rtm_type == RTM_GET &&
		    m_rtmsg.m_rtm.rtm_pid == pid &&
		    m_rtmsg.m_rtm.rtm_seq == seq) {
			if (m_rtmsg.m_rtm.rtm_errno != 0) {
				log_warnx("default route read rtm: %s",
				    strerror(m_rtmsg.m_rtm.rtm_errno));
				goto done;
			}
			break;
		}
	} while (1);

	populate_rti_info(rti_info, &m_rtmsg.m_rtm);

	sa_rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
	if (check_route_label(sa_rl) == ROUTE_LABEL_DHCLIENT_OURS)
		rslt = 1;

done:
	return rslt;
}

/*
 * resolv_conf_contents creates a string that are the resolv.conf contents
 * that should be used when the interface is determined to be the one to
 * create /etc/resolv.conf
 */
char *
resolv_conf_contents(char *name,
    uint8_t *rtsearch, unsigned int rtsearch_len,
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
		fatalx("no memory for domainname");
	len += strlen(dn);

	if (rtdns_len != 0) {
		addr = (struct in_addr *)rtdns;
		servers = rtdns_len / sizeof(struct in_addr);
		if (servers > MAXNS)
			servers = MAXNS;
		for (i = 0; i < servers; i++) {
			rslt = asprintf(&nss[i], "nameserver %s\n",
			    inet_ntoa(*addr));
			if (rslt == -1)
				fatalx("no memory for nameserver");
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
		return NULL;
	}

	rslt = asprintf(&courtesy, "# Generated by %s dhclient\n", name);
	if (rslt == -1)
		fatalx("no memory for courtesy line");
	len += strlen(courtesy);

	len++; /* Need room for terminating NUL. */
	contents = calloc(1, len);
	if (contents == NULL)
		fatalx("no memory for resolv.conf contents");

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

	return contents;
}

/*
 * populate_rti_info populates the rti_info with pointers to the
 * sockaddr's contained in a rtm message.
 */
void
populate_rti_info(struct sockaddr **rti_info, struct rt_msghdr *rtm)
{
	struct sockaddr	*sa;
	int		 i;

	sa = (struct sockaddr *)((char *)(rtm) + rtm->rtm_hdrlen);

	for (i = 0; i < RTAX_MAX; i++) {
		if (rtm->rtm_addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		} else
			rti_info[i] = NULL;
	}
}
