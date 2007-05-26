/*	$OpenBSD: if_msk.c,v 1.54 2007/05/26 16:44:21 reyk Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999, 2000
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
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: /c/ncvs/src/sys/pci/if_sk.c,v 1.20 2000/04/22 02:16:37 wpaul Exp $
 */

/*
 * Copyright (c) 2003 Nathan L. Binkert <binkertn@umich.edu>
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

/*
 * SysKonnect SK-NET gigabit ethernet driver for FreeBSD. Supports
 * the SK-984x series adapters, both single port and dual port.
 * References:
 * 	The XaQti XMAC II datasheet,
 * http://www.freebsd.org/~wpaul/SysKonnect/xmacii_datasheet_rev_c_9-29.pdf
 *	The SysKonnect GEnesis manual, http://www.syskonnect.com
 *
 * Note: XaQti has been acquired by Vitesse, and Vitesse does not have the
 * XMAC II datasheet online. I have put my copy at people.freebsd.org as a
 * convenience to others until Vitesse corrects this problem:
 *
 * http://people.freebsd.org/~wpaul/SysKonnect/xmacii_datasheet_rev_c_9-29.pdf
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Department of Electrical Engineering
 * Columbia University, New York City
 */

/*
 * The SysKonnect gigabit ethernet adapters consist of two main
 * components: the SysKonnect GEnesis controller chip and the XaQti Corp.
 * XMAC II gigabit ethernet MAC. The XMAC provides all of the MAC
 * components and a PHY while the GEnesis controller provides a PCI
 * interface with DMA support. Each card may have between 512K and
 * 2MB of SRAM on board depending on the configuration.
 *
 * The SysKonnect GEnesis controller can have either one or two XMAC
 * chips connected to it, allowing single or dual port NIC configurations.
 * SysKonnect has the distinction of being the only vendor on the market
 * with a dual port gigabit ethernet NIC. The GEnesis provides dual FIFOs,
 * dual DMA queues, packet/MAC/transmit arbiters and direct access to the
 * XMAC registers. This driver takes advantage of these features to allow
 * both XMACs to operate as independent interfaces.
 */
 
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sockio.h>
#include <sys/mbuf.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/timeout.h>
#include <sys/device.h>
#include <sys/queue.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/udp.h>
#include <netinet/tcp.h>
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>
#include <net/if_vlan_var.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/brgphyreg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/pci/if_skreg.h>
#include <dev/pci/if_mskvar.h>

int mskc_probe(struct device *, void *, void *);
void mskc_attach(struct device *, struct device *self, void *aux);
void mskc_reset(struct sk_softc *);
void mskc_shutdown(void *);
int msk_probe(struct device *, void *, void *);
void msk_attach(struct device *, struct device *self, void *aux);
void msk_reset(struct sk_if_softc *);
int mskcprint(void *, const char *);
int msk_intr(void *);
void msk_intr_yukon(struct sk_if_softc *);
__inline int msk_rxvalid(struct sk_softc *, u_int32_t, u_int32_t);
void msk_rxeof(struct sk_if_softc *, u_int16_t, u_int32_t);
void msk_txeof(struct sk_if_softc *);
int msk_encap(struct sk_if_softc *, struct mbuf *, u_int32_t *);
void msk_start(struct ifnet *);
int msk_ioctl(struct ifnet *, u_long, caddr_t);
void msk_init(void *);
void msk_init_yukon(struct sk_if_softc *);
void msk_stop(struct sk_if_softc *);
void msk_watchdog(struct ifnet *);
int msk_ifmedia_upd(struct ifnet *);
void msk_ifmedia_sts(struct ifnet *, struct ifmediareq *);
int msk_newbuf(struct sk_if_softc *, int, struct mbuf *, bus_dmamap_t);
int msk_alloc_jumbo_mem(struct sk_if_softc *);
void *msk_jalloc(struct sk_if_softc *);
void msk_jfree(caddr_t, u_int, void *);
int msk_init_rx_ring(struct sk_if_softc *);
int msk_init_tx_ring(struct sk_if_softc *);

int msk_miibus_readreg(struct device *, int, int);
void msk_miibus_writereg(struct device *, int, int, int);
void msk_miibus_statchg(struct device *);

void msk_setfilt(struct sk_if_softc *, caddr_t, int);
void msk_setmulti(struct sk_if_softc *);
void msk_setpromisc(struct sk_if_softc *);
void msk_tick(void *);

#ifdef MSK_DEBUG
#define DPRINTF(x)	if (mskdebug) printf x
#define DPRINTFN(n,x)	if (mskdebug >= (n)) printf x
int	mskdebug = 0;

void msk_dump_txdesc(struct msk_tx_desc *, int);
void msk_dump_mbuf(struct mbuf *);
void msk_dump_bytes(const char *, int);
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

/* supported device vendors */
const struct pci_matchid mskc_devices[] = {
	{ PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DGE550SX },
	{ PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DGE550T_B1 },
	{ PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DGE560SX },
	{ PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DGE560T },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_C032 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_C033 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_C034 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_C036 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_C042 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8021CU },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8021X },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8022CU },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8022X },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8035 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8036 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8038 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8039 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8050 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8052 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8053 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8055 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8056 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8058 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8061CU },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8061X },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8062CU },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8062X },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8070 },
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_YUKON_8071 },
	{ PCI_VENDOR_SCHNEIDERKOCH,	PCI_PRODUCT_SCHNEIDERKOCH_SK9Sxx },
	{ PCI_VENDOR_SCHNEIDERKOCH,	PCI_PRODUCT_SCHNEIDERKOCH_SK9Exx }
};

static inline u_int32_t
sk_win_read_4(struct sk_softc *sc, u_int32_t reg)
{
	return CSR_READ_4(sc, reg);
}

static inline u_int16_t
sk_win_read_2(struct sk_softc *sc, u_int32_t reg)
{
	return CSR_READ_2(sc, reg);
}

static inline u_int8_t
sk_win_read_1(struct sk_softc *sc, u_int32_t reg)
{
	return CSR_READ_1(sc, reg);
}

static inline void
sk_win_write_4(struct sk_softc *sc, u_int32_t reg, u_int32_t x)
{
	CSR_WRITE_4(sc, reg, x);
}

static inline void
sk_win_write_2(struct sk_softc *sc, u_int32_t reg, u_int16_t x)
{
	CSR_WRITE_2(sc, reg, x);
}

static inline void
sk_win_write_1(struct sk_softc *sc, u_int32_t reg, u_int8_t x)
{
	CSR_WRITE_1(sc, reg, x);
}

int
msk_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	u_int16_t val;
	int i;

        SK_YU_WRITE_2(sc_if, YUKON_SMICR, YU_SMICR_PHYAD(phy) |
		      YU_SMICR_REGAD(reg) | YU_SMICR_OP_READ);
        
	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		val = SK_YU_READ_2(sc_if, YUKON_SMICR);
		if (val & YU_SMICR_READ_VALID)
			break;
	}

	if (i == SK_TIMEOUT) {
		printf("%s: phy failed to come ready\n",
		       sc_if->sk_dev.dv_xname);
		return (0);
	}
        
 	DPRINTFN(9, ("msk_miibus_readreg: i=%d, timeout=%d\n", i,
		     SK_TIMEOUT));

        val = SK_YU_READ_2(sc_if, YUKON_SMIDR);

	DPRINTFN(9, ("msk_miibus_readreg phy=%d, reg=%#x, val=%#x\n",
		     phy, reg, val));

	return (val);
}

void
msk_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	int i;

	DPRINTFN(9, ("msk_miibus_writereg phy=%d reg=%#x val=%#x\n",
		     phy, reg, val));

	SK_YU_WRITE_2(sc_if, YUKON_SMIDR, val);
	SK_YU_WRITE_2(sc_if, YUKON_SMICR, YU_SMICR_PHYAD(phy) |
		      YU_SMICR_REGAD(reg) | YU_SMICR_OP_WRITE);

	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		if (!(SK_YU_READ_2(sc_if, YUKON_SMICR) & YU_SMICR_BUSY))
			break;
	}

	if (i == SK_TIMEOUT)
		printf("%s: phy write timed out\n", sc_if->sk_dev.dv_xname);
}

void
msk_miibus_statchg(struct device *dev)
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	struct mii_data *mii = &sc_if->sk_mii;
	struct ifmedia_entry *ife = mii->mii_media.ifm_cur;
	int gpcr;

	gpcr = SK_YU_READ_2(sc_if, YUKON_GPCR);
	gpcr &= (YU_GPCR_TXEN | YU_GPCR_RXEN);

	if (IFM_SUBTYPE(ife->ifm_media) != IFM_AUTO) {
		/* Set speed. */
		gpcr |= YU_GPCR_SPEED_DIS;
		switch (IFM_SUBTYPE(mii->mii_media_active)) {
		case IFM_1000_SX:
		case IFM_1000_LX:
		case IFM_1000_CX:
		case IFM_1000_T:
			gpcr |= (YU_GPCR_GIG | YU_GPCR_SPEED);
			break;
		case IFM_100_TX:
			gpcr |= YU_GPCR_SPEED;
			break;
		}

		/* Set duplex. */
		gpcr |= YU_GPCR_DPLX_DIS;
		if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX)
			gpcr |= YU_GPCR_DUPLEX;

		/* Disable flow control. */
		gpcr |= YU_GPCR_FCTL_DIS;
		gpcr |= (YU_GPCR_FCTL_TX_DIS | YU_GPCR_FCTL_RX_DIS);
	}

	SK_YU_WRITE_2(sc_if, YUKON_GPCR, gpcr);

	DPRINTFN(9, ("msk_miibus_statchg: gpcr=%x\n",
		     SK_YU_READ_2(((struct sk_if_softc *)dev), YUKON_GPCR)));
}

void
msk_setfilt(struct sk_if_softc *sc_if, caddr_t addr, int slot)
{
	int base = XM_RXFILT_ENTRY(slot);

	SK_XM_WRITE_2(sc_if, base, *(u_int16_t *)(&addr[0]));
	SK_XM_WRITE_2(sc_if, base + 2, *(u_int16_t *)(&addr[2]));
	SK_XM_WRITE_2(sc_if, base + 4, *(u_int16_t *)(&addr[4]));
}

void
msk_setmulti(struct sk_if_softc *sc_if)
{
	struct ifnet *ifp= &sc_if->arpcom.ac_if;
	u_int32_t hashes[2] = { 0, 0 };
	int h;
	struct arpcom *ac = &sc_if->arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;

	/* First, zot all the existing filters. */
	SK_YU_WRITE_2(sc_if, YUKON_MCAH1, 0);
	SK_YU_WRITE_2(sc_if, YUKON_MCAH2, 0);
	SK_YU_WRITE_2(sc_if, YUKON_MCAH3, 0);
	SK_YU_WRITE_2(sc_if, YUKON_MCAH4, 0);


	/* Now program new ones. */
allmulti:
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		hashes[0] = 0xFFFFFFFF;
		hashes[1] = 0xFFFFFFFF;
	} else {
		/* First find the tail of the list. */
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
				 ETHER_ADDR_LEN)) {
				ifp->if_flags |= IFF_ALLMULTI;
				goto allmulti;
			}
			h = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN) &
			    ((1 << SK_HASH_BITS) - 1);
			if (h < 32)
				hashes[0] |= (1 << h);
			else
				hashes[1] |= (1 << (h - 32));

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	SK_YU_WRITE_2(sc_if, YUKON_MCAH1, hashes[0] & 0xffff);
	SK_YU_WRITE_2(sc_if, YUKON_MCAH2, (hashes[0] >> 16) & 0xffff);
	SK_YU_WRITE_2(sc_if, YUKON_MCAH3, hashes[1] & 0xffff);
	SK_YU_WRITE_2(sc_if, YUKON_MCAH4, (hashes[1] >> 16) & 0xffff);
}

void
msk_setpromisc(struct sk_if_softc *sc_if)
{
	struct ifnet *ifp = &sc_if->arpcom.ac_if;

	if (ifp->if_flags & IFF_PROMISC)
		SK_YU_CLRBIT_2(sc_if, YUKON_RCR,
		    YU_RCR_UFLEN | YU_RCR_MUFLEN);
	else
		SK_YU_SETBIT_2(sc_if, YUKON_RCR,
		    YU_RCR_UFLEN | YU_RCR_MUFLEN);
}

int
msk_init_rx_ring(struct sk_if_softc *sc_if)
{
	struct msk_chain_data	*cd = &sc_if->sk_cdata;
	struct msk_ring_data	*rd = sc_if->sk_rdata;
	int			i, nexti;

	bzero((char *)rd->sk_rx_ring,
	    sizeof(struct msk_rx_desc) * MSK_RX_RING_CNT);

	for (i = 0; i < MSK_RX_RING_CNT; i++) {
		cd->sk_rx_chain[i].sk_le = &rd->sk_rx_ring[i];
		if (i == (MSK_RX_RING_CNT - 1))
			nexti = 0;
		else
			nexti = i + 1;
		cd->sk_rx_chain[i].sk_next = &cd->sk_rx_chain[nexti];
	}

	for (i = 0; i < MSK_RX_RING_CNT; i++) {
		if (msk_newbuf(sc_if, i, NULL,
		    sc_if->sk_cdata.sk_rx_jumbo_map) == ENOBUFS) {
			printf("%s: failed alloc of %dth mbuf\n",
			    sc_if->sk_dev.dv_xname, i);
			return (ENOBUFS);
		}
	}

	sc_if->sk_cdata.sk_rx_prod = MSK_RX_RING_CNT - 1;
	sc_if->sk_cdata.sk_rx_cons = 0;

	return (0);
}

int
msk_init_tx_ring(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct msk_chain_data	*cd = &sc_if->sk_cdata;
	struct msk_ring_data	*rd = sc_if->sk_rdata;
	bus_dmamap_t		dmamap;
	struct sk_txmap_entry	*entry;
	int			i, nexti;

	bzero((char *)sc_if->sk_rdata->sk_tx_ring,
	    sizeof(struct msk_tx_desc) * MSK_TX_RING_CNT);

	SIMPLEQ_INIT(&sc_if->sk_txmap_head);
	for (i = 0; i < MSK_TX_RING_CNT; i++) {
		cd->sk_tx_chain[i].sk_le = &rd->sk_tx_ring[i];
		if (i == (MSK_TX_RING_CNT - 1))
			nexti = 0;
		else
			nexti = i + 1;
		cd->sk_tx_chain[i].sk_next = &cd->sk_tx_chain[nexti];

		if (bus_dmamap_create(sc->sc_dmatag, SK_JLEN, SK_NTXSEG,
		   SK_JLEN, 0, BUS_DMA_NOWAIT, &dmamap))
			return (ENOBUFS);

		entry = malloc(sizeof(*entry), M_DEVBUF, M_NOWAIT);
		if (!entry) {
			bus_dmamap_destroy(sc->sc_dmatag, dmamap);
			return (ENOBUFS);
		}
		entry->dmamap = dmamap;
		SIMPLEQ_INSERT_HEAD(&sc_if->sk_txmap_head, entry, link);
	}

	sc_if->sk_cdata.sk_tx_prod = 0;
	sc_if->sk_cdata.sk_tx_cons = 0;
	sc_if->sk_cdata.sk_tx_cnt = 0;

	MSK_CDTXSYNC(sc_if, 0, MSK_TX_RING_CNT,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	return (0);
}

int
msk_newbuf(struct sk_if_softc *sc_if, int i, struct mbuf *m,
	  bus_dmamap_t dmamap)
{
	struct mbuf		*m_new = NULL;
	struct sk_chain		*c;
	struct msk_rx_desc	*r;

	if (m == NULL) {
		caddr_t buf = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return (ENOBUFS);
		
		/* Allocate the jumbo buffer */
		buf = msk_jalloc(sc_if);
		if (buf == NULL) {
			m_freem(m_new);
			DPRINTFN(1, ("%s jumbo allocation failed -- packet "
			    "dropped!\n", sc_if->arpcom.ac_if.if_xname));
			return (ENOBUFS);
		}

		/* Attach the buffer to the mbuf */
		m_new->m_len = m_new->m_pkthdr.len = SK_JLEN;
		MEXTADD(m_new, buf, SK_JLEN, 0, msk_jfree, sc_if);
	} else {
		/*
	 	 * We're re-using a previously allocated mbuf;
		 * be sure to re-init pointers and lengths to
		 * default values.
		 */
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = SK_JLEN;
		m_new->m_data = m_new->m_ext.ext_buf;
	}
	m_adj(m_new, ETHER_ALIGN);

	c = &sc_if->sk_cdata.sk_rx_chain[i];
	r = c->sk_le;
	c->sk_mbuf = m_new;
	r->sk_addr = htole32(dmamap->dm_segs[0].ds_addr +
	    (((vaddr_t)m_new->m_data
             - (vaddr_t)sc_if->sk_cdata.sk_jumbo_buf)));
	r->sk_len = htole16(SK_JLEN);
	r->sk_ctl = 0;
	r->sk_opcode = SK_Y2_RXOPC_PACKET | SK_Y2_RXOPC_OWN;

	MSK_CDRXSYNC(sc_if, i, BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	return (0);
}

/*
 * Memory management for jumbo frames.
 */

int
msk_alloc_jumbo_mem(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	caddr_t			ptr, kva;
	bus_dma_segment_t	seg;
	int		i, rseg, state, error;
	struct sk_jpool_entry   *entry;

	state = error = 0;

	/* Grab a big chunk o' storage. */
	if (bus_dmamem_alloc(sc->sc_dmatag, MSK_JMEM, PAGE_SIZE, 0,
			     &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf(": can't alloc rx buffers");
		return (ENOBUFS);
	}

	state = 1;
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg, MSK_JMEM, &kva,
			   BUS_DMA_NOWAIT)) {
		printf(": can't map dma buffers (%d bytes)", MSK_JMEM);
		error = ENOBUFS;
		goto out;
	}

	state = 2;
	if (bus_dmamap_create(sc->sc_dmatag, MSK_JMEM, 1, MSK_JMEM, 0,
	    BUS_DMA_NOWAIT, &sc_if->sk_cdata.sk_rx_jumbo_map)) {
		printf(": can't create dma map");
		error = ENOBUFS;
		goto out;
	}

	state = 3;
	if (bus_dmamap_load(sc->sc_dmatag, sc_if->sk_cdata.sk_rx_jumbo_map,
			    kva, MSK_JMEM, NULL, BUS_DMA_NOWAIT)) {
		printf(": can't load dma map");
		error = ENOBUFS;
		goto out;
	}

	state = 4;
	sc_if->sk_cdata.sk_jumbo_buf = (caddr_t)kva;
	DPRINTFN(1,("msk_jumbo_buf = 0x%08X\n", sc_if->sk_cdata.sk_jumbo_buf));

	LIST_INIT(&sc_if->sk_jfree_listhead);
	LIST_INIT(&sc_if->sk_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array.
	 */
	ptr = sc_if->sk_cdata.sk_jumbo_buf;
	for (i = 0; i < MSK_JSLOTS; i++) {
		sc_if->sk_cdata.sk_jslots[i] = ptr;
		ptr += SK_JLEN;
		entry = malloc(sizeof(struct sk_jpool_entry),
		    M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			sc_if->sk_cdata.sk_jumbo_buf = NULL;
			printf(": no memory for jumbo buffer queue!");
			error = ENOBUFS;
			goto out;
		}
		entry->slot = i;
		LIST_INSERT_HEAD(&sc_if->sk_jfree_listhead,
				 entry, jpool_entries);
	}
out:
	if (error != 0) {
		switch (state) {
		case 4:
			bus_dmamap_unload(sc->sc_dmatag,
			    sc_if->sk_cdata.sk_rx_jumbo_map);
		case 3:
			bus_dmamap_destroy(sc->sc_dmatag,
			    sc_if->sk_cdata.sk_rx_jumbo_map);
		case 2:
			bus_dmamem_unmap(sc->sc_dmatag, kva, MSK_JMEM);
		case 1:
			bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
			break;
		default:
			break;
		}
	}

	return (error);
}

/*
 * Allocate a jumbo buffer.
 */
void *
msk_jalloc(struct sk_if_softc *sc_if)
{
	struct sk_jpool_entry   *entry;

	entry = LIST_FIRST(&sc_if->sk_jfree_listhead);

	if (entry == NULL)
		return (NULL);

	LIST_REMOVE(entry, jpool_entries);
	LIST_INSERT_HEAD(&sc_if->sk_jinuse_listhead, entry, jpool_entries);
	return (sc_if->sk_cdata.sk_jslots[entry->slot]);
}

/*
 * Release a jumbo buffer.
 */
void
msk_jfree(caddr_t buf, u_int size, void	*arg)
{
	struct sk_jpool_entry *entry;
	struct sk_if_softc *sc;
	int i;

	/* Extract the softc struct pointer. */
	sc = (struct sk_if_softc *)arg;

	if (sc == NULL)
		panic("msk_jfree: can't find softc pointer!");

	/* calculate the slot this buffer belongs to */
	i = ((vaddr_t)buf
	     - (vaddr_t)sc->sk_cdata.sk_jumbo_buf) / SK_JLEN;

	if ((i < 0) || (i >= MSK_JSLOTS))
		panic("msk_jfree: asked to free buffer that we don't manage!");

	entry = LIST_FIRST(&sc->sk_jinuse_listhead);
	if (entry == NULL)
		panic("msk_jfree: buffer not in use!");
	entry->slot = i;
	LIST_REMOVE(entry, jpool_entries);
	LIST_INSERT_HEAD(&sc->sk_jfree_listhead, entry, jpool_entries);
}

/*
 * Set media options.
 */
int
msk_ifmedia_upd(struct ifnet *ifp)
{
	struct sk_if_softc *sc_if = ifp->if_softc;

	mii_mediachg(&sc_if->sk_mii);
	return (0);
}

/*
 * Report current media status.
 */
void
msk_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct sk_if_softc *sc_if = ifp->if_softc;

	mii_pollstat(&sc_if->sk_mii);
	ifmr->ifm_active = sc_if->sk_mii.mii_media_active;
	ifmr->ifm_status = sc_if->sk_mii.mii_media_status;
}

int
msk_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct sk_if_softc *sc_if = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct mii_data *mii;
	int s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc_if->arpcom, command, data)) > 0) {
		splx(s);
		return (error);
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			msk_init(sc_if);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc_if->arpcom, ifa);
#endif /* INET */
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu < ETHERMIN || ifr->ifr_mtu > ifp->if_hardmtu)
			error = EINVAL;
		else if (ifp->if_mtu != ifr->ifr_mtu)
			ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    (sc_if->sk_if_flags ^ ifp->if_flags) &
			     IFF_PROMISC) {
				msk_setpromisc(sc_if);
				msk_setmulti(sc_if);
			} else {
				if (!(ifp->if_flags & IFF_RUNNING))
					msk_init(sc_if);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				msk_stop(sc_if);
		}
		sc_if->sk_if_flags = ifp->if_flags;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc_if->arpcom) :
		    ether_delmulti(ifr, &sc_if->arpcom);

		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				msk_setmulti(sc_if);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = &sc_if->sk_mii;
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = ENOTTY;
		break;
	}

	splx(s);

	return (error);
}

/*
 * Probe for a SysKonnect GEnesis chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
int
mskc_probe(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, mskc_devices,
	    sizeof(mskc_devices)/sizeof(mskc_devices[0])));
}

/*
 * Force the GEnesis into reset, then bring it out of reset.
 */
void
mskc_reset(struct sk_softc *sc)
{
	u_int32_t imtimer_ticks, reg1;
	int reg;

	DPRINTFN(2, ("mskc_reset\n"));

	CSR_WRITE_1(sc, SK_CSR, SK_CSR_SW_RESET);
	CSR_WRITE_1(sc, SK_CSR, SK_CSR_MASTER_RESET);

	DELAY(1000);
	CSR_WRITE_1(sc, SK_CSR, SK_CSR_SW_UNRESET);
	DELAY(2);
	CSR_WRITE_1(sc, SK_CSR, SK_CSR_MASTER_UNRESET);

	sk_win_write_1(sc, SK_TESTCTL1, 2);

	reg1 = sk_win_read_4(sc, SK_Y2_PCI_REG(SK_PCI_OURREG1));
	if (sc->sk_type == SK_YUKON_XL && sc->sk_rev > SK_YUKON_XL_REV_A1)
		reg1 |= (SK_Y2_REG1_PHY1_COMA | SK_Y2_REG1_PHY2_COMA);
	else
		reg1 &= ~(SK_Y2_REG1_PHY1_COMA | SK_Y2_REG1_PHY2_COMA);
	sk_win_write_4(sc, SK_Y2_PCI_REG(SK_PCI_OURREG1), reg1);

	if (sc->sk_type == SK_YUKON_XL && sc->sk_rev > SK_YUKON_XL_REV_A1)
		sk_win_write_1(sc, SK_Y2_CLKGATE,
		    SK_Y2_CLKGATE_LINK1_GATE_DIS |
		    SK_Y2_CLKGATE_LINK2_GATE_DIS |
		    SK_Y2_CLKGATE_LINK1_CORE_DIS |
		    SK_Y2_CLKGATE_LINK2_CORE_DIS |
		    SK_Y2_CLKGATE_LINK1_PCI_DIS | SK_Y2_CLKGATE_LINK2_PCI_DIS);
	else
		sk_win_write_1(sc, SK_Y2_CLKGATE, 0);

	CSR_WRITE_2(sc, SK_LINK_CTRL, SK_LINK_RESET_SET);
	CSR_WRITE_2(sc, SK_LINK_CTRL + SK_WIN_LEN, SK_LINK_RESET_SET);
	DELAY(1000);
	CSR_WRITE_2(sc, SK_LINK_CTRL, SK_LINK_RESET_CLEAR);
	CSR_WRITE_2(sc, SK_LINK_CTRL + SK_WIN_LEN, SK_LINK_RESET_CLEAR);

	sk_win_write_1(sc, SK_TESTCTL1, 1);

	DPRINTFN(2, ("mskc_reset: sk_csr=%x\n", CSR_READ_1(sc, SK_CSR)));
	DPRINTFN(2, ("mskc_reset: sk_link_ctrl=%x\n",
		     CSR_READ_2(sc, SK_LINK_CTRL)));

	/* Disable ASF */
	CSR_WRITE_1(sc, SK_Y2_ASF_CSR, SK_Y2_ASF_RESET);
	CSR_WRITE_2(sc, SK_CSR, SK_CSR_ASF_OFF);

	/* Clear I2C IRQ noise */
	CSR_WRITE_4(sc, SK_I2CHWIRQ, 1);

	/* Disable hardware timer */
	CSR_WRITE_1(sc, SK_TIMERCTL, SK_IMCTL_STOP);
	CSR_WRITE_1(sc, SK_TIMERCTL, SK_IMCTL_IRQ_CLEAR);

	/* Disable descriptor polling */
	CSR_WRITE_4(sc, SK_DPT_TIMER_CTRL, SK_DPT_TCTL_STOP);

	/* Disable time stamps */
	CSR_WRITE_1(sc, SK_TSTAMP_CTL, SK_TSTAMP_STOP);
	CSR_WRITE_1(sc, SK_TSTAMP_CTL, SK_TSTAMP_IRQ_CLEAR);

	/* Enable RAM interface */
	sk_win_write_1(sc, SK_RAMCTL, SK_RAMCTL_UNRESET);
	for (reg = SK_TO0;reg <= SK_TO11; reg++)
		sk_win_write_1(sc, reg, 36);
	sk_win_write_1(sc, SK_RAMCTL + (SK_WIN_LEN / 2), SK_RAMCTL_UNRESET);
	for (reg = SK_TO0;reg <= SK_TO11; reg++)
		sk_win_write_1(sc, reg + (SK_WIN_LEN / 2), 36);

	/*
	 * Configure interrupt moderation. The moderation timer
	 * defers interrupts specified in the interrupt moderation
	 * timer mask based on the timeout specified in the interrupt
	 * moderation timer init register. Each bit in the timer
	 * register represents one tick, so to specify a timeout in
	 * microseconds, we have to multiply by the correct number of
	 * ticks-per-microsecond.
	 */
	switch (sc->sk_type) {
	case SK_YUKON_EC:
	case SK_YUKON_XL:
	case SK_YUKON_FE:
		imtimer_ticks = SK_IMTIMER_TICKS_YUKON_EC;
		break;
	default:
		imtimer_ticks = SK_IMTIMER_TICKS_YUKON;
	}

	/* Reset status ring. */
	bzero((char *)sc->sk_status_ring,
	    MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc));
	sc->sk_status_idx = 0;

	sk_win_write_4(sc, SK_STAT_BMU_CSR, SK_STAT_BMU_RESET);
	sk_win_write_4(sc, SK_STAT_BMU_CSR, SK_STAT_BMU_UNRESET);

	sk_win_write_2(sc, SK_STAT_BMU_LIDX, MSK_STATUS_RING_CNT - 1);
	sk_win_write_4(sc, SK_STAT_BMU_ADDRLO,
	    sc->sk_status_map->dm_segs[0].ds_addr);
	sk_win_write_4(sc, SK_STAT_BMU_ADDRHI,
	    (u_int64_t)sc->sk_status_map->dm_segs[0].ds_addr >> 32);
	sk_win_write_2(sc, SK_STAT_BMU_TX_THRESH, 10);
	sk_win_write_1(sc, SK_STAT_BMU_FIFOWM, 16);
	sk_win_write_1(sc, SK_STAT_BMU_FIFOIWM, 16);

#if 0
	sk_win_write_4(sc, SK_Y2_LEV_TIMERINIT, SK_IM_USECS(100));
	sk_win_write_4(sc, 0x0ec0, SK_IM_USECS(1000));

	sk_win_write_4(sc, 0x0ed0, SK_IM_USECS(20));
#else
	sk_win_write_4(sc, SK_Y2_ISR_ITIMERINIT, SK_IM_USECS(4));
#endif

	sk_win_write_4(sc, SK_STAT_BMU_CSR, SK_STAT_BMU_ON);

	sk_win_write_1(sc, SK_Y2_LEV_ITIMERCTL, SK_IMCTL_START);
	sk_win_write_1(sc, SK_Y2_TX_ITIMERCTL, SK_IMCTL_START);
	sk_win_write_1(sc, SK_Y2_ISR_ITIMERCTL, SK_IMCTL_START);
}

int
msk_probe(struct device *parent, void *match, void *aux)
{
	struct skc_attach_args *sa = aux;

	if (sa->skc_port != SK_PORT_A && sa->skc_port != SK_PORT_B)
		return (0);

	switch (sa->skc_type) {
	case SK_YUKON_XL:
	case SK_YUKON_EC_U:
	case SK_YUKON_EX:
	case SK_YUKON_EC:
	case SK_YUKON_FE:
		return (1);
	}

	return (0);
}

void
msk_reset(struct sk_if_softc *sc_if)
{
	/* GMAC and GPHY Reset */
	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_RESET_SET);
	SK_IF_WRITE_4(sc_if, 0, SK_GPHY_CTRL, SK_GPHY_RESET_SET);
	DELAY(1000);
	SK_IF_WRITE_4(sc_if, 0, SK_GPHY_CTRL, SK_GPHY_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_LOOP_OFF |
		      SK_GMAC_PAUSE_ON | SK_GMAC_RESET_CLEAR);
}

/*
 * Each XMAC chip is attached as a separate logical IP interface.
 * Single port cards will have only one logical interface of course.
 */
void
msk_attach(struct device *parent, struct device *self, void *aux)
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *) self;
	struct sk_softc *sc = (struct sk_softc *)parent;
	struct skc_attach_args *sa = aux;
	struct ifnet *ifp;
	caddr_t kva;
	bus_dma_segment_t seg;
	int i, rseg;
	u_int32_t chunk;
	int mii_flags;

	sc_if->sk_port = sa->skc_port;
	sc_if->sk_softc = sc;
	sc->sk_if[sa->skc_port] = sc_if;

	DPRINTFN(2, ("begin msk_attach: port=%d\n", sc_if->sk_port));

	/*
	 * Get station address for this interface. Note that
	 * dual port cards actually come with three station
	 * addresses: one for each port, plus an extra. The
	 * extra one is used by the SysKonnect driver software
	 * as a 'virtual' station address for when both ports
	 * are operating in failover mode. Currently we don't
	 * use this extra address.
	 */
	for (i = 0; i < ETHER_ADDR_LEN; i++)
		sc_if->arpcom.ac_enaddr[i] =
		    sk_win_read_1(sc, SK_MAC0_0 + (sa->skc_port * 8) + i);

	printf(": address %s\n",
	    ether_sprintf(sc_if->arpcom.ac_enaddr));

	/*
	 * Set up RAM buffer addresses. The Yukon2 has a small amount
	 * of SRAM on it, somewhere between 4K and 48K.  We need to
	 * divide this up between the transmitter and receiver.  We
	 * give the receiver 2/3 of the memory (rounded down), and the
	 * transmitter whatever remains.
	 */
	chunk = (2 * (sc->sk_ramsize / sizeof(u_int64_t)) / 3) & ~0xff;
	sc_if->sk_rx_ramstart = 0;
	sc_if->sk_rx_ramend = sc_if->sk_rx_ramstart + chunk - 1;
	chunk = (sc->sk_ramsize / sizeof(u_int64_t)) - chunk;
	sc_if->sk_tx_ramstart = sc_if->sk_rx_ramend + 1;
	sc_if->sk_tx_ramend = sc_if->sk_tx_ramstart + chunk - 1;

	DPRINTFN(2, ("msk_attach: rx_ramstart=%#x rx_ramend=%#x\n"
		     "           tx_ramstart=%#x tx_ramend=%#x\n",
		     sc_if->sk_rx_ramstart, sc_if->sk_rx_ramend,
		     sc_if->sk_tx_ramstart, sc_if->sk_tx_ramend));

	/* Allocate the descriptor queues. */
	if (bus_dmamem_alloc(sc->sc_dmatag, sizeof(struct msk_ring_data),
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf(": can't alloc rx buffers\n");
		goto fail;
	}
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg,
	    sizeof(struct msk_ring_data), &kva, BUS_DMA_NOWAIT)) {
		printf(": can't map dma buffers (%lu bytes)\n",
		       (ulong)sizeof(struct msk_ring_data));
		goto fail_1;
	}
	if (bus_dmamap_create(sc->sc_dmatag, sizeof(struct msk_ring_data), 1,
	    sizeof(struct msk_ring_data), 0, BUS_DMA_NOWAIT,
            &sc_if->sk_ring_map)) {
		printf(": can't create dma map\n");
		goto fail_2;
	}
	if (bus_dmamap_load(sc->sc_dmatag, sc_if->sk_ring_map, kva,
	    sizeof(struct msk_ring_data), NULL, BUS_DMA_NOWAIT)) {
		printf(": can't load dma map\n");
		goto fail_3;
	}
        sc_if->sk_rdata = (struct msk_ring_data *)kva;
	bzero(sc_if->sk_rdata, sizeof(struct msk_ring_data));

	/* Try to allocate memory for jumbo buffers. */
	if (msk_alloc_jumbo_mem(sc_if)) {
		printf(": jumbo buffer allocation failed\n");
		goto fail_3;
	}

	ifp = &sc_if->arpcom.ac_if;
	ifp->if_softc = sc_if;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = msk_ioctl;
	ifp->if_start = msk_start;
	ifp->if_watchdog = msk_watchdog;
	ifp->if_baudrate = 1000000000;
	if (sc->sk_type != SK_YUKON_FE)
		ifp->if_hardmtu = SK_JUMBO_MTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, MSK_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc_if->sk_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	ifp->if_capabilities = IFCAP_VLAN_MTU;

	msk_reset(sc_if);

	/*
	 * Do miibus setup.
	 */
	msk_init_yukon(sc_if);

 	DPRINTFN(2, ("msk_attach: 1\n"));

	sc_if->sk_mii.mii_ifp = ifp;
	sc_if->sk_mii.mii_readreg = msk_miibus_readreg;
	sc_if->sk_mii.mii_writereg = msk_miibus_writereg;
	sc_if->sk_mii.mii_statchg = msk_miibus_statchg;

	ifmedia_init(&sc_if->sk_mii.mii_media, 0,
	    msk_ifmedia_upd, msk_ifmedia_sts);
	mii_flags = MIIF_DOPAUSE;
	if (sc->sk_fibertype)
		mii_flags |= MIIF_HAVEFIBER;
	mii_attach(self, &sc_if->sk_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, mii_flags);
	if (LIST_FIRST(&sc_if->sk_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc_if->sk_dev.dv_xname);
		ifmedia_add(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_MANUAL,
			    0, NULL);
		ifmedia_set(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	} else
		ifmedia_set(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_AUTO);

	timeout_set(&sc_if->sk_tick_ch, msk_tick, sc_if);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	shutdownhook_establish(mskc_shutdown, sc);

	DPRINTFN(2, ("msk_attach: end\n"));
	return;

fail_3:
	bus_dmamap_destroy(sc->sc_dmatag, sc_if->sk_ring_map);
fail_2:
	bus_dmamem_unmap(sc->sc_dmatag, kva, sizeof(struct msk_ring_data));
fail_1:
	bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
fail:
	sc->sk_if[sa->skc_port] = NULL;
}

int
mskcprint(void *aux, const char *pnp)
{
	struct skc_attach_args *sa = aux;

	if (pnp)
		printf("sk port %c at %s",
		    (sa->skc_port == SK_PORT_A) ? 'A' : 'B', pnp);
	else
		printf(" port %c", (sa->skc_port == SK_PORT_A) ? 'A' : 'B');
	return (UNCONF);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
mskc_attach(struct device *parent, struct device *self, void *aux)
{
	struct sk_softc *sc = (struct sk_softc *)self;
	struct pci_attach_args *pa = aux;
	struct skc_attach_args skca;
	pci_chipset_tag_t pc = pa->pa_pc;
	pcireg_t command, memtype;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_size_t size;
	u_int8_t hw, pmd;
	char *revstr = NULL;
	caddr_t kva;
	bus_dma_segment_t seg;
	int rseg;

	DPRINTFN(2, ("begin mskc_attach\n"));

	/*
	 * Handle power management nonsense.
	 */
	command = pci_conf_read(pc, pa->pa_tag, SK_PCI_CAPID) & 0x000000FF;

	if (command == 0x01) {
		command = pci_conf_read(pc, pa->pa_tag, SK_PCI_PWRMGMTCTRL);
		if (command & SK_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(pc, pa->pa_tag, SK_PCI_LOIO);
			membase = pci_conf_read(pc, pa->pa_tag, SK_PCI_LOMEM);
			irq = pci_conf_read(pc, pa->pa_tag, SK_PCI_INTLINE);

			/* Reset the power state. */
			printf("%s chip is in D%d power mode "
			    "-- setting to D0\n", sc->sk_dev.dv_xname,
			    command & SK_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(pc, pa->pa_tag,
			    SK_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(pc, pa->pa_tag, SK_PCI_LOIO, iobase);
			pci_conf_write(pc, pa->pa_tag, SK_PCI_LOMEM, membase);
			pci_conf_write(pc, pa->pa_tag, SK_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */

	memtype = pci_mapreg_type(pc, pa->pa_tag, SK_PCI_LOMEM);
	switch (memtype) {
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_32BIT:
	case PCI_MAPREG_TYPE_MEM | PCI_MAPREG_MEM_TYPE_64BIT:
		if (pci_mapreg_map(pa, SK_PCI_LOMEM,
				   memtype, 0, &sc->sk_btag, &sc->sk_bhandle,
				   NULL, &size, 0) == 0)
			break;
	default:
		printf(": can't map mem space\n");
		return;
	}

	sc->sc_dmatag = pa->pa_dmat;

	sc->sk_type = sk_win_read_1(sc, SK_CHIPVER);
	sc->sk_rev = (sk_win_read_1(sc, SK_CONFIG) >> 4);

	/* bail out here if chip is not recognized */
	if (!(SK_IS_YUKON2(sc))) {
		printf(": unknown chip type: %d\n", sc->sk_type);
		goto fail_1;
	}
	DPRINTFN(2, ("mskc_attach: allocate interrupt\n"));

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail_1;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->sk_intrhand = pci_intr_establish(pc, ih, IPL_NET, msk_intr, sc,
	    self->dv_xname);
	if (sc->sk_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail_1;
	}

	if (bus_dmamem_alloc(sc->sc_dmatag,
	    MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc),
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf(": can't alloc status buffers\n");
		goto fail_2;
	}

	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg,
	    MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc),
	    &kva, BUS_DMA_NOWAIT)) {
		printf(": can't map dma buffers (%lu bytes)\n",
		    (ulong)(MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc)));
		goto fail_3;
	}
	if (bus_dmamap_create(sc->sc_dmatag,
	    MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc), 1,
	    MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc), 0,
	    BUS_DMA_NOWAIT, &sc->sk_status_map)) {
		printf(": can't create dma map\n");
		goto fail_4;
	}
	if (bus_dmamap_load(sc->sc_dmatag, sc->sk_status_map, kva,
	    MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc),
	    NULL, BUS_DMA_NOWAIT)) {
		printf(": can't load dma map\n");
		goto fail_5;
	}
	sc->sk_status_ring = (struct msk_status_desc *)kva;
	bzero(sc->sk_status_ring,
	    MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc));

	/* Reset the adapter. */
	mskc_reset(sc);

	sc->sk_ramsize = sk_win_read_1(sc, SK_EPROM0) * 4096;
	DPRINTFN(2, ("mskc_attach: ramsize=%dK\n", sc->sk_ramsize / 1024));

	pmd = sk_win_read_1(sc, SK_PMDTYPE);
	if (pmd == 'L' || pmd == 'S' || pmd == 'P')
		sc->sk_fibertype = 1;

	switch (sc->sk_type) {
	case SK_YUKON_XL:
		sc->sk_name = "Yukon-2 XL";
		break;
	case SK_YUKON_EC_U:
		sc->sk_name = "Yukon-2 EC Ultra";
		break;
	case SK_YUKON_EX:
		sc->sk_name = "Yukon-2 Extreme";
		break;
	case SK_YUKON_EC:
		sc->sk_name = "Yukon-2 EC";
		break;
	case SK_YUKON_FE:
		sc->sk_name = "Yukon-2 FE";
		break;
	default:
		sc->sk_name = "Yukon (Unknown)";
	}

	if (sc->sk_type == SK_YUKON_XL) {
		switch (sc->sk_rev) {
		case SK_YUKON_XL_REV_A0:
			revstr = "A0";
			break;
		case SK_YUKON_XL_REV_A1:
			revstr = "A1";
			break;
		case SK_YUKON_XL_REV_A2:
			revstr = "A2";
			break;
		case SK_YUKON_XL_REV_A3:
			revstr = "A3";
			break;
		default:
			;
		}
	}

	if (sc->sk_type == SK_YUKON_EC) {
		switch (sc->sk_rev) {
		case SK_YUKON_EC_REV_A1:
			revstr = "A1";
			break;
		case SK_YUKON_EC_REV_A2:
			revstr = "A2";
			break;
		case SK_YUKON_EC_REV_A3:
			revstr = "A3";
			break;
		default:
			;
		}
	}

	if (sc->sk_type == SK_YUKON_EC_U) {
		switch (sc->sk_rev) {
		case SK_YUKON_EC_U_REV_A0:
			revstr = "A0";
			break;
		case SK_YUKON_EC_U_REV_A1:
			revstr = "A1";
			break;
		default:
			;
		}
	}

	/* Announce the product name. */
	printf(", %s", sc->sk_name);
	if (revstr != NULL)
		printf(" rev. %s", revstr);
	printf(" (0x%x): %s\n", sc->sk_rev, intrstr);

	sc->sk_macs = 1;

	hw = sk_win_read_1(sc, SK_Y2_HWRES);
	if ((hw & SK_Y2_HWRES_LINK_MASK) == SK_Y2_HWRES_LINK_DUAL) {
		if ((sk_win_read_1(sc, SK_Y2_CLKGATE) &
		    SK_Y2_CLKGATE_LINK2_INACTIVE) == 0)
			sc->sk_macs++;
	}

	skca.skc_port = SK_PORT_A;
	skca.skc_type = sc->sk_type;
	skca.skc_rev = sc->sk_rev;
	(void)config_found(&sc->sk_dev, &skca, mskcprint);

	if (sc->sk_macs > 1) {
		skca.skc_port = SK_PORT_B;
		skca.skc_type = sc->sk_type;
		skca.skc_rev = sc->sk_rev;
		(void)config_found(&sc->sk_dev, &skca, mskcprint);
	}

	/* Turn on the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_ON);

	return;

fail_5:
	bus_dmamap_destroy(sc->sc_dmatag, sc->sk_status_map);
fail_4:
	bus_dmamem_unmap(sc->sc_dmatag, kva, 
	    MSK_STATUS_RING_CNT * sizeof(struct msk_status_desc));
fail_3:
	bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
fail_2:
	pci_intr_disestablish(pc, sc->sk_intrhand);
fail_1:
	bus_space_unmap(sc->sk_btag, sc->sk_bhandle, size);
}

int
msk_encap(struct sk_if_softc *sc_if, struct mbuf *m_head, u_int32_t *txidx)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct msk_tx_desc		*f = NULL;
	u_int32_t		frag, cur;
	int			i;
	struct sk_txmap_entry	*entry;
	bus_dmamap_t		txmap;

	DPRINTFN(2, ("msk_encap\n"));

	entry = SIMPLEQ_FIRST(&sc_if->sk_txmap_head);
	if (entry == NULL) {
		DPRINTFN(2, ("msk_encap: no txmap available\n"));
		return (ENOBUFS);
	}
	txmap = entry->dmamap;

	cur = frag = *txidx;

#ifdef MSK_DEBUG
	if (mskdebug >= 2)
		msk_dump_mbuf(m_head);
#endif

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	if (bus_dmamap_load_mbuf(sc->sc_dmatag, txmap, m_head,
	    BUS_DMA_NOWAIT)) {
		DPRINTFN(2, ("msk_encap: dmamap failed\n"));
		return (ENOBUFS);
	}

	if (txmap->dm_nsegs > (MSK_TX_RING_CNT - sc_if->sk_cdata.sk_tx_cnt - 2)) {
		DPRINTFN(2, ("msk_encap: too few descriptors free\n"));
		bus_dmamap_unload(sc->sc_dmatag, txmap);
		return (ENOBUFS);
	}

	DPRINTFN(2, ("msk_encap: dm_nsegs=%d\n", txmap->dm_nsegs));

	/* Sync the DMA map. */
	bus_dmamap_sync(sc->sc_dmatag, txmap, 0, txmap->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	for (i = 0; i < txmap->dm_nsegs; i++) {
		f = &sc_if->sk_rdata->sk_tx_ring[frag];
		f->sk_addr = htole32(txmap->dm_segs[i].ds_addr);
		f->sk_len = htole16(txmap->dm_segs[i].ds_len);
		f->sk_ctl = 0;
		if (i == 0)
			f->sk_opcode = SK_Y2_TXOPC_PACKET;
		else
			f->sk_opcode = SK_Y2_TXOPC_BUFFER | SK_Y2_TXOPC_OWN;
		cur = frag;
		SK_INC(frag, MSK_TX_RING_CNT);
	}

	sc_if->sk_cdata.sk_tx_chain[cur].sk_mbuf = m_head;
	SIMPLEQ_REMOVE_HEAD(&sc_if->sk_txmap_head, link);

	sc_if->sk_cdata.sk_tx_map[cur] = entry;
	sc_if->sk_rdata->sk_tx_ring[cur].sk_ctl |= SK_Y2_TXCTL_LASTFRAG;

	/* Sync descriptors before handing to chip */
	MSK_CDTXSYNC(sc_if, *txidx, txmap->dm_nsegs,
            BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc_if->sk_rdata->sk_tx_ring[*txidx].sk_opcode |= SK_Y2_TXOPC_OWN;

	/* Sync first descriptor to hand it off */
	MSK_CDTXSYNC(sc_if, *txidx, 1,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	sc_if->sk_cdata.sk_tx_cnt += txmap->dm_nsegs;

#ifdef MSK_DEBUG
	if (mskdebug >= 2) {
		struct msk_tx_desc *le;
		u_int32_t idx;
		for (idx = *txidx; idx != frag; SK_INC(idx, MSK_TX_RING_CNT)) {
			le = &sc_if->sk_rdata->sk_tx_ring[idx];
			msk_dump_txdesc(le, idx);
		}
	}
#endif

	*txidx = frag;

	DPRINTFN(2, ("msk_encap: completed successfully\n"));

	return (0);
}

void
msk_start(struct ifnet *ifp)
{
        struct sk_if_softc	*sc_if = ifp->if_softc;
        struct mbuf		*m_head = NULL;
        u_int32_t		idx = sc_if->sk_cdata.sk_tx_prod;
	int			pkts = 0;

	DPRINTFN(2, ("msk_start\n"));

	while (sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf == NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (msk_encap(sc_if, m_head, &idx)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* now we are committed to transmit the packet */
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		pkts++;

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head, BPF_DIRECTION_OUT);
#endif
	}
	if (pkts == 0)
		return;

	/* Transmit */
	if (idx != sc_if->sk_cdata.sk_tx_prod) {
		sc_if->sk_cdata.sk_tx_prod = idx;
		SK_IF_WRITE_2(sc_if, 1, SK_TXQA1_Y2_PREF_PUTIDX, idx);

		/* Set a timeout in case the chip goes out to lunch. */
		ifp->if_timer = 5;
	}
}

void
msk_watchdog(struct ifnet *ifp)
{
	struct sk_if_softc *sc_if = ifp->if_softc;

	/*
	 * Reclaim first as there is a possibility of losing Tx completion
	 * interrupts.
	 */
	msk_txeof(sc_if);
	if (sc_if->sk_cdata.sk_tx_cnt != 0) {
		printf("%s: watchdog timeout\n", sc_if->sk_dev.dv_xname);

		ifp->if_oerrors++;

		/* XXX Resets both ports; we shouldn't do that. */
		mskc_reset(sc_if->sk_softc);
		msk_reset(sc_if);
		msk_init(sc_if);
	}
}

void
mskc_shutdown(void *v)
{
	struct sk_softc		*sc = v;

	DPRINTFN(2, ("msk_shutdown\n"));

	/* Turn off the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_OFF);

	/*
	 * Reset the GEnesis controller. Doing this should also
	 * assert the resets on the attached XMAC(s).
	 */
	mskc_reset(sc);
}

__inline int
msk_rxvalid(struct sk_softc *sc, u_int32_t stat, u_int32_t len)
{
	if ((stat & (YU_RXSTAT_CRCERR | YU_RXSTAT_LONGERR |
	    YU_RXSTAT_MIIERR | YU_RXSTAT_BADFC | YU_RXSTAT_GOODFC |
	    YU_RXSTAT_JABBER)) != 0 ||
	    (stat & YU_RXSTAT_RXOK) != YU_RXSTAT_RXOK ||
	    YU_RXSTAT_BYTES(stat) != len)
		return (0);

	return (1);
}

void
msk_rxeof(struct sk_if_softc *sc_if, u_int16_t len, u_int32_t rxstat)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	struct mbuf		*m;
	struct sk_chain		*cur_rx;
	int			cur, total_len = len;
	bus_dmamap_t		dmamap;

	DPRINTFN(2, ("msk_rxeof\n"));

	cur = sc_if->sk_cdata.sk_rx_cons;
	SK_INC(sc_if->sk_cdata.sk_rx_cons, MSK_RX_RING_CNT);
	SK_INC(sc_if->sk_cdata.sk_rx_prod, MSK_RX_RING_CNT);

	/* Sync the descriptor */
	MSK_CDRXSYNC(sc_if, cur, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

	cur_rx = &sc_if->sk_cdata.sk_rx_chain[cur];
	dmamap = sc_if->sk_cdata.sk_rx_jumbo_map;

	bus_dmamap_sync(sc_if->sk_softc->sc_dmatag, dmamap, 0,
	    dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

	m = cur_rx->sk_mbuf;
	cur_rx->sk_mbuf = NULL;

	if (total_len < SK_MIN_FRAMELEN ||
	    total_len > SK_JUMBO_FRAMELEN ||
	    msk_rxvalid(sc, rxstat, total_len) == 0) {
		ifp->if_ierrors++;
		msk_newbuf(sc_if, cur, m, dmamap);
		return;
	}

	/*
	 * Try to allocate a new jumbo buffer. If that fails, copy the
	 * packet to mbufs and put the jumbo buffer back in the ring
	 * so it can be re-used. If allocating mbufs fails, then we
	 * have to drop the packet.
	 */
	if (msk_newbuf(sc_if, cur, NULL, dmamap) == ENOBUFS) {
		struct mbuf		*m0;
		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    total_len + ETHER_ALIGN, 0, ifp, NULL);
		msk_newbuf(sc_if, cur, m, dmamap);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			return;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;
	} else {
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = total_len;
	}

	ifp->if_ipackets++;

#if NBPFILTER > 0
	if (ifp->if_bpf)
		bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif

	/* pass it on. */
	ether_input_mbuf(ifp, m);
}

void
msk_txeof(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct msk_tx_desc	*cur_tx;
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	u_int32_t		idx, reg, sk_ctl;
	struct sk_txmap_entry	*entry;

	DPRINTFN(2, ("msk_txeof\n"));

	if (sc_if->sk_port == SK_PORT_A)
		reg = SK_STAT_BMU_TXA1_RIDX;
	else
		reg = SK_STAT_BMU_TXA2_RIDX;

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	idx = sc_if->sk_cdata.sk_tx_cons;
	while (idx != sk_win_read_2(sc, reg)) {
		MSK_CDTXSYNC(sc_if, idx, 1,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		cur_tx = &sc_if->sk_rdata->sk_tx_ring[idx];
		sk_ctl = cur_tx->sk_ctl;
#ifdef MSK_DEBUG
		if (mskdebug >= 2)
			msk_dump_txdesc(cur_tx, idx);
#endif
		if (sk_ctl & SK_Y2_TXCTL_LASTFRAG)
			ifp->if_opackets++;
		if (sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf != NULL) {
			entry = sc_if->sk_cdata.sk_tx_map[idx];

			m_freem(sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf = NULL;

			bus_dmamap_sync(sc->sc_dmatag, entry->dmamap, 0,
			    entry->dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);

			bus_dmamap_unload(sc->sc_dmatag, entry->dmamap);
			SIMPLEQ_INSERT_TAIL(&sc_if->sk_txmap_head, entry,
					  link);
			sc_if->sk_cdata.sk_tx_map[idx] = NULL;
		}
		sc_if->sk_cdata.sk_tx_cnt--;
		SK_INC(idx, MSK_TX_RING_CNT);
	}
	ifp->if_timer = sc_if->sk_cdata.sk_tx_cnt > 0 ? 5 : 0;

	if (sc_if->sk_cdata.sk_tx_cnt < MSK_TX_RING_CNT - 2)
		ifp->if_flags &= ~IFF_OACTIVE;

	sc_if->sk_cdata.sk_tx_cons = idx;
}

void
msk_tick(void *xsc_if)
{
	struct sk_if_softc *sc_if = xsc_if;  
	struct mii_data *mii = &sc_if->sk_mii;
	int s;

	s = splnet();
	mii_tick(mii);
	splx(s);
	timeout_add(&sc_if->sk_tick_ch, hz);
}

void
msk_intr_yukon(struct sk_if_softc *sc_if)
{
	u_int8_t status;

	status = SK_IF_READ_1(sc_if, 0, SK_GMAC_ISR);
	/* RX overrun */
	if ((status & SK_GMAC_INT_RX_OVER) != 0) {
		SK_IF_WRITE_1(sc_if, 0, SK_RXMF1_CTRL_TEST,
		    SK_RFCTL_RX_FIFO_OVER);
	}
	/* TX underrun */
	if ((status & SK_GMAC_INT_TX_UNDER) != 0) {
		SK_IF_WRITE_1(sc_if, 0, SK_TXMF1_CTRL_TEST,
		    SK_TFCTL_TX_FIFO_UNDER);
	}

	DPRINTFN(2, ("msk_intr_yukon status=%#x\n", status));
}

int
msk_intr(void *xsc)
{
	struct sk_softc		*sc = xsc;
	struct sk_if_softc	*sc_if0 = sc->sk_if[SK_PORT_A];
	struct sk_if_softc	*sc_if1 = sc->sk_if[SK_PORT_B];
	struct ifnet		*ifp0 = NULL, *ifp1 = NULL;
	int			claimed = 0;
	u_int32_t		status;
	struct msk_status_desc	*cur_st;

	status = CSR_READ_4(sc, SK_Y2_ISSR2);
	if (status == 0) {
		CSR_WRITE_4(sc, SK_Y2_ICR, 2);
		return (0);
	}

	status = CSR_READ_4(sc, SK_ISR);

	if (sc_if0 != NULL)
		ifp0 = &sc_if0->arpcom.ac_if;
	if (sc_if1 != NULL)
		ifp1 = &sc_if1->arpcom.ac_if;

	if (sc_if0 && (status & SK_Y2_IMR_MAC1) &&
	    (ifp0->if_flags & IFF_RUNNING)) {
		msk_intr_yukon(sc_if0);
	}

	if (sc_if1 && (status & SK_Y2_IMR_MAC2) &&
	    (ifp1->if_flags & IFF_RUNNING)) {
		msk_intr_yukon(sc_if1);
	}

	MSK_CDSTSYNC(sc, sc->sk_status_idx,
	    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	cur_st = &sc->sk_status_ring[sc->sk_status_idx];

	while (cur_st->sk_opcode & SK_Y2_STOPC_OWN) {
		cur_st->sk_opcode &= ~SK_Y2_STOPC_OWN;
		switch (cur_st->sk_opcode) {
		case SK_Y2_STOPC_RXSTAT:
			msk_rxeof(sc->sk_if[cur_st->sk_link],
			    letoh16(cur_st->sk_len),
			    letoh32(cur_st->sk_status));
			SK_IF_WRITE_2(sc->sk_if[cur_st->sk_link], 0,
			    SK_RXQ1_Y2_PREF_PUTIDX,
			    sc->sk_if[cur_st->sk_link]->sk_cdata.sk_rx_prod);
			break;
		case SK_Y2_STOPC_TXSTAT:
			if (sc_if0)
				msk_txeof(sc_if0);
			if (sc_if1)
				msk_txeof(sc_if1);
			break;
		default:
			printf("opcode=0x%x\n", cur_st->sk_opcode);
			break;
		}
		SK_INC(sc->sk_status_idx, MSK_STATUS_RING_CNT);

		MSK_CDSTSYNC(sc, sc->sk_status_idx,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		cur_st = &sc->sk_status_ring[sc->sk_status_idx];
	}

	if (status & SK_Y2_IMR_BMU) {
		CSR_WRITE_4(sc, SK_STAT_BMU_CSR, SK_STAT_BMU_IRQ_CLEAR);
		claimed = 1;
	}

	CSR_WRITE_4(sc, SK_Y2_ICR, 2);

	if (ifp0 != NULL && !IFQ_IS_EMPTY(&ifp0->if_snd))
		msk_start(ifp0);
	if (ifp1 != NULL && !IFQ_IS_EMPTY(&ifp1->if_snd))
		msk_start(ifp1);

	return (claimed);
}

void
msk_init_yukon(struct sk_if_softc *sc_if)
{
	u_int32_t		v;
	u_int16_t		reg;
	struct sk_softc		*sc;
	int			i;

	sc = sc_if->sk_softc;

	DPRINTFN(2, ("msk_init_yukon: start: sk_csr=%#x\n",
		     CSR_READ_4(sc_if->sk_softc, SK_CSR)));

	DPRINTFN(6, ("msk_init_yukon: 1\n"));

	DPRINTFN(3, ("msk_init_yukon: gmac_ctrl=%#x\n",
		     SK_IF_READ_4(sc_if, 0, SK_GMAC_CTRL)));

	DPRINTFN(6, ("msk_init_yukon: 3\n"));

	/* unused read of the interrupt source register */
	DPRINTFN(6, ("msk_init_yukon: 4\n"));
	SK_IF_READ_2(sc_if, 0, SK_GMAC_ISR);

	DPRINTFN(6, ("msk_init_yukon: 4a\n"));
	reg = SK_YU_READ_2(sc_if, YUKON_PAR);
	DPRINTFN(6, ("msk_init_yukon: YUKON_PAR=%#x\n", reg));

	/* MIB Counter Clear Mode set */
        reg |= YU_PAR_MIB_CLR;
	DPRINTFN(6, ("msk_init_yukon: YUKON_PAR=%#x\n", reg));
	DPRINTFN(6, ("msk_init_yukon: 4b\n"));
	SK_YU_WRITE_2(sc_if, YUKON_PAR, reg);

	/* MIB Counter Clear Mode clear */
	DPRINTFN(6, ("msk_init_yukon: 5\n"));
        reg &= ~YU_PAR_MIB_CLR;
	SK_YU_WRITE_2(sc_if, YUKON_PAR, reg);

	/* receive control reg */
	DPRINTFN(6, ("msk_init_yukon: 7\n"));
	SK_YU_WRITE_2(sc_if, YUKON_RCR, YU_RCR_CRCR);

	/* transmit parameter register */
	DPRINTFN(6, ("msk_init_yukon: 8\n"));
	SK_YU_WRITE_2(sc_if, YUKON_TPR, YU_TPR_JAM_LEN(0x3) |
		      YU_TPR_JAM_IPG(0xb) | YU_TPR_JAM2DATA_IPG(0x1a) );

	/* serial mode register */
	DPRINTFN(6, ("msk_init_yukon: 9\n"));
	reg = YU_SMR_DATA_BLIND(0x1c) |
	      YU_SMR_MFL_VLAN |
	      YU_SMR_IPG_DATA(0x1e);

	if (sc->sk_type != SK_YUKON_FE)
		reg |= YU_SMR_MFL_JUMBO;

	SK_YU_WRITE_2(sc_if, YUKON_SMR, reg);

	DPRINTFN(6, ("msk_init_yukon: 10\n"));
	/* Setup Yukon's address */
	for (i = 0; i < 3; i++) {
		/* Write Source Address 1 (unicast filter) */
		SK_YU_WRITE_2(sc_if, YUKON_SAL1 + i * 4, 
			      sc_if->arpcom.ac_enaddr[i * 2] |
			      sc_if->arpcom.ac_enaddr[i * 2 + 1] << 8);
	}

	for (i = 0; i < 3; i++) {
		reg = sk_win_read_2(sc_if->sk_softc,
				    SK_MAC1_0 + i * 2 + sc_if->sk_port * 8);
		SK_YU_WRITE_2(sc_if, YUKON_SAL2 + i * 4, reg);
	}

	/* Set promiscuous mode */
	msk_setpromisc(sc_if);

	/* Set multicast filter */
	DPRINTFN(6, ("msk_init_yukon: 11\n"));
	msk_setmulti(sc_if);

	/* enable interrupt mask for counter overflows */
	DPRINTFN(6, ("msk_init_yukon: 12\n"));
	SK_YU_WRITE_2(sc_if, YUKON_TIMR, 0);
	SK_YU_WRITE_2(sc_if, YUKON_RIMR, 0);
	SK_YU_WRITE_2(sc_if, YUKON_TRIMR, 0);

	/* Configure RX MAC FIFO Flush Mask */
	v = YU_RXSTAT_FOFL | YU_RXSTAT_CRCERR | YU_RXSTAT_MIIERR |
	    YU_RXSTAT_BADFC | YU_RXSTAT_GOODFC | YU_RXSTAT_RUNT |
	    YU_RXSTAT_JABBER;
	SK_IF_WRITE_2(sc_if, 0, SK_RXMF1_FLUSH_MASK, v);

	/* Configure RX MAC FIFO */
	SK_IF_WRITE_1(sc_if, 0, SK_RXMF1_CTRL_TEST, SK_RFCTL_RESET_CLEAR);
	SK_IF_WRITE_2(sc_if, 0, SK_RXMF1_CTRL_TEST, SK_RFCTL_OPERATION_ON |
	    SK_RFCTL_FIFO_FLUSH_ON);

	/* Increase flush threshould to 64 bytes */
	SK_IF_WRITE_2(sc_if, 0, SK_RXMF1_FLUSH_THRESHOLD,
	    SK_RFCTL_FIFO_THRESHOLD + 1);

	/* Configure TX MAC FIFO */
	SK_IF_WRITE_1(sc_if, 0, SK_TXMF1_CTRL_TEST, SK_TFCTL_RESET_CLEAR);
	SK_IF_WRITE_2(sc_if, 0, SK_TXMF1_CTRL_TEST, SK_TFCTL_OPERATION_ON);

#if 1
	SK_YU_WRITE_2(sc_if, YUKON_GPCR, YU_GPCR_TXEN | YU_GPCR_RXEN);
#endif
	DPRINTFN(6, ("msk_init_yukon: end\n"));
}

/*
 * Note that to properly initialize any part of the GEnesis chip,
 * you first have to take it out of reset mode.
 */
void
msk_init(void *xsc_if)
{
	struct sk_if_softc	*sc_if = xsc_if;
	struct sk_softc		*sc = sc_if->sk_softc;
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	struct mii_data		*mii = &sc_if->sk_mii;
	int			s;

	DPRINTFN(2, ("msk_init\n"));

	s = splnet();

	/* Cancel pending I/O and free all RX/TX buffers. */
	msk_stop(sc_if);

	/* Configure I2C registers */

	/* Configure XMAC(s) */
	msk_init_yukon(sc_if);
	mii_mediachg(mii);

	/* Configure transmit arbiter(s) */
	SK_IF_WRITE_1(sc_if, 0, SK_TXAR1_COUNTERCTL, SK_TXARCTL_ON);
#if 0
	    SK_TXARCTL_ON|SK_TXARCTL_FSYNC_ON);
#endif

	/* Configure RAMbuffers */
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_UNRESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_START, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_WR_PTR, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_RD_PTR, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_END, sc_if->sk_rx_ramend);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_ON);

	SK_IF_WRITE_4(sc_if, 1, SK_TXRBA1_CTLTST, SK_RBCTL_UNRESET);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBA1_CTLTST, SK_RBCTL_STORENFWD_ON);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBA1_START, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBA1_WR_PTR, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBA1_RD_PTR, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBA1_END, sc_if->sk_tx_ramend);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBA1_CTLTST, SK_RBCTL_ON);

	/* Configure BMUs */
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, 0x00000016);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, 0x00000d28);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, 0x00000080);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_WATERMARK, 0x00000600);

	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_BMU_CSR, 0x00000016);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_BMU_CSR, 0x00000d28);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_BMU_CSR, 0x00000080);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_WATERMARK, 0x00000600);

	/* Make sure the sync transmit queue is disabled. */
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_RESET);

	/* Init descriptors */
	if (msk_init_rx_ring(sc_if) == ENOBUFS) {
		printf("%s: initialization failed: no "
		    "memory for rx buffers\n", sc_if->sk_dev.dv_xname);
		msk_stop(sc_if);
		splx(s);
		return;
	}

	if (msk_init_tx_ring(sc_if) == ENOBUFS) {
		printf("%s: initialization failed: no "
		    "memory for tx buffers\n", sc_if->sk_dev.dv_xname);
		msk_stop(sc_if);
		splx(s);
		return;
	}

	/* Initialize prefetch engine. */
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_Y2_PREF_CSR, 0x00000001);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_Y2_PREF_CSR, 0x00000002);
	SK_IF_WRITE_2(sc_if, 0, SK_RXQ1_Y2_PREF_LIDX, MSK_RX_RING_CNT - 1);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_Y2_PREF_ADDRLO,
	    MSK_RX_RING_ADDR(sc_if, 0));
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_Y2_PREF_ADDRHI,
	    (u_int64_t)MSK_RX_RING_ADDR(sc_if, 0) >> 32);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_Y2_PREF_CSR, 0x00000008);
	SK_IF_READ_4(sc_if, 0, SK_RXQ1_Y2_PREF_CSR);

	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_Y2_PREF_CSR, 0x00000001);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_Y2_PREF_CSR, 0x00000002);
	SK_IF_WRITE_2(sc_if, 1, SK_TXQA1_Y2_PREF_LIDX, MSK_TX_RING_CNT - 1);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_Y2_PREF_ADDRLO,
	    MSK_TX_RING_ADDR(sc_if, 0));
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_Y2_PREF_ADDRHI,
	    (u_int64_t)MSK_TX_RING_ADDR(sc_if, 0) >> 32);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_Y2_PREF_CSR, 0x00000008);
	SK_IF_READ_4(sc_if, 1, SK_TXQA1_Y2_PREF_CSR);

	SK_IF_WRITE_2(sc_if, 0, SK_RXQ1_Y2_PREF_PUTIDX,
	    sc_if->sk_cdata.sk_rx_prod);

	/* Configure interrupt handling */
	if (sc_if->sk_port == SK_PORT_A)
		sc->sk_intrmask |= SK_Y2_INTRS1;
	else
		sc->sk_intrmask |= SK_Y2_INTRS2;
	sc->sk_intrmask |= SK_Y2_IMR_BMU;
	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	timeout_add(&sc_if->sk_tick_ch, hz);

	splx(s);
}

void
msk_stop(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	struct sk_txmap_entry	*dma;
	int			i;

	DPRINTFN(2, ("msk_stop\n"));

	timeout_del(&sc_if->sk_tick_ch);

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	/* Stop transfer of Tx descriptors */

	/* Stop transfer of Rx descriptors */

	/* Turn off various components of this interface. */
	SK_XM_SETBIT_2(sc_if, XM_GPIO, XM_GPIO_RESETMAC);
	SK_IF_WRITE_1(sc_if,0, SK_RXMF1_CTRL_TEST, SK_RFCTL_RESET_SET);
	SK_IF_WRITE_1(sc_if,0, SK_TXMF1_CTRL_TEST, SK_TFCTL_RESET_SET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_OFFLINE);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_RESET|SK_RBCTL_OFF);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_BMU_CSR, SK_TXBMU_OFFLINE);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBA1_CTLTST, SK_RBCTL_RESET|SK_RBCTL_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_TXAR1_COUNTERCTL, SK_TXARCTL_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_RXLED1_CTL, SK_RXLEDCTL_COUNTER_STOP);
	SK_IF_WRITE_1(sc_if, 0, SK_TXLED1_CTL, SK_TXLEDCTL_COUNTER_STOP);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_LINKSYNC_OFF);

	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_Y2_PREF_CSR, 0x00000001);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQA1_Y2_PREF_CSR, 0x00000001);

	/* Disable interrupts */
	if (sc_if->sk_port == SK_PORT_A)
		sc->sk_intrmask &= ~SK_Y2_INTRS1;
	else
		sc->sk_intrmask &= ~SK_Y2_INTRS2;
	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	SK_XM_READ_2(sc_if, XM_ISR);
	SK_XM_WRITE_2(sc_if, XM_IMR, 0xFFFF);

	/* Free RX and TX mbufs still in the queues. */
	for (i = 0; i < MSK_RX_RING_CNT; i++) {
		if (sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf);
			sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf = NULL;
		}
	}

	for (i = 0; i < MSK_TX_RING_CNT; i++) {
		if (sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf = NULL;
			SIMPLEQ_INSERT_HEAD(&sc_if->sk_txmap_head,
			    sc_if->sk_cdata.sk_tx_map[i], link);
			sc_if->sk_cdata.sk_tx_map[i] = 0;
		}
	}

	while ((dma = SIMPLEQ_FIRST(&sc_if->sk_txmap_head))) {
		SIMPLEQ_REMOVE_HEAD(&sc_if->sk_txmap_head, link);
		bus_dmamap_destroy(sc->sc_dmatag, dma->dmamap);
		free(dma, M_DEVBUF);
	}
}

struct cfattach mskc_ca = {
	sizeof(struct sk_softc), mskc_probe, mskc_attach,
};

struct cfdriver mskc_cd = {
	0, "mskc", DV_DULL
};

struct cfattach msk_ca = {
	sizeof(struct sk_if_softc), msk_probe, msk_attach,
};

struct cfdriver msk_cd = {
	0, "msk", DV_IFNET
};

#ifdef MSK_DEBUG
void
msk_dump_txdesc(struct msk_tx_desc *le, int idx)
{
#define DESC_PRINT(X)					\
	if (X)					\
		printf("txdesc[%d]." #X "=%#x\n",	\
		       idx, X);

	DESC_PRINT(letoh32(le->sk_addr));
	DESC_PRINT(letoh16(le->sk_len));
	DESC_PRINT(le->sk_ctl);
	DESC_PRINT(le->sk_opcode);
#undef DESC_PRINT
}

void
msk_dump_bytes(const char *data, int len)
{
	int c, i, j;

	for (i = 0; i < len; i += 16) {
		printf("%08x  ", i);
		c = len - i;
		if (c > 16) c = 16;

		for (j = 0; j < c; j++) {
			printf("%02x ", data[i + j] & 0xff);
			if ((j & 0xf) == 7 && j > 0)
				printf(" ");
		}
		
		for (; j < 16; j++)
			printf("   ");
		printf("  ");

		for (j = 0; j < c; j++) {
			int ch = data[i + j] & 0xff;
			printf("%c", ' ' <= ch && ch <= '~' ? ch : ' ');
		}
		
		printf("\n");
		
		if (c < 16)
			break;
	}
}

void
msk_dump_mbuf(struct mbuf *m)
{
	int count = m->m_pkthdr.len;

	printf("m=%#lx, m->m_pkthdr.len=%#d\n", m, m->m_pkthdr.len);

	while (count > 0 && m) {
		printf("m=%#lx, m->m_data=%#lx, m->m_len=%d\n",
		       m, m->m_data, m->m_len);
		msk_dump_bytes(mtod(m, char *), m->m_len);

		count -= m->m_len;
		m = m->m_next;
	}
}
#endif
