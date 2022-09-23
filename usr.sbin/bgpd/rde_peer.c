/*	$OpenBSD: rde_peer.c,v 1.25 2022/09/23 15:49:20 claudio Exp $ */

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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "bgpd.h"
#include "rde.h"

struct peer_tree	 peertable;
struct rde_peer		*peerself;

CTASSERT(sizeof(peerself->recv_eor) * 8 > AID_MAX);
CTASSERT(sizeof(peerself->sent_eor) * 8 > AID_MAX);

struct iq {
	SIMPLEQ_ENTRY(iq)	entry;
	struct imsg		imsg;
};

extern struct filter_head	*out_rules;

int
peer_has_as4byte(struct rde_peer *peer)
{
	return (peer->capa.as4byte);
}

int
peer_has_add_path(struct rde_peer *peer, uint8_t aid, int mode)
{
	if (aid > AID_MAX)
		return 0;
	if (aid == AID_UNSPEC) {
		/* check if at capability is set for at least one AID */
		for (aid = AID_MIN; aid < AID_MAX; aid++)
			if (peer->capa.add_path[aid] & mode)
				return 1;
		return 0;
	}
	return (peer->capa.add_path[aid] & mode);
}

int
peer_has_open_policy(struct rde_peer *peer, uint8_t *role)
{
	*role = peer->capa.role;
	return (peer->capa.role_ena != 0);
}

int
peer_accept_no_as_set(struct rde_peer *peer)
{
	return (peer->flags & PEERFLAG_NO_AS_SET);
}

void
peer_init(void)
{
	struct peer_config pc;

	RB_INIT(&peertable);

	memset(&pc, 0, sizeof(pc));
	snprintf(pc.descr, sizeof(pc.descr), "LOCAL");
	pc.id = PEER_ID_SELF;

	peerself = peer_add(PEER_ID_SELF, &pc);
	peerself->state = PEER_UP;
}

void
peer_shutdown(void)
{
	if (!RB_EMPTY(&peertable))
		log_warnx("%s: free non-free table", __func__);
}

/*
 * Traverse all peers calling callback for each peer.
 */
void
peer_foreach(void (*callback)(struct rde_peer *, void *), void *arg)
{
	struct rde_peer *peer, *np;

	RB_FOREACH_SAFE(peer, peer_tree, &peertable, np)
		callback(peer, arg);
}

/*
 * Lookup a peer by peer_id, return NULL if not found.
 */
struct rde_peer *
peer_get(uint32_t id)
{
	struct rde_peer	needle;

	needle.conf.id = id;
	return RB_FIND(peer_tree, &peertable, &needle);
}

/*
 * Find next peer that matches neighbor options in *n.
 * If peerid was set then pickup the lookup after that peer.
 * Returns NULL if no more peers match.
 */
struct rde_peer *
peer_match(struct ctl_neighbor *n, uint32_t peerid)
{
	struct rde_peer		*peer;

	if (peerid != 0) {
		peer = peer_get(peerid);
		if (peer)
			peer = RB_NEXT(peer_tree, &peertable, peer);
	} else
		peer = RB_MIN(peer_tree, &peertable);

	for (; peer != NULL; peer = RB_NEXT(peer_tree, &peertable, peer)) {
		if (rde_match_peer(peer, n))
			return (peer);
	}
	return (NULL);
}

struct rde_peer *
peer_add(uint32_t id, struct peer_config *p_conf)
{
	struct rde_peer		*peer;
	int			 conflict;

	if ((peer = peer_get(id))) {
		memcpy(&peer->conf, p_conf, sizeof(struct peer_config));
		return (peer);
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
	peer->eval = peer->conf.eval;
	peer->export_type = peer->conf.export_type;
	peer->flags = peer->conf.flags;
	SIMPLEQ_INIT(&peer->imsg_queue);

	/*
	 * Assign an even random unique transmit path id.
	 * Odd path_id_tx numbers are for peers using add-path recv.
	 */
	do {
		struct rde_peer *p;

		conflict = 0;
		peer->path_id_tx = arc4random() << 1;
		RB_FOREACH(p, peer_tree, &peertable) {
			if (p->path_id_tx == peer->path_id_tx) {
				conflict = 1;
				break;
			}
		}
	} while (conflict);

	if (RB_INSERT(peer_tree, &peertable, peer) != NULL)
		fatalx("rde peer table corrupted");

	return (peer);
}

static inline int
peer_cmp(struct rde_peer *a, struct rde_peer *b)
{
	if (a->conf.id > b->conf.id)
		return 1;
	if (a->conf.id < b->conf.id)
		return -1;
	return 0;
}

RB_GENERATE(peer_tree, rde_peer, entry, peer_cmp);

static void
peer_generate_update(struct rde_peer *peer, uint16_t rib_id,
    struct prefix *newbest, struct prefix *oldbest,
    struct prefix *newpath, struct prefix *oldpath,
    enum eval_mode mode)
{
	uint8_t		 aid;

	if (newbest != NULL)
		aid = newbest->pt->aid;
	else if (oldbest != NULL)
		aid = oldbest->pt->aid;
	else if (newpath != NULL)
		aid = newpath->pt->aid;
	else if (oldpath != NULL)
		aid = oldpath->pt->aid;
	else
		return;

	/* skip ourself */
	if (peer == peerself)
		return;
	if (peer->state != PEER_UP)
		return;
	/* skip peers using a different rib */
	if (peer->loc_rib_id != rib_id)
		return;
	/* check if peer actually supports the address family */
	if (peer->capa.mp[aid] == 0)
		return;
	/* skip peers with special export types */
	if (peer->export_type == EXPORT_NONE ||
	    peer->export_type == EXPORT_DEFAULT_ROUTE)
		return;

	/* if reconf skip peers which don't need to reconfigure */
	if (mode == EVAL_RECONF && peer->reconf_out == 0)
		return;

	/* handle peers with add-path */
	if (peer_has_add_path(peer, aid, CAPA_AP_SEND)) {
		if (peer->eval.mode == ADDPATH_EVAL_ALL)
			up_generate_addpath_all(out_rules, peer, newbest,
			    newpath, oldpath);
		else
			up_generate_addpath(out_rules, peer, newbest, oldbest);
		return;
	}

	/* skip regular peers if the best path didn't change */
	if (mode == EVAL_ALL && (peer->flags & PEERFLAG_EVALUATE_ALL) == 0)
		return;
	up_generate_updates(out_rules, peer, newbest, oldbest);
}

void
rde_generate_updates(struct rib *rib, struct prefix *newbest,
    struct prefix *oldbest, struct prefix *newpath, struct prefix *oldpath,
    enum eval_mode mode)
{
	struct rde_peer	*peer;

	/*
	 * If oldbest is != NULL we know it was active and should be removed.
	 * If newbest is != NULL we know it is reachable and then we should
	 * generate an update.
	 */
	if (oldbest == NULL && newbest == NULL)
		return;

	RB_FOREACH(peer, peer_tree, &peertable)
		peer_generate_update(peer, rib->id, newbest, oldbest, newpath,
		    oldpath, mode);
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
	uint32_t i;
	uint8_t prefixlen;

	pt_getaddr(re->prefix, &addr);
	prefixlen = re->prefix->prefixlen;
	TAILQ_FOREACH_SAFE(p, &re->prefix_h, entry.list.rib, np) {
		if (peer != prefix_peer(p))
			continue;
		if (staletime && p->lastchange > staletime)
			continue;

		for (i = RIB_LOC_START; i < rib_size; i++) {
			struct rib *rib = rib_byid(i);
			if (rib == NULL)
				continue;
			rp = prefix_get(rib, peer, p->path_id,
			    &addr, prefixlen);
			if (rp) {
				asp = prefix_aspath(rp);
				if (asp && asp->pftableid)
					rde_pftable_del(asp->pftableid, rp);

				prefix_destroy(rp);
				rde_update_log("flush", i, peer, NULL,
				    &addr, prefixlen);
			}
		}

		prefix_destroy(p);
		peer->prefix_cnt--;
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
rde_up_adjout_force_done(void *ptr, uint8_t aid)
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
	struct prefix		*p;

	if ((p = prefix_best(re)) == NULL)
		/* no eligible prefix, not even for 'evaluate all' */
		return;
	peer_generate_update(peer, re->rib_id, p, NULL, NULL, NULL, 0);
}

static void
rde_up_dump_done(void *ptr, uint8_t aid)
{
	struct rde_peer		*peer = ptr;

	/* force out all updates of Adj-RIB-Out for this peer */
	if (prefix_dump_new(peer, aid, 0, peer, rde_up_adjout_force_upcall,
	    rde_up_adjout_force_done, NULL) == -1)
		fatal("%s: prefix_dump_new", __func__);
}

/*
 * Session got established, bring peer up, load RIBs do initial table dump.
 */
void
peer_up(struct rde_peer *peer, struct session_up *sup)
{
	uint8_t	 i;

	if (peer->state == PEER_ERR) {
		/*
		 * There is a race condition when doing PEER_ERR -> PEER_DOWN.
		 * So just do a full reset of the peer here.
		 */
		rib_dump_terminate(peer);
		peer_imsg_flush(peer);
		if (prefix_dump_new(peer, AID_UNSPEC, 0, NULL,
		    peer_adjout_clear_upcall, NULL, NULL) == -1)
			fatal("%s: prefix_dump_new", __func__);
		peer_flush(peer, AID_UNSPEC, 0);
		peer->prefix_cnt = 0;
		peer->prefix_out_cnt = 0;
		peer->state = PEER_DOWN;
	}
	peer->remote_bgpid = ntohl(sup->remote_bgpid);
	peer->short_as = sup->short_as;
	peer->remote_addr = sup->remote_addr;
	peer->local_v4_addr = sup->local_v4_addr;
	peer->local_v6_addr = sup->local_v6_addr;
	memcpy(&peer->capa, &sup->capa, sizeof(peer->capa));

	/* clear eor markers depending on GR flags */
	if (peer->capa.grestart.restart) {
		peer->sent_eor = 0;
		peer->recv_eor = 0;
	} else {
		/* no EOR expected */
		peer->sent_eor = ~0;
		peer->recv_eor = ~0;
	}
	peer->state = PEER_UP;

	for (i = 0; i < AID_MAX; i++) {
		if (peer->capa.mp[i])
			peer_dump(peer, i);
	}
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
	/*
	 * stop all pending dumps which may depend on this peer
	 * and flush all pending imsg from the SE.
	 */
	rib_dump_terminate(peer);
	peer_imsg_flush(peer);

	/* flush Adj-RIB-Out */
	if (prefix_dump_new(peer, AID_UNSPEC, 0, NULL,
	    peer_adjout_clear_upcall, NULL, NULL) == -1)
		fatal("%s: prefix_dump_new", __func__);

	/* flush Adj-RIB-In */
	peer_flush(peer, AID_UNSPEC, 0);
	peer->prefix_cnt = 0;
	peer->prefix_out_cnt = 0;

	RB_REMOVE(peer_tree, &peertable, peer);
	free(peer);
}

/*
 * Flush all routes older then staletime. If staletime is 0 all routes will
 * be flushed.
 */
void
peer_flush(struct rde_peer *peer, uint8_t aid, time_t staletime)
{
	struct peer_flush pf = { peer, staletime };

	/* this dump must run synchronous, too much depends on that right now */
	if (rib_dump_new(RIB_ADJ_IN, aid, 0, &pf, peer_flush_upcall,
	    NULL, NULL) == -1)
		fatal("%s: rib_dump_new", __func__);

	/* every route is gone so reset staletime */
	if (aid == AID_UNSPEC) {
		uint8_t i;
		for (i = 0; i < AID_MAX; i++)
			peer->staletime[i] = 0;
	} else {
		peer->staletime[aid] = 0;
	}
}

/*
 * During graceful restart mark a peer as stale if the session goes down.
 * For the specified AID the Adj-RIB-Out is marked stale and the staletime
 * is set to the current timestamp for identifying stale routes in Adj-RIB-In.
 */
void
peer_stale(struct rde_peer *peer, uint8_t aid, int flushall)
{
	time_t now;

	/* flush the now even staler routes out */
	if (peer->staletime[aid])
		peer_flush(peer, aid, peer->staletime[aid]);

	peer->staletime[aid] = now = getmonotime();
	peer->state = PEER_DOWN;

	/*
	 * stop all pending dumps which may depend on this peer
	 * and flush all pending imsg from the SE.
	 */
	rib_dump_terminate(peer);
	peer_imsg_flush(peer);

	if (flushall)
		peer_flush(peer, aid, 0);

	/* XXX this is not quite correct */
	/* mark Adj-RIB-Out stale for this peer */
	if (prefix_dump_new(peer, aid, 0, NULL,
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
peer_dump(struct rde_peer *peer, uint8_t aid)
{
	if (peer->capa.enhanced_rr && (peer->sent_eor & (1 << aid)))
		rde_peer_send_rrefresh(peer, aid, ROUTE_REFRESH_BEGIN_RR);

	if (peer->export_type == EXPORT_NONE) {
		/* nothing to send apart from the marker */
		if (peer->capa.grestart.restart)
			prefix_add_eor(peer, aid);
	} else if (peer->export_type == EXPORT_DEFAULT_ROUTE) {
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
 * Start of an enhanced route refresh. Mark all routes as stale.
 * Once the route refresh ends a End of Route Refresh message is sent
 * which calls peer_flush() to remove all stale routes.
 */
void
peer_begin_rrefresh(struct rde_peer *peer, uint8_t aid)
{
	time_t now;

	/* flush the now even staler routes out */
	if (peer->staletime[aid])
		peer_flush(peer, aid, peer->staletime[aid]);

	peer->staletime[aid] = now = getmonotime();

	/* make sure new prefixes start on a higher timestamp */
	while (now >= getmonotime())
		sleep(1);
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
