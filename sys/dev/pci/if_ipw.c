/*	$Id: if_ipw.c,v 1.1 2004/10/20 12:50:48 deraadt Exp $  */

/*-
 * Copyright (c) 2004
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
 * http://www.intel.com/products/mobiletechnology/prowireless.htm
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

#include "if_ipwreg.h"
#include "if_ipwvar.h"

static int ipw_match(struct device *, void *, void *);
static void ipw_attach(struct device *, struct device *, void *);
static int ipw_detach(struct device *, int);
static int ipw_media_change(struct ifnet *);
static int ipw_newstate(struct ieee80211com *, enum ieee80211_state, int);
static void ipw_command_intr(struct ipw_softc *, struct ipw_soft_buf *);
static void ipw_newstate_intr(struct ipw_softc *, struct ipw_soft_buf *);
static void ipw_data_intr(struct ipw_softc *, struct ipw_status *, 
    struct ipw_soft_bd *, struct ipw_soft_buf *);
static void ipw_notification_intr(struct ipw_softc *, struct ipw_soft_buf *);
static void ipw_rx_intr(struct ipw_softc *);
static void ipw_release_sbd(struct ipw_softc *, struct ipw_soft_bd *);
static void ipw_tx_intr(struct ipw_softc *);
static int ipw_intr(void *);
static int ipw_cmd(struct ipw_softc *, u_int32_t, void *, u_int32_t);
static int ipw_tx_start(struct ifnet *, struct mbuf *, struct ieee80211_node *);
static void ipw_start(struct ifnet *);
static void ipw_watchdog(struct ifnet *);
static int ipw_get_table1(struct ipw_softc *, u_int32_t *);
static int ipw_get_radio(struct ipw_softc *, int *);
static int ipw_ioctl(struct ifnet *, u_long, caddr_t);
static u_int32_t ipw_read_table1(struct ipw_softc *, u_int32_t);
static void ipw_write_table1(struct ipw_softc *, u_int32_t, u_int32_t);
static int ipw_read_table2(struct ipw_softc *, u_int32_t, void *, u_int32_t *);
static int ipw_tx_init(struct ipw_softc *);
static void ipw_tx_stop(struct ipw_softc *);
static int ipw_rx_init(struct ipw_softc *);
static void ipw_rx_stop(struct ipw_softc *);
static void ipw_reset(struct ipw_softc *);
static int ipw_clock_sync(struct ipw_softc *);
static int ipw_load_ucode(struct ipw_softc *, u_char *, int);
static int ipw_load_firmware(struct ipw_softc *, u_char *, int);
static int ipw_firmware_init(struct ipw_softc *, u_char *);
static int ipw_config(struct ipw_softc *);
static int ipw_init(struct ifnet *);
static void ipw_stop(struct ifnet *, int);
static void ipw_read_mem_1(struct ipw_softc *, bus_size_t, u_int8_t *, 
    bus_size_t);
static void ipw_write_mem_1(struct ipw_softc *, bus_size_t, u_int8_t *, 
    bus_size_t);
static void ipw_zero_mem_4(struct ipw_softc *, bus_size_t, bus_size_t);

static inline u_int8_t MEM_READ_1(struct ipw_softc *sc, u_int32_t addr)
{
	CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_1(sc, IPW_CSR_INDIRECT_DATA);
}

static inline u_int16_t MEM_READ_2(struct ipw_softc *sc, u_int32_t addr)
{
	CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, addr);
	return CSR_READ_2(sc, IPW_CSR_INDIRECT_DATA);
}

static inline u_int32_t MEM_READ_4(struct ipw_softc *sc, u_int32_t addr)
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

static int
ipw_match(struct device *parent, void *match, void *aux)
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR (pa->pa_id) == PCI_VENDOR_INTEL && 
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_INTEL_PRO_2100_3B)
		return 1;

	return 0;
}

/* Base Address Register */
#define IPW_PCI_BAR0 0x10

static void
ipw_attach(struct device *parent, struct device *self, void *aux)
{
	struct ipw_softc *sc = (struct ipw_softc *)self;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ieee80211_rateset *rs;
	struct pci_attach_args *pa = aux;
	const char *intrstr;
	bus_space_tag_t memt;
	bus_space_handle_t memh;
	bus_addr_t base;
	pci_intr_handle_t ih;
	u_int32_t data;
	int i, error;

	sc->sc_pct = pa->pa_pc;

	/* enable bus-mastering */
	data = pci_conf_read(sc->sc_pct, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	data |= PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(sc->sc_pct, pa->pa_tag, PCI_COMMAND_STATUS_REG, data);

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
	printf(": %s\n", intrstr);

	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities */
	ic->ic_caps =  IEEE80211_C_IBSS | IEEE80211_C_MONITOR | 
	    IEEE80211_C_PMGT | IEEE80211_C_TXPMGT | IEEE80211_C_WEP;

	/* set supported 11.b rates */
	rs = &ic->ic_sup_rates[IEEE80211_MODE_11B];
	rs->rs_nrates = 4;
	rs->rs_rates[0] = 2;	/* 1Mbps */
	rs->rs_rates[1] = 4;	/* 2Mbps */
	rs->rs_rates[2] = 11;	/* 5.5Mbps */
	rs->rs_rates[3] = 22; 	/* 11Mbps */

	/* set supported 11.b channels (1 through 14) */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq = 
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_B);
		ic->ic_channels[i].ic_flags = IEEE80211_CHAN_B;
	}

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

	ieee80211_media_init(ifp, ipw_media_change, ieee80211_media_status);

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

static int
ipw_detach(struct device* self, int flags)
{
	struct ipw_softc *sc = (struct ipw_softc *)self;
	struct ifnet *ifp = &sc->sc_ic.ic_if;

	ipw_reset(sc);

#if NBPFILTER > 0
	bpfdetach(ifp);
#endif
	ieee80211_ifdetach(ifp);
	if_detach(ifp);

	if (sc->sc_ih != NULL) {
		pci_intr_disestablish(sc->sc_pct, sc->sc_ih);
		sc->sc_ih = NULL;
	}

	bus_space_unmap(sc->sc_st, sc->sc_sh, sc->sc_sz);

	return 0;
}

static int
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

static int
ipw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211_node *ni = ic->ic_bss;
	u_int32_t val, len;

	switch (nstate) {
	case IEEE80211_S_INIT:
		break;

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

	case IEEE80211_S_SCAN:
	case IEEE80211_S_AUTH:
	case IEEE80211_S_ASSOC:
		break;
	}

	ic->ic_state = nstate;
	return 0;
}

static void
ipw_command_intr(struct ipw_softc *sc, struct ipw_soft_buf *sbuf)
{
	struct ipw_cmd *cmd;

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, sizeof (struct ipw_cmd),
	    BUS_DMASYNC_POSTREAD);

	cmd = mtod(sbuf->m, struct ipw_cmd *);

	DPRINTFN(2, ("RX!CMD!%u!%u!%u!%u!%u\n", 
	    letoh32(cmd->type), letoh32(cmd->subtype), letoh32(cmd->seq),
	    letoh32(cmd->len), letoh32(cmd->status)));

	/* 
	 * Wake up processes waiting for command ack. In the case of the 
	 * IPW_CMD_DISABLE command, wake up the process only when the adapter
	 * enters the IPW_STATE_DISABLED state. This is notified in
	 * ipw_newstate_intr().
	 */
	if (letoh32(cmd->type) != IPW_CMD_DISABLE)
		wakeup(sc->cmd);
}

static void
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
		ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);
		break;

	case IPW_STATE_ASSOCIATION_LOST:
		ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
		break;

	case IPW_STATE_DISABLED:
		wakeup(sc->cmd);
		break;

	case IPW_STATE_RADIO_DISABLED:
		/* XXX should turn the interface down */
		break;
	}
}

static void
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

	/* Send it up to the upper layer */
	ieee80211_input(ifp, m, ni, status->rssi, 0/*rstamp*/);

	if (ni == ic->ic_bss)
		ieee80211_unref_node(&ni);
	else
		ieee80211_free_node(ic, ni);

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
		printf("%s: could not map rxbuf dma memory\n", 
		    sc->sc_dev.dv_xname);
		m_freem(m);
		return;
	}

	sbuf->m = m;
	sbd->bd->physaddr = htole32(sbuf->map->dm_segs[0].ds_addr);
}

static void
ipw_notification_intr(struct ipw_softc *sc, struct ipw_soft_buf *sbuf)
{
	DPRINTFN(2, ("RX!NOTIFICATION\n"));
}

static void
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

static void
ipw_release_sbd(struct ipw_softc *sc, struct ipw_soft_bd *sbd)
{
	struct ieee80211com *ic;
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;

	switch (sbd->type) {
	case IPW_SBD_TYPE_COMMAND:
		bus_dmamap_unload(sc->sc_dmat, sc->cmd_map);
		break;

	case IPW_SBD_TYPE_HEADER:
		shdr = sbd->priv;
		bus_dmamap_unload(sc->sc_dmat, shdr->map);
		TAILQ_INSERT_TAIL(&sc->sc_free_shdr, shdr, next);
		break;

	case IPW_SBD_TYPE_DATA:
		ic = &sc->sc_ic;
		sbuf = sbd->priv;
		bus_dmamap_unload(sc->sc_dmat, sbuf->map);
		m_freem(sbuf->m);
		if (sbuf->ni != NULL && sbuf->ni != ic->ic_bss)
			ieee80211_free_node(ic, sbuf->ni);
		/* kill watchdog timer */
		sc->sc_tx_timer = 0;
		TAILQ_INSERT_TAIL(&sc->sc_free_sbuf, sbuf, next);
		break;
	}
	sbd->type = IPW_SBD_TYPE_NOASSOC;
}

static void
ipw_tx_intr(struct ipw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	u_int32_t r, i;

	r = CSR_READ_4(sc, IPW_CSR_TX_READ_INDEX);

	for (i = (sc->txold + 1) % IPW_NTBD; i != r; i = (i + 1) % IPW_NTBD)
		ipw_release_sbd(sc, &sc->stbd_list[i]);

	/* Remember what the firmware has processed */
	sc->txold = (r == 0) ? IPW_NTBD - 1 : r - 1;

	/* Call start() since some buffer descriptors have been released */
	ifp->if_flags &= ~IFF_OACTIVE;
	(*ifp->if_start)(ifp);
}

static int
ipw_intr(void *arg)
{
	struct ipw_softc *sc = arg;
	u_int32_t r;

	if ((r = CSR_READ_4(sc, IPW_CSR_INTR)) == 0)
		return 0;

	/* Disable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, 0);

	DPRINTFN(8, ("INTR!0x%08x\n", r));

	if (r & IPW_INTR_RX_TRANSFER)
		ipw_rx_intr(sc);

	if (r & IPW_INTR_TX_TRANSFER)
		ipw_tx_intr(sc);

	if (r & IPW_INTR_FW_INIT_DONE) {
		if (!(r & (IPW_INTR_FATAL_ERROR | IPW_INTR_PARITY_ERROR)))
			wakeup(sc);
	}

	/* Acknowledge interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR, r);

	/* Re-enable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, IPW_INTR_MASK);

	return 0;
}

static int
ipw_cmd(struct ipw_softc *sc, u_int32_t type, void *data, u_int32_t len)
{
	struct ipw_soft_bd *sbd;
	int error;

	sbd = &sc->stbd_list[sc->txcur];

	error = bus_dmamap_load(sc->sc_dmat, sc->cmd_map, sc->cmd, 
	    sizeof (struct ipw_cmd), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map cmd dma memory\n", 
		    sc->sc_dev.dv_xname);
		return error;
	}

	sc->cmd->type = htole32(type);
	sc->cmd->subtype = htole32(0);
	sc->cmd->len = htole32(len);
	sc->cmd->seq = htole32(0);
	if (data != NULL)
		bcopy(data, sc->cmd->data, len);

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
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE_INDEX, sc->txcur);

	DPRINTFN(2, ("TX!CMD!%u!%u!%u!%u\n", type, 0, 0, len));

	/* Wait at most two seconds for command to complete */
	return tsleep(sc->cmd, 0, "ipwcmd", 2 * hz);
}

static int
ipw_tx_start(struct ifnet *ifp, struct mbuf *m, struct ieee80211_node *ni)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_frame *wh;
	struct ipw_soft_bd *sbd;
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;
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

	shdr = TAILQ_FIRST(&sc->sc_free_shdr);
	sbuf = TAILQ_FIRST(&sc->sc_free_sbuf);

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

	/*
	 * We need to map the mbuf first to know how many buffer descriptors  
	 * are needed for this transfer.
	 */
	error = bus_dmamap_load_mbuf(sc->sc_dmat, sbuf->map, m, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map mbuf (error %d)\n", 
		    sc->sc_dev.dv_xname, error);
		m_freem(m);
		return error;
	}

	error = bus_dmamap_load(sc->sc_dmat, shdr->map, &shdr->hdr,
	    sizeof (struct ipw_hdr), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map header (error %d)\n", 
		    sc->sc_dev.dv_xname, error);
		bus_dmamap_unload(sc->sc_dmat, sbuf->map);
		m_freem(m);
		return error;
	}

	TAILQ_REMOVE(&sc->sc_free_sbuf, sbuf, next);
	TAILQ_REMOVE(&sc->sc_free_shdr, shdr, next);

	sbd = &sc->stbd_list[sc->txcur];
	sbd->type = IPW_SBD_TYPE_HEADER;
	sbd->priv = shdr;
	sbd->bd->physaddr = htole32(shdr->map->dm_segs[0].ds_addr);
	sbd->bd->len = htole32(sizeof (struct ipw_hdr));
	sbd->bd->nfrag = 1 + sbuf->map->dm_nsegs;
	sbd->bd->flags = IPW_BD_FLAG_TX_FRAME_802_3 |
			 IPW_BD_FLAG_TX_NOT_LAST_FRAGMENT;

	DPRINTFN(5, ("TX!HDR!%u!%u!%u!%u\n", shdr->hdr.type, shdr->hdr.subtype, 
	    shdr->hdr.encrypted, shdr->hdr.encrypt)); 
	DPRINTFN(5, ("!%s", ether_sprintf(shdr->hdr.src_addr))); 
	DPRINTFN(5, ("!%s\n", ether_sprintf(shdr->hdr.dst_addr)));

	bus_dmamap_sync(sc->sc_dmat, sc->tbd_map, 
	    sc->txcur * sizeof (struct ipw_bd), 
	    sizeof (struct ipw_bd), BUS_DMASYNC_PREWRITE);

	sc->txcur = (sc->txcur + 1) % IPW_NTBD;

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
	}

	bus_dmamap_sync(sc->sc_dmat, shdr->map, 0, sizeof (struct ipw_hdr), 
	    BUS_DMASYNC_PREWRITE);

	bus_dmamap_sync(sc->sc_dmat, sbuf->map, 0, MCLBYTES, 
	    BUS_DMASYNC_PREWRITE);

	/* Inform firmware about this new packet */
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE_INDEX, sc->txcur);

	return 0;
}

static void
ipw_start(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct mbuf *m;
	struct ieee80211_node *ni;

	for (;;) {
		IF_DEQUEUE(&ifp->if_snd, m);
		if (m == NULL)
			break;

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
			if (ni != NULL && ni != ic->ic_bss)
				ieee80211_free_node(ic, ni);
			break;
		}

		/* start watchdog timer */
		sc->sc_tx_timer = 5;
		ifp->if_timer = 1;
	}
}

static void
ipw_watchdog(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;

	ifp->if_timer = 0;

	if (sc->sc_tx_timer > 0) {
		if (--sc->sc_tx_timer == 0) {
			printf("%s: device timeout\n", sc->sc_dev.dv_xname);
#ifdef notyet
			ipw_init(ifp);
#endif
			return;
		}
		ifp->if_timer = 1;
	}

	ieee80211_watchdog(ifp);
}

static int
ipw_get_table1(struct ipw_softc *sc, u_int32_t *tbl)
{
	u_int32_t addr, data, size, i;

	if (!(sc->flags & IPW_FLAG_FW_INITED))
		return ENOTTY;

	CSR_WRITE_4(sc, IPW_CSR_AUTOINC_ADDR, sc->table1_base);

	size = CSR_READ_4(sc, IPW_CSR_AUTOINC_DATA);
	if (copyout(&size, tbl, sizeof size) != 0)
		return EFAULT;

	for (i = 1, ++tbl; i < size; i++, tbl++) {
		addr = CSR_READ_4(sc, IPW_CSR_AUTOINC_DATA);
		data = MEM_READ_4(sc, addr);
		if (copyout(&data, tbl, sizeof data) != 0)
			return EFAULT;
	}
	return 0;
}

static int
ipw_get_radio(struct ipw_softc *sc, int *ret)
{
	u_int32_t addr;
	int val;

	if (!(sc->flags & IPW_FLAG_FW_INITED))
		return ENOTTY;

	addr = ipw_read_table1(sc, IPW_INFO_EEPROM_ADDRESS);
	if ((MEM_READ_4(sc, addr + 32) >> 24) & 1) {
		val = -1;
		copyout(&val, ret, sizeof val);
		return 0;
	}

	if (CSR_READ_4(sc, IPW_CSR_IO) & IPW_IO_RADIO_DISABLED)
		val = 0;
	else
		val = 1;
		
	copyout(&val, ret, sizeof val);

	return 0;
}

static int
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

	case SIOCSLOADFW:
		/* only super-user can do that! */
		if ((error = suser(curproc, 0)) != 0)
			break;

		ifr = (struct ifreq *)data;
		error = ipw_firmware_init(sc, (u_char *)ifr->ifr_data);
		break;

	case SIOCSKILLFW:
		/* only super-user can do that! */
		if ((error = suser(curproc, 0)) != 0)
			break;

		ipw_reset(sc);
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		if (error != ENETRESET)
			break;

		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == 
		    (IFF_UP | IFF_RUNNING))
			ipw_init(ifp);
		error = 0;
	}

	splx(s);
	return error;
}

static u_int32_t
ipw_read_table1(struct ipw_softc *sc, u_int32_t off)
{
	return MEM_READ_4(sc, MEM_READ_4(sc, sc->table1_base + off));
}

static void
ipw_write_table1(struct ipw_softc *sc, u_int32_t off, u_int32_t info)
{
	MEM_WRITE_4(sc, MEM_READ_4(sc, sc->table1_base + off), info);
}

static int
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

static int
ipw_tx_init(struct ipw_softc *sc)
{
	char *errmsg;
	struct ipw_bd *bd;
	struct ipw_soft_bd *sbd;
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;
	int error, i, nsegs;

	/* Allocate transmission buffer descriptors */
	error = bus_dmamap_create(sc->sc_dmat, IPW_TBD_SZ, 1, IPW_TBD_SZ, 0, 
	    BUS_DMA_NOWAIT, &sc->tbd_map);
	if (error != 0) {
		errmsg = "could not create tbd dma map";
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_TBD_SZ, PAGE_SIZE, 0, 
	    &sc->tbd_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not allocate tbd dma memory";
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->tbd_seg, nsegs, IPW_TBD_SZ, 
	    (caddr_t *)&sc->tbd_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not map tbd dma memory";
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->tbd_map, sc->tbd_list, 
	    IPW_TBD_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not load tbd dma memory";
		goto fail;
	}

	sc->stbd_list = malloc(IPW_NTBD * sizeof (struct ipw_soft_bd), 
	    M_DEVBUF, M_NOWAIT);
	if (sc->stbd_list == NULL) {
		errmsg = "could not allocate soft tbd";
		error = ENOMEM;
		goto fail;
	}
	sbd = sc->stbd_list;
	bd = sc->tbd_list;
	for (i = 0; i < IPW_NTBD; i++, sbd++, bd++) {
		sbd->type = IPW_SBD_TYPE_NOASSOC;
		sbd->bd = bd;
	}

	CSR_WRITE_4(sc, IPW_CSR_TX_BD_BASE, sc->tbd_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IPW_CSR_TX_BD_SIZE, IPW_NTBD);
	CSR_WRITE_4(sc, IPW_CSR_TX_READ_INDEX, 0);
	CSR_WRITE_4(sc, IPW_CSR_TX_WRITE_INDEX, 0);
	sc->txold = IPW_NTBD - 1; /* latest bd index ack'ed by firmware */
	sc->txcur = 0; /* bd index to write to */

	/* Allocate a DMA-able command */
	error = bus_dmamap_create(sc->sc_dmat, sizeof (struct ipw_cmd), 1, 
	    sizeof (struct ipw_cmd), 0, BUS_DMA_NOWAIT, &sc->cmd_map);
	if (error != 0) {
		errmsg = "could not create cmd dma map";
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, sizeof (struct ipw_cmd), 
	    PAGE_SIZE, 0, &sc->cmd_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not allocate cmd dma memory";
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->cmd_seg, nsegs, 
	    sizeof (struct ipw_cmd), (caddr_t *)&sc->cmd, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not map cmd dma memory";
		goto fail;
	}

	/* Allocate a pool of DMA-able headers */
	sc->shdr_list = malloc(IPW_NDATA * sizeof (struct ipw_soft_hdr),
	    M_DEVBUF, M_NOWAIT);
	if (sc->shdr_list == NULL) {
		errmsg = "could not allocate soft hdr";
		error = ENOMEM;
		goto fail;
	}
	TAILQ_INIT(&sc->sc_free_shdr);
	for (i = 0, shdr = sc->shdr_list; i < IPW_NDATA; i++, shdr++) {
		error = bus_dmamap_create(sc->sc_dmat, 
		    sizeof (struct ipw_soft_hdr), 1, 
	 	    sizeof (struct ipw_soft_hdr), 0, BUS_DMA_NOWAIT, 
		    &shdr->map);
		if (error != 0) {
			errmsg = "could not create hdr dma map";
			goto fail;
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_shdr, shdr, next);
	}

	/* Allocate a pool of DMA-able buffers */
	sc->tx_sbuf_list = malloc(IPW_NDATA * sizeof (struct ipw_soft_buf),
	    M_DEVBUF, M_NOWAIT);
	if (sc->tx_sbuf_list == NULL) {
		errmsg = "could not allocate soft txbuf";
		error = ENOMEM;
		goto fail;
	}
	TAILQ_INIT(&sc->sc_free_sbuf);
	for (i = 0, sbuf = sc->tx_sbuf_list; i < IPW_NDATA; i++, sbuf++) {
		error = bus_dmamap_create(sc->sc_dmat, IPW_NDATA * MCLBYTES, 
		    IPW_NDATA, MCLBYTES, 0, BUS_DMA_NOWAIT, &sbuf->map);
		if (error != 0) {
			errmsg = "could not create txbuf dma map";
			goto fail;
		}
		TAILQ_INSERT_TAIL(&sc->sc_free_sbuf, sbuf, next);
	}

	return 0;

fail:	printf("%s: %s\n", sc->sc_dev.dv_xname, errmsg);
	ipw_tx_stop(sc);

	return error;
}

static void
ipw_tx_stop(struct ipw_softc *sc)
{
	struct ipw_soft_hdr *shdr;
	struct ipw_soft_buf *sbuf;
	int i;

	if (sc->tbd_map != NULL) {
		if (sc->tbd_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->tbd_map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->tbd_list, 
			    IPW_TBD_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->tbd_seg, 1);
			sc->tbd_list = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->tbd_map);
		sc->tbd_map = NULL;
	}

	if (sc->stbd_list != NULL) {
		for (i = 0; i < IPW_NTBD; i++)
			ipw_release_sbd(sc, &sc->stbd_list[i]);
		free(sc->stbd_list, M_DEVBUF);
		sc->stbd_list = NULL;
	}

	if (sc->cmd_map != NULL) {
		if (sc->cmd != NULL) {
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->cmd, 
			    sizeof (struct ipw_cmd));
			bus_dmamem_free(sc->sc_dmat, &sc->cmd_seg, 1);
			sc->cmd = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->cmd_map);
		sc->cmd_map = NULL;
	}

	if (sc->shdr_list != NULL) {
		TAILQ_FOREACH(shdr, &sc->sc_free_shdr, next)
			bus_dmamap_destroy(sc->sc_dmat, shdr->map);
		free(sc->shdr_list, M_DEVBUF);
		sc->shdr_list = NULL;
	}


	if (sc->tx_sbuf_list != NULL) {
		TAILQ_FOREACH(sbuf, &sc->sc_free_sbuf, next)
			bus_dmamap_destroy(sc->sc_dmat, sbuf->map);
		free(sc->tx_sbuf_list, M_DEVBUF);
		sc->tx_sbuf_list = NULL;
	}
}

static int
ipw_rx_init(struct ipw_softc *sc)
{
	char *errmsg;
	struct ipw_bd *bd;
	struct ipw_soft_bd *sbd;
	struct ipw_soft_buf *sbuf;
	int error, i, nsegs;

	/* Allocate reception buffer descriptors */
	error = bus_dmamap_create(sc->sc_dmat, IPW_RBD_SZ, 1, IPW_RBD_SZ, 0, 
	    BUS_DMA_NOWAIT, &sc->rbd_map);
	if (error != 0) {
		errmsg = "could not create rbd dma map";
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_RBD_SZ, PAGE_SIZE, 0, 
	    &sc->rbd_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not allocate rbd dma memory";
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->rbd_seg, nsegs, IPW_RBD_SZ, 
	    (caddr_t *)&sc->rbd_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not map rbd dma memory";
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->rbd_map, sc->rbd_list, 
	    IPW_RBD_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not load rbd dma memory";
		goto fail;
	}

	sc->srbd_list = malloc(IPW_NRBD * sizeof (struct ipw_soft_bd), 
	    M_DEVBUF, M_NOWAIT);
	if (sc->srbd_list == NULL) {
		errmsg = "could not allocate soft rbd";
		error = ENOMEM;
		goto fail;
	}
	sbd = sc->srbd_list;
	bd = sc->rbd_list;
	for (i = 0; i < IPW_NRBD; i++, sbd++, bd++) {
		sbd->type = IPW_SBD_TYPE_NOASSOC;
		sbd->bd = bd;
	}

	CSR_WRITE_4(sc, IPW_CSR_RX_BD_BASE, sc->rbd_map->dm_segs[0].ds_addr);
	CSR_WRITE_4(sc, IPW_CSR_RX_BD_SIZE, IPW_NRBD);
	CSR_WRITE_4(sc, IPW_CSR_RX_READ_INDEX, 0);
	CSR_WRITE_4(sc, IPW_CSR_RX_WRITE_INDEX, IPW_NRBD - 1);
	sc->rxcur = IPW_NRBD - 1; /* latest bd index I've read */

	/* Allocate status descriptors */
	error = bus_dmamap_create(sc->sc_dmat, IPW_STATUS_SZ, 1, IPW_STATUS_SZ, 
	    0, BUS_DMA_NOWAIT, &sc->status_map);
	if (error != 0) {
		errmsg = "could not create status dma map";
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat, IPW_STATUS_SZ, PAGE_SIZE, 0, 
	    &sc->status_seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not allocate status dma memory";
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &sc->status_seg, nsegs, 
	    IPW_STATUS_SZ, (caddr_t *)&sc->status_list, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not map status dma memory";
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, sc->status_map, sc->status_list, 
	    IPW_STATUS_SZ, NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		errmsg = "could not load status dma memory";
		goto fail;
	}

	CSR_WRITE_4(sc, IPW_CSR_RX_STATUS_BASE, 
	    sc->status_map->dm_segs[0].ds_addr);

	sc->rx_sbuf_list = malloc(IPW_NRBD * sizeof (struct ipw_soft_buf),
	    M_DEVBUF, M_NOWAIT);
	if (sc->rx_sbuf_list == NULL) {
		errmsg = "could not allocate soft rxbuf";
		error = ENOMEM;
		goto fail;
	}

	sbuf = sc->rx_sbuf_list;
	sbd = sc->srbd_list;
	for (i = 0; i < IPW_NRBD; i++, sbuf++, sbd++) {

		MGETHDR(sbuf->m, M_DONTWAIT, MT_DATA);
		if (sbuf->m == NULL) {
			errmsg = "could not allocate rx mbuf";
			error = ENOMEM;
			goto fail;
		}
		MCLGET(sbuf->m, M_DONTWAIT);
		if (!(sbuf->m->m_flags & M_EXT)) {
			m_freem(sbuf->m);
			errmsg = "could not allocate rx mbuf cluster";
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_create(sc->sc_dmat, IPW_NRBD * MCLBYTES, 
		    IPW_NRBD, MCLBYTES, 0, BUS_DMA_NOWAIT, &sbuf->map);
		if (error != 0) {
			m_freem(sbuf->m);
			errmsg = "could not create rxbuf dma map";
			goto fail;
		}
		error = bus_dmamap_load(sc->sc_dmat, sbuf->map, 
		    mtod(sbuf->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			bus_dmamap_destroy(sc->sc_dmat, sbuf->map);
			m_freem(sbuf->m);
			errmsg = "could not map rxbuf dma memory";
			goto fail;
		}
		sbd->type = IPW_SBD_TYPE_DATA;
		sbd->priv = sbuf;
		sbd->bd->physaddr = htole32(sbuf->map->dm_segs[0].ds_addr);
		sbd->bd->len = htole32(MCLBYTES);
	}

	return 0;

fail:	printf("%s: %s\n", sc->sc_dev.dv_xname, errmsg);
	ipw_rx_stop(sc);

	return error;
}

static void
ipw_rx_stop(struct ipw_softc *sc)
{
	struct ipw_soft_bd *sbd;
	struct ipw_soft_buf *sbuf;
	int i;

	if (sc->rbd_map != NULL) {
		if (sc->rbd_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->rbd_map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->rbd_list, 
			    IPW_RBD_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->rbd_seg, 1);
			sc->rbd_list = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->rbd_map);
		sc->rbd_map = NULL;
	}

	if (sc->status_map != NULL) {
		if (sc->status_list != NULL) {
			bus_dmamap_unload(sc->sc_dmat, sc->status_map);
			bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->status_list, 
			    IPW_STATUS_SZ);
			bus_dmamem_free(sc->sc_dmat, &sc->status_seg, 1);
			sc->status_list = NULL;
		}
		bus_dmamap_destroy(sc->sc_dmat, sc->status_map);
		sc->status_map = NULL;
	}

	if (sc->srbd_list != NULL) {
		for (i = 0, sbd = sc->srbd_list; i < IPW_NRBD; i++, sbd++) {
			if (sbd->type == IPW_SBD_TYPE_NOASSOC)
				continue;

			sbuf = sbd->priv;
			bus_dmamap_unload(sc->sc_dmat, sbuf->map);
			bus_dmamap_destroy(sc->sc_dmat, sbuf->map);
			m_freem(sbuf->m);
		}
		free(sc->srbd_list, M_DEVBUF);
		sc->srbd_list = NULL;
	}

	if (sc->rx_sbuf_list != NULL) {
		free(sc->rx_sbuf_list, M_DEVBUF);
		sc->rx_sbuf_list = NULL;
	}
}

static void
ipw_reset(struct ipw_softc *sc)
{
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int ntries;

	ipw_stop(ifp, 1);

	if (sc->flags & IPW_FLAG_FW_INITED) {
		ipw_cmd(sc, IPW_CMD_DISABLE_PHY, NULL, 0);
		ipw_cmd(sc, IPW_CMD_PREPARE_POWER_DOWN, NULL, 0);

		sc->flags &= ~IPW_FLAG_FW_INITED;
	}

	/* Disable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, 0);

	CSR_WRITE_4(sc, IPW_CSR_RST, IPW_RST_STOP_MASTER);
	for (ntries = 0; ntries < 5; ntries++) {
		if (CSR_READ_4(sc, IPW_CSR_RST) & IPW_RST_MASTER_DISABLED)
			break;
		DELAY(10);
	}

	CSR_WRITE_4(sc, IPW_CSR_RST, IPW_RST_SW_RESET);

	ipw_rx_stop(sc);
	ipw_tx_stop(sc);

	ifp->if_flags &= ~IFF_UP;
}

static int
ipw_clock_sync(struct ipw_softc *sc)
{
	int ntries;
	u_int32_t r;

	CSR_WRITE_4(sc, IPW_CSR_RST, IPW_RST_SW_RESET);
	for (ntries = 0; ntries < 1000; ntries++) {
		if (CSR_READ_4(sc, IPW_CSR_RST) & IPW_RST_PRINCETON_RESET)
			break;
		DELAY(10);
	}
	if (ntries == 1000)
		return EIO;

	CSR_WRITE_4(sc, IPW_CSR_CTL, IPW_CTL_INIT_DONE);
	for (ntries = 0; ntries < 1000; ntries++) {
		if ((r = CSR_READ_4(sc, IPW_CSR_CTL)) & IPW_CTL_CLOCK_READY)
			break;
		DELAY(200);
	}
	if (ntries == 1000)
		return EIO;

	CSR_WRITE_4(sc, IPW_CSR_CTL, r | IPW_CTL_ALLOW_STANDBY);

	return 0;
}

static int
ipw_load_ucode(struct ipw_softc *sc, u_char *uc, int size)
{
	int ntries;

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

	for (ntries = 0; ntries < 10; ntries++) {
		if (MEM_READ_1(sc, 0x210000) & 1)
			break;
		DELAY(10);
	}
	if (ntries == 10)
		return EIO;

	return 0;
}

/* set of macros to handle unaligned little endian data in firmware image */
#define GETLE32(p) ((p)[0] | (p)[1] << 8 | (p)[2] << 16 | (p)[3] << 24)
#define GETLE16(p) ((p)[0] | (p)[1] << 8)
static int
ipw_load_firmware(struct ipw_softc *sc, u_char *fw, int size)
{
	u_char *p, *end;
	u_int32_t addr;
	u_int16_t len;

	p = fw;
	end = fw + size;
	while (p < end) {
		if (p + 6 > end)
			return EINVAL;

		addr = GETLE32(p);
		p += 4;
		len = GETLE16(p);
		p += 2;

		if (p + len > end)
			return EINVAL;

		ipw_write_mem_1(sc, addr, p, len);
		p += len;
	}
	return 0;
}

static int
ipw_firmware_init(struct ipw_softc *sc, u_char *data)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	struct ipw_fw_hdr hdr;
	u_int32_t r, len, fw_size, uc_size;
	u_char *fw, *uc;
	int error;

	ipw_reset(sc);

	if ((error = copyin(data, &hdr, sizeof hdr)) != 0)
		goto fail1;

	fw_size = letoh32(hdr.fw_size);
	uc_size = letoh32(hdr.uc_size);
	data += sizeof hdr;

	if ((fw = malloc(fw_size, M_DEVBUF, M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto fail1;
	}

	if ((error = copyin(data, fw, fw_size)) != 0)
		goto fail2;

	data += fw_size;

	if ((uc = malloc(uc_size, M_DEVBUF, M_NOWAIT)) == NULL) {
		error = ENOMEM;
		goto fail2;
	}

	if ((error = copyin(data, uc, uc_size)) != 0)
		goto fail3;

	if ((error = ipw_clock_sync(sc)) != 0) {
		printf("%s: clock synchronization failed\n", 
		    sc->sc_dev.dv_xname);
		goto fail3;
	}

	MEM_WRITE_4(sc, 0x003000e0, 0x80000000);

	CSR_WRITE_4(sc, IPW_CSR_RST, 0);

	if ((error = ipw_load_ucode(sc, uc, uc_size)) != 0) {
		printf("%s: could not load microcode\n", sc->sc_dev.dv_xname);
		goto fail3;
	}

	MEM_WRITE_4(sc, 0x003000e0, 0);

	if ((error = ipw_clock_sync(sc)) != 0) {
		printf("%s: clock synchronization failed\n", 
		    sc->sc_dev.dv_xname);
		goto fail3;
	}

	if ((error = ipw_load_firmware(sc, fw, fw_size))) {
		printf("%s: could not load firmware\n", sc->sc_dev.dv_xname);
		goto fail3;
	}

	ipw_zero_mem_4(sc, 0x0002f200, 196);
	ipw_zero_mem_4(sc, 0x0002f610, 8);
	ipw_zero_mem_4(sc, 0x0002fa00, 8);
	ipw_zero_mem_4(sc, 0x0002fc00, 4);
	ipw_zero_mem_4(sc, 0x0002ff80, 32);

	if ((error = ipw_rx_init(sc)) != 0) {
		printf("%s: could not initialize rx queue\n", 
		    sc->sc_dev.dv_xname);
		goto fail3;
	}

	if ((error = ipw_tx_init(sc)) != 0) {
		printf("%s: could not initialize tx queue\n",
		    sc->sc_dev.dv_xname);
		goto fail3;
	}

	CSR_WRITE_4(sc, IPW_CSR_IO, IPW_IO_GPIO1_ENABLE | IPW_IO_GPIO3_MASK | 
	    IPW_IO_LED_OFF);

	/* Enable interrupts */
	CSR_WRITE_4(sc, IPW_CSR_INTR_MASK, IPW_INTR_MASK);

	/* Let's go! */
	CSR_WRITE_4(sc, IPW_CSR_RST, 0);

	/* Wait at most 5 seconds for firmware initialization to complete */
	if ((error = tsleep(sc, 0, "ipwinit", 5 * hz)) != 0) {
		printf("%s: timeout waiting for firmware initialization to "
		    "complete\n", sc->sc_dev.dv_xname);
		goto fail3;
	}

	/* Firmware initialization completed */
	sc->flags |= IPW_FLAG_FW_INITED;

	free(uc, M_DEVBUF);
	free(fw, M_DEVBUF);

	r = CSR_READ_4(sc, IPW_CSR_IO);
	CSR_WRITE_4(sc, IPW_CSR_IO, r | IPW_IO_GPIO1_MASK | IPW_IO_GPIO3_MASK);

	/* Retrieve information tables base addresses */
	sc->table1_base = CSR_READ_4(sc, IPW_CSR_TABLE1_BASE);
	sc->table2_base = CSR_READ_4(sc, IPW_CSR_TABLE2_BASE);

	ipw_write_table1(sc, IPW_INFO_LOCK, 0);

	/* Retrieve adapter MAC address */
	len = IEEE80211_ADDR_LEN;
	ipw_read_table2(sc, IPW_INFO_ADAPTER_MAC, ic->ic_myaddr, &len);

	IEEE80211_ADDR_COPY(LLADDR(ifp->if_sadl), ic->ic_myaddr);

	return 0;

fail3:	free(uc, M_DEVBUF);
fail2:	free(fw, M_DEVBUF);
fail1:	ipw_reset(sc);

	return error;
}

static int
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
	DPRINTF(("Setting adapter mode to %u\n", data));
	error = ipw_cmd(sc, IPW_CMD_SET_MODE, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS || 
	    ic->ic_opmode == IEEE80211_M_MONITOR) {
		data = htole32(ieee80211_chan2ieee(ic, ic->ic_ibss_chan));
		DPRINTF(("Setting adapter channel to %u\n", data));
		error = ipw_cmd(sc, IPW_CMD_SET_CHANNEL, &data, sizeof data);
		if (error != 0)
			return error;
	}

	if (ic->ic_opmode == IEEE80211_M_MONITOR) {
		DPRINTF(("Enabling adapter\n"));
		return ipw_cmd(sc, IPW_CMD_ENABLE, NULL, 0);
	}

	DPRINTF(("Setting adapter MAC to %s\n", ether_sprintf(ic->ic_myaddr)));
	IEEE80211_ADDR_COPY(((struct arpcom *)ifp)->ac_enaddr, ic->ic_myaddr);
	IEEE80211_ADDR_COPY(LLADDR(ifp->if_sadl), ic->ic_myaddr);
	error = ipw_cmd(sc, IPW_CMD_SET_MAC_ADDRESS, ic->ic_myaddr, 
	    IEEE80211_ADDR_LEN);
	if (error != 0)
		return error;

	config.flags = htole32(IPW_CFG_BSS_MASK | IPW_CFG_IBSS_MASK | 
			       IPW_CFG_PREAMBLE_LEN | IPW_CFG_802_1x_ENABLE);
	if (ic->ic_opmode == IEEE80211_M_IBSS)
		config.flags |= htole32(IPW_CFG_IBSS_AUTO_START);
	if (ifp->if_flags & IFF_PROMISC)
		config.flags |= htole32(IPW_CFG_PROMISCUOUS);
	config.channels = htole32(0x3fff); /* channels 1-14 */
	config.ibss_chan = htole32(0x7ff);
	DPRINTF(("Setting adapter configuration 0x%08x\n", config.flags));
	error = ipw_cmd(sc, IPW_CMD_SET_CONFIGURATION, &config, sizeof config);
	if (error != 0)
		return error;

	data = htole32(0x3); /* 1, 2 */
	DPRINTF(("Setting adapter basic tx rates to 0x%x\n", data));
	error = ipw_cmd(sc, IPW_CMD_SET_BASIC_TX_RATES, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(0xf); /* 1, 2, 5.5, 11 */
	DPRINTF(("Setting adapter tx rates to 0x%x\n", data));
	error = ipw_cmd(sc, IPW_CMD_SET_TX_RATES, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(IPW_POWER_MODE_CAM);
	DPRINTF(("Setting adapter power mode to %u\n", data));
	error = ipw_cmd(sc, IPW_CMD_SET_POWER_MODE, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS) {
		data = htole32(ic->ic_txpower);
		DPRINTF(("Setting adapter tx power index to %u\n", data));
		error = ipw_cmd(sc, IPW_CMD_SET_TX_POWER_INDEX, &data, 
		    sizeof data);
		if (error != 0)
			return error;
	}

	data = htole32(ic->ic_rtsthreshold);
	DPRINTF(("Setting adapter RTS threshold to %u\n", data));
	error = ipw_cmd(sc, IPW_CMD_SET_RTS_THRESHOLD, &data, sizeof data);
	if (error != 0)
		return error;

	data = htole32(ic->ic_fragthreshold);
	DPRINTF(("Setting adapter frag threshold to %u\n", data));
	error = ipw_cmd(sc, IPW_CMD_SET_FRAG_THRESHOLD, &data, sizeof data);
	if (error != 0)
		return error;

#ifdef IPW_DEBUG
	if (ipw_debug > 0) {
		printf("Setting adapter ESSID to ");
		ieee80211_print_essid(ic->ic_des_essid, ic->ic_des_esslen);
		printf("\n");
	}
#endif
	error = ipw_cmd(sc, IPW_CMD_SET_ESSID, ic->ic_des_essid, 
	    ic->ic_des_esslen);
	if (error != 0)
		return error;

	/* no mandatory BSSID */
	error = ipw_cmd(sc, IPW_CMD_SET_MANDATORY_BSSID, NULL, 0);
	if (error != 0)
		return error;

	if (ic->ic_flags & IEEE80211_F_DESBSSID) {
		DPRINTF(("Setting adapter desired BSSID to %s\n", 
		    ether_sprintf(ic->ic_des_bssid)));
		error = ipw_cmd(sc, IPW_CMD_SET_DESIRED_BSSID, 
		    ic->ic_des_bssid, IEEE80211_ADDR_LEN);
		if (error != 0)
			return error;
	}

	security.authmode = IPW_AUTH_OPEN;
	security.ciphers = htole32(IPW_CIPHER_NONE);
	security.version = htole16(0);
	security.replay_counters_number = 0;
	security.unicast_using_group = 0;
	DPRINTF(("Setting adapter authmode to %u\n", security.authmode));
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
			DPRINTF(("Setting wep key index %d len %d\n", 
			    wepkey.idx, wepkey.len));
			error = ipw_cmd(sc, IPW_CMD_SET_WEP_KEY, &wepkey, 
			    sizeof wepkey);
			if (error != 0)
				return error;
		}

		data = htole32(ic->ic_wep_txkey);
		DPRINTF(("Setting adapter tx key index to %u\n", data));
		error = ipw_cmd(sc, IPW_CMD_SET_WEP_KEY_INDEX, &data, 
		    sizeof data);
		if (error != 0)
			return error;
	}

	data = htole32((sc->sc_ic.ic_flags & IEEE80211_F_WEPON) ? 0x8 : 0);
	DPRINTF(("Setting adapter wep flags to 0x%x\n", data));
	error = ipw_cmd(sc, IPW_CMD_SET_WEP_FLAGS, &data, sizeof data);
	if (error != 0)
		return error;

	if (ic->ic_opmode == IEEE80211_M_IBSS || 
	    ic->ic_opmode == IEEE80211_M_HOSTAP) {
		data = htole32(ic->ic_lintval);
		DPRINTF(("Setting adapter beacon interval to %u\n", data));
		error = ipw_cmd(sc, IPW_CMD_SET_BEACON_INTERVAL, &data, 
		    sizeof data);
		if (error != 0)
			return error;
	}

	options.flags = htole32(0);
	options.channels = htole32(0x3fff); /* scan channels 1-14 */
	error = ipw_cmd(sc, IPW_CMD_SET_SCAN_OPTIONS, &options, sizeof options);
	if (error != 0)
		return error;

	/* finally, enable adapter (start scanning for an access point) */
	DPRINTF(("Enabling adapter\n"));
	error = ipw_cmd(sc, IPW_CMD_ENABLE, NULL, 0);
	if (error != 0)
		return error;

	return 0;
}

static int 
ipw_init(struct ifnet *ifp)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	/* exit immediately if firmware has not been ioctl'd */
	if (!(sc->flags & IPW_FLAG_FW_INITED)) {
		ifp->if_flags &= ~IFF_UP;
		return EIO;
	}

	ipw_stop(ifp, 0);

	if (ipw_config(sc) != 0) {
		printf("%s: device configuration failed\n", 
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	ifp->if_flags &= ~IFF_OACTIVE;
	ifp->if_flags |= IFF_RUNNING;

	ic->ic_bss->ni_chan = ic->ic_channels;

	return 0;

fail:	ipw_stop(ifp, 0);

	return EIO;
}

static void
ipw_stop(struct ifnet *ifp, int disable)
{
	struct ipw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;

	if (ifp->if_flags & IFF_RUNNING) {
		DPRINTF(("Disabling adapter\n"));
		ipw_cmd(sc, IPW_CMD_DISABLE, NULL, 0);
	}

	ifp->if_timer = 0;
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);
}

static void
ipw_read_mem_1(struct ipw_softc *sc, bus_size_t offset, u_int8_t *datap, 
    bus_size_t count)
{
	for (; count > 0; offset++, datap++, count--) {
		CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, offset & ~3);
		*datap = CSR_READ_1(sc, IPW_CSR_INDIRECT_DATA + (offset & 3));
	}
}

static void
ipw_write_mem_1(struct ipw_softc *sc, bus_size_t offset, u_int8_t *datap, 
    bus_size_t count)
{
	for (; count > 0; offset++, datap++, count--) {
		CSR_WRITE_4(sc, IPW_CSR_INDIRECT_ADDR, offset & ~3);
		CSR_WRITE_1(sc, IPW_CSR_INDIRECT_DATA + (offset & 3), *datap);
	}
}

static void
ipw_zero_mem_4(struct ipw_softc *sc, bus_size_t offset, bus_size_t count)
{
	CSR_WRITE_4(sc, IPW_CSR_AUTOINC_ADDR, offset);
	while (count-- > 0)
		CSR_WRITE_4(sc, IPW_CSR_AUTOINC_DATA, 0);
}

struct cfdriver ipw_cd = {
	0, "ipw", DV_IFNET
};
