/**************************************************************************

Copyright (c) 2001-2002 Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms of the Software, with or
without modification, are permitted provided that the following conditions
are met:

 1. Redistributions of source code of the Software may retain the above
    copyright notice, this list of conditions and the following disclaimer.

 2. Redistributions in binary form of the Software may reproduce the above
    copyright notice, this list of conditions and the following disclaimer
    in the documentation and/or other materials provided with the
    distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors shall be used to endorse or promote products derived from
    this Software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR ITS CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
SUCH DAMAGE.

***************************************************************************/

/*$FreeBSD$*/

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

/*********************************************************************
 *  Set this to one to display debug statistics                                                   
 *********************************************************************/
int             em_display_debug_stats = 0;

/*********************************************************************
 *  Linked list of board private structures for all NICs found
 *********************************************************************/
#if 0
struct em_softc *em_em_softc_list = NULL;
#endif

/*********************************************************************
 *  Driver version
 *********************************************************************/

char em_driver_version[] = "1.3.14";


/*********************************************************************
 *  PCI Device ID Table
 *********************************************************************/
struct em_device
{
	u_int16_t	vendor_id;
	u_int16_t	device_id;
	int		match;
};

struct em_device em_devs[] =
{
	/* Intel(R) PRO/1000 Network Connection */
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82542,		2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC_SC,	2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82543GC,		2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544EI,		2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544EI_SC,	2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC,		2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82544GC_LX,	2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82540EM,		2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545EM,		2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546EB,		2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545EM_SC,	2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82546EB_SC,	2},
	{ PCI_VENDOR_INTEL, PCI_PRODUCT_INTEL_82545EM_LX,	2},
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
int  em_ioctl(struct ifnet *, IOCTL_CMD_TYPE, caddr_t);
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
void em_process_receive_interrupts(struct em_softc *);
void em_receive_checksum(struct em_softc *, 
				     struct em_rx_desc * rx_desc,
				     struct mbuf *);
#if 0
void em_transmit_checksum_setup(struct em_softc *,
					    struct mbuf *,
					    struct em_tx_buffer *,
					    u_int32_t *,
					    u_int32_t *);
#endif /* 0 */
void em_set_promisc(struct em_softc *);
void em_disable_promisc(struct em_softc *);
void em_set_multi(struct em_softc *);
void em_print_hw_stats(struct em_softc *);
void em_print_link_status(struct em_softc *);
int  em_get_buf(struct em_rx_buffer *, struct em_softc *,
			    struct mbuf *);
void em_enable_vlans(struct em_softc *em_softc);


int em_malloc_dma(struct em_softc *sc, struct em_dmamap *emm,
			 bus_size_t size);
void em_free_dma(struct em_softc *sc, struct em_dmamap *emm);

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
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	int i;

	INIT_DEBUGOUT("em_probe: begin");

	for (i = 0; i < sizeof(em_devs) / sizeof(em_devs[0]); i++) {
		if (PCI_VENDOR(pa->pa_id) == em_devs[i].vendor_id &&
		    PCI_PRODUCT(pa->pa_id) == em_devs[i].device_id)
			return (em_devs[i].match);
	}

	return (0);
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
#if 0
	pci_chipset_tag_t pc = pa->pa_pc;
#endif
	struct em_softc *sc = (struct em_softc *)self;
	int             s;
	int             tsize, rsize;

	INIT_DEBUGOUT("em_attach: begin");
	s = splimp();

	sc->osdep.em_pa = *pa;

	timeout_set(&sc->em_timeout, em_local_timer, sc);

	/* Determine hardware revision */
	em_identify_hardware(sc);

	/* Parameters (to be read from user) */
	sc->num_tx_desc = MAX_TXD;
	sc->num_rx_desc = MAX_RXD;
	sc->tx_int_delay = TIDV;
	sc->rx_int_delay = RIDV;
	sc->hw.autoneg = DO_AUTO_NEG;
	sc->hw.wait_autoneg_complete = WAIT_FOR_AUTO_NEG_DEFAULT;
	sc->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
	sc->hw.tbi_compatibility_en = TRUE;
	sc->rx_buffer_len = EM_RXBUFFER_2048;

	sc->hw.fc_high_water = FC_DEFAULT_HI_THRESH;
	sc->hw.fc_low_water  = FC_DEFAULT_LO_THRESH;
	sc->hw.fc_pause_time = FC_DEFAULT_TX_TIMER;
	sc->hw.fc_send_xon   = TRUE;
	sc->hw.fc = em_fc_full;

	/* Set the max frame size assuming standard ethernet sized frames */   
	sc->hw.max_frame_size = 
	ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN;

	sc->hw.min_frame_size = 
	MINIMUM_ETHERNET_PACKET_SIZE + ETHER_CRC_LEN;

	/* This controls when hardware reports transmit completion status. */
	if ((EM_REPORT_TX_EARLY == 0) || (EM_REPORT_TX_EARLY == 1)) {
		sc->hw.report_tx_early = EM_REPORT_TX_EARLY;
	} else {
		if (sc->hw.mac_type < em_82543) {
			sc->hw.report_tx_early = 0;
		} else {
			sc->hw.report_tx_early = 1;
		}
	}

	if (em_allocate_pci_resources(sc)) {
		printf("%s: Allocation of PCI resources failed\n", 
		       sc->sc_dv.dv_xname);
		em_free_pci_resources(sc);
		splx(s);
		return;
	}

	tsize = EM_ROUNDUP(sc->num_tx_desc *
			   sizeof(struct em_tx_desc), 4096);

	/* Allocate Transmit Descriptor ring */
	if(em_malloc_dma(sc, &sc->osdep.em_tx, tsize)) {
		printf("%s: Unable to allocate TxDescriptor memory\n", 
		       sc->sc_dv.dv_xname);
		em_free_pci_resources(sc);
		splx(s);
		return;
	}

	sc->tx_desc_base = (struct em_tx_desc *)sc->osdep.em_tx.emm_kva;

	rsize = EM_ROUNDUP(sc->num_rx_desc *
			   sizeof(struct em_rx_desc), 4096);

	/* Allocate Receive Descriptor ring */
	if(em_malloc_dma(sc, &sc->osdep.em_rx, rsize)) {
		printf("%s: Unable to allocate rx_desc memory\n", 
		       sc->sc_dv.dv_xname);
		em_free_pci_resources(sc);
		em_free_dma(sc, &sc->osdep.em_tx);
		splx(s);
		return;
	}

	sc->rx_desc_base = (struct em_rx_desc *)sc->osdep.em_rx.emm_kva;

	/* Initialize the hardware */
	if (em_hardware_init(sc)) {
		printf("%s: Unable to initialize the hardware\n",
		       sc->sc_dv.dv_xname);
		em_free_pci_resources(sc);
		em_free_dma(sc, &sc->osdep.em_tx);
		em_free_dma(sc, &sc->osdep.em_rx);
		splx(s);
		return;
	}

	/* Copy the permanent MAC address out of the EEPROM */
	if (em_read_mac_addr(&sc->hw) < 0) {
		printf("%s: EEPROM read error while reading mac address\n",
		       sc->sc_dv.dv_xname);
		return;
	}

	memcpy((char *)&sc->arpcom.ac_enaddr, sc->hw.mac_addr,
	       ETH_LENGTH_OF_ADDRESS);

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
}

/*********************************************************************
 *  Device removal routine
 *
 *  The detach entry point is called when the driver is being removed.
 *  This routine stops the em_softc and deallocates all the resources
 *  that were allocated for driver operation.
 *  
 *  return 0 on success, positive on failure
 *********************************************************************/
#if 0
int
em_detach(void* arg)
{
	struct em_softc *sc = arg;
	struct ifnet   *ifp = &sc->arpcom.ac_if;
	int             s;

	INIT_DEBUGOUT("em_detach: begin");
	s = splimp();

	em_stop(sc);
	em_phy_hw_reset(&sc->hw);
	if_detach(ifp);
	ether_ifdetach(ifp);
	em_free_pci_resources(sc);

	/* Free Transmit Descriptor ring */
	if (sc->tx_desc_base) {
		em_free_dma(sc, &sc->osdep.em_tx);
		sc->tx_desc_base = NULL;
	}

	/* Free Receive Descriptor ring */
	if (sc->rx_desc_base) {
		em_free_dma(sc, &sc->osdep.em_rx);
		sc->rx_desc_base = NULL;
	}

#if 0
	/* Remove from the em_softc list */
	if (em_em_softc_list == sc)
		em_em_softc_list = sc->next;
	if (sc->next != NULL)
		sc->next->prev = sc->prev;
	if (sc->prev != NULL)
		sc->prev->next = sc->next;
#endif /* 0 */

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	splx(s);
	return(0);
}

int
em_shutdown(void* arg)
{
	struct em_softc *sc = arg;
	em_stop(sc);
	return(0);
}

#endif /* 0 */

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
	int             i, s;
	struct mbuf    *m_head;
	u_int32_t       txd_upper; 
	u_int32_t       txd_lower;
	struct em_tx_buffer   *tx_buffer;
	struct em_tx_desc *current_tx_desc = NULL;
	struct em_softc * sc = ifp->if_softc;

	if (!sc->link_active)
		return;

	s = splimp();      

	for(;;) {
#if NVLAN > 0
		struct ifvlan *ifv = NULL;
#endif

		IFQ_POLL(&ifp->if_snd, m_head);

		if (m_head == NULL) break;

		if (sc->num_tx_desc_avail <= TX_CLEANUP_THRESHOLD)
			em_clean_transmit_interrupts(sc);

		if (sc->num_tx_desc_avail <= TX_CLEANUP_THRESHOLD) {
			ifp->if_flags |= IFF_OACTIVE;
			sc->no_tx_desc_avail++;
			break;
		}

		tx_buffer =  SIMPLEQ_FIRST(&sc->free_tx_buffer_list);
		if (!tx_buffer) {
			sc->no_tx_buffer_avail1++;
			/* 
			 * OK so we should not get here but I've seen
			 * it so let us try to clean up and then try
			 * to get a tx_buffer again and only break if
			 * we still don't get one.
			 */
			em_clean_transmit_interrupts(sc);
			tx_buffer = SIMPLEQ_FIRST(&sc->free_tx_buffer_list);
			if (!tx_buffer) {
				ifp->if_flags |= IFF_OACTIVE;
				sc->no_tx_buffer_avail2++;
				break;
			}
		}

		IFQ_DEQUEUE(&ifp->if_snd, m_head);

		SIMPLEQ_REMOVE_HEAD(&sc->free_tx_buffer_list, tx_buffer,
				    em_tx_entry);

		tx_buffer->num_tx_desc_used = 0;
		tx_buffer->m_head = m_head;
#if 0
		if (ifp->if_hwassist > 0) {
			em_transmit_checksum_setup(sc,  m_head, tx_buffer, 
						   &txd_upper, &txd_lower);
		} else {
#endif
			txd_upper = 0;
			txd_lower = 0;
#if 0
		}
#endif

#if NVLAN > 0
		/* Find out if we are in vlan mode */
		if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == 
		    (M_PROTO1|M_PKTHDR) &&
		    m_head->m_pkthdr.rcvif != NULL &&
		    m_head->m_pkthdr.rcvif->if_type == IFT_L2VLAN)
			ifv = m_head->m_pkthdr.rcvif->if_softc;
#endif

		if (bus_dmamap_load_mbuf(sc->osdep.em_pa.pa_dmat,
					 tx_buffer->dmamap,
					 m_head, BUS_DMA_NOWAIT))
			return;

		for (i = 0; i < tx_buffer->dmamap->dm_nsegs; i++) {
			bus_addr_t addr= tx_buffer->dmamap->dm_segs[i].ds_addr;
			bus_size_t len = tx_buffer->dmamap->dm_segs[i].ds_len;

			current_tx_desc = sc->next_avail_tx_desc;
			current_tx_desc->buffer_addr = htole64(addr);

			current_tx_desc->lower.data = htole32(txd_lower | len);
			current_tx_desc->upper.data = htole32(txd_upper);

			if (current_tx_desc == sc->last_tx_desc)
				sc->next_avail_tx_desc =
				sc->first_tx_desc;
			else
				sc->next_avail_tx_desc++;

			sc->num_tx_desc_avail--;
			tx_buffer->num_tx_desc_used++;
		}

		/* Put this tx_buffer at the end in the "in use" list */
		SIMPLEQ_INSERT_TAIL(&sc->used_tx_buffer_list, tx_buffer, 
				   em_tx_entry);

#if NVLAN > 0
		if (ifv != NULL) {
			/* Tell hardware to add tag */
			current_tx_desc->lower.data |=
				htole32(E1000_TXD_CMD_VLE);

			/* Set the vlan id */
			current_tx_desc->upper.fields.special =
				htole16(ifv->ifv_tag);
		}
#endif

		/* 
		 * Last Descriptor of Packet needs End Of Packet
		 * (EOP), Report Status (RS) and append Ethernet CRC
		 * (IFCS) bits set.
		 */
		current_tx_desc->lower.data |=
			htole32(sc->txd_cmd|E1000_TXD_CMD_EOP);

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif

		/* 
		 * Advance the Transmit Descriptor Tail (Tdt), this
		 * tells the E1000 that this frame is available to
		 * transmit.
		 */
		E1000_WRITE_REG(&sc->hw, TDT, 
				(((_BSD_PTRDIFF_T_) sc->next_avail_tx_desc -
				  (_BSD_PTRDIFF_T_) sc->first_tx_desc) >> 4));
	} /* end of while loop */

	splx(s);

	/* Set timeout in case chip has problems transmitting */
	ifp->if_timer = EM_TX_TIMEOUT;

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
em_ioctl(struct ifnet *ifp, IOCTL_CMD_TYPE command, caddr_t data)
{
	int             s, error = 0;
	struct ifreq   *ifr = (struct ifreq *) data;
	struct ifaddr  *ifa = (struct ifaddr *)data;
	struct em_softc * sc = ifp->if_softc;

	s = splimp();

        if ((error = ether_ioctl(ifp, &sc->arpcom, command, data)) > 0) {
                splx(s);
                return (error);
        }

	switch (command) {
	case SIOCSIFADDR:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFADDR (Set Interface "
			       "Addr)");
		ifp->if_flags |= IFF_UP;
                switch (ifa->ifa_addr->sa_family) {
#ifdef INET
                case AF_INET:
                        em_init(sc);
                        arp_ifinit(&sc->arpcom, ifa);
                        break;
#endif /* INET */
                default:
                        em_init(sc);
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
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCSIFFLAGS (Set Interface "
			       "Flags)");
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC) {
				em_set_promisc(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
				   !(ifp->if_flags & IFF_PROMISC)) {
				em_disable_promisc(sc);
			} else
				em_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING) {
				em_stop(sc);
			}
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOC(ADD|DEL)MULTI");
#if 0
		if (ifp->if_flags & IFF_RUNNING) {
			em_disable_intr(sc);
			em_set_multi(sc);
			if (sc->hw.mac_type == em_82542_rev2_0)
				em_initialize_receive_unit(sc);
			em_enable_intr(sc);
		}
		break;
#endif /* 0 */
                error = (command == SIOCADDMULTI)
                        ? ether_addmulti(ifr, &sc->arpcom)
                        : ether_delmulti(ifr, &sc->arpcom);

                if (error == ENETRESET) {
                        if (ifp->if_flags & IFF_RUNNING) {
				em_disable_intr(sc);
				em_set_multi(sc);
				if (sc->hw.mac_type == em_82542_rev2_0)
					em_initialize_receive_unit(sc);
				em_enable_intr(sc);
			}
                        error = 0;
                }
		break;
	case SIOCSIFMEDIA:
	case SIOCGIFMEDIA:
		IOCTL_DEBUGOUT("ioctl rcv'd: SIOCxIFMEDIA (Get/Set Interface "
			       "Media)");
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;
#if 0
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
#endif /* 0 */
	default:
		IOCTL_DEBUGOUT1("ioctl received: UNKNOWN (0x%d)\n",
				(int)command);
		error = EINVAL;
	}

	splx(s);
	return(error);
}

void
em_set_promisc(struct em_softc * sc)
{

	u_int32_t       reg_rctl;
	struct ifnet   *ifp = &sc->arpcom.ac_if;

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
	u_int32_t       reg_rctl;

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
	u_int16_t pci_cmd_word;
#if 0
	struct ifmultiaddr  *ifma;
#endif
	int mcnt = 0;
        struct pci_attach_args *pa = &sc->osdep.em_pa;
#if 0
	struct ifnet   *ifp = &sc->arpcom.ac_if;
#endif

	IOCTL_DEBUGOUT("em_set_multi: begin");

	if (sc->hw.mac_type == em_82542_rev2_0) {
		reg_rctl = E1000_READ_REG(&sc->hw, RCTL);
		if (sc->hw.pci_cmd_word & CMD_MEM_WRT_INVALIDATE) {
			pci_cmd_word = sc->hw.pci_cmd_word & 
				       ~CMD_MEM_WRT_INVALIDATE;
			pci_conf_write(pa->pa_pc, pa->pa_tag,
				       PCI_COMMAND_STATUS_REG, pci_cmd_word);
		}
		reg_rctl |= E1000_RCTL_RST;
		E1000_WRITE_REG(&sc->hw, RCTL, reg_rctl);
		msec_delay(5);
	}

#if 0
#if __FreeBSD_version < 500000 
	LIST_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#else
	TAILQ_FOREACH(ifma, &ifp->if_multiaddrs, ifma_link) {
#endif
		if (ifma->ifma_addr->sa_family != AF_LINK)
			continue;

		bcopy(LLADDR((struct sockaddr_dl *)ifma->ifma_addr),
		      &mta[mcnt*ETH_LENGTH_OF_ADDRESS], ETH_LENGTH_OF_ADDRESS);
		mcnt++;
	}
#endif /* 0 */

	if (mcnt > MAX_NUM_MULTICAST_ADDRESSES) {
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
			pci_conf_write(pa->pa_pc, pa->pa_tag,
				       PCI_COMMAND_STATUS_REG, 
				       sc->hw.pci_cmd_word);
		}
	}

	return;
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
	ifp = &sc->arpcom.ac_if;

	s = splimp();

	em_check_for_link(&sc->hw);
	em_print_link_status(sc);
	em_update_stats_counters(sc);   
	if (em_display_debug_stats && ifp->if_flags & IFF_RUNNING) {
		em_print_hw_stats(sc);
	}
	timeout_add(&sc->em_timeout, 2*hz);

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
	int             s;
	struct ifnet   *ifp;
	struct em_softc * sc= arg;

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

	em_enable_vlans(sc);

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

	ifp = &sc->arpcom.ac_if;
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

#if 0
	if (sc->hw.mac_type >= em_82543) {
		if (ifp->if_capenable & IFCAP_TXCSUM)
			ifp->if_hwassist = EM_CHECKSUM_FEATURES;
		else
			ifp->if_hwassist = 0;
	}
#endif /* 0 */

	timeout_add(&sc->em_timeout, 2*hz);
	em_clear_hw_cntrs(&sc->hw);
	em_enable_intr(sc);

	splx(s);
	return;
}


/*********************************************************************
 *
 *  This routine disables all traffic on the em_softc by issuing a
 *  global reset on the MAC and deallocates TX/RX buffers. 
 *
 **********************************************************************/

void
em_stop(void *arg)
{
	struct ifnet   *ifp;
	struct em_softc * sc = arg;
	ifp = &sc->arpcom.ac_if;

	INIT_DEBUGOUT("em_stop: begin\n");
	em_disable_intr(sc);
	em_reset_hw(&sc->hw);
	timeout_del(&sc->em_timeout);
	em_free_transmit_structures(sc);
	em_free_receive_structures(sc);


	/* Tell the stack that the interface is no longer active */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

/*********************************************************************
 *
 *  Interrupt Service routine
 *
 **********************************************************************/

int 
em_intr(void *arg)
{
	u_int32_t       loop_cnt = EM_MAX_INTR;
	u_int32_t       reg_icr;
	struct ifnet    *ifp;
	struct em_softc *sc= arg;

	ifp = &sc->arpcom.ac_if;

	em_disable_intr(sc);
	while (loop_cnt > 0 && 
	       (reg_icr = E1000_READ_REG(&sc->hw, ICR)) != 0) {

		/* Link status change */
		if (reg_icr & (E1000_ICR_RXSEQ | E1000_ICR_LSC)) {
			timeout_del(&sc->em_timeout);
			sc->hw.get_link_status = 1;
			em_check_for_link(&sc->hw);
			em_print_link_status(sc);
			timeout_add(&sc->em_timeout, 2*hz); 
		}

		if (ifp->if_flags & IFF_RUNNING) {
			em_process_receive_interrupts(sc);
			em_clean_transmit_interrupts(sc);
		}
		loop_cnt--;
	}

	em_enable_intr(sc);

	if (ifp->if_flags & IFF_RUNNING && IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		em_start(ifp);

	return (EM_MAX_INTR != loop_cnt);
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
#if __FreeBSD_version < 500000 
			ifmr->ifm_active |= IFM_1000_TX;
#else
			ifmr->ifm_active |= IFM_1000_T;
#endif
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
	struct ifmedia  *ifm = &sc->media;

	INIT_DEBUGOUT("em_media_change: begin");

	if (IFM_TYPE(ifm->ifm_media) != IFM_ETHER)
		return(EINVAL);

	switch (IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_AUTO:
		sc->hw.autoneg = DO_AUTO_NEG;
		sc->hw.autoneg_advertised = AUTONEG_ADV_DEFAULT;
		break;
	case IFM_1000_SX:
#if __FreeBSD_version < 500000 
	case IFM_1000_TX:
#else
	case IFM_1000_T:
#endif
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

	em_init(sc);

	return(0);
}
/* Section end: Other registered entry points */


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
		printf("%s: Memory Access and/or Bus Master bits not set!\n", 
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

	/* Set MacType, etc. based on this PCI info */
	switch (sc->hw.device_id) {
	case E1000_DEV_ID_82542:
		sc->hw.mac_type = (sc->hw.revision_id == 3) ?
				       em_82542_rev2_1 : em_82542_rev2_0;
		break;
	case E1000_DEV_ID_82543GC_FIBER:
	case E1000_DEV_ID_82543GC_COPPER:
		sc->hw.mac_type = em_82543;
		break;
	case E1000_DEV_ID_82544EI_FIBER:
	case E1000_DEV_ID_82544EI_COPPER:
	case E1000_DEV_ID_82544GC_COPPER:
	case E1000_DEV_ID_82544GC_LOM:
		sc->hw.mac_type = em_82544;
		break;
	case E1000_DEV_ID_82540EM:
		sc->hw.mac_type = em_82540;
		break;
	case E1000_DEV_ID_82545EM_FIBER:
	case E1000_DEV_ID_82545EM_COPPER:
		sc->hw.mac_type = em_82545;
		break;
	case E1000_DEV_ID_82546EB_FIBER:
	case E1000_DEV_ID_82546EB_COPPER:
		sc->hw.mac_type = em_82546;
		break;
	default:
		INIT_DEBUGOUT1("Unknown device id 0x%x", sc->hw.device_id);
	}
	return;
}

int
em_allocate_pci_resources(struct em_softc * sc)
{
	int             i, val, rid;
	pci_intr_handle_t       ih;
	const char              *intrstr = NULL;
	struct pci_attach_args *pa = &sc->osdep.em_pa;
	pci_chipset_tag_t       pc = pa->pa_pc;

	val = pci_conf_read(pa->pa_pc, pa->pa_tag, EM_MMBA);
	if (PCI_MAPREG_TYPE(val) != PCI_MAPREG_TYPE_MEM) {
		printf(": mmba isn't memory");
		return (ENXIO);
        }
        if (pci_mapreg_map(pa, EM_MMBA, PCI_MAPREG_MEM_TYPE(val), 0,
	    &sc->osdep.em_btag, &sc->osdep.em_bhandle,
	    &sc->osdep.em_membase, &sc->osdep.em_memsize, 0)) {
		printf(": can't find mem space");
		return (ENXIO);
	}

#if 0
        if (pci_mapreg_map(pa, EM_MMBA, PCI_MAPREG_MEM_TYPE_32BIT, 0,
	    &sc->osdep.em_btag, &sc->osdep.em_bhandle, &sc->osdep.em_membase,
	    &sc->osdep.em_memsize, 0) &&
       	    pci_mapreg_map(pa, EM_MMBA, PCI_MAPREG_MEM_TYPE_64BIT, 0,
    	    &sc->osdep.em_btag, &sc->osdep.em_bhandle, &sc->osdep.em_membase,
	    	&sc->osdep.em_memsize, 0)) {
                printf(": can't find mem space");
                return (ENXIO);
        }
#endif /* 0 */

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
        printf(": %s\n", intrstr);
		
	sc->hw.back = &sc->osdep;

	return(0);
}

void
em_free_pci_resources(struct em_softc* sc)
{
	struct pci_attach_args *pa = &sc->osdep.em_pa;
	pci_chipset_tag_t       pc = pa->pa_pc;

	if(sc->sc_intrhand)
		pci_intr_disestablish(pc, sc->sc_intrhand);
	sc->sc_intrhand = 0;

	if(sc->osdep.em_iobase)
		bus_space_unmap(sc->osdep.em_iobtag, sc->osdep.em_iobhandle,
				sc->osdep.em_iosize);
	sc->osdep.em_iobase = 0;

	if(sc->osdep.em_membase)
		bus_space_unmap(sc->osdep.em_btag, sc->osdep.em_bhandle,
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

	ifp = &sc->arpcom.ac_if;
	ifp->if_mtu = ETHERMTU;
	ifp->if_output = ether_output;
	ifp->if_baudrate = 1000000000;
#if 0
	ifp->if_init =  em_init;
#endif
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = em_ioctl;
	ifp->if_start = em_start;
	ifp->if_watchdog = em_watchdog;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->num_tx_desc - 1);
	IFQ_SET_READY(&ifp->if_snd);

	bcopy(sc->sc_dv.dv_xname, ifp->if_xname, IFNAMSIZ);
	

#if 0
	if (sc->hw.mac_type >= em_82543) {
		ifp->if_capabilities = IFCAP_HWCSUM;
		ifp->if_capenable = ifp->if_capabilities;
	}
#endif /* 0 */

	/* 
	 * Specify the media types supported by this em_softc and register
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
#if __FreeBSD_version < 500000 
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_TX | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_TX, 0, NULL);
#else
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T | IFM_FDX, 
			    0, NULL);
		ifmedia_add(&sc->media, IFM_ETHER | IFM_1000_T, 0, NULL);
#endif
	}
	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);

	if_attach(ifp);
	ether_ifattach(ifp);

	return;
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
	      (struct em_tx_buffer *) malloc(sizeof(struct em_tx_buffer) *
					     sc->num_tx_desc, M_DEVBUF,
					     M_NOWAIT))) {
		printf("%s: Unable to allocate tx_buffer memory\n", 
		       sc->sc_dv.dv_xname);
		return ENOMEM;
	}

	bzero(sc->tx_buffer_area,
	      sizeof(struct em_tx_buffer) * sc->num_tx_desc);

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
	struct em_tx_buffer   *tx_buffer;
	int             i;

	if (em_allocate_transmit_structures(sc))
		return ENOMEM;

	sc->first_tx_desc = sc->tx_desc_base;
	sc->last_tx_desc =
	sc->first_tx_desc + (sc->num_tx_desc - 1);


	SIMPLEQ_INIT(&sc->free_tx_buffer_list);
	SIMPLEQ_INIT(&sc->used_tx_buffer_list);

	tx_buffer = sc->tx_buffer_area;

	/* Setup the linked list of the tx_buffer's */
	for (i = 0; i < sc->num_tx_desc; i++, tx_buffer++) {
		bzero((void *) tx_buffer, sizeof(struct em_tx_buffer));
		if (bus_dmamap_create(sc->osdep.em_pa.pa_dmat, MCLBYTES, 32,
				      MCLBYTES, 0, BUS_DMA_NOWAIT,
				      &tx_buffer->dmamap))
			return ENOBUFS;
		SIMPLEQ_INSERT_TAIL(&sc->free_tx_buffer_list, 
				   tx_buffer, em_tx_entry);
	}

	bzero((void *) sc->first_tx_desc,
	      (sizeof(struct em_tx_desc)) * sc->num_tx_desc);

	/* Setup TX descriptor pointers */
	sc->next_avail_tx_desc = sc->first_tx_desc;
	sc->oldest_used_tx_desc = sc->first_tx_desc;

	/* Set number of descriptors available */
	sc->num_tx_desc_avail = sc->num_tx_desc;

	/* Set checksum context */
	sc->active_checksum_context = OFFLOAD_NONE;

	return 0;
}

/*********************************************************************
 *
 *  Enable transmit unit.
 *
 **********************************************************************/
void
em_initialize_transmit_unit(struct em_softc * sc)
{
	u_int32_t       reg_tctl;
	u_int32_t       reg_tipg = 0;

	/* Setup the Base and Length of the Tx Descriptor Ring */
	E1000_WRITE_REG(&sc->hw, TDBAL,
			sc->osdep.em_tx.emm_dmamap->dm_segs[0].ds_addr);
	E1000_WRITE_REG(&sc->hw, TDBAH, 0);
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
	case em_82543:
	case em_82544:
	case em_82540:
	case em_82545:
	case em_82546:
		if (sc->hw.media_type == em_media_type_fiber)
			reg_tipg = DEFAULT_82543_TIPG_IPGT_FIBER;
		else
			reg_tipg = DEFAULT_82543_TIPG_IPGT_COPPER;
		reg_tipg |= DEFAULT_82543_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		reg_tipg |= DEFAULT_82543_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	case em_82542_rev2_0:
	case em_82542_rev2_1:
		reg_tipg = DEFAULT_82542_TIPG_IPGT;
		reg_tipg |= DEFAULT_82542_TIPG_IPGR1 << E1000_TIPG_IPGR1_SHIFT;
		reg_tipg |= DEFAULT_82542_TIPG_IPGR2 << E1000_TIPG_IPGR2_SHIFT;
		break;
	default:
		printf("%s: Invalid mac type detected\n", sc->sc_dv.dv_xname);
	}
	E1000_WRITE_REG(&sc->hw, TIPG, reg_tipg);
	E1000_WRITE_REG(&sc->hw, TIDV, sc->tx_int_delay);

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
	sc->txd_cmd = E1000_TXD_CMD_IFCS;

	if (sc->tx_int_delay > 0)
		sc->txd_cmd |= E1000_TXD_CMD_IDE;

	if (sc->hw.report_tx_early == 1)
		sc->txd_cmd |= E1000_TXD_CMD_RS;
	else
		sc->txd_cmd |= E1000_TXD_CMD_RPS;

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
	struct em_tx_buffer   *tx_buffer;
	int             i;

	INIT_DEBUGOUT("free_transmit_structures: begin");

	if (sc->tx_buffer_area != NULL) {
		tx_buffer = sc->tx_buffer_area;
		for (i = 0; i < sc->num_tx_desc; i++, tx_buffer++) {
			if (tx_buffer->m_head != NULL)
				m_freem(tx_buffer->m_head);
			tx_buffer->m_head = NULL;
		}
	}
	if (sc->tx_buffer_area != NULL) {
		free(sc->tx_buffer_area, M_DEVBUF);
		sc->tx_buffer_area = NULL;
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
#if 0
void
em_transmit_checksum_setup(struct em_softc * sc,
			   struct mbuf *mp,
			   struct em_tx_buffer *tx_buffer,
			   u_int32_t *txd_upper,
			   u_int32_t *txd_lower) 
{
	struct em_context_desc *TXD;
	struct em_tx_desc * current_tx_desc;
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
	current_tx_desc = sc->next_avail_tx_desc;
	TXD = (struct em_context_desc *)current_tx_desc;

	TXD->lower_setup.ip_fields.ipcss = ETHER_HDR_LEN;
	TXD->lower_setup.ip_fields.ipcso = 
	ETHER_HDR_LEN + offsetof(struct ip, ip_sum);
	TXD->lower_setup.ip_fields.ipcse = 
	ETHER_HDR_LEN + sizeof(struct ip) - 1;

	TXD->upper_setup.tcp_fields.tucss = 
	ETHER_HDR_LEN + sizeof(struct ip);
	TXD->upper_setup.tcp_fields.tucse = 0;

	if (sc->active_checksum_context == OFFLOAD_TCP_IP) {
		TXD->upper_setup.tcp_fields.tucso = 
		ETHER_HDR_LEN + sizeof(struct ip) + 
		offsetof(struct tcphdr, th_sum);
	} else if (sc->active_checksum_context == OFFLOAD_UDP_IP) {
		TXD->upper_setup.tcp_fields.tucso = 
		ETHER_HDR_LEN + sizeof(struct ip) + 
		offsetof(struct udphdr, uh_sum);
	}

	TXD->tcp_seg_setup.data = 0;
	TXD->cmd_and_length = E1000_TXD_CMD_DEXT;

	if (current_tx_desc == sc->last_tx_desc)
		sc->next_avail_tx_desc = sc->first_tx_desc;
	else
		sc->next_avail_tx_desc++;

	sc->num_tx_desc_avail--;

	tx_buffer->num_tx_desc_used++;
	return;
}
#endif /* 0 */


/*********************************************************************
 *
 *  Get a buffer from system mbuf buffer pool.
 *
 **********************************************************************/
int
em_get_buf(struct em_rx_buffer *rx_buffer, struct em_softc *sc,
	   struct mbuf *mp)
{
	struct mbuf    *nmp;
	struct ifnet   *ifp;

	ifp = &sc->arpcom.ac_if;

	if (mp == NULL) {
		MGETHDR(nmp, M_DONTWAIT, MT_DATA);
		if (nmp == NULL) {
			sc->mbuf_alloc_failed++;
			return(ENOBUFS);
		}
		MCLGET(nmp, M_DONTWAIT);
		if ((nmp->m_flags & M_EXT) == 0) {
			m_freem(nmp);
			sc->mbuf_cluster_failed++;
			return(ENOBUFS);
		}
		nmp->m_len = nmp->m_pkthdr.len = MCLBYTES;
	} else {
		nmp = mp;
		nmp->m_len = nmp->m_pkthdr.len = MCLBYTES;
		nmp->m_data = nmp->m_ext.ext_buf;
		nmp->m_next = NULL;
	}

	if (bus_dmamap_load_mbuf(sc->osdep.em_pa.pa_dmat,
				 rx_buffer->dmamap,
				 nmp, BUS_DMA_NOWAIT))
		return(ENOBUFS);

	if (ifp->if_mtu <= ETHERMTU)
		m_adj(nmp, ETHER_ALIGN);

	rx_buffer->m_head = nmp;
	rx_buffer->buffer_addr = 
		rx_buffer->dmamap->dm_segs[0].ds_addr + ETHER_ALIGN;

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
	int             i;
	struct em_rx_buffer   *rx_buffer;

	if (!(sc->rx_buffer_area =
	      (struct em_rx_buffer *) malloc(sizeof(struct em_rx_buffer) *
					     sc->num_rx_desc, M_DEVBUF,
					     M_NOWAIT))) {
		printf("%s: Unable to allocate rx_buffer memory\n", 
		       sc->sc_dv.dv_xname);
		return(ENOMEM);
	}

	bzero(sc->rx_buffer_area,
	      sizeof(struct em_rx_buffer) * sc->num_rx_desc);

	for (i = 0, rx_buffer = sc->rx_buffer_area;
	    i < sc->num_rx_desc; i++, rx_buffer++) {

		if (bus_dmamap_create(sc->osdep.em_pa.pa_dmat,
				      MCLBYTES, 1, MCLBYTES, 0,
				      BUS_DMA_NOWAIT,
				      &rx_buffer->dmamap))
			return ENOBUFS;

		if (em_get_buf(rx_buffer, sc, NULL) == ENOBUFS) {
			rx_buffer->m_head = NULL;
			return(ENOBUFS);
		}
	}

	return(0);
}

/*********************************************************************
 *
 *  Allocate and initialize receive structures.
 *  
 **********************************************************************/
int
em_setup_receive_structures(struct em_softc * sc)
{
	struct em_rx_buffer   *rx_buffer;
	struct em_rx_desc     *rx_desc;
	int             i;

	if (em_allocate_receive_structures(sc))
		return ENOMEM;

	SIMPLEQ_INIT(&sc->rx_buffer_list);

	sc->first_rx_desc =
	(struct em_rx_desc *) sc->rx_desc_base;
	sc->last_rx_desc =
	sc->first_rx_desc + (sc->num_rx_desc - 1);

	rx_buffer = (struct em_rx_buffer *) sc->rx_buffer_area;

	bzero((void *) sc->first_rx_desc,
	      (sizeof(struct em_rx_desc)) * sc->num_rx_desc);

	/* Build a linked list of rx_buffer's */
	for (i = 0, rx_desc = sc->first_rx_desc;
	    i < sc->num_rx_desc;
	    i++, rx_buffer++, rx_desc++) {
		if (rx_buffer->m_head == NULL)
			printf("%s: Receive buffer memory not allocated", 
			       sc->sc_dv.dv_xname);
		else {
			rx_desc->buffer_addr = htole64(rx_buffer->buffer_addr);
			SIMPLEQ_INSERT_TAIL(&sc->rx_buffer_list, 
					   rx_buffer, em_rx_entry);
		}
	}

	/* Setup our descriptor pointers */
	sc->next_rx_desc_to_check = sc->first_rx_desc;

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
	u_int32_t       reg_rctl;
#if 0
	u_int32_t       reg_rxcsum;
#endif
	struct ifnet    *ifp;

	ifp = &sc->arpcom.ac_if;

	/*
	 * Make sure receives are disabled while setting up the
	 * descriptor ring
	 */
	E1000_WRITE_REG(&sc->hw, RCTL, 0);

	/* Set the Receive Delay Timer Register */
	E1000_WRITE_REG(&sc->hw, RDTR, 
			sc->rx_int_delay | E1000_RDT_FPDB);

	/* Setup the Base and Length of the Rx Descriptor Ring */
	E1000_WRITE_REG(&sc->hw, RDBAL, 
			sc->osdep.em_rx.emm_dmamap->dm_segs[0].ds_addr);
	E1000_WRITE_REG(&sc->hw, RDBAH, 0);
	E1000_WRITE_REG(&sc->hw, RDLEN, sc->num_rx_desc *
			sizeof(struct em_rx_desc));

	/* Setup the HW Rx Head and Tail Descriptor Pointers */
	E1000_WRITE_REG(&sc->hw, RDH, 0);
	E1000_WRITE_REG(&sc->hw, RDT,
			(((_BSD_PTRDIFF_T_) sc->last_rx_desc -
			  (_BSD_PTRDIFF_T_) sc->first_rx_desc) >> 4));

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

#if 0
	/* Enable 82543 Receive Checksum Offload for TCP and UDP */
	if ((sc->hw.mac_type >= em_82543) && 
	    (ifp->if_capenable & IFCAP_RXCSUM)) {
		reg_rxcsum = E1000_READ_REG(&sc->hw, RXCSUM);
		reg_rxcsum |= (E1000_RXCSUM_IPOFL | E1000_RXCSUM_TUOFL);
		E1000_WRITE_REG(&sc->hw, RXCSUM, reg_rxcsum);
	}
#endif /* 0 */

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
	struct em_rx_buffer   *rx_buffer;
	int             i;

	INIT_DEBUGOUT("free_receive_structures: begin");

	if (sc->rx_buffer_area != NULL) {
		rx_buffer = sc->rx_buffer_area;
		for (i = 0; i < sc->num_rx_desc; i++, rx_buffer++) {
			if (rx_buffer->m_head != NULL)
				m_freem(rx_buffer->m_head);
			rx_buffer->m_head = NULL;
		}
	}
	if (sc->rx_buffer_area != NULL) {
		free(sc->rx_buffer_area, M_DEVBUF);
		sc->rx_buffer_area = NULL;
	}
	return;
}

/*********************************************************************
 *
 *  This routine executes in interrupt context. It replenishes
 *  the mbufs in the descriptor and sends data which has been
 *  dma'ed into host memory to upper layer.
 *
 *********************************************************************/
void
em_process_receive_interrupts(struct em_softc* sc)
{
	struct mbuf         *mp;
	struct ifnet        *ifp;
	struct ether_header *eh;
	u_int16_t           len;
	u_int8_t            last_byte;
	u_int8_t            accept_frame = 0;
	u_int8_t            eop = 0;
	u_int32_t           pkt_len = 0;

	/* Pointer to the receive descriptor being examined. */
	struct em_rx_desc   *current_desc;
	struct em_rx_desc   *last_desc_processed;
	struct em_rx_buffer *rx_buffer;

	ifp = &sc->arpcom.ac_if;
	current_desc = sc->next_rx_desc_to_check;

	if (!((current_desc->status) & E1000_RXD_STAT_DD)) {
#ifdef DBG_STATS
		sc->no_pkts_avail++;
#endif
		return;
	}

	while (current_desc->status & E1000_RXD_STAT_DD) {

		/* Get a pointer to the actual receive buffer */
		rx_buffer = SIMPLEQ_FIRST(&sc->rx_buffer_list);

		if (rx_buffer == NULL) {
			printf("%s: Found null rx_buffer\n",
			       sc->sc_dv.dv_xname);
			return;
		}

		mp = rx_buffer->m_head;      
		accept_frame = 1;

		if (current_desc->status & E1000_RXD_STAT_EOP) {
			eop = 1;
			len = letoh16(current_desc->length) - ETHER_CRC_LEN;
		} else {
			eop = 0;
			len = letoh16(current_desc->length);
		}

		if (current_desc->errors & E1000_RXD_ERR_FRAME_ERR_MASK) {

			/* Compute packet length for tbi_accept macro */
			pkt_len = letoh16(current_desc->length);
			if (sc->fmp != NULL) {
				pkt_len += sc->fmp->m_pkthdr.len; 
			}

			last_byte = *(mtod(rx_buffer->m_head,caddr_t) + 
				      letoh16(current_desc->length) - 1);

			if (TBI_ACCEPT(&sc->hw, current_desc->status, 
				       current_desc->errors, 
				       pkt_len, last_byte)) {
				em_tbi_adjust_stats(&sc->hw, 
						    &sc->stats, 
						    pkt_len, 
						    sc->hw.mac_addr);
				len--;
			} else {
				accept_frame = 0;
			}
		}

		if (accept_frame) {

			if (em_get_buf(rx_buffer, sc, NULL) == ENOBUFS) {
				sc->dropped_pkts++;
				em_get_buf(rx_buffer, sc, mp);
				if (sc->fmp != NULL) m_freem(sc->fmp);
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
#if NBPFILTER > 0
				/*
				 * Handle BPF listeners. Let the BPF
				 * user see the packet.
				 */
				if (ifp->if_bpf)
					bpf_mtap(ifp->if_bpf, sc->fmp);
#endif


				eh = mtod(sc->fmp, struct ether_header *);

				/* Remove ethernet header from mbuf */
				m_adj(sc->fmp, sizeof(struct ether_header));
				em_receive_checksum(sc, current_desc, 
						    sc->fmp);
#if 0
				if (current_desc->status & E1000_RXD_STAT_VP)
					VLAN_INPUT_TAG(eh, sc->fmp, 
					     letoh16(current_desc->special));
				else
#endif /* 0 */
					ether_input(ifp, eh, sc->fmp);

				sc->fmp = NULL;
				sc->lmp = NULL;
			}
		} else {
			sc->dropped_pkts++;
			em_get_buf(rx_buffer, sc, mp);
			if (sc->fmp != NULL) m_freem(sc->fmp);
			sc->fmp = NULL;
			sc->lmp = NULL;
		}

		/* Zero out the receive descriptors status  */
		current_desc->status = 0;

		if (rx_buffer->m_head != NULL) {
			current_desc->buffer_addr =
				htole64(rx_buffer->buffer_addr);
		}

		/* Advance our pointers to the next descriptor
		 * (checking for wrap). */
		if (current_desc == sc->last_rx_desc)
			sc->next_rx_desc_to_check = sc->first_rx_desc;
		else
			((sc)->next_rx_desc_to_check)++;

		last_desc_processed = current_desc;
		current_desc = sc->next_rx_desc_to_check;
		/* 
		 * Put the buffer that we just indicated back at the
		 * end of our list
		 */
		SIMPLEQ_REMOVE_HEAD(&sc->rx_buffer_list, rx_buffer,
				    em_rx_entry);
		SIMPLEQ_INSERT_TAIL(&sc->rx_buffer_list, 
				   rx_buffer, em_rx_entry);

		/* Advance the E1000's Receive Queue #0  "Tail Pointer". */
		E1000_WRITE_REG(&sc->hw, RDT, 
				(((u_long) last_desc_processed -
				  (u_long) sc->first_rx_desc) >> 4));
	}
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
#if 0
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
#endif /* 0 */
}


void em_enable_vlans(struct em_softc * sc)
{
	uint32_t ctrl;

	E1000_WRITE_REG(&sc->hw, VET, QTAG_TYPE);

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

void em_write_pci_cfg(struct em_hw *hw,
		      uint32_t reg,
		      uint16_t *value)
{
        struct pci_attach_args *pa = &((struct em_osdep *)hw->back)->em_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_conf_write(pc, pa->pa_tag, reg, *value);
}

void em_read_pci_cfg(struct em_hw *hw, uint32_t reg,
		     uint16_t *value)
{
        struct pci_attach_args *pa = &((struct em_osdep *)hw->back)->em_pa;
	pci_chipset_tag_t pc = pa->pa_pc;
	*value = pci_conf_read(pc, pa->pa_tag, reg);
	return;
}

uint32_t em_io_read(struct em_hw *hw, uint32_t port)
{
#if 0
	return(inl(port));
#endif
	return bus_space_read_4(
                ((struct em_osdep *)(hw)->back)->em_iobtag,
		((struct em_osdep *)(hw)->back)->em_iobhandle,
		port);
}

void em_io_write(struct em_hw *hw, uint32_t port, uint32_t value)
{
#if 0
	outl(port, value);
#endif
	bus_space_write_4(
			((struct em_osdep *)(hw)->back)->em_iobtag,
			((struct em_osdep *)(hw)->back)->em_iobhandle,
			port,
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
	ifp = &sc->arpcom.ac_if;

	/* Fill out the OS statistics structure */
	ifp->if_ipackets = sc->stats.gprc;
	ifp->if_opackets = sc->stats.gptc;
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
em_print_hw_stats(struct em_softc *sc)
{
#ifdef DBG_STATS
	printf("%s: Packets not Avail = %ld\n", sc->sc_dv.dv_xname, 
	       sc->no_pkts_avail);
	printf("%s: CleanTxInterrupts = %ld\n", sc->sc_dv.dv_xname, 
	       sc->clean_tx_interrupts);
#endif

	printf("%s: Tx Descriptors not Avail = %ld\n", sc->sc_dv.dv_xname, 
	       sc->no_tx_desc_avail);
	printf("%s: Tx Buffer not avail1 = %ld\n", sc->sc_dv.dv_xname, 
	       sc->no_tx_buffer_avail1);
	printf("%s: Tx Buffer not avail2 = %ld\n", sc->sc_dv.dv_xname, 
	       sc->no_tx_buffer_avail2);
	printf("%s: Std Mbuf Failed = %ld\n",sc->sc_dv.dv_xname, 
	       sc->mbuf_alloc_failed);
	printf("%s: Std Cluster Failed = %ld\n",sc->sc_dv.dv_xname, 
	       sc->mbuf_cluster_failed);

	printf("%s: Symbol errors = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.symerrs);
	printf("%s: Sequence errors = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.sec);
	printf("%s: Defer count = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.dc);

	printf("%s: Missed Packets = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.mpc);
	printf("%s: Receive No Buffers = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.rnbc);
	printf("%s: Receive length errors = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.rlec);
	printf("%s: Receive errors = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.rxerrc);
	printf("%s: Crc errors = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.crcerrs);
	printf("%s: Alignment errors = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.algnerrc);
	printf("%s: Carrier extension errors = %lld\n", sc->sc_dv.dv_xname,
	       (long long)sc->stats.cexterr);
	printf("%s: Driver dropped packets = %ld\n", sc->sc_dv.dv_xname, 
	       sc->dropped_pkts);

	printf("%s: XON Rcvd = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.xonrxc);
	printf("%s: XON Xmtd = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.xontxc);
	printf("%s: XOFF Rcvd = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.xoffrxc);
	printf("%s: XOFF Xmtd = %lld\n", sc->sc_dv.dv_xname, 
	       (long long)sc->stats.xofftxc);

	printf("%s: Good Packets Rcvd = %lld\n", sc->sc_dv.dv_xname,
	       (long long)sc->stats.gprc);
	printf("%s: Good Packets Xmtd = %lld\n", sc->sc_dv.dv_xname,
	       (long long)sc->stats.gptc);
}


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
	struct em_tx_buffer *tx_buffer;
	struct em_tx_desc   *tx_desc;
	int             s;
	struct ifnet   *ifp;

	s = splimp();
#ifdef DBG_STATS
	sc->clean_tx_interrupts++;
#endif

	for (tx_buffer = SIMPLEQ_FIRST(&sc->used_tx_buffer_list);
	    tx_buffer; 
	    tx_buffer = SIMPLEQ_FIRST(&sc->used_tx_buffer_list)) {

		/* 
		 * Get hold of the next descriptor that the em will
		 * report status back to (this will be the last
		 * descriptor of a given tx_buffer). We only want to
		 * free the tx_buffer (and it resources) if the driver
		 * is done with ALL of the descriptors.  If the driver
		 * is done with the last one then it is done with all
		 * of them.
		 */

		tx_desc = sc->oldest_used_tx_desc +
			  (tx_buffer->num_tx_desc_used - 1);

		/* Check for wrap case */
		if (tx_desc > sc->last_tx_desc)
			tx_desc -= sc->num_tx_desc;


		/* 
		 * If the descriptor done bit is set free tx_buffer
		 * and associated resources
		 */
		if (tx_desc->upper.fields.status & E1000_TXD_STAT_DD) {

			SIMPLEQ_REMOVE_HEAD(&sc->used_tx_buffer_list, 
					   tx_buffer,
					   em_tx_entry);

			if ((tx_desc == sc->last_tx_desc))
				sc->oldest_used_tx_desc =
				sc->first_tx_desc;
			else
				sc->oldest_used_tx_desc = (tx_desc + 1);

			/* Make available the descriptors that were
			 * previously used */
			sc->num_tx_desc_avail +=
			tx_buffer->num_tx_desc_used;

			tx_buffer->num_tx_desc_used = 0;

			if (tx_buffer->m_head) {
				m_freem(tx_buffer->m_head);
				tx_buffer->m_head = NULL;
			}
			/* Return this "Software packet" back to the
			 * "free" list */
			SIMPLEQ_INSERT_TAIL(&sc->free_tx_buffer_list, 
					   tx_buffer, em_tx_entry);
		} else {
			/* 
			 * Found a tx_buffer that the em is not done
			 * with then there is no reason to check the
			 * rest of the queue.
			 */
			break;
		}
	}		      /* end for each tx_buffer */

	ifp = &sc->arpcom.ac_if;

	/* Tell the stack that it is OK to send packets */
	if (sc->num_tx_desc_avail > TX_CLEANUP_THRESHOLD) {
		ifp->if_timer = 0;
		ifp->if_flags &= ~IFF_OACTIVE;
	}
	splx(s);
	return;
}

int em_malloc_dma(struct em_softc *sc, struct em_dmamap *emm,
			 bus_size_t size)
{
	bus_dma_tag_t	dma_tag = sc->osdep.em_pa.pa_dmat;

	emm->emm_size = size;

        if (bus_dmamem_alloc(dma_tag, size, PAGE_SIZE, 0, &emm->emm_seg, 1,
			     &emm->emm_rseg, BUS_DMA_NOWAIT)) {
		goto fail0;
        }
        if (bus_dmamem_map(dma_tag, &emm->emm_seg, emm->emm_rseg, size,
			   &emm->emm_kva, BUS_DMA_NOWAIT)) {
		goto fail1;
        }
        if (bus_dmamap_create(dma_tag, size, 1, size, 0, BUS_DMA_NOWAIT,
			      &emm->emm_dmamap)) {
		goto fail2;
        }
        if (bus_dmamap_load(dma_tag, emm->emm_dmamap, emm->emm_kva, size,
			    NULL, BUS_DMA_NOWAIT)) {
		goto fail3;
        }
       	 
	return 0;

 fail3:
	bus_dmamap_destroy(dma_tag, emm->emm_dmamap);
 fail2:
	bus_dmamem_unmap(dma_tag, emm->emm_kva, size);
 fail1:
	bus_dmamem_free(dma_tag, &emm->emm_seg, emm->emm_rseg);
 fail0:
	return (ENOBUFS);
}

void em_free_dma(struct em_softc *sc, struct em_dmamap *emm)
{
	bus_dmamap_unload(sc->osdep.em_pa.pa_dmat, emm->emm_dmamap);
	bus_dmamap_destroy(sc->osdep.em_pa.pa_dmat, emm->emm_dmamap);
	bus_dmamem_unmap(sc->osdep.em_pa.pa_dmat, emm->emm_kva, emm->emm_size);
	bus_dmamem_free(sc->osdep.em_pa.pa_dmat, &emm->emm_seg, emm->emm_rseg);
}
