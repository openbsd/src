/* $OpenBSD: ip_spd.c,v 1.48 2004/04/14 20:10:04 markus Exp $ */
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
#include <sys/protosw.h>

#include <net/if.h>
#include <net/route.h>
#include <net/netisr.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/in_var.h>
#endif /* INET */

#ifdef INET6
#ifndef INET
#include <netinet/in.h>
#endif
#include <netinet6/in6_var.h>
#endif /* INET6 */

#include <netinet/ip_ipsp.h>
#include <net/pfkeyv2.h>

#ifdef ENCDEBUG
#define	DPRINTF(x)	if (encdebug) printf x
#else
#define	DPRINTF(x)
#endif

struct pool ipsec_policy_pool;
struct pool ipsec_acquire_pool;
int ipsec_policy_pool_initialized = 0;
int ipsec_acquire_pool_initialized = 0;

/*
 * Lookup at the SPD based on the headers contained on the mbuf. The second
 * argument indicates what protocol family the header at the beginning of
 * the mbuf is. hlen is the the offset of the transport protocol header
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
    struct tdb *tdbp, struct inpcb *inp)
{
	struct route_enc re0, *re = &re0;
	union sockaddr_union sdst, ssrc;
	struct sockaddr_encap *ddst;
	struct ipsec_policy *ipo;
	int signore = 0, dignore = 0;

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

	bzero((caddr_t) re, sizeof(struct route_enc));
	bzero((caddr_t) &sdst, sizeof(union sockaddr_union));
	bzero((caddr_t) &ssrc, sizeof(union sockaddr_union));
	ddst = (struct sockaddr_encap *) &re->re_dst;
	ddst->sen_family = PF_KEY;
	ddst->sen_len = SENT_LEN;

	switch (af) {
#ifdef INET
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
#endif /* INET */

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
		sdst.sin6.sin6_len = ssrc.sin6.sin6_family =
		    sizeof(struct sockaddr_in6);
		ssrc.sin6.sin6_addr = ddst->sen_ip6_src;
		sdst.sin6.sin6_addr = ddst->sen_ip6_dst;

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
	rtalloc((struct route *) re);
	if (re->re_rt == NULL) {
		/*
		 * Return whatever the socket requirements are, there are no
		 * system-wide policies.
		 */
		*error = 0;
		return ipsp_spd_inp(m, af, hlen, error, direction,
		    tdbp, inp, NULL);
	}

	/* Sanity check. */
	if ((re->re_rt->rt_gateway == NULL) ||
	    (((struct sockaddr_encap *) re->re_rt->rt_gateway)->sen_type !=
		SENT_IPSP)) {
		RTFREE(re->re_rt);
		*error = EHOSTUNREACH;
		DPRINTF(("ip_spd_lookup: no gateway in SPD entry!"));
		return NULL;
	}

	ipo = ((struct sockaddr_encap *) (re->re_rt->rt_gateway))->sen_ipsp;
	RTFREE(re->re_rt);
	if (ipo == NULL) {
		*error = EHOSTUNREACH;
		DPRINTF(("ip_spd_lookup: no policy attached to SPD entry!"));
		return NULL;
	}

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
#ifdef INET
	case AF_INET:
		if ((ipo->ipo_dst.sin.sin_addr.s_addr == INADDR_ANY) ||
		    (ipo->ipo_dst.sin.sin_addr.s_addr == INADDR_BROADCAST))
			dignore = 1;
		break;
#endif /* INET */

#ifdef INET6
	case AF_INET6:
		if ((IN6_IS_ADDR_UNSPECIFIED(&ipo->ipo_dst.sin6.sin6_addr)) ||
		    (bcmp(&ipo->ipo_dst.sin6.sin6_addr, &in6mask128,
			sizeof(in6mask128)) == 0))
			dignore = 1;
		break;
#endif /* INET6 */
	}

	/* Likewise for source. */
	switch (ipo->ipo_src.sa.sa_family) {
#ifdef INET
	case AF_INET:
		if (ipo->ipo_src.sin.sin_addr.s_addr == INADDR_ANY)
			signore = 1;
		break;
#endif /* INET */

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
			    !bcmp(&sdst, &ipo->ipo_dst, sdst.sa.sa_len)) {
				*error = 0;
				return NULL;
			}
		}

		/* Check that the cached TDB (if present), is appropriate. */
		if (ipo->ipo_tdb) {
			if ((ipo->ipo_last_searched <= ipsec_last_added) ||
			    (ipo->ipo_sproto != ipo->ipo_tdb->tdb_sproto) ||
			    bcmp(dignore ? &sdst : &ipo->ipo_dst,
				&ipo->ipo_tdb->tdb_dst,
				ipo->ipo_tdb->tdb_dst.sa.sa_len))
				goto nomatchout;

			if (!ipsp_aux_match(ipo->ipo_tdb,
			    ipo->ipo_srcid, ipo->ipo_dstid,
			    ipo->ipo_local_cred, NULL,
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
				ipo->ipo_last_searched = time.tv_sec;

			/* Find an appropriate SA from the existing ones. */
			ipo->ipo_tdb =
			    gettdbbyaddr(dignore ? &sdst : &ipo->ipo_dst,
				ipo->ipo_sproto, ipo->ipo_srcid,
				ipo->ipo_dstid, ipo->ipo_local_cred, m, af,
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

			/* Fall through */
		case IPSP_IPSEC_DONTACQ:
			*error = -EINVAL; /* Silently drop packet. */
			return NULL;

		case IPSP_IPSEC_ACQUIRE:
			/* Acquire SA through key management. */
			ipsp_acquire_sa(ipo, dignore ? &sdst : &ipo->ipo_dst,
			    signore ? NULL : &ipo->ipo_src, ddst, NULL);

			/* Fall through */
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

			if (bcmp(dignore ? &ssrc : &ipo->ipo_dst,
			    &tdbp->tdb_src, tdbp->tdb_src.sa.sa_len) ||
			    (ipo->ipo_sproto != tdbp->tdb_sproto))
				goto nomatchin;

			/* Match source ID. */
			if (ipo->ipo_srcid) {
				if (tdbp->tdb_dstid == NULL ||
				    !ipsp_ref_match(ipo->ipo_srcid,
					tdbp->tdb_dstid))
					goto nomatchin;
			}

			/* Match destination ID. */
			if (ipo->ipo_dstid) {
				if (tdbp->tdb_srcid == NULL ||
				    !ipsp_ref_match(ipo->ipo_dstid,
					tdbp->tdb_srcid))
					goto nomatchin;
			}

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
			 * set; credentials/IDs will be the same,
			 * since the cached entry is linked on this
			 * policy.
			 */
			if (ipo->ipo_sproto == ipo->ipo_tdb->tdb_sproto &&
			    !bcmp(&ipo->ipo_tdb->tdb_src,
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
				ipo->ipo_last_searched = time.tv_sec;

			ipo->ipo_tdb =
			    gettdbbysrc(dignore ? &ssrc : &ipo->ipo_dst,
				ipo->ipo_sproto, ipo->ipo_srcid,
				ipo->ipo_dstid, m, af, &ipo->ipo_addr,
				&ipo->ipo_mask);
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

			/* Fall through */
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

			/* Fall through */
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
	int err = 0;

	if (--ipo->ipo_ref_count > 0)
		return 0;

	/* Delete from SPD. */
	if (!(ipo->ipo_flags & IPSP_POLICY_SOCKET))
		err = rtrequest(RTM_DELETE, (struct sockaddr *) &ipo->ipo_addr,
		    (struct sockaddr *) 0,
		    (struct sockaddr *) &ipo->ipo_mask,
		    0, (struct rtentry **) 0);

	if (ipo->ipo_tdb != NULL)
		TAILQ_REMOVE(&ipo->ipo_tdb->tdb_policy_head, ipo,
		    ipo_tdb_next);

	while ((ipa = TAILQ_FIRST(&ipo->ipo_acquires)) != NULL)
		ipsp_delete_acquire(ipa);

	TAILQ_REMOVE(&ipsec_policy_head, ipo, ipo_list);

	if (ipo->ipo_srcid)
		ipsp_reffree(ipo->ipo_srcid);
	if (ipo->ipo_dstid)
		ipsp_reffree(ipo->ipo_dstid);
	if (ipo->ipo_local_cred)
		ipsp_reffree(ipo->ipo_local_cred);
	if (ipo->ipo_local_auth)
		ipsp_reffree(ipo->ipo_local_auth);

	pool_put(&ipsec_policy_pool, ipo);

	if (!(ipo->ipo_flags & IPSP_POLICY_SOCKET))
		ipsec_in_use--;

	return err;
}

/*
 * Add a policy to the SPD.
 */
struct ipsec_policy *
ipsec_add_policy(struct inpcb *inp, int af, int direction)
{
	struct ipsec_policy *ipon;

	if (ipsec_policy_pool_initialized == 0) {
		ipsec_policy_pool_initialized = 1;
		pool_init(&ipsec_policy_pool, sizeof(struct ipsec_policy),
		    0, 0, 0, "ipsec policy", NULL);
	}

	ipon = pool_get(&ipsec_policy_pool, 0);
	if (ipon == NULL)
		return NULL;

	bzero(ipon, sizeof(struct ipsec_policy));

	ipon->ipo_ref_count = 1;
	ipon->ipo_flags |= IPSP_POLICY_SOCKET;

	ipon->ipo_type = IPSP_IPSEC_REQUIRE; /* XXX */

	/* XXX
	 * We should actually be creating a linked list of
	 * policies (for tunnel/transport and ESP/AH), as needed.
	 */
	ipon->ipo_sproto = IPPROTO_ESP;

	TAILQ_INIT(&ipon->ipo_acquires);
	TAILQ_INSERT_HEAD(&ipsec_policy_head, ipon, ipo_list);

	ipsec_update_policy(inp, ipon, af, direction);

	return ipon;
}

/*
 * Update a PCB-attached policy.
 */
void
ipsec_update_policy(struct inpcb *inp, struct ipsec_policy *ipon, int af,
    int direction)
{
	ipon->ipo_addr.sen_len = ipon->ipo_mask.sen_len = SENT_LEN;
	ipon->ipo_addr.sen_family = ipon->ipo_mask.sen_family = PF_KEY;
	ipon->ipo_src.sa.sa_family = ipon->ipo_dst.sa.sa_family = af;

	switch (af) {
	case AF_INET:
#ifdef INET
		ipon->ipo_addr.sen_type = ipon->ipo_mask.sen_type = SENT_IP4;
		ipon->ipo_addr.sen_ip_src = inp->inp_laddr;
		ipon->ipo_addr.sen_ip_dst = inp->inp_faddr;
		ipon->ipo_addr.sen_sport = inp->inp_lport;
		ipon->ipo_addr.sen_dport = inp->inp_fport;
		ipon->ipo_addr.sen_proto =
		    inp->inp_socket->so_proto->pr_protocol;
		ipon->ipo_addr.sen_direction = direction;

		ipon->ipo_mask.sen_ip_src.s_addr = 0xffffffff;
		ipon->ipo_mask.sen_ip_dst.s_addr = 0xffffffff;
		ipon->ipo_mask.sen_sport = ipon->ipo_mask.sen_dport = 0xffff;
		ipon->ipo_mask.sen_proto = 0xff;
		ipon->ipo_mask.sen_direction = direction;

		ipon->ipo_src.sa.sa_len = sizeof(struct sockaddr_in);
		ipon->ipo_dst.sa.sa_len = sizeof(struct sockaddr_in);
		ipon->ipo_src.sin.sin_addr = inp->inp_laddr;
		ipon->ipo_dst.sin.sin_addr = inp->inp_faddr;
#endif /* INET */
		break;

	case AF_INET6:
#ifdef INET6
		ipon->ipo_addr.sen_type = ipon->ipo_mask.sen_type = SENT_IP6;
		ipon->ipo_addr.sen_ip6_src = inp->inp_laddr6;
		ipon->ipo_addr.sen_ip6_dst = inp->inp_faddr6;
		ipon->ipo_addr.sen_ip6_sport = inp->inp_lport;
		ipon->ipo_addr.sen_ip6_dport = inp->inp_fport;
		ipon->ipo_addr.sen_ip6_proto =
		    inp->inp_socket->so_proto->pr_protocol;
		ipon->ipo_addr.sen_ip6_direction = direction;

		ipon->ipo_mask.sen_ip6_src = in6mask128;
		ipon->ipo_mask.sen_ip6_dst = in6mask128;
		ipon->ipo_mask.sen_ip6_sport = 0xffff;
		ipon->ipo_mask.sen_ip6_dport = 0xffff;
		ipon->ipo_mask.sen_ip6_proto = 0xff;
		ipon->ipo_mask.sen_ip6_direction = direction;

		ipon->ipo_src.sa.sa_len = sizeof(struct sockaddr_in6);
		ipon->ipo_dst.sa.sa_len = sizeof(struct sockaddr_in6);
		ipon->ipo_src.sin6.sin6_addr = inp->inp_laddr6;
		ipon->ipo_dst.sin6.sin6_addr = inp->inp_faddr6;
#endif /* INET6 */
		break;
	}
}

/*
 * Delete a pending IPsec acquire record.
 */
void
ipsp_delete_acquire(void *v)
{
	struct ipsec_acquire *ipa = v;

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

	TAILQ_FOREACH (ipa, &ipo->ipo_acquires, ipa_ipo_next) {
		if (!bcmp(gw, &ipa->ipa_addr, gw->sa.sa_len))
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

	/*
	 * If this is a socket policy, it has to have authentication
	 * information accompanying it --- can't tell key mgmt. to
	 * "find" it for us. This avoids abusing key mgmt. to authenticate
	 * on an application's behalf, even if the application doesn't
	 * have/know (and shouldn't) the appropriate authentication
	 * material (passphrase, private key, etc.)
	 */
	if (ipo->ipo_flags & IPSP_POLICY_SOCKET &&
	    ipo->ipo_local_auth == NULL)
		return EINVAL;

	/* Check whether request has been made already. */
	if ((ipa = ipsp_pending_acquire(ipo, gw)) != NULL)
		return 0;

	/* Add request in cache and proceed. */
	if (ipsec_acquire_pool_initialized == 0) {
		ipsec_acquire_pool_initialized = 1;
		pool_init(&ipsec_acquire_pool, sizeof(struct ipsec_acquire),
		    0, 0, 0, "ipsec acquire", NULL);
	}

	ipa = pool_get(&ipsec_acquire_pool, 0);
	if (ipa == NULL)
		return ENOMEM;

	bzero(ipa, sizeof(struct ipsec_acquire));
	bcopy(gw, &ipa->ipa_addr, sizeof(union sockaddr_union));

	timeout_set(&ipa->ipa_timeout, ipsp_delete_acquire, ipa);

	ipa->ipa_info.sen_len = ipa->ipa_mask.sen_len = SENT_LEN;
	ipa->ipa_info.sen_family = ipa->ipa_mask.sen_family = PF_KEY;

	/* Just copy the right information. */
	switch (ipo->ipo_addr.sen_type) {
#ifdef INET
	case SENT_IP4:
		ipa->ipa_info.sen_type = ipa->ipa_mask.sen_type = SENT_IP4;
		ipa->ipa_info.sen_direction = ipo->ipo_addr.sen_direction;
		ipa->ipa_mask.sen_direction = ipo->ipo_mask.sen_direction;

		if (ipo->ipo_mask.sen_ip_src.s_addr == INADDR_ANY ||
		    ipo->ipo_addr.sen_ip_src.s_addr == INADDR_ANY ||
		    ipsp_is_unspecified(ipo->ipo_dst)) {
			ipa->ipa_info.sen_ip_src = ddst->sen_ip_src;
			ipa->ipa_mask.sen_ip_src.s_addr = INADDR_BROADCAST;
		} else {
			ipa->ipa_info.sen_ip_src = ipo->ipo_addr.sen_ip_src;
			ipa->ipa_mask.sen_ip_src = ipo->ipo_mask.sen_ip_src;
		}

		if (ipo->ipo_mask.sen_ip_dst.s_addr == INADDR_ANY ||
		    ipo->ipo_addr.sen_ip_dst.s_addr == INADDR_ANY ||
		    ipsp_is_unspecified(ipo->ipo_dst)) {
			ipa->ipa_info.sen_ip_dst = ddst->sen_ip_dst;
			ipa->ipa_mask.sen_ip_dst.s_addr = INADDR_BROADCAST;
		} else {
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
#endif /* INET */

#ifdef INET6
	case SENT_IP6:
		ipa->ipa_info.sen_type = ipa->ipa_mask.sen_type = SENT_IP6;
		ipa->ipa_info.sen_ip6_direction =
		    ipo->ipo_addr.sen_ip6_direction;
		ipa->ipa_mask.sen_ip6_direction =
		    ipo->ipo_mask.sen_ip6_direction;

		if (IN6_IS_ADDR_UNSPECIFIED(&ipo->ipo_mask.sen_ip6_src) ||
		    IN6_IS_ADDR_UNSPECIFIED(&ipo->ipo_addr.sen_ip6_src) ||
		    ipsp_is_unspecified(ipo->ipo_dst)) {
			ipa->ipa_info.sen_ip6_src = ddst->sen_ip6_src;
			ipa->ipa_mask.sen_ip6_src = in6mask128;
		} else {
			ipa->ipa_info.sen_ip6_src = ipo->ipo_addr.sen_ip6_src;
			ipa->ipa_mask.sen_ip6_src = ipo->ipo_mask.sen_ip6_src;
		}

		if (IN6_IS_ADDR_UNSPECIFIED(&ipo->ipo_mask.sen_ip6_dst) ||
		    IN6_IS_ADDR_UNSPECIFIED(&ipo->ipo_addr.sen_ip6_dst) ||
		    ipsp_is_unspecified(ipo->ipo_dst)) {
			ipa->ipa_info.sen_ip6_dst = ddst->sen_ip6_dst;
			ipa->ipa_mask.sen_ip6_dst = in6mask128;
		} else {
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

	timeout_add(&ipa->ipa_timeout, ipsec_expire_acquire * hz);

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
	struct ipsec_policy sipon;
	struct tdb_ident *tdbi;
	struct m_tag *mtag;
	struct tdb *tdb = NULL;

	/* Sanity check. */
	if (inp == NULL)
		goto justreturn;

	/* Verify that we need to check for socket policy. */
	if ((inp->inp_seclevel[SL_ESP_TRANS] == IPSEC_LEVEL_BYPASS ||
	    inp->inp_seclevel[SL_ESP_TRANS] == IPSEC_LEVEL_NONE) &&
	    (inp->inp_seclevel[SL_ESP_NETWORK] == IPSEC_LEVEL_BYPASS ||
	    inp->inp_seclevel[SL_ESP_NETWORK] == IPSEC_LEVEL_NONE) &&
	    (inp->inp_seclevel[SL_AUTH] == IPSEC_LEVEL_BYPASS ||
	    inp->inp_seclevel[SL_AUTH] == IPSEC_LEVEL_NONE))
		goto justreturn;

	switch (direction) {
	case IPSP_DIRECTION_IN:
		/*
		 * Some further checking: if the socket has specified
		 * that it will accept unencrypted traffic, don't
		 * bother checking any further -- just accept the packet.
		 */
		if ((inp->inp_seclevel[SL_ESP_TRANS] == IPSEC_LEVEL_AVAIL ||
		    inp->inp_seclevel[SL_ESP_TRANS] == IPSEC_LEVEL_USE) &&
		    (inp->inp_seclevel[SL_ESP_NETWORK] == IPSEC_LEVEL_AVAIL ||
		    inp->inp_seclevel[SL_ESP_NETWORK] == IPSEC_LEVEL_USE) &&
		    (inp->inp_seclevel[SL_AUTH] == IPSEC_LEVEL_AVAIL ||
		    inp->inp_seclevel[SL_AUTH] == IPSEC_LEVEL_USE))
			goto justreturn;

		/* Initialize socket policy if unset. */
		if (inp->inp_ipo == NULL) {
			inp->inp_ipo = ipsec_add_policy(inp, af,
			    IPSP_DIRECTION_OUT);
			if (inp->inp_ipo == NULL) {
				*error = ENOBUFS;
				return NULL;
			}
		}

		/*
		 * So we *must* have protected traffic. Let's see what
		 * we have received then.
		 */
		if (inp->inp_tdb_in != NULL) {
			if (inp->inp_tdb_in == tdbp)
				goto justreturn; /* We received packet under a
						  * previously-accepted TDB. */

			/*
			 * We should be receiving protected traffic, and
			 * have an SA in place, but packet was received
			 * unprotected. Simply discard.
			 */
			if (tdbp == NULL) {
				*error = -EINVAL;
				return NULL;
			}

			/* Update, since we may need all the relevant info. */
			ipsec_update_policy(inp, inp->inp_ipo, af,
			    IPSP_DIRECTION_OUT);

			/*
			 * Check that the TDB the packet was received under
			 * is acceptable under the socket policy. If so,
			 * accept the packet; otherwise, discard.
			 */
			if (tdbp->tdb_sproto == inp->inp_ipo->ipo_sproto &&
			    !bcmp(&tdbp->tdb_src, &inp->inp_ipo->ipo_dst,
				SA_LEN(&tdbp->tdb_src.sa)) &&
			    ipsp_aux_match(tdbp,
				inp->inp_ipo->ipo_srcid, 
				inp->inp_ipo->ipo_dstid,
				NULL, NULL,
				&inp->inp_ipo->ipo_addr,
				&inp->inp_ipo->ipo_mask))
				goto justreturn;
			else {
				*error = -EINVAL;
				return NULL;
			}
		} else {
			/* Update, since we may need all the relevant info. */
			ipsec_update_policy(inp, inp->inp_ipo, af,
			    IPSP_DIRECTION_OUT);

			/*
			 * If the packet was received under an SA, see if
			 * it's acceptable under socket policy. If it is,
			 * accept the packet.
			 */
			if (tdbp != NULL &&
			    tdbp->tdb_sproto == inp->inp_ipo->ipo_sproto &&
			    !bcmp(&tdbp->tdb_src, &inp->inp_ipo->ipo_dst,
				SA_LEN(&tdbp->tdb_src.sa)) &&
			    ipsp_aux_match(tdbp,
				inp->inp_ipo->ipo_srcid,
				inp->inp_ipo->ipo_dstid,
				NULL, NULL,
				&inp->inp_ipo->ipo_addr,
				&inp->inp_ipo->ipo_mask))
				goto justreturn;

			/*
			 * If the packet was not received under an SA, or
			 * if the SA it was received under is not acceptable,
			 * see if we already have an acceptable SA
			 * established. If we do, discard packet.
			 */
			if (inp->inp_ipo->ipo_last_searched <=
			    ipsec_last_added) {
				inp->inp_ipo->ipo_last_searched = time.tv_sec;

				/* Do we have an SA already established ? */
				if (gettdbbysrc(&inp->inp_ipo->ipo_dst,
				    inp->inp_ipo->ipo_sproto,
				    inp->inp_ipo->ipo_srcid,
				    inp->inp_ipo->ipo_dstid, m, af,
				    &inp->inp_ipo->ipo_addr,
				    &inp->inp_ipo->ipo_mask) != NULL) {
					*error = -EINVAL;
					return NULL;
				}
				/* Fall through */
			}

			/*
			 * If we don't have an appropriate SA, acquire one
			 * and discard the packet.
			 */
			ipsp_acquire_sa(inp->inp_ipo, &inp->inp_ipo->ipo_dst,
			    &inp->inp_ipo->ipo_src, &inp->inp_ipo->ipo_addr, m);
			*error = -EINVAL;
			return NULL;
		}

		break;

	case IPSP_DIRECTION_OUT:
		/* Do we have a cached entry ? */
		if (inp->inp_tdb_out != NULL) {
			/*
			 * If we also have to apply a different TDB as
			 * a result of a system-wide policy, add a tag
			 * to the packet.
			 */
			if (ipo != NULL && m != NULL &&
			    ipo->ipo_tdb != NULL &&
			    ipo->ipo_tdb != inp->inp_tdb_out) {
				tdb = inp->inp_tdb_out;
				goto tagandreturn;
			} else
				return inp->inp_tdb_out;
		}

		/*
		 * We need to either find an SA with the appropriate
		 * characteristics and link it to the PCB, or acquire
		 * one.
		 */
		/* XXX Only support one policy/protocol for now. */
		if (inp->inp_ipo != NULL) {
			if (inp->inp_ipo->ipo_last_searched <=
			    ipsec_last_added) {
				inp->inp_ipo->ipo_last_searched = time.tv_sec;

				/* Update, just in case. */
				ipsec_update_policy(inp, inp->inp_ipo, af,
				    IPSP_DIRECTION_OUT);

				tdb = gettdbbyaddr(&inp->inp_ipo->ipo_dst,
				    inp->inp_ipo->ipo_sproto,
				    inp->inp_ipo->ipo_srcid,
				    inp->inp_ipo->ipo_dstid,
				    inp->inp_ipo->ipo_local_cred, m, af,
				    &inp->inp_ipo->ipo_addr,
				    &inp->inp_ipo->ipo_mask);
			}
		} else {
			/*
			 * Construct a pseudo-policy, with just the necessary
			 * fields.
			 */
			ipsec_update_policy(inp, &sipon, af,
			    IPSP_DIRECTION_OUT);

			tdb = gettdbbyaddr(&sipon.ipo_dst, IPPROTO_ESP, NULL,
			    NULL, NULL, m, af, &sipon.ipo_addr,
			    &sipon.ipo_mask);
		}

		/* If we found an appropriate SA... */
		if (tdb != NULL) {
			tdb_add_inp(tdb, inp, 0); /* Latch onto PCB. */

			if (ipo != NULL && ipo->ipo_tdb != NULL &&
			    ipo->ipo_tdb != inp->inp_tdb_out && m != NULL)
				goto tagandreturn;
			else
				return tdb;
		} else {
			/* Do we need to acquire one ? */
			switch (inp->inp_seclevel[SL_ESP_TRANS]) {
			case IPSEC_LEVEL_BYPASS:
			case IPSEC_LEVEL_AVAIL:
				/* No need to do anything. */
				goto justreturn;
			case IPSEC_LEVEL_USE:
			case IPSEC_LEVEL_REQUIRE:
			case IPSEC_LEVEL_UNIQUE:
				/* Initialize socket policy if unset. */
				if (inp->inp_ipo == NULL) {
					inp->inp_ipo = ipsec_add_policy(inp, af, IPSP_DIRECTION_OUT);
					if (inp->inp_ipo == NULL) {
						*error = ENOBUFS;
						return NULL;
					}
				}

				/* Acquire a new SA. */
				if ((*error = ipsp_acquire_sa(inp->inp_ipo,
				    &inp->inp_ipo->ipo_dst,
				    &inp->inp_ipo->ipo_src,
				    &inp->inp_ipo->ipo_addr, m)) == 0)
					*error = -EINVAL;

				return NULL;
			default:
				DPRINTF(("ipsp_spd_inp: unknown sock security"
				    " level %d",
				    inp->inp_seclevel[SL_ESP_TRANS]));
				*error = -EINVAL;
				return NULL;
			}
		}
		break;

	default:  /* Should never happen. */
		*error = -EINVAL;
		return NULL;
	}

 tagandreturn:
	if (tdb == NULL)
		goto justreturn;

	mtag = m_tag_get(PACKET_TAG_IPSEC_PENDING_TDB,
	    sizeof (struct tdb_ident), M_NOWAIT);
	if (mtag == NULL) {
		*error = ENOMEM;
		return NULL;
	}

	tdbi = (struct tdb_ident *)(mtag + 1);
	tdbi->spi = ipo->ipo_tdb->tdb_spi;
	tdbi->proto = ipo->ipo_tdb->tdb_sproto;
	bcopy(&ipo->ipo_tdb->tdb_dst, &tdbi->dst,
	    ipo->ipo_tdb->tdb_dst.sa.sa_len);
	m_tag_prepend(m, mtag);
	return tdb;

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

	TAILQ_FOREACH (ipa, &ipsec_acquire_head, ipa_next)
		if (ipa->ipa_seq == seq)
			return ipa;

	return NULL;
}
