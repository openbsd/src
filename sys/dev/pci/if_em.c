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

/*$FreeBSD: if_em.c,v 1.26 2003/06/05 17:51:37 pdeuskar Exp $*/
/* $OpenBSD: if_em.c,v 1.17 2004/02/12 21:21:06 markus Exp $ */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/device.h>
#include <sys/socket.h>

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

#include <uvm/uvm_extern.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_em.h>

#ifdef DEBUG
#define EM_KASSERT(exp,msg)        do { if (!(exp)) panic msg; } while (0)
#else
#define EM_KASSERT(exp,msg)
#endif

/*********************************************************************
 *  Set this to one to display debug statistics
 *********************************************************************/
int             em_display_debug_stats = 0;

/*********************************************************************
 *  Linked list of board private structures for all NICs found
 *********************************************************************/

struct em_softc *em_adapter_list = NULL;


/*********************************************************************
 *  Driver version
 *********************************************************************/

char em_driver_version[] = "1.6.6";


/*********************************************************************
 *  PCI Device ID Table
 *********************************************************************/
const struct pci_matchid em_devices[] = {
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82542 },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC_SC },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544EI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544EI_SC },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC_LOM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545EM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546EB },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545EM_SC },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546EB_SC },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541EI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EM_LOM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EP_LOM },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EP },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541EP },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82547EI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546EB_QUAD },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EP_LP },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546GB_SERDES },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545GM_COPPER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545GM_FIBER },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545GM_SERDES },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82547GI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541GI },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541GI_MOBILE },
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82541ER }
};

/*********************************************************************
 *  Function prototypes
 *********************************************************************/
int  em_probe(struct device *, void *, void *);
void em_attach(struct device *, struct device *, void *);

#if 0
int  em_detach(void *);
int  em_shutdown(void *);
#endif
int  em_intr(void *);
void em_start(struct ifnet *);
int  em_ioctl(struct ifnet *, u_long, caddr_t);
void em_watchdog(struct ifnet *);
void em_init(void *);
void em_stop(void *);
void em_media_status(struct ifnet *, struct ifmediareq *);
int  em_media_change(struct ifnet *);
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
void em_clean_transmit_interrupts(struct em_softc *);
int  em_allocate_receive_structures(struct em_softc *);
int  em_allocate_transmit_structures(struct em_softc *);
void em_process_receive_interrupts(struct em_softc *, int);
void em_receive_checksum(struct em_softc *, 
				     struct em_rx_desc *,
				     struct mbuf *);
void em_transmit_checksum_setup(struct em_softc *,
					    struct mbuf *,
					    u_int32_t *,
					    u_int32_t *);
void em_set_promisc(struct em_softc *);
void em_disable_promisc(struct em_softc *);
void em_set_multi(struct em_softc *);
void em_print_hw_stats(struct em_softc *);
void em_print_link_status(struct em_softc *);
int  em_get_buf(int i, struct em_softc *,
			    struct mbuf *);
void em_enable_vlans(struct em_softc *);
int  em_encap(struct em_softc *, struct mbuf *);
void em_smartspeed(struct em_softc *);
int  em_82547_fifo_workaround(struct em_softc *, int);
void em_82547_update_fifo_head(struct em_softc *, int);
int  em_82547_tx_fifo_reset(struct em_softc *);
void em_82547_move_tail(void *);
int  em_dma_malloc(struct em_softc *, bus_size_t,
    struct em_dma_alloc *, int);
void em_dma_free(struct em_softc *, struct em_dma_alloc *);
void em_print_debug_info(struct em_softc *);
int  em_is_valid_ether_addr(u_int8_t *);

/*********************************************************************
 *  FreeBSD Device Interface Entry Points		     
 *********************************************************************/

struct cfattach em_ca = {
	sizeof(struct em_softc), em_probe, em_attach
};

struct cfdriver em_cd = {
	0, "em", DV_IFNET
};

/*********************************************************************
 *  Device identification routine
 *
 *  em_probe determines if the driver should be loaded on
 *  adapter based on PCI vendor/device id of the adapter.
 *
 *  return 0 on success, positive on failure
 *********************************************************************/

int
em_probe(struct device *parent, void *match, void *aux)
{
	INIT_DEBUGOUT("em_probe: begin");

	return (pci_matchbyid((struct pci_attach_args *)aux, em_devices,
	    sizeof(em_devices)/sizeof(em_devices[0])));
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
em_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = aux;
#if 0
	pci_chipset_tag_t pc = pa->pa_pc;
#endif
	struct em_softc *sc = (struct em_softc *)self;
	int		s;
	int		tsize, rsize;
	int		error = 0;

	INIT_DEBUGOUT("em_attach: begin");
	s = splimp();

#ifdef __FreeBSD__
	/* Allocate, clear, and link in our sc structure */
	if (!(sc = device_get_softc(dev))) {
		printf("em: sc structure allocation failed\n");
		splx(s);
		return(ENOMEM);
	}
	bzero(sc, sizeof(struct em_softc ));
	sc->dev = dev;
	sc->osdep.dev = dev;
	sc->sc_dv.dv_xname = device_get_unit(dev);
#endif /* __FreeBSD__ */

	sc->osdep.em_pa = *pa;

	if (em_adapter_list != NULL)
		em_adapter_list->prev = sc;
	sc->next = em_adapter_list;
	em_adapter_list = sc;

#ifdef __FreeBSD__
	/* SYSCTL stuff */
	sysctl_ctx_init(&sc->sysctl_ctx);
	sc->sysctl_tree = SYSCTL_ADD_NODE(&sc->sysctl_ctx,
					       SYSCTL_STATIC_CHILDREN(_hw),
					       OID_AUTO,
					       device_get_nameunit(dev),
					       CTLFLAG_RD,
					       0, "");
	if (sc->sysctl_tree == NULL) {
		error = EIO;
		goto err_sysctl;
	}

	SYSCTL_ADD_PROC(&sc->sysctl_ctx,
			SYSCTL_CHILDREN(sc->sysctl_tree),
			OID_AUTO, "debug_info", CTLTYPE_INT|CTLFLAG_RW,
			(void *)sc, 0,
			em_sysctl_debug_info, "I", "Debug Information");

	SYSCTL_ADD_PROC(&sc->sysctl_ctx,
			SYSCTL_CHILDREN(sc->sysctl_tree),
			OID_AUTO, "stats", CTLTYPE_INT|CTLFLAG_RW,
			(void *)sc, 0,
			em_sysctl_stats, "I", "Statistics");

	callout_handle_init(&sc->timer_handle);
	callout_handle_init(&sc->tx_fifo_timer_handle);
#endif /* __FreeBSD__ */

	timeout_set(&sc->timer_handle, em_local_timer, sc);
	timeout_set(&sc->tx_fifo_timer_handle, em_82547_move_tail, sc);

	/* Determine hardware revision */
	em_identify_hardware(sc);

	/* Parameters (to be read from user) */
	sc->num_tx_desc = EM_MAX_TXD;
	sc->num_rx_desc = EM_MAX_RXD;
	sc->tx_int_delay = EM_TIDV;
	sc->tx_abs_int_delay = EM_TADV;
	sc->rx_int_delay = EM_RDTR;
	sc->rx_abs_int_delay = EM_RADV;
	sc->hw.autoneg = DO_AUTO_NEG;
	sc->hw.wait_autoneg_complete = WAIT_FOR_AUTO_NEG_DEFAULT;
	sc->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
	sc->hw.tbi_compatibility_en = TRUE;
	sc->rx_buffer_len = EM_RXBUFFER_2048;

	/*
	 * These parameters control the automatic generation(Tx) and
	 * response(Rx) to Ethernet PAUSE frames.
	 */
	sc->hw.fc_high_water = FC_DEFAULT_HI_THRESH;
	sc->hw.fc_low_water  = FC_DEFAULT_LO_THRESH;
	sc->hw.fc_pause_time = FC_DEFAULT_TX_TIMER;
	sc->hw.fc_send_xon   = TRUE;
	sc->hw.fc = em_fc_full;

	sc->hw.phy_init_script = 1;

	/*
	 * Set the max frame size assuming standard ethernet
	 * sized frames
	 */
	sc->hw.max_frame_size = 
	    ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN;

	sc->hw.min_frame_size = 
	    MINIMUM_ETHERNET_PACKET_SIZE + ETHER_CRC_LEN;

	/*
	 * This controls when hardware reports transmit completion
	 * status.
	 */
	sc->hw.report_tx_early = 1;


	if (em_allocate_pci_resources(sc)) {
		printf("%s: Allocation of PCI resources failed\n", 
		       sc->sc_dv.dv_xname);
		error = ENXIO;
		goto err_pci;
	}


	/* Initialize eeprom parameters */
	em_init_eeprom_params(&sc->hw);

	tsize = EM_ROUNDUP(sc->num_tx_desc *
			   sizeof(struct em_tx_desc), 4096);

	/* Allocate Transmit Descriptor ring */
	if (em_dma_malloc(sc, tsize, &sc->txdma, BUS_DMA_NOWAIT)) {
		printf("%s: Unable to allocate tx_desc memory\n", 
		       sc->sc_dv.dv_xname);
		error = ENOMEM;
		goto err_tx_desc;
	}
	sc->tx_desc_base = (struct em_tx_desc *)sc->txdma.dma_vaddr;

	rsize = EM_ROUNDUP(sc->num_rx_desc *
			   sizeof(struct em_rx_desc), 4096);

	/* Allocate Receive Descriptor ring */
	if (em_dma_malloc(sc, rsize, &sc->rxdma, BUS_DMA_NOWAIT)) {
		printf("%s: Unable to allocate rx_desc memory\n", 
		       sc->sc_dv.dv_xname);
		error = ENOMEM;
		goto err_rx_desc;
	}
	sc->rx_desc_base = (struct em_rx_desc *) sc->rxdma.dma_vaddr;

	/* Initialize the hardware */
	if (em_hardware_init(sc)) {
		printf("%s: Unable to initialize the hardware\n",
		       sc->sc_dv.dv_xname);
		error = EIO;
		goto err_hw_init;
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (em_read_mac_addr(&sc->hw) < 0) {
		printf("%s: EEPROM read error while reading mac address\n",
		       sc->sc_dv.dv_xname);
		error = EIO;
		goto err_mac_addr;
	}

	if (!em_is_valid_ether_addr(sc->hw.mac_addr)) {
		printf("%s: Invalid mac address\n", sc->sc_dv.dv_xname);
		error = EIO;
		goto err_mac_addr;
	}

	bcopy(sc->hw.mac_addr, sc->interface_data.ac_enaddr,
	      ETHER_ADDR_LEN);

	printf(", address: %s\n", ether_sprintf(sc->interface_data.ac_enaddr));

	/* Setup OS specific network interface */
	em_setup_interface(sc);

	/* Initialize statistics */
	em_clear_hw_cntrs(&sc->hw);
	em_update_stats_counters(sc);
	sc->hw.get_link_status = 1;
	em_check_for_link(&sc->hw);

	/* Print the link status */
	if (sc->link_active == 1) {
		em_get_speed_and_duplex(&sc->hw, &sc->link_speed, 
					&sc->link_duplex);
	}

	INIT_DEBUGOUT("em_attach: end");
	splx(s);
	return;

err_mac_addr:
err_hw_init:
	em_dma_free(sc, &sc->rxdma);
err_rx_desc:
	em_dma_free(sc, &sc->txdma);
err_tx_desc:
err_pci:
	em_free_pci_resources(sc);
#ifdef __FreeBSD__
	sysctl_ctx_free(&sc->sysctl_ctx);
#endif /* __FreeBSD__ */
/*err_sysctl:*/
	splx(s);
	return;

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
#ifdef __FreeBSD__
int
em_detach(void* arg)
{
	struct em_softc *sc = arg;
	struct ifnet   *ifp = &sc->interface_data.ac_if;
	int		s;

	INIT_DEBUGOUT("em_detach: begin");
	s = splimp();

	em_stop(sc);
	em_phy_hw_reset(&sc->hw);
#if __FreeBSD_version < 500000
	ether_ifdetach(&sc->interface_data.ac_if, ETHER_BPF_SUPPORTED);
#else
	ether_ifdetach(&sc->interface_data.ac_if);
#endif
	em_free_pci_resources(sc);

	/* Free Transmit Descriptor ring */
	if (sc->tx_desc_base) {
		em_dma_free(sc, &sc->txdma);
		sc->tx_desc_base = NULL;
	}

	/* Free Receive Descriptor ring */
	if (sc->rx_desc_base) {
		em_dma_free(sc, &sc->rxdma);
		sc->rx_desc_base = NULL;
	}

	/* Remove from the adapter list */
	if (em_adapter_list == sc)
		em_adapter_list = sc->next;
	if (sc->next != NULL)
		sc->next->prev = sc->prev;
	if (sc->prev != NULL)
		sc->prev->next = sc->next;

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	splx(s);
	return(0);
}

/*********************************************************************
 *
 *  Shutdown entry point
 *
 **********************************************************************/

int
em_shutdown(void* arg)
{
	struct em_softc *sc = arg;
	em_stop(sc);
	return(0);
}

#endif /* __FreeBSD__ */

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
	int		s;
	struct mbuf    *m_head;
	struct em_softc *sc = ifp->if_softc;

	if (!sc->link_active)
		return;

	s = splimp();	   

	for (;;) {
		IFQ_POLL(&ifp->if_snd, m_head);

		if (m_head == NULL) break;

		if (em_encap(sc, m_head)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m_head);

#if NBPFILTER > 0
		/* Send a copy of the frame to the BPF listener */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif

		/* Set timeout in case hardware has problems transmitting */
		ifp->if_timer = EM_TX_TIMEOUT;

	}	
	splx(s);
	return;
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
	int		s, error = 0;
	struct ifreq   *ifr = (struct ifreq *) data;
	struct ifaddr  *ifa = (struct ifaddr *)data;
	struct em_softc * sc = ifp->if_softc;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->interface_data, command, data)) > 0) {
		splx(s);
		return (error);
	}

	switch (command) {
	case SIOCSIFADDR:
#ifdef __FreeBSD__
	case SIOCGIFADDR:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFADDR (Get/Set Interface Addr)");
		ether_ioctl(ifp, command, data);
		break;
#endif /* __FreeBSD__ */
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFADDR (Set Interface "
			       "Addr)");
		ifp->if_flags |= IFF_UP;
		em_init(sc);
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->interface_data, ifa);
			break;
#endif /* INET */
		default:
			break;
		}
		break;
	case SIOCSIFMTU:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFMTU (Set Interface MTU)");
		if (ifr->ifr_mtu > MAX_JUMBO_FRAME_SIZE - ETHER_HDR_LEN) {
			error = EINVAL;
		} else {
			ifp->if_mtu = ifr->ifr_mtu;
			sc->hw.max_frame_size = 
			ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;
			em_init(sc);
		}
		break;
	case SIOCSIFFLAGS:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFFLAGS (Set Interface Flags)");
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				em_init(sc);

			em_disable_promisc(sc);
			em_set_promisc(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				em_stop(sc);
			}
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
		error = (command == SIOCADDMULTI)
			? ether_addmulti(ifr, &sc->interface_data)
			: ether_delmulti(ifr, &sc->interface_data);

		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING) {
				em_disable_intr(sc);
				em_set_multi(sc);
				if (sc->hw.mac_type == em_82542_rev2_0) {
					em_initialize_receive_unit(sc);
				}
#ifdef DEVICE_POLLING
				if (!(ifp->if_ipending & IFF_POLLING))
#endif
					em_enable_intr(sc);
			}
			error = 0;
		}
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFMEDIA (Get/Set Interface Media)");
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;
#ifdef __FreeBSD__
	case SIOCSIFCAP:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFCAP (Set Capabilities)");
		mask = ifr->ifr_reqcap ^ ifp->if_capenable;
		if (mask & IFCAP_HWCSUM) {
			if (IFCAP_HWCSUM & ifp->if_capenable)
				ifp->if_capenable &= ~IFCAP_HWCSUM;
			else
				ifp->if_capenable |= IFCAP_HWCSUM;
			if (ifp->if_flags & IFF_RUNNING)
				em_init(sc);
		}
		break;
#endif /* __FreeBSD__ */
	default:
		IOCTL_DEBUGOUT1("ioctl received: UNKNOWN (0x%d)\n", (int)command);
		error = EINVAL;
	}

	splx(s);
	return(error);
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
	struct em_softc * sc;
	sc = ifp->if_softc;

	/* If we are in this routine because of pause frames, then
	 * don't reset the hardware.
	 */
	if (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_TXOFF) {
		ifp->if_timer = EM_TX_TIMEOUT;
		return;
	}

	printf("%s: watchdog timeout -- resetting\n", sc->sc_dv.dv_xname);

	ifp->if_flags &= ~IFF_RUNNING;

	em_stop(sc);
	em_init(sc);

	ifp->if_oerrors++;
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

void
em_init(void *arg)
{
	int		s;
	struct ifnet   *ifp;
	struct em_softc * sc = arg;

	INIT_DEBUGOUT("em_init: begin");

	s = splimp();

	em_stop(sc);

	/* Initialize the hardware */
	if (em_hardware_init(sc)) {
		printf("%s: Unable to initialize the hardware\n", 
		       sc->sc_dv.dv_xname);
		splx(s);
		return;
	}

	/* em_enable_vlans(sc); */

	/* Prepare transmit descriptors and buffers */
	if (em_setup_transmit_structures(sc)) {
		printf("%s: Could not setup transmit structures\n", 
		       sc->sc_dv.dv_xname);
		em_stop(sc); 
		splx(s);
		return;
	}
	em_initialize_transmit_unit(sc);

	/* Setup Multicast table */
	em_set_multi(sc);

	/* Prepare receive descriptors and buffers */
	if (em_setup_receive_structures(sc)) {
		printf("%s: Could not setup receive structures\n", 
		       sc->sc_dv.dv_xname);
		em_stop(sc);
		splx(s);
		return;
	}
	em_initialize_receive_unit(sc);

	ifp = &sc->interface_data.ac_if;
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

#ifdef __FreeBSD__
	if (sc->hw.mac_type >= em_82543) {
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist = EM_CHECKSUM_FEATURES;
		else
			ifp->if_hwassist = 0;
	}
#endif /* __FreeBSD__ */

	timeout_add(&sc->timer_handle, 2*hz);
	em_clear_hw_cntrs(&sc->hw);
#ifdef DEVICE_POLLING
        /*
         * Only enable interrupts if we are not polling, make sure
         * they are off otherwise.
         */
        if (ifp->if_ipending & IFF_POLLING)
                em_disable_intr(sc);
        else
#endif /* DEVICE_POLLING */
		em_enable_intr(sc);

	/* Don't reset the phy next time init gets called */
	sc->hw.phy_reset_disable = TRUE;

	splx(s);
	return;
}


#ifdef DEVICE_POLLING
static poll_handler_t em_poll;

static void
em_poll(struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct em_softc *sc = ifp->if_softc;
	u_int32_t reg_icr;

	if (cmd == POLL_DEREGISTER) {	    /* final call, enable interrupts */
		em_enable_intr(sc);
		return;
	}
	if (cmd == POLL_AND_CHECK_STATUS) {
		reg_icr = E1000_READ_REG(&sc->hw, ICR);
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			untimeout(em_local_timer, sc, sc->timer_handle);
			sc->hw.get_link_status = 1;
			em_check_for_link(&sc->hw);
			em_print_link_status(sc);
			sc->timer_handle = timeout(em_local_timer, sc, 2*hz);
		}
	}
	if (ifp->if_flags & IFF_RUNNING) {
		em_process_receive_interrupts(sc, count);
		em_clean_transmit_interrupts(sc);
	}

	if (ifp->if_flags & IFF_RUNNING && ifp->if_snd.ifq_head != NULL)
		em_start(ifp);
}
#endif /* DEVICE_POLLING */

/*********************************************************************
 *
 *  Interrupt Service routine
 *
 **********************************************************************/
int 
em_intr(void *arg)
{
	u_int32_t	loop_cnt = EM_MAX_INTR;
	u_int32_t	reg_icr;
	struct ifnet	*ifp;
	struct em_softc *sc = arg;

	ifp = &sc->interface_data.ac_if;

#ifdef DEVICE_POLLING
	if (ifp->if_ipending & IFF_POLLING)
		return;

	if (ether_poll_register(em_poll, ifp)) {
		em_disable_intr(sc);
		em_poll(ifp, 0, 1);
		return;
	}
#endif /* DEVICE_POLLING */
	reg_icr = E1000_READ_REG(&sc->hw, ICR);
	if (!reg_icr) {
		return (0);
	}

	/* Link status change */
	if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
		timeout_del(&sc->timer_handle);
		sc->hw.get_link_status = 1;
		em_check_for_link(&sc->hw);
		em_print_link_status(sc);
		timeout_add(&sc->timer_handle, 2*hz); 
	}

	while (loop_cnt > 0) {
		if (ifp->if_flags & IFF_RUNNING) {
			em_process_receive_interrupts(sc, -1);
			em_clean_transmit_interrupts(sc);
		}
		loop_cnt--;
	}

	if (ifp->if_flags & IFF_RUNNING && IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		em_start(ifp);

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
em_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct em_softc * sc= ifp->if_softc;

	INIT_DEBUGOUT("em_media_status: begin");

	em_check_for_link(&sc->hw);
	if (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_LU) {
		if (sc->link_active == 0) {
			em_get_speed_and_duplex(&sc->hw, 
						&sc->link_speed, 
						&sc->link_duplex);
			sc->link_active = 1;
		}
	} else {
		if (sc->link_active == 1) {
			sc->link_speed = 0;
			sc->link_duplex = 0;
			sc->link_active = 0;
		}
	}

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (!sc->link_active)
		return;

	ifmr->ifm_status |= IFM_ACTIVE;

	if (sc->hw.media_type == em_media_type_fiber) {
		ifmr->ifm_active |= IFM_1000_SX | IFM_FDX;
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
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
	}
	return;
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
	struct em_softc * sc = ifp->if_softc;
	struct ifmedia	*ifm = &sc->media;

	INIT_DEBUGOUT("em_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		sc->hw.autoneg = DO_AUTO_NEG;
		sc->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
		break;
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
			sc->hw.forced_speed_duplex	= em_100_half;
		break;
	case IFM_10_T:
		sc->hw.autoneg = FALSE;
		sc->hw.autoneg_advertised = 0;
		if ((ifm->ifm_media & IFM_GMASK) == IFM_FDX)
			sc->hw.forced_speed_duplex = em_10_full;
		else
			sc->hw.forced_speed_duplex	= em_10_half;
		break;
	default:
		printf("%s: Unsupported media type\n", sc->sc_dv.dv_xname);
	}

	/* As the speed/duplex settings my have changed we need to
	 * reset the PHY.
	 */
	sc->hw.phy_reset_disable = FALSE;

	em_init(sc);

	return(0);
}

#ifdef __FreeBSD__
void
em_tx_cb(void *arg, bus_dma_segment_t *seg, int nsegs, bus_size_t mapsize, int error)
{
	struct em_q *q = arg;

	if (error)
		return;
	EM_KASSERT(nsegs <= EM_MAX_SCATTER,
		("Too many DMA segments returned when mapping tx packet"));
	q->nsegs = nsegs;
	bcopy(seg, q->segs, nsegs * sizeof(seg[0]));
}
#endif /* __FreeBSD__ */

#define EM_FIFO_HDR		 0x10
#define EM_82547_PKT_THRESH	 0x3e0
#define EM_82547_TX_FIFO_SIZE	 0x2800
#define EM_82547_TX_FIFO_BEGIN	 0xf00
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
	u_int32_t	txd_lower;
	int		i, j, error;
#if NVLAN > 0
	struct ifvlan *ifv = NULL;
#endif
	struct em_q	q;

	struct em_buffer   *tx_buffer = NULL;
	struct em_tx_desc *current_tx_desc = NULL;
	/*struct ifnet	 *ifp = &sc->interface_data.ac_if;*/

	/*
	 * Force a cleanup if number of TX descriptors
	 * available hits the threshold
	 */
	if (sc->num_tx_desc_avail <= EM_TX_CLEANUP_THRESHOLD) {
		em_clean_transmit_interrupts(sc);
		if (sc->num_tx_desc_avail <= EM_TX_CLEANUP_THRESHOLD) {
			sc->no_tx_desc_avail1++;
			return (ENOBUFS);
		}
	}

	/*
	 * Map the packet for DMA.
	 */
	if (bus_dmamap_create(sc->txtag, MCLBYTES, 32, 0, 0, BUS_DMA_NOWAIT,
	    &q.map)) {
		sc->no_tx_map_avail++;
		return (ENOMEM);
	}
	error = bus_dmamap_load_mbuf(sc->txtag, q.map,
				     m_head, BUS_DMA_NOWAIT);
	if (error != 0) {
		sc->no_tx_dma_setup++;
		bus_dmamap_destroy(sc->txtag, q.map);
		return (error);
	}
	EM_KASSERT(q.map->dm_nsegs!= 0, ("em_encap: empty packet"));

	if (q.map->dm_nsegs > sc->num_tx_desc_avail) {
		sc->no_tx_desc_avail2++;
		bus_dmamap_destroy(sc->txtag, q.map);
		return (ENOBUFS);
	}


#ifdef __FreeBSD__
	if (ifp->if_hwassist > 0) {
		em_transmit_checksum_setup(sc,	m_head,
					   &txd_upper, &txd_lower);
	} else
#endif /* __FreeBSD__ */
		txd_upper = txd_lower = 0;


	/* Find out if we are in vlan mode */
#if NVLAN > 0
	if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL &&
	    m_head->m_pkthdr.rcvif->if_type == IFT_L2VLAN)
		ifv = m_head->m_pkthdr.rcvif->if_softc;
#endif

	i = sc->next_avail_tx_desc;
	for (j = 0; j < q.map->dm_nsegs; j++) {
		tx_buffer = &sc->tx_buffer_area[i];
		current_tx_desc = &sc->tx_desc_base[i];

		current_tx_desc->buffer_addr = htole64(q.map->dm_segs[j].ds_addr);
		current_tx_desc->lower.data = htole32(
		    sc->txd_cmd | txd_lower | q.map->dm_segs[j].ds_len);
		current_tx_desc->upper.data = htole32(txd_upper);

		if (++i == sc->num_tx_desc)
			i = 0;
		
		tx_buffer->m_head = NULL;
	}

	sc->num_tx_desc_avail -= q.map->dm_nsegs;
	sc->next_avail_tx_desc = i;

#if NVLAN > 0
	if (ifv != NULL) {
		/* Set the vlan id */
		current_tx_desc->upper.fields.special = htole16(ifv->ifv_tag);

		/* Tell hardware to add tag */
		current_tx_desc->lower.data |= htole32(E1000_TXD_CMD_VLE);
	}
#endif

	tx_buffer->m_head = m_head;
	tx_buffer->map = q.map;
	bus_dmamap_sync(sc->txtag, q.map, 0, q.map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* 
	 * Last Descriptor of Packet needs End Of Packet (EOP) 
	 */
	current_tx_desc->lower.data |= htole32(E1000_TXD_CMD_EOP);

	/* 
	 * Advance the Transmit Descriptor Tail (Tdt), this tells the E1000
	 * that this frame is available to transmit.
	 */
	if (sc->hw.mac_type == em_82547 &&
	    sc->link_duplex == HALF_DUPLEX) {
		em_82547_move_tail(sc);
	} else {
		E1000_WRITE_REG(&sc->hw, TDT, i);
		if (sc->hw.mac_type == em_82547) {
			em_82547_update_fifo_head(sc, m_head->m_pkthdr.len);
		}
	}

	return (0);
}

/*********************************************************************
 *
 * 82547 workaround to avoid controller hang in half-duplex environment.
 * The workaround is to avoid queuing a large packet that would span
 * the internal Tx FIFO ring boundary. We need to reset the FIFO pointers
 * in this case. We do that only when FIFO is queiced.
 *
 **********************************************************************/
void
em_82547_move_tail(void *arg)
{
	int s;
	struct em_softc *sc = arg;
	uint16_t hw_tdt;
	uint16_t sw_tdt;
	struct em_tx_desc *tx_desc;
	uint16_t length = 0;
	boolean_t eop = 0;

	s = splimp();
	hw_tdt = E1000_READ_REG(&sc->hw, TDT);
	sw_tdt = sc->next_avail_tx_desc;

	while (hw_tdt != sw_tdt) {
		tx_desc = &sc->tx_desc_base[hw_tdt];
		length += tx_desc->lower.flags.length;
		eop = tx_desc->lower.data & E1000_TXD_CMD_EOP;
		if(++hw_tdt == sc->num_tx_desc)
			hw_tdt = 0;

		if(eop) {
			if (em_82547_fifo_workaround(sc, length)) {
				sc->tx_fifo_wrk++;
				timeout_add(&sc->tx_fifo_timer_handle, 1);
				splx(s);
				return;
			}
			else {
				E1000_WRITE_REG(&sc->hw, TDT, hw_tdt);
				em_82547_update_fifo_head(sc, length);
				length = 0;
			}
		}
	}
	splx(s);
	return;
}

int
em_82547_fifo_workaround(struct em_softc *sc, int len)
{
	int fifo_space, fifo_pkt_len;

	fifo_pkt_len = EM_ROUNDUP(len + EM_FIFO_HDR, EM_FIFO_HDR);

	if (sc->link_duplex == HALF_DUPLEX) {
		fifo_space = EM_82547_TX_FIFO_SIZE - sc->tx_fifo_head;

		if (fifo_pkt_len >= (EM_82547_PKT_THRESH + fifo_space)) {
			if (em_82547_tx_fifo_reset(sc)) {
				return(0);
			}
			else {
				return(1);
			}
		}
	}

	return(0);
}

void
em_82547_update_fifo_head(struct em_softc *sc, int len)
{
	int fifo_pkt_len = EM_ROUNDUP(len + EM_FIFO_HDR, EM_FIFO_HDR);

	/* tx_fifo_head is always 16 byte aligned */
	sc->tx_fifo_head += fifo_pkt_len;
	if (sc->tx_fifo_head >= EM_82547_TX_FIFO_SIZE) {
		sc->tx_fifo_head -= EM_82547_TX_FIFO_SIZE;
	}

	return;
}


int
em_82547_tx_fifo_reset(struct em_softc *sc)
{
	uint32_t tctl;

	if ( (E1000_READ_REG(&sc->hw, TDT) ==
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
		E1000_WRITE_REG(&sc->hw, TDFT, EM_82547_TX_FIFO_BEGIN);
		E1000_WRITE_REG(&sc->hw, TDFH, EM_82547_TX_FIFO_BEGIN);
		E1000_WRITE_REG(&sc->hw, TDFTS, EM_82547_TX_FIFO_BEGIN);
		E1000_WRITE_REG(&sc->hw, TDFHS, EM_82547_TX_FIFO_BEGIN);

		/* Re-enable TX unit */
		E1000_WRITE_REG(&sc->hw, TCTL, tctl);
		E1000_WRITE_FLUSH(&sc->hw);

		sc->tx_fifo_head = 0;
		sc->tx_fifo_reset++;

		return(TRUE);
	}
	else {
		return(FALSE);
	}
}

void
em_set_promisc(struct em_softc * sc)
{

	u_int32_t	reg_rctl;
	struct ifnet   *ifp = &sc->interface_data.ac_if;

	reg_rctl = E1000_READ_REG(&sc->hw, RCTL);

	if (ifp->if_flags & IFF_PROMISC) {
		reg_rctl |= (E1000_RCTL_UPE | E1000_RCTL_MPE);
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
	} else if (ifp->if_flags & IFF_ALLMULTI) {
		reg_rctl |= E1000_RCTL_MPE;
		reg_rctl &= ~E1000_RCTL_UPE;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
	}

	return;
}

void
em_disable_promisc(struct em_softc * sc)
{
	u_int32_t	reg_rctl;

	reg_rctl = E1000_READ_REG(&sc->hw, RCTL);

	reg_rctl &=  (~E1000_RCTL_UPE);
	reg_rctl &=  (~E1000_RCTL_MPE);
	E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);

	return;
}


/*********************************************************************
 *  Multicast Update
 *
 *  This routine is called whenever multicast address list is updated.
 *
 **********************************************************************/

void
em_set_multi(struct em_softc * sc)
{
	u_int32_t reg_rctl = 0;
	u_int8_t  mta[MAX_NUM_MULTICAST_ADDRESSES * ETH_LENGTH_OF_ADDRESS];
	struct arpcom *ac = &sc->interface_data;
	struct ether_multi *enm;
	struct ether_multistep step;
	int mcnt = 0;
	struct ifnet *ifp = &sc->interface_data.ac_if;

	IOCTL_DEBUGOUT("em_set_multi: begin");

	if (sc->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		if (sc->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
			em_pci_clear_mwi(&sc->hw);
		}
		reg_rctl |= E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		msec_delay(5);
	}

	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			mcnt = MAX_NUM_MULTICAST_ADDRESSES;
		}
		if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
			break;
		bcopy(enm->enm_addrlo, &mta[mcnt*ETH_LENGTH_OF_ADDRESS],
		      ETH_LENGTH_OF_ADDRESS);
		mcnt++;
		ETHER_NEXT_MULTI(step, enm);
	}

	if (mcnt >= MAX_NUM_MULTICAST_ADDRESSES) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		reg_rctl |= E1000_RCTL_MPE;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
	} else
		em_mc_addr_list_update(&sc->hw, mta, mcnt, 0);

	if (sc->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		reg_rctl &= ~E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		msec_delay(5);
		if (sc->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
			em_pci_set_mwi(&sc->hw);
		}
	}

	return;
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
	int s;
	struct ifnet   *ifp;
	struct em_softc * sc = arg;
	ifp = &sc->interface_data.ac_if;

	s = splimp();

	em_check_for_link(&sc->hw);
	em_print_link_status(sc);
	em_update_stats_counters(sc);	
	if (em_display_debug_stats && ifp->if_flags & IFF_RUNNING) {
		em_print_hw_stats(sc);
	}
	em_smartspeed(sc);

	timeout_add(&sc->timer_handle, 2*hz);

	splx(s);
	return;
}

void
em_print_link_status(struct em_softc * sc)
{
	if (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_LU) {
		if (sc->link_active == 0) {
			em_get_speed_and_duplex(&sc->hw, 
						&sc->link_speed, 
						&sc->link_duplex);
			sc->link_active = 1;
			sc->smartspeed = 0;
		}
	} else {
		if (sc->link_active == 1) {
			sc->link_speed = 0;
			sc->link_duplex = 0;
			sc->link_active = 0;
		}
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
em_stop(void *arg)
{
	struct ifnet   *ifp;
	struct em_softc * sc = arg;
	ifp = &sc->interface_data.ac_if;

	INIT_DEBUGOUT("em_stop: begin\n");
	em_disable_intr(sc);
	em_reset_hw(&sc->hw);
	timeout_del(&sc->timer_handle);
	timeout_del(&sc->tx_fifo_timer_handle);
	em_free_transmit_structures(sc);
	em_free_receive_structures(sc);


	/* Tell the stack that the interface is no longer active */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}


/*********************************************************************
 *
 *  Determine hardware revision.
 *
 **********************************************************************/
void
em_identify_hardware(struct em_softc * sc)
{
	u_int32_t reg;
	struct pci_attach_args *pa = &sc->osdep.em_pa;

	/* Make sure our PCI config space has the necessary stuff set */
	sc->hw.pci_cmd_word = pci_conf_read(pa->pa_pc, pa->pa_tag,
					    PCI_COMMAND_STATUS_REG);
	if (!((sc->hw.pci_cmd_word & PCI_COMMAND_MASTER_ENABLE) &&
	      (sc->hw.pci_cmd_word & PCI_COMMAND_MEM_ENABLE))) {
		printf("%s: Memory Access and/or Bus Master bits were not set!\n", 
		       sc->sc_dv.dv_xname);
		sc->hw.pci_cmd_word |= 
		(PCI_COMMAND_MASTER_ENABLE | PCI_COMMAND_MEM_ENABLE);
		pci_conf_write(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG,
			       sc->hw.pci_cmd_word);
	}

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

	return;
}

int
em_allocate_pci_resources(struct em_softc * sc)
{
	int		i, val, rid;
	pci_intr_handle_t	ih;
	const char		*intrstr = NULL;
	struct pci_attach_args *pa = &sc->osdep.em_pa;
	pci_chipset_tag_t	pc = pa->pa_pc;

	val = pci_conf_read(pa->pa_pc, pa->pa_tag, EM_MMBA);
	if (PCI_MAPREG_TYPE(val) != PCI_MAPREG_TYPE_MEM) {
		printf(": mmba isn't memory");
		return (ENXIO);
	}
	if (pci_mapreg_map(pa, EM_MMBA, PCI_MAPREG_MEM_TYPE(val), 0,
	    &sc->osdep.mem_bus_space_tag, &sc->osdep.mem_bus_space_handle,
	    &sc->osdep.em_membase, &sc->osdep.em_memsize, 0)) {
		printf(": can't find mem space");
		return (ENXIO);
	}

	if (sc->hw.mac_type > em_82543) {
		/* Figure our where our IO BAR is ? */
		rid = EM_MMBA;
		for (i = 0; i < 5; i++) {
			val = pci_conf_read(pa->pa_pc, pa->pa_tag, rid);
			if (val & 0x00000001) {
				sc->io_rid = rid;
				break;
			}
			rid += 4;
		}
		if (pci_mapreg_map(pa, rid, PCI_MAPREG_TYPE_IO, 0,
				   &sc->osdep.em_iobtag,
				   &sc->osdep.em_iobhandle,
				   &sc->osdep.em_iobase,
				   &sc->osdep.em_iosize, 0)) {
			printf(": can't find io space");
			return (ENXIO);
		}
	}

	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		return (ENXIO);
	}

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
		
	sc->hw.back = &sc->osdep;

	return(0);
}

void
em_free_pci_resources(struct em_softc* sc)
{
	struct pci_attach_args *pa = &sc->osdep.em_pa;
	pci_chipset_tag_t	pc = pa->pa_pc;

	if(sc->sc_intrhand)
		pci_intr_disestablish(pc, sc->sc_intrhand);
	sc->sc_intrhand = 0;

	if(sc->osdep.em_iobase)
		bus_space_unmap(sc->osdep.em_iobtag, sc->osdep.em_iobhandle,
				sc->osdep.em_iosize);
	sc->osdep.em_iobase = 0;

	if(sc->osdep.em_membase)
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
em_hardware_init(struct em_softc * sc)
{
	/* Issue a global reset */
	em_reset_hw(&sc->hw);

	/* When hardware is reset, fifo_head is also reset */
	sc->tx_fifo_head = 0;

	/* Make sure we have a good EEPROM before we read from it */
	if (em_validate_eeprom_checksum(&sc->hw) < 0) {
		printf("%s: The EEPROM Checksum Is Not Valid\n",
		       sc->sc_dv.dv_xname);
		return(EIO);
	}

	if (em_read_part_num(&sc->hw, &(sc->part_num)) < 0) {
		printf("%s: EEPROM read error while reading part number\n",
		       sc->sc_dv.dv_xname);
		return(EIO);
	}

	if (em_init_hw(&sc->hw) < 0) {
		printf("%s: Hardware Initialization Failed",
		       sc->sc_dv.dv_xname);
		return(EIO);
	}

	em_check_for_link(&sc->hw);
	if (E1000_READ_REG(&sc->hw, STATUS) & E1000_STATUS_LU)
		sc->link_active = 1;
	else
		sc->link_active = 0;

	if (sc->link_active) {
		em_get_speed_and_duplex(&sc->hw, 
					&sc->link_speed, 
					&sc->link_duplex);
	} else {
		sc->link_speed = 0;
		sc->link_duplex = 0;
	}

	return(0);
}

/*********************************************************************
 *
 *  Setup networking device structure and register an interface.
 *
 **********************************************************************/
void
em_setup_interface(struct em_softc * sc)
{
	struct ifnet   *ifp;
	INIT_DEBUGOUT("em_setup_interface: begin");

	ifp = &sc->interface_data.ac_if;
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;
	ifp->if_baudrate = 1000000000;
#ifdef __FreeBSD__
	ifp->if_init =	em_init;
#endif
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = em_ioctl;
	ifp->if_start = em_start;
	ifp->if_watchdog = em_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->num_tx_desc - 1);
	IFQ_SET_READY(&ifp->if_snd);

	strlcpy(ifp->if_xname, sc->sc_dv.dv_xname, IFNAMSIZ);
	
#ifdef __FreeBSD__
	if (sc->hw.mac_type >= em_82543) {
		ifp->if_capabilities = IFCAP_HWCSUM;
		ifp->if_capenable = ifp->if_capabilities;
	}

	/*
	 * Tell the upper layer(s) we support long frames.
	 */
	ifp->if_data.ifi_hdrlen = sizeof(struct ehter_vlan_header);
#if __FreeBSD_version >= 500000
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING | IFCAP_VLAN_MTU;
#endif
#endif /* __FreeBSD__ */

#ifdef __OpenBSD__
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
#endif

	/* 
	 * Specify the media types supported by this adapter and register
	 * callbacks to update media and link information
	 */
	ifmedia_init(&sc->media, IFM_IMASK, em_media_change,
		     em_media_status);
	if (sc->hw.media_type == em_media_type_fiber) {
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_SX | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_SX, 
			    0, NULL);
	} else {
		ifmedia_add(&sc->media, IFM_ETHER | IFM_10_T, 0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_10_T | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_100_TX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_100_TX | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
	}
	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	return;
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
 
	if(sc->link_active || (sc->hw.phy_type != em_phy_igp) || 
	   !sc->hw.autoneg || !(sc->hw.autoneg_advertised & ADVERTISE_1000_FULL))
		return;

	if(sc->smartspeed == 0) {
		/* If Master/Slave config fault is asserted twice,
		 * we assume back-to-back */
		em_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &phy_tmp);
		if(!(phy_tmp & SR_1000T_MS_CONFIG_FAULT)) return;
		em_read_phy_reg(&sc->hw, PHY_1000T_STATUS, &phy_tmp);
		if(phy_tmp & SR_1000T_MS_CONFIG_FAULT) {
			em_read_phy_reg(&sc->hw, PHY_1000T_CTRL,
					&phy_tmp);
			if(phy_tmp & CR_1000T_MS_ENABLE) {
				phy_tmp &= ~CR_1000T_MS_ENABLE;
				em_write_phy_reg(&sc->hw,
						    PHY_1000T_CTRL, phy_tmp);
				sc->smartspeed++;
				if(sc->hw.autoneg &&
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
	} else if(sc->smartspeed == EM_SMARTSPEED_DOWNSHIFT) {
		/* If still no link, perhaps using 2/3 pair cable */
		em_read_phy_reg(&sc->hw, PHY_1000T_CTRL, &phy_tmp);
		phy_tmp |= CR_1000T_MS_ENABLE;
		em_write_phy_reg(&sc->hw, PHY_1000T_CTRL, phy_tmp);
		if(sc->hw.autoneg &&
		   !em_phy_setup_autoneg(&sc->hw) &&
		   !em_read_phy_reg(&sc->hw, PHY_CTRL, &phy_tmp)) {
			phy_tmp |= (MII_CR_AUTO_NEG_EN |
				    MII_CR_RESTART_AUTO_NEG);
			em_write_phy_reg(&sc->hw, PHY_CTRL, phy_tmp);
		}
	}
	/* Restart process after EM_SMARTSPEED_MAX iterations */
	if(sc->smartspeed++ == EM_SMARTSPEED_MAX)
		sc->smartspeed = 0;

	return;
}


/*
 * Manage DMA'able memory.
 */

#ifdef __FreeBSD__
void
em_dmamap_cb(void *arg, bus_dma_segment_t *segs, int nseg, int error)
{ 
	if (error)
		return;
	*(bus_addr_t*) arg = segs->ds_addr;
	return;
}
#endif /* __FreeBSD__ */

int
em_dma_malloc(struct em_softc *sc, bus_size_t size,
	struct em_dma_alloc *dma, int mapflags)
{
	int r;

#ifdef __FreeBSD__
	r = bus_dma_tag_create(NULL,			/* parent */
			       PAGE_SIZE, 0,		/* alignment, bounds */
			       BUS_SPACE_MAXADDR,	/* lowaddr */
			       BUS_SPACE_MAXADDR,	/* highaddr */
			       NULL, NULL,		/* filter, filterarg */
			       size,			/* maxsize */
			       1,			/* nsegments */
			       size,			/* maxsegsize */
			       BUS_DMA_ALLOCNOW,	/* flags */
			       &dma->dma_tag);
	if (r != 0) {
		printf("%s: em_dma_malloc: bus_dma_tag_create failed; "
			"error %u\n", sc->sc_dv.dv_xname, r);
		goto fail_0;
	}

	r = bus_dmamap_create(dma->dma_tag, BUS_DMA_NOWAIT, &dma->dma_map);
#endif /* __FreeBSD__ */
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
			    dma->dma_vaddr,
			    size,
			    NULL,
			    mapflags | BUS_DMA_NOWAIT);
	if (r != 0) {
		printf("%s: em_dma_malloc: bus_dmamap_load failed; "
			"error %u\n", sc->sc_dv.dv_xname, r);
		goto fail_3;
	}

	dma->dma_size = size;
	return (0);

/* fail_4: */
	bus_dmamap_unload(dma->dma_tag, dma->dma_map);
fail_3:
	bus_dmamem_unmap(dma->dma_tag, dma->dma_vaddr, size);
fail_2:
	bus_dmamem_free(dma->dma_tag, &dma->dma_seg, dma->dma_nseg);
fail_1:
	bus_dmamap_destroy(dma->dma_tag, dma->dma_map);
	/* bus_dma_tag_destroy(dma->dma_tag); */
fail_0:
	dma->dma_map = NULL;
	/* dma->dma_tag = NULL; */
	return (r);
}

void
em_dma_free(struct em_softc *sc, struct em_dma_alloc *dma)
{
	bus_dmamap_unload(dma->dma_tag, dma->dma_map);
	bus_dmamem_unmap(dma->dma_tag, dma->dma_vaddr, dma->dma_size);
	bus_dmamem_free(dma->dma_tag, &dma->dma_seg, dma->dma_nseg);
	bus_dmamap_destroy(dma->dma_tag, dma->dma_map);
	/* bus_dma_tag_destroy(dma->dma_tag); */
}


/*********************************************************************
 *
 *  Allocate memory for tx_buffer structures. The tx_buffer stores all 
 *  the information needed to transmit a packet on the wire. 
 *
 **********************************************************************/
int
em_allocate_transmit_structures(struct em_softc * sc)
{
	if (!(sc->tx_buffer_area =
	      (struct em_buffer *) malloc(sizeof(struct em_buffer) *
					     sc->num_tx_desc, M_DEVBUF,
					     M_NOWAIT))) {
		printf("%s: Unable to allocate tx_buffer memory\n", 
		       sc->sc_dv.dv_xname);
		return ENOMEM;
	}

	bzero(sc->tx_buffer_area,
	      sizeof(struct em_buffer) * sc->num_tx_desc);

	return 0;
}

/*********************************************************************
 *
 *  Allocate and initialize transmit structures. 
 *
 **********************************************************************/
int
em_setup_transmit_structures(struct em_softc* sc)
{
#ifdef __FreeBSD__
	/*
	 * Setup DMA descriptor areas.
	 */
	if (bus_dma_tag_create(NULL,	/* parent */
		    PAGE_SIZE, 0,	/* alignment, bounds */
		    BUS_SPACE_MAXADDR,       /* lowaddr */
		    BUS_SPACE_MAXADDR,       /* highaddr */
		    NULL, NULL,              /* filter, filterarg */
		    MCLBYTES * 8,            /* maxsize */
		    EM_MAX_SCATTER,          /* nsegments */
		    MCLBYTES * 8,            /* maxsegsize */
		    BUS_DMA_ALLOCNOW,        /* flags */
		    &sc->txtag)) {
		printf("%s: Unable to allocate TX DMA tag\n", sc->sc_dv.dv_xname);
		return (ENOMEM);
	}

#endif /* __FreeBSD__ */
	sc->txtag = sc->osdep.em_pa.pa_dmat;

	if (em_allocate_transmit_structures(sc))
		return (ENOMEM);

	bzero((void *) sc->tx_desc_base,
	      (sizeof(struct em_tx_desc)) * sc->num_tx_desc);

	sc->next_avail_tx_desc = 0;
	sc->oldest_used_tx_desc = 0;

	/* Set number of descriptors available */
	sc->num_tx_desc_avail = sc->num_tx_desc;

	/* Set checksum context */
	sc->active_checksum_context = OFFLOAD_NONE;

	return (0);
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
void
em_initialize_transmit_unit(struct em_softc * sc)
{
	u_int32_t	reg_tctl;
	u_int32_t	reg_tipg = 0;
	u_int64_t	bus_addr;

	/* Setup the Base and Length of the Tx Descriptor Ring */
	bus_addr = sc->txdma.dma_map->dm_segs[0].ds_addr;
	E1000_WRITE_REG(&sc->hw, TDBAL, (u_int32_t)bus_addr);
	E1000_WRITE_REG(&sc->hw, TDBAH, (u_int32_t)(bus_addr >> 32));
	E1000_WRITE_REG(&sc->hw, TDLEN, 
			sc->num_tx_desc *
			sizeof(struct em_tx_desc));

	/* Setup the HW Tx Head and Tail descriptor pointers */
	E1000_WRITE_REG(&sc->hw, TDH, 0);
	E1000_WRITE_REG(&sc->hw, TDT, 0);


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
	default:
		if (sc->hw.media_type == em_media_type_fiber)
			reg_tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
		else
			reg_tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
			reg_tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
			reg_tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
	}

	E1000_WRITE_REG(&sc->hw, TIPG, reg_tipg);
	E1000_WRITE_REG(&sc->hw, TIDV, sc->tx_int_delay);
	if(sc->hw.mac_type >= em_82540)
		E1000_WRITE_REG(&sc->hw, TADV, sc->tx_abs_int_delay);

	/* Program the Transmit Control Register */
	reg_tctl = E1000_TCTL_PSP | E1000_TCTL_EN |
		   (E1000_COLLISION_THRESHOLD << E1000_CT_SHIFT);
	if (sc->link_duplex == 1) {
		reg_tctl |= E1000_FDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	} else {
		reg_tctl |= E1000_HDX_COLLISION_DISTANCE << E1000_COLD_SHIFT;
	}
	E1000_WRITE_REG(&sc->hw, TCTL, reg_tctl);

	/* Setup Transmit Descriptor Settings for this adapter */   
	sc->txd_cmd = E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;

	if (sc->tx_int_delay > 0)
		sc->txd_cmd |= E1000_TXD_CMD_IDE;

	return;
}

/*********************************************************************
 *
 *  Free all transmit related data structures.
 *
 **********************************************************************/
void
em_free_transmit_structures(struct em_softc* sc)
{
	struct em_buffer   *tx_buffer;
	int		i;

	INIT_DEBUGOUT("free_transmit_structures: begin");

	if (sc->tx_buffer_area != NULL) {
		tx_buffer = sc->tx_buffer_area;
		for (i = 0; i < sc->num_tx_desc; i++, tx_buffer++) {
			if (tx_buffer->m_head != NULL) {
				bus_dmamap_unload(sc->txtag, tx_buffer->map);
				bus_dmamap_destroy(sc->txtag, tx_buffer->map);
				m_freem(tx_buffer->m_head);
			}
			tx_buffer->m_head = NULL;
		}
	}
	if (sc->tx_buffer_area != NULL) {
		free(sc->tx_buffer_area, M_DEVBUF);
		sc->tx_buffer_area = NULL;
	}
	if (sc->txtag != NULL) {
#ifdef __FreeBSD__
		bus_dma_tag_destroy(sc->txtag);
#endif
		sc->txtag = NULL;
	}
	return;
}

/*********************************************************************
 *
 *  The offload context needs to be set when we transfer the first
 *  packet of a particular protocol (TCP/UDP). We change the
 *  context only if the protocol type changes.
 *
 **********************************************************************/
#ifdef __FreeBSD__
void
em_transmit_checksum_setup(struct em_softc * sc,
			   struct mbuf *mp,
			   u_int32_t *txd_upper,
			   u_int32_t *txd_lower) 
{
	struct em_context_desc *TXD;
	struct em_buffer *tx_buffer;
	int curr_txd;

	if (mp->m_pkthdr.csum_flags) {

		if (mp->m_pkthdr.csum_flags & CSUM_TCP) {
			*txd_upper = E1000_TXD_POPTS_TXSM << 8;
			*txd_lower = E1000_TXD_CMD_DEXT | E1000_TXD_DTYP_D;
			if (sc->active_checksum_context == OFFLOAD_TCP_IP)
				return;
			else
				sc->active_checksum_context = OFFLOAD_TCP_IP;

		} else if (mp->m_pkthdr.csum_flags & CSUM_UDP) {
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

	if (++curr_txd == sc->num_tx_desc)
		curr_txd = 0;

	sc->num_tx_desc_avail--;
	sc->next_avail_tx_desc = curr_txd;

	return;
}
#endif /* __FreeBSD__ */

/**********************************************************************
 *
 *  Examine each tx_buffer in the used queue. If the hardware is done
 *  processing the packet then free associated resources. The
 *  tx_buffer is put back on the free queue. 
 *
 **********************************************************************/
void
em_clean_transmit_interrupts(struct em_softc* sc)
{
	int s;
	int i, num_avail;
	struct em_buffer *tx_buffer;
	struct em_tx_desc   *tx_desc;
	struct ifnet   *ifp = &sc->interface_data.ac_if;

	if (sc->num_tx_desc_avail == sc->num_tx_desc)
		return;

	s = splimp();
#ifdef DBG_STATS
	sc->clean_tx_interrupts++;
#endif
	num_avail = sc->num_tx_desc_avail;
	i = sc->oldest_used_tx_desc;

	tx_buffer = &sc->tx_buffer_area[i];
	tx_desc = &sc->tx_desc_base[i];

	while(tx_desc->upper.fields.status & E1000_TXD_STAT_DD) {

		tx_desc->upper.data = 0;
		num_avail++;

		if (tx_buffer->m_head) {
			ifp->if_opackets++;
			bus_dmamap_sync(sc->txtag, tx_buffer->map,
			    0, tx_buffer->map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->txtag, tx_buffer->map);
			bus_dmamap_destroy(sc->txtag, tx_buffer->map);

			m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
		}

		if (++i == sc->num_tx_desc)
			i = 0;

		tx_buffer = &sc->tx_buffer_area[i];
		tx_desc = &sc->tx_desc_base[i];
	}

	sc->oldest_used_tx_desc = i;

	/*
	 * If we have enough room, clear IFF_OACTIVE to tell the stack
	 * that it is OK to send packets.
	 * If there are no pending descriptors, clear the timeout. Otherwise,
	 * if some descriptors have been freed, restart the timeout.
	 */
	if (num_avail > EM_TX_CLEANUP_THRESHOLD) {
		ifp->if_flags &= ~IFF_OACTIVE;
		if (num_avail == sc->num_tx_desc)
			ifp->if_timer = 0;
		else if (num_avail == sc->num_tx_desc_avail)
			ifp->if_timer = EM_TX_TIMEOUT;
	}
	sc->num_tx_desc_avail = num_avail;
	splx(s);
	return;
}

/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
int
em_get_buf(int i, struct em_softc *sc,
    struct mbuf *nmp)
{
	struct mbuf    *mp = nmp;
	struct em_buffer *rx_buffer;
	struct ifnet   *ifp;
	int error;

	ifp = &sc->interface_data.ac_if;

	if (mp == NULL) {
		MGETHDR(mp, M_DONTWAIT, MT_DATA);
		if (mp == NULL) {
			sc->mbuf_alloc_failed++;
			return(ENOBUFS);
		}
		MCLGET(mp, M_DONTWAIT);
		if ((mp->m_flags & M_EXT) == 0) {
			m_freem(mp);
			sc->mbuf_cluster_failed++;
			return(ENOBUFS);
		}
		mp->m_len = mp->m_pkthdr.len = MCLBYTES;
	} else {
		mp->m_len = mp->m_pkthdr.len = MCLBYTES;
		mp->m_data = mp->m_ext.ext_buf;
		mp->m_next = NULL;
	}

	if (ifp->if_mtu <= ETHERMTU) {
		m_adj(mp, ETHER_ALIGN);
	}

	rx_buffer = &sc->rx_buffer_area[i];

	/*
	 * Using memory from the mbuf cluster pool, invoke the
	 * bus_dma machinery to arrange the memory mapping.
	 */
	error = bus_dmamap_load(sc->rxtag, rx_buffer->map,
	    mtod(mp, void *), mp->m_len, NULL,
	    0);
	if (error) {
		m_free(mp);
		return(error);
	}
	rx_buffer->m_head = mp;
	sc->rx_desc_base[i].buffer_addr = htole64(rx_buffer->map->dm_segs[0].ds_addr);
	bus_dmamap_sync(sc->rxtag, rx_buffer->map, 0,
	    rx_buffer->map->dm_mapsize, BUS_DMASYNC_PREREAD);

	return(0);
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
em_allocate_receive_structures(struct em_softc* sc)
{
	int		i, error;
	struct em_buffer *rx_buffer;

	if (!(sc->rx_buffer_area =
	      (struct em_buffer *) malloc(sizeof(struct em_buffer) *
					     sc->num_rx_desc, M_DEVBUF,
					     M_NOWAIT))) {
		printf("%s: Unable to allocate rx_buffer memory\n", 
		       sc->sc_dv.dv_xname);
		return(ENOMEM);
	}

	bzero(sc->rx_buffer_area,
	      sizeof(struct em_buffer) * sc->num_rx_desc);

#ifdef __FreeBSD__
	error = bus_dma_tag_create(NULL,                /* parent */
				PAGE_SIZE, 0,            /* alignment, bounds */
				BUS_SPACE_MAXADDR,       /* lowaddr */
				BUS_SPACE_MAXADDR,       /* highaddr */
				NULL, NULL,              /* filter, filterarg */
				MCLBYTES,                /* maxsize */
				1,                       /* nsegments */
				MCLBYTES,                /* maxsegsize */
				BUS_DMA_ALLOCNOW,        /* flags */
				&sc->rxtag);
	if (error != 0) {
		printf("%s: em_allocate_receive_structures: "
			"bus_dma_tag_create failed; error %u\n",
			sc->sc_dv.dv_xname, error);
		goto fail_0;
	}
#endif /* __FreeBSD__ */
	sc->rxtag = sc->osdep.em_pa.pa_dmat;

	rx_buffer = sc->rx_buffer_area;
	for (i = 0; i < sc->num_rx_desc; i++, rx_buffer++) {
		error = bus_dmamap_create(sc->rxtag, MCLBYTES, 1,
					MCLBYTES, 0, BUS_DMA_NOWAIT,
					&rx_buffer->map);
		if (error != 0) {
			printf("%s: em_allocate_receive_structures: "
			    "bus_dmamap_create failed; error %u\n",
			    sc->sc_dv.dv_xname, error);
			goto fail_1;
		}
	}

	for (i = 0; i < sc->num_rx_desc; i++) {
		error = em_get_buf(i, sc, NULL);
		if (error != 0) {
			sc->rx_buffer_area[i].m_head = NULL;
			sc->rx_desc_base[i].buffer_addr = 0;
			return(error);
                }
        }

        return(0);

fail_1:
	/* bus_dma_tag_destroy(sc->rxtag); */
/* fail_0: */
	sc->rxtag = NULL;
	free(sc->rx_buffer_area, M_DEVBUF);
	sc->rx_buffer_area = NULL;
	return (error);
}

/*********************************************************************
 *
 *  Allocate and initialize receive structures.
 *  
 **********************************************************************/
int
em_setup_receive_structures(struct em_softc * sc)
{
	bzero((void *) sc->rx_desc_base,
	    (sizeof(struct em_rx_desc)) * sc->num_rx_desc);

	if (em_allocate_receive_structures(sc))
		return ENOMEM;

	/* Setup our descriptor pointers */
	sc->next_rx_desc_to_check = 0;
	return(0);
}

/*********************************************************************
 *
 *  Enable receive unit.
 *  
 **********************************************************************/
void
em_initialize_receive_unit(struct em_softc * sc)
{
	u_int32_t	reg_rctl;
#ifdef __FreeBSD__
	u_int32_t	reg_rxcsum;
#endif
	struct ifnet	*ifp;
	u_int64_t	bus_addr;

	ifp = &sc->interface_data.ac_if;

	/* Make sure receives are disabled while setting up the descriptor ring */
	E1000_WRITE_REG(&sc->hw, RCTL, 0);

	/* Set the Receive Delay Timer Register */
	E1000_WRITE_REG(&sc->hw, RDTR, 
			sc->rx_int_delay | E1000_RDT_FPDB);

	if(sc->hw.mac_type >= em_82540) {
		E1000_WRITE_REG(&sc->hw, RADV, sc->rx_abs_int_delay);

		/* Set the interrupt throttling rate.  Value is calculated
		 * as DEFAULT_ITR = 1/(MAX_INTS_PER_SEC * 256ns) */
#define MAX_INTS_PER_SEC	8000
#define DEFAULT_ITR		1000000000/(MAX_INTS_PER_SEC * 256)
		E1000_WRITE_REG(&sc->hw, ITR, DEFAULT_ITR);
	}

	/* Setup the Base and Length of the Rx Descriptor Ring */
	bus_addr = sc->rxdma.dma_map->dm_segs[0].ds_addr;
	E1000_WRITE_REG(&sc->hw, RDBAL, (u_int32_t)bus_addr);
	E1000_WRITE_REG(&sc->hw, RDBAH, (u_int32_t)(bus_addr >> 32));
	E1000_WRITE_REG(&sc->hw, RDLEN, sc->num_rx_desc *
			sizeof(struct em_rx_desc));

	/* Setup the HW Rx Head and Tail Descriptor Pointers */
	E1000_WRITE_REG(&sc->hw, RDH, 0);
	E1000_WRITE_REG(&sc->hw, RDT, sc->num_rx_desc - 1);

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

	if (ifp->if_mtu > ETHERMTU)
		reg_rctl |= E1000_RCTL_LPE;

#ifdef __FreeBSD__
	/* Enable 82543 Receive Checksum Offload for TCP and UDP */
	if ((sc->hw.mac_type >= em_82543) && 
	    (ifp->if_capenable & IFCAP_RXCSUM)) {
		reg_rxcsum = E1000_READ_REG(&sc->hw, RXCSUM);
		reg_rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
		E1000_WRITE_REG(&sc->hw, RXCSUM, reg_rxcsum);
	}
#endif /* __FreeBSD__ */

	/* Enable Receives */
	E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);

	return;
}

/*********************************************************************
 *
 *  Free receive related data structures.
 *
 **********************************************************************/
void
em_free_receive_structures(struct em_softc * sc)
{
	struct em_buffer   *rx_buffer;
	int		i;

	INIT_DEBUGOUT("free_receive_structures: begin");

	if (sc->rx_buffer_area != NULL) {
		rx_buffer = sc->rx_buffer_area;
		for (i = 0; i < sc->num_rx_desc; i++, rx_buffer++) {
			if (rx_buffer->map != NULL) {
				bus_dmamap_unload(sc->rxtag, rx_buffer->map);
				bus_dmamap_destroy(sc->rxtag, rx_buffer->map);
			}
			if (rx_buffer->m_head != NULL)
				m_freem(rx_buffer->m_head);
			rx_buffer->m_head = NULL;
		}
	}
	if (sc->rx_buffer_area != NULL) {
		free(sc->rx_buffer_area, M_DEVBUF);
		sc->rx_buffer_area = NULL;
	}
	if (sc->rxtag != NULL) {
		/* bus_dma_tag_destroy(sc->rxtag); */
		sc->rxtag = NULL;
	}
	return;
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
em_process_receive_interrupts(struct em_softc* sc, int count)
{
	struct ifnet	    *ifp;
	struct mbuf	    *mp;
#ifdef __FreeBSD__
	struct ether_header *eh;
#endif
	u_int8_t	    accept_frame = 0;
	u_int8_t	    eop = 0;
	u_int16_t	    len, desc_len;
	int		    i;

	/* Pointer to the receive descriptor being examined. */
	struct em_rx_desc   *current_desc;

	ifp = &sc->interface_data.ac_if;
	i = sc->next_rx_desc_to_check;
	current_desc = &sc->rx_desc_base[i];

	if (!((current_desc->status) & E1000_RXD_STAT_DD)) {
#ifdef DBG_STATS
		sc->no_pkts_avail++;
#endif
		return;
	}

	while ((current_desc->status & E1000_RXD_STAT_DD) && (count != 0)) {

		mp = sc->rx_buffer_area[i].m_head;
		bus_dmamap_sync(sc->rxtag, sc->rx_buffer_area[i].map,
		    0, sc->rx_buffer_area[i].map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->rxtag, sc->rx_buffer_area[i].map);

		accept_frame = 1;
		desc_len = letoh16(current_desc->length);
		if (current_desc->status & E1000_RXD_STAT_EOP) {
			count--;
			eop = 1;
			len = desc_len - ETHER_CRC_LEN;
		} else {
			eop = 0;
			len = desc_len;
		}

		if (current_desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK) {
			u_int8_t last_byte;
			u_int32_t pkt_len = desc_len;

			if (sc->fmp != NULL)
				pkt_len += sc->fmp->m_pkthdr.len; 

			last_byte = *(mtod(mp, caddr_t) + desc_len - 1);

			if (TBI_ACCEPT(&sc->hw, current_desc->status,
				       current_desc->errors,
				       pkt_len, last_byte)) {
				em_tbi_adjust_stats(&sc->hw, 
						    &sc->stats, 
						    pkt_len, 
						    sc->hw.mac_addr);
				len--;
			}
			else {
				accept_frame = 0;
			}
		}

		if (accept_frame) {

			if (em_get_buf(i, sc, NULL) == ENOBUFS) {
				sc->dropped_pkts++;
				em_get_buf(i, sc, mp);
				if (sc->fmp != NULL)
					m_freem(sc->fmp);
				sc->fmp = NULL;
				sc->lmp = NULL;
				break;
			}

			/* Assign correct length to the current fragment */
			mp->m_len = len;

			if (sc->fmp == NULL) {
				mp->m_pkthdr.len = len;
				sc->fmp = mp;	 /* Store the first mbuf */
				sc->lmp = mp;
			} else {
				/* Chain mbuf's together */
				mp->m_flags &= ~M_PKTHDR;
				sc->lmp->m_next = mp;
				sc->lmp = sc->lmp->m_next;
				sc->fmp->m_pkthdr.len += len;
			}

			if (eop) {
				sc->fmp->m_pkthdr.rcvif = ifp;
				ifp->if_ipackets++;

#if NBPFILTER > 0
				/*
				 * Handle BPF listeners. Let the BPF
				 * user see the packet.
				 */
				if (ifp->if_bpf)
					bpf_mtap(ifp->if_bpf, sc->fmp);
#endif

#ifdef __FreeBSD__
				eh = mtod(sc->fmp, struct ether_header *);
				/* Remove ethernet header from mbuf */
				m_adj(sc->fmp, sizeof(struct ether_header));
#endif
				em_receive_checksum(sc, current_desc,
						sc->fmp);

#ifdef __FreeBSD__
				if (current_desc->status & E1000_RXD_STAT_VP)
					VLAN_INPUT_TAG(eh, sc->fmp, 
					    (letoh16(current_desc->special) &
					    E1000_RXD_SPC_VLAN_MASK));
				else
					ether_input(ifp, eh, sc->fmp);
#else /* __FreeBSD__ */
				ether_input_mbuf(ifp, sc->fmp);
#endif /* !__FreeBSD__ */

				sc->fmp = NULL;
				sc->lmp = NULL;
			}
		} else {
			sc->dropped_pkts++;
			em_get_buf(i, sc, mp);
			if (sc->fmp != NULL)
				m_freem(sc->fmp);
			sc->fmp = NULL;
			sc->lmp = NULL;
		}

		/* Zero out the receive descriptors status  */
		current_desc->status = 0;

		/* Advance the E1000's Receive Queue #0	 "Tail Pointer". */
		E1000_WRITE_REG(&sc->hw, RDT, i);

		/* Advance our pointers to the next descriptor */
		if (++i == sc->num_rx_desc) {
			i = 0;
			current_desc = sc->rx_desc_base;
		} else
			current_desc++;
	}
	sc->next_rx_desc_to_check = i;
	return;
}

/*********************************************************************
 *
 *  Verify that the hardware indicated that the checksum is valid. 
 *  Inform the stack about the status of checksum so that stack
 *  doesn't spend time verifying the checksum.
 *
 *********************************************************************/
void
em_receive_checksum(struct em_softc *sc,
		    struct em_rx_desc *rx_desc,
		    struct mbuf *mp)
{
#ifdef __FreeBSD__
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
			mp->m_pkthdr.csum_flags = CSUM_IP_CHECKED;
			mp->m_pkthdr.csum_flags |= CSUM_IP_VALID;

		} else {
			mp->m_pkthdr.csum_flags = 0;
		}
	}

	if (rx_desc->status & E1000_RXD_STAT_TCPCS) {
		/* Did it pass? */	  
		if (!(rx_desc->errors & E1000_RXD_ERR_TCPE)) {
			mp->m_pkthdr.csum_flags |= 
			(CSUM_DATA_VALID | CSUM_PSEUDO_HDR);
			mp->m_pkthdr.csum_data = htons(0xffff);
		}
	}

	return;
#else /* __FreeBSD__ */
	/* 82543 or newer only */
	if ((sc->hw.mac_type < em_82543) ||
	    /* Ignore Checksum bit is set */
	    (rx_desc->status & E1000_RXD_STAT_IXSM))
		return;

	if ((rx_desc->status & (E1000_RXD_STAT_IPCS|E1000_RXD_ERR_IPE)) ==
	    E1000_RXD_STAT_IPCS)
		mp->m_pkthdr.csum |= M_IPV4_CSUM_IN_OK;

	if ((rx_desc->status & (E1000_RXD_STAT_IPCS|E1000_RXD_ERR_IPE|
	    E1000_RXD_STAT_TCPCS|E1000_RXD_ERR_TCPE)) ==
	    (E1000_RXD_STAT_TCPCS | E1000_RXD_STAT_IPCS))
		mp->m_pkthdr.csum |= M_TCP_CSUM_IN_OK | M_UDP_CSUM_IN_OK;
#endif /* __FreeBSD__ */
}


void em_enable_vlans(struct em_softc * sc)
{
	uint32_t ctrl;

	E1000_WRITE_REG(&sc->hw, VET, ETHERTYPE_8021Q);

	ctrl = E1000_READ_REG(&sc->hw, CTRL);
	ctrl |= E1000_CTRL_VME; 
	E1000_WRITE_REG(&sc->hw, CTRL, ctrl);

	return;
}

void
em_enable_intr(struct em_softc* sc)
{
	E1000_WRITE_REG(&sc->hw, IMS, (IMS_ENABLE_MASK));
	return;
}

void
em_disable_intr(struct em_softc *sc)
{
	E1000_WRITE_REG(&sc->hw, IMC, 
			(0xffffffff & ~E1000_IMC_RXSEQ));
	return;
}

int
em_is_valid_ether_addr(u_int8_t *addr)
{
	const char zero_addr[6] = { 0, 0, 0, 0, 0, 0 };

	if ((addr[0] & 1) || (!bcmp(addr, zero_addr, ETHER_ADDR_LEN))) {
		return (FALSE);
	}

	return(TRUE);
}

void
em_write_pci_cfg(struct em_hw *hw,
		      uint32_t reg,
		      uint16_t *value)
{
	struct pci_attach_args *pa = &((struct em_osdep *)hw->back)->em_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	/* Should we do read/mask/write...?  16 vs 32 bit!!! */
	pci_conf_write(pc, pa->pa_tag, reg, *value);
}

void
em_read_pci_cfg(struct em_hw *hw, uint32_t reg,
		     uint16_t *value)
{
	struct pci_attach_args *pa = &((struct em_osdep *)hw->back)->em_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	*value = pci_conf_read(pc, pa->pa_tag, reg);
	return;
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

uint32_t
em_io_read(struct em_hw *hw, uint32_t port)
{
	return bus_space_read_4(((struct em_osdep *)(hw)->back)->em_iobtag,
		((struct em_osdep *)(hw)->back)->em_iobhandle, port);
}

void
em_io_write(struct em_hw *hw, uint32_t port, uint32_t value)
{
	bus_space_write_4(((struct em_osdep *)(hw)->back)->em_iobtag,
			((struct em_osdep *)(hw)->back)->em_iobhandle, port,
			value);
	return;
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

	sc->stats.crcerrs += E1000_READ_REG(&sc->hw, CRCERRS);
	sc->stats.symerrs += E1000_READ_REG(&sc->hw, SYMERRS);
	sc->stats.mpc += E1000_READ_REG(&sc->hw, MPC);
	sc->stats.scc += E1000_READ_REG(&sc->hw, SCC);
	sc->stats.ecol += E1000_READ_REG(&sc->hw, ECOL);

	sc->stats.mcc += E1000_READ_REG(&sc->hw, MCC);
	sc->stats.latecol += E1000_READ_REG(&sc->hw, LATECOL);
	sc->stats.colc += E1000_READ_REG(&sc->hw, COLC);
	sc->stats.dc += E1000_READ_REG(&sc->hw, DC);
	sc->stats.sec += E1000_READ_REG(&sc->hw, SEC);
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
	ifp->if_ibytes = sc->stats.gorcl;
	ifp->if_obytes = sc->stats.gotcl;
	ifp->if_imcasts = sc->stats.mprc;
	ifp->if_collisions = sc->stats.colc;

	/* Rx Errors */
	ifp->if_ierrors =
	sc->dropped_pkts +
	sc->stats.rxerrc +
	sc->stats.crcerrs +
	sc->stats.algnerrc +
	sc->stats.rlec + sc->stats.rnbc + 
	sc->stats.mpc + sc->stats.cexterr;

	/* Tx Errors */
	ifp->if_oerrors = sc->stats.ecol + sc->stats.latecol;

}


/**********************************************************************
 *
 *  This routine is called only when em_display_debug_stats is enabled.
 *  This routine provides a way to take a look at important statistics
 *  maintained by the driver and hardware.
 *
 **********************************************************************/
void
em_print_debug_info(struct em_softc *sc)
{
	const char * const unit = sc->sc_dv.dv_xname;

#ifdef DBG_STATS
	printf("%s: Packets not Avail = %ld\n", unit, 
	       sc->no_pkts_avail);
	printf("%s: CleanTxInterrupts = %ld\n", unit,
	       sc->clean_tx_interrupts);
#endif
	printf("%s: fifo workaround = %lld, fifo_reset = %lld\n", unit,
		(long long)sc->tx_fifo_wrk,
		(long long)sc->tx_fifo_reset);
	printf("%s: hw tdh = %d, hw tdt = %d\n", unit,
		E1000_READ_REG(&sc->hw, TDH),
		E1000_READ_REG(&sc->hw, TDT));
	printf("%s: Num Tx Descriptors avail = %ld\n", unit,
	       sc->num_tx_desc_avail);
	printf("%s: Tx Descriptors not avail1 = %ld\n", unit,
	       sc->no_tx_desc_avail1);
	printf("%s: Tx Descriptors not avail2 = %ld\n", unit,
	       sc->no_tx_desc_avail2);
	printf("%s: Std mbuf failed = %ld\n", unit,
		sc->mbuf_alloc_failed);
	printf("%s: Std mbuf cluster failed = %ld\n", unit,
		sc->mbuf_cluster_failed);
	printf("%s: Driver dropped packets = %ld\n", unit,
	       sc->dropped_pkts);

	return;
}

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
	printf("%s: Receive length errors = %lld\n", unit,
	       (long long)sc->stats.rlec);
	printf("%s: Receive errors = %lld\n", unit,
	       (long long)sc->stats.rxerrc);
	printf("%s: Crc errors = %lld\n", unit,
	       (long long)sc->stats.crcerrs);
	printf("%s: Alignment errors = %lld\n", unit,
	       (long long)sc->stats.algnerrc);
	printf("%s: Carrier extension errors = %lld\n", unit,
	       (long long)sc->stats.cexterr);

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

	return;
}

#ifdef __FreeBSD__
int
em_sysctl_debug_info(SYSCTL_HANDLER_ARGS)
{
	int error;
	int result;
	struct em_softc *sc;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		sc = (struct em_softc *)arg1;
		em_print_debug_info(sc);
	}

	return error;
}


int
em_sysctl_stats(SYSCTL_HANDLER_ARGS)
{
	int error;
	int result;
	struct em_softc *sc;

	result = -1;
	error = sysctl_handle_int(oidp, &result, 0, req);

	if (error || !req->newptr)
		return (error);

	if (result == 1) {
		sc = (struct em_softc *)arg1;
		em_print_hw_stats(sc);
	}

	return error;
}
#endif /* __FreeBSD__ */

