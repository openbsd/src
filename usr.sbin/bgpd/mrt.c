/*	$OpenBSD: mrt.c,v 1.48 2005/11/29 21:11:07 claudio Exp $ */

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

static u_int16_t	mrt_attr_length(struct rde_aspath *, int);
static int		mrt_attr_dump(void *, u_int16_t, struct rde_aspath *,
			    struct bgpd_addr *);
static int		mrt_dump_entry_mp(struct mrt *, struct prefix *,
			    u_int16_t, struct rde_peer*);
static int		mrt_dump_entry(struct mrt *, struct prefix *,
			    u_int16_t, struct rde_peer*);
static int		mrt_dump_header(struct buf *, u_int16_t, u_int16_t,
			    u_int32_t);
static int		mrt_open(struct mrt *, time_t);

#define DUMP_BYTE(x, b)							\
	do {								\
		u_char		t = (b);				\
		if (buf_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump1: buf_add error");		\
			buf_free((x));					\
			return (-1);					\
		}							\
	} while (0)

#define DUMP_SHORT(x, s)						\
	do {								\
		u_int16_t	t;					\
		t = htons((s));						\
		if (buf_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump2: buf_add error");		\
			buf_free((x));					\
			return (-1);					\
		}							\
	} while (0)

#define DUMP_LONG(x, l)							\
	do {								\
		u_int32_t	t;					\
		t = htonl((l));						\
		if (buf_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump3: buf_add error");		\
			buf_free((x));					\
			return (-1);					\
		}							\
	} while (0)

#define DUMP_NLONG(x, l)						\
	do {								\
		u_int32_t	t = (l);				\
		if (buf_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump4: buf_add error");		\
			buf_free((x));					\
			return (-1);					\
		}							\
	} while (0)

int
mrt_dump_bgp_msg(struct mrt *mrt, void *pkg, u_int16_t pkglen,
    struct peer *peer, struct bgpd_config *bgp)
{
	struct buf	*buf;
	u_int16_t	 len;

	switch (peer->sa_local.ss_family) {
	case AF_INET:
		len = pkglen + MRT_BGP4MP_IPv4_HEADER_SIZE;
		break;
	case AF_INET6:
		len = pkglen + MRT_BGP4MP_IPv6_HEADER_SIZE;
		break;
	default:
		return (-1);
	}

	if ((buf = buf_open(len + MRT_HEADER_SIZE)) == NULL) {
		log_warnx("mrt_dump_bgp_msg: buf_open error");
		return (-1);
	}

	if (mrt_dump_header(buf, MSG_PROTOCOL_BGP4MP, BGP4MP_MESSAGE,
	    len) == -1) {
		log_warnx("mrt_dump_bgp_msg: buf_add error");
		return (-1);
	}

	DUMP_SHORT(buf, bgp->as);
	DUMP_SHORT(buf, peer->conf.remote_as);
	DUMP_SHORT(buf, /* ifindex */ 0);
	switch (peer->sa_local.ss_family) {
	case AF_INET:
		DUMP_SHORT(buf, AFI_IPv4);
		DUMP_NLONG(buf,
		    ((struct sockaddr_in *)&peer->sa_local)->sin_addr.s_addr);
		DUMP_NLONG(buf,
		    ((struct sockaddr_in *)&peer->sa_remote)->sin_addr.s_addr);
		break;
	case AF_INET6:
		DUMP_SHORT(buf, AFI_IPv6);
		if (buf_add(buf,
		    &((struct sockaddr_in6 *)&peer->sa_local)->sin6_addr,
		    sizeof(struct in6_addr)) == -1 ||
		    buf_add(buf,
		    &((struct sockaddr_in6 *)&peer->sa_remote)->sin6_addr,
		    sizeof(struct in6_addr)) == -1) {
			log_warnx("mrt_dump_bgp_msg: buf_add error");
			buf_free(buf);
			return (-1);
		}
		break;
	}

	if (buf_add(buf, pkg, pkglen) == -1) {
		log_warnx("mrt_dump_bgp_msg: buf_add error");
		buf_free(buf);
		return (-1);
	}

	TAILQ_INSERT_TAIL(&mrt->bufs, buf, entry);
	mrt->queued++;

	return (len + MRT_HEADER_SIZE);
}

int
mrt_dump_state(struct mrt *mrt, u_int16_t old_state, u_int16_t new_state,
    struct peer *peer, struct bgpd_config *bgp)
{
	struct buf	*buf;
	u_int16_t	 len;

	switch (peer->sa_local.ss_family) {
	case AF_INET:
		len = 4 + MRT_BGP4MP_IPv4_HEADER_SIZE;
		break;
	case AF_INET6:
		len = 4 + MRT_BGP4MP_IPv6_HEADER_SIZE;
		break;
	default:
		return (-1);
	}

	if ((buf = buf_open(len + MRT_HEADER_SIZE)) == NULL) {
		log_warnx("mrt_dump_bgp_state: buf_open error");
		return (-1);
	}

	if (mrt_dump_header(buf, MSG_PROTOCOL_BGP4MP, BGP4MP_STATE_CHANGE,
	    len) == -1) {
		log_warnx("mrt_dump_bgp_state: buf_add error");
		return (-1);
	}

	DUMP_SHORT(buf, bgp->as);
	DUMP_SHORT(buf, peer->conf.remote_as);
	DUMP_SHORT(buf, /* ifindex */ 0);
	switch (peer->sa_local.ss_family) {
	case AF_INET:
		DUMP_SHORT(buf, AFI_IPv4);
		DUMP_NLONG(buf,
		    ((struct sockaddr_in *)&peer->sa_local)->sin_addr.s_addr);
		DUMP_NLONG(buf,
		    ((struct sockaddr_in *)&peer->sa_remote)->sin_addr.s_addr);
		break;
	case AF_INET6:
		DUMP_SHORT(buf, AFI_IPv6);
		if (buf_add(buf,
		    &((struct sockaddr_in6 *)&peer->sa_local)->sin6_addr,
		    sizeof(struct in6_addr)) == -1 ||
		    buf_add(buf,
		    &((struct sockaddr_in6 *)&peer->sa_remote)->sin6_addr,
		    sizeof(struct in6_addr)) == -1) {
			log_warnx("mrt_dump_bgp_msg: buf_add error");
			buf_free(buf);
			return (-1);
		}
		break;
	}

	DUMP_SHORT(buf, old_state);
	DUMP_SHORT(buf, new_state);

	TAILQ_INSERT_TAIL(&mrt->bufs, buf, entry);
	mrt->queued++;

	return (len + MRT_HEADER_SIZE);
}

static u_int16_t
mrt_attr_length(struct rde_aspath *a, int oldform)
{
	struct attr	*oa;
	u_int16_t	 alen, plen;

	alen = 4 /* origin */ + 7 /* lpref */;
	if (oldform)
		alen += 7 /* nexthop */;
	plen = aspath_length(a->aspath);
	alen += 2 + plen + (plen > 255 ? 2 : 1);
	if (a->med != 0)
		alen += 7;

	TAILQ_FOREACH(oa, &a->others, entry)
		alen += 2 + oa->len + (oa->len > 255 ? 2 : 1);

	return alen;
}

static int
mrt_attr_dump(void *p, u_int16_t len, struct rde_aspath *a,
    struct bgpd_addr *nexthop)
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

	if (nexthop) {
		/* nexthop, already network byte order */
		if ((r = attr_write(buf + wlen, len, ATTR_WELL_KNOWN,
		    ATTR_NEXTHOP, &nexthop->v4.s_addr, 4)) ==	-1)
			return (-1);
		wlen += r; len -= r;
	}

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
	TAILQ_FOREACH(oa, &a->others, entry) {
		if ((r = attr_write(buf + wlen, len, oa->flags, oa->type,
		    oa->data, oa->len)) == -1)
			return (-1);
		wlen += r; len -= r;
	}

	return (wlen);
}

static int
mrt_dump_entry_mp(struct mrt *mrt, struct prefix *p, u_int16_t snum,
    struct rde_peer *peer)
{
	struct buf	*buf;
	void		*bptr;
	struct bgpd_addr addr, nexthop, *nh;
	u_int16_t	 len, attr_len;
	u_int8_t	 p_len;
	sa_family_t	 af;

	attr_len = mrt_attr_length(p->aspath, 0);
	p_len = PREFIX_SIZE(p->prefix->prefixlen);
	pt_getaddr(p->prefix, &addr);

	af = peer->remote_addr.af == 0 ? addr.af : peer->remote_addr.af;
	switch (af) {
	case AF_INET:
		len = MRT_BGP4MP_IPv4_HEADER_SIZE;
		break;
	case AF_INET6:
		len = MRT_BGP4MP_IPv6_HEADER_SIZE;
		break;
	default:
		return (-1);
	}

	switch (addr.af) {
	case AF_INET:
		len += MRT_BGP4MP_IPv4_ENTRY_SIZE + p_len + attr_len;
		break;
	case AF_INET6:
		len += MRT_BGP4MP_IPv4_ENTRY_SIZE + p_len + attr_len;
		break;
	default:
		return (-1);
	}

	if ((buf = buf_open(len + MRT_HEADER_SIZE)) == NULL) {
		log_warnx("mrt_dump_entry_mp: buf_open error");
		return (-1);
	}

	if (mrt_dump_header(buf, MSG_PROTOCOL_BGP4MP, BGP4MP_ENTRY,
	    len) == -1) {
		log_warnx("mrt_dump_entry_mp: buf_add error");
		return (-1);
	}

	DUMP_SHORT(buf, rde_local_as());
	DUMP_SHORT(buf, peer->conf.remote_as);
	DUMP_SHORT(buf, /* ifindex */ 0);

	switch (af) {
	case AF_INET:
		DUMP_SHORT(buf, AFI_IPv4);
		DUMP_NLONG(buf, peer->local_v4_addr.v4.s_addr);
		DUMP_NLONG(buf, peer->remote_addr.v4.s_addr);
		break;
	case AF_INET6:
		DUMP_SHORT(buf, AFI_IPv6);
		if (buf_add(buf, &peer->local_v6_addr.v6,
		    sizeof(struct in6_addr)) == -1 ||
		    buf_add(buf, &peer->remote_addr.v6,
		    sizeof(struct in6_addr)) == -1) {
			log_warnx("mrt_dump_entry_mp: buf_add error");
			buf_free(buf);
			return (-1);
		}
		break;
	}

	DUMP_SHORT(buf, 0);		/* view */
	DUMP_SHORT(buf, 1);		/* status */
	DUMP_LONG(buf, p->lastchange);	/* originated */

	if (p->aspath->nexthop == NULL) {
		bzero(&nexthop, sizeof(struct bgpd_addr));
		nexthop.af = addr.af;
		nh = &nexthop;
	} else
		nh = &p->aspath->nexthop->exit_nexthop;

	switch (addr.af) {
	case AF_INET:
		DUMP_SHORT(buf, AFI_IPv4);	/* afi */
		DUMP_BYTE(buf, SAFI_UNICAST);	/* safi */
		DUMP_BYTE(buf, 4);		/* nhlen */
		DUMP_NLONG(buf, nh->v4.s_addr);	/* nexthop */
		break;
	case AF_INET6:
		DUMP_SHORT(buf, AFI_IPv6);	/* afi */
		DUMP_BYTE(buf, SAFI_UNICAST);	/* safi */
		DUMP_BYTE(buf, 16);		/* nhlen */
		if (buf_add(buf, &nh->v6, sizeof(struct in6_addr)) == -1) {
			log_warnx("mrt_dump_entry_mp: buf_add error");
			buf_free(buf);
			return (-1);
		}
		break;
	}

	if ((bptr = buf_reserve(buf, p_len)) == NULL) {
		log_warnx("mrt_dump_entry_mpbuf_reserve error");
		buf_free(buf);
		return (-1);
	}
	if (prefix_write(bptr, p_len, &addr, p->prefix->prefixlen) == -1) {
		log_warnx("mrt_dump_entry_mpprefix_write error");
		buf_free(buf);
		return (-1);
	}

	DUMP_SHORT(buf, attr_len);
	if ((bptr = buf_reserve(buf, attr_len)) == NULL) {
		log_warnx("mrt_dump_entry_mpbuf_reserve error");
		buf_free(buf);
		return (-1);
	}

	if (mrt_attr_dump(bptr, attr_len, p->aspath, NULL) == -1) {
		log_warnx("mrt_dump_entry_mpmrt_attr_dump error");
		buf_free(buf);
		return (-1);
	}

	TAILQ_INSERT_TAIL(&mrt->bufs, buf, entry);
	mrt->queued++;

	return (len + MRT_HEADER_SIZE);
}

static int
mrt_dump_entry(struct mrt *mrt, struct prefix *p, u_int16_t snum,
    struct rde_peer *peer)
{
	struct buf	*buf;
	void		*bptr;
	struct bgpd_addr addr, *nh;
	u_int16_t	 len, attr_len;

	if (p->prefix->af != AF_INET && peer->remote_addr.af == AF_INET)
		/* only for true IPv4 */
		return (0);

	attr_len = mrt_attr_length(p->aspath, 1);
	len = MRT_DUMP_HEADER_SIZE + attr_len;
	pt_getaddr(p->prefix, &addr);

	if ((buf = buf_open(len + MRT_HEADER_SIZE)) == NULL) {
		log_warnx("mrt_dump_entry: buf_open error");
		return (-1);
	}

	if (mrt_dump_header(buf, MSG_TABLE_DUMP, AFI_IPv4, len) == -1) {
		log_warnx("mrt_dump_entry: buf_add error");
		return (-1);
	}

	DUMP_SHORT(buf, 0);
	DUMP_SHORT(buf, snum);
	DUMP_NLONG(buf, addr.v4.s_addr);
	DUMP_BYTE(buf, p->prefix->prefixlen);
	DUMP_BYTE(buf, 1);		/* state */
	DUMP_LONG(buf, p->lastchange);	/* originated */
	DUMP_NLONG(buf, peer->remote_addr.v4.s_addr);
	DUMP_SHORT(buf, peer->conf.remote_as);

	DUMP_SHORT(buf, attr_len);
	if ((bptr = buf_reserve(buf, attr_len)) == NULL) {
		log_warnx("mrt_dump_entry: buf_reserve error");
		buf_free(buf);
		return (-1);
	}

	if (p->aspath->nexthop == NULL) {
		bzero(&addr, sizeof(struct bgpd_addr));
		addr.af = AF_INET;
		nh = &addr;
	} else
		nh = &p->aspath->nexthop->exit_nexthop;
	if (mrt_attr_dump(bptr, attr_len, p->aspath, nh) == -1) {
		log_warnx("mrt_dump_entry: mrt_attr_dump error");
		buf_free(buf);
		return (-1);
	}

	TAILQ_INSERT_TAIL(&mrt->bufs, buf, entry);
	mrt->queued++;

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
	struct mrt		*mrtbuf = ptr;
	struct prefix		*p;

	/*
	 * dump all prefixes even the inactive ones. That is the way zebra
	 * dumps the table so we do the same. If only the active route should
	 * be dumped p should be set to p = pt->active.
	 */
	LIST_FOREACH(p, &pt->prefix_h, prefix_l)
		if (mrtbuf->type == MRT_TABLE_DUMP)
			mrt_dump_entry(mrtbuf, p, sequencenum++,
			    p->aspath->peer);
		else
			mrt_dump_entry_mp(mrtbuf, p, sequencenum++,
			    p->aspath->peer);
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

int
mrt_write(struct mrt *mrt)
{
	struct buf	*b;
	int		 r = 0;

	while ((b = TAILQ_FIRST(&mrt->bufs)) &&
	    (r = buf_write(mrt->fd, b)) == 1) {
		TAILQ_REMOVE(&mrt->bufs, b, entry);
		mrt->queued--;
		buf_free(b);
	}
	if (r <= -1) {
		log_warn("mrt dump write");
		mrt_clean(mrt);
		return (-1);
	}
	return (0);
}

void
mrt_clean(struct mrt *mrt)
{
	struct buf	*b;

	close(mrt->fd);
	while ((b = TAILQ_FIRST(&mrt->bufs))) {
		TAILQ_REMOVE(&mrt->bufs, b, entry);
		buf_free(b);
	}
	mrt->queued = 0;
}

static struct imsgbuf	*mrt_imsgbuf[2];

void
mrt_init(struct imsgbuf *rde, struct imsgbuf *se)
{
	mrt_imsgbuf[0] = rde;
	mrt_imsgbuf[1] = se;
}

int
mrt_open(struct mrt *mrt, time_t now)
{
	enum imsg_type	type;
	int		i;

	if (strftime(MRT2MC(mrt)->file, sizeof(MRT2MC(mrt)->file),
	    MRT2MC(mrt)->name, localtime(&now)) == 0) {
		log_warnx("mrt_open: strftime conversion failed");
		mrt->fd = -1;
		return (-1);
	}

	mrt->fd = open(MRT2MC(mrt)->file,
	    O_WRONLY|O_NONBLOCK|O_CREAT|O_TRUNC, 0644);
	if (mrt->fd == -1) {
		log_warn("mrt_open %s", MRT2MC(mrt)->file);
		return (1);
	}

	if (MRT2MC(mrt)->state == MRT_STATE_OPEN)
		type = IMSG_MRT_OPEN;
	else
		type = IMSG_MRT_REOPEN;

	i = mrt->type == MRT_TABLE_DUMP ? 0 : 1;

	if (imsg_compose(mrt_imsgbuf[i], type, 0, 0, mrt->fd,
	    mrt, sizeof(struct mrt)) == -1)
		log_warn("mrt_open");

	return (1);
}

int
mrt_timeout(struct mrt_head *mrt)
{
	struct mrt	*m;
	time_t		 now;
	int		 timeout = MRT_MAX_TIMEOUT;

	now = time(NULL);
	LIST_FOREACH(m, mrt, entry) {
		if (MRT2MC(m)->state == MRT_STATE_RUNNING &&
		    MRT2MC(m)->ReopenTimerInterval != 0) {
			if (MRT2MC(m)->ReopenTimer <= now) {
				mrt_open(m, now);
				MRT2MC(m)->ReopenTimer =
				    now + MRT2MC(m)->ReopenTimerInterval;
			}
			if (MRT2MC(m)->ReopenTimer - now < timeout)
				timeout = MRT2MC(m)->ReopenTimer - now;
		}
	}
	return (timeout > 0 ? timeout : 0);
}

void
mrt_reconfigure(struct mrt_head *mrt)
{
	struct mrt	*m, *xm;
	time_t		 now;

	now = time(NULL);
	for (m = LIST_FIRST(mrt); m != NULL; m = xm) {
		xm = LIST_NEXT(m, entry);
		if (MRT2MC(m)->state == MRT_STATE_OPEN ||
		    MRT2MC(m)->state == MRT_STATE_REOPEN) {
			if (mrt_open(m, now) == -1)
				continue;
			if (MRT2MC(m)->ReopenTimerInterval != 0)
				MRT2MC(m)->ReopenTimer =
				    now + MRT2MC(m)->ReopenTimerInterval;
			MRT2MC(m)->state = MRT_STATE_RUNNING;
		}
		if (MRT2MC(m)->state == MRT_STATE_REMOVE) {
			LIST_REMOVE(m, entry);
			free(m);
			continue;
		}
	}
}

void
mrt_handler(struct mrt_head *mrt)
{
	struct mrt	*m;
	time_t		 now;

	now = time(NULL);
	LIST_FOREACH(m, mrt, entry) {
		if (MRT2MC(m)->state == MRT_STATE_RUNNING &&
		    (MRT2MC(m)->ReopenTimerInterval != 0 ||
		     m->type == MRT_TABLE_DUMP)) {
			if (mrt_open(m, now) == -1)
				continue;
			MRT2MC(m)->ReopenTimer =
			    now + MRT2MC(m)->ReopenTimerInterval;
		}
	}
}

struct mrt *
mrt_get(struct mrt_head *c, struct mrt *m)
{
	struct mrt	*t;

	LIST_FOREACH(t, c, entry) {
		if (t->type != m->type)
			continue;
		if (t->type == MRT_TABLE_DUMP)
			return (t);
		if (t->peer_id == m->peer_id &&
		    t->group_id == m->group_id)
			return (t);
	}
	return (NULL);
}

int
mrt_mergeconfig(struct mrt_head *xconf, struct mrt_head *nconf)
{
	struct mrt	*m, *xm;

	LIST_FOREACH(m, nconf, entry) {
		if ((xm = mrt_get(xconf, m)) == NULL) {
			/* NEW */
			if ((xm = calloc(1, sizeof(struct mrt_config))) == NULL)
				fatal("mrt_mergeconfig");
			memcpy(xm, m, sizeof(struct mrt_config));
			xm->fd = -1;
			MRT2MC(xm)->state = MRT_STATE_OPEN;
			LIST_INSERT_HEAD(xconf, xm, entry);
		} else {
			/* MERGE */
			if (strlcpy(MRT2MC(xm)->name, MRT2MC(m)->name,
			    sizeof(MRT2MC(xm)->name)) >=
			    sizeof(MRT2MC(xm)->name))
				fatalx("mrt_mergeconfig: strlcpy");
			MRT2MC(xm)->ReopenTimerInterval =
			    MRT2MC(m)->ReopenTimerInterval;
			MRT2MC(xm)->state = MRT_STATE_REOPEN;
		}
	}

	LIST_FOREACH(xm, xconf, entry)
		if (mrt_get(nconf, xm) == NULL)
			/* REMOVE */
			MRT2MC(xm)->state = MRT_STATE_REMOVE;

	/* free config */
	while ((m = LIST_FIRST(nconf)) != NULL) {
		LIST_REMOVE(m, entry);
		free(m);
	}

	return (0);
}

