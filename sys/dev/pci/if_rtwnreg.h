/*	$OpenBSD: if_rtwnreg.h,v 1.5 2016/03/07 18:05:41 stsp Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2015 Stefan Sperling <stsp@openbsd.org>
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

#define R92C_MAX_TX_PWR	0x3f

#define R92C_PUBQ_NPAGES	176
#define R92C_HPQ_NPAGES		41
#define R92C_LPQ_NPAGES		28
#define R92C_TXPKTBUF_COUNT	256
#define R92C_TX_PAGE_COUNT	\
	(R92C_PUBQ_NPAGES + R92C_HPQ_NPAGES + R92C_LPQ_NPAGES)
#define R92C_TX_PAGE_BOUNDARY	(R92C_TX_PAGE_COUNT + 1)

#define R92C_H2C_NBOX	4

/*
 * Driver definitions.
 */
#define RTWN_NTXQUEUES			9
#define RTWN_RX_LIST_COUNT		256
#define RTWN_TX_LIST_COUNT		256
#define RTWN_HOST_CMD_RING_COUNT	32

/* TX queue indices. */
#define RTWN_BK_QUEUE			0
#define RTWN_BE_QUEUE			1
#define RTWN_VI_QUEUE			2
#define RTWN_VO_QUEUE			3
#define RTWN_BEACON_QUEUE		4
#define RTWN_TXCMD_QUEUE		5
#define RTWN_MGNT_QUEUE			6
#define RTWN_HIGH_QUEUE			7
#define RTWN_HCCA_QUEUE			8

/* RX queue indices. */
#define RTWN_RX_QUEUE			0

#define RTWN_RXBUFSZ	(16 * 1024)
#define RTWN_TXBUFSZ	(sizeof(struct r92c_tx_desc_pci) + IEEE80211_MAX_LEN)

#define RTWN_RIDX_COUNT	28

#define RTWN_TX_TIMEOUT	5000	/* ms */

#define RTWN_LED_LINK	0
#define RTWN_LED_DATA	1

struct rtwn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_dbm_antsignal;
} __packed;

#define RTWN_RX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL |		\
	 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL)

struct rtwn_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define RTWN_TX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_CHANNEL)

struct rtwn_softc;

struct rtwn_rx_data {
	bus_dmamap_t		map;
	struct mbuf		*m;
};

struct rtwn_rx_ring {
	struct r92c_rx_desc_pci	*desc;
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	int			nsegs;
	struct rtwn_rx_data	rx_data[RTWN_RX_LIST_COUNT];

};
struct rtwn_tx_data {
	bus_dmamap_t			map;
	struct mbuf			*m;
	struct ieee80211_node		*ni;
};

struct rtwn_tx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	int			nsegs;
	struct r92c_tx_desc_pci	*desc;
	struct rtwn_tx_data	tx_data[RTWN_TX_LIST_COUNT];
	int			queued;
	int			cur;
};

struct rtwn_host_cmd {
	void	(*cb)(struct rtwn_softc *, void *);
	uint8_t	data[256];
};

struct rtwn_cmd_key {
	struct ieee80211_key	key;
	uint16_t		associd;
};

struct rtwn_host_cmd_ring {
	struct rtwn_host_cmd	cmd[RTWN_HOST_CMD_RING_COUNT];
	int			cur;
	int			next;
	int			queued;
};

struct rtwn_softc {
	struct device			sc_dev;
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);

	/* PCI specific goo. */
	bus_dma_tag_t 		sc_dmat;
	pci_chipset_tag_t	sc_pc;
	pcitag_t		sc_tag;
	void			*sc_ih;
	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	bus_size_t		sc_mapsize;
	int			sc_cap_off;


	struct timeout			scan_to;
	struct timeout			calib_to;
	struct task			init_task;
	int				ac2idx[EDCA_NUM_AC];
	u_int				sc_flags;
#define RTWN_FLAG_CCK_HIPWR	0x01
#define RTWN_FLAG_BUSY		0x02

	u_int				chip;
#define RTWN_CHIP_88C		0x00
#define RTWN_CHIP_92C		0x01
#define RTWN_CHIP_92C_1T2R	0x02
#define RTWN_CHIP_UMC		0x04
#define RTWN_CHIP_UMC_A_CUT	0x08

	uint8_t				board_type;
	uint8_t				regulatory;
	uint8_t				pa_setting;
	int				avg_pwdb;
	int				thcal_state;
	int				thcal_lctemp;
	int				ntxchains;
	int				nrxchains;
	int				ledlink;

	int				sc_tx_timer;
	int				fwcur;
	struct rtwn_rx_ring		rx_ring;
	struct rtwn_tx_ring		tx_ring[RTWN_NTXQUEUES];
	uint32_t			qfullmsk;
	struct r92c_rom			rom;

	uint32_t			rf_chnlbw[R92C_MAX_CHAINS];
#if NBPFILTER > 0
	caddr_t				sc_drvbpf;

	union {
		struct rtwn_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct rtwn_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
#endif
};
