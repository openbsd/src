/*	$OpenBSD: rtwphyio.c,v 1.1 2004/12/29 01:02:31 jsg Exp $	*/
/* $NetBSD: rtwphyio.c,v 1.4 2004/12/25 06:58:37 dyoung Exp $ */
/*-
 * Copyright (c) 2004, 2005 David Young.  All rights reserved.
 *
 * Programmed for NetBSD by David Young.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of David Young may not be used to endorse or promote
 *    products derived from this software without specific prior
 *    written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY David Young ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
 * PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL David
 * Young BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY
 * OF SUCH DAMAGE.
 */
/*
 * Control input/output with the Philips SA2400 RF front-end and
 * the baseband processor built into the Realtek RTL8180.
 */

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/socket.h>
#include <sys/systm.h>
#include <sys/types.h>

#include <machine/bus.h>

#include <net/if.h>
#include <net/if_media.h>
#include <netinet/in.h>
#include <netinet/if_ether.h>

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_compat.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/rtwreg.h>
#include <dev/ic/max2820reg.h>
#include <dev/ic/sa2400reg.h>
#include <dev/ic/si4136reg.h>
#include <dev/ic/rtwvar.h>
#include <dev/ic/rtwphyio.h>
#include <dev/ic/rtwphy.h>

u_int32_t rtw_grf5101_host_crypt(u_int, u_int32_t);
u_int32_t rtw_maxim_swizzle(u_int, uint32_t);
u_int32_t rtw_grf5101_mac_crypt(u_int, u_int32_t);

static int rtw_macbangbits_timeout = 100;

u_int8_t
rtw_bbp_read(struct rtw_regs *regs, u_int addr)
{
	KASSERT((addr & ~PRESHIFT(RTW_BB_ADDR_MASK)) == 0);
	RTW_WRITE(regs, RTW_BB,
	    LSHIFT(addr, RTW_BB_ADDR_MASK) | RTW_BB_RD_MASK | RTW_BB_WR_MASK);
	delay(10);	/* XXX */
	RTW_WBR(regs, RTW_BB, RTW_BB);
	return MASK_AND_RSHIFT(RTW_READ(regs, RTW_BB), RTW_BB_RD_MASK);
}

int
rtw_bbp_write(struct rtw_regs *regs, u_int addr, u_int val)
{
#define	BBP_WRITE_ITERS	50
#define	BBP_WRITE_DELAY	1
	int i;
	u_int32_t wrbbp, rdbbp;

	RTW_DPRINTF(RTW_DEBUG_PHYIO,
	    ("%s: bbp[%u] <- %u\n", __func__, addr, val));

	KASSERT((addr & ~PRESHIFT(RTW_BB_ADDR_MASK)) == 0);
	KASSERT((val & ~PRESHIFT(RTW_BB_WR_MASK)) == 0);

	wrbbp = LSHIFT(addr, RTW_BB_ADDR_MASK) | RTW_BB_WREN |
	    LSHIFT(val, RTW_BB_WR_MASK) | RTW_BB_RD_MASK,

	rdbbp = LSHIFT(addr, RTW_BB_ADDR_MASK) |
	    RTW_BB_WR_MASK | RTW_BB_RD_MASK;

	RTW_DPRINTF(RTW_DEBUG_PHYIO,
	    ("%s: rdbbp = %#08x, wrbbp = %#08x\n", __func__, rdbbp, wrbbp));

	for (i = BBP_WRITE_ITERS; --i >= 0; ) {
		RTW_RBW(regs, RTW_BB, RTW_BB);
		RTW_WRITE(regs, RTW_BB, wrbbp);
		RTW_SYNC(regs, RTW_BB, RTW_BB);
		RTW_WRITE(regs, RTW_BB, rdbbp);
		RTW_SYNC(regs, RTW_BB, RTW_BB);
		delay(BBP_WRITE_DELAY);	/* 1 microsecond */
		if (MASK_AND_RSHIFT(RTW_READ(regs, RTW_BB),
		                    RTW_BB_RD_MASK) == val) {
			RTW_DPRINTF(RTW_DEBUG_PHYIO,
			    ("%s: finished in %dus\n", __func__,
			    BBP_WRITE_DELAY * (BBP_WRITE_ITERS - i)));
			return 0;
		}
		delay(BBP_WRITE_DELAY);	/* again */
	}
	printf("%s: timeout\n", __func__);
	return -1;
}

/* Help rtw_rf_hostwrite bang bits to RF over 3-wire interface. */
static __inline void
rtw_rf_hostbangbits(struct rtw_regs *regs, u_int32_t bits, int lo_to_hi,
    u_int nbits)
{
	int i;
	u_int32_t mask, reg;

	KASSERT(nbits <= 32);

	RTW_DPRINTF(RTW_DEBUG_PHYIO,
	    ("%s: %u bits, %#08x, %s\n", __func__, nbits, bits,
	    (lo_to_hi) ? "lo to hi" : "hi to lo"));

	reg = RTW_PHYCFG_HST;
	RTW_WRITE(regs, RTW_PHYCFG, reg);
	RTW_SYNC(regs, RTW_PHYCFG, RTW_PHYCFG);

	if (lo_to_hi)
		mask = 0x1;
	else
		mask = 1 << (nbits - 1);

	for (i = 0; i < nbits; i++) {
		RTW_DPRINTF(RTW_DEBUG_PHYBITIO,
		    ("%s: bits %#08x mask %#08x -> bit %#08x\n",
		    __func__, bits, mask, bits & mask));

		if ((bits & mask) != 0)
			reg |= RTW_PHYCFG_HST_DATA;
		else
			reg &= ~RTW_PHYCFG_HST_DATA;

		reg |= RTW_PHYCFG_HST_CLK;
		RTW_WRITE(regs, RTW_PHYCFG, reg);
		RTW_SYNC(regs, RTW_PHYCFG, RTW_PHYCFG);

		DELAY(2);	/* arbitrary delay */

		reg &= ~RTW_PHYCFG_HST_CLK;
		RTW_WRITE(regs, RTW_PHYCFG, reg);
		RTW_SYNC(regs, RTW_PHYCFG, RTW_PHYCFG);

		if (lo_to_hi)
			mask <<= 1;
		else
			mask >>= 1;
	}

	reg |= RTW_PHYCFG_HST_EN;
	KASSERT((reg & RTW_PHYCFG_HST_CLK) == 0);
	RTW_WRITE(regs, RTW_PHYCFG, reg);
	RTW_SYNC(regs, RTW_PHYCFG, RTW_PHYCFG);
}

/* Help rtw_rf_macwrite: tell MAC to bang bits to RF over the 3-wire
 * interface.
 */
static __inline int
rtw_rf_macbangbits(struct rtw_regs *regs, u_int32_t reg)
{
	int i;

	RTW_DPRINTF(RTW_DEBUG_PHY, ("%s: %#08x\n", __func__, reg));

	RTW_WRITE(regs, RTW_PHYCFG, RTW_PHYCFG_MAC_POLL | reg);

	RTW_WBR(regs, RTW_PHYCFG, RTW_PHYCFG);

	for (i = rtw_macbangbits_timeout; --i >= 0; delay(1)) {
		if ((RTW_READ(regs, RTW_PHYCFG) & RTW_PHYCFG_MAC_POLL) == 0) {
			RTW_DPRINTF(RTW_DEBUG_PHY,
			    ("%s: finished in %dus\n", __func__,
			    rtw_macbangbits_timeout - i));
			return 0;
		}
		RTW_RBR(regs, RTW_PHYCFG, RTW_PHYCFG);	/* XXX paranoia? */
	}

	printf("%s: RTW_PHYCFG_MAC_POLL still set.\n", __func__);
	return -1;
}

u_int32_t
rtw_grf5101_host_crypt(u_int addr, u_int32_t val)
{
	/* TBD */
	return 0;
}

u_int32_t
rtw_grf5101_mac_crypt(u_int addr, u_int32_t val)
{
	u_int32_t data_and_addr;
#define EXTRACT_NIBBLE(d, which) (((d) >> (4 * (which))) & 0xf)
	static u_int8_t caesar[16] = {0x0, 0x8, 0x4, 0xc,
	                              0x2, 0xa, 0x6, 0xe,
				      0x1, 0x9, 0x5, 0xd,
				      0x3, 0xb, 0x7, 0xf};

	data_and_addr =  caesar[EXTRACT_NIBBLE(val, 2)] |
	                (caesar[EXTRACT_NIBBLE(val, 1)] <<  4) |
	                (caesar[EXTRACT_NIBBLE(val, 0)] <<  8) |
	                (caesar[(addr >> 1) & 0xf]      << 12) |
	                ((addr & 0x1)                   << 16) |
	                (caesar[EXTRACT_NIBBLE(val, 3)] << 24);
	return LSHIFT(data_and_addr,
	    RTW_PHYCFG_MAC_PHILIPS_ADDR_MASK|RTW_PHYCFG_MAC_PHILIPS_DATA_MASK);
#undef EXTRACT_NIBBLE
}

static __inline const char *
rtw_rfchipid_string(enum rtw_rfchipid rfchipid)
{
	switch (rfchipid) {
	case RTW_RFCHIPID_MAXIM:
		return "Maxim";
	case RTW_RFCHIPID_PHILIPS:
		return "Philips";
	case RTW_RFCHIPID_GCT:
		return "GCT";
	case RTW_RFCHIPID_RFMD:
		return "RFMD";
	case RTW_RFCHIPID_INTERSIL:
		return "Intersil";
	default:
		return "unknown";
	}
}

/* Bang bits over the 3-wire interface. */
int
rtw_rf_hostwrite(struct rtw_regs *regs, enum rtw_rfchipid rfchipid,
    u_int addr, u_int32_t val)
{
	u_int nbits;
	int lo_to_hi;
	u_int32_t bits;

	RTW_DPRINTF(RTW_DEBUG_PHYIO, ("%s: %s[%u] <- %#08x\n", __func__,
	    rtw_rfchipid_string(rfchipid), addr, val));

	switch (rfchipid) {
	case RTW_RFCHIPID_MAXIM:
		nbits = 16;
		lo_to_hi = 0;
		bits = LSHIFT(val, MAX2820_TWI_DATA_MASK) |
		       LSHIFT(addr, MAX2820_TWI_ADDR_MASK);
		break;
	case RTW_RFCHIPID_PHILIPS:
		KASSERT((addr & ~PRESHIFT(SA2400_TWI_ADDR_MASK)) == 0);
		KASSERT((val & ~PRESHIFT(SA2400_TWI_DATA_MASK)) == 0);
		bits = LSHIFT(val, SA2400_TWI_DATA_MASK) |
		       LSHIFT(addr, SA2400_TWI_ADDR_MASK) | SA2400_TWI_WREN;
		nbits = 32;
		lo_to_hi = 1;
		break;
	case RTW_RFCHIPID_GCT:
	case RTW_RFCHIPID_RFMD:
		KASSERT((addr & ~PRESHIFT(SI4126_TWI_ADDR_MASK)) == 0);
		KASSERT((val & ~PRESHIFT(SI4126_TWI_DATA_MASK)) == 0);
		if (rfchipid == RTW_RFCHIPID_GCT)
			bits = rtw_grf5101_host_crypt(addr, val);
		else {
			bits = LSHIFT(val, SI4126_TWI_DATA_MASK) |
			       LSHIFT(addr, SI4126_TWI_ADDR_MASK);
		}
		nbits = 22;
		lo_to_hi = 0;
		break;
	case RTW_RFCHIPID_INTERSIL:
	default:
		printf("%s: unknown rfchipid %d\n", __func__, rfchipid);
		return -1;
	}

	rtw_rf_hostbangbits(regs, bits, lo_to_hi, nbits);

	return 0;
}

u_int32_t
rtw_maxim_swizzle(u_int addr, u_int32_t val)
{
	u_int32_t hidata, lodata;

	KASSERT((val & ~(RTW_MAXIM_LODATA_MASK|RTW_MAXIM_HIDATA_MASK)) == 0);
	lodata = MASK_AND_RSHIFT(val, RTW_MAXIM_LODATA_MASK);
	hidata = MASK_AND_RSHIFT(val, RTW_MAXIM_HIDATA_MASK);
	return LSHIFT(lodata, RTW_PHYCFG_MAC_MAXIM_LODATA_MASK) |
	    LSHIFT(hidata, RTW_PHYCFG_MAC_MAXIM_HIDATA_MASK) |
	    LSHIFT(addr, RTW_PHYCFG_MAC_MAXIM_ADDR_MASK);
}

/* Tell the MAC what to bang over the 3-wire interface. */
int
rtw_rf_macwrite(struct rtw_regs *regs, enum rtw_rfchipid rfchipid,
    u_int addr, u_int32_t val)
{
	u_int32_t reg;

	RTW_DPRINTF(RTW_DEBUG_PHYIO, ("%s: %s[%u] <- %#08x\n", __func__,
	    rtw_rfchipid_string(rfchipid), addr, val));

	switch (rfchipid) {
	case RTW_RFCHIPID_GCT:
		reg = rtw_grf5101_mac_crypt(addr, val);
		break;
	case RTW_RFCHIPID_MAXIM:
		reg = rtw_maxim_swizzle(addr, val);
		break;
	default:		/* XXX */
	case RTW_RFCHIPID_PHILIPS:
		KASSERT(
		    (addr & ~PRESHIFT(RTW_PHYCFG_MAC_PHILIPS_ADDR_MASK)) == 0);
		KASSERT(
		    (val & ~PRESHIFT(RTW_PHYCFG_MAC_PHILIPS_DATA_MASK)) == 0);

		reg = LSHIFT(addr, RTW_PHYCFG_MAC_PHILIPS_ADDR_MASK) |
		      LSHIFT(val, RTW_PHYCFG_MAC_PHILIPS_DATA_MASK);
	}

	switch (rfchipid) {
	case RTW_RFCHIPID_GCT:
	case RTW_RFCHIPID_MAXIM:
	case RTW_RFCHIPID_RFMD:
		reg |= RTW_PHYCFG_MAC_RFTYPE_RFMD;
		break;
	case RTW_RFCHIPID_INTERSIL:
		reg |= RTW_PHYCFG_MAC_RFTYPE_INTERSIL;
		break;
	case RTW_RFCHIPID_PHILIPS:
		reg |= RTW_PHYCFG_MAC_RFTYPE_PHILIPS;
		break;
	default:
		printf("%s: unknown rfchipid %d\n", __func__, rfchipid);
		return -1;
	}

	return rtw_rf_macbangbits(regs, reg);
}
