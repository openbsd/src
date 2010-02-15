/*	$OpenBSD: athnvar.h,v 1.8 2010/02/15 17:16:36 damien Exp $	*/

/*-
 * Copyright (c) 2009 Damien Bergamini <damien.bergamini@free.fr>
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

#define ATHN_DEBUG		1
#define ATHN_BT_COEXISTENCE	1

#ifdef ATHN_DEBUG
#define DPRINTF(x)	do { if (athn_debug > 0) printf x; } while (0)
#define DPRINTFN(n, x)	do { if (athn_debug >= (n)) printf x; } while (0)
extern int athn_debug;
#else
#define DPRINTF(x)
#define DPRINTFN(n, x)
#endif

#define ATHN_RXBUFSZ	3872
#define ATHN_TXBUFSZ	4096

#define ATHN_NRXBUFS		64
#define ATHN_NTXBUFS		64	/* Shared between all Tx queues. */
#define ATHN_MAX_SCATTER	16

struct athn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsft;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	uint8_t		wr_antenna;
} __packed;

#define ATHN_RX_RADIOTAP_PRESENT						\
	(1 << IEEE80211_RADIOTAP_TSFT |					\
	 1 << IEEE80211_RADIOTAP_FLAGS |				\
	 1 << IEEE80211_RADIOTAP_RATE |					\
	 1 << IEEE80211_RADIOTAP_CHANNEL |				\
	 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL |			\
	 1 << IEEE80211_RADIOTAP_ANTENNA)

struct athn_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
	uint8_t		wt_hwqueue;
} __packed;

#define ATHN_TX_RADIOTAP_PRESENT						\
	(1 << IEEE80211_RADIOTAP_FLAGS |				\
	 1 << IEEE80211_RADIOTAP_RATE |					\
	 1 << IEEE80211_RADIOTAP_CHANNEL |				\
	 1 << IEEE80211_RADIOTAP_HWQUEUE)

struct athn_tx_buf {
	SIMPLEQ_ENTRY(athn_tx_buf)	bf_list;

	struct ar_tx_desc		*bf_descs;
	bus_dmamap_t			bf_map;
	bus_addr_t			bf_daddr;

	struct mbuf			*bf_m;
	struct ieee80211_node		*bf_ni;
};

struct athn_txq {
	SIMPLEQ_HEAD(, athn_tx_buf)	head;
	struct ar_tx_desc		*lastds;
};

struct athn_rx_buf {
	SIMPLEQ_ENTRY(athn_rx_buf)	bf_list;

	struct ar_rx_desc		*bf_desc;
	bus_dmamap_t			bf_map;

	struct mbuf			*bf_m;
	bus_addr_t			bf_daddr;
};

struct athn_rxq {
	struct athn_rx_buf		bf[ATHN_NRXBUFS];

	struct ar_rx_desc		*descs;
	struct ar_rx_desc		*lastds;
	bus_dmamap_t			map;
	bus_dma_segment_t		seg;

	SIMPLEQ_HEAD(, athn_rx_buf)	head;
};

/* Software rate indexes. */
#define ATHN_RIDX_CCK1	0
#define ATHN_RIDX_CCK2	1
#define ATHN_RIDX_OFDM6	4
#define ATHN_RIDX_MCS0	12
#define ATHN_RIDX_MCS15	27
#define ATHN_RIDX_MAX	27
#define ATHN_IS_HT_RIDX(ridx)	((ridx) >= ATHN_RIDX_MCS0)

static const struct athn_rate {
	uint8_t	rate;		/* Rate in 500Kbps unit or MCS if 0x80. */
	uint8_t	hwrate;		/* HW representation. */
	uint8_t	rspridx;	/* Control Response Frame rate index. */
	enum	ieee80211_phytype phy;
} athn_rates[] = {
	{    2, 0x1b, 0, IEEE80211_T_DS },
	{    4, 0x1a, 1, IEEE80211_T_DS },
	{   11, 0x19, 1, IEEE80211_T_DS },
	{   22, 0x18, 1, IEEE80211_T_DS },
	{   12, 0x0b, 4, IEEE80211_T_OFDM },
	{   18, 0x0f, 4, IEEE80211_T_OFDM },
	{   24, 0x0a, 6, IEEE80211_T_OFDM },
	{   36, 0x0e, 6, IEEE80211_T_OFDM },
	{   48, 0x09, 8, IEEE80211_T_OFDM },
	{   72, 0x0d, 8, IEEE80211_T_OFDM },
	{   96, 0x08, 8, IEEE80211_T_OFDM },
	{  108, 0x0c, 8, IEEE80211_T_OFDM },
	{ 0x80, 0x80, 8, IEEE80211_T_OFDM },
	{ 0x81, 0x81, 8, IEEE80211_T_OFDM },
	{ 0x82, 0x82, 8, IEEE80211_T_OFDM },
	{ 0x83, 0x83, 8, IEEE80211_T_OFDM },
	{ 0x84, 0x84, 8, IEEE80211_T_OFDM },
	{ 0x85, 0x85, 8, IEEE80211_T_OFDM },
	{ 0x86, 0x86, 8, IEEE80211_T_OFDM },
	{ 0x87, 0x87, 8, IEEE80211_T_OFDM },
	{ 0x88, 0x88, 8, IEEE80211_T_OFDM },
	{ 0x89, 0x89, 8, IEEE80211_T_OFDM },
	{ 0x8a, 0x8a, 8, IEEE80211_T_OFDM },
	{ 0x8b, 0x8b, 8, IEEE80211_T_OFDM },
	{ 0x8c, 0x8c, 8, IEEE80211_T_OFDM },
	{ 0x8d, 0x8d, 8, IEEE80211_T_OFDM },
	{ 0x8e, 0x8e, 8, IEEE80211_T_OFDM },
	{ 0x8f, 0x8f, 8, IEEE80211_T_OFDM }
};

struct athn_series {
	uint16_t	dur;
	uint8_t		hwrate;
};

struct athn_pier {
	uint8_t		fbin;
	const uint8_t	*pwr[AR_PD_GAINS_IN_MASK];
	const uint8_t	*vpd[AR_PD_GAINS_IN_MASK];
};

/*
 * Structures used to store initialization values.
 */
struct athn_ini {
	int		nregs;
	const uint16_t	*regs;
	const uint32_t	*vals_5g20;
#ifndef IEEE80211_NO_HT
	const uint32_t	*vals_5g40;
	const uint32_t	*vals_2g40;
#endif
	const uint32_t	*vals_2g20;
	int		ncmregs;
	const uint16_t	*cmregs;
	const uint32_t	*cmvals;
};

struct athn_gain {
	int		nregs;
	const uint16_t	*regs;
	const uint32_t	*vals_5g;
	const uint32_t	*vals_2g;
};

struct athn_addac {
	int		nvals;
	const uint32_t	*vals;
};

/* Tx queue software indexes. */
#define ATHN_QID_AC_BE		0
#define ATHN_QID_PSPOLL		1
#define ATHN_QID_AC_BK		2
#define ATHN_QID_AC_VI		3
#define ATHN_QID_AC_VO		4
#define ATHN_QID_UAPSD		5
#define ATHN_QID_CAB		6
#define ATHN_QID_BEACON		7
#define ATHN_QID_COUNT		8

/* Map Access Category to Tx queue Id. */
static const uint8_t athn_ac2qid[EDCA_NUM_AC] = {
	ATHN_QID_AC_BE,	/* EDCA_AC_BE */
	ATHN_QID_AC_BK,	/* EDCA_AC_BK */
	ATHN_QID_AC_VI,	/* EDCA_AC_VI */
	ATHN_QID_AC_VO	/* EDCA_AC_VO */
};

static const uint8_t athn_5ghz_chans[] = {
	/* UNII 1. */
	36, 40, 44, 48,
	/* UNII 2. */
	52, 56, 60, 64,
	/* Middle band. */
	100, 104, 108, 112, 116, 120, 124, 128, 132, 136, 140,
	/* UNII 3. */
	149, 153, 157, 161, 165
};

/* Number of data bits per OFDM symbol for MCS[0-15]. */
/* See tables 20-29, 20-30, 20-33, 20-34. */
static const uint16_t ar_mcs_ndbps[][2] = {
	/* 20MHz  40MHz */
	{     26,    54 },	/* MCS0 */
	{     52,   108 },	/* MCS1 */
	{     78,   162 },	/* MCS2 */
	{    104,   216 },	/* MCS3 */
	{    156,   324 },	/* MCS4 */
	{    208,   432 },	/* MCS5 */
	{    234,   486 },	/* MCS6 */
	{    260,   540 },	/* MCS7 */
	{     26,   108 },	/* MCS8 */
	{     52,   216 },	/* MCS9 */
	{     78,   324 },	/* MCS10 */
	{    104,   432 },	/* MCS11 */
	{    156,   648 },	/* MCS12 */
	{    208,   864 },	/* MCS13 */
	{    234,   972 },	/* MCS14 */
	{    260,  1080 }	/* MCS15 */
};

#define ATHN_POWER_OFDM6	0
#define ATHN_POWER_OFDM9	1
#define ATHN_POWER_OFDM12	2
#define ATHN_POWER_OFDM18	3
#define ATHN_POWER_OFDM24	4
#define ATHN_POWER_OFDM36	5
#define ATHN_POWER_OFDM48	6
#define ATHN_POWER_OFDM54	7
#define ATHN_POWER_CCK1_LP	8
#define ATHN_POWER_CCK2_LP	9
#define ATHN_POWER_CCK2_SP	10
#define ATHN_POWER_CCK55_LP	11
#define ATHN_POWER_CCK55_SP	12
#define ATHN_POWER_CCK11_LP	13
#define ATHN_POWER_CCK11_SP	14
#define ATHN_POWER_XR		15
#define ATHN_POWER_HT20(mcs)	(16 + (mcs))
#define ATHN_POWER_HT40(mcs)	(24 + (mcs))
#define ATHN_POWER_CCK_DUP	32
#define ATHN_POWER_OFDM_DUP	33
#define ATHN_POWER_CCK_EXT	34
#define ATHN_POWER_OFDM_EXT	35
#define ATHN_POWER_COUNT	36

struct athn_node {
	struct ieee80211_node		ni;
	struct ieee80211_amrr_node	amn;
	uint8_t				ridx[IEEE80211_RATE_MAXSIZE];
	uint8_t				fallback[IEEE80211_RATE_MAXSIZE];
};

#define ATHN_ANI_PERIOD		100
#define ATHN_ANI_RSSI_THR_HIGH	40
#define ATHN_ANI_RSSI_THR_LOW	7
struct athn_ani {
	uint8_t		noise_immunity_level;
	uint8_t		spur_immunity_level;
	uint8_t		firstep_level;
	uint8_t		ofdm_weak_signal;
	uint8_t		cck_weak_signal;

	uint32_t	listen_time;

	uint32_t	ofdm_trig_high;
	uint32_t	ofdm_trig_low;

	int32_t		cck_trig_high;
	int32_t		cck_trig_low;

	uint32_t	ofdm_phy_err_base;
	uint32_t	cck_phy_err_base;
	uint32_t	ofdm_phy_err_count;
	uint32_t	cck_phy_err_count;
	
	uint32_t	cyccnt;
	uint32_t	txfcnt;
	uint32_t	rxfcnt;
};

struct athn_iq_cal {
	uint32_t	pwr_meas_i;
	uint32_t	pwr_meas_q;
	int32_t		iq_corr_meas;
};

struct athn_adc_cal {
	uint32_t	pwr_meas_odd_i;
	uint32_t	pwr_meas_even_i;
	uint32_t	pwr_meas_odd_q;
	uint32_t	pwr_meas_even_q;
};

struct athn_calib {
	int			nsamples;
	struct athn_iq_cal	iq[AR_MAX_CHAINS];
	struct athn_adc_cal	adc_gain[AR_MAX_CHAINS];
	struct athn_adc_cal	adc_dc_offset[AR_MAX_CHAINS];
};

#define ATHN_NF_CAL_HIST_MAX	5

struct athn_softc;

struct athn_ops {
	void	(*setup)(struct athn_softc *);
	void	(*set_txpower)(struct athn_softc *, struct ieee80211_channel *,
		    struct ieee80211_channel *);
	void	(*spur_mitigate)(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
	const struct ar_spur_chan *(*get_spur_chans)(struct athn_softc *, int);
	void	(*init_from_rom)(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
	int	(*set_synth)(struct athn_softc *, struct ieee80211_channel *,
		    struct ieee80211_channel *);
	void	(*swap_rom)(struct athn_softc *);
	void	(*olpc_init)(struct athn_softc *);
};

struct athn_softc {
	struct device			sc_dev;
	struct ieee80211com		sc_ic;

	int				(*sc_enable)(struct athn_softc *);
	void				(*sc_disable)(struct athn_softc *);
	void				(*sc_power)(struct athn_softc *, int);
	void				(*sc_disable_aspm)(struct athn_softc *);

	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);

	bus_dma_tag_t			sc_dmat;
	bus_space_tag_t			sc_st;
	bus_space_handle_t		sc_sh;

	struct timeout			scan_to;
	struct timeout			calib_to;
	struct ieee80211_amrr		amrr;

	u_int				flags;
#define ATHN_FLAG_PCIE			(1 << 0)
#define ATHN_FLAG_OLPC			(1 << 1)
#define ATHN_FLAG_SPLIT_MMIC		(1 << 2)
#define ATHN_FLAG_RFSILENT		(1 << 3)
#define ATHN_FLAG_RFSILENT_REVERSED	(1 << 4)
#define ATHN_FLAG_BTCOEX2WIRE		(1 << 5)
#define ATHN_FLAG_BTCOEX3WIRE		(1 << 6)
/* Shortcut. */
#define ATHN_FLAG_BTCOEX	(ATHN_FLAG_BTCOEX2WIRE | ATHN_FLAG_BTCOEX3WIRE)

	uint8_t				ngpiopins;
	int				led_pin;
	int				rfsilent_pin;
	uint32_t			isync;
	uint32_t			imask;

	uint16_t			mac_ver;
	uint8_t				mac_rev;
	uint8_t				rf_rev;
	uint16_t			eep_rev;
	uint32_t			phy_rev;

	uint8_t				txchainmask;
	uint8_t				rxchainmask;
	uint8_t				ntxchains;
	uint8_t				nrxchains;

	uint8_t				calib_mask;
#define ATHN_CAL_IQ		(1 << 0)
#define ATHN_CAL_ADC_GAIN	(1 << 1)
#define ATHN_CAL_ADC_DC		(1 << 2)

	struct ieee80211_channel	*curchan;
	struct ieee80211_channel	*curchanext;

	/* Open Loop Power Control. */
	int8_t				tx_gain_tbl[AR9280_TX_GAIN_TABLE_SIZE];
	int8_t				pdadc;
	int8_t				tcomp;

	uint32_t			rwbuf[64];

	int				kc_entries;

	void				*eep;
	uint32_t			eep_base;
	uint32_t			eep_size;

	struct athn_rxq			rxq;
	struct athn_txq			txq[31];	/* 0x1f ??? */

	struct ar_tx_desc		*descs;
	bus_dmamap_t			map;
	bus_dma_segment_t		seg;
	SIMPLEQ_HEAD(, athn_tx_buf)	txbufs;
	struct athn_tx_buf		txpool[ATHN_NTXBUFS];

	int				sc_if_flags;
	int				sc_tx_timer;

	const struct athn_ini		*ini;
	const struct athn_gain		*rx_gain;
	const struct athn_gain		*tx_gain;
	const struct athn_addac		*addac;
	const uint32_t			*serdes;
	uint32_t			workaround;

	struct athn_ops			ops;

	int				fixed_ridx;

	int16_t				def_nf;
	struct {
		int16_t	nf[AR_MAX_CHAINS];
		int16_t	nf_ext[AR_MAX_CHAINS];
	}				nf_hist[ATHN_NF_CAL_HIST_MAX];
	int				nf_hist_cur;
	int16_t				nf_priv[AR_MAX_CHAINS];
	int16_t				nf_ext_priv[AR_MAX_CHAINS];

	struct athn_calib		calib;
	struct athn_ani			ani;

#if NBPFILTER > 0
	caddr_t				sc_drvbpf;

	union {
		struct athn_rx_radiotap_header th;
		uint8_t pad[IEEE80211_RADIOTAP_HDRLEN];
	} sc_rxtapu;
#define sc_rxtap			sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct athn_tx_radiotap_header th;
		uint8_t pad[IEEE80211_RADIOTAP_HDRLEN];
	} sc_txtapu;
#define sc_txtap			sc_txtapu.th
	int				sc_txtap_len;
#endif
};

extern int	athn_attach(struct athn_softc *);
extern void	athn_detach(struct athn_softc *);
extern int	athn_intr(void *);
extern int	ar5416_attach(struct athn_softc *);
extern int	ar9280_attach(struct athn_softc *);
extern int	ar9285_attach(struct athn_softc *);
extern int	ar9287_attach(struct athn_softc *);
extern uint8_t	athn_reverse_bits(uint8_t, int);
extern uint8_t	athn_chan2fbin(struct ieee80211_channel *);
extern void	athn_set_viterbi_mask(struct athn_softc *, int);
extern void	athn_write_txpower(struct athn_softc *, int16_t[]);
extern void	athn_get_lg_tpow(struct athn_softc *,
		    struct ieee80211_channel *, uint8_t,
		    const struct ar_cal_target_power_leg *, int, uint8_t[]);
extern void	athn_get_ht_tpow(struct athn_softc *,
		    struct ieee80211_channel *, uint8_t,
		    const struct ar_cal_target_power_ht *, int, uint8_t[]);
extern void	athn_get_pdadcs(struct athn_softc *, uint8_t,
		    struct athn_pier *, struct athn_pier *, int, int, uint8_t,
		    uint8_t *, uint8_t *);
extern void	athn_get_pier_ival(uint8_t, const uint8_t *, int, int *,
		    int *);
/* XXX not here. */
extern void	ar5416_set_txpower(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
extern void	ar5416_swap_rom(struct athn_softc *);
extern void	ar9280_2_0_olpc_get_pdadcs(struct athn_softc *,
		    struct ieee80211_channel *, int, uint8_t[], uint8_t[],
		    uint8_t *);
extern int	ar9280_set_synth(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
extern void	ar9280_spur_mitigate(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
extern void	ar9287_1_2_enable_async_fifo(struct athn_softc *);
extern void	ar9287_1_2_setup_async_fifo(struct athn_softc *);
extern const	struct ar_spur_chan *ar5416_get_spur_chans(struct athn_softc *,
		    int);
extern int	ar5416_init_calib(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
extern int	ar9285_1_2_init_calib(struct athn_softc *,
		    struct ieee80211_channel *, struct ieee80211_channel *);
extern void	ar9285_pa_calib(struct athn_softc *);
extern void	ar9280_reset_rx_gain(struct athn_softc *,
		    struct ieee80211_channel *);
extern void	ar9280_reset_tx_gain(struct athn_softc *,
		    struct ieee80211_channel *);
extern void	ar5416_reset_addac(struct athn_softc *,
		    struct ieee80211_channel *);
extern void	ar5416_reset_bb_gain(struct athn_softc *,
		    struct ieee80211_channel *);
extern void	ar5416_rf_reset(struct athn_softc *,
		    struct ieee80211_channel *);
