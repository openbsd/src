/*	$OpenBSD: session.c,v 1.147 2004/04/25 18:21:18 henning Exp $ */

/*
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


#include <sys/param.h>
#include <sys/types.h>

#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pwd.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "mrt.h"
#include "session.h"

#define	PFD_LISTEN	0
#define	PFD_LISTEN6	1
#define PFD_PIPE_MAIN	2
#define PFD_PIPE_ROUTE	3
#define	PFD_SOCK_CTL	4
#define PFD_PEERS_START	5

void	session_sighdlr(int);
int	setup_listener(struct sockaddr *);
void	init_conf(struct bgpd_config *);
void	init_peer(struct peer *);
int	timer_due(time_t);
void	start_timer_holdtime(struct peer *);
void	start_timer_keepalive(struct peer *);
void	session_close_connection(struct peer *);
void	change_state(struct peer *, enum session_state, enum session_events);
int	session_setup_socket(struct peer *);
void	session_accept(int);
int	session_connect(struct peer *);
void	session_tcp_established(struct peer *);
void	session_open(struct peer *);
void	session_keepalive(struct peer *);
void	session_update(u_int32_t, void *, size_t);
void	session_notification(struct peer *, u_int8_t, u_int8_t, void *,
	    ssize_t);
int	session_dispatch_msg(struct pollfd *, struct peer *);
int	parse_header(struct peer *, u_char *, u_int16_t *, u_int8_t *);
int	parse_open(struct peer *);
int	parse_update(struct peer *);
int	parse_notification(struct peer *);
int	parse_keepalive(struct peer *);
int	parse_capabilities(struct peer *, u_char *, u_int16_t);
void	session_dispatch_imsg(struct imsgbuf *, int);
void	session_up(struct peer *);
void	session_down(struct peer *);

struct peer		*getpeerbyip(struct sockaddr *);
int			 session_match_mask(struct peer *, struct sockaddr *);
struct peer		*getpeerbyid(u_int32_t);
static struct sockaddr	*addr2sa(struct bgpd_addr *, u_int16_t);

struct bgpd_config	*conf, *nconf = NULL;
struct peer		*npeers;
volatile sig_atomic_t	 session_quit = 0;
int			 pending_reconf = 0;
int			 sock = -1;
int			 sock6 = -1;
int			 csock = -1;
struct imsgbuf		 ibuf_rde;
struct imsgbuf		 ibuf_main;

struct mrt_config_head	 mrt_l;

void
session_sighdlr(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		session_quit = 1;
		break;
	}
}

int
setup_listener(struct sockaddr *sa)
{
	int			 fd, opt;

	if (sa->sa_family != AF_INET && sa->sa_family != AF_INET6)
		fatal("king bula sez: unknown address family");

	if ((fd = socket(sa->sa_family, SOCK_STREAM, IPPROTO_TCP)) == -1) {
		log_warn("error setting up %s listener",
		    sa->sa_family == AF_INET ? "IPv4" : "IPv6");
		return (fd);
	}

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt)) == -1)
		fatal("setsockopt SO_REUSEPORT");
	if (setsockopt(fd, IPPROTO_TCP, TCP_MD5SIG, &opt, sizeof(opt)) == -1)
		fatal("setsockopt TCP_MD5SIG");

	if (bind(fd, sa, sa->sa_len)) {
		close(fd);
		fatal("bind");
	}

	session_socket_blockmode(fd, BM_NONBLOCK);

	if (listen(fd, MAX_BACKLOG)) {
		close(fd);
		fatal("listen");
	}

	return (fd);
}

int
session_main(struct bgpd_config *config, struct peer *cpeers,
    struct network_head *net_l, struct filter_head *rules,
    struct mrt_head *m_l, int pipe_m2s[2], int pipe_s2r[2])
{
	int			 nfds, i, j, timeout, idx_peers;
	pid_t			 pid;
	time_t			 nextaction;
	struct passwd		*pw;
	struct peer		*p, *peer_l[OPEN_MAX], *last, *next;
	struct network		*net;
	struct mrt		*m;
	struct mrt_config	*mrt;
	struct filter_rule	*r;
	struct pollfd		 pfd[OPEN_MAX];
	struct ctl_conn		*ctl_conn;
	short			 events;

	conf = config;
	peers = cpeers;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	/* control socket is outside chroot */
	if ((csock = control_init()) == -1)
		fatalx("control socket setup failed");

	if ((pw = getpwnam(BGPD_USER)) == NULL)
		fatal(NULL);

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("session engine");
	bgpd_process = PROC_SE;

	sock = setup_listener((struct sockaddr *)&conf->listen_addr);
	sock6 = setup_listener((struct sockaddr *)&conf->listen6_addr);

	if (pfkey_init() == -1)
		fatalx("pfkey setup failed");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		fatal("can't drop privileges");

	endpwent();

	signal(SIGTERM, session_sighdlr);
	signal(SIGINT, session_sighdlr);
	signal(SIGPIPE, SIG_IGN);
	log_info("session engine ready");
	close(pipe_m2s[0]);
	close(pipe_s2r[1]);
	init_conf(conf);
	imsg_init(&ibuf_rde, pipe_s2r[0]);
	imsg_init(&ibuf_main, pipe_m2s[1]);
	TAILQ_INIT(&ctl_conns);
	csock = control_listen();
	LIST_INIT(&mrt_l);

	/* filter rules are not used in the SE */
	while ((r = TAILQ_FIRST(rules)) != NULL) {
		TAILQ_REMOVE(rules, r, entries);
		free(r);
	}
	free(rules);

	/* network list is not used in the SE */
	while ((net = TAILQ_FIRST(net_l)) != NULL) {
		TAILQ_REMOVE(net_l, net, network_l);
		free(net);
	}

	/* main mrt list is not used in the SE */
	while ((m = LIST_FIRST(m_l)) != NULL) {
		LIST_REMOVE(m, list);
		free(m);
	}

	while (session_quit == 0) {
		bzero(&pfd, sizeof(pfd));
		pfd[PFD_LISTEN].fd = sock;
		pfd[PFD_LISTEN].events = POLLIN;
		pfd[PFD_LISTEN6].fd = sock6;
		pfd[PFD_LISTEN6].events = POLLIN;
		pfd[PFD_PIPE_MAIN].fd = ibuf_main.sock;
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		if (ibuf_main.w.queued > 0)
			pfd[PFD_PIPE_MAIN].events |= POLLOUT;
		pfd[PFD_PIPE_ROUTE].fd = ibuf_rde.sock;
		pfd[PFD_PIPE_ROUTE].events = POLLIN;
		if (ibuf_rde.w.queued > 0)
			pfd[PFD_PIPE_ROUTE].events |= POLLOUT;
		pfd[PFD_SOCK_CTL].fd = csock;
		pfd[PFD_SOCK_CTL].events = POLLIN;

		nextaction = time(NULL) + 240;	/* loop every 240s at least */
		i = PFD_PEERS_START;

		last = NULL;
		for (p = peers; p != NULL; p = next) {
			next = p->next;
			if (!pending_reconf) {
				/* needs init? */
				if (p->state == STATE_NONE)
					init_peer(p);

				/* reinit due? */
				if (p->conf.reconf_action == RECONF_REINIT) {
					bgp_fsm(p, EVNT_STOP);
					p->IdleHoldTimer = time(NULL);
				}

				/* deletion due? */
				if (p->conf.reconf_action == RECONF_DELETE) {
					bgp_fsm(p, EVNT_STOP);
					log_peer_warnx(&p->conf, "removed");
					if (last != NULL)
						last->next = next;
					else
						peers = next;
					free(p);
					continue;
				}
				p->conf.reconf_action = RECONF_NONE;
			}
			last = p;

			/* check timers */
			if (timer_due(p->HoldTimer))
				bgp_fsm(p, EVNT_TIMER_HOLDTIME);
			if (timer_due(p->ConnectRetryTimer))
				bgp_fsm(p, EVNT_TIMER_CONNRETRY);
			if (timer_due(p->KeepaliveTimer))
				bgp_fsm(p, EVNT_TIMER_KEEPALIVE);
			if (timer_due(p->IdleHoldTimer))
				bgp_fsm(p, EVNT_START);
			if (timer_due(p->IdleHoldResetTimer)) {
				p->IdleHoldTime /= 2;
				if (p->IdleHoldTime <=
				    INTERVAL_IDLE_HOLD_INITIAL) {
					p->IdleHoldTime =
					    INTERVAL_IDLE_HOLD_INITIAL;
					p->IdleHoldResetTimer = 0;
				} else
					p->IdleHoldResetTimer =
					    time(NULL) + p->IdleHoldTime;
			}

			/* set nextaction to the first expiring timer */
			if (p->ConnectRetryTimer &&
			    p->ConnectRetryTimer < nextaction)
				nextaction = p->ConnectRetryTimer;
			if (p->HoldTimer && p->HoldTimer < nextaction)
				nextaction = p->HoldTimer;
			if (p->KeepaliveTimer && p->KeepaliveTimer < nextaction)
				nextaction = p->KeepaliveTimer;
			if (p->IdleHoldTimer && p->IdleHoldTimer < nextaction)
				nextaction = p->IdleHoldTimer;
			if (p->IdleHoldResetTimer &&
			    p->IdleHoldResetTimer < nextaction)
				nextaction = p->IdleHoldResetTimer;

			/* are we waiting for a write? */
			events = POLLIN;
			if (p->wbuf.queued > 0 || p->state == STATE_CONNECT)
				events |= POLLOUT;

			/* poll events */
			if (p->sock != -1 && events != 0) {
				pfd[i].fd = p->sock;
				pfd[i].events = events;
				peer_l[i] = p;
				i++;
			}
		}

		idx_peers = i;

		TAILQ_FOREACH(ctl_conn, &ctl_conns, entries) {
			pfd[i].fd = ctl_conn->ibuf.sock;
			pfd[i].events = POLLIN;
			if (ctl_conn->ibuf.w.queued > 0)
				pfd[i].events |= POLLOUT;
			i++;
		}

		timeout = nextaction - time(NULL);
		if (timeout < 0)
			timeout = 0;
		if ((nfds = poll(pfd, i, timeout * 1000)) == -1)
			if (errno != EINTR)
				fatal("poll error");

		if (nfds > 0 && pfd[PFD_LISTEN].revents & POLLIN) {
			nfds--;
			session_accept(sock);
		}

		if (nfds > 0 && pfd[PFD_LISTEN6].revents & POLLIN) {
			nfds--;
			session_accept(sock6);
		}

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & POLLOUT)
			if (msgbuf_write(&ibuf_main.w) < 0)
				fatal("pipe write error");

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & POLLIN) {
			nfds--;
			session_dispatch_imsg(&ibuf_main, PFD_PIPE_MAIN);
		}

		if (nfds > 0 && pfd[PFD_PIPE_ROUTE].revents & POLLOUT)
			if (msgbuf_write(&ibuf_rde.w) < 0)
				fatal("pipe write error");

		if (nfds > 0 && pfd[PFD_PIPE_ROUTE].revents & POLLIN) {
			nfds--;
			session_dispatch_imsg(&ibuf_rde, PFD_PIPE_ROUTE);
		}

		if (nfds > 0 && pfd[PFD_SOCK_CTL].revents & POLLIN) {
			nfds--;
			control_accept(csock);
		}

		for (j = PFD_PEERS_START; nfds > 0 && j < idx_peers; j++)
			nfds -= session_dispatch_msg(&pfd[j], peer_l[j]);

		for (; nfds > 0 && j < i; j++)
			nfds -= control_dispatch_msg(&pfd[j], j);
	}

	while ((p = peers) != NULL) {
		peers = p->next;
		bgp_fsm(p, EVNT_STOP);
		free(p);
	}

	while ((mrt = LIST_FIRST(&mrt_l)) != NULL) {
		LIST_REMOVE(mrt, list);
		free(mrt);
	}

	msgbuf_write(&ibuf_rde.w);
	msgbuf_clear(&ibuf_rde.w);
	msgbuf_write(&ibuf_main.w);
	msgbuf_clear(&ibuf_main.w);

	control_shutdown();
	log_info("session engine exiting");
	_exit(0);
}

void
init_conf(struct bgpd_config *c)
{
	if (!c->holdtime)
		c->holdtime = INTERVAL_HOLD;
}

void
init_peer(struct peer *p)
{
	p->sock = p->wbuf.sock = -1;
	p->capa.announce = p->conf.capabilities;

	change_state(p, STATE_IDLE, EVNT_NONE);
	p->IdleHoldTimer = time(NULL);	/* start ASAP */
}

void
bgp_fsm(struct peer *peer, enum session_events event)
{
	switch (peer->state) {
	case STATE_NONE:
		/* nothing */
		break;
	case STATE_IDLE:
		switch (event) {
		case EVNT_START:
			peer->HoldTimer = 0;
			peer->KeepaliveTimer = 0;
			peer->IdleHoldTimer = 0;

			/* allocate read buffer */
			peer->rbuf = calloc(1, sizeof(struct buf_read));
			if (peer->rbuf == NULL)
				fatal(NULL);
			peer->rbuf->wpos = 0;

			/* init write buffer */
			msgbuf_init(&peer->wbuf);

			/* init pfkey */
			if (pfkey_auth_establish(peer) == -1) {
				log_peer_warnx(&peer->conf,
				    "pfkey_auth setup failed");
				return;
			}

			if (peer->conf.passive || peer->conf.template) {
				change_state(peer, STATE_ACTIVE, event);
				peer->ConnectRetryTimer = 0;
			} else {
				change_state(peer, STATE_CONNECT, event);
				session_connect(peer);
				peer->ConnectRetryTimer =
				    time(NULL) + INTERVAL_CONNECTRETRY;
			}
			break;
		default:
			/* ignore */
			break;
		}
		break;
	case STATE_CONNECT:
		switch (event) {
		case EVNT_START:
			/* ignore */
			break;
		case EVNT_CON_OPEN:
			session_tcp_established(peer);
			session_open(peer);
			peer->ConnectRetryTimer = 0;
			change_state(peer, STATE_OPENSENT, event);
			break;
		case EVNT_CON_OPENFAIL:
			peer->ConnectRetryTimer =
			    time(NULL) + INTERVAL_CONNECTRETRY;
			session_close_connection(peer);
			change_state(peer, STATE_ACTIVE, event);
			break;
		case EVNT_TIMER_CONNRETRY:
			peer->ConnectRetryTimer =
			    time(NULL) + INTERVAL_CONNECTRETRY;
			session_connect(peer);
		break;
		default:
			change_state(peer, STATE_IDLE, event);
			break;
		}
		break;
	case STATE_ACTIVE:
		switch (event) {
		case EVNT_START:
			/* ignore */
			break;
		case EVNT_CON_OPEN:
			session_tcp_established(peer);
			session_open(peer);
			peer->ConnectRetryTimer = 0;
			peer->holdtime = INTERVAL_HOLD_INITIAL;
			start_timer_holdtime(peer);
			change_state(peer, STATE_OPENSENT, event);
			break;
		case EVNT_CON_OPENFAIL:
			peer->ConnectRetryTimer =
			    time(NULL) + INTERVAL_CONNECTRETRY;
			session_close_connection(peer);
			change_state(peer, STATE_ACTIVE, event);
			break;
		case EVNT_TIMER_CONNRETRY:
			peer->ConnectRetryTimer =
			    time(NULL) + peer->holdtime;
			change_state(peer, STATE_CONNECT, event);
			session_connect(peer);
			break;
		default:
			change_state(peer, STATE_IDLE, event);
			break;
		}
		break;
	case STATE_OPENSENT:
		switch (event) {
		case EVNT_START:
			/* ignore */
			break;
		case EVNT_STOP:
			session_notification(peer, ERR_CEASE, 0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_CON_CLOSED:
			session_close_connection(peer);
			peer->ConnectRetryTimer =
			    time(NULL) + INTERVAL_CONNECTRETRY;
			change_state(peer, STATE_ACTIVE, event);
			break;
		case EVNT_CON_FATAL:
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_TIMER_HOLDTIME:
			session_notification(peer, ERR_HOLDTIMEREXPIRED,
			    0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_RCVD_OPEN:
			/* parse_open calls change_state itself on failure */
			if (parse_open(peer))
				break;
			session_keepalive(peer);
			change_state(peer, STATE_OPENCONFIRM, event);
			break;
		case EVNT_RCVD_NOTIFICATION:
			if (parse_notification(peer)) {
				change_state(peer, STATE_IDLE, event);
				/* don't punish, capa negotiation */
				peer->IdleHoldTimer = time(NULL);
				peer->IdleHoldTime /= 2;
			} else
				change_state(peer, STATE_IDLE, event);
			break;
		default:
			session_notification(peer, ERR_FSM, 0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		}
		break;
	case STATE_OPENCONFIRM:
		switch (event) {
		case EVNT_START:
			/* ignore */
			break;
		case EVNT_STOP:
			session_notification(peer, ERR_CEASE, 0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_CON_CLOSED:
		case EVNT_CON_FATAL:
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_TIMER_HOLDTIME:
			session_notification(peer, ERR_HOLDTIMEREXPIRED,
			    0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_TIMER_KEEPALIVE:
			session_keepalive(peer);
			break;
		case EVNT_RCVD_KEEPALIVE:
			start_timer_holdtime(peer);
			change_state(peer, STATE_ESTABLISHED, event);
			break;
		case EVNT_RCVD_NOTIFICATION:
			parse_notification(peer);
			change_state(peer, STATE_IDLE, event);
			break;
		default:
			session_notification(peer, ERR_FSM, 0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		}
		break;
	case STATE_ESTABLISHED:
		switch (event) {
		case EVNT_START:
			/* ignore */
			break;
		case EVNT_STOP:
			session_notification(peer, ERR_CEASE, 0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_CON_CLOSED:
		case EVNT_CON_FATAL:
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_TIMER_HOLDTIME:
			session_notification(peer, ERR_HOLDTIMEREXPIRED,
			    0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_TIMER_KEEPALIVE:
			session_keepalive(peer);
			break;
		case EVNT_RCVD_KEEPALIVE:
			start_timer_holdtime(peer);
			break;
		case EVNT_RCVD_UPDATE:
			start_timer_holdtime(peer);
			if (parse_update(peer))
				change_state(peer, STATE_IDLE, event);
			else
				start_timer_holdtime(peer);
			break;
		case EVNT_RCVD_NOTIFICATION:
			parse_notification(peer);
			change_state(peer, STATE_IDLE, event);
			break;
		default:
			session_notification(peer, ERR_FSM, 0, NULL, 0);
			change_state(peer, STATE_IDLE, event);
			break;
		}
		break;
	}
}

int
timer_due(time_t timer)
{
	if (timer > 0 && timer <= time(NULL))
		return (1);
	return (0);
}

void
start_timer_holdtime(struct peer *peer)
{
	if (peer->holdtime > 0)
		peer->HoldTimer = time(NULL) + peer->holdtime;
	else
		peer->HoldTimer = 0;
}

void
start_timer_keepalive(struct peer *peer)
{
	if (peer->holdtime > 0)
		peer->KeepaliveTimer = time(NULL) + peer->holdtime / 3;
	else
		peer->KeepaliveTimer = 0;
}

void
session_close_connection(struct peer *peer)
{
	if (peer->sock != -1) {
		shutdown(peer->sock, SHUT_RDWR);
		close(peer->sock);
	}
	peer->sock = peer->wbuf.sock = -1;
}

void
change_state(struct peer *peer, enum session_state state,
    enum session_events event)
{
	struct mrt_config	*mrt;

	switch (state) {
	case STATE_IDLE:
		/*
		 * try to write out what's buffered (maybe a notification),
		 * don't bother if it fails
		 */
		if (peer->state >= STATE_OPENSENT && peer->wbuf.queued)
			msgbuf_write(&peer->wbuf);

		/*
		 * we must start the timer for the next EVNT_START
		 * if we are coming here due to an error and the
		 * session was not established successfully before, the
		 * starttimerinterval needs to be exponentially increased
		 */
		if (peer->IdleHoldTime == 0)
			peer->IdleHoldTime = INTERVAL_IDLE_HOLD_INITIAL;
		peer->holdtime = INTERVAL_HOLD_INITIAL;
		peer->ConnectRetryTimer = 0;
		peer->KeepaliveTimer = 0;
		peer->HoldTimer = 0;
		peer->IdleHoldResetTimer = 0;
		session_close_connection(peer);
		msgbuf_clear(&peer->wbuf);
		free(peer->rbuf);
		peer->rbuf = NULL;
		pfkey_auth_remove(peer);
		if (peer->state == STATE_ESTABLISHED)
			session_down(peer);
		if (event != EVNT_STOP && !peer->conf.cloned) {
			peer->IdleHoldTimer = time(NULL) + peer->IdleHoldTime;
			if (event != EVNT_NONE &&
			    peer->IdleHoldTime < MAX_IDLE_HOLD/2)
				peer->IdleHoldTime *= 2;
		}
		if (peer->state != STATE_NONE && peer->conf.cloned)
			peer->conf.reconf_action = RECONF_DELETE;
		break;
	case STATE_CONNECT:
		break;
	case STATE_ACTIVE:
		break;
	case STATE_OPENSENT:
		break;
	case STATE_OPENCONFIRM:
		break;
	case STATE_ESTABLISHED:
		if (peer->IdleHoldTime > INTERVAL_IDLE_HOLD_INITIAL)
			peer->IdleHoldResetTimer =
			    time(NULL) + peer->IdleHoldTime;
		session_up(peer);
		break;
	default:		/* something seriously fucked */
		break;
	}

	log_statechange(peer, state, event);
	LIST_FOREACH(mrt, &mrt_l, list) {
		if (mrt->type != MRT_ALL_IN && mrt->type != MRT_ALL_OUT)
			continue;
		if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
		    mrt->peer_id == peer->conf.id ||
		    mrt->group_id == peer->conf.groupid)
			mrt_dump_state(mrt, peer->state, state,
			    &peer->conf, conf);
	}
	peer->state = state;
}

void
session_accept(int listenfd)
{
	int			 connfd;
	int			 opt;
	socklen_t		 len;
	struct sockaddr_storage	 cliaddr;
	struct peer		*p = NULL;

	len = sizeof(cliaddr);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&cliaddr, &len)) == -1) {
		if (errno == EWOULDBLOCK || errno == EINTR)
			return;
		else
			log_warn("accept");
	}

	p = getpeerbyip((struct sockaddr *)&cliaddr);

	if (p != NULL &&
	    (p->state == STATE_CONNECT || p->state == STATE_ACTIVE)) {
		if (p->sock != -1) {
			if (p->state == STATE_CONNECT)
				session_close_connection(p);
			else {
				shutdown(connfd, SHUT_RDWR);
				close(connfd);
				return;
			}
		}
		if (p->conf.tcp_md5_key[0]) {
			len = sizeof(opt);
			if (getsockopt(connfd, IPPROTO_TCP, TCP_MD5SIG,
			    &opt, &len) == -1)
				fatal("getsockopt TCP_MD5SIG");
			if (!opt) {	/* non-md5'd connection! */
				log_peer_warnx(&p->conf,
				    "connection attempt without md5 signature");
				shutdown(connfd, SHUT_RDWR);
				close(connfd);
				return;
			}
		}
		p->sock = p->wbuf.sock = connfd;
		if (session_setup_socket(p)) {
			shutdown(connfd, SHUT_RDWR);
			close(connfd);
			return;
		}
		session_socket_blockmode(connfd, BM_NONBLOCK);
		bgp_fsm(p, EVNT_CON_OPEN);
	} else {
		log_conn_attempt(p, (struct sockaddr *)&cliaddr);
		shutdown(connfd, SHUT_RDWR);
		close(connfd);
	}
}

int
session_connect(struct peer *peer)
{
	int			 n, opt = 1;
	struct sockaddr		*sa;

	/*
	 * we do not need the overcomplicated collision detection rfc1771
	 * describes; we simply make sure there is only ever one concurrent
	 * tcp connection per peer.
	 */
	if (peer->sock != -1)
		return (-1);

	if ((peer->sock = socket(peer->conf.remote_addr.af, SOCK_STREAM,
	    IPPROTO_TCP)) == -1) {
		log_peer_warn(&peer->conf, "session_connect socket");
		bgp_fsm(peer, EVNT_CON_OPENFAIL);
		return (-1);
	}

	if (peer->conf.tcp_md5_key[0])
		if (setsockopt(peer->sock, IPPROTO_TCP, TCP_MD5SIG,
		    &opt, sizeof(opt)) == -1) {
			log_peer_warn(&peer->conf, "setsockopt md5sig");
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (-1);
		}

	peer->wbuf.sock = peer->sock;

	/* if update source is set we need to bind() */
	if (peer->conf.local_addr.af) {
		sa = addr2sa(&peer->conf.local_addr, 0);
		if (bind(peer->sock, sa, sa->sa_len) == -1) {
			log_peer_warn(&peer->conf, "session_connect bind");
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (-1);
		}
	}

	if (session_setup_socket(peer)) {
		bgp_fsm(peer, EVNT_CON_OPENFAIL);
		return (-1);
	}

	session_socket_blockmode(peer->sock, BM_NONBLOCK);

	sa = addr2sa(&peer->conf.remote_addr, BGP_PORT);
	if ((n = connect(peer->sock, sa, sa->sa_len)) == -1)
		if (errno != EINPROGRESS) {
			log_peer_warn(&peer->conf, "connect");
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (-1);
		}

	if (n == 0)
		bgp_fsm(peer, EVNT_CON_OPEN);

	return (0);
}

int
session_setup_socket(struct peer *p)
{
	int	ttl = p->conf.distance;
	int	pre = IPTOS_PREC_INTERNETCONTROL;
	int	nodelay = 1;

	if (p->conf.ebgp && p->sa_remote.ss_family == AF_INET)
		/* set TTL to foreign router's distance - 1=direct n=multihop */
		if (setsockopt(p->sock, IPPROTO_IP, IP_TTL, &ttl,
		    sizeof(ttl)) == -1) {
			log_peer_warn(&p->conf,
			    "session_setup_socket setsockopt TTL");
			return (-1);
		}

	if (p->conf.ebgp && p->sa_remote.ss_family == AF_INET6)
		/* set hoplimit to foreign router's distance */
		if (setsockopt(p->sock, IPPROTO_IPV6, IPV6_HOPLIMIT, &ttl,
		    sizeof(ttl)) == -1) {
			log_peer_warn(&p->conf,
			    "session_setup_socket setsockopt hoplimit");
			return (-1);
		}

	/* set TCP_NODELAY */
	if (setsockopt(p->sock, IPPROTO_TCP, TCP_NODELAY, &nodelay,
	    sizeof(nodelay)) == -1) {
		log_peer_warn(&p->conf,
		    "session_setup_socket setsockopt TCP_NODELAY");
		return (-1);
	}

	/* set precedence, see rfc1771 appendix 5 */
	if (p->sa_remote.ss_family == AF_INET &&
	    setsockopt(p->sock, IPPROTO_IP, IP_TOS, &pre, sizeof(pre)) == -1) {
		log_peer_warn(&p->conf,
		    "session_setup_socket setsockopt TOS");
		return (-1);
	}

	return (0);
}

void
session_socket_blockmode(int fd, enum blockmodes bm)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		fatal("fnctl F_GETFL");

	if (bm == BM_NONBLOCK)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		fatal("fnctl F_SETFL");
}

void
session_tcp_established(struct peer *peer)
{
	socklen_t	len;

	len = sizeof(peer->sa_local);
	if (getsockname(peer->sock, (struct sockaddr *)&peer->sa_local,
	    &len) == -1)
		log_warn("getsockname");
	len = sizeof(peer->sa_remote);
	if (getpeername(peer->sock, (struct sockaddr *)&peer->sa_remote,
	    &len) == -1)
		log_warn("getpeername");
}

void
session_open(struct peer *p)
{
	struct msg_open		 msg;
	struct buf		*buf;
	struct mrt_config	*mrt;
	u_int16_t		 len;
	int			 errs = 0, n;
	u_int8_t		 op_type, op_len = 0, optparamlen = 0;

	if (p->capa.announce) {
		/* multiprotocol extensions, RFC 2858 */
		/* route refresh, RFC 2918 */

		if (op_len > 0)
			optparamlen = sizeof(op_type) + sizeof(op_len) + op_len;
	}

	len = MSGSIZE_OPEN_MIN + optparamlen;

	memset(&msg.header.marker, 0xff, sizeof(msg.header.marker));
	msg.header.len = htons(len);
	msg.header.type = OPEN;
	msg.version = 4;
	msg.myas = htons(conf->as);
	if (p->conf.holdtime)
		msg.holdtime = htons(p->conf.holdtime);
	else
		msg.holdtime = htons(conf->holdtime);
	msg.bgpid = conf->bgpid;	/* is already in network byte order */
	msg.optparamlen = optparamlen;

	if ((buf = buf_open(len)) == NULL) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}
	errs += buf_add(buf, &msg.header.marker, sizeof(msg.header.marker));
	errs += buf_add(buf, &msg.header.len, sizeof(msg.header.len));
	errs += buf_add(buf, &msg.header.type, sizeof(msg.header.type));
	errs += buf_add(buf, &msg.version, sizeof(msg.version));
	errs += buf_add(buf, &msg.myas, sizeof(msg.myas));
	errs += buf_add(buf, &msg.holdtime, sizeof(msg.holdtime));
	errs += buf_add(buf, &msg.bgpid, sizeof(msg.bgpid));
	errs += buf_add(buf, &msg.optparamlen, sizeof(msg.optparamlen));

	if (p->capa.announce && optparamlen) {
		op_type = OPT_PARAM_CAPABILITIES;
		errs += buf_add(buf, &op_type, sizeof(op_type));
		errs += buf_add(buf, &op_len, sizeof(op_len));

		/* multiprotocol extensions, RFC 2858 */	
		/* route refresh, RFC 2918 */
	}

	if (errs == 0) {
		LIST_FOREACH(mrt, &mrt_l, list) {
			if (mrt->type != MRT_ALL_OUT)
				continue;
			if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
			    mrt->peer_id == p->conf.id ||
			    mrt->group_id == p->conf.groupid)
				mrt_dump_bgp_msg(mrt, buf->buf, len,
				    &p->conf, conf);
		}

		if ((n = buf_close(&p->wbuf, buf)) == -1) {
			log_peer_warn(&p->conf, "session_open buf_close");
			buf_free(buf);
			bgp_fsm(p, EVNT_CON_FATAL);
			return;
		}
	} else {
		buf_free(buf);
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	p->stats.msg_sent_open++;
}

void
session_keepalive(struct peer *peer)
{
	struct msg_header	 msg;
	struct buf		*buf;
	struct mrt_config	*mrt;
	ssize_t			 len;
	int			 errs = 0, n;

	len = MSGSIZE_KEEPALIVE;

	memset(&msg.marker, 0xff, sizeof(msg.marker));
	msg.len = htons(len);
	msg.type = KEEPALIVE;

	if ((buf = buf_open(len)) == NULL) {
		bgp_fsm(peer, EVNT_CON_FATAL);
		return;
	}
	errs += buf_add(buf, &msg.marker, sizeof(msg.marker));
	errs += buf_add(buf, &msg.len, sizeof(msg.len));
	errs += buf_add(buf, &msg.type, sizeof(msg.type));

	if (errs > 0) {
		buf_free(buf);
		bgp_fsm(peer, EVNT_CON_FATAL);
		return;
	}

	LIST_FOREACH(mrt, &mrt_l, list) {
		if (mrt->type != MRT_ALL_OUT)
			continue;
		if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
		    mrt->peer_id == peer->conf.id ||
		    mrt->group_id == peer->conf.groupid)
			mrt_dump_bgp_msg(mrt, buf->buf, len, &peer->conf, conf);
	}

	if ((n = buf_close(&peer->wbuf, buf)) == -1) {
		log_peer_warn(&peer->conf, "session_keepalive buf_close");
		buf_free(buf);
		bgp_fsm(peer, EVNT_CON_FATAL);
		return;
	}

	start_timer_keepalive(peer);
	peer->stats.msg_sent_keepalive++;
}

void
session_update(u_int32_t peerid, void *data, size_t datalen)
{
	struct peer		*p;
	struct msg_header	 msg;
	struct buf		*buf;
	struct mrt_config	*mrt;
	ssize_t			 len;
	int			 errs = 0, n;

	if ((p = getpeerbyid(peerid)) == NULL) {
		log_warnx("no such peer: id=%u", peerid);
		return;
	}

	len = MSGSIZE_HEADER + datalen;

	memset(&msg.marker, 0xff, sizeof(msg.marker));
	msg.len = htons(len);
	msg.type = UPDATE;

	if ((buf = buf_open(len)) == NULL) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}
	errs += buf_add(buf, &msg.marker, sizeof(msg.marker));
	errs += buf_add(buf, &msg.len, sizeof(msg.len));
	errs += buf_add(buf, &msg.type, sizeof(msg.type));
	errs += buf_add(buf, data, datalen);

	if (errs > 0) {
		buf_free(buf);
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	LIST_FOREACH(mrt, &mrt_l, list) {
		if (mrt->type != MRT_ALL_OUT && mrt->type != MRT_UPDATE_OUT)
			continue;
		if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
		    mrt->peer_id == p->conf.id ||
		    mrt->group_id == p->conf.groupid)
			mrt_dump_bgp_msg(mrt, buf->buf, len, &p->conf, conf);
	}

	if ((n = buf_close(&p->wbuf, buf)) == -1) {
		log_peer_warn(&p->conf, "session_update: buf_close");
		buf_free(buf);
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	start_timer_keepalive(p);
	p->stats.msg_sent_update++;
}

void
session_notification(struct peer *peer, u_int8_t errcode, u_int8_t subcode,
    void *data, ssize_t datalen)
{
	struct msg_header	 msg;
	struct buf		*buf;
	struct mrt_config	*mrt;
	ssize_t			 len;
	int			 errs = 0, n;

	len = MSGSIZE_NOTIFICATION_MIN + datalen;

	memset(&msg.marker, 0xff, sizeof(msg.marker));
	msg.len = htons(len);
	msg.type = NOTIFICATION;

	if ((buf = buf_open(len)) == NULL) {
		bgp_fsm(peer, EVNT_CON_FATAL);
		return;
	}
	errs += buf_add(buf, &msg.marker, sizeof(msg.marker));
	errs += buf_add(buf, &msg.len, sizeof(msg.len));
	errs += buf_add(buf, &msg.type, sizeof(msg.type));
	errs += buf_add(buf, &errcode, sizeof(errcode));
	errs += buf_add(buf, &subcode, sizeof(subcode));

	if (datalen > 0)
		errs += buf_add(buf, data, datalen);

	if (errs > 0) {
		buf_free(buf);
		bgp_fsm(peer, EVNT_CON_FATAL);
		return;
	}

	LIST_FOREACH(mrt, &mrt_l, list) {
		if (mrt->type != MRT_ALL_OUT)
			continue;
		if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
		    mrt->peer_id == peer->conf.id ||
		    mrt->group_id == peer->conf.groupid)
			mrt_dump_bgp_msg(mrt, buf->buf, len, &peer->conf, conf);
	}

	if ((n = buf_close(&peer->wbuf, buf)) == -1) {
		log_peer_warn(&peer->conf, "session_notification: buf_close");
		buf_free(buf);
		bgp_fsm(peer, EVNT_CON_FATAL);
		return;
	}
	peer->stats.msg_sent_notification++;
}

int
session_dispatch_msg(struct pollfd *pfd, struct peer *p)
{
	ssize_t		n, rpos, av, left;
	socklen_t	len;
	int		error;
	u_int16_t	msglen;
	u_int8_t	msgtype;

	if (p->state == STATE_CONNECT) {
		if (pfd->revents & POLLOUT) {
			if (pfd->revents & POLLIN) {
				/* error occurred */
				len = sizeof(error);
				if (getsockopt(pfd->fd, SOL_SOCKET, SO_ERROR,
				    &error, &len) == -1 || error) {
					if (error)
						errno = error;
					log_peer_warn(&p->conf, "socket error");
					bgp_fsm(p, EVNT_CON_OPENFAIL);
					return (1);
				}
			}
			bgp_fsm(p, EVNT_CON_OPEN);
			return (1);
		}
		if (pfd->revents & POLLHUP) {
			bgp_fsm(p, EVNT_CON_OPENFAIL);
			return (1);
		}
		if (pfd->revents & (POLLERR|POLLNVAL)) {
			bgp_fsm(p, EVNT_CON_FATAL);
			return (1);
		}
		return (0);
	}

	if (pfd->revents & POLLHUP) {
		bgp_fsm(p, EVNT_CON_CLOSED);
		return (1);
	}
	if (pfd->revents & (POLLERR|POLLNVAL)) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return (1);
	}

	if (pfd->revents & POLLOUT && p->wbuf.queued) {
		if ((error = msgbuf_write(&p->wbuf)) < 0) {
			if (error == -2)
				log_peer_warnx(&p->conf, "Connection closed");
			else
				log_peer_warn(&p->conf, "write error");
			bgp_fsm(p, EVNT_CON_FATAL);
			return (1);
		}
		if (!(pfd->revents & POLLIN))
			return (1);
	}

	if (pfd->revents & POLLIN) {
		if ((n = read(p->sock, p->rbuf->buf + p->rbuf->wpos,
			    sizeof(p->rbuf->buf) - p->rbuf->wpos)) ==
			    -1) {
				if (errno != EINTR && errno != EAGAIN) {
					log_peer_warn(&p->conf, "read error");
					bgp_fsm(p, EVNT_CON_FATAL);
				}
				return (1);
			}
			if (n == 0) {	/* connection closed */
				bgp_fsm(p, EVNT_CON_CLOSED);
				return (1);
			}

			rpos = 0;
			av = p->rbuf->wpos + n;
			p->stats.last_read = time(NULL);

			/*
			 * session might drop to IDLE -> buffers deallocated
			 * we MUST check rbuf != NULL before use
			 */
			for (;;) {
				if (rpos + MSGSIZE_HEADER > av)
					break;
				if (p->rbuf == NULL)
					break;
				if (parse_header(p, p->rbuf->buf + rpos,
				    &msglen, &msgtype) == -1)
					return (0);
				if (rpos + msglen > av)
					break;
				p->rbuf->rptr = p->rbuf->buf + rpos;

				switch (msgtype) {
				case OPEN:
					bgp_fsm(p, EVNT_RCVD_OPEN);
					p->stats.msg_rcvd_open++;
					break;
				case UPDATE:
					bgp_fsm(p, EVNT_RCVD_UPDATE);
					p->stats.msg_rcvd_update++;
					break;
				case NOTIFICATION:
					bgp_fsm(p, EVNT_RCVD_NOTIFICATION);
					p->stats.msg_rcvd_notification++;
					break;
				case KEEPALIVE:
					bgp_fsm(p, EVNT_RCVD_KEEPALIVE);
					p->stats.msg_rcvd_keepalive++;
					break;
				default:	/* cannot happen */
					session_notification(p, ERR_HEADER,
					    ERR_HDR_TYPE, &msgtype, 1);
					log_warnx("received message with "
					    "unknown type %u", msgtype);
				}
				rpos += msglen;
			}
			if (p->rbuf == NULL)
				return (1);

			if (rpos < av) {
				left = av - rpos;
				memcpy(&p->rbuf->buf, p->rbuf->buf + rpos,
				    left);
				p->rbuf->wpos = left;
			} else
				p->rbuf->wpos = 0;

		return (1);
	}
	return (0);
}

int
parse_header(struct peer *peer, u_char *data, u_int16_t *len, u_int8_t *type)
{
	struct mrt_config	*mrt;
	u_char			*p;
	u_char			 one = 0xff;
	int			 i;
	u_int16_t		 olen;

	/* caller MUST make sure we are getting 19 bytes! */
	p = data;
	for (i = 0; i < 16; i++) {
		if (memcmp(p, &one, 1)) {
			log_peer_warnx(&peer->conf, "sync error");
			session_notification(peer, ERR_HEADER, ERR_HDR_SYNC,
			    NULL, 0);
			bgp_fsm(peer, EVNT_CON_FATAL);
			return (-1);
		}
		p++;
	}
	memcpy(&olen, p, 2);
	*len = ntohs(olen);
	p += 2;
	memcpy(type, p, 1);

	if (*len < MSGSIZE_HEADER || *len > MAX_PKTSIZE) {
		log_peer_warnx(&peer->conf,
		    "received message: illegal length: %u byte", *len);
		session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
		    &olen, sizeof(olen));
		return (-1);
	}

	switch (*type) {
	case OPEN:
		if (*len < MSGSIZE_OPEN_MIN) {
			log_peer_warnx(&peer->conf,
			    "received OPEN: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			return (-1);
		}
		break;
	case NOTIFICATION:
		if (*len < MSGSIZE_NOTIFICATION_MIN) {
			log_peer_warnx(&peer->conf,
			    "received NOTIFICATION: illegal len: %u byte",
			    *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			return (-1);
		}
		break;
	case UPDATE:
		if (*len < MSGSIZE_UPDATE_MIN) {
			log_peer_warnx(&peer->conf,
			    "received UPDATE: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			return (-1);
		}
		break;
	case KEEPALIVE:
		if (*len != MSGSIZE_KEEPALIVE) {
			log_peer_warnx(&peer->conf,
			    "received KEEPALIVE: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			return (-1);
		}
		break;
	default:
		log_peer_warnx(&peer->conf,
		    "received msg with unknown type %u", *type);
		session_notification(peer, ERR_HEADER, ERR_HDR_TYPE,
		    type, 1);
		return (-1);
	}
	LIST_FOREACH(mrt, &mrt_l, list) {
		if (mrt->type != MRT_ALL_IN && (mrt->type != MRT_UPDATE_IN ||
		    *type != UPDATE))
			continue;
		if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
		    mrt->peer_id == peer->conf.id ||
		    mrt->group_id == peer->conf.groupid)
			mrt_dump_bgp_msg(mrt, data, *len, &peer->conf, conf);
	}
	return (0);
}

int
parse_open(struct peer *peer)
{
	u_char		*p, *op_val;
	u_int8_t	 version, rversion;
	u_int16_t	 as, msglen;
	u_int16_t	 holdtime, oholdtime, myholdtime;
	u_int32_t	 bgpid;
	u_int8_t	 optparamlen, plen;
	u_int8_t	 op_type, op_len;

	p = peer->rbuf->rptr;
	p += MSGSIZE_HEADER_MARKER;
	memcpy(&msglen, p, sizeof(msglen));
	msglen = ntohs(msglen);

	p = peer->rbuf->rptr;
	p += MSGSIZE_HEADER;	/* header is already checked */

	memcpy(&version, p, sizeof(version));
	p += sizeof(version);

	if (version != BGP_VERSION) {
		log_peer_warnx(&peer->conf,
		    "peer wants unrecognized version %u", version);
		if (version > BGP_VERSION)
			rversion = version - BGP_VERSION;
		else
			rversion = BGP_VERSION;
		session_notification(peer, ERR_OPEN, ERR_OPEN_VERSION,
		    &rversion, sizeof(rversion));
		change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
		return (-1);
	}

	memcpy(&as, p, sizeof(as));
	p += sizeof(as);

	if (peer->conf.remote_as != ntohs(as)) {
		log_peer_warnx(&peer->conf, "peer sent wrong AS %u", ntohs(as));
		session_notification(peer, ERR_OPEN, ERR_OPEN_AS, NULL, 0);
		change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
		return (-1);
	}

	memcpy(&oholdtime, p, sizeof(oholdtime));
	p += sizeof(oholdtime);

	holdtime = ntohs(oholdtime);
	if (holdtime && holdtime < peer->conf.min_holdtime) {
		log_peer_warnx(&peer->conf,
		    "peer requests unacceptable holdtime %u", holdtime);
		session_notification(peer, ERR_OPEN, ERR_OPEN_HOLDTIME,
		    NULL, 0);
		change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
		return (-1);
	}

	myholdtime = peer->conf.holdtime;
	if (!myholdtime)
		myholdtime = conf->holdtime;
	if (holdtime < myholdtime)
		peer->holdtime = holdtime;
	else
		peer->holdtime = myholdtime;

	memcpy(&bgpid, p, sizeof(bgpid));
	p += sizeof(bgpid);

	/* check bgpid for validity, must be a valid ip address - HOW? */
	/* if ( bgpid invalid ) {
		log_peer_warnx(&peer->conf, "peer BGPID %lu unacceptable",
		    ntohl(bgpid));
		session_notification(peer, ERR_OPEN, ERR_OPEN_BGPID,
		    NULL, 0);
		change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
		return (-1);
	} */
	peer->remote_bgpid = bgpid;

	memcpy(&optparamlen, p, sizeof(optparamlen));
	p += sizeof(optparamlen);

	if (optparamlen > msglen - MSGSIZE_OPEN_MIN) {
			log_peer_warnx(&peer->conf,
			    "corrupt OPEN message received: length mismatch");
			session_notification(peer, ERR_OPEN, 0, NULL, 0);
			change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
			return (-1);
	}

	plen = optparamlen;
	while (plen > 0) {
		if (plen < 2) {
			log_peer_warnx(&peer->conf,
			    "corrupt OPEN message received, len wrong");
			session_notification(peer, ERR_OPEN, 0, NULL, 0);
			change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
			return (-1);
		}
		memcpy(&op_type, p, sizeof(op_type));
		p += sizeof(op_type);
		plen -= sizeof(op_type);
		memcpy(&op_len, p, sizeof(op_len));
		p += sizeof(op_type);
		plen -= sizeof(op_type);
		if (op_len > 0) {
			if (plen < op_len) {
				log_peer_warnx(&peer->conf,
				    "corrupt OPEN message received, len wrong");
				session_notification(peer, ERR_OPEN, 0,
				    NULL, 0);
				change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
				return (-1);
			}
			op_val = p;
			p += op_len;
			plen -= op_len;
		} else
			op_val = NULL;

		switch (op_type) {
		case OPT_PARAM_CAPABILITIES:		/* RFC 3392 */
			if (parse_capabilities(peer, op_val, op_len) == -1) {
				session_notification(peer, ERR_OPEN, 0,
				    NULL, 0);
				change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
				return (-1);
			}
			break;
		case OPT_PARAM_AUTH:			/* deprecated */
		default:
			/*
			 * unsupported type
			 * the RFCs tell us to leave the data section empty
			 * and notify the peer with ERR_OPEN, ERR_OPEN_OPT.
			 * How the peer should know _which_ optional parameter
			 * we don't support is beyond me.
			 */
			log_peer_warnx(&peer->conf,
			    "received OPEN message with unsupported optional "
			    "parameter: type %u", op_type);
			session_notification(peer, ERR_OPEN, ERR_OPEN_OPT,
				NULL, 0);
			change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
			peer->IdleHoldTimer = time(NULL);	/* no punish */
			peer->IdleHoldTime /= 2;
			return (-1);
			/* not reached */			
		}
	}

	return (0);
}

int
parse_update(struct peer *peer)
{
	u_char		*p;
	u_int16_t	 datalen;

	/*
	 * we pass the message verbatim to the rde.
	 * in case of errors the whole session is reset with a
	 * notification anyway, we only need to know the peer
	 */
	p = peer->rbuf->rptr;
	p += MSGSIZE_HEADER_MARKER;
	memcpy(&datalen, p, sizeof(datalen));
	datalen = ntohs(datalen);

	p = peer->rbuf->rptr;
	p += MSGSIZE_HEADER;	/* header is already checked */
	datalen -= MSGSIZE_HEADER;

	if (imsg_compose(&ibuf_rde, IMSG_UPDATE, peer->conf.id, p,
	    datalen) == -1)
		return (-1);

	return (0);
}

int
parse_notification(struct peer *peer)
{
	u_char		*p;
	u_int8_t	 errcode;
	u_int8_t	 subcode;
	u_int16_t	 datalen;

	/* just log */
	p = peer->rbuf->rptr;
	p += MSGSIZE_HEADER_MARKER;
	memcpy(&datalen, p, sizeof(datalen));
	datalen = ntohs(datalen);

	p = peer->rbuf->rptr;
	p += MSGSIZE_HEADER;	/* header is already checked */
	datalen -= MSGSIZE_HEADER;

	memcpy(&errcode, p, sizeof(errcode));
	p += sizeof(errcode);
	datalen -= sizeof(errcode);

	memcpy(&subcode, p, sizeof(subcode));
	p += sizeof(subcode);
	datalen -= sizeof(subcode);

	/* read & parse data section if needed */

	log_notification(peer, errcode, subcode, p, datalen);

	if (errcode == ERR_OPEN && subcode == ERR_OPEN_OPT) {
		peer->capa.announce = 0;
		return (1);
	}

	return (0);
}

int
parse_capabilities(struct peer *peer, u_char *d, u_int16_t dlen)
{
	u_int16_t	 len;
	u_int8_t	 capa_code;
	u_int8_t	 capa_len;
	u_char		*capa_val;
	u_int16_t	 mp_afi;
	u_int8_t	 mp_safi;

	len = dlen;
	while (len > 0) {
		if (len < 2)
			return (-1);
		memcpy(&capa_code, d, sizeof(capa_code));
		d += sizeof(capa_code);
		len -= sizeof(capa_code);
		memcpy(&capa_len, d, sizeof(capa_len));
		d += sizeof(capa_len);
		len -= sizeof(capa_len);
		if (capa_len > 0) {
			if (len < capa_len)
				return (-1);
			capa_val = d;
			d += capa_len;
			len -= capa_len;
		} else
			capa_val = NULL;

		switch (capa_code) {
		case CAPA_MP:			/* RFC 2858 */
			if (capa_len != 4)
				return (-1);
			memcpy(&mp_afi, capa_val, sizeof(mp_afi));
			memcpy(&mp_safi, capa_val + 3, sizeof(mp_safi));
			switch (mp_afi) {
			case AFI_IPv4:
				if (mp_safi < 1 || mp_safi > 3)
					return (-1);
				peer->capa.mp_v4 = mp_safi;
				break;
			case AFI_IPv6:
				if (mp_safi < 1 || mp_safi > 3)
					return (-1);
				peer->capa.mp_v6 = mp_safi;
				break;
			default:			/* ignore */
				break;
			}
			break;
		default:
			break;
		}
	}

	return (0);
}

void
session_dispatch_imsg(struct imsgbuf *ibuf, int idx)
{
	struct imsg		 imsg;
	struct mrt_config	 xmrt;
	struct mrt_config	*mrt;
	struct peer_config	*pconf;
	struct peer		*p, *next;
	u_char			*data;
	enum reconf_action	 reconf;
	int			 n;
	u_int8_t		 errcode, subcode;

	if ((n = imsg_read(ibuf)) == -1)
		fatal("session_dispatch_imsg: imsg_read error");

	if (n == 0)	/* connection closed */
		fatal("session_dispatch_imsg: pipe closed");

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("session_dispatch_imsg: imsg_get error");

		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_RECONF_CONF:
			if (idx != PFD_PIPE_MAIN)
				fatalx("reconf request not from parent");
			if ((nconf = malloc(sizeof(struct bgpd_config))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct bgpd_config));
			npeers = NULL;
			init_conf(nconf);
			pending_reconf = 1;
			break;
		case IMSG_RECONF_PEER:
			if (idx != PFD_PIPE_MAIN)
				fatalx("reconf request not from parent");
			pconf = imsg.data;
			p = getpeerbyaddr(&pconf->remote_addr);
			if (p == NULL) {
				if ((p = calloc(1, sizeof(struct peer))) ==
				    NULL)
					fatal("new_peer");
				p->state = STATE_NONE;
				p->next = npeers;
				npeers = p;
				reconf = RECONF_REINIT;
			} else
				reconf = RECONF_KEEP;

			memcpy(&p->conf, pconf, sizeof(struct peer_config));
			p->conf.reconf_action = reconf;
			break;
		case IMSG_RECONF_DONE:
			if (idx != PFD_PIPE_MAIN)
				fatalx("reconf request not from parent");
			if (nconf == NULL)
				fatalx("got IMSG_RECONF_DONE but no config");
			conf->as = nconf->as;
			conf->holdtime = nconf->holdtime;
			conf->bgpid = nconf->bgpid;
			conf->min_holdtime = nconf->min_holdtime;
			/* add new peers */
			for (p = npeers; p != NULL; p = next) {
				next = p->next;
				p->next = peers;
				peers = p;
			}
			/* find peers to be deleted */
			for (p = peers; p != NULL; p = p->next)
				if (p->conf.reconf_action == RECONF_NONE &&
				    !p->conf.cloned)
					p->conf.reconf_action = RECONF_DELETE;
			free(nconf);
			nconf = NULL;
			pending_reconf = 0;
			log_info("SE reconfigured");
			break;
		case IMSG_MRT_REQ:
			if ((mrt = calloc(1, sizeof(struct mrt_config))) ==
			    NULL)
				fatal("session_dispatch_imsg");
			memcpy(mrt, imsg.data, sizeof(struct mrt_config));
			mrt->ibuf = &ibuf_main;
			LIST_INSERT_HEAD(&mrt_l, mrt, list);
			break;
		case IMSG_MRT_END:
			memcpy(&xmrt, imsg.data, sizeof(struct mrt_config));
			LIST_FOREACH(mrt, &mrt_l, list) {
				if (mrt->type != xmrt.type)
					continue;
				if (mrt->peer_id == xmrt.peer_id &&
				    mrt->group_id == xmrt.group_id) {
					LIST_REMOVE(mrt, list);
					free(mrt);
					break;
				}
			}
			break;
		case IMSG_CTL_KROUTE:
		case IMSG_CTL_KROUTE_ADDR:
		case IMSG_CTL_SHOW_NEXTHOP:
		case IMSG_CTL_SHOW_INTERFACE:
			if (idx != PFD_PIPE_MAIN)
				fatalx("ctl kroute request not from parent");
			control_imsg_relay(&imsg);
			break;
		case IMSG_CTL_SHOW_RIB:
		case IMSG_CTL_SHOW_RIB_PREFIX:
			if (idx != PFD_PIPE_ROUTE)
				fatalx("ctl rib request not from RDE");
			control_imsg_relay(&imsg);
			break;
		case IMSG_CTL_END:
			control_imsg_relay(&imsg);
			break;
		case IMSG_UPDATE:
			if (idx != PFD_PIPE_ROUTE)
				fatalx("update request not from RDE");
			if (imsg.hdr.len > IMSG_HEADER_SIZE +
			    MAX_PKTSIZE - MSGSIZE_HEADER ||
			    imsg.hdr.len < IMSG_HEADER_SIZE +
			    MSGSIZE_UPDATE_MIN - MSGSIZE_HEADER)
				log_warnx("RDE sent invalid update");
			else
				session_update(imsg.hdr.peerid, imsg.data,
				    imsg.hdr.len - IMSG_HEADER_SIZE);
			break;
		case IMSG_UPDATE_ERR:
			if (idx != PFD_PIPE_ROUTE)
				fatalx("update request not from RDE");
			if (imsg.hdr.len < IMSG_HEADER_SIZE + 2) {
				log_warnx("RDE sent invalid notification");
				break;
			}
			if ((p = getpeerbyid(imsg.hdr.peerid)) == NULL) {
				log_warnx("no such peer: id=%u",
				    imsg.hdr.peerid);
				break;
			}
			data = imsg.data;
			errcode = *data++;
			subcode = *data++;

			if (imsg.hdr.len == IMSG_HEADER_SIZE + 2)
				data = NULL;

			session_notification(p, errcode, subcode,
			    data, imsg.hdr.len - IMSG_HEADER_SIZE - 2);
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}
}

struct peer *
getpeerbyaddr(struct bgpd_addr *addr)
{
	struct peer *p;

	/* we might want a more effective way to find peers by IP */
	for (p = peers; p != NULL &&
	    memcmp(&p->conf.remote_addr, addr, sizeof(p->conf.remote_addr));
	    p = p->next)
		;	/* nothing */

	return (p);
}

struct peer *
getpeerbyip(struct sockaddr *ip)
{
	struct peer	*p, *newpeer, *loose = NULL;
	u_int32_t	 id;

	/* we might want a more effective way to find peers by IP */
	for (p = peers; p != NULL; p = p->next)
		if (!p->conf.template &&
		    p->conf.remote_addr.af == ip->sa_family) {
			if (p->conf.remote_addr.af == AF_INET &&
			    p->conf.remote_addr.v4.s_addr ==
			    ((struct sockaddr_in *)ip)->sin_addr.s_addr)
				return (p);
			if (p->conf.remote_addr.af == AF_INET6 &&
			    !bcmp(&p->conf.remote_addr.v6,
			    &((struct sockaddr_in6 *)ip)->sin6_addr,
			    sizeof(p->conf.remote_addr.v6)))
				return (p);
		}

	/* try template matching */
	for (p = peers; p != NULL; p = p->next)
		if (p->conf.template &&
		    p->conf.remote_addr.af == ip->sa_family &&
		    session_match_mask(p, ip))
			if (loose == NULL || loose->conf.remote_masklen <
			    p->conf.remote_masklen)
				loose = p;

	if (loose != NULL) {
		/* clone */
		if ((newpeer = malloc(sizeof(struct peer))) == NULL)
			fatal(NULL);
		memcpy(newpeer, loose, sizeof(struct peer));
		for (id = UINT_MAX; id > UINT_MAX / 2; id--) {
			for (p = peers; p != NULL && p->conf.id != id;
			    p = p->next)
				;	/* nothing */
			if (p == NULL) {	/* we found a free id */
				newpeer->conf.id = id;
				break;
			}
		}
		if (newpeer->conf.remote_addr.af == AF_INET) {
			newpeer->conf.remote_addr.v4.s_addr =
			    ((struct sockaddr_in *)ip)->sin_addr.s_addr;
			newpeer->conf.remote_masklen = 32;
		}
		if (newpeer->conf.remote_addr.af == AF_INET6) {
			memcpy(&p->conf.remote_addr.v6,
			    &((struct sockaddr_in6 *)ip)->sin6_addr,
			    sizeof(newpeer->conf.remote_addr.v6));
			newpeer->conf.remote_masklen = 128;
		}
		newpeer->conf.template = 0;
		newpeer->conf.cloned = 1;
		newpeer->state = STATE_NONE;
		newpeer->rbuf = NULL;
		init_peer(newpeer);
		bgp_fsm(newpeer, EVNT_START);
		newpeer->next = peers;
		peers = newpeer;
		return (newpeer);
	}

	return (NULL);
}

int
session_match_mask(struct peer *p, struct sockaddr *ip)
{
	int		 i;
	in_addr_t	 v4mask;
	struct in6_addr	*in;
	struct in6_addr	 mask;

	if (p->conf.remote_addr.af == AF_INET) {
		v4mask = htonl(0xffffffff << (32 - p->conf.remote_masklen));
		if (p->conf.remote_addr.v4.s_addr ==
		    ((((struct sockaddr_in *)ip)->sin_addr.s_addr) & v4mask))
			return (1);
		else
			return (0);
	}

	if (p->conf.remote_addr.af == AF_INET6) {
		for (i = 0; i < p->conf.remote_masklen / 8; i++)
			mask.s6_addr[i] = 0xff;
		i = p->conf.remote_masklen % 8;
		if (i)
			mask.s6_addr[p->conf.remote_masklen / 8] = 0xff00 >> i;

		in = &((struct sockaddr_in6 *)ip)->sin6_addr;

		for (i = 0; i < 16; i++)
			if ((in->s6_addr[i] & mask.s6_addr[i]) !=
			    p->conf.remote_addr.addr8[i])
				return (0);

		return (1);
	}

	return (0);
}

struct peer *
getpeerbyid(u_int32_t peerid)
{
	struct peer *p;

	/* we might want a more effective way to find peers by IP */
	for (p = peers; p != NULL &&
	    p->conf.id != peerid; p = p->next)
		;	/* nothing */

	return (p);
}

void
session_down(struct peer *peer)
{
	peer->stats.last_updown = time(NULL);
	if (imsg_compose(&ibuf_rde, IMSG_SESSION_DOWN, peer->conf.id,
	    NULL, 0) == -1)
		fatalx("imsg_compose error");
}

void
session_up(struct peer *peer)
{
	struct session_up	sup;

	sup.remote_bgpid = peer->remote_bgpid;

	switch (peer->sa_local.ss_family) {
	case AF_INET:
		sup.local_addr.af = AF_INET;
		memcpy(&sup.local_addr.v4,
		    &((struct sockaddr_in *)&peer->sa_local)->sin_addr,
		    sizeof(sup.local_addr.v4));
		sup.remote_addr.af = AF_INET;
		memcpy(&sup.remote_addr.v4,
		    &((struct sockaddr_in *)&peer->sa_remote)->sin_addr,
		    sizeof(sup.remote_addr.v4));
		break;
	case AF_INET6:
		sup.local_addr.af = AF_INET6;
		memcpy(&sup.local_addr.v6,
		    &((struct sockaddr_in6 *)&peer->sa_local)->sin6_addr,
		    sizeof(sup.local_addr.v6));
		sup.remote_addr.af = AF_INET6;
		memcpy(&sup.remote_addr.v6,
		    &((struct sockaddr_in6 *)&peer->sa_remote)->sin6_addr,
		    sizeof(sup.remote_addr.v6));
		break;
	default:
		fatalx("session_up: unsupported address family");
	}

	memcpy(&sup.conf, &peer->conf, sizeof(sup.conf));
	peer->stats.last_updown = time(NULL);
	if (imsg_compose(&ibuf_rde, IMSG_SESSION_UP, peer->conf.id,
	    &sup, sizeof(sup)) == -1)
		fatalx("imsg_compose error");
}

int
imsg_compose_parent(int type, pid_t pid, void *data, u_int16_t datalen)
{
	return (imsg_compose_pid(&ibuf_main, type, pid, data, datalen));
}

int
imsg_compose_rde(int type, pid_t pid, void *data, u_int16_t datalen)
{
	return (imsg_compose_pid(&ibuf_rde, type, pid, data, datalen));
}

static struct sockaddr *
addr2sa(struct bgpd_addr *addr, u_int16_t port)
{
	static struct sockaddr_storage	 ss;
	struct sockaddr_in		*sa_in = (struct sockaddr_in *)&ss;
	struct sockaddr_in6		*sa_in6 = (struct sockaddr_in6 *)&ss;

	bzero(&ss, sizeof(ss));
	switch (addr->af) {
	case AF_INET:
		sa_in->sin_family = AF_INET;
		sa_in->sin_len = sizeof(struct sockaddr_in);
		sa_in->sin_addr.s_addr = addr->v4.s_addr;
		sa_in->sin_port = htons(port);
		break;
	case AF_INET6:
		sa_in6->sin6_family = AF_INET6;
		sa_in6->sin6_len = sizeof(struct sockaddr_in6);
		memcpy(&sa_in6->sin6_addr, &addr->v6,
		    sizeof(sa_in6->sin6_addr));
		sa_in6->sin6_port = htons(port);
		break;
	}

	return ((struct sockaddr *)&ss);
}
