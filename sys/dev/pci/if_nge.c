/*	$OpenBSD: if_nge.c,v 1.28 2004/05/30 01:51:51 mcbride Exp $	*/
/*
 * Copyright (c) 2001 Wind River Systems
 * Copyright (c) 1997, 1998, 1999, 2000, 2001
 *	Bill Paul <wpaul@bsdi.com>.  All rights reserved.
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
 * $FreeBSD: if_nge.c,v 1.35 2002/08/08 18:33:28 ambrisko Exp $
 */

/*
 * National Semiconductor DP83820/DP83821 gigabit ethernet driver
 * for FreeBSD. Datasheets are available from:
 *
 * http://www.national.com/ds/DP/DP83820.pdf
 * http://www.national.com/ds/DP/DP83821.pdf
 *
 * These chips are used on several low cost gigabit ethernet NICs
 * sold by D-Link, Addtron, SMC and Asante. Both parts are
 * virtually the same, except the 83820 is a 64-bit/32-bit part,
 * while the 83821 is 32-bit only.
 *
 * Many cards also use National gigE transceivers, such as the
 * DP83891, DP83861 and DP83862 gigPHYTER parts. The DP83861 datasheet
 * contains a full register description that applies to all of these
 * components:
 *
 * http://www.national.com/ds/DP/DP83861.pdf
 *
 * Written by Bill Paul <wpaul@bsdi.com>
 * BSDi Open Source Solutions
 */

/*
 * The NatSemi DP83820 and 83821 controllers are enhanced versions
 * of the NatSemi MacPHYTER 10/100 devices. They support 10, 100
 * and 1000Mbps speeds with 1000baseX (ten bit interface), MII and GMII
 * ports. Other features include 8K TX FIFO and 32K RX FIFO, TCP/IP
 * hardware checksum offload (IPv4 only), VLAN tagging and filtering,
 * priority TX and RX queues, a 2048 bit multicast hash filter, 4 RX pattern
 * matching buffers, one perfect address filter buffer and interrupt
 * moderation. The 83820 supports both 64-bit and 32-bit addressing
 * and data transfers: the 64-bit support can be toggled on or off
 * via software. This affects the size of certain fields in the DMA
 * descriptors.
 *
 * There are two bugs/misfeatures in the 83820/83821 that I have
 * discovered so far:
 *
 * - Receive buffers must be aligned on 64-bit boundaries, which means
 *   you must resort to copying data in order to fix up the payload
 *   alignment.
 *
 * - In order to transmit jumbo frames larger than 8170 bytes, you have
 *   to turn off transmit checksum offloading, because the chip can't
 *   compute the checksum on an outgoing frame unless it fits entirely
 *   within the TX FIFO, which is only 8192 bytes in size. If you have
 *   TX checksum offload enabled and you transmit attempt to transmit a
 *   frame larger than 8170 bytes, the transmitter will wedge.
 *
 * To work around the latter problem, TX checksum offload is disabled
 * if the user selects an MTU larger than 8152 (8170 - 18).
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

#include <uvm/uvm_extern.h>              /* for vtophys */

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#define NGE_USEIOSPACE

#include <dev/pci/if_ngereg.h>

int nge_probe(struct device *, void *, void *);
void nge_attach(struct device *, struct device *, void *);

int nge_alloc_jumbo_mem(struct nge_softc *);
void *nge_jalloc(struct nge_softc *);
void nge_jfree(caddr_t, u_int, void *);

int nge_newbuf(struct nge_softc *, struct nge_desc *,
			     struct mbuf *);
int nge_encap(struct nge_softc *, struct mbuf *, u_int32_t *);
void nge_rxeof(struct nge_softc *);
void nge_txeof(struct nge_softc *);
int nge_intr(void *);
void nge_tick(void *);
void nge_start(struct ifnet *);
int nge_ioctl(struct ifnet *, u_long, caddr_t);
void nge_init(void *);
void nge_stop(struct nge_softc *);
void nge_watchdog(struct ifnet *);
void nge_shutdown(void *);
int nge_ifmedia_mii_upd(struct ifnet *);
void nge_ifmedia_mii_sts(struct ifnet *, struct ifmediareq *);
int nge_ifmedia_tbi_upd(struct ifnet *);
void nge_ifmedia_tbi_sts(struct ifnet *, struct ifmediareq *);

void nge_delay(struct nge_softc *);
void nge_eeprom_idle(struct nge_softc *);
void nge_eeprom_putbyte(struct nge_softc *, int);
void nge_eeprom_getword(struct nge_softc *, int, u_int16_t *);
void nge_read_eeprom(struct nge_softc *, caddr_t, int, int, int);

void nge_mii_sync(struct nge_softc *);
void nge_mii_send(struct nge_softc *, u_int32_t, int);
int nge_mii_readreg(struct nge_softc *, struct nge_mii_frame *);
int nge_mii_writereg(struct nge_softc *, struct nge_mii_frame *);

int nge_miibus_readreg(struct device *, int, int);
void nge_miibus_writereg(struct device *, int, int, int);
void nge_miibus_statchg(struct device *);

void nge_setmulti(struct nge_softc *);
void nge_reset(struct nge_softc *);
int nge_list_rx_init(struct nge_softc *);
int nge_list_tx_init(struct nge_softc *);

#ifdef NGE_USEIOSPACE
#define NGE_RES			SYS_RES_IOPORT
#define NGE_RID			NGE_PCI_LOIO
#else
#define NGE_RES			SYS_RES_MEMORY
#define NGE_RID			NGE_PCI_LOMEM
#endif

#ifdef NGE_DEBUG
#define DPRINTF(x)	if (ngedebug) printf x
#define DPRINTFN(n,x)	if (ngedebug >= (n)) printf x
int	ngedebug = 0;
#else
#define DPRINTF(x)
#define DPRINTFN(n,x)
#endif

#define NGE_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) | (x))

#define NGE_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg,				\
		CSR_READ_4(sc, reg) & ~(x))

#define SIO_SET(x)					\
	CSR_WRITE_4(sc, NGE_MEAR, CSR_READ_4(sc, NGE_MEAR) | x)

#define SIO_CLR(x)					\
	CSR_WRITE_4(sc, NGE_MEAR, CSR_READ_4(sc, NGE_MEAR) & ~x)

void
nge_delay(sc)
	struct nge_softc	*sc;
{
	int			idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, NGE_CSR);
}

void
nge_eeprom_idle(sc)
	struct nge_softc	*sc;
{
	int		i;

	SIO_SET(NGE_MEAR_EE_CSEL);
	nge_delay(sc);
	SIO_SET(NGE_MEAR_EE_CLK);
	nge_delay(sc);

	for (i = 0; i < 25; i++) {
		SIO_CLR(NGE_MEAR_EE_CLK);
		nge_delay(sc);
		SIO_SET(NGE_MEAR_EE_CLK);
		nge_delay(sc);
	}

	SIO_CLR(NGE_MEAR_EE_CLK);
	nge_delay(sc);
	SIO_CLR(NGE_MEAR_EE_CSEL);
	nge_delay(sc);
	CSR_WRITE_4(sc, NGE_MEAR, 0x00000000);
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
void
nge_eeprom_putbyte(sc, addr)
	struct nge_softc	*sc;
	int			addr;
{
	int			d, i;

	d = addr | NGE_EECMD_READ;

	/*
	 * Feed in each bit and strobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			SIO_SET(NGE_MEAR_EE_DIN);
		} else {
			SIO_CLR(NGE_MEAR_EE_DIN);
		}
		nge_delay(sc);
		SIO_SET(NGE_MEAR_EE_CLK);
		nge_delay(sc);
		SIO_CLR(NGE_MEAR_EE_CLK);
		nge_delay(sc);
	}
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
void
nge_eeprom_getword(sc, addr, dest)
	struct nge_softc	*sc;
	int			addr;
	u_int16_t		*dest;
{
	int			i;
	u_int16_t		word = 0;

	/* Force EEPROM to idle state. */
	nge_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	nge_delay(sc);
	SIO_CLR(NGE_MEAR_EE_CLK);
	nge_delay(sc);
	SIO_SET(NGE_MEAR_EE_CSEL);
	nge_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	nge_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(NGE_MEAR_EE_CLK);
		nge_delay(sc);
		if (CSR_READ_4(sc, NGE_MEAR) & NGE_MEAR_EE_DOUT)
			word |= i;
		nge_delay(sc);
		SIO_CLR(NGE_MEAR_EE_CLK);
		nge_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	nge_eeprom_idle(sc);

	*dest = word;
}

/*
 * Read a sequence of words from the EEPROM.
 */
void
nge_read_eeprom(sc, dest, off, cnt, swap)
	struct nge_softc	*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		nge_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
void
nge_mii_sync(sc)
	struct nge_softc		*sc;
{
	int			i;

	SIO_SET(NGE_MEAR_MII_DIR|NGE_MEAR_MII_DATA);

	for (i = 0; i < 32; i++) {
		SIO_SET(NGE_MEAR_MII_CLK);
		DELAY(1);
		SIO_CLR(NGE_MEAR_MII_CLK);
		DELAY(1);
	}
}

/*
 * Clock a series of bits through the MII.
 */
void
nge_mii_send(sc, bits, cnt)
	struct nge_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	SIO_CLR(NGE_MEAR_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			SIO_SET(NGE_MEAR_MII_DATA);
                } else {
			SIO_CLR(NGE_MEAR_MII_DATA);
                }
		DELAY(1);
		SIO_CLR(NGE_MEAR_MII_CLK);
		DELAY(1);
		SIO_SET(NGE_MEAR_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
int
nge_mii_readreg(sc, frame)
	struct nge_softc		*sc;
	struct nge_mii_frame	*frame;
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = NGE_MII_STARTDELIM;
	frame->mii_opcode = NGE_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;

	CSR_WRITE_4(sc, NGE_MEAR, 0);

	/*
	 * Turn on data xmit.
	 */
	SIO_SET(NGE_MEAR_MII_DIR);

	nge_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	nge_mii_send(sc, frame->mii_stdelim, 2);
	nge_mii_send(sc, frame->mii_opcode, 2);
	nge_mii_send(sc, frame->mii_phyaddr, 5);
	nge_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	SIO_CLR((NGE_MEAR_MII_CLK|NGE_MEAR_MII_DATA));
	DELAY(1);
	SIO_SET(NGE_MEAR_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	SIO_CLR(NGE_MEAR_MII_DIR);
	/* Check for ack */
	SIO_CLR(NGE_MEAR_MII_CLK);
	DELAY(1);
	SIO_SET(NGE_MEAR_MII_CLK);
	DELAY(1);
	ack = CSR_READ_4(sc, NGE_MEAR) & NGE_MEAR_MII_DATA;

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			SIO_CLR(NGE_MEAR_MII_CLK);
			DELAY(1);
			SIO_SET(NGE_MEAR_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		SIO_CLR(NGE_MEAR_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_4(sc, NGE_MEAR) & NGE_MEAR_MII_DATA)
				frame->mii_data |= i;
			DELAY(1);
		}
		SIO_SET(NGE_MEAR_MII_CLK);
		DELAY(1);
	}

fail:

	SIO_CLR(NGE_MEAR_MII_CLK);
	DELAY(1);
	SIO_SET(NGE_MEAR_MII_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
int
nge_mii_writereg(sc, frame)
	struct nge_softc		*sc;
	struct nge_mii_frame	*frame;
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = NGE_MII_STARTDELIM;
	frame->mii_opcode = NGE_MII_WRITEOP;
	frame->mii_turnaround = NGE_MII_TURNAROUND;

	/*
	 * Turn on data output.
	 */
	SIO_SET(NGE_MEAR_MII_DIR);

	nge_mii_sync(sc);

	nge_mii_send(sc, frame->mii_stdelim, 2);
	nge_mii_send(sc, frame->mii_opcode, 2);
	nge_mii_send(sc, frame->mii_phyaddr, 5);
	nge_mii_send(sc, frame->mii_regaddr, 5);
	nge_mii_send(sc, frame->mii_turnaround, 2);
	nge_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	SIO_SET(NGE_MEAR_MII_CLK);
	DELAY(1);
	SIO_CLR(NGE_MEAR_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	SIO_CLR(NGE_MEAR_MII_DIR);

	splx(s);

	return(0);
}

int
nge_miibus_readreg(dev, phy, reg)
	struct device		*dev;
	int			phy, reg;
{
	struct nge_softc	*sc = (struct nge_softc *)dev;
	struct nge_mii_frame	frame;

	DPRINTFN(9, ("%s: nge_miibus_readreg\n", sc->sc_dv.dv_xname));

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	nge_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

void
nge_miibus_writereg(dev, phy, reg, data)
	struct device		*dev;
	int			phy, reg, data;
{
	struct nge_softc	*sc = (struct nge_softc *)dev;
	struct nge_mii_frame	frame;


	DPRINTFN(9, ("%s: nge_miibus_writereg\n", sc->sc_dv.dv_xname));

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;
	nge_mii_writereg(sc, &frame);
}

void
nge_miibus_statchg(dev)
	struct device		*dev;
{
	struct nge_softc	*sc = (struct nge_softc *)dev;
	struct mii_data		*mii = &sc->nge_mii;
	u_int32_t		txcfg, rxcfg;

	txcfg = CSR_READ_4(sc, NGE_TX_CFG);
	rxcfg = CSR_READ_4(sc, NGE_RX_CFG);

	DPRINTFN(4, ("%s: nge_miibus_statchg txcfg=%#x, rxcfg=%#x\n",
		     sc->sc_dv.dv_xname, txcfg, rxcfg));

	if ((mii->mii_media_active & IFM_GMASK) == IFM_FDX) {
		txcfg |= (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR);
		rxcfg |= (NGE_RXCFG_RX_FDX);
	} else {
		txcfg &= ~(NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR);
		rxcfg &= ~(NGE_RXCFG_RX_FDX);
	}

	txcfg |= NGE_TXCFG_AUTOPAD;
	
	CSR_WRITE_4(sc, NGE_TX_CFG, txcfg);
	CSR_WRITE_4(sc, NGE_RX_CFG, rxcfg);

	/* If we have a 1000Mbps link, set the mode_1000 bit. */
	if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T)
		NGE_SETBIT(sc, NGE_CFG, NGE_CFG_MODE_1000);
	else
		NGE_CLRBIT(sc, NGE_CFG, NGE_CFG_MODE_1000);
}

void
nge_setmulti(sc)
	struct nge_softc	*sc;
{
	struct arpcom		*ac = &sc->arpcom;
	struct ifnet		*ifp = &ac->ac_if;
	struct ether_multi      *enm;
	struct ether_multistep  step;
	u_int32_t		h = 0, i, filtsave;
	int			bit, index;

allmulti:
	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		NGE_CLRBIT(sc, NGE_RXFILT_CTL,
		    NGE_RXFILTCTL_MCHASH|NGE_RXFILTCTL_UCHASH);
		NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ALLMULTI);
		return;
	}

	/*
	 * We have to explicitly enable the multicast hash table
	 * on the NatSemi chip if we want to use it, which we do.
	 * We also have to tell it that we don't want to use the
	 * hash table for matching unicast addresses.
	 */
	NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_MCHASH);
	NGE_CLRBIT(sc, NGE_RXFILT_CTL,
	    NGE_RXFILTCTL_ALLMULTI|NGE_RXFILTCTL_UCHASH);

	filtsave = CSR_READ_4(sc, NGE_RXFILT_CTL);

	/* first, zot all the existing hash bits */
	for (i = 0; i < NGE_MCAST_FILTER_LEN; i += 2) {
		CSR_WRITE_4(sc, NGE_RXFILT_CTL, NGE_FILTADDR_MCAST_LO + i);
		CSR_WRITE_4(sc, NGE_RXFILT_DATA, 0);
	}

	/*
	 * From the 11 bits returned by the crc routine, the top 7
	 * bits represent the 16-bit word in the mcast hash table
	 * that needs to be updated, and the lower 4 bits represent
	 * which bit within that byte needs to be set.
	 */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			goto allmulti;
		}
		h = (ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN) >> 21) &
		    0x00000FFF;
		index = (h >> 4) & 0x7F;
		bit = h & 0xF;
		CSR_WRITE_4(sc, NGE_RXFILT_CTL,
		    NGE_FILTADDR_MCAST_LO + (index * 2));
		NGE_SETBIT(sc, NGE_RXFILT_DATA, (1 << bit));
		ETHER_NEXT_MULTI(step, enm);
	}

	CSR_WRITE_4(sc, NGE_RXFILT_CTL, filtsave);
}

void
nge_reset(sc)
	struct nge_softc	*sc;
{
	int			i;

	NGE_SETBIT(sc, NGE_CSR, NGE_CSR_RESET);

	for (i = 0; i < NGE_TIMEOUT; i++) {
		if (!(CSR_READ_4(sc, NGE_CSR) & NGE_CSR_RESET))
			break;
	}

	if (i == NGE_TIMEOUT)
		printf("%s: reset never completed\n", sc->sc_dv.dv_xname);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

	/*
	 * If this is a NetSemi chip, make sure to clear
	 * PME mode.
	 */
	CSR_WRITE_4(sc, NGE_CLKRUN, NGE_CLKRUN_PMESTS);
	CSR_WRITE_4(sc, NGE_CLKRUN, 0);
}

/*
 * Probe for an NatSemi chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
int
nge_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *)aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_NS &&
	    PCI_PRODUCT(pa->pa_id) == PCI_PRODUCT_NS_DP83820)
		return (1);

	return (0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
nge_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct nge_softc	*sc = (struct nge_softc *)self;
	struct pci_attach_args	*pa = aux;
	pci_chipset_tag_t	pc = pa->pa_pc;
	pci_intr_handle_t	ih;
	const char		*intrstr = NULL;
	bus_addr_t		iobase;
	bus_size_t		iosize;
	bus_dma_segment_t	seg;
	bus_dmamap_t		dmamap;
	int			s, rseg;
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int32_t		command;
	struct ifnet		*ifp;
	int			error = 0;
	caddr_t			kva;

	s = splimp();

	/*
	 * Handle power management nonsense.
	 */
	DPRINTFN(5, ("%s: preparing for conf read\n", sc->sc_dv.dv_xname));
	command = pci_conf_read(pc, pa->pa_tag, NGE_PCI_CAPID) & 0x000000FF;
	if (command == 0x01) {
		command = pci_conf_read(pc, pa->pa_tag, NGE_PCI_PWRMGMTCTRL);
		if (command & NGE_PSTATE_MASK) {
			u_int32_t		iobase, membase, irq;

			/* Save important PCI config data. */
			iobase = pci_conf_read(pc, pa->pa_tag, NGE_PCI_LOIO);
			membase = pci_conf_read(pc, pa->pa_tag, NGE_PCI_LOMEM);
			irq = pci_conf_read(pc, pa->pa_tag, NGE_PCI_INTLINE);

			/* Reset the power state. */
			printf("%s: chip is in D%d power mode "
			       "-- setting to D0\n", sc->sc_dv.dv_xname,
			       command & NGE_PSTATE_MASK);
			command &= 0xFFFFFFFC;
			pci_conf_write(pc, pa->pa_tag,
				       NGE_PCI_PWRMGMTCTRL, command);

			/* Restore PCI config data. */
			pci_conf_write(pc, pa->pa_tag, NGE_PCI_LOIO, iobase);
			pci_conf_write(pc, pa->pa_tag, NGE_PCI_LOMEM, membase);
			pci_conf_write(pc, pa->pa_tag, NGE_PCI_INTLINE, irq);
		}
	}

	/*
	 * Map control/status registers.
	 */
	DPRINTFN(5, ("%s: map control/status regs\n", sc->sc_dv.dv_xname));
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	  PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

#ifdef NGE_USEIOSPACE
	if (!(command & PCI_COMMAND_IO_ENABLE)) {
		printf("%s: failed to enable I/O ports!\n",
		       sc->sc_dv.dv_xname);
		error = ENXIO;
		goto fail;
	}
	/*
	 * Map control/status registers.
	 */
	DPRINTFN(5, ("%s: pci_io_find\n", sc->sc_dv.dv_xname));
	if (pci_io_find(pc, pa->pa_tag, NGE_PCI_LOIO, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
		goto fail;
	}
	DPRINTFN(5, ("%s: bus_space_map\n", sc->sc_dv.dv_xname));
	if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &sc->nge_bhandle)) {
		printf(": can't map i/o space\n");
		goto fail;
	}
	sc->nge_btag = pa->pa_iot;
#else
	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf("%s: failed to enable memory mapping!\n",
		       sc->sc_dv.dv_xname);
		error = ENXIO;
		goto fail;
	}
	DPRINTFN(5, ("%s: pci_mem_find\n", sc->sc_dv.dv_xname));
	if (pci_mem_find(pc, pa->pa_tag, NGE_PCI_LOMEM, &iobase,
			 &iosize, NULL)) {
		printf(": can't find mem space\n");
		goto fail;
	}
	DPRINTFN(5, ("%s: bus_space_map\n", sc->sc_dv.dv_xname));
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->nge_bhandle)) {
		printf(": can't map mem space\n");
		goto fail;
	}

	sc->nge_btag = pa->pa_memt;
#endif

	/* Disable all interrupts */
	CSR_WRITE_4(sc, NGE_IER, 0);

	DPRINTFN(5, ("%s: pci_intr_map\n", sc->sc_dv.dv_xname));
	if (pci_intr_map(pa, &ih)) {
		printf(": couldn't map interrupt\n");
		goto fail;
	}

	DPRINTFN(5, ("%s: pci_intr_string\n", sc->sc_dv.dv_xname));
	intrstr = pci_intr_string(pc, ih);
	DPRINTFN(5, ("%s: pci_intr_establish\n", sc->sc_dv.dv_xname));
	sc->nge_intrhand = pci_intr_establish(pc, ih, IPL_NET, nge_intr, sc,
					      sc->sc_dv.dv_xname);
	if (sc->nge_intrhand == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		goto fail;
	}
	printf(": %s", intrstr);

	/* Reset the adapter. */
	DPRINTFN(5, ("%s: nge_reset\n", sc->sc_dv.dv_xname));
	nge_reset(sc);

	/*
	 * Get station address from the EEPROM.
	 */
	DPRINTFN(5, ("%s: nge_read_eeprom\n", sc->sc_dv.dv_xname));
	nge_read_eeprom(sc, (caddr_t)&eaddr[4], NGE_EE_NODEADDR, 1, 0);
	nge_read_eeprom(sc, (caddr_t)&eaddr[2], NGE_EE_NODEADDR + 1, 1, 0);
	nge_read_eeprom(sc, (caddr_t)&eaddr[0], NGE_EE_NODEADDR + 2, 1, 0);

	/*
	 * A NatSemi chip was detected. Inform the world.
	 */
	printf(": address: %s\n", ether_sprintf(eaddr));

	bcopy(eaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);

	sc->sc_dmatag = pa->pa_dmat;
	DPRINTFN(5, ("%s: bus_dmamem_alloc\n", sc->sc_dv.dv_xname));
	if (bus_dmamem_alloc(sc->sc_dmatag, sizeof(struct nge_list_data),
			     PAGE_SIZE, 0, &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: can't alloc rx buffers\n", sc->sc_dv.dv_xname);
		goto fail;
	}
	DPRINTFN(5, ("%s: bus_dmamem_map\n", sc->sc_dv.dv_xname));
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg,
			   sizeof(struct nge_list_data), &kva,
			   BUS_DMA_NOWAIT)) {
		printf("%s: can't map dma buffers (%d bytes)\n",
		       sc->sc_dv.dv_xname, sizeof(struct nge_list_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
	DPRINTFN(5, ("%s: bus_dmamem_create\n", sc->sc_dv.dv_xname));
	if (bus_dmamap_create(sc->sc_dmatag, sizeof(struct nge_list_data), 1,
			      sizeof(struct nge_list_data), 0,
			      BUS_DMA_NOWAIT, &dmamap)) {
		printf("%s: can't create dma map\n", sc->sc_dv.dv_xname);
		bus_dmamem_unmap(sc->sc_dmatag, kva,
				 sizeof(struct nge_list_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}
	DPRINTFN(5, ("%s: bus_dmamem_load\n", sc->sc_dv.dv_xname));
	if (bus_dmamap_load(sc->sc_dmatag, dmamap, kva,
			    sizeof(struct nge_list_data), NULL,
			    BUS_DMA_NOWAIT)) {
		bus_dmamap_destroy(sc->sc_dmatag, dmamap);
		bus_dmamem_unmap(sc->sc_dmatag, kva,
				 sizeof(struct nge_list_data));
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		goto fail;
	}

	DPRINTFN(5, ("%s: bzero\n", sc->sc_dv.dv_xname));
	sc->nge_ldata = (struct nge_list_data *)kva;
	bzero(sc->nge_ldata, sizeof(struct nge_list_data));

	/* Try to allocate memory for jumbo buffers. */
	DPRINTFN(5, ("%s: nge_alloc_jumbo_mem\n", sc->sc_dv.dv_xname));
	if (nge_alloc_jumbo_mem(sc)) {
		printf("%s: jumbo buffer allocation failed\n",
		       sc->sc_dv.dv_xname);
		goto fail;
	}

	ifp = &sc->arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = nge_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = nge_start;
	ifp->if_watchdog = nge_watchdog;
	ifp->if_baudrate = 1000000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, NGE_TX_LIST_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	ifp->if_capabilities =
	    IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;
#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_MTU;
#endif
	DPRINTFN(5, ("%s: bcopy\n", sc->sc_dv.dv_xname));
	bcopy(sc->sc_dv.dv_xname, ifp->if_xname, IFNAMSIZ);

	/*
	 * Do MII setup.
	 */
	DPRINTFN(5, ("%s: mii setup\n", sc->sc_dv.dv_xname));
	if (CSR_READ_4(sc, NGE_CFG) & NGE_CFG_TBI_EN) {
		DPRINTFN(5, ("%s: TBI mode\n", sc->sc_dv.dv_xname));
		sc->nge_tbi = 1;

		ifmedia_init(&sc->nge_ifmedia, 0, nge_ifmedia_tbi_upd, 
			     nge_ifmedia_tbi_sts);

		ifmedia_add(&sc->nge_ifmedia, IFM_ETHER|IFM_NONE, 0, NULL),
		ifmedia_add(&sc->nge_ifmedia, IFM_ETHER|IFM_1000_SX, 0, NULL);
		ifmedia_add(&sc->nge_ifmedia, IFM_ETHER|IFM_1000_SX|IFM_FDX,
			    0, NULL);
		ifmedia_add(&sc->nge_ifmedia, IFM_ETHER|IFM_AUTO, 0, NULL);

		ifmedia_set(&sc->nge_ifmedia, IFM_ETHER|IFM_AUTO);
	    
		CSR_WRITE_4(sc, NGE_GPIO, CSR_READ_4(sc, NGE_GPIO)
			    | NGE_GPIO_GP4_OUT 
			    | NGE_GPIO_GP1_OUTENB | NGE_GPIO_GP2_OUTENB 
			    | NGE_GPIO_GP3_OUTENB | NGE_GPIO_GP4_OUTENB
			    | NGE_GPIO_GP5_OUTENB);

		NGE_SETBIT(sc, NGE_CFG, NGE_CFG_MODE_1000);
	} else {
		sc->nge_mii.mii_ifp = ifp;
		sc->nge_mii.mii_readreg = nge_miibus_readreg;
		sc->nge_mii.mii_writereg = nge_miibus_writereg;
		sc->nge_mii.mii_statchg = nge_miibus_statchg;

		ifmedia_init(&sc->nge_mii.mii_media, 0, nge_ifmedia_mii_upd,
			     nge_ifmedia_mii_sts);
		mii_attach(&sc->sc_dv, &sc->nge_mii, 0xffffffff, MII_PHY_ANY,
			   MII_OFFSET_ANY, 0);
		
		if (LIST_FIRST(&sc->nge_mii.mii_phys) == NULL) {
			
			printf("%s: no PHY found!\n", sc->sc_dv.dv_xname);
			ifmedia_add(&sc->nge_mii.mii_media,
				    IFM_ETHER|IFM_MANUAL, 0, NULL);
			ifmedia_set(&sc->nge_mii.mii_media,
				    IFM_ETHER|IFM_MANUAL);
		}
		else
			ifmedia_set(&sc->nge_mii.mii_media,
				    IFM_ETHER|IFM_AUTO);
	}

	/*
	 * Call MI attach routine.
	 */
	DPRINTFN(5, ("%s: if_attach\n", sc->sc_dv.dv_xname));
	if_attach(ifp);
	DPRINTFN(5, ("%s: ether_ifattach\n", sc->sc_dv.dv_xname));
	ether_ifattach(ifp);
	DPRINTFN(5, ("%s: timeout_set\n", sc->sc_dv.dv_xname));
	timeout_set(&sc->nge_timeout, nge_tick, sc);
	timeout_add(&sc->nge_timeout, hz);

fail:
	splx(s);
}

/*
 * Initialize the transmit descriptors.
 */
int
nge_list_tx_init(sc)
	struct nge_softc	*sc;
{
	struct nge_list_data	*ld;
	struct nge_ring_data	*cd;
	int			i;

	cd = &sc->nge_cdata;
	ld = sc->nge_ldata;

	for (i = 0; i < NGE_TX_LIST_CNT; i++) {
		if (i == (NGE_TX_LIST_CNT - 1)) {
			ld->nge_tx_list[i].nge_nextdesc =
			    &ld->nge_tx_list[0];
			ld->nge_tx_list[i].nge_next =
			    vtophys(&ld->nge_tx_list[0]);
		} else {
			ld->nge_tx_list[i].nge_nextdesc =
			    &ld->nge_tx_list[i + 1];
			ld->nge_tx_list[i].nge_next =
			    vtophys(&ld->nge_tx_list[i + 1]);
		}
		ld->nge_tx_list[i].nge_mbuf = NULL;
		ld->nge_tx_list[i].nge_ptr = 0;
		ld->nge_tx_list[i].nge_ctl = 0;
	}

	cd->nge_tx_prod = cd->nge_tx_cons = cd->nge_tx_cnt = 0;

	return(0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
int
nge_list_rx_init(sc)
	struct nge_softc	*sc;
{
	struct nge_list_data	*ld;
	struct nge_ring_data	*cd;
	int			i;

	ld = sc->nge_ldata;
	cd = &sc->nge_cdata;

	for (i = 0; i < NGE_RX_LIST_CNT; i++) {
		if (nge_newbuf(sc, &ld->nge_rx_list[i], NULL) == ENOBUFS)
			return(ENOBUFS);
		if (i == (NGE_RX_LIST_CNT - 1)) {
			ld->nge_rx_list[i].nge_nextdesc =
			    &ld->nge_rx_list[0];
			ld->nge_rx_list[i].nge_next =
			    vtophys(&ld->nge_rx_list[0]);
		} else {
			ld->nge_rx_list[i].nge_nextdesc =
			    &ld->nge_rx_list[i + 1];
			ld->nge_rx_list[i].nge_next =
			    vtophys(&ld->nge_rx_list[i + 1]);
		}
	}

	cd->nge_rx_prod = 0;

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
int
nge_newbuf(sc, c, m)
	struct nge_softc	*sc;
	struct nge_desc		*c;
	struct mbuf		*m;
{
	struct mbuf		*m_new = NULL;
	caddr_t			*buf = NULL;

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL)
			return(ENOBUFS);

		/* Allocate the jumbo buffer */
		buf = nge_jalloc(sc);
		if (buf == NULL) {
			m_freem(m_new);
			return(ENOBUFS);
		}
		/* Attach the buffer to the mbuf */
		m_new->m_data = m_new->m_ext.ext_buf = (void *)buf;
		m_new->m_flags |= M_EXT;
		m_new->m_ext.ext_size = m_new->m_pkthdr.len =
			m_new->m_len = NGE_MCLBYTES;
		m_new->m_ext.ext_free = nge_jfree;
		m_new->m_ext.ext_arg = sc;
		MCLINITREFERENCE(m_new);
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = NGE_MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, sizeof(u_int64_t));

	c->nge_mbuf = m_new;
	c->nge_ptr = vtophys(mtod(m_new, caddr_t));
	DPRINTFN(7,("%s: c->nge_ptr=%#x\n", sc->sc_dv.dv_xname,
		    c->nge_ptr));
	c->nge_ctl = m_new->m_len;
	c->nge_extsts = 0;

	return(0);
}

int
nge_alloc_jumbo_mem(sc)
	struct nge_softc	*sc;
{
	caddr_t			ptr, kva;
	bus_dma_segment_t	seg;
	bus_dmamap_t		dmamap;
	int			i, rseg;
	struct nge_jpool_entry	*entry;

	if (bus_dmamem_alloc(sc->sc_dmatag, NGE_JMEM, PAGE_SIZE, 0,
			     &seg, 1, &rseg, BUS_DMA_NOWAIT)) {
		printf("%s: can't alloc rx buffers\n", sc->sc_dv.dv_xname);
		return (ENOBUFS);
	}
	if (bus_dmamem_map(sc->sc_dmatag, &seg, rseg, NGE_JMEM, &kva,
			   BUS_DMA_NOWAIT)) {
		printf("%s: can't map dma buffers (%d bytes)\n",
		       sc->sc_dv.dv_xname, NGE_JMEM);
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		return (ENOBUFS);
	}
	if (bus_dmamap_create(sc->sc_dmatag, NGE_JMEM, 1,
			      NGE_JMEM, 0, BUS_DMA_NOWAIT, &dmamap)) {
		printf("%s: can't create dma map\n", sc->sc_dv.dv_xname);
		bus_dmamem_unmap(sc->sc_dmatag, kva, NGE_JMEM);
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		return (ENOBUFS);
	}
	if (bus_dmamap_load(sc->sc_dmatag, dmamap, kva, NGE_JMEM,
			    NULL, BUS_DMA_NOWAIT)) {
		printf("%s: can't load dma map\n", sc->sc_dv.dv_xname);
		bus_dmamap_destroy(sc->sc_dmatag, dmamap);
		bus_dmamem_unmap(sc->sc_dmatag, kva, NGE_JMEM);
		bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
		return (ENOBUFS);
        }
	sc->nge_cdata.nge_jumbo_buf = (caddr_t)kva;
	DPRINTFN(1,("%s: nge_jumbo_buf=%#x, NGE_MCLBYTES=%#x\n",
		    sc->sc_dv.dv_xname , sc->nge_cdata.nge_jumbo_buf,
		    NGE_MCLBYTES));

	LIST_INIT(&sc->nge_jfree_listhead);
	LIST_INIT(&sc->nge_jinuse_listhead);

	/*
	 * Now divide it up into 9K pieces and save the addresses
	 * in an array. Note that we play an evil trick here by using
	 * the first few bytes in the buffer to hold the the address
	 * of the softc structure for this interface. This is because
	 * nge_jfree() needs it, but it is called by the mbuf management
	 * code which will not pass it to us explicitly.
	 */
	ptr = sc->nge_cdata.nge_jumbo_buf;
	for (i = 0; i < NGE_JSLOTS; i++) {
		sc->nge_cdata.nge_jslots[i].nge_buf = ptr;
		sc->nge_cdata.nge_jslots[i].nge_inuse = 0;
		ptr += NGE_MCLBYTES;
		entry = malloc(sizeof(struct nge_jpool_entry),
			       M_DEVBUF, M_NOWAIT);
		if (entry == NULL) {
			bus_dmamap_unload(sc->sc_dmatag, dmamap);
			bus_dmamap_destroy(sc->sc_dmatag, dmamap);
			bus_dmamem_unmap(sc->sc_dmatag, kva, NGE_JMEM);
			bus_dmamem_free(sc->sc_dmatag, &seg, rseg);
			sc->nge_cdata.nge_jumbo_buf = NULL;
			printf("%s: no memory for jumbo buffer queue!\n",
			       sc->sc_dv.dv_xname);
			return(ENOBUFS);
		}
		entry->slot = i;
		LIST_INSERT_HEAD(&sc->nge_jfree_listhead, entry,
				 jpool_entries);
	}

	return(0);
}

/*
 * Allocate a jumbo buffer.
 */
void *
nge_jalloc(sc)
	struct nge_softc	*sc;
{
	struct nge_jpool_entry   *entry;

	entry = LIST_FIRST(&sc->nge_jfree_listhead);

	if (entry == NULL) {
#ifdef NGE_VERBOSE
		printf("%s: no free jumbo buffers\n", sc->sc_dv.dv_xname);
#endif
		return(NULL);
	}

	LIST_REMOVE(entry, jpool_entries);
	LIST_INSERT_HEAD(&sc->nge_jinuse_listhead, entry, jpool_entries);
	sc->nge_cdata.nge_jslots[entry->slot].nge_inuse = 1;
	return(sc->nge_cdata.nge_jslots[entry->slot].nge_buf);
}

/*
 * Release a jumbo buffer.
 */
void
nge_jfree(buf, size, arg)
	caddr_t		buf;
	u_int		size;
	void		*arg;
{
	struct nge_softc	*sc;
	int		        i;
	struct nge_jpool_entry *entry;

	/* Extract the softc struct pointer. */
	sc = (struct nge_softc *)arg;

	if (sc == NULL)
		panic("nge_jfree: can't find softc pointer!");

	/* calculate the slot this buffer belongs to */

	i = ((vaddr_t)buf - (vaddr_t)sc->nge_cdata.nge_jumbo_buf)
	  / NGE_MCLBYTES;

	if ((i < 0) || (i >= NGE_JSLOTS))
		panic("nge_jfree: asked to free buffer that we don't manage!");
	else if (sc->nge_cdata.nge_jslots[i].nge_inuse == 0)
		panic("nge_jfree: buffer already free!");
	else {
		sc->nge_cdata.nge_jslots[i].nge_inuse--;
		if(sc->nge_cdata.nge_jslots[i].nge_inuse == 0) {
			entry = LIST_FIRST(&sc->nge_jinuse_listhead);
			if (entry == NULL)
				panic("nge_jfree: buffer not in use!");
			entry->slot = i;
			LIST_REMOVE(entry, jpool_entries);
			LIST_INSERT_HEAD(&sc->nge_jfree_listhead,
					 entry, jpool_entries);
		}
	}
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
void
nge_rxeof(sc)
	struct nge_softc	*sc;
{
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct nge_desc		*cur_rx;
	int			i, total_len = 0;
	u_int32_t		rxstat;

	ifp = &sc->arpcom.ac_if;
	i = sc->nge_cdata.nge_rx_prod;

	while(NGE_OWNDESC(&sc->nge_ldata->nge_rx_list[i])) {
		struct mbuf		*m0 = NULL;
		u_int32_t		extsts;

		cur_rx = &sc->nge_ldata->nge_rx_list[i];
		rxstat = cur_rx->nge_rxstat;
		extsts = cur_rx->nge_extsts;
		m = cur_rx->nge_mbuf;
		cur_rx->nge_mbuf = NULL;
		total_len = NGE_RXBYTES(cur_rx);
		NGE_INC(i, NGE_RX_LIST_CNT);

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
		 * comes up in the ring.
		 */
		if (!(rxstat & NGE_CMDSTS_PKT_OK)) {
			ifp->if_ierrors++;
			nge_newbuf(sc, cur_rx, m);
			continue;
		}

		/*
		 * Ok. NatSemi really screwed up here. This is the
		 * only gigE chip I know of with alignment constraints
		 * on receive buffers. RX buffers must be 64-bit aligned.
		 */
#ifndef __STRICT_ALIGNMENT
		/*
		 * By popular demand, ignore the alignment problems
		 * on the Intel x86 platform. The performance hit
		 * incurred due to unaligned accesses is much smaller
		 * than the hit produced by forcing buffer copies all
		 * the time, especially with jumbo frames. We still
		 * need to fix up the alignment everywhere else though.
		 */
		if (nge_newbuf(sc, cur_rx, NULL) == ENOBUFS) {
#endif
			m0 = m_devget(mtod(m, char *), total_len,
			    ETHER_ALIGN, ifp, NULL);
			nge_newbuf(sc, cur_rx, m);
			if (m0 == NULL) {
				ifp->if_ierrors++;
				continue;
			}
			m_adj(m0, ETHER_ALIGN);
			m = m0;
#ifndef __STRICT_ALIGNMENT
		} else {
			m->m_pkthdr.rcvif = ifp;
			m->m_pkthdr.len = m->m_len = total_len;
		}
#endif

		ifp->if_ipackets++;

#if NBPFILTER > 0
		/*
		 * Handle BPF listeners. Let the BPF user see the packet.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif

		/* Do IP checksum checking. */
		if (extsts & NGE_RXEXTSTS_IPPKT) {
			if (extsts & NGE_RXEXTSTS_IPCSUMERR)
				m->m_pkthdr.csum |= M_IPV4_CSUM_IN_BAD;
			else
				m->m_pkthdr.csum |= M_IPV4_CSUM_IN_OK;
		}
		if (extsts & NGE_RXEXTSTS_TCPPKT) {
			if (extsts & NGE_RXEXTSTS_TCPCSUMERR)
				m->m_pkthdr.csum |= M_TCP_CSUM_IN_BAD;
			else
				m->m_pkthdr.csum |= M_TCP_CSUM_IN_OK;
		}
		if (extsts & NGE_RXEXTSTS_UDPPKT) {
			if (extsts & NGE_RXEXTSTS_UDPCSUMERR)
				m->m_pkthdr.csum |= M_UDP_CSUM_IN_BAD;
			else
				m->m_pkthdr.csum |= M_UDP_CSUM_IN_OK;
		}

#if NVLAN > 0
		/*
		 * If we received a packet with a vlan tag, pass it
		 * to vlan_input() instead of ether_input().
		 */
		if (extsts & NGE_RXEXTSTS_VLANPKT) {
			if (vlan_input_tag(m, extsts & NGE_RXEXTSTS_VTCI) < 0)
				ifp->if_data.ifi_noproto++;
                        continue;
                }
#endif

		ether_input_mbuf(ifp, m);
	}

	sc->nge_cdata.nge_rx_prod = i;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

void
nge_txeof(sc)
	struct nge_softc	*sc;
{
	struct nge_desc		*cur_tx = NULL;
	struct ifnet		*ifp;
	u_int32_t		idx;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	idx = sc->nge_cdata.nge_tx_cons;
	while (idx != sc->nge_cdata.nge_tx_prod) {
		cur_tx = &sc->nge_ldata->nge_tx_list[idx];

		if (NGE_OWNDESC(cur_tx))
			break;

		if (cur_tx->nge_ctl & NGE_CMDSTS_MORE) {
			sc->nge_cdata.nge_tx_cnt--;
			NGE_INC(idx, NGE_TX_LIST_CNT);
			continue;
		}

		if (!(cur_tx->nge_ctl & NGE_CMDSTS_PKT_OK)) {
			ifp->if_oerrors++;
			if (cur_tx->nge_txstat & NGE_TXSTAT_EXCESSCOLLS)
				ifp->if_collisions++;
			if (cur_tx->nge_txstat & NGE_TXSTAT_OUTOFWINCOLL)
				ifp->if_collisions++;
		}

		ifp->if_collisions +=
		    (cur_tx->nge_txstat & NGE_TXSTAT_COLLCNT) >> 16;

		ifp->if_opackets++;
		if (cur_tx->nge_mbuf != NULL) {
			m_freem(cur_tx->nge_mbuf);
			cur_tx->nge_mbuf = NULL;
		}

		sc->nge_cdata.nge_tx_cnt--;
		NGE_INC(idx, NGE_TX_LIST_CNT);
		ifp->if_timer = 0;
	}

	sc->nge_cdata.nge_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;
}

void
nge_tick(xsc)
	void			*xsc;
{
	struct nge_softc	*sc = xsc;
	struct mii_data		*mii = &sc->nge_mii;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s;

	s = splimp();

	DPRINTFN(10, ("%s: nge_tick: link=%d\n", sc->sc_dv.dv_xname,
		      sc->nge_link));

	timeout_add(&sc->nge_timeout, hz);
	if (sc->nge_link) {
		splx(s);
		return;
	}

	if (sc->nge_tbi) {
		if (IFM_SUBTYPE(sc->nge_ifmedia.ifm_cur->ifm_media)
		    == IFM_AUTO) {
			u_int32_t bmsr, anlpar, txcfg, rxcfg;

			bmsr = CSR_READ_4(sc, NGE_TBI_BMSR);
			DPRINTFN(2, ("%s: nge_tick: bmsr=%#x\n",
				     sc->sc_dv.dv_xname, bmsr));

			if (!(bmsr & NGE_TBIBMSR_ANEG_DONE)) {
				CSR_WRITE_4(sc, NGE_TBI_BMCR, 0);

				splx(s);
				return;
			}
				
			anlpar = CSR_READ_4(sc, NGE_TBI_ANLPAR);
			txcfg = CSR_READ_4(sc, NGE_TX_CFG);
			rxcfg = CSR_READ_4(sc, NGE_RX_CFG);
			
			DPRINTFN(2, ("%s: nge_tick: anlpar=%#x, txcfg=%#x, "
				     "rxcfg=%#x\n", sc->sc_dv.dv_xname, anlpar,
				     txcfg, rxcfg));
			
			if (anlpar == 0 || anlpar & NGE_TBIANAR_FDX) {
				txcfg |= (NGE_TXCFG_IGN_HBEAT|
					  NGE_TXCFG_IGN_CARR);
				rxcfg |= NGE_RXCFG_RX_FDX;
			} else {
				txcfg &= ~(NGE_TXCFG_IGN_HBEAT|
					   NGE_TXCFG_IGN_CARR);
				rxcfg &= ~(NGE_RXCFG_RX_FDX);
			}
			txcfg |= NGE_TXCFG_AUTOPAD;
			CSR_WRITE_4(sc, NGE_TX_CFG, txcfg);
			CSR_WRITE_4(sc, NGE_RX_CFG, rxcfg);
		}

		DPRINTF(("%s: gigabit link up\n", sc->sc_dv.dv_xname));
		sc->nge_link++;
		if (!IFQ_IS_EMPTY(&ifp->if_snd))
			nge_start(ifp);
	} else {
		mii_tick(mii);
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			sc->nge_link++;
			if (IFM_SUBTYPE(mii->mii_media_active) == IFM_1000_T)
				DPRINTF(("%s: gigabit link up\n",
					 sc->sc_dv.dv_xname));
			if (!IFQ_IS_EMPTY(&ifp->if_snd))
				nge_start(ifp);
		}
		
	}

	splx(s);
}

int
nge_intr(arg)
	void			*arg;
{
	struct nge_softc	*sc;
	struct ifnet		*ifp;
	u_int32_t		status;
	int			claimed = 0;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		nge_stop(sc);
		return (0);
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, NGE_IER, 0);

	/* Data LED on for TBI mode */
	if(sc->nge_tbi)
		 CSR_WRITE_4(sc, NGE_GPIO, CSR_READ_4(sc, NGE_GPIO)
			     | NGE_GPIO_GP3_OUT);

	for (;;) {
		/* Reading the ISR register clears all interrupts. */
		status = CSR_READ_4(sc, NGE_ISR);

		if ((status & NGE_INTRS) == 0)
			break;

		claimed = 1;

		if ((status & NGE_ISR_TX_DESC_OK) ||
		    (status & NGE_ISR_TX_ERR) ||
		    (status & NGE_ISR_TX_OK) ||
		    (status & NGE_ISR_TX_IDLE))
			nge_txeof(sc);

		if ((status & NGE_ISR_RX_DESC_OK) ||
		    (status & NGE_ISR_RX_ERR) ||
		    (status & NGE_ISR_RX_OFLOW) ||
		    (status & NGE_ISR_RX_FIFO_OFLOW) ||
		    (status & NGE_ISR_RX_IDLE) ||
		    (status & NGE_ISR_RX_OK))
			nge_rxeof(sc);

		if ((status & NGE_ISR_RX_IDLE))
			NGE_SETBIT(sc, NGE_CSR, NGE_CSR_RX_ENABLE);

		if (status & NGE_ISR_SYSERR) {
			nge_reset(sc);
			ifp->if_flags &= ~IFF_RUNNING;
			nge_init(sc);
		}

#if 0
		/* 
		 * XXX: nge_tick() is not ready to be called this way
		 * it screws up the aneg timeout because mii_tick() is
		 * only to be called once per second.
		 */
		if (status & NGE_IMR_PHY_INTR) {
			sc->nge_link = 0;
			nge_tick(sc);
		}
#endif
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, NGE_IER, 1);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		nge_start(ifp);

	/* Data LED off for TBI mode */
	if(sc->nge_tbi)
		CSR_WRITE_4(sc, NGE_GPIO, CSR_READ_4(sc, NGE_GPIO)
			    & ~NGE_GPIO_GP3_OUT);

	return claimed;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
int
nge_encap(sc, m_head, txidx)
	struct nge_softc	*sc;
	struct mbuf		*m_head;
	u_int32_t		*txidx;
{
	struct nge_desc		*f = NULL;
	struct mbuf		*m;
	int			frag, cur, cnt = 0;
#if NVLAN > 0
	struct ifvlan		*ifv = NULL;

	if ((m_head->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m_head->m_pkthdr.rcvif != NULL)
		ifv = m_head->m_pkthdr.rcvif->if_softc;
#endif

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	m = m_head;
	cur = frag = *txidx;

	for (m = m_head; m != NULL; m = m->m_next) {
		if (m->m_len != 0) {
			if ((NGE_TX_LIST_CNT -
			    (sc->nge_cdata.nge_tx_cnt + cnt)) < 2)
				return(ENOBUFS);
			f = &sc->nge_ldata->nge_tx_list[frag];
			f->nge_ctl = NGE_CMDSTS_MORE | m->m_len;
			f->nge_ptr = vtophys(mtod(m, vaddr_t));
			DPRINTFN(7,("%s: f->nge_ptr=%#x\n",
				    sc->sc_dv.dv_xname, f->nge_ptr));
			if (cnt != 0)
				f->nge_ctl |= NGE_CMDSTS_OWN;
			cur = frag;
			NGE_INC(frag, NGE_TX_LIST_CNT);
			cnt++;
		}
	}

	if (m != NULL)
		return(ENOBUFS);

	/*
	 * Card handles checksumming on a packet by packet
	 * basis.
	 */
	sc->nge_ldata->nge_tx_list[*txidx].nge_extsts = 0;
	if (m_head->m_pkthdr.csum) {
		if (m_head->m_pkthdr.csum & M_IPV4_CSUM_OUT)
			sc->nge_ldata->nge_tx_list[*txidx].nge_extsts |=
			    NGE_TXEXTSTS_IPCSUM;
		if (m_head->m_pkthdr.csum & M_TCPV4_CSUM_OUT)
			sc->nge_ldata->nge_tx_list[*txidx].nge_extsts |=
			    NGE_TXEXTSTS_TCPCSUM;
		if (m_head->m_pkthdr.csum & M_UDPV4_CSUM_OUT)
			sc->nge_ldata->nge_tx_list[*txidx].nge_extsts |=
			    NGE_TXEXTSTS_UDPCSUM;
	}

#if NVLAN > 0
	if (ifv != NULL) {
		sc->nge_ldata->nge_tx_list[cur].nge_extsts |=
			(NGE_TXEXTSTS_VLANPKT|ifv->ifv_tag);
	}
#endif

	sc->nge_ldata->nge_tx_list[cur].nge_mbuf = m_head;
	sc->nge_ldata->nge_tx_list[cur].nge_ctl &= ~NGE_CMDSTS_MORE;
	sc->nge_ldata->nge_tx_list[*txidx].nge_ctl |= NGE_CMDSTS_OWN;
	sc->nge_cdata.nge_tx_cnt += cnt;
	*txidx = frag;

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

void
nge_start(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc;
	struct mbuf		*m_head = NULL;
	u_int32_t		idx;
	int			pkts = 0;

	sc = ifp->if_softc;

	if (!sc->nge_link)
		return;

	idx = sc->nge_cdata.nge_tx_prod;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	while(sc->nge_ldata->nge_tx_list[idx].nge_mbuf == NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (nge_encap(sc, m_head, &idx)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* now we are committed to transmit the packet */
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		pkts++;

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif
	}
	if (pkts == 0)
		return;

	/* Transmit */
	sc->nge_cdata.nge_tx_prod = idx;
	NGE_SETBIT(sc, NGE_CSR, NGE_CSR_TX_ENABLE);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

void
nge_init(xsc)
	void			*xsc;
{
	struct nge_softc	*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	struct mii_data		*mii;
	u_int32_t		txcfg, rxcfg;
	int			s, media;

	if (ifp->if_flags & IFF_RUNNING)
		return;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	nge_stop(sc);

	mii = sc->nge_tbi ? NULL: &sc->nge_mii;

	/* Set MAC address */
	CSR_WRITE_4(sc, NGE_RXFILT_CTL, NGE_FILTADDR_PAR0);
	CSR_WRITE_4(sc, NGE_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[0]);
	CSR_WRITE_4(sc, NGE_RXFILT_CTL, NGE_FILTADDR_PAR1);
	CSR_WRITE_4(sc, NGE_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[1]);
	CSR_WRITE_4(sc, NGE_RXFILT_CTL, NGE_FILTADDR_PAR2);
	CSR_WRITE_4(sc, NGE_RXFILT_DATA,
	    ((u_int16_t *)sc->arpcom.ac_enaddr)[2]);

	/* Init circular RX list. */
	if (nge_list_rx_init(sc) == ENOBUFS) {
		printf("%s: initialization failed: no "
			"memory for rx buffers\n", sc->sc_dv.dv_xname);
		nge_stop(sc);
		splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	nge_list_tx_init(sc);

	/*
	 * For the NatSemi chip, we have to explicitly enable the
	 * reception of ARP frames, as well as turn on the 'perfect
	 * match' filter where we store the station address, otherwise
	 * we won't receive unicasts meant for this host.
	 */
	NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ARP);
	NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_PERFECT);

	 /* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ALLPHYS);
	} else {
		NGE_CLRBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ALLPHYS);
	}

	/*
	 * Set the capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_BROAD);
	} else {
		NGE_CLRBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_BROAD);
	}

	/*
	 * Load the multicast filter.
	 */
	nge_setmulti(sc);

	/* Turn the receive filter on */
	NGE_SETBIT(sc, NGE_RXFILT_CTL, NGE_RXFILTCTL_ENABLE);

	/*
	 * Load the address of the RX and TX lists.
	 */
	CSR_WRITE_4(sc, NGE_RX_LISTPTR,
	    vtophys(&sc->nge_ldata->nge_rx_list[0]));
	CSR_WRITE_4(sc, NGE_TX_LISTPTR,
	    vtophys(&sc->nge_ldata->nge_tx_list[0]));

	/* Set RX configuration */
	CSR_WRITE_4(sc, NGE_RX_CFG, NGE_RXCFG);

	/*
	 * Enable hardware checksum validation for all IPv4
	 * packets, do not reject packets with bad checksums.
	 */
	CSR_WRITE_4(sc, NGE_VLAN_IP_RXCTL, NGE_VIPRXCTL_IPCSUM_ENB);

#if NVLAN > 0
	/*
	 * If VLAN support is enabled, tell the chip to detect
	 * and strip VLAN tag info from received frames. The tag
	 * will be provided in the extsts field in the RX descriptors.
	 */
	if (ifp->if_capabilities & IFCAP_VLAN_HWTAGGING)
		NGE_SETBIT(sc, NGE_VLAN_IP_RXCTL,
		    NGE_VIPRXCTL_TAG_DETECT_ENB|NGE_VIPRXCTL_TAG_STRIP_ENB);
#endif

	/* Set TX configuration */
	CSR_WRITE_4(sc, NGE_TX_CFG, NGE_TXCFG);

	/*
	 * Enable TX IPv4 checksumming on a per-packet basis.
	 */
	CSR_WRITE_4(sc, NGE_VLAN_IP_TXCTL, NGE_VIPTXCTL_CSUM_PER_PKT);

#if NVLAN > 0
	/*
	 * If VLAN support is enabled, tell the chip to insert
	 * VLAN tags on a per-packet basis as dictated by the
	 * code in the frame encapsulation routine.
	 */
	if (ifp->if_capabilities & IFCAP_VLAN_HWTAGGING)
		NGE_SETBIT(sc, NGE_VLAN_IP_TXCTL, NGE_VIPTXCTL_TAG_PER_PKT);
#endif

	/* Set full/half duplex mode. */
	if (sc->nge_tbi)
		media = sc->nge_ifmedia.ifm_cur->ifm_media;
	else
		media = mii->mii_media_active;

	txcfg = CSR_READ_4(sc, NGE_TX_CFG);
	rxcfg = CSR_READ_4(sc, NGE_RX_CFG);

	DPRINTFN(4, ("%s: nge_init txcfg=%#x, rxcfg=%#x\n",
		     sc->sc_dv.dv_xname, txcfg, rxcfg));

	if ((media & IFM_GMASK) == IFM_FDX) {
		txcfg |= (NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR);
		rxcfg |= (NGE_RXCFG_RX_FDX);
	} else {
		txcfg &= ~(NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR);
		rxcfg &= ~(NGE_RXCFG_RX_FDX);
	}

	txcfg |= NGE_TXCFG_AUTOPAD;
	
	CSR_WRITE_4(sc, NGE_TX_CFG, txcfg);
	CSR_WRITE_4(sc, NGE_RX_CFG, rxcfg);

	nge_tick(sc);

	/*
	 * Enable the delivery of PHY interrupts based on
	 * link/speed/duplex status changes and enable return
	 * of extended status information in the DMA descriptors,
	 * required for checksum offloading.
	 */
	NGE_SETBIT(sc, NGE_CFG, NGE_CFG_PHYINTR_SPD|NGE_CFG_PHYINTR_LNK|
		   NGE_CFG_PHYINTR_DUP|NGE_CFG_EXTSTS_ENB);

	DPRINTFN(1, ("%s: nge_init: config=%#x\n", sc->sc_dv.dv_xname,
		     CSR_READ_4(sc, NGE_CFG)));

	/*
	 * Configure interrupt holdoff (moderation). We can
	 * have the chip delay interrupt delivery for a certain
	 * period. Units are in 100us, and the max setting
	 * is 25500us (0xFF x 100us). Default is a 100us holdoff.
	 */
	CSR_WRITE_4(sc, NGE_IHR, 0x01);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, NGE_IMR, NGE_INTRS);
	CSR_WRITE_4(sc, NGE_IER, 1);

	/* Enable receiver and transmitter. */
	NGE_CLRBIT(sc, NGE_CSR, NGE_CSR_TX_DISABLE|NGE_CSR_RX_DISABLE);
	NGE_SETBIT(sc, NGE_CSR, NGE_CSR_RX_ENABLE);

	if (sc->nge_tbi)
	    nge_ifmedia_tbi_upd(ifp);
	else
	    nge_ifmedia_mii_upd(ifp);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);
}

/*
 * Set mii media options.
 */
int
nge_ifmedia_mii_upd(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc = ifp->if_softc;
	struct mii_data 	*mii = &sc->nge_mii;

	DPRINTFN(2, ("%s: nge_ifmedia_mii_upd\n", sc->sc_dv.dv_xname));

	sc->nge_link = 0;

	if (mii->mii_instance) {
		struct mii_softc	*miisc;
		for (miisc = LIST_FIRST(&mii->mii_phys); miisc != NULL;
		     miisc = LIST_NEXT(miisc, mii_list))
			mii_phy_reset(miisc);
	}
	mii_mediachg(mii);

	return(0);
}

/*
 * Report current mii media status.
 */
void
nge_ifmedia_mii_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct nge_softc	*sc = ifp->if_softc;
	struct mii_data *mii = &sc->nge_mii;

	DPRINTFN(2, ("%s: nge_ifmedia_mii_sts\n", sc->sc_dv.dv_xname));

	mii_pollstat(mii);
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

/*
 * Set mii media options.
 */
int
nge_ifmedia_tbi_upd(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc = ifp->if_softc;

	DPRINTFN(2, ("%s: nge_ifmedia_tbi_upd\n", sc->sc_dv.dv_xname));

	sc->nge_link = 0;

	if (IFM_SUBTYPE(sc->nge_ifmedia.ifm_cur->ifm_media) 
	    == IFM_AUTO) {
		u_int32_t anar, bmcr;
		anar = CSR_READ_4(sc, NGE_TBI_ANAR);
		anar |= (NGE_TBIANAR_HDX | NGE_TBIANAR_FDX);
		CSR_WRITE_4(sc, NGE_TBI_ANAR, anar);

		bmcr = CSR_READ_4(sc, NGE_TBI_BMCR);
		bmcr |= (NGE_TBIBMCR_ENABLE_ANEG|NGE_TBIBMCR_RESTART_ANEG);
		CSR_WRITE_4(sc, NGE_TBI_BMCR, bmcr);

		bmcr &= ~(NGE_TBIBMCR_RESTART_ANEG);
		CSR_WRITE_4(sc, NGE_TBI_BMCR, bmcr);
	} else {
		u_int32_t txcfg, rxcfg;
		txcfg = CSR_READ_4(sc, NGE_TX_CFG);
		rxcfg = CSR_READ_4(sc, NGE_RX_CFG);

		if ((sc->nge_ifmedia.ifm_cur->ifm_media & IFM_GMASK)
		    == IFM_FDX) {
			txcfg |= NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR;
			rxcfg |= NGE_RXCFG_RX_FDX;
		} else {
			txcfg &= ~(NGE_TXCFG_IGN_HBEAT|NGE_TXCFG_IGN_CARR);
			rxcfg &= ~(NGE_RXCFG_RX_FDX);
		}

		txcfg |= NGE_TXCFG_AUTOPAD;
		CSR_WRITE_4(sc, NGE_TX_CFG, txcfg);
		CSR_WRITE_4(sc, NGE_RX_CFG, rxcfg);
	}
	
	NGE_CLRBIT(sc, NGE_GPIO, NGE_GPIO_GP3_OUT);

	return(0);
}

/*
 * Report current tbi media status.
 */
void
nge_ifmedia_tbi_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct nge_softc	*sc = ifp->if_softc;
	u_int32_t		bmcr;

	bmcr = CSR_READ_4(sc, NGE_TBI_BMCR);
	
	if (IFM_SUBTYPE(sc->nge_ifmedia.ifm_cur->ifm_media) == IFM_AUTO) {
		u_int32_t bmsr = CSR_READ_4(sc, NGE_TBI_BMSR);
		DPRINTFN(2, ("%s: nge_ifmedia_tbi_sts bmsr=%#x, bmcr=%#x\n",
			     sc->sc_dv.dv_xname, bmsr, bmcr));
	
		if (!(bmsr & NGE_TBIBMSR_ANEG_DONE)) {
			ifmr->ifm_active = IFM_ETHER|IFM_NONE;
			ifmr->ifm_status = IFM_AVALID;
			return;
		}
	} else {
		DPRINTFN(2, ("%s: nge_ifmedia_tbi_sts bmcr=%#x\n",
			     sc->sc_dv.dv_xname, bmcr));
	}
		
	ifmr->ifm_status = IFM_AVALID|IFM_ACTIVE;
	ifmr->ifm_active = IFM_ETHER|IFM_1000_SX;
	
	if (bmcr & NGE_TBIBMCR_LOOPBACK)
		ifmr->ifm_active |= IFM_LOOP;
	
	if (IFM_SUBTYPE(sc->nge_ifmedia.ifm_cur->ifm_media) == IFM_AUTO) {
		u_int32_t anlpar = CSR_READ_4(sc, NGE_TBI_ANLPAR);
		DPRINTFN(2, ("%s: nge_ifmedia_tbi_sts anlpar=%#x\n",
			     sc->sc_dv.dv_xname, anlpar));
		
		ifmr->ifm_active |= IFM_AUTO;
		if (anlpar & NGE_TBIANLPAR_FDX) {
			ifmr->ifm_active |= IFM_FDX;
		} else if (anlpar & NGE_TBIANLPAR_HDX) {
			ifmr->ifm_active |= IFM_HDX;
		} else
			ifmr->ifm_active |= IFM_FDX;
		
	} else if ((sc->nge_ifmedia.ifm_cur->ifm_media & IFM_GMASK) == IFM_FDX)
		ifmr->ifm_active |= IFM_FDX;
	else
		ifmr->ifm_active |= IFM_HDX;
	
}

int
nge_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct nge_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct mii_data		*mii;
	int			s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->arpcom, command, data)) > 0) {
		splx(s);
		return (error);
	}

	switch(command) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > NGE_JUMBO_MTU || ifr->ifr_mtu < ETHERMIN)
			error = EINVAL;
		else {
			ifp->if_mtu = ifr->ifr_mtu;
			/*
			 * Workaround: if the MTU is larger than
			 * 8152 (TX FIFO size minus 64 minus 18), turn off
			 * TX checksum offloading.
			 */
			if (ifr->ifr_mtu >= 8152)
				ifp->if_capabilities &= ~(IFCAP_CSUM_IPv4 |
				    IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4);
			else
				ifp->if_capabilities = IFCAP_CSUM_IPv4 |
					IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;
		}
		break;
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			nge_init(sc);
			arp_ifinit(&sc->arpcom, ifa);
			break;
#endif /* INET */
		default:
			nge_init(sc);
			break;
                }
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->nge_if_flags & IFF_PROMISC)) {
				NGE_SETBIT(sc, NGE_RXFILT_CTL,
				    NGE_RXFILTCTL_ALLPHYS|
				    NGE_RXFILTCTL_ALLMULTI);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->nge_if_flags & IFF_PROMISC) {
				NGE_CLRBIT(sc, NGE_RXFILT_CTL,
				    NGE_RXFILTCTL_ALLPHYS);
				if (!(ifp->if_flags & IFF_ALLMULTI))
					NGE_CLRBIT(sc, NGE_RXFILT_CTL,
					    NGE_RXFILTCTL_ALLMULTI);
			} else {
				ifp->if_flags &= ~IFF_RUNNING;
				nge_init(sc);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				nge_stop(sc);
		}
		sc->nge_if_flags = ifp->if_flags;
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI)
			? ether_addmulti(ifr, &sc->arpcom)
			: ether_delmulti(ifr, &sc->arpcom);

		if (error == ENETRESET) {
			if (ifp->if_flags & IFF_RUNNING)
				nge_setmulti(sc);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->nge_tbi) {
			error = ifmedia_ioctl(ifp, ifr, &sc->nge_ifmedia, 
					      command);
		} else {
			mii = &sc->nge_mii;
			error = ifmedia_ioctl(ifp, ifr, &mii->mii_media,
					      command);
		}
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return(error);
}

void
nge_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct nge_softc	*sc;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", sc->sc_dv.dv_xname);

	nge_stop(sc);
	nge_reset(sc);
	ifp->if_flags &= ~IFF_RUNNING;
	nge_init(sc);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		nge_start(ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
nge_stop(sc)
	struct nge_softc	*sc;
{
	int			i;
	struct ifnet		*ifp;
	struct mii_data		*mii;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;
	if (sc->nge_tbi) {
		mii = NULL;
	} else {
		mii = &sc->nge_mii;
	}

	timeout_del(&sc->nge_timeout);
	CSR_WRITE_4(sc, NGE_IER, 0);
	CSR_WRITE_4(sc, NGE_IMR, 0);
	NGE_SETBIT(sc, NGE_CSR, NGE_CSR_TX_DISABLE|NGE_CSR_RX_DISABLE);
	DELAY(1000);
	CSR_WRITE_4(sc, NGE_TX_LISTPTR, 0);
	CSR_WRITE_4(sc, NGE_RX_LISTPTR, 0);

	if (!sc->nge_tbi)
		mii_down(mii);

	sc->nge_link = 0;

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < NGE_RX_LIST_CNT; i++) {
		if (sc->nge_ldata->nge_rx_list[i].nge_mbuf != NULL) {
			m_freem(sc->nge_ldata->nge_rx_list[i].nge_mbuf);
			sc->nge_ldata->nge_rx_list[i].nge_mbuf = NULL;
		}
	}
	bzero((char *)&sc->nge_ldata->nge_rx_list,
		sizeof(sc->nge_ldata->nge_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < NGE_TX_LIST_CNT; i++) {
		if (sc->nge_ldata->nge_tx_list[i].nge_mbuf != NULL) {
			m_freem(sc->nge_ldata->nge_tx_list[i].nge_mbuf);
			sc->nge_ldata->nge_tx_list[i].nge_mbuf = NULL;
		}
	}

	bzero((char *)&sc->nge_ldata->nge_tx_list,
		sizeof(sc->nge_ldata->nge_tx_list));

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
void
nge_shutdown(xsc)
	void *xsc;
{
	struct nge_softc *sc = (struct nge_softc *)xsc;

	nge_reset(sc);
	nge_stop(sc);
}

struct cfattach nge_ca = {
	sizeof(struct nge_softc), nge_probe, nge_attach
};

struct cfdriver nge_cd = {
	0, "nge", DV_IFNET
};
