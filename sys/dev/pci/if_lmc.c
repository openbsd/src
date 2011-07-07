/*	$OpenBSD: if_lmc.c,v 1.25 2011/07/07 20:42:56 henning Exp $ */
/*	$NetBSD: if_lmc.c,v 1.1 1999/03/25 03:32:43 explorer Exp $	*/

/*-
 * Copyright (c) 1997-1999 LAN Media Corporation (LMC)
 * All rights reserved.  www.lanmedia.com
 *
 * This code is written by Michael Graff <graff@vix.com> for LMC.
 * The code is derived from permitted modifications to software created
 * by Matt Thomas (matt@3am-software.com).
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. All marketing or advertising materials mentioning features or
 *    use of this software must display the following acknowledgement:
 *      This product includes software developed by LAN Media Corporation
 *      and its contributors.
 * 4. Neither the name of LAN Media Corporation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY LAN MEDIA CORPORATION AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

/*-
 * Copyright (c) 1994-1997 Matt Thomas (matt@3am-software.com)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * LMC1200 (DS1) & LMC5245 (DS3) LED definitions
 * led0 yellow = far-end link is in Red alarm condition
 * led1 blue   = received an Alarm Indication signal (upstream failure)
 * led2 Green  = power to adapter, Gate Array loaded & driver attached
 * led3 red    = Loss of Signal (LOS) or out of frame (OOF) conditions
 *               detected on T3 receive signal
 *
 * LMC1000 (SSI) & LMC5200 (HSSI) LED definitions
 * led0 Green  = power to adapter, Gate Array loaded & driver attached
 * led1 Green  = DSR and DTR and RTS and CTS are set (CA, TA for LMC5200)
 * led2 Green  = Cable detected (Green indicates non-loopback mode for LMC5200)
 * led3 red    = No timing is available from the cable or the on-board
 *               frequency generator. (ST not available for LMC5200)
 */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/proc.h>
#include <sys/device.h>

#include <dev/pci/pcidevs.h>

#include <net/if.h>
#include <net/if_types.h>
#include <net/if_dl.h>
#include <net/netisr.h>

#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net/if_sppp.h>

#include <machine/bus.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/ic/dc21040reg.h>

#include <dev/pci/if_lmc_types.h>
#include <dev/pci/if_lmcioctl.h>
#include <dev/pci/if_lmcvar.h>

/*
 * This module supports
 *	the DEC 21140A pass 2.2 PCI Fast Ethernet Controller.
 */
static ifnet_ret_t lmc_ifstart_one(struct ifnet *ifp);
static ifnet_ret_t lmc_ifstart(struct ifnet *ifp);
static struct mbuf *lmc_txput(lmc_softc_t * const sc, struct mbuf *m);
static void lmc_rx_intr(lmc_softc_t * const sc);

static void lmc_watchdog(struct ifnet *ifp);
static void lmc_ifup(lmc_softc_t * const sc);
static void lmc_ifdown(lmc_softc_t * const sc);

/*
 * Code the read the SROM and MII bit streams (I2C)
 */
static inline void
lmc_delay_300ns(lmc_softc_t * const sc)
{
	int idx;
	for (idx = (300 / 33) + 1; idx > 0; idx--)
		(void)LMC_CSR_READ(sc, csr_busmode);
}

#define EMIT    \
do { \
	LMC_CSR_WRITE(sc, csr_srom_mii, csr); \
	lmc_delay_300ns(sc); \
} while (0)

static inline void
lmc_srom_idle(lmc_softc_t * const sc)
{
	unsigned bit, csr;
    
	csr  = SROMSEL ; EMIT;
	csr  = SROMSEL | SROMRD; EMIT;  
	csr ^= SROMCS; EMIT;
	csr ^= SROMCLKON; EMIT;

	/*
	 * Write 25 cycles of 0 which will force the SROM to be idle.
	 */
	for (bit = 3 + SROM_BITWIDTH + 16; bit > 0; bit--) {
		csr ^= SROMCLKOFF; EMIT;    /* clock low; data not valid */
		csr ^= SROMCLKON; EMIT;     /* clock high; data valid */
	}
	csr ^= SROMCLKOFF; EMIT;
	csr ^= SROMCS; EMIT;
	csr  = 0; EMIT;
}

     
static void
lmc_srom_read(lmc_softc_t * const sc)
{   
	unsigned idx; 
	const unsigned bitwidth = SROM_BITWIDTH;
	const unsigned cmdmask = (SROMCMD_RD << bitwidth);
	const unsigned msb = 1 << (bitwidth + 3 - 1);
	unsigned lastidx = (1 << bitwidth) - 1;

	lmc_srom_idle(sc);

	for (idx = 0; idx <= lastidx; idx++) {
		unsigned lastbit, data, bits, bit, csr;
		csr  = SROMSEL ;	        EMIT;
		csr  = SROMSEL | SROMRD;        EMIT;
		csr ^= SROMCSON;                EMIT;
		csr ^=            SROMCLKON;    EMIT;
    
		lastbit = 0;
		for (bits = idx|cmdmask, bit = bitwidth + 3
			     ; bit > 0
			     ; bit--, bits <<= 1) {
			const unsigned thisbit = bits & msb;
			csr ^= SROMCLKOFF; EMIT;    /* clock L data invalid */
			if (thisbit != lastbit) {
				csr ^= SROMDOUT; EMIT;/* clock L invert data */
			} else {
				EMIT;
			}
			csr ^= SROMCLKON; EMIT;     /* clock H data valid */
			lastbit = thisbit;
		}
		csr ^= SROMCLKOFF; EMIT;

		for (data = 0, bits = 0; bits < 16; bits++) {
			data <<= 1;
			csr ^= SROMCLKON; EMIT;     /* clock H data valid */ 
			data |= LMC_CSR_READ(sc, csr_srom_mii) & SROMDIN ? 1 : 0;
			csr ^= SROMCLKOFF; EMIT;    /* clock L data invalid */
		}
		sc->lmc_rombuf[idx*2] = data & 0xFF;
		sc->lmc_rombuf[idx*2+1] = data >> 8;
		csr  = SROMSEL | SROMRD; EMIT;
		csr  = 0; EMIT;
	}
	lmc_srom_idle(sc);
}

#define MII_EMIT    do { LMC_CSR_WRITE(sc, csr_srom_mii, csr); lmc_delay_300ns(sc); } while (0)

static inline void
lmc_mii_writebits(lmc_softc_t * const sc, unsigned data, unsigned bits)
{
    unsigned msb = 1 << (bits - 1);
    unsigned csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    unsigned lastbit = (csr & MII_DOUT) ? msb : 0;

    csr |= MII_WR; MII_EMIT;  		/* clock low; assert write */

    for (; bits > 0; bits--, data <<= 1) {
	const unsigned thisbit = data & msb;
	if (thisbit != lastbit) {
	    csr ^= MII_DOUT; MII_EMIT;  /* clock low; invert data */
	}
	csr ^= MII_CLKON; MII_EMIT;     /* clock high; data valid */
	lastbit = thisbit;
	csr ^= MII_CLKOFF; MII_EMIT;    /* clock low; data not valid */
    }
}

static void
lmc_mii_turnaround(lmc_softc_t * const sc, u_int32_t cmd)
{
    u_int32_t csr;

    csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    if (cmd == MII_WRCMD) {
	csr |= MII_DOUT; MII_EMIT;	/* clock low; change data */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
	csr ^= MII_DOUT; MII_EMIT;	/* clock low; change data */
    } else {
	csr |= MII_RD; MII_EMIT;	/* clock low; switch to read */
    }
    csr ^= MII_CLKON; MII_EMIT;		/* clock high; data valid */
    csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
}

static u_int32_t
lmc_mii_readbits(lmc_softc_t * const sc)
{
    u_int32_t data;
    u_int32_t csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    int idx;

    for (idx = 0, data = 0; idx < 16; idx++) {
	data <<= 1;	/* this is NOOP on the first pass through */
	csr ^= MII_CLKON; MII_EMIT;	/* clock high; data valid */
	if (LMC_CSR_READ(sc, csr_srom_mii) & MII_DIN)
	    data |= 1;
	csr ^= MII_CLKOFF; MII_EMIT;	/* clock low; data not valid */
    }
    csr ^= MII_RD; MII_EMIT;		/* clock low; turn off read */

    return data;
}

u_int32_t
lmc_mii_readreg(lmc_softc_t * const sc, u_int32_t devaddr, u_int32_t regno)
{
    u_int32_t csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    u_int32_t data;

    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    lmc_mii_writebits(sc, MII_PREAMBLE, 32);
    lmc_mii_writebits(sc, MII_RDCMD, 8);
    lmc_mii_writebits(sc, devaddr, 5);
    lmc_mii_writebits(sc, regno, 5);
    lmc_mii_turnaround(sc, MII_RDCMD);

    data = lmc_mii_readbits(sc);
    return (data);
}

void
lmc_mii_writereg(lmc_softc_t * const sc, u_int32_t devaddr,
		   u_int32_t regno, u_int32_t data)
{
    u_int32_t csr;

    csr = LMC_CSR_READ(sc, csr_srom_mii) & (MII_RD|MII_DOUT|MII_CLK);
    csr &= ~(MII_RD|MII_CLK); MII_EMIT;
    lmc_mii_writebits(sc, MII_PREAMBLE, 32);
    lmc_mii_writebits(sc, MII_WRCMD, 8);
    lmc_mii_writebits(sc, devaddr, 5);
    lmc_mii_writebits(sc, regno, 5);
    lmc_mii_turnaround(sc, MII_WRCMD);
    lmc_mii_writebits(sc, data, 16);
}

int
lmc_read_macaddr(lmc_softc_t * const sc)
{
	lmc_srom_read(sc);

	bcopy(sc->lmc_rombuf + 20, sc->lmc_enaddr, 6);

	return 0;
}

/*
 * Check to make certain there is a signal from the modem, and flicker
 * lights as needed.
 */
static void
lmc_watchdog(struct ifnet *ifp)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
	u_int32_t ostatus;
	u_int32_t link_status;
	u_int32_t ticks;

	/*
	 * Make sure the tx jabber and rx watchdog are off,
	 * and the transmit and receive processes are running.
	 */
	LMC_CSR_WRITE (sc, csr_15, 0x00000011);
	sc->lmc_cmdmode |= TULIP_CMD_TXRUN | TULIP_CMD_RXRUN;
	LMC_CSR_WRITE (sc, csr_command, sc->lmc_cmdmode);

	/* Is the transmit clock still available? */
	ticks = LMC_CSR_READ (sc, csr_gp_timer);
	ticks = 0x0000ffff - (ticks & 0x0000ffff);
	if (ticks == 0)
	{
		/* no clock found ? */
		if (sc->tx_clockState != 0)
		{
			sc->tx_clockState = 0;
			if (sc->lmc_cardtype == LMC_CARDTYPE_SSI)
				lmc_led_on (sc, LMC_MII16_LED3); /* ON red */
		}
	else
		if (sc->tx_clockState == 0)
		{
			sc->tx_clockState = 1;
			if (sc->lmc_cardtype == LMC_CARDTYPE_SSI)
				lmc_led_off (sc, LMC_MII16_LED3); /* OFF red */
		}
	}

	link_status = sc->lmc_media->get_link_status(sc);
	ostatus = ((sc->lmc_flags & LMC_MODEMOK) == LMC_MODEMOK);

	/*
	 * hardware level link lost, but the interface is marked as up.
	 * Mark it as down.
	 */
        if (link_status == LMC_LINK_DOWN && ostatus) {
		printf(LMC_PRINTF_FMT ": physical link down\n",
		       LMC_PRINTF_ARGS);
		sc->lmc_flags &= ~LMC_MODEMOK;
		if (sc->lmc_cardtype == LMC_CARDTYPE_DS3 ||
		    sc->lmc_cardtype == LMC_CARDTYPE_T1)
			lmc_led_on (sc, LMC_DS3_LED3 | LMC_DS3_LED2);
							/* turn on red LED */
		else {
			lmc_led_off (sc, LMC_MII16_LED1);
			lmc_led_on (sc, LMC_MII16_LED0);
			if (sc->lmc_timing == LMC_CTL_CLOCK_SOURCE_EXT)
				lmc_led_on (sc, LMC_MII16_LED3);
		}

	}

	/*
	 * hardware link is up, but the interface is marked as down.
	 * Bring it back up again.
	 */
	if (link_status != LMC_LINK_DOWN && !ostatus) {
		printf(LMC_PRINTF_FMT ": physical link up\n",
		       LMC_PRINTF_ARGS);
		if (sc->lmc_flags & LMC_IFUP)
			lmc_ifup(sc);
		sc->lmc_flags |= LMC_MODEMOK;
		if (sc->lmc_cardtype == LMC_CARDTYPE_DS3 ||
		    sc->lmc_cardtype == LMC_CARDTYPE_T1)
		{
			sc->lmc_miireg16 |= LMC_DS3_LED3;
			lmc_led_off (sc, LMC_DS3_LED3);
							/* turn off red LED */
			lmc_led_on (sc, LMC_DS3_LED2);
		} else {
			lmc_led_on (sc, LMC_MII16_LED0 | LMC_MII16_LED1
				    | LMC_MII16_LED2);
			if (sc->lmc_timing != LMC_CTL_CLOCK_SOURCE_EXT)
				lmc_led_off (sc, LMC_MII16_LED3);
		}

		return;
	}

	/* Call media specific watchdog functions */
	sc->lmc_media->watchdog(sc);

	/*
	 * remember the timer value
	 */
	ticks = LMC_CSR_READ(sc, csr_gp_timer);
	LMC_CSR_WRITE(sc, csr_gp_timer, 0xffffffffUL);
	sc->ictl.ticks = 0x0000ffff - (ticks & 0x0000ffff);

	ifp->if_timer = 1;
}

/*
 * Mark the interface as "up" and enable TX/RX and TX/RX interrupts.
 * This also does a full software reset.
 */
static void
lmc_ifup(lmc_softc_t * const sc)
{
	sc->lmc_if.if_timer = 0;

	lmc_dec_reset(sc);
	lmc_reset(sc);

	sc->lmc_media->set_link_status(sc, LMC_LINK_UP);
	sc->lmc_media->set_status(sc, NULL);

	sc->lmc_flags |= LMC_IFUP;

	/*
	 * for DS3 & DS1 adapters light the green light, led2
	 */
	if (sc->lmc_cardtype == LMC_CARDTYPE_DS3 ||
	    sc->lmc_cardtype == LMC_CARDTYPE_T1)
		lmc_led_on (sc, LMC_MII16_LED2);
	else
		lmc_led_on (sc, LMC_MII16_LED0 | LMC_MII16_LED2);

	/*
	 * select what interrupts we want to get
	 */
	sc->lmc_intrmask |= (TULIP_STS_NORMALINTR
			       | TULIP_STS_RXINTR
			       | TULIP_STS_RXNOBUF
			       | TULIP_STS_TXINTR
			       | TULIP_STS_ABNRMLINTR
			       | TULIP_STS_SYSERROR
			       | TULIP_STS_TXSTOPPED
			       | TULIP_STS_TXUNDERFLOW
			       | TULIP_STS_RXSTOPPED
			       );
	LMC_CSR_WRITE(sc, csr_intr, sc->lmc_intrmask);

	sc->lmc_cmdmode |= TULIP_CMD_TXRUN;
	sc->lmc_cmdmode |= TULIP_CMD_RXRUN;
	LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode);

	sc->lmc_if.if_timer = 1;
}

/*
 * Mark the interface as "down" and disable TX/RX and TX/RX interrupts.
 * This is done by performing a full reset on the interface.
 */
static void
lmc_ifdown(lmc_softc_t * const sc)
{
	sc->lmc_if.if_timer = 0;
	sc->lmc_flags &= ~LMC_IFUP;

	sc->lmc_media->set_link_status(sc, LMC_LINK_DOWN);
	lmc_led_off(sc, LMC_MII16_LED_ALL);

	lmc_dec_reset(sc);
	lmc_reset(sc);
	sc->lmc_media->set_status(sc, NULL);
}

static void
lmc_rx_intr(lmc_softc_t * const sc)
{
	lmc_ringinfo_t * const ri = &sc->lmc_rxinfo;
	struct ifnet * const ifp = &sc->lmc_if;
	u_int32_t status;
	int fillok = 1;

	sc->lmc_rxtick++;

	for (;;) {
		lmc_desc_t *eop = ri->ri_nextin;
		int total_len = 0, last_offset = 0;
		struct mbuf *ms = NULL, *me = NULL;
		int accept = 0;
		bus_dmamap_t map;
		int error;

		if (fillok && IF_LEN(&sc->lmc_rxq) < LMC_RXQ_TARGET)
			goto queue_mbuf;

		/*
		 * If the TULIP has no descriptors, there can't be any receive
		 * descriptors to process.
		 */
		if (eop == ri->ri_nextout)
			break;
	    
		/*
		 * 90% of the packets will fit in one descriptor.  So we
		 * optimize for that case.
		 */
		LMC_RXDESC_POSTSYNC(sc, eop, sizeof(*eop));
		status = letoh32(((volatile lmc_desc_t *) eop)->d_status);
		if ((status &
			(TULIP_DSTS_OWNER|TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) == 
			(TULIP_DSTS_RxFIRSTDESC|TULIP_DSTS_RxLASTDESC)) {
			IF_DEQUEUE(&sc->lmc_rxq, ms);
			me = ms;
		} else {
			/*
			 * If still owned by the TULIP, don't touch it.
			 */
			if (status & TULIP_DSTS_OWNER)
				break;

			/*
			 * It is possible (though improbable unless the
			 * BIG_PACKET support is enabled or MCLBYTES < 1518)
			 * for a received packet to cross more than one
			 * receive descriptor.
			 */
			while ((status & TULIP_DSTS_RxLASTDESC) == 0) {
				if (++eop == ri->ri_last)
					eop = ri->ri_first;
				LMC_RXDESC_POSTSYNC(sc, eop, sizeof(*eop));
				status = letoh32(((volatile lmc_desc_t *)
					eop)->d_status);
				if (eop == ri->ri_nextout || 
					(status & TULIP_DSTS_OWNER)) {
					return;
				}
				total_len++;
			}

			/*
			 * Dequeue the first buffer for the start of the
			 * packet.  Hopefully this will be the only one we
			 * need to dequeue.  However, if the packet consumed
			 * multiple descriptors, then we need to dequeue
			 * those buffers and chain to the starting mbuf.
			 * All buffers but the last buffer have the same
			 * length so we can set that now. (we add to
			 * last_offset instead of multiplying since we
			 * normally won't go into the loop and thereby
			 * saving a ourselves from doing a multiplication
			 * by 0 in the normal case).
			 */
			IF_DEQUEUE(&sc->lmc_rxq, ms);
			for (me = ms; total_len > 0; total_len--) {
				map = LMC_GETCTX(me, bus_dmamap_t);
				LMC_RXMAP_POSTSYNC(sc, map);
				bus_dmamap_unload(sc->lmc_dmatag, map);
				sc->lmc_rxmaps[sc->lmc_rxmaps_free++] = map;
#if defined(DIAGNOSTIC)
				LMC_SETCTX(me, NULL);
#endif
				me->m_len = LMC_RX_BUFLEN;
				last_offset += LMC_RX_BUFLEN;
				IF_DEQUEUE(&sc->lmc_rxq, me->m_next);
				me = me->m_next;
			}
		}

		/*
		 *  Now get the size of received packet (minus the CRC).
		 */
		total_len = ((status >> 16) & 0x7FFF);
		if (sc->ictl.crc_length == 16)
			total_len -= 2;
		else
			total_len -= 4;

		if ((sc->lmc_flags & LMC_RXIGNORE) == 0
		    && ((status & LMC_DSTS_ERRSUM) == 0
#ifdef BIG_PACKET
			|| (total_len <= sc->lmc_if.if_mtu + PPP_HEADER_LEN
			    && (status & TULIP_DSTS_RxOVERFLOW) == 0)
#endif
			)) {

			map = LMC_GETCTX(me, bus_dmamap_t);
			bus_dmamap_sync(sc->lmc_dmatag, map, 0, me->m_len,
				BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->lmc_dmatag, map);
			sc->lmc_rxmaps[sc->lmc_rxmaps_free++] = map;
#if defined(DIAGNOSTIC)
			LMC_SETCTX(me, NULL);
#endif

			me->m_len = total_len - last_offset;
#if NBPFILTER > 0
			if (sc->lmc_bpf != NULL) {
				if (me == ms)
					LMC_BPF_TAP(sc, mtod(ms, caddr_t),
					    total_len, BPF_DIRECTION_IN);
				else
					LMC_BPF_MTAP(sc, ms, BPF_DIRECTION_IN);
			}
#endif
			sc->lmc_flags |= LMC_RXACT;
			accept = 1;
		} else {
			ifp->if_ierrors++;
			if (status & TULIP_DSTS_RxOVERFLOW) {
				sc->lmc_dot3stats.dot3StatsInternalMacReceiveErrors++;
			}
			map = LMC_GETCTX(me, bus_dmamap_t);
			bus_dmamap_unload(sc->lmc_dmatag, map);
			sc->lmc_rxmaps[sc->lmc_rxmaps_free++] = map;
#if defined(DIAGNOSTIC)
			LMC_SETCTX(me, NULL);
#endif
		}

		ifp->if_ipackets++;
		if (++eop == ri->ri_last)
			eop = ri->ri_first;
		ri->ri_nextin = eop;

	queue_mbuf:
		/*
		 * Either we are priming the TULIP with mbufs (m == NULL)
		 * or we are about to accept an mbuf for the upper layers
		 * so we need to allocate an mbuf to replace it.  If we
		 * can't replace it, send up it anyways.  This may cause
		 * us to drop packets in the future but that's better than
		 * being caught in livelock.
		 *
		 * Note that if this packet crossed multiple descriptors
		 * we don't even try to reallocate all the mbufs here.
		 * Instead we rely on the test of the beginning of
		 * the loop to refill for the extra consumed mbufs.
		 */
		if (accept || ms == NULL) {
			struct mbuf *m0;
			MGETHDR(m0, M_DONTWAIT, MT_DATA);
			if (m0 != NULL) {
				MCLGET(m0, M_DONTWAIT);
				if ((m0->m_flags & M_EXT) == 0) {
					m_freem(m0);
					m0 = NULL;
				}
			}
			if (accept) {
				ms->m_pkthdr.len = total_len;
				ms->m_pkthdr.rcvif = ifp;
				sppp_input(ifp, ms);
			}
			ms = m0;
		}
		if (ms == NULL) {
			/*
			 * Couldn't allocate a new buffer.  Don't bother 
			 * trying to replenish the receive queue.
			 */
			fillok = 0;
			sc->lmc_flags |= LMC_RXBUFSLOW;
			continue;
		}
		/*
		 * Now give the buffer(s) to the TULIP and save in our
		 * receive queue.
		 */
		do {
			u_int32_t ctl;
			lmc_desc_t * const nextout = ri->ri_nextout;

			if (sc->lmc_rxmaps_free > 0) {
				map = sc->lmc_rxmaps[--sc->lmc_rxmaps_free];
			} else {
				m_freem(ms);
				sc->lmc_flags |= LMC_RXBUFSLOW;
#if defined(LMC_DEBUG)
				sc->lmc_dbg.dbg_rxlowbufs++;
#endif
				break;
			}
			LMC_SETCTX(ms, map);
			error = bus_dmamap_load(sc->lmc_dmatag, map,
				mtod(ms, void *), LMC_RX_BUFLEN, 
				NULL, BUS_DMA_NOWAIT);
			if (error) {
				printf(LMC_PRINTF_FMT
					": unable to load rx map, "
					"error = %d\n",
					LMC_PRINTF_ARGS, error);
				panic("lmc_rx_intr");		/* XXX */
			}

			ctl = letoh32(nextout->d_ctl);
			/* For some weird reason we lose TULIP_DFLAG_ENDRING */
			if ((nextout+1) == ri->ri_last)
				ctl = LMC_CTL(LMC_CTL_FLGS(ctl)|
					TULIP_DFLAG_ENDRING, 0, 0);
			nextout->d_addr1 = htole32(map->dm_segs[0].ds_addr);
			if (map->dm_nsegs == 2) {
				nextout->d_addr2 = htole32(map->dm_segs[1].ds_addr);
				nextout->d_ctl = 
					htole32(LMC_CTL(LMC_CTL_FLGS(ctl),
						map->dm_segs[0].ds_len,
						map->dm_segs[1].ds_len));
			} else {
				nextout->d_addr2 = 0;
				nextout->d_ctl = 
					htole32(LMC_CTL(LMC_CTL_FLGS(ctl),
						map->dm_segs[0].ds_len, 0));
			}
			LMC_RXDESC_POSTSYNC(sc, nextout, sizeof(*nextout));
			ri->ri_nextout->d_status = htole32(TULIP_DSTS_OWNER);
			LMC_RXDESC_POSTSYNC(sc, nextout, sizeof(u_int32_t));
			if (++ri->ri_nextout == ri->ri_last)
				ri->ri_nextout = ri->ri_first;
			me = ms->m_next;
			ms->m_next = NULL;
			IF_ENQUEUE(&sc->lmc_rxq, ms);
		} while ((ms = me) != NULL);

		if (IF_LEN(&sc->lmc_rxq) >= LMC_RXQ_TARGET)
			sc->lmc_flags &= ~LMC_RXBUFSLOW;
	}
}

static int
lmc_tx_intr(lmc_softc_t * const sc)
{
    lmc_ringinfo_t * const ri = &sc->lmc_txinfo;
    struct mbuf *m;
    int xmits = 0;
    int descs = 0;
    u_int32_t d_status;

    sc->lmc_txtick++;

    while (ri->ri_free < ri->ri_max) {
	u_int32_t flag;

	LMC_TXDESC_POSTSYNC(sc, ri->ri_nextin, sizeof(*ri->ri_nextin));
	d_status = letoh32(((volatile lmc_desc_t *) ri->ri_nextin)->d_status);
	if (d_status & TULIP_DSTS_OWNER)
	    break;

	flag = LMC_CTL_FLGS(letoh32(ri->ri_nextin->d_ctl));
	if (flag & TULIP_DFLAG_TxLASTSEG) {
		IF_DEQUEUE(&sc->lmc_txq, m);
		if (m != NULL) {
		    bus_dmamap_t map = LMC_GETCTX(m, bus_dmamap_t);
		    LMC_TXMAP_POSTSYNC(sc, map);
		    sc->lmc_txmaps[sc->lmc_txmaps_free++] = map;
#if NBPFILTER > 0
		    if (sc->lmc_bpf != NULL)
			LMC_BPF_MTAP(sc, m, BPF_DIRECTION_OUT);
#endif
		    m_freem(m);
#if defined(LMC_DEBUG)
		} else {
		    printf(LMC_PRINTF_FMT ": tx_intr: failed to dequeue mbuf?!?\n", LMC_PRINTF_ARGS);
#endif
		}
		    xmits++;
		    if (d_status & LMC_DSTS_ERRSUM) {
			sc->lmc_if.if_oerrors++;
			if (d_status & TULIP_DSTS_TxUNDERFLOW) {
			    sc->lmc_dot3stats.dot3StatsInternalTransmitUnderflows++;
			}
		    } else {
			if (d_status & TULIP_DSTS_TxDEFERRED) {
			    sc->lmc_dot3stats.dot3StatsDeferredTransmissions++;
			}
		    }
	}

	if (++ri->ri_nextin == ri->ri_last)
	    ri->ri_nextin = ri->ri_first;

	ri->ri_free++;
	descs++;
	sc->lmc_if.if_flags &= ~IFF_OACTIVE;
    }
    /*
     * If nothing left to transmit, disable the timer.
     * Else if progress, reset the timer back to 2 ticks.
     */
    sc->lmc_if.if_opackets += xmits;

    return descs;
}

static void
lmc_print_abnormal_interrupt (lmc_softc_t * const sc, u_int32_t csr)
{
	printf(LMC_PRINTF_FMT ": Abnormal interrupt\n", LMC_PRINTF_ARGS);
}

static const char * const lmc_system_errors[] = {
    "parity error",
    "master abort",
    "target abort",
    "reserved #3",
    "reserved #4",
    "reserved #5",
    "reserved #6",
    "reserved #7",
};

static void
lmc_intr_handler(lmc_softc_t * const sc, int *progress_p)
{
    u_int32_t csr;

    while ((csr = LMC_CSR_READ(sc, csr_status)) & sc->lmc_intrmask) {

	*progress_p = 1;
	LMC_CSR_WRITE(sc, csr_status, csr);

	if (csr & TULIP_STS_SYSERROR) {
	    sc->lmc_last_system_error = (csr & TULIP_STS_ERRORMASK) >> TULIP_STS_ERR_SHIFT;
	    if (sc->lmc_flags & LMC_NOMESSAGES) {
		sc->lmc_flags |= LMC_SYSTEMERROR;
	    } else {
		printf(LMC_PRINTF_FMT ": system error: %s\n",
		       LMC_PRINTF_ARGS,
		       lmc_system_errors[sc->lmc_last_system_error]);
	    }
	    sc->lmc_flags |= LMC_NEEDRESET;
	    sc->lmc_system_errors++;
	    break;
	}
	if (csr & (TULIP_STS_RXINTR | TULIP_STS_RXNOBUF)) {
	    u_int32_t misses = LMC_CSR_READ(sc, csr_missed_frames);
	    if (csr & TULIP_STS_RXNOBUF)
		sc->lmc_dot3stats.dot3StatsMissedFrames += misses & 0xFFFF;
	    /*
	     * Pass 2.[012] of the 21140A-A[CDE] may hang and/or corrupt data
	     * on receive overflows.
	     */
	   if ((misses & 0x0FFE0000) && (sc->lmc_features & LMC_HAVE_RXBADOVRFLW)) {
		sc->lmc_dot3stats.dot3StatsInternalMacReceiveErrors++;
		/*
		 * Stop the receiver process and spin until it's stopped.
		 * Tell rx_intr to drop the packets it dequeues.
		 */
		LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode & ~TULIP_CMD_RXRUN);
		while ((LMC_CSR_READ(sc, csr_status) & TULIP_STS_RXSTOPPED) == 0)
		    ;
		LMC_CSR_WRITE(sc, csr_status, TULIP_STS_RXSTOPPED);
		sc->lmc_flags |= LMC_RXIGNORE;
	    }
	    lmc_rx_intr(sc);
	    if (sc->lmc_flags & LMC_RXIGNORE) {
		/*
		 * Restart the receiver.
		 */
		sc->lmc_flags &= ~LMC_RXIGNORE;
		LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode);
	    }
	}
	if (csr & TULIP_STS_ABNRMLINTR) {
	    u_int32_t tmp = csr & sc->lmc_intrmask
		& ~(TULIP_STS_NORMALINTR|TULIP_STS_ABNRMLINTR);
	    if (csr & TULIP_STS_TXUNDERFLOW) {
		if ((sc->lmc_cmdmode & TULIP_CMD_THRESHOLDCTL) != TULIP_CMD_THRSHLD160) {
		    sc->lmc_cmdmode += TULIP_CMD_THRSHLD96;
		    sc->lmc_flags |= LMC_NEWTXTHRESH;
		} else if (sc->lmc_features & LMC_HAVE_STOREFWD) {
		    sc->lmc_cmdmode |= TULIP_CMD_STOREFWD;
		    sc->lmc_flags |= LMC_NEWTXTHRESH;
		}
	    }
	    if (sc->lmc_flags & LMC_NOMESSAGES) {
		sc->lmc_statusbits |= tmp;
	    } else {
		lmc_print_abnormal_interrupt(sc, tmp);
		sc->lmc_flags |= LMC_NOMESSAGES;
	    }
	    LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode);
	}

	if (csr & TULIP_STS_TXINTR)
		lmc_tx_intr(sc);

	if (sc->lmc_flags & LMC_WANTTXSTART)
	    lmc_ifstart(&sc->lmc_if);
    }
}

lmc_intrfunc_t
lmc_intr_normal(void *arg)
{
	lmc_softc_t * sc = (lmc_softc_t *) arg;
	int progress = 0;

	lmc_intr_handler(sc, &progress);

#if !defined(LMC_VOID_INTRFUNC)
	return progress;
#endif
}

static struct mbuf *
lmc_mbuf_compress(struct mbuf *m)
{
	struct mbuf *m0;
#if MCLBYTES >= LMC_MTU + PPP_HEADER_LEN && !defined(BIG_PACKET)
	MGETHDR(m0, M_DONTWAIT, MT_DATA);
	if (m0 != NULL) {
		if (m->m_pkthdr.len > MHLEN) {
			MCLGET(m0, M_DONTWAIT);
			if ((m0->m_flags & M_EXT) == 0) {
				m_freem(m);
				m_freem(m0);
				return NULL;
			}
		}
		m_copydata(m, 0, m->m_pkthdr.len, mtod(m0, caddr_t));
		m0->m_pkthdr.len = m0->m_len = m->m_pkthdr.len;
	}
#else
	int mlen = MHLEN;
	int len = m->m_pkthdr.len;
	struct mbuf **mp = &m0;

	while (len > 0) {
		if (mlen == MHLEN) {
			MGETHDR(*mp, M_DONTWAIT, MT_DATA);
		} else {
			MGET(*mp, M_DONTWAIT, MT_DATA);
		}
		if (*mp == NULL) {
			m_freem(m0);
			m0 = NULL;
			break;
		}
		if (len > MLEN) {
			MCLGET(*mp, M_DONTWAIT);
			if (((*mp)->m_flags & M_EXT) == 0) {
				m_freem(m0);
				m0 = NULL;
				break;
			}
			(*mp)->m_len = (len <= MCLBYTES ? len : MCLBYTES);
		} else {
			(*mp)->m_len = (len <= mlen ? len : mlen);
		}
		m_copydata(m, m->m_pkthdr.len - len,
			   (*mp)->m_len, mtod((*mp), caddr_t));
		len -= (*mp)->m_len;
		mp = &(*mp)->m_next;
		mlen = MLEN;
	}
#endif
	m_freem(m);
	return m0;
}

/*
 * queue the mbuf handed to us for the interface.  If we cannot
 * queue it, return the mbuf.  Return NULL if the mbuf was queued.
 */
static struct mbuf *
lmc_txput(lmc_softc_t * const sc, struct mbuf *m)
{
	lmc_ringinfo_t * const ri = &sc->lmc_txinfo;
	lmc_desc_t *eop, *nextout;
	int segcnt, free;
	u_int32_t d_status, ctl;
	bus_dmamap_t map;
	int error;

#if defined(LMC_DEBUG)
	if ((sc->lmc_cmdmode & TULIP_CMD_TXRUN) == 0) {
		printf(LMC_PRINTF_FMT ": txput: tx not running\n",
		       LMC_PRINTF_ARGS);
		sc->lmc_flags |= LMC_WANTTXSTART;
		goto finish;
	}
#endif

	/*
	 * Now we try to fill in our transmit descriptors.  This is
	 * a bit reminiscent of going on the Ark two by two
	 * since each descriptor for the TULIP can describe
	 * two buffers.  So we advance through packet filling
	 * each of the two entries at a time to fill each
	 * descriptor.  Clear the first and last segment bits
	 * in each descriptor (actually just clear everything
	 * but the end-of-ring or chain bits) to make sure
	 * we don't get messed up by previously sent packets.
	 *
	 * We may fail to put the entire packet on the ring if
	 * there is either not enough ring entries free or if the
	 * packet has more than MAX_TXSEG segments.  In the former
	 * case we will just wait for the ring to empty.  In the
	 * latter case we have to recopy.
	 */
	d_status = 0;
	eop = nextout = ri->ri_nextout;
	segcnt = 0;
	free = ri->ri_free;
	/*
	 * Reclaim some DMA maps from if we are out.
	 */
	if (sc->lmc_txmaps_free == 0) {
#if defined(LMC_DEBUG)
		sc->lmc_dbg.dbg_no_txmaps++;
#endif
		free += lmc_tx_intr(sc);
	}
	if (sc->lmc_txmaps_free > 0) {
		map = sc->lmc_txmaps[sc->lmc_txmaps_free-1];
	} else {
		sc->lmc_flags |= LMC_WANTTXSTART;
#if defined(LMC_DEBUG)
		sc->lmc_dbg.dbg_txput_finishes[1]++;
#endif
		goto finish;
	}
	error = bus_dmamap_load_mbuf(sc->lmc_dmatag, map, m, BUS_DMA_NOWAIT);
	if (error != 0) {
		if (error == EFBIG) {
			/*
			 * The packet exceeds the number of transmit buffer
			 * entries that we can use for one packet, so we have
			 * to recopy it into one mbuf and then try again.
			 */
			m = lmc_mbuf_compress(m);
			if (m == NULL) {
#if defined(LMC_DEBUG)
				sc->lmc_dbg.dbg_txput_finishes[2]++;
#endif
				goto finish;
			}
			error = bus_dmamap_load_mbuf(sc->lmc_dmatag, map, m,
				BUS_DMA_NOWAIT);
		}
		if (error != 0) {
			printf(LMC_PRINTF_FMT ": unable to load tx map, "
				"error = %d\n", LMC_PRINTF_ARGS, error);
#if defined(LMC_DEBUG)
			sc->lmc_dbg.dbg_txput_finishes[3]++;
#endif
			goto finish;
		}
	}
	if ((free -= (map->dm_nsegs + 1) / 2) <= 0
		/*
		 * See if there's any unclaimed space in the transmit ring.
		 */
		&& (free += lmc_tx_intr(sc)) <= 0) {
		/*
		 * There's no more room but since nothing
		 * has been committed at this point, just
		 * show output is active, put back the
		 * mbuf and return.
		 */
		sc->lmc_flags |= LMC_WANTTXSTART;
#if defined(LMC_DEBUG)
		sc->lmc_dbg.dbg_txput_finishes[4]++;
#endif
		bus_dmamap_unload(sc->lmc_dmatag, map);
		goto finish;
	}
	for (; map->dm_nsegs - segcnt > 1; segcnt += 2) {
		int flg;

		eop = nextout;
		flg	       = LMC_CTL_FLGS(letoh32(eop->d_ctl));
		flg	      &= TULIP_DFLAG_ENDRING;
		flg	      |= TULIP_DFLAG_TxNOPADDING;
		if (sc->ictl.crc_length == 16)
			flg |= TULIP_DFLAG_TxHASCRC;
		eop->d_status  = htole32(d_status);
		eop->d_addr1   = htole32(map->dm_segs[segcnt].ds_addr);
		eop->d_addr2   = htole32(map->dm_segs[segcnt+1].ds_addr);
		eop->d_ctl     = htole32(LMC_CTL(flg, 
				 map->dm_segs[segcnt].ds_len,
				 map->dm_segs[segcnt+1].ds_len));
		d_status = TULIP_DSTS_OWNER;
		if (++nextout == ri->ri_last)
			nextout = ri->ri_first;
	}
	if (segcnt < map->dm_nsegs) {
		int flg;

		eop = nextout;
		flg	       = LMC_CTL_FLGS(letoh32(eop->d_ctl));
		flg	      &= TULIP_DFLAG_ENDRING;
		flg	      |= TULIP_DFLAG_TxNOPADDING;
		if (sc->ictl.crc_length == 16)
			flg |= TULIP_DFLAG_TxHASCRC;
		eop->d_status  = htole32(d_status);
		eop->d_addr1   = htole32(map->dm_segs[segcnt].ds_addr);
		eop->d_addr2   = 0;
		eop->d_ctl     = htole32(LMC_CTL(flg, 
				 map->dm_segs[segcnt].ds_len, 0));
		if (++nextout == ri->ri_last)
			nextout = ri->ri_first;
	}
	LMC_TXMAP_PRESYNC(sc, map);
	LMC_SETCTX(m, map);
	map = NULL;
	--sc->lmc_txmaps_free;		/* commit to using the dmamap */

	/*
	 * The descriptors have been filled in.  Now get ready
	 * to transmit.
	 */
	IF_ENQUEUE(&sc->lmc_txq, m);
	m = NULL;

	/*
	 * Make sure the next descriptor after this packet is owned
	 * by us since it may have been set up above if we ran out
	 * of room in the ring.
	 */
	nextout->d_status = 0;
	LMC_TXDESC_PRESYNC(sc, nextout, sizeof(u_int32_t));

	/*
	 * Mark the last and first segments, indicate we want a transmit
	 * complete interrupt, and tell it to transmit!
	 */
	ctl = letoh32(eop->d_ctl);
	eop->d_ctl = htole32(LMC_CTL(
		LMC_CTL_FLGS(ctl)|TULIP_DFLAG_TxLASTSEG|TULIP_DFLAG_TxWANTINTR,
		LMC_CTL_LEN1(ctl),
		LMC_CTL_LEN2(ctl)));

	/*
	 * Note that ri->ri_nextout is still the start of the packet
	 * and until we set the OWNER bit, we can still back out of
	 * everything we have done.
	 */
	ctl = letoh32(ri->ri_nextout->d_ctl);
	ri->ri_nextout->d_ctl = htole32(LMC_CTL(
		LMC_CTL_FLGS(ctl)|TULIP_DFLAG_TxFIRSTSEG,
		LMC_CTL_LEN1(ctl),
		LMC_CTL_LEN2(ctl)));
	if (eop < ri->ri_nextout) {
		LMC_TXDESC_PRESYNC(sc, ri->ri_nextout,
			(caddr_t) ri->ri_last - (caddr_t) ri->ri_nextout);
		LMC_TXDESC_PRESYNC(sc, ri->ri_first,
			(caddr_t) (eop + 1) - (caddr_t) ri->ri_first);
	} else {
		LMC_TXDESC_PRESYNC(sc, ri->ri_nextout,
			(caddr_t) (eop + 1) - (caddr_t) ri->ri_nextout);
	}
	ri->ri_nextout->d_status = htole32(TULIP_DSTS_OWNER);
	LMC_TXDESC_PRESYNC(sc, ri->ri_nextout, sizeof(u_int32_t));

	LMC_CSR_WRITE(sc, csr_txpoll, 1);

	/*
	 * This advances the ring for us.
	 */
	ri->ri_nextout = nextout;
	ri->ri_free = free;

	/*
	 * switch back to the single queueing ifstart.
	 */
	sc->lmc_flags &= ~LMC_WANTTXSTART;
	sc->lmc_if.if_start = lmc_ifstart_one;

	/*
	 * If we want a txstart, there must be not enough space in the
	 * transmit ring.  So we want to enable transmit done interrupts
	 * so we can immediately reclaim some space.  When the transmit
	 * interrupt is posted, the interrupt handler will call tx_intr
	 * to reclaim space and then txstart (since WANTTXSTART is set).
	 * txstart will move the packet into the transmit ring and clear
	 * WANTTXSTART thereby causing TXINTR to be cleared.
	 */
 finish:
	if (sc->lmc_flags & LMC_WANTTXSTART) {
		sc->lmc_if.if_flags |= IFF_OACTIVE;
		sc->lmc_if.if_start = lmc_ifstart;
	}

	return m;
}


/*
 * This routine is entered at splnet()
 */
static int
lmc_ifioctl(struct ifnet * ifp, ioctl_cmd_t cmd, caddr_t data)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
	int s;
	struct proc *p = curproc;
	int error = 0;
	struct ifreq *ifr = (struct ifreq *)data;
	u_int32_t new_state;
	u_int32_t old_state;
	lmc_ctl_t ctl;

	s = LMC_RAISESPL();

	switch (cmd) {
	case LMCIOCGINFO:
		error = copyout(&sc->ictl, ifr->ifr_data, sizeof(lmc_ctl_t));

		goto out;
		break;

	case LMCIOCSINFO:
		error = suser(p, 0);
		if (error)
			goto out;

		error = copyin(ifr->ifr_data, &ctl, sizeof(lmc_ctl_t));
		if (error != 0)
			goto out;

		sc->lmc_media->set_status(sc, &ctl);

		goto out;
		break;

	case SIOCSIFMTU:
		/*
		 * Don't allow the MTU to get larger than we can handle
		 */
		if (ifr->ifr_mtu > LMC_MTU) {
			error = EINVAL;
			goto out;
		} else {
                        ifp->if_mtu = ifr->ifr_mtu;
		}
		break;
	}

	/*
	 * call the sppp ioctl layer
	 */
	error = sppp_ioctl(ifp, cmd, data);
	if (error != 0)
		goto out;

	/*
	 * If we are transitioning from up to down or down to up, call
	 * our init routine.
	 */
	new_state = ifp->if_flags & IFF_UP;
	old_state = sc->lmc_flags & LMC_IFUP;

	if (new_state && !old_state)
		lmc_ifup(sc);
	else if (!new_state && old_state)
		lmc_ifdown(sc);

 out:
	LMC_RESTORESPL(s);

	return error;
}

/*
 * These routines gets called at device spl (from sppp_output).
 */

static ifnet_ret_t
lmc_ifstart(struct ifnet * const ifp)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
	struct mbuf *m, *m0;

	if (sc->lmc_flags & LMC_IFUP) {
		while (sppp_isempty(ifp) == 0) {
			m = sppp_pick(ifp);
			if (m == NULL)
				break;
			if ((m = lmc_txput(sc, m)) != NULL)
				break;
			m0 = sppp_dequeue(ifp);
#if defined(LMC_DEBUG)
			if (m0 != m)
				printf("lmc_ifstart: mbuf mismatch!\n");
#endif
		}
		LMC_CSR_WRITE(sc, csr_txpoll, 1);
	}
}

static ifnet_ret_t
lmc_ifstart_one(struct ifnet * const ifp)
{
	lmc_softc_t * const sc = LMC_IFP_TO_SOFTC(ifp);
	struct mbuf *m, *m0;

	if ((sc->lmc_flags & LMC_IFUP) && (sppp_isempty(ifp) == 0)) {
		m = sppp_pick(ifp);
		if ((m = lmc_txput(sc, m)) != NULL)
			return;
		m0 = sppp_dequeue(ifp);
#if defined(LMC_DEBUG)
		if (m0 != m)
			printf("lmc_ifstart: mbuf mismatch!\n");
#endif
		LMC_CSR_WRITE(sc, csr_txpoll, 1);
	}
}

/*
 * Set up the OS interface magic and attach to the operating system
 * network services.
 */
void
lmc_attach(lmc_softc_t * const sc)
{
	struct ifnet * const ifp = &sc->lmc_if;

	ifp->if_flags = IFF_POINTOPOINT | IFF_MULTICAST;
	ifp->if_ioctl = lmc_ifioctl;
	ifp->if_start = lmc_ifstart;
	ifp->if_watchdog = lmc_watchdog;
	ifp->if_timer = 1;
	ifp->if_mtu = LMC_MTU;
	IFQ_SET_MAXLEN(&ifp->if_snd, IFQ_MAXLEN);
	IFQ_SET_READY(&ifp->if_snd);
  
	if_attach(ifp);
	if_alloc_sadl(ifp);

	sppp_attach((struct ifnet *)&sc->lmc_sppp);
	sc->lmc_sppp.pp_flags = PP_CISCO | PP_KEEPALIVE;
	sc->lmc_sppp.pp_framebytes = 3;

#if NBPFILTER > 0
	LMC_BPF_ATTACH(sc);
#endif

	/*
	 * turn off those LEDs...
	 */
	sc->lmc_miireg16 |= LMC_MII16_LED_ALL;
	/*
	 * for DS3 & DS1 adapters light the green light, led2
	 */
	if (sc->lmc_cardtype == LMC_CARDTYPE_DS3 ||
	    sc->lmc_cardtype == LMC_CARDTYPE_T1)
		lmc_led_on (sc, LMC_MII16_LED2);
	else
		lmc_led_on (sc, LMC_MII16_LED0 | LMC_MII16_LED2);
}

void
lmc_initring(lmc_softc_t * const sc, lmc_ringinfo_t * const ri,
	       lmc_desc_t *descs, int ndescs)
{
	ri->ri_max = ndescs;
	ri->ri_first = descs;
	ri->ri_last = ri->ri_first + ri->ri_max;
	bzero((caddr_t) ri->ri_first, sizeof(ri->ri_first[0]) * ri->ri_max);
	ri->ri_last[-1].d_ctl = htole32(LMC_CTL(TULIP_DFLAG_ENDRING, 0, 0));
}
