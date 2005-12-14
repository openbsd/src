/*	$OpenBSD: if_nfe.c,v 1.2 2005/12/14 22:08:20 jsg Exp $	*/
/*
 * Copyright (c) 2005 Jonathan Gray <jsg@openbsd.org>
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

/* Driver for nvidia nForce Ethernet */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/endian.h>
#include <sys/systm.h>
#include <sys/types.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_nfereg.h>
#include <dev/pci/if_nfevar.h>

int	nfe_match(struct device *, void *, void *);
void	nfe_attach(struct device *, struct device *, void *);

int	nfe_intr(void *);
int	nfe_alloc_rx_ring(struct nfe_softc *, struct nfe_rx_ring *, int);
void	nfe_reset_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
void	nfe_free_rx_ring(struct nfe_softc *, struct nfe_rx_ring *);
int	nfe_rxintr(struct nfe_softc *);
int	nfe_alloc_tx_ring(struct nfe_softc *, struct nfe_tx_ring *, int);
void	nfe_reset_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
void	nfe_free_tx_ring(struct nfe_softc *, struct nfe_tx_ring *);
int	nfe_txintr(struct nfe_softc *);

int	nfe_ioctl(struct ifnet *, u_long, caddr_t);
void	nfe_start(struct ifnet *);
void	nfe_stop(struct ifnet *, int);
void	nfe_watchdog(struct ifnet *);
int	nfe_init(struct ifnet *);
void	nfe_reset(struct nfe_softc *);
void	nfe_setmulti(struct nfe_softc *);
void	nfe_update_promisc(struct nfe_softc *);
void	nfe_tick(void *);

int	nfe_miibus_readreg(struct device *, int, int);
void	nfe_miibus_writereg(struct device *, int, int, int);
void	nfe_miibus_statchg(struct device *);
int	nfe_mediachange(struct ifnet *);
void	nfe_mediastatus(struct ifnet *, struct ifmediareq *);

struct cfattach nfe_ca = {
	sizeof(struct nfe_softc),
	nfe_match,
	nfe_attach
};

struct cfdriver nfe_cd = {
	0, "nfe", DV_IFNET
};


#ifdef NFE_DEBUG
int	nfedebug = 1;
#define DPRINTF(x)	do { if (nfedebug) printf x; } while (0)
#define DPRINTFN(n,x)	do { if (nfedebug >= (n)) printf x; } while (0)
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

const struct pci_matchid nfe_devices[] = {
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE_LAN },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE2_LAN },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN3 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN4 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_NFORCE3_LAN5 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_CK804_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_CK804_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP04_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP04_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP51_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP51_LAN2 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP55_LAN1 },
	{ PCI_VENDOR_NVIDIA, PCI_PRODUCT_NVIDIA_MCP55_LAN2 },
};

int
nfe_match(struct device *dev, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, nfe_devices,
	    sizeof(nfe_devices)/sizeof(nfe_devices[0])));
}

void
nfe_attach(struct device *parent, struct device *self, void *aux)
{
	struct nfe_softc	*sc = (struct nfe_softc *)self;	
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;
	const char		*intrstr = NULL;
	struct ifnet		*ifp;
	bus_size_t		iosize;
	pcireg_t		command;
	int			i;

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	    
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if ((command & PCI_COMMAND_MEM_ENABLE) == 0) {
		printf(": mem space not enabled\n");
		return;
	}

	if (pci_mapreg_map(pa, NFE_PCI_BA, PCI_MAPREG_TYPE_MEM, 0,
	    &sc->sc_iot, &sc->sc_ioh, NULL, &iosize, 0)) {
		printf(": can't map mem space\n");
		return;
	}

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, nfe_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		return;
	}
	printf(": %s", intrstr);

	sc->sc_dmat = pa->pa_dmat;

	i = betoh16(NFE_READ(sc, NFE_MACADDR_LO) & 0xffff);
	memcpy((char *)sc->sc_arpcom.ac_enaddr, &i, 2);
	i = betoh32(NFE_READ(sc, NFE_MACADDR_HI));
	memcpy(&(sc->sc_arpcom.ac_enaddr[2]), &i, 4);

	printf(", address %s\n",
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	switch(PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN2:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN3: 
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN4:
	case PCI_PRODUCT_NVIDIA_NFORCE3_LAN5:
		sc->sc_flags |= NFE_JUMBO_SUP;
		break;
	case PCI_PRODUCT_NVIDIA_CK804_LAN1:
	case PCI_PRODUCT_NVIDIA_CK804_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP04_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP04_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP51_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP51_LAN2:
	case PCI_PRODUCT_NVIDIA_MCP55_LAN1:
	case PCI_PRODUCT_NVIDIA_MCP55_LAN2:
		sc->sc_flags |= NFE_JUMBO_SUP | NFE_40BIT_ADDR;
		break;
	default:
		sc->sc_flags = 0;
	}

	/*
	 * Allocate Tx and Rx rings.
	 */
	if (nfe_alloc_tx_ring(sc, &sc->txq, NFE_TX_RING_COUNT) != 0) {
		printf("%s: could not allocate Tx ring\n",
		    sc->sc_dev.dv_xname);
		goto fail1;
	}

	if (nfe_alloc_rx_ring(sc, &sc->rxq, NFE_RX_RING_COUNT) != 0) {
		printf("%s: could not allocate Rx ring\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	NFE_WRITE(sc, NFE_RING_SIZE, sc->rxq.count << 16 | sc->txq.count);

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nfe_ioctl;
	ifp->if_start = nfe_start;
	ifp->if_watchdog = nfe_watchdog;
	ifp->if_init = nfe_init;
	ifp->if_baudrate = 1000000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, NFE_IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);

	/* Set interface name */
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = nfe_miibus_readreg;
	sc->sc_mii.mii_writereg = nfe_miibus_writereg;
	sc->sc_mii.mii_statchg = nfe_miibus_statchg;

	/* XXX always seem to get a ghost ukphy along with eephy on nf4u */
	ifmedia_init(&sc->sc_mii.mii_media, 0,
	    nfe_mediachange, nfe_mediastatus);
	mii_attach(self, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc->sc_dev.dv_xname);
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL,
		    0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	/* XXX powerhook */
	return;

fail2:
	nfe_free_tx_ring(sc, &sc->txq);
fail1:
	nfe_free_rx_ring(sc, &sc->rxq);

	return;
}

void
nfe_miibus_statchg(struct device *dev)
{
	struct nfe_softc *sc = (struct nfe_softc *) dev;
	struct mii_data	*mii = &sc->sc_mii;
	uint32_t reg;

	reg = NFE_READ(sc, NFE_PHY_INT);

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
		reg |= NFE_PHY_1000T;
		break;
	case IFM_100_TX:
		reg |= NFE_PHY_100TX;
		break;
	}
	NFE_WRITE(sc, NFE_PHY_INT, reg);
}

int
nfe_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct nfe_softc *sc = (struct nfe_softc *) dev;
	uint32_t r;

	r = NFE_READ(sc, NFE_PHY_CTL);
	if (r & NFE_PHY_BUSY) {
		NFE_WRITE(sc, NFE_PHY_CTL, NFE_PHY_BUSY);
		delay(100);
	}

	NFE_WRITE(sc, NFE_PHY_CTL, reg | (phy << NFE_PHYADD_SHIFT));
	delay(1000);
	r = NFE_READ(sc, NFE_PHY_DATA);
	if (r != 0xffffffff && r != 0)
		sc->phyaddr = phy;

	DPRINTFN(2, ("nfe mii read phy %d reg 0x%x ret 0x%x\n", phy, reg, r));

	return (r);
}

void
nfe_miibus_writereg(struct device *dev, int phy, int reg, int data)
{
	struct nfe_softc *sc = (struct nfe_softc *) dev;
	uint32_t r;

	r = NFE_READ(sc, NFE_PHY_CTL);
	if (r & NFE_PHY_BUSY) {
		NFE_WRITE(sc, NFE_PHY_CTL, NFE_PHY_BUSY);
		delay(100);
	}

	NFE_WRITE(sc, NFE_PHY_DATA, data);
	r = reg | (phy << NFE_PHYADD_SHIFT) | NFE_PHY_WRITE;
	NFE_WRITE(sc, NFE_PHY_CTL, r);
}

int
nfe_intr(void *arg)
{
	struct nfe_softc *sc = arg;
	uint32_t r;

	/* disable interrupts */
	NFE_WRITE(sc, NFE_IRQ_MASK, 0);
	
	r = NFE_READ(sc, NFE_IRQ_STATUS) & 0x1ff;

	if (r == 0) {
		NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_WANTED | NFE_IRQ_TIMER);
		return(0);
	}

	if (r & NFE_IRQ_RX)
		nfe_rxintr(sc);

	if (r & NFE_IRQ_TX_DONE)
		nfe_txintr(sc);

	DPRINTF(("nfe_intr: interrupt register %x", r));

	/* re-enable interrupts */
	NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_WANTED | NFE_IRQ_TIMER);
	
	return (1);
}

int
nfe_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *) data;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ETHERMTU)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu)
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, cmd);
		break;
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			nfe_init(ifp);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			nfe_init(ifp);
			break;
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (cmd == SIOCADDMULTI)
			error = ether_addmulti(ifr, &sc->sc_arpcom);
		else
			error = ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				nfe_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				nfe_update_promisc(sc);
			else
				nfe_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				nfe_stop(ifp, 1);
		}
		break;
	default:
		error = EINVAL;
	}
	splx(s);

	return (error);
}

void
nfe_start(struct ifnet *ifp)
{
}

void
nfe_stop(struct ifnet *ifp, int disable)
{
	struct nfe_softc *sc = ifp->if_softc;

	timeout_del(&sc->sc_timeout);

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* abort Tx */
	NFE_WRITE(sc, NFE_TX_CTL, 0);

	/* disable Rx */
	NFE_WRITE(sc, NFE_RX_CTL, 0);

	/* disable interrupts */
	NFE_WRITE(sc, NFE_IRQ_MASK, 0);

	/* reset Tx and Rx rings */
	nfe_reset_tx_ring(sc, &sc->txq);
	nfe_reset_rx_ring(sc, &sc->rxq);
}

void
nfe_watchdog(struct ifnet *ifp)
{
}

int
nfe_init(struct ifnet *ifp)
{
	struct nfe_softc	*sc = ifp->if_softc;
	int r;

	nfe_stop(ifp, 0);

	NFE_WRITE(sc, NFE_TX_UNK, 0);

	NFE_WRITE(sc, NFE_RXTX_CTL, NFE_RXTX_RESET | NFE_RXTX_BIT2);
	delay(10);

	r = NFE_RXTX_BIT2;
	if (sc->sc_flags & NFE_40BIT_ADDR)
		r |= NFE_RXTX_V3MAGIC|NFE_RXTX_RXCHECK;
	else if (sc->sc_flags & NFE_JUMBO_SUP)
		r |= NFE_RXTX_V2MAGIC|NFE_RXTX_RXCHECK;

	NFE_WRITE(sc, NFE_RXTX_CTL, r);

	NFE_WRITE(sc, NFE_SETUP_R6, 0);

	/* XXX set MAC address */

	/* Tell MAC where rings are in memory */
	NFE_WRITE(sc, NFE_RX_RING_ADDR, sc->rxq.physaddr);
	NFE_WRITE(sc, NFE_TX_RING_ADDR, sc->txq.physaddr);

	NFE_WRITE(sc, NFE_SETUP_R1, NFE_R1_MAGIC);
	NFE_WRITE(sc, NFE_SETUP_R2, NFE_R2_MAGIC);
	NFE_WRITE(sc, NFE_TIMER_INT, 970);		/* XXX Magic */

	NFE_WRITE(sc, NFE_SETUP_R4, NFE_R4_MAGIC);
	NFE_WRITE(sc, NFE_WOL_CTL, NFE_WOL_MAGIC);

	/* enable Rx */
	NFE_WRITE(sc, NFE_RX_CTL, NFE_RX_START);

	nfe_setmulti(sc);

	/* enable interrupts */
	NFE_WRITE(sc, NFE_IRQ_MASK, NFE_IRQ_RXERR | NFE_IRQ_RX |
	    NFE_IRQ_RX_NOBUF | NFE_IRQ_TXERR | NFE_IRQ_TX_DONE | NFE_IRQ_LINK |
	    NFE_IRQ_TXERR2);

	mii_mediachg(&sc->sc_mii);

	timeout_set(&sc->sc_timeout, nfe_tick, sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return (0);
}

void
nfe_reset(struct nfe_softc *sc)
{
	printf("nfe_reset!\n");
}

int
nfe_alloc_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring, int count)
{
	struct arpcom *ac = &sc->sc_arpcom;
	struct ifnet *ifp = &ac->ac_if;
	struct nfe_rx_data *data;
	struct nfe_desc *desc_v1;
	struct nfe_desc_v3 *desc_v3;
	void **desc;
	int i, nsegs, error, descsize;
	int bufsz = ifp->if_mtu + 64;

	if (sc->sc_flags & NFE_40BIT_ADDR) {
		desc = (void **)&ring->desc_v3;
		descsize = sizeof(struct nfe_desc_v3);
	} else {
		desc = (void **)&ring->desc_v1;	
		descsize = sizeof(struct nfe_desc);
	}

	ring->count = count;
	ring->cur = ring->next = 0;

	error = bus_dmamap_create(sc->sc_dmat, count * descsize, 1,
	    count * descsize, 0, BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, count * descsize,
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * descsize, (caddr_t *)desc, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, *desc,
	    count * descsize, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	memset(*desc, 0, count * descsize);
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof (struct nfe_rx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	memset(ring->data, 0, count * sizeof (struct nfe_rx_data));
	for (i = 0; i < count; i++) {
		data = &sc->rxq.data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &data->map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		MCLGET(data->m, M_DONTWAIT);
		if (!(data->m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		if (sc->sc_flags & NFE_40BIT_ADDR) {
			desc_v3 = &sc->rxq.desc_v3[i];
			desc_v3->physaddr[0] =
#if 0
			(htole64(data->map->dm_segs->ds_addr) >> 32) & 0xffffffff;
#endif
			    0;
			desc_v3->physaddr[1] =
			    htole64(data->map->dm_segs->ds_addr) & 0xffffffff;
			desc_v3->flags = htole32(bufsz | NFE_RX_READY);
		} else {
			desc_v1 = &sc->rxq.desc_v1[i];
			desc_v1->physaddr =
			    htole32(data->map->dm_segs->ds_addr);
			desc_v1->flags = htole32(bufsz | NFE_RX_READY);
		}
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:	nfe_free_rx_ring(sc, ring);
	return error;
}

void
nfe_reset_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	struct arpcom *ac = &sc->sc_arpcom;
	struct ifnet *ifp = &ac->ac_if;
	int i, bufsz = ifp->if_mtu + 64;

	for (i = 0; i < ring->count; i++) {
		if (sc->sc_flags & NFE_40BIT_ADDR) {
			ring->desc_v3[i].flags =
			    htole32(bufsz | NFE_RX_READY);
		} else {
			ring->desc_v1[i].flags =
			    htole32(bufsz | NFE_RX_READY);
		}
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
}

void
nfe_free_rx_ring(struct nfe_softc *sc, struct nfe_rx_ring *ring)
{
	struct nfe_rx_data *data;
	void *desc;
	int i, descsize;

	if (sc->sc_flags & NFE_40BIT_ADDR) {
		desc = ring->desc_v3;
		descsize = sizeof(struct nfe_desc_v3);
	} else {
		desc = ring->desc_v1;	
		descsize = sizeof(struct nfe_desc);
	}

	if (desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)desc,
		    ring->count * descsize);
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

int
nfe_rxintr(struct nfe_softc *sc)
{
	printf("nfe_rxintr!\n");
	return (0);
}

int
nfe_alloc_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring, int count)
{
	int i, nsegs, error;
	void **desc;
	int descsize;

	if (sc->sc_flags & NFE_40BIT_ADDR) {
		desc = (void **)&ring->desc_v3;
		descsize = sizeof(struct nfe_desc_v3);
	} else {
		desc = (void **)&ring->desc_v1;	
		descsize = sizeof(struct nfe_desc);
	}

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = 0;

	error = bus_dmamap_create(sc->sc_dmat, count * descsize, 1,
	    count * descsize, 0, BUS_DMA_NOWAIT, &ring->map);
	    
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, count * descsize,
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * sizeof(struct nfe_desc), (caddr_t *)desc,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, *desc,
	    count * descsize, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	memset(desc, 0, count * sizeof(struct nfe_desc));
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof(struct nfe_tx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	memset(ring->data, 0, count * sizeof (struct nfe_tx_data));
	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    NFE_MAX_SCATTER, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &ring->data[i].map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}

	return 0;

fail:	nfe_free_tx_ring(sc, ring);
	return error;
}

void
nfe_reset_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	void *desc;
	struct nfe_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		if (sc->sc_flags & NFE_40BIT_ADDR)
			desc = &ring->desc_v3[i];
		else
			desc = &ring->desc_v1[i];	

		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}

		if (sc->sc_flags & NFE_40BIT_ADDR)
			((struct nfe_desc_v3 *)desc)->flags = 0;
		else
			((struct nfe_desc *)desc)->flags = 0;
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->queued = 0;
	ring->cur = ring->next = 0;
}

void
nfe_free_tx_ring(struct nfe_softc *sc, struct nfe_tx_ring *ring)
{
	struct nfe_tx_data *data;
	void *desc;
	int i, descsize;

	if (sc->sc_flags & NFE_40BIT_ADDR) {
		desc = ring->desc_v3;
		descsize = sizeof(struct nfe_desc_v3);
	} else {
		desc = ring->desc_v1;	
		descsize = sizeof(struct nfe_desc);
	}

	if (desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)desc,
		    ring->count * descsize);
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

int
nfe_txintr(struct nfe_softc *sc)
{
	printf("nfe_txintr!\n");
	return (0);
}

int 
nfe_mediachange(struct ifnet *ifp)
{
	struct nfe_softc *sc = ifp->if_softc;
	struct mii_data	*mii = &sc->sc_mii;
	int val;

	DPRINTF(("nfe_mediachange\n"));
#if 0
	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
		/* XXX? */
	else
#endif
		val = 0;

	val |= NFE_MEDIA_SET;

	switch (IFM_SUBTYPE(mii->mii_media_active)) {
	case IFM_1000_T:
		val |= NFE_MEDIA_1000T;
		break;
	case IFM_100_TX:
		val |= NFE_MEDIA_100TX;
		break;
	case IFM_10_T:
		val |= NFE_MEDIA_10T;
		break;
	}

	DPRINTF(("nfe_miibus_statchg: val=0x%x\n", val));
	NFE_WRITE(sc, NFE_LINKSPEED, val);
	return (0);
}

void
nfe_mediastatus(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct nfe_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

void
nfe_setmulti(struct nfe_softc *sc)
{
	NFE_WRITE(sc, NFE_MULT_ADDR1, 0x01);
	NFE_WRITE(sc, NFE_MULT_ADDR2, 0);
	NFE_WRITE(sc, NFE_MULT_MASK1, 0);
	NFE_WRITE(sc, NFE_MULT_MASK2, 0);
#ifdef notyet 
	NFE_WRITE(sc, NFE_MULTI_FLAGS, NFE_MC_ALWAYS | NFE_MC_MYADDR);
#else
	NFE_WRITE(sc, NFE_MULTI_FLAGS, NFE_MC_ALWAYS | NFE_MC_PROMISC);
#endif
}

void
nfe_update_promisc(struct nfe_softc *sc)
{
}

void
nfe_tick(void *arg)
{
	struct nfe_softc *sc = arg;
	int s;

	s = splnet();	
	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add(&sc->sc_timeout, hz);
}
