/*	$OpenBSD: if_pflow.c,v 1.49 2014/12/19 17:14:39 tedu Exp $	*/

/*
 * Copyright (c) 2011 Florian Obser <florian@narrans.de>
 * Copyright (c) 2011 Sebastian Benoit <benoit-lists@fb12.de>
 * Copyright (c) 2008 Henning Brauer <henning@openbsd.org>
 * Copyright (c) 2008 Joerg Goltermann <jg@osn.de>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF MIND, USE, DATA OR PROFITS, WHETHER IN
 * AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT
 * OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <sys/types.h>
#include <sys/malloc.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>

#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_pcb.h>

#include <net/pfvar.h>
#include <net/if_pflow.h>

#include "bpfilter.h"
#include "pflow.h"

#define PFLOW_MINMTU	\
    (sizeof(struct pflow_header) + sizeof(struct pflow_flow))

#ifdef PFLOWDEBUG
#define DPRINTF(x)	do { printf x ; } while (0)
#else
#define DPRINTF(x)
#endif

SLIST_HEAD(, pflow_softc) pflowif_list;
struct pflowstats	 pflowstats;

void	pflowattach(int);
int	pflow_clone_create(struct if_clone *, int);
int	pflow_clone_destroy(struct ifnet *);
void	pflow_init_timeouts(struct pflow_softc *);
int	pflow_calc_mtu(struct pflow_softc *, int, int);
void	pflow_setmtu(struct pflow_softc *, int);
int	pflowoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);
int	pflowioctl(struct ifnet *, u_long, caddr_t);
void	pflowstart(struct ifnet *);

struct mbuf	*pflow_get_mbuf(struct pflow_softc *, u_int16_t);
void	pflow_flush(struct pflow_softc *);
int	pflow_sendout_v5(struct pflow_softc *);
int	pflow_sendout_ipfix(struct pflow_softc *, sa_family_t);
int	pflow_sendout_ipfix_tmpl(struct pflow_softc *);
int	pflow_sendout_mbuf(struct pflow_softc *, struct mbuf *);
void	pflow_timeout(void *);
void	pflow_timeout6(void *);
void	pflow_timeout_tmpl(void *);
void	copy_flow_data(struct pflow_flow *, struct pflow_flow *,
	struct pf_state *, struct pf_state_key *, int, int);
void	copy_flow_ipfix_4_data(struct pflow_ipfix_flow4 *,
	struct pflow_ipfix_flow4 *, struct pf_state *, struct pf_state_key *,
	struct pflow_softc *, int, int);
void	copy_flow_ipfix_6_data(struct pflow_ipfix_flow6 *,
	struct pflow_ipfix_flow6 *, struct pf_state *, struct pf_state_key *,
	struct pflow_softc *, int, int);
int	pflow_pack_flow(struct pf_state *, struct pf_state_key *,
	struct pflow_softc *);
int	pflow_pack_flow_ipfix(struct pf_state *, struct pf_state_key *,
	struct pflow_softc *);
int	pflow_get_dynport(void);
int	export_pflow_if(struct pf_state*, struct pf_state_key *,
	struct pflow_softc *);
int	copy_flow_to_m(struct pflow_flow *flow, struct pflow_softc *sc);
int	copy_flow_ipfix_4_to_m(struct pflow_ipfix_flow4 *flow,
	struct pflow_softc *sc);
int	copy_flow_ipfix_6_to_m(struct pflow_ipfix_flow6 *flow,
	struct pflow_softc *sc);

struct if_clone	pflow_cloner =
    IF_CLONE_INITIALIZER("pflow", pflow_clone_create,
    pflow_clone_destroy);

void
pflowattach(int npflow)
{
	SLIST_INIT(&pflowif_list);
	if_clone_attach(&pflow_cloner);
}

int
pflow_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet		*ifp;
	struct pflow_softc	*pflowif;

	if ((pflowif = malloc(sizeof(*pflowif),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	pflowif->sc_imo.imo_membership = malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
	    M_WAITOK|M_ZERO);
	pflowif->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;
	pflowif->sc_receiver_ip.s_addr = INADDR_ANY;
	pflowif->sc_receiver_port = 0;
	pflowif->sc_sender_ip.s_addr = INADDR_ANY;
	pflowif->sc_sender_port = pflow_get_dynport();
	pflowif->sc_version = PFLOW_PROTO_DEFAULT;

	/* ipfix template init */
	bzero(&pflowif->sc_tmpl_ipfix,sizeof(pflowif->sc_tmpl_ipfix));
	pflowif->sc_tmpl_ipfix.set_header.set_id =
	    htons(PFLOW_IPFIX_TMPL_SET_ID);
	pflowif->sc_tmpl_ipfix.set_header.set_length =
	    htons(sizeof(struct pflow_ipfix_tmpl));

	/* ipfix IPv4 template */
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.h.tmpl_id =
	    htons(PFLOW_IPFIX_TMPL_IPV4_ID);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.h.field_count
	    = htons(PFLOW_IPFIX_TMPL_IPV4_FIELD_COUNT);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.src_ip.field_id =
	    htons(PFIX_IE_sourceIPv4Address);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.src_ip.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.dest_ip.field_id =
	    htons(PFIX_IE_destinationIPv4Address);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.dest_ip.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.if_index_in.field_id =
	    htons(PFIX_IE_ingressInterface);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.if_index_in.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.if_index_out.field_id =
	    htons(PFIX_IE_egressInterface);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.if_index_out.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.packets.field_id =
	    htons(PFIX_IE_packetDeltaCount);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.packets.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.octets.field_id =
	    htons(PFIX_IE_octetDeltaCount);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.octets.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.start.field_id =
	    htons(PFIX_IE_flowStartMilliseconds);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.start.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.finish.field_id =
	    htons(PFIX_IE_flowEndMilliseconds);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.finish.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.src_port.field_id =
	    htons(PFIX_IE_sourceTransportPort);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.src_port.len = htons(2);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.dest_port.field_id =
	    htons(PFIX_IE_destinationTransportPort);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.dest_port.len = htons(2);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.tos.field_id =
	    htons(PFIX_IE_ipClassOfService);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.tos.len = htons(1);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.protocol.field_id =
	    htons(PFIX_IE_protocolIdentifier);
	pflowif->sc_tmpl_ipfix.ipv4_tmpl.protocol.len = htons(1);

	/* ipfix IPv6 template */
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.h.tmpl_id =
	    htons(PFLOW_IPFIX_TMPL_IPV6_ID);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.h.field_count =
	    htons(PFLOW_IPFIX_TMPL_IPV6_FIELD_COUNT);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.src_ip.field_id =
	    htons(PFIX_IE_sourceIPv6Address);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.src_ip.len = htons(16);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.dest_ip.field_id =
	    htons(PFIX_IE_destinationIPv6Address);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.dest_ip.len = htons(16);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.if_index_in.field_id =
	    htons(PFIX_IE_ingressInterface);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.if_index_in.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.if_index_out.field_id =
	    htons(PFIX_IE_egressInterface);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.if_index_out.len = htons(4);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.packets.field_id =
	    htons(PFIX_IE_packetDeltaCount);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.packets.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.octets.field_id =
	    htons(PFIX_IE_octetDeltaCount);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.octets.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.start.field_id =
	    htons(PFIX_IE_flowStartMilliseconds);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.start.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.finish.field_id =
	    htons(PFIX_IE_flowEndMilliseconds);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.finish.len = htons(8);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.src_port.field_id =
	    htons(PFIX_IE_sourceTransportPort);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.src_port.len = htons(2);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.dest_port.field_id =
	    htons(PFIX_IE_destinationTransportPort);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.dest_port.len = htons(2);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.tos.field_id =
	    htons(PFIX_IE_ipClassOfService);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.tos.len = htons(1);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.protocol.field_id =
	    htons(PFIX_IE_protocolIdentifier);
	pflowif->sc_tmpl_ipfix.ipv6_tmpl.protocol.len = htons(1);

	ifp = &pflowif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "pflow%d", unit);
	ifp->if_softc = pflowif;
	ifp->if_ioctl = pflowioctl;
	ifp->if_output = pflowoutput;
	ifp->if_start = pflowstart;
	ifp->if_type = IFT_PFLOW;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	ifp->if_hdrlen = PFLOW_HDRLEN;
	ifp->if_flags = IFF_UP;
	ifp->if_flags &= ~IFF_RUNNING;	/* not running, need receiver */
	pflow_setmtu(pflowif, ETHERMTU);
	pflow_init_timeouts(pflowif);
	if_attach(ifp);
	if_alloc_sadl(ifp);

#if NBPFILTER > 0
	bpfattach(&pflowif->sc_if.if_bpf, ifp, DLT_RAW, 0);
#endif

	/* Insert into list of pflows */
	SLIST_INSERT_HEAD(&pflowif_list, pflowif, sc_next);
	return (0);
}

int
pflow_clone_destroy(struct ifnet *ifp)
{
	struct pflow_softc	*sc = ifp->if_softc;
	int			 s;

	s = splnet();
	if (timeout_initialized(&sc->sc_tmo))
		timeout_del(&sc->sc_tmo);
	if (timeout_initialized(&sc->sc_tmo6))
		timeout_del(&sc->sc_tmo6);
	if (timeout_initialized(&sc->sc_tmo_tmpl))
		timeout_del(&sc->sc_tmo_tmpl);
	pflow_flush(sc);
	if_detach(ifp);
	SLIST_REMOVE(&pflowif_list, sc, pflow_softc, sc_next);
	free(sc->sc_imo.imo_membership, M_IPMOPTS, 0);
	free(sc, M_DEVBUF, 0);
	splx(s);
	return (0);
}

/*
 * Start output on the pflow interface.
 */
void
pflowstart(struct ifnet *ifp)
{
	struct mbuf	*m;
	int		 s;

	for (;;) {
		s = splnet();
		IF_DROP(&ifp->if_snd);
		IF_DEQUEUE(&ifp->if_snd, m);
		splx(s);

		if (m == NULL)
			return;
		m_freem(m);
	}
}

int
pflowoutput(struct ifnet *ifp, struct mbuf *m, struct sockaddr *dst,
	struct rtentry *rt)
{
	m_freem(m);
	return (0);
}

/* ARGSUSED */
int
pflowioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct proc		*p = curproc;
	struct pflow_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *)data;
	struct pflowreq		 pflowr;
	int			 s, error;

	switch (cmd) {
	case SIOCSIFADDR:
	case SIOCAIFADDR:
	case SIOCSIFDSTADDR:
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) &&
		    sc->sc_receiver_ip.s_addr != INADDR_ANY &&
		    sc->sc_receiver_port != 0 &&
		    sc->sc_sender_port != 0) {
			ifp->if_flags |= IFF_RUNNING;
			sc->sc_gcounter=pflowstats.pflow_flows;
			/* send templates on startup */
			if (sc->sc_version == PFLOW_PROTO_10) {
				s = splnet();
				pflow_sendout_ipfix_tmpl(sc);
				splx(s);
			}
		} else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < PFLOW_MINMTU)
			return (EINVAL);
		if (ifr->ifr_mtu > MCLBYTES)
			ifr->ifr_mtu = MCLBYTES;
		s = splnet();
		if (ifr->ifr_mtu < ifp->if_mtu)
			pflow_flush(sc);
		pflow_setmtu(sc, ifr->ifr_mtu);
		splx(s);
		break;

	case SIOCGETPFLOW:
		bzero(&pflowr, sizeof(pflowr));

		pflowr.sender_ip = sc->sc_sender_ip;
		pflowr.receiver_ip = sc->sc_receiver_ip;
		pflowr.receiver_port = sc->sc_receiver_port;
		pflowr.version = sc->sc_version;

		if ((error = copyout(&pflowr, ifr->ifr_data,
		    sizeof(pflowr))))
			return (error);
		break;

	case SIOCSETPFLOW:
		if ((error = suser(p, 0)) != 0)
			return (error);
		if ((error = copyin(ifr->ifr_data, &pflowr,
		    sizeof(pflowr))))
			return (error);
		if (pflowr.addrmask & PFLOW_MASK_VERSION) {
			switch(pflowr.version) {
			case PFLOW_PROTO_5:
			case PFLOW_PROTO_10:
				break;
			default:
				return(EINVAL);
			}
		}
		s = splnet();

		pflow_flush(sc);

		if (pflowr.addrmask & PFLOW_MASK_DSTIP)
			sc->sc_receiver_ip.s_addr = pflowr.receiver_ip.s_addr;
		if (pflowr.addrmask & PFLOW_MASK_DSTPRT)
			sc->sc_receiver_port = pflowr.receiver_port;
		if (pflowr.addrmask & PFLOW_MASK_SRCIP)
			sc->sc_sender_ip.s_addr = pflowr.sender_ip.s_addr;
		/* error check is above */
		if (pflowr.addrmask & PFLOW_MASK_VERSION)
			sc->sc_version = pflowr.version;

		pflow_setmtu(sc, ETHERMTU);
		pflow_init_timeouts(sc);

		splx(s);

		if ((ifp->if_flags & IFF_UP) &&
		    sc->sc_receiver_ip.s_addr != INADDR_ANY &&
		    sc->sc_receiver_port != 0 &&
		    sc->sc_sender_port != 0) {
			ifp->if_flags |= IFF_RUNNING;
			sc->sc_gcounter=pflowstats.pflow_flows;
			if (sc->sc_version == PFLOW_PROTO_10) {
				s = splnet();
				pflow_sendout_ipfix_tmpl(sc);
				splx(s);
			}
		} else
			ifp->if_flags &= ~IFF_RUNNING;

		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

void
pflow_init_timeouts(struct pflow_softc *sc)
{
	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		if (timeout_initialized(&sc->sc_tmo6))
			timeout_del(&sc->sc_tmo6);
		if (timeout_initialized(&sc->sc_tmo_tmpl))
			timeout_del(&sc->sc_tmo_tmpl);
		if (!timeout_initialized(&sc->sc_tmo))
			timeout_set(&sc->sc_tmo, pflow_timeout, sc);
		break;
	case PFLOW_PROTO_10:
		if (!timeout_initialized(&sc->sc_tmo_tmpl))
			timeout_set(&sc->sc_tmo_tmpl, pflow_timeout_tmpl, sc);
		if (!timeout_initialized(&sc->sc_tmo))
			timeout_set(&sc->sc_tmo, pflow_timeout, sc);
		if (!timeout_initialized(&sc->sc_tmo6))
			timeout_set(&sc->sc_tmo6, pflow_timeout6, sc);

		timeout_add_sec(&sc->sc_tmo_tmpl, PFLOW_TMPL_TIMEOUT);
		break;
	default: /* NOTREACHED */
		break;
	}
}

int
pflow_calc_mtu(struct pflow_softc *sc, int mtu, int hdrsz)
{

	sc->sc_maxcount4 = (mtu - hdrsz -
	    sizeof(struct udpiphdr)) / sizeof(struct pflow_ipfix_flow4);
	sc->sc_maxcount6 = (mtu - hdrsz -
	    sizeof(struct udpiphdr)) / sizeof(struct pflow_ipfix_flow6);
	if (sc->sc_maxcount4 > PFLOW_MAXFLOWS)
		sc->sc_maxcount4 = PFLOW_MAXFLOWS;
	if (sc->sc_maxcount6 > PFLOW_MAXFLOWS)
		sc->sc_maxcount6 = PFLOW_MAXFLOWS;
	return (hdrsz + sizeof(struct udpiphdr) +
	    MIN(sc->sc_maxcount4 * sizeof(struct pflow_ipfix_flow4),
	    sc->sc_maxcount6 * sizeof(struct pflow_ipfix_flow6)));
}

void
pflow_setmtu(struct pflow_softc *sc, int mtu_req)
{
	int	mtu;

	if (sc->sc_pflow_ifp && sc->sc_pflow_ifp->if_mtu < mtu_req)
		mtu = sc->sc_pflow_ifp->if_mtu;
	else
		mtu = mtu_req;

	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		sc->sc_maxcount = (mtu - sizeof(struct pflow_header) -
		    sizeof(struct udpiphdr)) / sizeof(struct pflow_flow);
		if (sc->sc_maxcount > PFLOW_MAXFLOWS)
		    sc->sc_maxcount = PFLOW_MAXFLOWS;
		sc->sc_if.if_mtu = sizeof(struct pflow_header) +
		    sizeof(struct udpiphdr) + 
		    sc->sc_maxcount * sizeof(struct pflow_flow);
		break;
	case PFLOW_PROTO_10:
		sc->sc_if.if_mtu = 
		    pflow_calc_mtu(sc, mtu, sizeof(struct pflow_v10_header));
		break;
	default: /* NOTREACHED */
		break;
	}
}

struct mbuf *
pflow_get_mbuf(struct pflow_softc *sc, u_int16_t set_id)
{
	struct pflow_set_header	 set_hdr;
	struct pflow_header	 h;
	struct mbuf		*m;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		pflowstats.pflow_onomem++;
		return (NULL);
	}

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_free(m);
		pflowstats.pflow_onomem++;
		return (NULL);
	}

	m->m_len = m->m_pkthdr.len = 0;
	m->m_pkthdr.rcvif = NULL;

	if (sc == NULL)		/* get only a new empty mbuf */
		return (m);

	if (sc->sc_version == PFLOW_PROTO_5) {
		/* populate pflow_header */
		h.reserved1 = 0;
		h.reserved2 = 0;
		h.count = 0;
		h.version = htons(PFLOW_PROTO_5);
		h.flow_sequence = htonl(sc->sc_gcounter);
		h.engine_type = PFLOW_ENGINE_TYPE;
		h.engine_id = PFLOW_ENGINE_ID;
		m_copyback(m, 0, PFLOW_HDRLEN, &h, M_NOWAIT);

		sc->sc_count = 0;
		timeout_add_sec(&sc->sc_tmo, PFLOW_TIMEOUT);
	} else {
		/* populate pflow_set_header */
		set_hdr.set_length = 0;
		set_hdr.set_id = htons(set_id);
		m_copyback(m, 0, PFLOW_SET_HDRLEN, &set_hdr, M_NOWAIT);
	}

	return (m);
}

void
copy_flow_data(struct pflow_flow *flow1, struct pflow_flow *flow2,
    struct pf_state *st, struct pf_state_key *sk, int src, int dst)
{
	flow1->src_ip = flow2->dest_ip = sk->addr[src].v4.s_addr;
	flow1->src_port = flow2->dest_port = sk->port[src];
	flow1->dest_ip = flow2->src_ip = sk->addr[dst].v4.s_addr;
	flow1->dest_port = flow2->src_port = sk->port[dst];

	flow1->dest_as = flow2->src_as =
	    flow1->src_as = flow2->dest_as = 0;
	flow1->if_index_in = htons(st->if_index_in);
	flow1->if_index_out = htons(st->if_index_out);
	flow2->if_index_in = htons(st->if_index_out);
	flow2->if_index_out = htons(st->if_index_in);
	flow1->dest_mask = flow2->src_mask =
	    flow1->src_mask = flow2->dest_mask = 0;

	flow1->flow_packets = htonl(st->packets[0]);
	flow2->flow_packets = htonl(st->packets[1]);
	flow1->flow_octets = htonl(st->bytes[0]);
	flow2->flow_octets = htonl(st->bytes[1]);

	/*
	 * Pretend the flow was created or expired when the machine came up
	 * when creation is in the future of the last time a package was seen
	 * or was created / expired before this machine came up due to pfsync.
	 */
	flow1->flow_start = flow2->flow_start = st->creation < 0 ||
	    st->creation > st->expire ? htonl(0) : htonl(st->creation * 1000);
	flow1->flow_finish = flow2->flow_finish = st->expire < 0 ? htonl(0) :
	    htonl(st->expire * 1000);
	flow1->tcp_flags = flow2->tcp_flags = 0;
	flow1->protocol = flow2->protocol = sk->proto;
	flow1->tos = flow2->tos = st->rule.ptr->tos;
}

void
copy_flow_ipfix_4_data(struct pflow_ipfix_flow4 *flow1,
    struct pflow_ipfix_flow4 *flow2, struct pf_state *st,
    struct pf_state_key *sk, struct pflow_softc *sc, int src, int dst)
{
	flow1->src_ip = flow2->dest_ip = sk->addr[src].v4.s_addr;
	flow1->src_port = flow2->dest_port = sk->port[src];
	flow1->dest_ip = flow2->src_ip = sk->addr[dst].v4.s_addr;
	flow1->dest_port = flow2->src_port = sk->port[dst];

	flow1->if_index_in = htonl(st->if_index_in);
	flow1->if_index_out = htonl(st->if_index_out);
	flow2->if_index_in = htonl(st->if_index_out);
	flow2->if_index_out = htonl(st->if_index_in);

	flow1->flow_packets = htobe64(st->packets[0]);
	flow2->flow_packets = htobe64(st->packets[1]);
	flow1->flow_octets = htobe64(st->bytes[0]);
	flow2->flow_octets = htobe64(st->bytes[1]);

	/*
	 * Pretend the flow was created when the machine came up when creation
	 * is in the future of the last time a package was seen due to pfsync.
	 */
	if (st->creation > st->expire)
		flow1->flow_start = flow2->flow_start = htobe64((time_second -
		    time_uptime)*1000);
	else
		flow1->flow_start = flow2->flow_start = htobe64((time_second -
		    (time_uptime - st->creation))*1000);
	flow1->flow_finish = flow2->flow_finish = htobe64((time_second -
	    (time_uptime - st->expire))*1000);

	flow1->protocol = flow2->protocol = sk->proto;
	flow1->tos = flow2->tos = st->rule.ptr->tos;
}

void
copy_flow_ipfix_6_data(struct pflow_ipfix_flow6 *flow1,
    struct pflow_ipfix_flow6 *flow2, struct pf_state *st,
    struct pf_state_key *sk, struct pflow_softc *sc, int src, int dst)
{
	bcopy(&sk->addr[src].v6, &flow1->src_ip, sizeof(flow1->src_ip));
	bcopy(&sk->addr[src].v6, &flow2->dest_ip, sizeof(flow2->dest_ip));
	flow1->src_port = flow2->dest_port = sk->port[src];
	bcopy(&sk->addr[dst].v6, &flow1->dest_ip, sizeof(flow1->dest_ip));
	bcopy(&sk->addr[dst].v6, &flow2->src_ip, sizeof(flow2->src_ip));
	flow1->dest_port = flow2->src_port = sk->port[dst];

	flow1->if_index_in = htonl(st->if_index_in);
	flow1->if_index_out = htonl(st->if_index_out);
	flow2->if_index_in = htonl(st->if_index_out);
	flow2->if_index_out = htonl(st->if_index_in);

	flow1->flow_packets = htobe64(st->packets[0]);
	flow2->flow_packets = htobe64(st->packets[1]);
	flow1->flow_octets = htobe64(st->bytes[0]);
	flow2->flow_octets = htobe64(st->bytes[1]);

	/*
	 * Pretend the flow was created when the machine came up when creation
	 * is in the future of the last time a package was seen due to pfsync.
	 */
	if (st->creation > st->expire)
		flow1->flow_start = flow2->flow_start = htobe64((time_second -
		    time_uptime)*1000);
	else
		flow1->flow_start = flow2->flow_start = htobe64((time_second -
		    (time_uptime - st->creation))*1000);
	flow1->flow_finish = flow2->flow_finish = htobe64((time_second -
	    (time_uptime - st->expire))*1000);

	flow1->protocol = flow2->protocol = sk->proto;
	flow1->tos = flow2->tos = st->rule.ptr->tos;
}

int
export_pflow(struct pf_state *st)
{
	struct pflow_softc	*sc = NULL;
	struct pf_state_key	*sk;

	sk = st->key[st->direction == PF_IN ? PF_SK_WIRE : PF_SK_STACK];

	SLIST_FOREACH(sc, &pflowif_list, sc_next) {
		switch (sc->sc_version) {
		case PFLOW_PROTO_5:
			if( sk->af == AF_INET )
				export_pflow_if(st, sk, sc);
			break;
		case PFLOW_PROTO_10:
			if( sk->af == AF_INET || sk->af == AF_INET6 )
				export_pflow_if(st, sk, sc);
			break;
		default: /* NOTREACHED */
			break;
		}
	}

	return (0);
}

int
export_pflow_if(struct pf_state *st, struct pf_state_key *sk,
    struct pflow_softc *sc)
{
	struct pf_state		 pfs_copy;
	struct ifnet		*ifp = &sc->sc_if;
	u_int64_t		 bytes[2];
	int			 ret = 0;

	if (!(ifp->if_flags & IFF_RUNNING))
		return (0);

	if (sc->sc_version == PFLOW_PROTO_10)
		return (pflow_pack_flow_ipfix(st, sk, sc));

	/* PFLOW_PROTO_5 */
	if ((st->bytes[0] < (u_int64_t)PFLOW_MAXBYTES)
	    && (st->bytes[1] < (u_int64_t)PFLOW_MAXBYTES))
		return (pflow_pack_flow(st, sk, sc));

	/* flow > PFLOW_MAXBYTES need special handling */
	bcopy(st, &pfs_copy, sizeof(pfs_copy));
	bytes[0] = pfs_copy.bytes[0];
	bytes[1] = pfs_copy.bytes[1];

	while (bytes[0] > PFLOW_MAXBYTES) {
		pfs_copy.bytes[0] = PFLOW_MAXBYTES;
		pfs_copy.bytes[1] = 0;

		if ((ret = pflow_pack_flow(&pfs_copy, sk, sc)) != 0)
			return (ret);
		if ((bytes[0] - PFLOW_MAXBYTES) > 0)
			bytes[0] -= PFLOW_MAXBYTES;
	}

	while (bytes[1] > (u_int64_t)PFLOW_MAXBYTES) {
		pfs_copy.bytes[1] = PFLOW_MAXBYTES;
		pfs_copy.bytes[0] = 0;

		if ((ret = pflow_pack_flow(&pfs_copy, sk, sc)) != 0)
			return (ret);
		if ((bytes[1] - PFLOW_MAXBYTES) > 0)
			bytes[1] -= PFLOW_MAXBYTES;
	}

	pfs_copy.bytes[0] = bytes[0];
	pfs_copy.bytes[1] = bytes[1];

	return (pflow_pack_flow(&pfs_copy, sk, sc));
}

int
copy_flow_to_m(struct pflow_flow *flow, struct pflow_softc *sc)
{
	int		s, ret = 0;

	s = splnet();
	if (sc->sc_mbuf == NULL) {
		if ((sc->sc_mbuf = pflow_get_mbuf(sc, 0)) == NULL) {
			splx(s);
			return (ENOBUFS);
		}
	}
	m_copyback(sc->sc_mbuf, PFLOW_HDRLEN +
	    (sc->sc_count * sizeof(struct pflow_flow)),
	    sizeof(struct pflow_flow), flow, M_NOWAIT);

	if (pflowstats.pflow_flows == sc->sc_gcounter)
		pflowstats.pflow_flows++;
	sc->sc_gcounter++;
	sc->sc_count++;

	if (sc->sc_count >= sc->sc_maxcount)
		ret = pflow_sendout_v5(sc);

	splx(s);
	return(ret);
}

int
copy_flow_ipfix_4_to_m(struct pflow_ipfix_flow4 *flow, struct pflow_softc *sc)
{
	int		s, ret = 0;

	s = splnet();
	if (sc->sc_mbuf == NULL) {
		if ((sc->sc_mbuf =
		    pflow_get_mbuf(sc, PFLOW_IPFIX_TMPL_IPV4_ID)) == NULL) {
			splx(s);
			return (ENOBUFS);
		}
		sc->sc_count4 = 0;
		timeout_add_sec(&sc->sc_tmo, PFLOW_TIMEOUT);
	}
	m_copyback(sc->sc_mbuf, PFLOW_SET_HDRLEN +
	    (sc->sc_count4 * sizeof(struct pflow_ipfix_flow4)),
	    sizeof(struct pflow_ipfix_flow4), flow, M_NOWAIT);

	if (pflowstats.pflow_flows == sc->sc_gcounter)
		pflowstats.pflow_flows++;
	sc->sc_gcounter++;
	sc->sc_count4++;

	if (sc->sc_count4 >= sc->sc_maxcount4)
		ret = pflow_sendout_ipfix(sc, AF_INET);
	splx(s);
	return(ret);
}

int
copy_flow_ipfix_6_to_m(struct pflow_ipfix_flow6 *flow, struct pflow_softc *sc)
{
	int		s, ret = 0;

	s = splnet();
	if (sc->sc_mbuf6 == NULL) {
		if ((sc->sc_mbuf6 =
		    pflow_get_mbuf(sc, PFLOW_IPFIX_TMPL_IPV6_ID)) == NULL) {
			splx(s);
			return (ENOBUFS);
		}
		sc->sc_count6 = 0;
		timeout_add_sec(&sc->sc_tmo6, PFLOW_TIMEOUT);
	}
	m_copyback(sc->sc_mbuf6, PFLOW_SET_HDRLEN +
	    (sc->sc_count6 * sizeof(struct pflow_ipfix_flow6)),
	    sizeof(struct pflow_ipfix_flow6), flow, M_NOWAIT);

	if (pflowstats.pflow_flows == sc->sc_gcounter)
		pflowstats.pflow_flows++;
	sc->sc_gcounter++;
	sc->sc_count6++;

	if (sc->sc_count6 >= sc->sc_maxcount6)
		ret = pflow_sendout_ipfix(sc, AF_INET6);

	splx(s);
	return(ret);
}

int
pflow_pack_flow(struct pf_state *st, struct pf_state_key *sk,
    struct pflow_softc *sc)
{
	struct pflow_flow	 flow1;
	struct pflow_flow	 flow2;
	int			 ret = 0;

	bzero(&flow1, sizeof(flow1));
	bzero(&flow2, sizeof(flow2));

	if (st->direction == PF_OUT)
		copy_flow_data(&flow1, &flow2, st, sk, 1, 0);
	else
		copy_flow_data(&flow1, &flow2, st, sk, 0, 1);

	if (st->bytes[0] != 0) /* first flow from state */
		ret = copy_flow_to_m(&flow1, sc);

	if (st->bytes[1] != 0) /* second flow from state */
		ret = copy_flow_to_m(&flow2, sc);

	return (ret);
}

int
pflow_pack_flow_ipfix(struct pf_state *st, struct pf_state_key *sk,
    struct pflow_softc *sc)
{
	struct pflow_ipfix_flow4	 flow4_1, flow4_2;
	struct pflow_ipfix_flow6	 flow6_1, flow6_2;
	int				 ret = 0;
	if (sk->af == AF_INET) {
		bzero(&flow4_1, sizeof(flow4_1));
		bzero(&flow4_2, sizeof(flow4_2));

		if (st->direction == PF_OUT)
			copy_flow_ipfix_4_data(&flow4_1, &flow4_2, st, sk, sc,
			    1, 0);
		else
			copy_flow_ipfix_4_data(&flow4_1, &flow4_2, st, sk, sc,
			    0, 1);

		if (st->bytes[0] != 0) /* first flow from state */
			ret = copy_flow_ipfix_4_to_m(&flow4_1, sc);

		if (st->bytes[1] != 0) /* second flow from state */
			ret = copy_flow_ipfix_4_to_m(&flow4_2, sc);
	} else if (sk->af == AF_INET6) {
		bzero(&flow6_1, sizeof(flow6_1));
		bzero(&flow6_2, sizeof(flow6_2));

		if (st->direction == PF_OUT)
			copy_flow_ipfix_6_data(&flow6_1, &flow6_2, st, sk, sc,
			    1, 0);
		else
			copy_flow_ipfix_6_data(&flow6_1, &flow6_2, st, sk, sc,
			    0, 1);

		if (st->bytes[0] != 0) /* first flow from state */
			ret = copy_flow_ipfix_6_to_m(&flow6_1, sc);

		if (st->bytes[1] != 0) /* second flow from state */
			ret = copy_flow_ipfix_6_to_m(&flow6_2, sc);
	}
	return (ret);
}

void
pflow_timeout(void *v)
{
	struct pflow_softc	*sc = v;
	int			 s;

	s = splnet();
	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		pflow_sendout_v5(sc);
		break;
	case PFLOW_PROTO_10:
		pflow_sendout_ipfix(sc, AF_INET);
		break;
	default: /* NOTREACHED */
		break;
	}
	splx(s);
}

void
pflow_timeout6(void *v)
{
	struct pflow_softc	*sc = v;
	int			 s;

	s = splnet();
	pflow_sendout_ipfix(sc, AF_INET6);
	splx(s);
}

void
pflow_timeout_tmpl(void *v)
{
	struct pflow_softc	*sc = v;
	int			 s;

	s = splnet();
	pflow_sendout_ipfix_tmpl(sc);
	splx(s);
}

/* This must be called in splnet() */
void
pflow_flush(struct pflow_softc *sc)
{
	switch (sc->sc_version) {
	case PFLOW_PROTO_5:
		pflow_sendout_v5(sc);
		break;
	case PFLOW_PROTO_10:
		pflow_sendout_ipfix(sc, AF_INET);
		pflow_sendout_ipfix(sc, AF_INET6);
		break;
	default: /* NOTREACHED */
		break;
	}
}


/* This must be called in splnet() */
int
pflow_sendout_v5(struct pflow_softc *sc)
{
	struct mbuf		*m = sc->sc_mbuf;
	struct pflow_header	*h;
	struct ifnet		*ifp = &sc->sc_if;
	struct timespec		tv;

	timeout_del(&sc->sc_tmo);

	if (m == NULL)
		return (0);

	sc->sc_mbuf = NULL;
	if (!(ifp->if_flags & IFF_RUNNING)) {
		m_freem(m);
		return (0);
	}

	pflowstats.pflow_packets++;
	h = mtod(m, struct pflow_header *);
	h->count = htons(sc->sc_count);

	/* populate pflow_header */
	h->uptime_ms = htonl(time_uptime * 1000);

	getnanotime(&tv);
	h->time_sec = htonl(tv.tv_sec);			/* XXX 2038 */
	h->time_nanosec = htonl(tv.tv_nsec);

	return (pflow_sendout_mbuf(sc, m));
}

/* This must be called in splnet() */
int
pflow_sendout_ipfix(struct pflow_softc *sc, sa_family_t af)
{
	struct mbuf			*m;
	struct pflow_v10_header		*h10;
	struct pflow_set_header		*set_hdr;
	struct ifnet			*ifp = &sc->sc_if;
	u_int32_t			 count;
	int				 set_length;

	switch (af) {
	case AF_INET:
		m = sc->sc_mbuf;
		timeout_del(&sc->sc_tmo);
		if (m == NULL)
			return (0);
		sc->sc_mbuf = NULL;
		count = sc->sc_count4;
		set_length = sizeof(struct pflow_set_header)
		    + sc->sc_count4 * sizeof(struct pflow_ipfix_flow4);
		break;
	case AF_INET6:
		m = sc->sc_mbuf6;
		timeout_del(&sc->sc_tmo6);
		if (m == NULL)
			return (0);
		sc->sc_mbuf6 = NULL;
		count = sc->sc_count6;
		set_length = sizeof(struct pflow_set_header)
		    + sc->sc_count6 * sizeof(struct pflow_ipfix_flow6);
		break;
	default: /* NOTREACHED */
		break;
	}

	if (!(ifp->if_flags & IFF_RUNNING)) {
		m_freem(m);
		return (0);
	}

	pflowstats.pflow_packets++;
	set_hdr = mtod(m, struct pflow_set_header *);
	set_hdr->set_length = htons(set_length);

	/* populate pflow_header */
	M_PREPEND(m, sizeof(struct pflow_v10_header), M_DONTWAIT);
	if (m == NULL) {
		pflowstats.pflow_onomem++;
		return (ENOBUFS);
	}
	h10 = mtod(m, struct pflow_v10_header *);
	h10->version = htons(PFLOW_PROTO_10);
	h10->length = htons(PFLOW_IPFIX_HDRLEN + set_length);
	h10->time_sec = htonl(time_second);		/* XXX 2038 */
	h10->flow_sequence = htonl(sc->sc_sequence);
	sc->sc_sequence += count;
	h10->observation_dom = htonl(PFLOW_ENGINE_TYPE);
	return (pflow_sendout_mbuf(sc, m));
}

/* This must be called in splnet() */
int
pflow_sendout_ipfix_tmpl(struct pflow_softc *sc)
{
	struct mbuf			*m;
	struct pflow_v10_header		*h10;
	struct ifnet			*ifp = &sc->sc_if;

	timeout_del(&sc->sc_tmo_tmpl);

	if (!(ifp->if_flags & IFF_RUNNING)) {
		return (0);
	}
	m = pflow_get_mbuf(NULL, 0);
	if (m == NULL)
		return (0);
	if (m_copyback(m, 0, sizeof(struct pflow_ipfix_tmpl),
	    &sc->sc_tmpl_ipfix, M_NOWAIT)) {
		m_freem(m);
		return (0);
	}
	pflowstats.pflow_packets++;

	/* populate pflow_header */
	M_PREPEND(m, sizeof(struct pflow_v10_header), M_DONTWAIT);
	if (m == NULL) {
		pflowstats.pflow_onomem++;
		return (ENOBUFS);
	}
	h10 = mtod(m, struct pflow_v10_header *);
	h10->version = htons(PFLOW_PROTO_10);
	h10->length = htons(PFLOW_IPFIX_HDRLEN + sizeof(struct
	    pflow_ipfix_tmpl));
	h10->time_sec = htonl(time_second);		/* XXX 2038 */
	h10->flow_sequence = htonl(sc->sc_sequence);
	h10->observation_dom = htonl(PFLOW_ENGINE_TYPE);

	timeout_add_sec(&sc->sc_tmo_tmpl, PFLOW_TMPL_TIMEOUT);
	return (pflow_sendout_mbuf(sc, m));
}

int
pflow_sendout_mbuf(struct pflow_softc *sc, struct mbuf *m)
{
	struct udpiphdr	*ui;
	u_int16_t	 len = m->m_pkthdr.len;
#if NBPFILTER > 0
	struct ifnet	*ifp = &sc->sc_if;
#endif
	struct ip	*ip;
	int		 err;

	/* UDP Header*/
	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
	if (m == NULL) {
		pflowstats.pflow_onomem++;
		return (ENOBUFS);
	}

	ui = mtod(m, struct udpiphdr *);
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_src = sc->sc_sender_ip;
	ui->ui_sport = sc->sc_sender_port;
	ui->ui_dst = sc->sc_receiver_ip;
	ui->ui_dport = sc->sc_receiver_port;
	ui->ui_ulen = htons(sizeof(struct udphdr) + len);
	ui->ui_sum = 0;
	m->m_pkthdr.csum_flags |= M_UDP_CSUM_OUT;
	m->m_pkthdr.ph_rtableid = sc->sc_if.if_rdomain;

	ip = (struct ip *)ui;
	ip->ip_v = IPVERSION;
	ip->ip_hl = sizeof(struct ip) >> 2;
	ip->ip_id = htons(ip_randomid());
	ip->ip_off = htons(IP_DF);
	ip->ip_tos = IPTOS_LOWDELAY;
	ip->ip_ttl = IPDEFTTL;
	ip->ip_len = htons(sizeof(struct udpiphdr) + len);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

	sc->sc_if.if_opackets++;
	sc->sc_if.if_obytes += m->m_pkthdr.len;

	if ((err = ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL,
	    0))) {
		pflowstats.pflow_oerrors++;
		sc->sc_if.if_oerrors++;
	}
	return (err);
}

int
pflow_get_dynport(void)
{
	u_int16_t	tmp, low, high, cut;

	low = ipport_hifirstauto;     /* sysctl */
	high = ipport_hilastauto;

	cut = arc4random_uniform(1 + high - low) + low;

	for (tmp = cut; tmp <= high; ++(tmp)) {
		if (!in_baddynamic(tmp, IPPROTO_UDP))
			return (htons(tmp));
	}

	for (tmp = cut - 1; tmp >= low; --(tmp)) {
		if (!in_baddynamic(tmp, IPPROTO_UDP))
			return (htons(tmp));
	}

	return (htons(ipport_hilastauto)); /* XXX */
}

int
pflow_sysctl(int *name, u_int namelen, void *oldp, size_t *oldlenp,
    void *newp, size_t newlen)
{
	if (namelen != 1)
		return (ENOTDIR);

	switch (name[0]) {
	case NET_PFLOW_STATS:
		if (newp != NULL)
			return (EPERM);
		return (sysctl_struct(oldp, oldlenp, newp, newlen,
		    &pflowstats, sizeof(pflowstats)));
	default:
		return (EOPNOTSUPP);
	}
	return (0);
}
