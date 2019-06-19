/* $OpenBSD: ip_spd.c,v 1.99 2018/10/22 15:32:19 cheloha Exp $ */
/*
 * The author of this code is Angelos D. Keromytis (angelos@cis.upenn.edu)
 *
 * Copyright (c) 2000-2001 Angelos D. Keromytis.
 *
 * Permission to use, copy, and modify this software with or without fee
 * is hereby granted, provided that this entire notice is included in
 * all copies of any software which is or includes a copy or
 * modification of this software.
 * You may use this code under the GNU public license if you so wish. Please
 * contribute changes back to the authors under this freer than GPL license
 * so that we may further the use of strong encryption without limitations to
 * all.
 *
 * THIS SOFTWARE IS BEING PROVIDED "AS IS", WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTY. IN PARTICULAR, NONE OF THE AUTHORS MAKES ANY
 * REPRESENTATION OR WARRANTY OF ANY KIND CONCERNING THE
 * MERCHANTABILITY OF THIS SOFTWARE OR ITS FITNESS FOR ANY PARTICULAR
 * PURPOSE.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/kernel.h>
#include <sys/socketvar.h>
#include <sys/domain.h>
#include <sys/protosw.h>
#include <sys/pool.h>
#include <sys/timeout.h>

#include <net/route.h>
#include <net/netisr.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>

int	ipsp_acquire_sa(struct ipsec_policy *, union sockaddr_union *,
	    union sockaddr_union *, struct sockaddr_encap *, struct mbuf *);
struct	ipsec_acquire *ipsp_pending_acquire(struct ipsec_policy *,
	    union sockaddr_union *);
void	ipsp_delete_acquire_timo(void *);
void	ipsp_delete_acquire(struct ipsec_acquire *);

struct pool ipsec_policy_pool;
struct pool ipsec_acquire_pool;
int ipsec_policy_pool_initialized = 0;

/* Protected by the NET_LOCK(). */
int ipsec_acquire_pool_initialized = 0;
struct radix_node_head **spd_tables;
unsigned int spd_table_max;
TAILQ_HEAD(ipsec_acquire_head, ipsec_acquire) ipsec_acquire_head =
    TAILQ_HEAD_INITIALIZER(ipsec_acquire_head);

struct radix_node_head *
spd_table_get(unsigned int rtableid)
{
	unsigned int rdomain;

	NET_ASSERT_LOCKED();

	if (spd_tables == NULL)
		return (NULL);

	rdomain = rtable_l2(rtableid);
	if (rdomain > spd_table_max)
		return (NULL);

	return (spd_tables[rdomain]);
}

struct radix_node_head *
spd_table_add(unsigned int rtableid)
{
	struct radix_node_head *rnh = NULL;
	unsigned int rdomain;
	void *p;

	NET_ASSERT_LOCKED();

	rdomain = rtable_l2(rtableid);
	if (spd_tables == NULL || rdomain > spd_table_max) {
		if ((p = mallocarray(rdomain + 1, sizeof(*rnh),
		    M_RTABLE, M_NOWAIT|M_ZERO)) == NULL)
			return (NULL);

		if (spd_tables != NULL) {
			memcpy(p, spd_tables, sizeof(*rnh) * (spd_table_max+1));
			free(spd_tables, M_RTABLE, 0);
		}
		spd_tables = p;
		spd_table_max = rdomain;
	}

	if (spd_tables[rdomain] == NULL) {
		if (rn_inithead((void **)&rnh,
		    offsetof(struct sockaddr_encap, sen_type)) == 0)
			rnh = NULL;
		spd_tables[rdomain] = rnh;
	}

	return (spd_tables[rdomain]);
}

int
spd_table_walk(unsigned int rtableid,
    int (*func)(struct ipsec_policy *, void *, unsigned int), void *arg)
{
	struct radix_node_head *rnh;
	int (*walker)(struct radix_node *, void *, u_int) = (void *)func;
	int error;

	rnh = spd_table_get(rtableid);
	if (rnh == NULL)
		return (0);

	/* EGAIN means the tree changed. */
	while ((error = rn_walktree(rnh, walker, arg)) == EAGAIN)
		continue;

	return (error);
}

/*
 * Lookup at the SPD based on the headers contained on the mbuf. The second
 * argument indicates what protocol family the header at the beginning of
 * the mbuf is. hlen is the offset of the transport protocol header
 * in the mbuf.
 *
 * Return combinations (of return value and in *error):
 * - NULL/0 -> no IPsec required on packet
 * - NULL/-EINVAL -> silently drop the packet
 * - NULL/errno -> drop packet and return error
 * or a pointer to a TDB (and 0 in *error).
 *
 * In the case of incoming flows, only the first three combinations are
 * returned.
 */
struct tdb *
ipsp_spd_lookup(struct mbuf *m, int af, int hlen, int *error, int direction,
    struct tdb *tdbp, struct inpcb *inp, u_int32_t ipsecflowinfo)
{
	struct radix_node_head *rnh;
	struct radix_node *rn;
	union sockaddr_union sdst, ssrc;
	struct sockaddr_encap *ddst, dst;
	struct ipsec_policy *ipo;
	struct ipsec_ids *ids = NULL;
	int signore = 0, dignore = 0;
	u_int rdomain = rtable_l2(m->m_pkthdr.ph_rtableid);

	NET_ASSERT_LOCKED();

	/*
	 * If there are no flows in place, there's no point
	 * continuing with the SPD lookup.
	 */
	if (!ipsec_in_use && inp == NULL) {
		*error = 0;
		return NULL;
	}

	/*
	 * If an input packet is destined to a BYPASS socket, just accept it.
	 */
	if ((inp != NULL) && (direction == IPSP_DIRECTION_IN) &&
	    (inp->inp_seclevel[SL_ESP_TRANS] == IPSEC_LEVEL_BYPASS) &&
	    (inp->inp_seclevel[SL_ESP_NETWORK] == IPSEC_LEVEL_BYPASS) &&
	    (inp->inp_seclevel[SL_AUTH] == IPSEC_LEVEL_BYPASS)) {
		*error = 0;
		return NULL;
	}

	memset(&dst, 0, sizeof(dst));
	memset(&sdst, 0, sizeof(union sockaddr_union));
	memset(&ssrc, 0, sizeof(union sockaddr_union));
	ddst = (struct sockaddr_encap *)&dst;
	ddst->sen_family = PF_KEY;
	ddst->sen_len = SENT_LEN;

	switch (af) {
	case AF_INET:
		if (hlen < sizeof (struct ip) || m->m_pkthdr.len < hlen) {
			*error = EINVAL;
			return NULL;
		}
		ddst->sen_direction = direction;
		ddst->sen_type = SENT_IP4;

		m_copydata(m, offsetof(struct ip, ip_src),
		    sizeof(struct in_addr), (caddr_t) &(ddst->sen_ip_src));
		m_copydata(m, offsetof(struct ip, ip_dst),
		    sizeof(struct in_addr), (caddr_t) &(ddst->sen_ip_dst));
		m_copydata(m, offsetof(struct ip, ip_p), sizeof(u_int8_t),
		    (caddr_t) &(ddst->sen_proto));

		sdst.sin.sin_family = ssrc.sin.sin_family = AF_INET;
		sdst.sin.sin_len = ssrc.sin.sin_len =
		    sizeof(struct sockaddr_in);
		ssrc.sin.sin_addr = ddst->sen_ip_src;
		sdst.sin.sin_addr = ddst->sen_ip_dst;

		/*
		 * If TCP/UDP, extract the port numbers to use in the lookup.
		 */
		switch (ddst->sen_proto) {
		case IPPROTO_UDP:
		case IPPROTO_TCP:
			/* Make sure there's enough data in the packet. */
			if (m->m_pkthdr.len < hlen + 2 * sizeof(u_int16_t)) {
				*error = EINVAL;
				return NULL;
			}

			/*
			 * Luckily, the offset of the src/dst ports in
			 * both the UDP and TCP headers is the same (first
			 * two 16-bit values in the respective headers),
			 * so we can just copy them.
			 */
			m_copydata(m, hlen, sizeof(u_int16_t),
			    (caddr_t) &(ddst->sen_sport));
			m_copydata(m, hlen + sizeof(u_int16_t), sizeof(u_int16_t),
			    (caddr_t) &(ddst->sen_dport));
			break;

		default:
			ddst->sen_sport = 0;
			ddst->sen_dport = 0;
		}

		break;

#ifdef INET6
	case AF_INET6:
		if (hlen < sizeof (struct ip6_hdr) || m->m_pkthdr.len < hlen) {
			*error = EINVAL;
			return NULL;
		}
		ddst->sen_type = SENT_IP6;
		ddst->sen_ip6_direction = direction;

		m_copydata(m, offsetof(struct ip6_hdr, ip6_src),
		    sizeof(struct in6_addr),
		    (caddr_t) &(ddst->sen_ip6_src));
		m_copydata(m, offsetof(struct ip6_hdr, ip6_dst),
		    sizeof(struct in6_addr),
		    (caddr_t) &(ddst->sen_ip6_dst));
		m_copydata(m, offsetof(struct ip6_hdr, ip6_nxt),
		    sizeof(u_int8_t),
		    (caddr_t) &(ddst->sen_ip6_proto));

		sdst.sin6.sin6_family = ssrc.sin6.sin6_family = AF_INET6;
		sdst.sin6.sin6_len = ssrc.sin6.sin6_len =
		    sizeof(struct sockaddr_in6);
		in6_recoverscope(&ssrc.sin6, &ddst->sen_ip6_src);
		in6_recoverscope(&sdst.sin6, &ddst->sen_ip6_dst);

		/*
		 * If TCP/UDP, extract the port numbers to use in the lookup.
		 */
		switch (ddst->sen_ip6_proto) {
		case IPPROTO_UDP:
		case IPPROTO_TCP:
			/* Make sure there's enough data in the packet. */
			if (m->m_pkthdr.len < hlen + 2 * sizeof(u_int16_t)) {
				*error = EINVAL;
				return NULL;
			}

			/*
			 * Luckily, the offset of the src/dst ports in
			 * both the UDP and TCP headers is the same
			 * (first two 16-bit values in the respective
			 * headers), so we can just copy them.
			 */
			m_copydata(m, hlen, sizeof(u_int16_t),
			    (caddr_t) &(ddst->sen_ip6_sport));
			m_copydata(m, hlen + sizeof(u_int16_t), sizeof(u_int16_t),
			    (caddr_t) &(ddst->sen_ip6_dport));
			break;

		default:
			ddst->sen_ip6_sport = 0;
			ddst->sen_ip6_dport = 0;
		}

		break;
#endif /* INET6 */

	default:
		*error = EAFNOSUPPORT;
		return NULL;
	}

	/* Actual SPD lookup. */
	if ((rnh = spd_table_get(rdomain)) == NULL ||
	    (rn = rn_match((caddr_t)&dst, rnh)) == NULL) {
		/*
		 * Return whatever the socket requirements are, there are no
		 * system-wide policies.
		 */
		*error = 0;
		return ipsp_spd_inp(m, af, hlen, error, direction,
		    tdbp, inp, NULL);
	}
	ipo = (struct ipsec_policy *)rn;

	switch (ipo->ipo_type) {
	case IPSP_PERMIT:
		*error = 0;
		return ipsp_spd_inp(m, af, hlen, error, direction, tdbp,
		    inp, ipo);

	case IPSP_DENY:
		*error = EHOSTUNREACH;
		return NULL;

	case IPSP_IPSEC_USE:
	case IPSP_IPSEC_ACQUIRE:
	case IPSP_IPSEC_REQUIRE:
	case IPSP_IPSEC_DONTACQ:
		/* Nothing more needed here. */
		break;

	default:
		*error = EINVAL;
		return NULL;
	}

	/* Check for non-specific destination in the policy. */
	switch (ipo->ipo_dst.sa.sa_family) {
	case AF_INET:
		if ((ipo->ipo_dst.sin.sin_addr.s_addr == INADDR_ANY) ||
		    (ipo->ipo_dst.sin.sin_addr.s_addr == INADDR_BROADCAST))
			dignore = 1;
		break;

#ifdef INET6
	case AF_INET6:
		if ((IN6_IS_ADDR_UNSPECIFIED(&ipo->ipo_dst.sin6.sin6_addr)) ||
		    (memcmp(&ipo->ipo_dst.sin6.sin6_addr, &in6mask128,
		    sizeof(in6mask128)) == 0))
			dignore = 1;
		break;
#endif /* INET6 */
	}

	/* Likewise for source. */
	switch (ipo->ipo_src.sa.sa_family) {
	case AF_INET:
		if (ipo->ipo_src.sin.sin_addr.s_addr == INADDR_ANY)
			signore = 1;
		break;

#ifdef INET6
	case AF_INET6:
		if (IN6_IS_ADDR_UNSPECIFIED(&ipo->ipo_src.sin6.sin6_addr))
			signore = 1;
		break;
#endif /* INET6 */
	}

	/* Do we have a cached entry ? If so, check if it's still valid. */
	if ((ipo->ipo_tdb) && (ipo->ipo_tdb->tdb_flags & TDBF_INVALID)) {
		TAILQ_REMOVE(&ipo->ipo_tdb->tdb_policy_head, ipo,
		    ipo_tdb_next);
		ipo->ipo_tdb = NULL;
	}

	/* Outgoing packet policy check. */
	if (direction == IPSP_DIRECTION_OUT) {
		/*
		 * If the packet is destined for the policy-specified
		 * gateway/endhost, and the socket has the BYPASS
		 * option set, skip IPsec processing.
		 */
		if ((inp != NULL) &&
		    (inp->inp_seclevel[SL_ESP_TRANS] == IPSEC_LEVEL_BYPASS) &&
		    (inp->inp_seclevel[SL_ESP_NETWORK] ==
			IPSEC_LEVEL_BYPASS) &&
		    (inp->inp_seclevel[SL_AUTH] == IPSEC_LEVEL_BYPASS)) {
			/* Direct match. */
			if (dignore ||
			    !memcmp(&sdst, &ipo->ipo_dst, sdst.sa.sa_len)) {
				*error = 0;
				return NULL;
			}
		}

		if (ipsecflowinfo)
			ids = ipsp_ids_lookup(ipsecflowinfo);

		/* Check that the cached TDB (if present), is appropriate. */
		if (ipo->ipo_tdb) {
			if ((ipo->ipo_last_searched <= ipsec_last_added) ||
			    (ipo->ipo_sproto != ipo->ipo_tdb->tdb_sproto) ||
			    memcmp(dignore ? &sdst : &ipo->ipo_dst,
			    &ipo->ipo_tdb->tdb_dst,
			    ipo->ipo_tdb->tdb_dst.sa.sa_len))
				goto nomatchout;

			if (!ipsp_aux_match(ipo->ipo_tdb,
			    ids ? ids : ipo->ipo_ids,
			    &ipo->ipo_addr, &ipo->ipo_mask))
				goto nomatchout;

			/* Cached entry is good. */
			*error = 0;
			return ipsp_spd_inp(m, af, hlen, error, direction,
			    tdbp, inp, ipo);

  nomatchout:
			/* Cached TDB was not good. */
			TAILQ_REMOVE(&ipo->ipo_tdb->tdb_policy_head, ipo,
			    ipo_tdb_next);
			ipo->ipo_tdb = NULL;
			ipo->ipo_last_searched = 0;
		}

		/*
		 * If no SA has been added since the last time we did a
		 * lookup, there's no point searching for one. However, if the
		 * destination gateway is left unspecified (or is all-1's),
		 * always lookup since this is a generic-match rule
		 * (otherwise, we can have situations where SAs to some
		 * destinations exist but are not used, possibly leading to an
		 * explosion in the number of acquired SAs).
		 */
		if (ipo->ipo_last_searched <= ipsec_last_added)	{
			/* "Touch" the entry. */
			if (dignore == 0)
				ipo->ipo_last_searched = time_uptime;

			/* Find an appropriate SA from the existing ones. */
			ipo->ipo_tdb =
			    gettdbbydst(rdomain,
				dignore ? &sdst : &ipo->ipo_dst,
				ipo->ipo_sproto,
				ids ? ids: ipo->ipo_ids,
				&ipo->ipo_addr, &ipo->ipo_mask);
			if (ipo->ipo_tdb) {
				TAILQ_INSERT_TAIL(&ipo->ipo_tdb->tdb_policy_head,
				    ipo, ipo_tdb_next);
				*error = 0;
				return ipsp_spd_inp(m, af, hlen, error,
				    direction, tdbp, inp, ipo);
			}
		}

		/* So, we don't have an SA -- just a policy. */
		switch (ipo->ipo_type) {
		case IPSP_IPSEC_REQUIRE:
			/* Acquire SA through key management. */
			if (ipsp_acquire_sa(ipo,
			    dignore ? &sdst : &ipo->ipo_dst,
			    signore ? NULL : &ipo->ipo_src, ddst, m) != 0) {
				*error = EACCES;
				return NULL;
			}

			/* FALLTHROUGH */
		case IPSP_IPSEC_DONTACQ:
			*error = -EINVAL; /* Silently drop packet. */
			return NULL;

		case IPSP_IPSEC_ACQUIRE:
			/* Acquire SA through key management. */
			ipsp_acquire_sa(ipo, dignore ? &sdst : &ipo->ipo_dst,
			    signore ? NULL : &ipo->ipo_src, ddst, NULL);

			/* FALLTHROUGH */
		case IPSP_IPSEC_USE:
			*error = 0;
			return ipsp_spd_inp(m, af, hlen, error, direction,
			    tdbp, inp, ipo);
		}
	} else { /* IPSP_DIRECTION_IN */
		if (tdbp != NULL) {
			/* Direct match in the cache. */
			if (ipo->ipo_tdb == tdbp) {
				*error = 0;
				return ipsp_spd_inp(m, af, hlen, error,
				    direction, tdbp, inp, ipo);
			}

			if (memcmp(dignore ? &ssrc : &ipo->ipo_dst,
			    &tdbp->tdb_src, tdbp->tdb_src.sa.sa_len) ||
			    (ipo->ipo_sproto != tdbp->tdb_sproto))
				goto nomatchin;

			/* Match source/dest IDs. */
			if (ipo->ipo_ids)
				if (tdbp->tdb_ids == NULL ||
				    !ipsp_ids_match(ipo->ipo_ids, tdbp->tdb_ids))
					goto nomatchin;

			/* Add it to the cache. */
			if (ipo->ipo_tdb)
				TAILQ_REMOVE(&ipo->ipo_tdb->tdb_policy_head,
				    ipo, ipo_tdb_next);
			ipo->ipo_tdb = tdbp;
			TAILQ_INSERT_TAIL(&tdbp->tdb_policy_head, ipo,
			    ipo_tdb_next);
			*error = 0;
			return ipsp_spd_inp(m, af, hlen, error, direction,
			    tdbp, inp, ipo);

  nomatchin: /* Nothing needed here, falling through */
	;
		}

		/* Check whether cached entry applies. */
		if (ipo->ipo_tdb) {
			/*
			 * We only need to check that the correct
			 * security protocol and security gateway are
			 * set; IDs will be the same since the cached
			 * entry is linked on this policy.
			 */
			if (ipo->ipo_sproto == ipo->ipo_tdb->tdb_sproto &&
			    !memcmp(&ipo->ipo_tdb->tdb_src,
			    dignore ? &ssrc : &ipo->ipo_dst,
			    ipo->ipo_tdb->tdb_src.sa.sa_len))
				goto skipinputsearch;

			/* Not applicable, unlink. */
			TAILQ_REMOVE(&ipo->ipo_tdb->tdb_policy_head, ipo,
			    ipo_tdb_next);
			ipo->ipo_last_searched = 0;
			ipo->ipo_tdb = NULL;
		}

		/* Find whether there exists an appropriate SA. */
		if (ipo->ipo_last_searched <= ipsec_last_added)	{
			if (dignore == 0)
				ipo->ipo_last_searched = time_uptime;

			ipo->ipo_tdb =
			    gettdbbysrc(rdomain,
				dignore ? &ssrc : &ipo->ipo_dst,
				ipo->ipo_sproto, ipo->ipo_ids,
				&ipo->ipo_addr, &ipo->ipo_mask);
			if (ipo->ipo_tdb)
				TAILQ_INSERT_TAIL(&ipo->ipo_tdb->tdb_policy_head,
				    ipo, ipo_tdb_next);
		}
  skipinputsearch:

		switch (ipo->ipo_type) {
		case IPSP_IPSEC_REQUIRE:
			/* If appropriate SA exists, don't acquire another. */
			if (ipo->ipo_tdb) {
				*error = -EINVAL;
				return NULL;
			}

			/* Acquire SA through key management. */
			if ((*error = ipsp_acquire_sa(ipo,
			    dignore ? &ssrc : &ipo->ipo_dst,
			    signore ? NULL : &ipo->ipo_src, ddst, m)) != 0)
				return NULL;

			/* FALLTHROUGH */
		case IPSP_IPSEC_DONTACQ:
			/* Drop packet. */
			*error = -EINVAL;
			return NULL;

		case IPSP_IPSEC_ACQUIRE:
			/* If appropriate SA exists, don't acquire another. */
			if (ipo->ipo_tdb) {
				*error = 0;
				return ipsp_spd_inp(m, af, hlen, error,
				    direction, tdbp, inp, ipo);
			}

			/* Acquire SA through key management. */
			ipsp_acquire_sa(ipo, dignore ? &ssrc : &ipo->ipo_dst,
			    signore ? NULL : &ipo->ipo_src, ddst, NULL);

			/* FALLTHROUGH */
		case IPSP_IPSEC_USE:
			*error = 0;
			return ipsp_spd_inp(m, af, hlen, error, direction,
			    tdbp, inp, ipo);
		}
	}

	/* Shouldn't ever get this far. */
	*error = EINVAL;
	return NULL;
}

/*
 * Delete a policy from the SPD.
 */
int
ipsec_delete_policy(struct ipsec_policy *ipo)
{
	struct ipsec_acquire *ipa;
	struct radix_node_head *rnh;
	struct radix_node *rn = (struct radix_node *)ipo;
	int err = 0;

	NET_ASSERT_LOCKED();

	if (--ipo->ipo_ref_count > 0)
		return 0;

	/* Delete from SPD. */
	if ((rnh = spd_table_get(ipo->ipo_rdomain)) == NULL ||
	    rn_delete(&ipo->ipo_addr, &ipo->ipo_mask, rnh, rn) == NULL)
		return (ESRCH);

	if (ipo->ipo_tdb != NULL)
		TAILQ_REMOVE(&ipo->ipo_tdb->tdb_policy_head, ipo,
		    ipo_tdb_next);

	while ((ipa = TAILQ_FIRST(&ipo->ipo_acquires)) != NULL)
		ipsp_delete_acquire(ipa);

	TAILQ_REMOVE(&ipsec_policy_head, ipo, ipo_list);

	if (ipo->ipo_ids)
		ipsp_ids_free(ipo->ipo_ids);

	ipsec_in_use--;

	pool_put(&ipsec_policy_pool, ipo);

	return err;
}

void
ipsp_delete_acquire_timo(void *v)
{
	struct ipsec_acquire *ipa = v;

	NET_LOCK();
	ipsp_delete_acquire(ipa);
	NET_UNLOCK();
}

/*
 * Delete a pending IPsec acquire record.
 */
void
ipsp_delete_acquire(struct ipsec_acquire *ipa)
{
	NET_ASSERT_LOCKED();

	timeout_del(&ipa->ipa_timeout);
	TAILQ_REMOVE(&ipsec_acquire_head, ipa, ipa_next);
	if (ipa->ipa_policy != NULL)
		TAILQ_REMOVE(&ipa->ipa_policy->ipo_acquires, ipa,
		    ipa_ipo_next);
	pool_put(&ipsec_acquire_pool, ipa);
}

/*
 * Find out if there's an ACQUIRE pending.
 * XXX Need a better structure.
 */
struct ipsec_acquire *
ipsp_pending_acquire(struct ipsec_policy *ipo, union sockaddr_union *gw)
{
	struct ipsec_acquire *ipa;

	NET_ASSERT_LOCKED();

	TAILQ_FOREACH (ipa, &ipo->ipo_acquires, ipa_ipo_next) {
		if (!memcmp(gw, &ipa->ipa_addr, gw->sa.sa_len))
			return ipa;
	}

	return NULL;
}

/*
 * Signal key management that we need an SA.
 * XXX For outgoing policies, we could try to hold on to the mbuf.
 */
int
ipsp_acquire_sa(struct ipsec_policy *ipo, union sockaddr_union *gw,
    union sockaddr_union *laddr, struct sockaddr_encap *ddst, struct mbuf *m)
{
	struct ipsec_acquire *ipa;

	NET_ASSERT_LOCKED();

	/* Check whether request has been made already. */
	if ((ipa = ipsp_pending_acquire(ipo, gw)) != NULL)
		return 0;

	/* Add request in cache and proceed. */
	if (ipsec_acquire_pool_initialized == 0) {
		ipsec_acquire_pool_initialized = 1;
		pool_init(&ipsec_acquire_pool, sizeof(struct ipsec_acquire),
		    0, IPL_SOFTNET, 0, "ipsec acquire", NULL);
	}

	ipa = pool_get(&ipsec_acquire_pool, PR_NOWAIT|PR_ZERO);
	if (ipa == NULL)
		return ENOMEM;

	ipa->ipa_addr = *gw;

	timeout_set_proc(&ipa->ipa_timeout, ipsp_delete_acquire_timo, ipa);

	ipa->ipa_info.sen_len = ipa->ipa_mask.sen_len = SENT_LEN;
	ipa->ipa_info.sen_family = ipa->ipa_mask.sen_family = PF_KEY;

	/* Just copy the right information. */
	switch (ipo->ipo_addr.sen_type) {
	case SENT_IP4:
		ipa->ipa_info.sen_type = ipa->ipa_mask.sen_type = SENT_IP4;
		ipa->ipa_info.sen_direction = ipo->ipo_addr.sen_direction;
		ipa->ipa_mask.sen_direction = ipo->ipo_mask.sen_direction;

		if (ipsp_is_unspecified(ipo->ipo_dst)) {
			ipa->ipa_info.sen_ip_src = ddst->sen_ip_src;
			ipa->ipa_mask.sen_ip_src.s_addr = INADDR_BROADCAST;

			ipa->ipa_info.sen_ip_dst = ddst->sen_ip_dst;
			ipa->ipa_mask.sen_ip_dst.s_addr = INADDR_BROADCAST;
		} else {
			ipa->ipa_info.sen_ip_src = ipo->ipo_addr.sen_ip_src;
			ipa->ipa_mask.sen_ip_src = ipo->ipo_mask.sen_ip_src;

			ipa->ipa_info.sen_ip_dst = ipo->ipo_addr.sen_ip_dst;
			ipa->ipa_mask.sen_ip_dst = ipo->ipo_mask.sen_ip_dst;
		}

		ipa->ipa_info.sen_proto = ipo->ipo_addr.sen_proto;
		ipa->ipa_mask.sen_proto = ipo->ipo_mask.sen_proto;

		if (ipo->ipo_addr.sen_proto) {
			ipa->ipa_info.sen_sport = ipo->ipo_addr.sen_sport;
			ipa->ipa_mask.sen_sport = ipo->ipo_mask.sen_sport;

			ipa->ipa_info.sen_dport = ipo->ipo_addr.sen_dport;
			ipa->ipa_mask.sen_dport = ipo->ipo_mask.sen_dport;
		}
		break;

#ifdef INET6
	case SENT_IP6:
		ipa->ipa_info.sen_type = ipa->ipa_mask.sen_type = SENT_IP6;
		ipa->ipa_info.sen_ip6_direction =
		    ipo->ipo_addr.sen_ip6_direction;
		ipa->ipa_mask.sen_ip6_direction =
		    ipo->ipo_mask.sen_ip6_direction;

		if (ipsp_is_unspecified(ipo->ipo_dst)) {
			ipa->ipa_info.sen_ip6_src = ddst->sen_ip6_src;
			ipa->ipa_mask.sen_ip6_src = in6mask128;

			ipa->ipa_info.sen_ip6_dst = ddst->sen_ip6_dst;
			ipa->ipa_mask.sen_ip6_dst = in6mask128;
		} else {
			ipa->ipa_info.sen_ip6_src = ipo->ipo_addr.sen_ip6_src;
			ipa->ipa_mask.sen_ip6_src = ipo->ipo_mask.sen_ip6_src;

			ipa->ipa_info.sen_ip6_dst = ipo->ipo_addr.sen_ip6_dst;
			ipa->ipa_mask.sen_ip6_dst = ipo->ipo_mask.sen_ip6_dst;
		}

		ipa->ipa_info.sen_ip6_proto = ipo->ipo_addr.sen_ip6_proto;
		ipa->ipa_mask.sen_ip6_proto = ipo->ipo_mask.sen_ip6_proto;

		if (ipo->ipo_mask.sen_ip6_proto) {
			ipa->ipa_info.sen_ip6_sport =
			    ipo->ipo_addr.sen_ip6_sport;
			ipa->ipa_mask.sen_ip6_sport =
			    ipo->ipo_mask.sen_ip6_sport;
			ipa->ipa_info.sen_ip6_dport =
			    ipo->ipo_addr.sen_ip6_dport;
			ipa->ipa_mask.sen_ip6_dport =
			    ipo->ipo_mask.sen_ip6_dport;
		}
		break;
#endif /* INET6 */

	default:
		pool_put(&ipsec_acquire_pool, ipa);
		return 0;
	}

#ifdef IPSEC
	timeout_add_sec(&ipa->ipa_timeout, ipsec_expire_acquire);
#endif

	TAILQ_INSERT_TAIL(&ipsec_acquire_head, ipa, ipa_next);
	TAILQ_INSERT_TAIL(&ipo->ipo_acquires, ipa, ipa_ipo_next);
	ipa->ipa_policy = ipo;

	/* PF_KEYv2 notification message. */
	return pfkeyv2_acquire(ipo, gw, laddr, &ipa->ipa_seq, ddst);
}

/*
 * Deal with PCB security requirements.
 */
struct tdb *
ipsp_spd_inp(struct mbuf *m, int af, int hlen, int *error, int direction,
    struct tdb *tdbp, struct inpcb *inp, struct ipsec_policy *ipo)
{
	/* Sanity check. */
	if (inp == NULL)
		goto justreturn;

	/* We only support IPSEC_LEVEL_BYPASS or IPSEC_LEVEL_AVAIL */

	if (inp->inp_seclevel[SL_ESP_TRANS] == IPSEC_LEVEL_BYPASS &&
	    inp->inp_seclevel[SL_ESP_NETWORK] == IPSEC_LEVEL_BYPASS &&
	    inp->inp_seclevel[SL_AUTH] == IPSEC_LEVEL_BYPASS)
		goto justreturn;

	if (inp->inp_seclevel[SL_ESP_TRANS] == IPSEC_LEVEL_AVAIL &&
	    inp->inp_seclevel[SL_ESP_NETWORK] == IPSEC_LEVEL_AVAIL &&
	    inp->inp_seclevel[SL_AUTH] == IPSEC_LEVEL_AVAIL)
		goto justreturn;

	*error = -EINVAL;
	return NULL;

 justreturn:
	if (ipo != NULL)
		return ipo->ipo_tdb;
	else
		return NULL;
}

/*
 * Find a pending ACQUIRE record based on its sequence number.
 * XXX Need to use a better data structure.
 */
struct ipsec_acquire *
ipsec_get_acquire(u_int32_t seq)
{
	struct ipsec_acquire *ipa;

	NET_ASSERT_LOCKED();

	TAILQ_FOREACH (ipa, &ipsec_acquire_head, ipa_next)
		if (ipa->ipa_seq == seq)
			return ipa;

	return NULL;
}
