/*	$OpenBSD: if_re.c,v 1.1 2004/06/05 06:13:06 pvalchev Exp $	*/
/*
 * Copyright (c) 1997, 1998-2003
 *	Bill Paul <wpaul@windriver.com>.  All rights reserved.
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
 * $FreeBSD: /repoman/r/ncvs/src/sys/dev/re/if_re.c,v 1.20 2004/04/11 20:34:08 ru Exp $
 */

/*
 * RealTek 8139C+/8169/8169S/8110S PCI NIC driver
 *
 * Written by Bill Paul <wpaul@windriver.com>
 * Senior Networking Software Engineer
 * Wind River Systems
 */

/*
 * This driver is designed to support RealTek's next generation of
 * 10/100 and 10/100/1000 PCI ethernet controllers. There are currently
 * four devices in this family: the RTL8139C+, the RTL8169, the RTL8169S
 * and the RTL8110S.
 *
 * The 8139C+ is a 10/100 ethernet chip. It is backwards compatible
 * with the older 8139 family, however it also supports a special
 * C+ mode of operation that provides several new performance enhancing
 * features. These include:
 *
 *	o Descriptor based DMA mechanism. Each descriptor represents
 *	  a single packet fragment. Data buffers may be aligned on
 *	  any byte boundary.
 *
 *	o 64-bit DMA
 *
 *	o TCP/IP checksum offload for both RX and TX
 *
 *	o High and normal priority transmit DMA rings
 *
 *	o VLAN tag insertion and extraction
 *
 *	o TCP large send (segmentation offload)
 *
 * Like the 8139, the 8139C+ also has a built-in 10/100 PHY. The C+
 * programming API is fairly straightforward. The RX filtering, EEPROM
 * access and PHY access is the same as it is on the older 8139 series
 * chips.
 *
 * The 8169 is a 64-bit 10/100/1000 gigabit ethernet MAC. It has almost the
 * same programming API and feature set as the 8139C+ with the following
 * differences and additions:
 *
 *	o 1000Mbps mode
 *
 *	o Jumbo frames
 *
 * 	o GMII and TBI ports/registers for interfacing with copper
 *	  or fiber PHYs
 *
 *      o RX and TX DMA rings can have up to 1024 descriptors
 *        (the 8139C+ allows a maximum of 64)
 *
 *	o Slight differences in register layout from the 8139C+
 *
 * The TX start and timer interrupt registers are at different locations
 * on the 8169 than they are on the 8139C+. Also, the status word in the
 * RX descriptor has a slightly different bit layout. The 8169 does not
 * have a built-in PHY. Most reference boards use a Marvell 88E1000 'Alaska'
 * copper gigE PHY.
 *
 * The 8169S/8110S 10/100/1000 devices have built-in copper gigE PHYs
 * (the 'S' stands for 'single-chip'). These devices have the same
 * programming API as the older 8169, but also have some vendor-specific
 * registers for the on-board PHY. The 8110S is a LAN-on-motherboard
 * part designed to be pin-compatible with the RealTek 8100 10/100 chip.
 * 
 * This driver takes advantage of the RX and TX checksum offload and
 * VLAN tag insertion/extraction features. It also implements TX
 * interrupt moderation using the timer interrupt registers, which
 * significantly reduces TX interrupt load. There is also support
 * for jumbo frames, however the 8169/8169S/8110S can not transmit
 * jumbo frames larger than 7.5K, so the max MTU possible with this
 * driver is 7500 bytes.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/endian.h>
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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/*#define RE_CSUM_OFFLOAD */

#include <dev/ic/rtl81x9reg.h>

struct re_pci_softc {
	struct rl_softc sc_rl;

	void *sc_ih;
	pci_chipset_tag_t sc_pc;
	pcitag_t sc_pcitag;
};

int redebug = 0;
#define DPRINTF(x)	if (redebug) printf x

/* XXX add the rest from pcidevs */
const struct pci_matchid re_devices[] = {
	{ PCI_VENDOR_REALTEK, PCI_PRODUCT_REALTEK_RT8169 },
};

int re_probe		(struct device *, void *, void *);
void re_attach		(struct device *, struct device *, void *);

int re_encap		(struct rl_softc *, struct mbuf *, int *);

int re_allocmem		(struct rl_softc *);
int re_newbuf		(struct rl_softc *, int, struct mbuf *);
int re_rx_list_init	(struct rl_softc *);
int re_tx_list_init	(struct rl_softc *);
void re_rxeof		(struct rl_softc *);
void re_txeof		(struct rl_softc *);
int re_intr		(void *);
void re_tick		(void *);
void re_start		(struct ifnet *);
int re_ioctl		(struct ifnet *, u_long, caddr_t);
int re_init		(struct ifnet *);
void re_stop		(struct rl_softc *);
void re_watchdog	(struct ifnet *);
int re_ifmedia_upd	(struct ifnet *);
void re_ifmedia_sts	(struct ifnet *, struct ifmediareq *);

void re_eeprom_putbyte	(struct rl_softc *, int);
void re_eeprom_getword	(struct rl_softc *, int, u_int16_t *);
void re_read_eeprom	(struct rl_softc *, caddr_t, int, int, int);

int re_gmii_readreg	(struct device *, int, int);
void re_gmii_writereg	(struct device *, int, int, int);

int re_miibus_readreg	(struct device *, int, int);
void re_miibus_writereg	(struct device *, int, int, int);
void re_miibus_statchg	(struct device *);

void re_reset		(struct rl_softc *);

int re_diag		(struct rl_softc *);

struct cfattach re_ca = {
	sizeof(struct re_pci_softc), re_probe, re_attach
};

struct cfdriver re_cd = {
	0, "re", DV_IFNET
};

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) & ~x)

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
void
re_eeprom_putbyte(sc, addr)
	struct rl_softc		*sc;
	int			addr;
{
	register int		d, i;

	d = addr | sc->rl_eecmd_read;

	/*
	 * Feed in each bit and strobe the clock.
	 */
	for (i = 0x400; i; i >>= 1) {
		if (d & i) {
			EE_SET(RL_EE_DATAIN);
		} else {
			EE_CLR(RL_EE_DATAIN);
		}
		DELAY(100);
		EE_SET(RL_EE_CLK);
		DELAY(150);
		EE_CLR(RL_EE_CLK);
		DELAY(100);
	}
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
void
re_eeprom_getword(sc, addr, dest)
	struct rl_softc		*sc;
	int			addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Enter EEPROM access mode. */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_PROGRAM|RL_EE_SEL);

	/*
	 * Send address of word we want to read.
	 */
	re_eeprom_putbyte(sc, addr);

	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_PROGRAM|RL_EE_SEL);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		EE_SET(RL_EE_CLK);
		DELAY(100);
		if (CSR_READ_1(sc, RL_EECMD) & RL_EE_DATAOUT)
			word |= i;
		EE_CLR(RL_EE_CLK);
		DELAY(100);
	}

	/* Turn off EEPROM access mode. */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	*dest = word;
}

/*
 * Read a sequence of words from the EEPROM.
 */
void
re_read_eeprom(sc, dest, off, cnt, swap)
	struct rl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		re_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}
}

int
re_gmii_readreg(struct device *self, int phy, int reg)
{
	struct rl_softc	*sc = (struct rl_softc *)self;
	u_int32_t		rval;
	int			i;

	if (phy != 7) // XXX 1 ??
		return(0);

	/* Let the rgephy driver read the GMEDIASTAT register */

	if (reg == RL_GMEDIASTAT) {
		rval = CSR_READ_1(sc, RL_GMEDIASTAT);
		return(rval);
	}

	CSR_WRITE_4(sc, RL_PHYAR, reg << 16);
	DELAY(1000);

	for (i = 0; i < RL_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RL_PHYAR);
		if (rval & RL_PHYAR_BUSY)
			break;
		DELAY(100);
	}

	if (i == RL_TIMEOUT) {
		printf ("%s: PHY read failed\n", sc->sc_dev.dv_xname);
		return (0);
	}

	return (rval & RL_PHYAR_PHYDATA);
}

void
re_gmii_writereg(struct device *dev, int phy, int reg, int data)
{
	struct rl_softc	*sc = (struct rl_softc *)dev;
	u_int32_t		rval;
	int			i;

	CSR_WRITE_4(sc, RL_PHYAR, (reg << 16) |
	    (data & RL_PHYAR_PHYDATA) | RL_PHYAR_BUSY);
	DELAY(1000);

	for (i = 0; i < RL_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RL_PHYAR);
		if (!(rval & RL_PHYAR_BUSY))
			break;
		DELAY(100);
	}

	if (i == RL_TIMEOUT) {
		printf ("%s: PHY write failed\n", sc->sc_dev.dv_xname);
	}
}

int
re_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct rl_softc	*sc = (struct rl_softc *)dev;
	u_int16_t		rval = 0;
	u_int16_t		re8139_reg = 0;
	int			s;

	s = splimp();

	if (sc->rl_type == RL_8169) {
		rval = re_gmii_readreg(dev, phy, reg);
		splx(s);
		return (rval);
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy) {
		splx(s);
		return(0);
	}
	switch(reg) {
	case MII_BMCR:
		re8139_reg = RL_BMCR;
		break;
	case MII_BMSR:
		re8139_reg = RL_BMSR;
		break;
	case MII_ANAR:
		re8139_reg = RL_ANAR;
		break;
	case MII_ANER:
		re8139_reg = RL_ANER;
		break;
	case MII_ANLPAR:
		re8139_reg = RL_LPAR;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		splx(s);
		return(0);
	/*
	 * Allow the rlphy driver to read the media status
	 * register. If we have a link partner which does not
	 * support NWAY, this is the register which will tell
	 * us the results of parallel detection.
	 */
	case RL_MEDIASTAT:
		rval = CSR_READ_1(sc, RL_MEDIASTAT);
		splx(s);
		return(rval);
	default:
		printf("%s: bad phy register\n", sc->sc_dev.dv_xname);
		splx(s);
		return(0);
	}
	rval = CSR_READ_2(sc, re8139_reg);
	splx(s);
	return(rval);
}

void
re_miibus_writereg(struct device *dev, int phy, int reg, int data)
{
	struct rl_softc	*sc = (struct rl_softc *)dev;
	u_int16_t		re8139_reg = 0;
	int			s;

	s = splimp();

	if (sc->rl_type == RL_8169) {
		re_gmii_writereg(dev, phy, reg, data);
		splx(s);
		return;
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy) {
		splx(s);
		return;
	}
	switch(reg) {
	case MII_BMCR:
		re8139_reg = RL_BMCR;
		break;
	case MII_BMSR:
		re8139_reg = RL_BMSR;
		break;
	case MII_ANAR:
		re8139_reg = RL_ANAR;
		break;
	case MII_ANER:
		re8139_reg = RL_ANER;
		break;
	case MII_ANLPAR:
		re8139_reg = RL_LPAR;
		break;
	case MII_PHYIDR1:
	case MII_PHYIDR2:
		splx(s);
		return;
		break;
	default:
		printf("%s: bad phy register\n", sc->sc_dev.dv_xname);
		splx(s);
		return;
	}
	CSR_WRITE_2(sc, re8139_reg, data);
	splx(s);
	return;
}

void
re_miibus_statchg(struct device *dev)
{
	return;
}

void
re_reset(sc)
	struct rl_softc	*sc;
{
	register int		i;

	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_RESET);

	for (i = 0; i < RL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_1(sc, RL_COMMAND) & RL_CMD_RESET))
			break;
	}
	if (i == RL_TIMEOUT)
		printf("%s: reset never completed!\n", sc->sc_dev.dv_xname);

	CSR_WRITE_1(sc, 0x82, 1);
}

/*
 * The following routine is designed to test for a defect on some
 * 32-bit 8169 cards. Some of these NICs have the REQ64# and ACK64#
 * lines connected to the bus, however for a 32-bit only card, they
 * should be pulled high. The result of this defect is that the
 * NIC will not work right if you plug it into a 64-bit slot: DMA
 * operations will be done with 64-bit transfers, which will fail
 * because the 64-bit data lines aren't connected.
 *
 * There's no way to work around this (short of talking a soldering
 * iron to the board), however we can detect it. The method we use
 * here is to put the NIC into digital loopback mode, set the receiver
 * to promiscuous mode, and then try to send a frame. We then compare
 * the frame data we sent to what was received. If the data matches,
 * then the NIC is working correctly, otherwise we know the user has
 * a defective NIC which has been mistakenly plugged into a 64-bit PCI
 * slot. In the latter case, there's no way the NIC can work correctly,
 * so we print out a message on the console and abort the device attach.
 */

int
re_diag(sc)
	struct rl_softc	*sc;
{
	struct ifnet		*ifp = &sc->sc_arpcom.ac_if;
	struct mbuf		*m0;
	struct ether_header	*eh;
	struct rl_desc		*cur_rx;
	bus_dmamap_t		dmamap;
	u_int16_t		status;
	u_int32_t		rxstat;
	int			total_len, i, s, error = 0;
	u_int8_t		dst[] = { 0x00, 'h', 'e', 'l', 'l', 'o' };
	u_int8_t		src[] = { 0x00, 'w', 'o', 'r', 'l', 'd' };

	DPRINTF(("inside re_diag\n"));
	/* Allocate a single mbuf */

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return(ENOBUFS);

	/*
	 * Initialize the NIC in test mode. This sets the chip up
	 * so that it can send and receive frames, but performs the
	 * following special functions:
	 * - Puts receiver in promiscuous mode
	 * - Enables digital loopback mode
	 * - Leaves interrupts turned off
	 */

	ifp->if_flags |= IFF_PROMISC;
	sc->rl_testmode = 1;
	re_init(ifp);
	re_stop(sc);
	DELAY(100000);
	re_init(ifp);

	/* Put some data in the mbuf */

	eh = mtod(m0, struct ether_header *);
	bcopy ((char *)&dst, eh->ether_dhost, ETHER_ADDR_LEN);
	bcopy ((char *)&src, eh->ether_shost, ETHER_ADDR_LEN);
	eh->ether_type = htons(ETHERTYPE_IP);
	m0->m_pkthdr.len = m0->m_len = ETHER_MIN_LEN - ETHER_CRC_LEN;

	/*
	 * Queue the packet, start transmission.
	 */

	CSR_WRITE_2(sc, RL_ISR, 0xFFFF);
	s = splnet();
	IF_ENQUEUE(&ifp->if_snd, m0);
	re_start(ifp);
	splx(s);
	m0 = NULL;

	DPRINTF(("re_diag: transmission started\n"));

	/* Wait for it to propagate through the chip */

	DELAY(100000);
	for (i = 0; i < RL_TIMEOUT; i++) {
		status = CSR_READ_2(sc, RL_ISR);
		if ((status & (RL_ISR_TIMEOUT_EXPIRED|RL_ISR_RX_OK)) ==
		    (RL_ISR_TIMEOUT_EXPIRED|RL_ISR_RX_OK))
			break;
		DELAY(10);
	}
	if (i == RL_TIMEOUT) {
		printf("%s: diagnostic failed, failed to receive packet "
		    "in loopback mode\n", sc->sc_dev.dv_xname);
		error = EIO;
		goto done;
	}

	/*
	 * The packet should have been dumped into the first
	 * entry in the RX DMA ring. Grab it from there.
	 */

	dmamap = sc->rl_ldata.rl_rx_list_map;
	bus_dmamap_sync(sc->sc_dmat,
	    dmamap, 0, dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
	dmamap = sc->rl_ldata.rl_rx_dmamap[0];
	bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(sc->sc_dmat,
	    sc->rl_ldata.rl_rx_dmamap[0]);

	m0 = sc->rl_ldata.rl_rx_mbuf[0];
	sc->rl_ldata.rl_rx_mbuf[0] = NULL;
	eh = mtod(m0, struct ether_header *);

	cur_rx = &sc->rl_ldata.rl_rx_list[0];
	total_len = RL_RXBYTES(cur_rx);
	rxstat = letoh32(cur_rx->rl_cmdstat);

	if (total_len != ETHER_MIN_LEN) {
		printf("%s: diagnostic failed, received short packet\n",
		    sc->sc_dev.dv_xname);
		error = EIO;
		goto done;
	}

	DPRINTF(("re_diag: packet received\n"));

	/* Test that the received packet data matches what we sent. */

	if (bcmp((char *)&eh->ether_dhost, (char *)&dst, ETHER_ADDR_LEN) ||
	    bcmp((char *)&eh->ether_shost, (char *)&src, ETHER_ADDR_LEN) ||
	    ntohs(eh->ether_type) != ETHERTYPE_IP) {
		printf("%s: WARNING, DMA FAILURE!\n", sc->sc_dev.dv_xname);
		printf("%s: expected TX data: %s",
		    sc->sc_dev.dv_xname, ether_sprintf(dst));
		printf("/%s/0x%x\n", ether_sprintf(src), ETHERTYPE_IP);
		printf("%s: received RX data: %s",
		    sc->sc_dev.dv_xname,
		    ether_sprintf(eh->ether_dhost));
		printf("/%s/0x%x\n", ether_sprintf(eh->ether_shost),
		    ntohs(eh->ether_type));
		printf("%s: You may have a defective 32-bit NIC plugged "
		    "into a 64-bit PCI slot.\n", sc->sc_dev.dv_xname);
		printf("%s: Please re-install the NIC in a 32-bit slot "
		    "for proper operation.\n", sc->sc_dev.dv_xname);
		printf("%s: Read the re(4) man page for more details.\n",
		    sc->sc_dev.dv_xname);
		error = EIO;
	}

done:
	/* Turn interface off, release resources */

	sc->rl_testmode = 0;
	ifp->if_flags &= ~IFF_PROMISC;
	re_stop(sc);
	if (m0 != NULL)
		m_freem(m0);
	DPRINTF(("leaving re_diag\n"));

	return (error);
}

/*
 * Probe for a RealTek 8139C+/8169/8110 chip. Check the PCI vendor and device
 * IDs against our list and return a device name if we find a match.
 */
int
re_probe(struct device *parent, void *match, void *aux)
{
	return (pci_matchbyid((struct pci_attach_args *)aux, re_devices,
	    sizeof(re_devices)/sizeof(re_devices[0])));
}

int
re_allocmem(struct rl_softc *sc)
{
	int			error;
	int			nseg, rseg;
	int			i;

	nseg = 32;

	/* Allocate DMA'able memory for the TX ring */

	error = bus_dmamap_create(sc->sc_dmat, RL_TX_LIST_SZ, 1,
	    RL_TX_LIST_SZ, 0, BUS_DMA_ALLOCNOW,
	    &sc->rl_ldata.rl_tx_list_map);
        error = bus_dmamem_alloc(sc->sc_dmat, RL_TX_LIST_SZ,
	    ETHER_ALIGN, 0, 
	    &sc->rl_ldata.rl_tx_listseg, 1, &rseg, BUS_DMA_NOWAIT);
        if (error)
                return (ENOMEM);

	/* Load the map for the TX ring. */
	error = bus_dmamem_map(sc->sc_dmat, &sc->rl_ldata.rl_tx_listseg,
	    1, RL_TX_LIST_SZ,
	    (caddr_t *)&sc->rl_ldata.rl_tx_list, BUS_DMA_NOWAIT);
	memset(sc->rl_ldata.rl_tx_list, 0, RL_TX_LIST_SZ);

	error = bus_dmamap_load(sc->sc_dmat, sc->rl_ldata.rl_tx_list_map,
	    sc->rl_ldata.rl_tx_list, RL_TX_LIST_SZ, NULL, BUS_DMA_NOWAIT);

	/* Create DMA maps for TX buffers */

	for (i = 0; i < RL_TX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES * nseg, nseg,
		    MCLBYTES, 0, BUS_DMA_ALLOCNOW,
		    &sc->rl_ldata.rl_tx_dmamap[i]);
		if (error) {
			printf("%s: can't create DMA map for TX\n",
			    sc->sc_dev.dv_xname);
			return(ENOMEM);
		}
	}

	/* Allocate DMA'able memory for the RX ring */

	error = bus_dmamap_create(sc->sc_dmat, RL_RX_LIST_SZ, 1,
	    RL_RX_LIST_SZ, 0, BUS_DMA_ALLOCNOW,
	    &sc->rl_ldata.rl_rx_list_map);
        error = bus_dmamem_alloc(sc->sc_dmat, RL_RX_LIST_SZ, RL_RING_ALIGN,
	    0, &sc->rl_ldata.rl_rx_listseg, 1, &rseg, BUS_DMA_NOWAIT);
        if (error)
                return (ENOMEM);

	/* Load the map for the RX ring. */
	error = bus_dmamem_map(sc->sc_dmat, &sc->rl_ldata.rl_rx_listseg,
	    1, RL_RX_LIST_SZ,
	    (caddr_t *)&sc->rl_ldata.rl_rx_list, BUS_DMA_NOWAIT);
	memset(sc->rl_ldata.rl_rx_list, 0, RL_TX_LIST_SZ);

	error = bus_dmamap_load(sc->sc_dmat, sc->rl_ldata.rl_rx_list_map,
	     sc->rl_ldata.rl_rx_list, RL_RX_LIST_SZ, NULL, BUS_DMA_NOWAIT);

	/* Create DMA maps for RX buffers */

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES * nseg, nseg,
		    MCLBYTES, 0, BUS_DMA_ALLOCNOW,
		    &sc->rl_ldata.rl_rx_dmamap[i]);
		if (error) {
			printf("%s: can't create DMA map for RX\n",
			    sc->sc_dev.dv_xname);
			return(ENOMEM);
		}
	}

	return(0);
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
re_attach(struct device *parent, struct device *self, void *aux)
{
	u_char			eaddr[ETHER_ADDR_LEN];
	u_int16_t		as[3];
	struct re_pci_softc	*psc = (struct re_pci_softc *)self;
	struct rl_softc	*sc = &psc->sc_rl;
	struct pci_attach_args 	*pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	struct ifnet		*ifp;
	u_int16_t		re_did = 0;
	int			error = 0, i;
	bus_size_t		iosize;
	pcireg_t		command;

#ifndef BURN_BRIDGES
	/*
	 * Handle power management nonsense.
	 */

	command = pci_conf_read(pc, pa->pa_tag, RL_PCI_CAPID) & 0x000000FF;

	if (command == 0x01) {
		u_int32_t		iobase, membase, irq;

		/* Save important PCI config data. */
		iobase = pci_conf_read(pc, pa->pa_tag,  RL_PCI_LOIO);
		membase = pci_conf_read(pc, pa->pa_tag, RL_PCI_LOMEM);
		irq = pci_conf_read(pc, pa->pa_tag, RL_PCI_INTLINE);

		/* Reset the power state. */
		printf("%s: chip is is in D%d power mode "
		    "-- setting to D0\n", sc->sc_dev.dv_xname,
		    command & RL_PSTATE_MASK);
		command &= 0xFFFFFFFC;

		/* Restore PCI config data. */
		pci_conf_write(pc, pa->pa_tag, RL_PCI_LOIO, iobase);
		pci_conf_write(pc, pa->pa_tag, RL_PCI_LOMEM, membase);
		pci_conf_write(pc, pa->pa_tag, RL_PCI_INTLINE, irq);
	}
#endif

	/*
	 * Map control/status registers.
	 */
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);
	command |= PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE |
	    PCI_COMMAND_MASTER_ENABLE;
	pci_conf_write(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG, command);
	command = pci_conf_read(pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

	if ((command & (PCI_COMMAND_IO_ENABLE | PCI_COMMAND_MEM_ENABLE)) == 0) {
		printf(": neither i/o nor mem enabled\n");
		return;
	}

	if (command & PCI_COMMAND_MEM_ENABLE) {
		if (pci_mapreg_map(pa, RL_PCI_LOMEM, PCI_MAPREG_TYPE_MEM, 0,
		    &sc->rl_btag, &sc->rl_bhandle, NULL, &iosize, 0)) {
			printf(": can't map mem space\n");
			return;
		}
	} else {
		if (pci_mapreg_map(pa, RL_PCI_LOIO, PCI_MAPREG_TYPE_IO, 0,
		    &sc->rl_btag, &sc->rl_bhandle, NULL, &iosize, 0)) {
			printf(": can't map i/o space\n");
			return;
		}
	}

	/* Allocate interrupt */
	if (pci_intr_map(pa, &ih)) {
		printf("%s: couldn't map interrupt\n", sc->sc_dev.dv_xname);
		error = ENXIO;
		goto fail;
	}
	intrstr = pci_intr_string(pc, ih);
	psc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, re_intr, sc,
	    sc->sc_dev.dv_xname);
	if (psc->sc_ih == NULL) {
		printf("%s: couldn't establish interrupt",
		    sc->sc_dev.dv_xname);
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		goto fail;
	}
	printf(": %s", intrstr);

	sc->sc_dmat = pa->pa_dmat;
	sc->sc_flags |= RL_ENABLED;

	/* Reset the adapter. */
	re_reset(sc);

#if 0
	hw_rev = re_hwrevs;
	hwrev = CSR_READ_4(sc, RL_TXCFG) & RL_TXCFG_HWREV;
	while (hw_rev->rl_desc != NULL) {
		if (hw_rev->rl_rev == hwrev) {
			sc->rl_type = hw_rev->rl_type;
			break;
		}
		hw_rev++;
	}
#endif
	/* XXX Add proper check */
	sc->rl_type = RL_8169;

	if (sc->rl_type == RL_8169) {

		/* Set RX length mask */

		sc->rl_rxlenmask = RL_RDESC_STAT_GFRAGLEN;

		/* Force station address autoload from the EEPROM */

		CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_AUTOLOAD);
		for (i = 0; i < RL_TIMEOUT; i++) {
			if (!(CSR_READ_1(sc, RL_EECMD) & RL_EEMODE_AUTOLOAD))
				break;
			DELAY(100);
		}
		if (i == RL_TIMEOUT)
			printf ("%s: eeprom autoload timed out\n", sc->sc_dev.dv_xname);

			for (i = 0; i < ETHER_ADDR_LEN; i++)
				eaddr[i] = CSR_READ_1(sc, RL_IDR0 + i);
	} else {

		/* Set RX length mask */

		sc->rl_rxlenmask = RL_RDESC_STAT_FRAGLEN;

		sc->rl_eecmd_read = RL_EECMD_READ_6BIT;
		re_read_eeprom(sc, (caddr_t)&re_did, 0, 1, 0);
		if (re_did != 0x8129)
			sc->rl_eecmd_read = RL_EECMD_READ_8BIT;

		/*
		 * Get station address from the EEPROM.
		 */
		re_read_eeprom(sc, (caddr_t)as, RL_EE_EADDR, 3, 0);
		for (i = 0; i < 3; i++) {
			eaddr[(i * 2) + 0] = as[i] & 0xff;
			eaddr[(i * 2) + 1] = as[i] >> 8;
		}
	}

	bcopy(eaddr, (char *)&sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);

	printf(", address %s\n",
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	error = re_allocmem(sc);

	if (error)
		goto fail;

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = re_ioctl;
#ifdef VLANXXX
	sc->ethercom.ec_capabilities |=
	    ETHERCAP_VLAN_MTU | ETHERCAP_VLAN_HWTAGGING;
#endif
	ifp->if_start = re_start;
#ifdef RE_CSUM_OFFLOAD
	ifp->if_capabilities |=
	    IFCAP_CSUM_IPv4 | IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;
#endif
	ifp->if_watchdog = re_watchdog;
	ifp->if_init = re_init;
	if (sc->rl_type == RL_8169)
		ifp->if_baudrate = 1000000000;
	else
		ifp->if_baudrate = 100000000;
	ifp->if_snd.ifq_maxlen = RL_IFQ_MAXLEN;
	IFQ_SET_READY(&ifp->if_snd);

	timeout_set(&sc->timer_handle, re_tick, sc);

	/* Do MII setup */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = re_miibus_readreg;
	sc->sc_mii.mii_writereg = re_miibus_writereg;
	sc->sc_mii.mii_statchg = re_miibus_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, re_ifmedia_upd,
	    re_ifmedia_sts);
	DPRINTF(("calling mii_attach\n"));
	mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		printf("%s: no PHY found!\n", sc->sc_dev.dv_xname);
		ifmedia_add(&sc->sc_mii.mii_media,
		    IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media,
		    IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	/*
	 * Call MI attach routine.
	 */
	re_reset(sc);
	if_attach(ifp);
	ether_ifattach(ifp);

	/* Perform hardware diagnostic. */
	error = re_diag(sc);

	if (error) {
		printf("%s: attach aborted due to hardware diag failure\n",
		    sc->sc_dev.dv_xname);
		ether_ifdetach(ifp);
		goto fail;
	}

	DPRINTF(("leaving re_attach\n"));

fail:
	return;
}


int
re_newbuf(sc, idx, m)
	struct rl_softc	*sc;
	int			idx;
	struct mbuf		*m;
{
	struct mbuf		*n = NULL;
	bus_dmamap_t		map;
	struct rl_desc		*d;
	u_int32_t		cmdstat;
	int			error;

	if (m == NULL) {
		MGETHDR(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			return(ENOBUFS);
		m = n;

		MCLGET(m, M_DONTWAIT);
		if (! (m->m_flags & M_EXT)) {
			m_freem(m);
			return(ENOBUFS);
		}
	} else
		m->m_data = m->m_ext.ext_buf;

	/*
	 * Initialize mbuf length fields and fixup
	 * alignment so that the frame payload is
	 * longword aligned.
	 */
	m->m_len = m->m_pkthdr.len = MCLBYTES;
	m_adj(m, ETHER_ALIGN);

	map = sc->rl_ldata.rl_rx_dmamap[idx];
        error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m, BUS_DMA_NOWAIT);

	if (map->dm_nsegs > 1)
		goto out;
	if (error)
		goto out;

	d = &sc->rl_ldata.rl_rx_list[idx];
	if (letoh32(d->rl_cmdstat) & RL_RDESC_STAT_OWN)
		goto out;

	cmdstat = map->dm_segs[0].ds_len;
	d->rl_bufaddr_lo = htole32(RL_ADDR_LO(map->dm_segs[0].ds_addr));
	d->rl_bufaddr_hi = htole32(RL_ADDR_HI(map->dm_segs[0].ds_addr));
	cmdstat |= RL_TDESC_CMD_SOF;
	if (idx == (RL_RX_DESC_CNT - 1))
		cmdstat |= RL_TDESC_CMD_EOR;
	d->rl_cmdstat = htole32(cmdstat);

	d->rl_cmdstat |= htole32(RL_TDESC_CMD_EOF);


	sc->rl_ldata.rl_rx_list[idx].rl_cmdstat |= htole32(RL_RDESC_CMD_OWN);
	sc->rl_ldata.rl_rx_mbuf[idx] = m;

        bus_dmamap_sync(sc->sc_dmat, sc->rl_ldata.rl_rx_dmamap[idx], 0,
            sc->rl_ldata.rl_rx_dmamap[idx]->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	return 0;
out:
	if (n != NULL)
		m_freem(n);
	return ENOMEM;
}

int
re_tx_list_init(sc)
	struct rl_softc	*sc;
{
	memset((char *)sc->rl_ldata.rl_tx_list, 0, RL_TX_LIST_SZ);
	memset((char *)&sc->rl_ldata.rl_tx_mbuf, 0,
	    (RL_TX_DESC_CNT * sizeof(struct mbuf *)));

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rl_ldata.rl_tx_list_map, 0,
	    sc->rl_ldata.rl_tx_list_map->dm_mapsize, BUS_DMASYNC_PREWRITE);
	sc->rl_ldata.rl_tx_prodidx = 0;
	sc->rl_ldata.rl_tx_considx = 0;
	sc->rl_ldata.rl_tx_free = RL_TX_DESC_CNT;

	return(0);
}

int
re_rx_list_init(sc)
	struct rl_softc	*sc;
{
	int			i;

	memset((char *)sc->rl_ldata.rl_rx_list, 0, RL_RX_LIST_SZ);
	memset((char *)&sc->rl_ldata.rl_rx_mbuf, 0,
	    (RL_RX_DESC_CNT * sizeof(struct mbuf *)));

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		if (re_newbuf(sc, i, NULL) == ENOBUFS)
			return(ENOBUFS);
	}

	/* Flush the RX descriptors */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rl_ldata.rl_rx_list_map,
	    0, sc->rl_ldata.rl_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_rx_prodidx = 0;
	sc->rl_head = sc->rl_tail = NULL;

	return(0);
}

/*
 * RX handler for C+ and 8169. For the gigE chips, we support
 * the reception of jumbo frames that have been fragmented
 * across multiple 2K mbuf cluster buffers.
 */
void
re_rxeof(sc)
	struct rl_softc	*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			i, total_len;
	struct rl_desc		*cur_rx;
#ifdef VLANXXX
	struct m_tag		*mtag;
#endif
	u_int32_t		rxstat, rxvlan;

	ifp = &sc->sc_arpcom.ac_if;
	i = sc->rl_ldata.rl_rx_prodidx;

	/* Invalidate the descriptor memory */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rl_ldata.rl_rx_list_map,
	    0, sc->rl_ldata.rl_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	while (!RL_OWN(&sc->rl_ldata.rl_rx_list[i])) {

		cur_rx = &sc->rl_ldata.rl_rx_list[i];
		m = sc->rl_ldata.rl_rx_mbuf[i];
		total_len = RL_RXBYTES(cur_rx);
		rxstat = letoh32(cur_rx->rl_cmdstat);
		rxvlan = letoh32(cur_rx->rl_vlanctl);

		/* Invalidate the RX mbuf and unload its map */

		bus_dmamap_sync(sc->sc_dmat,
		    sc->rl_ldata.rl_rx_dmamap[i],
		    0, sc->rl_ldata.rl_rx_dmamap[i]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat,
		    sc->rl_ldata.rl_rx_dmamap[i]);

		if (!(rxstat & RL_RDESC_STAT_EOF)) {
			m->m_len = MCLBYTES - ETHER_ALIGN;
			if (sc->rl_head == NULL)
				sc->rl_head = sc->rl_tail = m;
			else {
				m->m_flags &= ~M_PKTHDR;
				sc->rl_tail->m_next = m;
				sc->rl_tail = m;
			}
			re_newbuf(sc, i, NULL);
			RL_DESC_INC(i);
			continue;
		}

		/*
		 * NOTE: for the 8139C+, the frame length field
		 * is always 12 bits in size, but for the gigE chips,
		 * it is 13 bits (since the max RX frame length is 16K).
		 * Unfortunately, all 32 bits in the status word
		 * were already used, so to make room for the extra
		 * length bit, RealTek took out the 'frame alignment
		 * error' bit and shifted the other status bits
		 * over one slot. The OWN, EOR, FS and LS bits are
		 * still in the same places. We have already extracted
		 * the frame length and checked the OWN bit, so rather
		 * than using an alternate bit mapping, we shift the
		 * status bits one space to the right so we can evaluate
		 * them using the 8169 status as though it was in the
		 * same format as that of the 8139C+.
		 */
		if (sc->rl_type == RL_8169)
			rxstat >>= 1;

		if (rxstat & RL_RDESC_STAT_RXERRSUM) {
			ifp->if_ierrors++;
			/*
			 * If this is part of a multi-fragment packet,
			 * discard all the pieces.
			 */
			if (sc->rl_head != NULL) {
				m_freem(sc->rl_head);
				sc->rl_head = sc->rl_tail = NULL;
			}
			re_newbuf(sc, i, m);
			RL_DESC_INC(i);
			continue;
		}

		/*
		 * If allocating a replacement mbuf fails,
		 * reload the current one.
		 */

		if (re_newbuf(sc, i, NULL)) {
			ifp->if_ierrors++;
			if (sc->rl_head != NULL) {
				m_freem(sc->rl_head);
				sc->rl_head = sc->rl_tail = NULL;
			}
			re_newbuf(sc, i, m);
			RL_DESC_INC(i);
			continue;
		}

		RL_DESC_INC(i);

		if (sc->rl_head != NULL) {
			m->m_len = total_len % (MCLBYTES - ETHER_ALIGN);
			/* 
			 * Special case: if there's 4 bytes or less
			 * in this buffer, the mbuf can be discarded:
			 * the last 4 bytes is the CRC, which we don't
			 * care about anyway.
			 */
			if (m->m_len <= ETHER_CRC_LEN) {
				sc->rl_tail->m_len -=
				    (ETHER_CRC_LEN - m->m_len);
				m_freem(m);
			} else {
				m->m_len -= ETHER_CRC_LEN;
				m->m_flags &= ~M_PKTHDR;
				sc->rl_tail->m_next = m;
			}
			m = sc->rl_head;
			sc->rl_head = sc->rl_tail = NULL;
			m->m_pkthdr.len = total_len - ETHER_CRC_LEN;
		} else
			m->m_pkthdr.len = m->m_len =
			    (total_len - ETHER_CRC_LEN);

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;

		/* Do RX checksumming if enabled */

#ifdef RE_CSUM_OFFLOAD
		if (ifp->if_capenable & IFCAP_CSUM_IPv4) {

			/* Check IP header checksum */
			if (rxstat & RL_RDESC_STAT_PROTOID)
				m->m_pkthdr.csum_flags |= M_CSUM_IPv4;;
			if (rxstat & RL_RDESC_STAT_IPSUMBAD)
                                m->m_pkthdr.csum_flags |= M_CSUM_IPv4_BAD;
		}

		/* Check TCP/UDP checksum */
		if (RL_TCPPKT(rxstat) &&
		    (ifp->if_capenable & IFCAP_CSUM_TCPv4)) {
			m->m_pkthdr.csum_flags |= M_CSUM_TCPv4;
			if (rxstat & RL_RDESC_STAT_TCPSUMBAD)
				m->m_pkthdr.csum_flags |= M_CSUM_TCP_UDP_BAD;
		}
		if (RL_UDPPKT(rxstat) &&
		    (ifp->if_capenable & IFCAP_CSUM_UDPv4)) {
			m->m_pkthdr.csum_flags |= M_CSUM_UDPv4;
			if (rxstat & RL_RDESC_STAT_UDPSUMBAD)
				m->m_pkthdr.csum_flags |= M_CSUM_TCP_UDP_BAD;
		}
#endif

#ifdef VLANXXX
		if (rxvlan & RL_RDESC_VLANCTL_TAG) {
			mtag = m_tag_get(PACKET_TAG_VLAN, sizeof(u_int),
			    M_NOWAIT);
			if (mtag == NULL) {
				ifp->if_ierrors++;
				m_freem(m);
				continue;
			}
			*(u_int *)(mtag + 1) = 
			    be16toh(rxvlan & RL_RDESC_VLANCTL_DATA);
			m_tag_prepend(m, mtag);
		}
#endif
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
		ether_input_mbuf(ifp, m);
	}

	/* Flush the RX DMA ring */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rl_ldata.rl_rx_list_map,
	    0, sc->rl_ldata.rl_rx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_rx_prodidx = i;

	return;
}

void
re_txeof(sc)
	struct rl_softc	*sc;
{
	struct ifnet		*ifp;
	u_int32_t		txstat;
	int			idx;

	ifp = &sc->sc_arpcom.ac_if;
	idx = sc->rl_ldata.rl_tx_considx;

	/* Invalidate the TX descriptor list */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rl_ldata.rl_tx_list_map,
	    0, sc->rl_ldata.rl_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);

	while (idx != sc->rl_ldata.rl_tx_prodidx) {

		txstat = letoh32(sc->rl_ldata.rl_tx_list[idx].rl_cmdstat);
		if (txstat & RL_TDESC_CMD_OWN)
			break;

		/*
		 * We only stash mbufs in the last descriptor
		 * in a fragment chain, which also happens to
		 * be the only place where the TX status bits
		 * are valid.
		 */

		if (txstat & RL_TDESC_CMD_EOF) {
			m_freem(sc->rl_ldata.rl_tx_mbuf[idx]);
			sc->rl_ldata.rl_tx_mbuf[idx] = NULL;
			bus_dmamap_unload(sc->sc_dmat,
			    sc->rl_ldata.rl_tx_dmamap[idx]);
			if (txstat & (RL_TDESC_STAT_EXCESSCOL|
			    RL_TDESC_STAT_COLCNT))
				ifp->if_collisions++;
			if (txstat & RL_TDESC_STAT_TXERRSUM)
				ifp->if_oerrors++;
			else
				ifp->if_opackets++;
		}
		sc->rl_ldata.rl_tx_free++;
		RL_DESC_INC(idx);
	}

	/* No changes made to the TX ring, so no flush needed */

	if (idx != sc->rl_ldata.rl_tx_considx) {
		sc->rl_ldata.rl_tx_considx = idx;
		ifp->if_flags &= ~IFF_OACTIVE;
		ifp->if_timer = 0;
	}

	/*
	 * If not all descriptors have been released reaped yet,
	 * reload the timer so that we will eventually get another
	 * interrupt that will cause us to re-enter this routine.
	 * This is done in case the transmitter has gone idle.
	 */
	if (sc->rl_ldata.rl_tx_free != RL_TX_DESC_CNT)
                CSR_WRITE_4(sc, RL_TIMERCNT, 1);

	return;
}

void
re_tick(xsc)
	void			*xsc;
{
	struct rl_softc	*sc = xsc;
	int s = splnet();

	mii_tick(&sc->sc_mii);
	splx(s);

	timeout_add(&sc->timer_handle, hz);
}

#ifdef DEVICE_POLLING
void
re_poll (struct ifnet *ifp, enum poll_cmd cmd, int count)
{
	struct rl_softc *sc = ifp->if_softc;

	RL_LOCK(sc);
	if (!(ifp->if_capenable & IFCAP_POLLING)) {
		ether_poll_deregister(ifp);
		cmd = POLL_DEREGISTER;
	}
	if (cmd == POLL_DEREGISTER) { /* final call, enable interrupts */
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);
		goto done;
	}

	sc->rxcycles = count;
	re_rxeof(sc);
	re_txeof(sc);

	if (ifp->if_snd.ifq_head != NULL)
		(*ifp->if_start)(ifp);

	if (cmd == POLL_AND_CHECK_STATUS) { /* also check status register */
		u_int16_t       status;

		status = CSR_READ_2(sc, RL_ISR);
		if (status == 0xffff)
			goto done;
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		/*
		 * XXX check behaviour on receiver stalls.
		 */

		if (status & RL_ISR_SYSTEM_ERR) {
			re_reset(sc);
			re_init(sc);
		}
	}
done:
	RL_UNLOCK(sc);
}
#endif /* DEVICE_POLLING */

int
re_intr(arg)
	void			*arg;
{
	struct rl_softc	*sc = arg;
	struct ifnet		*ifp;
	u_int16_t		status;
	int			claimed = 0;

	ifp = &sc->sc_arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP))
		return 0;

#ifdef DEVICE_POLLING
	if  (ifp->if_flags & IFF_POLLING)
		goto done;
	if ((ifp->if_capenable & IFCAP_POLLING) &&
	    ether_poll_register(re_poll, ifp)) { /* ok, disable interrupts */
		CSR_WRITE_2(sc, RL_IMR, 0x0000);
		re_poll(ifp, 0, 1);
		goto done;
	}
#endif /* DEVICE_POLLING */

	for (;;) {

		status = CSR_READ_2(sc, RL_ISR);
		/* If the card has gone away the read returns 0xffff. */
		if (status == 0xffff)
			break;
		if (status) {
			claimed = 1;
			CSR_WRITE_2(sc, RL_ISR, status);
		}

		if ((status & RL_INTRS_CPLUS) == 0)
			break;

		if (status & RL_ISR_RX_OK)
			re_rxeof(sc);

		if (status & RL_ISR_RX_ERR)
			re_rxeof(sc);

		if ((status & RL_ISR_TIMEOUT_EXPIRED) ||
		    (status & RL_ISR_TX_ERR) ||
		    (status & RL_ISR_TX_DESC_UNAVAIL))
			re_txeof(sc);

		if (status & RL_ISR_SYSTEM_ERR) {
			re_reset(sc);
			re_init(ifp);
		}

		if (status & RL_ISR_LINKCHG) {
			timeout_del(&sc->timer_handle);
			re_tick(sc);
		}
	}

	if (ifp->if_snd.ifq_head != NULL)
		(*ifp->if_start)(ifp);

#ifdef DEVICE_POLLING
done:
#endif

	return claimed;
}

int
re_encap(sc, m_head, idx)
	struct rl_softc	*sc;
	struct mbuf		*m_head;
	int			*idx;
{
	bus_dmamap_t		map;
	int			error, i, curidx;
#ifdef VLANXXX
	struct m_tag		*mtag;
#endif
	struct rl_desc		*d;
	u_int32_t		cmdstat, rl_flags;

	if (sc->rl_ldata.rl_tx_free <= 4)
		return(EFBIG);

	/*
	 * Set up checksum offload. Note: checksum offload bits must
	 * appear in all descriptors of a multi-descriptor transmit
	 * attempt. (This is according to testing done with an 8169
	 * chip. I'm not sure if this is a requirement or a bug.)
	 */

	rl_flags = 0;

#ifdef RE_CSUM_OFFLOAD
	if (m_head->m_pkthdr.csum_flags & M_CSUM_IPv4)
		rl_flags |= RL_TDESC_CMD_IPCSUM;
	if (m_head->m_pkthdr.csum_flags & M_CSUM_TCPv4)
		rl_flags |= RL_TDESC_CMD_TCPCSUM;
	if (m_head->m_pkthdr.csum_flags & M_CSUM_UDPv4)
		rl_flags |= RL_TDESC_CMD_UDPCSUM;
#endif

	map = sc->rl_ldata.rl_tx_dmamap[*idx];
	error = bus_dmamap_load_mbuf(sc->sc_dmat, map,
	    m_head, BUS_DMA_NOWAIT);

	if (error) {
		printf("%s: can't map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		return ENOBUFS;
	}

	if (map->dm_nsegs > sc->rl_ldata.rl_tx_free - 4)
		return ENOBUFS;
	/*
	 * Map the segment array into descriptors. Note that we set the
	 * start-of-frame and end-of-frame markers for either TX or RX, but
	 * they really only have meaning in the TX case. (In the RX case,
	 * it's the chip that tells us where packets begin and end.)
	 * We also keep track of the end of the ring and set the
	 * end-of-ring bits as needed, and we set the ownership bits
	 * in all except the very first descriptor. (The caller will
	 * set this descriptor later when it start transmission or
	 * reception.)
	 */
	i = 0;
	curidx = *idx;
	while (1) {
		d = &sc->rl_ldata.rl_tx_list[curidx];
		if (letoh32(d->rl_cmdstat) & RL_RDESC_STAT_OWN)
			return ENOBUFS;

		cmdstat = map->dm_segs[i].ds_len;
		d->rl_bufaddr_lo =
		    htole32(RL_ADDR_LO(map->dm_segs[i].ds_addr));
		d->rl_bufaddr_hi =
		    htole32(RL_ADDR_HI(map->dm_segs[i].ds_addr));
		if (i == 0)
			cmdstat |= RL_TDESC_CMD_SOF;
		else
			cmdstat |= RL_TDESC_CMD_OWN;
		if (curidx == (RL_RX_DESC_CNT - 1))
			cmdstat |= RL_TDESC_CMD_EOR;
		d->rl_cmdstat = htole32(cmdstat | rl_flags);
		i++;
		if (i == map->dm_nsegs)
			break;
		RL_DESC_INC(curidx);
	}

	d->rl_cmdstat |= htole32(RL_TDESC_CMD_EOF);

	/*
	 * Insure that the map for this transmission
	 * is placed at the array index of the last descriptor
	 * in this chain.
	 */
	sc->rl_ldata.rl_tx_dmamap[*idx] =
	    sc->rl_ldata.rl_tx_dmamap[curidx];
	sc->rl_ldata.rl_tx_dmamap[curidx] = map;
	sc->rl_ldata.rl_tx_mbuf[curidx] = m_head;
	sc->rl_ldata.rl_tx_free -= map->dm_nsegs;

	/*
	 * Set up hardware VLAN tagging. Note: vlan tag info must
	 * appear in the first descriptor of a multi-descriptor
	 * transmission attempt.
	 */

#ifdef VLANXXX
	if (sc->ethercom.ec_nvlans &&
	    (mtag = m_tag_find(m_head, PACKET_TAG_VLAN, NULL)) != NULL)
		sc->rl_ldata.rl_tx_list[*idx].rl_vlanctl =
		    htole32(htons(*(u_int *)(mtag + 1)) |
		    RL_TDESC_VLANCTL_TAG);
#endif

	/* Transfer ownership of packet to the chip. */

	sc->rl_ldata.rl_tx_list[curidx].rl_cmdstat |=
	    htole32(RL_TDESC_CMD_OWN);
	if (*idx != curidx)
		sc->rl_ldata.rl_tx_list[*idx].rl_cmdstat |=
		    htole32(RL_TDESC_CMD_OWN);

	RL_DESC_INC(curidx);
	*idx = curidx;

	return 0;
}

/*
 * Main transmit routine for C+ and gigE NICs.
 */

void
re_start(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc	*sc;
	struct mbuf		*m_head = NULL;
	int			idx;

	sc = ifp->if_softc;

	idx = sc->rl_ldata.rl_tx_prodidx;
	while (sc->rl_ldata.rl_tx_mbuf[idx] == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (re_encap(sc, m_head, &idx)) {
			IF_PREPEND(&ifp->if_snd, m_head);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif
	}

	/* Flush the TX descriptors */

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rl_ldata.rl_tx_list_map,
	    0, sc->rl_ldata.rl_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);

	sc->rl_ldata.rl_tx_prodidx = idx;

	/*
	 * RealTek put the TX poll request register in a different
	 * location on the 8169 gigE chip. I don't know why.
	 */

	if (sc->rl_type == RL_8169)
		CSR_WRITE_2(sc, RL_GTXSTART, RL_TXSTART_START);
	else
		CSR_WRITE_2(sc, RL_TXSTART, RL_TXSTART_START);

	/*
	 * Use the countdown timer for interrupt moderation.
	 * 'TX done' interrupts are disabled. Instead, we reset the
	 * countdown timer, which will begin counting until it hits
	 * the value in the TIMERINT register, and then trigger an
	 * interrupt. Each time we write to the TIMERCNT register,
	 * the timer count is reset to 0.
	 */
	CSR_WRITE_4(sc, RL_TIMERCNT, 1);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

int
re_init(struct ifnet *ifp)
{
	struct rl_softc	*sc = ifp->if_softc;
	u_int32_t		rxcfg = 0;
	u_int32_t		reg;
	int s;

	s = splimp(); /* XXX NOT NEEDED MAYBE, from rl(4) */

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	re_stop(sc);

	/*
	 * Enable C+ RX and TX mode, as well as VLAN stripping and
	 * RX checksum offload. We must configure the C+ register
	 * before all others.
	 */
#ifdef RE_CSUM_OFFLOAD
	CSR_WRITE_2(sc, RL_CPLUS_CMD, RL_CPLUSCMD_RXENB|
	    RL_CPLUSCMD_TXENB|RL_CPLUSCMD_PCI_MRW|
	    RL_CPLUSCMD_VLANSTRIP|
	    (ifp->if_capenable &
	    (IFCAP_CSUM_IPv4 |IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4) ?
	    RL_CPLUSCMD_RXCSUM_ENB : 0));
#else
	CSR_WRITE_2(sc, RL_CPLUS_CMD, RL_CPLUSCMD_RXENB|
	    RL_CPLUSCMD_TXENB|RL_CPLUSCMD_PCI_MRW|
	    RL_CPLUSCMD_VLANSTRIP);
#endif

	/*
	 * Init our MAC address.  Even though the chipset
	 * documentation doesn't mention it, we need to enter "Config
	 * register write enable" mode to modify the ID registers.
	 */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_WRITECFG);
	memcpy(&reg, LLADDR(ifp->if_sadl), 4);
	CSR_WRITE_4(sc, RL_IDR0, reg);
	reg = 0;
	memcpy(&reg, LLADDR(ifp->if_sadl) + 4, 4);
	CSR_WRITE_4(sc, RL_IDR4, reg);
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	/*
	 * For C+ mode, initialize the RX descriptors and mbufs.
	 */
	re_rx_list_init(sc);
	re_tx_list_init(sc);

	/*
	 * Enable transmit and receive.
	 */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	/*
	 * Set the initial TX and RX configuration.
	 */
	if (sc->rl_testmode) {
		if (sc->rl_type == RL_8169)
			CSR_WRITE_4(sc, RL_TXCFG,
			    RL_TXCFG_CONFIG|RL_LOOPTEST_ON);
		else
			CSR_WRITE_4(sc, RL_TXCFG,
			    RL_TXCFG_CONFIG|RL_LOOPTEST_ON_CPLUS);
	} else
		CSR_WRITE_4(sc, RL_TXCFG, RL_TXCFG_CONFIG);
	CSR_WRITE_4(sc, RL_RXCFG, RL_RXCFG_CONFIG);

	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RL_RXCFG);
	rxcfg |= RL_RXCFG_RX_INDIV;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		rxcfg |= RL_RXCFG_RX_ALLPHYS;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	} else {
		rxcfg &= ~RL_RXCFG_RX_ALLPHYS;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		rxcfg |= RL_RXCFG_RX_BROAD;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	} else {
		rxcfg &= ~RL_RXCFG_RX_BROAD;
		CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
	}

	/*
	 * Program the multicast filter, if necessary.
	 */
	rl_setmulti(sc);

#ifdef DEVICE_POLLING
	/*
	 * Disable interrupts if we are polling.
	 */
	if (ifp->if_flags & IFF_POLLING)
		CSR_WRITE_2(sc, RL_IMR, 0);
	else	/* otherwise ... */
#endif /* DEVICE_POLLING */
	/*
	 * Enable interrupts.
	 */
	if (sc->rl_testmode)
		CSR_WRITE_2(sc, RL_IMR, 0);
	else
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);
#ifdef notdef
	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);
#endif
	/*
	 * Load the addresses of the RX and TX lists into the chip.
	 */

	CSR_WRITE_4(sc, RL_RXLIST_ADDR_HI,
	    RL_ADDR_HI(sc->rl_ldata.rl_rx_listseg.ds_addr));
	CSR_WRITE_4(sc, RL_RXLIST_ADDR_LO,
	    RL_ADDR_LO(sc->rl_ldata.rl_rx_listseg.ds_addr));

	CSR_WRITE_4(sc, RL_TXLIST_ADDR_HI,
	    RL_ADDR_HI(sc->rl_ldata.rl_tx_listseg.ds_addr));
	CSR_WRITE_4(sc, RL_TXLIST_ADDR_LO,
	    RL_ADDR_LO(sc->rl_ldata.rl_tx_listseg.ds_addr));

	CSR_WRITE_1(sc, RL_EARLY_TX_THRESH, 16);

	/*
	 * Initialize the timer interrupt register so that
	 * a timer interrupt will be generated once the timer
	 * reaches a certain number of ticks. The timer is
	 * reloaded on each transmit. This gives us TX interrupt
	 * moderation, which dramatically improves TX frame rate.
	 */

	if (sc->rl_type == RL_8169)
		CSR_WRITE_4(sc, RL_TIMERINT_8169, 0x800);
	else
		CSR_WRITE_4(sc, RL_TIMERINT, 0x400);

	/*
	 * For 8169 gigE NICs, set the max allowed RX packet
	 * size so we can receive jumbo frames.
	 */
	if (sc->rl_type == RL_8169)
		CSR_WRITE_2(sc, RL_MAXRXPKTLEN, 16383);

	if (sc->rl_testmode)
		return 0;

	mii_mediachg(&sc->sc_mii);

	CSR_WRITE_1(sc, RL_CFG1, RL_CFG1_DRVLOAD|RL_CFG1_FULLDUPLEX);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	timeout_add(&sc->timer_handle, hz);

	return 0;
}

/*
 * Set media options.
 */
int
re_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc	*sc;

	sc = ifp->if_softc;

	return (mii_mediachg(&sc->sc_mii));
}

/*
 * Report current media status.
 */
void
re_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct rl_softc	*sc;

	sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

int
re_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct rl_softc	*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int			s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command,
	    data)) > 0) {
		splx(s);
		return (error);
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			re_init(ifp);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif /* INET */
		default:
			re_init(ifp);
			break;
		}
		break;
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > RL_JUMBO_MTU)
			error = EINVAL;
		ifp->if_mtu = ifr->ifr_mtu;
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			re_init(ifp);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				re_stop(sc);
		}
		error = 0;
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		error = ifmedia_ioctl(ifp, ifr, &sc->sc_mii.mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return(error);
}

void
re_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc	*sc;
	int			s;

	sc = ifp->if_softc;
	s = splnet();
	printf("%s: watchdog timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;

	re_txeof(sc);
	re_rxeof(sc);

	re_init(ifp);

	splx(s);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
re_stop(sc)
	struct rl_softc	*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_timer = 0;

	timeout_del(&sc->timer_handle);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
#ifdef DEVICE_POLLING
	ether_poll_deregister(ifp);
#endif /* DEVICE_POLLING */

	CSR_WRITE_1(sc, RL_COMMAND, 0x00);
	CSR_WRITE_2(sc, RL_IMR, 0x0000);

	if (sc->rl_head != NULL) {
		m_freem(sc->rl_head);
		sc->rl_head = sc->rl_tail = NULL;
	}

	/* Free the TX list buffers. */

	for (i = 0; i < RL_TX_DESC_CNT; i++) {
		if (sc->rl_ldata.rl_tx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->rl_ldata.rl_tx_dmamap[i]);
			m_freem(sc->rl_ldata.rl_tx_mbuf[i]);
			sc->rl_ldata.rl_tx_mbuf[i] = NULL;
		}
	}

	/* Free the RX list buffers. */

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		if (sc->rl_ldata.rl_rx_mbuf[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->rl_ldata.rl_rx_dmamap[i]);
			m_freem(sc->rl_ldata.rl_rx_mbuf[i]);
			sc->rl_ldata.rl_rx_mbuf[i] = NULL;
		}
	}

	return;
}
