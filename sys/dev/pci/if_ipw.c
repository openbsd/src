/*	$OpenBSD: if_ipw.c,v 1.41 2005/02/19 13:08:55 damien Exp $	*/

/*-
 * Copyright (c) 2004, 2005
 *      Damien Bergamini <damien.bergamini@free.fr>. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice unmodified, this list of conditions, and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
 */

/*-
 * Intel(R) PRO/Wireless 2100 MiniPCI driver
 * http://www.intel.com/network/connectivity/products/wireless/prowireless_mobile.htm
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/sockio.h>
#include <sys/sysctl.h>
#include <sys/mbuf.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/malloc.h>
#include <sys/conf.h>
#include <sys/device.h>

#include <machine/bus.h>
#include <machine/endian.h>
#include <machine/intr.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif
#include <net/if.h>
#include <net/if_arp.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_types.h>

#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/if_ipwreg.h>
#include <dev/pci/if_ipwvar.h>

static const struct ieee80211_rateset ipw_rateset_11b =
	{ 4, { 2, 4, 11, 22 } };

int ipw_match(struct device *, void *, void *);
void ipw_attach(struct device *, struct device *, void *);
int ipw_detach(struct device *, int);
void ipw_power(int, void *);
int ipw_dma_alloc(struct ipw_softc *);
void ipw_release(struct ipw_softc *);
int ipw_media_change(struct ifnet *);
void ipw_media_status(struct ifnet *, struct ifmediareq *);
int ipw_newstate(struct ieee80211com *, enum ieee80211_state, int);
u_int16_t ipw_read_prom_word(struct ipw_softc *, u_int8_t);
void ipw_scan_result(struct ipw_softc *);
void ipw_command_intr(struct ipw_softc *, struct ipw_soft_buf *);
void ipw_newstate_intr(struct ipw_softc *, struct ipw_soft_buf *);
void ipw_data_intr(struct ipw_softc *, struct ipw_status *,
    struct ipw_soft_bd *, struct ipw_soft_buf *);
void ipw_notification_intr(struct ipw_softc *, struct ipw_soft_buf *);
void ipw_rx_intr(struct ipw_softc *);
void ipw_release_sbd(struct ipw_softc *, struct ipw_soft_bd *);
void ipw_tx_intr(struct ipw_softc *);
int ipw_intr(void *);
int ipw_cmd(struct ipw_softc *, u_int32_t, void *, u_int32_t);
int ipw_tx_start(struct ifnet *, struct mbuf *, struct ieee80211_node *);
void ipw_start(struct ifnet *);
void ipw_watchdog(struct ifnet *);
int ipw_get_table1(struct ipw_softc *, u_int32_t *);
int ipw_get_radio(struct ipw_softc *, int *);
int ipw_ioctl(struct ifnet *, u_long, caddr_t);
u_int32_t ipw_read_table1(struct ipw_softc *, u_int32_t);
void ipw_write_table1(struct ipw_softc *, u_int32_t, u_int32_t);
int ipw_read_table2(struct ipw_softc *, u_int32_t, void *, u_int32_t *);
void ipw_stop_master(struct ipw_softc *);
int ipw_reset(struct ipw_softc *);
int ipw_load_ucode(struct ipw_softc *, u_char *, int);
int ipw_load_firmware(struct ipw_softc *, u_char *, int);
int ipw_read_firmware(struct ipw_softc *, struct ipw_firmware *);
int ipw_config(struct ipw_softc *);
int ipw_init(struct ifnet *);
void ipw_stop(struct ifnet *, int);
void ipw_read_mem_1(struct ipw_softc *, bus_size_t, u_int8_t *, bus_size_t);
void ipw_write_mem_1(struct ipw_softc *, bus_size_t, u_int8_t *, bus_size_t);

static __inline u_int8_t MEM_READ_1(struct ipw_softc *sc, u_int32_t addr)
{
	CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_1(sc, IPW_CSR_INDIRECT_DATA);
}

static __inline u_int32_t MEM_READ_4(struct ipw_softc *sc, u_int32_t addr)
{
	CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_4(sc, IPW_CSR_INDIRECT_DATA);
}

#ifdef IPW_DEBUG
#define DPRINTF(x)	if (ipw_debug > 0) printf x
#define DPRINTFN(n, x)	if (ipw_debug >= (n)) printf x
int ipw_debug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

struct cfattach ipw_ca = {
	sizeof (struct ipw_softc), ipw_match, ipw_attach, ipw_detach
};

int
ipw_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR (pa->pa_id) == PCI_VENDOR_INTEL &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_2100_3B)
		return 1;

	return 0;
}

/* Base Address Register */
#define IPW_PCI_BAR0	0x10

void
ipw_attach(struct device *parent, struct device *self, void *aux)
{
	struct ipw_softc *sc = (struct ipw_softc *)self;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_addr_t base;
	pci_intr_handle_t ih;
	pcireg_t data;
	u_int16_t val;
	int error, i;

	sc->sc_pct = pa->pa_pc;
	sc->sc_pcitag = pa->pa_tag,

	/* clear device specific PCI configuration register 0x41 */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	data &= ~0x0000ff00;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, data);

	/* enable bus-mastering */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG);
	data |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, PCI_COMMAND_STATUS_REG, data);

	/* map the register window */
	error = pci_mapreg_map(pa, IPW_PCI_BAR0, PCI_MAPREG_TYPE_MEM |
	    PCI_MAPREG_MEM_TYPE_32BIT, 0, &memt, &memh, &base, &sc->sc_sz, 0);
	if (error != 0) {
		printf(": could not map memory space\n");
		return;
	}

	sc->sc_st = memt;
	sc->sc_sh = memh;
	sc->sc_dmat = pa->pa_dmat;

	/* disable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, 0);

	if (pci_intr_map(pa, &ih) != 0) {
		printf(": could not map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(sc->sc_pct, ih);
	sc->sc_ih = pci_intr_establish(sc->sc_pct, ih, IPL_NET, ipw_intr, sc,
	    sc->sc_dev.dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": could not establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	if (ipw_reset(sc) != 0) {
		printf(": could not reset adapter\n");
		return;
	}

	if (ipw_dma_alloc(sc) != 0) {
		printf(": failed to allocate DMA resources\n");
		return;
	}

	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps = IEEE80211_C_IBSS | IEEE80211_C_MONITOR |
	    IEEE80211_C_PMGT | IEEE80211_C_TXPMGT | IEEE80211_C_WEP |
	    IEEE80211_C_SHPREAMBLE;

	/* read MAC address from EEPROM */
	val = ipw_read_prom_word(sc, IPW_EEPROM_MAC + 0);
	ic->ic_myaddr[0] = val >> 8;
	ic->ic_myaddr[1] = val & 0xff;
	val = ipw_read_prom_word(sc, IPW_EEPROM_MAC + 1);
	ic->ic_myaddr[2] = val >> 8;
	ic->ic_myaddr[3] = val & 0xff;
	val = ipw_read_prom_word(sc, IPW_EEPROM_MAC + 2);
	ic->ic_myaddr[4] = val >> 8;
	ic->ic_myaddr[5] = val & 0xff;

	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* set supported .11b rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ipw_rateset_11b;

	/* set supported .11b channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_B);
		ic->ic_channels[i].ic_flags = IEEE80211_CHAN_B;
	}

	/* default to authmode OPEN */
	sc->authmode = IEEE80211_AUTH_OPEN;

	/* IBSS channel undefined for now */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = ipw_init;
	ifp->if_ioctl = ipw_ioctl;
	ifp->if_start = ipw_start;
	ifp->if_watchdog = ipw_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	if_attach(ifp);
	ieee80211_ifattach(ifp);
	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = ipw_newstate;
	ieee80211_media_init(ifp, ipw_media_change, ipw_media_status);

	sc->powerhook = powerhook_establish(ipw_power, sc);

#if NBPFILTER > 0
	bpfattach(&sc->sc_drvbpf, ifp, DLT_IEEE802_11_RADIO,
	    sizeof (struct ieee80211_frame) + 64);

	sc->sc_rxtap_len = sizeof sc->sc_rxtapu;
	sc->sc_rxtap.wr_ihdr.it_len = htole16(sc->sc_rxtap_len);
	sc->sc_rxtap.wr_ihdr.it_present = htole32(IPW_RX_RADIOTAP_PRESENT);

	sc->sc_txtap_len = sizeof sc->sc_txtapu;
	sc->sc_txtap.wt_ihdr.it_len = htole16(sc->sc_txtap_len);
	sc->sc_txtap.wt_ihdr.it_present = htole32(IPW_TX_RADIOTAP_PRESENT);
#endif
}

int
ipw_detach(struct device* self, int flags)
{
	struct ipw_softc *sc = (struct ipw_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	ipw_stop(ifp, 1);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	ipw_release(sc);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);

	return 0;
}

void
ipw_power(int why, void *arg)
{
	struct ipw_softc *sc = arg;
	struct ifnet *ifp;
	pcireg_t data;

	if (why != PWR_RESUME)
		return;

	/* clear device specific PCI configuration register 0x41 */
	data = pci_conf_read(sc->sc_pct, sc->sc_pcitag, 0x40);
	data &= ~0x0000ff00;
	pci_conf_write(sc->sc_pct, sc->sc_pcitag, 0x40, data);

	ifp = &sc->sc_ic.ic_if;
	if (ifp->if_flags & IFF_UP) {
		ifp->if_init(ifp);
		if (ifp->if_flags & IFF_RUNNING)
			ifp->if_start(ifp);
	}
}

int
ipw_dma_alloc(struct ipw_softc *sc)
{
	struct ipw_soft_bd *sbd;
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;
	int i, nsegs, error;

	/*
	 * Allocate and map tx ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, IPW_TBD_SZ, 1, IPW_TBD_SZ, 0,
	    BUS_DMA_NOWAIT, &sc->tbd_map);
	if (error != 0) {
		printf("%s: could not create tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_TBD_SZ, PAGE_SIZE, 0,
	    &sc->tbd_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->tbd_seg, nsegs, IPW_TBD_SZ,
	    (caddr_t *)&sc->tbd_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map tx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->tbd_map, sc->tbd_list,
	    IPW_TBD_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/*
	 * Allocate and map rx ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, IPW_RBD_SZ, 1, IPW_RBD_SZ, 0,
	    BUS_DMA_NOWAIT, &sc->rbd_map);
	if (error != 0) {
		printf("%s: could not create rx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_RBD_SZ, PAGE_SIZE, 0,
	    &sc->rbd_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate rx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->rbd_seg, nsegs, IPW_RBD_SZ,
	    (caddr_t *)&sc->rbd_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map rx ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->rbd_map, sc->rbd_list,
	    IPW_RBD_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load tx ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/*
	 * Allocate and map status ring
	 */
	error = bus_dmamap_create(sc->sc_dmat, IPW_STATUS_SZ, 1, IPW_STATUS_SZ,
	    0, BUS_DMA_NOWAIT, &sc->status_map);
	if (error != 0) {
		printf("%s: could not create status ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_STATUS_SZ, PAGE_SIZE, 0,
	    &sc->status_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate status ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->status_seg, nsegs,
	    IPW_STATUS_SZ, (caddr_t *)&sc->status_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map status ring DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->status_map, sc->status_list,
	    IPW_STATUS_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load status ring DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/*
	 * Allocate command DMA map
	 */
	error = bus_dmamap_create(sc->sc_dmat, sizeof (struct ipw_cmd), 1,
	    sizeof (struct ipw_cmd), 0, BUS_DMA_NOWAIT, &sc->cmd_map);
	if (error != 0) {
		printf("%s: could not create command DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	/*
	 * Allocate headers DMA maps
	 */
	SLIST_INIT(&sc->free_shdr);
	for (i = 0; i < IPW_NDATA; i++) {
		shdr = &sc->shdr_list[i];
		error = bus_dmamap_create(sc->sc_dmat, sizeof (struct ipw_hdr),
		    1, sizeof (struct ipw_hdr), 0, BUS_DMA_NOWAIT, &shdr->map);
		if (error != 0) {
			printf("%s: could not create header DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
		SLIST_INSERT_HEAD(&sc->free_shdr, shdr, next);
	}

	/*
	 * Allocate tx buffers DMA maps
	 */
	SLIST_INIT(&sc->free_sbuf);
	for (i = 0; i < IPW_NDATA; i++) {
		sbuf = &sc->tx_sbuf_list[i];
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, IPW_MAX_NSEG,
		    MCLBYTES, 0, BUS_DMA_NOWAIT, &sbuf->map);
		if (error != 0) {
			printf("%s: could not create tx DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
		SLIST_INSERT_HEAD(&sc->free_sbuf, sbuf, next);
	}

	/*
	 * Initialize tx ring
	 */
	for (i = 0; i < IPW_NTBD; i++) {
		sbd = &sc->stbd_list[i];
		sbd->bd = &sc->tbd_list[i];
		sbd->type = IPW_SBD_TYPE_NOASSOC;
	}

	/*
	 * Pre-allocate rx buffers and DMA maps
	 */
	for (i = 0; i < IPW_NRBD; i++) {
		sbd = &sc->srbd_list[i];
		sbuf = &sc->rx_sbuf_list[i];
		sbd->bd = &sc->rbd_list[i];

		MGETHDR(sbuf->m, M_DONTWAIT, MT_DATA);
		if (sbuf->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		MCLGET(sbuf->m, M_DONTWAIT);
		if (!(sbuf->m->m_flags & M_EXT)) {
			m_freem(sbuf->m);
			printf("%s: could not allocate rx mbuf cluster\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &sbuf->map);
		if (error != 0) {
			printf("%s: could not create rx DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, sbuf->map,
		    mtod(sbuf->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not map rx DMA memory\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		sbd->type = IPW_SBD_TYPE_DATA;
		sbd->priv = sbuf;
		sbd->bd->physaddr = htole32(sbuf->map->dm_segs[0].ds_addr);
		sbd->bd->len = htole32(MCLBYTES);
	}

	bus_dmamap_sync(sc->sc_dmat, sc->rbd_map, 0, IPW_RBD_SZ,
	    BUS_DMASYNC_PREWRITE);

	return 0;

fail:	ipw_release(sc);
	return error;
}

void
ipw_release(struct ipw_softc *sc)
{
	struct ipw_soft_buf *sbuf;
	int i;

	if (sc->tbd_map != NULL) {
		if (sc->tbd_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->tbd_map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->tbd_list,
			    IPW_TBD_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->tbd_seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->tbd_map);
	}

	if (sc->rbd_map != NULL) {
		if (sc->rbd_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->rbd_map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->rbd_list,
			    IPW_RBD_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->rbd_seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->rbd_map);
	}

	if (sc->status_map != NULL) {
		if (sc->status_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->status_map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->status_list,
			    IPW_RBD_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->status_seg, 1);
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->status_map);
	}

	if (sc->cmd_map != NULL)
		bus_dmamap_destroy(sc->sc_dmat, sc->cmd_map);

	for (i = 0; i < IPW_NDATA; i++)
		bus_dmamap_destroy(sc->sc_dmat, sc->shdr_list[i].map);

	for (i = 0; i < IPW_NDATA; i++)
		bus_dmamap_destroy(sc->sc_dmat, sc->tx_sbuf_list[i].map);

	for (i = 0; i < IPW_NRBD; i++) {
		sbuf = &sc->rx_sbuf_list[i];
		if (sbuf->map != NULL) {
			if (sbuf->m != NULL) {
				bus_dmamap_unload(sc->sc_dmat, sbuf->map);
				m_freem(sbuf->m);
			}
			bus_dmamap_destroy(sc->sc_dmat, sbuf->map);
		}
	}
}

int
ipw_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		ipw_init(ifp);

	return 0;
}

void
ipw_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
#define N(a)	(sizeof (a) / sizeof (a[0]))
	static const struct {
		u_int32_t	val;
		int		rate;
	} rates[] = {
		{ IPW_RATE_DS1,   2 },
		{ IPW_RATE_DS2,   4 },
		{ IPW_RATE_DS5,  11 },
		{ IPW_RATE_DS11, 22 },
	};
	u_int32_t val;
	int rate, i;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;

	/* read current transmission rate from adapter */
	val = ipw_read_table1(sc, IPW_INFO_CURRENT_TX_RATE);
	val &= 0xf;

	/* convert rate to 802.11 rate */
	for (i = 0; i < N(rates) && rates[i].val != val; i++);
	rate = (i < N(rates)) ? rates[i].rate : 0;

	imr->ifm_active |= IFM_IEEE80211_11B;
	imr->ifm_active |= ieee80211_rate2media(ic, rate, IEEE80211_MODE_11B);
	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
		break;

	case IEEE80211_M_IBSS:
		imr->ifm_active |= IFM_IEEE80211_IBSS;
		break;

	case IEEE80211_M_MONITOR:
		imr->ifm_active |= IFM_IEEE80211_MONITOR;
		break;

	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_HOSTAP:
		/* should not get there */
		break;
	}
#undef N
}

int
ipw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ipw_softc *sc = ic->ic_softc;
	struct ieee80211_node *ni = ic->ic_bss;
	u_int32_t len;
	u_int8_t val;

	switch (nstate) {
	case IEEE80211_S_RUN:
		len = IEEE80211_NWID_LEN;
		ipw_read_table2(sc, IPW_INFO_CURRENT_SSID, ni->ni_essid, &len);
		ni->ni_esslen = len;

		val = ipw_read_table1(sc, IPW_INFO_CURRENT_CHANNEL);
		ni->ni_chan = &ic->ic_channels[val];

		DELAY(100); /* firmware needs a short delay here */

		len = IEEE80211_ADDR_LEN;
		ipw_read_table2(sc, IPW_INFO_CURRENT_BSSID, ni->ni_bssid, &len);
		break;

	case IEEE80211_S_INIT:
	case IEEE80211_S_SCAN:
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		break;
	}

	ic->ic_state = nstate;
	return 0;
}

/*
 * Read 16 bits at address 'addr' from the Microwire EEPROM.
 * DON'T PLAY WITH THIS CODE UNLESS YOU KNOW *EXACTLY* WHAT YOU'RE DOING!
 */
u_int16_t
ipw_read_prom_word(struct ipw_softc *sc, u_int8_t addr)
{
	u_int32_t tmp;
	u_int16_t val;
	int n;

	/* Clock C once before the first command */
	IPW_EEPROM_CTL(sc, 0);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_C);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);

	/* Write start bit (1) */
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_D);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_D | IPW_EEPROM_C);

	/* Write READ opcode (10) */
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_D);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_D | IPW_EEPROM_C);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_C);

	/* Write address A7-A0 */
	for (n = 7; n >= 0; n--) {
		IPW_EEPROM_CTL(sc, IPW_EEPROM_S |
		    (((addr >> n) & 1) << IPW_EEPROM_SHIFT_D));
		IPW_EEPROM_CTL(sc, IPW_EEPROM_S |
		    (((addr >> n) & 1) << IPW_EEPROM_SHIFT_D) | IPW_EEPROM_C);
	}

	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);

	/* Read data Q15-Q0 */
	val = 0;
	for (n = 15; n >= 0; n--) {
		IPW_EEPROM_CTL(sc, IPW_EEPROM_S | IPW_EEPROM_C);
		IPW_EEPROM_CTL(sc, IPW_EEPROM_S);
		tmp = MEM_READ_4(sc, IPW_MEM_EEPROM_CTL);
		val |= ((tmp & IPW_EEPROM_Q) >> IPW_EEPROM_SHIFT_Q) << n;
	}

	IPW_EEPROM_CTL(sc, 0);

	/* Clear Chip Select and clock C */
	IPW_EEPROM_CTL(sc, IPW_EEPROM_S);
	IPW_EEPROM_CTL(sc, 0);
	IPW_EEPROM_CTL(sc, IPW_EEPROM_C);

	return letoh16(val);
}

void
ipw_scan_result(struct ipw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	u_int32_t i, cnt, off;
	struct ipw_node ap;

	/* flush previously seen access points */
	ieee80211_free_allnodes(ic);

	cnt = ipw_read_table1(sc, IPW_INFO_APS_CNT);
	off = ipw_read_table1(sc, IPW_INFO_APS_BASE);

	DPRINTF(("Found %u APs\n", cnt));

	for (i = 0; i < cnt; i++) {
		ipw_read_mem_1(sc, off, (u_int8_t *)&ap, sizeof ap);
		off += sizeof ap;

#ifdef IPW_DEBUG
		if (ipw_debug >= 2) {
			u_char *p = (u_char *)&ap;
			int j;

			printf("AP%u\n", i);
			for (j = 0; j < sizeof ap; j++)
				printf("%02x", *p++);
			printf("\n");
		}
#endif

		ni = ieee80211_lookup_node(ic, ap.bssid,
		    &ic->ic_channels[ap.chan]);
		if (ni != NULL)
			continue;

		ni = ieee80211_alloc_node(ic, ap.bssid);
		if (ni == NULL)
			return;

		IEEE80211_ADDR_COPY(ni->ni_bssid, ap.bssid);
		ni->ni_rssi = ap.rssi;
		ni->ni_intval = letoh16(ap.intval);
		ni->ni_capinfo = letoh16(ap.capinfo);
		ni->ni_chan = &ic->ic_channels[ap.chan];
		ni->ni_esslen = ap.esslen;
		bcopy(ap.essid, ni->ni_essid, IEEE80211_NWID_LEN);
	}
}

void
ipw_command_intr(struct ipw_softc *sc, struct ipw_soft_buf *sbuf)
{
	struct ipw_cmd *cmd;

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, sizeof (struct ipw_cmd),
	    BUS_DMASYNC_POSTREAD);

	cmd = mtod(sbuf->m, struct ipw_cmd *);

	DPRINTFN(2, ("RX!CMD!%u!%u!%u!%u!%u\n",
	    letoh32(cmd->type), letoh32(cmd->subtype), letoh32(cmd->seq),
	    letoh32(cmd->len), letoh32(cmd->status)));

	wakeup(sc);
}

void
ipw_newstate_intr(struct ipw_softc *sc, struct ipw_soft_buf *sbuf)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int32_t state;

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, sizeof state,
	    BUS_DMASYNC_POSTREAD);

	state = letoh32(*mtod(sbuf->m, u_int32_t *));

	DPRINTFN(2, ("RX!NEWSTATE!%u\n", state));

	switch (state) {
	case IPW_STATE_ASSOCIATED:
		ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
		break;

	case IPW_STATE_SCANNING:
		/* don't leave run state on background scan */
		if (ic->ic_state != IEEE80211_S_RUN)
			ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		break;

	case IPW_STATE_SCAN_COMPLETE:
		ipw_scan_result(sc);
		break;

	case IPW_STATE_ASSOCIATION_LOST:
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		break;

	case IPW_STATE_RADIO_DISABLED:
		ipw_stop(&ic->ic_if, 1);
		break;
	}
}

void
ipw_data_intr(struct ipw_softc *sc, struct ipw_status *status,
    struct ipw_soft_bd *sbd, struct ipw_soft_buf *sbuf)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct mbuf *m;
	struct ieee80211_frame *wh;
	struct ieee80211_node *ni;
	int error;

	DPRINTFN(5, ("RX!DATA!%u!%u\n", letoh32(status->len), status->rssi));

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, letoh32(status->len),
	    BUS_DMASYNC_POSTREAD);

	bus_dmamap_unload(sc->sc_dmat, sbuf->map);

	/* Finalize mbuf */
	m = sbuf->m;
	m->m_pkthdr.rcvif = ifp;
	m->m_pkthdr.len = m->m_len = letoh32(status->len);

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct ipw_rx_radiotap_header *tap = &sc->sc_rxtap;

		tap->wr_flags = 0;
		tap->wr_antsignal = status->rssi;
		tap->wr_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wr_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);

		M_DUP_PKTHDR(&mb, m);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_rxtap_len;
		mb.m_next = m;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb);
	}
#endif

	wh = mtod(m, struct ieee80211_frame *);

	ni = ieee80211_find_rxnode(ic, wh);

	/* Send the frame to the upper layer */
	ieee80211_input(ifp, m, ni, status->rssi, 0);

	ieee80211_release_node(ic, ni);

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL) {
		printf("%s: could not allocate rx mbuf\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	MCLGET(m, M_DONTWAIT);
	if (!(m->m_flags & M_EXT)) {
		m_freem(m);
		printf("%s: could not allocate rx mbuf cluster\n",
		    sc->sc_dev.dv_xname);
		return;
	}

	error = bus_dmamap_load(sc->sc_dmat, sbuf->map, mtod(m, void *),
	    MCLBYTES, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map rx DMA memory\n",
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return;
	}

	sbuf->m = m;
	sbd->bd->physaddr = htole32(sbuf->map->dm_segs[0].ds_addr);
}

void
ipw_notification_intr(struct ipw_softc *sc, struct ipw_soft_buf *sbuf)
{
	DPRINTFN(2, ("RX!NOTIFICATION\n"));
}

void
ipw_rx_intr(struct ipw_softc *sc)
{
	struct ipw_status *status;
	struct ipw_soft_bd *sbd;
	struct ipw_soft_buf *sbuf;
	u_int32_t r, i;

	r = CSR_READ_4(sc, IPW_CSR_RX_READ_INDEX);

	for (i = (sc->rxcur + 1) % IPW_NRBD; i != r; i = (i + 1) % IPW_NRBD) {

		bus_dmamap_sync(sc->sc_dmat, sc->rbd_map,
		    i * sizeof (struct ipw_bd), sizeof (struct ipw_bd),
		    BUS_DMASYNC_POSTREAD);

		bus_dmamap_sync(sc->sc_dmat, sc->status_map,
		    i * sizeof (struct ipw_status), sizeof (struct ipw_status),
		    BUS_DMASYNC_POSTREAD);

		status = &sc->status_list[i];
		sbd = &sc->srbd_list[i];
		sbuf = sbd->priv;

		switch (letoh16(status->code) & 0xf) {
		case IPW_STATUS_CODE_COMMAND:
			ipw_command_intr(sc, sbuf);
			break;

		case IPW_STATUS_CODE_NEWSTATE:
			ipw_newstate_intr(sc, sbuf);
			break;

		case IPW_STATUS_CODE_DATA_802_3:
		case IPW_STATUS_CODE_DATA_802_11:
			ipw_data_intr(sc, status, sbd, sbuf);
			break;

		case IPW_STATUS_CODE_NOTIFICATION:
			ipw_notification_intr(sc, sbuf);
			break;

		default:
			printf("%s: unknown status code %u\n",
			    sc->sc_dev.dv_xname, letoh16(status->code));
		}
		sbd->bd->flags = 0;

		bus_dmamap_sync(sc->sc_dmat, sc->rbd_map,
		    i * sizeof (struct ipw_bd), sizeof (struct ipw_bd),
		    BUS_DMASYNC_PREWRITE);
	}

	/* Tell the firmware what we have processed */
	sc->rxcur = (r == 0) ? IPW_NRBD - 1 : r - 1;
	CSR_WRITE_4(sc, IPW_CSR_RX_WRITE_INDEX, sc->rxcur);
}

void
ipw_release_sbd(struct ipw_softc *sc, struct ipw_soft_bd *sbd)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;

	switch (sbd->type) {
	case IPW_SBD_TYPE_COMMAND:
		bus_dmamap_unload(sc->sc_dmat, sc->cmd_map);
		break;

	case IPW_SBD_TYPE_HEADER:
		shdr = sbd->priv;
		bus_dmamap_unload(sc->sc_dmat, shdr->map);
		SLIST_INSERT_HEAD(&sc->free_shdr, shdr, next);
		break;

	case IPW_SBD_TYPE_DATA:
		sbuf = sbd->priv;
		bus_dmamap_unload(sc->sc_dmat, sbuf->map);
		SLIST_INSERT_HEAD(&sc->free_sbuf, sbuf, next);

		m_freem(sbuf->m);

		if (sbuf->ni != NULL)
			ieee80211_release_node(ic, sbuf->ni);

		/* kill watchdog timer */
		sc->sc_tx_timer = 0;
		break;
	}
	sbd->type = IPW_SBD_TYPE_NOASSOC;
}

void
ipw_tx_intr(struct ipw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	u_int32_t r, i;

	r = CSR_READ_4(sc, IPW_CSR_TX_READ_INDEX);

	for (i = (sc->txold + 1) % IPW_NTBD; i != r; i = (i + 1) % IPW_NTBD) {
		ipw_release_sbd(sc, &sc->stbd_list[i]);
		sc->txfree++;
	}

	/* Remember what the firmware has processed */
	sc->txold = (r == 0) ? IPW_NTBD - 1 : r - 1;

	/* Call start() since some buffer descriptors have been released */
	ifp->if_flags &= ~IFF_OACTIVE;
	(*ifp->if_start)(ifp);
}

int
ipw_intr(void *arg)
{
	struct ipw_softc *sc = arg;
	u_int32_t r;

	if ((r = CSR_READ_4(sc, IPW_CSR_INTR)) == 0 || r == 0xffffffff)
		return 0;

	/* Disable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, 0);

	DPRINTFN(8, ("INTR!0x%08x\n", r));

	if (r & (IPW_INTR_FATAL_ERROR | IPW_INTR_PARITY_ERROR)) {
		printf("%s: fatal error\n", sc->sc_dev.dv_xname);
		ipw_stop(&sc->sc_ic.ic_if, 1);
	}

	if (r & IPW_INTR_FW_INIT_DONE) {
		if (!(r & (IPW_INTR_FATAL_ERROR | IPW_INTR_PARITY_ERROR)))
			wakeup(sc);
	}

	if (r & IPW_INTR_RX_TRANSFER)
		ipw_rx_intr(sc);

	if (r & IPW_INTR_TX_TRANSFER)
		ipw_tx_intr(sc);

	/* Acknowledge interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR, r);

	/* Re-enable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, IPW_INTR_MASK);

	return 1;
}

int
ipw_cmd(struct ipw_softc *sc, u_int32_t type, void *data, u_int32_t len)
{
	struct ipw_soft_bd *sbd;
	int error;

	sbd = &sc->stbd_list[sc->txcur];

	error = bus_dmamap_load(sc->sc_dmat, sc->cmd_map, &sc->cmd,
	    sizeof (struct ipw_cmd), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map command DMA memory\n",
		    sc->sc_dev.dv_xname);
		return error;
	}

	sc->cmd.type = htole32(type);
	sc->cmd.subtype = htole32(0);
	sc->cmd.len = htole32(len);
	sc->cmd.seq = htole32(0);
	if (data != NULL)
		bcopy(data, sc->cmd.data, len);

	sbd->type = IPW_SBD_TYPE_COMMAND;
	sbd->bd->physaddr = htole32(sc->cmd_map->dm_segs[0].ds_addr);
	sbd->bd->len = htole32(sizeof (struct ipw_cmd));
	sbd->bd->nfrag = 1;
	sbd->bd->flags = IPW_BD_FLAG_TX_FRAME_COMMAND |
			 IPW_BD_FLAG_TX_LAST_FRAGMENT;

	bus_dmamap_sync(sc->sc_dmat, sc->cmd_map, 0, sizeof (struct ipw_cmd),
	    BUS_DMASYNC_PREWRITE);
	
	bus_dmamap_sync(sc->sc_dmat, sc->tbd_map,
	    sc->txcur * sizeof (struct ipw_bd), sizeof (struct ipw_bd),
	    BUS_DMASYNC_PREWRITE);

	sc->txcur = (sc->txcur + 1) % IPW_NTBD;
	sc->txfree--;
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE_INDEX, sc->txcur);

	DPRINTFN(2, ("TX!CMD!%u!%u!%u!%u\n", type, 0, 0, len));

	/* Wait at most one second for command to complete */
	return tsleep(sc, 0, "ipwcmd", hz);
}

int
ipw_tx_start(struct ifnet *ifp, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ipw_soft_bd *sbd;
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;
	struct mbuf *mnew;
	int error, i;

	if (ic->ic_flags & IEEE80211_F_WEPON) {
		m = ieee80211_wep_crypt(ifp, m, 1);
		if (m == NULL)
			return ENOBUFS;
	}

#if NBPFILTER > 0
	if (sc->sc_drvbpf != NULL) {
		struct mbuf mb;
		struct ipw_tx_radiotap_header *tap = &sc->sc_txtap;

		tap->wt_flags = 0;
		tap->wt_chan_freq = htole16(ic->ic_bss->ni_chan->ic_freq);
		tap->wt_chan_flags = htole16(ic->ic_bss->ni_chan->ic_flags);

		M_DUP_PKTHDR(&mb, m);
		mb.m_data = (caddr_t)tap;
		mb.m_len = sc->sc_txtap_len;
		mb.m_next = m;
		mb.m_pkthdr.len += mb.m_len;
		bpf_mtap(sc->sc_drvbpf, &mb);
	}
#endif

	wh = mtod(m, struct ieee80211_frame *);

	shdr = SLIST_FIRST(&sc->free_shdr);
	sbuf = SLIST_FIRST(&sc->free_sbuf);

	shdr->hdr.type = htole32(IPW_HDR_TYPE_SEND);
	shdr->hdr.subtype = htole32(0);
	shdr->hdr.encrypted = (wh->i_fc[1] & IEEE80211_FC1_WEP) ? 1 : 0;
	shdr->hdr.encrypt = 0;
	shdr->hdr.keyidx = 0;
	shdr->hdr.keysz = 0;
	shdr->hdr.fragmentsz = htole16(0);
	IEEE80211_ADDR_COPY(shdr->hdr.src_addr, wh->i_addr2);
	if (ic->ic_opmode == IEEE80211_M_STA)
		IEEE80211_ADDR_COPY(shdr->hdr.dst_addr, wh->i_addr3);
	else
		IEEE80211_ADDR_COPY(shdr->hdr.dst_addr, wh->i_addr1);

	/* trim IEEE802.11 header */
	m_adj(m, sizeof (struct ieee80211_frame));

	error = bus_dmamap_load_mbuf(sc->sc_dmat, sbuf->map, m, BUS_DMA_NOWAIT);
	if (error != 0 && error != EFBIG) {
		printf("%s: could not map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		m_freem(m);
		return error;
	}
	if (error != 0) {
		/* too many fragments, linearize */

		MGETHDR(mnew, M_DONTWAIT, MT_DATA);
		if (mnew == NULL) {
			m_freem(m);
			return ENOMEM;
		}

		M_DUP_PKTHDR(mnew, m);
		MCLGET(mnew, M_DONTWAIT);
		if (!(mnew->m_flags & M_EXT)) {
			m_freem(m);
			m_freem(mnew);
			return ENOMEM;
		}

		m_copydata(m, 0, m->m_pkthdr.len, mtod(mnew, caddr_t));
		m_freem(m);
		mnew->m_len = mnew->m_pkthdr.len;
		m = mnew;

		error = bus_dmamap_load_mbuf(sc->sc_dmat, sbuf->map, m,
		    BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not map mbuf (error %d)\n",
			    sc->sc_dev.dv_xname, error);
			m_freem(m);
			return error;
		}
	}

	error = bus_dmamap_load(sc->sc_dmat, shdr->map, &shdr->hdr,
	    sizeof (struct ipw_hdr), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map header DMA memory (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		bus_dmamap_unload(sc->sc_dmat, sbuf->map);
		m_freem(m);
		return error;
	}

	SLIST_REMOVE_HEAD(&sc->free_sbuf, next);
	SLIST_REMOVE_HEAD(&sc->free_shdr, next);

	sbd = &sc->stbd_list[sc->txcur];
	sbd->type = IPW_SBD_TYPE_HEADER;
	sbd->priv = shdr;
	sbd->bd->physaddr = htole32(shdr->map->dm_segs[0].ds_addr);
	sbd->bd->len = htole32(sizeof (struct ipw_hdr));
	sbd->bd->nfrag = 1 + sbuf->map->dm_nsegs;
	sbd->bd->flags = IPW_BD_FLAG_TX_FRAME_802_3 |
			 IPW_BD_FLAG_TX_NOT_LAST_FRAGMENT;

	DPRINTFN(5, ("TX!HDR!%u!%u!%u!%u", shdr->hdr.type, shdr->hdr.subtype,
	    shdr->hdr.encrypted, shdr->hdr.encrypt));
	DPRINTFN(5, ("!%s", ether_sprintf(shdr->hdr.src_addr)));
	DPRINTFN(5, ("!%s\n", ether_sprintf(shdr->hdr.dst_addr)));

	bus_dmamap_sync(sc->sc_dmat, sc->tbd_map,
	    sc->txcur * sizeof (struct ipw_bd),
	    sizeof (struct ipw_bd), BUS_DMASYNC_PREWRITE);

	sc->txcur = (sc->txcur + 1) % IPW_NTBD;
	sc->txfree--;

	sbuf->m = m;
	sbuf->ni = ni;

	for (i = 0; i < sbuf->map->dm_nsegs; i++) {
		sbd = &sc->stbd_list[sc->txcur];
		sbd->bd->physaddr = htole32(sbuf->map->dm_segs[i].ds_addr);
		sbd->bd->len = htole32(sbuf->map->dm_segs[i].ds_len);
		sbd->bd->nfrag = 0; /* used only in first bd */
		sbd->bd->flags = IPW_BD_FLAG_TX_FRAME_802_3;
		if (i == sbuf->map->dm_nsegs - 1) {
			sbd->type = IPW_SBD_TYPE_DATA;
			sbd->priv = sbuf;
			sbd->bd->flags |= IPW_BD_FLAG_TX_LAST_FRAGMENT;
		} else {
			sbd->type = IPW_SBD_TYPE_NOASSOC;
			sbd->bd->flags |= IPW_BD_FLAG_TX_NOT_LAST_FRAGMENT;
		}

		DPRINTFN(5, ("TX!FRAG!%d!%d\n", i,
		    sbuf->map->dm_segs[i].ds_len));

		bus_dmamap_sync(sc->sc_dmat, sc->tbd_map,
		    sc->txcur * sizeof (struct ipw_bd),
		    sizeof (struct ipw_bd), BUS_DMASYNC_PREWRITE);

		sc->txcur = (sc->txcur + 1) % IPW_NTBD;
		sc->txfree--;
	}

	bus_dmamap_sync(sc->sc_dmat, shdr->map, 0, sizeof (struct ipw_hdr),
	    BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, MCLBYTES,
	    BUS_DMASYNC_PREWRITE);

	/* Inform firmware about this new packet */
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE_INDEX, sc->txcur);

	return 0;
}

void
ipw_start(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m;
	struct ieee80211_node *ni;

	if (ic->ic_state != IEEE80211_S_RUN)
		return;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (sc->txfree < 1 + IPW_MAX_NSEG) {
			IF_PREPEND(&ifp->if_snd, m);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

#if NBPFILTER > 0
		if (ifp->if_bpf != NULL)
			bpf_mtap(ifp->if_bpf, m);
#endif

		m = ieee80211_encap(ifp, m, &ni);
		if (m == NULL)
			continue;

#if NBPFILTER > 0
		if (ic->ic_rawbpf != NULL)
			bpf_mtap(ic->ic_rawbpf, m);
#endif

		if (ipw_tx_start(ifp, m, ni) != 0) {
			if (ni != NULL)
				ieee80211_release_node(ic, ni);
			break;
		}

		/* start watchdog timer */
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

void
ipw_watchdog(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
			ipw_stop(ifp, 1);
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

int
ipw_get_table1(struct ipw_softc *sc, u_int32_t *tbl)
{
	u_int32_t i, size, buf[256];

	if (!(sc->flags & IPW_FLAG_FW_INITED)) {
		bzero(buf, sizeof buf);
		return copyout(buf, tbl, sizeof buf);
	}

	CSR_WRITE_4(sc, IPW_CSR_AUTOINC_ADDR, sc->table1_base);

	size = min(CSR_READ_4(sc, IPW_CSR_AUTOINC_DATA), 256);
	for (i = 1; i < size; i++)
		buf[i] = MEM_READ_4(sc, CSR_READ_4(sc, IPW_CSR_AUTOINC_DATA));

	return copyout(buf, tbl, sizeof buf);
}

int
ipw_get_radio(struct ipw_softc *sc, int *ret)
{
	int val;

	val = (CSR_READ_4(sc, IPW_CSR_IO) & IPW_IO_RADIO_DISABLED) ? 0 : 1;
	return copyout(&val, ret, sizeof val);
}

int
ipw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ifreq *ifr;
	struct ifaddr *ifa;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifa = (struct ifaddr *) data;
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			arp_ifinit(&sc->sc_ic.ic_ac, ifa);
			ipw_init(ifp);
			break;
#endif
		default:
			ipw_init(ifp);
		}
		break;

	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (!(ifp->if_flags & IFF_RUNNING))
				ipw_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				ipw_stop(ifp, 1);
		}
		break;

	case SIOCGTABLE1:
		ifr = (struct ifreq *)data;
		error = ipw_get_table1(sc, (u_int32_t *)ifr->ifr_data);
		break;

	case SIOCGRADIO:
		ifr = (struct ifreq *)data;
		error = ipw_get_radio(sc, (int *)ifr->ifr_data);
		break;

	case SIOCG80211AUTH:
		((struct ieee80211_auth *)data)->i_authtype = sc->authmode;
		break;

	case SIOCS80211AUTH:
		/* only super-user can do that! */
		if ((error = suser(curproc, 0)) != 0)
			break;

		sc->authmode = ((struct ieee80211_auth *)data)->i_authtype;
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
	}

	if (error == ENETRESET && cmd != SIOCADDMULTI) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			ipw_init(ifp);
		error = 0;
	}

	splx(s);
	return error;
}

u_int32_t
ipw_read_table1(struct ipw_softc *sc, u_int32_t off)
{
	return MEM_READ_4(sc, MEM_READ_4(sc, sc->table1_base + off));
}

void
ipw_write_table1(struct ipw_softc *sc, u_int32_t off, u_int32_t info)
{
	MEM_WRITE_4(sc, MEM_READ_4(sc, sc->table1_base + off), info);
}

int
ipw_read_table2(struct ipw_softc *sc, u_int32_t off, void *buf, u_int32_t *len)
{
	u_int32_t addr, info;
	u_int16_t count, size;
	u_int32_t total;

	/* addr[4] + count[2] + size[2] */
	addr = MEM_READ_4(sc, sc->table2_base + off);
	info = MEM_READ_4(sc, sc->table2_base + off + 4);

	count = info >> 16;
	size = info & 0xffff;
	total = count * size;

	if (total > *len) {
		*len = total;
		return EINVAL;
	}

	*len = total;
	ipw_read_mem_1(sc, addr, buf, total);

	return 0;
}

void
ipw_stop_master(struct ipw_softc *sc)
{
	int ntries;

	/* Disable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, 0);

	CSR_WRITE_4(sc, IPW_CSR_RST, IPW_RST_STOP_MASTER);
	for (ntries = 0; ntries < 50; ntries++) {
		if (CSR_READ_4(sc, IPW_CSR_RST) & IPW_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}
	if (ntries == 50)
		printf("%s: timeout waiting for master\n",
		    sc->sc_dev.dv_xname);

	CSR_WRITE_4(sc, IPW_CSR_RST, CSR_READ_4(sc, IPW_CSR_RST) |
	    IPW_RST_PRINCETON_RESET);

	sc->flags &= ~IPW_FLAG_FW_INITED;
}

int
ipw_reset(struct ipw_softc *sc)
{
	int ntries;

	ipw_stop_master(sc);

	/* Move adapter to D0 state */
	CSR_WRITE_4(sc, IPW_CSR_CTL, CSR_READ_4(sc, IPW_CSR_CTL) |
	    IPW_CTL_INIT);

	/* Wait for clock stabilization */
	for (ntries = 0; ntries < 1000; ntries++) {
		if (CSR_READ_4(sc, IPW_CSR_CTL) & IPW_CTL_CLOCK_READY)
			break;
		DELAY(200);
	}
	if (ntries == 1000)
		return EIO;

	CSR_WRITE_4(sc, IPW_CSR_RST, CSR_READ_4(sc, IPW_CSR_RST) |
	    IPW_RST_SW_RESET);

	DELAY(10);

	CSR_WRITE_4(sc, IPW_CSR_CTL, CSR_READ_4(sc, IPW_CSR_CTL) |
	    IPW_CTL_INIT);

	return 0;
}

int
ipw_load_ucode(struct ipw_softc *sc, u_char *uc, int size)
{
	int ntries;

	MEM_WRITE_4(sc, 0x3000e0, 0x80000000);
	CSR_WRITE_4(sc, IPW_CSR_RST, 0);

	MEM_WRITE_2(sc, 0x220000, 0x0703);
	MEM_WRITE_2(sc, 0x220000, 0x0707);

	MEM_WRITE_1(sc, 0x210014, 0x72);
	MEM_WRITE_1(sc, 0x210014, 0x72);

	MEM_WRITE_1(sc, 0x210000, 0x40);
	MEM_WRITE_1(sc, 0x210000, 0x00);
	MEM_WRITE_1(sc, 0x210000, 0x40);

	MEM_WRITE_MULTI_1(sc, 0x210010, uc, size);

	MEM_WRITE_1(sc, 0x210000, 0x00);
	MEM_WRITE_1(sc, 0x210000, 0x00);
	MEM_WRITE_1(sc, 0x210000, 0x80);

	MEM_WRITE_2(sc, 0x220000, 0x0703);
	MEM_WRITE_2(sc, 0x220000, 0x0707);

	MEM_WRITE_1(sc, 0x210014, 0x72);
	MEM_WRITE_1(sc, 0x210014, 0x72);

	MEM_WRITE_1(sc, 0x210000, 0x00);
	MEM_WRITE_1(sc, 0x210000, 0x80);

	for (ntries = 0; ntries < 100; ntries++) {
		if (MEM_READ_1(sc, 0x210000) & 1)
			break;
		DELAY(1000);
	}
	if (ntries == 100) {
		printf("%s: timeout waiting for ucode to initialize\n",
		    sc->sc_dev.dv_xname);
		return EIO;
	}

	MEM_WRITE_4(sc, 0x3000e0, 0);

	return 0;
}

/* set of macros to handle unaligned little endian data in firmware image */
#define GETLE32(p) ((p)[0] | (p)[1] << 8 | (p)[2] << 16 | (p)[3] << 24)
#define GETLE16(p) ((p)[0] | (p)[1] << 8)
int
ipw_load_firmware(struct ipw_softc *sc, u_char *fw, int size)
{
	u_char *p, *end;
	u_int32_t dst;
	u_int16_t len;
	int error;

	p = fw;
	end = fw + size;
	while (p < end) {
		if (p + 6 > end)
			return EINVAL;

		dst = GETLE32(p); p += 4;
		len = GETLE16(p); p += 2;

		if (p + len > end)
			return EINVAL;

		ipw_write_mem_1(sc, dst, p, len);
		p += len;
	}

	CSR_WRITE_4(sc, IPW_CSR_IO, IPW_IO_GPIO1_ENABLE | IPW_IO_GPIO3_MASK |
	    IPW_IO_LED_OFF);

	/* Allow interrupts so we know when the firmware is inited */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, IPW_INTR_MASK);

	/* Tell the adapter to initialize the firmware */
	CSR_WRITE_4(sc, IPW_CSR_RST, 0);
	CSR_WRITE_4(sc, IPW_CSR_CTL, CSR_READ_4(sc, IPW_CSR_CTL) |
	    IPW_CTL_ALLOW_STANDBY);

	/* Wait at most one second for firmware initialization to complete */
	if ((error = tsleep(sc, 0, "ipwinit", hz)) != 0) {
		printf("%s: timeout waiting for firmware initialization to "
		    "complete\n", sc->sc_dev.dv_xname);
		return error;
	}

	CSR_WRITE_4(sc, IPW_CSR_IO, CSR_READ_4(sc, IPW_CSR_IO) |
	    IPW_IO_GPIO1_MASK | IPW_IO_GPIO3_MASK);

	return 0;
}

int
ipw_read_firmware(struct ipw_softc *sc, struct ipw_firmware *fw)
{
	struct ipw_firmware_hdr *hdr;
	const char *name;
	u_char *p;
	size_t size;
	int error;

	switch (sc->sc_ic.ic_opmode) {
	case IEEE80211_M_STA:
	case IEEE80211_M_HOSTAP:
		name = "ipw-bss";
		break;

	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		name = "ipw-ibss";
		break;

	case IEEE80211_M_MONITOR:
		name = "ipw-monitor";
		break;
	}

	if ((error = loadfirmware(name, &fw->data, &size)) != 0)
		return error;

	if (size < sizeof (struct ipw_firmware_hdr)) {
		error = EINVAL;
		goto fail;
	}

	p = fw->data;
	hdr = (struct ipw_firmware_hdr *)p;
	fw->main_size = letoh32(hdr->main_size);
	fw->ucode_size = letoh32(hdr->ucode_size);

	p += sizeof (struct ipw_firmware_hdr);
	size -= sizeof (struct ipw_firmware_hdr);

	if (size < fw->main_size + fw->ucode_size) {
		error = EINVAL;
		goto fail;
	}

	fw->main = p;
	fw->ucode = p + fw->main_size;

	return 0;

fail:	free(fw->data, M_DEVBUF);
	return error;
}

int
ipw_config(struct ipw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ipw_security security;
	struct ieee80211_wepkey *k;
	struct ipw_wep_key wepkey;
	struct ipw_scan_options options;
	struct ipw_configuration config;
	u_int32_t data;
	int error, i;

	switch (ic->ic_opmode) {
	case IEEE80211_M_STA:
	case IEEE80211_M_HOSTAP:
		data = htole32(IPW_MODE_BSS);
		break;

	case IEEE80211_M_IBSS:
	case IEEE80211_M_AHDEMO:
		data = htole32(IPW_MODE_IBSS);
		break;

	case IEEE80211_M_MONITOR:
		data = htole32(IPW_MODE_MONITOR);
		break;
	}
	DPRINTF(("Setting mode to %u\n", letoh32(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_MODE, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS ||
	    ic->ic_opmode == IEEE80211_M_MONITOR) {
		data = htole32(ieee80211_chan2ieee(ic, ic->ic_ibss_chan));
		DPRINTF(("Setting channel to %u\n", letoh32(data)));
		error = ipw_cmd(sc, IPW_CMD_SET_CHANNEL, &data, sizeof data);
		if (error != 0)
			return error;
	}

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		DPRINTF(("Enabling adapter\n"));
		return ipw_cmd(sc, IPW_CMD_ENABLE, NULL, 0);
	}

	IEEE80211_ADDR_COPY(ic->ic_myaddr, LLADDR(ifp->if_sadl));
	DPRINTF(("Setting MAC address to %s\n", ether_sprintf(ic->ic_myaddr)));
	error = ipw_cmd(sc, IPW_CMD_SET_MAC_ADDRESS, ic->ic_myaddr,
	    IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;

	config.flags = htole32(IPW_CFG_BSS_MASK | IPW_CFG_IBSS_MASK |
	    IPW_CFG_PREAMBLE_AUTO | IPW_CFG_802_1x_ENABLE);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		config.flags |= htole32(IPW_CFG_IBSS_AUTO_START);
	if (ifp->if_flags & IFF_PROMISC)
		config.flags |= htole32(IPW_CFG_PROMISCUOUS);
	config.bss_chan = htole32(0x3fff); /* channels 1-14 */
	config.ibss_chan = htole32(0x7ff); /* channels 1-11 */
	DPRINTF(("Setting configuration 0x%x\n", config.flags));
	error = ipw_cmd(sc, IPW_CMD_SET_CONFIGURATION, &config, sizeof config);
	if (error != 0)
		return error;

	data = htole32(0x3); /* 1, 2 */
	DPRINTF(("Setting basic tx rates to 0x%x\n", letoh32(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_BASIC_TX_RATES, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(0xf); /* 1, 2, 5.5, 11 */
	DPRINTF(("Setting tx rates to 0x%x\n", letoh32(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_TX_RATES, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(IPW_POWER_MODE_CAM);
	DPRINTF(("Setting power mode to %u\n", letoh32(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_POWER_MODE, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		data = htole32(32); /* default value */
		DPRINTF(("Setting tx power index to %u\n", letoh32(data)));
		error = ipw_cmd(sc, IPW_CMD_SET_TX_POWER_INDEX, &data,
		    sizeof data);
		if (error != 0)
			return error;
	}

	data = htole32(ic->ic_rtsthreshold);
	DPRINTF(("Setting RTS threshold to %u\n", letoh32(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_RTS_THRESHOLD, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(ic->ic_fragthreshold);
	DPRINTF(("Setting frag threshold to %u\n", letoh32(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_FRAG_THRESHOLD, &data, sizeof data);
	if (error != 0)
		return error;

#ifdef IPW_DEBUG
	if (ipw_debug > 0) {
		printf("Setting ESSID to ");
		ieee80211_print_essid(ic->ic_des_essid, ic->ic_des_esslen);
		printf("\n");
	}
#endif
	error = ipw_cmd(sc, IPW_CMD_SET_ESSID, ic->ic_des_essid,
	    ic->ic_des_esslen);
	if (error != 0)
		return error;

	/* no mandatory BSSID */
	DPRINTF(("Setting mandatory BSSID to null\n"));
	error = ipw_cmd(sc, IPW_CMD_SET_MANDATORY_BSSID, NULL, 0);
	if (error != 0)
		return error;

	if (ic->ic_flags & IEEE80211_F_DESBSSID) {
		DPRINTF(("Setting adapter BSSID to %s\n",
		    ether_sprintf(ic->ic_des_bssid)));
		error = ipw_cmd(sc, IPW_CMD_SET_DESIRED_BSSID,
		    ic->ic_des_bssid, IEEE80211_ADDR_LEN);
		if (error != 0)
			return error;
	}

	bzero(&security, sizeof security);
	security.authmode = (sc->authmode == IEEE80211_AUTH_SHARED) ?
	    IPW_AUTH_SHARED : IPW_AUTH_OPEN;
	security.ciphers = htole32(IPW_CIPHER_NONE);
	DPRINTF(("Setting authmode to %u\n", security.authmode));
	error = ipw_cmd(sc, IPW_CMD_SET_SECURITY_INFORMATION, &security,
	    sizeof security);
	if (error != 0)
		return error;

	if (ic->ic_flags & IEEE80211_F_WEPON) {
		k = ic->ic_nw_keys;
		for (i = 0; i < IEEE80211_WEP_NKID; i++, k++) {
			if (k->wk_len == 0)
				continue;

			wepkey.idx = i;
			wepkey.len = k->wk_len;
			bzero(wepkey.key, sizeof wepkey.key);
			bcopy(k->wk_key, wepkey.key, k->wk_len);
			DPRINTF(("Setting wep key index %u len %u\n",
			    wepkey.idx, wepkey.len));
			error = ipw_cmd(sc, IPW_CMD_SET_WEP_KEY, &wepkey,
			    sizeof wepkey);
			if (error != 0)
				return error;
		}

		data = htole32(ic->ic_wep_txkey);
		DPRINTF(("Setting wep tx key index to %u\n", letoh32(data)));
		error = ipw_cmd(sc, IPW_CMD_SET_WEP_KEY_INDEX, &data,
		    sizeof data);
		if (error != 0)
			return error;
	}

	data = htole32((ic->ic_flags & IEEE80211_F_WEPON) ? IPW_WEPON : 0);
	DPRINTF(("Setting wep flags to 0x%x\n", letoh32(data)));
	error = ipw_cmd(sc, IPW_CMD_SET_WEP_FLAGS, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS ||
	    ic->ic_opmode == IEEE80211_M_HOSTAP) {
		data = htole32(ic->ic_lintval);
		DPRINTF(("Setting beacon interval to %u\n", letoh32(data)));
		error = ipw_cmd(sc, IPW_CMD_SET_BEACON_INTERVAL, &data,
		    sizeof data);
		if (error != 0)
			return error;
	}

	options.flags = htole32(0);
	options.channels = htole32(0x3fff); /* scan channels 1-14 */
	DPRINTF(("Setting scan options to 0x%x\n", letoh32(options.flags)));
	error = ipw_cmd(sc, IPW_CMD_SET_SCAN_OPTIONS, &options, sizeof options);
	if (error != 0)
		return error;

	/* finally, enable adapter (start scanning for an access point) */
	DPRINTF(("Enabling adapter\n"));
	return ipw_cmd(sc, IPW_CMD_ENABLE, NULL, 0);
}

int
ipw_init(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ipw_firmware fw;
	int error;

	ipw_stop(ifp, 0);

	if ((error = ipw_reset(sc)) != 0) {
		printf("%s: could not reset adapter\n", sc->sc_dev.dv_xname);
		goto fail1;
	}

	if ((error = ipw_read_firmware(sc, &fw)) != NULL) {
		printf("%s: could not read firmware\n", sc->sc_dev.dv_xname);
		goto fail1;
	}

	if ((error = ipw_load_ucode(sc, fw.ucode, fw.ucode_size)) != 0) {
		printf("%s: could not load microcode\n", sc->sc_dev.dv_xname);
		goto fail2;
	}

	ipw_stop_master(sc);

	/*
	 * Setup tx, rx and status rings
	 */
	CSR_WRITE_4(sc, IPW_CSR_TX_BD_BASE, sc->tbd_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IPW_CSR_TX_BD_SIZE, IPW_NTBD);
	CSR_WRITE_4(sc, IPW_CSR_TX_READ_INDEX, 0);
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE_INDEX, 0);
	sc->txold = IPW_NTBD - 1; /* latest bd index ack'ed by firmware */
	sc->txcur = 0; /* bd index to write to */
	sc->txfree = IPW_NTBD - 2;

	CSR_WRITE_4(sc, IPW_CSR_RX_BD_BASE, sc->rbd_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IPW_CSR_RX_BD_SIZE, IPW_NRBD);
	CSR_WRITE_4(sc, IPW_CSR_RX_READ_INDEX, 0);
	CSR_WRITE_4(sc, IPW_CSR_RX_WRITE_INDEX, IPW_NRBD - 1);
	sc->rxcur = IPW_NRBD - 1; /* latest bd index I've read */

	CSR_WRITE_4(sc, IPW_CSR_RX_STATUS_BASE,
	    sc->status_map->dm_segs[0].ds_addr);

	if ((error = ipw_load_firmware(sc, fw.main, fw.main_size)) != 0) {
		printf("%s: could not load firmware\n", sc->sc_dev.dv_xname);
		goto fail2;
	}

	sc->flags |= IPW_FLAG_FW_INITED;

	/* Retrieve information tables base addresses */
	sc->table1_base = CSR_READ_4(sc, IPW_CSR_TABLE1_BASE);
	sc->table2_base = CSR_READ_4(sc, IPW_CSR_TABLE2_BASE);

	ipw_write_table1(sc, IPW_INFO_LOCK, 0);

	if ((error = ipw_config(sc)) != 0) {
		printf("%s: device configuration failed\n",
		    sc->sc_dev.dv_xname);
		goto fail2;
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	return 0;

fail2:	free(fw.data, M_DEVBUF);
fail1:	ipw_stop(ifp, 0);

	return error;
}

void
ipw_stop(struct ifnet *ifp, int disable)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	int i;

	ipw_stop_master(sc);
	CSR_WRITE_4(sc, IPW_CSR_RST, IPW_RST_SW_RESET);

	/*
	 * Release tx buffers
	 */
	for (i = 0; i < IPW_NTBD; i++)
		ipw_release_sbd(sc, &sc->stbd_list[i]);

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
}

void
ipw_read_mem_1(struct ipw_softc *sc, bus_size_t offset, u_int8_t *datap,
    bus_size_t count)
{
	for (; count > 0; offset++, datap++, count--) {
		CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, offset & ~3);
		*datap = CSR_READ_1(sc, IPW_CSR_INDIRECT_DATA + (offset & 3));
	}
}

void
ipw_write_mem_1(struct ipw_softc *sc, bus_size_t offset, u_int8_t *datap,
    bus_size_t count)
{
	for (; count > 0; offset++, datap++, count--) {
		CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, offset & ~3);
		CSR_WRITE_1(sc, IPW_CSR_INDIRECT_DATA + (offset & 3), *datap);
	}
}

struct cfdriver ipw_cd = {
	0, "ipw", DV_IFNET
};
