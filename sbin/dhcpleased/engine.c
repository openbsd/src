/*	$OpenBSD: engine.c,v 1.11 2021/03/22 15:34:07 otto Exp $	*/

/*
 * Copyright (c) 2017, 2021 Florian Obser <florian@openbsd.org>
 * Copyright (c) 2004, 2005 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <net/if.h>
#include <net/route.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/udp.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <vis.h>

#include "checksum.h"
#include "log.h"
#include "dhcpleased.h"
#include "engine.h"

/*
 * RFC 2131 4.1 p23 has "SHOULD be 4 seconds", we are a bit more aggressive,
 * networks are faster these days.
 */
#define	START_EXP_BACKOFF	 1
#define	MAX_EXP_BACKOFF_SLOW	64 /* RFC 2131 4.1 p23 */
#define	MAX_EXP_BACKOFF_FAST	 2
#define	MINIMUM(a, b)		(((a) < (b)) ? (a) : (b))

enum if_state {
	IF_DOWN,
	IF_INIT,
	/* IF_SELECTING, */
	IF_REQUESTING,
	IF_BOUND,
	IF_RENEWING,
	IF_REBINDING,
	/* IF_INIT_REBOOT, */
	IF_REBOOTING,
};

const char* if_state_name[] = {
	"Down",
	"Init",
	/* "Selecting", */
	"Requesting",
	"Bound",
	"Renewing",
	"Rebinding",
	/* "Init-Reboot", */
	"Rebooting",
};

struct dhcpleased_iface {
	LIST_ENTRY(dhcpleased_iface)	 entries;
	enum if_state			 state;
	struct event			 timer;
	struct timeval			 timo;
	uint32_t			 if_index;
	int				 rdomain;
	int				 running;
	struct ether_addr		 hw_address;
	int				 link_state;
	uint32_t			 cur_mtu;
	uint32_t			 xid;
	struct timespec			 request_time;
	struct in_addr			 server_identifier;
	struct in_addr			 dhcp_server; /* for unicast */
	struct in_addr			 requested_ip;
	struct in_addr			 mask;
	struct in_addr			 router;
	struct in_addr			 nameservers[MAX_RDNS_COUNT];
	uint32_t			 lease_time;
	uint32_t			 renewal_time;
	uint32_t			 rebinding_time;
};

LIST_HEAD(, dhcpleased_iface) dhcpleased_interfaces;

__dead void		 engine_shutdown(void);
void			 engine_sig_handler(int sig, short, void *);
void			 engine_dispatch_frontend(int, short, void *);
void			 engine_dispatch_main(int, short, void *);
#ifndef	SMALL
void			 send_interface_info(struct dhcpleased_iface *, pid_t);
void			 engine_showinfo_ctl(struct imsg *, uint32_t);
#endif	/* SMALL */
void			 engine_update_iface(struct imsg_ifinfo *);
struct dhcpleased_iface	*get_dhcpleased_iface_by_id(uint32_t);
void			 remove_dhcpleased_iface(uint32_t);
void			 parse_dhcp(struct dhcpleased_iface *,
			     struct imsg_dhcp *);
void			 state_transition(struct dhcpleased_iface *, enum
			     if_state);
void			 iface_timeout(int, short, void *);
void			 request_dhcp_discover(struct dhcpleased_iface *);
void			 request_dhcp_request(struct dhcpleased_iface *);
void			 log_lease(struct dhcpleased_iface *, int);
void			 log_rdns(struct dhcpleased_iface *, int);
void			 send_configure_interface(struct dhcpleased_iface *);
void			 send_rdns_proposal(struct dhcpleased_iface *);
void			 send_deconfigure_interface(struct dhcpleased_iface *);
void			 send_rdns_withdraw(struct dhcpleased_iface *);
void			 parse_lease(struct dhcpleased_iface *,
			     struct imsg_ifinfo *);
int			 engine_imsg_compose_main(int, pid_t, void *, uint16_t);
void			 log_dhcp_hdr(struct dhcp_hdr *);
const char		*dhcp_message_type2str(uint8_t);

static struct imsgev	*iev_frontend;
static struct imsgev	*iev_main;
int64_t			 proposal_id;

void
engine_sig_handler(int sig, short event, void *arg)
{
	/*
	 * Normal signal handler rules don't apply because libevent
	 * decouples for us.
	 */

	switch (sig) {
	case SIGINT:
	case SIGTERM:
		engine_shutdown();
	default:
		fatalx("unexpected signal");
	}
}

void
engine(int debug, int verbose)
{
	struct event		 ev_sigint, ev_sigterm;
	struct passwd		*pw;

	log_init(debug, LOG_DAEMON);
	log_setverbose(verbose);

	if ((pw = getpwnam(DHCPLEASED_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("%s", "engine");
	log_procinit("engine");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	if (pledge("stdio recvfd", NULL) == -1)
		fatal("pledge");

	event_init();

	/* Setup signal handler(s). */
	signal_set(&ev_sigint, SIGINT, engine_sig_handler, NULL);
	signal_set(&ev_sigterm, SIGTERM, engine_sig_handler, NULL);
	signal_add(&ev_sigint, NULL);
	signal_add(&ev_sigterm, NULL);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);

	/* Setup pipe and event handler to the main process. */
	if ((iev_main = malloc(sizeof(struct imsgev))) == NULL)
		fatal(NULL);

	imsg_init(&iev_main->ibuf, 3);
	iev_main->handler = engine_dispatch_main;

	/* Setup event handlers. */
	iev_main->events = EV_READ;
	event_set(&iev_main->ev, iev_main->ibuf.fd, iev_main->events,
	    iev_main->handler, iev_main);
	event_add(&iev_main->ev, NULL);

	LIST_INIT(&dhcpleased_interfaces);

	event_dispatch();

	engine_shutdown();
}

__dead void
engine_shutdown(void)
{
	/* Close pipes. */
	msgbuf_clear(&iev_frontend->ibuf.w);
	close(iev_frontend->ibuf.fd);
	msgbuf_clear(&iev_main->ibuf.w);
	close(iev_main->ibuf.fd);

	free(iev_frontend);
	free(iev_main);

	log_info("engine exiting");
	exit(0);
}

int
engine_imsg_compose_frontend(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_frontend, type, 0, pid, -1,
	    data, datalen));
}

int
engine_imsg_compose_main(int type, pid_t pid, void *data,
    uint16_t datalen)
{
	return (imsg_compose_event(iev_main, type, 0, pid, -1,
	    data, datalen));
}

void
engine_dispatch_frontend(int fd, short event, void *bula)
{
	struct imsgev			*iev = bula;
	struct imsgbuf			*ibuf = &iev->ibuf;
	struct imsg			 imsg;
	struct dhcpleased_iface		*iface;
	ssize_t				 n;
	int				 shut = 0;
#ifndef	SMALL
	int				 verbose;
#endif	/* SMALL */
	uint32_t			 if_index;

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
		case IMSG_CTL_LOG_VERBOSE:
			if (IMSG_DATA_SIZE(imsg) != sizeof(verbose))
				fatalx("%s: IMSG_CTL_LOG_VERBOSE wrong length: "
				    "%lu", __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		case IMSG_CTL_SHOW_INTERFACE_INFO:
			if (IMSG_DATA_SIZE(imsg) != sizeof(if_index))
				fatalx("%s: IMSG_CTL_SHOW_INTERFACE_INFO wrong "
				    "length: %lu", __func__,
				    IMSG_DATA_SIZE(imsg));
			memcpy(&if_index, imsg.data, sizeof(if_index));
			engine_showinfo_ctl(&imsg, if_index);
			break;
		case IMSG_CTL_SEND_REQUEST:
			if (IMSG_DATA_SIZE(imsg) != sizeof(if_index))
				fatalx("%s: IMSG_CTL_SEND_DISCOVER wrong "
				    "length: %lu", __func__,
				    IMSG_DATA_SIZE(imsg));
			memcpy(&if_index, imsg.data, sizeof(if_index));
			iface = get_dhcpleased_iface_by_id(if_index);
			if (iface != NULL) {
				switch (iface->state) {
				case IF_DOWN:
					break;
				case IF_INIT:
				case IF_REQUESTING:
				case IF_RENEWING:
				case IF_REBINDING:
				case IF_REBOOTING:
					state_transition(iface, iface->state);
					break;
				case IF_BOUND:
					state_transition(iface, IF_RENEWING);
					break;
				}
			}
			break;
#endif	/* SMALL */
		case IMSG_REMOVE_IF:
			if (IMSG_DATA_SIZE(imsg) != sizeof(if_index))
				fatalx("%s: IMSG_REMOVE_IF wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&if_index, imsg.data, sizeof(if_index));
			remove_dhcpleased_iface(if_index);
			break;
		case IMSG_DHCP: {
			struct imsg_dhcp	imsg_dhcp;
			if (IMSG_DATA_SIZE(imsg) != sizeof(imsg_dhcp))
				fatalx("%s: IMSG_DHCP wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&imsg_dhcp, imsg.data, sizeof(imsg_dhcp));
			iface = get_dhcpleased_iface_by_id(imsg_dhcp.if_index);
			if (iface != NULL)
				parse_dhcp(iface, &imsg_dhcp);
			break;
		}
		case IMSG_REPROPOSE_RDNS:
			LIST_FOREACH (iface, &dhcpleased_interfaces, entries)
				send_rdns_proposal(iface);
			break;
		default:
			log_debug("%s: unexpected imsg %d", __func__,
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
engine_dispatch_main(int fd, short event, void *bula)
{
	struct imsg		 imsg;
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg_ifinfo	 imsg_ifinfo;
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
			 * Setup pipe and event handler to the frontend
			 * process.
			 */
			if (iev_frontend)
				fatalx("%s: received unexpected imsg fd "
				    "to engine", __func__);

			if ((fd = imsg.fd) == -1)
				fatalx("%s: expected to receive imsg fd to "
				   "engine but didn't receive any", __func__);

			iev_frontend = malloc(sizeof(struct imsgev));
			if (iev_frontend == NULL)
				fatal(NULL);

			imsg_init(&iev_frontend->ibuf, fd);
			iev_frontend->handler = engine_dispatch_frontend;
			iev_frontend->events = EV_READ;

			event_set(&iev_frontend->ev, iev_frontend->ibuf.fd,
			iev_frontend->events, iev_frontend->handler,
			    iev_frontend);
			event_add(&iev_frontend->ev, NULL);

			if (pledge("stdio", NULL) == -1)
				fatal("pledge");

			break;
		case IMSG_UPDATE_IF:
			if (IMSG_DATA_SIZE(imsg) != sizeof(imsg_ifinfo))
				fatalx("%s: IMSG_UPDATE_IF wrong length: %lu",
				    __func__, IMSG_DATA_SIZE(imsg));
			memcpy(&imsg_ifinfo, imsg.data, sizeof(imsg_ifinfo));
			engine_update_iface(&imsg_ifinfo);
			break;
		default:
			log_debug("%s: unexpected imsg %d", __func__,
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

#ifndef	SMALL
void
send_interface_info(struct dhcpleased_iface *iface, pid_t pid)
{
	struct ctl_engine_info	 cei;

	memset(&cei, 0, sizeof(cei));
	cei.if_index = iface->if_index;
	cei.running = iface->running;
	cei.link_state = iface->link_state;
	strlcpy(cei.state, if_state_name[iface->state], sizeof(cei.state));
	memcpy(&cei.request_time, &iface->request_time,
	    sizeof(cei.request_time));
	cei.server_identifier.s_addr = iface->server_identifier.s_addr;
	cei.dhcp_server.s_addr = iface->dhcp_server.s_addr;
	cei.requested_ip.s_addr = iface->requested_ip.s_addr;
	cei.mask.s_addr = iface->mask.s_addr;
	cei.router.s_addr = iface->router.s_addr;
	memcpy(cei.nameservers, iface->nameservers, sizeof(cei.nameservers));
	cei.lease_time = iface->lease_time;
	cei.renewal_time = iface->renewal_time;
	cei.rebinding_time = iface->rebinding_time;
	engine_imsg_compose_frontend(IMSG_CTL_SHOW_INTERFACE_INFO, pid, &cei,
	    sizeof(cei));
}

void
engine_showinfo_ctl(struct imsg *imsg, uint32_t if_index)
{
	struct dhcpleased_iface			*iface;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_INTERFACE_INFO:
		if (if_index == 0) {
			LIST_FOREACH (iface, &dhcpleased_interfaces, entries)
				send_interface_info(iface, imsg->hdr.pid);
		} else {
			if ((iface = get_dhcpleased_iface_by_id(if_index)) !=
			    NULL)
				send_interface_info(iface, imsg->hdr.pid);
		}
		engine_imsg_compose_frontend(IMSG_CTL_END, imsg->hdr.pid, NULL,
		    0);
		break;
	default:
		log_debug("%s: error handling imsg", __func__);
		break;
	}
}
#endif	/* SMALL */

void
engine_update_iface(struct imsg_ifinfo *imsg_ifinfo)
{
	struct dhcpleased_iface	*iface;
	int			 need_refresh = 0;

	iface = get_dhcpleased_iface_by_id(imsg_ifinfo->if_index);

	if (iface == NULL) {
		if ((iface = calloc(1, sizeof(*iface))) == NULL)
			fatal("calloc");
		iface->state = IF_DOWN;
		iface->timo.tv_usec = arc4random_uniform(1000000);
		evtimer_set(&iface->timer, iface_timeout, iface);
		iface->if_index = imsg_ifinfo->if_index;
		iface->rdomain = imsg_ifinfo->rdomain;
		iface->running = imsg_ifinfo->running;
		iface->link_state = imsg_ifinfo->link_state;
		iface->requested_ip.s_addr = INADDR_ANY;
		memcpy(&iface->hw_address, &imsg_ifinfo->hw_address,
		    sizeof(struct ether_addr));
		LIST_INSERT_HEAD(&dhcpleased_interfaces, iface, entries);
		need_refresh = 1;
	} else {
		if (memcmp(&iface->hw_address, &imsg_ifinfo->hw_address,
		    sizeof(struct ether_addr)) != 0) {
			memcpy(&iface->hw_address, &imsg_ifinfo->hw_address,
			    sizeof(struct ether_addr));
			need_refresh = 1;
		}
		if (imsg_ifinfo->rdomain != iface->rdomain) {
			iface->rdomain = imsg_ifinfo->rdomain;
			need_refresh = 1;
		}
		if (imsg_ifinfo->running != iface->running) {
			iface->running = imsg_ifinfo->running;
			need_refresh = 1;
		}

		if (imsg_ifinfo->link_state != iface->link_state) {
			iface->link_state = imsg_ifinfo->link_state;
			need_refresh = 1;
		}
	}

	if (!need_refresh)
		return;

	if (iface->running && LINK_STATE_IS_UP(iface->link_state)) {
		if (iface->requested_ip.s_addr == INADDR_ANY)
			parse_lease(iface, imsg_ifinfo);

		if (iface->requested_ip.s_addr == INADDR_ANY)
			state_transition(iface, IF_INIT);
		else
			state_transition(iface, IF_REBOOTING);
	} else
		state_transition(iface, IF_DOWN);
}
struct dhcpleased_iface*
get_dhcpleased_iface_by_id(uint32_t if_index)
{
	struct dhcpleased_iface	*iface;
	LIST_FOREACH (iface, &dhcpleased_interfaces, entries) {
		if (iface->if_index == if_index)
			return (iface);
	}

	return (NULL);
}

void
remove_dhcpleased_iface(uint32_t if_index)
{
	struct dhcpleased_iface	*iface;

	iface = get_dhcpleased_iface_by_id(if_index);

	if (iface == NULL)
		return;

	send_rdns_withdraw(iface);
	send_deconfigure_interface(iface);
	LIST_REMOVE(iface, entries);
	evtimer_del(&iface->timer);
	free(iface);
}

void
parse_dhcp(struct dhcpleased_iface *iface, struct imsg_dhcp *dhcp)
{
	static uint8_t		 cookie[] = DHCP_COOKIE;
	static struct ether_addr bcast_mac;
	struct ether_header	*eh;
	struct ether_addr	 ether_src, ether_dst;
	struct ip		*ip;
	struct udphdr		*udp;
	struct dhcp_hdr		*dhcp_hdr;
	struct in_addr		 server_identifier, subnet_mask, router;
	struct in_addr		 nameservers[MAX_RDNS_COUNT];
	size_t			 rem, i;
	uint32_t		 sum, usum, lease_time = 0, renewal_time = 0;
	uint32_t		 rebinding_time = 0;
	uint8_t			*p, dho = DHO_PAD, dho_len;
	uint8_t			 dhcp_message_type = 0;
	char			 from[sizeof("xx:xx:xx:xx:xx:xx")];
	char			 to[sizeof("xx:xx:xx:xx:xx:xx")];
	char			 hbuf_src[INET_ADDRSTRLEN];
	char			 hbuf_dst[INET_ADDRSTRLEN];
	char			 hbuf[INET_ADDRSTRLEN];
	char			 vis_buf[4 * 255 + 1];
	char			 ifnamebuf[IF_NAMESIZE], *if_name;

	if (bcast_mac.ether_addr_octet[0] == 0)
		memset(bcast_mac.ether_addr_octet, 0xff, ETHER_ADDR_LEN);

	memset(hbuf_src, 0, sizeof(hbuf_src));
	memset(hbuf_dst, 0, sizeof(hbuf_dst));

	p = dhcp->packet;
	rem = dhcp->len;

	if (rem < sizeof(*eh)) {
		log_warnx("%s: message too short", __func__);
		return;
	}
	eh = (struct ether_header *)p;
	memcpy(ether_src.ether_addr_octet, eh->ether_shost,
	    sizeof(ether_src.ether_addr_octet));
	strlcpy(from, ether_ntoa(&ether_src), sizeof(from));
	memcpy(ether_dst.ether_addr_octet, eh->ether_dhost,
	    sizeof(ether_dst.ether_addr_octet));
	strlcpy(to, ether_ntoa(&ether_dst), sizeof(to));
	p += sizeof(*eh);
	rem -= sizeof(*eh);

	if (memcmp(&ether_dst, &iface->hw_address, sizeof(ether_dst)) != 0 &&
	    memcmp(&ether_dst, &bcast_mac, sizeof(ether_dst)) != 0)
		return ; /* silently ignore packet not for us */

	if (rem < sizeof(*ip))
		goto too_short;

	if (log_getverbose() > 1)
		log_debug("%s, from: %s, to: %s", __func__, from, to);

	ip = (struct ip *)p;

	if (rem < (size_t)ip->ip_hl << 2)
		goto too_short;

	if (wrapsum(checksum((uint8_t *)ip, ip->ip_hl << 2, 0)) != 0) {
		log_warnx("%s: bad IP checksum", __func__);
		return;
	}
	if (rem < ntohs(ip->ip_len))
		goto too_short;

	p += ip->ip_hl << 2;
	rem -= ip->ip_hl << 2;

	if (inet_ntop(AF_INET, &ip->ip_src, hbuf_src, sizeof(hbuf_src)) == NULL)
		hbuf_src[0] = '\0';
	if (inet_ntop(AF_INET, &ip->ip_dst, hbuf_dst, sizeof(hbuf_dst)) == NULL)
		hbuf_dst[0] = '\0';

	if (rem < sizeof(*udp))
		goto too_short;

	udp = (struct udphdr *)p;
	if (rem < ntohs(udp->uh_ulen))
		goto too_short;

	if (rem > ntohs(udp->uh_ulen)) {
		if (log_getverbose() > 1) {
			log_debug("%s: accepting packet with %lu bytes of data"
			    " after udp payload", __func__, rem -
			    ntohs(udp->uh_ulen));
		}
		rem = ntohs(udp->uh_ulen);
	}

	p += sizeof(*udp);
	rem -= sizeof(*udp);

	usum = udp->uh_sum;
	udp->uh_sum = 0;

	sum = wrapsum(checksum((uint8_t *)udp, sizeof(*udp), checksum(p, rem,
	    checksum((uint8_t *)&ip->ip_src, 2 * sizeof(ip->ip_src),
	    IPPROTO_UDP + ntohs(udp->uh_ulen)))));

	if (usum != 0 && usum != sum) {
		log_warnx("%s: bad UDP checksum", __func__);
		return;
	}

	if (log_getverbose() > 1) {
		log_debug("%s: %s:%d -> %s:%d", __func__, hbuf_src,
		    ntohs(udp->uh_sport), hbuf_dst, ntohs(udp->uh_dport));
	}

	if (ntohs(udp->uh_sport) != SERVER_PORT ||
	    ntohs(udp->uh_dport) != CLIENT_PORT) {
		log_warnx("%s: invalid ports used %s:%d -> %s:%d", __func__,
		    hbuf_src, ntohs(udp->uh_sport),
		    hbuf_dst, ntohs(udp->uh_dport));
		return;
	}
	if (rem < sizeof(*dhcp_hdr))
		goto too_short;

	dhcp_hdr = (struct dhcp_hdr *)p;
	p += sizeof(*dhcp_hdr);
	rem -= sizeof(*dhcp_hdr);

	dhcp_hdr->sname[DHCP_SNAME_LEN -1 ] = '\0'; /* ensure it's a string */
	dhcp_hdr->file[DHCP_FILE_LEN -1 ] = '\0'; /* ensure it's a string */

	if (log_getverbose() > 1)
		log_dhcp_hdr(dhcp_hdr);

	if (dhcp_hdr->op != DHCP_BOOTREPLY) {
		log_warnx("%s: ignorning non-reply packet", __func__);
		return;
	}

	if (dhcp_hdr->xid != iface->xid)
		return; /* silently ignore wrong xid */

	if (rem < sizeof(cookie))
		goto too_short;

	if (memcmp(p, cookie, sizeof(cookie)) != 0) {
		log_warnx("%s: no dhcp cookie in packet from %s", __func__,
		    from);
		return;
	}
	p += sizeof(cookie);
	rem -= sizeof(cookie);

	memset(&server_identifier, 0, sizeof(server_identifier));
	memset(&subnet_mask, 0, sizeof(subnet_mask));
	memset(&router, 0, sizeof(router));
	memset(&nameservers, 0, sizeof(nameservers));

	while (rem > 0 && dho != DHO_END) {
		dho = *p;
		p += 1;
		rem -= 1;
		/* only DHO_END and DHO_PAD are 1 byte long without length */
		if (dho == DHO_PAD || dho == DHO_END)
			dho_len = 0;
		else {
			if (rem == 0)
				goto too_short; /* missing option length */
			dho_len = *p;
			p += 1;
			rem -= 1;
			if (rem < dho_len)
				goto too_short;
		}

		switch(dho) {
		case DHO_PAD:
			if (log_getverbose() > 1)
				log_debug("DHO_PAD");
			break;
		case DHO_END:
			if (log_getverbose() > 1)
				log_debug("DHO_END");
			break;
		case DHO_DHCP_MESSAGE_TYPE:
			if (dho_len != 1)
				goto wrong_length;
			dhcp_message_type = *p;
			if (log_getverbose() > 1) {
				log_debug("DHO_DHCP_MESSAGE_TYPE: %s",
				    dhcp_message_type2str(dhcp_message_type));
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_DHCP_SERVER_IDENTIFIER:
			if (dho_len != sizeof(server_identifier))
				goto wrong_length;
			memcpy(&server_identifier, p,
			    sizeof(server_identifier));
			if (log_getverbose() > 1) {
				log_debug("DHO_DHCP_SERVER_IDENTIFIER: %s",
				    inet_ntop(AF_INET, &server_identifier,
				    hbuf, sizeof(hbuf)));
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_DHCP_LEASE_TIME:
			if (dho_len != sizeof(lease_time))
				goto wrong_length;
			memcpy(&lease_time, p, sizeof(lease_time));
			lease_time = ntohl(lease_time);
			if (log_getverbose() > 1) {
				log_debug("DHO_DHCP_LEASE_TIME %us",
				    lease_time);
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_SUBNET_MASK:
			if (dho_len != sizeof(subnet_mask))
				goto wrong_length;
			memcpy(&subnet_mask, p, sizeof(subnet_mask));
			if (log_getverbose() > 1) {
				log_debug("DHO_SUBNET_MASK: %s",
				    inet_ntop(AF_INET, &subnet_mask, hbuf,
				    sizeof(hbuf)));
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_ROUTERS:
			if (dho_len < sizeof(router))
				goto wrong_length;
			if (dho_len % sizeof(router) != 0)
				goto wrong_length;
			/* we only use one router */
			memcpy(&router, p, sizeof(router));
			if (log_getverbose() > 1) {
				log_debug("DHO_ROUTER: %s (1/%lu)",
				    inet_ntop(AF_INET, &router, hbuf,
				    sizeof(hbuf)), dho_len / sizeof(router));
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_DOMAIN_NAME_SERVERS:
			if (dho_len < sizeof(nameservers[0]))
				goto wrong_length;
			if (dho_len % sizeof(nameservers[0]) != 0)
				goto wrong_length;
			/* we limit ourself to 8 nameservers for proposals */
			memcpy(&nameservers, p, MINIMUM(sizeof(nameservers),
			    dho_len));
			if (log_getverbose() > 1) {
				for (i = 0; i < MINIMUM(sizeof(nameservers),
				    dho_len / sizeof(nameservers[0])); i++) {
					log_debug("DHO_DOMAIN_NAME_SERVERS: %s "
					    "(%lu/%lu)", inet_ntop(AF_INET,
					    &nameservers[i], hbuf,
					    sizeof(hbuf)), i + 1,
					    dho_len / sizeof(nameservers[0]));
				}
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_DOMAIN_NAME:
			if ( dho_len < 1)
				goto wrong_length;
			if (log_getverbose() > 1) {
				strvisx(vis_buf, p, dho_len, VIS_SAFE);
				log_debug("DHO_DOMAIN_NAME: %s", vis_buf);
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_DHCP_RENEWAL_TIME:
			if (dho_len != sizeof(renewal_time))
				goto wrong_length;
			memcpy(&renewal_time, p, sizeof(renewal_time));
			renewal_time = ntohl(renewal_time);
			if (log_getverbose() > 1) {
				log_debug("DHO_DHCP_RENEWAL_TIME %us",
				    renewal_time);
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_DHCP_REBINDING_TIME:
			if (dho_len != sizeof(rebinding_time))
				goto wrong_length;
			memcpy(&rebinding_time, p, sizeof(rebinding_time));
			rebinding_time = ntohl(rebinding_time);
			if (log_getverbose() > 1) {
				log_debug("DHO_DHCP_REBINDING_TIME %us",
				    rebinding_time);
			}
			p += dho_len;
			rem -= dho_len;
			break;
		case DHO_DHCP_CLIENT_IDENTIFIER:
			/* the server is supposed to echo this back to us */
			if (dho_len != 1 + sizeof(iface->hw_address))
				goto wrong_length;
			if (*p != HTYPE_ETHER) {
				log_warn("DHO_DHCP_CLIENT_IDENTIFIER: wrong "
				    "type");
				return;
			}
			if (memcmp(p + 1, &iface->hw_address,
			    sizeof(iface->hw_address)) != 0) {
				log_warn("wrong DHO_DHCP_CLIENT_IDENTIFIER");
				return;
			}
			p += dho_len;
			rem -= dho_len;
			break;
		default:
			if (log_getverbose() > 1)
				log_debug("DHO_%u, len: %u", dho, dho_len);
			p += dho_len;
			rem -= dho_len;
		}

	}
	while(rem != 0) {
		if (*p != DHO_PAD)
			break;
		p++;
		rem--;
	}
	if (rem != 0)
		log_warnx("%s: %lu bytes garbage data from %s", __func__, rem,
		    from);

	if_name = if_indextoname(iface->if_index, ifnamebuf);
	log_debug("%s on %s from %s/%s to %s/%s",
	    dhcp_message_type2str(dhcp_message_type), if_name == NULL ? "?" :
	    if_name, from, hbuf_src, to, hbuf_dst);

	switch (dhcp_message_type) {
	case DHCPOFFER:
		if (iface->state != IF_INIT) {
			log_debug("ignoring unexpected DHCPOFFER");
			return;
		}
		if (server_identifier.s_addr == INADDR_ANY &&
		    dhcp_hdr->yiaddr.s_addr == INADDR_ANY) {
			log_warnx("%s: did not receive server identifier or "
			    "offered IP address", __func__);
			return;
		}
		iface->server_identifier.s_addr = server_identifier.s_addr;
		iface->requested_ip.s_addr = dhcp_hdr->yiaddr.s_addr;
		state_transition(iface, IF_REQUESTING);
		break;
	case DHCPACK:
		switch (iface->state) {
		case IF_REQUESTING:
		case IF_RENEWING:
		case IF_REBINDING:
		case IF_REBOOTING:
			break;
		default:
			log_debug("ignoring unexpected DHCPACK");
			return;
		}
		if (server_identifier.s_addr == INADDR_ANY &&
		    dhcp_hdr->yiaddr.s_addr == INADDR_ANY) {
			log_warnx("%s: did not receive server identifier or "
			    "offered IP address", __func__);
			return;
		}
		if (lease_time == 0) {
			log_warnx("%s no lease time from %s", __func__, from);
			return;
		}
		if (subnet_mask.s_addr == INADDR_ANY) {
			log_warnx("%s: no subnetmask received from %s",
			    __func__, from);
			return;
		}

		/* RFC 2131 4.4.5 */
		if(renewal_time == 0)
			renewal_time = lease_time / 2;
		if (rebinding_time == 0)
			rebinding_time = lease_time - (lease_time / 8);

		if (renewal_time >= rebinding_time) {
			log_warnx("%s: renewal_time >= rebinding_time "
			    "(%u >= %u) from %s", __func__, renewal_time,
			    rebinding_time, from);
			return;
		}
		if (rebinding_time >= lease_time) {
			log_warnx("%s: rebinding_time >= lease_time"
			    "(%u >= %u) from %s", __func__, rebinding_time,
			    lease_time, from);
			return;
		}
		clock_gettime(CLOCK_MONOTONIC, &iface->request_time);
		iface->server_identifier.s_addr = server_identifier.s_addr;
		iface->requested_ip.s_addr = dhcp_hdr->yiaddr.s_addr;
		iface->mask.s_addr = subnet_mask.s_addr;
		iface->router.s_addr = router.s_addr;
		iface->lease_time = lease_time;
		iface->renewal_time = renewal_time;
		iface->rebinding_time = rebinding_time;
		memcpy(iface->nameservers, nameservers,
		    sizeof(iface->nameservers));
		state_transition(iface, IF_BOUND);
		break;
	case DHCPNAK:
		switch (iface->state) {
		case IF_REQUESTING:
		case IF_RENEWING:
		case IF_REBINDING:
		case IF_REBOOTING:
			break;
		default:
			log_debug("ignoring unexpected DHCPNAK");
			return;
		}

		state_transition(iface, IF_INIT);
		break;
	default:
		log_warnx("%s: unimplemented message type %d", __func__,
		    dhcp_message_type);
		break;
	}
	return;
 too_short:
	log_warnx("%s: message from %s too short", __func__, from);
	return;
 wrong_length:
	log_warnx("%s: received option %d with wrong length: %d", __func__,
	    dho, dho_len);
	return;
}

/* XXX check valid transitions */
void
state_transition(struct dhcpleased_iface *iface, enum if_state new_state)
{
	enum if_state	 old_state = iface->state;
	struct timespec	 now, res;
	char		 ifnamebuf[IF_NAMESIZE], *if_name;

	iface->state = new_state;
	switch (new_state) {
	case IF_DOWN:
		if (iface->requested_ip.s_addr == INADDR_ANY) {
			/* nothing to do until iface comes up */
			iface->timo.tv_sec = -1;
			break;
		}
		if (old_state == IF_DOWN) {
			/* nameservers already withdrawn when if went down */
			send_deconfigure_interface(iface);
			/* nothing more to do until iface comes back */
			iface->timo.tv_sec = -1;
		} else {
			send_rdns_withdraw(iface);
			clock_gettime(CLOCK_MONOTONIC, &now);
			timespecsub(&now, &iface->request_time, &res);
			iface->timo.tv_sec = iface->lease_time - res.tv_sec;
			if (iface->timo.tv_sec < 0)
				iface->timo.tv_sec = 0; /* deconfigure now */
		}
		break;
	case IF_INIT:
		switch (old_state) {
		case IF_INIT:
			if (iface->timo.tv_sec < MAX_EXP_BACKOFF_SLOW)
				iface->timo.tv_sec *= 2;
			break;
		case IF_REQUESTING:
		case IF_RENEWING:
		case IF_REBINDING:
		case IF_REBOOTING:
			/* lease expired, got DHCPNAK or timeout: delete IP */
			send_rdns_withdraw(iface);
			send_deconfigure_interface(iface);
			/* fall through */
		case IF_DOWN:
			iface->timo.tv_sec = START_EXP_BACKOFF;
			break;
		case IF_BOUND:
			fatal("invalid transition Bound -> Init");
			break;
		}
		request_dhcp_discover(iface);
		break;
	case IF_REBOOTING:
		if (old_state == IF_REBOOTING)
			iface->timo.tv_sec *= 2;
		else {
			/* make sure we send broadcast */
			iface->dhcp_server.s_addr = INADDR_ANY;
			iface->timo.tv_sec = START_EXP_BACKOFF;
		}
		request_dhcp_request(iface);
		break;
	case IF_REQUESTING:
		if (old_state == IF_REQUESTING)
			iface->timo.tv_sec *= 2;
		else
			iface->timo.tv_sec = START_EXP_BACKOFF;
		request_dhcp_request(iface);
		break;
	case IF_BOUND:
		iface->timo.tv_sec = iface->renewal_time;
		if (old_state == IF_REQUESTING || old_state == IF_REBOOTING) {
			send_configure_interface(iface);
			send_rdns_proposal(iface);
		}
		break;
	case IF_RENEWING:
		if (old_state == IF_BOUND) {
			iface->dhcp_server.s_addr =
			    iface->server_identifier.s_addr;
			iface->server_identifier.s_addr = INADDR_ANY;

			iface->timo.tv_sec = (iface->rebinding_time -
			    iface->renewal_time) / 2; /* RFC 2131 4.4.5 */
		} else
			iface->timo.tv_sec /= 2;

		if (iface->timo.tv_sec < 60)
			iface->timo.tv_sec = 60;
		request_dhcp_request(iface);
		break;
	case IF_REBINDING:
		if (old_state == IF_RENEWING) {
			iface->dhcp_server.s_addr = INADDR_ANY;
			iface->timo.tv_sec = (iface->lease_time -
			    iface->renewal_time) / 2; /* RFC 2131 4.4.5 */
		} else
			iface->timo.tv_sec /= 2;
		request_dhcp_request(iface);
		break;
	}

	if_name = if_indextoname(iface->if_index, ifnamebuf);
	log_debug("%s[%s] %s -> %s, timo: %lld", __func__, if_name == NULL ?
	    "?" : if_name, if_state_name[old_state], if_state_name[new_state],
	    iface->timo.tv_sec);

	if (iface->timo.tv_sec == -1) {
		if (evtimer_pending(&iface->timer, NULL))
			evtimer_del(&iface->timer);
	} else
		evtimer_add(&iface->timer, &iface->timo);
}

void
iface_timeout(int fd, short events, void *arg)
{
	struct dhcpleased_iface	*iface = (struct dhcpleased_iface *)arg;
	struct timespec		 now, res;

	log_debug("%s[%d]: %s", __func__, iface->if_index,
	    if_state_name[iface->state]);

	switch (iface->state) {
	case IF_DOWN:
		state_transition(iface, IF_DOWN);
		break;
	case IF_INIT:
		state_transition(iface, IF_INIT);
		break;
	case IF_REBOOTING:
		if (iface->timo.tv_sec >= MAX_EXP_BACKOFF_FAST)
			state_transition(iface, IF_INIT);
		else
			state_transition(iface, IF_REBOOTING);
		break;
	case IF_REQUESTING:
		if (iface->timo.tv_sec >= MAX_EXP_BACKOFF_FAST)
			state_transition(iface, IF_INIT);
		else
			state_transition(iface, IF_REQUESTING);
		break;
	case IF_BOUND:
		state_transition(iface, IF_RENEWING);
		break;
	case IF_RENEWING:
		clock_gettime(CLOCK_MONOTONIC, &now);
		timespecsub(&now, &iface->request_time, &res);
		log_debug("%s: res.tv_sec: %lld, rebinding_time: %u", __func__,
		    res.tv_sec, iface->rebinding_time);
		if (res.tv_sec > iface->rebinding_time)
			state_transition(iface, IF_REBINDING);
		else
			state_transition(iface, IF_RENEWING);
		break;
	case IF_REBINDING:
		clock_gettime(CLOCK_MONOTONIC, &now);
		timespecsub(&now, &iface->request_time, &res);
		log_debug("%s: res.tv_sec: %lld, lease_time: %u", __func__,
		    res.tv_sec, iface->lease_time);
		if (res.tv_sec > iface->lease_time)
			state_transition(iface, IF_INIT);
		else
			state_transition(iface, IF_REBINDING);
		break;
	}
}

void
request_dhcp_discover(struct dhcpleased_iface *iface)
{
	struct imsg_req_discover	 imsg_req_discover;

	imsg_req_discover.if_index = iface->if_index;
	imsg_req_discover.xid = iface->xid = arc4random();
	engine_imsg_compose_frontend(IMSG_SEND_DISCOVER, 0, &imsg_req_discover,
	    sizeof(imsg_req_discover));
}

void
request_dhcp_request(struct dhcpleased_iface *iface)
{
	struct imsg_req_request	 imsg_req_request;

	iface->xid = arc4random();
	imsg_req_request.if_index = iface->if_index;
	imsg_req_request.xid = iface->xid;
	imsg_req_request.server_identifier.s_addr =
	    iface->server_identifier.s_addr;
	imsg_req_request.requested_ip.s_addr = iface->requested_ip.s_addr;
	imsg_req_request.dhcp_server.s_addr =  iface->dhcp_server.s_addr;
	engine_imsg_compose_frontend(IMSG_SEND_REQUEST, 0, &imsg_req_request,
	    sizeof(imsg_req_request));
}

void
log_lease(struct dhcpleased_iface *iface, int deconfigure)
{
	char	 hbuf_lease[INET_ADDRSTRLEN], hbuf_server[INET_ADDRSTRLEN];
	char	 ifnamebuf[IF_NAMESIZE], *if_name;

	if_name = if_indextoname(iface->if_index, ifnamebuf);
	inet_ntop(AF_INET, &iface->requested_ip, hbuf_lease,
	    sizeof(hbuf_lease));
	inet_ntop(AF_INET, &iface->server_identifier, hbuf_server,
	    sizeof(hbuf_server));


	if (deconfigure)
		log_info("deleting %s from %s (lease from %s)", hbuf_lease,
		    if_name == NULL ? "?" : if_name, hbuf_server);
	else
		log_info("adding %s to %s (lease from %s)", hbuf_lease,
		    if_name == NULL ? "?" : if_name, hbuf_server);
}

void
send_configure_interface(struct dhcpleased_iface *iface)
{
	struct imsg_configure_interface	 imsg;

	log_lease(iface, 0);

	imsg.if_index = iface->if_index;
	imsg.rdomain = iface->rdomain;
	imsg.addr.s_addr = iface->requested_ip.s_addr;
	imsg.mask.s_addr = iface->mask.s_addr;
	imsg.router.s_addr = iface->router.s_addr;
	engine_imsg_compose_main(IMSG_CONFIGURE_INTERFACE, 0, &imsg,
	    sizeof(imsg));
}

void
send_deconfigure_interface(struct dhcpleased_iface *iface)
{
	struct imsg_configure_interface	 imsg;

	if (iface->requested_ip.s_addr == INADDR_ANY)
		return;

	log_lease(iface, 1);

	imsg.if_index = iface->if_index;
	imsg.rdomain = iface->rdomain;
	imsg.addr.s_addr = iface->requested_ip.s_addr;
	imsg.mask.s_addr = iface->mask.s_addr;
	imsg.router.s_addr = iface->router.s_addr;
	engine_imsg_compose_main(IMSG_DECONFIGURE_INTERFACE, 0, &imsg,
	    sizeof(imsg));

	iface->server_identifier.s_addr = INADDR_ANY;
	iface->dhcp_server.s_addr = INADDR_ANY;
	iface->requested_ip.s_addr = INADDR_ANY;
	iface->mask.s_addr = INADDR_ANY;
	iface->router.s_addr = INADDR_ANY;
}

void
log_rdns(struct dhcpleased_iface *iface, int withdraw)
{
	int	 i;
	char	 hbuf_rdns[INET_ADDRSTRLEN], hbuf_server[INET_ADDRSTRLEN];
	char	 ifnamebuf[IF_NAMESIZE], *if_name, *rdns_buf = NULL, *tmp_buf;

	if_name = if_indextoname(iface->if_index, ifnamebuf);

	inet_ntop(AF_INET, &iface->server_identifier, hbuf_server,
	    sizeof(hbuf_server));

	for (i = 0; i < MAX_RDNS_COUNT && iface->nameservers[i].s_addr !=
		 INADDR_ANY; i++) {
		inet_ntop(AF_INET, &iface->nameservers[i], hbuf_rdns,
		    sizeof(hbuf_rdns));
		tmp_buf = rdns_buf;
		if (asprintf(&rdns_buf, "%s %s", tmp_buf ? tmp_buf : "",
		    hbuf_rdns) < 0) {
			rdns_buf = NULL;
			break;
		}
		free(tmp_buf);
	}

	if (rdns_buf != NULL) {
		if (withdraw) {
			log_info("deleting nameservers%s (lease from %s on %s)",
			    rdns_buf, hbuf_server, if_name == NULL ? "?" :
			    if_name);
		} else {
			log_info("adding nameservers%s (lease from %s on %s)",
			    rdns_buf, hbuf_server, if_name == NULL ? "?" :
			    if_name);
		}
		free(rdns_buf);
	}
}

void
send_rdns_proposal(struct dhcpleased_iface *iface)
{
	struct imsg_propose_rdns	 imsg;

	log_rdns(iface, 0);

	memset(&imsg, 0, sizeof(imsg));

	imsg.if_index = iface->if_index;
	imsg.rdomain = iface->rdomain;
	for (imsg.rdns_count = 0; imsg.rdns_count < MAX_RDNS_COUNT &&
		 iface->nameservers[imsg.rdns_count].s_addr != INADDR_ANY;
	     imsg.rdns_count++)
		;
	memcpy(imsg.rdns, iface->nameservers, sizeof(imsg.rdns));
	engine_imsg_compose_main(IMSG_PROPOSE_RDNS, 0, &imsg, sizeof(imsg));
}

void
send_rdns_withdraw(struct dhcpleased_iface *iface)
{
	struct imsg_propose_rdns	 imsg;

	log_rdns(iface, 1);

	memset(&imsg, 0, sizeof(imsg));

	imsg.if_index = iface->if_index;
	imsg.rdomain = iface->rdomain;
	engine_imsg_compose_main(IMSG_WITHDRAW_RDNS, 0, &imsg, sizeof(imsg));
	memset(iface->nameservers, 0, sizeof(iface->nameservers));
}

void
parse_lease(struct dhcpleased_iface *iface, struct imsg_ifinfo *imsg_ifinfo)
{
	char	*p, *p1;

	/* make sure this is a string */
	imsg_ifinfo->lease[sizeof(imsg_ifinfo->lease) - 1] = '\0';

	iface->requested_ip.s_addr = INADDR_ANY;

	if ((p = strstr(imsg_ifinfo->lease, LEASE_PREFIX)) == NULL)
		return;

	p += sizeof(LEASE_PREFIX) - 1;
	if ((p1 = strchr(p, '\n')) == NULL)
		return;
	*p1 = '\0';

	if (inet_pton(AF_INET, p, &iface->requested_ip) != 1)
		iface->requested_ip.s_addr = INADDR_ANY;
}

void
log_dhcp_hdr(struct dhcp_hdr *dhcp_hdr)
{
	char	 hbuf[INET_ADDRSTRLEN];

	log_debug("dhcp_hdr op: %s (%d)", dhcp_hdr->op == DHCP_BOOTREQUEST ?
	    "Boot Request" : dhcp_hdr->op == DHCP_BOOTREPLY ? "Boot Reply" :
	    "Unknown", dhcp_hdr->op);
	log_debug("dhcp_hdr htype: %s (%d)", dhcp_hdr->htype == 1 ? "Ethernet":
	    "Unknown", dhcp_hdr->htype);
	log_debug("dhcp_hdr hlen: %d", dhcp_hdr->hlen);
	log_debug("dhcp_hdr hops: %d", dhcp_hdr->hops);
	log_debug("dhcp_hdr xid: 0x%x", dhcp_hdr->xid);
	log_debug("dhcp_hdr secs: %u", dhcp_hdr->secs);
	log_debug("dhcp_hdr flags: 0x%x", dhcp_hdr->flags);
	log_debug("dhcp_hdr ciaddr: %s", inet_ntop(AF_INET, &dhcp_hdr->ciaddr,
	    hbuf, sizeof(hbuf)));
	log_debug("dhcp_hdr yiaddr: %s", inet_ntop(AF_INET, &dhcp_hdr->yiaddr,
	    hbuf, sizeof(hbuf)));
	log_debug("dhcp_hdr siaddr: %s", inet_ntop(AF_INET, &dhcp_hdr->siaddr,
	    hbuf, sizeof(hbuf)));
	log_debug("dhcp_hdr giaddr: %s", inet_ntop(AF_INET, &dhcp_hdr->giaddr,
	    hbuf, sizeof(hbuf)));
	log_debug("dhcp_hdr chaddr: %02x:%02x:%02x:%02x:%02x:%02x "
	    "(%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x)",
	    dhcp_hdr->chaddr[0], dhcp_hdr->chaddr[1], dhcp_hdr->chaddr[2],
	    dhcp_hdr->chaddr[3], dhcp_hdr->chaddr[4], dhcp_hdr->chaddr[5],
	    dhcp_hdr->chaddr[6], dhcp_hdr->chaddr[7], dhcp_hdr->chaddr[8],
	    dhcp_hdr->chaddr[9], dhcp_hdr->chaddr[10], dhcp_hdr->chaddr[11],
	    dhcp_hdr->chaddr[12], dhcp_hdr->chaddr[13], dhcp_hdr->chaddr[14],
	    dhcp_hdr->chaddr[15]);
	/* ignore sname and file, if we ever print it use strvis(3) */
}

const char *
dhcp_message_type2str(uint8_t dhcp_message_type)
{
	switch (dhcp_message_type) {
	case DHCPDISCOVER:
		return "DHCPDISCOVER";
	case DHCPOFFER:
		return "DHCPOFFER";
	case DHCPREQUEST:
		return "DHCPREQUEST";
	case DHCPDECLINE:
		return "DHCPDECLINE";
	case DHCPACK:
		return "DHCPACK";
	case DHCPNAK:
		return "DHCPNAK";
	case DHCPRELEASE:
		return "DHCPRELEASE";
	case DHCPINFORM:
		return "DHCPINFORM";
	default:
		return "Unknown";
	}
}
