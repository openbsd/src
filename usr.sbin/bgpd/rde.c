/*	$OpenBSD: rde.c,v 1.3 2003/12/17 19:26:26 henning Exp $ */

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

#include <sys/types.h>

#include <errno.h>
#include <pwd.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "ensure.h"
#include "mrt.h"
#include "rde.h"

#define	PFD_PIPE_MAIN		0
#define PFD_PIPE_SESSION	1

void		 rde_sighdlr(int);
void		 rde_dispatch_imsg(int, int);
int		 rde_update_dispatch(struct imsg *);
int		 rde_update_get_prefix(u_char *, u_int16_t, struct in_addr *,
		     u_int8_t *);
void		 init_attr_flags(struct attr_flags *);
int		 rde_update_get_attr(u_char *, u_int16_t, struct attr_flags *);
void		 rde_update_err(u_int32_t, enum suberr_update);

void		 peer_init(struct bgpd_config *, u_long);
void		 peer_add(u_int32_t, struct peer_config *);
struct rde_peer	*peer_get(u_int32_t);
void		 peer_up(u_int32_t, u_int32_t);
void		 peer_down(u_int32_t);

volatile sig_atomic_t	 rde_quit = 0;
struct bgpd_config	*conf, *nconf;
int			 se_queued_writes = 0;
int			 se_sock;
int			 main_queued_writes = 0;

void
rde_sighdlr(int sig)
{
	switch (sig) {
	case SIGTERM:
		rde_quit = 1;
		break;
	}
}

u_long	peerhashsize = 64;
u_long	pathhashsize = 1024;
u_long	nexthophashsize = 64;

int
rde_main(struct bgpd_config *config, int pipe_m2r[2], int pipe_s2r[2])
{
	pid_t		 pid;
	struct passwd	*pw;
	struct pollfd	 pfd[2];
	size_t		 nfds;
	int		 n;

	conf = config;

	if ((pw = calloc(1, sizeof(struct passwd))) == NULL)
		fatal(NULL, errno);

	if ((pw = getpwnam(BGPD_USER)) == NULL)
		fatal("getpwnam", errno);

	switch (pid = fork()) {
	case -1:
		fatal("cannot fork", errno);
	case 0:
		break;
	default:
		return (pid);
	}

	if (chroot(pw->pw_dir) < 0)
		fatal("chroot failed", errno);
	chdir("/");
	setproctitle("route decision engine");

	if (setgroups(1, &pw->pw_gid) ||
	    setegid(pw->pw_gid) || setgid(pw->pw_gid) ||
	    seteuid(pw->pw_uid) || setuid(pw->pw_uid)) {
		fatal("can't drop privileges", errno);
	}

	signal(SIGTERM, rde_sighdlr);

	close(pipe_s2r[0]);
	close(pipe_m2r[0]);

	/* initialize the RIB structures */
	peer_init(config, peerhashsize);
	path_init(pathhashsize);
	nexthop_init(nexthophashsize);
	pt_init();
	init_imsg_buf();
	se_sock = pipe_s2r[1];

	logit(LOG_INFO, "route decision engine ready");

	while (rde_quit == 0) {
		bzero(&pfd, sizeof(pfd));
		pfd[PFD_PIPE_MAIN].fd = pipe_m2r[1];
		pfd[PFD_PIPE_MAIN].events = POLLIN;
		if (main_queued_writes > 0)
			pfd[PFD_PIPE_MAIN].events |= POLLOUT;

		pfd[PFD_PIPE_SESSION].fd = pipe_s2r[1];
		pfd[PFD_PIPE_SESSION].events = POLLIN;
		if (se_queued_writes > 0)
			pfd[PFD_PIPE_SESSION].events |= POLLOUT;

		if ((nfds = poll(pfd, 2, INFTIM)) == -1)
			if (errno != EINTR)
				fatal("poll error", errno);

		if (nfds > 0 && pfd[PFD_PIPE_MAIN].revents & POLLIN) {
			rde_dispatch_imsg(pfd[PFD_PIPE_MAIN].fd,
			    PFD_PIPE_MAIN);
		}
		if (nfds > 0 && pfd[PFD_PIPE_SESSION].revents & POLLIN) {
			rde_dispatch_imsg(pfd[PFD_PIPE_SESSION].fd,
			    PFD_PIPE_SESSION);
		}
		if (nfds > 0 && (pfd[PFD_PIPE_MAIN].revents & POLLOUT) &&
		    main_queued_writes) {
			nfds--;
			if ((n = buf_sock_write(pfd[PFD_PIPE_MAIN].fd)) == -1)
				fatal("pipe write error", errno);
			main_queued_writes -= n;
		}
		if (nfds > 0 && (pfd[PFD_PIPE_SESSION].revents & POLLOUT) &&
		    se_queued_writes) {
			nfds--;
			if ((n = buf_sock_write(pfd[PFD_PIPE_SESSION].fd)) ==
			    -1)
				fatal("pipe write error", errno);
			se_queued_writes -= n;
		}
	}

	logit(LOG_INFO, "route decision engine exiting");
	_exit(0);
}

void
rde_dispatch_imsg(int fd, int idx)
{
	struct imsg		 imsg;
	struct peer_config	*pconf;
	struct rde_peer		*p;
	u_int32_t		 rid;
	int			 reconf;

	if (get_imsg(fd, &imsg) > 0) {
		switch (imsg.hdr.type) {
		case IMSG_RECONF_CONF:
			if (idx != PFD_PIPE_MAIN)
				fatal("reconf request not from parent", 0);
			if ((nconf = malloc(sizeof(struct bgpd_config))) ==
			    NULL)
				fatal(NULL, errno);
			memcpy(nconf, imsg.data, sizeof(struct bgpd_config));
			nconf->peers = NULL;
			break;
		case IMSG_RECONF_PEER:
			if (idx != PFD_PIPE_MAIN)
				fatal("reconf request not from parent", 0);
			pconf = (struct peer_config *)imsg.data;
			p = peer_get(pconf->id); /* will always fail atm */
			if (p == NULL) {
				if ((p = calloc(1, sizeof(struct rde_peer))) ==
				    NULL)
					fatal("new peer", errno);
				p->state = PEER_NONE;
				reconf = RECONF_REINIT;
				/* XXXC peer_add */
			} else
				reconf = RECONF_KEEP;

			memcpy(&p->conf, pconf, sizeof(struct peer_config));
			p->conf.reconf_action = reconf;
			if (pconf->reconf_action > reconf)
				p->conf.reconf_action = pconf->reconf_action;
			break;
		case IMSG_RECONF_DONE:
			if (idx != PFD_PIPE_MAIN)
				fatal("reconf request not from parent", 0);
			if (nconf == NULL)
				fatal("got IMSG_RECONF_DONE but no config", 0);
			/* XXXC merge in as needed */
			free(nconf);
			logit(LOG_INFO, "RDE reconfigured");
			break;
		case IMSG_UPDATE:
			if (idx != PFD_PIPE_SESSION)
				fatal("update msg not from session engine", 0);
			rde_update_dispatch(&imsg);
			break;
		case IMSG_SESSION_UP:
			if (idx != PFD_PIPE_SESSION)
				fatal("session msg not from session engine", 0);
			if (imsg.hdr.len - IMSG_HEADER_SIZE != sizeof(rid))
				fatal("incorrect size of session request", 0);
			rid = *(u_int32_t *)imsg.data;
			peer_up(imsg.hdr.peerid, rid);
			break;
		case IMSG_SESSION_DOWN:
			if (idx != PFD_PIPE_SESSION)
				fatal("session msg not from session engine", 0);
			peer_down(imsg.hdr.peerid);
			break;
		case IMSG_MRT_REQ:
			if (idx != PFD_PIPE_MAIN)
				fatal("mrt request not from parent", 0);
			pt_dump(mrt_dump_upcall, fd, &main_queued_writes,
			    (void *)imsg.hdr.peerid);
			/* FALLTHROUGH */
		case IMSG_MRT_END:
			if (idx != PFD_PIPE_MAIN)
				fatal("mrt request not from parent", 0);
			/* ignore end message because a dump is atomic */
			main_queued_writes += imsg_compose(fd,
			    IMSG_MRT_END, imsg.hdr.peerid, NULL, 0);
			break;
		default:
		}
		imsg_free(&imsg);
	}
}

/*
 * rde_request_dispatch() -- handle all messages comming form the parent.
 * This are reconfiguration request and inquiries.
 * XXX most is done in rde_dispatch_imsg so probably drop this function.
 */


/*
 * rde_update_dispatch() -- handle routing updates comming from the session
 * engine.
 */

int
rde_update_dispatch(struct imsg *imsg)
{
	struct rde_peer		*peer;
	u_char			*p;
	int			 pos;
	u_int16_t		 len;
	u_int16_t		 withdrawn_len;
	u_int16_t		 attrpath_len;
	u_int16_t		 nlri_len;
	u_int8_t		 prefixlen;
	struct in_addr		 prefix;
	struct attr_flags	 attrs;

	peer = peer_get(imsg->hdr.peerid);
	if (peer == NULL)	/* unknown peer, cannot happen */
		return (-1);
	if (peer->state != PEER_UP)
		return (-1);	/* peer is not yet up, cannot happen */

	p = imsg->data;

	memcpy(&len, p, 2);
	withdrawn_len = ntohs(len);
	p += 2;
	if (imsg->hdr.len - IMSG_HEADER_SIZE < 2 + withdrawn_len + 2) {
		rde_update_err(peer->conf.id, ERR_UPD_ATTRLIST);
		return (-1);
	}

	while (withdrawn_len > 0) {
		if ((pos = rde_update_get_prefix(p, withdrawn_len, &prefix,
		    &prefixlen)) == -1) {
			rde_update_err(peer->conf.id, ERR_UPD_ATTRLIST);
			return (-1);
		}
		p += pos;
		withdrawn_len -= pos;
		prefix_remove(peer, prefix, prefixlen);
	}

	memcpy(&len, p, 2);
	attrpath_len = ntohs(len);
	p += 2;
	if (imsg->hdr.len - IMSG_HEADER_SIZE <
	    2 + withdrawn_len + 2 + attrpath_len) {
		rde_update_err(peer->conf.id, ERR_UPD_ATTRLIST);
		return (-1);
	}
	nlri_len =
	    imsg->hdr.len - IMSG_HEADER_SIZE - 4 - withdrawn_len - attrpath_len;
	if (attrpath_len == 0) /* 0 = no NLRI information in this message */
		return (0);

	init_attr_flags(&attrs);
	while (attrpath_len > 0) {
		if ((pos = rde_update_get_attr(p, attrpath_len, &attrs)) < 0) {
			rde_update_err(peer->conf.id, ERR_UPD_ATTRLIST);
			return (-1);
		}
		p += pos;
		attrpath_len -= pos;
	}

	while (nlri_len > 0) {
		if ((pos = rde_update_get_prefix(p, nlri_len, &prefix,
		    &prefixlen)) == -1) {
			rde_update_err(peer->conf.id, ERR_UPD_ATTRLIST);
			return (-1);
		}
		/* XXX only for debugging. Used as debugger hook. */
		if (prefixlen < 8 || prefixlen > 24) {
			logit(LOG_DEBUG, "XXX XXX %s/%d\t[%d] {%d}\n",
			    inet_ntoa(prefix), prefixlen, nlri_len,
			    imsg->hdr.len - IMSG_HEADER_SIZE);
		}
		p += pos;
		nlri_len -= pos;
		path_update(peer, &attrs, prefix, prefixlen);
	}

	/* need to free allocated attribute memory that is no longer used */
	aspath_destroy(attrs.aspath);

	return (0);
}

int
rde_update_get_prefix(u_char *p, u_int16_t len, struct in_addr *prefix,
    u_int8_t *prefixlen)
{
	int		i;
	u_int8_t	pfxlen;
	u_int16_t	plen;
	union {
		struct in_addr	addr32;
		u_int8_t	addr8[4];
	}		addr;

	if (len < 1)
		return (-1);

	memcpy(&pfxlen, p, 1);
	p += 1;
	plen = 1;

	addr.addr32.s_addr = 0;
	for (i = 0; i <= 3; i++) {
		if (pfxlen > i * 8) {
			if (len - plen < 1)
				return (-1);
			memcpy(&addr.addr8[i], p++, 1);
			plen++;
		}
	}
	prefix->s_addr = addr.addr32.s_addr;
	*prefixlen = pfxlen;

	return (plen);
}

#define UPD_READ(t, p, plen, n) \
	do { \
		memcpy(t, p, n); \
		p += n; \
		plen += n; \
	} while (0)

void
init_attr_flags(struct attr_flags *a)
{
	bzero(a, sizeof(struct attr_flags));
	a->origin = ORIGIN_INCOMPLETE;
}

int
rde_update_get_attr(u_char *p, u_int16_t len, struct attr_flags *a)
{
	u_int32_t	 tmp32;
	u_int16_t	 attr_len;
	u_int16_t	 plen = 0;
	u_int16_t	 tmp16;
	u_int8_t	 flags;
	u_int8_t	 type;
	u_int8_t	 tmp8;
	int		 r; /* XXX */

	if (len < 3)
		return (-1);

	UPD_READ(&flags, p, plen, 1);
	UPD_READ(&type, p, plen, 1);

	if (flags & ATTR_EXTLEN) {
		if (len - plen < 2)
			return (-1);
		UPD_READ(&attr_len, p, plen, 2);
	} else {
		UPD_READ(&tmp8, p, plen, 1);
		attr_len = tmp8;
	}

	if (len - plen < attr_len)
		return (-1);

	switch (type) {
	case ATTR_UNDEF:
		/* error! */
		return (-1);
	case ATTR_ORIGIN:
		if (attr_len != 1)
			return (-1);
		UPD_READ(&a->origin, p, plen, 1);
		break;
	case ATTR_ASPATH:
		if ((r = aspath_verify(p, attr_len, conf->as)) != 0) {
			/* XXX could also be a aspath loop but this
			 * check should be moved to the filtering. */
			logit(LOG_INFO,
			    "XXX aspath_verify failed: error %i\n", r);
			return (-1);
		}
		a->aspath = aspath_create(p, attr_len);
		plen += attr_len;
		break;
	case ATTR_NEXTHOP:
		if (attr_len != 4)
			return (-1);
		UPD_READ(&a->nexthop, p, plen, 4);	/* network byte order */
		break;
	case ATTR_MED:
		if (attr_len != 4)
			return (-1);
		UPD_READ(&tmp32, p, plen, 4);
		a->med = ntohl(tmp32);
		break;
	case ATTR_LOCALPREF:
		if (attr_len != 4)
			return (-1);
		UPD_READ(&tmp32, p, plen, 4);
		a->lpref = ntohl(tmp32);
		break;
	case ATTR_ATOMIC_AGGREGATE:
		if (attr_len > 0)
			return (-1);
		a->aggr_atm = 1;
		break;
	case ATTR_AGGREGATOR:
		if (attr_len != 6)
			return (-1);
		UPD_READ(&tmp16, p, plen, 2);
		a->aggr_as = ntohs(tmp16);
		UPD_READ(&a->aggr_ip, p, plen, 4);	/*network byte order */
		break;
	default:
		/* ignore for now */
		plen += attr_len;
	}

	return (plen);

}

void
rde_update_err(u_int32_t peerid, enum suberr_update errorcode)
{
	u_int8_t	errcode;

	errcode = errorcode;
	se_queued_writes += imsg_compose(se_sock, IMSG_UPDATE_ERR,
	    peerid, (u_char *)&errcode, sizeof(errcode));
}


/*
 * peer functions
 */
LIST_HEAD(rde_peer_head, rde_peer);

struct peer_table {
	struct rde_peer_head	*peer_hashtbl;
	u_long			 peer_hashmask;
} peertable;

#define PEER_HASH(x)		\
	&peertable.peer_hashtbl[(x) & peertable.peer_hashmask]

void
peer_init(struct bgpd_config *bgpconf, u_long hashsize)
{
	struct peer	*p;
	u_long		 hs, i;

	for (hs = 1; hs < hashsize; hs <<= 1) ;
	peertable.peer_hashtbl = calloc(hs, sizeof(struct rde_peer_head));
	if (peertable.peer_hashtbl == NULL)
		fatal("peer_init", errno);

	for (i = 0; i < hs; i++)
		LIST_INIT(&peertable.peer_hashtbl[i]);

	peertable.peer_hashmask = hs - 1;

	for (p = bgpconf->peers; p != NULL; p = p->next) {
		peer_add(p->conf.id, &p->conf);
	}
}

struct rde_peer *
peer_get(u_int32_t id)
{
	struct rde_peer_head	*head;
	struct rde_peer		*peer;

	head = PEER_HASH(id);
	ENSURE(head != NULL);

	LIST_FOREACH(peer, head, peer_l) {
		if (peer->conf.id == id)
			return peer;
	}
	return NULL;
}

void
peer_add(u_int32_t id, struct peer_config *p_conf)
{
	struct rde_peer_head	*head;
	struct rde_peer	*peer;

	ENSURE(peer_get(id) == NULL);

	peer = calloc(1, sizeof(peer));
	if (peer == NULL)
		fatal("peer_add", errno);

	LIST_INIT(&peer->path_h);
	memcpy(&peer->conf, p_conf, sizeof(*p_conf));

	head = PEER_HASH(id);
	ENSURE(head != NULL);

	LIST_INSERT_HEAD(head, peer, peer_l);
}

void
peer_up(u_int32_t id, u_int32_t rid)
{
	struct rde_peer	*peer;

	peer = peer_get(id);
	if (peer == NULL) {
		logit(LOG_CRIT, "peer_up: AYII, unknown peer!");
		return;
	}
	peer->remote_bgpid = rid;
	peer->state = PEER_UP;
}

void
peer_down(u_int32_t id)
{
	struct rde_peer		*peer;
	struct rde_aspath	*asp, *nasp;

	peer = peer_get(id);
	if (peer == NULL) {
		logit(LOG_CRIT, "peer_down: AYII, unknown peer!");
		return;
	}
	peer->remote_bgpid = 0;
	peer->state = PEER_DOWN;

	/* walk through per peer RIB list and remove all prefixes. */
	for (asp = LIST_FIRST(&peer->path_h);
	    asp != LIST_END(&peer->path_h);
	    asp = nasp) {
		nasp = LIST_NEXT(asp, peer_l);
		path_remove(asp);
	}
	LIST_INIT(&peer->path_h);
}
