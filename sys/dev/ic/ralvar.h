/*	$OpenBSD: ralvar.h,v 1.2 2005/02/19 09:45:16 damien Exp $  */

/*-
 * Copyright (c) 2005
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

struct ral_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antsignal;
};

#define RAL_RX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct ral_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
};

#define RAL_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL))

struct ral_tx_data {
	bus_dmamap_t			map;
	struct mbuf			*m;
	struct ieee80211_node		*ni;
	struct ieee80211_rssdesc	id;
};

struct ral_tx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct ral_tx_desc	*desc;
	struct ral_tx_data	*data;
	int			count;
	int			queued;
	int			cur;
	int			next;
	int			cur_encrypt;
	int			next_encrypt;
};

struct ral_rx_data {
	bus_dmamap_t	map;
	struct mbuf	*m;
	int		drop;
};

struct ral_rx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct ral_rx_desc	*desc;
	struct ral_rx_data	*data;
	int			count;
	int			cur;
	int			next;
	int			cur_decrypt;
};

struct ral_softc {
	struct device			sc_dev;

	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);

	int				(*sc_enable)(struct ral_softc *);
	void				(*sc_disable)(struct ral_softc *);
	void				(*sc_power)(struct ral_softc *, int);

	bus_dma_tag_t			sc_dmat;
	bus_space_tag_t			sc_st;
	bus_space_handle_t		sc_sh;

	struct ieee80211_rssadapt	rssadapt;

	struct timeout			scan_ch;
	struct timeout			rssadapt_ch;

	int				sc_tx_timer;

	uint32_t			asic_rev;
	uint32_t			eeprom_rev;
	uint8_t				rf_rev;

	struct ral_tx_ring		txq;
	struct ral_tx_ring		prioq;
	struct ral_tx_ring		atimq;
	struct ral_tx_ring		bcnq;
	struct ral_rx_ring		rxq;

	uint32_t			rf_regs[4];
	uint8_t				txpow[14];

	struct {
		uint8_t		reg;
		uint8_t		val;	
	}				bbp_prom[16];

	int				led_mode;
	int				hw_radio;
	int				rx_ant;
	int				tx_ant;
	int				nb_ant;
	int				bbp_tune;
	

#if NBPFILTER > 0
	caddr_t				sc_drvbpf;

	union {
		struct ral_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct ral_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
#endif
};

int	ral_attach(struct ral_softc *);
int	ral_detach(struct ral_softc *);
int	ral_intr(void *);
int	ral_activate(struct device *, enum devact);
