/*	$OpenBSD: re.c,v 1.71 2007/05/08 21:19:42 deraadt Exp $	*/
/*	$FreeBSD: if_re.c,v 1.31 2004/09/04 07:54:05 ru Exp $	*/
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
 * seven devices in this family: the RTL8139C+, the RTL8169, the RTL8169S,
 * RTL8110S, the RTL8168, the RTL8111 and the RTL8101E.
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
 * jumbo frames larger than 7440, so the max MTU possible with this
 * driver is 7422 bytes.
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
#include <sys/timeout.h>
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

#include <dev/ic/rtl81x9reg.h>
#include <dev/ic/revar.h>

#ifdef RE_DEBUG
int redebug = 0;
#define DPRINTF(x)	do { if (redebug) printf x; } while (0)
#else
#define DPRINTF(x)
#endif

static inline void re_set_bufaddr(struct rl_desc *, bus_addr_t);

int	re_encap(struct rl_softc *, struct mbuf *, int *);

int	re_newbuf(struct rl_softc *, int, struct mbuf *);
int	re_rx_list_init(struct rl_softc *);
int	re_tx_list_init(struct rl_softc *);
void	re_rxeof(struct rl_softc *);
void	re_txeof(struct rl_softc *);
void	re_tick(void *);
void	re_start(struct ifnet *);
int	re_ioctl(struct ifnet *, u_long, caddr_t);
void	re_watchdog(struct ifnet *);
int	re_ifmedia_upd(struct ifnet *);
void	re_ifmedia_sts(struct ifnet *, struct ifmediareq *);

void	re_eeprom_putbyte(struct rl_softc *, int);
void	re_eeprom_getword(struct rl_softc *, int, u_int16_t *);
void	re_read_eeprom(struct rl_softc *, caddr_t, int, int);

int	re_gmii_readreg(struct device *, int, int);
void	re_gmii_writereg(struct device *, int, int, int);

int	re_miibus_readreg(struct device *, int, int);
void	re_miibus_writereg(struct device *, int, int, int);
void	re_miibus_statchg(struct device *);

void	re_setmulti(struct rl_softc *);
void	re_setpromisc(struct rl_softc *);
void	re_reset(struct rl_softc *);

#ifdef RE_DIAG
int	re_diag(struct rl_softc *);
#endif

struct cfdriver re_cd = {
	0, "re", DV_IFNET
};

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) & ~x)

static const struct re_revision {
	u_int32_t		re_chipid;
	const char		*re_name;
} re_revisions[] = {
	{ RL_HWREV_8169,	"RTL8169" },
	{ RL_HWREV_8110S,	"RTL8110S" },
	{ RL_HWREV_8169S,	"RTL8169S" },
	{ RL_HWREV_8169_8110SB,	"RTL8169/8110SB" },
	{ RL_HWREV_8169_8110SC,	"RTL8169/8110SC" },
	{ RL_HWREV_8168_SPIN1,	"RTL8168 1" },
	{ RL_HWREV_8100E_SPIN1,	"RTL8100E 1" },
	{ RL_HWREV_8101E,	"RTL8101E" },
	{ RL_HWREV_8168_SPIN2,	"RTL8168 2" },
	{ RL_HWREV_8100E_SPIN2, "RTL8100E 2" },
	{ RL_HWREV_8139CPLUS,	"RTL8139C+" },
	{ RL_HWREV_8101,	"RTL8101" },
	{ RL_HWREV_8100,	"RTL8100" },

	{ 0, NULL }
};


static inline void
re_set_bufaddr(struct rl_desc *d, bus_addr_t addr)
{
	d->rl_bufaddr_lo = htole32((uint32_t)addr);
	if (sizeof(bus_addr_t) == sizeof(uint64_t))
		d->rl_bufaddr_hi = htole32((uint64_t)addr >> 32);
	else
		d->rl_bufaddr_hi = 0;
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
void
re_eeprom_putbyte(struct rl_softc *sc, int addr)
{
	int	d, i;

	d = addr | (RL_9346_READ << sc->rl_eewidth);

	/*
	 * Feed in each bit and strobe the clock.
	 */

	for (i = 1 << (sc->rl_eewidth + 3); i; i >>= 1) {
		if (d & i)
			EE_SET(RL_EE_DATAIN);
		else
			EE_CLR(RL_EE_DATAIN);
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
re_eeprom_getword(struct rl_softc *sc, int addr, u_int16_t *dest)
{
	int		i;
	u_int16_t	word = 0;

	/*
	 * Send address of word we want to read.
	 */
	re_eeprom_putbyte(sc, addr);

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

	*dest = word;
}

/*
 * Read a sequence of words from the EEPROM.
 */
void
re_read_eeprom(struct rl_softc *sc, caddr_t dest, int off, int cnt)
{
	int		i;
	u_int16_t	word = 0, *ptr;

	CSR_SETBIT_1(sc, RL_EECMD, RL_EEMODE_PROGRAM);

	DELAY(100);

	for (i = 0; i < cnt; i++) {
		CSR_SETBIT_1(sc, RL_EECMD, RL_EE_SEL);
		re_eeprom_getword(sc, off + i, &word);
		CSR_CLRBIT_1(sc, RL_EECMD, RL_EE_SEL);
		ptr = (u_int16_t *)(dest + (i * 2));
		*ptr = word;
	}

	CSR_CLRBIT_1(sc, RL_EECMD, RL_EEMODE_PROGRAM);
}

int
re_gmii_readreg(struct device *self, int phy, int reg)
{
	struct rl_softc	*sc = (struct rl_softc *)self;
	u_int32_t	rval;
	int		i;

	if (phy != 7)
		return (0);

	/* Let the rgephy driver read the GMEDIASTAT register */

	if (reg == RL_GMEDIASTAT) {
		rval = CSR_READ_1(sc, RL_GMEDIASTAT);
		return (rval);
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
	u_int32_t	rval;
	int		i;

	CSR_WRITE_4(sc, RL_PHYAR, (reg << 16) |
	    (data & RL_PHYAR_PHYDATA) | RL_PHYAR_BUSY);
	DELAY(1000);

	for (i = 0; i < RL_TIMEOUT; i++) {
		rval = CSR_READ_4(sc, RL_PHYAR);
		if (!(rval & RL_PHYAR_BUSY))
			break;
		DELAY(100);
	}

	if (i == RL_TIMEOUT)
		printf ("%s: PHY write failed\n", sc->sc_dev.dv_xname);
}

int
re_miibus_readreg(struct device *dev, int phy, int reg)
{
	struct rl_softc	*sc = (struct rl_softc *)dev;
	u_int16_t	rval = 0;
	u_int16_t	re8139_reg = 0;
	int		s;

	s = splnet();

	if (sc->rl_type == RL_8169) {
		rval = re_gmii_readreg(dev, phy, reg);
		splx(s);
		return (rval);
	}

	/* Pretend the internal PHY is only at address 0 */
	if (phy) {
		splx(s);
		return (0);
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
		return (0);
	/*
	 * Allow the rlphy driver to read the media status
	 * register. If we have a link partner which does not
	 * support NWAY, this is the register which will tell
	 * us the results of parallel detection.
	 */
	case RL_MEDIASTAT:
		rval = CSR_READ_1(sc, RL_MEDIASTAT);
		splx(s);
		return (rval);
	default:
		printf("%s: bad phy register %x\n", sc->sc_dev.dv_xname, reg);
		splx(s);
		return (0);
	}
	rval = CSR_READ_2(sc, re8139_reg);
	if (sc->rl_type == RL_8139CPLUS && re8139_reg == RL_BMCR) {
		/* 8139C+ has different bit layout. */
		rval &= ~(BMCR_LOOP | BMCR_ISO);
	}
	splx(s);
	return (rval);
}

void
re_miibus_writereg(struct device *dev, int phy, int reg, int data)
{
	struct rl_softc	*sc = (struct rl_softc *)dev;
	u_int16_t	re8139_reg = 0;
	int		s;

	s = splnet();

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
		if (sc->rl_type == RL_8139CPLUS) {
			/* 8139C+ has different bit layout. */
			data &= ~(BMCR_LOOP | BMCR_ISO);
		}
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
		printf("%s: bad phy register %x\n", sc->sc_dev.dv_xname, reg);
		splx(s);
		return;
	}
	CSR_WRITE_2(sc, re8139_reg, data);
	splx(s);
}

void
re_miibus_statchg(struct device *dev)
{
}

/*
 * Program the 64-bit multicast hash filter.
 */
void
re_setmulti(struct rl_softc *sc)
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	u_int32_t		hwrev, rxfilt;
	int			mcnt = 0;
	struct arpcom		*ac = &sc->sc_arpcom;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	
	ifp = &sc->sc_arpcom.ac_if;

	rxfilt = CSR_READ_4(sc, RL_RXCFG);

	if (ifp->if_flags & IFF_ALLMULTI || ifp->if_flags & IFF_PROMISC) {
		rxfilt |= RL_RXCFG_RX_MULTI;
		CSR_WRITE_4(sc, RL_RXCFG, rxfilt);
		CSR_WRITE_4(sc, RL_MAR0, 0xFFFFFFFF);
		CSR_WRITE_4(sc, RL_MAR4, 0xFFFFFFFF);
		return;
	}

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, RL_MAR0, 0);
	CSR_WRITE_4(sc, RL_MAR4, 0);

	/* now program new ones */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			mcnt = MAX_NUM_MULTICAST_ADDRESSES;
		}
		if (mcnt == MAX_NUM_MULTICAST_ADDRESSES)
			break;

		h = (ether_crc32_be(enm->enm_addrlo,
		    ETHER_ADDR_LEN) >> 26) & 0x0000003F;
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		mcnt++;
		ETHER_NEXT_MULTI(step, enm);
	}

	if (mcnt)
		rxfilt |= RL_RXCFG_RX_MULTI;
	else
		rxfilt &= ~RL_RXCFG_RX_MULTI;

	CSR_WRITE_4(sc, RL_RXCFG, rxfilt);

	/*
	 * For some unfathomable reason, RealTek decided to reverse
	 * the order of the multicast hash registers in the PCI Express
	 * parts. This means we have to write the hash pattern in reverse
	 * order for those devices.
	 */
	hwrev = CSR_READ_4(sc, RL_TXCFG) & RL_TXCFG_HWREV;
	if (hwrev == RL_HWREV_8100E_SPIN1 || hwrev == RL_HWREV_8100E_SPIN2 ||
	    hwrev == RL_HWREV_8101E || hwrev == RL_HWREV_8168_SPIN1 ||
	    hwrev == RL_HWREV_8168_SPIN2) {
		CSR_WRITE_4(sc, RL_MAR0, swap32(hashes[1]));
		CSR_WRITE_4(sc, RL_MAR4, swap32(hashes[0]));
	} else {
		CSR_WRITE_4(sc, RL_MAR0, hashes[0]);
		CSR_WRITE_4(sc, RL_MAR4, hashes[1]);
	}
}

void
re_setpromisc(struct rl_softc *sc)
{
	struct ifnet	*ifp;
	u_int32_t	rxcfg = 0;

	ifp = &sc->sc_arpcom.ac_if;

	rxcfg = CSR_READ_4(sc, RL_RXCFG);
	if (ifp->if_flags & IFF_PROMISC) 
		rxcfg |= RL_RXCFG_RX_ALLPHYS;
        else
		rxcfg &= ~RL_RXCFG_RX_ALLPHYS;
	CSR_WRITE_4(sc, RL_RXCFG, rxcfg);
}

void
re_reset(struct rl_softc *sc)
{
	int	i;

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

#ifdef RE_DIAG

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
re_diag(struct rl_softc *sc)
{
	struct ifnet		*ifp = &sc->sc_arpcom.ac_if;
	struct mbuf		*m0;
	struct ether_header	*eh;
	struct rl_rxsoft	*rxs;
	struct rl_desc		*cur_rx;
	bus_dmamap_t		dmamap;
	u_int16_t		status;
	u_int32_t		rxstat;
	int			total_len, i, s, error = 0, phyaddr;
	u_int8_t		dst[] = { 0x00, 'h', 'e', 'l', 'l', 'o' };
	u_int8_t		src[] = { 0x00, 'w', 'o', 'r', 'l', 'd' };

	DPRINTF(("inside re_diag\n"));
	/* Allocate a single mbuf */

	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 == NULL)
		return (ENOBUFS);

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
	re_reset(sc);
	re_init(ifp);
	sc->rl_link = 1;
	if (sc->rl_type == RL_8169)
		phyaddr = 1;
	else
		phyaddr = 0;

	re_miibus_writereg((struct device *)sc, phyaddr, MII_BMCR,
	    BMCR_RESET);
	for (i = 0; i < RL_TIMEOUT; i++) {
		status = re_miibus_readreg((struct device *)sc,
		    phyaddr, MII_BMCR);
		if (!(status & BMCR_RESET))
			break;
	}

	re_miibus_writereg((struct device *)sc, phyaddr, MII_BMCR,
	    BMCR_LOOP);
	CSR_WRITE_2(sc, RL_ISR, RL_INTRS);

	DELAY(100000);

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
	IFQ_ENQUEUE(&ifp->if_snd, m0, NULL, error);
	re_start(ifp);
	splx(s);
	m0 = NULL;

	DPRINTF(("re_diag: transmission started\n"));

	/* Wait for it to propagate through the chip */

	DELAY(100000);
	for (i = 0; i < RL_TIMEOUT; i++) {
		status = CSR_READ_2(sc, RL_ISR);
		CSR_WRITE_2(sc, RL_ISR, status);
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

	rxs = &sc->rl_ldata.rl_rxsoft[0];
	dmamap = rxs->rxs_dmamap;
	bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
	    BUS_DMASYNC_POSTREAD);
	bus_dmamap_unload(sc->sc_dmat, dmamap);

	m0 = rxs->rxs_mbuf;
	rxs->rxs_mbuf = NULL;
	eh = mtod(m0, struct ether_header *);

	RL_RXDESCSYNC(sc, 0, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	cur_rx = &sc->rl_ldata.rl_rx_list[0];
	rxstat = letoh32(cur_rx->rl_cmdstat);
	total_len = rxstat & sc->rl_rxlenmask;

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
	sc->rl_link = 0;
	ifp->if_flags &= ~IFF_PROMISC;
	re_stop(ifp, 1);
	if (m0 != NULL)
		m_freem(m0);
	DPRINTF(("leaving re_diag\n"));

	return (error);
}

#endif

#ifdef __armish__ 
/*
 * Thecus N2100 doesn't store the full mac address in eeprom
 * so we read the old mac address from the device before the reset
 * in hopes that the proper mac address is already there.
 */
union {
	u_int32_t eaddr_word[2];
	u_char eaddr[ETHER_ADDR_LEN];
} boot_eaddr;
int boot_eaddr_valid;
#endif /* __armish__ */
/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
int
re_attach(struct rl_softc *sc, const char *intrstr)
{
	u_char		eaddr[ETHER_ADDR_LEN];
	u_int16_t	as[ETHER_ADDR_LEN / 2];
	struct ifnet	*ifp;
	u_int16_t	re_did = 0;
	int		error = 0, i;
	u_int32_t	hwrev;
	const struct re_revision *rr;
	const char	*re_name = NULL;

	/* Reset the adapter. */
	re_reset(sc);

	sc->rl_eewidth = 6;
	re_read_eeprom(sc, (caddr_t)&re_did, 0, 1);
	if (re_did != 0x8129)
		sc->rl_eewidth = 8;

	/*
	 * Get station address from the EEPROM.
	 */
	re_read_eeprom(sc, (caddr_t)as, RL_EE_EADDR, 3);
	for (i = 0; i < ETHER_ADDR_LEN / 2; i++)
		as[i] = letoh16(as[i]);
	bcopy(as, eaddr, sizeof(eaddr));
#ifdef __armish__
	/*
	 * On the Thecus N2100, the MAC address in the EEPROM is
	 * always 00:14:fd:10:00:00.  The proper MAC address is stored
	 * in flash.  Fortunately RedBoot configures the proper MAC
	 * address (for the first onboard interface) which we can read
	 * from the IDR.
	 */
	if (eaddr[0] == 0x00 && eaddr[1] == 0x14 && eaddr[2] == 0xfd &&
	    eaddr[3] == 0x10 && eaddr[4] == 0x00 && eaddr[5] == 0x00) {
		if (boot_eaddr_valid == 0) {
			boot_eaddr.eaddr_word[1] = letoh32(CSR_READ_4(sc, RL_IDR4));
			boot_eaddr.eaddr_word[0] = letoh32(CSR_READ_4(sc, RL_IDR0));
			boot_eaddr_valid = 1;
		}

		bcopy(boot_eaddr.eaddr, eaddr, sizeof(eaddr));
		eaddr[5] += sc->sc_dev.dv_unit;
	}
#endif

	/*
	 * Set RX length mask, TX poll request register
	 * and TX descriptor count.
	 */
	if (sc->rl_type == RL_8169) {
		sc->rl_rxlenmask = RL_RDESC_STAT_GFRAGLEN;
		sc->rl_txstart = RL_GTXSTART;
		sc->rl_ldata.rl_tx_desc_cnt = RL_TX_DESC_CNT_8169;
	} else {
		sc->rl_rxlenmask = RL_RDESC_STAT_FRAGLEN;
		sc->rl_txstart = RL_TXSTART;
		sc->rl_ldata.rl_tx_desc_cnt = RL_TX_DESC_CNT_8139;
	}

	bcopy(eaddr, (char *)&sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);

	hwrev = CSR_READ_4(sc, RL_TXCFG) & RL_TXCFG_HWREV;
	for (rr = re_revisions; rr->re_name != NULL; rr++) {
		if (rr->re_chipid == hwrev)
			re_name = rr->re_name;
	}

	if (re_name == NULL)
		printf(": unknown ASIC (0x%04x)", hwrev >> 16);
	else
		printf(": %s (0x%04x)", re_name, hwrev >> 16);

	printf(", %s, address %s\n", intrstr,
	    ether_sprintf(sc->sc_arpcom.ac_enaddr));

	if (sc->rl_ldata.rl_tx_desc_cnt >
	    PAGE_SIZE / sizeof(struct rl_desc)) {
		sc->rl_ldata.rl_tx_desc_cnt =
		    PAGE_SIZE / sizeof(struct rl_desc);
	}

	/* Allocate DMA'able memory for the TX ring */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, RL_TX_LIST_SZ(sc),
		    RL_RING_ALIGN, 0, &sc->rl_ldata.rl_tx_listseg, 1,
		    &sc->rl_ldata.rl_tx_listnseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: can't allocate tx listseg, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_0;
	}

	/* Load the map for the TX ring. */
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->rl_ldata.rl_tx_listseg,
		    sc->rl_ldata.rl_tx_listnseg, RL_TX_LIST_SZ(sc),
		    (caddr_t *)&sc->rl_ldata.rl_tx_list,
		    BUS_DMA_COHERENT | BUS_DMA_NOWAIT)) != 0) {
		printf("%s: can't map tx list, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_1;
	}
	memset(sc->rl_ldata.rl_tx_list, 0, RL_TX_LIST_SZ(sc));

	if ((error = bus_dmamap_create(sc->sc_dmat, RL_TX_LIST_SZ(sc), 1,
		    RL_TX_LIST_SZ(sc), 0, 0,
		    &sc->rl_ldata.rl_tx_list_map)) != 0) {
		printf("%s: can't create tx list map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_2;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat,
		    sc->rl_ldata.rl_tx_list_map, sc->rl_ldata.rl_tx_list,
		    RL_TX_LIST_SZ(sc), NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: can't load tx list, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_3;
	}

	/* Create DMA maps for TX buffers */
	for (i = 0; i < RL_TX_QLEN; i++) {
		error = bus_dmamap_create(sc->sc_dmat,
		    RL_JUMBO_FRAMELEN,
		    RL_TX_DESC_CNT(sc) - RL_NTXDESC_RSVD, RL_TDESC_CMD_FRAGLEN,
		    0, 0, &sc->rl_ldata.rl_txq[i].txq_dmamap);
		if (error) {
			printf("%s: can't create DMA map for TX\n",
			    sc->sc_dev.dv_xname);
			goto fail_4;
		}
	}

        /* Allocate DMA'able memory for the RX ring */
	if ((error = bus_dmamem_alloc(sc->sc_dmat, RL_RX_DMAMEM_SZ,
		    RL_RING_ALIGN, 0, &sc->rl_ldata.rl_rx_listseg, 1,
		    &sc->rl_ldata.rl_rx_listnseg, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: can't allocate rx listnseg, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_4;
	}

        /* Load the map for the RX ring. */
	if ((error = bus_dmamem_map(sc->sc_dmat, &sc->rl_ldata.rl_rx_listseg,
		    sc->rl_ldata.rl_rx_listnseg, RL_RX_DMAMEM_SZ,
		    (caddr_t *)&sc->rl_ldata.rl_rx_list,
		    BUS_DMA_COHERENT | BUS_DMA_NOWAIT)) != 0) {
		printf("%s: can't map rx list, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_5;

	}
	memset(sc->rl_ldata.rl_rx_list, 0, RL_RX_DMAMEM_SZ);

	if ((error = bus_dmamap_create(sc->sc_dmat, RL_RX_DMAMEM_SZ, 1,
		    RL_RX_DMAMEM_SZ, 0, 0,
		    &sc->rl_ldata.rl_rx_list_map)) != 0) {
		printf("%s: can't create rx list map, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_6;
	}

	if ((error = bus_dmamap_load(sc->sc_dmat,
		    sc->rl_ldata.rl_rx_list_map, sc->rl_ldata.rl_rx_list,
		    RL_RX_DMAMEM_SZ, NULL, BUS_DMA_NOWAIT)) != 0) {
		printf("%s: can't load rx list, error = %d\n",
		    sc->sc_dev.dv_xname, error);
		goto fail_7;
	}

        /* Create DMA maps for RX buffers */
        for (i = 0; i < RL_RX_DESC_CNT; i++) {
                error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
                    0, 0, &sc->rl_ldata.rl_rxsoft[i].rxs_dmamap);
                if (error) {
                        printf("%s: can't create DMA map for RX\n",
                            sc->sc_dev.dv_xname);
			goto fail_8;
                }
        }

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_softc = sc;
	strlcpy(ifp->if_xname, sc->sc_dev.dv_xname, IFNAMSIZ);
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = re_ioctl;
	ifp->if_start = re_start;
	ifp->if_watchdog = re_watchdog;
	ifp->if_init = re_init;
	if (sc->rl_type == RL_8169)
		ifp->if_hardmtu = RL_JUMBO_MTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, RL_TX_QLEN);
	IFQ_SET_READY(&ifp->if_snd);

	ifp->if_capabilities = IFCAP_VLAN_MTU | IFCAP_CSUM_IPv4 |
			       IFCAP_CSUM_TCPv4 | IFCAP_CSUM_UDPv4;

#if NVLAN > 0
	ifp->if_capabilities |= IFCAP_VLAN_HWTAGGING;
#endif

	timeout_set(&sc->timer_handle, re_tick, sc);

	/* Do MII setup */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = re_miibus_readreg;
	sc->sc_mii.mii_writereg = re_miibus_writereg;
	sc->sc_mii.mii_statchg = re_miibus_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, IFM_IMASK, re_ifmedia_upd,
	    re_ifmedia_sts);
	mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, MIIF_DOPAUSE);
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

#ifdef RE_DIAG
	/*
	 * Perform hardware diagnostic on the original RTL8169.
	 * Some 32-bit cards were incorrectly wired and would
	 * malfunction if plugged into a 64-bit slot.
	 */
	if (sc->rl_type == RL_8169) {
		error = re_diag(sc);
		if (error) {
			printf("%s: attach aborted due to hardware diag failure\n",
			    sc->sc_dev.dv_xname);
			ether_ifdetach(ifp);
			goto fail_8;
		}
	}
#endif

	return (0);

fail_8:
	/* Destroy DMA maps for RX buffers. */
	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		if (sc->rl_ldata.rl_rxsoft[i].rxs_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->rl_ldata.rl_rxsoft[i].rxs_dmamap);
	}

	/* Free DMA'able memory for the RX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->rl_ldata.rl_rx_list_map);
fail_7:
	bus_dmamap_destroy(sc->sc_dmat, sc->rl_ldata.rl_rx_list_map);
fail_6:
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->rl_ldata.rl_rx_list, RL_RX_DMAMEM_SZ);
fail_5:
	bus_dmamem_free(sc->sc_dmat,
	    &sc->rl_ldata.rl_rx_listseg, sc->rl_ldata.rl_rx_listnseg);

fail_4:
	/* Destroy DMA maps for TX buffers. */
	for (i = 0; i < RL_TX_QLEN; i++) {
		if (sc->rl_ldata.rl_txq[i].txq_dmamap != NULL)
			bus_dmamap_destroy(sc->sc_dmat,
			    sc->rl_ldata.rl_txq[i].txq_dmamap);
	}

	/* Free DMA'able memory for the TX ring. */
	bus_dmamap_unload(sc->sc_dmat, sc->rl_ldata.rl_tx_list_map);
fail_3:
	bus_dmamap_destroy(sc->sc_dmat, sc->rl_ldata.rl_tx_list_map);
fail_2:
	bus_dmamem_unmap(sc->sc_dmat,
	    (caddr_t)sc->rl_ldata.rl_tx_list, RL_TX_LIST_SZ(sc));
fail_1:
	bus_dmamem_free(sc->sc_dmat,
	    &sc->rl_ldata.rl_tx_listseg, sc->rl_ldata.rl_tx_listnseg);
fail_0:
 	return (1);
}


int
re_newbuf(struct rl_softc *sc, int idx, struct mbuf *m)
{
	struct mbuf	*n = NULL;
	bus_dmamap_t	map;
	struct rl_desc	*d;
	struct rl_rxsoft *rxs;
	u_int32_t	cmdstat;
	int		error;

	if (m == NULL) {
		MGETHDR(n, M_DONTWAIT, MT_DATA);
		if (n == NULL)
			return (ENOBUFS);

		MCLGET(n, M_DONTWAIT);
		if (!(n->m_flags & M_EXT)) {
			m_freem(n);
			return (ENOBUFS);
		}
		m = n;
	} else
		m->m_data = m->m_ext.ext_buf;

	/*
	 * Initialize mbuf length fields and fixup
	 * alignment so that the frame payload is
	 * longword aligned on strict alignment archs.
	 */
	m->m_len = m->m_pkthdr.len = RE_RX_DESC_BUFLEN;
	m->m_data += RE_ETHER_ALIGN;

	rxs = &sc->rl_ldata.rl_rxsoft[idx];
	map = rxs->rxs_dmamap;
	error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
	    BUS_DMA_READ|BUS_DMA_NOWAIT);

	if (error)
		goto out;

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	d = &sc->rl_ldata.rl_rx_list[idx];
	RL_RXDESCSYNC(sc, idx, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	cmdstat = letoh32(d->rl_cmdstat);
	RL_RXDESCSYNC(sc, idx, BUS_DMASYNC_PREREAD);
	if (cmdstat & RL_RDESC_STAT_OWN) {
		printf("%s: tried to map busy RX descriptor\n",
		    sc->sc_dev.dv_xname);
		goto out;
	}

	rxs->rxs_mbuf = m;

	d->rl_vlanctl = 0;
	cmdstat = map->dm_segs[0].ds_len;
	if (idx == (RL_RX_DESC_CNT - 1))
		cmdstat |= RL_RDESC_CMD_EOR;
	re_set_bufaddr(d, map->dm_segs[0].ds_addr);
	d->rl_cmdstat = htole32(cmdstat);
	RL_RXDESCSYNC(sc, idx, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	cmdstat |= RL_RDESC_CMD_OWN;
	d->rl_cmdstat = htole32(cmdstat);
	RL_RXDESCSYNC(sc, idx, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	return (0);
 out:
	if (n != NULL)
		m_freem(n);
	return (ENOMEM);
}


int
re_tx_list_init(struct rl_softc *sc)
{
	int i;

	memset(sc->rl_ldata.rl_tx_list, 0, RL_TX_LIST_SZ(sc));
	for (i = 0; i < RL_TX_QLEN; i++) {
		sc->rl_ldata.rl_txq[i].txq_mbuf = NULL;
	}

	bus_dmamap_sync(sc->sc_dmat,
	    sc->rl_ldata.rl_tx_list_map, 0,
	    sc->rl_ldata.rl_tx_list_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	sc->rl_ldata.rl_txq_prodidx = 0;
	sc->rl_ldata.rl_txq_considx = 0;
	sc->rl_ldata.rl_tx_free = RL_TX_DESC_CNT(sc);
	sc->rl_ldata.rl_tx_nextfree = 0;

	return (0);
}

int
re_rx_list_init(struct rl_softc *sc)
{
	int	i;

	memset((char *)sc->rl_ldata.rl_rx_list, 0, RL_RX_LIST_SZ);

	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		if (re_newbuf(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
	}

	sc->rl_ldata.rl_rx_prodidx = 0;
	sc->rl_head = sc->rl_tail = NULL;

	return (0);
}

/*
 * RX handler for C+ and 8169. For the gigE chips, we support
 * the reception of jumbo frames that have been fragmented
 * across multiple 2K mbuf cluster buffers.
 */
void
re_rxeof(struct rl_softc *sc)
{
	struct mbuf	*m;
	struct ifnet	*ifp;
	int		i, total_len;
	struct rl_desc	*cur_rx;
	struct rl_rxsoft *rxs;
	u_int32_t	rxstat;

	ifp = &sc->sc_arpcom.ac_if;

	for (i = sc->rl_ldata.rl_rx_prodidx;; i = RL_NEXT_RX_DESC(sc, i)) {
		cur_rx = &sc->rl_ldata.rl_rx_list[i];
		RL_RXDESCSYNC(sc, i,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		rxstat = letoh32(cur_rx->rl_cmdstat);
		RL_RXDESCSYNC(sc, i, BUS_DMASYNC_PREREAD);
		if ((rxstat & RL_RDESC_STAT_OWN) != 0)
			break;
		total_len = rxstat & sc->rl_rxlenmask;
		rxs = &sc->rl_ldata.rl_rxsoft[i];
		m = rxs->rxs_mbuf;

		/* Invalidate the RX mbuf and unload its map */

		bus_dmamap_sync(sc->sc_dmat,
		    rxs->rxs_dmamap, 0, rxs->rxs_dmamap->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, rxs->rxs_dmamap);

		if (!(rxstat & RL_RDESC_STAT_EOF)) {
			m->m_len = RE_RX_DESC_BUFLEN;
			if (sc->rl_head == NULL)
				sc->rl_head = sc->rl_tail = m;
			else {
				m->m_flags &= ~M_PKTHDR;
				sc->rl_tail->m_next = m;
				sc->rl_tail = m;
			}
			re_newbuf(sc, i, NULL);
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

		/*
		 * if total_len > 2^13-1, both _RXERRSUM and _GIANT will be
		 * set, but if CRC is clear, it will still be a valid frame.
		 */
		if (rxstat & RL_RDESC_STAT_RXERRSUM && !(total_len > 8191 &&
		    (rxstat & RL_RDESC_STAT_ERRS) == RL_RDESC_STAT_GIANT)) {
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
			continue;
		}

		if (sc->rl_head != NULL) {
			m->m_len = total_len % RE_RX_DESC_BUFLEN;
			if (m->m_len == 0)
				m->m_len = RE_RX_DESC_BUFLEN;
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

		/* Do RX checksumming */

		/* Check IP header checksum */
		if ((rxstat & RL_RDESC_STAT_PROTOID) &&
		    !(rxstat & RL_RDESC_STAT_IPSUMBAD))
			m->m_pkthdr.csum_flags |= M_IPV4_CSUM_IN_OK;

		/* Check TCP/UDP checksum */
		if ((RL_TCPPKT(rxstat) &&
		    !(rxstat & RL_RDESC_STAT_TCPSUMBAD)) ||
		    (RL_UDPPKT(rxstat) &&
		    !(rxstat & RL_RDESC_STAT_UDPSUMBAD)))
			m->m_pkthdr.csum_flags |= M_TCP_CSUM_IN_OK | M_UDP_CSUM_IN_OK;

#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
#endif
		ether_input_mbuf(ifp, m);
	}

	sc->rl_ldata.rl_rx_prodidx = i;
}

void
re_txeof(struct rl_softc *sc)
{
	struct ifnet	*ifp;
	struct rl_txq	*txq;
	uint32_t	txstat;
	int		idx, descidx;

	ifp = &sc->sc_arpcom.ac_if;

	for (idx = sc->rl_ldata.rl_txq_considx;; idx = RL_NEXT_TXQ(sc, idx)) {
		txq = &sc->rl_ldata.rl_txq[idx];

		if (txq->txq_mbuf == NULL) {
			KASSERT(idx == sc->rl_ldata.rl_txq_prodidx);
			break;
		}

		descidx = txq->txq_descidx;
		RL_TXDESCSYNC(sc, descidx,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		txstat =
		    letoh32(sc->rl_ldata.rl_tx_list[descidx].rl_cmdstat);
		RL_TXDESCSYNC(sc, descidx, BUS_DMASYNC_PREREAD);
		KASSERT((txstat & RL_TDESC_CMD_EOF) != 0);
		if (txstat & RL_TDESC_CMD_OWN)
			break;

		sc->rl_ldata.rl_tx_free += txq->txq_nsegs;
		KASSERT(sc->rl_ldata.rl_tx_free <= RL_TX_DESC_CNT(sc));
		bus_dmamap_sync(sc->sc_dmat, txq->txq_dmamap,
		    0, txq->txq_dmamap->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, txq->txq_dmamap);
		m_freem(txq->txq_mbuf);
		txq->txq_mbuf = NULL;

		if (txstat & (RL_TDESC_STAT_EXCESSCOL | RL_TDESC_STAT_COLCNT))
			ifp->if_collisions++;
		if (txstat & RL_TDESC_STAT_TXERRSUM)
			ifp->if_oerrors++;
		else
			ifp->if_opackets++;
	}

	sc->rl_ldata.rl_txq_considx = idx;

	if (sc->rl_ldata.rl_tx_free > RL_NTXDESC_RSVD)
		ifp->if_flags &= ~IFF_OACTIVE;

	if (sc->rl_ldata.rl_tx_free < RL_TX_DESC_CNT(sc)) {
		/*
		 * Some chips will ignore a second TX request issued while an
		 * existing transmission is in progress. If the transmitter goes
		 * idle but there are still packets waiting to be sent, we need
		 * to restart the channel here to flush them out. This only
		 * seems to be required with the PCIe devices.
		 */
		CSR_WRITE_1(sc, sc->rl_txstart, RL_TXSTART_START);

		/*
		 * If not all descriptors have been released reaped yet,
		 * reload the timer so that we will eventually get another
		 * interrupt that will cause us to re-enter this routine.
		 * This is done in case the transmitter has gone idle.
		 */
		CSR_WRITE_4(sc, RL_TIMERCNT, 1);
	} else
		ifp->if_timer = 0;
}

void
re_tick(void *xsc)
{
	struct rl_softc	*sc = xsc;
	struct mii_data	*mii;
	struct ifnet	*ifp;
	int s;

	ifp = &sc->sc_arpcom.ac_if;
	mii = &sc->sc_mii;

	s = splnet();

	mii_tick(mii);
	if (sc->rl_link) {
		if (!(mii->mii_media_status & IFM_ACTIVE))
			sc->rl_link = 0;
	} else {
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			sc->rl_link = 1;
			if (!IFQ_IS_EMPTY(&ifp->if_snd))
				re_start(ifp);
		}
	}
	splx(s);

	timeout_add(&sc->timer_handle, hz);
}

int
re_intr(void *arg)
{
	struct rl_softc	*sc = arg;
	struct ifnet	*ifp;
	u_int16_t	status;
	int		claimed = 0;

	ifp = &sc->sc_arpcom.ac_if;

	if (!(ifp->if_flags & IFF_UP))
		return (0);

	for (;;) {

		status = CSR_READ_2(sc, RL_ISR);
		/* If the card has gone away the read returns 0xffff. */
		if (status == 0xffff)
			break;
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		if ((status & RL_INTRS_CPLUS) == 0)
			break;

		if (status & (RL_ISR_RX_OK | RL_ISR_RX_ERR)) {
			re_rxeof(sc);
			claimed = 1;
		}

		if (status & (RL_ISR_TIMEOUT_EXPIRED | RL_ISR_TX_ERR |
		    RL_ISR_TX_DESC_UNAVAIL)) {
			re_txeof(sc);
			claimed = 1;
		}

		if (status & RL_ISR_SYSTEM_ERR) {
			re_reset(sc);
			re_init(ifp);
			claimed = 1;
		}

		if (status & RL_ISR_LINKCHG) {
			timeout_del(&sc->timer_handle);
			re_tick(sc);
			claimed = 1;
		}
	}

	if (claimed && !IFQ_IS_EMPTY(&ifp->if_snd))
		re_start(ifp);

	return (claimed);
}

int
re_encap(struct rl_softc *sc, struct mbuf *m, int *idx)
{
	bus_dmamap_t	map;
	int		error, seg, nsegs, uidx, startidx, curidx, lastidx, pad;
	struct rl_desc	*d;
	u_int32_t	cmdstat, rl_flags = 0;
	struct rl_txq	*txq;
#if NVLAN > 0
	struct ifvlan	*ifv = NULL;

	if ((m->m_flags & (M_PROTO1|M_PKTHDR)) == (M_PROTO1|M_PKTHDR) &&
	    m->m_pkthdr.rcvif != NULL)
		ifv = m->m_pkthdr.rcvif->if_softc;
#endif

	if (sc->rl_ldata.rl_tx_free <= RL_NTXDESC_RSVD)
		return (EFBIG);

	/*
	 * Set up checksum offload. Note: checksum offload bits must
	 * appear in all descriptors of a multi-descriptor transmit
	 * attempt. This is according to testing done with an 8169
	 * chip. This is a requirement.
	 */

	/*
	 * Set RL_TDESC_CMD_IPCSUM if any checksum offloading
	 * is requested.  Otherwise, RL_TDESC_CMD_TCPCSUM/
	 * RL_TDESC_CMD_UDPCSUM does not take affect.
	 */

	if ((m->m_pkthdr.csum_flags &
	    (M_IPV4_CSUM_OUT|M_TCPV4_CSUM_OUT|M_UDPV4_CSUM_OUT)) != 0) {
		rl_flags |= RL_TDESC_CMD_IPCSUM;
		if (m->m_pkthdr.csum_flags & M_TCPV4_CSUM_OUT)
			rl_flags |= RL_TDESC_CMD_TCPCSUM;
		if (m->m_pkthdr.csum_flags & M_UDPV4_CSUM_OUT)
			rl_flags |= RL_TDESC_CMD_UDPCSUM;
	}

	txq = &sc->rl_ldata.rl_txq[*idx];
	map = txq->txq_dmamap;
	error = bus_dmamap_load_mbuf(sc->sc_dmat, map, m,
	    BUS_DMA_WRITE|BUS_DMA_NOWAIT);
	if (error) {
		/* XXX try to defrag if EFBIG? */
		printf("%s: can't map mbuf (error %d)\n",
		    sc->sc_dev.dv_xname, error);
		return (error);
	}

	nsegs = map->dm_nsegs;
	pad = 0;
	if (m->m_pkthdr.len <= RL_IP4CSUMTX_PADLEN &&
	    (rl_flags & RL_TDESC_CMD_IPCSUM) != 0) {
		pad = 1;
		nsegs++;
	}

	if (nsegs > sc->rl_ldata.rl_tx_free - RL_NTXDESC_RSVD) {
		error = EFBIG;
		goto fail_unload;
	}

	/*
	 * Make sure that the caches are synchronized before we
	 * ask the chip to start DMA for the packet data.
	 */
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
		BUS_DMASYNC_PREWRITE);

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
	curidx = startidx = sc->rl_ldata.rl_tx_nextfree;
	lastidx = -1;
	for (seg = 0; seg < map->dm_nsegs;
	    seg++, curidx = RL_NEXT_TX_DESC(sc, curidx)) {
		d = &sc->rl_ldata.rl_tx_list[curidx];
		RL_TXDESCSYNC(sc, curidx,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		cmdstat = letoh32(d->rl_cmdstat);
		RL_TXDESCSYNC(sc, curidx, BUS_DMASYNC_PREREAD);
		if (cmdstat & RL_TDESC_STAT_OWN) {
			printf("%s: tried to map busy TX descriptor\n",
			    sc->sc_dev.dv_xname);
			for (; seg > 0; seg --) {
				uidx = (curidx + RL_TX_DESC_CNT(sc) - seg) %
				    RL_TX_DESC_CNT(sc);
				sc->rl_ldata.rl_tx_list[uidx].rl_cmdstat = 0;
				RL_TXDESCSYNC(sc, uidx,
				    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
			}
			error = ENOBUFS;
			goto fail_unload;
		}

		d->rl_vlanctl = 0;
		re_set_bufaddr(d, map->dm_segs[seg].ds_addr);
		cmdstat = rl_flags | map->dm_segs[seg].ds_len;
		if (seg == 0)
			cmdstat |= RL_TDESC_CMD_SOF;
		else
			cmdstat |= RL_TDESC_CMD_OWN;
		if (curidx == (RL_TX_DESC_CNT(sc) - 1))
			cmdstat |= RL_TDESC_CMD_EOR;
		if (seg == nsegs - 1) {
			cmdstat |= RL_TDESC_CMD_EOF;
			lastidx = curidx;
		}
		d->rl_cmdstat = htole32(cmdstat);
		RL_TXDESCSYNC(sc, curidx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}
	if (pad) {
		bus_addr_t paddaddr;

		d = &sc->rl_ldata.rl_tx_list[curidx];
		d->rl_vlanctl = 0;
		paddaddr = RL_TXPADDADDR(sc);
		re_set_bufaddr(d, paddaddr);
		cmdstat = rl_flags |
		    RL_TDESC_CMD_OWN | RL_TDESC_CMD_EOF |
		    (RL_IP4CSUMTX_PADLEN + 1 - m->m_pkthdr.len);
		if (curidx == (RL_TX_DESC_CNT(sc) - 1))
			cmdstat |= RL_TDESC_CMD_EOR;
		d->rl_cmdstat = htole32(cmdstat);
		RL_TXDESCSYNC(sc, curidx,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
		lastidx = curidx;
		curidx = RL_NEXT_TX_DESC(sc, curidx);
	}
	KASSERT(lastidx != -1);

	/*
	 * Set up hardware VLAN tagging. Note: vlan tag info must
	 * appear in the first descriptor of a multi-descriptor
	 * transmission attempt.
	 */

#if NVLAN > 0
	if (ifv != NULL) {
		sc->rl_ldata.rl_tx_list[startidx].rl_vlanctl =
		    htole32(swap16(ifv->ifv_tag) |
		    RL_TDESC_VLANCTL_TAG);
	}
#endif

	/* Transfer ownership of packet to the chip. */

	sc->rl_ldata.rl_tx_list[startidx].rl_cmdstat |=
	    htole32(RL_TDESC_CMD_OWN);
	RL_TXDESCSYNC(sc, startidx, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	/* update info of TX queue and descriptors */
	txq->txq_mbuf = m;
	txq->txq_descidx = lastidx;
	txq->txq_nsegs = nsegs;

	sc->rl_ldata.rl_tx_free -= nsegs;
	sc->rl_ldata.rl_tx_nextfree = curidx;

	*idx = RL_NEXT_TXQ(sc, *idx);

	return (0);

fail_unload:
	bus_dmamap_unload(sc->sc_dmat, map);

	return (error);
}

/*
 * Main transmit routine for C+ and gigE NICs.
 */

void
re_start(struct ifnet *ifp)
{
	struct rl_softc	*sc;
	int		idx, queued = 0;

	sc = ifp->if_softc;

	if (!sc->rl_link || ifp->if_flags & IFF_OACTIVE)
		return;

	idx = sc->rl_ldata.rl_txq_prodidx;
	for (;;) {
		struct mbuf *m;
		int error;

		IFQ_POLL(&ifp->if_snd, m);
		if (m == NULL)
			break;

		if (sc->rl_ldata.rl_txq[idx].txq_mbuf != NULL) {
			KASSERT(idx == sc->rl_ldata.rl_txq_considx);
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		error = re_encap(sc, m, &idx);
		if (error == EFBIG &&
		    sc->rl_ldata.rl_tx_free == RL_TX_DESC_CNT(sc)) {
			IFQ_DEQUEUE(&ifp->if_snd, m);
			m_freem(m);
			ifp->if_oerrors++;
			continue;
		}
		if (error) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m);
		queued++;

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_OUT);
#endif
	}

	if (queued == 0) {
		if (sc->rl_ldata.rl_tx_free != RL_TX_DESC_CNT(sc))
			CSR_WRITE_4(sc, RL_TIMERCNT, 1);
		return;
	}

	sc->rl_ldata.rl_txq_prodidx = idx;

	CSR_WRITE_1(sc, sc->rl_txstart, RL_TXSTART_START);

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
}

int
re_init(struct ifnet *ifp)
{
	struct rl_softc *sc = ifp->if_softc;
	u_int32_t	rxcfg = 0;
	int		s;
	union {
		u_int32_t align_dummy;
		u_char eaddr[ETHER_ADDR_LEN];
	} eaddr;

	s = splnet();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	re_stop(ifp, 0);

	/*
	 * Enable C+ RX and TX mode, as well as RX checksum offload.
	 * We must configure the C+ register before all others.
	 */
	CSR_WRITE_2(sc, RL_CPLUS_CMD, RL_CPLUSCMD_RXENB|
	    RL_CPLUSCMD_TXENB|RL_CPLUSCMD_PCI_MRW|
	    RL_CPLUSCMD_RXCSUM_ENB);

	/*
	 * Init our MAC address.  Even though the chipset
	 * documentation doesn't mention it, we need to enter "Config
	 * register write enable" mode to modify the ID registers.
	 */
	bcopy(sc->sc_arpcom.ac_enaddr, eaddr.eaddr, ETHER_ADDR_LEN);
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_WRITECFG);
	CSR_WRITE_4(sc, RL_IDR4,
	    htole32(*(u_int32_t *)(&eaddr.eaddr[4])));
	CSR_WRITE_4(sc, RL_IDR0,
	    htole32(*(u_int32_t *)(&eaddr.eaddr[0])));
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_OFF);

	/*
	 * For C+ mode, initialize the RX descriptors and mbufs.
	 */
	re_rx_list_init(sc);
	re_tx_list_init(sc);

	/*
	 * Load the addresses of the RX and TX lists into the chip.
	 */
	CSR_WRITE_4(sc, RL_RXLIST_ADDR_HI,
	    RL_ADDR_HI(sc->rl_ldata.rl_rx_list_map->dm_segs[0].ds_addr));
	CSR_WRITE_4(sc, RL_RXLIST_ADDR_LO,
	    RL_ADDR_LO(sc->rl_ldata.rl_rx_list_map->dm_segs[0].ds_addr));

	CSR_WRITE_4(sc, RL_TXLIST_ADDR_HI,
	    RL_ADDR_HI(sc->rl_ldata.rl_tx_list_map->dm_segs[0].ds_addr));
	CSR_WRITE_4(sc, RL_TXLIST_ADDR_LO,
	    RL_ADDR_LO(sc->rl_ldata.rl_tx_list_map->dm_segs[0].ds_addr));

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

	CSR_WRITE_1(sc, RL_EARLY_TX_THRESH, 16);

	CSR_WRITE_4(sc, RL_RXCFG, RL_RXCFG_CONFIG);

	/* Set the individual bit to receive frames for this host only. */
	rxcfg = CSR_READ_4(sc, RL_RXCFG);
	rxcfg |= RL_RXCFG_RX_INDIV;

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST)
		rxcfg |= RL_RXCFG_RX_BROAD;
	else
		rxcfg &= ~RL_RXCFG_RX_BROAD;

	CSR_WRITE_4(sc, RL_RXCFG, rxcfg);

	/* Set promiscuous mode. */
	re_setpromisc(sc);

	/*
	 * Program the multicast filter, if necessary.
	 */
	re_setmulti(sc);

	/*
	 * Enable interrupts.
	 */
	if (sc->rl_testmode)
		CSR_WRITE_2(sc, RL_IMR, 0);
	else
		CSR_WRITE_2(sc, RL_IMR, RL_INTRS_CPLUS);
	CSR_WRITE_2(sc, RL_ISR, RL_INTRS_CPLUS);

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);
#ifdef notdef
	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);
#endif

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
		return (0);

	mii_mediachg(&sc->sc_mii);

	CSR_WRITE_1(sc, RL_CFG1, CSR_READ_1(sc, RL_CFG1) | RL_CFG1_DRVLOAD);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	sc->rl_link = 0;

	timeout_add(&sc->timer_handle, hz);

	return (0);
}

/*
 * Set media options.
 */
int
re_ifmedia_upd(struct ifnet *ifp)
{
	struct rl_softc	*sc;

	sc = ifp->if_softc;

	return (mii_mediachg(&sc->sc_mii));
}

/*
 * Report current media status.
 */
void
re_ifmedia_sts(struct ifnet *ifp, struct ifmediareq *ifmr)
{
	struct rl_softc	*sc;

	sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
}

int
re_ioctl(struct ifnet *ifp, u_long command, caddr_t data)
{
	struct rl_softc	*sc = ifp->if_softc;
	struct ifreq	*ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int		s, error = 0;

	s = splnet();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command,
	    data)) > 0) {
		splx(s);
		return (error);
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		if (!(ifp->if_flags & IFF_RUNNING))
			re_init(ifp);
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET)
			arp_ifinit(&sc->sc_arpcom, ifa);
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
			    ((ifp->if_flags ^ sc->if_flags) &
			     IFF_PROMISC)) {
				re_setpromisc(sc);
			} else {
				if (!(ifp->if_flags & IFF_RUNNING))
					re_init(ifp);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				re_stop(ifp, 1);
		}
		sc->if_flags = ifp->if_flags;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (command == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &sc->sc_arpcom) :
		    ether_delmulti(ifr, &sc->sc_arpcom);
		if (error == ENETRESET) {
			/*
			 * Multicast list has changed; set the hardware
			 * filter accordingly.
			 */
			if (ifp->if_flags & IFF_RUNNING)
				re_setmulti(sc);
			error = 0;
		}
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

	return (error);
}

void
re_watchdog(struct ifnet *ifp)
{
	struct rl_softc	*sc;
	int	s;

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
re_stop(struct ifnet *ifp, int disable)
{
	struct rl_softc *sc;
	int	i;

	sc = ifp->if_softc;

	ifp->if_timer = 0;
	sc->rl_link = 0;

	timeout_del(&sc->timer_handle);
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	mii_down(&sc->sc_mii);

	CSR_WRITE_1(sc, RL_COMMAND, 0x00);
	CSR_WRITE_2(sc, RL_IMR, 0x0000);
	CSR_WRITE_2(sc, RL_ISR, 0xFFFF);

	if (sc->rl_head != NULL) {
		m_freem(sc->rl_head);
		sc->rl_head = sc->rl_tail = NULL;
	}

	/* Free the TX list buffers. */
	for (i = 0; i < RL_TX_QLEN; i++) {
		if (sc->rl_ldata.rl_txq[i].txq_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->rl_ldata.rl_txq[i].txq_dmamap);
			m_freem(sc->rl_ldata.rl_txq[i].txq_mbuf);
			sc->rl_ldata.rl_txq[i].txq_mbuf = NULL;
		}
	}

	/* Free the RX list buffers. */
	for (i = 0; i < RL_RX_DESC_CNT; i++) {
		if (sc->rl_ldata.rl_rxsoft[i].rxs_mbuf != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->rl_ldata.rl_rxsoft[i].rxs_dmamap);
			m_freem(sc->rl_ldata.rl_rxsoft[i].rxs_mbuf);
			sc->rl_ldata.rl_rxsoft[i].rxs_mbuf = NULL;
		}
	}
}
