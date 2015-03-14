/*	$OpenBSD: if_pfsync.c,v 1.218 2015/03/14 03:38:51 jsg Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR OR HIS RELATIVES BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF MIND, USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
 * IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Copyright (c) 2009 David Gwynne <dlg@openbsd.org>
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
#include <sys/systm.h>
#include <sys/time.h>
#include <sys/malloc.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/timeout.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/pool.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/netisr.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>
#include <netinet/tcp_seq.h>
#include <netinet/tcp_fsm.h>

#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>

#ifdef IPSEC
#include <netinet/ip_ipsp.h>
#endif /* IPSEC */

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#endif /* INET6 */

#include "carp.h"
#if NCARP > 0
#include <netinet/ip_carp.h>
#endif

#define PF_DEBUGNAME	"pfsync: "
#include <net/pfvar.h>
#include <netinet/ip_ipsp.h>
#include <net/if_pfsync.h>

#include "bpfilter.h"
#include "pfsync.h"

#define PFSYNC_MINPKT ( \
	sizeof(struct ip) + \
	sizeof(struct pfsync_header))

int	pfsync_upd_tcp(struct pf_state *, struct pfsync_state_peer *,
	    struct pfsync_state_peer *);

int	pfsync_in_clr(caddr_t, int, int, int);
int	pfsync_in_iack(caddr_t, int, int, int);
int	pfsync_in_upd_c(caddr_t, int, int, int);
int	pfsync_in_ureq(caddr_t, int, int, int);
int	pfsync_in_del(caddr_t, int, int, int);
int	pfsync_in_del_c(caddr_t, int, int, int);
int	pfsync_in_bus(caddr_t, int, int, int);
int	pfsync_in_tdb(caddr_t, int, int, int);
int	pfsync_in_ins(caddr_t, int, int, int);
int	pfsync_in_upd(caddr_t, int, int, int);
int	pfsync_in_eof(caddr_t, int, int, int);

int	pfsync_in_error(caddr_t, int, int, int);

struct {
	int	(*in)(caddr_t, int, int, int);
	size_t	len;
} pfsync_acts[] = {
	/* PFSYNC_ACT_CLR */
	{ pfsync_in_clr,	sizeof(struct pfsync_clr) },
	 /* PFSYNC_ACT_OINS */
	{ pfsync_in_error,	0 },
	/* PFSYNC_ACT_INS_ACK */
	{ pfsync_in_iack,	sizeof(struct pfsync_ins_ack) },
	/* PFSYNC_ACT_OUPD */
	{ pfsync_in_error,	0 },
	/* PFSYNC_ACT_UPD_C */
	{ pfsync_in_upd_c,	sizeof(struct pfsync_upd_c) },
	/* PFSYNC_ACT_UPD_REQ */
	{ pfsync_in_ureq,	sizeof(struct pfsync_upd_req) },
	/* PFSYNC_ACT_DEL */
	{ pfsync_in_del,	sizeof(struct pfsync_state) },
	/* PFSYNC_ACT_DEL_C */
	{ pfsync_in_del_c,	sizeof(struct pfsync_del_c) },
	/* PFSYNC_ACT_INS_F */
	{ pfsync_in_error,	0 },
	/* PFSYNC_ACT_DEL_F */
	{ pfsync_in_error,	0 },
	/* PFSYNC_ACT_BUS */
	{ pfsync_in_bus,	sizeof(struct pfsync_bus) },
	/* PFSYNC_ACT_OTDB */
	{ pfsync_in_error,	0 },
	/* PFSYNC_ACT_EOF */
	{ pfsync_in_error,	0 },
	/* PFSYNC_ACT_INS */
	{ pfsync_in_ins,	sizeof(struct pfsync_state) },
	/* PFSYNC_ACT_UPD */
	{ pfsync_in_upd,	sizeof(struct pfsync_state) },
	/* PFSYNC_ACT_TDB */
	{ pfsync_in_tdb,	sizeof(struct pfsync_tdb) },
};

struct pfsync_q {
	void		(*write)(struct pf_state *, void *);
	size_t		len;
	u_int8_t	action;
};

/* we have one of these for every PFSYNC_S_ */
void	pfsync_out_state(struct pf_state *, void *);
void	pfsync_out_iack(struct pf_state *, void *);
void	pfsync_out_upd_c(struct pf_state *, void *);
void	pfsync_out_del(struct pf_state *, void *);

struct pfsync_q pfsync_qs[] = {
	{ pfsync_out_iack,  sizeof(struct pfsync_ins_ack), PFSYNC_ACT_INS_ACK },
	{ pfsync_out_upd_c, sizeof(struct pfsync_upd_c),   PFSYNC_ACT_UPD_C },
	{ pfsync_out_del,   sizeof(struct pfsync_del_c),   PFSYNC_ACT_DEL_C },
	{ pfsync_out_state, sizeof(struct pfsync_state),   PFSYNC_ACT_INS },
	{ pfsync_out_state, sizeof(struct pfsync_state),   PFSYNC_ACT_UPD }
};

void	pfsync_q_ins(struct pf_state *, int);
void	pfsync_q_del(struct pf_state *);

struct pfsync_upd_req_item {
	TAILQ_ENTRY(pfsync_upd_req_item)	ur_entry;
	struct pfsync_upd_req			ur_msg;
};
TAILQ_HEAD(pfsync_upd_reqs, pfsync_upd_req_item);

struct pfsync_deferral {
	TAILQ_ENTRY(pfsync_deferral)		 pd_entry;
	struct pf_state				*pd_st;
	struct mbuf				*pd_m;
	struct timeout				 pd_tmo;
};
TAILQ_HEAD(pfsync_deferrals, pfsync_deferral);

#define PFSYNC_PLSIZE	MAX(sizeof(struct pfsync_upd_req_item), \
			    sizeof(struct pfsync_deferral))

void	pfsync_out_tdb(struct tdb *, void *);

struct pfsync_softc {
	struct ifnet		 sc_if;
	struct ifnet		*sc_sync_if;

	struct pool		 sc_pool;

	struct ip_moptions	 sc_imo;

	struct in_addr		 sc_sync_peer;
	u_int8_t		 sc_maxupdates;

	struct ip		 sc_template;

	struct pf_state_queue	 sc_qs[PFSYNC_S_COUNT];
	size_t			 sc_len;

	struct pfsync_upd_reqs	 sc_upd_req_list;

	int			 sc_initial_bulk;
	int			 sc_link_demoted;

	int			 sc_defer;
	struct pfsync_deferrals	 sc_deferrals;
	u_int			 sc_deferred;

	void			*sc_plus;
	size_t			 sc_pluslen;

	u_int32_t		 sc_ureq_sent;
	int			 sc_bulk_tries;
	struct timeout		 sc_bulkfail_tmo;

	u_int32_t		 sc_ureq_received;
	struct pf_state		*sc_bulk_next;
	struct pf_state		*sc_bulk_last;
	struct timeout		 sc_bulk_tmo;

	TAILQ_HEAD(, tdb)	 sc_tdb_q;

	void			*sc_lhcookie;

	struct timeout		 sc_tmo;
};

struct pfsync_softc	*pfsyncif = NULL;
struct pfsyncstats	 pfsyncstats;

void	pfsyncattach(int);
int	pfsync_clone_create(struct if_clone *, int);
int	pfsync_clone_destroy(struct ifnet *);
int	pfsync_alloc_scrub_memory(struct pfsync_state_peer *,
	    struct pf_state_peer *);
void	pfsync_update_net_tdb(struct pfsync_tdb *);
int	pfsyncoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);
int	pfsyncioctl(struct ifnet *, u_long, caddr_t);
void	pfsyncstart(struct ifnet *);
void	pfsync_syncdev_state(void *);

struct mbuf *pfsync_if_dequeue(struct ifnet *);

void	pfsync_deferred(struct pf_state *, int);
void	pfsync_undefer(struct pfsync_deferral *, int);
void	pfsync_defer_tmo(void *);

void	pfsync_cancel_full_update(struct pfsync_softc *);
void	pfsync_request_full_update(struct pfsync_softc *);
void	pfsync_request_update(u_int32_t, u_int64_t);
void	pfsync_update_state_req(struct pf_state *);

void	pfsync_drop(struct pfsync_softc *);
void	pfsync_sendout(void);
void	pfsync_send_plus(void *, size_t);
void	pfsync_timeout(void *);
void	pfsync_tdb_timeout(void *);

void	pfsync_bulk_start(void);
void	pfsync_bulk_status(u_int8_t);
void	pfsync_bulk_update(void *);
void	pfsync_bulk_fail(void *);

#define PFSYNC_MAX_BULKTRIES	12
int	pfsync_sync_ok;

struct if_clone	pfsync_cloner =
    IF_CLONE_INITIALIZER("pfsync", pfsync_clone_create, pfsync_clone_destroy);

void
pfsyncattach(int npfsync)
{
	if_clone_attach(&pfsync_cloner);
}

int
pfsync_clone_create(struct if_clone *ifc, int unit)
{
	struct pfsync_softc *sc;
	struct ifnet *ifp;
	int q;

	if (unit != 0)
		return (EINVAL);

	pfsync_sync_ok = 1;

	sc = malloc(sizeof(*pfsyncif), M_DEVBUF, M_WAITOK | M_ZERO);

	for (q = 0; q < PFSYNC_S_COUNT; q++)
		TAILQ_INIT(&sc->sc_qs[q]);

	pool_init(&sc->sc_pool, PFSYNC_PLSIZE, 0, 0, 0, "pfsync", NULL);
	TAILQ_INIT(&sc->sc_upd_req_list);
	TAILQ_INIT(&sc->sc_deferrals);
	sc->sc_deferred = 0;

	TAILQ_INIT(&sc->sc_tdb_q);

	sc->sc_len = PFSYNC_MINPKT;
	sc->sc_maxupdates = 128;

	sc->sc_imo.imo_membership = (struct in_multi **)malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
	    M_WAITOK | M_ZERO);
	sc->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;

	ifp = &sc->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "pfsync%d", unit);
	ifp->if_softc = sc;
	ifp->if_ioctl = pfsyncioctl;
	ifp->if_output = pfsyncoutput;
	ifp->if_start = pfsyncstart;
	ifp->if_type = IFT_PFSYNC;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_hdrlen = sizeof(struct pfsync_header);
	ifp->if_mtu = ETHERMTU;
	timeout_set(&sc->sc_tmo, pfsync_timeout, sc);
	timeout_set(&sc->sc_bulk_tmo, pfsync_bulk_update, sc);
	timeout_set(&sc->sc_bulkfail_tmo, pfsync_bulk_fail, sc);

	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NCARP > 0
	if_addgroup(ifp, "carp");
#endif

#if NBPFILTER > 0
	bpfattach(&sc->sc_if.if_bpf, ifp, DLT_PFSYNC, PFSYNC_HDRLEN);
#endif

	pfsyncif = sc;

	return (0);
}

int
pfsync_clone_destroy(struct ifnet *ifp)
{
	struct pfsync_softc *sc = ifp->if_softc;
	struct pfsync_deferral *pd;
	int s;

	s = splsoftnet();
	timeout_del(&sc->sc_bulkfail_tmo);
	timeout_del(&sc->sc_bulk_tmo);
	timeout_del(&sc->sc_tmo);
#if NCARP > 0
	if (!pfsync_sync_ok)
		carp_group_demote_adj(&sc->sc_if, -1, "pfsync destroy");
	if (sc->sc_link_demoted)
		carp_group_demote_adj(&sc->sc_if, -1, "pfsync destroy");
#endif
	if (sc->sc_sync_if)
		hook_disestablish(
		    sc->sc_sync_if->if_linkstatehooks,
		    sc->sc_lhcookie);
	if_detach(ifp);

	pfsync_drop(sc);

	while (sc->sc_deferred > 0) {
		pd = TAILQ_FIRST(&sc->sc_deferrals);
		timeout_del(&pd->pd_tmo);
		pfsync_undefer(pd, 0);
	}

	pool_destroy(&sc->sc_pool);
	free(sc->sc_imo.imo_membership, M_IPMOPTS, 0);
	free(sc, M_DEVBUF, sizeof(*sc));

	pfsyncif = NULL;
	splx(s);

	return (0);
}

struct mbuf *
pfsync_if_dequeue(struct ifnet *ifp)
{
	struct mbuf *m;

	IF_DEQUEUE(&ifp->if_snd, m);

	return (m);
}

/*
 * Start output on the pfsync interface.
 */
void
pfsyncstart(struct ifnet *ifp)
{
	struct mbuf *m;
	int s;

	s = splnet();
	while ((m = pfsync_if_dequeue(ifp)) != NULL) {
		IF_DROP(&ifp->if_snd);
		m_freem(m);
	}
	splx(s);
}

void
pfsync_syncdev_state(void *arg)
{
	struct pfsync_softc *sc = arg;

	if (!sc->sc_sync_if || !(sc->sc_if.if_flags & IFF_UP))
		return;

	if (sc->sc_sync_if->if_link_state == LINK_STATE_DOWN) {
		sc->sc_if.if_flags &= ~IFF_RUNNING;
		if (!sc->sc_link_demoted) {
#if NCARP > 0
			carp_group_demote_adj(&sc->sc_if, 1,
			    "pfsync link state down");
#endif
			sc->sc_link_demoted = 1;
		}

		/* drop everything */
		timeout_del(&sc->sc_tmo);
		pfsync_drop(sc);

		pfsync_cancel_full_update(sc);
	} else if (sc->sc_link_demoted) {
		sc->sc_if.if_flags |= IFF_RUNNING;

		pfsync_request_full_update(sc);
	}
}

int
pfsync_alloc_scrub_memory(struct pfsync_state_peer *s,
    struct pf_state_peer *d)
{
	if (s->scrub.scrub_flag && d->scrub == NULL) {
		d->scrub = pool_get(&pf_state_scrub_pl, PR_NOWAIT | PR_ZERO);
		if (d->scrub == NULL)
			return (ENOMEM);
	}

	return (0);
}

void
pfsync_state_export(struct pfsync_state *sp, struct pf_state *st)
{
	pf_state_export(sp, st);
}

int
pfsync_state_import(struct pfsync_state *sp, int flags)
{
	struct pf_state	*st = NULL;
	struct pf_state_key *skw = NULL, *sks = NULL;
	struct pf_rule *r = NULL;
	struct pfi_kif	*kif;
	int pool_flags;
	int error;

	if (sp->creatorid == 0) {
		DPFPRINTF(LOG_NOTICE, "pfsync_state_import: "
		    "invalid creator id: %08x", ntohl(sp->creatorid));
		return (EINVAL);
	}

	if ((kif = pfi_kif_get(sp->ifname)) == NULL) {
		DPFPRINTF(LOG_NOTICE, "pfsync_state_import: "
		    "unknown interface: %s", sp->ifname);
		if (flags & PFSYNC_SI_IOCTL)
			return (EINVAL);
		return (0);	/* skip this state */
	}

	if (sp->af == 0)
		return (0);	/* skip this state */

	/*
	 * If the ruleset checksums match or the state is coming from the ioctl,
	 * it's safe to associate the state with the rule of that number.
	 */
	if (sp->rule != htonl(-1) && sp->anchor == htonl(-1) &&
	    (flags & (PFSYNC_SI_IOCTL | PFSYNC_SI_CKSUM)) && ntohl(sp->rule) <
	    pf_main_ruleset.rules.active.rcount)
		r = pf_main_ruleset.rules.active.ptr_array[ntohl(sp->rule)];
	else
		r = &pf_default_rule;

	if ((r->max_states && r->states_cur >= r->max_states))
		goto cleanup;

	if (flags & PFSYNC_SI_IOCTL)
		pool_flags = PR_WAITOK | PR_LIMITFAIL | PR_ZERO;
	else
		pool_flags = PR_NOWAIT | PR_LIMITFAIL | PR_ZERO;

	if ((st = pool_get(&pf_state_pl, pool_flags)) == NULL)
		goto cleanup;

	if ((skw = pf_alloc_state_key(pool_flags)) == NULL)
		goto cleanup;

	if ((sp->key[PF_SK_WIRE].af &&
	    (sp->key[PF_SK_WIRE].af != sp->key[PF_SK_STACK].af)) ||
	    PF_ANEQ(&sp->key[PF_SK_WIRE].addr[0],
	    &sp->key[PF_SK_STACK].addr[0], sp->af) ||
	    PF_ANEQ(&sp->key[PF_SK_WIRE].addr[1],
	    &sp->key[PF_SK_STACK].addr[1], sp->af) ||
	    sp->key[PF_SK_WIRE].port[0] != sp->key[PF_SK_STACK].port[0] ||
	    sp->key[PF_SK_WIRE].port[1] != sp->key[PF_SK_STACK].port[1] ||
	    sp->key[PF_SK_WIRE].rdomain != sp->key[PF_SK_STACK].rdomain) {
		if ((sks = pf_alloc_state_key(pool_flags)) == NULL)
			goto cleanup;
	} else
		sks = skw;

	/* allocate memory for scrub info */
	if (pfsync_alloc_scrub_memory(&sp->src, &st->src) ||
	    pfsync_alloc_scrub_memory(&sp->dst, &st->dst))
		goto cleanup;

	/* copy to state key(s) */
	skw->addr[0] = sp->key[PF_SK_WIRE].addr[0];
	skw->addr[1] = sp->key[PF_SK_WIRE].addr[1];
	skw->port[0] = sp->key[PF_SK_WIRE].port[0];
	skw->port[1] = sp->key[PF_SK_WIRE].port[1];
	skw->rdomain = ntohs(sp->key[PF_SK_WIRE].rdomain);
	skw->proto = sp->proto;
	if (!(skw->af = sp->key[PF_SK_WIRE].af))
		skw->af = sp->af;
	if (sks != skw) {
		sks->addr[0] = sp->key[PF_SK_STACK].addr[0];
		sks->addr[1] = sp->key[PF_SK_STACK].addr[1];
		sks->port[0] = sp->key[PF_SK_STACK].port[0];
		sks->port[1] = sp->key[PF_SK_STACK].port[1];
		sks->rdomain = ntohs(sp->key[PF_SK_STACK].rdomain);
		if (!(sks->af = sp->key[PF_SK_STACK].af))
			sks->af = sp->af;
		if (sks->af != skw->af) {
			switch (sp->proto) {
			case IPPROTO_ICMP:
				sks->proto = IPPROTO_ICMPV6;
				break;
			case IPPROTO_ICMPV6:
				sks->proto = IPPROTO_ICMP;
				break;
			default:
				sks->proto = sp->proto;
			}
		} else
			sks->proto = sp->proto;
	}
	st->rtableid[PF_SK_WIRE] = ntohl(sp->rtableid[PF_SK_WIRE]);
	st->rtableid[PF_SK_STACK] = ntohl(sp->rtableid[PF_SK_STACK]);

	/* copy to state */
	bcopy(&sp->rt_addr, &st->rt_addr, sizeof(st->rt_addr));
	st->creation = time_uptime - ntohl(sp->creation);
	st->expire = time_uptime;
	if (ntohl(sp->expire)) {
		u_int32_t timeout;

		timeout = r->timeout[sp->timeout];
		if (!timeout)
			timeout = pf_default_rule.timeout[sp->timeout];

		/* sp->expire may have been adaptively scaled by export. */
		st->expire -= timeout - ntohl(sp->expire);
	}

	st->direction = sp->direction;
	st->log = sp->log;
	st->timeout = sp->timeout;
	st->state_flags = ntohs(sp->state_flags);
	st->max_mss = ntohs(sp->max_mss);
	st->min_ttl = sp->min_ttl;
	st->set_tos = sp->set_tos;
	st->set_prio[0] = sp->set_prio[0];
	st->set_prio[1] = sp->set_prio[1];

	st->id = sp->id;
	st->creatorid = sp->creatorid;
	pf_state_peer_ntoh(&sp->src, &st->src);
	pf_state_peer_ntoh(&sp->dst, &st->dst);

	st->rule.ptr = r;
	st->anchor.ptr = NULL;
	st->rt_kif = NULL;

	st->pfsync_time = time_uptime;
	st->sync_state = PFSYNC_S_NONE;

	/* XXX when we have anchors, use STATE_INC_COUNTERS */
	r->states_cur++;
	r->states_tot++;

	if (!ISSET(flags, PFSYNC_SI_IOCTL))
		SET(st->state_flags, PFSTATE_NOSYNC);

	if (pf_state_insert(kif, &skw, &sks, st) != 0) {
		/* XXX when we have anchors, use STATE_DEC_COUNTERS */
		r->states_cur--;
		error = EEXIST;
		goto cleanup_state;
	}

	if (!ISSET(flags, PFSYNC_SI_IOCTL)) {
		CLR(st->state_flags, PFSTATE_NOSYNC);
		if (ISSET(st->state_flags, PFSTATE_ACK)) {
			pfsync_q_ins(st, PFSYNC_S_IACK);
			schednetisr(NETISR_PFSYNC);
		}
	}
	CLR(st->state_flags, PFSTATE_ACK);

	return (0);

 cleanup:
	error = ENOMEM;
	if (skw == sks)
		sks = NULL;
	if (skw != NULL)
		pool_put(&pf_state_key_pl, skw);
	if (sks != NULL)
		pool_put(&pf_state_key_pl, sks);

 cleanup_state:	/* pf_state_insert frees the state keys */
	if (st) {
		if (st->dst.scrub)
			pool_put(&pf_state_scrub_pl, st->dst.scrub);
		if (st->src.scrub)
			pool_put(&pf_state_scrub_pl, st->src.scrub);
		pool_put(&pf_state_pl, st);
	}
	return (error);
}

void
pfsync_input(struct mbuf *m, ...)
{
	struct pfsync_softc *sc = pfsyncif;
	struct ip *ip = mtod(m, struct ip *);
	struct mbuf *mp;
	struct pfsync_header *ph;
	struct pfsync_subheader subh;

	int offset, offp, len, count, mlen, flags = 0;

	pfsyncstats.pfsyncs_ipackets++;

	/* verify that we have a sync interface configured */
	if (sc == NULL || !ISSET(sc->sc_if.if_flags, IFF_RUNNING) ||
	    sc->sc_sync_if == NULL || !pf_status.running)
		goto done;

	/* verify that the packet came in on the right interface */
	if (sc->sc_sync_if != m->m_pkthdr.rcvif) {
		pfsyncstats.pfsyncs_badif++;
		goto done;
	}

	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	/* verify that the IP TTL is 255. */
	if (ip->ip_ttl != PFSYNC_DFLTTL) {
		pfsyncstats.pfsyncs_badttl++;
		goto done;
	}

	offset = ip->ip_hl << 2;
	mp = m_pulldown(m, offset, sizeof(*ph), &offp);
	if (mp == NULL) {
		pfsyncstats.pfsyncs_hdrops++;
		return;
	}
	ph = (struct pfsync_header *)(mp->m_data + offp);

	/* verify the version */
	if (ph->version != PFSYNC_VERSION) {
		pfsyncstats.pfsyncs_badver++;
		goto done;
	}
	len = ntohs(ph->len) + offset;
	if (m->m_pkthdr.len < len) {
		pfsyncstats.pfsyncs_badlen++;
		goto done;
	}

	if (!bcmp(&ph->pfcksum, &pf_status.pf_chksum, PF_MD5_DIGEST_LENGTH))
		flags = PFSYNC_SI_CKSUM;

	offset += sizeof(*ph);
	while (offset <= len - sizeof(subh)) {
		m_copydata(m, offset, sizeof(subh), (caddr_t)&subh);
		offset += sizeof(subh);

		mlen = subh.len << 2;
		count = ntohs(subh.count);

		if (subh.action >= PFSYNC_ACT_MAX ||
		    subh.action >= nitems(pfsync_acts) ||
		    mlen < pfsync_acts[subh.action].len) {
			/*
			 * subheaders are always followed by at least one
			 * message, so if the peer is new
			 * enough to tell us how big its messages are then we
			 * know enough to skip them.
			 */
			if (count > 0 && mlen > 0) {
				offset += count * mlen;
				continue;
			}
			pfsyncstats.pfsyncs_badact++;
			goto done;
		}

		mp = m_pulldown(m, offset, mlen * count, &offp);
		if (mp == NULL) {
			pfsyncstats.pfsyncs_badlen++;
			return;
		}

		if (pfsync_acts[subh.action].in(mp->m_data + offp,
		    mlen, count, flags) != 0)
			goto done;

		offset += mlen * count;
	}

done:
	m_freem(m);
}

int
pfsync_in_clr(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_clr *clr;
	int i;

	struct pf_state *st, *nexts;
	struct pf_state_key *sk, *nextsk;
	struct pf_state_item *si;
	u_int32_t creatorid;

	for (i = 0; i < count; i++) {
		clr = (struct pfsync_clr *)buf + len * i;
		creatorid = clr->creatorid;

		if (clr->ifname[0] == '\0') {
			for (st = RB_MIN(pf_state_tree_id, &tree_id);
			    st; st = nexts) {
				nexts = RB_NEXT(pf_state_tree_id, &tree_id, st);
				if (st->creatorid == creatorid) {
					SET(st->state_flags, PFSTATE_NOSYNC);
					pf_unlink_state(st);
				}
			}
		} else {
			if (pfi_kif_get(clr->ifname) == NULL)
				continue;

			/* XXX correct? */
			for (sk = RB_MIN(pf_state_tree, &pf_statetbl);
			    sk; sk = nextsk) {
				nextsk = RB_NEXT(pf_state_tree,
				    &pf_statetbl, sk);
				TAILQ_FOREACH(si, &sk->states, entry) {
					if (si->s->creatorid == creatorid) {
						SET(si->s->state_flags,
						    PFSTATE_NOSYNC);
						pf_unlink_state(si->s);
					}
				}
			}
		}
	}

	return (0);
}

int
pfsync_in_ins(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_state *sp;
	sa_family_t af1, af2;
	int i;

	for (i = 0; i < count; i++) {
		sp = (struct pfsync_state *)(buf + len * i);
		af1 = sp->key[0].af;
		af2 = sp->key[1].af;

		/* check for invalid values */
		if (sp->timeout >= PFTM_MAX ||
		    sp->src.state > PF_TCPS_PROXY_DST ||
		    sp->dst.state > PF_TCPS_PROXY_DST ||
		    sp->direction > PF_OUT ||
		    (((af1 || af2) &&
		     ((af1 != AF_INET && af1 != AF_INET6) ||
		      (af2 != AF_INET && af2 != AF_INET6))) ||
		    (sp->af != AF_INET && sp->af != AF_INET6))) {
			DPFPRINTF(LOG_NOTICE,
			    "pfsync_input: PFSYNC5_ACT_INS: invalid value");
			pfsyncstats.pfsyncs_badval++;
			continue;
		}

		if (pfsync_state_import(sp, flags) == ENOMEM) {
			/* drop out, but process the rest of the actions */
			break;
		}
	}

	return (0);
}

int
pfsync_in_iack(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_ins_ack *ia;
	struct pf_state_cmp id_key;
	struct pf_state *st;
	int i;

	for (i = 0; i < count; i++) {
		ia = (struct pfsync_ins_ack *)(buf + len * i);

		id_key.id = ia->id;
		id_key.creatorid = ia->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL)
			continue;

		if (ISSET(st->state_flags, PFSTATE_ACK))
			pfsync_deferred(st, 0);
	}

	return (0);
}

int
pfsync_upd_tcp(struct pf_state *st, struct pfsync_state_peer *src,
    struct pfsync_state_peer *dst)
{
	int sync = 0;

	/*
	 * The state should never go backwards except
	 * for syn-proxy states.  Neither should the
	 * sequence window slide backwards.
	 */
	if ((st->src.state > src->state &&
	    (st->src.state < PF_TCPS_PROXY_SRC ||
	    src->state >= PF_TCPS_PROXY_SRC)) ||

	    (st->src.state == src->state &&
	    SEQ_GT(st->src.seqlo, ntohl(src->seqlo))))
		sync++;
	else
		pf_state_peer_ntoh(src, &st->src);

	if ((st->dst.state > dst->state) ||

	    (st->dst.state >= TCPS_SYN_SENT &&
	    SEQ_GT(st->dst.seqlo, ntohl(dst->seqlo))))
		sync++;
	else
		pf_state_peer_ntoh(dst, &st->dst);

	return (sync);
}

int
pfsync_in_upd(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_state *sp;
	struct pf_state_cmp id_key;
	struct pf_state *st;
	int sync;

	int i;

	for (i = 0; i < count; i++) {
		sp = (struct pfsync_state *)(buf + len * i);

		/* check for invalid values */
		if (sp->timeout >= PFTM_MAX ||
		    sp->src.state > PF_TCPS_PROXY_DST ||
		    sp->dst.state > PF_TCPS_PROXY_DST) {
			DPFPRINTF(LOG_NOTICE,
			    "pfsync_input: PFSYNC_ACT_UPD: invalid value");
			pfsyncstats.pfsyncs_badval++;
			continue;
		}

		id_key.id = sp->id;
		id_key.creatorid = sp->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL) {
			/* insert the update */
			if (pfsync_state_import(sp, flags))
				pfsyncstats.pfsyncs_badstate++;
			continue;
		}

		if (ISSET(st->state_flags, PFSTATE_ACK))
			pfsync_deferred(st, 1);

		if (st->key[PF_SK_WIRE]->proto == IPPROTO_TCP)
			sync = pfsync_upd_tcp(st, &sp->src, &sp->dst);
		else {
			sync = 0;

			/*
			 * Non-TCP protocol state machine always go
			 * forwards
			 */
			if (st->src.state > sp->src.state)
				sync++;
			else
				pf_state_peer_ntoh(&sp->src, &st->src);

			if (st->dst.state > sp->dst.state)
				sync++;
			else
				pf_state_peer_ntoh(&sp->dst, &st->dst);
		}

		if (sync < 2) {
			pfsync_alloc_scrub_memory(&sp->dst, &st->dst);
			pf_state_peer_ntoh(&sp->dst, &st->dst);
			st->expire = time_uptime;
			st->timeout = sp->timeout;
		}
		st->pfsync_time = time_uptime;

		if (sync) {
			pfsyncstats.pfsyncs_stale++;

			pfsync_update_state(st);
			schednetisr(NETISR_PFSYNC);
		}
	}

	return (0);
}

int
pfsync_in_upd_c(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_upd_c *up;
	struct pf_state_cmp id_key;
	struct pf_state *st;

	int sync;

	int i;

	for (i = 0; i < count; i++) {
		up = (struct pfsync_upd_c *)(buf + len * i);

		/* check for invalid values */
		if (up->timeout >= PFTM_MAX ||
		    up->src.state > PF_TCPS_PROXY_DST ||
		    up->dst.state > PF_TCPS_PROXY_DST) {
			DPFPRINTF(LOG_NOTICE,
			    "pfsync_input: PFSYNC_ACT_UPD_C: invalid value");
			pfsyncstats.pfsyncs_badval++;
			continue;
		}

		id_key.id = up->id;
		id_key.creatorid = up->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL) {
			/* We don't have this state. Ask for it. */
			pfsync_request_update(id_key.creatorid, id_key.id);
			continue;
		}

		if (ISSET(st->state_flags, PFSTATE_ACK))
			pfsync_deferred(st, 1);

		if (st->key[PF_SK_WIRE]->proto == IPPROTO_TCP)
			sync = pfsync_upd_tcp(st, &up->src, &up->dst);
		else {
			sync = 0;
			/*
			 * Non-TCP protocol state machine always go
			 * forwards
			 */
			if (st->src.state > up->src.state)
				sync++;
			else
				pf_state_peer_ntoh(&up->src, &st->src);

			if (st->dst.state > up->dst.state)
				sync++;
			else
				pf_state_peer_ntoh(&up->dst, &st->dst);
		}
		if (sync < 2) {
			pfsync_alloc_scrub_memory(&up->dst, &st->dst);
			pf_state_peer_ntoh(&up->dst, &st->dst);
			st->expire = time_uptime;
			st->timeout = up->timeout;
		}
		st->pfsync_time = time_uptime;

		if (sync) {
			pfsyncstats.pfsyncs_stale++;

			pfsync_update_state(st);
			schednetisr(NETISR_PFSYNC);
		}
	}

	return (0);
}

int
pfsync_in_ureq(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_upd_req *ur;
	int i;

	struct pf_state_cmp id_key;
	struct pf_state *st;

	for (i = 0; i < count; i++) {
		ur = (struct pfsync_upd_req *)(buf + len * i);

		id_key.id = ur->id;
		id_key.creatorid = ur->creatorid;

		if (id_key.id == 0 && id_key.creatorid == 0)
			pfsync_bulk_start();
		else {
			st = pf_find_state_byid(&id_key);
			if (st == NULL) {
				pfsyncstats.pfsyncs_badstate++;
				continue;
			}
			if (ISSET(st->state_flags, PFSTATE_NOSYNC))
				continue;

			pfsync_update_state_req(st);
		}
	}

	return (0);
}

int
pfsync_in_del(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_state *sp;
	struct pf_state_cmp id_key;
	struct pf_state *st;
	int i;

	for (i = 0; i < count; i++) {
		sp = (struct pfsync_state *)(buf + len * i);

		id_key.id = sp->id;
		id_key.creatorid = sp->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL) {
			pfsyncstats.pfsyncs_badstate++;
			continue;
		}
		SET(st->state_flags, PFSTATE_NOSYNC);
		pf_unlink_state(st);
	}

	return (0);
}

int
pfsync_in_del_c(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_del_c *sp;
	struct pf_state_cmp id_key;
	struct pf_state *st;
	int i;

	for (i = 0; i < count; i++) {
		sp = (struct pfsync_del_c *)(buf + len * i);

		id_key.id = sp->id;
		id_key.creatorid = sp->creatorid;

		st = pf_find_state_byid(&id_key);
		if (st == NULL) {
			pfsyncstats.pfsyncs_badstate++;
			continue;
		}

		SET(st->state_flags, PFSTATE_NOSYNC);
		pf_unlink_state(st);
	}

	return (0);
}

int
pfsync_in_bus(caddr_t buf, int len, int count, int flags)
{
	struct pfsync_softc *sc = pfsyncif;
	struct pfsync_bus *bus;

	/* If we're not waiting for a bulk update, who cares. */
	if (sc->sc_ureq_sent == 0)
		return (0);

	bus = (struct pfsync_bus *)buf;

	switch (bus->status) {
	case PFSYNC_BUS_START:
		timeout_add(&sc->sc_bulkfail_tmo, 4 * hz +
		    pf_pool_limits[PF_LIMIT_STATES].limit /
		    ((sc->sc_if.if_mtu - PFSYNC_MINPKT) /
		    sizeof(struct pfsync_state)));
		DPFPRINTF(LOG_INFO, "received bulk update start");
		break;

	case PFSYNC_BUS_END:
		if (time_uptime - ntohl(bus->endtime) >=
		    sc->sc_ureq_sent) {
			/* that's it, we're happy */
			sc->sc_ureq_sent = 0;
			sc->sc_bulk_tries = 0;
			timeout_del(&sc->sc_bulkfail_tmo);
#if NCARP > 0
			if (!pfsync_sync_ok)
				carp_group_demote_adj(&sc->sc_if, -1,
				    sc->sc_link_demoted ?
				    "pfsync link state up" :
				    "pfsync bulk done");
			if (sc->sc_initial_bulk) {
				carp_group_demote_adj(&sc->sc_if, -32,
				    "pfsync init");
				sc->sc_initial_bulk = 0;
			}
#endif
			pfsync_sync_ok = 1;
			sc->sc_link_demoted = 0;
			DPFPRINTF(LOG_INFO, "received valid bulk update end");
		} else {
			DPFPRINTF(LOG_WARNING, "received invalid "
			    "bulk update end: bad timestamp");
		}
		break;
	}

	return (0);
}

int
pfsync_in_tdb(caddr_t buf, int len, int count, int flags)
{
#if defined(IPSEC)
	struct pfsync_tdb *tp;
	int i;

	for (i = 0; i < count; i++) {
		tp = (struct pfsync_tdb *)(buf + len * i);
		pfsync_update_net_tdb(tp);
	}
#endif

	return (0);
}

#if defined(IPSEC)
/* Update an in-kernel tdb. Silently fail if no tdb is found. */
void
pfsync_update_net_tdb(struct pfsync_tdb *pt)
{
	struct tdb		*tdb;
	int			 s;

	/* check for invalid values */
	if (ntohl(pt->spi) <= SPI_RESERVED_MAX ||
	    (pt->dst.sa.sa_family != AF_INET &&
	     pt->dst.sa.sa_family != AF_INET6))
		goto bad;

	s = splsoftnet();
	tdb = gettdb(ntohs(pt->rdomain), pt->spi,
	    (union sockaddr_union *)&pt->dst, pt->sproto);
	if (tdb) {
		pt->rpl = betoh64(pt->rpl);
		pt->cur_bytes = betoh64(pt->cur_bytes);

		/* Neither replay nor byte counter should ever decrease. */
		if (pt->rpl < tdb->tdb_rpl ||
		    pt->cur_bytes < tdb->tdb_cur_bytes) {
			splx(s);
			goto bad;
		}

		tdb->tdb_rpl = pt->rpl;
		tdb->tdb_cur_bytes = pt->cur_bytes;
	}
	splx(s);
	return;

 bad:
	DPFPRINTF(LOG_WARNING, "pfsync_insert: PFSYNC_ACT_TDB_UPD: "
	    "invalid value");
	pfsyncstats.pfsyncs_badstate++;
	return;
}
#endif


int
pfsync_in_eof(caddr_t buf, int len, int count, int flags)
{
	if (len > 0 || count > 0)
		pfsyncstats.pfsyncs_badact++;

	/* we're done. let the caller return */
	return (1);
}

int
pfsync_in_error(caddr_t buf, int len, int count, int flags)
{
	pfsyncstats.pfsyncs_badact++;
	return (-1);
}

int
pfsyncoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
int
pfsyncioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct proc *p = curproc;
	struct pfsync_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ip_moptions *imo = &sc->sc_imo;
	struct pfsyncreq pfsyncr;
	struct ifnet    *sifp;
	struct ip *ip;
	int s, error;

	switch (cmd) {
#if 0
	case SIOCSIFADDR:
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
#endif
	case SIOCSIFFLAGS:
		s = splnet();
		if ((ifp->if_flags & IFF_RUNNING) == 0 &&
		    (ifp->if_flags & IFF_UP)) {
			ifp->if_flags |= IFF_RUNNING;

#if NCARP > 0
			sc->sc_initial_bulk = 1;
			carp_group_demote_adj(&sc->sc_if, 32, "pfsync init");
#endif

			pfsync_request_full_update(sc);
		}
		if ((ifp->if_flags & IFF_RUNNING) &&
		    (ifp->if_flags & IFF_UP) == 0) {
			ifp->if_flags &= ~IFF_RUNNING;

			/* drop everything */
			timeout_del(&sc->sc_tmo);
			pfsync_drop(sc);

			pfsync_cancel_full_update(sc);
		}
		splx(s);
		break;
	case SIOCSIFMTU:
		if (!sc->sc_sync_if ||
		    ifr->ifr_mtu <= PFSYNC_MINPKT ||
		    ifr->ifr_mtu > sc->sc_sync_if->if_mtu)
			return (EINVAL);
		s = splnet();
		if (ifr->ifr_mtu < ifp->if_mtu)
			pfsync_sendout();
		ifp->if_mtu = ifr->ifr_mtu;
		splx(s);
		break;
	case SIOCGETPFSYNC:
		bzero(&pfsyncr, sizeof(pfsyncr));
		if (sc->sc_sync_if) {
			strlcpy(pfsyncr.pfsyncr_syncdev,
			    sc->sc_sync_if->if_xname, IFNAMSIZ);
		}
		pfsyncr.pfsyncr_syncpeer = sc->sc_sync_peer;
		pfsyncr.pfsyncr_maxupdates = sc->sc_maxupdates;
		pfsyncr.pfsyncr_defer = sc->sc_defer;
		return (copyout(&pfsyncr, ifr->ifr_data, sizeof(pfsyncr)));

	case SIOCSETPFSYNC:
		if ((error = suser(p, 0)) != 0)
			return (error);
		if ((error = copyin(ifr->ifr_data, &pfsyncr, sizeof(pfsyncr))))
			return (error);

		s = splnet();

		if (pfsyncr.pfsyncr_syncpeer.s_addr == 0)
			sc->sc_sync_peer.s_addr = INADDR_PFSYNC_GROUP;
		else
			sc->sc_sync_peer.s_addr =
			    pfsyncr.pfsyncr_syncpeer.s_addr;

		if (pfsyncr.pfsyncr_maxupdates > 255) {
			splx(s);
			return (EINVAL);
		}
		sc->sc_maxupdates = pfsyncr.pfsyncr_maxupdates;

		sc->sc_defer = pfsyncr.pfsyncr_defer;

		if (pfsyncr.pfsyncr_syncdev[0] == 0) {
			if (sc->sc_sync_if)
				hook_disestablish(
				    sc->sc_sync_if->if_linkstatehooks,
				    sc->sc_lhcookie);
			sc->sc_sync_if = NULL;
			if (imo->imo_num_memberships > 0) {
				in_delmulti(imo->imo_membership[
				    --imo->imo_num_memberships]);
				imo->imo_ifidx = 0;
			}
			splx(s);
			break;
		}

		if ((sifp = ifunit(pfsyncr.pfsyncr_syncdev)) == NULL) {
			splx(s);
			return (EINVAL);
		}

		if (sifp->if_mtu < sc->sc_if.if_mtu ||
		    (sc->sc_sync_if != NULL &&
		    sifp->if_mtu < sc->sc_sync_if->if_mtu) ||
		    sifp->if_mtu < MCLBYTES - sizeof(struct ip))
			pfsync_sendout();

		if (sc->sc_sync_if)
			hook_disestablish(
			    sc->sc_sync_if->if_linkstatehooks,
			    sc->sc_lhcookie);
		sc->sc_sync_if = sifp;

		if (imo->imo_num_memberships > 0) {
			in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
			imo->imo_ifidx = 0;
		}

		if (sc->sc_sync_if &&
		    sc->sc_sync_peer.s_addr == INADDR_PFSYNC_GROUP) {
			struct in_addr addr;

			if (!(sc->sc_sync_if->if_flags & IFF_MULTICAST)) {
				sc->sc_sync_if = NULL;
				splx(s);
				return (EADDRNOTAVAIL);
			}

			addr.s_addr = INADDR_PFSYNC_GROUP;

			if ((imo->imo_membership[0] =
			    in_addmulti(&addr, sc->sc_sync_if)) == NULL) {
				sc->sc_sync_if = NULL;
				splx(s);
				return (ENOBUFS);
			}
			imo->imo_num_memberships++;
			imo->imo_ifidx = sc->sc_sync_if->if_index;
			imo->imo_ttl = PFSYNC_DFLTTL;
			imo->imo_loop = 0;
		}

		ip = &sc->sc_template;
		bzero(ip, sizeof(*ip));
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(sc->sc_template) >> 2;
		ip->ip_tos = IPTOS_LOWDELAY;
		/* len and id are set later */
		ip->ip_off = htons(IP_DF);
		ip->ip_ttl = PFSYNC_DFLTTL;
		ip->ip_p = IPPROTO_PFSYNC;
		ip->ip_src.s_addr = INADDR_ANY;
		ip->ip_dst.s_addr = sc->sc_sync_peer.s_addr;

		sc->sc_lhcookie =
		    hook_establish(sc->sc_sync_if->if_linkstatehooks, 1,
		    pfsync_syncdev_state, sc);

		pfsync_request_full_update(sc);
		splx(s);

		break;

	default:
		return (ENOTTY);
	}

	return (0);
}

void
pfsync_out_state(struct pf_state *st, void *buf)
{
	struct pfsync_state *sp = buf;

	pfsync_state_export(sp, st);
}

void
pfsync_out_iack(struct pf_state *st, void *buf)
{
	struct pfsync_ins_ack *iack = buf;

	iack->id = st->id;
	iack->creatorid = st->creatorid;
}

void
pfsync_out_upd_c(struct pf_state *st, void *buf)
{
	struct pfsync_upd_c *up = buf;

	bzero(up, sizeof(*up));
	up->id = st->id;
	pf_state_peer_hton(&st->src, &up->src);
	pf_state_peer_hton(&st->dst, &up->dst);
	up->creatorid = st->creatorid;
	up->timeout = st->timeout;
}

void
pfsync_out_del(struct pf_state *st, void *buf)
{
	struct pfsync_del_c *dp = buf;

	dp->id = st->id;
	dp->creatorid = st->creatorid;

	SET(st->state_flags, PFSTATE_NOSYNC);
}

void
pfsync_drop(struct pfsync_softc *sc)
{
	struct pf_state *st;
	struct pfsync_upd_req_item *ur;
	struct tdb *t;
	int q;

	for (q = 0; q < PFSYNC_S_COUNT; q++) {
		if (TAILQ_EMPTY(&sc->sc_qs[q]))
			continue;

		TAILQ_FOREACH(st, &sc->sc_qs[q], sync_list) {
#ifdef PFSYNC_DEBUG
			KASSERT(st->sync_state == q);
#endif
			st->sync_state = PFSYNC_S_NONE;
		}
		TAILQ_INIT(&sc->sc_qs[q]);
	}

	while ((ur = TAILQ_FIRST(&sc->sc_upd_req_list)) != NULL) {
		TAILQ_REMOVE(&sc->sc_upd_req_list, ur, ur_entry);
		pool_put(&sc->sc_pool, ur);
	}

	sc->sc_plus = NULL;

	if (!TAILQ_EMPTY(&sc->sc_tdb_q)) {
		TAILQ_FOREACH(t, &sc->sc_tdb_q, tdb_sync_entry)
			CLR(t->tdb_flags, TDBF_PFSYNC);

		TAILQ_INIT(&sc->sc_tdb_q);
	}

	sc->sc_len = PFSYNC_MINPKT;
}

void
pfsync_sendout(void)
{
	struct pfsync_softc *sc = pfsyncif;
#if NBPFILTER > 0
	struct ifnet *ifp = &sc->sc_if;
#endif
	struct mbuf *m;
	struct ip *ip;
	struct pfsync_header *ph;
	struct pfsync_subheader *subh;
	struct pf_state *st;
	struct pfsync_upd_req_item *ur;
	struct tdb *t;

	int offset;
	int q, count = 0;

	if (sc == NULL || sc->sc_len == PFSYNC_MINPKT)
		return;

	if (!ISSET(sc->sc_if.if_flags, IFF_RUNNING) ||
#if NBPFILTER > 0
	    (ifp->if_bpf == NULL && sc->sc_sync_if == NULL)) {
#else
	    sc->sc_sync_if == NULL) {
#endif
		pfsync_drop(sc);
		return;
	}

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		sc->sc_if.if_oerrors++;
		pfsyncstats.pfsyncs_onomem++;
		pfsync_drop(sc);
		return;
	}

	if (max_linkhdr + sc->sc_len > MHLEN) {
		MCLGETI(m, M_DONTWAIT, NULL, max_linkhdr + sc->sc_len);
		if (!ISSET(m->m_flags, M_EXT)) {
			m_free(m);
			sc->sc_if.if_oerrors++;
			pfsyncstats.pfsyncs_onomem++;
			pfsync_drop(sc);
			return;
		}
	}
	m->m_data += max_linkhdr;
	m->m_len = m->m_pkthdr.len = sc->sc_len;

	/* build the ip header */
	ip = mtod(m, struct ip *);
	bcopy(&sc->sc_template, ip, sizeof(*ip));
	offset = sizeof(*ip);

	ip->ip_len = htons(m->m_pkthdr.len);
	ip->ip_id = htons(ip_randomid());

	/* build the pfsync header */
	ph = (struct pfsync_header *)(m->m_data + offset);
	bzero(ph, sizeof(*ph));
	offset += sizeof(*ph);

	ph->version = PFSYNC_VERSION;
	ph->len = htons(sc->sc_len - sizeof(*ip));
	bcopy(pf_status.pf_chksum, ph->pfcksum, PF_MD5_DIGEST_LENGTH);

	if (!TAILQ_EMPTY(&sc->sc_upd_req_list)) {
		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		while ((ur = TAILQ_FIRST(&sc->sc_upd_req_list)) != NULL) {
			TAILQ_REMOVE(&sc->sc_upd_req_list, ur, ur_entry);

			bcopy(&ur->ur_msg, m->m_data + offset,
			    sizeof(ur->ur_msg));
			offset += sizeof(ur->ur_msg);

			pool_put(&sc->sc_pool, ur);

			count++;
		}

		bzero(subh, sizeof(*subh));
		subh->len = sizeof(ur->ur_msg) >> 2;
		subh->action = PFSYNC_ACT_UPD_REQ;
		subh->count = htons(count);
	}

	/* has someone built a custom region for us to add? */
	if (sc->sc_plus != NULL) {
		bcopy(sc->sc_plus, m->m_data + offset, sc->sc_pluslen);
		offset += sc->sc_pluslen;

		sc->sc_plus = NULL;
	}

	if (!TAILQ_EMPTY(&sc->sc_tdb_q)) {
		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		TAILQ_FOREACH(t, &sc->sc_tdb_q, tdb_sync_entry) {
			pfsync_out_tdb(t, m->m_data + offset);
			offset += sizeof(struct pfsync_tdb);
			CLR(t->tdb_flags, TDBF_PFSYNC);

			count++;
		}
		TAILQ_INIT(&sc->sc_tdb_q);

		bzero(subh, sizeof(*subh));
		subh->action = PFSYNC_ACT_TDB;
		subh->len = sizeof(struct pfsync_tdb) >> 2;
		subh->count = htons(count);
	}

	/* walk the queues */
	for (q = 0; q < PFSYNC_S_COUNT; q++) {
		if (TAILQ_EMPTY(&sc->sc_qs[q]))
			continue;

		subh = (struct pfsync_subheader *)(m->m_data + offset);
		offset += sizeof(*subh);

		count = 0;
		TAILQ_FOREACH(st, &sc->sc_qs[q], sync_list) {
#ifdef PFSYNC_DEBUG
			KASSERT(st->sync_state == q);
#endif
			pfsync_qs[q].write(st, m->m_data + offset);
			offset += pfsync_qs[q].len;

			st->sync_state = PFSYNC_S_NONE;
			count++;
		}
		TAILQ_INIT(&sc->sc_qs[q]);

		bzero(subh, sizeof(*subh));
		subh->action = pfsync_qs[q].action;
		subh->len = pfsync_qs[q].len >> 2;
		subh->count = htons(count);
	}

	/* we're done, let's put it on the wire */
#if NBPFILTER > 0
	if (ifp->if_bpf) {
		m->m_data += sizeof(*ip);
		m->m_len = m->m_pkthdr.len = sc->sc_len - sizeof(*ip);
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
		m->m_data -= sizeof(*ip);
		m->m_len = m->m_pkthdr.len = sc->sc_len;
	}

	if (sc->sc_sync_if == NULL) {
		sc->sc_len = PFSYNC_MINPKT;
		m_freem(m);
		return;
	}
#endif

	/* start again */
	sc->sc_len = PFSYNC_MINPKT;

	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes += m->m_pkthdr.len;

	m->m_pkthdr.ph_rtableid = sc->sc_if.if_rdomain;

	if (ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL, 0) == 0)
		pfsyncstats.pfsyncs_opackets++;
	else
		pfsyncstats.pfsyncs_oerrors++;
}

void
pfsync_insert_state(struct pf_state *st)
{
	struct pfsync_softc *sc = pfsyncif;

	splsoftassert(IPL_SOFTNET);

	if (ISSET(st->rule.ptr->rule_flag, PFRULE_NOSYNC) ||
	    st->key[PF_SK_WIRE]->proto == IPPROTO_PFSYNC) {
		SET(st->state_flags, PFSTATE_NOSYNC);
		return;
	}

	if (sc == NULL || !ISSET(sc->sc_if.if_flags, IFF_RUNNING) ||
	    ISSET(st->state_flags, PFSTATE_NOSYNC))
		return;

#ifdef PFSYNC_DEBUG
	KASSERT(st->sync_state == PFSYNC_S_NONE);
#endif

	if (sc->sc_len == PFSYNC_MINPKT)
		timeout_add_sec(&sc->sc_tmo, 1);

	pfsync_q_ins(st, PFSYNC_S_INS);

	st->sync_updates = 0;
}

int
pfsync_defer(struct pf_state *st, struct mbuf *m)
{
	struct pfsync_softc *sc = pfsyncif;
	struct pfsync_deferral *pd;

	splsoftassert(IPL_SOFTNET);

	if (!sc->sc_defer ||
	    ISSET(st->state_flags, PFSTATE_NOSYNC) ||
	    m->m_flags & (M_BCAST|M_MCAST))
		return (0);

	if (sc->sc_deferred >= 128) {
		pd = TAILQ_FIRST(&sc->sc_deferrals);
		if (timeout_del(&pd->pd_tmo))
			pfsync_undefer(pd, 0);
	}

	pd = pool_get(&sc->sc_pool, M_NOWAIT);
	if (pd == NULL)
		return (0);

	m->m_pkthdr.pf.flags |= PF_TAG_GENERATED;
	SET(st->state_flags, PFSTATE_ACK);

	pd->pd_st = st;
	pd->pd_m = m;

	sc->sc_deferred++;
	TAILQ_INSERT_TAIL(&sc->sc_deferrals, pd, pd_entry);

	timeout_set(&pd->pd_tmo, pfsync_defer_tmo, pd);
	timeout_add_msec(&pd->pd_tmo, 20);

	schednetisr(NETISR_PFSYNC);

	return (1);
}

void
pfsync_undefer(struct pfsync_deferral *pd, int drop)
{
	struct pfsync_softc *sc = pfsyncif;

	splsoftassert(IPL_SOFTNET);

	TAILQ_REMOVE(&sc->sc_deferrals, pd, pd_entry);
	sc->sc_deferred--;

	CLR(pd->pd_st->state_flags, PFSTATE_ACK);
	if (drop)
		m_freem(pd->pd_m);
	else {
		if (pd->pd_st->rule.ptr->rt == PF_ROUTETO) {
			switch (pd->pd_st->key[PF_SK_WIRE]->af) {
			case AF_INET:
				pf_route(&pd->pd_m, pd->pd_st->rule.ptr,
				    pd->pd_st->direction, 
				    pd->pd_st->rt_kif->pfik_ifp, pd->pd_st);
				break;
#ifdef INET6
			case AF_INET6:
				pf_route6(&pd->pd_m, pd->pd_st->rule.ptr,
				    pd->pd_st->direction,
				    pd->pd_st->rt_kif->pfik_ifp, pd->pd_st);
				break;
#endif /* INET6 */
			}
		} else {
			switch (pd->pd_st->key[PF_SK_WIRE]->af) {
			case AF_INET:
				ip_output(pd->pd_m, NULL, NULL, 0, NULL, NULL,
				    0);
				break;
#ifdef INET6
	                case AF_INET6:
		                ip6_output(pd->pd_m, NULL, NULL, 0,
				    NULL, NULL, NULL);
				break;
#endif /* INET6 */
			}
                }
	}

	pool_put(&sc->sc_pool, pd);
}

void
pfsync_defer_tmo(void *arg)
{
	int s;

	s = splsoftnet();
	pfsync_undefer(arg, 0);
	splx(s);
}

void
pfsync_deferred(struct pf_state *st, int drop)
{
	struct pfsync_softc *sc = pfsyncif;
	struct pfsync_deferral *pd;

	splsoftassert(IPL_SOFTNET);

	TAILQ_FOREACH(pd, &sc->sc_deferrals, pd_entry) {
		 if (pd->pd_st == st) {
			if (timeout_del(&pd->pd_tmo))
				pfsync_undefer(pd, drop);
			return;
		}
	}

	panic("pfsync_deferred: unable to find deferred state");
}

void
pfsync_update_state(struct pf_state *st)
{
	struct pfsync_softc *sc = pfsyncif;
	int sync = 0;

	splsoftassert(IPL_SOFTNET);

	if (sc == NULL || !ISSET(sc->sc_if.if_flags, IFF_RUNNING))
		return;

	if (ISSET(st->state_flags, PFSTATE_ACK))
		pfsync_deferred(st, 0);
	if (ISSET(st->state_flags, PFSTATE_NOSYNC)) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st);
		return;
	}

	if (sc->sc_len == PFSYNC_MINPKT)
		timeout_add_sec(&sc->sc_tmo, 1);

	switch (st->sync_state) {
	case PFSYNC_S_UPD_C:
	case PFSYNC_S_UPD:
	case PFSYNC_S_INS:
		/* we're already handling it */

		if (st->key[PF_SK_WIRE]->proto == IPPROTO_TCP) {
			st->sync_updates++;
			if (st->sync_updates >= sc->sc_maxupdates)
				sync = 1;
		}
		break;

	case PFSYNC_S_IACK:
		pfsync_q_del(st);
	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_UPD_C);
		st->sync_updates = 0;
		break;

	default:
		panic("pfsync_update_state: unexpected sync state %d",
		    st->sync_state);
	}

	if (sync || (time_uptime - st->pfsync_time) < 2)
		schednetisr(NETISR_PFSYNC);
}

void
pfsync_cancel_full_update(struct pfsync_softc *sc)
{
	if (timeout_pending(&sc->sc_bulkfail_tmo) ||
	    timeout_pending(&sc->sc_bulk_tmo)) {
#if NCARP > 0
		if (!pfsync_sync_ok)
			carp_group_demote_adj(&sc->sc_if, -1,
			    "pfsync bulk cancelled");
		if (sc->sc_initial_bulk) {
			carp_group_demote_adj(&sc->sc_if, -32,
			    "pfsync init");
			sc->sc_initial_bulk = 0;
		}
#endif
		pfsync_sync_ok = 1;
		DPFPRINTF(LOG_INFO, "cancelling bulk update");
	}
	timeout_del(&sc->sc_bulkfail_tmo);
	timeout_del(&sc->sc_bulk_tmo);
	sc->sc_bulk_next = NULL;
	sc->sc_bulk_last = NULL;
	sc->sc_ureq_sent = 0;
	sc->sc_bulk_tries = 0;
}

void
pfsync_request_full_update(struct pfsync_softc *sc)
{
	if (sc->sc_sync_if && ISSET(sc->sc_if.if_flags, IFF_RUNNING)) {
		/* Request a full state table update. */
		sc->sc_ureq_sent = time_uptime;
#if NCARP > 0
		if (!sc->sc_link_demoted && pfsync_sync_ok)
			carp_group_demote_adj(&sc->sc_if, 1,
			    "pfsync bulk start");
#endif
		pfsync_sync_ok = 0;
		DPFPRINTF(LOG_INFO, "requesting bulk update");
		timeout_add(&sc->sc_bulkfail_tmo, 4 * hz +
		    pf_pool_limits[PF_LIMIT_STATES].limit /
		    ((sc->sc_if.if_mtu - PFSYNC_MINPKT) /
		    sizeof(struct pfsync_state)));
		pfsync_request_update(0, 0);
	}
}

void
pfsync_request_update(u_int32_t creatorid, u_int64_t id)
{
	struct pfsync_softc *sc = pfsyncif;
	struct pfsync_upd_req_item *item;
	size_t nlen = sizeof(struct pfsync_upd_req);

	/*
	 * this code does nothing to prevent multiple update requests for the
	 * same state being generated.
	 */

	item = pool_get(&sc->sc_pool, PR_NOWAIT);
	if (item == NULL) {
		/* XXX stats */
		return;
	}

	item->ur_msg.id = id;
	item->ur_msg.creatorid = creatorid;

	if (TAILQ_EMPTY(&sc->sc_upd_req_list))
		nlen += sizeof(struct pfsync_subheader);

	if (sc->sc_len + nlen > sc->sc_if.if_mtu) {
		pfsync_sendout();

		nlen = sizeof(struct pfsync_subheader) +
		    sizeof(struct pfsync_upd_req);
	}

	TAILQ_INSERT_TAIL(&sc->sc_upd_req_list, item, ur_entry);
	sc->sc_len += nlen;

	schednetisr(NETISR_PFSYNC);
}

void
pfsync_update_state_req(struct pf_state *st)
{
	struct pfsync_softc *sc = pfsyncif;

	if (sc == NULL)
		panic("pfsync_update_state_req: nonexistant instance");

	if (ISSET(st->state_flags, PFSTATE_NOSYNC)) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st);
		return;
	}

	switch (st->sync_state) {
	case PFSYNC_S_UPD_C:
	case PFSYNC_S_IACK:
		pfsync_q_del(st);
	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_UPD);
		schednetisr(NETISR_PFSYNC);
		return;

	case PFSYNC_S_INS:
	case PFSYNC_S_UPD:
	case PFSYNC_S_DEL:
		/* we're already handling it */
		return;

	default:
		panic("pfsync_update_state_req: unexpected sync state %d",
		    st->sync_state);
	}
}

void
pfsync_delete_state(struct pf_state *st)
{
	struct pfsync_softc *sc = pfsyncif;

	splsoftassert(IPL_SOFTNET);

	if (sc == NULL || !ISSET(sc->sc_if.if_flags, IFF_RUNNING))
		return;

	if (ISSET(st->state_flags, PFSTATE_ACK))
		pfsync_deferred(st, 1);
	if (ISSET(st->state_flags, PFSTATE_NOSYNC)) {
		if (st->sync_state != PFSYNC_S_NONE)
			pfsync_q_del(st);
		return;
	}

	if (sc->sc_len == PFSYNC_MINPKT)
		timeout_add_sec(&sc->sc_tmo, 1);

	switch (st->sync_state) {
	case PFSYNC_S_INS:
		/* we never got to tell the world so just forget about it */
		pfsync_q_del(st);
		return;

	case PFSYNC_S_UPD_C:
	case PFSYNC_S_UPD:
	case PFSYNC_S_IACK:
		pfsync_q_del(st);
		/* FALLTHROUGH to putting it on the del list */

	case PFSYNC_S_NONE:
		pfsync_q_ins(st, PFSYNC_S_DEL);
		return;

	default:
		panic("pfsync_delete_state: unexpected sync state %d",
		    st->sync_state);
	}
}

void
pfsync_clear_states(u_int32_t creatorid, const char *ifname)
{
	struct pfsync_softc *sc = pfsyncif;
	struct {
		struct pfsync_subheader subh;
		struct pfsync_clr clr;
	} __packed r;

	splsoftassert(IPL_SOFTNET);

	if (sc == NULL || !ISSET(sc->sc_if.if_flags, IFF_RUNNING))
		return;

	bzero(&r, sizeof(r));

	r.subh.action = PFSYNC_ACT_CLR;
	r.subh.len = sizeof(struct pfsync_clr) >> 2;
	r.subh.count = htons(1);

	strlcpy(r.clr.ifname, ifname, sizeof(r.clr.ifname));
	r.clr.creatorid = creatorid;

	pfsync_send_plus(&r, sizeof(r));
}

void
pfsync_q_ins(struct pf_state *st, int q)
{
	struct pfsync_softc *sc = pfsyncif;
	size_t nlen = pfsync_qs[q].len;

	KASSERT(st->sync_state == PFSYNC_S_NONE);

#if defined(PFSYNC_DEBUG)
	if (sc->sc_len < PFSYNC_MINPKT)
		panic("pfsync pkt len is too low %d", sc->sc_len);
#endif
	if (TAILQ_EMPTY(&sc->sc_qs[q]))
		nlen += sizeof(struct pfsync_subheader);

	if (sc->sc_len + nlen > sc->sc_if.if_mtu) {
		pfsync_sendout();

		nlen = sizeof(struct pfsync_subheader) + pfsync_qs[q].len;
	}

	sc->sc_len += nlen;
	TAILQ_INSERT_TAIL(&sc->sc_qs[q], st, sync_list);
	st->sync_state = q;
}

void
pfsync_q_del(struct pf_state *st)
{
	struct pfsync_softc *sc = pfsyncif;
	int q = st->sync_state;

	KASSERT(st->sync_state != PFSYNC_S_NONE);

	sc->sc_len -= pfsync_qs[q].len;
	TAILQ_REMOVE(&sc->sc_qs[q], st, sync_list);
	st->sync_state = PFSYNC_S_NONE;

	if (TAILQ_EMPTY(&sc->sc_qs[q]))
		sc->sc_len -= sizeof(struct pfsync_subheader);
}

void
pfsync_update_tdb(struct tdb *t, int output)
{
	struct pfsync_softc *sc = pfsyncif;
	size_t nlen = sizeof(struct pfsync_tdb);

	if (sc == NULL)
		return;

	if (!ISSET(t->tdb_flags, TDBF_PFSYNC)) {
		if (TAILQ_EMPTY(&sc->sc_tdb_q))
			nlen += sizeof(struct pfsync_subheader);

		if (sc->sc_len + nlen > sc->sc_if.if_mtu) {
			pfsync_sendout();

			nlen = sizeof(struct pfsync_subheader) +
			    sizeof(struct pfsync_tdb);
		}

		sc->sc_len += nlen;
		TAILQ_INSERT_TAIL(&sc->sc_tdb_q, t, tdb_sync_entry);
		SET(t->tdb_flags, TDBF_PFSYNC);
		t->tdb_updates = 0;
	} else {
		if (++t->tdb_updates >= sc->sc_maxupdates)
			schednetisr(NETISR_PFSYNC);
	}

	if (output)
		SET(t->tdb_flags, TDBF_PFSYNC_RPL);
	else
		CLR(t->tdb_flags, TDBF_PFSYNC_RPL);
}

void
pfsync_delete_tdb(struct tdb *t)
{
	struct pfsync_softc *sc = pfsyncif;

	if (sc == NULL || !ISSET(t->tdb_flags, TDBF_PFSYNC))
		return;

	sc->sc_len -= sizeof(struct pfsync_tdb);
	TAILQ_REMOVE(&sc->sc_tdb_q, t, tdb_sync_entry);
	CLR(t->tdb_flags, TDBF_PFSYNC);

	if (TAILQ_EMPTY(&sc->sc_tdb_q))
		sc->sc_len -= sizeof(struct pfsync_subheader);
}

void
pfsync_out_tdb(struct tdb *t, void *buf)
{
	struct pfsync_tdb *ut = buf;

	bzero(ut, sizeof(*ut));
	ut->spi = t->tdb_spi;
	bcopy(&t->tdb_dst, &ut->dst, sizeof(ut->dst));
	/*
	 * When a failover happens, the master's rpl is probably above
	 * what we see here (we may be up to a second late), so
	 * increase it a bit for outbound tdbs to manage most such
	 * situations.
	 *
	 * For now, just add an offset that is likely to be larger
	 * than the number of packets we can see in one second. The RFC
	 * just says the next packet must have a higher seq value.
	 *
	 * XXX What is a good algorithm for this? We could use
	 * a rate-determined increase, but to know it, we would have
	 * to extend struct tdb.
	 * XXX pt->rpl can wrap over MAXINT, but if so the real tdb
	 * will soon be replaced anyway. For now, just don't handle
	 * this edge case.
	 */
#define RPL_INCR 16384
	ut->rpl = htobe64(t->tdb_rpl + (ISSET(t->tdb_flags, TDBF_PFSYNC_RPL) ?
	    RPL_INCR : 0));
	ut->cur_bytes = htobe64(t->tdb_cur_bytes);
	ut->sproto = t->tdb_sproto;
	ut->rdomain = htons(t->tdb_rdomain);
}

void
pfsync_bulk_start(void)
{
	struct pfsync_softc *sc = pfsyncif;

	DPFPRINTF(LOG_INFO, "received bulk update request");

	if (TAILQ_EMPTY(&state_list))
		pfsync_bulk_status(PFSYNC_BUS_END);
	else {
		sc->sc_ureq_received = time_uptime;

		if (sc->sc_bulk_next == NULL)
			sc->sc_bulk_next = TAILQ_FIRST(&state_list);
		sc->sc_bulk_last = sc->sc_bulk_next;

		pfsync_bulk_status(PFSYNC_BUS_START);
		timeout_add(&sc->sc_bulk_tmo, 0);
	}
}

void
pfsync_bulk_update(void *arg)
{
	struct pfsync_softc *sc = arg;
	struct pf_state *st;
	int i = 0;
	int s;

	s = splsoftnet();

	st = sc->sc_bulk_next;

	for (;;) {
		if (st->sync_state == PFSYNC_S_NONE &&
		    st->timeout < PFTM_MAX &&
		    st->pfsync_time <= sc->sc_ureq_received) {
			pfsync_update_state_req(st);
			i++;
		}

		st = TAILQ_NEXT(st, entry_list);
		if (st == NULL)
			st = TAILQ_FIRST(&state_list);

		if (st == sc->sc_bulk_last) {
			/* we're done */
			sc->sc_bulk_next = NULL;
			sc->sc_bulk_last = NULL;
			pfsync_bulk_status(PFSYNC_BUS_END);
			break;
		}

		if (i > 1 && (sc->sc_if.if_mtu - sc->sc_len) <
		    sizeof(struct pfsync_state)) {
			/* we've filled a packet */
			sc->sc_bulk_next = st;
			timeout_add(&sc->sc_bulk_tmo, 1);
			break;
		}
	}

	splx(s);
}

void
pfsync_bulk_status(u_int8_t status)
{
	struct {
		struct pfsync_subheader subh;
		struct pfsync_bus bus;
	} __packed r;

	struct pfsync_softc *sc = pfsyncif;

	bzero(&r, sizeof(r));

	r.subh.action = PFSYNC_ACT_BUS;
	r.subh.len = sizeof(struct pfsync_bus) >> 2;
	r.subh.count = htons(1);

	r.bus.creatorid = pf_status.hostid;
	r.bus.endtime = htonl(time_uptime - sc->sc_ureq_received);
	r.bus.status = status;

	pfsync_send_plus(&r, sizeof(r));
}

void
pfsync_bulk_fail(void *arg)
{
	struct pfsync_softc *sc = arg;
	int s;

	s = splsoftnet();

	if (sc->sc_bulk_tries++ < PFSYNC_MAX_BULKTRIES) {
		/* Try again */
		timeout_add_sec(&sc->sc_bulkfail_tmo, 5);
		pfsync_request_update(0, 0);
	} else {
		/* Pretend like the transfer was ok */
		sc->sc_ureq_sent = 0;
		sc->sc_bulk_tries = 0;
#if NCARP > 0
		if (!pfsync_sync_ok)
			carp_group_demote_adj(&sc->sc_if, -1,
			    sc->sc_link_demoted ?
			    "pfsync link state up" :
			    "pfsync bulk fail");
		if (sc->sc_initial_bulk) {
			carp_group_demote_adj(&sc->sc_if, -32,
			    "pfsync init");
			sc->sc_initial_bulk = 0;
		}
#endif
		pfsync_sync_ok = 1;
		sc->sc_link_demoted = 0;
		DPFPRINTF(LOG_ERR, "failed to receive bulk update");
	}

	splx(s);
}

void
pfsync_send_plus(void *plus, size_t pluslen)
{
	struct pfsync_softc *sc = pfsyncif;

	if (sc->sc_len + pluslen > sc->sc_if.if_mtu)
		pfsync_sendout();

	sc->sc_plus = plus;
	sc->sc_len += (sc->sc_pluslen = pluslen);

	pfsync_sendout();
}

int
pfsync_up(void)
{
	struct pfsync_softc *sc = pfsyncif;

	if (sc == NULL || !ISSET(sc->sc_if.if_flags, IFF_RUNNING))
		return (0);

	return (1);
}

int
pfsync_state_in_use(struct pf_state *st)
{
	struct pfsync_softc *sc = pfsyncif;

	if (sc == NULL)
		return (0);

	if (st->sync_state != PFSYNC_S_NONE ||
	    st == sc->sc_bulk_next ||
	    st == sc->sc_bulk_last)
		return (1);

	return (0);
}

void
pfsync_timeout(void *arg)
{
	int s;

	s = splsoftnet();
	pfsync_sendout();
	splx(s);
}

/* this is a softnet/netisr handler */
void
pfsyncintr(void)
{
	pfsync_sendout();
}

int
pfsync_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case PFSYNCCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &pfsyncstats, sizeof(pfsyncstats)));
	default:
		return (ENOPROTOOPT);
	}
}
