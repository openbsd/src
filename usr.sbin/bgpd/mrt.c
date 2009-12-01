/*	$OpenBSD: mrt.c,v 1.66 2009/12/01 14:28:05 claudio Exp $ */

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

int mrt_attr_dump(struct buf *, struct rde_aspath *, struct bgpd_addr *);
int mrt_dump_entry_mp(struct mrt *, struct prefix *, u_int16_t,
    struct rde_peer*);
int mrt_dump_entry(struct mrt *, struct prefix *, u_int16_t, struct rde_peer*);
int mrt_dump_hdr_se(struct buf **, struct peer *, u_int16_t, u_int16_t,
    u_int32_t, int);
int mrt_dump_hdr_rde(struct buf **, u_int16_t type, u_int16_t, u_int32_t);
int mrt_open(struct mrt *, time_t);

#define DUMP_BYTE(x, b)							\
	do {								\
		u_char		t = (b);				\
		if (buf_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump1: buf_add error");		\
			goto fail;					\
		}							\
	} while (0)

#define DUMP_SHORT(x, s)						\
	do {								\
		u_int16_t	t;					\
		t = htons((s));						\
		if (buf_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump2: buf_add error");		\
			goto fail;					\
		}							\
	} while (0)

#define DUMP_LONG(x, l)							\
	do {								\
		u_int32_t	t;					\
		t = htonl((l));						\
		if (buf_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump3: buf_add error");		\
			goto fail;					\
		}							\
	} while (0)

#define DUMP_NLONG(x, l)						\
	do {								\
		u_int32_t	t = (l);				\
		if (buf_add((x), &t, sizeof(t)) == -1) {		\
			log_warnx("mrt_dump4: buf_add error");		\
			goto fail;					\
		}							\
	} while (0)

void
mrt_dump_bgp_msg(struct mrt *mrt, void *pkg, u_int16_t pkglen,
    struct peer *peer)
{
	struct buf	*buf;
	int		 incoming = 0;

	/* get the direction of the message to swap address and AS fields */
	if (mrt->type == MRT_ALL_IN || mrt->type == MRT_UPDATE_IN)
		incoming = 1;

	if (mrt_dump_hdr_se(&buf, peer, MSG_PROTOCOL_BGP4MP, BGP4MP_MESSAGE,
	    pkglen, incoming) == -1)
		return;

	if (buf_add(buf, pkg, pkglen) == -1) {
		log_warnx("mrt_dump_bgp_msg: buf_add error");
		buf_free(buf);
		return;
	}

	buf_close(&mrt->wbuf, buf);
}

void
mrt_dump_state(struct mrt *mrt, u_int16_t old_state, u_int16_t new_state,
    struct peer *peer)
{
	struct buf	*buf;

	if (mrt_dump_hdr_se(&buf, peer, MSG_PROTOCOL_BGP4MP, BGP4MP_MESSAGE,
	    2 * sizeof(short), 0) == -1)
		return;

	DUMP_SHORT(buf, old_state);
	DUMP_SHORT(buf, new_state);

	buf_close(&mrt->wbuf, buf);
	return;

fail:
	buf_free(buf);
}

int
mrt_attr_dump(struct buf *buf, struct rde_aspath *a, struct bgpd_addr *nexthop)
{
	struct attr	*oa;
	u_char		*pdata;
	u_int32_t	 tmp;
	int		 neednewpath = 0;
	u_int16_t	 plen;
	u_int8_t	 l;

	/* origin */
	if (attr_writebuf(buf, ATTR_WELL_KNOWN, ATTR_ORIGIN,
	    &a->origin, 1) == -1)
		return (-1);

	/* aspath */
	pdata = aspath_prepend(a->aspath, rde_local_as(), 0, &plen);
	pdata = aspath_deflate(pdata, &plen, &neednewpath);
	if (attr_writebuf(buf, ATTR_WELL_KNOWN, ATTR_ASPATH, pdata, plen) == -1)
		return (-1);
	free(pdata);

	if (nexthop) {
		/* nexthop, already network byte order */
		if (attr_writebuf(buf, ATTR_WELL_KNOWN, ATTR_NEXTHOP,
		    &nexthop->v4.s_addr, 4) ==	-1)
			return (-1);
	}

	/* MED, non transitive */
	if (a->med != 0) {
		tmp = htonl(a->med);
		if (attr_writebuf(buf, ATTR_OPTIONAL, ATTR_MED, &tmp, 4) == -1)
			return (-1);
	}

	/* local preference, only valid for ibgp */
	tmp = htonl(a->lpref);
	if (attr_writebuf(buf, ATTR_WELL_KNOWN, ATTR_LOCALPREF, &tmp, 4) == -1)
		return (-1);

	/* dump all other path attributes without modification */
	for (l = 0; l < a->others_len; l++) {
		if ((oa = a->others[l]) == NULL)
			break;
		if (attr_writebuf(buf, oa->flags, oa->type,
		    oa->data, oa->len) == -1)
			return (-1);
	}

	if (neednewpath) {
		pdata = aspath_prepend(a->aspath, rde_local_as(), 0, &plen);
		if (plen != 0)
			if (attr_writebuf(buf, ATTR_OPTIONAL|ATTR_TRANSITIVE,
			    ATTR_AS4_PATH, pdata, plen) == -1)
				return (-1);
		free(pdata);
	}

	return (0);
}

int
mrt_dump_entry_mp(struct mrt *mrt, struct prefix *p, u_int16_t snum,
    struct rde_peer *peer)
{
	struct buf	*buf, *hbuf = NULL, *h2buf = NULL;
	void		*bptr;
	struct bgpd_addr addr, nexthop, *nh;
	u_int16_t	 len;
	u_int8_t	 p_len;
	u_int8_t	 aid;

	if ((buf = buf_dynamic(0, MAX_PKTSIZE)) == NULL) {
		log_warn("mrt_dump_entry_mp: buf_dynamic");
		return (-1);
	}

	if (mrt_attr_dump(buf, p->aspath, NULL) == -1) {
		log_warnx("mrt_dump_entry_mp: mrt_attr_dump error");
		goto fail;
	}
	len = buf_size(buf);

	if ((h2buf = buf_dynamic(MRT_BGP4MP_IPv4_HEADER_SIZE +
	    MRT_BGP4MP_IPv4_ENTRY_SIZE, MRT_BGP4MP_IPv6_HEADER_SIZE +
	    MRT_BGP4MP_IPv6_ENTRY_SIZE + MRT_BGP4MP_MAX_PREFIXLEN)) == NULL) {
		log_warn("mrt_dump_entry_mp: buf_dynamic");
		goto fail;
	}

	DUMP_SHORT(h2buf, rde_local_as());
	DUMP_SHORT(h2buf, peer->short_as);
	DUMP_SHORT(h2buf, /* ifindex */ 0);

	/* XXX is this for peer self? */
	aid = peer->remote_addr.aid == AID_UNSPEC ? p->prefix->aid :
	     peer->remote_addr.aid;
	switch (aid) {
	case AID_INET:
		DUMP_SHORT(h2buf, AFI_IPv4);
		DUMP_NLONG(h2buf, peer->local_v4_addr.v4.s_addr);
		DUMP_NLONG(h2buf, peer->remote_addr.v4.s_addr);
		break;
	case AID_INET6:
		DUMP_SHORT(h2buf, AFI_IPv6);
		if (buf_add(h2buf, &peer->local_v6_addr.v6,
		    sizeof(struct in6_addr)) == -1 ||
		    buf_add(h2buf, &peer->remote_addr.v6,
		    sizeof(struct in6_addr)) == -1) {
			log_warnx("mrt_dump_entry_mp: buf_add error");
			goto fail;
		}
		break;
	default:
		log_warnx("king bula found new AF in mrt_dump_entry_mp");
		goto fail;
	}

	DUMP_SHORT(h2buf, 0);		/* view */
	DUMP_SHORT(h2buf, 1);		/* status */
	DUMP_LONG(h2buf, p->lastchange);	/* originated */

	if (p->aspath->nexthop == NULL) {
		bzero(&nexthop, sizeof(struct bgpd_addr));
		nexthop.aid = addr.aid;
		nh = &nexthop;
	} else
		nh = &p->aspath->nexthop->exit_nexthop;

	pt_getaddr(p->prefix, &addr);
	switch (addr.aid) {
	case AID_INET:
		DUMP_SHORT(h2buf, AFI_IPv4);	/* afi */
		DUMP_BYTE(h2buf, SAFI_UNICAST);	/* safi */
		DUMP_BYTE(h2buf, 4);		/* nhlen */
		DUMP_NLONG(h2buf, nh->v4.s_addr);	/* nexthop */
		break;
	case AID_INET6:
		DUMP_SHORT(h2buf, AFI_IPv6);	/* afi */
		DUMP_BYTE(h2buf, SAFI_UNICAST);	/* safi */
		DUMP_BYTE(h2buf, 16);		/* nhlen */
		if (buf_add(h2buf, &nh->v6, sizeof(struct in6_addr)) == -1) {
			log_warnx("mrt_dump_entry_mp: buf_add error");
			goto fail;
		}
		break;
	default:
		log_warnx("king bula found new AF in mrt_dump_entry_mp");
		goto fail;
	}

	p_len = PREFIX_SIZE(p->prefix->prefixlen);
	if ((bptr = buf_reserve(h2buf, p_len)) == NULL) {
		log_warnx("mrt_dump_entry_mp: buf_reserve error");
		goto fail;
	}
	if (prefix_write(bptr, p_len, &addr, p->prefix->prefixlen) == -1) {
		log_warnx("mrt_dump_entry_mp: prefix_write error");
		goto fail;
	}

	DUMP_SHORT(h2buf, len);
	len += buf_size(h2buf);

	if (mrt_dump_hdr_rde(&hbuf, MSG_PROTOCOL_BGP4MP, BGP4MP_ENTRY,
	    len) == -1)
		goto fail;

	buf_close(&mrt->wbuf, hbuf);
	buf_close(&mrt->wbuf, h2buf);
	buf_close(&mrt->wbuf, buf);

	return (len + MRT_HEADER_SIZE);

fail:
	if (hbuf)
		buf_free(hbuf);
	if (h2buf)
		buf_free(h2buf);
	buf_free(buf);
	return (-1);
}

int
mrt_dump_entry(struct mrt *mrt, struct prefix *p, u_int16_t snum,
    struct rde_peer *peer)
{
	struct buf	*buf, *hbuf;
	struct bgpd_addr addr, *nh;
	size_t		 len;

	if (p->prefix->aid != AID_INET &&
	    peer->remote_addr.aid == AID_INET)
		/* only able to dump IPv4 */
		return (0);

	if ((buf = buf_dynamic(0, MAX_PKTSIZE)) == NULL) {
		log_warnx("mrt_dump_entry: buf_dynamic");
		return (-1);
	}

	if (p->aspath->nexthop == NULL) {
		bzero(&addr, sizeof(struct bgpd_addr));
		addr.aid = AID_INET;
		nh = &addr;
	} else
		nh = &p->aspath->nexthop->exit_nexthop;
	if (mrt_attr_dump(buf, p->aspath, nh) == -1) {
		log_warnx("mrt_dump_entry: mrt_attr_dump error");
		buf_free(buf);
		return (-1);
	}
	len = buf_size(buf);

	if (mrt_dump_hdr_rde(&hbuf, MSG_TABLE_DUMP, AFI_IPv4, len) == -1) {
		buf_free(buf);
		return (-1);
	}

	DUMP_SHORT(hbuf, 0);
	DUMP_SHORT(hbuf, snum);

	pt_getaddr(p->prefix, &addr);
	DUMP_NLONG(hbuf, addr.v4.s_addr);
	DUMP_BYTE(hbuf, p->prefix->prefixlen);

	DUMP_BYTE(hbuf, 1);		/* state */
	DUMP_LONG(hbuf, p->lastchange);	/* originated */
	DUMP_NLONG(hbuf, peer->remote_addr.v4.s_addr);
	DUMP_SHORT(hbuf, peer->short_as);
	DUMP_SHORT(hbuf, len);

	buf_close(&mrt->wbuf, hbuf);
	buf_close(&mrt->wbuf, buf);

	return (len + MRT_HEADER_SIZE);

fail:
	buf_free(hbuf);
	buf_free(buf);
	return (-1);
}

void
mrt_dump_upcall(struct rib_entry *re, void *ptr)
{
	struct mrt		*mrtbuf = ptr;
	struct prefix		*p;

	/*
	 * dump all prefixes even the inactive ones. That is the way zebra
	 * dumps the table so we do the same. If only the active route should
	 * be dumped p should be set to p = pt->active.
	 */
	LIST_FOREACH(p, &re->prefix_h, rib_l) {
		if (mrtbuf->type == MRT_TABLE_DUMP)
			mrt_dump_entry(mrtbuf, p, mrtbuf->seqnum++,
			    p->aspath->peer);
		else
			mrt_dump_entry_mp(mrtbuf, p, mrtbuf->seqnum++,
			    p->aspath->peer);
	}
}

void
mrt_done(void *ptr)
{
	struct mrt		*mrtbuf = ptr;

	mrtbuf->state = MRT_STATE_REMOVE;
}

int
mrt_dump_hdr_se(struct buf ** bp, struct peer *peer, u_int16_t type,
    u_int16_t subtype, u_int32_t len, int swap)
{
	time_t	 	now;

	if ((*bp = buf_dynamic(MRT_HEADER_SIZE, MRT_HEADER_SIZE +
	    MRT_BGP4MP_AS4_IPv6_HEADER_SIZE + len)) == NULL) {
		log_warnx("mrt_dump_hdr_se: buf_open error");
		return (-1);
	}

	now = time(NULL);

	DUMP_LONG(*bp, now);
	DUMP_SHORT(*bp, type);
	DUMP_SHORT(*bp, subtype);

	switch (peer->sa_local.ss_family) {
	case AF_INET:
		if (subtype == BGP4MP_STATE_CHANGE_AS4 ||
		    subtype == BGP4MP_MESSAGE_AS4)
			len += MRT_BGP4MP_AS4_IPv4_HEADER_SIZE;
		else
			len += MRT_BGP4MP_IPv4_HEADER_SIZE;
		break;
	case AF_INET6:
		if (subtype == BGP4MP_STATE_CHANGE_AS4 ||
		    subtype == BGP4MP_MESSAGE_AS4)
			len += MRT_BGP4MP_AS4_IPv6_HEADER_SIZE;
		else
			len += MRT_BGP4MP_IPv6_HEADER_SIZE;
		break;
	case 0:
		goto fail;
	default:
		log_warnx("king bula found new AF in mrt_dump_hdr_se");
		goto fail;
	}

	DUMP_LONG(*bp, len);

	if (subtype == BGP4MP_STATE_CHANGE_AS4 ||
	    subtype == BGP4MP_MESSAGE_AS4) {
		if (!swap)
			DUMP_LONG(*bp, peer->conf.local_as);
		DUMP_LONG(*bp, peer->conf.remote_as);
		if (swap)
			DUMP_LONG(*bp, peer->conf.local_as);
	} else {
		if (!swap)
			DUMP_SHORT(*bp, peer->conf.local_short_as);
		DUMP_SHORT(*bp, peer->short_as);
		if (swap)
			DUMP_SHORT(*bp, peer->conf.local_short_as);
	}

	DUMP_SHORT(*bp, /* ifindex */ 0);

	switch (peer->sa_local.ss_family) {
	case AF_INET:
		DUMP_SHORT(*bp, AFI_IPv4);
		if (!swap)
			DUMP_NLONG(*bp, ((struct sockaddr_in *)
			    &peer->sa_local)->sin_addr.s_addr);
		DUMP_NLONG(*bp,
		    ((struct sockaddr_in *)&peer->sa_remote)->sin_addr.s_addr);
		if (swap)
			DUMP_NLONG(*bp, ((struct sockaddr_in *)
			    &peer->sa_local)->sin_addr.s_addr);
		break;
	case AF_INET6:
		DUMP_SHORT(*bp, AFI_IPv6);
		if (!swap)
			if (buf_add(*bp, &((struct sockaddr_in6 *)
			    &peer->sa_local)->sin6_addr,
			    sizeof(struct in6_addr)) == -1) {
				log_warnx("mrt_dump_hdr_se: buf_add error");
				goto fail;
			}
		if (buf_add(*bp,
		    &((struct sockaddr_in6 *)&peer->sa_remote)->sin6_addr,
		    sizeof(struct in6_addr)) == -1) {
			log_warnx("mrt_dump_hdr_se: buf_add error");
			goto fail;
		}
		if (swap)
			if (buf_add(*bp, &((struct sockaddr_in6 *)
			    &peer->sa_local)->sin6_addr,
			    sizeof(struct in6_addr)) == -1) {
				log_warnx("mrt_dump_hdr_se: buf_add error");
				goto fail;
			}
		break;
	}

	return (0);

fail:
	buf_free(*bp);
	return (-1);
}

int
mrt_dump_hdr_rde(struct buf **bp, u_int16_t type, u_int16_t subtype,
    u_int32_t len)
{
	time_t		 now;

	if ((*bp = buf_dynamic(MRT_HEADER_SIZE, MRT_HEADER_SIZE +
	    MRT_BGP4MP_AS4_IPv6_HEADER_SIZE + MRT_BGP4MP_IPv6_ENTRY_SIZE)) ==
	    NULL) {
		log_warnx("mrt_dump_hdr_rde: buf_dynamic error");
		return (-1);
	}

	now = time(NULL);
	DUMP_LONG(*bp, now);
	DUMP_SHORT(*bp, type);
	DUMP_SHORT(*bp, subtype);

	switch (type) {
	case MSG_TABLE_DUMP:
		DUMP_LONG(*bp, MRT_DUMP_HEADER_SIZE + len);
		break;
	case MSG_PROTOCOL_BGP4MP:
		DUMP_LONG(*bp, len);
		break;
	default:
		log_warnx("mrt_dump_hdr_rde: unsupported type");
		goto fail;
	}	
	return (0);

fail:
	buf_free(*bp);
	return (-1);
}

void
mrt_write(struct mrt *mrt)
{
	int	r;

	if ((r = buf_write(&mrt->wbuf)) < 0) {
		log_warn("mrt dump aborted, mrt_write");
		mrt_clean(mrt);
		mrt_done(mrt);
	}
}

void
mrt_clean(struct mrt *mrt)
{
	struct buf	*b;

	close(mrt->wbuf.fd);
	while ((b = TAILQ_FIRST(&mrt->wbuf.bufs))) {
		TAILQ_REMOVE(&mrt->wbuf.bufs, b, entry);
		buf_free(b);
	}
	mrt->wbuf.queued = 0;
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
	int		i = 1, fd;

	if (strftime(MRT2MC(mrt)->file, sizeof(MRT2MC(mrt)->file),
	    MRT2MC(mrt)->name, localtime(&now)) == 0) {
		log_warnx("mrt_open: strftime conversion failed");
		return (-1);
	}

	fd = open(MRT2MC(mrt)->file,
	    O_WRONLY|O_NONBLOCK|O_CREAT|O_TRUNC, 0644);
	if (fd == -1) {
		log_warn("mrt_open %s", MRT2MC(mrt)->file);
		return (1);
	}

	if (mrt->state == MRT_STATE_OPEN)
		type = IMSG_MRT_OPEN;
	else
		type = IMSG_MRT_REOPEN;

	if (mrt->type == MRT_TABLE_DUMP || mrt->type == MRT_TABLE_DUMP_MP)
		i = 0;

	if (imsg_compose(mrt_imsgbuf[i], type, 0, 0, fd,
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
		if (m->state == MRT_STATE_RUNNING &&
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
		if (m->state == MRT_STATE_OPEN ||
		    m->state == MRT_STATE_REOPEN) {
			if (mrt_open(m, now) == -1)
				continue;
			if (MRT2MC(m)->ReopenTimerInterval != 0)
				MRT2MC(m)->ReopenTimer =
				    now + MRT2MC(m)->ReopenTimerInterval;
			m->state = MRT_STATE_RUNNING;
		}
		if (m->state == MRT_STATE_REMOVE) {
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
		if (m->state == MRT_STATE_RUNNING &&
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
		if (strcmp(t->rib, m->rib))
			continue;
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
			xm->state = MRT_STATE_OPEN;
			LIST_INSERT_HEAD(xconf, xm, entry);
		} else {
			/* MERGE */
			if (strlcpy(MRT2MC(xm)->name, MRT2MC(m)->name,
			    sizeof(MRT2MC(xm)->name)) >=
			    sizeof(MRT2MC(xm)->name))
				fatalx("mrt_mergeconfig: strlcpy");
			MRT2MC(xm)->ReopenTimerInterval =
			    MRT2MC(m)->ReopenTimerInterval;
			xm->state = MRT_STATE_REOPEN;
		}
	}

	LIST_FOREACH(xm, xconf, entry)
		if (mrt_get(nconf, xm) == NULL)
			/* REMOVE */
			xm->state = MRT_STATE_REMOVE;

	/* free config */
	while ((m = LIST_FIRST(nconf)) != NULL) {
		LIST_REMOVE(m, entry);
		free(m);
	}

	return (0);
}
