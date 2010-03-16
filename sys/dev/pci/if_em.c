/**************************************************************************

Copyright (c) 2001-2003, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

/* $OpenBSD: if_em.c,v 1.235 2010/03/16 22:48:43 kettenis Exp $ */
/* $FreeBSD: if_em.c,v 1.46 2004/09/29 18:28:28 mlaier Exp $ */

#include <dev/pci/if_em.h>
#include <dev/pci/if_em_soc.h>

#ifdef EM_DEBUG
/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int             em_display_debug_stats = 0;
#endif

/*********************************************************************
 *  Driver version
 *********************************************************************/

#define EM_DRIVER_VERSION	"6.2.9"

/*********************************************************************
 *  PCI Device ID Table
 *********************************************************************/
const struct pci_matchid em_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_80003ES2LAN_CPR_DPT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_80003ES2LAN_SDS_DPT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_80003ES2LAN_CPR_SPT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_80003ES2LAN_SDS_SPT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EM_LOM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EP },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EP_LOM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EP_LP },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541EI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541EI_MOBILE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541ER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541ER_LOM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541GI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541GI_LF },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541GI_MOBILE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82542 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544EI_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544EI_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC_LOM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545EM_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545EM_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545GM_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545GM_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545GM_SERDES },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546EB_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546EB_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546EB_QUAD_CPR },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_PCIE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_QUAD_CPR },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_QUAD_CPR_K },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_SERDES },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82547EI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82547EI_MOBILE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82547GI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_AF },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_AT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_QUAD_CPR },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_QUAD_CPR_LP },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_QUAD_FBR },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_SERDES },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_SDS_DUAL },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571EB_SDS_QUAD },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82571PT_QUAD_CPR },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82572EI_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82572EI_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82572EI_SERDES },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82572EI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82573E },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82573E_IAMT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82573E_PM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82573L },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82573L_PL_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82573L_PL_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82573V_PM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82574L },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82575EB_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82575EB_SERDES },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82575GB_QUAD_CPR },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82576 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82576_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82576_SERDES },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82576_QUAD_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82576_NS },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH8_IFE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH8_IFE_G },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH8_IFE_GT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH8_IGP_AMT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH8_IGP_C },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH8_IGP_M },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH8_IGP_M_AMT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_BM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_IFE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_IFE_G },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_IFE_GT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_IGP_AMT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_IGP_C },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_IGP_M },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_IGP_M_AMT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH9_IGP_M_V },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH10_D_BM_LF },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH10_D_BM_LM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH10_R_BM_LF },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH10_R_BM_LM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_ICH10_R_BM_V },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_EP80579_LAN_1 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_EP80579_LAN_2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_EP80579_LAN_3 }
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
int  em_probe(struct device *, void *, void *);
void em_attach(struct device *, struct device *, void *);
void em_defer_attach(struct device*);
int  em_detach(struct device *, int);
int  em_activate(struct device *, int);
int  em_intr(void *);
void em_power(int, void *);
void em_start(struct ifnet *);
int  em_ioctl(struct ifnet *, u_long, caddr_t);
void em_watchdog(struct ifnet *);
void em_init(void *);
void em_stop(void *, int);
void em_media_status(struct ifnet *, struct ifmediareq *);
int  em_media_change(struct ifnet *);
int  em_flowstatus(struct em_softc *);
void em_identify_hardware(struct em_softc *);
int  em_allocate_pci_resources(struct em_softc *);
void em_free_pci_resources(struct em_softc *);
void em_local_timer(void *);
int  em_hardware_init(struct em_softc *);
void em_setup_interface(struct em_softc *);
int  em_setup_transmit_structures(struct em_softc *);
void em_initialize_transmit_unit(struct em_softc *);
int  em_setup_receive_structures(struct em_softc *);
void em_initialize_receive_unit(struct em_softc *);
void em_enable_intr(struct em_softc *);
void em_disable_intr(struct em_softc *);
void em_free_transmit_structures(struct em_softc *);
void em_free_receive_structures(struct em_softc *);
void em_update_stats_counters(struct em_softc *);
void em_txeof(struct em_softc *);
int  em_allocate_receive_structures(struct em_softc *);
int  em_allocate_transmit_structures(struct em_softc *);
#ifdef __STRICT_ALIGNMENT
void em_realign(struct em_softc *, struct mbuf *, u_int16_t *);
#else
#define em_realign(a, b, c) /* a, b, c */
#endif
int  em_rxfill(struct em_softc *);
void em_rxeof(struct em_softc *, int);
void em_receive_checksum(struct em_softc *, struct em_rx_desc *,
			 struct mbuf *);
#ifdef EM_CSUM_OFFLOAD
void em_transmit_checksum_setup(struct em_softc *, struct mbuf *,
				u_int32_t *, u_int32_t *);
#endif
void em_iff(struct em_softc *);
#ifdef EM_DEBUG
void em_print_hw_stats(struct em_softc *);
#endif
void em_update_link_status(struct em_softc *);
int  em_get_buf(struct em_softc *, int);
void em_enable_hw_vlans(struct em_softc *);
int  em_encap(struct em_softc *, struct mbuf *);
void em_smartspeed(struct em_softc *);
int  em_82547_fifo_workaround(struct em_softc *, int);
void em_82547_update_fifo_head(struct em_softc *, int);
int  em_82547_tx_fifo_reset(struct em_softc *);
void em_82547_move_tail(void *arg);
void em_82547_move_tail_locked(struct em_softc *);
int  em_dma_malloc(struct em_softc *, bus_size_t, struct em_dma_alloc *,
		   int);
void em_dma_free(struct em_softc *, struct em_dma_alloc *);
int  em_is_valid_ether_addr(u_int8_t *);
u_int32_t em_fill_descriptors(u_int64_t address, u_int32_t length,
			      PDESC_ARRAY desc_array);

/*********************************************************************
 *  OpenBSD Device Interface Entry Points
 *********************************************************************/

struct cfattach em_ca = {
	sizeof(struct em_softc), em_probe, em_attach, em_detach,
	em_activate
};

struct cfdriver em_cd = {
	NULL, "em", DV_IFNET
};

static int em_smart_pwr_down = FALSE;

/*********************************************************************
 *  Device identification routine
 *
 *  em_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 0 on no match, positive on match
 *********************************************************************/

int
em_probe(struct device *parent, void *match, void *aux)
{
	INIT_DEBUGOUT("em_probe: begin");

	return (pci_matchbyid((struct pci_attach_args *)aux, em_devices,
	    sizeof(em_devices)/sizeof(em_devices[0])));
}

void
em_defer_attach(struct device *self)
{
	struct em_softc *sc = (struct em_softc *)self;
	struct pci_attach_args *pa = &sc->osdep.em_pa;
	pci_chipset_tag_t	pc = pa->pa_pc;
	void *gcu;

	if ((gcu = em_lookup_gcu(self)) == 0) {
		printf("%s: No GCU found, defered attachment failed\n",
		    sc->sc_dv.dv_xname);

		if (sc->sc_intrhand)
			pci_intr_disestablish(pc, sc->sc_intrhand);
		sc->sc_intrhand = 0;

		if (sc->sc_powerhook != NULL)
			powerhook_disestablish(sc->sc_powerhook);

		em_stop(sc, 1);

		em_free_pci_resources(sc);
		em_dma_free(sc, &sc->rxdma);
		em_dma_free(sc, &sc->txdma);

		return;
	}
	
	sc->hw.gcu = gcu;
	
	em_attach_miibus(self);			

	em_setup_interface(sc);			

	em_update_link_status(sc);		

	em_setup_link(&sc->hw);			
}

/*********************************************************************
 *  Device initialization routine
 *
 *  The attach entry point is called when the driver is being loaded.
 *  This routine identifies the type of hardware, allocates all resources
 *  and initializes the hardware.
 *
 *********************************************************************/

void 
em_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
	struct em_softc *sc;
	int tsize, rsize;
	int defer = 0;
    
	INIT_DEBUGOUT("em_attach: begin");

	sc = (struct em_softc *)self;
	sc->osdep.em_pa = *pa;

	timeout_set(&sc->timer_handle, em_local_timer, sc);
	timeout_set(&sc->tx_fifo_timer_handle, em_82547_move_tail, sc);

	/* Determine hardware revision */
	em_identify_hardware(sc);

	/* Parameters (to be read from user) */
	if (sc->hw.mac_type >= em_82544) {
		sc->num_tx_desc = EM_MAX_TXD;
		sc->num_rx_desc = EM_MAX_RXD;
	} else {
		sc->num_tx_desc = EM_MAX_TXD_82543;
		sc->num_rx_desc = EM_MAX_RXD_82543;
	}
	sc->tx_int_delay = EM_TIDV;
	sc->tx_abs_int_delay = EM_TADV;
	sc->rx_int_delay = EM_RDTR;
	sc->rx_abs_int_delay = EM_RADV;
	sc->hw.autoneg = DO_AUTO_NEG;
	sc->hw.wait_autoneg_complete = WAIT_FOR_AUTO_NEG_DEFAULT;
	sc->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
	sc->hw.tbi_compatibility_en = TRUE;
	sc->rx_buffer_len = EM_RXBUFFER_2048;

	sc->hw.phy_init_script = 1;
	sc->hw.phy_reset_disable = FALSE;

#ifndef EM_MASTER_SLAVE
	sc->hw.master_slave = em_ms_hw_default;
#else
	sc->hw.master_slave = EM_MASTER_SLAVE;
#endif

	/*
	 * This controls when hardware reports transmit completion
	 * status.   
	 */
	sc->hw.report_tx_early = 1;

	if (em_allocate_pci_resources(sc))
		goto err_pci;

	/* Initialize eeprom parameters */
	em_init_eeprom_params(&sc->hw);

	/*
	 * Set the max frame size assuming standard Ethernet
	 * sized frames.
	 */
	switch (sc->hw.mac_type) {
		case em_82573:
		{
			uint16_t	eeprom_data = 0;

			/*
			 * 82573 only supports Jumbo frames
			 * if ASPM is disabled.
			 */
			em_read_eeprom(&sc->hw, EEPROM_INIT_3GIO_3,
			    1, &eeprom_data);
			if (eeprom_data & EEPROM_WORD1A_ASPM_MASK) {
				sc->hw.max_frame_size = ETHER_MAX_LEN;
				break;
			}
			/* Allow Jumbo frames */
			/* FALLTHROUGH */
		}
		case em_82571:
		case em_82572:
		case em_82574:
		case em_82575:
		case em_ich9lan:
		case em_ich10lan:
		case em_80003es2lan:
			/* Limit Jumbo Frame size */
			sc->hw.max_frame_size = 9234;
			break;
		case em_82542_rev2_0:
		case em_82542_rev2_1:
		case em_ich8lan:
			/* Adapters that do not support Jumbo frames */
			sc->hw.max_frame_size = ETHER_MAX_LEN;
			break;
		default:
			sc->hw.max_frame_size =
			    MAX_JUMBO_FRAME_SIZE;
	}

	sc->hw.min_frame_size = 
	    ETHER_MIN_LEN + ETHER_CRC_LEN;

	if (sc->hw.mac_type >= em_82544)
	    tsize = EM_ROUNDUP(sc->num_tx_desc * sizeof(struct em_tx_desc),
		EM_MAX_TXD * sizeof(struct em_tx_desc));
	else
	    tsize = EM_ROUNDUP(sc->num_tx_desc * sizeof(struct em_tx_desc),
		EM_MAX_TXD_82543 * sizeof(struct em_tx_desc));
	tsize = EM_ROUNDUP(tsize, PAGE_SIZE);

	/* Allocate Transmit Descriptor ring */
	if (em_dma_malloc(sc, tsize, &sc->txdma, BUS_DMA_NOWAIT)) {
		printf("%s: Unable to allocate tx_desc memory\n", 
		       sc->sc_dv.dv_xname);
		goto err_tx_desc;
	}
	sc->tx_desc_base = (struct em_tx_desc *)sc->txdma.dma_vaddr;

	if (sc->hw.mac_type >= em_82544)
	    rsize = EM_ROUNDUP(sc->num_rx_desc * sizeof(struct em_rx_desc),
		EM_MAX_RXD * sizeof(struct em_rx_desc));
	else
	    rsize = EM_ROUNDUP(sc->num_rx_desc * sizeof(struct em_rx_desc),
		EM_MAX_RXD_82543 * sizeof(struct em_rx_desc));
	rsize = EM_ROUNDUP(rsize, PAGE_SIZE);

	/* Allocate Receive Descriptor ring */
	if (em_dma_malloc(sc, rsize, &sc->rxdma, BUS_DMA_NOWAIT)) {
		printf("%s: Unable to allocate rx_desc memory\n",
		       sc->sc_dv.dv_xname);
		goto err_rx_desc;
	}
	sc->rx_desc_base = (struct em_rx_desc *) sc->rxdma.dma_vaddr;

	/* Initialize the hardware */
	if ((defer = em_hardware_init(sc))) {
		if (defer == EAGAIN)
			config_defer(self, em_defer_attach);
		else {
			printf("%s: Unable to initialize the hardware\n",
			    sc->sc_dv.dv_xname);
			goto err_hw_init;
		}
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (em_read_mac_addr(&sc->hw) < 0) {
		printf("%s: EEPROM read error while reading mac address\n",
		       sc->sc_dv.dv_xname);
		goto err_mac_addr;
	}

	if (!em_is_valid_ether_addr(sc->hw.mac_addr)) {
		printf("%s: Invalid mac address\n", sc->sc_dv.dv_xname);
		goto err_mac_addr;
	}

	bcopy(sc->hw.mac_addr, sc->interface_data.ac_enaddr,
	    ETHER_ADDR_LEN);

	/* Setup OS specific network interface */
	if (!defer)
		em_setup_interface(sc);

	/* Initialize statistics */
	em_clear_hw_cntrs(&sc->hw);
	em_update_stats_counters(sc);
	sc->hw.get_link_status = 1;
	if (!defer)
		em_update_link_status(sc);

	printf(", address %s\n", ether_sprintf(sc->interface_data.ac_enaddr));

	/* Indicate SOL/IDER usage */
	if (em_check_phy_reset_block(&sc->hw))
		printf("%s: PHY reset is blocked due to SOL/IDER session.\n",
		    sc->sc_dv.dv_xname);

	/* Identify 82544 on PCI-X */
	em_get_bus_info(&sc->hw);
	if (sc->hw.bus_type == em_bus_type_pcix &&
	    sc->hw.mac_type == em_82544)
		sc->pcix_82544 = TRUE;
        else
		sc->pcix_82544 = FALSE;

	sc->hw.icp_xxxx_is_link_up = FALSE;

	INIT_DEBUGOUT("em_attach: end");
	sc->sc_powerhook = powerhook_establish(em_power, sc);
	return;

err_mac_addr:
err_hw_init:
	em_dma_free(sc, &sc->rxdma);
err_rx_desc:
	em_dma_free(sc, &sc->txdma);
err_tx_desc:
err_pci:
	em_free_pci_resources(sc);
}

void
em_power(int why, void *arg)
{
	struct em_softc *sc = (struct em_softc *)arg;
	struct ifnet *ifp;

	if (why == PWR_RESUME) {
		ifp = &sc->interface_data.ac_if;
		if (ifp->if_flags & IFF_UP)
			em_init(sc);
	}
}

/*********************************************************************
 *  Transmit entry point
 *
 *  em_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

void
em_start(struct ifnet *ifp)
{
	struct mbuf    *m_head;
	struct em_softc *sc = ifp->if_softc;
	int		post = 0;

	if ((ifp->if_flags & (IFF_OACTIVE | IFF_RUNNING)) != IFF_RUNNING)
		return;

	if (!sc->link_active)
		return;

	if (sc->hw.mac_type != em_82547) {
		bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map, 0,
		    sc->txdma.dma_map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	}

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (em_encap(sc, m_head)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m_head);

#if NBPFILTER > 0
		/* Send a copy of the frame to the BPF listener */
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif

		/* Set timeout in case hardware has problems transmitting */
		ifp->if_timer = EM_TX_TIMEOUT;

		post = 1;
	}

	if (sc->hw.mac_type != em_82547) {
		bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map, 0,
		    sc->txdma.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		/* 
		 * Advance the Transmit Descriptor Tail (Tdt),
		 * this tells the E1000 that this frame is
		 * available to transmit.
		 */
		if (post)
			E1000_WRITE_REG(&sc->hw, TDT, sc->next_avail_tx_desc);
	}
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  em_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

int
em_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	int		error = 0;
	struct ifreq   *ifr = (struct ifreq *) data;
	struct ifaddr  *ifa = (struct ifaddr *)data;
	struct em_softc *sc = ifp->if_softc;
	int s;

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFADDR (Set Interface "
			       "Addr)");
		if (!(ifp->if_flags & IFF_UP)) {
			ifp->if_flags |= IFF_UP;
			em_init(sc);
		}
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->interface_data, ifa);
#endif /* INET */
		break;

	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFFLAGS (Set Interface Flags)");
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				em_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				em_stop(sc, 0);
		}
		break;

	case SIOCSIFMEDIA:
		/* Check SOL/IDER usage */
		if (em_check_phy_reset_block(&sc->hw)) {
			printf("%s: Media change is blocked due to SOL/IDER session.\n",
			    sc->sc_dv.dv_xname);
			break;
		}
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;

	default:
		error = ether_ioctl(ifp, &sc->interface_data, command, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING) {
			em_disable_intr(sc);
			em_iff(sc);
			if (sc->hw.mac_type == em_82542_rev2_0)
				em_initialize_receive_unit(sc);
			em_enable_intr(sc);
		}
		error = 0;
	}

	splx(s);
	return (error);
}

/*********************************************************************
 *  Watchdog entry point
 *
 *  This routine is called whenever hardware quits transmitting.
 *
 **********************************************************************/

void
em_watchdog(struct ifnet *ifp)
{
	struct em_softc *sc = ifp->if_softc;

	/* If we are in this routine because of pause frames, then
	 * don't reset the hardware.
	 */
	if (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_TXOFF) {
		ifp->if_timer = EM_TX_TIMEOUT;
		return;
	}
	printf("%s: watchdog timeout -- resetting\n", sc->sc_dv.dv_xname);

	em_init(sc);

	sc->watchdog_events++;
}

/*********************************************************************
 *  Init entry point
 *
 *  This routine is used in two ways. It is used by the stack as
 *  init entry point in network interface structure. It is also used
 *  by the driver as a hw/sw initialization routine to get to a
 *  consistent state.
 *
 **********************************************************************/

void
em_init(void *arg)
{
	struct em_softc *sc = arg;
	struct ifnet   *ifp = &sc->interface_data.ac_if;
	uint32_t	pba;
	int s;

	s = splnet();

	INIT_DEBUGOUT("em_init: begin");

	em_stop(sc, 0);

	/*
	 * Packet Buffer Allocation (PBA)
	 * Writing PBA sets the receive portion of the buffer
	 * the remainder is used for the transmit buffer.
	 *
	 * Devices before the 82547 had a Packet Buffer of 64K.
	 *   Default allocation: PBA=48K for Rx, leaving 16K for Tx.
	 * After the 82547 the buffer was reduced to 40K.
	 *   Default allocation: PBA=30K for Rx, leaving 10K for Tx.
	 *   Note: default does not leave enough room for Jumbo Frame >10k.
	 */
	switch (sc->hw.mac_type) {
	case em_82547:
	case em_82547_rev_2: /* 82547: Total Packet Buffer is 40K */
		if (sc->hw.max_frame_size > EM_RXBUFFER_8192)
			pba = E1000_PBA_22K; /* 22K for Rx, 18K for Tx */
		else
			pba = E1000_PBA_30K; /* 30K for Rx, 10K for Tx */
		sc->tx_fifo_head = 0;
		sc->tx_head_addr = pba << EM_TX_HEAD_ADDR_SHIFT;
		sc->tx_fifo_size = (E1000_PBA_40K - pba) << EM_PBA_BYTES_SHIFT;
		break;
	case em_82571:
	case em_82572: /* Total Packet Buffer on these is 48k */
	case em_82575:
	case em_80003es2lan:
		pba = E1000_PBA_32K; /* 32K for Rx, 16K for Tx */
		break;
	case em_82573: /* 82573: Total Packet Buffer is 32K */
		/* Jumbo frames not supported */
		pba = E1000_PBA_12K; /* 12K for Rx, 20K for Tx */
		break;
	case em_82574: /* Total Packet Buffer is 40k */
		pba = E1000_PBA_30K; /* 30K for Rx, 10K for Tx */
		break;
	case em_ich8lan:
		pba = E1000_PBA_8K;
		break;
	case em_ich9lan:
	case em_ich10lan:
		pba = E1000_PBA_10K;
		break;
	default:
		/* Devices before 82547 had a Packet Buffer of 64K.   */
		if (sc->hw.max_frame_size > EM_RXBUFFER_8192)
			pba = E1000_PBA_40K; /* 40K for Rx, 24K for Tx */
		else
			pba = E1000_PBA_48K; /* 48K for Rx, 16K for Tx */
	}
	INIT_DEBUGOUT1("em_init: pba=%dK",pba);
	E1000_WRITE_REG(&sc->hw, PBA, pba);

	/* Get the latest mac address, User can use a LAA */
	bcopy(sc->interface_data.ac_enaddr, sc->hw.mac_addr,
	      ETHER_ADDR_LEN);

	/* Initialize the hardware */
	if (em_hardware_init(sc)) {
		printf("%s: Unable to initialize the hardware\n", 
		       sc->sc_dv.dv_xname);
		splx(s);
		return;
	}
	em_update_link_status(sc);

	E1000_WRITE_REG(&sc->hw, VET, ETHERTYPE_VLAN);
	if (ifp->if_capabilities & IFCAP_VLAN_HWTAGGING)
		em_enable_hw_vlans(sc);

	/* Prepare transmit descriptors and buffers */
	if (em_setup_transmit_structures(sc)) {
		printf("%s: Could not setup transmit structures\n", 
		       sc->sc_dv.dv_xname);
		em_stop(sc, 0);
		splx(s);
		return;
	}
	em_initialize_transmit_unit(sc);

	/* Prepare receive descriptors and buffers */
	if (em_setup_receive_structures(sc)) {
		printf("%s: Could not setup receive structures\n", 
		       sc->sc_dv.dv_xname);
		em_stop(sc, 0);
		splx(s);
		return;
	}
	em_initialize_receive_unit(sc);

	/* Program promiscuous mode and multicast filters. */
	em_iff(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	timeout_add_sec(&sc->timer_handle, 1);
	em_clear_hw_cntrs(&sc->hw);
	em_enable_intr(sc);

	/* Don't reset the phy next time init gets called */
	sc->hw.phy_reset_disable = TRUE;

	splx(s);
}

/*********************************************************************
 *
 *  Interrupt Service routine
 *
 **********************************************************************/
int 
em_intr(void *arg)
{
	struct em_softc  *sc = arg;
	struct ifnet	*ifp;
	u_int32_t	reg_icr, test_icr;
	int claimed = 0;
	int refill;

	ifp = &sc->interface_data.ac_if;

	for (;;) {
		test_icr = reg_icr = E1000_READ_REG(&sc->hw, ICR);
		if (sc->hw.mac_type >= em_82571)
			test_icr = (reg_icr & E1000_ICR_INT_ASSERTED);
		if (!test_icr)
			break;

		claimed = 1;
		refill = 0;

		if (ifp->if_flags & IFF_RUNNING) {
			em_rxeof(sc, -1);
			em_txeof(sc);
			refill = 1;
		}

		/* Link status change */
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			timeout_del(&sc->timer_handle);
			sc->hw.get_link_status = 1;
			em_check_for_link(&sc->hw);
			em_update_link_status(sc);
			timeout_add_sec(&sc->timer_handle, 1); 
		}

		if (reg_icr & E1000_ICR_RXO) {
			sc->rx_overruns++;
			refill = 1;
		}

		if (refill && em_rxfill(sc)) {
			/* Advance the Rx Queue #0 "Tail Pointer". */
			E1000_WRITE_REG(&sc->hw, RDT, sc->last_rx_desc_filled);
		}
	}

	if (ifp->if_flags & IFF_RUNNING && !IFQ_IS_EMPTY(&ifp->if_snd))
		em_start(ifp);

	return (claimed);
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called whenever the user queries the status of
 *  the interface using ifconfig.
 *
 **********************************************************************/
void
em_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct em_softc *sc = ifp->if_softc;
	u_char fiber_type = IFM_1000_SX;
	u_int16_t gsr;

	INIT_DEBUGOUT("em_media_status: begin");

	em_check_for_link(&sc->hw);
	em_update_link_status(sc);

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->link_active) {
		ifmr->ifm_active |= IFM_NONE;
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	if (sc->hw.media_type == em_media_type_fiber ||
	    sc->hw.media_type == em_media_type_internal_serdes) {
		if (sc->hw.mac_type == em_82545)
			fiber_type = IFM_1000_LX;
		ifmr->ifm_active |= fiber_type | IFM_FDX;
	} else {
		switch (sc->link_speed) {
		case 10:
			ifmr->ifm_active |= IFM_10_T;
			break;
		case 100:
			ifmr->ifm_active |= IFM_100_TX;
			break;
		case 1000:
			ifmr->ifm_active |= IFM_1000_T;
			break;
		}

		if (sc->link_duplex == FULL_DUPLEX)
			ifmr->ifm_active |= em_flowstatus(sc) | IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;

		if (IFM_SUBTYPE(ifmr->ifm_active) == IFM_1000_T) {
			em_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &gsr);
			if (gsr & SR_1000T_MS_CONFIG_RES)
				ifmr->ifm_active |= IFM_ETH_MASTER;
		}
	}
}

/*********************************************************************
 *
 *  Media Ioctl callback
 *
 *  This routine is called when the user changes speed/duplex using
 *  media/mediopt option with ifconfig.
 *
 **********************************************************************/
int
em_media_change(struct ifnet *ifp)
{
	struct em_softc *sc = ifp->if_softc;
	struct ifmedia	*ifm = &sc->media;

	INIT_DEBUGOUT("em_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return (EINVAL);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		sc->hw.autoneg = DO_AUTO_NEG;
		sc->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
		break;
	case IFM_1000_LX:
	case IFM_1000_SX:
	case IFM_1000_T:
		sc->hw.autoneg = DO_AUTO_NEG;
		sc->hw.autoneg_advertised = ADVERTISE_1000_FULL;
		break;
	case IFM_100_TX:
		sc->hw.autoneg = FALSE;
		sc->hw.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.forced_speed_duplex = em_100_full;
		else
			sc->hw.forced_speed_duplex = em_100_half;
		break;
	case IFM_10_T:
		sc->hw.autoneg = FALSE;
		sc->hw.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.forced_speed_duplex = em_10_full;
		else
			sc->hw.forced_speed_duplex = em_10_half;
		break;
	default:
		printf("%s: Unsupported media type\n", sc->sc_dv.dv_xname);
	}

	/*
	 * As the speed/duplex settings may have changed we need to
	 * reset the PHY.
	 */
	sc->hw.phy_reset_disable = FALSE;

	em_init(sc);

	return (0);
}

int
em_flowstatus(struct em_softc *sc)
{
	u_int16_t ar, lpar;

	if (sc->hw.media_type == em_media_type_fiber ||
	    sc->hw.media_type == em_media_type_internal_serdes)
		return (0);

	em_read_phy_reg(&sc->hw, PHY_AUTONEG_ADV, &ar);
	em_read_phy_reg(&sc->hw, PHY_LP_ABILITY, &lpar);

	if ((ar & NWAY_AR_PAUSE) && (lpar & NWAY_LPAR_PAUSE))
		return (IFM_FLOW|IFM_ETH_TXPAUSE|IFM_ETH_RXPAUSE);
	else if (!(ar & NWAY_AR_PAUSE) && (ar & NWAY_AR_ASM_DIR) &&
		(lpar & NWAY_LPAR_PAUSE) && (lpar & NWAY_LPAR_ASM_DIR))
		return (IFM_FLOW|IFM_ETH_TXPAUSE);
	else if ((ar & NWAY_AR_PAUSE) && (ar & NWAY_AR_ASM_DIR) &&
		!(lpar & NWAY_LPAR_PAUSE) && (lpar & NWAY_LPAR_ASM_DIR))
		return (IFM_FLOW|IFM_ETH_RXPAUSE);

	return (0);
}

/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/
int
em_encap(struct em_softc *sc, struct mbuf *m_head)
{
	u_int32_t	txd_upper;
	u_int32_t	txd_lower, txd_used = 0, txd_saved = 0;
	int		i, j, first, error = 0, last = 0;
	bus_dmamap_t	map;

	/* For 82544 Workaround */
	DESC_ARRAY		desc_array;
	u_int32_t		array_elements;
	u_int32_t		counter;

	struct em_buffer   *tx_buffer, *tx_buffer_mapped;
	struct em_tx_desc *current_tx_desc = NULL;

	/*
	 * Force a cleanup if number of TX descriptors
	 * available hits the threshold
	 */
	if (sc->num_tx_desc_avail <= EM_TX_CLEANUP_THRESHOLD) {
		em_txeof(sc);
		/* Now do we at least have a minimal? */
		if (sc->num_tx_desc_avail <= EM_TX_OP_THRESHOLD) {
			sc->no_tx_desc_avail1++;
			return (ENOBUFS);
		}
	}

	if (sc->hw.mac_type == em_82547) {
		bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map, 0,
		    sc->txdma.dma_map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
	}

	/*
	 * Map the packet for DMA.
	 *
	 * Capture the first descriptor index,
	 * this descriptor will have the index
	 * of the EOP which is the only one that
	 * no gets a DONE bit writeback.
	 */
	first = sc->next_avail_tx_desc;
	tx_buffer = &sc->tx_buffer_area[first];
	tx_buffer_mapped = tx_buffer;
	map = tx_buffer->map;

	error = bus_dmamap_load_mbuf(sc->txtag, map, m_head, BUS_DMA_NOWAIT);
	if (error != 0) {
		sc->no_tx_dma_setup++;
		goto loaderr;
	}
	EM_KASSERT(map->dm_nsegs!= 0, ("em_encap: empty packet"));

	if (map->dm_nsegs > sc->num_tx_desc_avail - 2)
		goto fail;

#ifdef EM_CSUM_OFFLOAD
	if (sc->hw.mac_type >= em_82543)
		em_transmit_checksum_setup(sc, m_head, &txd_upper, &txd_lower);
	else
		txd_upper = txd_lower = 0;
#else
	txd_upper = txd_lower = 0;
#endif

	i = sc->next_avail_tx_desc;
	if (sc->pcix_82544)
		txd_saved = i;

	for (j = 0; j < map->dm_nsegs; j++) {
		/* If sc is 82544 and on PCI-X bus */
		if (sc->pcix_82544) {
			/*
			 * Check the Address and Length combination and
			 * split the data accordingly
			 */
			array_elements = em_fill_descriptors(map->dm_segs[j].ds_addr,
							     map->dm_segs[j].ds_len,
							     &desc_array);
			for (counter = 0; counter < array_elements; counter++) {
				if (txd_used == sc->num_tx_desc_avail) {
					sc->next_avail_tx_desc = txd_saved;
					goto fail;
				}
				tx_buffer = &sc->tx_buffer_area[i];
				current_tx_desc = &sc->tx_desc_base[i];
				current_tx_desc->buffer_addr = htole64(
					desc_array.descriptor[counter].address);
				current_tx_desc->lower.data = htole32(
					(sc->txd_cmd | txd_lower |
					 (u_int16_t)desc_array.descriptor[counter].length));
				current_tx_desc->upper.data = htole32((txd_upper));
				last = i;
				if (++i == sc->num_tx_desc)
					i = 0;

				tx_buffer->m_head = NULL;
				tx_buffer->next_eop = -1;
				txd_used++;
			}
		} else {
			tx_buffer = &sc->tx_buffer_area[i];
			current_tx_desc = &sc->tx_desc_base[i];

			current_tx_desc->buffer_addr = htole64(map->dm_segs[j].ds_addr);
			current_tx_desc->lower.data = htole32(
				sc->txd_cmd | txd_lower | map->dm_segs[j].ds_len);
			current_tx_desc->upper.data = htole32(txd_upper);
			last = i;
			if (++i == sc->num_tx_desc)
	        		i = 0;

			tx_buffer->m_head = NULL;
			tx_buffer->next_eop = -1;
		}
	}

	sc->next_avail_tx_desc = i;
	if (sc->pcix_82544)
		sc->num_tx_desc_avail -= txd_used;
	else
		sc->num_tx_desc_avail -= map->dm_nsegs;

#if NVLAN > 0
	/* Find out if we are in VLAN mode */
	if (m_head->m_flags & M_VLANTAG) {
		/* Set the VLAN id */
		current_tx_desc->upper.fields.special =
			htole16(m_head->m_pkthdr.ether_vtag);

		/* Tell hardware to add tag */
		current_tx_desc->lower.data |= htole32(E1000_TXD_CMD_VLE);
	}
#endif

	tx_buffer->m_head = m_head;
	tx_buffer_mapped->map = tx_buffer->map;
	tx_buffer->map = map;
	bus_dmamap_sync(sc->txtag, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* 
	 * Last Descriptor of Packet
	 * needs End Of Packet (EOP)
	 * and Report Status (RS)
	 */
	current_tx_desc->lower.data |=
	    htole32(E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS);

	/*
	 * Keep track in the first buffer which
	 * descriptor will be written back
	 */
	tx_buffer = &sc->tx_buffer_area[first];
	tx_buffer->next_eop = last;

	/* 
	 * Advance the Transmit Descriptor Tail (Tdt),
	 * this tells the E1000 that this frame is
	 * available to transmit.
	 */
	if (sc->hw.mac_type == em_82547) {
		bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map, 0,
		    sc->txdma.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
		if (sc->link_duplex == HALF_DUPLEX)
			em_82547_move_tail_locked(sc);
		else {
			E1000_WRITE_REG(&sc->hw, TDT, i);
			em_82547_update_fifo_head(sc, m_head->m_pkthdr.len);
		}
	}

	return (0);

fail:
	sc->no_tx_desc_avail2++;
	bus_dmamap_unload(sc->txtag, map);
	error = ENOBUFS;
loaderr:
	if (sc->hw.mac_type == em_82547) {
		bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map, 0,
		    sc->txdma.dma_map->dm_mapsize,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);
	}
	return (error);
}

/*********************************************************************
 *
 * 82547 workaround to avoid controller hang in half-duplex environment.
 * The workaround is to avoid queuing a large packet that would span
 * the internal Tx FIFO ring boundary. We need to reset the FIFO pointers
 * in this case. We do that only when FIFO is quiescent.
 *
 **********************************************************************/
void
em_82547_move_tail_locked(struct em_softc *sc)
{
	uint16_t hw_tdt;
	uint16_t sw_tdt;
	struct em_tx_desc *tx_desc;
	uint16_t length = 0;
	boolean_t eop = 0;

	hw_tdt = E1000_READ_REG(&sc->hw, TDT);
	sw_tdt = sc->next_avail_tx_desc;

	while (hw_tdt != sw_tdt) {
		tx_desc = &sc->tx_desc_base[hw_tdt];
		length += tx_desc->lower.flags.length;
		eop = tx_desc->lower.data & E1000_TXD_CMD_EOP;
		if (++hw_tdt == sc->num_tx_desc)
			hw_tdt = 0;

		if (eop) {
			if (em_82547_fifo_workaround(sc, length)) {
				sc->tx_fifo_wrk_cnt++;
				timeout_add(&sc->tx_fifo_timer_handle, 1);
				break;
			}
			E1000_WRITE_REG(&sc->hw, TDT, hw_tdt);
			em_82547_update_fifo_head(sc, length);
			length = 0;
		}
	}
}

void
em_82547_move_tail(void *arg)
{
	struct em_softc *sc = arg;
	int s;

	s = splnet();
	em_82547_move_tail_locked(sc);
	splx(s);
}

int
em_82547_fifo_workaround(struct em_softc *sc, int len)
{
	int fifo_space, fifo_pkt_len;

	fifo_pkt_len = EM_ROUNDUP(len + EM_FIFO_HDR, EM_FIFO_HDR);

	if (sc->link_duplex == HALF_DUPLEX) {
		fifo_space = sc->tx_fifo_size - sc->tx_fifo_head;

		if (fifo_pkt_len >= (EM_82547_PKT_THRESH + fifo_space)) {
			if (em_82547_tx_fifo_reset(sc))
				return (0);
			else
				return (1);
		}
	}

	return (0);
}

void
em_82547_update_fifo_head(struct em_softc *sc, int len)
{
	int fifo_pkt_len = EM_ROUNDUP(len + EM_FIFO_HDR, EM_FIFO_HDR);

	/* tx_fifo_head is always 16 byte aligned */
	sc->tx_fifo_head += fifo_pkt_len;
	if (sc->tx_fifo_head >= sc->tx_fifo_size)
		sc->tx_fifo_head -= sc->tx_fifo_size;
}

int
em_82547_tx_fifo_reset(struct em_softc *sc)
{
	uint32_t tctl;

	if ((E1000_READ_REG(&sc->hw, TDT) ==
	     E1000_READ_REG(&sc->hw, TDH)) &&
	    (E1000_READ_REG(&sc->hw, TDFT) ==
	     E1000_READ_REG(&sc->hw, TDFH)) &&
	    (E1000_READ_REG(&sc->hw, TDFTS) ==
	     E1000_READ_REG(&sc->hw, TDFHS)) &&
	    (E1000_READ_REG(&sc->hw, TDFPC) == 0)) {

		/* Disable TX unit */
		tctl = E1000_READ_REG(&sc->hw, TCTL);
		E1000_WRITE_REG(&sc->hw, TCTL, tctl & ~E1000_TCTL_EN);

		/* Reset FIFO pointers */
		E1000_WRITE_REG(&sc->hw, TDFT, sc->tx_head_addr);
		E1000_WRITE_REG(&sc->hw, TDFH, sc->tx_head_addr);
		E1000_WRITE_REG(&sc->hw, TDFTS, sc->tx_head_addr);
		E1000_WRITE_REG(&sc->hw, TDFHS, sc->tx_head_addr);

		/* Re-enable TX unit */
		E1000_WRITE_REG(&sc->hw, TCTL, tctl);
		E1000_WRITE_FLUSH(&sc->hw);

		sc->tx_fifo_head = 0;
		sc->tx_fifo_reset_cnt++;

		return (TRUE);
	} else
		return (FALSE);
}

void
em_iff(struct em_softc *sc)
{
	struct ifnet *ifp = &sc->interface_data.ac_if;
	struct arpcom *ac = &sc->interface_data;
	u_int32_t reg_rctl = 0;
	u_int8_t  mta[MAX_NUM_MULTICAST_ADDRESSES * ETH_LENGTH_OF_ADDRESS];
	struct ether_multi *enm;
	struct ether_multistep step;
	int i = 0;

	IOCTL_DEBUGOUT("em_iff: begin");

	if (sc->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		if (sc->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			em_pci_clear_mwi(&sc->hw);
		reg_rctl |= E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		msec_delay(5);
	}

	reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
	reg_rctl &= ~(E1000_RCTL_MPE | E1000_RCTL_UPE);
	ifp->if_flags &= ~IFF_ALLMULTI;

	if (ifp->if_flags & IFF_PROMISC || ac->ac_multirangecnt > 0 ||
	    ac->ac_multicnt > MAX_NUM_MULTICAST_ADDRESSES) {
		ifp->if_flags |= IFF_ALLMULTI;
		reg_rctl |= E1000_RCTL_MPE;
		if (ifp->if_flags & IFF_PROMISC)
			reg_rctl |= E1000_RCTL_UPE;
	} else {
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			bcopy(enm->enm_addrlo, mta + i, ETH_LENGTH_OF_ADDRESS);
			i += ETH_LENGTH_OF_ADDRESS;

			ETHER_NEXT_MULTI(step, enm);
		}

		em_mc_addr_list_update(&sc->hw, mta, ac->ac_multicnt, 0, 1);
	}

	E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);

	if (sc->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		reg_rctl &= ~E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		msec_delay(5);
		if (sc->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE)
			em_pci_set_mwi(&sc->hw);
	}
}

/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status and updates statistics.
 *
 **********************************************************************/

void
em_local_timer(void *arg)
{
	struct ifnet   *ifp;
	struct em_softc *sc = arg;
	int s;

	ifp = &sc->interface_data.ac_if;

	s = splnet();

	em_check_for_link(&sc->hw);
	em_update_link_status(sc);
	em_update_stats_counters(sc);
#ifdef EM_DEBUG
	if (em_display_debug_stats && ifp->if_flags & IFF_RUNNING)
		em_print_hw_stats(sc);
#endif
	em_smartspeed(sc);

	timeout_add_sec(&sc->timer_handle, 1);

	splx(s);
}

void
em_update_link_status(struct em_softc *sc)
{
	struct ifnet *ifp = &sc->interface_data.ac_if;

	if (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_LU) {
		if (sc->link_active == 0) {
			em_get_speed_and_duplex(&sc->hw,
						&sc->link_speed,
						&sc->link_duplex);
			/* Check if we may set SPEED_MODE bit on PCI-E */
			if ((sc->link_speed == SPEED_1000) &&
			    ((sc->hw.mac_type == em_82571) ||
			    (sc->hw.mac_type == em_82572) ||
			    (sc->hw.mac_type == em_82575))) {
				int tarc0;

				tarc0 = E1000_READ_REG(&sc->hw, TARC0);
				tarc0 |= SPEED_MODE_BIT;
				E1000_WRITE_REG(&sc->hw, TARC0, tarc0);
			}
			sc->link_active = 1;
			sc->smartspeed = 0;
			ifp->if_baudrate = sc->link_speed * 1000000;
		}
		if (!LINK_STATE_IS_UP(ifp->if_link_state)) {
			if (sc->link_duplex == FULL_DUPLEX)
				ifp->if_link_state = LINK_STATE_FULL_DUPLEX;
			else
				ifp->if_link_state = LINK_STATE_HALF_DUPLEX;
			if_link_state_change(ifp);
		}
	} else {
		if (sc->link_active == 1) {
			ifp->if_baudrate = sc->link_speed = 0;
			sc->link_duplex = 0;
			sc->link_active = 0;
		}
		if (ifp->if_link_state != LINK_STATE_DOWN) {
			ifp->if_link_state = LINK_STATE_DOWN;
			if_link_state_change(ifp);
		}
	}
}

/*********************************************************************
 *
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers. 
 *
 **********************************************************************/

void
em_stop(void *arg, int softonly)
{
	struct em_softc *sc = arg;
	struct ifnet   *ifp = &sc->interface_data.ac_if;

	/* Tell the stack that the interface is no longer active */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	INIT_DEBUGOUT("em_stop: begin");

	timeout_del(&sc->timer_handle);
	timeout_del(&sc->tx_fifo_timer_handle);

	if (!softonly) {
		em_disable_intr(sc);
		em_reset_hw(&sc->hw);
	}

	em_free_transmit_structures(sc);
	em_free_receive_structures(sc);
}

/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
void
em_identify_hardware(struct em_softc *sc)
{
	u_int32_t reg;
	struct pci_attach_args *pa = &sc->osdep.em_pa;

	/* Make sure our PCI config space has the necessary stuff set */
	sc->hw.pci_cmd_word = pci_conf_read(pa->pa_pc, pa->pa_tag,
					    PCI_COMMAND_STATUS_REG);

	/* Save off the information about this board */
	sc->hw.vendor_id = PCI_VENDOR(pa->pa_id);
	sc->hw.device_id = PCI_PRODUCT(pa->pa_id);

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
	sc->hw.revision_id = PCI_REVISION(reg);

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	sc->hw.subsystem_vendor_id = PCI_VENDOR(reg);
	sc->hw.subsystem_id = PCI_PRODUCT(reg);

	/* Identify the MAC */
	if (em_set_mac_type(&sc->hw))
		printf("%s: Unknown MAC Type\n", sc->sc_dv.dv_xname);

	if (sc->hw.mac_type == em_82541 ||
	    sc->hw.mac_type == em_82541_rev_2 ||
	    sc->hw.mac_type == em_82547 ||
	    sc->hw.mac_type == em_82547_rev_2)
		sc->hw.phy_init_script = TRUE;
}

int
em_allocate_pci_resources(struct em_softc *sc)
{
	int		val, rid;
	pci_intr_handle_t	ih;
	const char		*intrstr = NULL;
	struct pci_attach_args *pa = &sc->osdep.em_pa;
	pci_chipset_tag_t	pc = pa->pa_pc;

	val = pci_conf_read(pa->pa_pc, pa->pa_tag, EM_MMBA);
	if (PCI_MAPREG_TYPE(val) != PCI_MAPREG_TYPE_MEM) {
		printf(": mmba is not mem space\n");
		return (ENXIO);
	}
	if (pci_mapreg_map(pa, EM_MMBA, PCI_MAPREG_MEM_TYPE(val), 0,
	    &sc->osdep.mem_bus_space_tag, &sc->osdep.mem_bus_space_handle,
	    &sc->osdep.em_membase, &sc->osdep.em_memsize, 0)) {
		printf(": cannot find mem space\n");
		return (ENXIO);
	}

	if (sc->hw.mac_type > em_82543) {
		/* Figure out where our I/O BAR is ? */
		for (rid = PCI_MAPREG_START; rid < PCI_MAPREG_END;) {
			val = pci_conf_read(pa->pa_pc, pa->pa_tag, rid);
			if (PCI_MAPREG_TYPE(val) == PCI_MAPREG_TYPE_IO) {
				sc->io_rid = rid;
				break;
			}
			rid += 4;
			if (PCI_MAPREG_MEM_TYPE(val) ==
			    PCI_MAPREG_MEM_TYPE_64BIT)
				rid += 4;	/* skip high bits, too */
		}

		if (pci_mapreg_map(pa, rid, PCI_MAPREG_TYPE_IO, 0,
		    &sc->osdep.io_bus_space_tag, &sc->osdep.io_bus_space_handle,
		    &sc->osdep.em_iobase, &sc->osdep.em_iosize, 0)) {
			printf(": cannot find i/o space\n");
			return (ENXIO);
		}

		sc->hw.io_base = 0;
	}

	/* for ICH8 and family we need to find the flash memory */
	if (sc->hw.mac_type == em_ich8lan ||
	    sc->hw.mac_type == em_ich9lan ||
	    sc->hw.mac_type == em_ich10lan) {
		val = pci_conf_read(pa->pa_pc, pa->pa_tag, EM_FLASH);
		if (PCI_MAPREG_TYPE(val) != PCI_MAPREG_TYPE_MEM) {
			printf(": flash is not mem space\n");
			return (ENXIO);
		}

		if (pci_mapreg_map(pa, EM_FLASH, PCI_MAPREG_MEM_TYPE(val), 0,
		    &sc->osdep.flash_bus_space_tag, &sc->osdep.flash_bus_space_handle,
		    &sc->osdep.em_flashbase, &sc->osdep.em_flashsize, 0)) {
			printf(": cannot find mem space\n");
			return (ENXIO);
		}
        }

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return (ENXIO);
	}

	sc->osdep.dev = (struct device *)sc;
	sc->hw.back = &sc->osdep;

	intrstr = pci_intr_string(pc, ih);
	sc->sc_intrhand = pci_intr_establish(pc, ih, IPL_NET, em_intr, sc,
					      sc->sc_dv.dv_xname);
	if (sc->sc_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return (ENXIO);
	}
	printf(": %s", intrstr);

	/*
	 * the ICP_xxxx device has multiple, duplicate register sets for
	 * use when it is being used as a network processor. Disable those
	 * registers here, as they are not necessary in this context and
	 * can confuse the system
	 */
	if(sc->hw.mac_type == em_icp_xxxx) {
		uint8_t offset;
		pcireg_t val;
		
		if (!pci_get_capability(sc->osdep.em_pa.pa_pc, 
		    sc->osdep.em_pa.pa_tag, PCI_CAP_ID_ST, (int*) &offset, 
		    &val)) {
			return (0);
		}
		offset += PCI_ST_SMIA_OFFSET;
		pci_conf_write(sc->osdep.em_pa.pa_pc, sc->osdep.em_pa.pa_tag,
		    offset, 0x06);
		E1000_WRITE_REG(&sc->hw, IMC1, ~0x0);
		E1000_WRITE_REG(&sc->hw, IMC2, ~0x0);
	}
	return (0);
}

void
em_free_pci_resources(struct em_softc *sc)
{
	struct pci_attach_args *pa = &sc->osdep.em_pa;
	pci_chipset_tag_t	pc = pa->pa_pc;

	if (sc->sc_intrhand)
		pci_intr_disestablish(pc, sc->sc_intrhand);
	sc->sc_intrhand = 0;

	if (sc->osdep.em_flashbase)
		bus_space_unmap(sc->osdep.flash_bus_space_tag, sc->osdep.flash_bus_space_handle,
				sc->osdep.em_flashsize);
	sc->osdep.em_flashbase = 0;

	if (sc->osdep.em_iobase)
		bus_space_unmap(sc->osdep.io_bus_space_tag, sc->osdep.io_bus_space_handle,
				sc->osdep.em_iosize);
	sc->osdep.em_iobase = 0;

	if (sc->osdep.em_membase)
		bus_space_unmap(sc->osdep.mem_bus_space_tag, sc->osdep.mem_bus_space_handle,
				sc->osdep.em_memsize);
	sc->osdep.em_membase = 0;
}

/*********************************************************************
 *
 *  Initialize the hardware to a configuration as specified by the
 *  em_softc structure. The controller is reset, the EEPROM is
 *  verified, the MAC address is set, then the shared initialization
 *  routines are called.
 *
 **********************************************************************/
int
em_hardware_init(struct em_softc *sc)
{
	uint32_t ret_val;
	u_int16_t rx_buffer_size;

	INIT_DEBUGOUT("em_hardware_init: begin");
	/* Issue a global reset */
	em_reset_hw(&sc->hw);

	/* When hardware is reset, fifo_head is also reset */
	sc->tx_fifo_head = 0;

	/* Make sure we have a good EEPROM before we read from it */
	if (em_validate_eeprom_checksum(&sc->hw) < 0) {
		/*
		 * Some PCIe parts fail the first check due to
		 * the link being in sleep state, call it again,
		 * if it fails a second time its a real issue.
		 */
		if (em_validate_eeprom_checksum(&sc->hw) < 0) {
			printf("%s: The EEPROM Checksum Is Not Valid\n",
			       sc->sc_dv.dv_xname);
			return (EIO);
		}
	}

	if (em_read_part_num(&sc->hw, &(sc->part_num)) < 0) {
		printf("%s: EEPROM read error while reading part number\n",
		       sc->sc_dv.dv_xname);
		return (EIO);
	}

	/* Set up smart power down as default off on newer adapters */
	if (!em_smart_pwr_down &&
	     (sc->hw.mac_type == em_82571 ||
	      sc->hw.mac_type == em_82572 ||
	      sc->hw.mac_type == em_82575)) {
		uint16_t phy_tmp = 0;

		/* Speed up time to link by disabling smart power down */
		em_read_phy_reg(&sc->hw, IGP02E1000_PHY_POWER_MGMT, &phy_tmp);
		phy_tmp &= ~IGP02E1000_PM_SPD;
		em_write_phy_reg(&sc->hw, IGP02E1000_PHY_POWER_MGMT, phy_tmp);
	}

	/*
	 * These parameters control the automatic generation (Tx) and 
	 * response (Rx) to Ethernet PAUSE frames.
	 * - High water mark should allow for at least two frames to be
	 *   received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit.  Here we use an arbitary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer.  There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 * - The pause time is fairly large at 1000 x 512ns = 512 usec.
	 */
	rx_buffer_size = ((E1000_READ_REG(&sc->hw, PBA) & 0xffff) << 10 );

	sc->hw.fc_high_water = rx_buffer_size -
	    EM_ROUNDUP(sc->hw.max_frame_size, 1024);
	sc->hw.fc_low_water = sc->hw.fc_high_water - 1500;
	if (sc->hw.mac_type == em_80003es2lan)
		sc->hw.fc_pause_time = 0xFFFF;
	else
		sc->hw.fc_pause_time = 1000;
	sc->hw.fc_send_xon = TRUE;
	sc->hw.fc = E1000_FC_FULL;

	if ((ret_val = em_init_hw(&sc->hw)) != 0) {
		if (ret_val == E1000_DEFER_INIT) {
			INIT_DEBUGOUT("\nHardware Initialization Deferred ");
			return (EAGAIN);
		}
		printf("%s: Hardware Initialization Failed",
		       sc->sc_dv.dv_xname);
		return (EIO);
	}

	em_check_for_link(&sc->hw);

	return (0);
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
void
em_setup_interface(struct em_softc *sc)
{
	struct ifnet   *ifp;
	u_char fiber_type = IFM_1000_SX;

	INIT_DEBUGOUT("em_setup_interface: begin");

	ifp = &sc->interface_data.ac_if;
	strlcpy(ifp->if_xname, sc->sc_dv.dv_xname, IFNAMSIZ);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = em_ioctl;
	ifp->if_start = em_start;
	ifp->if_watchdog = em_watchdog;
	ifp->if_hardmtu =
		sc->hw.max_frame_size - ETHER_HDR_LEN - ETHER_CRC_LEN;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->num_tx_desc - 1);
	IFQ_SET_READY(&ifp->if_snd);

	m_clsetwms(ifp, MCLBYTES, 4, sc->num_rx_desc);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

#ifdef EM_CSUM_OFFLOAD
	if (sc->hw.mac_type >= em_82543)
		ifp->if_capabilities |= IFCAP_CSUM_TCPv4|IFCAP_CSUM_UDPv4;
#endif

	/* 
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&sc->media, IFM_IMASK, em_media_change,
		     em_media_status);
	if (sc->hw.media_type == em_media_type_fiber ||
	    sc->hw.media_type == em_media_type_internal_serdes) {
		if (sc->hw.mac_type == em_82545)
			fiber_type = IFM_1000_LX;
		ifmedia_add(&sc->media, IFM_ETHER | fiber_type | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | fiber_type, 
			    0, NULL);
	} else {
		ifmedia_add(&sc->media, IFM_ETHER | IFM_10_T, 0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_10_T | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_100_TX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_100_TX | IFM_FDX, 
			    0, NULL);
		if (sc->hw.phy_type != em_phy_ife) {
			ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T | IFM_FDX, 
				    0, NULL);
			ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
		}
	}
	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);
}

int
em_detach(struct device *self, int flags)
{
	struct em_softc *sc = (struct em_softc *)self;
	struct ifnet *ifp = &sc->interface_data.ac_if;
	struct pci_attach_args *pa = &sc->osdep.em_pa;
	pci_chipset_tag_t	pc = pa->pa_pc;

	if (sc->sc_intrhand)
		pci_intr_disestablish(pc, sc->sc_intrhand);
	sc->sc_intrhand = 0;

	if (sc->sc_powerhook != NULL)
		powerhook_disestablish(sc->sc_powerhook);

	em_stop(sc, 1);

	em_free_pci_resources(sc);
	em_dma_free(sc, &sc->rxdma);
	em_dma_free(sc, &sc->txdma);

	ether_ifdetach(ifp);
	if_detach(ifp);

	return (0);
}

int
em_activate(struct device *self, int act)
{
	struct em_softc *sc = (struct em_softc *)self;
	struct ifnet *ifp = &sc->interface_data.ac_if;
	int rv = 0;

	switch (act) {
	case DVACT_SUSPEND:
		/* We have no children atm, but we will soon */
		rv = config_activate_children(self, act);
		break;
	case DVACT_RESUME:
		em_stop(sc, 0);
		rv = config_activate_children(self, act);
		if (ifp->if_flags & IFF_UP)
			em_init(sc);
		break;
	}
	return rv;
}

/*********************************************************************
 *
 *  Workaround for SmartSpeed on 82541 and 82547 controllers
 *
 **********************************************************************/	
void
em_smartspeed(struct em_softc *sc)
{
	uint16_t phy_tmp;
 
	if (sc->link_active || (sc->hw.phy_type != em_phy_igp) || 
	    !sc->hw.autoneg || !(sc->hw.autoneg_advertised & ADVERTISE_1000_FULL))
		return;

	if (sc->smartspeed == 0) {
		/* If Master/Slave config fault is asserted twice,
		 * we assume back-to-back */
		em_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &phy_tmp);
		if (!(phy_tmp & SR_1000T_MS_CONFIG_FAULT))
			return;
		em_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &phy_tmp);
		if (phy_tmp & SR_1000T_MS_CONFIG_FAULT) {
			em_read_phy_reg(&sc->hw, PHY_1000T_CTRL,
					&phy_tmp);
			if (phy_tmp & CR_1000T_MS_ENABLE) {
				phy_tmp &= ~CR_1000T_MS_ENABLE;
				em_write_phy_reg(&sc->hw,
						    PHY_1000T_CTRL, phy_tmp);
				sc->smartspeed++;
				if (sc->hw.autoneg &&
				    !em_phy_setup_autoneg(&sc->hw) &&
				    !em_read_phy_reg(&sc->hw, PHY_CTRL,
						       &phy_tmp)) {
					phy_tmp |= (MII_CR_AUTO_NEG_EN |  
						    MII_CR_RESTART_AUTO_NEG);
					em_write_phy_reg(&sc->hw,
							 PHY_CTRL, phy_tmp);
				}
			}
		}
		return;
	} else if (sc->smartspeed == EM_SMARTSPEED_DOWNSHIFT) {
		/* If still no link, perhaps using 2/3 pair cable */
		em_read_phy_reg(&sc->hw, PHY_1000T_CTRL, &phy_tmp);
		phy_tmp |= CR_1000T_MS_ENABLE;
		em_write_phy_reg(&sc->hw, PHY_1000T_CTRL, phy_tmp);
		if (sc->hw.autoneg &&
		    !em_phy_setup_autoneg(&sc->hw) &&
		    !em_read_phy_reg(&sc->hw, PHY_CTRL, &phy_tmp)) {
			phy_tmp |= (MII_CR_AUTO_NEG_EN |
				    MII_CR_RESTART_AUTO_NEG);
			em_write_phy_reg(&sc->hw, PHY_CTRL, phy_tmp);
		}
	}
	/* Restart process after EM_SMARTSPEED_MAX iterations */
	if (sc->smartspeed++ == EM_SMARTSPEED_MAX)
		sc->smartspeed = 0;
}

/*
 * Manage DMA'able memory.
 */
int
em_dma_malloc(struct em_softc *sc, bus_size_t size,
    struct em_dma_alloc *dma, int mapflags)
{
	int r;

	dma->dma_tag = sc->osdep.em_pa.pa_dmat;
	r = bus_dmamap_create(dma->dma_tag, size, 1,
	    size, 0, BUS_DMA_NOWAIT, &dma->dma_map);
	if (r != 0) {
		printf("%s: em_dma_malloc: bus_dmamap_create failed; "
			"error %u\n", sc->sc_dv.dv_xname, r);
		goto fail_0;
	}

	r = bus_dmamem_alloc(dma->dma_tag, size, PAGE_SIZE, 0, &dma->dma_seg,
	    1, &dma->dma_nseg, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: em_dma_malloc: bus_dmammem_alloc failed; "
			"size %lu, error %d\n", sc->sc_dv.dv_xname,
			(unsigned long)size, r);
		goto fail_1;
	}

	r = bus_dmamem_map(dma->dma_tag, &dma->dma_seg, dma->dma_nseg, size,
	    &dma->dma_vaddr, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: em_dma_malloc: bus_dmammem_map failed; "
			"size %lu, error %d\n", sc->sc_dv.dv_xname,
			(unsigned long)size, r);
		goto fail_2;
	}

	r = bus_dmamap_load(sc->osdep.em_pa.pa_dmat, dma->dma_map,
			    dma->dma_vaddr, size, NULL,
			    mapflags | BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: em_dma_malloc: bus_dmamap_load failed; "
			"error %u\n", sc->sc_dv.dv_xname, r);
		goto fail_3;
	}

	dma->dma_size = size;
	return (0);

fail_3:
	bus_dmamem_unmap(dma->dma_tag, dma->dma_vaddr, size);
fail_2:
	bus_dmamem_free(dma->dma_tag, &dma->dma_seg, dma->dma_nseg);
fail_1:
	bus_dmamap_destroy(dma->dma_tag, dma->dma_map);
fail_0:
	dma->dma_map = NULL;
	dma->dma_tag = NULL;

	return (r);
}

void
em_dma_free(struct em_softc *sc, struct em_dma_alloc *dma)
{
	if (dma->dma_tag == NULL)
		return;

	if (dma->dma_map != NULL) {
		bus_dmamap_sync(dma->dma_tag, dma->dma_map, 0,
		    dma->dma_map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dma->dma_tag, dma->dma_map);
		bus_dmamem_unmap(dma->dma_tag, dma->dma_vaddr, dma->dma_size);
		bus_dmamem_free(dma->dma_tag, &dma->dma_seg, dma->dma_nseg);
		bus_dmamap_destroy(dma->dma_tag, dma->dma_map);
	}
	dma->dma_tag = NULL;
}

/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire.
 *
 **********************************************************************/
int
em_allocate_transmit_structures(struct em_softc *sc)
{
	if (!(sc->tx_buffer_area = malloc(sizeof(struct em_buffer) *
	    sc->num_tx_desc, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		printf("%s: Unable to allocate tx_buffer memory\n", 
		       sc->sc_dv.dv_xname);
		return (ENOMEM);
	}

	return (0);
}

/*********************************************************************
 *
 *  Allocate and initialize transmit structures. 
 *
 **********************************************************************/
int
em_setup_transmit_structures(struct em_softc *sc)
{
	struct  em_buffer *tx_buffer;
	int error, i;

	if ((error = em_allocate_transmit_structures(sc)) != 0)
		goto fail;

	bzero((void *) sc->tx_desc_base,
	      (sizeof(struct em_tx_desc)) * sc->num_tx_desc);

	sc->txtag = sc->osdep.em_pa.pa_dmat;

	tx_buffer = sc->tx_buffer_area;
	for (i = 0; i < sc->num_tx_desc; i++) {
		error = bus_dmamap_create(sc->txtag, MAX_JUMBO_FRAME_SIZE,
			    EM_MAX_SCATTER, MAX_JUMBO_FRAME_SIZE, 0,
			    BUS_DMA_NOWAIT, &tx_buffer->map);
		if (error != 0) {
			printf("%s: Unable to create TX DMA map\n",
			    sc->sc_dv.dv_xname);
			goto fail;
		}
		tx_buffer++;
	}

	sc->next_avail_tx_desc = 0;
	sc->next_tx_to_clean = 0;

	/* Set number of descriptors available */
	sc->num_tx_desc_avail = sc->num_tx_desc;

	/* Set checksum context */
	sc->active_checksum_context = OFFLOAD_NONE;
	bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map, 0,
	    sc->txdma.dma_size, BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);

fail:
	em_free_transmit_structures(sc);
	return (error);
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
void
em_initialize_transmit_unit(struct em_softc *sc)
{
	u_int32_t	reg_tctl, reg_tipg = 0;
	u_int64_t	bus_addr;

	INIT_DEBUGOUT("em_initialize_transmit_unit: begin");

	/* Setup the Base and Length of the Tx Descriptor Ring */
	bus_addr = sc->txdma.dma_map->dm_segs[0].ds_addr;
	E1000_WRITE_REG(&sc->hw, TDLEN, 
			sc->num_tx_desc *
			sizeof(struct em_tx_desc));
	E1000_WRITE_REG(&sc->hw, TDBAH, (u_int32_t)(bus_addr >> 32));
	E1000_WRITE_REG(&sc->hw, TDBAL, (u_int32_t)bus_addr);

	/* Setup the HW Tx Head and Tail descriptor pointers */
	E1000_WRITE_REG(&sc->hw, TDT, 0);
	E1000_WRITE_REG(&sc->hw, TDH, 0);

	HW_DEBUGOUT2("Base = %x, Length = %x\n", 
		     E1000_READ_REG(&sc->hw, TDBAL),
		     E1000_READ_REG(&sc->hw, TDLEN));

	/* Set the default values for the Tx Inter Packet Gap timer */
	switch (sc->hw.mac_type) {
	case em_82542_rev2_0:
	case em_82542_rev2_1:
		reg_tipg = DEFAULT_82542_TIPG_IPGT;
		reg_tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		reg_tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	case em_80003es2lan:
		reg_tipg = DEFAULT_82543_TIPG_IPGR1;
		reg_tipg |= DEFAULT_80003ES2LAN_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	default:
		if (sc->hw.media_type == em_media_type_fiber ||
		    sc->hw.media_type == em_media_type_internal_serdes)
			reg_tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
		else
			reg_tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
		reg_tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		reg_tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
	}


	E1000_WRITE_REG(&sc->hw, TIPG, reg_tipg);
	E1000_WRITE_REG(&sc->hw, TIDV, sc->tx_int_delay);
	if (sc->hw.mac_type >= em_82540)
		E1000_WRITE_REG(&sc->hw, TADV, sc->tx_abs_int_delay);

	/* Setup Transmit Descriptor Base Settings */   
	sc->txd_cmd = E1000_TXD_CMD_IFCS;

	if (sc->hw.mac_type == em_82575) {
		/* 82575/6 need to enable the TX queue and lack the IDE bit */
		reg_tctl = E1000_READ_REG(&sc->hw, TXDCTL);
		reg_tctl |= E1000_TXDCTL_QUEUE_ENABLE;
		E1000_WRITE_REG(&sc->hw, TXDCTL, reg_tctl);
	} else if (sc->tx_int_delay > 0)
		sc->txd_cmd |= E1000_TXD_CMD_IDE;

	/* Program the Transmit Control Register */
	reg_tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
	if (sc->hw.mac_type >= em_82571)
		reg_tctl |= E1000_TCTL_MULR;
	if (sc->link_duplex == FULL_DUPLEX)
		reg_tctl |= E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	else
		reg_tctl |= E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	/* This write will effectively turn on the transmit unit */
	E1000_WRITE_REG(&sc->hw, TCTL, reg_tctl);
}

/*********************************************************************
 *
 *  Free all transmit related data structures.
 *
 **********************************************************************/
void
em_free_transmit_structures(struct em_softc *sc)
{
	struct em_buffer   *tx_buffer;
	int		i;

	INIT_DEBUGOUT("free_transmit_structures: begin");

	if (sc->tx_buffer_area != NULL) {
		tx_buffer = sc->tx_buffer_area;
		for (i = 0; i < sc->num_tx_desc; i++, tx_buffer++) {
			if (tx_buffer->map != NULL &&
			    tx_buffer->map->dm_nsegs > 0) {
				bus_dmamap_sync(sc->txtag, tx_buffer->map,
				    0, tx_buffer->map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->txtag,
				    tx_buffer->map);
			}
			if (tx_buffer->m_head != NULL) {
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
			}
			if (tx_buffer->map != NULL) {
				bus_dmamap_destroy(sc->txtag,
				    tx_buffer->map);
				tx_buffer->map = NULL;
			}
		}
	}
	if (sc->tx_buffer_area != NULL) {
		free(sc->tx_buffer_area, M_DEVBUF);
		sc->tx_buffer_area = NULL;
	}
	if (sc->txtag != NULL)
		sc->txtag = NULL;
}

#ifdef EM_CSUM_OFFLOAD
/*********************************************************************
 *
 *  The offload context needs to be set when we transfer the first
 *  packet of a particular protocol (TCP/UDP). We change the
 *  context only if the protocol type changes.
 *
 **********************************************************************/
void
em_transmit_checksum_setup(struct em_softc *sc, struct mbuf *mp,
    u_int32_t *txd_upper, u_int32_t *txd_lower)
{
	struct em_context_desc *TXD;
	struct em_buffer *tx_buffer;
	int curr_txd;

	if (mp->m_pkthdr.csum_flags) {
		if (mp->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT) {
			*txd_upper = E1000_TXD_POPTS_TXSM << 8;
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			if (sc->active_checksum_context == OFFLOAD_TCP_IP)
				return;
			else
				sc->active_checksum_context = OFFLOAD_TCP_IP;
		} else if (mp->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT) {
			*txd_upper = E1000_TXD_POPTS_TXSM << 8;
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			if (sc->active_checksum_context == OFFLOAD_UDP_IP)
				return;
			else
				sc->active_checksum_context = OFFLOAD_UDP_IP;
		} else {
			*txd_upper = 0;
			*txd_lower = 0;
			return;
		}
	} else {
		*txd_upper = 0;
		*txd_lower = 0;
		return;
	}

	/* If we reach this point, the checksum offload context
	 * needs to be reset.
	 */
	curr_txd = sc->next_avail_tx_desc;
	tx_buffer = &sc->tx_buffer_area[curr_txd];
	TXD = (struct em_context_desc *) &sc->tx_desc_base[curr_txd];

	TXD->lower_setup.ip_fields.ipcss = ETHER_HDR_LEN;
	TXD->lower_setup.ip_fields.ipcso = 
	    ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
	TXD->lower_setup.ip_fields.ipcse = 
	    htole16(ETHER_HDR_LEN + sizeof(struct ip) - 1);

	TXD->upper_setup.tcp_fields.tucss = 
	    ETHER_HDR_LEN + sizeof(struct ip);
	TXD->upper_setup.tcp_fields.tucse = htole16(0);

	if (sc->active_checksum_context == OFFLOAD_TCP_IP) {
		TXD->upper_setup.tcp_fields.tucso = 
		    ETHER_HDR_LEN + sizeof(struct ip) + 
		    offsetof(struct tcphdr, th_sum);
	} else if (sc->active_checksum_context == OFFLOAD_UDP_IP) {
		TXD->upper_setup.tcp_fields.tucso = 
		    ETHER_HDR_LEN + sizeof(struct ip) + 
		    offsetof(struct udphdr, uh_sum);
	}

	TXD->tcp_seg_setup.data = htole32(0);
	TXD->cmd_and_length = htole32(sc->txd_cmd | E1000_TXD_CMD_DEXT);

	tx_buffer->m_head = NULL;
	tx_buffer->next_eop = -1;

	if (++curr_txd == sc->num_tx_desc)
		curr_txd = 0;

	sc->num_tx_desc_avail--;
	sc->next_avail_tx_desc = curr_txd;
}
#endif /* EM_CSUM_OFFLOAD */

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue. 
 *
 **********************************************************************/
void
em_txeof(struct em_softc *sc)
{
	int first, last, done, num_avail;
	struct em_buffer *tx_buffer;
	struct em_tx_desc   *tx_desc, *eop_desc;
	struct ifnet   *ifp = &sc->interface_data.ac_if;

	if (sc->num_tx_desc_avail == sc->num_tx_desc)
		return;

	num_avail = sc->num_tx_desc_avail;
	first = sc->next_tx_to_clean;
	tx_desc = &sc->tx_desc_base[first];
	tx_buffer = &sc->tx_buffer_area[first];
	last = tx_buffer->next_eop;
	eop_desc = &sc->tx_desc_base[last];

	/*
	 * What this does is get the index of the
	 * first descriptor AFTER the EOP of the 
	 * first packet, that way we can do the
	 * simple comparison on the inner while loop.
	 */
	if (++last == sc->num_tx_desc)
		last = 0;
	done = last;

	bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map, 0,
	    sc->txdma.dma_map->dm_mapsize, BUS_DMASYNC_POSTREAD);
	while (eop_desc->upper.fields.status & E1000_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
			tx_desc->upper.data = 0;
			tx_desc->lower.data = 0;
			num_avail++;

			if (tx_buffer->m_head != NULL) {
				ifp->if_opackets++;
				if (tx_buffer->map->dm_nsegs > 0) {
					bus_dmamap_sync(sc->txtag,
					    tx_buffer->map, 0,
					    tx_buffer->map->dm_mapsize,
					    BUS_DMASYNC_POSTWRITE);
					bus_dmamap_unload(sc->txtag,
					    tx_buffer->map);
				}
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
			}
			tx_buffer->next_eop = -1;

			if (++first == sc->num_tx_desc)
				first = 0;

			tx_buffer = &sc->tx_buffer_area[first];
			tx_desc = &sc->tx_desc_base[first];
		}
		/* See if we can continue to the next packet */
		last = tx_buffer->next_eop;
		if (last != -1) {
			eop_desc = &sc->tx_desc_base[last];
			/* Get new done point */
			if (++last == sc->num_tx_desc)
				last = 0;
			done = last;
		} else
			break;
	}
	bus_dmamap_sync(sc->txdma.dma_tag, sc->txdma.dma_map, 0,
	    sc->txdma.dma_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	sc->next_tx_to_clean = first;

	/*
	 * If we have enough room, clear IFF_OACTIVE to tell the stack
	 * that it is OK to send packets.
	 * If there are no pending descriptors, clear the timeout. Otherwise,
	 * if some descriptors have been freed, restart the timeout.
	 */
	if (num_avail > EM_TX_CLEANUP_THRESHOLD)
		ifp->if_flags &= ~IFF_OACTIVE;

	/* All clean, turn off the timer */
	if (num_avail == sc->num_tx_desc)
		ifp->if_timer = 0;
	/* Some cleaned, reset the timer */
	else if (num_avail != sc->num_tx_desc_avail)
		ifp->if_timer = EM_TX_TIMEOUT;

	sc->num_tx_desc_avail = num_avail;
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
int
em_get_buf(struct em_softc *sc, int i)
{
	struct mbuf    *m;
	struct em_buffer *pkt;
	struct em_rx_desc *desc;
	int error;

	pkt = &sc->rx_buffer_area[i];
	desc = &sc->rx_desc_base[i];

	if (pkt->m_head != NULL) {
		printf("%s: em_get_buf: slot %d already has an mbuf\n",
		    sc->sc_dv.dv_xname, i);
		return (ENOBUFS);
	}

	m = MCLGETI(NULL, M_DONTWAIT, &sc->interface_data.ac_if, MCLBYTES);
	if (!m) {
		sc->mbuf_cluster_failed++;
		return (ENOBUFS);
	}
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	if (sc->hw.max_frame_size <= (MCLBYTES - ETHER_ALIGN))
		m_adj(m, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf(sc->rxtag, pkt->map, m, BUS_DMA_NOWAIT);
	if (error) {
		m_freem(m);
		return (error);
	}

	bus_dmamap_sync(sc->rxtag, pkt->map, 0, pkt->map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);
	pkt->m_head = m;

	bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
	    sizeof(*desc) * i, sizeof(*desc), BUS_DMASYNC_POSTWRITE);

	bzero(desc, sizeof(*desc));
	desc->buffer_addr = htole64(pkt->map->dm_segs[0].ds_addr);

	bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
	    sizeof(*desc) * i, sizeof(*desc), BUS_DMASYNC_PREWRITE);

	sc->rx_ndescs++;

	return (0);
}

/*********************************************************************
 *
 *  Allocate memory for rx_buffer structures. Since we use one 
 *  rx_buffer per received packet, the maximum number of rx_buffer's 
 *  that we'll need is equal to the number of receive descriptors 
 *  that we've allocated.
 *
 **********************************************************************/
int
em_allocate_receive_structures(struct em_softc *sc)
{
	int		i, error;
	struct em_buffer *rx_buffer;

	if (!(sc->rx_buffer_area = malloc(sizeof(struct em_buffer) *
	    sc->num_rx_desc, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		printf("%s: Unable to allocate rx_buffer memory\n", 
		       sc->sc_dv.dv_xname);
		return (ENOMEM);
	}

	sc->rxtag = sc->osdep.em_pa.pa_dmat;

	rx_buffer = sc->rx_buffer_area;
	for (i = 0; i < sc->num_rx_desc; i++, rx_buffer++) {
		error = bus_dmamap_create(sc->rxtag, MCLBYTES, 1,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &rx_buffer->map);
		if (error != 0) {
			printf("%s: em_allocate_receive_structures: "
			    "bus_dmamap_create failed; error %u\n",
			    sc->sc_dv.dv_xname, error);
			goto fail;
		}
		rx_buffer->m_head = NULL;
	}
	bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map, 0,
	    sc->rxdma.dma_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

        return (0);

fail:
	em_free_receive_structures(sc);
	return (error);
}

/*********************************************************************
 *
 *  Allocate and initialize receive structures.
 *  
 **********************************************************************/
int
em_setup_receive_structures(struct em_softc *sc)
{
	bzero((void *) sc->rx_desc_base,
	    (sizeof(struct em_rx_desc)) * sc->num_rx_desc);

	if (em_allocate_receive_structures(sc))
		return (ENOMEM);

	/* Setup our descriptor pointers */
	sc->next_rx_desc_to_check = 0;
	sc->last_rx_desc_filled = sc->num_rx_desc - 1;
	sc->rx_ndescs = 0;

	em_rxfill(sc);
	if (sc->rx_ndescs < 1) {
		printf("%s: unable to fill any rx descriptors\n",
		    sc->sc_dv.dv_xname);
	}

	return (0);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *  
 **********************************************************************/
void
em_initialize_receive_unit(struct em_softc *sc)
{
	u_int32_t	reg_rctl;
	u_int32_t	reg_rxcsum;
	u_int64_t	bus_addr;

	INIT_DEBUGOUT("em_initialize_receive_unit: begin");

	/* Make sure receives are disabled while setting up the descriptor ring */
	E1000_WRITE_REG(&sc->hw, RCTL, 0);

	/* Set the Receive Delay Timer Register */
	E1000_WRITE_REG(&sc->hw, RDTR, 
			sc->rx_int_delay | E1000_RDT_FPDB);

	if (sc->hw.mac_type >= em_82540) {
		if (sc->rx_int_delay)
			E1000_WRITE_REG(&sc->hw, RADV, sc->rx_abs_int_delay);

		/* Set the interrupt throttling rate.  Value is calculated
		 * as DEFAULT_ITR = 1/(MAX_INTS_PER_SEC * 256ns) */
		E1000_WRITE_REG(&sc->hw, ITR, DEFAULT_ITR);
	}

	/* Setup the Base and Length of the Rx Descriptor Ring */
	bus_addr = sc->rxdma.dma_map->dm_segs[0].ds_addr;
	E1000_WRITE_REG(&sc->hw, RDLEN, sc->num_rx_desc *
			sizeof(struct em_rx_desc));
	E1000_WRITE_REG(&sc->hw, RDBAH, (u_int32_t)(bus_addr >> 32));
	E1000_WRITE_REG(&sc->hw, RDBAL, (u_int32_t)bus_addr);

	/* Setup the Receive Control Register */
	reg_rctl = E1000_RCTL_EN | E1000_RCTL_BAM | E1000_RCTL_LBM_NO |
	    E1000_RCTL_RDMTS_HALF |
	    (sc->hw.mc_filter_type << E1000_RCTL_MO_SHIFT);

	if (sc->hw.tbi_compatibility_on == TRUE)
		reg_rctl |= E1000_RCTL_SBP;

	switch (sc->rx_buffer_len) {
	default:
	case EM_RXBUFFER_2048:
		reg_rctl |= E1000_RCTL_SZ_2048;
		break;
	case EM_RXBUFFER_4096:
		reg_rctl |= E1000_RCTL_SZ_4096|E1000_RCTL_BSEX|E1000_RCTL_LPE;
		break;		  
	case EM_RXBUFFER_8192:
		reg_rctl |= E1000_RCTL_SZ_8192|E1000_RCTL_BSEX|E1000_RCTL_LPE;
		break;
	case EM_RXBUFFER_16384:
		reg_rctl |= E1000_RCTL_SZ_16384|E1000_RCTL_BSEX|E1000_RCTL_LPE;
		break;
	}

	if (sc->hw.max_frame_size != ETHER_MAX_LEN)
		reg_rctl |= E1000_RCTL_LPE;

	/* Enable 82543 Receive Checksum Offload for TCP and UDP */
	if (sc->hw.mac_type >= em_82543) {
		reg_rxcsum = E1000_READ_REG(&sc->hw, RXCSUM);
		reg_rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
		E1000_WRITE_REG(&sc->hw, RXCSUM, reg_rxcsum);
	}

	/*
	 * XXX TEMPORARY WORKAROUND: on some systems with 82573
	 * long latencies are observed, like Lenovo X60.
	 */
	if (sc->hw.mac_type == em_82573)
		E1000_WRITE_REG(&sc->hw, RDTR, 0x20);

	/* Enable Receives */
	E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);

	/* Setup the HW Rx Head and Tail Descriptor Pointers */
	E1000_WRITE_REG(&sc->hw, RDH, 0);
	E1000_WRITE_REG(&sc->hw, RDT, sc->last_rx_desc_filled);
}

/*********************************************************************
 *
 *  Free receive related data structures.
 *
 **********************************************************************/
void
em_free_receive_structures(struct em_softc *sc)
{
	struct em_buffer   *rx_buffer;
	int		i;

	INIT_DEBUGOUT("free_receive_structures: begin");

	if (sc->rx_buffer_area != NULL) {
		rx_buffer = sc->rx_buffer_area;
		for (i = 0; i < sc->num_rx_desc; i++, rx_buffer++) {
			if (rx_buffer->m_head != NULL) {
				bus_dmamap_sync(sc->rxtag, rx_buffer->map,
				    0, rx_buffer->map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->rxtag, rx_buffer->map);
				m_freem(rx_buffer->m_head);
				rx_buffer->m_head = NULL;
			}
			bus_dmamap_destroy(sc->rxtag, rx_buffer->map);
		}
	}
	if (sc->rx_buffer_area != NULL) {
		free(sc->rx_buffer_area, M_DEVBUF);
		sc->rx_buffer_area = NULL;
	}
	if (sc->rxtag != NULL)
		sc->rxtag = NULL;

	if (sc->fmp != NULL) {
		m_freem(sc->fmp);
		sc->fmp = NULL;
		sc->lmp = NULL;
	}
}

#ifdef __STRICT_ALIGNMENT
void
em_realign(struct em_softc *sc, struct mbuf *m, u_int16_t *prev_len_adj)
{
	unsigned char tmp_align_buf[ETHER_ALIGN];
	int tmp_align_buf_len = 0;

	/*
	 * The Ethernet payload is not 32-bit aligned when
	 * Jumbo packets are enabled, so on architectures with
	 * strict alignment we need to shift the entire packet
	 * ETHER_ALIGN bytes. Ugh.
	 */
	if (sc->hw.max_frame_size <= (MCLBYTES - ETHER_ALIGN))
		return;

	if (*prev_len_adj > sc->align_buf_len)
		*prev_len_adj -= sc->align_buf_len;
	else
		*prev_len_adj = 0;

	if (m->m_len > (MCLBYTES - ETHER_ALIGN)) {
		bcopy(m->m_data + (MCLBYTES - ETHER_ALIGN),
		    &tmp_align_buf, ETHER_ALIGN);
		tmp_align_buf_len = m->m_len -
		    (MCLBYTES - ETHER_ALIGN);
		m->m_len -= ETHER_ALIGN;
	} 

	if (m->m_len) {
		bcopy(m->m_data, m->m_data + ETHER_ALIGN, m->m_len);
		if (!sc->align_buf_len)
			m->m_data += ETHER_ALIGN;
	}

	if (sc->align_buf_len) {
		m->m_len += sc->align_buf_len;
		bcopy(&sc->align_buf, m->m_data, sc->align_buf_len);
	}

	if (tmp_align_buf_len) 
		bcopy(&tmp_align_buf, &sc->align_buf, tmp_align_buf_len);

	sc->align_buf_len = tmp_align_buf_len;
}
#endif /* __STRICT_ALIGNMENT */

int
em_rxfill(struct em_softc *sc)
{
	int post = 0;
	int i;

	i = sc->last_rx_desc_filled;

	while (sc->rx_ndescs < sc->num_rx_desc) {
		if (++i == sc->num_rx_desc)
			i = 0;

		if (em_get_buf(sc, i) != 0)
			break;

		sc->last_rx_desc_filled = i;
		post = 1;
	}

	return (post);
}

/*********************************************************************
 *
 *  This routine executes in interrupt context. It replenishes
 *  the mbufs in the descriptor and sends data which has been
 *  dma'ed into host memory to upper layer.
 *
 *  We loop at most count times if count is > 0, or until done if
 *  count < 0.
 *
 *********************************************************************/
void
em_rxeof(struct em_softc *sc, int count)
{
	struct ifnet	    *ifp = &sc->interface_data.ac_if;
	struct mbuf	    *m;
	u_int8_t	    accept_frame = 0;
	u_int8_t	    eop = 0;
	u_int16_t	    len, desc_len, prev_len_adj;
	int		    i;

	/* Pointer to the receive descriptor being examined. */
	struct em_rx_desc   *desc;
	struct em_buffer    *pkt;
	u_int8_t	    status;

	ifp = &sc->interface_data.ac_if;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return;

	i = sc->next_rx_desc_to_check;

	while (count != 0 && sc->rx_ndescs > 0) {
		m = NULL;

		desc = &sc->rx_desc_base[i];
		pkt = &sc->rx_buffer_area[i];

		bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
		    sizeof(*desc) * i, sizeof(*desc),
		    BUS_DMASYNC_POSTREAD);

		status = desc->status;
		if (!ISSET(status, E1000_RXD_STAT_DD)) {
			bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
			    sizeof(*desc) * i, sizeof(*desc),
			    BUS_DMASYNC_PREREAD);
			break;
		}

		/* pull the mbuf off the ring */
		bus_dmamap_sync(sc->rxtag, pkt->map, 0, pkt->map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rxtag, pkt->map);
		m = pkt->m_head;
		pkt->m_head = NULL;

		if (m == NULL) {
			panic("em_rxeof: NULL mbuf in slot %d "
			    "(nrx %d, filled %d)", i, sc->rx_ndescs,
			    sc->last_rx_desc_filled);
		}

		m_cluncount(m, 1);
		sc->rx_ndescs--;

		accept_frame = 1;
		prev_len_adj = 0;
		desc_len = letoh16(desc->length);

		if (status & E1000_RXD_STAT_EOP) {
			count--;
			eop = 1;
			if (desc_len < ETHER_CRC_LEN) {
				len = 0;
				prev_len_adj = ETHER_CRC_LEN - desc_len;
			} else
				len = desc_len - ETHER_CRC_LEN;
		} else {
			eop = 0;
			len = desc_len;
		}

		if (desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK) {
			u_int8_t last_byte;
			u_int32_t pkt_len = desc_len;

			if (sc->fmp != NULL)
				pkt_len += sc->fmp->m_pkthdr.len; 

			last_byte = *(mtod(m, caddr_t) + desc_len - 1);
			if (TBI_ACCEPT(&sc->hw, status, desc->errors,
			    pkt_len, last_byte)) {
				em_tbi_adjust_stats(&sc->hw, &sc->stats, 
				    pkt_len, sc->hw.mac_addr);
				if (len > 0)
					len--;
			} else
				accept_frame = 0;
		}

		if (accept_frame) {
			/* Assign correct length to the current fragment */
			m->m_len = len;

			em_realign(sc, m, &prev_len_adj); /* STRICT_ALIGN */

			if (sc->fmp == NULL) {
				m->m_pkthdr.len = m->m_len;
				sc->fmp = m;	 /* Store the first mbuf */
				sc->lmp = m;
			} else {
				/* Chain mbuf's together */
				m->m_flags &= ~M_PKTHDR;
				/*
				 * Adjust length of previous mbuf in chain if
				 * we received less than 4 bytes in the last
				 * descriptor.
				 */
				if (prev_len_adj > 0) {
					sc->lmp->m_len -= prev_len_adj;
					sc->fmp->m_pkthdr.len -= prev_len_adj;
				}
				sc->lmp->m_next = m;
				sc->lmp = m;
				sc->fmp->m_pkthdr.len += m->m_len;
			}

			if (eop) {
				ifp->if_ipackets++;

				m = sc->fmp;
				m->m_pkthdr.rcvif = ifp;

				em_receive_checksum(sc, desc, m);
#if NVLAN > 0
				if (desc->status & E1000_RXD_STAT_VP) {
					m->m_pkthdr.ether_vtag =
					    (letoh16(desc->special) &
					     E1000_RXD_SPC_VLAN_MASK);
					m->m_flags |= M_VLANTAG;
				}
#endif
#if NBPFILTER > 0
				if (ifp->if_bpf) {
					bpf_mtap_ether(ifp->if_bpf, m,
					    BPF_DIRECTION_IN);
				}
#endif

				ether_input_mbuf(ifp, m);

				sc->fmp = NULL;
				sc->lmp = NULL;
			}
		} else {
			sc->dropped_pkts++;

			if (sc->fmp != NULL) {
 				m_freem(sc->fmp);
				sc->fmp = NULL;
				sc->lmp = NULL;
			}

			m_freem(m);
		}

		bus_dmamap_sync(sc->rxdma.dma_tag, sc->rxdma.dma_map,
		    sizeof(*desc) * i, sizeof(*desc),
		    BUS_DMASYNC_PREREAD);

		/* Advance our pointers to the next descriptor. */
		if (++i == sc->num_rx_desc)
			i = 0;
	}
	sc->next_rx_desc_to_check = i;
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid. 
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
void
em_receive_checksum(struct em_softc *sc, struct em_rx_desc *rx_desc,
    struct mbuf *mp)
{
	/* 82543 or newer only */
	if ((sc->hw.mac_type < em_82543) ||
	    /* Ignore Checksum bit is set */
	    (rx_desc->status & E1000_RXD_STAT_IXSM)) {
		mp->m_pkthdr.csum_flags = 0;
		return;
	}

	if (rx_desc->status & E1000_RXD_STAT_IPCS) {
		/* Did it pass? */
		if (!(rx_desc->errors & E1000_RXD_ERR_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags = M_IPV4_CSUM_IN_OK;

		} else
			mp->m_pkthdr.csum_flags = 0;
	}

	if (rx_desc->status & E1000_RXD_STAT_TCPCS) {
		/* Did it pass? */        
		if (!(rx_desc->errors & E1000_RXD_ERR_TCPE))
			mp->m_pkthdr.csum_flags |=
				M_TCP_CSUM_IN_OK | M_UDP_CSUM_IN_OK;
	}
}

/*
 * This turns on the hardware offload of the VLAN
 * tag insertion and strip
 */
void 
em_enable_hw_vlans(struct em_softc *sc)
{
	uint32_t ctrl;

	ctrl = E1000_READ_REG(&sc->hw, CTRL);
	ctrl |= E1000_CTRL_VME;
	E1000_WRITE_REG(&sc->hw, CTRL, ctrl);
}

void
em_enable_intr(struct em_softc *sc)
{
	E1000_WRITE_REG(&sc->hw, IMS, (IMS_ENABLE_MASK));
}

void
em_disable_intr(struct em_softc *sc)
{
	/*
	 * The first version of 82542 had an errata where when link
	 * was forced it would stay up even if the cable was disconnected
	 * Sequence errors were used to detect the disconnect and then
	 * the driver would unforce the link.  This code is in the ISR.
	 * For this to work correctly the Sequence error interrupt had
	 * to be enabled all the time.
	 */

	if (sc->hw.mac_type == em_82542_rev2_0)
		E1000_WRITE_REG(&sc->hw, IMC, (0xffffffff & ~E1000_IMC_RXSEQ));
	else
		E1000_WRITE_REG(&sc->hw, IMC, 0xffffffff);
}

int
em_is_valid_ether_addr(u_int8_t *addr)
{
	const char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN)))
		return (FALSE);

	return (TRUE);
}

void
em_write_pci_cfg(struct em_hw *hw, uint32_t reg, uint16_t *value)
{
	struct pci_attach_args *pa = &((struct em_osdep *)hw->back)->em_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	/* Should we do read/mask/write...?  16 vs 32 bit!!! */
	pci_conf_write(pc, pa->pa_tag, reg, *value);
}

void
em_read_pci_cfg(struct em_hw *hw, uint32_t reg, uint16_t *value)
{
	struct pci_attach_args *pa = &((struct em_osdep *)hw->back)->em_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	*value = pci_conf_read(pc, pa->pa_tag, reg);
}

void
em_pci_set_mwi(struct em_hw *hw)
{
	struct pci_attach_args *pa = &((struct em_osdep *)hw->back)->em_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	/* Should we do read/mask/write...?  16 vs 32 bit!!! */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		(hw->pci_cmd_word | CMD_MEM_WRT_INVALIDATE));
}

void
em_pci_clear_mwi(struct em_hw *hw)
{
	struct pci_attach_args *pa = &((struct em_osdep *)hw->back)->em_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	/* Should we do read/mask/write...?  16 vs 32 bit!!! */
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
		(hw->pci_cmd_word & ~CMD_MEM_WRT_INVALIDATE));
}

/*
 * We may eventually really do this, but its unnecessary
 * for now so we just return unsupported.
 */
int32_t
em_read_pcie_cap_reg(struct em_hw *hw, uint32_t reg, uint16_t *value)
{
	return -E1000_NOT_IMPLEMENTED;
}

/*********************************************************************
* 82544 Coexistence issue workaround.
*    There are 2 issues.
*       1. Transmit Hang issue.
*    To detect this issue, following equation can be used...
*          SIZE[3:0] + ADDR[2:0] = SUM[3:0].
*          If SUM[3:0] is in between 1 to 4, we will have this issue.
*
*       2. DAC issue.
*    To detect this issue, following equation can be used...
*          SIZE[3:0] + ADDR[2:0] = SUM[3:0].
*          If SUM[3:0] is in between 9 to c, we will have this issue.
*
*
*    WORKAROUND:
*          Make sure we do not have ending address as 1,2,3,4(Hang) or 9,a,b,c (DAC)
*
*** *********************************************************************/
u_int32_t
em_fill_descriptors(u_int64_t address, u_int32_t length,
    PDESC_ARRAY desc_array)
{
        /* Since issue is sensitive to length and address.*/
        /* Let us first check the address...*/
        u_int32_t safe_terminator;
        if (length <= 4) {
                desc_array->descriptor[0].address = address;
                desc_array->descriptor[0].length = length;
                desc_array->elements = 1;
                return desc_array->elements;
        }
        safe_terminator = (u_int32_t)((((u_int32_t)address & 0x7) + (length & 0xF)) & 0xF);
        /* if it does not fall between 0x1 to 0x4 and 0x9 to 0xC then return */
        if (safe_terminator == 0   ||
        (safe_terminator > 4   &&
        safe_terminator < 9)   ||
        (safe_terminator > 0xC &&
        safe_terminator <= 0xF)) {
                desc_array->descriptor[0].address = address;
                desc_array->descriptor[0].length = length;
                desc_array->elements = 1;
                return desc_array->elements;
        }

        desc_array->descriptor[0].address = address;
        desc_array->descriptor[0].length = length - 4;
        desc_array->descriptor[1].address = address + (length - 4);
        desc_array->descriptor[1].length = 4;
        desc_array->elements = 2;
        return desc_array->elements;
}

/**********************************************************************
 *
 *  Update the board statistics counters. 
 *
 **********************************************************************/
void
em_update_stats_counters(struct em_softc *sc)
{
	struct ifnet   *ifp;

	if (sc->hw.media_type == em_media_type_copper ||
	    (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_LU)) {
		sc->stats.symerrs += E1000_READ_REG(&sc->hw, SYMERRS);
		sc->stats.sec += E1000_READ_REG(&sc->hw, SEC);
	}
	sc->stats.crcerrs += E1000_READ_REG(&sc->hw, CRCERRS);
	sc->stats.mpc += E1000_READ_REG(&sc->hw, MPC);
	sc->stats.scc += E1000_READ_REG(&sc->hw, SCC);
	sc->stats.ecol += E1000_READ_REG(&sc->hw, ECOL);

	sc->stats.mcc += E1000_READ_REG(&sc->hw, MCC);
	sc->stats.latecol += E1000_READ_REG(&sc->hw, LATECOL);
	sc->stats.colc += E1000_READ_REG(&sc->hw, COLC);
	sc->stats.dc += E1000_READ_REG(&sc->hw, DC);
	sc->stats.rlec += E1000_READ_REG(&sc->hw, RLEC);
	sc->stats.xonrxc += E1000_READ_REG(&sc->hw, XONRXC);
	sc->stats.xontxc += E1000_READ_REG(&sc->hw, XONTXC);
	sc->stats.xoffrxc += E1000_READ_REG(&sc->hw, XOFFRXC);
	sc->stats.xofftxc += E1000_READ_REG(&sc->hw, XOFFTXC);
	sc->stats.fcruc += E1000_READ_REG(&sc->hw, FCRUC);
	sc->stats.prc64 += E1000_READ_REG(&sc->hw, PRC64);
	sc->stats.prc127 += E1000_READ_REG(&sc->hw, PRC127);
	sc->stats.prc255 += E1000_READ_REG(&sc->hw, PRC255);
	sc->stats.prc511 += E1000_READ_REG(&sc->hw, PRC511);
	sc->stats.prc1023 += E1000_READ_REG(&sc->hw, PRC1023);
	sc->stats.prc1522 += E1000_READ_REG(&sc->hw, PRC1522);
	sc->stats.gprc += E1000_READ_REG(&sc->hw, GPRC);
	sc->stats.bprc += E1000_READ_REG(&sc->hw, BPRC);
	sc->stats.mprc += E1000_READ_REG(&sc->hw, MPRC);
	sc->stats.gptc += E1000_READ_REG(&sc->hw, GPTC);

	/* For the 64-bit byte counters the low dword must be read first. */
	/* Both registers clear on the read of the high dword */

	sc->stats.gorcl += E1000_READ_REG(&sc->hw, GORCL); 
	sc->stats.gorch += E1000_READ_REG(&sc->hw, GORCH);
	sc->stats.gotcl += E1000_READ_REG(&sc->hw, GOTCL);
	sc->stats.gotch += E1000_READ_REG(&sc->hw, GOTCH);

	sc->stats.rnbc += E1000_READ_REG(&sc->hw, RNBC);
	sc->stats.ruc += E1000_READ_REG(&sc->hw, RUC);
	sc->stats.rfc += E1000_READ_REG(&sc->hw, RFC);
	sc->stats.roc += E1000_READ_REG(&sc->hw, ROC);
	sc->stats.rjc += E1000_READ_REG(&sc->hw, RJC);

	sc->stats.torl += E1000_READ_REG(&sc->hw, TORL);
	sc->stats.torh += E1000_READ_REG(&sc->hw, TORH);
	sc->stats.totl += E1000_READ_REG(&sc->hw, TOTL);
	sc->stats.toth += E1000_READ_REG(&sc->hw, TOTH);

	sc->stats.tpr += E1000_READ_REG(&sc->hw, TPR);
	sc->stats.tpt += E1000_READ_REG(&sc->hw, TPT);
	sc->stats.ptc64 += E1000_READ_REG(&sc->hw, PTC64);
	sc->stats.ptc127 += E1000_READ_REG(&sc->hw, PTC127);
	sc->stats.ptc255 += E1000_READ_REG(&sc->hw, PTC255);
	sc->stats.ptc511 += E1000_READ_REG(&sc->hw, PTC511);
	sc->stats.ptc1023 += E1000_READ_REG(&sc->hw, PTC1023);
	sc->stats.ptc1522 += E1000_READ_REG(&sc->hw, PTC1522);
	sc->stats.mptc += E1000_READ_REG(&sc->hw, MPTC);
	sc->stats.bptc += E1000_READ_REG(&sc->hw, BPTC);

	if (sc->hw.mac_type >= em_82543) {
		sc->stats.algnerrc += 
		E1000_READ_REG(&sc->hw, ALGNERRC);
		sc->stats.rxerrc += 
		E1000_READ_REG(&sc->hw, RXERRC);
		sc->stats.tncrs += 
		E1000_READ_REG(&sc->hw, TNCRS);
		sc->stats.cexterr += 
		E1000_READ_REG(&sc->hw, CEXTERR);
		sc->stats.tsctc += 
		E1000_READ_REG(&sc->hw, TSCTC);
		sc->stats.tsctfc += 
		E1000_READ_REG(&sc->hw, TSCTFC);
	}
	ifp = &sc->interface_data.ac_if;

	/* Fill out the OS statistics structure */
	ifp->if_collisions = sc->stats.colc;

	/* Rx Errors */
	ifp->if_ierrors =
	    sc->dropped_pkts +
	    sc->stats.rxerrc +
	    sc->stats.crcerrs +
	    sc->stats.algnerrc +
	    sc->stats.ruc + sc->stats.roc +
	    sc->stats.mpc + sc->stats.cexterr +
	    sc->rx_overruns;

	/* Tx Errors */
	ifp->if_oerrors = sc->stats.ecol + sc->stats.latecol +
	    sc->watchdog_events;
}

#ifdef EM_DEBUG
/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
void
em_print_hw_stats(struct em_softc *sc)
{
	const char * const unit = sc->sc_dv.dv_xname;

	printf("%s: Excessive collisions = %lld\n", unit,
		(long long)sc->stats.ecol);
	printf("%s: Symbol errors = %lld\n", unit,
		(long long)sc->stats.symerrs);
	printf("%s: Sequence errors = %lld\n", unit,
		(long long)sc->stats.sec);
	printf("%s: Defer count = %lld\n", unit,
		(long long)sc->stats.dc);

	printf("%s: Missed Packets = %lld\n", unit,
		(long long)sc->stats.mpc);
	printf("%s: Receive No Buffers = %lld\n", unit,
		(long long)sc->stats.rnbc);
	/* RLEC is inaccurate on some hardware, calculate our own */
	printf("%s: Receive Length Errors = %lld\n", unit,
		((long long)sc->stats.roc +
		(long long)sc->stats.ruc));
	printf("%s: Receive errors = %lld\n", unit,
		(long long)sc->stats.rxerrc);
	printf("%s: Crc errors = %lld\n", unit,
		(long long)sc->stats.crcerrs);
	printf("%s: Alignment errors = %lld\n", unit,
		(long long)sc->stats.algnerrc);
	printf("%s: Carrier extension errors = %lld\n", unit,
		(long long)sc->stats.cexterr);

	printf("%s: RX overruns = %ld\n", unit,
		sc->rx_overruns);
	printf("%s: watchdog timeouts = %ld\n", unit,
		sc->watchdog_events);

	printf("%s: XON Rcvd = %lld\n", unit,
		(long long)sc->stats.xonrxc);
	printf("%s: XON Xmtd = %lld\n", unit,
		(long long)sc->stats.xontxc);
	printf("%s: XOFF Rcvd = %lld\n", unit,
		(long long)sc->stats.xoffrxc);
	printf("%s: XOFF Xmtd = %lld\n", unit,
		(long long)sc->stats.xofftxc);

	printf("%s: Good Packets Rcvd = %lld\n", unit,
		(long long)sc->stats.gprc);
	printf("%s: Good Packets Xmtd = %lld\n", unit,
		(long long)sc->stats.gptc);
}
#endif
