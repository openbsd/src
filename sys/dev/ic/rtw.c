/*	$OpenBSD: rtw.c,v 1.8 2005/01/19 11:07:32 jsg Exp $	*/
/* $NetBSD: rtw.c,v 1.29 2004/12/27 19:49:16 dyoung Exp $ */
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
 * Device driver for the Realtek RTL8180 802.11 MAC/BBP.
 */

#include <sys/cdefs.h>
#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h> 
#include <sys/mbuf.h>   
#include <sys/malloc.h>
#include <sys/kernel.h>
#if 0
#include <sys/errno.h>
#include <sys/device.h>
#endif
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include <machine/endian.h>
#include <machine/bus.h>
#include <machine/intr.h>	/* splnet */

#include <uvm/uvm_extern.h>
 
#include <net/if.h>
#include <net/if_media.h>

#if NBPFILTER > 0 
#include <net/bpf.h>
#endif 

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_compat.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/ic/rtwreg.h>
#include <dev/ic/rtwvar.h>
#include <dev/ic/rtwphyio.h>
#include <dev/ic/rtwphy.h>

#include <dev/ic/smc93cx6var.h>

#define	KASSERT2(__cond, __msg)		\
	do {				\
		if (!(__cond))		\
			panic __msg ;	\
	} while (0)

int rtw_rfprog_fallback = 0;
int rtw_host_rfio = 0;
int rtw_flush_rfio = 1;
int rtw_rfio_delay = 0;

#ifdef RTW_DEBUG
int rtw_debug = 0;
#endif /* RTW_DEBUG */

#define NEXT_ATTACH_STATE(sc, state) do {			\
	DPRINTF(sc, RTW_DEBUG_ATTACH,				\
	    ("%s: attach state %s\n", __func__, #state));	\
	sc->sc_attach_state = state;				\
} while (0)

int rtw_dwelltime = 200;	/* milliseconds per channel */

void rtw_start(struct ifnet *);
void rtw_srom_defaults(struct rtw_srom *, u_int32_t *, u_int8_t *,
    enum rtw_rfchipid *, u_int32_t *);
void rtw_txdesc_blk_init_all(struct rtw_txdesc_blk *);
void rtw_txctl_blk_init_all(struct rtw_txctl_blk *);
void rtw_txdescs_sync(bus_dma_tag_t, bus_dmamap_t , struct rtw_txdesc_blk *,
    u_int, u_int, int);
void rtw_txdescs_sync_all(bus_dma_tag_t, bus_dmamap_t,
    struct rtw_txdesc_blk *);
void rtw_rxbufs_release(bus_dma_tag_t, struct rtw_rxctl *);
void rtw_rxdesc_init_all(bus_dma_tag_t, bus_dmamap_t,
    struct rtw_rxdesc *, struct rtw_rxctl *, int);
void rtw_io_enable(struct rtw_regs *, u_int8_t, int);
void rtw_intr_rx(struct rtw_softc *, u_int16_t);
void rtw_intr_beacon(struct rtw_softc *, u_int16_t);
void rtw_intr_atim(struct rtw_softc *);
void rtw_transmit_config(struct rtw_regs *);
void rtw_pktfilt_load(struct rtw_softc *);
void rtw_start(struct ifnet *);
void rtw_watchdog(struct ifnet *);
void rtw_start_beacon(struct rtw_softc *, int);
void rtw_next_scan(void *);
void rtw_recv_mgmt(struct ieee80211com *, struct mbuf *,
    struct ieee80211_node *, int, int, u_int32_t);
struct ieee80211_node * rtw_node_alloc(struct ieee80211com *);
void rtw_node_free(struct ieee80211com *, struct ieee80211_node *);
void rtw_media_status(struct ifnet *, struct ifmediareq *);
void rtw_txctl_blk_cleanup_all(struct rtw_softc *);
void rtw_txdesc_blk_setup(struct rtw_txdesc_blk *, struct rtw_txdesc *,
    u_int, bus_addr_t, bus_addr_t);
void rtw_txdesc_blk_setup_all(struct rtw_softc *);
void rtw_intr_tx(struct rtw_softc *, u_int16_t);
void rtw_intr_ioerror(struct rtw_softc *, u_int16_t);
void rtw_intr_timeout(struct rtw_softc *);
void rtw_stop(struct ifnet *, int);
void rtw_maxim_pwrstate(struct rtw_regs *, enum rtw_pwrstate, int, int);
void rtw_philips_pwrstate(struct rtw_regs *, enum rtw_pwrstate, int, int);
void rtw_pwrstate0(struct rtw_softc *, enum rtw_pwrstate, int, int);
void rtw_recv_beacon(struct rtw_softc *, struct mbuf *,
    struct ieee80211_node *, int, int, u_int32_t);
void rtw_join_bss(struct rtw_softc *, uint8_t *, enum ieee80211_opmode,
    uint16_t);
void rtw_set_access1(struct rtw_regs *, enum rtw_access, enum rtw_access);
int rtw_srom_parse(struct rtw_srom *, u_int32_t *, u_int8_t *,
    enum rtw_rfchipid *, u_int32_t *, enum rtw_locale *, const char *);
int rtw_srom_read(struct rtw_regs *, u_int32_t, struct rtw_srom *,
    const char *);
void rtw_set_rfprog(struct rtw_regs *, enum rtw_rfchipid,
    const char *);
u_int8_t rtw_chan2txpower(struct rtw_srom *, struct ieee80211com *,
    struct ieee80211_channel *);
int rtw_txctl_blk_init(struct rtw_txctl_blk *);
int rtw_rxctl_init_all(bus_dma_tag_t, struct rtw_rxctl *, u_int *,
    const char *);
void rtw_txbuf_release(bus_dma_tag_t, struct ieee80211com *,
    struct rtw_txctl *);
void rtw_txbufs_release(bus_dma_tag_t, bus_dmamap_t,
    struct ieee80211com *, struct rtw_txctl_blk *);
void rtw_hwring_setup(struct rtw_softc *);
void rtw_swring_setup(struct rtw_softc *);
void rtw_txdesc_blk_reset(struct rtw_txdesc_blk *);
void rtw_txdescs_reset(struct rtw_softc *);
void rtw_rxdescs_reset(struct rtw_softc *);
void rtw_rfmd_pwrstate(struct rtw_regs *, enum rtw_pwrstate, int, int);
int rtw_pwrstate(struct rtw_softc *, enum rtw_pwrstate);
int rtw_tune(struct rtw_softc *);
void rtw_set_nettype(struct rtw_softc *, enum ieee80211_opmode);
int rtw_init(struct ifnet *);
int rtw_ioctl(struct ifnet *, u_long, caddr_t);
int rtw_seg_too_short(bus_dmamap_t);
struct mbuf * rtw_dmamap_load_txbuf(bus_dma_tag_t, bus_dmamap_t, struct mbuf *,
    u_int, short *, const char *);
int rtw_newstate(struct ieee80211com *, enum ieee80211_state, int);
int rtw_media_change(struct ifnet *);
int rtw_txctl_blk_setup_all(struct rtw_softc *);
struct rtw_rf * rtw_rf_attach(struct rtw_softc *, enum rtw_rfchipid, int);
u_int8_t rtw_check_phydelay(struct rtw_regs *, u_int32_t);
int rtw_chip_reset1(struct rtw_regs *, const char *);
int rtw_chip_reset(struct rtw_regs *, const char *);
int rtw_recall_eeprom(struct rtw_regs *, const char *);
int rtw_reset(struct rtw_softc *);
int rtw_txdesc_dmamaps_create(bus_dma_tag_t, struct rtw_txctl *, u_int);
int rtw_rxdesc_dmamaps_create(bus_dma_tag_t, struct rtw_rxctl *, u_int);
void rtw_rxctls_setup(struct rtw_rxctl *);
void rtw_rxdesc_dmamaps_destroy(bus_dma_tag_t, struct rtw_rxctl *, u_int);
void rtw_txdesc_dmamaps_destroy(bus_dma_tag_t, struct rtw_txctl *, u_int);
void rtw_srom_free(struct rtw_srom *);
void rtw_init_channels(enum rtw_locale, struct ieee80211_channel (*)[],
    const char*);
void rtw_identify_country(struct rtw_regs *, enum rtw_locale *, const char *);
int rtw_identify_sta(struct rtw_regs *, u_int8_t (*)[], const char *);
void rtw_rxdescs_sync(bus_dma_tag_t, bus_dmamap_t, u_int, u_int, int);
int rtw_rxbuf_alloc(bus_dma_tag_t, struct rtw_rxctl *);
void rtw_rxdesc_init(bus_dma_tag_t, bus_dmamap_t, struct rtw_rxdesc *,
    struct rtw_rxctl *, int, int);
void rtw_collect_txpkt(struct rtw_softc *, struct rtw_txdesc_blk *,
    struct rtw_txctl *, int);
void rtw_collect_txring(struct rtw_softc *, struct rtw_txctl_blk *,
    struct rtw_txdesc_blk *);
void rtw_suspend_ticks(struct rtw_softc *);
void rtw_resume_ticks(struct rtw_softc *);
void rtw_enable_interrupts(struct rtw_softc *);
int rtw_dequeue(struct ifnet *, struct rtw_txctl_blk **,
    struct rtw_txdesc_blk **, struct mbuf **,
    struct ieee80211_node **);
void rtw_setifprops(struct ifnet *, const char *, void *);
void rtw_set80211props(struct ieee80211com *);
void rtw_set80211methods(struct rtw_mtbl *, struct ieee80211com *);
void rtw_establish_hooks(struct rtw_hooks *, const char *, void *);
void rtw_disestablish_hooks(struct rtw_hooks *, const char *, void *);
void rtw_init_radiotap(struct rtw_softc *);
int rtw_txctl_blk_setup(struct rtw_txctl_blk *, u_int);


#ifdef RTW_DEBUG
void rtw_print_txdesc(struct rtw_softc *, const char *,
    struct rtw_txctl *, struct rtw_txdesc_blk *, int);
const char * rtw_access_string(enum rtw_access);
void rtw_dump_rings(struct rtw_softc *);
void rtw_print_txdesc(struct rtw_softc *, const char *,
    struct rtw_txctl *, struct rtw_txdesc_blk *, int);
void rtw_print_regs(struct rtw_regs *, const char *, const char *);
#endif

struct cfdriver rtw_cd = {
    NULL, "rtw", DV_IFNET
};

#ifdef RTW_DEBUG
void
rtw_print_regs(struct rtw_regs *regs, const char *dvname, const char *where)
{
#define PRINTREG32(sc, reg)				\
	RTW_DPRINTF(RTW_DEBUG_REGDUMP,			\
	    ("%s: reg[ " #reg " / %03x ] = %08x\n",	\
	    dvname, reg, RTW_READ(regs, reg)))

#define PRINTREG16(sc, reg)				\
	RTW_DPRINTF(RTW_DEBUG_REGDUMP,			\
	    ("%s: reg[ " #reg " / %03x ] = %04x\n",	\
	    dvname, reg, RTW_READ16(regs, reg)))

#define PRINTREG8(sc, reg)				\
	RTW_DPRINTF(RTW_DEBUG_REGDUMP,			\
	    ("%s: reg[ " #reg " / %03x ] = %02x\n",	\
	    dvname, reg, RTW_READ8(regs, reg)))

	RTW_DPRINTF(RTW_DEBUG_REGDUMP, ("%s: %s\n", dvname, where));

	PRINTREG32(regs, RTW_IDR0);
	PRINTREG32(regs, RTW_IDR1);
	PRINTREG32(regs, RTW_MAR0);
	PRINTREG32(regs, RTW_MAR1);
	PRINTREG32(regs, RTW_TSFTRL);
	PRINTREG32(regs, RTW_TSFTRH);
	PRINTREG32(regs, RTW_TLPDA);
	PRINTREG32(regs, RTW_TNPDA);
	PRINTREG32(regs, RTW_THPDA);
	PRINTREG32(regs, RTW_TCR);
	PRINTREG32(regs, RTW_RCR);
	PRINTREG32(regs, RTW_TINT);
	PRINTREG32(regs, RTW_TBDA);
	PRINTREG32(regs, RTW_ANAPARM);
	PRINTREG32(regs, RTW_BB);
	PRINTREG32(regs, RTW_PHYCFG);
	PRINTREG32(regs, RTW_WAKEUP0L);
	PRINTREG32(regs, RTW_WAKEUP0H);
	PRINTREG32(regs, RTW_WAKEUP1L);
	PRINTREG32(regs, RTW_WAKEUP1H);
	PRINTREG32(regs, RTW_WAKEUP2LL);
	PRINTREG32(regs, RTW_WAKEUP2LH);
	PRINTREG32(regs, RTW_WAKEUP2HL);
	PRINTREG32(regs, RTW_WAKEUP2HH);
	PRINTREG32(regs, RTW_WAKEUP3LL);
	PRINTREG32(regs, RTW_WAKEUP3LH);
	PRINTREG32(regs, RTW_WAKEUP3HL);
	PRINTREG32(regs, RTW_WAKEUP3HH);
	PRINTREG32(regs, RTW_WAKEUP4LL);
	PRINTREG32(regs, RTW_WAKEUP4LH);
	PRINTREG32(regs, RTW_WAKEUP4HL);
	PRINTREG32(regs, RTW_WAKEUP4HH);
	PRINTREG32(regs, RTW_DK0);
	PRINTREG32(regs, RTW_DK1);
	PRINTREG32(regs, RTW_DK2);
	PRINTREG32(regs, RTW_DK3);
	PRINTREG32(regs, RTW_RETRYCTR);
	PRINTREG32(regs, RTW_RDSAR);
	PRINTREG32(regs, RTW_FER);
	PRINTREG32(regs, RTW_FEMR);
	PRINTREG32(regs, RTW_FPSR);
	PRINTREG32(regs, RTW_FFER);

	/* 16-bit registers */
	PRINTREG16(regs, RTW_BRSR);
	PRINTREG16(regs, RTW_IMR);
	PRINTREG16(regs, RTW_ISR);
	PRINTREG16(regs, RTW_BCNITV);
	PRINTREG16(regs, RTW_ATIMWND);
	PRINTREG16(regs, RTW_BINTRITV);
	PRINTREG16(regs, RTW_ATIMTRITV);
	PRINTREG16(regs, RTW_CRC16ERR);
	PRINTREG16(regs, RTW_CRC0);
	PRINTREG16(regs, RTW_CRC1);
	PRINTREG16(regs, RTW_CRC2);
	PRINTREG16(regs, RTW_CRC3);
	PRINTREG16(regs, RTW_CRC4);
	PRINTREG16(regs, RTW_CWR);

	/* 8-bit registers */
	PRINTREG8(regs, RTW_CR);
	PRINTREG8(regs, RTW_9346CR);
	PRINTREG8(regs, RTW_CONFIG0);
	PRINTREG8(regs, RTW_CONFIG1);
	PRINTREG8(regs, RTW_CONFIG2);
	PRINTREG8(regs, RTW_MSR);
	PRINTREG8(regs, RTW_CONFIG3);
	PRINTREG8(regs, RTW_CONFIG4);
	PRINTREG8(regs, RTW_TESTR);
	PRINTREG8(regs, RTW_PSR);
	PRINTREG8(regs, RTW_SCR);
	PRINTREG8(regs, RTW_PHYDELAY);
	PRINTREG8(regs, RTW_CRCOUNT);
	PRINTREG8(regs, RTW_PHYADDR);
	PRINTREG8(regs, RTW_PHYDATAW);
	PRINTREG8(regs, RTW_PHYDATAR);
	PRINTREG8(regs, RTW_CONFIG5);
	PRINTREG8(regs, RTW_TPPOLL);

	PRINTREG16(regs, RTW_BSSID16);
	PRINTREG32(regs, RTW_BSSID32);
#undef PRINTREG32
#undef PRINTREG16
#undef PRINTREG8
}
#endif /* RTW_DEBUG */


void
rtw_continuous_tx_enable(struct rtw_softc *sc, int enable)
{
	struct rtw_regs *regs = &sc->sc_regs;

	u_int32_t tcr;
	tcr = RTW_READ(regs, RTW_TCR);
	tcr &= ~RTW_TCR_LBK_MASK;
	if (enable)
		tcr |= RTW_TCR_LBK_CONT;
	else
		tcr |= RTW_TCR_LBK_NORMAL;
	RTW_WRITE(regs, RTW_TCR, tcr);
	RTW_SYNC(regs, RTW_TCR, RTW_TCR);
	rtw_set_access(sc, RTW_ACCESS_ANAPARM);
	rtw_txdac_enable(sc, !enable);
	rtw_set_access(sc, RTW_ACCESS_ANAPARM); /* XXX Voodoo from Linux. */
	rtw_set_access(sc, RTW_ACCESS_NONE);
}

#ifdef RTW_DEBUG
const char *
rtw_access_string(enum rtw_access access)
{
        switch (access) {
        case RTW_ACCESS_NONE:
                return "none";
        case RTW_ACCESS_CONFIG:
                return "config";
        case RTW_ACCESS_ANAPARM:
                return "anaparm";
        default:
                return "unknown";
        }
}
#endif

void
rtw_set_access1(struct rtw_regs *regs,
    enum rtw_access oaccess, enum rtw_access naccess)
{
	KASSERT(naccess >= RTW_ACCESS_NONE && naccess <= RTW_ACCESS_ANAPARM);
	KASSERT(oaccess >= RTW_ACCESS_NONE && oaccess <= RTW_ACCESS_ANAPARM);

	if (naccess == oaccess)
		return;

	switch (naccess) {
	case RTW_ACCESS_NONE:
		switch (oaccess) {
		case RTW_ACCESS_ANAPARM:
			rtw_anaparm_enable(regs, 0);
			/*FALLTHROUGH*/
		case RTW_ACCESS_CONFIG:
			rtw_config0123_enable(regs, 0);
			/*FALLTHROUGH*/
		case RTW_ACCESS_NONE:
			break;
		}
		break;
	case RTW_ACCESS_CONFIG:
		switch (oaccess) {
		case RTW_ACCESS_NONE:
			rtw_config0123_enable(regs, 1);
			/*FALLTHROUGH*/
		case RTW_ACCESS_CONFIG:
			break;
		case RTW_ACCESS_ANAPARM:
			rtw_anaparm_enable(regs, 0);
			break;
		}
		break;
	case RTW_ACCESS_ANAPARM:
		switch (oaccess) {
		case RTW_ACCESS_NONE:
			rtw_config0123_enable(regs, 1);
			/*FALLTHROUGH*/
		case RTW_ACCESS_CONFIG:
			rtw_anaparm_enable(regs, 1);
			/*FALLTHROUGH*/
		case RTW_ACCESS_ANAPARM:
			break;
		}
		break;
	}
}

void
rtw_set_access(struct rtw_softc *sc, enum rtw_access access)
{
	rtw_set_access1(&sc->sc_regs, sc->sc_access, access);
	RTW_DPRINTF(RTW_DEBUG_ACCESS,
	    ("%s: access %s -> %s\n", sc->sc_dev.dv_xname,
	    rtw_access_string(sc->sc_access),
	    rtw_access_string(access)));
	sc->sc_access = access;
}

/*
 * Enable registers, switch register banks.
 */
void
rtw_config0123_enable(struct rtw_regs *regs, int enable)
{
	u_int8_t ecr;
	ecr = RTW_READ8(regs, RTW_9346CR);
	ecr &= ~(RTW_9346CR_EEM_MASK | RTW_9346CR_EECS | RTW_9346CR_EESK);
	if (enable)
		ecr |= RTW_9346CR_EEM_CONFIG;
	else {
		RTW_WBW(regs, RTW_9346CR, MAX(RTW_CONFIG0, RTW_CONFIG3));
		ecr |= RTW_9346CR_EEM_NORMAL;
	}
	RTW_WRITE8(regs, RTW_9346CR, ecr);
	RTW_SYNC(regs, RTW_9346CR, RTW_9346CR);
}

/* requires rtw_config0123_enable(, 1) */
void
rtw_anaparm_enable(struct rtw_regs *regs, int enable)
{
	u_int8_t cfg3;

	cfg3 = RTW_READ8(regs, RTW_CONFIG3);
	cfg3 |= RTW_CONFIG3_CLKRUNEN;
	if (enable)
		cfg3 |= RTW_CONFIG3_PARMEN;
	else
		cfg3 &= ~RTW_CONFIG3_PARMEN;
	RTW_WRITE8(regs, RTW_CONFIG3, cfg3);
	RTW_SYNC(regs, RTW_CONFIG3, RTW_CONFIG3);
}

/* requires rtw_anaparm_enable(, 1) */
void
rtw_txdac_enable(struct rtw_softc *sc, int enable)
{
	u_int32_t anaparm;
	struct rtw_regs *regs = &sc->sc_regs;

	anaparm = RTW_READ(regs, RTW_ANAPARM);
	if (enable)
		anaparm &= ~RTW_ANAPARM_TXDACOFF;
	else
		anaparm |= RTW_ANAPARM_TXDACOFF;
	RTW_WRITE(regs, RTW_ANAPARM, anaparm);
	RTW_SYNC(regs, RTW_ANAPARM, RTW_ANAPARM);
}

__inline int
rtw_chip_reset1(struct rtw_regs *regs, const char *dvname)
{
	u_int8_t cr;
	int i;

	RTW_WRITE8(regs, RTW_CR, RTW_CR_RST);

	RTW_WBR(regs, RTW_CR, RTW_CR);

	for (i = 0; i < 1000; i++) {
		if ((cr = RTW_READ8(regs, RTW_CR) & RTW_CR_RST) == 0) {
			RTW_DPRINTF(RTW_DEBUG_RESET,
			    ("%s: reset in %dus\n", dvname, i));
			return 0;
		}
		RTW_RBR(regs, RTW_CR, RTW_CR);
		DELAY(10); /* 10us */
	}

	printf("%s: reset failed\n", dvname);
	return ETIMEDOUT;
}

__inline int
rtw_chip_reset(struct rtw_regs *regs, const char *dvname)
{
	uint32_t tcr;

	/* from Linux driver */
	tcr = RTW_TCR_CWMIN | RTW_TCR_MXDMA_2048 |
	      LSHIFT(7, RTW_TCR_SRL_MASK) | LSHIFT(7, RTW_TCR_LRL_MASK);

	RTW_WRITE(regs, RTW_TCR, tcr);

	RTW_WBW(regs, RTW_CR, RTW_TCR);

	return rtw_chip_reset1(regs, dvname);
}

__inline int
rtw_recall_eeprom(struct rtw_regs *regs, const char *dvname)
{
	int i;
	u_int8_t ecr;

	ecr = RTW_READ8(regs, RTW_9346CR);
	ecr = (ecr & ~RTW_9346CR_EEM_MASK) | RTW_9346CR_EEM_AUTOLOAD;
	RTW_WRITE8(regs, RTW_9346CR, ecr);

	RTW_WBR(regs, RTW_9346CR, RTW_9346CR);

	/* wait 2.5ms for completion */
	for (i = 0; i < 25; i++) {
		ecr = RTW_READ8(regs, RTW_9346CR);
		if ((ecr & RTW_9346CR_EEM_MASK) == RTW_9346CR_EEM_NORMAL) {
			RTW_DPRINTF(RTW_DEBUG_RESET,
			    ("%s: recall EEPROM in %dus\n", dvname, i * 100));
			return 0;
		}
		RTW_RBR(regs, RTW_9346CR, RTW_9346CR);
		DELAY(100);
	}
	printf("%s: recall EEPROM failed\n", dvname);
	return ETIMEDOUT;
}

__inline int
rtw_reset(struct rtw_softc *sc)
{
	int rc;
	uint8_t config1;

	if ((rc = rtw_chip_reset(&sc->sc_regs, sc->sc_dev.dv_xname)) != 0)
		return rc;

	if ((rc = rtw_recall_eeprom(&sc->sc_regs, sc->sc_dev.dv_xname)) != 0)
		;

	config1 = RTW_READ8(&sc->sc_regs, RTW_CONFIG1);
	RTW_WRITE8(&sc->sc_regs, RTW_CONFIG1, config1 & ~RTW_CONFIG1_PMEN);
	/* TBD turn off maximum power saving? */

	return 0;
}

__inline int
rtw_txdesc_dmamaps_create(bus_dma_tag_t dmat, struct rtw_txctl *descs,
    u_int ndescs)
{
	int i, rc = 0;
	for (i = 0; i < ndescs; i++) {
		rc = bus_dmamap_create(dmat, MCLBYTES, RTW_MAXPKTSEGS, MCLBYTES,
		    0, 0, &descs[i].stx_dmamap);
		if (rc != 0)
			break;
	}
	return rc;
}

__inline int
rtw_rxdesc_dmamaps_create(bus_dma_tag_t dmat, struct rtw_rxctl *descs,
    u_int ndescs)
{
	int i, rc = 0;
	for (i = 0; i < ndescs; i++) {
		rc = bus_dmamap_create(dmat, MCLBYTES, 1, MCLBYTES, 0, 0,
		    &descs[i].srx_dmamap);
		if (rc != 0)
			break;
	}
	return rc;
}

__inline void
rtw_rxctls_setup(struct rtw_rxctl *descs)
{
	int i;
	for (i = 0; i < RTW_RXQLEN; i++)
		descs[i].srx_mbuf = NULL;
}

__inline void
rtw_rxdesc_dmamaps_destroy(bus_dma_tag_t dmat, struct rtw_rxctl *descs,
    u_int ndescs)
{
	int i;
	for (i = 0; i < ndescs; i++) {
		if (descs[i].srx_dmamap != NULL)
			bus_dmamap_destroy(dmat, descs[i].srx_dmamap);
	}
}

__inline void
rtw_txdesc_dmamaps_destroy(bus_dma_tag_t dmat, struct rtw_txctl *descs,
    u_int ndescs)
{
	int i;
	for (i = 0; i < ndescs; i++) {
		if (descs[i].stx_dmamap != NULL)
			bus_dmamap_destroy(dmat, descs[i].stx_dmamap);
	}
}

__inline void
rtw_srom_free(struct rtw_srom *sr)
{
	sr->sr_size = 0;
	if (sr->sr_content == NULL)
		return;
	free(sr->sr_content, M_DEVBUF);
	sr->sr_content = NULL;
}

void
rtw_srom_defaults(struct rtw_srom *sr, u_int32_t *flags, u_int8_t *cs_threshold,
    enum rtw_rfchipid *rfchipid, u_int32_t *rcr)
{
	*flags |= (RTW_F_DIGPHY|RTW_F_ANTDIV);
	*cs_threshold = RTW_SR_ENERGYDETTHR_DEFAULT;
	*rcr |= RTW_RCR_ENCS1;
	*rfchipid = RTW_RFCHIPID_PHILIPS;
}

int
rtw_srom_parse(struct rtw_srom *sr, u_int32_t *flags, u_int8_t *cs_threshold,
    enum rtw_rfchipid *rfchipid, u_int32_t *rcr, enum rtw_locale *locale,
    const char *dvname)
{
	int i;
	const char *rfname, *paname;
	char scratch[sizeof("unknown 0xXX")];
	u_int16_t version;
	u_int8_t mac[IEEE80211_ADDR_LEN];

	*flags &= ~(RTW_F_DIGPHY|RTW_F_DFLANTB|RTW_F_ANTDIV);
	*rcr &= ~(RTW_RCR_ENCS1 | RTW_RCR_ENCS2);

	version = RTW_SR_GET16(sr, RTW_SR_VERSION);
	printf("SROM %d.%d ", version >> 8, version & 0xff);

	if (version <= 0x0101) {
		printf(" is not understood, limping along with defaults ");
		rtw_srom_defaults(sr, flags, cs_threshold, rfchipid, rcr);
		return 0;
	}

	for (i = 0; i < IEEE80211_ADDR_LEN; i++)
		mac[i] = RTW_SR_GET(sr, RTW_SR_MAC + i);

	RTW_DPRINTF(RTW_DEBUG_ATTACH,
	    ("%s: EEPROM MAC %s\n", dvname, ether_sprintf(mac)));

	*cs_threshold = RTW_SR_GET(sr, RTW_SR_ENERGYDETTHR);

	if ((RTW_SR_GET(sr, RTW_SR_CONFIG2) & RTW_CONFIG2_ANT) != 0)
		*flags |= RTW_F_ANTDIV;

	/* Note well: the sense of the RTW_SR_RFPARM_DIGPHY bit seems
	 * to be reversed.
	 */
	if ((RTW_SR_GET(sr, RTW_SR_RFPARM) & RTW_SR_RFPARM_DIGPHY) == 0)
		*flags |= RTW_F_DIGPHY;
	if ((RTW_SR_GET(sr, RTW_SR_RFPARM) & RTW_SR_RFPARM_DFLANTB) != 0)
		*flags |= RTW_F_DFLANTB;

	*rcr |= LSHIFT(MASK_AND_RSHIFT(RTW_SR_GET(sr, RTW_SR_RFPARM),
	    RTW_SR_RFPARM_CS_MASK), RTW_RCR_ENCS1);

	*rfchipid = RTW_SR_GET(sr, RTW_SR_RFCHIPID);
	switch (*rfchipid) {
	case RTW_RFCHIPID_GCT:		/* this combo seen in the wild */
		rfname = "GCT GRF5101";
		paname = "Winspring WS9901";
		break;
	case RTW_RFCHIPID_MAXIM:
		rfname = "MAX2820";	/* guess */
		paname = "MAX2422";	/* guess */
		break;
	case RTW_RFCHIPID_INTERSIL:
		rfname = "Intersil HFA3873";	/* guess */
		paname = "Intersil <unknown>";
		break;
	case RTW_RFCHIPID_PHILIPS:	/* this combo seen in the wild */
		rfname = "SA2400A";
		paname = "SA2411";
		break;
	case RTW_RFCHIPID_RFMD:
		/* this is the same front-end as an atw(4)! */
		rfname = "RFMD RF2948B, "	/* mentioned in Realtek docs */
			 "LNA: RFMD RF2494, "	/* mentioned in Realtek docs */
			 "SYN: Silicon Labs Si4126";	/* inferred from
			 				 * reference driver
							 */
		paname = "RFMD RF2189";		/* mentioned in Realtek docs */
		break;
	case RTW_RFCHIPID_RESERVED:
		rfname = paname = "reserved";
		break;
	default:
		snprintf(scratch, sizeof(scratch), "unknown 0x%02x", *rfchipid);
		rfname = paname = scratch;
	}
	printf("RF %s PA %s ", rfname, paname);

	switch (RTW_SR_GET(sr, RTW_SR_CONFIG0) & RTW_CONFIG0_GL_MASK) {
	case RTW_CONFIG0_GL_USA:
		*locale = RTW_LOCALE_USA;
		break;
	case RTW_CONFIG0_GL_EUROPE:
		*locale = RTW_LOCALE_EUROPE;
		break;
	case RTW_CONFIG0_GL_JAPAN:
		*locale = RTW_LOCALE_JAPAN;
		break;
	default:
		*locale = RTW_LOCALE_UNKNOWN;
		break;
	}
	return 0;
}

/* Returns -1 on failure. */
int
rtw_srom_read(struct rtw_regs *regs, u_int32_t flags, struct rtw_srom *sr,
    const char *dvname)
{
	int rc;
	struct seeprom_descriptor sd;
	u_int8_t ecr;

	(void)memset(&sd, 0, sizeof(sd));

	ecr = RTW_READ8(regs, RTW_9346CR);

	if ((flags & RTW_F_9356SROM) != 0) {
		RTW_DPRINTF(RTW_DEBUG_ATTACH, ("%s: 93c56 SROM\n", dvname));
		sr->sr_size = 256;
		sd.sd_chip = C56_66;
	} else {
		RTW_DPRINTF(RTW_DEBUG_ATTACH, ("%s: 93c46 SROM\n", dvname));
		sr->sr_size = 128;
		sd.sd_chip = C46;
	}

	ecr &= ~(RTW_9346CR_EEDI | RTW_9346CR_EEDO | RTW_9346CR_EESK |
	    RTW_9346CR_EEM_MASK | RTW_9346CR_EECS);
	ecr |= RTW_9346CR_EEM_PROGRAM;

	RTW_WRITE8(regs, RTW_9346CR, ecr);

	sr->sr_content = malloc(sr->sr_size, M_DEVBUF, M_NOWAIT);

	if (sr->sr_content == NULL) {
		printf("%s: unable to allocate SROM buffer\n", dvname);
		return ENOMEM;
	}

	(void)memset(sr->sr_content, 0, sr->sr_size);

	/* RTL8180 has a single 8-bit register for controlling the
	 * 93cx6 SROM.  There is no "ready" bit. The RTL8180
	 * input/output sense is the reverse of read_seeprom's.
	 */
	sd.sd_tag = regs->r_bt;
	sd.sd_bsh = regs->r_bh;
	sd.sd_regsize = 1;
	sd.sd_control_offset = RTW_9346CR;
	sd.sd_status_offset = RTW_9346CR;
	sd.sd_dataout_offset = RTW_9346CR;
	sd.sd_CK = RTW_9346CR_EESK;
	sd.sd_CS = RTW_9346CR_EECS;
	sd.sd_DI = RTW_9346CR_EEDO;
	sd.sd_DO = RTW_9346CR_EEDI;
	/* make read_seeprom enter EEPROM read/write mode */ 
	sd.sd_MS = ecr;
	sd.sd_RDY = 0;
#if 0
	sd.sd_clkdelay = 50;
#endif

	/* TBD bus barriers */
	if (!read_seeprom(&sd, sr->sr_content, 0, sr->sr_size/2)) {
		printf("%s: could not read SROM\n", dvname);
		free(sr->sr_content, M_DEVBUF);
		sr->sr_content = NULL;
		return -1;	/* XXX */
	}

	/* end EEPROM read/write mode */ 
	RTW_WRITE8(regs, RTW_9346CR,
	    (ecr & ~RTW_9346CR_EEM_MASK) | RTW_9346CR_EEM_NORMAL);
	RTW_WBRW(regs, RTW_9346CR, RTW_9346CR);

	if ((rc = rtw_recall_eeprom(regs, dvname)) != 0)
		return rc;

#ifdef RTW_DEBUG
	{
		int i;
		RTW_DPRINTF(RTW_DEBUG_ATTACH,
		    ("\n%s: serial ROM:\n\t", dvname));
		for (i = 0; i < sr->sr_size/2; i++) {
			if (((i % 8) == 0) && (i != 0))
				RTW_DPRINTF(RTW_DEBUG_ATTACH, ("\n\t"));
			RTW_DPRINTF(RTW_DEBUG_ATTACH,
			    (" %04x", sr->sr_content[i]));
		}
		RTW_DPRINTF(RTW_DEBUG_ATTACH, ("\n"));
	}
#endif /* RTW_DEBUG */
	return 0;
}

void
rtw_set_rfprog(struct rtw_regs *regs, enum rtw_rfchipid rfchipid,
    const char *dvname)
{
	u_int8_t cfg4;
	const char *method;

	cfg4 = RTW_READ8(regs, RTW_CONFIG4) & ~RTW_CONFIG4_RFTYPE_MASK;

	switch (rfchipid) {
	default:
		cfg4 |= LSHIFT(rtw_rfprog_fallback, RTW_CONFIG4_RFTYPE_MASK);
		method = "fallback";
		break;
	case RTW_RFCHIPID_INTERSIL:
		cfg4 |= RTW_CONFIG4_RFTYPE_INTERSIL;
		method = "Intersil";
		break;
	case RTW_RFCHIPID_PHILIPS:
		cfg4 |= RTW_CONFIG4_RFTYPE_PHILIPS;
		method = "Philips";
		break;
	case RTW_RFCHIPID_RFMD:
		cfg4 |= RTW_CONFIG4_RFTYPE_RFMD;
		method = "RFMD";
		break;
	}

	RTW_WRITE8(regs, RTW_CONFIG4, cfg4);

	RTW_WBR(regs, RTW_CONFIG4, RTW_CONFIG4);

	RTW_DPRINTF(RTW_DEBUG_INIT,
	    ("%s: %s RF programming method, %#02x\n", dvname, method,
	    RTW_READ8(regs, RTW_CONFIG4)));
}

#if 0
__inline int
rtw_identify_rf(struct rtw_regs *regs, enum rtw_rftype *rftype,
    const char *dvname)
{
	u_int8_t cfg4;
	const char *name;

	cfg4 = RTW_READ8(regs, RTW_CONFIG4);

	switch (cfg4 & RTW_CONFIG4_RFTYPE_MASK) {
	case RTW_CONFIG4_RFTYPE_PHILIPS:
		*rftype = RTW_RFTYPE_PHILIPS;
		name = "Philips";
		break;
	case RTW_CONFIG4_RFTYPE_INTERSIL:
		*rftype = RTW_RFTYPE_INTERSIL;
		name = "Intersil";
		break;
	case RTW_CONFIG4_RFTYPE_RFMD:
		*rftype = RTW_RFTYPE_RFMD;
		name = "RFMD";
		break;
	default:
		name = "<unknown>";
		return ENXIO;
	}

	printf("%s: RF prog type %s\n", dvname, name);
	return 0;
}
#endif

__inline void
rtw_init_channels(enum rtw_locale locale,
    struct ieee80211_channel (*chans)[IEEE80211_CHAN_MAX+1],
    const char *dvname)
{
	int i;
	const char *name = NULL;
#define ADD_CHANNEL(_chans, _chan) do {			\
	(*_chans)[_chan].ic_flags = IEEE80211_CHAN_B;		\
	(*_chans)[_chan].ic_freq =				\
	    ieee80211_ieee2mhz(_chan, (*_chans)[_chan].ic_flags);\
} while (0)

	switch (locale) {
	case RTW_LOCALE_USA:	/* 1-11 */
		name = "USA";
		for (i = 1; i <= 11; i++)
			ADD_CHANNEL(chans, i);
		break;
	case RTW_LOCALE_JAPAN:	/* 1-14 */
		name = "Japan";
		ADD_CHANNEL(chans, 14);
		for (i = 1; i <= 14; i++)
			ADD_CHANNEL(chans, i);
		break;
	case RTW_LOCALE_EUROPE:	/* 1-13 */
		name = "Europe";
		for (i = 1; i <= 13; i++)
			ADD_CHANNEL(chans, i);
		break;
	default:			/* 10-11 allowed by most countries */
		name = "<unknown>";
		for (i = 10; i <= 11; i++)
			ADD_CHANNEL(chans, i);
		break;
	}
	RTW_DPRINTF(RTW_DEBUG_ATTACH, ("%s: Geographic Location %s\n",
	    dvname, name));
#undef ADD_CHANNEL
}

__inline void
rtw_identify_country(struct rtw_regs *regs, enum rtw_locale *locale,
    const char *dvname)
{
	u_int8_t cfg0 = RTW_READ8(regs, RTW_CONFIG0);

	switch (cfg0 & RTW_CONFIG0_GL_MASK) {
	case RTW_CONFIG0_GL_USA:
		*locale = RTW_LOCALE_USA;
		break;
	case RTW_CONFIG0_GL_JAPAN:
		*locale = RTW_LOCALE_JAPAN;
		break;
	case RTW_CONFIG0_GL_EUROPE:
		*locale = RTW_LOCALE_EUROPE;
		break;
	default:
		*locale = RTW_LOCALE_UNKNOWN;
		break;
	}
}

__inline int
rtw_identify_sta(struct rtw_regs *regs, u_int8_t (*addr)[IEEE80211_ADDR_LEN],
    const char *dvname)
{
	static const u_int8_t empty_macaddr[IEEE80211_ADDR_LEN] = {
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	u_int32_t idr0 = RTW_READ(regs, RTW_IDR0),
	          idr1 = RTW_READ(regs, RTW_IDR1);

	(*addr)[0] = MASK_AND_RSHIFT(idr0, BITS(0,  7));
	(*addr)[1] = MASK_AND_RSHIFT(idr0, BITS(8,  15));
	(*addr)[2] = MASK_AND_RSHIFT(idr0, BITS(16, 23));
	(*addr)[3] = MASK_AND_RSHIFT(idr0, BITS(24 ,31));

	(*addr)[4] = MASK_AND_RSHIFT(idr1, BITS(0,  7));
	(*addr)[5] = MASK_AND_RSHIFT(idr1, BITS(8, 15));

	if (IEEE80211_ADDR_EQ(addr, empty_macaddr)) {
		printf("%s: could not get mac address, attach failed\n",
		    dvname);
		return ENXIO;
	}

	printf("address %s\n", ether_sprintf(*addr));

	return 0;
}

u_int8_t
rtw_chan2txpower(struct rtw_srom *sr, struct ieee80211com *ic,
    struct ieee80211_channel *chan)
{
	u_int idx = RTW_SR_TXPOWER1 + ieee80211_chan2ieee(ic, chan) - 1;
	KASSERT2(idx >= RTW_SR_TXPOWER1 && idx <= RTW_SR_TXPOWER14,
	    ("%s: channel %d out of range", __func__,
	     idx - RTW_SR_TXPOWER1 + 1));
	return RTW_SR_GET(sr, idx);
}

void
rtw_txdesc_blk_init_all(struct rtw_txdesc_blk *htcs)
{
	int pri;
	u_int ndesc[RTW_NTXPRI] =
	    {RTW_NTXDESCLO, RTW_NTXDESCMD, RTW_NTXDESCHI, RTW_NTXDESCBCN};

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		htcs[pri].htc_nfree = ndesc[pri];
		htcs[pri].htc_next = 0;
	}
}

int
rtw_txctl_blk_init(struct rtw_txctl_blk *stc)
{
	int i;
	struct rtw_txctl *stx;

	SIMPLEQ_INIT(&stc->stc_dirtyq);
	SIMPLEQ_INIT(&stc->stc_freeq);
	for (i = 0; i < stc->stc_ndesc; i++) {
		stx = &stc->stc_desc[i];
		stx->stx_mbuf = NULL;
		SIMPLEQ_INSERT_TAIL(&stc->stc_freeq, stx, stx_q);
	}
	return 0;
}

void
rtw_txctl_blk_init_all(struct rtw_txctl_blk *stcs)
{
	int pri;
	for (pri = 0; pri < RTW_NTXPRI; pri++)
		rtw_txctl_blk_init(&stcs[pri]);
}

__inline void
rtw_rxdescs_sync(bus_dma_tag_t dmat, bus_dmamap_t dmap, u_int desc0, u_int
    nsync, int ops)
{
	KASSERT(nsync <= RTW_RXQLEN);
	/* sync to end of ring */
	if (desc0 + nsync > RTW_RXQLEN) {
		bus_dmamap_sync(dmat, dmap,
		    offsetof(struct rtw_descs, hd_rx[desc0]),
		    sizeof(struct rtw_rxdesc) * (RTW_RXQLEN - desc0), ops);
		nsync -= (RTW_RXQLEN - desc0);
		desc0 = 0;
	}

	KASSERT(desc0 < RTW_RXQLEN);
	KASSERT(nsync <= RTW_RXQLEN);
	KASSERT(desc0 + nsync <= RTW_RXQLEN);

	/* sync what remains */
	bus_dmamap_sync(dmat, dmap,
	    offsetof(struct rtw_descs, hd_rx[desc0]),
	    sizeof(struct rtw_rxdesc) * nsync, ops);
}

void
rtw_txdescs_sync(bus_dma_tag_t dmat, bus_dmamap_t dmap,
    struct rtw_txdesc_blk *htc, u_int desc0, u_int nsync, int ops)
{
	/* sync to end of ring */
	if (desc0 + nsync > htc->htc_ndesc) {
		bus_dmamap_sync(dmat, dmap,
		    htc->htc_ofs + sizeof(struct rtw_txdesc) * desc0,
		    sizeof(struct rtw_txdesc) * (htc->htc_ndesc - desc0),
		    ops);
		nsync -= (htc->htc_ndesc - desc0);
		desc0 = 0;
	}

	/* sync what remains */
	bus_dmamap_sync(dmat, dmap,
	    htc->htc_ofs + sizeof(struct rtw_txdesc) * desc0,
	    sizeof(struct rtw_txdesc) * nsync, ops);
}

void
rtw_txdescs_sync_all(bus_dma_tag_t dmat, bus_dmamap_t dmap,
    struct rtw_txdesc_blk *htcs)
{
	int pri;
	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		rtw_txdescs_sync(dmat, dmap,
		    &htcs[pri], 0, htcs[pri].htc_ndesc,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
	}
}

void
rtw_rxbufs_release(bus_dma_tag_t dmat, struct rtw_rxctl *desc)
{
	int i;
	struct rtw_rxctl *srx;

	for (i = 0; i < RTW_RXQLEN; i++) {
		srx = &desc[i];
		bus_dmamap_sync(dmat, srx->srx_dmamap, 0,
		    srx->srx_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);
		bus_dmamap_unload(dmat, srx->srx_dmamap);
		m_freem(srx->srx_mbuf);
		srx->srx_mbuf = NULL;
	}
}

__inline int
rtw_rxbuf_alloc(bus_dma_tag_t dmat, struct rtw_rxctl *srx)
{
	int rc;
	struct mbuf *m;

	MGETHDR(m, M_DONTWAIT, MT_DATA); 
	if (m == NULL)
		return ENOBUFS;

	MCLGET(m, M_DONTWAIT); 
	if (m == NULL)
		return ENOBUFS;

	m->m_pkthdr.len = m->m_len = m->m_ext.ext_size;

	if (srx->srx_mbuf != NULL)
		bus_dmamap_unload(dmat, srx->srx_dmamap);

	srx->srx_mbuf = NULL;

	rc = bus_dmamap_load_mbuf(dmat, srx->srx_dmamap, m, BUS_DMA_NOWAIT);
	if (rc != 0) {
		m_freem(m);
		return -1;
	}

	srx->srx_mbuf = m;

	return 0;
}

int
rtw_rxctl_init_all(bus_dma_tag_t dmat, struct rtw_rxctl *desc,
    u_int *next, const char *dvname)
{
	int i, rc;
	struct rtw_rxctl *srx;

	for (i = 0; i < RTW_RXQLEN; i++) {
		srx = &desc[i];
		if ((rc = rtw_rxbuf_alloc(dmat, srx)) == 0)
			continue;
		printf("%s: failed rtw_rxbuf_alloc after %d buffers, rc = %d\n",
		    dvname, i, rc);
		if (i == 0) {
			rtw_rxbufs_release(dmat, desc);
			return rc;
		}
	}
	*next = 0;
	return 0;
}

__inline void
rtw_rxdesc_init(bus_dma_tag_t dmat, bus_dmamap_t dmam,
    struct rtw_rxdesc *hrx, struct rtw_rxctl *srx, int idx, int kick)
{
	int is_last = (idx == RTW_RXQLEN - 1);
	u_int32_t ctl, octl, obuf;

	obuf = hrx->hrx_buf;
	hrx->hrx_buf = htole32(srx->srx_dmamap->dm_segs[0].ds_addr);

	ctl = LSHIFT(srx->srx_mbuf->m_len, RTW_RXCTL_LENGTH_MASK) |
	    RTW_RXCTL_OWN | RTW_RXCTL_FS | RTW_RXCTL_LS;

	if (is_last)
		ctl |= RTW_RXCTL_EOR;

	octl = hrx->hrx_ctl;
	hrx->hrx_ctl = htole32(ctl);

	RTW_DPRINTF(
	    kick ? (RTW_DEBUG_RECV_DESC | RTW_DEBUG_IO_KICK)
	         : RTW_DEBUG_RECV_DESC,
	    ("%s: hrx %p buf %08x -> %08x ctl %08x -> %08x\n", __func__, hrx,
	     letoh32(obuf), letoh32(hrx->hrx_buf), letoh32(octl),
	     letoh32(hrx->hrx_ctl)));

	/* sync the mbuf */
	bus_dmamap_sync(dmat, srx->srx_dmamap, 0, srx->srx_dmamap->dm_mapsize,
	    BUS_DMASYNC_PREREAD);

	/* sync the descriptor */
	bus_dmamap_sync(dmat, dmam, RTW_DESC_OFFSET(hd_rx, idx),
	    sizeof(struct rtw_rxdesc),
	    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
}

void
rtw_rxdesc_init_all(bus_dma_tag_t dmat, bus_dmamap_t dmam,
    struct rtw_rxdesc *desc, struct rtw_rxctl *ctl, int kick)
{
	int i;
	struct rtw_rxdesc *hrx;
	struct rtw_rxctl *srx;

	for (i = 0; i < RTW_RXQLEN; i++) {
		hrx = &desc[i];
		srx = &ctl[i];
		rtw_rxdesc_init(dmat, dmam, hrx, srx, i, kick);
	}
}

void
rtw_io_enable(struct rtw_regs *regs, u_int8_t flags, int enable)
{
	u_int8_t cr;

	RTW_DPRINTF(RTW_DEBUG_IOSTATE, ("%s: %s 0x%02x\n", __func__,
	    enable ? "enable" : "disable", flags));

	cr = RTW_READ8(regs, RTW_CR);

	/* XXX reference source does not enable MULRW */
#if 0
	/* enable PCI Read/Write Multiple */
	cr |= RTW_CR_MULRW;
#endif

	RTW_RBW(regs, RTW_CR, RTW_CR);	/* XXX paranoia? */
	if (enable)
		cr |= flags;
	else
		cr &= ~flags;
	RTW_WRITE8(regs, RTW_CR, cr);
	RTW_SYNC(regs, RTW_CR, RTW_CR);
}

void
rtw_intr_rx(struct rtw_softc *sc, u_int16_t isr)
{
	static const int ratetbl[4] = {2, 4, 11, 22};	/* convert rates:
							 * hardware -> net80211
							 */

	u_int next, nproc = 0;
	int hwrate, len, rate, rssi;
	u_int32_t hrssi, hstat, htsfth, htsftl;
	struct rtw_rxdesc *hrx;
	struct rtw_rxctl *srx;
	struct mbuf *m;

	struct ieee80211com *ic = &sc->sc_ic;
	struct ieee80211_node *ni;
	struct ieee80211_frame *wh;

	KASSERT(sc->sc_rxnext < RTW_RXQLEN);

	for (next = sc->sc_rxnext; ; next = (next + 1) % RTW_RXQLEN) {
		rtw_rxdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap,
		    next, 1, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
		hrx = &sc->sc_rxdesc[next];
		srx = &sc->sc_rxctl[next];

		hstat = letoh32(hrx->hrx_stat);
		hrssi = letoh32(hrx->hrx_rssi);
		htsfth = letoh32(hrx->hrx_tsfth);
		htsftl = letoh32(hrx->hrx_tsftl);

		RTW_DPRINTF(RTW_DEBUG_RECV_DESC,
		    ("%s: rxdesc[%d] hstat %08x hrssi %08x htsft %08x%08x\n",
		    __func__, next, hstat, hrssi, htsfth, htsftl));

		KASSERT((hstat & (RTW_RXSTAT_FS|RTW_RXSTAT_LS)) ==
		    (RTW_RXSTAT_FS|RTW_RXSTAT_LS));

		++nproc;

		/* still belongs to NIC */
		if ((hstat & RTW_RXSTAT_OWN) != 0) {
			if (nproc > 1)
				break;

			/* sometimes the NIC skips to the 0th descriptor */
			rtw_rxdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap,
			    0, 1, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
			hrx = &sc->sc_rxdesc[0];
			if ((hrx->hrx_stat & htole32(RTW_RXSTAT_OWN)) != 0)
				break;
			RTW_DPRINTF(RTW_DEBUG_BUGS,
			    ("%s: NIC skipped to rxdesc[0]\n",
			     sc->sc_dev.dv_xname));
			next = 0;
			continue;
		}

		if ((hstat & RTW_RXSTAT_IOERROR) != 0) {
			printf("%s: DMA error/FIFO overflow %08x, "
			    "rx descriptor %d\n", sc->sc_dev.dv_xname,
			    hstat & RTW_RXSTAT_IOERROR, next);
			sc->sc_if.if_ierrors++;
			goto next;
		}

		len = MASK_AND_RSHIFT(hstat, RTW_RXSTAT_LENGTH_MASK);
		if (len < IEEE80211_MIN_LEN) {
			sc->sc_ic.ic_stats.is_rx_tooshort++;
			goto next;
		}

		hwrate = MASK_AND_RSHIFT(hstat, RTW_RXSTAT_RATE_MASK);
		if (hwrate >= sizeof(ratetbl) / sizeof(ratetbl[0])) {
			printf("%s: unknown rate #%d\n", sc->sc_dev.dv_xname,
			    MASK_AND_RSHIFT(hstat, RTW_RXSTAT_RATE_MASK));
			sc->sc_if.if_ierrors++;
			goto next;
		}
		rate = ratetbl[hwrate];

#ifdef RTW_DEBUG
#define PRINTSTAT(flag) do { \
	if ((hstat & flag) != 0) { \
		printf("%s" #flag, delim); \
		delim = ","; \
	} \
} while (0)
		if ((rtw_debug & RTW_DEBUG_RECV_DESC) != 0) {
			const char *delim = "<";
			printf("%s: ", sc->sc_dev.dv_xname);
			if ((hstat & RTW_RXSTAT_DEBUG) != 0) {
				printf("status %08x", hstat);
				PRINTSTAT(RTW_RXSTAT_SPLCP);
				PRINTSTAT(RTW_RXSTAT_MAR);
				PRINTSTAT(RTW_RXSTAT_PAR);
				PRINTSTAT(RTW_RXSTAT_BAR);
				PRINTSTAT(RTW_RXSTAT_PWRMGT);
				PRINTSTAT(RTW_RXSTAT_CRC32);
				PRINTSTAT(RTW_RXSTAT_ICV);
				printf(">, ");
			}
			printf("rate %d.%d Mb/s, time %08x%08x\n",
			    (rate * 5) / 10, (rate * 5) % 10, htsfth, htsftl);
		}
#endif /* RTW_DEBUG */

		if ((hstat & RTW_RXSTAT_RES) != 0 &&
		    sc->sc_ic.ic_opmode != IEEE80211_M_MONITOR)
			goto next;

		/* if bad flags, skip descriptor */
		if ((hstat & RTW_RXSTAT_ONESEG) != RTW_RXSTAT_ONESEG) {
			printf("%s: too many rx segments\n",
			    sc->sc_dev.dv_xname);
			goto next;
		}

		bus_dmamap_sync(sc->sc_dmat, srx->srx_dmamap, 0,
		    srx->srx_dmamap->dm_mapsize, BUS_DMASYNC_POSTREAD);

		m = srx->srx_mbuf;

		/* if temporarily out of memory, re-use mbuf */
		switch (rtw_rxbuf_alloc(sc->sc_dmat, srx)) {
		case 0:
			break;
		case ENOBUFS:
			printf("%s: rtw_rxbuf_alloc(, %d) failed, "
			    "dropping this packet\n", sc->sc_dev.dv_xname,
			    next);
			goto next;
		default:
			/* XXX shorten rx ring, instead? */
			panic("%s: could not load DMA map\n",
			    sc->sc_dev.dv_xname);
		}

		if (sc->sc_rfchipid == RTW_RFCHIPID_PHILIPS)
			rssi = MASK_AND_RSHIFT(hrssi, RTW_RXRSSI_RSSI);
		else {
			rssi = MASK_AND_RSHIFT(hrssi, RTW_RXRSSI_IMR_RSSI);
			/* TBD find out each front-end's LNA gain in the
			 * front-end's units
			 */
			if ((hrssi & RTW_RXRSSI_IMR_LNA) == 0)
				rssi |= 0x80;
		}

		m->m_pkthdr.rcvif = &sc->sc_if;
		m->m_pkthdr.len = m->m_len = len;
		m->m_flags |= M_HASFCS;

		wh = mtod(m, struct ieee80211_frame *);
		/* TBD use _MAR, _BAR, _PAR flags as hints to _find_rxnode? */
		ni = ieee80211_find_rxnode(&sc->sc_ic, wh);

		sc->sc_tsfth = htsfth;

#ifdef RTW_DEBUG
		if ((sc->sc_if.if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) {
			ieee80211_dump_pkt(mtod(m, uint8_t *), m->m_pkthdr.len,
			    rate, rssi);
		}
#endif /* RTW_DEBUG */

		ieee80211_input(&sc->sc_if, m, ni, rssi, htsftl);
		if (ni == ic->ic_bss)
			ieee80211_unref_node(&ni);
		else
			ieee80211_free_node(&sc->sc_ic, ni);
next:
		rtw_rxdesc_init(sc->sc_dmat, sc->sc_desc_dmamap,
		    hrx, srx, next, 0);
	}
	KASSERT(sc->sc_rxnext < RTW_RXQLEN);

	sc->sc_rxnext = next;

	return;
}

void
rtw_txbuf_release(bus_dma_tag_t dmat, struct ieee80211com *ic,
    struct rtw_txctl *stx)
{
	struct mbuf *m;
	struct ieee80211_node *ni;

	m = stx->stx_mbuf;
	ni = stx->stx_ni;
	KASSERT(m != NULL);
	KASSERT(ni != NULL);
	stx->stx_mbuf = NULL;
	stx->stx_ni = NULL;

	bus_dmamap_sync(dmat, stx->stx_dmamap, 0, stx->stx_dmamap->dm_mapsize,
	    BUS_DMASYNC_POSTWRITE);
	bus_dmamap_unload(dmat, stx->stx_dmamap);
	m_freem(m);
	if (ni == ic->ic_bss)
		ieee80211_unref_node(&ni);
	else
		ieee80211_free_node(ic, ni);
}

void
rtw_txbufs_release(bus_dma_tag_t dmat, bus_dmamap_t desc_dmamap,
    struct ieee80211com *ic, struct rtw_txctl_blk *stc)
{
	struct rtw_txctl *stx;

	while ((stx = SIMPLEQ_FIRST(&stc->stc_dirtyq)) != NULL) {
		rtw_txbuf_release(dmat, ic, stx);
		SIMPLEQ_REMOVE_HEAD(&stc->stc_dirtyq, stx_q);
		SIMPLEQ_INSERT_TAIL(&stc->stc_freeq, stx, stx_q);
	}
}

__inline void
rtw_collect_txpkt(struct rtw_softc *sc, struct rtw_txdesc_blk *htc,
    struct rtw_txctl *stx, int ndesc)
{
	uint32_t hstat;
	int data_retry, rts_retry;
	struct rtw_txdesc *htxn;
	const char *condstring;

	rtw_txbuf_release(sc->sc_dmat, &sc->sc_ic, stx);

	htc->htc_nfree += ndesc;

	htxn = &htc->htc_desc[stx->stx_last];

	hstat = letoh32(htxn->htx_stat);
	rts_retry = MASK_AND_RSHIFT(hstat, RTW_TXSTAT_RTSRETRY_MASK);
	data_retry = MASK_AND_RSHIFT(hstat, RTW_TXSTAT_DRC_MASK);

	sc->sc_if.if_collisions += rts_retry + data_retry;

	if ((hstat & RTW_TXSTAT_TOK) != 0)
		condstring = "ok";
	else {
		sc->sc_if.if_oerrors++;
		condstring = "error";
	}

	DPRINTF(sc, RTW_DEBUG_XMIT_DESC,
	    ("%s: stx %p txdesc[%d, %d] %s tries rts %u data %u\n",
	    sc->sc_dev.dv_xname, stx, stx->stx_first, stx->stx_last,
	    condstring, rts_retry, data_retry));
}

/* Collect transmitted packets. */
__inline void
rtw_collect_txring(struct rtw_softc *sc, struct rtw_txctl_blk *stc,
    struct rtw_txdesc_blk *htc)
{
	int ndesc;
	struct rtw_txctl *stx;

	while ((stx = SIMPLEQ_FIRST(&stc->stc_dirtyq)) != NULL) {
		ndesc = 1 + stx->stx_last - stx->stx_first;
		if (stx->stx_last < stx->stx_first)
			ndesc += htc->htc_ndesc;

		KASSERT(ndesc > 0);

		rtw_txdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap, htc,
		    stx->stx_first, ndesc,
		    BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);

		if ((htc->htc_desc[stx->stx_last].htx_stat &
		    htole32(RTW_TXSTAT_OWN)) != 0) 
			break;

		rtw_collect_txpkt(sc, htc, stx, ndesc);
		SIMPLEQ_REMOVE_HEAD(&stc->stc_dirtyq, stx_q);
		SIMPLEQ_INSERT_TAIL(&stc->stc_freeq, stx, stx_q);
		sc->sc_if.if_flags &= ~IFF_OACTIVE;
	}
	if (stx == NULL)
		stc->stc_tx_timer = 0;
}

void
rtw_intr_tx(struct rtw_softc *sc, u_int16_t isr)
{
	int pri;
	struct rtw_txctl_blk	*stc;
	struct rtw_txdesc_blk	*htc;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		stc = &sc->sc_txctl_blk[pri];
		htc = &sc->sc_txdesc_blk[pri];

		rtw_collect_txring(sc, stc, htc);

		if ((isr & RTW_INTR_TX) != 0)
			rtw_start(&sc->sc_if);
	}

	/* TBD */
	return;
}

void
rtw_intr_beacon(struct rtw_softc *sc, u_int16_t isr)
{
	/* TBD */
	return;
}

void
rtw_intr_atim(struct rtw_softc *sc)
{
	/* TBD */
	return;
}

#ifdef RTW_DEBUG
void
rtw_dump_rings(struct rtw_softc *sc)
{
	struct rtw_txdesc_blk *htc;
	struct rtw_rxdesc *hrx;
	int desc, pri;

	if ((rtw_debug & RTW_DEBUG_IO_KICK) == 0)
		return;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		htc = &sc->sc_txdesc_blk[pri];
		printf("%s: txpri %d ndesc %d nfree %d\n", __func__, pri,
		    htc->htc_ndesc, htc->htc_nfree);
		for (desc = 0; desc < htc->htc_ndesc; desc++)
			rtw_print_txdesc(sc, ".", NULL, htc, desc);
	}

	for (desc = 0; desc < RTW_RXQLEN; desc++) {
		hrx = &sc->sc_rxdesc[desc];
		printf("%s: ctl %08x rsvd0/rssi %08x buf/tsftl %08x "
		    "rsvd1/tsfth %08x\n", __func__,
		    letoh32(hrx->hrx_ctl), letoh32(hrx->hrx_rssi),
		    letoh32(hrx->hrx_buf), letoh32(hrx->hrx_tsfth));
	}
}
#endif /* RTW_DEBUG */

void
rtw_hwring_setup(struct rtw_softc *sc)
{
	struct rtw_regs *regs = &sc->sc_regs;
	RTW_WRITE(regs, RTW_RDSAR, RTW_RING_BASE(sc, hd_rx));
	RTW_WRITE(regs, RTW_TLPDA, RTW_RING_BASE(sc, hd_txlo));
	RTW_WRITE(regs, RTW_TNPDA, RTW_RING_BASE(sc, hd_txmd));
	RTW_WRITE(regs, RTW_THPDA, RTW_RING_BASE(sc, hd_txhi));
	RTW_WRITE(regs, RTW_TBDA, RTW_RING_BASE(sc, hd_bcn));
	RTW_SYNC(regs, RTW_TLPDA, RTW_RDSAR);
#if 0
	RTW_DPRINTF(RTW_DEBUG_XMIT_DESC,
	    ("%s: reg[TLPDA] <- %" PRIxPTR "\n", __func__,
	     (uintptr_t)RTW_RING_BASE(sc, hd_txlo)));
	RTW_DPRINTF(RTW_DEBUG_XMIT_DESC,
	    ("%s: reg[TNPDA] <- %" PRIxPTR "\n", __func__,
	     (uintptr_t)RTW_RING_BASE(sc, hd_txmd)));
	RTW_DPRINTF(RTW_DEBUG_XMIT_DESC,
	    ("%s: reg[THPDA] <- %" PRIxPTR "\n", __func__,
	     (uintptr_t)RTW_RING_BASE(sc, hd_txhi)));
	RTW_DPRINTF(RTW_DEBUG_XMIT_DESC,
	    ("%s: reg[TBDA] <- %" PRIxPTR "\n", __func__,
	     (uintptr_t)RTW_RING_BASE(sc, hd_bcn)));
	RTW_DPRINTF(RTW_DEBUG_RECV_DESC,
	    ("%s: reg[RDSAR] <- %" PRIxPTR "\n", __func__,
	     (uintptr_t)RTW_RING_BASE(sc, hd_rx)));
#endif
}

void
rtw_swring_setup(struct rtw_softc *sc)
{
	rtw_txdesc_blk_init_all(&sc->sc_txdesc_blk[0]);

	rtw_txctl_blk_init_all(&sc->sc_txctl_blk[0]);

	rtw_rxdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap,
	    0, RTW_RXQLEN, BUS_DMASYNC_POSTREAD|BUS_DMASYNC_POSTWRITE);
	rtw_rxctl_init_all(sc->sc_dmat, sc->sc_rxctl, &sc->sc_rxnext,
	    sc->sc_dev.dv_xname);
	rtw_rxdesc_init_all(sc->sc_dmat, sc->sc_desc_dmamap,
	    sc->sc_rxdesc, sc->sc_rxctl, 1);

	rtw_txdescs_sync_all(sc->sc_dmat, sc->sc_desc_dmamap,
	    &sc->sc_txdesc_blk[0]);
#if 0	/* redundant with rtw_rxdesc_init_all */
	rtw_rxdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap,
	    0, RTW_RXQLEN, BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);
#endif
}

void
rtw_txdesc_blk_reset(struct rtw_txdesc_blk *htc)
{
	int i;

	(void)memset(htc->htc_desc, 0,
	    sizeof(htc->htc_desc[0]) * htc->htc_ndesc);
	for (i = 0; i < htc->htc_ndesc; i++)
		htc->htc_desc[i].htx_next = htole32(RTW_NEXT_DESC(htc, i));
	htc->htc_nfree = htc->htc_ndesc;
	htc->htc_next = 0;
}

void
rtw_txdescs_reset(struct rtw_softc *sc)
{
	int pri;
	struct rtw_txdesc_blk *htc;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		htc = &sc->sc_txdesc_blk[pri];
		rtw_txbufs_release(sc->sc_dmat, sc->sc_desc_dmamap, &sc->sc_ic,
		    &sc->sc_txctl_blk[pri]);
		rtw_txdesc_blk_reset(htc);
		rtw_txdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap, htc,
		    0, htc->htc_ndesc,
		    BUS_DMASYNC_PREWRITE|BUS_DMASYNC_PREREAD);
	}
}

void
rtw_rxdescs_reset(struct rtw_softc *sc)
{
	/* Re-initialize descriptors, just in case. */
	rtw_rxdesc_init_all(sc->sc_dmat, sc->sc_desc_dmamap, sc->sc_rxdesc,
	    &sc->sc_rxctl[0], 1);

	/* Reset to start of ring. */
	sc->sc_rxnext = 0;
}

void
rtw_intr_ioerror(struct rtw_softc *sc, u_int16_t isr)
{
	struct rtw_regs *regs = &sc->sc_regs;

	if ((isr & RTW_INTR_TXFOVW) != 0)
		printf("%s: tx fifo overflow\n", sc->sc_dev.dv_xname);

	if ((isr & (RTW_INTR_RDU|RTW_INTR_RXFOVW)) == 0)
		return;

	RTW_DPRINTF(RTW_DEBUG_BUGS, ("%s: restarting xmit/recv\n",
	    sc->sc_dev.dv_xname));

#ifdef RTW_DEBUG
	rtw_dump_rings(sc);
#endif /* RTW_DEBUG */

	rtw_io_enable(regs, RTW_CR_RE | RTW_CR_TE, 0);

	/* Collect rx'd packets.  Refresh rx buffers. */ 
	rtw_intr_rx(sc, 0);
	/* Collect tx'd packets. */ 
	rtw_intr_tx(sc, 0);

	RTW_WRITE16(regs, RTW_IMR, 0);
	RTW_SYNC(regs, RTW_IMR, RTW_IMR);

	rtw_chip_reset1(regs, sc->sc_dev.dv_xname);

	rtw_rxdescs_reset(sc);
	rtw_txdescs_reset(sc);

	rtw_hwring_setup(sc);

#ifdef RTW_DEBUG
	rtw_dump_rings(sc);
#endif /* RTW_DEBUG */

	RTW_WRITE16(regs, RTW_IMR, sc->sc_inten);
	RTW_SYNC(regs, RTW_IMR, RTW_IMR);
	rtw_io_enable(regs, RTW_CR_RE | RTW_CR_TE, 1);
}

__inline void
rtw_suspend_ticks(struct rtw_softc *sc)
{
	RTW_DPRINTF(RTW_DEBUG_TIMEOUT,
	    ("%s: suspending ticks\n", sc->sc_dev.dv_xname));
	sc->sc_do_tick = 0;
}

__inline void
rtw_resume_ticks(struct rtw_softc *sc)
{
	u_int32_t tsftrl0, tsftrl1, next_tick;

	tsftrl0 = RTW_READ(&sc->sc_regs, RTW_TSFTRL);

	tsftrl1 = RTW_READ(&sc->sc_regs, RTW_TSFTRL);
	next_tick = tsftrl1 + 1000000;
	RTW_WRITE(&sc->sc_regs, RTW_TINT, next_tick);

	sc->sc_do_tick = 1;

	RTW_DPRINTF(RTW_DEBUG_TIMEOUT,
	    ("%s: resume ticks delta %#08x now %#08x next %#08x\n",
	    sc->sc_dev.dv_xname, tsftrl1 - tsftrl0, tsftrl1, next_tick));
}

void
rtw_intr_timeout(struct rtw_softc *sc)
{
	RTW_DPRINTF(RTW_DEBUG_TIMEOUT, ("%s: timeout\n", sc->sc_dev.dv_xname));
	if (sc->sc_do_tick)
		rtw_resume_ticks(sc);
	return;
}

int
rtw_intr(void *arg)
{
	int i;
	struct rtw_softc *sc = arg;
	struct rtw_regs *regs = &sc->sc_regs;
	u_int16_t isr;

	/*
	 * If the interface isn't running, the interrupt couldn't
	 * possibly have come from us.
	 */
	if ((sc->sc_flags & RTW_F_ENABLED) == 0 ||
	    (sc->sc_if.if_flags & IFF_RUNNING) == 0 ||
	    (sc->sc_dev.dv_flags & DVF_ACTIVE) == 0) {
		RTW_DPRINTF(RTW_DEBUG_INTR, ("%s: stray interrupt\n",
		     sc->sc_dev.dv_xname));
		return (0);
	}

	for (i = 0; i < 10; i++) {
		isr = RTW_READ16(regs, RTW_ISR);

		RTW_WRITE16(regs, RTW_ISR, isr);
		RTW_WBR(regs, RTW_ISR, RTW_ISR);

		if (sc->sc_intr_ack != NULL)
			(*sc->sc_intr_ack)(regs);

		if (isr == 0)
			break;

#ifdef RTW_DEBUG
#define PRINTINTR(flag) do { \
	if ((isr & flag) != 0) { \
		printf("%s" #flag, delim); \
		delim = ","; \
	} \
} while (0)

		if ((rtw_debug & RTW_DEBUG_INTR) != 0 && isr != 0) {
			const char *delim = "<";

			printf("%s: reg[ISR] = %x", sc->sc_dev.dv_xname, isr);

			PRINTINTR(RTW_INTR_TXFOVW);
			PRINTINTR(RTW_INTR_TIMEOUT);
			PRINTINTR(RTW_INTR_BCNINT);
			PRINTINTR(RTW_INTR_ATIMINT);
			PRINTINTR(RTW_INTR_TBDER);
			PRINTINTR(RTW_INTR_TBDOK);
			PRINTINTR(RTW_INTR_THPDER);
			PRINTINTR(RTW_INTR_THPDOK);
			PRINTINTR(RTW_INTR_TNPDER);
			PRINTINTR(RTW_INTR_TNPDOK);
			PRINTINTR(RTW_INTR_RXFOVW);
			PRINTINTR(RTW_INTR_RDU);
			PRINTINTR(RTW_INTR_TLPDER);
			PRINTINTR(RTW_INTR_TLPDOK);
			PRINTINTR(RTW_INTR_RER);
			PRINTINTR(RTW_INTR_ROK);

			printf(">\n");
		}
#undef PRINTINTR
#endif /* RTW_DEBUG */

		if ((isr & RTW_INTR_RX) != 0)
			rtw_intr_rx(sc, isr & RTW_INTR_RX);
		if ((isr & RTW_INTR_TX) != 0)
			rtw_intr_tx(sc, isr & RTW_INTR_TX);
		if ((isr & RTW_INTR_BEACON) != 0)
			rtw_intr_beacon(sc, isr & RTW_INTR_BEACON);
		if ((isr & RTW_INTR_ATIMINT) != 0)
			rtw_intr_atim(sc);
		if ((isr & RTW_INTR_IOERROR) != 0)
			rtw_intr_ioerror(sc, isr & RTW_INTR_IOERROR);
		if ((isr & RTW_INTR_TIMEOUT) != 0)
			rtw_intr_timeout(sc);
	}

	return 1;
}

/* Must be called at splnet. */
void
rtw_stop(struct ifnet *ifp, int disable)
{
	int pri;
	struct rtw_softc *sc = (struct rtw_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct rtw_regs *regs = &sc->sc_regs;

	if ((sc->sc_flags & RTW_F_ENABLED) == 0)
		return;

	rtw_suspend_ticks(sc);

	ieee80211_new_state(ic, IEEE80211_S_INIT, -1);

	if ((sc->sc_flags & RTW_F_INVALID) == 0) {
		/* Disable interrupts. */
		RTW_WRITE16(regs, RTW_IMR, 0);

		RTW_WBW(regs, RTW_TPPOLL, RTW_IMR);

		/* Stop the transmit and receive processes. First stop DMA,
		 * then disable receiver and transmitter.
		 */
		RTW_WRITE8(regs, RTW_TPPOLL,
		    RTW_TPPOLL_SBQ|RTW_TPPOLL_SHPQ|RTW_TPPOLL_SNPQ|
		    RTW_TPPOLL_SLPQ);

		RTW_SYNC(regs, RTW_TPPOLL, RTW_IMR);

		rtw_io_enable(&sc->sc_regs, RTW_CR_RE|RTW_CR_TE, 0);
	}

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		rtw_txbufs_release(sc->sc_dmat, sc->sc_desc_dmamap, &sc->sc_ic,
		    &sc->sc_txctl_blk[pri]);
	}

	if (disable) {
		rtw_disable(sc);
		rtw_rxbufs_release(sc->sc_dmat, &sc->sc_rxctl[0]);
	}

	/* Mark the interface as not running.  Cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	return;
}

const char *
rtw_pwrstate_string(enum rtw_pwrstate power)
{
	switch (power) {
	case RTW_ON:
		return "on";
	case RTW_SLEEP:
		return "sleep";
	case RTW_OFF:
		return "off";
	default:
		return "unknown";
	}
}

/* XXX For Maxim, I am using the RFMD settings gleaned from the
 * reference driver, plus a magic Maxim "ON" value that comes from
 * the Realtek document "Windows PG for Rtl8180."
 */
void
rtw_maxim_pwrstate(struct rtw_regs *regs, enum rtw_pwrstate power,
    int before_rf, int digphy)
{
	u_int32_t anaparm;

	anaparm = RTW_READ(regs, RTW_ANAPARM);
	anaparm &= ~(RTW_ANAPARM_RFPOW_MASK | RTW_ANAPARM_TXDACOFF);

	switch (power) {
	case RTW_OFF:
		if (before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_MAXIM_OFF;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_SLEEP:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_MAXIM_SLEEP;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_ON:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_MAXIM_ON;
		break;
	}
	RTW_DPRINTF(RTW_DEBUG_PWR,
	    ("%s: power state %s, %s RF, reg[ANAPARM] <- %08x\n",
	    __func__, rtw_pwrstate_string(power),
	    (before_rf) ? "before" : "after", anaparm));

	RTW_WRITE(regs, RTW_ANAPARM, anaparm);
	RTW_SYNC(regs, RTW_ANAPARM, RTW_ANAPARM);
}

/* XXX I am using the RFMD settings gleaned from the reference
 * driver.  They agree 
 */
void
rtw_rfmd_pwrstate(struct rtw_regs *regs, enum rtw_pwrstate power,
    int before_rf, int digphy)
{
	u_int32_t anaparm;

	anaparm = RTW_READ(regs, RTW_ANAPARM);
	anaparm &= ~(RTW_ANAPARM_RFPOW_MASK | RTW_ANAPARM_TXDACOFF);

	switch (power) {
	case RTW_OFF:
		if (before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_RFMD_OFF;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_SLEEP:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_RFMD_SLEEP;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_ON:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_RFMD_ON;
		break;
	}
	RTW_DPRINTF(RTW_DEBUG_PWR,
	    ("%s: power state %s, %s RF, reg[ANAPARM] <- %08x\n",
	    __func__, rtw_pwrstate_string(power),
	    (before_rf) ? "before" : "after", anaparm));

	RTW_WRITE(regs, RTW_ANAPARM, anaparm);
	RTW_SYNC(regs, RTW_ANAPARM, RTW_ANAPARM);
}

void
rtw_philips_pwrstate(struct rtw_regs *regs, enum rtw_pwrstate power,
    int before_rf, int digphy)
{
	u_int32_t anaparm;

	anaparm = RTW_READ(regs, RTW_ANAPARM);
	anaparm &= ~(RTW_ANAPARM_RFPOW_MASK | RTW_ANAPARM_TXDACOFF);

	switch (power) {
	case RTW_OFF:
		if (before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_PHILIPS_OFF;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_SLEEP:
		if (!before_rf)
			return;
		anaparm |= RTW_ANAPARM_RFPOW_PHILIPS_SLEEP;
		anaparm |= RTW_ANAPARM_TXDACOFF;
		break;
	case RTW_ON:
		if (!before_rf)
			return;
		if (digphy) {
			anaparm |= RTW_ANAPARM_RFPOW_DIG_PHILIPS_ON;
			/* XXX guess */
			anaparm |= RTW_ANAPARM_TXDACOFF;
		} else
			anaparm |= RTW_ANAPARM_RFPOW_ANA_PHILIPS_ON;
		break;
	}
	RTW_DPRINTF(RTW_DEBUG_PWR,
	    ("%s: power state %s, %s RF, reg[ANAPARM] <- %08x\n",
	    __func__, rtw_pwrstate_string(power),
	    (before_rf) ? "before" : "after", anaparm));

	RTW_WRITE(regs, RTW_ANAPARM, anaparm);
	RTW_SYNC(regs, RTW_ANAPARM, RTW_ANAPARM);
}

void
rtw_pwrstate0(struct rtw_softc *sc, enum rtw_pwrstate power, int before_rf,
    int digphy)
{
	struct rtw_regs *regs = &sc->sc_regs;

	rtw_set_access(sc, RTW_ACCESS_ANAPARM);

	(*sc->sc_pwrstate_cb)(regs, power, before_rf, digphy);

	rtw_set_access(sc, RTW_ACCESS_NONE);

	return;
}

int
rtw_pwrstate(struct rtw_softc *sc, enum rtw_pwrstate power)
{
	int rc;

	RTW_DPRINTF(RTW_DEBUG_PWR,
	    ("%s: %s->%s\n", __func__,
	    rtw_pwrstate_string(sc->sc_pwrstate), rtw_pwrstate_string(power)));

	if (sc->sc_pwrstate == power)
		return 0;

	rtw_pwrstate0(sc, power, 1, sc->sc_flags & RTW_F_DIGPHY);
	rc = rtw_rf_pwrstate(sc->sc_rf, power);
	rtw_pwrstate0(sc, power, 0, sc->sc_flags & RTW_F_DIGPHY);

	switch (power) {
	case RTW_ON:
		/* TBD set LEDs */
		break;
	case RTW_SLEEP:
		/* TBD */
		break;
	case RTW_OFF:
		/* TBD */
		break;
	}
	if (rc == 0)
		sc->sc_pwrstate = power;
	else
		sc->sc_pwrstate = RTW_OFF;
	return rc;
}

int
rtw_tune(struct rtw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	u_int chan;
	int rc;
	int antdiv = sc->sc_flags & RTW_F_ANTDIV,
	    dflantb = sc->sc_flags & RTW_F_DFLANTB;

	KASSERT(ic->ic_bss->ni_chan != NULL);

	chan = ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan);
	if (chan == IEEE80211_CHAN_ANY)
		panic("%s: chan == IEEE80211_CHAN_ANY\n", __func__);

	if (chan == sc->sc_cur_chan) {
		RTW_DPRINTF(RTW_DEBUG_TUNE,
		    ("%s: already tuned chan #%d\n", __func__, chan));
		return 0;
	}

	rtw_suspend_ticks(sc);

	rtw_io_enable(&sc->sc_regs, RTW_CR_RE | RTW_CR_TE, 0);

	/* TBD wait for Tx to complete */

	KASSERT((sc->sc_flags & RTW_F_ENABLED) != 0);

	if ((rc = rtw_phy_init(&sc->sc_regs, sc->sc_rf,
	    rtw_chan2txpower(&sc->sc_srom, ic, ic->ic_bss->ni_chan),
	    sc->sc_csthr, ic->ic_bss->ni_chan->ic_freq, antdiv,
	    dflantb, RTW_ON)) != 0) {
		/* XXX condition on powersaving */
		printf("%s: phy init failed\n", sc->sc_dev.dv_xname);
	}

	sc->sc_cur_chan = chan;

	rtw_io_enable(&sc->sc_regs, RTW_CR_RE | RTW_CR_TE, 1);

	rtw_resume_ticks(sc);

	return rc;
}

void
rtw_disable(struct rtw_softc *sc)
{
	int rc;

	if ((sc->sc_flags & RTW_F_ENABLED) == 0)
		return;

	/* turn off PHY */
	if ((rc = rtw_pwrstate(sc, RTW_OFF)) != 0)
		printf("%s: failed to turn off PHY (%d)\n",
		    sc->sc_dev.dv_xname, rc);

	if (sc->sc_disable != NULL)
		(*sc->sc_disable)(sc);

	sc->sc_flags &= ~RTW_F_ENABLED;
}

int
rtw_enable(struct rtw_softc *sc)
{
	if ((sc->sc_flags & RTW_F_ENABLED) == 0) {
		if (sc->sc_enable != NULL && (*sc->sc_enable)(sc) != 0) {
			printf("%s: device enable failed\n",
			    sc->sc_dev.dv_xname);
			return (EIO);
		}
		sc->sc_flags |= RTW_F_ENABLED;
	}
	return (0);
}

void
rtw_transmit_config(struct rtw_regs *regs)
{
	u_int32_t tcr;

	tcr = RTW_READ(regs, RTW_TCR);

	tcr |= RTW_TCR_CWMIN;
	tcr &= ~RTW_TCR_MXDMA_MASK;
	tcr |= RTW_TCR_MXDMA_256;
	tcr |= RTW_TCR_SAT;		/* send ACK as fast as possible */
	tcr &= ~RTW_TCR_LBK_MASK;
	tcr |= RTW_TCR_LBK_NORMAL;	/* normal operating mode */

	/* set short/long retry limits */
	tcr &= ~(RTW_TCR_SRL_MASK|RTW_TCR_LRL_MASK);
	tcr |= LSHIFT(4, RTW_TCR_SRL_MASK) | LSHIFT(4, RTW_TCR_LRL_MASK);

	tcr &= ~RTW_TCR_CRC;    /* NIC appends CRC32 */

	RTW_WRITE(regs, RTW_TCR, tcr);
	RTW_SYNC(regs, RTW_TCR, RTW_TCR);
}

__inline void
rtw_enable_interrupts(struct rtw_softc *sc)
{
	struct rtw_regs *regs = &sc->sc_regs;

	sc->sc_inten = RTW_INTR_RX|RTW_INTR_TX|RTW_INTR_BEACON|RTW_INTR_ATIMINT;
	sc->sc_inten |= RTW_INTR_IOERROR|RTW_INTR_TIMEOUT;

	RTW_WRITE16(regs, RTW_IMR, sc->sc_inten);
	RTW_WBW(regs, RTW_IMR, RTW_ISR);
	RTW_WRITE16(regs, RTW_ISR, 0xffff);
	RTW_SYNC(regs, RTW_IMR, RTW_ISR);

	/* XXX necessary? */
	if (sc->sc_intr_ack != NULL)
		(*sc->sc_intr_ack)(regs);
}

void
rtw_set_nettype(struct rtw_softc *sc, enum ieee80211_opmode opmode)
{
	uint8_t msr;

	/* I'm guessing that MSR is protected as CONFIG[0123] are. */
	rtw_set_access(sc, RTW_ACCESS_CONFIG);

	msr = RTW_READ8(&sc->sc_regs, RTW_MSR) & ~RTW_MSR_NETYPE_MASK;

	switch (opmode) {
	case IEEE80211_M_AHDEMO:
	case IEEE80211_M_IBSS:
		msr |= RTW_MSR_NETYPE_ADHOC_OK;
		break;
	case IEEE80211_M_HOSTAP:
		msr |= RTW_MSR_NETYPE_AP_OK;
		break;
	case IEEE80211_M_MONITOR:
		/* XXX */
		msr |= RTW_MSR_NETYPE_NOLINK;
		break;
	case IEEE80211_M_STA:
		msr |= RTW_MSR_NETYPE_INFRA_OK;
		break;
	}
	RTW_WRITE8(&sc->sc_regs, RTW_MSR, msr);

	rtw_set_access(sc, RTW_ACCESS_NONE);
}

/* XXX is the endianness correct? test. */
#define	rtw_calchash(addr) \
	(ether_crc32_le((addr), IEEE80211_ADDR_LEN) & BITS(5, 0))

void
rtw_pktfilt_load(struct rtw_softc *sc)
{
	struct rtw_regs *regs = &sc->sc_regs;
	struct ieee80211com *ic = &sc->sc_ic;
	struct arpcom *ec = &ic->ic_ac;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int hash;
	u_int32_t hashes[2] = { 0, 0 };
	struct ether_multi *enm;
	struct ether_multistep step;

	/* XXX might be necessary to stop Rx/Tx engines while setting filters */

#define RTW_RCR_MONITOR (RTW_RCR_ACRC32|RTW_RCR_APM|RTW_RCR_AAP|RTW_RCR_AB|RTW_RCR_ACF | RTW_RCR_AICV | RTW_RCR_ACRC32)

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		sc->sc_rcr |= RTW_RCR_MONITOR;
	else
		sc->sc_rcr &= ~RTW_RCR_MONITOR;

	/* XXX reference sources BEGIN */
	sc->sc_rcr |= RTW_RCR_ENMARP;
	sc->sc_rcr |= RTW_RCR_AB | RTW_RCR_AM | RTW_RCR_APM;
#if 0
	/* receive broadcasts in our BSS */
	sc->sc_rcr |= RTW_RCR_ADD3;
#endif
	/* XXX reference sources END */

	/* receive pwrmgmt frames. */
	sc->sc_rcr |= RTW_RCR_APWRMGT;
	/* receive mgmt/ctrl/data frames. */
	sc->sc_rcr |= RTW_RCR_ADF | RTW_RCR_AMF;
	/* initialize Rx DMA threshold, Tx DMA burst size */
	sc->sc_rcr |= RTW_RCR_RXFTH_WHOLE | RTW_RCR_MXDMA_1024;

	ifp->if_flags &= ~IFF_ALLMULTI;

	if (ifp->if_flags & IFF_PROMISC) {
		sc->sc_rcr |= RTW_RCR_AB;	/* accept all broadcast */
allmulti:
		ifp->if_flags |= IFF_ALLMULTI;
		goto setit;
	}

	/*
	 * Program the 64-bit multicast hash filter.
	 */
	ETHER_FIRST_MULTI(step, ec, enm);
	while (enm != NULL) {
		/* XXX */
		if (memcmp(enm->enm_addrlo, enm->enm_addrhi,
		    ETHER_ADDR_LEN) != 0)
			goto allmulti;

		hash = rtw_calchash(enm->enm_addrlo);
		hashes[hash >> 5] |= 1 << (hash & 0x1f);
		ETHER_NEXT_MULTI(step, enm);
	}

	if (ifp->if_flags & IFF_BROADCAST) {
		hash = rtw_calchash(etherbroadcastaddr);
		hashes[hash >> 5] |= 1 << (hash & 0x1f);
	}

	/* all bits set => hash is useless */
	if (~(hashes[0] & hashes[1]) == 0)
		goto allmulti;

 setit:
	if (ifp->if_flags & IFF_ALLMULTI)
		sc->sc_rcr |= RTW_RCR_AM;	/* accept all multicast */

	if (ic->ic_state == IEEE80211_S_SCAN)
		sc->sc_rcr |= RTW_RCR_AB;	/* accept all broadcast */

	hashes[0] = hashes[1] = 0xffffffff;

	RTW_WRITE(regs, RTW_MAR0, hashes[0]);
	RTW_WRITE(regs, RTW_MAR1, hashes[1]);
	RTW_WRITE(regs, RTW_RCR, sc->sc_rcr);
	RTW_SYNC(regs, RTW_MAR0, RTW_RCR); /* RTW_MAR0 < RTW_MAR1 < RTW_RCR */

	DPRINTF(sc, RTW_DEBUG_PKTFILT,
	    ("%s: RTW_MAR0 %08x RTW_MAR1 %08x RTW_RCR %08x\n",
	    sc->sc_dev.dv_xname, RTW_READ(regs, RTW_MAR0),
	    RTW_READ(regs, RTW_MAR1), RTW_READ(regs, RTW_RCR)));

	return;
}

/* Must be called at splnet. */
int
rtw_init(struct ifnet *ifp)
{
	struct rtw_softc *sc = (struct rtw_softc *)ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct rtw_regs *regs = &sc->sc_regs;
	int rc = 0;

	if ((rc = rtw_enable(sc)) != 0)
		goto out;

	/* Cancel pending I/O and reset. */
	rtw_stop(ifp, 0);

	ic->ic_bss->ni_chan = ic->ic_ibss_chan;
	DPRINTF(sc, RTW_DEBUG_TUNE, ("%s: channel %d freq %d flags 0x%04x\n",
	    __func__, ieee80211_chan2ieee(ic, ic->ic_bss->ni_chan),
	    ic->ic_bss->ni_chan->ic_freq, ic->ic_bss->ni_chan->ic_flags));

	if ((rc = rtw_pwrstate(sc, RTW_OFF)) != 0)
		goto out;

	rtw_swring_setup(sc);

	rtw_transmit_config(regs);

	rtw_set_access(sc, RTW_ACCESS_CONFIG);

	RTW_WRITE8(regs, RTW_MSR, 0x0); /* no link */
	RTW_WBW(regs, RTW_MSR, RTW_BRSR);

	/* long PLCP header, 1Mb/2Mb basic rate */
	RTW_WRITE16(regs, RTW_BRSR, RTW_BRSR_MBR8180_2MBPS);
	RTW_SYNC(regs, RTW_BRSR, RTW_BRSR);

	rtw_set_access(sc, RTW_ACCESS_ANAPARM);
	rtw_set_access(sc, RTW_ACCESS_NONE);

#if 0
	RTW_WRITE(regs, RTW_FEMR, RTW_FEMR_GWAKE|RTW_FEMR_WKUP|RTW_FEMR_INTR);
#endif
	/* XXX from reference sources */
	RTW_WRITE(regs, RTW_FEMR, 0xffff);
	RTW_SYNC(regs, RTW_FEMR, RTW_FEMR);

	rtw_set_rfprog(regs, sc->sc_rfchipid, sc->sc_dev.dv_xname);

	RTW_WRITE8(regs, RTW_PHYDELAY, sc->sc_phydelay);
	/* from Linux driver */
	RTW_WRITE8(regs, RTW_CRCOUNT, RTW_CRCOUNT_MAGIC);

	RTW_SYNC(regs, RTW_PHYDELAY, RTW_CRCOUNT);

	rtw_enable_interrupts(sc);

	rtw_pktfilt_load(sc);

	rtw_hwring_setup(sc);

	rtw_io_enable(regs, RTW_CR_RE|RTW_CR_TE, 1);

	ifp->if_flags |= IFF_RUNNING;
	ic->ic_state = IEEE80211_S_INIT;

	RTW_WRITE16(regs, RTW_BSSID16, 0x0);
	RTW_WRITE(regs, RTW_BSSID32, 0x0);

	rtw_resume_ticks(sc);

	rtw_set_nettype(sc, IEEE80211_M_MONITOR);

	if (ic->ic_opmode == IEEE80211_M_MONITOR)
		return ieee80211_new_state(ic, IEEE80211_S_RUN, -1);
	else
		return ieee80211_new_state(ic, IEEE80211_S_SCAN, -1);

out:
	return rc;
}

int
rtw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	int rc = 0, s;
	struct rtw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq *ifr = (struct ifreq *)data;
	struct ifaddr *ifa = (struct ifaddr *)data;

	s = splnet();
	switch (cmd) {
	case SIOCSIFMTU:
		if (ifr->ifr_mtu > ETHERMTU || ifr->ifr_mtu < ETHERMIN) {
			rc = EINVAL;
		} else if (ifp->if_mtu != ifr->ifr_mtu) {
			ifp->if_mtu = ifr->ifr_mtu;
		}
		break;
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;
#ifdef INET
		if (ifa->ifa_addr->sa_family == AF_INET) {
			arp_ifinit(&ic->ic_ac, ifa);
		}
#endif  /* INET */
		/* FALLTHROUGH */

	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) != 0) {
			if (0 && (sc->sc_flags & RTW_F_ENABLED) != 0) {
				rtw_pktfilt_load(sc);
			} else
				rc = rtw_init(ifp);
#ifdef RTW_DEBUG
			rtw_print_regs(&sc->sc_regs, ifp->if_xname, __func__);
#endif /* RTW_DEBUG */
		} else if ((sc->sc_flags & RTW_F_ENABLED) != 0) {
#ifdef RTW_DEBUG
			rtw_print_regs(&sc->sc_regs, ifp->if_xname, __func__);
#endif /* RTW_DEBUG */
			rtw_stop(ifp, 1);
		}
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		if (cmd == SIOCADDMULTI)
			rc = ether_addmulti(ifr, &sc->sc_ic.ic_ac);
		else
			rc = ether_delmulti(ifr, &sc->sc_ic.ic_ac);
		if (rc == ENETRESET) {
			if (sc->sc_flags & IFF_RUNNING)
				rtw_pktfilt_load(sc);
			rc = 0;
		}
		break;
	default:
		if ((rc = ieee80211_ioctl(ifp, cmd, data)) == ENETRESET) {
			if ((sc->sc_flags & RTW_F_ENABLED) != 0)
				rc = rtw_init(ifp);
			else
				rc = 0;
		}
		break;
	}
	splx(s);
	return rc;
}

/* Point *mp at the next 802.11 frame to transmit.  Point *stcp
 * at the driver's selection of transmit control block for the packet.
 */
__inline int
rtw_dequeue(struct ifnet *ifp, struct rtw_txctl_blk **stcp,
    struct rtw_txdesc_blk **htcp, struct mbuf **mp,
    struct ieee80211_node **nip)
{
	struct rtw_txctl_blk *stc;
	struct rtw_txdesc_blk *htc;
	struct mbuf *m0;
	struct rtw_softc *sc;
	struct ieee80211com *ic;

	sc = (struct rtw_softc *)ifp->if_softc;

	DPRINTF(sc, RTW_DEBUG_XMIT,
	    ("%s: enter %s\n", sc->sc_dev.dv_xname, __func__));
	*mp = NULL;

	stc = &sc->sc_txctl_blk[RTW_TXPRIMD];
	htc = &sc->sc_txdesc_blk[RTW_TXPRIMD];

	if (SIMPLEQ_EMPTY(&stc->stc_freeq) || htc->htc_nfree == 0) {
		DPRINTF(sc, RTW_DEBUG_XMIT,
		    ("%s: out of descriptors\n", __func__));
		ifp->if_flags |= IFF_OACTIVE;
		return 0;
	}

	ic = &sc->sc_ic;

	if (!IF_IS_EMPTY(&ic->ic_mgtq)) {
		IF_DEQUEUE(&ic->ic_mgtq, m0);
		*nip = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
		m0->m_pkthdr.rcvif = NULL;
		DPRINTF(sc, RTW_DEBUG_XMIT,
		    ("%s: dequeue mgt frame\n", __func__));
	} else if (ic->ic_state != IEEE80211_S_RUN) {
		DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: not running\n", __func__));
		return 0;
	} else if (!IF_IS_EMPTY(&ic->ic_pwrsaveq)) {
		IF_DEQUEUE(&ic->ic_pwrsaveq, m0);
		*nip = (struct ieee80211_node *)m0->m_pkthdr.rcvif;
		m0->m_pkthdr.rcvif = NULL;
		DPRINTF(sc, RTW_DEBUG_XMIT,
		    ("%s: dequeue pwrsave frame\n", __func__));
	} else {
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL) {
			DPRINTF(sc, RTW_DEBUG_XMIT,
			    ("%s: no frame\n", __func__));
			return 0;
		}
		DPRINTF(sc, RTW_DEBUG_XMIT,
		    ("%s: dequeue data frame\n", __func__));
		ifp->if_opackets++;
#if NBPFILTER > 0
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0);
#endif
		if ((m0 = ieee80211_encap(ifp, m0, nip)) == NULL) {
			DPRINTF(sc, RTW_DEBUG_XMIT,
			    ("%s: encap error\n", __func__));
			ifp->if_oerrors++;
			return -1;
		}
	}
	DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: leave\n", __func__));
	*stcp = stc;
	*htcp = htc;
	*mp = m0;
	return 0;
}

int
rtw_seg_too_short(bus_dmamap_t dmamap)
{
	int i;
	for (i = 0; i < dmamap->dm_nsegs; i++) {
		if (dmamap->dm_segs[i].ds_len < 4) {
			printf("%s: segment too short\n", __func__);
			return 1;
		}
	}
	return 0;
}

/* TBD factor with atw_start */
struct mbuf *
rtw_dmamap_load_txbuf(bus_dma_tag_t dmat, bus_dmamap_t dmam, struct mbuf *chain,
    u_int ndescfree, short *ifflagsp, const char *dvname)
{
	int first, rc;
	struct mbuf *m, *m0;

	m0 = chain;

	/*
	 * Load the DMA map.  Copy and try (once) again if the packet
	 * didn't fit in the alloted number of segments.
	 */
	for (first = 1;
	     ((rc = bus_dmamap_load_mbuf(dmat, dmam, m0,
			  BUS_DMA_WRITE|BUS_DMA_NOWAIT)) != 0 ||
	      dmam->dm_nsegs > ndescfree || rtw_seg_too_short(dmam)) && first;
	     first = 0) {
		if (rc == 0)
			bus_dmamap_unload(dmat, dmam);
		MGETHDR(m, M_DONTWAIT, MT_DATA);
		if (m == NULL) {
			printf("%s: unable to allocate Tx mbuf\n",
			    dvname);
			break;
		}
		if (m0->m_pkthdr.len > MHLEN) {
			MCLGET(m, M_DONTWAIT);
			if ((m->m_flags & M_EXT) == 0) {
				printf("%s: cannot allocate Tx cluster\n",
				    dvname);
				m_freem(m);
				break;
			}
		}
		m_copydata(m0, 0, m0->m_pkthdr.len, mtod(m, caddr_t));
		m->m_pkthdr.len = m->m_len = m0->m_pkthdr.len;
		m_freem(m0);
		m0 = m;
		m = NULL;
	}
	if (rc != 0) {
		printf("%s: cannot load Tx buffer, rc = %d\n", dvname, rc);
		m_freem(m0);
		return NULL;
	} else if (rtw_seg_too_short(dmam)) {
		printf("%s: cannot load Tx buffer, segment too short\n",
		    dvname);
		bus_dmamap_unload(dmat, dmam);
		m_freem(m0);
		return NULL;
	} else if (dmam->dm_nsegs > ndescfree) {
		*ifflagsp |= IFF_OACTIVE;
		bus_dmamap_unload(dmat, dmam);
		m_freem(m0);
		return NULL;
	}
	return m0;
}

#ifdef RTW_DEBUG
void
rtw_print_txdesc(struct rtw_softc *sc, const char *action,
    struct rtw_txctl *stx, struct rtw_txdesc_blk *htc, int desc)
{
	struct rtw_txdesc *htx = &htc->htc_desc[desc];
	DPRINTF(sc, RTW_DEBUG_XMIT_DESC, ("%s: %p %s txdesc[%d] ctl0 %#08x "
	    "ctl1 %#08x buf %#08x len %#08x\n",
	    sc->sc_dev.dv_xname, stx, action, desc,
	    letoh32(htx->htx_ctl0),
	    letoh32(htx->htx_ctl1), letoh32(htx->htx_buf),
	    letoh32(htx->htx_len)));
}
#endif /* RTW_DEBUG */

void
rtw_start(struct ifnet *ifp)
{
	uint8_t tppoll;
	int desc, i, lastdesc, npkt, rate;
	uint32_t proto_ctl0, ctl0, ctl1;
	bus_dmamap_t		dmamap;
	struct ieee80211com	*ic;
	struct ieee80211_duration *d0;
	struct ieee80211_frame	*wh;
	struct ieee80211_node	*ni;
	struct mbuf		*m0;
	struct rtw_softc	*sc;
	struct rtw_txctl_blk	*stc;
	struct rtw_txdesc_blk	*htc;
	struct rtw_txctl	*stx;
	struct rtw_txdesc	*htx;

	sc = (struct rtw_softc *)ifp->if_softc;
	ic = &sc->sc_ic;

	DPRINTF(sc, RTW_DEBUG_XMIT,
	    ("%s: enter %s\n", sc->sc_dev.dv_xname, __func__));

	/* XXX do real rate control */
	proto_ctl0 = RTW_TXCTL0_RTSRATE_1MBPS;

	switch (rate = MAX(2, ieee80211_get_rate(ic))) {
	case 2:
		proto_ctl0 |= RTW_TXCTL0_RATE_1MBPS;
		break;
	case 4:
		proto_ctl0 |= RTW_TXCTL0_RATE_2MBPS;
		break;
	case 11:
		proto_ctl0 |= RTW_TXCTL0_RATE_5MBPS;
		break;
	case 22:
		proto_ctl0 |= RTW_TXCTL0_RATE_11MBPS;
		break;
	}

	if ((ic->ic_flags & IEEE80211_F_SHPREAMBLE) != 0)
		proto_ctl0 |= RTW_TXCTL0_SPLCP;

	for (;;) {
		if (rtw_dequeue(ifp, &stc, &htc, &m0, &ni) == -1)
			continue;
		if (m0 == NULL)
			break;
		stx = SIMPLEQ_FIRST(&stc->stc_freeq);

		dmamap = stx->stx_dmamap;

		m0 = rtw_dmamap_load_txbuf(sc->sc_dmat, dmamap, m0,
		    htc->htc_nfree, &ifp->if_flags, sc->sc_dev.dv_xname);

		if (m0 == NULL || dmamap->dm_nsegs == 0) {
			DPRINTF(sc, RTW_DEBUG_XMIT,
			    ("%s: fail dmamap load\n", __func__));
			goto post_dequeue_err;
		}

#ifdef RTW_DEBUG
		if ((sc->sc_if.if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) {
			ieee80211_dump_pkt(mtod(m0, uint8_t *),
			    (dmamap->dm_nsegs == 1) ? m0->m_pkthdr.len
			                            : sizeof(wh),
			    rate, 0);
		}
#endif /* RTW_DEBUG */
		ctl0 = proto_ctl0 |
		    LSHIFT(m0->m_pkthdr.len, RTW_TXCTL0_TPKTSIZE_MASK);

		wh = mtod(m0, struct ieee80211_frame *);

		if (ieee80211_compute_duration(wh, m0->m_pkthdr.len,
		    ic->ic_flags, ic->ic_fragthreshold,
		    rate, &stx->stx_d0, &stx->stx_dn, &npkt,
		    (sc->sc_if.if_flags & (IFF_DEBUG|IFF_LINK2)) ==
		    (IFF_DEBUG|IFF_LINK2)) == -1) {
			DPRINTF(sc, RTW_DEBUG_XMIT,
			    ("%s: fail compute duration\n", __func__));
			goto post_load_err;
		}

		/* XXX >= ? */
		if (m0->m_pkthdr.len > ic->ic_rtsthreshold)
			ctl0 |= RTW_TXCTL0_RTSEN;

		d0 = &stx->stx_d0;

		*(uint16_t*)wh->i_dur = htole16(d0->d_data_dur);

		ctl1 = LSHIFT(d0->d_plcp_len, RTW_TXCTL1_LENGTH_MASK) |
		    LSHIFT(d0->d_rts_dur, RTW_TXCTL1_RTSDUR_MASK);

		if (d0->d_residue)
			ctl1 |= RTW_TXCTL1_LENGEXT;

		/* TBD fragmentation */

		stx->stx_first = htc->htc_next;

		rtw_txdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap,
		    htc, stx->stx_first, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREWRITE);

		KASSERT(stx->stx_first < htc->htc_ndesc);

		for (i = 0, lastdesc = desc = stx->stx_first;
		     i < dmamap->dm_nsegs;
		     i++, desc = RTW_NEXT_IDX(htc, desc)) {
			if (dmamap->dm_segs[i].ds_len > RTW_TXLEN_LENGTH_MASK) {
				DPRINTF(sc, RTW_DEBUG_XMIT_DESC,
				    ("%s: seg too long\n", __func__));
				goto post_load_err;
			}
			htx = &htc->htc_desc[desc];
			htx->htx_ctl0 = htole32(ctl0);
			if (i != 0)
				htx->htx_ctl0 |= htole32(RTW_TXCTL0_OWN);
			htx->htx_ctl1 = htole32(ctl1);
			htx->htx_buf = htole32(dmamap->dm_segs[i].ds_addr);
			htx->htx_len = htole32(dmamap->dm_segs[i].ds_len);
			lastdesc = desc;
#ifdef RTW_DEBUG
			rtw_print_txdesc(sc, "load", stx, htc, desc);
#endif /* RTW_DEBUG */
		}

		KASSERT(desc < htc->htc_ndesc);

		stx->stx_ni = ni;
		stx->stx_mbuf = m0;
		stx->stx_last = lastdesc;
		htc->htc_desc[stx->stx_last].htx_ctl0 |= htole32(RTW_TXCTL0_LS);
		htc->htc_desc[stx->stx_first].htx_ctl0 |=
		   htole32(RTW_TXCTL0_FS);

#ifdef RTW_DEBUG
		rtw_print_txdesc(sc, "FS on", stx, htc, stx->stx_first);
		rtw_print_txdesc(sc, "LS on", stx, htc, stx->stx_last);
#endif /* RTW_DEBUG */

		htc->htc_nfree -= dmamap->dm_nsegs;
		htc->htc_next = desc;

		rtw_txdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap,
		    htc, stx->stx_first, dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		htc->htc_desc[stx->stx_first].htx_ctl0 |=
		    htole32(RTW_TXCTL0_OWN);

#ifdef RTW_DEBUG
		rtw_print_txdesc(sc, "OWN on", stx, htc, stx->stx_first);
#endif /* RTW_DEBUG */

		rtw_txdescs_sync(sc->sc_dmat, sc->sc_desc_dmamap,
		    htc, stx->stx_first, 1,
		    BUS_DMASYNC_PREREAD|BUS_DMASYNC_PREWRITE);

		SIMPLEQ_REMOVE_HEAD(&stc->stc_freeq, stx_q);
		SIMPLEQ_INSERT_TAIL(&stc->stc_dirtyq, stx, stx_q);

		stc->stc_tx_timer = 5;
		ifp->if_timer = 1;

		tppoll = RTW_READ8(&sc->sc_regs, RTW_TPPOLL);

		/* TBD poke other queues. */
		RTW_WRITE8(&sc->sc_regs, RTW_TPPOLL, tppoll | RTW_TPPOLL_NPQ);
		RTW_SYNC(&sc->sc_regs, RTW_TPPOLL, RTW_TPPOLL);
	}
	DPRINTF(sc, RTW_DEBUG_XMIT, ("%s: leave\n", __func__));
	return;
post_load_err:
	bus_dmamap_unload(sc->sc_dmat, dmamap);
	m_freem(m0);
post_dequeue_err:
	if (ni != ic->ic_bss)
		ieee80211_free_node(&sc->sc_ic, ni);
	return;
}

void
rtw_watchdog(struct ifnet *ifp)
{
	int pri;
	struct rtw_softc *sc;
	struct rtw_txctl_blk *stc;

	sc = ifp->if_softc;

	ifp->if_timer = 0;

	if ((sc->sc_flags & RTW_F_ENABLED) == 0)
		return;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		stc = &sc->sc_txctl_blk[pri];

		if (stc->stc_tx_timer == 0)
			continue;

		if (--stc->stc_tx_timer == 0) {
			if (SIMPLEQ_EMPTY(&stc->stc_dirtyq))
				continue;
			printf("%s: transmit timeout, priority %d\n",
			    ifp->if_xname, pri);
			ifp->if_oerrors++;
			/* Stop Tx DMA, disable transmitter, clear
			 * Tx rings, and restart.
			 */
			RTW_WRITE8(&sc->sc_regs, RTW_TPPOLL, RTW_TPPOLL_SNPQ);
			RTW_SYNC(&sc->sc_regs, RTW_TPPOLL, RTW_TPPOLL);
			rtw_io_enable(&sc->sc_regs, RTW_CR_TE, 0);
			rtw_txdescs_reset(sc);
			rtw_io_enable(&sc->sc_regs, RTW_CR_TE, 1);
			rtw_start(ifp);
		} else
			ifp->if_timer = 1;
	}
	ieee80211_watchdog(ifp);
	return;
}

void
rtw_start_beacon(struct rtw_softc *sc, int enable)
{
	/* TBD */
	return;
}

void
rtw_next_scan(void *arg)
{
	struct rtw_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int s;

	/* don't call rtw_start w/o network interrupts blocked */
	s = splnet();
	if (ic->ic_state == IEEE80211_S_SCAN)
		ieee80211_next_scan(ifp);
	splx(s);
}

void
rtw_join_bss(struct rtw_softc *sc, uint8_t *bssid, enum ieee80211_opmode opmode,
    uint16_t intval0)
{
	uint16_t bcnitv, intval;
	int i;
	struct rtw_regs *regs = &sc->sc_regs;

	for (i = 0; i < IEEE80211_ADDR_LEN; i++)
		RTW_WRITE8(regs, RTW_BSSID + i, bssid[i]);

	RTW_SYNC(regs, RTW_BSSID16, RTW_BSSID32);

	rtw_set_access(sc, RTW_ACCESS_CONFIG);

	intval = MIN(intval0, PRESHIFT(RTW_BCNITV_BCNITV_MASK));

	bcnitv = RTW_READ16(regs, RTW_BCNITV) & ~RTW_BCNITV_BCNITV_MASK;
	bcnitv |= LSHIFT(intval, RTW_BCNITV_BCNITV_MASK);
	RTW_WRITE16(regs, RTW_BCNITV, bcnitv);
	/* magic from Linux */
	RTW_WRITE16(regs, RTW_ATIMWND, LSHIFT(1, RTW_ATIMWND_ATIMWND));
	RTW_WRITE16(regs, RTW_ATIMTRITV, LSHIFT(2, RTW_ATIMTRITV_ATIMTRITV));

	rtw_set_nettype(sc, opmode);

	rtw_set_access(sc, RTW_ACCESS_NONE);

	/* TBD WEP */
	RTW_WRITE8(regs, RTW_SCR, 0);

	rtw_io_enable(regs, RTW_CR_RE | RTW_CR_TE, 1);
}

/* Synchronize the hardware state with the software state. */
int
rtw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
	struct ifnet *ifp = &ic->ic_if;
	struct rtw_softc *sc = ifp->if_softc;
	enum ieee80211_state ostate;
	int error;

	ostate = ic->ic_state;

	if (nstate == IEEE80211_S_INIT) {
		timeout_del(&sc->sc_scan_to);
		sc->sc_cur_chan = IEEE80211_CHAN_ANY;
		rtw_start_beacon(sc, 0);
		return (*sc->sc_mtbl.mt_newstate)(ic, nstate, arg);
	}

	if (ostate == IEEE80211_S_INIT && nstate != IEEE80211_S_INIT)
		rtw_pwrstate(sc, RTW_ON);

	if ((error = rtw_tune(sc)) != 0)
		return error;

	switch (nstate) {
	case IEEE80211_S_ASSOC:
		rtw_join_bss(sc, ic->ic_bss->ni_bssid, ic->ic_opmode,
		    ic->ic_bss->ni_intval);
		break;
	case IEEE80211_S_INIT:
		panic("%s: unexpected state IEEE80211_S_INIT\n", __func__);
		break;
	case IEEE80211_S_SCAN:
		if (ostate != IEEE80211_S_SCAN) {
			(void)memset(ic->ic_bss->ni_bssid, 0,
			    IEEE80211_ADDR_LEN);
			rtw_join_bss(sc, ic->ic_bss->ni_bssid, ic->ic_opmode,
			    ic->ic_bss->ni_intval);
		}

		timeout_add(&sc->sc_scan_to, rtw_dwelltime * hz / 1000);

		break;
	case IEEE80211_S_RUN:
		if (ic->ic_opmode == IEEE80211_M_STA)
			break;
		/*FALLTHROUGH*/
	case IEEE80211_S_AUTH:
#if 0
		rtw_write_bcn_thresh(sc);
		rtw_write_ssid(sc);
		rtw_write_sup_rates(sc);
#endif
		if (ic->ic_opmode == IEEE80211_M_AHDEMO ||
		    ic->ic_opmode == IEEE80211_M_MONITOR)
			break;

		/* TBD set listen interval */

#if 0
		rtw_tsf(sc);
#endif
		break;
	}

	if (nstate != IEEE80211_S_SCAN)
		timeout_del(&sc->sc_scan_to);

	if (nstate == IEEE80211_S_RUN &&
	    (ic->ic_opmode == IEEE80211_M_HOSTAP ||
	     ic->ic_opmode == IEEE80211_M_IBSS))
		rtw_start_beacon(sc, 1);
	else
		rtw_start_beacon(sc, 0);

	return (*sc->sc_mtbl.mt_newstate)(ic, nstate, arg);
}

void
rtw_recv_beacon(struct rtw_softc *sc, struct mbuf *m,
    struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	(*sc->sc_mtbl.mt_recv_mgmt)(&sc->sc_ic, m, ni, subtype, rssi, rstamp);
	return;
}

void
rtw_recv_mgmt(struct ieee80211com *ic, struct mbuf *m,
    struct ieee80211_node *ni, int subtype, int rssi, u_int32_t rstamp)
{
	struct rtw_softc *sc = (struct rtw_softc*)ic->ic_softc;

	switch (subtype) {
	case IEEE80211_FC0_SUBTYPE_PROBE_REQ:
		/* do nothing: hardware answers probe request XXX */
		break;
	case IEEE80211_FC0_SUBTYPE_PROBE_RESP:
	case IEEE80211_FC0_SUBTYPE_BEACON:
		rtw_recv_beacon(sc, m, ni, subtype, rssi, rstamp);
		break;
	default:
		(*sc->sc_mtbl.mt_recv_mgmt)(ic, m, ni, subtype, rssi, rstamp);
		break;
	}
	return;
}

struct ieee80211_node *
rtw_node_alloc(struct ieee80211com *ic)
{
	struct rtw_softc *sc = (struct rtw_softc *)ic->ic_if.if_softc;
	struct ieee80211_node *ni = (*sc->sc_mtbl.mt_node_alloc)(ic);

	DPRINTF(sc, RTW_DEBUG_NODE,
	    ("%s: alloc node %p\n", sc->sc_dev.dv_xname, ni));
	return ni;
}

void
rtw_node_free(struct ieee80211com *ic, struct ieee80211_node *ni)
{
	struct rtw_softc *sc = (struct rtw_softc *)ic->ic_if.if_softc;

	DPRINTF(sc, RTW_DEBUG_NODE,
	    ("%s: freeing node %p %s\n", sc->sc_dev.dv_xname, ni,
	    ether_sprintf(ni->ni_bssid)));
	(*sc->sc_mtbl.mt_node_free)(ic, ni);
}

int
rtw_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_RUNNING|IFF_UP)) ==
		    (IFF_RUNNING|IFF_UP))
			rtw_init(ifp);		/* XXX lose error */
		error = 0;
	}
	return error;
}

void
rtw_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct rtw_softc *sc = ifp->if_softc;

	if ((sc->sc_flags & RTW_F_ENABLED) == 0) {
		imr->ifm_active = IFM_IEEE80211 | IFM_NONE;
		imr->ifm_status = 0;
		return;
	}
	ieee80211_media_status(ifp, imr);
}

void
rtw_power(int why, void *arg)
{
	struct rtw_softc *sc = arg;
	struct ifnet *ifp = &sc->sc_ic.ic_if;
	int s;

	DPRINTF(sc, RTW_DEBUG_PWR,
	    ("%s: rtw_power(%d,)\n", sc->sc_dev.dv_xname, why));

	s = splnet();
	switch (why) {
	case PWR_STANDBY:
		/* XXX do nothing. */
		break;
	case PWR_SUSPEND:
		rtw_stop(ifp, 0);
		if (sc->sc_power != NULL)
			(*sc->sc_power)(sc, why);
		break;
	case PWR_RESUME:
		if (ifp->if_flags & IFF_UP) {
			if (sc->sc_power != NULL)
				(*sc->sc_power)(sc, why);
			rtw_init(ifp);
		}
		break;
	}
	splx(s);
}

/* rtw_shutdown: make sure the interface is stopped at reboot time. */
void
rtw_shutdown(void *arg)
{
	struct rtw_softc *sc = arg;

	rtw_stop(&sc->sc_ic.ic_if, 1);
}

__inline void
rtw_setifprops(struct ifnet *ifp, const char *dvname, void *softc)
{
	(void)memcpy(ifp->if_xname, dvname, IFNAMSIZ);
	ifp->if_softc = softc;
	ifp->if_flags = IFF_SIMPLEX | IFF_BROADCAST | IFF_MULTICAST |
	    IFF_NOTRAILERS;
	ifp->if_ioctl = rtw_ioctl;
	ifp->if_start = rtw_start;
	ifp->if_watchdog = rtw_watchdog;
}

__inline void
rtw_set80211props(struct ieee80211com *ic)
{
	int nrate;
	ic->ic_phytype = IEEE80211_T_DS;
	ic->ic_opmode = IEEE80211_M_STA;
	ic->ic_caps = IEEE80211_C_PMGT | IEEE80211_C_IBSS |
	    IEEE80211_C_HOSTAP | IEEE80211_C_MONITOR | IEEE80211_C_WEP;

	nrate = 0;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] =
	    IEEE80211_RATE_BASIC | 2;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] =
	    IEEE80211_RATE_BASIC | 4;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 11;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_rates[nrate++] = 22;
	ic->ic_sup_rates[IEEE80211_MODE_11B].rs_nrates = nrate;
}

__inline void
rtw_set80211methods(struct rtw_mtbl *mtbl, struct ieee80211com *ic)
{
	mtbl->mt_newstate = ic->ic_newstate;
	ic->ic_newstate = rtw_newstate;

	mtbl->mt_recv_mgmt = ic->ic_recv_mgmt;
	ic->ic_recv_mgmt = rtw_recv_mgmt;

	mtbl->mt_node_free = ic->ic_node_free;
	ic->ic_node_free = rtw_node_free;

	mtbl->mt_node_alloc = ic->ic_node_alloc;
	ic->ic_node_alloc = rtw_node_alloc;
}

__inline void
rtw_establish_hooks(struct rtw_hooks *hooks, const char *dvname,
    void *arg)
{
	/*
	 * Make sure the interface is shutdown during reboot.
	 */
	hooks->rh_shutdown = shutdownhook_establish(rtw_shutdown, arg);
	if (hooks->rh_shutdown == NULL)
		printf("%s: WARNING: unable to establish shutdown hook\n",
		    dvname);

	/*
	 * Add a suspend hook to make sure we come back up after a
	 * resume.
	 */
	hooks->rh_power = powerhook_establish(rtw_power, arg);
	if (hooks->rh_power == NULL)
		printf("%s: WARNING: unable to establish power hook\n",
		    dvname);
}

__inline void
rtw_disestablish_hooks(struct rtw_hooks *hooks, const char *dvname,
    void *arg)
{
	if (hooks->rh_shutdown != NULL)
		shutdownhook_disestablish(hooks->rh_shutdown);

	if (hooks->rh_power != NULL)
		powerhook_disestablish(hooks->rh_power);
}

__inline void
rtw_init_radiotap(struct rtw_softc *sc)
{
	memset(&sc->sc_rxtapu, 0, sizeof(sc->sc_rxtapu));
	sc->sc_rxtap.rr_ihdr.it_len = sizeof(sc->sc_rxtapu);
	sc->sc_rxtap.rr_ihdr.it_present = RTW_RX_RADIOTAP_PRESENT;

	memset(&sc->sc_txtapu, 0, sizeof(sc->sc_txtapu));
	sc->sc_txtap.rt_ihdr.it_len = sizeof(sc->sc_txtapu);
	sc->sc_txtap.rt_ihdr.it_present = RTW_TX_RADIOTAP_PRESENT;
}

int
rtw_txctl_blk_setup(struct rtw_txctl_blk *stc, u_int qlen)
{
	SIMPLEQ_INIT(&stc->stc_dirtyq);
	SIMPLEQ_INIT(&stc->stc_freeq);
	stc->stc_ndesc = qlen;
	stc->stc_desc = malloc(qlen * sizeof(*stc->stc_desc), M_DEVBUF,
	    M_NOWAIT);
	if (stc->stc_desc == NULL)
		return ENOMEM;
	return 0;
}

void
rtw_txctl_blk_cleanup_all(struct rtw_softc *sc)
{
	int pri;
	struct rtw_txctl_blk *stc;

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		stc = &sc->sc_txctl_blk[pri];
		free(stc->stc_desc, M_DEVBUF);
		stc->stc_desc = NULL;
	}
}

int
rtw_txctl_blk_setup_all(struct rtw_softc *sc)
{
	int pri, rc = 0;
	int qlen[RTW_NTXPRI] =
	     {RTW_TXQLENLO, RTW_TXQLENMD, RTW_TXQLENHI, RTW_TXQLENBCN};

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		rc = rtw_txctl_blk_setup(&sc->sc_txctl_blk[pri], qlen[pri]);
		if (rc != 0)
			break;
	}
	return rc;
}

void
rtw_txdesc_blk_setup(struct rtw_txdesc_blk *htc, struct rtw_txdesc *desc,
    u_int ndesc, bus_addr_t ofs, bus_addr_t physbase)
{
	htc->htc_ndesc = ndesc;
	htc->htc_desc = desc;
	htc->htc_physbase = physbase;
	htc->htc_ofs = ofs;

	(void)memset(htc->htc_desc, 0,
	    sizeof(htc->htc_desc[0]) * htc->htc_ndesc);

	rtw_txdesc_blk_reset(htc);
}

void
rtw_txdesc_blk_setup_all(struct rtw_softc *sc)
{
	rtw_txdesc_blk_setup(&sc->sc_txdesc_blk[RTW_TXPRILO],
	    &sc->sc_descs->hd_txlo[0], RTW_NTXDESCLO,
	    RTW_RING_OFFSET(hd_txlo), RTW_RING_BASE(sc, hd_txlo));

	rtw_txdesc_blk_setup(&sc->sc_txdesc_blk[RTW_TXPRIMD],
	    &sc->sc_descs->hd_txmd[0], RTW_NTXDESCMD,
	    RTW_RING_OFFSET(hd_txmd), RTW_RING_BASE(sc, hd_txmd));

	rtw_txdesc_blk_setup(&sc->sc_txdesc_blk[RTW_TXPRIHI],
	    &sc->sc_descs->hd_txhi[0], RTW_NTXDESCHI,
	    RTW_RING_OFFSET(hd_txhi), RTW_RING_BASE(sc, hd_txhi));

	rtw_txdesc_blk_setup(&sc->sc_txdesc_blk[RTW_TXPRIBCN],
	    &sc->sc_descs->hd_bcn[0], RTW_NTXDESCBCN,
	    RTW_RING_OFFSET(hd_bcn), RTW_RING_BASE(sc, hd_bcn));
}

struct rtw_rf *
rtw_rf_attach(struct rtw_softc *sc, enum rtw_rfchipid rfchipid, int digphy)
{
	rtw_rf_write_t rf_write;
	struct rtw_rf *rf;

	switch (rfchipid) {
	default:
		rf_write = rtw_rf_hostwrite;
		break;
	case RTW_RFCHIPID_INTERSIL:
	case RTW_RFCHIPID_PHILIPS:
	case RTW_RFCHIPID_GCT: /* XXX a guess */
	case RTW_RFCHIPID_RFMD:
		rf_write = (rtw_host_rfio) ? rtw_rf_hostwrite : rtw_rf_macwrite;
		break;

	}

	switch (rfchipid) {
	case RTW_RFCHIPID_MAXIM:
		rf = rtw_max2820_create(&sc->sc_regs, rf_write, 0);
		sc->sc_pwrstate_cb = rtw_maxim_pwrstate;
		break;
	case RTW_RFCHIPID_PHILIPS:
		rf = rtw_sa2400_create(&sc->sc_regs, rf_write, digphy);
		sc->sc_pwrstate_cb = rtw_philips_pwrstate;
		break;
	case RTW_RFCHIPID_RFMD:
		/* XXX RFMD has no RF constructor */
		sc->sc_pwrstate_cb = rtw_rfmd_pwrstate;
		/*FALLTHROUGH*/
	default:
		return NULL;
	}
	rf->rf_continuous_tx_cb =
	    (rtw_continuous_tx_cb_t)rtw_continuous_tx_enable;
	rf->rf_continuous_tx_arg = (void *)sc;
	return rf;
}

/* Revision C and later use a different PHY delay setting than
 * revisions A and B.
 */
u_int8_t
rtw_check_phydelay(struct rtw_regs *regs, u_int32_t rcr0)
{
#define REVAB (RTW_RCR_MXDMA_UNLIMITED | RTW_RCR_AICV)
#define REVC (REVAB | RTW_RCR_RXFTH_WHOLE)

	u_int8_t phydelay = LSHIFT(0x6, RTW_PHYDELAY_PHYDELAY);

	RTW_WRITE(regs, RTW_RCR, REVAB);
	RTW_WBW(regs, RTW_RCR, RTW_RCR);
	RTW_WRITE(regs, RTW_RCR, REVC);

	RTW_WBR(regs, RTW_RCR, RTW_RCR);
	if ((RTW_READ(regs, RTW_RCR) & REVC) == REVC)
		phydelay |= RTW_PHYDELAY_REVC_MAGIC;

	RTW_WRITE(regs, RTW_RCR, rcr0);	/* restore RCR */
	RTW_SYNC(regs, RTW_RCR, RTW_RCR);

	return phydelay;
#undef REVC
}

void
rtw_attach(struct rtw_softc *sc)
{
	struct rtw_txctl_blk *stc;
	int pri, rc;

#if 0
	CASSERT(RTW_DESC_ALIGNMENT % sizeof(struct rtw_txdesc) == 0,
	    "RTW_DESC_ALIGNMENT is not a multiple of "
	    "sizeof(struct rtw_txdesc)");

	CASSERT(RTW_DESC_ALIGNMENT % sizeof(struct rtw_rxdesc) == 0,
	    "RTW_DESC_ALIGNMENT is not a multiple of "
	    "sizeof(struct rtw_rxdesc)");

	CASSERT(RTW_DESC_ALIGNMENT % RTW_MAXPKTSEGS == 0,
	    "RTW_DESC_ALIGNMENT is not a multiple of RTW_MAXPKTSEGS");
#endif

	NEXT_ATTACH_STATE(sc, DETACHED);

	switch (RTW_READ(&sc->sc_regs, RTW_TCR) & RTW_TCR_HWVERID_MASK) {
	case RTW_TCR_HWVERID_F:
		sc->sc_hwverid = 'F';
		break;
	case RTW_TCR_HWVERID_D:
		sc->sc_hwverid = 'D';
		break;
	default:
		sc->sc_hwverid = '?';
		break;
	}
	printf("%s: ver %c ", sc->sc_dev.dv_xname, sc->sc_hwverid);

	rc = bus_dmamem_alloc(sc->sc_dmat, sizeof(struct rtw_descs),
	    RTW_DESC_ALIGNMENT, 0, &sc->sc_desc_segs, 1, &sc->sc_desc_nsegs,
	    0);

	if (rc != 0) {
		printf("%s: could not allocate hw descriptors, error %d\n",
		     sc->sc_dev.dv_xname, rc);
		goto err;
	}

	NEXT_ATTACH_STATE(sc, FINISH_DESC_ALLOC);

	rc = bus_dmamem_map(sc->sc_dmat, &sc->sc_desc_segs,
	    sc->sc_desc_nsegs, sizeof(struct rtw_descs),
	    (caddr_t*)&sc->sc_descs, BUS_DMA_COHERENT);

	if (rc != 0) {
		printf("%s: could not map hw descriptors, error %d\n",
		    sc->sc_dev.dv_xname, rc);
		goto err;
	}
	NEXT_ATTACH_STATE(sc, FINISH_DESC_MAP);

	rc = bus_dmamap_create(sc->sc_dmat, sizeof(struct rtw_descs), 1,
	    sizeof(struct rtw_descs), 0, 0, &sc->sc_desc_dmamap);

	if (rc != 0) {
		printf("%s: could not create DMA map for hw descriptors, "
		    "error %d\n", sc->sc_dev.dv_xname, rc);
		goto err;
	}
	NEXT_ATTACH_STATE(sc, FINISH_DESCMAP_CREATE);

	rc = bus_dmamap_load(sc->sc_dmat, sc->sc_desc_dmamap, sc->sc_descs,
	    sizeof(struct rtw_descs), NULL, 0);

	if (rc != 0) {
		printf("%s: could not load DMA map for hw descriptors, "
		    "error %d\n", sc->sc_dev.dv_xname, rc);
		goto err;
	}
	NEXT_ATTACH_STATE(sc, FINISH_DESCMAP_LOAD);

	if (rtw_txctl_blk_setup_all(sc) != 0)
		goto err;
	NEXT_ATTACH_STATE(sc, FINISH_TXCTLBLK_SETUP);

	rtw_txdesc_blk_setup_all(sc);

	NEXT_ATTACH_STATE(sc, FINISH_TXDESCBLK_SETUP);

	sc->sc_rxdesc = &sc->sc_descs->hd_rx[0];

	rtw_rxctls_setup(&sc->sc_rxctl[0]);

	for (pri = 0; pri < RTW_NTXPRI; pri++) {
		stc = &sc->sc_txctl_blk[pri];

		if ((rc = rtw_txdesc_dmamaps_create(sc->sc_dmat,
		    &stc->stc_desc[0], stc->stc_ndesc)) != 0) {
			printf("%s: could not load DMA map for "
			    "hw tx descriptors, error %d\n",
			    sc->sc_dev.dv_xname, rc);
			goto err;
		}
	}

	NEXT_ATTACH_STATE(sc, FINISH_TXMAPS_CREATE);
	if ((rc = rtw_rxdesc_dmamaps_create(sc->sc_dmat, &sc->sc_rxctl[0],
	                                    RTW_RXQLEN)) != 0) {
		printf("%s: could not load DMA map for hw rx descriptors, "
		    "error %d\n", sc->sc_dev.dv_xname, rc);
		goto err;
	}
	NEXT_ATTACH_STATE(sc, FINISH_RXMAPS_CREATE);

	/* Reset the chip to a known state. */
	if (rtw_reset(sc) != 0)
		goto err;
	NEXT_ATTACH_STATE(sc, FINISH_RESET);

	sc->sc_rcr = RTW_READ(&sc->sc_regs, RTW_RCR);

	if ((sc->sc_rcr & RTW_RCR_9356SEL) != 0)
		sc->sc_flags |= RTW_F_9356SROM;

	if (rtw_srom_read(&sc->sc_regs, sc->sc_flags, &sc->sc_srom,
	    sc->sc_dev.dv_xname) != 0)
		goto err;

	NEXT_ATTACH_STATE(sc, FINISH_READ_SROM);

	if (rtw_srom_parse(&sc->sc_srom, &sc->sc_flags, &sc->sc_csthr,
	    &sc->sc_rfchipid, &sc->sc_rcr, &sc->sc_locale,
	    sc->sc_dev.dv_xname) != 0) {
		printf("%s: attach failed, malformed serial ROM\n",
		    sc->sc_dev.dv_xname);
		goto err;
	}

	RTW_DPRINTF(RTW_DEBUG_ATTACH, ("%s: %s PHY\n", sc->sc_dev.dv_xname,
	    ((sc->sc_flags & RTW_F_DIGPHY) != 0) ? "digital" : "analog"));

	RTW_DPRINTF(RTW_DEBUG_ATTACH, ("%s: CS threshold %u\n",
	    sc->sc_dev.dv_xname, sc->sc_csthr));

	NEXT_ATTACH_STATE(sc, FINISH_PARSE_SROM);

	sc->sc_rf = rtw_rf_attach(sc, sc->sc_rfchipid,
	    sc->sc_flags & RTW_F_DIGPHY);

	if (sc->sc_rf == NULL) {
		printf("%s: attach failed, could not attach RF\n",
		    sc->sc_dev.dv_xname);
		goto err;
	}

#if 0
	if (rtw_identify_rf(&sc->sc_regs, &sc->sc_rftype,
	    sc->sc_dev.dv_xname) != 0) {
		printf("%s: attach failed, unknown RF unidentified\n",
		    sc->sc_dev.dv_xname);
		goto err;
	}
#endif

	NEXT_ATTACH_STATE(sc, FINISH_RF_ATTACH);

	sc->sc_phydelay = rtw_check_phydelay(&sc->sc_regs, sc->sc_rcr);

	RTW_DPRINTF(RTW_DEBUG_ATTACH,
	    ("%s: PHY delay %d\n", sc->sc_dev.dv_xname, sc->sc_phydelay));

	if (sc->sc_locale == RTW_LOCALE_UNKNOWN)
		rtw_identify_country(&sc->sc_regs, &sc->sc_locale,
		    sc->sc_dev.dv_xname);

	rtw_init_channels(sc->sc_locale, &sc->sc_ic.ic_channels,
	    sc->sc_dev.dv_xname);

	if (rtw_identify_sta(&sc->sc_regs, &sc->sc_ic.ic_myaddr,
	    sc->sc_dev.dv_xname) != 0)
		goto err;
	NEXT_ATTACH_STATE(sc, FINISH_ID_STA);

	rtw_setifprops(&sc->sc_if, sc->sc_dev.dv_xname, (void*)sc);

	IFQ_SET_READY(&sc->sc_if.if_snd);

	rtw_set80211props(&sc->sc_ic);

	/*
	 * Call MI attach routines.
	 */
	if_attach(&sc->sc_if);
	ieee80211_ifattach(&sc->sc_if);

	rtw_set80211methods(&sc->sc_mtbl, &sc->sc_ic);

	/* possibly we should fill in our own sc_send_prresp, since
	 * the RTL8180 is probably sending probe responses in ad hoc
	 * mode.
	 */

	/* complete initialization */
	ieee80211_media_init(&sc->sc_if, rtw_media_change, rtw_media_status);
	timeout_set(&sc->sc_scan_to, rtw_next_scan, sc);

#if NBPFILTER > 0
	bpfattach(&sc->sc_radiobpf, &sc->sc_ic.ic_if, DLT_IEEE802_11_RADIO,
	    sizeof(struct ieee80211_frame) + 64);
#endif

	rtw_establish_hooks(&sc->sc_hooks, sc->sc_dev.dv_xname, (void*)sc);

	rtw_init_radiotap(sc);

	NEXT_ATTACH_STATE(sc, FINISHED);

	return;
err:
	rtw_detach(sc);
	return;
}

int
rtw_detach(struct rtw_softc *sc)
{
	int pri;

	switch (sc->sc_attach_state) {
	case FINISHED:
		rtw_stop(&sc->sc_if, 1);

		rtw_disestablish_hooks(&sc->sc_hooks, sc->sc_dev.dv_xname,
		    (void*)sc);
		timeout_del(&sc->sc_scan_to);
		ieee80211_ifdetach(&sc->sc_if);
		if_detach(&sc->sc_if);
		break;
	case FINISH_ID_STA:
	case FINISH_RF_ATTACH:
		rtw_rf_destroy(sc->sc_rf);
		sc->sc_rf = NULL;
		/*FALLTHROUGH*/
	case FINISH_PARSE_SROM:
	case FINISH_READ_SROM:
		rtw_srom_free(&sc->sc_srom);
		/*FALLTHROUGH*/
	case FINISH_RESET:
	case FINISH_RXMAPS_CREATE:
		rtw_rxdesc_dmamaps_destroy(sc->sc_dmat, &sc->sc_rxctl[0],
		    RTW_RXQLEN);
		/*FALLTHROUGH*/
	case FINISH_TXMAPS_CREATE:
		for (pri = 0; pri < RTW_NTXPRI; pri++) {
			rtw_txdesc_dmamaps_destroy(sc->sc_dmat,
			    sc->sc_txctl_blk[pri].stc_desc,
			    sc->sc_txctl_blk[pri].stc_ndesc);
		}
		/*FALLTHROUGH*/
	case FINISH_TXDESCBLK_SETUP:
	case FINISH_TXCTLBLK_SETUP:
		rtw_txctl_blk_cleanup_all(sc);
		/*FALLTHROUGH*/
	case FINISH_DESCMAP_LOAD:
		bus_dmamap_unload(sc->sc_dmat, sc->sc_desc_dmamap);
		/*FALLTHROUGH*/
	case FINISH_DESCMAP_CREATE:
		bus_dmamap_destroy(sc->sc_dmat, sc->sc_desc_dmamap);
		/*FALLTHROUGH*/
	case FINISH_DESC_MAP:
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)sc->sc_descs,
		    sizeof(struct rtw_descs));
		/*FALLTHROUGH*/
	case FINISH_DESC_ALLOC:
		bus_dmamem_free(sc->sc_dmat, &sc->sc_desc_segs,
		    sc->sc_desc_nsegs);
		/*FALLTHROUGH*/
	case DETACHED:
		NEXT_ATTACH_STATE(sc, DETACHED);
		break;
	}
	return 0;
}

void
rtw_rf_destroy(struct rtw_rf *rf)
{
	(*rf->rf_destroy)(rf);
}

int
rtw_rf_init(struct rtw_rf *rf, u_int freq, u_int8_t opaque_txpower,
    enum rtw_pwrstate power)
{
	return (*rf->rf_init)(rf, freq, opaque_txpower, power);
}

int
rtw_rf_pwrstate(struct rtw_rf *rf, enum rtw_pwrstate power)
{
	return (*rf->rf_pwrstate)(rf, power);
}

int
rtw_rf_tune(struct rtw_rf *rf, u_int freq)
{
	return (*rf->rf_tune)(rf, freq);
}

int
rtw_rf_txpower(struct rtw_rf *rf, u_int8_t opaque_txpower)
{
	return (*rf->rf_txpower)(rf, opaque_txpower);
}

int
rtw_rfbus_write(struct rtw_rfbus *bus, enum rtw_rfchipid rfchipid, u_int addr,
    u_int32_t val)
{
	return (*bus->b_write)(bus->b_regs, rfchipid, addr, val);
}
