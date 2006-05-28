/*	$OpenBSD: if_vic.c,v 1.7 2006/05/28 00:04:24 jason Exp $	*/

/*
 * Copyright (c) 2006 Reyk Floeter <reyk@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Driver for the VMware Virtual NIC ("vmxnet")
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/timeout.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/intr.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_vicreg.h>
#include <dev/pci/if_vicvar.h>

int	vic_match(struct device *, void *, void *);
void	vic_attach(struct device *, struct device *, void *);
void	vic_link_state(struct vic_softc *);
void	vic_shutdown(void *);
int	vic_intr(void *);
void	vic_rx_proc(struct vic_softc *);
void	vic_tx_proc(struct vic_softc *);
void	vic_iff(struct vic_softc *, u_int);
void	vic_getlladdr(struct vic_softc *);
void	vic_setlladdr(struct vic_softc *);
int	vic_media_change(struct ifnet *);
void	vic_media_status(struct ifnet *, struct ifmediareq *);
void	vic_start(struct ifnet *);
int	vic_tx_start(struct vic_softc *, struct mbuf *);
void	vic_watchdog(struct ifnet *);
int	vic_ioctl(struct ifnet *, u_long, caddr_t);
void	vic_init(struct ifnet *);
void	vic_stop(struct ifnet *);
void	vic_printb(unsigned short, char *);
void	vic_timer(void *);
void	vic_poll(void *); /* XXX poll */

struct mbuf *vic_alloc_mbuf(struct vic_softc *, bus_dmamap_t);
int	vic_alloc_data(struct vic_softc *);
void	vic_reset_data(struct vic_softc *);
void	vic_free_data(struct vic_softc *);

struct cfattach vic_ca = {
	sizeof(struct vic_softc), vic_match, vic_attach
};

struct cfdriver vic_cd = {
	0, "vic", DV_IFNET
};

const struct pci_matchid vic_devices[] = {
	{ PCI_VENDOR_VMWARE, PCI_PRODUCT_VMWARE_NET }
};

extern int ifqmaxlen;

int
vic_match(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux,
	    vic_devices, sizeof(vic_devices)/sizeof(vic_devices[0])));
}

void
vic_attach(struct device *parent, struct device *self, void *aux)
{
	struct vic_softc *sc = (struct vic_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t size;
	struct ifnet *ifp;

	/* Enable I/O mapping */
	if (pci_mapreg_map(pa, VIC_BAR0, PCI_MAPREG_TYPE_IO, 0,
	    &sc->sc_st, &sc->sc_sh, NULL, &size, 0)) {
		printf(": I/O mapping of register space failed\n");
		return;
	}

	/* Map and enable the interrupt line */
	if (pci_intr_map(pa, &ih)) {
		printf(": interrupt mapping failed\n");
		goto fail_1;
	}

	intrstr = pci_intr_string(pc, ih);

	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, vic_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail_1;
	}

	printf(": %s\n", intrstr);

	sc->sc_dmat = pa->pa_dmat;

	sc->sc_ver_major = VIC_READ(VIC_VERSION_MAJOR);
	sc->sc_ver_minor = VIC_READ(VIC_VERSION_MINOR);

	printf("%s: vmxnet %X", sc->sc_dev.dv_xname,
	    sc->sc_ver_major & ~VIC_VERSION_MAJOR_M);

	/* Check for a supported version */
	if (((sc->sc_ver_major & VIC_VERSION_MAJOR_M) !=
	    (VIC_MAGIC & VIC_VERSION_MAJOR_M)) ||
	    VIC_MAGIC > sc->sc_ver_major ||
	    VIC_MAGIC < sc->sc_ver_minor) {
		printf(" unsupported device\n");
		goto fail_2;
	}

	VIC_WRITE(VIC_CMD, VIC_CMD_NUM_Rx_BUF);
	sc->sc_nrxbuf = VIC_READ(VIC_CMD);
	if (sc->sc_nrxbuf > VIC_NBUF_MAX || sc->sc_nrxbuf == 0)
		sc->sc_nrxbuf = VIC_NBUF;

	VIC_WRITE(VIC_CMD, VIC_CMD_NUM_Tx_BUF);
	sc->sc_ntxbuf = VIC_READ(VIC_CMD);
	if (sc->sc_ntxbuf > VIC_NBUF_MAX || sc->sc_ntxbuf == 0)
		sc->sc_ntxbuf = VIC_NBUF;

	VIC_WRITE(VIC_CMD, VIC_CMD_FEATURE);
	sc->sc_feature = VIC_READ(VIC_CMD);

	VIC_WRITE(VIC_CMD, VIC_CMD_HWCAP);
	sc->sc_cap = VIC_READ(VIC_CMD);
	if (sc->sc_cap) {
		printf(", ");
		vic_printb(sc->sc_cap, VIC_CMD_HWCAP_BITS);
	}

	printf("\n");

	vic_getlladdr(sc);

	bcopy(sc->sc_lladdr, sc->sc_ac.ac_enaddr, ETHER_ADDR_LEN);

	ifp = &sc->sc_ac.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = vic_ioctl;
	ifp->if_start = vic_start;
	ifp->if_watchdog = vic_watchdog;
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
	IFQ_SET_MAXLEN(&ifp->if_snd, ifqmaxlen);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

#if 0
	/* XXX interface capabilities */
	if (sc->sc_cap & VIC_CMD_HWCAP_VLAN)
		ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
	if (sc->sc_cap & VIC_CMD_HWCAP_CSUM)
		ifp->if_capabilities |= IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 |
		    IFCAP_CSUM_UDPv4;
#endif

	/* Allocate Rx and Tx queues */
	if (vic_alloc_data(sc) != 0) {
		printf(": could not allocate queues\n");
		goto fail_2;
	}

	printf(", address %s\n", ether_sprintf(sc->sc_lladdr));

	/* Initialise pseudo media types */
	ifmedia_init(&sc->sc_media, 0, vic_media_change, vic_media_status);
	ifmedia_add(&sc->sc_media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->sc_media, IFM_ETHER | IFM_AUTO);

	/* Attach the device structures */
	if_attach(ifp);
	ether_ifattach(ifp);

	sc->sc_sdhook = shutdownhook_establish(vic_shutdown, sc);

	/* Initialize timeout for link state update. */
	timeout_set(&sc->sc_timer, vic_timer, sc);

	/* XXX poll */
	timeout_set(&sc->sc_poll, vic_poll, sc);
	return;

fail_2:
	pci_intr_disestablish(pc, sc->sc_ih);

fail_1:
	bus_space_unmap(sc->sc_st, sc->sc_sh, size);
}

void
vic_link_state(struct vic_softc *sc)
{       
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	u_int32_t status;
	int link_state = LINK_STATE_DOWN;

	status = VIC_READ(VIC_STATUS);
	if (status & VIC_STATUS_CONNECTED)
		link_state = LINK_STATE_UP;
	if (ifp->if_link_state != link_state) {
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}
}

void
vic_shutdown(void *self)
{
	struct vic_softc *sc = (struct vic_softc *)self;

	vic_stop(&sc->sc_ac.ac_if);
}

int
vic_intr(void *arg)
{
	struct vic_softc *sc = (struct vic_softc *)arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

	VIC_WRITE(VIC_CMD, VIC_CMD_INTR_ACK);

#ifdef VIC_DEBUG
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: %s\n", sc->sc_dev.dv_xname, __func__);
#endif

	vic_rx_proc(sc);
	vic_tx_proc(sc);

	return (1);
}

void
vic_rx_proc(struct vic_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct vic_rxdesc *desc;
	struct vic_rxbuf *rxb;
	struct mbuf *m;
	int len, idx;

	for (;;) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_map, 0,
		    sizeof(struct vic_data),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		idx = letoh32(sc->sc_data->vd_rx_nextidx);
		if (idx >= sc->sc_data->vd_rx_length) {
			ifp->if_ierrors++;
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: receive index error\n",
				    sc->sc_dev.dv_xname);
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, sc->sc_map,
		    VIC_OFF_RXDESC(idx), sizeof(struct vic_rxdesc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		desc = &sc->sc_rxq[idx];

		if (desc->rx_owner != VIC_OWNER_DRIVER)
			break;

		len = letoh32(desc->rx_length);
		ifp->if_ibytes += len;
		ifp->if_ipackets++;

		if (len < ETHER_MIN_LEN) {
			ifp->if_iqdrops++;
			goto nextp;
		}

		if ((rxb = (struct vic_rxbuf *)desc->rx_priv) == NULL ||
		    rxb->rxb_m == NULL) {
			ifp->if_ierrors++;
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: receive buffer error\n",
				    sc->sc_dev.dv_xname);
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, rxb->rxb_map, 0,
		    rxb->rxb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, rxb->rxb_map);
		m = rxb->rxb_m;
		rxb->rxb_m = NULL;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;

		/* Get new mbuf for the Rx queue */
		if ((rxb->rxb_m = vic_alloc_mbuf(sc, rxb->rxb_map)) == NULL) {
			ifp->if_ierrors++;
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: receive buffer failed\n",
				    sc->sc_dev.dv_xname);
			break;
		}
		desc->rx_physaddr = rxb->rxb_map->dm_segs->ds_addr;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		ether_input_mbuf(ifp, m);

 nextp:
		desc->rx_owner = VIC_OWNER_NIC;
		VIC_INC(sc->sc_data->vd_rx_nextidx, sc->sc_data->vd_rx_length);
	}
}

void
vic_tx_proc(struct vic_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct vic_txdesc *desc;
	struct vic_txbuf *txb;
	int idx;

	for (; sc->sc_txpending;) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_map, 0,
		    sizeof(struct vic_data),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		idx = letoh32(sc->sc_data->vd_tx_curidx);
		if (idx >= sc->sc_data->vd_tx_length) {
			ifp->if_oerrors++;
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: transmit index error\n",
				    sc->sc_dev.dv_xname);
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, sc->sc_map,
		    VIC_OFF_TXDESC(idx), sizeof(struct vic_rxdesc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		desc = &sc->sc_txq[idx];

		if (desc->tx_owner != VIC_OWNER_DRIVER)
			break;

		if ((txb = (struct vic_txbuf *)desc->tx_priv) == NULL ||
		    txb->txb_m == NULL) {
			ifp->if_oerrors++;
			if (ifp->if_flags & IFF_DEBUG)
				printf("%s: transmit buffer error\n",
				    sc->sc_dev.dv_xname);
			break;
		}

		bus_dmamap_sync(sc->sc_dmat, txb->txb_map, 0,
		    txb->txb_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, txb->txb_map);
		if (txb->txb_m != NULL) {
			m_freem(txb->txb_m);
			txb->txb_m = NULL;
			ifp->if_flags &= ~IFF_OACTIVE;
		}

		sc->sc_txpending--;
		sc->sc_data->vd_tx_stopped = 0;

		VIC_INC(sc->sc_data->vd_tx_curidx, sc->sc_data->vd_tx_length);
	}

	sc->sc_txtimeout = 0;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		vic_start(ifp);
}

void
vic_iff(struct vic_softc *sc, u_int flags)
{
	/* XXX ALLMULTI */
	memset(&sc->sc_data->vd_mcastfil, 0xff,
	    sizeof(sc->sc_data->vd_mcastfil));
	sc->sc_data->vd_iff = flags;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_map, 0,
	    sizeof(struct vic_data), BUS_DMASYNC_POSTWRITE);

	VIC_WRITE(VIC_CMD, VIC_CMD_MCASTFIL);
	VIC_WRITE(VIC_CMD, VIC_CMD_IFF);
}

void
vic_getlladdr(struct vic_softc *sc)
{
	u_int32_t reg;
	int i;

	/* Get MAC address */
	reg = sc->sc_cap & VIC_CMD_HWCAP_VPROM ? VIC_VPROM : VIC_LLADDR;
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc->sc_lladdr[i] = VIC_READ8(reg + i);

	/* Update the MAC address register */
	if (reg == VIC_VPROM)
		vic_setlladdr(sc);
}

void
vic_setlladdr(struct vic_softc *sc)
{
	int i;

	for (i = 0; i < ETHER_ADDR_LEN; i++)
		VIC_WRITE8(VIC_LLADDR + i, sc->sc_lladdr[i]);
}

int
vic_media_change(struct ifnet *ifp)
{
	/* Ignore */
	return (0);
}

void
vic_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct vic_softc *sc = (struct vic_softc *)ifp->if_softc;

	imr->ifm_active = IFM_ETHER | IFM_AUTO;
	imr->ifm_status = IFM_AVALID;

	vic_link_state(sc);

	if (ifp->if_link_state == LINK_STATE_UP &&
	    ifp->if_flags & IFF_UP)
		imr->ifm_status |= IFM_ACTIVE;
}

void
vic_start(struct ifnet *ifp)
{
	struct vic_softc *sc = (struct vic_softc *)ifp->if_softc;
	struct mbuf *m;
	int error;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	for (;; error = 0) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif

		if ((error = vic_tx_start(sc, m)) != 0)
			ifp->if_oerrors++;
	}
}

int
vic_tx_start(struct vic_softc *sc, struct mbuf *m)
{
	struct ifnet *ifp = &sc->sc_ac.ac_if;
	struct vic_txbuf *txb;
	struct vic_txdesc *desc;
	struct mbuf *m0 = NULL;
	int idx;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_map, 0,
	    sizeof(struct vic_data),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	idx = letoh32(sc->sc_data->vd_tx_nextidx);
	if (idx >= sc->sc_data->vd_tx_length) {
		ifp->if_oerrors++;
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: transmit index error\n",
			    sc->sc_dev.dv_xname);
		return (EINVAL);
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_map,
	    VIC_OFF_TXDESC(idx), sizeof(struct vic_txdesc),
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	desc = &sc->sc_txq[idx];
	txb = (struct vic_txbuf *)desc->tx_priv;

	if (VIC_TXURN(sc)) {
		ifp->if_flags |= IFF_OACTIVE;
		return (ENOBUFS);
	} else if (txb == NULL) {
		ifp->if_oerrors++;
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: transmit buffer error\n",
			    sc->sc_dev.dv_xname);
		return (ENOBUFS);
	} else if (txb->txb_m != NULL) {
		sc->sc_data->vd_tx_stopped = 1;
		ifp->if_oerrors++;
		return (ENOMEM);
	}

	if (bus_dmamap_load_mbuf(sc->sc_dmat, txb->txb_map,
	    m, BUS_DMA_NOWAIT) != 0) {
		MGETHDR(m0, M_DONTWAIT, MT_DATA);
		if (m0 == NULL)
			return (ENOBUFS);
		if (m->m_pkthdr.len > MHLEN) {
			MCLGET(m0, M_DONTWAIT);
			if (!(m0->m_flags & M_EXT)) {
				m_freem(m0);
				return (E2BIG);
			}
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, caddr_t));
		m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, txb->txb_map,
		    m0, BUS_DMA_NOWAIT) != 0) {
			m_freem(m0);
			return (ENOBUFS);
		}
	}

	IFQ_DEQUEUE(&ifp->if_snd, m);
	if (m0 != NULL) {
		m_freem(m);
		m = m0;
		m0 = NULL;
	}

	desc->tx_flags = VIC_TX_FLAGS_KEEP;
	desc->tx_sa.sa_length = 1;
	desc->tx_sa.sa_sg[0].sg_length = htole16(m->m_len);
	desc->tx_sa.sa_sg[0].sg_addr_low = txb->txb_map->dm_segs->ds_addr;
	desc->tx_owner = VIC_OWNER_NIC;

	if (VIC_TXURN_WARN(sc)) {
		if (ifp->if_flags & IFF_DEBUG)
			printf("%s: running out of tx descriptors\n",
			    sc->sc_dev.dv_xname);
		desc->tx_flags |= VIC_TX_FLAGS_TXURN;
	}

	ifp->if_opackets++;
	ifp->if_obytes += m->m_len;

	sc->sc_txpending++;
	sc->sc_txtimeout = VIC_TX_TIMEOUT;
	ifp->if_timer = 1;

	VIC_INC(sc->sc_data->vd_tx_nextidx, sc->sc_data->vd_tx_length);

	return (0);
}

void
vic_watchdog(struct ifnet *ifp)
{
	struct vic_softc *sc = (struct vic_softc *)ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_txpending && sc->sc_txtimeout > 0) {
		if (--sc->sc_txtimeout == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			ifp->if_flags &= ~IFF_RUNNING;
			vic_init(ifp);
			ifp->if_oerrors++;
			return;
		}
		ifp->if_timer = 1;
	}

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		vic_start(ifp);
}

int
vic_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct vic_softc *sc = (struct vic_softc *)ifp->if_softc;
	struct ifaddr *ifa;
	struct ifreq *ifr;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->sc_ac, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *)data;
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_ac, ifa);
#endif
		/* FALLTHROUGH */
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING) == 0)
				vic_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				vic_stop(ifp);
		}
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ETHERMTU_JUMBO)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu)
			ifp->if_mtu = ifr->ifr_mtu;
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		ifr = (struct ifreq *)data;
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_ac) :
		    ether_delmulti(ifr, &sc->sc_ac);

		if (error == ENETRESET)
			error = 0;
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = ENOTTY;
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			vic_init(ifp);
		error = 0;
	}

	splx(s);

	return (error);
}

void
vic_init(struct ifnet *ifp)
{
	struct vic_softc *sc = (struct vic_softc *)ifp->if_softc;
	int s;

	s = splnet();

	timeout_add(&sc->sc_timer, hz * VIC_TIMER_DELAY);

	vic_reset_data(sc);

#ifdef notyet
	VIC_WRITE(VIC_CMD, VIC_CMD_INTR_ENABLE);
#endif

	VIC_WRITE(VIC_DATA_ADDR, sc->sc_physaddr);
	VIC_WRITE(VIC_DATA_LENGTH, htole32(sc->sc_size));
	if (ifp->if_flags & IFF_PROMISC)
		vic_iff(sc, VIC_CMD_IFF_PROMISC);
	else
		vic_iff(sc, VIC_CMD_IFF_BROADCAST | VIC_CMD_IFF_MULTICAST);

#ifdef VIC_DEBUG
	if (ifp->if_flags & IFF_DEBUG)
		printf("%s: physaddr 0x%08x length 0x%08x\n",
		    sc->sc_dev.dv_xname, sc->sc_physaddr, sc->sc_size);
#endif

	sc->sc_data->vd_tx_stopped = sc->sc_data->vd_tx_queued = 0;

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/* XXX poll */
	if (ifp->if_flags & IFF_LINK0) {
		sc->sc_polling = 1;
		timeout_add(&sc->sc_poll, VIC_TIMER_MS(100));
	} else
		sc->sc_polling = 0;

	splx(s);
}

void
vic_stop(struct ifnet *ifp)
{
	struct vic_softc *sc = (struct vic_softc *)ifp->if_softc;
	int s;

	s = splnet();

	sc->sc_txtimeout = 0;
	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

#ifdef notyet
	VIC_WRITE(VIC_CMD, VIC_CMD_INTR_DISABLE);
#endif

	VIC_WRITE(VIC_DATA_ADDR, 0);
	vic_iff(sc, 0);

	sc->sc_data->vd_tx_stopped = 1;
	timeout_del(&sc->sc_timer);

	/* XXX poll */
	if (sc->sc_polling) {
		sc->sc_polling = 0;
		timeout_del(&sc->sc_poll);
	}

	splx(s);
}

void
vic_printb(unsigned short v, char *bits)
{
	int i, any = 0;
	char c;

	/*
	 * Used to print capability bits on startup
	 */
	bits++;
	if (bits) {
		while ((i = *bits++)) {
			if (v & (1 << (i-1))) {
				if (any)
					printf(" ");
				any = 1;
				for (; (c = *bits) > 32; bits++)
					printf("%c", c);
			} else
				for (; *bits > 32; bits++)
					;
		}
	}
}

struct mbuf *
vic_alloc_mbuf(struct vic_softc *sc, bus_dmamap_t map)
{
	struct mbuf *m = NULL;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (NULL);
	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (NULL);
	}
	if (bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT) != 0) {
		printf("%s: could not load mbuf DMA map",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return (NULL);
	}

	return (m);
}

int
vic_alloc_data(struct vic_softc *sc)
{
	struct vic_rxbuf *rxb;
	struct vic_txbuf *txb;
	u_int32_t offset;
	int i, error;

	sc->sc_size = sizeof(struct vic_data) +
	    (sc->sc_nrxbuf + VIC_QUEUE2_SIZE) * sizeof(struct vic_rxdesc) +
	    sc->sc_ntxbuf * sizeof(struct vic_txdesc);

	if ((error = bus_dmamap_create(sc->sc_dmat, sc->sc_size, 1,
	    sc->sc_size, 0, BUS_DMA_NOWAIT, &sc->sc_map)) != 0) {
		printf("%s: could not create DMA material\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	if ((error = bus_dmamem_alloc(sc->sc_dmat, sc->sc_size, PAGE_SIZE, 0,
	    &sc->sc_seg, 1, &sc->sc_nsegs, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->sc_seg,
	    sc->sc_nsegs, sc->sc_size, (caddr_t *)&sc->sc_data,
	    BUS_DMA_NOWAIT | BUS_DMA_COHERENT)) != 0) {
		printf("%s: could not map DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat, sc->sc_map,
	    sc->sc_data, sc->sc_size, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: could not load DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bzero(sc->sc_data, sc->sc_size);
	sc->sc_physaddr = sc->sc_map->dm_segs->ds_addr;
	sc->sc_data->vd_magic = VIC_MAGIC;
	sc->sc_data->vd_length = htole32(sc->sc_size);
	offset = (u_int32_t)sc->sc_data + sizeof(struct vic_data);

#ifdef VIC_DEBUG
	printf("%s: (rxbuf %d * %d) (txbuf %d * %d) (size %d)\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_nrxbuf, sizeof(struct vic_rxdesc),
	    sc->sc_ntxbuf, sizeof(struct vic_txdesc),
	    sc->sc_size);
#endif

	/* Setup the Rx queue */
	sc->sc_rxq = (struct vic_rxdesc *)offset;
	sc->sc_data->vd_rx_offset = htole32(offset);
	sc->sc_data->vd_rx_length = htole32(sc->sc_nrxbuf);
	offset += sc->sc_nrxbuf + sizeof(struct vic_rxdesc);
	for (i = 0; i < sc->sc_nrxbuf; i++) {
		rxb = &sc->sc_rxbuf[i];
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    VIC_MAX_SCATTER, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &rxb->rxb_map)) != 0) {
			printf("%s: could not create rx DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		/* Preallocate the Rx mbuf */
		if ((rxb->rxb_m = vic_alloc_mbuf(sc, rxb->rxb_map)) == NULL) {
			error = ENOMEM;
			goto fail;
		}
		sc->sc_rxq[i].rx_physaddr = rxb->rxb_map->dm_segs->ds_addr;
	}

	/* Setup Rx queue 2 (unused gap) */
	sc->sc_rxq2 = (struct vic_rxdesc *)offset;
	sc->sc_data->vd_rx_offset = htole32(offset);
	sc->sc_data->vd_rx_length = htole32(1);
	sc->sc_rxq2[0].rx_owner = VIC_OWNER_DRIVER;
	offset += sizeof(struct vic_rxdesc);

	/* Setup the Tx queue */
	sc->sc_txq = (struct vic_txdesc *)offset;
	sc->sc_data->vd_tx_offset = htole32(offset);
	sc->sc_data->vd_tx_length = htole32(sc->sc_ntxbuf);
	offset += sc->sc_ntxbuf + sizeof(struct vic_txdesc);
	for (i = 0; i < sc->sc_ntxbuf; i++) {
		txb = &sc->sc_txbuf[i];
		if ((error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    VIC_MAX_SCATTER, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &txb->txb_map)) != 0) {
			printf("%s: could not create tx DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}

	return (0);

 fail:
	vic_free_data(sc);

	return (error);
}

void
vic_reset_data(struct vic_softc *sc)
{
	struct vic_rxbuf *rxb;
	struct vic_txbuf *txb;
	int i;

	for (i = 0; i < sc->sc_nrxbuf; i++) {
		rxb = &sc->sc_rxbuf[i];

		bzero(&sc->sc_rxq[i], sizeof(struct vic_rxdesc));
		sc->sc_rxq[i].rx_physaddr =
		    rxb->rxb_map->dm_segs->ds_addr;
		sc->sc_rxq[i].rx_buflength = htole32(MCLBYTES);
		sc->sc_rxq[i].rx_owner = VIC_OWNER_NIC;
		sc->sc_rxq[i].rx_priv = rxb;

		bus_dmamap_sync(sc->sc_dmat, rxb->rxb_map, 0,
		    rxb->rxb_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	}

	for (i = 0; i < sc->sc_ntxbuf; i++) {
		txb = &sc->sc_txbuf[i];

		bzero(&sc->sc_txq[i], sizeof(struct vic_txdesc));
		sc->sc_txq[i].tx_owner = VIC_OWNER_DRIVER;
		sc->sc_txq[i].tx_priv = txb;

		if (sc->sc_txbuf[i].txb_m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, txb->txb_map, 0,
			    txb->txb_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, txb->txb_map);
			m_freem(sc->sc_txbuf[i].txb_m);
			sc->sc_txbuf[i].txb_m = NULL;
		}
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_map, 0,
	    sc->sc_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
}

void
vic_free_data(struct vic_softc *sc)
{
	bus_dmamap_t map;
	int i;

	if (sc->sc_data == NULL)
		return;

	/* Free Rx queue */
	for (i = 0; i < sc->sc_nrxbuf; i++) {
		if ((map = sc->sc_rxbuf[i].rxb_map) == NULL)
			continue;
		if (sc->sc_rxbuf[i].rxb_m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, map, 0,
			    map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, map);
			m_freem(sc->sc_rxbuf[i].rxb_m);
		}
		bus_dmamap_destroy(sc->sc_dmat, map);
	}

	/* Free Tx queue */
	for (i = 0; i < sc->sc_ntxbuf; i++) {
		if ((map = sc->sc_txbuf[i].txb_map) == NULL)
			continue;
		if (sc->sc_txbuf[i].txb_m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, map, 0,
			    map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, map);
			m_freem(sc->sc_txbuf[i].txb_m);
		}
		bus_dmamap_destroy(sc->sc_dmat, map);
	}

	bus_dmamap_sync(sc->sc_dmat, sc->sc_map, 0,
	    sc->sc_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat, sc->sc_map);
	bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_data, sc->sc_size);
	bus_dmamem_free(sc->sc_dmat, &sc->sc_seg, sc->sc_nsegs);
}

void
vic_timer(void *arg)
{
	struct vic_softc *sc = (struct vic_softc *)arg;
	struct ifnet *ifp = &sc->sc_ac.ac_if;

#ifdef VIC_DEBUG
	if (ifp->if_flags & IFF_DEBUG) {
		if (sc->sc_polling)
			printf("%s: %s (polling #%u)\n",
			    ifp->if_xname, __func__, sc->sc_polling);
		else
			printf("%s: %s\n",
			    ifp->if_xname, __func__);
	}
#endif

	/* Update link state (if changed) */
	vic_link_state(sc);

	/* Re-schedule another timeout. */
	timeout_add(&sc->sc_timer, hz * VIC_TIMER_DELAY);
}

 /* XXX poll */
void
vic_poll(void *arg)
{
	struct vic_softc *sc = (struct vic_softc *)arg;
	int s;

	s = splnet();

	vic_rx_proc(sc);
	vic_tx_proc(sc);

	VIC_INC_POS(sc->sc_polling, UINT_MAX);

	timeout_add(&sc->sc_poll, VIC_TIMER_MS(100));

	splx(s);
}
