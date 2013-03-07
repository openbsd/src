/*	$OpenBSD: kroute.c,v 1.41 2013/03/07 13:23:27 krw Exp $	*/

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

#include "dhcpd.h"
#include "privsep.h"

#include <sys/ioctl.h>
#include <sys/sysctl.h>
#include <sys/uio.h>

#include <net/if_types.h>

#include <ifaddrs.h>
#include <signal.h>

struct in_addr active_addr;

int	create_route_label(struct sockaddr_rtlabel *);
int	check_route_label(struct sockaddr_rtlabel *);

#define	ROUTE_LABEL_NONE		1
#define	ROUTE_LABEL_NOT_DHCLIENT	2
#define	ROUTE_LABEL_DHCLIENT_OURS	3
#define	ROUTE_LABEL_DHCLIENT_UNKNOWN	4
#define	ROUTE_LABEL_DHCLIENT_LIVE	5
#define	ROUTE_LABEL_DHCLIENT_DEAD	6

/*
 * Do equivalent of 
 *
 *	route -q $rdomain -n flush -inet -iface $interface
 *	arp -dan
 */
void
flush_routes_and_arp_cache(char *ifname, int rdomain)
{
	struct imsg_flush_routes imsg;
	int			 rslt;

	memset(&imsg, 0, sizeof(imsg));

	strlcpy(imsg.ifname, ifname, sizeof(imsg.ifname));
	imsg.rdomain = rdomain;

	rslt = imsg_compose(unpriv_ibuf, IMSG_FLUSH_ROUTES, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		warning("flush_routes_and_arp_cache: imsg_compose: %s",
		    strerror(errno));

	/* Do flush to maximize chances of cleaning up routes on exit. */
	rslt = imsg_flush(unpriv_ibuf);
	if (rslt == -1)
		warning("flush_routes_and_arp_cache: imsg_flush: %s",
		    strerror(errno));
}

void
priv_flush_routes_and_arp_cache(struct imsg_flush_routes *imsg)
{
	char ifname[IF_NAMESIZE];
	struct sockaddr *rti_info[RTAX_MAX];
	int mib[7];
	size_t needed;
	char *lim, *buf, *next, *errmsg;
	struct rt_msghdr *rtm;
	struct sockaddr *sa;
	struct sockaddr_dl *sdl;
	struct sockaddr_in *sa_in;
	struct sockaddr_inarp *sin;
	struct sockaddr_rtlabel *sa_rl;
	int s, seqno = 0, rlen, retry, i;

	mib[0] = CTL_NET;
	mib[1] = PF_ROUTE;
	mib[2] = 0;
	mib[3] = 0;
	mib[4] = NET_RT_DUMP;
	mib[5] = 0;
	mib[6] = imsg->rdomain;

	buf = NULL;
	retry = 0;
	do {
		retry++;
		errmsg = NULL;
		if (buf)
			free(buf);
		if (sysctl(mib, 7, NULL, &needed, NULL, 0) == -1) {
			errmsg = "sysctl size of routes:";
			continue;
		}
		if (needed == 0)
			return;
		if ((buf = malloc(needed)) == NULL) {
			errmsg = "routes buf malloc:";
			continue;
		}
		if (sysctl(mib, 7, buf, &needed, NULL, 0) == -1) {
			errmsg = "sysctl retrieval of routes:";
		}
	} while (retry < 10 && errmsg != NULL);

	if (errmsg) {
		warning("route cleanup failed - %s %s (%d retries, msize=%zu)",
		    errmsg, strerror(errno), retry, needed);
		return;
	}

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("opening socket to flush routes: %s", strerror(errno));

#define ROUNDUP(a) \
    ((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

	lim = buf + needed;
	for (next = buf; next < lim; next += rtm->rtm_msglen) {
		rtm = (struct rt_msghdr *)next;
		if (rtm->rtm_version != RTM_VERSION)
			continue;

		sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);
		if (sa->sa_family == AF_KEY)
			continue;  /* Don't flush SPD */

		memset(rti_info, 0, sizeof(rti_info));
		for (i = 0; i < RTAX_MAX; i++) {
			if (rtm->rtm_addrs & (1 << i)) {
				rti_info[i] = sa;
				sa = (struct sockaddr *)((char *)(sa) +
				    ROUNDUP(sa->sa_len));
			}
		}

		sa = (struct sockaddr *)(next + rtm->rtm_hdrlen);

		sa_rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
		switch (check_route_label(sa_rl)) {
		case ROUTE_LABEL_DHCLIENT_OURS:
			/* Always delete routes we labeled. */
			goto delete;
		case ROUTE_LABEL_DHCLIENT_LIVE:
		case ROUTE_LABEL_DHCLIENT_DEAD:
		case ROUTE_LABEL_DHCLIENT_UNKNOWN:
			/* Never delete routes labelled by another dhclient. */
			continue;
		default:
			break;
		}

		if (rtm->rtm_flags & RTF_LLINFO) {
			if (rtm->rtm_flags & RTF_GATEWAY)
				continue;
			if (rtm->rtm_flags & RTF_PERMANENT_ARP)
				continue;

			/* XXXX Check for AF_INET too? (arp ask for them) */
			/* XXXX Need 'retry' for proxy entries? (arp does) */

			sin = (struct sockaddr_inarp *)(sa);
			sdl = (struct sockaddr_dl *)(ROUNDUP(sin->sin_len) +
			   (char *)sin);

			if (sdl->sdl_family == AF_LINK) {
				switch (sdl->sdl_type) {
				case IFT_ETHER:
				case IFT_FDDI:
				case IFT_ISO88023:
				case IFT_ISO88024:
				case IFT_ISO88025:
				case IFT_CARP:
					/* Delete it. */
					goto delete;
				default:
					break;
				}
			}
			continue;
		}

		if (rtm->rtm_flags & RTF_GATEWAY) {
			memset(ifname, 0, sizeof(ifname));
			if (if_indextoname(rtm->rtm_index, ifname) == NULL)
				continue;
			sa_in = (struct sockaddr_in *)rti_info[RTAX_NETMASK];
			if (sa_in && 
			    sa_in->sin_addr.s_addr == INADDR_ANY &&
			    rtm->rtm_tableid == imsg->rdomain &&
			    strcmp(imsg->ifname, ifname) == 0)
				goto delete;
		}

		continue;

delete:
		rtm->rtm_type = RTM_DELETE;
		rtm->rtm_seq = seqno;
		rtm->rtm_tableid = imsg->rdomain;

		rlen = write(s, next, rtm->rtm_msglen);
		if (rlen == -1) {
			if (errno != ESRCH)
				error("RTM_DELETE write: %s", strerror(errno));
		} else if (rlen < (int)rtm->rtm_msglen)
			error("short RTM_DELETE write (%d)\n", rlen);

		seqno++;
	}

	close(s);
	free(buf);
}

/*
 * [priv_]add_default_route is the equivalent of
 *
 *	route -q $rdomain -n flush -inet -iface $interface
 *
 * and one of
 *
 *	route -q $rdomain add default -iface $router
 * 	route -q $rdomain add default $router
 *
 * depending on the contents of the gateway parameter.
 */
void
add_default_route(int rdomain, struct in_addr addr, struct in_addr gateway)
{
	struct imsg_add_default_route	 imsg;
	int				 rslt;

	memset(&imsg, 0, sizeof(imsg));

	imsg.rdomain = rdomain;
	imsg.addr = addr;
	imsg.gateway = gateway;

	rslt = imsg_compose(unpriv_ibuf, IMSG_ADD_DEFAULT_ROUTE, 0, 0, -1,
	    &imsg, sizeof(imsg));

	if (rslt == -1)
		warning("add_default_route: imsg_compose: %s", strerror(errno));
}

void
priv_add_default_route(struct imsg_add_default_route *imsg)
{
	struct rt_msghdr rtm;
	struct sockaddr_in dest, gateway, mask;
	struct sockaddr_rtlabel label;
	struct iovec iov[5];
	int s, i, iovcnt = 0;

	/*
	 * Add a default route via the specified address.
	 */

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("Routing Socket open failed: %s", strerror(errno));

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_ADD;
	rtm.rtm_tableid = imsg->rdomain;
	rtm.rtm_priority = RTP_NONE;
	rtm.rtm_msglen = sizeof(rtm);

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);
	
	/* Set destination address of all zeros. */

	memset(&dest, 0, sizeof(dest));

	dest.sin_len = sizeof(dest);
	dest.sin_family = AF_INET;

	rtm.rtm_addrs |= RTA_DST;
	rtm.rtm_msglen += sizeof(dest);

	iov[iovcnt].iov_base = &dest;
	iov[iovcnt++].iov_len = sizeof(dest);
	
	/*
	 * Set gateway address if and only if non-zero addr supplied. A
	 * gateway address of 0 implies '-iface'.
	 */

	memset(&gateway, 0, sizeof(gateway));
	if (bcmp(&imsg->gateway, &imsg->addr, sizeof(imsg->addr)) != 0) {
		gateway.sin_len = sizeof(gateway);
		gateway.sin_family = AF_INET;
		gateway.sin_addr.s_addr = imsg->gateway.s_addr;

		rtm.rtm_flags |= RTF_GATEWAY | RTF_STATIC;
		rtm.rtm_addrs |= RTA_GATEWAY;
		rtm.rtm_msglen += sizeof(gateway);

		iov[iovcnt].iov_base = &gateway;
		iov[iovcnt++].iov_len = sizeof(gateway);
	}

	/* Add netmask of 0. */
	memset(&mask, 0, sizeof(mask));

	mask.sin_len = sizeof(mask);
	mask.sin_family = AF_INET;

	rtm.rtm_addrs |= RTA_NETMASK;
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
		if (writev(s, iov, iovcnt) != -1)
			break;
		if (errno != EEXIST && errno != ENETUNREACH)
			error("failed to add default route: %s",
			    strerror(errno));
		sleep(1);
	}

	close(s);
}

/*
 * Delete all existing inet addresses on interface.
 */
void
delete_addresses(char *ifname, int rdomain)
{
	struct in_addr addr;
	struct ifaddrs *ifap, *ifa;

	if (getifaddrs(&ifap) != 0)
		error("delete_addresses getifaddrs: %s", strerror(errno));

	for (ifa = ifap; ifa; ifa = ifa->ifa_next) {
		if ((ifa->ifa_flags & IFF_LOOPBACK) ||
		    (ifa->ifa_flags & IFF_POINTOPOINT) ||
		    (!(ifa->ifa_flags & IFF_UP)) ||
		    (ifa->ifa_addr->sa_family != AF_INET) ||
		    (strcmp(ifi->name, ifa->ifa_name)))
			continue;

		memset(&addr, 0, sizeof(addr));
		memcpy(&addr,
		    &((struct sockaddr_in *)ifa->ifa_addr)->sin_addr,
		    sizeof(in_addr_t));

		delete_address(ifi->name, ifi->rdomain, addr);
 	}

	freeifaddrs(ifap);
}

/*
 * [priv_]delete_address is the equivalent of
 *
 *	ifconfig <ifname> inet <addr> delete
 *	route -q <rdomain> delete <addr> 127.0.0.1
 */
void
delete_address(char *ifname, int rdomain, struct in_addr addr)
{
	struct imsg_delete_address	 imsg;
	int				 rslt;

	memset(&imsg, 0, sizeof(imsg));

	/* Note the address we are deleting for RTM_DELADDR filtering! */
	deleting.s_addr = addr.s_addr;

	strlcpy(imsg.ifname, ifname, sizeof(imsg.ifname));
	imsg.rdomain = rdomain;
	imsg.addr = addr;

	rslt = imsg_compose(unpriv_ibuf, IMSG_DELETE_ADDRESS, 0, 0 , -1, &imsg,
	    sizeof(imsg));
	if (rslt == -1)
		warning("delete_address: imsg_compose: %s", strerror(errno));

	/* Do flush to quickly kill previous dhclient, if any. */
	rslt = imsg_flush(unpriv_ibuf);
	if (rslt == -1 && errno != EPIPE)
		warning("delete_address: imsg_flush: %s", strerror(errno));
}

void
priv_delete_address(struct imsg_delete_address *imsg)
{
	struct ifaliasreq ifaliasreq;
	struct rt_msghdr rtm;
	struct sockaddr_in dest, gateway;
	struct iovec iov[3];
	struct sockaddr_in *in;
	int s, iovcnt = 0;

	/*
	 * Delete specified address on specified interface.
	 */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("socket open failed: %s", strerror(errno));

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, imsg->ifname,
	    sizeof(ifaliasreq.ifra_name));

	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = imsg->addr.s_addr;

	/* SIOCDIFADDR will result in a RTM_DELADDR message we must catch! */
	if (ioctl(s, SIOCDIFADDR, &ifaliasreq) == -1) {
		if (errno != EADDRNOTAVAIL)
			warning("SIOCDIFADDR failed (%s): %s",
			    inet_ntoa(imsg->addr), strerror(errno));
		close(s);
		return;
	}

	close(s);

	/*
	 * Delete the 127.0.0.1 route for the specified address.
	 */

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("Routing Socket open failed: %s", strerror(errno));

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_DELETE;
	rtm.rtm_tableid = imsg->rdomain;
	rtm.rtm_priority = RTP_NONE;
	rtm.rtm_msglen = sizeof(rtm);

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);
	
	/* Set destination address */

	memset(&dest, 0, sizeof(dest));

	dest.sin_len = sizeof(dest);
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = imsg->addr.s_addr;

	rtm.rtm_addrs |= RTA_DST;
	rtm.rtm_msglen += sizeof(dest);

	iov[iovcnt].iov_base = &dest;
	iov[iovcnt++].iov_len = sizeof(dest);
	
	/* Set gateway address */

	memset(&gateway, 0, sizeof(gateway));

	gateway.sin_len = sizeof(gateway);
	gateway.sin_family = AF_INET;
	gateway.sin_addr.s_addr = inet_addr("127.0.0.1");

	rtm.rtm_flags |= RTF_GATEWAY;
	rtm.rtm_addrs |= RTA_GATEWAY;
	rtm.rtm_msglen += sizeof(gateway);

	iov[iovcnt].iov_base = &gateway;
	iov[iovcnt++].iov_len = sizeof(gateway);

	/* ESRCH means the route does not exist to delete. */
	if ((writev(s, iov, iovcnt) == -1) && (errno != ESRCH))
		error("failed to delete 127.0.0.1: %s", strerror(errno));

	close(s);
}

/*
 * [priv_]add_address is the equivalent of
 *
 *	ifconfig <if> inet <addr> netmask <mask> broadcast <addr>
 *	route -q <rdomain> add <addr> 127.0.0.1
 */
void
add_address(char *ifname, int rdomain, struct in_addr addr,
    struct in_addr mask)
{
	struct imsg_add_address imsg;
	int			rslt;
 
	memset(&imsg, 0, sizeof(imsg));

	/* Note the address we are adding for RTM_NEWADDR filtering! */
	adding = addr;

	strlcpy(imsg.ifname, ifname, sizeof(imsg.ifname));
	imsg.rdomain = rdomain;
	imsg.addr = addr;
	imsg.mask = mask;
 
	rslt = imsg_compose(unpriv_ibuf, IMSG_ADD_ADDRESS, 0, 0, -1, &imsg,
	    sizeof(imsg));

	if (rslt == -1)
		warning("add_address: imsg_compose: %s", strerror(errno));
}

void
priv_add_address(struct imsg_add_address *imsg)
{
	struct ifaliasreq ifaliasreq;
	struct rt_msghdr rtm;
	struct sockaddr_in dest, gateway;
	struct sockaddr_rtlabel label;
	struct iovec iov[4];
	struct sockaddr_in *in;
	int s, i, iovcnt = 0;

	if (imsg->addr.s_addr == INADDR_ANY) {
		/* Notification that the active_addr has been deleted. */
		active_addr.s_addr = INADDR_ANY;
		quit = INTERNALSIG;
		return;
	}

	/*
	 * Add specified address on specified interface.
	 */

	if ((s = socket(AF_INET, SOCK_STREAM, 0)) == -1)
		error("socket open failed: %s", strerror(errno));

	memset(&ifaliasreq, 0, sizeof(ifaliasreq));
	strncpy(ifaliasreq.ifra_name, imsg->ifname,
	    sizeof(ifaliasreq.ifra_name));

	/* The actual address in ifra_addr. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_addr;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_addr);
	in->sin_addr.s_addr = imsg->addr.s_addr;

	/* And the netmask in ifra_mask. */
	in = (struct sockaddr_in *)&ifaliasreq.ifra_mask;
	in->sin_family = AF_INET;
	in->sin_len = sizeof(ifaliasreq.ifra_mask);
	memcpy(&in->sin_addr.s_addr, &imsg->mask, sizeof(imsg->mask));

	/* No need to set broadcast address. Kernel can figure it out. */

	if (ioctl(s, SIOCAIFADDR, &ifaliasreq) == -1)
		warning("SIOCAIFADDR failed (%s): %s", inet_ntoa(imsg->addr),
		    strerror(errno));

	close(s);

	/*
	 * Add the 127.0.0.1 route for the specified address.
	 */

	if ((s = socket(AF_ROUTE, SOCK_RAW, 0)) == -1)
		error("Routing Socket open failed: %s", strerror(errno));

	/* Build RTM header */

	memset(&rtm, 0, sizeof(rtm));

	rtm.rtm_version = RTM_VERSION;
	rtm.rtm_type = RTM_ADD;
	rtm.rtm_tableid = imsg->rdomain;
	rtm.rtm_priority = RTP_NONE;
	rtm.rtm_msglen = sizeof(rtm);

	iov[iovcnt].iov_base = &rtm;
	iov[iovcnt++].iov_len = sizeof(rtm);
	
	/* Set destination address */

	memset(&dest, 0, sizeof(dest));

	dest.sin_len = sizeof(dest);
	dest.sin_family = AF_INET;
	dest.sin_addr.s_addr = imsg->addr.s_addr;

	rtm.rtm_addrs |= RTA_DST;
	rtm.rtm_msglen += sizeof(dest);

	iov[iovcnt].iov_base = &dest;
	iov[iovcnt++].iov_len = sizeof(dest);
	
	/* Set gateway address */

	memset(&gateway, 0, sizeof(gateway));

	gateway.sin_len = sizeof(gateway);
	gateway.sin_family = AF_INET;
	gateway.sin_addr.s_addr = inet_addr("127.0.0.1");

	rtm.rtm_flags |= RTF_GATEWAY;
	rtm.rtm_addrs |= RTA_GATEWAY;
	rtm.rtm_msglen += sizeof(gateway);

	iov[iovcnt].iov_base = &gateway;
	iov[iovcnt++].iov_len = sizeof(gateway);

	/* Add our label so we can identify the route as our creation. */
	if (create_route_label(&label) == 0) {
		rtm.rtm_addrs |= RTA_LABEL;
		rtm.rtm_msglen += sizeof(label);
		iov[iovcnt].iov_base = &label;
		iov[iovcnt++].iov_len = sizeof(label);
	}

	/* Check for EEXIST since other dhclient may not be done. */
	for (i = 0; i < 5; i++) {
		if (writev(s, iov, iovcnt) != -1)
			break;
		if (errno == ENETUNREACH) {
			/* Not our responsibility to ensure 127/8 exists. */
			break;
		} else if (errno != EEXIST)
			error("failed to add 127.0.0.1 route: %s",
			    strerror(errno));
		sleep(1);
	}

	close(s);

	active_addr = imsg->addr;
}

/*
 * Inform the [priv] process a HUP was received and it should restart.
 */
void
sendhup(struct client_lease *active)
{
	struct imsg_hup imsg;
	int rslt;

	memset(&imsg, 0, sizeof(imsg));

	strlcpy(imsg.ifname, ifi->name, sizeof(imsg.ifname));
	imsg.rdomain = ifi->rdomain;
	if (active)
		imsg.addr = active->address;

	rslt = imsg_compose(unpriv_ibuf, IMSG_HUP, 0, 0, -1,
	    &imsg, sizeof(imsg));
	if (rslt == -1)
		warning("cleanup: imsg_compose: %s", strerror(errno));

	/* Do flush so cleanup message gets through immediately. */
	rslt = imsg_flush(unpriv_ibuf);
	if (rslt == -1 && errno != EPIPE)
		warning("cleanup: imsg_flush: %s", strerror(errno));
}

/*
 * priv_cleanup removes dhclient installed routes and address.
 */
void
priv_cleanup(struct imsg_hup *imsg)
{
	struct imsg_flush_routes fimsg;
	struct imsg_delete_address dimsg;

	memset(&fimsg, 0, sizeof(fimsg));
	fimsg.rdomain = imsg->rdomain;
	priv_flush_routes_and_arp_cache(&fimsg);

	if (imsg->addr.s_addr == INADDR_ANY)
		return;

	memset(&dimsg, 0, sizeof(dimsg));
	strlcpy(dimsg.ifname, imsg->ifname, sizeof(imsg->ifname));
	dimsg.rdomain = imsg->rdomain;
	dimsg.addr = imsg->addr;
	priv_delete_address(&dimsg);
}

int
resolv_conf_priority(int domain)
{
	struct iovec iov[3];
	struct {
		struct rt_msghdr	m_rtm;
		char			m_space[512];
	} m_rtmsg;
	struct sockaddr *rti_info[RTAX_MAX];
	struct sockaddr *sa;
	struct sockaddr_in sin;
	struct sockaddr_rtlabel *sa_rl;
	pid_t pid;
	ssize_t len;
	u_int32_t seq;
	int i, s, rslt, iovcnt = 0;

	rslt = 0;

	s = socket(PF_ROUTE, SOCK_RAW, AF_INET);
	if (s == -1) {
		warning("default route socket: %s", strerror(errno));
		return (0);
	}

	/* Build RTM header */

	memset(&m_rtmsg, 0, sizeof(m_rtmsg));

	m_rtmsg.m_rtm.rtm_version = RTM_VERSION;
	m_rtmsg.m_rtm.rtm_type = RTM_GET;
	m_rtmsg.m_rtm.rtm_msglen = sizeof(m_rtmsg.m_rtm);
	m_rtmsg.m_rtm.rtm_flags = RTF_STATIC | RTF_GATEWAY | RTF_UP;
	m_rtmsg.m_rtm.rtm_seq = seq = arc4random();
	m_rtmsg.m_rtm.rtm_tableid = domain;

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

	if (writev(s, iov, iovcnt) == -1) {
		if (errno != ESRCH)
			warning("RTM_GET of default route: %s",
			    strerror(errno));
		goto done;
	}

	pid = getpid();

	do {
		len = read(s, &m_rtmsg, sizeof(m_rtmsg));
		if (len == -1) {
			warning("get default route read: %s", strerror(errno));
			break;
		} else if (len == 0) {
			warning("no data from default route read");
			break;
		}
		if (m_rtmsg.m_rtm.rtm_version != RTM_VERSION)
			continue;
		if (m_rtmsg.m_rtm.rtm_type == RTM_GET &&
		    m_rtmsg.m_rtm.rtm_pid == pid &&
		    m_rtmsg.m_rtm.rtm_seq == seq) {
			if (m_rtmsg.m_rtm.rtm_errno) {
				warning("default route read rtm: %s",
				    strerror(m_rtmsg.m_rtm.rtm_errno));
				goto done;
			}
			break;
		}
	} while (1);

	sa = (struct sockaddr *)((char *)&m_rtmsg + m_rtmsg.m_rtm.rtm_hdrlen);
	memset(rti_info, 0, sizeof(rti_info));
	for (i = 0; i < RTAX_MAX; i++) {
		if (m_rtmsg.m_rtm.rtm_addrs & (1 << i)) {
			rti_info[i] = sa;
			sa = (struct sockaddr *)((char *)(sa) +
			    ROUNDUP(sa->sa_len));
		}
	}

	sa_rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
	if (check_route_label(sa_rl) == ROUTE_LABEL_DHCLIENT_OURS)
		rslt = 1;

done:
	close(s);
	return (rslt);
}

int
create_route_label(struct sockaddr_rtlabel *label)
{
	int len;

	memset(label, 0, sizeof(*label));

	label->sr_len = sizeof(label);
	label->sr_family = AF_UNSPEC;

	len = snprintf(label->sr_label, sizeof(label->sr_label), "DHCLIENT %d",
	    (int)getpid());

	if (len == -1) {
		warning("creating route label: %s", strerror(errno));
		return (1);
	}

	if (len >= sizeof(label->sr_label)) {
		warning("creating route label: label too long (%d vs %zd)", len,
		    sizeof(label->sr_label));
		return (1);
	}

	return (0);
}

int
check_route_label(struct sockaddr_rtlabel *label)
{
	pid_t pid;

	if (!label)
		return (ROUTE_LABEL_NONE);

	if (strncmp("DHCLIENT ", label->sr_label, 9))
		return (ROUTE_LABEL_NOT_DHCLIENT);

	pid = (pid_t)strtonum(label->sr_label + 9, 1, INT_MAX, NULL);
	if (pid <= 0)
		return (ROUTE_LABEL_DHCLIENT_UNKNOWN);

	if (pid == getpid())
		return (ROUTE_LABEL_DHCLIENT_OURS);

	if (kill(pid, 0) == -1) {
		if (errno == ESRCH)
			return (ROUTE_LABEL_DHCLIENT_DEAD);
		else
			return (ROUTE_LABEL_DHCLIENT_UNKNOWN);
	}

	return (ROUTE_LABEL_DHCLIENT_LIVE);
}
