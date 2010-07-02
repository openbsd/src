/*	$OpenBSD: ip_ether.c,v 1.55 2010/07/02 02:40:16 blambert Exp $  */
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

#include "bridge.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/proc.h>
#include <sys/sysctl.h>

#include <net/bpf.h>
#include <net/if.h>
#include <net/netisr.h>
#include <net/route.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/in_pcb.h>
#include <netinet/ip_var.h>
#endif /* INET */

#include <netinet/ip_ether.h>
#include <netinet/if_ether.h>

#include <net/if_gif.h>

#if NBRIDGE > 0
#include <net/if_bridge.h>
#endif
#ifdef MPLS
#include <netmpls/mpls.h>
#endif

#include "bpfilter.h"

#ifdef ENCDEBUG
#define DPRINTF(x)	if (encdebug) printf x
#else
#define DPRINTF(x)
#endif

#if NBRIDGE > 0
void	etherip_decap(struct mbuf *, int);
#endif
#ifdef MPLS
void	mplsip_decap(struct mbuf *, int);
#endif
struct gif_softc	*etherip_getgif(struct mbuf *);

/*
 * We can control the acceptance of EtherIP packets by altering the sysctl
 * net.inet.etherip.allow value. Zero means drop them, all else is acceptance.
 */
int etherip_allow = 0;

struct etheripstat etheripstat;

#ifdef INET
/*
 * etherip_input gets called when we receive an encapsulated packet.
 * Only a wrapper for the IPv4 case.
 */
void
etherip_input(struct mbuf *m, ...)
{
	struct ip *ip;
	va_list ap;
	int iphlen;

	ip = mtod(m, struct ip *);

	va_start(ap, m);
	iphlen = va_arg(ap, int);
	va_end(ap);

	switch (ip->ip_p) {
#if NBRIDGE > 0
	case IPPROTO_ETHERIP:
		/* If we do not accept EtherIP explicitly, drop. */
		if (!etherip_allow && (m->m_flags & (M_AUTH|M_CONF)) == 0) {
			DPRINTF(("etherip_input(): dropped due to policy\n"));
			etheripstat.etherip_pdrops++;
			m_freem(m);
			return;
		}
		etherip_decap(m, iphlen);
		return;
#endif
#ifdef MPLS
	case IPPROTO_MPLS:
		mplsip_decap(m, iphlen);
		return;
#endif
	default:
		DPRINTF(("etherip_input(): dropped, unhandled protcol \n"));
		etheripstat.etherip_pdrops++;
		m_freem(m);
		return;
	}
}
#endif

#ifdef INET6
int
etherip_input6(struct mbuf **m, int *offp, int proto)
{
	switch (proto) {
#if NBRIDGE > 0
	case IPPROTO_ETHERIP:
		/* If we do not accept EtherIP explicitly, drop. */
		if (proto == IPPROTO_ETHERIP && !etherip_allow &&
		    ((*m)->m_flags & (M_AUTH|M_CONF)) == 0) {
			DPRINTF(("etherip_input6(): dropped due to policy\n"));
			etheripstat.etherip_pdrops++;
			m_freem(*m);
			return IPPROTO_DONE;
		}
		etherip_decap(*m, *offp);
		return IPPROTO_DONE;
#endif
#ifdef MPLS
	case IPPROTO_MPLS:
		mplsip_decap(*m, *offp);
		return IPPROTO_DONE;
#endif
	default:
		DPRINTF(("etherip_input6(): dropped, unhandled protcol \n"));
		etheripstat.etherip_pdrops++;
		m_freem(*m);
		return IPPROTO_DONE;
	}
}
#endif

#if NBRIDGE > 0
void
etherip_decap(struct mbuf *m, int iphlen)
{
	struct ether_header eh;
	struct etherip_header eip;
	struct gif_softc *sc;
	int s;

	etheripstat.etherip_ipackets++;

	/*
	 * Make sure there's at least an ethernet header's and an EtherIP
	 * header's of worth of data after the outer IP header.
	 */
	if (m->m_pkthdr.len < iphlen + sizeof(struct ether_header) +
	    sizeof(struct etherip_header)) {
		DPRINTF(("etherip_input(): encapsulated packet too short\n"));
		etheripstat.etherip_hdrops++;
		m_freem(m);
		return;
	}

	/* Verify EtherIP version number */
	m_copydata(m, iphlen, sizeof(struct etherip_header), (caddr_t)&eip);
	if ((eip.eip_ver & ETHERIP_VER_VERS_MASK) != ETHERIP_VERSION) {
		DPRINTF(("etherip_input(): received EtherIP version number "
		    "%d not suppoorted\n", eip.eip_ver));
		etheripstat.etherip_adrops++;
		m_freem(m);
		return;
	}

	/*
	 * Note that the other potential failure of the above check is that the
	 * second nibble of the EtherIP header (the reserved part) is not
	 * zero; this is also invalid protocol behaviour.
	 */
	if (eip.eip_ver & ETHERIP_VER_RSVD_MASK) {
		DPRINTF(("etherip_input(): received EtherIP invalid EtherIP "
		    "header (reserved field non-zero\n"));
		etheripstat.etherip_adrops++;
		m_freem(m);
		return;
	}

	/* Finally, the pad value must be zero. */
	if (eip.eip_pad) {
		DPRINTF(("etherip_input(): received EtherIP invalid "
		    "pad value\n"));
		etheripstat.etherip_adrops++;
		m_freem(m);
		return;
	}

	/* Make sure the ethernet header at least is in the first mbuf. */
	if (m->m_len < iphlen + sizeof(struct ether_header) +
	    sizeof(struct etherip_header)) {
		if ((m = m_pullup(m, iphlen + sizeof(struct ether_header) +
		    sizeof(struct etherip_header))) == NULL) {
			DPRINTF(("etherip_input(): m_pullup() failed\n"));
			etheripstat.etherip_adrops++;
			return;
		}
	}

	sc = etherip_getgif(m);
	if (sc == NULL)
		return;
	if (sc->gif_if.if_bridge == NULL) {
		DPRINTF(("etherip_input(): interface not part of bridge\n"));
		etheripstat.etherip_noifdrops++;
		m_freem(m);
		return;
	}

	/* Chop off the `outer' IP and EtherIP headers and reschedule. */
	m_adj(m, iphlen + sizeof(struct etherip_header));

	/* Statistics */
	etheripstat.etherip_ibytes += m->m_pkthdr.len;

	/* Copy ethernet header */
	m_copydata(m, 0, sizeof(eh), (void *) &eh);

	/* Reset the flags based on the inner packet */
	m->m_flags &= ~(M_BCAST|M_MCAST|M_AUTH|M_CONF|M_AUTH_AH);
	if (eh.ether_dhost[0] & 1) {
		if (bcmp((caddr_t) etherbroadcastaddr,
		    (caddr_t)eh.ether_dhost, sizeof(etherbroadcastaddr)) == 0)
			m->m_flags |= M_BCAST;
		else
			m->m_flags |= M_MCAST;
	}

#if NBPFILTER > 0
	if (sc->gif_if.if_bpf)
		bpf_mtap_af(sc->gif_if.if_bpf, AF_LINK, m, BPF_DIRECTION_IN);
#endif

	/* Trim the beginning of the mbuf, to remove the ethernet header. */
	m_adj(m, sizeof(struct ether_header));

	/*
	 * Tap the packet off here for a bridge. bridge_input() returns
	 * NULL if it has consumed the packet.  In the case of gif's,
	 * bridge_input() returns non-NULL when an error occurs.
	 */
#if NPF > 0
	pf_pkt_addr_changed(m);
#endif
	m->m_pkthdr.rcvif = &sc->gif_if;
	m->m_pkthdr.rdomain = sc->gif_if.if_rdomain;
	if (m->m_flags & (M_BCAST|M_MCAST))
		sc->gif_if.if_imcasts++;

	s = splnet();
	m = bridge_input(&sc->gif_if, &eh, m);
	splx(s);
	if (m == NULL)
		return;

	etheripstat.etherip_noifdrops++;
	m_freem(m);
	return;
}
#endif

#ifdef MPLS
void
mplsip_decap(struct mbuf *m, int iphlen)
{
	struct gif_softc *sc;
	struct ifqueue *ifq;
	int s;

	etheripstat.etherip_ipackets++;

	/*
	 * Make sure there's at least one MPLS label worth of data after
	 * the outer IP header.
	 */
	if (m->m_pkthdr.len < iphlen + sizeof(struct shim_hdr)) {
		DPRINTF(("mplsip_input(): encapsulated packet too short\n"));
		etheripstat.etherip_hdrops++;
		m_freem(m);
		return;
	}

	/* Make sure the mpls label at least is in the first mbuf. */
	if (m->m_len < iphlen + sizeof(struct shim_hdr)) {
		if ((m = m_pullup(m, iphlen + sizeof(struct shim_hdr))) ==
		    NULL) {
			DPRINTF(("mplsip_input(): m_pullup() failed\n"));
			etheripstat.etherip_adrops++;
			return;
		}
	}

	sc = etherip_getgif(m);
	if (sc == NULL)
		return;

	/* Chop off the `outer' IP header and reschedule. */
	m_adj(m, iphlen);

	/* Statistics */
	etheripstat.etherip_ibytes += m->m_pkthdr.len;

	/* Reset the flags based */
	m->m_flags &= ~(M_BCAST|M_MCAST);

#if NBPFILTER > 0
	if (sc->gif_if.if_bpf)
		bpf_mtap_af(sc->gif_if.if_bpf, AF_MPLS, m, BPF_DIRECTION_IN);
#endif

	m->m_pkthdr.rcvif = &sc->gif_if;
	m->m_pkthdr.rdomain = sc->gif_if.if_rdomain;
#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ifq = &mplsintrq;
	s = splnet();
	if (IF_QFULL(ifq)) {
		IF_DROP(ifq);
		m_freem(m);
		etheripstat.etherip_qfull++;
		splx(s);

		DPRINTF(("mplsip_input(): packet dropped because of full "
		    "queue\n"));
		return;
	}
	IF_ENQUEUE(ifq, m);
	schednetisr(NETISR_MPLS);
	splx(s);
	return;
}
#endif

struct gif_softc *
etherip_getgif(struct mbuf *m)
{
	union sockaddr_union ssrc, sdst;
	struct gif_softc *sc;
	u_int8_t v;

	/* Copy the addresses for use later. */
	bzero(&ssrc, sizeof(ssrc));
	bzero(&sdst, sizeof(sdst));

	v = *mtod(m, u_int8_t *);
	switch (v >> 4) {
#ifdef INET
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
#endif /* INET */
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
		DPRINTF(("etherip_input(): invalid protocol %d\n", v));
		m_freem(m);
		etheripstat.etherip_hdrops++;
		return NULL;
	}

	/* Find appropriate gif(4) interface */
	LIST_FOREACH(sc, &gif_softc_list, gif_list) {
		if ((sc->gif_psrc == NULL) ||
		    (sc->gif_pdst == NULL) ||
		    !(sc->gif_if.if_flags & (IFF_UP|IFF_RUNNING)))
			continue;

		if (!bcmp(sc->gif_psrc, &sdst, sc->gif_psrc->sa_len) &&
		    !bcmp(sc->gif_pdst, &ssrc, sc->gif_pdst->sa_len))
			break;
	}

	/* None found. */
	if (sc == NULL) {
		DPRINTF(("etherip_input(): no interface found\n"));
		etheripstat.etherip_noifdrops++;
		m_freem(m);
		return NULL;
	}

	return sc;
}

int
etherip_output(struct mbuf *m, struct tdb *tdb, struct mbuf **mp, int proto)
{
#ifdef INET
	struct ip *ipo;
#endif /* INET */
#ifdef INET6
	struct ip6_hdr *ip6;
#endif /* INET6 */
	struct etherip_header eip;
	ushort hlen;

	/* Some address family sanity checks. */
	if ((tdb->tdb_src.sa.sa_family != 0) &&
	    (tdb->tdb_src.sa.sa_family != AF_INET) &&
	    (tdb->tdb_src.sa.sa_family != AF_INET6)) {
		DPRINTF(("etherip_output(): IP in protocol-family <%d> "
		    "attempted, aborting", tdb->tdb_src.sa.sa_family));
		etheripstat.etherip_adrops++;
		m_freem(m);
		return EINVAL;
	}

	if ((tdb->tdb_dst.sa.sa_family != AF_INET) &&
	    (tdb->tdb_dst.sa.sa_family != AF_INET6)) {
		DPRINTF(("etherip_output(): IP in protocol-family <%d> "
		    "attempted, aborting", tdb->tdb_dst.sa.sa_family));
		etheripstat.etherip_adrops++;
		m_freem(m);
		return EINVAL;
	}

	if (tdb->tdb_dst.sa.sa_family != tdb->tdb_src.sa.sa_family) {
		DPRINTF(("etherip_output(): mismatch in tunnel source and "
		    "destination address protocol families (%d/%d), aborting",
		    tdb->tdb_src.sa.sa_family, tdb->tdb_dst.sa.sa_family));
		etheripstat.etherip_adrops++;
		m_freem(m);
		return EINVAL;
	}

	switch (tdb->tdb_dst.sa.sa_family) {
#ifdef INET
	case AF_INET:
		hlen = sizeof(struct ip);
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		hlen = sizeof(struct ip6_hdr);
		break;
#endif /* INET6 */
	default:
		DPRINTF(("etherip_output(): unsupported tunnel protocol "
		    "family <%d>, aborting", tdb->tdb_dst.sa.sa_family));
		etheripstat.etherip_adrops++;
		m_freem(m);
		return EINVAL;
	}

	if (proto == IPPROTO_ETHERIP)
		/* Don't forget the EtherIP header. */
		hlen += sizeof(struct etherip_header);

	M_PREPEND(m, hlen, M_DONTWAIT);
	if (m == NULL) {
		DPRINTF(("etherip_output(): M_PREPEND failed\n"));
		etheripstat.etherip_adrops++;
		return ENOBUFS;
	}

	/* Statistics */
	etheripstat.etherip_opackets++;
	etheripstat.etherip_obytes += m->m_pkthdr.len - hlen;

	switch (tdb->tdb_dst.sa.sa_family) {
#ifdef INET
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
#endif /* INET */
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

	if (proto == IPPROTO_ETHERIP) {
		/* Set the version number */
		eip.eip_ver = ETHERIP_VERSION & ETHERIP_VER_VERS_MASK;
		eip.eip_pad = 0;
		m_copyback(m, hlen - sizeof(struct etherip_header),
		    sizeof(struct etherip_header), &eip, M_NOWAIT);
	}

	*mp = m;

	return 0;
}

int
etherip_sysctl(name, namelen, oldp, oldlenp, newp, newlen)
	int *name;
	u_int namelen;
	void *oldp, *newp;
	size_t *oldlenp, newlen;
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case ETHERIPCTL_ALLOW:
		return (sysctl_int(oldp, oldlenp, newp, newlen,
		    &etherip_allow));
	case ETHERIPCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &etheripstat, sizeof(etheripstat)));
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}
