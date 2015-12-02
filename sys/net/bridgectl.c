/*	$OpenBSD: bridgectl.c,v 1.2 2015/12/02 08:04:12 mpi Exp $	*/

/*
 * Copyright (c) 1999, 2000 Jason L. Wright (jason@thought.net)
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
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN
 * ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Effort sponsored in part by the Defense Advanced Research Projects
 * Agency (DARPA) and Air Force Research Laboratory, Air Force
 * Materiel Command, USAF, under agreement number F30602-01-2-0537.
 *
 */

#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/timeout.h>
#include <sys/kernel.h>

#include <crypto/siphash.h>

#include <net/if.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/if_bridge.h>


int	bridge_rtfind(struct bridge_softc *, struct ifbaconf *);
void	bridge_rtage(struct bridge_softc *);
int	bridge_rtdaddr(struct bridge_softc *, struct ether_addr *);
u_int32_t bridge_hash(struct bridge_softc *, struct ether_addr *);

int	bridge_brlconf(struct bridge_softc *, struct ifbrlconf *);
int	bridge_addrule(struct bridge_iflist *, struct ifbrlreq *, int out);

int
bridgectl_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bridge_softc *sc = (struct bridge_softc *)ifp->if_softc;
	struct ifbreq *req = (struct ifbreq *)data;
	struct ifbrlreq *brlreq = (struct ifbrlreq *)data;
	struct ifbareq *bareq = (struct ifbareq *)data;
	struct ifbrparam *bparam = (struct ifbrparam *)data;
	struct bridge_iflist *p;
	struct ifnet *ifs;
	int error = 0;

	switch (cmd) {
	case SIOCBRDGRTS:
		error = bridge_rtfind(sc, (struct ifbaconf *)data);
		break;
	case SIOCBRDGFLUSH:
		bridge_rtflush(sc, req->ifbr_ifsflags);
		break;
	case SIOCBRDGSADDR:
		ifs = ifunit(bareq->ifba_ifsname);
		if (ifs == NULL) {			/* no such interface */
			error = ENOENT;
			break;
		}
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
			error = ESRCH;
			break;
		}

		ifs = bridge_rtupdate(sc, &bareq->ifba_dst, ifs, 1,
		    bareq->ifba_flags, NULL);
		if (ifs == NULL)
			error = ENOMEM;
		break;
	case SIOCBRDGDADDR:
		error = bridge_rtdaddr(sc, &bareq->ifba_dst);
		break;
	case SIOCBRDGGCACHE:
		bparam->ifbrp_csize = sc->sc_brtmax;
		break;
	case SIOCBRDGSCACHE:
		sc->sc_brtmax = bparam->ifbrp_csize;
		break;
	case SIOCBRDGSTO:
		if (bparam->ifbrp_ctime < 0 ||
		    bparam->ifbrp_ctime > INT_MAX / hz) {
			error = EINVAL;
			break;
		}
		sc->sc_brttimeout = bparam->ifbrp_ctime;
		if (bparam->ifbrp_ctime != 0)
			timeout_add_sec(&sc->sc_brtimeout, sc->sc_brttimeout);
		else
			timeout_del(&sc->sc_brtimeout);
		break;
	case SIOCBRDGGTO:
		bparam->ifbrp_ctime = sc->sc_brttimeout;
		break;
	case SIOCBRDGARL:
		ifs = ifunit(brlreq->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		if ((brlreq->ifbr_action != BRL_ACTION_BLOCK &&
		    brlreq->ifbr_action != BRL_ACTION_PASS) ||
		    (brlreq->ifbr_flags & (BRL_FLAG_IN|BRL_FLAG_OUT)) == 0) {
			error = EINVAL;
			break;
		}
		if (brlreq->ifbr_flags & BRL_FLAG_IN) {
			error = bridge_addrule(p, brlreq, 0);
			if (error)
				break;
		}
		if (brlreq->ifbr_flags & BRL_FLAG_OUT) {
			error = bridge_addrule(p, brlreq, 1);
			if (error)
				break;
		}
		break;
	case SIOCBRDGFRL:
		ifs = ifunit(brlreq->ifbr_ifsname);
		if (ifs == NULL) {
			error = ENOENT;
			break;
		}
		p = (struct bridge_iflist *)ifs->if_bridgeport;
		if (p == NULL || p->bridge_sc != sc) {
			error = ESRCH;
			break;
		}
		bridge_flushrule(p);
		break;
	case SIOCBRDGGRL:
		error = bridge_brlconf(sc, (struct ifbrlconf *)data);
		break;
	default:
		break;
	}

	return (error);
}

struct ifnet *
bridge_rtupdate(struct bridge_softc *sc, struct ether_addr *ea,
    struct ifnet *ifp, int setflags, u_int8_t flags, struct mbuf *m)
{
	struct bridge_rtnode *p, *q;
	struct sockaddr *sa = NULL;
	u_int32_t h;
	int dir;

	if (m != NULL) {
		/* Check if the mbuf was tagged with a tunnel endpoint addr */
		sa = bridge_tunnel(m);
	}

	h = bridge_hash(sc, ea);
	p = LIST_FIRST(&sc->sc_rts[h]);
	if (p == NULL) {
		if (sc->sc_brtcnt >= sc->sc_brtmax)
			goto done;
		p = malloc(sizeof(*p), M_DEVBUF, M_NOWAIT);
		if (p == NULL)
			goto done;

		bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
		p->brt_if = ifp;
		p->brt_age = 1;
		bridge_copyaddr(sa, (struct sockaddr *)&p->brt_tunnel);

		if (setflags)
			p->brt_flags = flags;
		else
			p->brt_flags = IFBAF_DYNAMIC;

		LIST_INSERT_HEAD(&sc->sc_rts[h], p, brt_next);
		sc->sc_brtcnt++;
		goto want;
	}

	do {
		q = p;
		p = LIST_NEXT(p, brt_next);

		dir = memcmp(ea, &q->brt_addr, sizeof(q->brt_addr));
		if (dir == 0) {
			if (setflags) {
				q->brt_if = ifp;
				q->brt_flags = flags;
			} else if (!(q->brt_flags & IFBAF_STATIC))
				q->brt_if = ifp;

			if (q->brt_if == ifp)
				q->brt_age = 1;
			ifp = q->brt_if;
			bridge_copyaddr(sa,
			     (struct sockaddr *)&q->brt_tunnel);

			goto want;
		}

		if (dir > 0) {
			if (sc->sc_brtcnt >= sc->sc_brtmax)
				goto done;
			p = malloc(sizeof(*p), M_DEVBUF, M_NOWAIT);
			if (p == NULL)
				goto done;

			bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
			p->brt_if = ifp;
			p->brt_age = 1;
			bridge_copyaddr(sa,
			    (struct sockaddr *)&p->brt_tunnel);

			if (setflags)
				p->brt_flags = flags;
			else
				p->brt_flags = IFBAF_DYNAMIC;

			LIST_INSERT_BEFORE(q, p, brt_next);
			sc->sc_brtcnt++;
			goto want;
		}

		if (p == NULL) {
			if (sc->sc_brtcnt >= sc->sc_brtmax)
				goto done;
			p = malloc(sizeof(*p), M_DEVBUF, M_NOWAIT);
			if (p == NULL)
				goto done;

			bcopy(ea, &p->brt_addr, sizeof(p->brt_addr));
			p->brt_if = ifp;
			p->brt_age = 1;
			bridge_copyaddr(sa,
			    (struct sockaddr *)&p->brt_tunnel);

			if (setflags)
				p->brt_flags = flags;
			else
				p->brt_flags = IFBAF_DYNAMIC;
			LIST_INSERT_AFTER(q, p, brt_next);
			sc->sc_brtcnt++;
			goto want;
		}
	} while (p != NULL);

done:
	ifp = NULL;
want:
	return (ifp);
}

struct bridge_rtnode *
bridge_rtlookup(struct bridge_softc *sc, struct ether_addr *ea)
{
	struct bridge_rtnode *p;
	u_int32_t h;
	int dir;

	h = bridge_hash(sc, ea);
	LIST_FOREACH(p, &sc->sc_rts[h], brt_next) {
		dir = memcmp(ea, &p->brt_addr, sizeof(p->brt_addr));
		if (dir == 0)
			return (p);
		if (dir > 0)
			goto fail;
	}
fail:
	return (NULL);
}

u_int32_t
bridge_hash(struct bridge_softc *sc, struct ether_addr *addr)
{
	return SipHash24((SIPHASH_KEY *)sc->sc_hashkey, addr, ETHER_ADDR_LEN) &
	    BRIDGE_RTABLE_MASK;
}

void
bridge_timer(void *vsc)
{
	struct bridge_softc *sc = vsc;
	int s;

	s = splsoftnet();
	bridge_rtage(sc);
	splx(s);
}

/*
 * Perform an aging cycle
 */
void
bridge_rtage(struct bridge_softc *sc)
{
	struct bridge_rtnode *n, *p;
	int i;

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if ((n->brt_flags & IFBAF_TYPEMASK) == IFBAF_STATIC) {
				n->brt_age = !n->brt_age;
				if (n->brt_age)
					n->brt_age = 0;
				n = LIST_NEXT(n, brt_next);
			} else if (n->brt_age) {
				n->brt_age = 0;
				n = LIST_NEXT(n, brt_next);
			} else {
				p = LIST_NEXT(n, brt_next);
				LIST_REMOVE(n, brt_next);
				sc->sc_brtcnt--;
				free(n, M_DEVBUF, sizeof *n);
				n = p;
			}
		}
	}

	if (sc->sc_brttimeout != 0)
		timeout_add_sec(&sc->sc_brtimeout, sc->sc_brttimeout);
}

void
bridge_rtagenode(struct ifnet *ifp, int age)
{
	struct bridge_softc *sc;
	struct bridge_rtnode *n;
	int i;

	sc = ((struct bridge_iflist *)ifp->if_bridgeport)->bridge_sc;
	if (sc == NULL)
		return;

	/*
	 * If the age is zero then flush, otherwise set all the expiry times to
	 * age for the interface
	 */
	if (age == 0)
		bridge_rtdelete(sc, ifp, 1);
	else {
		for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
			LIST_FOREACH(n, &sc->sc_rts[i], brt_next) {
				/* Cap the expiry time to 'age' */
				if (n->brt_if == ifp &&
				    n->brt_age > time_uptime + age &&
				    (n->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC)
					n->brt_age = time_uptime + age;
			}
		}
	}
}

/*
 * Remove all dynamic addresses from the cache
 */
void
bridge_rtflush(struct bridge_softc *sc, int full)
{
	int i;
	struct bridge_rtnode *p, *n;

	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if (full ||
			    (n->brt_flags & IFBAF_TYPEMASK) == IFBAF_DYNAMIC) {
				p = LIST_NEXT(n, brt_next);
				LIST_REMOVE(n, brt_next);
				sc->sc_brtcnt--;
				free(n, M_DEVBUF, sizeof *n);
				n = p;
			} else
				n = LIST_NEXT(n, brt_next);
		}
	}
}

/*
 * Remove an address from the cache
 */
int
bridge_rtdaddr(struct bridge_softc *sc, struct ether_addr *ea)
{
	int h;
	struct bridge_rtnode *p;

	h = bridge_hash(sc, ea);
	LIST_FOREACH(p, &sc->sc_rts[h], brt_next) {
		if (bcmp(ea, &p->brt_addr, sizeof(p->brt_addr)) == 0) {
			LIST_REMOVE(p, brt_next);
			sc->sc_brtcnt--;
			free(p, M_DEVBUF, sizeof *p);
			return (0);
		}
	}

	return (ENOENT);
}

/*
 * Delete routes to a specific interface member.
 */
void
bridge_rtdelete(struct bridge_softc *sc, struct ifnet *ifp, int dynonly)
{
	int i;
	struct bridge_rtnode *n, *p;

	/*
	 * Loop through all of the hash buckets and traverse each
	 * chain looking for routes to this interface.
	 */
	for (i = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		n = LIST_FIRST(&sc->sc_rts[i]);
		while (n != NULL) {
			if (n->brt_if != ifp) {
				/* Not ours */
				n = LIST_NEXT(n, brt_next);
				continue;
			}
			if (dynonly &&
			    (n->brt_flags & IFBAF_TYPEMASK) != IFBAF_DYNAMIC) {
				/* only deleting dynamics */
				n = LIST_NEXT(n, brt_next);
				continue;
			}
			p = LIST_NEXT(n, brt_next);
			LIST_REMOVE(n, brt_next);
			sc->sc_brtcnt--;
			free(n, M_DEVBUF, sizeof *n);
			n = p;
		}
	}
}

/*
 * Gather all of the routes for this interface.
 */
int
bridge_rtfind(struct bridge_softc *sc, struct ifbaconf *baconf)
{
	int i, error = 0, onlycnt = 0;
	u_int32_t cnt = 0;
	struct bridge_rtnode *n;
	struct ifbareq bareq;

	if (baconf->ifbac_len == 0)
		onlycnt = 1;

	for (i = 0, cnt = 0; i < BRIDGE_RTABLE_SIZE; i++) {
		LIST_FOREACH(n, &sc->sc_rts[i], brt_next) {
			if (!onlycnt) {
				if (baconf->ifbac_len < sizeof(struct ifbareq))
					goto done;
				bcopy(sc->sc_if.if_xname, bareq.ifba_name,
				    sizeof(bareq.ifba_name));
				bcopy(n->brt_if->if_xname, bareq.ifba_ifsname,
				    sizeof(bareq.ifba_ifsname));
				bcopy(&n->brt_addr, &bareq.ifba_dst,
				    sizeof(bareq.ifba_dst));
				bridge_copyaddr(&n->brt_tunnel.sa,
				    (struct sockaddr *)&bareq.ifba_dstsa);
				bareq.ifba_age = n->brt_age;
				bareq.ifba_flags = n->brt_flags;
				error = copyout((caddr_t)&bareq,
				    (caddr_t)(baconf->ifbac_req + cnt), sizeof(bareq));
				if (error)
					goto done;
				baconf->ifbac_len -= sizeof(struct ifbareq);
			}
			cnt++;
		}
	}
done:
	baconf->ifbac_len = cnt * sizeof(struct ifbareq);
	return (error);
}

void
bridge_update(struct ifnet *ifp, struct ether_addr *ea, int delete)
{
	struct bridge_softc *sc;
	struct bridge_iflist *bif;
	u_int8_t *addr;

	addr = (u_int8_t *)ea;

	bif = (struct bridge_iflist *)ifp->if_bridgeport;
	sc = bif->bridge_sc;

	/*
	 * Update the bridge interface if it is in
	 * the learning state.
	 */
	if ((bif->bif_flags & IFBIF_LEARNING) &&
	    (ETHER_IS_MULTICAST(addr) == 0) &&
	    !(addr[0] == 0 && addr[1] == 0 && addr[2] == 0 &&
	      addr[3] == 0 && addr[4] == 0 && addr[5] == 0)) {
		/* Care must be taken with spanning tree */
		if ((bif->bif_flags & IFBIF_STP) &&
		    (bif->bif_state == BSTP_IFSTATE_DISCARDING))
			return;

		/* Delete the address from the bridge */
		bridge_rtdaddr(sc, ea);

		if (!delete) {
			/* Update the bridge table */
			bridge_rtupdate(sc, ea, ifp, 0, IFBAF_DYNAMIC, NULL);
		}
	}
}

/*
 * bridge filter/matching rules
 */
int
bridge_brlconf(struct bridge_softc *sc, struct ifbrlconf *bc)
{
	struct ifnet *ifp;
	struct bridge_iflist *ifl;
	struct brl_node *n;
	struct ifbrlreq req;
	int error = 0;
	u_int32_t i = 0, total = 0;

	ifp = ifunit(bc->ifbrl_ifsname);
	if (ifp == NULL)
		return (ENOENT);
	ifl = (struct bridge_iflist *)ifp->if_bridgeport;
	if (ifl == NULL || ifl->bridge_sc != sc)
		return (ESRCH);

	SIMPLEQ_FOREACH(n, &ifl->bif_brlin, brl_next) {
		total++;
	}
	SIMPLEQ_FOREACH(n, &ifl->bif_brlout, brl_next) {
		total++;
	}

	if (bc->ifbrl_len == 0) {
		i = total;
		goto done;
	}

	SIMPLEQ_FOREACH(n, &ifl->bif_brlin, brl_next) {
		bzero(&req, sizeof req);
		if (bc->ifbrl_len < sizeof(req))
			goto done;
		strlcpy(req.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(req.ifbr_ifsname, ifl->ifp->if_xname, IFNAMSIZ);
		req.ifbr_action = n->brl_action;
		req.ifbr_flags = n->brl_flags;
		req.ifbr_src = n->brl_src;
		req.ifbr_dst = n->brl_dst;
#if NPF > 0
		req.ifbr_tagname[0] = '\0';
		if (n->brl_tag)
			pf_tag2tagname(n->brl_tag, req.ifbr_tagname);
#endif
		error = copyout((caddr_t)&req,
		    (caddr_t)(bc->ifbrl_buf + (i * sizeof(req))), sizeof(req));
		if (error)
			goto done;
		i++;
		bc->ifbrl_len -= sizeof(req);
	}

	SIMPLEQ_FOREACH(n, &ifl->bif_brlout, brl_next) {
		bzero(&req, sizeof req);
		if (bc->ifbrl_len < sizeof(req))
			goto done;
		strlcpy(req.ifbr_name, sc->sc_if.if_xname, IFNAMSIZ);
		strlcpy(req.ifbr_ifsname, ifl->ifp->if_xname, IFNAMSIZ);
		req.ifbr_action = n->brl_action;
		req.ifbr_flags = n->brl_flags;
		req.ifbr_src = n->brl_src;
		req.ifbr_dst = n->brl_dst;
#if NPF > 0
		req.ifbr_tagname[0] = '\0';
		if (n->brl_tag)
			pf_tag2tagname(n->brl_tag, req.ifbr_tagname);
#endif
		error = copyout((caddr_t)&req,
		    (caddr_t)(bc->ifbrl_buf + (i * sizeof(req))), sizeof(req));
		if (error)
			goto done;
		i++;
		bc->ifbrl_len -= sizeof(req);
	}

done:
	bc->ifbrl_len = i * sizeof(req);
	return (error);
}

u_int8_t
bridge_filterrule(struct brl_head *h, struct ether_header *eh, struct mbuf *m)
{
	struct brl_node *n;
	u_int8_t flags;

	SIMPLEQ_FOREACH(n, h, brl_next) {
		flags = n->brl_flags & (BRL_FLAG_SRCVALID|BRL_FLAG_DSTVALID);
		if (flags == 0)
			goto return_action;
		if (flags == (BRL_FLAG_SRCVALID|BRL_FLAG_DSTVALID)) {
			if (bcmp(eh->ether_shost, &n->brl_src, ETHER_ADDR_LEN))
				continue;
			if (bcmp(eh->ether_dhost, &n->brl_dst, ETHER_ADDR_LEN))
				continue;
			goto return_action;
		}
		if (flags == BRL_FLAG_SRCVALID) {
			if (bcmp(eh->ether_shost, &n->brl_src, ETHER_ADDR_LEN))
				continue;
			goto return_action;
		}
		if (flags == BRL_FLAG_DSTVALID) {
			if (bcmp(eh->ether_dhost, &n->brl_dst, ETHER_ADDR_LEN))
				continue;
			goto return_action;
		}
	}
	return (BRL_ACTION_PASS);

return_action:
#if NPF > 0
	pf_tag_packet(m, n->brl_tag, -1);
#endif
	return (n->brl_action);
}

int
bridge_addrule(struct bridge_iflist *bif, struct ifbrlreq *req, int out)
{
	struct brl_node *n;

	n = malloc(sizeof(*n), M_DEVBUF, M_NOWAIT);
	if (n == NULL)
		return (ENOMEM);
	bcopy(&req->ifbr_src, &n->brl_src, sizeof(struct ether_addr));
	bcopy(&req->ifbr_dst, &n->brl_dst, sizeof(struct ether_addr));
	n->brl_action = req->ifbr_action;
	n->brl_flags = req->ifbr_flags;
#if NPF > 0
	if (req->ifbr_tagname[0])
		n->brl_tag = pf_tagname2tag(req->ifbr_tagname, 1);
	else
		n->brl_tag = 0;
#endif
	if (out) {
		n->brl_flags &= ~BRL_FLAG_IN;
		n->brl_flags |= BRL_FLAG_OUT;
		SIMPLEQ_INSERT_TAIL(&bif->bif_brlout, n, brl_next);
	} else {
		n->brl_flags &= ~BRL_FLAG_OUT;
		n->brl_flags |= BRL_FLAG_IN;
		SIMPLEQ_INSERT_TAIL(&bif->bif_brlin, n, brl_next);
	}
	return (0);
}

void
bridge_flushrule(struct bridge_iflist *bif)
{
	struct brl_node *p;

	while (!SIMPLEQ_EMPTY(&bif->bif_brlin)) {
		p = SIMPLEQ_FIRST(&bif->bif_brlin);
		SIMPLEQ_REMOVE_HEAD(&bif->bif_brlin, brl_next);
#if NPF > 0
		pf_tag_unref(p->brl_tag);
#endif
		free(p, M_DEVBUF, sizeof *p);
	}
	while (!SIMPLEQ_EMPTY(&bif->bif_brlout)) {
		p = SIMPLEQ_FIRST(&bif->bif_brlout);
		SIMPLEQ_REMOVE_HEAD(&bif->bif_brlout, brl_next);
#if NPF > 0
		pf_tag_unref(p->brl_tag);
#endif
		free(p, M_DEVBUF, sizeof *p);
	}
}
