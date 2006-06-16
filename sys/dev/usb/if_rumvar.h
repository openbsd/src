/*	$OpenBSD: if_rumvar.h,v 1.1 2006/06/16 22:30:46 niallo Exp $  */
/*-
 * Copyright (c) 2005, 2006 Damien Bergamini <damien.bergamini@free.fr>
 * Copyright (c) 2006 Niall O'Higgins <niallo@openbsd.org>
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

#define RT2573_RX_LIST_COUNT	1
#define RT2573_TX_LIST_COUNT	5

struct rum_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_antenna;
	uint8_t		wr_antsignal;
} __packed;

#define RT2573_RX_RADIOTAP_PRESENT					\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA) |				\
	 (1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL))

struct rum_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint8_t		wt_rate;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
	uint8_t		wt_antenna;
} __packed;

#define RT2573_TX_RADIOTAP_PRESENT						\
	((1 << IEEE80211_RADIOTAP_FLAGS) |				\
	 (1 << IEEE80211_RADIOTAP_RATE) |				\
	 (1 << IEEE80211_RADIOTAP_CHANNEL) |				\
	 (1 << IEEE80211_RADIOTAP_ANTENNA))

struct rum_softc;

struct rum_tx_data {
	struct rum_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
	struct mbuf		*m;
	struct ieee80211_node	*ni;
};

struct rum_rx_data {
	struct rum_softc	*sc;
	usbd_xfer_handle	xfer;
	uint8_t			*buf;
	struct mbuf		*m;
};

struct rum_tx_ring {
	struct rum_tx_data	*data;
	struct rum_tx_desc	*desc;
	int			count;
	int			queued;
	int			cur;
	int			next;
	int			stat;
};
struct rum_rx_ring {
	struct rum_rx_data	*data;
	struct rum_rx_desc	*desc;
	int			cur;
	int			next;
	int			stat;
};

struct rum_softc {
	USBBASEDEVICE			sc_dev;
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);

	usbd_device_handle		sc_udev;
	usbd_interface_handle		sc_iface;

	struct ieee80211_channel	*sc_curchan;

	int				sc_rx_no;
	int				sc_tx_no;

	uint32_t			asic_rev;
	uint16_t			macbbp_rev;
	uint8_t				rf_rev;
	uint32_t			rfprog;
	uint8_t				rffreq;

	usbd_pipe_handle		sc_rx_pipeh;
	usbd_pipe_handle		sc_tx_pipeh;

	enum ieee80211_state		sc_state;
	struct usb_task			sc_task;

	struct rum_rx_data		rx_data[RT2573_RX_LIST_COUNT];
	struct rum_tx_data		tx_data[RT2573_TX_LIST_COUNT];
	int				tx_queued;

	struct rum_tx_ring		txq[5];
	struct rum_tx_ring		mgtq;
	struct rum_rx_ring		rxq;

	struct timeout			scan_ch;

	int				sc_tx_timer;

	uint16_t			sta[11];
	uint32_t			rf_regs[4];
	uint8_t				txpow[14];

	struct {
		uint8_t	val;
		uint8_t	reg;
	} __packed			bbp_prom[16];

	int				led_mode;
	int				hw_radio;
	int				rx_ant;
	int				tx_ant;
	int				nb_ant;

#if NBPFILTER > 0
	caddr_t				sc_drvbpf;

	union {
		struct rum_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct rum_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
	struct timeout			rssadapt_ch;
#endif
};

