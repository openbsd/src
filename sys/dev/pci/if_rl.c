/*	$OpenBSD: if_rl.c,v 1.4 1998/11/18 20:11:32 jason Exp $	*/

/*
 * Copyright (c) 1997, 1998
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
 *	$FreeBSD: if_rl.c,v 1.1 1998/10/18 16:24:30 wpaul Exp $
 */

/*
 * RealTek 8129/8139 PCI NIC driver
 *
 * Supports several extremely cheap PCI 10/100 adapters based on
 * the RealTek chipset. Datasheets can be obtained from
 * www.realtek.com.tw.
 *
 * Written by Bill Paul <wpaul@ctr.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The RealTek 8139 PCI NIC redefines the meaning of 'low end.' This is
 * probably the worst PCI ethernet controller ever made, with the possible
 * exception of the FEAST chip made by SMC. The 8139 supports bus-master
 * DMA, but it has a terrible interface that nullifies any performance
 * gains that bus-master DMA usually offers.
 *
 * For transmission, the chip offers a series of four TX descriptor
 * registers. Each transmit frame must be in a contiguous buffer, aligned
 * on a doubleword (32-bit) boundary. This means we almost always have to
 * do mbuf copies in order to transmit a frame, except in the unlikely
 * case where a) the packet fits into a single mbuf, and b) the packet
 * is 32-bit aligned within the mbuf's data area. The presence of only
 * four descriptor registers means that we can never have more than four
 * packets queued for transmission at any one time.
 *
 * Reception is not much better. The driver has to allocate a single large
 * buffer area (up to 64K in size) into which the chip will DMA received
 * frames. Because we don't know where within this region received packets
 * will begin or end, we have no choice but to copy data from the buffer
 * area into mbufs in order to pass the packets up to the higher protocol
 * levels.
 *
 * It's impossible given this rotten design to really achieve decent
 * performance at 100Mbps, unless you happen to have a 400Mhz PII or
 * some equally overmuscled CPU to drive it.
 *
 * On the bright side, the 8139 does have a built-in PHY, although
 * rather than using an MDIO serial interface like most other NICs, the
 * PHY registers are directly accessible through the 8139's register
 * space. The 8139 supports autonegotiation, as well as a 64-bit multicast
 * filter.
 *
 * The 8129 chip is an older version of the 8139 that uses an external PHY
 * chip. The 8129 has a serial MDIO interface for accessing the MII where
 * the 8139 lets you directly access the on-board PHY registers. We need
 * to select which interface to use depending on the chip type.
 *
 * Note: beware of trying to use the Linux RealTek driver as a reference
 * for information about the RealTek chip. It contains several bogosities.
 * It contains definitions for several undocumented registers which it
 * claims are 'required for proper operation' yet it does not use these
 * registers anywhere in the code. It also refers to some undocumented
 * 'Twister tuning codes' which it doesn't use anywhere. It also contains
 * bit definitions for several registers which are totally ignored: magic
 * numbers are used instead, making the code hard to read.
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

#include <vm/vm.h>              /* for vtophys */
#include <vm/pmap.h>            /* for vtophys */
#include <vm/vm_kern.h>
#include <vm/vm_extern.h>

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>
#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

/*
 * Default to using PIO access for this driver. On SMP systems,
 * there appear to be problems with memory mapped mode: it looks like
 * doing too many memory mapped access back to back in rapid succession
 * can hang the bus. I'm inclined to blame this on crummy design/construction
 * on the part of RealTek. Memory mapped mode does appear to work on
 * uniprocessor systems though.
 */
#define RL_USEIOSPACE

#include <dev/pci/if_rlreg.h>

/*
 * Various supported PHY vendors/types and their names. Note that
 * this driver will work with pretty much any MII-compliant PHY,
 * so failure to positively identify the chip is not a fatal error.
 */

#define bootverbose 0
static int rl_probe	__P((struct device *, void *, void *));
static void rl_attach	__P((struct device *, struct device *, void *));
static int rl_intr	__P((void *));
static void rl_shutdown	__P((void *));

/*
 * MII glue
 */
static int rl_mii_read __P((struct device *, int, int));
static void rl_mii_write __P((struct device *, int, int, int));
static void rl_mii_statchg __P((struct device *));

static int rl_encap		__P((struct rl_softc *, struct rl_chain *,
						struct mbuf * ));

static void rl_rxeof		__P((struct rl_softc *));
static void rl_txeof		__P((struct rl_softc *));
static void rl_txeoc		__P((struct rl_softc *));
static void rl_start		__P((struct ifnet *));
static int rl_ioctl		__P((struct ifnet *, u_long, caddr_t));
static void rl_init		__P((void *));
static void rl_stop		__P((struct rl_softc *));
static void rl_watchdog		__P((struct ifnet *));
static int rl_ifmedia_upd	__P((struct ifnet *));
static void rl_ifmedia_sts	__P((struct ifnet *, struct ifmediareq *));

static void rl_eeprom_putbyte	__P((struct rl_softc *, u_int8_t));
static void rl_eeprom_getword	__P((struct rl_softc *, u_int8_t, u_int16_t *));
static void rl_read_eeprom	__P((struct rl_softc *, caddr_t,
					int, int, int));
static void rl_mii_sync		__P((struct rl_softc *));
static void rl_mii_send		__P((struct rl_softc *, u_int32_t, int));
static int rl_mii_readreg	__P((struct rl_softc *, struct rl_mii_frame *));
static int rl_mii_writereg	__P((struct rl_softc *, struct rl_mii_frame *));

static u_int8_t rl_calchash	__P((u_int8_t *));
static void rl_setmulti		__P((struct rl_softc *));
static void rl_reset		__P((struct rl_softc *));
static int rl_list_tx_init	__P((struct rl_softc *));

#define EE_SET(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) | x)

#define EE_CLR(x)					\
	CSR_WRITE_1(sc, RL_EECMD,			\
		CSR_READ_1(sc, RL_EECMD) & ~x)

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
static void rl_eeprom_putbyte(sc, addr)
	struct rl_softc		*sc;
	u_int8_t		addr;
{
	register int		d, i;

	d = addr | RL_EECMD_READ;

	/*
	 * Feed in each bit and stobe the clock.
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

	return;
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
static void rl_eeprom_getword(sc, addr, dest)
	struct rl_softc		*sc;
	u_int8_t		addr;
	u_int16_t		*dest;
{
	register int		i;
	u_int16_t		word = 0;

	/* Enter EEPROM access mode. */
	CSR_WRITE_1(sc, RL_EECMD, RL_EEMODE_PROGRAM|RL_EE_SEL);

	/*
	 * Send address of word we want to read.
	 */
	rl_eeprom_putbyte(sc, addr);

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

	return;
}

/*
 * Read a sequence of words from the EEPROM.
 */
static void rl_read_eeprom(sc, dest, off, cnt, swap)
	struct rl_softc		*sc;
	caddr_t			dest;
	int			off;
	int			cnt;
	int			swap;
{
	int			i;
	u_int16_t		word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		rl_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = ntohs(word);
		else
			*ptr = word;
	}

	return;
}


/*
 * MII access routines are provided for the 8129, which
 * doesn't have a built-in PHY. For the 8139, we fake things
 * up by diverting rl_phy_readreg()/rl_phy_writereg() to the
 * direct access PHY registers.
 */
#define MII_SET(x)					\
	CSR_WRITE_1(sc, RL_MII,				\
		CSR_READ_1(sc, RL_MII) | x)

#define MII_CLR(x)					\
	CSR_WRITE_1(sc, RL_MII,				\
		CSR_READ_1(sc, RL_MII) & ~x)

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
static void rl_mii_sync(sc)
	struct rl_softc		*sc;
{
	register int		i;

	MII_SET(RL_MII_DIR|RL_MII_DATAOUT);

	for (i = 0; i < 32; i++) {
		MII_SET(RL_MII_CLK);
		DELAY(1);
		MII_CLR(RL_MII_CLK);
		DELAY(1);
	}

	return;
}

/*
 * Clock a series of bits through the MII.
 */
static void rl_mii_send(sc, bits, cnt)
	struct rl_softc		*sc;
	u_int32_t		bits;
	int			cnt;
{
	int			i;

	MII_CLR(RL_MII_CLK);

	for (i = (0x1 << (cnt - 1)); i; i >>= 1) {
                if (bits & i) {
			MII_SET(RL_MII_DATAOUT);
                } else {
			MII_CLR(RL_MII_DATAOUT);
                }
		DELAY(1);
		MII_CLR(RL_MII_CLK);
		DELAY(1);
		MII_SET(RL_MII_CLK);
	}
}

/*
 * Read an PHY register through the MII.
 */
static int rl_mii_readreg(sc, frame)
	struct rl_softc		*sc;
	struct rl_mii_frame	*frame;
	
{
	int			i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = RL_MII_STARTDELIM;
	frame->mii_opcode = RL_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	CSR_WRITE_2(sc, RL_MII, 0);

	/*
 	 * Turn on data xmit.
	 */
	MII_SET(RL_MII_DIR);

	rl_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	rl_mii_send(sc, frame->mii_stdelim, 2);
	rl_mii_send(sc, frame->mii_opcode, 2);
	rl_mii_send(sc, frame->mii_phyaddr, 5);
	rl_mii_send(sc, frame->mii_regaddr, 5);

	/* Idle bit */
	MII_CLR((RL_MII_CLK|RL_MII_DATAOUT));
	DELAY(1);
	MII_SET(RL_MII_CLK);
	DELAY(1);

	/* Turn off xmit. */
	MII_CLR(RL_MII_DIR);

	/* Check for ack */
	MII_CLR(RL_MII_CLK);
	DELAY(1);
	MII_SET(RL_MII_CLK);
	DELAY(1);
	ack = CSR_READ_2(sc, RL_MII) & RL_MII_DATAIN;

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			MII_CLR(RL_MII_CLK);
			DELAY(1);
			MII_SET(RL_MII_CLK);
			DELAY(1);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		MII_CLR(RL_MII_CLK);
		DELAY(1);
		if (!ack) {
			if (CSR_READ_2(sc, RL_MII) & RL_MII_DATAIN)
				frame->mii_data |= i;
			DELAY(1);
		}
		MII_SET(RL_MII_CLK);
		DELAY(1);
	}

fail:

	MII_CLR(RL_MII_CLK);
	DELAY(1);
	MII_SET(RL_MII_CLK);
	DELAY(1);

	splx(s);

	if (ack)
		return(1);
	return(0);
}

/*
 * Write to a PHY register through the MII.
 */
static int rl_mii_writereg(sc, frame)
	struct rl_softc		*sc;
	struct rl_mii_frame	*frame;
	
{
	int			s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = RL_MII_STARTDELIM;
	frame->mii_opcode = RL_MII_WRITEOP;
	frame->mii_turnaround = RL_MII_TURNAROUND;
	
	/*
 	 * Turn on data output.
	 */
	MII_SET(RL_MII_DIR);

	rl_mii_sync(sc);

	rl_mii_send(sc, frame->mii_stdelim, 2);
	rl_mii_send(sc, frame->mii_opcode, 2);
	rl_mii_send(sc, frame->mii_phyaddr, 5);
	rl_mii_send(sc, frame->mii_regaddr, 5);
	rl_mii_send(sc, frame->mii_turnaround, 2);
	rl_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	MII_SET(RL_MII_CLK);
	DELAY(1);
	MII_CLR(RL_MII_CLK);
	DELAY(1);

	/*
	 * Turn off xmit.
	 */
	MII_CLR(RL_MII_DIR);

	splx(s);

	return(0);
}

/*
 * Calculate CRC of a multicast group address, return the lower 6 bits.
 */
static u_int8_t rl_calchash(addr)
	u_int8_t		*addr;
{
	u_int32_t		crc, carry;
	int			i, j;
	u_int8_t		c;

	/* Compute CRC for the address value. */
	crc = 0xFFFFFFFF; /* initial value */

	for (i = 0; i < 6; i++) {
		c = *(addr + i);
		for (j = 0; j < 8; j++) {
			carry = ((crc & 0x80000000) ? 1 : 0) ^ (c & 0x01);
			crc <<= 1;
			c >>= 1;
			if (carry)
				crc = (crc ^ 0x04c11db6) | carry;
		}
	}

	/* return the filter bit position */
	return(crc & 0x0000003F);
}

/*
 * Program the 64-bit multicast hash filter.
 */
static void rl_setmulti(sc)
	struct rl_softc		*sc;
{
	struct ifnet		*ifp;
	int			h = 0;
	u_int32_t		hashes[2] = { 0, 0 };
	struct arpcom		*ac = &sc->arpcom;
	struct ether_multi	*enm;
	struct ether_multistep	step;
	u_int32_t		rxfilt;
	int			mcnt = 0;

	ifp = &sc->arpcom.ac_if;

	rxfilt = CSR_READ_4(sc, RL_RXCFG);

	if (ifp->if_flags & IFF_ALLMULTI) {
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
		mcnt++;
		h = rl_calchash(enm->enm_addrlo);
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
	CSR_WRITE_4(sc, RL_MAR0, hashes[0]);
	CSR_WRITE_4(sc, RL_MAR4, hashes[1]);

	return;
}

static void rl_reset(sc)
	struct rl_softc		*sc;
{
	register int		i;

	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_RESET);

	for (i = 0; i < RL_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_1(sc, RL_COMMAND) & RL_CMD_RESET))
			break;
	}
	if (i == RL_TIMEOUT)
		printf("rl%d: reset never completed!\n", sc->rl_unit);

        return;
}

/*
 * Initialize the transmit descriptors.
 */
static int rl_list_tx_init(sc)
	struct rl_softc		*sc;
{
	struct rl_chain_data	*cd;
	int			i;

	cd = &sc->rl_cdata;
	for (i = 0; i < RL_TX_LIST_CNT; i++) {
		cd->rl_tx_chain[i].rl_desc = i * 4;
		CSR_WRITE_4(sc, RL_TXADDR0 + cd->rl_tx_chain[i].rl_desc, 0);
		CSR_WRITE_4(sc, RL_TXSTAT0 + cd->rl_tx_chain[i].rl_desc, 0);
		if (i == (RL_TX_LIST_CNT - 1))
			cd->rl_tx_chain[i].rl_next = &cd->rl_tx_chain[0];
		else
			cd->rl_tx_chain[i].rl_next = &cd->rl_tx_chain[i + 1];
	}

	sc->rl_cdata.rl_tx_cnt = 0;
	cd->rl_tx_cur = cd->rl_tx_free = &cd->rl_tx_chain[0];

	return(0);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 *
 * You know there's something wrong with a PCI bus-master chip design
 * when you have to use m_devget().
 *
 * The receive operation is badly documented in the datasheet, so I'll
 * attempt to document it here. The driver provides a buffer area and
 * places its base address in the RX buffer start address register.
 * The chip then begins copying frames into the RX buffer. Each frame
 * is preceeded by a 32-bit RX status word which specifies the length
 * of the frame and certain other status bits. Each frame (starting with
 * the status word) is also 32-bit aligned. The frame length is in the
 * first 16 bits of the status word; the lower 15 bits correspond with
 * the 'rx status register' mentioned in the datasheet.
 */
static void rl_rxeof(sc)
	struct rl_softc		*sc;
{
        struct ether_header	*eh;
        struct mbuf		*m;
        struct ifnet		*ifp;
	int			total_len = 0;
	u_int32_t		rxstat;
	caddr_t			rxbufpos;
	int			wrap = 0;
	u_int16_t		cur_rx;
	u_int16_t		limit;
	u_int16_t		rx_bytes = 0, max_bytes;

	ifp = &sc->arpcom.ac_if;

	cur_rx = (CSR_READ_2(sc, RL_CURRXADDR) + 16) % RL_RXBUFLEN;

	/* Do not try to read past this point. */
	limit = CSR_READ_2(sc, RL_CURRXBUF) % RL_RXBUFLEN;

	if (limit < cur_rx)
		max_bytes = (RL_RXBUFLEN - cur_rx) + limit;
	else
		max_bytes = limit - cur_rx;

	while((CSR_READ_1(sc, RL_COMMAND) & 1) == 0) {
		rxbufpos = sc->rl_cdata.rl_rx_buf + cur_rx;
		rxstat = *(u_int32_t *)rxbufpos;

		/*
		 * Here's a totally undocumented fact for you. When the
		 * RealTek chip is in the process of copying a packet into
		 * RAM for you, the length will be 0xfff0. If you spot a
		 * packet header with this value, you need to stop. The
		 * datasheet makes absolutely no mention of this and
		 * RealTek should be shot for this.
		 */
		if ((u_int16_t)(rxstat >> 16) == RL_RXSTAT_UNFINISHED)
			break;
	
		if (!(rxstat & RL_RXSTAT_RXOK)) {
			ifp->if_ierrors++;
			if (rxstat & (RL_RXSTAT_BADSYM|RL_RXSTAT_RUNT|
					RL_RXSTAT_GIANT|RL_RXSTAT_CRCERR|
					RL_RXSTAT_ALIGNERR)) {
				CSR_WRITE_2(sc, RL_COMMAND, RL_CMD_TX_ENB);
				CSR_WRITE_2(sc, RL_COMMAND, RL_CMD_TX_ENB|
							RL_CMD_RX_ENB);
				CSR_WRITE_4(sc, RL_RXCFG, RL_RXCFG_CONFIG);
				CSR_WRITE_4(sc, RL_RXADDR,
					vtophys(sc->rl_cdata.rl_rx_buf));
				CSR_WRITE_2(sc, RL_CURRXADDR, cur_rx - 16);
				cur_rx = 0;
			}
			break;
		}

		/* No errors; receive the packet. */	
		total_len = rxstat >> 16;
		rx_bytes += total_len + 4;

		/*
		 * Avoid trying to read more bytes than we know
		 * the chip has prepared for us.
		 */
		if (rx_bytes > max_bytes)
			break;

		rxbufpos = sc->rl_cdata.rl_rx_buf +
			((cur_rx + sizeof(u_int32_t)) % RL_RXBUFLEN);

		if (rxbufpos == (sc->rl_cdata.rl_rx_buf + RL_RXBUFLEN))
			rxbufpos = sc->rl_cdata.rl_rx_buf;

		wrap = (sc->rl_cdata.rl_rx_buf + RL_RXBUFLEN) - rxbufpos;

		if (total_len > wrap) {
			m = m_devget(rxbufpos, wrap, 0, ifp, NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
				printf("rl%d: out of mbufs, tried to "
					"copy %d bytes\n", sc->rl_unit, wrap);
			}
			else
				m_copyback(m, wrap, total_len - wrap,
					sc->rl_cdata.rl_rx_buf);
			cur_rx = (total_len - wrap);
		} else {
			m = m_devget(rxbufpos, total_len, 0, ifp, NULL);
			if (m == NULL) {
				ifp->if_ierrors++;
				printf("rl%d: out of mbufs, tried to "
				"copy %d bytes\n", sc->rl_unit, total_len);
			}
			cur_rx += total_len + 4;
		}

		/*
		 * Round up to 32-bit boundary.
		 */
		cur_rx = (cur_rx + 3) & ~3;
		CSR_WRITE_2(sc, RL_CURRXADDR, cur_rx - 16);

		if (m == NULL)
			continue;

		eh = mtod(m, struct ether_header *);
		ifp->if_ipackets++;

#if NBPFILTER > 0
		/*
		 * Handle BPF listeners. Let the BPF user see the packet, but
		 * don't pass it up to the ether_input() layer unless it's
		 * a broadcast packet, multicast packet, matches our ethernet
		 * address or the interface is in promiscuous mode.
		 */
		if (ifp->if_bpf) {
			bpf_mtap(ifp->if_bpf, m);
			if (ifp->if_flags & IFF_PROMISC &&
				(bcmp(eh->ether_dhost, sc->arpcom.ac_enaddr,
						ETHER_ADDR_LEN) &&
					(eh->ether_dhost[0] & 1) == 0)) {
				m_freem(m);
				continue;
			}
		}
#endif
		/* Remove header from mbuf and pass it on. */
		m_adj(m, sizeof(struct ether_header));
		ether_input(ifp, eh, m);
	}

	return;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */
static void rl_txeof(sc)
	struct rl_softc		*sc;
{
	struct rl_chain		*cur_tx;
	struct ifnet		*ifp;
	u_int32_t		txstat;

	ifp = &sc->arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been uploaded.
	 */
	if (sc->rl_cdata.rl_tx_free == NULL)
		return;

	while(sc->rl_cdata.rl_tx_free->rl_mbuf != NULL) {
		cur_tx = sc->rl_cdata.rl_tx_free;
		txstat = CSR_READ_4(sc, RL_TXSTAT0 + cur_tx->rl_desc);

		if (!(txstat & RL_TXSTAT_TX_OK))
			break;

		if (txstat & RL_TXSTAT_COLLCNT)
			ifp->if_collisions +=
					(txstat & RL_TXSTAT_COLLCNT) >> 24;

		sc->rl_cdata.rl_tx_free = cur_tx->rl_next;

		sc->rl_cdata.rl_tx_cnt--;
		m_freem(cur_tx->rl_mbuf);
		cur_tx->rl_mbuf = NULL;
		ifp->if_opackets++;
	}

	if (!sc->rl_cdata.rl_tx_cnt) {
		ifp->if_flags &= ~IFF_OACTIVE;
	} else {
		if (ifp->if_snd.ifq_head != NULL)
			rl_start(ifp);
	}

	return;
}

/*
 * TX error handler.
 */
static void rl_txeoc(sc)
	struct rl_softc		*sc;
{
	u_int32_t		txstat;
	struct rl_chain		*cur_tx;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;

	if (sc->rl_cdata.rl_tx_free == NULL)
		return;

	while(sc->rl_cdata.rl_tx_free->rl_mbuf != NULL) {
		cur_tx = sc->rl_cdata.rl_tx_free;
		txstat = CSR_READ_4(sc, RL_TXSTAT0 + cur_tx->rl_desc);

		if (!(txstat & RL_TXSTAT_OWN))
			break;

		if (!(txstat & RL_TXSTAT_TX_OK)) {
			ifp->if_oerrors++;
			if (txstat & RL_TXSTAT_COLLCNT)
				ifp->if_collisions +=
					(txstat & RL_TXSTAT_COLLCNT) >> 24;
			CSR_WRITE_4(sc, RL_TXADDR0 + cur_tx->rl_desc,
				vtophys(mtod(cur_tx->rl_mbuf, caddr_t)));
			CSR_WRITE_4(sc, RL_TXSTAT0 + cur_tx->rl_desc,
				RL_TX_EARLYTHRESH |
					cur_tx->rl_mbuf->m_pkthdr.len);
			break;
		} else {
			if (txstat & RL_TXSTAT_COLLCNT)
				ifp->if_collisions +=
					(txstat & RL_TXSTAT_COLLCNT) >> 24;
			sc->rl_cdata.rl_tx_free = cur_tx->rl_next;

			sc->rl_cdata.rl_tx_cnt--;
			m_freem(cur_tx->rl_mbuf);
			cur_tx->rl_mbuf = NULL;
			ifp->if_opackets++;
		}
	}

	return;
}

static int rl_intr(arg)
	void			*arg;
{
	struct rl_softc		*sc;
	struct ifnet		*ifp;
	int			claimed = 0;
	u_int16_t		status;

	sc = arg;
	ifp = &sc->arpcom.ac_if;

	/* Disable interrupts. */
	CSR_WRITE_2(sc, RL_IMR, 0x0000);

	for (;;) {

		status = CSR_READ_2(sc, RL_ISR);
		if (status)
			CSR_WRITE_2(sc, RL_ISR, status);

		if ((status & RL_INTRS) == 0)
			break;

		if (status & RL_ISR_RX_OK)
			rl_rxeof(sc);

		if (status & RL_ISR_RX_ERR)
			rl_rxeof(sc);

		if (status & RL_ISR_TX_OK)
			rl_txeof(sc);

		if (status & RL_ISR_TX_ERR)
			rl_txeoc(sc);

		if (status & RL_ISR_SYSTEM_ERR) {
			rl_reset(sc);
			rl_init(sc);
		}
		claimed = 1;
	}

	/* Re-enable interrupts. */
	CSR_WRITE_2(sc, RL_IMR, RL_INTRS);

	if (ifp->if_snd.ifq_head != NULL) {
		rl_start(ifp);
	}

	return claimed;
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
static int rl_encap(sc, c, m_head)
	struct rl_softc		*sc;
	struct rl_chain		*c;
	struct mbuf		*m_head;
{
	struct mbuf		*m;

	/*
	 * There are two possible encapsulation mechanisms
	 * that we can use: an efficient one, and a very lossy
	 * one. The efficient one only happens very rarely,
	 * whereas the lossy one can and most likely will happen
	 * all the time.
	 * The efficient case happens if:
	 * - the packet fits in a single mbuf
	 * - the packet is 32-bit aligned within the mbuf data area
	 * In this case, we can DMA from the mbuf directly.
	 * The lossy case covers everything else. Bah.
	 */

	m = m_head;

	if (m->m_pkthdr.len > MHLEN || (mtod(m, u_int32_t) & 0x00000003)) {
		struct mbuf		*m_new = NULL;

		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("rl%d: no memory for tx list", sc->rl_unit);
			return(1);
		}
		if (m_head->m_pkthdr.len > MHLEN) {
			MCLGET(m_new, M_DONTWAIT);
			if (!(m_new->m_flags & M_EXT)) {
				m_freem(m_new);
				printf("rl%d: no memory for tx list",
						sc->rl_unit);
				return(1);
			}
		}
		m_copydata(m_head, 0, m_head->m_pkthdr.len,	
					mtod(m_new, caddr_t));
		m_new->m_pkthdr.len = m_new->m_len = m_head->m_pkthdr.len;
		m_freem(m_head);
		m_head = m_new;
	}

	/* Pad frames to at least 60 bytes. */
	if (m_head->m_pkthdr.len < RL_MIN_FRAMELEN)
		m_head->m_pkthdr.len +=
			(RL_MIN_FRAMELEN - m_head->m_pkthdr.len);

	c->rl_mbuf = m_head;

	return(0);
}

/*
 * Main transmit routine.
 */

static void rl_start(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;
	struct mbuf		*m_head = NULL;
	struct rl_chain		*cur_tx = NULL;

	sc = ifp->if_softc;

	/*
	 * Check for an available queue slot. If there are none,
	 * punt.
	 */
	if (sc->rl_cdata.rl_tx_cur->rl_mbuf != NULL) {
		ifp->if_flags |= IFF_OACTIVE;
		return;
	}

	while(sc->rl_cdata.rl_tx_cur->rl_mbuf == NULL) {
		IF_DEQUEUE(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;


		/* Pick a descriptor off the free list. */
		cur_tx = sc->rl_cdata.rl_tx_cur;
		sc->rl_cdata.rl_tx_cur = cur_tx->rl_next;
		sc->rl_cdata.rl_tx_cnt++;

		/* Pack the data into the descriptor. */
		rl_encap(sc, cur_tx, m_head);

#if NBPFILTER > 0
		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, cur_tx->rl_mbuf);
#endif
		/*
		 * Transmit the frame.
	 	 */
		CSR_WRITE_4(sc, RL_TXADDR0 + cur_tx->rl_desc,
				vtophys(mtod(cur_tx->rl_mbuf, caddr_t)));
		CSR_WRITE_4(sc, RL_TXSTAT0 + cur_tx->rl_desc,
			RL_TX_EARLYTHRESH | cur_tx->rl_mbuf->m_pkthdr.len);
	}

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;

	return;
}

static void rl_init(xsc)
	void			*xsc;
{
	struct rl_softc		*sc = xsc;
	struct ifnet		*ifp = &sc->arpcom.ac_if;
	int			s, i;
	u_int32_t		rxcfg = 0;

	s = splimp();

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	rl_stop(sc);

	/* Init our MAC address */
	for (i = 0; i < ETHER_ADDR_LEN; i++) {
		CSR_WRITE_1(sc, RL_IDR0 + i, sc->arpcom.ac_enaddr[i]);
	}

	/* Init the RX buffer pointer register. */
	CSR_WRITE_4(sc, RL_RXADDR, vtophys(sc->rl_cdata.rl_rx_buf));

	/* Init TX descriptors. */
	rl_list_tx_init(sc);

	/*
	 * Enable transmit and receive.
	 */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	/*
	 * Set the buffer size values.
	 */
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

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_2(sc, RL_IMR, RL_INTRS);

	/* Start RX/TX process. */
	CSR_WRITE_4(sc, RL_MISSEDPKT, 0);

	/* Enable receiver and transmitter. */
	CSR_WRITE_1(sc, RL_COMMAND, RL_CMD_TX_ENB|RL_CMD_RX_ENB);

	CSR_WRITE_1(sc, RL_CFG1, RL_CFG1_DRVLOAD|RL_CFG1_FULLDUPLEX);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	(void)splx(s);

	return;
}

/*
 * Set media options.
 */
static int rl_ifmedia_upd(ifp)
	struct ifnet		*ifp;
{
	if (ifp->if_flags & IFF_UP)
		rl_init(ifp->if_softc);
	return (0);
}

/*
 * Report current media status.
 */
static void rl_ifmedia_sts(ifp, ifmr)
	struct ifnet		*ifp;
	struct ifmediareq	*ifmr;
{
	struct rl_softc *sc = ifp->if_softc;

	mii_pollstat(&sc->sc_mii);
	ifmr->ifm_status = sc->sc_mii.mii_media_status;
	ifmr->ifm_active = sc->sc_mii.mii_media_active;
}

static int rl_ioctl(ifp, command, data)
	struct ifnet		*ifp;
	u_long			command;
	caddr_t			data;
{
	struct rl_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int			s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->arpcom, command, data)) > 0) {
		splx(s);
		return error;
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			rl_init(sc);
			arp_ifinit(&sc->arpcom, ifa);
			break;
#endif /* INET */
		default:
			rl_init(sc);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			rl_init(sc);
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				rl_stop(sc);
		}
		error = 0;
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		rl_setmulti(sc);
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

	(void)splx(s);

	return(error);
}

static void rl_watchdog(ifp)
	struct ifnet		*ifp;
{
	struct rl_softc		*sc;

	sc = ifp->if_softc;

	printf("rl%d: watchdog timeout\n", sc->rl_unit);
	ifp->if_oerrors++;
	rl_txeoc(sc);
	rl_txeof(sc);
	rl_rxeof(sc);
	rl_init(sc);

	return;
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
static void rl_stop(sc)
	struct rl_softc		*sc;
{
	register int		i;
	struct ifnet		*ifp;

	ifp = &sc->arpcom.ac_if;
	ifp->if_timer = 0;

	CSR_WRITE_1(sc, RL_COMMAND, 0x00);
	CSR_WRITE_2(sc, RL_IMR, 0x0000);

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < RL_TX_LIST_CNT; i++) {
		if (sc->rl_cdata.rl_tx_chain[i].rl_mbuf != NULL) {
			m_freem(sc->rl_cdata.rl_tx_chain[i].rl_mbuf);
			sc->rl_cdata.rl_tx_chain[i].rl_mbuf = NULL;
			CSR_WRITE_4(sc, RL_TXADDR0 +
			sc->rl_cdata.rl_tx_chain[i].rl_desc, 0x00000000);
		}
	}

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);

	return;
}

static int
rl_probe(parent, match, aux)
	struct device *parent;
	void *match;
	void *aux;
{
	struct pci_attach_args *pa = (struct pci_attach_args *) aux;

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_REALTEK) {
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_REALTEK_RT8129:
		case PCI_PRODUCT_REALTEK_RT8139:
			return 1;
		}
		return 0;
	}

	if (PCI_VENDOR(pa->pa_id) == PCI_VENDOR_REALTEK2) {
		switch (PCI_PRODUCT(pa->pa_id)) {
		case PCI_PRODUCT_REALTEK2_RT8139:
			return 1;
		}
	}

	return 0;
}

static void
rl_attach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct rl_softc *sc = (struct rl_softc *)self;
	struct pci_attach_args *pa = aux;
	pci_chipset_tag_t pc = pa->pa_pc;
	pci_intr_handle_t ih;
	const char *intrstr = NULL;
	u_int8_t enaddr[ETHER_ADDR_LEN];
	struct ifnet *ifp = &sc->arpcom.ac_if;
	bus_addr_t iobase;
	bus_size_t iosize;
	bus_dma_segment_t seg;
	int rseg;
	caddr_t kva;
	u_int32_t command;
	u_int16_t rl_did;

	sc->rl_unit = sc->sc_dev.dv_unit;

	command = pci_conf_read(pa->pa_pc, pa->pa_tag, PCI_COMMAND_STATUS_REG);

#ifdef RL_USEIOSPACE
	if (!(command & PCI_COMMAND_IO_ENABLE)) {
		printf(": failed to enable i/o ports\n");
		return;
	}

	/*
	 * Map control/status registers.
	 */
	if (pci_io_find(pc, pa->pa_tag, RL_PCI_LOIO, &iobase, &iosize)) {
		printf(": can't find i/o space\n");
		return;
	}
	if (bus_space_map(pa->pa_iot, iobase, iosize, 0, &sc->sc_sh)) {
		printf(": can't map i/o space\n");
		return;
	}
	sc->sc_st = pa->pa_iot;
#else
	if (!(command & PCI_COMMAND_MEM_ENABLE)) {
		printf(": failed to enable memory mapping\n");
		return;
	}
	if (pci_mem_find(pc, pa->pa_tag, RL_PCI_LOMEM, &iobase, &iosize, NULL)){
		printf(": can't find mem space\n");
		return;
	}
	if (bus_space_map(pa->pa_memt, iobase, iosize, 0, &sc->sc_sh)) {
		printf(":can't map mem space\n");
		return;
	}
	sc->sc_st = pa->pa_memt;
#endif

	/*
	 * Allocate our interrupt.
	 */
	if (pci_intr_map(pc, pa->pa_intrtag, pa->pa_intrpin,
	    pa->pa_intrline, &ih)) {
		printf(": couldn't map interrupt\n");
		return;
	}

	intrstr = pci_intr_string(pc, ih);
	sc->sc_ih = pci_intr_establish(pc, ih, IPL_NET, rl_intr, sc,
	    self->dv_xname);
	if (sc->sc_ih == NULL) {
		printf(": couldn't establish interrupt");
		if (intrstr != NULL)
			printf(" at %s", intrstr);
		printf("\n");
		return;
	}
	printf(": %s", intrstr);

	rl_reset(sc);

	rl_read_eeprom(sc, (caddr_t)&enaddr, RL_EE_EADDR, 3, 0);
	bcopy(enaddr, (char *)&sc->arpcom.ac_enaddr, ETHER_ADDR_LEN);
	printf(" address %s\n", ether_sprintf(sc->arpcom.ac_enaddr));

	rl_read_eeprom(sc, (caddr_t)&rl_did, RL_EE_PCI_DID, 1, 0);

	if (rl_did == RT_DEVICEID_8139)
		sc->rl_type = RL_8139;
	else if (rl_did == RT_DEVICEID_8129)
		sc->rl_type = RL_8129;
	else {
		printf("\n%s: unknown device id: %x\n", sc->sc_dev.dv_xname,
		    rl_did);
		return;
	}

	sc->sc_dmat = pa->pa_dmat;
	if (bus_dmamem_alloc(sc->sc_dmat, RL_RXBUFLEN + 16, PAGE_SIZE, 0, &seg,
	    1, &rseg, BUS_DMA_NOWAIT)) {
		printf("\n%s: cannot alloc rx buffers\n", sc->sc_dev.dv_xname);
		return;
	}
	if (bus_dmamem_map(sc->sc_dmat, &seg, rseg, RL_RXBUFLEN + 16, &kva,
	    BUS_DMA_NOWAIT | BUS_DMAMEM_NOSYNC)) {
		printf("\n%s: cannot map dma buffers (%d bytes)\n",
		    sc->sc_dev.dv_xname, RL_RXBUFLEN + 16);
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		return;
	}
	sc->sc_dma_mapsize = RL_RXBUFLEN + 16;
	if (bus_dmamap_create(sc->sc_dmat, RL_RXBUFLEN + 16, 1,
	    RL_RXBUFLEN + 16, 0, BUS_DMA_NOWAIT, &sc->sc_dma_prog)) {
		printf("\n%s: cannot create dma map\n");
		bus_dmamem_unmap(sc->sc_dmat, kva, RL_RXBUFLEN + 16);
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		return;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_dma_prog, kva,
	    RL_RXBUFLEN + 16, NULL, BUS_DMA_NOWAIT)) {
		printf("%s: cannot load dma map\n");
		bus_dmamem_unmap(sc->sc_dmat, kva, RL_RXBUFLEN + 16);
		bus_dmamem_free(sc->sc_dmat, &seg, rseg);
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_dma_prog);
	}
	sc->rl_cdata.rl_rx_buf = (caddr_t) kva;
	bzero(sc->rl_cdata.rl_rx_buf, RL_RXBUFLEN + 16);

	ifp->if_softc = sc;
	ifp->if_mtu = ETHERMTU;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = rl_ioctl;
	ifp->if_output = ether_output;
	ifp->if_start = rl_start;
	ifp->if_watchdog = rl_watchdog;
	ifp->if_baudrate = 10000000;
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/*
	 * Initialize our media structures and probe the MII.
	 */
	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = rl_mii_read;
	sc->sc_mii.mii_writereg = rl_mii_write;
	sc->sc_mii.mii_statchg = rl_mii_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, rl_ifmedia_upd, rl_ifmedia_sts);
	mii_phy_probe(self, &sc->sc_mii, 0xffffffff);
	if (LIST_FIRST(&sc->sc_mii.mii_phys) == NULL) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	/*
	 * Attach us everywhere
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

#if NBPFILTER > 0
	bpfattach(&sc->arpcom.ac_if.if_bpf, ifp,
	    DLT_EN10MB, sizeof(struct ether_header));

#endif
	shutdownhook_establish(rl_shutdown, sc);
}

static void rl_shutdown(arg)
	void			*arg;
{
	struct rl_softc		*sc = (struct rl_softc *)arg;

	rl_stop(sc);
}

static int
rl_mii_read(self, phy, reg)
	struct device *self;
	int phy, reg;
{
	struct rl_softc *sc = (struct rl_softc *)self;
	struct rl_mii_frame frame;

	if (sc->rl_type == RL_8139) {
		/*
	 	* The RTL8139 PHY is mapped into PCI registers, unforunately
	 	* it has no phyid, or phyaddr, so assume it is phyaddr 0.
	 	*/
		if (phy != 0)
			return(0);

		DELAY(100);
		switch (reg) {
		case MII_BMCR:
			return CSR_READ_2(sc, RL_BMCR);
		case MII_BMSR:
			return CSR_READ_2(sc, RL_BMSR);
		case MII_ANAR:
			return CSR_READ_2(sc, RL_ANAR);
		case MII_ANLPAR:
			return CSR_READ_2(sc, RL_LPAR);
		case MII_ANER:
			return CSR_READ_2(sc, RL_ANER);
		}
		return (0);
	}

	bzero((char *)&frame, sizeof(frame));

	frame.mii_phyaddr = sc->rl_phy_addr;
	frame.mii_regaddr = reg;
	rl_mii_readreg(sc, &frame);

	return(frame.mii_data);
}

static void
rl_mii_write(self, phy, reg, val)
	struct device *self;
	int phy, reg, val;
{
	struct rl_softc *sc = (struct rl_softc *)self;
	struct rl_mii_frame frame;

	if (sc->rl_type == RL_8139) {
		if (phy != 0)
			return;

		switch (reg) {
		case MII_BMCR:
			CSR_WRITE_2(sc, RL_BMCR, val);
			break;
		case MII_BMSR:
			CSR_WRITE_2(sc, RL_BMSR, val);
			break;
		case MII_ANAR:
			CSR_WRITE_2(sc, RL_ANAR, val);
			break;
		case MII_ANLPAR:
			CSR_WRITE_2(sc, RL_LPAR, val);
			break;
		case MII_ANER:
			CSR_WRITE_2(sc, RL_ANER, val);
			break;
		}
		return;
	}

	bzero((char *)&frame, sizeof(frame));
	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = val;
	rl_mii_writereg(sc, &frame);
}

static void
rl_mii_statchg(self)
	struct device *self;
{
	/* Nothing to do */
}

struct cfattach rl_ca = {
	sizeof(struct rl_softc), rl_probe, rl_attach,
};

struct cfdriver rl_cd = {
	0, "rl", DV_IFNET
};
