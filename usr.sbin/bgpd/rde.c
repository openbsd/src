/*	$OpenBSD: rde.c,v 1.114 2004/05/21 12:10:22 claudio Exp $ */

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

#include <errno.h>
#include <limits.h>
#include <pwd.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "ensure.h"
#include "mrt.h"
#include "rde.h"
#include "session.h"

#define	PFD_PIPE_MAIN		0
#define PFD_PIPE_SESSION	1

void		 rde_sighdlr(int);
void		 rde_dispatch_imsg_session(struct imsgbuf *);
void		 rde_dispatch_imsg_parent(struct imsgbuf *);
int		 rde_update_dispatch(struct imsg *);
int		 rde_update_get_prefix(u_char *, u_int16_t, struct bgpd_addr *,
		     u_int8_t *);
void		 rde_update_err(struct rde_peer *, u_int8_t , u_int8_t,
		     void *, u_int16_t);
void		 rde_dump_rib_as(struct prefix *, pid_t);
void		 rde_dump_rib_prefix(struct prefix *, pid_t);
void		 rde_dump_upcall(struct pt_entry *, void *);
void		 rde_dump_as(struct as_filter *, pid_t);
void		 rde_dump_prefix_upcall(struct pt_entry *, void *);
void		 rde_dump_prefix(struct ctl_show_rib_prefix *, pid_t);
void		 rde_update_log(const char *,
		     const struct rde_peer *, const struct attr_flags *,
		     const struct bgpd_addr *, u_int8_t);
void		 rde_update_queue_runner(void);

void		 peer_init(u_int32_t);
void		 peer_shutdown(void);
struct rde_peer	*peer_add(u_int32_t, struct peer_config *);
void		 peer_remove(struct rde_peer *);
struct rde_peer	*peer_get(u_int32_t);
void		 peer_up(u_int32_t, struct session_up *);
void		 peer_down(u_int32_t);
void		 peer_dump(u_int32_t, u_int16_t, u_int8_t);

void		 network_init(struct network_head *);
void		 network_add(struct network_config *, int);
void		 network_delete(struct network_config *, int);
void		 network_dump_upcall(struct pt_entry *, void *);
void		 network_flush(int);

void		 rde_shutdown(void);

volatile sig_atomic_t	 rde_quit = 0;
struct bgpd_config	*conf, *nconf;
time_t			 reloadtime;
struct rde_peer_head	 peerlist;
struct rde_peer		 peerself;
struct rde_peer		 peerdynamic;
struct filter_head	*rules_l, *newrules;
struct imsgbuf		 ibuf_se;
struct imsgbuf		 ibuf_main;

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
u_int32_t	nexthophashsize = 64;

int
rde_main(struct bgpd_config *config, struct network_head *net_l,
    struct filter_head *rules, struct mrt_head *mrt_l,
    int pipe_m2r[2], int pipe_s2r[2])
{
	pid_t		 pid;
	struct passwd	*pw;
	struct mrt	*m;
	struct pollfd	 pfd[2];
	int		 nfds;

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork");
	case 0:
		break;
	default:
		return (pid);
	}

	conf = config;

	if ((pw = getpwnam(BGPD_USER)) == NULL)
		fatal("getpwnam");

	if (chroot(pw->pw_dir) == -1)
		fatal("chroot");
	if (chdir("/") == -1)
		fatal("chdir(\"/\")");

	setproctitle("route decision engine");
	bgpd_process = PROC_RDE;

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid)) {
		fatal("can't drop privileges");
	}

	endpwent();

	signal(SIGTERM, rde_sighdlr);
	signal(SIGINT, rde_sighdlr);

	close(pipe_s2r[0]);
	close(pipe_m2r[0]);

	/* initialize the RIB structures */
	imsg_init(&ibuf_se, pipe_s2r[1]);
	imsg_init(&ibuf_main, pipe_m2r[1]);

	/* main mrt list is not used in the SE */
	while ((m = LIST_FIRST(mrt_l)) != NULL) {
		LIST_REMOVE(m, list);
		free(m);
	}

	pt_init();
	path_init(pathhashsize);
	nexthop_init(nexthophashsize);
	peer_init(peerhashsize);
	rules_l = rules;
	network_init(net_l);

	log_info("route decision engine ready");

	while (rde_quit == 0) {
		bzero(&pfd, sizeof(pfd));
		pfd[PFD_PIPE_MAIN].fd = ibuf_main.fd;
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		if (ibuf_main.w.queued > 0)
			pfd[PFD_PIPE_MAIN].events |= POLLOUT;

		pfd[PFD_PIPE_SESSION].fd = ibuf_se.fd;
		pfd[PFD_PIPE_SESSION].events = POLLIN;
		if (ibuf_se.w.queued > 0)
			pfd[PFD_PIPE_SESSION].events |= POLLOUT;

		if ((nfds = poll(pfd, 2, INFTIM)) == -1)
			if (errno != EINTR)
				fatal("poll error");

		if (nfds > 0 && (pfd[PFD_PIPE_MAIN].revents & POLLOUT) &&
		    ibuf_main.w.queued)
			if (msgbuf_write(&ibuf_main.w) < 0)
				fatal("pipe write error");

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & POLLIN) {
			nfds--;
			rde_dispatch_imsg_parent(&ibuf_main);
		}

		if (nfds > 0 && (pfd[PFD_PIPE_SESSION].revents & POLLOUT) &&
		    ibuf_se.w.queued)
			if (msgbuf_write(&ibuf_se.w) < 0)
				fatal("pipe write error");

		if (nfds > 0 && pfd[PFD_PIPE_SESSION].revents & POLLIN) {
			nfds--;
			rde_dispatch_imsg_session(&ibuf_se);
		}
		rde_update_queue_runner();
	}

	rde_shutdown();

	msgbuf_write(&ibuf_se.w);
	msgbuf_clear(&ibuf_se.w);
	msgbuf_write(&ibuf_main.w);
	msgbuf_clear(&ibuf_main.w);

	log_info("route decision engine exiting");
	_exit(0);
}

void
rde_dispatch_imsg_session(struct imsgbuf *ibuf)
{
	struct imsg		 imsg;
	struct session_up	 sup;
	struct rrefresh		 r;
	pid_t			 pid;
	int			 n;

	if ((n = imsg_read(ibuf)) == -1)
		fatal("rde_dispatch_imsg_session: imsg_read error");
	if (n == 0)	/* connection closed */
		fatal("rde_dispatch_imsg_session: pipe closed");

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_imsg_session: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_UPDATE:
			rde_update_dispatch(&imsg);
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
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(r)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			memcpy(&r, imsg.data, sizeof(r));
			peer_dump(imsg.hdr.peerid, r.afi, r.safi);
			break;
		case IMSG_NETWORK_ADD:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct network_config)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			network_add(imsg.data, 0);
			break;
		case IMSG_NETWORK_REMOVE:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct network_config)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			network_delete(imsg.data, 0);
			break;
		case IMSG_NETWORK_FLUSH:
			if (imsg.hdr.len != IMSG_HEADER_SIZE) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			network_flush(0);
			break;
		case IMSG_CTL_SHOW_NETWORK:
			if (imsg.hdr.len != IMSG_HEADER_SIZE) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			pid = imsg.hdr.pid;
			pt_dump(network_dump_upcall, &pid);
			imsg_compose_pid(&ibuf_se, IMSG_CTL_END, pid, NULL, 0);
			break;
		case IMSG_CTL_SHOW_RIB:
			if (imsg.hdr.len != IMSG_HEADER_SIZE) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			pid = imsg.hdr.pid;
			pt_dump(rde_dump_upcall, &pid);
			imsg_compose_pid(&ibuf_se, IMSG_CTL_END, pid, NULL, 0);
			break;
		case IMSG_CTL_SHOW_RIB_AS:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct as_filter)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			pid = imsg.hdr.pid;
			rde_dump_as(imsg.data, pid);
			imsg_compose_pid(&ibuf_se, IMSG_CTL_END, pid, NULL, 0);
			break;
		case IMSG_CTL_SHOW_RIB_PREFIX:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct ctl_show_rib_prefix)) {
				log_warnx("rde_dispatch: wrong imsg len");
				break;
			}
			pid = imsg.hdr.pid;
			rde_dump_prefix(imsg.data, pid);
			imsg_compose_pid(&ibuf_se, IMSG_CTL_END, pid, NULL, 0);
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
	struct imsg		 imsg;
	struct mrt_config	 mrt;
	struct filter_rule	*r;
	int			 n;

	if ((n = imsg_read(ibuf)) == -1)
		fatal("rde_dispatch_imsg_parent: imsg_read error");
	if (n == 0)	/* connection closed */
		fatal("rde_dispatch_imsg_parent: pipe closed");

	for (;;) {
		if ((n = imsg_get(ibuf, &imsg)) == -1)
			fatal("rde_dispatch_imsg_parent: imsg_read error");
		if (n == 0)
			break;

		switch (imsg.hdr.type) {
		case IMSG_RECONF_CONF:
			reloadtime = time(NULL);
			newrules = calloc(1, sizeof(struct filter_head));
			if (newrules == NULL)
				fatal(NULL);
			TAILQ_INIT(newrules);
			if ((nconf = malloc(sizeof(struct bgpd_config))) ==
			    NULL)
				fatal(NULL);
			memcpy(nconf, imsg.data, sizeof(struct bgpd_config));
			break;
		case IMSG_NETWORK_ADD:
			network_add(imsg.data, 1);
			break;
		case IMSG_RECONF_FILTER:
			if (imsg.hdr.len - IMSG_HEADER_SIZE !=
			    sizeof(struct filter_rule))
				fatalx("IMSG_RECONF_FILTER bad len");
			if ((r = malloc(sizeof(struct filter_rule))) == NULL)
				fatal(NULL);
			memcpy(r, imsg.data, sizeof(struct filter_rule));
			TAILQ_INSERT_TAIL(newrules, r, entries);
			break;
		case IMSG_RECONF_DONE:
			if (nconf == NULL)
				fatalx("got IMSG_RECONF_DONE but no config");
			if ((nconf->flags & BGPD_FLAG_NO_EVALUATE)
			    != (conf->flags & BGPD_FLAG_NO_EVALUATE)) {
				log_warnx( "change to/from route-collector "
				    "mode ignored");
				if (conf->flags & BGPD_FLAG_NO_EVALUATE)
					nconf->flags |= BGPD_FLAG_NO_EVALUATE;
				else
					nconf->flags &= ~BGPD_FLAG_NO_EVALUATE;
			}
			memcpy(conf, nconf, sizeof(struct bgpd_config));
			free(nconf);
			nconf = NULL;
			prefix_network_clean(&peerself, reloadtime);
			while ((r = TAILQ_FIRST(rules_l)) != NULL) {
				TAILQ_REMOVE(rules_l, r, entries);
				free(r);
			}
			free(rules_l);
			rules_l = newrules;
			log_info("RDE reconfigured");
			break;
		case IMSG_NEXTHOP_UPDATE:
			nexthop_update(imsg.data);
			break;
		case IMSG_MRT_REQ:
			memcpy(&mrt, imsg.data, sizeof(mrt));
			mrt.ibuf = &ibuf_main;
			if (mrt.type == MRT_TABLE_DUMP) {
				mrt_clear_seq();
				pt_dump(mrt_dump_upcall, &mrt);
				if (imsg_compose(&ibuf_main, IMSG_MRT_END,
				    mrt.id, NULL, 0) == -1)
					fatalx("imsg_compose error");
			}
			break;
		case IMSG_MRT_END:
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
	struct rde_peer		*peer;
	u_char			*p, *emsg;
	int			 pos;
	u_int16_t		 len;
	u_int16_t		 withdrawn_len;
	u_int16_t		 attrpath_len;
	u_int16_t		 nlri_len, size;
	u_int8_t		 prefixlen, subtype;
	struct bgpd_addr	 prefix;
	struct attr_flags	 attrs, fattrs;

	peer = peer_get(imsg->hdr.peerid);
	if (peer == NULL)	/* unknown peer, cannot happen */
		return (-1);
	if (peer->state != PEER_UP)
		return (-1);	/* peer is not yet up, cannot happen */

	p = imsg->data;

	memcpy(&len, p, 2);
	withdrawn_len = len = ntohs(len);
	p += 2;
	if (imsg->hdr.len < IMSG_HEADER_SIZE + 2 + withdrawn_len + 2) {
		rde_update_err(peer, ERR_UPDATE, ERR_UPD_ATTRLIST, NULL, 0);
		return (-1);
	}

	/* withdraw prefix */
	while (len > 0) {
		if ((pos = rde_update_get_prefix(p, len, &prefix,
		    &prefixlen)) == -1) {
			/*
			 * the rfc does not mention what we should do in
			 * this case. Let's do the same as in the NLRI case.
			 */
			log_peer_warnx(&peer->conf, "bad withdraw prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    NULL, 0);
			return (-1);
		}
		if (prefixlen > 32) {
			log_peer_warnx(&peer->conf, "bad withdraw prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    NULL, 0);
			return (-1);
		}

		p += pos;
		len -= pos;

		/* input filter */
		if (rde_filter(peer, NULL, &prefix, prefixlen,
		    DIR_IN) == ACTION_DENY)
			continue;

		rde_update_log("withdraw", peer, NULL, &prefix, prefixlen);
		prefix_remove(peer, &prefix, prefixlen);
	}

	memcpy(&len, p, 2);
	attrpath_len = ntohs(len);
	p += 2;
	if (imsg->hdr.len <
	    IMSG_HEADER_SIZE + 2 + withdrawn_len + 2 + attrpath_len) {
		rde_update_err(peer, ERR_UPDATE, ERR_UPD_ATTRLIST, NULL, 0);
		return (-1);
	}
	nlri_len =
	    imsg->hdr.len - IMSG_HEADER_SIZE - 4 - withdrawn_len - attrpath_len;
	if (attrpath_len == 0) /* 0 = no NLRI information in this message */
		return (0);

	/* parse path attributes */
	attr_init(&attrs);
	while (attrpath_len > 0) {
		if ((pos = attr_parse(p, attrpath_len, &attrs, peer->conf.ebgp,
		    peer->conf.enforce_as, peer->conf.remote_as)) < 0) {
			emsg = attr_error(p, attrpath_len, &attrs,
			    &subtype, &size);
			rde_update_err(peer, ERR_UPDATE, subtype, emsg, size);
			attr_free(&attrs);
			return (-1);
		}
		p += pos;
		attrpath_len -= pos;
	}

	/* check for missing but necessary attributes */
	if ((subtype = attr_missing(&attrs, peer->conf.ebgp)) != 0) {
		rde_update_err(peer, ERR_UPDATE, ERR_UPD_MISSNG_WK_ATTR,
		    &subtype, sizeof(u_int8_t));
		attr_free(&attrs);
		return (-1);
	}

	/* aspath needs to be loop free nota bene this is not a hard error */
	if (peer->conf.ebgp && !aspath_loopfree(attrs.aspath, conf->as)) {
		char *s;
		aspath_asprint(&s, attrs.aspath->data, attrs.aspath->hdr.len);
		log_peer_warnx(&peer->conf, "AS path loop: %s", s);
		free(s);
		aspath_destroy(attrs.aspath);
		attr_optfree(&attrs);
		return (0);
	}

	/* apply default overrides */
	rde_apply_set(&attrs, &peer->conf.attrset);

	/* parse nlri prefix */
	while (nlri_len > 0) {
		if ((pos = rde_update_get_prefix(p, nlri_len, &prefix,
		    &prefixlen)) == -1) {
			log_peer_warnx(&peer->conf, "bad nlri prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    NULL, 0);
			attr_free(&attrs);
			return (-1);
		}
		if (prefixlen > 32) {
			log_peer_warnx(&peer->conf, "bad nlri prefix");
			rde_update_err(peer, ERR_UPDATE, ERR_UPD_NETWORK,
			    NULL, 0);
			attr_free(&attrs);
			return (-1);
		}

		p += pos;
		nlri_len -= pos;

		/*
		 * We need to copy attrs befor calling the filter because
		 * the filter may change the attributes.
		 */
		attr_copy(&fattrs, &attrs);
		/* input filter */
		if (rde_filter(peer, &fattrs, &prefix, prefixlen,
		    DIR_IN) == ACTION_DENY) {
			attr_free(&fattrs);
			continue;
		}

		/* max prefix checker */
		if (peer->conf.max_prefix &&
		    peer->prefix_cnt >= peer->conf.max_prefix) {
			log_peer_warnx(&peer->conf, "prefix limit reached");
			rde_update_err(peer, ERR_CEASE, ERR_CEASE_MAX_PREFIX,
			    NULL, 0);
			attr_free(&attrs);
			attr_free(&fattrs);
			return (-1);
		}

		rde_update_log("update", peer, &fattrs, &prefix, prefixlen);
		path_update(peer, &fattrs, &prefix, prefixlen);
	}

	/* need to free allocated attribute memory that is no longer used */
	attr_free(&attrs);

	return (0);
}

int
rde_update_get_prefix(u_char *p, u_int16_t len, struct bgpd_addr *prefix,
    u_int8_t *prefixlen)
{
	int		i;
	u_int8_t	pfxlen;
	u_int16_t	plen;
	union {
		struct in_addr	a32;
		u_int8_t	a8[4];
	}		addr;

	if (len < 1)
		return (-1);

	memcpy(&pfxlen, p, 1);
	p += 1;
	plen = 1;

	addr.a32.s_addr = 0;
	for (i = 0; i <= 3; i++) {
		if (pfxlen > i * 8) {
			if (len - plen < 1)
				return (-1);
			memcpy(&addr.a8[i], p++, 1);
			plen++;
		}
	}
	prefix->af = AF_INET;
	prefix->v4.s_addr = addr.a32.s_addr;
	*prefixlen = pfxlen;

	return (plen);
}

void
rde_update_err(struct rde_peer *peer, u_int8_t error, u_int8_t suberr,
    void *data, u_int16_t size)
{
	struct buf	*wbuf;

	if ((wbuf = imsg_create(&ibuf_se, IMSG_UPDATE_ERR, peer->conf.id,
	    size + sizeof(error) + sizeof(suberr))) == NULL)
		fatal("imsg_create error");
	if (imsg_add(wbuf, &error, sizeof(error)) == -1 ||
	    imsg_add(wbuf, &suberr, sizeof(suberr)) == -1 ||
	    imsg_add(wbuf, data, size) == -1)
		fatal("imsg_add error");
	if (imsg_close(&ibuf_se, wbuf) == -1)
		fatal("imsg_close error");
	peer->state = PEER_ERR;
}

void
rde_update_log(const char *message,
    const struct rde_peer *peer, const struct attr_flags *attr,
    const struct bgpd_addr *prefix, u_int8_t prefixlen)
{
	char		*nexthop = NULL;

	if (! (conf->log & BGPD_LOG_UPDATES))
		return;

	if (attr != NULL)
		asprintf(&nexthop, " via %s", inet_ntoa(attr->nexthop));

	log_debug("neighbor %s (AS%u) %s %s/%u %s",
	    log_addr(&peer->conf.remote_addr), peer->conf.remote_as, message,
	    inet_ntoa(prefix->v4), prefixlen,
	    nexthop ? nexthop : "");

	free(nexthop);
}

/*
 * control specific functions
 */
void
rde_dump_rib_as(struct prefix *p, pid_t pid)
{
	struct ctl_show_rib	 rib;
	struct buf		*wbuf;

	rib.lastchange = p->lastchange;
	rib.local_pref = p->aspath->flags.lpref;
	rib.med = p->aspath->flags.med;
	rib.prefix_cnt = p->aspath->prefix_cnt;
	rib.active_cnt = p->aspath->active_cnt;
	memcpy(&rib.nexthop, &p->aspath->nexthop->true_nexthop,
	    sizeof(rib.nexthop));
	memcpy(&rib.prefix, &p->prefix->prefix, sizeof(rib.prefix));
	rib.prefixlen = p->prefix->prefixlen;
	rib.origin = p->aspath->flags.origin;
	rib.flags = 0;
	if (p->aspath->nexthop->state == NEXTHOP_REACH)
		rib.flags |= F_RIB_ELIGIBLE;
	if (p->prefix->active == p)
		rib.flags |= F_RIB_ACTIVE;
	if (p->aspath->peer->conf.ebgp == 0)
		rib.flags |= F_RIB_INTERNAL;
	if (p->aspath->nexthop->flags & NEXTHOP_ANNOUNCE)
		rib.flags |= F_RIB_ANNOUNCE;
	rib.aspath_len = aspath_length(p->aspath->flags.aspath);

	if ((wbuf = imsg_create_pid(&ibuf_se, IMSG_CTL_SHOW_RIB, pid,
	    sizeof(rib) + rib.aspath_len)) == NULL)
		return;
	if (imsg_add(wbuf, &rib, sizeof(rib)) == -1 ||
	    imsg_add(wbuf, aspath_dump(p->aspath->flags.aspath),
	    rib.aspath_len) == -1)
		return;
	if (imsg_close(&ibuf_se, wbuf) == -1)
		return;
}

void
rde_dump_rib_prefix(struct prefix *p, pid_t pid)
{
	struct ctl_show_rib_prefix	 prefix;

	prefix.lastchange = p->lastchange;
	memcpy(&prefix.prefix, &p->prefix->prefix, sizeof(prefix.prefix));
	prefix.prefixlen = p->prefix->prefixlen;
	prefix.flags = 0;
	if (p->aspath->nexthop->state == NEXTHOP_REACH)
		prefix.flags |= F_RIB_ELIGIBLE;
	if (p->prefix->active == p)
		prefix.flags |= F_RIB_ACTIVE;
	if (p->aspath->peer->conf.ebgp == 0)
		prefix.flags |= F_RIB_INTERNAL;
	if (p->aspath->nexthop->flags & NEXTHOP_ANNOUNCE)
		prefix.flags |= F_RIB_ANNOUNCE;
	if (imsg_compose_pid(&ibuf_se, IMSG_CTL_SHOW_RIB_PREFIX, pid,
	    &prefix, sizeof(prefix)) == -1)
		log_warnx("rde_dump_as: imsg_compose error");
}

void
rde_dump_upcall(struct pt_entry *pt, void *ptr)
{
	struct prefix		*p;
	pid_t			 pid;

	memcpy(&pid, ptr, sizeof(pid));

	LIST_FOREACH(p, &pt->prefix_h, prefix_l)
	    rde_dump_rib_as(p, pid);
}

void
rde_dump_as(struct as_filter *a, pid_t pid)
{
	extern struct path_table	 pathtable;
	struct rde_aspath		*asp;
	struct prefix			*p;
	u_int32_t			 i;

	i = pathtable.path_hashmask;
	do {
		LIST_FOREACH(asp, &pathtable.path_hashtbl[i], path_l) {
			if (!aspath_match(asp->flags.aspath, a->type, a->as))
				continue;
			/* match found */
			ENSURE(!path_empty(asp));
			rde_dump_rib_as(LIST_FIRST(&asp->prefix_h), pid);
			for (p = LIST_NEXT(LIST_FIRST(&asp->prefix_h), path_l);
			    p != NULL; p = LIST_NEXT(p, path_l))
				rde_dump_rib_prefix(p, pid);
		}
	} while (i-- != 0);
}

void
rde_dump_prefix_upcall(struct pt_entry *pt, void *ptr)
{
	struct {
		pid_t				 pid;
		struct ctl_show_rib_prefix	*pref;
	}		*ctl = ptr;
	struct prefix	*p;
	in_addr_t	 mask;

	mask = htonl(0xffffffff << (32 - ctl->pref->prefixlen));
	if (ctl->pref->prefixlen <= pt->prefixlen &&
	    (ctl->pref->prefix.v4.s_addr & mask) ==
	    (pt->prefix.v4.s_addr & mask))
		LIST_FOREACH(p, &pt->prefix_h, prefix_l)
			rde_dump_rib_as(p, ctl->pid);
}

void
rde_dump_prefix(struct ctl_show_rib_prefix *pref, pid_t pid)
{
	struct pt_entry	*pt;
	struct {
		pid_t				 pid;
		struct ctl_show_rib_prefix	*pref;
	} ctl;

	if (pref->prefixlen == 32) {
		if ((pt = pt_lookup(&pref->prefix)) != NULL)
			rde_dump_upcall(pt, &pid);
	} else if (pref->flags & F_LONGER) {
		ctl.pid = pid;
		ctl.pref = pref;
		pt_dump(rde_dump_prefix_upcall, &ctl);
	} else {
		if ((pt = pt_get(&pref->prefix, pref->prefixlen)) != NULL)
			rde_dump_upcall(pt, &pid);
	}
}

/*
 * kroute specific functions
 */
void
rde_send_kroute(struct prefix *new, struct prefix *old)
{
	struct kroute	 kr;
	struct prefix	*p;
	enum imsg_type	 type;

	ENSURE(old == NULL || old->aspath->nexthop != NULL);
	ENSURE(new == NULL || new->aspath->nexthop != NULL);
	/*
	 * If old is != NULL we know it was active and should be removed.
	 * On the other hand new may be UNREACH and then we should not
	 * generate an update.
	 */
	if ((old == NULL || old->aspath->nexthop->flags & NEXTHOP_ANNOUNCE) &&
	    (new == NULL || new->aspath->nexthop->state != NEXTHOP_REACH ||
	    new->aspath->nexthop->flags & NEXTHOP_ANNOUNCE))
		return;

	if (new == NULL || new->aspath->nexthop == NULL ||
	    new->aspath->nexthop->state != NEXTHOP_REACH ||
	    new->aspath->nexthop->flags & NEXTHOP_ANNOUNCE) {
		type = IMSG_KROUTE_DELETE;
		p = old;
		kr.nexthop.s_addr = 0;
	} else {
		type = IMSG_KROUTE_CHANGE;
		p = new;
		kr.nexthop.s_addr = p->aspath->nexthop->true_nexthop.v4.s_addr;
	}

	kr.prefix.s_addr = p->prefix->prefix.v4.s_addr;
	kr.prefixlen = p->prefix->prefixlen;

	if (imsg_compose(&ibuf_main, type, 0, &kr, sizeof(kr)) == -1)
		fatal("imsg_compose error");
}

/*
 * pf table specific functions
 */
void
rde_send_pftable(const char *table, struct bgpd_addr *addr,
    u_int8_t len, int del)
{
	struct pftable_msg pfm;

	if (*table == '\0')
		return;

	bzero(&pfm, sizeof(pfm));
	strlcpy(pfm.pftable, table, sizeof(pfm.pftable));
	memcpy(&pfm.addr, addr, sizeof(pfm.addr));
	pfm.len = len;

	if (imsg_compose(&ibuf_main,
	    del ? IMSG_PFTABLE_REMOVE : IMSG_PFTABLE_ADD,
	    0, &pfm, sizeof(pfm)) == -1)
		fatal("imsg_compose error");
}

void
rde_send_pftable_commit(void)
{
	if (imsg_compose(&ibuf_main, IMSG_PFTABLE_COMMIT, 0, NULL, 0) == -1)
		fatal("imsg_compose error");
}

/*
 * nexthop specific functions
 */
void
rde_send_nexthop(struct bgpd_addr *next, int valid)
{
	int			type;

	if (valid)
		type = IMSG_NEXTHOP_ADD;
	else
		type = IMSG_NEXTHOP_REMOVE;

	if (imsg_compose(&ibuf_main, type, 0, next,
	    sizeof(struct bgpd_addr)) == -1)
		fatal("imsg_compose error");
}

/*
 * update specific functions
 */
u_char	queue_buf[4096];

void
rde_generate_updates(struct prefix *new, struct prefix *old)
{
	struct rde_peer			*peer;

	ENSURE(old == NULL || old->aspath->nexthop != NULL);
	ENSURE(new == NULL || new->aspath->nexthop != NULL);
	/*
	 * If old is != NULL we know it was active and should be removed.
	 * On the other hand new may be UNREACH and then we should not
	 * generate an update.
	 */
	if (old == NULL && (new == NULL ||
	    new->aspath->nexthop->state != NEXTHOP_REACH))
		return;

	LIST_FOREACH(peer, &peerlist, peer_l) {
		if (peer->state != PEER_UP)
			continue;
		up_generate_updates(peer, new, old);
	}
}

void
rde_update_queue_runner(void)
{
	struct rde_peer		*peer;
	int			 r, sent;
	u_int16_t		 len, wd_len, wpos;

	len = sizeof(queue_buf) - MSGSIZE_HEADER;
	do {
		sent = 0;
		LIST_FOREACH(peer, &peerlist, peer_l) {
			if (peer->state != PEER_UP)
				continue;
			/* first withdraws */
			wpos = 2; /* reserve space for the length field */
			r = up_dump_prefix(queue_buf + wpos, len - wpos - 2,
			    &peer->withdraws, peer);
			wd_len = r;
			/* write withdraws length filed */
			wd_len = htons(wd_len);
			memcpy(queue_buf, &wd_len, 2);
			wpos += r;

			/* now bgp path attributes */
			r = up_dump_attrnlri(queue_buf + wpos, len - wpos,
			    peer);
			wpos += r;

			if (wpos == 4)
				/*
				 * No packet to send. The 4 bytes are the
				 * needed withdraw and path attribute length.
				 */
				continue;

			/* finally send message to SE */
			if (imsg_compose(&ibuf_se, IMSG_UPDATE, peer->conf.id,
			    queue_buf, wpos) == -1)
				fatal("imsg_compose error");
			sent++;
		}
	} while (sent != 0);
}

/*
 * generic helper function
 */
u_int16_t
rde_local_as(void)
{
	return (conf->as);
}

int
rde_noevaluate(void)
{
	return (conf->flags & BGPD_FLAG_NO_EVALUATE);
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
	ENSURE(head != NULL);

	LIST_FOREACH(peer, head, hash_l) {
		if (peer->conf.id == id)
			return peer;
	}
	return NULL;
}

struct rde_peer *
peer_add(u_int32_t id, struct peer_config *p_conf)
{
	struct rde_peer_head	*head;
	struct rde_peer		*peer;

	ENSURE(peer_get(id) == NULL);

	peer = calloc(1, sizeof(struct rde_peer));
	if (peer == NULL)
		fatal("peer_add");

	LIST_INIT(&peer->path_h);
	memcpy(&peer->conf, p_conf, sizeof(struct peer_config));
	peer->remote_bgpid = 0;
	peer->state = PEER_NONE;
	up_init(peer);

	head = PEER_HASH(id);
	ENSURE(head != NULL);

	LIST_INSERT_HEAD(head, peer, hash_l);
	LIST_INSERT_HEAD(&peerlist, peer, peer_l);

	return (peer);
}

void
peer_remove(struct rde_peer *peer)
{
	ENSURE(peer->state == PEER_DOWN);
	ENSURE(peer_get(peer->conf.id) != NULL);
	ENSURE(LIST_EMPTY(&peer->path_h));

	LIST_REMOVE(peer, hash_l);
	LIST_REMOVE(peer, peer_l);

	free(peer);
}

void
peer_up(u_int32_t id, struct session_up *sup)
{
	struct rde_peer	*peer;

	peer = peer_add(id, &sup->conf);
	if (peer == NULL) {
		log_warnx("peer_up: unknown peer id %d", id);
		return;
	}

	ENSURE(peer->state == PEER_DOWN || peer->state == PEER_NONE);
	peer->remote_bgpid = ntohl(sup->remote_bgpid);
	memcpy(&peer->local_addr, &sup->local_addr, sizeof(peer->local_addr));
	memcpy(&peer->remote_addr, &sup->remote_addr,
	    sizeof(peer->remote_addr));
	peer->state = PEER_UP;
	up_init(peer);

	if (rde_noevaluate())
		/*
		 * no need to dump the table to the peer, there are no active
		 * prefixes anyway. This is a speed up hack.
		 */
		return;

	peer_dump(id, AFI_ALL, SAFI_ALL);
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

	/* Deletions are performed in path_remove() */
	rde_send_pftable_commit();

	peer_remove(peer);
}

void
peer_dump(u_int32_t id, u_int16_t afi, u_int8_t safi)
{
	struct rde_peer		*peer;

	peer = peer_get(id);
	if (peer == NULL) {
		log_warnx("peer_down: unknown peer id %d", id);
		return;
	}

	if (afi == AFI_ALL || afi == AFI_IPv4)
		if (safi == SAFI_ALL || safi == SAFI_UNICAST ||
		    safi == SAFI_BOTH) {
			pt_dump(up_dump_upcall, peer);
			return;
		}

	log_peer_warnx(&peer->conf, "unsupported AFI, SAFI combination");
}

/*
 * network announcement stuff
 */
void
network_init(struct network_head *net_l)
{
	struct network	*n;

	reloadtime = time(NULL);
	bzero(&peerself, sizeof(peerself));
	peerself.state = PEER_UP;
	peerself.remote_bgpid = conf->bgpid;
	peerself.conf.remote_as = conf->as;
	snprintf(peerself.conf.descr, sizeof(peerself.conf.descr),
	    "LOCAL AS %hu", conf->as);
	bzero(&peerdynamic, sizeof(peerdynamic));
	peerdynamic.state = PEER_UP;
	peerdynamic.remote_bgpid = conf->bgpid;
	peerdynamic.conf.remote_as = conf->as;
	snprintf(peerdynamic.conf.descr, sizeof(peerdynamic.conf.descr),
	    "LOCAL AS %hu", conf->as);

	while ((n = TAILQ_FIRST(net_l)) != NULL) {
		TAILQ_REMOVE(net_l, n, network_l);
		network_add(&n->net, 1);
		free(n);
	}
}

void
network_add(struct network_config *nc, int flagstatic)
{
	struct attr_flags	 attrs;

	bzero(&attrs, sizeof(attrs));

	attrs.aspath = aspath_create(NULL, 0);
	attrs.nexthop.s_addr = INADDR_ANY;
	/* med = 0 */
	/* lpref = 0 */
	attrs.origin = ORIGIN_IGP;
	TAILQ_INIT(&attrs.others);

	/* apply default overrides */
	rde_apply_set(&attrs, &nc->attrset);

	if (flagstatic)
		path_update(&peerself, &attrs, &nc->prefix, nc->prefixlen);
	else
		path_update(&peerdynamic, &attrs, &nc->prefix, nc->prefixlen);
}

void
network_delete(struct network_config *nc, int flagstatic)
{
	if (flagstatic)
		prefix_remove(&peerself, &nc->prefix, nc->prefixlen);
	else
		prefix_remove(&peerdynamic, &nc->prefix, nc->prefixlen);
}

void
network_dump_upcall(struct pt_entry *pt, void *ptr)
{
	struct prefix		*p;
	struct kroute		 k;
	pid_t			 pid;

	memcpy(&pid, ptr, sizeof(pid));

	LIST_FOREACH(p, &pt->prefix_h, prefix_l)
	    if (p->aspath->nexthop->flags & NEXTHOP_ANNOUNCE) {
		    bzero(&k, sizeof(k));
		    memcpy(&k.prefix, &p->prefix->prefix.v4.s_addr,
			sizeof(k.prefix));
		    k.prefixlen = p->prefix->prefixlen;
		    if (p->peer == &peerself)
			    k.flags = F_KERNEL;
		    if (imsg_compose_pid(&ibuf_se, IMSG_CTL_SHOW_NETWORK, pid,
			&k, sizeof(k)) == -1)
			    log_warnx("network_dump_upcall: "
				"imsg_compose error");
	    }
}

void
network_flush(int flagstatic)
{
	if (flagstatic)
		prefix_network_clean(&peerself, time(NULL));
	else
		prefix_network_clean(&peerdynamic, time(NULL));
}

/* clean up */
void
rde_shutdown(void)
{
	struct rde_peer		*p;
	struct rde_aspath	*asp, *nasp;
	struct filter_rule	*r;
	u_int32_t		 i;

	/*
	 * the decision process is turend of if rde_quit = 1 and
	 * rde_shutdown depends on this.
	 */
	ENSURE(rde_quit != 0);

	/* First mark all peer as down */
	for (i = 0; i <= peertable.peer_hashmask; i++)
		LIST_FOREACH(p, &peertable.peer_hashtbl[i], peer_l) {
			p->remote_bgpid = 0;
			p->state = PEER_DOWN;
			up_down(p);
		}
	/*
	 * Now walk through the aspath list and remove everything.
	 * path_remove will also remove the prefixes and the pt_entries.
	 */
	for (i = 0; i <= peertable.peer_hashmask; i++)
		while ((p = LIST_FIRST(&peertable.peer_hashtbl[i])) != NULL) {
			for (asp = LIST_FIRST(&p->path_h);
			    asp != NULL; asp = nasp) {
				nasp = LIST_NEXT(asp, peer_l);
				path_remove(asp);
			}
			/* finally remove peer */
			peer_remove(p);
		}

	/* free announced network prefixes */
	peerself.remote_bgpid = 0;
	peerself.state = PEER_DOWN;
	for (asp = LIST_FIRST(&peerself.path_h); asp != NULL; asp = nasp) {
		nasp = LIST_NEXT(asp, peer_l);
		path_remove(asp);
	}

	peerdynamic.remote_bgpid = 0;
	peerdynamic.state = PEER_DOWN;
	for (asp = LIST_FIRST(&peerdynamic.path_h); asp != NULL; asp = nasp) {
		nasp = LIST_NEXT(asp, peer_l);
		path_remove(asp);
	}

	/* free filters */
	while ((r = TAILQ_FIRST(rules_l)) != NULL) {
		TAILQ_REMOVE(rules_l, r, entries);
		free(r);
	}
	free(rules_l);

	nexthop_shutdown();
	path_shutdown();
	pt_shutdown();
	peer_shutdown();
}

