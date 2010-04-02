/*	$OpenBSD: if_se.c,v 1.2 2010/04/02 22:42:55 jsg Exp $	*/

/*-
 * Copyright (c) 2009, 2010 Christopher Zimmermann <madroach@zakweb.de>
 * Copyright (c) 2007, 2008 Alexander Pohoyda <alexander.pohoyda@gmx.net>
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL AUTHORS OR
 * THE VOICES IN THEIR HEADS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/cdefs.h>

/*
 * SiS 190 Fast Ethernet PCI NIC driver.
 *
 * Adapted to SiS 190 NIC by Alexander Pohoyda based on the original
 * SiS 900 driver by Bill Paul, using SiS 190/191 Solaris driver by
 * Masayuki Murayama and SiS 190/191 GNU/Linux driver by K.M. Liu
 * <kmliu@sis.com>.  Thanks to Pyun YongHyeon <pyunyh@gmail.com> for
 * review and very useful comments.
 *
 * Ported to OpenBSD by Christopher Zimmermann 2009/10
 *
 * It should be easy to adapt this driver to SiS 191 Gigabit Ethernet
 * PCI NIC.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/socket.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>

#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net/bpf.h>

#include <dev/mii/miivar.h>

#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include "if_sereg.h"

/*
 * Various supported device vendors/types and their names.
 */
const struct pci_matchid se_devices[] = {
	{ PCI_VENDOR_SIS, PCI_PRODUCT_SIS_190 },
	/* Gigabit variant not supported yet. */
	/*{ PCI_VENDOR_SIS, PCI_PRODUCT_SIS_191 }*/
};

int se_probe(struct device *, void *, void *);
void se_attach(struct device *, struct device *, void *);

struct cfattach se_ca = {
	sizeof(struct se_softc), se_probe, se_attach
};

struct cfdriver se_cd = {
	0, "se", DV_IFNET
};

int	se_get_mac_addr_cmos	(struct se_softc *, caddr_t);
int	se_get_mac_addr_eeprom	(struct se_softc *, caddr_t);
int	se_isabridge_match	(struct pci_attach_args *);
uint16_t se_read_eeprom	(struct se_softc *, int);
void	miibus_cmd		(struct se_softc *, u_int32_t);
int	se_miibus_readreg	(struct device *, int, int);
void	se_miibus_writereg	(struct device *, int, int, int);
void	se_miibus_statchg	(struct device *);

int	se_ifmedia_upd		(struct ifnet *);
void	se_ifmedia_sts		(struct ifnet *, struct ifmediareq *);

int	se_ioctl		(struct ifnet *, u_long, caddr_t);

void	se_setmulti		(struct se_softc *);
uint32_t se_mchash		(struct se_softc *, const uint8_t *);

int	se_newbuf		(struct se_softc *, u_int32_t,
 				struct mbuf *);
int	se_encap		(struct se_softc *,
 				struct mbuf *, u_int32_t *);
int	se_init		(struct ifnet *);
int	se_list_rx_init	(struct se_softc *);
int	se_list_rx_free	(struct se_softc *);
int	se_list_tx_init	(struct se_softc *);
int	se_list_tx_free	(struct se_softc *);

int	se_intr		(void *);
void	se_rxeof		(struct se_softc *);
void	se_txeof		(struct se_softc *);

void	se_tick		(void *);
void	se_watchdog		(struct ifnet *);

void	se_start		(struct ifnet *);
void	se_reset		(struct se_softc *);
void	se_stop		(struct se_softc *);
void	se_shutdown		(void *);


/*
 * Read a sequence of words from the EEPROM.
 */
uint16_t
se_read_eeprom(sc, offset)
	struct se_softc	*sc;
	int			offset;
{
	uint32_t	ret, val, i;
        int             s;

	KASSERT(offset <= EI_OFFSET);

	s = splnet();

	val = EI_REQ | EI_OP_RD | (offset << EI_OFFSET_SHIFT);
	CSR_WRITE_4(sc, ROMInterface, val);
	DELAY(500);

	for (i = 0; ((ret = CSR_READ_4(sc, ROMInterface)) & EI_REQ) != 0; i++) {
	    if (i > 1000) {
		/* timeout */
		printf("EEPROM read timeout %d\n", i);
		splx(s);
		return (0xffff);
	    }
	    DELAY(100);
	}

	splx(s);
	return ((ret & EI_DATA) >> EI_DATA_SHIFT);
}

int
se_isabridge_match(struct pci_attach_args *pa)
{
	const struct pci_matchid device_ids[] = {
	    { PCI_VENDOR_SIS, PCI_PRODUCT_SIS_965},
	    { PCI_VENDOR_SIS, PCI_PRODUCT_SIS_966},
	    { PCI_VENDOR_SIS, PCI_PRODUCT_SIS_968},
	};

	return (pci_matchbyid(pa, device_ids,
		    sizeof(device_ids)/sizeof(device_ids[0])));
}

int
se_get_mac_addr_cmos(sc, dest)
	struct se_softc	*sc;
	caddr_t			dest;
{
	struct pci_attach_args	isa_bridge;
	u_int32_t reg, tmp;
	int i, s;

	if (!pci_find_device(&isa_bridge, se_isabridge_match)) {
	    printf("Could not find ISA bridge to retrieve MAC address.\n");
	    return -1;
	}

	s = splnet();

	/* Enable port 78h & 79h to access APC Registers.
	 * Taken from linux driver. */
	tmp = pci_conf_read(isa_bridge.pa_pc, isa_bridge.pa_tag, 0x48);
	reg = tmp & ~0x02;
	pci_conf_write(isa_bridge.pa_pc, isa_bridge.pa_tag, 0x48, reg);
	delay(50);
	reg = pci_conf_read(isa_bridge.pa_pc, isa_bridge.pa_tag, 0x48);

	for (i = 0; i < ETHER_ADDR_LEN; i++) {
	    bus_space_write_1(isa_bridge.pa_iot, 0x0, 0x78, 0x9 + i);
	    *(dest + i) = bus_space_read_1(isa_bridge.pa_iot, 0x0, 0x79);
	}

	bus_space_write_1(isa_bridge.pa_iot, 0x0, 0x78, 0x12);
	reg = bus_space_read_1(isa_bridge.pa_iot, 0x0, 0x79);

	pci_conf_write(isa_bridge.pa_pc, isa_bridge.pa_tag, 0x48, tmp);

	/* XXX: pci_dev_put(isa_bridge) ? */
	splx(s);
	return 0;
}

int
se_get_mac_addr_eeprom(sc, dest)
	struct se_softc	*sc;
	caddr_t			dest;
{
	uint16_t	val, i;

	val = se_read_eeprom(sc, EEPROMSignature);
	if (val == 0xffff || val == 0x0000)
	    return (1);

	for (i = 0; i < ETHER_ADDR_LEN; i += 2) {
	    val = se_read_eeprom(sc, EEPROMMACAddr + i/2);
	    dest[i + 0] = (uint8_t) val;
	    dest[i + 1] = (uint8_t) (val >> 8);
	}

	return (0);
}

void
miibus_cmd(sc, ctl)
    	struct se_softc	*sc;
	u_int32_t		ctl;
{
	uint32_t	i;
        int             s;

	s = splnet();
	CSR_WRITE_4(sc, GMIIControl, ctl);
	DELAY(10);

	for (i = 0; (CSR_READ_4(sc, GMIIControl) & GMI_REQ) != 0; i++)
	{
	    if (i > 1000) {
		/* timeout */
		printf("MIIBUS timeout\n");
		splx(s);
		return;
	    }
	    DELAY(100);
	}
        splx(s);
        return;
}

int
se_miibus_readreg(self, phy, reg)
	struct device		*self;
	int			phy, reg;
{
	struct se_softc	*sc = (struct se_softc *)self;
	miibus_cmd(sc, (phy << GMI_PHY_SHIFT) |
		(reg << GMI_REG_SHIFT) | GMI_OP_RD | GMI_REQ);
	return ((CSR_READ_4(sc, GMIIControl) & GMI_DATA) >> GMI_DATA_SHIFT);
}

void
se_miibus_writereg(self, phy, reg, data)
	struct device		*self;
	int			phy, reg, data;
{
	struct se_softc	*sc = (struct se_softc *)self;
	miibus_cmd(sc, (((uint32_t) data) << GMI_DATA_SHIFT) |
		(((uint32_t) phy) << GMI_PHY_SHIFT) |
		(reg << GMI_REG_SHIFT) | GMI_OP_WR | GMI_REQ);
}

void
se_miibus_statchg(self)
	struct device		*self;
{
	struct se_softc	*sc = (struct se_softc *)self;

	se_init(sc->se_ifp);
}

u_int32_t
se_mchash(sc, addr)
	struct se_softc	*sc;
	const uint8_t		*addr;
{
	return (ether_crc32_be(addr, ETHER_ADDR_LEN) >> 26);
}

void
se_setmulti(sc)
	struct se_softc	*sc;
{
	struct ifnet		*ifp = sc->se_ifp;
	struct arpcom		*ac = &sc->arpcom;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	uint32_t		hashes[2] = { 0, 0 }, rxfilt;
	//struct ifmultiaddr	*ifma;

	rxfilt = AcceptMyPhys | 0x0052;

	if (ifp->if_flags & IFF_PROMISC) {
	    rxfilt |= AcceptAllPhys;
	} else {
	    rxfilt &= ~AcceptAllPhys;
	}

	if (ifp->if_flags & IFF_BROADCAST) {
	    rxfilt |= AcceptBroadcast;
	} else {
	    rxfilt &= ~AcceptBroadcast;
	}

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
allmulti:
	    rxfilt |= AcceptMulticast;
	    CSR_WRITE_2(sc, RxMacControl, rxfilt);
	    CSR_WRITE_4(sc, RxHashTable, 0xFFFFFFFF);
	    CSR_WRITE_4(sc, RxHashTable2, 0xFFFFFFFF);
	    return;
	}

	/* now program new ones */
	//TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
	    if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
		ifp->if_flags |= IFF_ALLMULTI;
		goto allmulti;
	    }

	    int bit_nr = se_mchash(sc, LLADDR((struct sockaddr_dl *)
				    enm->enm_addrlo));
	    hashes[bit_nr >> 5] |= 1 << (bit_nr & 31);
	    rxfilt |= AcceptMulticast;
	    ETHER_NEXT_MULTI(step, enm);
	}

	CSR_WRITE_2(sc, RxMacControl, rxfilt);
	CSR_WRITE_4(sc, RxHashTable, hashes[0]);
	CSR_WRITE_4(sc, RxHashTable2, hashes[1]);
}

void
se_reset(sc)
	struct se_softc	*sc;
{
	CSR_WRITE_4(sc, IntrMask, 0);
	CSR_WRITE_4(sc, IntrStatus, 0xffffffff);

	CSR_WRITE_4(sc, TxControl, 0x00001a00);
	CSR_WRITE_4(sc, RxControl, 0x00001a1d);

	CSR_WRITE_4(sc, IntrControl, 0x8000);
	SE_PCI_COMMIT();
	DELAY(100);
	CSR_WRITE_4(sc, IntrControl, 0x0);

	CSR_WRITE_4(sc, IntrMask, 0);
	CSR_WRITE_4(sc, IntrStatus, 0xffffffff);
}

/*
 * Probe for an SiS chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
int
se_probe(parent, match, aux)
    	struct device		*parent;
	void			*match;
	void			*aux;
{
	return (pci_matchbyid((struct pci_attach_args *)aux, se_devices,
	    sizeof(se_devices)/sizeof(se_devices[0])));
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
se_attach(parent, self, aux)
	struct device		*parent;
	struct device		*self;
	void			*aux;
{
	const char		*intrstr = NULL;
	struct se_softc	*sc = (struct se_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;
	bus_size_t		size;
	struct ifnet		*ifp;
	bus_dma_segment_t	seg;
	int			nseg;
	struct se_list_data	*ld;
	struct se_chain_data *cd;
	int			i, error = 0;

	ld = &sc->se_ldata;
	cd = &sc->se_cdata;

	/* TODO: What about power management ? */

	/*
	 * Handle power management nonsense.
	 */
#if 0
/* power management registers */
#define SIS_PCI_CAPID		0x50 /* 8 bits */
#define SIS_PCI_NEXTPTR		0x51 /* 8 bits */
#define SIS_PCI_PWRMGMTCAP	0x52 /* 16 bits */
#define SIS_PCI_PWRMGMTCTRL	0x54 /* 16 bits */

#define SIS_PSTATE_MASK		0x0003
#define SIS_PSTATE_D0		0x0000
#define SIS_PSTATE_D1		0x0001
#define SIS_PSTATE_D2		0x0002
#define SIS_PSTATE_D3		0x0003
#define SIS_PME_EN		0x0010
#define SIS_PME_STATUS		0x8000
#define SIS_PCI_INTLINE		0x3C
#define SIS_PCI_LOMEM		0x14
	command = pci_conf_read(pc, pa->pa_tag, SIS_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {

		command = pci_conf_read(pc, pa->pa_tag, SIS_PCI_PWRMGMTCTRL);
		if (command & SIS_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(pc, pa->pa_tag, SIS_PCI_LOIO);
			membase = pci_conf_read(pc, pa->pa_tag, SIS_PCI_LOMEM);
			irq = pci_conf_read(pc, pa->pa_tag, SIS_PCI_INTLINE);

			/* Reset the power state. */
			printf("%s: chip is in D%d power mode -- setting to D0\n",
			    sc->sc_dev.dv_xname, command & SIS_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(pc, pa->pa_tag, SIS_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(pc, pa->pa_tag, SIS_PCI_LOIO, iobase);
			pci_conf_write(pc, pa->pa_tag, SIS_PCI_LOMEM, membase);
			pci_conf_write(pc, pa->pa_tag, SIS_PCI_INTLINE, irq);
		}
	}
#endif

	/*
	 * Map control/status registers.
	 */

	/* Map IO */
	if ((error = pci_mapreg_map(
			pa,
			SE_PCI_LOMEM,
			PCI_MAPREG_TYPE_MEM, 0,
			&sc->se_btag,
			&sc->se_bhandle,
			NULL,
			&size,
			0)))
	{
	    printf(": can't map i/o space (code %d)\n", error);
	    return;
 	}

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
	    printf(": couldn't map interrupt\n");
	    goto intfail;
	}
	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, se_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
	    printf(": couldn't establish interrupt");
	    if (intrstr != NULL)
		printf(" at %s", intrstr);
	    printf("\n");
	    goto intfail;
	}

	/* Reset the adapter. */
	se_reset(sc);

	/*
	 * Get MAC address from the EEPROM.
	 */
	if (se_get_mac_addr_eeprom(sc, (caddr_t)&sc->arpcom.ac_enaddr) &&
	    se_get_mac_addr_cmos(sc, (caddr_t)&sc->arpcom.ac_enaddr)
	   )
	    goto fail;

	printf(": %s, address %s\n", intrstr,
	    ether_sprintf(sc->arpcom.ac_enaddr));

	/*
	 * Now do all the DMA mapping stuff
	 */

	sc->se_tag = pa->pa_dmat;

	/* First create TX/RX busdma maps. */
	for (i = 0; i < SE_RX_RING_CNT; i++) {
	    error = bus_dmamap_create(sc->se_tag, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &cd->se_rx_map[i]);
	    if (error) {
		printf("cannot init the RX map array!\n");
		goto fail;
	    }
	}

	for (i = 0; i < SE_TX_RING_CNT; i++) {
	    error = bus_dmamap_create(sc->se_tag, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &cd->se_tx_map[i]);
	    if (error) {
		printf("cannot init the TX map array!\n");
		goto fail;
	    }
	}


	/*
	 * Now allocate a tag for the DMA descriptor lists and a chunk
	 * of DMA-able memory based on the tag.  Also obtain the physical
	 * addresses of the RX and TX ring, which we'll need later.
	 * All of our lists are allocated as a contiguous block
	 * of memory.
	 */

	/* RX */

	error = bus_dmamem_alloc(sc->se_tag, SE_RX_RING_SZ, PAGE_SIZE, 0,
		&seg, 1, &nseg, BUS_DMA_NOWAIT);
	if (error) {
	    printf("no memory for rx list buffers!\n");
	    goto fail;
	}

	error = bus_dmamem_map(sc->se_tag, &seg, nseg, SE_RX_RING_SZ,
		(caddr_t *)&ld->se_rx_ring, BUS_DMA_NOWAIT);
	if (error) {
	    printf("can't map rx list buffers!\n");
	    goto fail;
	}

	error = bus_dmamap_create(sc->se_tag, SE_RX_RING_SZ, 1,
		SE_RX_RING_SZ, 0, BUS_DMA_NOWAIT, &ld->se_rx_dmamap);
	if (error) {
	    printf("can't alloc rx list map!\n");
	    goto fail;
	}

	error = bus_dmamap_load(sc->se_tag, ld->se_rx_dmamap,
		(caddr_t)ld->se_rx_ring, SE_RX_RING_SZ,
		NULL, BUS_DMA_NOWAIT);
	if (error) {
	    printf("can't load rx ring mapping!\n");
	    bus_dmamem_unmap(sc->se_tag,
		    (caddr_t)ld->se_rx_ring, SE_RX_RING_SZ);
	    bus_dmamap_destroy(sc->se_tag, ld->se_rx_dmamap);
	    bus_dmamem_free(sc->se_tag, &seg, nseg);
	    goto fail;
	}

	/* TX */

	error = bus_dmamem_alloc(sc->se_tag, SE_TX_RING_SZ, PAGE_SIZE, 0,
		&seg, 1, &nseg, BUS_DMA_NOWAIT);
	if (error) {
	    printf("no memory for tx list buffers!\n");
	    goto fail;
	}

	error = bus_dmamem_map(sc->se_tag, &seg, nseg, SE_TX_RING_SZ,
		(caddr_t *)&ld->se_tx_ring, BUS_DMA_NOWAIT);
	if (error) {
	    printf("can't map tx list buffers!\n");
	    goto fail;
	}

	error = bus_dmamap_create(sc->se_tag, SE_TX_RING_SZ, 1,
		SE_TX_RING_SZ, 0, BUS_DMA_NOWAIT, &ld->se_tx_dmamap);
	if (error) {
	    printf("can't alloc tx list map!\n");
	    goto fail;
	}

	error = bus_dmamap_load(sc->se_tag, ld->se_tx_dmamap,
		(caddr_t)ld->se_tx_ring, SE_TX_RING_SZ,
		NULL, BUS_DMA_NOWAIT);
	if (error) {
	    printf("can't load tx ring mapping!\n");
	    bus_dmamem_unmap(sc->se_tag,
		    (caddr_t)ld->se_tx_ring, SE_TX_RING_SZ);
	    bus_dmamap_destroy(sc->se_tag, ld->se_tx_dmamap);
	    bus_dmamem_free(sc->se_tag, &seg, nseg);
	    goto fail;
	}

	timeout_set(&sc->se_timeout, se_tick, sc);

	sc->se_ifp = ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = se_ioctl;
	ifp->if_start = se_start;
	ifp->if_watchdog = se_watchdog;
	ifp->if_baudrate = IF_Mbps(100);
	ifp->if_init = se_init;
	IFQ_SET_MAXLEN(&ifp->if_snd, SE_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/*
	 * Do MII setup.
	 */

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = se_miibus_readreg;
	sc->sc_mii.mii_writereg = se_miibus_writereg;
	sc->sc_mii.mii_statchg = se_miibus_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0,
			se_ifmedia_upd,se_ifmedia_sts);
	mii_phy_probe(self, &sc->sc_mii, 0xffffffff);

	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
	    ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
	    ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
	    ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	/*
	 * Call MI attach routine.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	shutdownhook_establish(se_shutdown, sc);

	return;

fail:
	pci_intr_disestablish(pc, sc->sc_ih);

intfail:
	bus_space_unmap(sc->se_btag, sc->se_bhandle, size);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
void
se_shutdown(v)
	void			*v;
{
	struct se_softc	*sc = (struct se_softc *)v;

	se_reset(sc);
	se_stop(sc);
}



/*
 * Initialize the TX descriptors.
 */
int
se_list_tx_init(sc)
	struct se_softc	*sc;
{
	struct se_list_data	*ld = &sc->se_ldata;
	struct se_chain_data	*cd = &sc->se_cdata;

	bzero(ld->se_tx_ring, SE_TX_RING_SZ);
	ld->se_tx_ring[SE_TX_RING_CNT - 1].se_flags |= RING_END;
	cd->se_tx_prod = cd->se_tx_cons = cd->se_tx_cnt = 0;

	return (0);
}

int
se_list_tx_free(sc)
	struct se_softc	*sc;
{
	struct se_chain_data	*cd = &sc->se_cdata;
	int		i;

	for (i = 0; i < SE_TX_RING_CNT; i++) {
	    if (cd->se_tx_mbuf[i] != NULL) {
		bus_dmamap_unload(sc->se_tag, cd->se_tx_map[i]);
		m_free(cd->se_tx_mbuf[i]);
		cd->se_tx_mbuf[i] = NULL;
	    }
	}

	return (0);
}

/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * has RING_END flag set.
 */
int
se_list_rx_init(sc)
	struct se_softc	*sc;
{
	struct se_list_data	*ld = &sc->se_ldata;
	struct se_chain_data	*cd = &sc->se_cdata;
	int		i;

	bzero(ld->se_rx_ring, SE_RX_RING_SZ);
	for (i = 0; i < SE_RX_RING_CNT; i++) {
	    if (se_newbuf(sc, i, NULL) == ENOBUFS) {
		printf("unable to allocate MBUFs, %d\n", i);
		return (ENOBUFS);
	    }
	}

	ld->se_rx_ring[SE_RX_RING_CNT - 1].se_flags |= RING_END;
	cd->se_rx_prod = 0;

	return (0);
}

int
se_list_rx_free(sc)
	struct se_softc	*sc;
{
	struct se_chain_data	*cd = &sc->se_cdata;
	int		i;

	for (i = 0; i < SE_RX_RING_CNT; i++) {
	    if (cd->se_rx_mbuf[i] != NULL) {
		bus_dmamap_unload(sc->se_tag, cd->se_rx_map[i]);
		m_free(cd->se_rx_mbuf[i]);
		cd->se_rx_mbuf[i] = NULL;
	    }
	}

	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
int
se_newbuf(sc, i, m)
	struct se_softc	*sc;
	u_int32_t		i;
	struct mbuf		*m;
{
	struct se_list_data	*ld = &sc->se_ldata;
	struct se_chain_data	*cd = &sc->se_cdata;
        /*struct ifnet		*ifp = sc->se_ifp;*/
	int			error, alloc;

	if (m == NULL) {
	    m = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES);
	    if (m == NULL) {
		printf("unable to get new MBUF\n");
		return (ENOBUFS);
	    }
	    cd->se_rx_mbuf[i] = m;
	    alloc = 1;
	} else {
	    m->m_data = m->m_ext.ext_buf;
	    alloc = 0;
	}

	m->m_len = m->m_pkthdr.len = MCLBYTES;

	if (alloc) {
	    error = bus_dmamap_load_mbuf(sc->se_tag, cd->se_rx_map[i],
					 m, BUS_DMA_NOWAIT);
	    if (error) {
		printf("unable to map and load the MBUF\n");
		m_freem(m);
		return (ENOBUFS);
	    }
	}

	/* This is used both to initialize the newly created RX
	 * descriptor as well as for re-initializing it for reuse. */
	ld->se_rx_ring[i].se_sts_size = 0;
	ld->se_rx_ring[i].se_cmdsts =
		htole32(OWNbit | INTbit | IPbit | TCPbit | UDPbit);
	ld->se_rx_ring[i].se_ptr =
		htole32(cd->se_rx_map[i]->dm_segs[0].ds_addr);
	ld->se_rx_ring[i].se_flags =
		htole32(cd->se_rx_map[i]->dm_segs[0].ds_len)
		| (i == SE_RX_RING_CNT - 1 ? RING_END : 0);
	KASSERT(cd->se_rx_map[i]->dm_nsegs == 1);

	bus_dmamap_sync(sc->se_tag, cd->se_rx_map[i], 0,
			cd->se_rx_map[i]->dm_mapsize,
			BUS_DMASYNC_PREREAD);

	return (0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
void
se_rxeof(sc)
	struct se_softc	*sc;
{
        struct mbuf		*m, *m0;
        struct ifnet		*ifp = sc->se_ifp;
	struct se_list_data	*ld = &sc->se_ldata;
	struct se_chain_data	*cd = &sc->se_cdata;
	struct se_desc	*cur_rx;
	u_int32_t		i, rxstat, total_len = 0;

	for (i = cd->se_rx_prod; !SE_OWNDESC(&ld->se_rx_ring[i]);
	    SE_INC(i, SE_RX_RING_CNT))
	{
	    bus_dmamap_sync(sc->se_tag, cd->se_rx_map[i], 0,
			    cd->se_rx_map[i]->dm_mapsize,
			    BUS_DMASYNC_POSTREAD);

	    cur_rx = &ld->se_rx_ring[i];
	    rxstat = SE_RXSTATUS(cur_rx);
	    total_len = SE_RXSIZE(cur_rx);
	    m = cd->se_rx_mbuf[i];

	    /*
	     * If an error occurs, update stats, clear the
	     * status word and leave the mbuf cluster in place:
	     * it should simply get re-used next time this descriptor
	     * comes up in the ring.
	     */
	    if (rxstat & RX_ERR_BITS) {
		printf("error_bits=%#x\n", rxstat);
		ifp->if_ierrors++;
		/* TODO: better error differentiation */
		se_newbuf(sc, i, m);
		continue;
	    }

	    /* No errors; receive the packet. */
	    cd->se_rx_mbuf[i] = NULL; /* XXX neccessary? */
#ifndef __STRICT_ALIGNMENT
	    if (se_newbuf(sc, i, NULL) == 0) {
		m->m_pkthdr.len = m->m_len = total_len;
	    } else
#endif
	    {
		/* ETHER_ALIGN is 2 */
		m0 = m_devget(mtod(m, char *), total_len,
		    2, ifp, NULL);
		se_newbuf(sc, i, m);
		if (m0 == NULL) {
		    printf("unable to copy MBUF\n");
		    ifp->if_ierrors++;
		    continue;
		}
		m = m0;
	    }

	    ifp->if_ipackets++;
	    m->m_pkthdr.rcvif = ifp;

#if NBPFILTER > 0
	    if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

	    /* pass it on. */
	    ether_input_mbuf(ifp, m);
	}

	cd->se_rx_prod = i;
}


/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

void
se_txeof(sc)
	struct se_softc	*sc;
{
	struct ifnet		*ifp = sc->se_ifp;
	struct se_list_data	*ld = &sc->se_ldata;
	struct se_chain_data	*cd = &sc->se_cdata;
	struct se_desc	*cur_tx;
	u_int32_t		i, txstat;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	for (i = cd->se_tx_cons; cd->se_tx_cnt > 0 &&
	       !SE_OWNDESC(&ld->se_tx_ring[i]);
	    cd->se_tx_cnt--, SE_INC(i, SE_TX_RING_CNT))
	{
	    cur_tx = &ld->se_tx_ring[i];
	    txstat = letoh32(cur_tx->se_cmdsts);
	    bus_dmamap_sync(sc->se_tag, cd->se_tx_map[i], 0,
			    cd->se_tx_map[i]->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);

	    /* current slot is transferred now */

	    if (txstat & TX_ERR_BITS) {
		printf("error_bits=%#x\n", txstat);
		ifp->if_oerrors++;
		/* TODO: better error differentiation */
	    }

	    ifp->if_opackets++;
	    if (cd->se_tx_mbuf[i] != NULL) {
		bus_dmamap_unload(sc->se_tag, cd->se_tx_map[i]);
		m_free(cd->se_tx_mbuf[i]);
		cd->se_tx_mbuf[i] = NULL;
	    }
	    cur_tx->se_sts_size = 0;
	    cur_tx->se_cmdsts = 0;
	    cur_tx->se_ptr = 0;
	    cur_tx->se_flags &= RING_END;
	}

	if (i != cd->se_tx_cons) {
	    /* we freed up some buffers */
	    cd->se_tx_cons = i;
	    ifp->if_flags &= ~IFF_OACTIVE;
	}

	sc->se_watchdog_timer = (cd->se_tx_cnt == 0) ? 0 : 5;
}

void
se_tick(xsc)
	void			*xsc;
{
	struct se_softc	*sc = xsc;
	struct mii_data		*mii;
	struct ifnet		*ifp = sc->se_ifp;
        int                     s;

	s = splnet();

	sc->in_tick = 1;

	mii = &sc->sc_mii;
	mii_tick(mii);

	se_watchdog(ifp);

	if (!sc->se_link && mii->mii_media_status & IFM_ACTIVE &&
	    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE)
	{
	    sc->se_link++;
	    if (!IFQ_IS_EMPTY(&ifp->if_snd))
		se_start(ifp);
	}

	timeout_add_sec(&sc->se_timeout, 1);

	sc->in_tick = 0;

        splx(s);
        return;
}

int
se_intr(arg)
	void			*arg;
{
	struct se_softc	*sc = arg;
	struct ifnet		*ifp = sc->se_ifp;
	int			status;
	int			claimed = 0;

	if (sc->se_stopped)	/* Most likely shared interrupt */
	    return (claimed);

	DISABLE_INTERRUPTS(sc);

	for (;;) {
	    /* Reading the ISR register clears all interrupts. */
	    status = CSR_READ_4(sc, IntrStatus);
	    if ((status == 0xffffffff) || (status == 0x0))
		break;

	    claimed = 1; /* XXX just a guess to put this here */

	    CSR_WRITE_4(sc, IntrStatus, status);

	    if (status & TxQInt)
		se_txeof(sc);

	    if (status & RxQInt)
		se_rxeof(sc);
	}

	ENABLE_INTERRUPTS(sc);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
	    se_start(ifp);

	return (claimed);
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
int
se_encap(sc, m_head, txidx)
	struct se_softc	*sc;
	struct mbuf		*m_head;
	u_int32_t		*txidx;
{
	struct mbuf		*m;
	struct se_list_data	*ld = &sc->se_ldata;
	struct se_chain_data	*cd = &sc->se_cdata;
	int			error, i, cnt = 0;

	/*
	 * If there's no way we can send any packets, return now.
	 */
	if (SE_TX_RING_CNT - cd->se_tx_cnt < 2)
	    return (ENOBUFS);

#if 1
	if (m_defrag(m_head, M_DONTWAIT)) {
	    printf("unable to defragment MBUFs\n");
	    return (ENOBUFS);
	}
#else
	m = MCLGETI(NULL, M_DONTWAIT, NULL, MCLBYTES);
	if (m == NULL) {
	    printf("se_encap: unable to allocate MBUF\n");
	    return (ENOBUFS);
	}
	m_copydata(m_head, 0, m_head->m_pkthdr.len, mtod(m, caddr_t));
	m->m_pkthdr.len = m->m_len = m_head->m_pkthdr.len;
	map = cd->se_tx_map[i];
	error = bus_dmamap_load_mbuf(sc->se_tag, map,
				     m, BUS_DMA_NOWAIT);
	if (error) {
	    printf("unable to load the MBUF\n");
	    return (ENOBUFS);
	}
#endif
	
	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	i = *txidx;

	for (m = m_head; m != NULL; m = m->m_next) {
	    if (m->m_len == 0)
		continue;
	    if ((SE_TX_RING_CNT - (cd->se_tx_cnt + cnt)) < 2)
		return (ENOBUFS);
	    cd->se_tx_mbuf[i] = m;
	    error = bus_dmamap_load_mbuf(sc->se_tag, cd->se_tx_map[i],
					 m, BUS_DMA_NOWAIT);
	    if (error) {
		printf("unable to load the MBUF\n");
		return (ENOBUFS);
	    }

	    ld->se_tx_ring[i].se_sts_size =
		    htole32(cd->se_tx_map[i]->dm_segs->ds_len);
	    ld->se_tx_ring[i].se_cmdsts =
		    htole32(OWNbit | INTbit | PADbit | CRCbit | DEFbit);
	    ld->se_tx_ring[i].se_ptr =
		    htole32(cd->se_tx_map[i]->dm_segs->ds_addr);
	    ld->se_tx_ring[i].se_flags |=
		    htole32(cd->se_tx_map[i]->dm_segs->ds_len);
	    KASSERT(cd->se_tx_map[i]->dm_nsegs == 1);

	    bus_dmamap_sync(sc->se_tag, cd->se_tx_map[i], 0,
			    cd->se_tx_map[i]->dm_mapsize,
			    BUS_DMASYNC_PREWRITE);
	    SE_INC(i, SE_TX_RING_CNT);
	    cnt++;
	}

	if (m != NULL) {
	    printf("unable to encap all MBUFs\n");
	    return (ENOBUFS);
	}

	cd->se_tx_cnt += cnt;
	*txidx = i;

	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */
void
se_start(ifp)
	struct ifnet		*ifp;
{
	struct se_softc	*sc = ifp->if_softc;
	struct mbuf		*m_head = NULL;
	struct se_chain_data	*cd = &sc->se_cdata;
	u_int32_t		i, queued = 0;

	if (!sc->se_link) {
	    return;
	}

	if (ifp->if_flags & IFF_OACTIVE) {
	    return;
	}

	i = cd->se_tx_prod;

	while (cd->se_tx_mbuf[i] == NULL) {
	    IFQ_POLL(&ifp->if_snd, m_head);
	    if (m_head == NULL)
		break;

	    if (se_encap(sc, m_head, &i)) {
		ifp->if_flags |= IFF_OACTIVE;
		break;
	    }

	    /* now we are committed to transmit the packet */
	    IFQ_DEQUEUE(&ifp->if_snd, m_head);
	    queued++;

	    /*
	     * If there's a BPF listener, bounce a copy of this frame
	     * to him.
	     */
#if NBPFILTER > 0
	    if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif
	}

	if (queued) {
	    /* Transmit */
	    cd->se_tx_prod = i;
	    SE_SETBIT(sc, TxControl, CmdReset);

	    /*
	     * Set a timeout in case the chip goes out to lunch.
	     */
	    sc->se_watchdog_timer = 5;
	}
}

/* TODO: Find out right return codes */
int
se_init(ifp)
	struct ifnet		*ifp;
{
	struct se_softc	*sc = ifp->if_softc;
	int			s;

	s = splnet();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	se_stop(sc);
	sc->se_stopped = 0;

	/* Init circular RX list. */
	if (se_list_rx_init(sc) == ENOBUFS) {
	    printf("initialization failed: no "
		   "memory for rx buffers\n");
	    se_stop(sc);
	    splx(s);
	    return 1;
	}

	/* Init TX descriptors. */
	se_list_tx_init(sc);

	se_reset(sc);

	/*
	 * Load the address of the RX and TX lists.
	 */
	CSR_WRITE_4(sc, TxDescStartAddr,
			sc->se_ldata.se_tx_dmamap->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, RxDescStartAddr,
			sc->se_ldata.se_rx_dmamap->dm_segs[0].ds_addr);

/* 	CSR_WRITE_4(sc, PMControl, 0xffc00000); */
	CSR_WRITE_4(sc, Reserved2, 0);

	CSR_WRITE_4(sc, IntrStatus, 0xffffffff);
	DISABLE_INTERRUPTS(sc);


	/*
	 * Default is 100Mbps.
	 * A bit strange: 100Mbps is 0x1801 elsewhere -- FR 2005/06/09
	 */
	CSR_WRITE_4(sc, StationControl, 0x04001801); // 1901

	CSR_WRITE_4(sc, GMacIOCR, 0x0);
	CSR_WRITE_4(sc, GMacIOCTL, 0x0);

	CSR_WRITE_4(sc, TxMacControl, 0x2364);		/* 0x60 */
	CSR_WRITE_4(sc, TxMacTimeLimit, 0x000f);
	CSR_WRITE_4(sc, RGMIIDelay, 0x0);
	CSR_WRITE_4(sc, Reserved3, 0x0);

	CSR_WRITE_4(sc, RxWakeOnLan, 0x80ff0000);
	CSR_WRITE_4(sc, RxWakeOnLanData, 0x80ff0000);
	CSR_WRITE_4(sc, RxMPSControl, 0x0);
	CSR_WRITE_4(sc, Reserved4, 0x0);

	SE_PCI_COMMIT();

	/*
	 * Load the multicast filter.
	 */
	se_setmulti(sc);

	/*
	 * Enable interrupts.
	 */
	ENABLE_INTERRUPTS(sc);

	/* Enable receiver and transmitter. */
	SE_SETBIT(sc, TxControl, CmdTxEnb | CmdReset);
	SE_SETBIT(sc, RxControl, CmdRxEnb | CmdReset);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	if (!sc->in_tick)
	    timeout_add_sec(&sc->se_timeout, 1);

	splx(s);
	return 0;
}

/*
 * Set media options.
 */
int
se_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct se_softc	*sc = ifp->if_softc;
	struct mii_data		*mii;

	mii = &sc->sc_mii;
	sc->se_link = 0;
	if (mii->mii_instance) {
	    struct mii_softc	*miisc;
	    LIST_FOREACH(miisc, &mii->mii_phys, mii_list)
		mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return (0);
}

/*
 * Report current media status.
 */
void
se_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct se_softc	*sc = ifp->if_softc;
	struct mii_data		*mii;

	mii = &sc->sc_mii;
	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

int
se_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct se_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct mii_data		*mii;
	int			s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFFLAGS:
	    if (ifp->if_flags & IFF_UP)
		se_init(ifp);
	    else if (ifp->if_flags & IFF_RUNNING)
		se_stop(sc);
	    error = 0;
	    break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
	    se_setmulti(sc);
	    error = 0;
	    break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
	    mii = &sc->sc_mii;
	    error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
	    break;
	default:
	    error = ether_ioctl(ifp, &sc->arpcom, command, data);
	    break;
	}

	splx(s);
	return (error);
}

void
se_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct se_softc	*sc = ifp->if_softc;
        int                     s;

	if (sc->se_stopped)
	    return;

	if (sc->se_watchdog_timer == 0 || --sc->se_watchdog_timer >0)
	    return;

	printf("watchdog timeout\n");
	sc->se_ifp->if_oerrors++;

	s = splnet();
	se_stop(sc);
	se_reset(sc);
	se_init(ifp);

	if (!IFQ_IS_EMPTY(&sc->se_ifp->if_snd))
	    se_start(sc->se_ifp);

	splx(s);
        return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
se_stop(sc)
	struct se_softc	*sc;
{
	struct ifnet		*ifp = sc->se_ifp;

	if (sc->se_stopped)
	    return;

	sc->se_watchdog_timer = 0;

	timeout_del(&sc->se_timeout);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	DISABLE_INTERRUPTS(sc);

	CSR_WRITE_4(sc, IntrControl, 0x8000);
	SE_PCI_COMMIT();
	DELAY(100);
	CSR_WRITE_4(sc, IntrControl, 0x0);

	SE_CLRBIT(sc, TxControl, CmdTxEnb);
	SE_CLRBIT(sc, RxControl, CmdRxEnb);
	DELAY(100);
	CSR_WRITE_4(sc, TxDescStartAddr, 0);
	CSR_WRITE_4(sc, RxDescStartAddr, 0);

	sc->se_link = 0;

	/*
	 * Free data in the RX lists.
	 */
	se_list_rx_free(sc);

	/*
	 * Free the TX list buffers.
	 */
	se_list_tx_free(sc);

	sc->se_stopped = 1;
}
