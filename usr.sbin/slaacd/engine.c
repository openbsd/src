/*	$OpenBSD: engine.c,v 1.1 2017/03/18 17:33:13 florian Exp $	*/

/*
 * Copyright (c) 2017 Florian Obser <florian@openbsd.org>
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

/*
 * Copyright (C) 1995, 1996, 1997, and 1998 WIDE Project.
 * All rights reserved.
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the project nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE PROJECT AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE PROJECT OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/queue.h>
#include <sys/socket.h>
#include <sys/syslog.h>
#include <sys/uio.h>

#include <net/if.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet/icmp6.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <netdb.h>
#include <pwd.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "log.h"
#include "slaacd.h"
#include "engine.h"

#define	MAX_RTR_SOLICITATION_DELAY	1
#define	MAX_RTR_SOLICITATION_DELAY_USEC	MAX_RTR_SOLICITATION_DELAY * 1000000
#define	RTR_SOLICITATION_INTERVAL	4
#define	MAX_RTR_SOLICITATIONS		3

enum if_state {
	IF_DOWN,
	IF_DELAY,
	IF_PROBE,
	IF_IDLE,
};

const char* if_state_name[] = {
	"IF_DOWN",
	"IF_DELAY",
	"IF_PROBE",
	"IF_IDLE",
};

struct radv_prefix {
	LIST_ENTRY(radv_prefix)	entries;
	struct in6_addr		prefix;
	uint8_t			prefix_len;
	int			onlink;
	int			autonomous;
	uint32_t		vltime;
	uint32_t		pltime;
};

struct radv_rdns {
	LIST_ENTRY(radv_rdns)	entries;
	struct in6_addr		rdns;
};

struct radv_dnssl {
	LIST_ENTRY(radv_dnssl)	entries;
	char			dnssl[SLAACD_MAX_DNSSL];
};

struct radv {
	LIST_ENTRY(radv)		 entries;
	struct sockaddr_in6		 from;
	struct timespec			 when;
	struct timespec			 uptime;
	struct event			 timer;
	uint32_t			 min_lifetime;
	uint8_t				 curhoplimit;
	int				 managed;
	int				 other;
	enum rpref			 rpref;
	uint16_t			 router_lifetime; /* in seconds */
	uint32_t			 reachable_time; /* in milliseconds */
	uint32_t			 retrans_time; /* in milliseconds */
	LIST_HEAD(, radv_prefix)	 prefixes;
	uint32_t			 rdns_lifetime;
	LIST_HEAD(, radv_rdns)		 rdns_servers;
	uint32_t			 dnssl_lifetime;
	LIST_HEAD(, radv_dnssl)		 dnssls;
};

struct slaacd_iface {
	LIST_ENTRY(slaacd_iface)	 entries;
	enum if_state			 state;
	struct event			 timer;
	int				 probes;
	uint32_t			 if_index;
	int				 running;
	int				 autoconfprivacy;
	struct ether_addr		 hw_address;
	LIST_HEAD(, radv)		 radvs;
};

LIST_HEAD(, slaacd_iface) slaacd_interfaces;

__dead void		 engine_shutdown(void);
void			 engine_sig_handler(int sig, short, void *);
void			 engine_dispatch_frontend(int, short, void *);
void			 engine_dispatch_main(int, short, void *);
void			 send_interface_info(struct slaacd_iface *, pid_t);
void			 engine_showinfo_ctl(struct imsg *, uint32_t);
struct slaacd_iface	*get_slaacd_iface_by_id(uint32_t);
void			 remove_slaacd_iface(uint32_t);
void			 free_ra(struct radv *);
void			 parse_ra(struct slaacd_iface *, struct imsg_ra *);
void			 debug_log_ra(struct imsg_ra *);
char			*parse_dnssl(char *, int);
void		 	 update_iface_ra(struct slaacd_iface *, struct radv *);
void			 start_probe(struct slaacd_iface *);
void			 ra_timeout(int, short, void *);
void			 iface_timeout(int, short, void *);
struct radv		*find_ra(struct slaacd_iface *, struct sockaddr_in6 *);

struct imsgev		*iev_frontend;
struct imsgev		*iev_main;

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

	if ((pw = getpwnam(SLAACD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	slaacd_process = PROC_ENGINE;
	setproctitle(log_procnames[slaacd_process]);
	log_procinit(log_procnames[slaacd_process]);

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

	LIST_INIT(&slaacd_interfaces);

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

void
engine_dispatch_frontend(int fd, short event, void *bula)
{
	struct imsgev		*iev = bula;
	struct imsgbuf		*ibuf = &iev->ibuf;
	struct imsg		 imsg;
	struct slaacd_iface	*iface;
	struct imsg_ra		 ra;
	struct imsg_ifinfo	 imsg_ifinfo;
	ssize_t			 n;
	int			 shut = 0, verbose;
	uint32_t		 if_index;

	DEBUG_IMSG("%s", __func__);

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
		DEBUG_IMSG("%s: EV_READ, n=%ld", __func__, n);
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
		DEBUG_IMSG("%s: EV_WRITE, n=%ld", __func__, n);
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		DEBUG_IMSG("%s: %s", __func__, imsg_type_name[imsg.hdr.type]);

		switch (imsg.hdr.type) {
		case IMSG_CTL_LOG_VERBOSE:
			/* Already checked by frontend. */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		case IMSG_CTL_SHOW_INTERFACE_INFO:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(if_index))
				fatal("%s: IMSG_CTL_SEND_SOLICITATION wrong "
				    "length: %d", __func__, imsg.hdr.len);
			memcpy(&if_index, imsg.data, sizeof(if_index));
			engine_showinfo_ctl(&imsg, if_index);
			break;
		case IMSG_UPDATE_IF:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(imsg_ifinfo))
				fatal("%s: IMSG_UPDATE_IF wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&imsg_ifinfo, imsg.data, sizeof(imsg_ifinfo));
			DEBUG_IMSG("%s: IMSG_UPDATE_IF: %d[%s], running: %s, "
			    "privacy: %s", __func__, imsg_ifinfo.if_index,
			    ether_ntoa(&imsg_ifinfo.hw_address),
			    imsg_ifinfo.running ? "yes" : "no",
			    imsg_ifinfo.autoconfprivacy ? "yes" : "no" );

			iface = get_slaacd_iface_by_id(imsg_ifinfo.if_index);
			if (iface == NULL) {
				DEBUG_IMSG("%s: new interface: %d", __func__,
				    imsg_ifinfo.if_index);
				if ((iface = calloc(1, sizeof(*iface))) == NULL)
					fatal("calloc");
				evtimer_set(&iface->timer, iface_timeout,
				    iface);
				iface->if_index = imsg_ifinfo.if_index;
				iface->running = imsg_ifinfo.running;
				if (iface->running)
					start_probe(iface);
				else
					iface->state = IF_DOWN;
				iface->autoconfprivacy =
				    imsg_ifinfo.autoconfprivacy;
				memcpy(&iface->hw_address,
				    &imsg_ifinfo.hw_address,
				    sizeof(struct ether_addr));
				LIST_INIT(&iface->radvs);
				LIST_INSERT_HEAD(&slaacd_interfaces,
				    iface, entries);
			} else {
				DEBUG_IMSG("%s: updating %d", __func__,
				    imsg_ifinfo.if_index);
				if (!iface->state == IF_DOWN &&
				    imsg_ifinfo.running)
					start_probe(iface);
				iface->running = imsg_ifinfo.running;
				if (!iface->running) {
					iface->state = IF_DOWN;
					if (evtimer_pending(&iface->timer,
					    NULL))
						evtimer_del(&iface->timer);
				}
				iface->autoconfprivacy =
				    imsg_ifinfo.autoconfprivacy;
				memcpy(&iface->hw_address,
				    &imsg_ifinfo.hw_address,
				    sizeof(struct ether_addr));
			}
			break;
		case IMSG_REMOVE_IF:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(if_index))
				fatal("%s: IMSG_REMOVE_IF wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&if_index, imsg.data, sizeof(if_index));
			DEBUG_IMSG("%s: IMSG_REMOVE_IF: %d", __func__,
			    if_index);
			remove_slaacd_iface(if_index);
			break;
		case IMSG_RA:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(ra))
				fatal("%s: IMSG_RA wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&ra, imsg.data, sizeof(ra));
			DEBUG_IMSG("%s: IMSG_RA: %d", __func__, ra->if_index);
			iface = get_slaacd_iface_by_id(ra.if_index);
			if (iface != NULL)
				parse_ra(iface, &ra);
			break;
		case IMSG_CTL_SEND_SOLICITATION:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(if_index))
				fatal("%s: IMSG_CTL_SEND_SOLICITATION wrong "
				    "length: %d", __func__, imsg.hdr.len);
			memcpy(&if_index, imsg.data, sizeof(if_index));
			iface = get_slaacd_iface_by_id(if_index);
			if (iface == NULL)
				log_warn("requested to send solicitation on "
				    "non-autoconf interface: %u", if_index);
			else
				engine_imsg_compose_frontend(
				    IMSG_CTL_SEND_SOLICITATION, imsg.hdr.pid,
				    &iface->if_index, sizeof(iface->if_index));
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
	ssize_t			 n;
	int			 shut = 0;

	DEBUG_IMSG("%s", __func__);

	if (event & EV_READ) {
		if ((n = imsg_read(ibuf)) == -1 && errno != EAGAIN)
			fatal("imsg_read error");
		if (n == 0)	/* Connection closed. */
			shut = 1;
		DEBUG_IMSG("%s: EV_READ, n=%ld", __func__, n);
	}
	if (event & EV_WRITE) {
		if ((n = msgbuf_write(&ibuf->w)) == -1 && errno != EAGAIN)
			fatal("msgbuf_write");
		if (n == 0)	/* Connection closed. */
			shut = 1;
		DEBUG_IMSG("%s: EV_WRITE, n=%ld", __func__, n);
	}

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("%s: imsg_get error", __func__);
		if (n == 0)	/* No more messages. */
			break;

		DEBUG_IMSG("%s: %s", __func__, imsg_type_name[imsg.hdr.type]);
		switch (imsg.hdr.type) {
		case IMSG_SOCKET_IPC:
			/*
			 * Setup pipe and event handler to the frontend
			 * process.
			 */
			if (iev_frontend) {
				log_warnx("%s: received unexpected imsg fd "
				    "to engine", __func__);
				break;
			}
			if ((fd = imsg.fd) == -1) {
				log_warnx("%s: expected to receive imsg fd to "
				   "engine but didn't receive any", __func__);
				break;
			}

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
send_interface_info(struct slaacd_iface *iface, pid_t pid)
{
	struct ctl_engine_info			 cei;
	struct ctl_engine_info_ra		 cei_ra;
	struct ctl_engine_info_ra_prefix	 cei_ra_prefix;
	struct ctl_engine_info_ra_rdns		 cei_ra_rdns;
	struct ctl_engine_info_ra_dnssl		 cei_ra_dnssl;
	struct radv				*ra;
	struct radv_prefix			*prefix;
	struct radv_rdns			*rdns;
	struct radv_dnssl			*dnssl;

	memset(&cei, 0, sizeof(cei));
	cei.if_index = iface->if_index;
	cei.running = iface->running;
	cei.autoconfprivacy = iface->autoconfprivacy;
	memcpy(&cei.hw_address, &iface->hw_address, sizeof(struct ether_addr));
	engine_imsg_compose_frontend(IMSG_CTL_SHOW_INTERFACE_INFO, pid, &cei,
	    sizeof(cei));
	LIST_FOREACH(ra, &iface->radvs, entries) {
		memset(&cei_ra, 0, sizeof(cei_ra));
		memcpy(&cei_ra.from, &ra->from, sizeof(cei_ra.from));
		memcpy(&cei_ra.when, &ra->when, sizeof(cei_ra.when));
		memcpy(&cei_ra.uptime, &ra->uptime, sizeof(cei_ra.uptime));
		cei_ra.curhoplimit = ra->curhoplimit;
		cei_ra.managed = ra->managed;
		cei_ra.other = ra->other;
		cei_ra.rpref = ra->rpref;
		cei_ra.router_lifetime = ra->router_lifetime;
		cei_ra.reachable_time = ra->reachable_time;
		cei_ra.retrans_time = ra->retrans_time;
		engine_imsg_compose_frontend(IMSG_CTL_SHOW_INTERFACE_INFO_RA,
		     pid, &cei_ra, sizeof(cei_ra));

		LIST_FOREACH(prefix, &ra->prefixes, entries) {
			memset(&cei_ra_prefix, 0, sizeof(cei_ra_prefix));

			cei_ra_prefix.prefix = prefix->prefix;
			cei_ra_prefix.prefix_len = prefix->prefix_len;
			cei_ra_prefix.onlink = prefix->onlink;
			cei_ra_prefix.autonomous = prefix->autonomous;
			cei_ra_prefix.vltime = prefix->vltime;
			cei_ra_prefix.pltime = prefix->pltime;
			engine_imsg_compose_frontend(
			    IMSG_CTL_SHOW_INTERFACE_INFO_RA_PREFIX, pid,
			    &cei_ra_prefix, sizeof(cei_ra_prefix));
		}

		LIST_FOREACH(rdns, &ra->rdns_servers, entries) {
			memset(&cei_ra_rdns, 0, sizeof(cei_ra_rdns));
			memcpy(&cei_ra_rdns.rdns, &rdns->rdns,
			    sizeof(cei_ra_rdns.rdns));
			cei_ra_rdns.lifetime = ra->rdns_lifetime;
			engine_imsg_compose_frontend(
			    IMSG_CTL_SHOW_INTERFACE_INFO_RA_RDNS, pid,
			    &cei_ra_rdns, sizeof(cei_ra_rdns));
		}

		LIST_FOREACH(dnssl, &ra->dnssls, entries) {
			memset(&cei_ra_dnssl, 0, sizeof(cei_ra_dnssl));
			memcpy(&cei_ra_dnssl.dnssl, &dnssl->dnssl,
			    sizeof(cei_ra_dnssl.dnssl));
			cei_ra_dnssl.lifetime = ra->dnssl_lifetime;
			engine_imsg_compose_frontend(
			    IMSG_CTL_SHOW_INTERFACE_INFO_RA_DNSSL, pid,
			    &cei_ra_dnssl, sizeof(cei_ra_dnssl));
		}
	}
}

void
engine_showinfo_ctl(struct imsg *imsg, uint32_t if_index)
{
	struct slaacd_iface			*iface;

	switch (imsg->hdr.type) {
	case IMSG_CTL_SHOW_INTERFACE_INFO:
		if (if_index == 0) {
			LIST_FOREACH (iface, &slaacd_interfaces, entries)
				send_interface_info(iface, imsg->hdr.pid);
		} else {
			if ((iface = get_slaacd_iface_by_id(if_index)) != NULL)
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

struct slaacd_iface*
get_slaacd_iface_by_id(uint32_t if_index)
{
	struct slaacd_iface	*iface;
	LIST_FOREACH (iface, &slaacd_interfaces, entries) {
		if (iface->if_index == if_index)
			return (iface);
	}

	return (NULL);
}

void
remove_slaacd_iface(uint32_t if_index)
{
	struct slaacd_iface	*iface, *tiface;
	struct radv		*ra;

	LIST_FOREACH_SAFE (iface, &slaacd_interfaces, entries, tiface) {
		if (iface->if_index == if_index) {
			LIST_REMOVE(iface, entries);
			while(!LIST_EMPTY(&iface->radvs)) {
				ra = LIST_FIRST(&iface->radvs);
				LIST_REMOVE(ra, entries);
				free_ra(ra);
			}
			free(iface);
			break;
		}
	}
}

void
free_ra(struct radv *ra)
{
	struct radv_prefix	*prefix;
	struct radv_rdns	*rdns;
	struct radv_dnssl	*dnssl;

	if (ra == NULL)
		return;

	evtimer_del(&ra->timer);

	while (!LIST_EMPTY(&ra->prefixes)) {
		prefix = LIST_FIRST(&ra->prefixes);
		LIST_REMOVE(prefix, entries);
		free(prefix);
	}

	while (!LIST_EMPTY(&ra->rdns_servers)) {
		rdns = LIST_FIRST(&ra->rdns_servers);
		LIST_REMOVE(rdns, entries);
		free(rdns);
	}

	while (!LIST_EMPTY(&ra->dnssls)) {
		dnssl = LIST_FIRST(&ra->dnssls);
		LIST_REMOVE(dnssl, entries);
		free(dnssl);
	}

	free(ra);
}

void
parse_ra(struct slaacd_iface *iface, struct imsg_ra *ra)
{
	struct nd_router_advert	*nd_ra;
	struct radv		*radv;
	struct radv_prefix	*prefix;
	struct radv_rdns	*rdns;
	struct radv_dnssl	*ra_dnssl;
	ssize_t			 len = ra->len;
	char			 hbuf[NI_MAXHOST];
	uint8_t			*p;

#if 0
	debug_log_ra(ra);
#endif

	if (getnameinfo((struct sockaddr *)&ra->from, ra->from.sin6_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV)) {
		log_warn("cannot get router IP");
		strlcpy(hbuf, "uknown", sizeof(hbuf));
	}

	if (!IN6_IS_ADDR_LINKLOCAL(&ra->from.sin6_addr)) {
		log_warn("RA from non link local address %s", hbuf);
		return;
	}

	if ((size_t)len < sizeof(struct nd_router_advert)) {
		log_warn("received to short message (%ld) from %s", len, hbuf);
		return;
	}

	if ((radv = calloc(1, sizeof(*radv))) == NULL)
		fatal("calloc");

	LIST_INIT(&radv->prefixes);
	LIST_INIT(&radv->rdns_servers);
	LIST_INIT(&radv->dnssls);

	radv->min_lifetime = UINT32_MAX;

	p = ra->packet;
	nd_ra = (struct nd_router_advert *)p;
	len -= sizeof(struct nd_router_advert);
	p += sizeof(struct nd_router_advert);

	log_debug("ICMPv6 type(%d), code(%d) from %s of length %ld",
	    nd_ra->nd_ra_type, nd_ra->nd_ra_code, hbuf, len);

	if (nd_ra->nd_ra_type != ND_ROUTER_ADVERT) {
		log_warnx("invalid ICMPv6 type (%d) from %s", nd_ra->nd_ra_type,
		    hbuf);
		goto err;
	}

	if (nd_ra->nd_ra_code != 0) {
		log_warn("invalid ICMPv6 code (%d) from %s", nd_ra->nd_ra_code,
		    hbuf);
		goto err;
	}

	memcpy(&radv->from, &ra->from, sizeof(ra->from));

	if (clock_gettime(CLOCK_REALTIME, &radv->when))
		fatal("clock_gettime");
	if (clock_gettime(CLOCK_MONOTONIC, &radv->uptime))
		fatal("clock_gettime");

	radv->curhoplimit = nd_ra->nd_ra_curhoplimit;
	radv->managed = nd_ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED;
	radv->other = nd_ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER;

	switch (nd_ra->nd_ra_flags_reserved & ND_RA_FLAG_RTPREF_MASK) {
	case ND_RA_FLAG_RTPREF_HIGH:
		radv->rpref=HIGH;
		break;
	case ND_RA_FLAG_RTPREF_LOW:
		radv->rpref=LOW;
		break;
	case ND_RA_FLAG_RTPREF_MEDIUM:
		/* fallthrough */
	default:
		radv->rpref=MEDIUM;
		break;
	}
	radv->router_lifetime = ntohs(nd_ra->nd_ra_router_lifetime);
	if (radv->router_lifetime != 0)
		radv->min_lifetime = radv->router_lifetime;
	radv->reachable_time = ntohl(nd_ra->nd_ra_reachable);
	radv->retrans_time = ntohl(nd_ra->nd_ra_retransmit);

	while ((size_t)len >= sizeof(struct nd_opt_hdr)) {
		struct nd_opt_hdr *nd_opt_hdr = (struct nd_opt_hdr *)p;
		struct nd_opt_prefix_info *prf;
		struct nd_opt_rdnss *rdnss;
		struct nd_opt_dnssl *dnssl;
		struct in6_addr *in6;
		int i;
		char *nssl;

		len -= sizeof(struct nd_opt_hdr);
		p += sizeof(struct nd_opt_hdr);

		if (nd_opt_hdr->nd_opt_len * 8 - 2 > len) {
			log_warnx("invalid option len: %hhu > %ld",
			    nd_opt_hdr->nd_opt_len, len);
			goto err;
		}

		switch (nd_opt_hdr->nd_opt_type) {
		case ND_OPT_PREFIX_INFORMATION:
			if (nd_opt_hdr->nd_opt_len != 4) {
				log_warn("invalid ND_OPT_PREFIX_INFORMATION: "
				   "len != 4");
				goto err;
			}

			if ((prefix = calloc(1, sizeof(*prefix))) == NULL)
				fatal("calloc");

			prf = (struct nd_opt_prefix_info*) nd_opt_hdr;
			prefix->prefix = prf->nd_opt_pi_prefix;
			prefix->prefix_len = prf->nd_opt_pi_prefix_len;
			prefix->onlink = prf->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_ONLINK;
			prefix->autonomous = prf->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_AUTO;
			prefix->vltime = ntohl(prf->nd_opt_pi_valid_time);
			prefix->pltime = ntohl(prf->nd_opt_pi_preferred_time);
			if (radv->min_lifetime > prefix->pltime)
				radv->min_lifetime = prefix->pltime;

			LIST_INSERT_HEAD(&radv->prefixes, prefix, entries);

			break;

		case ND_OPT_RDNSS:
			if (nd_opt_hdr->nd_opt_len  < 3) {
				log_warnx("invalid ND_OPT_RDNSS: len < 24");
				goto err;
			}

			if ((nd_opt_hdr->nd_opt_len - 1) % 2 != 0) {
				log_warnx("invalid ND_OPT_RDNSS: length with"
				    "out header is not multiply of 16: %d",
				    (nd_opt_hdr->nd_opt_len - 1) * 8);
				goto err;
			}

			rdnss = (struct nd_opt_rdnss*) nd_opt_hdr;

			radv->rdns_lifetime = ntohl(
			    rdnss->nd_opt_rdnss_lifetime);
			if (radv->min_lifetime > radv->rdns_lifetime)
				radv->min_lifetime = radv->rdns_lifetime;

			in6 = (struct in6_addr*) (p + 6);
			for (i=0; i < (nd_opt_hdr->nd_opt_len - 1)/2; i++,
			    in6++) {
				if((rdns = calloc(1, sizeof(*rdns))) == NULL)
					fatal("calloc");
				memcpy(&rdns->rdns, in6, sizeof(rdns->rdns));
				LIST_INSERT_HEAD(&radv->rdns_servers, rdns,
				    entries);
			}
			break;
		case ND_OPT_DNSSL:
			if (nd_opt_hdr->nd_opt_len  < 2) {
				log_warnx("invalid ND_OPT_DNSSL: len < 16");
				goto err;
			}

			dnssl = (struct nd_opt_dnssl*) nd_opt_hdr;

			if ((nssl = parse_dnssl(p + 6,
			    (nd_opt_hdr->nd_opt_len - 1) * 8)) == NULL)
				goto err; /* error logging in parse_dnssl */

			if((ra_dnssl = calloc(1, sizeof(*ra_dnssl))) == NULL)
				fatal("calloc");

			radv->dnssl_lifetime = ntohl(
			    dnssl->nd_opt_dnssl_lifetime);
			if (radv->min_lifetime > radv->dnssl_lifetime)
				radv->min_lifetime = radv->dnssl_lifetime;

			if (strlcpy(ra_dnssl->dnssl, nssl,
			    sizeof(ra_dnssl->dnssl)) >=
			    sizeof(ra_dnssl->dnssl)) {
				log_warn("dnssl too long");
				goto err;
			}
			free(nssl);

			LIST_INSERT_HEAD(&radv->dnssls, ra_dnssl, entries);

			break;
		case ND_OPT_REDIRECTED_HEADER:
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_MTU:
		case ND_OPT_ROUTE_INFO:
#if 0
			log_debug("\tOption: %hhu (len: %hhu) not implemented",
			    nd_opt_hdr->nd_opt_type, nd_opt_hdr->nd_opt_len *
			    8);
#endif
			break;
		default:
			log_debug("\t\tUNKNOWN: %d", nd_opt_hdr->nd_opt_type);
			break;

		}
		len -= nd_opt_hdr->nd_opt_len * 8 - 2;
		p += nd_opt_hdr->nd_opt_len * 8 - 2;
	}
	update_iface_ra(iface, radv);
	iface->state = IF_IDLE;
	return;

err:
	free_ra(radv);
}


void
debug_log_ra(struct imsg_ra *ra)
{
	struct nd_router_advert	*nd_ra;
	ssize_t			 len = ra->len;
	char			 hbuf[NI_MAXHOST], ntopbuf[INET6_ADDRSTRLEN];
	uint8_t			*p;

	if (getnameinfo((struct sockaddr *)&ra->from, ra->from.sin6_len, hbuf,
	    sizeof(hbuf), NULL, 0, NI_NUMERICHOST | NI_NUMERICSERV)) {
		log_warn("cannot get router IP");
		strlcpy(hbuf, "uknown", sizeof(hbuf));
	}

	if (!IN6_IS_ADDR_LINKLOCAL(&ra->from.sin6_addr)) {
		log_warn("RA from non link local address %s", hbuf);
		return;
	}

	if ((size_t)len < sizeof(struct nd_router_advert)) {
		log_warn("received to short message (%ld) from %s", len, hbuf);
		return;
	}

	p = ra->packet;
	nd_ra = (struct nd_router_advert *)p;
	len -= sizeof(struct nd_router_advert);
	p += sizeof(struct nd_router_advert);

	log_debug("ICMPv6 type(%d), code(%d) from %s of length %ld",
	    nd_ra->nd_ra_type, nd_ra->nd_ra_code, hbuf, len);

	if (nd_ra->nd_ra_type != ND_ROUTER_ADVERT) {
		log_warnx("invalid ICMPv6 type (%d) from %s", nd_ra->nd_ra_type,
		    hbuf);
		return;
	}

	if (nd_ra->nd_ra_code != 0) {
		log_warn("invalid ICMPv6 code (%d) from %s", nd_ra->nd_ra_code,
		    hbuf);
		return;
	}

	log_debug("---");
	log_debug("RA from %s", hbuf);
	log_debug("\tCur Hop Limit: %hhu", nd_ra->nd_ra_curhoplimit);
	log_debug("\tManaged address configuration: %d",
	    (nd_ra->nd_ra_flags_reserved & ND_RA_FLAG_MANAGED) ? 1 : 0);
	log_debug("\tOther configuration: %d",
	    (nd_ra->nd_ra_flags_reserved & ND_RA_FLAG_OTHER) ? 1 : 0);
	switch (nd_ra->nd_ra_flags_reserved & ND_RA_FLAG_RTPREF_MASK) {
	case ND_RA_FLAG_RTPREF_HIGH:
		log_debug("\tRouter Preference: high");
		break;
	case ND_RA_FLAG_RTPREF_MEDIUM:
		log_debug("\tRouter Preference: medium");
		break;
	case ND_RA_FLAG_RTPREF_LOW:
		log_debug("\tRouter Preference: low");
		break;
	case ND_RA_FLAG_RTPREF_RSV:
		log_debug("\tRouter Preference: reserved");
		break;
	}
	log_debug("\tRouter Lifetime: %hds",
	    ntohs(nd_ra->nd_ra_router_lifetime));
	log_debug("\tReachable Time: %ums", ntohl(nd_ra->nd_ra_reachable));
	log_debug("\tRetrans Timer: %ums", ntohl(nd_ra->nd_ra_retransmit));

	while ((size_t)len >= sizeof(struct nd_opt_hdr)) {
		struct nd_opt_hdr *nd_opt_hdr = (struct nd_opt_hdr *)p;
		struct nd_opt_mtu *mtu;
		struct nd_opt_prefix_info *prf;
		struct nd_opt_rdnss *rdnss;
		struct nd_opt_dnssl *dnssl;
		struct in6_addr *in6;
		int i;
		char *nssl;

		len -= sizeof(struct nd_opt_hdr);
		p += sizeof(struct nd_opt_hdr);
		if (nd_opt_hdr->nd_opt_len * 8 - 2 > len) {
			log_warnx("invalid option len: %hhu > %ld",
			    nd_opt_hdr->nd_opt_len, len);
			return;
		}
		log_debug("\tOption: %hhu (len: %hhu)", nd_opt_hdr->nd_opt_type,
		    nd_opt_hdr->nd_opt_len * 8);
		switch (nd_opt_hdr->nd_opt_type) {
		case ND_OPT_SOURCE_LINKADDR:
			if (nd_opt_hdr->nd_opt_len == 1)
				log_debug("\t\tND_OPT_SOURCE_LINKADDR: "
				    "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
				    p[0], p[1], p[2], p[3], p[4], p[5], p[6],
				    p[7]);
			else
				log_debug("\t\tND_OPT_SOURCE_LINKADDR");
			break;
		case ND_OPT_TARGET_LINKADDR:
			if (nd_opt_hdr->nd_opt_len == 1)
				log_debug("\t\tND_OPT_TARGET_LINKADDR: "
				    "%02x:%02x:%02x:%02x:%02x:%02x:%02x:%02x",
				    p[0], p[1], p[2], p[3], p[4], p[5], p[6],
				    p[7]);
			else
				log_debug("\t\tND_OPT_TARGET_LINKADDR");
			break;
		case ND_OPT_PREFIX_INFORMATION:
			if (nd_opt_hdr->nd_opt_len != 4) {
				log_warn("invalid ND_OPT_PREFIX_INFORMATION: "
				   "len != 4");
				return;
			}
			prf = (struct nd_opt_prefix_info*) nd_opt_hdr;

			log_debug("\t\tND_OPT_PREFIX_INFORMATION: %s/%hhu",
			    inet_ntop(AF_INET6, &prf->nd_opt_pi_prefix,
			    ntopbuf, INET6_ADDRSTRLEN),
			    prf->nd_opt_pi_prefix_len);
			log_debug("\t\t\tOn-link: %d",
			    prf->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_ONLINK ? 1:0);
			log_debug("\t\t\tAutonomous address-configuration: %d",
			    prf->nd_opt_pi_flags_reserved &
			    ND_OPT_PI_FLAG_AUTO ? 1 : 0);
			log_debug("\t\t\tvltime: %u",
			    ntohl(prf->nd_opt_pi_valid_time));
			log_debug("\t\t\tpltime: %u",
			    ntohl(prf->nd_opt_pi_preferred_time));
			break;
		case ND_OPT_REDIRECTED_HEADER:
			log_debug("\t\tND_OPT_REDIRECTED_HEADER");
			break;
		case ND_OPT_MTU:
			if (nd_opt_hdr->nd_opt_len != 1) {
				log_warn("invalid ND_OPT_MTU: len != 1");
				return;
			}
			mtu = (struct nd_opt_mtu*) nd_opt_hdr;
			log_debug("\t\tND_OPT_MTU: %u",
			    ntohl(mtu->nd_opt_mtu_mtu));
			break;
		case ND_OPT_ROUTE_INFO:
			log_debug("\t\tND_OPT_ROUTE_INFO");
			break;
		case ND_OPT_RDNSS:
			if (nd_opt_hdr->nd_opt_len  < 3) {
				log_warnx("invalid ND_OPT_RDNSS: len < 24");
				return;
			}
			if ((nd_opt_hdr->nd_opt_len - 1) % 2 != 0) {
				log_warnx("invalid ND_OPT_RDNSS: length with"
				    "out header is not multiply of 16: %d",
				    (nd_opt_hdr->nd_opt_len - 1) * 8);
				return;
			}
			rdnss = (struct nd_opt_rdnss*) nd_opt_hdr;
			log_debug("\t\tND_OPT_RDNSS: lifetime: %u", ntohl(
			    rdnss->nd_opt_rdnss_lifetime));
			in6 = (struct in6_addr*) (p + 6);
			for (i=0; i < (nd_opt_hdr->nd_opt_len - 1)/2; i++,
			    in6++) {
				log_debug("\t\t\t%s", inet_ntop(AF_INET6, in6,
				    ntopbuf, INET6_ADDRSTRLEN));
			}
			break;
		case ND_OPT_DNSSL:
			if (nd_opt_hdr->nd_opt_len  < 2) {
				log_warnx("invalid ND_OPT_DNSSL: len < 16");
				return;
			}
			dnssl = (struct nd_opt_dnssl*) nd_opt_hdr;
			nssl = parse_dnssl(p + 6, (nd_opt_hdr->nd_opt_len - 1)
			    * 8);

			if (nssl == NULL)
				return;

			log_debug("\t\tND_OPT_DNSSL: lifetime: %u", ntohl(
			    dnssl->nd_opt_dnssl_lifetime));
			log_debug("\t\t\tsearch: %s", nssl);

			free(nssl);
			break;
		default:
			log_debug("\t\tUNKNOWN: %d", nd_opt_hdr->nd_opt_type);
			break;

		}
		len -= nd_opt_hdr->nd_opt_len * 8 - 2;
		p += nd_opt_hdr->nd_opt_len * 8 - 2;
	}
}

char*
parse_dnssl(char* data, int datalen)
{
	int len, pos;
	char *nssl, *nsslp;

	if((nssl = calloc(1, datalen + 1)) == NULL) {
		log_warn("malloc");
		return NULL;
	}
	nsslp = nssl;

	pos = 0;

	do {
		len = data[pos];
		if (len > 63 || len + pos + 1 > datalen) {
			free(nssl);
			log_warnx("invalid label in DNSSL");
			return NULL;
		}
		if (len == 0) {
			if ( pos < datalen && data[pos + 1] != 0)
				*nsslp++ = ' '; /* seperator for next domain */
			else
				break;
		} else {
			if (pos != 0 && data[pos - 1] != 0) /* no . at front */
				*nsslp++ = '.';
			memcpy(nsslp, data + pos + 1, len);
			nsslp += len;
		}
		pos += len + 1;
	} while(pos < datalen);
	if (len != 0) {
		free(nssl);
		log_warnx("invalid label in DNSSL");
		return NULL;
	}
	return nssl;
}

void
update_iface_ra(struct slaacd_iface *iface, struct radv *ra)
{

	struct radv		*old_ra;
	struct timeval		 tv;

	old_ra = find_ra(iface, &ra->from);

	if (old_ra == NULL)
		LIST_INSERT_HEAD(&iface->radvs, ra, entries);
	else {
		LIST_REPLACE(old_ra, ra, entries);
		free_ra(old_ra);
	}

	/*
	 * if we haven't gotten an adv the maximum amount of seconds we
	 * would wait if this were a startup before the previous adv
	 * expires start the probe process
	 */
	tv.tv_sec = ra->min_lifetime - MAX_RTR_SOLICITATIONS *
	    (RTR_SOLICITATION_INTERVAL + 1);
	tv.tv_usec = 0;

	log_debug("%s: iface %d: %lld s", __func__, iface->if_index, tv.tv_sec);

	evtimer_set(&ra->timer, ra_timeout, iface);
	evtimer_add(&ra->timer, &tv);
}

void
start_probe(struct slaacd_iface *iface)
{
	struct timeval	tv;

	iface->state = IF_DELAY;
	iface->probes = 0;

	tv.tv_sec = 0;
	tv.tv_usec = arc4random_uniform(MAX_RTR_SOLICITATION_DELAY_USEC);

	log_debug("%s: iface %d: sleeping for %ldusec", __func__,
	    iface->if_index, tv.tv_usec);

	evtimer_add(&iface->timer, &tv);
}

void
ra_timeout(int fd, short events, void *arg)
{
	struct slaacd_iface	*iface = (struct slaacd_iface *)arg;

	start_probe(iface);
}

void
iface_timeout(int fd, short events, void *arg)
{
	struct slaacd_iface	*iface = (struct slaacd_iface *)arg;
	struct timeval		 tv;

	log_debug("%s[%d]: %s", __func__, iface->if_index,
	    if_state_name[iface->state]);

	switch (iface->state) {
		case IF_DELAY:
		case IF_PROBE:
			iface->state = IF_PROBE;
			engine_imsg_compose_frontend(
			    IMSG_CTL_SEND_SOLICITATION, 0, &iface->if_index,
			    sizeof(iface->if_index));
			if (++iface->probes >= MAX_RTR_SOLICITATIONS)
				iface->state = IF_IDLE;
			else {
				tv.tv_sec = RTR_SOLICITATION_INTERVAL;
				tv.tv_usec = arc4random_uniform(1000000);
				evtimer_add(&iface->timer, &tv);
			}
			break;		
		case IF_DOWN:
		case IF_IDLE:
		default:
			break;
	}
}

struct radv*
find_ra(struct slaacd_iface *iface, struct sockaddr_in6 *from)
{
	struct radv	*ra;

	LIST_FOREACH (ra, &iface->radvs, entries) {
		if (memcmp(&ra->from.sin6_addr, &from->sin6_addr,
		    sizeof(from->sin6_addr)) == 0)
			return (ra);
	}

	return (NULL);
	
}
