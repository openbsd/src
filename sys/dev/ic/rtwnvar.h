/*	$OpenBSD: rtwnvar.h,v 1.7 2016/06/17 10:53:55 stsp Exp $	*/

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

/* Operations provided by bus-specific attachment drivers. */
struct rtwn_ops {
	void		*cookie; /* Attachment driver's private data. */

	uint8_t		(*read_1)(void *, uint16_t);
	uint16_t	(*read_2)(void *, uint16_t);
	uint32_t	(*read_4)(void *, uint16_t);
	void		(*write_1)(void *, uint16_t, uint8_t);
	void		(*write_2)(void *, uint16_t, uint16_t);
	void		(*write_4)(void *, uint16_t, uint32_t);
	int		(*tx)(void *, struct mbuf *, struct ieee80211_node *);
	int		(*power_on)(void *);
	int		(*dma_init)(void *);
	int		(*fw_loadpage)(void *, int, uint8_t *, int);
	int		(*load_firmware)(void *, u_char **fw, size_t *);
	void		(*mac_init)(void *);
	void		(*bb_init)(void *);
	int		(*alloc_buffers)(void *);
	int		(*init)(void *);
	void		(*stop)(void *);
	int		(*is_oactive)(void *);
	void		(*next_calib)(void *);
	void		(*cancel_calib)(void *);
	void		(*next_scan)(void *);
	void		(*cancel_scan)(void *);
	void		(*wait_async)(void *);
};

#define RTWN_LED_LINK	0
#define RTWN_LED_DATA	1

#define RTWN_INT_ENABLE	(R92C_IMR_ROK | R92C_IMR_VODOK | R92C_IMR_VIDOK | \
			R92C_IMR_BEDOK | R92C_IMR_BKDOK | R92C_IMR_MGNTDOK | \
			R92C_IMR_HIGHDOK | R92C_IMR_BDOK | R92C_IMR_RDU | \
			R92C_IMR_RXFOVW)

struct rtwn_softc {
	/* sc_ops must be initialized by the attachment driver! */
	struct rtwn_ops			sc_ops;

	struct device			*sc_pdev;
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
	struct task			init_task;
	int				ac2idx[EDCA_NUM_AC];
	uint32_t			sc_flags;
#define RTWN_FLAG_CCK_HIPWR		0x01
#define RTWN_FLAG_BUSY			0x02
#define RTWN_FLAG_FORCE_RAID_11B	0x04

	uint32_t		chip;
#define RTWN_CHIP_92C		0x00000001
#define RTWN_CHIP_92C_1T2R	0x00000002
#define RTWN_CHIP_UMC		0x00000004
#define RTWN_CHIP_UMC_A_CUT	0x00000008
#define RTWN_CHIP_88C		0x00000010
#define RTWN_CHIP_88E		0x00000020

#define RTWN_CHIP_PCI		0x40000000
#define RTWN_CHIP_USB		0x80000000

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
	struct r92c_rom			rom;

	uint8_t				r88e_rom[512];
	uint8_t				cck_tx_pwr[6];
	uint8_t				ht40_tx_pwr[5];
	int8_t				bw20_tx_pwr_diff;
	int8_t				ofdm_tx_pwr_diff;

	uint32_t			rf_chnlbw[R92C_MAX_CHAINS];
};

int		rtwn_attach(struct device *, struct rtwn_softc *, uint32_t);
int		rtwn_detach(struct rtwn_softc *, int);
int		rtwn_activate(struct rtwn_softc *, int);
int8_t		rtwn_get_rssi(struct rtwn_softc *, int, void *);
void		rtwn_update_avgrssi(struct rtwn_softc *, int, int8_t);
void		rtwn_calib(struct rtwn_softc *);
void		rtwn_next_scan(struct rtwn_softc *);
int		rtwn_newstate(struct ieee80211com *, enum ieee80211_state, int);
void		rtwn_updateedca(struct ieee80211com *);
int		rtwn_set_key(struct ieee80211com *, struct ieee80211_node *,
		    struct ieee80211_key *);
void		rtwn_delete_key(struct ieee80211com *,
		    struct ieee80211_node *, struct ieee80211_key *);
int		rtwn_ioctl(struct ifnet *, u_long, caddr_t);
void		rtwn_start(struct ifnet *);
void		rtwn_fw_reset(struct rtwn_softc *);
