/*	$OpenBSD: if_wpivar.h,v 1.11 2007/06/05 19:49:40 damien Exp $	*/

/*-
 * Copyright (c) 2006, 2007
 *	Damien Bergamini <damien.bergamini@free.fr>
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

struct wpi_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsft;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	int8_t		wr_dbm_antsignal;
	int8_t		wr_dbm_antnoise;
	uint8_t		wr_antenna;
} __packed;

#define WPI_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_TSFT) |				\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL) |			\
	 (1 << IEEE80211_RADIOTAP_DBM_ANTNOISE) |			\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

struct wpi_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
	uint8_t		wt_hwqueue;
} __packed;

#define WPI_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_HWQUEUE))

struct wpi_dma_info {
	bus_dma_tag_t		tag;
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		paddr;
	caddr_t			vaddr;
	bus_size_t		size;
};

struct wpi_tx_data {
	bus_dmamap_t		map;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct wpi_tx_ring {
	struct wpi_dma_info	desc_dma;
	struct wpi_dma_info	cmd_dma;
	struct wpi_tx_desc	*desc;
	struct wpi_tx_cmd	*cmd;
	struct wpi_tx_data	*data;
	int			qid;
	int			count;
	int			queued;
	int			cur;
};

#define WPI_RBUF_COUNT	(WPI_RX_RING_COUNT + 16)

struct wpi_softc;

struct wpi_rbuf {
	struct wpi_softc	*sc;
	caddr_t			vaddr;
	bus_addr_t		paddr;
	SLIST_ENTRY(wpi_rbuf)	next;
};

struct wpi_rx_data {
	struct mbuf	*m;
};

struct wpi_rx_ring {
	struct wpi_dma_info	desc_dma;
	struct wpi_dma_info	buf_dma;
	uint32_t		*desc;
	struct wpi_rx_data	data[WPI_RX_RING_COUNT];
	struct wpi_rbuf		rbuf[WPI_RBUF_COUNT];
	SLIST_HEAD(, wpi_rbuf)	freelist;
	int			cur;
};

struct wpi_node {
	struct	ieee80211_node		ni;	/* must be the first */
	struct	ieee80211_amrr_node	amn;
};

struct wpi_power_sample {
	uint8_t	index;
	int8_t	power;
};

struct wpi_power_group {
#define WPI_SAMPLES_COUNT	5
	struct	wpi_power_sample samples[WPI_SAMPLES_COUNT];
	uint8_t	chan;
	int8_t	maxpwr;
	int16_t	temp;
};

struct wpi_softc {
	struct device		sc_dev;

	struct ieee80211com	sc_ic;
	int			(*sc_newstate)(struct ieee80211com *,
				    enum ieee80211_state, int);
	struct ieee80211_amrr	amrr;

	bus_dma_tag_t		sc_dmat;

	/* shared area */
	struct wpi_dma_info	shared_dma;
	struct wpi_shared	*shared;

	/* firmware DMA transfer */
	struct wpi_dma_info	fw_dma;

	/* rings */
	struct wpi_tx_ring	txq[4];
	struct wpi_tx_ring	cmdq;
	struct wpi_tx_ring	svcq;
	struct wpi_rx_ring	rxq;

	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	void 			*sc_ih;
	pci_chipset_tag_t	sc_pct;
	pcitag_t		sc_pcitag;
	bus_size_t		sc_sz;

	struct ksensordev	sensordev;
	struct ksensor		sensor;
	struct timeout		calib_to;
	int			calib_cnt;

	struct wpi_config	config;
	int			temp;

	uint8_t			cap;
	uint16_t		rev;
	uint8_t			type;
	struct wpi_power_group	groups[WPI_POWER_GROUPS_COUNT];
	int8_t			maxpwr[IEEE80211_CHAN_MAX];

	int			sc_tx_timer;
	void			*powerhook;

#if NBPFILTER > 0
	caddr_t			sc_drvbpf;

	union {
		struct wpi_rx_radiotap_header th;
		uint8_t	pad[IEEE80211_RADIOTAP_HDRLEN];
	} sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int			sc_rxtap_len;

	union {
		struct wpi_tx_radiotap_header th;
		uint8_t	pad[IEEE80211_RADIOTAP_HDRLEN];
	} sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int			sc_txtap_len;
#endif
};
