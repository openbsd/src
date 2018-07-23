/*	$OpenBSD: frontend.c,v 1.22 2018/07/23 17:25:52 florian Exp $	*/

/*
 * Copyright (c) 2017 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2005 Claudio Jeker <claudio@openbsd.org>
 * Copyright (c) 2004 Esben Norby <norby@openbsd.org>
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
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
#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/route.h>

#include <arpa/inet.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <errno.h>
#include <event.h>
#include <ifaddrs.h>
#include <imsg.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "log.h"
#include "slaacd.h"
#include "frontend.h"
#include "control.h"

#define	ROUTE_SOCKET_BUF_SIZE	16384
#define	ALLROUTER		"ff02::2"

__dead void	 frontend_shutdown(void);
void		 frontend_sig_handler(int, short, void *);
void		 update_iface(uint32_t, char*);
void		 frontend_startup(void);
void		 route_receive(int, short, void *);
void		 handle_route_message(struct rt_msghdr *, struct sockaddr **);
void		 get_rtaddrs(int, struct sockaddr *, struct sockaddr **);
void		 icmp6_receive(int, short, void *);
int		 get_flags(char *);
int		 get_xflags(char *);
void		 get_lladdr(char *, struct ether_addr *, struct sockaddr_in6 *);
void		 send_solicitation(uint32_t);
#ifndef	SMALL
void		 update_autoconf_addresses(uint32_t, char*);
const char	*flags_to_str(int);
#endif	/* SMALL */

struct imsgev			*iev_main;
struct imsgev			*iev_engine;
struct event			 ev_route;
struct msghdr			 sndmhdr;
struct iovec			 sndiov[4];
struct nd_router_solicit	 rs;
struct nd_opt_hdr		 nd_opt_hdr;
struct ether_addr		 nd_opt_source_link_addr;
struct sockaddr_in6		 dst;
int				 icmp6sock = -1, ioctlsock;

struct icmp6_ev {
	struct event		 ev;
	uint8_t			 answer[1500];
	struct msghdr		 rcvmhdr;
	struct iovec		 rcviov[1];
	struct sockaddr_in6	 from;
} icmp6ev;

void
frontend_sig_handler(int sig, short event, void *bula)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		frontend_shutdown();
	default:
		fatalx("unexpected signal");
	}
}

void
frontend(int debug, int verbose)
{
	struct event		 ev_sigint, ev_sigterm;
	struct passwd		*pw;
	struct in6_pktinfo	*pi;
	struct cmsghdr		*cm;
	size_t			 rcvcmsglen, sndcmsglen;
	int			 hoplimit = 255;
	uint8_t			*rcvcmsgbuf, *sndcmsgbuf;

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

#ifndef	SMALL
	control_state.fd = -1;
#endif	/* SMALL */

	if ((pw = getpwnam(SLAACD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	slaacd_process = PROC_FRONTEND;
	setproctitle("%s", log_procnames[slaacd_process]);
	log_procinit(log_procnames[slaacd_process]);

	if ((ioctlsock = socket(AF_INET6, SOCK_DGRAM | SOCK_CLOEXEC, 0)) < 0)
		fatal("socket");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio unix recvfd route", NULL) == -1)
		fatal("pledge");

	event_init();

	/* Setup signal handler. */
	signal_set(&ev_sigint, SIGINT, frontend_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, frontend_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Setup pipe and event handler to the parent process. */
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);
	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = frontend_dispatch_main;
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	rcvcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));
	if((rcvcmsgbuf = malloc(rcvcmsglen)) == NULL)
		fatal("malloc");

	icmp6ev.rcviov[0].iov_base = (caddr_t)icmp6ev.answer;
	icmp6ev.rcviov[0].iov_len = sizeof(icmp6ev.answer);
	icmp6ev.rcvmhdr.msg_name = (caddr_t)&icmp6ev.from;
	icmp6ev.rcvmhdr.msg_namelen = sizeof(icmp6ev.from);
	icmp6ev.rcvmhdr.msg_iov = icmp6ev.rcviov;
	icmp6ev.rcvmhdr.msg_iovlen = 1;
	icmp6ev.rcvmhdr.msg_control = (caddr_t) rcvcmsgbuf;
	icmp6ev.rcvmhdr.msg_controllen = rcvcmsglen;

	sndcmsglen = CMSG_SPACE(sizeof(struct in6_pktinfo)) +
	    CMSG_SPACE(sizeof(int));

	if ((sndcmsgbuf = malloc(sndcmsglen)) == NULL)
		fatal("malloc");

	rs.nd_rs_type = ND_ROUTER_SOLICIT;
	rs.nd_rs_code = 0;
	rs.nd_rs_cksum = 0;
	rs.nd_rs_reserved = 0;

	nd_opt_hdr.nd_opt_type = ND_OPT_SOURCE_LINKADDR;
	nd_opt_hdr.nd_opt_len = 1;

	memset(&dst, 0, sizeof(dst));
	dst.sin6_family = AF_INET6;
	if (inet_pton(AF_INET6, ALLROUTER, &dst.sin6_addr.s6_addr) != 1)
		fatal("inet_pton");

	sndmhdr.msg_namelen = sizeof(struct sockaddr_in6);
	sndmhdr.msg_iov = sndiov;
	sndmhdr.msg_iovlen = 3;
	sndmhdr.msg_control = (caddr_t)sndcmsgbuf;
	sndmhdr.msg_controllen = sndcmsglen;

	sndmhdr.msg_name = (caddr_t)&dst;
	sndmhdr.msg_iov[0].iov_base = (caddr_t)&rs;
	sndmhdr.msg_iov[0].iov_len = sizeof(rs);
	sndmhdr.msg_iov[1].iov_base = (caddr_t)&nd_opt_hdr;
	sndmhdr.msg_iov[1].iov_len = sizeof(nd_opt_hdr);
	sndmhdr.msg_iov[2].iov_base = (caddr_t)&nd_opt_source_link_addr;
	sndmhdr.msg_iov[2].iov_len = sizeof(nd_opt_source_link_addr);

	cm = CMSG_FIRSTHDR(&sndmhdr);

	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_PKTINFO;
	cm->cmsg_len = CMSG_LEN(sizeof(struct in6_pktinfo));
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
	memset(&pi->ipi6_addr, 0, sizeof(pi->ipi6_addr));
	pi->ipi6_ifindex = 0;

	cm = CMSG_NXTHDR(&sndmhdr, cm);
	cm->cmsg_level = IPPROTO_IPV6;
	cm->cmsg_type = IPV6_HOPLIMIT;
	cm->cmsg_len = CMSG_LEN(sizeof(int));
	memcpy(CMSG_DATA(cm), &hoplimit, sizeof(int));

	event_dispatch();

	frontend_shutdown();
}

__dead void
frontend_shutdown(void)
{
	/* Close pipes. */
	msgbuf_write(&iev_engine->ibuf.w);
	msgbuf_clear(&iev_engine->ibuf.w);
	close(iev_engine->ibuf.fd);
	msgbuf_write(&iev_main->ibuf.w);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	free(iev_engine);
	free(iev_main);

	log_info("frontend exiting");
	exit(0);
}

int
frontend_imsg_compose_main(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1, data,
	    datalen));
}

int
frontend_imsg_compose_engine(int type, uint32_t peerid, pid_t pid,
    void *data, uint16_t datalen)
{
	return (imsg_compose_event(iev_engine, type, peerid, pid, -1,
	    data, datalen));
}

void
frontend_dispatch_main(int fd, short event, void *bula)
{
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	ssize_t			 n;
	int			 shut = 0;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
		case IMSG_SOCKET_IPC:
			/*
			 * Setup pipe and event handler to the engine
			 * process.
			 */
			if (iev_engine)
				fatalx("%s: received unexpected imsg fd "
				    "to frontend", __func__);

			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg fd to "
				   "frontend but didn't receive any",
				   __func__);

			iev_engine = malloc(sizeof(struct imsgev));
			if (iev_engine == NULL)
				fatal(NULL);

			imsg_init(&iev_engine->ibuf, fd);
			iev_engine->handler = frontend_dispatch_engine;
			iev_engine->events = EV_READ;

			event_set(&iev_engine->ev, iev_engine->ibuf.fd,
			iev_engine->events, iev_engine->handler, iev_engine);
			event_add(&iev_engine->ev, NULL);
			break;
		case IMSG_ICMP6SOCK:
			if ((icmp6sock = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "ICMPv6 fd but didn't receive any",
				    __func__);
			event_set(&icmp6ev.ev, icmp6sock, EV_READ | EV_PERSIST,
			    icmp6_receive, NULL);
			break;
		case IMSG_ROUTESOCK:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "routesocket fd but didn't receive any",
				    __func__);
			event_set(&ev_route, fd, EV_READ | EV_PERSIST,
			    route_receive, NULL);
			break;
		case IMSG_STARTUP:
			if (pledge("stdio unix route", NULL) == -1)
				fatal("pledge");
			frontend_startup();
			break;
#ifndef	SMALL
		case IMSG_CONTROLFD:
			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg "
				    "control fd but didn't receive any",
				    __func__);
			control_state.fd = fd;
			/* Listen on control socket. */
			TAILQ_INIT(&ctl_conns);
			control_listen();
			break;
		case IMSG_CTL_END:
			control_imsg_relay(&imsg);
			break;
#endif	/* SMALL */
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

void
frontend_dispatch_engine(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	ssize_t			 n;
	int			 shut = 0;
	uint32_t		 if_index;

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		switch (imsg.hdr.type) {
#ifndef	SMALL
		case IMSG_CTL_END:
		case IMSG_CTL_SHOW_INTERFACE_INFO:
		case IMSG_CTL_SHOW_INTERFACE_INFO_RA:
		case IMSG_CTL_SHOW_INTERFACE_INFO_RA_PREFIX:
		case IMSG_CTL_SHOW_INTERFACE_INFO_RA_RDNS:
		case IMSG_CTL_SHOW_INTERFACE_INFO_RA_DNSSL:
		case IMSG_CTL_SHOW_INTERFACE_INFO_ADDR_PROPOSALS:
		case IMSG_CTL_SHOW_INTERFACE_INFO_ADDR_PROPOSAL:
		case IMSG_CTL_SHOW_INTERFACE_INFO_DFR_PROPOSALS:
		case IMSG_CTL_SHOW_INTERFACE_INFO_DFR_PROPOSAL:
			control_imsg_relay(&imsg);
			break;
#endif	/* SMALL */
		case IMSG_CTL_SEND_SOLICITATION:
			if_index = *((uint32_t *)imsg.data);
			send_solicitation(if_index);
			break;
		case IMSG_FAKE_ACK:
			frontend_imsg_compose_engine(IMSG_PROPOSAL_ACK,
			   0, 0, imsg.data, sizeof(struct imsg_proposal_ack));
			break;
		default:
			log_debug("%s: error handling imsg %d", __func__,
			    imsg.hdr.type);
			break;
		}
		imsg_free(&imsg);
	}
	if (!shut)
		imsg_event_add(iev);
	else {
		/* This pipe is dead. Remove its event handler. */
		event_del(&iev->ev);
		event_loopexit(NULL);
	}
}

int
get_flags(char *if_name)
{
	struct ifreq		 ifr;

	(void) strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	if (ioctl(ioctlsock, SIOCGIFFLAGS, (caddr_t)&ifr) < 0)
		fatal("SIOCGIFFLAGS");
	return ifr.ifr_flags;
}

int
get_xflags(char *if_name)
{
	struct ifreq		 ifr;

	(void) strlcpy(ifr.ifr_name, if_name, sizeof(ifr.ifr_name));
	if (ioctl(ioctlsock, SIOCGIFXFLAGS, (caddr_t)&ifr) < 0)
		fatal("SIOCGIFXFLAGS");
	return ifr.ifr_flags;
}

void
update_iface(uint32_t if_index, char* if_name)
{
	struct imsg_ifinfo	 imsg_ifinfo;
	int			 flags, xflags;

	flags = get_flags(if_name);
	xflags = get_xflags(if_name);

	if (!(xflags & IFXF_AUTOCONF6))
		return;

	memset(&imsg_ifinfo, 0, sizeof(imsg_ifinfo));

	imsg_ifinfo.if_index = if_index;
	imsg_ifinfo.running = (flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP |
	    IFF_RUNNING);
	imsg_ifinfo.autoconfprivacy = !(xflags & IFXF_INET6_NOPRIVACY);
	imsg_ifinfo.soii = !(xflags & IFXF_INET6_NOSOII);
	get_lladdr(if_name, &imsg_ifinfo.hw_address, &imsg_ifinfo.ll_address);

	memcpy(&nd_opt_source_link_addr, &imsg_ifinfo.hw_address,
	    sizeof(nd_opt_source_link_addr));

	frontend_imsg_compose_main(IMSG_UPDATE_IF, 0, &imsg_ifinfo,
	    sizeof(imsg_ifinfo));
}

#ifndef	SMALL
void
update_autoconf_addresses(uint32_t if_index, char* if_name)
{
	struct in6_ifreq	 ifr6;
	struct imsg_addrinfo	 imsg_addrinfo;
	struct ifaddrs		*ifap, *ifa;
	struct in6_addrlifetime *lifetime;
	struct sockaddr_in6	*sin6;
	struct imsg_link_state	 imsg_link_state;
	time_t			 t;
	int			 xflags;

	xflags = get_xflags(if_name);

	if (!(xflags & IFXF_AUTOCONF6))
		return;

	memset(&imsg_addrinfo, 0, sizeof(imsg_addrinfo));
	imsg_addrinfo.if_index = if_index;
	get_lladdr(if_name, &imsg_addrinfo.hw_address,
	    &imsg_addrinfo.ll_address);

	memset(&imsg_link_state, 0, sizeof(imsg_link_state));
	imsg_link_state.if_index = if_index;

	if (getifaddrs(&ifap) != 0)
		fatal("getifaddrs");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(if_name, ifa->ifa_name) != 0)
			continue;

		if (ifa->ifa_addr->sa_family == AF_LINK)
			imsg_link_state.link_state =
			    ((struct if_data *)ifa->ifa_data)->ifi_link_state;

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;
		sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
		if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
			continue;

		log_debug("%s: IP: %s", __func__, sin6_to_str(sin6));
		imsg_addrinfo.addr = *sin6;

		memset(&ifr6, 0, sizeof(ifr6));
		(void) strlcpy(ifr6.ifr_name, if_name, sizeof(ifr6.ifr_name));
		memcpy(&ifr6.ifr_addr, sin6, sizeof(ifr6.ifr_addr));

		if (ioctl(ioctlsock, SIOCGIFAFLAG_IN6, (caddr_t)&ifr6) < 0) {
			log_warn("SIOCGIFAFLAG_IN6");
			continue;
		}

		if (!(ifr6.ifr_ifru.ifru_flags6 & (IN6_IFF_AUTOCONF |
		    IN6_IFF_PRIVACY)))
			continue;

		imsg_addrinfo.privacy = ifr6.ifr_ifru.ifru_flags6 &
		    IN6_IFF_PRIVACY ? 1 : 0;

		memset(&ifr6, 0, sizeof(ifr6));
		(void) strlcpy(ifr6.ifr_name, if_name, sizeof(ifr6.ifr_name));
		memcpy(&ifr6.ifr_addr, sin6, sizeof(ifr6.ifr_addr));
		
		if (ioctl(ioctlsock, SIOCGIFNETMASK_IN6, (caddr_t)&ifr6) < 0) {
			log_warn("SIOCGIFNETMASK_IN6");
			continue;
		}

		imsg_addrinfo.mask = ((struct sockaddr_in6 *)&ifr6.ifr_addr)
		    ->sin6_addr;

		memset(&ifr6, 0, sizeof(ifr6));
		(void) strlcpy(ifr6.ifr_name, if_name, sizeof(ifr6.ifr_name));
		memcpy(&ifr6.ifr_addr, sin6, sizeof(ifr6.ifr_addr));
		lifetime = &ifr6.ifr_ifru.ifru_lifetime;

		if (ioctl(ioctlsock, SIOCGIFALIFETIME_IN6, (caddr_t)&ifr6) <
		    0) {
			log_warn("SIOCGIFALIFETIME_IN6");
			continue;
		}

		imsg_addrinfo.vltime = ND6_INFINITE_LIFETIME;
		imsg_addrinfo.pltime = ND6_INFINITE_LIFETIME;
		t = time(NULL);

		if (lifetime->ia6t_preferred)
			imsg_addrinfo.pltime = lifetime->ia6t_preferred < t ? 0
			    : lifetime->ia6t_preferred - t;

		if (lifetime->ia6t_expire)
			imsg_addrinfo.vltime = lifetime->ia6t_expire < t ? 0 :
			    lifetime->ia6t_expire - t;

		frontend_imsg_compose_main(IMSG_UPDATE_ADDRESS, 0,
		    &imsg_addrinfo, sizeof(imsg_addrinfo));

	}
	freeifaddrs(ifap);

	log_debug("%s: %s link state down? %s", __func__, if_name,
	    imsg_link_state.link_state == LINK_STATE_DOWN ? "yes" : "no");

	frontend_imsg_compose_main(IMSG_UPDATE_LINK_STATE, 0, 
	    &imsg_link_state, sizeof(imsg_link_state));
}

const char*
flags_to_str(int flags)
{
	static char	buf[sizeof(" anycast tentative duplicated detached "
			    "deprecated autoconf autoconfprivacy")];

	buf[0] = '\0';
	if (flags & IN6_IFF_ANYCAST)
		(void)strlcat(buf, " anycast", sizeof(buf));
	if (flags & IN6_IFF_TENTATIVE)
		(void)strlcat(buf, " tentative", sizeof(buf));
	if (flags & IN6_IFF_DUPLICATED)
		(void)strlcat(buf, " duplicated", sizeof(buf));
	if (flags & IN6_IFF_DETACHED)
		(void)strlcat(buf, " detached", sizeof(buf));
	if (flags & IN6_IFF_DEPRECATED)
		(void)strlcat(buf, " deprecated", sizeof(buf));
	if (flags & IN6_IFF_AUTOCONF)
		(void)strlcat(buf, " autoconf", sizeof(buf));
	if (flags & IN6_IFF_PRIVACY)
		(void)strlcat(buf, " autoconfprivacy", sizeof(buf));

	return (buf);
}
#endif	/* SMALL */

void
frontend_startup(void)
{
	struct if_nameindex	*ifnidxp, *ifnidx;

	if (!event_initialized(&ev_route))
		fatalx("%s: did not receive a route socket from the main "
		    "process", __func__);

	event_add(&ev_route, NULL);

	if (!event_initialized(&icmp6ev.ev))
		fatalx("%s: did not receive a icmp6 socket fd from the main "
		    "process", __func__);

	event_add(&icmp6ev.ev, NULL);

	if ((ifnidxp = if_nameindex()) == NULL)
		fatalx("if_nameindex");

	frontend_imsg_compose_main(IMSG_STARTUP_DONE, 0, NULL, 0);

	for(ifnidx = ifnidxp; ifnidx->if_index !=0 && ifnidx->if_name != NULL;
	    ifnidx++) {
		update_iface(ifnidx->if_index, ifnidx->if_name);
#ifndef	SMALL
		update_autoconf_addresses(ifnidx->if_index, ifnidx->if_name);
#endif	/* SMALL */
	}

	if_freenameindex(ifnidxp);
}

void
route_receive(int fd, short events, void *arg)
{
	static uint8_t			 *buf;

	struct rt_msghdr		*rtm;
	struct sockaddr			*sa, *rti_info[RTAX_MAX];
	ssize_t				 n;

	if (buf == NULL) {
		buf = malloc(ROUTE_SOCKET_BUF_SIZE);
		if (buf == NULL)
			fatal("malloc");
	}
	rtm = (struct rt_msghdr *)buf;
	if ((n = read(fd, buf, ROUTE_SOCKET_BUF_SIZE)) == -1) {
		if (errno == EAGAIN || errno == EINTR)
			return;
		log_warn("dispatch_rtmsg: read error");
		return;
	}

	if (n == 0)
		fatal("routing socket closed");

	if (n < (ssize_t)sizeof(rtm->rtm_msglen) || n < rtm->rtm_msglen) {
		log_warnx("partial rtm of %zd in buffer", n);
		return;
	}

	if (rtm->rtm_version != RTM_VERSION)
		return;

	sa = (struct sockaddr *)(buf + rtm->rtm_hdrlen);
	get_rtaddrs(rtm->rtm_addrs, sa, rti_info);

	handle_route_message(rtm, rti_info);
}

void
handle_route_message(struct rt_msghdr *rtm, struct sockaddr **rti_info)
{
	struct if_msghdr		*ifm;
	struct imsg_proposal_ack	 proposal_ack;
	struct imsg_del_addr		 del_addr;
	struct imsg_del_route		 del_route;
	struct imsg_dup_addr		 dup_addr;
	struct sockaddr_rtlabel		*rl;
	struct sockaddr_in6		*sin6;
	struct in6_ifreq		 ifr6;
	struct in6_addr			*in6;
	int64_t				 id, pid;
	int				 xflags, if_index;
	char				 ifnamebuf[IFNAMSIZ];
	char				*if_name;
	char				**ap, *argv[4], *p;
	const char			*errstr;

	switch (rtm->rtm_type) {
	case RTM_IFINFO:
		ifm = (struct if_msghdr *)rtm;
		if_name = if_indextoname(ifm->ifm_index, ifnamebuf);
		if (if_name == NULL) {
			log_debug("RTM_IFINFO: lost if %d", ifm->ifm_index);
			if_index = ifm->ifm_index;
			frontend_imsg_compose_engine(IMSG_REMOVE_IF, 0, 0,
			    &if_index, sizeof(if_index));
		} else {
			xflags = get_xflags(if_name);
			if (!(xflags & IFXF_AUTOCONF6)) {
				log_debug("RTM_IFINFO: %s(%d) no(longer) "
				   "autoconf6", if_name, ifm->ifm_index);
				if_index = ifm->ifm_index;
				frontend_imsg_compose_engine(IMSG_REMOVE_IF, 0,
				    0, &if_index, sizeof(if_index));
			} else {
				update_iface(ifm->ifm_index, if_name);
#ifndef	SMALL
				update_autoconf_addresses(ifm->ifm_index,
				    if_name);
#endif	/* SMALL */
			}
		}
		break;
	case RTM_NEWADDR:
		ifm = (struct if_msghdr *)rtm;
		if_name = if_indextoname(ifm->ifm_index, ifnamebuf);
		log_debug("RTM_NEWADDR: %s[%u]", if_name, ifm->ifm_index);
		update_iface(ifm->ifm_index, if_name);
		break;
	case RTM_DELADDR:
		ifm = (struct if_msghdr *)rtm;
		if_name = if_indextoname(ifm->ifm_index, ifnamebuf);
		if (rtm->rtm_addrs & RTA_IFA && rti_info[RTAX_IFA]->sa_family
		    == AF_INET6) {
			del_addr.if_index = ifm->ifm_index;
			memcpy(&del_addr.addr, rti_info[RTAX_IFA], sizeof(
			    del_addr.addr));
			frontend_imsg_compose_engine(IMSG_DEL_ADDRESS,
				    0, 0, &del_addr, sizeof(del_addr));
			log_debug("RTM_DELADDR: %s[%u]", if_name,
			    ifm->ifm_index);
		}
		break;
	case RTM_CHGADDRATTR:
		ifm = (struct if_msghdr *)rtm;
		if_name = if_indextoname(ifm->ifm_index, ifnamebuf);
		if (rtm->rtm_addrs & RTA_IFA && rti_info[RTAX_IFA]->sa_family
		    == AF_INET6) {
			sin6 = (struct sockaddr_in6 *) rti_info[RTAX_IFA];

			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
				break;

			memset(&ifr6, 0, sizeof(ifr6));
			(void) strlcpy(ifr6.ifr_name, if_name,
			    sizeof(ifr6.ifr_name));
			memcpy(&ifr6.ifr_addr, sin6, sizeof(ifr6.ifr_addr));

			if (ioctl(ioctlsock, SIOCGIFAFLAG_IN6, (caddr_t)&ifr6)
			    < 0) {
				log_warn("SIOCGIFAFLAG_IN6");
				break;
			}

#ifndef	SMALL
			log_debug("RTM_CHGADDRATTR: %s -%s",
			    sin6_to_str(sin6),
			    flags_to_str(ifr6.ifr_ifru.ifru_flags6));
#endif	/* SMALL */

			if (ifr6.ifr_ifru.ifru_flags6 & IN6_IFF_DUPLICATED) {
				dup_addr.if_index = ifm->ifm_index;
				dup_addr.addr = *sin6;
				frontend_imsg_compose_engine(IMSG_DUP_ADDRESS,
				    0, 0, &dup_addr, sizeof(dup_addr));
			}

		}
		break;
	case RTM_DELETE:
		ifm = (struct if_msghdr *)rtm;
		if ((rtm->rtm_addrs & (RTA_DST | RTA_GATEWAY | RTA_LABEL)) !=
		    (RTA_DST | RTA_GATEWAY | RTA_LABEL))
			break;
		if (rti_info[RTAX_DST]->sa_family != AF_INET6)
			break;
		if (!IN6_IS_ADDR_UNSPECIFIED(&((struct sockaddr_in6 *)
		    rti_info[RTAX_DST])->sin6_addr))
			break;
		if (rti_info[RTAX_GATEWAY]->sa_family != AF_INET6)
			break;
		if (rti_info[RTAX_LABEL]->sa_len !=
		    sizeof(struct sockaddr_rtlabel))
			break;

		rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
		if (strcmp(rl->sr_label, SLAACD_RTA_LABEL) != 0)
			break;

		if_name = if_indextoname(ifm->ifm_index, ifnamebuf);

		del_route.if_index = ifm->ifm_index;
		memcpy(&del_route.gw, rti_info[RTAX_GATEWAY],
		    sizeof(del_route.gw));
		in6 = &del_route.gw.sin6_addr;
		/* XXX from route(8) p_sockaddr() */
		if (IN6_IS_ADDR_LINKLOCAL(in6) ||
		    IN6_IS_ADDR_MC_LINKLOCAL(in6) ||
		    IN6_IS_ADDR_MC_INTFACELOCAL(in6)) {
			del_route.gw.sin6_scope_id =
			    (u_int32_t)ntohs(*(u_short *) &in6->s6_addr[2]);
			*(u_short *)&in6->s6_addr[2] = 0;
		}
		frontend_imsg_compose_engine(IMSG_DEL_ROUTE,
		    0, 0, &del_route, sizeof(del_route));
		log_debug("RTM_DELETE: %s[%u]", if_name,
		    ifm->ifm_index);

		break;
	case RTM_PROPOSAL:
		ifm = (struct if_msghdr *)rtm;
		if_name = if_indextoname(ifm->ifm_index, ifnamebuf);

		if ((rtm->rtm_flags & (RTF_DONE | RTF_PROTO1)) ==
		    (RTF_DONE | RTF_PROTO1) && rtm->rtm_addrs == RTA_LABEL) {
			rl = (struct sockaddr_rtlabel *)rti_info[RTAX_LABEL];
			/* XXX validate rl */

			p = rl->sr_label;

			for (ap = argv; ap < &argv[3] && (*ap =
			    strsep(&p, " ")) != NULL;) {
				if (**ap != '\0')
					ap++;
			}
			*ap = NULL;

			if (argv[0] != NULL && strncmp(argv[0],
			    SLAACD_RTA_LABEL":", strlen(SLAACD_RTA_LABEL":"))
			    == 0 && argv[1] != NULL && argv[2] != NULL &&
			    argv[3] == NULL) {
				id = strtonum(argv[1], 0, INT64_MAX, &errstr);
				if (errstr != NULL) {
					log_warn("%s: proposal seq is %s: %s",
					    __func__, errstr, argv[1]);
					break;
				}
				pid = strtonum(argv[2], 0, INT32_MAX, &errstr);
				if (errstr != NULL) {
					log_warn("%s: pid is %s: %s",
					    __func__, errstr, argv[2]);
					break;
				}
				proposal_ack.id = id;
				proposal_ack.pid = pid;
				proposal_ack.if_index = ifm->ifm_index;

				frontend_imsg_compose_engine(IMSG_PROPOSAL_ACK,
				    0, 0, &proposal_ack, sizeof(proposal_ack));
			} else {
				log_debug("cannot parse: %s", rl->sr_label);
			}
		} else {
#if 0
			log_debug("%s: got flags %x, expcted %x", __func__,
			    rtm->rtm_flags, (RTF_DONE | RTF_PROTO1));
#endif
		}

		break;
	default:
		log_debug("unexpected RTM: %d", rtm->rtm_type);
		break;
	}

}

#define ROUNDUP(a) \
	((a) > 0 ? (1 + (((a) - 1) | (sizeof(long) - 1))) : sizeof(long))

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

void
get_lladdr(char *if_name, struct ether_addr *mac, struct sockaddr_in6 *ll)
{
	struct ifaddrs		*ifap, *ifa;
	struct sockaddr_dl	*sdl;
	struct sockaddr_in6	*sin6;

	if (getifaddrs(&ifap) != 0)
		fatal("getifaddrs");

	memset(mac, 0, sizeof(*mac));
	memset(ll, 0, sizeof(*ll));

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (strcmp(if_name, ifa->ifa_name) != 0)
			continue;

		switch(ifa->ifa_addr->sa_family) {
		case AF_LINK:
			sdl = (struct sockaddr_dl *)ifa->ifa_addr;
			if (sdl->sdl_type != IFT_ETHER ||
			    sdl->sdl_alen != ETHER_ADDR_LEN)
				continue;

			memcpy(mac->ether_addr_octet, LLADDR(sdl),
			    ETHER_ADDR_LEN);
			break;
		case AF_INET6:
			sin6 = (struct sockaddr_in6 *)ifa->ifa_addr;
			if (IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr)) {
				sin6->sin6_scope_id = ntohs(*(u_int16_t *)
				    &sin6->sin6_addr.s6_addr[2]);
				sin6->sin6_addr.s6_addr[2] =
				    sin6->sin6_addr.s6_addr[3] = 0;
				memcpy(ll, sin6, sizeof(*ll));
			}
			break;
		default:
			break;
		}
	}
	freeifaddrs(ifap);
}

void
icmp6_receive(int fd, short events, void *arg)
{
	struct imsg_ra		 ra;

	struct in6_pktinfo	*pi = NULL;
	struct cmsghdr		*cm;
	ssize_t			 len;
	int			 if_index = 0, *hlimp = NULL;
	char			 ntopbuf[INET6_ADDRSTRLEN], ifnamebuf[IFNAMSIZ];

	if ((len = recvmsg(fd, &icmp6ev.rcvmhdr, 0)) < 0) {
		log_warn("recvmsg");
		return;
	}

	/* extract optional information via Advanced API */
	for (cm = (struct cmsghdr *)CMSG_FIRSTHDR(&icmp6ev.rcvmhdr); cm;
	    cm = (struct cmsghdr *)CMSG_NXTHDR(&icmp6ev.rcvmhdr, cm)) {
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_PKTINFO &&
		    cm->cmsg_len == CMSG_LEN(sizeof(struct in6_pktinfo))) {
			pi = (struct in6_pktinfo *)(CMSG_DATA(cm));
			if_index = pi->ipi6_ifindex;
		}
		if (cm->cmsg_level == IPPROTO_IPV6 &&
		    cm->cmsg_type == IPV6_HOPLIMIT &&
		    cm->cmsg_len == CMSG_LEN(sizeof(int)))
			hlimp = (int *)CMSG_DATA(cm);
	}

	if (if_index == 0) {
		log_warnx("failed to get receiving interface");
		return;
	}

	if (hlimp == NULL) {
		log_warnx("failed to get receiving hop limit");
		return;
	}

	if (*hlimp != 255) {
		log_warnx("invalid RA with hop limit of %d from %s on %s",
		    *hlimp, inet_ntop(AF_INET6, &icmp6ev.from.sin6_addr,
		    ntopbuf, INET6_ADDRSTRLEN), if_indextoname(if_index,
		    ifnamebuf));
		return;
	}

	if ((size_t)len > sizeof(ra.packet)) {
		log_warnx("invalid RA with size %ld from %s on %s",
		    len, inet_ntop(AF_INET6, &icmp6ev.from.sin6_addr,
		    ntopbuf, INET6_ADDRSTRLEN), if_indextoname(if_index,
		    ifnamebuf));
		return;
	}

	ra.if_index = if_index;
	memcpy(&ra.from,  &icmp6ev.from, sizeof(ra.from));
	ra.len = len;
	memcpy(ra.packet, icmp6ev.answer, len);

	frontend_imsg_compose_engine(IMSG_RA, 0, 0, &ra, sizeof(ra));
}

void
send_solicitation(uint32_t if_index)
{
	struct in6_pktinfo		*pi;
	struct cmsghdr			*cm;

	log_debug("%s(%u)", __func__, if_index);

	dst.sin6_scope_id = if_index;

	cm = CMSG_FIRSTHDR(&sndmhdr);
	pi = (struct in6_pktinfo *)CMSG_DATA(cm);
	pi->ipi6_ifindex = if_index;

	if (sendmsg(icmp6sock, &sndmhdr, 0) != sizeof(rs) +
	    sizeof(nd_opt_hdr) + sizeof(nd_opt_source_link_addr))
		log_warn("sendmsg");
}
