/* $OpenBSD: if_gx.c,v 1.7 2002/09/24 03:51:22 nate Exp $ */
/*-
 * Copyright (c) 1999,2000,2001 Jonathan Lemon
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	$FreeBSD$
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/device.h>
#include <sys/queue.h>

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

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/if_gxreg.h>
#include <dev/pci/if_gxvar.h>

#define TUNABLE_TX_INTR_DELAY	100
#define TUNABLE_RX_INTR_DELAY	100

#define GX_CSUM_FEATURES	(CSUM_IP | CSUM_TCP | CSUM_UDP | CSUM_IP_FRAGS)

/*
 * Various supported device vendors/types and their names.
 */
struct gx_device {
	u_int16_t	vendor;
	u_int16_t	device;
	int		version_flags;
	u_int32_t	version_ipg;
};

struct gx_device gx_devs[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82542,
	    GXF_WISEMAN | GXF_FORCE_TBI | GXF_OLD_REGS,
	    10 | 2 << 10 | 10 << 20 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC_SC,
	    GXF_LIVENGOOD | GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    6 | 8 << 10 | 6 << 20 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC,
	    GXF_LIVENGOOD | GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    8 | 8 << 10 | 6 << 20 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544EI,
	    GXF_CORDOVA | GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    8 | 8 << 10 | 6 << 20 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544EI_SC,
	    GXF_CORDOVA | GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    6 | 8 << 10 | 6 << 20 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC,
	    GXF_CORDOVA | GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    8 | 8 << 10 | 6 << 20 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC_LX,
	    GXF_CORDOVA | GXF_DMA | GXF_ENABLE_MWI | GXF_CSUM,
	    8 | 8 << 10 | 6 << 20 },
	{ 0, 0, 0, NULL }
};

struct gx_regs new_regs = {
	GX_RX_RING_BASE, GX_RX_RING_LEN,
	GX_RX_RING_HEAD, GX_RX_RING_TAIL,
	GX_RX_INTR_DELAY, GX_RX_DMA_CTRL,

	GX_TX_RING_BASE, GX_TX_RING_LEN,
	GX_TX_RING_HEAD, GX_TX_RING_TAIL,
	GX_TX_INTR_DELAY, GX_TX_DMA_CTRL,
};
struct gx_regs old_regs = {
	GX_RX_OLD_RING_BASE, GX_RX_OLD_RING_LEN,
	GX_RX_OLD_RING_HEAD, GX_RX_OLD_RING_TAIL,
	GX_RX_OLD_INTR_DELAY, GX_RX_OLD_DMA_CTRL,

	GX_TX_OLD_RING_BASE, GX_TX_OLD_RING_LEN,
	GX_TX_OLD_RING_HEAD, GX_TX_OLD_RING_TAIL,
	GX_TX_OLD_INTR_DELAY, GX_TX_OLD_DMA_CTRL,
};

int 	gx_probe(struct device *, void *, void *);
void 	gx_attach(struct device *, struct device *, void *);
int 	gx_detach(void *xsc);
void 	gx_shutdown(void *xsc);

void	gx_rxeof(struct gx_softc *gx);
void	gx_txeof(struct gx_softc *gx);
int	gx_encap(struct gx_softc *gx, struct mbuf *m_head);
int 	gx_intr(void *xsc);
void	gx_init(void *xsc);

struct 	gx_device *gx_match(void *aux);
void	gx_eeprom_getword(struct gx_softc *gx, int addr,
		    u_int16_t *dest);
int	gx_read_eeprom(struct gx_softc *gx, caddr_t dest, int off,
		    int cnt);
int	gx_ifmedia_upd(struct ifnet *ifp);
void	gx_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr);
int 	gx_miibus_livengood_readreg(struct device *dev, int phy, int reg);
void 	gx_miibus_livengood_writereg(struct device *dev, int phy, int reg, int value);
int 	gx_miibus_cordova_readreg(struct device *dev, int phy, int reg);
void 	gx_miibus_cordova_writereg(struct device *dev, int phy, int reg, int value);
void 	gx_miibus_statchg(struct device *dev);
void	gx_mii_shiftin(struct gx_softc *gx, int data, int length);
u_int16_t gx_mii_shiftout(struct gx_softc *gx);
int	gx_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
void	gx_setmulti(struct gx_softc *gx);
void	gx_reset(struct gx_softc *gx);
void 	gx_phy_reset(struct gx_softc *gx);
void 	gx_release(struct gx_softc *gx);
void	gx_stop(struct gx_softc *gx);
void	gx_watchdog(struct ifnet *ifp);
void	gx_start(struct ifnet *ifp);

int	gx_newbuf(struct gx_softc *gx, int idx, struct mbuf *m);
int	gx_init_rx_ring(struct gx_softc *gx);
void	gx_free_rx_ring(struct gx_softc *gx);
int	gx_init_tx_ring(struct gx_softc *gx);
void	gx_free_tx_ring(struct gx_softc *gx);

#ifdef GX_DEBUG
#define DPRINTF(x)	if (gxdebug) printf x
#define DPRINTFN(n,x)	if (gxdebug >= (n)) printf x
int	gxdebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

struct gx_device *
gx_match(void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	int i;

	for (i = 0; i < sizeof(gx_devs) / sizeof(gx_devs[0]); i++) {
		if ((PCI_VENDOR(pa->pa_id) == gx_devs[i].vendor) &&
		    (PCI_PRODUCT(pa->pa_id) == gx_devs[i].device))
			return (&gx_devs[i]);
	}
	return (NULL);
}

int
gx_probe(struct device *parent, void *match, void *aux)
{
	if (gx_match(aux) != NULL)
		return (1);

	return (0);
}

void
gx_attach(struct device *parent, struct device *self, void *aux)
{
	struct gx_softc		*gx = (struct gx_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;
	const char		*intrstr = NULL;
	bus_addr_t		iobase;
	bus_size_t		iosize;
	bus_dma_segment_t	seg;
	int			i, rseg;
	u_int32_t		command;
	struct gx_device	*gx_dev;
	struct ifnet		*ifp;
	int			s, error = 0;
	caddr_t			kva;

	s = splimp();

	gx_dev = gx_match(aux);
	gx->gx_vflags = gx_dev->version_flags;
	gx->gx_ipg = gx_dev->version_ipg;

	mtx_init(&gx->gx_mtx, device_get_nameunit(dev), MTX_DEF | MTX_RECURSE);

	GX_LOCK(gx);

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_MEM_ENABLE | PCI_COMMAND_MASTER_ENABLE;
	if (gx->gx_vflags & GXF_ENABLE_MWI)
		command |= PCIM_CMD_MWIEN;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

/* XXX check cache line size? */

	if ((command & PCI_COMMAND_MEM_ENABLE) == 0) {
		printf(": failed to enable memory mapping!\n");
		error = ENXIO;
		goto fail;
	}

	if (pci_mem_find(pc, pa->pa_tag, GX_PCI_LOMEM, &iobase, &iosize,
			 NULL)) {
		printf(": can't find mem space\n");
 		goto fail;
 	}

	DPRINTFN(5, ("%s: bus_space_map\n", gx->gx_dev.dv_xname));
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &gx->gx_bhandle)) {
		printf(": can't map mem space\n");
		goto fail;
	}

	gx->gx_btag = pa->pa_memt;

	/* Allocate interrupt */
	DPRINTFN(5, ("%s: pci_intr_map\n", gx->gx_dev.dv_xname));
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
 		goto fail;
 	}

	DPRINTFN(5, ("%s: pci_intr_string\n", gx->gx_dev.dv_xname));
	intrstr = pci_intr_string(pc, ih);
	
	DPRINTFN(5, ("%s: pci_intr_establish\n", gx->gx_dev.dv_xname));
	gx->gx_intrhand = pci_intr_establish(pc, ih, IPL_NET, gx_intr, gx,
					     gx->gx_dev.dv_xname);
	
	if (gx->gx_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
 	}
	printf(": %s", intrstr);

	/* compensate for different register mappings */
	if (gx->gx_vflags & GXF_OLD_REGS)
		gx->gx_reg = old_regs;
	else
		gx->gx_reg = new_regs;

	if (gx_read_eeprom(gx, (caddr_t)&gx->arpcom.ac_enaddr,
	    GX_EEMAP_MAC, 3)) {
		printf("failed to read station address\n");
		error = ENXIO;
		goto fail;
	}

	printf(": address: %s\n", ether_sprintf(gx->arpcom.ac_enaddr));

	/* Allocate the ring buffers. */
	gx->gx_dmatag = pa->pa_dmat;
	DPRINTFN(5, ("%s: bus_dmamem_alloc\n", gx->gx_dev.dv_xname));
	if (bus_dmamem_alloc(gx->gx_dmatag, sizeof(struct gx_ring_data),
			     PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: can't alloc rx buffers\n", gx->gx_dev.dv_xname);
		goto fail;
	}
	DPRINTFN(5, ("%s: bus_dmamem_map\n", gx->gx_dev.dv_xname));
	if (bus_dmamem_map(gx->gx_dmatag, &seg, rseg,
			   sizeof(struct gx_ring_data), &kva,
			   BUS_DMA_NOWAIT)) {
		printf("%s: can't map dma buffers (%d bytes)\n",
		       gx->gx_dev.dv_xname, sizeof(struct gx_ring_data));
		bus_dmamem_free(gx->gx_dmatag, &seg, rseg);
 		goto fail;
 	}
	DPRINTFN(5, ("%s: bus_dmamem_create\n", gx->gx_dev.dv_xname));
	if (bus_dmamap_create(gx->gx_dmatag, sizeof(struct gx_ring_data), 1,
			      sizeof(struct gx_ring_data), 0,
			      BUS_DMA_NOWAIT, &gx->gx_ring_map)) {
		printf("%s: can't create dma map\n", gx->gx_dev.dv_xname);
		bus_dmamem_unmap(gx->gx_dmatag, kva,
				 sizeof(struct gx_ring_data));
		bus_dmamem_free(gx->gx_dmatag, &seg, rseg);
		goto fail;
	}
	DPRINTFN(5, ("%s: bus_dmamem_load\n", gx->gx_dev.dv_xname));
	if (bus_dmamap_load(gx->gx_dmatag, gx->gx_ring_map, kva,
			    sizeof(struct gx_ring_data), NULL,
			    BUS_DMA_NOWAIT)) {
		bus_dmamap_destroy(gx->gx_dmatag, gx->gx_ring_map);
		bus_dmamem_unmap(gx->gx_dmatag, kva,
				 sizeof(struct gx_ring_data));
		bus_dmamem_free(gx->gx_dmatag, &seg, rseg);
		goto fail;
	}
	
	gx->gx_rdata = (struct gx_ring_data *)kva;
	bzero(gx->gx_rdata, sizeof(struct gx_ring_data));
	bzero(&gx->gx_cdata, sizeof(struct gx_chain_data));

	DPRINTFN(5, ("%s: gx->gx_rdata = 0x%x, size = %d\n",
		     gx->gx_dev.dv_xname, gx->gx_rdata,
		     sizeof(struct gx_ring_data)));

	DPRINTFN(5, ("%s: gx = 0x%x, size = %d\n",
		     gx->gx_dev.dv_xname, gx, sizeof(struct gx_softc)));

	for (i = 0; i < GX_RX_RING_CNT; i++) {
		if (bus_dmamap_create(gx->gx_dmatag, MCLBYTES, 1, MCLBYTES,
                    0, BUS_DMA_NOWAIT, &gx->gx_cdata.gx_rx_map[i]))
			printf("%s: can't create dma map\n",
			       gx->gx_dev.dv_xname);
	}

	for (i = 0; i < GX_TX_RING_CNT; i++) {
		if (bus_dmamap_create(gx->gx_dmatag, MCLBYTES, GX_NTXSEG,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &gx->gx_cdata.gx_tx_map[i]))
			printf("%s: can't create dma map\n",
			       gx->gx_dev.dv_xname);
	}

	/* Set default tuneable values. */
	gx->gx_tx_intr_delay = TUNABLE_TX_INTR_DELAY;
	gx->gx_rx_intr_delay = TUNABLE_RX_INTR_DELAY;

	/* Set up ifnet structure */
	ifp = &gx->arpcom.ac_if;
	ifp->if_softc = gx;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = gx_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = gx_start;
	ifp->if_watchdog = gx_watchdog;
	ifp->if_baudrate = 1000000000;
	ifp->if_mtu = ETHERMTU;
	ifp->if_snd.ifq_maxlen = GX_TX_RING_CNT - 1;
	DPRINTFN(5, ("%s: bcopy\n", gx->gx_dev.dv_xname));
	bcopy(gx->gx_dev.dv_xname, ifp->if_xname, IFNAMSIZ);
	
	/* figure out transciever type */
	if (gx->gx_vflags & GXF_FORCE_TBI ||
	    CSR_READ_4(gx, GX_STATUS) & GX_STAT_TBIMODE)
		gx->gx_tbimode = 1;

	/*
	 * Do MII setup.
	 */
	DPRINTFN(5, ("%s: mii setup\n", gx->gx_dev.dv_xname));
	if (!gx->gx_tbimode && (gx->gx_vflags & GXF_LIVENGOOD)) {
		gx->gx_mii.mii_ifp = ifp;
		gx->gx_mii.mii_readreg = gx_miibus_livengood_readreg;
		gx->gx_mii.mii_writereg = gx_miibus_livengood_writereg;
		gx->gx_mii.mii_statchg = gx_miibus_statchg;
	} else if (!gx->gx_tbimode && (gx->gx_vflags & GXF_CORDOVA)) {
		gx->gx_mii.mii_ifp = ifp;
		gx->gx_mii.mii_readreg = gx_miibus_cordova_readreg;
		gx->gx_mii.mii_writereg = gx_miibus_cordova_writereg;
		gx->gx_mii.mii_statchg = gx_miibus_statchg;
	} else {
		gx->gx_mii.mii_ifp = NULL;
		gx->gx_mii.mii_readreg = NULL;
		gx->gx_mii.mii_writereg = NULL;
		gx->gx_mii.mii_statchg = NULL;
	}

	if (gx->gx_tbimode) {
		/* SERDES transceiver */
		ifmedia_init(&gx->gx_media, IFM_IMASK, gx_ifmedia_upd,
		    gx_ifmedia_sts);
		ifmedia_add(&gx->gx_media,
		    IFM_ETHER|IFM_1000_SX|IFM_FDX, 0, NULL);
		ifmedia_add(&gx->gx_media, IFM_ETHER|IFM_AUTO, 0, NULL);
		ifmedia_set(&gx->gx_media, IFM_ETHER|IFM_AUTO);
	} else {
		/*
		 * Do transceiver setup.
		 */
		if (gx->gx_vflags & GXF_LIVENGOOD) {
			u_int32_t tmp;
			
			/* settings to talk to PHY */
			tmp = CSR_READ_4(gx, GX_CTRL);
			tmp |= GX_CTRL_FORCESPEED | GX_CTRL_FORCEDUPLEX |
				GX_CTRL_SET_LINK_UP;
			CSR_WRITE_4(gx, GX_CTRL, tmp);
		}

		/* GMII/MII transceiver */
		gx_phy_reset(gx);
		ifmedia_init(&gx->gx_mii.mii_media, 0, gx_ifmedia_upd,
			     gx_ifmedia_sts);
		mii_attach(&gx->gx_dev, &gx->gx_mii, 0xffffffff,
			   MII_PHY_ANY, MII_OFFSET_ANY, 0);

		
		if (LIST_FIRST(&gx->gx_mii.mii_phys) == NULL) {
			printf("%s: no PHY found!\n", gx->gx_dev.dv_xname);
			ifmedia_add(&gx->gx_mii.mii_media,
				    IFM_ETHER|IFM_MANUAL, 0, NULL);
			ifmedia_set(&gx->gx_mii.mii_media,
				    IFM_ETHER|IFM_MANUAL);
		} else
			ifmedia_set(&gx->gx_mii.mii_media,
				    IFM_ETHER|IFM_AUTO);
	}

	/*
	 * Call MI attach routines.
	 */
	DPRINTFN(5, ("%s: if_attach\n", gx->gx_dev.dv_xname));
	if_attach(ifp);
	DPRINTFN(5, ("%s: ether_ifattach\n", gx->gx_dev.dv_xname));
	ether_ifattach(ifp);
	DPRINTFN(5, ("%s: timeout_set\n", gx->gx_dev.dv_xname));

	GX_UNLOCK(gx);
	splx(s);
	return;

fail:
	GX_UNLOCK(gx);
	gx_release(gx);
	splx(s);
}

void
gx_release(struct gx_softc *gx)
{
	int i;

#ifdef notyet
	bus_generic_detach(gx->gx_dev);
	if (gx->gx_miibus)
		device_delete_child(gx->gx_dev, gx->gx_miibus);

	if (gx->gx_intrhand)
		bus_teardown_intr(gx->gx_dev, gx->gx_irq, gx->gx_intrhand);
	if (gx->gx_irq)
		bus_release_resource(gx->gx_dev, SYS_RES_IRQ, 0, gx->gx_irq);
	if (gx->gx_res)
		bus_release_resource(gx->gx_dev, SYS_RES_MEMORY,
		    GX_PCI_LOMEM, gx->gx_res);

	bus_dmamap_destroy(gx->gx_dmatag, gx->gx_ring_map);
	bus_dmamem_unmap(gx->gx_dmatag, gx->gx_rdata,
			 sizeof(struct gx_ring_data));
	bus_dmamem_free(gx->gx_dmatag, &seg, rseg);

#endif

	for (i = 0; i < GX_RX_RING_CNT; i++)
		if (gx->gx_cdata.gx_rx_map[i])
			bus_dmamap_destroy(gx->gx_dmatag,
			    gx->gx_cdata.gx_rx_map[i]);

	for (i = 0; i < GX_TX_RING_CNT; i++)
		if (gx->gx_cdata.gx_tx_map[i])
			bus_dmamap_destroy(gx->gx_dmatag,
			    gx->gx_cdata.gx_tx_map[i]);
}

void
gx_init(void *xsc)
{
	struct gx_softc *gx = (struct gx_softc *)xsc;
	struct ifnet *ifp;
	struct device *dev;
	u_int16_t *m;
	u_int32_t ctrl;
	int s, i;

	dev = &gx->gx_dev;
	ifp = &gx->arpcom.ac_if;

	s = splimp();
	GX_LOCK(gx);

	/* Disable host interrupts, halt chip. */
	gx_reset(gx);

	/* disable I/O, flush RX/TX FIFOs, and free RX/TX buffers */
	gx_stop(gx);

	/* Load our MAC address, invalidate other 15 RX addresses. */
	m = (u_int16_t *)&gx->arpcom.ac_enaddr[0];
	if (gx->gx_vflags & GXF_CORDOVA) {
	    CSR_WRITE_4(gx, GX_RX_CORDOVA_ADDR_BASE, (m[1] << 16) | m[0]);
	    CSR_WRITE_4(gx, GX_RX_CORDOVA_ADDR_BASE + 4, m[2] | GX_RA_VALID);
	    for (i = 1; i < 16; i++)
		CSR_WRITE_8(gx, GX_RX_CORDOVA_ADDR_BASE + i * 8, (u_quad_t)0);
	} else {
	    CSR_WRITE_4(gx, GX_RX_ADDR_BASE, (m[1] << 16) | m[0]);
	    CSR_WRITE_4(gx, GX_RX_ADDR_BASE + 4, m[2] | GX_RA_VALID);
	    for (i = 1; i < 16; i++)
		CSR_WRITE_8(gx, GX_RX_ADDR_BASE + i * 8, (u_quad_t)0);
	}

	/* Program multicast filter. */
	gx_setmulti(gx);

#if 1
	/* Init RX ring. */
	gx_init_rx_ring(gx);

	/* Init TX ring. */
	gx_init_tx_ring(gx);
#endif

	if (gx->gx_vflags & GXF_DMA) {
		/* set up DMA control */	
		CSR_WRITE_4(gx, gx->gx_reg.r_rx_dma_ctrl, 0x00010000);
		CSR_WRITE_4(gx, gx->gx_reg.r_tx_dma_ctrl, 0x00000000);
	}

	/* enable receiver */
	ctrl = GX_RXC_ENABLE | GX_RXC_RX_THOLD_HALF | GX_RXC_RX_BSIZE_2K;
	ctrl |= GX_RXC_BCAST_ACCEPT;

	/* Enable or disable promiscuous mode as needed. */
	if (ifp->if_flags & IFF_PROMISC)
		ctrl |= GX_RXC_UNI_PROMISC;

	/* This is required if we want to accept jumbo frames */
	if (ifp->if_mtu > ETHERMTU)
		ctrl |= GX_RXC_LONG_PKT_ENABLE;

#ifdef notyet
	/* setup receive checksum control */
	if (ifp->if_capenable & IFCAP_RXCSUM)
		CSR_WRITE_4(gx, GX_RX_CSUM_CONTROL,
		    GX_CSUM_TCP/* | GX_CSUM_IP*/);

	/* setup transmit checksum control */
	if (ifp->if_capenable & IFCAP_TXCSUM)
	        ifp->if_hwassist = GX_CSUM_FEATURES;

	ctrl |= GX_RXC_STRIP_ETHERCRC;		/* not on 82542? */
#endif
	CSR_WRITE_4(gx, GX_RX_CONTROL, ctrl);

	/* enable transmitter */
	ctrl = GX_TXC_ENABLE | GX_TXC_PAD_SHORT_PKTS | GX_TXC_COLL_RETRY_16;

	/* XXX we should support half-duplex here too... */
	ctrl |= GX_TXC_COLL_TIME_FDX;

	CSR_WRITE_4(gx, GX_TX_CONTROL, ctrl);

	/*
	 * set up recommended IPG times, which vary depending on chip type:
	 * 	IPG transmit time:  80ns
	 *	IPG receive time 1: 20ns
	 *	IPG receive time 2: 80ns
	 */
	CSR_WRITE_4(gx, GX_TX_IPG, gx->gx_ipg);

	/* set up 802.3x MAC flow control address -- 01:80:c2:00:00:01 */
	CSR_WRITE_4(gx, GX_FLOW_CTRL_BASE, GX_FLOW_CTRL_CONST);
	CSR_WRITE_4(gx, GX_FLOW_CTRL_BASE+4, GX_FLOW_CTRL_CONST_HIGH);

	/* set up 802.3x MAC flow control type -- 88:08 */
	CSR_WRITE_4(gx, GX_FLOW_CTRL_TYPE, GX_FLOW_CTRL_TYPE_CONST);

	/* Set up tuneables */
	CSR_WRITE_4(gx, gx->gx_reg.r_rx_delay, gx->gx_rx_intr_delay);
	CSR_WRITE_4(gx, gx->gx_reg.r_tx_delay, gx->gx_tx_intr_delay);

	ctrl = 0;

#if 0
	if (gx->gx_vflags & GXF_CORDOVA) {
		u_int16_t cfg1, cfg2, gpio;
		gx_read_eeprom(gx, (caddr_t)&cfg1, GX_EEMAP_INIT1, 1);
		gx_read_eeprom(gx, (caddr_t)&cfg2, GX_EEMAP_INIT2, 1);
		gx_read_eeprom(gx, (caddr_t)&gpio, GX_EEMAP_SWDPIN, 1);

		if (cfg1 & GX_EEMAP_INIT1_ILOS)
			ctrl |= GX_CTRL_INVERT_LOS;

		ctrl |= ((gpio >> GX_EEMAP_GPIO_DIR_SHIFT) & 0xf) <<
			GX_CTRL_GPIO_DIR_SHIFT;

		ctrl |= ((gpio >> GX_EEMAP_GPIO_SHIFT) & 0xf) <<
			GX_CTRL_GPIO_SHIFT;
	}
#endif

	/*
	 * Configure chip for correct operation.
	 */
	ctrl |= GX_CTRL_DUPLEX;
#if BYTE_ORDER == BIG_ENDIAN
	ctrl |= GX_CTRL_BIGENDIAN;
#endif
	ctrl |= GX_CTRL_VLAN_ENABLE;

	if (gx->gx_tbimode) {
		/*
		 * It seems that TXCW must be initialized from the EEPROM
		 * manually.
		 *
		 * XXX
		 * should probably read the eeprom and re-insert the
		 * values here.
		 */
#define TXCONFIG_WORD	0x000001A0
		CSR_WRITE_4(gx, GX_TX_CONFIG, TXCONFIG_WORD);

		/* turn on hardware autonegotiate */
		GX_SETBIT(gx, GX_TX_CONFIG, GX_TXCFG_AUTONEG);
	} else {
		/*
		 * Auto-detect speed from PHY, instead of using direct
		 * indication.  The SLU bit doesn't force the link, but
		 * must be present for ASDE to work.
		 */
		gx_phy_reset(gx);
		ctrl |= GX_CTRL_SET_LINK_UP | GX_CTRL_AUTOSPEED;
	}

	/*
	 * Take chip out of reset and start it running.
	 */
	CSR_WRITE_4(gx, GX_CTRL, ctrl);

	/* Turn interrupts on. */
	CSR_WRITE_4(gx, GX_INT_MASK_SET, GX_INT_WANTED);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	/*
	 * Set the current media.
	 */
	if (gx->gx_mii.mii_ifp != NULL) {
		mii_mediachg(&gx->gx_mii);
	} else {
		struct ifmedia *ifm = &gx->gx_media;
		int tmp = ifm->ifm_media;
		ifm->ifm_media = ifm->ifm_cur->ifm_media;
		gx_ifmedia_upd(ifp);
		ifm->ifm_media = tmp;
	}

	/*
	 * XXX
	 * Have the LINK0 flag force the link in TBI mode.
	 */
	if (gx->gx_tbimode && ifp->if_flags & IFF_LINK0) {
		GX_CLRBIT(gx, GX_TX_CONFIG, GX_TXCFG_AUTONEG);
		GX_SETBIT(gx, GX_CTRL, GX_CTRL_SET_LINK_UP);
	}

#if 0
printf("66mhz: %s  64bit: %s\n",
	CSR_READ_4(gx, GX_STATUS) & GX_STAT_PCI66 ? "yes" : "no",
	CSR_READ_4(gx, GX_STATUS) & GX_STAT_BUS64 ? "yes" : "no");
#endif

	GX_UNLOCK(gx);
	splx(s);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
void
gx_shutdown(void *xsc)
{
	struct gx_softc *gx = (struct gx_softc *)xsc;

	gx_reset(gx);
	gx_stop(gx);
}

#ifdef notyet
int
gx_detach(void *xsc)
{
	struct gx_softc *gx = (struct gx_softc *)xsc;
	struct ifnet *ifp;
	int s;

	s = splimp();

	ifp = &gx->arpcom.ac_if;
	GX_LOCK(gx);

	ether_ifdetach(ifp, ETHER_BPF_SUPPORTED);
	gx_reset(gx);
	gx_stop(gx);
	ifmedia_removeall(&gx->gx_media);
	gx_release(gx);

	contigfree(gx->gx_rdata, sizeof(struct gx_ring_data), M_DEVBUF);
		
	GX_UNLOCK(gx);
	mtx_destroy(&gx->gx_mtx);
	splx(s);

	return (0);
}
#endif

void
gx_eeprom_getword(struct gx_softc *gx, int addr, u_int16_t *dest)
{
	u_int16_t word = 0;
	u_int32_t base, reg;
	int x;

	addr = (GX_EE_OPC_READ << GX_EE_ADDR_SIZE) |
	    (addr & ((1 << GX_EE_ADDR_SIZE) - 1));

	base = CSR_READ_4(gx, GX_EEPROM_CTRL);
	base &= ~(GX_EE_DATA_OUT | GX_EE_DATA_IN | GX_EE_CLOCK);
	base |= GX_EE_SELECT;

	CSR_WRITE_4(gx, GX_EEPROM_CTRL, base);

	for (x = 1 << ((GX_EE_OPC_SIZE + GX_EE_ADDR_SIZE) - 1); x; x >>= 1) {
		reg = base | (addr & x ? GX_EE_DATA_IN : 0);
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, reg);
		DELAY(10);
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, reg | GX_EE_CLOCK);
		DELAY(10);
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, reg);
		DELAY(10);
	}

	for (x = 1 << 15; x; x >>= 1) {
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, base | GX_EE_CLOCK);
		DELAY(10);
		reg = CSR_READ_4(gx, GX_EEPROM_CTRL);
		if (reg & GX_EE_DATA_OUT)
			word |= x;
		CSR_WRITE_4(gx, GX_EEPROM_CTRL, base);
		DELAY(10);
	}

	CSR_WRITE_4(gx, GX_EEPROM_CTRL, base & ~GX_EE_SELECT);
	DELAY(10);

	*dest = word;
}
	
int
gx_read_eeprom(struct gx_softc *gx, caddr_t dest, int off, int cnt)
{
	u_int16_t *word;
	int i;

	word = (u_int16_t *)dest;
	for (i = 0; i < cnt; i ++) {
		gx_eeprom_getword(gx, off + i, word);
		word++;
	}
	return (0);
}

/*
 * Set media options.
 */
int
gx_ifmedia_upd(struct ifnet *ifp)
{
	struct gx_softc	*gx;
	struct ifmedia *ifm;
	struct mii_data *mii;

	gx = ifp->if_softc;

	if (gx->gx_tbimode) {
		ifm = &gx->gx_media;
		if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
			return (EINVAL);
		switch (IFM_SUBTYPE(ifm->ifm_media)) {
		case IFM_AUTO:
			GX_SETBIT(gx, GX_CTRL, GX_CTRL_LINK_RESET);
			GX_SETBIT(gx, GX_TX_CONFIG, GX_TXCFG_AUTONEG);
			GX_CLRBIT(gx, GX_CTRL, GX_CTRL_LINK_RESET);
			break;
		case IFM_1000_SX:
			printf("%s: manual config not supported yet.\n",
			       gx->gx_dev.dv_xname);
#if 0
			GX_CLRBIT(gx, GX_TX_CONFIG, GX_TXCFG_AUTONEG);
			config = /* bit symbols for 802.3z */0;
			ctrl |= GX_CTRL_SET_LINK_UP;
			if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
				ctrl |= GX_CTRL_DUPLEX;
#endif
			break;
		default:
			return (EINVAL);
		}
	} else {
		ifm = &gx->gx_mii.mii_media;

		/*
		 * 1000TX half duplex does not work.
		 */
		if (IFM_TYPE(ifm->ifm_media) == IFM_ETHER &&
		    IFM_SUBTYPE(ifm->ifm_media) == IFM_1000_TX &&
		    (IFM_OPTIONS(ifm->ifm_media) & IFM_FDX) == 0)
			return (EINVAL);
		mii = &gx->gx_mii;
		mii_mediachg(mii);
	}
	return (0);
}

/*
 * Report current media status.
 */
void
gx_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct gx_softc	*gx;
	struct mii_data *mii;
	u_int32_t status;

	gx = ifp->if_softc;

	if (gx->gx_tbimode) {
		ifmr->ifm_status = IFM_AVALID;
		ifmr->ifm_active = IFM_ETHER;

		status = CSR_READ_4(gx, GX_STATUS);
		if ((status & GX_STAT_LINKUP) == 0)
			return;

		ifmr->ifm_status |= IFM_ACTIVE;
		ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;
	} else {
		mii = &gx->gx_mii;
		mii_pollstat(mii);
		if ((mii->mii_media_active & (IFM_1000_TX | IFM_HDX)) ==
		    (IFM_1000_TX | IFM_HDX))
			mii->mii_media_active = IFM_ETHER | IFM_NONE;
		ifmr->ifm_active = mii->mii_media_active;
		ifmr->ifm_status = mii->mii_media_status;
	}
}

void 
gx_mii_shiftin(struct gx_softc *gx, int data, int length)
{
        u_int32_t reg, x;

	/*
	 * Set up default GPIO direction + PHY data out.
	 */
	reg = CSR_READ_4(gx, GX_CTRL);
	reg &= ~(GX_CTRL_GPIO_DIR_MASK | GX_CTRL_PHY_IO | GX_CTRL_PHY_CLK);
	reg |= GX_CTRL_GPIO_DIR | GX_CTRL_PHY_IO_DIR;

        /*
         * Shift in data to PHY.
         */
	for (x = 1 << (length - 1); x; x >>= 1) {
                if (data & x)
                        reg |= GX_CTRL_PHY_IO;
                else
                        reg &= ~GX_CTRL_PHY_IO;
                CSR_WRITE_4(gx, GX_CTRL, reg);
                DELAY(10);
                CSR_WRITE_4(gx, GX_CTRL, reg | GX_CTRL_PHY_CLK);
                DELAY(10);
                CSR_WRITE_4(gx, GX_CTRL, reg);
                DELAY(10);
        }
}

u_int16_t 
gx_mii_shiftout(struct gx_softc *gx)
{
        u_int32_t reg;
	u_int16_t data;
	int x;

	/*
	 * Set up default GPIO direction + PHY data in.
	 */
	reg = CSR_READ_4(gx, GX_CTRL);
	reg &= ~(GX_CTRL_GPIO_DIR_MASK | GX_CTRL_PHY_IO | GX_CTRL_PHY_CLK);
	reg |= GX_CTRL_GPIO_DIR;

	CSR_WRITE_4(gx, GX_CTRL, reg);
	DELAY(10);
	CSR_WRITE_4(gx, GX_CTRL, reg | GX_CTRL_PHY_CLK);
	DELAY(10);
	CSR_WRITE_4(gx, GX_CTRL, reg);
	DELAY(10);
	/*
	 * Shift out data from PHY.
	 */
	data = 0;
	for (x = 1 << 15; x; x >>= 1) {
		CSR_WRITE_4(gx, GX_CTRL, reg | GX_CTRL_PHY_CLK);
		DELAY(10);
		if (CSR_READ_4(gx, GX_CTRL) & GX_CTRL_PHY_IO)
			data |= x;
		CSR_WRITE_4(gx, GX_CTRL, reg);
		DELAY(10);
	}
	CSR_WRITE_4(gx, GX_CTRL, reg | GX_CTRL_PHY_CLK);
	DELAY(10);
	CSR_WRITE_4(gx, GX_CTRL, reg);
	DELAY(10);

	return (data);
}

int
gx_miibus_livengood_readreg(struct device *dev, int phy, int reg)
{
	struct gx_softc *gx = (struct gx_softc *)dev;

	if (gx->gx_tbimode)
		return (0);

	gx_mii_shiftin(gx, GX_PHY_PREAMBLE, GX_PHY_PREAMBLE_LEN);
	gx_mii_shiftin(gx, (GX_PHY_SOF << 12) | (GX_PHY_OP_READ << 10) |
	    (phy << 5) | reg, GX_PHY_READ_LEN);
	return (gx_mii_shiftout(gx));
}

void
gx_miibus_livengood_writereg(struct device *dev, int phy, int reg, int value)
{
	struct gx_softc *gx = (struct gx_softc *)dev;

	if (gx->gx_tbimode)
		return;

	gx_mii_shiftin(gx, GX_PHY_PREAMBLE, GX_PHY_PREAMBLE_LEN);
	gx_mii_shiftin(gx, (GX_PHY_SOF << 30) | (GX_PHY_OP_WRITE << 28) |
	    (phy << 23) | (reg << 18) | (GX_PHY_TURNAROUND << 16) |
	    (value & 0xffff), GX_PHY_WRITE_LEN);
}

/*
 * gx_miibus_cordova_readreg:	[mii interface function]
 *
 *	Read a PHY register on the GMII.
 */
int
gx_miibus_cordova_readreg(struct device *self, int phy, int reg)
{
	struct gx_softc *sc = (void *) self;
	uint32_t mdic;
	int i, rv;

	CSR_WRITE_4(sc, GX_MDIC, GX_MDIC_OP_READ | GX_MDIC_PHYADD(phy) |
	    GX_MDIC_REGADD(reg));

	for (i = 0; i < 100; i++) {
		mdic = CSR_READ_4(sc, GX_MDIC);
		if (mdic & GX_MDIC_READY)
			break;
		delay(10);
	}

	if ((mdic & GX_MDIC_READY) == 0) {
		printf("%s: GX_MDIC read timed out: phy %d reg %d\n",
		    sc->gx_dev.dv_xname, phy, reg);
		rv = 0;
	} else if (mdic & GX_MDIC_E) {
		/* This is normal if no PHY is present. */
		DPRINTFN(2, ("%s: GX_MDIC read error: phy %d reg %d\n",
			     sc->gx_dev.dv_xname, phy, reg));
		rv = 0;
	} else {
		rv = GX_MDIC_DATA(mdic);
		if (rv == 0xffff)
			rv = 0;
	}

	return (rv);
}

/*
 * gx_miibus_cordova_writereg:	[mii interface function]
 *
 *	Write a PHY register on the GMII.
 */
void
gx_miibus_cordova_writereg(struct device *self, int phy, int reg, int val)
{
	struct gx_softc *sc = (void *) self;
	uint32_t mdic;
	int i;

	CSR_WRITE_4(sc, GX_MDIC, GX_MDIC_OP_WRITE | GX_MDIC_PHYADD(phy) |
	    GX_MDIC_REGADD(reg) | GX_MDIC_DATA(val));

	for (i = 0; i < 100; i++) {
		mdic = CSR_READ_4(sc, GX_MDIC);
		if (mdic & GX_MDIC_READY)
			break;
		delay(10);
	}

	if ((mdic & GX_MDIC_READY) == 0)
		printf("%s: GX_MDIC write timed out: phy %d reg %d\n",
		    sc->gx_dev.dv_xname, phy, reg);
	else if (mdic & GX_MDIC_E)
		printf("%s: GX_MDIC write error: phy %d reg %d\n",
		    sc->gx_dev.dv_xname, phy, reg);
}

void
gx_miibus_statchg(struct device *dev)
{
	struct gx_softc *gx = (struct gx_softc *)dev;
	struct mii_data *mii;
	int reg, s;

	if (gx->gx_tbimode)
		return;

	/*
	 * Set flow control behavior to mirror what PHY negotiated.
	 */
	mii = &gx->gx_mii;

	s = splimp();
	GX_LOCK(gx);

	reg = CSR_READ_4(gx, GX_CTRL);
	if (mii->mii_media_active & IFM_FLAG0)
		reg |= GX_CTRL_RX_FLOWCTRL;
	else
		reg &= ~GX_CTRL_RX_FLOWCTRL;
	if (mii->mii_media_active & IFM_FLAG1)
		reg |= GX_CTRL_TX_FLOWCTRL;
	else
		reg &= ~GX_CTRL_TX_FLOWCTRL;
	CSR_WRITE_4(gx, GX_CTRL, reg);

	GX_UNLOCK(gx);
	splx(s);
}

int
gx_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct gx_softc	*gx = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;
	struct mii_data *mii;

	s = splimp();
	GX_LOCK(gx);

	if ((error = ether_ioctl(ifp, &gx->arpcom, command, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			gx_init(gx);
			arp_ifinit(&gx->arpcom, ifa);
			break;
#endif /* INET */
		default:
			gx_init(gx);
			break;
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > GX_MAX_MTU) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
			gx_init(gx);
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			   ((ifp->if_flags & IFF_PROMISC) != 
			    (gx->gx_if_flags & IFF_PROMISC))) {
				if (ifp->if_flags & IFF_PROMISC)
					GX_SETBIT(gx, GX_RX_CONTROL,
						  GX_RXC_UNI_PROMISC);
				else 
					GX_CLRBIT(gx, GX_RX_CONTROL,
						  GX_RXC_UNI_PROMISC);
			} else
				gx_init(gx);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				gx_stop(gx);
			}
		}

		gx->gx_if_flags = ifp->if_flags;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (ifp->if_flags & IFF_RUNNING)
			gx_setmulti(gx);
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		if (gx->gx_tbimode) {
			error = ifmedia_ioctl(ifp, ifr, &gx->gx_media,
					      command);
		} else {
			mii = &gx->gx_mii;
			error = ifmedia_ioctl(ifp, ifr, &mii->mii_media,
					      command);
		}
	  break;
	default:
		error = EINVAL;
		break;
	}

	GX_UNLOCK(gx);
	splx(s);
	return (error);
}

void
gx_phy_reset(struct gx_softc *gx)
{
	int reg;

	GX_SETBIT(gx, GX_CTRL, GX_CTRL_SET_LINK_UP);

	if (gx->gx_vflags & GXF_CORDOVA) {
		/* post-livingood (cordova) only */
		GX_SETBIT(gx, GX_CTRL, GX_CTRL_PHY_RESET);
		DELAY(1000);
		GX_CLRBIT(gx, GX_CTRL, GX_CTRL_PHY_RESET);
	} else {
		/*
		 * PHY reset is active low.
		 */
		reg = CSR_READ_4(gx, GX_CTRL_EXT);
		reg &= ~(GX_CTRLX_GPIO_DIR_MASK | GX_CTRLX_PHY_RESET);
		reg |= GX_CTRLX_GPIO_DIR;
		
		CSR_WRITE_4(gx, GX_CTRL_EXT, reg | GX_CTRLX_PHY_RESET);
		DELAY(10);
		CSR_WRITE_4(gx, GX_CTRL_EXT, reg);
		DELAY(10);
		CSR_WRITE_4(gx, GX_CTRL_EXT, reg | GX_CTRLX_PHY_RESET);
		DELAY(10);
	}
}

void
gx_reset(struct gx_softc *gx)
{

	/* Disable host interrupts. */
	CSR_WRITE_4(gx, GX_INT_MASK_CLR, GX_INT_ALL);

	/* reset chip (THWAP!) */
	GX_SETBIT(gx, GX_CTRL, GX_CTRL_DEVICE_RESET);
	DELAY(10);
}

void
gx_stop(struct gx_softc *gx)
{
	struct ifnet *ifp;

	ifp = &gx->arpcom.ac_if;

	/* reset and flush transmitter */
	CSR_WRITE_4(gx, GX_TX_CONTROL, GX_TXC_RESET);

	/* reset and flush receiver */
	CSR_WRITE_4(gx, GX_RX_CONTROL, GX_RXC_RESET);

	/* reset link */
	if (gx->gx_tbimode)
		GX_SETBIT(gx, GX_CTRL, GX_CTRL_LINK_RESET);

#if 1
	/* Free the RX lists. */
	gx_free_rx_ring(gx);

	/* Free TX buffers. */
	gx_free_tx_ring(gx);
#endif

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

void
gx_watchdog(struct ifnet *ifp)
{
	struct gx_softc	*gx;

	gx = ifp->if_softc;

	printf("%s: watchdog timeout -- resetting\n", gx->gx_dev.dv_xname);
	gx_reset(gx);
	gx_init(gx);

	ifp->if_oerrors++;
}

/*
 * Intialize a receive ring descriptor.
 */
int
gx_newbuf(struct gx_softc *gx, int idx, struct mbuf *m)
{
	struct mbuf *m_new = NULL;
	struct gx_rx_desc *r;
	bus_dmamap_t rxmap = gx->gx_cdata.gx_rx_map[idx];


	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: mbuf alloc failed -- packet dropped\n",
			       gx->gx_dev.dv_xname);
			return (ENOBUFS);
		}
		MCLGET(m_new, M_DONTWAIT);
		if ((m_new->m_flags & M_EXT) == 0) {
			printf("%s: cluster alloc failed -- packet dropped\n",
			       gx->gx_dev.dv_xname);
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	} else {
		m->m_len = m->m_pkthdr.len = MCLBYTES;
		m->m_data = m->m_ext.ext_buf;
		m->m_next = NULL;
		m_new = m;
	}

	if (bus_dmamap_load_mbuf(gx->gx_dmatag, rxmap, m_new, BUS_DMA_NOWAIT))
		return(ENOBUFS);

	/*
	 * XXX
	 * this will _NOT_ work for large MTU's; it will overwrite
	 * the end of the buffer.  E.g.: take this out for jumbograms,
	 * but then that breaks alignment.
	 */
	if (gx->arpcom.ac_if.if_mtu <= ETHERMTU)
		m_adj(m_new, ETHER_ALIGN);

	gx->gx_cdata.gx_rx_chain[idx] = m_new;
	r = &gx->gx_rdata->gx_rx_ring[idx];
 	r->rx_addr = rxmap->dm_segs[0].ds_addr;
	if (gx->arpcom.ac_if.if_mtu <= ETHERMTU)
		r->rx_addr += ETHER_ALIGN;
	r->rx_staterr = 0;

	return (0);
}

/*
 * The receive ring can have up to 64K descriptors, which at 2K per mbuf
 * cluster, could add up to 128M of memory.  Due to alignment constraints,
 * the number of descriptors must be a multiple of 8.  For now, we
 * allocate 256 entries and hope that our CPU is fast enough to keep up
 * with the NIC.
 */
int
gx_init_rx_ring(struct gx_softc *gx)
{
	int i, error;

	for (i = 0; i < GX_RX_RING_CNT; i++) {
		error = gx_newbuf(gx, i, NULL);
		if (error)
			return (error);
	}

	/* bring receiver out of reset state, leave disabled */
	CSR_WRITE_4(gx, GX_RX_CONTROL, 0);

	/* set up ring registers */
	CSR_WRITE_8(gx, gx->gx_reg.r_rx_base,
		    (u_quad_t)(gx->gx_ring_map->dm_segs[0].ds_addr +
			       offsetof(struct gx_ring_data, gx_rx_ring)));

	CSR_WRITE_4(gx, gx->gx_reg.r_rx_length,
	    GX_RX_RING_CNT * sizeof(struct gx_rx_desc));
	CSR_WRITE_4(gx, gx->gx_reg.r_rx_head, 0);
	CSR_WRITE_4(gx, gx->gx_reg.r_rx_tail, GX_RX_RING_CNT - 1);
	gx->gx_rx_tail_idx = 0;

	return (0);
}

void
gx_free_rx_ring(struct gx_softc *gx)
{
	int i;
	for (i = 0; i < GX_RX_RING_CNT; i++) {
		if (gx->gx_cdata.gx_rx_chain[i] != NULL) {
			bus_dmamap_unload(gx->gx_dmatag,
					  gx->gx_cdata.gx_rx_map[i]);
			m_freem(gx->gx_cdata.gx_rx_chain[i]);
			gx->gx_cdata.gx_rx_chain[i] = NULL;
		}
	}

	bzero((void *)gx->gx_rdata->gx_rx_ring,
	    GX_RX_RING_CNT * sizeof(struct gx_rx_desc));

	/* release any partially-received packet chain */
	if (gx->gx_pkthdr != NULL) {
		m_freem(gx->gx_pkthdr);
		gx->gx_pkthdr = NULL;
	}
}

int
gx_init_tx_ring(struct gx_softc *gx)
{
	/* bring transmitter out of reset state, leave disabled */
	CSR_WRITE_4(gx, GX_TX_CONTROL, 0);

	/* set up ring registers */
	CSR_WRITE_8(gx, gx->gx_reg.r_tx_base,
		    (u_quad_t)(gx->gx_ring_map->dm_segs[0].ds_addr +
			       offsetof(struct gx_ring_data, gx_tx_ring)));
	CSR_WRITE_4(gx, gx->gx_reg.r_tx_length,
	    GX_TX_RING_CNT * sizeof(struct gx_tx_desc));
	CSR_WRITE_4(gx, gx->gx_reg.r_tx_head, 0);
	CSR_WRITE_4(gx, gx->gx_reg.r_tx_tail, 0);
	gx->gx_tx_head_idx = 0;
	gx->gx_tx_tail_idx = 0;
	gx->gx_txcnt = 0;

	/* set up initial TX context */
	gx->gx_txcontext = GX_TXCONTEXT_NONE;

	return (0);
}

void
gx_free_tx_ring(struct gx_softc *gx)
{
	int i;

	for (i = 0; i < GX_TX_RING_CNT; i++) {
		if (gx->gx_cdata.gx_tx_chain[i] != NULL) {
			bus_dmamap_unload(gx->gx_dmatag,
					  gx->gx_cdata.gx_tx_map[i]);
			m_freem(gx->gx_cdata.gx_tx_chain[i]);
			gx->gx_cdata.gx_tx_chain[i] = NULL;
		}
	}

	bzero((void *)gx->gx_rdata->gx_tx_ring,
	    GX_TX_RING_CNT * sizeof(struct gx_tx_desc));
}

void
gx_setmulti(struct gx_softc *gx)
{
	int i;

	if (gx->gx_vflags & GXF_CORDOVA) {
		/* wipe out the multicast table */
		for (i = 1; i < 128; i++)
			CSR_WRITE_4(gx, GX_CORDOVA_MULTICAST_BASE + i * 4, 0);
	} else {
		/* wipe out the multicast table */
		for (i = 1; i < 128; i++)
			CSR_WRITE_4(gx, GX_MULTICAST_BASE + i * 4, 0);
	}
}

void
gx_rxeof(struct gx_softc *gx)
{
	struct gx_rx_desc *rx;
	struct ifnet *ifp;
	int idx, staterr, len;
	struct mbuf *m;

	gx->gx_rx_interrupts++;

	ifp = &gx->arpcom.ac_if;
	idx = gx->gx_rx_tail_idx;

	while (gx->gx_rdata->gx_rx_ring[idx].rx_staterr &
	       GX_RXSTAT_COMPLETED) {

		rx = &gx->gx_rdata->gx_rx_ring[idx];
		m = gx->gx_cdata.gx_rx_chain[idx];
		/*
		 * gx_newbuf overwrites status and length bits, so we 
		 * make a copy of them here.
		 */
		len = rx->rx_len;
		staterr = rx->rx_staterr;

		if (staterr & GX_INPUT_ERROR)
			goto ierror;

		if (gx_newbuf(gx, idx, NULL) == ENOBUFS)
			goto ierror;

		GX_INC(idx, GX_RX_RING_CNT);

		if (staterr & GX_RXSTAT_INEXACT_MATCH) {
			/*
			 * multicast packet, must verify against
			 * multicast address.
			 */
		}

		if ((staterr & GX_RXSTAT_END_OF_PACKET) == 0) {
			if (gx->gx_pkthdr == NULL) {
				m->m_len = len;
				m->m_pkthdr.len = len;
				gx->gx_pkthdr = m;
				gx->gx_pktnextp = &m->m_next;
			} else {
				m->m_len = len;
				m->m_flags &= ~M_PKTHDR;
				gx->gx_pkthdr->m_pkthdr.len += len;
				*(gx->gx_pktnextp) = m;
				gx->gx_pktnextp = &m->m_next;
			}
			continue;
		}

		if (gx->gx_pkthdr == NULL) {
			m->m_len = len;
			m->m_pkthdr.len = len;
		} else {
			m->m_len = len;
			m->m_flags &= ~M_PKTHDR;
			gx->gx_pkthdr->m_pkthdr.len += len;
			*(gx->gx_pktnextp) = m;
			m = gx->gx_pkthdr;
			gx->gx_pkthdr = NULL;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

#ifdef notyet
#define IP_CSMASK 	(GX_RXSTAT_IGNORE_CSUM | GX_RXSTAT_HAS_IP_CSUM)
#define TCP_CSMASK \
    (GX_RXSTAT_IGNORE_CSUM | GX_RXSTAT_HAS_TCP_CSUM | GX_RXERR_TCP_CSUM)
		if (ifp->if_capenable & IFCAP_RXCSUM) {
#if 0
			/*
			 * Intel Erratum #23 indicates that the Receive IP
			 * Checksum offload feature has been completely
			 * disabled.
			 */
			if ((staterr & IP_CSUM_MASK) == GX_RXSTAT_HAS_IP_CSUM) {
				m->m_pkthdr.csum_flags |= CSUM_IP_CHECKED;
				if ((staterr & GX_RXERR_IP_CSUM) == 0)
					m->m_pkthdr.csum_flags |= CSUM_IP_VALID;
			}
#endif
			if ((staterr & TCP_CSMASK) == GX_RXSTAT_HAS_TCP_CSUM) {
				m->m_pkthdr.csum_flags |=
				    CSUM_DATA_VALID | CSUM_PSEUDO_HDR;
				m->m_pkthdr.csum_data = 0xffff;
			}
		}

#if NVLAN > 0
		/*
		 * If we received a packet with a vlan tag, pass it
		 * to vlan_input() instead of ether_input().
		 */
		if (staterr & GX_RXSTAT_VLAN_PKT) {
			VLAN_INPUT_TAG(eh, m, rx->rx_special);
			continue;
		}
#endif
#endif

#if NBPFILTER > 0
		/*
		 * Handle BPF listeners. Let the BPF user see the packet.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		ether_input_mbuf(ifp, m);
		continue;

  ierror:
		ifp->if_ierrors++;
		gx_newbuf(gx, idx, m);

		/* 
		 * XXX
		 * this isn't quite right.  Suppose we have a packet that
		 * spans 5 descriptors (9K split into 2K buffers).  If
		 * the 3rd descriptor sets an error, we need to ignore
		 * the last two.  The way things stand now, the last two
		 * will be accepted as a single packet.
		 *
		 * we don't worry about this -- the chip may not set an
		 * error in this case, and the checksum of the upper layers
		 * will catch the error.
		 */
		if (gx->gx_pkthdr != NULL) {
			m_freem(gx->gx_pkthdr);
			gx->gx_pkthdr = NULL;
		}
		GX_INC(idx, GX_RX_RING_CNT);
	}

	gx->gx_rx_tail_idx = idx;
	if (--idx < 0)
		idx = GX_RX_RING_CNT - 1;
	CSR_WRITE_4(gx, gx->gx_reg.r_rx_tail, idx);
}

void
gx_txeof(struct gx_softc *gx)
{
	struct ifnet *ifp;
	int idx, cnt;

	gx->gx_tx_interrupts++;

	ifp = &gx->arpcom.ac_if;
	idx = gx->gx_tx_head_idx;
	cnt = gx->gx_txcnt;

	/*
	 * If the system chipset performs I/O write buffering, it is 
	 * possible for the PIO read of the head descriptor to bypass the
	 * memory write of the descriptor, resulting in reading a descriptor
	 * which has not been updated yet.
	 */
	while (cnt) {
		struct gx_tx_desc_old *tx;

		tx = (struct gx_tx_desc_old *)&gx->gx_rdata->gx_tx_ring[idx];
		cnt--;

		if ((tx->tx_command & GX_TXOLD_END_OF_PKT) == 0) {
			GX_INC(idx, GX_TX_RING_CNT);
			continue;
		}

		if ((tx->tx_status & GX_TXSTAT_DONE) == 0)
			break;

		ifp->if_opackets++;

		m_freem(gx->gx_cdata.gx_tx_chain[idx]);
		gx->gx_cdata.gx_tx_chain[idx] = NULL;
		bus_dmamap_unload(gx->gx_dmatag, gx->gx_cdata.gx_tx_map[idx]);
		
		gx->gx_txcnt = cnt;
		ifp->if_timer = 0;

		GX_INC(idx, GX_TX_RING_CNT);
		gx->gx_tx_head_idx = idx;
	}

	if (gx->gx_txcnt == 0)
		ifp->if_flags &= ~IFF_OACTIVE;
}

int
gx_intr(void *xsc)
{
	struct gx_softc	*gx;
	struct ifnet *ifp;
	u_int32_t intr;
	int s;

	gx = xsc;
	ifp = &gx->arpcom.ac_if;

	s = splimp();

	gx->gx_interrupts++;

	/* Disable host interrupts. */
	CSR_WRITE_4(gx, GX_INT_MASK_CLR, GX_INT_ALL);

	/*
	 * find out why we're being bothered.
	 * reading this register automatically clears all bits.
	 */
	intr = CSR_READ_4(gx, GX_INT_READ);

	if (intr) {
		DPRINTFN(8, ("%s: gx_intr.  intr = 0x%x\n",
			     gx->gx_dev.dv_xname, intr));
	}

	/* Check RX return ring producer/consumer */
	if (intr & (GX_INT_RCV_TIMER | GX_INT_RCV_THOLD | GX_INT_RCV_OVERRUN))
	    gx_rxeof(gx);
	
	/* Check TX ring producer/consumer */
	if (intr & (GX_INT_XMIT_DONE | GX_INT_XMIT_EMPTY))
	    gx_txeof(gx);
	
	/*
	 * handle other interrupts here.
	 */
	
	/*
	 * Link change interrupts are not reliable; the interrupt may
	 * not be generated if the link is lost.  However, the register
	 * read is reliable, so check that.  Use SEQ errors to possibly
	 * indicate that the link has changed.
	 */
#ifdef GX_DEBUG
	if (gxdebug >= 1) {
	    if (intr & GX_INT_LINK_CHANGE) {
		int status = CSR_READ_4(gx, GX_STATUS);
		printf("%s: link %s\n", gx->gx_dev.dv_xname,
		       (status & GX_STAT_LINKUP) ? "up" :
		       "down");
	    }
	}
#endif

	/* Turn interrupts on. */
	CSR_WRITE_4(gx, GX_INT_MASK_SET, GX_INT_WANTED);

	if (ifp->if_flags & IFF_RUNNING && ifp->if_snd.ifq_head != NULL)
		gx_start(ifp);

	splx(s);

	return (1);
}

/*
 * Encapsulate an mbuf chain in the tx ring by coupling the mbuf data
 * pointers to descriptors.
 */
int
gx_encap(struct gx_softc *gx, struct mbuf *m_head)
{
	struct gx_tx_desc_data *tx = NULL;
#ifdef notyet
	struct gx_tx_desc_ctx *tctx;
#endif
	bus_dmamap_t txmap;
	int i = 0;
	int idx, cnt, /*csumopts, */ txcontext;

#if NVLAN > 0
	struct ifvlan *ifv = NULL;

	if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL &&
	    m_head->m_pkthdr.rcvif->if_type == IFT_L2VLAN)
		ifv = m_head->m_pkthdr.rcvif->if_softc;
#endif

	cnt = gx->gx_txcnt;
	idx = gx->gx_tx_tail_idx;
	txcontext = gx->gx_txcontext;

	/*
	 * Insure we have at least 4 descriptors pre-allocated.
	 */
	if (cnt >= GX_TX_RING_CNT - 4)
		return (ENOBUFS);

#ifdef notyet
	/*
	 * Set up the appropriate offload context if necessary.
	 */
	csumopts = 0;
	if (m_head->m_pkthdr.csum_flags) {
		if (m_head->m_pkthdr.csum_flags & CSUM_IP)
			csumopts |= GX_TXTCP_OPT_IP_CSUM;
		if (m_head->m_pkthdr.csum_flags & CSUM_TCP) {
			csumopts |= GX_TXTCP_OPT_TCP_CSUM;
			txcontext = GX_TXCONTEXT_TCPIP;
		} else if (m_head->m_pkthdr.csum_flags & CSUM_UDP) {
			csumopts |= GX_TXTCP_OPT_TCP_CSUM;
			txcontext = GX_TXCONTEXT_UDPIP;
		} else if (txcontext == GX_TXCONTEXT_NONE)
			txcontext = GX_TXCONTEXT_TCPIP;
		if (txcontext == gx->gx_txcontext)
			goto context_done;

		tctx = (struct gx_tx_desc_ctx *)&gx->gx_rdata->gx_tx_ring[idx];
		tctx->tx_ip_csum_start = ETHER_HDR_LEN;
		tctx->tx_ip_csum_end = ETHER_HDR_LEN + sizeof(struct ip) - 1;
		tctx->tx_ip_csum_offset = 
		    ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
		tctx->tx_tcp_csum_start = ETHER_HDR_LEN + sizeof(struct ip);
		tctx->tx_tcp_csum_end = 0;
		if (txcontext == GX_TXCONTEXT_TCPIP)
			tctx->tx_tcp_csum_offset = ETHER_HDR_LEN +
			    sizeof(struct ip) + offsetof(struct tcphdr, th_sum);
		else
			tctx->tx_tcp_csum_offset = ETHER_HDR_LEN +
			    sizeof(struct ip) + offsetof(struct udphdr, uh_sum);
		tctx->tx_command = GX_TXCTX_EXTENSION | GX_TXCTX_INT_DELAY;
		tctx->tx_type = 0;
		tctx->tx_status = 0;
		GX_INC(idx, GX_TX_RING_CNT);
		cnt++;
	}
context_done:
#endif

	/*
 	 * Start packing the mbufs in this chain into the transmit
	 * descriptors.  Stop when we run out of descriptors or hit
	 * the end of the mbuf chain.
	 */
	txmap = gx->gx_cdata.gx_tx_map[idx];
	if (bus_dmamap_load_mbuf(gx->gx_dmatag, txmap, m_head, BUS_DMA_NOWAIT))
		return(ENOBUFS);

	for (i = 0; i < txmap->dm_nsegs; i++) {
		if (cnt == GX_TX_RING_CNT) {
			printf("%s: overflow(2): %d, %d\n", cnt,
			       GX_TX_RING_CNT, gx->gx_dev.dv_xname);
			return (ENOBUFS);
		}

		tx = (struct gx_tx_desc_data *)&gx->gx_rdata->gx_tx_ring[idx];
		tx->tx_addr = txmap->dm_segs[i].ds_addr;
		tx->tx_status = 0;
		tx->tx_len = txmap->dm_segs[i].ds_len;
#ifdef notyet
		if (gx->arpcom.ac_if.if_hwassist) {
			tx->tx_type = 1;
			tx->tx_command = GX_TXTCP_EXTENSION;
			tx->tx_options = csumopts;
		} else {
#endif
			/*
			 * This is really a struct gx_tx_desc_old.
			 */
			tx->tx_command = 0;
#ifdef notyet
		}
#endif
		GX_INC(idx, GX_TX_RING_CNT);
		cnt++;
	}

	if (tx != NULL) {
		tx->tx_command |= GX_TXTCP_REPORT_STATUS | GX_TXTCP_INT_DELAY |
		    GX_TXTCP_ETHER_CRC | GX_TXTCP_END_OF_PKT;
#if NVLAN > 0
		if (ifv != NULL) {
			tx->tx_command |= GX_TXTCP_VLAN_ENABLE;
			tx->tx_vlan = ifv->ifv_tag;
		}
#endif
		gx->gx_txcnt = cnt;
		gx->gx_tx_tail_idx = idx;
		gx->gx_txcontext = txcontext;
		idx = GX_PREV(idx, GX_TX_RING_CNT);
		gx->gx_cdata.gx_tx_chain[idx] = m_head;

		CSR_WRITE_4(gx, gx->gx_reg.r_tx_tail, gx->gx_tx_tail_idx);
	}
	
	return (0);
}
 
/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit descriptors.
 */
void
gx_start(struct ifnet *ifp)
{
	struct gx_softc	*gx;
	struct mbuf *m_head;
	int s;

	s = splimp();

	gx = ifp->if_softc;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (gx_encap(gx, m_head) != 0) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif

		/*
		 * Set a timeout in case the chip goes out to lunch.
		 */
		ifp->if_timer = 5;
	}

	splx(s);
}

struct cfattach gx_ca = {
	sizeof(struct gx_softc), gx_probe, gx_attach
};

struct cfdriver gx_cd = {
	0, "gx", DV_IFNET
};
