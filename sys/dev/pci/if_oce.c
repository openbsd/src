/*	$OpenBSD: if_oce.c,v 1.23 2012/10/22 02:49:03 brad Exp $	*/

/*
 * Copyright (c) 2012 Mike Belopuhov
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

/*-
 * Copyright (C) 2012 Emulex
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the Emulex Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * Contact Information:
 * freebsd-drivers@emulex.com
 *
 * Emulex
 * 3333 Susan Street
 * Costa Mesa, CA 92626
 */

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
#include <sys/timeout.h>
#include <sys/pool.h>

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
#endif

#if NVLAN > 0
#include <net/if_types.h>
#include <net/if_vlan_var.h>
#endif

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/ocereg.h>
#include <dev/pci/ocevar.h>

int  oce_probe(struct device *parent, void *match, void *aux);
void oce_attach(struct device *parent, struct device *self, void *aux);
void oce_attachhook(void *arg);
int  oce_attach_ifp(struct oce_softc *sc);
int  oce_ioctl(struct ifnet *ifp, u_long command, caddr_t data);
void oce_init(void *xsc);
void oce_stop(struct oce_softc *sc);
void oce_iff(struct oce_softc *sc);

int  oce_pci_alloc(struct oce_softc *sc);
int  oce_intr(void *arg);
int  oce_alloc_intr(struct oce_softc *sc);
void oce_intr_enable(struct oce_softc *sc);
void oce_intr_disable(struct oce_softc *sc);

void oce_media_status(struct ifnet *ifp, struct ifmediareq *ifmr);
int  oce_media_change(struct ifnet *ifp);
void oce_update_link_status(struct oce_softc *sc);
void oce_link_event(struct oce_softc *sc,
    struct oce_async_cqe_link_state *acqe);

int  oce_get_buf(struct oce_rq *rq);
int  oce_alloc_rx_bufs(struct oce_rq *rq);
void oce_refill_rx(void *arg);

void oce_watchdog(struct ifnet *ifp);
void oce_start(struct ifnet *ifp);
int  oce_encap(struct oce_softc *sc, struct mbuf **mpp, int wq_index);
void oce_txeof(struct oce_wq *wq);

void oce_discard_rx_comp(struct oce_rq *rq, struct oce_nic_rx_cqe *cqe);
int  oce_cqe_vtp_valid(struct oce_softc *sc, struct oce_nic_rx_cqe *cqe);
int  oce_cqe_portid_valid(struct oce_softc *sc, struct oce_nic_rx_cqe *cqe);
void oce_rxeof(struct oce_rq *rq, struct oce_nic_rx_cqe *cqe);
int  oce_start_rq(struct oce_rq *rq);
void oce_stop_rq(struct oce_rq *rq);
void oce_free_posted_rxbuf(struct oce_rq *rq);

int  oce_vid_config(struct oce_softc *sc);
void oce_set_macaddr(struct oce_softc *sc);
void oce_local_timer(void *arg);

#if defined(INET6) || defined(INET)
#ifdef OCE_LRO
int  oce_init_lro(struct oce_softc *sc);
void oce_free_lro(struct oce_softc *sc);
void oce_rx_flush_lro(struct oce_rq *rq);
#endif
#ifdef OCE_TSO
struct mbuf * oce_tso_setup(struct oce_softc *sc, struct mbuf **mpp);
#endif
#endif

void oce_intr_mq(void *arg);
void oce_intr_wq(void *arg);
void oce_intr_rq(void *arg);

int  oce_init_queues(struct oce_softc *sc);
void oce_release_queues(struct oce_softc *sc);

struct oce_wq *oce_create_wq(struct oce_softc *sc, struct oce_eq *eq,
    uint32_t q_len);
void oce_drain_wq(struct oce_wq *wq);
void oce_destroy_wq(struct oce_wq *wq);

struct oce_rq *oce_create_rq(struct oce_softc *sc, struct oce_eq *eq,
    uint32_t if_id, uint32_t q_len, uint32_t frag_size, uint32_t rss);
void oce_drain_rq(struct oce_rq *rq);
void oce_destroy_rq(struct oce_rq *rq);

struct oce_eq *oce_create_eq(struct oce_softc *sc);
void oce_arm_eq(struct oce_eq *eq, int npopped, int rearm, int clearint);
void oce_drain_eq(struct oce_eq *eq);
void oce_destroy_eq(struct oce_eq *eq);

struct oce_mq *oce_create_mq(struct oce_softc *sc, struct oce_eq *eq);
void oce_drain_mq(struct oce_mq *mq);
void oce_destroy_mq(struct oce_mq *mq);

struct oce_cq *oce_create_cq(struct oce_softc *sc, struct oce_eq *eq,
    uint32_t q_len, uint32_t item_size, uint32_t is_eventable,
    uint32_t nodelay, uint32_t ncoalesce);
void oce_arm_cq(struct oce_cq *cq, int npopped, int rearm);
void oce_destroy_cq(struct oce_cq *cq);

struct cfdriver oce_cd = {
	NULL, "oce", DV_IFNET
};

struct cfattach oce_ca = {
	sizeof(struct oce_softc), oce_probe, oce_attach, NULL, NULL
};

const struct pci_matchid oce_devices[] = {
	{ PCI_VENDOR_SERVERENGINES, PCI_PRODUCT_SERVERENGINES_BE2 },
	{ PCI_VENDOR_SERVERENGINES, PCI_PRODUCT_SERVERENGINES_BE3 },
	{ PCI_VENDOR_SERVERENGINES, PCI_PRODUCT_SERVERENGINES_OCBE2 },
	{ PCI_VENDOR_SERVERENGINES, PCI_PRODUCT_SERVERENGINES_OCBE3 },
	{ PCI_VENDOR_EMULEX, PCI_PRODUCT_EMULEX_XE201 },
};

int
oce_probe(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid(aux, oce_devices, nitems(oce_devices)));
}

void
oce_attach(struct device *parent, struct device *self, void *aux)
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;
	struct oce_softc *sc = (struct oce_softc *)self;

	switch (PCI_PRODUCT(pa->pa_id)) {
	case PCI_PRODUCT_SERVERENGINES_BE2:
	case PCI_PRODUCT_SERVERENGINES_OCBE2:
		sc->flags |= OCE_FLAGS_BE2;
		break;
	case PCI_PRODUCT_SERVERENGINES_BE3:
	case PCI_PRODUCT_SERVERENGINES_OCBE3:
		sc->flags |= OCE_FLAGS_BE3;
		break;
	case PCI_PRODUCT_EMULEX_XE201:
		sc->flags |= OCE_FLAGS_XE201;
		break;
	}

	sc->pa = *pa;
	if (oce_pci_alloc(sc))
		return;

	sc->rss_enable 	 = 0;
	sc->tx_ring_size = OCE_TX_RING_SIZE;
	sc->rx_ring_size = OCE_RX_RING_SIZE;
	sc->rq_frag_size = OCE_RQ_BUF_SIZE;
	sc->flow_control = OCE_DEFAULT_FLOW_CONTROL;

	/* create the bootstrap mailbox */
	if (oce_dma_alloc(sc, sizeof(struct oce_bmbx), &sc->bsmbx)) {
		printf(": failed to allocate mailbox memory\n");
		return;
	}

	if (oce_init_fw(sc))
		goto fail_1;

	if (oce_mbox_init(sc)) {
		printf(": failed to initialize mailbox\n");
		goto fail_1;
	}

	if (oce_get_fw_config(sc)) {
		printf(": failed to fetch fw configuration\n");
		goto fail_1;
	}

	if (IS_BE(sc) && (sc->flags & OCE_FLAGS_BE3)) {
		if (oce_check_native_mode(sc))
			goto fail_1;
	} else
		sc->be3_native = 0;

	if (oce_read_macaddr(sc, sc->macaddr)) {
		printf(": failed to fetch MAC address\n");
		goto fail_1;
	}
	bcopy(sc->macaddr, sc->arpcom.ac_enaddr, ETH_ADDR_LEN);

	sc->nrqs = 1;
	sc->nwqs = 1;
	sc->intr_count = 1;

	if (oce_alloc_intr(sc))
		goto fail_1;

	if (oce_init_queues(sc))
		goto fail_1;

	if (oce_attach_ifp(sc))
		goto fail_2;

#ifdef OCE_LRO
	if (oce_init_lro(sc))
		goto fail_3;
#endif

	timeout_set(&sc->timer, oce_local_timer, sc);
	timeout_set(&sc->rxrefill, oce_refill_rx, sc);

	mountroothook_establish(oce_attachhook, sc);

	printf(", address %s\n", ether_sprintf(sc->arpcom.ac_enaddr));

	return;

#ifdef OCE_LRO
fail_4:
	oce_free_lro(sc);
fail_3:
#endif
	ether_ifdetach(&sc->arpcom.ac_if);
	if_detach(&sc->arpcom.ac_if);
fail_2:
	oce_release_queues(sc);
fail_1:
	oce_dma_free(sc, &sc->bsmbx);
}

void
oce_attachhook(void *arg)
{
	struct oce_softc *sc = arg;

	oce_get_link_status(sc);

	oce_arm_cq(sc->mq->cq, 0, TRUE);

	/*
	 * We need to get MCC async events. So enable intrs and arm
	 * first EQ, Other EQs will be armed after interface is UP
	 */
	oce_intr_enable(sc);
	oce_arm_eq(sc->eq[0], 0, TRUE, FALSE);

	/*
	 * Send first mcc cmd and after that we get gracious
	 * MCC notifications from FW
	 */
	oce_first_mcc_cmd(sc);
}

int
oce_attach_ifp(struct oce_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;

	ifmedia_init(&sc->media, IFM_IMASK, oce_media_change, oce_media_status);
	ifmedia_add(&sc->media, IFM_ETHER | IFM_AUTO, 0, NULL);
	ifmedia_set(&sc->media, IFM_ETHER | IFM_AUTO);

	strlcpy(ifp->if_xname, sc->dev.dv_xname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = oce_ioctl;
	ifp->if_start = oce_start;
	ifp->if_watchdog = oce_watchdog;
	ifp->if_hardmtu = OCE_MAX_MTU;
	ifp->if_softc = sc;
	IFQ_SET_MAXLEN(&ifp->if_snd, sc->tx_ring_size - 1);
	IFQ_SET_READY(&ifp->if_snd);

	/* oce splits jumbos into 2k chunks... */
	m_clsetwms(ifp, MCLBYTES, 8, sc->rx_ring_size);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

#if defined(INET6) || defined(INET)
#ifdef OCE_TSO
	ifp->if_capabilities |= IFCAP_TSO;
	ifp->if_capabilities |= IFCAP_VLAN_HWTSO;
#endif
#ifdef OCE_LRO
	ifp->if_capabilities |= IFCAP_LRO;
#endif
#endif

	if_attach(ifp);
	ether_ifattach(ifp);

	return 0;
}

int
oce_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct oce_softc *sc = ifp->if_softc;
	struct ifaddr *ifa = (struct ifaddr *)data;
	struct ifreq *ifr = (struct ifreq *)data;
	int s, error = 0;

	s = splnet();

	switch (command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			oce_init(sc);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->arpcom, ifa);
#endif
		break;

	case SIOCGIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->media, command);
		break;

	case SIOCSIFMTU:
		if (ifr->ifr_mtu > OCE_MAX_MTU)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
			oce_init(sc);
		}
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING)
				error = ENETRESET;
			else
				oce_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				oce_stop(sc);
		}
		break;

	default:
		error = ether_ioctl(ifp, &sc->arpcom, command, data);
		break;
	}

	if (error == ENETRESET) {
		if (ifp->if_flags & IFF_RUNNING)
			oce_iff(sc);
		error = 0;
	}

	splx(s);

	return error;
}

void
oce_iff(struct oce_softc *sc)
{
	uint8_t multi[OCE_MAX_MC_FILTER_SIZE][ETH_ADDR_LEN];
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct arpcom *ac = &sc->arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;
	int naddr = 0, promisc = 0;

	ifp->if_flags &= ~IFF_ALLMULTI;

	if (ifp->if_flags & IFF_PROMISC || ac->ac_multirangecnt > 0 ||
	    ac->ac_multicnt >= OCE_MAX_MC_FILTER_SIZE) {
		ifp->if_flags |= IFF_ALLMULTI;
		promisc = 1;
	} else {
		ETHER_FIRST_MULTI(step, &sc->arpcom, enm);
		while (enm != NULL) {
			bcopy(enm->enm_addrlo, multi[naddr++], ETH_ADDR_LEN);

			ETHER_NEXT_MULTI(step, enm);
		}

		oce_update_mcast(sc, multi, naddr);
	}

	oce_set_promisc(sc, promisc);
}

int
oce_intr(void *arg)
{
	struct oce_softc *sc = arg;
	struct oce_eq *eq = sc->eq[0];
	struct oce_eqe *eqe;
	struct oce_cq *cq = NULL;
	int i, claimed = 0, neqe = 0;

	oce_dma_sync(&eq->ring->dma, BUS_DMASYNC_POSTREAD);

	for (;;) {
		eqe = RING_GET_CONSUMER_ITEM_VA(eq->ring, struct oce_eqe);
		if (eqe->evnt == 0)
			break;
		eqe->evnt = 0;
		oce_dma_sync(&eq->ring->dma, BUS_DMASYNC_PREWRITE);
		RING_GET(eq->ring, 1);
		neqe++;
	}

	if (!neqe)
		goto eq_arm; /* Spurious */

	claimed = 1;

 	/* Clear EQ entries, but dont arm */
	oce_arm_eq(eq, neqe, FALSE, TRUE);

	/* Process TX, RX and MCC. But dont arm CQ */
	for (i = 0; i < eq->cq_valid; i++) {
		cq = eq->cq[i];
		(*cq->cq_intr)(cq->cb_arg);
	}

	/* Arm all cqs connected to this EQ */
	for (i = 0; i < eq->cq_valid; i++) {
		cq = eq->cq[i];
		oce_arm_cq(cq, 0, TRUE);
	}

eq_arm:
	oce_arm_eq(eq, 0, TRUE, FALSE);
	return (claimed);
}

int
oce_pci_alloc(struct oce_softc *sc)
{
	struct pci_attach_args *pa = &sc->pa;
	pci_sli_intf_t intf;
	pcireg_t memtype, reg;

	/* setup the device config region */
	if (IS_BE(sc) && (sc->flags & OCE_FLAGS_BE2))
		reg = OCE_BAR_CFG_BE2;
	else
		reg = OCE_BAR_CFG;

	memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, reg);
	if (pci_mapreg_map(pa, reg, memtype, 0, &sc->cfg_iot,
	    &sc->cfg_ioh, NULL, &sc->cfg_size,
	    IS_BE(sc) ? 0 : 32768)) {
		printf(": can't find cfg mem space\n");
		return (ENXIO);
	}

	/* Read the SLI_INTF register and determine whether we
	 * can use this port and its features
	 */
	intf.dw0 = pci_conf_read(pa->pa_pc, pa->pa_tag, OCE_INTF_REG_OFFSET);

	if (intf.bits.sli_valid != OCE_INTF_VALID_SIG) {
		printf(": invalid signature\n");
		goto fail_1;
	}

	if (intf.bits.sli_rev != OCE_INTF_SLI_REV4) {
		printf(": adapter doesnt support SLI revision %d\n",
		    intf.bits.sli_rev);
		goto fail_1;
	}

	if (intf.bits.sli_if_type == OCE_INTF_IF_TYPE_1)
		sc->flags |= OCE_FLAGS_MBOX_ENDIAN_RQD;

	if (intf.bits.sli_hint1 == OCE_INTF_FUNC_RESET_REQD)
		sc->flags |= OCE_FLAGS_FUNCRESET_RQD;

	if (intf.bits.sli_func_type == OCE_INTF_VIRT_FUNC)
		sc->flags |= OCE_FLAGS_VIRTUAL_PORT;

	/* Lancer has one BAR (CFG) but BE3 has three (CFG, CSR, DB) */
	if (IS_BE(sc)) {
		/* set up CSR region */
		reg = OCE_BAR_CSR;
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, reg);
		if (pci_mapreg_map(pa, reg, memtype, 0, &sc->csr_iot,
		    &sc->csr_ioh, NULL, &sc->csr_size, 0)) {
			printf(": can't find csr mem space\n");
			goto fail_1;
		}

		/* set up DB doorbell region */
		reg = OCE_BAR_DB;
		memtype = pci_mapreg_type(pa->pa_pc, pa->pa_tag, reg);
		if (pci_mapreg_map(pa, reg, memtype, 0, &sc->db_iot,
		    &sc->db_ioh, NULL, &sc->db_size, 0)) {
			printf(": can't find csr mem space\n");
			goto fail_2;
		}
	}

	return (0);

fail_2:
	bus_space_unmap(sc->csr_iot, sc->csr_ioh, sc->csr_size);
fail_1:
	bus_space_unmap(sc->cfg_iot, sc->cfg_ioh, sc->cfg_size);
	return (ENXIO);
}

int
oce_alloc_intr(struct oce_softc *sc)
{
	const char *intrstr = NULL;
	struct pci_attach_args *pa = &sc->pa;
	pci_intr_handle_t ih;

	/* We allocate a single interrupt resource */
	if (pci_intr_map_msi(pa, &ih) != 0 &&
	    pci_intr_map(pa, &ih) != 0) {
		printf(": couldn't map interrupt\n");
		return (ENXIO);
	}

	intrstr = pci_intr_string(pa->pa_pc, ih);
	if (pci_intr_establish(pa->pa_pc, ih, IPL_NET, oce_intr, sc,
	    sc->dev.dv_xname) == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return (ENXIO);
	}
	printf(": %s", intrstr);

	return (0);
}

void
oce_intr_enable(struct oce_softc *sc)
{
	uint32_t reg;

	reg = OCE_READ_REG32(sc, cfg, PCICFG_INTR_CTRL);
	reg |= HOSTINTR_MASK;
	OCE_WRITE_REG32(sc, cfg, PCICFG_INTR_CTRL, reg);
}

void
oce_intr_disable(struct oce_softc *sc)
{
	uint32_t reg;

	reg = OCE_READ_REG32(sc, cfg, PCICFG_INTR_CTRL);
	reg &= ~HOSTINTR_MASK;
	OCE_WRITE_REG32(sc, cfg, PCICFG_INTR_CTRL, reg);
}

void
oce_update_link_status(struct oce_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int speed = 0;

	if (sc->link_status) {
		if (sc->link_active == 0) {
			switch (sc->link_speed) {
			case 1: /* 10 Mbps */
				speed = 10;
				break;
			case 2: /* 100 Mbps */
				speed = 100;
				break;
			case 3: /* 1 Gbps */
				speed = 1000;
				break;
			case 4: /* 10 Gbps */
				speed = 10000;
				break;
			}
			sc->link_active = 1;
			ifp->if_baudrate = speed * 1000000ULL;
		}
		if (!LINK_STATE_IS_UP(ifp->if_link_state)) {
			ifp->if_link_state = LINK_STATE_FULL_DUPLEX;
			if_link_state_change(ifp);
		}
	} else {
		if (sc->link_active == 1) {
			ifp->if_baudrate = 0;
			sc->link_active = 0;
		}
		if (ifp->if_link_state != LINK_STATE_DOWN) {
			ifp->if_link_state = LINK_STATE_DOWN;
			if_link_state_change(ifp);
		}
	}
}

void
oce_media_status(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct oce_softc *sc = ifp->if_softc;

	ifmr->ifm_status = IFM_AVALID;
	ifmr->ifm_active = IFM_ETHER;

	if (oce_get_link_status(sc) == 0)
		oce_update_link_status(sc);

	if (!sc->link_status) {
		ifmr->ifm_active |= IFM_NONE;
		return;
	}

	ifmr->ifm_status |= IFM_ACTIVE;

	switch (sc->link_speed) {
	case 1: /* 10 Mbps */
		ifmr->ifm_active |= IFM_10_T | IFM_FDX;
		break;
	case 2: /* 100 Mbps */
		ifmr->ifm_active |= IFM_100_TX | IFM_FDX;
		break;
	case 3: /* 1 Gbps */
		ifmr->ifm_active |= IFM_1000_T | IFM_FDX;
		break;
	case 4: /* 10 Gbps */
		ifmr->ifm_active |= IFM_10G_SR | IFM_FDX;
		break;
	}

	if (sc->flow_control & OCE_FC_TX)
		ifmr->ifm_active |= IFM_FLOW | IFM_ETH_TXPAUSE;
	if (sc->flow_control & OCE_FC_RX)
		ifmr->ifm_active |= IFM_FLOW | IFM_ETH_RXPAUSE;
}

int
oce_media_change(struct ifnet *ifp)
{
	return 0;
}

int
oce_encap(struct oce_softc *sc, struct mbuf **mpp, int wq_index)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mbuf *m = *mpp;
	struct oce_wq *wq = sc->wq[wq_index];
	struct oce_packet_desc *pd;
	struct oce_nic_hdr_wqe *nichdr;
	struct oce_nic_frag_wqe *nicfrag;
	uint32_t reg;
	int i, nwqe, out, rc;

#ifdef OCE_TSO
	if (m->m_pkthdr.csum_flags & CSUM_TSO) {
		/* consolidate packet buffers for TSO/LSO segment offload */
#if defined(INET6) || defined(INET)
		m = oce_tso_setup(sc, mpp);
#else
		m = NULL;	/* Huh? */
#endif
		if (m == NULL)
			goto error;
	}
#endif

	out = wq->packets_out + 1;
	if (out == OCE_WQ_PACKET_ARRAY_SIZE)
		out = 0;
	if (out == wq->packets_in)
		goto error;

	pd = &wq->pckts[wq->packets_out];

	rc = bus_dmamap_load_mbuf(wq->tag, pd->map, m, BUS_DMA_NOWAIT);
	if (rc == EFBIG) {
		if (m_defrag(m, M_DONTWAIT) ||
		    bus_dmamap_load_mbuf(wq->tag, pd->map, m, BUS_DMA_NOWAIT))
			goto error;
		*mpp = m;
	} else if (rc != 0)
		goto error;

	pd->nsegs = pd->map->dm_nsegs;

	nwqe = pd->nsegs + 1;
	if (IS_BE(sc)) {
		/*Dummy required only for BE3.*/
		if (nwqe & 1)
			nwqe++;
	}
	if (nwqe >= RING_NUM_FREE(wq->ring)) {
		bus_dmamap_unload(wq->tag, pd->map);
		goto error;
	}

	bus_dmamap_sync(wq->tag, pd->map, 0, pd->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);
	pd->mbuf = m;
	wq->packets_out = out;

	nichdr = RING_GET_PRODUCER_ITEM_VA(wq->ring, struct oce_nic_hdr_wqe);
	nichdr->u0.dw[0] = 0;
	nichdr->u0.dw[1] = 0;
	nichdr->u0.dw[2] = 0;
	nichdr->u0.dw[3] = 0;

	nichdr->u0.s.complete = 1;
	nichdr->u0.s.event = 1;
	nichdr->u0.s.crc = 1;
	nichdr->u0.s.forward = 0;
	nichdr->u0.s.ipcs = (m->m_pkthdr.csum_flags & M_IPV4_CSUM_OUT) ? 1 : 0;
	nichdr->u0.s.udpcs = (m->m_pkthdr.csum_flags & M_UDP_CSUM_OUT) ? 1 : 0;
	nichdr->u0.s.tcpcs = (m->m_pkthdr.csum_flags & M_TCP_CSUM_OUT) ? 1 : 0;
	nichdr->u0.s.num_wqe = nwqe;
	nichdr->u0.s.total_length = m->m_pkthdr.len;

#if NVLAN > 0
	if (m->m_flags & M_VLANTAG) {
		nichdr->u0.s.vlan = 1; /* Vlan present */
		nichdr->u0.s.vlan_tag = m->m_pkthdr.ether_vtag;
	}
#endif

#ifdef OCE_TSO
	if (m->m_pkthdr.csum_flags & CSUM_TSO) {
		if (m->m_pkthdr.tso_segsz) {
			nichdr->u0.s.lso = 1;
			nichdr->u0.s.lso_mss  = m->m_pkthdr.tso_segsz;
		}
		if (!IS_BE(sc))
			nichdr->u0.s.ipcs = 1;
	}
#endif

	oce_dma_sync(&wq->ring->dma, BUS_DMASYNC_PREREAD |
	    BUS_DMASYNC_PREWRITE);

	RING_PUT(wq->ring, 1);
	wq->ring->num_used++;

	for (i = 0; i < pd->nsegs; i++) {
		nicfrag = RING_GET_PRODUCER_ITEM_VA(wq->ring,
			struct oce_nic_frag_wqe);
		nicfrag->u0.s.rsvd0 = 0;
		nicfrag->u0.s.frag_pa_hi = ADDR_HI(pd->map->dm_segs[i].ds_addr);
		nicfrag->u0.s.frag_pa_lo = ADDR_LO(pd->map->dm_segs[i].ds_addr);
		nicfrag->u0.s.frag_len = pd->map->dm_segs[i].ds_len;
		pd->wqe_idx = wq->ring->pidx;
		RING_PUT(wq->ring, 1);
		wq->ring->num_used++;
	}
	if (nwqe > (pd->nsegs + 1)) {
		nicfrag = RING_GET_PRODUCER_ITEM_VA(wq->ring,
			struct oce_nic_frag_wqe);
		nicfrag->u0.dw[0] = 0;
		nicfrag->u0.dw[1] = 0;
		nicfrag->u0.dw[2] = 0;
		nicfrag->u0.dw[3] = 0;
		pd->wqe_idx = wq->ring->pidx;
		RING_PUT(wq->ring, 1);
		wq->ring->num_used++;
		pd->nsegs++;
	}

	ifp->if_opackets++;

	oce_dma_sync(&wq->ring->dma, BUS_DMASYNC_POSTREAD |
	    BUS_DMASYNC_POSTWRITE);

	reg = (nwqe << 16) | wq->id;
	OCE_WRITE_REG32(sc, db, PD_TXULP_DB, reg);

	return (0);

error:
	m_freem(*mpp);
	*mpp = NULL;
	return (1);
}

void
oce_txeof(struct oce_wq *wq)
{
	struct oce_softc *sc = (struct oce_softc *) wq->sc;
	struct oce_packet_desc *pd;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mbuf *m;
	uint32_t in;

	if (wq->packets_out == wq->packets_in)
		printf("%s: WQ transmit descriptor missing\n");

	in = wq->packets_in + 1;
	if (in == OCE_WQ_PACKET_ARRAY_SIZE)
		in = 0;

	pd = &wq->pckts[wq->packets_in];
	wq->packets_in = in;
	wq->ring->num_used -= (pd->nsegs + 1);
	bus_dmamap_sync(wq->tag, pd->map, 0, pd->map->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(wq->tag, pd->map);

	m = pd->mbuf;
	m_freem(m);
	pd->mbuf = NULL;

	if (ifp->if_flags & IFF_OACTIVE) {
		if (wq->ring->num_used < (wq->ring->num_items / 2)) {
			ifp->if_flags &= ~IFF_OACTIVE;
			oce_start(ifp);
		}
	}
	if (wq->ring->num_used == 0)
		ifp->if_timer = 0;
}

#if OCE_TSO
#if defined(INET6) || defined(INET)
struct mbuf *
oce_tso_setup(struct oce_softc *sc, struct mbuf **mpp)
{
	struct mbuf *m;
#ifdef INET
	struct ip *ip;
#endif
#ifdef INET6
	struct ip6_hdr *ip6;
#endif
	struct ether_vlan_header *eh;
	struct tcphdr *th;
	uint16_t etype;
	int total_len = 0, ehdrlen = 0;

	m = *mpp;

	if (M_WRITABLE(m) == 0) {
		m = m_dup(*mpp, M_DONTWAIT);
		if (!m)
			return NULL;
		m_freem(*mpp);
		*mpp = m;
	}

	eh = mtod(m, struct ether_vlan_header *);
	if (eh->evl_encap_proto == htons(ETHERTYPE_VLAN)) {
		etype = ntohs(eh->evl_proto);
		ehdrlen = ETHER_HDR_LEN + ETHER_VLAN_ENCAP_LEN;
	} else {
		etype = ntohs(eh->evl_encap_proto);
		ehdrlen = ETHER_HDR_LEN;
	}

	switch (etype) {
#ifdef INET
	case ETHERTYPE_IP:
		ip = (struct ip *)(m->m_data + ehdrlen);
		if (ip->ip_p != IPPROTO_TCP)
			return NULL;
		th = (struct tcphdr *)((caddr_t)ip + (ip->ip_hl << 2));

		total_len = ehdrlen + (ip->ip_hl << 2) + (th->th_off << 2);
		break;
#endif
#ifdef INET6
	case ETHERTYPE_IPV6:
		ip6 = (struct ip6_hdr *)(m->m_data + ehdrlen);
		if (ip6->ip6_nxt != IPPROTO_TCP)
			return NULL;
		th = (struct tcphdr *)((caddr_t)ip6 + sizeof(struct ip6_hdr));

		total_len = ehdrlen + sizeof(struct ip6_hdr) + (th->th_off << 2);
		break;
#endif
	default:
		return NULL;
	}

	m = m_pullup(m, total_len);
	if (!m)
		return NULL;
	*mpp = m;
	return m;

}
#endif /* INET6 || INET */
#endif

void
oce_watchdog(struct ifnet *ifp)
{
	printf("%s: watchdog timeout -- resetting\n", ifp->if_xname);

	oce_init(ifp->if_softc);

	ifp->if_oerrors++;
}

void
oce_start(struct ifnet *ifp)
{
	struct oce_softc *sc = ifp->if_softc;
	struct mbuf *m;
	int pkts = 0;

	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

	for (;;) {
		IFQ_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (oce_encap(sc, &m, 0)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
		pkts++;
	}

	/* Set a timeout in case the chip goes out to lunch */
	if (pkts)
		ifp->if_timer = 5;
}

/* Handle the Completion Queue for transmit */
void
oce_intr_wq(void *arg)
{
	struct oce_wq *wq = (struct oce_wq *)arg;
	struct oce_cq *cq = wq->cq;
	struct oce_nic_tx_cqe *cqe;
	int ncqe = 0;

	oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_POSTREAD);
	cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_tx_cqe);
	while (cqe->u0.dw[3]) {
		DW_SWAP((uint32_t *) cqe, sizeof(oce_wq_cqe));

		wq->ring->cidx = cqe->u0.s.wqe_index + 1;
		if (wq->ring->cidx >= wq->ring->num_items)
			wq->ring->cidx -= wq->ring->num_items;

		oce_txeof(wq);

		cqe->u0.dw[3] = 0;
		RING_GET(cq->ring, 1);
		oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_PREWRITE);
		cqe =
		    RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_tx_cqe);
		ncqe++;
	}

	if (ncqe)
		oce_arm_cq(cq, ncqe, FALSE);
}

void
oce_rxeof(struct oce_rq *rq, struct oce_nic_rx_cqe *cqe)
{
	struct oce_softc *sc = (struct oce_softc *)rq->sc;
	struct oce_packet_desc *pd;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct mbuf *m = NULL, *tail = NULL;
	int i, len, frag_len;
	uint32_t out;
	uint16_t vtag;

	len = cqe->u0.s.pkt_size;
	if (!len) {
		/* partial DMA workaround for Lancer */
		oce_discard_rx_comp(rq, cqe);
		goto exit;
	}

	 /* Get vlan_tag value */
	if (IS_BE(sc))
		vtag = BSWAP_16(cqe->u0.s.vlan_tag);
	else
		vtag = cqe->u0.s.vlan_tag;

	for (i = 0; i < cqe->u0.s.num_fragments; i++) {
		if (rq->packets_out == rq->packets_in) {
			printf("%s: RQ transmit descriptor missing\n",
			    sc->dev.dv_xname);
		}
		out = rq->packets_out + 1;
		if (out == OCE_RQ_PACKET_ARRAY_SIZE)
			out = 0;
		pd = &rq->pckts[rq->packets_out];
		rq->packets_out = out;

		bus_dmamap_sync(rq->tag, pd->map, 0, pd->map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rq->tag, pd->map);
		rq->pending--;

		frag_len = (len > rq->cfg.frag_size) ? rq->cfg.frag_size : len;
		pd->mbuf->m_len = frag_len;

		if (tail != NULL) {
			/* additional fragments */
			pd->mbuf->m_flags &= ~M_PKTHDR;
			tail->m_next = pd->mbuf;
			tail = pd->mbuf;
		} else {
			/* first fragment, fill out much of the packet header */
			pd->mbuf->m_pkthdr.len = len;
			pd->mbuf->m_pkthdr.csum_flags = 0;
			if (IF_CSUM_ENABLED(ifp)) {
				if (cqe->u0.s.ip_cksum_pass) {
					if (!cqe->u0.s.ip_ver) { /* IPV4 */
						pd->mbuf->m_pkthdr.csum_flags =
						    M_IPV4_CSUM_IN_OK;
					}
				}
				if (cqe->u0.s.l4_cksum_pass) {
					pd->mbuf->m_pkthdr.csum_flags |=
					    M_TCP_CSUM_IN_OK | M_UDP_CSUM_IN_OK;
				}
			}
			m = tail = pd->mbuf;
		}
		pd->mbuf = NULL;
		len -= frag_len;
	}

	if (m) {
		if (!oce_cqe_portid_valid(sc, cqe)) {
			 m_freem(m);
			 goto exit;
		}

		/* Account for jumbo chains */
		m_cluncount(m, 1);

		m->m_pkthdr.rcvif = ifp;

#if NVLAN > 0
		/* This determies if vlan tag is valid */
		if (oce_cqe_vtp_valid(sc, cqe)) {
			if (sc->function_mode & FNM_FLEX10_MODE) {
				/* FLEX10. If QnQ is not set, neglect VLAN */
				if (cqe->u0.s.qnq) {
					m->m_pkthdr.ether_vtag = vtag;
					m->m_flags |= M_VLANTAG;
				}
			} else if (sc->pvid != (vtag & VLAN_VID_MASK))  {
				/*
				 * In UMC mode generally pvid will be striped by
				 * hw. But in some cases we have seen it comes
				 * with pvid. So if pvid == vlan, neglect vlan.
				 */
				m->m_pkthdr.ether_vtag = vtag;
				m->m_flags |= M_VLANTAG;
			}
		}
#endif

		ifp->if_ipackets++;

#ifdef OCE_LRO
#if defined(INET6) || defined(INET)
		/* Try to queue to LRO */
		if (IF_LRO_ENABLED(sc) &&
		    !(m->m_flags & M_VLANTAG) &&
		    (cqe->u0.s.ip_cksum_pass) &&
		    (cqe->u0.s.l4_cksum_pass) &&
		    (!cqe->u0.s.ip_ver)       &&
		    (rq->lro.lro_cnt != 0)) {

			if (tcp_lro_rx(&rq->lro, m, 0) == 0) {
				rq->lro_pkts_queued ++;
				goto exit;
			}
			/* If LRO posting fails then try to post to STACK */
		}
#endif
#endif /* OCE_LRO */

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap_ether(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

		ether_input_mbuf(ifp, m);
	}
exit:
	return;
}

void
oce_discard_rx_comp(struct oce_rq *rq, struct oce_nic_rx_cqe *cqe)
{
	uint32_t out, i = 0;
	struct oce_packet_desc *pd;
	struct oce_softc *sc = (struct oce_softc *) rq->sc;
	int num_frags = cqe->u0.s.num_fragments;

	if (IS_XE201(sc) && cqe->u0.s.error) {
		/* Lancer A0 workaround
		 * num_frags will be 1 more than actual in case of error
		 */
		if (num_frags)
			num_frags -= 1;
	}
	for (i = 0; i < num_frags; i++) {
		if (rq->packets_out == rq->packets_in) {
			printf("%s: RQ transmit descriptor missing\n",
			    sc->dev.dv_xname);
		}
		out = rq->packets_out + 1;
		if (out == OCE_RQ_PACKET_ARRAY_SIZE)
			out = 0;
		pd = &rq->pckts[rq->packets_out];
		rq->packets_out = out;

		bus_dmamap_sync(rq->tag, pd->map, 0, pd->map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rq->tag, pd->map);
		rq->pending--;
		m_freem(pd->mbuf);
	}
}

int
oce_cqe_vtp_valid(struct oce_softc *sc, struct oce_nic_rx_cqe *cqe)
{
	struct oce_nic_rx_cqe_v1 *cqe_v1;
	int vtp = 0;

	if (sc->be3_native) {
		cqe_v1 = (struct oce_nic_rx_cqe_v1 *)cqe;
		vtp = cqe_v1->u0.s.vlan_tag_present;
	} else
		vtp = cqe->u0.s.vlan_tag_present;

	return vtp;

}

int
oce_cqe_portid_valid(struct oce_softc *sc, struct oce_nic_rx_cqe *cqe)
{
	struct oce_nic_rx_cqe_v1 *cqe_v1;

	if (sc->be3_native && IS_BE(sc)) {
		cqe_v1 = (struct oce_nic_rx_cqe_v1 *)cqe;
		if (sc->port_id != cqe_v1->u0.s.port)
			return 0;
	} else
		;/* For BE3 legacy and Lancer this is dummy */

	return 1;

}

#ifdef OCE_LRO
#if defined(INET6) || defined(INET)
void
oce_rx_flush_lro(struct oce_rq *rq)
{
	struct lro_ctrl	*lro = &rq->lro;
	struct lro_entry *queued;
	struct oce_softc *sc = (struct oce_softc *) rq->sc;

	if (!IF_LRO_ENABLED(sc))
		return;

	while ((queued = SLIST_FIRST(&lro->lro_active)) != NULL) {
		SLIST_REMOVE_HEAD(&lro->lro_active, next);
		tcp_lro_flush(lro, queued);
	}
	rq->lro_pkts_queued = 0;

	return;
}

int
oce_init_lro(struct oce_softc *sc)
{
	struct lro_ctrl *lro = NULL;
	int i = 0, rc = 0;

	for (i = 0; i < sc->nrqs; i++) {
		lro = &sc->rq[i]->lro;
		rc = tcp_lro_init(lro);
		if (rc != 0) {
			printf("%s: LRO init failed\n");
			return rc;
		}
		lro->ifp = &sc->arpcom.ac_if;
	}

	return rc;
}

void
oce_free_lro(struct oce_softc *sc)
{
	struct lro_ctrl *lro = NULL;
	int i = 0;

	for (i = 0; i < sc->nrqs; i++) {
		lro = &sc->rq[i]->lro;
		if (lro)
			tcp_lro_free(lro);
	}
}
#endif /* INET6 || INET */
#endif /* OCE_LRO */

int
oce_get_buf(struct oce_rq *rq)
{
	struct oce_softc *sc = (struct oce_softc *)rq->sc;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct oce_packet_desc *pd;
	struct oce_nic_rqe *rqe;
	int in = rq->packets_in + 1;

	if (in == OCE_RQ_PACKET_ARRAY_SIZE)
		in = 0;
	if (in == rq->packets_out)
		return 0;	/* no more room */

	pd = &rq->pckts[rq->packets_in];

	pd->mbuf = MCLGETI(NULL, M_DONTWAIT, ifp, MCLBYTES);
	if (pd->mbuf == NULL)
		return 0;

	pd->mbuf->m_len = pd->mbuf->m_pkthdr.len = MCLBYTES;

	if (bus_dmamap_load_mbuf(rq->tag, pd->map, pd->mbuf, BUS_DMA_NOWAIT)) {
		m_freem(pd->mbuf);
		pd->mbuf = NULL;
		return 0;
	}

	rq->packets_in = in;

	bus_dmamap_sync(rq->tag, pd->map, 0, pd->map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	oce_dma_sync(&rq->ring->dma, BUS_DMASYNC_PREREAD |
	    BUS_DMASYNC_PREWRITE);

	rqe = RING_GET_PRODUCER_ITEM_VA(rq->ring, struct oce_nic_rqe);
	rqe->u0.s.frag_pa_hi = ADDR_HI(pd->map->dm_segs[0].ds_addr);
	rqe->u0.s.frag_pa_lo = ADDR_LO(pd->map->dm_segs[0].ds_addr);
	DW_SWAP(u32ptr(rqe), sizeof(struct oce_nic_rqe));
	RING_PUT(rq->ring, 1);
	rq->pending++;

	oce_dma_sync(&rq->ring->dma, BUS_DMASYNC_POSTREAD |
	    BUS_DMASYNC_POSTWRITE);

	return 1;
}

int
oce_alloc_rx_bufs(struct oce_rq *rq)
{
	struct oce_softc *sc = (struct oce_softc *)rq->sc;
	pd_rxulp_db_t rxdb_reg;
	int i, nbufs = 0;

	while (oce_get_buf(rq))
		nbufs++;
	if (!nbufs)
		return 0;
	for (i = nbufs / OCE_MAX_RQ_POSTS; i > 0; i--) {
		DELAY(1);
		bzero(&rxdb_reg, sizeof(rxdb_reg));
		rxdb_reg.bits.num_posted = OCE_MAX_RQ_POSTS;
		rxdb_reg.bits.qid = rq->id;
		OCE_WRITE_REG32(sc, db, PD_RXULP_DB, rxdb_reg.dw0);
		nbufs -= OCE_MAX_RQ_POSTS;
	}
	if (nbufs > 0) {
		DELAY(1);
		bzero(&rxdb_reg, sizeof(rxdb_reg));
		rxdb_reg.bits.qid = rq->id;
		rxdb_reg.bits.num_posted = nbufs;
		OCE_WRITE_REG32(sc, db, PD_RXULP_DB, rxdb_reg.dw0);
	}
	return 1;
}

void
oce_refill_rx(void *arg)
{
	struct oce_softc *sc = arg;
	struct oce_rq *rq;
	int i, s;

	s = splnet();
	for_all_rq_queues(sc, rq, i) {
		oce_alloc_rx_bufs(rq);
		if (!rq->pending)
			timeout_add(&sc->rxrefill, 1);
	}
	splx(s);
}

/* Handle the Completion Queue for receive */
void
oce_intr_rq(void *arg)
{
	struct oce_rq *rq = (struct oce_rq *)arg;
	struct oce_cq *cq = rq->cq;
	struct oce_softc *sc = rq->sc;
	struct oce_nic_rx_cqe *cqe;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	int ncqe = 0, rq_buffers_used = 0;

	oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_POSTREAD);
	cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_rx_cqe);
	while (cqe->u0.dw[2]) {
		DW_SWAP((uint32_t *)cqe, sizeof(oce_rq_cqe));

		RING_GET(rq->ring, 1);
		if (cqe->u0.s.error == 0) {
			oce_rxeof(rq, cqe);
		} else {
			ifp->if_ierrors++;
			if (IS_XE201(sc))
				/* Lancer A0 no buffer workaround */
				oce_discard_rx_comp(rq, cqe);
			else
				/* Post L3/L4 errors to stack.*/
				oce_rxeof(rq, cqe);
		}
		cqe->u0.dw[2] = 0;

#ifdef OCE_LRO
#if defined(INET6) || defined(INET)
		if (IF_LRO_ENABLED(sc) && rq->lro_pkts_queued >= 16)
			oce_rx_flush_lro(rq);
#endif
#endif

		RING_GET(cq->ring, 1);
		oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_PREWRITE);
		cqe =
		    RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_rx_cqe);
		ncqe++;
		if (ncqe >= (IS_XE201(sc) ? 8 : OCE_MAX_RSP_HANDLED))
			break;
	}

#ifdef OCE_LRO
#if defined(INET6) || defined(INET)
	if (IF_LRO_ENABLED(sc))
		oce_rx_flush_lro(rq);
#endif
#endif

	if (ncqe) {
		oce_arm_cq(cq, ncqe, FALSE);
		rq_buffers_used = OCE_RQ_PACKET_ARRAY_SIZE - rq->pending;
		if (rq_buffers_used > 1 && !oce_alloc_rx_bufs(rq))
			timeout_add(&sc->rxrefill, 1);
	}
}

void
oce_set_macaddr(struct oce_softc *sc)
{
	uint32_t old_pmac_id = sc->pmac_id;
	int status = 0;

	if (!bcmp(sc->macaddr, sc->arpcom.ac_enaddr, ETH_ADDR_LEN))
		return;

	status = oce_macaddr_add(sc, sc->arpcom.ac_enaddr, sc->if_id,
	    &sc->pmac_id);
	if (!status)
		status = oce_macaddr_del(sc, sc->if_id, old_pmac_id);
	else
		printf("%s: failed to set MAC address\n", sc->dev.dv_xname);
}

void
oce_local_timer(void *arg)
{
	struct oce_softc *sc = arg;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	u_int64_t rxe, txe;
	int s;

	s = splnet();

	if (!(oce_update_stats(sc, &rxe, &txe))) {
		ifp->if_ierrors += (rxe > sc->rx_errors) ?
		    rxe - sc->rx_errors : sc->rx_errors - rxe;
		sc->rx_errors = rxe;
		ifp->if_oerrors += (txe > sc->tx_errors) ?
		    txe - sc->tx_errors : sc->tx_errors - txe;
		sc->tx_errors = txe;
	}

	splx(s);

	timeout_add_sec(&sc->timer, 1);
}

void
oce_stop(struct oce_softc *sc)
{
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct oce_rq *rq;
	struct oce_wq *wq;
	struct oce_eq *eq;
	int i;

	timeout_del(&sc->timer);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	/* Stop intrs and finish any bottom halves pending */
	oce_intr_disable(sc);

	/* Invalidate any pending cq and eq entries */
	for_all_eq_queues(sc, eq, i)
		oce_drain_eq(eq);
	for_all_rq_queues(sc, rq, i) {
		oce_destroy_queue(sc, QTYPE_RQ, rq->id);
		oce_free_posted_rxbuf(rq);
		oce_drain_rq(rq);
	}
	for_all_wq_queues(sc, wq, i)
		oce_drain_wq(wq);
}

void
oce_init(void *arg)
{
	struct oce_softc *sc = arg;
	struct ifnet *ifp = &sc->arpcom.ac_if;
	struct oce_eq *eq;
	struct oce_rq *rq;
	struct oce_wq *wq;
	int i;

	oce_stop(sc);

	DELAY(10);

	oce_set_macaddr(sc);

	oce_iff(sc);

	for_all_rq_queues(sc, rq, i) {
		rq->cfg.mtu = ifp->if_mtu + ETHER_HDR_LEN + ETHER_CRC_LEN +
		    ETHER_VLAN_ENCAP_LEN;
		if (oce_new_rq(sc, rq)) {
			printf("%s: failed to create rq\n", sc->dev.dv_xname);
			goto error;
		}
		rq->pending	 = 0;
		rq->ring->cidx	 = 0;
		rq->ring->pidx	 = 0;
		rq->packets_in	 = 0;
		rq->packets_out	 = 0;

		if (!oce_alloc_rx_bufs(rq)) {
			printf("%s: failed to allocate rx buffers\n",
			    sc->dev.dv_xname);
			goto error;
		}
	}

#ifdef OCE_RSS
	/* RSS config */
	if (sc->rss_enable) {
		if (oce_config_nic_rss(sc, (uint8_t)sc->if_id, RSS_ENABLE)) {
			printf("%s: failed to configure RSS\n",
			    sc->dev.dv_xname);
			goto error;
		}
	}
#endif

	for_all_rq_queues(sc, rq, i)
		oce_arm_cq(rq->cq, 0, TRUE);

	for_all_wq_queues(sc, wq, i)
		oce_arm_cq(wq->cq, 0, TRUE);

	oce_arm_cq(sc->mq->cq, 0, TRUE);

	for_all_eq_queues(sc, eq, i)
		oce_arm_eq(eq, 0, TRUE, FALSE);

	if (oce_get_link_status(sc) == 0)
		oce_update_link_status(sc);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	timeout_add_sec(&sc->timer, 1);

	oce_intr_enable(sc);

	return;
error:
	oce_stop(sc);
}

void
oce_link_event(struct oce_softc *sc, struct oce_async_cqe_link_state *acqe)
{
	/* Update Link status */
	if ((acqe->u0.s.link_status & ~ASYNC_EVENT_LOGICAL) ==
	     ASYNC_EVENT_LINK_UP)
		sc->link_status = ASYNC_EVENT_LINK_UP;
	else
		sc->link_status = ASYNC_EVENT_LINK_DOWN;

	/* Update speed */
	sc->link_speed = acqe->u0.s.speed;
	sc->qos_link_speed = (uint32_t) acqe->u0.s.qos_link_speed * 10;

	oce_update_link_status(sc);
}

/* Handle the Completion Queue for the Mailbox/Async notifications */
void
oce_intr_mq(void *arg)
{
	struct oce_mq *mq = (struct oce_mq *)arg;
	struct oce_softc *sc = mq->sc;
	struct oce_cq *cq = mq->cq;
	struct oce_mq_cqe *cqe;
	struct oce_async_cqe_link_state *acqe;
	struct oce_async_event_grp5_pvid_state *gcqe;
	int evt_type, optype, ncqe = 0;

	oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_POSTREAD);
	cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_mq_cqe);
	while (cqe->u0.dw[3]) {
		DW_SWAP((uint32_t *) cqe, sizeof(oce_mq_cqe));
		if (cqe->u0.s.async_event) {
			evt_type = cqe->u0.s.event_type;
			optype = cqe->u0.s.async_type;
			if (evt_type  == ASYNC_EVENT_CODE_LINK_STATE) {
				/* Link status evt */
				acqe = (struct oce_async_cqe_link_state *)cqe;
				oce_link_event(sc, acqe);
			} else if ((evt_type == ASYNC_EVENT_GRP5) &&
				   (optype == ASYNC_EVENT_PVID_STATE)) {
				/* GRP5 PVID */
				gcqe =
				(struct oce_async_event_grp5_pvid_state *)cqe;
				if (gcqe->enabled)
					sc->pvid = gcqe->tag & VLAN_VID_MASK;
				else
					sc->pvid = 0;
			}
		}
		cqe->u0.dw[3] = 0;
		RING_GET(cq->ring, 1);
		oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_PREWRITE);
		cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_mq_cqe);
		ncqe++;
	}

	if (ncqe)
		oce_arm_cq(cq, ncqe, FALSE /* TRUE */);
}

int
oce_init_queues(struct oce_softc *sc)
{
	struct oce_wq *wq;
	struct oce_rq *rq;
	int i;

	/* Create network interface on card */
	if (oce_create_iface(sc, sc->macaddr))
		goto error;

	/* create all of the event queues */
	for (i = 0; i < sc->intr_count; i++) {
		sc->eq[i] = oce_create_eq(sc);
		if (!sc->eq[i])
			goto error;
	}

	/* alloc tx queues */
	for_all_wq_queues(sc, wq, i) {
		sc->wq[i] = oce_create_wq(sc, sc->eq[i], sc->tx_ring_size);
		if (!sc->wq[i])
			goto error;
	}

	/* alloc rx queues */
	for_all_rq_queues(sc, rq, i) {
		sc->rq[i] = oce_create_rq(sc, sc->eq[i == 0 ? 0 : i - 1],
		    sc->if_id, sc->rx_ring_size, sc->rq_frag_size,
		    (i == 0) ? 0 : sc->rss_enable);
		if (!sc->rq[i])
			goto error;
	}

	/* alloc mailbox queue */
	sc->mq = oce_create_mq(sc, sc->eq[0]);
	if (!sc->mq)
		goto error;

	return (0);
error:
	oce_release_queues(sc);
	return (1);
}

void
oce_release_queues(struct oce_softc *sc)
{
	int i = 0;
	struct oce_wq *wq;
	struct oce_rq *rq;
	struct oce_eq *eq;

	for_all_rq_queues(sc, rq, i) {
		if (rq)
			oce_destroy_rq(sc->rq[i]);
	}

	for_all_wq_queues(sc, wq, i) {
		if (wq)
			oce_destroy_wq(sc->wq[i]);
	}

	if (sc->mq)
		oce_destroy_mq(sc->mq);

	for_all_eq_queues(sc, eq, i) {
		if (eq)
			oce_destroy_eq(sc->eq[i]);
	}
}

/**
 * @brief 		Function to create a WQ for NIC Tx
 * @param sc 		software handle to the device
 * @param qlen		number of entries in the queue
 * @returns		the pointer to the WQ created or NULL on failure
 */
struct oce_wq *
oce_create_wq(struct oce_softc *sc, struct oce_eq *eq, uint32_t q_len)
{
	struct oce_wq *wq;
	struct oce_cq *cq;
	int i;

	if (q_len < 256 || q_len > 2048)
		return (NULL);

	wq = malloc(sizeof(struct oce_wq), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!wq)
		return (NULL);

	wq->ring = oce_create_ring(sc, q_len, NIC_WQE_SIZE, 8);
	if (!wq->ring) {
		free(wq, M_DEVBUF);
		return (NULL);
	}

	cq = oce_create_cq(sc, eq, CQ_LEN_512, sizeof(struct oce_nic_tx_cqe),
	    1, 0, 3);
	if (!cq) {
		oce_destroy_ring(sc, wq->ring);
		free(wq, M_DEVBUF);
		return (NULL);
	}

	wq->id = -1;
	wq->sc = sc;
	wq->tag = sc->pa.pa_dmat;

	wq->cq = cq;
	wq->cfg.q_len = q_len;
	wq->cfg.wq_type = NIC_WQ_TYPE_STANDARD;
	wq->cfg.eqd = OCE_DEFAULT_WQ_EQD;
	wq->cfg.nbufs = 2 * wq->cfg.q_len;

	for (i = 0; i < OCE_WQ_PACKET_ARRAY_SIZE; i++) {
		if (bus_dmamap_create(wq->tag, OCE_MAX_TX_SIZE,
		    OCE_MAX_TX_ELEMENTS, PAGE_SIZE, 0, BUS_DMA_NOWAIT,
		    &wq->pckts[i].map)) {
			oce_destroy_wq(wq);
			return (NULL);
		}
	}

	if (oce_new_wq(sc, wq)) {
		oce_destroy_wq(wq);
		return (NULL);
	}

	wq->ring->cidx = 0;
	wq->ring->pidx = 0;

	eq->cq[eq->cq_valid] = cq;
	eq->cq_valid++;
	cq->cb_arg = wq;
	cq->cq_intr = oce_intr_wq;

	return (wq);
}

void
oce_destroy_wq(struct oce_wq *wq)
{
	struct oce_softc *sc = wq->sc;
	int i;

	if (wq->id >= 0)
		oce_destroy_queue(sc, QTYPE_WQ, wq->id);
	if (wq->cq != NULL)
		oce_destroy_cq(wq->cq);
	if (wq->ring != NULL)
		oce_destroy_ring(sc, wq->ring);
	for (i = 0; i < OCE_WQ_PACKET_ARRAY_SIZE; i++) {
		if (wq->pckts[i].map != NULL) {
			bus_dmamap_unload(wq->tag, wq->pckts[i].map);
			bus_dmamap_destroy(wq->tag, wq->pckts[i].map);
		}
	}
	free(wq, M_DEVBUF);
}

/**
 * @brief 		function to allocate receive queue resources
 * @param sc		software handle to the device
 * @param eq		pointer to associated event queue
 * @param if_id		interface identifier index
 * @param q_len		length of receive queue
 * @param frag_size	size of an receive queue fragment
 * @param rss		is-rss-queue flag
 * @returns		the pointer to the RQ created or NULL on failure
 */
struct oce_rq *
oce_create_rq(struct oce_softc *sc, struct oce_eq *eq, uint32_t if_id,
    uint32_t q_len, uint32_t frag_size, uint32_t rss)
{
	struct oce_rq *rq;
	struct oce_cq *cq;
	int i;

	if (ilog2(frag_size) <= 0)
		return (NULL);

	/* Hardware doesn't support any other value */
	if (q_len != 1024)
		return (NULL);

	rq = malloc(sizeof(struct oce_rq), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!rq)
		return (NULL);

	rq->ring = oce_create_ring(sc, q_len, sizeof(struct oce_nic_rqe), 2);
	if (!rq->ring) {
		free(rq, M_DEVBUF);
		return (NULL);
	}

	cq = oce_create_cq(sc, eq, CQ_LEN_1024, sizeof(struct oce_nic_rx_cqe),
	    1, 0, 3);
	if (!cq) {
		oce_destroy_ring(sc, rq->ring);
		free(rq, M_DEVBUF);
		return (NULL);
	}

	rq->id = -1;
	rq->sc = sc;
	rq->tag = sc->pa.pa_dmat;

	rq->cfg.if_id = if_id;
	rq->cfg.q_len = q_len;
	rq->cfg.frag_size = frag_size;
	rq->cfg.is_rss_queue = rss;

	for (i = 0; i < OCE_RQ_PACKET_ARRAY_SIZE; i++) {
		if (bus_dmamap_create(rq->tag, frag_size, 1, frag_size, 0,
		    BUS_DMA_NOWAIT, &rq->pckts[i].map)) {
			oce_destroy_rq(rq);
			return (NULL);
		}
	}

	rq->cq = cq;
	eq->cq[eq->cq_valid] = cq;
	eq->cq_valid++;
	cq->cb_arg = rq;
	cq->cq_intr = oce_intr_rq;

	/* RX queue is created in oce_init */

	return (rq);
}

void
oce_destroy_rq(struct oce_rq *rq)
{
	struct oce_softc *sc = rq->sc;
	int i;

	if (rq->id >= 0)
		oce_destroy_queue(sc, QTYPE_RQ, rq->id);
	if (rq->cq != NULL)
		oce_destroy_cq(rq->cq);
	if (rq->ring != NULL)
		oce_destroy_ring(sc, rq->ring);
	for (i = 0; i < OCE_RQ_PACKET_ARRAY_SIZE; i++) {
		if (rq->pckts[i].map != NULL) {
			bus_dmamap_unload(rq->tag, rq->pckts[i].map);
			bus_dmamap_destroy(rq->tag, rq->pckts[i].map);
		}
		if (rq->pckts[i].mbuf)
			m_freem(rq->pckts[i].mbuf);
	}
	free(rq, M_DEVBUF);
}

struct oce_eq *
oce_create_eq(struct oce_softc *sc)
{
	struct oce_eq *eq;

	/* allocate an eq */
	eq = malloc(sizeof(struct oce_eq), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (eq == NULL)
		return (NULL);

	eq->ring = oce_create_ring(sc, EQ_LEN_1024, EQE_SIZE_4, 8);
	if (!eq->ring) {
		free(eq, M_DEVBUF);
		return (NULL);
	}

	eq->id = -1;
	eq->sc = sc;
	eq->cfg.q_len = EQ_LEN_1024;	/* length of event queue */
	eq->cfg.item_size = EQE_SIZE_4; /* size of a queue item */
	eq->cfg.cur_eqd = (uint8_t)80;	/* event queue delay */

	if (oce_new_eq(sc, eq)) {
		oce_destroy_ring(sc, eq->ring);
		free(eq, M_DEVBUF);
		return (NULL);
	}

	return (eq);
}

void
oce_destroy_eq(struct oce_eq *eq)
{
	struct oce_softc *sc = eq->sc;

	if (eq->id >= 0)
		oce_destroy_queue(sc, QTYPE_EQ, eq->id);
	if (eq->ring != NULL)
		oce_destroy_ring(sc, eq->ring);
	free(eq, M_DEVBUF);
}

struct oce_mq *
oce_create_mq(struct oce_softc *sc, struct oce_eq *eq)
{
	struct oce_mq *mq = NULL;
	struct oce_cq *cq;

	/* allocate the mq */
	mq = malloc(sizeof(struct oce_mq), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!mq)
		return (NULL);

	mq->ring = oce_create_ring(sc, 128, sizeof(struct oce_mbx), 8);
	if (!mq->ring) {
		free(mq, M_DEVBUF);
		return (NULL);
	}

	cq = oce_create_cq(sc, eq, CQ_LEN_256, sizeof(struct oce_mq_cqe),
	    1, 0, 0);
	if (!cq) {
		oce_destroy_ring(sc, mq->ring);
		free(mq, M_DEVBUF);
		return (NULL);
	}

	mq->id = -1;
	mq->sc = sc;
	mq->cq = cq;

	mq->cfg.q_len = 128;

	if (oce_new_mq(sc, mq)) {
		oce_destroy_cq(mq->cq);
		oce_destroy_ring(sc, mq->ring);
		free(mq, M_DEVBUF);
		return (NULL);
	}

	eq->cq[eq->cq_valid] = cq;
	eq->cq_valid++;
	mq->cq->eq = eq;
	mq->cq->cb_arg = mq;
	mq->cq->cq_intr = oce_intr_mq;

	return (mq);
}

void
oce_destroy_mq(struct oce_mq *mq)
{
	struct oce_softc *sc = mq->sc;

	if (mq->id >= 0)
		oce_destroy_queue(sc, QTYPE_MQ, mq->id);
	if (mq->ring != NULL)
		oce_destroy_ring(sc, mq->ring);
	if (mq->cq != NULL)
		oce_destroy_cq(mq->cq);
	free(mq, M_DEVBUF);
}

/**
 * @brief		Function to create a completion queue
 * @param sc		software handle to the device
 * @param eq		optional eq to be associated with to the cq
 * @param q_len		length of completion queue
 * @param item_size	size of completion queue items
 * @param is_eventable	event table
 * @param nodelay	no delay flag
 * @param ncoalesce	no coalescence flag
 * @returns 		pointer to the cq created, NULL on failure
 */
struct oce_cq *
oce_create_cq(struct oce_softc *sc, struct oce_eq *eq, uint32_t q_len,
    uint32_t item_size, uint32_t eventable, uint32_t nodelay,
    uint32_t ncoalesce)
{
	struct oce_cq *cq = NULL;

	cq = malloc(sizeof(struct oce_cq), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (!cq)
		return NULL;

	cq->ring = oce_create_ring(sc, q_len, item_size, 4);
	if (!cq->ring) {
		free(cq, M_DEVBUF);
		return NULL;
	}

	cq->sc = sc;
	cq->eq = eq;
	cq->cfg.q_len = q_len;
	cq->cfg.item_size = item_size;
	cq->cfg.nodelay = (uint8_t) nodelay;
	cq->cfg.ncoalesce = ncoalesce;
	cq->cfg.eventable = eventable;

	if (oce_new_cq(sc, cq)) {
		oce_destroy_ring(sc, cq->ring);
		free(cq, M_DEVBUF);
		return (NULL);
	}

	sc->cq[sc->ncqs++] = cq;

	return (cq);
}

void
oce_destroy_cq(struct oce_cq *cq)
{
	struct oce_softc *sc = cq->sc;

	if (cq->ring != NULL) {
		oce_destroy_queue(sc, QTYPE_CQ, cq->id);
		oce_destroy_ring(sc, cq->ring);
	}
	free(cq, M_DEVBUF);
}

/**
 * @brief		Function to arm an EQ so that it can generate events
 * @param eq		pointer to event queue structure
 * @param npopped	number of EQEs to arm
 * @param rearm		rearm bit enable/disable
 * @param clearint	bit to clear the interrupt condition because of which
 *			EQEs are generated
 */
void
oce_arm_eq(struct oce_eq *eq, int npopped, int rearm, int clearint)
{
	struct oce_softc *sc = eq->sc;
	eq_db_t eq_db = { 0 };

	eq_db.bits.rearm = rearm;
	eq_db.bits.event = 1;
	eq_db.bits.num_popped = npopped;
	eq_db.bits.clrint = clearint;
	eq_db.bits.qid = eq->id;
	OCE_WRITE_REG32(sc, db, PD_EQ_DB, eq_db.dw0);
}

/**
 * @brief		Function to arm a CQ with CQEs
 * @param cq		pointer to the completion queue structure
 * @param npopped	number of CQEs to arm
 * @param rearm		rearm bit enable/disable
 */
void
oce_arm_cq(struct oce_cq *cq, int npopped, int rearm)
{
	struct oce_softc *sc = cq->sc;
	cq_db_t cq_db = { 0 };

	cq_db.bits.rearm = rearm;
	cq_db.bits.num_popped = npopped;
	cq_db.bits.event = 0;
	cq_db.bits.qid = cq->id;
	OCE_WRITE_REG32(sc, db, PD_CQ_DB, cq_db.dw0);
}

/**
 * @brief		function to cleanup the eqs used during stop
 * @param eq		pointer to event queue structure
 * @returns		the number of EQs processed
 */
void
oce_drain_eq(struct oce_eq *eq)
{
	struct oce_eqe *eqe;
	int neqe = 0;

	oce_dma_sync(&eq->ring->dma, BUS_DMASYNC_POSTREAD);

	for (;;) {
		eqe = RING_GET_CONSUMER_ITEM_VA(eq->ring, struct oce_eqe);
		if (eqe->evnt == 0)
			break;
		eqe->evnt = 0;
		oce_dma_sync(&eq->ring->dma, BUS_DMASYNC_POSTWRITE);
		RING_GET(eq->ring, 1);
		neqe++;
	}

	oce_arm_eq(eq, neqe, FALSE, TRUE);
}

void
oce_drain_wq(struct oce_wq *wq)
{
	struct oce_cq *cq = wq->cq;
	struct oce_nic_tx_cqe *cqe;
	int ncqe = 0;

	oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_POSTREAD);

	for (;;) {
		cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_tx_cqe);
		if (cqe->u0.dw[3] == 0)
			break;
		cqe->u0.dw[3] = 0;
		oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_POSTWRITE);
		RING_GET(cq->ring, 1);
		ncqe++;
	}

	oce_arm_cq(cq, ncqe, FALSE);
}

void
oce_drain_mq(struct oce_mq *mq)
{
	/* TODO: additional code. */
}

void
oce_drain_rq(struct oce_rq *rq)
{
	struct oce_nic_rx_cqe *cqe;
	struct oce_cq *cq = rq->cq;
	int ncqe = 0;

	oce_dma_sync(&cq->ring->dma, BUS_DMASYNC_POSTREAD);

	cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring, struct oce_nic_rx_cqe);
	/* dequeue till you reach an invalid cqe */
	while (RQ_CQE_VALID(cqe)) {
		RQ_CQE_INVALIDATE(cqe);
		RING_GET(cq->ring, 1);
		cqe = RING_GET_CONSUMER_ITEM_VA(cq->ring,
		    struct oce_nic_rx_cqe);
		ncqe++;
	}

	oce_arm_cq(cq, ncqe, FALSE);
}

void
oce_free_posted_rxbuf(struct oce_rq *rq)
{
	struct oce_packet_desc *pd;

	while (rq->pending) {
		pd = &rq->pckts[rq->packets_out];
		bus_dmamap_sync(rq->tag, pd->map, 0, pd->map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(rq->tag, pd->map);
		if (pd->mbuf != NULL) {
			m_freem(pd->mbuf);
			pd->mbuf = NULL;
		}

		if ((rq->packets_out + 1) == OCE_RQ_PACKET_ARRAY_SIZE)
			rq->packets_out = 0;
		else
			rq->packets_out++;

                rq->pending--;
	}
}

int
oce_dma_alloc(struct oce_softc *sc, bus_size_t size, struct oce_dma_mem *dma)
{
	int rc;

	bzero(dma, sizeof(struct oce_dma_mem));

	dma->tag = sc->pa.pa_dmat;
	rc = bus_dmamap_create(dma->tag, size, 1, size, 0, BUS_DMA_NOWAIT,
	    &dma->map);
	if (rc != 0) {
		printf("%s: failed to allocate DMA handle", sc->dev.dv_xname);
		goto fail_0;
	}

	rc = bus_dmamem_alloc(dma->tag, size, PAGE_SIZE, 0, &dma->segs, 1,
	    &dma->nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (rc != 0) {
		printf("%s: failed to allocate DMA memory", sc->dev.dv_xname);
		goto fail_1;
	}

	rc = bus_dmamem_map(dma->tag, &dma->segs, dma->nsegs, size,
	    &dma->vaddr, BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: failed to map DMA memory", sc->dev.dv_xname);
		goto fail_2;
	}

	rc = bus_dmamap_load(dma->tag, dma->map, dma->vaddr, size, NULL,
	    BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: failed to load DMA memory", sc->dev.dv_xname);
		goto fail_3;
	}

	bus_dmamap_sync(dma->tag, dma->map, 0, dma->map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	dma->paddr = dma->map->dm_segs[0].ds_addr;
	dma->size = size;

	return 0;

fail_3:
	bus_dmamem_unmap(dma->tag, dma->vaddr, size);
fail_2:
	bus_dmamem_free(dma->tag, &dma->segs, dma->nsegs);
fail_1:
	bus_dmamap_destroy(dma->tag, dma->map);
fail_0:
	return rc;
}

void
oce_dma_free(struct oce_softc *sc, struct oce_dma_mem *dma)
{
	if (dma->tag == NULL)
		return;

	if (dma->map != NULL) {
		oce_dma_sync(dma, BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(dma->tag, dma->map);

		if (dma->vaddr != 0) {
			bus_dmamem_free(dma->tag, &dma->segs, dma->nsegs);
			dma->vaddr = 0;
		}

		bus_dmamap_destroy(dma->tag, dma->map);
		dma->map = NULL;
		dma->tag = NULL;
	}
}

struct oce_ring *
oce_create_ring(struct oce_softc *sc, int q_len, int item_size,
    int max_segs)
{
	bus_size_t size = q_len * item_size;
	struct oce_ring *ring;
	int rc;

	if (size > max_segs * PAGE_SIZE)
		return NULL;

	ring = malloc(sizeof(struct oce_ring), M_DEVBUF, M_NOWAIT | M_ZERO);
	if (ring == NULL)
		return NULL;

	ring->item_size = item_size;
	ring->num_items = q_len;

	ring->dma.tag = sc->pa.pa_dmat;
	rc = bus_dmamap_create(ring->dma.tag, size, max_segs, PAGE_SIZE, 0,
	    BUS_DMA_NOWAIT, &ring->dma.map);
	if (rc != 0) {
		printf("%s: failed to allocate DMA handle", sc->dev.dv_xname);
		goto fail_0;
	}

	rc = bus_dmamem_alloc(ring->dma.tag, size, 0, 0, &ring->dma.segs,
	    max_segs, &ring->dma.nsegs, BUS_DMA_NOWAIT | BUS_DMA_ZERO);
	if (rc != 0) {
		printf("%s: failed to allocate DMA memory", sc->dev.dv_xname);
		goto fail_1;
	}

	rc = bus_dmamem_map(ring->dma.tag, &ring->dma.segs, ring->dma.nsegs,
	    size, &ring->dma.vaddr, BUS_DMA_NOWAIT);
	if (rc != 0) {
		printf("%s: failed to map DMA memory", sc->dev.dv_xname);
		goto fail_2;
	}

	bus_dmamap_sync(ring->dma.tag, ring->dma.map, 0,
	    ring->dma.map->dm_mapsize, BUS_DMASYNC_PREREAD |
	    BUS_DMASYNC_PREWRITE);

	ring->dma.paddr = 0;
	ring->dma.size = size;

	return (ring);

fail_2:
	bus_dmamem_free(ring->dma.tag, &ring->dma.segs, ring->dma.nsegs);
fail_1:
	bus_dmamap_destroy(ring->dma.tag, ring->dma.map);
fail_0:
	free(ring, M_DEVBUF);
	return (NULL);
}

void
oce_destroy_ring(struct oce_softc *sc, struct oce_ring *ring)
{
	oce_dma_free(sc, &ring->dma);
	free(ring, M_DEVBUF);
}

int
oce_load_ring(struct oce_softc *sc, struct oce_ring *ring,
    struct phys_addr *pa_list, int max_segs)
{
	struct oce_dma_mem *dma = &ring->dma;
	bus_dma_segment_t *segs;
	int i, nsegs;

	if (bus_dmamap_load(dma->tag, dma->map, dma->vaddr,
	    ring->item_size * ring->num_items, NULL, BUS_DMA_NOWAIT)) {
		printf("%s: failed to load a ring map\n", sc->dev.dv_xname);
		return (0);
	}

	segs = dma->map->dm_segs;
	nsegs = dma->map->dm_nsegs;
	if (nsegs > max_segs) {
		printf("%s: too many segments", sc->dev.dv_xname);
		return (0);
	}

	bus_dmamap_sync(dma->tag, dma->map, 0, dma->map->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	for (i = 0; i < nsegs; i++) {
		pa_list[i].lo = ADDR_LO(segs[i].ds_addr);
		pa_list[i].hi = ADDR_HI(segs[i].ds_addr);
	}

	return (nsegs);
}
