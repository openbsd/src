/*	$OpenBSD: if_pflow.c,v 1.3 2008/09/16 15:48:12 gollo Exp $	*/

/*
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
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/kernel.h>
#include <sys/sysctl.h>
#include <dev/rndvar.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/bpf.h>
#include <net/route.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>
#include <netinet/tcp.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_var.h>
#include <netinet/in_systm.h>
#include <netinet/ip.h>
#include <netinet/ip_var.h>
#include <netinet/udp.h>
#include <netinet/udp_var.h>
#include <netinet/in_pcb.h>
#endif /* INET */

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

struct pflow_softc	*pflowif = NULL;
struct pflowstats	 pflowstats;

void	pflowattach(int);
int	pflow_clone_create(struct if_clone *, int);
int	pflow_clone_destroy(struct ifnet *);
void	pflow_setmtu(struct pflow_softc *, int);
int	pflowoutput(struct ifnet *, struct mbuf *, struct sockaddr *,
	    struct rtentry *);
int	pflowioctl(struct ifnet *, u_long, caddr_t);
void	pflowstart(struct ifnet *);

struct mbuf *pflow_get_mbuf(struct pflow_softc *, void **);
int	pflow_sendout(struct pflow_softc *);
int	pflow_sendout_mbuf(struct pflow_softc *, struct mbuf *);
void	pflow_timeout(void *);
void	copy_flow_data(struct pflow_flow *, struct pflow_flow *,
	struct pf_state *, int, int);
int	pflow_pack_flow(struct pf_state *);
int	pflow_get_dynport(void);

struct if_clone	pflow_cloner =
    IF_CLONE_INITIALIZER("pflow", pflow_clone_create,
    pflow_clone_destroy);

/* from in_pcb.c */
extern int ipport_hifirstauto;
extern int ipport_hilastauto;

void
pflowattach(int npflow)
{
	if_clone_attach(&pflow_cloner);
}

int
pflow_clone_create(struct if_clone *ifc, int unit)
{
	struct ifnet	*ifp;

	if (unit != 0)
		return (EINVAL);

	if ((pflowif = malloc(sizeof(*pflowif),
	    M_DEVBUF, M_NOWAIT|M_ZERO)) == NULL)
		return (ENOMEM);

	pflowif->sc_sender_ip.s_addr = INADDR_ANY;
	pflowif->sc_sender_port = pflow_get_dynport();

	pflowif->sc_imo.imo_membership = malloc(
	    (sizeof(struct in_multi *) * IP_MIN_MEMBERSHIPS), M_IPMOPTS,
	    M_WAITOK|M_ZERO);
	pflowif->sc_imo.imo_max_memberships = IP_MIN_MEMBERSHIPS;
	pflowif->sc_receiver_ip.s_addr = 0;
	pflowif->sc_receiver_port = 0;
	pflowif->sc_sender_ip.s_addr = INADDR_ANY;
	pflowif->sc_sender_port = pflow_get_dynport();
	ifp = &pflowif->sc_if;
	snprintf(ifp->if_xname, sizeof ifp->if_xname, "pflow%d", unit);
	ifp->if_softc = pflowif;
	ifp->if_ioctl = pflowioctl;
	ifp->if_output = pflowoutput;
	ifp->if_start = pflowstart;
	ifp->if_type = IFT_PFLOW;
	ifp->if_snd.ifq_maxlen = ifqmaxlen;
	ifp->if_hdrlen = PFLOW_HDRLEN;
	ifp->if_flags = IFF_UP;
	ifp->if_flags &= ~IFF_RUNNING;	/* not running, need receiver */
	pflow_setmtu(pflowif, ETHERMTU);
	timeout_set(&pflowif->sc_tmo, pflow_timeout, pflowif);
	if_attach(ifp);
	if_alloc_sadl(ifp);

	return (0);
}

int
pflow_clone_destroy(struct ifnet *ifp)
{
	struct pflow_softc *sc = ifp->if_softc;

	timeout_del(&sc->sc_tmo);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	if_detach(ifp);
	free(pflowif->sc_imo.imo_membership, M_IPMOPTS);
	free(pflowif, M_DEVBUF);
	pflowif = NULL;
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
		    sc->sc_receiver_ip.s_addr != 0 &&
		    sc->sc_receiver_port != 0)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < PFLOW_MINMTU)
			return (EINVAL);
		if (ifr->ifr_mtu > MCLBYTES)
			ifr->ifr_mtu = MCLBYTES;
		s = splnet();
		if (ifr->ifr_mtu < ifp->if_mtu)
			pflow_sendout(sc);
		pflow_setmtu(sc, ifr->ifr_mtu);
		splx(s);
		break;

	case SIOCGETPFLOW:
		bzero(&pflowr, sizeof(pflowr));

		pflowr.sender_ip = sc->sc_sender_ip;
		pflowr.receiver_ip = sc->sc_receiver_ip;
		pflowr.receiver_port = sc->sc_receiver_port;

		if ((error = copyout(&pflowr, ifr->ifr_data,
		    sizeof(pflowr))))
			return (error);
		break;

	case SIOCSETPFLOW:
		if ((error = suser(p, p->p_acflag)) != 0)
			return (error);
		if ((error = copyin(ifr->ifr_data, &pflowr,
		    sizeof(pflowr))))
			return (error);

		if ((ifp->if_flags & IFF_UP) && sc->sc_receiver_ip.s_addr != 0
		    && sc->sc_receiver_port != 0) {
			s = splnet();
			pflow_sendout(sc);
			splx(s);
		}

		if (pflowr.addrmask & PFLOW_MASK_DSTIP)
			sc->sc_receiver_ip = pflowr.receiver_ip;
		if (pflowr.addrmask & PFLOW_MASK_DSTPRT)
			sc->sc_receiver_port = pflowr.receiver_port;
		if (pflowr.addrmask & PFLOW_MASK_SRCIP)
			sc->sc_sender_ip.s_addr = pflowr.sender_ip.s_addr;

		if ((ifp->if_flags & IFF_UP) &&
		    sc->sc_receiver_ip.s_addr != 0 &&
		    sc->sc_receiver_port != 0)
			ifp->if_flags |= IFF_RUNNING;
		else
			ifp->if_flags &= ~IFF_RUNNING;

		break;

	default:
		return (ENOTTY);
	}
	return (0);
}

void
pflow_setmtu(struct pflow_softc *sc, int mtu_req)
{
	int	mtu;

	if (sc->sc_pflow_ifp && sc->sc_pflow_ifp->if_mtu < mtu_req)
		mtu = sc->sc_pflow_ifp->if_mtu;
	else
		mtu = mtu_req;

	sc->sc_maxcount = (mtu - sizeof(struct pflow_header)) /
	    sizeof(struct pflow_flow);
	if (sc->sc_maxcount > PFLOW_MAXFLOWS)
	    sc->sc_maxcount = PFLOW_MAXFLOWS;
	sc->sc_if.if_mtu = sizeof(struct pflow_header) +
	    sc->sc_maxcount * sizeof(struct pflow_flow);
}

struct mbuf *
pflow_get_mbuf(struct pflow_softc *sc, void **sp)
{
	struct pflow_header	*h;
	struct mbuf		*m, *top = NULL, **mp = &top;
	int			len, totlen;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		sc->sc_if.if_oerrors++;
		return (NULL);
	}

	len = MHLEN;
	totlen = (sc->sc_maxcount * sizeof(struct pflow_flow)) +
	    sizeof(struct pflow_header);

	while (totlen > 0) {
		if (top) {
			MGET(m, M_DONTWAIT, MT_DATA);
			if (m == 0) {
				m_freem(top);
				sc->sc_if.if_oerrors++;
				return (NULL);
			}
			len = MLEN;
		}
		if (totlen >= MINCLSIZE) {
			MCLGET(m, M_DONTWAIT);
			if (m->m_flags & M_EXT)
				len = MCLBYTES;
			else {
				m_free(m);
				m_freem(top);
				sc->sc_if.if_oerrors++;
				return (NULL);
			}
		}
		m->m_len = len = min(totlen, len);
		totlen -= len;
		*mp = m;
		mp = &m->m_next;
	}

	top->m_pkthdr.rcvif = NULL;
	top->m_len = m->m_pkthdr.len = sizeof(struct pflow_header);

	/* populate pflow_header */
	h = mtod(top, struct pflow_header *);
	h->reserved1 = 0;
	h->reserved2 = 0;
	h->count = 0;
	h->version = htons(PFLOW_VERSION);
	h->flow_sequence = htonl(sc->sc_gcounter);
	h->engine_type = PFLOW_ENGINE_TYPE;
	h->engine_id = PFLOW_ENGINE_ID;

	sc->sc_count = 0;
	*sp = (void *)((char *)h + PFLOW_HDRLEN);
	timeout_add_sec(&sc->sc_tmo, PFLOW_TIMEOUT);
	return (top);
}

void
copy_flow_data(struct pflow_flow *flow1, struct pflow_flow *flow2,
    struct pf_state *st, int src, int dst)
{
	struct pf_state_key	*sk = st->key[PF_SK_WIRE];

	flow1->src_ip = flow2->dest_ip =
	    st->key[PF_SK_WIRE]->addr[src].v4.s_addr;
	flow1->src_port = flow2->dest_port = st->key[PF_SK_WIRE]->port[src];
	flow1->dest_ip = flow2->src_ip =
	    st->key[PF_SK_WIRE]->addr[dst].v4.s_addr;
	flow1->dest_port = flow2->src_port = st->key[PF_SK_WIRE]->port[dst];

	flow1->dest_as = flow2->src_as =
	    flow1->src_as = flow2->dest_as = 0;
	flow1->if_index_out = flow2->if_index_in =
	    flow1->if_index_in = flow2->if_index_out = 0;
	flow1->dest_mask = flow2->src_mask =
	    flow1->src_mask = flow2->dest_mask = 0;

	flow1->flow_packets = htonl(st->packets[0]);
	flow2->flow_packets = htonl(st->packets[1]);
	flow1->flow_octets = htonl(st->bytes[0]);
	flow2->flow_octets = htonl(st->bytes[1]);

	flow1->flow_start = flow2->flow_start = htonl(st->creation * 1000);
	flow1->flow_finish = flow2->flow_finish = htonl(time_second * 1000);
	flow1->tcp_flags = flow2->tcp_flags = 0;
	flow1->protocol = flow2->protocol = sk->proto;
	flow1->tos = flow2->tos = st->rule.ptr->tos;
}

int
export_pflow(struct pf_state *st)
{
	struct pf_state		 pfs_copy;
	struct pflow_softc	*sc = pflowif;
	struct ifnet		*ifp = NULL;
	u_int64_t		 bytes[2];
	int			 ret = 0;

	if (sc == NULL)
		return (0);

	ifp = &sc->sc_if;
	if (!(ifp->if_flags & IFF_UP))
		return (0);

	if ((st->bytes[0] < (u_int64_t)PFLOW_MAXBYTES)
	    && (st->bytes[1] < (u_int64_t)PFLOW_MAXBYTES))
		return pflow_pack_flow(st);

	/* flow > PFLOW_MAXBYTES need special handling */
	bcopy(st, &pfs_copy, sizeof(pfs_copy));
	bytes[0] = pfs_copy.bytes[0];
	bytes[1] = pfs_copy.bytes[1];

	while (bytes[0] > PFLOW_MAXBYTES) {
		pfs_copy.bytes[0] = PFLOW_MAXBYTES;
		pfs_copy.bytes[1] = 0;

		if ((ret = pflow_pack_flow(&pfs_copy)) != 0)
			return (ret);
		if ((bytes[0] - PFLOW_MAXBYTES) > 0)
			bytes[0] -= PFLOW_MAXBYTES;
	}

	while (bytes[1] > (u_int64_t)PFLOW_MAXBYTES) {
		pfs_copy.bytes[1] = PFLOW_MAXBYTES;
		pfs_copy.bytes[0] = 0;

		if ((ret = pflow_pack_flow(&pfs_copy)) != 0)
			return (ret);
		if ((bytes[1] - PFLOW_MAXBYTES) > 0)
			bytes[1] -= PFLOW_MAXBYTES;
	}

	pfs_copy.bytes[0] = bytes[0];
	pfs_copy.bytes[1] = bytes[1];

	return (pflow_pack_flow(&pfs_copy));
}

int
pflow_pack_flow(struct pf_state *st)
{
	struct pflow_softc	*sc = pflowif;
	struct pflow_flow	*flow1 = NULL;
	struct pflow_flow	 flow2;
	struct pf_state_key	*sk = st->key[PF_SK_WIRE];
	int			 s, ret = 0;

	if (sk->af != AF_INET)
		return (0);

	s = splnet();

	if (sc->sc_mbuf == NULL) {
		if ((sc->sc_mbuf = pflow_get_mbuf(sc,
		    (void **)&sc->sc_flowp.s)) == NULL) {
			splx(s);
			return (ENOMEM);
		}
	}

	pflowstats.pflow_flows++;
	sc->sc_gcounter++;
	sc->sc_count++;

	flow1 = sc->sc_flowp.s++;
	sc->sc_mbuf->m_pkthdr.len =
	    sc->sc_mbuf->m_len += sizeof(struct pflow_flow);
	bzero(flow1, sizeof(*flow1));
	bzero(&flow2, sizeof(flow2));

	if (st->direction == PF_OUT)
		copy_flow_data(flow1, &flow2, st, 1, 0);
	else
		copy_flow_data(flow1, &flow2, st, 0, 1);

	if (st->bytes[0] != 0) { /* first flow from state */
		if (sc->sc_count >= sc->sc_maxcount)
			ret = pflow_sendout(sc);

		if (st->bytes[1] != 0) {
			/* one more flow, second part from state */
			if (sc->sc_mbuf == NULL) {
				if ((sc->sc_mbuf = pflow_get_mbuf(sc,
				    (void **)&sc->sc_flowp.s)) == NULL) {
					splx(s);
					return (ENOMEM);
				}
			}

			pflowstats.pflow_flows++;
			sc->sc_gcounter++;
			sc->sc_count++;

			flow1 = sc->sc_flowp.s++;
			sc->sc_mbuf->m_pkthdr.len =
			    sc->sc_mbuf->m_len += sizeof(struct pflow_flow);
			bzero(flow1, sizeof(*flow1));
		}
	}

	if (st->bytes[1] != 0) { /* second flow from state */
		bcopy(&flow2, flow1, sizeof(*flow1));
		if (sc->sc_count >= sc->sc_maxcount)
			ret = pflow_sendout(sc);
	}

	splx(s);
	return (ret);
}

void
pflow_timeout(void *v)
{
	struct pflow_softc	*sc = v;
	int			 s;

	s = splnet();
	pflow_sendout(sc);
	splx(s);
}

/* This must be called in splnet() */
int
pflow_sendout(struct pflow_softc *sc)
{
	struct mbuf		*m;
	struct pflow_header	*h;
#if NBPFILTER > 0
	struct ifnet		*ifp = &sc->sc_if;
#endif

	timeout_del(&sc->sc_tmo);

	if (sc->sc_mbuf == NULL)
		return (0);

	pflowstats.pflow_packets++;

	if (!(ifp->if_flags & IFF_RUNNING)) {
		m_freem(m);
		return (0);
	}

	m = sc->sc_mbuf;
	sc->sc_mbuf = NULL;
	sc->sc_flowp.s = NULL;
	h = mtod(m, struct pflow_header *);
	h->count = htons(sc->sc_count);

	/* populate pflow_header */
	h->uptime_ms = htonl(time_uptime * 1000);
	h->time_sec = htonl(time_second);
	h->time_nanosec = htonl(ticks);

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

	return (pflow_sendout_mbuf(sc, m));
}

int
pflow_sendout_mbuf(struct pflow_softc *sc, struct mbuf *m)
{
	struct udpiphdr	*ui;
	int		 len = m->m_pkthdr.len;

	/* UDP Header*/
	M_PREPEND(m, sizeof(struct udpiphdr), M_DONTWAIT);
	if (m == NULL) {
		pflowstats.pflow_onomem++;
		return (0);
	}

	ui = mtod(m, struct udpiphdr *);
	ui->ui_pr = IPPROTO_UDP;
	ui->ui_len = htons((u_int16_t) len + sizeof (struct udphdr));
	ui->ui_src = sc->sc_sender_ip;
	ui->ui_sport = sc->sc_sender_port;
	ui->ui_dst = sc->sc_receiver_ip;
	ui->ui_dport = sc->sc_receiver_port;
	ui->ui_ulen = ui->ui_len;

	((struct ip *)ui)->ip_v = IPVERSION;
	((struct ip *)ui)->ip_hl = sizeof(struct ip) >> 2;
	((struct ip *)ui)->ip_id = htons(ip_randomid());
	((struct ip *)ui)->ip_off = htons(IP_DF);
	((struct ip *)ui)->ip_tos = IPTOS_LOWDELAY;
	((struct ip *)ui)->ip_ttl = IPDEFTTL;
	((struct ip *)ui)->ip_len = htons(sizeof (struct udpiphdr) + len);

	/*
 	 * Compute the pseudo-header checksum; defer further checksumming
 	 * until ip_output() or hardware (if it exists).
	 */
	m->m_pkthdr.csum_flags |= M_UDPV4_CSUM_OUT;
	ui->ui_sum = in_cksum_phdr(ui->ui_src.s_addr,
	    ui->ui_dst.s_addr, htons((u_int16_t)len +
	    sizeof(struct udphdr) + IPPROTO_UDP));

	if (ip_output(m, NULL, NULL, IP_RAWOUTPUT, &sc->sc_imo, NULL))
		pflowstats.pflow_oerrors++;
	return (0);
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
