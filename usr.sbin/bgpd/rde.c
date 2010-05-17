/*	$OpenBSD: rde.c,v 1.294 2010/05/17 15:49:29 claudio Exp $ */

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

#include <sys/types.h>
#include <sys/socket.h>

#include <errno.h>
#include <ifaddrs.h>
#include <limits.h>
#include <pwd.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "mrt.h"
#include "rde.h"
#include "session.h"

#define PFD_PIPE_MAIN		0
#define PFD_PIPE_SESSION	1
#define PFD_PIPE_SESSION_CTL	2
#define PFD_PIPE_COUNT		3

void		 rde_sighdlr(int);
void		 rde_dispatch_imsg_session(struct imsgbuf *);
void		 rde_dispatch_imsg_parent(struct imsgbuf *);
int		 rde_update_dispatch(struct imsg *);
void		 rde_update_update(struct rde_peer *, struct rde_aspath *,
		     struct bgpd_addr *, u_int8_t);
void		 rde_update_withdraw(struct rde_peer *, struct bgpd_addr *,
		     u_int8_t);
int		 rde_attr_parse(u_char *, u_int16_t, struct rde_peer *,
		     struct rde_aspath *, struct mpattr *);
u_int8_t	 rde_attr_missing(struct rde_aspath *, int, u_int16_t);
int		 rde_get_mp_nexthop(u_char *, u_int16_t, u_int8_t,
		     struct rde_aspath *);
int		 rde_update_extract_prefix(u_char *, u_int16_t, void *,
		     u_int8_t, u_int8_t);
int		 rde_update_get_prefix(u_char *, u_int16_t, struct bgpd_addr *,
		     u_int8_t *);
int		 rde_update_get_prefix6(u_char *, u_int16_t, struct bgpd_addr *,
		     u_int8_t *);
int		 rde_update_get_vpn4(u_char *, u_int16_t, struct bgpd_addr *,
		     u_int8_t *);
void		 rde_update_err(struct rde_peer *, u_int8_t , u_int8_t,
		     void *, u_int16_t);
void		 rde_update_log(const char *, u_int16_t,
		     const struct rde_peer *, const struct bgpd_addr *,
		     const struct bgpd_addr *, u_int8_t);
void		 rde_as4byte_fixup(struct rde_peer *, struct rde_aspath *);
void		 rde_reflector(struct rde_peer *, struct rde_aspath *);

void		 rde_dump_rib_as(struct prefix *, struct rde_aspath *,pid_t,
		     int);
void		 rde_dump_filter(struct prefix *,
		     struct ctl_show_rib_request *);
void		 rde_dump_filterout(struct rde_peer *, struct prefix *,
		     struct ctl_show_rib_request *);
void		 rde_dump_upcall(struct rib_entry *, void *);
void		 rde_dump_prefix_upcall(struct rib_entry *, void *);
void		 rde_dump_ctx_new(struct ctl_show_rib_request *, pid_t,
		     enum imsg_type);
void		 rde_dump_mrt_new(struct mrt *, pid_t, int);
void		 rde_dump_done(void *);

int		 rde_rdomain_import(struct rde_aspath *, struct rdomain *);
void		 rde_up_dump_upcall(struct rib_entry *, void *);
void		 rde_softreconfig_out(struct rib_entry *, void *);
void		 rde_softreconfig_in(struct rib_entry *, void *);
void		 rde_softreconfig_load(struct rib_entry *, void *);
void		 rde_update_queue_runner(void);
void		 rde_update6_queue_runner(u_int8_t);

void		 peer_init(u_int32_t);
void		 peer_shutdown(void);
void		 peer_localaddrs(struct rde_peer *, struct bgpd_addr *);
struct rde_peer	*peer_add(u_int32_t, struct peer_config *);
struct rde_peer	*peer_get(u_int32_t);
void		 peer_up(u_int32_t, struct session_up *);
void		 peer_down(u_int32_t);
void		 peer_dump(u_int32_t, u_int8_t);
void		 peer_send_eor(struct rde_peer *, u_int8_t);

void		 network_add(struct network_config *, int);
void		 network_delete(struct network_config *, int);
void		 network_dump_upcall(struct rib_entry *, void *);

void		 rde_shutdown(void);
int		 sa_cmp(struct bgpd_addr *, struct sockaddr *);

volatile sig_atomic_t	 rde_quit = 0;
struct bgpd_config	*conf, *nconf;
time_t			 reloadtime;
struct rde_peer_head	 peerlist;
struct rde_peer		*peerself;
struct filter_head	*rules_l, *newrules;
struct rdomain_head	*rdomains_l, *newdomains;
struct imsgbuf		*ibuf_se;
struct imsgbuf		*ibuf_se_ctl;
struct imsgbuf		*ibuf_main;
struct rde_memstats	 rdemem;

struct rde_dump_ctx {
	struct rib_context		ribctx;
	struct ctl_show_rib_request	req;
	sa_family_t			af;
};

struct rde_mrt_ctx {
	struct mrt		mrt;
	struct rib_context	ribctx;
	LIST_ENTRY(rde_mrt_ctx)	entry;
};

LIST_HEAD(, rde_mrt_ctx) rde_mrts = LIST_HEAD_INITIALIZER(rde_mrts);
u_int rde_mrt_cnt;

void
rde_sighdlr(int sig)
{
	switch (sig) {
	case SIGINT:
	case SIGTERM:
		rde_quit = 1;
		break;
	}
}

u_int32_t	peerhashsize = 64;
u_int32_t	pathhashsize = 1024;
u_int32_t	attrhashsize = 512;
u_int32_t	nexthophashsize = 64;

pid_t
rde_main(int pipe_m2r[2], int pipe_s2r[2], int pipe_m2s[2], int pipe_s2rctl[2],
    int debug)
{
	pid_t			 pid;
	struct passwd		*pw;
	struct pollfd		*pfd = NULL;
	struct rde_mrt_ctx	*mctx, *xmctx;
	void			*newp;
	u_int			 pfd_elms = 0, i, j;
	int			 timeout;
	u_int8_t		 aid;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	if ((pw = getpwnam(BGPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	bgpd_process = PROC_RDE;

	if (setgroups(1, &pw->pw_gid) ||
	    setresgid(pw->pw_gid, pw->pw_gid, pw->pw_gid) ||
	    setresuid(pw->pw_uid, pw->pw_uid, pw->pw_uid))
		fatal("can't drop privileges");

	signal(SIGTERM, rde_sighdlr);
	signal(SIGINT, rde_sighdlr);
	signal(SIGPIPE, SIG_IGN);
	signal(SIGHUP, SIG_IGN);
	signal(SIGALRM, SIG_IGN);
	signal(SIGUSR1, SIG_IGN);

	close(pipe_s2r[0]);
	close(pipe_s2rctl[0]);
	close(pipe_m2r[0]);
	close(pipe_m2s[0]);
	close(pipe_m2s[1]);

	/* initialize the RIB structures */
	if ((ibuf_se = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_se_ctl = malloc(sizeof(struct imsgbuf))) == NULL ||
	    (ibuf_main = malloc(sizeof(struct imsgbuf))) == NULL)
		fatal(NULL);
	imsg_init(ibuf_se, pipe_s2r[1]);
	imsg_init(ibuf_se_ctl, pipe_s2rctl[1]);
	imsg_init(ibuf_main, pipe_m2r[1]);

	pt_init();
	path_init(pathhashsize);
	aspath_init(pathhashsize);
	attr_init(attrhashsize);
	nexthop_init(nexthophashsize);
	peer_init(peerhashsize);

	rules_l = calloc(1, sizeof(struct filter_head));
	if (rules_l == NULL)
		fatal(NULL);
	TAILQ_INIT(rules_l);
	rdomains_l = calloc(1, sizeof(struct rdomain_head));
	if (rdomains_l == NULL)
		fatal(NULL);
	SIMPLEQ_INIT(rdomains_l);
	if ((conf = malloc(sizeof(struct bgpd_config))) == NULL)
		fatal(NULL);
	log_info("route decision engine ready");

	while (rde_quit == 0) {
		if (pfd_elms < PFD_PIPE_COUNT + rde_mrt_cnt) {
			if ((newp = realloc(pfd, sizeof(struct pollfd) *
			    (PFD_PIPE_COUNT + rde_mrt_cnt))) == NULL) {
				/* panic for now  */
				log_warn("could not resize pfd from %u -> %u"
				    " entries", pfd_elms, PFD_PIPE_COUNT +
				    rde_mrt_cnt);
				fatalx("exiting");
			}
			pfd = newp;
			pfd_elms = PFD_PIPE_COUNT + rde_mrt_cnt;
		}
		timeout = INFTIM;
		bzero(pfd, sizeof(struct pollfd) * pfd_elms);
		pfd[PFD_PIPE_MAIN].fd = ibuf_main->fd;
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		if (ibuf_main->w.queued > 0)
			pfd[PFD_PIPE_MAIN].events |= POLLOUT;

		pfd[PFD_PIPE_SESSION].fd = ibuf_se->fd;
		pfd[PFD_PIPE_SESSION].events = POLLIN;
		if (ibuf_se->w.queued > 0)
			pfd[PFD_PIPE_SESSION].events |= POLLOUT;

		pfd[PFD_PIPE_SESSION_CTL].fd = ibuf_se_ctl->fd;
		pfd[PFD_PIPE_SESSION_CTL].events = POLLIN;
		if (ibuf_se_ctl->w.queued > 0)
			pfd[PFD_PIPE_SESSION_CTL].events |= POLLOUT;
		else if (rib_dump_pending())
			timeout = 0;

		i = PFD_PIPE_COUNT;
		LIST_FOREACH(mctx, &rde_mrts, entry) {
			if (mctx->mrt.wbuf.queued) {
				pfd[i].fd = mctx->mrt.wbuf.fd;
				pfd[i].events = POLLOUT;
				i++;
			}
		}

		if (poll(pfd, i, timeout) == -1) {
			if (errno != EINTR)
				fatal("poll error");
			continue;
		}

		if ((pfd[PFD_PIPE_MAIN].revents & POLLOUT) &&
		    ibuf_main->w.queued)
			if (msgbuf_write(&ibuf_main->w) < 0)
				fatal("pipe write error");

		if (pfd[PFD_PIPE_MAIN].revents & POLLIN)
			rde_dispatch_imsg_parent(ibuf_main);

		if ((pfd[PFD_PIPE_SESSION].revents & POLLOUT) &&
		    ibuf_se->w.queued)
			if (msgbuf_write(&ibuf_se->w) < 0)
				fatal("pipe write error");

		if (pfd[PFD_PIPE_SESSION].revents & POLLIN)
			rde_dispatch_imsg_session(ibuf_se);

		if ((pfd[PFD_PIPE_SESSION_CTL].revents & POLLOUT) &&
		    ibuf_se_ctl->w.queued)
			if (msgbuf_write(&ibuf_se_ctl->w) < 0)
				fatal("pipe write error");

		if (pfd[PFD_PIPE_SESSION_CTL].revents & POLLIN)
			rde_dispatch_imsg_session(ibuf_se_ctl);

		for (j = PFD_PIPE_COUNT, mctx = LIST_FIRST(&rde_mrts);
		    j < i && mctx != 0; j++) {
			xmctx = LIST_NEXT(mctx, entry);
			if (pfd[j].fd == mctx->mrt.wbuf.fd &&
			    pfd[j].revents & POLLOUT)
				mrt_write(&mctx->mrt);
			if (mctx->mrt.wbuf.queued == 0 && 
			    mctx->mrt.state == MRT_STATE_REMOVE) {
				close(mctx->mrt.wbuf.fd);
				LIST_REMOVE(&mctx->ribctx, entry);
				LIST_REMOVE(mctx, entry);
				free(mctx);
				rde_mrt_cnt--;
			}
			mctx = xmctx;
		}

		rde_update_queue_runner();
		for (aid = AID_INET6; aid < AID_MAX; aid++)
			rde_update6_queue_runner(aid);
		if (ibuf_se_ctl->w.queued <= 0)
			rib_dump_runner();
	}

	/* do not clean up on shutdown on production, it takes ages. */
	if (debug)
		rde_shutdown();

	while ((mctx = LIST_FIRST(&rde_mrts)) != NULL) {
		msgbuf_clear(&mctx->mrt.wbuf);
		close(mctx->mrt.wbuf.fd);
		LIST_REMOVE(&mctx->ribctx, entry);
		LIST_REMOVE(mctx, entry);
		free(mctx);
	}

	msgbuf_clear(&ibuf_se->w);
	free(ibuf_se);
	msgbuf_clear(&ibuf_se_ctl->w);
	free(ibuf_se_ctl);
	msgbuf_clear(&ibuf_main->w);
	free(ibuf_main);

	log_info("route decision engine exiting");
	_exit(0);
}

struct network_config	 netconf_s, netconf_p;
struct filter_set_head	*session_set, *parent_set;

void
rde_dispatch_imsg_session(struct imsgbuf *ibuf)
{
	struct imsg		 imsg;
	struct peer		 p;
	struct peer_config	 pconf;
	struct rde_peer		*peer;
	struct session_up	 sup;
	struct ctl_show_rib_request	req;
	struct filter_set	*s;
	struct nexthop		*nh;
	ssize_t			 n;
	int			 verbose;
	u_int8_t		 aid;

	if ((n = imsg_read(ibuf)) == -1)
		fatal("rde_dispatch_imsg_session: imsg_read error");
	if (n == 0)	/* connection closed */
		fatalx("rde_dispatch_imsg_session: pipe closed");

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_imsg_session: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_UPDATE:
			rde_update_dispatch(&imsg);
			break;
		case IMSG_SESSION_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(pconf))
				fatalx("incorrect size of session request");
			memcpy(&pconf, imsg.data, sizeof(pconf));
			peer = peer_add(imsg.hdr.peerid, &pconf);
			if (peer == NULL) {
				log_warnx("session add: "
				    "peer id %d already exists",
				    imsg.hdr.peerid);
				break;
			}
			break;
		case IMSG_SESSION_UP:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(sup))
				fatalx("incorrect size of session request");
			memcpy(&sup, imsg.data, sizeof(sup));
			peer_up(imsg.hdr.peerid, &sup);
			break;
		case IMSG_SESSION_DOWN:
			peer_down(imsg.hdr.peerid);
			break;
		case IMSG_REFRESH:
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(aid)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&aid, imsg.data, sizeof(aid));
			if (aid >= AID_MAX)
				fatalx("IMSG_REFRESH: bad AID");
			peer_dump(imsg.hdr.peerid, aid);
			break;
		case IMSG_NETWORK_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct network_config)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&netconf_s, imsg.data, sizeof(netconf_s));
			TAILQ_INIT(&netconf_s.attrset);
			session_set = &netconf_s.attrset;
			break;
		case IMSG_NETWORK_DONE:
			if (imsg.hdr.len != IMSG_HEADER_SIZE) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			session_set = NULL;
			switch (netconf_s.prefix.aid) {
			case AID_INET:
				if (netconf_s.prefixlen > 32)
					goto badnet;
				network_add(&netconf_s, 0);
				break;
			case AID_INET6:
				if (netconf_s.prefixlen > 128)
					goto badnet;
				network_add(&netconf_s, 0);
				break;
			default:
badnet:
				log_warnx("rde_dispatch: bad network");
				break;
			}
			break;
		case IMSG_NETWORK_REMOVE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct network_config)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&netconf_s, imsg.data, sizeof(netconf_s));
			TAILQ_INIT(&netconf_s.attrset);
			network_delete(&netconf_s, 0);
			break;
		case IMSG_NETWORK_FLUSH:
			if (imsg.hdr.len != IMSG_HEADER_SIZE) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			prefix_network_clean(peerself, time(NULL),
			    F_ANN_DYNAMIC);
			break;
		case IMSG_FILTER_SET:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct filter_set)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			if (session_set == NULL) {
				log_warnx("rde_dispatch: "
				    "IMSG_FILTER_SET unexpected");
				break;
			}
			if ((s = malloc(sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			memcpy(s, imsg.data, sizeof(struct filter_set));
			TAILQ_INSERT_TAIL(session_set, s, entry);

			if (s->type == ACTION_SET_NEXTHOP) {
				nh = nexthop_get(&s->action.nexthop);
				nh->refcnt++;
			}
			break;
		case IMSG_CTL_SHOW_NETWORK:
		case IMSG_CTL_SHOW_RIB:
		case IMSG_CTL_SHOW_RIB_AS:
		case IMSG_CTL_SHOW_RIB_COMMUNITY:
		case IMSG_CTL_SHOW_RIB_PREFIX:
			if (imsg.hdr.len != IMSG_HEADER_SIZE + sizeof(req)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&req, imsg.data, sizeof(req));
			rde_dump_ctx_new(&req, imsg.hdr.pid, imsg.hdr.type);
			break;
		case IMSG_CTL_SHOW_NEIGHBOR:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct peer)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&p, imsg.data, sizeof(struct peer));
			peer = peer_get(p.conf.id);
			if (peer != NULL) {
				p.stats.prefix_cnt = peer->prefix_cnt;
				p.stats.prefix_rcvd_update =
				    peer->prefix_rcvd_update;
				p.stats.prefix_rcvd_withdraw =
				    peer->prefix_rcvd_withdraw;
				p.stats.prefix_sent_update =
				    peer->prefix_sent_update;
				p.stats.prefix_sent_withdraw =
				    peer->prefix_sent_withdraw;
			}
			imsg_compose(ibuf_se_ctl, IMSG_CTL_SHOW_NEIGHBOR, 0,
			    imsg.hdr.pid, -1, &p, sizeof(struct peer));
			break;
		case IMSG_CTL_END:
			imsg_compose(ibuf_se_ctl, IMSG_CTL_END, 0, imsg.hdr.pid,
			    -1, NULL, 0);
			break;
		case IMSG_CTL_SHOW_RIB_MEM:
			imsg_compose(ibuf_se_ctl, IMSG_CTL_SHOW_RIB_MEM, 0,
			    imsg.hdr.pid, -1, &rdemem, sizeof(rdemem));
			break;
		case IMSG_CTL_LOG_VERBOSE:
			/* already checked by SE */
			memcpy(&verbose, imsg.data, sizeof(verbose));
			log_verbose(verbose);
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}
}

void
rde_dispatch_imsg_parent(struct imsgbuf *ibuf)
{
	static struct rdomain	*rd;
	struct imsg		 imsg;
	struct mrt		 xmrt;
	struct rde_rib		 rn;
	struct rde_peer		*peer;
	struct filter_rule	*r;
	struct filter_set	*s;
	struct nexthop		*nh;
	int			 n, fd, reconf_in = 0, reconf_out = 0;
	u_int16_t		 rid;

	if ((n = imsg_read(ibuf)) == -1)
		fatal("rde_dispatch_imsg_parent: imsg_read error");
	if (n == 0)	/* connection closed */
		fatalx("rde_dispatch_imsg_parent: pipe closed");

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_imsg_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_NETWORK_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct network_config)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&netconf_p, imsg.data, sizeof(netconf_p));
			TAILQ_INIT(&netconf_p.attrset);
			parent_set = &netconf_p.attrset;
			break;
		case IMSG_NETWORK_DONE:
			parent_set = NULL;
			network_add(&netconf_p, 1);
			break;
		case IMSG_NETWORK_REMOVE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct network_config)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&netconf_p, imsg.data, sizeof(netconf_p));
			TAILQ_INIT(&netconf_p.attrset);
			network_delete(&netconf_p, 1);
			break;
		case IMSG_RECONF_CONF:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct bgpd_config))
				fatalx("IMSG_RECONF_CONF bad len");
			reloadtime = time(NULL);
			newrules = calloc(1, sizeof(struct filter_head));
			if (newrules == NULL)
				fatal(NULL);
			TAILQ_INIT(newrules);
			newdomains = calloc(1, sizeof(struct rdomain_head));
			if (newdomains == NULL)
				fatal(NULL);
			SIMPLEQ_INIT(newdomains);
			if ((nconf = malloc(sizeof(struct bgpd_config))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct bgpd_config));
			for (rid = 0; rid < rib_size; rid++)
				ribs[rid].state = RECONF_DELETE;
			break;
		case IMSG_RECONF_RIB:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct rde_rib))
				fatalx("IMSG_RECONF_RIB bad len");
			memcpy(&rn, imsg.data, sizeof(rn));
			rid = rib_find(rn.name);
			if (rid == RIB_FAILED)
				rib_new(rn.name, rn.rtableid, rn.flags);
			else if (ribs[rid].rtableid != rn.rtableid ||
			    (ribs[rid].flags & F_RIB_HASNOFIB) !=
			    (rn.flags & F_RIB_HASNOFIB)) {
				/* Big hammer in the F_RIB_NOFIB case but
				 * not often enough used to optimise it more. */
				rib_free(&ribs[rid]);
				rib_new(rn.name, rn.rtableid, rn.flags);
			} else
				ribs[rid].state = RECONF_KEEP;
			break;
		case IMSG_RECONF_FILTER:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct filter_rule))
				fatalx("IMSG_RECONF_FILTER bad len");
			if ((r = malloc(sizeof(struct filter_rule))) == NULL)
				fatal(NULL);
			memcpy(r, imsg.data, sizeof(struct filter_rule));
			TAILQ_INIT(&r->set);
			r->peer.ribid = rib_find(r->rib);
			parent_set = &r->set;
			TAILQ_INSERT_TAIL(newrules, r, entry);
			break;
		case IMSG_RECONF_RDOMAIN:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct rdomain))
				fatalx("IMSG_RECONF_RDOMAIN bad len");
			if ((rd = malloc(sizeof(struct rdomain))) == NULL)
				fatal(NULL);
			memcpy(rd, imsg.data, sizeof(struct rdomain));
			TAILQ_INIT(&rd->import);
			TAILQ_INIT(&rd->export);
			SIMPLEQ_INSERT_TAIL(newdomains, rd, entry);
			break;
		case IMSG_RECONF_RDOMAIN_EXPORT:
			if (rd == NULL) {
				log_warnx("rde_dispatch_imsg_parent: "
				    "IMSG_RECONF_RDOMAIN_EXPORT unexpected");
				break;
			}
			parent_set = &rd->export;
			break;
		case IMSG_RECONF_RDOMAIN_IMPORT:
			if (rd == NULL) {
				log_warnx("rde_dispatch_imsg_parent: "
				    "IMSG_RECONF_RDOMAIN_IMPORT unexpected");
				break;
			}
			parent_set = &rd->import;
			break;
		case IMSG_RECONF_RDOMAIN_DONE:
			parent_set = NULL;
			break;
		case IMSG_RECONF_DONE:
			if (nconf == NULL)
				fatalx("got IMSG_RECONF_DONE but no config");
			if ((nconf->flags & BGPD_FLAG_NO_EVALUATE)
			    != (conf->flags & BGPD_FLAG_NO_EVALUATE)) {
				log_warnx("change to/from route-collector "
				    "mode ignored");
				if (conf->flags & BGPD_FLAG_NO_EVALUATE)
					nconf->flags |= BGPD_FLAG_NO_EVALUATE;
				else
					nconf->flags &= ~BGPD_FLAG_NO_EVALUATE;
			}
			memcpy(conf, nconf, sizeof(struct bgpd_config));
			conf->listen_addrs = NULL;
			conf->csock = NULL;
			conf->rcsock = NULL;
			free(nconf);
			nconf = NULL;
			parent_set = NULL;
			/* sync peerself with conf */
			peerself->remote_bgpid = ntohl(conf->bgpid);
			peerself->conf.local_as = conf->as;
			peerself->conf.remote_as = conf->as;
			peerself->short_as = conf->short_as;

			/* apply new set of rdomain, sync will be done later */
			while ((rd = SIMPLEQ_FIRST(rdomains_l)) != NULL) {
				SIMPLEQ_REMOVE_HEAD(rdomains_l, entry);
				filterset_free(&rd->import);
				filterset_free(&rd->export);
				free(rd);
			}
			free(rdomains_l);
			rdomains_l = newdomains;

			/* check if filter changed */
			LIST_FOREACH(peer, &peerlist, peer_l) {
				if (peer->conf.id == 0)
					continue;
				peer->reconf_out = 0;
				peer->reconf_in = 0;
				if (peer->conf.softreconfig_out &&
				    !rde_filter_equal(rules_l, newrules, peer,
				    DIR_OUT)) {
					peer->reconf_out = 1;
					reconf_out = 1;
				}
				if (peer->conf.softreconfig_in &&
				    !rde_filter_equal(rules_l, newrules, peer,
				    DIR_IN)) {
					peer->reconf_in = 1;
					reconf_in = 1;
				}
			}
			/* bring ribs in sync before softreconfig dance */
			for (rid = 0; rid < rib_size; rid++) {
				if (ribs[rid].state == RECONF_DELETE)
					rib_free(&ribs[rid]);
				else if (ribs[rid].state == RECONF_REINIT)
					rib_dump(&ribs[0],
					    rde_softreconfig_load, &ribs[rid],
					    AID_UNSPEC);
			}
			/* sync local-RIBs first */
			if (reconf_in)
				rib_dump(&ribs[0], rde_softreconfig_in, NULL,
				    AID_UNSPEC);
			/* then sync peers */
			if (reconf_out) {
				int i;
				for (i = 1; i < rib_size; i++) {
					if (ribs[i].state == RECONF_REINIT)
						/* already synced by _load */
						continue;
					rib_dump(&ribs[i], rde_softreconfig_out,
					    NULL, AID_UNSPEC);
				}
			}

			while ((r = TAILQ_FIRST(rules_l)) != NULL) {
				TAILQ_REMOVE(rules_l, r, entry);
				filterset_free(&r->set);
				free(r);
			}
			free(rules_l);
			rules_l = newrules;

			log_info("RDE reconfigured");
			break;
		case IMSG_NEXTHOP_UPDATE:
			nexthop_update(imsg.data);
			break;
		case IMSG_FILTER_SET:
			if (imsg.hdr.len > IMSG_HEADER_SIZE +
			    sizeof(struct filter_set))
				fatalx("IMSG_RECONF_CONF bad len");
			if (parent_set == NULL) {
				log_warnx("rde_dispatch_imsg_parent: "
				    "IMSG_FILTER_SET unexpected");
				break;
			}
			if ((s = malloc(sizeof(struct filter_set))) == NULL)
				fatal(NULL);
			memcpy(s, imsg.data, sizeof(struct filter_set));
			TAILQ_INSERT_TAIL(parent_set, s, entry);

			if (s->type == ACTION_SET_NEXTHOP) {
				nh = nexthop_get(&s->action.nexthop);
				nh->refcnt++;
			}
			break;
		case IMSG_MRT_OPEN:
		case IMSG_MRT_REOPEN:
			if (imsg.hdr.len > IMSG_HEADER_SIZE +
			    sizeof(struct mrt)) {
				log_warnx("wrong imsg len");
				break;
			}
			memcpy(&xmrt, imsg.data, sizeof(xmrt));
			if ((fd = imsg.fd) == -1)
				log_warnx("expected to receive fd for mrt dump "
				    "but didn't receive any");
			else if (xmrt.type == MRT_TABLE_DUMP ||
			    xmrt.type == MRT_TABLE_DUMP_MP) {
				rde_dump_mrt_new(&xmrt, imsg.hdr.pid, fd);
			} else
				close(fd);
			break;
		case IMSG_MRT_CLOSE:
			/* ignore end message because a dump is atomic */
			break;
		default:
			break;
		}
		imsg_free(&imsg);
	}
}

/* handle routing updates from the session engine. */
int
rde_update_dispatch(struct imsg *imsg)
{
	struct bgpd_addr	 prefix;
	struct mpattr		 mpa;
	struct rde_peer		*peer;
	struct rde_aspath	*asp = NULL;
	u_char			*p, *mpp = NULL;
	int			 error = -1, pos = 0;
	u_int16_t		 afi, len, mplen;
	u_int16_t		 withdrawn_len;
	u_int16_t		 attrpath_len;
	u_int16_t		 nlri_len;
	u_int8_t		 aid, prefixlen, safi, subtype;

	peer = peer_get(imsg->hdr.peerid);
	if (peer == NULL)	/* unknown peer, cannot happen */
		return (-1);
	if (peer->state != PEER_UP)
		return (-1);	/* peer is not yet up, cannot happen */

	p = imsg->data;

	if (imsg->hdr.len < IMSG_HEADER_SIZE + 2) {
		rde_update_err(peer, ERR_UPDATE, ERR_UPD_ATTRLIST, NULL, 0);
		return (-1);
	}

	memcpy(&len, p, 2);
	withdrawn_len = ntohs(len);
	p += 2;
	if (imsg->hdr.len < IMSG_HEADER_SIZE + 2 + withdrawn_len + 2) {
		rde_update_err(peer, ERR_UPDATE, ERR_UPD_ATTRLIST, NULL, 0);
		return (-1);
	}

	p += withdrawn_len;
	memcpy(&len, p, 2);
	attrpath_len = len = ntohs(len);
	p += 2;
	if (imsg->hdr.len <
	    IMSG_HEADER_SIZE + 2 + withdrawn_len + 2 + attrpath_len) {
		rde_update_err(peer, ERR_UPDATE, ERR_UPD_ATTRLIST, NULL, 0);
		return (-1);
	}

	nlri_len =
	    imsg->hdr.len - IMSG_HEADER_SIZE - 4 - withdrawn_len - attrpath_len;
	bzero(&mpa, sizeof(mpa));

	if (attrpath_len != 0) { /* 0 = no NLRI information in this message */
		/* parse path attributes */
		asp = path_get();
		while (len > 0) {
			if ((pos = rde_attr_parse(p, len, peer, asp,
			    &mpa)) < 0)
				goto done;
			p += pos;
			len -= pos;
		}

		/* check for missing but necessary attributes */
		if ((subtype = rde_attr_missing(asp, peer->conf.ebgp,
		    nlri_len))) {
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_MISSNG_WK_ATTR,
			    &subtype, sizeof(u_int8_t));
			goto done;
		}

		rde_as4byte_fixup(peer, asp);

		/* enforce remote AS if requested */
		if (asp->flags & F_ATTR_ASPATH &&
		    peer->conf.enforce_as == ENFORCE_AS_ON)
			if (peer->conf.remote_as !=
			    aspath_neighbor(asp->aspath)) {
				log_peer_warnx(&peer->conf, "bad path, "
				    "enforce remote-as enabled");
				rde_update_err(peer, ERR_UPDATE, ERR_UPD_ASPATH,
				    NULL, 0);
				goto done;
			}

		rde_reflector(peer, asp);
	}

	p = imsg->data;
	len = withdrawn_len;
	p += 2;
	/* withdraw prefix */
	while (len > 0) {
		if ((pos = rde_update_get_prefix(p, len, &prefix,
		    &prefixlen)) == -1) {
			/*
			 * the RFC does not mention what we should do in
			 * this case. Let's do the same as in the NLRI case.
			 */
			log_peer_warnx(&peer->conf, "bad withdraw prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    NULL, 0);
			goto done;
		}
		if (prefixlen > 32) {
			log_peer_warnx(&peer->conf, "bad withdraw prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    NULL, 0);
			goto done;
		}

		p += pos;
		len -= pos;

		if (peer->capa.mp[AID_INET] == 0) {
			log_peer_warnx(&peer->conf,
			    "bad withdraw, %s disabled", aid2str(AID_INET));
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_OPTATTR,
			    NULL, 0);
			goto done;
		}

		rde_update_withdraw(peer, &prefix, prefixlen);
	}

	if (attrpath_len == 0) {
		/* 0 = no NLRI information in this message */
		if (nlri_len != 0) {
			/* crap at end of update which should not be there */
			rde_update_err(peer, ERR_UPDATE,
			    ERR_UPD_ATTRLIST, NULL, 0);
			return (-1);
		}
		return (0);
	}

	/* withdraw MP_UNREACH_NLRI if available */
	if (mpa.unreach_len != 0) {
		mpp = mpa.unreach;
		mplen = mpa.unreach_len;
		memcpy(&afi, mpp, 2);
		mpp += 2;
		mplen -= 2;
		afi = ntohs(afi);
		safi = *mpp++;
		mplen--;

		if (afi2aid(afi, safi, &aid) == -1) {
			log_peer_warnx(&peer->conf,
			    "bad AFI/SAFI pair in withdraw");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_OPTATTR,
			    NULL, 0);
			goto done;
		}

		if (peer->capa.mp[aid] == 0) {
			log_peer_warnx(&peer->conf,
			    "bad withdraw, %s disabled", aid2str(aid));
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_OPTATTR,
			    NULL, 0);
			goto done;
		}

		switch (aid) {
		case AID_INET6:
			while (mplen > 0) {
				if ((pos = rde_update_get_prefix6(mpp, mplen,
				    &prefix, &prefixlen)) == -1) {
					log_peer_warnx(&peer->conf,
					    "bad IPv6 withdraw prefix");
					rde_update_err(peer, ERR_UPDATE,
					    ERR_UPD_OPTATTR,
					    mpa.unreach, mpa.unreach_len);
					goto done;
				}
				if (prefixlen > 128) {
					log_peer_warnx(&peer->conf,
					    "bad IPv6 withdraw prefix");
					rde_update_err(peer, ERR_UPDATE,
					    ERR_UPD_OPTATTR,
					    mpa.unreach, mpa.unreach_len);
					goto done;
				}

				mpp += pos;
				mplen -= pos;

				rde_update_withdraw(peer, &prefix, prefixlen);
			}
			break;
		case AID_VPN_IPv4:
			while (mplen > 0) {
				if ((pos = rde_update_get_vpn4(mpp, mplen,
				    &prefix, &prefixlen)) == -1) {
					log_peer_warnx(&peer->conf,
					    "bad VPNv4 withdraw prefix");
					rde_update_err(peer, ERR_UPDATE,
					    ERR_UPD_OPTATTR,
					    mpa.unreach, mpa.unreach_len);
					goto done;
				}
				if (prefixlen > 32) {
					log_peer_warnx(&peer->conf,
					    "bad VPNv4 withdraw prefix");
					rde_update_err(peer, ERR_UPDATE,
					    ERR_UPD_OPTATTR,
					    mpa.unreach, mpa.unreach_len);
					goto done;
				}

				mpp += pos;
				mplen -= pos;

				rde_update_withdraw(peer, &prefix, prefixlen);
			}
			break;
		default:
			/* silently ignore unsupported multiprotocol AF */
			break;
		}

		if ((asp->flags & ~F_ATTR_MP_UNREACH) == 0) {
			error = 0;
			goto done;
		}
	}

	/* shift to NLRI information */
	p += 2 + attrpath_len;

	/* aspath needs to be loop free nota bene this is not a hard error */
	if (peer->conf.ebgp && !aspath_loopfree(asp->aspath, conf->as))
		asp->flags |= F_ATTR_LOOP;

	/* parse nlri prefix */
	while (nlri_len > 0) {
		if ((pos = rde_update_get_prefix(p, nlri_len, &prefix,
		    &prefixlen)) == -1) {
			log_peer_warnx(&peer->conf, "bad nlri prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    NULL, 0);
			goto done;
		}
		if (prefixlen > 32) {
			log_peer_warnx(&peer->conf, "bad nlri prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    NULL, 0);
			goto done;
		}

		p += pos;
		nlri_len -= pos;

		if (peer->capa.mp[AID_INET] == 0) {
			log_peer_warnx(&peer->conf,
			    "bad update, %s disabled", aid2str(AID_INET));
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_OPTATTR,
			    NULL, 0);
			goto done;
		}

		rde_update_update(peer, asp, &prefix, prefixlen);

		/* max prefix checker */
		if (peer->conf.max_prefix &&
		    peer->prefix_cnt >= peer->conf.max_prefix) {
			log_peer_warnx(&peer->conf, "prefix limit reached");
			rde_update_err(peer, ERR_CEASE, ERR_CEASE_MAX_PREFIX,
			    NULL, 0);
			goto done;
		}

	}

	/* add MP_REACH_NLRI if available */
	if (mpa.reach_len != 0) {
		mpp = mpa.reach;
		mplen = mpa.reach_len;
		memcpy(&afi, mpp, 2);
		mpp += 2;
		mplen -= 2;
		afi = ntohs(afi);
		safi = *mpp++;
		mplen--;

		if (afi2aid(afi, safi, &aid) == -1) {
			log_peer_warnx(&peer->conf,
			    "bad AFI/SAFI pair in update");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_OPTATTR,
			    NULL, 0);
			goto done;
		}

		if (peer->capa.mp[aid] == 0) {
			log_peer_warnx(&peer->conf,
			    "bad update, %s disabled", aid2str(aid));
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_OPTATTR,
			    NULL, 0);
			goto done;
		}

		/*
		 * this works because asp is not linked.
		 * But first unlock the previously locked nexthop.
		 */
		if (asp->nexthop) {
			asp->nexthop->refcnt--;
			(void)nexthop_delete(asp->nexthop);
			asp->nexthop = NULL;
		}
		if ((pos = rde_get_mp_nexthop(mpp, mplen, aid, asp)) == -1) {
			log_peer_warnx(&peer->conf, "bad nlri prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_OPTATTR,
			    mpa.reach, mpa.reach_len);
			goto done;
		}
		mpp += pos;
		mplen -= pos;

		switch (aid) {
		case AID_INET6:
			while (mplen > 0) {
				if ((pos = rde_update_get_prefix6(mpp, mplen,
				    &prefix, &prefixlen)) == -1) {
					log_peer_warnx(&peer->conf,
					    "bad IPv6 nlri prefix");
					rde_update_err(peer, ERR_UPDATE,
					    ERR_UPD_OPTATTR,
					    mpa.reach, mpa.reach_len);
					goto done;
				}
				if (prefixlen > 128) {
					rde_update_err(peer, ERR_UPDATE,
					    ERR_UPD_OPTATTR,
					    mpa.reach, mpa.reach_len);
					goto done;
				}

				mpp += pos;
				mplen -= pos;

				rde_update_update(peer, asp, &prefix,
				    prefixlen);

				/* max prefix checker */
				if (peer->conf.max_prefix &&
				    peer->prefix_cnt >= peer->conf.max_prefix) {
					log_peer_warnx(&peer->conf,
					    "prefix limit reached");
					rde_update_err(peer, ERR_CEASE,
					    ERR_CEASE_MAX_PREFIX, NULL, 0);
					goto done;
				}

			}
			break;
		case AID_VPN_IPv4:
			while (mplen > 0) {
				if ((pos = rde_update_get_vpn4(mpp, mplen,
				    &prefix, &prefixlen)) == -1) {
					log_peer_warnx(&peer->conf,
					    "bad VPNv4 nlri prefix");
					rde_update_err(peer, ERR_UPDATE,
					    ERR_UPD_OPTATTR,
					    mpa.reach, mpa.reach_len);
					goto done;
				}
				if (prefixlen > 32) {
					rde_update_err(peer, ERR_UPDATE,
					    ERR_UPD_OPTATTR,
					    mpa.reach, mpa.reach_len);
					goto done;
				}

				mpp += pos;
				mplen -= pos;

				rde_update_update(peer, asp, &prefix,
				    prefixlen);

				/* max prefix checker */
				if (peer->conf.max_prefix &&
				    peer->prefix_cnt >= peer->conf.max_prefix) {
					log_peer_warnx(&peer->conf,
					    "prefix limit reached");
					rde_update_err(peer, ERR_CEASE,
					    ERR_CEASE_MAX_PREFIX, NULL, 0);
					goto done;
				}

			}
			break;
		default:
			/* silently ignore unsupported multiprotocol AF */
			break;
		}
	}

done:
	if (attrpath_len != 0) {
		/* unlock the previously locked entry */
		if (asp->nexthop) {
			asp->nexthop->refcnt--;
			(void)nexthop_delete(asp->nexthop);
		}
		/* free allocated attribute memory that is no longer used */
		path_put(asp);
	}

	return (error);
}

extern u_int16_t rib_size;

void
rde_update_update(struct rde_peer *peer, struct rde_aspath *asp,
    struct bgpd_addr *prefix, u_int8_t prefixlen)
{
	struct rde_aspath	*fasp;
	enum filter_actions	 action;
	int			 r = 0, f = 0;
	u_int16_t		 i;

	peer->prefix_rcvd_update++;
	/* add original path to the Adj-RIB-In */
	if (peer->conf.softreconfig_in)
		r += path_update(&ribs[0], peer, asp, prefix, prefixlen);

	for (i = 1; i < rib_size; i++) {
		/* input filter */
		action = rde_filter(i, &fasp, rules_l, peer, asp, prefix,
		    prefixlen, peer, DIR_IN);

		if (fasp == NULL)
			fasp = asp;

		if (action == ACTION_ALLOW) {
			rde_update_log("update", i, peer,
			    &fasp->nexthop->exit_nexthop, prefix, prefixlen);
			r += path_update(&ribs[i], peer, fasp, prefix,
			    prefixlen);
		} else if (prefix_remove(&ribs[i], peer, prefix, prefixlen,
		    0)) {
			rde_update_log("filtered withdraw", i, peer,
			    NULL, prefix, prefixlen);
			f++;
		}

		/* free modified aspath */
		if (fasp != asp)
			path_put(fasp);
	}

	if (r)
		peer->prefix_cnt++;
	else if (f)
		peer->prefix_cnt--;
}

void
rde_update_withdraw(struct rde_peer *peer, struct bgpd_addr *prefix,
    u_int8_t prefixlen)
{
	int r = 0;
	u_int16_t i;

	peer->prefix_rcvd_withdraw++;

	for (i = rib_size - 1; ; i--) {
		if (prefix_remove(&ribs[i], peer, prefix, prefixlen, 0)) {
			rde_update_log("withdraw", i, peer, NULL, prefix,
			    prefixlen);
			r++;
		}
		if (i == 0)
			break;
	}

	if (r)
		peer->prefix_cnt--;
}

/*
 * BGP UPDATE parser functions
 */

/* attribute parser specific makros */
#define UPD_READ(t, p, plen, n) \
	do { \
		memcpy(t, p, n); \
		p += n; \
		plen += n; \
	} while (0)

#define CHECK_FLAGS(s, t, m)	\
	(((s) & ~(ATTR_EXTLEN | (m))) == (t))

int
rde_attr_parse(u_char *p, u_int16_t len, struct rde_peer *peer,
    struct rde_aspath *a, struct mpattr *mpa)
{
	struct bgpd_addr nexthop;
	u_char		*op = p, *npath;
	u_int32_t	 tmp32;
	u_int16_t	 attr_len, nlen;
	u_int16_t	 plen = 0;
	u_int8_t	 flags;
	u_int8_t	 type;
	u_int8_t	 tmp8;

	if (len < 3) {
bad_len:
		rde_update_err(peer, ERR_UPDATE, ERR_UPD_ATTRLEN, op, len);
		return (-1);
	}

	UPD_READ(&flags, p, plen, 1);
	UPD_READ(&type, p, plen, 1);

	if (flags & ATTR_EXTLEN) {
		if (len - plen < 2)
			goto bad_len;
		UPD_READ(&attr_len, p, plen, 2);
		attr_len = ntohs(attr_len);
	} else {
		UPD_READ(&tmp8, p, plen, 1);
		attr_len = tmp8;
	}

	if (len - plen < attr_len)
		goto bad_len;

	/* adjust len to the actual attribute size including header */
	len = plen + attr_len;

	switch (type) {
	case ATTR_UNDEF:
		/* ignore and drop path attributes with a type code of 0 */
		break;
	case ATTR_ORIGIN:
		if (attr_len != 1)
			goto bad_len;

		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0)) {
bad_flags:
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_ATTRFLAGS,
			    op, len);
			return (-1);
		}

		UPD_READ(&a->origin, p, plen, 1);
		if (a->origin > ORIGIN_INCOMPLETE) {
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_ORIGIN,
			    op, len);
			return (-1);
		}
		if (a->flags & F_ATTR_ORIGIN)
			goto bad_list;
		a->flags |= F_ATTR_ORIGIN;
		break;
	case ATTR_ASPATH:
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			goto bad_flags;
		if (aspath_verify(p, attr_len, rde_as4byte(peer)) != 0) {
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_ASPATH,
			    NULL, 0);
			return (-1);
		}
		if (a->flags & F_ATTR_ASPATH)
			goto bad_list;
		if (rde_as4byte(peer)) {
			npath = p;
			nlen = attr_len;
		} else
			npath = aspath_inflate(p, attr_len, &nlen);
		a->flags |= F_ATTR_ASPATH;
		a->aspath = aspath_get(npath, nlen);
		if (npath != p)
			free(npath);
		plen += attr_len;
		break;
	case ATTR_NEXTHOP:
		if (attr_len != 4)
			goto bad_len;
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			goto bad_flags;
		if (a->flags & F_ATTR_NEXTHOP)
			goto bad_list;
		a->flags |= F_ATTR_NEXTHOP;

		bzero(&nexthop, sizeof(nexthop));
		nexthop.aid = AID_INET;
		UPD_READ(&nexthop.v4.s_addr, p, plen, 4);
		/*
		 * Check if the nexthop is a valid IP address. We consider
		 * multicast and experimental addresses as invalid.
		 */
		tmp32 = ntohl(nexthop.v4.s_addr);
		if (IN_MULTICAST(tmp32) || IN_BADCLASS(tmp32)) {
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    op, len);
			return (-1);
		}
		a->nexthop = nexthop_get(&nexthop);
		/*
		 * lock the nexthop because it is not yet linked else
		 * withdraws may remove this nexthop which in turn would
		 * cause a use after free error.
		 */
		a->nexthop->refcnt++;
		break;
	case ATTR_MED:
		if (attr_len != 4)
			goto bad_len;
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL, 0))
			goto bad_flags;
		if (a->flags & F_ATTR_MED)
			goto bad_list;
		a->flags |= F_ATTR_MED;

		UPD_READ(&tmp32, p, plen, 4);
		a->med = ntohl(tmp32);
		break;
	case ATTR_LOCALPREF:
		if (attr_len != 4)
			goto bad_len;
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			goto bad_flags;
		if (peer->conf.ebgp) {
			/* ignore local-pref attr on non ibgp peers */
			plen += 4;
			break;
		}
		if (a->flags & F_ATTR_LOCALPREF)
			goto bad_list;
		a->flags |= F_ATTR_LOCALPREF;

		UPD_READ(&tmp32, p, plen, 4);
		a->lpref = ntohl(tmp32);
		break;
	case ATTR_ATOMIC_AGGREGATE:
		if (attr_len != 0)
			goto bad_len;
		if (!CHECK_FLAGS(flags, ATTR_WELL_KNOWN, 0))
			goto bad_flags;
		goto optattr;
	case ATTR_AGGREGATOR:
		if ((!rde_as4byte(peer) && attr_len != 6) ||
		    (rde_as4byte(peer) && attr_len != 8)) {
			/*
			 * ignore attribute in case of error as per
			 * draft-ietf-idr-optional-transitive-00.txt
			 * but only if partial bit is set
			 */
			if ((flags & ATTR_PARTIAL) == 0)
				goto bad_len;
			plen += attr_len;
			break;
		}
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE,
		    ATTR_PARTIAL))
			goto bad_flags;
		if (!rde_as4byte(peer)) {
			/* need to inflate aggregator AS to 4-byte */
			u_char	t[8];
			t[0] = t[1] = 0;
			UPD_READ(&t[2], p, plen, 2);
			UPD_READ(&t[4], p, plen, 4);
			if (attr_optadd(a, flags, type, t,
			    sizeof(t)) == -1)
				goto bad_list;
			break;
		}
		/* 4-byte ready server take the default route */
		goto optattr;
	case ATTR_COMMUNITIES:
		if ((attr_len & 0x3) != 0) {
			/*
			 * mark update as bad and withdraw all routes as per
			 * draft-ietf-idr-optional-transitive-00.txt
			 * but only if partial bit is set
			 */
			if ((flags & ATTR_PARTIAL) == 0)
				goto bad_len;
			else
				a->flags |= F_ATTR_PARSE_ERR;
		}
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE,
		    ATTR_PARTIAL))
			goto bad_flags;
		goto optattr;
	case ATTR_EXT_COMMUNITIES:
		if ((attr_len & 0x7) != 0) {
			/*
			 * mark update as bad and withdraw all routes as per
			 * draft-ietf-idr-optional-transitive-00.txt
			 * but only if partial bit is set
			 */
			if ((flags & ATTR_PARTIAL) == 0)
				goto bad_len;
			else
				a->flags |= F_ATTR_PARSE_ERR;
		}
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE,
		    ATTR_PARTIAL))
			goto bad_flags;
		goto optattr;
	case ATTR_ORIGINATOR_ID:
		if (attr_len != 4)
			goto bad_len;
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL, 0))
			goto bad_flags;
		goto optattr;
	case ATTR_CLUSTER_LIST:
		if ((attr_len & 0x3) != 0)
			goto bad_len;
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL, 0))
			goto bad_flags;
		goto optattr;
	case ATTR_MP_REACH_NLRI:
		if (attr_len < 4)
			goto bad_len;
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL, 0))
			goto bad_flags;
		/* the validity is checked in rde_update_dispatch() */
		if (a->flags & F_ATTR_MP_REACH)
			goto bad_list;
		a->flags |= F_ATTR_MP_REACH;

		mpa->reach = p;
		mpa->reach_len = attr_len;
		plen += attr_len;
		break;
	case ATTR_MP_UNREACH_NLRI:
		if (attr_len < 3)
			goto bad_len;
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL, 0))
			goto bad_flags;
		/* the validity is checked in rde_update_dispatch() */
		if (a->flags & F_ATTR_MP_UNREACH)
			goto bad_list;
		a->flags |= F_ATTR_MP_UNREACH;

		mpa->unreach = p;
		mpa->unreach_len = attr_len;
		plen += attr_len;
		break;
	case ATTR_AS4_AGGREGATOR:
		if (attr_len != 8) {
			/* see ATTR_AGGREGATOR ... */
			if ((flags & ATTR_PARTIAL) == 0)
				goto bad_len;
			/* we should add a warning here */
			plen += attr_len;
			break;
		}
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE,
		    ATTR_PARTIAL))
			goto bad_flags;
		a->flags |= F_ATTR_AS4BYTE_NEW;
		goto optattr;
	case ATTR_AS4_PATH:
		if (!CHECK_FLAGS(flags, ATTR_OPTIONAL|ATTR_TRANSITIVE,
		    ATTR_PARTIAL))
			goto bad_flags;
		if (aspath_verify(p, attr_len, 1) != 0) {
			/*
			 * XXX RFC does not specify how to handle errors.
			 * XXX Instead of dropping the session because of a
			 * XXX bad path just mark the full update as having
			 * XXX a parse error which makes the update no longer
			 * XXX eligible and will not be considered for routing
			 * XXX or redistribution.
			 * XXX We follow draft-ietf-idr-optional-transitive
			 * XXX by looking at the partial bit.
			 */
			if (flags & ATTR_PARTIAL) {
				a->flags |= F_ATTR_PARSE_ERR;
				goto optattr;
			} else {
				rde_update_err(peer, ERR_UPDATE, ERR_UPD_ASPATH,
				    NULL, 0);
				return (-1);
			}
		}
		a->flags |= F_ATTR_AS4BYTE_NEW;
		goto optattr;
	default:
		if ((flags & ATTR_OPTIONAL) == 0) {
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_UNKNWN_WK_ATTR,
			    op, len);
			return (-1);
		}
optattr:
		if (attr_optadd(a, flags, type, p, attr_len) == -1) {
bad_list:
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_ATTRLIST,
			    NULL, 0);
			return (-1);
		}

		plen += attr_len;
		break;
	}

	return (plen);
}
#undef UPD_READ
#undef CHECK_FLAGS

u_int8_t
rde_attr_missing(struct rde_aspath *a, int ebgp, u_int16_t nlrilen)
{
	/* ATTR_MP_UNREACH_NLRI may be sent alone */
	if (nlrilen == 0 && a->flags & F_ATTR_MP_UNREACH &&
	    (a->flags & F_ATTR_MP_REACH) == 0)
		return (0);

	if ((a->flags & F_ATTR_ORIGIN) == 0)
		return (ATTR_ORIGIN);
	if ((a->flags & F_ATTR_ASPATH) == 0)
		return (ATTR_ASPATH);
	if ((a->flags & F_ATTR_MP_REACH) == 0 &&
	    (a->flags & F_ATTR_NEXTHOP) == 0)
		return (ATTR_NEXTHOP);
	if (!ebgp)
		if ((a->flags & F_ATTR_LOCALPREF) == 0)
			return (ATTR_LOCALPREF);
	return (0);
}

int
rde_get_mp_nexthop(u_char *data, u_int16_t len, u_int8_t aid,
    struct rde_aspath *asp)
{
	struct bgpd_addr	nexthop;
	u_int8_t		totlen, nhlen;

	if (len == 0)
		return (-1);

	nhlen = *data++;
	totlen = 1;
	len--;

	if (nhlen > len)
		return (-1);

	bzero(&nexthop, sizeof(nexthop));
	nexthop.aid = aid;
	switch (aid) {
	case AID_INET6:
		/*
		 * RFC2545 describes that there may be a link-local
		 * address carried in nexthop. Yikes!
		 * This is not only silly, it is wrong and we just ignore
		 * this link-local nexthop. The bgpd session doesn't run
		 * over the link-local address so why should all other
		 * traffic.
		 */
		if (nhlen != 16 && nhlen != 32) {
			log_warnx("bad multiprotocol nexthop, bad size");
			return (-1);
		}
		memcpy(&nexthop.v6.s6_addr, data, 16);
		break;
	case AID_VPN_IPv4:
		/*
		 * Neither RFC4364 nor RFC3107 specify the format of the
		 * nexthop in an explicit way. The quality of RFC went down
		 * the toilet the larger the the number got. 
		 * RFC4364 is very confusing about VPN-IPv4 address and the
		 * VPN-IPv4 prefix that carries also a MPLS label.
		 * So the nexthop is a 12-byte address with a 64bit RD and
		 * an IPv4 address following. In the nexthop case the RD can
		 * be ignored.
		 * Since the nexthop has to be in the main IPv4 table just
		 * create an AID_INET nexthop. So we don't need to handle
		 * AID_VPN_IPv4 in nexthop and kroute.
		 */
		if (nhlen != 12) {
			log_warnx("bad multiprotocol nexthop, bad size");
			return (-1);
		}
		data += sizeof(u_int64_t);
		nexthop.aid = AID_INET;
		memcpy(&nexthop.v4, data, sizeof(nexthop.v4));
		break;
	default:
		log_warnx("bad multiprotocol nexthop, bad AID");
		return (-1);
	}

	asp->nexthop = nexthop_get(&nexthop);
	/*
	 * lock the nexthop because it is not yet linked else
	 * withdraws may remove this nexthop which in turn would
	 * cause a use after free error.
	 */
	asp->nexthop->refcnt++;

	/* ignore reserved (old SNPA) field as per RFC4760 */
	totlen += nhlen + 1;
	data += nhlen + 1;

	return (totlen);
}

int
rde_update_extract_prefix(u_char *p, u_int16_t len, void *va,
    u_int8_t pfxlen, u_int8_t max)
{
	static u_char addrmask[] = {
	    0x00, 0x80, 0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff };
	u_char		*a = va;
	int		 i;
	u_int16_t	 plen = 0;

	for (i = 0; pfxlen && i < max; i++) {
		if (len <= plen)
			return (-1);
		if (pfxlen < 8) {
			a[i] = *p++ & addrmask[pfxlen];
			plen++;
			break;
		} else {
			a[i] = *p++;
			plen++;
			pfxlen -= 8;
		}
	}
	return (plen);
}

int
rde_update_get_prefix(u_char *p, u_int16_t len, struct bgpd_addr *prefix,
    u_int8_t *prefixlen)
{
	u_int8_t	 pfxlen;
	int		 plen;

	if (len < 1)
		return (-1);

	pfxlen = *p++;
	len--;

	bzero(prefix, sizeof(struct bgpd_addr));
	prefix->aid = AID_INET;
	*prefixlen = pfxlen;

	if ((plen = rde_update_extract_prefix(p, len, &prefix->v4, pfxlen,
	    sizeof(prefix->v4))) == -1)
		return (-1);

	return (plen + 1);	/* pfxlen needs to be added */
}

int
rde_update_get_prefix6(u_char *p, u_int16_t len, struct bgpd_addr *prefix,
    u_int8_t *prefixlen)
{
	int		plen;
	u_int8_t	pfxlen;

	if (len < 1)
		return (-1);

	pfxlen = *p++;
	len--;

	bzero(prefix, sizeof(struct bgpd_addr));
	prefix->aid = AID_INET6;
	*prefixlen = pfxlen;

	if ((plen = rde_update_extract_prefix(p, len, &prefix->v6, pfxlen,
	    sizeof(prefix->v6))) == -1)
		return (-1);

	return (plen + 1);	/* pfxlen needs to be added */
}

int
rde_update_get_vpn4(u_char *p, u_int16_t len, struct bgpd_addr *prefix,
    u_int8_t *prefixlen)
{
	int		 rv, done = 0;
	u_int8_t	 pfxlen;
	u_int16_t	 plen;

	if (len < 1)
		return (-1);

	memcpy(&pfxlen, p, 1);
	p += 1;
	plen = 1;

	bzero(prefix, sizeof(struct bgpd_addr));

	/* label stack */
	do {
		if (len - plen < 3 || pfxlen < 3 * 8)
			return (-1);
		if (prefix->vpn4.labellen + 3U >
		    sizeof(prefix->vpn4.labelstack))
			return (-1);
		prefix->vpn4.labelstack[prefix->vpn4.labellen++] = *p++;
		prefix->vpn4.labelstack[prefix->vpn4.labellen++] = *p++;
		prefix->vpn4.labelstack[prefix->vpn4.labellen] = *p++;
		if (prefix->vpn4.labelstack[prefix->vpn4.labellen] &
		    BGP_MPLS_BOS)
			done = 1;
		prefix->vpn4.labellen++;
		plen += 3;
		pfxlen -= 3 * 8;
	} while (!done);

	/* RD */
	if (len - plen < (int)sizeof(u_int64_t) ||
	    pfxlen < sizeof(u_int64_t) * 8)
		return (-1);
	memcpy(&prefix->vpn4.rd, p, sizeof(u_int64_t));
	pfxlen -= sizeof(u_int64_t) * 8;
	p += sizeof(u_int64_t);
	plen += sizeof(u_int64_t);

	/* prefix */
	prefix->aid = AID_VPN_IPv4;
	*prefixlen = pfxlen;

	if ((rv = rde_update_extract_prefix(p, len, &prefix->vpn4.addr,
	    pfxlen, sizeof(prefix->vpn4.addr))) == -1)
		return (-1);

	return (plen + rv);
}

void
rde_update_err(struct rde_peer *peer, u_int8_t error, u_int8_t suberr,
    void *data, u_int16_t size)
{
	struct buf	*wbuf;

	if ((wbuf = imsg_create(ibuf_se, IMSG_UPDATE_ERR, peer->conf.id, 0,
	    size + sizeof(error) + sizeof(suberr))) == NULL)
		fatal("imsg_create error");
	if (imsg_add(wbuf, &error, sizeof(error)) == -1 ||
	    imsg_add(wbuf, &suberr, sizeof(suberr)) == -1 ||
	    imsg_add(wbuf, data, size) == -1)
		fatal("imsg_add error");
	imsg_close(ibuf_se, wbuf);
	peer->state = PEER_ERR;
}

void
rde_update_log(const char *message, u_int16_t rid,
    const struct rde_peer *peer, const struct bgpd_addr *next,
    const struct bgpd_addr *prefix, u_int8_t prefixlen)
{
	char		*l = NULL;
	char		*n = NULL;
	char		*p = NULL;

	if (!(conf->log & BGPD_LOG_UPDATES))
		return;

	if (next != NULL)
		if (asprintf(&n, " via %s", log_addr(next)) == -1)
			n = NULL;
	if (asprintf(&p, "%s/%u", log_addr(prefix), prefixlen) == -1)
		p = NULL;
	l = log_fmt_peer(&peer->conf);
	log_info("Rib %s: %s AS%s: %s %s%s", ribs[rid].name,
	    l, log_as(peer->conf.remote_as), message,
	    p ? p : "out of memory", n ? n : "");

	free(l);
	free(n);
	free(p);
}

/*
 * 4-Byte ASN helper function.
 * Two scenarios need to be considered:
 * - NEW session with NEW attributes present -> just remove the attributes
 * - OLD session with NEW attributes present -> try to merge them
 */
void
rde_as4byte_fixup(struct rde_peer *peer, struct rde_aspath *a)
{
	struct attr	*nasp, *naggr, *oaggr;
	u_int32_t	 as;

	/*
	 * if either ATTR_AS4_AGGREGATOR or ATTR_AS4_PATH is present
	 * try to fixup the attributes.
	 * Do not fixup if F_ATTR_PARSE_ERR is set.
	 */
	if (!(a->flags & F_ATTR_AS4BYTE_NEW) || a->flags & F_ATTR_PARSE_ERR)
		return;

	/* first get the attributes */
	nasp = attr_optget(a, ATTR_AS4_PATH);
	naggr = attr_optget(a, ATTR_AS4_AGGREGATOR);

	if (rde_as4byte(peer)) {
		/* NEW session using 4-byte ASNs */
		if (nasp)
			attr_free(a, nasp);
		if (naggr)
			attr_free(a, naggr);
		return;
	}
	/* OLD session using 2-byte ASNs */
	/* try to merge the new attributes into the old ones */
	if ((oaggr = attr_optget(a, ATTR_AGGREGATOR))) {
		memcpy(&as, oaggr->data, sizeof(as));
		if (ntohl(as) != AS_TRANS) {
			/* per RFC ignore AS4_PATH and AS4_AGGREGATOR */
			if (nasp)
				attr_free(a, nasp);
			if (naggr)
				attr_free(a, naggr);
			return;
		}
		if (naggr) {
			/* switch over to new AGGREGATOR */
			attr_free(a, oaggr);
			if (attr_optadd(a, ATTR_OPTIONAL | ATTR_TRANSITIVE,
			    ATTR_AGGREGATOR, naggr->data, naggr->len))
				fatalx("attr_optadd failed but impossible");
		}
	}
	/* there is no need for AS4_AGGREGATOR any more */
	if (naggr)
		attr_free(a, naggr);

	/* merge AS4_PATH with ASPATH */
	if (nasp)
		aspath_merge(a, nasp);
}


/*
 * route reflector helper function
 */
void
rde_reflector(struct rde_peer *peer, struct rde_aspath *asp)
{
	struct attr	*a;
	u_int8_t	*p;
	u_int16_t	 len;
	u_int32_t	 id;

	/* do not consider updates with parse errors */
	if (asp->flags & F_ATTR_PARSE_ERR)
		return;

	/* check for originator id if eq router_id drop */
	if ((a = attr_optget(asp, ATTR_ORIGINATOR_ID)) != NULL) {
		if (memcmp(&conf->bgpid, a->data, sizeof(conf->bgpid)) == 0) {
			/* this is coming from myself */
			asp->flags |= F_ATTR_LOOP;
			return;
		}
	} else if (conf->flags & BGPD_FLAG_REFLECTOR) {
		if (peer->conf.ebgp == 0)
			id = htonl(peer->remote_bgpid);
		else
			id = conf->bgpid;
		if (attr_optadd(asp, ATTR_OPTIONAL, ATTR_ORIGINATOR_ID,
		    &id, sizeof(u_int32_t)) == -1)
			fatalx("attr_optadd failed but impossible");
	}

	/* check for own id in the cluster list */
	if (conf->flags & BGPD_FLAG_REFLECTOR) {
		if ((a = attr_optget(asp, ATTR_CLUSTER_LIST)) != NULL) {
			for (len = 0; len < a->len;
			    len += sizeof(conf->clusterid))
				/* check if coming from my cluster */
				if (memcmp(&conf->clusterid, a->data + len,
				    sizeof(conf->clusterid)) == 0) {
					asp->flags |= F_ATTR_LOOP;
					return;
				}

			/* prepend own clusterid by replacing attribute */
			len = a->len + sizeof(conf->clusterid);
			if (len < a->len)
				fatalx("rde_reflector: cluster-list overflow");
			if ((p = malloc(len)) == NULL)
				fatal("rde_reflector");
			memcpy(p, &conf->clusterid, sizeof(conf->clusterid));
			memcpy(p + sizeof(conf->clusterid), a->data, a->len);
			attr_free(asp, a);
			if (attr_optadd(asp, ATTR_OPTIONAL, ATTR_CLUSTER_LIST,
			    p, len) == -1)
				fatalx("attr_optadd failed but impossible");
			free(p);
		} else if (attr_optadd(asp, ATTR_OPTIONAL, ATTR_CLUSTER_LIST,
		    &conf->clusterid, sizeof(conf->clusterid)) == -1)
			fatalx("attr_optadd failed but impossible");
	}
}

/*
 * control specific functions
 */
void
rde_dump_rib_as(struct prefix *p, struct rde_aspath *asp, pid_t pid, int flags)
{
	struct ctl_show_rib	 rib;
	struct buf		*wbuf;
	struct attr		*a;
	void			*bp;
	u_int8_t		 l;

	bzero(&rib, sizeof(rib));
	rib.lastchange = p->lastchange;
	rib.local_pref = asp->lpref;
	rib.med = asp->med;
	rib.prefix_cnt = asp->prefix_cnt;
	rib.active_cnt = asp->active_cnt;
	strlcpy(rib.descr, asp->peer->conf.descr, sizeof(rib.descr));
	memcpy(&rib.remote_addr, &asp->peer->remote_addr,
	    sizeof(rib.remote_addr));
	rib.remote_id = asp->peer->remote_bgpid;
	if (asp->nexthop != NULL) {
		memcpy(&rib.true_nexthop, &asp->nexthop->true_nexthop,
		    sizeof(rib.true_nexthop));
		memcpy(&rib.exit_nexthop, &asp->nexthop->exit_nexthop,
		    sizeof(rib.exit_nexthop));
	} else {
		/* announced network may have a NULL nexthop */
		bzero(&rib.true_nexthop, sizeof(rib.true_nexthop));
		bzero(&rib.exit_nexthop, sizeof(rib.exit_nexthop));
		rib.true_nexthop.aid = p->prefix->aid;
		rib.exit_nexthop.aid = p->prefix->aid;
	}
	pt_getaddr(p->prefix, &rib.prefix);
	rib.prefixlen = p->prefix->prefixlen;
	rib.origin = asp->origin;
	rib.flags = 0;
	if (p->rib->active == p)
		rib.flags |= F_PREF_ACTIVE;
	if (asp->peer->conf.ebgp == 0)
		rib.flags |= F_PREF_INTERNAL;
	if (asp->flags & F_PREFIX_ANNOUNCED)
		rib.flags |= F_PREF_ANNOUNCE;
	if (asp->nexthop == NULL || asp->nexthop->state == NEXTHOP_REACH)
		rib.flags |= F_PREF_ELIGIBLE;
	if (asp->flags & F_ATTR_LOOP)
		rib.flags &= ~F_PREF_ELIGIBLE;
	rib.aspath_len = aspath_length(asp->aspath);

	if ((wbuf = imsg_create(ibuf_se_ctl, IMSG_CTL_SHOW_RIB, 0, pid,
	    sizeof(rib) + rib.aspath_len)) == NULL)
		return;
	if (imsg_add(wbuf, &rib, sizeof(rib)) == -1 ||
	    imsg_add(wbuf, aspath_dump(asp->aspath),
	    rib.aspath_len) == -1)
		return;
	imsg_close(ibuf_se_ctl, wbuf);

	if (flags & F_CTL_DETAIL)
		for (l = 0; l < asp->others_len; l++) {
			if ((a = asp->others[l]) == NULL)
				break;
			if ((wbuf = imsg_create(ibuf_se_ctl,
			    IMSG_CTL_SHOW_RIB_ATTR, 0, pid,
			    attr_optlen(a))) == NULL)
				return;
			if ((bp = buf_reserve(wbuf, attr_optlen(a))) == NULL) {
				buf_free(wbuf);
				return;
			}
			if (attr_write(bp, attr_optlen(a), a->flags,
			    a->type, a->data, a->len) == -1) {
				buf_free(wbuf);
				return;
			}
			imsg_close(ibuf_se_ctl, wbuf);
		}
}

void
rde_dump_filterout(struct rde_peer *peer, struct prefix *p,
    struct ctl_show_rib_request *req)
{
	struct bgpd_addr	 addr;
	struct rde_aspath	*asp;
	enum filter_actions	 a;

	if (up_test_update(peer, p) != 1)
		return;

	pt_getaddr(p->prefix, &addr);
	a = rde_filter(1 /* XXX */, &asp, rules_l, peer, p->aspath, &addr,
	    p->prefix->prefixlen, p->aspath->peer, DIR_OUT);
	if (asp)
		asp->peer = p->aspath->peer;
	else
		asp = p->aspath;

	if (a == ACTION_ALLOW)
		rde_dump_rib_as(p, asp, req->pid, req->flags);

	if (asp != p->aspath)
		path_put(asp);
}

void
rde_dump_filter(struct prefix *p, struct ctl_show_rib_request *req)
{
	struct rde_peer		*peer;

	if (req->flags & F_CTL_ADJ_IN || 
	    !(req->flags & (F_CTL_ADJ_IN|F_CTL_ADJ_OUT))) {
		if (req->peerid && req->peerid != p->aspath->peer->conf.id)
			return;
		if (req->type == IMSG_CTL_SHOW_RIB_AS && 
		    !aspath_match(p->aspath->aspath, req->as.type, req->as.as))
			return;
		if (req->type == IMSG_CTL_SHOW_RIB_COMMUNITY &&
		    !community_match(p->aspath, req->community.as,
		    req->community.type))
			return;
		rde_dump_rib_as(p, p->aspath, req->pid, req->flags);
	} else if (req->flags & F_CTL_ADJ_OUT) {
		if (p->rib->active != p)
			/* only consider active prefix */
			return;
		if (req->peerid) {
			if ((peer = peer_get(req->peerid)) != NULL)
				rde_dump_filterout(peer, p, req);
			return;
		}
	}
}

void
rde_dump_upcall(struct rib_entry *re, void *ptr)
{
	struct prefix		*p;
	struct rde_dump_ctx	*ctx = ptr;

	LIST_FOREACH(p, &re->prefix_h, rib_l)
		rde_dump_filter(p, &ctx->req);
}

void
rde_dump_prefix_upcall(struct rib_entry *re, void *ptr)
{
	struct rde_dump_ctx	*ctx = ptr;
	struct prefix		*p;
	struct pt_entry		*pt;
	struct bgpd_addr	 addr;

	pt = re->prefix;
	pt_getaddr(pt, &addr);
	if (addr.aid != ctx->req.prefix.aid)
		return;
	if (ctx->req.prefixlen > pt->prefixlen)
		return;
	if (!prefix_compare(&ctx->req.prefix, &addr, ctx->req.prefixlen))
		LIST_FOREACH(p, &re->prefix_h, rib_l)
			rde_dump_filter(p, &ctx->req);
}

void
rde_dump_ctx_new(struct ctl_show_rib_request *req, pid_t pid,
    enum imsg_type type)
{
	struct rde_dump_ctx	*ctx;
	struct rib_entry	*re;
	u_int			 error;
	u_int16_t		 id;
	u_int8_t		 hostplen;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		log_warn("rde_dump_ctx_new");
		error = CTL_RES_NOMEM;
		imsg_compose(ibuf_se_ctl, IMSG_CTL_RESULT, 0, pid, -1, &error,
		    sizeof(error));
		return;
	}
	if ((id = rib_find(req->rib)) == RIB_FAILED) {
		log_warnx("rde_dump_ctx_new: no such rib %s", req->rib);
		error = CTL_RES_NOSUCHPEER;
		imsg_compose(ibuf_se_ctl, IMSG_CTL_RESULT, 0, pid, -1, &error,
		    sizeof(error));
		free(ctx);
		return;
	}

	memcpy(&ctx->req, req, sizeof(struct ctl_show_rib_request));
	ctx->req.pid = pid;
	ctx->req.type = type;
	ctx->ribctx.ctx_count = RDE_RUNNER_ROUNDS;
	ctx->ribctx.ctx_rib = &ribs[id];
	switch (ctx->req.type) {
	case IMSG_CTL_SHOW_NETWORK:
		ctx->ribctx.ctx_upcall = network_dump_upcall;
		break;
	case IMSG_CTL_SHOW_RIB:
	case IMSG_CTL_SHOW_RIB_AS:
	case IMSG_CTL_SHOW_RIB_COMMUNITY:
		ctx->ribctx.ctx_upcall = rde_dump_upcall;
		break;
	case IMSG_CTL_SHOW_RIB_PREFIX:
		if (req->flags & F_LONGER) {
			ctx->ribctx.ctx_upcall = rde_dump_prefix_upcall;
			break;
		}
		switch (req->prefix.aid) {
		case AID_INET:
		case AID_VPN_IPv4:
			hostplen = 32;
			break;
		case AID_INET6:
			hostplen = 128;
			break;
		default:
			fatalx("rde_dump_ctx_new: unknown af");
		}
		if (req->prefixlen == hostplen)
			re = rib_lookup(&ribs[id], &req->prefix);
		else
			re = rib_get(&ribs[id], &req->prefix, req->prefixlen);
		if (re)
			rde_dump_upcall(re, ctx);
		rde_dump_done(ctx);
		return;
	default:
		fatalx("rde_dump_ctx_new: unsupported imsg type");
	}
	ctx->ribctx.ctx_done = rde_dump_done;
	ctx->ribctx.ctx_arg = ctx;
	ctx->ribctx.ctx_aid = ctx->req.aid;
	rib_dump_r(&ctx->ribctx);
}

void
rde_dump_done(void *arg)
{
	struct rde_dump_ctx	*ctx = arg;

	imsg_compose(ibuf_se_ctl, IMSG_CTL_END, 0, ctx->req.pid,
	    -1, NULL, 0);
	free(ctx);
}

void
rde_dump_mrt_new(struct mrt *mrt, pid_t pid, int fd)
{
	struct rde_mrt_ctx	*ctx;
	u_int16_t		 id;

	if ((ctx = calloc(1, sizeof(*ctx))) == NULL) {
		log_warn("rde_dump_mrt_new");
		return;
	}
	memcpy(&ctx->mrt, mrt, sizeof(struct mrt));
	TAILQ_INIT(&ctx->mrt.wbuf.bufs);
	ctx->mrt.wbuf.fd = fd;
	ctx->mrt.state = MRT_STATE_RUNNING;
	id = rib_find(ctx->mrt.rib);
	if (id == RIB_FAILED) {
		log_warnx("non existing RIB %s for mrt dump", ctx->mrt.rib);
		free(ctx);
		return;
	}
	ctx->ribctx.ctx_count = RDE_RUNNER_ROUNDS;
	ctx->ribctx.ctx_rib = &ribs[id];
	ctx->ribctx.ctx_upcall = mrt_dump_upcall;
	ctx->ribctx.ctx_done = mrt_done;
	ctx->ribctx.ctx_arg = &ctx->mrt;
	ctx->ribctx.ctx_aid = AID_UNSPEC;
	LIST_INSERT_HEAD(&rde_mrts, ctx, entry);
	rde_mrt_cnt++;
	rib_dump_r(&ctx->ribctx);
}

/*
 * kroute specific functions
 */
int
rde_rdomain_import(struct rde_aspath *asp, struct rdomain *rd)
{
	struct filter_set	*s;

	TAILQ_FOREACH(s, &rd->import, entry) {
		if (community_ext_match(asp, &s->action.ext_community, 0))
			return (1);
	}
	return (0);
}

void
rde_send_kroute(struct prefix *new, struct prefix *old, u_int16_t ribid)
{
	struct kroute_full	 kr;
	struct bgpd_addr	 addr;
	struct prefix		*p;
	struct rdomain		*rd;
	enum imsg_type		 type;

	/*
	 * Make sure that self announce prefixes are not commited to the
	 * FIB. If both prefixes are unreachable no update is needed.
	 */
	if ((old == NULL || old->aspath->flags & F_PREFIX_ANNOUNCED) &&
	    (new == NULL || new->aspath->flags & F_PREFIX_ANNOUNCED))
		return;

	if (new == NULL || new->aspath->flags & F_PREFIX_ANNOUNCED) {
		type = IMSG_KROUTE_DELETE;
		p = old;
	} else {
		type = IMSG_KROUTE_CHANGE;
		p = new;
	}

	pt_getaddr(p->prefix, &addr);
	bzero(&kr, sizeof(kr));
	memcpy(&kr.prefix, &addr, sizeof(kr.prefix));
	kr.prefixlen = p->prefix->prefixlen;
	if (p->aspath->flags & F_NEXTHOP_REJECT)
		kr.flags |= F_REJECT;
	if (p->aspath->flags & F_NEXTHOP_BLACKHOLE)
		kr.flags |= F_BLACKHOLE;
	if (type == IMSG_KROUTE_CHANGE)
		memcpy(&kr.nexthop, &p->aspath->nexthop->true_nexthop,
		    sizeof(kr.nexthop));
	strlcpy(kr.label, rtlabel_id2name(p->aspath->rtlabelid),
	    sizeof(kr.label));

	switch (addr.aid) {
	case AID_VPN_IPv4:
		if (ribid != 1)
			/* not Loc-RIB, no update for VPNs */
			break;

		SIMPLEQ_FOREACH(rd, rdomains_l, entry) {
			if (addr.vpn4.rd != rd->rd)
				continue;
			if (!rde_rdomain_import(p->aspath, rd))
				continue;
			/* must send exit_nexthop so that correct MPLS tunnel
			 * is chosen
			 */
			if (type == IMSG_KROUTE_CHANGE)
				memcpy(&kr.nexthop,
				    &p->aspath->nexthop->exit_nexthop,
				    sizeof(kr.nexthop));
			if (imsg_compose(ibuf_main, type, rd->rtableid, 0, -1,
			    &kr, sizeof(kr)) == -1)
				fatal("imsg_compose error");
		}
		break;
	default:
		if (imsg_compose(ibuf_main, type, ribs[ribid].rtableid, 0, -1,
		    &kr, sizeof(kr)) == -1)
			fatal("imsg_compose error");
		break;
	}
}

/*
 * pf table specific functions
 */
void
rde_send_pftable(u_int16_t id, struct bgpd_addr *addr,
    u_int8_t len, int del)
{
	struct pftable_msg pfm;

	if (id == 0)
		return;

	/* do not run while cleaning up */
	if (rde_quit)
		return;

	bzero(&pfm, sizeof(pfm));
	strlcpy(pfm.pftable, pftable_id2name(id), sizeof(pfm.pftable));
	memcpy(&pfm.addr, addr, sizeof(pfm.addr));
	pfm.len = len;

	if (imsg_compose(ibuf_main,
	    del ? IMSG_PFTABLE_REMOVE : IMSG_PFTABLE_ADD,
	    0, 0, -1, &pfm, sizeof(pfm)) == -1)
		fatal("imsg_compose error");
}

void
rde_send_pftable_commit(void)
{
	/* do not run while cleaning up */
	if (rde_quit)
		return;

	if (imsg_compose(ibuf_main, IMSG_PFTABLE_COMMIT, 0, 0, -1, NULL, 0) ==
	    -1)
		fatal("imsg_compose error");
}

/*
 * nexthop specific functions
 */
void
rde_send_nexthop(struct bgpd_addr *next, int valid)
{
	int			 type;

	if (valid)
		type = IMSG_NEXTHOP_ADD;
	else
		type = IMSG_NEXTHOP_REMOVE;

	if (imsg_compose(ibuf_main, type, 0, 0, -1, next,
	    sizeof(struct bgpd_addr)) == -1)
		fatal("imsg_compose error");
}

/*
 * soft reconfig specific functions
 */
void
rde_softreconfig_out(struct rib_entry *re, void *ptr)
{
	struct prefix		*p = re->active;
	struct pt_entry		*pt;
	struct rde_peer		*peer;
	struct rde_aspath	*oasp, *nasp;
	enum filter_actions	 oa, na;
	struct bgpd_addr	 addr;

	if (p == NULL)
		return;

	pt = re->prefix;
	pt_getaddr(pt, &addr);
	LIST_FOREACH(peer, &peerlist, peer_l) {
		if (peer->conf.id == 0)
			continue;
		if (peer->ribid != re->ribid)
			continue;
		if (peer->reconf_out == 0)
			continue;
		if (up_test_update(peer, p) != 1)
			continue;

		oa = rde_filter(re->ribid, &oasp, rules_l, peer, p->aspath,
		    &addr, pt->prefixlen, p->aspath->peer, DIR_OUT);
		na = rde_filter(re->ribid, &nasp, newrules, peer, p->aspath,
		    &addr, pt->prefixlen, p->aspath->peer, DIR_OUT);
		oasp = oasp != NULL ? oasp : p->aspath;
		nasp = nasp != NULL ? nasp : p->aspath;

		if (oa == ACTION_DENY && na == ACTION_DENY)
			/* nothing todo */
			goto done;
		if (oa == ACTION_DENY && na == ACTION_ALLOW) {
			/* send update */
			up_generate(peer, nasp, &addr, pt->prefixlen);
			goto done;
		}
		if (oa == ACTION_ALLOW && na == ACTION_DENY) {
			/* send withdraw */
			up_generate(peer, NULL, &addr, pt->prefixlen);
			goto done;
		}
		if (oa == ACTION_ALLOW && na == ACTION_ALLOW) {
			if (path_compare(nasp, oasp) == 0)
				goto done;
			/* send update */
			up_generate(peer, nasp, &addr, pt->prefixlen);
		}

done:
		if (oasp != p->aspath)
			path_put(oasp);
		if (nasp != p->aspath)
			path_put(nasp);
	}
}

void
rde_softreconfig_in(struct rib_entry *re, void *ptr)
{
	struct prefix		*p, *np;
	struct pt_entry		*pt;
	struct rde_peer		*peer;
	struct rde_aspath	*asp, *oasp, *nasp;
	enum filter_actions	 oa, na;
	struct bgpd_addr	 addr;
	u_int16_t		 i;

	pt = re->prefix;
	pt_getaddr(pt, &addr);
	for (p = LIST_FIRST(&re->prefix_h); p != NULL; p = np) {
		np = LIST_NEXT(p, rib_l);

		/* store aspath as prefix may change till we're done */
		asp = p->aspath;
		peer = asp->peer;

		/* XXX how can this happen ??? */
		if (peer->reconf_in == 0)
			continue;

		for (i = 1; i < rib_size; i++) {
			/* only active ribs need a softreconfig rerun */
			if (ribs[i].state != RECONF_KEEP)
				continue;

			/* check if prefix changed */
			oa = rde_filter(i, &oasp, rules_l, peer, asp, &addr,
			    pt->prefixlen, peer, DIR_IN);
			na = rde_filter(i, &nasp, newrules, peer, asp, &addr,
			    pt->prefixlen, peer, DIR_IN);
			oasp = oasp != NULL ? oasp : asp;
			nasp = nasp != NULL ? nasp : asp;

			if (oa == ACTION_DENY && na == ACTION_DENY)
				/* nothing todo */
				goto done;
			if (oa == ACTION_DENY && na == ACTION_ALLOW) {
				/* update Local-RIB */
				path_update(&ribs[i], peer, nasp, &addr,
				    pt->prefixlen);
				goto done;
			}
			if (oa == ACTION_ALLOW && na == ACTION_DENY) {
				/* remove from Local-RIB */
				prefix_remove(&ribs[i], peer, &addr,
				    pt->prefixlen, 0);
				goto done;
			}
			if (oa == ACTION_ALLOW && na == ACTION_ALLOW) {
				if (path_compare(nasp, oasp) == 0)
					goto done;
				/* send update */
				path_update(&ribs[i], peer, nasp, &addr,
				    pt->prefixlen);
			}

done:
			if (oasp != asp)
				path_put(oasp);
			if (nasp != asp)
				path_put(nasp);
		}
	}
}

void
rde_softreconfig_load(struct rib_entry *re, void *ptr)
{
	struct rib		*rib = ptr;
	struct prefix		*p, *np;
	struct pt_entry		*pt;
	struct rde_peer		*peer;
	struct rde_aspath	*asp, *nasp;
	enum filter_actions	 action;
	struct bgpd_addr	 addr;

	pt = re->prefix;
	pt_getaddr(pt, &addr);
	for (p = LIST_FIRST(&re->prefix_h); p != NULL; p = np) {
		np = LIST_NEXT(p, rib_l);

		/* store aspath as prefix may change till we're done */
		asp = p->aspath;
		peer = asp->peer;

		action = rde_filter(rib->id, &nasp, newrules, peer, asp, &addr,
		    pt->prefixlen, peer, DIR_IN);
		nasp = nasp != NULL ? nasp : asp;

		if (action == ACTION_ALLOW) {
			/* update Local-RIB */
			path_update(rib, peer, nasp, &addr, pt->prefixlen);
		}

		if (nasp != asp)
			path_put(nasp);
	}
}

/*
 * update specific functions
 */
u_char	queue_buf[4096];

void
rde_up_dump_upcall(struct rib_entry *re, void *ptr)
{
	struct rde_peer		*peer = ptr;

	if (re->ribid != peer->ribid)
		fatalx("King Bula: monsterous evil horror.");
	if (re->active == NULL)
		return;
	up_generate_updates(rules_l, peer, re->active, NULL);
}

void
rde_generate_updates(u_int16_t ribid, struct prefix *new, struct prefix *old)
{
	struct rde_peer			*peer;

	/*
	 * If old is != NULL we know it was active and should be removed.
	 * If new is != NULL we know it is reachable and then we should 
	 * generate an update.
	 */
	if (old == NULL && new == NULL)
		return;

	LIST_FOREACH(peer, &peerlist, peer_l) {
		if (peer->conf.id == 0)
			continue;
		if (peer->ribid != ribid)
			continue;
		if (peer->state != PEER_UP)
			continue;
		up_generate_updates(rules_l, peer, new, old);
	}
}

void
rde_update_queue_runner(void)
{
	struct rde_peer		*peer;
	int			 r, sent, max = RDE_RUNNER_ROUNDS, eor = 0;
	u_int16_t		 len, wd_len, wpos;

	len = sizeof(queue_buf) - MSGSIZE_HEADER;
	do {
		sent = 0;
		LIST_FOREACH(peer, &peerlist, peer_l) {
			if (peer->conf.id == 0)
				continue;
			if (peer->state != PEER_UP)
				continue;
			/* first withdraws */
			wpos = 2; /* reserve space for the length field */
			r = up_dump_prefix(queue_buf + wpos, len - wpos - 2,
			    &peer->withdraws[AID_INET], peer);
			wd_len = r;
			/* write withdraws length filed */
			wd_len = htons(wd_len);
			memcpy(queue_buf, &wd_len, 2);
			wpos += r;

			/* now bgp path attributes */
			r = up_dump_attrnlri(queue_buf + wpos, len - wpos,
			    peer);
			switch (r) {
			case -1:
				eor = 1;
				if (wd_len == 0) {
					/* no withdraws queued just send EoR */
					peer_send_eor(peer, AID_INET);
					continue;
				}
				break;
			case 2:
				if (wd_len == 0) {
					/*
					 * No packet to send. No withdraws and
					 * no path attributes. Skip.
					 */
					continue;
				}
				/* FALLTHROUGH */
			default:
				wpos += r;
				break;
			}

			/* finally send message to SE */
			if (imsg_compose(ibuf_se, IMSG_UPDATE, peer->conf.id,
			    0, -1, queue_buf, wpos) == -1)
				fatal("imsg_compose error");
			sent++;
			if (eor) {
				eor = 0;
				peer_send_eor(peer, AID_INET);
			}
		}
		max -= sent;
	} while (sent != 0 && max > 0);
}

void
rde_update6_queue_runner(u_int8_t aid)
{
	struct rde_peer		*peer;
	u_char			*b;
	int			 r, sent, max = RDE_RUNNER_ROUNDS / 2;
	u_int16_t		 len;

	/* first withdraws ... */
	do {
		sent = 0;
		LIST_FOREACH(peer, &peerlist, peer_l) {
			if (peer->conf.id == 0)
				continue;
			if (peer->state != PEER_UP)
				continue;
			len = sizeof(queue_buf) - MSGSIZE_HEADER;
			b = up_dump_mp_unreach(queue_buf, &len, peer, aid);

			if (b == NULL)
				continue;
			/* finally send message to SE */
			if (imsg_compose(ibuf_se, IMSG_UPDATE, peer->conf.id,
			    0, -1, b, len) == -1)
				fatal("imsg_compose error");
			sent++;
		}
		max -= sent;
	} while (sent != 0 && max > 0);

	/* ... then updates */
	max = RDE_RUNNER_ROUNDS / 2;
	do {
		sent = 0;
		LIST_FOREACH(peer, &peerlist, peer_l) {
			if (peer->conf.id == 0)
				continue;
			if (peer->state != PEER_UP)
				continue;
			len = sizeof(queue_buf) - MSGSIZE_HEADER;
			r = up_dump_mp_reach(queue_buf, &len, peer, aid);
			switch (r) {
			case -2:
				continue;
			case -1:
				peer_send_eor(peer, aid);
				continue;
			default:
				b = queue_buf + r;
				break;
			}
				
			/* finally send message to SE */
			if (imsg_compose(ibuf_se, IMSG_UPDATE, peer->conf.id,
			    0, -1, b, len) == -1)
				fatal("imsg_compose error");
			sent++;
		}
		max -= sent;
	} while (sent != 0 && max > 0);
}

/*
 * generic helper function
 */
u_int32_t
rde_local_as(void)
{
	return (conf->as);
}

int
rde_noevaluate(void)
{
	/* do not run while cleaning up */
	if (rde_quit)
		return (1);

	return (conf->flags & BGPD_FLAG_NO_EVALUATE);
}

int
rde_decisionflags(void)
{
	return (conf->flags & BGPD_FLAG_DECISION_MASK);
}

int
rde_as4byte(struct rde_peer *peer)
{
	return (peer->capa.as4byte);
}

/*
 * peer functions
 */
struct peer_table {
	struct rde_peer_head	*peer_hashtbl;
	u_int32_t		 peer_hashmask;
} peertable;

#define PEER_HASH(x)		\
	&peertable.peer_hashtbl[(x) & peertable.peer_hashmask]

void
peer_init(u_int32_t hashsize)
{
	struct peer_config pc;
	u_int32_t	 hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1)
		;
	peertable.peer_hashtbl = calloc(hs, sizeof(struct rde_peer_head));
	if (peertable.peer_hashtbl == NULL)
		fatal("peer_init");

	for (i = 0; i < hs; i++)
		LIST_INIT(&peertable.peer_hashtbl[i]);
	LIST_INIT(&peerlist);

	peertable.peer_hashmask = hs - 1;

	bzero(&pc, sizeof(pc));
	snprintf(pc.descr, sizeof(pc.descr), "LOCAL");

	peerself = peer_add(0, &pc);
	if (peerself == NULL)
		fatalx("peer_init add self");

	peerself->state = PEER_UP;
}

void
peer_shutdown(void)
{
	u_int32_t	i;

	for (i = 0; i <= peertable.peer_hashmask; i++)
		if (!LIST_EMPTY(&peertable.peer_hashtbl[i]))
			log_warnx("peer_free: free non-free table");

	free(peertable.peer_hashtbl);
}

struct rde_peer *
peer_get(u_int32_t id)
{
	struct rde_peer_head	*head;
	struct rde_peer		*peer;

	head = PEER_HASH(id);

	LIST_FOREACH(peer, head, hash_l) {
		if (peer->conf.id == id)
			return (peer);
	}
	return (NULL);
}

struct rde_peer *
peer_add(u_int32_t id, struct peer_config *p_conf)
{
	struct rde_peer_head	*head;
	struct rde_peer		*peer;

	if (peer_get(id))
		return (NULL);

	peer = calloc(1, sizeof(struct rde_peer));
	if (peer == NULL)
		fatal("peer_add");

	LIST_INIT(&peer->path_h);
	memcpy(&peer->conf, p_conf, sizeof(struct peer_config));
	peer->remote_bgpid = 0;
	peer->ribid = rib_find(peer->conf.rib);
	peer->state = PEER_NONE;
	up_init(peer);

	head = PEER_HASH(id);

	LIST_INSERT_HEAD(head, peer, hash_l);
	LIST_INSERT_HEAD(&peerlist, peer, peer_l);

	return (peer);
}

void
peer_localaddrs(struct rde_peer *peer, struct bgpd_addr *laddr)
{
	struct ifaddrs	*ifap, *ifa, *match;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (match = ifap; match != NULL; match = match->ifa_next)
		if (sa_cmp(laddr, match->ifa_addr) == 0)
			break;

	if (match == NULL)
		fatalx("peer_localaddrs: local address not found");

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    strcmp(ifa->ifa_name, match->ifa_name) == 0) {
			if (ifa->ifa_addr->sa_family ==
			    match->ifa_addr->sa_family)
				ifa = match;
			sa2addr(ifa->ifa_addr, &peer->local_v4_addr);
			break;
		}
	}
	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    strcmp(ifa->ifa_name, match->ifa_name) == 0) {
			/*
			 * only accept global scope addresses except explicitly
			 * specified.
			 */
			if (ifa->ifa_addr->sa_family ==
			    match->ifa_addr->sa_family)
				ifa = match;
			else if (IN6_IS_ADDR_LINKLOCAL(
			    &((struct sockaddr_in6 *)ifa->
			    ifa_addr)->sin6_addr) ||
			    IN6_IS_ADDR_SITELOCAL(
			    &((struct sockaddr_in6 *)ifa->
			    ifa_addr)->sin6_addr))
				continue;
			sa2addr(ifa->ifa_addr, &peer->local_v6_addr);
			break;
		}
	}

	freeifaddrs(ifap);
}

void
peer_up(u_int32_t id, struct session_up *sup)
{
	struct rde_peer	*peer;
	u_int8_t	 i;

	peer = peer_get(id);
	if (peer == NULL) {
		log_warnx("peer_up: peer id %d already exists", id);
		return;
	}

	if (peer->state != PEER_DOWN && peer->state != PEER_NONE)
		fatalx("peer_up: bad state");
	peer->remote_bgpid = ntohl(sup->remote_bgpid);
	peer->short_as = sup->short_as;
	memcpy(&peer->remote_addr, &sup->remote_addr,
	    sizeof(peer->remote_addr));
	memcpy(&peer->capa, &sup->capa, sizeof(peer->capa));

	peer_localaddrs(peer, &sup->local_addr);

	peer->state = PEER_UP;
	up_init(peer);

	if (rde_noevaluate())
		/*
		 * no need to dump the table to the peer, there are no active
		 * prefixes anyway. This is a speed up hack.
		 */
		return;

	for (i = 0; i < AID_MAX; i++) {
		if (peer->capa.mp[i])
			peer_dump(id, i);
	}
}

void
peer_down(u_int32_t id)
{
	struct rde_peer		*peer;
	struct rde_aspath	*asp, *nasp;

	peer = peer_get(id);
	if (peer == NULL) {
		log_warnx("peer_down: unknown peer id %d", id);
		return;
	}
	peer->remote_bgpid = 0;
	peer->state = PEER_DOWN;
	up_down(peer);

	/* walk through per peer RIB list and remove all prefixes. */
	for (asp = LIST_FIRST(&peer->path_h); asp != NULL; asp = nasp) {
		nasp = LIST_NEXT(asp, peer_l);
		path_remove(asp);
	}
	LIST_INIT(&peer->path_h);
	peer->prefix_cnt = 0;

	/* Deletions are performed in path_remove() */
	rde_send_pftable_commit();

	LIST_REMOVE(peer, hash_l);
	LIST_REMOVE(peer, peer_l);
	free(peer);
}

void
peer_dump(u_int32_t id, u_int8_t aid)
{
	struct rde_peer		*peer;

	peer = peer_get(id);
	if (peer == NULL) {
		log_warnx("peer_dump: unknown peer id %d", id);
		return;
	}

	if (peer->conf.announce_type == ANNOUNCE_DEFAULT_ROUTE)
		up_generate_default(rules_l, peer, aid);
	else
		rib_dump(&ribs[peer->ribid], rde_up_dump_upcall, peer, aid);
	if (peer->capa.restart)
		up_generate_marker(peer, aid);
}

/* End-of-RIB marker, RFC 4724 */
void
peer_send_eor(struct rde_peer *peer, u_int8_t aid)
{
	u_int16_t	afi;
	u_int8_t	safi;

	if (aid == AID_INET) {
		u_char null[4];

		bzero(&null, 4);
		if (imsg_compose(ibuf_se, IMSG_UPDATE, peer->conf.id,
		    0, -1, &null, 4) == -1)
			fatal("imsg_compose error in peer_send_eor");
	} else {
		u_int16_t	i;
		u_char		buf[10];

		if (aid2afi(aid, &afi, &safi) == -1)
			fatalx("peer_send_eor: bad AID");

		i = 0;	/* v4 withdrawn len */
		bcopy(&i, &buf[0], sizeof(i));
		i = htons(6);	/* path attr len */
		bcopy(&i, &buf[2], sizeof(i));
		buf[4] = ATTR_OPTIONAL;
		buf[5] = ATTR_MP_UNREACH_NLRI;
		buf[6] = 3;	/* withdrawn len */
		i = htons(afi);
		bcopy(&i, &buf[7], sizeof(i));
		buf[9] = safi;

		if (imsg_compose(ibuf_se, IMSG_UPDATE, peer->conf.id,
		    0, -1, &buf, 10) == -1)
			fatal("imsg_compose error in peer_send_eor");
	}
}

/*
 * network announcement stuff
 */
void
network_add(struct network_config *nc, int flagstatic)
{
	struct rdomain		*rd;
	struct rde_aspath	*asp;
	struct filter_set_head	*vpnset = NULL;
	in_addr_t		 prefix4;
	u_int16_t		 i;

	if (nc->rtableid) {
		SIMPLEQ_FOREACH(rd, rdomains_l, entry) {
			if (rd->rtableid != nc->rtableid)
				continue;
			switch (nc->prefix.aid) {
			case AID_INET:
				prefix4 = nc->prefix.v4.s_addr;
				bzero(&nc->prefix, sizeof(nc->prefix));
				nc->prefix.aid = AID_VPN_IPv4;
				nc->prefix.vpn4.rd = rd->rd;
				nc->prefix.vpn4.addr.s_addr = prefix4;
				nc->prefix.vpn4.labellen = 3;
				nc->prefix.vpn4.labelstack[0] =
				    (rd->label >> 12) & 0xff;
				nc->prefix.vpn4.labelstack[1] =
				    (rd->label >> 4) & 0xff;
				nc->prefix.vpn4.labelstack[2] =
				    (rd->label << 4) & 0xf0;
				nc->prefix.vpn4.labelstack[2] |= BGP_MPLS_BOS;
				vpnset = &rd->export;
				break;
			default:
				log_warnx("unable to VPNize prefix");
				filterset_free(&nc->attrset);
				return;
			}
		}
	}

	asp = path_get();
	asp->aspath = aspath_get(NULL, 0);
	asp->origin = ORIGIN_IGP;
	asp->flags = F_ATTR_ORIGIN | F_ATTR_ASPATH |
	    F_ATTR_LOCALPREF | F_PREFIX_ANNOUNCED;
	/* the nexthop is unset unless a default set overrides it */
	if (!flagstatic)
		asp->flags |= F_ANN_DYNAMIC;

	rde_apply_set(asp, &nc->attrset, nc->prefix.aid, peerself, peerself);
	if (vpnset)
		rde_apply_set(asp, vpnset, nc->prefix.aid, peerself, peerself);
	for (i = 1; i < rib_size; i++)
		path_update(&ribs[i], peerself, asp, &nc->prefix,
		    nc->prefixlen);

	path_put(asp);
	filterset_free(&nc->attrset);
}

void
network_delete(struct network_config *nc, int flagstatic)
{
	struct rdomain	*rd;
	in_addr_t	 prefix4;
	u_int32_t	 flags = F_PREFIX_ANNOUNCED;
	u_int32_t	 i;

	if (!flagstatic)
		flags |= F_ANN_DYNAMIC;

	if (nc->rtableid) {
		SIMPLEQ_FOREACH(rd, rdomains_l, entry) {
			if (rd->rtableid != nc->rtableid)
				continue;
			switch (nc->prefix.aid) {
			case AID_INET:
				prefix4 = nc->prefix.v4.s_addr;
				bzero(&nc->prefix, sizeof(nc->prefix));
				nc->prefix.aid = AID_VPN_IPv4;
				nc->prefix.vpn4.rd = rd->rd;
				nc->prefix.vpn4.addr.s_addr = prefix4;
				nc->prefix.vpn4.labellen = 3;
				nc->prefix.vpn4.labelstack[0] =
				    (rd->label >> 12) & 0xff;
				nc->prefix.vpn4.labelstack[1] =
				    (rd->label >> 4) & 0xff;
				nc->prefix.vpn4.labelstack[2] =
				    (rd->label << 4) & 0xf0;
				nc->prefix.vpn4.labelstack[2] |= BGP_MPLS_BOS;
				break;
			default:
				log_warnx("unable to VPNize prefix");
				return;
			}
		}
	}

	for (i = rib_size - 1; i > 0; i--)
		prefix_remove(&ribs[i], peerself, &nc->prefix, nc->prefixlen,
		    flags);
}

void
network_dump_upcall(struct rib_entry *re, void *ptr)
{
	struct prefix		*p;
	struct kroute_full	 k;
	struct bgpd_addr	 addr;
	struct rde_dump_ctx	*ctx = ptr;

	LIST_FOREACH(p, &re->prefix_h, rib_l) {
		if (!(p->aspath->flags & F_PREFIX_ANNOUNCED))
			continue;
		pt_getaddr(p->prefix, &addr);

		bzero(&k, sizeof(k));
		memcpy(&k.prefix, &addr, sizeof(k.prefix));
		if (p->aspath->nexthop == NULL ||
		    p->aspath->nexthop->state != NEXTHOP_REACH)
			k.nexthop.aid = k.prefix.aid;
		else
			memcpy(&k.nexthop, &p->aspath->nexthop->true_nexthop,
			    sizeof(k.nexthop));
		k.prefixlen = p->prefix->prefixlen;
		k.flags = F_KERNEL;
		if ((p->aspath->flags & F_ANN_DYNAMIC) == 0)
			k.flags = F_STATIC;
		if (imsg_compose(ibuf_se_ctl, IMSG_CTL_SHOW_NETWORK, 0,
		    ctx->req.pid, -1, &k, sizeof(k)) == -1)
			log_warnx("network_dump_upcall: "
			    "imsg_compose error");
	}
}

/* clean up */
void
rde_shutdown(void)
{
	struct rde_peer		*p;
	struct filter_rule	*r;
	u_int32_t		 i;

	/*
	 * the decision process is turned off if rde_quit = 1 and
	 * rde_shutdown depends on this.
	 */

	/*
	 * All peers go down
	 */
	for (i = 0; i <= peertable.peer_hashmask; i++)
		while ((p = LIST_FIRST(&peertable.peer_hashtbl[i])) != NULL)
			peer_down(p->conf.id);

	/* free filters */
	while ((r = TAILQ_FIRST(rules_l)) != NULL) {
		TAILQ_REMOVE(rules_l, r, entry);
		filterset_free(&r->set);
		free(r);
	}
	free(rules_l);

	nexthop_shutdown();
	path_shutdown();
	aspath_shutdown();
	attr_shutdown();
	pt_shutdown();
	peer_shutdown();
}

int
sa_cmp(struct bgpd_addr *a, struct sockaddr *b)
{
	struct sockaddr_in	*in_b;
	struct sockaddr_in6	*in6_b;

	if (aid2af(a->aid) != b->sa_family)
		return (1);

	switch (b->sa_family) {
	case AF_INET:
		in_b = (struct sockaddr_in *)b;
		if (a->v4.s_addr != in_b->sin_addr.s_addr)
			return (1);
		break;
	case AF_INET6:
		in6_b = (struct sockaddr_in6 *)b;
#ifdef __KAME__
		/* directly stolen from sbin/ifconfig/ifconfig.c */
		if (IN6_IS_ADDR_LINKLOCAL(&in6_b->sin6_addr)) {
			in6_b->sin6_scope_id =
			    ntohs(*(u_int16_t *)&in6_b->sin6_addr.s6_addr[2]);
			in6_b->sin6_addr.s6_addr[2] =
			    in6_b->sin6_addr.s6_addr[3] = 0;
		}
#endif
		if (bcmp(&a->v6, &in6_b->sin6_addr,
		    sizeof(struct in6_addr)))
			return (1);
		break;
	default:
		fatal("king bula sez: unknown address family");
		/* NOTREACHED */
	}

	return (0);
}
