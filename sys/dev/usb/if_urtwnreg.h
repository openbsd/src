/*	$OpenBSD: if_urtwnreg.h,v 1.8 2016/03/07 16:17:36 stsp Exp $	*/

/*-
 * Copyright (c) 2010 Damien Bergamini <damien.bergamini@free.fr>
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

/* Maximum number of output pipes is 3. */
#define R92C_MAX_EPOUT	3

#define R92C_MAX_TX_PWR	0x3f

#define R92C_PUBQ_NPAGES	231
#define R92C_TXPKTBUF_COUNT	256
#define R92C_TX_PAGE_COUNT	248
#define R92C_TX_PAGE_BOUNDARY	(R92C_TX_PAGE_COUNT + 1)
#define R88E_TXPKTBUF_COUNT	177
#define R88E_TX_PAGE_COUNT	169
#define R88E_TX_PAGE_BOUNDARY	(R88E_TX_PAGE_COUNT + 1)

#define R92C_H2C_NBOX	4

/* USB Requests. */
#define R92C_REQ_REGS	0x05

/* Rx MAC descriptor. */
struct r92c_rx_stat {
	uint32_t	rxdw0;
#define R92C_RXDW0_PKTLEN_M	0x00003fff
#define R92C_RXDW0_PKTLEN_S	0
#define R92C_RXDW0_CRCERR	0x00004000
#define R92C_RXDW0_ICVERR	0x00008000
#define R92C_RXDW0_INFOSZ_M	0x000f0000
#define R92C_RXDW0_INFOSZ_S	16
#define R92C_RXDW0_QOS		0x00800000
#define R92C_RXDW0_SHIFT_M	0x03000000
#define R92C_RXDW0_SHIFT_S	24
#define R92C_RXDW0_PHYST	0x04000000
#define R92C_RXDW0_DECRYPTED	0x08000000

	uint32_t	rxdw1;
	uint32_t	rxdw2;
#define R92C_RXDW2_PKTCNT_M	0x00ff0000
#define R92C_RXDW2_PKTCNT_S	16

	uint32_t	rxdw3;
#define R92C_RXDW3_RATE_M	0x0000003f
#define R92C_RXDW3_RATE_S	0
#define R92C_RXDW3_HT		0x00000040
#define R92C_RXDW3_HTC		0x00000400

	uint32_t	rxdw4;
	uint32_t	rxdw5;
} __packed __attribute__((aligned(4)));

/* Tx MAC descriptor. */
struct r92c_tx_desc {
	uint32_t	txdw0;
#define R92C_TXDW0_PKTLEN_M	0x0000ffff
#define R92C_TXDW0_PKTLEN_S	0
#define R92C_TXDW0_OFFSET_M	0x00ff0000
#define R92C_TXDW0_OFFSET_S	16
#define R92C_TXDW0_BMCAST	0x01000000
#define R92C_TXDW0_LSG		0x04000000
#define R92C_TXDW0_FSG		0x08000000
#define R92C_TXDW0_OWN		0x80000000

	uint32_t	txdw1;
#define R92C_TXDW1_MACID_M	0x0000001f
#define R92C_TXDW1_MACID_S	0
#define R88E_TXDW1_MACID_M	0x0000003f
#define R88E_TXDW1_MACID_S	0
#define R92C_TXDW1_AGGEN	0x00000020
#define R92C_TXDW1_AGGBK	0x00000040
#define R92C_TXDW1_QSEL_M	0x00001f00
#define R92C_TXDW1_QSEL_S	8
#define R92C_TXDW1_QSEL_BE	0x00
#define R92C_TXDW1_QSEL_MGNT	0x12
#define R92C_TXDW1_RAID_M	0x000f0000
#define R92C_TXDW1_RAID_S	16
#define R92C_TXDW1_CIPHER_M	0x00c00000
#define R92C_TXDW1_CIPHER_S	22
#define R92C_TXDW1_CIPHER_NONE	0
#define R92C_TXDW1_CIPHER_RC4	1
#define R92C_TXDW1_CIPHER_AES	3
#define R92C_TXDW1_PKTOFF_M	0x7c000000
#define R92C_TXDW1_PKTOFF_S	26

	uint32_t	txdw2;
#define R88E_TXDW2_AGGBK	0x00010000

	uint16_t	txdw3;
	uint16_t	txdseq;

	uint32_t	txdw4;
#define R92C_TXDW4_RTSRATE_M	0x0000003f
#define R92C_TXDW4_RTSRATE_S	0
#define R92C_TXDW4_QOS		0x00000040
#define R92C_TXDW4_HWSEQ	0x00000080
#define R92C_TXDW4_DRVRATE	0x00000100
#define R92C_TXDW4_CTS2SELF	0x00000800
#define R92C_TXDW4_RTSEN	0x00001000
#define R92C_TXDW4_HWRTSEN	0x00002000
#define R92C_TXDW4_SCO_M	0x003f0000
#define R92C_TXDW4_SCO_S	20
#define R92C_TXDW4_SCO_SCA	1
#define R92C_TXDW4_SCO_SCB	2
#define R92C_TXDW4_40MHZ	0x02000000

	uint32_t	txdw5;
#define R92C_TXDW5_DATARATE_M	0x0000003f
#define R92C_TXDW5_DATARATE_S	0
#define R92C_TXDW5_SGI		0x00000040
#define R92C_TXDW5_AGGNUM_M	0xff000000
#define R92C_TXDW5_AGGNUM_S	24

	uint32_t	txdw6;
	uint16_t	txdsum;
	uint16_t	pad;
} __packed __attribute__((aligned(4)));


/*
 * Driver definitions.
 */
#define URTWN_RX_LIST_COUNT		1
#define URTWN_TX_LIST_COUNT		8
#define URTWN_HOST_CMD_RING_COUNT	32

#define URTWN_RXBUFSZ	(16 * 1024)
#define URTWN_TXBUFSZ	(sizeof(struct r92c_tx_desc) + IEEE80211_MAX_LEN)

#define URTWN_RIDX_COUNT	28

#define URTWN_TX_TIMEOUT	5000	/* ms */

#define URTWN_LED_LINK	0
#define URTWN_LED_DATA	1

struct urtwn_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	uint8_t		wr_flags;
	uint8_t		wr_rate;
	uint16_t	wr_chan_freq;
	uint16_t	wr_chan_flags;
	uint8_t		wr_dbm_antsignal;
} __packed;

#define URTWN_RX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_RATE |			\
	 1 << IEEE80211_RADIOTAP_CHANNEL |		\
	 1 << IEEE80211_RADIOTAP_DBM_ANTSIGNAL)

struct urtwn_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	uint8_t		wt_flags;
	uint16_t	wt_chan_freq;
	uint16_t	wt_chan_flags;
} __packed;

#define URTWN_TX_RADIOTAP_PRESENT			\
	(1 << IEEE80211_RADIOTAP_FLAGS |		\
	 1 << IEEE80211_RADIOTAP_CHANNEL)

struct urtwn_softc;

struct urtwn_rx_data {
	struct urtwn_softc	*sc;
	struct usbd_xfer	*xfer;
	uint8_t			*buf;
};

struct urtwn_tx_data {
	struct urtwn_softc		*sc;
	struct usbd_pipe		*pipe;
	struct usbd_xfer		*xfer;
	uint8_t				*buf;
	TAILQ_ENTRY(urtwn_tx_data)	next;
};

struct urtwn_host_cmd {
	void	(*cb)(struct urtwn_softc *, void *);
	uint8_t	data[256];
};

struct urtwn_cmd_newstate {
	enum ieee80211_state	state;
	int			arg;
};

struct urtwn_cmd_key {
	struct ieee80211_key	key;
	uint16_t		associd;
};

struct urtwn_host_cmd_ring {
	struct urtwn_host_cmd	cmd[URTWN_HOST_CMD_RING_COUNT];
	int			cur;
	int			next;
	int			queued;
};

struct urtwn_softc {
	struct device			sc_dev;
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	struct usbd_device		*sc_udev;
	struct usbd_interface		*sc_iface;
	struct usb_task			sc_task;
	struct timeout			scan_to;
	struct timeout			calib_to;
	struct usbd_pipe		*rx_pipe;
	struct usbd_pipe		*tx_pipe[R92C_MAX_EPOUT];
	int				ac2idx[EDCA_NUM_AC];
	u_int				sc_flags;
#define URTWN_FLAG_CCK_HIPWR		0x01
#define URTWN_FLAG_FORCE_RAID_11B	0x02

	u_int				chip;
#define	URTWN_CHIP_92C		0x01
#define	URTWN_CHIP_92C_1T2R	0x02
#define	URTWN_CHIP_UMC		0x04
#define	URTWN_CHIP_UMC_A_CUT	0x08
#define	URTWN_CHIP_88E		0x10

	void				(*sc_rf_write)(struct urtwn_softc *,
					    int, uint8_t, uint32_t);
	int				(*sc_power_on)(struct urtwn_softc *);
	int				(*sc_dma_init)(struct urtwn_softc *);

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
	struct urtwn_host_cmd_ring	cmdq;
	int				fwcur;
	struct urtwn_rx_data		rx_data[URTWN_RX_LIST_COUNT];
	struct urtwn_tx_data		tx_data[URTWN_TX_LIST_COUNT];
	TAILQ_HEAD(, urtwn_tx_data)	tx_free_list;
	struct r92c_rom			rom;
	uint8_t				r88e_rom[512];
	uint8_t				cck_tx_pwr[6];
	uint8_t				ht40_tx_pwr[5];
	int8_t				bw20_tx_pwr_diff;
	int8_t				ofdm_tx_pwr_diff;

	uint32_t			rf_chnlbw[R92C_MAX_CHAINS];
#if NBPFILTER > 0
	caddr_t				sc_drvbpf;

	union {
		struct urtwn_rx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_rxtapu;
#define sc_rxtap	sc_rxtapu.th
	int				sc_rxtap_len;

	union {
		struct urtwn_tx_radiotap_header th;
		uint8_t	pad[64];
	}				sc_txtapu;
#define sc_txtap	sc_txtapu.th
	int				sc_txtap_len;
#endif
};
