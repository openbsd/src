/*      $OpenBSD: if_gre.c,v 1.97 2018/02/09 00:03:13 dlg Exp $ */
/*	$NetBSD: if_gre.c,v 1.9 1999/10/25 19:18:11 drochner Exp $ */

/*
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Heiko W.Rupp <hwr@pilhuhn.de>
 *
 * IPv6-over-GRE contributed by Gert Doering <gert@greenie.muc.de>
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Encapsulate L3 protocols into IP, per RFC 1701 and 1702.
 * See gre(4) for more details.
 * Also supported: IP in IP encapsulation (proto 55) per RFC 2004.
 */

#include "bpfilter.h"
#include "pf.h"

#include <sys/param.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/kernel.h>
#include <sys/systm.h>
#include <sys/errno.h>
#include <sys/timeout.h>
#include <sys/tree.h>

#include <crypto/siphash.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_media.h>
#include <net/route.h>

#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>

#ifdef INET6
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#endif

#ifdef PIPEX
#include <net/pipex.h>
#endif

#ifdef MPLS
#include <netmpls/mpls.h>
#endif /* MPLS */

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#if NPF > 0
#include <net/pfvar.h>
#endif

#include <net/if_gre.h>

#include <netinet/ip_gre.h>
#include <sys/sysctl.h>

/*
 * packet formats
 */
struct gre_header {
	uint16_t		gre_flags;
#define GRE_CP				0x8000  /* Checksum Present */
#define GRE_KP				0x2000  /* Key Present */
#define GRE_SP				0x1000  /* Sequence Present */

#define GRE_VERS_MASK			0x0007
#define GRE_VERS_0			0x0000
#define GRE_VERS_1			0x0001

	uint16_t		gre_proto;
} __packed __aligned(4);

struct gre_h_cksum {
	uint16_t		gre_cksum;
	uint16_t		gre_reserved1;
} __packed __aligned(4);

struct gre_h_key {
	uint32_t		gre_key;
} __packed __aligned(4);

struct gre_h_seq {
	uint32_t		gre_seq;
} __packed __aligned(4);

struct gre_h_wccp {
	uint8_t			wccp_flags;
	uint8_t			service_id;
	uint8_t			alt_bucket;
	uint8_t			pri_bucket;
} __packed __aligned(4);

#define GRE_WCCP 0x883e

/*
 * GRE tunnel metadata
 */

#define GRE_KA_NONE		0
#define GRE_KA_DOWN		1
#define GRE_KA_HOLD		2
#define GRE_KA_UP		3

struct gre_tunnel {
	RBT_ENTRY(gre_entry)	t_entry;

	uint32_t		t_key_mask;
#define GRE_KEY_NONE			htonl(0x00000000U)
#define GRE_KEY_ENTROPY			htonl(0xffffff00U)
#define GRE_KEY_MASK			htonl(0xffffffffU)
	uint32_t		t_key;

	u_int			t_rtableid;
	uint32_t		t_src[4];
	uint32_t		t_dst[4];
	int			t_ttl;
	sa_family_t		t_af;
};

RBT_HEAD(gre_tree, gre_tunnel);

static inline int
		gre_cmp(const struct gre_tunnel *, const struct gre_tunnel *);

RBT_PROTOTYPE(gre_tree, gre_tunnel, t_entry, gre_cmp);

static int	gre_set_tunnel(struct gre_tunnel *, struct if_laddrreq *);
static int	gre_get_tunnel(struct gre_tunnel *, struct if_laddrreq *);
static int	gre_del_tunnel(struct gre_tunnel *);

static int	gre_set_vnetid(struct gre_tunnel *, struct ifreq *);
static int	gre_get_vnetid(struct gre_tunnel *, struct ifreq *);
static int	gre_del_vnetid(struct gre_tunnel *);

static struct mbuf *
		gre_ip_encap(const struct gre_tunnel *, struct mbuf *,
		    uint8_t, uint8_t);
static int
		gre_ip_output(const struct gre_tunnel *, struct mbuf *,
		    uint8_t, uint8_t);

static int	gre_tunnel_ioctl(struct ifnet *, struct gre_tunnel *,
		    u_long, void *);

/*
 * layer 3 GRE tunnels
 */

struct gre_softc {
	struct gre_tunnel	sc_tunnel; /* must be first */
	struct ifnet		sc_if;

	struct timeout		sc_ka_send;
	struct timeout		sc_ka_hold;

	unsigned int		sc_ka_state;
	unsigned int		sc_ka_timeo;
	unsigned int		sc_ka_count;

	unsigned int		sc_ka_holdmax;
	unsigned int		sc_ka_holdcnt;

	SIPHASH_KEY		sc_ka_key;
	uint32_t		sc_ka_bias;
	int			sc_ka_recvtm;
};

struct gre_keepalive {
	uint32_t		gk_uptime;
	uint32_t		gk_random;
	uint8_t			gk_digest[SIPHASH_DIGEST_LENGTH];
} __packed __aligned(4);

static int	gre_clone_create(struct if_clone *, int);
static int	gre_clone_destroy(struct ifnet *);

struct if_clone gre_cloner =
    IF_CLONE_INITIALIZER("gre", gre_clone_create, gre_clone_destroy);

struct gre_tree gre_softcs = RBT_INITIALIZER();

static int	gre_output(struct ifnet *, struct mbuf *, struct sockaddr *,
		    struct rtentry *);
static void	gre_start(struct ifnet *);
static int	gre_ioctl(struct ifnet *, u_long, caddr_t);

static int	gre_up(struct gre_softc *);
static int	gre_down(struct gre_softc *);
static void	gre_link_state(struct gre_softc *);

static int	gre_input_key(struct mbuf **, int *, int, int,
		    struct gre_tunnel *);

static struct mbuf *
		gre_encap(struct gre_tunnel *, struct mbuf *, uint16_t);

static void	gre_keepalive_send(void *);
static void	gre_keepalive_recv(struct ifnet *ifp, struct mbuf *);
static void	gre_keepalive_hold(void *);

/*
 * Ethernet GRE tunnels
 */

struct egre_softc {
	struct gre_tunnel	sc_tunnel; /* must be first */
	struct arpcom		sc_ac;
	struct ifmedia		sc_media;
};

static int	egre_clone_create(struct if_clone *, int);
static int	egre_clone_destroy(struct ifnet *);

static void	egre_start(struct ifnet *);
static int	egre_ioctl(struct ifnet *, u_long, caddr_t);
static int	egre_media_change(struct ifnet *);
static void	egre_media_status(struct ifnet *, struct ifmediareq *);

static int	egre_up(struct egre_softc *);
static int	egre_down(struct egre_softc *);

static int	egre_input(const struct gre_tunnel *, struct mbuf *, int);

struct if_clone egre_cloner =
    IF_CLONE_INITIALIZER("egre", egre_clone_create, egre_clone_destroy);

struct gre_tree egre_softcs = RBT_INITIALIZER();

/*
 * It is not easy to calculate the right value for a GRE MTU.
 * We leave this task to the admin and use the same default that
 * other vendors use.
 */
#define GREMTU 1476

/*
 * We can control the acceptance of GRE and MobileIP packets by
 * altering the sysctl net.inet.gre.allow values
 * respectively. Zero means drop them, all else is acceptance.  We can also
 * control acceptance of WCCPv1-style GRE packets through the
 * net.inet.gre.wccp value, but be aware it depends upon normal GRE being
 * allowed as well.
 *
 */
int gre_allow = 0;
int gre_wccp = 0;

void
greattach(int n)
{
	if_clone_attach(&gre_cloner);
	if_clone_attach(&egre_cloner);
}

static int
gre_clone_create(struct if_clone *ifc, int unit)
{
	struct gre_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	snprintf(sc->sc_if.if_xname, sizeof sc->sc_if.if_xname, "%s%d",
	    ifc->ifc_name, unit);

	ifp = &sc->sc_if;
	ifp->if_softc = sc;
	ifp->if_type = IFT_TUNNEL;
	ifp->if_hdrlen = 24; /* IP + GRE */
	ifp->if_mtu = GREMTU;
	ifp->if_flags = IFF_POINTOPOINT|IFF_MULTICAST;
	ifp->if_xflags = IFXF_CLONED;
	ifp->if_output = gre_output;
	ifp->if_start = gre_start;
	ifp->if_ioctl = gre_ioctl;
	ifp->if_rtrequest = p2p_rtrequest;

	sc->sc_tunnel.t_ttl = ip_defttl;

	timeout_set(&sc->sc_ka_send, gre_keepalive_send, sc);
	timeout_set_proc(&sc->sc_ka_hold, gre_keepalive_hold, sc);
	sc->sc_ka_state = GRE_KA_NONE;

	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(uint32_t));
#endif

	return (0);
}

static int
gre_clone_destroy(struct ifnet *ifp)
{
	struct gre_softc *sc = ifp->if_softc;

	NET_LOCK();
	if (ISSET(ifp->if_flags, IFF_RUNNING))
		gre_down(sc);
	NET_UNLOCK();

	if_detach(ifp);

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

static int
egre_clone_create(struct if_clone *ifc, int unit)
{
	struct egre_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_WAITOK|M_ZERO);
	ifp = &sc->sc_ac.ac_if;

	snprintf(ifp->if_xname, sizeof(ifp->if_xname), "%s%d",
	    ifc->ifc_name, unit);

	ifp->if_softc = sc;
	ifp->if_mtu = 1500; /* XXX */
	ifp->if_ioctl = egre_ioctl;
	ifp->if_start = egre_start;
	ifp->if_xflags = IFXF_CLONED;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ether_fakeaddr(ifp);

	sc->sc_tunnel.t_ttl = ip_defttl;

	ifmedia_init(&sc->sc_media, 0, egre_media_change, egre_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	return (0);
}

static int
egre_clone_destroy(struct ifnet *ifp)
{
	struct egre_softc *sc = ifp->if_softc;

	NET_LOCK();
	if (ISSET(ifp->if_flags, IFF_RUNNING))
		egre_down(sc);
	NET_UNLOCK();

	ifmedia_delete_instance(&sc->sc_media, IFM_INST_ANY);
	ether_ifdetach(ifp);
	if_detach(ifp);

	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

int
gre_input(struct mbuf **mp, int *offp, int type, int af)
{
	struct mbuf *m = *mp;
	struct gre_tunnel key;
	struct ip *ip;

	ip = mtod(m, struct ip *);

	key.t_af = AF_INET;
	key.t_ttl = ip->ip_ttl;
	key.t_src[0] = ip->ip_dst.s_addr;
	key.t_dst[0] = ip->ip_src.s_addr;

	if (gre_input_key(mp, offp, type, af, &key) == -1)
		return (rip_input(mp, offp, type, af));

	return (IPPROTO_DONE);
}

#ifdef INET6
int
gre_input6(struct mbuf **mp, int *offp, int type, int af)
{
	struct mbuf *m = *mp;
	struct gre_tunnel key;
	struct ip6_hdr *ip6;

	ip6 = mtod(m, struct ip6_hdr *);

	key.t_af = AF_INET6;
	key.t_ttl = ip6->ip6_hlim;
	memcpy(key.t_src, &ip6->ip6_dst, sizeof(key.t_src));
	memcpy(key.t_dst, &ip6->ip6_src, sizeof(key.t_dst));

	if (gre_input_key(mp, offp, type, af, &key) == -1)
		return (rip6_input(mp, offp, type, af));

	return (IPPROTO_DONE);
}
#endif /* INET6 */

static int
gre_input_key(struct mbuf **mp, int *offp, int type, int af,
    struct gre_tunnel *key)
{
	struct mbuf *m = *mp;
	int iphlen = *offp, hlen;
	struct gre_softc *sc;
	struct ifnet *ifp;
	caddr_t buf;
	struct gre_header *gh;
	struct gre_h_key *gkh;
	void (*input)(struct ifnet *, struct mbuf *);
	int bpf_af = AF_UNSPEC; /* bpf */
	int mcast = 0;
	int ttloff;

	if (!gre_allow)
		goto decline;

	hlen = iphlen + sizeof(*gh);
	if (m->m_pkthdr.len < hlen)
		goto decline;

	m = m_pullup(m, hlen);
	if (m == NULL)
		return (IPPROTO_DONE);

	buf = mtod(m, caddr_t);
	gh = (struct gre_header *)(buf + iphlen);

	/* check the version */
	switch (gh->gre_flags & htons(GRE_VERS_MASK)) {
	case htons(GRE_VERS_0):
		break;

	case htons(GRE_VERS_1):
#ifdef PIPEX
		if (pipex_enable) {
			struct pipex_session *session;

			session = pipex_pptp_lookup_session(m);
			if (session != NULL &&
			    pipex_pptp_input(m, session) == NULL)
				return (IPPROTO_DONE);
		}
#endif
		/* FALLTHROUGH */
	default:
		goto decline;
	}

	/* the only optional bit in the header is K flag */
	if ((gh->gre_flags & htons(~(GRE_KP|GRE_VERS_MASK))) != htons(0))
		goto decline;

	if (gh->gre_flags & htons(GRE_KP)) {
		hlen += sizeof(*gkh);
		if (m->m_pkthdr.len < hlen)
			goto decline;

		m = m_pullup(m, hlen);
		if (m == NULL)
			return (IPPROTO_DONE);

		buf = mtod(m, caddr_t);
		gh = (struct gre_header *)(buf + iphlen);
		gkh = (struct gre_h_key *)(gh + 1);

		key->t_key_mask = GRE_KEY_MASK;
		key->t_key = gkh->gre_key;
	} else
		key->t_key_mask = GRE_KEY_NONE;

	key->t_rtableid = m->m_pkthdr.ph_rtableid;

	switch (gh->gre_proto) {
	case htons(GRE_WCCP):
		/* WCCP/GRE:
		 *   So far as I can see (and test) it seems that Cisco's WCCP
		 *   GRE tunnel is precisely a IP-in-GRE tunnel that differs
		 *   only in its protocol number.  At least, it works for me.
		 *
		 *   The Internet Drafts can be found if you look for
		 *   the following:
		 *     draft-forster-wrec-wccp-v1-00.txt
		 *     draft-wilson-wrec-wccp-v2-01.txt
		 *
		 *   So yes, we're doing a fall-through (unless, of course,
		 *   net.inet.gre.wccp is 0).
		 */
		switch (gre_wccp) {
		case 1:
			break;
		case 2:
			hlen += sizeof(gre_wccp);
			break;
		case 0:
		default:
			goto decline;
		}

		/* FALLTHROUGH */
	case htons(ETHERTYPE_IP):
#if NBPFILTER > 0
		bpf_af = AF_INET;
#endif
		ttloff = offsetof(struct ip, ip_ttl);
		input = ipv4_input;
		break;
#ifdef INET6
	case htons(ETHERTYPE_IPV6):
#if NBPFILTER > 0
		bpf_af = AF_INET6;
#endif
		ttloff = offsetof(struct ip6_hdr, ip6_hlim);
		input = ipv6_input;
		break;
#endif
#ifdef MPLS
	case htons(ETHERTYPE_MPLS_MCAST):
		mcast = M_MCAST|M_BCAST;
		/* fallthrough */
	case htons(ETHERTYPE_MPLS):
#if NBPFILTER > 0
		bpf_af = AF_MPLS;
#endif
		ttloff = 3; /* XXX */
		input = mpls_input;
		break;
#endif
	case htons(0):
#if NBPFILTER > 0
		bpf_af = AF_UNSPEC;
#endif
		input = gre_keepalive_recv;
		break;

	case htons(ETHERTYPE_TRANSETHER):
		if (egre_input(key, m, hlen) == -1)
			goto decline;
	default:
		goto decline;
	}

	sc = (struct gre_softc *)RBT_FIND(gre_tree, &gre_softcs, key);
	if (sc == NULL)
		goto decline;

	ifp = &sc->sc_if;

	m_adj(m, hlen);

	if (sc->sc_tunnel.t_ttl == -1) {
		m = m_pullup(m, ttloff + 1);
		if (m == NULL)
			return (IPPROTO_DONE);

		*(m->m_data + ttloff) = key->t_ttl;
	}

	m->m_flags &= ~(M_MCAST|M_BCAST);
	m->m_flags |= mcast;
	m->m_pkthdr.ph_ifidx = ifp->if_index;
	m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ifp->if_ipackets++;
	ifp->if_ibytes += m->m_pkthdr.len;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_af(ifp->if_bpf, bpf_af, m, BPF_DIRECTION_IN);
#endif

	(*input)(ifp, m);
	return (IPPROTO_DONE);
decline:
	mp = &m;
	return (-1);
}

static int
egre_input(const struct gre_tunnel *key, struct mbuf *m, int hlen)
{
	struct egre_softc *sc;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct mbuf *n;
	int off;

	sc = (struct egre_softc *)RBT_FIND(gre_tree, &egre_softcs, key);
	if (sc == NULL)
		return (-1);

	/* it's ours now */

	m_adj(m, hlen);

	if (m->m_pkthdr.len < sizeof(struct ether_header)) {
		m_freem(m);
		return (0);
	}

	m = m_pullup(m, sizeof(struct ether_header));
	if (m == NULL)
		return (0);

	n = m_getptr(m, sizeof(struct ether_header), &off);
	if (n == NULL) {
		m_freem(m);
		return (0);
	}

	if (!ALIGNED_POINTER(mtod(n, caddr_t) + off, uint32_t)) {
		n = m_dup_pkt(m, ETHER_ALIGN, M_NOWAIT);
		m_freem(m);
		if (n == NULL)
			return (0);
		m = n;
	}

	m->m_flags &= ~(M_MCAST|M_BCAST);

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	ml_enqueue(&ml, m);
	if_input(&sc->sc_ac.ac_if, &ml);

	return (0);
}

static void
gre_keepalive_recv(struct ifnet *ifp, struct mbuf *m)
{
	struct gre_softc *sc = ifp->if_softc;
	struct gre_keepalive *gk;
	SIPHASH_CTX ctx;
	uint8_t digest[SIPHASH_DIGEST_LENGTH];
	int uptime, delta;
	int tick = ticks;

	if (sc->sc_ka_state == GRE_KA_NONE)
		goto drop;

	if (m->m_pkthdr.len < sizeof(*gk))
		goto drop;
	m = m_pullup(m, sizeof(*gk));
	if (m == NULL)
		return;

	gk = mtod(m, struct gre_keepalive *);
	uptime = bemtoh32(&gk->gk_uptime) - sc->sc_ka_bias;
	delta = tick - uptime;
	if (delta < 0)
		goto drop;
	if (delta > hz * 10) /* magic */
		goto drop;

	/* avoid too much siphash work */
	delta = tick - sc->sc_ka_recvtm;
	if (delta > 0 && delta < (hz / 10))
		goto drop;

	SipHash24_Init(&ctx, &sc->sc_ka_key);
	SipHash24_Update(&ctx, &gk->gk_uptime, sizeof(gk->gk_uptime));
	SipHash24_Update(&ctx, &gk->gk_random, sizeof(gk->gk_random));
	SipHash24_Final(digest, &ctx);

	if (memcmp(digest, gk->gk_digest, sizeof(digest)) != 0)
		goto drop;

	sc->sc_ka_recvtm = tick;

	switch (sc->sc_ka_state) {
	case GRE_KA_DOWN:
		sc->sc_ka_state = GRE_KA_HOLD;
		sc->sc_ka_holdcnt = sc->sc_ka_holdmax;
		sc->sc_ka_holdmax = MIN(sc->sc_ka_holdmax * 2,
		    16 * sc->sc_ka_count);
		break;
	case GRE_KA_HOLD:
		if (--sc->sc_ka_holdcnt > 0)
			break;

		sc->sc_ka_state = GRE_KA_UP;
		gre_link_state(sc);
		break;

	case GRE_KA_UP:
		sc->sc_ka_holdmax--;
		sc->sc_ka_holdmax = MAX(sc->sc_ka_holdmax, sc->sc_ka_count);
		break;
	}

	timeout_add_sec(&sc->sc_ka_hold, sc->sc_ka_timeo * sc->sc_ka_count);

drop:
	m_freem(m);
}

static int
gre_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
    struct rtentry *rt)
{
	struct m_tag *mtag;
	int error = 0;

	if (!gre_allow) {
		error = EACCES;
		goto drop;
	}

	if (!ISSET(ifp->if_flags, IFF_RUNNING)) {
		error = ENETDOWN;
		goto drop;
	}

	switch (dst->sa_family) {
	case AF_INET:
#ifdef INET6
	case AF_INET6:
#endif
#ifdef MPLS
	case AF_MPLS:
#endif
		break;
	default:
		error = EAFNOSUPPORT;
		goto drop;
	}

	/* Try to limit infinite recursion through misconfiguration. */
	for (mtag = m_tag_find(m, PACKET_TAG_GRE, NULL); mtag;
	     mtag = m_tag_find(m, PACKET_TAG_GRE, mtag)) {
		if (memcmp((caddr_t)(mtag + 1), &ifp->if_index,
		    sizeof(ifp->if_index)) == 0) {
			m_freem(m);
			error = EIO;
			goto end;
		}
	}

	mtag = m_tag_get(PACKET_TAG_GRE, sizeof(ifp->if_index), M_NOWAIT);
	if (mtag == NULL) {
		m_freem(m);
		error = ENOBUFS;
		goto end;
	}
	memcpy((caddr_t)(mtag + 1), &ifp->if_index, sizeof(ifp->if_index));
	m_tag_prepend(m, mtag);

	m->m_pkthdr.ph_family = dst->sa_family;

	error = if_enqueue(ifp, m);
end:
	if (error)
		ifp->if_oerrors++;
	return (error);

drop:
	m_freem(m);
	return (error);
}

void
gre_start(struct ifnet *ifp)
{
	struct gre_softc *sc = ifp->if_softc;
	struct mbuf *m;
	uint8_t ttl, tos;
	int tttl;
	uint16_t proto;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif
	int ttloff;

	tttl = sc->sc_tunnel.t_ttl;

	while ((m = ifq_dequeue(&ifp->if_snd)) != NULL) {
#if NBPFILTER > 0
		if_bpf = ifp->if_bpf;
		if (if_bpf) {
			int af = m->m_pkthdr.ph_family;
			bpf_mtap_af(if_bpf, af, m, BPF_DIRECTION_OUT);
		}
#endif

		m->m_flags &= ~(M_MCAST|M_BCAST);
		m->m_pkthdr.ph_ifidx = ifp->if_index;
		m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NPF > 0
		pf_pkt_addr_changed(m);
#endif

		switch (m->m_pkthdr.ph_family) {
		case AF_INET: {
			struct ip *ip;

			m = m_pullup(m, sizeof(*ip));
			if (m == NULL)
				continue;

			ip = mtod(m, struct ip *);
			tos = ip->ip_tos;

			ttloff = offsetof(struct ip, ip_ttl);
			proto = htons(ETHERTYPE_IP);
			break;
		}
#ifdef INET6
		case AF_INET6:
			tos = 0;
			ttloff = offsetof(struct ip6_hdr, ip6_hlim);
			proto = htons(ETHERTYPE_IPV6);
			break;
#endif
#ifdef MPLS
		case AF_MPLS:
			ttloff = 3;
			tos = 0;

			if (m->m_flags & (M_BCAST | M_MCAST))
				proto = htons(ETHERTYPE_MPLS_MCAST);
			else
				proto = htons(ETHERTYPE_MPLS);
			break;
#endif
		default:
			unhandled_af(m->m_pkthdr.ph_family);
		}

		if (tttl == -1) {
			m = m_pullup(m, ttloff + 1);
			if (m == NULL)
				continue;

			ttl = *(m->m_data + ttloff);
		} else
			ttl = tttl;

		m = gre_encap(&sc->sc_tunnel, m, proto);
		if (m == NULL ||
		    gre_ip_output(&sc->sc_tunnel, m, ttl, tos) != 0) {
			ifp->if_oerrors++;
			continue;
		}
	}
}

static void
egre_start(struct ifnet *ifp)
{
	struct egre_softc *sc = ifp->if_softc;
	struct mbuf *m0, *m;
#if NBPFILTER > 0
	caddr_t if_bpf;
#endif

	if (!gre_allow)
		ifq_purge(&ifp->if_snd);

	while ((m0 = ifq_dequeue(&ifp->if_snd)) != NULL) {
#if NBPFILTER > 0
		if_bpf = ifp->if_bpf;
		if (if_bpf)
			bpf_mtap_ether(if_bpf, m0, BPF_DIRECTION_OUT);
#endif

		m = m_gethdr(M_DONTWAIT, m0->m_type);
		if (m == NULL) {
			m_freem(m0);
			continue;
		}

		M_MOVE_PKTHDR(m, m0);
		m->m_next = m0;

		MH_ALIGN(m, 0);
		m->m_len = 0;

		m->m_flags &= ~(M_MCAST|M_BCAST);
		m->m_pkthdr.ph_ifidx = ifp->if_index;
		m->m_pkthdr.ph_rtableid = ifp->if_rdomain;

#if NPF > 0
		pf_pkt_addr_changed(m);
#endif

		m = gre_encap(&sc->sc_tunnel, m, htons(ETHERTYPE_TRANSETHER));
		if (m == NULL || gre_ip_output(&sc->sc_tunnel, m,
		    sc->sc_tunnel.t_ttl, 0) != 0) {
			ifp->if_oerrors++;
			continue;
		}
	}
}

static int
gre_ip_output(const struct gre_tunnel *tunnel, struct mbuf *m,
     uint8_t ttl, uint8_t tos)
{
	m = gre_ip_encap(tunnel, m, ttl, tos);
	if (m == NULL)
		return (-1);

	switch (tunnel->t_af) {
	case AF_INET:
		ip_send(m);
		break;
#ifdef INET6
	case AF_INET6:
		ip6_send(m);
		break;
#endif
	default:
		panic("%s: unsupported af %d in %p", __func__, tunnel->t_af,
		    tunnel);
	}

	return (0);
}

static struct mbuf *
gre_encap(struct gre_tunnel *tunnel, struct mbuf *m, uint16_t proto)
{
	struct gre_header *gh;
	struct gre_h_key *gkh;
	int hlen;

	hlen = sizeof(*gh);
	if (tunnel->t_key_mask != GRE_KEY_NONE)
		hlen += sizeof(*gkh);

	m = m_prepend(m, hlen, M_DONTWAIT);
	if (m == NULL)
		return (NULL);

	gh = mtod(m, struct gre_header *);
	gh->gre_flags = GRE_VERS_0;
	gh->gre_proto = proto;
	if (tunnel->t_key_mask != GRE_KEY_NONE) {
		gh->gre_flags |= htons(GRE_KP);

		gkh = (struct gre_h_key *)(gh + 1);
		gkh->gre_key = tunnel->t_key;
	}

	return (m);
}

static struct mbuf *
gre_ip_encap(const struct gre_tunnel *tunnel, struct mbuf *m,
    uint8_t ttl, uint8_t tos)
{
	m->m_flags &= ~(M_BCAST|M_MCAST);
	m->m_pkthdr.ph_rtableid = tunnel->t_rtableid;

#if NPF > 0
	pf_pkt_addr_changed(m);
#endif

	switch (tunnel->t_af) {
	case AF_INET: {
		struct ip *ip;

		m = m_prepend(m, sizeof(*ip), M_DONTWAIT);
		if (m == NULL)
			return (NULL);

		ip = mtod(m, struct ip *);
		ip->ip_off = 0; /* DF ? */
		ip->ip_tos = tos;
		ip->ip_len = htons(m->m_pkthdr.len);
		ip->ip_ttl = ttl;
		ip->ip_p = IPPROTO_GRE;
		ip->ip_src.s_addr = tunnel->t_src[0];
		ip->ip_dst.s_addr = tunnel->t_dst[0];
		break;
	}
#ifdef INET6
	case AF_INET6: {
		struct ip6_hdr *ip6;
		int len = m->m_pkthdr.len;

		m = m_prepend(m, sizeof(*ip6), M_DONTWAIT);
		if (m == NULL)
			return (NULL);

		ip6 = mtod(m, struct ip6_hdr *);
		ip6->ip6_flow = ISSET(m->m_pkthdr.ph_flowid, M_FLOWID_VALID) ?
		    htonl(m->m_pkthdr.ph_flowid & M_FLOWID_MASK) : 0;
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_plen = htons(len);
		ip6->ip6_nxt = IPPROTO_GRE;
		ip6->ip6_hlim = ttl;
		memcpy(&ip6->ip6_src, tunnel->t_src, sizeof(ip6->ip6_src));
		memcpy(&ip6->ip6_dst, tunnel->t_dst, sizeof(ip6->ip6_dst));
		break;
	}
#endif /* INET6 */
	default:
		panic("%s: unsupported af %d in %p", __func__, tunnel->t_af,
		    tunnel);
	}

	return (m);
}

static int
gre_tunnel_ioctl(struct ifnet *ifp, struct gre_tunnel *tunnel,
    u_long cmd, void *data)
{
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch(cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < 576) {
			error = EINVAL;
			break;
		}
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		break;

	case SIOCSIFRDOMAIN:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}
		break;

	case SIOCSVNETID:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}
		error = gre_set_vnetid(tunnel, ifr);
		break;

	case SIOCGVNETID:
		error = gre_get_vnetid(tunnel, ifr);
		break;
	case SIOCDVNETID:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}
		error = gre_del_vnetid(tunnel);
		break;

	case SIOCSLIFPHYADDR:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}
		error = gre_set_tunnel(tunnel, (struct if_laddrreq *)data);
		break;
	case SIOCGLIFPHYADDR:
		error = gre_get_tunnel(tunnel, (struct if_laddrreq *)data);
		break;
	case SIOCDIFPHYADDR:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}

		error = gre_del_tunnel(tunnel);
		break;

	case SIOCSLIFPHYRTABLE:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}

		if (ifr->ifr_rdomainid < 0 ||
		    ifr->ifr_rdomainid > RT_TABLEID_MAX ||
		    !rtable_exists(ifr->ifr_rdomainid)) {
			error = EINVAL;
			break;
		}
		tunnel->t_rtableid = ifr->ifr_rdomainid;
		break;
	case SIOCGLIFPHYRTABLE:
		ifr->ifr_rdomainid = tunnel->t_rtableid;
		break;

	default:
		error = ENOTTY;
		break;
	}

	return (error);
}

static int
gre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct gre_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifkalivereq *ikar = (struct ifkalivereq *)data;
	int error = 0;

	switch(cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = gre_up(sc);
			else
				error = 0;
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = gre_down(sc);
		}
		break;
	case SIOCSIFRDOMAIN:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}
		break;

	case SIOCSETKALIVE:
		if (ISSET(ifp->if_flags, IFF_RUNNING)) {
			error = EBUSY;
			break;
		}

		if (ikar->ikar_timeo < 0 || ikar->ikar_timeo > 86400 ||
		    ikar->ikar_cnt < 0 || ikar->ikar_cnt > 256)
			return (EINVAL);

		if (ikar->ikar_timeo == 0 || ikar->ikar_cnt == 0) {
			sc->sc_ka_count = 0;
			sc->sc_ka_timeo = 0;
			sc->sc_ka_state = GRE_KA_NONE;
		} else {
			sc->sc_ka_count = ikar->ikar_cnt;
			sc->sc_ka_timeo = ikar->ikar_timeo;
			sc->sc_ka_state = GRE_KA_DOWN;
		}
		break;

	case SIOCGETKALIVE:
		ikar->ikar_cnt = sc->sc_ka_count;
		ikar->ikar_timeo = sc->sc_ka_timeo;
		break;

	case SIOCSLIFPHYTTL:
		if (ifr->ifr_ttl != -1 &&
		    (ifr->ifr_ttl < 1 || ifr->ifr_ttl > 0xff)) {
			error = EINVAL;
			break;
		}

		/* commit */
		sc->sc_tunnel.t_ttl = ifr->ifr_ttl;
		break;

	case SIOCGLIFPHYTTL:
		ifr->ifr_ttl = sc->sc_tunnel.t_ttl;
		break;

	default:
		error = gre_tunnel_ioctl(ifp, &sc->sc_tunnel, cmd, data);
		break;
	}

	return (error);
}

static int
egre_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct egre_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	int error = 0;

	switch(cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ISSET(ifp->if_flags, IFF_UP)) {
			if (!ISSET(ifp->if_flags, IFF_RUNNING))
				error = egre_up(sc);
			else
				error = 0;
		} else {
			if (ISSET(ifp->if_flags, IFF_RUNNING))
				error = egre_down(sc);
		}
		break;

	case SIOCSLIFPHYTTL:
		if (ifr->ifr_ttl < 1 || ifr->ifr_ttl > 0xff) {
			error = EINVAL;
			break;
		}

		/* commit */
		sc->sc_tunnel.t_ttl = (uint8_t)ifr->ifr_ttl;
		break;

	case SIOCGLIFPHYTTL:
		ifr->ifr_ttl = (int)sc->sc_tunnel.t_ttl;
		break;

	default:
		error = gre_tunnel_ioctl(ifp, &sc->sc_tunnel, cmd, data);
		if (error == ENOTTY)
			error = ether_ioctl(ifp, &sc->sc_ac, cmd, data);
		break;
	}

	return (error);
}

static int
gre_up(struct gre_softc *sc)
{
	if (sc->sc_tunnel.t_af == AF_UNSPEC)
		return (ENXIO);

	NET_ASSERT_LOCKED(); 
	if (sc->sc_ka_state != GRE_KA_NONE &&
	    sc->sc_tunnel.t_rtableid != sc->sc_if.if_rdomain)
		return (EHOSTUNREACH);

	if (RBT_INSERT(gre_tree, &gre_softcs, &sc->sc_tunnel) != NULL)
		return (EADDRINUSE);

	/* we're up and running now */

	SET(sc->sc_if.if_flags, IFF_RUNNING);

	if (sc->sc_ka_state != GRE_KA_NONE) {
		arc4random_buf(&sc->sc_ka_key, sizeof(sc->sc_ka_key));
		sc->sc_ka_bias = arc4random();

		sc->sc_ka_recvtm = ticks - hz;
		sc->sc_ka_holdmax = sc->sc_ka_count;

		gre_keepalive_send(sc);
	}

	return (0);
}

static int
gre_down(struct gre_softc *sc)
{
	NET_ASSERT_LOCKED();
	RBT_REMOVE(gre_tree, &gre_softcs, &sc->sc_tunnel);

	CLR(sc->sc_if.if_flags, IFF_RUNNING);

	if (sc->sc_ka_state != GRE_KA_NONE) {
		if (!timeout_del(&sc->sc_ka_hold))
			timeout_barrier(&sc->sc_ka_hold);
		if (!timeout_del(&sc->sc_ka_send))
			timeout_barrier(&sc->sc_ka_send);

		sc->sc_ka_state = GRE_KA_DOWN;

		gre_link_state(sc);
	}

	return (0);
}

static void
gre_link_state(struct gre_softc *sc)
{
	struct ifnet *ifp = &sc->sc_if;
	int link_state = LINK_STATE_UNKNOWN;

	if (ISSET(ifp->if_flags, IFF_RUNNING)) {
		switch (sc->sc_ka_state) {
		case GRE_KA_NONE:
			/* maybe up? or down? it's unknown, really */
			break;
		case GRE_KA_UP:
			link_state = LINK_STATE_UP;
			break;
		default:
			link_state = LINK_STATE_KALIVE_DOWN;
			break;
		}
	}

	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}

static void
gre_keepalive_send(void *arg)
{
	struct gre_tunnel t;
	struct gre_softc *sc = arg;
	struct mbuf *m;
	struct gre_keepalive *gk;
	SIPHASH_CTX ctx;
	int linkhdr, len;
	uint16_t proto;
	uint8_t ttl;

	if (!ISSET(sc->sc_if.if_flags, IFF_RUNNING) ||
	    sc->sc_ka_state == GRE_KA_NONE)
		return;

	/* this is really conservative */
	linkhdr = max_linkhdr + MAX(sizeof(struct ip), sizeof(struct ip6_hdr)) +
	    sizeof(struct gre_header) + sizeof(struct gre_h_key);
	len = linkhdr + sizeof(*gk);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return;

	if (len > MHLEN) {
		MCLGETI(m, M_DONTWAIT, NULL, len);
		if (!ISSET(m->m_flags, M_EXT)) {
			m_freem(m);
			return;
		}
	}

	m->m_pkthdr.len = m->m_len = len;
	m_adj(m, linkhdr);

	/*
	 * build the inside packet
	 */
	gk = mtod(m, struct gre_keepalive *);
	htobem32(&gk->gk_uptime, sc->sc_ka_bias + ticks);
	htobem32(&gk->gk_random, arc4random());

	SipHash24_Init(&ctx, &sc->sc_ka_key);
	SipHash24_Update(&ctx, &gk->gk_uptime, sizeof(gk->gk_uptime));
	SipHash24_Update(&ctx, &gk->gk_random, sizeof(gk->gk_random));
	SipHash24_Final(gk->gk_digest, &ctx);

	m = gre_encap(&sc->sc_tunnel, m, htons(0));
	if (m == NULL)
		return;

	ttl = sc->sc_tunnel.t_ttl == -1 ? ip_defttl : sc->sc_tunnel.t_ttl;

	t.t_af = sc->sc_tunnel.t_af;
	memcpy(t.t_src, sc->sc_tunnel.t_dst, sizeof(t.t_src));
	memcpy(t.t_dst, sc->sc_tunnel.t_src, sizeof(t.t_dst));
	m = gre_ip_encap(&t, m, ttl, 0);
	if (m == NULL)
		return;

	switch (sc->sc_tunnel.t_af) {
	case AF_INET: {
		struct ip *ip;

		ip = mtod(m, struct ip *);
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_off &= htons(IP_DF);
		ip->ip_id = htons(ip_randomid());
		ip->ip_sum = 0;
		ip->ip_sum = in_cksum(m, sizeof(*ip));

		proto = htons(ETHERTYPE_IP);
		break;
	}
#ifdef INET6
	case AF_INET6:
		proto = htons(ETHERTYPE_IPV6);
		break;
#endif
	}


	/*
	 * put it in the tunnel
	 */
	m = gre_encap(&sc->sc_tunnel, m, proto);
	if (m == NULL)
		return;

	m->m_pkthdr.ph_ifidx = sc->sc_if.if_index;
	m->m_pkthdr.ph_rtableid = sc->sc_if.if_rdomain;

	gre_ip_output(&sc->sc_tunnel, m, ttl, 0);

	timeout_add_sec(&sc->sc_ka_send, sc->sc_ka_timeo);
}

static void
gre_keepalive_hold(void *arg)
{
	struct gre_softc *sc = arg;

	if (!ISSET(sc->sc_if.if_flags, IFF_RUNNING) ||
	    sc->sc_ka_state == GRE_KA_NONE)
		return;

	NET_LOCK();
	sc->sc_ka_state = GRE_KA_DOWN;
	gre_link_state(sc);
	NET_UNLOCK();
}

static int
gre_set_tunnel(struct gre_tunnel *tunnel, struct if_laddrreq *req)
{
	struct sockaddr *src = (struct sockaddr *)&req->addr;
	struct sockaddr *dst = (struct sockaddr *)&req->dstaddr;
	struct sockaddr_in *src4, *dst4;
#ifdef INET6
	struct sockaddr_in6 *src6, *dst6;
	int error;
#endif

	/* sa_family and sa_len must be equal */
	if (src->sa_family != dst->sa_family || src->sa_len != dst->sa_len)
		return (EINVAL);

	/* validate */
	switch (dst->sa_family) {
	case AF_INET:
		if (dst->sa_len != sizeof(*dst4))
			return (EINVAL);

		src4 = (struct sockaddr_in *)src;
		if (in_nullhost(src4->sin_addr) ||
		    IN_MULTICAST(src4->sin_addr.s_addr))
			return (EINVAL);

		dst4 = (struct sockaddr_in *)dst;
		if (in_nullhost(dst4->sin_addr) ||
		    IN_MULTICAST(dst4->sin_addr.s_addr))
			return (EINVAL);

		tunnel->t_src[0] = src4->sin_addr.s_addr;
		tunnel->t_dst[0] = dst4->sin_addr.s_addr;

		break;
#ifdef INET6
	case AF_INET6:
		if (dst->sa_len != sizeof(*dst6))
			return (EINVAL);

		src6 = (struct sockaddr_in6 *)src;
		if (IN6_IS_ADDR_UNSPECIFIED(&src6->sin6_addr) ||
		    IN6_IS_ADDR_MULTICAST(&src6->sin6_addr))
			return (EINVAL);

		dst6 = (struct sockaddr_in6 *)dst;
		if (IN6_IS_ADDR_UNSPECIFIED(&dst6->sin6_addr) ||
		    IN6_IS_ADDR_MULTICAST(&dst6->sin6_addr))
			return (EINVAL);

		error = in6_embedscope((struct in6_addr *)tunnel->t_src,
		    src6, NULL);
		if (error != 0)
			return (error);

		error = in6_embedscope((struct in6_addr *)tunnel->t_dst,
		    dst6, NULL);
		if (error != 0)
			return (error);

		break;
#endif
	default:
		return (EAFNOSUPPORT);
	}

	/* commit */
	tunnel->t_af = dst->sa_family;

	return (0);
}

static int
gre_get_tunnel(struct gre_tunnel *tunnel, struct if_laddrreq *req)
{
	struct sockaddr *src = (struct sockaddr *)&req->addr;
	struct sockaddr *dst = (struct sockaddr *)&req->dstaddr;
	struct sockaddr_in *sin;
#ifdef INET6 /* ifconfig already embeds the scopeid */
	struct sockaddr_in6 *sin6;
#endif

	switch (tunnel->t_af) {
	case AF_UNSPEC:
		return (EADDRNOTAVAIL);
	case AF_INET:
		sin = (struct sockaddr_in *)src;
		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr.s_addr = tunnel->t_src[0];

		sin = (struct sockaddr_in *)dst;
		memset(sin, 0, sizeof(*sin));
		sin->sin_family = AF_INET;
		sin->sin_len = sizeof(*sin);
		sin->sin_addr.s_addr = tunnel->t_dst[0];

		break;

#ifdef INET6
	case AF_INET6:
		sin6 = (struct sockaddr_in6 *)src;
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		in6_recoverscope(sin6, (struct in6_addr *)tunnel->t_src);

		sin6 = (struct sockaddr_in6 *)dst;
		memset(sin6, 0, sizeof(*sin6));
		sin6->sin6_family = AF_INET6;
		sin6->sin6_len = sizeof(*sin6);
		in6_recoverscope(sin6, (struct in6_addr *)tunnel->t_dst);

		break;
#endif
	default:
		return (EAFNOSUPPORT);
	}

	return (0);
}

static int
gre_del_tunnel(struct gre_tunnel *tunnel)
{
	/* commit */
	tunnel->t_af = AF_UNSPEC;

	return (0);
}

static int
gre_set_vnetid(struct gre_tunnel *tunnel, struct ifreq *ifr)
{
	uint32_t key;

	if (ifr->ifr_vnetid < 0 || ifr->ifr_vnetid > 0xffffffff)
		return EINVAL;

	key = htonl(ifr->ifr_vnetid);

	if (tunnel->t_key_mask == GRE_KEY_MASK && tunnel->t_key == key)
		return (0);

	/* commit */
	tunnel->t_key_mask = GRE_KEY_MASK;
	tunnel->t_key = key;

	return (0);
}

static int
gre_get_vnetid(struct gre_tunnel *tunnel, struct ifreq *ifr)
{
	if (tunnel->t_key_mask == GRE_KEY_NONE)
		return (EADDRNOTAVAIL);

	ifr->ifr_vnetid = (int64_t)ntohl(tunnel->t_key);

	return (0);
}

static int
gre_del_vnetid(struct gre_tunnel *tunnel)
{
	tunnel->t_key_mask = GRE_KEY_NONE;

	return (0);
}

static int
egre_up(struct egre_softc *sc)
{
	if (sc->sc_tunnel.t_af == AF_UNSPEC)
		return (ENXIO);

	NET_ASSERT_LOCKED(); 
	if (RBT_INSERT(gre_tree, &egre_softcs, &sc->sc_tunnel) != NULL)
		return (EADDRINUSE);

	SET(sc->sc_ac.ac_if.if_flags, IFF_RUNNING);

	return (0);
}

static int
egre_down(struct egre_softc *sc)
{
	NET_ASSERT_LOCKED();

	RBT_REMOVE(gre_tree, &egre_softcs, &sc->sc_tunnel);

	CLR(sc->sc_ac.ac_if.if_flags, IFF_RUNNING);

	/* barrier? */

	return (0);
}

static int
egre_media_change(struct ifnet *ifp)
{
	return (ENOTTY);
}

static void
egre_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID | IFM_ACTIVE;
}

int
gre_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	int error;

	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case GRECTL_ALLOW:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen, &gre_allow);
		NET_UNLOCK();
		return (error);
	case GRECTL_WCCP:
		NET_LOCK();
		error = sysctl_int(oldp, oldlenp, newp, newlen, &gre_wccp);
		NET_UNLOCK();
		return (error);
	default:
		return (ENOPROTOOPT);
	}
	/* NOTREACHED */
}

static inline int
gre_ip_cmp(int af, const uint32_t *a, const uint32_t *b)
{
	switch (af) {
#ifdef INET6
	case AF_INET6:
		if (a[3] > b[3])
			return (1);
		if (a[3] < b[3])
			return (-1);

		if (a[2] > b[2])
			return (1);
		if (a[2] < b[2])
			return (-1);

		if (a[1] > b[1])
			return (1);
		if (a[1] < b[1])
			return (-1);

		/* FALLTHROUGH */
#endif /* INET6 */
	case AF_INET:
		if (a[0] > b[0])
			return (1);
		if (a[0] < b[0])
			return (-1);
		break;
	default:
		panic("%s: unsupported af %d\n", __func__, af);
	}

	return (0);
}

static inline int
gre_cmp(const struct gre_tunnel *a, const struct gre_tunnel *b)
{
	uint32_t ka, kb;
	uint32_t mask;
	int rv;

	/* sort by routing table */
	if (a->t_rtableid > b->t_rtableid)
		return (1);
	if (a->t_rtableid < b->t_rtableid)
		return (-1);

	/* sort by address */
	if (a->t_af > b->t_af)
		return (1);
	if (a->t_af < b->t_af)
		return (-1);

	rv = gre_ip_cmp(a->t_af, a->t_dst, b->t_dst);
	if (rv != 0)
		return (rv);

	rv = gre_ip_cmp(a->t_af, a->t_src, b->t_src);
	if (rv != 0)
		return (rv);

	/* is K set at all? */
	ka = a->t_key_mask & GRE_KEY_ENTROPY;
	kb = b->t_key_mask & GRE_KEY_ENTROPY;

	/* sort by whether K is set */
	if (ka > kb)
		return (1);
	if (ka < kb)
		return (-1);

	/* is K set on both? */
	if (ka != GRE_KEY_NONE) {
		/* get common prefix */
		mask = a->t_key_mask & b->t_key_mask;

		ka = a->t_key & mask;
		kb = b->t_key & mask;

		/* sort by common prefix */
		if (ka > kb)
			return (1);
		if (ka < kb)
			return (-1);
	}

	return (0);
}

RBT_GENERATE(gre_tree, gre_tunnel, t_entry, gre_cmp);

