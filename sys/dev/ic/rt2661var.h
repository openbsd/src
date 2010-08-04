/*	$OpenBSD: rt2661var.h,v 1.11 2010/08/04 19:48:33 damien Exp $	*/

/*-
 * Copyright (c) 2006
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

struct rt2661_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint64_t	wr_tsf;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antsignal;
} __packed;

#define RT2661_RX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_TSFT) |				\
	 (1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct rt2661_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define RT2661_TX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct rt2661_tx_data {
	bus_dmamap_t			map;
	struct mbuf			*m;
	struct ieee80211_node		*ni;
};

struct rt2661_tx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct rt2661_tx_desc	*desc;
	struct rt2661_tx_data	*data;
	int			count;
	int			queued;
	int			cur;
	int			next;
	int			stat;
};

struct rt2661_rx_data {
	bus_dmamap_t	map;
	struct mbuf	*m;
};

struct rt2661_rx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct rt2661_rx_desc	*desc;
	struct rt2661_rx_data	*data;
	int			count;
	int			cur;
	int			next;
};

struct rt2661_node {
	struct ieee80211_node		ni;
	struct ieee80211_amrr_node	amn;
};

struct rt2661_softc {
	struct device			sc_dev;

	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	struct ieee80211_amrr		amrr;

	int				(*sc_enable)(struct rt2661_softc *);
	void				(*sc_disable)(struct rt2661_softc *);
	void				(*sc_power)(struct rt2661_softc *, int);

	bus_dma_tag_t			sc_dmat;
	bus_space_tag_t			sc_st;
	bus_space_handle_t		sc_sh;

	struct timeout			scan_to;
	struct timeout			amrr_to;

	int				sc_id;
	int				sc_flags;
#define RT2661_ENABLED		(1 << 0)
#define RT2661_UPDATE_SLOT	(1 << 1)
#define RT2661_SET_SLOTTIME	(1 << 2)

	int				sc_tx_timer;

	struct ieee80211_channel	*sc_curchan;

	u_char				*ucode;
	size_t				ucsize;

	uint8_t				rf_rev;

	uint8_t				rfprog;
	uint8_t				rffreq;

	struct rt2661_tx_ring		txq[5];
	struct rt2661_tx_ring		mgtq;
	struct rt2661_rx_ring		rxq;

	uint32_t			rf_regs[4];
	int8_t				txpow[38];

	struct {
		uint8_t	reg;
		uint8_t	val;
	}				bbp_prom[16];

	int				hw_radio;
	int				rx_ant;
	int				tx_ant;
	int				nb_ant;
	int				ext_2ghz_lna;
	int				ext_5ghz_lna;
	int				rssi_2ghz_corr;
	int				rssi_5ghz_corr;

	int				ncalls;
	int				avg_rssi;
	int				sifs;

	uint32_t			erp_csr;

	uint8_t				bbp18;
	uint8_t				bbp21;
	uint8_t				bbp22;
	uint8_t				bbp16;
	uint8_t				bbp17;
	uint8_t				bbp64;

#if NBPFILTER > 0
	caddr_t				sc_drvbpf;

	union {
		struct rt2661_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap			sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct rt2661_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap			sc_txtapu.th
	int				sc_txtap_len;
#endif
	void				*sc_powerhook;
};

int	rt2661_attach(void *, int);
int	rt2661_detach(void *);
void	rt2661_suspend(void *);
void	rt2661_resume(void *);
int	rt2661_intr(void *);
