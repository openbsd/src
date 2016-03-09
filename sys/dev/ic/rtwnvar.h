/*	$OpenBSD: rtwnvar.h,v 1.1 2016/03/09 18:18:28 stsp Exp $	*/

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
	void		(*next_scan)(void *);
	int		(*tx)(void *, struct mbuf *, struct ieee80211_node *);
	int		(*configure_dma)(void *);
	void		(*enable_intr)(void *);
	void		(*disable_intr)(void *);
	void		(*stop)(void *);
	int		(*is_oactive)(void *);
};

struct rtwn_softc {
	/* sc_ops must be initialized by the attachment driver! */
	struct rtwn_ops			sc_ops;

	struct device			*sc_pdev;
	struct ieee80211com		sc_ic;
	int				(*sc_newstate)(struct ieee80211com *,
					    enum ieee80211_state, int);
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
	struct r92c_rom			rom;

	uint32_t			rf_chnlbw[R92C_MAX_CHAINS];
};

void		rtwn_attach(struct device *, struct rtwn_softc *);
int		rtwn_detach(struct rtwn_softc *, int);
int		rtwn_activate(struct rtwn_softc *, int);
int8_t		rtwn_get_rssi(struct rtwn_softc *, int, void *);
void		rtwn_update_avgrssi(struct rtwn_softc *, int, int8_t);
