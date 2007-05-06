/*	$OpenBSD: ip_carp.c,v 1.137 2007/05/06 12:20:04 henning Exp $	*/

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
 *	- support for hardware checksum calculations;
 *
 */

#include "ether.h"

#include <sys/param.h>
#include <sys/proc.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/socketvar.h>
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
#include <net/if_dl.h>
#endif

#ifdef INET6
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

struct carp_softc {
	struct arpcom sc_ac;
#define	sc_if		sc_ac.ac_if
#define	sc_carpdev	sc_ac.ac_if.if_carpdev
	void *ah_cookie;
	void *lh_cookie;
	struct ip_moptions sc_imo;
#ifdef INET6
	struct ip6_moptions sc_im6o;
#endif /* INET6 */
	TAILQ_ENTRY(carp_softc) sc_list;

	enum { INIT = 0, BACKUP, MASTER }	sc_state;

	int sc_suppress;
	int sc_bow_out;

	int sc_sendad_errors;
#define CARP_SENDAD_MAX_ERRORS	3
	int sc_sendad_success;
#define CARP_SENDAD_MIN_SUCCESS 3

	char sc_carplladdr[ETHER_ADDR_LEN];
	char sc_curlladdr[ETHER_ADDR_LEN];
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

	SHA1_CTX sc_sha1[HMAC_MAX];
	u_int32_t sc_hashkey[2];
	u_int32_t sc_lsmask;		/* load sharing mask */
	int sc_lscount;			/* # load sharing interfaces (max 32) */

	struct timeout sc_ad_tmo;	/* advertisement timeout */
	struct timeout sc_md_tmo;	/* master down timeout */
	struct timeout sc_md6_tmo;	/* master down timeout */
	int sc_delayed_arp;		/* delayed ARP request countdown */

	LIST_HEAD(__carp_mchead, carp_mc_entry)	carp_mc_listhead;
};

int carp_opts[CARPCTL_MAXID] = { 0, 1, 0, 0, 0 };	/* XXX for now */
struct carpstats carpstats;

struct carp_if {
	TAILQ_HEAD(, carp_softc) vhif_vrs;
	int vhif_nvrs;

	struct ifnet *vhif_ifp;
};

#define	CARP_LOG(sc, s)							\
	if (carp_opts[CARPCTL_LOG]) {					\
		if (sc)							\
			log(LOG_INFO, "%s: ",				\
			    (sc)->sc_if.if_xname);			\
		else							\
			log(LOG_INFO, "carp: ");			\
		addlog s;						\
		addlog("\n");						\
	}

void	carp_hmac_prepare(struct carp_softc *);
void	carp_hmac_prepare_ctx(struct carp_softc *, u_int8_t);
void	carp_hmac_generate(struct carp_softc *, u_int32_t *,
	    unsigned char *, u_int8_t);
int	carp_hmac_verify(struct carp_softc *, u_int32_t *,
	    unsigned char *);
void	carp_setroute(struct carp_softc *, int);
void	carp_proto_input_c(struct mbuf *, struct carp_header *, sa_family_t);
void	carpattach(int);
void	carpdetach(struct carp_softc *);
int	carp_prepare_ad(struct mbuf *, struct carp_softc *,
	    struct carp_header *);
void	carp_send_ad_all(void);
void	carp_send_ad(void *);
void	carp_send_arp(struct carp_softc *);
void	carp_master_down(void *);
int	carp_ioctl(struct ifnet *, u_long, caddr_t);
void	carp_ifgroup_ioctl(struct ifnet *, u_long, caddr_t);
void	carp_ifgattr_ioctl(struct ifnet *, u_long, caddr_t);
void	carp_start(struct ifnet *);
void	carp_setrun(struct carp_softc *, sa_family_t);
void	carp_set_state(struct carp_softc *, int);
int	carp_addrcount(struct carp_if *, struct ifaddr *, int);
enum	{ CARP_COUNT_MASTER, CARP_COUNT_RUNNING, CARP_COUNT_LINK0 };
void	carp_multicast_cleanup(struct carp_softc *);
int	carp_set_ifp(struct carp_softc *, struct ifnet *);
void	carp_set_enaddr(struct carp_softc *);
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
	u_int8_t i;

	for (i=0; i < HMAC_MAX; i++)
		carp_hmac_prepare_ctx(sc, i);
}

void
carp_hmac_prepare_ctx(struct carp_softc *sc, u_int8_t ctx)
{
	u_int8_t version = CARP_VERSION, type = CARP_ADVERTISEMENT;
	u_int8_t vhid = sc->sc_vhid & 0xff;
	SHA1_CTX sha1ctx;
	u_int32_t kmd[5];
	struct ifaddr *ifa;
	int i, found;
	struct in_addr last, cur, in;
#ifdef INET6
	struct in6_addr last6, cur6, in6;
#endif /* INET6 */

	/* compute ipad from key */
	bzero(sc->sc_pad, sizeof(sc->sc_pad));
	bcopy(sc->sc_key, sc->sc_pad, sizeof(sc->sc_key));
	for (i = 0; i < sizeof(sc->sc_pad); i++)
		sc->sc_pad[i] ^= 0x36;

	/* precompute first part of inner hash */
	SHA1Init(&sc->sc_sha1[ctx]);
	SHA1Update(&sc->sc_sha1[ctx], sc->sc_pad, sizeof(sc->sc_pad));
	SHA1Update(&sc->sc_sha1[ctx], (void *)&version, sizeof(version));
	SHA1Update(&sc->sc_sha1[ctx], (void *)&type, sizeof(type));

	/* generate a key for the arpbalance hash, before the vhid is hashed */
	bcopy(&sc->sc_sha1[ctx], &sha1ctx, sizeof(sha1ctx));
	SHA1Final((unsigned char *)kmd, &sha1ctx);
	sc->sc_hashkey[0] = kmd[0] ^ kmd[1];
	sc->sc_hashkey[1] = kmd[2] ^ kmd[3];

	/* the rest of the precomputation */
	if (bcmp(sc->sc_ac.ac_enaddr, &sc->sc_carplladdr, ETHER_ADDR_LEN) != 0)
		SHA1Update(&sc->sc_sha1[ctx], sc->sc_ac.ac_enaddr,
		    ETHER_ADDR_LEN);

	SHA1Update(&sc->sc_sha1[ctx], (void *)&vhid, sizeof(vhid));

	/* Hash the addresses from smallest to largest, not interface order */
#ifdef INET
	cur.s_addr = 0;
	do {
		found = 0;
		last = cur;
		cur.s_addr = 0xffffffff;
		TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {
			in.s_addr = ifatoia(ifa)->ia_addr.sin_addr.s_addr;
			if (ifa->ifa_addr->sa_family == AF_INET &&
			    ntohl(in.s_addr) > ntohl(last.s_addr) &&
			    ntohl(in.s_addr) < ntohl(cur.s_addr)) {
				cur.s_addr = in.s_addr;
				found++;
			}
		}
		if (found)
			SHA1Update(&sc->sc_sha1[ctx],
			    (void *)&cur, sizeof(cur));
	} while (found);
#endif /* INET */
#ifdef INET6
	memset(&cur6, 0x00, sizeof(cur6));
	do {
		found = 0;
		last6 = cur6;
		memset(&cur6, 0xff, sizeof(cur6));
		TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {
			in6 = ifatoia6(ifa)->ia_addr.sin6_addr;
			if (IN6_IS_SCOPE_EMBED(&in6)) {
				if (ctx == HMAC_NOV6LL)
					continue;
				in6.s6_addr16[1] = 0;
			}
			if (ifa->ifa_addr->sa_family == AF_INET6 &&
			    memcmp(&in6, &last6, sizeof(in6)) > 0 &&
			    memcmp(&in6, &cur6, sizeof(in6)) < 0) {
				cur6 = in6;
				found++;
			}
		}
		if (found)
			SHA1Update(&sc->sc_sha1[ctx],
			    (void *)&cur6, sizeof(cur6));
	} while (found);
#endif /* INET6 */

	/* convert ipad to opad */
	for (i = 0; i < sizeof(sc->sc_pad); i++)
		sc->sc_pad[i] ^= 0x36 ^ 0x5c;
}

void
carp_hmac_generate(struct carp_softc *sc, u_int32_t counter[2],
    unsigned char md[20], u_int8_t ctx)
{
	SHA1_CTX sha1ctx;

	/* fetch first half of inner hash */
	bcopy(&sc->sc_sha1[ctx], &sha1ctx, sizeof(sha1ctx));

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
	u_int8_t i;

	for (i=0; i < HMAC_MAX; i++) { 
		carp_hmac_generate(sc, counter, md2, i);
		if (!bcmp(md, md2, sizeof(md2)))
			return (0);
	}
	return (1);
}

void
carp_setroute(struct carp_softc *sc, int cmd)
{
	struct ifaddr *ifa;
	int s;

	/* XXX this mess needs fixing */

	s = splsoftnet();
	TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {
		switch (ifa->ifa_addr->sa_family) {
		case AF_INET: {
			int count = 0, error;
			struct sockaddr sa;
			struct rtentry *rt;
			struct radix_node_head *rnh;
			struct radix_node *rn;
			struct rt_addrinfo info;
			int hr_otherif, nr_ourif;

			/*
			 * Avoid screwing with the routes if there are other
			 * carp interfaces which are master and have the same
			 * address.
			 */
			if (sc->sc_carpdev != NULL &&
			    sc->sc_carpdev->if_carp != NULL) {
				count = carp_addrcount(
				    (struct carp_if *)sc->sc_carpdev->if_carp,
				    ifa, CARP_COUNT_MASTER);
				if ((cmd == RTM_ADD && count != 1) ||
				    (cmd == RTM_DELETE && count != 0))
					continue;
			}

			/* Remove the existing host route, if any */
			bzero(&info, sizeof(info));
			info.rti_info[RTAX_DST] = ifa->ifa_addr;
			info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
			info.rti_flags = RTF_HOST;
			error = rtrequest1(RTM_DELETE, &info, NULL, 0);
			rt_missmsg(RTM_DELETE, &info, info.rti_flags, NULL,
			    error, 0);


			/* Check for our address on another interface */
			/* XXX cries for proper API */
			rnh = rt_gettable(ifa->ifa_addr->sa_family, 0);
			rn = rnh->rnh_matchaddr(ifa->ifa_addr, rnh);
			rt = (struct rtentry *)rn;
			hr_otherif = (rt && rt->rt_ifp != &sc->sc_if &&
			    rt->rt_flags & (RTF_CLONING|RTF_CLONED));

			/* Check for a network route on our interface */
			bcopy(ifa->ifa_addr, &sa, sizeof(sa));
			satosin(&sa)->sin_addr.s_addr = satosin(ifa->ifa_netmask
			    )->sin_addr.s_addr & satosin(&sa)->sin_addr.s_addr;
			rt = (struct rtentry *)rt_lookup(&sa,
			    ifa->ifa_netmask, 0);
			nr_ourif = (rt && rt->rt_ifp == &sc->sc_if);

			switch (cmd) {
			case RTM_ADD:
				if (hr_otherif) {
					ifa->ifa_rtrequest = NULL;
					ifa->ifa_flags &= ~RTF_CLONING;
					bzero(&info, sizeof(info));
					info.rti_info[RTAX_DST] = ifa->ifa_addr;
					info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
					info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
					info.rti_flags = RTF_UP | RTF_HOST;
					error = rtrequest1(RTM_ADD, &info, NULL, 0);
					rt_missmsg(RTM_ADD, &info, info.rti_flags, NULL,
					    error, 0);
				}
				if (!hr_otherif || nr_ourif || !rt) {
					if (nr_ourif && !(rt->rt_flags &
					    RTF_CLONING)) {
						bzero(&info, sizeof(info));
						info.rti_info[RTAX_DST] = &sa;
						info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
						error = rtrequest1(RTM_DELETE, &info, NULL, 0);
						rt_missmsg(RTM_DELETE, &info, info.rti_flags, NULL,
						    error, 0);
					}

					ifa->ifa_rtrequest = arp_rtrequest;
					ifa->ifa_flags |= RTF_CLONING;

					bzero(&info, sizeof(info));
					info.rti_info[RTAX_DST] = ifa->ifa_addr;
					info.rti_info[RTAX_GATEWAY] = ifa->ifa_addr;
					info.rti_info[RTAX_NETMASK] = ifa->ifa_netmask;
					error = rtrequest1(RTM_ADD, &info, NULL, 0);
					if (error == 0)
						ifa->ifa_flags |= IFA_ROUTE;
					rt_missmsg(RTM_ADD, &info, info.rti_flags, NULL,
					    error, 0);
				}
				break;
			case RTM_DELETE:
				break;
			default:
				break;
			}
			break;
		}

#ifdef INET6
		case AF_INET6:
			if (cmd == RTM_ADD)
				in6_ifaddloop(ifa);
			else
				in6_ifremloop(ifa);
			break;
#endif /* INET6 */
		default:
			break;
		}
	}
	splx(s);
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
	struct carp_softc *sc = NULL;
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
	if (m->m_pkthdr.rcvif->if_type != IFT_CARP) {
		carpstats.carps_badif++;
		CARP_LOG(sc, ("packet received on non-carp interface: %s",
		    m->m_pkthdr.rcvif->if_xname));
		m_freem(m);
		return;
	}

	/* verify that the IP TTL is 255.  */
	if (ip->ip_ttl != CARP_DFLTTL) {
		carpstats.carps_badttl++;
		CARP_LOG(sc, ("received ttl %d != %d on %s", ip->ip_ttl,
		    CARP_DFLTTL, m->m_pkthdr.rcvif->if_xname));
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
		CARP_LOG(sc, ("packet too short %d on %s", m->m_pkthdr.len,
		    m->m_pkthdr.rcvif->if_xname));
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
		CARP_LOG(sc, ("checksum failed on %s",
		    m->m_pkthdr.rcvif->if_xname));
		m_freem(m);
		return;
	}
	m->m_data -= iplen;

	carp_proto_input_c(m, ch, AF_INET);
}

#ifdef INET6
int
carp6_proto_input(struct mbuf **mp, int *offp, int proto)
{
	struct mbuf *m = *mp;
	struct carp_softc *sc = NULL;
	struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
	struct carp_header *ch;
	u_int len;

	carpstats.carps_ipackets6++;

	if (!carp_opts[CARPCTL_ALLOW]) {
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* check if received on a valid carp interface */
	if (m->m_pkthdr.rcvif->if_type != IFT_CARP) {
		carpstats.carps_badif++;
		CARP_LOG(sc, ("packet received on non-carp interface: %s",
		    m->m_pkthdr.rcvif->if_xname));
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* verify that the IP TTL is 255 */
	if (ip6->ip6_hlim != CARP_DFLTTL) {
		carpstats.carps_badttl++;
		CARP_LOG(sc, ("received ttl %d != %d on %s", ip6->ip6_hlim,
		    CARP_DFLTTL, m->m_pkthdr.rcvif->if_xname));
		m_freem(m);
		return (IPPROTO_DONE);
	}

	/* verify that we have a complete carp packet */
	len = m->m_len;
	IP6_EXTHDR_GET(ch, struct carp_header *, m, *offp, sizeof(*ch));
	if (ch == NULL) {
		carpstats.carps_badlen++;
		CARP_LOG(sc, ("packet size %u too small", len));
		return (IPPROTO_DONE);
	}


	/* verify the CARP checksum */
	m->m_data += *offp;
	if (carp_cksum(m, sizeof(*ch))) {
		carpstats.carps_badsum++;
		CARP_LOG(sc, ("checksum failed, on %s",
		    m->m_pkthdr.rcvif->if_xname));
		m_freem(m);
		return (IPPROTO_DONE);
	}
	m->m_data -= *offp;

	carp_proto_input_c(m, ch, AF_INET6);
	return (IPPROTO_DONE);
}
#endif /* INET6 */

void
carp_proto_input_c(struct mbuf *m, struct carp_header *ch, sa_family_t af)
{
	struct carp_softc *sc;
	u_int64_t tmp_counter;
	struct timeval sc_tv, ch_tv;

	TAILQ_FOREACH(sc, &((struct carp_if *)
	    m->m_pkthdr.rcvif->if_carpdev->if_carp)->vhif_vrs, sc_list)
		if (sc->sc_vhid == ch->carp_vhid)
			break;

	if (!sc || (sc->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
	    (IFF_UP|IFF_RUNNING)) {
		carpstats.carps_badvhid++;
		m_freem(m);
		return;
	}

	/*
	 * Check if our own advertisement was duplicated
	 * from a non simplex interface.
	 * XXX If there is no address on our physical interface
	 * there is no way to distinguish our ads from the ones
	 * another carp host might have sent us.
	 */
	if ((sc->sc_carpdev->if_flags & IFF_SIMPLEX) == 0) {
		struct sockaddr sa;
		struct ifaddr *ifa;

		bzero(&sa, sizeof(sa));
		sa.sa_family = af;
		ifa = ifaof_ifpforaddr(&sa, sc->sc_carpdev);

		if (ifa && af == AF_INET) {
			struct ip *ip = mtod(m, struct ip *);
			if (ip->ip_src.s_addr ==
			    ifatoia(ifa)->ia_addr.sin_addr.s_addr) {
				m_freem(m);
				return;
			}
		}
#ifdef INET6
		if (ifa && af == AF_INET6) {
			struct ip6_hdr *ip6 = mtod(m, struct ip6_hdr *);
			struct in6_addr in6_src, in6_found;

			in6_src = ip6->ip6_src;
			in6_found = ifatoia6(ifa)->ia_addr.sin6_addr;
			if (IN6_IS_SCOPE_EMBED(&in6_src))
				in6_src.s6_addr16[1] = 0;
			if (IN6_IS_SCOPE_EMBED(&in6_found))
				in6_found.s6_addr16[1] = 0;
			if (IN6_ARE_ADDR_EQUAL(&in6_src, &in6_found)) {
				m_freem(m);
				return;
			}
		}
#endif /* INET6 */
	}

	getmicrotime(&sc->sc_if.if_lastchange);
	sc->sc_if.if_ipackets++;
	sc->sc_if.if_ibytes += m->m_pkthdr.len;

	/* verify the CARP version. */
	if (ch->carp_version != CARP_VERSION) {
		carpstats.carps_badver++;
		sc->sc_if.if_ierrors++;
		CARP_LOG(sc, ("invalid version %d != %d",
		    ch->carp_version, CARP_VERSION));
		m_freem(m);
		return;
	}

	/* verify the hash */
	if (carp_hmac_verify(sc, ch->carp_counter, ch->carp_md)) {
		carpstats.carps_badauth++;
		sc->sc_if.if_ierrors++;
		CARP_LOG(sc, ("incorrect hash"));
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
	if (carp_group_demote_count(sc) && sc->sc_advskew <  240)
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
		    (timercmp(&sc_tv, &ch_tv, ==) &&
		    ch->carp_demote <=
		    (carp_group_demote_count(sc) & 0xff))) {
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
		 * Take over masters advertising with a higher demote count,
		 * regardless of CARPCTL_PREEMPT.
		 */ 
		if (ch->carp_demote > (carp_group_demote_count(sc) & 0xff)) {
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
	struct ifg_group	*ifg;

	if ((ifg = if_creategroup("carp")) != NULL)
		ifg->ifg_refcnt++;	/* keep around even if empty */
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

	LIST_INIT(&sc->carp_mc_listhead);
	ifp = &sc->sc_if;
	ifp->if_softc = sc;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "%s%d", ifc->ifc_name,
	    unit);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = carp_ioctl;
	ifp->if_start = carp_start;
	ifp->if_output = carp_output;
	ifp->if_type = IFT_CARP;
	ifp->if_addrlen = ETHER_ADDR_LEN;
	ifp->if_hdrlen = ETHER_HDR_LEN;
	ifp->if_mtu = ETHERMTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	IFQ_SET_READY(&ifp->if_snd);
	if_attach(ifp);

	if_alloc_sadl(ifp);
	LIST_INIT(&sc->sc_ac.ac_multiaddrs);
#if NBPFILTER > 0
	bpfattach(&ifp->if_bpf, ifp, DLT_EN10MB, ETHER_HDR_LEN);
#endif
	return (0);
}

int
carp_clone_destroy(struct ifnet *ifp)
{
	carpdetach(ifp->if_softc);
	ether_ifdetach(ifp);
	if_detach(ifp);
	free(ifp->if_softc, M_DEVBUF);

	return (0);
}

void
carpdetach(struct carp_softc *sc)
{
	struct carp_if *cif;
	int s;

	timeout_del(&sc->sc_ad_tmo);
	timeout_del(&sc->sc_md_tmo);
	timeout_del(&sc->sc_md6_tmo);

	if (sc->sc_suppress)
		carp_group_demote_adj(&sc->sc_if, -1);
	sc->sc_suppress = 0;

	if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS)
		carp_group_demote_adj(&sc->sc_if, -1);
	sc->sc_sendad_errors = 0;

	carp_set_state(sc, INIT);
	sc->sc_if.if_flags &= ~IFF_UP;
	carp_setrun(sc, 0);
	carp_multicast_cleanup(sc);

	s = splnet();
	if (sc->sc_carpdev != NULL) {
		if (sc->lh_cookie != NULL)
			hook_disestablish(sc->sc_carpdev->if_linkstatehooks,
			    sc->lh_cookie);
		cif = (struct carp_if *)sc->sc_carpdev->if_carp;
		TAILQ_REMOVE(&cif->vhif_vrs, sc, sc_list);
		if (!--cif->vhif_nvrs) {
			ifpromisc(sc->sc_carpdev, 0);
			sc->sc_carpdev->if_carp = NULL;
			FREE(cif, M_IFADDR);
		}
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

int
carp_prepare_ad(struct mbuf *m, struct carp_softc *sc, struct carp_header *ch)
{
	if (sc->sc_init_counter) {
		/* this could also be seconds since unix epoch */
		sc->sc_counter = arc4random();
		sc->sc_counter = sc->sc_counter << 32;
		sc->sc_counter += arc4random();
	} else
		sc->sc_counter++;

	ch->carp_counter[0] = htonl((sc->sc_counter>>32)&0xffffffff);
	ch->carp_counter[1] = htonl(sc->sc_counter&0xffffffff);

	/*
	 * For the time being, do not include the IPv6 linklayer addresses
	 * in the HMAC.
	 */
	carp_hmac_generate(sc, ch->carp_counter, ch->carp_md, HMAC_NOV6LL);

	return (0);
}

void
carp_send_ad_all(void)
{
	struct ifnet *ifp;
	struct carp_if *cif;
	struct carp_softc *vh;

	TAILQ_FOREACH(ifp, &ifnet, if_list) {
		if (ifp->if_carp == NULL || ifp->if_type == IFT_CARP)
			continue;

		cif = (struct carp_if *)ifp->if_carp;
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((vh->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) ==
			    (IFF_UP|IFF_RUNNING) && vh->sc_state == MASTER)
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
	int error, len, advbase, advskew, s;
	struct ifaddr *ifa;
	struct sockaddr sa;

	s = splsoftnet();

	if (sc->sc_carpdev == NULL) {
		sc->sc_if.if_oerrors++;
		goto retry_later;
	}

	/* bow out if we've gone to backup (the carp interface is going down) */
	if (sc->sc_bow_out) {
		sc->sc_bow_out = 0;
		advbase = 255;
		advskew = 255;
	} else {
		advbase = sc->sc_advbase;
		if (!carp_group_demote_count(sc) || sc->sc_advskew > 240)
			advskew = sc->sc_advskew;
		else
			advskew = 240;
		tv.tv_sec = advbase;
		tv.tv_usec = advskew * 1000000 / 256;
	}

	ch.carp_version = CARP_VERSION;
	ch.carp_type = CARP_ADVERTISEMENT;
	ch.carp_vhid = sc->sc_vhid;
	ch.carp_demote = carp_group_demote_count(sc) & 0xff;
	ch.carp_advbase = advbase;
	ch.carp_advskew = advskew;
	ch.carp_authlen = 7;	/* XXX DEFINE */
	ch.carp_cksum = 0;


#ifdef INET
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

		bzero(&sa, sizeof(sa));
		sa.sa_family = AF_INET;
		ifa = ifaof_ifpforaddr(&sa, sc->sc_carpdev);
		if (ifa == NULL)
			ip->ip_src.s_addr = 0;
		else
			ip->ip_src.s_addr =
			    ifatoia(ifa)->ia_addr.sin_addr.s_addr;
		ip->ip_dst.s_addr = INADDR_CARP_GROUP;

		ch_ptr = (void *)ip + sizeof(*ip);
		bcopy(&ch, ch_ptr, sizeof(ch));
		if (carp_prepare_ad(m, sc, ch_ptr))
			goto retry_later;

		m->m_data += sizeof(*ip);
		ch_ptr->carp_cksum = carp_cksum(m, len - sizeof(*ip));
		m->m_data -= sizeof(*ip);

		getmicrotime(&sc->sc_if.if_lastchange);
		sc->sc_if.if_opackets++;
		sc->sc_if.if_obytes += len;
		carpstats.carps_opackets++;

		error = ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo,
		    NULL);
		if (error) {
			if (error == ENOBUFS)
				carpstats.carps_onomem++;
			else
				CARP_LOG(sc, ("ip_output failed: %d", error));
			sc->sc_if.if_oerrors++;
			if (sc->sc_sendad_errors < INT_MAX)
				sc->sc_sendad_errors++;
			if (sc->sc_sendad_errors == CARP_SENDAD_MAX_ERRORS)
				carp_group_demote_adj(&sc->sc_if, 1);
			sc->sc_sendad_success = 0;
		} else {
			if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS) {
				if (++sc->sc_sendad_success >=
				    CARP_SENDAD_MIN_SUCCESS) {
					carp_group_demote_adj(&sc->sc_if, -1);
					sc->sc_sendad_errors = 0;
				}
			} else
				sc->sc_sendad_errors = 0;
		}
		if (sc->sc_delayed_arp > 0)
			sc->sc_delayed_arp--;
		if (sc->sc_delayed_arp == 0) {
			carp_send_arp(sc);
			sc->sc_delayed_arp = -1;
		}
	}
#endif /* INET */
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
		m->m_pkthdr.rcvif = NULL;
		m->m_len = len;
		MH_ALIGN(m, m->m_len);
		m->m_flags |= M_MCAST;
		ip6 = mtod(m, struct ip6_hdr *);
		bzero(ip6, sizeof(*ip6));
		ip6->ip6_vfc |= IPV6_VERSION;
		ip6->ip6_hlim = CARP_DFLTTL;
		ip6->ip6_nxt = IPPROTO_CARP;

		/* set the source address */
		bzero(&sa, sizeof(sa));
		sa.sa_family = AF_INET6;
		ifa = ifaof_ifpforaddr(&sa, sc->sc_carpdev);
		if (ifa == NULL)	/* This should never happen with IPv6 */
			bzero(&ip6->ip6_src, sizeof(struct in6_addr));
		else
			bcopy(ifatoia6(ifa)->ia_addr.sin6_addr.s6_addr,
			    &ip6->ip6_src, sizeof(struct in6_addr));
		/* set the multicast destination */

		ip6->ip6_dst.s6_addr8[0] = 0xff;
		ip6->ip6_dst.s6_addr8[1] = 0x02;
		ip6->ip6_dst.s6_addr8[15] = 0x12;

		ch_ptr = (void *)ip6 + sizeof(*ip6);
		bcopy(&ch, ch_ptr, sizeof(ch));
		if (carp_prepare_ad(m, sc, ch_ptr))
			goto retry_later;

		m->m_data += sizeof(*ip6);
		ch_ptr->carp_cksum = carp_cksum(m, len - sizeof(*ip6));
		m->m_data -= sizeof(*ip6);

		getmicrotime(&sc->sc_if.if_lastchange);
		sc->sc_if.if_opackets++;
		sc->sc_if.if_obytes += len;
		carpstats.carps_opackets6++;

		error = ip6_output(m, NULL, NULL, 0, &sc->sc_im6o, NULL);
		if (error) {
			if (error == ENOBUFS)
				carpstats.carps_onomem++;
			else
				CARP_LOG(sc, ("ip6_output failed: %d", error));
			sc->sc_if.if_oerrors++;
			if (sc->sc_sendad_errors < INT_MAX)
				sc->sc_sendad_errors++;
			if (sc->sc_sendad_errors == CARP_SENDAD_MAX_ERRORS)
				carp_group_demote_adj(&sc->sc_if, 1);
			sc->sc_sendad_success = 0;
		} else {
			if (sc->sc_sendad_errors >= CARP_SENDAD_MAX_ERRORS) {
				if (++sc->sc_sendad_success >=
				    CARP_SENDAD_MIN_SUCCESS) {
					carp_group_demote_adj(&sc->sc_if, -1);
					sc->sc_sendad_errors = 0;
				}
			} else
				sc->sc_sendad_errors = 0;
		}
	}
#endif /* INET6 */

retry_later:
	splx(s);
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
	int s = splsoftnet();

	TAILQ_FOREACH(ifa, &sc->sc_if.if_addrlist, ifa_list) {

		if (ifa->ifa_addr->sa_family != AF_INET)
			continue;

		if (carp_addrcount((struct carp_if *)sc->sc_carpdev->if_carp,
		    ifa, CARP_COUNT_LINK0))
			continue;

		in = ifatoia(ifa)->ia_addr.sin_addr.s_addr;
		arprequest(sc->sc_carpdev, &in, &in, sc->sc_ac.ac_enaddr);
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
		nd6_na_output(sc->sc_carpdev, &mcast, in6,
		    ND_NA_FLAG_OVERRIDE, 1, NULL);
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

int
carp_addrcount(struct carp_if *cif, struct ifaddr *ifa0, int type)
{
	struct carp_softc *vh;
	struct ifaddr *ifa;
	int count = 0;

	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
		switch (type) {
		case CARP_COUNT_RUNNING:
			if ((vh->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
			    (IFF_UP|IFF_RUNNING))
				continue;
			break;
		case CARP_COUNT_MASTER:
			if (vh->sc_state != MASTER)
				continue;
			break;
		case CARP_COUNT_LINK0:
			if (!(vh->sc_if.if_flags & IFF_LINK0) ||
			    (vh->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
			    (IFF_UP|IFF_RUNNING))
				continue;
			break;
		}
		TAILQ_FOREACH(ifa, &vh->sc_if.if_addrlist, ifa_list) {
			if (ifa->ifa_addr->sa_family == AF_INET &&
			    ifa0->ifa_addr->sa_family == AF_INET &&
			    ifatoia(ifa0)->ia_addr.sin_addr.s_addr ==
			    ifatoia(ifa)->ia_addr.sin_addr.s_addr)
				count++;
#ifdef INET6
			if (ifa->ifa_addr->sa_family == AF_INET6 &&
			    ifa0->ifa_addr->sa_family == AF_INET6 &&
			    IN6_ARE_ADDR_EQUAL(IFA_IN6(ifa0), IFA_IN6(ifa)))
				count++;
#endif
		}
	}
	return (count);
}

void
carp_update_lsmask(struct carp_softc *sc)
{
	struct carp_softc *curvh, *vh, *sc0 = NULL;
	struct carp_if *cif;
	struct ifaddr *ifa, *ifa0 = NULL;
	int cur, last, count, found;

	if (!sc->sc_carpdev)
		return;
	cif = (struct carp_if *)sc->sc_carpdev->if_carp;

	/*
	 * Take the first IPv4 address from the LINK0 carp interface
	 * to determine the load sharing group.
	 * Fallback on the first IPv6 address.
	 */
	TAILQ_FOREACH(sc0, &cif->vhif_vrs, sc_list)
		if (sc0->sc_if.if_flags & IFF_LINK0)
			break;
	if (sc0 == NULL)
		return;

	TAILQ_FOREACH(ifa0, &sc0->sc_if.if_addrlist, ifa_list)
		if (ifa0->ifa_addr->sa_family == AF_INET)
			break;
#ifdef INET6
	if (ifa0 == NULL)
		TAILQ_FOREACH(ifa0, &sc0->sc_if.if_addrlist, ifa_list)
			if (ifa0->ifa_addr->sa_family == AF_INET6 &&
			    !IN6_IS_ADDR_LINKLOCAL(IFA_IN6(ifa0)))
				break;
#endif
	if (ifa0 == NULL)
		return;
	/*
	 * Calculate the load sharing mask w/ all carp interfaces
	 * that share the first address of the LINK0 interface.
	 * Sort by virtual host ID.
	 */
	sc0->sc_lsmask = 0;
	cur = 0;
	curvh = NULL;
	count = 0;
	do {
		found = 0;
		last = cur;
		cur = 255;
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			if ((vh->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
			    (IFF_UP|IFF_RUNNING))
				continue;
			TAILQ_FOREACH(ifa, &vh->sc_if.if_addrlist, ifa_list) {
				if (ifa->ifa_addr->sa_family == AF_INET &&
				    ifa0->ifa_addr->sa_family == AF_INET &&
				    ifatoia(ifa0)->ia_addr.sin_addr.s_addr ==
				    ifatoia(ifa)->ia_addr.sin_addr.s_addr)
					break;
#ifdef INET6
				if (ifa->ifa_addr->sa_family == AF_INET6 &&
				    ifa0->ifa_addr->sa_family == AF_INET6 &&
				    IN6_ARE_ADDR_EQUAL(IFA_IN6(ifa0), IFA_IN6(ifa)))
					break;
#endif
			}
			if (ifa && vh->sc_vhid > last && vh->sc_vhid < cur) {
				cur = vh->sc_vhid;
				curvh = vh;
				found++;
			}
		}
		if (found) {
			if (curvh->sc_state == MASTER &&
			    count < sizeof(sc0->sc_lsmask) * 8)
				sc0->sc_lsmask |= 1 << count;
			count++;
		}
	} while (found);

	sc0->sc_lscount = count;
	if (count == 0)
		return;

	CARP_LOG(sc, ("carp_update_lsmask: %x", sc0->sc_lsmask))
}

int
carp_iamatch(struct in_ifaddr *ia, u_char *src,
    u_int32_t *count, u_int32_t index)
{
	struct carp_softc *sc = ia->ia_ifp->if_softc;

	/*
	 * If the asked address is found on a LINK0 interface
	 * don't answer the arp reply unless we are MASTER on it.
	 */
	if (!(sc->sc_if.if_flags & IFF_LINK0) && sc->sc_carpdev &&
	    carp_addrcount((struct carp_if *)sc->sc_carpdev->if_carp,
	    (struct ifaddr *)ia, CARP_COUNT_LINK0))
		return (0);

	if (carp_opts[CARPCTL_ARPBALANCE]) {
		/*
		 * We use the source ip to decide which virtual host should
		 * handle the request. If we're master of that virtual host,
		 * then we respond, otherwise, just drop the arp packet on
		 * the floor.
		 */

		/* Count the eligible carp interfaces with this address */
		if (*count == 0)
			*count = carp_addrcount(
			    (struct carp_if *)ia->ia_ifp->if_carpdev->if_carp,
			    (struct ifaddr *)ia, CARP_COUNT_RUNNING);

		/* This should never happen, but... */
		if (*count == 0)
			return (0);

		if (carp_hash(sc, src) % *count == index - 1 &&
		    sc->sc_state == MASTER) {
			return (1);
		}
	} else {
		if (sc->sc_state == MASTER)
			return (1);
	}

	return (0);
}

#ifdef INET6
int
carp_iamatch6(struct ifnet *ifp, struct ifaddr *ifa)
{
	struct carp_softc *sc = ifp->if_softc;

	/*
	 * If the asked address is found on a LINK0 interface
	 * don't answer the arp request unless we are MASTER on it.
	 */
	if (!(sc->sc_if.if_flags & IFF_LINK0) && sc->sc_carpdev &&
	    carp_addrcount((struct carp_if *)sc->sc_carpdev->if_carp,
	    ifa, CARP_COUNT_LINK0))
		return (0);

	if (sc->sc_state == MASTER)
		return (1);

	return (0);
}
#endif /* INET6 */

struct ifnet *
carp_ourether(void *v, struct ether_header *eh, u_char iftype, int src)
{
	struct carp_if *cif = (struct carp_if *)v;
	struct carp_softc *vh;
	u_int8_t *ena;

	if (src)
		ena = (u_int8_t *)&eh->ether_shost;
	else
		ena = (u_int8_t *)&eh->ether_dhost;

	TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
		if ((vh->sc_if.if_flags & (IFF_UP|IFF_RUNNING)) !=
		    (IFF_UP|IFF_RUNNING))
			continue;
		if ((vh->sc_state == MASTER || vh->sc_if.if_flags & IFF_LINK0)
		    && !bcmp(ena, vh->sc_ac.ac_enaddr, ETHER_ADDR_LEN))
			return (&vh->sc_if);
	}
	return (NULL);
}

int
carp_input(struct mbuf *m, u_int8_t *shost, u_int8_t *dhost, u_int16_t etype)
{
	struct ether_header eh;
	struct carp_if *cif = (struct carp_if *)m->m_pkthdr.rcvif->if_carp;
	struct ifnet *ifp;

	bcopy(shost, &eh.ether_shost, sizeof(eh.ether_shost));
	bcopy(dhost, &eh.ether_dhost, sizeof(eh.ether_dhost));
	eh.ether_type = etype;

	if ((ifp = carp_ourether(cif, &eh, m->m_pkthdr.rcvif->if_type, 0)))
		;
	else if (m->m_flags & (M_BCAST|M_MCAST)) {
		struct carp_softc *vh;
		struct mbuf *m0;

		/*
		 * XXX Should really check the list of multicast addresses
		 * for each CARP interface _before_ copying.
		 */
		TAILQ_FOREACH(vh, &cif->vhif_vrs, sc_list) {
			m0 = m_copym2(m, 0, M_COPYALL, M_DONTWAIT);
			if (m0 == NULL)
				continue;
			m0->m_pkthdr.rcvif = &vh->sc_if;
			ether_input(&vh->sc_if, &eh, m0);
		}
		return (1);
	}

	if (ifp == NULL)
		return (1);

	m->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap_hdr(ifp->if_bpf, (char *)&eh, ETHER_HDR_LEN, m,
		    BPF_DIRECTION_IN);
#endif
	ifp->if_ipackets++;
	ether_input(ifp, &eh, m);

	return (0);
}

int
carp_lsdrop(struct mbuf *m, sa_family_t af, u_int32_t *src, u_int32_t *dst)
{
	struct carp_softc *sc = m->m_pkthdr.rcvif->if_softc;
	int match;
	u_int32_t fold;

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
	struct carp_softc *sc = v;

	switch (sc->sc_state) {
	case INIT:
		printf("%s: master_down event in INIT state\n",
		    sc->sc_if.if_xname);
		break;
	case MASTER:
		break;
	case BACKUP:
		carp_set_state(sc, MASTER);
		carp_send_ad(sc);
		carp_send_arp(sc);
		/* Schedule a delayed ARP request to deal w/ some L3 switches */
		sc->sc_delayed_arp = 2;
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

	if (sc->sc_carpdev == NULL) {
		sc->sc_if.if_flags &= ~IFF_RUNNING;
		carp_set_state(sc, INIT);
		return;
	}

	if (sc->sc_if.if_flags & IFF_UP && sc->sc_vhid > 0 &&
	    (sc->sc_naddrs || sc->sc_naddrs6) && !sc->sc_suppress) {
		sc->sc_if.if_flags |= IFF_RUNNING;
	} else {
		sc->sc_if.if_flags &= ~IFF_RUNNING;
		carp_setroute(sc, RTM_DELETE);
		return;
	}

	switch (sc->sc_state) {
	case INIT:
		carp_set_state(sc, BACKUP);
		carp_setroute(sc, RTM_DELETE);
		carp_setrun(sc, 0);
		break;
	case BACKUP:
		timeout_del(&sc->sc_ad_tmo);
		tv.tv_sec = 3 * sc->sc_advbase;
		tv.tv_usec = sc->sc_advskew * 1000000 / 256;
		sc->sc_delayed_arp = -1;
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
	imo->imo_multicast_ifp = NULL;

#ifdef INET6
	while (!LIST_EMPTY(&im6o->im6o_memberships)) {
		struct in6_multi_mship *imm =
		    LIST_FIRST(&im6o->im6o_memberships);

		LIST_REMOVE(imm, i6mm_chain);
		in6_leavegroup(imm);
	}
	im6o->im6o_multicast_ifp = NULL;
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

	if (ifp != NULL) {
		if ((ifp->if_flags & IFF_MULTICAST) == 0)
			return (EADDRNOTAVAIL);

		if (ifp->if_type == IFT_CARP)
			return (EINVAL);

		if (ifp->if_carp == NULL) {
			MALLOC(ncif, struct carp_if *, sizeof(*cif),
			    M_IFADDR, M_NOWAIT);
			if (ncif == NULL)
				return (ENOBUFS);
			if ((error = ifpromisc(ifp, 1))) {
				FREE(ncif, M_IFADDR);
				return (error);
			}

			ncif->vhif_ifp = ifp;
			TAILQ_INIT(&ncif->vhif_vrs);
		} else {
			cif = (struct carp_if *)ifp->if_carp;
			TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
				if (vr != sc && vr->sc_vhid == sc->sc_vhid)
					return (EINVAL);
		}

		/* detach from old interface */
		if (sc->sc_carpdev != NULL)
			carpdetach(sc);

		/* join multicast groups */
		if (sc->sc_naddrs < 0 &&
		    (error = carp_join_multicast(sc)) != 0) {
			if (ncif != NULL)
				FREE(ncif, M_IFADDR);
			return (error);
		}

#ifdef INET6
		if (sc->sc_naddrs6 < 0 &&
		    (error = carp_join_multicast6(sc)) != 0) {
			if (ncif != NULL)
				FREE(ncif, M_IFADDR);
			carp_multicast_cleanup(sc);
			return (error);
		}
#endif

		/* attach carp interface to physical interface */
		if (ncif != NULL)
			ifp->if_carp = (caddr_t)ncif;
		sc->sc_carpdev = ifp;
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
				TAILQ_INSERT_AFTER(&cif->vhif_vrs, after,
				    sc, sc_list);
			}
			cif->vhif_nvrs++;
		}
		if (sc->sc_naddrs || sc->sc_naddrs6)
			sc->sc_if.if_flags |= IFF_UP;
		carp_set_enaddr(sc);
		s = splnet();
		sc->lh_cookie = hook_establish(ifp->if_linkstatehooks, 1,
		    carp_carpdev_state, ifp);
		carp_carpdev_state(ifp);
		splx(s);
	} else {
		carpdetach(sc);
		sc->sc_if.if_flags &= ~(IFF_UP|IFF_RUNNING);
	}
	return (0);
}

void
carp_set_enaddr(struct carp_softc *sc)
{
	if (sc->sc_vhid != -1 && sc->sc_carpdev) {
		/* XXX detach ipv6 link-local address? */
		if (sc->sc_carpdev->if_type == IFT_ISO88025) {
			sc->sc_carplladdr[0] = 3;
			sc->sc_carplladdr[1] = 0;
			sc->sc_carplladdr[2] = 0x40 >> (sc->sc_vhid - 1);
			sc->sc_carplladdr[3] = 0x40000 >> (sc->sc_vhid - 1);
			sc->sc_carplladdr[4] = 0;
			sc->sc_carplladdr[5] = 0;
		} else {
			if (sc->sc_if.if_flags & IFF_LINK2)
				sc->sc_carplladdr[0] = 1;
			else
				sc->sc_carplladdr[0] = 0;
			sc->sc_carplladdr[1] = 0;
			sc->sc_carplladdr[2] = 0x5e;
			sc->sc_carplladdr[3] = 0;
			sc->sc_carplladdr[4] = 1;
			sc->sc_carplladdr[5] = sc->sc_vhid;
		}
	} else
		bzero(&sc->sc_carplladdr, ETHER_ADDR_LEN);

	/*
	 * Use the carp lladdr if the running one isn't manually set.
	 * Only compare static parts of the lladdr.
	 */
	if ((bcmp(sc->sc_ac.ac_enaddr + 1, sc->sc_carplladdr + 1,
	    ETHER_ADDR_LEN - 2) == 0) ||
	    (!sc->sc_ac.ac_enaddr[0] && !sc->sc_ac.ac_enaddr[1] &&
	    !sc->sc_ac.ac_enaddr[2] && !sc->sc_ac.ac_enaddr[3] &&
	    !sc->sc_ac.ac_enaddr[4] && !sc->sc_ac.ac_enaddr[5]))
		bcopy(sc->sc_carplladdr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);

	/* Make sure the enaddr has changed before further twiddling. */
	if (bcmp(sc->sc_ac.ac_enaddr, sc->sc_curlladdr, ETHER_ADDR_LEN) != 0) {
		bcopy(sc->sc_ac.ac_enaddr, LLADDR(sc->sc_if.if_sadl),
		    ETHER_ADDR_LEN);
		bcopy(sc->sc_ac.ac_enaddr, sc->sc_curlladdr, ETHER_ADDR_LEN);
#ifdef INET6
		/*
		 * (re)attach a link-local address which matches
		 * our new MAC address.
		 */
		in6_ifattach_linklocal(&sc->sc_if, NULL);
#endif
		carp_set_state(sc, INIT);
		carp_setrun(sc, 0);
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
		else if (ifa->ifa_addr->sa_family == AF_INET6 &&
		    !IN6_IS_ADDR_LINKLOCAL(&ifatoia6(ifa)->ia_addr.sin6_addr))
				new_naddrs6++;
	}

	/* Handle a callback after SIOCDIFADDR */
	if (new_naddrs < sc->sc_naddrs || new_naddrs6 < sc->sc_naddrs6) {
		struct in_addr mc_addr;
		struct in_multi *inm;

		sc->sc_naddrs = new_naddrs;
		sc->sc_naddrs6 = new_naddrs6;

		/* Re-establish multicast membership removed by in_control */
		mc_addr.s_addr = INADDR_CARP_GROUP;
		IN_LOOKUP_MULTI(mc_addr, &sc->sc_if, inm);
		if (inm == NULL) {
			bzero(&sc->sc_imo, sizeof(sc->sc_imo));

			if (sc->sc_carpdev != NULL && sc->sc_naddrs > 0)
				carp_join_multicast(sc);
		}

		if (sc->sc_naddrs == 0 && sc->sc_naddrs6 == 0) {
			sc->sc_if.if_flags &= ~IFF_UP;
			carp_set_state(sc, INIT);
		} else
			carp_hmac_prepare(sc);
	}

	carp_setrun(sc, 0);
}

int
carp_set_addr(struct carp_softc *sc, struct sockaddr_in *sin)
{
	struct ifnet *ifp = sc->sc_carpdev;
	struct in_ifaddr *ia, *ia_if;
	int error = 0;

	if (sin->sin_addr.s_addr == 0) {
		if (!(sc->sc_if.if_flags & IFF_UP))
			carp_set_state(sc, INIT);
		if (sc->sc_naddrs)
			sc->sc_if.if_flags |= IFF_UP;
		carp_setrun(sc, 0);
		return (0);
	}

	/* we have to do this by hand to ensure we don't match on ourselves */
	ia_if = NULL;
	for (ia = TAILQ_FIRST(&in_ifaddr); ia;
	    ia = TAILQ_NEXT(ia, ia_list)) {

		/* and, yeah, we need a multicast-capable iface too */
		if (ia->ia_ifp != &sc->sc_if &&
		    ia->ia_ifp->if_type != IFT_CARP &&
		    (ia->ia_ifp->if_flags & IFF_MULTICAST) &&
		    (sin->sin_addr.s_addr & ia->ia_subnetmask) ==
		    ia->ia_subnet) {
			if (!ia_if)
				ia_if = ia;
		}
	}

	if (ia_if) {
		ia = ia_if;
		if (ifp) {
			if (ifp != ia->ia_ifp)
				return (EADDRNOTAVAIL);
		} else {
			ifp = ia->ia_ifp;
		}
	}

	if ((error = carp_set_ifp(sc, ifp)))
		return (error);

	if (sc->sc_carpdev == NULL)
		return (EADDRNOTAVAIL);

	if (sc->sc_naddrs == 0 && (error = carp_join_multicast(sc)) != 0)
		return (error);

	sc->sc_naddrs++;
	if (sc->sc_carpdev != NULL)
		sc->sc_if.if_flags |= IFF_UP;

	carp_set_state(sc, INIT);

	/*
	 * Hook if_addrhooks so that we get a callback after in_ifinit has run,
	 * to correct any inappropriate routes that it inserted.
	 */
	if (sc->ah_cookie == NULL)
		sc->ah_cookie = hook_establish(sc->sc_if.if_addrhooks, 0,
		    carp_addr_updated, sc);

	return (0);
}

int
carp_join_multicast(struct carp_softc *sc)
{
	struct ip_moptions *imo = &sc->sc_imo, tmpimo;
	struct in_addr addr;

	bzero(&tmpimo, sizeof(tmpimo));
	addr.s_addr = INADDR_CARP_GROUP;
	if ((tmpimo.imo_membership[0] =
	    in_addmulti(&addr, &sc->sc_if)) == NULL) {
		return (ENOBUFS);
	}

	imo->imo_membership[0] = tmpimo.imo_membership[0];
	imo->imo_num_memberships = 1;
	imo->imo_multicast_ifp = &sc->sc_if;
	imo->imo_multicast_ttl = CARP_DFLTTL;
	imo->imo_multicast_loop = 0;
	return (0);
}


#ifdef INET6
int
carp_set_addr6(struct carp_softc *sc, struct sockaddr_in6 *sin6)
{
	struct ifnet *ifp = sc->sc_carpdev;
	struct in6_ifaddr *ia, *ia_if;
	int error = 0;

	if (IN6_IS_ADDR_UNSPECIFIED(&sin6->sin6_addr)) {
		if (!(sc->sc_if.if_flags & IFF_UP))
			carp_set_state(sc, INIT);
		if (sc->sc_naddrs6)
			sc->sc_if.if_flags |= IFF_UP;
		carp_setrun(sc, 0);
		return (0);
	}

	/* we have to do this by hand to ensure we don't match on ourselves */
	ia_if = NULL;
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
		if (ia->ia_ifp != &sc->sc_if &&
		    ia->ia_ifp->if_type != IFT_CARP &&
		    (ia->ia_ifp->if_flags & IFF_MULTICAST) &&
		    (i == 4)) {
			if (!ia_if)
				ia_if = ia;
		}
	}

	if (ia_if) {
		ia = ia_if;
		if (sc->sc_carpdev) {
			if (sc->sc_carpdev != ia->ia_ifp)
				return (EADDRNOTAVAIL);
		} else {
			ifp = ia->ia_ifp;
		}
	}

	if ((error = carp_set_ifp(sc, ifp)))
		return (error);

	if (sc->sc_carpdev == NULL)
		return (EADDRNOTAVAIL);

	if (sc->sc_naddrs6 == 0 && (error = carp_join_multicast6(sc)) != 0)
		return (error);

	if (!IN6_IS_ADDR_LINKLOCAL(&sin6->sin6_addr))
		sc->sc_naddrs6++;
	if (sc->sc_carpdev != NULL && sc->sc_naddrs6)
		sc->sc_if.if_flags |= IFF_UP;
	carp_set_state(sc, INIT);
	carp_setrun(sc, 0);

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
	bzero(&addr6, sizeof(addr6));
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
	bzero(&addr6.sin6_addr, sizeof(addr6.sin6_addr));
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
	im6o->im6o_multicast_ifp = &sc->sc_if;
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
	struct carp_softc *sc = ifp->if_softc, *vr;
	struct carpreq carpr;
	struct ifaddr *ifa = (struct ifaddr *)addr;
	struct ifreq *ifr = (struct ifreq *)addr;
	struct ifnet *cdev = NULL;
	int error = 0;

	switch (cmd) {
	case SIOCSIFADDR:
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			sc->sc_if.if_flags |= IFF_UP;
			bcopy(ifa->ifa_addr, ifa->ifa_dstaddr,
			    sizeof(struct sockaddr));
			error = carp_set_addr(sc, satosin(ifa->ifa_addr));
			break;
#endif /* INET */
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
		if (sc->sc_state != INIT && !(ifr->ifr_flags & IFF_UP)) {
			timeout_del(&sc->sc_ad_tmo);
			timeout_del(&sc->sc_md_tmo);
			timeout_del(&sc->sc_md6_tmo);
			if (sc->sc_state == MASTER) {
				/* we need the interface up to bow out */
				sc->sc_if.if_flags |= IFF_UP;
				sc->sc_bow_out = 1;
				carp_send_ad(sc);
			}
			sc->sc_if.if_flags &= ~IFF_UP;
			carp_set_state(sc, INIT);
			carp_setrun(sc, 0);
		} else if (sc->sc_state == INIT && (ifr->ifr_flags & IFF_UP)) {
			sc->sc_if.if_flags |= IFF_UP;
			carp_setrun(sc, 0);
		}
		carp_set_enaddr(sc); /* for changes on LINK2 */
		if (ifr->ifr_flags & IFF_LINK0)
			carp_update_lsmask(sc);
		break;

	case SIOCSVH:
		if ((error = suser(p, p->p_acflag)) != 0)
			break;
		if ((error = copyin(ifr->ifr_data, &carpr, sizeof carpr)))
			break;
		error = 1;
		if (carpr.carpr_carpdev[0] != '\0' &&
		    (cdev = ifunit(carpr.carpr_carpdev)) == NULL)
			return (EINVAL);
		if ((error = carp_set_ifp(sc, cdev)))
			return (error);
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
		if (carpr.carpr_vhid > 0 && carpr.carpr_vhid != sc->sc_vhid) {
			if (carpr.carpr_vhid > 255) {
				error = EINVAL;
				break;
			}
			if (sc->sc_carpdev) {
				struct carp_if *cif;
				cif = (struct carp_if *)sc->sc_carpdev->if_carp;
				TAILQ_FOREACH(vr, &cif->vhif_vrs, sc_list)
					if (vr != sc &&
					    vr->sc_vhid == carpr.carpr_vhid)
						return (EINVAL);
			}
			if (carpr.carpr_vhid != sc->sc_vhid) {
				sc->sc_vhid = carpr.carpr_vhid;
				carp_set_enaddr(sc);
				carp_set_state(sc, INIT);
			}
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
		if (sc->sc_carpdev != NULL)
			strlcpy(carpr.carpr_carpdev, sc->sc_carpdev->if_xname,
			    IFNAMSIZ);
		carpr.carpr_state = sc->sc_state;
		carpr.carpr_vhid = sc->sc_vhid;
		carpr.carpr_advbase = sc->sc_advbase;
		carpr.carpr_advskew = sc->sc_advskew;
		if (suser(p, p->p_acflag) == 0)
			bcopy(sc->sc_key, carpr.carpr_key,
			    sizeof(carpr.carpr_key));
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
		if (sc->sc_suppress)
			carp_ifgroup_ioctl(ifp, cmd, addr);
		break;
	case SIOCSIFGATTR:
		carp_ifgattr_ioctl(ifp, cmd, addr);
		break;
	default:
		error = EINVAL;
	}

	if (bcmp(sc->sc_ac.ac_enaddr, sc->sc_curlladdr, ETHER_ADDR_LEN) != 0)
		carp_set_enaddr(sc);
	carp_hmac_prepare(sc);
	return (error);
}

void
carp_ifgroup_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct ifgroupreq *ifgr = (struct ifgroupreq *)addr;
	struct ifg_list	*ifgl;

	if (!strcmp(ifgr->ifgr_group, IFG_ALL))
		return;
	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next)
		if (!strcmp(ifgl->ifgl_group->ifg_group, ifgr->ifgr_group)) {
			if (cmd == SIOCAIFGROUP)
				ifgl->ifgl_group->ifg_carp_demoted++;
			else if (cmd == SIOCDIFGROUP &&
			    ifgl->ifgl_group->ifg_carp_demoted)
				ifgl->ifgl_group->ifg_carp_demoted--;
		}
}

void
carp_ifgattr_ioctl(struct ifnet *ifp, u_long cmd, caddr_t addr)
{
	struct ifgroupreq *ifgr = (struct ifgroupreq *)addr;
	struct carp_softc *sc = ifp->if_softc;

	if (ifgr->ifgr_attrib.ifg_carp_demoted > 0 && (sc->sc_if.if_flags &
	    (IFF_UP|IFF_RUNNING)) == (IFF_UP|IFF_RUNNING) &&
	    sc->sc_state == MASTER)
		carp_send_ad(sc);
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
	struct carp_softc *sc = ((struct carp_softc *)ifp->if_softc);

	if (sc->sc_carpdev != NULL && sc->sc_state == MASTER)
		return (sc->sc_carpdev->if_output(ifp, m, sa, rt));
	else {
		m_freem(m);
		return (ENETUNREACH);
	}
}

void
carp_set_state(struct carp_softc *sc, int state)
{
	if (sc->sc_state == state)
		return;

	sc->sc_state = state;
	carp_update_lsmask(sc);

	switch (state) {
	case BACKUP:
		sc->sc_if.if_link_state = LINK_STATE_DOWN;
		break;
	case MASTER:
		sc->sc_if.if_link_state = LINK_STATE_UP;
		break;
	default:
		sc->sc_if.if_link_state = LINK_STATE_UNKNOWN;
		break;
	}
	if_link_state_change(&sc->sc_if);
}

void
carp_group_demote_adj(struct ifnet *ifp, int adj)
{
	struct ifg_list	*ifgl;
	int *dm;
	struct carp_softc *nil = NULL;

	TAILQ_FOREACH(ifgl, &ifp->if_groups, ifgl_next) {
		if (!strcmp(ifgl->ifgl_group->ifg_group, IFG_ALL))
			continue;
		dm = &ifgl->ifgl_group->ifg_carp_demoted;

		if (*dm + adj >= 0)
			*dm += adj;
		else
			*dm = 0;

		if (adj > 0 && *dm == 1)
			carp_send_ad_all();
		CARP_LOG(nil, ("%s demoted group %s to %d", ifp->if_xname,
		    ifgl->ifgl_group->ifg_group, *dm));
	}
}

int
carp_group_demote_count(struct carp_softc *sc)
{
	struct ifg_list	*ifgl;
	int count = 0;

	TAILQ_FOREACH(ifgl, &sc->sc_if.if_groups, ifgl_next)
		count += ifgl->ifgl_group->ifg_carp_demoted;

	return (count);
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
			timeout_del(&sc->sc_ad_tmo);
			timeout_del(&sc->sc_md_tmo);
			timeout_del(&sc->sc_md6_tmo);
			carp_set_state(sc, INIT);
			sc->sc_suppress = 1;
			carp_setrun(sc, 0);
			if (!suppressed)
				carp_group_demote_adj(&sc->sc_if, 1);
		} else {
			carp_set_state(sc, INIT);
			sc->sc_suppress = 0;
			carp_setrun(sc, 0);
			if (suppressed)
				carp_group_demote_adj(&sc->sc_if, -1);
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
	MALLOC(mc, struct carp_mc_entry *, sizeof(struct carp_mc_entry),
	    M_DEVBUF, M_NOWAIT);
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
	FREE(mc, M_DEVBUF);
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
		FREE(mc, M_DEVBUF);
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
		FREE(mc, M_DEVBUF);
	}
}
