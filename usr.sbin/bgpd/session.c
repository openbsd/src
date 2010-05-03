/*	$OpenBSD: session.c,v 1.308 2010/05/03 13:09:38 claudio Exp $ */

/*
 * Copyright (c) 2003, 2004, 2005 Henning Brauer <henning@openbsd.org>
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
#include <net/if_types.h>
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
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

#define PFD_PIPE_MAIN		0
#define PFD_PIPE_ROUTE		1
#define PFD_PIPE_ROUTE_CTL	2
#define PFD_SOCK_CTL		3
#define PFD_SOCK_RCTL		4
#define PFD_LISTENERS_START	5

void	session_sighdlr(int);
int	setup_listeners(u_int *);
void	init_conf(struct bgpd_config *);
void	init_peer(struct peer *);
void	start_timer_holdtime(struct peer *);
void	start_timer_keepalive(struct peer *);
void	session_close_connection(struct peer *);
void	change_state(struct peer *, enum session_state, enum session_events);
int	session_setup_socket(struct peer *);
void	session_accept(int);
int	session_connect(struct peer *);
void	session_tcp_established(struct peer *);
void	session_capa_ann_none(struct peer *);
int	session_capa_add(struct buf *, u_int8_t, u_int8_t);
int	session_capa_add_mp(struct buf *, u_int8_t);
struct bgp_msg	*session_newmsg(enum msg_type, u_int16_t);
int	session_sendmsg(struct bgp_msg *, struct peer *);
void	session_open(struct peer *);
void	session_keepalive(struct peer *);
void	session_update(u_int32_t, void *, size_t);
void	session_notification(struct peer *, u_int8_t, u_int8_t, void *,
	    ssize_t);
void	session_rrefresh(struct peer *, u_int8_t);
int	session_dispatch_msg(struct pollfd *, struct peer *);
int	parse_header(struct peer *, u_char *, u_int16_t *, u_int8_t *);
int	parse_open(struct peer *);
int	parse_update(struct peer *);
int	parse_refresh(struct peer *);
int	parse_notification(struct peer *);
int	parse_capabilities(struct peer *, u_char *, u_int16_t, u_int32_t *);
int	capa_neg_calc(struct peer *);
void	session_dispatch_imsg(struct imsgbuf *, int, u_int *);
void	session_up(struct peer *);
void	session_down(struct peer *);
void	session_demote(struct peer *, int);

int		 la_cmp(struct listen_addr *, struct listen_addr *);
struct peer	*getpeerbyip(struct sockaddr *);
int		 session_match_mask(struct peer *, struct bgpd_addr *);
struct peer	*getpeerbyid(u_int32_t);

struct bgpd_config	*conf, *nconf;
struct bgpd_sysdep	 sysdep;
struct peer		*peers, *npeers;
volatile sig_atomic_t	 session_quit;
int			 pending_reconf;
int			 csock = -1, rcsock = -1;
u_int			 peer_cnt;
struct imsgbuf		*ibuf_rde;
struct imsgbuf		*ibuf_rde_ctl;
struct imsgbuf		*ibuf_main;

struct mrt_head		 mrthead;

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
setup_listeners(u_int *la_cnt)
{
	int			 ttl = 255;
	int			 opt;
	struct listen_addr	*la;
	u_int			 cnt = 0;

	TAILQ_FOREACH(la, conf->listen_addrs, entry) {
		la->reconf = RECONF_NONE;
		cnt++;

		if (la->flags & LISTENER_LISTENING)
			continue;

		if (la->fd == -1) {
			log_warn("cannot establish listener on %s: invalid fd",
			    log_sockaddr((struct sockaddr *)&la->sa));
			continue;
		}

		opt = 1;
		if (setsockopt(la->fd, IPPROTO_TCP, TCP_MD5SIG,
		    &opt, sizeof(opt)) == -1) {
			if (errno == ENOPROTOOPT) {	/* system w/o md5sig */
				log_warnx("md5sig not available, disabling");
				sysdep.no_md5sig = 1;
			} else
				fatal("setsockopt TCP_MD5SIG");
		}

		/* set ttl to 255 so that ttl-security works */
		if (la->sa.ss_family == AF_INET && setsockopt(la->fd,
		    IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == -1) {
			log_warn("setup_listeners setsockopt TTL");
			continue;
		}

		session_socket_blockmode(la->fd, BM_NONBLOCK);

		if (listen(la->fd, MAX_BACKLOG)) {
			close(la->fd);
			fatal("listen");
		}

		la->flags |= LISTENER_LISTENING;

		log_info("listening on %s",
		    log_sockaddr((struct sockaddr *)&la->sa));
	}

	*la_cnt = cnt;

	return (0);
}

pid_t
session_main(int pipe_m2s[2], int pipe_s2r[2], int pipe_m2r[2], 
    int pipe_s2rctl[2], char *cname, char *rcname)
{
	int			 nfds, timeout;
	unsigned int		 i, j, idx_peers, idx_listeners, idx_mrts;
	pid_t			 pid;
	u_int			 pfd_elms = 0, peer_l_elms = 0, mrt_l_elms = 0;
	u_int			 listener_cnt, ctl_cnt, mrt_cnt;
	u_int			 new_cnt;
	u_int32_t		 ctl_queued;
	struct passwd		*pw;
	struct peer		*p, **peer_l = NULL, *last, *next;
	struct mrt		*m, *xm, **mrt_l = NULL;
	struct pollfd		*pfd = NULL;
	struct ctl_conn		*ctl_conn;
	struct listen_addr	*la;
	void			*newp;
	short			 events;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	/* control socket is outside chroot */
	if ((csock = control_init(0, cname)) == -1)
		fatalx("control socket setup failed");
	if (rcname != NULL && (rcsock = control_init(1, rcname)) == -1)
		fatalx("control socket setup failed");

	if ((pw = getpwnam(BGPD_USER)) == NULL)
		fatal(NULL);

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("session engine");
	bgpd_process = PROC_SE;

	if (pfkey_init(&sysdep) == -1)
		fatalx("pfkey setup failed");

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	signal(SIGTERM, session_sighdlr);
	signal(SIGINT, session_sighdlr);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	close(pipe_m2s[0]);
	close(pipe_s2r[1]);
	close(pipe_s2rctl[1]);
	close(pipe_m2r[0]);
	close(pipe_m2r[1]);
	if ((ibuf_rde = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_rde_ctl = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_rde, pipe_s2r[0]);
	imsg_init(ibuf_rde_ctl, pipe_s2rctl[0]);
	imsg_init(ibuf_main, pipe_m2s[1]);

	TAILQ_INIT(&ctl_conns);
	control_listen(csock);
	control_listen(rcsock);
	LIST_INIT(&mrthead);
	listener_cnt = 0;
	peer_cnt = 0;
	ctl_cnt = 0;

	if ((conf = malloc(sizeof(struct bgpd_config))) == NULL)
		fatal(NULL);
	if ((conf->listen_addrs = calloc(1, sizeof(struct listen_addrs))) ==
	    NULL)
		fatal(NULL);
	TAILQ_INIT(conf->listen_addrs);

	log_info("session engine ready");

	while (session_quit == 0) {
		/* check for peers to be initialized or deleted */
		last = NULL;
		for (p = peers; p != NULL; p = next) {
			next = p->next;
			if (!pending_reconf) {
				/* cloned peer that idled out? */
				if (p->state == STATE_IDLE && p->conf.cloned &&
				    time(NULL) - p->stats.last_updown >=
				    INTERVAL_HOLD_CLONED)
					p->conf.reconf_action = RECONF_DELETE;

				/* new peer that needs init? */
				if (p->state == STATE_NONE)
					init_peer(p);

				/* reinit due? */
				if (p->conf.reconf_action == RECONF_REINIT) {
					session_stop(p, ERR_CEASE_ADMIN_RESET);
					if (!p->conf.down)
						timer_set(p, Timer_IdleHold, 0);
				}

				/* deletion due? */
				if (p->conf.reconf_action == RECONF_DELETE) {
					if (p->demoted)
						session_demote(p, -1);
					p->conf.demote_group[0] = 0;
					session_stop(p, ERR_CEASE_PEER_UNCONF);
					log_peer_warnx(&p->conf, "removed");
					if (last != NULL)
						last->next = next;
					else
						peers = next;
					timer_remove_all(p);
					free(p);
					peer_cnt--;
					continue;
				}
				p->conf.reconf_action = RECONF_NONE;
			}
			last = p;
		}

		if (peer_cnt > peer_l_elms) {
			if ((newp = realloc(peer_l, sizeof(struct peer *) *
			    peer_cnt)) == NULL) {
				/* panic for now  */
				log_warn("could not resize peer_l from %u -> %u"
				    " entries", peer_l_elms, peer_cnt);
				fatalx("exiting");
			}
			peer_l = newp;
			peer_l_elms = peer_cnt;
		}

		mrt_cnt = 0;
		for (m = LIST_FIRST(&mrthead); m != NULL; m = xm) {
			xm = LIST_NEXT(m, entry);
			if (m->state == MRT_STATE_REMOVE) {
				mrt_clean(m);
				LIST_REMOVE(m, entry);
				free(m);
				continue;
			}
			if (m->wbuf.queued)
				mrt_cnt++;
		}

		if (mrt_cnt > mrt_l_elms) {
			if ((newp = realloc(mrt_l, sizeof(struct mrt *) *
			    mrt_cnt)) == NULL) {
				/* panic for now  */
				log_warn("could not resize mrt_l from %u -> %u"
				    " entries", mrt_l_elms, mrt_cnt);
				fatalx("exiting");
			}
			mrt_l = newp;
			mrt_l_elms = mrt_cnt;
		}

		new_cnt = PFD_LISTENERS_START + listener_cnt + peer_cnt +
		    ctl_cnt + mrt_cnt;
		if (new_cnt > pfd_elms) {
			if ((newp = realloc(pfd, sizeof(struct pollfd) *
			    new_cnt)) == NULL) {
				/* panic for now  */
				log_warn("could not resize pfd from %u -> %u"
				    " entries", pfd_elms, new_cnt);
				fatalx("exiting");
			}
			pfd = newp;
			pfd_elms = new_cnt;
		}

		bzero(pfd, sizeof(struct pollfd) * pfd_elms);
		pfd[PFD_PIPE_MAIN].fd = ibuf_main->fd;
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		if (ibuf_main->w.queued > 0)
			pfd[PFD_PIPE_MAIN].events |= POLLOUT;
		pfd[PFD_PIPE_ROUTE].fd = ibuf_rde->fd;
		pfd[PFD_PIPE_ROUTE].events = POLLIN;
		if (ibuf_rde->w.queued > 0)
			pfd[PFD_PIPE_ROUTE].events |= POLLOUT;

		ctl_queued = 0;
		TAILQ_FOREACH(ctl_conn, &ctl_conns, entry)
			ctl_queued += ctl_conn->ibuf.w.queued;

		pfd[PFD_PIPE_ROUTE_CTL].fd = ibuf_rde_ctl->fd;
		if (ctl_queued < SESSION_CTL_QUEUE_MAX)
			/*
			 * Do not act as unlimited buffer. Don't read in more
			 * messages if the ctl sockets are getting full. 
			 */
			pfd[PFD_PIPE_ROUTE_CTL].events = POLLIN;
		pfd[PFD_SOCK_CTL].fd = csock;
		pfd[PFD_SOCK_CTL].events = POLLIN;
		pfd[PFD_SOCK_RCTL].fd = rcsock;
		pfd[PFD_SOCK_RCTL].events = POLLIN;

		i = PFD_LISTENERS_START;
		TAILQ_FOREACH(la, conf->listen_addrs, entry) {
			pfd[i].fd = la->fd;
			pfd[i].events = POLLIN;
			i++;
		}
		idx_listeners = i;
		timeout = 240;	/* loop every 240s at least */

		for (p = peers; p != NULL; p = p->next) {
			time_t	nextaction;
			struct peer_timer *pt;

			/* check timers */
			if ((pt = timer_nextisdue(p)) != NULL) {
				switch (pt->type) {
				case Timer_Hold:
					bgp_fsm(p, EVNT_TIMER_HOLDTIME);
					break;
				case Timer_ConnectRetry:
					bgp_fsm(p, EVNT_TIMER_CONNRETRY);
					break;
				case Timer_Keepalive:
					bgp_fsm(p, EVNT_TIMER_KEEPALIVE);
					break;
				case Timer_IdleHold:
					bgp_fsm(p, EVNT_START);
					break;
				case Timer_IdleHoldReset:
					p->IdleHoldTime /= 2;
					if (p->IdleHoldTime <=
					    INTERVAL_IDLE_HOLD_INITIAL) {
						p->IdleHoldTime =
						    INTERVAL_IDLE_HOLD_INITIAL;
						timer_stop(p,
						    Timer_IdleHoldReset);
						p->errcnt = 0;
					} else
						timer_set(p,
						    Timer_IdleHoldReset,
						    p->IdleHoldTime);
					break;
				case Timer_CarpUndemote:
					timer_stop(p, Timer_CarpUndemote);
					if (p->demoted &&
					    p->state == STATE_ESTABLISHED)
						session_demote(p, -1);
					break;
				default:
					fatalx("King Bula lost in time");
				}
			}
			if ((nextaction = timer_nextduein(p)) != -1 &&
			    nextaction < timeout)
				timeout = nextaction;

			/* are we waiting for a write? */
			events = POLLIN;
			if (p->wbuf.queued > 0 || p->state == STATE_CONNECT)
				events |= POLLOUT;

			/* poll events */
			if (p->fd != -1 && events != 0) {
				pfd[i].fd = p->fd;
				pfd[i].events = events;
				peer_l[i - idx_listeners] = p;
				i++;
			}
		}

		idx_peers = i;

		LIST_FOREACH(m, &mrthead, entry)
			if (m->wbuf.queued) {
				pfd[i].fd = m->wbuf.fd;
				pfd[i].events = POLLOUT;
				mrt_l[i - idx_peers] = m;
				i++;
			}

		idx_mrts = i;

		TAILQ_FOREACH(ctl_conn, &ctl_conns, entry) {
			pfd[i].fd = ctl_conn->ibuf.fd;
			pfd[i].events = POLLIN;
			if (ctl_conn->ibuf.w.queued > 0)
				pfd[i].events |= POLLOUT;
			i++;
		}

		if (timeout < 0)
			timeout = 0;
		if ((nfds = poll(pfd, i, timeout * 1000)) == -1)
			if (errno != EINTR)
				fatal("poll error");

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & POLLOUT)
			if (msgbuf_write(&ibuf_main->w) < 0)
				fatal("pipe write error");

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & POLLIN) {
			nfds--;
			session_dispatch_imsg(ibuf_main, PFD_PIPE_MAIN,
			    &listener_cnt);
		}

		if (nfds > 0 && pfd[PFD_PIPE_ROUTE].revents & POLLOUT)
			if (msgbuf_write(&ibuf_rde->w) < 0)
				fatal("pipe write error");

		if (nfds > 0 && pfd[PFD_PIPE_ROUTE].revents & POLLIN) {
			nfds--;
			session_dispatch_imsg(ibuf_rde, PFD_PIPE_ROUTE,
			    &listener_cnt);
		}

		if (nfds > 0 && pfd[PFD_PIPE_ROUTE_CTL].revents & POLLIN) {
			nfds--;
			session_dispatch_imsg(ibuf_rde_ctl, PFD_PIPE_ROUTE_CTL,
			    &listener_cnt);
		}

		if (nfds > 0 && pfd[PFD_SOCK_CTL].revents & POLLIN) {
			nfds--;
			ctl_cnt += control_accept(csock, 0);
		}

		if (nfds > 0 && pfd[PFD_SOCK_RCTL].revents & POLLIN) {
			nfds--;
			ctl_cnt += control_accept(rcsock, 1);
		}

		for (j = PFD_LISTENERS_START; nfds > 0 && j < idx_listeners;
		    j++)
			if (pfd[j].revents & POLLIN) {
				nfds--;
				session_accept(pfd[j].fd);
			}

		for (; nfds > 0 && j < idx_peers; j++)
			nfds -= session_dispatch_msg(&pfd[j],
			    peer_l[j - idx_listeners]);

		for (; nfds > 0 && j < idx_mrts; j++)
			if (pfd[j].revents & POLLOUT) {
				nfds--;
				mrt_write(mrt_l[j - idx_peers]);
			}

		for (; nfds > 0 && j < i; j++)
			nfds -= control_dispatch_msg(&pfd[j], &ctl_cnt);
	}

	while ((p = peers) != NULL) {
		peers = p->next;
		session_stop(p, ERR_CEASE_ADMIN_DOWN);
		pfkey_remove(p);
		free(p);
	}

	while ((m = LIST_FIRST(&mrthead)) != NULL) {
		mrt_clean(m);
		LIST_REMOVE(m, entry);
		free(m);
	}

	while ((la = TAILQ_FIRST(conf->listen_addrs)) != NULL) {
		TAILQ_REMOVE(conf->listen_addrs, la, entry);
		free(la);
	}
	free(conf->listen_addrs);
	free(peer_l);
	free(mrt_l);
	free(pfd);

	msgbuf_write(&ibuf_rde->w);
	msgbuf_clear(&ibuf_rde->w);
	free(ibuf_rde);
	msgbuf_write(&ibuf_main->w);
	msgbuf_clear(&ibuf_main->w);
	free(ibuf_main);

	control_shutdown(csock);
	control_shutdown(rcsock);
	log_info("session engine exiting");
	_exit(0);
}

void
init_conf(struct bgpd_config *c)
{
	if (!c->holdtime)
		c->holdtime = INTERVAL_HOLD;
	if (!c->connectretry)
		c->connectretry = INTERVAL_CONNECTRETRY;
}

void
init_peer(struct peer *p)
{
	TAILQ_INIT(&p->timers);
	p->fd = p->wbuf.fd = -1;

	if (p->conf.if_depend[0])
		imsg_compose(ibuf_main, IMSG_IFINFO, 0, 0, -1,
		    p->conf.if_depend, sizeof(p->conf.if_depend));
	else
		p->depend_ok = 1;

	peer_cnt++;

	change_state(p, STATE_IDLE, EVNT_NONE);
	if (p->conf.down)
		timer_stop(p, Timer_IdleHold);		/* no autostart */
	else
		timer_set(p, Timer_IdleHold, 0);	/* start ASAP */

	/*
	 * on startup, demote if requested.
	 * do not handle new peers. they must reach ESTABLISHED beforehands.
	 * peers added at runtime have reconf_action set to RECONF_REINIT.
	 */
	if (p->conf.reconf_action != RECONF_REINIT && p->conf.demote_group[0])
		session_demote(p, +1);
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
			timer_stop(peer, Timer_Hold);
			timer_stop(peer, Timer_Keepalive);
			timer_stop(peer, Timer_IdleHold);

			/* allocate read buffer */
			peer->rbuf = calloc(1, sizeof(struct buf_read));
			if (peer->rbuf == NULL)
				fatal(NULL);

			/* init write buffer */
			msgbuf_init(&peer->wbuf);

			/* init pfkey - remove old if any, load new ones */
			pfkey_remove(peer);
			if (pfkey_establish(peer) == -1) {
				log_peer_warnx(&peer->conf,
				    "pfkey setup failed");
				return;
			}

			peer->stats.last_sent_errcode = 0;
			peer->stats.last_sent_suberr = 0;

			if (!peer->depend_ok)
				timer_stop(peer, Timer_ConnectRetry);
			else if (peer->passive || peer->conf.passive ||
			    peer->conf.template) {
				change_state(peer, STATE_ACTIVE, event);
				timer_stop(peer, Timer_ConnectRetry);
			} else {
				change_state(peer, STATE_CONNECT, event);
				timer_set(peer, Timer_ConnectRetry,
				    conf->connectretry);
				session_connect(peer);
			}
			peer->passive = 0;
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
			timer_stop(peer, Timer_ConnectRetry);
			peer->holdtime = INTERVAL_HOLD_INITIAL;
			start_timer_holdtime(peer);
			change_state(peer, STATE_OPENSENT, event);
			break;
		case EVNT_CON_OPENFAIL:
			timer_set(peer, Timer_ConnectRetry,
			    conf->connectretry);
			session_close_connection(peer);
			change_state(peer, STATE_ACTIVE, event);
			break;
		case EVNT_TIMER_CONNRETRY:
			timer_set(peer, Timer_ConnectRetry,
			    conf->connectretry);
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
			timer_stop(peer, Timer_ConnectRetry);
			peer->holdtime = INTERVAL_HOLD_INITIAL;
			start_timer_holdtime(peer);
			change_state(peer, STATE_OPENSENT, event);
			break;
		case EVNT_CON_OPENFAIL:
			timer_set(peer, Timer_ConnectRetry,
			    conf->connectretry);
			session_close_connection(peer);
			change_state(peer, STATE_ACTIVE, event);
			break;
		case EVNT_TIMER_CONNRETRY:
			timer_set(peer, Timer_ConnectRetry,
			    peer->holdtime);
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
			change_state(peer, STATE_IDLE, event);
			break;
		case EVNT_CON_CLOSED:
			session_close_connection(peer);
			timer_set(peer, Timer_ConnectRetry,
			    conf->connectretry);
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
				timer_set(peer, Timer_IdleHold, 0);
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

void
start_timer_holdtime(struct peer *peer)
{
	if (peer->holdtime > 0)
		timer_set(peer, Timer_Hold, peer->holdtime);
	else
		timer_stop(peer, Timer_Hold);
}

void
start_timer_keepalive(struct peer *peer)
{
	if (peer->holdtime > 0)
		timer_set(peer, Timer_Keepalive, peer->holdtime / 3);
	else
		timer_stop(peer, Timer_Keepalive);
}

void
session_close_connection(struct peer *peer)
{
	if (peer->fd != -1)
		close(peer->fd);

	peer->fd = peer->wbuf.fd = -1;
}

void
change_state(struct peer *peer, enum session_state state,
    enum session_events event)
{
	struct mrt	*mrt;

	switch (state) {
	case STATE_IDLE:
		/* carp demotion first. new peers handled in init_peer */
		if (peer->state == STATE_ESTABLISHED &&
		    peer->conf.demote_group[0] && !peer->demoted)
			session_demote(peer, +1);

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
		timer_stop(peer, Timer_ConnectRetry);
		timer_stop(peer, Timer_Keepalive);
		timer_stop(peer, Timer_Hold);
		timer_stop(peer, Timer_IdleHold);
		timer_stop(peer, Timer_IdleHoldReset);
		session_close_connection(peer);
		msgbuf_clear(&peer->wbuf);
		free(peer->rbuf);
		peer->rbuf = NULL;
		bzero(&peer->capa.peer, sizeof(peer->capa.peer));
		if (peer->state == STATE_ESTABLISHED)
			session_down(peer);
		if (event != EVNT_STOP) {
			timer_set(peer, Timer_IdleHold, peer->IdleHoldTime);
			if (event != EVNT_NONE &&
			    peer->IdleHoldTime < MAX_IDLE_HOLD/2)
				peer->IdleHoldTime *= 2;
		}
		if (peer->state == STATE_NONE ||
		    peer->state == STATE_ESTABLISHED) {
			/* initialize capability negotiation structures */
			memcpy(&peer->capa.ann, &peer->conf.capabilities,
			    sizeof(peer->capa.ann));
			if (!peer->conf.announce_capa)
				session_capa_ann_none(peer);
		}
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
		timer_set(peer, Timer_IdleHoldReset, peer->IdleHoldTime);
		if (peer->demoted)
			timer_set(peer, Timer_CarpUndemote,
			    INTERVAL_HOLD_DEMOTED);
		session_up(peer);
		break;
	default:		/* something seriously fucked */
		break;
	}

	log_statechange(peer, state, event);
	LIST_FOREACH(mrt, &mrthead, entry) {
		if (!(mrt->type == MRT_ALL_IN || mrt->type == MRT_ALL_OUT))
			continue;
		if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
		    mrt->peer_id == peer->conf.id || (mrt->group_id != 0 &&
		    mrt->group_id == peer->conf.groupid))
			mrt_dump_state(mrt, peer->state, state, peer);
	}
	peer->prev_state = peer->state;
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

	if (p != NULL && p->state == STATE_IDLE && p->errcnt < 2) {
		if (timer_running(p, Timer_IdleHold, NULL)) {
			/* fast reconnect after clear */
			p->passive = 1;
			bgp_fsm(p, EVNT_START);
		}
	}

	if (p != NULL &&
	    (p->state == STATE_CONNECT || p->state == STATE_ACTIVE)) {
		if (p->fd != -1) {
			if (p->state == STATE_CONNECT)
				session_close_connection(p);
			else {
				close(connfd);
				return;
			}
		}

		if (p->conf.auth.method != AUTH_NONE && sysdep.no_pfkey) {
			log_peer_warnx(&p->conf,
			    "ipsec or md5sig configured but not available");
			close(connfd);
			return;
		}

		if (p->conf.auth.method == AUTH_MD5SIG) {
			if (sysdep.no_md5sig) {
				log_peer_warnx(&p->conf,
				    "md5sig configured but not available");
				close(connfd);
				return;
			}
			len = sizeof(opt);
			if (getsockopt(connfd, IPPROTO_TCP, TCP_MD5SIG,
			    &opt, &len) == -1)
				fatal("getsockopt TCP_MD5SIG");
			if (!opt) {	/* non-md5'd connection! */
				log_peer_warnx(&p->conf,
				    "connection attempt without md5 signature");
				close(connfd);
				return;
			}
		}
		p->fd = p->wbuf.fd = connfd;
		if (session_setup_socket(p)) {
			close(connfd);
			return;
		}
		session_socket_blockmode(connfd, BM_NONBLOCK);
		bgp_fsm(p, EVNT_CON_OPEN);
	} else {
		log_conn_attempt(p, (struct sockaddr *)&cliaddr);
		close(connfd);
	}
}

int
session_connect(struct peer *peer)
{
	int			 opt = 1;
	struct sockaddr		*sa;

	/*
	 * we do not need the overcomplicated collision detection RFC 1771
	 * describes; we simply make sure there is only ever one concurrent
	 * tcp connection per peer.
	 */
	if (peer->fd != -1)
		return (-1);

	if ((peer->fd = socket(aid2af(peer->conf.remote_addr.aid), SOCK_STREAM,
	    IPPROTO_TCP)) == -1) {
		log_peer_warn(&peer->conf, "session_connect socket");
		bgp_fsm(peer, EVNT_CON_OPENFAIL);
		return (-1);
	}

	if (peer->conf.auth.method != AUTH_NONE && sysdep.no_pfkey) {
		log_peer_warnx(&peer->conf,
		    "ipsec or md5sig configured but not available");
		bgp_fsm(peer, EVNT_CON_OPENFAIL);
		return (-1);
	}

	if (peer->conf.auth.method == AUTH_MD5SIG) {
		if (sysdep.no_md5sig) {
			log_peer_warnx(&peer->conf,
			    "md5sig configured but not available");
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (-1);
		}
		if (setsockopt(peer->fd, IPPROTO_TCP, TCP_MD5SIG,
		    &opt, sizeof(opt)) == -1) {
			log_peer_warn(&peer->conf, "setsockopt md5sig");
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (-1);
		}
	}
	peer->wbuf.fd = peer->fd;

	/* if update source is set we need to bind() */
	if ((sa = addr2sa(&peer->conf.local_addr, 0)) != NULL) {
		if (bind(peer->fd, sa, sa->sa_len) == -1) {
			log_peer_warn(&peer->conf, "session_connect bind");
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (-1);
		}
	}

	if (session_setup_socket(peer)) {
		bgp_fsm(peer, EVNT_CON_OPENFAIL);
		return (-1);
	}

	session_socket_blockmode(peer->fd, BM_NONBLOCK);

	sa = addr2sa(&peer->conf.remote_addr, BGP_PORT);
	if (connect(peer->fd, sa, sa->sa_len) == -1) {
		if (errno != EINPROGRESS) {
			if (errno != peer->lasterr)
				log_peer_warn(&peer->conf, "connect");
			peer->lasterr = errno;
			bgp_fsm(peer, EVNT_CON_OPENFAIL);
			return (-1);
		}
	} else
		bgp_fsm(peer, EVNT_CON_OPEN);

	return (0);
}

int
session_setup_socket(struct peer *p)
{
	int	ttl = p->conf.distance;
	int	pre = IPTOS_PREC_INTERNETCONTROL;
	int	nodelay = 1;
	int	bsize;

	switch (p->conf.remote_addr.aid) {
	case AID_INET:
		/* set precedence, see RFC 1771 appendix 5 */
		if (setsockopt(p->fd, IPPROTO_IP, IP_TOS, &pre, sizeof(pre)) ==
		    -1) {
			log_peer_warn(&p->conf,
			    "session_setup_socket setsockopt TOS");
			return (-1);
		}

		if (p->conf.ebgp) {
			/* set TTL to foreign router's distance
			   1=direct n=multihop with ttlsec, we always use 255 */
			if (p->conf.ttlsec) {
				ttl = 256 - p->conf.distance;
				if (setsockopt(p->fd, IPPROTO_IP, IP_MINTTL,
				    &ttl, sizeof(ttl)) == -1) {
					log_peer_warn(&p->conf,
					    "session_setup_socket: "
					    "setsockopt MINTTL");
					return (-1);
				}
				ttl = 255;
			}

			if (setsockopt(p->fd, IPPROTO_IP, IP_TTL, &ttl,
			    sizeof(ttl)) == -1) {
				log_peer_warn(&p->conf,
				    "session_setup_socket setsockopt TTL");
				return (-1);
			}
		}
		break;
	case AID_INET6:
		if (p->conf.ebgp) {
			/* set hoplimit to foreign router's distance */
			if (setsockopt(p->fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS,
			    &ttl, sizeof(ttl)) == -1) {
				log_peer_warn(&p->conf,
				    "session_setup_socket setsockopt hoplimit");
				return (-1);
			}
		}
		break;
	}

	/* set TCP_NODELAY */
	if (setsockopt(p->fd, IPPROTO_TCP, TCP_NODELAY, &nodelay,
	    sizeof(nodelay)) == -1) {
		log_peer_warn(&p->conf,
		    "session_setup_socket setsockopt TCP_NODELAY");
		return (-1);
	}

	/* only increase bufsize (and thus window) if md5 or ipsec is in use */
	if (p->conf.auth.method != AUTH_NONE) {
		/* try to increase bufsize. no biggie if it fails */
		bsize = 65535;
		while (setsockopt(p->fd, SOL_SOCKET, SO_RCVBUF, &bsize,
		    sizeof(bsize)) == -1)
			bsize /= 2;
		bsize = 65535;
		while (setsockopt(p->fd, SOL_SOCKET, SO_SNDBUF, &bsize,
		    sizeof(bsize)) == -1)
			bsize /= 2;
	}

	return (0);
}

void
session_socket_blockmode(int fd, enum blockmodes bm)
{
	int	flags;

	if ((flags = fcntl(fd, F_GETFL, 0)) == -1)
		fatal("fcntl F_GETFL");

	if (bm == BM_NONBLOCK)
		flags |= O_NONBLOCK;
	else
		flags &= ~O_NONBLOCK;

	if ((flags = fcntl(fd, F_SETFL, flags)) == -1)
		fatal("fcntl F_SETFL");
}

void
session_tcp_established(struct peer *peer)
{
	socklen_t	len;

	len = sizeof(peer->sa_local);
	if (getsockname(peer->fd, (struct sockaddr *)&peer->sa_local,
	    &len) == -1)
		log_warn("getsockname");
	len = sizeof(peer->sa_remote);
	if (getpeername(peer->fd, (struct sockaddr *)&peer->sa_remote,
	    &len) == -1)
		log_warn("getpeername");
}

void
session_capa_ann_none(struct peer *peer)
{
	bzero(&peer->capa.ann, sizeof(peer->capa.ann));
}

int
session_capa_add(struct buf *opb, u_int8_t capa_code, u_int8_t capa_len)
{
	int errs = 0;

	errs += buf_add(opb, &capa_code, sizeof(capa_code));
	errs += buf_add(opb, &capa_len, sizeof(capa_len));
	return (errs);
}

int
session_capa_add_mp(struct buf *buf, u_int8_t aid)
{
	u_int8_t		 safi, pad = 0;
	u_int16_t		 afi;
	int			 errs = 0;

	if (aid2afi(aid, &afi, &safi) == -1)
		fatalx("session_capa_add_mp: bad afi/safi pair");
	afi = htons(afi);
	errs += buf_add(buf, &afi, sizeof(afi));
	errs += buf_add(buf, &pad, sizeof(pad));
	errs += buf_add(buf, &safi, sizeof(safi));

	return (errs);
}

struct bgp_msg *
session_newmsg(enum msg_type msgtype, u_int16_t len)
{
	struct bgp_msg		*msg;
	struct msg_header	 hdr;
	struct buf		*buf;
	int			 errs = 0;

	memset(&hdr.marker, 0xff, sizeof(hdr.marker));
	hdr.len = htons(len);
	hdr.type = msgtype;

	if ((buf = buf_open(len)) == NULL)
		return (NULL);

	errs += buf_add(buf, &hdr.marker, sizeof(hdr.marker));
	errs += buf_add(buf, &hdr.len, sizeof(hdr.len));
	errs += buf_add(buf, &hdr.type, sizeof(hdr.type));

	if (errs > 0 ||
	    (msg = calloc(1, sizeof(*msg))) == NULL) {
		buf_free(buf);
		return (NULL);
	}

	msg->buf = buf;
	msg->type = msgtype;
	msg->len = len;

	return (msg);
}

int
session_sendmsg(struct bgp_msg *msg, struct peer *p)
{
	struct mrt		*mrt;

	LIST_FOREACH(mrt, &mrthead, entry) {
		if (!(mrt->type == MRT_ALL_OUT || (msg->type == UPDATE &&
		    mrt->type == MRT_UPDATE_OUT)))
			continue;
		if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
		    mrt->peer_id == p->conf.id || (mrt->group_id == 0 &&
		    mrt->group_id == p->conf.groupid))
			mrt_dump_bgp_msg(mrt, msg->buf->buf, msg->len, p);
	}

	buf_close(&p->wbuf, msg->buf);
	free(msg);
	return (0);
}

void
session_open(struct peer *p)
{
	struct bgp_msg		*buf;
	struct buf		*opb;
	struct msg_open		 msg;
	u_int16_t		 len;
	u_int8_t		 i, op_type, optparamlen = 0;
	u_int			 errs = 0;


	if ((opb = buf_dynamic(0, UCHAR_MAX - sizeof(op_type) -
	    sizeof(optparamlen))) == NULL) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	/* multiprotocol extensions, RFC 4760 */
	for (i = 0; i < AID_MAX; i++)
		if (p->capa.ann.mp[i]) {	/* 4 bytes data */
			errs += session_capa_add(opb, CAPA_MP, 4);
			errs += session_capa_add_mp(opb, i);
		}

	/* route refresh, RFC 2918 */
	if (p->capa.ann.refresh)	/* no data */
		errs += session_capa_add(opb, CAPA_REFRESH, 0);

	/* End-of-RIB marker, RFC 4724 */
	if (p->capa.ann.restart) {	/* 2 bytes data */
		u_char		c[2];

		c[0] = 0x80; /* we're always restarting */
		c[1] = 0;
		errs += session_capa_add(opb, CAPA_RESTART, 2);
		errs += buf_add(opb, &c, 2);
	}

	/* 4-bytes AS numbers, draft-ietf-idr-as4bytes-13 */
	if (p->capa.ann.as4byte) {	/* 4 bytes data */
		u_int32_t	nas;

		nas = htonl(conf->as);
		errs += session_capa_add(opb, CAPA_AS4BYTE, sizeof(nas));
		errs += buf_add(opb, &nas, sizeof(nas));
	}

	if (buf_size(opb))
		optparamlen = buf_size(opb) + sizeof(op_type) +
		    sizeof(optparamlen);

	len = MSGSIZE_OPEN_MIN + optparamlen;
	if (errs || (buf = session_newmsg(OPEN, len)) == NULL) {
		buf_free(opb);
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	msg.version = 4;
	msg.myas = htons(conf->short_as);
	if (p->conf.holdtime)
		msg.holdtime = htons(p->conf.holdtime);
	else
		msg.holdtime = htons(conf->holdtime);
	msg.bgpid = conf->bgpid;	/* is already in network byte order */
	msg.optparamlen = optparamlen;

	errs += buf_add(buf->buf, &msg.version, sizeof(msg.version));
	errs += buf_add(buf->buf, &msg.myas, sizeof(msg.myas));
	errs += buf_add(buf->buf, &msg.holdtime, sizeof(msg.holdtime));
	errs += buf_add(buf->buf, &msg.bgpid, sizeof(msg.bgpid));
	errs += buf_add(buf->buf, &msg.optparamlen, sizeof(msg.optparamlen));

	if (optparamlen) {
		op_type = OPT_PARAM_CAPABILITIES;
		optparamlen = buf_size(opb);
		errs += buf_add(buf->buf, &op_type, sizeof(op_type));
		errs += buf_add(buf->buf, &optparamlen, sizeof(optparamlen));
		errs += buf_add(buf->buf, opb->buf, buf_size(opb));
	}

	buf_free(opb);

	if (errs > 0) {
		buf_free(buf->buf);
		free(buf);
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	if (session_sendmsg(buf, p) == -1) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	p->stats.msg_sent_open++;
}

void
session_keepalive(struct peer *p)
{
	struct bgp_msg		*buf;

	if ((buf = session_newmsg(KEEPALIVE, MSGSIZE_KEEPALIVE)) == NULL ||
	    session_sendmsg(buf, p) == -1) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	start_timer_keepalive(p);
	p->stats.msg_sent_keepalive++;
}

void
session_update(u_int32_t peerid, void *data, size_t datalen)
{
	struct peer		*p;
	struct bgp_msg		*buf;

	if ((p = getpeerbyid(peerid)) == NULL) {
		log_warnx("no such peer: id=%u", peerid);
		return;
	}

	if (p->state != STATE_ESTABLISHED)
		return;

	if ((buf = session_newmsg(UPDATE, MSGSIZE_HEADER + datalen)) == NULL) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	if (buf_add(buf->buf, data, datalen)) {
		buf_free(buf->buf);
		free(buf);
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	if (session_sendmsg(buf, p) == -1) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	start_timer_keepalive(p);
	p->stats.msg_sent_update++;
}

void
session_notification(struct peer *p, u_int8_t errcode, u_int8_t subcode,
    void *data, ssize_t datalen)
{
	struct bgp_msg		*buf;
	u_int			 errs = 0;

	if (p->stats.last_sent_errcode)	/* some notification already sent */
		return;

	if ((buf = session_newmsg(NOTIFICATION,
	    MSGSIZE_NOTIFICATION_MIN + datalen)) == NULL) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	errs += buf_add(buf->buf, &errcode, sizeof(errcode));
	errs += buf_add(buf->buf, &subcode, sizeof(subcode));

	if (datalen > 0)
		errs += buf_add(buf->buf, data, datalen);

	if (errs > 0) {
		buf_free(buf->buf);
		free(buf);
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	if (session_sendmsg(buf, p) == -1) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	p->stats.msg_sent_notification++;
	p->stats.last_sent_errcode = errcode;
	p->stats.last_sent_suberr = subcode;
}

int
session_neighbor_rrefresh(struct peer *p)
{
	u_int8_t	i;

	if (!p->capa.peer.refresh)
		return (-1);

	for (i = 0; i < AID_MAX; i++) {
		if (p->capa.peer.mp[i] != 0)
			session_rrefresh(p, i);
	}

	return (0);
}

void
session_rrefresh(struct peer *p, u_int8_t aid)
{
	struct bgp_msg		*buf;
	int			 errs = 0;
	u_int16_t		 afi;
	u_int8_t		 safi, null8 = 0;

	if (aid2afi(aid, &afi, &safi) == -1)
		fatalx("session_rrefresh: bad afi/safi pair");

	if ((buf = session_newmsg(RREFRESH, MSGSIZE_RREFRESH)) == NULL) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	afi = htons(afi);
	errs += buf_add(buf->buf, &afi, sizeof(afi));
	errs += buf_add(buf->buf, &null8, sizeof(null8));
	errs += buf_add(buf->buf, &safi, sizeof(safi));

	if (errs > 0) {
		buf_free(buf->buf);
		free(buf);
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	if (session_sendmsg(buf, p) == -1) {
		bgp_fsm(p, EVNT_CON_FATAL);
		return;
	}

	p->stats.msg_sent_rrefresh++;
}

int
session_dispatch_msg(struct pollfd *pfd, struct peer *p)
{
	ssize_t		n, rpos, av, left;
	socklen_t	len;
	int		error, processed = 0;
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
					if (errno != p->lasterr) {
						log_peer_warn(&p->conf,
						    "socket error");
						p->lasterr = errno;
					}
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

	if (p->rbuf && pfd->revents & POLLIN) {
		if ((n = read(p->fd, p->rbuf->buf + p->rbuf->wpos,
		    sizeof(p->rbuf->buf) - p->rbuf->wpos)) == -1) {
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
			if (parse_header(p, p->rbuf->buf + rpos, &msglen,
			    &msgtype) == -1)
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
			case RREFRESH:
				parse_refresh(p);
				p->stats.msg_rcvd_rrefresh++;
				break;
			default:	/* cannot happen */
				session_notification(p, ERR_HEADER,
				    ERR_HDR_TYPE, &msgtype, 1);
				log_warnx("received message with "
				    "unknown type %u", msgtype);
				bgp_fsm(p, EVNT_CON_FATAL);
			}
			rpos += msglen;
			if (++processed > MSG_PROCESS_LIMIT)
				break;
		}
		if (p->rbuf == NULL)
			return (1);

		if (rpos < av) {
			left = av - rpos;
			memcpy(&p->rbuf->buf, p->rbuf->buf + rpos, left);
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
	struct mrt		*mrt;
	u_char			*p;
	u_int16_t		 olen;
	static const u_int8_t	 marker[MSGSIZE_HEADER_MARKER] = { 0xff, 0xff,
				    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
				    0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };

	/* caller MUST make sure we are getting 19 bytes! */
	p = data;
	if (memcmp(p, marker, sizeof(marker))) {
		log_peer_warnx(&peer->conf, "sync error");
		session_notification(peer, ERR_HEADER, ERR_HDR_SYNC, NULL, 0);
		bgp_fsm(peer, EVNT_CON_FATAL);
		return (-1);
	}
	p += MSGSIZE_HEADER_MARKER;

	memcpy(&olen, p, 2);
	*len = ntohs(olen);
	p += 2;
	memcpy(type, p, 1);

	if (*len < MSGSIZE_HEADER || *len > MAX_PKTSIZE) {
		log_peer_warnx(&peer->conf,
		    "received message: illegal length: %u byte", *len);
		session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
		    &olen, sizeof(olen));
		bgp_fsm(peer, EVNT_CON_FATAL);
		return (-1);
	}

	switch (*type) {
	case OPEN:
		if (*len < MSGSIZE_OPEN_MIN) {
			log_peer_warnx(&peer->conf,
			    "received OPEN: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			bgp_fsm(peer, EVNT_CON_FATAL);
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
			bgp_fsm(peer, EVNT_CON_FATAL);
			return (-1);
		}
		break;
	case UPDATE:
		if (*len < MSGSIZE_UPDATE_MIN) {
			log_peer_warnx(&peer->conf,
			    "received UPDATE: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			bgp_fsm(peer, EVNT_CON_FATAL);
			return (-1);
		}
		break;
	case KEEPALIVE:
		if (*len != MSGSIZE_KEEPALIVE) {
			log_peer_warnx(&peer->conf,
			    "received KEEPALIVE: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			bgp_fsm(peer, EVNT_CON_FATAL);
			return (-1);
		}
		break;
	case RREFRESH:
		if (*len != MSGSIZE_RREFRESH) {
			log_peer_warnx(&peer->conf,
			    "received RREFRESH: illegal len: %u byte", *len);
			session_notification(peer, ERR_HEADER, ERR_HDR_LEN,
			    &olen, sizeof(olen));
			bgp_fsm(peer, EVNT_CON_FATAL);
			return (-1);
		}
		break;
	default:
		log_peer_warnx(&peer->conf,
		    "received msg with unknown type %u", *type);
		session_notification(peer, ERR_HEADER, ERR_HDR_TYPE,
		    type, 1);
		bgp_fsm(peer, EVNT_CON_FATAL);
		return (-1);
	}
	LIST_FOREACH(mrt, &mrthead, entry) {
		if (!(mrt->type == MRT_ALL_IN || (*type == UPDATE &&
		    mrt->type == MRT_UPDATE_IN)))
			continue;
		if ((mrt->peer_id == 0 && mrt->group_id == 0) ||
		    mrt->peer_id == peer->conf.id || (mrt->group_id != 0 &&
		    mrt->group_id == peer->conf.groupid))
			mrt_dump_bgp_msg(mrt, data, *len, peer);
	}
	return (0);
}

int
parse_open(struct peer *peer)
{
	u_char		*p, *op_val;
	u_int8_t	 version, rversion;
	u_int16_t	 short_as, msglen;
	u_int16_t	 holdtime, oholdtime, myholdtime;
	u_int32_t	 as, bgpid;
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

	memcpy(&short_as, p, sizeof(short_as));
	p += sizeof(short_as);
	as = peer->short_as = ntohs(short_as);

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

	/* check bgpid for validity - just disallow 0 */
	if (ntohl(bgpid) == 0) {
		log_peer_warnx(&peer->conf, "peer BGPID %lu unacceptable",
		    ntohl(bgpid));
		session_notification(peer, ERR_OPEN, ERR_OPEN_BGPID,
		    NULL, 0);
		change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
		return (-1);
	}
	peer->remote_bgpid = bgpid;

	memcpy(&optparamlen, p, sizeof(optparamlen));
	p += sizeof(optparamlen);

	if (optparamlen != msglen - MSGSIZE_OPEN_MIN) {
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
		p += sizeof(op_len);
		plen -= sizeof(op_len);
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
			if (parse_capabilities(peer, op_val, op_len,
			    &as) == -1) {
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
			timer_set(peer, Timer_IdleHold, 0);	/* no punish */
			peer->IdleHoldTime /= 2;
			return (-1);
		}
	}

	/* if remote-as is zero and it's a cloned neighbor, accept any */
	if (peer->conf.cloned && !peer->conf.remote_as && as != AS_TRANS) {
		peer->conf.remote_as = as;
		peer->conf.ebgp = (peer->conf.remote_as != conf->as);
		if (!peer->conf.ebgp)
			/* force enforce_as off for iBGP sessions */
			peer->conf.enforce_as = ENFORCE_AS_OFF;
	}

	if (peer->conf.remote_as != as) {
		log_peer_warnx(&peer->conf, "peer sent wrong AS %s",
		    log_as(as));
		session_notification(peer, ERR_OPEN, ERR_OPEN_AS, NULL, 0);
		change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
		return (-1);
	}

	if (capa_neg_calc(peer) == -1) {
		log_peer_warnx(&peer->conf,
		    "capabilitiy negotiation calculation failed");
		session_notification(peer, ERR_OPEN, 0, NULL, 0);
		change_state(peer, STATE_IDLE, EVNT_RCVD_OPEN);
		return (-1);
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

	if (imsg_compose(ibuf_rde, IMSG_UPDATE, peer->conf.id, 0, -1, p,
	    datalen) == -1)
		return (-1);

	return (0);
}

int
parse_refresh(struct peer *peer)
{
	u_char		*p;
	u_int16_t	 afi;
	u_int8_t	 aid, safi;

	p = peer->rbuf->rptr;
	p += MSGSIZE_HEADER;	/* header is already checked */

	/*
	 * We could check if we actually announced the capability but
	 * as long as the message is correctly encoded we don't care.
	 */

	/* afi, 2 byte */
	memcpy(&afi, p, sizeof(afi));
	afi = ntohs(afi);
	p += 2;
	/* reserved, 1 byte */
	p += 1;
	/* safi, 1 byte */
	memcpy(&safi, p, sizeof(safi));

	/* afi/safi unchecked -	unrecognized values will be ignored anyway */
	if (afi2aid(afi, safi, &aid) == -1) {
		log_peer_warnx(&peer->conf, "peer sent bad refresh, "
		    "invalid afi/safi pair");
		return (0);
	}

	if (imsg_compose(ibuf_rde, IMSG_REFRESH, peer->conf.id, 0, -1, &aid,
	    sizeof(aid)) == -1)
		return (-1);

	return (0);
}

int
parse_notification(struct peer *peer)
{
	u_char		*p;
	u_int16_t	 datalen;
	u_int8_t	 errcode;
	u_int8_t	 subcode;
	u_int8_t	 capa_code;
	u_int8_t	 capa_len;
	u_int8_t	 i;

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

	log_notification(peer, errcode, subcode, p, datalen);
	peer->errcnt++;

	if (errcode == ERR_OPEN && subcode == ERR_OPEN_CAPA) {
		if (datalen == 0) {	/* zebra likes to send those.. humbug */
			log_peer_warnx(&peer->conf, "received \"unsupported "
			    "capability\" notification without data part, "
			    "disabling capability announcements altogether");
			session_capa_ann_none(peer);
		}

		while (datalen > 0) {
			if (datalen < 2) {
				log_peer_warnx(&peer->conf,
				    "parse_notification: "
				    "expect len >= 2, len is %u", datalen);
				return (-1);
			}
			memcpy(&capa_code, p, sizeof(capa_code));
			p += sizeof(capa_code);
			datalen -= sizeof(capa_code);
			memcpy(&capa_len, p, sizeof(capa_len));
			p += sizeof(capa_len);
			datalen -= sizeof(capa_len);
			if (datalen < capa_len) {
				log_peer_warnx(&peer->conf,
				    "parse_notification: capa_len %u exceeds "
				    "remaining msg length %u", capa_len,
				    datalen);
				return (-1);
			}
			p += capa_len;
			datalen -= capa_len;
			switch (capa_code) {
			case CAPA_MP:
				for (i = 0; i < AID_MAX; i++)
					peer->capa.ann.mp[i] = 0;
				log_peer_warnx(&peer->conf,
				    "disabling multiprotocol capability");
				break;
			case CAPA_REFRESH:
				peer->capa.ann.refresh = 0;
				log_peer_warnx(&peer->conf,
				    "disabling route refresh capability");
				break;
			case CAPA_RESTART:
				peer->capa.ann.restart = 0;
				log_peer_warnx(&peer->conf,
				    "disabling restart capability");
				break;
			case CAPA_AS4BYTE:
				peer->capa.ann.as4byte = 0;
				log_peer_warnx(&peer->conf,
				    "disabling 4-byte AS num capability");
				break;
			default:	/* should not happen... */
				log_peer_warnx(&peer->conf, "received "
				    "\"unsupported capability\" notification "
				    "for unknown capability %u, disabling "
				    "capability announcements altogether",
				    capa_code);
				session_capa_ann_none(peer);
				break;
			}
		}

		return (1);
	}

	if (errcode == ERR_OPEN && subcode == ERR_OPEN_OPT) {
		session_capa_ann_none(peer);
		return (1);
	}

	return (0);
}

int
parse_capabilities(struct peer *peer, u_char *d, u_int16_t dlen, u_int32_t *as)
{
	u_char		*capa_val;
	u_int32_t	 remote_as;
	u_int16_t	 len;
	u_int16_t	 afi;
	u_int8_t	 safi;
	u_int8_t	 aid;
	u_int8_t	 capa_code;
	u_int8_t	 capa_len;

	len = dlen;
	while (len > 0) {
		if (len < 2) {
			log_peer_warnx(&peer->conf, "parse_capabilities: "
			    "expect len >= 2, len is %u", len);
			return (-1);
		}
		memcpy(&capa_code, d, sizeof(capa_code));
		d += sizeof(capa_code);
		len -= sizeof(capa_code);
		memcpy(&capa_len, d, sizeof(capa_len));
		d += sizeof(capa_len);
		len -= sizeof(capa_len);
		if (capa_len > 0) {
			if (len < capa_len) {
				log_peer_warnx(&peer->conf,
				    "parse_capabilities: "
				    "len %u smaller than capa_len %u",
				    len, capa_len);
				return (-1);
			}
			capa_val = d;
			d += capa_len;
			len -= capa_len;
		} else
			capa_val = NULL;

		switch (capa_code) {
		case CAPA_MP:			/* RFC 4760 */
			if (capa_len != 4) {
				log_peer_warnx(&peer->conf,
				    "parse_capabilities: "
				    "expect len 4, len is %u", capa_len);
				return (-1);
			}
			memcpy(&afi, capa_val, sizeof(afi));
			afi = ntohs(afi);
			memcpy(&safi, capa_val + 3, sizeof(safi));
			if (afi2aid(afi, safi, &aid) == -1) {
				log_peer_warnx(&peer->conf,
				    "parse_capabilities: AFI %u, "
				    "safi %u unknown", afi, safi);
				break;
			}
			peer->capa.peer.mp[aid] = 1;
			break;
		case CAPA_REFRESH:
			peer->capa.peer.refresh = 1;
			break;
		case CAPA_RESTART:
			peer->capa.peer.restart = 1;
			/* we don't care about the further restart capas yet */
			break;
		case CAPA_AS4BYTE:
			if (capa_len != 4) {
				log_peer_warnx(&peer->conf,
				    "parse_capabilities: "
				    "expect len 4, len is %u", capa_len);
				return (-1);
			}
			memcpy(&remote_as, capa_val, sizeof(remote_as));
			*as = ntohl(remote_as);
			peer->capa.peer.as4byte = 1;
			break;
		default:
			break;
		}
	}

	return (0);
}

int
capa_neg_calc(struct peer *p)
{
	u_int8_t	i, hasmp = 0;

	/* refresh: does not realy matter here, use peer setting */
	p->capa.neg.refresh = p->capa.peer.refresh;

	/* as4byte: both side must announce capability */
	if (p->capa.ann.as4byte && p->capa.peer.as4byte)
		p->capa.neg.as4byte = 1;
	else
		p->capa.neg.as4byte = 0;

	/* MP: both side must announce capability */
	for (i = 0; i < AID_MAX; i++) {
		if (p->capa.ann.mp[i] && p->capa.peer.mp[i]) {
			p->capa.neg.mp[i] = 1;
			hasmp = 1;
		} else
			p->capa.neg.mp[i] = 0;
	}
	/* if no MP capability present for default IPv4 unicast mode */
	if (!hasmp)
		p->capa.neg.mp[AID_INET] = 1;

	p->capa.neg.restart = p->capa.peer.restart;

	return (0);
}

void
session_dispatch_imsg(struct imsgbuf *ibuf, int idx, u_int *listener_cnt)
{
	struct imsg		 imsg;
	struct mrt		 xmrt;
	struct mrt		*mrt;
	struct peer_config	*pconf;
	struct peer		*p, *next;
	struct listen_addr	*la, *nla;
	struct kif		*kif;
	u_char			*data;
	enum reconf_action	 reconf;
	int			 n, depend_ok;
	u_int8_t		 errcode, subcode;

	if ((n = imsg_read(ibuf)) == -1)
		fatal("session_dispatch_imsg: imsg_read error");

	if (n == 0)	/* connection closed */
		fatalx("session_dispatch_imsg: pipe closed");

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
			if ((nconf->listen_addrs = calloc(1,
			    sizeof(struct listen_addrs))) == NULL)
				fatal(NULL);
			TAILQ_INIT(nconf->listen_addrs);
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
				p->state = p->prev_state = STATE_NONE;
				p->next = npeers;
				npeers = p;
				reconf = RECONF_REINIT;
			} else
				reconf = RECONF_KEEP;

			memcpy(&p->conf, pconf, sizeof(struct peer_config));
			p->conf.reconf_action = reconf;
			break;
		case IMSG_RECONF_LISTENER:
			if (idx != PFD_PIPE_MAIN)
				fatalx("reconf request not from parent");
			if (nconf == NULL)
				fatalx("IMSG_RECONF_LISTENER but no config");
			nla = imsg.data;
			TAILQ_FOREACH(la, conf->listen_addrs, entry)
				if (!la_cmp(la, nla))
					break;

			if (la == NULL) {
				if (nla->reconf != RECONF_REINIT)
					fatalx("king bula sez: "
					    "expected REINIT");

				if ((nla->fd = imsg.fd) == -1)
					log_warnx("expected to receive fd for "
					    "%s but didn't receive any",
					    log_sockaddr((struct sockaddr *)
					    &nla->sa));

				la = calloc(1, sizeof(struct listen_addr));
				if (la == NULL)
					fatal(NULL);
				memcpy(&la->sa, &nla->sa, sizeof(la->sa));
				la->flags = nla->flags;
				la->fd = nla->fd;
				la->reconf = RECONF_REINIT;
				TAILQ_INSERT_TAIL(nconf->listen_addrs, la,
				    entry);
			} else {
				if (nla->reconf != RECONF_KEEP)
					fatalx("king bula sez: expected KEEP");
				la->reconf = RECONF_KEEP;
			}

			break;
		case IMSG_RECONF_DONE:
			if (idx != PFD_PIPE_MAIN)
				fatalx("reconf request not from parent");
			if (nconf == NULL)
				fatalx("got IMSG_RECONF_DONE but no config");
			conf->flags = nconf->flags;
			conf->log = nconf->log;
			conf->bgpid = nconf->bgpid;
			conf->clusterid = nconf->clusterid;
			conf->as = nconf->as;
			conf->short_as = nconf->short_as;
			conf->holdtime = nconf->holdtime;
			conf->min_holdtime = nconf->min_holdtime;
			conf->connectretry = nconf->connectretry;

			/* add new peers */
			for (p = npeers; p != NULL; p = next) {
				next = p->next;
				p->next = peers;
				peers = p;
			}
			/* find ones that need attention */
			for (p = peers; p != NULL; p = p->next) {
				/* needs to be deleted? */
				if (p->conf.reconf_action == RECONF_NONE &&
				    !p->conf.cloned)
					p->conf.reconf_action = RECONF_DELETE;
				/* had demotion, is demoted, demote removed? */
				if (p->demoted && !p->conf.demote_group[0])
						session_demote(p, -1);
			}

			/* delete old listeners */
			for (la = TAILQ_FIRST(conf->listen_addrs); la != NULL;
			    la = nla) {
				nla = TAILQ_NEXT(la, entry);
				if (la->reconf == RECONF_NONE) {
					log_info("not listening on %s any more",
					    log_sockaddr(
					    (struct sockaddr *)&la->sa));
					TAILQ_REMOVE(conf->listen_addrs, la,
					    entry);
					close(la->fd);
					free(la);
				}
			}

			/* add new listeners */
			while ((la = TAILQ_FIRST(nconf->listen_addrs)) !=
			    NULL) {
				TAILQ_REMOVE(nconf->listen_addrs, la, entry);
				TAILQ_INSERT_TAIL(conf->listen_addrs, la,
				    entry);
			}

			setup_listeners(listener_cnt);
			free(nconf->listen_addrs);
			free(nconf);
			nconf = NULL;
			pending_reconf = 0;
			log_info("SE reconfigured");
			break;
		case IMSG_IFINFO:
			if (idx != PFD_PIPE_MAIN)
				fatalx("IFINFO message not from parent");
			if (imsg.hdr.len != IMSG_HEADER_SIZE +
			    sizeof(struct kif))
				fatalx("IFINFO imsg with wrong len");
			kif = imsg.data;
			depend_ok = (kif->flags & IFF_UP) &&
			    (LINK_STATE_IS_UP(kif->link_state) ||
			    (kif->link_state == LINK_STATE_UNKNOWN &&
			    kif->media_type != IFT_CARP));

			for (p = peers; p != NULL; p = p->next)
				if (!strcmp(p->conf.if_depend, kif->ifname)) {
					if (depend_ok && !p->depend_ok) {
						p->depend_ok = depend_ok;
						bgp_fsm(p, EVNT_START);
					} else if (!depend_ok && p->depend_ok) {
						p->depend_ok = depend_ok;
						session_stop(p,
						    ERR_CEASE_OTHER_CHANGE);
					}
				}
			break;
		case IMSG_MRT_OPEN:
		case IMSG_MRT_REOPEN:
			if (imsg.hdr.len > IMSG_HEADER_SIZE +
			    sizeof(struct mrt)) {
				log_warnx("wrong imsg len");
				break;
			}

			memcpy(&xmrt, imsg.data, sizeof(struct mrt));
			if ((xmrt.wbuf.fd = imsg.fd) == -1)
				log_warnx("expected to receive fd for mrt dump "
				    "but didn't receive any");

			mrt = mrt_get(&mrthead, &xmrt);
			if (mrt == NULL) {
				/* new dump */
				mrt = calloc(1, sizeof(struct mrt));
				if (mrt == NULL)
					fatal("session_dispatch_imsg");
				memcpy(mrt, &xmrt, sizeof(struct mrt));
				TAILQ_INIT(&mrt->wbuf.bufs);
				LIST_INSERT_HEAD(&mrthead, mrt, entry);
			} else {
				/* old dump reopened */
				close(mrt->wbuf.fd);
				mrt->wbuf.fd = xmrt.wbuf.fd;
			}
			break;
		case IMSG_MRT_CLOSE:
			if (imsg.hdr.len > IMSG_HEADER_SIZE +
			    sizeof(struct mrt)) {
				log_warnx("wrong imsg len");
				break;
			}

			memcpy(&xmrt, imsg.data, sizeof(struct mrt));
			mrt = mrt_get(&mrthead, &xmrt);
			if (mrt != NULL) {
				mrt_clean(mrt);
				LIST_REMOVE(mrt, entry);
				free(mrt);
			}
			break;
		case IMSG_CTL_KROUTE:
		case IMSG_CTL_KROUTE_ADDR:
		case IMSG_CTL_SHOW_NEXTHOP:
		case IMSG_CTL_SHOW_INTERFACE:
		case IMSG_CTL_SHOW_FIB_TABLES:
			if (idx != PFD_PIPE_MAIN)
				fatalx("ctl kroute request not from parent");
			control_imsg_relay(&imsg);
			break;
		case IMSG_CTL_SHOW_RIB:
		case IMSG_CTL_SHOW_RIB_PREFIX:
		case IMSG_CTL_SHOW_RIB_ATTR:
		case IMSG_CTL_SHOW_RIB_MEM:
		case IMSG_CTL_SHOW_NETWORK:
		case IMSG_CTL_SHOW_NEIGHBOR:
			if (idx != PFD_PIPE_ROUTE_CTL)
				fatalx("ctl rib request not from RDE");
			control_imsg_relay(&imsg);
			break;
		case IMSG_CTL_END:
		case IMSG_CTL_RESULT:
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
			switch (errcode) {
			case ERR_CEASE:
				switch (subcode) {
				case ERR_CEASE_MAX_PREFIX:
					bgp_fsm(p, EVNT_STOP);
					if (p->conf.max_prefix_restart)
						timer_set(p, Timer_IdleHold, 60 *
						    p->conf.max_prefix_restart);
					break;
				default:
					bgp_fsm(p, EVNT_CON_FATAL);
					break;
				}
				break;
			default:
				bgp_fsm(p, EVNT_CON_FATAL);
				break;
			}
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}
}

int
la_cmp(struct listen_addr *a, struct listen_addr *b)
{
	struct sockaddr_in	*in_a, *in_b;
	struct sockaddr_in6	*in6_a, *in6_b;

	if (a->sa.ss_family != b->sa.ss_family)
		return (1);

	switch (a->sa.ss_family) {
	case AF_INET:
		in_a = (struct sockaddr_in *)&a->sa;
		in_b = (struct sockaddr_in *)&b->sa;
		if (in_a->sin_addr.s_addr != in_b->sin_addr.s_addr)
			return (1);
		if (in_a->sin_port != in_b->sin_port)
			return (1);
		break;
	case AF_INET6:
		in6_a = (struct sockaddr_in6 *)&a->sa;
		in6_b = (struct sockaddr_in6 *)&b->sa;
		if (bcmp(&in6_a->sin6_addr, &in6_b->sin6_addr,
		    sizeof(struct in6_addr)))
			return (1);
		if (in6_a->sin6_port != in6_b->sin6_port)
			return (1);
		break;
	default:
		fatal("king bula sez: unknown address family");
		/* NOTREACHED */
	}

	return (0);
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
getpeerbydesc(const char *descr)
{
	struct peer	*p, *res = NULL;
	int		 match = 0;

	for (p = peers; p != NULL; p = p->next)
		if (!strcmp(p->conf.descr, descr)) {
			res = p;
			match++;
		}

	if (match > 1)
		log_info("neighbor description \"%s\" not unique, request "
		    "aborted", descr);

	if (match == 1)
		return (res);
	else
		return (NULL);
}

struct peer *
getpeerbyip(struct sockaddr *ip)
{
	struct bgpd_addr addr;
	struct peer	*p, *newpeer, *loose = NULL;
	u_int32_t	 id;

	sa2addr(ip, &addr);

	/* we might want a more effective way to find peers by IP */
	for (p = peers; p != NULL; p = p->next)
		if (!p->conf.template &&
		    !memcmp(&addr, &p->conf.remote_addr, sizeof(addr)))
			return (p);

	/* try template matching */
	for (p = peers; p != NULL; p = p->next)
		if (p->conf.template &&
		    p->conf.remote_addr.aid == addr.aid &&
		    session_match_mask(p, &addr))
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
		sa2addr(ip, &newpeer->conf.remote_addr);
		switch (ip->sa_family) {
		case AF_INET:
			newpeer->conf.remote_masklen = 32;
			break;
		case AF_INET6:
			newpeer->conf.remote_masklen = 128;
			break;
		}
		newpeer->conf.template = 0;
		newpeer->conf.cloned = 1;
		newpeer->state = newpeer->prev_state = STATE_NONE;
		newpeer->conf.reconf_action = RECONF_KEEP;
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
session_match_mask(struct peer *p, struct bgpd_addr *a)
{
	in_addr_t	 v4mask;
	struct in6_addr	 masked;

	switch (p->conf.remote_addr.aid) {
	case AID_INET:
		v4mask = htonl(prefixlen2mask(p->conf.remote_masklen));
		if (p->conf.remote_addr.v4.s_addr == (a->v4.s_addr & v4mask))
			return (1);
		return (0);
	case AID_INET6:
		inet6applymask(&masked, &a->v6, p->conf.remote_masklen);

		if (!memcmp(&masked, &p->conf.remote_addr.v6, sizeof(masked)))
			return (1);
		return (0);
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
	bzero(&peer->capa.neg, sizeof(peer->capa.neg));
	peer->stats.last_updown = time(NULL);
	if (imsg_compose(ibuf_rde, IMSG_SESSION_DOWN, peer->conf.id, 0, -1,
	    NULL, 0) == -1)
		fatalx("imsg_compose error");
}

void
session_up(struct peer *p)
{
	struct session_up	 sup;

	if (imsg_compose(ibuf_rde, IMSG_SESSION_ADD, p->conf.id, 0, -1,
	    &p->conf, sizeof(p->conf)) == -1)
		fatalx("imsg_compose error");

	sa2addr((struct sockaddr *)&p->sa_local, &sup.local_addr);
	sa2addr((struct sockaddr *)&p->sa_remote, &sup.remote_addr);

	sup.remote_bgpid = p->remote_bgpid;
	sup.short_as = p->short_as;
	memcpy(&sup.capa, &p->capa.neg, sizeof(sup.capa));
	p->stats.last_updown = time(NULL);
	if (imsg_compose(ibuf_rde, IMSG_SESSION_UP, p->conf.id, 0, -1,
	    &sup, sizeof(sup)) == -1)
		fatalx("imsg_compose error");
}

int
imsg_compose_parent(int type, u_int32_t peerid, pid_t pid, void *data,
    u_int16_t datalen)
{
	return (imsg_compose(ibuf_main, type, peerid, pid, -1, data, datalen));
}

int
imsg_compose_rde(int type, pid_t pid, void *data, u_int16_t datalen)
{
	return (imsg_compose(ibuf_rde, type, 0, pid, -1, data, datalen));
}

void
session_demote(struct peer *p, int level)
{
	struct demote_msg	msg;

	strlcpy(msg.demote_group, p->conf.demote_group,
	    sizeof(msg.demote_group));
	msg.level = level;
	if (imsg_compose(ibuf_main, IMSG_DEMOTE, p->conf.id, 0, -1,
	    &msg, sizeof(msg)) == -1)
		fatalx("imsg_compose error");

	p->demoted += level;
}

void
session_stop(struct peer *peer, u_int8_t subcode)
{
	switch (peer->state) {
	case STATE_OPENSENT:
	case STATE_OPENCONFIRM:
	case STATE_ESTABLISHED:
		session_notification(peer, ERR_CEASE, subcode, NULL, 0);
		break;
	default:
		/* session not open, no need to send notification */
		break;
	}
	bgp_fsm(peer, EVNT_STOP);
}
