/*	$OpenBSD: ip_carp.c,v 1.54 2004/05/25 02:32:07 jolan Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff. All rights reserved.
 * Copyright (c) 2003 Ryan McBride. All rights reserved.
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
 * TODO:
 *	- iface reconfigure
 *	- track iface ip address changes;
 *	- support for hardware checksum calculations;
 *
 */

#include "ether.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <machine/cpu.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_llc.h>
#include <net/route.h>
#include <net/netisr.h>

#if NFDDI > 0
#include <net/if_fddi.h>
#endif
#if NTOKEN > 0
#include <net/if_token.h>
#endif

#include <crypto/sha1.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_ipsp.h>

#include <net/if_enc.h>
#endif

#ifdef INET6
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <net/if_dl.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/ip_carp.h>

struct carp_softc {
	struct arpcom sc_ac;
	int if_flags;			/* current flags to treat UP/DOWN */
	struct ifnet *sc_ifp;
	struct in_ifaddr *sc_ia;	/* primary iface address */
	struct ip_moptions sc_imo;
#ifdef INET6
	struct in6_ifaddr *sc_ia6;	/* primary iface address v6 */
	struct ip6_moptions sc_im6o;
#endif /* INET6 */
	TAILQ_ENTRY(carp_softc) sc_list;

	enum { INIT = 0, BACKUP, MASTER }	sc_state;

	int sc_flags_backup;
	int sc_suppress;

	int sc_sendad_errors;
#define CARP_SENDAD_MAX_ERRORS	3
	int sc_sendad_success;
#define CARP_SENDAD_MIN_SUCCESS 3

	int sc_vhid;
	int sc_advskew;
	int sc_naddrs;
	int sc_naddrs6;
	int sc_advbase;		/* seconds */
	int sc_init_counter;
	u_int64_t sc_counter;

	/* authentication */
#define CARP_HMAC_PAD	64
	unsigned char sc_key[CARP_KEY_LEN];
	unsigned char sc_pad[CARP_HMAC_PAD];
	SHA1_CTX sc_sha1;

	struct timeout sc_ad_tmo;	/* advertisement timeout */
	struct timeout sc_md_tmo;	/* master down timeout */
	struct timeout sc_md6_tmo;	/* master down timeout */

};

int carp_suppress_preempt = 0;
int carp_opts[CARPCTL_MAXID] = { 0, 1, 0, 0, 0 };	/* XXX for now */
struct carpstats carpstats;

struct carp_if {
	TAILQ_HEAD(, carp_softc) vhif_vrs;
	int vhif_nvrs;

	struct ifnet *vhif_ifp;
};

#define	CARP_LOG(s,a) if (carp_opts[CARPCTL_LOG])			\
	log(LOG_INFO, "carp: " s "\n", (a));
#define	CARP_LOG1(sc,s,a) if (carp_opts[CARPCTL_LOG])			\
	log(LOG_INFO, "%s: " s "\n", (sc)->sc_ac.ac_if.if_xname, (a));

void	carp_hmac_prepare(struct carp_softc *);
void	carp_hmac_generate(struct carp_softc *, u_int32_t *,
	    unsigned char *);
int	carp_hmac_verify(struct carp_softc *, u_int32_t *,
	    unsigned char *);
void	carp_setroute(struct carp_softc *, int);
void	carp_input_c(struct mbuf *, struct carp_header *, sa_family_t);
void	carpattach(int);
void	carpdetach(struct carp_softc *);
int	carp_prepare_ad(struct mbuf *, struct carp_softc *,
	    struct carp_header *);
void	carp_send_ad_all(void);
void	carp_send_ad(void *);
void	carp_send_arp(struct carp_softc *);
void	carp_master_down(void *);
int	carp_ioctl(struct ifnet *, u_long, caddr_t);
void	carp_start(struct ifnet *);
void	carp_setrun(struct carp_softc *, sa_family_t);
void	carp_set_state(struct carp_softc *, int);
int	carp_addrcount(struct carp_if *, struct in_ifaddr *, int);
enum	{ CARP_COUNT_MASTER, CARP_COUNT_RUNNING };

int	carp_set_addr(struct carp_softc *, struct sockaddr_in *);
int	carp_del_addr(struct carp_softc *, struct sockaddr_in *);
#ifdef INET6
void	carp_send_na(struct carp_softc *);
int	carp_set_addr6(struct carp_softc *, struct sockaddr_in6 *);
int	carp_del_addr6(struct carp_softc *, struct sockaddr_in6 *);
#endif
int     carp_clone_create(struct if_clone *, int);
int     carp_clone_destroy(struct ifnet *);

struct if_clone carp_cloner =
    IF_CLONE_INITIALIZER("carp", carp_clone_create, carp_clone_destroy);

static __inline u_int16_t
carp_cksum(struct mbuf *m, int len)
{
	return (in_cksum(m, len));
}

void
carp_hmac_prepare(struct carp_softc *sc)
{
	u_int8_t version = CARP_VERSION, type = CARP_ADVERTISEMENT;
	u_int8_t vhid = sc->sc_vhid & 0xff;
	struct ifaddr *ifa;
	int i;
#ifdef INET6
	struct in6_addr in6;
#endif /* INET6 */

	/* compute ipad from key */
	bzero(sc->sc_pad, sizeof(sc->sc_pad));
	bcopy(sc->sc_key, sc->sc_pad, sizeof(sc->sc_key));
	for (i = 0; i < sizeof(sc->sc_pad); i++)
		sc->sc_pad[i] ^= 0x36;

	/* precompute first part of inner hash */
	SHA1Init(&sc->sc_sha1);
	SHA1Update(&sc->sc_sha1, sc->sc_pad, sizeof(sc->sc_pad));
	SHA1Update(&sc->sc_sha1, (void *)&version, sizeof(version));
	SHA1Update(&sc->sc_sha1, (void *)&type, sizeof(type));
	SHA1Update(&sc->sc_sha1, (void *)&vhid, sizeof(vhid));
#ifdef INET
	TAILQ_FOREACH(ifa, &sc->sc_ac.ac_if.if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET)
			SHA1Update(&sc->sc_sha1,
			    (void *)&ifatoia(ifa)->ia_addr.sin_addr.s_addr,
			    sizeof(struct in_addr));
	}
#endif /* INET */
#ifdef INET6
	TAILQ_FOREACH(ifa, &sc->sc_ac.ac_if.if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			in6 = ifatoia6(ifa)->ia_addr.sin6_addr;
			if (IN6_IS_ADDR_LINKLOCAL(&in6))
				in6.s6_addr16[1] = 0;
			SHA1Update(&sc->sc_sha1, (void *)&in6, sizeof(in6));
		}
	}
#endif /* INET6 */

	/* convert ipad to opad */
	for (i = 0; i < sizeof(sc->sc_pad); i++)
		sc->sc_pad[i] ^= 0x36 ^ 0x5c;
}

void
carp_hmac_generate(struct carp_softc *sc, u_int32_t counter[2],
    unsigned char md[20])
{
	SHA1_CTX sha1ctx;

	/* fetch first half of inner hash */
	bcopy(&sc->sc_sha1, &sha1ctx, sizeof(sha1ctx));

	SHA1Update(&sha1ctx, (void *)counter, sizeof(sc->sc_counter));
	SHA1Final(md, &sha1ctx);

	/* outer hash */
	SHA1Init(&sha1ctx);
	SHA1Update(&sha1ctx, sc->sc_pad, sizeof(sc->sc_pad));
	SHA1Update(&sha1ctx, md, 20);
	SHA1Final(md, &sha1ctx);
}

int
carp_hmac_verify(struct carp_softc *sc, u_int32_t counter[2],
    unsigned char md[20])
{
	unsigned char md2[20];

	carp_hmac_generate(sc, counter, md2);

	return (bcmp(md, md2, sizeof(md2)));
}

void
carp_setroute(struct carp_softc *sc, int cmd)
{
	struct ifaddr *ifa;
	int s;

	s = splnet();
	TAILQ_FOREACH(ifa, &sc->sc_ac.ac_if.if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET && sc->sc_ifp != NULL) {
			int count = carp_addrcount(
			    (struct carp_if *)sc->sc_ifp->if_carp,
			    ifatoia(ifa), CARP_COUNT_MASTER);

			if ((cmd == RTM_ADD && count == 1) ||
			    (cmd == RTM_DELETE && count == 0))
				rtinit(ifa, cmd, RTF_UP | RTF_HOST);
		}
#ifdef INET6
		if (ifa->ifa_addr->sa_family == AF_INET6) {
			if (cmd == RTM_ADD)
				in6_ifaddloop(ifa);
			else
				in6_ifremloop(ifa);
		}
#endif /* INET6 */
	}
	splx(s);
}

/*
 * process input packet.
 * we have rearranged checks order compared to the rfc,
 * but it seems more efficient this way or not possible otherwise.
 */
void
carp_input(struct mbuf *m, ...)
{
	struct ip *ip = mtod(m, struct ip *);
	struct carp_header *ch;
	int iplen, len, hlen;
	va_list ap;

	va_start(ap, m);
	hlen = va_arg(ap, int);
	va_end(ap);

	carpstats.carps_ipackets++;

	if (!carp_opts[CARPCTL_ALLOW]) {
		m_freem(m);
		return;
	}

	/* check if received on a valid carp interface */
	if (m->m_pkthdr.rcvif->if_carp == NULL) {
		carpstats.carps_badif++;
		CARP_LOG("packet received on non-carp interface: %s",
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return;
	}

	/* verify that the IP TTL is 255.  */
	if (ip->ip_ttl != CARP_DFLTTL) {
		carpstats.carps_badttl++;
		CARP_LOG("received ttl %d != 255", ip->ip_ttl);
		m_freem(m);
		return;
	}

	iplen = ip->ip_hl << 2;

	if (m->m_pkthdr.len < iplen + sizeof(*ch)) {
		carpstats.carps_badlen++;
		CARP_LOG("received len %d < 36",
		    m->m_len - sizeof(struct ip));
		m_freem(m);
		return;
	}

	if (iplen + sizeof(*ch) < m->m_len) {
		if ((m = m_pullup2(m, iplen + sizeof(*ch))) == NULL) {
			carpstats.carps_hdrops++;
			/* CARP_LOG ? */
			return;
		}
		ip = mtod(m, struct ip *);
	}
	ch = (void *)ip + iplen;

	/*
	 * verify that the received packet length is
	 * equal to the CARP header
	 */
	len = iplen + sizeof(*ch);
	if (len > m->m_pkthdr.len) {
		carpstats.carps_badlen++;
		CARP_LOG("packet too short %d", m->m_pkthdr.len);
		m_freem(m);
		return;
	}

	if ((m = m_pullup2(m, len)) == NULL) {
		carpstats.carps_hdrops++;
		return;
	}
	ip = mtod(m, struct ip *);
	ch = (void *)ip + iplen;

	/* verify the CARP checksum */
	m->m_data += iplen;
	if (carp_cksum(m, len - iplen)) {
		carpstats.carps_badsum++;
		CARP_LOG("checksum failed", 0);
		m_freem(m);
		return;
	}
	m->m_data -= iplen;

	carp_input_c(m, ch, AF_INET);
}

#ifdef INET6
int
carp6_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct carp_header *ch;
	u_int len;

	carpstats.carps_ipackets6++;

	if (!carp_opts[CARPCTL_ALLOW]) {
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* check if received on a valid carp interface */
	if (m->m_pkthdr.rcvif->if_carp == NULL) {
		carpstats.carps_badif++;
		CARP_LOG("packet received on non-carp interface: %s",
		    m->m_pkthdr.rcvif->if_xname);
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* verify that the IP TTL is 255 */
	if (ip6->ip6_hlim != CARP_DFLTTL) {
		carpstats.carps_badttl++;
		CARP_LOG("received ttl %d != 255", ip6->ip6_hlim);
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* verify that we have a complete carp packet */
	len = m->m_len;
	IP6_EXTHDR_GET(ch, struct carp_header *, m, *offp, sizeof(*ch));
	if (ch == NULL) {
		carpstats.carps_badlen++;
		CARP_LOG("packet size %u too small", len);
		return (IPPROTO_DONE);
	}


	/* verify the CARP checksum */
	m->m_data += *offp;
	if (carp_cksum(m, sizeof(*ch))) {
		carpstats.carps_badsum++;
		CARP_LOG("checksum failed", 0);
		m_freem(m);
		return (IPPROTO_DONE);
	}
	m->m_data -= *offp;

	carp_input_c(m, ch, AF_INET6);
	return (IPPROTO_DONE);
}
#endif /* INET6 */

void
carp_input_c(struct mbuf *m, struct carp_header *ch, sa_family_t af)
{
	struct carp_softc *sc;
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	u_int64_t tmp_counter;
	struct timeval sc_tv, ch_tv;

	/* verify that the VHID is valid on the receiving interface */
	TAILQ_FOREACH(sc, &((struct carp_if *)ifp->if_carp)->vhif_vrs, sc_list)
		if (sc->sc_vhid == ch->carp_vhid)
			break;
	if (!sc || (sc->sc_ac.ac_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
	    (IFF_UP|IFF_RUNNING)) {
		carpstats.carps_badvhid++;
		m_freem(m);
		return;
	}

	sc->sc_ac.ac_if.if_lastchange = time;
	sc->sc_ac.ac_if.if_ipackets++;
	sc->sc_ac.ac_if.if_ibytes += m->m_pkthdr.len;

#if NBPFILTER > 0
	if (sc->sc_ac.ac_if.if_bpf) {
		/*
		 * We need to prepend the address family as
		 * a four byte field.  Cons up a dummy header
		 * to pacify bpf.  This is safe because bpf
		 * will only read from the mbuf (i.e., it won't
		 * try to free it or keep a pointer to it).
		 */
		struct mbuf m0;
		u_int32_t af = htonl(af);

		m0.m_next = m;
		m0.m_len = sizeof(af);
		m0.m_data = (char *)&af;
		bpf_mtap(sc->sc_ac.ac_if.if_bpf, &m0);
	}
#endif

	/* verify the CARP version. */
	if (ch->carp_version != CARP_VERSION) {
		carpstats.carps_badver++;
		sc->sc_ac.ac_if.if_ierrors++;
		CARP_LOG1(sc, "invalid version %d", ch->carp_version);
		m_freem(m);
		return;
	}

	/* verify the hash */
	if (carp_hmac_verify(sc, ch->carp_counter, ch->carp_md)) {
		carpstats.carps_badauth++;
		sc->sc_ac.ac_if.if_ierrors++;
		CARP_LOG("incorrect hash", 0);
		m_freem(m);
		return;
	}

	tmp_counter = ntohl(ch->carp_counter[0]);
	tmp_counter = tmp_counter<<32;
	tmp_counter += ntohl(ch->carp_counter[1]);

	/* XXX Replay protection goes here */

	sc->sc_init_counter = 0;
	sc->sc_counter = tmp_counter;


	sc_tv.tv_sec = sc->sc_advbase;
	if (carp_suppress_preempt && sc->sc_advskew <  240)
		sc_tv.tv_usec = 240 * 1000000 / 256;
	else
		sc_tv.tv_usec = sc->sc_advskew * 1000000 / 256;
	ch_tv.tv_sec = ch->carp_advbase;
	ch_tv.tv_usec = ch->carp_advskew * 1000000 / 256;

	switch (sc->sc_state) {
	case INIT:
		break;
	case MASTER:
		/*
		 * If we receive an advertisement from a master who's going to
		 * be more frequent than us, go into BACKUP state.
		 */
		if (timercmp(&sc_tv, &ch_tv, >) ||
		    timercmp(&sc_tv, &ch_tv, ==)) {
			timeout_del(&sc->sc_ad_tmo);
			carp_set_state(sc, BACKUP);
			carp_setrun(sc, 0);
			carp_setroute(sc, RTM_DELETE);
		}
		break;
	case BACKUP:
		/*
		 * If we're pre-empting masters who advertise slower than us,
		 * and this one claims to be slower, treat him as down.
		 */
		if (carp_opts[CARPCTL_PREEMPT] && timercmp(&sc_tv, &ch_tv, <)) {
			carp_master_down(sc);
			break;
		}

		/*
		 *  If the master is going to advertise at such a low frequency
		 *  that he's guaranteed to time out, we'd might as well just
		 *  treat him as timed out now.
		 */
		sc_tv.tv_sec = sc->sc_advbase * 3;
		if (timercmp(&sc_tv, &ch_tv, <)) {
			carp_master_down(sc);
			break;
		}

		/*
		 * Otherwise, we reset the counter and wait for the next
		 * advertisement.
		 */
		carp_setrun(sc, af);
		break;
	}

	m_freem(m);
	return;
}

int
carp_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp, void *newp,
    size_t newlen)
{
	/* All sysctl names at this level are terminal. */
	if (namelen != 1)
		return (ENOTDIR);

	if (name[0] <= 0 || name[0] >= CARPCTL_MAXID)
		return (ENOPROTOOPT);

	return sysctl_int(oldp, oldlenp, newp, newlen, &carp_opts[name[0]]);
}

/*
 * Interface side of the CARP implementation.
 */

/* ARGSUSED */
void
carpattach(int n)
{
	if_clone_attach(&carp_cloner);
}

int
carp_clone_create(ifc, unit)
	struct if_clone *ifc;
	int unit;
{
	extern int ifqmaxlen;
	struct carp_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT);
	if (!sc)
		return (ENOMEM);
	bzero(sc, sizeof(*sc));

	sc->sc_flags_backup = 0;
	sc->sc_suppress = 0;
	sc->sc_advbase = CARP_DFLTINTV;
	sc->sc_vhid = -1;	/* required setting */
	sc->sc_advskew = 0;
	sc->sc_init_counter = 1;
	sc->sc_naddrs = sc->sc_naddrs6 = 0;
#ifdef INET6
	sc->sc_im6o.im6o_multicast_hlim = CARP_DFLTTL;
#endif /* INET6 */

	timeout_set(&sc->sc_ad_tmo, carp_send_ad, sc);
	timeout_set(&sc->sc_md_tmo, carp_master_down, sc);
	timeout_set(&sc->sc_md6_tmo, carp_master_down, sc);

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "%s%d", ifc->ifc_name,
	    unit);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = 0;
	ifp->if_ioctl = carp_ioctl;
	ifp->if_output = looutput;
	ifp->if_start = carp_start;
	ifp->if_type = IFT_PROPVIRTUAL;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = 0;
	if_attach(ifp);
	if_alloc_sadl(ifp);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_LOOP, sizeof(u_int32_t));
#endif
	return (0);
}

int
carp_clone_destroy(struct ifnet *ifp)
{
	struct carp_softc *sc = ifp->if_softc;
	struct carp_if *cif;

	timeout_del(&sc->sc_ad_tmo);
	timeout_del(&sc->sc_md_tmo);
	timeout_del(&sc->sc_md6_tmo);

	if (sc->sc_ifp != NULL) {
		cif = (struct carp_if *)sc->sc_ifp->if_carp;
		TAILQ_REMOVE(&cif->vhif_vrs, sc, sc_list);
		if (!--cif->vhif_nvrs) {
			sc->sc_ifp->if_carp = NULL;
			FREE(cif, M_IFADDR);
		}
	}

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);

	return (0);
}

void
carpdetach(struct carp_softc *sc)
{
	struct ifaddr *ifa;

	timeout_del(&sc->sc_ad_tmo);
	timeout_del(&sc->sc_md_tmo);
	timeout_del(&sc->sc_md6_tmo);

	while ((ifa = TAILQ_FIRST(&sc->sc_ac.ac_if.if_addrlist)) != NULL)
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct in_ifaddr *ia = ifatoia(ifa);

			carp_del_addr(sc, &ia->ia_addr);

			/* ripped screaming from in_control(SIOCDIFADDR) */
			in_ifscrub(&sc->sc_ac.ac_if, ia);
			TAILQ_REMOVE(&sc->sc_ac.ac_if.if_addrlist,
			    ifa, ifa_list);
			TAILQ_REMOVE(&in_ifaddr, ia, ia_list);
			IFAFREE((&ia->ia_ifa));
		}
}

/* Detach an interface from the carp.  */
void
carp_ifdetach(struct ifnet *ifp)
{
	struct carp_softc *sc;

	TAILQ_FOREACH(sc, &((struct carp_if *)ifp->if_carp)->vhif_vrs, sc_list)
		carpdetach(sc);
}

int
carp_prepare_ad(struct mbuf *m, struct carp_softc *sc, struct carp_header *ch)
{
	struct m_tag *mtag;

	if (sc->sc_init_counter) {
		/* this could also be seconds since unix epoch */
		sc->sc_counter = arc4random();
		sc->sc_counter = sc->sc_counter << 32;
		sc->sc_counter += arc4random();
	} else
		sc->sc_counter++;

	ch->carp_counter[0] = htonl((sc->sc_counter>>32)&0xffffffff);
	ch->carp_counter[1] = htonl(sc->sc_counter&0xffffffff);

	carp_hmac_generate(sc, ch->carp_counter, ch->carp_md);

	/* Tag packet for carp_output */
	mtag = m_tag_get(PACKET_TAG_CARP,
	    sizeof(struct carp_softc *), M_NOWAIT);
	if (mtag == NULL) {
		m_freem(m);
		sc->sc_ac.ac_if.if_oerrors++;
		return (ENOMEM);
	}
	bcopy(&sc, (caddr_t)(mtag + 1), sizeof(struct carp_softc *));
	m_tag_prepend(m, mtag);

	return (0);
}

void
carp_send_ad_all(void)
{
	struct ifnet *ifp;
	struct carp_if *cif;
	struct carp_softc *vh;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_carp == NULL)
			continue;

		cif = (struct carp_if *)ifp->if_carp;
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((vh->sc_ac.ac_if.if_flags & (IFF_UP|IFF_RUNNING)) &&
			     vh->sc_state == MASTER)
				carp_send_ad(vh);
		}
	}
}


void
carp_send_ad(void *v)
{
	struct carp_header ch;
	struct timeval tv;
	struct carp_softc *sc = v;
	struct carp_header *ch_ptr;
	struct mbuf *m;
	int len, advbase, advskew;

	/* bow out if we've lost our UPness or RUNNINGuiness */
	if ((sc->sc_ac.ac_if.if_flags &
	    (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		advbase = 255;
		advskew = 255;
	} else {
		advbase = sc->sc_advbase;
		if (!carp_suppress_preempt || sc->sc_advskew > 240)
			advskew = sc->sc_advskew;
		else
			advskew = 240;
		tv.tv_sec = advbase;
		tv.tv_usec = advskew * 1000000 / 256;
	}

	ch.carp_version = CARP_VERSION;
	ch.carp_type = CARP_ADVERTISEMENT;
	ch.carp_vhid = sc->sc_vhid;
	ch.carp_advbase = advbase;
	ch.carp_advskew = advskew;
	ch.carp_authlen = 7;	/* XXX DEFINE */
	ch.carp_pad1 = 0;	/* must be zero */
	ch.carp_cksum = 0;


#ifdef INET
	if (sc->sc_ia) {
		struct ip *ip;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			sc->sc_ac.ac_if.if_oerrors++;
			carpstats.carps_onomem++;
			/* XXX maybe less ? */
			if (advbase != 255 || advskew != 255)
				timeout_add(&sc->sc_ad_tmo, tvtohz(&tv));
			return;
		}
		len = sizeof(*ip) + sizeof(ch);
		m->m_pkthdr.len = len;
		m->m_pkthdr.rcvif = NULL;
		m->m_len = len;
		MH_ALIGN(m, m->m_len);
		m->m_flags |= M_MCAST;
		ip = mtod(m, struct ip *);
		ip->ip_v = IPVERSION;
		ip->ip_hl = sizeof(*ip) >> 2;
		ip->ip_tos = IPTOS_LOWDELAY;
		ip->ip_len = htons(len);
		ip->ip_id = htons(ip_randomid());
		ip->ip_off = htons(IP_DF);
		ip->ip_ttl = CARP_DFLTTL;
		ip->ip_p = IPPROTO_CARP;
		ip->ip_sum = 0;
		ip->ip_src.s_addr = sc->sc_ia->ia_addr.sin_addr.s_addr;
		ip->ip_dst.s_addr = INADDR_CARP_GROUP;

		ch_ptr = (void *)ip + sizeof(*ip);
		bcopy(&ch, ch_ptr, sizeof(ch));
		if (carp_prepare_ad(m, sc, ch_ptr))
			return;

		m->m_data += sizeof(*ip);
		ch_ptr->carp_cksum = carp_cksum(m, len - sizeof(*ip));
		m->m_data -= sizeof(*ip);

		sc->sc_ac.ac_if.if_lastchange = time;
		sc->sc_ac.ac_if.if_opackets++;
		sc->sc_ac.ac_if.if_obytes += len;
		carpstats.carps_opackets++;

		if (ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL)) {
			sc->sc_ac.ac_if.if_oerrors++;
			if (sc->sc_sendad_errors < INT_MAX)
				sc->sc_sendad_errors++;
			if (sc->sc_sendad_errors == CARP_SENDAD_MAX_ERRORS) {
				carp_suppress_preempt++;
				if (carp_suppress_preempt == 1)
					carp_send_ad_all();
			}
			sc->sc_sendad_success = 0;
		} else {
			if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS) {
				if (++sc->sc_sendad_success >=
				    CARP_SENDAD_MIN_SUCCESS) {
					carp_suppress_preempt--;
					sc->sc_sendad_errors = 0;
				}
			} else
				sc->sc_sendad_errors = 0;
		}
	}
#endif /* INET */
#ifdef INET6
	if (sc->sc_ia6) {
		struct ip6_hdr *ip6;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			sc->sc_ac.ac_if.if_oerrors++;
			carpstats.carps_onomem++;
			/* XXX maybe less ? */
			if (advbase != 255 || advskew != 255)
				timeout_add(&sc->sc_ad_tmo, tvtohz(&tv));
			return;
		}
		len = sizeof(*ip6) + sizeof(ch);
		m->m_pkthdr.len = len;
		m->m_pkthdr.rcvif = NULL;
		m->m_len = len;
		MH_ALIGN(m, m->m_len);
		m->m_flags |= M_MCAST;
		ip6 = mtod(m, struct ip6_hdr *);
		bzero(ip6, sizeof(*ip6));
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_hlim = CARP_DFLTTL;
		ip6->ip6_nxt = IPPROTO_CARP;
		bcopy(&sc->sc_ia6->ia_addr.sin6_addr, &ip6->ip6_src,
		    sizeof(struct in6_addr));
		/* set the multicast destination */

		ip6->ip6_dst.s6_addr8[0] = 0xff;
		ip6->ip6_dst.s6_addr8[1] = 0x02;
		ip6->ip6_dst.s6_addr8[15] = 0x12;

		ch_ptr = (void *)ip6 + sizeof(*ip6);
		bcopy(&ch, ch_ptr, sizeof(ch));
		if (carp_prepare_ad(m, sc, ch_ptr))
			return;

		m->m_data += sizeof(*ip6);
		ch_ptr->carp_cksum = carp_cksum(m, len - sizeof(*ip6));
		m->m_data -= sizeof(*ip6);

		sc->sc_ac.ac_if.if_lastchange = time;
		sc->sc_ac.ac_if.if_opackets++;
		sc->sc_ac.ac_if.if_obytes += len;
		carpstats.carps_opackets6++;

		if (ip6_output(m, NULL, NULL, 0, &sc->sc_im6o, NULL)) {
			sc->sc_ac.ac_if.if_oerrors++;
			if (sc->sc_sendad_errors < INT_MAX)
				sc->sc_sendad_errors++;
			if (sc->sc_sendad_errors == CARP_SENDAD_MAX_ERRORS) {
				carp_suppress_preempt++;
				if (carp_suppress_preempt == 1)
					carp_send_ad_all();
			}
			sc->sc_sendad_success = 0;
		} else {
			if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS) {
				if (++sc->sc_sendad_success >=
				    CARP_SENDAD_MIN_SUCCESS) {
					carp_suppress_preempt--;
					sc->sc_sendad_errors = 0;
				}
			} else
				sc->sc_sendad_errors = 0;
		}
	}
#endif /* INET6 */

	if (advbase != 255 || advskew != 255)
		timeout_add(&sc->sc_ad_tmo, tvtohz(&tv));
}

/*
 * Broadcast a gratuitous ARP request containing
 * the virtual router MAC address for each IP address
 * associated with the virtual router.
 */
void
carp_send_arp(struct carp_softc *sc)
{
	struct ifaddr *ifa;
	in_addr_t in;

	TAILQ_FOREACH(ifa, &sc->sc_ac.ac_if.if_addrlist, ifa_list) {

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		in = ifatoia(ifa)->ia_addr.sin_addr.s_addr;
		arprequest(sc->sc_ifp, &in, &in, sc->sc_ac.ac_enaddr);
		DELAY(1000);	/* XXX */
	}
}

#ifdef INET6
void
carp_send_na(struct carp_softc *sc)
{
	struct ifaddr *ifa;
	struct in6_addr *in6;
	static struct in6_addr mcast = IN6ADDR_LINKLOCAL_ALLNODES_INIT;

	TAILQ_FOREACH(ifa, &sc->sc_ac.ac_if.if_addrlist, ifa_list) {

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		in6 = &ifatoia6(ifa)->ia_addr.sin6_addr;
		nd6_na_output(sc->sc_ifp, &mcast, in6,
		    ND_NA_FLAG_OVERRIDE, 1, NULL);
		DELAY(1000);	/* XXX */
	}
}
#endif /* INET6 */

int
carp_addrcount(struct carp_if *cif, struct in_ifaddr *ia, int type)
{
	struct carp_softc *vh;
	struct ifaddr *ifa;
	int count = 0;

	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
		if ((type == CARP_COUNT_RUNNING &&
		    (vh->sc_ac.ac_if.if_flags & (IFF_UP|IFF_RUNNING)) ==
		    (IFF_UP|IFF_RUNNING)) ||
		    (type == CARP_COUNT_MASTER && vh->sc_state == MASTER)) {
			TAILQ_FOREACH(ifa, &vh->sc_ac.ac_if.if_addrlist,
			    ifa_list) {
				if (ifa->ifa_addr->sa_family == AF_INET &&
				    ia->ia_addr.sin_addr.s_addr ==
				    ifatoia(ifa)->ia_addr.sin_addr.s_addr)
					count++;
			}
		}
	}
	return (count);
}

int
carp_iamatch(void *v, struct in_ifaddr *ia,
    struct in_addr *isaddr, u_int8_t **enaddr)
{
	struct carp_if *cif = v;
	struct carp_softc *vh;
	int index, count = 0;
	struct ifaddr *ifa;

	if (carp_opts[CARPCTL_ARPBALANCE]) {
		/*
		 * XXX proof of concept implementation.
		 * We use the source ip to decide which virtual host should
		 * handle the request. If we're master of that virtual host,
		 * then we respond, otherwise, just drop the arp packet on
		 * the floor.
		 */
		count = carp_addrcount(cif, ia, CARP_COUNT_RUNNING);
		if (count == 0) {
			/* should never reach this */
			return (0);
		}

		/* this should be a hash, like pf_hash() */
		index = isaddr->s_addr % count;
		count = 0;

		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((vh->sc_ac.ac_if.if_flags & (IFF_UP|IFF_RUNNING)) ==
			    (IFF_UP|IFF_RUNNING)) {
				TAILQ_FOREACH(ifa, &vh->sc_ac.ac_if.if_addrlist,
				    ifa_list) {
					if (ifa->ifa_addr->sa_family ==
					    AF_INET &&
					    ia->ia_addr.sin_addr.s_addr ==
					    ifatoia(ifa)->ia_addr.sin_addr.s_addr) {
						if (count == index) {
							if (vh->sc_state ==
							    MASTER) {
								*enaddr = vh->sc_ac.ac_enaddr;
								return (1);
							} else
								return (0);
						}
						count++;
					}
				}
			}
		}
	} else {
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((vh->sc_ac.ac_if.if_flags & (IFF_UP|IFF_RUNNING)) ==
			    (IFF_UP|IFF_RUNNING) && ia->ia_ifp ==
			    &vh->sc_ac.ac_if) {
				*enaddr = vh->sc_ac.ac_enaddr;
				return (1);
			}
		}
	}

	return (0);
}

#ifdef INET6
struct ifaddr *
carp_iamatch6(void *v, struct in6_addr *taddr)
{
	struct carp_if *cif = v;
	struct carp_softc *vh;
	struct ifaddr *ifa;

	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
		TAILQ_FOREACH(ifa, &vh->sc_ac.ac_if.if_addrlist, ifa_list) {
			if (IN6_ARE_ADDR_EQUAL(taddr,
			    &ifatoia6(ifa)->ia_addr.sin6_addr) &&
			    ((vh->sc_ac.ac_if.if_flags &
			    (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING)))
				return (ifa);
		}
	}

	return (NULL);
}

void *
carp_macmatch6(void *v, struct mbuf *m, struct in6_addr *taddr)
{
	struct m_tag *mtag;
	struct carp_if *cif = v;
	struct carp_softc *sc;
	struct ifaddr *ifa;


	TAILQ_FOREACH(sc, &cif->vhif_vrs, sc_list) {
		TAILQ_FOREACH(ifa, &sc->sc_ac.ac_if.if_addrlist, ifa_list) {
			if (IN6_ARE_ADDR_EQUAL(taddr,
			    &ifatoia6(ifa)->ia_addr.sin6_addr) &&
			    ((sc->sc_ac.ac_if.if_flags &
			    (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING))) {
				mtag = m_tag_get(PACKET_TAG_CARP,
				    sizeof(struct carp_softc *), M_NOWAIT);
				if (mtag == NULL) {
					/* better a bit than nothing */
					return (sc->sc_ac.ac_enaddr);
				}
				bcopy(&sc, (caddr_t)(mtag + 1),
				    sizeof(struct carp_softc *));
				m_tag_prepend(m, mtag);

				return (sc->sc_ac.ac_enaddr);
			}
		}
	}

	return (NULL);
}
#endif /* INET6 */

struct ifnet *
carp_forus(void *v, void *dhost)
{
	struct carp_if *cif = v;
	struct carp_softc *vh;
	u_int8_t *ena = dhost;

	if (ena[0] || ena[1] || ena[2] != 0x5e || ena[3] || ena[4] != 1)
		return (NULL);

	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list)
		if ((vh->sc_ac.ac_if.if_flags & (IFF_UP|IFF_RUNNING)) ==
		    (IFF_UP|IFF_RUNNING) && vh->sc_state == MASTER &&
		    !bcmp(dhost, vh->sc_ac.ac_enaddr, ETHER_ADDR_LEN))
			return (&vh->sc_ac.ac_if);

	return (NULL);
}

void
carp_master_down(void *v)
{
	struct carp_softc *sc = v;

	switch (sc->sc_state) {
	case INIT:
		printf("%s: master_down event in INIT state\n",
		    sc->sc_ac.ac_if.if_xname);
		break;
	case MASTER:
		break;
	case BACKUP:
		carp_set_state(sc, MASTER);
		carp_send_ad(sc);
		carp_send_arp(sc);
#ifdef INET6
		carp_send_na(sc);
#endif /* INET6 */
		carp_setrun(sc, 0);
		carp_setroute(sc, RTM_ADD);
		break;
	}
}

/*
 * When in backup state, af indicates whether to reset the master down timer
 * for v4 or v6. If it's set to zero, reset the ones which are already pending.
 */
void
carp_setrun(struct carp_softc *sc, sa_family_t af)
{
	struct timeval tv;

	if (sc->sc_ac.ac_if.if_flags & IFF_UP &&
	    sc->sc_vhid > 0 && (sc->sc_naddrs || sc->sc_naddrs6))
		sc->sc_ac.ac_if.if_flags |= IFF_RUNNING;
	else {
		sc->sc_ac.ac_if.if_flags &= ~IFF_RUNNING;
		carp_setroute(sc, RTM_DELETE);
		return;
	}

	switch (sc->sc_state) {
	case INIT:
		if (carp_opts[CARPCTL_PREEMPT] && !carp_suppress_preempt) {
			carp_send_ad(sc);
			carp_send_arp(sc);
#ifdef INET6
			carp_send_na(sc);
#endif /* INET6 */
			carp_set_state(sc, MASTER);
			carp_setroute(sc, RTM_ADD);
		} else {
			carp_set_state(sc, BACKUP);
			carp_setroute(sc, RTM_DELETE);
			carp_setrun(sc, 0);
		}
		break;
	case BACKUP:
		timeout_del(&sc->sc_ad_tmo);
		tv.tv_sec = 3 * sc->sc_advbase;
		tv.tv_usec = sc->sc_advskew * 1000000 / 256;
		switch (af) {
#ifdef INET
		case AF_INET:
			timeout_add(&sc->sc_md_tmo, tvtohz(&tv));
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			timeout_add(&sc->sc_md6_tmo, tvtohz(&tv));
			break;
#endif /* INET6 */
		default:
			if (sc->sc_naddrs)
				timeout_add(&sc->sc_md_tmo, tvtohz(&tv));
			if (sc->sc_naddrs6)
				timeout_add(&sc->sc_md6_tmo, tvtohz(&tv));
			break;
		}
		break;
	case MASTER:
		tv.tv_sec = sc->sc_advbase;
		tv.tv_usec = sc->sc_advskew * 1000000 / 256;
		timeout_add(&sc->sc_ad_tmo, tvtohz(&tv));
		break;
	}
}

int
carp_set_addr(struct carp_softc *sc, struct sockaddr_in *sin)
{
	struct ifnet *ifp;
	struct carp_if *cif;
	struct in_ifaddr *ia, *ia_if;
	struct ip_moptions *imo = &sc->sc_imo;
	struct in_addr addr;
	int own, error;

	if (sin->sin_addr.s_addr == 0) {
		if (!(sc->sc_ac.ac_if.if_flags & IFF_UP))
			carp_set_state(sc, INIT);
		if (sc->sc_naddrs)
			sc->sc_ac.ac_if.if_flags |= IFF_UP;
		carp_setrun(sc, 0);
		return (0);
	}

	/* we have to do it by hands to check we won't match on us */
	ia_if = NULL; own = 0;
	for (ia = TAILQ_FIRST(&in_ifaddr); ia; ia = TAILQ_NEXT(ia, ia_list)) {

		/* and, yeah, we need a multicast-capable iface too */
		if (ia->ia_ifp != &sc->sc_ac.ac_if &&
		    (ia->ia_ifp->if_flags & IFF_MULTICAST) &&
		    (sin->sin_addr.s_addr & ia->ia_subnetmask) ==
		    ia->ia_subnet) {
			if (!ia_if)
				ia_if = ia;
			if (sin->sin_addr.s_addr == ia->ia_addr.sin_addr.s_addr)
				own++;
		}
	}

	if (!ia_if)
		return (EADDRNOTAVAIL);
	ia = ia_if;
	ifp = ia->ia_ifp;

	if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0 ||
	    (imo->imo_multicast_ifp && imo->imo_multicast_ifp != ifp))
		return (EADDRNOTAVAIL);

	if (imo->imo_num_memberships == 0) {
		addr.s_addr = INADDR_CARP_GROUP;
		if ((imo->imo_membership[0] = in_addmulti(&addr, ifp)) == NULL)
			return (ENOBUFS);
		imo->imo_num_memberships++;
		imo->imo_multicast_ifp = ifp;
		imo->imo_multicast_ttl = CARP_DFLTTL;
		imo->imo_multicast_loop = 0;
	}

	if (!ifp->if_carp) {

		MALLOC(cif, struct carp_if *, sizeof(*cif), M_IFADDR, M_WAITOK);
		if (!cif) {
			error = ENOBUFS;
			goto cleanup;
		}
		if ((error = ifpromisc(ifp, 1))) {
			FREE(cif, M_IFADDR);
			goto cleanup;
		}

		cif->vhif_ifp = ifp;
		TAILQ_INIT(&cif->vhif_vrs);
		ifp->if_carp = (caddr_t)cif;

	} else {
		struct carp_softc *vr;

		cif = (struct carp_if *)ifp->if_carp;
		TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
			if (vr != sc && vr->sc_vhid == sc->sc_vhid) {
				error = EINVAL;
				goto cleanup;
			}
	}
	sc->sc_ia = ia;
	sc->sc_ifp = ifp;

	{ /* XXX prevent endless loop if already in queue */
	struct carp_softc *vr, *after = NULL;
	int myself = 0;
	cif = (struct carp_if *)ifp->if_carp;

	TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list) {
		if (vr == sc)
			myself = 1;
		if (vr->sc_vhid < sc->sc_vhid)
			after = vr;
	}

	if (!myself) {
		/* We're trying to keep things in order */
		if (after == NULL) {
			TAILQ_INSERT_TAIL(&cif->vhif_vrs, sc, sc_list);
		} else {
			TAILQ_INSERT_AFTER(&cif->vhif_vrs, after, sc, sc_list);
		}
		cif->vhif_nvrs++;
	}
	}

	sc->sc_naddrs++;
	sc->sc_ac.ac_if.if_flags |= IFF_UP;
	if (own)
		sc->sc_advskew = 0;
	carp_set_state(sc, INIT);
	carp_setrun(sc, 0);

	return (0);

cleanup:
	in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
	return (error);
}

int
carp_del_addr(struct carp_softc *sc, struct sockaddr_in *sin)
{
	int error = 0;

	if (!--sc->sc_naddrs) {
		struct carp_if *cif = (struct carp_if *)sc->sc_ifp->if_carp;
		struct ip_moptions *imo = &sc->sc_imo;

		timeout_del(&sc->sc_ad_tmo);
		sc->sc_ac.ac_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
		sc->sc_vhid = -1;
		in_delmulti(imo->imo_membership[--imo->imo_num_memberships]);
		imo->imo_multicast_ifp = NULL;
		TAILQ_REMOVE(&cif->vhif_vrs, sc, sc_list);
		if (!--cif->vhif_nvrs) {
			sc->sc_ifp->if_carp = NULL;
			FREE(cif, M_IFADDR);
		}
	}

	return (error);
}

#ifdef INET6
int
carp_set_addr6(struct carp_softc *sc, struct sockaddr_in6 *sin6)
{
	struct ifnet *ifp;
	struct carp_if *cif;
	struct in6_ifaddr *ia, *ia_if;
	struct ip6_moptions *im6o = &sc->sc_im6o;
	struct in6_multi_mship *imm;
	struct sockaddr_in6 addr;
	int own, error;

	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		if (!(sc->sc_ac.ac_if.if_flags & IFF_UP))
			carp_set_state(sc, INIT);
		if (sc->sc_naddrs6)
			sc->sc_ac.ac_if.if_flags |= IFF_UP;
		carp_setrun(sc, 0);
		return (0);
	}

	/* we have to do it by hands to check we won't match on us */
	ia_if = NULL; own = 0;
	for (ia = in6_ifaddr; ia; ia = ia->ia_next) {
		int i;

		for (i = 0; i < 4; i++) {
			if ((sin6->sin6_addr.s6_addr32[i] &
			    ia->ia_prefixmask.sin6_addr.s6_addr32[i]) !=
			    (ia->ia_addr.sin6_addr.s6_addr32[i] &
			    ia->ia_prefixmask.sin6_addr.s6_addr32[i]))
				break;
		}
		/* and, yeah, we need a multicast-capable iface too */
		if (ia->ia_ifp != &sc->sc_ac.ac_if &&
		    (ia->ia_ifp->if_flags & IFF_MULTICAST) &&
		    (i == 4)) {
			if (!ia_if)
				ia_if = ia;
			if (IN6_ARE_ADDR_EQUAL(&sin6->sin6_addr,
			    &ia->ia_addr.sin6_addr))
				own++;
		}
	}

	if (!ia_if)
		return (EADDRNOTAVAIL);
	ia = ia_if;
	ifp = ia->ia_ifp;

	if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0 ||
	    (im6o->im6o_multicast_ifp && im6o->im6o_multicast_ifp != ifp))
		return (EADDRNOTAVAIL);

	if (!sc->sc_naddrs6) {
		im6o->im6o_multicast_ifp = ifp;

		/* join CARP multicast address */
		bzero(&addr, sizeof(addr));
		addr.sin6_family = AF_INET6;
		addr.sin6_len = sizeof(addr);
		addr.sin6_addr.s6_addr16[0] = htons(0xff02);
		addr.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
		addr.sin6_addr.s6_addr8[15] = 0x12;
		if ((imm = in6_joingroup(ifp, &addr.sin6_addr, &error)) == NULL)
			goto cleanup;
		LIST_INSERT_HEAD(&im6o->im6o_memberships, imm, i6mm_chain);

		/* join solicited multicast address */
		bzero(&addr.sin6_addr, sizeof(addr.sin6_addr));
		addr.sin6_addr.s6_addr16[0] = htons(0xff02);
		addr.sin6_addr.s6_addr16[1] = htons(ifp->if_index);
		addr.sin6_addr.s6_addr32[1] = 0;
		addr.sin6_addr.s6_addr32[2] = htonl(1);
		addr.sin6_addr.s6_addr32[3] = sin6->sin6_addr.s6_addr32[3];
		addr.sin6_addr.s6_addr8[12] = 0xff;
		if ((imm = in6_joingroup(ifp, &addr.sin6_addr, &error)) == NULL)
			goto cleanup;
		LIST_INSERT_HEAD(&im6o->im6o_memberships, imm, i6mm_chain);
	}

	if (!ifp->if_carp) {
		MALLOC(cif, struct carp_if *, sizeof(*cif), M_IFADDR, M_WAITOK);
		if (!cif) {
			error = ENOBUFS;
			goto cleanup;
		}
		if ((error = ifpromisc(ifp, 1))) {
			FREE(cif, M_IFADDR);
			goto cleanup;
		}

		cif->vhif_ifp = ifp;
		TAILQ_INIT(&cif->vhif_vrs);
		ifp->if_carp = (caddr_t)cif;

	} else {
		struct carp_softc *vr;

		cif = (struct carp_if *)ifp->if_carp;
		TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
			if (vr != sc && vr->sc_vhid == sc->sc_vhid) {
				error = EINVAL;
				goto cleanup;
			}
	}
	sc->sc_ia6 = ia;
	sc->sc_ifp = ifp;

	{ /* XXX prevent endless loop if already in queue */
	struct carp_softc *vr, *after = NULL;
	int myself = 0;
	cif = (struct carp_if *)ifp->if_carp;

	TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list) {
		if (vr == sc)
			myself = 1;
		if (vr->sc_vhid < sc->sc_vhid)
			after = vr;
	}

	if (!myself) {
		/* We're trying to keep things in order */
		if (after == NULL) {
			TAILQ_INSERT_TAIL(&cif->vhif_vrs, sc, sc_list);
		} else {
			TAILQ_INSERT_AFTER(&cif->vhif_vrs, after, sc, sc_list);
		}
		cif->vhif_nvrs++;
	}
	}

	sc->sc_naddrs6++;
	sc->sc_ac.ac_if.if_flags |= IFF_UP;
	if (own)
		sc->sc_advskew = 0;
	carp_set_state(sc, INIT);
	carp_setrun(sc, 0);

	return (0);

cleanup:
	/* clean up multicast memberships */
	if (!sc->sc_naddrs6) {
		while (!LIST_EMPTY(&im6o->im6o_memberships)) {
			imm = LIST_FIRST(&im6o->im6o_memberships);
			LIST_REMOVE(imm, i6mm_chain);
			in6_leavegroup(imm);
		}
	}
	return (error);
}

int
carp_del_addr6(struct carp_softc *sc, struct sockaddr_in6 *sin6)
{
	int error = 0;

	if (!--sc->sc_naddrs6) {
		struct carp_if *cif = (struct carp_if *)sc->sc_ifp->if_carp;
		struct ip6_moptions *im6o = &sc->sc_im6o;

		timeout_del(&sc->sc_ad_tmo);
		sc->sc_ac.ac_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
		sc->sc_vhid = -1;
		while (!LIST_EMPTY(&im6o->im6o_memberships)) {
			struct in6_multi_mship *imm =
			    LIST_FIRST(&im6o->im6o_memberships);

			LIST_REMOVE(imm, i6mm_chain);
			in6_leavegroup(imm);
		}
		im6o->im6o_multicast_ifp = NULL;
		TAILQ_REMOVE(&cif->vhif_vrs, sc, sc_list);
		if (!--cif->vhif_nvrs) {
			sc->sc_ifp->if_carp = NULL;
			FREE(cif, M_IFADDR);
		}
	}

	return (error);
}

#endif /* INET6 */

int
carp_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct proc *p = curproc;	/* XXX */
	struct carp_softc *sc = ifp->if_softc, *vr;
	struct carpreq carpr;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	struct ifaliasreq *ifra;
	int error = 0;

	ifa = (struct ifaddr *)addr;
	ifra = (struct ifaliasreq *)addr;
	ifr = (struct ifreq *)addr;

	switch (cmd) {
	case SIOCSIFADDR:
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			sc->sc_ac.ac_if.if_flags |= IFF_UP;
			bcopy(ifa->ifa_addr, ifa->ifa_dstaddr,
			    sizeof(struct sockaddr));
			error = carp_set_addr(sc, satosin(ifa->ifa_addr));
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			sc->sc_ac.ac_if.if_flags|= IFF_UP;
			error = carp_set_addr6(sc, satosin6(ifa->ifa_addr));
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCAIFADDR:
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			sc->sc_ac.ac_if.if_flags |= IFF_UP;
			bcopy(ifa->ifa_addr, ifa->ifa_dstaddr,
			    sizeof(struct sockaddr));
			error = carp_set_addr(sc, satosin(&ifra->ifra_addr));
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			sc->sc_ac.ac_if.if_flags |= IFF_UP;
			error = carp_set_addr6(sc, satosin6(&ifra->ifra_addr));
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCDIFADDR:
		sc->sc_ac.ac_if.if_flags &= ~IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			error = carp_del_addr(sc, satosin(&ifra->ifra_addr));
			break;
#endif /* INET */
#ifdef INET6
		case AF_INET6:
			error = carp_del_addr6(sc, satosin6(&ifra->ifra_addr));
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if (sc->sc_ac.ac_if.if_flags & IFF_UP &&
		    (ifr->ifr_flags & IFF_UP) == 0) {
			sc->sc_ac.ac_if.if_flags &= ~IFF_UP;
			timeout_del(&sc->sc_ad_tmo);
			timeout_del(&sc->sc_md_tmo);
			timeout_del(&sc->sc_md6_tmo);
			if (sc->sc_state == MASTER)
				carp_send_ad(sc);
			carp_set_state(sc, INIT);
			carp_setrun(sc, 0);
		}
		if (ifr->ifr_flags & IFF_UP &&
		    (sc->sc_ac.ac_if.if_flags & IFF_UP) == 0) {
			sc->sc_ac.ac_if.if_flags |= IFF_UP;
			carp_set_state(sc, INIT);
			carp_setrun(sc, 0);
		}
		break;

	case SIOCSVH:
		if ((error = suser(p, p->p_acflag)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &carpr, sizeof carpr)))
			break;
		error = 1;
		if (sc->sc_state != INIT && carpr.carpr_state != sc->sc_state) {
			switch (carpr.carpr_state) {
			case BACKUP:
				timeout_del(&sc->sc_ad_tmo);
				carp_set_state(sc, BACKUP);
				carp_setrun(sc, 0);
				carp_setroute(sc, RTM_DELETE);
				break;
			case MASTER:
				carp_master_down(sc);
				break;
			default:
				break;
			}
		}
		if (carpr.carpr_vhid > 0) {
			if (carpr.carpr_vhid > 255) {
				error = EINVAL;
				break;
			}
			if (sc->sc_ifp) {
				struct carp_if *cif;
				cif = (struct carp_if *)sc->sc_ifp->if_carp;
				TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
					if (vr != sc &&
					    vr->sc_vhid == carpr.carpr_vhid)
						return (EINVAL);
			}
			sc->sc_vhid = carpr.carpr_vhid;
			sc->sc_ac.ac_enaddr[0] = 0;
			sc->sc_ac.ac_enaddr[1] = 0;
			sc->sc_ac.ac_enaddr[2] = 0x5e;
			sc->sc_ac.ac_enaddr[3] = 0;
			sc->sc_ac.ac_enaddr[4] = 1;
			sc->sc_ac.ac_enaddr[5] = sc->sc_vhid;
			error--;
		}
		if (carpr.carpr_advbase > 0 || carpr.carpr_advskew > 0) {
			if (carpr.carpr_advskew >= 255) {
				error = EINVAL;
				break;
			}
			if (carpr.carpr_advbase > 255) {
				error = EINVAL;
				break;
			}
			sc->sc_advbase = carpr.carpr_advbase;
			sc->sc_advskew = carpr.carpr_advskew;
			error--;
		}
		bcopy(carpr.carpr_key, sc->sc_key, sizeof(sc->sc_key));
		if (error > 0)
			error = EINVAL;
		else {
			error = 0;
			carp_setrun(sc, 0);
		}
		break;

	case SIOCGVH:
		bzero(&carpr, sizeof(carpr));
		carpr.carpr_state = sc->sc_state;
		carpr.carpr_vhid = sc->sc_vhid;
		carpr.carpr_advbase = sc->sc_advbase;
		carpr.carpr_advskew = sc->sc_advskew;
		if (suser(p, p->p_acflag) == 0)
			bcopy(sc->sc_key, carpr.carpr_key,
			    sizeof(carpr.carpr_key));
		error = copyout(&carpr, ifr->ifr_data, sizeof(carpr));
		break;

	default:
		error = EINVAL;
	}

	carp_hmac_prepare(sc);
	return (error);
}


/*
 * Start output on carp interface. This function should never be called.
 */
void
carp_start(struct ifnet *ifp)
{
#ifdef DEBUG
	printf("%s: start called\n", ifp->if_xname);
#endif
}

int
carp_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *sa,
    struct rtentry *rt)
{
	struct m_tag *mtag;
	struct carp_softc *sc;

	if (!sa)
		return (0);

	switch (sa->sa_family) {
#ifdef INET
	case AF_INET:
		break;
#endif /* INET */
#ifdef INET6
	case AF_INET6:
		break;
#endif /* INET6 */
	default:
		return (0);
	}

	mtag = m_tag_find(m, PACKET_TAG_CARP, NULL);
	if (mtag == NULL)
		return (0);

	bcopy(mtag + 1, &sc, sizeof(struct carp_softc *));

	/* Set the source MAC address to Virtual Router MAC Address */
	switch (ifp->if_type) {
#if NETHER > 0
	case IFT_ETHER: {
			struct ether_header *eh;

			eh = mtod(m, struct ether_header *);
			eh->ether_shost[0] = 0;
			eh->ether_shost[1] = 0;
			eh->ether_shost[2] = 0x5e;
			eh->ether_shost[3] = 0;
			eh->ether_shost[4] = 1;
			eh->ether_shost[5] = sc->sc_vhid;
		}
		break;
#endif
#if NFDDI > 0
	case IFT_FDDI: {
			struct fddi_header *fh;

			fh = mtod(m, struct fddi_header *);
			fh->fddi_shost[0] = 0;
			fh->fddi_shost[1] = 0;
			fh->fddi_shost[2] = 0x5e;
			fh->fddi_shost[3] = 0;
			fh->fddi_shost[4] = 1;
			fh->fddi_shost[5] = sc->sc_vhid;
		}
		break;
#endif
#if NTOKEN > 0
	case IFT_ISO88025: {
			struct token_header *th;

			th = mtod(m, struct token_header *);
			th->token_shost[0] = 3;
			th->token_shost[1] = 0;
			th->token_shost[2] = 0x40 >> (sc->sc_vhid - 1);
			th->token_shost[3] = 0x40000 >> (sc->sc_vhid - 1);
			th->token_shost[4] = 0;
			th->token_shost[5] = 0;
		}
		break;
#endif
	default:
		printf("%s: carp is not supported for this interface type\n",
		    ifp->if_xname);
		return (EOPNOTSUPP);
	}

	return (0);
}

void
carp_set_state(struct carp_softc *sc, int state)
{
	if (sc->sc_state == state)
		return;

	sc->sc_state = state;
	switch (state) {
	case BACKUP:
		sc->sc_ac.ac_if.if_link_state = LINK_STATE_DOWN;
		break;
	case MASTER:
		sc->sc_ac.ac_if.if_link_state = LINK_STATE_UP;
		break;
	default:
		sc->sc_ac.ac_if.if_link_state = LINK_STATE_UNKNOWN;
		break;
	}
	rt_ifmsg(&sc->sc_ac.ac_if);
}

void
carp_carpdev_state(void *v)
{
	struct carp_if *cif = v;
	struct carp_softc *sc;

	TAILQ_FOREACH(sc, &cif->vhif_vrs, sc_list) {
		if (sc->sc_ifp->if_link_state == LINK_STATE_DOWN ||
		    !(sc->sc_ifp->if_flags & IFF_UP)) {
			sc->sc_flags_backup = sc->sc_ac.ac_if.if_flags;
			sc->sc_ac.ac_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
			timeout_del(&sc->sc_ad_tmo);
			timeout_del(&sc->sc_md_tmo);
			timeout_del(&sc->sc_md6_tmo);
			carp_set_state(sc, INIT);
			carp_setrun(sc, 0);
			if (!sc->sc_suppress) {
				carp_suppress_preempt++;
				if (carp_suppress_preempt == 1)
					carp_send_ad_all();
			}
			sc->sc_suppress = 1;
		} else {
			sc->sc_ac.ac_if.if_flags |= sc->sc_flags_backup;
			carp_set_state(sc, INIT);
			carp_setrun(sc, 0);
			if (sc->sc_suppress)
				carp_suppress_preempt--;
			sc->sc_suppress = 0;
		}
	}
}
