/*	$OpenBSD: xl.c,v 1.62 2005/01/15 05:24:11 brad Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
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
 * $FreeBSD: if_xl.c,v 1.77 2000/08/28 20:40:03 wpaul Exp $
 */

/*
 * 3Com 3c90x Etherlink XL PCI NIC driver
 *
 * Supports the 3Com "boomerang", "cyclone", and "hurricane" PCI
 * bus-master chips (3c90x cards and embedded controllers) including
 * the following:
 *
 * 3Com 3c900-TPO	10Mbps/RJ-45
 * 3Com 3c900-COMBO	10Mbps/RJ-45,AUI,BNC
 * 3Com 3c905-TX	10/100Mbps/RJ-45
 * 3Com 3c905-T4	10/100Mbps/RJ-45
 * 3Com 3c900B-TPO	10Mbps/RJ-45
 * 3Com 3c900B-COMBO	10Mbps/RJ-45,AUI,BNC
 * 3Com 3c900B-TPC	10Mbps/RJ-45,BNC
 * 3Com 3c900B-FL	10Mbps/Fiber-optic
 * 3Com 3c905B-COMBO	10/100Mbps/RJ-45,AUI,BNC
 * 3Com 3c905B-TX	10/100Mbps/RJ-45
 * 3Com 3c905B-FL/FX	10/100Mbps/Fiber-optic
 * 3Com 3c905C-TX	10/100Mbps/RJ-45 (Tornado ASIC)
 * 3Com 3c980-TX	10/100Mbps server adapter (Hurricane ASIC)
 * 3Com 3c980C-TX	10/100Mbps server adapter (Tornado ASIC)
 * 3Com 3cSOHO100-TX	10/100Mbps/RJ-45 (Hurricane ASIC)
 * 3Com 3c450-TX	10/100Mbps/RJ-45 (Tornado ASIC)
 * 3Com 3c555		10/100Mbps/RJ-45 (MiniPCI, Laptop Hurricane)
 * 3Com 3c556		10/100Mbps/RJ-45 (MiniPCI, Hurricane ASIC)
 * 3Com 3c556B		10/100Mbps/RJ-45 (MiniPCI, Hurricane ASIC)
 * 3Com 3c575TX		10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3c575B		10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3c575C		10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3cxfem656	10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3cxfem656b	10/100Mbps/RJ-45 (Cardbus, Hurricane ASIC)
 * 3Com 3cxfem656c	10/100Mbps/RJ-45 (Cardbus, Tornado ASIC)
 * Dell Optiplex GX1 on-board 3c918 10/100Mbps/RJ-45
 * Dell on-board 3c920 10/100Mbps/RJ-45
 * Dell Precision on-board 3c905B 10/100Mbps/RJ-45
 * Dell Latitude laptop docking station embedded 3c905-TX
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The 3c90x series chips use a bus-master DMA interface for transfering
 * packets to and from the controller chip. Some of the "vortex" cards
 * (3c59x) also supported a bus master mode, however for those chips
 * you could only DMA packets to/from a contiguous memory buffer. For
 * transmission this would mean copying the contents of the queued mbuf
 * chain into an mbuf cluster and then DMAing the cluster. This extra
 * copy would sort of defeat the purpose of the bus master support for
 * any packet that doesn't fit into a single mbuf.
 *
 * By contrast, the 3c90x cards support a fragment-based bus master
 * mode where mbuf chains can be encapsulated using TX descriptors.
 * This is similar to other PCI chips such as the Texas Instruments
 * ThunderLAN and the Intel 82557/82558.
 *
 * The "vortex" driver (if_vx.c) happens to work for the "boomerang"
 * bus master chips because they maintain the old PIO interface for
 * backwards compatibility, but starting with the 3c905B and the
 * "cyclone" chips, the compatibility interface has been dropped.
 * Since using bus master DMA is a big win, we use this driver to
 * support the PCI "boomerang" chips even though they work with the
 * "vortex" driver in order to obtain better performance.
 */

#include "bpfilter.h"
#include "vlan.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/protosw.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>   /* only for declaration of wakeup() used by vm.h */
#include <sys/device.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_types.h>
#include <net/if_media.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/in_systm.h>
#include <netinet/in_var.h>
#include <netinet/ip.h>
#include <netinet/if_ether.h>
#endif

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <machine/bus.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <dev/ic/xlreg.h>

/* 
 * TX Checksumming is disabled by default for two reasons:
 * - TX Checksumming will occasionally produce corrupt packets
 * - TX Checksumming seems to reduce performance
 *
 * Only 905B/C cards were reported to have this problem, it is possible
 * that later chips _may_ be immune.
 */
#define	XL905B_TXCSUM_BROKEN	1

int xl_newbuf(struct xl_softc *, struct xl_chain_onefrag *);
void xl_stats_update(void *);
int xl_encap(struct xl_softc *, struct xl_chain *,
    struct mbuf * );
int xl_encap_90xB(struct xl_softc *, struct xl_chain *,
    struct mbuf * );
void xl_rxeof(struct xl_softc *);
int xl_rx_resync(struct xl_softc *);
void xl_txeof(struct xl_softc *);
void xl_txeof_90xB(struct xl_softc *);
void xl_txeoc(struct xl_softc *);
int xl_intr(void *);
void xl_start(struct ifnet *);
void xl_start_90xB(struct ifnet *);
int xl_ioctl(struct ifnet *, u_long, caddr_t);
void xl_init(void *);
void xl_stop(struct xl_softc *);
void xl_freetxrx(struct xl_softc *);
void xl_watchdog(struct ifnet *);
void xl_shutdown(void *);
int xl_ifmedia_upd(struct ifnet *);
void xl_ifmedia_sts(struct ifnet *, struct ifmediareq *);

int xl_eeprom_wait(struct xl_softc *);
int xl_read_eeprom(struct xl_softc *, caddr_t, int, int, int);
void xl_mii_sync(struct xl_softc *);
void xl_mii_send(struct xl_softc *, u_int32_t, int);
int xl_mii_readreg(struct xl_softc *, struct xl_mii_frame *);
int xl_mii_writereg(struct xl_softc *, struct xl_mii_frame *);

void xl_setcfg(struct xl_softc *);
void xl_setmode(struct xl_softc *, int);
void xl_setmulti(struct xl_softc *);
void xl_setmulti_hash(struct xl_softc *);
void xl_reset(struct xl_softc *);
int xl_list_rx_init(struct xl_softc *);
int xl_list_tx_init(struct xl_softc *);
int xl_list_tx_init_90xB(struct xl_softc *);
void xl_wait(struct xl_softc *);
void xl_mediacheck(struct xl_softc *);
void xl_choose_xcvr(struct xl_softc *, int);
#ifdef notdef
void xl_testpacket(struct xl_softc *);
#endif

int xl_miibus_readreg(struct device *, int, int);
void xl_miibus_writereg(struct device *, int, int, int);
void xl_miibus_statchg(struct device *);

void xl_power(int, void *);

void
xl_power(why, arg)
	int why;
	void *arg;
{
	struct xl_softc *sc = arg;
	struct ifnet *ifp;
	int s;

	s = splimp();
	if (why != PWR_RESUME)
		xl_stop(sc);
	else {
		ifp = &sc->sc_arpcom.ac_if;
		if (ifp->if_flags & IFF_UP) {
			xl_reset(sc);
			xl_init(sc);
		}
	}
	splx(s);
}

/*
 * Murphy's law says that it's possible the chip can wedge and
 * the 'command in progress' bit may never clear. Hence, we wait
 * only a finite amount of time to avoid getting caught in an
 * infinite loop. Normally this delay routine would be a macro,
 * but it isn't called during normal operation so we can afford
 * to make it a function.
 */
void
xl_wait(sc)
	struct xl_softc		*sc;
{
	register int		i;

	for (i = 0; i < XL_TIMEOUT; i++) {
		if (!(CSR_READ_2(sc, XL_STATUS) & XL_STAT_CMDBUSY))
			break;
	}

	if (i == XL_TIMEOUT)
		printf("%s: command never completed!\n", sc->sc_dev.dv_xname);

	return;
}

/*
 * MII access routines are provided for adapters with external
 * PHYs (3c905-TX, 3c905-T4, 3c905B-T4) and those with built-in
 * autoneg logic that's faked up to look like a PHY (3c905B-TX).
 * Note: if you don't perform the MDIO operations just right,
 * it's possible to end up with code that works correctly with
 * some chips/CPUs/processor speeds/bus speeds/etc but not
 * with others.
 */
#define MII_SET(x)					\
	CSR_WRITE_2(sc, XL_W4_PHY_MGMT,			\
		CSR_READ_2(sc, XL_W4_PHY_MGMT) | (x))

#define MII_CLR(x)					\
	CSR_WRITE_2(sc, XL_W4_PHY_MGMT,			\
		CSR_READ_2(sc, XL_W4_PHY_MGMT) & ~(x))

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
void
xl_mii_sync(sc)
	struct xl_softc		*sc;
{
	register int		i;

	XL_SEL_WIN(4);
	MII_SET(XL_MII_DIR|XL_MII_DATA);

	for (i = 0; i < 32; i++) {
		MII_SET(XL_MII_CLK);
		MII_SET(XL_MII_DATA);
		MII_SET(XL_MII_DATA);
		MII_CLR(XL_MII_CLK);
		MII_SET(XL_MII_DATA);
		MII_SET(XL_MII_DATA);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
void
xl_mii_send(sc, bits, cnt)
	struct xl_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	XL_SEL_WIN(4);
	MII_CLR(XL_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			MII_SET(XL_MII_DATA);
                } else {
			MII_CLR(XL_MII_DATA);
                }
		MII_CLR(XL_MII_CLK);
		MII_SET(XL_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
int
xl_mii_readreg(sc, frame)
	struct xl_softc		*sc;
	struct xl_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = XL_MII_STARTDELIM;
	frame->mii_opcode = XL_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	/*
	 * Select register window 4.
	 */

	XL_SEL_WIN(4);

	CSR_WRITE_2(sc, XL_W4_PHY_MGMT, 0);
	/*
 	 * Turn on data xmit.
	 */
	MII_SET(XL_MII_DIR);

	xl_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	xl_mii_send(sc, frame->mii_stdelim, 2);
	xl_mii_send(sc, frame->mii_opcode, 2);
	xl_mii_send(sc, frame->mii_phyaddr, 5);
	xl_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	MII_CLR((XL_MII_CLK|XL_MII_DATA));
	MII_SET(XL_MII_CLK);

	/* Turn off xmit. */
	MII_CLR(XL_MII_DIR);

	/* Check for ack */
	MII_CLR(XL_MII_CLK);
	ack = CSR_READ_2(sc, XL_W4_PHY_MGMT) & XL_MII_DATA;
	MII_SET(XL_MII_CLK);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			MII_CLR(XL_MII_CLK);
			MII_SET(XL_MII_CLK);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		MII_CLR(XL_MII_CLK);
		if (!ack) {
			if (CSR_READ_2(sc, XL_W4_PHY_MGMT) & XL_MII_DATA)
				frame->mii_data |= i;
		}
		MII_SET(XL_MII_CLK);
	}

fail:

	MII_CLR(XL_MII_CLK);
	MII_SET(XL_MII_CLK);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
int
xl_mii_writereg(sc, frame)
	struct xl_softc		*sc;
	struct xl_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = XL_MII_STARTDELIM;
	frame->mii_opcode = XL_MII_WRITEOP;
	frame->mii_turnaround = XL_MII_TURNAROUND;
	
	/*
	 * Select the window 4.
	 */
	XL_SEL_WIN(4);

	/*
 	 * Turn on data output.
	 */
	MII_SET(XL_MII_DIR);

	xl_mii_sync(sc);

	xl_mii_send(sc, frame->mii_stdelim, 2);
	xl_mii_send(sc, frame->mii_opcode, 2);
	xl_mii_send(sc, frame->mii_phyaddr, 5);
	xl_mii_send(sc, frame->mii_regaddr, 5);
	xl_mii_send(sc, frame->mii_turnaround, 2);
	xl_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	MII_SET(XL_MII_CLK);
	MII_CLR(XL_MII_CLK);

	/*
	 * Turn off xmit.
	 */
	MII_CLR(XL_MII_DIR);

	splx(s);

	return(0);
}

int
xl_miibus_readreg(self, phy, reg)
	struct device *self;
	int phy, reg;
{
	struct xl_softc *sc = (struct xl_softc *)self;
	struct xl_mii_frame	frame;

	if (!(sc->xl_flags & XL_FLAG_PHYOK) && phy != 24)
		return (0);

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	xl_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

void
xl_miibus_writereg(self, phy, reg, data)
	struct device *self;
	int phy, reg, data;
{
	struct xl_softc *sc = (struct xl_softc *)self;
	struct xl_mii_frame	frame;

	if (!(sc->xl_flags & XL_FLAG_PHYOK) && phy != 24)
		return;

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	xl_mii_writereg(sc, &frame);

	return;
}

void
xl_miibus_statchg(self)
	struct device *self;
{
	struct xl_softc *sc = (struct xl_softc *)self;

	xl_setcfg(sc);

	/* Set ASIC's duplex mode to match the PHY. */
	XL_SEL_WIN(3);
	if ((sc->sc_mii.mii_media_active & IFM_GMASK) == IFM_FDX)
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL, XL_MACCTRL_DUPLEX);
	else
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL,
		    (CSR_READ_1(sc, XL_W3_MAC_CTRL) & ~XL_MACCTRL_DUPLEX));
}

/*
 * The EEPROM is slow: give it time to come ready after issuing
 * it a command.
 */
int
xl_eeprom_wait(sc)
	struct xl_softc		*sc;
{
	int			i;

	for (i = 0; i < 100; i++) {
		if (CSR_READ_2(sc, XL_W0_EE_CMD) & XL_EE_BUSY)
			DELAY(162);
		else
			break;
	}

	if (i == 100) {
		printf("%s: eeprom failed to come ready\n", sc->sc_dev.dv_xname);
		return(1);
	}

	return(0);
}

/*
 * Read a sequence of words from the EEPROM. Note that ethernet address
 * data is stored in the EEPROM in network byte order.
 */
int
xl_read_eeprom(sc, dest, off, cnt, swap)
	struct xl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			err = 0, i;
	u_int16_t		word = 0, *ptr;
#define EEPROM_5BIT_OFFSET(A) ((((A) << 2) & 0x7F00) | ((A) & 0x003F))
#define EEPROM_8BIT_OFFSET(A) ((A) & 0x003F)
	/* WARNING! DANGER!
	 * It's easy to accidentally overwrite the rom content!
	 * Note: the 3c575 uses 8bit EEPROM offsets.
	 */
	XL_SEL_WIN(0);

	if (xl_eeprom_wait(sc))
		return(1);

	if (sc->xl_flags & XL_FLAG_EEPROM_OFFSET_30)
		off += 0x30;

	for (i = 0; i < cnt; i++) {
		if (sc->xl_flags & XL_FLAG_8BITROM)
			CSR_WRITE_2(sc, XL_W0_EE_CMD,
			    XL_EE_8BIT_READ | EEPROM_8BIT_OFFSET(off + i));
		else
			CSR_WRITE_2(sc, XL_W0_EE_CMD,
			    XL_EE_READ | EEPROM_5BIT_OFFSET(off + i));
		err = xl_eeprom_wait(sc);
		if (err)
			break;
		word = CSR_READ_2(sc, XL_W0_EE_DATA);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;	
	}

	return(err ? 1 : 0);
}

/*
 * NICs older than the 3c905B have only one multicast option, which
 * is to enable reception of all multicast frames.
 */
void
xl_setmulti(sc)
	struct xl_softc		*sc;
{
	struct ifnet		*ifp;
	struct arpcom *ac = &sc->sc_arpcom;
	u_int8_t		rxfilt;

	ifp = &sc->sc_arpcom.ac_if;

	XL_SEL_WIN(5);
	rxfilt = CSR_READ_1(sc, XL_W5_RX_FILTER);

	if (ifp->if_flags & IFF_ALLMULTI) {
		rxfilt |= XL_RXFILTER_ALLMULTI;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
		return;
	}

	if (ac->ac_multicnt > 0)
		rxfilt |= XL_RXFILTER_ALLMULTI;
	else
		rxfilt &= ~XL_RXFILTER_ALLMULTI;

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);

	return;
}

/*
 * 3c905B adapters have a hash filter that we can program.
 */
void
xl_setmulti_hash(sc)
	struct xl_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0, i;
	struct arpcom *ac = &sc->sc_arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int8_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->sc_arpcom.ac_if;

	XL_SEL_WIN(5);
	rxfilt = CSR_READ_1(sc, XL_W5_RX_FILTER);

	if (ifp->if_flags & IFF_ALLMULTI) {
allmulti:
		rxfilt |= XL_RXFILTER_ALLMULTI;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
		return;
	} else
		rxfilt &= ~XL_RXFILTER_ALLMULTI;


	/* first, zot all the existing hash bits */
	for (i = 0; i < XL_HASHFILT_SIZE; i++)
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_HASH|i);

	/* now program new ones */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			goto allmulti;
		}
		h = ether_crc32_be(enm->enm_addrlo, ETHER_ADDR_LEN) &
		    0x000000FF;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_HASH|XL_HASH_SET|h);
		mcnt++;
		ETHER_NEXT_MULTI(step, enm);
	}

	if (mcnt)
		rxfilt |= XL_RXFILTER_MULTIHASH;
	else
		rxfilt &= ~XL_RXFILTER_MULTIHASH;

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);

	return;
}

#ifdef notdef
void
xl_testpacket(sc)
	struct xl_softc		*sc;
{
	struct mbuf		*m;
	struct ifnet		*ifp;
	int			error;

	ifp = &sc->sc_arpcom.ac_if;

	MGETHDR(m, M_DONTWAIT, MT_DATA);

	if (m == NULL)
		return;

	bcopy(&sc->sc_arpcom.ac_enaddr,
		mtod(m, struct ether_header *)->ether_dhost, ETHER_ADDR_LEN);
	bcopy(&sc->sc_arpcom.ac_enaddr,
		mtod(m, struct ether_header *)->ether_shost, ETHER_ADDR_LEN);
	mtod(m, struct ether_header *)->ether_type = htons(3);
	mtod(m, unsigned char *)[14] = 0;
	mtod(m, unsigned char *)[15] = 0;
	mtod(m, unsigned char *)[16] = 0xE3;
	m->m_len = m->m_pkthdr.len = sizeof(struct ether_header) + 3;
	IFQ_ENQUEUE(&ifp->if_snd, m, NULL, error);
	xl_start(ifp);

	return;
}
#endif

void
xl_setcfg(sc)
	struct xl_softc *sc;
{
	u_int32_t icfg;

	XL_SEL_WIN(3);
	icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG);
	icfg &= ~XL_ICFG_CONNECTOR_MASK;
	if (sc->xl_media & XL_MEDIAOPT_MII ||
		sc->xl_media & XL_MEDIAOPT_BT4)
		icfg |= (XL_XCVR_MII << XL_ICFG_CONNECTOR_BITS);
	if (sc->xl_media & XL_MEDIAOPT_BTX)
		icfg |= (XL_XCVR_AUTO << XL_ICFG_CONNECTOR_BITS);

	CSR_WRITE_4(sc, XL_W3_INTERNAL_CFG, icfg);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);

	return;
}

void
xl_setmode(sc, media)
	struct xl_softc *sc;
	int media;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	u_int32_t icfg;
	u_int16_t mediastat;

	XL_SEL_WIN(4);
	mediastat = CSR_READ_2(sc, XL_W4_MEDIA_STATUS);
	XL_SEL_WIN(3);
	icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG);

	if (sc->xl_media & XL_MEDIAOPT_BT) {
		if (IFM_SUBTYPE(media) == IFM_10_T) {
			ifp->if_baudrate = IF_Mbps(10);
			sc->xl_xcvr = XL_XCVR_10BT;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_10BT << XL_ICFG_CONNECTOR_BITS);
			mediastat |= XL_MEDIASTAT_LINKBEAT|
					XL_MEDIASTAT_JABGUARD;
			mediastat &= ~XL_MEDIASTAT_SQEENB;
		}
	}

	if (sc->xl_media & XL_MEDIAOPT_BFX) {
		if (IFM_SUBTYPE(media) == IFM_100_FX) {
			ifp->if_baudrate = IF_Mbps(100);
			sc->xl_xcvr = XL_XCVR_100BFX;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_100BFX << XL_ICFG_CONNECTOR_BITS);
			mediastat |= XL_MEDIASTAT_LINKBEAT;
			mediastat &= ~XL_MEDIASTAT_SQEENB;
		}
	}

	if (sc->xl_media & (XL_MEDIAOPT_AUI|XL_MEDIAOPT_10FL)) {
		if (IFM_SUBTYPE(media) == IFM_10_5) {
			ifp->if_baudrate = IF_Mbps(10);
			sc->xl_xcvr = XL_XCVR_AUI;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_AUI << XL_ICFG_CONNECTOR_BITS);
			mediastat &= ~(XL_MEDIASTAT_LINKBEAT|
					XL_MEDIASTAT_JABGUARD);
			mediastat |= ~XL_MEDIASTAT_SQEENB;
		}
		if (IFM_SUBTYPE(media) == IFM_10_FL) {
			ifp->if_baudrate = IF_Mbps(10);
			sc->xl_xcvr = XL_XCVR_AUI;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_AUI << XL_ICFG_CONNECTOR_BITS);
			mediastat &= ~(XL_MEDIASTAT_LINKBEAT|
					XL_MEDIASTAT_JABGUARD);
			mediastat |= ~XL_MEDIASTAT_SQEENB;
		}
	}

	if (sc->xl_media & XL_MEDIAOPT_BNC) {
		if (IFM_SUBTYPE(media) == IFM_10_2) {
			ifp->if_baudrate = IF_Mbps(10);
			sc->xl_xcvr = XL_XCVR_COAX;
			icfg &= ~XL_ICFG_CONNECTOR_MASK;
			icfg |= (XL_XCVR_COAX << XL_ICFG_CONNECTOR_BITS);
			mediastat &= ~(XL_MEDIASTAT_LINKBEAT|
					XL_MEDIASTAT_JABGUARD|
					XL_MEDIASTAT_SQEENB);
		}
	}

	if ((media & IFM_GMASK) == IFM_FDX ||
			IFM_SUBTYPE(media) == IFM_100_FX) {
		XL_SEL_WIN(3);
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL, XL_MACCTRL_DUPLEX);
	} else {
		XL_SEL_WIN(3);
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL,
			(CSR_READ_1(sc, XL_W3_MAC_CTRL) & ~XL_MACCTRL_DUPLEX));
	}

	if (IFM_SUBTYPE(media) == IFM_10_2)
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_START);
	else
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);
	CSR_WRITE_4(sc, XL_W3_INTERNAL_CFG, icfg);
	XL_SEL_WIN(4);
	CSR_WRITE_2(sc, XL_W4_MEDIA_STATUS, mediastat);
	DELAY(800);
	XL_SEL_WIN(7);

	return;
}

void
xl_reset(sc)
	struct xl_softc		*sc;
{
	register int		i;

	XL_SEL_WIN(0);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RESET |
		    ((sc->xl_flags & XL_FLAG_WEIRDRESET) ?
		     XL_RESETOPT_DISADVFD:0));

	/*
	 * If we're using memory mapped register mode, pause briefly
	 * after issuing the reset command before trying to access any
	 * other registers. With my 3c575C cardbus card, failing to do
	 * this results in the system locking up while trying to poll
	 * the command busy bit in the status register.
	 */
	if (sc->xl_flags & XL_FLAG_USE_MMIO)
		DELAY(100000);

	for (i = 0; i < XL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_2(sc, XL_STATUS) & XL_STAT_CMDBUSY))
			break;
	}

	if (i == XL_TIMEOUT)
		printf("%s: reset didn't complete\n", sc->sc_dev.dv_xname);

	/* Note: the RX reset takes an absurd amount of time
	 * on newer versions of the Tornado chips such as those
	 * on the 3c905CX and newer 3c908C cards. We wait an
	 * extra amount of time so that xl_wait() doesn't complain
	 * and annoy the users.
	 */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
	DELAY(100000);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);

	if (sc->xl_flags & XL_FLAG_INVERT_LED_PWR || 
	    sc->xl_flags & XL_FLAG_INVERT_MII_PWR) {
		XL_SEL_WIN(2);
		CSR_WRITE_2(sc, XL_W2_RESET_OPTIONS, CSR_READ_2(sc,
		    XL_W2_RESET_OPTIONS) 
		    | ((sc->xl_flags & XL_FLAG_INVERT_LED_PWR)?XL_RESETOPT_INVERT_LED:0)
		    | ((sc->xl_flags & XL_FLAG_INVERT_MII_PWR)?XL_RESETOPT_INVERT_MII:0)
		    );
	}

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(100000);
        return;
}

/*
 * This routine is a kludge to work around possible hardware faults
 * or manufacturing defects that can cause the media options register
 * (or reset options register, as it's called for the first generation
 * 3c90x adapters) to return an incorrect result. I have encountered
 * one Dell Latitude laptop docking station with an integrated 3c905-TX
 * which doesn't have any of the 'mediaopt' bits set. This screws up
 * the attach routine pretty badly because it doesn't know what media
 * to look for. If we find ourselves in this predicament, this routine
 * will try to guess the media options values and warn the user of a
 * possible manufacturing defect with his adapter/system/whatever.
 */
void
xl_mediacheck(sc)
	struct xl_softc		*sc;
{
	/*
	 * If some of the media options bits are set, assume they are
	 * correct. If not, try to figure it out down below.
	 * XXX I should check for 10baseFL, but I don't have an adapter
	 * to test with.
	 */
	if (sc->xl_media & (XL_MEDIAOPT_MASK & ~XL_MEDIAOPT_VCO)) {
		/*
	 	 * Check the XCVR value. If it's not in the normal range
	 	 * of values, we need to fake it up here.
	 	 */
		if (sc->xl_xcvr <= XL_XCVR_AUTO)
			return;
		else {
			printf("%s: bogus xcvr value "
			"in EEPROM (%x)\n", sc->sc_dev.dv_xname, sc->xl_xcvr);
			printf("%s: choosing new default based "
				"on card type\n", sc->sc_dev.dv_xname);
		}
	} else {
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media & XL_MEDIAOPT_10FL)
			return;
		printf("%s: WARNING: no media options bits set in "
			"the media options register!!\n", sc->sc_dev.dv_xname);
		printf("%s: this could be a manufacturing defect in "
			"your adapter or system\n", sc->sc_dev.dv_xname);
		printf("%s: attempting to guess media type; you "
			"should probably consult your vendor\n", sc->sc_dev.dv_xname);
	}

	xl_choose_xcvr(sc, 1);
}

void
xl_choose_xcvr(sc, verbose)
	struct xl_softc *sc;
	int verbose;
{
	u_int16_t devid;

	/*
	 * Read the device ID from the EEPROM.
	 * This is what's loaded into the PCI device ID register, so it has
	 * to be correct otherwise we wouldn't have gotten this far.
	 */
	xl_read_eeprom(sc, (caddr_t)&devid, XL_EE_PRODID, 1, 0);

	switch(devid) {
	case TC_DEVICEID_BOOMERANG_10BT:	/* 3c900-TPO */
	case TC_DEVICEID_KRAKATOA_10BT:		/* 3c900B-TPO */
		sc->xl_media = XL_MEDIAOPT_BT;
		sc->xl_xcvr = XL_XCVR_10BT;
		if (verbose)
			printf("%s: guessing 10BaseT transceiver\n",
			    sc->sc_dev.dv_xname);
		break;
	case TC_DEVICEID_BOOMERANG_10BT_COMBO:	/* 3c900-COMBO */
	case TC_DEVICEID_KRAKATOA_10BT_COMBO:	/* 3c900B-COMBO */
		sc->xl_media = XL_MEDIAOPT_BT|XL_MEDIAOPT_BNC|XL_MEDIAOPT_AUI;
		sc->xl_xcvr = XL_XCVR_10BT;
		if (verbose)
			printf("%s: guessing COMBO (AUI/BNC/TP)\n",
			    sc->sc_dev.dv_xname);
		break;
	case TC_DEVICEID_KRAKATOA_10BT_TPC:	/* 3c900B-TPC */
		sc->xl_media = XL_MEDIAOPT_BT|XL_MEDIAOPT_BNC;
		sc->xl_xcvr = XL_XCVR_10BT;
		if (verbose)
			printf("%s: guessing TPC (BNC/TP)\n", sc->sc_dev.dv_xname);
		break;
	case TC_DEVICEID_CYCLONE_10FL:		/* 3c900B-FL */
		sc->xl_media = XL_MEDIAOPT_10FL;
		sc->xl_xcvr = XL_XCVR_AUI;
		if (verbose)
			printf("%s: guessing 10baseFL\n", sc->sc_dev.dv_xname);
		break;
	case TC_DEVICEID_BOOMERANG_10_100BT:	/* 3c905-TX */
	case TC_DEVICEID_HURRICANE_555:		/* 3c555 */
	case TC_DEVICEID_HURRICANE_556:		/* 3c556 */
	case TC_DEVICEID_HURRICANE_556B:	/* 3c556B */
	case TC_DEVICEID_HURRICANE_575A:	/* 3c575TX */
	case TC_DEVICEID_HURRICANE_575B:	/* 3c575B */
	case TC_DEVICEID_HURRICANE_575C:	/* 3c575C */
	case TC_DEVICEID_HURRICANE_656:		/* 3c656 */
	case TC_DEVICEID_HURRICANE_656B:	/* 3c656B */
	case TC_DEVICEID_TORNADO_656C:		/* 3c656C */
	case TC_DEVICEID_TORNADO_10_100BT_920B: /* 3c920B-EMB */
		sc->xl_media = XL_MEDIAOPT_MII;
		sc->xl_xcvr = XL_XCVR_MII;
		if (verbose)
			printf("%s: guessing MII\n", sc->sc_dev.dv_xname);
		break;
	case TC_DEVICEID_BOOMERANG_100BT4:	/* 3c905-T4 */
	case TC_DEVICEID_CYCLONE_10_100BT4:	/* 3c905B-T4 */
		sc->xl_media = XL_MEDIAOPT_BT4;
		sc->xl_xcvr = XL_XCVR_MII;
		if (verbose)
			printf("%s: guessing 100BaseT4/MII\n", sc->sc_dev.dv_xname);
		break;
	case TC_DEVICEID_HURRICANE_10_100BT:	/* 3c905B-TX */
	case TC_DEVICEID_HURRICANE_10_100BT_SERV:/* 3c980-TX */
	case TC_DEVICEID_TORNADO_10_100BT_SERV:	/* 3c980C-TX */
	case TC_DEVICEID_HURRICANE_SOHO100TX:	/* 3cSOHO100-TX */
	case TC_DEVICEID_TORNADO_10_100BT:	/* 3c905C-TX */
	case TC_DEVICEID_TORNADO_HOMECONNECT:	/* 3c450-TX */
		sc->xl_media = XL_MEDIAOPT_BTX;
		sc->xl_xcvr = XL_XCVR_AUTO;
		if (verbose)
			printf("%s: guessing 10/100 internal\n",
			    sc->sc_dev.dv_xname);
		break;
	case TC_DEVICEID_CYCLONE_10_100_COMBO:	/* 3c905B-COMBO */
		sc->xl_media = XL_MEDIAOPT_BTX|XL_MEDIAOPT_BNC|XL_MEDIAOPT_AUI;
		sc->xl_xcvr = XL_XCVR_AUTO;
		if (verbose)
			printf("%s: guessing 10/100 plus BNC/AUI\n",
			    sc->sc_dev.dv_xname);
		break;
	default:
		printf("%s: unknown device ID: %x -- "
			"defaulting to 10baseT\n", sc->sc_dev.dv_xname, devid);
		sc->xl_media = XL_MEDIAOPT_BT;
		break;
	}

	return;
}

/*
 * Initialize the transmit descriptors.
 */
int
xl_list_tx_init(sc)
	struct xl_softc		*sc;
{
	struct xl_chain_data	*cd;
	struct xl_list_data	*ld;
	int			i;

	cd = &sc->xl_cdata;
	ld = sc->xl_ldata;
	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		cd->xl_tx_chain[i].xl_ptr = &ld->xl_tx_list[i];
		if (i == (XL_TX_LIST_CNT - 1))
			cd->xl_tx_chain[i].xl_next = NULL;
		else
			cd->xl_tx_chain[i].xl_next = &cd->xl_tx_chain[i + 1];
	}

	cd->xl_tx_free = &cd->xl_tx_chain[0];
	cd->xl_tx_tail = cd->xl_tx_head = NULL;

	return(0);
}

/*
 * Initialize the transmit descriptors.
 */
int
xl_list_tx_init_90xB(sc)
	struct xl_softc *sc;
{
	struct xl_chain_data *cd;
	struct xl_list_data *ld;
	int i;

	cd = &sc->xl_cdata;
	ld = sc->xl_ldata;
	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		cd->xl_tx_chain[i].xl_ptr = &ld->xl_tx_list[i];
		cd->xl_tx_chain[i].xl_phys =
		    sc->sc_listmap->dm_segs[0].ds_addr +   
		    offsetof(struct xl_list_data, xl_tx_list[i]);
		if (i == (XL_TX_LIST_CNT - 1))
			cd->xl_tx_chain[i].xl_next = &cd->xl_tx_chain[0];
		else
			cd->xl_tx_chain[i].xl_next = &cd->xl_tx_chain[i + 1];
		if (i == 0)
			cd->xl_tx_chain[i].xl_prev =
			    &cd->xl_tx_chain[XL_TX_LIST_CNT - 1];
		else
			cd->xl_tx_chain[i].xl_prev =
			    &cd->xl_tx_chain[i - 1];
	}

	bzero((char *)ld->xl_tx_list, sizeof(struct xl_list) * XL_TX_LIST_CNT);
	ld->xl_tx_list[0].xl_status = htole32(XL_TXSTAT_EMPTY);

	cd->xl_tx_prod = 1;
	cd->xl_tx_cons = 1;
	cd->xl_tx_cnt = 0;

	return (0);
}

/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
int
xl_list_rx_init(sc)
	struct xl_softc		*sc;
{
	struct xl_chain_data	*cd;
	struct xl_list_data	*ld;
	int			i;
	bus_addr_t		next;

	cd = &sc->xl_cdata;
	ld = sc->xl_ldata;

	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		cd->xl_rx_chain[i].xl_ptr =
			(struct xl_list_onefrag *)&ld->xl_rx_list[i];
		if (xl_newbuf(sc, &cd->xl_rx_chain[i]) == ENOBUFS)
			return(ENOBUFS);
		next = sc->sc_listmap->dm_segs[0].ds_addr;
		if (i == (XL_RX_LIST_CNT - 1)) {
			cd->xl_rx_chain[i].xl_next = &cd->xl_rx_chain[0];
			next +=
			    offsetof(struct xl_list_data, xl_rx_list[0]);
		} else {
			cd->xl_rx_chain[i].xl_next = &cd->xl_rx_chain[i + 1];
			next +=
			    offsetof(struct xl_list_data, xl_rx_list[i + 1]);
		}
		ld->xl_rx_list[i].xl_next = htole32(next);
	}

	cd->xl_rx_head = &cd->xl_rx_chain[0];

	return(0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
int
xl_newbuf(sc, c)
	struct xl_softc		*sc;
	struct xl_chain_onefrag	*c;
{
	struct mbuf		*m_new = NULL;
	bus_dmamap_t		map;

	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL)
		return(ENOBUFS);

	MCLGET(m_new, M_DONTWAIT);
	if (!(m_new->m_flags & M_EXT)) {
		m_freem(m_new);
		return(ENOBUFS);
	}

	m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_rx_sparemap,
	    mtod(m_new, caddr_t), MCLBYTES, NULL, BUS_DMA_NOWAIT) != 0) {
		printf("%s: rx load failed\n", sc->sc_dev.dv_xname);
		m_freem(m_new);
		return (ENOBUFS);
	}

	/* sync the old map, and unload it (if necessary) */
	if (c->map->dm_nsegs != 0) {
		bus_dmamap_sync(sc->sc_dmat, c->map,
		    0, c->map->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(sc->sc_dmat, c->map);
	}

	map = c->map;
	c->map = sc->sc_rx_sparemap;
	sc->sc_rx_sparemap = map;

	/* Force longword alignment for packet payload. */
	m_adj(m_new, ETHER_ALIGN);

	bus_dmamap_sync(sc->sc_dmat, c->map, 0, c->map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	c->xl_mbuf = m_new;
	c->xl_ptr->xl_frag.xl_addr =
	    htole32(c->map->dm_segs[0].ds_addr + ETHER_ALIGN);
	c->xl_ptr->xl_frag.xl_len =
	    htole32(c->map->dm_segs[0].ds_len | XL_LAST_FRAG);
	c->xl_ptr->xl_status = htole32(0);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    ((caddr_t)c->xl_ptr - sc->sc_listkva), sizeof(struct xl_list),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return(0);
}

int
xl_rx_resync(sc)
	struct xl_softc *sc;
{
	struct xl_chain_onefrag *pos;
	int i;

	pos = sc->xl_cdata.xl_rx_head;

	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
		    ((caddr_t)pos->xl_ptr - sc->sc_listkva),
		    sizeof(struct xl_list),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		if (pos->xl_ptr->xl_status)
			break;
		pos = pos->xl_next;
	}

	if (i == XL_RX_LIST_CNT)
		return(0);

	sc->xl_cdata.xl_rx_head = pos;

	return(EAGAIN);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
void
xl_rxeof(sc)
	struct xl_softc		*sc;
{
        struct mbuf		*m;
        struct ifnet		*ifp;
	struct xl_chain_onefrag	*cur_rx;
	int			total_len = 0, sumflags = 0;
	u_int32_t		rxstat;

	ifp = &sc->sc_arpcom.ac_if;

again:

	while ((rxstat = letoh32(sc->xl_cdata.xl_rx_head->xl_ptr->xl_status))
	    != 0) {
		cur_rx = sc->xl_cdata.xl_rx_head;
		sc->xl_cdata.xl_rx_head = cur_rx->xl_next;
		total_len = rxstat & XL_RXSTAT_LENMASK;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
		    ((caddr_t)cur_rx->xl_ptr - sc->sc_listkva),
		    sizeof(struct xl_list),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		/*
		 * Since we have told the chip to allow large frames,
		 * we need to trap giant frame errors in software. We allow
		 * a little more than the normal frame size to account for
		 * frames with VLAN tags.
		 */
		if (total_len > XL_MAX_FRAMELEN)
			rxstat |= (XL_RXSTAT_UP_ERROR|XL_RXSTAT_OVERSIZE);

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & XL_RXSTAT_UP_ERROR) {
			ifp->if_ierrors++;
			cur_rx->xl_ptr->xl_status = htole32(0);
			continue;
		}

		/*
		 * If the error bit was not set, the upload complete
		 * bit should be set which means we have a valid packet.
		 * If not, something truly strange has happened.
		 */
		if (!(rxstat & XL_RXSTAT_UP_CMPLT)) {
			printf("%s: bad receive status -- "
			    "packet dropped\n", sc->sc_dev.dv_xname);
			ifp->if_ierrors++;
			cur_rx->xl_ptr->xl_status = htole32(0);
			continue;
		}

		/* No errors; receive the packet. */	
		m = cur_rx->xl_mbuf;

		/*
		 * Try to conjure up a new mbuf cluster. If that
		 * fails, it means we have an out of memory condition and
		 * should leave the buffer in place and continue. This will
		 * result in a lost packet, but there's little else we
		 * can do in this situation.
		 */
		if (xl_newbuf(sc, cur_rx) == ENOBUFS) {
			ifp->if_ierrors++;
			cur_rx->xl_ptr->xl_status = htole32(0);
			continue;
		}

		ifp->if_ipackets++;
		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = total_len;
#if NBPFILTER > 0
		/*
		 * Handle BPF listeners. Let the BPF user see the packet.
		 */
		if (ifp->if_bpf) {
			bpf_mtap(ifp->if_bpf, m);
		}
#endif

		if (sc->xl_type == XL_TYPE_905B) {
			if (rxstat & XL_RXSTAT_IPCKERR)
				sumflags |= M_IPV4_CSUM_IN_BAD;
			else if (rxstat & XL_RXSTAT_IPCKOK)
				sumflags |= M_IPV4_CSUM_IN_OK;

			if (rxstat & XL_RXSTAT_TCPCKERR)
				sumflags |= M_TCP_CSUM_IN_BAD;
			else if (rxstat & XL_RXSTAT_TCPCKOK)
				sumflags |= M_TCP_CSUM_IN_OK;

			if (rxstat & XL_RXSTAT_UDPCKERR)
				sumflags |= M_UDP_CSUM_IN_BAD;
			else if (rxstat & XL_RXSTAT_UDPCKOK)
				sumflags |= M_UDP_CSUM_IN_OK;

			m->m_pkthdr.csum = sumflags;
		}
		ether_input_mbuf(ifp, m);
	}

	/*
	 * Handle the 'end of channel' condition. When the upload
	 * engine hits the end of the RX ring, it will stall. This
	 * is our cue to flush the RX ring, reload the uplist pointer
	 * register and unstall the engine.
	 * XXX This is actually a little goofy. With the ThunderLAN
	 * chip, you get an interrupt when the receiver hits the end
	 * of the receive ring, which tells you exactly when you
	 * you need to reload the ring pointer. Here we have to
	 * fake it. I'm mad at myself for not being clever enough
	 * to avoid the use of a goto here.
	 */
	if (CSR_READ_4(sc, XL_UPLIST_PTR) == 0 ||
		CSR_READ_4(sc, XL_UPLIST_STATUS) & XL_PKTSTAT_UP_STALLED) {
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_STALL);
		xl_wait(sc);
		CSR_WRITE_4(sc, XL_UPLIST_PTR,
		    sc->sc_listmap->dm_segs[0].ds_addr +
		    offsetof(struct xl_list_data, xl_rx_list[0]));
		sc->xl_cdata.xl_rx_head = &sc->xl_cdata.xl_rx_chain[0];
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_UNSTALL);
		goto again;
	}

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
void
xl_txeof(sc)
	struct xl_softc		*sc;
{
	struct xl_chain		*cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->sc_arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been uploaded. Note: the 3c905B
	 * sets a special bit in the status word to let us
	 * know that a frame has been downloaded, but the
	 * original 3c900/3c905 adapters don't do that.
	 * Consequently, we have to use a different test if
	 * xl_type != XL_TYPE_905B.
	 */
	while(sc->xl_cdata.xl_tx_head != NULL) {
		cur_tx = sc->xl_cdata.xl_tx_head;

		bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
		    ((caddr_t)cur_tx->xl_ptr - sc->sc_listkva),
		    sizeof(struct xl_list),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		if (CSR_READ_4(sc, XL_DOWNLIST_PTR))
			break;

		sc->xl_cdata.xl_tx_head = cur_tx->xl_next;
		ifp->if_opackets++;
		if (cur_tx->map->dm_nsegs != 0) {
			bus_dmamap_t map = cur_tx->map;

			bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, map);
		}
		if (cur_tx->xl_mbuf != NULL) {
			m_freem(cur_tx->xl_mbuf);
			cur_tx->xl_mbuf = NULL;
		}
		cur_tx->xl_next = sc->xl_cdata.xl_tx_free;
		sc->xl_cdata.xl_tx_free = cur_tx;
	}

	if (sc->xl_cdata.xl_tx_head == NULL) {
		ifp->if_flags &= ~IFF_OACTIVE;
		sc->xl_cdata.xl_tx_tail = NULL;
	} else {
		if (CSR_READ_4(sc, XL_DMACTL) & XL_DMACTL_DOWN_STALLED ||
			!CSR_READ_4(sc, XL_DOWNLIST_PTR)) {
			CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
			    sc->sc_listmap->dm_segs[0].ds_addr +
			    ((caddr_t)sc->xl_cdata.xl_tx_head->xl_ptr -
			    sc->sc_listkva));
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		}
	}

	return;
}

void
xl_txeof_90xB(sc)
	struct xl_softc *sc;
{
	struct xl_chain *cur_tx = NULL;
	struct ifnet *ifp;
	int idx;

	ifp = &sc->sc_arpcom.ac_if;

	idx = sc->xl_cdata.xl_tx_cons;
	while(idx != sc->xl_cdata.xl_tx_prod) {

		cur_tx = &sc->xl_cdata.xl_tx_chain[idx];

		if ((cur_tx->xl_ptr->xl_status &
		    htole32(XL_TXSTAT_DL_COMPLETE)) == 0)
			break;

		if (cur_tx->xl_mbuf != NULL) {
			m_freem(cur_tx->xl_mbuf);
			cur_tx->xl_mbuf = NULL;
		}

		if (cur_tx->map->dm_nsegs != 0) {
			bus_dmamap_sync(sc->sc_dmat, cur_tx->map,
			    0, cur_tx->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, cur_tx->map);
		}

		ifp->if_opackets++;

		sc->xl_cdata.xl_tx_cnt--;
		XL_INC(idx, XL_TX_LIST_CNT);
		ifp->if_timer = 0;
	}

	sc->xl_cdata.xl_tx_cons = idx;

	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;

	return;
}

/*
 * TX 'end of channel' interrupt handler. Actually, we should
 * only get a 'TX complete' interrupt if there's a transmit error,
 * so this is really TX error handler.
 */
void
xl_txeoc(sc)
	struct xl_softc		*sc;
{
	u_int8_t		txstat;

	while((txstat = CSR_READ_1(sc, XL_TX_STATUS))) {
		if (txstat & XL_TXSTATUS_UNDERRUN ||
			txstat & XL_TXSTATUS_JABBER ||
			txstat & XL_TXSTATUS_RECLAIM) {
			if (txstat != 0x90) {
				printf("%s: transmission error: %x\n",
				    sc->sc_dev.dv_xname, txstat);
			}
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
			xl_wait(sc);
			if (sc->xl_type == XL_TYPE_905B) {
				int i;
				struct xl_chain *c;
				i = sc->xl_cdata.xl_tx_cons;
				c = &sc->xl_cdata.xl_tx_chain[i];
				CSR_WRITE_4(sc, XL_DOWNLIST_PTR, c->xl_phys);
				CSR_WRITE_1(sc, XL_DOWN_POLL, 64);
			} else {
				if (sc->xl_cdata.xl_tx_head != NULL)
					CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
					    sc->sc_listmap->dm_segs[0].ds_addr +
					    ((caddr_t)sc->xl_cdata.xl_tx_head->xl_ptr -
					    sc->sc_listkva));
			}
			/*
			 * Remember to set this for the
			 * first generation 3c90X chips.
			 */
			CSR_WRITE_1(sc, XL_TX_FREETHRESH, XL_PACKET_SIZE >> 8);
			if (txstat & XL_TXSTATUS_UNDERRUN &&
			    sc->xl_tx_thresh < XL_PACKET_SIZE) {
				sc->xl_tx_thresh += XL_MIN_FRAMELEN;
#ifdef notdef
				printf("%s: tx underrun, increasing tx start"
				    " threshold to %d\n", sc->sc_dev.dv_xname,
				    sc->xl_tx_thresh);
#endif
			}
			CSR_WRITE_2(sc, XL_COMMAND,
			    XL_CMD_TX_SET_START|sc->xl_tx_thresh);
			if (sc->xl_type == XL_TYPE_905B) {
				CSR_WRITE_2(sc, XL_COMMAND,
				XL_CMD_SET_TX_RECLAIM|(XL_PACKET_SIZE >> 4));
			}
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_ENABLE);
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		} else {
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_ENABLE);
			CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		}
		/*
		 * Write an arbitrary byte to the TX_STATUS register
	 	 * to clear this interrupt/error and advance to the next.
		 */
		CSR_WRITE_1(sc, XL_TX_STATUS, 0x01);
	}

	return;
}

int
xl_intr(arg)
	void			*arg;
{
	struct xl_softc		*sc;
	struct ifnet		*ifp;
	u_int16_t		status;
	int claimed = 0;

	sc = arg;
	ifp = &sc->sc_arpcom.ac_if;

	while ((status = CSR_READ_2(sc, XL_STATUS)) & XL_INTRS && status != 0xFFFF) {

		claimed = 1;

		CSR_WRITE_2(sc, XL_COMMAND,
		    XL_CMD_INTR_ACK|(status & XL_INTRS));

		if (sc->intr_ack)
			(*sc->intr_ack)(sc);

		if (status & XL_STAT_UP_COMPLETE) {
			int curpkts;

			curpkts = ifp->if_ipackets;
			xl_rxeof(sc);
			if (curpkts == ifp->if_ipackets) {
				while (xl_rx_resync(sc))
					xl_rxeof(sc);
			}
		}

		if (status & XL_STAT_DOWN_COMPLETE) {
			if (sc->xl_type == XL_TYPE_905B)
				xl_txeof_90xB(sc);
			else
				xl_txeof(sc);
		}

		if (status & XL_STAT_TX_COMPLETE) {
			ifp->if_oerrors++;
			xl_txeoc(sc);
		}

		if (status & XL_STAT_ADFAIL) {
			xl_reset(sc);
			xl_init(sc);
		}

		if (status & XL_STAT_STATSOFLOW) {
			sc->xl_stats_no_timeout = 1;
			xl_stats_update(sc);
			sc->xl_stats_no_timeout = 0;
		}
	}

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		(*ifp->if_start)(ifp);

	return (claimed);
}

void
xl_stats_update(xsc)
	void			*xsc;
{
	struct xl_softc		*sc;
	struct ifnet		*ifp;
	struct xl_stats		xl_stats;
	u_int8_t		*p;
	int			i;
	struct mii_data		*mii = NULL;

	bzero((char *)&xl_stats, sizeof(struct xl_stats));

	sc = xsc;
	ifp = &sc->sc_arpcom.ac_if;
	if (sc->xl_hasmii)
		mii = &sc->sc_mii;

	p = (u_int8_t *)&xl_stats;

	/* Read all the stats registers. */
	XL_SEL_WIN(6);

	for (i = 0; i < 16; i++)
		*p++ = CSR_READ_1(sc, XL_W6_CARRIER_LOST + i);

	ifp->if_ierrors += xl_stats.xl_rx_overrun;

	ifp->if_collisions += xl_stats.xl_tx_multi_collision +
				xl_stats.xl_tx_single_collision +
				xl_stats.xl_tx_late_collision;

	/*
	 * Boomerang and cyclone chips have an extra stats counter
	 * in window 4 (BadSSD). We have to read this too in order
	 * to clear out all the stats registers and avoid a statsoflow
	 * interrupt.
	 */
	XL_SEL_WIN(4);
	CSR_READ_1(sc, XL_W4_BADSSD);

	if (mii != NULL && (!sc->xl_stats_no_timeout))
		mii_tick(mii);

	XL_SEL_WIN(7);

	if (!sc->xl_stats_no_timeout)
		timeout_add(&sc->xl_stsup_tmo, hz);

	return;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
int
xl_encap(sc, c, m_head)
	struct xl_softc		*sc;
	struct xl_chain		*c;
	struct mbuf		*m_head;
{
	int			frag, total_len;
	bus_dmamap_t		map;

	map = sc->sc_tx_sparemap;

reload:
	if (bus_dmamap_load_mbuf(sc->sc_dmat, map,
	    m_head, BUS_DMA_NOWAIT) != 0)
		return (ENOBUFS);

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	for (frag = 0, total_len = 0; frag < map->dm_nsegs; frag++) {
		if ((XL_TX_LIST_CNT - (sc->xl_cdata.xl_tx_cnt + frag)) < 3)
			return (ENOBUFS);
		if (frag == XL_MAXFRAGS)
			break;
		total_len += map->dm_segs[frag].ds_len;
		c->xl_ptr->xl_frag[frag].xl_addr =
		    htole32(map->dm_segs[frag].ds_addr);
		c->xl_ptr->xl_frag[frag].xl_len =
		    htole32(map->dm_segs[frag].ds_len);
	}

	/*
	 * Handle special case: we used up all 63 fragments,
	 * but we have more mbufs left in the chain. Copy the
	 * data into an mbuf cluster. Note that we don't
	 * bother clearing the values in the other fragment
	 * pointers/counters; it wouldn't gain us anything,
	 * and would waste cycles.
	 */
	if (frag != map->dm_nsegs) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			m_freem(m_head);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				m_freem(m_head);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
		    mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
		goto reload;
	}

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	if (c->map->dm_nsegs != 0) {
		bus_dmamap_sync(sc->sc_dmat, c->map,
		    0, c->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, c->map);
	}

	c->xl_mbuf = m_head;
	sc->sc_tx_sparemap = c->map;
	c->map = map;
	c->xl_ptr->xl_frag[frag - 1].xl_len |= htole32(XL_LAST_FRAG);
	c->xl_ptr->xl_status = htole32(total_len);
	c->xl_ptr->xl_next = 0;

	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    offsetof(struct xl_list_data, xl_tx_list[0]),
	    sizeof(struct xl_list) * XL_TX_LIST_CNT,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return(0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */
void
xl_start(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct xl_chain		*prev = NULL, *cur_tx = NULL, *start_tx;
	struct xl_chain		*prev_tx;
	int			error;

	sc = ifp->if_softc;

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->xl_cdata.xl_tx_free == NULL) {
		xl_txeoc(sc);
		xl_txeof(sc);
		if (sc->xl_cdata.xl_tx_free == NULL) {
			ifp->if_flags |= IFF_OACTIVE;
			return;
		}
	}

	start_tx = sc->xl_cdata.xl_tx_free;

	while(sc->xl_cdata.xl_tx_free != NULL) {
		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		/* Pick a descriptor off the free list. */
		prev_tx = cur_tx;
		cur_tx = sc->xl_cdata.xl_tx_free;

		/* Pack the data into the descriptor. */
		error = xl_encap(sc, cur_tx, m_head);
		if (error) {
			cur_tx = prev_tx;
			continue;
		}

		sc->xl_cdata.xl_tx_free = cur_tx->xl_next;
		cur_tx->xl_next = NULL;

		/* Chain it together. */
		if (prev != NULL) {
			prev->xl_next = cur_tx;
			prev->xl_ptr->xl_next =
			    sc->sc_listmap->dm_segs[0].ds_addr +
			    ((caddr_t)cur_tx->xl_ptr - sc->sc_listkva);

		}
		prev = cur_tx;

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, cur_tx->xl_mbuf);
#endif
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	/*
	 * Place the request for the upload interrupt
	 * in the last descriptor in the chain. This way, if
	 * we're chaining several packets at once, we'll only
	 * get an interrupt once for the whole chain rather than
	 * once for each packet.
	 */
	cur_tx->xl_ptr->xl_status |= htole32(XL_TXSTAT_DL_INTR);

	/*
	 * Queue the packets. If the TX channel is clear, update
	 * the downlist pointer register.
	 */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_STALL);
	xl_wait(sc);

	if (sc->xl_cdata.xl_tx_head != NULL) {
		sc->xl_cdata.xl_tx_tail->xl_next = start_tx;
		sc->xl_cdata.xl_tx_tail->xl_ptr->xl_next =
		    sc->sc_listmap->dm_segs[0].ds_addr +
		    ((caddr_t)start_tx->xl_ptr - sc->sc_listkva);
		sc->xl_cdata.xl_tx_tail->xl_ptr->xl_status &=
		    htole32(~XL_TXSTAT_DL_INTR);
		sc->xl_cdata.xl_tx_tail = cur_tx;
	} else {
		sc->xl_cdata.xl_tx_head = start_tx;
		sc->xl_cdata.xl_tx_tail = cur_tx;
	}
	if (!CSR_READ_4(sc, XL_DOWNLIST_PTR))
		CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
		    sc->sc_listmap->dm_segs[0].ds_addr +
		    ((caddr_t)start_tx->xl_ptr - sc->sc_listkva));

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);

	XL_SEL_WIN(7);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	/*
	 * XXX Under certain conditions, usually on slower machines
	 * where interrupts may be dropped, it's possible for the
	 * adapter to chew up all the buffers in the receive ring
	 * and stall, without us being able to do anything about it.
	 * To guard against this, we need to make a pass over the
	 * RX queue to make sure there aren't any packets pending.
	 * Doing it here means we can flush the receive ring at the
	 * same time the chip is DMAing the transmit descriptors we
	 * just gave it.
 	 *
	 * 3Com goes to some lengths to emphasize the Parallel Tasking (tm)
	 * nature of their chips in all their marketing literature;
	 * we may as well take advantage of it. :)
	 */
	xl_rxeof(sc);

	return;
}

int
xl_encap_90xB(sc, c, m_head)
	struct xl_softc *sc;
	struct xl_chain *c;
	struct mbuf *m_head;
{
	struct xl_frag *f = NULL;
	struct xl_list *d;
	int frag;
	bus_dmamap_t map;

	/*
	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
	 * of fragments or hit the end of the mbuf chain.
	 */
	map = sc->sc_tx_sparemap;
	d = c->xl_ptr;
	d->xl_status = htole32(0);
	d->xl_next = 0;

	if (bus_dmamap_load_mbuf(sc->sc_dmat, map,
	    m_head, BUS_DMA_NOWAIT) != 0)
		return (ENOBUFS);

	for (frag = 0; frag < map->dm_nsegs; frag++) {
		if (frag == XL_MAXFRAGS)
			break;
		f = &d->xl_frag[frag];
		f->xl_addr = htole32(map->dm_segs[frag].ds_addr);
		f->xl_len = htole32(map->dm_segs[frag].ds_len);
	}

	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	/* sync the old map, and unload it (if necessary) */
	if (c->map->dm_nsegs != 0) {
		bus_dmamap_sync(sc->sc_dmat, c->map, 0, c->map->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, c->map);
	}

	c->xl_mbuf = m_head;
	sc->sc_tx_sparemap = c->map;
	c->map = map;
	c->xl_ptr->xl_frag[frag - 1].xl_len |= htole32(XL_LAST_FRAG);
	c->xl_ptr->xl_status = htole32(XL_TXSTAT_RND_DEFEAT);

#ifndef XL905B_TXCSUM_BROKEN
	if (m_head->m_pkthdr.csum & M_IPV4_CSUM_OUT)
		c->xl_ptr->xl_status |= htole32(XL_TXSTAT_IPCKSUM);
	if (m_head->m_pkthdr.csum & M_TCPV4_CSUM_OUT)
		c->xl_ptr->xl_status |= htole32(XL_TXSTAT_TCPCKSUM);
	if (m_head->m_pkthdr.csum & M_UDPV4_CSUM_OUT)
		c->xl_ptr->xl_status |= htole32(XL_TXSTAT_UDPCKSUM);
#endif

	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    offsetof(struct xl_list_data, xl_tx_list[0]),
	    sizeof(struct xl_list) * XL_TX_LIST_CNT,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return(0);
}

void
xl_start_90xB(ifp)
	struct ifnet *ifp;
{
	struct xl_softc *sc;
	struct mbuf *m_head = NULL;
	struct xl_chain *prev = NULL, *cur_tx = NULL, *start_tx;
	struct xl_chain		*prev_tx;
	int error, idx;

	sc = ifp->if_softc;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	idx = sc->xl_cdata.xl_tx_prod;
	start_tx = &sc->xl_cdata.xl_tx_chain[idx];

	while (sc->xl_cdata.xl_tx_chain[idx].xl_mbuf == NULL) {

		if ((XL_TX_LIST_CNT - sc->xl_cdata.xl_tx_cnt) < 3) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		IFQ_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		prev_tx = cur_tx;
		cur_tx = &sc->xl_cdata.xl_tx_chain[idx];

		/* Pack the data into the descriptor. */
		error = xl_encap_90xB(sc, cur_tx, m_head);
		if (error) {
			cur_tx = prev_tx;
			continue;
		}

		/* Chain it together. */
		if (prev != NULL)
			prev->xl_ptr->xl_next = htole32(cur_tx->xl_phys);
		prev = cur_tx;

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, cur_tx->xl_mbuf);
#endif

		XL_INC(idx, XL_TX_LIST_CNT);
		sc->xl_cdata.xl_tx_cnt++;
	}

	/*
	 * If there are no packets queued, bail.
	 */
	if (cur_tx == NULL)
		return;

	/*
	 * Place the request for the upload interrupt
	 * in the last descriptor in the chain. This way, if
	 * we're chaining several packets at once, we'll only
	 * get an interrupt once for the whole chain rather than
	 * once for each packet.
	 */
	cur_tx->xl_ptr->xl_status |= htole32(XL_TXSTAT_DL_INTR);

	/* Start transmission */
	sc->xl_cdata.xl_tx_prod = idx;
	start_tx->xl_prev->xl_ptr->xl_next = htole32(start_tx->xl_phys);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

void
xl_init(xsc)
	void			*xsc;
{
	struct xl_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->sc_arpcom.ac_if;
	int			s, i;
	u_int16_t		rxfilt = 0;
	struct mii_data		*mii = NULL;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	xl_stop(sc);

	if (sc->xl_hasmii)
		mii = &sc->sc_mii;

	if (mii == NULL) {
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
		xl_wait(sc);
	}
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);
	DELAY(10000);

	/* Init our MAC address */
	XL_SEL_WIN(2);
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, XL_W2_STATION_ADDR_LO + i,
				sc->sc_arpcom.ac_enaddr[i]);
	}

	/* Clear the station mask. */
	for (i = 0; i < 3; i++)
		CSR_WRITE_2(sc, XL_W2_STATION_MASK_LO + (i * 2), 0);
#ifdef notdef
	/* Reset TX and RX. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);
#endif
	/* Init circular RX list. */
	if (xl_list_rx_init(sc) == ENOBUFS) {
		printf("%s: initialization failed: no "
			"memory for rx buffers\n", sc->sc_dev.dv_xname);
		xl_stop(sc);
		splx(s);
		return;
	}

	/* Init TX descriptors. */
	if (sc->xl_type == XL_TYPE_905B)
		xl_list_tx_init_90xB(sc);
	else
		xl_list_tx_init(sc);

	/*
	 * Set the TX freethresh value.
	 * Note that this has no effect on 3c905B "cyclone"
	 * cards but is required for 3c900/3c905 "boomerang"
	 * cards in order to enable the download engine.
	 */
	CSR_WRITE_1(sc, XL_TX_FREETHRESH, XL_PACKET_SIZE >> 8);

	/* Set the TX start threshold for best performance. */
	sc->xl_tx_thresh = XL_MIN_FRAMELEN;
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_SET_START|sc->xl_tx_thresh);

	/*
	 * If this is a 3c905B, also set the tx reclaim threshold.
	 * This helps cut down on the number of tx reclaim errors
	 * that could happen on a busy network. The chip multiplies
	 * the register value by 16 to obtain the actual threshold
	 * in bytes, so we divide by 16 when setting the value here.
	 * The existing threshold value can be examined by reading
	 * the register at offset 9 in window 5.
	 */
	if (sc->xl_type == XL_TYPE_905B) {
		CSR_WRITE_2(sc, XL_COMMAND,
		    XL_CMD_SET_TX_RECLAIM|(XL_PACKET_SIZE >> 4));
	}

	/* Set RX filter bits. */
	XL_SEL_WIN(5);
	rxfilt = CSR_READ_1(sc, XL_W5_RX_FILTER);

	/* Set the individual bit to receive frames for this host only. */
	rxfilt |= XL_RXFILTER_INDIVIDUAL;

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC) {
		rxfilt |= XL_RXFILTER_ALLFRAMES;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
	} else {
		rxfilt &= ~XL_RXFILTER_ALLFRAMES;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
	}

	/*
	 * Set capture broadcast bit to capture broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST) {
		rxfilt |= XL_RXFILTER_BROADCAST;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
	} else {
		rxfilt &= ~XL_RXFILTER_BROADCAST;
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_FILT|rxfilt);
	}

	/*
	 * Program the multicast filter, if necessary.
	 */
	if (sc->xl_type == XL_TYPE_905B)
		xl_setmulti_hash(sc);
	else
		xl_setmulti(sc);

	/*
	 * Load the address of the RX list. We have to
	 * stall the upload engine before we can manipulate
	 * the uplist pointer register, then unstall it when
	 * we're finished. We also have to wait for the
	 * stall command to complete before proceeding.
	 * Note that we have to do this after any RX resets
	 * have completed since the uplist register is cleared
	 * by a reset.
	 */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_STALL);
	xl_wait(sc);
	CSR_WRITE_4(sc, XL_UPLIST_PTR, sc->sc_listmap->dm_segs[0].ds_addr +
	    offsetof(struct xl_list_data, xl_rx_list[0]));
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_UP_UNSTALL);
	xl_wait(sc);

	if (sc->xl_type == XL_TYPE_905B) {
		/* Set polling interval */
		CSR_WRITE_1(sc, XL_DOWN_POLL, 64);
		/* Load the address of the TX list */
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_STALL);
		xl_wait(sc);
		CSR_WRITE_4(sc, XL_DOWNLIST_PTR,
		    sc->sc_listmap->dm_segs[0].ds_addr +
		    offsetof(struct xl_list_data, xl_tx_list[0]));
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_DOWN_UNSTALL);
		xl_wait(sc);
	}

	/*
	 * If the coax transceiver is on, make sure to enable
	 * the DC-DC converter.
 	 */
	XL_SEL_WIN(3);
	if (sc->xl_xcvr == XL_XCVR_COAX)
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_START);
	else
		CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);

	/*
	 * increase packet size to allow reception of 802.1q or ISL packets.
	 * For the 3c90x chip, set the 'allow large packets' bit in the MAC
	 * control register. For 3c90xB/C chips, use the RX packet size
	 * register.
	 */

	if (sc->xl_type == XL_TYPE_905B)
		CSR_WRITE_2(sc, XL_W3_MAXPKTSIZE, XL_PACKET_SIZE);
	else {
		u_int8_t macctl;
		macctl = CSR_READ_1(sc, XL_W3_MAC_CTRL);
		macctl |= XL_MACCTRL_ALLOW_LARGE_PACK;
		CSR_WRITE_1(sc, XL_W3_MAC_CTRL, macctl);
	}

	/* Clear out the stats counters. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STATS_DISABLE);
	sc->xl_stats_no_timeout = 1;
	xl_stats_update(sc);
	sc->xl_stats_no_timeout = 0;
	XL_SEL_WIN(4);
	CSR_WRITE_2(sc, XL_W4_NET_DIAG, XL_NETDIAG_UPPER_BYTES_ENABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STATS_ENABLE);

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ACK|0xFF);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STAT_ENB|XL_INTRS);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ENB|XL_INTRS);

	if (sc->intr_ack)
		(*sc->intr_ack)(sc);

	/* Set the RX early threshold */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_SET_THRESH|(XL_PACKET_SIZE >>2));
	CSR_WRITE_2(sc, XL_DMACTL, XL_DMACTL_UP_RX_EARLY);

	/* Enable receiver and transmitter. */
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_ENABLE);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_ENABLE);
	xl_wait(sc);

	/* Restore state of BMCR */
	if (mii != NULL)
		mii_mediachg(mii);

	/* Select window 7 for normal operations. */
	XL_SEL_WIN(7);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	timeout_add(&sc->xl_stsup_tmo, hz);

	return;
}

/*
 * Set media options.
 */
int
xl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	struct ifmedia		*ifm = NULL;
	struct mii_data		*mii = NULL;

	sc = ifp->if_softc;

	if (sc->xl_hasmii)
		mii = &sc->sc_mii;
	if (mii == NULL)
		ifm = &sc->ifmedia;
	else
		ifm = &mii->mii_media;

	switch(IFM_SUBTYPE(ifm->ifm_media)) {
	case IFM_100_FX:
	case IFM_10_FL:
	case IFM_10_2:
	case IFM_10_5:
		xl_setmode(sc, ifm->ifm_media);
		return(0);
		break;
	default:
		break;
	}

	if (sc->xl_media & XL_MEDIAOPT_MII || sc->xl_media & XL_MEDIAOPT_BTX
		|| sc->xl_media & XL_MEDIAOPT_BT4) {
		xl_init(sc);
	} else {
		xl_setmode(sc, ifm->ifm_media);
	}

	return(0);
}

/*
 * Report current media status.
 */
void
xl_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct xl_softc		*sc;
	u_int32_t		icfg;
	u_int16_t		status = 0;
	struct mii_data		*mii = NULL;

	sc = ifp->if_softc;
	if (sc->xl_hasmii != 0)
		mii = &sc->sc_mii;

	XL_SEL_WIN(4);
	status = CSR_READ_2(sc, XL_W4_MEDIA_STATUS);

	XL_SEL_WIN(3);
	icfg = CSR_READ_4(sc, XL_W3_INTERNAL_CFG) & XL_ICFG_CONNECTOR_MASK;
	icfg >>= XL_ICFG_CONNECTOR_BITS;

	ifmr->ifm_active = IFM_ETHER;
	ifmr->ifm_status = IFM_AVALID;

	if ((status & XL_MEDIASTAT_CARRIER) == 0)
		ifmr->ifm_status |= IFM_ACTIVE;

	switch(icfg) {
	case XL_XCVR_10BT:
		ifmr->ifm_active = IFM_ETHER|IFM_10_T;
		if (CSR_READ_1(sc, XL_W3_MAC_CTRL) & XL_MACCTRL_DUPLEX)
			ifmr->ifm_active |= IFM_FDX;
		else
			ifmr->ifm_active |= IFM_HDX;
		break;
	case XL_XCVR_AUI:
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media == XL_MEDIAOPT_10FL) {
			ifmr->ifm_active = IFM_ETHER|IFM_10_FL;
			if (CSR_READ_1(sc, XL_W3_MAC_CTRL) & XL_MACCTRL_DUPLEX)
				ifmr->ifm_active |= IFM_FDX;
			else
				ifmr->ifm_active |= IFM_FDX;
		} else
			ifmr->ifm_active = IFM_ETHER|IFM_10_5;
		break;
	case XL_XCVR_COAX:
		ifmr->ifm_active = IFM_ETHER|IFM_10_2;
		break;
	/*
	 * XXX MII and BTX/AUTO should be separate cases.
	 */

	case XL_XCVR_100BTX:
	case XL_XCVR_AUTO:
	case XL_XCVR_MII:
		if (mii != NULL) {
			mii_pollstat(mii);
			ifmr->ifm_active = mii->mii_media_active;
			ifmr->ifm_status = mii->mii_media_status;
		}
		break;
	case XL_XCVR_100BFX:
		ifmr->ifm_active = IFM_ETHER|IFM_100_FX;
		break;
	default:
		printf("%s: unknown XCVR type: %d\n", sc->sc_dev.dv_xname, icfg);
		break;
	}

	return;
}

int
xl_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct xl_softc *sc = ifp->if_softc;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;
	struct mii_data *mii = NULL;
	u_int8_t rxfilt;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			xl_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
#endif /* INET */
		default:
			xl_init(sc);
			break;
		}
		break;

	case SIOCSIFMTU:
		if(ifr->ifr_mtu > ETHERMTU || ifr->ifr_mtu < ETHERMIN) {
			error = EINVAL;
		} else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;

	case SIOCSIFFLAGS:
		XL_SEL_WIN(5);
		rxfilt = CSR_READ_1(sc, XL_W5_RX_FILTER);
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->xl_if_flags & IFF_PROMISC)) {
				rxfilt |= XL_RXFILTER_ALLFRAMES;
				CSR_WRITE_2(sc, XL_COMMAND,
				    XL_CMD_RX_SET_FILT|rxfilt);
				XL_SEL_WIN(7);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->xl_if_flags & IFF_PROMISC) {
				rxfilt &= ~XL_RXFILTER_ALLFRAMES;
				CSR_WRITE_2(sc, XL_COMMAND,
				    XL_CMD_RX_SET_FILT|rxfilt);
				XL_SEL_WIN(7);
			} else
				xl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				xl_stop(sc);
		}
		sc->xl_if_flags = ifp->if_flags;
		error = 0;
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
			if (ifp->if_flags & IFF_RUNNING) {
				if (sc->xl_type == XL_TYPE_905B)
					xl_setmulti_hash(sc);
				else
					xl_setmulti(sc);
			}
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		if (sc->xl_hasmii != 0)
			mii = &sc->sc_mii;
		if (mii == NULL)
			error = ifmedia_ioctl(ifp, ifr,
			    &sc->ifmedia, command);
		else
			error = ifmedia_ioctl(ifp, ifr,
			    &mii->mii_media, command);
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return(error);
}

void
xl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct xl_softc		*sc;
	u_int16_t		status = 0;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	XL_SEL_WIN(4);
	status = CSR_READ_2(sc, XL_W4_MEDIA_STATUS);
	printf("%s: watchdog timeout\n", sc->sc_dev.dv_xname);

	if (status & XL_MEDIASTAT_CARRIER)
		printf("%s: no carrier - transceiver cable problem?\n",
								sc->sc_dev.dv_xname);
	xl_txeoc(sc);
	xl_txeof(sc);
	xl_rxeof(sc);
	xl_reset(sc);
	xl_init(sc);

	if (!IFQ_IS_EMPTY(&ifp->if_snd))
		(*ifp->if_start)(ifp);

	return;
}

void
xl_freetxrx(sc)
	struct xl_softc *sc;
{
	int i;

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		if (sc->xl_cdata.xl_rx_chain[i].map->dm_nsegs != 0) {
			bus_dmamap_t map = sc->xl_cdata.xl_rx_chain[i].map;

			bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, map);
		}
		if (sc->xl_cdata.xl_rx_chain[i].xl_mbuf != NULL) {
			m_freem(sc->xl_cdata.xl_rx_chain[i].xl_mbuf);
			sc->xl_cdata.xl_rx_chain[i].xl_mbuf = NULL;
		}
	}
	bzero((char *)&sc->xl_ldata->xl_rx_list,
		sizeof(sc->xl_ldata->xl_rx_list));
	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		if (sc->xl_cdata.xl_tx_chain[i].map->dm_nsegs != 0) {
			bus_dmamap_t map = sc->xl_cdata.xl_tx_chain[i].map;

			bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, map);
		}
		if (sc->xl_cdata.xl_tx_chain[i].xl_mbuf != NULL) {
			m_freem(sc->xl_cdata.xl_tx_chain[i].xl_mbuf);
			sc->xl_cdata.xl_tx_chain[i].xl_mbuf = NULL;
		}
	}
	bzero((char *)&sc->xl_ldata->xl_tx_list,
		sizeof(sc->xl_ldata->xl_tx_list));
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
xl_stop(sc)
	struct xl_softc *sc;
{
	struct ifnet *ifp;

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_timer = 0;

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_DISABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STATS_DISABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ENB);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_DISCARD);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_DISABLE);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_COAX_STOP);
	DELAY(800);

#ifdef foo
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_RX_RESET);
	xl_wait(sc);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_TX_RESET);
	xl_wait(sc);
#endif

	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ACK|XL_STAT_INTLATCH);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_STAT_ENB|0);
	CSR_WRITE_2(sc, XL_COMMAND, XL_CMD_INTR_ENB|0);

	if (sc->intr_ack)
		(*sc->intr_ack)(sc);

	/* Stop the stats updater. */
	timeout_del(&sc->xl_stsup_tmo);

	xl_freetxrx(sc);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

void
xl_attach(sc)
	struct xl_softc *sc;
{
	u_int8_t enaddr[ETHER_ADDR_LEN];
	u_int16_t		xcvr[2];
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	int i, media = IFM_ETHER|IFM_100_TX|IFM_FDX;
	struct ifmedia *ifm;

	i = splimp();
	xl_reset(sc);
	splx(i);

	/*
	 * Get station address from the EEPROM.
	 */
	if (xl_read_eeprom(sc, (caddr_t)&enaddr, XL_EE_OEM_ADR0, 3, 1)) {
		printf("\n%s: failed to read station address\n",
		    sc->sc_dev.dv_xname);
		return;
	}
	bcopy(enaddr, (char *)&sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);

	if (bus_dmamem_alloc(sc->sc_dmat, sizeof(struct xl_list_data),
	    PAGE_SIZE, 0, sc->sc_listseg, 1, &sc->sc_listnseg,
	    BUS_DMA_NOWAIT) != 0) {
		printf(": can't alloc list mem\n");
		return;
	}
	if (bus_dmamem_map(sc->sc_dmat, sc->sc_listseg, sc->sc_listnseg,
	    sizeof(struct xl_list_data), &sc->sc_listkva,
	    BUS_DMA_NOWAIT) != 0) {
		printf(": can't map list mem\n");
		return;
	}
	if (bus_dmamap_create(sc->sc_dmat, sizeof(struct xl_list_data), 1,
	    sizeof(struct xl_list_data), 0, BUS_DMA_NOWAIT,
	    &sc->sc_listmap) != 0) {
		printf(": can't alloc list map\n");
		return;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_listmap, sc->sc_listkva,
	    sizeof(struct xl_list_data), NULL, BUS_DMA_NOWAIT) != 0) {
		printf(": can't load list map\n");
		return;
	}
	sc->xl_ldata = (struct xl_list_data *)sc->sc_listkva;
	bzero(sc->xl_ldata, sizeof(struct xl_list_data));

	for (i = 0; i < XL_RX_LIST_CNT; i++) {
		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT,
		    &sc->xl_cdata.xl_rx_chain[i].map) != 0) {
			printf(": can't create rx map\n");
			return;
		}
	}
	if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES, 0,
	    BUS_DMA_NOWAIT, &sc->sc_rx_sparemap) != 0) {
		printf(": can't create rx spare map\n");
		return;
	}

	for (i = 0; i < XL_TX_LIST_CNT; i++) {
		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    XL_TX_LIST_CNT - 3, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &sc->xl_cdata.xl_tx_chain[i].map) != 0) {
			printf(": can't create tx map\n");
			return;
		}
	}
	if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, XL_TX_LIST_CNT - 3,
	    MCLBYTES, 0, BUS_DMA_NOWAIT, &sc->sc_tx_sparemap) != 0) {
		printf(": can't create tx spare map\n");
		return;
	}

	printf(", address %s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	if (sc->xl_flags & (XL_FLAG_INVERT_LED_PWR|XL_FLAG_INVERT_MII_PWR)) {
		u_int16_t n;

		XL_SEL_WIN(2);
		n = CSR_READ_2(sc, 12);

		if (sc->xl_flags & XL_FLAG_INVERT_LED_PWR)
			n |= 0x0010;

		if (sc->xl_flags & XL_FLAG_INVERT_MII_PWR)
			n |= 0x4000;

		CSR_WRITE_2(sc, 12, n);
	}

	/*
	 * Figure out the card type. 3c905B adapters have the
	 * 'supportsNoTxLength' bit set in the capabilities
	 * word in the EEPROM.
	 * Note: my 3c575C cardbus card lies. It returns a value
	 * of 0x1578 for its capabilities word, which is somewhat
	 * nonsensical. Another way to distinguish a 3c90x chip
	 * from a 3c90xB/C chip is to check for the 'supportsLargePackets'
	 * bit. This will only be set for 3c90x boomerage chips.
	 */
	xl_read_eeprom(sc, (caddr_t)&sc->xl_caps, XL_EE_CAPS, 1, 0);
	if (sc->xl_caps & XL_CAPS_NO_TXLENGTH ||
	    !(sc->xl_caps & XL_CAPS_LARGE_PKTS))
		sc->xl_type = XL_TYPE_905B;
	else
		sc->xl_type = XL_TYPE_90X;

	timeout_set(&sc->xl_stsup_tmo, xl_stats_update, sc);

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = xl_ioctl;
	ifp->if_capabilities = IFCAP_VLAN_MTU;
	if (sc->xl_type == XL_TYPE_905B) {
		ifp->if_start = xl_start_90xB;
#ifndef XL905B_TXCSUM_BROKEN
		ifp->if_capabilities |= IFCAP_CSUM_IPv4|IFCAP_CSUM_TCPv4|
		    IFCAP_CSUM_UDPv4;
#endif
	} else
		ifp->if_start = xl_start;
	ifp->if_watchdog = xl_watchdog;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, XL_TX_LIST_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	XL_SEL_WIN(3);
	sc->xl_media = CSR_READ_2(sc, XL_W3_MEDIA_OPT);

	xl_read_eeprom(sc, (char *)&xcvr, XL_EE_ICFG_0, 2, 0);
	sc->xl_xcvr = xcvr[0] | xcvr[1] << 16;
	sc->xl_xcvr &= XL_ICFG_CONNECTOR_MASK;
	sc->xl_xcvr >>= XL_ICFG_CONNECTOR_BITS;

	xl_mediacheck(sc);

	if (sc->xl_media & XL_MEDIAOPT_MII || sc->xl_media & XL_MEDIAOPT_BTX
	    || sc->xl_media & XL_MEDIAOPT_BT4) {
		ifmedia_init(&sc->sc_mii.mii_media, 0,
		    xl_ifmedia_upd, xl_ifmedia_sts);
		sc->xl_hasmii = 1;
		sc->sc_mii.mii_ifp = ifp;
		sc->sc_mii.mii_readreg = xl_miibus_readreg;
		sc->sc_mii.mii_writereg = xl_miibus_writereg;
		sc->sc_mii.mii_statchg = xl_miibus_statchg;
		xl_setcfg(sc);
		mii_attach((struct device *)sc, &sc->sc_mii, 0xffffffff,
		    MII_PHY_ANY, MII_OFFSET_ANY, 0);

		if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
			ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE,
			    0, NULL);
			ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
		}
		else {
			ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);
		}
		ifm = &sc->sc_mii.mii_media;
	}
	else {
		ifmedia_init(&sc->ifmedia, 0, xl_ifmedia_upd, xl_ifmedia_sts);
		sc->xl_hasmii = 0;
		ifm = &sc->ifmedia;
	}

	/*
	 * Sanity check. If the user has selected "auto" and this isn't
	 * a 10/100 card of some kind, we need to force the transceiver
	 * type to something sane.
	 */
	if (sc->xl_xcvr == XL_XCVR_AUTO) {
		xl_choose_xcvr(sc, 0);
		i = splimp();
		xl_reset(sc);
		splx(i);
	}

	if (sc->xl_media & XL_MEDIAOPT_BT) {
		ifmedia_add(ifm, IFM_ETHER|IFM_10_T, 0, NULL);
		ifmedia_add(ifm, IFM_ETHER|IFM_10_T|IFM_HDX, 0, NULL);
		if (sc->xl_caps & XL_CAPS_FULL_DUPLEX)
			ifmedia_add(ifm, IFM_ETHER|IFM_10_T|IFM_FDX, 0, NULL);
	}

	if (sc->xl_media & (XL_MEDIAOPT_AUI|XL_MEDIAOPT_10FL)) {
		/*
		 * Check for a 10baseFL board in disguise.
		 */
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media == XL_MEDIAOPT_10FL) {
			ifmedia_add(ifm, IFM_ETHER|IFM_10_FL, 0, NULL);
			ifmedia_add(ifm, IFM_ETHER|IFM_10_FL|IFM_HDX,
			    0, NULL);
			if (sc->xl_caps & XL_CAPS_FULL_DUPLEX)
				ifmedia_add(ifm,
				    IFM_ETHER|IFM_10_FL|IFM_FDX, 0, NULL);
		} else {
			ifmedia_add(ifm, IFM_ETHER|IFM_10_5, 0, NULL);
		}
	}

	if (sc->xl_media & XL_MEDIAOPT_BNC) {
		ifmedia_add(ifm, IFM_ETHER|IFM_10_2, 0, NULL);
	}

	if (sc->xl_media & XL_MEDIAOPT_BFX) {
		ifp->if_baudrate = 100000000;
		ifmedia_add(ifm, IFM_ETHER|IFM_100_FX, 0, NULL);
	}

	/* Choose a default media. */
	switch(sc->xl_xcvr) {
	case XL_XCVR_10BT:
		media = IFM_ETHER|IFM_10_T;
		xl_setmode(sc, media);
		break;
	case XL_XCVR_AUI:
		if (sc->xl_type == XL_TYPE_905B &&
		    sc->xl_media == XL_MEDIAOPT_10FL) {
			media = IFM_ETHER|IFM_10_FL;
			xl_setmode(sc, media);
		} else {
			media = IFM_ETHER|IFM_10_5;
			xl_setmode(sc, media);
		}
		break;
	case XL_XCVR_COAX:
		media = IFM_ETHER|IFM_10_2;
		xl_setmode(sc, media);
		break;
	case XL_XCVR_AUTO:
	case XL_XCVR_100BTX:
	case XL_XCVR_MII:
		/* Chosen by miibus */
		break;
	case XL_XCVR_100BFX:
		media = IFM_ETHER|IFM_100_FX;
		xl_setmode(sc, media);
		break;
	default:
		printf("%s: unknown XCVR type: %d\n", sc->sc_dev.dv_xname,
							sc->xl_xcvr);
		/*
		 * This will probably be wrong, but it prevents
		 * the ifmedia code from panicking.
		 */
		media = IFM_ETHER | IFM_10_T;
		break;
	}

	if (sc->xl_hasmii == 0)
		ifmedia_set(&sc->ifmedia, media);

	if (sc->xl_flags & XL_FLAG_NO_XCVR_PWR) {
		XL_SEL_WIN(0);
		CSR_WRITE_2(sc, XL_W0_MFG_ID, XL_NO_XCVR_PWR_MAGICBITS);
	}

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	sc->sc_sdhook = shutdownhook_establish(xl_shutdown, sc);
	sc->sc_pwrhook = powerhook_establish(xl_power, sc);
}

int
xl_detach(sc)
	struct xl_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	/* Unhook our tick handler. */
	timeout_del(&sc->xl_stsup_tmo);

	xl_freetxrx(sc);

	/* Detach all PHYs */
	if (sc->xl_hasmii)
		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);

	/* Delete all remaining media. */
	ifmedia_delete_instance(&sc->sc_mii.mii_media, IFM_INST_ANY);

	ether_ifdetach(ifp);
	if_detach(ifp);

	shutdownhook_disestablish(sc->sc_sdhook);
	powerhook_disestablish(sc->sc_pwrhook);

	return (0);
}

void
xl_shutdown(v)
	void *v;
{
	struct xl_softc	*sc = (struct xl_softc *)v;

	xl_reset(sc);
	xl_stop(sc);
}

struct cfdriver xl_cd = {
	0, "xl", DV_IFNET
};
