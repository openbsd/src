/*	$OpenBSD: session.c,v 1.30 2003/12/23 18:28:05 henning Exp $ */

/*
 * Copyright (c) 2003 Henning Brauer <henning@openbsd.org>
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
#include "session.h"

#define	PFD_LISTEN	0
#define PFD_PIPE_MAIN	1
#define PFD_PIPE_ROUTE	2
#define PFD_PEERS_START	3

void	session_sighdlr(int);
int	setup_listener(void);
void	init_conf(struct bgpd_config *);
void	init_peers(void);
void	bgp_fsm(struct peer *, enum session_events);
int	timer_due(time_t);
void	start_timer_holdtime(struct peer *);
void	start_timer_keepalive(struct peer *);
void	session_close_connection(struct peer *);
void	session_terminate(void);
void	change_state(struct peer *, enum session_state, enum session_events);
int	session_setup_socket(struct peer *);
void	session_accept(int);
int	session_connect(struct peer *);
void	session_open(struct peer *);
void	session_keepalive(struct peer *);
void	session_update(struct peer *);
void	session_notification(struct peer *, u_int8_t, u_int8_t, void *,
	    ssize_t);
int	session_dispatch_msg(struct pollfd *, struct peer *);
int	parse_header(struct peer *, u_char *, u_int16_t *, u_int8_t *);
int	parse_open(struct peer *);
int	parse_update(struct peer *);
int	parse_notification(struct peer *);
int	parse_keepalive(struct peer *);
void	session_dispatch_imsg(struct imsgbuf *, int);
void	session_up(struct peer *);
void	session_down(struct peer *);

struct peer	*getpeerbyip(in_addr_t);

struct bgpd_config	*conf = NULL, *nconf = NULL;
volatile sig_atomic_t	 session_quit = 0;
int			 pending_reconf = 0;
int			 sock = -1;
struct imsgbuf		 ibuf_rde;
struct imsgbuf		 ibuf_main;

void
session_sighdlr(int sig)
{
	switch (sig) {
	case SIGTERM:
		session_quit = 1;
		break;
	}
}

int
setup_listener(void)
{
	int			 fd, opt;

	if ((fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0)
		return (fd);

	opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &opt, sizeof(opt));

	if (bind(fd, (struct sockaddr *)&conf->listen_addr,
	    sizeof(conf->listen_addr))) {
		close(fd);
		return (-1);
	}
	if (listen(fd, MAX_BACKLOG)) {
		close(fd);
		return (-1);
	}
	if (fcntl(fd, F_SETFL, O_NONBLOCK) == -1) {
		close(fd);
		return (-1);
	}
	
	return (fd);
}


int
session_main(struct bgpd_config *config, int pipe_m2s[2], int pipe_s2r[2])
{
	int		 nfds, i, j, timeout;
	pid_t		 pid;
	time_t		 nextaction;
	struct passwd	*pw;
	struct peer	*p, *peers[OPEN_MAX], *last, *next;
	struct pollfd	 pfd[OPEN_MAX];

	conf = config;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork", errno);
	case 0:
		break;
	default:
		return (pid);
	}

	if ((pw = getpwnam(BGPD_USER)) == NULL)
		fatal(NULL, errno);

	if (chroot(pw->pw_dir) < 0)
		fatal("chroot failed", errno);
	chdir("/");

	setproctitle("session engine");
	bgpd_process = PROC_SE;

	if ((sock = setup_listener()) < 0)
		fatal("listener setup failed", 0);

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid))
		fatal("can't drop privileges", errno);

	endpwent();

	signal(SIGTERM, session_sighdlr);
	logit(LOG_INFO, "session engine ready");
	close(pipe_m2s[0]);
	close(pipe_s2r[1]);
	init_conf(conf);
	imsg_init(&ibuf_rde, pipe_s2r[0]);
	imsg_init(&ibuf_main, pipe_m2s[1]);
	init_peers();

	while (session_quit == 0) {
		bzero(&pfd, sizeof(pfd));
		if (sock != -1) {
			pfd[PFD_LISTEN].fd = sock;
			pfd[PFD_LISTEN].events = POLLIN;
		}
		pfd[PFD_PIPE_MAIN].fd = ibuf_main.sock;
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		pfd[PFD_PIPE_ROUTE].fd = ibuf_rde.sock;
		pfd[PFD_PIPE_ROUTE].events = POLLIN;
		if (ibuf_rde.w.queued > 0)
			pfd[PFD_PIPE_ROUTE].events |= POLLOUT;

		nextaction = time(NULL) + 240;	/* loop every 240s at least */
		i = PFD_PEERS_START;

		last = NULL;
		for (p = conf->peers; p != NULL; p = next) {
			next = p->next;
			if (!pending_reconf) {
				/* needs init? */
				if (p->state == STATE_NONE)
					change_state(p, STATE_IDLE, EVNT_NONE);

				/* reinit due? */
				if (p->conf.reconf_action == RECONF_REINIT) {
					bgp_fsm(p, EVNT_STOP);
					p->StartTimer = time(NULL);
				}

				/* deletion due? */
				if (p->conf.reconf_action == RECONF_DELETE) {
					bgp_fsm(p, EVNT_STOP);
					log_errx(p, "removed");
					if (last != NULL)
						last->next = next;
					else
						conf->peers = next;
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
			if (timer_due(p->StartTimer))
				bgp_fsm(p, EVNT_START);

			/* set nextaction to the first expiring timer */
			if (p->ConnectRetryTimer &&
			    p->ConnectRetryTimer < nextaction)
				nextaction = p->ConnectRetryTimer;
			if (p->HoldTimer && p->HoldTimer < nextaction)
				nextaction = p->HoldTimer;
			if (p->KeepaliveTimer && p->KeepaliveTimer < nextaction)
				nextaction = p->KeepaliveTimer;
			if (p->StartTimer && p->StartTimer < nextaction)
				nextaction = p->StartTimer;

			/* are we waiting for a write? */
			if (p->wbuf.queued > 0)
				p->events |= POLLOUT;

			/* poll events */
			if (p->sock != -1 && p->events != 0) {
				pfd[i].fd = p->sock;
				pfd[i].events = p->events;
				peers[i] = p;
				i++;
			}
		}

		timeout = nextaction - time(NULL);
		if (timeout < 0)
			timeout = 0;
		nfds = poll(pfd, i, timeout * 1000);
		/*
		 * what do we do on poll error?
		 */
		if (nfds > 0 && pfd[PFD_LISTEN].revents & POLLIN) {
			nfds--;
			session_accept(sock);
		}

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & POLLIN) {
			nfds--;
			session_dispatch_imsg(&ibuf_main, PFD_PIPE_MAIN);
		}

		if (nfds > 0 && pfd[PFD_PIPE_ROUTE].revents & POLLOUT)
			if (msgbuf_write(&ibuf_rde.w) == -1)
				fatal("pipe write error", 0);

		if (nfds > 0 && pfd[PFD_PIPE_ROUTE].revents & POLLIN) {
			nfds--;
			session_dispatch_imsg(&ibuf_rde, PFD_PIPE_ROUTE);
		}

		for (j = PFD_PEERS_START; nfds > 0 && j < i; j++) {
			nfds -= session_dispatch_msg(&pfd[j], peers[j]);
		}
	}

	logit(LOG_INFO, "session engine exiting");
	_exit(0);
}

void
init_conf(struct bgpd_config *c)
{
	if (!c->holdtime)
		c->holdtime = INTERVAL_HOLD;
}

void
init_peers(void)
{
	struct peer	*p;

	for (p = conf->peers; p != NULL; p = p->next) {
		if (p->state == STATE_NONE) {
			change_state(p, STATE_IDLE, EVNT_NONE);
			p->StartTimer = time(NULL);	/* start ASAP */
		}
	}
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
			peer->events = 0;
			peer->StartTimer = 0;
			peer->ConnectRetryTimer =
			    time(NULL) + INTERVAL_CONNECTRETRY;

			/* allocate read buffer */
			peer->rbuf = calloc(1, sizeof(struct peer_buf_read));
			if (peer->rbuf == NULL)
				fatal(NULL, errno);
			peer->rbuf->wptr = peer->rbuf->buf;
			peer->rbuf->pkt_len = MSGSIZE_HEADER;

			/* init write buffer */
			msgbuf_init(&peer->wbuf);

			change_state(peer, STATE_CONNECT, event);
			session_connect(peer);
			break;
		default:
			/* ignore */
		}
		break;
	case STATE_CONNECT:
		switch (event) {
		case EVNT_START:
			/* ignore */
			break;
		case EVNT_CON_OPEN:
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
			session_connect(peer);
			change_state(peer, STATE_CONNECT, event);
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
			if (parse_open(peer))
				change_state(peer, STATE_IDLE, event);
			else {
				session_keepalive(peer);
				change_state(peer, STATE_OPENCONFIRM, event);
			}
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
		peer->sock = -1;
		peer->wbuf.sock = -1;
	}
}

void
session_terminate(void)
{
	struct peer	*p;

	for (p = conf->peers; p != NULL; p = p->next)
		bgp_fsm(p, EVNT_STOP);

	shutdown(sock, SHUT_RDWR);
	close(sock);
	sock = -1;
}

void
change_state(struct peer *peer, enum session_state state,
    enum session_events event)
{
	switch (state) {
	case STATE_IDLE:
		/*
		 * we must start the timer for the next EVNT_START
		 * if we are coming here due to an error and the
		 * session was not established successfull before, the
		 * starttimerinterval needs to be exponentially increased
		 */
		peer->events = 0;
		if (peer->StartTimerInterval == 0)
			peer->StartTimerInterval = INTERVAL_START;
		peer->holdtime = INTERVAL_HOLD_INITIAL;
		peer->ConnectRetryTimer = 0;
		peer->KeepaliveTimer = 0;
		peer->HoldTimer = 0;
		session_close_connection(peer);
		msgbuf_clear(&peer->wbuf);
		free(peer->rbuf);
		peer->rbuf = NULL;
		if (peer->state == STATE_ESTABLISHED)
			session_down(peer);
		if (event != EVNT_STOP) {
			peer->StartTimer = time(NULL) +
			    peer->StartTimerInterval;
			if (peer->StartTimerInterval < UINT_MAX / 2)
				peer->StartTimerInterval *= 2;
		}
		break;
	case STATE_CONNECT:
		peer->events = (POLLIN|POLLOUT);
		break;
	case STATE_ACTIVE:
		peer->events = (POLLIN|POLLOUT);
		break;
	case STATE_OPENSENT:
		peer->events = POLLIN;
		break;
	case STATE_OPENCONFIRM:
		peer->events = POLLIN;
		break;
	case STATE_ESTABLISHED:
		peer->events = POLLIN;
		peer->StartTimerInterval = INTERVAL_START;
		session_up(peer);
		break;
	default:		/* something seriously fucked */
	}

	log_statechange(peer, state, event);
	peer->state = state;
}

void
session_accept(int listenfd)
{
	int			 connfd;
	socklen_t		 len;
	struct sockaddr_in	 cliaddr;
	struct peer		*p = NULL;

	/* collision detection, 6.8, missing */

	len = sizeof(cliaddr);
	if ((connfd = accept(listenfd,
	    (struct sockaddr *)&cliaddr, &len)) == -1) {
		if (errno == EWOULDBLOCK || errno == EINTR)
			/* EINTR check needed? stevens says yes */
			return;
		else
			/* what do we do here? log & ignore? */
			;
	}

	p = getpeerbyip(cliaddr.sin_addr.s_addr);

	if (p != NULL &&
	    (p->state == STATE_CONNECT || p->state == STATE_ACTIVE)) {
		p->sock = connfd;
		p->wbuf.sock = connfd;
		if (session_setup_socket(p)) {
			shutdown(connfd, SHUT_RDWR);
			close(connfd);
			return;
		}
		bgp_fsm(p, EVNT_CON_OPEN);
	} else {
		log_conn_attempt(p, cliaddr.sin_addr);
		shutdown(connfd, SHUT_RDWR);
		close(connfd);
	}
}

int
session_connect(struct peer *peer)
{
	int		n;

	/* collision detection, 6.8, missing */

	if (peer->sock != -1)	/* what do we do here? */
		return (-1);

	if ((peer->sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
		log_err(peer, "session_connect socket");
		bgp_fsm(peer, EVNT_CON_OPENFAIL);
		return (-1);
	}

	peer->wbuf.sock = peer->sock;

	/* if update source is set we need to bind() */
	if (peer->conf.local_addr.sin_addr.s_addr)
		if (bind(peer->sock, (struct sockaddr *)&peer->conf.local_addr,
		    sizeof(peer->conf.local_addr))) {
			log_err(peer, "session_connect bind");
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (-1);
		}

	if (fcntl(peer->sock, F_SETFL, O_NONBLOCK) == -1) {
		log_err(peer, "session_connect fcntl");
		bgp_fsm(peer, EVNT_CON_OPENFAIL);
		return (-1);
	}

	if (session_setup_socket(peer)) {
		bgp_fsm(peer, EVNT_CON_OPENFAIL);
		return (-1);
	}

	if ((n = connect(peer->sock, (struct sockaddr *)&peer->conf.remote_addr,
	    sizeof(peer->conf.remote_addr))) < 0)
		if (errno != EINPROGRESS) {
			log_err(peer, "connect");
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

	if (p->conf.ebgp)
		/* set TTL to foreign router's distance - 1=direct n=multihop */
		if (setsockopt(p->sock, IPPROTO_IP, IP_TTL, &ttl,
		    sizeof(ttl)) == -1) {
			log_err(p, "session_setup_socket setsockopt TTL");
			return (-1);
		}

	/* set TCP_NODELAY */
	if (setsockopt(p->sock, IPPROTO_TCP, TCP_NODELAY, &nodelay,
	    sizeof(nodelay)) == -1) {
		log_err(p, "session_setup_socket setsockopt TCP_NODELAY");
		return (-1);
	}

	/* set precedence, see rfc1771 appendix 5 */
	if (setsockopt(p->sock, IPPROTO_IP, IP_TOS, &pre, sizeof(pre)) == -1) {
		log_err(p, "session_setup_socket setsockopt TOS");
		return (-1);
	}

	return (0);
}

void
session_open(struct peer *peer)
{
	struct msg_open	 msg;
	struct buf	*buf;
	u_int16_t	 len;
	int		 errs = 0;

	len = MSGSIZE_OPEN_MIN;

	memset(&msg.header.marker, 0xff, sizeof(msg.header.marker));
	msg.header.len = htons(len);
	msg.header.type = OPEN;
	msg.version = 4;
	msg.myas = htons(conf->as);
	msg.holdtime = htons(conf->holdtime);
	msg.bgpid = conf->bgpid;	/* is already in network byte order */
	msg.optparamlen = 0;

	if ((buf = buf_open(len)) == NULL)
		bgp_fsm(peer, EVNT_CON_FATAL);
	errs += buf_add(buf, &msg.header.marker, sizeof(msg.header.marker));
	errs += buf_add(buf, &msg.header.len, sizeof(msg.header.len));
	errs += buf_add(buf, &msg.header.type, sizeof(msg.header.type));
	errs += buf_add(buf, &msg.version, sizeof(msg.version));
	errs += buf_add(buf, &msg.myas, sizeof(msg.myas));
	errs += buf_add(buf, &msg.holdtime, sizeof(msg.holdtime));
	errs += buf_add(buf, &msg.bgpid, sizeof(msg.bgpid));
	errs += buf_add(buf, &msg.optparamlen, sizeof(msg.optparamlen));

	if (errs == 0) {
		if (buf_close(&peer->wbuf, buf) == -1) {
			buf_free(buf);
			bgp_fsm(peer, EVNT_CON_FATAL);
		}
	} else {
		buf_free(buf);
		bgp_fsm(peer, EVNT_CON_FATAL);
	}
}

void
session_keepalive(struct peer *peer)
{
	struct msg_header	 msg;
	struct buf		*buf;
	ssize_t			 len;
	int			 errs = 0;

	len = MSGSIZE_KEEPALIVE;

	memset(&msg.marker, 0xff, sizeof(msg.marker));
	msg.len = htons(len);
	msg.type = KEEPALIVE;

	if ((buf = buf_open(len)) == NULL)
		bgp_fsm(peer, EVNT_CON_FATAL);
	errs += buf_add(buf, &msg.marker, sizeof(msg.marker));
	errs += buf_add(buf, &msg.len, sizeof(msg.len));
	errs += buf_add(buf, &msg.type, sizeof(msg.type));

	if (errs > 0) {
		buf_free(buf);
		bgp_fsm(peer, EVNT_CON_FATAL);
		return;
	}

	if (buf_close(&peer->wbuf, buf) == -1) {
		buf_free(buf);
		bgp_fsm(peer, EVNT_CON_FATAL);
		return;
	}
	start_timer_keepalive(peer);
}

void
session_update(struct peer *peer)
{
	start_timer_keepalive(peer);
}

void
session_notification(struct peer *peer, u_int8_t errcode, u_int8_t subcode,
    void *data, ssize_t datalen)
{
	struct msg_header	 msg;
	struct buf		*buf;
	ssize_t			 len;
	int			 errs = 0;

	len = MSGSIZE_NOTIFICATION_MIN + datalen;

	memset(&msg.marker, 0xff, sizeof(msg.marker));
	msg.len = htons(len);
	msg.type = NOTIFICATION;

	if ((buf = buf_open(len)) == NULL)
		bgp_fsm(peer, EVNT_CON_FATAL);
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

	if (buf_close(&peer->wbuf, buf) == -1) {
		buf_free(buf);
		bgp_fsm(peer, EVNT_CON_FATAL);
	}
}

int
session_dispatch_msg(struct pollfd *pfd, struct peer *peer)
{
	ssize_t		n, read_total;
	socklen_t	len;
	int		error;

	if (peer->state == STATE_CONNECT) {
		if (pfd->revents & POLLOUT) {
			if (pfd->revents & POLLIN) {
				/* error occured */
				len = sizeof(error);
				if (getsockopt(pfd->fd, SOL_SOCKET, SO_ERROR,
				    &error, &len) == -1)
					logit(LOG_CRIT, "unknown socket error");
				else {
					errno = error;
					log_err(peer, "socket error");
				}
				bgp_fsm(peer, EVNT_CON_OPENFAIL);
			} else
				bgp_fsm(peer, EVNT_CON_OPEN);
			return (1);
		}
		if (pfd->revents & POLLHUP) {
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (1);
		}
		if (pfd->revents & (POLLERR|POLLNVAL)) {
			bgp_fsm(peer, EVNT_CON_FATAL);
			return (1);
		}
		return (0);
	}

	if (pfd->revents & POLLHUP) {
		bgp_fsm(peer, EVNT_CON_CLOSED);
		return (1);
	}
	if (pfd->revents & (POLLERR|POLLNVAL)) {
		bgp_fsm(peer, EVNT_CON_FATAL);
		return (1);
	}

	if (pfd->revents & POLLOUT && peer->wbuf.queued) {
		if (msgbuf_write(&peer->wbuf))
			bgp_fsm(peer, EVNT_CON_FATAL);
		if (!(pfd->revents & POLLIN))
			return (1);
	}

	if (pfd->revents & POLLIN) {
		read_total = 0;
		do {
			if ((n = read(peer->sock, peer->rbuf->wptr,
			    peer->rbuf->pkt_len - peer->rbuf->read_len)) ==
			    -1) {
				if (errno != EAGAIN && errno != EINTR) {
					log_err(peer, "read error");
					bgp_fsm(peer, EVNT_CON_FATAL);
				}
				return (1);
			}
			read_total += n;
			peer->rbuf->wptr += n;
			peer->rbuf->read_len += n;
			if (peer->rbuf->read_len == peer->rbuf->pkt_len) {
				if (!peer->rbuf->seen_hdr) {	/* got header */
					if (parse_header(peer,
					    peer->rbuf->buf,
					    &peer->rbuf->pkt_len,
					    &peer->rbuf->type) == 1) {
						bgp_fsm(peer, EVNT_CON_FATAL);
						return (1);
					}
					peer->rbuf->seen_hdr = 1;
				} else {	/* we got the full packet */
					switch (peer->rbuf->type) {
					case OPEN:
						bgp_fsm(peer, EVNT_RCVD_OPEN);
						break;
					case UPDATE:
						bgp_fsm(peer, EVNT_RCVD_UPDATE);
						break;
					case NOTIFICATION:
						bgp_fsm(peer,
						    EVNT_RCVD_NOTIFICATION);
						break;
					case KEEPALIVE:
						bgp_fsm(peer,
						    EVNT_RCVD_KEEPALIVE);
						break;
					default:	/* cannot happen */
						session_notification(peer,
						    ERR_HEADER, ERR_HDR_TYPE,
						    &peer->rbuf->type, 1);
						logit(LOG_CRIT,
						    "received message with "
						    "unknown type %u",
						    peer->rbuf->type);
					}
					n = 0;	/* give others a chance... */
					if (peer->rbuf != NULL) {
						bzero(peer->rbuf,
						    sizeof(struct peer_buf_read));
						peer->rbuf->wptr =
						    peer->rbuf->buf;
						peer->rbuf->pkt_len =
						    MSGSIZE_HEADER;
					}
				}
			}
		} while (n > 0);
		if (read_total == 0) /* connection closed */
			bgp_fsm(peer, EVNT_CON_CLOSED);
		return (1);
	}

	return (0);
}

int
parse_header(struct peer *peer, u_char *data, u_int16_t *len, u_int8_t *type)
{
	u_char		*p;
	u_char		 one = 0xff;
	int		 i;
	u_int16_t	 olen;

	/* caller MUST make sure we are getting 19 bytes! */
	p = data;
	for (i = 0; i < 16; i++) {
		if (memcmp(p, &one, 1)) {
			log_errx(peer, "received message: sync error");
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
		log_errx(peer, "received message: illegal length: %u byte",
		    *len);
		session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
		    &olen, sizeof(olen));
		return (-1);
	}

	switch (*type) {
	case OPEN:
		if (*len < MSGSIZE_OPEN_MIN) {
			log_errx(peer,
			    "received OPEN: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			return (-1);
		}
		break;
	case NOTIFICATION:
		if (*len < MSGSIZE_NOTIFICATION_MIN) {
			log_errx(peer,
			    "received NOTIFICATION: illegal len: %u byte",
			    *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			return (-1);
		}
		break;
	case UPDATE:
		if (*len < MSGSIZE_UPDATE_MIN) {
			log_errx(peer,
			    "received UPDATE: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			return (-1);
		}
		break;
	case KEEPALIVE:
		if (*len != MSGSIZE_KEEPALIVE) {
			log_errx(peer,
			    "received KEEPALIVE: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			return (-1);
		}
		break;
	default:
		log_errx(peer, "received msg with unknown type %u", *type);
		session_notification(peer, ERR_HEADER, ERR_HDR_TYPE,
		    type, 1);
		return (-1);
	}
	return (0);
}

int
parse_open(struct peer *peer)
{
	u_char		*p;
	u_int8_t	 version;
	u_int16_t	 as;
	u_int16_t	 holdtime, oholdtime;
	u_int32_t	 bgpid;
	u_int8_t	 optparamlen;

	p = peer->rbuf->buf;
	p += MSGSIZE_HEADER;	/* header is already checked */

	memcpy(&version, p, sizeof(version));
	p += sizeof(version);

	if (version != BGP_VERSION) {
		if (version > BGP_VERSION)
			log_errx(peer, "peer wants unrecognized version %u",
			    version);
			session_notification(peer, ERR_OPEN,
			    ERR_OPEN_VERSION, &version, sizeof(version));
		return (-1);
	}

	memcpy(&as, p, sizeof(as));
	p += sizeof(as);

	if (peer->conf.remote_as != ntohs(as)) {
		log_errx(peer, "peer AS %u unacceptable", ntohs(as));
		session_notification(peer, ERR_OPEN, ERR_OPEN_AS, NULL, 0);
		return (-1);
	}

	memcpy(&oholdtime, p, sizeof(oholdtime));
	p += sizeof(oholdtime);

	holdtime = ntohs(oholdtime);
	if (holdtime && holdtime < conf->min_holdtime) {
		log_errx(peer, "peer requests unacceptable holdtime %u",
		    holdtime);
		session_notification(peer, ERR_OPEN, ERR_OPEN_HOLDTIME,
		    NULL, 0);
		return (-1);
	}
	if (holdtime < conf->holdtime)
		peer->holdtime = holdtime;
	else
		peer->holdtime = conf->holdtime;

	memcpy(&bgpid, p, sizeof(bgpid));
	p += sizeof(bgpid);

	/* check bgpid for validity, must be a valid ip address - HOW? */
	/* if ( bgpid invalid ) {
		log_errx(peer, "peer BGPID %lu unacceptable", ntohl(bgpid));
		session_notification(peer, ERR_OPEN, ERR_OPEN_BGPID,
		    NULL, 0);
		return (-1);
	} */
	peer->remote_bgpid = ntohl(bgpid);

	memcpy(&optparamlen, p, sizeof(optparamlen));
	p += sizeof(optparamlen);

	/* handle opt params... */

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
	p = peer->rbuf->buf;
	p += MSGSIZE_HEADER_MARKER;
	memcpy(&datalen, p, sizeof(datalen));
	datalen = ntohs(datalen);

	p = peer->rbuf->buf;
	p += MSGSIZE_HEADER;	/* header is already checked */
	datalen -= MSGSIZE_HEADER;

	imsg_compose(&ibuf_rde, IMSG_UPDATE, peer->conf.id, p, datalen);

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
	p = peer->rbuf->buf;
	p += MSGSIZE_HEADER_MARKER;
	memcpy(&datalen, p, sizeof(datalen));
	datalen = ntohs(datalen);

	p = peer->rbuf->buf;
	p += MSGSIZE_HEADER;	/* header is already checked */
	datalen -= MSGSIZE_HEADER;

	memcpy(&errcode, p, sizeof(errcode));
	p += sizeof(errcode);
	datalen -= sizeof(errcode);

	memcpy(&subcode, p, sizeof(subcode));
	p += sizeof(subcode);
	datalen -= sizeof(subcode);

	/* read & parse data section if needed */

	/* log */
	log_notification(peer, errcode, subcode, p, datalen);

	return (0);
}

void
session_dispatch_imsg(struct imsgbuf *ibuf, int idx)
{
	struct imsg		 imsg;
	struct peer_config	*pconf;
	struct peer		*p, *next;
	enum reconf_action	 reconf;

	if (imsg_get(ibuf, &imsg) > 0) {
		switch (imsg.hdr.type) {
		case IMSG_RECONF_CONF:
			if (idx != PFD_PIPE_MAIN)
				fatal("reconf request not from parent", 0);
			if ((nconf = malloc(sizeof(struct bgpd_config))) ==
			    NULL)
				fatal(NULL, errno);
			memcpy(nconf, imsg.data, sizeof(struct bgpd_config));
			nconf->peers = NULL;
			init_conf(nconf);
			pending_reconf = 1;
			break;
		case IMSG_RECONF_PEER:
			if (idx != PFD_PIPE_MAIN)
				fatal("reconf request not from parent", 0);
			pconf = imsg.data;
			p = getpeerbyip(pconf->remote_addr.sin_addr.s_addr);
			if (p == NULL) {
				if ((p = calloc(1, sizeof(struct peer))) ==
				    NULL)
					fatal("new_peer", errno);
				p->state = STATE_NONE;
				p->sock = -1;
				p->next = nconf->peers;
				nconf->peers = p;
				reconf = RECONF_REINIT;
			} else
				reconf = RECONF_KEEP;

			if (bcmp(&p->conf.remote_addr, &pconf->remote_addr,
			    sizeof(struct sockaddr_in)))
				reconf = RECONF_REINIT;
			if (bcmp(&p->conf.local_addr, &pconf->local_addr,
			    sizeof(struct sockaddr_in)))
				reconf = RECONF_REINIT;
			if (p->conf.remote_as != pconf->remote_as)
				reconf = RECONF_REINIT;
			if (p->conf.distance != pconf->distance)
				reconf = RECONF_REINIT;

			memcpy(&p->conf, pconf, sizeof(struct peer_config));
			p->conf.reconf_action = reconf;
			if (pconf->reconf_action > reconf)
				p->conf.reconf_action = pconf->reconf_action;

			if (p->state >= STATE_OPENSENT) {
				if (p->holdtime == conf->holdtime &&
				    nconf->holdtime > conf->holdtime)
					p->conf.reconf_action = RECONF_REINIT;
				if (p->holdtime > nconf->holdtime)
					p->conf.reconf_action = RECONF_REINIT;
				if (p->holdtime < nconf->min_holdtime)
					p->conf.reconf_action = RECONF_REINIT;
			}
			break;
		case IMSG_RECONF_DONE:
			if (idx != PFD_PIPE_MAIN)
				fatal("reconf request not from parent", 0);
			if (nconf == NULL)
				fatal("got IMSG_RECONF_DONE but no config", 0);
			conf->as = nconf->as;
			conf->holdtime = nconf->holdtime;
			conf->bgpid = nconf->bgpid;
			conf->min_holdtime = nconf->min_holdtime;
			/* add new peers */
			for (p = nconf->peers; p != NULL; p = next) {
				next = p->next;
				p->next = conf->peers;
				conf->peers = p;
			}
			/* find peers to be deleted */
			for (p = conf->peers; p != NULL; p = p->next)
				if (p->conf.reconf_action == RECONF_NONE)
					p->conf.reconf_action = RECONF_DELETE;
			free(nconf);
			nconf = NULL;
			pending_reconf = 0;
			logit(LOG_INFO, "SE reconfigured");
			break;
		case IMSG_SHUTDOWN_REQUEST:
			session_terminate();
			imsg_compose(&ibuf_main, IMSG_SHUTDOWN_DONE, 0,
			    NULL, 0);
			break;
		default:
		}
		imsg_free(&imsg);
	}
}

struct peer *
getpeerbyip(in_addr_t ip)
{
	struct peer *p;

	/* we might want a more effective way to find peers by IP */
	for (p = conf->peers; p != NULL &&
	    p->conf.remote_addr.sin_addr.s_addr != ip; p = p->next)
		;	/* nothing */

	return (p);
}

void
session_down(struct peer *peer)
{
	imsg_compose(&ibuf_rde, IMSG_SESSION_DOWN, peer->conf.id,
	    NULL, 0);
}

void
session_up(struct peer *peer)
{
	imsg_compose(&ibuf_rde, IMSG_SESSION_UP, peer->conf.id,
	    &peer->remote_bgpid, sizeof(peer->remote_bgpid));
}
