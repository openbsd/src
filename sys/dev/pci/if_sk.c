/*	$OpenBSD: if_sk.c,v 1.48 2004/11/11 19:08:00 brad Exp $	*/

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

#define	SK_VERBOSE
/* #define SK_USEIOSPACE */

#include <dev/pci/if_skreg.h>
#include <dev/pci/xmaciireg.h>
#include <dev/pci/yukonreg.h>

int skc_probe(struct device *, void *, void *);
void skc_attach(struct device *, struct device *self, void *aux);
int sk_probe(struct device *, void *, void *);
void sk_attach(struct device *, struct device *self, void *aux);
int skcprint(void *, const char *);
int sk_intr(void *);
void sk_intr_bcom(struct sk_if_softc *);
void sk_intr_xmac(struct sk_if_softc *);
void sk_intr_yukon(struct sk_if_softc *);
void sk_rxeof(struct sk_if_softc *);
void sk_txeof(struct sk_if_softc *);
int sk_encap(struct sk_if_softc *, struct mbuf *, u_int32_t *);
void sk_start(struct ifnet *);
int sk_ioctl(struct ifnet *, u_long, caddr_t);
void sk_init(void *);
void sk_init_xmac(struct sk_if_softc *);
void sk_init_yukon(struct sk_if_softc *);
void sk_stop(struct sk_if_softc *);
void sk_watchdog(struct ifnet *);
void sk_shutdown(void *);
int sk_ifmedia_upd(struct ifnet *);
void sk_ifmedia_sts(struct ifnet *, struct ifmediareq *);
void sk_reset(struct sk_softc *);
int sk_newbuf(struct sk_if_softc *, int, struct mbuf *, bus_dmamap_t);
int sk_alloc_jumbo_mem(struct sk_if_softc *);
void sk_free_jumbo_mem(struct sk_if_softc *);
void *sk_jalloc(struct sk_if_softc *);
void sk_jfree(caddr_t, u_int, void *);
int sk_init_rx_ring(struct sk_if_softc *);
int sk_init_tx_ring(struct sk_if_softc *);
u_int8_t sk_vpd_readbyte(struct sk_softc *, int);
void sk_vpd_read_res(struct sk_softc *, struct vpd_res *, int);
void sk_vpd_read(struct sk_softc *);

int sk_xmac_miibus_readreg(struct device *, int, int);
void sk_xmac_miibus_writereg(struct device *, int, int, int);
void sk_xmac_miibus_statchg(struct device *);

int sk_marv_miibus_readreg(struct device *, int, int);
void sk_marv_miibus_writereg(struct device *, int, int, int);
void sk_marv_miibus_statchg(struct device *);

u_int32_t sk_xmac_hash(caddr_t);
u_int32_t sk_yukon_hash(caddr_t);
void sk_setfilt(struct sk_if_softc *, caddr_t, int);
void sk_setmulti(struct sk_if_softc *);
void sk_tick(void *);
void sk_rxcsum(struct ifnet *, struct mbuf *, const u_int16_t, const u_int16_t);

#ifdef SK_DEBUG
#define DPRINTF(x)	if (skdebug) printf x
#define DPRINTFN(n,x)	if (skdebug >= (n)) printf x
int	skdebug = 0;

void sk_dump_txdesc(struct sk_tx_desc *, int);
void sk_dump_mbuf(struct mbuf *);
void sk_dump_bytes(const char *, int);
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

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

/* supported device vendors */
const struct pci_matchid skc_devices[] = {
	{ PCI_VENDOR_3COM,		PCI_PRODUCT_3COM_3C940},
	{ PCI_VENDOR_DLINK,		PCI_PRODUCT_DLINK_DGE530T},
	{ PCI_VENDOR_LINKSYS,		PCI_PRODUCT_LINKSYS_EG1032},
	{ PCI_VENDOR_LINKSYS,		PCI_PRODUCT_LINKSYS_EG1064},
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_SK_V2},
	{ PCI_VENDOR_MARVELL,		PCI_PRODUCT_MARVELL_SK_V2_BELKIN},
	{ PCI_VENDOR_SCHNEIDERKOCH,	PCI_PRODUCT_SCHNEIDERKOCH_GE},
	{ PCI_VENDOR_SCHNEIDERKOCH,	PCI_PRODUCT_SCHNEIDERKOCH_SK9821v2},
};

static inline u_int32_t
sk_win_read_4(struct sk_softc *sc, u_int32_t reg)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return CSR_READ_4(sc, SK_WIN_BASE + SK_REG(reg));
#else
	return CSR_READ_4(sc, reg);
#endif
}

static inline u_int16_t
sk_win_read_2(struct sk_softc *sc, u_int32_t reg)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return CSR_READ_2(sc, SK_WIN_BASE + SK_REG(reg));
#else
	return CSR_READ_2(sc, reg);
#endif
}

static inline u_int8_t
sk_win_read_1(struct sk_softc *sc, u_int32_t reg)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	return CSR_READ_1(sc, SK_WIN_BASE + SK_REG(reg));
#else
	return CSR_READ_1(sc, reg);
#endif
}

static inline void
sk_win_write_4(struct sk_softc *sc, u_int32_t reg, u_int32_t x)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_4(sc, SK_WIN_BASE + SK_REG(reg), x);
#else
	CSR_WRITE_4(sc, reg, x);
#endif
}

static inline void
sk_win_write_2(struct sk_softc *sc, u_int32_t reg, u_int16_t x)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_2(sc, SK_WIN_BASE + SK_REG(reg), x);
#else
	CSR_WRITE_2(sc, reg, x);
#endif
}

static inline void
sk_win_write_1(struct sk_softc *sc, u_int32_t reg, u_int8_t x)
{
#ifdef SK_USEIOSPACE
	CSR_WRITE_4(sc, SK_RAP, SK_WIN(reg));
	CSR_WRITE_1(sc, SK_WIN_BASE + SK_REG(reg), x);
#else
	CSR_WRITE_1(sc, reg, x);
#endif
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
u_int8_t
sk_vpd_readbyte(struct sk_softc *sc, int addr)
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

void
sk_vpd_read_res(struct sk_softc *sc, struct vpd_res *res, int addr)
{
	int			i;
	u_int8_t		*ptr;

	ptr = (u_int8_t *)res;
	for (i = 0; i < sizeof(struct vpd_res); i++)
		ptr[i] = sk_vpd_readbyte(sc, i + addr);
}

void
sk_vpd_read(struct sk_softc *sc)
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

	/*
	 * Bail out quietly if the eeprom appears to be missing or empty.
	 */
	if (res.vr_id == 0xff && res.vr_len == 0xff && res.vr_pad == 0xff)
		return;

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
}

int
sk_xmac_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	int i;

	DPRINTFN(9, ("sk_xmac_miibus_readreg\n"));

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
sk_xmac_miibus_writereg(struct device *dev, int phy, int reg, int val)
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	int i;

	DPRINTFN(9, ("sk_xmac_miibus_writereg\n"));

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
}

void
sk_xmac_miibus_statchg(struct device *dev)
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	struct mii_data *mii = &sc_if->sk_mii;

	DPRINTFN(9, ("sk_xmac_miibus_statchg\n"));

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
}

int
sk_marv_miibus_readreg(dev, phy, reg)
	struct device *dev;
	int phy, reg;
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	u_int16_t val;
	int i;

	if (phy != 0 ||
	    (sc_if->sk_phytype != SK_PHYTYPE_MARV_COPPER &&
	     sc_if->sk_phytype != SK_PHYTYPE_MARV_FIBER)) {
		DPRINTFN(9, ("sk_marv_miibus_readreg (skip) phy=%d, reg=%#x\n",
			     phy, reg));
		return(0);
	}

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
		return 0;
	}
        
 	DPRINTFN(9, ("sk_marv_miibus_readreg: i=%d, timeout=%d\n", i,
		     SK_TIMEOUT));

        val = SK_YU_READ_2(sc_if, YUKON_SMIDR);

	DPRINTFN(9, ("sk_marv_miibus_readreg phy=%d, reg=%#x, val=%#x\n",
		     phy, reg, val));

	return val;
}

void
sk_marv_miibus_writereg(dev, phy, reg, val)
	struct device *dev;
	int phy, reg, val;
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *)dev;
	int i;

	DPRINTFN(9, ("sk_marv_miibus_writereg phy=%d reg=%#x val=%#x\n",
		     phy, reg, val));

	SK_YU_WRITE_2(sc_if, YUKON_SMIDR, val);
	SK_YU_WRITE_2(sc_if, YUKON_SMICR, YU_SMICR_PHYAD(phy) |
		      YU_SMICR_REGAD(reg) | YU_SMICR_OP_WRITE);

	for (i = 0; i < SK_TIMEOUT; i++) {
		DELAY(1);
		if (SK_YU_READ_2(sc_if, YUKON_SMICR) & YU_SMICR_BUSY)
			break;
	}
}

void
sk_marv_miibus_statchg(dev)
	struct device *dev;
{
	DPRINTFN(9, ("sk_marv_miibus_statchg: gpcr=%x\n",
		     SK_YU_READ_2(((struct sk_if_softc *)dev), YUKON_GPCR)));
}

#define HASH_BITS	6
  
u_int32_t
sk_xmac_hash(caddr_t addr)
{
	u_int32_t crc;

	crc = ether_crc32_le(addr, ETHER_ADDR_LEN);
	return (~crc & ((1 << HASH_BITS) - 1));
}

u_int32_t
sk_yukon_hash(caddr_t addr)
{
	u_int32_t crc;

	crc = ether_crc32_be(addr, ETHER_ADDR_LEN);
	return (crc & ((1 << HASH_BITS) - 1));
}

void
sk_setfilt(struct sk_if_softc *sc_if, caddr_t addr, int slot)
{
	int base = XM_RXFILT_ENTRY(slot);

	SK_XM_WRITE_2(sc_if, base, *(u_int16_t *)(&addr[0]));
	SK_XM_WRITE_2(sc_if, base + 2, *(u_int16_t *)(&addr[2]));
	SK_XM_WRITE_2(sc_if, base + 4, *(u_int16_t *)(&addr[4]));
}

void
sk_setmulti(struct sk_if_softc *sc_if)
{
	struct sk_softc *sc = sc_if->sk_softc;
	struct ifnet *ifp= &sc_if->arpcom.ac_if;
	u_int32_t hashes[2] = { 0, 0 };
	int h, i;
	struct arpcom *ac = &sc_if->arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int8_t dummy[] = { 0, 0, 0, 0, 0 ,0 };

	/* First, zot all the existing filters. */
	switch(sc->sk_type) {
	case SK_GENESIS:
		for (i = 1; i < XM_RXFILT_MAX; i++)
			sk_setfilt(sc_if, (caddr_t)&dummy, i);

		SK_XM_WRITE_4(sc_if, XM_MAR0, 0);
		SK_XM_WRITE_4(sc_if, XM_MAR2, 0);
		break;
	case SK_YUKON:
		SK_YU_WRITE_2(sc_if, YUKON_MCAH1, 0);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH2, 0);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH3, 0);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH4, 0);
		break;
	}

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
			if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
				 ETHER_ADDR_LEN)) {
				ifp->if_flags |= IFF_ALLMULTI;
				goto allmulti;
			}
			/*
			 * Program the first XM_RXFILT_MAX multicast groups
			 * into the perfect filter. For all others,
			 * use the hash table.
			 */
			if (sc->sk_type == SK_GENESIS && i < XM_RXFILT_MAX) {
				sk_setfilt(sc_if, enm->enm_addrlo, i);
				i++;
			}
			else {
				switch(sc->sk_type) {
				case SK_GENESIS:
					h = sk_xmac_hash(enm->enm_addrlo);
					break;
					
				case SK_YUKON:
					h = sk_yukon_hash(enm->enm_addrlo);
					break;
				}
				if (h < 32)
					hashes[0] |= (1 << h);
				else
					hashes[1] |= (1 << (h - 32));
			}

			ETHER_NEXT_MULTI(step, enm);
		}
	}

	switch(sc->sk_type) {
	case SK_GENESIS:
		SK_XM_SETBIT_4(sc_if, XM_MODE, XM_MODE_RX_USE_HASH|
			       XM_MODE_RX_USE_PERFECT);
		SK_XM_WRITE_4(sc_if, XM_MAR0, hashes[0]);
		SK_XM_WRITE_4(sc_if, XM_MAR2, hashes[1]);
		break;
	case SK_YUKON:
		SK_YU_WRITE_2(sc_if, YUKON_MCAH1, hashes[0] & 0xffff);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH2, (hashes[0] >> 16) & 0xffff);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH3, hashes[1] & 0xffff);
		SK_YU_WRITE_2(sc_if, YUKON_MCAH4, (hashes[1] >> 16) & 0xffff);
		break;
	}
}

int
sk_init_rx_ring(struct sk_if_softc *sc_if)
{
	struct sk_chain_data	*cd = &sc_if->sk_cdata;
	struct sk_ring_data	*rd = sc_if->sk_rdata;
	int			i;

	bzero((char *)rd->sk_rx_ring,
	    sizeof(struct sk_rx_desc) * SK_RX_RING_CNT);

	for (i = 0; i < SK_RX_RING_CNT; i++) {
		cd->sk_rx_chain[i].sk_desc = &rd->sk_rx_ring[i];
		if (i == (SK_RX_RING_CNT - 1)) {
			cd->sk_rx_chain[i].sk_next = &cd->sk_rx_chain[0];
			rd->sk_rx_ring[i].sk_next = SK_RX_RING_ADDR(sc_if, 0);
		} else {
			cd->sk_rx_chain[i].sk_next = &cd->sk_rx_chain[i + 1];
			rd->sk_rx_ring[i].sk_next = SK_RX_RING_ADDR(sc_if,i+1);
		}
		rd->sk_rx_ring[i].sk_csum1_start = ETHER_HDR_LEN;
		rd->sk_rx_ring[i].sk_csum2_start = ETHER_HDR_LEN +
		    sizeof(struct ip);
	}

	for (i = 0; i < SK_RX_RING_CNT; i++) {
		if (sk_newbuf(sc_if, i, NULL,
		    sc_if->sk_cdata.sk_rx_jumbo_map) == ENOBUFS) {
			printf("%s: failed alloc of %dth mbuf\n",
			    sc_if->sk_dev.dv_xname, i);
			return(ENOBUFS);
		}
	}

	sc_if->sk_cdata.sk_rx_prod = 0;
	sc_if->sk_cdata.sk_rx_cons = 0;

	return(0);
}

int
sk_init_tx_ring(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct sk_chain_data	*cd = &sc_if->sk_cdata;
	struct sk_ring_data	*rd = sc_if->sk_rdata;
	bus_dmamap_t		dmamap;
	struct sk_txmap_entry	*entry;
	int			i;

	bzero((char *)sc_if->sk_rdata->sk_tx_ring,
	    sizeof(struct sk_tx_desc) * SK_TX_RING_CNT);

	SLIST_INIT(&sc_if->sk_txmap_listhead);
	for (i = 0; i < SK_TX_RING_CNT; i++) {
		cd->sk_tx_chain[i].sk_desc = &rd->sk_tx_ring[i];
		if (i == (SK_TX_RING_CNT - 1)) {
			cd->sk_tx_chain[i].sk_next = &cd->sk_tx_chain[0];
			rd->sk_tx_ring[i].sk_next = SK_TX_RING_ADDR(sc_if, 0);
		} else {
			cd->sk_tx_chain[i].sk_next = &cd->sk_tx_chain[i + 1];
			rd->sk_tx_ring[i].sk_next = SK_TX_RING_ADDR(sc_if,i+1);
		}

		if (bus_dmamap_create(sc->sc_dmatag, SK_JLEN, SK_NTXSEG,
		   SK_JLEN, 0, BUS_DMA_NOWAIT, &dmamap))
			return (ENOBUFS);

		entry = malloc(sizeof(*entry), M_DEVBUF, M_NOWAIT);
		if (!entry) {
			bus_dmamap_destroy(sc->sc_dmatag, dmamap);
			return (ENOBUFS);
		}
		entry->dmamap = dmamap;
		SLIST_INSERT_HEAD(&sc_if->sk_txmap_listhead, entry, link);
	}

	sc_if->sk_cdata.sk_tx_prod = 0;
	sc_if->sk_cdata.sk_tx_cons = 0;
	sc_if->sk_cdata.sk_tx_cnt = 0;

	return (0);
}

int
sk_newbuf(struct sk_if_softc *sc_if, int i, struct mbuf *m,
	  bus_dmamap_t dmamap)
{
	struct mbuf		*m_new = NULL;
	struct sk_chain		*c;
	struct sk_rx_desc	*r;

	if (m == NULL) {
		caddr_t *buf = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(ENOBUFS);
		
		/* Allocate the jumbo buffer */
		buf = sk_jalloc(sc_if);
		if (buf == NULL) {
			m_freem(m_new);
			DPRINTFN(1, ("%s jumbo allocation failed -- packet "
			    "dropped!\n", sc_if->arpcom.ac_if.if_xname));
			return(ENOBUFS);
		}

		/* Attach the buffer to the mbuf */
		m_new->m_len = m_new->m_pkthdr.len = SK_JLEN;
		MEXTADD(m_new, buf, SK_JLEN, 0, sk_jfree, sc_if);
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
	r = c->sk_desc;
	c->sk_mbuf = m_new;
	r->sk_data_lo = dmamap->dm_segs[0].ds_addr +
	    (((vaddr_t)m_new->m_data
             - (vaddr_t)sc_if->sk_cdata.sk_jumbo_buf));
	r->sk_ctl = SK_JLEN | SK_RXSTAT;

	return(0);
}

/*
 * Memory management for jumbo frames.
 */

int
sk_alloc_jumbo_mem(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	caddr_t			ptr, kva;
	bus_dma_segment_t	seg;
	int		i, rseg;
	struct sk_jpool_entry   *entry;

	/* Grab a big chunk o' storage. */
	if (bus_dmamem_alloc(sc->sc_dmatag, SK_JMEM, PAGE_SIZE, 0,
			     &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: can't alloc rx buffers\n", sc->sk_dev.dv_xname);
		return (ENOBUFS);
	}
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg, SK_JMEM, &kva,
			   BUS_DMA_NOWAIT)) {
		printf("%s: can't map dma buffers (%d bytes)\n",
		    sc->sk_dev.dv_xname, SK_JMEM);
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		return (ENOBUFS);
	}
	if (bus_dmamap_create(sc->sc_dmatag, SK_JMEM, 1, SK_JMEM, 0,
	    BUS_DMA_NOWAIT, &sc_if->sk_cdata.sk_rx_jumbo_map)) {
		printf("%s: can't create dma map\n", sc->sk_dev.dv_xname);
		bus_dmamem_unmap(sc->sc_dmatag, kva, SK_JMEM);
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		return (ENOBUFS);
	}
	if (bus_dmamap_load(sc->sc_dmatag, sc_if->sk_cdata.sk_rx_jumbo_map,
			    kva, SK_JMEM, NULL, BUS_DMA_NOWAIT)) {
		printf("%s: can't load dma map\n", sc->sk_dev.dv_xname);
		bus_dmamap_destroy(sc->sc_dmatag,
				   sc_if->sk_cdata.sk_rx_jumbo_map);
		bus_dmamem_unmap(sc->sc_dmatag, kva, SK_JMEM);
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		return (ENOBUFS);
	}
	sc_if->sk_cdata.sk_jumbo_buf = (caddr_t)kva;
	DPRINTFN(1,("sk_jumbo_buf = 0x%08X\n", sc_if->sk_cdata.sk_jumbo_buf));

	LIST_INIT(&sc_if->sk_jfree_listhead);
	LIST_INIT(&sc_if->sk_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array.
	 */
	ptr = sc_if->sk_cdata.sk_jumbo_buf;
	for (i = 0; i < SK_JSLOTS; i++) {
		sc_if->sk_cdata.sk_jslots[i] = ptr;
		ptr += SK_JLEN;
		entry = malloc(sizeof(struct sk_jpool_entry),
		    M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			bus_dmamap_unload(sc->sc_dmatag,
			    sc_if->sk_cdata.sk_rx_jumbo_map);
			bus_dmamap_destroy(sc->sc_dmatag,
			    sc_if->sk_cdata.sk_rx_jumbo_map);
			bus_dmamem_unmap(sc->sc_dmatag, kva, SK_JMEM);
			bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
			sc_if->sk_cdata.sk_jumbo_buf = NULL;
			printf("%s: no memory for jumbo buffer queue!\n",
			    sc->sk_dev.dv_xname);
			return(ENOBUFS);
		}
		entry->slot = i;
		if (i)
		LIST_INSERT_HEAD(&sc_if->sk_jfree_listhead,
				 entry, jpool_entries);
		else
		LIST_INSERT_HEAD(&sc_if->sk_jinuse_listhead,
				 entry, jpool_entries);
	}

	return(0);
}

/*
 * Allocate a jumbo buffer.
 */
void *
sk_jalloc(struct sk_if_softc *sc_if)
{
	struct sk_jpool_entry   *entry;

	entry = LIST_FIRST(&sc_if->sk_jfree_listhead);

	if (entry == NULL) {
		DPRINTF(("%s: no free jumbo buffers\n",
		    sc_if->sk_dev.dv_xname));
		return (NULL);
	}

	LIST_REMOVE(entry, jpool_entries);
	LIST_INSERT_HEAD(&sc_if->sk_jinuse_listhead, entry, jpool_entries);
	return (sc_if->sk_cdata.sk_jslots[entry->slot]);
}

/*
 * Release a jumbo buffer.
 */
void
sk_jfree(caddr_t buf, u_int size, void	*arg)
{
	struct sk_jpool_entry *entry;
	struct sk_if_softc *sc;
	int i;

	/* Extract the softc struct pointer. */
	sc = (struct sk_if_softc *)arg;

	if (sc == NULL)
		panic("sk_jfree: can't find softc pointer!");

	/* calculate the slot this buffer belongs to */

	i = ((vaddr_t)buf
	     - (vaddr_t)sc->sk_cdata.sk_jumbo_buf) / SK_JLEN;

	if ((i < 0) || (i >= SK_JSLOTS))
		panic("sk_jfree: asked to free buffer that we don't manage!");

	entry = LIST_FIRST(&sc->sk_jinuse_listhead);
	if (entry == NULL)
		panic("sk_jfree: buffer not in use!");
	entry->slot = i;
	LIST_REMOVE(entry, jpool_entries);
	LIST_INSERT_HEAD(&sc->sk_jfree_listhead, entry, jpool_entries);
}

/*
 * Set media options.
 */
int
sk_ifmedia_upd(struct ifnet *ifp)
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
sk_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct sk_if_softc *sc_if = ifp->if_softc;

	mii_pollstat(&sc_if->sk_mii);
	ifmr->ifm_active = sc_if->sk_mii.mii_media_active;
	ifmr->ifm_status = sc_if->sk_mii.mii_media_status;
}

int
sk_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct sk_if_softc *sc_if = ifp->if_softc;
	struct sk_softc *sc = sc_if->sk_softc;
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
		if (ifr->ifr_mtu > ETHERMTU_JUMBO)
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
				switch(sc->sk_type) {
				case SK_GENESIS:
					SK_XM_SETBIT_4(sc_if, XM_MODE,
					    XM_MODE_RX_PROMISC);
					break;
				case SK_YUKON:
					SK_YU_CLRBIT_2(sc_if, YUKON_RCR,
					    YU_RCR_UFLEN | YU_RCR_MUFLEN);
					break;
				}
				sk_setmulti(sc_if);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc_if->sk_if_flags & IFF_PROMISC) {
				switch(sc->sk_type) {
				case SK_GENESIS:
					SK_XM_CLRBIT_4(sc_if, XM_MODE,
					    XM_MODE_RX_PROMISC);
					break;
				case SK_YUKON:
					SK_YU_SETBIT_2(sc_if, YUKON_RCR,
					    YU_RCR_UFLEN | YU_RCR_MUFLEN);
					break;
				}

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
skc_probe(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, skc_devices,
	    sizeof(skc_devices)/sizeof(skc_devices[0])));
}

/*
 * Force the GEnesis into reset, then bring it out of reset.
 */
void sk_reset(struct sk_softc *sc)
{
	DPRINTFN(2, ("sk_reset\n"));

	CSR_WRITE_2(sc, SK_CSR, SK_CSR_SW_RESET);
	CSR_WRITE_2(sc, SK_CSR, SK_CSR_MASTER_RESET);
	if (sc->sk_type == SK_YUKON)
		CSR_WRITE_2(sc, SK_LINK_CTRL, SK_LINK_RESET_SET);

	DELAY(1000);
	CSR_WRITE_2(sc, SK_CSR, SK_CSR_SW_UNRESET);
	DELAY(2);
	CSR_WRITE_2(sc, SK_CSR, SK_CSR_MASTER_UNRESET);
	if (sc->sk_type == SK_YUKON)
		CSR_WRITE_2(sc, SK_LINK_CTRL, SK_LINK_RESET_CLEAR);

	DPRINTFN(2, ("sk_reset: sk_csr=%x\n", CSR_READ_2(sc, SK_CSR)));
	DPRINTFN(2, ("sk_reset: sk_link_ctrl=%x\n",
		     CSR_READ_2(sc, SK_LINK_CTRL)));

	if (sc->sk_type == SK_GENESIS) {
		/* Configure packet arbiter */
		sk_win_write_2(sc, SK_PKTARB_CTL, SK_PKTARBCTL_UNRESET);
		sk_win_write_2(sc, SK_RXPA1_TINIT, SK_PKTARB_TIMEOUT);
		sk_win_write_2(sc, SK_TXPA1_TINIT, SK_PKTARB_TIMEOUT);
		sk_win_write_2(sc, SK_RXPA2_TINIT, SK_PKTARB_TIMEOUT);
		sk_win_write_2(sc, SK_TXPA2_TINIT, SK_PKTARB_TIMEOUT);
	}

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
}

int
sk_probe(struct device *parent, void *match, void *aux)
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
sk_attach(struct device *parent, struct device *self, void *aux)
{
	struct sk_if_softc *sc_if = (struct sk_if_softc *) self;
	struct sk_softc *sc = (struct sk_softc *)parent;
	struct skc_attach_args *sa = aux;
	struct ifnet *ifp;
	caddr_t kva;
	bus_dma_segment_t seg;
	int i, rseg;

	sc_if->sk_port = sa->skc_port;
	sc_if->sk_softc = sc;
	sc->sk_if[sa->skc_port] = sc_if;

	if (sa->skc_port == SK_PORT_A)
		sc_if->sk_tx_bmu = SK_BMU_TXS_CSR0;
	if (sa->skc_port == SK_PORT_B)
		sc_if->sk_tx_bmu = SK_BMU_TXS_CSR1;

	DPRINTFN(2, ("begin sk_attach: port=%d\n", sc_if->sk_port));

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
	 * dual port NIC. Our algorithm is to divide up the memory
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

	DPRINTFN(2, ("sk_attach: rx_ramstart=%#x rx_ramend=%#x\n"
		     "           tx_ramstart=%#x tx_ramend=%#x\n",
		     sc_if->sk_rx_ramstart, sc_if->sk_rx_ramend,
		     sc_if->sk_tx_ramstart, sc_if->sk_tx_ramend));

	/* Read and save PHY type and set PHY address */
	sc_if->sk_phytype = sk_win_read_1(sc, SK_EPROM1) & 0xF;
	switch (sc_if->sk_phytype) {
	case SK_PHYTYPE_XMAC:
		sc_if->sk_phyaddr = SK_PHYADDR_XMAC;
		break;
	case SK_PHYTYPE_BCOM:
		sc_if->sk_phyaddr = SK_PHYADDR_BCOM;
		break;
	case SK_PHYTYPE_MARV_COPPER:
		sc_if->sk_phyaddr = SK_PHYADDR_MARV;
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
	    sizeof(struct sk_ring_data), 0, BUS_DMA_NOWAIT,
            &sc_if->sk_ring_map)) {
		printf("%s: can't create dma map\n", sc_if->sk_dev.dv_xname);
		bus_dmamem_unmap(sc->sc_dmatag, kva,
		    sizeof(struct sk_ring_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
	if (bus_dmamap_load(sc->sc_dmatag, sc_if->sk_ring_map, kva,
	    sizeof(struct sk_ring_data), NULL, BUS_DMA_NOWAIT)) {
		printf("%s: can't load dma map\n", sc_if->sk_dev.dv_xname);
		bus_dmamap_destroy(sc->sc_dmatag, sc_if->sk_ring_map);
		bus_dmamem_unmap(sc->sc_dmatag, kva,
		    sizeof(struct sk_ring_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
        sc_if->sk_rdata = (struct sk_ring_data *)kva;
	bzero(sc_if->sk_rdata, sizeof(struct sk_ring_data));

	/* Try to allocate memory for jumbo buffers. */
	if (sk_alloc_jumbo_mem(sc_if)) {
		printf("%s: jumbo buffer allocation failed\n", ifp->if_xname);
		goto fail;
	}

	ifp = &sc_if->arpcom.ac_if;
	ifp->if_softc = sc_if;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = sk_ioctl;
	ifp->if_start = sk_start;
	ifp->if_watchdog = sk_watchdog;
	ifp->if_baudrate = 1000000000;
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, SK_TX_RING_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc_if->sk_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/*
	 * Do miibus setup.
	 */
	switch (sc->sk_type) {
	case SK_GENESIS:
		sk_init_xmac(sc_if);
		break;
	case SK_YUKON:
		sk_init_yukon(sc_if);
		break;
	default:
		panic("%s: unknown device type %d", sc->sk_dev.dv_xname,
		      sc->sk_type);
	}

 	DPRINTFN(2, ("sk_attach: 1\n"));

	sc_if->sk_mii.mii_ifp = ifp;
	switch (sc->sk_type) {
	case SK_GENESIS:
		sc_if->sk_mii.mii_readreg = sk_xmac_miibus_readreg;
		sc_if->sk_mii.mii_writereg = sk_xmac_miibus_writereg;
		sc_if->sk_mii.mii_statchg = sk_xmac_miibus_statchg;
		break;
	case SK_YUKON:
		sc_if->sk_mii.mii_readreg = sk_marv_miibus_readreg;
		sc_if->sk_mii.mii_writereg = sk_marv_miibus_writereg;
		sc_if->sk_mii.mii_statchg = sk_marv_miibus_statchg;
		break;
	}

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

	DPRINTFN(2, ("sk_attach: 1\n"));

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	DPRINTFN(2, ("sk_attach: end\n"));

	return;

fail:
	sc->sk_if[sa->skc_port] = NULL;
}

int
skcprint(void *aux, const char *pnp)
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
skc_attach(struct device *parent, struct device *self, void *aux)
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

	DPRINTFN(2, ("begin skc_attach\n"));

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

#define SK_MK_ID(vnd,prd) \
    (((vnd) << PCI_VENDOR_SHIFT) | ((prd) << PCI_PRODUCT_SHIFT))

	switch (pa->pa_id) {
	case SK_MK_ID(PCI_VENDOR_SCHNEIDERKOCH, PCI_PRODUCT_SCHNEIDERKOCH_GE):
		sc->sk_type = SK_GENESIS;
		break;
	case SK_MK_ID(PCI_VENDOR_3COM, PCI_PRODUCT_3COM_3C940):
	case SK_MK_ID(PCI_VENDOR_DLINK, PCI_PRODUCT_DLINK_DGE530T):
	case SK_MK_ID(PCI_VENDOR_LINKSYS, PCI_PRODUCT_LINKSYS_EG1032):
	case SK_MK_ID(PCI_VENDOR_LINKSYS, PCI_PRODUCT_LINKSYS_EG1064):
	case SK_MK_ID(PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_SK_V2):
	case SK_MK_ID(PCI_VENDOR_MARVELL, PCI_PRODUCT_MARVELL_SK_V2_BELKIN):
	case SK_MK_ID(PCI_VENDOR_SCHNEIDERKOCH, PCI_PRODUCT_SCHNEIDERKOCH_SK9821v2):
		sc->sk_type = SK_YUKON;
		break;
	default:
		printf(": unknown device!\n");
		goto fail;
	}
#undef SK_MK_ID

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

	DPRINTFN(2, ("skc_attach: iobase=%#x, iosize=%#x\n", iobase, iosize));
#endif
	sc->sc_dmatag = pa->pa_dmat;

	DPRINTFN(2, ("skc_attach: allocate interrupt\n"));

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

	if (sc->sk_type == SK_GENESIS) {
		u_int8_t val = sk_win_read_1(sc, SK_EPROM0);
		/* Read and save RAM size and RAMbuffer offset */
		switch(val) {
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
			       sc->sk_dev.dv_xname, val);
			goto fail;
			break;
		}
	} else {
		u_int8_t val = sk_win_read_1(sc, SK_EPROM0);
		sc->sk_ramsize =  ( val == 0 ) ?  0x20000 : (( val * 4 )*1024);
		sc->sk_rboff = SK_RBOFF_0;
	}

	DPRINTFN(2, ("skc_attach: ramsize=%d (%dk), rboff=%d\n",
		     sc->sk_ramsize, sc->sk_ramsize / 1024,
		     sc->sk_rboff));

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

int
sk_encap(struct sk_if_softc *sc_if, struct mbuf *m_head, u_int32_t *txidx)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct sk_tx_desc	*f = NULL;
	u_int32_t		frag, cur, cnt = 0;
	int			i;
	struct sk_txmap_entry	*entry;
	bus_dmamap_t		txmap;

	DPRINTFN(2, ("sk_encap\n"));

	entry = SLIST_FIRST(&sc_if->sk_txmap_listhead);
	if (entry == NULL) {
		DPRINTFN(2, ("sk_encap: no txmap available\n"));
		return ENOBUFS;
	}
	txmap = entry->dmamap;

	cur = frag = *txidx;

#ifdef SK_DEBUG
	if (skdebug >= 2)
		sk_dump_mbuf(m_head);
#endif

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	if (bus_dmamap_load_mbuf(sc->sc_dmatag, txmap, m_head,
	    BUS_DMA_NOWAIT)) {
		DPRINTFN(2, ("sk_encap: dmamap failed\n"));
		return(ENOBUFS);
	}

	DPRINTFN(2, ("sk_encap: dm_nsegs=%d\n", txmap->dm_nsegs));

	for (i = 0; i < txmap->dm_nsegs; i++) {
		if ((SK_TX_RING_CNT - (sc_if->sk_cdata.sk_tx_cnt + cnt)) < 2) {
			DPRINTFN(2, ("sk_encap: too few descriptors free\n"));
			return(ENOBUFS);
		}
		f = &sc_if->sk_rdata->sk_tx_ring[frag];
		f->sk_data_lo = txmap->dm_segs[i].ds_addr;
		f->sk_ctl = txmap->dm_segs[i].ds_len | SK_OPCODE_DEFAULT;
		if (cnt == 0)
			f->sk_ctl |= SK_TXCTL_FIRSTFRAG;
		else
			f->sk_ctl |= SK_TXCTL_OWN;

		cur = frag;
		SK_INC(frag, SK_TX_RING_CNT);
		cnt++;
	}

	sc_if->sk_cdata.sk_tx_chain[cur].sk_mbuf = m_head;
	SLIST_REMOVE_HEAD(&sc_if->sk_txmap_listhead, link);
	sc_if->sk_cdata.sk_tx_map[cur] = entry;
	sc_if->sk_rdata->sk_tx_ring[cur].sk_ctl |=
		SK_TXCTL_LASTFRAG|SK_TXCTL_EOF_INTR;
	sc_if->sk_rdata->sk_tx_ring[*txidx].sk_ctl |= SK_TXCTL_OWN;
	sc_if->sk_cdata.sk_tx_cnt += cnt;

#ifdef SK_DEBUG
	if (skdebug >= 2) {
		struct sk_tx_desc *desc;
		u_int32_t idx;
		for (idx = *txidx; idx != frag; SK_INC(idx, SK_TX_RING_CNT)) {
			desc = &sc_if->sk_rdata->sk_tx_ring[idx];
			sk_dump_txdesc(desc, idx);
		}
	}
#endif

	*txidx = frag;

	DPRINTFN(2, ("sk_encap: completed successfully\n"));

	return(0);
}

void
sk_start(struct ifnet *ifp)
{
        struct sk_if_softc	*sc_if = ifp->if_softc;
        struct sk_softc		*sc = sc_if->sk_softc;
        struct mbuf		*m_head = NULL;
        u_int32_t		idx = sc_if->sk_cdata.sk_tx_prod;
	int			pkts = 0;

	DPRINTFN(2, ("sk_start\n"));

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
}


void
sk_watchdog(struct ifnet *ifp)
{
	struct sk_if_softc *sc_if = ifp->if_softc;

	printf("%s: watchdog timeout\n", sc_if->sk_dev.dv_xname);
	sk_init(sc_if);
}

void
sk_shutdown(void *v)
{
	struct sk_softc		*sc = v;

	DPRINTFN(2, ("sk_shutdown\n"));

	/* Turn off the 'driver is loaded' LED. */
	CSR_WRITE_2(sc, SK_LED, SK_LED_GREEN_OFF);

	/*
	 * Reset the GEnesis controller. Doing this should also
	 * assert the resets on the attached XMAC(s).
	 */
	sk_reset(sc);
}

void
sk_rxeof(struct sk_if_softc *sc_if)
{
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	struct mbuf		*m;
	struct sk_chain		*cur_rx;
	struct sk_rx_desc	*cur_desc;
	int			i, cur, total_len = 0;
	u_int32_t		rxstat;
	bus_dmamap_t		dmamap;
	u_int16_t		csum1, csum2;

	DPRINTFN(2, ("sk_rxeof\n"));

	i = sc_if->sk_cdata.sk_rx_prod;

	while(!(sc_if->sk_rdata->sk_rx_ring[i].sk_ctl & SK_RXCTL_OWN)) {
		cur = i;
		cur_rx = &sc_if->sk_cdata.sk_rx_chain[cur];
		cur_desc = &sc_if->sk_rdata->sk_rx_ring[cur];

		rxstat = cur_desc->sk_xmac_rxstat;
		m = cur_rx->sk_mbuf;
		cur_rx->sk_mbuf = NULL;
		total_len = SK_RXBYTES(cur_desc->sk_ctl);

		dmamap = sc_if->sk_cdata.sk_rx_jumbo_map;

		csum1 = sc_if->sk_rdata->sk_rx_ring[i].sk_csum1;
		csum2 = sc_if->sk_rdata->sk_rx_ring[i].sk_csum2;

		SK_INC(i, SK_RX_RING_CNT);

		if (rxstat & XM_RXSTAT_ERRFRAME) {
			ifp->if_ierrors++;
			sk_newbuf(sc_if, cur, m, dmamap);
			continue;
		}

		/*
		 * Try to allocate a new jumbo buffer. If that
		 * fails, copy the packet to mbufs and put the
		 * jumbo buffer back in the ring so it can be
		 * re-used. If allocating mbufs fails, then we
		 * have to drop the packet.
		 */
		if (sk_newbuf(sc_if, cur, NULL, dmamap) == ENOBUFS) {
			struct mbuf		*m0;
			m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
			    total_len + ETHER_ALIGN, 0, ifp, NULL);
			sk_newbuf(sc_if, cur, m, dmamap);
			if (m0 == NULL) {
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

		sk_rxcsum(ifp, m, csum1, csum2);

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* pass it on. */
		ether_input_mbuf(ifp, m);
	}

	sc_if->sk_cdata.sk_rx_prod = i;
}

void
sk_rxcsum(struct ifnet *ifp, struct mbuf *m, const u_int16_t csum1, const u_int16_t csum2)
{
	struct ether_header *eh;
	struct ip *ip;
	u_int8_t *pp;
	int hlen, len, plen;
	u_int16_t iph_csum, ipo_csum, ipd_csum, csum;

	pp = mtod(m, u_int8_t *);
	plen = m->m_pkthdr.len;
	if (plen < sizeof(*eh))
		return;
	eh = (struct ether_header *)pp;
	iph_csum = in_cksum_addword(csum1, (~csum2 & 0xffff));

	if (eh->ether_type == htons(ETHERTYPE_VLAN)) {
		u_int16_t *xp = (u_int16_t *)pp;

		xp = (u_int16_t *)pp;
		if (xp[1] != htons(ETHERTYPE_IP))
			return;
		iph_csum = in_cksum_addword(iph_csum, (~xp[0] & 0xffff));
		iph_csum = in_cksum_addword(iph_csum, (~xp[1] & 0xffff));
		xp = (u_int16_t *)(pp + sizeof(struct ip));
		iph_csum = in_cksum_addword(iph_csum, xp[0]);
		iph_csum = in_cksum_addword(iph_csum, xp[1]);
		pp += EVL_ENCAPLEN;
	} else if (eh->ether_type != htons(ETHERTYPE_IP))
		return;

	pp += sizeof(*eh);
	plen -= sizeof(*eh);

	ip = (struct ip *)pp;

	if (ip->ip_v != IPVERSION)
		return;

	hlen = ip->ip_hl << 2;
	if (hlen < sizeof(struct ip))
		return;
	if (hlen > ntohs(ip->ip_len))
		return;

	/* Don't deal with truncated or padded packets. */
	if (plen != ntohs(ip->ip_len))
		return;

	len = hlen - sizeof(struct ip);
	if (len > 0) {
		u_int16_t *p;

		p = (u_int16_t *)(ip + 1);
		ipo_csum = 0;
		for (ipo_csum = 0; len > 0; len -= sizeof(*p), p++)
			ipo_csum = in_cksum_addword(ipo_csum, *p);
		iph_csum = in_cksum_addword(iph_csum, ipo_csum);
		ipd_csum = in_cksum_addword(csum2, (~ipo_csum & 0xffff));
	} else
		ipd_csum = csum2;

	if (iph_csum != 0xffff) {
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
		return;
	}
	m->m_pkthdr.csum |= M_IPV4_CSUM_IN_OK;

	if (ip->ip_off & htons(IP_MF | IP_OFFMASK))
		return;                 /* ip frag, we're done for now */

	pp += hlen;

	/* Only know checksum protocol for udp/tcp */
	if (ip->ip_p == IPPROTO_UDP) {
		struct udphdr *uh = (struct udphdr *)pp;

		if (uh->uh_sum == 0)    /* udp with no checksum */
			return;
	} else if (ip->ip_p != IPPROTO_TCP)
		return;

	csum = in_cksum_phdr(ip->ip_src.s_addr, ip->ip_dst.s_addr,
	    htonl(ntohs(ip->ip_len) - hlen + ip->ip_p) + ipd_csum);
	if (csum == 0xffff) {
		m->m_pkthdr.csum |= (ip->ip_p == IPPROTO_TCP) ?
		    M_TCP_CSUM_IN_OK : M_UDP_CSUM_IN_OK;
	}
}

void
sk_txeof(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct sk_tx_desc	*cur_tx = NULL;
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	u_int32_t		idx;
	struct sk_txmap_entry	*entry;

	DPRINTFN(2, ("sk_txeof\n"));

	/*
	 * Go through our tx ring and free mbufs for those
	 * frames that have been sent.
	 */
	idx = sc_if->sk_cdata.sk_tx_cons;
	while(idx != sc_if->sk_cdata.sk_tx_prod) {
		cur_tx = &sc_if->sk_rdata->sk_tx_ring[idx];
#ifdef SK_DEBUG
		if (skdebug >= 2)
			sk_dump_txdesc(cur_tx, idx);
#endif
		if (cur_tx->sk_ctl & SK_TXCTL_OWN)
			break;
		if (cur_tx->sk_ctl & SK_TXCTL_LASTFRAG)
			ifp->if_opackets++;
		if (sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf != NULL) {
			m_freem(sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf);
			sc_if->sk_cdata.sk_tx_chain[idx].sk_mbuf = NULL;

			entry = sc_if->sk_cdata.sk_tx_map[idx];
			bus_dmamap_sync(sc->sc_dmatag, entry->dmamap, 0,
			    entry->dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);

			bus_dmamap_unload(sc->sc_dmatag, entry->dmamap);
			SLIST_INSERT_HEAD(&sc_if->sk_txmap_listhead, entry,
					  link);
			sc_if->sk_cdata.sk_tx_map[idx] = NULL;
		}
		sc_if->sk_cdata.sk_tx_cnt--;
		SK_INC(idx, SK_TX_RING_CNT);
	}
	if (sc_if->sk_cdata.sk_tx_cnt == 0)
		ifp->if_timer = 0;
	else /* nudge chip to keep tx ring moving */
		CSR_WRITE_4(sc, sc_if->sk_tx_bmu, SK_TXBMU_TX_START);

	sc_if->sk_cdata.sk_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;
}

void
sk_tick(void *xsc_if)
{
	struct sk_if_softc *sc_if = xsc_if;
	struct mii_data *mii = &sc_if->sk_mii;
	struct ifnet *ifp = &sc_if->arpcom.ac_if;
	int i;

	DPRINTFN(2, ("sk_tick\n"));

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
sk_intr_bcom(struct sk_if_softc *sc_if)
{
	struct mii_data *mii = &sc_if->sk_mii;
	struct ifnet *ifp = &sc_if->arpcom.ac_if;
	int status;


	DPRINTFN(2, ("sk_intr_bcom\n"));

	SK_XM_CLRBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);

	/*
	 * Read the PHY interrupt register to make sure
	 * we clear any pending interrupts.
	 */
	status = sk_xmac_miibus_readreg((struct device *)sc_if,
	    SK_PHYADDR_BCOM, BRGPHY_MII_ISR);

	if (!(ifp->if_flags & IFF_RUNNING)) {
		sk_init_xmac(sc_if);
		return;
	}

	if (status & (BRGPHY_ISR_LNK_CHG|BRGPHY_ISR_AN_PR)) {
		int lstat;
		lstat = sk_xmac_miibus_readreg((struct device *)sc_if,
		    SK_PHYADDR_BCOM, BRGPHY_MII_AUXSTS);

		if (!(lstat & BRGPHY_AUXSTS_LINK) && sc_if->sk_link) {
			mii_mediachg(mii);
			/* Turn off the link LED. */
			SK_IF_WRITE_1(sc_if, 0,
			    SK_LINKLED1_CTL, SK_LINKLED_OFF);
			sc_if->sk_link = 0;
		} else if (status & BRGPHY_ISR_LNK_CHG) {
			sk_xmac_miibus_writereg((struct device *)sc_if,
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
}

void
sk_intr_xmac(struct sk_if_softc	*sc_if)
{
	u_int16_t status = SK_XM_READ_2(sc_if, XM_ISR);

	DPRINTFN(2, ("sk_intr_xmac\n"));

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
}

void
sk_intr_yukon(sc_if)
	struct sk_if_softc *sc_if;
{
	int status;

	status = SK_IF_READ_2(sc_if, 0, SK_GMAC_ISR);

	DPRINTFN(2, ("sk_intr_yukon status=%#x\n", status));
}

int
sk_intr(void *xsc)
{
	struct sk_softc		*sc = xsc;
	struct sk_if_softc	*sc_if0 = sc->sk_if[SK_PORT_A];
	struct sk_if_softc	*sc_if1 = sc->sk_if[SK_PORT_B];
	struct ifnet		*ifp0 = NULL, *ifp1 = NULL;
	u_int32_t		status;
	int			claimed = 0;

	if (sc_if0 != NULL)
		ifp0 = &sc_if0->arpcom.ac_if;
	if (sc_if1 != NULL)
		ifp1 = &sc_if1->arpcom.ac_if;

	for (;;) {
		status = CSR_READ_4(sc, SK_ISSR);
		DPRINTFN(2, ("sk_intr: status=%#x\n", status));

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
		if (status & SK_ISR_MAC1 && (ifp0->if_flags & IFF_RUNNING)) {
			if (sc->sk_type == SK_GENESIS)
				sk_intr_xmac(sc_if0);
			else
				sk_intr_yukon(sc_if0);
		}

		if (status & SK_ISR_MAC2 && (ifp1->if_flags & IFF_RUNNING)) {
			if (sc->sk_type == SK_GENESIS)
				sk_intr_xmac(sc_if1);
			else
				sk_intr_yukon(sc_if1);

		}

		if (status & SK_ISR_EXTERNAL_REG) {
			if (ifp0 != NULL &&
			    sc_if0->sk_phytype == SK_PHYTYPE_BCOM)
				sk_intr_bcom(sc_if0);

			if (ifp1 != NULL &&
			    sc_if1->sk_phytype == SK_PHYTYPE_BCOM)
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

void
sk_init_xmac(struct sk_if_softc	*sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	struct sk_bcom_hack     bhack[] = {
	{ 0x18, 0x0c20 }, { 0x17, 0x0012 }, { 0x15, 0x1104 }, { 0x17, 0x0013 },
	{ 0x15, 0x0404 }, { 0x17, 0x8006 }, { 0x15, 0x0132 }, { 0x17, 0x8006 },
	{ 0x15, 0x0232 }, { 0x17, 0x800D }, { 0x15, 0x000F }, { 0x18, 0x0420 },
	{ 0, 0 } };

	DPRINTFN(2, ("sk_init_xmac\n"));

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

		sk_xmac_miibus_writereg((struct device *)sc_if,
		    SK_PHYADDR_BCOM, BRGPHY_MII_BMCR, BRGPHY_BMCR_RESET);
		DELAY(10000);
		sk_xmac_miibus_writereg((struct device *)sc_if,
		    SK_PHYADDR_BCOM, BRGPHY_MII_IMR, 0xFFF0);

		/*
		 * Early versions of the BCM5400 apparently have
		 * a bug that requires them to have their reserved
		 * registers initialized to some magic values. I don't
		 * know what the numbers do, I'm just the messenger.
		 */
		if (sk_xmac_miibus_readreg((struct device *)sc_if,
		    SK_PHYADDR_BCOM, 0x03) == 0x6041) {
			while(bhack[i].reg) {
				sk_xmac_miibus_writereg((struct device *)sc_if,
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
}

void sk_init_yukon(sc_if)
	struct sk_if_softc	*sc_if;
{
	u_int32_t		/*mac, */phy;
	u_int16_t		reg;
	int			i;

	DPRINTFN(2, ("sk_init_yukon: start: sk_csr=%#x\n",
		     CSR_READ_4(sc_if->sk_softc, SK_CSR)));

	/* GMAC and GPHY Reset */
	SK_IF_WRITE_4(sc_if, 0, SK_GPHY_CTRL, SK_GPHY_RESET_SET);

	DPRINTFN(6, ("sk_init_yukon: 1\n"));

	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_RESET_SET);
	DELAY(1000);
	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_RESET_SET);
	DELAY(1000);


	DPRINTFN(6, ("sk_init_yukon: 2\n"));

	phy = SK_GPHY_INT_POL_HI | SK_GPHY_DIS_FC | SK_GPHY_DIS_SLEEP |
		SK_GPHY_ENA_XC | SK_GPHY_ANEG_ALL | SK_GPHY_ENA_PAUSE;

	switch(sc_if->sk_softc->sk_pmd) {
	case IFM_1000_SX:
	case IFM_1000_LX:
		phy |= SK_GPHY_FIBER;
		break;

	case IFM_1000_CX:
	case IFM_1000_T:
		phy |= SK_GPHY_COPPER;
		break;
	}

	DPRINTFN(3, ("sk_init_yukon: phy=%#x\n", phy));

	SK_IF_WRITE_4(sc_if, 0, SK_GPHY_CTRL, phy | SK_GPHY_RESET_SET);
	DELAY(1000);
	SK_IF_WRITE_4(sc_if, 0, SK_GPHY_CTRL, phy | SK_GPHY_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_GMAC_CTRL, SK_GMAC_LOOP_OFF |
		      SK_GMAC_PAUSE_ON | SK_GMAC_RESET_CLEAR);

	DPRINTFN(3, ("sk_init_yukon: gmac_ctrl=%#x\n",
		     SK_IF_READ_4(sc_if, 0, SK_GMAC_CTRL)));

	DPRINTFN(6, ("sk_init_yukon: 3\n"));

	/* unused read of the interrupt source register */
	DPRINTFN(6, ("sk_init_yukon: 4\n"));
	SK_IF_READ_2(sc_if, 0, SK_GMAC_ISR);

	DPRINTFN(6, ("sk_init_yukon: 4a\n"));
	reg = SK_YU_READ_2(sc_if, YUKON_PAR);
	DPRINTFN(6, ("sk_init_yukon: YUKON_PAR=%#x\n", reg));

	/* MIB Counter Clear Mode set */
        reg |= YU_PAR_MIB_CLR;
	DPRINTFN(6, ("sk_init_yukon: YUKON_PAR=%#x\n", reg));
	DPRINTFN(6, ("sk_init_yukon: 4b\n"));
	SK_YU_WRITE_2(sc_if, YUKON_PAR, reg);
        
	/* MIB Counter Clear Mode clear */
	DPRINTFN(6, ("sk_init_yukon: 5\n"));
        reg &= ~YU_PAR_MIB_CLR;
	SK_YU_WRITE_2(sc_if, YUKON_PAR, reg);

	/* receive control reg */
	DPRINTFN(6, ("sk_init_yukon: 7\n"));
	SK_YU_WRITE_2(sc_if, YUKON_RCR, YU_RCR_UFLEN | YU_RCR_MUFLEN |
		      YU_RCR_CRCR);

	/* transmit parameter register */
	DPRINTFN(6, ("sk_init_yukon: 8\n"));
	SK_YU_WRITE_2(sc_if, YUKON_TPR, YU_TPR_JAM_LEN(0x3) |
		      YU_TPR_JAM_IPG(0xb) | YU_TPR_JAM2DATA_IPG(0x1a) );

	/* serial mode register */
	DPRINTFN(6, ("sk_init_yukon: 9\n"));
	SK_YU_WRITE_2(sc_if, YUKON_SMR, YU_SMR_DATA_BLIND(0x1c) |
		      YU_SMR_MFL_VLAN | YU_SMR_IPG_DATA(0x1e));

	DPRINTFN(6, ("sk_init_yukon: 10\n"));
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

	/* Set multicast filter */
	DPRINTFN(6, ("sk_init_yukon: 11\n"));
	sk_setmulti(sc_if);

	/* enable interrupt mask for counter overflows */
	DPRINTFN(6, ("sk_init_yukon: 12\n"));
	SK_YU_WRITE_2(sc_if, YUKON_TIMR, 0);
	SK_YU_WRITE_2(sc_if, YUKON_RIMR, 0);
	SK_YU_WRITE_2(sc_if, YUKON_TRIMR, 0);

	/* Configure RX MAC FIFO */
	SK_IF_WRITE_1(sc_if, 0, SK_RXMF1_CTRL_TEST, SK_RFCTL_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_RXMF1_CTRL_TEST, SK_RFCTL_OPERATION_ON);
	
	/* Configure TX MAC FIFO */
	SK_IF_WRITE_1(sc_if, 0, SK_TXMF1_CTRL_TEST, SK_TFCTL_RESET_CLEAR);
	SK_IF_WRITE_4(sc_if, 0, SK_TXMF1_CTRL_TEST, SK_TFCTL_OPERATION_ON);
		
	DPRINTFN(6, ("sk_init_yukon: end\n"));
}

/*
 * Note that to properly initialize any part of the GEnesis chip,
 * you first have to take it out of reset mode.
 */
void
sk_init(void *xsc_if)
{
	struct sk_if_softc	*sc_if = xsc_if;
	struct sk_softc		*sc = sc_if->sk_softc;
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	struct mii_data		*mii = &sc_if->sk_mii;
	int			s;

	DPRINTFN(2, ("sk_init\n"));

	s = splimp();

	/* Cancel pending I/O and free all RX/TX buffers. */
	sk_stop(sc_if);

	if (sc->sk_type == SK_GENESIS) {
		/* Configure LINK_SYNC LED */
		SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL, SK_LINKLED_ON);
		SK_IF_WRITE_1(sc_if, 0, SK_LINKLED1_CTL,
			      SK_LINKLED_LINKSYNC_ON);

		/* Configure RX LED */
		SK_IF_WRITE_1(sc_if, 0, SK_RXLED1_CTL,
			      SK_RXLEDCTL_COUNTER_START);
		
		/* Configure TX LED */
		SK_IF_WRITE_1(sc_if, 0, SK_TXLED1_CTL,
			      SK_TXLEDCTL_COUNTER_START);
	}

	/* Configure I2C registers */

	/* Configure XMAC(s) */
	switch (sc->sk_type) {
	case SK_GENESIS:
		sk_init_xmac(sc_if);
		break;
	case SK_YUKON:
		sk_init_yukon(sc_if);
		break;
	}
	mii_mediachg(mii);

	if (sc->sk_type == SK_GENESIS) {
		/* Configure MAC FIFOs */
		SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_UNRESET);
		SK_IF_WRITE_4(sc_if, 0, SK_RXF1_END, SK_FIFO_END);
		SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_ON);
		
		SK_IF_WRITE_4(sc_if, 0, SK_TXF1_CTL, SK_FIFO_UNRESET);
		SK_IF_WRITE_4(sc_if, 0, SK_TXF1_END, SK_FIFO_END);
		SK_IF_WRITE_4(sc_if, 0, SK_TXF1_CTL, SK_FIFO_ON);
	}

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
	    SK_RX_RING_ADDR(sc_if, 0));
	SK_IF_WRITE_4(sc_if, 0, SK_RXQ1_CURADDR_HI, 0);

	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_BMU_CSR, SK_TXBMU_ONLINE);
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_CURADDR_LO,
            SK_TX_RING_ADDR(sc_if, 0));
	SK_IF_WRITE_4(sc_if, 1, SK_TXQS1_CURADDR_HI, 0);

	/* Init descriptors */
	if (sk_init_rx_ring(sc_if) == ENOBUFS) {
		printf("%s: initialization failed: no "
		    "memory for rx buffers\n", sc_if->sk_dev.dv_xname);
		sk_stop(sc_if);
		splx(s);
		return;
	}

	if (sk_init_tx_ring(sc_if) == ENOBUFS) {
		printf("%s: initialization failed: no "
		    "memory for tx buffers\n", sc_if->sk_dev.dv_xname);
		sk_stop(sc_if);
		splx(s);
		return;
	}

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

	if (sc->sk_type == SK_GENESIS) {
		/* Enable XMACs TX and RX state machines */
		SK_XM_CLRBIT_2(sc_if, XM_MMUCMD, XM_MMUCMD_IGNPAUSE);
		SK_XM_SETBIT_2(sc_if, XM_MMUCMD,
			       XM_MMUCMD_TX_ENB|XM_MMUCMD_RX_ENB);
	}

	if (sc->sk_type == SK_YUKON) {
		u_int16_t reg = SK_YU_READ_2(sc_if, YUKON_GPCR);
		reg |= YU_GPCR_TXEN | YU_GPCR_RXEN;
		reg &= ~(YU_GPCR_SPEED_EN | YU_GPCR_DPLX_EN);
		SK_YU_WRITE_2(sc_if, YUKON_GPCR, reg);
	}


	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);
}

void
sk_stop(struct sk_if_softc *sc_if)
{
	struct sk_softc		*sc = sc_if->sk_softc;
	struct ifnet		*ifp = &sc_if->arpcom.ac_if;
	int			i;

	DPRINTFN(2, ("sk_stop\n"));

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
	switch (sc->sk_type) {
	case SK_GENESIS:
		SK_IF_WRITE_2(sc_if, 0, SK_TXF1_MACCTL,
			      SK_TXMACCTL_XMAC_RESET);
		SK_IF_WRITE_4(sc_if, 0, SK_RXF1_CTL, SK_FIFO_RESET);
		break;
	case SK_YUKON:
		SK_IF_WRITE_1(sc_if,0, SK_RXMF1_CTRL_TEST, SK_RFCTL_RESET_SET);
		SK_IF_WRITE_1(sc_if,0, SK_TXMF1_CTRL_TEST, SK_TFCTL_RESET_SET);
		break;
	}
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

#ifdef SK_DEBUG
void
sk_dump_txdesc(struct sk_tx_desc *desc, int idx)
{
#define DESC_PRINT(X)					\
	if (desc->X)					\
		printf("txdesc[%d]." #X "=%#x\n",	\
		       idx, desc->X);

	DESC_PRINT(sk_ctl);
	DESC_PRINT(sk_next);
	DESC_PRINT(sk_data_lo);
	DESC_PRINT(sk_data_hi);
	DESC_PRINT(sk_xmac_txstat);
	DESC_PRINT(sk_rsvd0);
	DESC_PRINT(sk_csum_startval);
	DESC_PRINT(sk_csum_startpos);
	DESC_PRINT(sk_csum_writepos);
	DESC_PRINT(sk_rsvd1);
#undef PRINT
}

void
sk_dump_bytes(const char *data, int len)
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
sk_dump_mbuf(struct mbuf *m)
{
	int count = m->m_pkthdr.len;

	printf("m=%#lx, m->m_pkthdr.len=%#d\n", m, m->m_pkthdr.len);

	while (count > 0 && m) {
		printf("m=%#lx, m->m_data=%#lx, m->m_len=%d\n",
		       m, m->m_data, m->m_len);
		sk_dump_bytes(mtod(m, char *), m->m_len);

		count -= m->m_len;
		m = m->m_next;
	}
}
#endif
