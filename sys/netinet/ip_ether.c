/*	$OpenBSD: ip_ether.c,v 1.97 2017/11/20 10:35:24 mpi Exp $  */
/*
 * The author of this code is Angelos D. Keromytis (kermit@adk.gr)
 *
 * This code was written by Angelos D. Keromytis for OpenBSD in October 1999.
 *
 * Copyright (C) 1999-2001 Angelos D. Keromytis.
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

/*
 * Ethernet-inside-IP processing (RFC3378).
 */

#include "pf.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>

#include <netinet/ip_ether.h>
#include <netinet/if_ether.h>

#include <net/if_gif.h>

#ifdef MPLS
#include <netmpls/mpls.h>
#endif
#if NPF > 0
#include <net/pfvar.h>
#endif

#include "bpfilter.h"

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#ifdef MPLS
void	mplsip_decap(struct mbuf *, int);
struct gif_softc	*mplsip_getgif(struct mbuf *);

/*
 * mplsip_input gets called when we receive an encapsulated packet.
 */
int
mplsip_input(struct mbuf **mp, int *offp, int proto, int af)
{
	switch (proto) {
	case IPPROTO_MPLS:
		mplsip_decap(*mp, *offp);
		return IPPROTO_DONE;
	default:
		DPRINTF(("%s: dropped, unhandled protocol\n", __func__));
		m_freemp(mp);
		return IPPROTO_DONE;
	}
}

void
mplsip_decap(struct mbuf *m, int iphlen)
{
	struct gif_softc *sc;

	/*
	 * Make sure there's at least one MPLS label worth of data after
	 * the outer IP header.
	 */
	if (m->m_pkthdr.len < iphlen + sizeof(struct shim_hdr)) {
		DPRINTF(("%s: encapsulated packet too short\n", __func__));
		m_freem(m);
		return;
	}

	/* Make sure the mpls label at least is in the first mbuf. */
	if (m->m_len < iphlen + sizeof(struct shim_hdr)) {
		if ((m = m_pullup(m, iphlen + sizeof(struct shim_hdr))) ==
		    NULL) {
			DPRINTF(("%s: m_pullup() failed\n", __func__));
			return;
		}
	}

	sc = mplsip_getgif(m);
	if (sc == NULL)
		return;

	/* Chop off the `outer' IP header and reschedule. */
	m_adj(m, iphlen);

	/* Reset the flags based */
	m->m_flags &= ~(M_BCAST|M_MCAST);

#if NBPFILTER > 0
	if (sc->gif_if.if_bpf)
		bpf_mtap_af(sc->gif_if.if_bpf, AF_MPLS, m, BPF_DIRECTION_IN);
#endif

	m->m_pkthdr.ph_ifidx = sc->gif_if.if_index;
	m->m_pkthdr.ph_rtableid = sc->gif_if.if_rdomain;
#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	mpls_input(m);
}

struct gif_softc *
mplsip_getgif(struct mbuf *m)
{
	union sockaddr_union ssrc, sdst;
	struct gif_softc *sc;
	u_int8_t v;

	/* Copy the addresses for use later. */
	memset(&ssrc, 0, sizeof(ssrc));
	memset(&sdst, 0, sizeof(sdst));

	v = *mtod(m, u_int8_t *);
	switch (v >> 4) {
	case 4:
		ssrc.sa.sa_len = sdst.sa.sa_len = sizeof(struct sockaddr_in);
		ssrc.sa.sa_family = sdst.sa.sa_family = AF_INET;
		m_copydata(m, offsetof(struct ip, ip_src),
		    sizeof(struct in_addr),
		    (caddr_t) &ssrc.sin.sin_addr);
		m_copydata(m, offsetof(struct ip, ip_dst),
		    sizeof(struct in_addr),
		    (caddr_t) &sdst.sin.sin_addr);
		break;
#ifdef INET6
	case 6:
		ssrc.sa.sa_len = sdst.sa.sa_len = sizeof(struct sockaddr_in6);
		ssrc.sa.sa_family = sdst.sa.sa_family = AF_INET6;
		m_copydata(m, offsetof(struct ip6_hdr, ip6_src),
		    sizeof(struct in6_addr),
		    (caddr_t) &ssrc.sin6.sin6_addr);
		m_copydata(m, offsetof(struct ip6_hdr, ip6_dst),
		    sizeof(struct in6_addr),
		    (caddr_t) &sdst.sin6.sin6_addr);
		break;
#endif /* INET6 */
	default:
		DPRINTF(("%s: invalid protocol %d\n", __func__, v));
		m_freem(m);
		return NULL;
	}

	NET_ASSERT_LOCKED();
	/* Find appropriate gif(4) interface */
	LIST_FOREACH(sc, &gif_softc_list, gif_list) {
		if ((sc->gif_psrc == NULL) ||
		    (sc->gif_pdst == NULL) ||
		    !(sc->gif_if.if_flags & (IFF_UP|IFF_RUNNING)))
			continue;

		if (!memcmp(sc->gif_psrc, &sdst, sc->gif_psrc->sa_len) &&
		    !memcmp(sc->gif_pdst, &ssrc, sc->gif_pdst->sa_len))
			break;
	}

	/* None found. */
	if (sc == NULL) {
		DPRINTF(("%s: no interface found\n", __func__));
		m_freem(m);
		return NULL;
	}

	return sc;
}

int
mplsip_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int proto)
{
	struct ip *ipo;
#ifdef INET6
	struct ip6_hdr *ip6;
#endif /* INET6 */
	ushort hlen;

	/* Some address family sanity checks. */
	if ((tdb->tdb_src.sa.sa_family != 0) &&
	    (tdb->tdb_src.sa.sa_family != AF_INET) &&
	    (tdb->tdb_src.sa.sa_family != AF_INET6)) {
		DPRINTF(("%s: IP in protocol-family <%d> attempted, aborting",
		    __func__, tdb->tdb_src.sa.sa_family));
		m_freem(m);
		return EINVAL;
	}

	if ((tdb->tdb_dst.sa.sa_family != AF_INET) &&
	    (tdb->tdb_dst.sa.sa_family != AF_INET6)) {
		DPRINTF(("%s: IP in protocol-family <%d> attempted, aborting",
		    __func__, tdb->tdb_dst.sa.sa_family));
		m_freem(m);
		return EINVAL;
	}

	if (tdb->tdb_dst.sa.sa_family != tdb->tdb_src.sa.sa_family) {
		DPRINTF(("%s: mismatch in tunnel source and destination address"
		    " protocol families (%d/%d), aborting", __func__,
		    tdb->tdb_src.sa.sa_family, tdb->tdb_dst.sa.sa_family));
		m_freem(m);
		return EINVAL;
	}

	switch (tdb->tdb_dst.sa.sa_family) {
	case AF_INET:
		hlen = sizeof(struct ip);
		break;
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif /* INET6 */
	default:
		DPRINTF(("%s: unsupported tunnel protocol family <%d>, "
		    "aborting", __func__, tdb->tdb_dst.sa.sa_family));
		m_freem(m);
		return EINVAL;
	}

	M_PREPEND(m, hlen, M_DONTWAIT);
	if (m == NULL) {
		DPRINTF(("%s: M_PREPEND failed\n", __func__));
		return ENOBUFS;
	}

	/*
	 * Normalize mbuf so that it can be reinjected into higherlevel
	 * output functions (alignment also required in this function).
	 */
	if ((long)mtod(m, caddr_t) & 0x03) {
		int off = (long)mtod(m, caddr_t) & 0x03;
		if (M_LEADINGSPACE(m) < off)
			panic("mplsip_output: no space for align fixup");
		m->m_data -= off;
		memmove(mtod(m, caddr_t), mtod(m, caddr_t) + off, m->m_len);
	}

	switch (tdb->tdb_dst.sa.sa_family) {
	case AF_INET:
		ipo = mtod(m, struct ip *);

		ipo->ip_v = IPVERSION;
		ipo->ip_hl = 5;
		ipo->ip_len = htons(m->m_pkthdr.len);
		ipo->ip_ttl = ip_defttl;
		ipo->ip_p = proto;
		ipo->ip_tos = 0;
		ipo->ip_off = 0;
		ipo->ip_sum = 0;
		ipo->ip_id = htons(ip_randomid());

		/*
		 * We should be keeping tunnel soft-state and send back
		 * ICMPs as needed.
		 */

		ipo->ip_src = tdb->tdb_src.sin.sin_addr;
		ipo->ip_dst = tdb->tdb_dst.sin.sin_addr;
		break;
#ifdef INET6
	case AF_INET6:
		ip6 = mtod(m, struct ip6_hdr *);

		ip6->ip6_flow = 0;
		ip6->ip6_nxt = proto;
		ip6->ip6_vfc &= ~IPV6_VERSION_MASK;
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_plen = htons(m->m_pkthdr.len - sizeof(*ip6));
		ip6->ip6_hlim = ip_defttl;
		ip6->ip6_dst = tdb->tdb_dst.sin6.sin6_addr;
		ip6->ip6_src = tdb->tdb_src.sin6.sin6_addr;
		break;
#endif /* INET6 */
	}

	*mp = m;

	return 0;
}
#endif /* MPLS */
