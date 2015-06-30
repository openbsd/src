/*	$OpenBSD: ip_carp.c,v 1.263 2015/06/30 13:54:42 mpi Exp $	*/

/*
 * Copyright (c) 2002 Michael Shalayeff. All rights reserved.
 * Copyright (c) 2003 Ryan McBride. All rights reserved.
 * Copyright (c) 2006-2008 Marco Pfatschbacher. All rights reserved.
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
 *	- support for hardware checksum calculations;
 *
 */

#include "ether.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
#include <sys/timeout.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <sys/syslog.h>

#include <net/if.h>
#include <net/if_var.h>
#include <net/if_types.h>
#include <net/netisr.h>

#include <crypto/sha1.h>

#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip_ipsp.h>

#include <net/if_dl.h>

#ifdef INET6
#include <netinet6/in6_var.h>
#include <netinet/icmp6.h>
#include <netinet/ip6.h>
#include <netinet6/ip6_var.h>
#include <netinet6/nd6.h>
#include <netinet6/in6_ifattach.h>
#endif

#include "bpfilter.h"
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <netinet/ip_carp.h>

struct carp_mc_entry {
	LIST_ENTRY(carp_mc_entry)	mc_entries;
	union {
		struct ether_multi	*mcu_enm;
	} mc_u;
	struct sockaddr_storage		mc_addr;
};
#define	mc_enm	mc_u.mcu_enm

enum { HMAC_ORIG=0, HMAC_NOV6LL=1, HMAC_MAX=2 };

struct carp_vhost_entry {
	LIST_ENTRY(carp_vhost_entry)	vhost_entries;
	struct carp_softc *parent_sc;
	int vhe_leader;
	int vhid;
	int advskew;
	enum { INIT = 0, BACKUP, MASTER }	state;
	struct timeout ad_tmo;	/* advertisement timeout */
	struct timeout md_tmo;	/* master down timeout */
	struct timeout md6_tmo;	/* master down timeout */

	u_int64_t vhe_replay_cookie;

	/* authentication */
#define CARP_HMAC_PAD	64
	unsigned char vhe_pad[CARP_HMAC_PAD];
	SHA1_CTX vhe_sha1[HMAC_MAX];

	u_int8_t vhe_enaddr[ETHER_ADDR_LEN];
	struct sockaddr_dl vhe_sdl;	/* for IPv6 ndp balancing */
};

struct carp_softc {
	struct arpcom sc_ac;
#define	sc_if		sc_ac.ac_if
#define	sc_carpdev	sc_ac.ac_if.if_carpdev
	void *ah_cookie;
	void *lh_cookie;
	struct ifih *sc_ifih;
	struct ip_moptions sc_imo;
#ifdef INET6
	struct ip6_moptions sc_im6o;
#endif /* INET6 */
	TAILQ_ENTRY(carp_softc) sc_list;

	int sc_suppress;
	int sc_bow_out;
	int sc_demote_cnt;

	int sc_sendad_errors;
#define CARP_SENDAD_MAX_ERRORS(sc) (3 * (sc)->sc_vhe_count)
	int sc_sendad_success;
#define CARP_SENDAD_MIN_SUCCESS(sc) (3 * (sc)->sc_vhe_count)

	char sc_curlladdr[ETHER_ADDR_LEN];

	LIST_HEAD(__carp_vhosthead, carp_vhost_entry)	carp_vhosts;
	int sc_vhe_count;
	u_int8_t sc_vhids[CARP_MAXNODES];
	u_int8_t sc_advskews[CARP_MAXNODES];
	u_int8_t sc_balancing;

	int sc_naddrs;
	int sc_naddrs6;
	int sc_advbase;		/* seconds */

	/* authentication */
	unsigned char sc_key[CARP_KEY_LEN];

	u_int32_t sc_hashkey[2];
	u_int32_t sc_lsmask;		/* load sharing mask */
	int sc_lscount;			/* # load sharing interfaces (max 32) */
	int sc_delayed_arp;		/* delayed ARP request countdown */
	int sc_realmac;			/* using real mac */

	struct in_addr sc_peer;

	LIST_HEAD(__carp_mchead, carp_mc_entry)	carp_mc_listhead;
	struct carp_vhost_entry *cur_vhe; /* current active vhe */
};

int carp_opts[CARPCTL_MAXID] = { 0, 1, 0, LOG_CRIT };	/* XXX for now */
struct carpstats carpstats;

int	carp_send_all_recur = 0;

struct carp_if {
	TAILQ_HEAD(, carp_softc) vhif_vrs;
	int vhif_nvrs;

	struct ifnet *vhif_ifp;
};

#define	CARP_LOG(l, sc, s)						\
	do {								\
		if (carp_opts[CARPCTL_LOG] >= l) {			\
			if (sc)						\
				log(l, "%s: ",				\
				    (sc)->sc_if.if_xname);		\
			else						\
				log(l, "carp: ");			\
			addlog s;					\
			addlog("\n");					\
		}							\
	} while (0)

void	carp_hmac_prepare(struct carp_softc *);
void	carp_hmac_prepare_ctx(struct carp_vhost_entry *, u_int8_t);
void	carp_hmac_generate(struct carp_vhost_entry *, u_int32_t *,
	    unsigned char *, u_int8_t);
int	carp_hmac_verify(struct carp_vhost_entry *, u_int32_t *,
	    unsigned char *);
int	carp_input(struct mbuf *);
void	carp_proto_input_c(struct mbuf *, struct carp_header *, int,
	    sa_family_t);
void	carpattach(int);
void	carpdetach(struct carp_softc *);
int	carp_prepare_ad(struct mbuf *, struct carp_vhost_entry *,
	    struct carp_header *);
void	carp_send_ad_all(void);
void	carp_vhe_send_ad_all(struct carp_softc *);
void	carp_send_ad(void *);
void	carp_send_arp(struct carp_softc *);
void	carp_master_down(void *);
int	carp_ioctl(struct ifnet *, u_long, caddr_t);
int	carp_vhids_ioctl(struct carp_softc *, struct carpreq *);
int	carp_check_dup_vhids(struct carp_softc *, struct carp_if *,
	    struct carpreq *);
void	carp_ifgroup_ioctl(struct ifnet *, u_long, caddr_t);
void	carp_ifgattr_ioctl(struct ifnet *, u_long, caddr_t);
void	carp_start(struct ifnet *);
void	carp_setrun_all(struct carp_softc *, sa_family_t);
void	carp_setrun(struct carp_vhost_entry *, sa_family_t);
void	carp_set_state_all(struct carp_softc *, int);
void	carp_set_state(struct carp_vhost_entry *, int);
void	carp_multicast_cleanup(struct carp_softc *);
int	carp_set_ifp(struct carp_softc *, struct ifnet *);
void	carp_set_enaddr(struct carp_softc *);
void	carp_set_vhe_enaddr(struct carp_vhost_entry *);
void	carp_addr_updated(void *);
u_int32_t	carp_hash(struct carp_softc *, u_char *);
int	carp_set_addr(struct carp_softc *, struct sockaddr_in *);
int	carp_join_multicast(struct carp_softc *);
#ifdef INET6
void	carp_send_na(struct carp_softc *);
int	carp_set_addr6(struct carp_softc *, struct sockaddr_in6 *);
int	carp_join_multicast6(struct carp_softc *);
#endif
int	carp_clone_create(struct if_clone *, int);
int	carp_clone_destroy(struct ifnet *);
int	carp_ether_addmulti(struct carp_softc *, struct ifreq *);
int	carp_ether_delmulti(struct carp_softc *, struct ifreq *);
void	carp_ether_purgemulti(struct carp_softc *);
int	carp_group_demote_count(struct carp_softc *);
void	carp_update_lsmask(struct carp_softc *);
int	carp_new_vhost(struct carp_softc *, int, int);
void	carp_destroy_vhosts(struct carp_softc *);
void	carp_del_all_timeouts(struct carp_softc *);

struct if_clone carp_cloner =
    IF_CLONE_INITIALIZER("carp", carp_clone_create, carp_clone_destroy);

#define carp_cksum(_m, _l)	((u_int16_t)in_cksum((_m), (_l)))
#define CARP_IFQ_PRIO	6

void
carp_hmac_prepare(struct carp_softc *sc)
{
	struct carp_vhost_entry *vhe;
	u_int8_t i;

	LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
		for (i = 0; i < HMAC_MAX; i++) {
			carp_hmac_prepare_ctx(vhe, i);
		}
	}
}

void
carp_hmac_prepare_ctx(struct carp_vhost_entry *vhe, u_int8_t ctx)
{
	struct carp_softc *sc = vhe->parent_sc;

	u_int8_t version = CARP_VERSION, type = CARP_ADVERTISEMENT;
	u_int8_t vhid = vhe->vhid & 0xff;
	SHA1_CTX sha1ctx;
	u_int32_t kmd[5];
	struct ifaddr *ifa;
	int i, found;
	struct in_addr last, cur, in;
#ifdef INET6
	struct in6_addr last6, cur6, in6;
#endif /* INET6 */

	/* compute ipad from key */
	memset(vhe->vhe_pad, 0, sizeof(vhe->vhe_pad));
	bcopy(sc->sc_key, vhe->vhe_pad, sizeof(sc->sc_key));
	for (i = 0; i < sizeof(vhe->vhe_pad); i++)
		vhe->vhe_pad[i] ^= 0x36;

	/* precompute first part of inner hash */
	SHA1Init(&vhe->vhe_sha1[ctx]);
	SHA1Update(&vhe->vhe_sha1[ctx], vhe->vhe_pad, sizeof(vhe->vhe_pad));
	SHA1Update(&vhe->vhe_sha1[ctx], (void *)&version, sizeof(version));
	SHA1Update(&vhe->vhe_sha1[ctx], (void *)&type, sizeof(type));

	/* generate a key for the arpbalance hash, before the vhid is hashed */
	if (vhe->vhe_leader) {
		bcopy(&vhe->vhe_sha1[ctx], &sha1ctx, sizeof(sha1ctx));
		SHA1Final((unsigned char *)kmd, &sha1ctx);
		sc->sc_hashkey[0] = kmd[0] ^ kmd[1];
		sc->sc_hashkey[1] = kmd[2] ^ kmd[3];
	}

	/* the rest of the precomputation */
	if (!sc->sc_realmac && vhe->vhe_leader &&
	    memcmp(sc->sc_ac.ac_enaddr, vhe->vhe_enaddr, ETHER_ADDR_LEN) != 0)
		SHA1Update(&vhe->vhe_sha1[ctx], sc->sc_ac.ac_enaddr,
		    ETHER_ADDR_LEN);

	SHA1Update(&vhe->vhe_sha1[ctx], (void *)&vhid, sizeof(vhid));

	/* Hash the addresses from smallest to largest, not interface order */
	cur.s_addr = 0;
	do {
		found = 0;
		last = cur;
		cur.s_addr = 0xffffffff;
		TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET)
				continue;
			in.s_addr = ifatoia(ifa)->ia_addr.sin_addr.s_addr;
			if (ntohl(in.s_addr) > ntohl(last.s_addr) &&
			    ntohl(in.s_addr) < ntohl(cur.s_addr)) {
				cur.s_addr = in.s_addr;
				found++;
			}
		}
		if (found)
			SHA1Update(&vhe->vhe_sha1[ctx],
			    (void *)&cur, sizeof(cur));
	} while (found);
#ifdef INET6
	memset(&cur6, 0x00, sizeof(cur6));
	do {
		found = 0;
		last6 = cur6;
		memset(&cur6, 0xff, sizeof(cur6));
		TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family != AF_INET6)
				continue;
			in6 = ifatoia6(ifa)->ia_addr.sin6_addr;
			if (IN6_IS_SCOPE_EMBED(&in6)) {
				if (ctx == HMAC_NOV6LL)
					continue;
				in6.s6_addr16[1] = 0;
			}
			if (memcmp(&in6, &last6, sizeof(in6)) > 0 &&
			    memcmp(&in6, &cur6, sizeof(in6)) < 0) {
				cur6 = in6;
				found++;
			}
		}
		if (found)
			SHA1Update(&vhe->vhe_sha1[ctx],
			    (void *)&cur6, sizeof(cur6));
	} while (found);
#endif /* INET6 */

	/* convert ipad to opad */
	for (i = 0; i < sizeof(vhe->vhe_pad); i++)
		vhe->vhe_pad[i] ^= 0x36 ^ 0x5c;
}

void
carp_hmac_generate(struct carp_vhost_entry *vhe, u_int32_t counter[2],
    unsigned char md[20], u_int8_t ctx)
{
	SHA1_CTX sha1ctx;

	/* fetch first half of inner hash */
	bcopy(&vhe->vhe_sha1[ctx], &sha1ctx, sizeof(sha1ctx));

	SHA1Update(&sha1ctx, (void *)counter, sizeof(vhe->vhe_replay_cookie));
	SHA1Final(md, &sha1ctx);

	/* outer hash */
	SHA1Init(&sha1ctx);
	SHA1Update(&sha1ctx, vhe->vhe_pad, sizeof(vhe->vhe_pad));
	SHA1Update(&sha1ctx, md, 20);
	SHA1Final(md, &sha1ctx);
}

int
carp_hmac_verify(struct carp_vhost_entry *vhe, u_int32_t counter[2],
    unsigned char md[20])
{
	unsigned char md2[20];
	u_int8_t i;

	for (i = 0; i < HMAC_MAX; i++) { 
		carp_hmac_generate(vhe, counter, md2, i);
		if (!timingsafe_bcmp(md, md2, sizeof(md2)))
			return (0);
	}
	return (1);
}

/*
 * process input packet.
 * we have rearranged checks order compared to the rfc,
 * but it seems more efficient this way or not possible otherwise.
 */
void
carp_proto_input(struct mbuf *m, ...)
{
	struct ip *ip = mtod(m, struct ip *);
	struct ifnet *ifp;
	struct carp_softc *sc = NULL;
	struct carp_header *ch;
	int iplen, len, hlen, ismulti;
	va_list ap;

	va_start(ap, m);
	hlen = va_arg(ap, int);
	va_end(ap);

	ifp = if_get(m->m_pkthdr.ph_ifidx);
	if (ifp == NULL) {
		m_freem(m);
		return;
	}

	carpstats.carps_ipackets++;

	if (!carp_opts[CARPCTL_ALLOW]) {
		m_freem(m);
		return;
	}

	ismulti = IN_MULTICAST(ip->ip_dst.s_addr);

	/* check if received on a valid carp interface */
	if (!((ifp->if_type == IFT_CARP && ismulti) ||
	    (ifp->if_type != IFT_CARP && !ismulti && ifp->if_carp != NULL))) {
		carpstats.carps_badif++;
		CARP_LOG(LOG_INFO, sc,
		    ("packet received on non-carp interface: %s",
		     ifp->if_xname));
		m_freem(m);
		return;
	}

	/* verify that the IP TTL is 255.  */
	if (ip->ip_ttl != CARP_DFLTTL) {
		carpstats.carps_badttl++;
		CARP_LOG(LOG_NOTICE, sc, ("received ttl %d != %d on %s",
		    ip->ip_ttl, CARP_DFLTTL, ifp->if_xname));
		m_freem(m);
		return;
	}

	/*
	 * verify that the received packet length is
	 * equal to the CARP header
	 */
	iplen = ip->ip_hl << 2;
	len = iplen + sizeof(*ch);
	if (len > m->m_pkthdr.len) {
		carpstats.carps_badlen++;
		CARP_LOG(LOG_INFO, sc, ("packet too short %d on %s",
		    m->m_pkthdr.len, ifp->if_xname));
		m_freem(m);
		return;
	}

	if ((m = m_pullup(m, len)) == NULL) {
		carpstats.carps_hdrops++;
		return;
	}
	ip = mtod(m, struct ip *);
	ch = (struct carp_header *)(mtod(m, caddr_t) + iplen);

	/* verify the CARP checksum */
	m->m_data += iplen;
	if (carp_cksum(m, len - iplen)) {
		carpstats.carps_badsum++;
		CARP_LOG(LOG_INFO, sc, ("checksum failed on %s",
		    ifp->if_xname));
		m_freem(m);
		return;
	}
	m->m_data -= iplen;

	carp_proto_input_c(m, ch, ismulti, AF_INET);
}

#ifdef INET6
int
carp6_proto_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct ifnet *ifp;
	struct carp_softc *sc = NULL;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct carp_header *ch;
	u_int len;

	ifp = if_get(m->m_pkthdr.ph_ifidx);
	if (ifp == NULL) {
		m_freem(m);
		return (IPPROTO_DONE);
	}

	carpstats.carps_ipackets6++;

	if (!carp_opts[CARPCTL_ALLOW]) {
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* check if received on a valid carp interface */
	if (ifp->if_type != IFT_CARP) {
		carpstats.carps_badif++;
		CARP_LOG(LOG_INFO, sc, ("packet received on non-carp interface: %s",
		    ifp->if_xname));
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* verify that the IP TTL is 255 */
	if (ip6->ip6_hlim != CARP_DFLTTL) {
		carpstats.carps_badttl++;
		CARP_LOG(LOG_NOTICE, sc, ("received ttl %d != %d on %s",
		    ip6->ip6_hlim, CARP_DFLTTL, ifp->if_xname));
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* verify that we have a complete carp packet */
	len = m->m_len;
	if ((m = m_pullup(m, *offp + sizeof(*ch))) == NULL) {
		carpstats.carps_badlen++;
		CARP_LOG(LOG_INFO, sc, ("packet size %u too small", len));
		return (IPPROTO_DONE);
	}
	ch = (struct carp_header *)(mtod(m, caddr_t) + *offp);

	/* verify the CARP checksum */
	m->m_data += *offp;
	if (carp_cksum(m, sizeof(*ch))) {
		carpstats.carps_badsum++;
		CARP_LOG(LOG_INFO, sc, ("checksum failed, on %s",
		    ifp->if_xname));
		m_freem(m);
		return (IPPROTO_DONE);
	}
	m->m_data -= *offp;

	carp_proto_input_c(m, ch, 1, AF_INET6);
	return (IPPROTO_DONE);
}
#endif /* INET6 */

void
carp_proto_input_c(struct mbuf *m, struct carp_header *ch, int ismulti,
    sa_family_t af)
{
	struct ifnet *ifp;
	struct carp_softc *sc;
	struct carp_vhost_entry *vhe;
	struct timeval sc_tv, ch_tv;
	struct carp_if *cif;

	ifp = if_get(m->m_pkthdr.ph_ifidx);
	KASSERT(ifp != NULL);

	if (ifp->if_type == IFT_CARP)
		cif = (struct carp_if *)ifp->if_carpdev->if_carp;
	else
		cif = (struct carp_if *)ifp->if_carp;

	TAILQ_FOREACH(sc, &cif->vhif_vrs, sc_list) {
		if (af == AF_INET &&
		    ismulti != IN_MULTICAST(sc->sc_peer.s_addr))
			continue;
		LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
			if (vhe->vhid == ch->carp_vhid)
				goto found;
		}
	}
 found:

	if (!sc || (sc->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
	    (IFF_UP|IFF_RUNNING)) {
		carpstats.carps_badvhid++;
		m_freem(m);
		return;
	}

	getmicrotime(&sc->sc_if.if_lastchange);
	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	/* verify the CARP version. */
	if (ch->carp_version != CARP_VERSION) {
		carpstats.carps_badver++;
		sc->sc_if.if_ierrors++;
		CARP_LOG(LOG_NOTICE, sc, ("invalid version %d != %d",
		    ch->carp_version, CARP_VERSION));
		m_freem(m);
		return;
	}

	/* verify the hash */
	if (carp_hmac_verify(vhe, ch->carp_counter, ch->carp_md)) {
		carpstats.carps_badauth++;
		sc->sc_if.if_ierrors++;
		CARP_LOG(LOG_INFO, sc, ("incorrect hash"));
		m_freem(m);
		return;
	}

	if (!memcmp(&vhe->vhe_replay_cookie, ch->carp_counter,
	    sizeof(ch->carp_counter))) {
		/* Do not log duplicates from non simplex interfaces */
		if (sc->sc_carpdev->if_flags & IFF_SIMPLEX) {
			carpstats.carps_badauth++;
			sc->sc_if.if_ierrors++;
			CARP_LOG(LOG_WARNING, sc,
			    ("replay or network loop detected"));
		}
		m_freem(m);
		return;
	}

	sc_tv.tv_sec = sc->sc_advbase;
	sc_tv.tv_usec = vhe->advskew * 1000000 / 256;
	ch_tv.tv_sec = ch->carp_advbase;
	ch_tv.tv_usec = ch->carp_advskew * 1000000 / 256;

	switch (vhe->state) {
	case INIT:
		break;
	case MASTER:
		/*
		 * If we receive an advertisement from a master who's going to
		 * be more frequent than us, and whose demote count is not higher
		 * than ours, go into BACKUP state. If his demote count is lower,
		 * also go into BACKUP.
		 */
		if (((timercmp(&sc_tv, &ch_tv, >) ||
		    timercmp(&sc_tv, &ch_tv, ==)) &&
		    (ch->carp_demote <= carp_group_demote_count(sc))) ||
		    ch->carp_demote < carp_group_demote_count(sc)) {
			timeout_del(&vhe->ad_tmo);
			carp_set_state(vhe, BACKUP);
			carp_setrun(vhe, 0);
		}
		break;
	case BACKUP:
		/*
		 * If we're pre-empting masters who advertise slower than us,
		 * and do not have a better demote count, treat them as down.
		 * 
		 */
		if (carp_opts[CARPCTL_PREEMPT] &&
		    timercmp(&sc_tv, &ch_tv, <) &&
		    ch->carp_demote >= carp_group_demote_count(sc)) {
			carp_master_down(vhe);
			break;
		}

		/*
		 * Take over masters advertising with a higher demote count,
		 * regardless of CARPCTL_PREEMPT.
		 */ 
		if (ch->carp_demote > carp_group_demote_count(sc)) {
			carp_master_down(vhe);
			break;
		}

		/*
		 *  If the master is going to advertise at such a low frequency
		 *  that he's guaranteed to time out, we'd might as well just
		 *  treat him as timed out now.
		 */
		sc_tv.tv_sec = sc->sc_advbase * 3;
		if (sc->sc_advbase && timercmp(&sc_tv, &ch_tv, <)) {
			carp_master_down(vhe);
			break;
		}

		/*
		 * Otherwise, we reset the counter and wait for the next
		 * advertisement.
		 */
		carp_setrun(vhe, af);
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

	switch (name[0]) {
	case CARPCTL_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &carpstats, sizeof(carpstats)));
	default:
		if (name[0] <= 0 || name[0] >= CARPCTL_MAXID)
			return (ENOPROTOOPT);
		return sysctl_int(oldp, oldlenp, newp, newlen,
		    &carp_opts[name[0]]);
	}
}

/*
 * Interface side of the CARP implementation.
 */

/* ARGSUSED */
void
carpattach(int n)
{
	struct ifg_group	*ifg;

	if ((ifg = if_creategroup("carp")) != NULL)
		ifg->ifg_refcnt++;	/* keep around even if empty */
	if_clone_attach(&carp_cloner);
}

int
carp_clone_create(struct if_clone *ifc, int unit)
{
	struct carp_softc *sc;
	struct ifnet *ifp;

	sc = malloc(sizeof(*sc), M_DEVBUF, M_NOWAIT|M_ZERO);
	if (!sc)
		return (ENOMEM);

	LIST_INIT(&sc->carp_vhosts);
	sc->sc_vhe_count = 0;
	if (carp_new_vhost(sc, 0, 0)) {
		free(sc, M_DEVBUF, sizeof(*sc));
		return (ENOMEM);
	}

	sc->sc_suppress = 0;
	sc->sc_advbase = CARP_DFLTINTV;
	sc->sc_naddrs = sc->sc_naddrs6 = 0;
#ifdef INET6
	sc->sc_im6o.im6o_hlim = CARP_DFLTTL;
#endif /* INET6 */
	sc->sc_imo.imo_membership = (struct in_multi **)malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
	    M_WAITOK|M_ZERO);
	sc->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;

	LIST_INIT(&sc->carp_mc_listhead);
	ifp = &sc->sc_if;
	ifp->if_softc = sc;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "%s%d", ifc->ifc_name,
	    unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = carp_ioctl;
	ifp->if_start = carp_start;
	IFQ_SET_MAXLEN(&ifp->if_snd, 1);
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);
	ether_ifattach(ifp);
	ifp->if_type = IFT_CARP;
	ifp->if_sadl->sdl_type = IFT_CARP;
	ifp->if_output = carp_output;
	ifp->if_priority = IF_CARP_DEFAULT_PRIORITY;
	ifp->if_link_state = LINK_STATE_INVALID;

	/* Hook carp_addr_updated to cope with address and route changes. */
	sc->ah_cookie = hook_establish(sc->sc_if.if_addrhooks, 0,
	    carp_addr_updated, sc);

	return (0);
}

int
carp_new_vhost(struct carp_softc *sc, int vhid, int advskew)
{
	struct carp_vhost_entry *vhe, *vhe0;

	vhe = malloc(sizeof(*vhe), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (vhe == NULL)
		return (ENOMEM);

	vhe->parent_sc = sc;
	vhe->vhid = vhid;
	vhe->advskew = advskew;
	vhe->state = INIT;
	timeout_set(&vhe->ad_tmo, carp_send_ad, vhe);
	timeout_set(&vhe->md_tmo, carp_master_down, vhe);
	timeout_set(&vhe->md6_tmo, carp_master_down, vhe);

	/* mark the first vhe as leader */
	if (LIST_EMPTY(&sc->carp_vhosts)) {
		vhe->vhe_leader = 1;
		LIST_INSERT_HEAD(&sc->carp_vhosts, vhe, vhost_entries);
		sc->sc_vhe_count = 1;
		return (0);
	}

	LIST_FOREACH(vhe0, &sc->carp_vhosts, vhost_entries)
		if (LIST_NEXT(vhe0, vhost_entries) == NULL)
			break;
	LIST_INSERT_AFTER(vhe0, vhe, vhost_entries);
	sc->sc_vhe_count++;

	return (0);
}

int
carp_clone_destroy(struct ifnet *ifp)
{
	struct carp_softc *sc = ifp->if_softc;

	carpdetach(sc);
	ether_ifdetach(ifp);
	if_detach(ifp);
	carp_destroy_vhosts(ifp->if_softc);
	free(sc->sc_imo.imo_membership, M_IPMOPTS, 0);
	free(sc, M_DEVBUF, sizeof(*sc));

	return (0);
}

void
carp_del_all_timeouts(struct carp_softc *sc)
{
	struct carp_vhost_entry *vhe;

	LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
		timeout_del(&vhe->ad_tmo);
		timeout_del(&vhe->md_tmo);
		timeout_del(&vhe->md6_tmo);
	}
}

void
carpdetach(struct carp_softc *sc)
{
	struct ifnet *ifp;
	struct carp_if *cif;
	int s;

	carp_del_all_timeouts(sc);

	if (sc->sc_demote_cnt)
		carp_group_demote_adj(&sc->sc_if, -sc->sc_demote_cnt, "detach");
	sc->sc_suppress = 0;
	sc->sc_sendad_errors = 0;

	carp_set_state_all(sc, INIT);
	sc->sc_if.if_flags &= ~IFF_UP;
	carp_setrun_all(sc, 0);
	carp_multicast_cleanup(sc);

	if (sc->ah_cookie != NULL)
		hook_disestablish(sc->sc_if.if_addrhooks, sc->ah_cookie);

	ifp = sc->sc_carpdev;
	if (ifp == NULL)
		return;

	s = splnet();
	/* Restore previous input handler. */
	if (--sc->sc_ifih->ifih_refcnt == 0) {
		SLIST_REMOVE(&ifp->if_inputs, sc->sc_ifih, ifih, ifih_next);
		free(sc->sc_ifih, M_DEVBUF, sizeof(*sc->sc_ifih));
	}

	if (sc->lh_cookie != NULL)
		hook_disestablish(ifp->if_linkstatehooks,
		    sc->lh_cookie);
	cif = (struct carp_if *)ifp->if_carp;
	TAILQ_REMOVE(&cif->vhif_vrs, sc, sc_list);
	if (!--cif->vhif_nvrs) {
		ifpromisc(ifp, 0);
		ifp->if_carp = NULL;
		free(cif, M_IFADDR, sizeof(*cif));
	}
	sc->sc_carpdev = NULL;
	splx(s);
}

/* Detach an interface from the carp. */
void
carp_ifdetach(struct ifnet *ifp)
{
	struct carp_softc *sc, *nextsc;
	struct carp_if *cif = (struct carp_if *)ifp->if_carp;

	for (sc = TAILQ_FIRST(&cif->vhif_vrs); sc; sc = nextsc) {
		nextsc = TAILQ_NEXT(sc, sc_list);
		carpdetach(sc);
	}
}

void
carp_destroy_vhosts(struct carp_softc *sc)
{
	/* XXX bow out? */
	struct carp_vhost_entry *vhe, *nvhe;

	for (vhe = LIST_FIRST(&sc->carp_vhosts); vhe != NULL; vhe = nvhe) {
		nvhe = LIST_NEXT(vhe, vhost_entries);
		free(vhe, M_DEVBUF, sizeof(*vhe));
	}
	LIST_INIT(&sc->carp_vhosts);
	sc->sc_vhe_count = 0;
}

int
carp_prepare_ad(struct mbuf *m, struct carp_vhost_entry *vhe,
    struct carp_header *ch)
{
	if (!vhe->vhe_replay_cookie) {
		arc4random_buf(&vhe->vhe_replay_cookie,
		    sizeof(vhe->vhe_replay_cookie));
	}

	bcopy(&vhe->vhe_replay_cookie, ch->carp_counter,
	    sizeof(ch->carp_counter));

	/*
	 * For the time being, do not include the IPv6 linklayer addresses
	 * in the HMAC.
	 */
	carp_hmac_generate(vhe, ch->carp_counter, ch->carp_md, HMAC_NOV6LL);

	return (0);
}

void
carp_send_ad_all(void)
{
	struct ifnet *ifp;
	struct carp_if *cif;
	struct carp_softc *vh;

	if (carp_send_all_recur > 0)
		return;
	++carp_send_all_recur;
	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_carp == NULL || ifp->if_type == IFT_CARP)
			continue;

		cif = (struct carp_if *)ifp->if_carp;
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((vh->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) ==
			    (IFF_UP|IFF_RUNNING)) {
				carp_vhe_send_ad_all(vh);
			}
		}
	}
	--carp_send_all_recur;
}

void
carp_vhe_send_ad_all(struct carp_softc *sc)
{
	struct carp_vhost_entry *vhe;

	LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
		if (vhe->state == MASTER)
			carp_send_ad(vhe);
	}
}

void
carp_send_ad(void *v)
{
	struct carp_header ch;
	struct timeval tv;
	struct carp_vhost_entry *vhe = v;
	struct carp_softc *sc = vhe->parent_sc;
	struct carp_header *ch_ptr;

	struct mbuf *m;
	int error, len, advbase, advskew, s;
	struct ifaddr *ifa;
	struct sockaddr sa;

	if (sc->sc_carpdev == NULL) {
		sc->sc_if.if_oerrors++;
		return;
	}

	s = splsoftnet();

	/* bow out if we've gone to backup (the carp interface is going down) */
	if (sc->sc_bow_out) {
		advbase = 255;
		advskew = 255;
	} else {
		advbase = sc->sc_advbase;
		advskew = vhe->advskew;
		tv.tv_sec = advbase;
		if (advbase == 0 && advskew == 0)
			tv.tv_usec = 1 * 1000000 / 256;
		else
			tv.tv_usec = advskew * 1000000 / 256;
	}

	ch.carp_version = CARP_VERSION;
	ch.carp_type = CARP_ADVERTISEMENT;
	ch.carp_vhid = vhe->vhid;
	ch.carp_demote = carp_group_demote_count(sc) & 0xff;
	ch.carp_advbase = advbase;
	ch.carp_advskew = advskew;
	ch.carp_authlen = 7;	/* XXX DEFINE */
	ch.carp_cksum = 0;

	sc->cur_vhe = vhe; /* we need the vhe later on the output path */

	if (sc->sc_naddrs) {
		struct ip *ip;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			sc->sc_if.if_oerrors++;
			carpstats.carps_onomem++;
			/* XXX maybe less ? */
			goto retry_later;
		}
		len = sizeof(*ip) + sizeof(ch);
		m->m_pkthdr.len = len;
		m->m_pkthdr.ph_ifidx = 0;
		m->m_pkthdr.ph_rtableid = sc->sc_if.if_rdomain;
		m->m_pkthdr.pf.prio = CARP_IFQ_PRIO;
		m->m_len = len;
		MH_ALIGN(m, m->m_len);
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

		memset(&sa, 0, sizeof(sa));
		sa.sa_family = AF_INET;
		/* Prefer addresses on the parent interface as source for AD. */
		ifa = ifaof_ifpforaddr(&sa, sc->sc_carpdev);
		if (ifa == NULL)
			ifa = ifaof_ifpforaddr(&sa, &sc->sc_if);
		KASSERT(ifa != NULL);
		ip->ip_src.s_addr = ifatoia(ifa)->ia_addr.sin_addr.s_addr;
		ip->ip_dst.s_addr = sc->sc_peer.s_addr;
		if (IN_MULTICAST(ip->ip_dst.s_addr))
			m->m_flags |= M_MCAST;

		ch_ptr = (struct carp_header *)(ip + 1);
		bcopy(&ch, ch_ptr, sizeof(ch));
		if (carp_prepare_ad(m, vhe, ch_ptr))
			goto retry_later;

		m->m_data += sizeof(*ip);
		ch_ptr->carp_cksum = carp_cksum(m, len - sizeof(*ip));
		m->m_data -= sizeof(*ip);

		getmicrotime(&sc->sc_if.if_lastchange);
		sc->sc_if.if_opackets++;
		sc->sc_if.if_obytes += len;
		carpstats.carps_opackets++;

		error = ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo,
		    NULL, 0);
		if (error) {
			if (error == ENOBUFS)
				carpstats.carps_onomem++;
			else
				CARP_LOG(LOG_WARNING, sc,
				    ("ip_output failed: %d", error));
			sc->sc_if.if_oerrors++;
			if (sc->sc_sendad_errors < INT_MAX)
				sc->sc_sendad_errors++;
			if (sc->sc_sendad_errors == CARP_SENDAD_MAX_ERRORS(sc))
				carp_group_demote_adj(&sc->sc_if, 1,
				    "> snderrors");
			sc->sc_sendad_success = 0;
		} else {
			if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS(sc)) {
				if (++sc->sc_sendad_success >=
				    CARP_SENDAD_MIN_SUCCESS(sc)) {
					carp_group_demote_adj(&sc->sc_if, -1,
					    "< snderrors");
					sc->sc_sendad_errors = 0;
				}
			} else
				sc->sc_sendad_errors = 0;
		}
		if (vhe->vhe_leader) {
			if (sc->sc_delayed_arp > 0)
				sc->sc_delayed_arp--;
			if (sc->sc_delayed_arp == 0) {
				carp_send_arp(sc);
				sc->sc_delayed_arp = -1;
			}
		}
	}
#ifdef INET6
	if (sc->sc_naddrs6) {
		struct ip6_hdr *ip6;

		MGETHDR(m, M_DONTWAIT, MT_HEADER);
		if (m == NULL) {
			sc->sc_if.if_oerrors++;
			carpstats.carps_onomem++;
			/* XXX maybe less ? */
			goto retry_later;
		}
		len = sizeof(*ip6) + sizeof(ch);
		m->m_pkthdr.len = len;
		m->m_pkthdr.ph_ifidx = 0;
		m->m_pkthdr.pf.prio = CARP_IFQ_PRIO;
		m->m_pkthdr.ph_rtableid = sc->sc_if.if_rdomain;
		m->m_len = len;
		MH_ALIGN(m, m->m_len);
		m->m_flags |= M_MCAST;
		ip6 = mtod(m, struct ip6_hdr *);
		memset(ip6, 0, sizeof(*ip6));
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_hlim = CARP_DFLTTL;
		ip6->ip6_nxt = IPPROTO_CARP;

		/* set the source address */
		memset(&sa, 0, sizeof(sa));
		sa.sa_family = AF_INET6;
		/* Prefer addresses on the parent interface as source for AD. */
		ifa = ifaof_ifpforaddr(&sa, sc->sc_carpdev);
		if (ifa == NULL)
			ifa = ifaof_ifpforaddr(&sa, &sc->sc_if);
		KASSERT(ifa != NULL);
		bcopy(ifatoia6(ifa)->ia_addr.sin6_addr.s6_addr,
		    &ip6->ip6_src, sizeof(struct in6_addr));
		/* set the multicast destination */

		ip6->ip6_dst.s6_addr16[0] = htons(0xff02);
		ip6->ip6_dst.s6_addr16[1] = htons(sc->sc_carpdev->if_index);
		ip6->ip6_dst.s6_addr8[15] = 0x12;

		ch_ptr = (struct carp_header *)(ip6 + 1);
		bcopy(&ch, ch_ptr, sizeof(ch));
		if (carp_prepare_ad(m, vhe, ch_ptr))
			goto retry_later;

		m->m_data += sizeof(*ip6);
		ch_ptr->carp_cksum = carp_cksum(m, len - sizeof(*ip6));
		m->m_data -= sizeof(*ip6);

		getmicrotime(&sc->sc_if.if_lastchange);
		sc->sc_if.if_opackets++;
		sc->sc_if.if_obytes += len;
		carpstats.carps_opackets6++;

		error = ip6_output(m, NULL, NULL, 0, &sc->sc_im6o, NULL, NULL);
		if (error) {
			if (error == ENOBUFS)
				carpstats.carps_onomem++;
			else
				CARP_LOG(LOG_WARNING, sc,
				    ("ip6_output failed: %d", error));
			sc->sc_if.if_oerrors++;
			if (sc->sc_sendad_errors < INT_MAX)
				sc->sc_sendad_errors++;
			if (sc->sc_sendad_errors == CARP_SENDAD_MAX_ERRORS(sc))
				carp_group_demote_adj(&sc->sc_if, 1,
					    "> snd6errors");
			sc->sc_sendad_success = 0;
		} else {
			if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS(sc)) {
				if (++sc->sc_sendad_success >=
				    CARP_SENDAD_MIN_SUCCESS(sc)) {
					carp_group_demote_adj(&sc->sc_if, -1,
					    "< snd6errors");
					sc->sc_sendad_errors = 0;
				}
			} else
				sc->sc_sendad_errors = 0;
		}
	}
#endif /* INET6 */

retry_later:
	sc->cur_vhe = NULL;
	splx(s);
	if (advbase != 255 || advskew != 255)
		timeout_add(&vhe->ad_tmo, tvtohz(&tv));
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
	int s = splsoftnet();

	TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		in = ifatoia(ifa)->ia_addr.sin_addr.s_addr;
		arprequest(&sc->sc_if, &in, &in, sc->sc_ac.ac_enaddr);
		DELAY(1000);	/* XXX */
	}
	splx(s);
}

#ifdef INET6
void
carp_send_na(struct carp_softc *sc)
{
	struct ifaddr *ifa;
	struct in6_addr *in6;
	static struct in6_addr mcast = IN6ADDR_LINKLOCAL_ALLNODES_INIT;
	int s = splsoftnet();

	TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {

		if (ifa->ifa_addr->sa_family != AF_INET6)
			continue;

		in6 = &ifatoia6(ifa)->ia_addr.sin6_addr;
		nd6_na_output(&sc->sc_if, &mcast, in6,
		    ND_NA_FLAG_OVERRIDE |
		    (ip6_forwarding ? ND_NA_FLAG_ROUTER : 0), 1, NULL);
		DELAY(1000);	/* XXX */
	}
	splx(s);
}
#endif /* INET6 */

/*
 * Based on bridge_hash() in if_bridge.c
 */
#define	mix(a,b,c) \
	do {						\
		a -= b; a -= c; a ^= (c >> 13);		\
		b -= c; b -= a; b ^= (a << 8);		\
		c -= a; c -= b; c ^= (b >> 13);		\
		a -= b; a -= c; a ^= (c >> 12);		\
		b -= c; b -= a; b ^= (a << 16);		\
		c -= a; c -= b; c ^= (b >> 5);		\
		a -= b; a -= c; a ^= (c >> 3);		\
		b -= c; b -= a; b ^= (a << 10);		\
		c -= a; c -= b; c ^= (b >> 15);		\
	} while (0)

u_int32_t
carp_hash(struct carp_softc *sc, u_char *src)
{
	u_int32_t a = 0x9e3779b9, b = sc->sc_hashkey[0], c = sc->sc_hashkey[1];

	c += sc->sc_key[3] << 24;
	c += sc->sc_key[2] << 16;
	c += sc->sc_key[1] << 8;
	c += sc->sc_key[0];
	b += src[5] << 8;
	b += src[4];
	a += src[3] << 24;
	a += src[2] << 16;
	a += src[1] << 8;
	a += src[0];

	mix(a, b, c);
	return (c);
}

void
carp_update_lsmask(struct carp_softc *sc)
{
	struct carp_vhost_entry *vhe;
	int count;

	if (!sc->sc_balancing)
		return;

	sc->sc_lsmask = 0;
	count = 0;

	LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
		if (vhe->state == MASTER && count < sizeof(sc->sc_lsmask) * 8)
			sc->sc_lsmask |= 1 << count;
		count++;
	}
	sc->sc_lscount = count;
	CARP_LOG(LOG_DEBUG, sc, ("carp_update_lsmask: %x", sc->sc_lsmask));
}

int
carp_iamatch(struct in_ifaddr *ia, u_char *src, u_int8_t **sha,
    u_int8_t **ether_shost)
{
	struct carp_softc *sc = ia->ia_ifp->if_softc;
	struct carp_vhost_entry *vhe = LIST_FIRST(&sc->carp_vhosts);

	if (sc->sc_balancing == CARP_BAL_ARP) {
		int lshash;
		/*
		 * We use the source MAC address to decide which virtual host
		 * should handle the request. If we're master of that virtual
		 * host, then we respond, otherwise, just drop the arp packet
		 * on the floor.
		 */

		if (sc->sc_lscount == 0) /* just to be safe */
			return (0);
		lshash = carp_hash(sc, src) % sc->sc_lscount;
		if ((1 << lshash) & sc->sc_lsmask) {
			int i = 0;
			LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
				if (i++ == lshash)
					break;
			}
			if (vhe == NULL)
				return (0);
			*sha = vhe->vhe_enaddr;
			return (1);
		}
	} else if (sc->sc_balancing == CARP_BAL_IPSTEALTH ||
	    sc->sc_balancing == CARP_BAL_IP) {
		if (vhe->state == MASTER) {
			*ether_shost = ((struct arpcom *)sc->sc_carpdev)->
			    ac_enaddr;
			return (1);
		}
	} else {
		if (vhe->state == MASTER)
			return (1);
	}

	return (0);
}

#ifdef INET6
int
carp_iamatch6(struct ifnet *ifp, u_char *src, struct sockaddr_dl **sdl)
{
	struct carp_softc *sc = ifp->if_softc;
	struct carp_vhost_entry *vhe = LIST_FIRST(&sc->carp_vhosts);

	if (sc->sc_balancing == CARP_BAL_ARP) {
		int lshash;
		/*
		 * We use the source MAC address to decide which virtual host
		 * should handle the request. If we're master of that virtual
		 * host, then we respond, otherwise, just drop the ndp packet
		 * on the floor.
		 */

		/* can happen if optional src lladdr is not provided */
		if (src == NULL)
			return (0);
		if (sc->sc_lscount == 0) /* just to be safe */
			return (0);
		lshash = carp_hash(sc, src) % sc->sc_lscount;
		if ((1 << lshash) & sc->sc_lsmask) {
			int i = 0;
			LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
				if (i++ == lshash)
					break;
			}
			if (vhe == NULL)
				return (0);
			*sdl = &vhe->vhe_sdl;
			return (1);
		}
	} else {
		if (vhe->state == MASTER)
			return (1);
	}

	return (0);
}
#endif /* INET6 */

struct ifnet *
carp_ourether(void *v, u_int8_t *ena)
{
	struct carp_if *cif = (struct carp_if *)v;
	struct carp_softc *vh;

	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
		struct carp_vhost_entry *vhe;
		if ((vh->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING))
			continue;
		if (vh->sc_balancing == CARP_BAL_ARP) {
			LIST_FOREACH(vhe, &vh->carp_vhosts, vhost_entries)
				if (vhe->state == MASTER &&
				    !memcmp(ena, vhe->vhe_enaddr,
				    ETHER_ADDR_LEN))
					return (&vh->sc_if);
		} else {
			vhe = LIST_FIRST(&vh->carp_vhosts);
			if ((vhe->state == MASTER ||
			    vh->sc_balancing >= CARP_BAL_IP) &&
			    !memcmp(ena, vh->sc_ac.ac_enaddr, ETHER_ADDR_LEN))
				return (&vh->sc_if);
		}
	}
	return (NULL);
}

int
carp_input(struct mbuf *m)
{
	struct carp_softc *sc;
	struct ether_header *eh;
	struct mbuf_list ml = MBUF_LIST_INITIALIZER();
	struct carp_if *cif;
	struct ifnet *ifp0, *ifp;

	ifp0 = if_get(m->m_pkthdr.ph_ifidx);
	KASSERT(ifp0 != NULL);
	if ((ifp0->if_flags & IFF_UP) == 0) {
		m_freem(m);
		return (1);
	}

	eh = mtod(m, struct ether_header *);
	cif = (struct carp_if *)ifp0->if_carp;

	ifp = carp_ourether(cif, eh->ether_dhost);
	if (ifp == NULL && !ETHER_IS_MULTICAST(eh->ether_dhost))
		return (0);

	if (ifp == NULL) {
		struct carp_softc *vh;
		struct mbuf *m0;

		/*
		 * XXX Should really check the list of multicast addresses
		 * for each CARP interface _before_ copying.
		 */
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if (!(vh->sc_if.if_flags & IFF_UP))
				continue;
			m0 = m_copym2(m, 0, M_COPYALL, M_DONTWAIT);
			if (m0 == NULL)
				continue;

			ml_init(&ml);
			ml_enqueue(&ml, m0);

			if_input(&vh->sc_if, &ml);
		}

		return (0);
	}

	/*
	 * Clear mcast if received on a carp IP balanced address.
	 */
	sc = ifp->if_softc;
	if (sc->sc_balancing == CARP_BAL_IP &&
	    ETHER_IS_MULTICAST(eh->ether_dhost))
		*(eh->ether_dhost) &= ~0x01;


	ml_enqueue(&ml, m);

	if_input(ifp, &ml);
	return (1);
}

int
carp_lsdrop(struct mbuf *m, sa_family_t af, u_int32_t *src, u_int32_t *dst)
{
	struct ifnet *ifp;
	struct carp_softc *sc;
	int match;
	u_int32_t fold;

	ifp = if_get(m->m_pkthdr.ph_ifidx);
	KASSERT(ifp != NULL);

	sc = ifp->if_softc;
	if (sc->sc_balancing < CARP_BAL_IP)
		return (0);
	/*
	 * Never drop carp advertisements.
	 * XXX Bad idea to pass all broadcast / multicast traffic?
	 */
	if (m->m_flags & (M_BCAST|M_MCAST))
		return (0);

	fold = src[0] ^ dst[0];
#ifdef INET6
	if (af == AF_INET6) {
		int i;
		for (i = 1; i < 4; i++)
			fold ^= src[i] ^ dst[i];
	}
#endif
	if (sc->sc_lscount == 0) /* just to be safe */
		return (1);
	match = (1 << (ntohl(fold) % sc->sc_lscount)) & sc->sc_lsmask;

	return (!match);
}

void
carp_master_down(void *v)
{
	struct carp_vhost_entry *vhe = v;
	struct carp_softc *sc = vhe->parent_sc;

	switch (vhe->state) {
	case INIT:
		printf("%s: master_down event in INIT state\n",
		    sc->sc_if.if_xname);
		break;
	case MASTER:
		break;
	case BACKUP:
		carp_set_state(vhe, MASTER);
		carp_send_ad(vhe);
		if (sc->sc_balancing == CARP_BAL_NONE && vhe->vhe_leader) {
			carp_send_arp(sc);
			/* Schedule a delayed ARP to deal w/ some L3 switches */
			sc->sc_delayed_arp = 2;
#ifdef INET6
			carp_send_na(sc);
#endif /* INET6 */
		}
		carp_setrun(vhe, 0);
		carpstats.carps_preempt++;
		break;
	}
}

void
carp_setrun_all(struct carp_softc *sc, sa_family_t af)
{
	struct carp_vhost_entry *vhe;
	LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
		carp_setrun(vhe, af);
	}
}

/*
 * When in backup state, af indicates whether to reset the master down timer
 * for v4 or v6. If it's set to zero, reset the ones which are already pending.
 */
void
carp_setrun(struct carp_vhost_entry *vhe, sa_family_t af)
{
	struct timeval tv;
	struct carp_softc *sc = vhe->parent_sc;

	if (sc->sc_carpdev == NULL) {
		sc->sc_if.if_flags &= ~IFF_RUNNING;
		carp_set_state_all(sc, INIT);
		return;
	}

	if (memcmp(((struct arpcom *)sc->sc_carpdev)->ac_enaddr,
	    sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN) == 0)
		sc->sc_realmac = 1;
	else
		sc->sc_realmac = 0;

	if (sc->sc_if.if_flags & IFF_UP && vhe->vhid > 0 &&
	    (sc->sc_naddrs || sc->sc_naddrs6) && !sc->sc_suppress) {
		sc->sc_if.if_flags |= IFF_RUNNING;
	} else {
		sc->sc_if.if_flags &= ~IFF_RUNNING;
		return;
	}

	switch (vhe->state) {
	case INIT:
		carp_set_state(vhe, BACKUP);
		carp_setrun(vhe, 0);
		break;
	case BACKUP:
		timeout_del(&vhe->ad_tmo);
		tv.tv_sec = 3 * sc->sc_advbase;
		if (sc->sc_advbase == 0 && vhe->advskew == 0)
			tv.tv_usec = 3 * 1000000 / 256;
		else if (sc->sc_advbase == 0)
			tv.tv_usec = 3 * vhe->advskew * 1000000 / 256;
		else
			tv.tv_usec = vhe->advskew * 1000000 / 256;
		if (vhe->vhe_leader)
			sc->sc_delayed_arp = -1;
		switch (af) {
		case AF_INET:
			timeout_add(&vhe->md_tmo, tvtohz(&tv));
			break;
#ifdef INET6
		case AF_INET6:
			timeout_add(&vhe->md6_tmo, tvtohz(&tv));
			break;
#endif /* INET6 */
		default:
			if (sc->sc_naddrs)
				timeout_add(&vhe->md_tmo, tvtohz(&tv));
			if (sc->sc_naddrs6)
				timeout_add(&vhe->md6_tmo, tvtohz(&tv));
			break;
		}
		break;
	case MASTER:
		tv.tv_sec = sc->sc_advbase;
		if (sc->sc_advbase == 0 && vhe->advskew == 0)
			tv.tv_usec = 1 * 1000000 / 256;
		else
			tv.tv_usec = vhe->advskew * 1000000 / 256;
		timeout_add(&vhe->ad_tmo, tvtohz(&tv));
		break;
	}
}

void
carp_multicast_cleanup(struct carp_softc *sc)
{
	struct ip_moptions *imo = &sc->sc_imo;
#ifdef INET6
	struct ip6_moptions *im6o = &sc->sc_im6o;
#endif
	u_int16_t n = imo->imo_num_memberships;

	/* Clean up our own multicast memberships */
	while (n-- > 0) {
		if (imo->imo_membership[n] != NULL) {
			in_delmulti(imo->imo_membership[n]);
			imo->imo_membership[n] = NULL;
		}
	}
	imo->imo_num_memberships = 0;
	imo->imo_ifidx = 0;

#ifdef INET6
	while (!LIST_EMPTY(&im6o->im6o_memberships)) {
		struct in6_multi_mship *imm =
		    LIST_FIRST(&im6o->im6o_memberships);

		LIST_REMOVE(imm, i6mm_chain);
		in6_leavegroup(imm);
	}
	im6o->im6o_ifidx = 0;
#endif

	/* And any other multicast memberships */
	carp_ether_purgemulti(sc);
}

int
carp_set_ifp(struct carp_softc *sc, struct ifnet *ifp)
{
	struct carp_if *cif, *ncif = NULL;
	struct carp_softc *vr, *after = NULL;
	int myself = 0, error = 0;
	int s;

	if (ifp == sc->sc_carpdev)
		return (0);

	if ((ifp->if_flags & IFF_MULTICAST) == 0)
		return (EADDRNOTAVAIL);

	if (ifp->if_type == IFT_CARP)
		return (EINVAL);

	if (ifp->if_carp == NULL) {
		ncif = malloc(sizeof(*cif), M_IFADDR, M_NOWAIT|M_ZERO);
		if (ncif == NULL)
			return (ENOBUFS);
		if ((error = ifpromisc(ifp, 1))) {
			free(ncif, M_IFADDR, sizeof(*ncif));
			return (error);
		}

		ncif->vhif_ifp = ifp;
		TAILQ_INIT(&ncif->vhif_vrs);
	} else {
		cif = (struct carp_if *)ifp->if_carp;
		if (carp_check_dup_vhids(sc, cif, NULL))
			return (EINVAL);
	}

	/* Can we share an ifih between multiple carp(4) instances? */
	sc->sc_ifih = SLIST_FIRST(&ifp->if_inputs);
	if (sc->sc_ifih->ifih_input != carp_input) {
		sc->sc_ifih = malloc(sizeof(*sc->sc_ifih), M_DEVBUF, M_NOWAIT);
		if (sc->sc_ifih == NULL) {
			free(ncif, M_IFADDR, sizeof(*ncif));
			return (ENOMEM);
		}
		sc->sc_ifih->ifih_input = carp_input;
		sc->sc_ifih->ifih_refcnt = 0;
	}

	/* detach from old interface */
	if (sc->sc_carpdev != NULL)
		carpdetach(sc);

	/* attach carp interface to physical interface */
	if (ncif != NULL)
		ifp->if_carp = (caddr_t)ncif;
	sc->sc_carpdev = ifp;
	sc->sc_if.if_capabilities = ifp->if_capabilities &
	    IFCAP_CSUM_MASK;
	cif = (struct carp_if *)ifp->if_carp;
	TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list) {
		if (vr == sc)
			myself = 1;
		if (LIST_FIRST(&vr->carp_vhosts)->vhid <
		    LIST_FIRST(&sc->carp_vhosts)->vhid)
			after = vr;
	}

	if (!myself) {
		/* We're trying to keep things in order */
		if (after == NULL) {
			TAILQ_INSERT_TAIL(&cif->vhif_vrs, sc, sc_list);
		} else {
			TAILQ_INSERT_AFTER(&cif->vhif_vrs, after,
			    sc, sc_list);
		}
		cif->vhif_nvrs++;
	}
	if (sc->sc_naddrs || sc->sc_naddrs6)
		sc->sc_if.if_flags |= IFF_UP;
	carp_set_enaddr(sc);

	sc->lh_cookie = hook_establish(ifp->if_linkstatehooks, 1,
	    carp_carpdev_state, ifp);

	s = splnet();
	/* Change input handler of the physical interface. */
	if (++sc->sc_ifih->ifih_refcnt == 1)
		SLIST_INSERT_HEAD(&ifp->if_inputs, sc->sc_ifih, ifih_next);

	carp_carpdev_state(ifp);
	splx(s);

	return (0);
}

void
carp_set_vhe_enaddr(struct carp_vhost_entry *vhe)
{
	struct carp_softc *sc = vhe->parent_sc;

	if (vhe->vhid != 0 && sc->sc_carpdev) {
		if (vhe->vhe_leader && sc->sc_balancing == CARP_BAL_IP)
			vhe->vhe_enaddr[0] = 1;
		else
			vhe->vhe_enaddr[0] = 0;
		vhe->vhe_enaddr[1] = 0;
		vhe->vhe_enaddr[2] = 0x5e;
		vhe->vhe_enaddr[3] = 0;
		vhe->vhe_enaddr[4] = 1;
		vhe->vhe_enaddr[5] = vhe->vhid;

		vhe->vhe_sdl.sdl_family = AF_LINK;
		vhe->vhe_sdl.sdl_alen = ETHER_ADDR_LEN;
		bcopy(vhe->vhe_enaddr, vhe->vhe_sdl.sdl_data, ETHER_ADDR_LEN);
	} else
		memset(vhe->vhe_enaddr, 0, ETHER_ADDR_LEN);
}

void
carp_set_enaddr(struct carp_softc *sc)
{
	struct carp_vhost_entry *vhe;

	LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries)
		carp_set_vhe_enaddr(vhe);

	vhe = LIST_FIRST(&sc->carp_vhosts);

	/*
	 * Use the carp lladdr if the running one isn't manually set.
	 * Only compare static parts of the lladdr.
	 */
	if ((memcmp(sc->sc_ac.ac_enaddr + 1, vhe->vhe_enaddr + 1,
	    ETHER_ADDR_LEN - 2) == 0) ||
	    (!sc->sc_ac.ac_enaddr[0] && !sc->sc_ac.ac_enaddr[1] &&
	    !sc->sc_ac.ac_enaddr[2] && !sc->sc_ac.ac_enaddr[3] &&
	    !sc->sc_ac.ac_enaddr[4] && !sc->sc_ac.ac_enaddr[5]))
		bcopy(vhe->vhe_enaddr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);

	/* Make sure the enaddr has changed before further twiddling. */
	if (memcmp(sc->sc_ac.ac_enaddr, sc->sc_curlladdr, ETHER_ADDR_LEN) != 0) {
		bcopy(sc->sc_ac.ac_enaddr, LLADDR(sc->sc_if.if_sadl),
		    ETHER_ADDR_LEN);
		bcopy(sc->sc_ac.ac_enaddr, sc->sc_curlladdr, ETHER_ADDR_LEN);
#ifdef INET6
		/*
		 * (re)attach a link-local address which matches
		 * our new MAC address.
		 */
		if (sc->sc_naddrs6)
			in6_ifattach_linklocal(&sc->sc_if, NULL);
#endif
		carp_set_state_all(sc, INIT);
		carp_setrun_all(sc, 0);
	}
}

void
carp_addr_updated(void *v)
{
	struct carp_softc *sc = (struct carp_softc *) v;
	struct ifaddr *ifa;
	int new_naddrs = 0, new_naddrs6 = 0;

	TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {
		if (ifa->ifa_addr->sa_family == AF_INET)
			new_naddrs++;
#ifdef INET6
		else if (ifa->ifa_addr->sa_family == AF_INET6)
			new_naddrs6++;
#endif /* INET6 */
	}

	/* We received address changes from if_addrhooks callback */
	if (new_naddrs != sc->sc_naddrs || new_naddrs6 != sc->sc_naddrs6) {
		struct in_addr mc_addr;
		struct in_multi *inm;

		sc->sc_naddrs = new_naddrs;
		sc->sc_naddrs6 = new_naddrs6;

		/* Re-establish multicast membership removed by in_control */
		if (IN_MULTICAST(sc->sc_peer.s_addr)) {
			mc_addr.s_addr = sc->sc_peer.s_addr;
			IN_LOOKUP_MULTI(mc_addr, &sc->sc_if, inm);
			if (inm == NULL) {
				struct in_multi **imm =
				    sc->sc_imo.imo_membership;
				u_int16_t maxmem =
				    sc->sc_imo.imo_max_memberships;

				memset(&sc->sc_imo, 0, sizeof(sc->sc_imo));
				sc->sc_imo.imo_membership = imm;
				sc->sc_imo.imo_max_memberships = maxmem;

				if (sc->sc_carpdev != NULL && sc->sc_naddrs > 0)
					carp_join_multicast(sc);
			}
		}

		if (sc->sc_naddrs == 0 && sc->sc_naddrs6 == 0) {
			sc->sc_if.if_flags &= ~IFF_UP;
			carp_set_state_all(sc, INIT);
		} else
			carp_hmac_prepare(sc);
	}

	carp_setrun_all(sc, 0);
}

int
carp_set_addr(struct carp_softc *sc, struct sockaddr_in *sin)
{
	struct in_addr *in = &sin->sin_addr;
	int error;

	KASSERT(sc->sc_carpdev != NULL);

	/* XXX is this necessary? */
	if (in->s_addr == INADDR_ANY) {
		carp_setrun_all(sc, 0);
		return (0);
	}

	if (sc->sc_naddrs == 0 && (error = carp_join_multicast(sc)) != 0)
		return (error);

	carp_set_state_all(sc, INIT);

	return (0);
}

int
carp_join_multicast(struct carp_softc *sc)
{
	struct ip_moptions *imo = &sc->sc_imo;
	struct in_multi *imm;
	struct in_addr addr;

	if (!IN_MULTICAST(sc->sc_peer.s_addr))
		return (0);

	addr.s_addr = sc->sc_peer.s_addr;
	if ((imm = in_addmulti(&addr, &sc->sc_if)) == NULL)
		return (ENOBUFS);

	imo->imo_membership[0] = imm;
	imo->imo_num_memberships = 1;
	imo->imo_ifidx = sc->sc_if.if_index;
	imo->imo_ttl = CARP_DFLTTL;
	imo->imo_loop = 0;
	return (0);
}


#ifdef INET6
int
carp_set_addr6(struct carp_softc *sc, struct sockaddr_in6 *sin6)
{
	int error;

	KASSERT(sc->sc_carpdev != NULL);

	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		carp_setrun_all(sc, 0);
		return (0);
	}

	if (sc->sc_naddrs6 == 0 && (error = carp_join_multicast6(sc)) != 0)
		return (error);

	carp_set_state_all(sc, INIT);

	return (0);
}

int
carp_join_multicast6(struct carp_softc *sc)
{
	struct in6_multi_mship *imm, *imm2;
	struct ip6_moptions *im6o = &sc->sc_im6o;
	struct sockaddr_in6 addr6;
	int error;

	/* Join IPv6 CARP multicast group */
	memset(&addr6, 0, sizeof(addr6));
	addr6.sin6_family = AF_INET6;
	addr6.sin6_len = sizeof(addr6);
	addr6.sin6_addr.s6_addr16[0] = htons(0xff02);
	addr6.sin6_addr.s6_addr16[1] = htons(sc->sc_if.if_index);
	addr6.sin6_addr.s6_addr8[15] = 0x12;
	if ((imm = in6_joingroup(&sc->sc_if,
	    &addr6.sin6_addr, &error)) == NULL) {
		return (error);
	}
	/* join solicited multicast address */
	memset(&addr6.sin6_addr, 0, sizeof(addr6.sin6_addr));
	addr6.sin6_addr.s6_addr16[0] = htons(0xff02);
	addr6.sin6_addr.s6_addr16[1] = htons(sc->sc_if.if_index);
	addr6.sin6_addr.s6_addr32[1] = 0;
	addr6.sin6_addr.s6_addr32[2] = htonl(1);
	addr6.sin6_addr.s6_addr32[3] = 0;
	addr6.sin6_addr.s6_addr8[12] = 0xff;
	if ((imm2 = in6_joingroup(&sc->sc_if,
	    &addr6.sin6_addr, &error)) == NULL) {
		in6_leavegroup(imm);
		return (error);
	}

	/* apply v6 multicast membership */
	im6o->im6o_ifidx = sc->sc_if.if_index;
	if (imm)
		LIST_INSERT_HEAD(&im6o->im6o_memberships, imm,
		    i6mm_chain);
	if (imm2)
		LIST_INSERT_HEAD(&im6o->im6o_memberships, imm2,
		    i6mm_chain);

	return (0);
}

#endif /* INET6 */

int
carp_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct proc *p = curproc;	/* XXX */
	struct carp_softc *sc = ifp->if_softc;
	struct carp_vhost_entry *vhe;
	struct carpreq carpr;
	struct ifaddr *ifa = (struct ifaddr *)addr;
	struct ifreq *ifr = (struct ifreq *)addr;
	struct ifnet *cdev = NULL;
	int i, error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		if (sc->sc_carpdev == NULL)
			return (EINVAL);

		switch (ifa->ifa_addr->sa_family) {
		case AF_INET:
			sc->sc_if.if_flags |= IFF_UP;
			ifa->ifa_rtrequest = arp_rtrequest;
			error = carp_set_addr(sc, satosin(ifa->ifa_addr));
			break;
#ifdef INET6
		case AF_INET6:
			sc->sc_if.if_flags |= IFF_UP;
			error = carp_set_addr6(sc, satosin6(ifa->ifa_addr));
			break;
#endif /* INET6 */
		default:
			error = EAFNOSUPPORT;
			break;
		}
		break;

	case SIOCSIFFLAGS:
		vhe = LIST_FIRST(&sc->carp_vhosts);
		if (vhe->state != INIT && !(ifr->ifr_flags & IFF_UP)) {
			carp_del_all_timeouts(sc);

			/* we need the interface up to bow out */
			sc->sc_if.if_flags |= IFF_UP;
			sc->sc_bow_out = 1;
			carp_vhe_send_ad_all(sc);
			sc->sc_bow_out = 0;

			sc->sc_if.if_flags &= ~IFF_UP;
			carp_set_state_all(sc, INIT);
			carp_setrun_all(sc, 0);
		} else if (vhe->state == INIT && (ifr->ifr_flags & IFF_UP)) {
			sc->sc_if.if_flags |= IFF_UP;
			carp_setrun_all(sc, 0);
		}
		break;

	case SIOCSVH:
		vhe = LIST_FIRST(&sc->carp_vhosts);
		if ((error = suser(p, 0)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &carpr, sizeof carpr)))
			break;
		error = 1;
		if (carpr.carpr_carpdev[0] != '\0' &&
		    (cdev = ifunit(carpr.carpr_carpdev)) == NULL)
			return (EINVAL);
		if (carpr.carpr_peer.s_addr == 0)
			sc->sc_peer.s_addr = INADDR_CARP_GROUP;
		else
			sc->sc_peer.s_addr = carpr.carpr_peer.s_addr;
		if ((error = carp_set_ifp(sc, cdev)))
			return (error);
		if (vhe->state != INIT && carpr.carpr_state != vhe->state) {
			switch (carpr.carpr_state) {
			case BACKUP:
				timeout_del(&vhe->ad_tmo);
				carp_set_state_all(sc, BACKUP);
				carp_setrun_all(sc, 0);
				break;
			case MASTER:
				LIST_FOREACH(vhe, &sc->carp_vhosts,
				    vhost_entries)
					carp_master_down(vhe);
				break;
			default:
				break;
			}
		}
		if ((error = carp_vhids_ioctl(sc, &carpr)))
			return (error);
		if (carpr.carpr_advbase >= 0) {
			if (carpr.carpr_advbase > 255) {
				error = EINVAL;
				break;
			}
			sc->sc_advbase = carpr.carpr_advbase;
			error--;
		}
		if (memcmp(sc->sc_advskews, carpr.carpr_advskews,
		    sizeof(sc->sc_advskews))) {
			i = 0;
			LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries)
				vhe->advskew = carpr.carpr_advskews[i++];
			bcopy(carpr.carpr_advskews, sc->sc_advskews,
			    sizeof(sc->sc_advskews));
		}
		if (sc->sc_balancing != carpr.carpr_balancing) {
			if (carpr.carpr_balancing > CARP_BAL_MAXID) {
				error = EINVAL;
				break;
			}
			sc->sc_balancing = carpr.carpr_balancing;
			carp_set_enaddr(sc);
			carp_update_lsmask(sc);
		}
		bcopy(carpr.carpr_key, sc->sc_key, sizeof(sc->sc_key));
		if (error > 0)
			error = EINVAL;
		else {
			error = 0;
			carp_hmac_prepare(sc);
			carp_setrun_all(sc, 0);
		}
		break;

	case SIOCGVH:
		memset(&carpr, 0, sizeof(carpr));
		if (sc->sc_carpdev != NULL)
			strlcpy(carpr.carpr_carpdev, sc->sc_carpdev->if_xname,
			    IFNAMSIZ);
		i = 0;
		LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
			carpr.carpr_vhids[i] = vhe->vhid;
			carpr.carpr_advskews[i] = vhe->advskew;
			carpr.carpr_states[i] = vhe->state;
			i++;
		}
		carpr.carpr_advbase = sc->sc_advbase;
		carpr.carpr_balancing = sc->sc_balancing;
		if (suser(p, 0) == 0)
			bcopy(sc->sc_key, carpr.carpr_key,
			    sizeof(carpr.carpr_key));
		carpr.carpr_peer.s_addr = sc->sc_peer.s_addr;
		error = copyout(&carpr, ifr->ifr_data, sizeof(carpr));
		break;

	case SIOCADDMULTI:
		error = carp_ether_addmulti(sc, ifr);
		break;

	case SIOCDELMULTI:
		error = carp_ether_delmulti(sc, ifr);
		break;
	case SIOCAIFGROUP:
	case SIOCDIFGROUP:
		if (sc->sc_demote_cnt)
			carp_ifgroup_ioctl(ifp, cmd, addr);
		break;
	case SIOCSIFGATTR:
		carp_ifgattr_ioctl(ifp, cmd, addr);
		break;
	default:
		error = ENOTTY;
	}

	if (memcmp(sc->sc_ac.ac_enaddr, sc->sc_curlladdr, ETHER_ADDR_LEN) != 0)
		carp_set_enaddr(sc);
	return (error);
}

int
carp_check_dup_vhids(struct carp_softc *sc, struct carp_if *cif,
    struct carpreq *carpr)
{
	struct carp_softc *vr;
	struct carp_vhost_entry *vhe, *vhe0;
	int i;

	TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list) {
		if (vr == sc)
			continue;
		LIST_FOREACH(vhe, &vr->carp_vhosts, vhost_entries) {
			if (carpr) {
				for (i = 0; carpr->carpr_vhids[i]; i++) {
					if (vhe->vhid == carpr->carpr_vhids[i])
						return (EINVAL);
				}
			}
			LIST_FOREACH(vhe0, &sc->carp_vhosts, vhost_entries) {
				if (vhe->vhid == vhe0->vhid)
					return (EINVAL);
			}
		}
	}
	return (0);
}

int
carp_vhids_ioctl(struct carp_softc *sc, struct carpreq *carpr)
{
	int i, j;
	u_int8_t taken_vhids[256];

	if (carpr->carpr_vhids[0] == 0 ||
	    !memcmp(sc->sc_vhids, carpr->carpr_vhids, sizeof(sc->sc_vhids)))
		return (0);

	memset(taken_vhids, 0, sizeof(taken_vhids));
	for (i = 0; carpr->carpr_vhids[i]; i++) {
		if (taken_vhids[carpr->carpr_vhids[i]])
			return (EINVAL);
		taken_vhids[carpr->carpr_vhids[i]] = 1;

		if (sc->sc_carpdev) {
			struct carp_if *cif;
			cif = (struct carp_if *)sc->sc_carpdev->if_carp;
			if (carp_check_dup_vhids(sc, cif, carpr))
				return (EINVAL);
		}
		if (carpr->carpr_advskews[i] >= 255)
			return (EINVAL);
	}
	/* set sane balancing defaults */
	if (i <= 1)
		carpr->carpr_balancing = CARP_BAL_NONE;
	else if (carpr->carpr_balancing == CARP_BAL_NONE &&
	    sc->sc_balancing == CARP_BAL_NONE)
		carpr->carpr_balancing = CARP_BAL_IP;

	/* destroy all */
	carp_del_all_timeouts(sc);
	carp_destroy_vhosts(sc);
	memset(sc->sc_vhids, 0, sizeof(sc->sc_vhids));

	/* sort vhosts list by vhid */
	for (j = 1; j <= 255; j++) {
		for (i = 0; carpr->carpr_vhids[i]; i++) {
			if (carpr->carpr_vhids[i] != j)
				continue;
			if (carp_new_vhost(sc, carpr->carpr_vhids[i],
			    carpr->carpr_advskews[i]))
				return (ENOMEM);
			sc->sc_vhids[i] = carpr->carpr_vhids[i];
			sc->sc_advskews[i] = carpr->carpr_advskews[i];
		}
	}
	carp_set_enaddr(sc);
	carp_set_state_all(sc, INIT);
	return (0);
}

void
carp_ifgroup_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct ifgroupreq *ifgr = (struct ifgroupreq *)addr;
	struct ifg_list	*ifgl;
	int *dm, adj;

	if (!strcmp(ifgr->ifgr_group, IFG_ALL))
		return;
	adj = ((struct carp_softc *)ifp->if_softc)->sc_demote_cnt;
	if (cmd == SIOCDIFGROUP)
		adj = adj * -1;

	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		if (!strcmp(ifgl->ifgl_group->ifg_group, ifgr->ifgr_group)) {
			dm = &ifgl->ifgl_group->ifg_carp_demoted;
			if (*dm + adj >= 0)
				*dm += adj;
			else
				*dm = 0;
		}
}

void
carp_ifgattr_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct ifgroupreq *ifgr = (struct ifgroupreq *)addr;
	struct carp_softc *sc = ifp->if_softc;

	if (ifgr->ifgr_attrib.ifg_carp_demoted > 0 && (sc->sc_if.if_flags &
	    (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING))
		carp_vhe_send_ad_all(sc);
}

void
carp_start(struct ifnet *ifp)
{
	struct carp_softc *sc = ifp->if_softc;
	struct mbuf *m;

	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif /* NBPFILTER > 0 */

		if ((ifp->if_carpdev->if_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING)) {
			IF_DROP(&ifp->if_carpdev->if_snd);
			ifp->if_oerrors++;
			m_freem(m);
			continue;
		}

		/*
		 * Do not leak the multicast address when sending
		 * advertisements in 'ip' and 'ip-stealth' balacing
		 * modes.
		 */
		if (sc->sc_balancing != CARP_BAL_IPSTEALTH &&
		    sc->sc_balancing != CARP_BAL_IP &&
		    (sc->cur_vhe && !sc->cur_vhe->vhe_leader)) {
			struct ether_header *eh;
			uint8_t *esrc;

			eh = mtod(m, struct ether_header *);
			esrc = sc->cur_vhe->vhe_enaddr;
			memcpy(eh->ether_shost, esrc, sizeof(eh->ether_shost));
		}

		if (if_enqueue(ifp->if_carpdev, m)) {
			ifp->if_oerrors++;
			continue;
		}
		ifp->if_opackets++;
	}
}

int
carp_output(struct ifnet *ifp, struct mbuf *m, struct sockaddr *sa,
    struct rtentry *rt)
{
	struct carp_softc *sc = ((struct carp_softc *)ifp->if_softc);
	struct carp_vhost_entry *vhe;

	vhe = sc->cur_vhe ? sc->cur_vhe : LIST_FIRST(&sc->carp_vhosts);

	if ((sc->sc_carpdev == NULL) ||
	    (!sc->sc_balancing && vhe->state != MASTER)) {
		m_freem(m);
		return (ENETUNREACH);
	}

	return (ether_output(ifp, m, sa, rt));
}

void
carp_set_state_all(struct carp_softc *sc, int state)
{
	struct carp_vhost_entry *vhe;

	LIST_FOREACH(vhe, &sc->carp_vhosts, vhost_entries) {
		if (vhe->state == state)
			continue;

		carp_set_state(vhe, state);
	}
}

void
carp_set_state(struct carp_vhost_entry *vhe, int state)
{
	struct carp_softc *sc = vhe->parent_sc;
	static const char *carp_states[] = { CARP_STATES };
	int loglevel;

	KASSERT(vhe->state != state);

	if (vhe->state == INIT || state == INIT)
		loglevel = LOG_WARNING;
	else
		loglevel = LOG_CRIT;

	if (sc->sc_vhe_count > 1)
		CARP_LOG(loglevel, sc,
		    ("state transition (vhid %d): %s -> %s", vhe->vhid,
		    carp_states[vhe->state], carp_states[state]));
	else
		CARP_LOG(loglevel, sc,
		    ("state transition: %s -> %s",
		    carp_states[vhe->state], carp_states[state]));

	vhe->state = state;
	carp_update_lsmask(sc);

	/* only the master vhe creates link state messages */
	if (!vhe->vhe_leader)
		return;

	switch (state) {
	case BACKUP:
		sc->sc_if.if_link_state = LINK_STATE_DOWN;
		break;
	case MASTER:
		sc->sc_if.if_link_state = LINK_STATE_UP;
		break;
	default:
		sc->sc_if.if_link_state = LINK_STATE_INVALID;
		break;
	}
	if_link_state_change(&sc->sc_if);
}

void
carp_group_demote_adj(struct ifnet *ifp, int adj, char *reason)
{
	struct ifg_list	*ifgl;
	int *dm, need_ad;
	struct carp_softc *nil = NULL;

	if (ifp->if_type == IFT_CARP) {
		dm = &((struct carp_softc *)ifp->if_softc)->sc_demote_cnt;
		if (*dm + adj >= 0)
			*dm += adj;
		else
			*dm = 0;
	}

	need_ad = 0;
	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next) {
		if (!strcmp(ifgl->ifgl_group->ifg_group, IFG_ALL))
			continue;
		dm = &ifgl->ifgl_group->ifg_carp_demoted;

		if (*dm + adj >= 0)
			*dm += adj;
		else
			*dm = 0;

		if (adj > 0 && *dm == 1)
			need_ad = 1;
		CARP_LOG(LOG_ERR, nil,
		    ("%s demoted group %s by %d to %d (%s)",
		    ifp->if_xname, ifgl->ifgl_group->ifg_group,
		    adj, *dm, reason));
	}
	if (need_ad)
		carp_send_ad_all();
}

int
carp_group_demote_count(struct carp_softc *sc)
{
	struct ifg_list	*ifgl;
	int count = 0;

	TAILQ_FOREACH(ifgl, &sc->sc_if.if_groups, ifgl_next)
		count += ifgl->ifgl_group->ifg_carp_demoted;

	if (count == 0 && sc->sc_demote_cnt)
		count = sc->sc_demote_cnt;

	return (count > 255 ? 255 : count);
}

void
carp_carpdev_state(void *v)
{
	struct carp_if *cif;
	struct carp_softc *sc;
	struct ifnet *ifp = v;

	if (ifp->if_type == IFT_CARP)
		return;

	cif = (struct carp_if *)ifp->if_carp;

	TAILQ_FOREACH(sc, &cif->vhif_vrs, sc_list) {
		int suppressed = sc->sc_suppress;

		if (sc->sc_carpdev->if_link_state == LINK_STATE_DOWN ||
		    !(sc->sc_carpdev->if_flags & IFF_UP)) {
			sc->sc_if.if_flags &= ~IFF_RUNNING;
			carp_del_all_timeouts(sc);
			carp_set_state_all(sc, INIT);
			sc->sc_suppress = 1;
			carp_setrun_all(sc, 0);
			if (!suppressed)
				carp_group_demote_adj(&sc->sc_if, 1, "carpdev");
		} else if (suppressed) {
			carp_set_state_all(sc, INIT);
			sc->sc_suppress = 0;
			carp_setrun_all(sc, 0);
			carp_group_demote_adj(&sc->sc_if, -1, "carpdev");
		}
	}
}

int
carp_ether_addmulti(struct carp_softc *sc, struct ifreq *ifr)
{
	struct ifnet *ifp;
	struct carp_mc_entry *mc;
	u_int8_t addrlo[ETHER_ADDR_LEN], addrhi[ETHER_ADDR_LEN];
	int error;

	ifp = sc->sc_carpdev;
	if (ifp == NULL)
		return (EINVAL);

	error = ether_addmulti(ifr, (struct arpcom *)&sc->sc_ac);
	if (error != ENETRESET)
		return (error);

	/*
	 * This is new multicast address.  We have to tell parent
	 * about it.  Also, remember this multicast address so that
	 * we can delete them on unconfigure.
	 */
	mc = malloc(sizeof(*mc), M_DEVBUF, M_NOWAIT);
	if (mc == NULL) {
		error = ENOMEM;
		goto alloc_failed;
	}

	/*
	 * As ether_addmulti() returns ENETRESET, following two
	 * statement shouldn't fail.
	 */
	(void)ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi);
	ETHER_LOOKUP_MULTI(addrlo, addrhi, &sc->sc_ac, mc->mc_enm);
	memcpy(&mc->mc_addr, &ifr->ifr_addr, ifr->ifr_addr.sa_len);
	LIST_INSERT_HEAD(&sc->carp_mc_listhead, mc, mc_entries);

	error = (*ifp->if_ioctl)(ifp, SIOCADDMULTI, (caddr_t)ifr);
	if (error != 0)
		goto ioctl_failed;

	return (error);

 ioctl_failed:
	LIST_REMOVE(mc, mc_entries);
	free(mc, M_DEVBUF, sizeof(*mc));
 alloc_failed:
	(void)ether_delmulti(ifr, (struct arpcom *)&sc->sc_ac);

	return (error);
}

int
carp_ether_delmulti(struct carp_softc *sc, struct ifreq *ifr)
{
	struct ifnet *ifp;
	struct ether_multi *enm;
	struct carp_mc_entry *mc;
	u_int8_t addrlo[ETHER_ADDR_LEN], addrhi[ETHER_ADDR_LEN];
	int error;

	ifp = sc->sc_carpdev;
	if (ifp == NULL)
		return (EINVAL);

	/*
	 * Find a key to lookup carp_mc_entry.  We have to do this
	 * before calling ether_delmulti for obvious reason.
	 */
	if ((error = ether_multiaddr(&ifr->ifr_addr, addrlo, addrhi)) != 0)
		return (error);
	ETHER_LOOKUP_MULTI(addrlo, addrhi, &sc->sc_ac, enm);
	if (enm == NULL)
		return (EINVAL);

	LIST_FOREACH(mc, &sc->carp_mc_listhead, mc_entries)
		if (mc->mc_enm == enm)
			break;

	/* We won't delete entries we didn't add */
	if (mc == NULL)
		return (EINVAL);

	error = ether_delmulti(ifr, (struct arpcom *)&sc->sc_ac);
	if (error != ENETRESET)
		return (error);

	/* We no longer use this multicast address.  Tell parent so. */
	error = (*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)ifr);
	if (error == 0) {
		/* And forget about this address. */
		LIST_REMOVE(mc, mc_entries);
		free(mc, M_DEVBUF, sizeof(*mc));
	} else
		(void)ether_addmulti(ifr, (struct arpcom *)&sc->sc_ac);
	return (error);
}

/*
 * Delete any multicast address we have asked to add from parent
 * interface.  Called when the carp is being unconfigured.
 */
void
carp_ether_purgemulti(struct carp_softc *sc)
{
	struct ifnet *ifp = sc->sc_carpdev;		/* Parent. */
	struct carp_mc_entry *mc;
	union {
		struct ifreq ifreq;
		struct {
			char ifr_name[IFNAMSIZ];
			struct sockaddr_storage ifr_ss;
		} ifreq_storage;
	} u;
	struct ifreq *ifr = &u.ifreq;

	if (ifp == NULL)
		return;

	memcpy(ifr->ifr_name, ifp->if_xname, IFNAMSIZ);
	while ((mc = LIST_FIRST(&sc->carp_mc_listhead)) != NULL) {
		memcpy(&ifr->ifr_addr, &mc->mc_addr, mc->mc_addr.ss_len);
		(void)(*ifp->if_ioctl)(ifp, SIOCDELMULTI, (caddr_t)ifr);
		LIST_REMOVE(mc, mc_entries);
		free(mc, M_DEVBUF, sizeof(*mc));
	}
}
