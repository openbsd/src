/*	$OpenBSD: ar5xxx.h,v 1.4 2004/11/08 16:48:25 reyk Exp $	*/

/*
 * Copyright (c) 2004 Reyk Floeter <reyk@vantronix.net>.
 *
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT
 * OF THIRD PARTY RIGHTS. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
 * HOLDERS INCLUDED IN THIS NOTICE BE LIABLE FOR ANY CLAIM, OR ANY
 * SPECIAL INDIRECT OR CONSEQUENTIAL DAMAGES, OR ANY DAMAGES WHATSOEVER
 * RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF
 * CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN
 * CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * HAL interface for Atheros Wireless LAN devices.
 *
 * ar5k is a free replacement of the binary-only HAL used by some drivers
 * for Atheros chipsets. While using a different ABI, it tries to be
 * source-compatible with the original (non-free) HAL interface.
 *
 * Many thanks to various contributors who supported the development of
 * ar5k with hard work and useful information. And, of course, for all the
 * people who encouraged me to continue this work which has been based
 * on my initial approach found on http://team.vantronix.net/ar5k/.
 */

#ifndef _AR5K_H
#define _AR5K_H

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/sysctl.h>
#include <sys/malloc.h>
#include <sys/lock.h>
#include <sys/kernel.h>
#include <sys/socket.h>
#include <sys/sockio.h>
#include <sys/errno.h>

#include <machine/endian.h>
#include <machine/bus.h>

#include <net/if.h>
#include <net/if_dl.h>
#include <net/if_media.h>
#include <net/if_arp.h>
#include <net/if_llc.h>

#ifdef INET
#include <netinet/in.h>
#include <netinet/if_ether.h>
#endif

#include <net80211/ieee80211_var.h>
#include <net80211/ieee80211_compat.h>
#include <net80211/ieee80211_regdomain.h>

/*
 * Generic definitions
 */

typedef enum {
	AH_FALSE = 0,
	AH_TRUE,
} HAL_BOOL;

typedef enum {
	HAL_MODE_11A = 0x001,
	HAL_MODE_TURBO = 0x002,
	HAL_MODE_11B = 0x004,
	HAL_MODE_PUREG = 0x008,
	HAL_MODE_11G = 0x008, /* 0x010 for dynamic OFDM/CCK */
	HAL_MODE_108G = 0x020,
	HAL_MODE_ALL = 0xfff
} HAL_MODE;

typedef enum {
	HAL_M_STA = 1,
	HAL_M_IBSS = 0,
	HAL_M_HOSTAP = 6,
	HAL_M_MONITOR = 8,
} HAL_OPMODE;

typedef int HAL_STATUS;

#define HAL_OK		0
#define HAL_EINPROGRESS EINPROGRESS

/*
 * TX queues
 */

typedef enum {
	HAL_TX_QUEUE_INACTIVE = 0,
	HAL_TX_QUEUE_DATA,
	HAL_TX_QUEUE_BEACON,
	HAL_TX_QUEUE_CAB,
	HAL_TX_QUEUE_PSPOLL,
} HAL_TX_QUEUE;

#define HAL_NUM_TX_QUEUES	10

typedef enum {
	HAL_WME_AC_BK = 0,
	HAL_WME_AC_BE = 1,
	HAL_WME_AC_VI = 2,
	HAL_WME_AC_VO = 3,
	HAL_WME_UPSD = 4,
} HAL_TX_QUEUE_SUBTYPE;

#define AR5K_TXQ_FLAG_TXINT_ENABLE		0x0001
#define AR5K_TXQ_FLAG_TXDESCINT_ENABLE		0x0002
#define AR5K_TXQ_FLAG_BACKOFF_DISABLE		0x0004
#define AR5K_TXQ_FLAG_COMPRESSION_ENABLE	0x0008
#define AR5K_TXQ_FLAG_RDYTIME_EXP_POLICY_ENABLE	0x0010
#define AR5K_TXQ_FLAG_FRAG_BURST_BACKOFF_ENABLE	0x0020

typedef struct {
	u_int32_t		tqi_ver;
	HAL_TX_QUEUE		tqi_type;
	HAL_TX_QUEUE_SUBTYPE	tqi_subtype;
	u_int16_t		tqi_flags;
	u_int32_t		tqi_priority;
	u_int32_t		tqi_aifs;
	int32_t			tqi_cw_min;
	int32_t			tqi_cw_max;
	u_int32_t		tqi_cbr_period;
	u_int32_t		tqi_cbr_overflow_limit;
	u_int32_t		tqi_burst_time;
	u_int32_t		tqi_ready_time;
} HAL_TXQ_INFO;

typedef enum {
	HAL_PKT_TYPE_NORMAL = 0,
	HAL_PKT_TYPE_ATIM,
	HAL_PKT_TYPE_PSPOLL,
	HAL_PKT_TYPE_BEACON,
	HAL_PKT_TYPE_PROBE_RESP,
	HAL_PKT_TYPE_PIFS,
} HAL_PKT_TYPE;

/*
 * Used to compute TX times
 */

#define AR5K_CCK_SIFS_TIME		10
#define AR5K_CCK_PREAMBLE_BITS		144
#define AR5K_CCK_PLCP_BITS		48
#define AR5K_CCK_NUM_BITS(_frmlen) (_frmlen << 3)
#define AR5K_CCK_PHY_TIME(_sp) (_sp ?					\
        ((AR5K_CCK_PREAMBLE_BITS + AR5K_CCK_PLCP_BITS) >> 1) :		\
        (AR5K_CCK_PREAMBLE_BITS + AR5K_CCK_PLCP_BITS))
#define AR5K_CCK_TX_TIME(_kbps, _frmlen, _sp)				\
        AR5K_CCK_PHY_TIME(_sp) +					\
        ((AR5K_CCK_NUM_BITS(_frmlen) * 1000) / _kbps) +			\
        AR5K_CCK_SIFS_TIME

#define AR5K_OFDM_SIFS_TIME		16
#define AR5K_OFDM_PREAMBLE_TIME	20
#define AR5K_OFDM_PLCP_BITS		22
#define AR5K_OFDM_SYMBOL_TIME		4
#define AR5K_OFDM_NUM_BITS(_frmlen) (AR5K_OFDM_PLCP_BITS + (_frmlen << 3))
#define AR5K_OFDM_NUM_BITS_PER_SYM(_kbps) ((_kbps *			\
        AR5K_OFDM_SYMBOL_TIME) / 1000)
#define AR5K_OFDM_NUM_BITS(_frmlen) (AR5K_OFDM_PLCP_BITS + (_frmlen << 3))
#define AR5K_OFDM_NUM_SYMBOLS(_kbps, _frmlen)				\
        howmany(AR5K_OFDM_NUM_BITS(_frmlen), AR5K_OFDM_NUM_BITS_PER_SYM(_kbps))
#define AR5K_OFDM_TX_TIME(_kbps, _frmlen)				\
        AR5K_OFDM_PREAMBLE_TIME + AR5K_OFDM_SIFS_TIME +			\
        (AR5K_OFDM_NUM_SYMBOLS(_kbps, _frmlen) * AR5K_OFDM_SYMBOL_TIME)

#define AR5K_TURBO_SIFS_TIME		8
#define AR5K_TURBO_PREAMBLE_TIME	14
#define AR5K_TURBO_PLCP_BITS		22
#define AR5K_TURBO_SYMBOL_TIME		4
#define AR5K_TURBO_NUM_BITS(_frmlen) (AR5K_TURBO_PLCP_BITS + (_frmlen << 3))
#define AR5K_TURBO_NUM_BITS_PER_SYM(_kbps) (((_kbps << 1) *		\
        AR5K_TURBO_SYMBOL_TIME) / 1000)
#define AR5K_TURBO_NUM_BITS(_frmlen) (AR5K_TURBO_PLCP_BITS + (_frmlen << 3))
#define AR5K_TURBO_NUM_SYMBOLS(_kbps, _frmlen)				\
        howmany(AR5K_TURBO_NUM_BITS(_frmlen),				\
        AR5K_TURBO_NUM_BITS_PER_SYM(_kbps))
#define AR5K_TURBO_TX_TIME(_kbps, _frmlen)				\
        AR5K_TURBO_PREAMBLE_TIME + AR5K_TURBO_SIFS_TIME +		\
        (AR5K_TURBO_NUM_SYMBOLS(_kbps, _frmlen) * AR5K_TURBO_SYMBOL_TIME)

#define AR5K_XR_SIFS_TIME		16
#define AR5K_XR_PLCP_BITS		22
#define AR5K_XR_SYMBOL_TIME		4
#define AR5K_XR_PREAMBLE_TIME(_kbps) (((_kbps) < 1000) ? 173 : 76)
#define AR5K_XR_NUM_BITS_PER_SYM(_kbps) ((_kbps *			\
        AR5K_XR_SYMBOL_TIME) / 1000)
#define AR5K_XR_NUM_BITS(_frmlen) (AR5K_XR_PLCP_BITS + (_frmlen << 3))
#define AR5K_XR_NUM_SYMBOLS(_kbps, _frmlen)				\
        howmany(AR5K_XR_NUM_BITS(_frmlen), AR5K_XR_NUM_BITS_PER_SYM(_kbps))
#define AR5K_XR_TX_TIME(_kbps, _frmlen)					\
        AR5K_XR_PREAMBLE_TIME(_kbps) + AR5K_XR_SIFS_TIME +		\
        (AR5K_XR_NUM_SYMBOLS(_kbps, _frmlen) * AR5K_XR_SYMBOL_TIME)

/*
 * RX definitions
 */

#define	HAL_RX_FILTER_UCAST	0x00000001
#define	HAL_RX_FILTER_MCAST	0x00000002
#define	HAL_RX_FILTER_BCAST	0x00000004
#define	HAL_RX_FILTER_CONTROL	0x00000008
#define	HAL_RX_FILTER_BEACON	0x00000010
#define	HAL_RX_FILTER_PROM	0x00000020
#define	HAL_RX_FILTER_PROBEREQ	0x00000080
#define	HAL_RX_FILTER_PHYERR	0x00000100
#define	HAL_RX_FILTER_PHYRADAR	0x00000200

typedef struct {
	u_int32_t	ackrcv_bad;
	u_int32_t	rts_bad;
	u_int32_t	rts_good;
	u_int32_t	fcs_bad;
	u_int32_t	beacons;
} HAL_MIB_STATS;

/*
 * Beacon/AP definitions
 */

#define	HAL_BEACON_PERIOD	0x0000ffff
#define	HAL_BEACON_ENA		0x00800000
#define	HAL_BEACON_RESET_TSF	0x01000000

typedef struct {
	u_int32_t	bs_next_beacon;
	u_int32_t	bs_next_dtim;
	u_int32_t	bs_interval;
	u_int8_t	bs_dtim_period;
	u_int8_t	bs_cfp_period;
	u_int16_t	bs_cfp_max_duration;
	u_int16_t	bs_cfp_du_remain;
	u_int16_t	bs_tim_offset;
	u_int16_t	bs_sleep_duration;
	u_int16_t	bs_bmiss_threshold;

#define bs_nexttbtt		bs_next_beacon
#define bs_intval		bs_interval
#define bs_nextdtim		bs_next_dtim
#define bs_bmissthreshold	bs_bmiss_threshold
#define bs_sleepduration	bs_sleep_duration
#define bs_dtimperiod		bs_dtim_period

} HAL_BEACON_STATE;

/*
 * Power management
 */

typedef enum {
	HAL_PM_UNDEFINED = 0,
	HAL_PM_AUTO,
	HAL_PM_AWAKE,
	HAL_PM_FULL_SLEEP,
	HAL_PM_NETWORK_SLEEP,
} HAL_POWER_MODE;

/*
 * Weak wireless crypto definitions (use IPsec/WLSec/...)
 */

typedef enum {
	HAL_CIPHER_WEP = 0,
	HAL_CIPHER_AES_CCM,
	HAL_CIPHER_CKIP,
} HAL_CIPHER;

#define AR5K_MAX_KEYS	16

typedef struct {
	int		wk_len;
	u_int8_t	wk_key[AR5K_MAX_KEYS];
} HAL_KEYVAL;

#define AR5K_ASSERT_ENTRY(_e, _s) do {					\
        if (_e >= _s)							\
                return (AH_FALSE);					\
} while (0)

/*
 * PHY
 */

#define AR5K_MAX_RATES	32

typedef struct {
	u_int8_t	valid;
	u_int8_t	phy;
	u_int16_t	rateKbps;
	u_int8_t	rateCode;
	u_int8_t	shortPreamble;
	u_int8_t	dot11Rate;
	u_int8_t	controlRate;

#define r_valid			valid
#define r_phy			phy
#define r_rate_kbps		rateKbps
#define r_short_preamble	short_preamble
#define r_dot11_rate		dot11Rate
#define r_control_rate		controlRate

} HAL_RATE;

typedef struct {
	u_int16_t	rateCount;
	u_int8_t	rateCodeToIndex[AR5K_MAX_RATES];
	HAL_RATE	info[AR5K_MAX_RATES];

#define rt_rate_count		rateCount
#define rt_rate_code_index	rateCodeToIndex
#define rt_info			info

} HAL_RATE_TABLE;

#define AR5K_RATES_11A { 8, { 0 }, {					\
        { 1, IEEE80211_T_OFDM, 6000, 11, 0, 140, 0 },			\
        { 1, IEEE80211_T_OFDM, 9000, 15, 0, 18, 0 },			\
        { 1, IEEE80211_T_OFDM, 12000, 10, 0, 152, 2 },			\
        { 1, IEEE80211_T_OFDM, 18000, 14, 0, 36, 2 },			\
        { 1, IEEE80211_T_OFDM, 24000, 9, 0, 176, 4 },			\
        { 1, IEEE80211_T_OFDM, 36000, 13, 0, 72, 4 },			\
        { 1, IEEE80211_T_OFDM, 48000, 8, 0, 96, 4 },			\
        { 1, IEEE80211_T_OFDM, 54000, 12, 0, 108, 4 } }			\
}

#define AR5K_RATES_TURBO { 8, { 0 }, {					\
        { 1, IEEE80211_T_TURBO, 6000, 11, 0, 140, 0 },			\
        { 1, IEEE80211_T_TURBO, 9000, 15, 0, 18, 0 },			\
        { 1, IEEE80211_T_TURBO, 12000, 10, 0, 152, 2 },			\
        { 1, IEEE80211_T_TURBO, 18000, 14, 0, 36, 2 },			\
        { 1, IEEE80211_T_TURBO, 24000, 9, 0, 176, 4 },			\
        { 1, IEEE80211_T_TURBO, 36000, 13, 0, 72, 4 },			\
        { 1, IEEE80211_T_TURBO, 48000, 8, 0, 96, 4 },			\
        { 1, IEEE80211_T_TURBO, 54000, 12, 0, 108, 4 } }		\
}

/* XXX TODO: 2GHz rates for 11b/11g */
#define AR5K_RATES_11B { 0, }
#define AR5K_RATES_11G { 0, }

typedef enum {
	HAL_RFGAIN_INACTIVE = 0,
	HAL_RFGAIN_READ_REQUESTED,
	HAL_RFGAIN_NEED_CHANGE,
} HAL_RFGAIN;

typedef struct {
	u_int16_t	channel; /* MHz */
	u_int16_t	channelFlags;

#define c_channel	channel
#define c_channel_flags	cnannelFlags

} HAL_CHANNEL;

#define HAL_SLOT_TIME_9		9
#define	HAL_SLOT_TIME_20	20
#define HAL_SLOT_TIME_MAX	ar5k_clocktoh(0xffff, hal->ah_turbo)

#define CHANNEL_A	(IEEE80211_CHAN_5GHZ | IEEE80211_CHAN_OFDM)
#define CHANNEL_B	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_CCK)
#define CHANNEL_G	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM) /* _DYN */
#define CHANNEL_PUREG	(IEEE80211_CHAN_2GHZ | IEEE80211_CHAN_OFDM)
#define CHANNEL_T	(CHANNEL_A | IEEE80211_CHAN_TURBO)
#define CHANNEL_TG	(CHANNEL_PUREG | IEEE80211_CHAN_TURBO)
#define CHANNEL_XR	(CHANNEL_A | IEEE80211_CHAN_XR)

/*
 * Regulation stuff
 */

typedef enum ieee80211_countrycode HAL_CTRY_CODE;

/*
 * HAL interrupt abstraction
 */

#define	HAL_INT_RX	0x00000001
#define	HAL_INT_RXDESC	0x00000002
#define	HAL_INT_RXNOFRM	0x00000008
#define HAL_INT_RXEOL	0x00000010
#define HAL_INT_RXORN	0x00000020
#define HAL_INT_TX	0x00000040
#define HAL_INT_TXDESC	0x00000080
#define HAL_INT_TXURN	0x00000800
#define HAL_INT_MIB	0x00001000
#define HAL_INT_RXPHY	0x00004000
#define HAL_INT_RXKCM	0x00008000
#define HAL_INT_SWBA	0x00010000
#define HAL_INT_BMISS	0x00040000
#define HAL_INT_BNR	0x00100000
#define HAL_INT_GPIO	0x01000000
#define HAL_INT_FATAL	0x40000000
#define HAL_INT_GLOBAL	0x80000000
#define HAL_INT_NOCARD	0xffffffff
#define HAL_INT_COMMON	(						\
        HAL_INT_RXNOFRM | HAL_INT_RXDESC | HAL_INT_RXEOL |		\
	HAL_INT_RXORN | HAL_INT_TXURN | HAL_INT_TXDESC |		\
	HAL_INT_MIB | HAL_INT_RXPHY | HAL_INT_RXKCM |			\
	HAL_INT_SWBA | HAL_INT_BMISS | HAL_INT_GPIO			\
)

typedef u_int32_t HAL_INT;

/*
 * LED states
 */

typedef enum ieee80211_state HAL_LED_STATE;

#define HAL_LED_INIT	IEEE80211_S_INIT
#define HAL_LED_SCAN	IEEE80211_S_SCAN
#define HAL_LED_AUTH	IEEE80211_S_AUTH
#define HAL_LED_ASSOC	IEEE80211_S_ASSOC
#define HAL_LED_RUN	IEEE80211_S_RUN

/*
 * Chipset capabilities
 */

typedef struct {
	/*
	 * Supported PHY modes
	 * (ie. IEEE80211_CHAN_A, IEEE80211_CHAN_B, ...)
	 */
	u_int16_t	cap_mode;

	/*
	 * Frequency range (without regulation restrictions)
	 */
	struct {
		u_int16_t	range_2ghz_min;
		u_int16_t	range_2ghz_max;
		u_int16_t	range_5ghz_min;
		u_int16_t	range_5ghz_max;
	} cap_range;

	/*
	 * Active regulation domain settings
	 */
	struct {
		ieee80211_regdomain_t	reg_current;
		ieee80211_regdomain_t	reg_hw;
	} cap_regdomain;

	/*
	 * Values stored in the EEPROM (some of them...)
	 */
	struct {
		u_int16_t	ee_magic;
		u_int16_t	ee_antenna;
		u_int16_t	ee_protect;
		u_int16_t	ee_regdomain;
		u_int8_t	ee_rfkill;
		u_int16_t	ee_version;
	} cap_eeprom;

	/*
	 * Queue information
	 */
	struct {
		u_int8_t	q_tx_num;
	} cap_queues;
} ar5k_capabilities_t;

/*
 * Atheros descriptor definitions
 */

struct ath_tx_status {
	u_int16_t	ts_seqnum;
	u_int16_t	ts_tstamp;
	u_int8_t	ts_status;
	u_int8_t	ts_rate;
	int8_t		ts_rssi;
	u_int8_t	ts_shortretry;
	u_int8_t	ts_longretry;
	u_int8_t	ts_virtcol;
	u_int8_t	ts_antenna;
};

#define	HAL_TXSTAT_ALTRATE	0x80
#define	HAL_TXERR_XRETRY	0x01
#define	HAL_TXERR_FILT		0x02
#define	HAL_TXERR_FIFO		0x04

struct ath_rx_status {
	u_int16_t	rs_datalen;
	u_int16_t	rs_tstamp;
	u_int8_t	rs_status;
	u_int8_t	rs_phyerr;
	int8_t		rs_rssi;
	u_int8_t	rs_keyix;
	u_int8_t	rs_rate;
	u_int8_t	rs_antenna;
	u_int8_t	rs_more;
};

#define	HAL_RXERR_CRC		0x01
#define	HAL_RXERR_PHY		0x02
#define	HAL_RXERR_FIFO		0x04
#define	HAL_RXERR_DECRYPT	0x08
#define	HAL_RXERR_MIC		0x10
#define	HAL_RXKEYIX_INVALID	((u_int8_t) - 1)
#define	HAL_TXKEYIX_INVALID	((u_int32_t) - 1)

#define	HAL_PHYERR_UNDERRUN		0x00
#define	HAL_PHYERR_TIMING		0x01
#define	HAL_PHYERR_PARITY		0x02
#define	HAL_PHYERR_RATE			0x03
#define	HAL_PHYERR_LENGTH		0x04
#define	HAL_PHYERR_RADAR		0x05
#define	HAL_PHYERR_SERVICE		0x06
#define	HAL_PHYERR_TOR			0x07
#define	HAL_PHYERR_OFDM_TIMING		0x11
#define	HAL_PHYERR_OFDM_SIGNAL_PARITY	0x12
#define	HAL_PHYERR_OFDM_RATE_ILLEGAL	0x13
#define	HAL_PHYERR_OFDM_LENGTH_ILLEGAL	0x14
#define	HAL_PHYERR_OFDM_POWER_DROP	0x15
#define	HAL_PHYERR_OFDM_SERVICE		0x16
#define	HAL_PHYERR_OFDM_RESTART		0x17
#define	HAL_PHYERR_CCK_TIMING		0x19
#define	HAL_PHYERR_CCK_HEADER_CRC	0x1a
#define	HAL_PHYERR_CCK_RATE_ILLEGAL	0x1b
#define	HAL_PHYERR_CCK_SERVICE		0x1e
#define	HAL_PHYERR_CCK_RESTART		0x1f

struct ath_desc {
	u_int32_t	ds_link;
	u_int32_t	ds_data;
	u_int32_t	ds_ctl0;
	u_int32_t	ds_ctl1;
	u_int32_t	ds_hw[4];

	union {
		struct ath_rx_status rx;
		struct ath_tx_status tx;
	} ds_us;

#define	ds_rxstat ds_us.rx
#define	ds_txstat ds_us.tx

} __packed;

#define	HAL_RXDESC_INTREQ	0x0020

#define	HAL_TXDESC_CLRDMASK	0x0001
#define	HAL_TXDESC_NOACK	0x0002
#define	HAL_TXDESC_RTSENA	0x0004
#define	HAL_TXDESC_CTSENA	0x0008
#define	HAL_TXDESC_INTREQ	0x0010
#define	HAL_TXDESC_VEOL		0x0020

/*
 * Hardware abstraction layer structure
 */

#define AR5K_HAL_FUNCTION(_hal, _n, _f)	(_hal)->ah_##_f = ar5k_##_n##_##_f
#define AR5K_HAL_FUNCTIONS(_t, _n, _a) \
	_t const HAL_RATE_TABLE *(_a ##_n##_getRateTable)(struct ath_hal *, \
	    u_int mode); \
	_t void (_a ##_n##_detach)(struct ath_hal *); \
        /* Reset functions */ \
	_t HAL_BOOL (_a ##_n##_reset)(struct ath_hal *, HAL_OPMODE, \
            HAL_CHANNEL *, HAL_BOOL change_channel, HAL_STATUS *status); \
	_t void (_a ##_n##_setPCUConfig)(struct ath_hal *); \
	_t HAL_BOOL (_a ##_n##_perCalibration)(struct ath_hal*, \
            HAL_CHANNEL *); \
	/* Transmit functions */ \
	_t HAL_BOOL (_a ##_n##_updateTxTrigLevel)(struct ath_hal*, \
            HAL_BOOL level); \
	_t int (_a ##_n##_setupTxQueue)(struct ath_hal *, HAL_TX_QUEUE, \
            const HAL_TXQ_INFO *); \
	_t HAL_BOOL (_a ##_n##_setTxQueueProps)(struct ath_hal *, int queue, \
            const HAL_TXQ_INFO *); \
	_t HAL_BOOL (_a ##_n##_releaseTxQueue)(struct ath_hal *, u_int queue); \
	_t HAL_BOOL (_a ##_n##_resetTxQueue)(struct ath_hal *, u_int queue); \
	_t u_int32_t (_a ##_n##_getTxDP)(struct ath_hal *, u_int queue); \
	_t HAL_BOOL (_a ##_n##_setTxDP)(struct ath_hal *, u_int, \
            u_int32_t phys_addr); \
	_t HAL_BOOL (_a ##_n##_startTxDma)(struct ath_hal *, u_int queue); \
	_t HAL_BOOL (_a ##_n##_stopTxDma)(struct ath_hal *, u_int queue); \
	_t HAL_BOOL (_a ##_n##_setupTxDesc)(struct ath_hal *, \
            struct ath_desc *, \
            u_int packet_length, u_int header_length, HAL_PKT_TYPE type, \
            u_int txPower, u_int tx_rate0, u_int tx_tries0, u_int key_index, \
            u_int antenna_mode, u_int flags, u_int rtscts_rate, \
            u_int rtscts_duration); \
	_t HAL_BOOL (_a ##_n##_setupXTxDesc)(struct ath_hal *, \
            struct ath_desc *, \
            u_int tx_rate1, u_int tx_tries1, u_int tx_rate2, u_int tx_tries2, \
            u_int tx_rate3, u_int tx_tries3); \
	_t HAL_BOOL (_a ##_n##_fillTxDesc)(struct ath_hal *, \
            struct ath_desc *, \
            u_int segLen, HAL_BOOL firstSeg, HAL_BOOL lastSeg); \
	_t HAL_STATUS (_a ##_n##_procTxDesc)(struct ath_hal *, \
            struct ath_desc *); \
	_t HAL_BOOL (_a ##_n##_hasVEOL)(struct ath_hal *); \
	/* Receive Functions */ \
	_t u_int32_t (_a ##_n##_getRxDP)(struct ath_hal*); \
	_t void (_a ##_n##_setRxDP)(struct ath_hal*, u_int32_t rxdp); \
	_t void (_a ##_n##_enableReceive)(struct ath_hal*); \
	_t HAL_BOOL (_a ##_n##_stopDmaReceive)(struct ath_hal*); \
	_t void (_a ##_n##_startPcuReceive)(struct ath_hal*); \
	_t void (_a ##_n##_stopPcuReceive)(struct ath_hal*); \
	_t void (_a ##_n##_setMulticastFilter)(struct ath_hal*, \
            u_int32_t filter0, u_int32_t filter1); \
	_t HAL_BOOL (_a ##_n##_setMulticastFilterIndex)(struct ath_hal*, \
            u_int32_t index); \
	_t HAL_BOOL (_a ##_n##_clrMulticastFilterIndex)(struct ath_hal*, \
            u_int32_t index); \
	_t u_int32_t (_a ##_n##_getRxFilter)(struct ath_hal*); \
	_t void (_a ##_n##_setRxFilter)(struct ath_hal*, u_int32_t); \
	_t HAL_BOOL (_a ##_n##_setupRxDesc)(struct ath_hal *, \
            struct ath_desc *, u_int32_t size, u_int flags); \
	_t HAL_STATUS (_a ##_n##_procRxDesc)(struct ath_hal *, \
            struct ath_desc *, u_int32_t phyAddr, struct ath_desc *next); \
	_t void (_a ##_n##_rxMonitor)(struct ath_hal *); \
	/* Misc Functions */ \
	_t void (_a ##_n##_dumpState)(struct ath_hal *); \
	_t HAL_BOOL (_a ##_n##_getDiagState)(struct ath_hal *, int, void **, \
            u_int *); \
	_t void (_a ##_n##_getMacAddress)(struct ath_hal *, u_int8_t *); \
	_t HAL_BOOL (_a ##_n##_setMacAddress)(struct ath_hal *, const u_int8_t*); \
	_t HAL_BOOL (_a ##_n##_setRegulatoryDomain)(struct ath_hal*, \
            u_int16_t, HAL_STATUS *); \
	_t void (_a ##_n##_setLedState)(struct ath_hal*, HAL_LED_STATE); \
	_t void (_a ##_n##_writeAssocid)(struct ath_hal*, \
            const u_int8_t *bssid, u_int16_t assocId, u_int16_t timOffset); \
	_t HAL_BOOL (_a ##_n##_gpioCfgOutput)(struct ath_hal *, \
            u_int32_t gpio); \
	_t HAL_BOOL (_a ##_n##_gpioCfgInput)(struct ath_hal *, \
            u_int32_t gpio); \
	_t u_int32_t (_a ##_n##_gpioGet)(struct ath_hal *, u_int32_t gpio); \
	_t HAL_BOOL (_a ##_n##_gpioSet)(struct ath_hal *, u_int32_t gpio, \
            u_int32_t val); \
	_t void (_a ##_n##_gpioSetIntr)(struct ath_hal*, u_int, u_int32_t); \
	_t u_int32_t (_a ##_n##_getTsf32)(struct ath_hal*); \
	_t u_int64_t (_a ##_n##_getTsf64)(struct ath_hal*); \
	_t void (_a ##_n##_resetTsf)(struct ath_hal*); \
	_t u_int16_t (_a ##_n##_getRegDomain)(struct ath_hal*); \
	_t HAL_BOOL (_a ##_n##_detectCardPresent)(struct ath_hal*); \
	_t void (_a ##_n##_updateMibCounters)(struct ath_hal*, \
           HAL_MIB_STATS*); \
	_t HAL_BOOL (_a ##_n##_isHwCipherSupported)(struct ath_hal*, \
           HAL_CIPHER); \
	_t HAL_RFGAIN (_a ##_n##_getRfGain)(struct ath_hal*); \
	/*								\
            u_int32_t (_a ##_n##_getCurRssi)(struct ath_hal*);		\
            u_int32_t (_a ##_n##_getDefAntenna)(struct ath_hal*);	\
            void (_a ##_n##_setDefAntenna)(struct ath_hal*, u_int32_t ant); \
        */								\
	_t HAL_BOOL (_a ##_n##_setSlotTime)(struct ath_hal*, u_int);	\
	_t u_int (_a ##_n##_getSlotTime)(struct ath_hal*);		\
	_t HAL_BOOL (_a ##_n##_setAckTimeout)(struct ath_hal *, u_int); \
	_t u_int (_a ##_n##_getAckTimeout)(struct ath_hal*);		\
	_t HAL_BOOL (_a ##_n##_setCTSTimeout)(struct ath_hal*, u_int);	\
	_t u_int (_a ##_n##_getCTSTimeout)(struct ath_hal*);		\
	/* Key Cache Functions */ \
	_t u_int32_t (_a ##_n##_getKeyCacheSize)(struct ath_hal*); \
	_t HAL_BOOL (_a ##_n##_resetKeyCacheEntry)(struct ath_hal*, \
           u_int16_t); \
	_t HAL_BOOL (_a ##_n##_isKeyCacheEntryValid)(struct ath_hal *, \
           u_int16_t); \
	_t HAL_BOOL (_a ##_n##_setKeyCacheEntry)(struct ath_hal*, u_int16_t, \
            const HAL_KEYVAL *, const u_int8_t *, int);	\
	_t HAL_BOOL (_a ##_n##_setKeyCacheEntryMac)(struct ath_hal*, \
            u_int16_t, const u_int8_t *); \
	/* Power Management Functions */ \
	_t HAL_BOOL (_a ##_n##_setPowerMode)(struct ath_hal*, \
            HAL_POWER_MODE mode, \
            int setChip, u_int16_t sleepDuration); \
	_t HAL_POWER_MODE (_a ##_n##_getPowerMode)(struct ath_hal*); \
	_t HAL_BOOL (_a ##_n##_queryPSPollSupport)(struct ath_hal*); \
	_t HAL_BOOL (_a ##_n##_initPSPoll)(struct ath_hal*); \
	_t HAL_BOOL (_a ##_n##_enablePSPoll)(struct ath_hal *, u_int8_t *, \
            u_int16_t); \
	_t HAL_BOOL (_a ##_n##_disablePSPoll)(struct ath_hal *); \
	/* Beacon Management Functions */ \
	_t void (_a ##_n##_beaconInit)(struct ath_hal *, u_int32_t nexttbtt, \
            u_int32_t intval); \
	_t void (_a ##_n##_setStationBeaconTimers)(struct ath_hal *, \
            const HAL_BEACON_STATE *, u_int32_t tsf, u_int32_t dtimCount, \
            u_int32_t cfpCcount); \
	_t void (_a ##_n##_resetStationBeaconTimers)(struct ath_hal *); \
	_t HAL_BOOL (_a ##_n##_waitForBeaconDone)(struct ath_hal *, \
            bus_addr_t); \
	/* Interrupt functions */ \
	_t HAL_BOOL (_a ##_n##_isInterruptPending)(struct ath_hal *); \
	_t HAL_BOOL (_a ##_n##_getPendingInterrupts)(struct ath_hal *, \
            u_int32_t *); \
	_t u_int32_t (_a ##_n##_getInterrupts)(struct ath_hal *); \
	_t HAL_INT (_a ##_n##_setInterrupts)(struct ath_hal *, HAL_INT); \
	/* Chipset functions (ar5k-specific, non-HAL) */ \
	_t HAL_BOOL (_a ##_n##_get_capabilities)(struct ath_hal *); \
	_t void (_a ##_n##_radar_alert)(struct ath_hal *, HAL_BOOL enable); \
	_t HAL_BOOL (_a ##_n##_regulation_domain)(struct ath_hal *, \
            HAL_BOOL read, ieee80211_regdomain_t *); \
	_t int (_a ##_n##_eeprom_init)(struct ath_hal *); \
	_t HAL_BOOL (_a ##_n##_eeprom_is_busy)(struct ath_hal *); \
	_t int (_a ##_n##_eeprom_read)(struct ath_hal *, u_int32_t offset, \
            u_int16_t *data); \
	_t int (_a ##_n##_eeprom_write)(struct ath_hal *, u_int32_t offset, \
            u_int16_t data);

#define AR5K_MAX_GPIO	10

struct ath_hal {
	u_int32_t		ah_magic;
	u_int32_t		ah_abi;
	u_int16_t		ah_device;
	u_int16_t		ah_sub_vendor;

	void			*ah_sc;
	bus_space_tag_t		ah_st;
	bus_space_handle_t	ah_sh;

	HAL_INT			ah_imr;

	HAL_CTRY_CODE		ah_country_code;
	HAL_OPMODE		ah_op_mode;
	HAL_POWER_MODE		ah_power_mode;
	HAL_CHANNEL		ah_current_channel;
	HAL_BOOL		ah_turbo;

#define ah_countryCode		ah_country_code

	HAL_RATE_TABLE		ah_rt_11a;
	HAL_RATE_TABLE		ah_rt_11b;
	HAL_RATE_TABLE		ah_rt_11g;
	HAL_RATE_TABLE		ah_rt_turbo;

	u_int32_t		ah_mac_version;
	u_int16_t		ah_mac_revision;
	u_int16_t		ah_phy_revision;
	u_int16_t		ah_radio_5ghz_revision;
	u_int16_t		ah_radio_2ghz_revision;

#define ah_macVersion		ah_mac_version
#define ah_macRev		ah_mac_revision
#define ah_phyRev		ah_phy_revision
#define ah_analog5GhzRev	ah_radio_5ghz_revision
#define ah_analog2GhzRev	ah_radio_2ghz_revision
#define ah_regdomain		ah_capabilities.cap_eeprom.ee_regdomain

	u_int32_t		ah_atim_window;
	u_int32_t		ah_aifs;
	u_int32_t		ah_cw_min;
	HAL_BOOL		ah_software_retry;
	u_int32_t		ah_limit_tx_retries;

	u_int8_t		ah_sta_id[IEEE80211_ADDR_LEN];
	u_int8_t		ah_bssid[IEEE80211_ADDR_LEN];

	u_int32_t		ah_gpio[AR5K_MAX_GPIO];

	ar5k_capabilities_t	ah_capabilities;

	HAL_TXQ_INFO		ah_txq[HAL_NUM_TX_QUEUES];

	struct {
		HAL_BOOL	r_enabled;
		int		r_last_alert;
		HAL_CHANNEL	r_last_channel;
	} ah_radar;

	/*
	 * Function pointers
	 */
	AR5K_HAL_FUNCTIONS(, ah, *);
};

/*
 * Misc defines
 */

#define HAL_ABI_VERSION		0x04090901 /* YYMMDDnn */

#define AR5K_PRINTF(_x, ...) printf(__func__ ": " _x)
#define AR5K_TRACE printf("%s:%d\n", __func__, __LINE__)
#define AR5K_DELAY(_n) delay(_n)

#define AR5K_ELEMENTS(_array) (sizeof(_array) / sizeof(_array[0]))

typedef struct ath_hal*(ar5k_attach_t)
	(u_int16_t, void *, bus_space_tag_t, bus_space_handle_t, HAL_STATUS *);

/*
 * Some tuneable values (these should be changeable by the user)
 */

#define AR5K_TUNE_DMA_BEACON_RESP		2
#define AR5K_TUNE_SW_BEACON_RESP		10
#define AR5K_TUNE_ADDITIONAL_SWBA_BACKOFF	0
#define AR5K_TUNE_RADAR_ALERT			AH_FALSE
#define AR5K_TUNE_MIN_TX_FIFO_THRES		1
#define AR5K_TUNE_MAX_TX_FIFO_THRES		((IEEE80211_MAX_LEN / 64) + 1)
#define AR5K_TUNE_RSSI_THRES			1792
#define AR5K_TUNE_REGISTER_TIMEOUT		20000
#define AR5K_TUNE_REGISTER_DWELL_TIME		20000
#define AR5K_TUNE_BEACON_INTERVAL		100
#define AR5K_TUNE_AIFS				2
#define AR5K_TUNE_CWMIN				15

/*
 * Common initial register values
 */

#define AR5K_INIT_TX_LATENCY			502
#define AR5K_INIT_USEC				39
#define AR5K_INIT_USEC_TURBO			79
#define AR5K_INIT_USEC_32			31
#define AR5K_INIT_CARR_SENSE_EN			1
#define AR5K_INIT_PROG_IFS			920
#define AR5K_INIT_PROG_IFS_TURBO		960
#define AR5K_INIT_EIFS				3440
#define AR5K_INIT_EIFS_TURBO			6880
#define AR5K_INIT_SLOT_TIME			360
#define AR5K_INIT_SLOT_TIME_TURBO		480
#define AR5K_INIT_ACK_CTS_TIMEOUT		1024
#define AR5K_INIT_ACK_CTS_TIMEOUT_TURBO		0x08000800
#define AR5K_INIT_SIFS				560
#define AR5K_INIT_SIFS_TURBO			480
#define AR5K_INIT_SH_RETRY			10
#define AR5K_INIT_LG_RETRY			AR5K_INIT_SH_RETRY
#define AR5K_INIT_SSH_RETRY			32
#define AR5K_INIT_SLG_RETRY			AR5K_INIT_SSH_RETRY
#define AR5K_INIT_TX_RETRY			10
#define AR5K_INIT_TOPS				8
#define AR5K_INIT_RXNOFRM			8
#define AR5K_INIT_RPGTO				0
#define AR5K_INIT_TXNOFRM			0
#define AR5K_INIT_BEACON_PERIOD			65535
#define AR5K_INIT_TIM_OFFSET			0
#define AR5K_INIT_BEACON_EN			0
#define AR5K_INIT_RESET_TSF			0
#define AR5K_INIT_TRANSMIT_LATENCY		(			\
        (AR5K_INIT_TX_LATENCY << 14) | (AR5K_INIT_USEC_32 << 7) |	\
        (AR5K_INIT_USEC)						\
)
#define AR5K_INIT_TRANSMIT_LATENCY_TURBO	(			\
        (AR5K_INIT_TX_LATENCY << 14) | (AR5K_INIT_USEC_32 << 7) |	\
        (AR5K_INIT_USEC_TURBO)						\
)
#define AR5K_INIT_PROTO_TIME_CNTRL		(			\
        (AR5K_INIT_CARR_SENSE_EN << 26) | (AR5K_INIT_EIFS << 12) |	\
        (AR5K_INIT_PROG_IFS)						\
)
#define AR5K_INIT_PROTO_TIME_CNTRL_TURBO	(			\
        (AR5K_INIT_CARR_SENSE_EN << 26) | (AR5K_INIT_EIFS_TURBO << 12) |\
        (AR5K_INIT_PROG_IFS_TURBO)					\
)
#define AR5K_INIT_BEACON_CONTROL		(			\
        (AR5K_INIT_RESET_TSF << 24) | (AR5K_INIT_BEACON_EN << 23) |	\
        (AR5K_INIT_TIM_OFFSET << 16) | (AR5K_INIT_BEACON_PERIOD)	\
)

/*
 * AR5k register access
 */

#define AR5K_REG_WRITE(_reg, _val)					\
        bus_space_write_4(hal->ah_st, hal->ah_sh, (_reg), (_val))
#define AR5K_REG_READ(_reg)						\
        ((u_int32_t)bus_space_read_4(hal->ah_st, hal->ah_sh, (_reg)))

#define	AR5K_REG_SM(_val, _flags)					\
        (((_val) << _flags##_S) & (_flags))
#define	AR5K_REG_MS(_val, _flags)					\
        (((_val) & (_flags)) >> _flags##_S)
#define	AR5K_REG_WRITE_BITS(_reg, _flags, _val)			\
        AR5K_REG_WRITE(_reg, (AR5K_REG_READ(_reg) &~ (_flags)) |	\
            (((_val) << _flags##_S) & (_flags)))
#define	AR5K_REG_ENABLE_BITS(_reg, _flags)				\
        AR5K_REG_WRITE(_reg, AR5K_REG_READ(_reg) | (_flags))
#define	AR5K_REG_DISABLE_BITS(_reg, _flags)				\
        AR5K_REG_WRITE(_reg, AR5K_REG_READ(_reg) &~ (_flags))

/*
 * Initial register values
 */

struct ar5k_ini {
	u_int32_t	ini_register;
	u_int32_t	ini_value;

	enum {
		INI_WRITE = 0,
		INI_READ = 1,
	} ini_mode;
};

/*
 * Unaligned little endian access
 */

#define	AR5K_LE_READ_2(_p)						\
        (((u_int8_t *)(_p))[0] | (((u_int8_t *)(_p))[1] << 8))
#define	AR5K_LE_READ_4(_p) \
        (((u_int8_t *)(_p))[0] | (((u_int8_t *)(_p))[1] << 8) | 	\
        (((u_int8_t *)(_p))[2] << 16) | (((u_int8_t *)(_p))[3] << 24))
#define	AR5K_LE_WRITE_2(_p, _val) \
        ((((u_int8_t *)(_p))[0] = ((u_int32_t)(_val) & 0xff)), 		\
        (((u_int8_t *)(_p))[1] = (((u_int32_t)(_val) >> 8) & 0xff)))
#define	AR5K_LE_WRITE_4(_p, _val)					\
        ((((u_int8_t *)(_p))[0] = ((u_int32_t)(_val) & 0xff)),		\
        (((u_int8_t *)(_p))[1] = (((u_int32_t)(_val) >> 8) & 0xff)),	\
        (((u_int8_t *)(_p))[2] = (((u_int32_t)(_val) >> 16) & 0xff)),	\
        (((u_int8_t *)(_p))[3] = (((u_int32_t)(_val) >> 24) & 0xff)))

/*
 * Prototypes
 */

__BEGIN_DECLS

const char		*ath_hal_probe(u_int16_t, u_int16_t);

struct ath_hal		*ath_hal_attach(u_int16_t, void *, bus_space_tag_t,
    bus_space_handle_t, HAL_STATUS *);

u_int16_t		 ath_hal_computetxtime(struct ath_hal *,
    const HAL_RATE_TABLE *, u_int32_t, u_int16_t, HAL_BOOL);

u_int			 ath_hal_mhz2ieee(u_int, u_int);
u_int			 ath_hal_ieee2mhz(u_int, u_int);

HAL_BOOL		 ath_hal_init_channels(struct ath_hal *, HAL_CHANNEL *,
    u_int, u_int *, HAL_CTRY_CODE, u_int16_t, HAL_BOOL, HAL_BOOL);

void			 ar5k_radar_alert(struct ath_hal *);
int			 ar5k_eeprom_read_mac(struct ath_hal *, u_int8_t *);
ieee80211_regdomain_t	*ar5k_regdomain_to_ieee(u_int8_t);
u_int8_t		 ar5k_regdomain_from_ieee(ieee80211_regdomain_t *);
u_int32_t		 ar5k_bitswap(u_int32_t, u_int);
u_int			 ar5k_clocktoh(u_int, HAL_BOOL);
u_int			 ar5k_htoclock(u_int, HAL_BOOL);
void			 ar5k_rt_copy(HAL_RATE_TABLE *, HAL_RATE_TABLE *);

HAL_BOOL		 ar5k_register_timeout(struct ath_hal *, u_int32_t,
    u_int32_t, u_int32_t, HAL_BOOL);

__END_DECLS

#endif /* _AR5K_H */
