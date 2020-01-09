/*	$OpenBSD: rde_peer.c,v 1.2 2020/01/09 13:31:52 claudio Exp $ */

/*
 * Copyright (c) 2019 Claudio Jeker <claudio@openbsd.org>
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
#include <sys/socket.h>

#include <ifaddrs.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "rde.h"

struct peer_table {
	struct rde_peer_head	*peer_hashtbl;
	u_int32_t		 peer_hashmask;
} peertable;

#define PEER_HASH(x)		\
	&peertable.peer_hashtbl[(x) & peertable.peer_hashmask]

struct rde_peer_head	 peerlist;
struct rde_peer		*peerself;

struct iq {
	SIMPLEQ_ENTRY(iq)	entry;
	struct imsg		imsg;
};

extern struct filter_head      *out_rules;

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
	pc.id = PEER_ID_SELF;

	peerself = peer_add(PEER_ID_SELF, &pc);
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

/*
 * Traverse all peers calling callback for each peer.
 */
void
peer_foreach(void (*callback)(struct rde_peer *, void *), void *arg)
{
	struct rde_peer *peer, *np;

	LIST_FOREACH_SAFE(peer,  &peerlist, peer_l, np)
		callback(peer, arg);
}

/*
 * Lookup a peer by peer_id, return NULL if not found.
 */
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

/*
 * Find next peer that matches neighbor options in *n.
 * If peerid was set then pickup the lookup after that peer.
 * Returns NULL if no more peers match.
 */
struct rde_peer *
peer_match(struct ctl_neighbor *n, u_int32_t peerid)
{
	struct rde_peer_head	*head;
	struct rde_peer		*peer;
	u_int32_t		i = 0;

	if (peerid != 0)
		i = peerid & peertable.peer_hashmask;

	while (i <= peertable.peer_hashmask) {
		head = &peertable.peer_hashtbl[i];
		LIST_FOREACH(peer, head, hash_l) {
			/* skip peers until peerid is found */
			if (peerid == peer->conf.id) {
				peerid = 0;
				continue;
			}
			if (peerid != 0)
				continue;

			if (rde_match_peer(peer, n))
				return (peer);
		}
		i++;
	}
	return (NULL);
}

struct rde_peer *
peer_add(u_int32_t id, struct peer_config *p_conf)
{
	struct rde_peer_head	*head;
	struct rde_peer		*peer;

	if ((peer = peer_get(id))) {
		memcpy(&peer->conf, p_conf, sizeof(struct peer_config));
		return (NULL);
	}

	peer = calloc(1, sizeof(struct rde_peer));
	if (peer == NULL)
		fatal("peer_add");

	memcpy(&peer->conf, p_conf, sizeof(struct peer_config));
	peer->remote_bgpid = 0;
	peer->loc_rib_id = rib_find(peer->conf.rib);
	if (peer->loc_rib_id == RIB_NOTFOUND)
		fatalx("King Bula's new peer met an unknown RIB");
	peer->state = PEER_NONE;
	SIMPLEQ_INIT(&peer->imsg_queue);

	head = PEER_HASH(id);

	LIST_INSERT_HEAD(head, peer, hash_l);
	LIST_INSERT_HEAD(&peerlist, peer, peer_l);

	return (peer);
}

/*
 * Various RIB walker callbacks.
 */
static void
peer_adjout_clear_upcall(struct prefix *p, void *arg)
{
	prefix_adjout_destroy(p);
}

static void
peer_adjout_stale_upcall(struct prefix *p, void *arg)
{
	if (p->flags & PREFIX_FLAG_DEAD) {
		return;
	} else if (p->flags & PREFIX_FLAG_WITHDRAW) {
		/* no need to keep stale withdraws, they miss all attributes */
		prefix_adjout_destroy(p);
		return;
	} else if (p->flags & PREFIX_FLAG_UPDATE) {
		RB_REMOVE(prefix_tree, &prefix_peer(p)->updates[p->pt->aid], p);
		p->flags &= ~PREFIX_FLAG_UPDATE;
	}
	p->flags |= PREFIX_FLAG_STALE;
}

struct peer_flush {
	struct rde_peer *peer;
	time_t		 staletime;
};

static void
peer_flush_upcall(struct rib_entry *re, void *arg)
{
	struct rde_peer *peer = ((struct peer_flush *)arg)->peer;
	struct rde_aspath *asp;
	struct bgpd_addr addr;
	struct prefix *p, *np, *rp;
	time_t staletime = ((struct peer_flush *)arg)->staletime;
	u_int32_t i;
	u_int8_t prefixlen;

	pt_getaddr(re->prefix, &addr);
	prefixlen = re->prefix->prefixlen;
	LIST_FOREACH_SAFE(p, &re->prefix_h, entry.list.rib, np) {
		if (peer != prefix_peer(p))
			continue;
		if (staletime && p->lastchange > staletime)
			continue;

		for (i = RIB_LOC_START; i < rib_size; i++) {
			struct rib *rib = rib_byid(i);
			if (rib == NULL)
				continue;
			rp = prefix_get(rib, peer, &addr, prefixlen);
			if (rp) {
				asp = prefix_aspath(rp);
				if (asp->pftableid)
					rde_send_pftable(asp->pftableid, &addr,
					    prefixlen, 1);

				prefix_destroy(rp);
				rde_update_log("flush", i, peer, NULL,
				    &addr, prefixlen);
			}
		}

		prefix_destroy(p);
		peer->prefix_cnt--;
		break;	/* optimization, only one match per peer possible */
	}
}

static void
rde_up_adjout_force_upcall(struct prefix *p, void *ptr)
{
	if (p->flags & PREFIX_FLAG_STALE) {
		/* remove stale entries */
		prefix_adjout_destroy(p);
	} else if (p->flags & PREFIX_FLAG_DEAD) {
		/* ignore dead prefixes, they will go away soon */
	} else if ((p->flags & PREFIX_FLAG_MASK) == 0) {
		/* put entries on the update queue if not allready on a queue */
		p->flags |= PREFIX_FLAG_UPDATE;
		if (RB_INSERT(prefix_tree, &prefix_peer(p)->updates[p->pt->aid],
		    p) != NULL)
			fatalx("%s: RB tree invariant violated", __func__);
	}
}

static void
rde_up_adjout_force_done(void *ptr, u_int8_t aid)
{
	struct rde_peer		*peer = ptr;

	/* Adj-RIB-Out ready, unthrottle peer and inject EOR */
	peer->throttled = 0;
	if (peer->capa.grestart.restart)
		prefix_add_eor(peer, aid);
}

static void
rde_up_dump_upcall(struct rib_entry *re, void *ptr)
{
	struct rde_peer		*peer = ptr;

	if (re->rib_id != peer->loc_rib_id)
		fatalx("%s: Unexpected RIB %u != %u.", __func__, re->rib_id,
		    peer->loc_rib_id);
	if (re->active == NULL)
		return;
	up_generate_updates(out_rules, peer, re->active, NULL);
}

static void
rde_up_dump_done(void *ptr, u_int8_t aid)
{
	struct rde_peer		*peer = ptr;

	/* force out all updates of Adj-RIB-Out for this peer */
	if (prefix_dump_new(peer, aid, 0, peer, rde_up_adjout_force_upcall,
	    rde_up_adjout_force_done, NULL) == -1)
		fatal("%s: prefix_dump_new", __func__);
}

static int
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

/*
 * Figure out the local IP addresses most suitable for this session.
 * This looks up the local address of other address family based on
 * the address of the TCP session.
 */
static int
peer_localaddrs(struct rde_peer *peer, struct bgpd_addr *laddr)
{
	struct ifaddrs	*ifap, *ifa, *match;

	if (getifaddrs(&ifap) == -1)
		fatal("getifaddrs");

	for (match = ifap; match != NULL; match = match->ifa_next)
		if (sa_cmp(laddr, match->ifa_addr) == 0)
			break;

	if (match == NULL) {
		log_warnx("peer_localaddrs: local address not found");
		return (-1);
	}

	for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
		if (ifa->ifa_addr->sa_family == AF_INET &&
		    strcmp(ifa->ifa_name, match->ifa_name) == 0) {
			if (ifa->ifa_addr->sa_family ==
			    match->ifa_addr->sa_family)
				ifa = match;
			sa2addr(ifa->ifa_addr, &peer->local_v4_addr, NULL);
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
			sa2addr(ifa->ifa_addr, &peer->local_v6_addr, NULL);
			break;
		}
	}

	freeifaddrs(ifap);
	return (0);
}

/*
 * Session got established, bring peer up, load RIBs do initial table dump.
 */
int
peer_up(struct rde_peer *peer, struct session_up *sup)
{
	u_int8_t	 i;

	if (peer->state == PEER_ERR) {
		/*
		 * There is a race condition when doing PEER_ERR -> PEER_DOWN.
		 * So just do a full reset of the peer here.
		 */
		if (prefix_dump_new(peer, AID_UNSPEC, 0, NULL,
		    peer_adjout_clear_upcall, NULL, NULL) == -1)
			fatal("%s: prefix_dump_new", __func__);
		peer_flush(peer, AID_UNSPEC, 0);
		peer->prefix_cnt = 0;
		peer->state = PEER_DOWN;
	}
	peer->remote_bgpid = ntohl(sup->remote_bgpid);
	peer->short_as = sup->short_as;
	memcpy(&peer->remote_addr, &sup->remote_addr,
	    sizeof(peer->remote_addr));
	memcpy(&peer->capa, &sup->capa, sizeof(peer->capa));

	if (peer_localaddrs(peer, &sup->local_addr))
		return (-1);

	peer->state = PEER_UP;

	for (i = 0; i < AID_MAX; i++) {
		if (peer->capa.mp[i])
			peer_dump(peer, i);
	}

	return (0);
}

/*
 * Session dropped and no graceful restart is done. Stop everything for
 * this peer and clean up.
 */
void
peer_down(struct rde_peer *peer, void *bula)
{
	peer->remote_bgpid = 0;
	peer->state = PEER_DOWN;
	/* stop all pending dumps which may depend on this peer */
	rib_dump_terminate(peer);

	/* flush Adj-RIB-Out */
	if (prefix_dump_new(peer, AID_UNSPEC, 0, NULL,
	    peer_adjout_clear_upcall, NULL, NULL) == -1)
		fatal("%s: prefix_dump_new", __func__);

	/* flush Adj-RIB-In */
	peer_flush(peer, AID_UNSPEC, 0);
	peer->prefix_cnt = 0;

	peer_imsg_flush(peer);

	LIST_REMOVE(peer, hash_l);
	LIST_REMOVE(peer, peer_l);
	free(peer);
}

/*
 * Flush all routes older then staletime. If staletime is 0 all routes will
 * be flushed.
 */
void
peer_flush(struct rde_peer *peer, u_int8_t aid, time_t staletime)
{
	struct peer_flush pf = { peer, staletime };

	/* this dump must run synchronous, too much depends on that right now */
	if (rib_dump_new(RIB_ADJ_IN, aid, 0, &pf, peer_flush_upcall,
	    NULL, NULL) == -1)
		fatal("%s: rib_dump_new", __func__);

	/* Deletions may have been performed in peer_flush_upcall */
	rde_send_pftable_commit();

	/* flushed no need to keep staletime */
	if (aid == AID_UNSPEC) {
		u_int8_t i;
		for (i = 0; i < AID_MAX; i++)
			peer->staletime[i] = 0;
	} else {
		peer->staletime[aid] = 0;
	}
}

/*
 * During graceful restart mark a peer as stale if the session goes down.
 * For the specified AID the Adj-RIB-Out as marked stale and the staletime
 * is set to the current timestamp for identifying stale routes in Adj-RIB-In.
 */
void
peer_stale(struct rde_peer *peer, u_int8_t aid)
{
	time_t			 now;

	/* flush the now even staler routes out */
	if (peer->staletime[aid])
		peer_flush(peer, aid, peer->staletime[aid]);

	peer->staletime[aid] = now = getmonotime();
	peer->state = PEER_DOWN;

	/* mark Adj-RIB-Out stale for this peer */
	if (prefix_dump_new(peer, AID_UNSPEC, 0, NULL,
	    peer_adjout_stale_upcall, NULL, NULL) == -1)
		fatal("%s: prefix_dump_new", __func__);

	/* make sure new prefixes start on a higher timestamp */
	while (now >= getmonotime())
		sleep(1);
}

/*
 * Load the Adj-RIB-Out of a peer normally called when a session is established.
 * Once the Adj-RIB-Out is ready stale routes are removed from the Adj-RIB-Out
 * and all routes are put on the update queue so they will be sent out.
 */
void
peer_dump(struct rde_peer *peer, u_int8_t aid)
{
	if (peer->conf.export_type == EXPORT_NONE) {
		/* nothing to send apart from the marker */
		if (peer->capa.grestart.restart)
			prefix_add_eor(peer, aid);
	} else if (peer->conf.export_type == EXPORT_DEFAULT_ROUTE) {
		up_generate_default(out_rules, peer, aid);
		rde_up_dump_done(peer, aid);
	} else {
		if (rib_dump_new(peer->loc_rib_id, aid, RDE_RUNNER_ROUNDS, peer,
		    rde_up_dump_upcall, rde_up_dump_done, NULL) == -1)
			fatal("%s: rib_dump_new", __func__);
		/* throttle peer until dump is done */
		peer->throttled = 1;
	}
}

/*
 * move an imsg from src to dst, disconnecting any dynamic memory from src.
 */
static void
imsg_move(struct imsg *dst, struct imsg *src)
{
	*dst = *src;
	src->data = NULL;	/* allocation was moved */
}

/*
 * push an imsg onto the peer imsg queue.
 */
void
peer_imsg_push(struct rde_peer *peer, struct imsg *imsg)
{
	struct iq *iq;

	if ((iq = calloc(1, sizeof(*iq))) == NULL)
		fatal(NULL);
	imsg_move(&iq->imsg, imsg);
	SIMPLEQ_INSERT_TAIL(&peer->imsg_queue, iq, entry);
}

/*
 * pop first imsg from peer imsg queue and move it into imsg argument.
 * Returns 1 if an element is returned else 0.
 */
int
peer_imsg_pop(struct rde_peer *peer, struct imsg *imsg)
{
	struct iq *iq;

	iq = SIMPLEQ_FIRST(&peer->imsg_queue);
	if (iq == NULL)
		return 0;

	imsg_move(imsg, &iq->imsg);

	SIMPLEQ_REMOVE_HEAD(&peer->imsg_queue, entry);
	free(iq);

	return 1;
}

static void
peer_imsg_queued(struct rde_peer *peer, void *arg)
{
	int *p = arg;

	*p = *p || !SIMPLEQ_EMPTY(&peer->imsg_queue);
}

/*
 * Check if any imsg are pending, return 0 if none are pending
 */
int
peer_imsg_pending(void)
{
	int pending = 0;

	peer_foreach(peer_imsg_queued, &pending);

	return pending;
}

/*
 * flush all imsg queued for a peer.
 */
void
peer_imsg_flush(struct rde_peer *peer)
{
	struct iq *iq;

	while ((iq = SIMPLEQ_FIRST(&peer->imsg_queue)) != NULL) {
		SIMPLEQ_REMOVE_HEAD(&peer->imsg_queue, entry);
		free(iq);
	}
}
