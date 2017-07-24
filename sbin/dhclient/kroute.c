/*	$OpenBSD: kroute.c,v 1.118 2017/07/24 18:13:19 krw Exp $	*/

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
void	add_route(struct in_addr, struct in_addr, struct in_addr,
    struct in_addr, int, int);
void	flush_routes(void);
void	delete_addresses(char *);

#define	ROUTE_LABEL_NONE		1
#define	ROUTE_LABEL_NOT_DHCLIENT	2
#define	ROUTE_LABEL_DHCLIENT_OURS	3
#define	ROUTE_LABEL_DHCLIENT_UNKNOWN	4
#define	ROUTE_LABEL_DHCLIENT_LIVE	5
#define	ROUTE_LABEL_DHCLIENT_DEAD	6

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
	int			 mib[7];
	char			 ifname[IF_NAMESIZE];
	struct sockaddr		*rti_info[RTAX_MAX];
	char			*lim, *buf = NULL, *bufp, *next, *errmsg = NULL;
	struct rt_msghdr	*rtm;
	struct sockaddr_in	*sa_in;
	struct sockaddr_rtlabel	*sa_rl;
	size_t			 needed;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = AF_INET;
	mib[4] = NET_RT_FLAGS;
	mib[5] = RTF_GATEWAY;
	mib[6] = rdomain;

	while (1) {
		if (sysctl(mib, 7, NULL, &needed, NULL, 0) == -1) {
			errmsg = "sysctl size of routes:";
			break;
		}
		if (needed == 0) {
			free(buf);
			return;
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
		log_warn("route cleanup failed - %s (msize=%zu)", errmsg,
		    needed);
		free(buf);
		return;
	}

	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		populate_rti_info(rti_info, rtm);

		sa_rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
		sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];

		switch (check_route_label(sa_rl)) {
		case ROUTE_LABEL_DHCLIENT_OURS:
			/* Always delete routes we labeled. */
			delete_route(routefd, rtm);
			break;
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
 * add_direct_route is the equivalent of
 *
 *     route add -net $dest -netmask $mask -cloning -iface $iface
 */
void
add_direct_route(struct in_addr dest, struct in_addr mask,
    struct in_addr iface)
{
	struct in_addr	 ifa = { INADDR_ANY };

	add_route(dest, mask, iface, ifa,
	    RTA_DST | RTA_NETMASK | RTA_GATEWAY, RTF_CLONING | RTF_STATIC);
}

/*
 * add_default_route is the equivalent of
 *
 *	route -q $rdomain add default -iface $router
 *
 *	or
 *
 *	route -q $rdomain add default $router
 */
void
add_default_route(struct in_addr gateway, struct in_addr addr)
{
	struct in_addr	 netmask, dest;
	int		 addrs, flags;

	memset(&netmask, 0, sizeof(netmask));
	memset(&dest, 0, sizeof(dest));
	addrs = RTA_DST | RTA_NETMASK;
	flags = 0;

	/*
	 * When 'addr' and 'gateway' are identical the desired behaviour is
	 * to emulate the '-iface' variant of 'route'. This is done by
	 * claiming there is no gateway address to use.
	 */
	if (memcmp(&gateway, &addr, sizeof(addr)) != 0) {
		addrs |= RTA_GATEWAY | RTA_IFA;
		flags |= RTF_GATEWAY | RTF_STATIC;
	}

	add_route(dest, netmask, gateway, addr, addrs, flags);
}

/*
 *
 * add_classless_static_routes() accepts a list of static routes in the
 * format specified for DHCP options 121 (classless-static-routes) and
 * 249 (classless-ms-static-routes).
 */
void
add_classless_static_routes(struct option_data *opt, struct in_addr iface)
{
	struct in_addr	 dest, netmask, gateway;
	unsigned int	 i, bits, bytes;

	i = 0;
	while (i < opt->len) {
		bits = opt->data[i++];
		bytes = (bits + 7) / 8;

		if (bytes > sizeof(netmask))
			return;
		else if (i + bytes > opt->len)
			return;

		if (bits != 0)
			netmask.s_addr = htonl(0xffffffff << (32 - bits));
		else
			netmask.s_addr = INADDR_ANY;

		memcpy(&dest, &opt->data[i], bytes);
		dest.s_addr = dest.s_addr & netmask.s_addr;
		i += bytes;

		if (i + sizeof(gateway) > opt->len)
			return;
		memcpy(&gateway, &opt->data[i], sizeof(gateway));
		i += sizeof(gateway);

		if (gateway.s_addr == INADDR_ANY)
			add_direct_route(dest, netmask, iface);
		else
			add_route(dest, netmask, gateway, iface,
			    RTA_DST | RTA_GATEWAY | RTA_NETMASK | RTA_IFA,
			    RTF_GATEWAY | RTF_STATIC);
	}
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
set_routes(struct in_addr addr, struct option_data *classless,
    struct option_data *msclassless, struct option_data *routers,
    struct option_data *subnet)
{
	struct in_addr	gateway, mask;

	flush_routes();

	if (classless->len != 0) {
		add_classless_static_routes(classless, addr);
		return;
	}

	if (msclassless->len != 0) {
		add_classless_static_routes(msclassless, addr);
		return;
	}

	if (routers->len >= sizeof(gateway)) {
		/* XXX Only use FIRST router address for now. */
		gateway.s_addr = ((struct in_addr *)routers->data)->s_addr;

		/*
		 * To be compatible with ISC DHCP behavior on Linux, if
		 * we were given a /32 IP assignment, then add a /32
		 * direct route for the gateway to make it routable.
		 */
		if (subnet->len == sizeof(mask)) {
			mask.s_addr = ((struct in_addr *)subnet->data)->s_addr;
			if (mask.s_addr == INADDR_BROADCAST)
				add_direct_route(gateway, mask, addr);
		}

		add_default_route(gateway, addr);
	}
}

/*
 * [priv_]add_route() add a single route to the routing table.
 */
void
add_route(struct in_addr dest, struct in_addr netmask,
    struct in_addr gateway, struct in_addr ifa, int addrs, int flags)
{
	struct imsg_add_route	 imsg;
	int			 rslt;

	imsg.dest = dest;
	imsg.gateway = gateway;
	imsg.netmask = netmask;
	imsg.ifa = ifa;
	imsg.addrs = addrs;
	imsg.flags = flags;

	rslt = imsg_compose(unpriv_ibuf, IMSG_ADD_ROUTE, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		log_warn("add_route: imsg_compose");

	flush_unpriv_ibuf("add_route");
}

void
priv_add_route(int rdomain, int routefd, struct imsg_add_route *imsg)
{
	char			 destbuf[INET_ADDRSTRLEN];
	char			 gatewaybuf[INET_ADDRSTRLEN];
	char			 maskbuf[INET_ADDRSTRLEN];
	char			 ifabuf[INET_ADDRSTRLEN];
	struct iovec		 iov[6];
	struct rt_msghdr	 rtm;
	struct sockaddr_in	 dest, gateway, mask, ifa;
	struct sockaddr_rtlabel	 label;
	int			 i, iovcnt = 0;

	memset(destbuf, 0, sizeof(destbuf));
	memset(maskbuf, 0, sizeof(maskbuf));
	memset(gatewaybuf, 0, sizeof(gatewaybuf));
	memset(ifabuf, 0, sizeof(ifabuf));

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_ADD;
	rtm.rtm_tableid = rdomain;
	rtm.rtm_priority = RTP_NONE;
	rtm.rtm_msglen = sizeof(rtm);
	rtm.rtm_addrs = imsg->addrs;
	rtm.rtm_flags = imsg->flags;

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);

	if (imsg->addrs & RTA_DST) {
		strlcpy(destbuf, inet_ntoa(imsg->dest), sizeof(destbuf));
		memset(&dest, 0, sizeof(dest));

		dest.sin_len = sizeof(dest);
		dest.sin_family = AF_INET;
		dest.sin_addr.s_addr = imsg->dest.s_addr;

		rtm.rtm_msglen += sizeof(dest);

		iov[iovcnt].iov_base = &dest;
		iov[iovcnt++].iov_len = sizeof(dest);
	}

	if (imsg->addrs & RTA_GATEWAY) {
		strlcpy(gatewaybuf, inet_ntoa(imsg->gateway),
		    sizeof(gatewaybuf));
		memset(&gateway, 0, sizeof(gateway));

		gateway.sin_len = sizeof(gateway);
		gateway.sin_family = AF_INET;
		gateway.sin_addr.s_addr = imsg->gateway.s_addr;

		rtm.rtm_msglen += sizeof(gateway);

		iov[iovcnt].iov_base = &gateway;
		iov[iovcnt++].iov_len = sizeof(gateway);
	}

	if (imsg->addrs & RTA_NETMASK) {
		strlcpy(maskbuf, inet_ntoa(imsg->netmask), sizeof(maskbuf));
		memset(&mask, 0, sizeof(mask));

		mask.sin_len = sizeof(mask);
		mask.sin_family = AF_INET;
		mask.sin_addr.s_addr = imsg->netmask.s_addr;

		rtm.rtm_msglen += sizeof(mask);

		iov[iovcnt].iov_base = &mask;
		iov[iovcnt++].iov_len = sizeof(mask);
	}

	if (imsg->addrs & RTA_IFA) {
		strlcpy(ifabuf, inet_ntoa(imsg->ifa), sizeof(ifabuf));
		memset(&ifa, 0, sizeof(ifa));

		ifa.sin_len = sizeof(ifa);
		ifa.sin_family = AF_INET;
		ifa.sin_addr.s_addr = imsg->ifa.s_addr;

		rtm.rtm_msglen += sizeof(ifa);

		iov[iovcnt].iov_base = &ifa;
		iov[iovcnt++].iov_len = sizeof(ifa);
	}

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
		if (i == 4)
			log_warn("failed to add route (%s/%s via %s/%s)",
			    destbuf, maskbuf, gatewaybuf, ifabuf);
		else if (errno == EEXIST || errno == ENETUNREACH)
			sleep(1);
	}
}

/*
 * delete_addresses deletes all existing inet addresses on the specified
 * interface.
 */
void
delete_addresses(char *name)
{
	struct in_addr		 addr;
	struct ifaddrs		*ifap, *ifa;

	if (getifaddrs(&ifap) != 0)
		fatal("delete_addresses getifaddrs");

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) != 0 ||
		    (ifa->ifa_flags & IFF_POINTOPOINT) != 0 ||
		    ((ifa->ifa_flags & IFF_UP) == 0) ||
		    (ifa->ifa_addr->sa_family != AF_INET) ||
		    (strcmp(name, ifa->ifa_name) != 0))
			continue;

		memcpy(&addr, &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
		    sizeof(addr));

		delete_address(addr);
	}

	freeifaddrs(ifap);
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
set_mtu(struct option_data *mtu)
{
	struct imsg_set_mtu	 imsg;
	int			 rslt;

	if (mtu->len != sizeof(uint16_t))
		return;

	memcpy(&imsg.mtu, mtu->data, sizeof(uint16_t));
	imsg.mtu = ntohs(imsg.mtu);
	if (imsg.mtu < 68) {
		log_warnx("mtu size %u < 68: ignored", imsg.mtu);
		return;
	}

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
set_address(char *name, struct in_addr addr, struct option_data *mask)
{
	struct imsg_set_address	 imsg;
	int			 rslt;

	/* Deleting the addresses also clears out arp entries. */
	delete_addresses(name);

	imsg.addr = addr;
	if (mask->len == sizeof(imsg.mask))
		imsg.mask.s_addr = ((struct in_addr *)mask->data)->s_addr;
	else
		imsg.mask.s_addr = INADDR_ANY;

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
			break;
		} else if (len == 0) {
			log_warnx("no data from default route read");
			break;
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
    struct option_data *domainname, struct option_data *nameservers,
    struct option_data *domainsearch)
{
	char		*dn, *ns, *nss[MAXNS], *contents, *courtesy, *p, *buf;
	size_t		 len;
	int		 i, rslt;

	memset(nss, 0, sizeof(nss));

	if (domainsearch->len != 0) {
		buf = pretty_print_domain_search(domainsearch->data,
		    domainsearch->len);
		if (buf == NULL)
			dn = strdup("");
		else {
			rslt = asprintf(&dn, "search %s\n", buf);
			if (rslt == -1)
				dn = NULL;
		}
	} else if (domainname->len != 0) {
		rslt = asprintf(&dn, "search %s\n",
		    pretty_print_option(DHO_DOMAIN_NAME, domainname, 0));
		if (rslt == -1)
			dn = NULL;
	} else
		dn = strdup("");
	if (dn == NULL)
		fatalx("no memory for domainname");

	if (nameservers->len != 0) {
		ns = pretty_print_option(DHO_DOMAIN_NAME_SERVERS, nameservers,
		    0);
		for (i = 0; i < MAXNS; i++) {
			p = strsep(&ns, " ");
			if (p == NULL)
				break;
			if (*p == '\0')
				continue;
			rslt = asprintf(&nss[i], "nameserver %s\n", p);
			if (rslt == -1)
				fatalx("no memory for nameserver");
		}
	}

	len = strlen(dn);
	for (i = 0; i < MAXNS; i++)
		if (nss[i] != NULL)
			len += strlen(nss[i]);

	if (len > 0 && config->resolv_tail)
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

	if (config->resolv_tail)
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
