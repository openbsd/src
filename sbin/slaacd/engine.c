/*	$OpenBSD: engine.c,v 1.31 2018/07/27 06:23:08 bket Exp $	*/

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
#include <net/route.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet/icmp6.h>

#include <crypto/sha2.h>

#include <errno.h>
#include <event.h>
#include <imsg.h>
#include <pwd.h>
#include <signal.h>
#include <stddef.h>
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

enum proposal_state {
	PROPOSAL_NOT_CONFIGURED,
	PROPOSAL_SENT,
	PROPOSAL_CONFIGURED,
	PROPOSAL_NEARLY_EXPIRED,
	PROPOSAL_WITHDRAWN,
	PROPOSAL_DUPLICATED,
};

const char* proposal_state_name[] = {
	"NOT_CONFIGURED",
	"SENT",
	"CONFIGURED",
	"NEARLY_EXPIRED",
	"WITHDRAWN",
	"DUPLICATED",
};

const char* rpref_name[] = {
	"Low",
	"Medium",
	"High",
};

struct radv_prefix {
	LIST_ENTRY(radv_prefix)	entries;
	struct in6_addr		prefix;
	uint8_t			prefix_len; /*XXX int */
	int			onlink;
	int			autonomous;
	uint32_t		vltime;
	uint32_t		pltime;
	int			dad_counter;
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
	uint32_t			 mtu;
};

struct address_proposal {
	LIST_ENTRY(address_proposal)	 entries;
	struct event			 timer;
	int64_t				 id;
	enum proposal_state		 state;
	time_t				 next_timeout;
	int				 timeout_count;
	struct timespec			 when;
	struct timespec			 uptime;
	uint32_t			 if_index;
	struct ether_addr		 hw_address;
	struct sockaddr_in6		 addr;
	struct in6_addr			 mask;
	struct in6_addr			 prefix;
	int				 privacy;
	uint8_t				 prefix_len;
	uint32_t			 vltime;
	uint32_t			 pltime;
	uint8_t				 soiikey[SLAACD_SOIIKEY_LEN];
	uint32_t			 mtu;
};

struct dfr_proposal {
	LIST_ENTRY(dfr_proposal)	 entries;
	struct event			 timer;
	int64_t				 id;
	enum proposal_state		 state;
	time_t				 next_timeout;
	int				 timeout_count;
	struct timespec			 when;
	struct timespec			 uptime;
	uint32_t			 if_index;
	struct sockaddr_in6		 addr;
	uint32_t			 router_lifetime;
	enum rpref			 rpref;
};

struct slaacd_iface {
	LIST_ENTRY(slaacd_iface)	 entries;
	enum if_state			 state;
	struct event			 timer;
	int				 probes;
	uint32_t			 if_index;
	int				 running;
	int				 autoconfprivacy;
	int				 soii;
	struct ether_addr		 hw_address;
	struct sockaddr_in6		 ll_address;
	uint8_t				 soiikey[SLAACD_SOIIKEY_LEN];
	int				 link_state;
	uint32_t			 cur_mtu;
	LIST_HEAD(, radv)		 radvs;
	LIST_HEAD(, address_proposal)	 addr_proposals;
	LIST_HEAD(, dfr_proposal)	 dfr_proposals;
};

LIST_HEAD(, slaacd_iface) slaacd_interfaces;

__dead void		 engine_shutdown(void);
void			 engine_sig_handler(int sig, short, void *);
void			 engine_dispatch_frontend(int, short, void *);
void			 engine_dispatch_main(int, short, void *);
#ifndef	SMALL
void			 send_interface_info(struct slaacd_iface *, pid_t);
void			 engine_showinfo_ctl(struct imsg *, uint32_t);
void			 debug_log_ra(struct imsg_ra *);
int			 in6_mask2prefixlen(struct in6_addr *);
void			 deprecate_all_proposals(struct slaacd_iface *);
#endif	/* SMALL */
struct slaacd_iface	*get_slaacd_iface_by_id(uint32_t);
void			 remove_slaacd_iface(uint32_t);
void			 free_ra(struct radv *);
void			 parse_ra(struct slaacd_iface *, struct imsg_ra *);
void			 gen_addr(struct slaacd_iface *, struct radv_prefix *,
			     struct address_proposal *, int);
void			 gen_address_proposal(struct slaacd_iface *, struct
			     radv *, struct radv_prefix *, int);
void			 free_address_proposal(struct address_proposal *);
void			 timeout_from_lifetime(struct address_proposal *);
void			 configure_address(struct address_proposal *);
void			 in6_prefixlen2mask(struct in6_addr *, int len);
void			 gen_dfr_proposal(struct slaacd_iface *, struct
			     radv *);
void			 configure_dfr(struct dfr_proposal *);
void			 free_dfr_proposal(struct dfr_proposal *);
void			 withdraw_dfr(struct dfr_proposal *);
char			*parse_dnssl(char *, int);
void			 update_iface_ra(struct slaacd_iface *, struct radv *);
void			 send_proposal(struct imsg_proposal *);
void			 start_probe(struct slaacd_iface *);
void			 address_proposal_timeout(int, short, void *);
void			 dfr_proposal_timeout(int, short, void *);
void			 iface_timeout(int, short, void *);
struct radv		*find_ra(struct slaacd_iface *, struct sockaddr_in6 *);
struct address_proposal	*find_address_proposal_by_id(struct slaacd_iface *,
			     int64_t);
struct address_proposal	*find_address_proposal_by_addr(struct slaacd_iface *,
			     struct sockaddr_in6 *);
struct dfr_proposal	*find_dfr_proposal_by_id(struct slaacd_iface *,
			     int64_t);
struct dfr_proposal	*find_dfr_proposal_by_gw(struct slaacd_iface *,
			     struct sockaddr_in6 *);
struct radv_prefix	*find_prefix(struct radv *, struct radv_prefix *);
int			 engine_imsg_compose_main(int, pid_t, void *, uint16_t);
uint32_t		 real_lifetime(struct timespec *, uint32_t);
void			 merge_dad_couters(struct radv *, struct radv *);

struct imsgev		*iev_frontend;
struct imsgev		*iev_main;
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

	if ((pw = getpwnam(SLAACD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	slaacd_process = PROC_ENGINE;
	setproctitle("%s", log_procnames[slaacd_process]);
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
	struct slaacd_iface		*iface;
	struct imsg_ra			 ra;
	struct imsg_proposal_ack	 proposal_ack;
	struct address_proposal		*addr_proposal = NULL;
	struct dfr_proposal		*dfr_proposal = NULL;
	struct imsg_del_addr		 del_addr;
	struct imsg_del_route		 del_route;
	struct imsg_dup_addr		 dup_addr;
	struct timeval			 tv;
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
			/* Already checked by frontend. */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_setverbose(verbose);
			break;
		case IMSG_CTL_SHOW_INTERFACE_INFO:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(if_index))
				fatal("%s: IMSG_CTL_SHOW_INTERFACE_INFO wrong "
				    "length: %d", __func__, imsg.hdr.len);
			memcpy(&if_index, imsg.data, sizeof(if_index));
			engine_showinfo_ctl(&imsg, if_index);
			break;
#endif	/* SMALL */
		case IMSG_REMOVE_IF:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(if_index))
				fatal("%s: IMSG_REMOVE_IF wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&if_index, imsg.data, sizeof(if_index));
			remove_slaacd_iface(if_index);
			break;
		case IMSG_RA:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(ra))
				fatal("%s: IMSG_RA wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&ra, imsg.data, sizeof(ra));
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
				log_warnx("requested to send solicitation on "
				    "non-autoconf interface: %u", if_index);
			else
				engine_imsg_compose_frontend(
				    IMSG_CTL_SEND_SOLICITATION, imsg.hdr.pid,
				    &iface->if_index, sizeof(iface->if_index));
			break;
		case IMSG_PROPOSAL_ACK:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(proposal_ack))
				fatal("%s: IMSG_PROPOSAL_ACK wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&proposal_ack, imsg.data, sizeof(proposal_ack));
			log_debug("%s: IMSG_PROPOSAL_ACK: %lld - %d", __func__,
			    proposal_ack.id, proposal_ack.pid);
			if (proposal_ack.pid != getpid()) {
				log_debug("IMSG_PROPOSAL_ACK: wrong pid, "
				    "ignoring");
				break;
			}

			iface = get_slaacd_iface_by_id(proposal_ack.if_index);
			if (iface == NULL) {
				log_debug("IMSG_PROPOSAL_ACK: unknown interface"
				    ", ignoring");
				break;
			}

			addr_proposal = find_address_proposal_by_id(iface,
			    proposal_ack.id);
			if (addr_proposal == NULL) {
				dfr_proposal = find_dfr_proposal_by_id(iface,
				    proposal_ack.id);
				if (dfr_proposal == NULL) {
					log_debug("IMSG_PROPOSAL_ACK: cannot "
					    "find proposal, ignoring");
					break;
				}
			}
			if (addr_proposal != NULL)
				configure_address(addr_proposal);
			else if (dfr_proposal != NULL)
				configure_dfr(dfr_proposal);

			break;
		case IMSG_DEL_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(del_addr))
				fatal("%s: IMSG_DEL_ADDRESS wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&del_addr, imsg.data, sizeof(del_addr));
			iface = get_slaacd_iface_by_id(del_addr.if_index);
			if (iface == NULL) {
				log_debug("IMSG_DEL_ADDRESS: unknown interface"
				    ", ignoring");
				break;
			}

			addr_proposal = find_address_proposal_by_addr(iface,
			    &del_addr.addr);

			if (addr_proposal) {
				/* XXX should we inform netcfgd? */
				LIST_REMOVE(addr_proposal, entries);
				free_address_proposal(addr_proposal);
			}

			break;
		case IMSG_DEL_ROUTE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(del_route))
				fatal("%s: IMSG_DEL_ROUTE wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&del_route, imsg.data, sizeof(del_route));
			iface = get_slaacd_iface_by_id(del_route.if_index);
			if (iface == NULL) {
				log_debug("IMSG_DEL_ROUTE: unknown interface"
				    ", ignoring");
				break;
			}

			dfr_proposal = find_dfr_proposal_by_gw(iface,
			    &del_route.gw);

			if (dfr_proposal) {
				dfr_proposal->state = PROPOSAL_WITHDRAWN;
				free_dfr_proposal(dfr_proposal);
				start_probe(iface);
			}
			break;
		case IMSG_DUP_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(dup_addr))
				fatal("%s: IMSG_DUP_ADDRESS wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&dup_addr, imsg.data, sizeof(dup_addr));
			iface = get_slaacd_iface_by_id(dup_addr.if_index);
			if (iface == NULL) {
				log_debug("IMSG_DUP_ADDRESS: unknown interface"
				    ", ignoring");
				break;
			}

			addr_proposal = find_address_proposal_by_addr(iface,
			    &dup_addr.addr);

			if (addr_proposal) {
				/* XXX should we inform netcfgd? */
				addr_proposal->state = PROPOSAL_DUPLICATED;
				tv.tv_sec = 0;
				tv.tv_usec = arc4random_uniform(1000000);
				addr_proposal->next_timeout = 0;
				evtimer_add(&addr_proposal->timer, &tv);
			}
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
	struct slaacd_iface	*iface;
	ssize_t			 n;
	int			 shut = 0;
#ifndef	SMALL
	struct imsg_addrinfo	 imsg_addrinfo;
	struct imsg_link_state	 imsg_link_state;
	struct address_proposal	*addr_proposal = NULL;
	size_t			 i;
#endif	/* SMALL */

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
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(imsg_ifinfo))
				fatal("%s: IMSG_UPDATE_IF wrong length: %d",
				    __func__, imsg.hdr.len);
			memcpy(&imsg_ifinfo, imsg.data, sizeof(imsg_ifinfo));

			iface = get_slaacd_iface_by_id(imsg_ifinfo.if_index);
			if (iface == NULL) {
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
				iface->soii = imsg_ifinfo.soii;
				memcpy(&iface->hw_address,
				    &imsg_ifinfo.hw_address,
				    sizeof(struct ether_addr));
				memcpy(&iface->ll_address,
				    &imsg_ifinfo.ll_address,
				    sizeof(struct sockaddr_in6));
				memcpy(iface->soiikey, imsg_ifinfo.soiikey,
				    sizeof(iface->soiikey));
				LIST_INIT(&iface->radvs);
				LIST_INSERT_HEAD(&slaacd_interfaces,
				    iface, entries);
				LIST_INIT(&iface->addr_proposals);
				LIST_INIT(&iface->dfr_proposals);
			} else {
				int need_refresh = 0;

				if (iface->autoconfprivacy !=
				    imsg_ifinfo.autoconfprivacy) {
					iface->autoconfprivacy =
					    imsg_ifinfo.autoconfprivacy;
					need_refresh = 1;
				}

				if (iface->soii !=
				    imsg_ifinfo.soii) {
					iface->soii =
					    imsg_ifinfo.soii;
					need_refresh = 1;
				}

				if (memcmp(&iface->hw_address,
					    &imsg_ifinfo.hw_address,
					    sizeof(struct ether_addr)) != 0) {
					memcpy(&iface->hw_address,
					    &imsg_ifinfo.hw_address,
					    sizeof(struct ether_addr));
					need_refresh = 1;
				}
				if (memcmp(iface->soiikey,
					    imsg_ifinfo.soiikey,
					    sizeof(iface->soiikey)) != 0) {
					memcpy(iface->soiikey,
					    imsg_ifinfo.soiikey,
					    sizeof(iface->soiikey));
					need_refresh = 1;
				}

				if (iface->state != IF_DOWN &&
				    imsg_ifinfo.running && need_refresh)
					start_probe(iface);

				iface->running = imsg_ifinfo.running;
				if (!iface->running) {
					iface->state = IF_DOWN;
					if (evtimer_pending(&iface->timer,
					    NULL))
						evtimer_del(&iface->timer);
				}

				memcpy(&iface->ll_address,
				    &imsg_ifinfo.ll_address,
				    sizeof(struct sockaddr_in6));
			}
			break;
#ifndef	SMALL
		case IMSG_UPDATE_ADDRESS:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(imsg_addrinfo))
				fatal("%s: IMSG_UPDATE_ADDRESS wrong length: "
				    "%d", __func__, imsg.hdr.len);

			memcpy(&imsg_addrinfo, imsg.data,
			    sizeof(imsg_addrinfo));

			iface = get_slaacd_iface_by_id(imsg_addrinfo.if_index);
			if (iface == NULL)
				break;

			log_debug("%s: IMSG_UPDATE_ADDRESS", __func__);

			addr_proposal = find_address_proposal_by_addr(iface,
			    &imsg_addrinfo.addr);
			if (addr_proposal)
				break;

			if ((addr_proposal = calloc(1,
			    sizeof(*addr_proposal))) == NULL)
				fatal("calloc");
			evtimer_set(&addr_proposal->timer,
			    address_proposal_timeout, addr_proposal);
			addr_proposal->id = ++proposal_id;
			addr_proposal->state = PROPOSAL_CONFIGURED;
			addr_proposal->vltime = imsg_addrinfo.vltime;
			addr_proposal->pltime = imsg_addrinfo.pltime;
			addr_proposal->timeout_count = 0;

			timeout_from_lifetime(addr_proposal);

			if (clock_gettime(CLOCK_REALTIME, &addr_proposal->when))
				fatal("clock_gettime");
			if (clock_gettime(CLOCK_MONOTONIC,
			    &addr_proposal->uptime))
				fatal("clock_gettime");
			addr_proposal->if_index = imsg_addrinfo.if_index;
			memcpy(&addr_proposal->hw_address,
			    &imsg_addrinfo.hw_address,
			    sizeof(addr_proposal->hw_address));
			addr_proposal->addr = imsg_addrinfo.addr;
			addr_proposal->mask = imsg_addrinfo.mask;
			addr_proposal->prefix = addr_proposal->addr.sin6_addr;

			for (i = 0; i < sizeof(addr_proposal->prefix.s6_addr) /
			    sizeof(addr_proposal->prefix.s6_addr[0]); i++)
				addr_proposal->prefix.s6_addr[i] &=
				    addr_proposal->mask.s6_addr[i];

			addr_proposal->privacy = imsg_addrinfo.privacy;
			addr_proposal->prefix_len =
			    in6_mask2prefixlen(&addr_proposal->mask);

			LIST_INSERT_HEAD(&iface->addr_proposals,
			    addr_proposal, entries);

			break;
		case IMSG_UPDATE_LINK_STATE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(imsg_link_state))
				fatal("%s: IMSG_UPDATE_LINK_STATE wrong "
				    "length: %d", __func__, imsg.hdr.len);

			memcpy(&imsg_link_state, imsg.data,
			    sizeof(imsg_link_state));

			iface = get_slaacd_iface_by_id(
			    imsg_link_state.if_index);
			if (iface == NULL)
				break;
			if (iface->link_state != imsg_link_state.link_state) {
				iface->link_state = imsg_link_state.link_state;
				if (iface->link_state == LINK_STATE_DOWN)
					deprecate_all_proposals(iface);
				else
					start_probe(iface);
			}
			break;
#endif	/* SMALL */
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
send_interface_info(struct slaacd_iface *iface, pid_t pid)
{
	struct ctl_engine_info			 cei;
	struct ctl_engine_info_ra		 cei_ra;
	struct ctl_engine_info_ra_prefix	 cei_ra_prefix;
	struct ctl_engine_info_ra_rdns		 cei_ra_rdns;
	struct ctl_engine_info_ra_dnssl		 cei_ra_dnssl;
	struct ctl_engine_info_address_proposal	 cei_addr_proposal;
	struct ctl_engine_info_dfr_proposal	 cei_dfr_proposal;
	struct radv				*ra;
	struct radv_prefix			*prefix;
	struct radv_rdns			*rdns;
	struct radv_dnssl			*dnssl;
	struct address_proposal			*addr_proposal;
	struct dfr_proposal			*dfr_proposal;

	memset(&cei, 0, sizeof(cei));
	cei.if_index = iface->if_index;
	cei.running = iface->running;
	cei.autoconfprivacy = iface->autoconfprivacy;
	cei.soii = iface->soii;
	memcpy(&cei.hw_address, &iface->hw_address, sizeof(struct ether_addr));
	memcpy(&cei.ll_address, &iface->ll_address,
	    sizeof(struct sockaddr_in6));
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
		if (strlcpy(cei_ra.rpref, rpref_name[ra->rpref], sizeof(
		    cei_ra.rpref)) >= sizeof(cei_ra.rpref))
			log_warnx("truncated router preference");
		cei_ra.router_lifetime = ra->router_lifetime;
		cei_ra.reachable_time = ra->reachable_time;
		cei_ra.retrans_time = ra->retrans_time;
		cei_ra.mtu = ra->mtu;
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

	if (!LIST_EMPTY(&iface->addr_proposals))
		engine_imsg_compose_frontend(
		    IMSG_CTL_SHOW_INTERFACE_INFO_ADDR_PROPOSALS, pid, NULL, 0);

	LIST_FOREACH(addr_proposal, &iface->addr_proposals, entries) {
		memset(&cei_addr_proposal, 0, sizeof(cei_addr_proposal));
		cei_addr_proposal.id = addr_proposal->id;
		if(strlcpy(cei_addr_proposal.state,
		    proposal_state_name[addr_proposal->state],
		    sizeof(cei_addr_proposal.state)) >=
		    sizeof(cei_addr_proposal.state))
			log_warnx("truncated state name");
		cei_addr_proposal.next_timeout = addr_proposal->next_timeout;
		cei_addr_proposal.timeout_count = addr_proposal->timeout_count;
		cei_addr_proposal.when = addr_proposal->when;
		cei_addr_proposal.uptime = addr_proposal->uptime;
		memcpy(&cei_addr_proposal.addr, &addr_proposal->addr, sizeof(
		    cei_addr_proposal.addr));
		memcpy(&cei_addr_proposal.prefix, &addr_proposal->prefix,
		    sizeof(cei_addr_proposal.prefix));
		cei_addr_proposal.prefix_len = addr_proposal->prefix_len;
		cei_addr_proposal.privacy = addr_proposal->privacy;
		cei_addr_proposal.vltime = addr_proposal->vltime;
		cei_addr_proposal.pltime = addr_proposal->pltime;

		engine_imsg_compose_frontend(
		    IMSG_CTL_SHOW_INTERFACE_INFO_ADDR_PROPOSAL, pid,
			    &cei_addr_proposal, sizeof(cei_addr_proposal));
	}

	if (!LIST_EMPTY(&iface->dfr_proposals))
		engine_imsg_compose_frontend(
		    IMSG_CTL_SHOW_INTERFACE_INFO_DFR_PROPOSALS, pid, NULL, 0);

	LIST_FOREACH(dfr_proposal, &iface->dfr_proposals, entries) {
		memset(&cei_dfr_proposal, 0, sizeof(cei_dfr_proposal));
		cei_dfr_proposal.id = dfr_proposal->id;
		if(strlcpy(cei_dfr_proposal.state,
		    proposal_state_name[dfr_proposal->state],
		    sizeof(cei_dfr_proposal.state)) >=
		    sizeof(cei_dfr_proposal.state))
			log_warnx("truncated state name");
		cei_dfr_proposal.next_timeout = dfr_proposal->next_timeout;
		cei_dfr_proposal.timeout_count = dfr_proposal->timeout_count;
		cei_dfr_proposal.when = dfr_proposal->when;
		cei_dfr_proposal.uptime = dfr_proposal->uptime;
		memcpy(&cei_dfr_proposal.addr, &dfr_proposal->addr, sizeof(
		    cei_dfr_proposal.addr));
		cei_dfr_proposal.router_lifetime =
		    dfr_proposal->router_lifetime;
		if(strlcpy(cei_dfr_proposal.rpref,
		    rpref_name[dfr_proposal->rpref],
		    sizeof(cei_dfr_proposal.rpref)) >=
		    sizeof(cei_dfr_proposal.rpref))
			log_warnx("truncated router preference");
		engine_imsg_compose_frontend(
		    IMSG_CTL_SHOW_INTERFACE_INFO_DFR_PROPOSAL, pid,
			    &cei_dfr_proposal, sizeof(cei_dfr_proposal));
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
void
deprecate_all_proposals(struct slaacd_iface *iface)
{
	struct address_proposal	*addr_proposal;

	log_debug("%s: iface: %d", __func__, iface->if_index);

	LIST_FOREACH (addr_proposal, &iface->addr_proposals, entries) {
		addr_proposal->pltime = 0;
		configure_address(addr_proposal);
		addr_proposal->state = PROPOSAL_NEARLY_EXPIRED;
	}
}
#endif	/* SMALL */

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
	struct slaacd_iface	*iface;
	struct radv		*ra;
	struct address_proposal	*addr_proposal;
	struct dfr_proposal	*dfr_proposal;

	iface = get_slaacd_iface_by_id(if_index);

	if (iface == NULL)
		return;

	LIST_REMOVE(iface, entries);
	while(!LIST_EMPTY(&iface->radvs)) {
		ra = LIST_FIRST(&iface->radvs);
		LIST_REMOVE(ra, entries);
		free_ra(ra);
	}
	/* XXX inform netcfgd? */
	while(!LIST_EMPTY(&iface->addr_proposals)) {
		addr_proposal = LIST_FIRST(&iface->addr_proposals);
		LIST_REMOVE(addr_proposal, entries);
		free_address_proposal(addr_proposal);
	}
	while(!LIST_EMPTY(&iface->dfr_proposals)) {
		dfr_proposal = LIST_FIRST(&iface->dfr_proposals);
		LIST_REMOVE(dfr_proposal, entries);
		free_dfr_proposal(dfr_proposal);
	}
	evtimer_del(&iface->timer);
	free(iface);
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
	const char		*hbuf;
	uint8_t			*p;

#ifndef	SMALL
	if (log_getverbose() > 1)
		debug_log_ra(ra);
#endif	/* SMALL */

	hbuf = sin6_to_str(&ra->from);
	if (!IN6_IS_ADDR_LINKLOCAL(&ra->from.sin6_addr)) {
		log_warnx("RA from non link local address %s", hbuf);
		return;
	}

	if ((size_t)len < sizeof(struct nd_router_advert)) {
		log_warnx("received too short message (%ld) from %s", len,
		    hbuf);
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
		log_warnx("invalid ICMPv6 code (%d) from %s", nd_ra->nd_ra_code,
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
		struct nd_opt_mtu *mtu;
		struct in6_addr *in6;
		int i;
		char *nssl;

		len -= sizeof(struct nd_opt_hdr);
		p += sizeof(struct nd_opt_hdr);

		if (nd_opt_hdr->nd_opt_len * 8 - 2 > len) {
			log_warnx("invalid option len: %u > %ld",
			    nd_opt_hdr->nd_opt_len, len);
			goto err;
		}

		switch (nd_opt_hdr->nd_opt_type) {
		case ND_OPT_PREFIX_INFORMATION:
			if (nd_opt_hdr->nd_opt_len != 4) {
				log_warnx("invalid ND_OPT_PREFIX_INFORMATION: "
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
				log_warnx("dnssl too long");
				goto err;
			}
			free(nssl);

			LIST_INSERT_HEAD(&radv->dnssls, ra_dnssl, entries);

			break;
		case ND_OPT_MTU:
			if (nd_opt_hdr->nd_opt_len != 1) {
				log_warnx("invalid ND_OPT_MTU: len != 1");
				goto err;
			}
			mtu = (struct nd_opt_mtu*) nd_opt_hdr;
			radv->mtu = ntohl(mtu->nd_opt_mtu_mtu);

			/* path MTU cannot be less than IPV6_MMTU */
			if (radv->mtu < IPV6_MMTU) {
				radv->mtu = 0;
				log_warnx("invalid advertised MTU");
			}

			break;
		case ND_OPT_REDIRECTED_HEADER:
		case ND_OPT_SOURCE_LINKADDR:
		case ND_OPT_TARGET_LINKADDR:
		case ND_OPT_ROUTE_INFO:
#if 0
			log_debug("\tOption: %u (len: %u) not implemented",
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
gen_addr(struct slaacd_iface *iface, struct radv_prefix *prefix, struct
    address_proposal *addr_proposal, int privacy)
{
	SHA2_CTX ctx;
	struct in6_addr	iid;
	int i;
	u_int8_t digest[SHA512_DIGEST_LENGTH];

	memset(&iid, 0, sizeof(iid));

	/* from in6_ifadd() in nd6_rtr.c */
	/* XXX from in6.h, guarded by #ifdef _KERNEL   XXX nonstandard */
#define s6_addr32 __u6_addr.__u6_addr32

	in6_prefixlen2mask(&addr_proposal->mask, addr_proposal->prefix_len);

	memset(&addr_proposal->addr, 0, sizeof(addr_proposal->addr));

	addr_proposal->addr.sin6_family = AF_INET6;
	addr_proposal->addr.sin6_len = sizeof(addr_proposal->addr);

	memcpy(&addr_proposal->addr.sin6_addr, &prefix->prefix,
	    sizeof(addr_proposal->addr.sin6_addr));

	for (i = 0; i < 4; i++)
		addr_proposal->addr.sin6_addr.s6_addr32[i] &=
		    addr_proposal->mask.s6_addr32[i];

	if (privacy) {
		arc4random_buf(&iid.s6_addr, sizeof(iid.s6_addr));
	} else if (iface->soii) {
		SHA512Init(&ctx);
		SHA512Update(&ctx, &prefix->prefix,
		    sizeof(prefix->prefix));
		SHA512Update(&ctx, &iface->hw_address,
		    sizeof(iface->hw_address));
		SHA512Update(&ctx, &prefix->dad_counter,
		    sizeof(prefix->dad_counter));
		SHA512Update(&ctx, addr_proposal->soiikey,
		    sizeof(addr_proposal->soiikey));
		SHA512Final(digest, &ctx);

		memcpy(&iid.s6_addr, digest + (sizeof(digest) -
		    sizeof(iid.s6_addr)), sizeof(iid.s6_addr));
	} else {
		/* This is safe, because we have a 64 prefix len */
		memcpy(&iid.s6_addr, &iface->ll_address.sin6_addr,
		    sizeof(iid.s6_addr));
	}

	for (i = 0; i < 4; i++)
		addr_proposal->addr.sin6_addr.s6_addr32[i] |=
		    (iid.s6_addr32[i] & ~addr_proposal->mask.s6_addr32[i]);
#undef s6_addr32
}

/* from sys/netinet6/in6.c */
void
in6_prefixlen2mask(struct in6_addr *maskp, int len)
{
	u_char maskarray[8] = {0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff};
	int bytelen, bitlen, i;

	if (0 > len || len > 128)
		fatal("%s: invalid prefix length(%d)\n", __func__, len);

	bzero(maskp, sizeof(*maskp));
	bytelen = len / 8;
	bitlen = len % 8;
	for (i = 0; i < bytelen; i++)
		maskp->s6_addr[i] = 0xff;
	/* len == 128 is ok because bitlen == 0 then */
	if (bitlen)
		maskp->s6_addr[bytelen] = maskarray[bitlen - 1];
}

#ifndef	SMALL
/* from kame via ifconfig, where it's called prefix() */
int
in6_mask2prefixlen(struct in6_addr *in6)
{
	u_char *nam = (u_char *)in6;
	int byte, bit, plen = 0, size = sizeof(struct in6_addr);

	for (byte = 0; byte < size; byte++, plen += 8)
		if (nam[byte] != 0xff)
			break;
	if (byte == size)
		return (plen);
	for (bit = 7; bit != 0; bit--, plen++)
		if (!(nam[byte] & (1 << bit)))
			break;
	for (; bit != 0; bit--)
		if (nam[byte] & (1 << bit))
			return (0);
	byte++;
	for (; byte < size; byte++)
		if (nam[byte])
			return (0);
	return (plen);
}

void
debug_log_ra(struct imsg_ra *ra)
{
	struct nd_router_advert	*nd_ra;
	ssize_t			 len = ra->len;
	char			 ntopbuf[INET6_ADDRSTRLEN];
	const char		*hbuf;
	uint8_t			*p;

	hbuf = sin6_to_str(&ra->from);

	if (!IN6_IS_ADDR_LINKLOCAL(&ra->from.sin6_addr)) {
		log_warnx("RA from non link local address %s", hbuf);
		return;
	}

	if ((size_t)len < sizeof(struct nd_router_advert)) {
		log_warnx("received too short message (%ld) from %s", len,
		    hbuf);
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
		log_warnx("invalid ICMPv6 code (%d) from %s", nd_ra->nd_ra_code,
		    hbuf);
		return;
	}

	log_debug("---");
	log_debug("RA from %s", hbuf);
	log_debug("\tCur Hop Limit: %u", nd_ra->nd_ra_curhoplimit);
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
			log_warnx("invalid option len: %u > %ld",
			    nd_opt_hdr->nd_opt_len, len);
			return;
		}
		log_debug("\tOption: %u (len: %u)", nd_opt_hdr->nd_opt_type,
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
				log_warnx("invalid ND_OPT_PREFIX_INFORMATION: "
				   "len != 4");
				return;
			}
			prf = (struct nd_opt_prefix_info*) nd_opt_hdr;

			log_debug("\t\tND_OPT_PREFIX_INFORMATION: %s/%u",
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
				log_warnx("invalid ND_OPT_MTU: len != 1");
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
#endif	/* SMALL */

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
			if (pos < datalen && data[pos + 1] != 0)
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

void update_iface_ra(struct slaacd_iface *iface, struct radv *ra)
{
	struct radv		*old_ra;
	struct radv_prefix	*prefix;
	struct address_proposal	*addr_proposal;
	struct dfr_proposal	*dfr_proposal, *tmp;
	uint32_t		 remaining_lifetime;
	int			 found, found_privacy, duplicate_found;
	const char		*hbuf;

	if ((old_ra = find_ra(iface, &ra->from)) == NULL)
		LIST_INSERT_HEAD(&iface->radvs, ra, entries);
	else {
		LIST_REPLACE(old_ra, ra, entries);

		merge_dad_couters(old_ra, ra);

		free_ra(old_ra);
	}
	if (ra->router_lifetime == 0) {
		LIST_FOREACH_SAFE(dfr_proposal, &iface->dfr_proposals, entries,
		    tmp) {
			if (memcmp(&dfr_proposal->addr,
			    &ra->from, sizeof(struct sockaddr_in6)) ==
			    0) {
				free_dfr_proposal(dfr_proposal);
			}
		}
	} else {
		found = 0;
		LIST_FOREACH(dfr_proposal, &iface->dfr_proposals, entries) {
			if (memcmp(&dfr_proposal->addr,
			    &ra->from, sizeof(struct sockaddr_in6)) ==
			    0) {
				found = 1;
				if (real_lifetime(&dfr_proposal->uptime,
				    dfr_proposal->router_lifetime) >
				    ra->router_lifetime)
					log_warnx("ignoring router "
					    "advertisement that lowers router "
					    "lifetime");
				else {
					dfr_proposal->when = ra->when;
					dfr_proposal->uptime = ra->uptime;
					dfr_proposal->router_lifetime =
					    ra->router_lifetime;

					log_debug("%s, dfr state: %s, rl: %d",
					    __func__, proposal_state_name[
					    dfr_proposal->state],
					    real_lifetime(&dfr_proposal->uptime,
					    dfr_proposal->router_lifetime));

					switch (dfr_proposal->state) {
					case PROPOSAL_CONFIGURED:
					case PROPOSAL_NEARLY_EXPIRED:
						log_debug("updating dfr");
						configure_dfr(dfr_proposal);
						break;
					default:
						hbuf = sin6_to_str(
						    &dfr_proposal->addr);
						log_debug("%s: iface %d: %s",
						    __func__, iface->if_index,
						    hbuf);
						break;
					}
				}

				break;
			}
		}
		if (!found)
			/* new proposal */
			gen_dfr_proposal(iface, ra);

		LIST_FOREACH(prefix, &ra->prefixes, entries) {
			if (!prefix->autonomous || prefix->vltime == 0 ||
			    prefix->pltime > prefix->vltime ||
			    IN6_IS_ADDR_LINKLOCAL(&prefix->prefix))
				continue;
			found = 0;
			found_privacy = 0;
			duplicate_found = 0;

			LIST_FOREACH(addr_proposal, &iface->addr_proposals,
			    entries) {
				if (prefix->prefix_len ==
				    addr_proposal-> prefix_len &&
				    memcmp(&prefix->prefix,
				    &addr_proposal->prefix,
				    sizeof(struct in6_addr)) != 0)
					continue;

				if (memcmp(&addr_proposal->hw_address,
				    &iface->hw_address,
				    sizeof(addr_proposal->hw_address)) != 0)
					continue;

				if (memcmp(&addr_proposal->soiikey,
				    &iface->soiikey,
				    sizeof(addr_proposal->soiikey)) != 0)
					continue;

				if (addr_proposal->privacy) {
					/*
					 * create new privacy address if old
					 * expires
					 */
					if (addr_proposal->state !=
					    PROPOSAL_NEARLY_EXPIRED &&
					    addr_proposal->state !=
					    PROPOSAL_DUPLICATED)
						found_privacy = 1;

					if (!iface->autoconfprivacy)
						log_debug("%s XXX need to "
						    "remove privacy address",
						    __func__);

					log_debug("%s, privacy addr state: %s",
					    __func__, proposal_state_name[
					    addr_proposal->state]);

					/* privacy addresses just expire */
					continue;
				}

				if (addr_proposal->state ==
				    PROPOSAL_DUPLICATED) {
					duplicate_found = 1;
					continue;
				}

				found = 1;

				remaining_lifetime =
				    real_lifetime(&addr_proposal->uptime,
				    addr_proposal->vltime);

				addr_proposal->when = ra->when;
				addr_proposal->uptime = ra->uptime;

/* RFC 4862 5.5.3 two hours rule */
#define TWO_HOURS 2 * 3600
				if (prefix->vltime > TWO_HOURS ||
				    prefix->vltime > remaining_lifetime)
					addr_proposal->vltime = prefix->vltime;
				else
					addr_proposal->vltime = TWO_HOURS;
				addr_proposal->pltime = prefix->pltime;

				if (ra->mtu == iface->cur_mtu)
					addr_proposal->mtu = 0;
				else {
					addr_proposal->mtu = ra->mtu;
					iface->cur_mtu = ra->mtu;
				}

				log_debug("%s, addr state: %s", __func__,
				    proposal_state_name[addr_proposal->state]);

				switch (addr_proposal->state) {
				case PROPOSAL_CONFIGURED:
				case PROPOSAL_NEARLY_EXPIRED:
					log_debug("updating address");
					configure_address(addr_proposal);
					break;
				default:
					hbuf = sin6_to_str(&addr_proposal->
					    addr);
					log_debug("%s: iface %d: %s", __func__,
					    iface->if_index, hbuf);
					break;
				}
			}

			if (!found && duplicate_found && iface->soii) {
				prefix->dad_counter++;
				log_debug("%s dad_counter: %d",
				     __func__, prefix->dad_counter);
			}

			if (!found &&
			    (iface->soii || prefix->prefix_len <= 64))
				/* new proposal */
				gen_address_proposal(iface, ra, prefix, 0);

			/* privacy addresses do not depend on eui64 */
			if (!found_privacy && iface->autoconfprivacy) {
				if (prefix->pltime <
				    ND6_PRIV_MAX_DESYNC_FACTOR) {
					hbuf = sin6_to_str(&ra->from);
					log_warnx("%s: pltime from %s is too "
					    "small: %d < %d; not generating "
					    "privacy address", __func__, hbuf,
					    prefix->pltime,
					    ND6_PRIV_MAX_DESYNC_FACTOR);
				} else
					/* new privacy proposal */
					gen_address_proposal(iface, ra, prefix,
					    1);
			}
		}
	}
}

void
timeout_from_lifetime(struct address_proposal *addr_proposal)
{
	struct timeval	 tv;
	time_t		 lifetime;

	addr_proposal->next_timeout = 0;

	if (addr_proposal->pltime > MAX_RTR_SOLICITATIONS *
	    (RTR_SOLICITATION_INTERVAL + 1))
		lifetime = addr_proposal->pltime;
	else
		lifetime = addr_proposal->vltime;

	if (lifetime > MAX_RTR_SOLICITATIONS *
	    (RTR_SOLICITATION_INTERVAL + 1)) {
		addr_proposal->next_timeout = lifetime - MAX_RTR_SOLICITATIONS *
		    (RTR_SOLICITATION_INTERVAL + 1);
		tv.tv_sec = addr_proposal->next_timeout;
		tv.tv_usec = arc4random_uniform(1000000);
		evtimer_add(&addr_proposal->timer, &tv);
		log_debug("%s: %d, scheduling new timeout in %llds.%06ld",
		    __func__, addr_proposal->if_index, tv.tv_sec, tv.tv_usec);
	}
}

void
configure_address(struct address_proposal *addr_proposal)
{
	struct imsg_configure_address	 address;

	timeout_from_lifetime(addr_proposal);
	addr_proposal->state = PROPOSAL_CONFIGURED;

	log_debug("%s: %d", __func__, addr_proposal->if_index);

	address.if_index = addr_proposal->if_index;
	memcpy(&address.addr, &addr_proposal->addr, sizeof(address.addr));
	memcpy(&address.mask, &addr_proposal->mask, sizeof(address.mask));
	address.vltime = addr_proposal->vltime;
	address.pltime = addr_proposal->pltime;
	address.privacy = addr_proposal->privacy;
	address.mtu = addr_proposal->mtu;

	engine_imsg_compose_main(IMSG_CONFIGURE_ADDRESS, 0, &address,
	    sizeof(address));
}

void
gen_address_proposal(struct slaacd_iface *iface, struct radv *ra, struct
    radv_prefix *prefix, int privacy)
{
	struct address_proposal	*addr_proposal;
	struct timeval		 tv;
	const char		*hbuf;

	if ((addr_proposal = calloc(1, sizeof(*addr_proposal))) == NULL)
		fatal("calloc");
	evtimer_set(&addr_proposal->timer, address_proposal_timeout,
	    addr_proposal);
	addr_proposal->next_timeout = 1;
	addr_proposal->timeout_count = 0;
	addr_proposal->state = PROPOSAL_NOT_CONFIGURED;
	addr_proposal->when = ra->when;
	addr_proposal->uptime = ra->uptime;
	addr_proposal->if_index = iface->if_index;
	memcpy(&addr_proposal->hw_address, &iface->hw_address,
	    sizeof(addr_proposal->hw_address));
	memcpy(&addr_proposal->soiikey, &iface->soiikey,
	    sizeof(addr_proposal->soiikey));
	addr_proposal->privacy = privacy;
	memcpy(&addr_proposal->prefix, &prefix->prefix,
	    sizeof(addr_proposal->prefix));
	addr_proposal->prefix_len = prefix->prefix_len;

	if (privacy) {
		if (prefix->vltime > ND6_PRIV_VALID_LIFETIME)
			addr_proposal->vltime = ND6_PRIV_VALID_LIFETIME;
		else
			addr_proposal->vltime = prefix->vltime;

		if (prefix->pltime > ND6_PRIV_PREFERRED_LIFETIME)
			addr_proposal->pltime = ND6_PRIV_PREFERRED_LIFETIME
			    - arc4random_uniform(ND6_PRIV_MAX_DESYNC_FACTOR);
		else
			addr_proposal->pltime = prefix->pltime;
	} else {
		addr_proposal->vltime = prefix->vltime;
		addr_proposal->pltime = prefix->pltime;
	}

	if (ra->mtu == iface->cur_mtu)
		addr_proposal->mtu = 0;
	else {
		addr_proposal->mtu = ra->mtu;
		iface->cur_mtu = ra->mtu;
	}

	gen_addr(iface, prefix, addr_proposal, privacy);

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	evtimer_add(&addr_proposal->timer, &tv);

	LIST_INSERT_HEAD(&iface->addr_proposals, addr_proposal, entries);

	hbuf = sin6_to_str(&addr_proposal->addr);
	log_debug("%s: iface %d: %s: %lld s", __func__,
	    iface->if_index, hbuf, tv.tv_sec);
}

void
free_address_proposal(struct address_proposal *addr_proposal)
{
	if (addr_proposal == NULL)
		return;

	evtimer_del(&addr_proposal->timer);
	free(addr_proposal);
}

void
gen_dfr_proposal(struct slaacd_iface *iface, struct radv *ra)
{
	struct dfr_proposal	*dfr_proposal;
	struct timeval		 tv;
	const char		*hbuf;

	if ((dfr_proposal = calloc(1, sizeof(*dfr_proposal))) == NULL)
		fatal("calloc");
	evtimer_set(&dfr_proposal->timer, dfr_proposal_timeout,
	    dfr_proposal);
	dfr_proposal->next_timeout = 1;
	dfr_proposal->timeout_count = 0;
	dfr_proposal->state = PROPOSAL_NOT_CONFIGURED;
	dfr_proposal->when = ra->when;
	dfr_proposal->uptime = ra->uptime;
	dfr_proposal->if_index = iface->if_index;
	memcpy(&dfr_proposal->addr, &ra->from,
	    sizeof(dfr_proposal->addr));
	dfr_proposal->router_lifetime = ra->router_lifetime;
	dfr_proposal->rpref = ra->rpref;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	evtimer_add(&dfr_proposal->timer, &tv);

	LIST_INSERT_HEAD(&iface->dfr_proposals, dfr_proposal, entries);

	hbuf = sin6_to_str(&dfr_proposal->addr);
	log_debug("%s: iface %d: %s: %lld s", __func__,
	    iface->if_index, hbuf, tv.tv_sec);
}

void
configure_dfr(struct dfr_proposal *dfr_proposal)
{
	struct imsg_configure_dfr	 dfr;
	struct timeval			 tv;
	enum proposal_state		 prev_state;

	if (dfr_proposal->router_lifetime > MAX_RTR_SOLICITATIONS *
	    (RTR_SOLICITATION_INTERVAL + 1)) {
		dfr_proposal->next_timeout = dfr_proposal->router_lifetime -
		    MAX_RTR_SOLICITATIONS * (RTR_SOLICITATION_INTERVAL + 1);
		tv.tv_sec = dfr_proposal->next_timeout;
		tv.tv_usec = arc4random_uniform(1000000);
		evtimer_add(&dfr_proposal->timer, &tv);
		log_debug("%s: %d, scheduling new timeout in %llds.%06ld",
		    __func__, dfr_proposal->if_index, tv.tv_sec, tv.tv_usec);
	} else
		dfr_proposal->next_timeout = 0;

	prev_state = dfr_proposal->state;

	dfr_proposal->state = PROPOSAL_CONFIGURED;

	log_debug("%s: %d", __func__, dfr_proposal->if_index);

	if (prev_state == PROPOSAL_CONFIGURED || prev_state ==
	    PROPOSAL_NEARLY_EXPIRED) {
		/* nothing to do here, routes do not expire in the kernel */
		return;
	}

	dfr.if_index = dfr_proposal->if_index;
	memcpy(&dfr.addr, &dfr_proposal->addr, sizeof(dfr.addr));
	dfr.router_lifetime = dfr_proposal->router_lifetime;

	engine_imsg_compose_main(IMSG_CONFIGURE_DFR, 0, &dfr, sizeof(dfr));
}

void
withdraw_dfr(struct dfr_proposal *dfr_proposal)
{
	struct imsg_configure_dfr	 dfr;

	log_debug("%s: %d", __func__, dfr_proposal->if_index);

	dfr.if_index = dfr_proposal->if_index;
	memcpy(&dfr.addr, &dfr_proposal->addr, sizeof(dfr.addr));
	dfr.router_lifetime = dfr_proposal->router_lifetime;

	engine_imsg_compose_main(IMSG_WITHDRAW_DFR, 0, &dfr, sizeof(dfr));
}

void
free_dfr_proposal(struct dfr_proposal *dfr_proposal)
{

	LIST_REMOVE(dfr_proposal, entries);
	evtimer_del(&dfr_proposal->timer);
	switch (dfr_proposal->state) {
	case PROPOSAL_CONFIGURED:
	case PROPOSAL_NEARLY_EXPIRED:
		withdraw_dfr(dfr_proposal);
		break;
	default:
		break;
	}
	free(dfr_proposal);
}

void
send_proposal(struct imsg_proposal *proposal)
{
#ifndef SKIP_PROPOSAL
	engine_imsg_compose_main(IMSG_PROPOSAL, 0, proposal, sizeof(*proposal));
#else
	struct imsg_proposal_ack	ack;
	ack.id = proposal->id;
	ack.pid = proposal->pid;
	ack.if_index = proposal->if_index;
	engine_imsg_compose_frontend(IMSG_FAKE_ACK, 0, &ack, sizeof(ack));
#endif
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
address_proposal_timeout(int fd, short events, void *arg)
{
	struct address_proposal	*addr_proposal;
	struct imsg_proposal	 proposal;
	struct timeval		 tv;
	const char		*hbuf;

	addr_proposal = (struct address_proposal *)arg;

	hbuf = sin6_to_str(&addr_proposal->addr);
	log_debug("%s: iface %d: %s [%s], priv: %s", __func__,
	    addr_proposal->if_index, hbuf,
	    proposal_state_name[addr_proposal->state],
	    addr_proposal->privacy ? "y" : "n");

	switch (addr_proposal->state) {
	case PROPOSAL_NOT_CONFIGURED:
	case PROPOSAL_SENT:
		if (addr_proposal->timeout_count++ < 6) {
			addr_proposal->id = ++proposal_id;

			memset(&proposal, 0, sizeof(proposal));
			proposal.if_index = addr_proposal->if_index;
			proposal.pid = getpid();
			proposal.id = addr_proposal->id;
			memcpy(&proposal.addr, &addr_proposal->addr,
			    sizeof(proposal.addr));
			memcpy(&proposal.mask, &addr_proposal->mask,
			    sizeof(proposal.mask));

			proposal.rtm_addrs = RTA_NETMASK | RTA_IFA;

			addr_proposal->state = PROPOSAL_SENT;

			send_proposal(&proposal);

			tv.tv_sec = addr_proposal->next_timeout;
			tv.tv_usec = arc4random_uniform(1000000);
			addr_proposal->next_timeout *= 2;
			evtimer_add(&addr_proposal->timer, &tv);
			log_debug("%s: scheduling new timeout in %llds.%06ld",
			    __func__, tv.tv_sec, tv.tv_usec);
		} else {
			log_debug("%s: giving up, no response to proposal",
			    __func__);
			LIST_REMOVE(addr_proposal, entries);
			free_address_proposal(addr_proposal);
		}
		break;
	case PROPOSAL_CONFIGURED:
		log_debug("PROPOSAL_CONFIGURED timeout: id: %lld, privacy: %s",
		    addr_proposal->id, addr_proposal->privacy ? "y" : "n");

		addr_proposal->next_timeout = 1;
		addr_proposal->timeout_count = 0;
		addr_proposal->state = PROPOSAL_NEARLY_EXPIRED;

		tv.tv_sec = 0;
		tv.tv_usec = 0;
		evtimer_add(&addr_proposal->timer, &tv);

		break;
	case PROPOSAL_NEARLY_EXPIRED:
		log_debug("%s: rl: %d", __func__,
		    real_lifetime(&addr_proposal->uptime,
		    addr_proposal->vltime));
		/*
		 * we should have gotten a RTM_DELADDR from the kernel,
		 * in case we missed it, delete to not waste memory
		 */
		if (real_lifetime(&addr_proposal->uptime,
		    addr_proposal->vltime) == 0) {
			evtimer_del(&addr_proposal->timer);
			LIST_REMOVE(addr_proposal, entries);
			free_address_proposal(addr_proposal);
			log_debug("%s: removing address proposal", __func__);
			break;
		}

		engine_imsg_compose_frontend(IMSG_CTL_SEND_SOLICITATION,
		    0, &addr_proposal->if_index,
		    sizeof(addr_proposal->if_index));

		if (addr_proposal->privacy) {
			addr_proposal->next_timeout = 0;
			break; /* just let it expire */
		}

		tv.tv_sec = addr_proposal->next_timeout;
		tv.tv_usec = arc4random_uniform(1000000);
		addr_proposal->next_timeout *= 2;
		evtimer_add(&addr_proposal->timer, &tv);
		log_debug("%s: scheduling new timeout in %llds.%06ld",
		    __func__, tv.tv_sec, tv.tv_usec);
		break;
	case PROPOSAL_DUPLICATED:
		engine_imsg_compose_frontend(IMSG_CTL_SEND_SOLICITATION,
		    0, &addr_proposal->if_index,
		    sizeof(addr_proposal->if_index));
		log_debug("%s: address duplicated",
		    __func__);
		break;
	default:
		log_debug("%s: unhandled state: %s", __func__,
		    proposal_state_name[addr_proposal->state]);
	}
}

void
dfr_proposal_timeout(int fd, short events, void *arg)
{
	struct dfr_proposal	*dfr_proposal;
	struct imsg_proposal	 proposal;
	struct timeval		 tv;
	const char		*hbuf;

	dfr_proposal = (struct dfr_proposal *)arg;

	hbuf = sin6_to_str(&dfr_proposal->addr);
	log_debug("%s: iface %d: %s [%s]", __func__, dfr_proposal->if_index,
	    hbuf, proposal_state_name[dfr_proposal->state]);

	switch (dfr_proposal->state) {
	case PROPOSAL_NOT_CONFIGURED:
	case PROPOSAL_SENT:
		if (dfr_proposal->timeout_count++ < 6) {
			dfr_proposal->id = ++proposal_id;

			memset(&proposal, 0, sizeof(proposal));
			proposal.if_index = dfr_proposal->if_index;
			proposal.pid = getpid();
			proposal.id = dfr_proposal->id;
			memcpy(&proposal.addr, &dfr_proposal->addr,
			    sizeof(proposal.addr));

			proposal.rtm_addrs = RTA_GATEWAY;

			dfr_proposal->state = PROPOSAL_SENT;

			send_proposal(&proposal);

			tv.tv_sec = dfr_proposal->next_timeout;
			tv.tv_usec = arc4random_uniform(1000000);
			dfr_proposal->next_timeout *= 2;
			evtimer_add(&dfr_proposal->timer, &tv);
			log_debug("%s: scheduling new timeout in %llds.%06ld",
			    __func__, tv.tv_sec, tv.tv_usec);
		} else {
			log_debug("%s: giving up, no response to proposal",
			    __func__);
			free_dfr_proposal(dfr_proposal);
		}
		break;
	case PROPOSAL_CONFIGURED:
		log_debug("PROPOSAL_CONFIGURED timeout: id: %lld",
		    dfr_proposal->id);

		dfr_proposal->next_timeout = 1;
		dfr_proposal->timeout_count = 0;
		dfr_proposal->state = PROPOSAL_NEARLY_EXPIRED;

		tv.tv_sec = 0;
		tv.tv_usec = 0;
		evtimer_add(&dfr_proposal->timer, &tv);

		break;
	case PROPOSAL_NEARLY_EXPIRED:
		if (real_lifetime(&dfr_proposal->uptime,
		    dfr_proposal->router_lifetime) == 0) {
			free_dfr_proposal(dfr_proposal);
			log_debug("%s: removing dfr proposal", __func__);
			break;
		}
		engine_imsg_compose_frontend(IMSG_CTL_SEND_SOLICITATION,
		    0, &dfr_proposal->if_index,
		    sizeof(dfr_proposal->if_index));
		tv.tv_sec = dfr_proposal->next_timeout;
		tv.tv_usec = arc4random_uniform(1000000);
		dfr_proposal->next_timeout *= 2;
		evtimer_add(&dfr_proposal->timer, &tv);
		log_debug("%s: scheduling new timeout in %llds.%06ld",
		    __func__, tv.tv_sec, tv.tv_usec);
		break;
	default:
		log_debug("%s: unhandled state: %s", __func__,
		    proposal_state_name[dfr_proposal->state]);
	}
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

struct address_proposal*
find_address_proposal_by_id(struct slaacd_iface *iface, int64_t id)
{
	struct address_proposal	*addr_proposal;

	LIST_FOREACH (addr_proposal, &iface->addr_proposals, entries) {
		if (addr_proposal->id == id)
			return (addr_proposal);
	}

	return (NULL);
}

struct address_proposal*
find_address_proposal_by_addr(struct slaacd_iface *iface, struct sockaddr_in6
    *addr)
{
	struct address_proposal	*addr_proposal;

	LIST_FOREACH (addr_proposal, &iface->addr_proposals, entries) {
		if (memcmp(&addr_proposal->addr, addr, sizeof(*addr)) == 0)
			return (addr_proposal);
	}

	return (NULL);
}

struct dfr_proposal*
find_dfr_proposal_by_id(struct slaacd_iface *iface, int64_t id)
{
	struct dfr_proposal	*dfr_proposal;

	LIST_FOREACH (dfr_proposal, &iface->dfr_proposals, entries) {
		if (dfr_proposal->id == id)
			return (dfr_proposal);
	}

	return (NULL);
}

struct dfr_proposal*
find_dfr_proposal_by_gw(struct slaacd_iface *iface, struct sockaddr_in6
    *addr)
{
	struct dfr_proposal	*dfr_proposal;

	LIST_FOREACH (dfr_proposal, &iface->dfr_proposals, entries) {
		if (memcmp(&dfr_proposal->addr, addr, sizeof(*addr)) == 0)
			return (dfr_proposal);
	}

	return (NULL);
}


struct radv_prefix *
find_prefix(struct radv *ra, struct radv_prefix *prefix)
{
	struct radv_prefix	*result;


	LIST_FOREACH(result, &ra->prefixes, entries) {
		if (memcmp(&result->prefix, &prefix->prefix,
		    sizeof(prefix->prefix)) == 0 && result->prefix_len ==
		    prefix->prefix_len)
			return (result);
	}
	return (NULL);
}

uint32_t
real_lifetime(struct timespec *received_uptime, uint32_t ltime)
{
	struct timespec	 now, diff;
	int64_t		 remaining;

	if (clock_gettime(CLOCK_MONOTONIC, &now))
		fatal("clock_gettime");

	timespecsub(&now, received_uptime, &diff);

	remaining = ((int64_t)ltime) - diff.tv_sec;

	if (remaining < 0)
		remaining = 0;

	return (remaining);
}

void
merge_dad_couters(struct radv *old_ra, struct radv *new_ra)
{

	struct radv_prefix	*old_prefix, *new_prefix;

	LIST_FOREACH(old_prefix, &old_ra->prefixes, entries) {
		if (!old_prefix->dad_counter)
			continue;
		if ((new_prefix = find_prefix(new_ra, old_prefix)) != NULL)
			new_prefix->dad_counter = old_prefix->dad_counter;
	}
}
