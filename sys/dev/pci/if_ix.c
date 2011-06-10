/*	$OpenBSD: if_ix.c,v 1.52 2011/06/10 12:46:35 claudio Exp $	*/

/******************************************************************************

  Copyright (c) 2001-2008, Intel Corporation 
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

******************************************************************************/
/*$FreeBSD: src/sys/dev/ixgbe/ixgbe.c,v 1.5 2008/05/16 18:46:30 jfv Exp $*/

#include <dev/pci/if_ix.h>
#include <dev/pci/ixgbe_type.h>

/*********************************************************************
 *  Driver version
 *********************************************************************/

#define IXGBE_DRIVER_VERSION	"1.4.4"

/*********************************************************************
 *  PCI Device ID Table
 *
 *  Used by probe to select devices to load on
 *********************************************************************/

const struct pci_matchid ixgbe_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598_BX },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598AF_DUAL },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598AF },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598AT },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598AT2 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598AT_DUAL },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598EB_CX4 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598EB_CX4_DUAL },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598EB_XF_LR },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598EB_SFP },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598_SR_DUAL_EM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82598_DA_DUAL },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_KX4 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_KX4_MEZZ },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_XAUI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_COMBO_BACKPLANE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_BPLANE_FCOE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_CX4 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_T3_LOM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_SFP },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_SFP_EM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599_SFP_FCOE }
#if 0
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82599VF }
#endif
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
int	ixgbe_probe(struct device *, void *, void *);
void	ixgbe_attach(struct device *, struct device *, void *);
int	ixgbe_detach(struct device *, int);
void	ixgbe_start(struct ifnet *);
void	ixgbe_start_locked(struct tx_ring *, struct ifnet *);
int	ixgbe_ioctl(struct ifnet *, u_long, caddr_t);
void	ixgbe_watchdog(struct ifnet *);
void	ixgbe_init(void *);
void	ixgbe_stop(void *);
void	ixgbe_media_status(struct ifnet *, struct ifmediareq *);
int	ixgbe_media_change(struct ifnet *);
void	ixgbe_identify_hardware(struct ix_softc *);
int	ixgbe_allocate_pci_resources(struct ix_softc *);
int	ixgbe_allocate_legacy(struct ix_softc *);
int	ixgbe_allocate_queues(struct ix_softc *);
void	ixgbe_free_pci_resources(struct ix_softc *);
void	ixgbe_local_timer(void *);
void	ixgbe_setup_interface(struct ix_softc *);
void	ixgbe_config_link(struct ix_softc *sc);

int	ixgbe_allocate_transmit_buffers(struct tx_ring *);
int	ixgbe_setup_transmit_structures(struct ix_softc *);
int	ixgbe_setup_transmit_ring(struct tx_ring *);
void	ixgbe_initialize_transmit_units(struct ix_softc *);
void	ixgbe_free_transmit_structures(struct ix_softc *);
void	ixgbe_free_transmit_buffers(struct tx_ring *);

int	ixgbe_allocate_receive_buffers(struct rx_ring *);
int	ixgbe_setup_receive_structures(struct ix_softc *);
int	ixgbe_setup_receive_ring(struct rx_ring *);
void	ixgbe_initialize_receive_units(struct ix_softc *);
void	ixgbe_free_receive_structures(struct ix_softc *);
void	ixgbe_free_receive_buffers(struct rx_ring *);
int	ixgbe_rxfill(struct rx_ring *);

void	ixgbe_enable_intr(struct ix_softc *);
void	ixgbe_disable_intr(struct ix_softc *);
void	ixgbe_update_stats_counters(struct ix_softc *);
int	ixgbe_txeof(struct tx_ring *);
int	ixgbe_rxeof(struct ix_queue *, int);
void	ixgbe_rx_checksum(uint32_t, struct mbuf *, uint32_t);
void	ixgbe_set_promisc(struct ix_softc *);
void	ixgbe_disable_promisc(struct ix_softc *);
void	ixgbe_set_multi(struct ix_softc *);
#ifdef IX_DEBUG
void	ixgbe_print_hw_stats(struct ix_softc *);
#endif
void	ixgbe_update_link_status(struct ix_softc *);
int	ixgbe_get_buf(struct rx_ring *, int);
int	ixgbe_encap(struct tx_ring *, struct mbuf *);
int	ixgbe_dma_malloc(struct ix_softc *, bus_size_t,
		    struct ixgbe_dma_alloc *, int);
void	ixgbe_dma_free(struct ix_softc *, struct ixgbe_dma_alloc *);
int	ixgbe_tx_ctx_setup(struct tx_ring *, struct mbuf *);
int	ixgbe_tso_setup(struct tx_ring *, struct mbuf *, uint32_t *);
void	ixgbe_set_ivar(struct ix_softc *, uint8_t, uint8_t, int8_t);
void	ixgbe_configure_ivars(struct ix_softc *);
uint8_t	*ixgbe_mc_array_itr(struct ixgbe_hw *, uint8_t **, uint32_t *);

void	ixgbe_setup_vlan_hw_support(struct ix_softc *);

/* Support for pluggable optic modules */
int	ixgbe_sfp_probe(struct ix_softc *);
void	ixgbe_setup_optics(struct ix_softc *);

/* Legacy (single vector interrupt handler */
int	ixgbe_legacy_irq(void *);
void	ixgbe_enable_queue(struct ix_softc *, uint32_t);
void	ixgbe_disable_queue(struct ix_softc *, uint32_t);
void	ixgbe_rearm_queue(struct ix_softc *, uint32_t);
void	ixgbe_handle_que(void *, int);

/*********************************************************************
 *  OpenBSD Device Interface Entry Points
 *********************************************************************/

struct cfdriver ix_cd = {
	NULL, "ix", DV_IFNET
};

struct cfattach ix_ca = {
	sizeof(struct ix_softc), ixgbe_probe, ixgbe_attach, ixgbe_detach
};

int ixgbe_smart_speed = ixgbe_smart_speed_on;

/*********************************************************************
 *  Device identification routine
 *
 *  ixgbe_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

int
ixgbe_probe(struct device *parent, void *match, void *aux)
{
	INIT_DEBUGOUT("ixgbe_probe: begin");

	return (pci_matchbyid((struct pci_attach_args *)aux, ixgbe_devices,
	    nitems(ixgbe_devices)));
}

/*********************************************************************
 *  Device initialization routine
 *
 *  The attach entry point is called when the driver is being loaded.
 *  This routine identifies the type of hardware, allocates all resources
 *  and initializes the hardware.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

void
ixgbe_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args	*pa = (struct pci_attach_args *)aux;
	struct ix_softc		*sc = (struct ix_softc *)self;
	int			 error = 0;
	uint16_t		 csum;
	uint32_t			 ctrl_ext;
	struct ixgbe_hw		*hw = &sc->hw;

	INIT_DEBUGOUT("ixgbe_attach: begin");

	sc->osdep.os_sc = sc;
	sc->osdep.os_pa = pa;

	/* Core Lock Init*/
	mtx_init(&sc->core_mtx, IPL_NET);

	/* Set up the timer callout */
	timeout_set(&sc->timer, ixgbe_local_timer, sc);

	/* Determine hardware revision */
	ixgbe_identify_hardware(sc);

	/* Indicate to RX setup to use Jumbo Clusters */
	sc->num_tx_desc = DEFAULT_TXD;
	sc->num_rx_desc = DEFAULT_RXD;
	sc->rx_process_limit = 100;	// XXX

	/* Do base PCI setup - map BAR0 */
	if (ixgbe_allocate_pci_resources(sc))
		goto err_out;

	/* Allocate our TX/RX Queues */
	if (ixgbe_allocate_queues(sc))
		goto err_out;

	/* Allocate multicast array memory. */
	sc->mta = malloc(sizeof(uint8_t) * IXGBE_ETH_LENGTH_OF_ADDRESS *
	    MAX_NUM_MULTICAST_ADDRESSES, M_DEVBUF, M_NOWAIT);
	if (sc->mta == 0) {
		printf(": Can not allocate multicast setup array\n");
		goto err_late;
	}

	/* Initialize the shared code */
	switch (hw->mac.type) {
	case ixgbe_mac_82598EB:
		error = ixgbe_init_ops_82598(hw);
		break;
	case ixgbe_mac_82599EB:
		error = ixgbe_init_ops_82599(hw);
		break;
#if 0
	case ixgbe_mac_82599_vf:
		error = ixgbe_init_ops_vf(hw);
		break;
#endif
	default:
		error = IXGBE_ERR_DEVICE_NOT_SUPPORTED;
		break;
	}
	if (error == IXGBE_ERR_SFP_NOT_PRESENT) {
		/*
		 * No optics in this port, set up
		 * so the timer routine will probe 
		 * for later insertion.
		 */
		sc->sfp_probe = TRUE;
		error = 0;
	} else if (error == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		printf(": Unsupported SFP+ module detected!\n");
		goto err_late;
	} else if (error) {
		printf(": Unable to initialize the shared code\n");
		goto err_late;
	}

	/* Make sure we have a good EEPROM before we read from it */
	if (sc->hw.eeprom.ops.validate_checksum(&sc->hw, &csum) < 0) {
		printf(": The EEPROM Checksum Is Not Valid\n");
		goto err_late;
	}

	/* Get Hardware Flow Control setting */
	hw->fc.requested_mode = ixgbe_fc_full;
	hw->fc.pause_time = IXGBE_FC_PAUSE;
	hw->fc.low_water = IXGBE_FC_LO;
	hw->fc.high_water = IXGBE_FC_HI;
	hw->fc.send_xon = TRUE;

	error = sc->hw.mac.ops.init_hw(hw);
	if (error == IXGBE_ERR_EEPROM_VERSION) {
		printf(": This device is a pre-production adapter/"
		    "LOM.  Please be aware there may be issues associated "
		    "with your hardware.\n If you are experiencing problems "
		    "please contact your Intel or hardware representative "
		    "who provided you with this hardware.\n");
	} else if (error == IXGBE_ERR_SFP_NOT_SUPPORTED) {
		printf("Unsupported SFP+ Module\n");
	}

	if (error) {
		printf(": Hardware Initialization Failure\n");
		goto err_late;
	}

	bcopy(sc->hw.mac.addr, sc->arpcom.ac_enaddr,
	    IXGBE_ETH_LENGTH_OF_ADDRESS);

	/* XXX sc->msix > 1 && ixgbe_allocate_msix() */
	error = ixgbe_allocate_legacy(sc); 
	if (error) 
		goto err_late;

	/* Setup OS specific network interface */
	ixgbe_setup_interface(sc);

	/* Initialize statistics */
	ixgbe_update_stats_counters(sc);

	/* Print PCIE bus type/speed/width info */
	hw->mac.ops.get_bus_info(hw);

	/* let hardware know driver is loaded */
	ctrl_ext = IXGBE_READ_REG(&sc->hw, IXGBE_CTRL_EXT);
	ctrl_ext |= IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&sc->hw, IXGBE_CTRL_EXT, ctrl_ext);

	printf(", address %s\n", ether_sprintf(sc->hw.mac.addr));

	INIT_DEBUGOUT("ixgbe_attach: end");
	return;

err_late:
	ixgbe_free_transmit_structures(sc);
	ixgbe_free_receive_structures(sc);
err_out:
	ixgbe_free_pci_resources(sc);
	free(sc->mta, M_DEVBUF);
}

/*********************************************************************
 *  Device removal routine
 *
 *  The detach entry point is called when the driver is being removed.
 *  This routine stops the adapter and deallocates all the resources
 *  that were allocated for driver operation.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

int
ixgbe_detach(struct device *self, int flags)
{
	struct ix_softc *sc = (struct ix_softc *)self;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	uint32_t	ctrl_ext;

	INIT_DEBUGOUT("ixgbe_detach: begin");

	ixgbe_stop(sc);

	/* let hardware know driver is unloading */
	ctrl_ext = IXGBE_READ_REG(&sc->hw, IXGBE_CTRL_EXT);
	ctrl_ext &= ~IXGBE_CTRL_EXT_DRV_LOAD;
	IXGBE_WRITE_REG(&sc->hw, IXGBE_CTRL_EXT, ctrl_ext);

	ether_ifdetach(ifp);
	if_detach(ifp);

	timeout_del(&sc->timer);
	ixgbe_free_pci_resources(sc);

	ixgbe_free_transmit_structures(sc);
	ixgbe_free_receive_structures(sc);
	free(sc->mta, M_DEVBUF);

	return (0);
}

/*********************************************************************
 *  Transmit entry point
 *
 *  ixgbe_start is called by the stack to initiate a transmit.
 *  The driver will remain in this routine as long as there are
 *  packets to transmit and transmit resources are available.
 *  In case resources are not available stack is notified and
 *  the packet is requeued.
 **********************************************************************/

void
ixgbe_start_locked(struct tx_ring *txr, struct ifnet * ifp)
{
	struct mbuf  		*m_head;
	struct ix_softc		*sc = txr->sc;
	int			 post = 0;

	if ((ifp->if_flags & (IFF_RUNNING|IFF_OACTIVE)) != IFF_RUNNING)
		return;

	if (!sc->link_active)
		return;

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map, 0,
	    txr->txdma.dma_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (ixgbe_encap(txr, m_head)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m_head);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif

		/* Set timeout in case hardware has problems transmitting */
		txr->watchdog_timer = IXGBE_TX_TIMEOUT;
		ifp->if_timer = IXGBE_TX_TIMEOUT;

		post = 1;
	}

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    0, txr->txdma.dma_map->dm_mapsize, 
            BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the
	 * hardware that this frame is available to transmit.
	 */
	if (post)
		IXGBE_WRITE_REG(&sc->hw, IXGBE_TDT(txr->me),
		    txr->next_avail_desc);
}


void
ixgbe_start(struct ifnet *ifp)
{
	struct ix_softc *sc = ifp->if_softc;
	struct tx_ring	*txr = sc->tx_rings;
	uint32_t queue = 0;

#if 0
	/*
	 * This is really just here for testing
	 * TX multiqueue, ultimately what is
	 * needed is the flow support in the stack
	 * and appropriate logic here to deal with
	 * it. -jfv
	 */
	if (sc->num_queues > 1)
		queue = (curcpu % sc->num_queues);
#endif

	txr = &sc->tx_rings[queue];

	if (ifp->if_flags & IFF_RUNNING)
		ixgbe_start_locked(txr, ifp);

	return;
}

/*********************************************************************
 *  Ioctl entry point
 *
 *  ixgbe_ioctl is called when the user wants to configure the
 *  interface.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

int
ixgbe_ioctl(struct ifnet * ifp, u_long command, caddr_t data)
{
	struct ix_softc	*sc = ifp->if_softc;
	struct ifaddr	*ifa = (struct ifaddr *) data;
	struct ifreq	*ifr = (struct ifreq *) data;
	int		s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		IOCTL_DEBUGOUT("ioctl: SIOCxIFADDR (Get/Set Interface Addr)");
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			ixgbe_init(sc);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->arpcom, ifa);
#endif
		break;

	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ifp->if_hardmtu)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
			sc->max_frame_size =
				ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
			ixgbe_init(sc);
		}
		break;

	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl: SIOCSIFFLAGS (Set Interface Flags)");
		if (ifp->if_flags & IFF_UP) {
			if ((ifp->if_flags & IFF_RUNNING)) {
				if ((ifp->if_flags ^ sc->if_flags) &
				    (IFF_PROMISC | IFF_ALLMULTI)) {
					ixgbe_disable_promisc(sc);
					ixgbe_set_promisc(sc);
                                }
			} else
				ixgbe_init(sc);
		} else
			if (ifp->if_flags & IFF_RUNNING)
				ixgbe_stop(sc);
		sc->if_flags = ifp->if_flags;
		break;

	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;

	default:
		error = ether_ioctl(ifp, &sc->arpcom, command, data);
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING) {
			ixgbe_disable_intr(sc);
			ixgbe_set_multi(sc);
			ixgbe_enable_intr(sc);
		}
		error = 0;
	}

	splx(s);
	return (error);
}

/*********************************************************************
 *  Watchdog entry point
 *
 *  This routine is called by the local timer
 *  to detect hardware hangs .
 *
 **********************************************************************/

void
ixgbe_watchdog(struct ifnet * ifp)
{
	struct ix_softc *sc = (struct ix_softc *)ifp->if_softc;
	struct tx_ring *txr = sc->tx_rings;
	struct ixgbe_hw *hw = &sc->hw;
	int		tx_hang = FALSE;
	int		i;

        /*
         * The timer is set to 5 every time ixgbe_start() queues a packet.
         * Then ixgbe_txeof() keeps resetting to 5 as long as it cleans at
         * least one descriptor.
         * Finally, anytime all descriptors are clean the timer is
         * set to 0.
         */
	for (i = 0; i < sc->num_queues; i++, txr++) {
        	if (txr->watchdog_timer == 0 || --txr->watchdog_timer)
                	continue;
		else {
			tx_hang = TRUE;
			break;
		}
	}
	if (tx_hang == FALSE)
		return;

	/*
	 * If we are in this routine because of pause frames, then don't
	 * reset the hardware.
	 */
	if (IXGBE_READ_REG(hw, IXGBE_TFCS) & IXGBE_TFCS_TXOFF) {
		for (i = 0; i < sc->num_queues; i++, txr++)
			txr->watchdog_timer = IXGBE_TX_TIMEOUT;
		ifp->if_timer = IXGBE_TX_TIMEOUT;
		return;
	}


	printf("%s: Watchdog timeout -- resetting\n", ifp->if_xname);
	for (i = 0; i < sc->num_queues; i++, txr++) {
		printf("%s: Queue(%d) tdh = %d, hw tdt = %d\n", ifp->if_xname, i,
		    IXGBE_READ_REG(hw, IXGBE_TDH(i)),
		    IXGBE_READ_REG(hw, IXGBE_TDT(i)));
		printf("%s: TX(%d) desc avail = %d, Next TX to Clean = %d\n", ifp->if_xname,
		    i, txr->tx_avail, txr->next_to_clean);
	}
	ifp->if_flags &= ~IFF_RUNNING;
	sc->watchdog_events++;

	ixgbe_init(sc);
	return;
}

/*********************************************************************
 *  Init entry point
 *
 *  This routine is used in two ways. It is used by the stack as
 *  init entry point in network interface structure. It is also used
 *  by the driver as a hw/sw initialization routine to get to a
 *  consistent state.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/
#define IXGBE_MHADD_MFS_SHIFT 16

void
ixgbe_init(void *arg)
{
	struct ix_softc	*sc = (struct ix_softc *)arg;
	struct ifnet	*ifp = &sc->arpcom.ac_if;
	struct rx_ring	*rxr = sc->rx_rings;
	uint32_t	 k, txdctl, rxdctl, rxctrl, mhadd, gpie;
	int		 err;
	int		 i, s;

	INIT_DEBUGOUT("ixgbe_init: begin");

	s = splnet();

	ixgbe_stop(sc);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_hw(&sc->hw, set_rar, 0, sc->hw.mac.addr, 0, IXGBE_RAH_AV);

	/* Get the latest mac address, User can use a LAA */
	bcopy(sc->arpcom.ac_enaddr, sc->hw.mac.addr,
	      IXGBE_ETH_LENGTH_OF_ADDRESS);
	ixgbe_hw(&sc->hw, set_rar, 0, sc->hw.mac.addr, 0, 1);
	sc->hw.addr_ctrl.rar_used_count = 1;

	/* Prepare transmit descriptors and buffers */
	if (ixgbe_setup_transmit_structures(sc)) {
		printf("%s: Could not setup transmit structures\n",
		    ifp->if_xname);
		ixgbe_stop(sc);
		splx(s);
		return;
	}

	ixgbe_hw0(&sc->hw, init_hw);
	ixgbe_initialize_transmit_units(sc);

	/* Setup Multicast table */
	ixgbe_set_multi(sc);

	/* Determine the correct buffer size for jumbo/headersplit */
	if (sc->max_frame_size <= 2048)
		sc->rx_mbuf_sz = MCLBYTES;
	else if (sc->max_frame_size <= 4096)
		sc->rx_mbuf_sz = 4096;
	else if (sc->max_frame_size <= 9216)
		sc->rx_mbuf_sz = 9216;
	else
		sc->rx_mbuf_sz = 16 * 1024;

	/* Prepare receive descriptors and buffers */
	if (ixgbe_setup_receive_structures(sc)) {
		printf("%s: Could not setup receive structures\n",
		    ifp->if_xname);
		ixgbe_stop(sc);
		splx(s);
		return;
	}

	/* Configure RX settings */
	ixgbe_initialize_receive_units(sc);

	gpie = IXGBE_READ_REG(&sc->hw, IXGBE_GPIE);

	/* Enable Fan Failure Interrupt */
	gpie |= IXGBE_SDP1_GPIEN;

	/* Add for Thermal detection */
	if (sc->hw.mac.type == ixgbe_mac_82599EB)
		gpie |= IXGBE_SDP2_GPIEN;

	if (sc->msix > 1) {
		/* Enable Enhanced MSIX mode */
		gpie |= IXGBE_GPIE_MSIX_MODE;
		gpie |= IXGBE_GPIE_EIAME | IXGBE_GPIE_PBA_SUPPORT |
		    IXGBE_GPIE_OCD;
	}
	IXGBE_WRITE_REG(&sc->hw, IXGBE_GPIE, gpie);

	/* Set MTU size */
	if (ifp->if_mtu > ETHERMTU) {
		mhadd = IXGBE_READ_REG(&sc->hw, IXGBE_MHADD);
		mhadd &= ~IXGBE_MHADD_MFS_MASK;
		mhadd |= sc->max_frame_size << IXGBE_MHADD_MFS_SHIFT;
		IXGBE_WRITE_REG(&sc->hw, IXGBE_MHADD, mhadd);
	}
	
	/* Now enable all the queues */

	for (i = 0; i < sc->num_queues; i++) {
		txdctl = IXGBE_READ_REG(&sc->hw, IXGBE_TXDCTL(i));
		txdctl |= IXGBE_TXDCTL_ENABLE;
		/* Set WTHRESH to 8, burst writeback */
		txdctl |= (8 << 16);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_TXDCTL(i), txdctl);
	}

	for (i = 0; i < sc->num_queues; i++) {
		rxdctl = IXGBE_READ_REG(&sc->hw, IXGBE_RXDCTL(i));
		if (sc->hw.mac.type == ixgbe_mac_82598EB) {
			/*
			 * PTHRESH = 21
			 * HTHRESH = 4
			 * WTHRESH = 8
			 */
			rxdctl &= ~0x3FFFFF;
			rxdctl |= 0x080420;
		}
		rxdctl |= IXGBE_RXDCTL_ENABLE;
		IXGBE_WRITE_REG(&sc->hw, IXGBE_RXDCTL(i), rxdctl);
		for (k = 0; k < 10; k++) {
			if (IXGBE_READ_REG(&sc->hw, IXGBE_RXDCTL(i)) &
			    IXGBE_RXDCTL_ENABLE)
				break;
			else
				msec_delay(1);
		}
		/* XXX wmb() : memory barrier */
		IXGBE_WRITE_REG(&sc->hw, IXGBE_RDT(i), rxr->last_desc_filled);
	}

	/* Set up VLAN support and filter */
	ixgbe_setup_vlan_hw_support(sc);

	/* Enable Receive engine */
	rxctrl = IXGBE_READ_REG(&sc->hw, IXGBE_RXCTRL);
	if (sc->hw.mac.type == ixgbe_mac_82598EB)
		rxctrl |= IXGBE_RXCTRL_DMBYPS;
	rxctrl |= IXGBE_RXCTRL_RXEN;
	ixgbe_hw(&sc->hw, enable_rx_dma, rxctrl);

	timeout_add_sec(&sc->timer, 1);

#ifdef MSI
	/* Set up MSI/X routing */
	if (ixgbe_enable_msix) {
		ixgbe_configure_ivars(sc);
		/* Set up auto-mask */
		if (sc->hw.mac.type == ixgbe_mac_82598EB)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
		else {
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EIAM_EX(0), 0xFFFFFFFF);
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EIAM_EX(1), 0xFFFFFFFF);
		}
	} else  /* Simple settings for Legacy/MSI */
#else
	{
		ixgbe_set_ivar(sc, 0, 0, 0);
		ixgbe_set_ivar(sc, 0, 0, 1);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIAM, IXGBE_EICS_RTX_QUEUE);
	}
#endif

#ifdef IXGBE_FDIR
	/* Init Flow director */
	if (sc->hw.mac.type != ixgbe_mac_82598EB)
		ixgbe_init_fdir_signature_82599(&sc->hw, fdir_pballoc);
#endif

	/*
	 * Check on any SFP devices that
	 * need to be kick-started
	 */
	if (sc->hw.phy.type == ixgbe_phy_none) {
		err = sc->hw.phy.ops.identify(&sc->hw);
		if (err == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			printf("Unsupported SFP+ module type was detected.\n");
			splx(s);
			return;
		}
	}

	/* Set moderation on the Link interrupt */
	IXGBE_WRITE_REG(&sc->hw, IXGBE_EITR(sc->linkvec), IXGBE_LINK_ITR);

	/* Config/Enable Link */
	ixgbe_config_link(sc);

	/* And now turn on interrupts */
	ixgbe_enable_intr(sc);

	/* Now inform the stack we're ready */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);
}

/*
 * MSIX Interrupt Handlers
 */
void
ixgbe_enable_queue(struct ix_softc *sc, uint32_t vector)
{
	uint64_t queue = 1ULL << vector;
	uint32_t mask;

	if (sc->hw.mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & queue);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMS, mask);
	} else {
		mask = (queue & 0xFFFFFFFF);
		if (mask)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMS_EX(0), mask);
		mask = (queue >> 32);
		if (mask)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMS_EX(1), mask);
	}
}

void
ixgbe_disable_queue(struct ix_softc *sc, uint32_t vector)
{
	uint64_t queue = 1ULL << vector;
	uint32_t mask;
	
	if (sc->hw.mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & queue);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMC, mask);
	} else {
		mask = (queue & 0xFFFFFFFF);
		if (mask)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMC_EX(0), mask);
		mask = (queue >> 32);
		if (mask)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMC_EX(1), mask);
	}
}

void    
ixgbe_rearm_queue(struct ix_softc *sc, uint32_t vector)
{
        uint64_t queue = 1ULL << vector;
	uint32_t mask;

	if (sc->hw.mac.type == ixgbe_mac_82598EB) {
		mask = (IXGBE_EIMS_RTX_QUEUE & queue);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EICS, mask);
	} else {
		mask = (queue & 0xFFFFFFFF);
		if (mask)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EICS_EX(0), mask);
		mask = (queue >> 32);
		if (mask)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_EICS_EX(1), mask);
	}
}

void
ixgbe_handle_que(void *context, int pending)
{
        struct ix_queue *que = context;
	struct ix_softc *sc = que->sc;
	struct tx_ring	*txr = que->txr;
	struct ifnet	*ifp = &que->sc->arpcom.ac_if;

	if (ifp->if_flags & IFF_RUNNING) {
		ixgbe_rxeof(que, -1 /* XXX sc->rx_process_limit */);
		ixgbe_txeof(txr);

		if (ixgbe_rxfill(que->rxr)) {
			/* Advance the Rx Queue "Tail Pointer" */
			IXGBE_WRITE_REG(&sc->hw, IXGBE_RDT(que->rxr->me),
			    que->rxr->last_desc_filled);
		}

		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			ixgbe_start_locked(txr, ifp);
	}

	/* Reenable this interrupt */
	ixgbe_enable_queue(que->sc, que->msix);
}

/*********************************************************************
 *
 *  Legacy Interrupt Service routine
 *
 **********************************************************************/

int
ixgbe_legacy_irq(void *arg)
{
	struct ix_softc	*sc = (struct ix_softc *)arg;
	struct ix_queue *que = sc->queues;
	struct ifnet	*ifp = &sc->arpcom.ac_if;
	struct tx_ring	*txr = sc->tx_rings;
	struct ixgbe_hw	*hw = &sc->hw;
	uint32_t	 reg_eicr;
	int		 refill = 0;

	reg_eicr = IXGBE_READ_REG(&sc->hw, IXGBE_EICR);
	if (reg_eicr == 0) {
		ixgbe_enable_intr(sc);
		return (0);
	}

	++que->irqs;
	if (ifp->if_flags & IFF_RUNNING) {
		ixgbe_rxeof(que, -1);
		ixgbe_txeof(txr);
		refill = 1;
	}

	/* Check for fan failure */
	if ((hw->phy.media_type == ixgbe_media_type_copper) &&
	    (reg_eicr & IXGBE_EICR_GPI_SDP1)) {
                printf("\n%s: CRITICAL: FAN FAILURE!! "
		    "REPLACE IMMEDIATELY!!\n", ifp->if_xname);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMS,
		    IXGBE_EICR_GPI_SDP1);
	}

	/* Link status change */
	if (reg_eicr & IXGBE_EICR_LSC) {
		timeout_del(&sc->timer);
	        ixgbe_update_link_status(sc);
		timeout_add_sec(&sc->timer, 1);
	}

	if (refill && ixgbe_rxfill(que->rxr)) {
		/* Advance the Rx Queue "Tail Pointer" */
		IXGBE_WRITE_REG(&sc->hw, IXGBE_RDT(que->rxr->me),
		    que->rxr->last_desc_filled);
	}

	if (ifp->if_flags & IFF_RUNNING && !IFQ_IS_EMPTY(&ifp->if_snd))
		ixgbe_start_locked(txr, ifp);

	ixgbe_enable_intr(sc);
	return (1);
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
ixgbe_media_status(struct ifnet * ifp, struct ifmediareq * ifmr)
{
	struct ix_softc *sc = ifp->if_softc;

	ifmr->ifm_active = IFM_ETHER;
	ifmr->ifm_status = IFM_AVALID;

	INIT_DEBUGOUT("ixgbe_media_status: begin");
	ixgbe_update_link_status(sc);

	if (LINK_STATE_IS_UP(ifp->if_link_state)) {
		ifmr->ifm_status |= IFM_ACTIVE;

		switch (sc->link_speed) {
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifmr->ifm_active |= IFM_1000_T | IFM_FDX;
			break;
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifmr->ifm_active |= sc->optics | IFM_FDX;
			break;
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
ixgbe_media_change(struct ifnet * ifp)
{
	/* ignore */
	return (0);
}

/*********************************************************************
 *
 *  This routine maps the mbufs to tx descriptors.
 *    WARNING: while this code is using an MQ style infrastructure,
 *    it would NOT work as is with more than 1 queue.
 *
 *  return 0 on success, positive on failure
 **********************************************************************/

int
ixgbe_encap(struct tx_ring *txr, struct mbuf *m_head)
{
	struct ix_softc *sc = txr->sc;
	uint32_t	olinfo_status = 0, cmd_type_len = 0;
	int             i, j, error;
	int		first, last = 0;
	bus_dmamap_t	map;
	struct ixgbe_tx_buf *txbuf;
	union ixgbe_adv_tx_desc *txd = NULL;
	uint32_t	paylen = 0;

	/* Basic descriptor defines */
        cmd_type_len |= IXGBE_ADVTXD_DTYP_DATA;
        cmd_type_len |= IXGBE_ADVTXD_DCMD_IFCS | IXGBE_ADVTXD_DCMD_DEXT;

#if NVLAN > 0
	if (m_head->m_flags & M_VLANTAG)
		cmd_type_len |= IXGBE_ADVTXD_DCMD_VLE;
#endif

#if 0
	/*
	 * Force a cleanup if number of TX descriptors
	 * available is below the threshold. If it fails
	 * to get above, then abort transmit.
	 */
	if (txr->tx_avail <= IXGBE_TX_CLEANUP_THRESHOLD) {
		ixgbe_txeof(txr);
		/* Make sure things have improved */
		if (txr->tx_avail <= IXGBE_TX_OP_THRESHOLD) {
			txr->no_desc_avail++;
			return (ENOBUFS);
		}
	}
#endif

        /*
         * Important to capture the first descriptor
         * used because it will contain the index of
         * the one we tell the hardware to report back
         */
        first = txr->next_avail_desc;
	txbuf = &txr->tx_buffers[first];
	map = txbuf->map;

	/*
	 * Map the packet for DMA.
	 */
	error = bus_dmamap_load_mbuf(txr->txdma.dma_tag, map,
	    m_head, BUS_DMA_NOWAIT);
	/* XXX EFBIG */
	if (error == ENOMEM) {
		sc->no_tx_dma_setup++;
		return (error);
	} else if (error != 0) {
		sc->no_tx_dma_setup++;
		return (error);
	}

	/* Make certain there are enough descriptors */
	if (map->dm_nsegs > txr->tx_avail - 2) {
		txr->no_desc_avail++;
		error = ENOBUFS;
		goto xmit_fail;
	}

	/*
	 * Set the appropriate offload context
	 * this becomes the first descriptor of 
	 * a packet.
	 */
#ifdef notyet
	if (ixgbe_tso_setup(txr, m_head, &paylen)) {
		cmd_type_len |= IXGBE_ADVTXD_DCMD_TSE;
		olinfo_status |= IXGBE_TXD_POPTS_IXSM << 8;
		olinfo_status |= IXGBE_TXD_POPTS_TXSM << 8;
		olinfo_status |= paylen << IXGBE_ADVTXD_PAYLEN_SHIFT;
		++sc->tso_tx;
	} else
#endif
	if (ixgbe_tx_ctx_setup(txr, m_head))
		olinfo_status |= IXGBE_TXD_POPTS_IXSM << 8;

	/* Record payload length */
	if (paylen == 0)
		olinfo_status |= m_head->m_pkthdr.len <<
		    IXGBE_ADVTXD_PAYLEN_SHIFT;

	i = txr->next_avail_desc;
	for (j = 0; j < map->dm_nsegs; j++) {
		txbuf = &txr->tx_buffers[i];
		txd = &txr->tx_base[i];

		txd->read.buffer_addr = htole64(map->dm_segs[j].ds_addr);
		txd->read.cmd_type_len = htole32(txr->txd_cmd |
		    cmd_type_len | map->dm_segs[j].ds_len);
		txd->read.olinfo_status = htole32(olinfo_status);
		last = i; /* descriptor that will get completion IRQ */

		if (++i == sc->num_tx_desc)
			i = 0;

		txbuf->m_head = NULL;
		txbuf->eop_index = -1;
	}

	txd->read.cmd_type_len |=
	    htole32(IXGBE_TXD_CMD_EOP | IXGBE_TXD_CMD_RS);
	txr->tx_avail -= map->dm_nsegs;
	txr->next_avail_desc = i;

	txbuf->m_head = m_head;
	/* swap maps because last tx descriptor is tracking all the data */
	txr->tx_buffers[first].map = txbuf->map;
	txbuf->map = map;
	bus_dmamap_sync(txr->txdma.dma_tag, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

        /* Set the index of the descriptor that will be marked done */
        txbuf = &txr->tx_buffers[first];
	txbuf->eop_index = last;

	++txr->tx_packets;
	return (0);

xmit_fail:
	bus_dmamap_unload(txr->txdma.dma_tag, txbuf->map);
	return (error);

}

void
ixgbe_set_promisc(struct ix_softc *sc)
{

	uint32_t       reg_rctl;
	struct ifnet *ifp = &sc->arpcom.ac_if;

	reg_rctl = IXGBE_READ_REG(&sc->hw, IXGBE_FCTRL);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_FCTRL, reg_rctl);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= IXGBE_FCTRL_MPE;
		reg_rctl &= ~IXGBE_FCTRL_UPE;
		IXGBE_WRITE_REG(&sc->hw, IXGBE_FCTRL, reg_rctl);
	}
	return;
}

void
ixgbe_disable_promisc(struct ix_softc * sc)
{
	uint32_t       reg_rctl;

	reg_rctl = IXGBE_READ_REG(&sc->hw, IXGBE_FCTRL);
	reg_rctl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	IXGBE_WRITE_REG(&sc->hw, IXGBE_FCTRL, reg_rctl);

	return;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/
void
ixgbe_set_multi(struct ix_softc *sc)
{
	uint32_t	fctrl;
	uint8_t	*mta;
	uint8_t	*update_ptr;
	struct ether_multi *enm;
	struct ether_multistep step;
	int	mcnt = 0;
	struct ifnet *ifp = &sc->arpcom.ac_if;

	IOCTL_DEBUGOUT("ixgbe_set_multi: begin");

	mta = sc->mta;
	bzero(mta, sizeof(uint8_t) * IXGBE_ETH_LENGTH_OF_ADDRESS *
	    MAX_NUM_MULTICAST_ADDRESSES);

	fctrl = IXGBE_READ_REG(&sc->hw, IXGBE_FCTRL);
	fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	if (ifp->if_flags & IFF_PROMISC)
		fctrl |= (IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	else if (ifp->if_flags & IFF_ALLMULTI) {
		fctrl |= IXGBE_FCTRL_MPE;
		fctrl &= ~IXGBE_FCTRL_UPE;
	} else
		fctrl &= ~(IXGBE_FCTRL_UPE | IXGBE_FCTRL_MPE);
	
	IXGBE_WRITE_REG(&sc->hw, IXGBE_FCTRL, fctrl);

	ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			mcnt = MAX_NUM_MULTICAST_ADDRESSES;
		}
		if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
			break;
		bcopy(enm->enm_addrlo,
		    &mta[mcnt * IXGBE_ETH_LENGTH_OF_ADDRESS],
		    IXGBE_ETH_LENGTH_OF_ADDRESS);
		mcnt++;
		ETHER_NEXT_MULTI(step, enm);
	}

	update_ptr = mta;
	ixgbe_hw(&sc->hw, update_mc_addr_list,
	    update_ptr, mcnt, ixgbe_mc_array_itr);

	return;
}

/*
 * This is an iterator function now needed by the multicast
 * shared code. It simply feeds the shared code routine the
 * addresses in the array of ixgbe_set_multi() one by one.
 */
uint8_t *
ixgbe_mc_array_itr(struct ixgbe_hw *hw, uint8_t **update_ptr, uint32_t *vmdq)
{
	uint8_t *addr = *update_ptr;
	uint8_t *newptr;
	*vmdq = 0;

	newptr = addr + IXGBE_ETH_LENGTH_OF_ADDRESS;
	*update_ptr = newptr;
	return addr;
}


/*********************************************************************
 *  Timer routine
 *
 *  This routine checks for link status,updates statistics,
 *  and runs the watchdog timer.
 *
 **********************************************************************/

void
ixgbe_local_timer(void *arg)
{
	struct ix_softc *sc = arg;
#ifdef IX_DEBUG
	struct ifnet	*ifp = &sc->arpcom.ac_if;
#endif
	int		 s;
	
	s = splnet();

	/* Check for pluggable optics */
	if (sc->sfp_probe)
		if (!ixgbe_sfp_probe(sc))
			goto out; /* Nothing to do */

	ixgbe_update_link_status(sc);
	ixgbe_update_stats_counters(sc);

out:
#ifdef IX_DEBUG
	if ((ifp->if_flags & (IFF_RUNNING|IFF_DEBUG)) ==
	    (IFF_RUNNING|IFF_DEBUG))
		ixgbe_print_hw_stats(sc);
#endif
	timeout_add_sec(&sc->timer, 1);

	splx(s);
}

void
ixgbe_update_link_status(struct ix_softc *sc)
{
	int link_up = FALSE;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct tx_ring *txr = sc->tx_rings;
	int		link_state;
	int		i;

	ixgbe_hw(&sc->hw, check_link, &sc->link_speed, &link_up, 0);

	link_state = link_up ? LINK_STATE_FULL_DUPLEX : LINK_STATE_DOWN;

	if (ifp->if_link_state != link_state) {
		sc->link_active = link_up;
		ifp->if_link_state = link_state;
		if_link_state_change(ifp);
	}

	if (LINK_STATE_IS_UP(ifp->if_link_state)) {
		switch (sc->link_speed) {
		case IXGBE_LINK_SPEED_UNKNOWN:
			ifp->if_baudrate = 0;
			break;
		case IXGBE_LINK_SPEED_100_FULL:
			ifp->if_baudrate = IF_Mbps(100);
			break;
		case IXGBE_LINK_SPEED_1GB_FULL:
			ifp->if_baudrate = IF_Gbps(1);
			break;
		case IXGBE_LINK_SPEED_10GB_FULL:
			ifp->if_baudrate = IF_Gbps(10);
			break;
		}
	} else {
		ifp->if_baudrate = 0;
		ifp->if_timer = 0;
		for (i = 0; i < sc->num_queues; i++)
			txr[i].watchdog_timer = FALSE;
	}


	return;
}



/*********************************************************************
 *
 *  This routine disables all traffic on the sc by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers.
 *
 **********************************************************************/

void
ixgbe_stop(void *arg)
{
	struct ix_softc *sc = arg;
	struct ifnet   *ifp = &sc->arpcom.ac_if;

	/* Tell the stack that the interface is no longer active */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	INIT_DEBUGOUT("ixgbe_stop: begin\n");
	ixgbe_disable_intr(sc);

	ixgbe_hw0(&sc->hw, reset_hw);
	sc->hw.adapter_stopped = FALSE;
	ixgbe_hw0(&sc->hw, stop_adapter);
	/* Turn off the laser */
	if (sc->hw.phy.multispeed_fiber)
		ixgbe_hw0(&sc->hw, disable_tx_laser);
	timeout_del(&sc->timer);

	/* reprogram the RAR[0] in case user changed it. */
	ixgbe_hw(&sc->hw, set_rar, 0, sc->hw.mac.addr, 0, IXGBE_RAH_AV);

	/* Should we really clear all structures on stop? */
	ixgbe_free_transmit_structures(sc);
	ixgbe_free_receive_structures(sc);
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
void
ixgbe_identify_hardware(struct ix_softc *sc)
{
	struct ixgbe_osdep	*os = &sc->osdep;
	struct pci_attach_args	*pa = os->os_pa;
	uint32_t		 reg;

	/* Save off the information about this board */
	sc->hw.vendor_id = PCI_VENDOR(pa->pa_id);
	sc->hw.device_id = PCI_PRODUCT(pa->pa_id);

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_CLASS_REG);
	sc->hw.revision_id = PCI_REVISION(reg);

	reg = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_SUBSYS_ID_REG);
	sc->hw.subsystem_vendor_id = PCI_VENDOR(reg);
	sc->hw.subsystem_device_id = PCI_PRODUCT(reg);

	switch (sc->hw.device_id) {
	case PCI_PRODUCT_INTEL_82598:
	case PCI_PRODUCT_INTEL_82598AF_DUAL:
	case PCI_PRODUCT_INTEL_82598_DA_DUAL:
	case PCI_PRODUCT_INTEL_82598AF:
	case PCI_PRODUCT_INTEL_82598_SR_DUAL_EM:
	case PCI_PRODUCT_INTEL_82598EB_SFP:
		sc->hw.mac.type = ixgbe_mac_82598EB;
		sc->optics = IFM_10G_SR;
		break;
	case PCI_PRODUCT_INTEL_82598EB_CX4_DUAL:
	case PCI_PRODUCT_INTEL_82598EB_CX4:
		sc->hw.mac.type = ixgbe_mac_82598EB;
		sc->optics = IFM_10G_CX4;
		break;
	case PCI_PRODUCT_INTEL_82598EB_XF_LR:
		sc->hw.mac.type = ixgbe_mac_82598EB;
		sc->optics = IFM_10G_LR;
		break;
	case PCI_PRODUCT_INTEL_82598AT:
	case PCI_PRODUCT_INTEL_82598AT2:
	case PCI_PRODUCT_INTEL_82598AT_DUAL:
		sc->hw.mac.type = ixgbe_mac_82598EB;
		sc->optics = IFM_10G_T;
		break;
	case PCI_PRODUCT_INTEL_82598_BX:
		sc->hw.mac.type = ixgbe_mac_82598EB;
		sc->optics = IFM_AUTO;
		break;
	case PCI_PRODUCT_INTEL_82599_SFP:
	case PCI_PRODUCT_INTEL_82599_SFP_EM:
	case PCI_PRODUCT_INTEL_82599_SFP_FCOE:
		sc->hw.mac.type = ixgbe_mac_82599EB;
		sc->optics = IFM_10G_SR;
		sc->hw.phy.smart_speed = ixgbe_smart_speed;
		break;
	case PCI_PRODUCT_INTEL_82599_KX4:
	case PCI_PRODUCT_INTEL_82599_KX4_MEZZ:
	case PCI_PRODUCT_INTEL_82599_CX4:
		sc->hw.mac.type = ixgbe_mac_82599EB;
		sc->optics = IFM_10G_CX4;
		sc->hw.phy.smart_speed = ixgbe_smart_speed;
		break;
	case PCI_PRODUCT_INTEL_82599_T3_LOM:
		sc->hw.mac.type = ixgbe_mac_82599EB;
		sc->optics = IFM_10G_T;
		sc->hw.phy.smart_speed = ixgbe_smart_speed;
		break;
	case PCI_PRODUCT_INTEL_82599_XAUI:
	case PCI_PRODUCT_INTEL_82599_COMBO_BACKPLANE:
	case PCI_PRODUCT_INTEL_82599_BPLANE_FCOE:
		sc->hw.mac.type = ixgbe_mac_82599EB;
		sc->optics = IFM_AUTO;
		sc->hw.phy.smart_speed = ixgbe_smart_speed;
		break;
	case PCI_PRODUCT_INTEL_82599VF:
		sc->hw.mac.type = ixgbe_mac_82599_vf;
		sc->optics = IFM_AUTO;
		sc->hw.phy.smart_speed = ixgbe_smart_speed;
		break;
	default:
		sc->optics = IFM_AUTO;
		break;
	}
}

/*********************************************************************
 *
 *  Determine optic type
 *
 **********************************************************************/
void
ixgbe_setup_optics(struct ix_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;
	int		layer;
	
	layer = ixgbe_hw(hw, get_supported_physical_layer);
	switch (layer) {
		case IXGBE_PHYSICAL_LAYER_10GBASE_T:
			sc->optics = IFM_10G_T;
			break;
		case IXGBE_PHYSICAL_LAYER_1000BASE_T:
			sc->optics = IFM_1000_T;
			break;
		case IXGBE_PHYSICAL_LAYER_10GBASE_LR:
		case IXGBE_PHYSICAL_LAYER_10GBASE_LRM:
			sc->optics = IFM_10G_LR;
			break;
		case IXGBE_PHYSICAL_LAYER_10GBASE_SR:
			sc->optics = IFM_10G_SR;
			break;
		case IXGBE_PHYSICAL_LAYER_10GBASE_KX4:
		case IXGBE_PHYSICAL_LAYER_10GBASE_CX4:
			sc->optics = IFM_10G_CX4;
			break;
		case IXGBE_PHYSICAL_LAYER_SFP_PLUS_CU:
			sc->optics = IFM_10G_SFP_CU;
			break;
		case IXGBE_PHYSICAL_LAYER_1000BASE_KX:
		case IXGBE_PHYSICAL_LAYER_10GBASE_KR:
		case IXGBE_PHYSICAL_LAYER_10GBASE_XAUI:
		case IXGBE_PHYSICAL_LAYER_UNKNOWN:
		default:
			sc->optics = IFM_ETHER | IFM_AUTO;
			break;
	}
	return;
}

/*********************************************************************
 *
 *  Setup the Legacy or MSI Interrupt handler
 *
 **********************************************************************/
int
ixgbe_allocate_legacy(struct ix_softc *sc)
{
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct ixgbe_osdep	*os = &sc->osdep;
	struct pci_attach_args	*pa = os->os_pa;
	const char		*intrstr = NULL;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;

	/* We allocate a single interrupt resource */
	if (/* pci_intr_map_msi(pa, &ih) != 0 && */
	    pci_intr_map(pa, &ih) != 0) {
		printf(": couldn't map interrupt\n");
		return (ENXIO);
	}

#if 0
	/* XXX */
	/* Tasklets for Link, SFP and Multispeed Fiber */
	TASK_INIT(&sc->link_task, 0, ixgbe_handle_link, sc);
	TASK_INIT(&sc->mod_task, 0, ixgbe_handle_mod, sc);
	TASK_INIT(&sc->msf_task, 0, ixgbe_handle_msf, sc);
#endif

	intrstr = pci_intr_string(pc, ih);
	sc->tag = pci_intr_establish(pc, ih, IPL_NET,
	    ixgbe_legacy_irq, sc, ifp->if_xname);
	if (sc->tag == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return (ENXIO);
	}
	printf(": %s", intrstr);

	/* For simplicity in the handlers */
	sc->que_mask = IXGBE_EIMS_ENABLE_MASK;

	return (0);
}

int
ixgbe_allocate_pci_resources(struct ix_softc *sc)
{
	struct ixgbe_osdep	*os = &sc->osdep;
	struct pci_attach_args	*pa = os->os_pa;
	int			 val;

	val = pci_conf_read(pa->pa_pc, pa->pa_tag, PCIR_BAR(0));
	if (PCI_MAPREG_TYPE(val) != PCI_MAPREG_TYPE_MEM &&
	    PCI_MAPREG_TYPE(val) != PCI_MAPREG_MEM_TYPE_64BIT) {
		printf(": mmba is not mem space\n");
		return (ENXIO);
	}

	if (pci_mapreg_map(pa, PCIR_BAR(0), PCI_MAPREG_MEM_TYPE(val), 0,
	    &os->os_memt, &os->os_memh, &os->os_membase, &os->os_memsize, 0)) {
		printf(": cannot find mem space\n");
		return (ENXIO);
	}
	sc->hw.hw_addr = (uint8_t *)os->os_membase;

	/* Legacy defaults */
	sc->num_queues = 1;
	sc->hw.back = os;

#ifdef notyet
	/* Now setup MSI or MSI/X, return us the number of supported vectors. */
	sc->msix = ixgbe_setup_msix(sc);
#endif

	return (0);
}

void
ixgbe_free_pci_resources(struct ix_softc * sc)
{
	struct ixgbe_osdep	*os = &sc->osdep;
	struct pci_attach_args	*pa = os->os_pa;
	struct ix_queue *que = sc->queues;
	int i;


	/* Release all msix queue resources: */
	for (i = 0; i < sc->num_queues; i++, que++) {
		if (que->tag)
			pci_intr_disestablish(pa->pa_pc, que->tag);
		que->tag = NULL;
	}

	if (sc->tag)
		pci_intr_disestablish(pa->pa_pc, sc->tag);
	sc->tag = NULL;
	if (os->os_membase != 0)
		bus_space_unmap(os->os_memt, os->os_memh, os->os_memsize);
	os->os_membase = 0;

	return;
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
void
ixgbe_setup_interface(struct ix_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	INIT_DEBUGOUT("ixgbe_setup_interface: begin");

	strlcpy(ifp->if_xname, sc->dev.dv_xname, IFNAMSIZ);
	ifp->if_baudrate = IF_Gbps(10);
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = ixgbe_ioctl;
	ifp->if_start = ixgbe_start;
	ifp->if_timer = 0;
	ifp->if_watchdog = ixgbe_watchdog;
	ifp->if_hardmtu = IXGBE_MAX_FRAME_SIZE -
	    ETHER_HDR_LEN - ETHER_CRC_LEN;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->num_tx_desc - 1);
	IFQ_SET_READY(&ifp->if_snd);
	
	m_clsetwms(ifp, MCLBYTES, 4, sc->num_rx_desc);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

#ifdef IX_CSUM_OFFLOAD
	ifp->if_capabilities |= IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4 |
	    IFCAP_CSUM_IPv4;
#endif

	sc->max_frame_size =
	    ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	/*
	 * Specify the media types supported by this sc and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&sc->media, IFM_IMASK, ixgbe_media_change,
		     ixgbe_media_status);
	ifmedia_add(&sc->media, IFM_ETHER | sc->optics |
	    IFM_FDX, 0, NULL);
	if ((hw->device_id == PCI_PRODUCT_INTEL_82598AT) ||
	    (hw->device_id == PCI_PRODUCT_INTEL_82598AT_DUAL)) {
		ifmedia_add(&sc->media,
		    IFM_ETHER | IFM_1000_T | IFM_FDX, 0, NULL);
		ifmedia_add(&sc->media,
		    IFM_ETHER | IFM_1000_T, 0, NULL);
	}
	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);


	return;
}

void
ixgbe_config_link(struct ix_softc *sc)
{
	uint32_t	autoneg, err = 0;
	int		sfp, negotiate;

	switch (sc->hw.phy.type) {
	case ixgbe_phy_sfp_avago:
	case ixgbe_phy_sfp_ftl:
	case ixgbe_phy_sfp_intel:
	case ixgbe_phy_sfp_unknown:
	case ixgbe_phy_sfp_passive_tyco:
	case ixgbe_phy_sfp_passive_unknown:
		sfp = 1;
		break;
	default:
		sfp = 0;
		break;
	}

	if (sfp) {
		if (&sc->hw.phy.multispeed_fiber) {
			sc->hw.mac.ops.setup_sfp(&sc->hw);
			ixgbe_hw0(&sc->hw, enable_tx_laser);
			/* XXX taskqueue_enqueue(sc->tq, &sc->msf_task); */
		} /* else */
			/* XXX taskqueue_enqueue(sc->tq, &sc->mod_task); */
	} else {
		if (sc->hw.mac.ops.check_link)
			err = sc->hw.mac.ops.check_link(&sc->hw, &autoneg,
			    &sc->link_up, FALSE);
			if (err)
				return;
		if (sc->hw.mac.ops.setup_link)
			err = sc->hw.mac.ops.setup_link(&sc->hw, autoneg,
			    negotiate, sc->link_up);
	}
	return;
}


/********************************************************************
 * Manage DMA'able memory.
  *******************************************************************/
int
ixgbe_dma_malloc(struct ix_softc *sc, bus_size_t size,
		struct ixgbe_dma_alloc *dma, int mapflags)
{
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct ixgbe_osdep	*os = &sc->osdep;
	int			 r;

	dma->dma_tag = os->os_pa->pa_dmat;
	r = bus_dmamap_create(dma->dma_tag, size, 1,
	    size, 0, BUS_DMA_NOWAIT, &dma->dma_map);
	if (r != 0) {
		printf("%s: ixgbe_dma_malloc: bus_dma_tag_create failed; "
		       "error %u\n", ifp->if_xname, r);
		goto fail_0;
	}

	r = bus_dmamem_alloc(dma->dma_tag, size, PAGE_SIZE, 0, &dma->dma_seg,
	    1, &dma->dma_nseg, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: ixgbe_dma_malloc: bus_dmamem_alloc failed; "
		       "error %u\n", ifp->if_xname, r);
		goto fail_1;
	}

	r = bus_dmamem_map(dma->dma_tag, &dma->dma_seg, dma->dma_nseg, size,
	    &dma->dma_vaddr, BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: ixgbe_dma_malloc: bus_dmamem_map failed; "
		       "error %u\n", ifp->if_xname, r);
		goto fail_2;
	}

	r = bus_dmamap_load(dma->dma_tag, dma->dma_map,
	    dma->dma_vaddr, size, NULL, 
	    mapflags | BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: ixgbe_dma_malloc: bus_dmamap_load failed; "
		       "error %u\n", ifp->if_xname, r);
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
ixgbe_dma_free(struct ix_softc *sc, struct ixgbe_dma_alloc *dma)
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
		dma->dma_map = NULL;
	}
}


/*********************************************************************
 *
 *  Allocate memory for the transmit and receive rings, and then
 *  the descriptors associated with each, called only once at attach.
 *
 **********************************************************************/
int
ixgbe_allocate_queues(struct ix_softc *sc)
{
	struct ifnet	*ifp = &sc->arpcom.ac_if;
	struct ix_queue *que;
	struct tx_ring *txr;
	struct rx_ring *rxr;
	int rsize, tsize;
	int txconf = 0, rxconf = 0, i;

	/* First allocate the top level queue structs */
	if (!(sc->queues =
	    (struct ix_queue *) malloc(sizeof(struct ix_queue) *
	    sc->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		printf("%s: Unable to allocate queue memory\n", ifp->if_xname);
		goto fail;
	}

	/* Then allocate the TX ring struct memory */
	if (!(sc->tx_rings =
	    (struct tx_ring *) malloc(sizeof(struct tx_ring) *
	    sc->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		printf("%s: Unable to allocate TX ring memory\n", ifp->if_xname);
		goto fail;
	}

	/* Next allocate the RX */
	if (!(sc->rx_rings =
	    (struct rx_ring *) malloc(sizeof(struct rx_ring) *
	    sc->num_queues, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		printf("%s: Unable to allocate RX ring memory\n", ifp->if_xname);
		goto rx_fail;
	}

	/* For the ring itself */
	tsize = roundup2(sc->num_tx_desc *
	    sizeof(union ixgbe_adv_tx_desc), 4096);

	/*
	 * Now set up the TX queues, txconf is needed to handle the
	 * possibility that things fail midcourse and we need to
	 * undo memory gracefully
	 */ 
	for (i = 0; i < sc->num_queues; i++, txconf++) {
		/* Set up some basics */
		txr = &sc->tx_rings[i];
		txr->sc = sc;
		txr->me = i;

		/* Initialize the TX side lock */
		mtx_init(&txr->tx_mtx, IPL_NET);

		if (ixgbe_dma_malloc(sc, tsize,
		    &txr->txdma, BUS_DMA_NOWAIT)) {
			printf("%s: Unable to allocate TX Descriptor memory\n",
			    ifp->if_xname);
			goto err_tx_desc;
		}
		txr->tx_base = (union ixgbe_adv_tx_desc *)txr->txdma.dma_vaddr;
		bzero((void *)txr->tx_base, tsize);
	}

	/*
	 * Next the RX queues...
	 */ 
	rsize = roundup2(sc->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), 4096);
	for (i = 0; i < sc->num_queues; i++, rxconf++) {
		rxr = &sc->rx_rings[i];
		/* Set up some basics */
		rxr->sc = sc;
		rxr->me = i;

		/* Initialize the TX side lock */
		mtx_init(&rxr->rx_mtx, IPL_NET);

		if (ixgbe_dma_malloc(sc, rsize,
			&rxr->rxdma, BUS_DMA_NOWAIT)) {
			printf("%s: Unable to allocate RxDescriptor memory\n",
			    ifp->if_xname);
			goto err_rx_desc;
		}
		rxr->rx_base = (union ixgbe_adv_rx_desc *)rxr->rxdma.dma_vaddr;
		bzero((void *)rxr->rx_base, rsize);
	}

	/*
	 * Finally set up the queue holding structs
	 */
	for (i = 0; i < sc->num_queues; i++) {
		que = &sc->queues[i];
		que->sc = sc;
		que->txr = &sc->tx_rings[i];
		que->rxr = &sc->rx_rings[i];
	}

	return (0);

err_rx_desc:
	for (rxr = sc->rx_rings; rxconf > 0; rxr++, rxconf--)
		ixgbe_dma_free(sc, &rxr->rxdma);
err_tx_desc:
	for (txr = sc->tx_rings; txconf > 0; txr++, txconf--) {
		ixgbe_dma_free(sc, &txr->txdma);
	}
	free(sc->rx_rings, M_DEVBUF);
	sc->rx_rings = NULL;
rx_fail:
	free(sc->tx_rings, M_DEVBUF);
	sc->tx_rings = NULL;
fail:
	return (ENOMEM);
}

/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all
 *  the information needed to transmit a packet on the wire. This is
 *  called only once at attach, setup is done every reset.
 *
 **********************************************************************/
int
ixgbe_allocate_transmit_buffers(struct tx_ring *txr)
{
	struct ix_softc 	*sc;
	struct ixgbe_osdep	*os;
	struct ifnet		*ifp;
	struct ixgbe_tx_buf	*txbuf;
	int			 error, i;
	int			 max_segs;

	sc = txr->sc;
	os = &sc->osdep;
	ifp = &sc->arpcom.ac_if;

	if (sc->hw.mac.type == ixgbe_mac_82598EB)
		max_segs = IXGBE_82598_SCATTER;
	else
		max_segs = IXGBE_82599_SCATTER;

	if (!(txr->tx_buffers =
	    (struct ixgbe_tx_buf *) malloc(sizeof(struct ixgbe_tx_buf) *
	    sc->num_tx_desc, M_DEVBUF, M_NOWAIT | M_ZERO))) {
		printf("%s: Unable to allocate tx_buffer memory\n",
		    ifp->if_xname);
		error = ENOMEM;
		goto fail;
	}
	txr->txtag = txr->txdma.dma_tag;

        /* Create the descriptor buffer dma maps */
	for (i = 0; i < sc->num_tx_desc; i++) {
		txbuf = &txr->tx_buffers[i];
		error = bus_dmamap_create(txr->txdma.dma_tag, IXGBE_TSO_SIZE,
			    max_segs, PAGE_SIZE, 0,
			    BUS_DMA_NOWAIT, &txbuf->map);

		if (error != 0) {
			printf("%s: Unable to create TX DMA map\n",
			    ifp->if_xname);
			goto fail;
		}
	}

	return 0;
fail:
	return (error);
}

/*********************************************************************
 *
 *  Initialize a transmit ring.
 *
 **********************************************************************/
int
ixgbe_setup_transmit_ring(struct tx_ring *txr)
{
	struct ix_softc		*sc = txr->sc;
	int			 error;

	/* Now allocate transmit buffers for the ring */
	if ((error = ixgbe_allocate_transmit_buffers(txr)) != 0)
		return (error);

	/* Clear the old ring contents */
	bzero((void *)txr->tx_base,
	      (sizeof(union ixgbe_adv_tx_desc)) * sc->num_tx_desc);

	/* Reset indices */
	txr->next_avail_desc = 0;
	txr->next_to_clean = 0;

	/* Set number of descriptors available */
	txr->tx_avail = sc->num_tx_desc;

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    0, txr->txdma.dma_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

/*********************************************************************
 *
 *  Initialize all transmit rings.
 *
 **********************************************************************/
int
ixgbe_setup_transmit_structures(struct ix_softc *sc)
{
	struct tx_ring *txr = sc->tx_rings;
	int		i, error;

	for (i = 0; i < sc->num_queues; i++, txr++) {
		if ((error = ixgbe_setup_transmit_ring(txr)) != 0)
			goto fail;
	}

	return (0);
fail:
	ixgbe_free_transmit_structures(sc);
	return (error);
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
void
ixgbe_initialize_transmit_units(struct ix_softc *sc)
{
	struct ifnet	*ifp = &sc->arpcom.ac_if;
	struct tx_ring	*txr;
	struct ixgbe_hw	*hw = &sc->hw;
	int		 i;
	uint64_t	 tdba;
	uint32_t	 txctrl;

	/* Setup the Base and Length of the Tx Descriptor Ring */

	for (i = 0; i < sc->num_queues; i++) {
		txr = &sc->tx_rings[i];

		/* Setup descriptor base address */
		tdba = txr->txdma.dma_map->dm_segs[0].ds_addr;
		IXGBE_WRITE_REG(hw, IXGBE_TDBAL(i),
		       (tdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(hw, IXGBE_TDBAH(i), (tdba >> 32));
		IXGBE_WRITE_REG(hw, IXGBE_TDLEN(i),
		    sc->num_tx_desc * sizeof(struct ixgbe_legacy_tx_desc));

		/* Setup the HW Tx Head and Tail descriptor pointers */
		IXGBE_WRITE_REG(hw, IXGBE_TDH(i), 0);
		IXGBE_WRITE_REG(hw, IXGBE_TDT(i), 0);

		/* Setup Transmit Descriptor Cmd Settings */
		txr->txd_cmd = IXGBE_TXD_CMD_IFCS;
		txr->queue_status = IXGBE_QUEUE_IDLE;
		txr->watchdog_timer = 0;

		/* Disable Head Writeback */
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL(i));
			break;
		case ixgbe_mac_82599EB:
		default:
			txctrl = IXGBE_READ_REG(hw, IXGBE_DCA_TXCTRL_82599(i));
			break;
		}
		txctrl &= ~IXGBE_DCA_TXCTRL_TX_WB_RO_EN;
		switch (hw->mac.type) {
		case ixgbe_mac_82598EB:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL(i), txctrl);
			break;
		case ixgbe_mac_82599EB:
		default:
			IXGBE_WRITE_REG(hw, IXGBE_DCA_TXCTRL_82599(i), txctrl);
			break;
		}
	}
	ifp->if_timer = 0;

	if (hw->mac.type == ixgbe_mac_82599EB) {
		uint32_t dmatxctl, rttdcs;
		dmatxctl = IXGBE_READ_REG(hw, IXGBE_DMATXCTL);
		dmatxctl |= IXGBE_DMATXCTL_TE;
		IXGBE_WRITE_REG(hw, IXGBE_DMATXCTL, dmatxctl);
		/* Disable arbiter to set MTQC */
		rttdcs = IXGBE_READ_REG(hw, IXGBE_RTTDCS);
		rttdcs |= IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
		IXGBE_WRITE_REG(hw, IXGBE_MTQC, IXGBE_MTQC_64Q_1PB);
		rttdcs &= ~IXGBE_RTTDCS_ARBDIS;
		IXGBE_WRITE_REG(hw, IXGBE_RTTDCS, rttdcs);
	}

	return;
}

/*********************************************************************
 *
 *  Free all transmit rings.
 *
 **********************************************************************/
void
ixgbe_free_transmit_structures(struct ix_softc *sc)
{
	struct tx_ring *txr = sc->tx_rings;
	int		i;

	for (i = 0; i < sc->num_queues; i++, txr++) {
		ixgbe_free_transmit_buffers(txr);
	}
}

/*********************************************************************
 *
 *  Free transmit ring related data structures.
 *
 **********************************************************************/
void
ixgbe_free_transmit_buffers(struct tx_ring *txr)
{
	struct ix_softc *sc = txr->sc;
	struct ixgbe_tx_buf *tx_buffer;
	int             i;

	INIT_DEBUGOUT("free_transmit_ring: begin");

	if (txr->tx_buffers == NULL)
		return;

	tx_buffer = txr->tx_buffers;
	for (i = 0; i < sc->num_tx_desc; i++, tx_buffer++) {
		if (tx_buffer->map != NULL && tx_buffer->map->dm_nsegs > 0) {
			bus_dmamap_sync(txr->txdma.dma_tag, tx_buffer->map,
			    0, tx_buffer->map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(txr->txdma.dma_tag,
			    tx_buffer->map);
		}
		if (tx_buffer->m_head != NULL) {
			m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
		}
		if (tx_buffer->map != NULL) {
			bus_dmamap_destroy(txr->txdma.dma_tag,
			    tx_buffer->map);
			tx_buffer->map = NULL;
		}
	}

	if (txr->tx_buffers != NULL)
		free(txr->tx_buffers, M_DEVBUF);
	txr->tx_buffers = NULL;
	txr->txtag = NULL;
}

/*********************************************************************
 *
 *  Advanced Context Descriptor setup for VLAN or CSUM
 *
 **********************************************************************/

int
ixgbe_tx_ctx_setup(struct tx_ring *txr, struct mbuf *mp)
{
	struct ix_softc *sc = txr->sc;
	struct ifnet	*ifp = &sc->arpcom.ac_if;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ixgbe_tx_buf        *tx_buffer;
	uint32_t vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	struct ip *ip;
#ifdef notyet
	struct ip6_hdr *ip6;
#endif
	uint8_t ipproto = 0;
	int  ehdrlen, ip_hlen = 0;
	uint16_t etype;
	int offload = TRUE;
	int ctxd = txr->next_avail_desc;
#if NVLAN > 0
	struct ether_vlan_header *eh;
#else
	struct ether_header *eh;
#endif
	uint16_t vtag = 0;

	if ((ifp->if_capabilities & IFCAP_CSUM_IPv4) == 0)
		offload = FALSE;

	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	/*
	 * In advanced descriptors the vlan tag must 
	 * be placed into the descriptor itself.
	 */
#if NVLAN > 0
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
		vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	} else
#endif
	if (offload == FALSE)
		return FALSE;	/* No need for CTX */

	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present,
	 * helpful for QinQ too.
	 */
#if NVLAN > 0
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}
#else
	eh = mtod(mp, struct ether_header *);
	etype = ntohs(eh->ether_type);
	ehdrlen = ETHER_HDR_LEN;
#endif

	/* Set the ether header length */
	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;

	switch (etype) {
	case ETHERTYPE_IP:
		ip = (struct ip *)(mp->m_data + ehdrlen);
		ip_hlen = ip->ip_hl << 2;
		if (mp->m_len < ehdrlen + ip_hlen)
			return FALSE; /* failure */
		ipproto = ip->ip_p;
		if (mp->m_pkthdr.csum_flags & M_IPV4_CSUM_OUT)
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
		break;
#ifdef notyet
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(mp->m_data + ehdrlen);
		ip_hlen = sizeof(struct ip6_hdr);
		if (mp->m_len < ehdrlen + ip_hlen)
			return FALSE; /* failure */
		ipproto = ip6->ip6_nxt;
		if (mp->m_pkthdr.csum_flags & M_IPV4_CSUM_OUT)
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV6;
		break;
#endif
	default:
		offload = FALSE;
		break;
	}

	vlan_macip_lens |= ip_hlen;
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;

	switch (ipproto) {
	case IPPROTO_TCP:
		if (mp->m_pkthdr.csum_flags & M_TCP_CSUM_OUT)
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
		break;
	case IPPROTO_UDP:
		if (mp->m_pkthdr.csum_flags & M_UDP_CSUM_OUT)
			type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_UDP;
		break;
	default:
		offload = FALSE;
		break;
	}

	/* Now copy bits into descriptor */
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);
	TXD->seqnum_seed = htole32(0);
	TXD->mss_l4len_idx = htole32(0);

	tx_buffer->m_head = NULL;
	tx_buffer->eop_index = -1;

	/* We've consumed the first desc, adjust counters */
	if (++ctxd == sc->num_tx_desc)
		ctxd = 0;
	txr->next_avail_desc = ctxd;
	--txr->tx_avail;

        return (offload);
}

#ifdef notyet
/**********************************************************************
 *
 *  Setup work for hardware segmentation offload (TSO) on
 *  scs using advanced tx descriptors
 *
 **********************************************************************/
int
ixgbe_tso_setup(struct tx_ring *txr, struct mbuf *mp, uint32_t *paylen)
{
	struct ix_softc *sc = txr->sc;
	struct ixgbe_adv_tx_context_desc *TXD;
	struct ixgbe_tx_buf        *tx_buffer;
	uint32_t vlan_macip_lens = 0, type_tucmd_mlhl = 0;
	uint32_t mss_l4len_idx = 0;
	int ctxd, ehdrlen,  hdrlen, ip_hlen, tcp_hlen;
#if NVLAN > 0
	uint16_t vtag = 0;
	struct ether_vlan_header *eh;
#else
	struct ether_header *eh;
#endif
	struct ip *ip;
	struct tcphdr *th;

	if (((mp->m_pkthdr.csum_flags & CSUM_TSO) == 0) ||
	    (mp->m_pkthdr.len <= IXGBE_TX_BUFFER_SIZE))
	        return FALSE;

	/*
	 * Determine where frame payload starts.
	 * Jump over vlan headers if already present
	 */
#if NVLAN > 0
	eh = mtod(mp, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) 
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	else
		ehdrlen = ETHER_HDR_LEN;
#else
	eh = mtod(mp, struct ether_header *);
	ehdrlen = ETHER_HDR_LEN;
#endif

        /* Ensure we have at least the IP+TCP header in the first mbuf. */
        if (mp->m_len < ehdrlen + sizeof(struct ip) + sizeof(struct tcphdr))
		return FALSE;

	ctxd = txr->next_avail_desc;
	tx_buffer = &txr->tx_buffers[ctxd];
	TXD = (struct ixgbe_adv_tx_context_desc *) &txr->tx_base[ctxd];

	ip = (struct ip *)(mp->m_data + ehdrlen);
	if (ip->ip_p != IPPROTO_TCP)
		return FALSE;   /* 0 */
	ip->ip_len = 0;
	ip->ip_sum = 0;
	ip_hlen = ip->ip_hl << 2;
	th = (struct tcphdr *)((caddr_t)ip + ip_hlen);
	th->th_sum = in_pseudo(ip->ip_src.s_addr,
	    ip->ip_dst.s_addr, htons(IPPROTO_TCP));
	tcp_hlen = th->th_off << 2;
	hdrlen = ehdrlen + ip_hlen + tcp_hlen;
	/* This is used in the transmit desc in encap */
	*paylen = mp->m_pkthdr.len - hdrlen;

#if NVLAN > 0
	/* VLAN MACLEN IPLEN */
	if (mp->m_flags & M_VLANTAG) {
		vtag = htole16(mp->m_pkthdr.ether_vtag);
		vlan_macip_lens |= (vtag << IXGBE_ADVTXD_VLAN_SHIFT);
	}
#endif

	vlan_macip_lens |= ehdrlen << IXGBE_ADVTXD_MACLEN_SHIFT;
	vlan_macip_lens |= ip_hlen;
	TXD->vlan_macip_lens |= htole32(vlan_macip_lens);

	/* ADV DTYPE TUCMD */
	type_tucmd_mlhl |= IXGBE_ADVTXD_DCMD_DEXT | IXGBE_ADVTXD_DTYP_CTXT;
	type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_L4T_TCP;
	type_tucmd_mlhl |= IXGBE_ADVTXD_TUCMD_IPV4;
	TXD->type_tucmd_mlhl |= htole32(type_tucmd_mlhl);


	/* MSS L4LEN IDX */
	mss_l4len_idx |= (mp->m_pkthdr.tso_segsz << IXGBE_ADVTXD_MSS_SHIFT);
	mss_l4len_idx |= (tcp_hlen << IXGBE_ADVTXD_L4LEN_SHIFT);
	TXD->mss_l4len_idx = htole32(mss_l4len_idx);

	TXD->seqnum_seed = htole32(0);
	tx_buffer->m_head = NULL;

	if (++ctxd == sc->num_tx_desc)
		ctxd = 0;

	txr->tx_avail--;
	txr->next_avail_desc = ctxd;
	return TRUE;
}

#else
/* This makes it easy to keep the code common */
int
ixgbe_tso_setup(struct tx_ring *txr, struct mbuf *mp, uint32_t *paylen)
{
	return (FALSE);
}
#endif

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue.
 *
 **********************************************************************/
int
ixgbe_txeof(struct tx_ring *txr)
{
	struct ix_softc			*sc = txr->sc;
	struct ifnet			*ifp = &sc->arpcom.ac_if;
	uint32_t			 first, last, done, processed;
	struct ixgbe_tx_buf		*tx_buffer;
	struct ixgbe_legacy_tx_desc *tx_desc, *eop_desc;

	if (txr->tx_avail == sc->num_tx_desc) {
		txr->queue_status = IXGBE_QUEUE_IDLE;
		return FALSE;
	}

	processed = 0;
	first = txr->next_to_clean;
	tx_buffer = &txr->tx_buffers[first];
	/* For cleanup we just use legacy struct */
	tx_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[first];
	last = tx_buffer->eop_index;
	if (last == -1)
		return FALSE;
	eop_desc = (struct ixgbe_legacy_tx_desc *)&txr->tx_base[last];

	/*
	 * Get the index of the first descriptor
	 * BEYOND the EOP and call that 'done'.
	 * I do this so the comparison in the
	 * inner while loop below can be simple
	 */
	if (++last == sc->num_tx_desc) last = 0;
	done = last;

        bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    0, txr->txdma.dma_map->dm_mapsize,
            BUS_DMASYNC_POSTREAD);

	while (eop_desc->upper.fields.status & IXGBE_TXD_STAT_DD) {
		/* We clean the range of the packet */
		while (first != done) {
			tx_desc->upper.data = 0;
			tx_desc->lower.data = 0;
			tx_desc->buffer_addr = 0;
			++txr->tx_avail;
			++processed;

			if (tx_buffer->m_head) {
				bus_dmamap_sync(txr->txdma.dma_tag,
				    tx_buffer->map,
				    0, tx_buffer->map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(txr->txdma.dma_tag,
				    tx_buffer->map);
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
			}
			tx_buffer->eop_index = -1;

			if (++first == sc->num_tx_desc)
				first = 0;

			tx_buffer = &txr->tx_buffers[first];
			tx_desc = (struct ixgbe_legacy_tx_desc *)
			    &txr->tx_base[first];
		}
		++txr->packets;
		++ifp->if_opackets;
		/* See if there is more work now */
		last = tx_buffer->eop_index;
		if (last != -1) {
			eop_desc =
			    (struct ixgbe_legacy_tx_desc *)&txr->tx_base[last];
			/* Get next done point */
			if (++last == sc->num_tx_desc) last = 0;
			done = last;
		} else
			break;
	}

	bus_dmamap_sync(txr->txdma.dma_tag, txr->txdma.dma_map,
	    0, txr->txdma.dma_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	txr->next_to_clean = first;

	/*
	 * If we have enough room, clear IFF_OACTIVE to tell the stack that
	 * it is OK to send packets. If there are no pending descriptors,
	 * clear the timeout. Otherwise, if some descriptors have been freed,
	 * restart the timeout.
	 */
	if (txr->tx_avail > IXGBE_TX_CLEANUP_THRESHOLD) {
		ifp->if_flags &= ~IFF_OACTIVE;

		/* If all are clean turn off the timer */
		if (txr->tx_avail == sc->num_tx_desc) {
			ifp->if_timer = 0;
			txr->watchdog_timer = 0;
			return FALSE;
		}
		/* Some were cleaned, so reset timer */
		else if (processed) {
			ifp->if_timer = IXGBE_TX_TIMEOUT;
			txr->watchdog_timer = IXGBE_TX_TIMEOUT;
		}
	}

	return TRUE;
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
int
ixgbe_get_buf(struct rx_ring *rxr, int i)
{
	struct ix_softc		*sc = rxr->sc;
	struct ixgbe_rx_buf	*rxbuf;
	struct mbuf		*mh, *mp;
	int			error;
	union ixgbe_adv_rx_desc	*rxdesc;
	size_t			 dsize = sizeof(union ixgbe_adv_rx_desc);

	rxbuf = &rxr->rx_buffers[i];
	rxdesc = &rxr->rx_base[i];
	if (rxbuf->m_head != NULL || rxbuf->m_pack) {
		printf("%s: ixgbe_get_buf: slot %d already has an mbuf\n",
		    sc->dev.dv_xname, i);
		return (ENOBUFS);
	}

	/* needed in any case so prealocate since this one will fail for sure */
	mp = MCLGETI(NULL, M_DONTWAIT, &sc->arpcom.ac_if, sc->rx_mbuf_sz);
	if (!mp) {
		sc->mbuf_packet_failed++;
		return (ENOBUFS);
	}

	if (rxr->hdr_split == FALSE)
		goto no_split;

	mh = m_gethdr(M_DONTWAIT, MT_DATA);
	if (mh == NULL)
		return (ENOBUFS);

	mh->m_pkthdr.len = mh->m_len = MHLEN;
	mh->m_len = MHLEN;
	/* always offset header buffers */
	m_adj(mh, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf(rxr->rxdma.dma_tag, rxbuf->hmap,
	    mh, BUS_DMA_NOWAIT);
	if (error) {
		m_freem(mh);
		return (error);
	}
        bus_dmamap_sync(rxr->rxdma.dma_tag, rxbuf->hmap,
	    0, rxbuf->hmap->dm_mapsize, BUS_DMASYNC_PREREAD);
	rxbuf->m_head = mh;

	rxdesc->read.hdr_addr = htole64(rxbuf->hmap->dm_segs[0].ds_addr);

no_split:
	mp->m_len = mp->m_pkthdr.len = sc->rx_mbuf_sz;
	/* only adjust if this is not a split header */
	if (rxr->hdr_split == FALSE &&
	    sc->max_frame_size <= (sc->rx_mbuf_sz - ETHER_ALIGN))
		m_adj(mp, ETHER_ALIGN);

	error = bus_dmamap_load_mbuf(rxr->rxdma.dma_tag, rxbuf->pmap,
	    mp, BUS_DMA_NOWAIT);
	if (error) {
		m_freem(mp);
		return (error);
	}

        bus_dmamap_sync(rxr->rxdma.dma_tag, rxbuf->pmap,
	    0, rxbuf->pmap->dm_mapsize, BUS_DMASYNC_PREREAD);
	rxbuf->m_pack = mp;

	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    dsize * i, dsize, BUS_DMASYNC_POSTWRITE);

	rxdesc->read.pkt_addr = htole64(rxbuf->pmap->dm_segs[0].ds_addr);

	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
	    dsize * i, dsize, BUS_DMASYNC_PREWRITE);

	rxr->rx_ndescs++;

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
ixgbe_allocate_receive_buffers(struct rx_ring *rxr)
{
	struct ix_softc		*sc = rxr->sc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct ixgbe_rx_buf 	*rxbuf;
	int             	i, bsize, error;

	bsize = sizeof(struct ixgbe_rx_buf) * sc->num_rx_desc;
	if (!(rxr->rx_buffers = (struct ixgbe_rx_buf *) malloc(bsize,
	    M_DEVBUF, M_NOWAIT | M_ZERO))) {
		printf("%s: Unable to allocate rx_buffer memory\n",
		    ifp->if_xname);
		error = ENOMEM;
		goto fail;
	}

	rxbuf = rxr->rx_buffers;
	for (i = 0; i < sc->num_rx_desc; i++, rxbuf++) {
		error = bus_dmamap_create(rxr->rxdma.dma_tag, MSIZE, 1,
		    MSIZE, 0, BUS_DMA_NOWAIT, &rxbuf->hmap);
		if (error) {
			printf("%s: Unable to create Head DMA map\n",
			    ifp->if_xname);
			goto fail;
		}
		error = bus_dmamap_create(rxr->rxdma.dma_tag, 16 * 1024, 1,
		    16 * 1024, 0, BUS_DMA_NOWAIT, &rxbuf->pmap);
		if (error) {
			printf("%s: Unable to create Pack DMA map\n",
			    ifp->if_xname);
			goto fail;
		}
	}
	bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map, 0,
	    rxr->rxdma.dma_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);

fail:
	return (error);
}

/*********************************************************************
 *
 *  Initialize a receive ring and its buffers.
 *
 **********************************************************************/
int
ixgbe_setup_receive_ring(struct rx_ring *rxr)
{
	struct ix_softc		*sc = rxr->sc;
	int			 rsize, error;

	rsize = roundup2(sc->num_rx_desc *
	    sizeof(union ixgbe_adv_rx_desc), 4096);
	/* Clear the ring contents */
	bzero((void *)rxr->rx_base, rsize);

	if ((error = ixgbe_allocate_receive_buffers(rxr)) != 0)
		return (error);

	/* Setup our descriptor indices */
	rxr->next_to_check = 0;
	rxr->last_desc_filled = sc->num_rx_desc - 1;
	rxr->rx_ndescs = 0;

	ixgbe_rxfill(rxr);
	if (rxr->rx_ndescs < 1) {
		printf("%s: unable to fill any rx descriptors\n",
		    sc->dev.dv_xname);
		return (ENOBUFS);
	}

	return (0);
}

int
ixgbe_rxfill(struct rx_ring *rxr)
{
	struct ix_softc *sc = rxr->sc;
	int		 post = 0;
	int		 i;

	i = rxr->last_desc_filled;
	while (rxr->rx_ndescs < sc->num_rx_desc) {
		if (++i == sc->num_rx_desc)
			i = 0;

		if (ixgbe_get_buf(rxr, i) != 0)
			break;

		rxr->last_desc_filled = i;
		post = 1;
	}

	return (post);
}

/*********************************************************************
 *
 *  Initialize all receive rings.
 *
 **********************************************************************/
int
ixgbe_setup_receive_structures(struct ix_softc *sc)
{
	struct rx_ring *rxr = sc->rx_rings;
	int i;

	for (i = 0; i < sc->num_queues; i++, rxr++)
		if (ixgbe_setup_receive_ring(rxr))
			goto fail;

	return (0);

fail:
	ixgbe_free_receive_structures(sc);
	return (ENOBUFS);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *
 **********************************************************************/
#define IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT 2

void
ixgbe_initialize_receive_units(struct ix_softc *sc)
{
	struct	rx_ring	*rxr = sc->rx_rings;
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	uint32_t	bufsz, rxctrl, fctrl, srrctl, rxcsum;
	uint32_t	reta, mrqc = 0, hlreg;
	uint32_t	random[10];
	int		i;

	/*
	 * Make sure receives are disabled while
	 * setting up the descriptor ring
	 */
	rxctrl = IXGBE_READ_REG(&sc->hw, IXGBE_RXCTRL);
	IXGBE_WRITE_REG(&sc->hw, IXGBE_RXCTRL,
	    rxctrl & ~IXGBE_RXCTRL_RXEN);

	/* Enable broadcasts */
	fctrl = IXGBE_READ_REG(&sc->hw, IXGBE_FCTRL);
	fctrl |= IXGBE_FCTRL_BAM;
	fctrl |= IXGBE_FCTRL_DPF;
	fctrl |= IXGBE_FCTRL_PMCF;
	IXGBE_WRITE_REG(&sc->hw, IXGBE_FCTRL, fctrl);

	/* Set for Jumbo Frames? */
	hlreg = IXGBE_READ_REG(&sc->hw, IXGBE_HLREG0);
	if (ifp->if_mtu > ETHERMTU)
		hlreg |= IXGBE_HLREG0_JUMBOEN;
	else
		hlreg &= ~IXGBE_HLREG0_JUMBOEN;
	IXGBE_WRITE_REG(&sc->hw, IXGBE_HLREG0, hlreg);

	bufsz = sc->rx_mbuf_sz >> IXGBE_SRRCTL_BSIZEPKT_SHIFT;

	for (i = 0; i < sc->num_queues; i++, rxr++) {
		uint64_t rdba = rxr->rxdma.dma_map->dm_segs[0].ds_addr;

		/* Setup the Base and Length of the Rx Descriptor Ring */
		IXGBE_WRITE_REG(&sc->hw, IXGBE_RDBAL(i),
			       (rdba & 0x00000000ffffffffULL));
		IXGBE_WRITE_REG(&sc->hw, IXGBE_RDBAH(i), (rdba >> 32));
		IXGBE_WRITE_REG(&sc->hw, IXGBE_RDLEN(i),
		    sc->num_rx_desc * sizeof(union ixgbe_adv_rx_desc));

		/* Set up the SRRCTL register */
		srrctl = IXGBE_READ_REG(&sc->hw, IXGBE_SRRCTL(i));
		srrctl &= ~IXGBE_SRRCTL_BSIZEHDR_MASK;
		srrctl &= ~IXGBE_SRRCTL_BSIZEPKT_MASK;
		srrctl |= bufsz;
		if (rxr->hdr_split) {
			/* Use a standard mbuf for the header */
			srrctl |= ((IXGBE_RX_HDR <<
			    IXGBE_SRRCTL_BSIZEHDRSIZE_SHIFT)
			    & IXGBE_SRRCTL_BSIZEHDR_MASK);
			srrctl |= IXGBE_SRRCTL_DESCTYPE_HDR_SPLIT_ALWAYS;
		} else
			srrctl |= IXGBE_SRRCTL_DESCTYPE_ADV_ONEBUF;
		IXGBE_WRITE_REG(&sc->hw, IXGBE_SRRCTL(i), srrctl);

		/* Setup the HW Rx Head and Tail Descriptor Pointers */
		IXGBE_WRITE_REG(&sc->hw, IXGBE_RDH(i), 0);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_RDT(i), 0);
	}

	if (sc->hw.mac.type != ixgbe_mac_82598EB) {
		uint32_t psrtype = IXGBE_PSRTYPE_TCPHDR |
		              IXGBE_PSRTYPE_UDPHDR |
			      IXGBE_PSRTYPE_IPV4HDR |
			      IXGBE_PSRTYPE_IPV6HDR;
		IXGBE_WRITE_REG(&sc->hw, IXGBE_PSRTYPE(0), psrtype);
	}

	rxcsum = IXGBE_READ_REG(&sc->hw, IXGBE_RXCSUM);

	/* Setup RSS */
	if (sc->num_queues > 1) {
		int j;
		reta = 0;
		/* set up random bits */
		arc4random_buf(&random, sizeof(random));

		/* Set up the redirection table */
		for (i = 0, j = 0; i < 128; i++, j++) {
			if (j == sc->num_queues)
				j = 0;
			reta = (reta << 8) | (j * 0x11);
			if ((i & 3) == 3)
				IXGBE_WRITE_REG(&sc->hw, IXGBE_RETA(i >> 2), reta);
		}

		/* Now fill our hash function seeds */
		for (i = 0; i < 10; i++)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_RSSRK(i), random[i]);

		/* Perform hash on these packet types */
		mrqc = IXGBE_MRQC_RSSEN
		    | IXGBE_MRQC_RSS_FIELD_IPV4
		    | IXGBE_MRQC_RSS_FIELD_IPV4_TCP
		    | IXGBE_MRQC_RSS_FIELD_IPV4_UDP
		    | IXGBE_MRQC_RSS_FIELD_IPV6_EX_TCP
		    | IXGBE_MRQC_RSS_FIELD_IPV6_EX
		    | IXGBE_MRQC_RSS_FIELD_IPV6
		    | IXGBE_MRQC_RSS_FIELD_IPV6_TCP
		    | IXGBE_MRQC_RSS_FIELD_IPV6_UDP
		    | IXGBE_MRQC_RSS_FIELD_IPV6_EX_UDP;
		IXGBE_WRITE_REG(&sc->hw, IXGBE_MRQC, mrqc);

		/* RSS and RX IPP Checksum are mutually exclusive */
		rxcsum |= IXGBE_RXCSUM_PCSD;
	}

	if (ifp->if_capabilities & IFCAP_CSUM_IPv4)
		rxcsum |= IXGBE_RXCSUM_PCSD;

	if (!(rxcsum & IXGBE_RXCSUM_PCSD))
		rxcsum |= IXGBE_RXCSUM_IPPCSE;

	IXGBE_WRITE_REG(&sc->hw, IXGBE_RXCSUM, rxcsum);

	return;
}

/*********************************************************************
 *
 *  Free all receive rings.
 *
 **********************************************************************/
void
ixgbe_free_receive_structures(struct ix_softc *sc)
{
	struct rx_ring *rxr = sc->rx_rings;
	int		i;

	for (i = 0; i < sc->num_queues; i++, rxr++) {
		ixgbe_free_receive_buffers(rxr);
	}
}

/*********************************************************************
 *
 *  Free receive ring data structures
 *
 **********************************************************************/
void
ixgbe_free_receive_buffers(struct rx_ring *rxr)
{
	struct ix_softc		*sc;
	struct ixgbe_rx_buf	*rxbuf;
	int			 i;

	sc = rxr->sc;
	if (rxr->rx_buffers != NULL) {
		for (i = 0; i < sc->num_rx_desc; i++) {
			rxbuf = &rxr->rx_buffers[i];
			if (rxbuf->m_head != NULL) {
				bus_dmamap_sync(rxr->rxdma.dma_tag, rxbuf->hmap,
				    0, rxbuf->hmap->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->rxdma.dma_tag,
				    rxbuf->hmap);
				m_freem(rxbuf->m_head);
				rxbuf->m_head = NULL;
			}
			if (rxbuf->m_pack != NULL) {
				bus_dmamap_sync(rxr->rxdma.dma_tag, rxbuf->pmap,
				    0, rxbuf->pmap->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(rxr->rxdma.dma_tag,
				    rxbuf->pmap);
				m_freem(rxbuf->m_pack);
				rxbuf->m_pack = NULL;
			}
			bus_dmamap_destroy(rxr->rxdma.dma_tag, rxbuf->hmap);
			bus_dmamap_destroy(rxr->rxdma.dma_tag, rxbuf->pmap);
			rxbuf->hmap = NULL;
			rxbuf->pmap = NULL;
		}
		free(rxr->rx_buffers, M_DEVBUF);
		rxr->rx_buffers = NULL;
	}
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
int
ixgbe_rxeof(struct ix_queue *que, int count)
{
	struct ix_softc 	*sc = que->sc;
	struct rx_ring		*rxr = que->rxr;
	struct ifnet   		*ifp = &sc->arpcom.ac_if;
	struct mbuf    		*mh, *mp, *sendmp;
	uint8_t		    	 eop = 0;
	uint16_t		 hlen, plen, hdr, vtag;
	uint32_t		 staterr, ptype;
	struct ixgbe_rx_buf	*rxbuf, *nxbuf;
	union ixgbe_adv_rx_desc	*rxdesc;
	size_t			 dsize = sizeof(union ixgbe_adv_rx_desc);
	int			 i, nextp;

	if (!ISSET(ifp->if_flags, IFF_RUNNING))
		return FALSE;

	i = rxr->next_to_check;
	while (count != 0 && rxr->rx_ndescs > 0) {
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    dsize * i, dsize, BUS_DMASYNC_POSTREAD);

		rxdesc = &rxr->rx_base[i];
		staterr = letoh32(rxdesc->wb.upper.status_error);
		if (!ISSET(staterr, IXGBE_RXD_STAT_DD)) {
			bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
			    dsize * i, dsize,
			    BUS_DMASYNC_PREREAD);
			break;
		}

		/* Zero out the receive descriptors status  */
		rxdesc->wb.upper.status_error = 0;
		rxbuf = &rxr->rx_buffers[i];

		/* pull the mbuf off the ring */
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxbuf->hmap, 0,
		    rxbuf->hmap->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rxr->rxdma.dma_tag, rxbuf->hmap);
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxbuf->pmap, 0,
		    rxbuf->pmap->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rxr->rxdma.dma_tag, rxbuf->pmap);

		mh = rxbuf->m_head;
		mp = rxbuf->m_pack;
		plen = letoh16(rxdesc->wb.upper.length);
		ptype = letoh32(rxdesc->wb.lower.lo_dword.data) &
		    IXGBE_RXDADV_PKTTYPE_MASK;
		hdr = letoh16(rxdesc->wb.lower.lo_dword.hs_rss.hdr_info);
		vtag = letoh16(rxdesc->wb.upper.vlan);
		eop = ((staterr & IXGBE_RXD_STAT_EOP) != 0);

		if (staterr & IXGBE_RXDADV_ERR_FRAME_ERR_MASK) {
			ifp->if_ierrors++;
			sc->dropped_pkts++;

			if (rxbuf->fmp) {
				m_freem(rxbuf->fmp);
				rxbuf->fmp = NULL;
			}

			m_freem(mh);
			m_freem(mp);
			rxbuf->m_head = NULL;
			rxbuf->m_pack = NULL;
			goto next_desc;
		}

		if (mp == NULL) {
			panic("%s: ixgbe_rxeof: NULL mbuf in slot %d "
			    "(nrx %d, filled %d)", sc->dev.dv_xname,
			    i, rxr->rx_ndescs,
			    rxr->last_desc_filled);
		}

		/* XXX ixgbe_realign() STRICT_ALIGN */
		/* Currently no HW RSC support of 82599 */ 
		if (!eop) {
			/*
			 * Figure out the next descriptor of this frame.
			 */
			nextp = i + 1;
			if (nextp == sc->num_rx_desc)
				nextp = 0;
			nxbuf = &rxr->rx_buffers[nextp];
			/* prefetch(nxbuf); */
		}
		/*
		 * The header mbuf is ONLY used when header
		 * split is enabled, otherwise we get normal
		 * behavior, ie, both header and payload
		 * are DMA'd into the payload buffer.
		 *
		 * Rather than using the fmp/lmp global pointers
		 * we now keep the head of a packet chain in the
		 * buffer struct and pass this along from one
		 * descriptor to the next, until we get EOP.
		 */
		if (rxr->hdr_split && (rxbuf->fmp == NULL)) {
			/* This must be an initial descriptor */
			hlen = (hdr & IXGBE_RXDADV_HDRBUFLEN_MASK) >>
			    IXGBE_RXDADV_HDRBUFLEN_SHIFT;
			if (hlen > IXGBE_RX_HDR)
				hlen = IXGBE_RX_HDR;
			mh->m_len = hlen;
			mh->m_pkthdr.len = mh->m_len;
			rxbuf->m_head = NULL;
			/*
			 * Check the payload length, this could be zero if
			 * its a small packet.
			 */
			if (plen > 0) {
				mp->m_len = plen;
				mp->m_flags &= ~M_PKTHDR;
				mh->m_next = mp;
				mh->m_pkthdr.len += mp->m_len;
				rxbuf->m_pack = NULL;
				rxr->rx_split_packets++;
			} else {
				m_freem(mp);
				rxbuf->m_pack = NULL;
			}
			/* Now create the forward chain. */
			if (eop == 0) {
				/* stash the chain head */
				nxbuf->fmp = mh;
				/* Make forward chain */
				if (plen)
					mp->m_next = nxbuf->m_pack;
				else
					mh->m_next = nxbuf->m_pack;
			} else {
				/* Singlet, prepare to send */
				sendmp = mh;
#if NVLAN > 0
				if ((sc->num_vlans) &&
				    (staterr & IXGBE_RXD_STAT_VP)) {
					sendmp->m_pkthdr.ether_vtag = vtag;
					sendmp->m_flags |= M_VLANTAG;
				}
#endif
			}
		} else {
			/*
			 * Either no header split, or a
			 * secondary piece of a fragmented
			 * split packet.
			 */
			mp->m_len = plen;
			/*
			 * See if there is a stored head
			 * that determines what we are
			 */
			sendmp = rxbuf->fmp;
			rxbuf->m_pack = rxbuf->fmp = NULL;

			if (sendmp != NULL) /* secondary frag */
				sendmp->m_pkthdr.len += mp->m_len;
			else {
				 /* first desc of a non-ps chain */
				 sendmp = mp;
				 sendmp->m_pkthdr.len = mp->m_len;
#if NVLAN > 0
				if ((sc->num_vlans) &&
				    (staterr & IXGBE_RXD_STAT_VP)) {
					sendmp->m_pkthdr.ether_vtag = vtag;
					sendmp->m_flags |= M_VLANTAG;
				}
#endif
			}
			/* Pass the head pointer on */
			if (eop == 0) {
				nxbuf->fmp = sendmp;
				sendmp = NULL;
				mp->m_next = nxbuf->m_pack;
			}
		}
		rxr->rx_ndescs--;
		/* Sending this frame? */
		if (eop) {
			m_cluncount(sendmp, 1);

			sendmp->m_pkthdr.rcvif = ifp;
			ifp->if_ipackets++;
			rxr->rx_packets++;
			/* capture data for AIM */
			rxr->bytes += sendmp->m_pkthdr.len;
			rxr->rx_bytes += sendmp->m_pkthdr.len;

			ixgbe_rx_checksum(staterr, sendmp, ptype);

#if NBPFILTER > 0
			if (ifp->if_bpf)
				bpf_mtap_ether(ifp->if_bpf, sendmp,
				    BPF_DIRECTION_IN);
#endif

			ether_input_mbuf(ifp, sendmp);
		}
next_desc:
		bus_dmamap_sync(rxr->rxdma.dma_tag, rxr->rxdma.dma_map,
		    dsize * i, dsize,
		    BUS_DMASYNC_PREREAD);

		/* Advance our pointers to the next descriptor. */
		if (++i == sc->num_rx_desc)
			i = 0;
	}
	rxr->next_to_check = i;

	if (!(staterr & IXGBE_RXD_STAT_DD))
		return FALSE;

	return TRUE;
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid.
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
void
ixgbe_rx_checksum(uint32_t staterr, struct mbuf * mp, uint32_t ptype)
{
	uint16_t status = (uint16_t) staterr;
	uint8_t  errors = (uint8_t) (staterr >> 24);

	if (status & IXGBE_RXD_STAT_IPCS) {
		/* Did it pass? */
		if (!(errors & IXGBE_RXD_ERR_IPE)) {
			/* IP Checksum Good */
			mp->m_pkthdr.csum_flags = M_IPV4_CSUM_IN_OK;
		} else
			mp->m_pkthdr.csum_flags = 0;
	}

	if (status & IXGBE_RXD_STAT_L4CS) {
		/* Did it pass? */
		if (!(errors & IXGBE_RXD_ERR_TCPE))
			mp->m_pkthdr.csum_flags |=
				M_TCP_CSUM_IN_OK | M_UDP_CSUM_IN_OK;
	}

}

void
ixgbe_setup_vlan_hw_support(struct ix_softc *sc)
{
	uint32_t	ctrl;
	int		i;

	/*
	 * We get here thru ixgbe_init, meaning
	 * a soft reset, this has already cleared
	 * the VFTA and other state, so if there
	 * have been no vlan's registered do nothing.
	 */
	if (sc->num_vlans == 0)
		return;

	/*
	 * A soft reset zero's out the VFTA, so
	 * we need to repopulate it now.
	 */
	for (i = 0; i < IXGBE_VFTA_SIZE; i++)
		if (sc->shadow_vfta[i] != 0)
			IXGBE_WRITE_REG(&sc->hw, IXGBE_VFTA(i),
			    sc->shadow_vfta[i]);

	ctrl = IXGBE_READ_REG(&sc->hw, IXGBE_VLNCTRL);
#if 0
	/* Enable the Filter Table if enabled */
	if (ifp->if_capenable & IFCAP_VLAN_HWFILTER) {
		ctrl &= ~IXGBE_VLNCTRL_CFIEN;
		ctrl |= IXGBE_VLNCTRL_VFE;
	}
#endif
	if (sc->hw.mac.type == ixgbe_mac_82598EB)
		ctrl |= IXGBE_VLNCTRL_VME;
	IXGBE_WRITE_REG(&sc->hw, IXGBE_VLNCTRL, ctrl);

	/* On 82599 the VLAN enable is per/queue in RXDCTL */
	if (sc->hw.mac.type != ixgbe_mac_82598EB)
		for (i = 0; i < sc->num_queues; i++) {
			ctrl = IXGBE_READ_REG(&sc->hw, IXGBE_RXDCTL(i));
			ctrl |= IXGBE_RXDCTL_VME;
			IXGBE_WRITE_REG(&sc->hw, IXGBE_RXDCTL(i), ctrl);
		}

}

void
ixgbe_enable_intr(struct ix_softc *sc)
{
	struct ixgbe_hw *hw = &sc->hw;
	struct ix_queue *que = sc->queues;
	uint32_t mask = IXGBE_EIMS_ENABLE_MASK & ~IXGBE_EIMS_RTX_QUEUE;
	int i;

	/* Enable Fan Failure detection */
	if (hw->device_id == IXGBE_DEV_ID_82598AT)
		    mask |= IXGBE_EIMS_GPI_SDP1;
	else {
		mask |= IXGBE_EIMS_ECC;
		mask |= IXGBE_EIMS_GPI_SDP1;
		mask |= IXGBE_EIMS_GPI_SDP2;
	}

	IXGBE_WRITE_REG(hw, IXGBE_EIMS, mask);

	/* With RSS we use auto clear */
	if (sc->msix) {
		mask = IXGBE_EIMS_ENABLE_MASK;
		/* Dont autoclear Link */
		mask &= ~IXGBE_EIMS_OTHER;
		mask &= ~IXGBE_EIMS_LSC;
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIAC, mask);
	}

	/*
	 * Now enable all queues, this is done separately to
	 * allow for handling the extended (beyond 32) MSIX
	 * vectors that can be used by 82599
	 */
	for (i = 0; i < sc->num_queues; i++, que++)
		ixgbe_enable_queue(sc, que->msix);

	IXGBE_WRITE_FLUSH(hw);

	return;
}

void
ixgbe_disable_intr(struct ix_softc *sc)
{
	if (sc->msix)
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIAC, 0);
	if (sc->hw.mac.type == ixgbe_mac_82598EB) {
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMC, ~0);
	} else {
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMC, 0xFFFF0000);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMC_EX(0), ~0);
		IXGBE_WRITE_REG(&sc->hw, IXGBE_EIMC_EX(1), ~0);
	}
	IXGBE_WRITE_FLUSH(&sc->hw);
	return;
}

uint16_t
ixgbe_read_pci_cfg(struct ixgbe_hw *hw, uint32_t reg)
{
	struct pci_attach_args	*pa;
	uint32_t value;
	int high = 0;

	if (reg & 0x2) {
		high = 1;
		reg &= ~0x2;
	}
	pa = ((struct ixgbe_osdep *)hw->back)->os_pa;
	value = pci_conf_read(pa->pa_pc, pa->pa_tag, reg);

	if (high)
		value >>= 16;

	return (value & 0xffff);
}

void
ixgbe_write_pci_cfg(struct ixgbe_hw *hw, uint32_t reg, uint16_t value)
{
	struct pci_attach_args	*pa;
	uint32_t rv;
	int high = 0;

	/* Need to do read/mask/write... because 16 vs 32 bit!!! */
	if (reg & 0x2) {
		high = 1;
		reg &= ~0x2;
	}
	pa = ((struct ixgbe_osdep *)hw->back)->os_pa;
	rv = pci_conf_read(pa->pa_pc, pa->pa_tag, reg);
	if (!high)
		rv = (rv & 0xffff0000) | value;
	else
		rv = (rv & 0xffff) | ((uint32_t)value << 16);
	pci_conf_write(pa->pa_pc, pa->pa_tag, reg, rv);
}

/*
 * Setup the correct IVAR register for a particular MSIX interrupt
 *   (yes this is all very magic and confusing :)
 *  - entry is the register array entry
 *  - vector is the MSIX vector for this queue
 *  - type is RX/TX/MISC
 */
void
ixgbe_set_ivar(struct ix_softc *sc, uint8_t entry, uint8_t vector, int8_t type)
{
	struct ixgbe_hw *hw = &sc->hw;
	uint32_t ivar, index;

	vector |= IXGBE_IVAR_ALLOC_VAL;

	switch (hw->mac.type) {

	case ixgbe_mac_82598EB:
		if (type == -1)
			entry = IXGBE_IVAR_OTHER_CAUSES_INDEX;
		else
			entry += (type * 64);
		index = (entry >> 2) & 0x1F;
		ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(index));
		ivar &= ~(0xFF << (8 * (entry & 0x3)));
		ivar |= (vector << (8 * (entry & 0x3)));
		IXGBE_WRITE_REG(&sc->hw, IXGBE_IVAR(index), ivar);
		break;

	case ixgbe_mac_82599EB:
		if (type == -1) { /* MISC IVAR */
			index = (entry & 1) * 8;
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR_MISC);
			ivar &= ~(0xFF << index);
			ivar |= (vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR_MISC, ivar);
		} else {	/* RX/TX IVARS */
			index = (16 * (entry & 1)) + (8 * type);
			ivar = IXGBE_READ_REG(hw, IXGBE_IVAR(entry >> 1));
			ivar &= ~(0xFF << index);
			ivar |= (vector << index);
			IXGBE_WRITE_REG(hw, IXGBE_IVAR(entry >> 1), ivar);
		}

	default:
		break;
	}
}

void
ixgbe_configure_ivars(struct ix_softc *sc)
{
	struct ix_queue *que = sc->queues;
	uint32_t newitr;
	int i;

#if 0
	if (ixgbe_max_interrupt_rate > 0)
		newitr = (8000000 / ixgbe_max_interrupt_rate) & 0x0FF8;
	else
#endif
		newitr = 0;

        for (i = 0; i < sc->num_queues; i++, que++) {
		/* First the RX queue entry */
		ixgbe_set_ivar(sc, i, que->msix, 0);
		/* ... and the TX */
		ixgbe_set_ivar(sc, i, que->msix, 1);
		/* Set an Initial EITR value */
		IXGBE_WRITE_REG(&sc->hw,
		    IXGBE_EITR(que->msix), newitr);
	}

	/* For the Link interrupt */
        ixgbe_set_ivar(sc, 1, sc->linkvec, -1);
}

/*
 * ixgbe_sfp_probe - called in the local timer to
 * determine if a port had optics inserted.
 */
int
ixgbe_sfp_probe(struct ix_softc *sc)
{
	int result = FALSE;

	if ((sc->hw.phy.type == ixgbe_phy_nl) &&
	    (sc->hw.phy.sfp_type == ixgbe_sfp_type_not_present)) {
		int32_t  ret = sc->hw.phy.ops.identify_sfp(&sc->hw);
		if (ret)
			goto out;
		ret = sc->hw.phy.ops.reset(&sc->hw);
		if (ret == IXGBE_ERR_SFP_NOT_SUPPORTED) {
			printf("%s: Unsupported SFP+ module detected!",
			    sc->dev.dv_xname);
			goto out;
		}
		/* We now have supported optics */
		sc->sfp_probe = FALSE;
		/* Set the optics type so system reports correctly */
		ixgbe_setup_optics(sc);
		result = TRUE;
	}
out:
	return (result);
}

/**********************************************************************
 *
 *  Update the board statistics counters.
 *
 **********************************************************************/
void
ixgbe_update_stats_counters(struct ix_softc *sc)
{
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	struct ixgbe_hw *hw = &sc->hw;
	uint32_t  missed_rx = 0, bprc, lxon, lxoff, total;
	int	i;

	sc->stats.crcerrs += IXGBE_READ_REG(hw, IXGBE_CRCERRS);

	for (i = 0; i < 8; i++) {
		int mp;
		mp = IXGBE_READ_REG(hw, IXGBE_MPC(i));
		missed_rx += mp;
        	sc->stats.mpc[i] += mp;
		if (hw->mac.type == ixgbe_mac_82598EB)
			sc->stats.rnbc[i] += IXGBE_READ_REG(hw, IXGBE_RNBC(i));
	}

	/* Hardware workaround, gprc counts missed packets */
	sc->stats.gprc += IXGBE_READ_REG(hw, IXGBE_GPRC);
	sc->stats.gprc -= missed_rx;

	sc->stats.gorc += IXGBE_READ_REG(hw, IXGBE_GORCH);
	sc->stats.gotc += IXGBE_READ_REG(hw, IXGBE_GOTCH);
	sc->stats.tor += IXGBE_READ_REG(hw, IXGBE_TORH);

	/*
	 * Workaround: mprc hardware is incorrectly counting
	 * broadcasts, so for now we subtract those.
	 */
	bprc = IXGBE_READ_REG(hw, IXGBE_BPRC);
	sc->stats.bprc += bprc;
	sc->stats.mprc += IXGBE_READ_REG(hw, IXGBE_MPRC);
	sc->stats.mprc -= bprc;

	sc->stats.roc += IXGBE_READ_REG(hw, IXGBE_ROC);
	sc->stats.prc64 += IXGBE_READ_REG(hw, IXGBE_PRC64);
	sc->stats.prc127 += IXGBE_READ_REG(hw, IXGBE_PRC127);
	sc->stats.prc255 += IXGBE_READ_REG(hw, IXGBE_PRC255);
	sc->stats.prc511 += IXGBE_READ_REG(hw, IXGBE_PRC511);
	sc->stats.prc1023 += IXGBE_READ_REG(hw, IXGBE_PRC1023);
	sc->stats.prc1522 += IXGBE_READ_REG(hw, IXGBE_PRC1522);
	sc->stats.rlec += IXGBE_READ_REG(hw, IXGBE_RLEC);

	sc->stats.lxonrxc += IXGBE_READ_REG(hw, IXGBE_LXONRXC);
	sc->stats.lxoffrxc += IXGBE_READ_REG(hw, IXGBE_LXOFFRXC);

	lxon = IXGBE_READ_REG(hw, IXGBE_LXONTXC);
	sc->stats.lxontxc += lxon;
	lxoff = IXGBE_READ_REG(hw, IXGBE_LXOFFTXC);
	sc->stats.lxofftxc += lxoff;
	total = lxon + lxoff;

	sc->stats.gptc += IXGBE_READ_REG(hw, IXGBE_GPTC);
	sc->stats.mptc += IXGBE_READ_REG(hw, IXGBE_MPTC);
	sc->stats.ptc64 += IXGBE_READ_REG(hw, IXGBE_PTC64);
	sc->stats.gptc -= total;
	sc->stats.mptc -= total;
	sc->stats.ptc64 -= total;
	sc->stats.gotc -= total * ETHER_MIN_LEN;

	sc->stats.ruc += IXGBE_READ_REG(hw, IXGBE_RUC);
	sc->stats.rfc += IXGBE_READ_REG(hw, IXGBE_RFC);
	sc->stats.rjc += IXGBE_READ_REG(hw, IXGBE_RJC);
	sc->stats.tpr += IXGBE_READ_REG(hw, IXGBE_TPR);
	sc->stats.ptc127 += IXGBE_READ_REG(hw, IXGBE_PTC127);
	sc->stats.ptc255 += IXGBE_READ_REG(hw, IXGBE_PTC255);
	sc->stats.ptc511 += IXGBE_READ_REG(hw, IXGBE_PTC511);
	sc->stats.ptc1023 += IXGBE_READ_REG(hw, IXGBE_PTC1023);
	sc->stats.ptc1522 += IXGBE_READ_REG(hw, IXGBE_PTC1522);
	sc->stats.bptc += IXGBE_READ_REG(hw, IXGBE_BPTC);

#if 0
	/* Fill out the OS statistics structure */
	ifp->if_ipackets = sc->stats.gprc;
	ifp->if_opackets = sc->stats.gptc;
	ifp->if_ibytes = sc->stats.gorc;
	ifp->if_obytes = sc->stats.gotc;
	ifp->if_imcasts = sc->stats.mprc;
#endif
	ifp->if_collisions = 0;
	ifp->if_oerrors = sc->watchdog_events;
	ifp->if_ierrors = missed_rx + sc->stats.crcerrs + sc->stats.rlec;
}

#ifdef IX_DEBUG
/**********************************************************************
 *
 *  This routine is called only when ixgbe_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
void
ixgbe_print_hw_stats(struct ix_softc * sc)
{
	struct ifnet   *ifp = &sc->arpcom.ac_if;;

	printf("%s: mbuf alloc failed %lu, mbuf cluster failed %lu, "
	    "missed pkts %llu, rx len errs %llu, crc errs %llu, "
	    "dropped pkts %lu, watchdog timeouts %ld, "
	    "XON rx %llu, XON tx %llu, XOFF rx %llu, XOFF tx %llu, "
	    "total pkts rx %llu, good pkts rx %llu, good pkts tx %llu, "
	    "tso tx %lu\n",
	    ifp->if_xname,
	    sc->mbuf_alloc_failed,
	    sc->mbuf_cluster_failed,
	    (long long)sc->stats.mpc[0],
	    (long long)sc->stats.roc + (long long)sc->stats.ruc,
	    (long long)sc->stats.crcerrs,
	    sc->dropped_pkts,
	    sc->watchdog_events,
	    (long long)sc->stats.lxonrxc,
	    (long long)sc->stats.lxontxc,
	    (long long)sc->stats.lxoffrxc,
	    (long long)sc->stats.lxofftxc,
	    (long long)sc->stats.tpr,
	    (long long)sc->stats.gprc,
	    (long long)sc->stats.gptc,
	    sc->tso_tx);
}
#endif
