/*      $OpenBSD: athvar.h,v 1.4 2005/02/17 21:02:24 reyk Exp $  */
/*	$NetBSD: athvar.h,v 1.10 2004/08/10 01:03:53 dyoung Exp $	*/

/*-
 * Copyright (c) 2002-2004 Sam Leffler, Errno Consulting
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer,
 *    without modification.
 * 2. Redistributions in binary form must reproduce at minimum a disclaimer
 *    similar to the "NO WARRANTY" disclaimer below ("Disclaimer") and any
 *    redistribution must be conditioned upon including a substantially
 *    similar Disclaimer requirement for further binary redistribution.
 * 3. Neither the names of the above-listed copyright holders nor the names
 *    of any contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * Alternatively, this software may be distributed under the terms of the
 * GNU General Public License ("GPL") version 2 as published by the Free
 * Software Foundation.
 *
 * NO WARRANTY
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF NONINFRINGEMENT, MERCHANTIBILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
 * THE COPYRIGHT HOLDERS OR CONTRIBUTORS BE LIABLE FOR SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGES.
 *
 * $FreeBSD: src/sys/dev/ath/if_athvar.h,v 1.14 2004/04/03 03:33:02 sam Exp $
 */

/*
 * Defintions for the Atheros Wireless LAN controller driver.
 */
#ifndef _DEV_ATH_ATHVAR_H
#define _DEV_ATH_ATHVAR_H

#include <net80211/ieee80211_radiotap.h>
#include <dev/ic/ar5xxx.h>

#include "gpio.h"

#define	ATH_TIMEOUT		1000

#define	ATH_RXBUF	40		/* number of RX buffers */
#define	ATH_TXBUF	60		/* number of TX buffers */
#define	ATH_TXDESC	8		/* number of descriptors per buffer */
#define ATH_MAXGPIO	10		/* maximal number of gpio pins */

struct ath_recv_hist {
	int		arh_ticks;	/* sample time by system clock */
	u_int8_t	arh_rssi;	/* rssi */
	u_int8_t	arh_antenna;	/* antenna */
};
#define	ATH_RHIST_SIZE		16	/* number of samples */
#define	ATH_RHIST_NOTIME	(~0)

/*
 * Ioctl-related defintions for the Atheros Wireless LAN controller driver.
 */
struct ath_stats {
	u_int32_t	ast_watchdog;	/* device reset by watchdog */
	u_int32_t	ast_hardware;	/* fatal hardware error interrupts */
	u_int32_t	ast_bmiss;	/* beacon miss interrupts */
	u_int32_t	ast_rxorn;	/* rx overrun interrupts */
	u_int32_t	ast_rxeol;	/* rx eol interrupts */
	u_int32_t	ast_txurn;	/* tx underrun interrupts */
	u_int32_t	ast_intrcoal;	/* interrupts coalesced */
	u_int32_t	ast_tx_mgmt;	/* management frames transmitted */
	u_int32_t	ast_tx_discard;	/* frames discarded prior to assoc */
	u_int32_t	ast_tx_qstop;	/* output stopped 'cuz no buffer */
	u_int32_t	ast_tx_encap;	/* tx encapsulation failed */
	u_int32_t	ast_tx_nonode;	/* tx failed 'cuz no node */
	u_int32_t	ast_tx_nombuf;	/* tx failed 'cuz no mbuf */
	u_int32_t	ast_tx_nomcl;	/* tx failed 'cuz no cluster */
	u_int32_t	ast_tx_linear;	/* tx linearized to cluster */
	u_int32_t	ast_tx_nodata;	/* tx discarded empty frame */
	u_int32_t	ast_tx_busdma;	/* tx failed for dma resrcs */
	u_int32_t	ast_tx_xretries;/* tx failed 'cuz too many retries */
	u_int32_t	ast_tx_fifoerr;	/* tx failed 'cuz FIFO underrun */
	u_int32_t	ast_tx_filtered;/* tx failed 'cuz xmit filtered */
	u_int32_t	ast_tx_shortretry;/* tx on-chip retries (short) */
	u_int32_t	ast_tx_longretry;/* tx on-chip retries (long) */
	u_int32_t	ast_tx_badrate;	/* tx failed 'cuz bogus xmit rate */
	u_int32_t	ast_tx_noack;	/* tx frames with no ack marked */
	u_int32_t	ast_tx_rts;	/* tx frames with rts enabled */
	u_int32_t	ast_tx_cts;	/* tx frames with cts enabled */
	u_int32_t	ast_tx_shortpre;/* tx frames with short preamble */
	u_int32_t	ast_tx_altrate;	/* tx frames with alternate rate */
	u_int32_t	ast_tx_protect;	/* tx frames with protection */
	u_int32_t	ast_rx_nombuf;	/* rx setup failed 'cuz no mbuf */
	u_int32_t	ast_rx_busdma;	/* rx setup failed for dma resrcs */
	u_int32_t	ast_rx_orn;	/* rx failed 'cuz of desc overrun */
	u_int32_t	ast_rx_crcerr;	/* rx failed 'cuz of bad CRC */
	u_int32_t	ast_rx_fifoerr;	/* rx failed 'cuz of FIFO overrun */
	u_int32_t	ast_rx_badcrypt;/* rx failed 'cuz decryption */
	u_int32_t	ast_rx_phyerr;	/* rx failed 'cuz of PHY err */
	u_int32_t	ast_rx_phy[32];	/* rx PHY error per-code counts */
	u_int32_t	ast_rx_tooshort;/* rx discarded 'cuz frame too short */
	u_int32_t	ast_rx_toobig;	/* rx discarded 'cuz frame too large */
	u_int32_t	ast_rx_ctl;	/* rx discarded 'cuz ctl frame */
	u_int32_t	ast_be_nombuf;	/* beacon setup failed 'cuz no mbuf */
	u_int32_t	ast_per_cal;	/* periodic calibration calls */
	u_int32_t	ast_per_calfail;/* periodic calibration failed */
	u_int32_t	ast_per_rfgain;	/* periodic calibration rfgain reset */
	u_int32_t	ast_rate_calls;	/* rate control checks */
	u_int32_t	ast_rate_raise;	/* rate control raised xmit rate */
	u_int32_t	ast_rate_drop;	/* rate control dropped xmit rate */
};

#define	SIOCGATHSTATS	_IOWR('i', 137, struct ifreq)

/*
 * Radio capture format.
 */
#define ATH_RX_RADIOTAP_PRESENT (		\
	(1 << IEEE80211_RADIOTAP_FLAGS)		| \
	(1 << IEEE80211_RADIOTAP_RATE)		| \
	(1 << IEEE80211_RADIOTAP_CHANNEL)	| \
	(1 << IEEE80211_RADIOTAP_ANTENNA)	| \
	(1 << IEEE80211_RADIOTAP_DB_ANTSIGNAL)	| \
	0)

struct ath_rx_radiotap_header {
	struct ieee80211_radiotap_header wr_ihdr;
	u_int8_t	wr_flags;		/* XXX for padding */
	u_int8_t	wr_rate;
	u_int16_t	wr_chan_freq;
	u_int16_t	wr_chan_flags;
	u_int8_t	wr_antenna;
	u_int8_t	wr_antsignal;
};

#define ATH_TX_RADIOTAP_PRESENT (		\
	(1 << IEEE80211_RADIOTAP_FLAGS)		| \
	(1 << IEEE80211_RADIOTAP_RATE)		| \
	(1 << IEEE80211_RADIOTAP_CHANNEL)	| \
	(1 << IEEE80211_RADIOTAP_DBM_TX_POWER)	| \
	(1 << IEEE80211_RADIOTAP_ANTENNA)	| \
	0)

struct ath_tx_radiotap_header {
	struct ieee80211_radiotap_header wt_ihdr;
	u_int8_t	wt_flags;		/* XXX for padding */
	u_int8_t	wt_rate;
	u_int16_t	wt_chan_freq;
	u_int16_t	wt_chan_flags;
	u_int8_t	wt_txpower;
	u_int8_t	wt_antenna;
};

/* 
 * driver-specific node 
 */
struct ath_node {
	struct ieee80211_node an_node;	/* base class */
	u_int		an_tx_ok;	/* tx ok pkt */
	u_int		an_tx_err;	/* tx !ok pkt */
	u_int		an_tx_retr;	/* tx retry count */
	int		an_tx_upper;	/* tx upper rate req cnt */
	u_int		an_tx_antenna;	/* antenna for last good frame */
	u_int		an_rx_antenna;	/* antenna for last rcvd frame */
	struct ath_recv_hist an_rx_hist[ATH_RHIST_SIZE];
	u_int		an_rx_hist_next;/* index of next ``free entry'' */
};
#define	ATH_NODE(_n)	((struct ath_node *)(_n))

struct ath_buf {
	TAILQ_ENTRY(ath_buf)	bf_list;
	bus_dmamap_t		bf_dmamap;	/* DMA map of the buffer */
#define bf_nseg			bf_dmamap->dm_nsegs
#define bf_mapsize		bf_dmamap->dm_mapsize
#define bf_segs			bf_dmamap->dm_segs
	struct ath_desc		*bf_desc;	/* virtual addr of desc */
	bus_addr_t		bf_daddr;	/* physical addr of desc */
	struct mbuf		*bf_m;		/* mbuf for buf */
	struct ieee80211_node	*bf_node;	/* pointer to the node */
#define	ATH_MAX_SCATTER		64
};

typedef struct ath_task {
	void	(*t_func)(void*, int);
	void	*t_context;
} ath_task_t;

struct ath_softc {
#ifndef __FreeBSD__
	struct device		sc_dev;
#endif
	struct ieee80211com	sc_ic;		/* IEEE 802.11 common */
#ifndef __FreeBSD__
	int			(*sc_enable)(struct ath_softc *);
	void			(*sc_disable)(struct ath_softc *);
	void			(*sc_power)(struct ath_softc *, int);
#endif
	int			(*sc_newstate)(struct ieee80211com *,
					enum ieee80211_state, int);
	void			(*sc_node_free)(struct ieee80211com *,
					struct ieee80211_node *);
	void			(*sc_node_copy)(struct ieee80211com *,
					struct ieee80211_node *,
					const struct ieee80211_node *);
	void			(*sc_recv_mgmt)(struct ieee80211com *,
				    struct mbuf *, struct ieee80211_node *,
				    int, int, u_int32_t);
#ifdef __FreeBSD__
	device_t		sc_dev;
#endif
	bus_space_tag_t		sc_st;		/* bus space tag */
	bus_space_handle_t	sc_sh;		/* bus space handle */
	bus_dma_tag_t		sc_dmat;	/* bus DMA tag */
#ifdef __FreeBSD__
	struct mtx		sc_mtx;		/* master lock (recursive) */
#endif
	struct ath_hal		*sc_ah;		/* Atheros HAL */
	unsigned int		sc_invalid  : 1,/* disable hardware accesses */
				sc_doani    : 1,/* dynamic noise immunity */
				sc_hasveol  : 1,/* tx VEOL support */
				sc_probing  : 1;/* probing AP on beacon miss */
						/* rate tables */
	const HAL_RATE_TABLE	*sc_rates[IEEE80211_MODE_MAX];
	const HAL_RATE_TABLE	*sc_currates;	/* current rate table */
	enum ieee80211_phymode	sc_curmode;	/* current phy mode */
	u_int8_t		sc_rixmap[256];	/* IEEE to h/w rate table ix */
	u_int8_t		sc_hwmap[32];	/* h/w rate ix to IEEE table */
	HAL_INT			sc_imask;	/* interrupt mask copy */

#ifdef __FreeBSD__
	struct bpf_if		*sc_drvbpf;
#else
	caddr_t			sc_drvbpf;
#endif
	union {
		struct ath_tx_radiotap_header th;
		u_int8_t	pad[64];
	} u_tx_rt;
	int			sc_tx_th_len;
	union {
		struct ath_rx_radiotap_header th;
		u_int8_t	pad[64];
	} u_rx_rt;
	int			sc_rx_th_len;

	struct ath_desc		*sc_desc;	/* TX/RX descriptors */
	bus_dma_segment_t	sc_dseg;
#ifndef __NetBSD__
	int			sc_dnseg;	/* number of segments */
#endif
	bus_dmamap_t		sc_ddmamap;	/* DMA map for descriptors */
	bus_addr_t		sc_desc_paddr;	/* physical addr of sc_desc */
	bus_addr_t		sc_desc_len;	/* size of sc_desc */

	ath_task_t		sc_fataltask;	/* fatal int processing */
	ath_task_t		sc_rxorntask;	/* rxorn int processing */

	TAILQ_HEAD(, ath_buf)	sc_rxbuf;	/* receive buffer */
	u_int32_t		*sc_rxlink;	/* link ptr in last RX desc */
	ath_task_t		sc_rxtask;	/* rx int processing */

	u_int			sc_txhalq;	/* HAL q for outgoing frames */
	u_int32_t		*sc_txlink;	/* link ptr in last TX desc */
	int			sc_tx_timer;	/* transmit timeout */
	TAILQ_HEAD(, ath_buf)	sc_txbuf;	/* transmit buffer */
#ifdef __FreeBSD__
	struct mtx		sc_txbuflock;	/* txbuf lock */
#endif
	TAILQ_HEAD(, ath_buf)	sc_txq;		/* transmitting queue */
#ifdef __FreeBSD__
	struct mtx		sc_txqlock;	/* lock on txq and txlink */
#endif
	ath_task_t		sc_txtask;	/* tx int processing */

	u_int			sc_bhalq;	/* HAL q for outgoing beacons */
	struct ath_buf		*sc_bcbuf;	/* beacon buffer */
	struct ath_buf		*sc_bufptr;	/* allocated buffer ptr */
	ath_task_t		sc_swbatask;	/* swba int processing */
	ath_task_t		sc_bmisstask;	/* bmiss int processing */

#ifdef __OpenBSD__
	struct timeval		sc_last_ch;
	struct timeout		sc_cal_to;
	struct timeval		sc_last_beacon;
	struct timeout		sc_scan_to;
#else
	struct callout		sc_cal_ch;	/* callout handle for cals */
	struct callout		sc_scan_ch;	/* callout handle for scan */
#endif
	struct ath_stats	sc_stats;	/* interface statistics */

#ifndef __FreeBSD__
	void			*sc_sdhook;	/* shutdown hook */
	void			*sc_powerhook;	/* power management hook */
	u_int			sc_flags;	/* misc flags */
#endif

	u_int8_t                sc_broadcast_addr[IEEE80211_ADDR_LEN];

#if NGPIO > 0
	struct gpio_chipset_tag sc_gpio_gc;	/* gpio(4) framework */
	gpio_pin_t		sc_gpio_pins[ATH_MAXGPIO];
#endif
};

/* unaligned little endian access */     
#define LE_READ_2(p)							\
	((u_int16_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8)))
#define LE_READ_4(p)							\
	((u_int32_t)							\
	 ((((u_int8_t *)(p))[0]      ) | (((u_int8_t *)(p))[1] <<  8) |	\
	 (((u_int8_t *)(p))[2] << 16) | (((u_int8_t *)(p))[3] << 24)))

#ifdef AR_DEBUG
enum {
	ATH_DEBUG_XMIT		= 0x00000001,	/* basic xmit operation */
	ATH_DEBUG_XMIT_DESC	= 0x00000002,	/* xmit descriptors */
	ATH_DEBUG_RECV		= 0x00000004,	/* basic recv operation */
	ATH_DEBUG_RECV_DESC	= 0x00000008,	/* recv descriptors */
	ATH_DEBUG_RATE		= 0x00000010,	/* rate control */
	ATH_DEBUG_RESET		= 0x00000020,	/* reset processing */
	ATH_DEBUG_MODE		= 0x00000040,	/* mode init/setup */
	ATH_DEBUG_BEACON	= 0x00000080,	/* beacon handling */
	ATH_DEBUG_WATCHDOG	= 0x00000100,	/* watchdog timeout */
	ATH_DEBUG_INTR		= 0x00001000,	/* ISR */
	ATH_DEBUG_TX_PROC	= 0x00002000,	/* tx ISR proc */
	ATH_DEBUG_RX_PROC	= 0x00004000,	/* rx ISR proc */
	ATH_DEBUG_BEACON_PROC	= 0x00008000,	/* beacon ISR proc */
	ATH_DEBUG_CALIBRATE	= 0x00010000,	/* periodic calibration */
	ATH_DEBUG_ANY		= 0xffffffff
};
#define	IFF_DUMPPKTS(_ifp, _m) \
	((ath_debug & _m) || \
	    ((_ifp)->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#define	DPRINTF(_m,X)	if (ath_debug & (_m)) printf X
#else
#define	IFF_DUMPPKTS(_ifp, _m) \
	(((_ifp)->if_flags & (IFF_DEBUG|IFF_LINK2)) == (IFF_DEBUG|IFF_LINK2))
#define	DPRINTF(_m, X)
#endif

/*
 * Wrapper code
 */
#ifndef __FreeBSD__
#undef KASSERT
#define KASSERT(cond, complaint) if (!(cond)) panic complaint

#define	ATH_ATTACHED		0x0001		/* attach has succeeded */
#define ATH_ENABLED		0x0002		/* chip is enabled */
#define ATH_GPIO		0x0004		/* gpio device attached */

#define	ATH_IS_ENABLED(sc)	((sc)->sc_flags & ATH_ENABLED)
#endif

#define	sc_tx_th		u_tx_rt.th
#define	sc_rx_th		u_rx_rt.th

#define	ATH_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_mtx, device_get_nameunit((_sc)->sc_dev), \
		 MTX_NETWORK_LOCK, MTX_DEF | MTX_RECURSE)
#define	ATH_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_mtx)
#define	ATH_LOCK(_sc)		mtx_lock(&(_sc)->sc_mtx)
#define	ATH_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_mtx)
#define	ATH_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_mtx, MA_OWNED)

#define	ATH_TXBUF_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_txbuflock, \
		device_get_nameunit((_sc)->sc_dev), "xmit buf q", MTX_DEF)
#define	ATH_TXBUF_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_txbuflock)
#define	ATH_TXBUF_LOCK(_sc)		mtx_lock(&(_sc)->sc_txbuflock)
#define	ATH_TXBUF_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_txbuflock)
#define	ATH_TXBUF_LOCK_ASSERT(_sc) \
	mtx_assert(&(_sc)->sc_txbuflock, MA_OWNED)

#define	ATH_TXQ_LOCK_INIT(_sc) \
	mtx_init(&(_sc)->sc_txqlock, \
		device_get_nameunit((_sc)->sc_dev), "xmit q", MTX_DEF)
#define	ATH_TXQ_LOCK_DESTROY(_sc)	mtx_destroy(&(_sc)->sc_txqlock)
#define	ATH_TXQ_LOCK(_sc)		mtx_lock(&(_sc)->sc_txqlock)
#define	ATH_TXQ_UNLOCK(_sc)		mtx_unlock(&(_sc)->sc_txqlock)
#define	ATH_TXQ_LOCK_ASSERT(_sc)	mtx_assert(&(_sc)->sc_txqlock, MA_OWNED)

#define ATH_TICKS() (tick)
#define ATH_CALLOUT_INIT(chp) callout_init((chp))
#define ATH_TASK_INIT(task, func, context)	\
	do {					\
		(task)->t_func = (func);	\
		(task)->t_context = (context);	\
	} while (0)
#define ATH_TASK_RUN_OR_ENQUEUE(task) ((*(task)->t_func)((task)->t_context, 1))

typedef unsigned long u_intptr_t;

int	ath_attach(u_int16_t, struct ath_softc *);
int	ath_detach(struct ath_softc *, int);
void	ath_resume(struct ath_softc *, int);
void	ath_suspend(struct ath_softc *, int);
#ifdef __NetBSD__
int	ath_activate(struct device *, enum devact);
void	ath_power(int, void *);
#endif
void	ath_shutdown(void *);
int	ath_intr(void *);

/*
 * HAL definitions to comply with local coding convention.
 */
#define	ath_hal_reset(_ah, _opmode, _chan, _outdoor, _pstatus) \
	((*(_ah)->ah_reset)((_ah), (_opmode), (_chan), (_outdoor), (_pstatus)))
#define	ath_hal_getratetable(_ah, _mode) \
	((*(_ah)->ah_getRateTable)((_ah), (_mode)))
#define	ath_hal_getmac(_ah, _mac) \
	((*(_ah)->ah_getMacAddress)((_ah), (_mac)))
#define	ath_hal_setmac(_ah, _mac) \
	((*(_ah)->ah_setMacAddress)((_ah), (_mac)))
#define	ath_hal_intrset(_ah, _mask) \
	((*(_ah)->ah_setInterrupts)((_ah), (_mask)))
#define	ath_hal_intrget(_ah) \
	((*(_ah)->ah_getInterrupts)((_ah)))
#define	ath_hal_intrpend(_ah) \
	((*(_ah)->ah_isInterruptPending)((_ah)))
#define	ath_hal_getisr(_ah, _pmask) \
	((*(_ah)->ah_getPendingInterrupts)((_ah), (_pmask)))
#define	ath_hal_updatetxtriglevel(_ah, _inc) \
	((*(_ah)->ah_updateTxTrigLevel)((_ah), (_inc)))
#define	ath_hal_setpower(_ah, _mode, _sleepduration) \
	((*(_ah)->ah_setPowerMode)((_ah), (_mode), AH_TRUE, (_sleepduration)))
#define	ath_hal_keyreset(_ah, _ix) \
	((*(_ah)->ah_resetKeyCacheEntry)((_ah), (_ix)))
#define	ath_hal_keyset(_ah, _ix, _pk) \
	((*(_ah)->ah_setKeyCacheEntry)((_ah), (_ix), (_pk), NULL, AH_FALSE))
#define	ath_hal_keyisvalid(_ah, _ix) \
	(((*(_ah)->ah_isKeyCacheEntryValid)((_ah), (_ix))))
#define	ath_hal_keysetmac(_ah, _ix, _mac) \
	((*(_ah)->ah_setKeyCacheEntryMac)((_ah), (_ix), (_mac)))
#define	ath_hal_getrxfilter(_ah) \
	((*(_ah)->ah_getRxFilter)((_ah)))
#define	ath_hal_setrxfilter(_ah, _filter) \
	((*(_ah)->ah_setRxFilter)((_ah), (_filter)))
#define	ath_hal_setmcastfilter(_ah, _mfilt0, _mfilt1) \
	((*(_ah)->ah_setMulticastFilter)((_ah), (_mfilt0), (_mfilt1)))
#define	ath_hal_waitforbeacon(_ah, _bf) \
	((*(_ah)->ah_waitForBeaconDone)((_ah), (_bf)->bf_daddr))
#define	ath_hal_putrxbuf(_ah, _bufaddr) \
	((*(_ah)->ah_setRxDP)((_ah), (_bufaddr)))
#define	ath_hal_gettsf32(_ah) \
	((*(_ah)->ah_getTsf32)((_ah)))
#define	ath_hal_gettsf64(_ah) \
	((*(_ah)->ah_getTsf64)((_ah)))
#define	ath_hal_resettsf(_ah) \
	((*(_ah)->ah_resetTsf)((_ah)))
#define	ath_hal_rxena(_ah) \
	((*(_ah)->ah_enableReceive)((_ah)))
#define	ath_hal_puttxbuf(_ah, _q, _bufaddr) \
	((*(_ah)->ah_setTxDP)((_ah), (_q), (_bufaddr)))
#define	ath_hal_gettxbuf(_ah, _q) \
	((*(_ah)->ah_getTxDP)((_ah), (_q)))
#define	ath_hal_getrxbuf(_ah) \
	((*(_ah)->ah_getRxDP)((_ah)))
#define	ath_hal_txstart(_ah, _q) \
	((*(_ah)->ah_startTxDma)((_ah), (_q)))
#define	ath_hal_setchannel(_ah, _chan) \
	((*(_ah)->ah_setChannel)((_ah), (_chan)))
#define	ath_hal_calibrate(_ah, _chan) \
	((*(_ah)->ah_perCalibration)((_ah), (_chan)))
#define	ath_hal_setledstate(_ah, _state) \
	((*(_ah)->ah_setLedState)((_ah), (_state)))
#define	ath_hal_beaconinit(_ah, _nextb, _bperiod) \
	((*(_ah)->ah_beaconInit)((_ah), (_nextb), (_bperiod)))
#define	ath_hal_beaconreset(_ah) \
	((*(_ah)->ah_resetStationBeaconTimers)((_ah)))
#define	ath_hal_beacontimers(_ah, _bs, _tsf, _dc, _cc) \
	((*(_ah)->ah_setStationBeaconTimers)((_ah), (_bs), (_tsf), \
	 (_dc), (_cc)))
#define	ath_hal_setassocid(_ah, _bss, _associd) \
	((*(_ah)->ah_writeAssocid)((_ah), (_bss), (_associd), 0))
#define	ath_hal_getregdomain(_ah, _prd) \
	(*(_prd) = (_ah)->ah_getRegDomain(_ah))
#define	ath_hal_getcountrycode(_ah, _pcc) \
	(*(_pcc) = (_ah)->ah_countryCode)
#define	ath_hal_detach(_ah) \
	((*(_ah)->ah_detach)(_ah))

#define ath_hal_gpiocfgoutput(_ah, _gpio) \
	((*(_ah)->ah_gpioCfgOutput)((_ah), (_gpio)))
#define ath_hal_gpiocfginput(_ah, _gpio) \
	((*(_ah)->ah_gpioCfgInput)((_ah), (_gpio)))
#define ath_hal_gpioget(_ah, _gpio) \
	((*(_ah)->ah_gpioGet)((_ah), (_gpio)))
#define ath_hal_gpioset(_ah, _gpio, _b) \
	((*(_ah)->ah_gpioSet)((_ah), (_gpio), (_b)))
#define ath_hal_gpiosetintr(_ah, _gpio, _b) \
	((*(_ah)->ah_gpioSetIntr)((_ah), (_gpio), (_b)))

#define	ath_hal_setopmode(_ah) \
	((*(_ah)->ah_setPCUConfig)((_ah)))
#define	ath_hal_stoptxdma(_ah, _qnum) \
	((*(_ah)->ah_stopTxDma)((_ah), (_qnum)))
#define	ath_hal_stoppcurecv(_ah) \
	((*(_ah)->ah_stopPcuReceive)((_ah)))
#define	ath_hal_startpcurecv(_ah) \
	((*(_ah)->ah_startPcuReceive)((_ah)))
#define	ath_hal_stopdmarecv(_ah) \
	((*(_ah)->ah_stopDmaReceive)((_ah)))
#define	ath_hal_getdiagstate(_ah, _id, _indata, _insize, _outdata, _outsize) \
	((*(_ah)->ah_getDiagState)((_ah), (_id), \
	 (_indata), (_insize), (_outdata), (_outsize)))

#define	ath_hal_setuptxqueue(_ah, _type, _qinfo) \
	((*(_ah)->ah_setupTxQueue)((_ah), (_type), (_qinfo)))
#define	ath_hal_resettxqueue(_ah, _q) \
	((*(_ah)->ah_resetTxQueue)((_ah), (_q)))
#define	ath_hal_releasetxqueue(_ah, _q) \
	((*(_ah)->ah_releaseTxQueue)((_ah), (_q)))
#define	ath_hal_hasveol(_ah) \
	((*(_ah)->ah_hasVEOL)((_ah)))
#define	ath_hal_getrfgain(_ah) \
	((*(_ah)->ah_getRfGain)((_ah)))
#define	ath_hal_rxmonitor(_ah) \
	((*(_ah)->ah_rxMonitor)((_ah)))

#define	ath_hal_setuprxdesc(_ah, _ds, _size, _intreq) \
	((*(_ah)->ah_setupRxDesc)((_ah), (_ds), (_size), (_intreq)))
#define	ath_hal_rxprocdesc(_ah, _ds, _dspa, _dsnext) \
	((*(_ah)->ah_procRxDesc)((_ah), (_ds), (_dspa), (_dsnext)))
#define	ath_hal_setuptxdesc(_ah, _ds, _plen, _hlen, _atype, _txpow, \
	    _txr0, _txtr0, _keyix, _ant, _flags, \
	    _rtsrate, _rtsdura) \
	((*(_ah)->ah_setupTxDesc)((_ah), (_ds), (_plen), (_hlen), (_atype), \
	    (_txpow), (_txr0), (_txtr0), (_keyix), (_ant), \
	    (_flags), (_rtsrate), (_rtsdura)))
#define	ath_hal_setupxtxdesc(_ah, _ds, \
	    _txr1, _txtr1, _txr2, _txtr2, _txr3, _txtr3) \
	((*(_ah)->ah_setupXTxDesc)((_ah), (_ds), \
	    (_txr1), (_txtr1), (_txr2), (_txtr2), (_txr3), (_txtr3)))
#define	ath_hal_filltxdesc(_ah, _ds, _l, _first, _last) \
	((*(_ah)->ah_fillTxDesc)((_ah), (_ds), (_l), (_first), (_last)))
#define	ath_hal_txprocdesc(_ah, _ds) \
	((*(_ah)->ah_procTxDesc)((_ah), (_ds)))

#endif /* _DEV_ATH_ATHVAR_H */
