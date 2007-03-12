/*	$OpenBSD: bcw.c,v 1.69 2007/03/12 06:51:16 mglocker Exp $ */

/*
 * Copyright (c) 2006 Jon Simola <jsimola@gmail.com>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * Broadcom BCM43xx Wireless network chipsets (broadcom.com)
 * SiliconBackplane is technology from Sonics, Inc.(sonicsinc.com)
 */
 
/* standard includes, probably some extras */

#include "bpfilter.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
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
#if NBPFILTER > 0
#include <net/bpf.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_radiotap.h>

#include <dev/pci/pcireg.h>
#include <dev/pci/pcivar.h>
#include <dev/pci/pcidevs.h>

#include <dev/ic/bcwreg.h>
#include <dev/ic/bcwvar.h>

#include <uvm/uvm_extern.h>

/* helper routines */
void		bcw_shm_ctl_word(struct bcw_softc *, uint16_t, uint16_t);
uint16_t	bcw_shm_read16(struct bcw_softc *, uint16_t, uint16_t);
void		bcw_shm_write16(struct bcw_softc *, uint16_t, uint16_t,
		    uint16_t);
void		bcw_radio_write16(struct bcw_softc *, uint16_t, uint16_t);
int		bcw_radio_read16(struct bcw_softc *, uint16_t);
void		bcw_phy_write16(struct bcw_softc *, uint16_t, uint16_t);
int		bcw_phy_read16(struct bcw_softc *, uint16_t);
void		bcw_ram_write(struct bcw_softc *, uint16_t, uint32_t);
int		bcw_lv(int, int, int);

void		bcw_dummy_transmission(struct bcw_softc *);
struct bcw_lopair *
		bcw_get_lopair(struct bcw_softc *sc, uint16_t radio_atten,
		    uint16_t baseband_atten);
void		bcw_stack_save(uint32_t *, size_t *, uint8_t, uint16_t,
		    uint16_t);
uint16_t	bcw_stack_restore(uint32_t *, uint8_t, uint16_t);
int		bcw_using_pio(struct bcw_softc *);

void		bcw_reset(struct bcw_softc *);
int		bcw_init(struct ifnet *);
void		bcw_start(struct ifnet *);
void		bcw_stop(struct ifnet *, int);
void		bcw_watchdog(struct ifnet *);
void		bcw_rxintr(struct bcw_softc *);
void		bcw_txintr(struct bcw_softc *);
//void		bcw_add_mac(struct bcw_softc *, uint8_t *, unsigned long);
int		bcw_add_rxbuf(struct bcw_softc *, int);
void		bcw_rxdrain(struct bcw_softc *);
void		bcw_set_filter(struct ifnet *); 
void		bcw_tick(void *);
int		bcw_ioctl(struct ifnet *, u_long, caddr_t);
int		bcw_alloc_rx_ring(struct bcw_softc *, struct bcw_rx_ring *,
		    int);
void		bcw_reset_rx_ring(struct bcw_softc *, struct bcw_rx_ring *);
void		bcw_free_rx_ring(struct bcw_softc *, struct bcw_rx_ring *);
int		bcw_alloc_tx_ring(struct bcw_softc *, struct bcw_tx_ring *,
		    int);
void		bcw_reset_tx_ring(struct bcw_softc *, struct bcw_tx_ring *);
void		bcw_free_tx_ring(struct bcw_softc *, struct bcw_tx_ring *);
/* 80211 functions copied from iwi */
int		bcw_newstate(struct ieee80211com *, enum ieee80211_state, int);
int		bcw_media_change(struct ifnet *);
void		bcw_media_status(struct ifnet *, struct ifmediareq *);
/* fashionably new functions */
int		bcw_validatechipaccess(struct bcw_softc *);
void		bcw_powercontrol_crystal_off(struct bcw_softc *);
int		bcw_change_core(struct bcw_softc *, int);
int		bcw_reset_core(struct bcw_softc *, uint32_t);
int		bcw_get_firmware(const char *, const uint8_t *, size_t,
		    size_t *, size_t *);
int		bcw_load_firmware(struct bcw_softc *);
int		bcw_write_initvals(struct bcw_softc *,
		    const struct bcw_initval *, const unsigned int);
int		bcw_load_initvals(struct bcw_softc *);
void		bcw_leds_switch_all(struct bcw_softc *, int);
int		bcw_gpio_init(struct bcw_softc *);
/* phy */
int		bcw_phy_init(struct bcw_softc *);
void		bcw_phy_initg(struct bcw_softc *);
void		bcw_phy_initb2(struct bcw_softc *);
void		bcw_phy_initb4(struct bcw_softc *);
void		bcw_phy_initb5(struct bcw_softc *);
void		bcw_phy_initb6(struct bcw_softc *);
void		bcw_phy_inita(struct bcw_softc *);
void		bcw_phy_setupa(struct bcw_softc *);
void		bcw_phy_setupg(struct bcw_softc *);
void		bcw_phy_calc_loopback_gain(struct bcw_softc *);
void		bcw_phy_agcsetup(struct bcw_softc *);
void		bcw_phy_init_pctl(struct bcw_softc *);
void		bcw_phy_init_noisescaletbl(struct bcw_softc *);
void		bcw_phy_set_baseband_atten(struct bcw_softc *, uint16_t);
int8_t		bcw_phy_estimate_powerout(struct bcw_softc *, int8_t tssi);
void		bcw_phy_xmitpower(struct bcw_softc *);
uint16_t	bcw_phy_lo_b_r15_loop(struct bcw_softc *);
void		bcw_phy_lo_b_measure(struct bcw_softc *);
void		bcw_phy_lo_g_state(struct bcw_softc *,  struct bcw_lopair *,
		    struct bcw_lopair *, uint16_t);
void		bcw_phy_lo_g_measure(struct bcw_softc *);
void		bcw_phy_lo_g_measure_txctl2(struct bcw_softc *);
uint32_t	bcw_phy_lo_g_singledeviation(struct bcw_softc *, uint16_t);
uint16_t	bcw_phy_lo_g_deviation_subval(struct bcw_softc *, uint16_t);
void		bcw_phy_lo_adjust(struct bcw_softc *, int fixed);
void		bcw_phy_lo_mark_current_used(struct bcw_softc *);
void		bcw_phy_lo_write(struct bcw_softc *, struct bcw_lopair *);
struct bcw_lopair *
		bcw_phy_find_lopair(struct bcw_softc *, uint16_t, uint16_t,
		    uint16_t);
struct bcw_lopair *
		bcw_phy_current_lopair(struct bcw_softc *);
void		bcw_phy_prepare_init(struct bcw_softc *);
void		bcw_phy_set_antenna_diversity(struct bcw_softc *);
/* radio */
void		bcw_radio_off(struct bcw_softc *);
void		bcw_radio_on(struct bcw_softc *);
void		bcw_radio_nrssi_hw_write(struct bcw_softc *, uint16_t, int16_t);
int16_t		bcw_radio_nrssi_hw_read(struct bcw_softc *, uint16_t);
void		bcw_radio_nrssi_hw_update(struct bcw_softc *, uint16_t);
void		bcw_radio_calc_nrssi_threshold(struct bcw_softc *);
void		bcw_radio_calc_nrssi_slope(struct bcw_softc *);
void		bcw_radio_calc_nrssi_offset(struct bcw_softc *);
void		bcw_radio_set_all_gains(struct bcw_softc *, int16_t, int16_t,
		    int16_t);
void		bcw_radio_set_original_gains(struct bcw_softc *);
uint16_t	bcw_radio_calibrationvalue(struct bcw_softc *);
void		bcw_radio_set_txpower_a(struct bcw_softc *, uint16_t);
void		bcw_radio_set_txpower_bg(struct bcw_softc *, uint16_t, uint16_t,
		    uint16_t);
uint16_t	bcw_radio_init2050(struct bcw_softc *);
void		bcw_radio_init2060(struct bcw_softc *);
void		bcw_radio_spw(struct bcw_softc *, uint8_t);
int		bcw_radio_select_channel(struct bcw_softc *, uint8_t, int );
uint16_t	bcw_radio_chan2freq_a(uint8_t);
uint16_t	bcw_radio_chan2freq_bg(uint8_t);
uint16_t	bcw_radio_default_baseband_attenuation(struct bcw_softc *);
uint16_t	bcw_radio_default_radio_attenuation(struct bcw_softc *);
uint16_t	bcw_radio_default_txctl1(struct bcw_softc *);
void		bcw_radio_clear_tssi(struct bcw_softc *);
void		bcw_radio_set_tx_iq(struct bcw_softc *);
uint16_t	bcw_radio_get_txgain_baseband(uint16_t);
uint16_t	bcw_radio_get_txgain_freq_power_amp(uint16_t);
uint16_t	bcw_radio_get_txgain_dac(uint16_t);
uint16_t	bcw_radio_freq_r3a_value(uint16_t);
void		bcw_radio_prepare_init(struct bcw_softc *);
int		bcw_radio_set_interference_mitigation(struct bcw_softc *, int);
int		bcw_radio_interference_mitigation_enable(struct bcw_softc *,
		    int);
void		bcw_radio_set_txantenna(struct bcw_softc *, uint32_t);
/* ilt */
void		bcw_ilt_write(struct bcw_softc *, uint16_t, uint16_t);
uint16_t	bcw_ilt_read(struct bcw_softc *, uint16_t);

struct cfdriver bcw_cd = {
	NULL, "bcw", DV_IFNET
};

#define BCW_PHY_STACKSAVE(offset)				\
	do {							\
		bcw_stack_save(stack, &stackidx, 0x1, (offset),	\
		    bcw_phy_read16(sc, (offset)));		\
	} while (0)
#define BCW_ILT_STACKSAVE(offset)				\
	do {							\
		bcw_stack_save(stack, &stackidx, 0x3, (offset), \
		    bcw_ilt_read(sc, (offset)));		\
	} while (0)

/*
 * Table for bcw_radio_calibrationvalue()
 */
const uint16_t rcc_table[16] = {
	0x0002, 0x0003, 0x0001, 0x000F,
	0x0006, 0x0007, 0x0005, 0x000F,
	0x000A, 0x000B, 0x0009, 0x000F,
	0x000E, 0x000F, 0x000D, 0x000F,
};

/*
 * ILT (Internal Lookup Table)
 */
const uint32_t bcw_ilt_rotor[BCW_ILT_ROTOR_SIZE] = {
	0xFEB93FFD, 0xFEC63FFD, /* 0 */
	0xFED23FFD, 0xFEDF3FFD,
	0xFEEC3FFE, 0xFEF83FFE,
	0xFF053FFE, 0xFF113FFE,
	0xFF1E3FFE, 0xFF2A3FFF, /* 8 */
	0xFF373FFF, 0xFF443FFF,
	0xFF503FFF, 0xFF5D3FFF,
	0xFF693FFF, 0xFF763FFF,
	0xFF824000, 0xFF8F4000, /* 16 */
	0xFF9B4000, 0xFFA84000,
	0xFFB54000, 0xFFC14000,
	0xFFCE4000, 0xFFDA4000,
	0xFFE74000, 0xFFF34000, /* 24 */
	0x00004000, 0x000D4000,
	0x00194000, 0x00264000,
	0x00324000, 0x003F4000,
	0x004B4000, 0x00584000, /* 32 */
	0x00654000, 0x00714000,
	0x007E4000, 0x008A3FFF,
	0x00973FFF, 0x00A33FFF,
	0x00B03FFF, 0x00BC3FFF, /* 40 */
	0x00C93FFF, 0x00D63FFF,
	0x00E23FFE, 0x00EF3FFE,
	0x00FB3FFE, 0x01083FFE,
	0x01143FFE, 0x01213FFD, /* 48 */
	0x012E3FFD, 0x013A3FFD,
	0x01473FFD,
};

const uint16_t bcw_ilt_sigmasqr1[BCW_ILT_SIGMASQR_SIZE] = {
	0x007A, 0x0075, 0x0071, 0x006C, /* 0 */
	0x0067, 0x0063, 0x005E, 0x0059,
	0x0054, 0x0050, 0x004B, 0x0046,
	0x0042, 0x003D, 0x003D, 0x003D,
	0x003D, 0x003D, 0x003D, 0x003D, /* 16 */
	0x003D, 0x003D, 0x003D, 0x003D,
	0x003D, 0x003D, 0x0000, 0x003D,
	0x003D, 0x003D, 0x003D, 0x003D,
	0x003D, 0x003D, 0x003D, 0x003D, /* 32 */
	0x003D, 0x003D, 0x003D, 0x003D,
	0x0042, 0x0046, 0x004B, 0x0050,
	0x0054, 0x0059, 0x005E, 0x0063,
	0x0067, 0x006C, 0x0071, 0x0075, /* 48 */
	0x007A,
};

const uint16_t bcw_ilt_sigmasqr2[BCW_ILT_SIGMASQR_SIZE] = {
	0x00DE, 0x00DC, 0x00DA, 0x00D8, /* 0 */
	0x00D6, 0x00D4, 0x00D2, 0x00CF,
	0x00CD, 0x00CA, 0x00C7, 0x00C4,
	0x00C1, 0x00BE, 0x00BE, 0x00BE,
	0x00BE, 0x00BE, 0x00BE, 0x00BE, /* 16 */
	0x00BE, 0x00BE, 0x00BE, 0x00BE,
	0x00BE, 0x00BE, 0x0000, 0x00BE,
	0x00BE, 0x00BE, 0x00BE, 0x00BE,
	0x00BE, 0x00BE, 0x00BE, 0x00BE, /* 32 */
	0x00BE, 0x00BE, 0x00BE, 0x00BE,
	0x00C1, 0x00C4, 0x00C7, 0x00CA,
	0x00CD, 0x00CF, 0x00D2, 0x00D4,
	0x00D6, 0x00D8, 0x00DA, 0x00DC, /* 48 */
	0x00DE,
};

const uint32_t bcw_ilt_retard[BCW_ILT_RETARD_SIZE] = {
	0xDB93CB87, 0xD666CF64, /* 0 */
	0xD1FDD358, 0xCDA6D826,
	0xCA38DD9F, 0xC729E2B4,
	0xC469E88E, 0xC26AEE2B,
	0xC0DEF46C, 0xC073FA62, /* 8 */
	0xC01D00D5, 0xC0760743,
	0xC1560D1E, 0xC2E51369,
	0xC4ED18FF, 0xC7AC1ED7,
	0xCB2823B2, 0xCEFA28D9, /* 16 */
	0xD2F62D3F, 0xD7BB3197,
	0xDCE53568, 0xE1FE3875,
	0xE7D13B35, 0xED663D35,
	0xF39B3EC4, 0xF98E3FA7, /* 24 */
	0x00004000, 0x06723FA7,
	0x0C653EC4, 0x129A3D35,
	0x182F3B35, 0x1E023875,
	0x231B3568, 0x28453197, /* 32 */
	0x2D0A2D3F, 0x310628D9,
	0x34D823B2, 0x38541ED7,
	0x3B1318FF, 0x3D1B1369,
	0x3EAA0D1E, 0x3F8A0743, /* 40 */
	0x3FE300D5, 0x3F8DFA62,
	0x3F22F46C, 0x3D96EE2B,
	0x3B97E88E, 0x38D7E2B4,
	0x35C8DD9F, 0x325AD826, /* 48 */
	0x2E03D358, 0x299ACF64,
	0x246DCB87,
};

const uint16_t bcw_ilt_finefreqa[BCW_ILT_FINEFREQA_SIZE] = {
	0x0082, 0x0082, 0x0102, 0x0182, /* 0 */
	0x0202, 0x0282, 0x0302, 0x0382,
	0x0402, 0x0482, 0x0502, 0x0582,
	0x05E2, 0x0662, 0x06E2, 0x0762,
	0x07E2, 0x0842, 0x08C2, 0x0942, /* 16 */
	0x09C2, 0x0A22, 0x0AA2, 0x0B02,
	0x0B82, 0x0BE2, 0x0C62, 0x0CC2,
	0x0D42, 0x0DA2, 0x0E02, 0x0E62,
	0x0EE2, 0x0F42, 0x0FA2, 0x1002, /* 32 */
	0x1062, 0x10C2, 0x1122, 0x1182,
	0x11E2, 0x1242, 0x12A2, 0x12E2,
	0x1342, 0x13A2, 0x1402, 0x1442,
	0x14A2, 0x14E2, 0x1542, 0x1582, /* 48 */
	0x15E2, 0x1622, 0x1662, 0x16C1,
	0x1701, 0x1741, 0x1781, 0x17E1,
	0x1821, 0x1861, 0x18A1, 0x18E1,
	0x1921, 0x1961, 0x19A1, 0x19E1, /* 64 */
	0x1A21, 0x1A61, 0x1AA1, 0x1AC1,
	0x1B01, 0x1B41, 0x1B81, 0x1BA1,
	0x1BE1, 0x1C21, 0x1C41, 0x1C81,
	0x1CA1, 0x1CE1, 0x1D01, 0x1D41, /* 80 */
	0x1D61, 0x1DA1, 0x1DC1, 0x1E01,
	0x1E21, 0x1E61, 0x1E81, 0x1EA1,
	0x1EE1, 0x1F01, 0x1F21, 0x1F41,
	0x1F81, 0x1FA1, 0x1FC1, 0x1FE1, /* 96 */
	0x2001, 0x2041, 0x2061, 0x2081,
	0x20A1, 0x20C1, 0x20E1, 0x2101,
	0x2121, 0x2141, 0x2161, 0x2181,
	0x21A1, 0x21C1, 0x21E1, 0x2201, /* 112 */
	0x2221, 0x2241, 0x2261, 0x2281,
	0x22A1, 0x22C1, 0x22C1, 0x22E1,
	0x2301, 0x2321, 0x2341, 0x2361,
	0x2361, 0x2381, 0x23A1, 0x23C1, /* 128 */
	0x23E1, 0x23E1, 0x2401, 0x2421,
	0x2441, 0x2441, 0x2461, 0x2481,
	0x2481, 0x24A1, 0x24C1, 0x24C1,
	0x24E1, 0x2501, 0x2501, 0x2521, /* 144 */
	0x2541, 0x2541, 0x2561, 0x2561,
	0x2581, 0x25A1, 0x25A1, 0x25C1,
	0x25C1, 0x25E1, 0x2601, 0x2601,
	0x2621, 0x2621, 0x2641, 0x2641, /* 160 */
	0x2661, 0x2661, 0x2681, 0x2681,
	0x26A1, 0x26A1, 0x26C1, 0x26C1,
	0x26E1, 0x26E1, 0x2701, 0x2701,
	0x2721, 0x2721, 0x2740, 0x2740, /* 176 */
	0x2760, 0x2760, 0x2780, 0x2780,
	0x2780, 0x27A0, 0x27A0, 0x27C0,
	0x27C0, 0x27E0, 0x27E0, 0x27E0,
	0x2800, 0x2800, 0x2820, 0x2820, /* 192 */
	0x2820, 0x2840, 0x2840, 0x2840,
	0x2860, 0x2860, 0x2880, 0x2880,
	0x2880, 0x28A0, 0x28A0, 0x28A0,
	0x28C0, 0x28C0, 0x28C0, 0x28E0, /* 208 */
	0x28E0, 0x28E0, 0x2900, 0x2900,
	0x2900, 0x2920, 0x2920, 0x2920,
	0x2940, 0x2940, 0x2940, 0x2960,
	0x2960, 0x2960, 0x2960, 0x2980, /* 224 */
	0x2980, 0x2980, 0x29A0, 0x29A0,
	0x29A0, 0x29A0, 0x29C0, 0x29C0,
	0x29C0, 0x29E0, 0x29E0, 0x29E0,
	0x29E0, 0x2A00, 0x2A00, 0x2A00, /* 240 */
	0x2A00, 0x2A20, 0x2A20, 0x2A20,
	0x2A20, 0x2A40, 0x2A40, 0x2A40,
	0x2A40, 0x2A60, 0x2A60, 0x2A60,
};

const uint16_t bcw_ilt_noisea2[BCW_ILT_NOISEA2_SIZE] = {
	0x0001, 0x0001, 0x0001, 0xFFFE,
	0xFFFE, 0x3FFF, 0x1000, 0x0393,
};

const uint16_t bcw_ilt_noisea3[BCW_ILT_NOISEA3_SIZE] = {
	0x4C4C, 0x4C4C, 0x4C4C, 0x2D36,
	0x4C4C, 0x4C4C, 0x4C4C, 0x2D36,
};

const uint16_t bcw_ilt_finefreqg[BCW_ILT_FINEFREQG_SIZE] = {
        0x0089, 0x02E9, 0x0409, 0x04E9, /* 0 */
        0x05A9, 0x0669, 0x0709, 0x0789,
        0x0829, 0x08A9, 0x0929, 0x0989,
        0x0A09, 0x0A69, 0x0AC9, 0x0B29,
        0x0BA9, 0x0BE9, 0x0C49, 0x0CA9, /* 16 */
        0x0D09, 0x0D69, 0x0DA9, 0x0E09,
        0x0E69, 0x0EA9, 0x0F09, 0x0F49,
        0x0FA9, 0x0FE9, 0x1029, 0x1089,
        0x10C9, 0x1109, 0x1169, 0x11A9, /* 32 */
        0x11E9, 0x1229, 0x1289, 0x12C9,
        0x1309, 0x1349, 0x1389, 0x13C9,
        0x1409, 0x1449, 0x14A9, 0x14E9,
        0x1529, 0x1569, 0x15A9, 0x15E9, /* 48 */
        0x1629, 0x1669, 0x16A9, 0x16E8,
        0x1728, 0x1768, 0x17A8, 0x17E8,
        0x1828, 0x1868, 0x18A8, 0x18E8,
        0x1928, 0x1968, 0x19A8, 0x19E8, /* 64 */
        0x1A28, 0x1A68, 0x1AA8, 0x1AE8,
        0x1B28, 0x1B68, 0x1BA8, 0x1BE8,
        0x1C28, 0x1C68, 0x1CA8, 0x1CE8,
        0x1D28, 0x1D68, 0x1DC8, 0x1E08, /* 80 */
        0x1E48, 0x1E88, 0x1EC8, 0x1F08,
        0x1F48, 0x1F88, 0x1FE8, 0x2028,
        0x2068, 0x20A8, 0x2108, 0x2148,
        0x2188, 0x21C8, 0x2228, 0x2268, /* 96 */
        0x22C8, 0x2308, 0x2348, 0x23A8,
        0x23E8, 0x2448, 0x24A8, 0x24E8,
        0x2548, 0x25A8, 0x2608, 0x2668,
        0x26C8, 0x2728, 0x2787, 0x27E7, /* 112 */
        0x2847, 0x28C7, 0x2947, 0x29A7,
        0x2A27, 0x2AC7, 0x2B47, 0x2BE7,
        0x2CA7, 0x2D67, 0x2E47, 0x2F67,
        0x3247, 0x3526, 0x3646, 0x3726, /* 128 */
        0x3806, 0x38A6, 0x3946, 0x39E6,
        0x3A66, 0x3AE6, 0x3B66, 0x3BC6,
        0x3C45, 0x3CA5, 0x3D05, 0x3D85,
        0x3DE5, 0x3E45, 0x3EA5, 0x3EE5, /* 144 */
        0x3F45, 0x3FA5, 0x4005, 0x4045,
        0x40A5, 0x40E5, 0x4145, 0x4185,
        0x41E5, 0x4225, 0x4265, 0x42C5,
        0x4305, 0x4345, 0x43A5, 0x43E5, /* 160 */
        0x4424, 0x4464, 0x44C4, 0x4504,
        0x4544, 0x4584, 0x45C4, 0x4604,
        0x4644, 0x46A4, 0x46E4, 0x4724,
        0x4764, 0x47A4, 0x47E4, 0x4824, /* 176 */
        0x4864, 0x48A4, 0x48E4, 0x4924,
        0x4964, 0x49A4, 0x49E4, 0x4A24,
        0x4A64, 0x4AA4, 0x4AE4, 0x4B23,
        0x4B63, 0x4BA3, 0x4BE3, 0x4C23, /* 192 */
        0x4C63, 0x4CA3, 0x4CE3, 0x4D23,
        0x4D63, 0x4DA3, 0x4DE3, 0x4E23,
        0x4E63, 0x4EA3, 0x4EE3, 0x4F23,
        0x4F63, 0x4FC3, 0x5003, 0x5043, /* 208 */
        0x5083, 0x50C3, 0x5103, 0x5143,
        0x5183, 0x51E2, 0x5222, 0x5262,
        0x52A2, 0x52E2, 0x5342, 0x5382,
        0x53C2, 0x5402, 0x5462, 0x54A2, /* 224 */
        0x5502, 0x5542, 0x55A2, 0x55E2,
        0x5642, 0x5682, 0x56E2, 0x5722,
        0x5782, 0x57E1, 0x5841, 0x58A1,
        0x5901, 0x5961, 0x59C1, 0x5A21, /* 240 */
        0x5AA1, 0x5B01, 0x5B81, 0x5BE1,
        0x5C61, 0x5D01, 0x5D80, 0x5E20,
        0x5EE0, 0x5FA0, 0x6080, 0x61C0,
};

const uint16_t bcw_ilt_noiseg1[BCW_ILT_NOISEG1_SIZE] = {
        0x013C, 0x01F5, 0x031A, 0x0631,
        0x0001, 0x0001, 0x0001, 0x0001,
};

const uint16_t bcw_ilt_noiseg2[BCW_ILT_NOISEG2_SIZE] = {
        0x5484, 0x3C40, 0x0000, 0x0000,
        0x0000, 0x0000, 0x0000, 0x0000,
};

const uint16_t bcw_ilt_noisescaleg1[BCW_ILT_NOISESCALEG_SIZE] = {
        0x6C77, 0x5162, 0x3B40, 0x3335, /* 0 */
        0x2F2D, 0x2A2A, 0x2527, 0x1F21,
        0x1A1D, 0x1719, 0x1616, 0x1414,
        0x1414, 0x1400, 0x1414, 0x1614,
        0x1716, 0x1A19, 0x1F1D, 0x2521, /* 16 */
        0x2A27, 0x2F2A, 0x332D, 0x3B35,
        0x5140, 0x6C62, 0x0077,
};

const uint16_t bcw_ilt_noisescaleg3[BCW_ILT_NOISESCALEG_SIZE] = {
        0xA4A4, 0xA4A4, 0xA4A4, 0xA4A4, /* 0 */
        0xA4A4, 0xA4A4, 0xA4A4, 0xA4A4,
        0xA4A4, 0xA4A4, 0xA4A4, 0xA4A4,
        0xA4A4, 0xA400, 0xA4A4, 0xA4A4,
        0xA4A4, 0xA4A4, 0xA4A4, 0xA4A4, /* 16 */
        0xA4A4, 0xA4A4, 0xA4A4, 0xA4A4,
        0xA4A4, 0xA4A4, 0x00A4,
};

const uint16_t bcw_ilt_noisescaleg2[BCW_ILT_NOISESCALEG_SIZE] = {
        0xD8DD, 0xCBD4, 0xBCC0, 0XB6B7, /* 0 */
        0xB2B0, 0xADAD, 0xA7A9, 0x9FA1,
        0x969B, 0x9195, 0x8F8F, 0x8A8A,
        0x8A8A, 0x8A00, 0x8A8A, 0x8F8A,
        0x918F, 0x9695, 0x9F9B, 0xA7A1, /* 16 */
        0xADA9, 0xB2AD, 0xB6B0, 0xBCB7,
        0xCBC0, 0xD8D4, 0x00DD,
};

/*
 * Helper routines
 */
void
bcw_shm_ctl_word(struct bcw_softc *sc, uint16_t routing, uint16_t offset)
{
	uint32_t control;

	control = routing;
	control <<= 16;
	control |= offset;

	BCW_WRITE(sc, BCW_SHM_CONTROL, control);
}

uint16_t
bcw_shm_read16(struct bcw_softc *sc, uint16_t routing, uint16_t offset)
{
	if (routing == BCW_SHM_CONTROL_SHARED) {
		if (offset & 0x0003) {
			bcw_shm_ctl_word(sc, routing, offset >> 2);

			return (BCW_READ16(sc, BCW_SHM_DATAHIGH));
		}
		offset >>= 2;
	}
	bcw_shm_ctl_word(sc, routing, offset);

	return (BCW_READ16(sc, BCW_SHM_DATA));
}

void
bcw_shm_write16(struct bcw_softc *sc, uint16_t routing, uint16_t offset,
    uint16_t val)
{
	if (routing == BCW_SHM_CONTROL_SHARED) {
		if (offset & 0x0003) {
			bcw_shm_ctl_word(sc, routing, offset >> 2);
			BCW_WRITE16(sc, BCW_SHM_DATAHIGH, val);
			return;
		}
		offset >>= 2;
	}
	bcw_shm_ctl_word(sc, routing, offset);
	BCW_WRITE16(sc, BCW_SHM_DATA, val);
}

void
bcw_radio_write16(struct bcw_softc *sc, uint16_t offset, uint16_t val)
{
	BCW_WRITE16(sc, BCW_RADIO_CONTROL, offset);
	BCW_WRITE16(sc, BCW_RADIO_DATALOW, val);
}

int
bcw_radio_read16(struct bcw_softc *sc, uint16_t offset)
{
	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		offset |= 0x0040;
		break;
	case BCW_PHY_TYPEB:
		if (sc->sc_radio_ver == 0x2053) {
			if (offset < 0x70)
				offset += 0x80;
			else if (offset < 0x80)
				offset += 0x70;
		} else if (sc->sc_radio_ver == 0x2050)
			offset |= 0x80;
		else
			return (0);
		break;
	case BCW_PHY_TYPEG:
		offset |= 0x80;
		break;
	}

	BCW_WRITE16(sc, BCW_RADIO_CONTROL, offset);

	return (BCW_READ16(sc, BCW_RADIO_DATALOW));
}

void
bcw_phy_write16(struct bcw_softc *sc, uint16_t offset, uint16_t val)
{
	BCW_WRITE16(sc, BCW_PHY_CONTROL, offset);
	BCW_WRITE16(sc, BCW_PHY_DATA, val);
}

int
bcw_phy_read16(struct bcw_softc *sc, uint16_t offset)
{
	BCW_WRITE16(sc, BCW_PHY_CONTROL, offset);

	return (BCW_READ16(sc, BCW_PHY_DATA));
}

void
bcw_ram_write(struct bcw_softc *sc, uint16_t offset, uint32_t val)
{
	uint32_t status;

	status = BCW_READ(sc, BCW_SBF);
	if (!(status & BCW_SBF_REGISTER_BYTESWAP))
		val = htobe32(val); /* XXX swab32() */

	BCW_WRITE(sc, BCW_MMIO_RAM_CONTROL, offset);
	BCW_WRITE(sc, BCW_MMIO_RAM_DATA, val);
}

int
bcw_lv(int number, int min, int max)
{
	if (number < min)
		return (min);
	else if (number > max)
		return (max);
	else
		return (number);
}

void
bcw_dummy_transmission(struct bcw_softc *sc)
{
	unsigned int i, max_loop;
	uint16_t val = 0;
	uint32_t buffer[5] = {
	    0x00000000,
	    0x00000400,
	    0x00000000,
	    0x00000001,
	    0x00000000 };

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		max_loop = 0x1e;
		buffer[0] = 0xcc010200;
		break;
	case BCW_PHY_TYPEB:
	case BCW_PHY_TYPEG:
		max_loop = 0xfa;
		buffer[0] = 0x6e840b00;
		break;
	default:
		/* XXX assert? */
		return;
	}

	for (i = 0; i < 5; i++)
		bcw_ram_write(sc, i * 4, buffer[i]);

	BCW_READ(sc, BCW_SBF);

	BCW_WRITE16(sc, 0x0568, 0x0000);
	BCW_WRITE16(sc, 0x07c0, 0x0000);
	BCW_WRITE16(sc, 0x050c, ((sc->sc_phy_type == BCW_PHY_TYPEA) ? 1 : 0));

	BCW_WRITE16(sc, 0x0508, 0x0000);
	BCW_WRITE16(sc, 0x050a, 0x0000);
	BCW_WRITE16(sc, 0x054c, 0x0000);
	BCW_WRITE16(sc, 0x056a, 0x0014);
	BCW_WRITE16(sc, 0x0568, 0x0826);
	BCW_WRITE16(sc, 0x0500, 0x0000);
	BCW_WRITE16(sc, 0x0502, 0x0030);

	if (sc->sc_radio_ver == 0x2050 && sc->sc_radio_rev <= 0x5)
		bcw_radio_write16(sc, 0x0051, 0x0017);
	for (i = 0; i < max_loop; i++) {
		val = BCW_READ16(sc, 0x050e);
		if (val & 0x0080)
			break;
		delay(10);
	}
	for (i = 0; i < 0x0a; i++) {
		val = BCW_READ16(sc, 0x050e);
		if (val & 0x0400)
			break;
		delay(10);
	}
	for (i = 0; i < 0x0a; i++) {
		val = BCW_READ16(sc, 0x0690);
		if (!(val & 0x0100))
			break;
		delay(10);
	}
	if (sc->sc_radio_ver == 0x2050 && sc->sc_radio_rev <= 0x5)
		bcw_radio_write16(sc, 0x0051, 0x0037);
}

struct bcw_lopair *
bcw_get_lopair(struct bcw_softc *sc, uint16_t radio_atten,
    uint16_t baseband_atten)
{
	return (sc->sc_phy_lopairs + (radio_atten + 14 *
	    (baseband_atten / 2)));
}

void
bcw_stack_save(uint32_t *_stackptr, size_t *stackidx, uint8_t id,
    uint16_t offset, uint16_t val)
{
	uint32_t *stackptr = &(_stackptr[*stackidx]);

	/* XXX assert() */

	*stackptr = offset;
	*stackptr |= ((uint32_t)id) << 12;
	*stackptr |= ((uint32_t)val) << 16;
	(*stackidx)++;

	/* XXX assert() */
}

uint16_t
bcw_stack_restore(uint32_t *stackptr, uint8_t id, uint16_t offset)
{
	size_t i;

	/* XXX assert() */

	for (i = 0; i < BCW_INTERFSTACK_SIZE; i++, stackptr++) {
		if ((*stackptr & 0x00000fff) != offset)
			continue;
		if (((*stackptr & 0x0000f000) >> 12) != id)
			continue;
		return (((*stackptr & 0xffff0000) >> 16));
	}

	/* XXX assert() */

	return (0);
}

int
bcw_using_pio(struct bcw_softc *sc)
{
	return (sc->sc_using_pio);
}

void
bcw_attach(struct bcw_softc *sc)
{
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;
	int error;
	int i;
	uint32_t sbval;
	uint32_t core_id, core_rev, core_vendor;

	/* power on cardbus socket */
	if (sc->sc_enable)
		sc->sc_enable(sc);

	DPRINTF(("\n%s: BoardVendor=0x%x, BoardType=0x%x, BoardRev=0x%x\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_board_vendor, sc->sc_board_type, sc->sc_board_rev));

	/*
	 * Don't reset the chip here, we can only reset each core and we
	 * haven't identified the cores yet.
	 */
	//bcw_reset(sc);

	/*
	 * Attach to the Backplane and start the card up
	 */

	/*
	 * Get a copy of the BoardFlags and fix for broken boards.
	 * This needs to be done as soon as possible to determine if the
	 * board supports power control settings. If so, the board has to
	 * be powered on and the clock started. This may even need to go
	 * before the initial chip reset above.
	 */
	sc->sc_boardflags = BCW_READ16(sc, BCW_SPROM_BOARDFLAGS);

	/*
	 * Dell, Product ID 0x4301 Revision 0x74, set BCW_BF_BTCOEXIST
	 * Apple Board Type 0x4e Revision > 0x40, set BCW_BF_PACTRL
	 */

	/*
	 * Should just about everything below here be moved to external files
	 * to keep this file sane? The BCM43xx chips have so many exceptions
	 * based on the version of the chip, the radio, cores and phys that
	 * it would be a huge mess to inline it all here. See the 100 lines
	 * below for an example of just figuring out what the chip id is and
	 * how many cores it has.
	 */

	/*
	 * Try and change to the ChipCommon Core
	 */
	if (bcw_change_core(sc, 0) == 0)
		DPRINTF(("%s: Selected ChipCommon Core\n",
		    sc->sc_dev.dv_xname));

	/*
	 * Core ID REG, this is either the default wireless core (0x812) or
	 * a ChipCommon core that was successfully selected above
	 */
	sbval = BCW_READ(sc, BCW_CIR_SBID_HI);
	core_id = (sbval & 0x8ff0) >> 4;
	core_rev = (sbval & 0x7000) >> 8;
	core_rev |= (sbval & 0xf);
	core_vendor = (sbval & 0xffff0000) >> 16;
	DPRINTF(("%s: CoreID=0x%x, CoreRev=0x%x, CoreVendor=0x%x\n",
	    sc->sc_dev.dv_xname, core_id, core_rev, core_vendor));

	/*
	 * If we successfully got a commoncore, and the corerev=4 or >=6
	 * get the number of cores from the chipid reg
	 */
	if (core_id == BCW_CORE_COMMON) {
		sc->sc_havecommon = 1;

		/* XXX do early init of sc_core[0] here */

		sbval = BCW_READ(sc, BCW_CORE_COMMON_CHIPID);
		sc->sc_chip_id = (sbval & 0x0000ffff);
		sc->sc_chip_rev = (sbval & 0x000f0000) >> 16;
		sc->sc_chip_package = (sbval & 0x00f00000) >> 20;

		if (core_rev >= 4)
			sc->sc_numcores = (sbval & 0x0f000000) >> 24;
		else {
			switch (sc->sc_chip_id) {
			case 0x4710:
			case 0x4610:
			case 0x4704:
				sc->sc_numcores = 9;
				break;
			case 0x4310:
				sc->sc_numcores = 8;
				break;
			case 0x5365:
				sc->sc_numcores = 7;
				break;
			case 0x4306:
				sc->sc_numcores = 6;
				break;
			case 0x4307:
			case 0x4301:
				sc->sc_numcores = 5;
				break;
			case 0x4402:
				sc->sc_numcores = 3;
				break;
			default:
				/* set to max */
				sc->sc_numcores = BCW_MAX_CORES;
			}
		}
	} else { /* No CommonCore, set chipid, cores, rev based on product id */
		sc->sc_core_common = NULL;
		sc->sc_havecommon = 0;
		switch (sc->sc_prodid) {
		case 0x4710:
		case 0x4711:
		case 0x4712:
		case 0x4713:
		case 0x4714:
		case 0x4715:
			sc->sc_chip_id = 0x4710;
			sc->sc_numcores = 9;
			break;
		case 0x4610:
		case 0x4611:
		case 0x4612:
		case 0x4613:
		case 0x4614:
		case 0x4615:
			sc->sc_chip_id = 0x4610;
			sc->sc_numcores = 9;
			break;
		case 0x4402:
		case 0x4403:
			sc->sc_chip_id = 0x4402;
			sc->sc_numcores = 3;
			break;
		case 0x4305:
		case 0x4306:
		case 0x4307:
			sc->sc_chip_id = 0x4307;
			sc->sc_numcores = 5;
			break;
		case 0x4301:
			sc->sc_chip_id = 0x4301;
			sc->sc_numcores = 5;
			break;
		default:
			sc->sc_chip_id = sc->sc_prodid;
			/* Set to max */
			sc->sc_numcores = BCW_MAX_CORES;
		}
	}

	DPRINTF(("%s: ChipID=0x%x, ChipRev=0x%x, ChipPkg=0x%x, NumCores=%d\n",
	    sc->sc_dev.dv_xname,
	    sc->sc_chip_id, sc->sc_chip_rev, sc->sc_chip_package,
	    sc->sc_numcores));

       /* Reset and Identify each core */
       for (i = 0; i < sc->sc_numcores; i++) {
		if (bcw_change_core(sc, i) == 0) {
			sbval = BCW_READ(sc, BCW_CIR_SBID_HI);

			sc->sc_core[i].id = (sbval & 0x00008ff0) >> 4;
			sc->sc_core[i].rev =
			    ((sbval & 0x00007000) >> 8 | (sbval & 0x0000000f));

			switch (sc->sc_core[i].id) {
			case BCW_CORE_COMMON:
				bcw_reset_core(sc, 0);
				sc->sc_core_common = &sc->sc_core[i];
				break;
			case BCW_CORE_PCI:
#if 0
				bcw_reset_core(sc,0);
				(sc->sc_ca == NULL)
#endif
				sc->sc_core_bus = &sc->sc_core[i];
				break;
#if 0
			case BCW_CORE_PCMCIA:
				bcw_reset_core(sc,0);
				if (sc->sc_pa == NULL)
					sc->sc_core_bus = &sc->sc_core[i];
				break;
#endif
			case BCW_CORE_80211:
				bcw_reset_core(sc,
				    SBTML_80211FLAG | SBTML_80211PHY);
				sc->sc_core_80211 = &sc->sc_core[i];
				break;
			case BCW_CORE_NONEXIST:
				sc->sc_numcores = i + 1;
				break;
			default:
				/* Ignore all other core types */
				break;
			}
			DPRINTF(("%s: core %d is type 0x%x rev %d\n",
			    sc->sc_dev.dv_xname, i, 
			    sc->sc_core[i].id, sc->sc_core[i].rev));
			/* XXX Fill out the core location vars */
			sbval = BCW_READ(sc, BCW_SBTPSFLAG);
			sc->sc_core[i].backplane_flag =
			sbval & SBTPS_BACKPLANEFLAGMASK;
			sc->sc_core[i].num = i;
		} else
			DPRINTF(("%s: Failed change to core %d",
			    sc->sc_dev.dv_xname, i));
	} /* End of For loop */

	/* Now that we have cores identified, finish the reset */
	bcw_reset(sc);

	/*
	 * XXX Select the 802.11 core, then
	 * Get and display the PHY info from the MIMO
	 * This probably won't work for cards with multiple radio cores, as
	 * the spec suggests that there is one PHY for each core
	 */
	bcw_change_core(sc, sc->sc_core_80211->num);

	sbval = BCW_READ16(sc, 0x3E0);
	sc->sc_phy_ver = (sbval & 0xf000) >> 12;
	sc->sc_phy_rev = sbval & 0xf;
	sc->sc_phy_type = (sbval & 0xf00) >> 8;

	DPRINTF(("%s: PHY version %d revision %d ",
	    sc->sc_dev.dv_xname, sc->sc_phy_ver, sc->sc_phy_rev));

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		DPRINTF(("PHY %d (A)\n", sc->sc_phy_type));
		break;
	case BCW_PHY_TYPEB:
		DPRINTF(("PHY %d (B)\n", sc->sc_phy_type));
		break;
	case BCW_PHY_TYPEG:
		DPRINTF(("PHY %d (G)\n", sc->sc_phy_type));
		break;
	case BCW_PHY_TYPEN:
		DPRINTF(("PHY %d (N)\n", sc->sc_phy_type));
		break;
	default:
		DPRINTF(("Unrecognizeable PHY type %d\n",
		    sc->sc_phy_type));
		break;
	}

	/*
	 * Initialize softc vars
	 */
	sc->sc_phy_lopairs = malloc(sizeof(struct bcw_lopair) * BCW_LO_COUNT,
	    M_DEVBUF, M_NOWAIT);
	bcw_phy_prepare_init(sc);
	bcw_radio_prepare_init(sc);

	/*
	 * Query the RadioID register, on a 4317 use a lookup instead
	 * XXX Different PHYs have different radio register layouts, so
	 * a wrapper func should be written.
	 * Getting the RadioID is the only 32bit operation done with the
	 * Radio registers, and requires seperate 16bit reads from the low
	 * and the high data addresses.
	 */
	if (sc->sc_chip_id != 0x4317) {
		BCW_WRITE16(sc, BCW_RADIO_CONTROL, BCW_RADIO_ID);
		sbval = BCW_READ16(sc, BCW_RADIO_DATAHIGH);
		sbval <<= 16;
		BCW_WRITE16(sc, BCW_RADIO_CONTROL, BCW_RADIO_ID);
		sc->sc_radio_mnf = sbval | BCW_READ16(sc, BCW_RADIO_DATALOW);
	} else {
		switch (sc->sc_chip_rev) {
		case 0:	
			sc->sc_radio_mnf = 0x3205017F;
			break;
		case 1:
			sc->sc_radio_mnf = 0x4205017f;
			break;
		default:
			sc->sc_radio_mnf = 0x5205017f;
		}
	}

	sc->sc_radio_rev = (sc->sc_radio_mnf & 0xf0000000) >> 28;
	sc->sc_radio_ver = (sc->sc_radio_mnf & 0x0ffff000) >> 12;

	DPRINTF(("%s: Radio Rev %d, Ver 0x%x, Manuf 0x%x\n",
	    sc->sc_dev.dv_xname, sc->sc_radio_rev, sc->sc_radio_ver,
	    sc->sc_radio_mnf & 0xfff));

	error = bcw_validatechipaccess(sc);
	if (error) {
		printf("%s: failed Chip Access Validation at %d\n",
		    sc->sc_dev.dv_xname, error);
		return;
	}

	/* Test for valid PHY/revision combinations, probably a simpler way */
	if (sc->sc_phy_type == BCW_PHY_TYPEA) {
		switch (sc->sc_phy_rev) {
		case 2:
		case 3:
		case 5:
		case 6:
		case 7:
			break;
		default:
			printf("%s: invalid PHY A revision %d\n",
			    sc->sc_dev.dv_xname, sc->sc_phy_rev);
			return;
		}
	}
	if (sc->sc_phy_type == BCW_PHY_TYPEB) {
		switch (sc->sc_phy_rev) {
		case 2:
		case 4:
		case 7:
			break;
		default:
			printf("%s: invalid PHY B revision %d\n",
			    sc->sc_dev.dv_xname, sc->sc_phy_rev);
			return;
		}
	}
	if (sc->sc_phy_type == BCW_PHY_TYPEG) {
		switch(sc->sc_phy_rev) {
		case 1:
		case 2:
		case 4:
		case 6:
		case 7:
		case 8:
			break;
		default:
			printf("%s: invalid PHY G revision %d\n",
			    sc->sc_dev.dv_xname, sc->sc_phy_rev);
			return;
		}
	}

	/* test for valid radio revisions */
	if ((sc->sc_phy_type == BCW_PHY_TYPEA) &
	    (sc->sc_radio_ver != 0x2060)) {
		    	printf("%s: invalid PHY A radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radio_ver);
		    	return;
	}
	if ((sc->sc_phy_type == BCW_PHY_TYPEB) &
	    ((sc->sc_radio_ver & 0xfff0) != 0x2050)) {
		    	printf("%s: invalid PHY B radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radio_ver);
		    	return;
	}
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &
	    (sc->sc_radio_ver != 0x2050)) {
		    	printf("%s: invalid PHY G radio 0x%x\n",
		    	    sc->sc_dev.dv_xname, sc->sc_radio_ver);
		    	return;
	}

	bcw_radio_off(sc);

	/* Read antenna gain from SPROM and multiply by 4 */
	sbval = BCW_READ16(sc, BCW_SPROM_ANTGAIN);
	/* If unset, assume 2 */
	if ((sbval == 0) || (sbval == 0xffff))
		sbval = 0x0202;
	if (sc->sc_phy_type == BCW_PHY_TYPEA)
		sc->sc_radio_gain = (sbval & 0xff);
	else
		sc->sc_radio_gain = ((sbval & 0xff00) >> 8);
	sc->sc_radio_gain *= 4;

	/*
	 * Set the paXbY vars, X=0 for PHY A, X=1 for B/G, but we'll
	 * just grab them all while we're here
	 */
	sc->sc_radio_pa0b0 = BCW_READ16(sc, BCW_SPROM_PA0B0);
	sc->sc_radio_pa0b1 = BCW_READ16(sc, BCW_SPROM_PA0B1);
	    
	sc->sc_radio_pa0b2 = BCW_READ16(sc, BCW_SPROM_PA0B2);
	sc->sc_radio_pa1b0 = BCW_READ16(sc, BCW_SPROM_PA1B0);
	    
	sc->sc_radio_pa1b1 = BCW_READ16(sc, BCW_SPROM_PA1B1);
	sc->sc_radio_pa1b2 = BCW_READ16(sc, BCW_SPROM_PA1B2);

	/* Get the idle TSSI */
	sbval = BCW_READ16(sc, BCW_SPROM_IDLETSSI);
	if (sc->sc_phy_type == BCW_PHY_TYPEA)
		sc->sc_idletssi = (sbval & 0xff);
	else
		sc->sc_idletssi = ((sbval & 0xff00) >> 8);
	
	/* Init the Microcode Flags Bitfield */
	/* http://bcm-specs.sipsolutions.net/MicrocodeFlagsBitfield */
	sbval = 0;
	if ((sc->sc_phy_type == BCW_PHY_TYPEA) ||
	    (sc->sc_phy_type == BCW_PHY_TYPEB) ||
	    (sc->sc_phy_type == BCW_PHY_TYPEG))
		sbval |= 2; /* Turned on during init for non N phys */
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    (sc->sc_phy_rev == 1))
		sbval |= 0x20;
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    ((sc->sc_boardflags & BCW_BF_PACTRL) == BCW_BF_PACTRL))
		sbval |= 0x40;
	if ((sc->sc_phy_type == BCW_PHY_TYPEG) &&
	    (sc->sc_phy_rev < 3))
		sbval |= 0x8; /* MAGIC */
	if ((sc->sc_boardflags & BCW_BF_XTAL) == BCW_BF_XTAL)
		sbval |= 0x400;
	if (sc->sc_phy_type == BCW_PHY_TYPEB)
		sbval |= 0x4;
	if ((sc->sc_radio_ver == 0x2050) &&
	    (sc->sc_radio_rev <= 5))
	    	sbval |= 0x40000;
	/*
	 * XXX If the device isn't up and this is a PCI bus with revision
	 * 10 or less set bit 0x80000
	 */

	/*
	 * Now, write the value into the regster
	 *
	 * The MicrocodeBitFlags is an unaligned 32bit value in SHM, so the
	 * strategy is to select the aligned word for the lower 16 bits,
	 * but write to the unaligned address. Then, because the SHM
	 * pointer is automatically incremented to the next aligned word,
	 * we can just write the remaining bits as a 16 bit write.
	 * This explanation could make more sense, but an SHM read/write
	 * wrapper of some sort would be better.
	 */
	BCW_WRITE(sc, BCW_SHM_CONTROL,
	    (BCW_SHM_CONTROL_SHARED << 16) + BCW_SHM_MICROCODEFLAGSLOW - 2);
	BCW_WRITE16(sc, BCW_SHM_DATAHIGH, sbval & 0x00ff);
	BCW_WRITE16(sc, BCW_SHM_DATALOW, (sbval & 0xff00) >> 16);

	/*
	 * Initialize the TSSI to DBM table
	 * The method is described at
	 * http://bcm-specs.sipsolutions.net/TSSI_to_DBM_Table
	 * but I suspect there's a standard way to do it in the 80211 stuff
	 */

	/*
	 * XXX TODO still for the card attach:
	 * - Disable the 80211 Core (and wrapper for on/off)
	 * - Setup LEDs to blink in whatever fashionable manner
	 */
	//bcw_powercontrol_crystal_off(sc);	/* TODO Fix panic! */

	/*
	 * Allocate DMA-safe memory for ring descriptors.
	 * The receive and transmit rings are 4k aligned
	 */
	bcw_alloc_rx_ring(sc, &sc->sc_rxring, BCW_RX_RING_COUNT);
	bcw_alloc_tx_ring(sc, &sc->sc_txring, BCW_TX_RING_COUNT);

	ic->ic_phytype = IEEE80211_T_OFDM;
	ic->ic_opmode = IEEE80211_M_STA; /* default to BSS mode */
	ic->ic_state = IEEE80211_S_INIT;

	/* set device capabilities - keep it simple */
	ic->ic_caps = IEEE80211_C_IBSS; /* IBSS mode supported */

	/* MAC address */
	if (sc->sc_phy_type == BCW_PHY_TYPEA) {
		i = BCW_READ16(sc, BCW_SPROM_ET1MACADDR);
		ic->ic_myaddr[0] = (i & 0xff00) >> 8;
		ic->ic_myaddr[1] = i & 0xff;
		i = BCW_READ16(sc, BCW_SPROM_ET1MACADDR + 2);
		    
		ic->ic_myaddr[2] = (i & 0xff00) >> 8;
		ic->ic_myaddr[3] = i & 0xff;
		i = BCW_READ16(sc, BCW_SPROM_ET1MACADDR + 4);
		    
		ic->ic_myaddr[4] = (i & 0xff00) >> 8;
		ic->ic_myaddr[5] = i & 0xff;
	} else { /* assume B or G PHY */
		i = BCW_READ16(sc, BCW_SPROM_IL0MACADDR);
		    
		ic->ic_myaddr[0] = (i & 0xff00) >> 8;
		ic->ic_myaddr[1] = i & 0xff;
		i = BCW_READ16(sc, BCW_SPROM_IL0MACADDR + 2);
		    
		ic->ic_myaddr[2] = (i & 0xff00) >> 8;
		ic->ic_myaddr[3] = i & 0xff;
		i = BCW_READ16(sc, BCW_SPROM_IL0MACADDR + 4);
		    
		ic->ic_myaddr[4] = (i & 0xff00) >> 8;
		ic->ic_myaddr[5] = i & 0xff;
	}
	
	printf(", address %s\n", ether_sprintf(ic->ic_myaddr));

	/* Set supported rates */
	ic->ic_sup_rates[IEEE80211_MODE_11B] = ieee80211_std_rateset_11b;
	ic->ic_sup_rates[IEEE80211_MODE_11G] = ieee80211_std_rateset_11g;

	/* Set supported channels */
	for (i = 1; i <= 14; i++) {
		ic->ic_channels[i].ic_freq =
		    ieee80211_ieee2mhz(i, IEEE80211_CHAN_2GHZ);
		ic->ic_channels[i].ic_flags =
		    IEEE80211_CHAN_CCK | IEEE80211_CHAN_OFDM |
		    IEEE80211_CHAN_DYN | IEEE80211_CHAN_2GHZ;
	}

	/* IBSS channel undefined for now */
	ic->ic_ibss_chan = &ic->ic_channels[0];

	ifp->if_softc = sc;
	ifp->if_flags = IFF_BROADCAST | IFF_SIMPLEX | IFF_MULTICAST;
	ifp->if_init = bcw_init;
	ifp->if_ioctl = bcw_ioctl;
	ifp->if_start = bcw_start;
	ifp->if_watchdog = bcw_watchdog;
	IFQ_SET_READY(&ifp->if_snd);
	bcopy(sc->sc_dev.dv_xname, ifp->if_xname, IFNAMSIZ);

	/* Attach the interface */
	if_attach(ifp);
	ieee80211_ifattach(ifp);

	/* override state transition machine */
	sc->sc_newstate = ic->ic_newstate;
	ic->ic_newstate = bcw_newstate;
	ieee80211_media_init(ifp, bcw_media_change, bcw_media_status);

	timeout_set(&sc->sc_timeout, bcw_tick, sc);
}

/* handle media, and ethernet requests */
int
bcw_ioctl(struct ifnet *ifp, u_long cmd, caddr_t data)
{
	struct bcw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifreq   *ifr = (struct ifreq *) data;
	struct ifaddr *ifa = (struct ifaddr *)data;
	int s, error = 0;

	s = splnet();

	switch (cmd) {
	case SIOCSIFADDR:
		ifp->if_flags |= IFF_UP;

		switch (ifa->ifa_addr->sa_family) {
#ifdef INET
		case AF_INET:
			//bcw_init(ifp);
			/* XXX arp_ifinit(&sc->bcw_ac, ifa); */
			break;
#endif /* INET */
		default:
			//bcw_init(ifp);
			break;
		}
		break;
	case SIOCSIFFLAGS:
		if ((ifp->if_flags & IFF_UP) &&
		    (!(ifp->if_flags & IFF_RUNNING)))
				bcw_init(ifp);
		else if (ifp->if_flags & IFF_RUNNING)
			bcw_stop(ifp, 1);
		break;
	case SIOCADDMULTI:
	case SIOCDELMULTI:
		error = (cmd == SIOCADDMULTI) ?
		    ether_addmulti(ifr, &ic->ic_ac) :
		    ether_delmulti(ifr, &ic->ic_ac);

		if (error == ENETRESET)
			error = 0;
		break;

	case SIOCG80211TXPOWER:
		/*
		 * If the hardware radio transmitter switch is off, report a
		 * tx power of IEEE80211_TXPOWER_MIN to indicate that radio
		 * transmitter is killed.
		 */
		break;

	default:
		error = ieee80211_ioctl(ifp, cmd, data);
		break;
	}

	if (error == ENETRESET) {
		if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) ==
		    (IFF_UP | IFF_RUNNING))
			bcw_init(ifp);
		error = 0;
	}

	splx(s);
	return error;
}

/* Start packet transmission on the interface. */
void
bcw_start(struct ifnet *ifp)
{
#if 0
	struct bcw_softc *sc = ifp->if_softc;
	struct mbuf    *m0;
	bus_dmamap_t    dmamap;
	int		txstart;
	int		txsfree;
	int		error;
#endif
	int		newpkts = 0;

	/*
	 * do not start another if currently transmitting, and more
	 * descriptors(tx slots) are needed for next packet.
	 */
	if ((ifp->if_flags & (IFF_RUNNING | IFF_OACTIVE)) != IFF_RUNNING)
		return;

#if 0   /* FIXME */
	/* determine number of descriptors available */
	if (sc->sc_txsnext >= sc->sc_txin)
		txsfree = BCW_NTXDESC - 1 + sc->sc_txin - sc->sc_txsnext;
	else
		txsfree = sc->sc_txin - sc->sc_txsnext - 1;

	/*
	 * Loop through the send queue, setting up transmit descriptors
	 * until we drain the queue, or use up all available transmit
	 * descriptors.
	 */
	while (txsfree > 0) {
		int	seg;

		/* Grab a packet off the queue. */
		IFQ_POLL(&ifp->if_snd, m0);
		if (m0 == NULL)
			break;

		/* get the transmit slot dma map */
		dmamap = sc->sc_cdata.bcw_tx_map[sc->sc_txsnext];

		/*
		 * Load the DMA map.  If this fails, the packet either
		 * didn't fit in the alloted number of segments, or we
		 * were short on resources. If the packet will not fit,
		 * it will be dropped. If short on resources, it will
		 * be tried again later.
		 */
		error = bus_dmamap_load_mbuf(sc->sc_dmat, dmamap, m0,
		    BUS_DMA_WRITE | BUS_DMA_NOWAIT);
		if (error == EFBIG) {
			printf("%s: Tx packet consumes too many DMA segments, "
			    "dropping...\n", sc->sc_dev.dv_xname);
			IFQ_DEQUEUE(&ifp->if_snd, m0);
			m_freem(m0);
			ifp->if_oerrors++;
			continue;
		} else if (error) {
			/* short on resources, come back later */
			printf("%s: unable to load Tx buffer, error = %d\n",
			    sc->sc_dev.dv_xname, error);
			break;
		}
		/* If not enough descriptors available, try again later */
		if (dmamap->dm_nsegs > txsfree) {
			ifp->if_flags |= IFF_OACTIVE;
			bus_dmamap_unload(sc->sc_dmat, dmamap);
			break;
		}
		/* WE ARE NOW COMMITTED TO TRANSMITTING THE PACKET. */

		/* So take it off the queue */
		IFQ_DEQUEUE(&ifp->if_snd, m0);

		/* save the pointer so it can be freed later */
		sc->sc_cdata.bcw_tx_chain[sc->sc_txsnext] = m0;

		/* Sync the data DMA map. */
		bus_dmamap_sync(sc->sc_dmat, dmamap, 0, dmamap->dm_mapsize,
		    BUS_DMASYNC_PREWRITE);

		/* Initialize the transmit descriptor(s). */
		txstart = sc->sc_txsnext;
		for (seg = 0; seg < dmamap->dm_nsegs; seg++) {
			uint32_t ctrl;

			ctrl = dmamap->dm_segs[seg].ds_len & CTRL_BC_MASK;
			if (seg == 0)
				ctrl |= CTRL_SOF;
			if (seg == dmamap->dm_nsegs - 1)
				ctrl |= CTRL_EOF;
			if (sc->sc_txsnext == BCW_NTXDESC - 1)
				ctrl |= CTRL_EOT;
			ctrl |= CTRL_IOC;
			sc->bcw_tx_ring[sc->sc_txsnext].ctrl = htole32(ctrl);
			/* MAGIC */
			sc->bcw_tx_ring[sc->sc_txsnext].addr =
			    htole32(dmamap->dm_segs[seg].ds_addr + 0x40000000);
			if (sc->sc_txsnext + 1 > BCW_NTXDESC - 1)
				sc->sc_txsnext = 0;
			else
				sc->sc_txsnext++;
			txsfree--;
		}
		/* sync descriptors being used */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_ring_map,
		    sizeof(struct bcw_dma_slot) * txstart + PAGE_SIZE,
		    sizeof(struct bcw_dma_slot) * dmamap->dm_nsegs,
		    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

		/* Give the packet to the chip. */
		BCW_WRITE(sc, BCW_DMA_DPTR,
		    sc->sc_txsnext * sizeof(struct bcw_dma_slot));

		newpkts++;

#if NBPFILTER > 0
		/* Pass the packet to any BPF listeners. */
		if (ifp->if_bpf)
			bpf_mtap(ifp->if_bpf, m0, BPF_DIRECTION_OUT);
#endif				/* NBPFILTER > 0 */
	}
	if (txsfree == 0) {
		/* No more slots left; notify upper layer. */
		ifp->if_flags |= IFF_OACTIVE;
	}
#endif /* FIXME */
	if (newpkts) {
		/* Set a watchdog timer in case the chip flakes out. */
		ifp->if_timer = 5;
	}
}

/* Watchdog timer handler. */
void
bcw_watchdog(struct ifnet *ifp)
{
	struct bcw_softc *sc = ifp->if_softc;

	printf("%s: device timeout\n", sc->sc_dev.dv_xname);
	ifp->if_oerrors++;

	(void) bcw_init(ifp);

	/* Try to get more packets going. */
	bcw_start(ifp);
}

int
bcw_intr(void *xsc)
{
	struct bcw_softc *sc;
	struct ifnet *ifp;
	uint32_t intstatus;
	int wantinit;
	int handled = 0;

	sc = xsc;

	for (wantinit = 0; wantinit == 0;) {
		intstatus = (sc->sc_conf_read)(sc->sc_dev_softc, BCW_INT_STS);

		/* ignore if not ours, or unsolicited interrupts */
		intstatus &= sc->sc_intmask;
		if (intstatus == 0)
			break;

		handled = 1;

		/* Ack interrupt */
		(sc->sc_conf_write)(sc->sc_dev_softc, BCW_INT_STS, intstatus);

		/* Receive interrupts. */
		if (intstatus & I_RI)
			bcw_rxintr(sc);
		/* Transmit interrupts. */
		if (intstatus & I_XI)
			bcw_txintr(sc);
		/* Error interrupts */
		if (intstatus & ~(I_RI | I_XI)) {
			if (intstatus & I_XU)
				printf("%s: transmit fifo underflow\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_RO) {
				printf("%s: receive fifo overflow\n",
				    sc->sc_dev.dv_xname);
				ifp->if_ierrors++;
			}
			if (intstatus & I_RU)
				printf("%s: receive descriptor underflow\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_DE)
				printf("%s: descriptor protocol error\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_PD)
				printf("%s: data error\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_PC)
				printf("%s: descriptor error\n",
				    sc->sc_dev.dv_xname);
			if (intstatus & I_TO)
				printf("%s: general purpose timeout\n",
				    sc->sc_dev.dv_xname);
			wantinit = 1;
		}
	}

	if (handled) {
		if (wantinit)
			bcw_init(ifp);
		/* Try to get more packets going. */
		bcw_start(ifp);
	}

	return (handled);
}

/* Receive interrupt handler */
void
bcw_rxintr(struct bcw_softc *sc)
{
#if 0
	struct rx_pph *pph;
	struct mbuf *m;
	int len;
	int i;
#endif
	int curr;

	/* get pointer to active receive slot */
	curr = BCW_READ(sc, BCW_DMA_RXSTATUS(0)) & RS_CD_MASK;
	curr = curr / sizeof(struct bcw_dma_slot);
	if (curr >= BCW_RX_RING_COUNT)
		curr = BCW_RX_RING_COUNT - 1;

#if 0
	/* process packets up to but not current packet being worked on */
	for (i = sc->sc_rxin; i != curr;
	    i + 1 > BCW_NRXDESC - 1 ? i = 0 : i++) {
		/* complete any post dma memory ops on packet */
		bus_dmamap_sync(sc->sc_dmat, sc->sc_cdata.bcw_rx_map[i], 0,
		    sc->sc_cdata.bcw_rx_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTREAD);

		/*
		 * If the packet had an error, simply recycle the buffer,
		 * resetting the len, and flags.
		 */
		pph = mtod(sc->sc_cdata.bcw_rx_chain[i], struct rx_pph *);
		if (pph->flags & (RXF_NO | RXF_RXER | RXF_CRC | RXF_OV)) {
			/* XXX Increment input error count */
			pph->len = 0;
			pph->flags = 0;
			continue;
		}
		/* receive the packet */
		len = pph->len;
		if (len == 0)
			continue;	/* no packet if empty */
		pph->len = 0;
		pph->flags = 0;
		/* bump past pre header to packet */
		sc->sc_cdata.bcw_rx_chain[i]->m_data +=
		    BCW_PREPKT_HEADER_SIZE;

 		/*
		 * The chip includes the CRC with every packet.  Trim
		 * it off here.
		 */
		len -= ETHER_CRC_LEN;

		/*
		 * If the packet is small enough to fit in a
		 * single header mbuf, allocate one and copy
		 * the data into it.  This greatly reduces
		 * memory consumption when receiving lots
		 * of small packets.
		 *
		 * Otherwise, add a new buffer to the receive
		 * chain.  If this fails, drop the packet and
		 * recycle the old buffer.
		 */
		if (len <= (MHLEN - 2)) {
			MGETHDR(m, M_DONTWAIT, MT_DATA);
			if (m == NULL)
				goto dropit;
			m->m_data += 2;
			memcpy(mtod(m, caddr_t),
			    mtod(sc->sc_cdata.bcw_rx_chain[i], caddr_t), len);
			sc->sc_cdata.bcw_rx_chain[i]->m_data -=
			    BCW_PREPKT_HEADER_SIZE;
		} else {
			m = sc->sc_cdata.bcw_rx_chain[i];
			if (bcw_add_rxbuf(sc, i) != 0) {
		dropit:
				/* XXX increment wireless input error counter */
				/* continue to use old buffer */
				sc->sc_cdata.bcw_rx_chain[i]->m_data -=
				    BCW_PREPKT_HEADER_SIZE;
				bus_dmamap_sync(sc->sc_dmat,
				    sc->sc_cdata.bcw_rx_map[i], 0,
				    sc->sc_cdata.bcw_rx_map[i]->dm_mapsize,
				    BUS_DMASYNC_PREREAD);
				continue;
			}
		}

		m->m_pkthdr.rcvif = ifp;
		m->m_pkthdr.len = m->m_len = len;
		/* XXX Increment input packet count */

#if NBPFILTER > 0
		/*
		 * Pass this up to any BPF listeners, but only
		 * pass it up the stack if it's for us.
		 *
		 * if (ifp->if_bpf)
		 *	bpf_mtap(ifp->if_bpf, m, BPF_DIRECTION_IN);
		 */
#endif				/* NBPFILTER > 0 */

		/* XXX Pass it on. */
		//ether_input_mbuf(ifp, m);

		/* re-check current in case it changed */
		curr = (BCW_READ(sc, BCW_DMA_RXSTATUS) & RS_CD_MASK) /
		    sizeof(struct bcw_dma_slot);
		if (curr >= BCW_NRXDESC)
			curr = BCW_NRXDESC - 1;
	}
	sc->sc_rxin = curr;
#endif
}

/* Transmit interrupt handler */
void
bcw_txintr(struct bcw_softc *sc)
{
//	struct ifnet *ifp = &sc->bcw_ac.ac_if;
	int curr;
//	int i;

//	ifp->if_flags &= ~IFF_OACTIVE;

#if 0
	/*
	 * Go through the Tx list and free mbufs for those
	 * frames which have been transmitted.
	 */
	curr = BCW_READ(sc, BCW_DMA_TXSTATUS) & RS_CD_MASK;
	curr = curr / sizeof(struct bcw_dma_slot);
	if (curr >= BCW_NTXDESC)
		curr = BCW_NTXDESC - 1;
	for (i = sc->sc_txin; i != curr;
	    i + 1 > BCW_NTXDESC - 1 ? i = 0 : i++) {
		/* do any post dma memory ops on transmit data */
		if (sc->sc_cdata.bcw_tx_chain[i] == NULL)
			continue;
		bus_dmamap_sync(sc->sc_dmat, sc->sc_cdata.bcw_tx_map[i], 0,
		    sc->sc_cdata.bcw_tx_map[i]->dm_mapsize,
		    BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, sc->sc_cdata.bcw_tx_map[i]);
		m_freem(sc->sc_cdata.bcw_tx_chain[i]);
		sc->sc_cdata.bcw_tx_chain[i] = NULL;
		ifp->if_opackets++;
	}
#endif
	sc->sc_txin = curr;

	/*
	 * If there are no more pending transmissions, cancel the watchdog
	 * timer
	 */
//	if (sc->sc_txsnext == sc->sc_txin)
//		ifp->if_timer = 0;
}

/* initialize the interface */
int
bcw_init(struct ifnet *ifp)
{
	struct bcw_softc *sc = ifp->if_softc;
	uint16_t val16;
	uint32_t val32;
	int error, i, tmp;

	BCW_WRITE(sc, BCW_SBF, BCW_SBF_CORE_READY | BCW_SBF_400_MAGIC);

	/* load firmware */
	if ((error = bcw_load_firmware(sc)))
		return (error);

	/*
	 * verify firmware revision
	 */
	BCW_WRITE(sc, BCW_GIR, 0xffffffff);
	BCW_WRITE(sc, BCW_SBF, 0x00020402);
	for (i = 0; i < 50; i++) {
		if (BCW_READ(sc, BCW_GIR) == BCW_INTR_READY)
			break;
		delay(10);
	}
	if (i == 50) {
		printf("%s: interrupt-ready timeout!\n", sc->sc_dev.dv_xname);
		return (1);
	}
	BCW_READ(sc, BCW_GIR);	/* dummy read */

	val16 = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_REVISION);

	DPRINTF(("%s: Firmware revision 0x%x, patchlevel 0x%x "
            "(20%.2i-%.2i-%.2i %.2i:%.2i:%.2i)\n",
	    sc->sc_dev.dv_xname, val16,
	    bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_PATCHLEVEL),
            (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_DATE) >> 12)
            & 0xf,
            (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_DATE) >> 8)
            & 0xf,
            bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_DATE)
            & 0xff,
            (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_TIME) >> 11)
            & 0x1f,
            (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_TIME) >> 5)
            & 0x3f,
            bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, BCW_UCODE_TIME)
	    & 0x1f));

	if (val16 > 0x128) {
		printf("%s: no support for this firmware revision!\n",
		    sc->sc_dev.dv_xname);
		return (1);
	}

	/* initialize GPIO */
	if ((error = bcw_gpio_init(sc)))
		return (error);

	/* load init values */
	if ((error = bcw_load_initvals(sc)))
		return (error);

	/* turn radio on */
	bcw_radio_on(sc);

	BCW_WRITE16(sc, 0x03e6, 0);
	if ((error = bcw_phy_init(sc)))
		return (error);

	/* select initial interference mitigation */
	tmp = sc->sc_radio_interfmode;
	sc->sc_radio_interfmode = BCW_RADIO_INTERFMODE_NONE;
	bcw_radio_set_interference_mitigation(sc, tmp);

	bcw_phy_set_antenna_diversity(sc);
	bcw_radio_set_txantenna(sc, BCW_RADIO_TXANTENNA_DEFAULT);
	if (sc->sc_phy_type == BCW_PHY_TYPEB) {
		val16 = BCW_READ16(sc, 0x005e);
		val16 |= 0x0004;
		BCW_WRITE16(sc, 0x005e, val16);
	}
	BCW_WRITE(sc, 0x0100, 0x01000000);
	if (1) /* XXX current_core->rev */
		BCW_WRITE(sc, 0x010c, 0x01000000);

	val32 = BCW_READ(sc, BCW_SBF);
	val32 &= ~BCW_SBF_ADHOC;
	BCW_WRITE(sc, BCW_SBF, val32);
	val32 = BCW_READ(sc, BCW_SBF);
	val32 |= BCW_SBF_ADHOC;
	BCW_WRITE(sc, BCW_SBF, val32);

	val32 = BCW_READ(sc, BCW_SBF);
	val32 |= 0x100000;
	BCW_WRITE(sc, BCW_SBF, val32);

	if (bcw_using_pio(sc)) {
		BCW_WRITE(sc, 0x0210, 0x00000100);
		BCW_WRITE(sc, 0x0230, 0x00000100);
		BCW_WRITE(sc, 0x0250, 0x00000100);
		BCW_WRITE(sc, 0x0270, 0x00000100);
		bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0034, 0);
	}

	bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0074, 0);

	if (1) { /* XXX core_rev */
		BCW_WRITE16(sc, 0x060e, 0);
		BCW_WRITE16(sc, 0x0610, 0x8000);
		BCW_WRITE16(sc, 0x0604, 0);
		BCW_WRITE16(sc, 0x0606, 0x0200);
	} else {
		BCW_WRITE(sc, 0x0188, 0x80000000);
		BCW_WRITE(sc, 0x0606, 0x02000000);
	}
	BCW_WRITE(sc, BCW_GIR, 0x00004000);
	BCW_WRITE(sc, BCW_DMA0_INT_MASK, 0x0001dc00);
	BCW_WRITE(sc, BCW_DMA1_INT_MASK, 0x0000dc00);
	BCW_WRITE(sc, BCW_DMA2_INT_MASK, 0x0000dc00);
	BCW_WRITE(sc, BCW_DMA3_INT_MASK, 0x0001dc00);
	BCW_WRITE(sc, BCW_DMA4_INT_MASK, 0x0000dc00);
	BCW_WRITE(sc, BCW_DMA5_INT_MASK, 0x0000dc00);

	val32 = BCW_READ(sc, BCW_CIR_SBTMSTATELOW);
	val32 |= 0x00100000;
	BCW_WRITE(sc, BCW_CIR_SBTMSTATELOW, val32);

	DPRINTF(("%s: Chip initialized\n", sc->sc_dev.dv_xname));

#if 0
	/* Cancel any pending I/O. */
	bcw_stop(ifp, 0);

	/*
	 * Most of this needs to be rewritten to take into account the
	 * possible single/multiple core nature of the BCM43xx, and the
	 * differences from the BCM44xx ethernet chip that if_bce.c is
	 * written for.
	 */

	/* enable pci interrupts, bursts, and prefetch */

	/* remap the pci registers to the Sonics config registers */
	/* XXX - use (sc->sc_conf_read/write) */
	/* save the current map, so it can be restored */
	reg_win = BCW_READ(sc, BCW_REG0_WIN);

	/* set register window to Sonics registers */
	BCW_WRITE(sc, BCW_REG0_WIN, BCW_SONICS_WIN);

	/* enable SB to PCI interrupt */
	BCW_WRITE(sc, BCW_SBINTVEC, BCW_READ(sc, BCW_SBINTVEC) | SBIV_ENET0);

	/* enable prefetch and bursts for sonics-to-pci translation 2 */
	BCW_WRITE(sc, BCW_SPCI_TR2,
	    BCW_READ(sc, BCW_SPCI_TR2) | SBTOPCI_PREF | SBTOPCI_BURST);

	/* restore to ethernet register space */
	BCW_WRITE(sc, BCW_REG0_WIN, reg_win);

	/* Reset the chip to a known state. */
	bcw_reset(sc);

	/* FIXME */
	/* Initialize transmit descriptors */
	memset(sc->bcw_tx_ring, 0, BCW_NTXDESC * sizeof(struct bcw_dma_slot));
	sc->sc_txsnext = 0;
	sc->sc_txin = 0;

	/* enable crc32 generation and set proper LED modes */
	BCW_WRITE(sc, BCW_MACCTL,
	    BCW_READ(sc, BCW_MACCTL) | BCW_EMC_CRC32_ENAB | BCW_EMC_LED);
	    
	/* reset or clear powerdown control bit  */
	BCW_WRITE(sc, BCW_MACCTL, BCW_READ(sc, BCW_MACCTL) & ~BCW_EMC_PDOWN);

	/* setup DMA interrupt control */
	BCW_WRITE(sc, BCW_DMAI_CTL, 1 << 24);	/* MAGIC */

	/* setup packet filter */
	bcw_set_filter(ifp);

	/* set max frame length, account for possible VLAN tag */
	BCW_WRITE(sc, BCW_RX_MAX, ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);
	BCW_WRITE(sc, BCW_TX_MAX, ETHER_MAX_LEN + ETHER_VLAN_ENCAP_LEN);

	/* set tx watermark */
	BCW_WRITE(sc, BCW_TX_WATER, 56);

	/* enable transmit */
	BCW_WRITE(sc, BCW_DMA_TXCTL, XC_XE);

	/*
	 * Give the receive ring to the chip, and
	 * start the receive DMA engine.
	 */
	sc->sc_rxin = 0;

	/* enable receive */
	BCW_WRITE(sc, BCW_DMA_RXCTL, BCW_PREPKT_HEADER_SIZE << 1 | 1);

	/* Enable interrupts */
	sc->sc_intmask =
	    I_XI | I_RI | I_XU | I_RO | I_RU | I_DE | I_PD | I_PC | I_TO;
	BCW_WRITE(sc, BCW_INT_MASK, sc->sc_intmask);
	    
	/* FIXME */
	/* start the receive dma */
	BCW_WRITE(sc, BCW_DMA_RXDPTR,
	    BCW_NRXDESC * sizeof(struct bcw_dma_slot));

	/* set media */
	//mii_mediachg(&sc->bcw_mii);

	/* turn on the ethernet mac */
	BCW_WRITE(sc, BCW_ENET_CTL, BCW_READ(sc, BCW_ENET_CTL) | EC_EE);
	    
#endif
	/* start timer */
	timeout_add(&sc->sc_timeout, hz);

	/* mark as running, and no outputs active */
	ifp->if_flags |= IFF_RUNNING;
	ifp->if_flags &= ~IFF_OACTIVE;

	return (0);
}

/* Add a receive buffer to the indicated descriptor. */
int
bcw_add_rxbuf(struct bcw_softc *sc, int idx)
{
#if 0
	struct mbuf *m;
	int error;

	MGETHDR(m, M_DONTWAIT, MT_DATA);
	if (m == NULL)
		return (ENOBUFS);

	MCLGET(m, M_DONTWAIT);
	if ((m->m_flags & M_EXT) == 0) {
		m_freem(m);
		return (ENOBUFS);
	}
	if (sc->sc_cdata.bcw_rx_chain[idx] != NULL)
		bus_dmamap_unload(sc->sc_dmat,
		    sc->sc_cdata.bcw_rx_map[idx]);

	sc->sc_cdata.bcw_rx_chain[idx] = m;

	error = bus_dmamap_load(sc->sc_dmat, sc->sc_cdata.bcw_rx_map[idx],
	    m->m_ext.ext_buf, m->m_ext.ext_size, NULL,
	    BUS_DMA_READ | BUS_DMA_NOWAIT);
	if (error)
		return (error);

	bus_dmamap_sync(sc->sc_dmat, sc->sc_cdata.bcw_rx_map[idx], 0,
	    sc->sc_cdata.bcw_rx_map[idx]->dm_mapsize, BUS_DMASYNC_PREREAD);

	BCW_INIT_RXDESC(sc, idx);

	return (0);
#endif
	return (1);

}

/* Drain the receive queue. */
void
bcw_rxdrain(struct bcw_softc *sc)
{
#if 0
	/* FIXME */
	int i;

	for (i = 0; i < BCW_NRXDESC; i++) {
		if (sc->sc_cdata.bcw_rx_chain[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->sc_cdata.bcw_rx_map[i]);
			m_freem(sc->sc_cdata.bcw_rx_chain[i]);
			sc->sc_cdata.bcw_rx_chain[i] = NULL;
		}
	}
#endif
}

/* Stop transmission on the interface */
void
bcw_stop(struct ifnet *ifp, int disable)
{
	struct bcw_softc *sc = ifp->if_softc;
	//uint32_t val;

	/* Stop the 1 second timer */
	timeout_del(&sc->sc_timeout);

	/* Mark the interface down and cancel the watchdog timer. */
	ifp->if_flags &= ~(IFF_RUNNING | IFF_OACTIVE);
	ifp->if_timer = 0;

	/* Disable interrupts. */
	BCW_WRITE(sc, BCW_DMA0_INT_MASK, 0);
	sc->sc_intmask = 0;
	delay(10);

	/* Disable emac */
#if 0
	BCW_WRITE(sc, BCW_ENET_CTL, EC_ED);
	for (i = 0; i < 200; i++) {
		val = BCW_READ(sc, BCW_ENET_CTL);
		    
		if (!(val & EC_ED))
			break;
		delay(10);
	}
#endif
	/* Stop the DMA */
	BCW_WRITE(sc, BCW_DMA_RXCONTROL(0), 0);
	BCW_WRITE(sc, BCW_DMA_TXCONTROL(0), 0);
	delay(10);

#if 0	/* FIXME */
	/* Release any queued transmit buffers. */
	for (i = 0; i < BCW_NTXDESC; i++) {
		if (sc->sc_cdata.bcw_tx_chain[i] != NULL) {
			bus_dmamap_unload(sc->sc_dmat,
			    sc->sc_cdata.bcw_tx_map[i]);
			m_freem(sc->sc_cdata.bcw_tx_chain[i]);
			sc->sc_cdata.bcw_tx_chain[i] = NULL;
		}
	}
#endif

	/* drain receive queue */
	if (disable)
		bcw_rxdrain(sc);
}

/* reset the chip */
void
bcw_reset(struct bcw_softc *sc)
{
	uint32_t sbval;
	uint32_t reject;

	/*
	 * Figure out what revision the Sonic Backplane is, as the position
	 * of the Reject bit changes.
	 */
	sbval = BCW_READ(sc, BCW_CIR_SBID_LO);
	sc->sc_sbrev = (sbval & SBREV_MASK) >> SBREV_MASK_SHIFT;

	switch (sc->sc_sbrev) {
	case 0:
		reject = SBTML_REJ22;
		break;
	case 1:
		reject = SBTML_REJ23;
		break;
	default:
		reject = SBTML_REJ22 | SBTML_REJ23;
	}

	sbval = BCW_READ(sc, BCW_SBTMSTATELOW);

	/*
	 * If the 802.11 core is enabled, only clock of clock,reset,reject
	 * will be set, and we need to reset all the DMA engines first.
	 */
	bcw_change_core(sc, sc->sc_core_80211->num);

	sbval = BCW_READ(sc, BCW_SBTMSTATELOW);
#if 0
	if ((sbval & (SBTML_RESET | reject | SBTML_CLK)) == SBTML_CLK) {
		/* XXX Stop all DMA */
		/* XXX reset the dma engines */
	}
	/* XXX Cores are reset manually elsewhere for now */
	/* Reset the wireless core, attaching the PHY */
	bcw_reset_core(sc, SBTML_80211FLAG | SBTML_80211PHY );
	bcw_change_core(sc, sc->sc_core_common->num);
	bcw_reset_core(sc, 0);
	bcw_change_core(sc, sc->sc_core_bus->num);
	bcw_reset_core(sc, 0);
#endif
	/* XXX update PHYConnected to requested value */

	/* Clear Baseband Attenuation, might only work for B/G rev < 0 */
	BCW_WRITE16(sc, BCW_RADIO_BASEBAND, 0);

	/* Set 0x400 in the MMIO StatusBitField reg */
	sbval = BCW_READ(sc, BCW_SBF);
	sbval |= BCW_SBF_400_MAGIC;
	BCW_WRITE(sc, BCW_SBF, sbval);

	/* XXX Clear saved interrupt status for DMA controllers */

	/*
	 * XXX Attach cores to the backplane, if we have more than one
	 * Don't attach PCMCIA cores on a PCI card, and reverse?
	 * OR together the bus flags of the 3 cores and write to PCICR
	 */
#if 0
	if (sc->sc_havecommon == 1) {
		sbval = (sc->sc_conf_read)(sc, BCW_PCICR);
		sbval |= 0x1 << 8; /* XXX hardcoded bitmask of single core */
		(sc->sc_conf_write)(sc, BCW_PCICR, sbval);
	}
#endif
	/* Change back to the Wireless core */
	bcw_change_core(sc, sc->sc_core_80211->num);
}

/* Set up the receive filter. */
void
bcw_set_filter(struct ifnet *ifp)
{
#if 0
	struct bcw_softc *sc = ifp->if_softc;

	if (ifp->if_flags & IFF_PROMISC) {
		ifp->if_flags |= IFF_ALLMULTI;
		BCW_WRITE(sc, BCW_RX_CTL, BCW_READ(sc, BCW_RX_CTL) | ERC_PE);
	} else {
		ifp->if_flags &= ~IFF_ALLMULTI;

		/* turn off promiscuous */
		BCW_WRITE(sc, BCW_RX_CTL, BCW_READ(sc, BCW_RX_CTL) & ~ERC_PE);

		/* enable/disable broadcast */
		if (ifp->if_flags & IFF_BROADCAST)
			BCW_WRITE(sc, BCW_RX_CTL,
			    BCW_READ(sc, BCW_RX_CTL) & ~ERC_DB);
		else
			BCW_WRITE(sc, BCW_RX_CTL,
			    BCW_READ(sc, BCW_RX_CTL) | ERC_DB);

		/* disable the filter */
		BCW_WRITE(sc, BCW_FILT_CTL, 0);

		/* add our own address */
		// bcw_add_mac(sc, sc->bcw_ac.ac_enaddr, 0);

		/* for now accept all multicast */
		BCW_WRITE(sc, BCW_RX_CTL, BCW_READ(sc, BCW_RX_CTL) | ERC_AM);

		ifp->if_flags |= IFF_ALLMULTI;

		/* enable the filter */
		BCW_WRITE(sc, BCW_FILT_CTL, BCW_READ(sc, BCW_FILT_CTL) | 1);
	}
#endif
}

int
bcw_newstate(struct ieee80211com *ic, enum ieee80211_state nstate, int arg)
{
#if 0
	struct bcw_softc *sc = ic->ic_softc;
	enum ieee80211_state ostate;
	uint32_t tmp;

	ostate = ic->ic_state;
#endif
	return (0);
}

int
bcw_media_change(struct ifnet *ifp)
{
	int error;

	error = ieee80211_media_change(ifp);
	if (error != ENETRESET)
		return error;

	if ((ifp->if_flags & (IFF_UP | IFF_RUNNING)) == (IFF_UP | IFF_RUNNING))
		bcw_init(ifp);

	return (0);
}

	void
bcw_media_status(struct ifnet *ifp, struct ifmediareq *imr)
{
	struct bcw_softc *sc = ifp->if_softc;
	struct ieee80211com *ic = &sc->sc_ic;
	//uint32_t val;
	int rate;

	imr->ifm_status = IFM_AVALID;
	imr->ifm_active = IFM_IEEE80211;
	if (ic->ic_state == IEEE80211_S_RUN)
		imr->ifm_status |= IFM_ACTIVE;

	/*
	 * XXX Read current transmission rate from the adapter.
	 */
	//val = CSR_READ_4(sc, IWI_CSR_CURRENT_TX_RATE);
	/* convert PLCP signal to 802.11 rate */
	//rate = bcw_rate(val);
	rate = 0;

	imr->ifm_active |= ieee80211_rate2media(ic, rate, ic->ic_curmode);
	switch (ic->ic_opmode) {
		case IEEE80211_M_STA:
			break;
		case IEEE80211_M_IBSS:
			imr->ifm_active |= IFM_IEEE80211_ADHOC;
			break;
		case IEEE80211_M_MONITOR:
			imr->ifm_active |= IFM_IEEE80211_MONITOR;
			break;
		case IEEE80211_M_AHDEMO:
		case IEEE80211_M_HOSTAP:
			/* should not get there */
			break;
	}
}

/* One second timer, checks link status */
void
bcw_tick(void *v)
{
#if 0
	struct bcw_softc *sc = v;
	/* http://bcm-specs.sipsolutions.net/PeriodicTasks */
	timeout_add(&sc->bcw_timeout, hz);
#endif
}

/*
 * Validate Chip Access
 */
int
bcw_validatechipaccess(struct bcw_softc *sc)
{
	uint32_t save,val;

	/* Make sure we're dealing with the wireless core */
	bcw_change_core(sc, sc->sc_core_80211->num);

	/*
	 * We use the offset of zero a lot here to reset the SHM pointer to the
	 * beginning of it's memory area, as it automatically moves on every
	 * access to the SHM DATA registers
	 */

	/* Backup SHM uCode Revision before we clobber it */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	save = BCW_READ(sc, BCW_SHM_DATA);

	/* write test value */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE(sc, BCW_SHM_DATA, 0xaa5555aa);
	/* Read it back */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);

	val = BCW_READ(sc, BCW_SHM_DATA);
	if (val != 0xaa5555aa)
		return (1);

	/* write 2nd test value */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE(sc, BCW_SHM_DATA, 0x55aaaa55);
	/* Read it back */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);

	val = BCW_READ(sc, BCW_SHM_DATA);
	if (val != 0x55aaaa55)
		return 2;

	/* Restore the saved value now that we're done */
	BCW_WRITE(sc, BCW_SHM_CONTROL, (BCW_SHM_CONTROL_SHARED << 16) + 0);
	BCW_WRITE(sc, BCW_SHM_DATA, save);
	if (sc->sc_core_80211->rev >= 3) {
		/* do some test writes and reads against the TSF */
		/*
		 * This works during the attach, but the spec at
		 * http://bcm-specs.sipsolutions.net/Timing
		 * say that we're reading/writing silly places, so these regs
		 * are not quite documented yet
		 */
		BCW_WRITE16(sc, 0x18c, 0xaaaa);
		BCW_WRITE(sc, 0x18c, 0xccccbbbb);
		val = BCW_READ16(sc, 0x604);
		if (val != 0xbbbb) return 3;
		val = BCW_READ16(sc, 0x606);
		if (val != 0xcccc) return 4;
		/* re-clear the TSF since we just filled it with garbage */
		BCW_WRITE(sc, 0x18c, 0x0);
	}

	/* Check the Status Bit Field for some unknown bits */
	val = BCW_READ(sc, BCW_SBF);
	if ((val | 0x80000000) != 0x80000400 ) {
		printf("%s: Warning, SBF is 0x%x, expected 0x80000400\n",
		    sc->sc_dev.dv_xname, val);
		/* May not be a critical failure, just warn for now */
		//return (5);
	}
	/* Verify there are no interrupts active on the core */
	val = BCW_READ(sc, BCW_GIR);
	if (val != 0) {
		DPRINTF(("Failed Pending Interrupt test with val=0x%x\n", val));
		return (6);
	}

	/* Above G means it's unsupported currently, like N */
	if (sc->sc_phy_type > BCW_PHY_TYPEG) {
		DPRINTF(("PHY type %d greater than supported type %d\n",
		    sc->sc_phy_type, BCW_PHY_TYPEG));
		return (7);
	}
	
	return (0);
}

int
bcw_detach(void *arg)
{
	struct bcw_softc *sc = arg;
	struct ieee80211com *ic = &sc->sc_ic;
	struct ifnet *ifp = &ic->ic_if;

	timeout_del(&sc->sc_scan_to);

	bcw_stop(ifp, 1);
	ieee80211_ifdetach(ifp);
	if_detach(ifp);
	bcw_free_rx_ring(sc, &sc->sc_rxring);
	bcw_free_tx_ring(sc, &sc->sc_txring);

	/* power off cardbus socket */
	if (sc->sc_disable)
		sc->sc_disable(sc);

	return (0);
}

#if 0
void
bcw_free_ring(struct bcw_softc *sc, struct bcw_dma_slot *ring)
{
	struct bcw_chain data *data;
	struct bcw_dma_slot *bcwd;
	int i;

	if (sc->bcw_rx_chain != NULL) {
		for (i = 0; i < BCW_NRXDESC; i++) {
			bcwd = &sc->bcw_rx_ring[i];

			if (sc->bcw_rx_chain[i] != NULL) {
				bus_dmamap_sync(sc->sc_dmat,
				    sc->bcw_ring_map,
				    sizeof(struct bcw_dma_slot) * x,
				    sizeof(struct bcw_dma_slot),
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat,
				    sc->bcw_ring_map);
				m_freem(sc->bcw_rx_chain[i]);
			}

			if (sc->bcw_ring_map != NULL)
				bus_dmamap_destroy(sc->sc_dmat,
				    sc->bcw_ring_map);
		}
	}
}
#endif

int
bcw_alloc_rx_ring(struct bcw_softc *sc, struct bcw_rx_ring *ring, int count)
{
	struct bcw_desc *desc;
	struct bcw_rx_data *data;
	int i, nsegs, error;

	ring->count = count;
	ring->cur = ring->next = 0;

	error = bus_dmamap_create(sc->sc_dmat,
	    count * sizeof(struct bcw_desc), 1,
	    count * sizeof(struct bcw_desc), 0,
	    BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    count * sizeof(struct bcw_desc),
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * sizeof(struct bcw_desc), (caddr_t *)&ring->desc,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * sizeof(struct bcw_desc), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	bzero(ring->desc, count * sizeof(struct bcw_desc));
	ring->physaddr = ring->map->dm_segs->ds_addr;

	ring->data = malloc(count * sizeof (struct bcw_rx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	BCW_WRITE(sc, BCW_DMA_RXADDR, ring->physaddr + 0x40000000);

	/*
	 * Pre-allocate Rx buffers and populate Rx ring.
	 */
	bzero(ring->data, count * sizeof (struct bcw_rx_data));
	for (i = 0; i < count; i++) {
		desc = &ring->desc[i];
		data = &ring->data[i];

		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES, 1, MCLBYTES,
		    0, BUS_DMA_NOWAIT, &data->map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		MGETHDR(data->m, M_DONTWAIT, MT_DATA);
		if (data->m == NULL) {
			printf("%s: could not allocate rx mbuf\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		MCLGET(data->m, M_DONTWAIT);
		if (!(data->m->m_flags & M_EXT)) {
			printf("%s: could not allocate rx mbuf cluster\n",
			    sc->sc_dev.dv_xname);
			error = ENOMEM;
			goto fail;
		}

		error = bus_dmamap_load(sc->sc_dmat, data->map,
		    mtod(data->m, void *), MCLBYTES, NULL, BUS_DMA_NOWAIT);
		if (error != 0) {
			printf("%s: could not load rx buf DMA map",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	
		desc->addr = htole32(data->map->dm_segs->ds_addr);

		if (i != (count - 1))
			desc->ctrl = htole32(BCW_RXBUF_LEN);
		else
			desc->ctrl = htole32(BCW_RXBUF_LEN | CTRL_EOT);
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	return (0);

fail:	bcw_free_rx_ring(sc, ring);
	return (error);
}

void
bcw_reset_rx_ring(struct bcw_softc *sc, struct bcw_rx_ring *ring)
{
	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->cur = ring->next = 0;
}

void
bcw_free_rx_ring(struct bcw_softc *sc, struct bcw_rx_ring *ring)
{
	struct bcw_rx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->desc,
		    ring->count * sizeof(struct bcw_desc));
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTREAD);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

int
bcw_alloc_tx_ring(struct bcw_softc *sc, struct bcw_tx_ring *ring,
    int count)
{
	int i, nsegs, error;

	ring->count = count;
	ring->queued = 0;
	ring->cur = ring->next = ring->stat = 0;

	error = bus_dmamap_create(sc->sc_dmat,
	    count * sizeof(struct bcw_desc), 1,
	    count * sizeof(struct bcw_desc), 0, BUS_DMA_NOWAIT, &ring->map);
	if (error != 0) {
		printf("%s: could not create desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_alloc(sc->sc_dmat,
	    count * sizeof(struct bcw_desc),
	    PAGE_SIZE, 0, &ring->seg, 1, &nsegs, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not allocate DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamem_map(sc->sc_dmat, &ring->seg, nsegs,
	    count * sizeof(struct bcw_desc), (caddr_t *)&ring->desc,
	    BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not map desc DMA memory\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	error = bus_dmamap_load(sc->sc_dmat, ring->map, ring->desc,
	    count * sizeof(struct bcw_desc), NULL, BUS_DMA_NOWAIT);
	if (error != 0) {
		printf("%s: could not load desc DMA map\n",
		    sc->sc_dev.dv_xname);
		goto fail;
	}

	memset(ring->desc, 0, count * sizeof(struct bcw_desc));
	ring->physaddr = ring->map->dm_segs->ds_addr;

	/* MAGIC */
	BCW_WRITE(sc, BCW_DMA_TXADDR, ring->physaddr + 0x40000000);

	ring->data = malloc(count * sizeof(struct bcw_tx_data), M_DEVBUF,
	    M_NOWAIT);
	if (ring->data == NULL) {
		printf("%s: could not allocate soft data\n",
		    sc->sc_dev.dv_xname);
		error = ENOMEM;
		goto fail;
	}

	memset(ring->data, 0, count * sizeof(struct bcw_tx_data));
	for (i = 0; i < count; i++) {
		error = bus_dmamap_create(sc->sc_dmat, MCLBYTES,
		    BCW_MAX_SCATTER, MCLBYTES, 0, BUS_DMA_NOWAIT,
		    &ring->data[i].map);
		if (error != 0) {
			printf("%s: could not create DMA map\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	}

	return (0);

fail:	bcw_free_tx_ring(sc, ring);
	return (error);
}

void
bcw_reset_tx_ring(struct bcw_softc *sc, struct bcw_tx_ring *ring)
{
	struct bcw_desc *desc;
	struct bcw_tx_data *data;
	int i;

	for (i = 0; i < ring->count; i++) {
		desc = &ring->desc[i];
		data = &ring->data[i];

		if (data->m != NULL) {
			bus_dmamap_sync(sc->sc_dmat, data->map, 0,
			    data->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
			bus_dmamap_unload(sc->sc_dmat, data->map);
			m_freem(data->m);
			data->m = NULL;
		}

		/*
		 * The node has already been freed at that point so don't call
		 * ieee80211_release_node() here.
		 */
		data->ni = NULL;
	}

	bus_dmamap_sync(sc->sc_dmat, ring->map, 0, ring->map->dm_mapsize,
	    BUS_DMASYNC_PREWRITE);

	ring->queued = 0;
	ring->cur = ring->next = ring->stat = 0;
}

void
bcw_free_tx_ring(struct bcw_softc *sc, struct bcw_tx_ring *ring)
{
	struct bcw_tx_data *data;
	int i;

	if (ring->desc != NULL) {
		bus_dmamap_sync(sc->sc_dmat, ring->map, 0,
		    ring->map->dm_mapsize, BUS_DMASYNC_POSTWRITE);
		bus_dmamap_unload(sc->sc_dmat, ring->map);
		bus_dmamem_unmap(sc->sc_dmat, (caddr_t)ring->desc,
		    ring->count * sizeof(struct bcw_desc));
		bus_dmamem_free(sc->sc_dmat, &ring->seg, 1);
	}

	if (ring->data != NULL) {
		for (i = 0; i < ring->count; i++) {
			data = &ring->data[i];

			if (data->m != NULL) {
				bus_dmamap_sync(sc->sc_dmat, data->map, 0,
				    data->map->dm_mapsize,
				    BUS_DMASYNC_POSTWRITE);
				bus_dmamap_unload(sc->sc_dmat, data->map);
				m_freem(data->m);
			}

			/*
			 * The node has already been freed at that point so
			 * don't call ieee80211_release_node() here.
			 */
			data->ni = NULL;

			if (data->map != NULL)
				bus_dmamap_destroy(sc->sc_dmat, data->map);
		}
		free(ring->data, M_DEVBUF);
	}
}

void
bcw_powercontrol_crystal_on(struct bcw_softc *sc)
{
	uint32_t sbval;

	sbval = (sc->sc_conf_read)(sc->sc_dev_softc, BCW_GPIOI);
	if ((sbval & BCW_XTALPOWERUP) != BCW_XTALPOWERUP) {
		sbval = (sc->sc_conf_read)(sc->sc_dev_softc, BCW_GPIOO);
		sbval |= (BCW_XTALPOWERUP & BCW_PLLPOWERDOWN);
		(sc->sc_conf_write)(sc->sc_dev_softc, BCW_GPIOO, sbval);
		delay(1000);
		sbval = (sc->sc_conf_read)(sc->sc_dev_softc, BCW_GPIOO);
		sbval &= ~BCW_PLLPOWERDOWN;
		(sc->sc_conf_write)(sc->sc_dev_softc, BCW_GPIOO, sbval);
		delay(5000);
	}
}

void
bcw_powercontrol_crystal_off(struct bcw_softc *sc)
{
	uint32_t sbval;

	/* XXX Return if radio is hardware disabled */
	if (sc->sc_chip_rev < 5)
		return;
	if ((sc->sc_boardflags & BCW_BF_XTAL) == BCW_BF_XTAL)
		return;

	/* XXX bcw_powercontrol_clock_slow() */

	sbval = (sc->sc_conf_read)(sc->sc_dev_softc, BCW_GPIOO);
	sbval |= BCW_PLLPOWERDOWN;
	sbval &= ~BCW_XTALPOWERUP;
	(sc->sc_conf_write)(sc->sc_dev_softc, BCW_GPIOO, sbval);
	sbval = (sc->sc_conf_read)(sc->sc_dev_softc, BCW_GPIOE);
	sbval |= BCW_PLLPOWERDOWN | BCW_XTALPOWERUP;
	(sc->sc_conf_write)(sc->sc_dev_softc, BCW_GPIOE, sbval);
}

int
bcw_change_core(struct bcw_softc *sc, int changeto)
{
	uint32_t sbval;
	int i;

	(sc->sc_conf_write)(sc->sc_dev_softc, BCW_ADDR_SPACE0,
	    BCW_CORE_SELECT(changeto));

	/* loop to see if the selected core shows up */
	for (i = 0; i < 10; i++) {
		sbval = (sc->sc_conf_read)(sc->sc_dev_softc, BCW_ADDR_SPACE0);
		if (sbval == BCW_CORE_SELECT(changeto))
			break;
		delay(10);
	}
	if (i == 10) {
		DPRINTF(("%s: can not change to core %d!\n",
		    sc->sc_dev.dv_xname, changeto)); 
		return (1);
	}

	/* core changed */
	sc->sc_lastcore = sc->sc_currentcore;
	sc->sc_currentcore = changeto;

	return (0);
}

int
bcw_reset_core(struct bcw_softc *sc, uint32_t flags)
{
	uint32_t	sbval, reject, val;
	int		i;

	/*
	 * Figure out what revision the Sonic Backplane is, as the position
	 * of the Reject bit changes.
	 */
	switch (sc->sc_sbrev) {
	case 0:
		reject = SBTML_REJ22;
		break;
	case 1:
		reject = SBTML_REJ23;
		break;
	default:
		reject = SBTML_REJ22 | SBTML_REJ23;
	}

	/* disable core if not in reset */
	if (!(sbval & SBTML_RESET)) {
		/* if the core is not enabled, the clock won't be enabled */
		if (!(sbval & SBTML_CLK)) {
			BCW_WRITE(sc, BCW_SBTMSTATELOW,
			    SBTML_RESET | reject | flags);
			delay(1);
			sbval = BCW_READ(sc, BCW_SBTMSTATELOW);
			goto disabled;

			BCW_WRITE(sc, BCW_SBTMSTATELOW, reject);
			delay(1);
			/* wait until busy is clear */
			for (i = 0; i < 10000; i++) {
				val = BCW_READ(sc, BCW_SBTMSTATEHI);
				if (!(val & SBTMH_BUSY))
					break;
				delay(10);
			}
			if (i == 10000)
				printf("%s: while resetting core, busy did "
				    "not clear\n", sc->sc_dev.dv_xname);

			val = BCW_READ(sc, BCW_CIR_SBID_LO);
			if (val & BCW_CIR_SBID_LO_INITIATOR) {
				sbval = BCW_READ(sc, BCW_SBIMSTATE);
				BCW_WRITE(sc, BCW_SBIMSTATE,
				    sbval | SBIM_REJECT);
				sbval = BCW_READ(sc, BCW_SBIMSTATE);
				delay(1);

				/* wait until busy is clear */
				for (i = 0; i < 10000; i++) {
					val = BCW_READ(sc, BCW_SBTMSTATEHI);
					if (!(val & SBTMH_BUSY))
						break;
					delay(10);
				}
				if (i == 10000)
					printf("%s: while resetting core, busy "
					    "did not clear\n",
					    sc->sc_dev.dv_xname);
			} /* end initiator check */

			/* set reset and reject while enabling the clocks */
			/* XXX why isn't reject in here? */
			BCW_WRITE(sc, BCW_SBTMSTATELOW,
			    SBTML_FGC | SBTML_CLK | SBTML_RESET | flags);
			val = BCW_READ(sc, BCW_SBTMSTATELOW);
			delay(10);

			val = BCW_READ(sc, BCW_CIR_SBID_LO);
			if (val & BCW_CIR_SBID_LO_INITIATOR) {
				sbval = BCW_READ(sc, BCW_SBIMSTATE);
				BCW_WRITE(sc, BCW_SBIMSTATE,
				    sbval & ~SBIM_REJECT);
				sbval = BCW_READ(sc, BCW_SBIMSTATE);
				delay(1);

				/* wait until busy is clear */
				for (i = 0; i < 10000; i++) {
					val = BCW_READ(sc, BCW_SBTMSTATEHI);
					if (!(val & SBTMH_BUSY))
						break;
					delay(10);
				}
				if (i == 10000)
					printf("%s: while resetting core, busy "
					    "did not clear\n",
					    sc->sc_dev.dv_xname);
			} /* end initiator check */

			BCW_WRITE(sc, BCW_SBTMSTATELOW,
			    SBTML_RESET | reject | flags);
			delay(1);
		}
	}

disabled:

	/* This is enabling/resetting the core */
	/* enable clock */
	BCW_WRITE(sc, BCW_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | SBTML_RESET | flags);
	val = BCW_READ(sc, BCW_SBTMSTATELOW);
	delay(1);

	/* clear any error bits that may be on */
	val = BCW_READ(sc, BCW_SBTMSTATEHI);
	if (val & SBTMH_SERR)
		BCW_WRITE(sc, BCW_SBTMSTATEHI, 0);
	val = BCW_READ(sc, BCW_SBIMSTATE);
	if (val & (SBIM_INBANDERR | SBIM_TIMEOUT))
		BCW_WRITE(sc, BCW_SBIMSTATE,
		    val & ~(SBIM_INBANDERR | SBIM_TIMEOUT));

	/* clear reset and allow it to propagate throughout the core */
	BCW_WRITE(sc, BCW_SBTMSTATELOW,
	    SBTML_FGC | SBTML_CLK | flags);
	val = BCW_READ(sc, BCW_SBTMSTATELOW);
	delay(1);

	/* leave clock enabled */
	BCW_WRITE(sc, BCW_SBTMSTATELOW, SBTML_CLK | flags);
	val = BCW_READ(sc, BCW_SBTMSTATELOW);
	delay(1);

	return 0;
}

int
bcw_get_firmware(const char *name, const uint8_t *ucode, size_t size_ucode,
    size_t *size, size_t *offset)
{
	int i, nfiles, off = 0, ret = 1;
	struct fwheader *h;

	if ((h = malloc(sizeof(struct fwheader), M_DEVBUF, M_NOWAIT)) == NULL)
		return (ret);

	/* get number of firmware files */
	bcopy(ucode, &nfiles, sizeof(nfiles));
	nfiles = ntohl(nfiles);
	off += sizeof(nfiles);

	/* parse header and search the firmware */
	for (i = 0; i < nfiles && off < size_ucode; i++) {
		bzero(h, sizeof(struct fwheader));
		bcopy(ucode + off, h, sizeof(struct fwheader));
		off += sizeof(struct fwheader);

		if (strcmp(name, h->filename) == 0) {
			ret = 0;
			*size = ntohl(h->filesize);
			*offset = ntohl(h->fileoffset);
			break;
		}
	}

	free(h, M_DEVBUF);

	return (ret);
}

int
bcw_load_firmware(struct bcw_softc *sc)
{
	int rev = sc->sc_core[sc->sc_currentcore].rev;
	int error, len, i;
	uint32_t *data;
	uint8_t *ucode;
	size_t size_ucode, size_micro, size_pcm, off_micro, off_pcm;
	char *name = "bcw-bcm43xx";
	char filename[64];

	/* load firmware */
	if ((error = loadfirmware(name, &ucode, &size_ucode)) != 0) {
		printf("%s: error %d, could not read microcode %s!\n",
		    sc->sc_dev.dv_xname, error, name);
		return (EIO);
	}
	DPRINTF(("%s: successfully read %s\n", sc->sc_dev.dv_xname, name));

	/* get microcode file offset */
	snprintf(filename, sizeof(filename), "bcm43xx_microcode%d.fw",
	    rev >= 5 ? 5 : rev);

	if (bcw_get_firmware(filename, ucode, size_ucode, &size_micro,
	    &off_micro) != 0) {
		printf("%s: get offset for firmware file %s failed!\n",
		    sc->sc_dev.dv_xname, filename);
		goto fail;
	}

	/* get pcm file offset */
	snprintf(filename, sizeof(filename), "bcm43xx_pcm%d.fw",
	    rev < 5 ? 4 : 5);

	if (bcw_get_firmware(filename, ucode, size_ucode, &size_pcm,
	    &off_pcm) != 0) {
		printf("%s: get offset for firmware file %s failed!\n",
		    sc->sc_dev.dv_xname, filename);
		goto fail;
	}

	/* upload microcode */
	data = (uint32_t *)(ucode + off_micro);
	len = size_micro / sizeof(uint32_t);
	bcw_shm_ctl_word(sc, BCW_SHM_CONTROL_MCODE, 0);
	for (i = 0; i < len; i++) {
		BCW_WRITE(sc, BCW_SHM_DATA, betoh32(data[i]));
		delay(10);
	}
	DPRINTF(("%s: uploaded microcode\n", sc->sc_dev.dv_xname));

	/* upload pcm */
	data = (uint32_t *)(ucode + off_pcm);
	len = size_pcm / sizeof(uint32_t);
	bcw_shm_ctl_word(sc, BCW_SHM_CONTROL_PCM, 0x01ea);
	BCW_WRITE(sc, BCW_SHM_DATA, 0x00004000);
	bcw_shm_ctl_word(sc, BCW_SHM_CONTROL_PCM, 0x01eb);
	for (i = 0; i < len; i++) {
		BCW_WRITE(sc, BCW_SHM_DATA, betoh32(data[i]));
		delay(10);
	}
	DPRINTF(("%s: uploaded pcm\n", sc->sc_dev.dv_xname));

	free(ucode, M_DEVBUF);

	return (0);

fail:	free(ucode, M_DEVBUF);
	return (EIO);
}

int
bcw_write_initvals(struct bcw_softc *sc, const struct bcw_initval *data,
    const unsigned int len)
{
	int i;
	uint16_t offset, size;
	uint32_t value;

	for (i = 0; i < len; i++) {
		offset = betoh16(data[i].offset);
		size = betoh16(data[i].size);
		value = betoh32(data[i].value);

		if (offset >= 0x1000)
			goto bad_format;
		if (size == 2) {
			if (value & 0xffff0000)
				goto bad_format;
			BCW_WRITE16(sc, offset, (uint16_t)value);
		} else if (size == 4)
			BCW_WRITE(sc, offset, value);
		else
			goto bad_format;
	}

	return (0);

bad_format:
	printf("%s: initvals file-format error!\n", sc->sc_dev.dv_xname);
	return (EIO);
}

int
bcw_load_initvals(struct bcw_softc *sc)
{
	int rev = sc->sc_core[sc->sc_currentcore].rev;
	int error, nr;
	uint32_t val;
	uint8_t *ucode;
	size_t size_ucode, size_ival0, size_ival1, off_ival0, off_ival1;
	char *name = "bcw-bcm43xx";
	char filename[64];

	/* load firmware */
	if ((error = loadfirmware(name, &ucode, &size_ucode)) != 0) {
		printf("%s: error %d, could not read microcode %s!\n",
		    sc->sc_dev.dv_xname, error, name);
		return (EIO);
	}
	DPRINTF(("%s: successfully read %s\n", sc->sc_dev.dv_xname, name));

	/* get initval0 file offset */
	if (rev == 2 || rev == 4) {
		switch (sc->sc_phy_type) {
		case BCW_PHY_TYPEA:
			nr = 3;
			break;
		case BCW_PHY_TYPEB:
		case BCW_PHY_TYPEG:
			nr = 1;
			break;
		default:
			printf("%s: no initvals available!\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	} else if (rev >= 5) {
		switch (sc->sc_phy_type) {
		case BCW_PHY_TYPEA:
			nr = 7;
			break;
		case BCW_PHY_TYPEB:
		case BCW_PHY_TYPEG:
			nr = 5;
			break;
		default:
			printf("%s: no initvals available!\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}
	} else {
		printf("%s: no initvals available!\n", sc->sc_dev.dv_xname);
		goto fail;
	}

	snprintf(filename, sizeof(filename), "bcm43xx_initval%02d.fw", nr);

	if (bcw_get_firmware(filename, ucode, size_ucode, &size_ival0,
	    &off_ival0) != 0) {
		printf("%s: get offset for initval0 file %s failed\n",
		    sc->sc_dev.dv_xname, filename);
		goto fail;
	}

	/* get initval1 file offset */
	if (rev >= 5) {
		switch (sc->sc_phy_type) {
		case BCW_PHY_TYPEA:
			val = BCW_READ(sc, BCW_SBTMSTATEHI);
			if (val & 0x00010000)
				nr = 9;
			else
				nr = 10;
			break;
		case BCW_PHY_TYPEB:
		case BCW_PHY_TYPEG:
			nr = 6;
			break;
		default:
			printf("%s: no initvals available!\n",
			    sc->sc_dev.dv_xname);
			goto fail;
		}

		snprintf(filename, sizeof(filename), "bcm43xx_initval%02d.fw",
		    nr);

		if (bcw_get_firmware(filename, ucode, size_ucode, &size_ival1,
		    &off_ival1) != 0) {
			printf("%s: get offset for initval1 file %s failed\n",
			    sc->sc_dev.dv_xname, filename);
			goto fail;
		}
	}

	/* upload initval0 */
	if (bcw_write_initvals(sc, (struct bcw_initval *)(ucode + off_ival0),
	    size_ival0 / sizeof(struct bcw_initval)))
		goto fail;
	DPRINTF(("%s: uploaded initval0\n", sc->sc_dev.dv_xname));

	/* upload initval1 */
	if (off_ival1 != 0) {
		if (bcw_write_initvals(sc,
		    (struct bcw_initval *)(ucode + off_ival1),
		    size_ival1 / sizeof(struct bcw_initval)))
			goto fail;
		DPRINTF(("%s: uploaded initval1\n", sc->sc_dev.dv_xname));
	}

	free(ucode, M_DEVBUF);

	return (0);

fail:	free(ucode, M_DEVBUF);
	return (EIO);
}

void
bcw_leds_switch_all(struct bcw_softc *sc, int on)
{
	struct bcw_led *led;
	uint16_t ledctl;
	int i, bit_on;

	ledctl = BCW_READ16(sc, BCW_MMIO_GPIO_CONTROL);

	for (i = 0; i < BCW_NR_LEDS; i++) {
		led = &(sc->leds[i]);
		if (led->behaviour == BCW_LED_INACTIVE)
			continue;
		if (on)
			bit_on = led->activelow ? 0 : 1;
		else
			bit_on = led->activelow ? 0 : 1;
		if (bit_on)
			ledctl |= (1 << i);
		else
			ledctl &= ~(1 << i);
	}

	BCW_WRITE16(sc, BCW_MMIO_GPIO_CONTROL, ledctl);
}

int
bcw_gpio_init(struct bcw_softc *sc)
{
	uint32_t mask, set;
	int error = 0;

	BCW_WRITE(sc, BCW_SBF, BCW_READ(sc, BCW_SBF) & 0xffff3fff);

	bcw_leds_switch_all(sc, 0);

	BCW_WRITE16(sc, BCW_GPIO_MASK, BCW_READ16(sc, BCW_GPIO_MASK) | 0x000f);

	mask = 0x0000001f;
	set = 0x0000000f;

	if (sc->sc_chip_id == 0x4301) {
		mask |= 0x0060;
		set |= 0x0060;
	}
	if (0) { /* FIXME conditional unknown */
		BCW_WRITE16(sc, BCW_GPIO_MASK, BCW_READ16(sc, BCW_GPIO_MASK) |
		    0x0100);
		mask |= 0x0180;
		set |= 0x0180;
	}
	if (sc->sc_boardflags & BCW_BF_PACTRL) {
		BCW_WRITE16(sc, BCW_GPIO_MASK, BCW_READ16(sc, BCW_GPIO_MASK) |
		    0x0200);
		mask |= 0x0200;
		set |= 0x0200;
	}
	if (sc->sc_chip_rev >= 2)
		mask |= 0x0010; /* FIXME this is redundant */

	/*
	 * TODO bcw_change_core_to_gpio()
	 *
	 * Where to find the GPIO register depends on the chipset.
	 * If it has a ChipCommon, its register at offset 0x6c is the GPIO
	 * control register. Otherwise the register at offset 0x6c in the
	 * PCI core is the GPIO control register.
	 */
	if ((error = bcw_change_core(sc, 0)))
		return (error);

	BCW_WRITE(sc, BCW_GPIO_CTRL, (BCW_READ(sc, BCW_GPIO_CTRL) & mask) |
	    set);

	error = bcw_change_core(sc, sc->sc_lastcore);

	return (error);
}

/*
 * PHY
 */
int
bcw_phy_init(struct bcw_softc *sc)
{
	int error = ENODEV;

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		if (sc->sc_phy_rev == 2 || sc->sc_phy_rev == 3) {
			error = 0;
		}
		break;
	case BCW_PHY_TYPEB:
		switch (sc->sc_phy_rev) {
		case 2:
			bcw_phy_initb2(sc);
			error = 0;
			break;
		case 4:
			bcw_phy_initb4(sc);
			error = 0;
			break;
		case 5:
			bcw_phy_initb5(sc);
			error = 0;
			break;
		case 6:
			bcw_phy_initb6(sc);
			error = 0;
			break;
		}
		break;
	case BCW_PHY_TYPEG:
		bcw_phy_initg(sc);
		error = 0;
		DPRINTF(("%s: PHY type G initialized\n", sc->sc_dev.dv_xname));
		break;
	}

	if (error)
		printf("%s: PHY type unknown!\n", sc->sc_dev.dv_xname);

	return (error);
}

void
bcw_phy_initg(struct bcw_softc *sc)
{
	uint16_t tmp;

	if (sc->sc_phy_rev == 1)
		bcw_phy_initb5(sc);
	else
		bcw_phy_initb6(sc);
	if (sc->sc_phy_rev >= 2 || sc->sc_phy_connected)
		bcw_phy_inita(sc);

	if (sc->sc_phy_rev >= 2) {
		bcw_phy_write16(sc, 0x0814, 0);
		bcw_phy_write16(sc, 0x0815, 0);
		if (sc->sc_phy_rev == 2)
			bcw_phy_write16(sc, 0x0811, 0x0000);
		else if (sc->sc_phy_rev >= 3)
			bcw_phy_write16(sc, 0x0811, 0x0400);
		bcw_phy_write16(sc, 0x0015, 0x00c0);
		if (sc->sc_phy_connected) {
			tmp = bcw_phy_read16(sc, 0x0400) & 0xff;
			if (tmp < 6) {
				bcw_phy_write16(sc, 0x04c2, 0x1816);
				bcw_phy_write16(sc, 0x04c3, 0x8006);
				if (tmp != 3)
					bcw_phy_write16(sc, 0x04cc,
					    (bcw_phy_read16(sc, 0x04cc) &
					    0x00ff) | 0x1f00);
			}
		}
	}

	if (sc->sc_phy_rev < 3 && sc->sc_phy_connected)
		bcw_phy_write16(sc, 0x047e, 0x0078);
	if (sc->sc_phy_rev >= 6 && sc->sc_phy_rev <= 8) {
		bcw_phy_write16(sc, 0x0801, bcw_phy_read16(sc, 0x0801) |
		    0x0080);
		bcw_phy_write16(sc, 0x043e, bcw_phy_read16(sc, 0x043e) |
		    0x0004);
	}
	if (sc->sc_phy_rev >= 2 && sc->sc_phy_connected)
		bcw_phy_calc_loopback_gain(sc);
	if (sc->sc_radio_rev != 8) {
		if (sc->sc_radio_initval == 0xffff)
			sc->sc_radio_initval = bcw_radio_init2050(sc);
		else
			bcw_radio_write16(sc, 0x0078, sc->sc_radio_initval);
	}
	if (sc->sc_radio_txctl2 == 0xffff)
		bcw_phy_lo_g_measure(sc);
	else {
		if (sc->sc_radio_ver == 0x2050 && sc->sc_radio_rev == 8)
			bcw_radio_write16(sc, 0x0052,
			    (sc->sc_radio_txctl1 << 4) | sc->sc_radio_txctl2);
		else
			bcw_radio_write16(sc, 0x0052,
			    (bcw_radio_read16(sc, 0x0052) & 0xfff0) |
			    sc->sc_radio_txctl1);
		if (sc->sc_phy_rev >= 6)
			bcw_phy_write16(sc, 0x0036,
			    (bcw_phy_read16(sc, 0x0036) & 0xf000) |
			    (sc->sc_radio_txctl2 << 12));
		if (sc->sc_boardflags & BCW_BF_PACTRL)
			bcw_phy_write16(sc, 0x002e, 0x8075);
		else
			bcw_phy_write16(sc, 0x002e, 0x807f);
		if (sc->sc_phy_rev < 2)
			bcw_phy_write16(sc, 0x002f, 0x0101);
		else
			bcw_phy_write16(sc, 0x002f, 0x0202);
	}
	if (sc->sc_phy_connected) {
		bcw_phy_lo_adjust(sc, 0);
		bcw_phy_write16(sc, 0x080f, 0x8078);
	}

	if (!(sc->sc_boardflags & BCW_BF_RSSI)) {
		bcw_radio_nrssi_hw_update(sc, 0xffff);
		bcw_radio_calc_nrssi_threshold(sc);
	} else if (sc->sc_phy_connected) {
		if (sc->sc_radio_nrssi[0] == -1000) {
			/* XXX assert() */
			bcw_radio_calc_nrssi_slope(sc);
		} else {
			/* XXX assert() */
			bcw_radio_calc_nrssi_threshold(sc);
		}
	}

	if (sc->sc_radio_rev == 8)
		bcw_phy_write16(sc, 0x0805, 0x3230);
	bcw_phy_init_pctl(sc);
	if (sc->sc_chip_id == 0x4306 && sc->sc_chip_rev == 2) {
		bcw_phy_write16(sc, 0x0429, bcw_phy_read16(sc, 0x0429) &
		    0xbfff);
		bcw_phy_write16(sc, 0x04c3, bcw_phy_read16(sc, 0x04c3) &
		    0x7fff);
	}
}

void
bcw_phy_initb5(struct bcw_softc *sc)
{
	uint16_t offset;

	if (sc->sc_phy_rev == 1 && sc->sc_radio_rev == 0x2050)
		bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) |
		    0x0050);

	if (sc->sc_board_vendor != PCI_VENDOR_BROADCOM &&
	    sc->sc_board_type != 0x0416) {
		for (offset = 0x00a8; offset < 0x00c7; offset++)
			bcw_phy_write16(sc, offset,
			    (bcw_phy_read16(sc, offset) + 0x02020) & 0x3f3f);
	}

	bcw_phy_write16(sc, 0x0035, (bcw_phy_read16(sc, 0x0035) & 0xf0ff) |
	    0x0700);

	if (sc->sc_radio_rev == 0x2050)
		bcw_phy_write16(sc, 0x0038, 0x0667);

	if (sc->sc_phy_connected) {
		if (sc->sc_radio_rev == 0x2050) {
			bcw_radio_write16(sc, 0x007a,
			    bcw_radio_read16(sc, 0x007a) | 0x0020);
			bcw_radio_write16(sc, 0x0051,
			    bcw_radio_read16(sc, 0x0051) | 0x0004);
		}
		BCW_WRITE16(sc, BCW_MMIO_PHY_RADIO, 0);
		bcw_phy_write16(sc, 0x0802, bcw_phy_read16(sc, 0x0802) |
		    0x0100);
		bcw_phy_write16(sc, 0x042b, bcw_phy_read16(sc, 0x042b) |
		    0x2000);
		bcw_phy_write16(sc, 0x001c, 0x186a);
		bcw_phy_write16(sc, 0x0013,
		    (bcw_phy_read16(sc, 0x0013) & 0x00ff) | 0x1900);
		bcw_phy_write16(sc, 0x0035,
		    (bcw_phy_read16(sc, 0x0035) & 0xffc0) | 0x0064);
		bcw_phy_write16(sc, 0x005d,
		    (bcw_phy_read16(sc, 0x005d) & 0xff80) | 0x000a);
	}

	if (sc->sc_phy_rev == 1 && sc->sc_radio_rev == 0x2050) {
		bcw_phy_write16(sc, 0x0026, 0xce00);
		bcw_phy_write16(sc, 0x0021, 0x3763);
		bcw_phy_write16(sc, 0x0022, 0x1bc3);
		bcw_phy_write16(sc, 0x0023, 0x06f9);
		bcw_phy_write16(sc, 0x0024, 0x037e);
	} else
		bcw_phy_write16(sc, 0x0026, 0xcc00);
	bcw_phy_write16(sc, 0x0030, 0x00c6);
	BCW_WRITE16(sc, 0x3ec, 0x3f22);

	if (sc->sc_phy_rev == 1 && sc->sc_radio_rev == 0x2050)
		bcw_phy_write16(sc, 0x0020, 0x3e1c);
	else
		bcw_phy_write16(sc, 0x0020, 0x301c);

	if (sc->sc_phy_rev == 0)
		BCW_WRITE16(sc, 0x03e4, 0x3000);

	/* force to channel 7, even if not supported */
	bcw_radio_select_channel(sc, 7, 0);

	if (sc->sc_radio_rev != 0x2050) {
		bcw_radio_write16(sc, 0x0075, 0x0080);
		bcw_radio_write16(sc, 0x0079, 0x0081);
	}

	bcw_radio_write16(sc, 0x0050, 0x0020);
	bcw_radio_write16(sc, 0x0050, 0x0023);

	if (sc->sc_radio_rev == 0x2050) {
		bcw_radio_write16(sc, 0x0050, 0x0020);
		bcw_radio_write16(sc, 0x005a, 0x0070);
	}

	bcw_radio_write16(sc, 0x005b, 0x007b);
	bcw_radio_write16(sc, 0x005c, 0x00b0);

	bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) | 0x0007);

	bcw_radio_select_channel(sc, BCW_RADIO_DEFAULT_CHANNEL_BG, 0);

	bcw_phy_write16(sc, 0x0014, 0x0080);
	bcw_phy_write16(sc, 0x0032, 0x00ca);
	bcw_phy_write16(sc, 0x88a3, 0x002a);

	bcw_radio_set_txpower_bg(sc, 0xffff, 0xffff, 0xffff);

	if (sc->sc_radio_rev == 0x2050)
		bcw_radio_write16(sc, 0x005d, 0x000d);

	BCW_WRITE16(sc, 0x03e4, (BCW_READ16(sc, 0x03e4) & 0xffc0) | 0x0004);
}

void
bcw_phy_initb2(struct bcw_softc *sc)
{
	uint16_t offset, val;

	BCW_WRITE16(sc, 0x03ec, 0x3f22);
	bcw_phy_write16(sc, 0x0020, 0x301c);
	bcw_phy_write16(sc, 0x0026, 0x0000);
	bcw_phy_write16(sc, 0x0030, 0x00c6);
	bcw_phy_write16(sc, 0x0088, 0x3e00);
	val = 0x3c3d;
	for (offset = 0x0089; offset < 0x00a7; offset++) {
		bcw_phy_write16(sc, offset, val);
		val -= 0x0202;
	}
	bcw_phy_write16(sc, 0x03e4, 0x3000);
	if (sc->sc_radio_channel == 0xff)
		bcw_radio_select_channel(sc, BCW_RADIO_DEFAULT_CHANNEL_BG, 0);
	else
		bcw_radio_select_channel(sc, sc->sc_radio_channel, 0);
	if (sc->sc_radio_ver != 0x2050) {
		bcw_radio_write16(sc, 0x0075, 0x0080);
		bcw_radio_write16(sc, 0x0079, 0x0081);
	}
	bcw_radio_write16(sc, 0x0050, 0x0020);
	bcw_radio_write16(sc, 0x0050, 0x0023);
	if (sc->sc_radio_ver == 0x2050) {
		bcw_radio_write16(sc, 0x0050, 0x0020);
		bcw_radio_write16(sc, 0x005a, 0x0070);
		bcw_radio_write16(sc, 0x005b, 0x007b);
		bcw_radio_write16(sc, 0x005c, 0x00b0);
		bcw_radio_write16(sc, 0x007a, 0x000f);
		bcw_phy_write16(sc, 0x0038, 0x0677);
		bcw_radio_init2050(sc);
	}
	bcw_phy_write16(sc, 0x0014, 0x0080);
	bcw_phy_write16(sc, 0x0032, 0x00ca);
	bcw_phy_write16(sc, 0x0032, 0x00cc);
	bcw_phy_write16(sc, 0x0035, 0x07c2);
	bcw_phy_lo_b_measure(sc);
	bcw_phy_write16(sc, 0x0026, 0xcc00);
	if (sc->sc_radio_ver != 0x2050)
		bcw_phy_write16(sc, 0x0026, 0xce00);
	BCW_WRITE16(sc, BCW_MMIO_CHANNEL_EXT, 0x1000);
	bcw_phy_write16(sc, 0x002a, 0x88a3);
	bcw_radio_set_txpower_bg(sc, 0xffff, 0xffff, 0xffff);
	bcw_phy_init_pctl(sc);
}

void
bcw_phy_initb4(struct bcw_softc *sc)
{
	uint16_t offset, val;

	BCW_WRITE16(sc, 0x03ec, 0x3f22);
	bcw_phy_write16(sc, 0x0020, 0x301c);
	bcw_phy_write16(sc, 0x0026, 0x0000);
	bcw_phy_write16(sc, 0x0030, 0x00c6);
	bcw_phy_write16(sc, 0x0088, 0x3e00);
	val = 0x3c3d;
	for (offset = 0x0089; offset < 0x00a7; offset++) {
		bcw_phy_write16(sc, offset, val);
		val -= 0x0202;
	}
	bcw_phy_write16(sc, 0x3e4, 0x3000);
	if (sc->sc_radio_channel == 0xff)
		bcw_radio_select_channel(sc, BCW_RADIO_DEFAULT_CHANNEL_BG, 0);
	else
		bcw_radio_select_channel(sc, sc->sc_radio_channel, 0);
	if (sc->sc_radio_ver == 0x2050) {
		bcw_radio_write16(sc, 0x0050, 0x0020);
		bcw_radio_write16(sc, 0x005a, 0x0070);
		bcw_radio_write16(sc, 0x005b, 0x007b);
		bcw_radio_write16(sc, 0x005c, 0x00b0);
		bcw_radio_write16(sc, 0x007a, 0x000f);
		bcw_phy_write16(sc, 0x0038, 0x0677);
		bcw_radio_init2050(sc);
	}
	bcw_phy_write16(sc, 0x0014, 0x0080);
	bcw_phy_write16(sc, 0x0032, 0x00ca);
	if (sc->sc_radio_ver == 0x2050)
		bcw_phy_write16(sc, 0x0032, 0x00e0);
	bcw_phy_write16(sc, 0x0035, 0x07c2);

	bcw_phy_lo_b_measure(sc);

	bcw_phy_write16(sc, 0x0026, 0xcc00);
	if (sc->sc_radio_ver == 0x2050)
		bcw_phy_write16(sc, 0x0026, 0xce00);
	BCW_WRITE16(sc, BCW_MMIO_CHANNEL_EXT, 0x1100);
	bcw_phy_write16(sc, 0x002a, 0x88c2);
	if (sc->sc_radio_ver == 0x2050)
		bcw_phy_write16(sc, 0x002a, 0x88c2);
	bcw_radio_set_txpower_bg(sc, 0xffff, 0xffff, 0xffff);
	if (sc->sc_boardflags & BCW_BF_RSSI) {
		bcw_radio_calc_nrssi_slope(sc);
		bcw_radio_calc_nrssi_threshold(sc);
	}
	bcw_phy_init_pctl(sc);
}

void
bcw_phy_initb6(struct bcw_softc *sc)
{
	uint16_t offset, val;

	bcw_phy_write16(sc, 0x003e, 0x817a);
	bcw_radio_write16(sc, 0x007a, (bcw_radio_read16(sc, 0x007a) | 0x0058));

	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2050 &&
	    (sc->sc_radio_rev == 3 || sc->sc_radio_rev == 4 ||
	    sc->sc_radio_rev == 5)) {
		bcw_radio_write16(sc, 0x0051, 0x001f);
		bcw_radio_write16(sc, 0x0052, 0x0040);
		bcw_radio_write16(sc, 0x0053, 0x005b);
		bcw_radio_write16(sc, 0x0054, 0x0098);
		bcw_radio_write16(sc, 0x005a, 0x0088);
		bcw_radio_write16(sc, 0x005b, 0x0088);
		bcw_radio_write16(sc, 0x005d, 0x0088);
		bcw_radio_write16(sc, 0x005e, 0x0088);
		bcw_radio_write16(sc, 0x007d, 0x0088);
	}

	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2050 &&
	    sc->sc_radio_rev == 6) {
		bcw_radio_write16(sc, 0x0051, 0);
		bcw_radio_write16(sc, 0x0052, 0x0040);
		bcw_radio_write16(sc, 0x0053, 0x00b7);
		bcw_radio_write16(sc, 0x0054, 0x0098);
		bcw_radio_write16(sc, 0x005a, 0x0088);
		bcw_radio_write16(sc, 0x005b, 0x008b);
		bcw_radio_write16(sc, 0x005c, 0x00b5);
		bcw_radio_write16(sc, 0x005d, 0x0088);
		bcw_radio_write16(sc, 0x005e, 0x0088);
		bcw_radio_write16(sc, 0x007d, 0x0088);
		bcw_radio_write16(sc, 0x007c, 0x0001);
		bcw_radio_write16(sc, 0x007e, 0x0008);
	}

	if (sc->sc_radio_mnf == 0x017f && sc->sc_radio_ver == 0x2050 &&
	    sc->sc_radio_rev == 7) {
		bcw_radio_write16(sc, 0x0051, 0);
		bcw_radio_write16(sc, 0x0052, 0x0040);
		bcw_radio_write16(sc, 0x0053, 0x00b7);
		bcw_radio_write16(sc, 0x0054, 0x0098);
		bcw_radio_write16(sc, 0x005a, 0x0088);
		bcw_radio_write16(sc, 0x005b, 0x00a8);
		bcw_radio_write16(sc, 0x005c, 0x0075);
		bcw_radio_write16(sc, 0x005d, 0x00f5);
		bcw_radio_write16(sc, 0x005e, 0x00b8);
		bcw_radio_write16(sc, 0x007d, 0x00e8);
		bcw_radio_write16(sc, 0x007c, 0x0001);
		bcw_radio_write16(sc, 0x007e, 0x0008);
		bcw_radio_write16(sc, 0x007b, 0);
	}

	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2050 &&
	    sc->sc_radio_rev == 8) {
		bcw_radio_write16(sc, 0x0051, 0);
		bcw_radio_write16(sc, 0x0052, 0x0040);
		bcw_radio_write16(sc, 0x0053, 0x00b7);
		bcw_radio_write16(sc, 0x0054, 0x0098);
		bcw_radio_write16(sc, 0x005a, 0x0088);
		bcw_radio_write16(sc, 0x005b, 0x006b);
		bcw_radio_write16(sc, 0x005c, 0x000f);
		if (sc->sc_boardflags & 0x8000) {
			bcw_radio_write16(sc, 0x005d, 0x00fa);
			bcw_radio_write16(sc, 0x005e, 0x00d8);
		} else {
			bcw_radio_write16(sc, 0x005d, 0x00f5);
			bcw_radio_write16(sc, 0x005e, 0x00b8);
		}
		bcw_radio_write16(sc, 0x0073, 0x0003);
		bcw_radio_write16(sc, 0x007d, 0x00a8);
		bcw_radio_write16(sc, 0x007c, 0x0001);
		bcw_radio_write16(sc, 0x007e, 0x0008);
	}

	val = 0x1e1f;
	for (offset = 0x0088; offset < 0x0098; offset++) {
		bcw_phy_write16(sc, offset, val);
		val -= 0x0202;
	}
	val = 0x3e3f;
	for (offset = 0x0098; offset < 0x00a8; offset++) {
		bcw_phy_write16(sc, offset, val);
		val -= 0x0202;
	}
	val = 0x2120;
	for (offset = 0x00a8; offset < 0x00c8; offset++) {
		bcw_phy_write16(sc, offset, (val & 0x3f3f));
		val += 0x0202;
	}

	if (sc->sc_phy_type == BCW_PHY_TYPEG) {
		bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) |
		    0x0020);
		bcw_radio_write16(sc, 0x0051, bcw_radio_read16(sc, 0x0051) |
		    0x0004);
		bcw_phy_write16(sc, 0x0802, bcw_phy_read16(sc, 0x0802) |
		    0x0100);
		bcw_phy_write16(sc, 0x042b, bcw_phy_read16(sc, 0x042b) |
		    0x2000);
	}

	/* force to channel 7, even if not supported */
	bcw_radio_select_channel(sc, 7, 0);

	bcw_radio_write16(sc, 0x0050, 0x0020);
	bcw_radio_write16(sc, 0x0050, 0x0023);
	delay(40);
	bcw_radio_write16(sc, 0x007c, (bcw_radio_read16(sc, 0x007c) |
	    0x0002));
	bcw_radio_write16(sc, 0x0050, 0x0020);
	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2050 &&
	    sc->sc_radio_rev <= 2) {
		bcw_radio_write16(sc, 0x0050, 0x0020);
		bcw_radio_write16(sc, 0x005a, 0x0070);
		bcw_radio_write16(sc, 0x005b, 0x007b);
		bcw_radio_write16(sc, 0x005c, 0x00b0);
	}
	bcw_radio_write16(sc, 0x007a, (bcw_radio_read16(sc, 0x007a) & 0x00f8) |
	    0x0007);

	bcw_radio_select_channel(sc, BCW_RADIO_DEFAULT_CHANNEL_BG, 0);
	
	bcw_phy_write16(sc, 0x0014, 0x0200);
	if (sc->sc_radio_ver == 0x2050) {
		if (sc->sc_radio_rev == 3 || sc->sc_radio_rev == 4 ||
		    sc->sc_radio_rev == 5)
			bcw_phy_write16(sc, 0x002a, 0x8ac0);
		else
			bcw_phy_write16(sc, 0x002a, 0x88c2);
	}
	bcw_phy_write16(sc, 0x0038, 0x0668);
	bcw_radio_set_txpower_bg(sc, 0xffff, 0xffff, 0xffff);
	if (sc->sc_radio_ver == 0x2050) {
		if (sc->sc_radio_rev == 3 || sc->sc_radio_rev == 4 ||
		    sc->sc_radio_rev == 5)
			bcw_phy_write16(sc, 0x005d,
			    bcw_phy_read16(sc, 0x005d) | 0x0003);
		else if (sc->sc_radio_rev <= 2)
			bcw_phy_write16(sc, 0x005d, 0x000d);
	}

	if (sc->sc_phy_rev == 4)
		bcw_phy_write16(sc, 0x0002, (bcw_phy_read16(sc, 0x0002) &
		    0xffc0) | 0x0004);
	else
		BCW_WRITE16(sc, 0x03e4, 0x0009);
	if (sc->sc_phy_type == BCW_PHY_TYPEB) {
		BCW_WRITE16(sc, 0x03e6, 0x8140);
		bcw_phy_write16(sc, 0x0016, 0x0410);
		bcw_phy_write16(sc, 0x0017, 0x0820);
		bcw_phy_write16(sc, 0x0062, 0x0007);
		(void)bcw_radio_calibrationvalue(sc);
		bcw_phy_lo_b_measure(sc);
		if (sc->sc_boardflags & BCW_BF_RSSI) {
			bcw_radio_calc_nrssi_slope(sc);
			bcw_radio_calc_nrssi_threshold(sc);
		}
		bcw_phy_init_pctl(sc);
	} else
		BCW_WRITE16(sc, 0x03e6, 0);
}

void
bcw_phy_inita(struct bcw_softc *sc)
{
	uint16_t tval;

	if (sc->sc_phy_type == BCW_PHY_TYPEA)
		bcw_phy_setupa(sc);
	else {
		bcw_phy_setupg(sc);
		if (sc->sc_boardflags & BCW_BF_PACTRL)
			bcw_phy_write16(sc, 0x046e, 0x03cf);
		return;
	}

	bcw_phy_write16(sc, BCW_PHY_A_CRS, (bcw_phy_read16(sc, BCW_PHY_A_CRS) &
	    0xf83c) | 0x0340);
	bcw_phy_write16(sc, 0x0034, 0x0001);

	bcw_phy_write16(sc, BCW_PHY_A_CRS, bcw_phy_read16(sc, BCW_PHY_A_CRS) |
	    (1 << 14));
	bcw_radio_init2060(sc);

	if (sc->sc_board_vendor == PCI_VENDOR_BROADCOM &&
	    (sc->sc_board_type == 0x0416 || sc->sc_board_type == 0x040a)) {
		if (sc->sc_radio_lofcal == 0xffff)
			bcw_radio_set_tx_iq(sc);
		else
			bcw_radio_write16(sc, 0x001e, sc->sc_radio_lofcal);
	}

	bcw_phy_write16(sc, 0x007a, 0xf111);

	if (sc->sc_phy_savedpctlreg == 0xffff) {
		bcw_radio_write16(sc, 0x0019, 0x0000);
		bcw_radio_write16(sc, 0x0017, 0x0020);

		tval = bcw_ilt_read(sc, 0x3001);
		if (sc->sc_phy_rev == 1)
			bcw_ilt_write(sc, 0x3001, (bcw_ilt_read(sc, 0x3001) &
			    0xff87) | 0x0058);
		else
			bcw_ilt_write(sc, 0x3001, (bcw_ilt_read(sc, 0x3001) &
			    0xffc3) | 0x002c);
		bcw_dummy_transmission(sc);
		sc->sc_phy_savedpctlreg = bcw_phy_read16(sc, BCW_PHY_A_PCTL);
		bcw_ilt_write(sc, 0x3001, tval);

		bcw_radio_set_txpower_a(sc, 0x0018);
	}
	bcw_radio_clear_tssi(sc);
}

void
bcw_phy_setupa(struct bcw_softc *sc)
{
	uint16_t i;

	switch (sc->sc_phy_rev) {
	case 2:
		bcw_phy_write16(sc, 0x008e, 0x3800);
		bcw_phy_write16(sc, 0x0035, 0x03ff);
		bcw_phy_write16(sc, 0x0036, 0x0400);

		bcw_ilt_write(sc, 0x3807, 0x0051);

		bcw_phy_write16(sc, 0x001c, 0x0ff9);
		bcw_phy_write16(sc, 0x0020, bcw_phy_read16(sc, 0x0020) &
		    0xff0f);
		bcw_ilt_write(sc, 0x3c0c, 0x07bf);
		bcw_radio_write16(sc, 0x0002, 0x07bf);

		bcw_phy_write16(sc, 0x0024, 0x4680);
		bcw_phy_write16(sc, 0x0020, 0x0003);
		bcw_phy_write16(sc, 0x001d, 0x0f40);
		bcw_phy_write16(sc, 0x001f, 0x1c00);

		bcw_phy_write16(sc, 0x002a, (bcw_phy_read16(sc, 0x002a) &
		    0x00ff) | 0x0400);
		bcw_phy_write16(sc, 0x002b, bcw_phy_read16(sc, 0x002b) &
		    0xfbff);
		bcw_phy_write16(sc, 0x008e, 0x58c1);

		bcw_ilt_write(sc, 0x0803, 0x000f);
		bcw_ilt_write(sc, 0x0804, 0x001f);
		bcw_ilt_write(sc, 0x0805, 0x002a);
		bcw_ilt_write(sc, 0x0805, 0x0030);
		bcw_ilt_write(sc, 0x0807, 0x003a);

		bcw_ilt_write(sc, 0x0000, 0x0013);
		bcw_ilt_write(sc, 0x0001, 0x0013);
		bcw_ilt_write(sc, 0x0002, 0x0013);
		bcw_ilt_write(sc, 0x0003, 0x0013);
		bcw_ilt_write(sc, 0x0004, 0x0015);
		bcw_ilt_write(sc, 0x0005, 0x0015);
		bcw_ilt_write(sc, 0x0006, 0x0019);

		bcw_ilt_write(sc, 0x0404, 0x0003);
		bcw_ilt_write(sc, 0x0405, 0x0003);
		bcw_ilt_write(sc, 0x0406, 0x0007);

		for (i = 0; i < 16; i++)
			bcw_ilt_write(sc, 0x4000 + i, (0x8 + i) & 0x000f);

		bcw_ilt_write(sc, 0x3003, 0x1044);
		bcw_ilt_write(sc, 0x3004, 0x7201);
		bcw_ilt_write(sc, 0x3006, 0x0040);
		bcw_ilt_write(sc, 0x3001, (bcw_ilt_read(sc, 0x3001) & 0x0010) |
		    0x0008);

		for (i = 0; i < BCW_ILT_FINEFREQA_SIZE; i++)
			bcw_ilt_write(sc, 0x5800 + i, bcw_ilt_finefreqa[i]);

		for (i = 0; i < BCW_ILT_NOISEA2_SIZE; i++)
			bcw_ilt_write(sc, 0x1800 + i, bcw_ilt_noisea2[i]);

		for (i = 0; i < BCW_ILT_ROTOR_SIZE; i++)
			bcw_ilt_write(sc, 0x2000 + i, bcw_ilt_rotor[i]);

		bcw_phy_init_noisescaletbl(sc);
		for (i = 0; i < BCW_ILT_RETARD_SIZE; i++)
			bcw_ilt_write(sc, 0x2400 + i, bcw_ilt_retard[i]);

		break;
	case 3:
		for (i = 0; i< 64; i++)
			bcw_ilt_write(sc, 0x4000 + i, i);

		bcw_ilt_write(sc, 0x3807, 0x0051);

		bcw_phy_write16(sc, 0x001c, 0x0ff9);
		bcw_phy_write16(sc, 0x0020, bcw_phy_read16(sc, 0x0020) &
		    0x0ff0f);
		bcw_radio_write16(sc, 0x0002, 0x07bf);

		bcw_phy_write16(sc, 0x0024, 0x4680);
		bcw_phy_write16(sc, 0x0020, 0x0003);
		bcw_phy_write16(sc, 0x001d, 0x0f40);
		bcw_phy_write16(sc, 0x001f, 0x1c00);
		bcw_phy_write16(sc, 0x002a, (bcw_phy_read16(sc, 0x002a) &
		    0x00ff) | 0x0400);
		bcw_ilt_write(sc, 0x3001, (bcw_ilt_read(sc, 0x3001) &
		    0x0010) | 0x0008);
		for (i = 0; i < BCW_ILT_NOISEA3_SIZE; i++)
			bcw_ilt_write(sc, 0x1800 + i, bcw_ilt_noisea3[i]);

		bcw_phy_init_noisescaletbl(sc);
		for (i = 0; i < BCW_ILT_SIGMASQR_SIZE; i++)
			bcw_ilt_write(sc, 0x5000 + i, bcw_ilt_sigmasqr1[i]);

		bcw_phy_write16(sc, 0x0003, 0x1808);

		bcw_ilt_write(sc, 0x0803, 0x000f);
		bcw_ilt_write(sc, 0x0804, 0x001f);
		bcw_ilt_write(sc, 0x0805, 0x002a);
		bcw_ilt_write(sc, 0x0805, 0x0030);
		bcw_ilt_write(sc, 0x0807, 0x003a);

		bcw_ilt_write(sc, 0x0000, 0x0013);
		bcw_ilt_write(sc, 0x0001, 0x0013);
		bcw_ilt_write(sc, 0x0002, 0x0013);
		bcw_ilt_write(sc, 0x0003, 0x0013);
		bcw_ilt_write(sc, 0x0004, 0x0015);
		bcw_ilt_write(sc, 0x0005, 0x0015);
		bcw_ilt_write(sc, 0x0006, 0x0019);

		bcw_ilt_write(sc, 0x0404, 0x0003);
		bcw_ilt_write(sc, 0x0405, 0x0003);
		bcw_ilt_write(sc, 0x0406, 0x0007);

		bcw_ilt_write(sc, 0x3c02, 0x000f);
		bcw_ilt_write(sc, 0x3c03, 0x0014);
		break;
	default:
		/* XXX assert? */
		break;
	}
}

void
bcw_phy_setupg(struct bcw_softc *sc)
{
	uint16_t i;

	/* XXX assert? */

	if (sc->sc_phy_rev == 1) {
		bcw_phy_write16(sc, 0x0406, 0x4f19);
		bcw_phy_write16(sc, BCW_PHY_G_CRS, (bcw_phy_read16(sc,
		    BCW_PHY_G_CRS) & 0xfc3f) | 0x0340);
		bcw_phy_write16(sc, 0x042c, 0x005a);
		bcw_phy_write16(sc, 0x0427, 0x001a);

		for (i = 0; i < BCW_ILT_FINEFREQG_SIZE; i++)
			bcw_ilt_write(sc, 0x5800 + i, bcw_ilt_finefreqg[i]);

		for (i = 0; i < BCW_ILT_NOISEG1_SIZE; i++)
			bcw_ilt_write(sc, 0x1800 + i, bcw_ilt_noiseg1[i]);

		for (i = 0; i < BCW_ILT_ROTOR_SIZE; i++)
			bcw_ilt_write(sc, 0x2000 + i, bcw_ilt_rotor[i]);
	} else {
		bcw_radio_nrssi_hw_write(sc, 0xba98, (int16_t) 0x7654);

		if (sc->sc_phy_type == 2) {
			bcw_phy_write16(sc, 0x04c0, 0x1861);
			bcw_phy_write16(sc, 0x04c1, 0x0271);
		} else {
			bcw_phy_write16(sc, 0x04c0, 0x0098);
			bcw_phy_write16(sc, 0x04c1, 0x0070);
			bcw_phy_write16(sc, 0x04c9, 0x0080);
		}
		bcw_phy_write16(sc, 0x042b, bcw_phy_read16(sc, 0x042b) |
		    0x0800);

		for (i = 0; i < 64; i++)
			bcw_ilt_write(sc, 0x4000 + i, i);
		for (i = 0; i < BCW_ILT_NOISEG2_SIZE; i++)
			bcw_ilt_write(sc, 0x1800 + i, bcw_ilt_noiseg2[i]);
	}

	if (sc->sc_phy_rev <= 2)
		for (i = 0; i < BCW_ILT_NOISESCALEG_SIZE; i++)
			bcw_ilt_write(sc, 0x1400 + i, bcw_ilt_noisescaleg1[i]);
	else if ((sc->sc_phy_rev >= 7) && (bcw_phy_read16(sc, 0x0449) & 0x0200))
		for (i = 0; i < BCW_ILT_NOISESCALEG_SIZE; i++)
			bcw_ilt_write(sc, 0x1400 + i, bcw_ilt_noisescaleg3[i]);
	else
		for (i = 0; i < BCW_ILT_NOISESCALEG_SIZE; i++)
			bcw_ilt_write(sc, 0x1400 + i, bcw_ilt_noisescaleg2[i]);

	if (sc->sc_phy_rev == 2)
		for (i = 0; i < BCW_ILT_SIGMASQR_SIZE; i++)
			bcw_ilt_write(sc, 0x5000 + i, bcw_ilt_sigmasqr1[i]);
	else if ((sc->sc_phy_rev > 2) && (sc->sc_phy_rev <= 8))
		for (i = 0; i < BCW_ILT_SIGMASQR_SIZE; i++)
			bcw_ilt_write(sc, 0x5000 + i, bcw_ilt_sigmasqr2[i]);

	if (sc->sc_phy_rev == 1) {
		for (i = 0; i < BCW_ILT_RETARD_SIZE; i++)
			bcw_ilt_write(sc, 0x2400 + i, bcw_ilt_retard[i]);
		for (i = 0; i < 4; i++) {
			bcw_ilt_write(sc, 0x5404 + i, 0x0020);
			bcw_ilt_write(sc, 0x5408 + i, 0x0020);
			bcw_ilt_write(sc, 0x540c + i, 0x0020);
			bcw_ilt_write(sc, 0x5410 + i, 0x0020);
		}
		bcw_phy_agcsetup(sc);

		if (sc->sc_board_vendor == PCI_VENDOR_BROADCOM &&
		    sc->sc_board_type == 0x0416 &&
		    sc->sc_board_rev == 0x0017)
			return;

		bcw_ilt_write(sc, 0x5001, 0x0002);
		bcw_ilt_write(sc, 0x5002, 0x0001);
	} else {
		for (i = 0; i <= 0x2f; i++)
			bcw_ilt_write(sc, 0x1000 +  i, 0x0820);
		bcw_phy_agcsetup(sc);
		bcw_phy_read16(sc, 0x0400);
		bcw_phy_write16(sc, 0x0403, 0x1000);
		bcw_ilt_write(sc, 0x3c02, 0x000f);
		bcw_ilt_write(sc, 0x3c03, 0x0014);

		if (sc->sc_board_vendor == PCI_VENDOR_BROADCOM &&
		    sc->sc_board_type == 0x0416 &&
		    sc->sc_board_rev == 0x0017)
			return;

		bcw_ilt_write(sc, 0x0401, 0x0002);
		bcw_ilt_write(sc, 0x0402, 0x0001);
	}
}

void
bcw_phy_calc_loopback_gain(struct bcw_softc *sc)
{
	uint16_t backup_phy[15];
	uint16_t backup_radio[3];
	uint16_t backup_bband;
	uint16_t i;
	uint16_t loop1_cnt, loop1_done, loop1_omitted;
	uint16_t loop2_done;

	backup_phy[0]  = bcw_phy_read16(sc, 0x0429);
	backup_phy[1]  = bcw_phy_read16(sc, 0x0001);
	backup_phy[2]  = bcw_phy_read16(sc, 0x0811);
	backup_phy[3]  = bcw_phy_read16(sc, 0x0812);
	backup_phy[4]  = bcw_phy_read16(sc, 0x0814);
	backup_phy[5]  = bcw_phy_read16(sc, 0x0815);
	backup_phy[6]  = bcw_phy_read16(sc, 0x005a);
	backup_phy[7]  = bcw_phy_read16(sc, 0x0059);
	backup_phy[8]  = bcw_phy_read16(sc, 0x0058);
	backup_phy[9]  = bcw_phy_read16(sc, 0x000a);
	backup_phy[10] = bcw_phy_read16(sc, 0x0003);
	backup_phy[11] = bcw_phy_read16(sc, 0x080f);
	backup_phy[12] = bcw_phy_read16(sc, 0x0810);
	backup_phy[13] = bcw_phy_read16(sc, 0x002b);
	backup_phy[14] = bcw_phy_read16(sc, 0x0015);
	bcw_phy_read16(sc, 0x002d);
	backup_bband = sc->sc_radio_baseband_atten;
	backup_radio[0] = bcw_radio_read16(sc, 0x0052);
	backup_radio[1] = bcw_radio_read16(sc, 0x0043);
	backup_radio[2] = bcw_radio_read16(sc, 0x007a);

	bcw_phy_write16(sc, 0x0429, bcw_phy_read16(sc, 0x0429) & 0x3fff);
	bcw_phy_write16(sc, 0x0001, bcw_phy_read16(sc, 0x0001) & 0x8000);
	bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) & 0x0002);
	bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) & 0xfffd);
	bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) & 0x0001);
	bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) & 0xfffe);
	bcw_phy_write16(sc, 0x0814, bcw_phy_read16(sc, 0x0814) & 0x0001);
	bcw_phy_write16(sc, 0x0815, bcw_phy_read16(sc, 0x0815) & 0xfffe);
	bcw_phy_write16(sc, 0x0814, bcw_phy_read16(sc, 0x0814) & 0x0002);
	bcw_phy_write16(sc, 0x0815, bcw_phy_read16(sc, 0x0815) & 0xfffd);
	bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) & 0x000c);
	bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) & 0x000c);

	bcw_phy_write16(sc, 0x0811, (bcw_phy_read16(sc, 0x0811) & 0xffcf) |
	    0x0030);
	bcw_phy_write16(sc, 0x0812, (bcw_phy_read16(sc, 0x0812) & 0xffcf) |
	    0x0010);

	bcw_phy_write16(sc, 0x005a, 0x0780);
	bcw_phy_write16(sc, 0x0059, 0xc810);
	bcw_phy_write16(sc, 0x0058, 0x000d);
	if (sc->sc_phy_ver == 9)
		bcw_phy_write16(sc, 0x0003, 0x0122);
	else
		bcw_phy_write16(sc, 0x000a, bcw_phy_read16(sc, 0x000a) |
		    0x2000);
	bcw_phy_write16(sc, 0x0814, bcw_phy_read16(sc, 0x0814) | 0x0004);
	bcw_phy_write16(sc, 0x0815, bcw_phy_read16(sc, 0x0815) & 0xfffb);
	bcw_phy_write16(sc, 0x0003, (bcw_phy_read16(sc, 0x0003) & 0xff9f) |
	    0x0040);
	if (sc->sc_radio_ver == 0x2050 && sc->sc_radio_rev == 2) {
		bcw_radio_write16(sc, 0x0052, 0x0000);
		bcw_radio_write16(sc, 0x0043, (bcw_radio_read16(sc, 0x0043) &
		    0xfff0) | 0x0009);
		loop1_cnt = 9;
	} else if (sc->sc_radio_rev == 8) {
		bcw_radio_write16(sc, 0x0043, 0x000f);
		loop1_cnt = 15;
	} else
		loop1_cnt = 0;

	bcw_phy_set_baseband_atten(sc, 11);

	if (sc->sc_phy_rev >= 3)
		bcw_phy_write16(sc, 0x080f, 0xc020);
	else
		bcw_phy_write16(sc, 0x080f, 0x8020);
	bcw_phy_write16(sc, 0x0810, 0x0000);

	bcw_phy_write16(sc, 0x002b, (bcw_phy_read16(sc, 0x002b) & 0xffc0) |
	    0x0001);
	bcw_phy_write16(sc, 0x002b, (bcw_phy_read16(sc, 0x002b) & 0xc0ff) |
	    0x0800);
	bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) | 0x0100);
	bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) | 0xcfff);
	if (sc->sc_boardflags & BCW_BF_EXTLNA) {
		if (sc->sc_phy_rev >= 7) {
			bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) |
			    0x0800);
			bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) |
			    0x8000);
		}
	}
	bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) & 0x00f7);

	for (i = 0; i < loop1_cnt; i++) {
		bcw_radio_write16(sc, 0x0043, loop1_cnt);
		bcw_phy_write16(sc, 0x0812, (bcw_phy_read16(sc, 0x0812) &
		    0xf0ff) | (i << 8));
		bcw_phy_write16(sc, 0x0015, (bcw_phy_read16(sc, 0x0015) &
		    0x0fff) | 0xa000);
		bcw_phy_write16(sc, 0x0015, (bcw_phy_read16(sc, 0x0015) &
		    0x0fff) | 0xf000);
		delay(20);
		if (bcw_phy_read16(sc, 0x0002d) >= 0x0dfc)
			break;
	}
	loop1_done = i;
	loop1_omitted = loop1_cnt - loop1_done;

	loop2_done = 0;
	if (loop1_done >= 8) {
		bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) |
		    0x0030);
		for (i = loop1_done - 8; i < 16; i++) {
			bcw_phy_write16(sc, 0x0812,
			    (bcw_phy_read16(sc, 0x0812) & 0xf0ff) | (i << 8));
			bcw_phy_write16(sc, 0x0015,
			    (bcw_phy_read16(sc, 0x0015) & 0x0fff) | 0xa000);
			bcw_phy_write16(sc, 0x0015,
			    (bcw_phy_read16(sc, 0x0015) & 0x0fff) | 0xf000);
			delay(20);
			if (bcw_phy_read16(sc, 0x002d) >= 0x0dfc)
				break;
		}
	}

	bcw_phy_write16(sc, 0x0814, backup_phy[4]);
	bcw_phy_write16(sc, 0x0815, backup_phy[5]);
	bcw_phy_write16(sc, 0x005a, backup_phy[6]);
	bcw_phy_write16(sc, 0x0059, backup_phy[7]);
	bcw_phy_write16(sc, 0x0058, backup_phy[8]);
	bcw_phy_write16(sc, 0x000a, backup_phy[9]);
	bcw_phy_write16(sc, 0x0003, backup_phy[10]);
	bcw_phy_write16(sc, 0x080f, backup_phy[11]);
	bcw_phy_write16(sc, 0x0810, backup_phy[12]);
	bcw_phy_write16(sc, 0x002b, backup_phy[13]);
	bcw_phy_write16(sc, 0x0015, backup_phy[14]);

	bcw_phy_set_baseband_atten(sc, backup_bband);

	bcw_radio_write16(sc, 0x0052, backup_radio[0]);
	bcw_radio_write16(sc, 0x0043, backup_radio[1]);
	bcw_radio_write16(sc, 0x007a, backup_radio[2]);

	bcw_phy_write16(sc, 0x0811, backup_phy[2] | 0x0003);
	delay(10);
	bcw_phy_write16(sc, 0x0811, backup_phy[2]);
	bcw_phy_write16(sc, 0x0812, backup_phy[3]);
	bcw_phy_write16(sc, 0x0429, backup_phy[0]);
	bcw_phy_write16(sc, 0x0001, backup_phy[1]);

	sc->sc_phy_loopback_gain[0] = ((loop1_done * 6) - (loop1_omitted * 4))
	    - 11;
	sc->sc_phy_loopback_gain[1] = (24 - (3 * loop2_done)) * 2; 
}

void
bcw_phy_agcsetup(struct bcw_softc *sc)
{
	uint16_t offset = 0;

	if (sc->sc_phy_rev == 1)
		offset = 0x4c00;

	bcw_ilt_write(sc, offset, 0x00fe);
	bcw_ilt_write(sc, offset + 1, 0x000d);
	bcw_ilt_write(sc, offset + 2, 0x0013);
	bcw_ilt_write(sc, offset + 3, 0x0019);

	if (sc->sc_phy_rev == 1) {
		bcw_ilt_write(sc, 0x1800, 0x2710);
		bcw_ilt_write(sc, 0x1801, 0x9b83);
		bcw_ilt_write(sc, 0x1802, 0x9b83);
		bcw_ilt_write(sc, 0x1803, 0x0f8d);
		bcw_phy_write16(sc, 0x0455, 0x0004);
	}

	bcw_phy_write16(sc, 0x04a5, (bcw_phy_read16(sc, 0x04a5) & 0x00ff) |
	    0x5700);
	bcw_phy_write16(sc, 0x041a, (bcw_phy_read16(sc, 0x041a) & 0xff80) |
	    0x000f);
	bcw_phy_write16(sc, 0x041a, (bcw_phy_read16(sc, 0x041a) & 0xc07f) |
	    0x2b80);
	bcw_phy_write16(sc, 0x048c, (bcw_phy_read16(sc, 0x048c) & 0xf0ff) |
	    0x0300);

	bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) | 0x0008);

	bcw_phy_write16(sc, 0x04a0, (bcw_phy_read16(sc, 0x04a0) & 0xfff0) |
	    0x0008);
	bcw_phy_write16(sc, 0x04a1, (bcw_phy_read16(sc, 0x04a1) & 0xf0ff) |
	    0x0600);
	bcw_phy_write16(sc, 0x04a2, (bcw_phy_read16(sc, 0x04a2) & 0xf0ff) |
	    0x0700);
	bcw_phy_write16(sc, 0x04a0, (bcw_phy_read16(sc, 0x04a0) & 0xf0ff) |
	    0x0100);

	if (sc->sc_phy_rev == 1)
		bcw_phy_write16(sc, 0x04a2, (bcw_phy_read16(sc, 0x4a2) &
		    0xfff0) | 0x0007);

	bcw_phy_write16(sc, 0x0488, (bcw_phy_read16(sc, 0x0488) & 0xff00) |
	    0x001c);
	bcw_phy_write16(sc, 0x0488, (bcw_phy_read16(sc, 0x0488) & 0xc0ff) |
	    0x0200);
	bcw_phy_write16(sc, 0x0496, (bcw_phy_read16(sc, 0x0496) & 0xff00) |
	    0x001c);
	bcw_phy_write16(sc, 0x0489, (bcw_phy_read16(sc, 0x0489) & 0xff00) |
	    0x0020);
	bcw_phy_write16(sc, 0x0489, (bcw_phy_read16(sc, 0x0489) & 0xc0ff) |
	    0x0200);
	bcw_phy_write16(sc, 0x0482, (bcw_phy_read16(sc, 0x0482) & 0xff00) |
	    0x001c);
	bcw_phy_write16(sc, 0x0496, (bcw_phy_read16(sc, 0x0496) & 0x00ff) |
	    0x1a00);
	bcw_phy_write16(sc, 0x0481, (bcw_phy_read16(sc, 0x0481) & 0xff00) |
	    0x0028);
	bcw_phy_write16(sc, 0x0481, (bcw_phy_read16(sc, 0x0481) & 0x00ff) |
	    0x2c00);

	if (sc->sc_phy_rev == 1) {
		bcw_phy_write16(sc, 0x0430, 0x092b);
		bcw_phy_write16(sc, 0x041b, (bcw_phy_read16(sc, 0x041b) &
		    0xffe1) | 0x0002);
	} else {
		bcw_phy_write16(sc, 0x041b, bcw_phy_read16(sc, 0x41b) &
		    0xffe1);
		bcw_phy_write16(sc, 0x041f, 0x287a);
		bcw_phy_write16(sc, 0x0420, (bcw_phy_read16(sc, 0x0420) &
		    0xfff0) | 0x0004);
	}

	if (sc->sc_phy_rev > 2) {
		bcw_phy_write16(sc, 0x0422, 0x287a);
		bcw_phy_write16(sc, 0x0420, (bcw_phy_read16(sc, 0x0420) &
		   0x0fff) | 0x3000);
	}

	bcw_phy_write16(sc, 0x04a8, (bcw_phy_read16(sc, 0x04a8) & 0x8080) |
	    0x7874);
	bcw_phy_write16(sc, 0x048e, 0x1c00);

	if (sc->sc_phy_rev == 1) {
		bcw_phy_write16(sc, 0x04ab, (bcw_phy_read16(sc, 0x04ab) &
		    0xf0ff) | 0x0600);
		bcw_phy_write16(sc, 0x048b, 0x005e);
		bcw_phy_write16(sc, 0x048c, (bcw_phy_read16(sc, 0x048c) &
		    0xff00) | 0x001e);
		bcw_phy_write16(sc, 0x048d, 0x0002);
	}

	bcw_ilt_write(sc, offset + 0x0800, 0);
	bcw_ilt_write(sc, offset + 0x0801, 7);
	bcw_ilt_write(sc, offset + 0x0802, 16);
	bcw_ilt_write(sc, offset + 0x0803, 28);
}

void
bcw_phy_init_pctl(struct bcw_softc *sc)
{
	uint16_t saved_batt = 0, saved_ratt = 0, saved_txctl1 = 0;
	int must_reset_txpower = 0;

	/* XXX assert() */

	if (sc->sc_board_vendor == PCI_VENDOR_BROADCOM &&
	    sc->sc_board_type == 0x0416)
		return;

	BCW_WRITE16(sc, 0x03e6, BCW_READ16(sc, 0x03e6) & 0xffdf);
	bcw_phy_write16(sc, 0x0028, 0x8018);

	if (sc->sc_phy_type == BCW_PHY_TYPEG) {
		if (!sc->sc_phy_connected)
			return;
		bcw_phy_write16(sc, 0x047a, 0xc111);
	}
	if (sc->sc_phy_savedpctlreg != 0xffff)
		return;

	if (sc->sc_phy_type == BCW_PHY_TYPEB &&
	    sc->sc_phy_rev >= 2 &&
	    sc->sc_radio_ver == 0x2050) {
		bcw_radio_write16(sc, 0x0076, bcw_radio_read16(sc, 0x0076) |
		    0x0084);
	} else {
		saved_batt = sc->sc_radio_baseband_atten;
		saved_ratt = sc->sc_radio_radio_atten;
		saved_txctl1 = sc->sc_radio_txctl1;
		if ((sc->sc_radio_rev >= 6) && (sc->sc_radio_rev <= 8))
			bcw_radio_set_txpower_bg(sc, 0xb, 0x1f, 0);
		else
			bcw_radio_set_txpower_bg(sc, 0xb, 9, 0);
		must_reset_txpower = 1;
	}
	bcw_dummy_transmission(sc);

	sc->sc_phy_savedpctlreg = bcw_phy_read16(sc, BCW_PHY_G_PCTL);

	if (must_reset_txpower)
		bcw_radio_set_txpower_bg(sc, saved_batt, saved_ratt,
		    saved_txctl1);
	else
		bcw_radio_write16(sc, 0x0076, bcw_radio_read16(sc, 0x0076) &
		    0xff7b);
	bcw_radio_clear_tssi(sc);
}

void
bcw_phy_init_noisescaletbl(struct bcw_softc *sc)
{
	int i;

	bcw_phy_write16(sc, BCW_PHY_ILT_A_CTRL, 0x1400);
	for (i = 0; i < 12; i++) {
		if (sc->sc_phy_rev == 2)
			bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, 0x6767);
		else
			bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, 0x2323);
	}

	if (sc->sc_phy_rev == 2)
		bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, 0x6700);
	else
		bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, 0x2300);

	for (i = 0; i < 11; i++) {
		if (sc->sc_phy_rev == 2)
			bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, 0x6767);
		else
			bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, 0x0023);
	}

	if (sc->sc_phy_rev == 2)
		bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, 0x0067);
	else
		bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, 0x0023);
}

void
bcw_phy_set_baseband_atten(struct bcw_softc *sc,
    uint16_t baseband_atten)
{
	uint16_t val;

	if (sc->sc_phy_ver == 0) {
		val = (BCW_READ16(sc, 0x03e6) & 0xfff0);
		val |= (baseband_atten & 0x000f);
		BCW_WRITE16(sc, 0x03e6, val);
		return;
	}

	if (sc->sc_phy_ver > 1) {
		val = bcw_phy_read16(sc, 0x0060) & ~0x003c;
		val |= (baseband_atten << 2) & 0x003c;
	} else {
		val = bcw_phy_read16(sc, 0x0060) & ~0x0078;
		val |= (baseband_atten << 3) & 0x0078;
	}
	bcw_phy_write16(sc, 0x0060, val);
}

int8_t
bcw_phy_estimate_powerout(struct bcw_softc *sc, int8_t tssi)
{
	int8_t dbm = 0;
	int32_t tmp;

	tmp = sc->sc_phy_idle_tssi;
	tmp += tssi;
	tmp -= sc->sc_phy_savedpctlreg;

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		tmp += 0x80;
		tmp = bcw_lv(tmp, 0x00, 0xff);
		//dbm = sc->sc_phy_tssi2dbm[tmp]; /* XXX */
		break;
	case BCW_PHY_TYPEB:
	case BCW_PHY_TYPEG:
		tmp = bcw_lv(tmp, 0x00, 0x3f);
		//dbm = sc->sc_phy_tssi2dbm[tmp]; /* XXX "/
		break;
	default:
		/* XXX assert() */
		break;
	}

	return (dbm);
}

void
bcw_phy_xmitpower(struct bcw_softc *sc)
{
	if (sc->sc_phy_savedpctlreg == 0xffff)
		return;
	if (sc->sc_board_type == 0x0416 &&
	    sc->sc_board_vendor == PCI_VENDOR_BROADCOM)
		return;

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		/* nohting todo for A PHYs yet */
		break;
	case BCW_PHY_TYPEB:
	case BCW_PHY_TYPEG: {
		uint16_t tmp;
		uint16_t txpower;
		int8_t v0, v1, v2, v3;
		int8_t average;
		uint8_t max_pwr;
		int16_t desired_pwr, estimated_pwr, pwr_adjust;
		int16_t radio_att_delta, baseband_att_delta;
		int16_t radio_attenuation, baseband_attenuation;

		tmp = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, 0x0058);
		v0 = (int8_t)(tmp & 0x00ff);
		v1 = (int8_t)((tmp & 0xff00) >> 8);
		tmp = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, 0x005a);
		v2 = (int8_t)(tmp & 0x00ff);
		v3 = (int8_t)((tmp & 0xff00) >> 8);
		tmp = 0;

		if (v0 == 0x7f || v1 == 0x7f || v2 == 0x7f || v3 == 0x07) {
			tmp = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED,
			    0x0070);
			v0 = (int8_t)(tmp & 0x00ff);
			v1 = (int8_t)((tmp & 0xff00) >> 8);
			tmp = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED,
			    0x0072);

			v2 = (int8_t)(tmp & 0x00ff);
			v3 = (int8_t)((tmp & 0xff00) >> 8);
			if (v0 == 0x7f || v1 == 0x7f || v2 == 0x7f ||
			    v3 == 0x7f)
				return;
			v0 = (v0 + 0x20) & 0x3f;
			v1 = (v1 + 0x20) & 0x3f;
			v2 = (v2 + 0x20) & 0x3f;
			v3 = (v3 + 0x20) & 0x3f;
			tmp = 1;
		}
		bcw_radio_clear_tssi(sc);

		average = (v0 + v1 + v2 + v3 + 2) / 4;

		if (tmp && (bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, 0x005e) &
		    0x8))
			average -= 13;

		bcw_phy_estimate_powerout(sc, average);

		if ((sc->sc_boardflags & BCW_BF_PACTRL) &&
		    (sc->sc_phy_type == BCW_PHY_TYPEG))
			max_pwr -= 0x3;

		desired_pwr = bcw_lv(sc->sc_radio_txpower_desired, 0, max_pwr);
		/* check if we need to adjust the current power */
		pwr_adjust = desired_pwr - estimated_pwr;
		radio_att_delta = -(pwr_adjust + 7) >> 3;
		baseband_att_delta = -(pwr_adjust >> 1) - (4 * radio_att_delta);
		if ((radio_att_delta == 0) && (baseband_att_delta == 0)) {
			bcw_phy_lo_mark_current_used(sc);
			return;
		}

		/* calculate the new attenuation values */
		baseband_attenuation = sc->sc_radio_baseband_atten;
		baseband_attenuation += baseband_att_delta;
		radio_attenuation = sc->sc_radio_radio_atten;
		radio_attenuation += radio_att_delta;

		if (radio_attenuation < 0) {
			baseband_attenuation -= (4 * -radio_attenuation);
			radio_attenuation = 0;
		} else if (radio_attenuation > 9) {
			baseband_attenuation += (4 * (radio_attenuation - 9));
			radio_attenuation = 9;
		} else {
			while (baseband_attenuation < 0 &&
			    radio_attenuation > 0) {
				baseband_attenuation += 4;
				radio_attenuation--;
			}
			while (baseband_attenuation > 11 &&
			    radio_attenuation < 9) {
				baseband_attenuation -= 4;
				radio_attenuation++;
			}
		}
		baseband_attenuation = bcw_lv(baseband_attenuation, 0, 11);

		txpower = sc->sc_radio_txctl1;
		if ((sc->sc_radio_ver == 0x02050) && (sc->sc_radio_rev == 2)) {
			if (radio_attenuation <= 1) {
				if (txpower == 0) {
					txpower = 3;
					radio_attenuation += 2;
					baseband_attenuation += 2;
				} else if (sc->sc_boardflags & BCW_BF_PACTRL) {
					baseband_attenuation += 4 *
					    (radio_attenuation - 2);
					radio_attenuation = 2;
				}
			} else if (radio_attenuation > 4 && txpower != 0) {
				txpower = 0;
				if (baseband_attenuation < 3) {
					radio_attenuation -= 3;
					baseband_attenuation += 2;
				} else {
					radio_attenuation -= 2;
					baseband_attenuation -= 2;
				}
			}
		}
		sc->sc_radio_txctl1 = txpower;
		baseband_attenuation = bcw_lv(baseband_attenuation, 0, 11);
		radio_attenuation = bcw_lv(radio_attenuation, 0, 9);

		/* TODO bcw_phy_lock() */
		/* TODO bcw_radio_lock() */
		bcw_radio_set_txpower_bg(sc, baseband_attenuation,
		    radio_attenuation, txpower);
		bcw_phy_lo_mark_current_used(sc);
		/* TODO bcw_radio_unlock() */
		/* TODO bcw_phy_unlock() */
		break;
	}
	default:
		/* XXX assert() */
		break;
	}
}

uint16_t
bcw_phy_lo_b_r15_loop(struct bcw_softc *sc)
{
	int i;
	uint16_t r = 0;

	/* XXX splnet() ? */
	for (i = 0; i < 10; i++) {
		bcw_phy_write16(sc, 0x0015, 0xafa0);
		delay(1);
		bcw_phy_write16(sc, 0x0015, 0xefa0);
		delay(10);
		bcw_phy_write16(sc, 0x0015, 0xffa0);
		delay(40);
		r += bcw_phy_read16(sc, 0x002c);
	}
	/* XXX splnet() ? */
	/* XXX bcm43xx_voluntary_preempt() ? */

	return (r);
}

void
bcw_phy_lo_b_measure(struct bcw_softc *sc)
{
	uint16_t regstack[12] = { 0 };
	uint16_t mls;
	uint16_t fval;
	int i, j;

	regstack[0] = bcw_phy_read16(sc, 0x0015);
	regstack[1] = bcw_radio_read16(sc, 0x0052) & 0xfff0;

	if (sc->sc_radio_ver == 0x2053) {
		regstack[2]  = bcw_phy_read16(sc, 0x000a);
		regstack[3]  = bcw_phy_read16(sc, 0x002a);
		regstack[4]  = bcw_phy_read16(sc, 0x0035);
		regstack[5]  = bcw_phy_read16(sc, 0x0003);
		regstack[6]  = bcw_phy_read16(sc, 0x0001);
		regstack[7]  = bcw_phy_read16(sc, 0x0030);

		regstack[8]  = bcw_radio_read16(sc, 0x0043);
		regstack[9]  = bcw_radio_read16(sc, 0x007a);
		regstack[10] = BCW_READ16(sc, 0x03ec);
		regstack[11] = bcw_radio_read16(sc, 0x0052) & 0x00f0;

		bcw_phy_write16(sc, 0x0030, 0x00ff);
		BCW_WRITE16(sc, 0x3ec, 0x3f3f);
		bcw_phy_write16(sc, 0x0035, regstack[4] & 0xff7f);
		bcw_radio_write16(sc, 0x007a, regstack[9] & 0xfff0);
	}
	bcw_phy_write16(sc, 0x0015, 0xb000);
	bcw_phy_write16(sc, 0x002b, 0x0004);

	if (sc->sc_radio_ver == 0x2053) {
		bcw_phy_write16(sc, 0x002b, 0x0203);
		bcw_phy_write16(sc, 0x002a, 0x08a3);
	}

	sc->sc_phy_minlowsig[0] = 0xffff;

	for (i = 0; i < 4; i++) {
		bcw_radio_write16(sc, 0x0052, regstack[1] | i);
		bcw_phy_lo_b_r15_loop(sc);
	}
	for (i = 0; i < 10; i++) {
		bcw_radio_write16(sc, 0x0052, regstack[1] | i);
		mls = bcw_phy_lo_b_r15_loop(sc) / 10;
		if (mls < sc->sc_phy_minlowsig[0]) {
			sc->sc_phy_minlowsig[0] = mls;
			sc->sc_phy_minlowsigpos[0] = i;
		}
	}
	bcw_radio_write16(sc, 0x0052, regstack[1] | sc->sc_phy_minlowsigpos[0]);

	sc->sc_phy_minlowsig[1] = 0xffff;

	for (i = -4; i < 5; i += 2) {
		for (j = -4; j < 5; j += 2) {
			if (j < 0)
				fval = (0x0100 * i) + j + 0x0100;
			else
				fval = (0x0100 * i) + j;
			bcw_phy_write16(sc, 0x002f, fval);
			mls = bcw_phy_lo_b_r15_loop(sc) / 10;
			if (mls < sc->sc_phy_minlowsig[1]) {
				sc->sc_phy_minlowsig[1] = mls;
				sc->sc_phy_minlowsigpos[1] = fval;
			}
		}
	}
	sc->sc_phy_minlowsigpos[1] += 0x0101;

	bcw_phy_write16(sc, 0x002f, sc->sc_phy_minlowsigpos[1]);
	if (sc->sc_radio_ver == 0x2053) {
		bcw_phy_write16(sc, 0x000a, regstack[2]);
		bcw_phy_write16(sc, 0x002a, regstack[3]);
		bcw_phy_write16(sc, 0x0035, regstack[4]);
		bcw_phy_write16(sc, 0x0003, regstack[5]);
		bcw_phy_write16(sc, 0x0001, regstack[6]);
		bcw_phy_write16(sc, 0x0030, regstack[7]);

		bcw_radio_write16(sc, 0x0043, regstack[8]);
		bcw_radio_write16(sc, 0x007a, regstack[9]);

		bcw_radio_write16(sc, 0x0052, (bcw_radio_read16(sc, 0x0052) &
		    0x000f) | regstack[11]);
		BCW_WRITE16(sc, 0x03ec, regstack[10]);
	}
	bcw_phy_write16(sc, 0x0015, regstack[0]);
}

void
bcw_phy_lo_g_state(struct bcw_softc *sc, struct bcw_lopair *in_pair,
    struct bcw_lopair *out_pair, uint16_t r27)
{
	const struct bcw_lopair transitions[8] = {
	    { .high =  1,  .low =  1, },
	    { .high =  1,  .low =  0, },
	    { .high =  1,  .low = -1, },
	    { .high =  0,  .low = -1, },
	    { .high = -1,  .low = -1, },
	    { .high = -1,  .low =  0, },
	    { .high = -1,  .low =  1, },
	    { .high =  0,  .low =  1, } };
	struct bcw_lopair lowest_transition = {
	    .high = in_pair->high,
	    .low = in_pair->low };
	struct bcw_lopair tmp_pair;
	struct bcw_lopair transition;
	int i = 12;
	int state = 0;
	int found_lower;
	int j, begin, end;
	uint32_t lowest_deviation;
	uint32_t tmp;

	bcw_phy_lo_write(sc, &lowest_transition);
	lowest_deviation = bcw_phy_lo_g_singledeviation(sc, r27);
	do {
		found_lower = 0;
		/* XXX assert() */
		if (state == 0) {
			begin = 1;
			end = 8;
		} else if (state % 2 == 0) {
			begin = state - 1;
			end = state + 1;
		} else {
			begin = state - 2;
			end = state + 2;
		}
		if (begin < 1)
			begin += 8;
		if (end > 8)
			end -= 8;

		j = begin;
		tmp_pair.high = lowest_transition.high;
		tmp_pair.low = lowest_transition.low;
		while (1) {
			/* XXX assert() */
			transition.high = tmp_pair.high +
			    transitions[j - 1].high;
			transition.low = tmp_pair.low +
			    transitions[j - 1].low;
			if ((abs(transition.low) < 9) && (abs(transition.high)
			    < 9)) {
				bcw_phy_lo_write(sc, &transition);
				tmp = bcw_phy_lo_g_singledeviation(sc, r27);
				if (tmp < lowest_deviation) {
					lowest_deviation = tmp;
					state = j;
					found_lower = 1;
					lowest_transition.high =
					    transition.high;
					lowest_transition.low =
					    transition.low;
				}
			}
			if (j == end)
				break;
			if (j == 8)
				j = 1;
			else
				j++;
		}
	} while (i-- && found_lower);

	out_pair->high = lowest_transition.high;
	out_pair->low = lowest_transition.low;
}

void
bcw_phy_lo_g_measure(struct bcw_softc *sc)
{
	const uint8_t pairorder[10] = { 3, 1, 5, 7, 9, 2, 0, 4, 6, 8 };
	const int is_initializing = 0; /* XXX */
	uint16_t h, i, oldi = 0, j;
	struct bcw_lopair control;
	struct bcw_lopair *tmp_control;
	uint16_t tmp;
	uint16_t regstack[16] = { 0 };
	uint16_t oldchannel;
	uint8_t r27 = 0, r31;

	oldchannel = sc->sc_radio_channel;
	/* setup */
	if (sc->sc_phy_connected) {
		regstack[0] = bcw_phy_read16(sc, BCW_PHY_G_CRS);
		regstack[1] = bcw_phy_read16(sc, 0x0802);
		bcw_phy_write16(sc, BCW_PHY_G_CRS, regstack[0] & 0x7fff);
		bcw_phy_write16(sc, 0x0802, regstack[1] & 0xfffc);
	}
	regstack[3]  = BCW_READ16(sc, 0x03e2);
	BCW_WRITE16(sc, 0x03e2, regstack[3] | 0x8000);
	regstack[4]  = BCW_READ16(sc, BCW_MMIO_CHANNEL_EXT);
	regstack[5]  = bcw_phy_read16(sc, 0x15);
	regstack[6]  = bcw_phy_read16(sc, 0x2a);
	regstack[7]  = bcw_phy_read16(sc, 0x35);
	regstack[8]  = bcw_phy_read16(sc, 0x60);
	regstack[9]  = bcw_radio_read16(sc, 0x43);
	regstack[10] = bcw_radio_read16(sc, 0x7a);
	regstack[11] = bcw_radio_read16(sc, 0x52);
	if (sc->sc_phy_connected) {
		regstack[12] = bcw_phy_read16(sc, 0x0811);
		regstack[13] = bcw_phy_read16(sc, 0x0812);
		regstack[14] = bcw_phy_read16(sc, 0x0814);
		regstack[15] = bcw_phy_read16(sc, 0x0815);
	}
	bcw_radio_select_channel(sc, 6, 0);
	if (sc->sc_phy_connected) {
		bcw_phy_write16(sc, BCW_PHY_G_CRS, regstack[0] & 0x7fff);
		bcw_phy_write16(sc, 0x0802, regstack[1] & 0xfffc);
		bcw_dummy_transmission(sc);
	}
	bcw_radio_write16(sc, 0x0043, 0x0006);

	bcw_phy_set_baseband_atten(sc, 2);

	BCW_WRITE16(sc, BCW_MMIO_CHANNEL_EXT, 0);
	bcw_phy_write16(sc, 0x002e, 0x007f);
	bcw_phy_write16(sc, 0x080f, 0x0078);
	bcw_phy_write16(sc, 0x0035, regstack[7] & ~(1 << 7));
	bcw_radio_write16(sc, 0x007a, regstack[10] & 0xfff0);
	bcw_phy_write16(sc, 0x002b, 0x0203);
	bcw_phy_write16(sc, 0x002a, 0x08a3);
	if (sc->sc_phy_type) {
		bcw_phy_write16(sc, 0x0814, regstack[14] | 0x0003);
		bcw_phy_write16(sc, 0x0815, regstack[15] & 0xfffc);
		bcw_phy_write16(sc, 0x0811, 0x01b3);
		bcw_phy_write16(sc, 0x0812, 0x00b2);
	}
	if (is_initializing)
		bcw_phy_lo_g_measure_txctl2(sc);

	bcw_phy_write16(sc, 0x080f, 0x8078);

	/* measure */
	control.low = 0;
	control.high = 0;
	for (h = 0; h < 10; h++) {
		/* loop over each possible RadioAttenuation (0 - 9) */
		i = pairorder[h];
		if (is_initializing) {
			if (i == 3) {
				control.low = 0;
				control.high = 0;
			} else if (((i % 2 == 1) && (oldi % 2 == 1)) ||
			    ((i % 2 == 0) && (oldi % 2 == 0))) {
				tmp_control = bcw_get_lopair(sc, oldi, 0);
				memcpy(&control, tmp_control, sizeof(control));
			} else {
				tmp_control = bcw_get_lopair(sc, 3, 0);
				memcpy(&control, tmp_control, sizeof(control));
			}
		}
		/* loop over each possible BasebandAttenuation / 2 */
		for (j = 0; j < 4; j++) {
			if (is_initializing) {
				tmp = i * 2 + j;
				r27 = 0;
				r31 = 0;
				if (tmp > 14) {
					r31 = 1;
					if (tmp > 17)
						r27 = 1;
					if (tmp > 19)
						r27 = 2;
				}
			} else {
				tmp_control = bcw_get_lopair(sc, i, j * 2);
				if (!tmp_control->used)
					continue;
				memcpy(&control, tmp_control, sizeof(control));
				r27 = 3;
				r31 = 0;
			}
			bcw_radio_write16(sc, 0x43, i);
			bcw_radio_write16(sc, 0x52, sc->sc_radio_txctl2);
			delay(10);
			/* XXX bcm43xx_voluntary_preempt() ? */

			bcw_phy_set_baseband_atten(sc, j * 2);

			tmp = (regstack[10] & 0xfff0);
			if (r31)
				tmp |= 0x0008;
			bcw_radio_write16(sc, 0x007a, tmp);

			tmp_control = bcw_get_lopair(sc, i, j * 2);
			bcw_phy_lo_g_state(sc, &control, tmp_control, r27);
		}
		oldi = i;
	}
	/* loop over each possible RadioAttenuation (10 - 13) */
	for (i = 10; i < 14; i++) {
		/* loop over each possible BasebandAttenuation / 2 */
		for (j = 0; j < 4; j++) {
			if (is_initializing) {
				tmp_control = bcw_get_lopair(sc, i - 9, j * 2);
				memcpy(&control, tmp_control, sizeof(control));
				tmp = (i - 9) * 2 + j - 5; /* FIXME */
				r27 = 0;
				r31 = 0;
				if (tmp > 14) {
					r31 = 1;
					if (tmp > 17)
						r27 = 1;
					if (tmp > 19)
						r27 = 2;
				}
			} else {
				tmp_control = bcw_get_lopair(sc, i - 9, j * 2);
				if (!tmp_control->used)
					continue;
				memcpy(&control, tmp_control, sizeof(control));
				r27 = 3;
				r31 = 0;
			}
			bcw_radio_write16(sc, 0x043, i - 9);
			bcw_radio_write16(sc, 0x052, sc->sc_radio_txctl2 |
			    (3 << 4)); /* FIXME */
			delay(10);
			/* XXX bcm43xx_voluntary_preempt() */

			bcw_phy_set_baseband_atten(sc, j * 2);

			tmp = (regstack[10] & 0xfff0);
			if (r31)
				tmp |= 0x0008;
			bcw_radio_write16(sc, 0x7a, tmp);

			tmp_control = bcw_get_lopair(sc, i, j * 2);
			/* XXX bcm43xx_phy_lo_g_state() */
		}

	}

	/* restoration */
	if (sc->sc_phy_connected) {
		bcw_phy_write16(sc, 0x0015, 0xe300);
		bcw_phy_write16(sc, 0x0812, (r27 << 8) | 0xa0);
		delay(5);
		bcw_phy_write16(sc, 0x0812, (r27 << 8) | 0xa2);
		delay(2);
		bcw_phy_write16(sc, 0x0812, (r27 << 8) | 0xa3);
		/* XXX bcm43xx_voluntary_preempt() */
	} else
		bcw_phy_write16(sc, 0x0015, r27 | 0xefa0);
	bcw_phy_lo_adjust(sc, is_initializing);
	bcw_phy_write16(sc, 0x002e, 0x807f);
	if (sc->sc_phy_connected)
		bcw_phy_write16(sc, 0x002f, 0x0202);
	else
		bcw_phy_write16(sc, 0x002f, 0x0101);
	bcw_phy_write16(sc, BCW_MMIO_CHANNEL_EXT, regstack[4]);
	bcw_phy_write16(sc, 0x0015, regstack[5]);
	bcw_phy_write16(sc, 0x0015, regstack[6]);
	bcw_phy_write16(sc, 0x0015, regstack[7]);
	bcw_phy_write16(sc, 0x0015, regstack[8]);
	bcw_radio_write16(sc, 0x0043, regstack[9]);
	bcw_radio_write16(sc, 0x007a, regstack[10]);
	regstack[11] &= 0x00f0;
	regstack[11] |= (bcw_radio_read16(sc, 0x52) & 0x000f);
	bcw_radio_write16(sc, 0x52, regstack[11]);
	BCW_WRITE16(sc, 0x03e2, regstack[3]);
	if (sc->sc_phy_connected) {
		bcw_phy_write16(sc, 0x0811, regstack[12]);
		bcw_phy_write16(sc, 0x0812, regstack[13]);
		bcw_phy_write16(sc, 0x0814, regstack[14]);
		bcw_phy_write16(sc, 0x0815, regstack[15]);
		bcw_phy_write16(sc, BCW_PHY_G_CRS, regstack[0]);
		bcw_phy_write16(sc, 0x0802, regstack[1]);
	}
	bcw_radio_select_channel(sc, oldchannel, 1);
}

void
bcw_phy_lo_g_measure_txctl2(struct bcw_softc *sc)
{
	uint16_t txctl2 = 0, i;
	uint32_t smallest, tmp;

	bcw_radio_write16(sc, 0x0052, 0);
	delay(10);
	smallest = bcw_phy_lo_g_singledeviation(sc, 0);
	for (i = 0; i < 16; i++) {
		bcw_radio_write16(sc, 0x0052, i);
		delay(10);
		tmp = bcw_phy_lo_g_singledeviation(sc, 0);
		if (tmp < smallest) {
			smallest = tmp;
			txctl2 = i;
		}
	}
	sc->sc_radio_txctl2 = txctl2;
}

uint32_t
bcw_phy_lo_g_singledeviation(struct bcw_softc *sc, uint16_t control)
{
	int i;
	uint32_t r = 0;

	for (i = 0; i < 8; i++)
		r += bcw_phy_lo_g_deviation_subval(sc, control);

	return (r);
}

uint16_t
bcw_phy_lo_g_deviation_subval(struct bcw_softc *sc, uint16_t control)
{
	uint16_t r;
	//unsigned long flags;

	/* XXX splnet ? */
	if (sc->sc_phy_connected) {
		bcw_phy_write16(sc, 0x15, 0xe300);
		control <<= 8;
		bcw_phy_write16(sc, 0x0812, control | 0x00b0);
		delay(5);
		bcw_phy_write16(sc, 0x0812, control | 0x00b2);
		delay(2);
		bcw_phy_write16(sc, 0x0812, control | 0x00b3);
		delay(4);
		bcw_phy_write16(sc, 0x0015, 0xf300);
		delay(8);
	} else {
		bcw_phy_write16(sc, 0x0015, control | 0xefa0);
		delay(2);
		bcw_phy_write16(sc, 0x0015, control | 0xefe0);
		delay(4);
		bcw_phy_write16(sc, 0x0015, control | 0xffe0);
		delay(8);
	}
	r = bcw_phy_read16(sc, 0x002d);

	/* XXX splnet ? */
	/* XXX bcm43xx_voluntary_preempt() ? */

	return (r);
}

void
bcw_phy_lo_adjust(struct bcw_softc *sc, int fixed)
{
	struct bcw_lopair *pair;

	if (fixed)
		pair = bcw_phy_find_lopair(sc, 2, 3, 0);
	else
		pair = bcw_phy_current_lopair(sc);

	bcw_phy_lo_write(sc, pair);
}

void
bcw_phy_lo_mark_current_used(struct bcw_softc *sc)
{
	struct bcw_lopair *pair;

	pair = bcw_phy_current_lopair(sc);
	pair->used = 1;
}

void
bcw_phy_lo_write(struct bcw_softc *sc, struct bcw_lopair *pair)
{
	uint16_t val;

	val = (uint8_t)(pair->low);
	val |= ((uint8_t)(pair->high)) << 8;

#ifdef BCW_DEBUG
	if (pair->low < -8 || pair->low > 8 ||
	    pair->high < -8 || pair->high > 8)
		printf("%s: writing invalid LO pair "
		    "low: %d, high: %d, index: %lu)\n",
		    pair->low, pair->high,
		    (unsigned long)(pair - sc->sc_phy_lopairs));
#endif

	bcw_phy_write16(sc, BCW_PHY_G_LO_CONTROL, val);
}

struct bcw_lopair *
bcw_phy_find_lopair(struct bcw_softc *sc, uint16_t baseband_atten,
    uint16_t radio_atten, uint16_t tx)
{
	static const uint8_t dict[10] =
	    { 11, 10, 11, 12, 13, 12, 13, 12, 13, 12 };

	if (baseband_atten > 6)
		baseband_atten = 6;

	/* XXX assert() */

	if (tx == 3)
		return (bcw_get_lopair(sc, radio_atten, baseband_atten));

	return (bcw_get_lopair(sc, dict[radio_atten], baseband_atten));
}

struct bcw_lopair *
bcw_phy_current_lopair(struct bcw_softc *sc)
{
	return (bcw_phy_find_lopair(sc, sc->sc_radio_baseband_atten,
	    sc->sc_radio_radio_atten, sc->sc_radio_txctl1));
}

void
bcw_phy_prepare_init(struct bcw_softc *sc)
{
	sc->sc_phy_antenna_diversity = 0xffff;
	memset(sc->sc_phy_minlowsig, 0xff, sizeof(sc->sc_phy_minlowsig));
	memset(sc->sc_phy_minlowsigpos, 0, sizeof(sc->sc_phy_minlowsigpos));

	/* flags */
	sc->sc_phy_calibrated = 0;
	sc->sc_phy_is_locked = 0;

	if (sc->sc_phy_lopairs)
		memset(sc->sc_phy_lopairs, 0, sizeof(struct bcw_lopair) *
		    BCW_LO_COUNT);

	memset(sc->sc_phy_loopback_gain, 0, sizeof(sc->sc_phy_loopback_gain));
}

void
bcw_phy_set_antenna_diversity(struct bcw_softc *sc)
{
	uint16_t antennadiv;
	uint16_t offset;
	uint16_t val;
	uint32_t ucodeflags;

	if (antennadiv == 0xffff)
		antennadiv = 3;
	/* XXX assert() */

	ucodeflags = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED,
	    BCW_SHM_MICROCODEFLAGSLOW);
	bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, BCW_SHM_MICROCODEFLAGSLOW,
	    ucodeflags & ~BCW_SHM_MICROCODEFLAGSAUTODIV);

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
	case BCW_PHY_TYPEG:
		if (sc->sc_phy_type == BCW_PHY_TYPEA)
			offset = 0;
		else
			offset = 0x0400;

		if (antennadiv == 2)
			val = (3 << 7);
		else
			val = (antennadiv << 7);
		bcw_phy_write16(sc, offset + 1,
		    (bcw_phy_read16(sc, offset + 1) & 0x7e7f) | val);

		if (antennadiv >= 2) {
			if (antennadiv == 2)
				val = (antennadiv << 7);
			else
				val = (0 << 7);
			bcw_phy_write16(sc, offset + 0x2b,
			    (bcw_phy_read16(sc, offset + 0x2b) & 0xfeff) | val);
		}

		if (sc->sc_phy_type == BCW_PHY_TYPEG) {
			if (antennadiv >= 2)
				bcw_phy_write16(sc, 0x048c,
				    bcw_phy_read16(sc, 0x048c) | 0x2000);
			else
				bcw_phy_write16(sc, 0x048c,
				    bcw_phy_read16(sc, 0x048c) | ~0x2000);
			if (sc->sc_phy_rev >= 2) {
				bcw_phy_write16(sc, 0x0461,
				    bcw_phy_read16(sc, 0x0461) | 0x0010);
				bcw_phy_write16(sc, 0x04ad,
				    (bcw_phy_read16(sc, 0x04ad) & 0x00ff) |
				    0x0015);
				if (sc->sc_phy_rev == 2)
					bcw_phy_write16(sc, 0x0427, 0x0008);
				else
					bcw_phy_write16(sc, 0x0427,
					    (bcw_phy_read16(sc, 0x0427) &
					    0x00ff) | 0x0008);
			} else if (sc->sc_phy_rev >= 6)
				bcw_phy_write16(sc, 0x049b, 0x00dc);
		} else {
			if (sc->sc_phy_rev < 3)
				bcw_phy_write16(sc, 0x002b,
				    (bcw_phy_read16(sc, 0x002b) & 0x00ff) |
				    0x0024);
			else {
				bcw_phy_write16(sc, 0x0061,
				    bcw_phy_read16(sc, 0x0061) | 0x0010);
				if (sc->sc_phy_rev == 3) {
					bcw_phy_write16(sc, 0x0093, 0x001d);
					bcw_phy_write16(sc, 0x0027, 0x0008);
				} else {
					bcw_phy_write16(sc, 0x0093, 0x003a);
					bcw_phy_write16(sc, 0x0027,
					    (bcw_phy_read16(sc, 0x0027) &
					    0x00ff) | 0x0008);	
				}
			}
		}
		break;
	case BCW_PHY_TYPEB:
		if (1) /* XXX current_core->rev */
			val = (3 << 7);
		else
			val = (antennadiv << 7);
		bcw_phy_write16(sc, 0x03e2, (bcw_phy_read16(sc, 0x03e2) &
		    0xfe7f) | val);
		break;
	default:
		/* XXX assert() */
		break;
	}

	if (antennadiv >= 2) {
		ucodeflags = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED,
		    BCW_SHM_MICROCODEFLAGSLOW);
		bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED,
		    BCW_SHM_MICROCODEFLAGSLOW, ucodeflags |
		    BCW_SHM_MICROCODEFLAGSAUTODIV);
	}

	sc->sc_phy_antenna_diversity = antennadiv;
}

/*
 * Radio
 */
void
bcw_radio_off(struct bcw_softc *sc)
{
	/* Magic unexplained values */
	if (sc->sc_phy_type == BCW_PHY_TYPEA) {
		bcw_radio_write16(sc, 0x0004, 0x00ff);
		bcw_radio_write16(sc, 0x0005, 0x00fb);
		bcw_phy_write16(sc, 0x0010, bcw_phy_read16(sc, 0x0010) |
		    0x0008);
		bcw_phy_write16(sc, 0x0011, bcw_phy_read16(sc, 0x0011) |
		    0x0008);
	}
	if (sc->sc_phy_type == BCW_PHY_TYPEB && sc->sc_core_80211->rev >= 5) {
		bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) |
		    0x008c);
		bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) &
		    0xff73);
	} else
		bcw_phy_write16(sc, 0x0015, 0xaa00);

	DPRINTF(("%s: Radio turned off\n", sc->sc_dev.dv_xname));
}

void
bcw_radio_on(struct bcw_softc *sc)
{
	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		bcw_radio_write16(sc, 0x0004, 0x00c0);
		bcw_radio_write16(sc, 0x0005, 0x0008);
		bcw_phy_write16(sc, 0x0010, bcw_phy_read16(sc, 0x0010) &
		    0xfff7);
		bcw_phy_write16(sc, 0x0011, bcw_phy_read16(sc, 0x0011) &
		    0xfff7);
		bcw_radio_init2060(sc);
		break;
	case BCW_PHY_TYPEB:
	case BCW_PHY_TYPEG:
		bcw_phy_write16(sc, 0x0015, 0x8000);
		bcw_phy_write16(sc, 0x0015, 0xcc00);
		bcw_phy_write16(sc, 0x0015, sc->sc_phy_connected ? 0x00c0 : 0);
		if (bcw_radio_select_channel(sc, BCW_RADIO_DEFAULT_CHANNEL_BG,
		    1))
			return;
		break;
	default:
		return;
	}

	DPRINTF(("%s: Radio turned on\n", sc->sc_dev.dv_xname)); 
}

void
bcw_radio_nrssi_hw_write(struct bcw_softc *sc, uint16_t offset, int16_t val)
{
	bcw_phy_write16(sc, BCW_PHY_NRSSILT_CTRL, offset);
	bcw_phy_write16(sc, BCW_PHY_NRSSILT_DATA, (uint16_t)val);
}

int16_t
bcw_radio_nrssi_hw_read(struct bcw_softc *sc, uint16_t offset)
{
	uint16_t val;

	bcw_phy_write16(sc, BCW_PHY_NRSSILT_CTRL, offset);
	val = bcw_phy_read16(sc, BCW_PHY_NRSSILT_DATA);

	return ((int16_t)val);
}

void
bcw_radio_nrssi_hw_update(struct bcw_softc *sc, uint16_t val)
{
	uint16_t i;
	int16_t tmp;

	for (i = 0; i < 64; i++) {
		tmp = bcw_radio_nrssi_hw_read(sc, i);
		tmp -= val;
		tmp = bcw_lv(tmp, 32, 31);
		bcw_radio_nrssi_hw_write(sc, i, tmp);
	}
}

void
bcw_radio_calc_nrssi_threshold(struct bcw_softc *sc)
{
	int32_t threshold;
	int32_t a, b;
	int16_t tmp16;
	uint16_t tmp_u16;

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEB:
		if (sc->sc_radio_ver != 0x2050)
			return;
		if (!(sc->sc_boardflags & BCW_BF_RSSI))
			return;

		if (sc->sc_radio_rev >= 6) {
			threshold = (sc->sc_radio_nrssi[1] -
			    sc->sc_radio_nrssi[0]) * 32;
			threshold += 20 * (sc->sc_radio_nrssi[0] + 1);
			threshold /= 40;
		} else
			threshold = sc->sc_radio_nrssi[1] - 5;

		threshold = bcw_lv(threshold, 0, 0x3e);
		bcw_phy_read16(sc, 0x0020);
		bcw_phy_write16(sc, 0x0020, (((uint16_t)threshold) << 8) |
		    0x001c);

		if (sc->sc_radio_rev >= 6) {
			bcw_phy_write16(sc, 0x0087, 0x0e0d);
			bcw_phy_write16(sc, 0x0086, 0x0c0d);
			bcw_phy_write16(sc, 0x0085, 0x0a09);
			bcw_phy_write16(sc, 0x0084, 0x0808);
			bcw_phy_write16(sc, 0x0083, 0x0808);
			bcw_phy_write16(sc, 0x0082, 0x0604);
			bcw_phy_write16(sc, 0x0081, 0x0302);
			bcw_phy_write16(sc, 0x0080, 0x0100);
		}
		break;
	case BCW_PHY_TYPEG:
		if (!sc->sc_phy_connected ||
		    !(sc->sc_boardflags & BCW_BF_RSSI)) {
			tmp16 = bcw_radio_nrssi_hw_read(sc, 0x20);
			if (tmp16 >= 0x20)
				tmp16 -= 0x40;
			if (tmp16 < 3)
				bcw_phy_write16(sc, 0x048a,
				    (bcw_phy_read16(sc, 0x048a) & 0xf000) |
				    0x09eb);
			else
				bcw_phy_write16(sc, 0x048a,
				    (bcw_phy_read16(sc, 0x048a) & 0xf000) |
				    0x0aed);
		} else {
			if (sc->sc_radio_interfmode ==
			    BCW_RADIO_INTERFMODE_NONWLAN) {
				a = 0xe;
				b = 0xa;
			} else if (sc->sc_radio_aci_wlan_automatic &&
			    sc->sc_radio_aci_enable) {
				a = 0x13;
				b = 0x12;
			} else {
				a = 0xe;
				b = 0x11;
			}

			a = a * (sc->sc_radio_nrssi[1] - sc->sc_radio_nrssi[0]);
			a += (sc->sc_radio_nrssi[0] << 6);
			if (a < 32)
				a += 31;
			else
				a += 32;
			a = a >> 6;
			a = bcw_lv(a, -31, 31);

			b = b * (sc->sc_radio_nrssi[1] - sc->sc_radio_nrssi[0]);
			b += (sc->sc_radio_nrssi[0] << 6);
			if (b < 32)
				b += 31;
			else
				b += 32;
			b = b >> 6;
			b = bcw_lv(b, -31, 31);

			tmp_u16 = bcw_phy_read16(sc, 0x048a) & 0xf000;
			tmp_u16 |= ((uint32_t)b & 0x0000003f);
			tmp_u16 |= (((uint32_t)a & 0x0000003f) << 6);
			bcw_phy_write16(sc, 0x048a, tmp_u16);
		}
		break;
	default:
		/* XXX assert() */
		return;
	}
}

void
bcw_radio_calc_nrssi_slope(struct bcw_softc *sc)
{
	uint16_t backup[18] = { 0 };
	uint16_t tmp;
	int16_t nrssi0, nrssi1;

	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEB:
		backup[0]  = bcw_radio_read16(sc, 0x007a);
		backup[1]  = bcw_radio_read16(sc, 0x0052);
		backup[2]  = bcw_radio_read16(sc, 0x0043);
		backup[3]  = bcw_phy_read16(sc, 0x0030);
		backup[4]  = bcw_phy_read16(sc, 0x0026);
		backup[5]  = bcw_phy_read16(sc, 0x0015);
		backup[6]  = bcw_phy_read16(sc, 0x0020);
		backup[7]  = bcw_phy_read16(sc, 0x005a);
		backup[8]  = bcw_phy_read16(sc, 0x0059);
		backup[9]  = bcw_phy_read16(sc, 0x0058);
		backup[10] = bcw_phy_read16(sc, 0x03e2);
		backup[11] = BCW_READ16(sc, 0x03e6);
		backup[12] = BCW_READ16(sc, 0x007a);
		backup[13] = BCW_READ16(sc, BCW_MMIO_CHANNEL_EXT);

		tmp = bcw_radio_read16(sc, 0x007a);
		tmp &= ((sc->sc_phy_rev >= 5) ? 0x007f : 0x000f);
		bcw_radio_write16(sc, 0x007a, tmp);
		bcw_phy_write16(sc, 0x0030, 0x00f);
		BCW_WRITE16(sc, 0x03ec, 0x7f7f);
		bcw_phy_write16(sc, 0x0026, 0);
		bcw_phy_write16(sc, 0x0015, bcw_phy_read16(sc, 0x0015) |
		    0x0020);
		bcw_phy_write16(sc, 0x002a, 0x08a3);
		bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) |
		    0x0080);

		nrssi0 = (uint16_t)bcw_phy_read16(sc, 0x0027);
		bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) &
		    0x007f);
		if (sc->sc_phy_rev >= 2)
			BCW_WRITE16(sc, 0x03e6, 0x0040);
		else if (sc->sc_phy_rev == 0)
			BCW_WRITE16(sc, 0x03e6, 0x0122);
		else
			BCW_WRITE16(sc, BCW_MMIO_CHANNEL_EXT,
			    BCW_READ16(sc, BCW_MMIO_CHANNEL_EXT) & 0x2000);
		bcw_phy_write16(sc, 0x0020, 0x3f3f);
		bcw_phy_write16(sc, 0x0014, 0xf330);
		bcw_radio_write16(sc, 0x005a, 0xf330);
		bcw_radio_write16(sc, 0x0043,
		    bcw_radio_read16(sc, 0x0043) & 0x0f0);
		bcw_phy_write16(sc, 0x005a, 0x0480);
		bcw_phy_write16(sc, 0x0059, 0x0810);
		bcw_phy_write16(sc, 0x0058, 0x000d);
		delay(20);

		nrssi1 = (int16_t)bcw_phy_read16(sc, 0x0027);
		bcw_phy_write16(sc, 0x0030, backup[3]);
		bcw_radio_write16(sc, 0x007a, backup[0]);
		BCW_WRITE16(sc, 0x03e2, backup[11]);
		bcw_phy_write16(sc, 0x0026, backup[4]);
		bcw_phy_write16(sc, 0x0015, backup[5]);
		bcw_phy_write16(sc, 0x002a, backup[6]);
		bcw_radio_spw(sc, sc->sc_radio_channel);
		if (sc->sc_phy_rev != 0)
			BCW_WRITE16(sc, 0x3f4, backup[13]);

		bcw_phy_write16(sc, 0x0020, backup[7]);
		bcw_phy_write16(sc, 0x0020, backup[8]);
		bcw_phy_write16(sc, 0x0020, backup[9]);
		bcw_phy_write16(sc, 0x0020, backup[10]);
		bcw_radio_write16(sc, 0x0052, backup[1]);
		bcw_radio_write16(sc, 0x0043, backup[2]);

		if (nrssi0 == nrssi1)
			sc->sc_radio_nrssislope = 0x00010000;
		else
			sc->sc_radio_nrssislope = 0x00400000 /
			    (nrssi0 - nrssi1);

		if (nrssi0 <= -4) {
			sc->sc_radio_nrssi[0] = nrssi0;
			sc->sc_radio_nrssi[1] = nrssi1;
		}
		break;
	case BCW_PHY_TYPEG:
		if (sc->sc_radio_rev >= 9)
			return;
		if (sc->sc_radio_rev == 8)
			bcw_radio_calc_nrssi_offset(sc);
		break;
	}
}

void
bcw_radio_calc_nrssi_offset(struct bcw_softc *sc)
{
	uint16_t backup[20] = { 0 };
	int16_t v47f;
	uint16_t i;
	uint16_t saved = 0xffff;

	backup[0] = bcw_phy_read16(sc, 0x0001);
	backup[1] = bcw_phy_read16(sc, 0x0811);
	backup[2] = bcw_phy_read16(sc, 0x0812);
	backup[3] = bcw_phy_read16(sc, 0x0814);
	backup[4] = bcw_phy_read16(sc, 0x0815);
	backup[5] = bcw_phy_read16(sc, 0x005a);
	backup[6] = bcw_phy_read16(sc, 0x0059);
	backup[7] = bcw_phy_read16(sc, 0x0058);
	backup[8] = bcw_phy_read16(sc, 0x000a);
	backup[9] = bcw_phy_read16(sc, 0x0003);
	backup[10] = bcw_radio_read16(sc, 0x007a);
	backup[11] = bcw_radio_read16(sc, 0x0043);

	bcw_phy_write16(sc, 0x0429, bcw_phy_read16(sc, 0x0429) & 0x7fff);
	bcw_phy_write16(sc, 0x0001, (bcw_phy_read16(sc, 0x0001) & 0x3fff) |
	    0x4000);
	bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) | 0x000c);
	bcw_phy_write16(sc, 0x0812, (bcw_phy_read16(sc, 0x0812) & 0xfff3) |
	    0x0004);
	bcw_phy_write16(sc, 0x0802, bcw_phy_read16(sc, 0x0802) & ~(0x1 | 0x2));

	if (sc->sc_phy_rev >= 6) {
		backup[12] = bcw_phy_read16(sc, 0x002e);
		backup[13] = bcw_phy_read16(sc, 0x002f);
		backup[14] = bcw_phy_read16(sc, 0x080f);
		backup[15] = bcw_phy_read16(sc, 0x0810);
		backup[16] = bcw_phy_read16(sc, 0x0801);
		backup[17] = bcw_phy_read16(sc, 0x0060);
		backup[18] = bcw_phy_read16(sc, 0x0014);
		backup[19] = bcw_phy_read16(sc, 0x0478);

		bcw_phy_write16(sc, 0x002e, 0);
		bcw_phy_write16(sc, 0x002e, 0);
		bcw_phy_write16(sc, 0x002e, 0);
		bcw_phy_write16(sc, 0x002e, 0);
		bcw_phy_write16(sc, 0x0478, bcw_phy_read16(sc, 0x0478) |
		    0x0100);
		bcw_phy_write16(sc, 0x0801, bcw_phy_read16(sc, 0x0801) |
		    0x0040);
		bcw_phy_write16(sc, 0x0060, bcw_phy_read16(sc, 0x0060) |
		    0x0040);
		bcw_phy_write16(sc, 0x0014, bcw_phy_read16(sc, 0x0014) |
		    0x0200);
	}
	bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) | 0x0070);
	bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) | 0x0080);
	delay(30);

	v47f = (int16_t)((bcw_phy_read16(sc, 0x047f) >> 8) & 0x00ef);
	if (v47f >= 0x20)
		v47f -= 0x40;
	if (v47f == 31) {
		for (i = 7; i >= 4; i--) {
			bcw_radio_write16(sc, 0x007b, i);
			delay(20);
			v47f = (int16_t)((bcw_phy_read16(sc, 0x047f) >> 8) &
			    0x003f);
			if (v47f >= 0x20)
				v47f -= 0x40;
			if (v47f < 31 && saved == 0xffff)
				saved = i;
		}
		if (saved == 0xffff)
			saved = 4;
	} else {
		bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) &
		    0x007f);

		bcw_phy_write16(sc, 0x0814, bcw_phy_read16(sc, 0x0814) |
		    0x0001);
		bcw_phy_write16(sc, 0x0815, bcw_phy_read16(sc, 0x0815) &
		    0xfffe);
		bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) |
		    0x000c);
		bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) |
		    0x000c);
		bcw_phy_write16(sc, 0x0811, bcw_phy_read16(sc, 0x0811) |
		    0x0030);
		bcw_phy_write16(sc, 0x0812, bcw_phy_read16(sc, 0x0812) |
		    0x0030);
		bcw_phy_write16(sc, 0x005a, 0x0480);
		bcw_phy_write16(sc, 0x0059, 0x0810);
		bcw_phy_write16(sc, 0x0058, 0x000d);
		if (sc->sc_phy_rev == 0)
			bcw_phy_write16(sc, 0x0003, 0x0122);
		else
			bcw_phy_write16(sc, 0x000a, bcw_phy_read16(sc, 0x000a) |
			    0x2000);
		bcw_phy_write16(sc, 0x0814, bcw_phy_read16(sc, 0x0814) |
		    0x0004);
		bcw_phy_write16(sc, 0x0815, bcw_phy_read16(sc, 0x0815) &
		    0xfffb);
		bcw_phy_write16(sc, 0x0003, (bcw_phy_read16(sc, 0x0003) &
		    0xff9f) | 0x0040);
		bcw_radio_write16(sc, 0x007a, bcw_radio_read16(sc, 0x007a) |
		    0x000f);
		bcw_radio_set_all_gains(sc, 3, 0, 1);
		bcw_radio_write16(sc, 0x0043, (bcw_radio_read16(sc, 0x0043) &
		    0x00f0) | 0x000f);
		delay(30);
		v47f = (int16_t)((bcw_phy_read16(sc, 0x047f) >> 8) & 0x003f);
		if (v47f >= 0x20)
			v47f -= 0x40;
		if (v47f == -32) {
			for (i = 0; i < 4; i++) {
				bcw_radio_write16(sc, 0x007b, i);
				delay(20);
				v47f = (int16_t)((bcw_phy_read16(sc, 0x047f) >>
				    8) & 0x003f);
				if (v47f >= 0x20)
					v47f -= 0x40;
				if (v47f > -31 && saved == 0xffff)
					saved = i;
			}
			if (saved == 0xfff)
				saved = 3;
		} else
			saved = 0;
	}
	bcw_radio_write16(sc, 0x007b, saved);

	if (sc->sc_phy_rev >= 6) {
		bcw_phy_write16(sc, 0x002e, backup[12]);
		bcw_phy_write16(sc, 0x002f, backup[13]);
		bcw_phy_write16(sc, 0x080f, backup[14]);
		bcw_phy_write16(sc, 0x0810, backup[15]);
	}
	bcw_phy_write16(sc, 0x0814, backup[3]);
	bcw_phy_write16(sc, 0x0815, backup[4]);
	bcw_phy_write16(sc, 0x005a, backup[5]);
	bcw_phy_write16(sc, 0x0059, backup[6]);
	bcw_phy_write16(sc, 0x0058, backup[7]);
	bcw_phy_write16(sc, 0x000a, backup[8]);
	bcw_phy_write16(sc, 0x0003, backup[9]);
	bcw_radio_write16(sc, 0x0043, backup[11]);
	bcw_radio_write16(sc, 0x007a, backup[10]);
	bcw_phy_write16(sc, 0x0802, bcw_phy_read16(sc, 0x0802) | 0x1 | 0x2);
	bcw_phy_write16(sc, 0x0429, bcw_phy_read16(sc, 0x0429) | 0x8000);
	bcw_radio_set_original_gains(sc);
	if (sc->sc_phy_rev >= 6) {
		bcw_phy_write16(sc, 0x0801, backup[16]);
		bcw_phy_write16(sc, 0x0060, backup[17]);
		bcw_phy_write16(sc, 0x0014, backup[18]);
		bcw_phy_write16(sc, 0x0478, backup[19]);
	}
	bcw_phy_write16(sc, 0x0001, backup[0]);
	bcw_phy_write16(sc, 0x0812, backup[2]);
	bcw_phy_write16(sc, 0x0001, backup[1]);
}

void
bcw_radio_set_all_gains(struct bcw_softc *sc, int16_t first, int16_t second,
    int16_t third)
{
	uint16_t i;
	uint16_t start = 0x08, end = 0x18;
	uint16_t offset = 0x0400;
	uint16_t tmp;

	if (sc->sc_phy_rev <= 1) {
		offset = 0x5000;
		start = 0x10;
		end = 0x20;
	}

	for (i = 0; i < 4; i++)
		bcw_ilt_write(sc, offset + i, first);

	for (i = start; i < end; i++)
		bcw_ilt_write(sc, offset + i, second);

	if (third != -1) {
		bcw_phy_write16(sc, 0x04a0, (bcw_phy_read16(sc, 0x04a0) &
		    0xbfbf) | tmp);
		bcw_phy_write16(sc, 0x04a1, (bcw_phy_read16(sc, 0x04a1) &
		    0xbfbf) | tmp);
		bcw_phy_write16(sc, 0x04a2, (bcw_phy_read16(sc, 0x04a2) &
		    0xbfbf) | tmp);
	}
	bcw_dummy_transmission(sc);
}

void
bcw_radio_set_original_gains(struct bcw_softc *sc)
{
	uint16_t i, tmp;
	uint16_t offset = 0x0400;
	uint16_t start = 0x0008, end = 0x0018;

	if (sc->sc_phy_rev <= 1) {
		offset = 0x5000;
		start = 0x0010;
		end = 0x0020;
	}

	for (i = 0; i < 4; i++) {
		tmp = (i & 0xfffc);
		tmp |= (i & 0x0001) << 1;
		tmp |= (i & 0x0002) >> 1;

		bcw_ilt_write(sc, offset + i, tmp);
	}

	for (i = start; i < end; i++)
		bcw_ilt_write(sc, offset + i, i - start);

	bcw_phy_write16(sc, 0x04a0, (bcw_phy_read16(sc, 0x04a0) & 0xbfbf) |
	    0x4040);
	bcw_phy_write16(sc, 0x04a1, (bcw_phy_read16(sc, 0x04a1) & 0xbfbf) |
	    0x4040);
	bcw_phy_write16(sc, 0x04a2, (bcw_phy_read16(sc, 0x04a2) & 0xbfbf) |
	    0x4000);
	bcw_dummy_transmission(sc);
}

uint16_t
bcw_radio_calibrationvalue(struct bcw_softc *sc)
{
	uint16_t reg, index, r;

	r = bcw_radio_read16(sc, 0x0060);
	index = (reg & 0x001e) >> 1;
	r = rcc_table[index] << 1;
	r |= (reg & 0x0001);
	r |= 0x0020;

	return (r);
}

void
bcw_radio_set_txpower_a(struct bcw_softc *sc, uint16_t txpower)
{
	uint16_t pamp, base, dac, ilt;

	txpower = bcw_lv(txpower, 0, 63);

	pamp = bcw_radio_get_txgain_freq_power_amp(txpower);
	pamp <<= 5;
	pamp &= 0x00e0;
	bcw_phy_write16(sc, 0x0019, pamp);

	base = bcw_radio_get_txgain_baseband(txpower);
	base &= 0x000f;
	bcw_phy_write16(sc, 0x0017, base | 0x0020);

	ilt = bcw_ilt_read(sc, 0x3001);
	ilt &= 0x0007;

	dac = bcw_radio_get_txgain_dac(txpower);
	dac <<= 3;
	dac |= ilt;

	bcw_ilt_write(sc, 0x3001, dac);

	sc->sc_radio_txpwr_offset = txpower;
}

void
bcw_radio_set_txpower_bg(struct bcw_softc *sc, uint16_t baseband_atten,
    uint16_t radio_atten, uint16_t txpower)
{
	if (baseband_atten == 0xffff)
		baseband_atten = sc->sc_radio_baseband_atten;
	if (radio_atten == 0xffff)
		radio_atten = sc->sc_radio_radio_atten;
	if (txpower == 0xffff)
		txpower = sc->sc_radio_txctl1;

	sc->sc_radio_baseband_atten = baseband_atten;
	sc->sc_radio_radio_atten = radio_atten;
	sc->sc_radio_txctl1 = txpower;

	/* XXX assert() */

	bcw_phy_set_baseband_atten(sc, baseband_atten);
	bcw_radio_write16(sc, 0x0043, radio_atten);
	bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0064, radio_atten);
	if (sc->sc_radio_ver == 0x2050)
		bcw_radio_write16(sc, 0x0052, (bcw_radio_read16(sc, 0x0052) &
		    ~0x0070) | ((txpower << 4) & 0x0070));

	/* XXX unclear specs */
	if (sc->sc_phy_type == BCW_PHY_TYPEG)
		bcw_phy_lo_adjust(sc, 0);
}

uint16_t
bcw_radio_init2050(struct bcw_softc *sc)
{
	uint16_t backup[19] = { 0 };
	uint16_t r;
	uint16_t i, j;
	uint32_t tmp1 = 0, tmp2 = 0;

	backup[0]  = bcw_radio_read16(sc, 0x0043);
	backup[14] = bcw_radio_read16(sc, 0x0051);
	backup[15] = bcw_radio_read16(sc, 0x0052);
	backup[1]  = bcw_phy_read16(sc, 0x0015);
	backup[16] = bcw_phy_read16(sc, 0x005a);
	backup[17] = bcw_phy_read16(sc, 0x0059);
	backup[18] = bcw_phy_read16(sc, 0x0058);
	if (sc->sc_phy_type == BCW_PHY_TYPEB) {
		backup[2] = bcw_phy_read16(sc, 0x0030);
		backup[3] = BCW_READ16(sc, 0x03ec);
		bcw_phy_write16(sc, 0x0030, 0x00ff);
		BCW_WRITE16(sc, 0x03ec, 0x3f3f);
	} else {
		if (sc->sc_phy_connected) {
			backup[4] = bcw_phy_read16(sc, 0x0811);
			backup[5] = bcw_phy_read16(sc, 0x0812);
			backup[6] = bcw_phy_read16(sc, 0x0814);
			backup[7] = bcw_phy_read16(sc, 0x0815);
			backup[8] = bcw_phy_read16(sc, BCW_PHY_G_CRS);
			backup[9] = bcw_phy_read16(sc, 0x0802);
			bcw_phy_write16(sc, 0x0814,
			    (bcw_phy_read16(sc, 0x0814) | 0x0003));
			bcw_phy_write16(sc, 0x0815,
			    (bcw_phy_read16(sc, 0x0815) & 0xfffc));
			bcw_phy_write16(sc, BCW_PHY_G_CRS,
			    (bcw_phy_read16(sc, BCW_PHY_G_CRS) & 0x7fff));
			bcw_phy_write16(sc, 0x0802,
			    (bcw_phy_read16(sc, 0x0802) & 0xfffc));
			bcw_phy_write16(sc, 0x0811, 0x01b3);
			bcw_phy_write16(sc, 0x0812, 0x0fb2);
		}
		BCW_WRITE16(sc, BCW_MMIO_PHY_RADIO,
		    (BCW_READ16(sc, BCW_MMIO_PHY_RADIO) | 0x8000));
	}
	backup[10] = bcw_phy_read16(sc, 0x0035);
	bcw_phy_write16(sc, 0x0035, (bcw_phy_read16(sc, 0x0035) & 0xff7f));
	backup[11] = BCW_READ16(sc, 0x03e6);
	backup[12] = BCW_READ16(sc, BCW_MMIO_CHANNEL_EXT);

	/* initialization */
	if (sc->sc_phy_ver == 0)
		BCW_WRITE16(sc, 0x03e6, 0x0122);
	else {
		if (sc->sc_phy_ver >= 2)
			BCW_WRITE16(sc, 0x03e6, 0x0040);
		BCW_WRITE16(sc, BCW_MMIO_CHANNEL_EXT,
		    (BCW_READ16(sc, BCW_MMIO_CHANNEL_EXT) | 0x2000));
	}

	r = bcw_radio_calibrationvalue(sc);

	if (sc->sc_phy_type == BCW_PHY_TYPEB)
		bcw_radio_write16(sc, 0x0078, 0x0003);

	bcw_phy_write16(sc, 0x0015, 0xbfaf);
	bcw_phy_write16(sc, 0x002b, 0x1403);
	if (sc->sc_phy_connected)
		bcw_phy_write16(sc, 0x0812, 0x00b2);
	bcw_phy_write16(sc, 0x0015, 0xbfa0);
	bcw_radio_write16(sc, 0x0051, (bcw_radio_read16(sc, 0x0051) |
	    0x0004));
	bcw_radio_write16(sc, 0x0052, 0);
	bcw_radio_write16(sc, 0x0043, bcw_radio_read16(sc, 0x0043) | 0x0009);
	bcw_phy_write16(sc, 0x0058, 0);

	for (i = 0; i < 16; i++) {
		bcw_phy_write16(sc, 0x005a, 0x0480);
		bcw_phy_write16(sc, 0x0059, 0xc810);
		bcw_phy_write16(sc, 0x0058, 0x000d);
		if (sc->sc_phy_connected)
			bcw_phy_write16(sc, 0x0812, 0x30b2);
		bcw_phy_write16(sc, 0x0015, 0xafb0);
		delay(10);
		if (sc->sc_phy_connected)
			bcw_phy_write16(sc, 0x0812, 0x30b2);
		bcw_phy_write16(sc, 0x0015, 0xfff0);
		delay(10);
		if (sc->sc_phy_connected)
			bcw_phy_write16(sc, 0x0812, 0x30b2);
		bcw_phy_write16(sc, 0x0015, 0xfff0);
		delay(10);
		tmp1 += bcw_phy_read16(sc, 0x002d);
		bcw_phy_write16(sc, 0x0058, 0);
		if (sc->sc_phy_connected)
			bcw_phy_write16(sc, 0x0812, 0x30b2);
		bcw_phy_write16(sc, 0x0015, 0xafb0);
	}

	tmp1++;
	tmp1 >>= 9;
	delay(10);
	bcw_phy_write16(sc, 0x0058, 0);

	for (i = 0; i < 16; i++) {
		/* XXX flip_4bit(i) */
		bcw_radio_write16(sc, 0x0078, i << 1 | 0x0020);

		backup[13] = bcw_radio_read16(sc, 0x0078);
		delay(10);
		for (j = 0; j < 16; j++) {
			bcw_phy_write16(sc, 0x005a, 0x0d80);
			bcw_phy_write16(sc, 0x0059, 0xc810);
			bcw_phy_write16(sc, 0x0058, 0x000d);
			if (sc->sc_phy_connected)
				bcw_phy_write16(sc, 0x0812, 0x30b2);
			bcw_phy_write16(sc, 0x0015, 0xafb0);
			delay(10);
			if (sc->sc_phy_connected)
				bcw_phy_write16(sc, 0x0812, 0x30b2);
			bcw_phy_write16(sc, 0x0015, 0xefb0);
			delay(10);
			if (sc->sc_phy_connected)
				bcw_phy_write16(sc, 0x812, 0x30b3);
			bcw_phy_write16(sc, 0x0015, 0xfff0);
			delay(10);
			tmp2 += bcw_phy_read16(sc, 0x002d);
			bcw_phy_write16(sc, 0x0058, 0);
			if (sc->sc_phy_connected)
				bcw_phy_write16(sc, 0x0812, 0x30b2);
			bcw_phy_write16(sc, 0x0015, 0xafb0);
		}
		tmp2++;
		tmp2 >>= 8;
		if (tmp1 < tmp2)
			break;
	}

	/* restore the registers */
	bcw_phy_write16(sc, 0x0015, backup[1]);
	bcw_radio_write16(sc, 0x0051, backup[14]);
	bcw_radio_write16(sc, 0x0052, backup[15]);
	bcw_radio_write16(sc, 0x0043, backup[0]);
	bcw_phy_write16(sc, 0x005a, backup[16]);
	bcw_phy_write16(sc, 0x0059, backup[17]);
	bcw_phy_write16(sc, 0x0058, backup[18]);
	BCW_WRITE16(sc, 0x03e6, backup[11]);
	if (sc->sc_phy_ver != 0)
		BCW_WRITE16(sc, BCW_MMIO_CHANNEL_EXT, backup[12]);
	bcw_phy_write16(sc, 0x0035, backup[10]);
	bcw_radio_select_channel(sc, sc->sc_radio_channel, 1);
	if (sc->sc_phy_type == BCW_PHY_TYPEB) {
		bcw_phy_write16(sc, 0x0030, backup[2]);
		BCW_WRITE16(sc, 0x03ec, backup[3]);
	} else {
		BCW_WRITE16(sc, BCW_MMIO_PHY_RADIO,
		    (BCW_READ16(sc, BCW_MMIO_PHY_RADIO) & 0x7fff));
		if (sc->sc_phy_connected) {
			bcw_phy_write16(sc, 0x0811, backup[4]);
			bcw_phy_write16(sc, 0x0812, backup[5]);
			bcw_phy_write16(sc, 0x0814, backup[6]);
			bcw_phy_write16(sc, 0x0815, backup[7]);
			bcw_phy_write16(sc, BCW_PHY_G_CRS, backup[8]);
			bcw_phy_write16(sc, 0x0802, backup[9]);
		}
	}
	if (i >= 15)
		r = backup[13];

	return (r);
}

void
bcw_radio_init2060(struct bcw_softc *sc)
{
	int error;

	bcw_radio_write16(sc, 0x0004, 0x00c0);
	bcw_radio_write16(sc, 0x0005, 0x0008);
	bcw_radio_write16(sc, 0x0009, 0x0040);
	bcw_radio_write16(sc, 0x0005, 0x00aa);
	bcw_radio_write16(sc, 0x0032, 0x008f);
	bcw_radio_write16(sc, 0x0006, 0x008f);
	bcw_radio_write16(sc, 0x0034, 0x008f);
	bcw_radio_write16(sc, 0x002c, 0x0007);
	bcw_radio_write16(sc, 0x0082, 0x0080);
	bcw_radio_write16(sc, 0x0080, 0x0000);
	bcw_radio_write16(sc, 0x003f, 0x00d4);
	bcw_radio_write16(sc, 0x0005, bcw_radio_read16(sc, 0x0005) & ~0x0008);
	bcw_radio_write16(sc, 0x0081, bcw_radio_read16(sc, 0x0081) & ~0x0010);
	bcw_radio_write16(sc, 0x0081, bcw_radio_read16(sc, 0x0081) & ~0x0020);
	bcw_radio_write16(sc, 0x0081, bcw_radio_read16(sc, 0x0081) & ~0x0020);
	delay(400);
	bcw_radio_write16(sc, 0x0081, (bcw_radio_read16(sc, 0x0081) & ~0x0020) |
	    0x0010);
	delay(400);
	bcw_radio_write16(sc, 0x0005, (bcw_radio_read16(sc, 0x0005) & ~0x0020) |
	    0x0008);
	bcw_radio_write16(sc, 0x0085, bcw_radio_read16(sc, 0x0085) & ~0x0010);
	bcw_radio_write16(sc, 0x0005, bcw_radio_read16(sc, 0x0005) & ~0x0008);
	bcw_radio_write16(sc, 0x0081, bcw_radio_read16(sc, 0x0081) & ~0x0040);
	bcw_radio_write16(sc, 0x0081, (bcw_radio_read16(sc, 0x0081) & ~0x0040) |
	    0x0040);
	bcw_radio_write16(sc, 0x0005, (bcw_radio_read16(sc, 0x0081) & ~0x0008) |
	    0x0008);
	bcw_phy_write16(sc, 0x0063, 0xddc6);
	bcw_phy_write16(sc, 0x0069, 0x07be);
	bcw_phy_write16(sc, 0x006a, 0x0000);

	error = bcw_radio_select_channel(sc, BCW_RADIO_DEFAULT_CHANNEL_A, 0);

	/* XXX assert() */

	delay(1000);
}

void
bcw_radio_spw(struct bcw_softc *sc, uint8_t channel)
{
	if (sc->sc_radio_ver != 0x2050 || sc->sc_radio_rev >= 6)
		/* we do not need the workaround */
		return;

	if (channel <= 10)
		BCW_WRITE16(sc, BCW_MMIO_CHANNEL,
		    bcw_radio_chan2freq_bg(channel + 4));
	else
		BCW_WRITE16(sc, BCW_MMIO_CHANNEL, bcw_radio_chan2freq_bg(1));

	delay(100);

	BCW_WRITE16(sc, BCW_MMIO_CHANNEL, bcw_radio_chan2freq_bg(channel));
}

int
bcw_radio_select_channel(struct bcw_softc *sc, uint8_t channel, int spw)
{
	uint16_t freq, tmp, r8;

	if (sc->sc_radio_mnf == 0x17f && sc->sc_radio_ver == 0x2060 &&
	    sc->sc_radio_rev == 1) {
		freq = bcw_radio_chan2freq_a(channel);

		r8 = bcw_radio_read16(sc, 0x0008);
		BCW_WRITE16(sc, 0x03f0, freq);
		bcw_radio_write16(sc, 0x0008, r8);

		tmp = bcw_radio_read16(sc, 0x002e);
		tmp &= 0x0080;

		bcw_radio_write16(sc, 0x002e, tmp);

		if (freq >= 4920 && freq <= 5500)
			r8 = 3 * freq / 116;
		bcw_radio_write16(sc, 0x0007, (r8 << 4) | r8);
		bcw_radio_write16(sc, 0x0020, (r8 << 4) | r8);
		bcw_radio_write16(sc, 0x0021, (r8 << 4) | r8);
		bcw_radio_write16(sc, 0x0022, (bcw_radio_read16(sc, 0x0022) &
		    0x000f) | (r8 << 4));
		bcw_radio_write16(sc, 0x002a, (r8 << 4));
		bcw_radio_write16(sc, 0x002b, (r8 << 4));
		bcw_radio_write16(sc, 0x0008, (bcw_radio_read16(sc, 0x0008) &
		    0x00f0) | (r8 << 4));
		bcw_radio_write16(sc, 0x0029, (bcw_radio_read16(sc, 0x0029) &
		    0xff0f) | 0x00b0);
		bcw_radio_write16(sc, 0x0035, 0x00aa);
		bcw_radio_write16(sc, 0x0036, 0x0085);
		bcw_radio_write16(sc, 0x003a, (bcw_radio_read16(sc, 0x003a) &
		    0xff20) | bcw_radio_freq_r3a_value(freq));
		bcw_radio_write16(sc, 0x003d, bcw_radio_read16(sc, 0x003d) &
		    0x00ff);
		bcw_radio_write16(sc, 0x0081, (bcw_radio_read16(sc, 0x0081) &
		    0xff7f) | 0x0080);
		bcw_radio_write16(sc, 0x0035, bcw_radio_read16(sc, 0x0035) &
		    0xffef);
		bcw_radio_write16(sc, 0x0035, (bcw_radio_read16(sc, 0x0035) &
		    0xffef) | 0x0010);
		bcw_radio_set_tx_iq(sc);
		bcw_phy_xmitpower(sc);
	} else {
		if (spw)
			bcw_radio_spw(sc, channel);
		
		BCW_WRITE16(sc, BCW_MMIO_CHANNEL,
		    bcw_radio_chan2freq_bg(channel));

		if (channel == 14) {
			/* TODO */
		} else {
			BCW_WRITE16(sc, BCW_MMIO_CHANNEL_EXT,
			    BCW_READ16(sc, BCW_MMIO_CHANNEL_EXT) & 0xf7bf);
		}
	}

	sc->sc_radio_channel = channel;

	delay(8000);

	return (0);
}

uint16_t
bcw_radio_chan2freq_a(uint8_t channel)
{
	/* XXX assert() */

	return (5000 + 5 * channel);
}

uint16_t
bcw_radio_chan2freq_bg(uint8_t channel)
{
	static const uint16_t freqs_bg[14] = {
	    12, 17, 22, 27,
	    32, 37, 42, 47,
	    52, 57, 62, 67,
	    72, 84 };

	if (channel < 1 && channel > 14)
		return (0);

	return (freqs_bg[channel - 1]);
}

uint16_t
bcw_radio_default_baseband_attenuation(struct bcw_softc *sc)
{
	if (sc->sc_radio_ver == 0x2050 && sc->sc_radio_rev < 6)
		return (0);

	return (2);
}

uint16_t
bcw_radio_default_radio_attenuation(struct bcw_softc *sc)
{
	uint16_t att = 0xffff;

	if (sc->sc_phy_type == BCW_PHY_TYPEA)
		return (0x60);

	switch (sc->sc_radio_ver) {
	case 0x2053:
		switch (sc->sc_radio_rev) {
		case 1:
			att = 6;
			break;
		}
		break;
	case 0x2050:
		switch (sc->sc_radio_rev) {
		case 0:
			att = 5;
			break;
		case 1:
			if (sc->sc_phy_type == BCW_PHY_TYPEG) {
				if (sc->sc_board_vendor ==
				    PCI_VENDOR_BROADCOM &&
				    sc->sc_board_type == 0x0421 &&
				    sc->sc_board_rev >= 30)
					att = 3;
				else if (sc->sc_board_vendor ==
				    PCI_VENDOR_BROADCOM &&
				    sc->sc_board_type == 0x0416)
					att = 3;
				else
						att = 1;
			} else {
				if (sc->sc_board_vendor ==
				    PCI_VENDOR_BROADCOM &&
				    sc->sc_board_type == 0x0421 &&
				    sc->sc_board_rev >= 30)
					att = 7;
				else
					att = 6;
			}
			break;
		case 2:
			if (sc->sc_phy_type == BCW_PHY_TYPEG) {
				if (sc->sc_board_vendor ==
				    PCI_VENDOR_BROADCOM &&
				    sc->sc_board_type == 0x0421 &&
				    sc->sc_board_rev >= 30)
					att = 3;
				else if (sc->sc_board_vendor ==
				    PCI_VENDOR_BROADCOM &&
				    sc->sc_board_type == 0x0416)
					att = 5;
				else if (sc->sc_chip_id == 0x4320)
					att = 4;
				else
					att = 3;
			} else
				att = 6;
			break;
		case 3:
			att = 5;
			break;
		case 4:
		case 5:
			att = 1;
			break;
		case 6:
		case 7:
			att = 5;
			break;
		case 8:
			att = 0x1a;
			break;
		case 9:
		default:
			att = 5;
		}
	}
	if (sc->sc_board_vendor == PCI_VENDOR_BROADCOM &&
	    sc->sc_board_type == 0x0421) {
		if (sc->sc_board_rev < 0x43)
			att = 2;
		else if (sc->sc_board_rev < 0x51)
			att = 3; 
	}
	if (att == 0x0ffff)
		att = 5;

	return (att);
}

uint16_t
bcw_radio_default_txctl1(struct bcw_softc *sc)
{
	if (sc->sc_radio_ver != 0x2050)
		return (0);
	if (sc->sc_radio_rev == 1)
		return (3);
	if (sc->sc_radio_rev < 6)
		return (2);
	if (sc->sc_radio_rev == 8)
		return (1);

	return (0);
}

void
bcw_radio_clear_tssi(struct bcw_softc *sc)
{
	switch (sc->sc_phy_type) {
	case BCW_PHY_TYPEA:
		bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0068, 0x7f7f);
		bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x006a, 0x7f7f);
		break;
	case BCW_PHY_TYPEB:
	case BCW_PHY_TYPEG:
		bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0058, 0x7f7f);
		bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x005a, 0x7f7f);
		bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0070, 0x7f7f);
		bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0072, 0x7f7f);
		break;
	}
}

void
bcw_radio_set_tx_iq(struct bcw_softc *sc)
{
	const uint8_t data_high[5] = { 0x00, 0x40, 0x80, 0x90, 0xD0 };
	const uint8_t data_low[5] = { 0x00, 0x01, 0x05, 0x06, 0x0A };
	uint16_t tmp = bcw_radio_read16(sc, 0x001e);
	int i, j;

	for (i = 0; i < 5; i++) {
		for (j = 0; j < 5; j++) {
			if (tmp == (data_high[i] << 4 | data_low[j])) {
				bcw_phy_write16(sc, 0x0069, (i - j) << 8 |
				    0x00c0);
				return;
			}
		}
	}
}

uint16_t
bcw_radio_get_txgain_baseband(uint16_t txpower)
{
	uint16_t r;

	/* XXX assert(txpower <= 63); ? */

	if (txpower >= 54)
		r = 2;
	else if (txpower >= 49)
		r = 4;
	else if (txpower >= 44)
		r = 5;
	else
		r = 6;

	return (r);
}

uint16_t
bcw_radio_get_txgain_freq_power_amp(uint16_t txpower)
{
	uint16_t r;

	/* XXX assert(txpower <= 63); ? */

	if (txpower >= 32)
		r = 0;
	else if (txpower >= 25)
		r = 1;
	else if (txpower >= 20)
		r = 2;
	else if (txpower >= 12)
		r = 3;
	else
		r = 4;

	return (r);
}

uint16_t
bcw_radio_get_txgain_dac(uint16_t txpower)
{
	uint16_t r;

	/* XXX assert(txpower <= 63); ? */

	if (txpower >= 54)
		r = txpower - 53;
	else if (txpower >= 49)
		r = txpower - 42;
	else if (txpower >= 44)
		r = txpower - 37;
	else if (txpower >= 32)
		r = txpower - 32;
	else if (txpower >= 25)
		r = txpower - 20;
	else if (txpower >= 20)
		r = txpower - 13;
	else if (txpower >= 12)
		r = txpower - 8;
	else
		r = txpower;

	return (r);
}

uint16_t
bcw_radio_freq_r3a_value(uint16_t frequency)
{
	uint16_t val;

	if (frequency < 5091)
		val = 0x0040;
	else if (frequency < 5321)
		val = 0;
	else if (frequency < 5806)
		val = 0x0080;
	else
		val = 0x0040;

	return (val);
}

void
bcw_radio_prepare_init(struct bcw_softc *sc)
{
	int i;

	/* set default attenuation values */
	sc->sc_radio_baseband_atten =
	    bcw_radio_default_baseband_attenuation(sc);
	sc->sc_radio_radio_atten =
	    bcw_radio_default_radio_attenuation(sc);
	sc->sc_radio_txctl1 = bcw_radio_default_txctl1(sc);
	sc->sc_radio_txctl2 = 0xffff;
	sc->sc_radio_txpwr_offset = 0;

	/* nrssi */
	sc->sc_radio_nrssislope = 0;
	for (i = 0; i < BCW_ARRAY_SIZE(sc->sc_radio_nrssi); i++)
		sc->sc_radio_nrssi[i] = -1000;
	for (i = 0; i < BCW_ARRAY_SIZE(sc->sc_radio_nrssi_lt); i++)
		sc->sc_radio_nrssi_lt[i] = i;

	sc->sc_radio_lofcal = 0xffff;
	sc->sc_radio_initval = 0xffff;

	sc->sc_radio_aci_enable = 0;
	sc->sc_radio_aci_wlan_automatic = 0;
	sc->sc_radio_aci_hw_rssi = 0;
}

int
bcw_radio_set_interference_mitigation(struct bcw_softc *sc, int mode)
{
	int currentmode;

	if (sc->sc_phy_type != BCW_PHY_TYPEG || sc->sc_phy_type == 0 ||
	    sc->sc_phy_connected == 0)
		return (ENODEV);

	sc->sc_radio_aci_wlan_automatic = 0;
	switch (mode) {
	case BCW_RADIO_INTERFMODE_AUTOWLAN:
		sc->sc_radio_aci_wlan_automatic = 1;
		if (sc->sc_radio_aci_enable)
			mode = BCW_RADIO_INTERFMODE_MANUALWLAN;
		else
			mode = BCW_RADIO_INTERFMODE_NONE;
		break;
	case BCW_RADIO_INTERFMODE_NONE:
	case BCW_RADIO_INTERFMODE_NONWLAN:
	case BCW_RADIO_INTERFMODE_MANUALWLAN:
		break;
	default:
		return (EINVAL);
	}

	currentmode = sc->sc_radio_interfmode;
	if (currentmode == mode)
		return (0);
	if (currentmode != BCW_RADIO_INTERFMODE_NONE) {
		sc->sc_radio_aci_enable = 0;
		sc->sc_radio_aci_hw_rssi = 0;
	} else
		bcw_radio_interference_mitigation_enable(sc, mode);
	sc->sc_radio_interfmode = mode;

	return (0);
}

int
bcw_radio_interference_mitigation_enable(struct bcw_softc *sc, int mode)
{
	uint16_t tmp, flipped;
	uint32_t tmp32;
	size_t stackidx = 0;
	uint32_t *stack = sc->sc_radio_interfstack;

	switch (mode) {
	case BCW_RADIO_INTERFMODE_NONWLAN:
		if (sc->sc_phy_rev != 1) {
			bcw_phy_write16(sc, 0x042b,
			    bcw_phy_read16(sc, 0x042b) | 0x0800);
			bcw_phy_write16(sc, BCW_PHY_G_CRS,
			    bcw_phy_read16(sc, BCW_PHY_G_CRS) & ~0x4000);
			break;
		}
		/* XXX radio_stacksave() */
		tmp = (bcw_radio_read16(sc, 0x0078) & 0x001e);
		flipped = tmp; /* XXX flip_4bit(tmp) */
		if (flipped < 10 && flipped >= 8)
			flipped = 7;
		else if (flipped >= 10)
			flipped -= 3;
		flipped = flipped; /* XXX flip_4bit(flipped) */
		flipped = (flipped << 1) | 0x0020;
		bcw_radio_write16(sc, 0x0078, flipped);

		bcw_radio_calc_nrssi_threshold(sc);

		BCW_PHY_STACKSAVE(0x0406);
		bcw_phy_write16(sc, 0x0406, 0x7e28);

		bcw_phy_write16(sc, 0x042b,
		    bcw_phy_read16(sc, 0x042b) | 0x08000);
		bcw_phy_write16(sc, BCW_PHY_RADIO_BITFIELD,
		    bcw_phy_read16(sc, BCW_PHY_RADIO_BITFIELD) | 0x1000);
		BCW_PHY_STACKSAVE(0x04a0);
		bcw_phy_write16(sc, 0x04a0,
		    (bcw_phy_read16(sc, 0x04a0) & 0xc0c0) | 0x0008);
		BCW_PHY_STACKSAVE(0x04a1);
		bcw_phy_write16(sc, 0x04a1,
		    (bcw_phy_read16(sc, 0x04a1) & 0xc0c0) | 0x0605);
		BCW_PHY_STACKSAVE(0x04a2);
		bcw_phy_write16(sc, 0x04a2,
		    (bcw_phy_read16(sc, 0x04a2) & 0xc0c0) | 0x0204);
		BCW_PHY_STACKSAVE(0x04a8);
		bcw_phy_write16(sc, 0x04a8,
		    (bcw_phy_read16(sc, 0x04a8) & 0xc0c0) | 0x0803);
		BCW_PHY_STACKSAVE(0x04ab);
		bcw_phy_write16(sc, 0x04ab,
		    (bcw_phy_read16(sc, 0x04ab) & 0xc0c0) | 0x0605);

		BCW_PHY_STACKSAVE(0x04a7);
		bcw_phy_write16(sc, 0x04a7, 0x0002);
		BCW_PHY_STACKSAVE(0x04a3);
		bcw_phy_write16(sc, 0x04a3, 0x287a);
		BCW_PHY_STACKSAVE(0x04a9);
		bcw_phy_write16(sc, 0x04a9, 0x2027);
		BCW_PHY_STACKSAVE(0x0493);
		bcw_phy_write16(sc, 0x0493, 0x32f5);
		BCW_PHY_STACKSAVE(0x04aa);
		bcw_phy_write16(sc, 0x04aa, 0x2027);
		BCW_PHY_STACKSAVE(0x04ac);
		bcw_phy_write16(sc, 0x04ac, 0x32f5);
		break;
	case BCW_RADIO_INTERFMODE_MANUALWLAN:
		if (bcw_phy_read16(sc, 0x0033) & 0x0800)
			break;

		sc->sc_radio_aci_enable = 1;

		BCW_PHY_STACKSAVE(BCW_PHY_RADIO_BITFIELD);
		BCW_PHY_STACKSAVE(BCW_PHY_G_CRS);
		if (sc->sc_phy_rev < 2)
			BCW_PHY_STACKSAVE(0x0406);
		else {
			BCW_PHY_STACKSAVE(0x04c0);
			BCW_PHY_STACKSAVE(0x04c1);
		}
		BCW_PHY_STACKSAVE(0x0033);
		BCW_PHY_STACKSAVE(0x04a7);
		BCW_PHY_STACKSAVE(0x04a3);
		BCW_PHY_STACKSAVE(0x04a9);
		BCW_PHY_STACKSAVE(0x04aa);
		BCW_PHY_STACKSAVE(0x04ac);
		BCW_PHY_STACKSAVE(0x0493);
		BCW_PHY_STACKSAVE(0x04a1);
		BCW_PHY_STACKSAVE(0x04a0);
		BCW_PHY_STACKSAVE(0x04a2);
		BCW_PHY_STACKSAVE(0x048a);
		BCW_PHY_STACKSAVE(0x04a8);
		BCW_PHY_STACKSAVE(0x04ab);
		if (sc->sc_phy_rev == 2) {
			BCW_PHY_STACKSAVE(0x04ad);
			BCW_PHY_STACKSAVE(0x04ae);
		} else if (sc->sc_phy_rev >= 3) {
			BCW_PHY_STACKSAVE(0x04ad);
			BCW_PHY_STACKSAVE(0x0415);
			BCW_PHY_STACKSAVE(0x0416);
			BCW_PHY_STACKSAVE(0x0417);
			BCW_ILT_STACKSAVE(0x1a00 + 0x2);
			BCW_ILT_STACKSAVE(0x1a00 + 0x3);
		}
		BCW_PHY_STACKSAVE(0x042b);
		BCW_PHY_STACKSAVE(0x048c);

		bcw_phy_write16(sc, BCW_PHY_RADIO_BITFIELD,
		    bcw_phy_read16(sc, BCW_PHY_RADIO_BITFIELD) & ~0x1000);
		bcw_phy_write16(sc, BCW_PHY_G_CRS,
		    (bcw_phy_read16(sc, BCW_PHY_G_CRS) & 0xfffc) | 0x0002);

		bcw_phy_write16(sc, 0x0033, 0x0800);
		bcw_phy_write16(sc, 0x04a3, 0x2027);
		bcw_phy_write16(sc, 0x04a9, 0x1ca8);
		bcw_phy_write16(sc, 0x0493, 0x287a);
		bcw_phy_write16(sc, 0x04aa, 0x1ca8);
		bcw_phy_write16(sc, 0x04ac, 0x287a);

		bcw_phy_write16(sc, 0x04a0,
		    (bcw_phy_read16(sc, 0x04a0) & 0xffc0) | 0x001a);
		bcw_phy_write16(sc, 0x04a7, 0x000d);

		if (sc->sc_phy_rev < 2) {
			bcw_phy_write16(sc, 0x0406, 0xff0d);
		} else if (sc->sc_phy_rev == 2) {
			bcw_phy_write16(sc, 0x04c0, 0xffff);
			bcw_phy_write16(sc, 0x04c1, 0x00a9);
		} else {
			bcw_phy_write16(sc, 0x04c0, 0x00c1);
			bcw_phy_write16(sc, 0x04c1, 0x0059);
		}

		bcw_phy_write16(sc, 0x04a1,
		    (bcw_phy_read16(sc, 0x04a1) & 0xc0ff) | 0x1800);
		bcw_phy_write16(sc, 0x04a1,
		    (bcw_phy_read16(sc, 0x04a1) & 0xc0ff) | 0x0015);
		bcw_phy_write16(sc, 0x04a8,
		    (bcw_phy_read16(sc, 0x04a8) & 0xcfff) | 0x1000);
		bcw_phy_write16(sc, 0x04a8,
		    (bcw_phy_read16(sc, 0x04a8) & 0xf0ff) | 0x0a00);
		bcw_phy_write16(sc, 0x04ab,
		    (bcw_phy_read16(sc, 0x04a8) & 0xcfff) | 0x1000);
		bcw_phy_write16(sc, 0x04ab,
		    (bcw_phy_read16(sc, 0x04ab) & 0xf0ff) | 0x0800);
		bcw_phy_write16(sc, 0x04ab,
		    (bcw_phy_read16(sc, 0x04ab) & 0xf0ff) | 0x0010);
		bcw_phy_write16(sc, 0x04ab,
		    (bcw_phy_read16(sc, 0x04ab) & 0xfff0) | 0x0005);
		bcw_phy_write16(sc, 0x04a8,
		    (bcw_phy_read16(sc, 0x04a8) & 0xffcf) | 0x0010);
		bcw_phy_write16(sc, 0x04a8,
		    (bcw_phy_read16(sc, 0x04a8) & 0xfff0) | 0x0006);
		bcw_phy_write16(sc, 0x04a2,
		    (bcw_phy_read16(sc, 0x04a2) & 0xf0ff) | 0x0800);
		bcw_phy_write16(sc, 0x04a0,
		    (bcw_phy_read16(sc, 0x04a0) & 0xf0ff) | 0x0500);
		bcw_phy_write16(sc, 0x04a2,
		    (bcw_phy_read16(sc, 0x04a2) & 0xfff0) | 0x000b);

		if (sc->sc_phy_rev >= 3) {
			bcw_phy_write16(sc, 0x048a,
			    bcw_phy_read16(sc, 0x048a) & ~0x8000);
			bcw_phy_write16(sc, 0x0415,
			    (bcw_phy_read16(sc, 0x0415) & 0x8000) | 0x36d8);
			bcw_phy_write16(sc, 0x0416,
			    (bcw_phy_read16(sc, 0x0416) & 0x8000) | 0x36d8);
			bcw_phy_write16(sc, 0x0417,
			    (bcw_phy_read16(sc, 0x0417) & 0xfe00) | 0x016d);
		} else {
			bcw_phy_write16(sc, 0x048a,
			    bcw_phy_read16(sc, 0x048a) | 0x1000);
			bcw_phy_write16(sc, 0x048a,
			    (bcw_phy_read16(sc, 0x048a) & 0x9fff) | 0x2000);
			tmp32 = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED,
			    BCW_SHM_MICROCODEFLAGSLOW);
			if (!(tmp32 & 0x800)) {
				tmp32 |= 0x800;
				bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED,
				    BCW_SHM_MICROCODEFLAGSLOW, tmp32);
			}
		}
		if (sc->sc_phy_rev >= 2) {
			bcw_phy_write16(sc, 0x042b, bcw_phy_read16(sc, 0x042b) |
			    0x0800);
		}
		bcw_phy_write16(sc, 0x048c, (bcw_phy_read16(sc, 0x048c) &
		    0xff00) | 0x007f);
		if (sc->sc_phy_rev == 2) {
			bcw_phy_write16(sc, 0x04ae,
			    (bcw_phy_read16(sc, 0x04ae) & 0xff00) | 0x007f);
			bcw_phy_write16(sc, 0x04ad,
			    (bcw_phy_read16(sc, 0x04ad) & 0x00ff) | 0x1300);
		} else if (sc->sc_phy_rev >= 6) {
			bcw_ilt_write(sc, 0x1a00 + 0x3, 0x007f);
			bcw_ilt_write(sc, 0x1a00 + 0x2, 0x007f);
			bcw_phy_write16(sc, 0x04ad,
			    bcw_phy_read16(sc, 0x04ad) & 0x00ff);
		}
		bcw_radio_calc_nrssi_slope(sc);
		break;
	default:
		 /* XXX assert () */
		break;
	}

	return (0);
}

void
bcw_radio_set_txantenna(struct bcw_softc *sc, uint32_t val)
{
	uint16_t tmp;

	val <<= 8;
	tmp = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, 0x0022) & 0xfcff;
	bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0022, tmp | val);
	tmp = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, 0x03a8) & 0xfcff;
	bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x03a8, tmp | val);
	tmp = bcw_shm_read16(sc, BCW_SHM_CONTROL_SHARED, 0x0054) & 0xfcff;
	bcw_shm_write16(sc, BCW_SHM_CONTROL_SHARED, 0x0054, tmp | val);
}

/*
 * ILT
 */
void
bcw_ilt_write(struct bcw_softc *sc, uint16_t offset, uint16_t val)
{
	if (sc->sc_phy_type == BCW_PHY_TYPEA) {
		bcw_phy_write16(sc, BCW_PHY_ILT_A_CTRL, offset);
		bcw_phy_write16(sc, BCW_PHY_ILT_A_DATA1, val);
	} else {
		bcw_phy_write16(sc, BCW_PHY_ILT_G_CTRL, offset);
		bcw_phy_write16(sc, BCW_PHY_ILT_G_DATA1, val);
	}
}

uint16_t
bcw_ilt_read(struct bcw_softc *sc, uint16_t offset)
{
        if (sc->sc_phy_type == BCW_PHY_TYPEA) {
                bcw_phy_write16(sc, BCW_PHY_ILT_A_CTRL, offset);
                return (bcw_phy_read16(sc, BCW_PHY_ILT_A_DATA1));
        } else {
                bcw_phy_write16(sc, BCW_PHY_ILT_G_CTRL, offset);
                return (bcw_phy_read16(sc, BCW_PHY_ILT_G_DATA1));
        }
}
