/*	$OpenBSD: ip_carp.c,v 1.6 2003/10/22 14:56:54 markus Exp $	*/

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
 *	- find a way to schednetisr() packet earlier than through inetsw[];
 *	- track iface ip address changes;
 *	- support for hardware checksum calculations;
 *	- support for inet6;
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
	TAILQ_ENTRY(carp_softc) sc_list;

	enum { INIT = 0, BACKUP, MASTER }	sc_state;

	int sc_vhid;
	int sc_advskew;
	int sc_naddrs;
	int sc_advbase;		/* seconds */
	int sc_init_counter;
	unsigned char sc_key[CARP_KEY_LEN];
	u_int64_t sc_counter;

	struct timeout sc_ad_tmo;	/* advertisement timeout */
	struct timeout sc_md_tmo;	/* master down timeout */

} *carp_softc;
int carp_number;
int carp_opts[CARPCTL_MAXID] = { 0, 1, 0, 1, 0 };	/* XXX for now */
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

void	carp_hmac_generate (struct carp_softc *, u_int32_t *,
	    unsigned char *);
int	carp_hmac_verify (struct carp_softc *, u_int32_t *,
	    unsigned char *);
void	carpattach (int);
void	carpdetach (struct carp_softc *);
void	carp_send_ad (void *);
void	carp_send_arp (struct carp_softc *);
void	carp_master_down (void *);
int	carp_sluggish (struct carp_softc *, struct carp_header *);
int	carp_ioctl (struct ifnet *, u_long, caddr_t);
void	carp_start (struct ifnet *);
void	carp_setrun (struct carp_softc *);
int	carp_set_addr (struct carp_softc *, struct sockaddr_in *);
int	carp_del_addr (struct carp_softc *, struct sockaddr_in *);

static __inline u_int16_t
carp_cksum(struct mbuf *m, int len)
{
	return in_cksum(m, len);
}

#define CARP_HMAC_PAD	64

void
carp_hmac_generate(struct carp_softc *sc, u_int32_t counter[2],
    unsigned char md[20])
{
	SHA1_CTX sha1ctx;
	u_int8_t version = CARP_VERSION, type = CARP_ADVERTISEMENT;
	u_int8_t vhid = sc->sc_vhid & 0xff;
	struct ifaddr *ifa;

	unsigned char ipad[CARP_HMAC_PAD], opad[CARP_HMAC_PAD];
	int i;

	/* pad keys */
	/* XXX precompute ipad/opad and store in sc */
	bzero(ipad, CARP_HMAC_PAD);
	bzero(opad, CARP_HMAC_PAD);
	bcopy(sc->sc_key, ipad, sizeof(sc->sc_key));
	bcopy(sc->sc_key, opad, sizeof(sc->sc_key));
	for (i = 0; i < CARP_HMAC_PAD; i++) {
		ipad[i] ^= 0x36;
		opad[i] ^= 0x5c;
	}

	/* inner hash */
	SHA1Init(&sha1ctx);
	SHA1Update(&sha1ctx, ipad, CARP_HMAC_PAD);
	SHA1Update(&sha1ctx, (void *)&version, sizeof(version));
	SHA1Update(&sha1ctx, (void *)&type, sizeof(type));
	SHA1Update(&sha1ctx, (void *)&vhid, sizeof(vhid));
	TAILQ_FOREACH(ifa, &sc->sc_ac.ac_if.if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET)
			SHA1Update(&sha1ctx,
			    (void *)&ifatoia(ifa)->ia_addr.sin_addr.s_addr,
			    sizeof(u_int32_t));
	}
	SHA1Update(&sha1ctx, (void *)counter, sizeof(sc->sc_counter));
	SHA1Final(md, &sha1ctx);

	/* outer hash */
	SHA1Init(&sha1ctx);
	SHA1Update(&sha1ctx, opad, CARP_HMAC_PAD);
	SHA1Update(&sha1ctx, md, sizeof(md));
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

/*
 * process input packet.
 * we have rearranged checks order compared to the rfc,
 * but it seems more efficient this way or not possible otherwise.
 */
void
carp_input(struct mbuf *m, ...)
{
	struct ifnet *ifp = m->m_pkthdr.rcvif;
	struct carp_softc *sc;
	struct ip *ip = mtod(m, struct ip *);
	struct carp_header *ch;
	int iplen, len, hlen;
	va_list ap;
	u_int64_t tmp_counter;
	struct timeval sc_tv, ch_tv;

	va_start(ap, m);
	hlen = va_arg(ap, int);
	va_end(ap);

	carpstats.carps_ipackets++;

	if (!carp_opts[CARPCTL_ALLOW]) {
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
		CARP_LOG("received len %d < 8",
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
		u_int32_t af = htonl(AF_INET);

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

	if (len < m->m_len) {
		if ((m = m_pullup(m, len)) == NULL) {
			carpstats.carps_hdrops++;
			sc->sc_ac.ac_if.if_ierrors++;
			/* CARP_LOG ? */
			m_freem(m);
			return;
		}
		ip = mtod(m, struct ip *);
		ch = mtod(m, struct carp_header *) + iplen;
	}
	len -= iplen;

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

	if (sc->sc_init_counter)
		sc->sc_init_counter = 0;
	sc->sc_counter = tmp_counter;


	sc_tv.tv_sec = sc->sc_advbase;
	sc_tv.tv_usec = sc->sc_advskew * 1000000 / 256;
	ch_tv.tv_sec = ch->carp_advbase;
	ch_tv.tv_usec = ch->carp_advskew * 1000000 / 256;

	switch (sc->sc_state) {
	case INIT:
		break;
	case MASTER:
		/*
		 * If we recieve an advertisement from a master who's going to
		 * be more frequent than us, go into BACKUP state.
		 */
		if (timercmp(&sc_tv, &ch_tv, >) ||
		    (timercmp(&sc_tv, &ch_tv, ==) &&
		    ip->ip_src.s_addr > sc->sc_ia->ia_addr.sin_addr.s_addr)) {
			timeout_del(&sc->sc_ad_tmo);
			sc->sc_state = BACKUP;
			carp_setrun(sc);
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
		carp_setrun(sc);
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
		return ENOTDIR;

	if (name[0] == 0 || name[0] >= CARPCTL_MAXID)
		return ENOPROTOOPT;

	return sysctl_int(oldp, oldlenp, newp, newlen, &carp_opts[name[0]]);
}


/*
 * Interface side of the CARP implementation.
 */
void
carpattach(int number)
{
	extern int ifqmaxlen;
	int i;
	struct carp_softc *sc;
	struct ifnet *ifp;

	carp_softc = malloc(number * sizeof(*carp_softc), M_DEVBUF, M_NOWAIT);
	if (!carp_softc) {
		printf("cannot alloc CARP data\n");
		return;
	}
	bzero(carp_softc, number * sizeof(*carp_softc));
	carp_number = number;

	for (i = 0; i < number; i++) {

		sc = &carp_softc[i];
		sc->sc_advbase = CARP_DFLTINTV;
		sc->sc_vhid = -1;	/* required setting */
		sc->sc_advskew = 0;
		sc->sc_init_counter = 1;
		timeout_set(&sc->sc_ad_tmo, carp_send_ad, sc);
		timeout_set(&sc->sc_md_tmo, carp_master_down, sc);

		ifp = &sc->sc_ac.ac_if;
		ifp->if_softc = sc;
		snprintf(ifp->if_xname, sizeof(ifp->if_xname), "carp%d", i);
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
	}
}

void
carpdetach(struct carp_softc *sc)
{
	struct ifaddr *ifa;

	timeout_del(&sc->sc_ad_tmo);
	timeout_del(&sc->sc_md_tmo);

	while ((ifa = TAILQ_FIRST(&sc->sc_ac.ac_if.if_addrlist)) != NULL)
		if (ifa->ifa_addr->sa_family == AF_INET) {
			struct in_ifaddr *ia = ifatoia(ifa);

			carp_del_addr(sc, &ia->ia_addr);

			/* ripped screaming from in_control(SIOCDIFADDR) */
			in_ifscrub(&sc->sc_ac.ac_if, ia);
			TAILQ_REMOVE(&sc->sc_ac.ac_if.if_addrlist, ifa, ifa_list);
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

void
carp_send_ad(void *v)
{
	struct carp_softc *sc = v;
	struct carp_header *ch;
	struct mbuf *m;
	struct m_tag *mtag;
	struct ip *ip;
	int len, advbase, advskew, error;

	/* bow out if we've lost our UPness or RUNNINGuiness */
	if ((sc->sc_ac.ac_if.if_flags &
	    (IFF_UP|IFF_RUNNING)) != (IFF_UP|IFF_RUNNING)) {
		advbase = 255;
		advskew = 255;
	} else {
		advbase = sc->sc_advbase;
		advskew = sc->sc_advskew;
	}

	carpstats.carps_opackets++;

	/* MGETHDR(m, M_DONTWAIT, MT_HEADER); */
	m = m_gethdr(M_DONTWAIT, MT_HEADER);
	if (m == NULL) {
		sc->sc_ac.ac_if.if_oerrors++;
		carpstats.carps_onomem++;
		/* XXX maybe less ? */
		timeout_add(&sc->sc_ad_tmo, hz * sc->sc_advbase);
		return;
	}
	len = sizeof(*ip) + sizeof(*ch);
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

	ch = (void *)ip + sizeof(*ip);
	ch->carp_version = CARP_VERSION;
	ch->carp_type = CARP_ADVERTISEMENT;
	ch->carp_vhid = sc->sc_vhid;
	ch->carp_advbase = advbase;
	ch->carp_advskew = advskew;
	ch->carp_authlen = 7;	/* XXX DEFINE */
	ch->carp_pad1 = 0;	/* must be zero */
	ch->carp_cksum = 0;

	if (sc->sc_init_counter) {
		/* this could also be seconds since unix epoch */
		sc->sc_counter = arc4random();
		sc->sc_counter = sc->sc_counter << 32;
		sc->sc_counter += arc4random();
	} else if (sc->sc_counter == 0xffffffffffffffff) {
		sc->sc_counter = 0;
	} else
		sc->sc_counter++;

	ch->carp_counter[0] = htonl((sc->sc_counter>>32)&0xffffffff);
	ch->carp_counter[1] = htonl(sc->sc_counter&0xffffffff);

	carp_hmac_generate(sc, ch->carp_counter, ch->carp_md);

	m->m_data += sizeof(*ip);
	ch->carp_cksum = carp_cksum(m, len - sizeof(*ip));
	m->m_data -= sizeof(*ip);

	sc->sc_ac.ac_if.if_lastchange = time;
	sc->sc_ac.ac_if.if_opackets++;
	sc->sc_ac.ac_if.if_obytes += len;

	/* Tag packet for carp_output */
	mtag = m_tag_get(PACKET_TAG_CARP, sizeof(struct carp_softc *), M_NOWAIT);
	if (mtag == NULL) {
		m_freem(m);
		sc->sc_ac.ac_if.if_oerrors++;
		error = ENOMEM;
		return;
	}
	bcopy(&sc, (caddr_t)(mtag + 1), sizeof(struct carp_softc *));
	m_tag_prepend(m, mtag);

	if ((error = ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL)))
		sc->sc_ac.ac_if.if_oerrors++;

	if (advbase)
		timeout_add(&sc->sc_ad_tmo, hz * sc->sc_advbase);
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
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			TAILQ_FOREACH(ifa, &vh->sc_ac.ac_if.if_addrlist,
			    ifa_list) {
				if (ia->ia_addr.sin_addr.s_addr ==
				    ifatoia(ifa)->ia_addr.sin_addr.s_addr)
					count++;
			}
		}
		if (count == 0) {
			/* should never reach this */
			return 1;
		}
		/* this should be a hash, like pf_hash() */
		index = isaddr->s_addr % count;

		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			TAILQ_FOREACH(ifa, &vh->sc_ac.ac_if.if_addrlist,
			    ifa_list) {
		if (ia->ia_addr.sin_addr.s_addr ==
				    ifatoia(ifa)->ia_addr.sin_addr.s_addr) {
					if (index == 0 &&
					    ((vh->sc_ac.ac_if.if_flags &
					    (IFF_UP|IFF_RUNNING)) ==
			    		    (IFF_UP|IFF_RUNNING))) {
						*enaddr = vh->sc_ac.ac_enaddr;
						return (1);
					}
					index--;
				}
			}
		}
		return (0);
	} else {
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((vh->sc_ac.ac_if.if_flags & (IFF_UP|IFF_RUNNING)) ==
			    (IFF_UP|IFF_RUNNING) && ia->ia_ifp ==
			    &vh->sc_ac.ac_if) {
				*enaddr = vh->sc_ac.ac_enaddr;
			}
		}
	}

	return (1);
}

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
		carp_send_ad(sc);
		carp_send_arp(sc);
		sc->sc_state = MASTER;
		carp_setrun(sc);
		break;
	}
}

void
carp_setrun(struct carp_softc *sc)
{
	struct timeval tv;

	if (sc->sc_ac.ac_if.if_flags & IFF_UP &&
	    sc->sc_vhid > 0 && sc->sc_naddrs)
		sc->sc_ac.ac_if.if_flags |= IFF_RUNNING;
	else {
		sc->sc_ac.ac_if.if_flags &= ~IFF_RUNNING;
		return;
	}

	switch (sc->sc_state) {
	case INIT:
		if (carp_opts[CARPCTL_PREEMPT]) {
			carp_send_ad(sc);
			carp_send_arp(sc);
			sc->sc_state = MASTER;
		} else {
			sc->sc_state = BACKUP;
			carp_setrun(sc);
		}
		break;
	case BACKUP:
		tv.tv_sec = 3 * sc->sc_advbase;
		tv.tv_usec = sc->sc_advskew * 1000000 / 256;
		timeout_add(&sc->sc_md_tmo, tvtohz(&tv));
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
			sc->sc_state = INIT;
		if (sc->sc_naddrs)
			sc->sc_ac.ac_if.if_flags |= IFF_UP;
		carp_setrun(sc);
		return 0;
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
		return EADDRNOTAVAIL;
	ia = ia_if;
	ifp = ia->ia_ifp;

	if (ifp == NULL || (ifp->if_flags & IFF_MULTICAST) == 0 ||
	    (imo->imo_multicast_ifp && imo->imo_multicast_ifp != ifp))
		return EADDRNOTAVAIL;

	if (imo->imo_num_memberships == 0) {
		addr.s_addr = INADDR_CARP_GROUP;
		if ((imo->imo_membership[0] = in_addmulti(&addr, ifp)) == NULL)
			return ENOBUFS;
		imo->imo_num_memberships++;
		imo->imo_multicast_ifp = ifp;
		imo->imo_multicast_ttl = CARP_DFLTTL;
		imo->imo_multicast_loop = 0;
	}

	if (!ifp->if_carp) {

		MALLOC(cif, struct carp_if *, sizeof(*cif), M_IFADDR, M_WAITOK);
		if (!cif || (error = ifpromisc(ifp, 1))) {
			in_delmulti(imo->imo_membership[
			    --imo->imo_num_memberships]);
			return cif? error : ENOBUFS;
		}

		cif->vhif_ifp = ifp;
		TAILQ_INIT(&cif->vhif_vrs);
		ifp->if_carp = (caddr_t)cif;

	} else {
		struct carp_softc *vr;

		cif = (struct carp_if *)ifp->if_carp;
		TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
			if (vr != sc && vr->sc_vhid == sc->sc_vhid) {
				in_delmulti(imo->imo_membership[
				    --imo->imo_num_memberships]);
				return EINVAL;
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
	sc->sc_state = INIT;
	carp_setrun(sc);

	return 0;
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

	return error;
}

int
carp_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct proc *p = curproc;	/* XXX */
	struct carp_softc *sc = ifp->if_softc, *vr;
	struct carpreq carpr;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	struct ifaliasreq *ifra;
	register int error = 0;

	ifa = (struct ifaddr *)addr;
	ifra = (struct ifaliasreq *)addr;
	ifr = (struct ifreq *)addr;

	switch (cmd) {
	case SIOCSIFADDR:
		if (ifa->ifa_addr->sa_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		sc->if_flags |= IFF_UP;
		error = carp_set_addr(sc, satosin(ifa->ifa_addr));
		break;

	case SIOCAIFADDR:
		if (ifra->ifra_addr.sa_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		sc->if_flags |= IFF_UP;
		error = carp_set_addr(sc, satosin(&ifra->ifra_addr));
		break;

	case SIOCDIFADDR:
		if (ifra->ifra_addr.sa_family != AF_INET) {
			error = EAFNOSUPPORT;
			break;
		}
		sc->if_flags &= ~IFF_UP;
		error = carp_del_addr(sc, satosin(&ifra->ifra_addr));
		break;

	case SIOCSIFFLAGS:
		if (sc->if_flags & IFF_UP && (ifr->ifr_flags & IFF_UP) == 0) {
			sc->if_flags &= ~IFF_UP;
			timeout_del(&sc->sc_ad_tmo);
			timeout_del(&sc->sc_md_tmo);
			if (sc->sc_state == MASTER)
				carp_send_ad(sc);
			sc->sc_state = INIT;
			carp_setrun(sc);
		}
		if (ifr->ifr_flags & IFF_UP && (sc->if_flags & IFF_UP) == 0) {
			sc->if_flags |= IFF_UP;
			sc->sc_state = INIT;
			carp_setrun(sc);
		}
		break;

	case SIOCSVH:
		if ((error = suser(p, p->p_acflag)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &carpr, sizeof carpr)))
			break;
		error = 1;
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
						return EINVAL;
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
			carp_setrun(sc);
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

	if (sa && sa->sa_family != AF_INET)
		return 0;

	mtag = m_tag_find(m, PACKET_TAG_CARP, NULL);
	if (mtag == NULL)
		return 0;

	bcopy( mtag + 1, &sc, sizeof(struct carp_softc *));

	/* Set the source MAC address to Virtual Router MAC Address */
	switch (ifp->if_type) {
#if NETHER > 0
	case IFT_ETHER: {
			register struct ether_header *eh;

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
			register struct fddi_header *fh;

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
			register struct token_header *th;

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
		return EOPNOTSUPP;
	}

	return 0;
}
