/*	$OpenBSD: dc.c,v 1.68 2004/09/23 17:45:16 brad Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999
 *	Bill Paul <wpaul@ee.columbia.edu>.  All rights reserved.
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
 * $FreeBSD: src/sys/pci/if_dc.c,v 1.43 2001/01/19 23:55:07 wpaul Exp $
 */

/*
 * DEC "tulip" clone ethernet driver. Supports the DEC/Intel 21143
 * series chips and several workalikes including the following:
 *
 * Macronix 98713/98715/98725/98727/98732 PMAC (www.macronix.com)
 * Macronix/Lite-On 82c115 PNIC II (www.macronix.com)
 * Lite-On 82c168/82c169 PNIC (www.litecom.com)
 * ASIX Electronics AX88140A (www.asix.com.tw)
 * ASIX Electronics AX88141 (www.asix.com.tw)
 * ADMtek AL981 (www.admtek.com.tw)
 * ADMtek AN983 (www.admtek.com.tw)
 * Davicom DM9100, DM9102, DM9102A (www.davicom8.com)
 * Accton EN1217, EN2242 (www.accton.com)
 * Xircom X3201 (www.xircom.com)
 *
 * Datasheets for the 21143 are available at developer.intel.com.
 * Datasheets for the clone parts can be found at their respective sites.
 * (Except for the PNIC; see www.freebsd.org/~wpaul/PNIC/pnic.ps.gz.)
 * The PNIC II is essentially a Macronix 98715A chip; the only difference
 * worth noting is that its multicast hash table is only 128 bits wide
 * instead of 512.
 *
 * Written by Bill Paul <wpaul@ee.columbia.edu>
 * Electrical Engineering Department
 * Columbia University, New York City
 */

/*
 * The Intel 21143 is the successor to the DEC 21140. It is basically
 * the same as the 21140 but with a few new features. The 21143 supports
 * three kinds of media attachments:
 *
 * o MII port, for 10Mbps and 100Mbps support and NWAY
 *   autonegotiation provided by an external PHY.
 * o SYM port, for symbol mode 100Mbps support.
 * o 10baseT port.
 * o AUI/BNC port.
 *
 * The 100Mbps SYM port and 10baseT port can be used together in
 * combination with the internal NWAY support to create a 10/100
 * autosensing configuration.
 *
 * Note that not all tulip workalikes are handled in this driver: we only
 * deal with those which are relatively well behaved. The Winbond is
 * handled separately due to its different register offsets and the
 * special handling needed for its various bugs. The PNIC is handled
 * here, but I'm not thrilled about it.
 *
 * All of the workalike chips use some form of MII transceiver support
 * with the exception of the Macronix chips, which also have a SYM port.
 * The ASIX AX88140A is also documented to have a SYM port, but all
 * the cards I've seen use an MII transceiver, probably because the
 * AX88140A doesn't support internal NWAY.
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
#include <sys/device.h>
#include <sys/timeout.h>

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

#include <dev/mii/mii.h>
#include <dev/mii/miivar.h>

#include <machine/bus.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/dcreg.h>

int dc_intr(void *);
void dc_shutdown(void *);
struct dc_type *dc_devtype(void *);
int dc_newbuf(struct dc_softc *, int, struct mbuf *);
int dc_encap(struct dc_softc *, struct mbuf *, u_int32_t *);
int dc_coal(struct dc_softc *, struct mbuf **);

void dc_pnic_rx_bug_war(struct dc_softc *, int);
int dc_rx_resync(struct dc_softc *);
void dc_rxeof(struct dc_softc *);
void dc_txeof(struct dc_softc *);
void dc_tick(void *);
void dc_start(struct ifnet *);
int dc_ioctl(struct ifnet *, u_long, caddr_t);
void dc_init(void *);
void dc_stop(struct dc_softc *);
void dc_watchdog(struct ifnet *);
int dc_ifmedia_upd(struct ifnet *);
void dc_ifmedia_sts(struct ifnet *, struct ifmediareq *);

void dc_delay(struct dc_softc *);
void dc_eeprom_width(struct dc_softc *);
void dc_eeprom_idle(struct dc_softc *);
void dc_eeprom_putbyte(struct dc_softc *, int);
void dc_eeprom_getword(struct dc_softc *, int, u_int16_t *);
void dc_eeprom_getword_pnic(struct dc_softc *, int, u_int16_t *);
void dc_read_eeprom(struct dc_softc *, caddr_t, int, int, int);

void dc_mii_writebit(struct dc_softc *, int);
int dc_mii_readbit(struct dc_softc *);
void dc_mii_sync(struct dc_softc *);
void dc_mii_send(struct dc_softc *, u_int32_t, int);
int dc_mii_readreg(struct dc_softc *, struct dc_mii_frame *);
int dc_mii_writereg(struct dc_softc *, struct dc_mii_frame *);
int dc_miibus_readreg(struct device *, int, int);
void dc_miibus_writereg(struct device *, int, int, int);
void dc_miibus_statchg(struct device *);

void dc_setcfg(struct dc_softc *, int);
u_int32_t dc_crc_le(struct dc_softc *, caddr_t);
u_int32_t dc_crc_be(caddr_t);
void dc_setfilt_21143(struct dc_softc *);
void dc_setfilt_asix(struct dc_softc *);
void dc_setfilt_admtek(struct dc_softc *);
void dc_setfilt_xircom(struct dc_softc *);

void dc_setfilt(struct dc_softc *);

void dc_reset(struct dc_softc *);
int dc_list_rx_init(struct dc_softc *);
int dc_list_tx_init(struct dc_softc *);

void dc_read_srom(struct dc_softc *, int);
void dc_parse_21143_srom(struct dc_softc *);
void dc_decode_leaf_sia(struct dc_softc *,
				     struct dc_eblock_sia *);
void dc_decode_leaf_mii(struct dc_softc *,
				     struct dc_eblock_mii *);
void dc_decode_leaf_sym(struct dc_softc *,
				     struct dc_eblock_sym *);
void dc_apply_fixup(struct dc_softc *, int);

#define DC_SETBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) | (x))

#define DC_CLRBIT(sc, reg, x)				\
	CSR_WRITE_4(sc, reg, CSR_READ_4(sc, reg) & ~(x))

#define SIO_SET(x)	DC_SETBIT(sc, DC_SIO, (x))
#define SIO_CLR(x)	DC_CLRBIT(sc, DC_SIO, (x))

void
dc_delay(sc)
	struct dc_softc *sc;
{
	int idx;

	for (idx = (300 / 33) + 1; idx > 0; idx--)
		CSR_READ_4(sc, DC_BUSCTL);
}

void
dc_eeprom_width(sc)
	struct dc_softc *sc;
{
	int i;

	/* Force EEPROM to idle state. */
	dc_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, DC_SIO, DC_SIO_EESEL);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_ROMCTL_READ);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);

	for (i = 3; i--;) {
		if (6 & (1 << i))
			DC_SETBIT(sc, DC_SIO, DC_SIO_EE_DATAIN);
		else
			DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_DATAIN);
		dc_delay(sc);
		DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
		DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	for (i = 1; i <= 12; i++) {
		DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
		if (!(CSR_READ_4(sc, DC_SIO) & DC_SIO_EE_DATAOUT)) {
			DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
			dc_delay(sc);
			break;
		}
		DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	dc_eeprom_idle(sc);

	if (i < 4 || i > 12)
		sc->dc_romwidth = 6;
	else
		sc->dc_romwidth = i;

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, DC_SIO, DC_SIO_EESEL);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_ROMCTL_READ);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);

	/* Turn off EEPROM access mode. */
	dc_eeprom_idle(sc);
}

void
dc_eeprom_idle(sc)
	struct dc_softc *sc;
{
	int i;

	CSR_WRITE_4(sc, DC_SIO, DC_SIO_EESEL);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_ROMCTL_READ);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);

	for (i = 0; i < 25; i++) {
		DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
		DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);
	CSR_WRITE_4(sc, DC_SIO, 0x00000000);
}

/*
 * Send a read command and address to the EEPROM, check for ACK.
 */
void
dc_eeprom_putbyte(sc, addr)
	struct dc_softc *sc;
	int addr;
{
	int d, i;

	d = DC_EECMD_READ >> 6;

	for (i = 3; i--; ) {
		if (d & (1 << i))
			DC_SETBIT(sc, DC_SIO, DC_SIO_EE_DATAIN);
		else
			DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_DATAIN);
		dc_delay(sc);
		DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
		DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	/*
	 * Feed in each bit and strobe the clock.
	 */
	for (i = sc->dc_romwidth; i--;) {
		if (addr & (1 << i)) {
			SIO_SET(DC_SIO_EE_DATAIN);
		} else {
			SIO_CLR(DC_SIO_EE_DATAIN);
		}
		dc_delay(sc);
		SIO_SET(DC_SIO_EE_CLK);
		dc_delay(sc);
		SIO_CLR(DC_SIO_EE_CLK);
		dc_delay(sc);
	}
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 * The PNIC 82c168/82c169 has its own non-standard way to read
 * the EEPROM.
 */
void
dc_eeprom_getword_pnic(sc, addr, dest)
	struct dc_softc *sc;
	int addr;
	u_int16_t *dest;
{
	int i;
	u_int32_t r;

	CSR_WRITE_4(sc, DC_PN_SIOCTL, DC_PN_EEOPCODE_READ|addr);

	for (i = 0; i < DC_TIMEOUT; i++) {
		DELAY(1);
		r = CSR_READ_4(sc, DC_SIO);
		if (!(r & DC_PN_SIOCTL_BUSY)) {
			*dest = (u_int16_t)(r & 0xFFFF);
			return;
		}
	}
}

/*
 * Read a word of data stored in the EEPROM at address 'addr.'
 */
void
dc_eeprom_getword(sc, addr, dest)
	struct dc_softc *sc;
	int addr;
	u_int16_t *dest;
{
	int i;
	u_int16_t word = 0;

	/* Force EEPROM to idle state. */
	dc_eeprom_idle(sc);

	/* Enter EEPROM access mode. */
	CSR_WRITE_4(sc, DC_SIO, DC_SIO_EESEL);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO,  DC_SIO_ROMCTL_READ);
	dc_delay(sc);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_EE_CLK);
	dc_delay(sc);
	DC_SETBIT(sc, DC_SIO, DC_SIO_EE_CS);
	dc_delay(sc);

	/*
	 * Send address of word we want to read.
	 */
	dc_eeprom_putbyte(sc, addr);

	/*
	 * Start reading bits from EEPROM.
	 */
	for (i = 0x8000; i; i >>= 1) {
		SIO_SET(DC_SIO_EE_CLK);
		dc_delay(sc);
		if (CSR_READ_4(sc, DC_SIO) & DC_SIO_EE_DATAOUT)
			word |= i;
		dc_delay(sc);
		SIO_CLR(DC_SIO_EE_CLK);
		dc_delay(sc);
	}

	/* Turn off EEPROM access mode. */
	dc_eeprom_idle(sc);

	*dest = word;
}

/*
 * Read a sequence of words from the EEPROM.
 */
void dc_read_eeprom(sc, dest, off, cnt, swap)
	struct dc_softc *sc;
	caddr_t dest;
	int off, cnt, swap;
{
	int i;
	u_int16_t word = 0, *ptr;

	for (i = 0; i < cnt; i++) {
		if (DC_IS_PNIC(sc))
			dc_eeprom_getword_pnic(sc, off + i, &word);
		else
			dc_eeprom_getword(sc, off + i, &word);
		ptr = (u_int16_t *)(dest + (i * 2));
		if (swap)
			*ptr = betoh16(word);
		else
			*ptr = letoh16(word);
	}
}

/*
 * The following two routines are taken from the Macronix 98713
 * Application Notes pp.19-21.
 */
/*
 * Write a bit to the MII bus.
 */
void
dc_mii_writebit(sc, bit)
	struct dc_softc *sc;
	int bit;
{
	if (bit)
		CSR_WRITE_4(sc, DC_SIO,
		    DC_SIO_ROMCTL_WRITE|DC_SIO_MII_DATAOUT);
	else
		CSR_WRITE_4(sc, DC_SIO, DC_SIO_ROMCTL_WRITE);

	DC_SETBIT(sc, DC_SIO, DC_SIO_MII_CLK);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_MII_CLK);
}

/*
 * Read a bit from the MII bus.
 */
int
dc_mii_readbit(sc)
	struct dc_softc *sc;
{
	CSR_WRITE_4(sc, DC_SIO, DC_SIO_ROMCTL_READ|DC_SIO_MII_DIR);
	CSR_READ_4(sc, DC_SIO);
	DC_SETBIT(sc, DC_SIO, DC_SIO_MII_CLK);
	DC_CLRBIT(sc, DC_SIO, DC_SIO_MII_CLK);
	if (CSR_READ_4(sc, DC_SIO) & DC_SIO_MII_DATAIN)
		return (1);
	return (0);
}

/*
 * Sync the PHYs by setting data bit and strobing the clock 32 times.
 */
void
dc_mii_sync(sc)
	struct dc_softc *sc;
{
	int i;

	CSR_WRITE_4(sc, DC_SIO, DC_SIO_ROMCTL_WRITE);

	for (i = 0; i < 32; i++)
		dc_mii_writebit(sc, 1);
}

/*
 * Clock a series of bits through the MII.
 */
void
dc_mii_send(sc, bits, cnt)
	struct dc_softc *sc;
	u_int32_t bits;
	int cnt;
{
	int i;

	for (i = (0x1 << (cnt - 1)); i; i >>= 1)
		dc_mii_writebit(sc, bits & i);
}

/*
 * Read an PHY register through the MII.
 */
int
dc_mii_readreg(sc, frame)
	struct dc_softc *sc;
	struct dc_mii_frame *frame;
{
	int i, ack, s;

	s = splimp();

	/*
	 * Set up frame for RX.
	 */
	frame->mii_stdelim = DC_MII_STARTDELIM;
	frame->mii_opcode = DC_MII_READOP;
	frame->mii_turnaround = 0;
	frame->mii_data = 0;
	
	/*
	 * Sync the PHYs.
	 */
	dc_mii_sync(sc);

	/*
	 * Send command/address info.
	 */
	dc_mii_send(sc, frame->mii_stdelim, 2);
	dc_mii_send(sc, frame->mii_opcode, 2);
	dc_mii_send(sc, frame->mii_phyaddr, 5);
	dc_mii_send(sc, frame->mii_regaddr, 5);

#ifdef notdef
	/* Idle bit */
	dc_mii_writebit(sc, 1);
	dc_mii_writebit(sc, 0);
#endif

	/* Check for ack */
	ack = dc_mii_readbit(sc);

	/*
	 * Now try reading data bits. If the ack failed, we still
	 * need to clock through 16 cycles to keep the PHY(s) in sync.
	 */
	if (ack) {
		for(i = 0; i < 16; i++) {
			dc_mii_readbit(sc);
		}
		goto fail;
	}

	for (i = 0x8000; i; i >>= 1) {
		if (dc_mii_readbit(sc))
			frame->mii_data |= i;
	}

fail:

	dc_mii_writebit(sc, 0);
	dc_mii_writebit(sc, 0);

	splx(s);

	if (ack)
		return (1);
	return (0);
}

/*
 * Write to a PHY register through the MII.
 */
int
dc_mii_writereg(sc, frame)
	struct dc_softc *sc;
	struct dc_mii_frame *frame;
{
	int s;

	s = splimp();
	/*
	 * Set up frame for TX.
	 */

	frame->mii_stdelim = DC_MII_STARTDELIM;
	frame->mii_opcode = DC_MII_WRITEOP;
	frame->mii_turnaround = DC_MII_TURNAROUND;

	/*
	 * Sync the PHYs.
	 */	
	dc_mii_sync(sc);

	dc_mii_send(sc, frame->mii_stdelim, 2);
	dc_mii_send(sc, frame->mii_opcode, 2);
	dc_mii_send(sc, frame->mii_phyaddr, 5);
	dc_mii_send(sc, frame->mii_regaddr, 5);
	dc_mii_send(sc, frame->mii_turnaround, 2);
	dc_mii_send(sc, frame->mii_data, 16);

	/* Idle bit. */
	dc_mii_writebit(sc, 0);
	dc_mii_writebit(sc, 0);

	splx(s);
	return (0);
}

int
dc_miibus_readreg(self, phy, reg)
	struct device *self;
	int phy, reg;
{
	struct dc_mii_frame frame;
	struct dc_softc *sc = (struct dc_softc *)self;
	int i, rval, phy_reg;

	/*
	 * Note: both the AL981 and AN983 have internal PHYs,
	 * however the AL981 provides direct access to the PHY
	 * registers while the AN983 uses a serial MII interface.
	 * The AN983's MII interface is also buggy in that you
	 * can read from any MII address (0 to 31), but only address 1
	 * behaves normally. To deal with both cases, we pretend
	 * that the PHY is at MII address 1.
	 */
	if (DC_IS_ADMTEK(sc) && phy != DC_ADMTEK_PHYADDR)
		return (0);

	/*
	 * Note: the ukphy probs of the RS7112 report a PHY at
	 * MII address 0 (possibly HomePNA?) and 1 (ethernet)
	 * so we only respond to correct one.
	 */
	if (DC_IS_CONEXANT(sc) && phy != DC_CONEXANT_PHYADDR)
		return (0);

	if (sc->dc_pmode != DC_PMODE_MII) {
		if (phy == (MII_NPHY - 1)) {
			switch(reg) {
			case MII_BMSR:
				/*
				 * Fake something to make the probe
				 * code think there's a PHY here.
				 */
				return (BMSR_MEDIAMASK);
				break;
			case MII_PHYIDR1:
				if (DC_IS_PNIC(sc))
					return (PCI_VENDOR_LITEON);
				return (PCI_VENDOR_DEC);
				break;
			case MII_PHYIDR2:
				if (DC_IS_PNIC(sc))
					return (PCI_PRODUCT_LITEON_PNIC);
				return (PCI_PRODUCT_DEC_21142);
				break;
			default:
				return (0);
				break;
			}
		} else
			return (0);
	}

	if (DC_IS_PNIC(sc)) {
		CSR_WRITE_4(sc, DC_PN_MII, DC_PN_MIIOPCODE_READ |
		    (phy << 23) | (reg << 18));
		for (i = 0; i < DC_TIMEOUT; i++) {
			DELAY(1);
			rval = CSR_READ_4(sc, DC_PN_MII);
			if (!(rval & DC_PN_MII_BUSY)) {
				rval &= 0xFFFF;
				return (rval == 0xFFFF ? 0 : rval);
			}
		}
		return (0);
	}

	if (DC_IS_COMET(sc)) {
		switch(reg) {
		case MII_BMCR:
			phy_reg = DC_AL_BMCR;
			break;
		case MII_BMSR:
			phy_reg = DC_AL_BMSR;
			break;
		case MII_PHYIDR1:
			phy_reg = DC_AL_VENID;
			break;
		case MII_PHYIDR2:
			phy_reg = DC_AL_DEVID;
			break;
		case MII_ANAR:
			phy_reg = DC_AL_ANAR;
			break;
		case MII_ANLPAR:
			phy_reg = DC_AL_LPAR;
			break;
		case MII_ANER:
			phy_reg = DC_AL_ANER;
			break;
		default:
			printf("%s: phy_read: bad phy register %x\n",
			    sc->sc_dev.dv_xname, reg);
			return (0);
			break;
		}

		rval = CSR_READ_4(sc, phy_reg) & 0x0000FFFF;

		if (rval == 0xFFFF)
			return (0);
		return (rval);
	}

	bzero(&frame, sizeof(frame));

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	if (sc->dc_type == DC_TYPE_98713) {
		phy_reg = CSR_READ_4(sc, DC_NETCFG);
		CSR_WRITE_4(sc, DC_NETCFG, phy_reg & ~DC_NETCFG_PORTSEL);
	}
	dc_mii_readreg(sc, &frame);
	if (sc->dc_type == DC_TYPE_98713)
		CSR_WRITE_4(sc, DC_NETCFG, phy_reg);

	return (frame.mii_data);
}

void
dc_miibus_writereg(self, phy, reg, data)
	struct device *self;
	int phy, reg, data;
{
	struct dc_softc *sc = (struct dc_softc *)self;
	struct dc_mii_frame frame;
	int i, phy_reg;

	bzero((char *)&frame, sizeof(frame));

	if (DC_IS_ADMTEK(sc) && phy != DC_ADMTEK_PHYADDR)
		return;
	if (DC_IS_CONEXANT(sc) && phy != DC_CONEXANT_PHYADDR)
		return;

	if (DC_IS_PNIC(sc)) {
		CSR_WRITE_4(sc, DC_PN_MII, DC_PN_MIIOPCODE_WRITE |
		    (phy << 23) | (reg << 10) | data);
		for (i = 0; i < DC_TIMEOUT; i++) {
			if (!(CSR_READ_4(sc, DC_PN_MII) & DC_PN_MII_BUSY))
				break;
		}
		return;
	}

	if (DC_IS_COMET(sc)) {
		switch(reg) {
		case MII_BMCR:
			phy_reg = DC_AL_BMCR;
			break;
		case MII_BMSR:
			phy_reg = DC_AL_BMSR;
			break;
		case MII_PHYIDR1:
			phy_reg = DC_AL_VENID;
			break;
		case MII_PHYIDR2:
			phy_reg = DC_AL_DEVID;
			break;
		case MII_ANAR:
			phy_reg = DC_AL_ANAR;
			break;
		case MII_ANLPAR:
			phy_reg = DC_AL_LPAR;
			break;
		case MII_ANER:
			phy_reg = DC_AL_ANER;
			break;
		default:
			printf("%s: phy_write: bad phy register %x\n",
			    sc->sc_dev.dv_xname, reg);
			return;
			break;
		}

		CSR_WRITE_4(sc, phy_reg, data);
		return;
	}

	frame.mii_phyaddr = phy;
	frame.mii_regaddr = reg;
	frame.mii_data = data;

	if (sc->dc_type == DC_TYPE_98713) {
		phy_reg = CSR_READ_4(sc, DC_NETCFG);
		CSR_WRITE_4(sc, DC_NETCFG, phy_reg & ~DC_NETCFG_PORTSEL);
	}
	dc_mii_writereg(sc, &frame);
	if (sc->dc_type == DC_TYPE_98713)
		CSR_WRITE_4(sc, DC_NETCFG, phy_reg);
}

void
dc_miibus_statchg(self)
	struct device *self;
{
	struct dc_softc *sc = (struct dc_softc *)self;
	struct mii_data *mii;
	struct ifmedia *ifm;

	if (DC_IS_ADMTEK(sc))
		return;

	mii = &sc->sc_mii;
	ifm = &mii->mii_media;
	if (DC_IS_DAVICOM(sc) && IFM_SUBTYPE(ifm->ifm_media) == IFM_HPNA_1) {
		dc_setcfg(sc, ifm->ifm_media);
		sc->dc_if_media = ifm->ifm_media;
	} else {
		dc_setcfg(sc, mii->mii_media_active);
		sc->dc_if_media = mii->mii_media_active;
	}
}

#define DC_BITS_512	9
#define DC_BITS_128	7
#define DC_BITS_64	6

u_int32_t
dc_crc_le(sc, addr)
	struct dc_softc *sc;
	caddr_t addr;
{
	u_int32_t crc;

	/* Compute CRC for the address value. */
	crc = ether_crc32_le(addr, ETHER_ADDR_LEN);

	/*
	 * The hash table on the PNIC II and the MX98715AEC-C/D/E
	 * chips is only 128 bits wide.
	 */
	if (sc->dc_flags & DC_128BIT_HASH)
		return (crc & ((1 << DC_BITS_128) - 1));

	/* The hash table on the MX98715BEC is only 64 bits wide. */
	if (sc->dc_flags & DC_64BIT_HASH)
		return (crc & ((1 << DC_BITS_64) - 1));

	/* Xircom's hash filtering table is different (read: weird) */
	/* Xircom uses the LEAST significant bits */
	if (DC_IS_XIRCOM(sc)) {
		if ((crc & 0x180) == 0x180)
			return (crc & 0x0F) + (crc	& 0x70)*3 + (14 << 4);
		else
			return (crc & 0x1F) + ((crc>>1) & 0xF0)*3 + (12 << 4);
	}

	return (crc & ((1 << DC_BITS_512) - 1));
}

/*
 * Calculate CRC of a multicast group address, return the lower 6 bits.
 */
#define dc_crc_be(addr)	((ether_crc32_be(addr,ETHER_ADDR_LEN) >> 26) \
	& 0x0000003F)

/*
 * 21143-style RX filter setup routine. Filter programming is done by
 * downloading a special setup frame into the TX engine. 21143, Macronix,
 * PNIC, PNIC II and Davicom chips are programmed this way.
 *
 * We always program the chip using 'hash perfect' mode, i.e. one perfect
 * address (our node address) and a 512-bit hash filter for multicast
 * frames. We also sneak the broadcast address into the hash filter since
 * we need that too.
 */
void
dc_setfilt_21143(sc)
	struct dc_softc *sc;
{
	struct dc_desc *sframe;
	u_int32_t h, *sp;
	struct arpcom *ac = &sc->sc_arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;
	struct ifnet *ifp;
	int i;

	ifp = &sc->sc_arpcom.ac_if;

	i = sc->dc_cdata.dc_tx_prod;
	DC_INC(sc->dc_cdata.dc_tx_prod, DC_TX_LIST_CNT);
	sc->dc_cdata.dc_tx_cnt++;
	sframe = &sc->dc_ldata->dc_tx_list[i];
	sp = &sc->dc_ldata->dc_sbuf[0];
	bzero((char *)sp, DC_SFRAME_LEN);

	sframe->dc_data = htole32(sc->sc_listmap->dm_segs[0].ds_addr +
	    offsetof(struct dc_list_data, dc_sbuf));
	sframe->dc_ctl = htole32(DC_SFRAME_LEN | DC_TXCTL_SETUP |
	    DC_TXCTL_TLINK | DC_FILTER_HASHPERF | DC_TXCTL_FINT);

	sc->dc_cdata.dc_tx_chain[i].sd_mbuf =
	    (struct mbuf *)&sc->dc_ldata->dc_sbuf[0];

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);

allmulti:
	if (ifp->if_flags & IFF_ALLMULTI)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);
	else {
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);

		ETHER_FIRST_MULTI(step, ac, enm);
		while (enm != NULL) {
			if (bcmp(enm->enm_addrlo, enm->enm_addrhi,
			    ETHER_ADDR_LEN)) {
				ifp->if_flags |= IFF_ALLMULTI;
				goto allmulti;
			}

			h = dc_crc_le(sc, enm->enm_addrlo);
			sp[h >> 4] |= htole32(1 << (h & 0xF));
			ETHER_NEXT_MULTI(step, enm);
		}
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		h = dc_crc_le(sc, (caddr_t)&etherbroadcastaddr);
		sp[h >> 4] |= htole32(1 << (h & 0xF));
	}

	/* Set our MAC address */
	sp[39] = DC_SP_FIELD(sc->sc_arpcom.ac_enaddr, 0);
	sp[40] = DC_SP_FIELD(sc->sc_arpcom.ac_enaddr, 1);
	sp[41] = DC_SP_FIELD(sc->sc_arpcom.ac_enaddr, 2);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    offsetof(struct dc_list_data, dc_sbuf[0]),
	    sizeof(struct dc_list_data) - 
	    offsetof(struct dc_list_data, dc_sbuf[0]),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	sframe->dc_status = htole32(DC_TXSTAT_OWN);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    offsetof(struct dc_list_data, dc_tx_list[i]),
	    sizeof(struct dc_desc), BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);

	/*
	 * The PNIC takes an exceedingly long time to process its
	 * setup frame; wait 10ms after posting the setup frame
	 * before proceeding, just so it has time to swallow its
	 * medicine.
	 */
	DELAY(10000);

	ifp->if_timer = 5;
}

void
dc_setfilt_admtek(sc)
	struct dc_softc *sc;
{
	struct ifnet *ifp;
	struct arpcom *ac = &sc->sc_arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;
	int h = 0;
	u_int32_t hashes[2] = { 0, 0 };

	ifp = &sc->sc_arpcom.ac_if;

	/* Init our MAC address */
	CSR_WRITE_4(sc, DC_AL_PAR0, *(u_int32_t *)(&sc->sc_arpcom.ac_enaddr[0]));
	CSR_WRITE_4(sc, DC_AL_PAR1, *(u_int32_t *)(&sc->sc_arpcom.ac_enaddr[4]));

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);

allmulti:
	if (ifp->if_flags & IFF_ALLMULTI)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, DC_AL_MAR0, 0);
	CSR_WRITE_4(sc, DC_AL_MAR1, 0);

	/*
	 * If we're already in promisc or allmulti mode, we
	 * don't have to bother programming the multicast filter.
	 */
	if (ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI))
		return;

	/* now program new ones */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		if (bcmp(enm->enm_addrlo, enm->enm_addrhi, ETHER_ADDR_LEN)) {
			ifp->if_flags |= IFF_ALLMULTI;
			goto allmulti;
		}

		if (DC_IS_CENTAUR(sc))
			h = dc_crc_le(sc, enm->enm_addrlo);
		else
			h = dc_crc_be(enm->enm_addrlo);
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		ETHER_NEXT_MULTI(step, enm);
	}

	CSR_WRITE_4(sc, DC_AL_MAR0, hashes[0]);
	CSR_WRITE_4(sc, DC_AL_MAR1, hashes[1]);
}

void
dc_setfilt_asix(sc)
	struct dc_softc *sc;
{
	struct ifnet *ifp;
	struct arpcom *ac = &sc->sc_arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;
	int h = 0;
	u_int32_t hashes[2] = { 0, 0 };

	ifp = &sc->sc_arpcom.ac_if;

	/* Init our MAC address */
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_PAR0);
	CSR_WRITE_4(sc, DC_AX_FILTDATA,
	    *(u_int32_t *)(&sc->sc_arpcom.ac_enaddr[0]));
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_PAR1);
	CSR_WRITE_4(sc, DC_AX_FILTDATA,
	    *(u_int32_t *)(&sc->sc_arpcom.ac_enaddr[4]));

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);

	/*
	 * The ASIX chip has a special bit to enable reception
	 * of broadcast frames.
	 */
	if (ifp->if_flags & IFF_BROADCAST)
		DC_SETBIT(sc, DC_NETCFG, DC_AX_NETCFG_RX_BROAD);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_AX_NETCFG_RX_BROAD);

	/* first, zot all the existing hash bits */
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_MAR0);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, 0);
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_MAR1);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, 0);

	/*
	 * If we're already in promisc or allmulti mode, we
	 * don't have to bother programming the multicast filter.
	 */
	if (ifp->if_flags & (IFF_PROMISC|IFF_ALLMULTI))
		return;

	/* now program new ones */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		h = dc_crc_be(enm->enm_addrlo);
		if (h < 32)
			hashes[0] |= (1 << h);
		else
			hashes[1] |= (1 << (h - 32));
		ETHER_NEXT_MULTI(step, enm);
	}

	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_MAR0);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, hashes[0]);
	CSR_WRITE_4(sc, DC_AX_FILTIDX, DC_AX_FILTIDX_MAR1);
	CSR_WRITE_4(sc, DC_AX_FILTDATA, hashes[1]);
}

void
dc_setfilt_xircom(sc)
	struct dc_softc *sc;
{
	struct dc_desc *sframe;
	struct arpcom *ac = &sc->sc_arpcom;
	struct ether_multi *enm;
	struct ether_multistep step;
	u_int32_t h, *sp;
	struct ifnet *ifp;
	int i;

	ifp = &sc->sc_arpcom.ac_if;
	DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_TX_ON|DC_NETCFG_RX_ON));

	i = sc->dc_cdata.dc_tx_prod;
	DC_INC(sc->dc_cdata.dc_tx_prod, DC_TX_LIST_CNT);
	sc->dc_cdata.dc_tx_cnt++;
	sframe = &sc->dc_ldata->dc_tx_list[i];
	sp = &sc->dc_ldata->dc_sbuf[0];
	bzero((char *)sp, DC_SFRAME_LEN);

	sframe->dc_data = htole32(sc->sc_listmap->dm_segs[0].ds_addr +
	    offsetof(struct dc_list_data, dc_sbuf));
	sframe->dc_ctl = htole32(DC_SFRAME_LEN | DC_TXCTL_SETUP |
	    DC_TXCTL_TLINK | DC_FILTER_HASHPERF | DC_TXCTL_FINT);

	sc->dc_cdata.dc_tx_chain[i].sd_mbuf =
	    (struct mbuf *)&sc->dc_ldata->dc_sbuf[0];

	/* If we want promiscuous mode, set the allframes bit. */
	if (ifp->if_flags & IFF_PROMISC)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_PROMISC);

	if (ifp->if_flags & IFF_ALLMULTI)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);
	else
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_RX_ALLMULTI);

	/* now program new ones */
	ETHER_FIRST_MULTI(step, ac, enm);
	while (enm != NULL) {
		h = dc_crc_le(sc, enm->enm_addrlo);
		sp[h >> 4] |= htole32(1 << (h & 0xF));
		ETHER_NEXT_MULTI(step, enm);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		h = dc_crc_le(sc, (caddr_t)&etherbroadcastaddr);
		sp[h >> 4] |= htole32(1 << (h & 0xF));
	}

	/* Set our MAC address */
	sp[0] = DC_SP_FIELD(sc->sc_arpcom.ac_enaddr, 0);
	sp[1] = DC_SP_FIELD(sc->sc_arpcom.ac_enaddr, 1);
	sp[2] = DC_SP_FIELD(sc->sc_arpcom.ac_enaddr, 2);

	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON);
	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ON);
	ifp->if_flags |= IFF_RUNNING;
	sframe->dc_status = htole32(DC_TXSTAT_OWN);
	CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);

	/*
	 * wait some time...
	 */
	DELAY(1000);

	ifp->if_timer = 5;
}

void
dc_setfilt(sc)
	struct dc_softc *sc;
{
	if (DC_IS_INTEL(sc) || DC_IS_MACRONIX(sc) || DC_IS_PNIC(sc) ||
	    DC_IS_PNICII(sc) || DC_IS_DAVICOM(sc) || DC_IS_CONEXANT(sc))
		dc_setfilt_21143(sc);

	if (DC_IS_ASIX(sc))
		dc_setfilt_asix(sc);

	if (DC_IS_ADMTEK(sc))
		dc_setfilt_admtek(sc);

	if (DC_IS_XIRCOM(sc))
		dc_setfilt_xircom(sc);
}

/*
 * In order to fiddle with the
 * 'full-duplex' and '100Mbps' bits in the netconfig register, we
 * first have to put the transmit and/or receive logic in the idle state.
 */
void
dc_setcfg(sc, media)
	struct dc_softc *sc;
	int media;
{
	int i, restart = 0;
	u_int32_t isr;

	if (IFM_SUBTYPE(media) == IFM_NONE)
		return;

	if (CSR_READ_4(sc, DC_NETCFG) & (DC_NETCFG_TX_ON|DC_NETCFG_RX_ON)) {
		restart = 1;
		DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_TX_ON|DC_NETCFG_RX_ON));

		for (i = 0; i < DC_TIMEOUT; i++) {
			DELAY(10);
			isr = CSR_READ_4(sc, DC_ISR);
			if (isr & DC_ISR_TX_IDLE ||
			    (isr & DC_ISR_RX_STATE) == DC_RXSTATE_STOPPED)
				break;
		}

		if (i == DC_TIMEOUT)
			printf("%s: failed to force tx and "
			    "rx to idle state\n", sc->sc_dev.dv_xname);

	}

	if (IFM_SUBTYPE(media) == IFM_100_TX) {
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_SPEEDSEL);
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_HEARTBEAT);
		if (sc->dc_pmode == DC_PMODE_MII) {
			int watchdogreg;

			if (DC_IS_INTEL(sc)) {
			/* there's a write enable bit here that reads as 1 */
				watchdogreg = CSR_READ_4(sc, DC_WATCHDOG);
				watchdogreg &= ~DC_WDOG_CTLWREN;
				watchdogreg |= DC_WDOG_JABBERDIS;
				CSR_WRITE_4(sc, DC_WATCHDOG, watchdogreg);
			} else {
				DC_SETBIT(sc, DC_WATCHDOG, DC_WDOG_JABBERDIS);
			}
			DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_PCS|
			    DC_NETCFG_PORTSEL|DC_NETCFG_SCRAMBLER));
			if (sc->dc_type == DC_TYPE_98713)
				DC_SETBIT(sc, DC_NETCFG, (DC_NETCFG_PCS|
				    DC_NETCFG_SCRAMBLER));
			if (!DC_IS_DAVICOM(sc))
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			DC_CLRBIT(sc, DC_10BTCTRL, 0xFFFF);
			if (DC_IS_INTEL(sc))
				dc_apply_fixup(sc, IFM_AUTO);
		} else {
			if (DC_IS_PNIC(sc)) {
				DC_PN_GPIO_SETBIT(sc, DC_PN_GPIO_SPEEDSEL);
				DC_PN_GPIO_SETBIT(sc, DC_PN_GPIO_100TX_LOOP);
				DC_SETBIT(sc, DC_PN_NWAY, DC_PN_NWAY_SPEEDSEL);
			}
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PCS);
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_SCRAMBLER);
			if (DC_IS_INTEL(sc))
				dc_apply_fixup(sc,
				    (media & IFM_GMASK) == IFM_FDX ?
				    IFM_100_TX|IFM_FDX : IFM_100_TX);
		}
	}

	if (IFM_SUBTYPE(media) == IFM_10_T) {
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_SPEEDSEL);
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_HEARTBEAT);
		if (sc->dc_pmode == DC_PMODE_MII) {
			int watchdogreg;

			if (DC_IS_INTEL(sc)) {
			/* there's a write enable bit here that reads as 1 */
				watchdogreg = CSR_READ_4(sc, DC_WATCHDOG);
				watchdogreg &= ~DC_WDOG_CTLWREN;
				watchdogreg |= DC_WDOG_JABBERDIS;
				CSR_WRITE_4(sc, DC_WATCHDOG, watchdogreg);
			} else {
				DC_SETBIT(sc, DC_WATCHDOG, DC_WDOG_JABBERDIS);
			}
			DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_PCS|
			    DC_NETCFG_PORTSEL|DC_NETCFG_SCRAMBLER));
			if (sc->dc_type == DC_TYPE_98713)
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PCS);
			if (!DC_IS_DAVICOM(sc))
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			DC_CLRBIT(sc, DC_10BTCTRL, 0xFFFF);
			if (DC_IS_INTEL(sc))
				dc_apply_fixup(sc, IFM_AUTO);
		} else {
			if (DC_IS_PNIC(sc)) {
				DC_PN_GPIO_CLRBIT(sc, DC_PN_GPIO_SPEEDSEL);
				DC_PN_GPIO_SETBIT(sc, DC_PN_GPIO_100TX_LOOP);
				DC_CLRBIT(sc, DC_PN_NWAY, DC_PN_NWAY_SPEEDSEL);
			}
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_PCS);
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_SCRAMBLER);
			if (DC_IS_INTEL(sc)) {
				DC_CLRBIT(sc, DC_SIARESET, DC_SIA_RESET);
				DC_CLRBIT(sc, DC_10BTCTRL, 0xFFFF);
				if ((media & IFM_GMASK) == IFM_FDX)
					DC_SETBIT(sc, DC_10BTCTRL, 0x7F3D);
				else
					DC_SETBIT(sc, DC_10BTCTRL, 0x7F3F);
				DC_SETBIT(sc, DC_SIARESET, DC_SIA_RESET);
				DC_CLRBIT(sc, DC_10BTCTRL,
				    DC_TCTL_AUTONEGENBL);
				dc_apply_fixup(sc,
				    (media & IFM_GMASK) == IFM_FDX ?
				    IFM_10_T|IFM_FDX : IFM_10_T);
				DELAY(20000);
			}
		}
	}

	/*
	 * If this is a Davicom DM9102A card with a DM9801 HomePNA
	 * PHY and we want HomePNA mode, set the portsel bit to turn
	 * on the external MII port.
	 */
	if (DC_IS_DAVICOM(sc)) {
		if (IFM_SUBTYPE(media) == IFM_HPNA_1) {
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
			sc->dc_link = 1;
		} else {
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_PORTSEL);
		}
	}

	if ((media & IFM_GMASK) == IFM_FDX) {
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_FULLDUPLEX);
		if (sc->dc_pmode == DC_PMODE_SYM && DC_IS_PNIC(sc))
			DC_SETBIT(sc, DC_PN_NWAY, DC_PN_NWAY_DUPLEX);
	} else {
		DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_FULLDUPLEX);
		if (sc->dc_pmode == DC_PMODE_SYM && DC_IS_PNIC(sc))
			DC_CLRBIT(sc, DC_PN_NWAY, DC_PN_NWAY_DUPLEX);
	}

	if (restart)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON|DC_NETCFG_RX_ON);
}

void
dc_reset(sc)
	struct dc_softc *sc;
{
	int i;

	DC_SETBIT(sc, DC_BUSCTL, DC_BUSCTL_RESET);

	for (i = 0; i < DC_TIMEOUT; i++) {
		DELAY(10);
		if (!(CSR_READ_4(sc, DC_BUSCTL) & DC_BUSCTL_RESET))
			break;
	}

	if (DC_IS_ASIX(sc) || DC_IS_ADMTEK(sc) || DC_IS_XIRCOM(sc) ||
	    DC_IS_INTEL(sc) || DC_IS_CONEXANT(sc)) {
		DELAY(10000);
		DC_CLRBIT(sc, DC_BUSCTL, DC_BUSCTL_RESET);
		i = 0;
	}

	if (i == DC_TIMEOUT)
		printf("%s: reset never completed!\n", sc->sc_dev.dv_xname);

	/* Wait a little while for the chip to get its brains in order. */
	DELAY(1000);

	CSR_WRITE_4(sc, DC_IMR, 0x00000000);
	CSR_WRITE_4(sc, DC_BUSCTL, 0x00000000);
	CSR_WRITE_4(sc, DC_NETCFG, 0x00000000);

	/*
	 * Bring the SIA out of reset. In some cases, it looks
	 * like failing to unreset the SIA soon enough gets it
	 * into a state where it will never come out of reset
	 * until we reset the whole chip again.
	 */
	if (DC_IS_INTEL(sc)) {
		DC_SETBIT(sc, DC_SIARESET, DC_SIA_RESET);
		CSR_WRITE_4(sc, DC_10BTCTRL, 0);
		CSR_WRITE_4(sc, DC_WATCHDOG, 0);
	}

	if (sc->dc_type == DC_TYPE_21145)
		dc_setcfg(sc, IFM_10_T);
}

void
dc_apply_fixup(sc, media)
	struct dc_softc *sc;
	int media;
{
	struct dc_mediainfo *m;
	u_int8_t *p;
	int i;
	u_int32_t reg;

	m = sc->dc_mi;

	while (m != NULL) {
		if (m->dc_media == media)
			break;
		m = m->dc_next;
	}

	if (m == NULL)
		return;

	for (i = 0, p = m->dc_reset_ptr; i < m->dc_reset_len; i++, p += 2) {
		reg = (p[0] | (p[1] << 8)) << 16;
		CSR_WRITE_4(sc, DC_WATCHDOG, reg);
	}

	for (i = 0, p = m->dc_gp_ptr; i < m->dc_gp_len; i++, p += 2) {
		reg = (p[0] | (p[1] << 8)) << 16;
		CSR_WRITE_4(sc, DC_WATCHDOG, reg);
	}
}

void
dc_decode_leaf_sia(sc, l)
	struct dc_softc *sc;
	struct dc_eblock_sia *l;
{
	struct dc_mediainfo *m;

	m = malloc(sizeof(struct dc_mediainfo), M_DEVBUF, M_NOWAIT);
	if (m == NULL)
		return;
	bzero(m, sizeof(struct dc_mediainfo));
	if (l->dc_sia_code == DC_SIA_CODE_10BT)
		m->dc_media = IFM_10_T;

	if (l->dc_sia_code == DC_SIA_CODE_10BT_FDX)
		m->dc_media = IFM_10_T|IFM_FDX;

	if (l->dc_sia_code == DC_SIA_CODE_10B2)
		m->dc_media = IFM_10_2;

	if (l->dc_sia_code == DC_SIA_CODE_10B5)
		m->dc_media = IFM_10_5;

	m->dc_gp_len = 2;
	m->dc_gp_ptr = (u_int8_t *)&l->dc_sia_gpio_ctl;

	m->dc_next = sc->dc_mi;
	sc->dc_mi = m;

	sc->dc_pmode = DC_PMODE_SIA;
}

void
dc_decode_leaf_sym(sc, l)
	struct dc_softc *sc;
	struct dc_eblock_sym *l;
{
	struct dc_mediainfo *m;

	m = malloc(sizeof(struct dc_mediainfo), M_DEVBUF, M_NOWAIT);
	if (m == NULL)
		return;
	bzero(m, sizeof(struct dc_mediainfo));
	if (l->dc_sym_code == DC_SYM_CODE_100BT)
		m->dc_media = IFM_100_TX;

	if (l->dc_sym_code == DC_SYM_CODE_100BT_FDX)
		m->dc_media = IFM_100_TX|IFM_FDX;

	m->dc_gp_len = 2;
	m->dc_gp_ptr = (u_int8_t *)&l->dc_sym_gpio_ctl;

	m->dc_next = sc->dc_mi;
	sc->dc_mi = m;

	sc->dc_pmode = DC_PMODE_SYM;
}

void
dc_decode_leaf_mii(sc, l)
	struct dc_softc *sc;
	struct dc_eblock_mii *l;
{
	u_int8_t *p;
	struct dc_mediainfo *m;

	m = malloc(sizeof(struct dc_mediainfo), M_DEVBUF, M_NOWAIT);
	if (m == NULL)
		return;
	bzero(m, sizeof(struct dc_mediainfo));
	/* We abuse IFM_AUTO to represent MII. */
	m->dc_media = IFM_AUTO;
	m->dc_gp_len = l->dc_gpr_len;

	p = (u_int8_t *)l;
	p += sizeof(struct dc_eblock_mii);
	m->dc_gp_ptr = p;
	p += 2 * l->dc_gpr_len;
	m->dc_reset_len = *p;
	p++;
	m->dc_reset_ptr = p;

	m->dc_next = sc->dc_mi;
	sc->dc_mi = m;
}

void
dc_read_srom(sc, bits)
	struct dc_softc *sc;
	int bits;
{
	int size;

	size = 2 << bits;
	sc->dc_srom = malloc(size, M_DEVBUF, M_NOWAIT);
	if (sc->dc_srom == NULL)
		return;
	dc_read_eeprom(sc, (caddr_t)sc->dc_srom, 0, (size / 2), 0);
}

void
dc_parse_21143_srom(sc)
	struct dc_softc *sc;
{
	struct dc_leaf_hdr *lhdr;
	struct dc_eblock_hdr *hdr;
	int i, loff;
	char *ptr;

	loff = sc->dc_srom[27];
	lhdr = (struct dc_leaf_hdr *)&(sc->dc_srom[loff]);

	ptr = (char *)lhdr;
	ptr += sizeof(struct dc_leaf_hdr) - 1;
	for (i = 0; i < lhdr->dc_mcnt; i++) {
		hdr = (struct dc_eblock_hdr *)ptr;
		switch(hdr->dc_type) {
		case DC_EBLOCK_MII:
			dc_decode_leaf_mii(sc, (struct dc_eblock_mii *)hdr);
			break;
		case DC_EBLOCK_SIA:
			dc_decode_leaf_sia(sc, (struct dc_eblock_sia *)hdr);
			break;
		case DC_EBLOCK_SYM:
			dc_decode_leaf_sym(sc, (struct dc_eblock_sym *)hdr);
			break;
		default:
			/* Don't care. Yet. */
			break;
		}
		ptr += (hdr->dc_len & 0x7F);
		ptr++;
	}
}

/*
 * Attach the interface. Allocate softc structures, do ifmedia
 * setup and ethernet/BPF attach.
 */
void
dc_attach(sc)
	struct dc_softc *sc;
{
	struct ifnet *ifp;
	int mac_offset, tmp, i;

	/*
	 * Get station address from the EEPROM.
	 */
	if (sc->sc_hasmac)
		goto hasmac;

	switch(sc->dc_type) {
	case DC_TYPE_98713:
	case DC_TYPE_98713A:
	case DC_TYPE_987x5:
	case DC_TYPE_PNICII:
		dc_read_eeprom(sc, (caddr_t)&mac_offset,
		    (DC_EE_NODEADDR_OFFSET / 2), 1, 0);
		dc_read_eeprom(sc, (caddr_t)&sc->sc_arpcom.ac_enaddr,
		    (mac_offset / 2), 3, 0);
		break;
	case DC_TYPE_PNIC:
		dc_read_eeprom(sc, (caddr_t)&sc->sc_arpcom.ac_enaddr, 0, 3, 1);
		break;
	case DC_TYPE_DM9102:
	case DC_TYPE_21143:
	case DC_TYPE_21145:
	case DC_TYPE_ASIX:
		dc_read_eeprom(sc, (caddr_t)&sc->sc_arpcom.ac_enaddr,	
		    DC_EE_NODEADDR, 3, 0);
		break;
	case DC_TYPE_AL981:
	case DC_TYPE_AN983:
		bcopy(&sc->dc_srom[DC_AL_EE_NODEADDR], &sc->sc_arpcom.ac_enaddr,
		    ETHER_ADDR_LEN);
		break;
	case DC_TYPE_XIRCOM:
		break;
	case DC_TYPE_CONEXANT:
		bcopy(&sc->dc_srom + DC_CONEXANT_EE_NODEADDR,
		    &sc->sc_arpcom.ac_enaddr, ETHER_ADDR_LEN);
		break;
	default:
		dc_read_eeprom(sc, (caddr_t)&sc->sc_arpcom.ac_enaddr,
		    DC_EE_NODEADDR, 3, 0);
		break;
	}
hasmac:

	if (bus_dmamem_alloc(sc->sc_dmat, sizeof(struct dc_list_data),
	    PAGE_SIZE, 0, sc->sc_listseg, 1, &sc->sc_listnseg,
	    BUS_DMA_NOWAIT) != 0) {
		printf(": can't alloc list mem\n");
		goto fail;
	}
	if (bus_dmamem_map(sc->sc_dmat, sc->sc_listseg, sc->sc_listnseg,
	    sizeof(struct dc_list_data), &sc->sc_listkva,
	    BUS_DMA_NOWAIT) != 0) {
		printf(": can't map list mem\n");
		goto fail;
	}
	if (bus_dmamap_create(sc->sc_dmat, sizeof(struct dc_list_data), 1,
	    sizeof(struct dc_list_data), 0, BUS_DMA_NOWAIT,
	    &sc->sc_listmap) != 0) {
		printf(": can't alloc list map\n");
		goto fail;
	}
	if (bus_dmamap_load(sc->sc_dmat, sc->sc_listmap, sc->sc_listkva,
	    sizeof(struct dc_list_data), NULL, BUS_DMA_NOWAIT) != 0) {
		printf(": can't load list map\n");
		goto fail;
	}
	sc->dc_ldata = (struct dc_list_data *)sc->sc_listkva;
	bzero(sc->dc_ldata, sizeof(struct dc_list_data));

	for (i = 0; i < DC_RX_LIST_CNT; i++) {
		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT,
		    &sc->dc_cdata.dc_rx_chain[i].sd_map) != 0) {
			printf(": can't create rx map\n");
			return;
		}
	}
	if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES, 0,
	    BUS_DMA_NOWAIT, &sc->sc_rx_sparemap) != 0) {
		printf(": can't create rx spare map\n");
		return;
	}

	for (i = 0; i < DC_TX_LIST_CNT; i++) {
		if (bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    DC_TX_LIST_CNT - 5, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &sc->dc_cdata.dc_tx_chain[i].sd_map) != 0) {
			printf(": can't create tx map\n");
			return;
		}
	}
	if (bus_dmamap_create(sc->sc_dmat, MCLBYTES, DC_TX_LIST_CNT - 5,
	    MCLBYTES, 0, BUS_DMA_NOWAIT, &sc->sc_tx_sparemap) != 0) {
		printf(": can't create tx spare map\n");
		return;
	}

	/*
	 * A 21143 or clone chip was detected. Inform the world.
	 */
	printf(" address %s\n", ether_sprintf(sc->sc_arpcom.ac_enaddr));

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_ioctl = dc_ioctl;
	ifp->if_start = dc_start;
	ifp->if_watchdog = dc_watchdog;
	ifp->if_baudrate = 10000000;
	IFQ_SET_MAXLEN(&ifp->if_snd, DC_TX_LIST_CNT - 1);
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

#if NVLAN > 0
	ifp->if_capabilities = IFCAP_VLAN_MTU;
#endif

	/* Do MII setup. If this is a 21143, check for a PHY on the
	 * MII bus after applying any necessary fixups to twiddle the
	 * GPIO bits. If we don't end up finding a PHY, restore the
	 * old selection (SIA only or SIA/SYM) and attach the dcphy
	 * driver instead.
	 */
	if (DC_IS_INTEL(sc)) {
		dc_apply_fixup(sc, IFM_AUTO);
		tmp = sc->dc_pmode;
		sc->dc_pmode = DC_PMODE_MII;
	}

	sc->sc_mii.mii_ifp = ifp;
	sc->sc_mii.mii_readreg = dc_miibus_readreg;
	sc->sc_mii.mii_writereg = dc_miibus_writereg;
	sc->sc_mii.mii_statchg = dc_miibus_statchg;
	ifmedia_init(&sc->sc_mii.mii_media, 0, dc_ifmedia_upd, dc_ifmedia_sts);
	mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff, MII_PHY_ANY,
	    MII_OFFSET_ANY, 0);

	if (DC_IS_INTEL(sc)) {
		if (LIST_EMPTY(&sc->sc_mii.mii_phys)) {
			sc->dc_pmode = tmp;
			if (sc->dc_pmode != DC_PMODE_SIA)
				sc->dc_pmode = DC_PMODE_SYM;
			sc->dc_flags |= DC_21143_NWAY;
			if (sc->dc_flags & DC_MOMENCO_BOTCH)
				sc->dc_pmode = DC_PMODE_MII;
			mii_attach(&sc->sc_dev, &sc->sc_mii, 0xffffffff,
			    MII_PHY_ANY, MII_OFFSET_ANY, 0);
		} else {
			/* we have a PHY, so we must clear this bit */
			sc->dc_flags &= ~DC_TULIP_LEDS;
		}
	}

	if (LIST_EMPTY(&sc->sc_mii.mii_phys)) {
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE, 0, NULL);
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_NONE);
		printf("%s: MII without any PHY!\n", sc->sc_dev.dv_xname);
	} else if (sc->dc_type == DC_TYPE_21145) {
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_10_T);
	} else
		ifmedia_set(&sc->sc_mii.mii_media, IFM_ETHER|IFM_AUTO);

	if (DC_IS_DAVICOM(sc) && sc->dc_revision >= DC_REVISION_DM9102A)
		ifmedia_add(&sc->sc_mii.mii_media, IFM_ETHER|IFM_HPNA_1,0,NULL);

	if (DC_IS_XIRCOM(sc)) {
		/*
		 * setup General Purpose Port mode and data so the tulip
		 * can talk to the MII.
		 */
		CSR_WRITE_4(sc, DC_SIAGP, DC_SIAGP_WRITE_EN | DC_SIAGP_INT1_EN |
		    DC_SIAGP_MD_GP2_OUTPUT | DC_SIAGP_MD_GP0_OUTPUT);
		DELAY(10);
		CSR_WRITE_4(sc, DC_SIAGP, DC_SIAGP_INT1_EN |
		    DC_SIAGP_MD_GP2_OUTPUT | DC_SIAGP_MD_GP0_OUTPUT);
		DELAY(10);
	}

	/*
	 * Call MI attach routines.
	 */
	if_attach(ifp);
	ether_ifattach(ifp);

	sc->sc_dhook = shutdownhook_establish(dc_shutdown, sc);

fail:
	return;
}

int
dc_detach(sc)
	struct dc_softc *sc;
{
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;

	if (LIST_FIRST(&sc->sc_mii.mii_phys) != NULL)
		mii_detach(&sc->sc_mii, MII_PHY_ANY, MII_OFFSET_ANY);

	if (sc->dc_srom)
		free(sc->dc_srom, M_DEVBUF);

	timeout_del(&sc->dc_tick_tmo);

	ether_ifdetach(ifp);
	if_detach(ifp);

	shutdownhook_disestablish(sc->sc_dhook);

	return (0);
}

/*
 * Initialize the transmit descriptors.
 */
int
dc_list_tx_init(sc)
	struct dc_softc *sc;
{
	struct dc_chain_data *cd;
	struct dc_list_data *ld;
	int i;
	bus_addr_t next;

	cd = &sc->dc_cdata;
	ld = sc->dc_ldata;
	for (i = 0; i < DC_TX_LIST_CNT; i++) {
		next = sc->sc_listmap->dm_segs[0].ds_addr;
		if (i == (DC_TX_LIST_CNT - 1))
			next +=
			    offsetof(struct dc_list_data, dc_tx_list[0]);
		else
			next +=
			    offsetof(struct dc_list_data, dc_tx_list[i + 1]);
		cd->dc_tx_chain[i].sd_mbuf = NULL;
		ld->dc_tx_list[i].dc_data = htole32(0);
		ld->dc_tx_list[i].dc_ctl = htole32(0);
		ld->dc_tx_list[i].dc_next = htole32(next);
	}

	cd->dc_tx_prod = cd->dc_tx_cons = cd->dc_tx_cnt = 0;

	return (0);
}


/*
 * Initialize the RX descriptors and allocate mbufs for them. Note that
 * we arrange the descriptors in a closed ring, so that the last descriptor
 * points back to the first.
 */
int
dc_list_rx_init(sc)
	struct dc_softc *sc;
{
	struct dc_chain_data *cd;
	struct dc_list_data *ld;
	int i;
	bus_addr_t next;

	cd = &sc->dc_cdata;
	ld = sc->dc_ldata;

	for (i = 0; i < DC_RX_LIST_CNT; i++) {
		if (dc_newbuf(sc, i, NULL) == ENOBUFS)
			return (ENOBUFS);
		next = sc->sc_listmap->dm_segs[0].ds_addr;
		if (i == (DC_RX_LIST_CNT - 1))
			next +=
			    offsetof(struct dc_list_data, dc_rx_list[0]);
		else
			next +=
			    offsetof(struct dc_list_data, dc_rx_list[i + 1]);
		ld->dc_rx_list[i].dc_next = htole32(next);
	}

	cd->dc_rx_prod = 0;

	return (0);
}

/*
 * Initialize an RX descriptor and attach an MBUF cluster.
 */
int
dc_newbuf(sc, i, m)
	struct dc_softc *sc;
	int i;
	struct mbuf *m;
{
	struct mbuf *m_new = NULL;
	struct dc_desc *c;
	bus_dmamap_t map;

	c = &sc->dc_ldata->dc_rx_list[i];

	if (m == NULL) {
		MGETHDR(m_new, M_DONTWAIT, MT_DATA);
		if (m_new == NULL) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", sc->sc_dev.dv_xname);
			return (ENOBUFS);
		}

		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			printf("%s: no memory for rx list "
			    "-- packet dropped!\n", sc->sc_dev.dv_xname);
			m_freem(m_new);
			return (ENOBUFS);
		}
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		if (bus_dmamap_load_mbuf(sc->sc_dmat, sc->sc_rx_sparemap,
		    m_new, BUS_DMA_NOWAIT) != 0) {
			printf("%s: rx load failed\n", sc->sc_dev.dv_xname);
			m_freem(m_new);
			return (ENOBUFS);
		}
		map = sc->dc_cdata.dc_rx_chain[i].sd_map;
		sc->dc_cdata.dc_rx_chain[i].sd_map = sc->sc_rx_sparemap;
		sc->sc_rx_sparemap = map;
	} else {
		m_new = m;
		m_new->m_len = m_new->m_pkthdr.len = MCLBYTES;
		m_new->m_data = m_new->m_ext.ext_buf;
	}

	m_adj(m_new, sizeof(u_int64_t));

	/*
	 * If this is a PNIC chip, zero the buffer. This is part
	 * of the workaround for the receive bug in the 82c168 and
	 * 82c169 chips.
	 */
	if (sc->dc_flags & DC_PNIC_RX_BUG_WAR)
		bzero((char *)mtod(m_new, char *), m_new->m_len);

	bus_dmamap_sync(sc->sc_dmat, sc->dc_cdata.dc_rx_chain[i].sd_map, 0,
	    sc->dc_cdata.dc_rx_chain[i].sd_map->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	sc->dc_cdata.dc_rx_chain[i].sd_mbuf = m_new;
	c->dc_data = htole32(
	    sc->dc_cdata.dc_rx_chain[i].sd_map->dm_segs[0].ds_addr +
	    sizeof(u_int64_t));
	c->dc_ctl = htole32(DC_RXCTL_RLINK | DC_RXLEN);
	c->dc_status = htole32(DC_RXSTAT_OWN);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    offsetof(struct dc_list_data, dc_rx_list[i]),
	    sizeof(struct dc_desc),
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	return (0);
}

/*
 * Grrrrr.
 * The PNIC chip has a terrible bug in it that manifests itself during
 * periods of heavy activity. The exact mode of failure if difficult to
 * pinpoint: sometimes it only happens in promiscuous mode, sometimes it
 * will happen on slow machines. The bug is that sometimes instead of
 * uploading one complete frame during reception, it uploads what looks
 * like the entire contents of its FIFO memory. The frame we want is at
 * the end of the whole mess, but we never know exactly how much data has
 * been uploaded, so salvaging the frame is hard.
 *
 * There is only one way to do it reliably, and it's disgusting.
 * Here's what we know:
 *
 * - We know there will always be somewhere between one and three extra
 *   descriptors uploaded.
 *
 * - We know the desired received frame will always be at the end of the
 *   total data upload.
 *
 * - We know the size of the desired received frame because it will be
 *   provided in the length field of the status word in the last descriptor.
 *
 * Here's what we do:
 *
 * - When we allocate buffers for the receive ring, we bzero() them.
 *   This means that we know that the buffer contents should be all
 *   zeros, except for data uploaded by the chip.
 *
 * - We also force the PNIC chip to upload frames that include the
 *   ethernet CRC at the end.
 *
 * - We gather all of the bogus frame data into a single buffer.
 *
 * - We then position a pointer at the end of this buffer and scan
 *   backwards until we encounter the first non-zero byte of data.
 *   This is the end of the received frame. We know we will encounter
 *   some data at the end of the frame because the CRC will always be
 *   there, so even if the sender transmits a packet of all zeros,
 *   we won't be fooled.
 *
 * - We know the size of the actual received frame, so we subtract
 *   that value from the current pointer location. This brings us
 *   to the start of the actual received packet.
 *
 * - We copy this into an mbuf and pass it on, along with the actual
 *   frame length.
 *
 * The performance hit is tremendous, but it beats dropping frames all
 * the time.
 */

#define DC_WHOLEFRAME	(DC_RXSTAT_FIRSTFRAG|DC_RXSTAT_LASTFRAG)
void
dc_pnic_rx_bug_war(sc, idx)
	struct dc_softc *sc;
	int idx;
{
	struct dc_desc		*cur_rx;
	struct dc_desc		*c = NULL;
	struct mbuf		*m = NULL;
	unsigned char		*ptr;
	int			i, total_len;
	u_int32_t		rxstat = 0;

	i = sc->dc_pnic_rx_bug_save;
	cur_rx = &sc->dc_ldata->dc_rx_list[idx];
	ptr = sc->dc_pnic_rx_buf;
	bzero(ptr, DC_RXLEN * 5);

	/* Copy all the bytes from the bogus buffers. */
	while (1) {
		c = &sc->dc_ldata->dc_rx_list[i];
		rxstat = letoh32(c->dc_status);
		m = sc->dc_cdata.dc_rx_chain[i].sd_mbuf;
		bcopy(mtod(m, char *), ptr, DC_RXLEN);
		ptr += DC_RXLEN;
		/* If this is the last buffer, break out. */
		if (i == idx || rxstat & DC_RXSTAT_LASTFRAG)
			break;
		dc_newbuf(sc, i, m);
		DC_INC(i, DC_RX_LIST_CNT);
	}

	/* Find the length of the actual receive frame. */
	total_len = DC_RXBYTES(rxstat);

	/* Scan backwards until we hit a non-zero byte. */
	while(*ptr == 0x00)
		ptr--;

	/* Round off. */
	if ((unsigned long)(ptr) & 0x3)
		ptr -= 1;

	/* Now find the start of the frame. */
	ptr -= total_len;
	if (ptr < sc->dc_pnic_rx_buf)
		ptr = sc->dc_pnic_rx_buf;

	/*
	 * Now copy the salvaged frame to the last mbuf and fake up
	 * the status word to make it look like a successful
 	 * frame reception.
	 */
	dc_newbuf(sc, i, m);
	bcopy(ptr, mtod(m, char *), total_len);	
	cur_rx->dc_status = htole32(rxstat | DC_RXSTAT_FIRSTFRAG);
}

/*
 * This routine searches the RX ring for dirty descriptors in the
 * event that the rxeof routine falls out of sync with the chip's
 * current descriptor pointer. This may happen sometimes as a result
 * of a "no RX buffer available" condition that happens when the chip
 * consumes all of the RX buffers before the driver has a chance to
 * process the RX ring. This routine may need to be called more than
 * once to bring the driver back in sync with the chip, however we
 * should still be getting RX DONE interrupts to drive the search
 * for new packets in the RX ring, so we should catch up eventually.
 */
int
dc_rx_resync(sc)
	struct dc_softc *sc;
{
	u_int32_t stat;
	int i, pos, offset;

	pos = sc->dc_cdata.dc_rx_prod;

	for (i = 0; i < DC_RX_LIST_CNT; i++) {

		offset = offsetof(struct dc_list_data, dc_rx_list[pos]);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
		    offset, sizeof(struct dc_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		stat = sc->dc_ldata->dc_rx_list[pos].dc_status;
		if (!(stat & htole32(DC_RXSTAT_OWN)))
			break;
		DC_INC(pos, DC_RX_LIST_CNT);
	}

	/* If the ring really is empty, then just return. */
	if (i == DC_RX_LIST_CNT)
		return (0);

	/* We've fallen behind the chip: catch it. */
	sc->dc_cdata.dc_rx_prod = pos;

	return (EAGAIN);
}

/*
 * A frame has been uploaded: pass the resulting mbuf chain up to
 * the higher level protocols.
 */
void
dc_rxeof(sc)
	struct dc_softc *sc;
{
	struct mbuf *m;
	struct ifnet *ifp;
	struct dc_desc *cur_rx;
	int i, offset, total_len = 0;
	u_int32_t rxstat;

	ifp = &sc->sc_arpcom.ac_if;
	i = sc->dc_cdata.dc_rx_prod;

	for(;;) {
		struct mbuf	*m0 = NULL;

		offset = offsetof(struct dc_list_data, dc_rx_list[i]);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
		    offset, sizeof(struct dc_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cur_rx = &sc->dc_ldata->dc_rx_list[i];
		rxstat = letoh32(cur_rx->dc_status);
		if (rxstat & DC_RXSTAT_OWN)
			break;

		m = sc->dc_cdata.dc_rx_chain[i].sd_mbuf;
		total_len = DC_RXBYTES(rxstat);

		bus_dmamap_sync(sc->sc_dmat, sc->dc_cdata.dc_rx_chain[i].sd_map,
		    0, sc->dc_cdata.dc_rx_chain[i].sd_map->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);

		if (sc->dc_flags & DC_PNIC_RX_BUG_WAR) {
			if ((rxstat & DC_WHOLEFRAME) != DC_WHOLEFRAME) {
				if (rxstat & DC_RXSTAT_FIRSTFRAG)
					sc->dc_pnic_rx_bug_save = i;
				if ((rxstat & DC_RXSTAT_LASTFRAG) == 0) {
					DC_INC(i, DC_RX_LIST_CNT);
					continue;
				}
				dc_pnic_rx_bug_war(sc, i);
				rxstat = letoh32(cur_rx->dc_status);
				total_len = DC_RXBYTES(rxstat);
			}
		}

		sc->dc_cdata.dc_rx_chain[i].sd_mbuf = NULL;

		/*
		 * If an error occurs, update stats, clear the
		 * status word and leave the mbuf cluster in place:
		 * it should simply get re-used next time this descriptor
	 	 * comes up in the ring.
		 */
		if (rxstat & DC_RXSTAT_RXERR
#if NVLAN > 0
		/*
		 * If VLANs are enabled, allow frames up to 4 bytes
		 * longer than the MTU. This should really check if
		 * the giant packet has a vlan tag
		 */
		 && ((rxstat & (DC_RXSTAT_GIANT|DC_RXSTAT_LASTFRAG)) == 0
		 && total_len <= ifp->if_mtu + 4) 
#endif
		    ) {
			ifp->if_ierrors++;
			if (rxstat & DC_RXSTAT_COLLSEEN)
				ifp->if_collisions++;
			dc_newbuf(sc, i, m);
			if (rxstat & DC_RXSTAT_CRCERR) {
				DC_INC(i, DC_RX_LIST_CNT);
				continue;
			} else {
				dc_init(sc);
				return;
			}
		}

		/* No errors; receive the packet. */	
		total_len -= ETHER_CRC_LEN;

		m->m_pkthdr.rcvif = ifp;
		m0 = m_devget(mtod(m, char *) - ETHER_ALIGN,
		    total_len + ETHER_ALIGN, 0, ifp, NULL);
		dc_newbuf(sc, i, m);
		DC_INC(i, DC_RX_LIST_CNT);
		if (m0 == NULL) {
			ifp->if_ierrors++;
			continue;
		}
		m_adj(m0, ETHER_ALIGN);
		m = m0;

		ifp->if_ipackets++;
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m);
#endif
		ether_input_mbuf(ifp, m);
	}

	sc->dc_cdata.dc_rx_prod = i;
}

/*
 * A frame was downloaded to the chip. It's safe for us to clean up
 * the list buffers.
 */

void
dc_txeof(sc)
	struct dc_softc *sc;
{
	struct dc_desc *cur_tx = NULL;
	struct ifnet *ifp;
	int idx, offset;

	ifp = &sc->sc_arpcom.ac_if;

	/* Clear the timeout timer. */
	ifp->if_timer = 0;

	/*
	 * Go through our tx list and free mbufs for those
	 * frames that have been transmitted.
	 */
	idx = sc->dc_cdata.dc_tx_cons;
	while(idx != sc->dc_cdata.dc_tx_prod) {
		u_int32_t		txstat;

		offset = offsetof(struct dc_list_data, dc_tx_list[idx]);
		bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
		    offset, sizeof(struct dc_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		cur_tx = &sc->dc_ldata->dc_tx_list[idx];
		txstat = letoh32(cur_tx->dc_status);

		if (txstat & DC_TXSTAT_OWN)
			break;

		if (!(cur_tx->dc_ctl & htole32(DC_TXCTL_LASTFRAG)) ||
		    cur_tx->dc_ctl & htole32(DC_TXCTL_SETUP)) {
			sc->dc_cdata.dc_tx_cnt--;
			if (cur_tx->dc_ctl & htole32(DC_TXCTL_SETUP)) {
				/*
				 * Yes, the PNIC is so brain damaged
				 * that it will sometimes generate a TX
				 * underrun error while DMAing the RX
				 * filter setup frame. If we detect this,
				 * we have to send the setup frame again,
				 * or else the filter won't be programmed
				 * correctly.
				 */
				if (DC_IS_PNIC(sc)) {
					if (txstat & DC_TXSTAT_ERRSUM)
						dc_setfilt(sc);
				}
				sc->dc_cdata.dc_tx_chain[idx].sd_mbuf = NULL;
			}
			DC_INC(idx, DC_TX_LIST_CNT);
			continue;
		}

		if (DC_IS_XIRCOM(sc) || DC_IS_CONEXANT(sc)) {
			/*
			 * XXX: Why does my Xircom taunt me so?
			 * For some reason it likes setting the CARRLOST flag
			 * even when the carrier is there. wtf?!
			 * Who knows, but Conexant chips have the
			 * same problem. Maybe they took lessons
			 * from Xircom.
			 */
			if (/*sc->dc_type == DC_TYPE_21143 &&*/
			    sc->dc_pmode == DC_PMODE_MII &&
			    ((txstat & 0xFFFF) & ~(DC_TXSTAT_ERRSUM|
			    DC_TXSTAT_NOCARRIER)))
				txstat &= ~DC_TXSTAT_ERRSUM;
		} else {
			if (/*sc->dc_type == DC_TYPE_21143 &&*/
			    sc->dc_pmode == DC_PMODE_MII &&
		    	    ((txstat & 0xFFFF) & ~(DC_TXSTAT_ERRSUM|
		    	    DC_TXSTAT_NOCARRIER|DC_TXSTAT_CARRLOST)))
				txstat &= ~DC_TXSTAT_ERRSUM;
		}

		if (txstat & DC_TXSTAT_ERRSUM) {
			ifp->if_oerrors++;
			if (txstat & DC_TXSTAT_EXCESSCOLL)
				ifp->if_collisions++;
			if (txstat & DC_TXSTAT_LATECOLL)
				ifp->if_collisions++;
			if (!(txstat & DC_TXSTAT_UNDERRUN)) {
				dc_init(sc);
				return;
			}
		}

		ifp->if_collisions += (txstat & DC_TXSTAT_COLLCNT) >> 3;

		ifp->if_opackets++;
		if (sc->dc_cdata.dc_tx_chain[idx].sd_map->dm_nsegs != 0) {
			bus_dmamap_t map = sc->dc_cdata.dc_tx_chain[idx].sd_map;

			bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, map);
		}
		if (sc->dc_cdata.dc_tx_chain[idx].sd_mbuf != NULL) {
			m_freem(sc->dc_cdata.dc_tx_chain[idx].sd_mbuf);
			sc->dc_cdata.dc_tx_chain[idx].sd_mbuf = NULL;
		}

		bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
		    offset, sizeof(struct dc_desc),
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);

		sc->dc_cdata.dc_tx_cnt--;
		DC_INC(idx, DC_TX_LIST_CNT);
	}

	sc->dc_cdata.dc_tx_cons = idx;
	if (cur_tx != NULL)
		ifp->if_flags &= ~IFF_OACTIVE;
}

void
dc_tick(xsc)
	void *xsc;
{
	struct dc_softc *sc = (struct dc_softc *)xsc;
	struct mii_data *mii;
	struct ifnet *ifp;
	int s;
	u_int32_t r;

	s = splimp();

	ifp = &sc->sc_arpcom.ac_if;
	mii = &sc->sc_mii;

	if (sc->dc_flags & DC_REDUCED_MII_POLL) {
		if (sc->dc_flags & DC_21143_NWAY) {
			r = CSR_READ_4(sc, DC_10BTSTAT);
			if (IFM_SUBTYPE(mii->mii_media_active) ==
			    IFM_100_TX && (r & DC_TSTAT_LS100)) {
				sc->dc_link = 0;
				mii_mediachg(mii);
			}
			if (IFM_SUBTYPE(mii->mii_media_active) ==
			    IFM_10_T && (r & DC_TSTAT_LS10)) {
				sc->dc_link = 0;
				mii_mediachg(mii);
			}
			if (sc->dc_link == 0)
				mii_tick(mii);
		} else {
			r = CSR_READ_4(sc, DC_ISR);
			if ((r & DC_ISR_RX_STATE) == DC_RXSTATE_WAIT &&
			    sc->dc_cdata.dc_tx_cnt == 0 && !DC_IS_ASIX(sc))
				mii_tick(mii);
			if (!(mii->mii_media_status & IFM_ACTIVE))
				sc->dc_link = 0;
		}
	} else
		mii_tick(mii);

	/*
	 * When the init routine completes, we expect to be able to send
	 * packets right away, and in fact the network code will send a
	 * gratuitous ARP the moment the init routine marks the interface
	 * as running. However, even though the MAC may have been initialized,
	 * there may be a delay of a few seconds before the PHY completes
	 * autonegotiation and the link is brought up. Any transmissions
	 * made during that delay will be lost. Dealing with this is tricky:
	 * we can't just pause in the init routine while waiting for the
	 * PHY to come ready since that would bring the whole system to
	 * a screeching halt for several seconds.
	 *
	 * What we do here is prevent the TX start routine from sending
	 * any packets until a link has been established. After the
	 * interface has been initialized, the tick routine will poll
	 * the state of the PHY until the IFM_ACTIVE flag is set. Until
	 * that time, packets will stay in the send queue, and once the
	 * link comes up, they will be flushed out to the wire.
	 */
	if (!sc->dc_link) {
		mii_pollstat(mii);
		if (mii->mii_media_status & IFM_ACTIVE &&
		    IFM_SUBTYPE(mii->mii_media_active) != IFM_NONE) {
			sc->dc_link++;
			if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
				dc_start(ifp);
		}
	}

	if (sc->dc_flags & DC_21143_NWAY && !sc->dc_link)
		timeout_add(&sc->dc_tick_tmo, hz / 10);
	else
		timeout_add(&sc->dc_tick_tmo, hz);

	splx(s);
}

int
dc_intr(arg)
	void *arg;
{
	struct dc_softc *sc;
	struct ifnet *ifp;
	u_int32_t status;
	int claimed = 0;

	sc = arg;
	ifp = &sc->sc_arpcom.ac_if;

	/* Supress unwanted interrupts */
	if (!(ifp->if_flags & IFF_UP)) {
		if (CSR_READ_4(sc, DC_ISR) & DC_INTRS)
			dc_stop(sc);
		return (claimed);
	}

	/* Disable interrupts. */
	CSR_WRITE_4(sc, DC_IMR, 0x00000000);

	while(((status = CSR_READ_4(sc, DC_ISR)) & DC_INTRS) &&
	    status != 0xFFFFFFFF) {

		claimed = 1;
		CSR_WRITE_4(sc, DC_ISR, status);

		if (status & DC_ISR_RX_OK) {
			int		curpkts;
			curpkts = ifp->if_ipackets;
			dc_rxeof(sc);
			if (curpkts == ifp->if_ipackets) {
				while(dc_rx_resync(sc))
					dc_rxeof(sc);
			}
		}

		if (status & (DC_ISR_TX_OK|DC_ISR_TX_NOBUF))
			dc_txeof(sc);

		if (status & DC_ISR_TX_IDLE) {
			dc_txeof(sc);
			if (sc->dc_cdata.dc_tx_cnt) {
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON);
				CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);
			}
		}

		if (status & DC_ISR_TX_UNDERRUN) {
			u_int32_t		cfg;

			if (DC_IS_DAVICOM(sc) || DC_IS_INTEL(sc))
				dc_init(sc);
			cfg = CSR_READ_4(sc, DC_NETCFG);
			cfg &= ~DC_NETCFG_TX_THRESH;
			if (sc->dc_txthresh == DC_TXTHRESH_160BYTES) {
				DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
			} else if (sc->dc_flags & DC_TX_STORENFWD) {
			} else {
				sc->dc_txthresh += 0x4000;
				CSR_WRITE_4(sc, DC_NETCFG, cfg);
				DC_SETBIT(sc, DC_NETCFG, sc->dc_txthresh);
				DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
			}
		}

		if ((status & DC_ISR_RX_WATDOGTIMEO)
		    || (status & DC_ISR_RX_NOBUF)) {
			int		curpkts;
			curpkts = ifp->if_ipackets;
			dc_rxeof(sc);
			if (curpkts == ifp->if_ipackets) {
				while(dc_rx_resync(sc))
					dc_rxeof(sc);
			}
		}

		if (status & DC_ISR_BUS_ERR) {
			dc_reset(sc);
			dc_init(sc);
		}
	}

	/* Re-enable interrupts. */
	CSR_WRITE_4(sc, DC_IMR, DC_INTRS);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		dc_start(ifp);

	return (claimed);
}

/*
 * Encapsulate an mbuf chain in a descriptor by coupling the mbuf data
 * pointers to the fragment pointers.
 */
int
dc_encap(sc, m_head, txidx)
	struct dc_softc *sc;
	struct mbuf *m_head;
	u_int32_t *txidx;
{
	struct dc_desc *f = NULL;
	int frag, cur, cnt = 0, i;
	bus_dmamap_t map;

	/*
 	 * Start packing the mbufs in this chain into
	 * the fragment pointers. Stop when we run out
 	 * of fragments or hit the end of the mbuf chain.
	 */
	map = sc->sc_tx_sparemap;

	if (bus_dmamap_load_mbuf(sc->sc_dmat, map,
	    m_head, BUS_DMA_NOWAIT) != 0)
		return (ENOBUFS);

	cur = frag = *txidx;

	for (i = 0; i < map->dm_nsegs; i++) {
		if (sc->dc_flags & DC_TX_ADMTEK_WAR) {
			if (*txidx != sc->dc_cdata.dc_tx_prod &&
			    frag == (DC_TX_LIST_CNT - 1)) {
				bus_dmamap_unload(sc->sc_dmat, map);
				return (ENOBUFS);
			}
		}
		if ((DC_TX_LIST_CNT -
		    (sc->dc_cdata.dc_tx_cnt + cnt)) < 5) {
			bus_dmamap_unload(sc->sc_dmat, map);
			return (ENOBUFS);
		}

		f = &sc->dc_ldata->dc_tx_list[frag];
		f->dc_ctl = htole32(DC_TXCTL_TLINK | map->dm_segs[i].ds_len);
		if (cnt == 0) {
			f->dc_status = htole32(0);
			f->dc_ctl |= htole32(DC_TXCTL_FIRSTFRAG);
		} else
			f->dc_status = htole32(DC_TXSTAT_OWN);
		f->dc_data = htole32(map->dm_segs[i].ds_addr);
		cur = frag;
		DC_INC(frag, DC_TX_LIST_CNT);
		cnt++;
	}

	sc->dc_cdata.dc_tx_cnt += cnt;
	sc->dc_cdata.dc_tx_chain[cur].sd_mbuf = m_head;
	sc->sc_tx_sparemap = sc->dc_cdata.dc_tx_chain[cur].sd_map;
	sc->dc_cdata.dc_tx_chain[cur].sd_map = map;
	sc->dc_ldata->dc_tx_list[cur].dc_ctl |= htole32(DC_TXCTL_LASTFRAG);
	if (sc->dc_flags & DC_TX_INTR_FIRSTFRAG)
		sc->dc_ldata->dc_tx_list[*txidx].dc_ctl |=
		    htole32(DC_TXCTL_FINT);
	if (sc->dc_flags & DC_TX_INTR_ALWAYS)
		sc->dc_ldata->dc_tx_list[cur].dc_ctl |=
		    htole32(DC_TXCTL_FINT);
	if (sc->dc_flags & DC_TX_USE_TX_INTR && sc->dc_cdata.dc_tx_cnt > 64)
		sc->dc_ldata->dc_tx_list[cur].dc_ctl |=
		    htole32(DC_TXCTL_FINT);
#ifdef ALTQ
	else if ((sc->dc_flags & DC_TX_USE_TX_INTR) &&
		 TBR_IS_ENABLED(&sc->sc_arpcom.ac_if.if_snd))
		sc->dc_ldata->dc_tx_list[cur].dc_ctl |=
		    htole32(DC_TXCTL_FINT);
#endif
	bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	sc->dc_ldata->dc_tx_list[*txidx].dc_status = htole32(DC_TXSTAT_OWN);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    offsetof(struct dc_list_data, dc_tx_list[*txidx]),
	    sizeof(struct dc_desc) * cnt,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	*txidx = frag;

	return (0);
}

/*
 * Coalesce an mbuf chain into a single mbuf cluster buffer.
 * Needed for some really badly behaved chips that just can't
 * do scatter/gather correctly.
 */
int
dc_coal(sc, m_head)
	struct dc_softc *sc;
	struct mbuf **m_head;
{
	struct mbuf		*m_new, *m;

	m = *m_head;
	MGETHDR(m_new, M_DONTWAIT, MT_DATA);
	if (m_new == NULL) {
		printf("%s: no memory for tx list", sc->sc_dev.dv_xname);
		return (ENOBUFS);
	}
	if (m->m_pkthdr.len > MHLEN) {
		MCLGET(m_new, M_DONTWAIT);
		if (!(m_new->m_flags & M_EXT)) {
			m_freem(m_new);
			printf("%s: no memory for tx list",
			    sc->sc_dev.dv_xname);
			return (ENOBUFS);
		}
	}
	m_copydata(m, 0, m->m_pkthdr.len, mtod(m_new, caddr_t));
	m_new->m_pkthdr.len = m_new->m_len = m->m_pkthdr.len;
	m_freem(m);
	*m_head = m_new;

	return (0);
}

/*
 * Main transmit routine. To avoid having to do mbuf copies, we put pointers
 * to the mbuf data regions directly in the transmit lists. We also save a
 * copy of the pointers since the transmit list fragment pointers are
 * physical addresses.
 */

void
dc_start(ifp)
	struct ifnet *ifp;
{
	struct dc_softc *sc;
	struct mbuf *m_head = NULL;
	int idx;

	sc = ifp->if_softc;

	if (!sc->dc_link)
		return;

	if (ifp->if_flags & IFF_OACTIVE)
		return;

	idx = sc->dc_cdata.dc_tx_prod;

	while(sc->dc_cdata.dc_tx_chain[idx].sd_mbuf == NULL) {
		IFQ_POLL(&ifp->if_snd, m_head);
		if (m_head == NULL)
			break;

		if (sc->dc_flags & DC_TX_COALESCE) {
#ifdef ALTQ
			/* note: dc_coal breaks the poll-and-dequeue rule.
			 * if dc_coal fails, we lose the packet.
			 */
#endif
			IFQ_DEQUEUE(&ifp->if_snd, m_head);
			if (dc_coal(sc, &m_head)) {
				ifp->if_flags |= IFF_OACTIVE;
				break;
			}
		}

		if (dc_encap(sc, m_head, &idx)) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}

		/* now we are committed to transmit the packet */
		if (sc->dc_flags & DC_TX_COALESCE) {
			/* if mbuf is coalesced, it is already dequeued */
		} else
			IFQ_DEQUEUE(&ifp->if_snd, m_head);

		/*
		 * If there's a BPF listener, bounce a copy of this frame
		 * to him.
		 */
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m_head);
#endif
		if (sc->dc_flags & DC_TX_ONE) {
			ifp->if_flags |= IFF_OACTIVE;
			break;
		}
	}
	if (idx == sc->dc_cdata.dc_tx_prod)
		return;

	/* Transmit */
	sc->dc_cdata.dc_tx_prod = idx;
	if (!(sc->dc_flags & DC_TX_POLL))
		CSR_WRITE_4(sc, DC_TXSTART, 0xFFFFFFFF);

	/*
	 * Set a timeout in case the chip goes out to lunch.
	 */
	ifp->if_timer = 5;
}

void
dc_init(xsc)
	void *xsc;
{
	struct dc_softc *sc = xsc;
	struct ifnet *ifp = &sc->sc_arpcom.ac_if;
	struct mii_data *mii;
	int s;

	s = splimp();

	mii = &sc->sc_mii;

	/*
	 * Cancel pending I/O and free all RX/TX buffers.
	 */
	dc_stop(sc);
	dc_reset(sc);

	/*
	 * Set cache alignment and burst length.
	 */
	if (DC_IS_ASIX(sc) || DC_IS_DAVICOM(sc))
		CSR_WRITE_4(sc, DC_BUSCTL, 0);
	else
		CSR_WRITE_4(sc, DC_BUSCTL, DC_BUSCTL_MRME|DC_BUSCTL_MRLE);
	if (DC_IS_DAVICOM(sc) || DC_IS_INTEL(sc)) {
		DC_SETBIT(sc, DC_BUSCTL, DC_BURSTLEN_USECA);
	} else {
		DC_SETBIT(sc, DC_BUSCTL, DC_BURSTLEN_16LONG);
	}
	if (sc->dc_flags & DC_TX_POLL)
		DC_SETBIT(sc, DC_BUSCTL, DC_TXPOLL_1);
	switch(sc->dc_cachesize) {
	case 32:
		DC_SETBIT(sc, DC_BUSCTL, DC_CACHEALIGN_32LONG);
		break;
	case 16:
		DC_SETBIT(sc, DC_BUSCTL, DC_CACHEALIGN_16LONG);
		break; 
	case 8:
		DC_SETBIT(sc, DC_BUSCTL, DC_CACHEALIGN_8LONG);
		break;  
	case 0:
	default:
		DC_SETBIT(sc, DC_BUSCTL, DC_CACHEALIGN_NONE);
		break;
	}

	if (sc->dc_flags & DC_TX_STORENFWD)
		DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
	else {
		if (sc->dc_txthresh == DC_TXTHRESH_160BYTES) {
			DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
		} else {
			DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_STORENFWD);
			DC_SETBIT(sc, DC_NETCFG, sc->dc_txthresh);
		}
	}

	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_NO_RXCRC);
	DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_TX_BACKOFF);

	if (DC_IS_MACRONIX(sc) || DC_IS_PNICII(sc)) {
		/*
		 * The app notes for the 98713 and 98715A say that
		 * in order to have the chips operate properly, a magic
		 * number must be written to CSR16. Macronix does not
		 * document the meaning of these bits so there's no way
		 * to know exactly what they do. The 98713 has a magic
		 * number all its own; the rest all use a different one.
		 */
		DC_CLRBIT(sc, DC_MX_MAGICPACKET, 0xFFFF0000);
		if (sc->dc_type == DC_TYPE_98713)
			DC_SETBIT(sc, DC_MX_MAGICPACKET, DC_MX_MAGIC_98713);
		else
			DC_SETBIT(sc, DC_MX_MAGICPACKET, DC_MX_MAGIC_98715);
	}

	if (DC_IS_XIRCOM(sc)) {
		CSR_WRITE_4(sc, DC_SIAGP, DC_SIAGP_WRITE_EN | DC_SIAGP_INT1_EN |
		    DC_SIAGP_MD_GP2_OUTPUT | DC_SIAGP_MD_GP0_OUTPUT);
		DELAY(10);
		CSR_WRITE_4(sc, DC_SIAGP, DC_SIAGP_INT1_EN |
		    DC_SIAGP_MD_GP2_OUTPUT | DC_SIAGP_MD_GP0_OUTPUT);
		DELAY(10);
	}

	DC_CLRBIT(sc, DC_NETCFG, DC_NETCFG_TX_THRESH);
	DC_SETBIT(sc, DC_NETCFG, DC_TXTHRESH_72BYTES);

	/* Init circular RX list. */
	if (dc_list_rx_init(sc) == ENOBUFS) {
		printf("%s: initialization failed: no "
		    "memory for rx buffers\n", sc->sc_dev.dv_xname);
		dc_stop(sc);
		splx(s);
		return;
	}

	/*
	 * Init tx descriptors.
	 */
	dc_list_tx_init(sc);

	/*
	 * Sync down both lists initialized.
	 */
	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    0, sc->sc_listmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	/*
	 * Load the address of the RX list.
	 */
	CSR_WRITE_4(sc, DC_RXADDR, sc->sc_listmap->dm_segs[0].ds_addr +
	    offsetof(struct dc_list_data, dc_rx_list[0]));
	CSR_WRITE_4(sc, DC_TXADDR, sc->sc_listmap->dm_segs[0].ds_addr +
	    offsetof(struct dc_list_data, dc_tx_list[0]));

	/*
	 * Enable interrupts.
	 */
	CSR_WRITE_4(sc, DC_IMR, DC_INTRS);
	CSR_WRITE_4(sc, DC_ISR, 0xFFFFFFFF);

	/* Enable transmitter. */
	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_TX_ON);

	/*
	 * If this is an Intel 21143 and we're not using the
	 * MII port, program the LED control pins so we get
	 * link and activity indications.
	 */
	if (sc->dc_flags & DC_TULIP_LEDS) {
		CSR_WRITE_4(sc, DC_WATCHDOG,
		    DC_WDOG_CTLWREN|DC_WDOG_LINK|DC_WDOG_ACTIVITY);
		CSR_WRITE_4(sc, DC_WATCHDOG, 0);
	}

	/*
	 * Load the RX/multicast filter. We do this sort of late
	 * because the filter programming scheme on the 21143 and
	 * some clones requires DMAing a setup frame via the TX
	 * engine, and we need the transmitter enabled for that.
	 */
	dc_setfilt(sc);

	/* Enable receiver. */
	DC_SETBIT(sc, DC_NETCFG, DC_NETCFG_RX_ON);
	CSR_WRITE_4(sc, DC_RXSTART, 0xFFFFFFFF);

	mii_mediachg(mii);
	dc_setcfg(sc, sc->dc_if_media);

	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	splx(s);

	timeout_set(&sc->dc_tick_tmo, dc_tick, sc);

	if (IFM_SUBTYPE(mii->mii_media.ifm_media) == IFM_HPNA_1)
		sc->dc_link = 1;
	else {
		if (sc->dc_flags & DC_21143_NWAY)
			timeout_add(&sc->dc_tick_tmo, hz / 10);
		else
			timeout_add(&sc->dc_tick_tmo, hz);
	}

#ifdef SRM_MEDIA
	if(sc->dc_srm_media) {
		struct ifreq ifr;

		ifr.ifr_media = sc->dc_srm_media;
		ifmedia_ioctl(ifp, &ifr, &mii->mii_media, SIOCSIFMEDIA);
		sc->dc_srm_media = 0;
	}
#endif
}

/*
 * Set media options.
 */
int
dc_ifmedia_upd(ifp)
	struct ifnet *ifp;
{
	struct dc_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;

	sc = ifp->if_softc;
	mii = &sc->sc_mii;
	mii_mediachg(mii);

	ifm = &mii->mii_media;

	if (DC_IS_DAVICOM(sc) &&
	    IFM_SUBTYPE(ifm->ifm_media) == IFM_HPNA_1)
		dc_setcfg(sc, ifm->ifm_media);
	else
		sc->dc_link = 0;

	return (0);
}

/*
 * Report current media status.
 */
void
dc_ifmedia_sts(ifp, ifmr)
	struct ifnet *ifp;
	struct ifmediareq *ifmr;
{
	struct dc_softc *sc;
	struct mii_data *mii;
	struct ifmedia *ifm;

	sc = ifp->if_softc;
	mii = &sc->sc_mii;
	mii_pollstat(mii);
	ifm = &mii->mii_media;
	if (DC_IS_DAVICOM(sc)) {
		if (IFM_SUBTYPE(ifm->ifm_media) == IFM_HPNA_1) {
			ifmr->ifm_active = ifm->ifm_media;
			ifmr->ifm_status = 0;
			return;
		}
	}
	ifmr->ifm_active = mii->mii_media_active;
	ifmr->ifm_status = mii->mii_media_status;
}

int
dc_ioctl(ifp, command, data)
	struct ifnet *ifp;
	u_long command;
	caddr_t data;
{
	struct dc_softc		*sc = ifp->if_softc;
	struct ifreq		*ifr = (struct ifreq *) data;
	struct ifaddr		*ifa = (struct ifaddr *)data;
	struct mii_data		*mii;
	int			s, error = 0;

	s = splimp();

	if ((error = ether_ioctl(ifp, &sc->sc_arpcom, command, data)) > 0) {
		splx(s);
		return (error);
	}

	switch(command) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
		switch (ifa->ifa_addr->sa_family) {
		case AF_INET:
			dc_init(sc);
			arp_ifinit(&sc->sc_arpcom, ifa);
			break;
		default:
			dc_init(sc);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if (ifp->if_flags & IFF_UP) {
			if (ifp->if_flags & IFF_RUNNING &&
			    ifp->if_flags & IFF_PROMISC &&
			    !(sc->dc_if_flags & IFF_PROMISC)) {
				dc_setfilt(sc);
			} else if (ifp->if_flags & IFF_RUNNING &&
			    !(ifp->if_flags & IFF_PROMISC) &&
			    sc->dc_if_flags & IFF_PROMISC) {
				dc_setfilt(sc);
			} else if (!(ifp->if_flags & IFF_RUNNING)) {
				sc->dc_txthresh = 0;
				dc_init(sc);
			}
		} else {
			if (ifp->if_flags & IFF_RUNNING)
				dc_stop(sc);
		}
		sc->dc_if_flags = ifp->if_flags;
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
			dc_setfilt(sc);
			error = 0;
		}
		break;
	case SIOCGIFMEDIA:
	case SIOCSIFMEDIA:
		mii = &sc->sc_mii;
		error = ifmedia_ioctl(ifp, ifr, &mii->mii_media, command);
#ifdef SRM_MEDIA
		if (sc->dc_srm_media)
			sc->dc_srm_media = 0;
#endif
		break;
	default:
		error = EINVAL;
		break;
	}

	splx(s);

	return (error);
}

void
dc_watchdog(ifp)
	struct ifnet *ifp;
{
	struct dc_softc *sc;

	sc = ifp->if_softc;

	ifp->if_oerrors++;
	printf("%s: watchdog timeout\n", sc->sc_dev.dv_xname);

	dc_stop(sc);
	dc_reset(sc);
	dc_init(sc);

	if (IFQ_IS_EMPTY(&ifp->if_snd) == 0)
		dc_start(ifp);
}

/*
 * Stop the adapter and free any mbufs allocated to the
 * RX and TX lists.
 */
void
dc_stop(sc)
	struct dc_softc *sc;
{
	struct ifnet *ifp;
	int i;

	ifp = &sc->sc_arpcom.ac_if;
	ifp->if_timer = 0;

	timeout_del(&sc->dc_tick_tmo);

	DC_CLRBIT(sc, DC_NETCFG, (DC_NETCFG_RX_ON|DC_NETCFG_TX_ON));
	CSR_WRITE_4(sc, DC_IMR, 0x00000000);
	CSR_WRITE_4(sc, DC_TXADDR, 0x00000000);
	CSR_WRITE_4(sc, DC_RXADDR, 0x00000000);
	sc->dc_link = 0;

	/*
	 * Free data in the RX lists.
	 */
	for (i = 0; i < DC_RX_LIST_CNT; i++) {
		if (sc->dc_cdata.dc_rx_chain[i].sd_map->dm_nsegs != 0) {
			bus_dmamap_t map = sc->dc_cdata.dc_rx_chain[i].sd_map;

			bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
			    BUS_DMASYNC_POSTREAD);
			bus_dmamap_unload(sc->sc_dmat, map);
		}
		if (sc->dc_cdata.dc_rx_chain[i].sd_mbuf != NULL) {
			m_freem(sc->dc_cdata.dc_rx_chain[i].sd_mbuf);
			sc->dc_cdata.dc_rx_chain[i].sd_mbuf = NULL;
		}
	}
	bzero((char *)&sc->dc_ldata->dc_rx_list,
		sizeof(sc->dc_ldata->dc_rx_list));

	/*
	 * Free the TX list buffers.
	 */
	for (i = 0; i < DC_TX_LIST_CNT; i++) {
		if (sc->dc_cdata.dc_tx_chain[i].sd_map->dm_nsegs != 0) {
			bus_dmamap_t map = sc->dc_cdata.dc_tx_chain[i].sd_map;

			bus_dmamap_sync(sc->sc_dmat, map, 0, map->dm_mapsize,
			    BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, map);
		}
		if (sc->dc_cdata.dc_tx_chain[i].sd_mbuf != NULL) {
			if (sc->dc_ldata->dc_tx_list[i].dc_ctl &
			    htole32(DC_TXCTL_SETUP)) {
				sc->dc_cdata.dc_tx_chain[i].sd_mbuf = NULL;
				continue;
			}
			m_freem(sc->dc_cdata.dc_tx_chain[i].sd_mbuf);
			sc->dc_cdata.dc_tx_chain[i].sd_mbuf = NULL;
		}
	}
	bzero((char *)&sc->dc_ldata->dc_tx_list,
		sizeof(sc->dc_ldata->dc_tx_list));

	bus_dmamap_sync(sc->sc_dmat, sc->sc_listmap,
	    0, sc->sc_listmap->dm_mapsize,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
}

/*
 * Stop all chip I/O so that the kernel's probe routines don't
 * get confused by errant DMAs when rebooting.
 */
void
dc_shutdown(v)
	void *v;
{
	struct dc_softc *sc = (struct dc_softc *)v;

	dc_stop(sc);
}

struct cfdriver dc_cd = {
	0, "dc", DV_IFNET
};
