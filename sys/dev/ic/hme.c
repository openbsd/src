/*	$OpenBSD: hme.c,v 1.20 2002/09/28 02:04:44 jason Exp $	*/
/*	$NetBSD: hme.c,v 1.21 2001/07/07 15:59:37 thorpej Exp $	*/

/*-
 * Copyright (c) 1999 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Paul Kranenburg.
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
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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
 * HME Ethernet module driver.
 */

#include "bpfilter.h"
#include "vlan.h"

#undef HMEDEBUG

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/mbuf.h> 
#include <sys/syslog.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/malloc.h>
#include <sys/ioctl.h>
#include <sys/errno.h>

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

#if NBPFILTER > 0
#include <net/bpf.h>
#include <net/bpfdesc.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <machine/bus.h>

#if NVLAN > 0
#include <net/if_vlan_var.h>
#endif

#include <dev/ic/hmereg.h>
#include <dev/ic/hmevar.h>

struct cfdriver hme_cd = {
	NULL, "hme", DV_IFNET
};

#define	HME_RX_OFFSET	2

void		hme_start(struct ifnet *);
void		hme_stop(struct hme_softc *);
int		hme_ioctl(struct ifnet *, u_long, caddr_t);
void		hme_tick(void *);
void		hme_watchdog(struct ifnet *);
void		hme_shutdown(void *);
void		hme_init(struct hme_softc *);
void		hme_meminit(struct hme_softc *);
void		hme_mifinit(struct hme_softc *);
void		hme_reset(struct hme_softc *);
void		hme_setladrf(struct hme_softc *);
int		hme_newbuf(struct hme_softc *, struct hme_sxd *, int);
int		hme_encap(struct hme_softc *, struct mbuf *, int *);

/* MII methods & callbacks */
static int	hme_mii_readreg(struct device *, int, int);
static void	hme_mii_writereg(struct device *, int, int, int);
static void	hme_mii_statchg(struct device *);

int		hme_mediachange(struct ifnet *);
void		hme_mediastatus(struct ifnet *, struct ifmediareq *);

int		hme_eint(struct hme_softc *, u_int);
int		hme_rint(struct hme_softc *);
int		hme_tint(struct hme_softc *);

void
hme_config(sc)
	struct hme_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mii_data *mii = &sc->sc_mii;
	struct mii_softc *child;
	bus_dma_tag_t dmatag = sc->sc_dmatag;
	bus_dma_segment_t seg;
	bus_size_t size;
	int rseg, error, i;

	/*
	 * HME common initialization.
	 *
	 * hme_softc fields that must be initialized by the front-end:
	 *
	 * the bus tag:
	 *	sc_bustag
	 *
	 * the dma bus tag:
	 *	sc_dmatag
	 *
	 * the bus handles:
	 *	sc_seb		(Shared Ethernet Block registers)
	 *	sc_erx		(Receiver Unit registers)
	 *	sc_etx		(Transmitter Unit registers)
	 *	sc_mac		(MAC registers)
	 *	sc_mif		(Managment Interface registers)
	 *
	 * the maximum bus burst size:
	 *	sc_burst
	 *
	 * the local Ethernet address:
	 *	sc_enaddr
	 *
	 */

	bcopy(sc->sc_enaddr, sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);

	/* Make sure the chip is stopped. */
	hme_stop(sc);


	for (i = 0; i < HME_TX_RING_SIZE; i++) {
		if (bus_dmamap_create(sc->sc_dmatag, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &sc->sc_txd[i].sd_map) != 0) {
			sc->sc_txd[i].sd_map = NULL;
			goto fail;
		}
	}
	for (i = 0; i < HME_RX_RING_SIZE; i++) {
		if (bus_dmamap_create(sc->sc_dmatag, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW,
		    &sc->sc_rxd[i].sd_map) != 0) {
			sc->sc_rxd[i].sd_map = NULL;
			goto fail;
		}
	}
	if (bus_dmamap_create(sc->sc_dmatag, MCLBYTES, 1, MCLBYTES, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &sc->sc_rxmap_spare) != 0) {
		sc->sc_rxmap_spare = NULL;
		goto fail;
	}

	/*
	 * Allocate DMA capable memory
	 * Buffer descriptors must be aligned on a 2048 byte boundary;
	 * take this into account when calculating the size. Note that
	 * the maximum number of descriptors (256) occupies 2048 bytes,
	 * so we allocate that much regardless of the number of descriptors.
	 */
	size = (HME_XD_SIZE * HME_RX_RING_MAX) +	/* RX descriptors */
	    (HME_XD_SIZE * HME_TX_RING_MAX);		/* TX descriptors */

	/* Allocate DMA buffer */
	if ((error = bus_dmamem_alloc(dmatag, size, 2048, 0, &seg, 1, &rseg,
	    BUS_DMA_NOWAIT)) != 0) {
		printf("%s: DMA buffer alloc error %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}

	/* Map DMA memory in CPU addressable space */
	if ((error = bus_dmamem_map(dmatag, &seg, rseg, size,
	    &sc->sc_rb.rb_membase, BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: DMA buffer map error %d\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamap_unload(dmatag, sc->sc_dmamap);
		bus_dmamem_free(dmatag, &seg, rseg);
		return;
	}

	if ((error = bus_dmamap_create(dmatag, size, 1, size, 0,
	    BUS_DMA_NOWAIT, &sc->sc_dmamap)) != 0) {
		printf("%s: DMA map create error %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}

	/* Load the buffer */
	if ((error = bus_dmamap_load(dmatag, sc->sc_dmamap,
	    sc->sc_rb.rb_membase, size, NULL,
	    BUS_DMA_NOWAIT|BUS_DMA_COHERENT)) != 0) {
		printf("%s: DMA buffer map load error %d\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamem_free(dmatag, &seg, rseg);
		return;
	}
	sc->sc_rb.rb_dmabase = sc->sc_dmamap->dm_segs[0].ds_addr;

	printf(": address %s\n", ether_sprintf(sc->sc_enaddr));

	/* Initialize ifnet structure. */
	strcpy(ifp->if_xname, sc->sc_dev.dv_xname);
	ifp->if_softc = sc;
	ifp->if_start = hme_start;
	ifp->if_ioctl = hme_ioctl;
	ifp->if_watchdog = hme_watchdog;
	ifp->if_flags =
	    IFF_BROADCAST | IFF_SIMPLEX | IFF_NOTRAILERS | IFF_MULTICAST;
	IFQ_SET_READY(&ifp->if_snd);

	/* Initialize ifmedia structures and MII info */
	mii->mii_ifp = ifp;
	mii->mii_readreg = hme_mii_readreg; 
	mii->mii_writereg = hme_mii_writereg;
	mii->mii_statchg = hme_mii_statchg;

	ifmedia_init(&mii->mii_media, 0, hme_mediachange, hme_mediastatus);

	hme_mifinit(sc);

	mii_attach(&sc->sc_dev, mii, 0xffffffff,
	    MII_PHY_ANY, MII_OFFSET_ANY, 0);

	child = LIST_FIRST(&mii->mii_phys);
	if (child == NULL) {
		/* No PHY attached */
		ifmedia_add(&sc->sc_media, IFM_ETHER|IFM_MANUAL, 0, NULL);
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_MANUAL);
	} else {
		/*
		 * Walk along the list of attached MII devices and
		 * establish an `MII instance' to `phy number'
		 * mapping. We'll use this mapping in media change
		 * requests to determine which phy to use to program
		 * the MIF configuration register.
		 */
		for (; child != NULL; child = LIST_NEXT(child, mii_list)) {
			/*
			 * Note: we support just two PHYs: the built-in
			 * internal device and an external on the MII
			 * connector.
			 */
			if (child->mii_phy > 1 || child->mii_inst > 1) {
				printf("%s: cannot accommodate MII device %s"
				    " at phy %d, instance %d\n",
				    sc->sc_dev.dv_xname,
				    child->mii_dev.dv_xname,
				    child->mii_phy, child->mii_inst);
				continue;
			}

			sc->sc_phys[child->mii_inst] = child->mii_phy;
		}

		/*
		 * XXX - we can really do the following ONLY if the
		 * phy indeed has the auto negotiation capability!!
		 */
		ifmedia_set(&sc->sc_media, IFM_ETHER|IFM_AUTO);
	}

	/* Attach the interface. */
	if_attach(ifp);
	ether_ifattach(ifp);

	sc->sc_sh = shutdownhook_establish(hme_shutdown, sc);
	if (sc->sc_sh == NULL)
		panic("hme_config: can't establish shutdownhook");

	timeout_set(&sc->sc_tick_ch, hme_tick, sc);
	return;

fail:
	if (sc->sc_rxmap_spare != NULL)
		bus_dmamap_destroy(sc->sc_dmatag, sc->sc_rxmap_spare);
	for (i = 0; i < HME_TX_RING_SIZE; i++)
		if (sc->sc_txd[i].sd_map != NULL)
			bus_dmamap_destroy(sc->sc_dmatag, sc->sc_txd[i].sd_map);
	for (i = 0; i < HME_RX_RING_SIZE; i++)
		if (sc->sc_rxd[i].sd_map != NULL)
			bus_dmamap_destroy(sc->sc_dmatag, sc->sc_rxd[i].sd_map);
}

void
hme_tick(arg)
	void *arg;
{
	struct hme_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_mac;
	int s;

	s = splnet();
	/*
	 * Unload collision counters
	 */
	ifp->if_collisions +=
	    bus_space_read_4(t, mac, HME_MACI_NCCNT) +
	    bus_space_read_4(t, mac, HME_MACI_FCCNT) +
	    bus_space_read_4(t, mac, HME_MACI_EXCNT) +
	    bus_space_read_4(t, mac, HME_MACI_LTCNT);

	/*
	 * then clear the hardware counters.
	 */
	bus_space_write_4(t, mac, HME_MACI_NCCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_FCCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_EXCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_LTCNT, 0);

	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add(&sc->sc_tick_ch, hz);
}

void
hme_reset(sc)
	struct hme_softc *sc;
{
	int s;

	s = splnet();
	hme_init(sc);
	splx(s);
}

void
hme_stop(sc)
	struct hme_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t seb = sc->sc_seb;
	int n;

	timeout_del(&sc->sc_tick_ch);
	mii_down(&sc->sc_mii);

	/* Reset transmitter and receiver */
	bus_space_write_4(t, seb, HME_SEBI_RESET,
	    (HME_SEB_RESET_ETX | HME_SEB_RESET_ERX));

	for (n = 0; n < 20; n++) {
		u_int32_t v = bus_space_read_4(t, seb, HME_SEBI_RESET);
		if ((v & (HME_SEB_RESET_ETX | HME_SEB_RESET_ERX)) == 0)
			break;
		DELAY(20);
	}
	if (n >= 20)
		printf("%s: hme_stop: reset failed\n", sc->sc_dev.dv_xname);

	for (n = 0; n < HME_TX_RING_SIZE; n++) {
		if (sc->sc_txd[n].sd_loaded) {
			bus_dmamap_sync(sc->sc_dmatag, sc->sc_txd[n].sd_map,
			    0, sc->sc_txd[n].sd_map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmatag, sc->sc_txd[n].sd_map);
			sc->sc_txd[n].sd_loaded = 0;
		}
		if (sc->sc_txd[n].sd_mbuf != NULL) {
			m_freem(sc->sc_txd[n].sd_mbuf);
			sc->sc_txd[n].sd_mbuf = NULL;
		}
	}
}

void
hme_meminit(sc)
	struct hme_softc *sc;
{
	bus_addr_t dma;
	caddr_t p;
	unsigned int i;
	struct hme_ring *hr = &sc->sc_rb;

	p = hr->rb_membase;
	dma = hr->rb_dmabase;

	/*
	 * Allocate transmit descriptors
	 */
	hr->rb_txd = p;
	hr->rb_txddma = dma;
	p += HME_TX_RING_SIZE * HME_XD_SIZE;
	dma += HME_TX_RING_SIZE * HME_XD_SIZE;
	/* We have reserved descriptor space until the next 2048 byte boundary.*/
	dma = (bus_addr_t)roundup((u_long)dma, 2048);
	p = (caddr_t)roundup((u_long)p, 2048);

	/*
	 * Allocate receive descriptors
	 */
	hr->rb_rxd = p;
	hr->rb_rxddma = dma;
	p += HME_RX_RING_SIZE * HME_XD_SIZE;
	dma += HME_RX_RING_SIZE * HME_XD_SIZE;
	/* Again move forward to the next 2048 byte boundary.*/
	dma = (bus_addr_t)roundup((u_long)dma, 2048);
	p = (caddr_t)roundup((u_long)p, 2048);

	/*
	 * Initialize transmit descriptors
	 */
	for (i = 0; i < HME_TX_RING_SIZE; i++) {
		HME_XD_SETADDR(sc->sc_pci, hr->rb_txd, i, 0);
		HME_XD_SETFLAGS(sc->sc_pci, hr->rb_txd, i, 0);
		sc->sc_txd[i].sd_mbuf = NULL;
	}

	/*
	 * Initialize receive descriptors
	 */
	for (i = 0; i < HME_RX_RING_SIZE; i++) {
		if (hme_newbuf(sc, &sc->sc_rxd[i], 1)) {
			printf("%s: rx allocation failed\n",
			    sc->sc_dev.dv_xname);
			break;
		}
		HME_XD_SETADDR(sc->sc_pci, hr->rb_rxd, i,
		    sc->sc_rxd[i].sd_map->dm_segs[0].ds_addr);
		HME_XD_SETFLAGS(sc->sc_pci, hr->rb_rxd, i,
		    HME_XD_OWN | HME_XD_ENCODE_RSIZE(HME_RX_PKTSIZE));
	}

	sc->sc_tx_prod = sc->sc_tx_cons = sc->sc_tx_cnt = 0;
	sc->sc_last_rd = 0;
}

/*
 * Initialization of interface; set up initialization block
 * and transmit/receive descriptor rings.
 */
void
hme_init(sc)
	struct hme_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t seb = sc->sc_seb;
	bus_space_handle_t etx = sc->sc_etx;
	bus_space_handle_t erx = sc->sc_erx;
	bus_space_handle_t mac = sc->sc_mac;
	bus_space_handle_t mif = sc->sc_mif;
	u_int8_t *ea;
	u_int32_t v;

	/*
	 * Initialization sequence. The numbered steps below correspond
	 * to the sequence outlined in section 6.3.5.1 in the Ethernet
	 * Channel Engine manual (part of the PCIO manual).
	 * See also the STP2002-STQ document from Sun Microsystems.
	 */

	/* step 1 & 2. Reset the Ethernet Channel */
	hme_stop(sc);

	/* Re-initialize the MIF */
	hme_mifinit(sc);

	/* Call MI reset function if any */
	if (sc->sc_hwreset)
		(*sc->sc_hwreset)(sc);

#if 0
	/* Mask all MIF interrupts, just in case */
	bus_space_write_4(t, mif, HME_MIFI_IMASK, 0xffff);
#endif

	/* step 3. Setup data structures in host memory */
	hme_meminit(sc);

	/* step 4. TX MAC registers & counters */
	bus_space_write_4(t, mac, HME_MACI_NCCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_FCCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_EXCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_LTCNT, 0);
	bus_space_write_4(t, mac, HME_MACI_TXSIZE, HME_MTU);

	/* Load station MAC address */
	ea = sc->sc_enaddr;
	bus_space_write_4(t, mac, HME_MACI_MACADDR0, (ea[0] << 8) | ea[1]);
	bus_space_write_4(t, mac, HME_MACI_MACADDR1, (ea[2] << 8) | ea[3]);
	bus_space_write_4(t, mac, HME_MACI_MACADDR2, (ea[4] << 8) | ea[5]);

	/*
	 * Init seed for backoff
	 * (source suggested by manual: low 10 bits of MAC address)
	 */ 
	v = ((ea[4] << 8) | ea[5]) & 0x3fff;
	bus_space_write_4(t, mac, HME_MACI_RANDSEED, v);


	/* Note: Accepting power-on default for other MAC registers here.. */


	/* step 5. RX MAC registers & counters */
	hme_setladrf(sc);

	/* step 6 & 7. Program Descriptor Ring Base Addresses */
	bus_space_write_4(t, etx, HME_ETXI_RING, sc->sc_rb.rb_txddma);
	bus_space_write_4(t, etx, HME_ETXI_RSIZE, HME_TX_RING_SIZE);

	bus_space_write_4(t, erx, HME_ERXI_RING, sc->sc_rb.rb_rxddma);
	bus_space_write_4(t, mac, HME_MACI_RXSIZE, HME_MTU);

	/* step 8. Global Configuration & Interrupt Mask */
	bus_space_write_4(t, seb, HME_SEBI_IMASK,
	    ~(HME_SEB_STAT_HOSTTOTX | HME_SEB_STAT_RXTOHOST |
	      HME_SEB_STAT_TXALL | HME_SEB_STAT_TXPERR |
	      HME_SEB_STAT_RCNTEXP | HME_SEB_STAT_ALL_ERRORS));

	switch (sc->sc_burst) {
	default:
		v = 0;
		break;
	case 16:
		v = HME_SEB_CFG_BURST16;
		break;
	case 32:
		v = HME_SEB_CFG_BURST32;
		break;
	case 64:
		v = HME_SEB_CFG_BURST64;
		break;
	}
	bus_space_write_4(t, seb, HME_SEBI_CFG, v);

	/* step 9. ETX Configuration: use mostly default values */

	/* Enable DMA */
	v = bus_space_read_4(t, etx, HME_ETXI_CFG);
	v |= HME_ETX_CFG_DMAENABLE;
	bus_space_write_4(t, etx, HME_ETXI_CFG, v);

	/* Transmit Descriptor ring size: in increments of 16 */
	bus_space_write_4(t, etx, HME_ETXI_RSIZE, HME_TX_RING_SIZE / 16 - 1);

	/* step 10. ERX Configuration */
	v = bus_space_read_4(t, erx, HME_ERXI_CFG);
	v &= ~HME_ERX_CFG_RINGSIZE256;
#if HME_RX_RING_SIZE == 32
	v |= HME_ERX_CFG_RINGSIZE32;
#elif HME_RX_RING_SIZE == 64
	v |= HME_ERX_CFG_RINGSIZE64;
#elif HME_RX_RING_SIZE == 128
	v |= HME_ERX_CFG_RINGSIZE128;
#elif HME_RX_RING_SIZE == 256
	v |= HME_ERX_CFG_RINGSIZE256;
#else
# error	"RX ring size must be 32, 64, 128, or 256"
#endif
	/* Enable DMA */
	v |= HME_ERX_CFG_DMAENABLE | (HME_RX_OFFSET << 3);
	bus_space_write_4(t, erx, HME_ERXI_CFG, v);

	/* step 11. XIF Configuration */
	v = bus_space_read_4(t, mac, HME_MACI_XIF);
	v |= HME_MAC_XIF_OE;
	/* If an external transceiver is connected, enable its MII drivers */
	if ((bus_space_read_4(t, mif, HME_MIFI_CFG) & HME_MIF_CFG_MDI1) != 0)
		v |= HME_MAC_XIF_MIIENABLE;
	bus_space_write_4(t, mac, HME_MACI_XIF, v);


	/* step 12. RX_MAC Configuration Register */
	v = bus_space_read_4(t, mac, HME_MACI_RXCFG);
	v |= HME_MAC_RXCFG_ENABLE;
	bus_space_write_4(t, mac, HME_MACI_RXCFG, v);

	/* step 13. TX_MAC Configuration Register */
	v = bus_space_read_4(t, mac, HME_MACI_TXCFG);
	v |= (HME_MAC_TXCFG_ENABLE | HME_MAC_TXCFG_DGIVEUP);
	bus_space_write_4(t, mac, HME_MACI_TXCFG, v);

	/* step 14. Issue Transmit Pending command */

	/* Call MI initialization function if any */
	if (sc->sc_hwinit)
		(*sc->sc_hwinit)(sc);

	/* Start the one second timer. */
	timeout_add(&sc->sc_tick_ch, hz);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_timer = 0;
	hme_start(ifp);
}

void
hme_start(ifp)
	struct ifnet *ifp;
{
	struct hme_softc *sc = (struct hme_softc *)ifp->if_softc;
	struct mbuf *m;
	int bix, cnt = 0;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	bix = sc->sc_tx_prod;
	while (sc->sc_txd[bix].sd_mbuf == NULL) {
		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

#if NBPFILTER > 0
		/*
		 * If BPF is listening on this interface, let it see the
		 * packet before we commit it to the wire.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		if (hme_encap(sc, m, &bix)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);

		bus_space_write_4(sc->sc_bustag, sc->sc_etx, HME_ETXI_PENDING,
		    HME_ETX_TP_DMAWAKEUP);
		cnt++;
	}

	if (cnt != 0) {
		sc->sc_tx_prod = bix;
		ifp->if_timer = 0;
	}
}

/*
 * Transmit interrupt.
 */
int
hme_tint(sc)
	struct hme_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	unsigned int ri, txflags;
	struct hme_sxd *sd;
	int cnt = sc->sc_tx_cnt;

	/* Fetch current position in the transmit ring */
	ri = sc->sc_tx_cons;
	sd = &sc->sc_txd[ri];

	for (;;) {
		if (cnt <= 0)
			break;

		txflags = HME_XD_GETFLAGS(sc->sc_pci, sc->sc_rb.rb_txd, ri);

		if (txflags & HME_XD_OWN)
			break;

		ifp->if_flags &= ~IFF_OACTIVE;
		if (txflags & HME_XD_EOP)
			ifp->if_opackets++;

		bus_dmamap_sync(sc->sc_dmatag, sd->sd_map,
		    0, sd->sd_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmatag, sd->sd_map);
		sd->sd_loaded = 0;

		if (sd->sd_mbuf != NULL) {
			m_freem(sd->sd_mbuf);
			sd->sd_mbuf = NULL;
		}

		if (++ri == HME_TX_RING_SIZE) {
			ri = 0;
			sd = sc->sc_txd;
		} else
			sd++;

		--cnt;
	}

	sc->sc_tx_cnt = cnt;
	if (cnt == 0)
		ifp->if_timer = 0;

	/* Update ring */
	sc->sc_tx_cons = ri;

	hme_start(ifp);

	return (1);
}

/*
 * Receive interrupt.
 */
int
hme_rint(sc)
	struct hme_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mbuf *m;
	struct hme_sxd *sd;
	unsigned int ri, len;
	u_int32_t flags;

	ri = sc->sc_last_rd;
	sd = &sc->sc_rxd[ri];

	/*
	 * Process all buffers with valid data.
	 */
	for (;;) {
		flags = HME_XD_GETFLAGS(sc->sc_pci, sc->sc_rb.rb_rxd, ri);
		if (flags & HME_XD_OWN)
			break;

		if (flags & HME_XD_OFL) {
			printf("%s: buffer overflow, ri=%d; flags=0x%x\n",
			    sc->sc_dev.dv_xname, ri, flags);
			goto again;
		}

		m = sd->sd_mbuf;
		len = HME_XD_DECODE_RSIZE(flags);
		m->m_pkthdr.len = m->m_len = len;

		if (hme_newbuf(sc, sd, 0)) {
			/*
			 * Allocation of new mbuf cluster failed, leave the
			 * old one in place and keep going.
			 */
			ifp->if_ierrors++;
			goto again;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf) {
			m->m_pkthdr.len = m->m_len = len;
			bpf_mtap(ifp->if_bpf, m);
		}
#endif

		ifp->if_ipackets++;
		ether_input_mbuf(ifp, m);

again:
		HME_XD_SETADDR(sc->sc_pci, sc->sc_rb.rb_rxd, ri,
		    sd->sd_map->dm_segs[0].ds_addr);
		HME_XD_SETFLAGS(sc->sc_pci, sc->sc_rb.rb_rxd, ri,
		    HME_XD_OWN | HME_XD_ENCODE_RSIZE(HME_RX_PKTSIZE));

		if (++ri == HME_RX_RING_SIZE) {
			ri = 0;
			sd = sc->sc_rxd;
		} else
			sd++;
	}

	sc->sc_last_rd = ri;
	return (1);
}

int
hme_eint(sc, status)
	struct hme_softc *sc;
	u_int status;
{
	if ((status & HME_SEB_STAT_MIFIRQ) != 0) {
		printf("%s: XXXlink status changed\n", sc->sc_dev.dv_xname);
		return (1);
	}

	printf("%s: status=%b\n", sc->sc_dev.dv_xname, status, HME_SEB_STAT_BITS);
	return (1);
}

int
hme_intr(v)
	void *v;
{
	struct hme_softc *sc = (struct hme_softc *)v;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t seb = sc->sc_seb;
	u_int32_t status;
	int r = 0;

	status = bus_space_read_4(t, seb, HME_SEBI_STAT);

	if ((status & HME_SEB_STAT_ALL_ERRORS) != 0)
		r |= hme_eint(sc, status);

	if ((status & (HME_SEB_STAT_TXALL | HME_SEB_STAT_HOSTTOTX)) != 0)
		r |= hme_tint(sc);

	if ((status & HME_SEB_STAT_RXTOHOST) != 0)
		r |= hme_rint(sc);

	return (r);
}


void
hme_watchdog(ifp)
	struct ifnet *ifp;
{
	struct hme_softc *sc = ifp->if_softc;

	log(LOG_ERR, "%s: device timeout\n", sc->sc_dev.dv_xname);
	++ifp->if_oerrors;

	hme_reset(sc);
}

/*
 * Initialize the MII Management Interface
 */
void
hme_mifinit(sc)
	struct hme_softc *sc;
{
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_mif;
	u_int32_t v;

	/* Configure the MIF in frame mode */
	v = bus_space_read_4(t, mif, HME_MIFI_CFG);
	v &= ~HME_MIF_CFG_BBMODE;
	bus_space_write_4(t, mif, HME_MIFI_CFG, v);
}

/*
 * MII interface
 */
static int
hme_mii_readreg(self, phy, reg)
	struct device *self;
	int phy, reg;
{
	struct hme_softc *sc = (void *)self;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_mif;
	int n;
	u_int32_t v;

	/* Select the desired PHY in the MIF configuration register */
	v = bus_space_read_4(t, mif, HME_MIFI_CFG);
	/* Clear PHY select bit */
	v &= ~HME_MIF_CFG_PHY;
	if (phy == HME_PHYAD_EXTERNAL)
		/* Set PHY select bit to get at external device */
		v |= HME_MIF_CFG_PHY;
	bus_space_write_4(t, mif, HME_MIFI_CFG, v);

	/* Construct the frame command */
	v = (MII_COMMAND_START << HME_MIF_FO_ST_SHIFT) |
	    HME_MIF_FO_TAMSB |
	    (MII_COMMAND_READ << HME_MIF_FO_OPC_SHIFT) |
	    (phy << HME_MIF_FO_PHYAD_SHIFT) |
	    (reg << HME_MIF_FO_REGAD_SHIFT);

	bus_space_write_4(t, mif, HME_MIFI_FO, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, HME_MIFI_FO);
		if (v & HME_MIF_FO_TALSB)
			return (v & HME_MIF_FO_DATA);
	}

	printf("%s: mii_read timeout\n", sc->sc_dev.dv_xname);
	return (0);
}

static void
hme_mii_writereg(self, phy, reg, val)
	struct device *self;
	int phy, reg, val;
{
	struct hme_softc *sc = (void *)self;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_mif;
	int n;
	u_int32_t v;

	/* Select the desired PHY in the MIF configuration register */
	v = bus_space_read_4(t, mif, HME_MIFI_CFG);
	/* Clear PHY select bit */
	v &= ~HME_MIF_CFG_PHY;
	if (phy == HME_PHYAD_EXTERNAL)
		/* Set PHY select bit to get at external device */
		v |= HME_MIF_CFG_PHY;
	bus_space_write_4(t, mif, HME_MIFI_CFG, v);

	/* Construct the frame command */
	v = (MII_COMMAND_START << HME_MIF_FO_ST_SHIFT)	|
	    HME_MIF_FO_TAMSB				|
	    (MII_COMMAND_WRITE << HME_MIF_FO_OPC_SHIFT)	|
	    (phy << HME_MIF_FO_PHYAD_SHIFT)		|
	    (reg << HME_MIF_FO_REGAD_SHIFT)		|
	    (val & HME_MIF_FO_DATA);

	bus_space_write_4(t, mif, HME_MIFI_FO, v);
	for (n = 0; n < 100; n++) {
		DELAY(1);
		v = bus_space_read_4(t, mif, HME_MIFI_FO);
		if (v & HME_MIF_FO_TALSB)
			return;
	}

	printf("%s: mii_write timeout\n", sc->sc_dev.dv_xname);
}

static void
hme_mii_statchg(dev)
	struct device *dev;
{
	struct hme_softc *sc = (void *)dev;
	int instance = IFM_INST(sc->sc_mii.mii_media.ifm_cur->ifm_media);
	int phy = sc->sc_phys[instance];
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mif = sc->sc_mif;
	bus_space_handle_t mac = sc->sc_mac;
	u_int32_t v;

#ifdef HMEDEBUG
	if (sc->sc_debug)
		printf("hme_mii_statchg: status change: phy = %d\n", phy);
#endif

	/* Select the current PHY in the MIF configuration register */
	v = bus_space_read_4(t, mif, HME_MIFI_CFG);
	v &= ~HME_MIF_CFG_PHY;
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MIF_CFG_PHY;
	bus_space_write_4(t, mif, HME_MIFI_CFG, v);

	/* Set the MAC Full Duplex bit appropriately */
	v = bus_space_read_4(t, mac, HME_MACI_TXCFG);
	if ((IFM_OPTIONS(sc->sc_mii.mii_media_active) & IFM_FDX) != 0) {
		v |= HME_MAC_TXCFG_FULLDPLX;
		sc->sc_arpcom.ac_if.if_flags |= IFF_SIMPLEX;
	} else {
		v &= ~HME_MAC_TXCFG_FULLDPLX;
		sc->sc_arpcom.ac_if.if_flags &= ~IFF_SIMPLEX;
	}
	bus_space_write_4(t, mac, HME_MACI_TXCFG, v);

	/* If an external transceiver is selected, enable its MII drivers */
	v = bus_space_read_4(t, mac, HME_MACI_XIF);
	v &= ~HME_MAC_XIF_MIIENABLE;
	if (phy == HME_PHYAD_EXTERNAL)
		v |= HME_MAC_XIF_MIIENABLE;
	bus_space_write_4(t, mac, HME_MACI_XIF, v);
}

int
hme_mediachange(ifp)
	struct ifnet *ifp;
{
	struct hme_softc *sc = ifp->if_softc;

	if (IFM_TYPE(sc->sc_media.ifm_media) != IFM_ETHER)
		return (EINVAL);

	return (mii_mediachg(&sc->sc_mii));
}

void
hme_mediastatus(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct hme_softc *sc = ifp->if_softc;

	if ((ifp->if_flags & IFF_UP) == 0)
		return;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

/*
 * Process an ioctl request.
 */
int
hme_ioctl(ifp, cmd, data)
	struct ifnet *ifp;
	u_long cmd;
	caddr_t data;
{
	struct hme_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, cmd, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (cmd) {

	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			hme_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif
		default:
			hme_init(sc);
			break;
		}
		break;

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) == 0 &&
		    (ifp->if_flags & IFF_RUNNING) != 0) {
			/*
			 * If interface is marked down and it is running, then
			 * stop it.
			 */
			hme_stop(sc);
			ifp->if_flags &= ~IFF_RUNNING;
		} else if ((ifp->if_flags & IFF_UP) != 0 &&
		    	   (ifp->if_flags & IFF_RUNNING) == 0) {
			/*
			 * If interface is marked up and it is stopped, then
			 * start it.
			 */
			hme_init(sc);
		} else if ((ifp->if_flags & IFF_UP) != 0) {
			/*
			 * Reset the interface to pick up changes in any other
			 * flags that affect hardware registers.
			 */
			hme_init(sc);
		}
#ifdef HMEDEBUG
		sc->sc_debug = (ifp->if_flags & IFF_DEBUG) != 0 ? 1 : 0;
#endif
		break;

	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware filter
			 * accordingly.
			 */
			hme_setladrf(sc);
			error = 0;
		}
		break;

	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_media, cmd);
		break;

	default:
		error = ENOTTY;
		break;
	}

	splx(s);
	return (error);
}

void
hme_shutdown(arg)
	void *arg;
{
	hme_stop((struct hme_softc *)arg);
}

/*
 * Set up the logical address filter.
 */
void
hme_setladrf(sc)
	struct hme_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct ether_multi *enm;
	struct ether_multistep step;
	struct arpcom *ac = &sc->sc_arpcom;
	bus_space_tag_t t = sc->sc_bustag;
	bus_space_handle_t mac = sc->sc_mac;
	u_char *cp;
	u_int32_t crc;
	u_int32_t hash[4];
	u_int32_t v;
	int len;

	/* Clear hash table */
	hash[3] = hash[2] = hash[1] = hash[0] = 0;

	/* Get current RX configuration */
	v = bus_space_read_4(t, mac, HME_MACI_RXCFG);

	if ((ifp->if_flags & IFF_PROMISC) != 0) {
		/* Turn on promiscuous mode; turn off the hash filter */
		v |= HME_MAC_RXCFG_PMISC;
		v &= ~HME_MAC_RXCFG_HENABLE;
		ifp->if_flags |= IFF_ALLMULTI;
		goto chipit;
	}

	/* Turn off promiscuous mode; turn on the hash filter */
	v &= ~HME_MAC_RXCFG_PMISC;
	v |= HME_MAC_RXCFG_HENABLE;

	/*
	 * Set up multicast address filter by passing all multicast addresses
	 * through a crc generator, and then using the high order 6 bits as an
	 * index into the 64 bit logical address filter.  The high order bit
	 * selects the word, while the rest of the bits select the bit within
	 * the word.
	 */

	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			/*
			 * We must listen to a range of multicast addresses.
			 * For now, just accept all multicasts, rather than
			 * trying to set only those filter bits needed to match
			 * the range.  (At this time, the only use of address
			 * ranges is for IP multicast routing, for which the
			 * range is big enough to require all bits set.)
			 */
			hash[3] = hash[2] = hash[1] = hash[0] = 0xffff;
			ifp->if_flags |= IFF_ALLMULTI;
			goto chipit;
		}

		cp = enm->enm_addrlo;
		crc = 0xffffffff;
		for (len = sizeof(enm->enm_addrlo); --len >= 0;) {
			int octet = *cp++;
			int i;

#define MC_POLY_LE	0xedb88320UL	/* mcast crc, little endian */
			for (i = 0; i < 8; i++) {
				if ((crc & 1) ^ (octet & 1)) {
					crc >>= 1;
					crc ^= MC_POLY_LE;
				} else {
					crc >>= 1;
				}
				octet >>= 1;
			}
		}
		/* Just want the 6 most significant bits. */
		crc >>= 26;

		/* Set the corresponding bit in the filter. */
		hash[crc >> 4] |= 1 << (crc & 0xf);

		ETHER_NEXT_MULTI(step, enm);
	}

	ifp->if_flags &= ~IFF_ALLMULTI;

chipit:
	/* Now load the hash table into the chip */
	bus_space_write_4(t, mac, HME_MACI_HASHTAB0, hash[0]);
	bus_space_write_4(t, mac, HME_MACI_HASHTAB1, hash[1]);
	bus_space_write_4(t, mac, HME_MACI_HASHTAB2, hash[2]);
	bus_space_write_4(t, mac, HME_MACI_HASHTAB3, hash[3]);
	bus_space_write_4(t, mac, HME_MACI_RXCFG, v);
}

int
hme_encap(sc, mhead, bixp)
	struct hme_softc *sc;
	struct mbuf *mhead;
	int *bixp;
{
	struct hme_sxd *sd;
	struct mbuf *m;
	int frag, cur, cnt = 0;
	u_int32_t flags;
	struct hme_ring *hr = &sc->sc_rb;

	cur = frag = *bixp;
	sd = &sc->sc_txd[frag];

	for (m = mhead; m != NULL; m = m->m_next) {
		if (m->m_len == 0)
			continue;

		if ((HME_TX_RING_SIZE - (sc->sc_tx_cnt + cnt)) < 5)
			goto err;

		if (bus_dmamap_load(sc->sc_dmatag, sd->sd_map,
		    mtod(m, caddr_t), m->m_len, NULL, BUS_DMA_NOWAIT) != 0)
			goto err;

		sd->sd_loaded = 1;
		bus_dmamap_sync(sc->sc_dmatag, sd->sd_map, 0,
		    sd->sd_map->dm_mapsize, BUS_DMASYNC_PREWRITE);

		sd->sd_mbuf = NULL;

		flags = HME_XD_ENCODE_TSIZE(m->m_len);
		if (cnt == 0)
			flags |= HME_XD_SOP;
		else
			flags |= HME_XD_OWN;

		HME_XD_SETADDR(sc->sc_pci, hr->rb_txd, frag,
		    sd->sd_map->dm_segs[0].ds_addr);
		HME_XD_SETFLAGS(sc->sc_pci, hr->rb_txd, frag, flags);

		cur = frag;
		cnt++;
		if (++frag == HME_TX_RING_SIZE) {
			frag = 0;
			sd = sc->sc_txd;
		} else
			sd++;
	}

	/* Set end of packet on last descriptor. */
	flags = HME_XD_GETFLAGS(sc->sc_pci, hr->rb_txd, cur);
	flags |= HME_XD_EOP;
	HME_XD_SETFLAGS(sc->sc_pci, hr->rb_txd, cur, flags);
	sc->sc_txd[cur].sd_mbuf = mhead;

	/* Give first frame over to the hardware. */
	flags = HME_XD_GETFLAGS(sc->sc_pci, hr->rb_txd, (*bixp));
	flags |= HME_XD_OWN;
	HME_XD_SETFLAGS(sc->sc_pci, hr->rb_txd, (*bixp), flags);

	sc->sc_tx_cnt += cnt;
	*bixp = frag;

	/* sync descriptors */

	return (0);

err:
	/*
	 * Invalidate the stuff we may have already put into place. We
	 * will be called again to queue it later.
	 */
	for (; cnt > 0; cnt--) {
		if (--frag == -1)
			frag = HME_TX_RING_SIZE - 1;
		sd = &sc->sc_txd[frag];
		bus_dmamap_sync(sc->sc_dmatag, sd->sd_map, 0,
		    sd->sd_map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmatag, sd->sd_map);
		sd->sd_loaded = 0;
		sd->sd_mbuf = NULL;
	}
	return (ENOBUFS);
}

int
hme_newbuf(sc, d, freeit)
	struct hme_softc *sc;
	struct hme_sxd *d;
	int freeit;
{
	struct mbuf *m;
	bus_dmamap_t map;

	/*
	 * All operations should be on local variables and/or rx spare map
	 * until we're sure everything is a success.
	 */

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);
	m->m_pkthdr.rcvif = &sc->sc_arpcom.ac_if;

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	if (bus_dmamap_load(sc->sc_dmatag, sc->sc_rxmap_spare,
	    mtod(m, caddr_t), MCLBYTES - HME_RX_OFFSET, NULL,
	    BUS_DMA_NOWAIT) != 0) {
		m_freem(m);
		return (ENOBUFS);
	}

	/*
	 * At this point we have a new buffer loaded into the spare map.
	 * Just need to clear out the old mbuf/map and put the new one
	 * in place.
	 */

	if (d->sd_loaded) {
		bus_dmamap_sync(sc->sc_dmatag, d->sd_map,
		    0, d->sd_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmatag, d->sd_map);
		d->sd_loaded = 0;
	}

	if ((d->sd_mbuf != NULL) && freeit) {
		m_freem(d->sd_mbuf);
		d->sd_mbuf = NULL;
	}

	map = d->sd_map;
	d->sd_map = sc->sc_rxmap_spare;
	sc->sc_rxmap_spare = map;

	d->sd_loaded = 1;

	bus_dmamap_sync(sc->sc_dmatag, d->sd_map, 0, d->sd_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	m->m_data += HME_RX_OFFSET;
	d->sd_mbuf = m;
	return (0);
}
