/*	$OpenBSD: if_lmc_common.c,v 1.13 2011/07/06 02:42:28 henning Exp $ */
/*	$NetBSD: if_lmc_common.c,v 1.1 1999/03/25 03:32:43 explorer Exp $	*/

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

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/mbuf.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/errno.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
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

#define d_length1 u.bd_length1
#define d_length2 u.bd_length2
#define d_flag u.bd_flag

#include <dev/pci/if_lmc_types.h>
#include <dev/pci/if_lmcioctl.h>
#include <dev/pci/if_lmcvar.h>

void
lmc_gpio_mkinput(lmc_softc_t * const sc, u_int32_t bits)
{
	sc->lmc_gpio_io &= ~bits;
	LMC_CSR_WRITE(sc, csr_gp, TULIP_GP_PINSET | (sc->lmc_gpio_io));
}

void
lmc_gpio_mkoutput(lmc_softc_t * const sc, u_int32_t bits)
{
	sc->lmc_gpio_io |= bits;
	LMC_CSR_WRITE(sc, csr_gp, TULIP_GP_PINSET | (sc->lmc_gpio_io));
}

void
lmc_led_on(lmc_softc_t * const sc, u_int32_t led)
{
	sc->lmc_miireg16 &= ~led;
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

void
lmc_led_off(lmc_softc_t * const sc, u_int32_t led)
{
	sc->lmc_miireg16 |= led;
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);
}

void
lmc_reset(lmc_softc_t * const sc)
{
	sc->lmc_miireg16 |= LMC_MII16_FIFO_RESET;
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);

	sc->lmc_miireg16 &= ~LMC_MII16_FIFO_RESET;
	lmc_mii_writereg(sc, 0, 16, sc->lmc_miireg16);

	/*
	 * make some of the GPIO pins be outputs
	 */
	lmc_gpio_mkoutput(sc, LMC_GEP_DP | LMC_GEP_RESET);

	/*
	 * drive DP and RESET low to force configuration.  This also forces
	 * the transmitter clock to be internal, but we expect to reset
	 * that later anyway.
	 */
	sc->lmc_gpio &= ~(LMC_GEP_DP | LMC_GEP_RESET);
	LMC_CSR_WRITE(sc, csr_gp, sc->lmc_gpio);

	/*
	 * hold for more than 10 microseconds
	 */
	DELAY(50);

	/*
	 * stop driving Xilinx-related signals
	 */
	lmc_gpio_mkinput(sc, LMC_GEP_DP | LMC_GEP_RESET);

	/*
	 * busy wait for the chip to reset
	 */
	while ((LMC_CSR_READ(sc, csr_gp) & LMC_GEP_DP) == 0);

	/*
	 * Call media specific init routine
	 */
	sc->lmc_media->init(sc);
}

void
lmc_dec_reset(lmc_softc_t * const sc)
{
#ifndef __linux__
	lmc_ringinfo_t *ri;
	lmc_desc_t *di;
#endif
	u_int32_t val;

	/*
	 * disable all interrupts
	 */
	sc->lmc_intrmask = 0;
	LMC_CSR_WRITE(sc, csr_intr, sc->lmc_intrmask);

	/*
	 * we are, obviously, down.
	 */
#ifndef __linux__
	sc->lmc_flags &= ~(LMC_IFUP | LMC_MODEMOK);

	DP(("lmc_dec_reset\n"));
#endif

	/*
	 * Reset the chip with a software reset command.
	 * Wait 10 microseconds (actually 50 PCI cycles but at 
	 * 33MHz that comes to two microseconds but wait a
	 * bit longer anyways)
	 */
	LMC_CSR_WRITE(sc, csr_busmode, TULIP_BUSMODE_SWRESET);
	DELAY(10);
	sc->lmc_cmdmode = LMC_CSR_READ(sc, csr_command);

	/*
	 * We want:
	 *   no ethernet address in frames we write
	 *   disable padding (txdesc, padding disable)
	 *   ignore runt frames (rdes0 bit 15)
	 *   no receiver watchdog or transmitter jabber timer
	 *       (csr15 bit 0,14 == 1)
	 *   if using 16-bit CRC, turn off CRC (trans desc, crc disable)
	 */

#ifndef TULIP_CMD_RECEIVEALL
#define TULIP_CMD_RECEIVEALL 0x40000000L
#endif

	sc->lmc_cmdmode |= ( TULIP_CMD_PROMISCUOUS
			       | TULIP_CMD_FULLDUPLEX
			       | TULIP_CMD_PASSBADPKT
			       | TULIP_CMD_NOHEARTBEAT
			       | TULIP_CMD_PORTSELECT
			       | TULIP_CMD_RECEIVEALL
			       | TULIP_CMD_MUSTBEONE
			       );
	sc->lmc_cmdmode &= ~( TULIP_CMD_OPERMODE
				| TULIP_CMD_THRESHOLDCTL
				| TULIP_CMD_STOREFWD
				| TULIP_CMD_TXTHRSHLDCTL
				);

	LMC_CSR_WRITE(sc, csr_command, sc->lmc_cmdmode);

	/*
	 * disable receiver watchdog and transmit jabber
	 */
	val = LMC_CSR_READ(sc, csr_sia_general);
	val |= (TULIP_WATCHDOG_TXDISABLE | TULIP_WATCHDOG_RXDISABLE);
	LMC_CSR_WRITE(sc, csr_sia_general, val);

	/*
	 * turn off those LEDs...
	 */
	sc->lmc_miireg16 |= LMC_MII16_LED_ALL;
	lmc_led_on(sc, LMC_MII16_LED0);

#ifndef __linux__
	/*
	 * reprogram the tx desc, rx desc, and PCI bus options
	 */
	LMC_CSR_WRITE(sc, csr_txlist, sc->lmc_txdescmap->dm_segs[0].ds_addr);
	LMC_CSR_WRITE(sc, csr_rxlist, sc->lmc_rxdescmap->dm_segs[0].ds_addr);
	LMC_CSR_WRITE(sc, csr_busmode,
		(1 << (LMC_BURSTSIZE(sc->lmc_unit) + 8))
		|TULIP_BUSMODE_CACHE_ALIGN8
		|TULIP_BUSMODE_READMULTIPLE);

	IFQ_SET_MAXLEN(&sc->lmc_txq, LMC_TXDESCS);

	/*
	 * Free all the mbufs that were on the transmit ring.
	 */
	for (;;) {
		bus_dmamap_t map;
		struct mbuf *m;

		IF_DEQUEUE(&sc->lmc_txq, m);
		if (m == NULL)
			break;
		map = LMC_GETCTX(m, bus_dmamap_t);
		bus_dmamap_unload(sc->lmc_dmatag, map);
		sc->lmc_txmaps[sc->lmc_txmaps_free++] = map;
		m_freem(m);
	}

	/*
	 * reset descriptor state and reclaim all descriptors.
	 */
	ri = &sc->lmc_txinfo;
	ri->ri_nextin = ri->ri_nextout = ri->ri_first;
	ri->ri_free = ri->ri_max;
	for (di = ri->ri_first; di < ri->ri_last; di++)
		di->d_status = 0;
	bus_dmamap_sync(sc->lmc_dmatag, sc->lmc_txdescmap,
		0, sc->lmc_txdescmap->dm_mapsize,
		BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

	/*
	 * We need to collect all the mbufs were on the 
	 * receive ring before we reinit it either to put
	 * them back on or to know if we have to allocate
	 * more.
	 */
	ri = &sc->lmc_rxinfo;
	ri->ri_nextin = ri->ri_nextout = ri->ri_first;
	ri->ri_free = ri->ri_max;
	for (di = ri->ri_first; di < ri->ri_last; di++) {
		u_int32_t ctl = di->d_ctl;
		di->d_status = 0;
		di->d_ctl = LMC_CTL(LMC_CTL_FLGS(ctl),0,0);
		di->d_addr1 = 0;
		di->d_addr2 = 0;
	}
	bus_dmamap_sync(sc->lmc_dmatag, sc->lmc_rxdescmap,
		0, sc->lmc_rxdescmap->dm_mapsize,
		BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	for (;;) {
		bus_dmamap_t map;
		struct mbuf *m;
		IF_DEQUEUE(&sc->lmc_rxq, m);
		if (m == NULL)
			break;
		map = LMC_GETCTX(m, bus_dmamap_t);
		bus_dmamap_unload(sc->lmc_dmatag, map);
		sc->lmc_rxmaps[sc->lmc_rxmaps_free++] = map;
		m_freem(m);
	}
#endif
}

void
lmc_initcsrs(lmc_softc_t * const sc, lmc_csrptr_t csr_base,
	     size_t csr_size)
{
	sc->lmc_csrs.csr_busmode	= csr_base +  0 * csr_size;
	sc->lmc_csrs.csr_txpoll		= csr_base +  1 * csr_size;
	sc->lmc_csrs.csr_rxpoll		= csr_base +  2 * csr_size;
	sc->lmc_csrs.csr_rxlist		= csr_base +  3 * csr_size;
	sc->lmc_csrs.csr_txlist		= csr_base +  4 * csr_size;
	sc->lmc_csrs.csr_status		= csr_base +  5 * csr_size;
	sc->lmc_csrs.csr_command	= csr_base +  6 * csr_size;
	sc->lmc_csrs.csr_intr		= csr_base +  7 * csr_size;
	sc->lmc_csrs.csr_missed_frames	= csr_base +  8 * csr_size;
	sc->lmc_csrs.csr_9		= csr_base +  9 * csr_size;
	sc->lmc_csrs.csr_10		= csr_base + 10 * csr_size;
	sc->lmc_csrs.csr_11		= csr_base + 11 * csr_size;
	sc->lmc_csrs.csr_12		= csr_base + 12 * csr_size;
	sc->lmc_csrs.csr_13		= csr_base + 13 * csr_size;
	sc->lmc_csrs.csr_14		= csr_base + 14 * csr_size;
	sc->lmc_csrs.csr_15		= csr_base + 15 * csr_size;
}
