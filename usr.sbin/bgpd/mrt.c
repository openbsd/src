/*	$OpenBSD: mrt.c,v 1.4 2003/12/20 21:14:55 henning Exp $ */

/*
 * Copyright (c) 2003 Claudio Jeker <cjeker@diehard.n-r-g.com>
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

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include "bgpd.h"
#include "rde.h"
#include "session.h"	/* needed for MSGSIZE_HEADER et al. */

#include "mrt.h"

/*
 * XXX These functions break the imsg encapsulation.
 * XXX The imsg API is way to basic, we need something like
 * XXX imsg_create(), imsg_add(), imsg_close() ...
 */

static int	mrt_dump_entry(int, struct prefix *, u_int16_t,
		    struct peer_config *, u_int32_t);
static void	mrt_dump_header(struct buf *, u_int16_t, u_int16_t, u_int32_t);
static int	mrt_open(struct mrtdump_config *);

/* XXX breaks buf encapsulation */
#define DUMP_BYTE(x, b)							\
	(x)->buf[buf->wpos++] = (b)

#define DUMP_SHORT(x, s)						\
	do {								\
		u_int16_t	t;					\
		t = htons((s));						\
		if (buf_add((x), &t, sizeof(t)) == -1)			\
			fatal("buf_add error", 0);			\
	} while (0)

#define DUMP_LONG(x, l)							\
	do {								\
		u_int32_t	t;					\
		t = htonl((l));						\
		if (buf_add((x), &t, sizeof(t)) == -1)			\
			fatal("buf_add error", 0);			\
	} while (0)

#define DUMP_NLONG(x, l)						\
	do {								\
		u_int32_t	t = (l);				\
		if (buf_add((x), &t, sizeof(t)) == -1)			\
			fatal("buf_add error", 0);			\
	} while (0)

int
mrt_dump_bgp_msg(int fd, u_char *pkg, u_int16_t pkglen, int type,
    struct peer_config *peer, struct bgpd_config *bgp)
{
	struct buf	*buf;
	struct imsg_hdr	 hdr;
	int		 i, n;
	u_int16_t	 len;

	len = pkglen + MRT_BGP4MP_HEADER_SIZE + type>0?MSGSIZE_HEADER:0;

	hdr.len = len + IMSG_HEADER_SIZE + MRT_HEADER_SIZE;
	hdr.type = IMSG_MRT_MSG;
	hdr.peerid = peer->id;
	buf = buf_open(NULL, fd, hdr.len);
	if (buf == NULL)
		fatal("mrt_dump_bgp_msg", errno);
	if (buf_add(buf, &hdr, sizeof(hdr)) == -1)
		fatal("buf_add error", 0);

	mrt_dump_header(buf, MSG_PROTOCOL_BGP4MP, BGP4MP_MESSAGE, len);

	DUMP_SHORT(buf, bgp->as);
	DUMP_SHORT(buf, peer->remote_as);
	DUMP_SHORT(buf, /* ifindex */ 0);
	DUMP_SHORT(buf, 4);
	DUMP_NLONG(buf, peer->local_addr.sin_addr.s_addr);
	DUMP_NLONG(buf, peer->remote_addr.sin_addr.s_addr);

	/* bgp header was chopped off so glue a new one together. */
	if (type > 0) {
		for (i = 0; i < MSGSIZE_HEADER_MARKER; i++)
			DUMP_BYTE(buf, 0xff);
		DUMP_SHORT(buf, pkglen + MSGSIZE_HEADER);
		DUMP_BYTE(buf, type);
	}

	if (buf_add(buf, pkg, pkglen) == -1)
		fatal("buf_add error", 0);

	if ((n = buf_close(buf)) == -1)
		fatal("buf_close error", 0);

	return (n);
}

static int
mrt_dump_entry(int fd, struct prefix *p, u_int16_t snum,
    struct peer_config *peer, u_int32_t id)
{
	struct buf	*buf;
	u_char		*s;
	struct imsg_hdr	 hdr;
	u_int16_t	 len, attr_len;
	int		 n;

	attr_len = attr_length(&p->aspath->flags);
	len = MRT_DUMP_HEADER_SIZE + attr_len;

	hdr.len = len + IMSG_HEADER_SIZE + MRT_HEADER_SIZE;
	hdr.type = IMSG_MRT_MSG;
	hdr.peerid = id;
	buf = buf_open(NULL, fd, hdr.len);
	if (buf == NULL)
		fatal("mrt_dump_entry", errno);
	if (buf_add(buf, &hdr, sizeof(hdr)) == -1)
		fatal("buf_add error", 0);

	mrt_dump_header(buf, MSG_TABLE_DUMP, AFI_IPv4, len);

	DUMP_SHORT(buf, 0);
	DUMP_SHORT(buf, snum);
	DUMP_NLONG(buf, p->prefix->prefix.s_addr);
	DUMP_BYTE(buf, p->prefix->prefixlen);
	DUMP_BYTE(buf, 1);
	DUMP_LONG(buf, p->lastchange);	/* originated */
	DUMP_NLONG(buf, peer->remote_addr.sin_addr.s_addr);
	DUMP_SHORT(buf, peer->remote_as);
	DUMP_SHORT(buf,  attr_len);

	if ((s = buf_reserve(buf, attr_len)) == NULL)
		fatal("buf_reserve error", 0);

	if (attr_dump(s, attr_len, &p->aspath->flags) == -1)
		fatal("attr_dump error", 0);

	if ((n = buf_close(buf)) == -1)
		fatal("buf_close error", 0);

	return (n);
}

static u_int16_t sequencenum = 0;

void
mrt_clear_seq(void)
{
	sequencenum = 0;
}

void
mrt_dump_upcall(struct pt_entry *pt, int fd, int *wait, void *arg)
{
	struct prefix	*p;
	u_int32_t	*idp = arg;
	u_int32_t	 id = *idp;

	/*
	 * dump all prefixes even the inactive ones. That is the way zebra
	 * dumps the table so we do the same. If only the active route should
	 * be dumped p should be set to p = pt->active.
	 */
	LIST_FOREACH(p, &pt->prefix_h, prefix_l)
	    *wait += mrt_dump_entry(fd, p, sequencenum++, &p->peer->conf, id);
}

static void
mrt_dump_header(struct buf *buf, u_int16_t type, u_int16_t subtype,
    u_int32_t len)
{
	struct mrt_header	mrt;
	time_t			now;

	now = time(NULL);

	mrt.timestamp = htonl(now);
	mrt.type = ntohs(type);
	mrt.subtype = ntohs(subtype);
	mrt.length = htonl(len);

	if (buf_add(buf, &mrt, sizeof(mrt)) == -1)
		fatal("buf_add error", 0);
}

static int
mrt_open(struct mrtdump_config *conf)
{
	time_t	now;

	now = time(NULL);
	if (strftime(conf->file, sizeof(conf->file), conf->name,
		    localtime(&now)) == 0) {
		logit(LOG_CRIT, "mrt_open strftime failed");
		conf->fd = -1;
		return -1;
	}

	conf->fd = open(conf->file, O_WRONLY|O_NONBLOCK|O_CREAT|O_TRUNC, 0644);
	if (conf->fd == -1)
		logit(LOG_CRIT, "mrt_open %s: %s",
		    conf->file, strerror(errno));

	return conf->fd;
}

int
mrt_state(struct mrtdump_config *m, enum imsg_type type,
    int rfd, int *rwait /*, int sfd, int *swait */)
{
	switch (m->state) {
	case MRT_STATE_DONE:
		/* no dump expected */
		return (0);
	case MRT_STATE_CLOSE:
		switch (type) {
		case IMSG_NONE:
			if (m->type == MRT_TABLE_DUMP)
				*rwait += imsg_compose(rfd, IMSG_MRT_END,
				    m->id, NULL, 0);
			return (0);
		case IMSG_MRT_END:
			/* dump no longer valid */
			close(m->fd);
			LIST_REMOVE(m, list);
			free(m);
			return (0);
		default:
			break;
		}
		break;
	case MRT_STATE_REOPEN:
		switch (type) {
		case IMSG_NONE:
			if (m->type == MRT_TABLE_DUMP)
				*rwait += imsg_compose(rfd, IMSG_MRT_END,
				    m->id, NULL, 0);
			return (0);
		case IMSG_MRT_END:
			if (m->fd != -1)
				close(m->fd);
			m->state = MRT_STATE_OPEN;
			if (m->type == MRT_TABLE_DUMP)
				*rwait += imsg_compose(rfd, IMSG_MRT_REQ,
				    m->id, NULL, 0);
			return (0);
		default:
			break;
		}
		break;
	case MRT_STATE_OPEN:
		switch (type) {
		case IMSG_NONE:
			if (m->type == MRT_TABLE_DUMP)
				*rwait += imsg_compose(rfd, IMSG_MRT_REQ,
				    m->id, NULL, 0);
			return (0);
		case IMSG_MRT_MSG:
			mrt_open(m);
			m->state = MRT_STATE_RUNNING;
			break;
		default:
			return (0);
		}
		break;
	default:
		break;
	}
	return (1);
}

int
mrt_usr1(struct mrt_config *conf, int rfd, int *rwait
    /*, int sfd, int *swait */)
{
	struct mrtdump_config	*m;
	time_t			 now;
	int			 interval = INT_MAX;

	now = time(NULL);
	LIST_FOREACH(m, conf, list) {
		if (m->type == MRT_TABLE_DUMP) {
			switch (m->state) {
			case MRT_STATE_REOPEN:
			case MRT_STATE_CLOSE:
				break;
			case MRT_STATE_DONE:
				m->state = MRT_STATE_OPEN;
				*rwait += imsg_compose(rfd,
				    IMSG_MRT_REQ, m->id, NULL, 0);
				break;
			default:
				m->state = MRT_STATE_REOPEN;
				*rwait += imsg_compose(rfd,
				    IMSG_MRT_END, m->id, NULL, 0);
				break;
			}

			m->ReopenTimer = now + m->ReopenTimerInterval;
		}
		if (m->ReopenTimer - now < interval)
			interval = m->ReopenTimer - now;
	}

	if (interval != INT_MAX)
		alarm(interval);

	return (0);
}

int
mrt_alrm(struct mrt_config *conf, int rfd, int *rwait
    /*, int sfd, int *swait */)
{
	struct mrtdump_config	*m;
	time_t			 now;
	int			 interval = INT_MAX;

	now = time(NULL);
	LIST_FOREACH(m, conf, list) {
		if (m->ReopenTimerInterval == 0)
			continue;
		if (m->ReopenTimer <= now) {
			switch (m->state) {
			case MRT_STATE_REOPEN:
			case MRT_STATE_CLOSE:
				break;
			case MRT_STATE_DONE:
				m->state = MRT_STATE_OPEN;
				if (m->type == MRT_TABLE_DUMP)
					*rwait += imsg_compose(rfd,
					    IMSG_MRT_REQ, m->id, NULL, 0);
				break;
			default:
				m->state = MRT_STATE_REOPEN;
				if (m->type == MRT_TABLE_DUMP)
					*rwait += imsg_compose(rfd,
					    IMSG_MRT_END, m->id, NULL, 0);
				break;
			}

			m->ReopenTimer = now + m->ReopenTimerInterval;
		}
		if (m->ReopenTimer - now < interval)
			interval = m->ReopenTimer - now;
	}

	if (interval != INT_MAX)
		alarm(interval);

	return (0);
}

static u_int32_t	 max_id = 1;

static struct mrtdump_config *
getconf(struct mrt_config *c, struct mrtdump_config *m)
{
	struct mrtdump_config	*t;
	LIST_FOREACH(t, c, list)
		if (t->type == m->type)
			return t;
	return (NULL);
}

int
mrt_mergeconfig(struct mrt_config *xconf, struct mrt_config *conf)
{
	struct mrtdump_config	*m, *xm;
	time_t			 now;
	int			 interval = INT_MAX;

	now = time(NULL);
	LIST_FOREACH(m, conf, list)
		if ((xm = getconf(xconf, m)) == NULL) {
			/* NEW */
			if ((xm = calloc(1, sizeof(struct mrtdump_config))) ==
			    NULL)
				fatal("mrt_mergeconfig", errno);
			memcpy(xm, m, sizeof(struct mrtdump_config));
			xm->id = max_id++;
			if (xm->ReopenTimerInterval != 0) {
				xm->ReopenTimer = now + xm->ReopenTimerInterval;
				interval = xm->ReopenTimerInterval < interval ?
				    xm->ReopenTimerInterval : interval;
			}
			xm->state=MRT_STATE_OPEN;
			LIST_INSERT_HEAD(xconf, xm, list);
		} else {
			/* MERGE */
			if (strlcpy(xm->name, m->name, sizeof(xm->name)) >
			    sizeof(xm->name))
				fatal("mrt_mergeconfig: strlcpy", 0);
			xm->ReopenTimerInterval = m->ReopenTimerInterval;
			if (xm->ReopenTimerInterval != 0) {
				xm->ReopenTimer = now + xm->ReopenTimerInterval;
				interval = xm->ReopenTimerInterval < interval ?
				    xm->ReopenTimerInterval : interval;
			}
			xm->state=MRT_STATE_REOPEN;
		}
	LIST_FOREACH(xm, xconf, list)
		if (getconf(conf, xm) == NULL)
			/* REMOVE */
			xm->state = MRT_STATE_CLOSE;

	/* free config */
	for (m = LIST_FIRST(conf); m != LIST_END(conf); m = xm) {
		xm = LIST_NEXT(m, list);
		free(m);
	}
	free(conf);

	if (interval != INT_MAX)
		alarm(interval);

	return (0);
}

