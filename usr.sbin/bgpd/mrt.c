/*	$OpenBSD: mrt.c,v 1.30 2004/04/29 19:56:04 deraadt Exp $ */

/*
 * Copyright (c) 2003, 2004 Claudio Jeker <claudio@openbsd.org>
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
#include "session.h"

#include "mrt.h"

static u_int16_t	mrt_attr_length(struct attr_flags *);
static int		mrt_attr_dump(void *, u_int16_t, struct attr_flags *);
static int		mrt_dump_entry(struct mrt_config *, struct prefix *,
			    u_int16_t, struct peer_config *);
static int		mrt_dump_header(struct buf *, u_int16_t, u_int16_t,
			    u_int32_t);
static int		mrt_open(struct mrt *);

#define DUMP_BYTE(x, b)							\
	do {								\
		u_char		t = (b);				\
		if (imsg_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump1: imsg_add error");		\
			return (-1);					\
		}							\
	} while (0)

#define DUMP_SHORT(x, s)						\
	do {								\
		u_int16_t	t;					\
		t = htons((s));						\
		if (imsg_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump2: imsg_add error");		\
			return (-1);					\
		}							\
	} while (0)

#define DUMP_LONG(x, l)							\
	do {								\
		u_int32_t	t;					\
		t = htonl((l));						\
		if (imsg_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump3: imsg_add error");		\
			return (-1);					\
		}							\
	} while (0)

#define DUMP_NLONG(x, l)						\
	do {								\
		u_int32_t	t = (l);				\
		if (imsg_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump4: imsg_add error");		\
			return (-1);					\
		}							\
	} while (0)

int
mrt_dump_bgp_msg(struct mrt_config *mrt, void *pkg, u_int16_t pkglen,
    struct peer_config *peer, struct bgpd_config *bgp)
{
	struct buf	*buf;
	u_int16_t	 len;

	len = pkglen + MRT_BGP4MP_HEADER_SIZE;

	if ((buf = imsg_create(mrt->ibuf, IMSG_MRT_MSG, mrt->id,
	    len + MRT_HEADER_SIZE)) == NULL) {
		log_warnx("mrt_dump_bgp_msg: imsg_open error");
		return (-1);
	}

	if (mrt_dump_header(buf, MSG_PROTOCOL_BGP4MP, BGP4MP_MESSAGE,
	    len) == -1) {
		log_warnx("mrt_dump_bgp_msg: imsg_add error");
		return (-1);
	}

	DUMP_SHORT(buf, bgp->as);
	DUMP_SHORT(buf, peer->remote_as);
	DUMP_SHORT(buf, /* ifindex */ 0);
	DUMP_SHORT(buf, 4);
	DUMP_NLONG(buf, peer->local_addr.v4.s_addr);
	DUMP_NLONG(buf, peer->remote_addr.v4.s_addr);

	if (imsg_add(buf, pkg, pkglen) == -1) {
		log_warnx("mrt_dump_bgp_msg: imsg_add error");
		return (-1);
	}

	if ((imsg_close(mrt->ibuf, buf)) == -1) {
		log_warnx("mrt_dump_bgp_msg: imsg_close error");
		return (-1);
	}

	return (len + MRT_HEADER_SIZE);
}

int
mrt_dump_state(struct mrt_config *mrt, u_int16_t old_state, u_int16_t new_state,
    struct peer_config *peer, struct bgpd_config *bgp)
{
	struct buf	*buf;
	u_int16_t	 len;

	len = 4 + MRT_BGP4MP_HEADER_SIZE;

	if ((buf = imsg_create(mrt->ibuf, IMSG_MRT_MSG, mrt->id,
	    len + MRT_HEADER_SIZE)) == NULL) {
		log_warnx("mrt_dump_bgp_state: imsg_open error");
		return (-1);
	}

	if (mrt_dump_header(buf, MSG_PROTOCOL_BGP4MP, BGP4MP_STATE_CHANGE,
	    len) == -1) {
		log_warnx("mrt_dump_bgp_state: imsg_add error");
		return (-1);
	}

	DUMP_SHORT(buf, bgp->as);
	DUMP_SHORT(buf, peer->remote_as);
	DUMP_SHORT(buf, /* ifindex */ 0);
	DUMP_SHORT(buf, 4);
	DUMP_NLONG(buf, peer->local_addr.v4.s_addr);
	DUMP_NLONG(buf, peer->remote_addr.v4.s_addr);

	DUMP_SHORT(buf, old_state);
	DUMP_SHORT(buf, new_state);

	if ((imsg_close(mrt->ibuf, buf)) == -1) {
		log_warnx("mrt_dump_bgp_state: imsg_close error");
		return (-1);
	}

	return (len + MRT_HEADER_SIZE);
}

static u_int16_t
mrt_attr_length(struct attr_flags *a)
{
	struct attr	*oa;
	u_int16_t	 alen, plen;

	alen = 4 /* origin */ + 7 /* nexthop */ + 7 /* lpref */;
	plen = aspath_length(a->aspath);
	alen += 2 + plen + (plen > 255 ? 2 : 1);
	if (a->med != 0)
		alen += 7;

	TAILQ_FOREACH(oa, &a->others, attr_l)
		alen += 2 + oa->len + (oa->len > 255 ? 2 : 1);

	return alen;
}

static int
mrt_attr_dump(void *p, u_int16_t len, struct attr_flags *a)
{
	struct attr	*oa;
	u_char		*buf = p;
	u_int32_t	 tmp32;
	int		 r;
	u_int16_t	 aslen, wlen = 0;

	/* origin */
	if ((r = attr_write(buf + wlen, len, ATTR_WELL_KNOWN, ATTR_ORIGIN,
	    &a->origin, 1)) == -1)
		return (-1);
	wlen += r; len -= r;

	/* aspath */
	aslen = aspath_length(a->aspath);
	if ((r = attr_write(buf + wlen, len, ATTR_WELL_KNOWN, ATTR_ASPATH,
	    aspath_dump(a->aspath), aslen)) == -1)
		return (-1);
	wlen += r; len -= r;

	/* nexthop, already network byte order */
	if ((r = attr_write(buf + wlen, len, ATTR_WELL_KNOWN, ATTR_NEXTHOP,
	    &a->nexthop, 4)) ==	-1)
		return (-1);
	wlen += r; len -= r;

	/* MED, non transitive */
	if (a->med != 0) {
		tmp32 = htonl(a->med);
		if ((r = attr_write(buf + wlen, len, ATTR_OPTIONAL, ATTR_MED,
		    &tmp32, 4)) == -1)
			return (-1);
		wlen += r; len -= r;
	}

	/* local preference, only valid for ibgp */
	tmp32 = htonl(a->lpref);
	if ((r = attr_write(buf + wlen, len, ATTR_WELL_KNOWN, ATTR_LOCALPREF,
	    &tmp32, 4)) == -1)
		return (-1);
	wlen += r; len -= r;

	/* dump all other path attributes without modification */
	TAILQ_FOREACH(oa, &a->others, attr_l) {
		if ((r = attr_write(buf + wlen, len, oa->flags, oa->type,
		    oa->data, oa->len)) == -1)
			return (-1);
		wlen += r; len -= r;
	}

	return (wlen);
}

static int
mrt_dump_entry(struct mrt_config *mrt, struct prefix *p, u_int16_t snum,
    struct peer_config *peer)
{
	struct buf	*buf;
	void		*bptr;
	u_int16_t	 len, attr_len;

	attr_len = mrt_attr_length(&p->aspath->flags);
	len = MRT_DUMP_HEADER_SIZE + attr_len;

	if ((buf = imsg_create(mrt->ibuf, IMSG_MRT_MSG, mrt->id,
	    len + MRT_HEADER_SIZE)) == NULL) {
		log_warnx("mrt_dump_entry: imsg_open error");
		return (-1);
	}

	if (mrt_dump_header(buf, MSG_TABLE_DUMP, AFI_IPv4, len) == -1) {
		log_warnx("mrt_dump_bgp_msg: buf_add error");
		return (-1);
	}

	DUMP_SHORT(buf, 0);
	DUMP_SHORT(buf, snum);
	DUMP_NLONG(buf, p->prefix->prefix.v4.s_addr);
	DUMP_BYTE(buf, p->prefix->prefixlen);
	DUMP_BYTE(buf, 1);		/* state */
	DUMP_LONG(buf, p->lastchange);	/* originated */
	DUMP_NLONG(buf, peer->remote_addr.v4.s_addr);
	DUMP_SHORT(buf, peer->remote_as);
	DUMP_SHORT(buf, attr_len);

	if ((bptr = buf_reserve(buf, attr_len)) == NULL) {
		log_warnx("mrt_dump_entry: buf_reserve error");
		buf_free(buf);
		return (-1);
	}

	if (mrt_attr_dump(bptr, attr_len, &p->aspath->flags) == -1) {
		log_warnx("mrt_dump_entry: mrt_attr_dump error");
		buf_free(buf);
		return (-1);
	}

	if ((imsg_close(mrt->ibuf, buf)) == -1) {
		log_warnx("mrt_dump_bgp_state: imsg_close error");
		return (-1);
	}

	return (len + MRT_HEADER_SIZE);
}

static u_int16_t sequencenum = 0;

void
mrt_clear_seq(void)
{
	sequencenum = 0;
}

void
mrt_dump_upcall(struct pt_entry *pt, void *ptr)
{
	struct mrt_config	*mrtbuf = ptr;
	struct prefix		*p;

	/*
	 * dump all prefixes even the inactive ones. That is the way zebra
	 * dumps the table so we do the same. If only the active route should
	 * be dumped p should be set to p = pt->active.
	 */
	LIST_FOREACH(p, &pt->prefix_h, prefix_l)
		mrt_dump_entry(mrtbuf, p, sequencenum++,
		    &p->aspath->peer->conf);
}

static int
mrt_dump_header(struct buf *buf, u_int16_t type, u_int16_t subtype,
    u_int32_t len)
{
	time_t			now;

	now = time(NULL);

	DUMP_LONG(buf, now);
	DUMP_SHORT(buf, type);
	DUMP_SHORT(buf, subtype);
	DUMP_LONG(buf, len);

	return (0);
}

static struct imsgbuf	*mrt_imsgbuf[2];

void
mrt_init(struct imsgbuf *rde, struct imsgbuf *se)
{
	mrt_imsgbuf[0] = rde;
	mrt_imsgbuf[1] = se;
}

static int
mrt_open(struct mrt *mrt)
{
	time_t	now;

	now = time(NULL);
	if (strftime(mrt->file, sizeof(mrt->file), mrt->name,
		    localtime(&now)) == 0) {
		log_warnx("mrt_open: strftime conversion failed");
		mrt->msgbuf.fd = -1;
		return (0);
	}

	mrt->msgbuf.fd = open(mrt->file,
	    O_WRONLY|O_NONBLOCK|O_CREAT|O_TRUNC, 0644);
	if (mrt->msgbuf.fd == -1) {
		log_warnx("mrt_open %s: %s",
		    mrt->file, strerror(errno));
		return (0);
	}
	return (1);
}

static int
mrt_close(struct mrt *mrt)
{
	/*
	 * close the mrt filedescriptor but first ensure that the last
	 * mrt message was written correctly. If not mrt_write needs to do
	 * that the next time called.
	 * To ensure this we need to fiddle around with internal msgbuf stuff.
	 */
	if (msgbuf_unbounded(&mrt->msgbuf))
		return (0);

	if (mrt->msgbuf.fd != -1) {
		close(mrt->msgbuf.fd);
		mrt->msgbuf.fd = -1;
	}

	return (1);
}

void
mrt_abort(struct mrt *mrt)
{
	/*
	 * something failed horribly. Stop all dumping and go back to start
	 * position. Retry after MRT_MIN_RETRY or ReopenTimerInterval. Which-
	 * ever is bigger.
	 */
	msgbuf_clear(&mrt->msgbuf);
	mrt_close(mrt);
	mrt->state = MRT_STATE_STOPPED;

	if (MRT_MIN_RETRY > mrt->ReopenTimerInterval)
		mrt->ReopenTimer = MRT_MIN_RETRY + time(NULL);
	else
		mrt->ReopenTimer = mrt->ReopenTimerInterval + time(NULL);
}

int
mrt_queue(struct mrt_head *mrtc, struct imsg *imsg)
{
	struct buf	*wbuf;
	struct mrt	*m;
	ssize_t		 len;
	int		 n;

	if (imsg->hdr.type != IMSG_MRT_MSG && imsg->hdr.type != IMSG_MRT_END)
		return (-1);

	LIST_FOREACH(m, mrtc, list) {
		if (m->conf.id != imsg->hdr.peerid)
			continue;
		if (m->state != MRT_STATE_RUNNING &&
		    m->state != MRT_STATE_REOPEN)
			return (0);

		if (imsg->hdr.type == IMSG_MRT_END) {
			m->state = MRT_STATE_CLOSE;
			return (0);
		}

		len = imsg->hdr.len - IMSG_HEADER_SIZE;
		wbuf = buf_open(len);
		if (wbuf == NULL)
			return (-1);
		if (buf_add(wbuf, imsg->data, len) == -1) {
			buf_free(wbuf);
			return (-1);
		}
		if ((n = buf_close(&m->msgbuf, wbuf)) < 0) {
			buf_free(wbuf);
			return (-1);
		}
		return (n);
	}
	return (0);
}

int
mrt_write(struct mrt *mrt)
{
	int	r;

	if (mrt->state == MRT_STATE_REOPEN ||
	    mrt->state == MRT_STATE_REMOVE)
		r = msgbuf_writebound(&mrt->msgbuf);
	else
		r = msgbuf_write(&mrt->msgbuf);

	switch (r) {
	case 1:
		/* only msgbuf_writebound returns 1 */
		break;
	case 0:
		if (mrt->state == MRT_STATE_CLOSE && mrt->msgbuf.queued == 0) {
			if (mrt_close(mrt) != 1) {
				log_warnx("mrt_write: mrt_close failed");
				mrt_abort(mrt);
				return (0);
			}
			mrt->state = MRT_STATE_STOPPED;
		}
		return (0);
	case -1:
		log_warnx("mrt_write: msgbuf_write: %s",
		    strerror(errno));
		mrt_abort(mrt);
		return (0);
	case -2:
		log_warnx("mrt_write: msgbuf_write: %s",
		    "connection closed");
		mrt_abort(mrt);
		return (0);
	default:
		fatalx("mrt_write: unexpected retval from msgbuf_write");
	}

	if (mrt_close(mrt) != 1) {
		log_warnx("mrt_write: mrt_close failed");
		mrt_abort(mrt);
		return (0);
	}

	switch (mrt->state) {
	case MRT_STATE_REMOVE:
		/*
		 * Remove request: free all left buffers and
		 * remove the descriptor.
		 */
		msgbuf_clear(&mrt->msgbuf);
		LIST_REMOVE(mrt, list);
		free(mrt);
		return (0);
	case MRT_STATE_REOPEN:
		if (mrt_open(mrt) == 0) {
			mrt_abort(mrt);
			return (0);
		} else {
			if (mrt->ReopenTimerInterval != 0)
				mrt->ReopenTimer = time(NULL) +
				    mrt->ReopenTimerInterval;
			mrt->state = MRT_STATE_RUNNING;
		}
		break;
	default:
		break;
	}
	return (1);
}

int
mrt_select(struct mrt_head *mc, struct pollfd *pfd, struct mrt **mrt,
    int start, int size, int *timeout)
{
	struct mrt	*m, *xm;
	time_t		 now;
	int		 t;

	now = time(NULL);
	for (m = LIST_FIRST(mc); m != NULL; m = xm) {
		xm = LIST_NEXT(m, list);
		if (m->state == MRT_STATE_TOREMOVE) {
			imsg_compose(m->ibuf, IMSG_MRT_END, 0,
			    &m->conf, sizeof(m->conf));
			if (mrt_close(m) == 0) {
				m->state = MRT_STATE_REMOVE;
				m->ReopenTimer = 0;
			} else {
				msgbuf_clear(&m->msgbuf);
				LIST_REMOVE(m, list);
				free(m);
				continue;
			}
		}
		if (m->state == MRT_STATE_OPEN) {
			switch (m->conf.type) {
			case MRT_TABLE_DUMP:
				m->ibuf = mrt_imsgbuf[0];
				break;
			case MRT_ALL_IN:
			case MRT_ALL_OUT:
			case MRT_UPDATE_IN:
			case MRT_UPDATE_OUT:
				m->ibuf = mrt_imsgbuf[1];
				break;
			default:
				continue;
			}
			if (mrt_open(m) == 0) {
				mrt_abort(m);
				t = m->ReopenTimer - now;
				if (*timeout > t)
					*timeout = t;
				continue;
			}
			if (m->ReopenTimerInterval != 0)
				m->ReopenTimer = now + m->ReopenTimerInterval;
			m->state = MRT_STATE_RUNNING;
			imsg_compose(m->ibuf, IMSG_MRT_REQ, 0,
			    &m->conf, sizeof(m->conf));
		}
		if (m->state == MRT_STATE_REOPEN) {
			if (mrt_close(m) == 0) {
				m->state = MRT_STATE_REOPEN;
				continue;
			}
			if (mrt_open(m) == 0) {
				mrt_abort(m);
				t = m->ReopenTimer - now;
				if (*timeout > t)
					*timeout = t;
				continue;
			}
			if (m->ReopenTimerInterval != 0)
				m->ReopenTimer = now + m->ReopenTimerInterval;
			m->state = MRT_STATE_RUNNING;
		}
		if (m->ReopenTimer != 0) {
			t = m->ReopenTimer - now;
			if (t <= 0 && (m->state == MRT_STATE_RUNNING ||
			    m->state == MRT_STATE_STOPPED)) {
				if (m->state == MRT_STATE_RUNNING) {
					/* reopen file */
					if (mrt_close(m) == 0) {
						m->state = MRT_STATE_REOPEN;
						continue;
					}
				}
				if (mrt_open(m) == 0) {
					mrt_abort(m);
					t = m->ReopenTimer - now;
					if (*timeout > t)
						*timeout = t;
					continue;
				}
				if (m->conf.type == MRT_TABLE_DUMP &&
				    m->state == MRT_STATE_STOPPED) {
					imsg_compose(mrt_imsgbuf[0],
					    IMSG_MRT_REQ, 0,
					    &m->conf, sizeof(m->conf));
				}

				m->state = MRT_STATE_RUNNING;
				if (m->ReopenTimerInterval != 0) {
					m->ReopenTimer = now +
					    m->ReopenTimerInterval;
					if (*timeout > m->ReopenTimerInterval)
						*timeout = t;
				}
			}
		}
		if (m->msgbuf.queued > 0) {
			if (m->msgbuf.fd == -1 ||
			    m->state == MRT_STATE_STOPPED) {
				log_warnx("mrt_select: orphaned buffer");
				mrt_abort(m);
				continue;
			}
			if (start < size) {
				pfd[start].fd = m->msgbuf.fd;
				pfd[start].events = POLLOUT;
				mrt[start++] = m;
			}
		}
	}
	return (start);
}

int
mrt_handler(struct mrt_head *mrt)
{
	struct mrt	*m;
	time_t		 now;

	now = time(NULL);
	LIST_FOREACH(m, mrt, list) {
		if (m->state == MRT_STATE_RUNNING)
			m->state = MRT_STATE_REOPEN;
		if (m->conf.type == MRT_TABLE_DUMP) {
			if (m->state == MRT_STATE_STOPPED) {
				if (mrt_open(m) == 0) {
					mrt_abort(m);
					break;
				}
				imsg_compose(mrt_imsgbuf[0], IMSG_MRT_REQ, 0,
				    &m->conf, sizeof(m->conf));
				m->state = MRT_STATE_RUNNING;
			}
		}
		if (m->ReopenTimerInterval != 0)
			m->ReopenTimer = now + m->ReopenTimerInterval;
	}
	return (0);
}

static u_int32_t	 max_id = 1;

static struct mrt *
getconf(struct mrt_head *c, struct mrt *m)
{
	struct mrt	*t;

	LIST_FOREACH(t, c, list) {
		if (t->conf.type != m->conf.type)
			continue;
		if (t->conf.type == MRT_TABLE_DUMP)
			return t;
		if (t->conf.peer_id == m->conf.peer_id &&
		    t->conf.group_id == m->conf.group_id)
			return t;
	}
	return (NULL);
}

int
mrt_mergeconfig(struct mrt_head *xconf, struct mrt_head *nconf)
{
	struct mrt	*m, *xm;

	LIST_FOREACH(m, nconf, list)
		if ((xm = getconf(xconf, m)) == NULL) {
			/* NEW */
			if ((xm = calloc(1, sizeof(struct mrt))) == NULL)
				fatal("mrt_mergeconfig");
			memcpy(xm, m, sizeof(struct mrt));
			msgbuf_init(&xm->msgbuf);
			xm->conf.id = max_id++;
			xm->state = MRT_STATE_OPEN;
			LIST_INSERT_HEAD(xconf, xm, list);
		} else {
			/* MERGE */
			if (strlcpy(xm->name, m->name, sizeof(xm->name)) >=
			    sizeof(xm->name))
				fatalx("mrt_mergeconfig: strlcpy");
			xm->ReopenTimerInterval = m->ReopenTimerInterval;
			xm->state = MRT_STATE_REOPEN;
		}

	LIST_FOREACH(xm, xconf, list)
		if (getconf(nconf, xm) == NULL)
			/* REMOVE */
			xm->state = MRT_STATE_TOREMOVE;

	/* free config */
	while ((m = LIST_FIRST(nconf)) != NULL) {
		LIST_REMOVE(m, list);
		free(m);
	}

	return (0);
}

