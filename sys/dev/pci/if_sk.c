/*	$OpenBSD: if_sk.c,v 1.26 2002/11/26 06:01:28 nate Exp $	*/

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
#include <netinet/if_ether.h>
#endif

#include <net/if_media.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <uvm/uvm_extern.h>              /* for vtophys */
#include <machine/bus.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/mii/brgphyreg.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#define SK_USEIOSPACE
#define	SK_VERBOSE

#include <dev/pci/if_skreg.h>
#include <dev/pci/xmaciireg.h>

int skc_probe(struct device *, void *, void *);
void skc_attach(struct device *, struct device *self, void *aux);
int sk_probe(struct device *, void *, void *);
void sk_attach(struct device *, struct device *self, void *aux);
int skcprint(void *, const char *);
int sk_attach_xmac(struct sk_softc *, int);
int sk_intr(void *);
void sk_intr_bcom(struct sk_if_softc *);
void sk_intr_xmac(struct sk_if_softc *);
void sk_rxeof(struct sk_if_softc *);
void sk_txeof(struct sk_if_softc *);
int sk_encap(struct sk_if_softc *, struct mbuf *, u_int32_t *);
void sk_start(struct ifnet *);
int sk_ioctl(struct ifnet *, u_long, caddr_t);
void sk_init(void *);
void sk_init_xmac(struct sk_if_softc *);
void sk_stop(struct sk_if_softc *);
void sk_watchdog(struct ifnet *);
void sk_shutdown(void *);
int sk_ifmedia_upd(struct ifnet *);
void sk_ifmedia_sts(struct ifnet *, struct ifmediareq *);
void sk_reset(struct sk_softc *);
int sk_newbuf(struct sk_if_softc *, struct sk_chain *,
    struct mbuf *);
int sk_init_rx_ring(struct sk_if_softc *);
void sk_init_tx_ring(struct sk_if_softc *);
u_int32_t sk_win_read_4(struct sk_softc *, int);
u_int16_t sk_win_read_2(struct sk_softc *, int);
u_int8_t sk_win_read_1(struct sk_softc *, int);
void sk_win_write_4(struct sk_softc *, int, u_int32_t);
void sk_win_write_2(struct sk_softc *, int, u_int32_t);
void sk_win_write_1(struct sk_softc *, int, u_int32_t);
u_int8_t sk_vpd_readbyte(struct sk_softc *, int);
void sk_vpd_read_res(struct sk_softc *,
					struct vpd_res *, int);
void sk_vpd_read(struct sk_softc *);

int sk_miibus_readreg(struct device *, int, int);
void sk_miibus_writereg(struct device *, int, int, int);
void sk_miibus_statchg(struct device *);

u_int32_t sk_calchash(caddr_t);
void sk_setfilt(struct sk_if_softc *, caddr_t, int);
void sk_setmulti(struct sk_if_softc *);
void sk_tick(void *);

#define SK_SETBIT(sc, reg, x)		\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) | x)

#define SK_CLRBIT(sc, reg, x)		\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) & ~x)

#define SK_WIN_SETBIT_4(sc, reg, x)	\
	sk_win_write_4(sc, reg, sk_win_read_4(sc, reg) | x)

#define SK_WIN_CLRBIT_4(sc, reg, x)	\
	sk_win_write_4(sc, reg, sk_win_read_4(sc, reg) & ~x)

#define SK_WIN_SETBIT_2(sc, reg, x)	\
	sk_win_write_2(sc, reg, sk_win_read_2(sc, reg) | x)

#define SK_WIN_CLRBIT_2(sc, reg, x)	\
	sk_win_write_2(sc, reg, sk_win_read_2(sc, reg) & ~x)

u_int32_t sk_win_read_4(sc, reg)
	struct sk_softc		*sc;
	int			reg;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return(CSR_READ_4(sc, SK_WIN_BASE + SK_REG(reg)));
}

u_int16_t sk_win_read_2(sc, reg)
	struct sk_softc		*sc;
	int			reg;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return(CSR_READ_2(sc, SK_WIN_BASE + SK_REG(reg)));
}

u_int8_t sk_win_read_1(sc, reg)
	struct sk_softc		*sc;
	int			reg;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return(CSR_READ_1(sc, SK_WIN_BASE + SK_REG(reg)));
}

void sk_win_write_4(sc, reg, val)
	struct sk_softc		*sc;
	int			reg;
	u_int32_t		val;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_4(sc, SK_WIN_BASE + SK_REG(reg), val);
	return;
}

void sk_win_write_2(sc, reg, val)
	struct sk_softc		*sc;
	int			reg;
	u_int32_t		val;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_2(sc, SK_WIN_BASE + SK_REG(reg), (u_int32_t)val);
	return;
}

void sk_win_write_1(sc, reg, val)
	struct sk_softc		*sc;
	int			reg;
	u_int32_t		val;
{
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_1(sc, SK_WIN_BASE + SK_REG(reg), val);
	return;
}

/*
 * The VPD EEPROM contains Vital Product Data, as suggested in
 * the PCI 2.1 specification. The VPD data is separared into areas
 * denoted by resource IDs. The SysKonnect VPD contains an ID string
 * resource (the name of the adapter), a read-only area resource
 * containing various key/data fields and a read/write area which
 * can be used to store asset management information or log messages.
 * We read the ID string and read-only into buffers attached to
 * the controller softc structure for later use. At the moment,
 * we only use the ID string during sk_attach().
 */
u_int8_t sk_vpd_readbyte(sc, addr)
	struct sk_softc		*sc;
	int			addr;
{
	int			i;

	sk_win_write_2(sc, SK_PCI_REG(SK_PCI_VPD_ADDR), addr);
	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		if (sk_win_read_2(sc,
		    SK_PCI_REG(SK_PCI_VPD_ADDR)) & SK_VPD_FLAG)
			break;
	}

	if (i == SK_TIMEOUT)
		return(0);

	return(sk_win_read_1(sc, SK_PCI_REG(SK_PCI_VPD_DATA)));
}

void sk_vpd_read_res(sc, res, addr)
	struct sk_softc		*sc;
	struct vpd_res		*res;
	int			addr;
{
	int			i;
	u_int8_t		*ptr;

	ptr = (u_int8_t *)res;
	for (i = 0; i < sizeof(struct vpd_res); i++)
		ptr[i] = sk_vpd_readbyte(sc, i + addr);

	return;
}

void sk_vpd_read(sc)
	struct sk_softc		*sc;
{
	int			pos = 0, i;
	struct vpd_res		res;

	if (sc->sk_vpd_prodname != NULL)
		free(sc->sk_vpd_prodname, M_DEVBUF);
	if (sc->sk_vpd_readonly != NULL)
		free(sc->sk_vpd_readonly, M_DEVBUF);
	sc->sk_vpd_prodname = NULL;
	sc->sk_vpd_readonly = NULL;

	sk_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_ID) {
		printf("%s: bad VPD resource id: expected %x got %x\n",
		    sc->sk_dev.dv_xname, VPD_RES_ID, res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->sk_vpd_prodname = malloc(res.vr_len + 1, M_DEVBUF, M_NOWAIT);
	if (sc->sk_vpd_prodname == NULL)
		panic("sk_vpd_read");
	for (i = 0; i < res.vr_len; i++)
		sc->sk_vpd_prodname[i] = sk_vpd_readbyte(sc, i + pos);
	sc->sk_vpd_prodname[i] = '\0';
	pos += i;

	sk_vpd_read_res(sc, &res, pos);

	if (res.vr_id != VPD_RES_READ) {
		printf("%s: bad VPD resource id: expected %x got %x\n",
		    sc->sk_dev.dv_xname, VPD_RES_READ, res.vr_id);
		return;
	}

	pos += sizeof(res);
	sc->sk_vpd_readonly = malloc(res.vr_len, M_DEVBUF, M_NOWAIT);
	if (sc->sk_vpd_readonly == NULL)
		panic("sk_vpd_read");
	for (i = 0; i < res.vr_len + 1; i++)
		sc->sk_vpd_readonly[i] = sk_vpd_readbyte(sc, i + pos);

	return;
}

int
sk_miibus_readreg(dev, phy, reg)
	struct device *dev;
	int phy, reg;
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	int i;

	if (sc_if->sk_phytype == SK_PHYTYPE_XMAC && phy != 0)
		return(0);

	SK_XM_WRITE_2(sc_if, XM_PHY_ADDR, reg|(phy << 8));
	SK_XM_READ_2(sc_if, XM_PHY_DATA);
	if (sc_if->sk_phytype != SK_PHYTYPE_XMAC) {
		for (i = 0; i < SK_TIMEOUT; i++) {
			DELAY(1);
			if (SK_XM_READ_2(sc_if, XM_MMUCMD) &
			    XM_MMUCMD_PHYDATARDY)
				break;
		}

		if (i == SK_TIMEOUT) {
			printf("%s: phy failed to come ready\n",
			    sc_if->sk_dev.dv_xname);
			return(0);
		}
	}
	DELAY(1);
	return(SK_XM_READ_2(sc_if, XM_PHY_DATA));
}

void
sk_miibus_writereg(dev, phy, reg, val)
	struct device *dev;
	int phy, reg, val;
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	int i;

	SK_XM_WRITE_2(sc_if, XM_PHY_ADDR, reg|(phy << 8));
	for (i = 0; i < SK_TIMEOUT; i++) {
		if (!(SK_XM_READ_2(sc_if, XM_MMUCMD) & XM_MMUCMD_PHYBUSY))
			break;
	}

	if (i == SK_TIMEOUT) {
		printf("%s: phy failed to come ready\n",
		    sc_if->sk_dev.dv_xname);
		return;
	}

	SK_XM_WRITE_2(sc_if, XM_PHY_DATA, val);
	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		if (!(SK_XM_READ_2(sc_if, XM_MMUCMD) & XM_MMUCMD_PHYBUSY))
			break;
	}

	if (i == SK_TIMEOUT)
		printf("%s: phy write timed out\n", sc_if->sk_dev.dv_xname);

	return;
}

void
sk_miibus_statchg(dev)
	struct device *dev;
{
	struct sk_if_softc *sc_if;
	struct mii_data *mii;

	sc_if = (struct sk_if_softc *)dev;
	mii = &sc_if->sk_mii;

	/*
	 * If this is a GMII PHY, manually set the XMAC's
	 * duplex mode accordingly.
	 */
	if (sc_if->sk_phytype != SK_PHYTYPE_XMAC) {
		if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
			SK_XM_SETBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_GMIIFDX);
		} else {
			SK_XM_CLRBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_GMIIFDX);
		}
	}

	return;
}

#define SK_POLY		0xEDB88320
#define SK_BITS		6

u_int32_t sk_calchash(addr)
	caddr_t			addr;
{
	u_int32_t		idx, bit, data, crc;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (idx = 0; idx < 6; idx++) {
		for (data = *addr++, bit = 0; bit < 8; bit++, data >>= 1)
			crc = (crc >> 1) ^ (((crc ^ data) & 1) ? SK_POLY : 0);
	}

	return (~crc & ((1 << SK_BITS) - 1));
}

void sk_setfilt(sc_if, addr, slot)
	struct sk_if_softc	*sc_if;
	caddr_t			addr;
	int			slot;
{
	int			base;

	base = XM_RXFILT_ENTRY(slot);

	SK_XM_WRITE_2(sc_if, base, *(u_int16_t *)(&addr[0]));
	SK_XM_WRITE_2(sc_if, base + 2, *(u_int16_t *)(&addr[2]));
	SK_XM_WRITE_2(sc_if, base + 4, *(u_int16_t *)(&addr[4]));

	return;
}

void
sk_setmulti(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct ifnet *ifp;
	u_int32_t hashes[2] = { 0, 0 };
	int h, i;
	struct arpcom *ac = &sc_if->arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int8_t dummy[] = { 0, 0, 0, 0, 0 ,0 };

	ifp = &sc_if->arpcom.ac_if;

	/* First, zot all the existing filters. */
	for (i = 1; i < XM_RXFILT_MAX; i++)
		sk_setfilt(sc_if, (caddr_t)&dummy, i);
	SK_XM_WRITE_4(sc_if, XM_MAR0, 0);
	SK_XM_WRITE_4(sc_if, XM_MAR2, 0);

	/* Now program new ones. */
allmulti:
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		hashes[0] = 0xFFFFFFFF;
		hashes[1] = 0xFFFFFFFF;
	} else {
		i = 1;
		/* First find the tail of the list. */
		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
				ifp->if_flags |= IFF_ALLMULTI;
				goto allmulti;
			}
			/*
			 * Program the first XM_RXFILT_MAX multicast groups
			 * into the perfect filter. For all others,
			 * use the hash table.
			 */
			if (i < XM_RXFILT_MAX) {
				sk_setfilt(sc_if, enm->enm_addrlo, i);
				i++;
			}
			else {
				h = sk_calchash(enm->enm_addrlo);
				if (h < 32)
					hashes[0] |= (1 << h);
				else
					hashes[1] |= (1 << (h - 32));
			}

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_USE_HASH|
	    XM_MODE_RX_USE_PERFECT);
	SK_XM_WRITE_4(sc_if, XM_MAR0, hashes[0]);
	SK_XM_WRITE_4(sc_if, XM_MAR2, hashes[1]);

	return;
}

int sk_init_rx_ring(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_chain_data	*cd;
	struct sk_ring_data	*rd;
	int			i;

	cd = &sc_if->sk_cdata;
	rd = sc_if->sk_rdata;

	bzero((char *)rd->sk_rx_ring,
	    sizeof(struct sk_rx_desc) * SK_RX_RING_CNT);

	for (i = 0; i < SK_RX_RING_CNT; i++) {
		cd->sk_rx_chain[i].sk_desc = &rd->sk_rx_ring[i];
		if (sk_newbuf(sc_if, &cd->sk_rx_chain[i], NULL) == ENOBUFS) {
			printf("%s: failed alloc of %dth mbuf\n",
			    sc_if->sk_dev.dv_xname, i);
			return(ENOBUFS);
		}
		if (i == (SK_RX_RING_CNT - 1)) {
			cd->sk_rx_chain[i].sk_next =
			    &cd->sk_rx_chain[0];
			rd->sk_rx_ring[i].sk_next = 
			    vtophys(&rd->sk_rx_ring[0]);
		} else {
			cd->sk_rx_chain[i].sk_next =
			    &cd->sk_rx_chain[i + 1];
			rd->sk_rx_ring[i].sk_next = 
			    vtophys(&rd->sk_rx_ring[i + 1]);
		}
	}

	sc_if->sk_cdata.sk_rx_prod = 0;
	sc_if->sk_cdata.sk_rx_cons = 0;

	return(0);
}

void sk_init_tx_ring(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_chain_data	*cd;
	struct sk_ring_data	*rd;
	int			i;

	cd = &sc_if->sk_cdata;
	rd = sc_if->sk_rdata;

	bzero((char *)sc_if->sk_rdata->sk_tx_ring,
	    sizeof(struct sk_tx_desc) * SK_TX_RING_CNT);

	for (i = 0; i < SK_TX_RING_CNT; i++) {
		cd->sk_tx_chain[i].sk_desc = &rd->sk_tx_ring[i];
		if (i == (SK_TX_RING_CNT - 1)) {
			cd->sk_tx_chain[i].sk_next =
			    &cd->sk_tx_chain[0];
			rd->sk_tx_ring[i].sk_next = 
			    vtophys(&rd->sk_tx_ring[0]);
		} else {
			cd->sk_tx_chain[i].sk_next =
			    &cd->sk_tx_chain[i + 1];
			rd->sk_tx_ring[i].sk_next = 
			    vtophys(&rd->sk_tx_ring[i + 1]);
		}
	}

	sc_if->sk_cdata.sk_tx_prod = 0;
	sc_if->sk_cdata.sk_tx_cons = 0;
	sc_if->sk_cdata.sk_tx_cnt = 0;

	return;
}

int sk_newbuf(sc_if, c, m)
	struct sk_if_softc	*sc_if;
	struct sk_chain		*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	struct sk_rx_desc	*r;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for rx list -- "
			    "packet dropped!\n", sc_if->sk_dev.dv_xname);
			return(ENOBUFS);
		}

		/* Allocate the jumbo buffer */
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			return (ENOBUFS);
		}
	} else {
		/*
	 	 * We're re-using a previously allocated mbuf;
		 * be sure to re-init pointers and lengths to
		 * default values.
		 */
		m_new = m;
		m_new->m_data = m_new->m_ext.ext_buf;
	}
	m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;

	/*
	 * Adjust alignment so packet payload begins on a
	 * longword boundary. Mandatory for Alpha, useful on
	 * x86 too.
	 */
	m_adj(m_new, ETHER_ALIGN);

	r = c->sk_desc;
	c->sk_mbuf = m_new;
	r->sk_data_lo = vtophys(mtod(m_new, caddr_t));
	r->sk_ctl = m_new->m_len | SK_RXSTAT;

	return(0);
}

/*
 * Set media options.
 */
int
sk_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct sk_if_softc *sc_if = ifp->if_softc;

	sk_init(sc_if);
	mii_mediachg(&sc_if->sk_mii);
	return(0);
}

/*
 * Report current media status.
 */
void
sk_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct sk_if_softc *sc_if = ifp->if_softc;

	mii_pollstat(&sc_if->sk_mii);
	ifmr->ifm_active = sc_if->sk_mii.mii_media_active;
	ifmr->ifm_status = sc_if->sk_mii.mii_media_status;
}

int
sk_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct sk_if_softc *sc_if = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *) data;
	struct mii_data *mii;
	int s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc_if->arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			sk_init(sc_if);
			arp_ifinit(&sc_if->arpcom, ifa);
			break;
#endif /* INET */
		default:
			sk_init(sc_if);
			break;
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > SK_JUMBO_MTU)
			error = EINVAL;
		else
			ifp->if_mtu = ifr->ifr_mtu;
		sk_init(sc_if);
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc_if->sk_if_flags & IFF_PROMISC)) {
				SK_XM_SETBIT_4(sc_if, XM_MODE,
				    XM_MODE_RX_PROMISC);
				sk_setmulti(sc_if);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc_if->sk_if_flags & IFF_PROMISC) {
				SK_XM_CLRBIT_4(sc_if, XM_MODE,
				    XM_MODE_RX_PROMISC);
				sk_setmulti(sc_if);
			} else
				sk_init(sc_if);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				sk_stop(sc_if);
		}
		sc_if->sk_if_flags = ifp->if_flags;
		error = 0;
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
			sk_setmulti(sc_if);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = &sc_if->sk_mii;
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return(error);
}

/*
 * Probe for a SysKonnect GEnesis chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
int
skc_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct pci_attach_args *pa = aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_SCHNEIDERKOCH &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_SCHNEIDERKOCH_GE)
		return (1);

	return (0);
}

/*
 * Force the GEnesis into reset, then bring it out of reset.
 */
void sk_reset(sc)
	struct sk_softc		*sc;
{
	CSR_WRITE_4(sc, SK_CSR, SK_CSR_SW_RESET);
	CSR_WRITE_4(sc, SK_CSR, SK_CSR_MASTER_RESET);
	DELAY(1000);
	CSR_WRITE_4(sc, SK_CSR, SK_CSR_SW_UNRESET);
	CSR_WRITE_4(sc, SK_CSR, SK_CSR_MASTER_UNRESET);

	/* Configure packet arbiter */
	sk_win_write_2(sc, SK_PKTARB_CTL, SK_PKTARBCTL_UNRESET);
	sk_win_write_2(sc, SK_RXPA1_TINIT, SK_PKTARB_TIMEOUT);
	sk_win_write_2(sc, SK_TXPA1_TINIT, SK_PKTARB_TIMEOUT);
	sk_win_write_2(sc, SK_RXPA2_TINIT, SK_PKTARB_TIMEOUT);
	sk_win_write_2(sc, SK_TXPA2_TINIT, SK_PKTARB_TIMEOUT);

	/* Enable RAM interface */
	sk_win_write_4(sc, SK_RAMCTL, SK_RAMCTL_UNRESET);

	/*
         * Configure interrupt moderation. The moderation timer
	 * defers interrupts specified in the interrupt moderation
	 * timer mask based on the timeout specified in the interrupt
	 * moderation timer init register. Each bit in the timer
	 * register represents 18.825ns, so to specify a timeout in
	 * microseconds, we have to multiply by 54.
	 */
        sk_win_write_4(sc, SK_IMTIMERINIT, SK_IM_USECS(200));
        sk_win_write_4(sc, SK_IMMR, SK_ISR_TX1_S_EOF|SK_ISR_TX2_S_EOF|
	    SK_ISR_RX1_EOF|SK_ISR_RX2_EOF);
        sk_win_write_1(sc, SK_IMTIMERCTL, SK_IMCTL_START);

	return;
}

int
sk_probe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct skc_attach_args *sa = aux;

	if (sa->skc_port != SK_PORT_A && sa->skc_port != SK_PORT_B)
		return(0);

	return (1);
}

/*
 * Each XMAC chip is attached as a separate logical IP interface.
 * Single port cards will have only one logical interface of course.
 */
void
sk_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *) self;
	struct sk_softc *sc = (struct sk_softc *)parent;
	struct skc_attach_args *sa = aux;
	struct ifnet *ifp;
	caddr_t kva;
	bus_dma_segment_t seg;
	bus_dmamap_t dmamap;
	int i, rseg;

	sc_if->sk_port = sa->skc_port;
	sc_if->sk_softc = sc;
	sc->sk_if[sa->skc_port] = sc_if;

	if (sa->skc_port == SK_PORT_A)
		sc_if->sk_tx_bmu = SK_BMU_TXS_CSR0;
	if (sa->skc_port == SK_PORT_B)
		sc_if->sk_tx_bmu = SK_BMU_TXS_CSR1;
	
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
	 * Set up RAM buffer addresses. The NIC will have a certain
	 * amount of SRAM on it, somewhere between 512K and 2MB. We
	 * need to divide this up a) between the transmitter and
 	 * receiver and b) between the two XMACs, if this is a
	 * dual port NIC. Our algotithm is to divide up the memory
	 * evenly so that everyone gets a fair share.
	 */
	if (sk_win_read_1(sc, SK_CONFIG) & SK_CONFIG_SINGLEMAC) {
		u_int32_t		chunk, val;

		chunk = sc->sk_ramsize / 2;
		val = sc->sk_rboff / sizeof(u_int64_t);
		sc_if->sk_rx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_rx_ramend = val - 1;
		sc_if->sk_tx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_tx_ramend = val - 1;
	} else {
		u_int32_t		chunk, val;

		chunk = sc->sk_ramsize / 4;
		val = (sc->sk_rboff + (chunk * 2 * sc_if->sk_port)) /
		    sizeof(u_int64_t);
		sc_if->sk_rx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_rx_ramend = val - 1;
		sc_if->sk_tx_ramstart = val;
		val += (chunk / sizeof(u_int64_t));
		sc_if->sk_tx_ramend = val - 1;
	}

	/* Read and save PHY type and set PHY address */
	sc_if->sk_phytype = sk_win_read_1(sc, SK_EPROM1) & 0xF;
	switch (sc_if->sk_phytype) {
	case SK_PHYTYPE_XMAC:
		sc_if->sk_phyaddr = SK_PHYADDR_XMAC;
		break;
	case SK_PHYTYPE_BCOM:
		sc_if->sk_phyaddr = SK_PHYADDR_BCOM;
		break;
	default:
		printf("%s: unsupported PHY type: %d\n",
		    sc->sk_dev.dv_xname, sc_if->sk_phytype);
		return;
	}

	/* Allocate the descriptor queues. */
	if (bus_dmamem_alloc(sc->sc_dmatag, sizeof(struct sk_ring_data),
	    PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: can't alloc rx buffers\n", sc->sk_dev.dv_xname);
		goto fail;
	}
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg,
	    sizeof(struct sk_ring_data), &kva, BUS_DMA_NOWAIT)) {
		printf("%s: can't map dma buffers (%d bytes)\n",
		       sc_if->sk_dev.dv_xname, sizeof(struct sk_ring_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
	if (bus_dmamap_create(sc->sc_dmatag, sizeof(struct sk_ring_data), 1,
	    sizeof(struct sk_ring_data), 0, BUS_DMA_NOWAIT, &dmamap)) {
		printf("%s: can't create dma map\n", sc_if->sk_dev.dv_xname);
		bus_dmamem_unmap(sc->sc_dmatag, kva,
		    sizeof(struct sk_ring_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
	if (bus_dmamap_load(sc->sc_dmatag, dmamap, kva,
	    sizeof(struct sk_ring_data), NULL, BUS_DMA_NOWAIT)) {
		printf("%s: can't load dma map\n", sc_if->sk_dev.dv_xname);
		bus_dmamap_destroy(sc->sc_dmatag, dmamap);
		bus_dmamem_unmap(sc->sc_dmatag, kva,
		    sizeof(struct sk_ring_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
        sc_if->sk_rdata = (struct sk_ring_data *)kva;
	bzero(sc_if->sk_rdata, sizeof(struct sk_ring_data));

	ifp = &sc_if->arpcom.ac_if;
	ifp->if_softc = sc_if;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sk_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = sk_start;
	ifp->if_watchdog = sk_watchdog;
	ifp->if_baudrate = 1000000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, SK_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc_if->sk_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/*
	 * Do miibus setup.
	 */
	sk_init_xmac(sc_if);
	sc_if->sk_mii.mii_ifp = ifp;
	sc_if->sk_mii.mii_readreg = sk_miibus_readreg;
	sc_if->sk_mii.mii_writereg = sk_miibus_writereg;
	sc_if->sk_mii.mii_statchg = sk_miibus_statchg;
	ifmedia_init(&sc_if->sk_mii.mii_media, 0,
	    sk_ifmedia_upd, sk_ifmedia_sts);
	mii_attach(self, &sc_if->sk_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc_if->sk_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc_if->sk_dev.dv_xname);
		ifmedia_add(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_MANUAL,
		    0, NULL);
		ifmedia_set(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_MANUAL);
	}
	else
		ifmedia_set(&sc_if->sk_mii.mii_media, IFM_ETHER|IFM_AUTO);

	timeout_set(&sc_if->sk_tick_ch, sk_tick, sc_if);
	timeout_add(&sc_if->sk_tick_ch, hz);

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	return;

fail:
	sc->sk_if[sa->skc_port] = NULL;
}

int
skcprint(aux, pnp)
	void *aux;
	const char *pnp;
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
skc_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sk_softc *sc = (struct sk_softc *)self;
	struct pci_attach_args *pa = aux;
	struct skc_attach_args skca;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	bus_addr_t iobase;
	bus_size_t iosize;
	int s;
	u_int32_t command;

	s = splimp();

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
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_IO_ENABLE |
	    PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

#ifdef SK_USEIOSPACE
	if (!(command & PCI_COMMAND_IO_ENABLE)) {
		printf(": failed to enable I/O ports!\n");
		goto fail;
	}
	/*
	 * Map control/status registers.
	 */
	if (pci_io_find(pc, pa->pa_tag, SK_PCI_LOIO, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
		goto fail;
	}
	if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &sc->sk_bhandle)) {
		printf(": can't map i/o space\n");
		goto fail;
	}
	sc->sk_btag = pa->pa_iot;
#else
	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping!\n");
		goto fail;
	}
	if (pci_mem_find(pc, pa->pa_tag, SK_PCI_LOMEM, &iobase, &iosize, NULL)){
		printf(": can't find mem space\n");
		goto fail;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sk_bhandle)) {
		printf(": can't map mem space\n");
		goto fail;
	}
	sc->sk_btag = pa->pa_memt;
#endif
	sc->sc_dmatag = pa->pa_dmat;

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->sk_intrhand = pci_intr_establish(pc, ih, IPL_NET, sk_intr, sc,
	    self->dv_xname);
	if (sc->sk_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		goto fail;
	}
	printf(": %s\n", intrstr);

	/* Reset the adapter. */
	sk_reset(sc);

	/* Read and save vital product data from EEPROM. */
	sk_vpd_read(sc);

	/* Read and save RAM size and RAMbuffer offset */
	switch(sk_win_read_1(sc, SK_EPROM0)) {
	case SK_RAMSIZE_512K_64:
		sc->sk_ramsize = 0x80000;
		sc->sk_rboff = SK_RBOFF_0;
		break;
	case SK_RAMSIZE_1024K_64:
		sc->sk_ramsize = 0x100000;
		sc->sk_rboff = SK_RBOFF_80000;
		break;
	case SK_RAMSIZE_1024K_128:
		sc->sk_ramsize = 0x100000;
		sc->sk_rboff = SK_RBOFF_0;
		break;
	case SK_RAMSIZE_2048K_128:
		sc->sk_ramsize = 0x200000;
		sc->sk_rboff = SK_RBOFF_0;
		break;
	default:
		printf("%s: unknown ram size: %d\n",
		    sc->sk_dev.dv_xname, sk_win_read_1(sc, SK_EPROM0));
		goto fail;
		break;
	}

	/* Read and save physical media type */
	switch(sk_win_read_1(sc, SK_PMDTYPE)) {
	case SK_PMD_1000BASESX:
		sc->sk_pmd = IFM_1000_SX;
		break;
	case SK_PMD_1000BASELX:
		sc->sk_pmd = IFM_1000_LX;
		break;
	case SK_PMD_1000BASECX:
		sc->sk_pmd = IFM_1000_CX;
		break;
	case SK_PMD_1000BASETX:
		sc->sk_pmd = IFM_1000_T;
		break;
	default:
		printf("%s: unknown media type: 0x%x\n",
		    sc->sk_dev.dv_xname, sk_win_read_1(sc, SK_PMDTYPE));
		goto fail;
	}

	/* Announce the product name. */
	printf("%s: %s\n", sc->sk_dev.dv_xname, sc->sk_vpd_prodname);

	skca.skc_port = SK_PORT_A;
	(void)config_found(&sc->sk_dev, &skca, skcprint);

	if (!(sk_win_read_1(sc, SK_CONFIG) & SK_CONFIG_SINGLEMAC)) {
		skca.skc_port = SK_PORT_B;
		(void)config_found(&sc->sk_dev, &skca, skcprint);
	}

	/* Turn on the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_ON);

fail:
	splx(s);
}

int sk_encap(sc_if, m_head, txidx)
        struct sk_if_softc	*sc_if;
        struct mbuf		*m_head;
        u_int32_t		*txidx;
{
	struct sk_tx_desc	*f = NULL;
	struct mbuf		*m;
	u_int32_t		frag, cur, cnt = 0;

	m = m_head;
	cur = frag = *txidx;

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	for (m = m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((SK_TX_RING_CNT -
			    (sc_if->sk_cdata.sk_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc_if->sk_rdata->sk_tx_ring[frag];
			f->sk_data_lo = vtophys(mtod(m, vm_offset_t));
			f->sk_ctl = m->m_len | SK_OPCODE_DEFAULT;
			if (cnt == 0)
				f->sk_ctl |= SK_TXCTL_FIRSTFRAG;
			else
				f->sk_ctl |= SK_TXCTL_OWN;
			cur = frag;
			SK_INC(frag, SK_TX_RING_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	sc_if->sk_rdata->sk_tx_ring[cur].sk_ctl |=
		SK_TXCTL_LASTFRAG|SK_TXCTL_EOF_INTR;
	sc_if->sk_cdata.sk_tx_chain[cur].sk_mbuf = m_head;
	sc_if->sk_rdata->sk_tx_ring[*txidx].sk_ctl |= SK_TXCTL_OWN;
	sc_if->sk_cdata.sk_tx_cnt += cnt;

	*txidx = frag;

	return(0);
}

void sk_start(ifp)
	struct ifnet		*ifp;
{
        struct sk_softc		*sc;
        struct sk_if_softc	*sc_if;
        struct mbuf		*m_head = NULL;
        u_int32_t		idx;
	int			pkts = 0;

	sc_if = ifp->if_softc;
	sc = sc_if->sk_softc;

	idx = sc_if->sk_cdata.sk_tx_prod;

	while(sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf == NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/*
		 * Pack the data into the transmit ring. If we
		 * don't have room, set the OACTIVE flag and wait
		 * for the NIC to drain the ring.
		 */
		if (sk_encap(sc_if, m_head, &idx)) {
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
			bpf_mtap(ifp->if_bpf, m_head);
#endif
	}
	if (pkts == 0)
		return;

	/* Transmit */
	sc_if->sk_cdata.sk_tx_prod = idx;
	CSR_WRITE_4(sc, sc_if->sk_tx_bmu, SK_TXBMU_TX_START);

	/* Set a timeout in case the chip goes out to lunch. */
	ifp->if_timer = 5;

	return;
}


void sk_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct sk_if_softc	*sc_if;

	sc_if = ifp->if_softc;

	printf("%s: watchdog timeout\n", sc_if->sk_dev.dv_xname);
	sk_init(sc_if);

	return;
}

void sk_shutdown(v)
	void *v;
{
	struct sk_softc		*sc = v;

	/* Turn off the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_OFF);

	/*
	 * Reset the GEnesis controller. Doing this should also
	 * assert the resets on the attached XMAC(s).
	 */
	sk_reset(sc);

	return;
}

void sk_rxeof(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	struct sk_chain		*cur_rx;
	int			total_len = 0;
	int			i;
	u_int32_t		rxstat;

	ifp = &sc_if->arpcom.ac_if;
	i = sc_if->sk_cdata.sk_rx_prod;
	cur_rx = &sc_if->sk_cdata.sk_rx_chain[i];

	while(!(sc_if->sk_rdata->sk_rx_ring[i].sk_ctl & SK_RXCTL_OWN)) {

		cur_rx = &sc_if->sk_cdata.sk_rx_chain[i];
		rxstat = sc_if->sk_rdata->sk_rx_ring[i].sk_xmac_rxstat;
		m = cur_rx->sk_mbuf;
		cur_rx->sk_mbuf = NULL;
		total_len = SK_RXBYTES(sc_if->sk_rdata->sk_rx_ring[i].sk_ctl);
		SK_INC(i, SK_RX_RING_CNT);

		if (rxstat & XM_RXSTAT_ERRFRAME) {
			ifp->if_ierrors++;
			sk_newbuf(sc_if, cur_rx, m);
			continue;
		}

		/*
		 * Try to allocate a new jumbo buffer. If that
		 * fails, copy the packet to mbufs and put the
		 * jumbo buffer back in the ring so it can be
		 * re-used. If allocating mbufs fails, then we
		 * have to drop the packet.
		 */
		if (sk_newbuf(sc_if, cur_rx, NULL) == ENOBUFS) {
			struct mbuf		*m0;
			m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
			    total_len + ETHER_ALIGN, 0, ifp, NULL);
			sk_newbuf(sc_if, cur_rx, m);
			if (m0 == NULL) {
				printf("%s: no receive buffers "
				    "available -- packet dropped!\n",
				    sc_if->sk_dev.dv_xname);
				ifp->if_ierrors++;
				continue;
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
			bpf_mtap(ifp->if_bpf, m);
#endif
		/* pass it on. */
		ether_input_mbuf(ifp, m);
	}

	sc_if->sk_cdata.sk_rx_prod = i;

	return;
}

void sk_txeof(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_tx_desc	*cur_tx = NULL;
	struct ifnet		*ifp;
	u_int32_t		idx;

	ifp = &sc_if->arpcom.ac_if;

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	idx = sc_if->sk_cdata.sk_tx_cons;
	while(idx != sc_if->sk_cdata.sk_tx_prod) {
		cur_tx = &sc_if->sk_rdata->sk_tx_ring[idx];
		if (cur_tx->sk_ctl & SK_TXCTL_OWN)
			break;
		if (cur_tx->sk_ctl & SK_TXCTL_LASTFRAG)
			ifp->if_opackets++;
		if (sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf = NULL;
		}
		sc_if->sk_cdata.sk_tx_cnt--;
		SK_INC(idx, SK_TX_RING_CNT);
		ifp->if_timer = 0;
	}

	sc_if->sk_cdata.sk_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

void
sk_tick(xsc_if)
	void *xsc_if;
{
	struct sk_if_softc *sc_if;
	struct mii_data *mii;
	struct ifnet *ifp;
	int i;

	sc_if = xsc_if;
	ifp = &sc_if->arpcom.ac_if;
	mii = &sc_if->sk_mii;

	if (!(ifp->if_flags & IFF_UP))
		return;

	if (sc_if->sk_phytype == SK_PHYTYPE_BCOM) {
		sk_intr_bcom(sc_if);
		return;
	}

	/*
	 * According to SysKonnect, the correct way to verify that
	 * the link has come back up is to poll bit 0 of the GPIO
	 * register three times. This pin has the signal from the
	 * link sync pin connected to it; if we read the same link
	 * state 3 times in a row, we know the link is up.
	 */
	for (i = 0; i < 3; i++) {
		if (SK_XM_READ_2(sc_if, XM_GPIO) & XM_GPIO_GP0_SET)
			break;
	}

	if (i != 3) {
		timeout_add(&sc_if->sk_tick_ch, hz);
		return;
	}

	/* Turn the GP0 interrupt back on. */
	SK_XM_CLRBIT_2(sc_if, XM_IMR, XM_IMR_GP0_SET);
	SK_XM_READ_2(sc_if, XM_ISR);
	mii_tick(mii);
	mii_pollstat(mii);
	timeout_del(&sc_if->sk_tick_ch);
}

void
sk_intr_bcom(sc_if)
	struct sk_if_softc *sc_if;
{
	struct sk_softc *sc;
	struct mii_data *mii;
	struct ifnet *ifp;
	int status;

	sc = sc_if->sk_softc;
	mii = &sc_if->sk_mii;
	ifp = &sc_if->arpcom.ac_if;

	SK_XM_CLRBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);

	/*
	 * Read the PHY interrupt register to make sure
	 * we clear any pending interrupts.
	 */
	status = sk_miibus_readreg((struct device *)sc_if,
	    SK_PHYADDR_BCOM, BRGPHY_MII_ISR);

	if (!(ifp->if_flags & IFF_RUNNING)) {
		sk_init_xmac(sc_if);
		return;
	}

	if (status & (BRGPHY_ISR_LNK_CHG|BRGPHY_ISR_AN_PR)) {
		int lstat;
		lstat = sk_miibus_readreg((struct device *)sc_if,
		    SK_PHYADDR_BCOM, BRGPHY_MII_AUXSTS);

		if (!(lstat & BRGPHY_AUXSTS_LINK) && sc_if->sk_link) {
			mii_mediachg(mii);
			/* Turn off the link LED. */
			SK_IF_WRITE_1(sc_if, 0,
			    SK_LINKLED1_CTL, SK_LINKLED_OFF);
			sc_if->sk_link = 0;
		} else if (status & BRGPHY_ISR_LNK_CHG) {
			sk_miibus_writereg((struct device *)sc_if,
			    SK_PHYADDR_BCOM, BRGPHY_MII_IMR, 0xFF00);
			mii_tick(mii);
			sc_if->sk_link = 1;
			/* Turn on the link LED. */
			SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL,
			    SK_LINKLED_ON|SK_LINKLED_LINKSYNC_OFF|
			    SK_LINKLED_BLINK_OFF);
			mii_pollstat(mii);
		} else {
			mii_tick(mii);
			timeout_add(&sc_if->sk_tick_ch, hz);
		}
	}

	SK_XM_SETBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);

	return;
}

void sk_intr_xmac(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_softc		*sc;
	u_int16_t		status;

	sc = sc_if->sk_softc;
	status = SK_XM_READ_2(sc_if, XM_ISR);

	if (sc_if->sk_phytype == SK_PHYTYPE_XMAC) {
		if (status & XM_ISR_GP0_SET) {
			SK_XM_SETBIT_2(sc_if, XM_IMR, XM_IMR_GP0_SET);
			timeout_add(&sc_if->sk_tick_ch, hz);
		}

		if (status & XM_ISR_AUTONEG_DONE) {
			timeout_add(&sc_if->sk_tick_ch, hz);
		}
	}

	if (status & XM_IMR_TX_UNDERRUN)
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_FLUSH_TXFIFO);

	if (status & XM_IMR_RX_OVERRUN)
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_FLUSH_RXFIFO);

	return;
}

int sk_intr(xsc)
	void			*xsc;
{
	struct sk_softc		*sc = xsc;
	struct sk_if_softc	*sc_if0 = NULL, *sc_if1 = NULL;
	struct ifnet		*ifp0 = NULL, *ifp1 = NULL;
	u_int32_t		status;
	int			claimed = 0;

	sc_if0 = sc->sk_if[SK_PORT_A];
	sc_if1 = sc->sk_if[SK_PORT_B];

	if (sc_if0 != NULL)
		ifp0 = &sc_if0->arpcom.ac_if;
	if (sc_if1 != NULL)
		ifp1 = &sc_if1->arpcom.ac_if;

	for (;;) {
		status = CSR_READ_4(sc, SK_ISSR);
		if (!(status & sc->sk_intrmask))
			break;

		claimed = 1;

		/* Handle receive interrupts first. */
		if (status & SK_ISR_RX1_EOF) {
			sk_rxeof(sc_if0);
			CSR_WRITE_4(sc, SK_BMU_RX_CSR0,
			    SK_RXBMU_CLR_IRQ_EOF|SK_RXBMU_RX_START);
		}
		if (status & SK_ISR_RX2_EOF) {
			sk_rxeof(sc_if1);
			CSR_WRITE_4(sc, SK_BMU_RX_CSR1,
			    SK_RXBMU_CLR_IRQ_EOF|SK_RXBMU_RX_START);
		}

		/* Then transmit interrupts. */
		if (status & SK_ISR_TX1_S_EOF) {
			sk_txeof(sc_if0);
			CSR_WRITE_4(sc, SK_BMU_TXS_CSR0,
			    SK_TXBMU_CLR_IRQ_EOF);
		}
		if (status & SK_ISR_TX2_S_EOF) {
			sk_txeof(sc_if1);
			CSR_WRITE_4(sc, SK_BMU_TXS_CSR1,
			    SK_TXBMU_CLR_IRQ_EOF);
		}

		/* Then MAC interrupts. */
		if (status & SK_ISR_MAC1 &&
		    ifp0->if_flags & IFF_RUNNING)
			sk_intr_xmac(sc_if0);

		if (status & SK_ISR_MAC2 &&
		    ifp1->if_flags & IFF_RUNNING)
			sk_intr_xmac(sc_if1);

		if (status & SK_ISR_EXTERNAL_REG) {
			if (ifp0 != NULL)
				sk_intr_bcom(sc_if0);
			if (ifp1 != NULL)
				sk_intr_bcom(sc_if1);
		}
	}

	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	if (ifp0 != NULL && !IFQ_IS_EMPTY(&ifp0->if_snd))
		sk_start(ifp0);
	if (ifp1 != NULL && !IFQ_IS_EMPTY(&ifp1->if_snd))
		sk_start(ifp1);

	return (claimed);
}

void sk_init_xmac(sc_if)
	struct sk_if_softc	*sc_if;
{
	struct sk_softc		*sc;
	struct ifnet		*ifp;
	struct sk_bcom_hack     bhack[] = {
	{ 0x18, 0x0c20 }, { 0x17, 0x0012 }, { 0x15, 0x1104 }, { 0x17, 0x0013 },
	{ 0x15, 0x0404 }, { 0x17, 0x8006 }, { 0x15, 0x0132 }, { 0x17, 0x8006 },
	{ 0x15, 0x0232 }, { 0x17, 0x800D }, { 0x15, 0x000F }, { 0x18, 0x0420 },
	{ 0, 0 } };

	sc = sc_if->sk_softc;
	ifp = &sc_if->arpcom.ac_if;

	/* Unreset the XMAC. */
	SK_IF_WRITE_2(sc_if, 0, SK_TXF1_MACCTL, SK_TXMACCTL_XMAC_UNRESET);
	DELAY(1000);

	/* Reset the XMAC's internal state. */
	SK_XM_SETBIT_2(sc_if, XM_GPIO, XM_GPIO_RESETMAC);

	/* Save the XMAC II revision */
	sc_if->sk_xmac_rev = XM_XMAC_REV(SK_XM_READ_4(sc_if, XM_DEVID));

	/*
	 * Perform additional initialization for external PHYs,
	 * namely for the 1000baseTX cards that use the XMAC's
	 * GMII mode.
	 */
	if (sc_if->sk_phytype == SK_PHYTYPE_BCOM) {
		int			i = 0;
		u_int32_t		val;

		/* Take PHY out of reset. */
		val = sk_win_read_4(sc, SK_GPIO);
		if (sc_if->sk_port == SK_PORT_A)
			val |= SK_GPIO_DIR0|SK_GPIO_DAT0;
		else
			val |= SK_GPIO_DIR2|SK_GPIO_DAT2;
		sk_win_write_4(sc, SK_GPIO, val);

		/* Enable GMII mode on the XMAC. */
		SK_XM_SETBIT_2(sc_if, XM_HWCFG, XM_HWCFG_GMIIMODE);

		sk_miibus_writereg((struct device *)sc_if, SK_PHYADDR_BCOM,
		    BRGPHY_MII_BMCR, BRGPHY_BMCR_RESET);
		DELAY(10000);
		sk_miibus_writereg((struct device *)sc_if, SK_PHYADDR_BCOM,
		    BRGPHY_MII_IMR, 0xFFF0);

		/*
		 * Early versions of the BCM5400 apparently have
		 * a bug that requires them to have their reserved
		 * registers initialized to some magic values. I don't
		 * know what the numbers do, I'm just the messenger.
		 */
		if (sk_miibus_readreg((struct device *)sc_if,
		    SK_PHYADDR_BCOM, 0x03) == 0x6041) {
			while(bhack[i].reg) {
				sk_miibus_writereg((struct device *)sc_if,
				    SK_PHYADDR_BCOM, bhack[i].reg,
				    bhack[i].val);
				i++;
			}
		}
	}

	/* Set station address */
	SK_XM_WRITE_2(sc_if, XM_PAR0,
	    *(u_int16_t *)(&sc_if->arpcom.ac_enaddr[0]));
	SK_XM_WRITE_2(sc_if, XM_PAR1,
	    *(u_int16_t *)(&sc_if->arpcom.ac_enaddr[2]));
	SK_XM_WRITE_2(sc_if, XM_PAR2,
	    *(u_int16_t *)(&sc_if->arpcom.ac_enaddr[4]));
	SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_USE_STATION);

	if (ifp->if_flags & IFF_PROMISC) {
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_PROMISC);
	} else {
		SK_XM_CLRBIT_4(sc_if, XM_MODE, XM_MODE_RX_PROMISC);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		SK_XM_CLRBIT_4(sc_if, XM_MODE, XM_MODE_RX_NOBROAD);
	} else {
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_NOBROAD);
	}

	/* We don't need the FCS appended to the packet. */
	SK_XM_SETBIT_2(sc_if, XM_RXCMD, XM_RXCMD_STRIPFCS);

	/* We want short frames padded to 60 bytes. */
	SK_XM_SETBIT_2(sc_if, XM_TXCMD, XM_TXCMD_AUTOPAD);

	/*
	 * Enable the reception of all error frames. This is is
	 * a necessary evil due to the design of the XMAC. The
	 * XMAC's receive FIFO is only 8K in size, however jumbo
	 * frames can be up to 9000 bytes in length. When bad
	 * frame filtering is enabled, the XMAC's RX FIFO operates
	 * in 'store and forward' mode. For this to work, the
	 * entire frame has to fit into the FIFO, but that means
	 * that jumbo frames larger than 8192 bytes will be
	 * truncated. Disabling all bad frame filtering causes
	 * the RX FIFO to operate in streaming mode, in which
	 * case the XMAC will start transfering frames out of the
	 * RX FIFO as soon as the FIFO threshold is reached.
	 */
	SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_BADFRAMES|
	    XM_MODE_RX_GIANTS|XM_MODE_RX_RUNTS|XM_MODE_RX_CRCERRS|
	    XM_MODE_RX_INRANGELEN);

	if (ifp->if_mtu > (ETHERMTU + ETHER_HDR_LEN + ETHER_CRC_LEN))
		SK_XM_SETBIT_2(sc_if, XM_RXCMD, XM_RXCMD_BIGPKTOK);
	else
		SK_XM_CLRBIT_2(sc_if, XM_RXCMD, XM_RXCMD_BIGPKTOK);

	/*
	 * Bump up the transmit threshold. This helps hold off transmit
	 * underruns when we're blasting traffic from both ports at once.
	 */
	SK_XM_WRITE_2(sc_if, XM_TX_REQTHRESH, SK_XM_TX_FIFOTHRESH);

	/* Set multicast filter */
	sk_setmulti(sc_if);

	/* Clear and enable interrupts */
	SK_XM_READ_2(sc_if, XM_ISR);
	if (sc_if->sk_phytype == SK_PHYTYPE_XMAC)
		SK_XM_WRITE_2(sc_if, XM_IMR, XM_INTRS);
	else
		SK_XM_WRITE_2(sc_if, XM_IMR, 0xFFFF);

	/* Configure MAC arbiter */
	switch(sc_if->sk_xmac_rev) {
	case XM_XMAC_REV_B2:
		sk_win_write_1(sc, SK_RCINIT_RX1, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_TX1, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_RX2, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RCINIT_TX2, SK_RCINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_RX1, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_TX1, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_RX2, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_MINIT_TX2, SK_MINIT_XMAC_B2);
		sk_win_write_1(sc, SK_RECOVERY_CTL, SK_RECOVERY_XMAC_B2);
		break;
	case XM_XMAC_REV_C1:
		sk_win_write_1(sc, SK_RCINIT_RX1, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_TX1, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_RX2, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RCINIT_TX2, SK_RCINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_RX1, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_TX1, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_RX2, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_MINIT_TX2, SK_MINIT_XMAC_C1);
		sk_win_write_1(sc, SK_RECOVERY_CTL, SK_RECOVERY_XMAC_B2);
		break;
	default:
		break;
	}
	sk_win_write_2(sc, SK_MACARB_CTL,
	    SK_MACARBCTL_UNRESET|SK_MACARBCTL_FASTOE_OFF);

	sc_if->sk_link = 1;

	return;
}

/*
 * Note that to properly initialize any part of the GEnesis chip,
 * you first have to take it out of reset mode.
 */
void sk_init(xsc)
	void			*xsc;
{
	struct sk_if_softc	*sc_if = xsc;
	struct sk_softc		*sc;
	struct ifnet		*ifp;
	struct mii_data		*mii;
	int			s;

	s = splimp();

	ifp = &sc_if->arpcom.ac_if;
	sc = sc_if->sk_softc;
	mii = &sc_if->sk_mii;

	/* Cancel pending I/O and free all RX/TX buffers. */
	sk_stop(sc_if);

	/* Configure LINK_SYNC LED */
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_ON);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_LINKSYNC_ON);

	/* Configure RX LED */
	SK_IF_WRITE_1(sc_if, 0, SK_RXLED1_CTL, SK_RXLEDCTL_COUNTER_START);

	/* Configure TX LED */
	SK_IF_WRITE_1(sc_if, 0, SK_TXLED1_CTL, SK_TXLEDCTL_COUNTER_START);

	/* Configure I2C registers */

	/* Configure XMAC(s) */
	sk_init_xmac(sc_if);
	mii_mediachg(mii);

	/* Configure MAC FIFOs */
	SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_UNRESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXF1_END, SK_FIFO_END);
	SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_ON);

	SK_IF_WRITE_4(sc_if, 0, SK_TXF1_CTL, SK_FIFO_UNRESET);
	SK_IF_WRITE_4(sc_if, 0, SK_TXF1_END, SK_FIFO_END);
	SK_IF_WRITE_4(sc_if, 0, SK_TXF1_CTL, SK_FIFO_ON);

	/* Configure transmit arbiter(s) */
	SK_IF_WRITE_1(sc_if, 0, SK_TXAR1_COUNTERCTL,
	    SK_TXARCTL_ON|SK_TXARCTL_FSYNC_ON);

	/* Configure RAMbuffers */
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_UNRESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_START, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_WR_PTR, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_RD_PTR, sc_if->sk_rx_ramstart);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_END, sc_if->sk_rx_ramend);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_ON);

	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_UNRESET);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_STORENFWD_ON);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_START, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_WR_PTR, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_RD_PTR, sc_if->sk_tx_ramstart);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_END, sc_if->sk_tx_ramend);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_ON);

	/* Configure BMUs */
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_ONLINE);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_CURADDR_LO,
	    vtophys(&sc_if->sk_rdata->sk_rx_ring[0]));
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_CURADDR_HI, 0);

	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_BMU_CSR, SK_TXBMU_ONLINE);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_CURADDR_LO,
	    vtophys(&sc_if->sk_rdata->sk_tx_ring[0]));
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_CURADDR_HI, 0);

	/* Init descriptors */
	if (sk_init_rx_ring(sc_if) == ENOBUFS) {
		printf("%s: initialization failed: no "
		    "memory for rx buffers\n", sc_if->sk_dev.dv_xname);
		sk_stop(sc_if);
		splx(s);
		return;
	}
	sk_init_tx_ring(sc_if);

	/* Configure interrupt handling */
	CSR_READ_4(sc, SK_ISSR);
	if (sc_if->sk_port == SK_PORT_A)
		sc->sk_intrmask |= SK_INTRS1;
	else
		sc->sk_intrmask |= SK_INTRS2;

	sc->sk_intrmask |= SK_ISR_EXTERNAL_REG;

	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	/* Start BMUs. */
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_RX_START);

	/* Enable XMACs TX and RX state machines */
	SK_XM_CLRBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_IGNPAUSE);
	SK_XM_SETBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	return;
}

void sk_stop(sc_if)
	struct sk_if_softc	*sc_if;
{
	int			i;
	struct sk_softc		*sc;
	struct ifnet		*ifp;

	sc = sc_if->sk_softc;
	ifp = &sc_if->arpcom.ac_if;

	timeout_del(&sc_if->sk_tick_ch);

	if (sc_if->sk_phytype == SK_PHYTYPE_BCOM) {
		u_int32_t		val;

		/* Put PHY back into reset. */
		val = sk_win_read_4(sc, SK_GPIO);
		if (sc_if->sk_port == SK_PORT_A) {
			val |= SK_GPIO_DIR0;
			val &= ~SK_GPIO_DAT0;
		} else {
			val |= SK_GPIO_DIR2;
			val &= ~SK_GPIO_DAT2;
		}
		sk_win_write_4(sc, SK_GPIO, val);
	}

	/* Turn off various components of this interface. */
	SK_XM_SETBIT_2(sc_if, XM_GPIO, XM_GPIO_RESETMAC);
	SK_IF_WRITE_2(sc_if, 0, SK_TXF1_MACCTL, SK_TXMACCTL_XMAC_RESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_RESET);
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_BMU_CSR, SK_RXBMU_OFFLINE);
	SK_IF_WRITE_4(sc_if, 0, SK_RXRB1_CTLTST, SK_RBCTL_RESET|SK_RBCTL_OFF);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_BMU_CSR, SK_TXBMU_OFFLINE);
	SK_IF_WRITE_4(sc_if, 1, SK_TXRBS1_CTLTST, SK_RBCTL_RESET|SK_RBCTL_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_TXAR1_COUNTERCTL, SK_TXARCTL_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_RXLED1_CTL, SK_RXLEDCTL_COUNTER_STOP);
	SK_IF_WRITE_1(sc_if, 0, SK_TXLED1_CTL, SK_RXLEDCTL_COUNTER_STOP);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_OFF);
	SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_LINKSYNC_OFF);

	/* Disable interrupts */
	if (sc_if->sk_port == SK_PORT_A)
		sc->sk_intrmask &= ~SK_INTRS1;
	else
		sc->sk_intrmask &= ~SK_INTRS2;
	CSR_WRITE_4(sc, SK_IMR, sc->sk_intrmask);

	SK_XM_READ_2(sc_if, XM_ISR);
	SK_XM_WRITE_2(sc_if, XM_IMR, 0xFFFF);

	/* Free RX and TX mbufs still in the queues. */
	for (i = 0; i < SK_RX_RING_CNT; i++) {
		if (sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf);
			sc_if->sk_cdata.sk_rx_chain[i].sk_mbuf = NULL;
		}
	}

	for (i = 0; i < SK_TX_RING_CNT; i++) {
		if (sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[i].sk_mbuf = NULL;
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING|IFF_OACTIVE);

	return;
}

struct cfattach skc_ca = {
	sizeof(struct sk_softc), skc_probe, skc_attach,
};

struct cfdriver skc_cd = {
	0, "skc", DV_DULL
};

struct cfattach sk_ca = {
	sizeof(struct sk_if_softc), sk_probe, sk_attach,
};

struct cfdriver sk_cd = {
	0, "sk", DV_IFNET
};
