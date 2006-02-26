/*	$OpenBSD: if_vicvar.h,v 1.2 2006/02/26 02:13:54 brad Exp $	*/

/*
 * Copyright (c) 2006 Reyk Floeter <reyk@openbsd.org>
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

#ifndef _DEV_IC_VICVAR_H
#define _DEV_IC_VICVAR_H

#define VIC_DEBUG		1

#define VIC_BAR0		0x10	/* Base Address Register */

#define VIC_NBUF		100
#define VIC_NBUF_MAX		128
#define VIC_MAX_SCATTER		1	/* 8? */
#define VIC_QUEUE_SIZE		VIC_NBUF_MAX
#define VIC_QUEUE2_SIZE		1
#define VIC_INC(_x, _y)		(_x) = ((_x) + 1) % (_y)
#define VIC_INC_POS(_x, _y)	(_x) = (++(_x)) % (_y) ? (_x) : 1
#define VIC_TX_TIMEOUT		5
#define VIC_TIMER_DELAY		2
#define VIC_TIMER_MS(_ms)	(_ms * hz / 1000)

#define VIC_TXURN_WARN(_sc)	((_sc)->sc_txpending >= ((_sc)->sc_ntxbuf - 5))
#define VIC_TXURN(_sc)		((_sc)->sc_txpending >= (_sc)->sc_ntxbuf)
#define VIC_OFF_RXDESC(_n)							\
	(sizeof(struct vic_data) + ((_n) * sizeof(struct vic_rxdesc)))
#define VIC_OFF_TXDESC(_n)							\
	(sizeof(struct vic_data) +						\
	((sc->sc_nrxbuf + VIC_QUEUE2_SIZE) * sizeof(struct vic_rxdesc)) +	\
	((_n) * sizeof(struct vic_txdesc)))

#define VIC_WRITE(_reg, _val)							\
	bus_space_write_4(sc->sc_st, sc->sc_sh, (_reg), (_val))
#define VIC_READ(_reg)								\
	bus_space_read_4(sc->sc_st, sc->sc_sh, (_reg))
#define VIC_WRITE8(_reg, _val)							\
	bus_space_write_1(sc->sc_st, sc->sc_sh, (_reg), (_val))
#define VIC_READ8(_reg)								\
	bus_space_read_1(sc->sc_st, sc->sc_sh, (_reg))

struct vic_rxbuf {
	bus_dmamap_t		rxb_map;
	struct mbuf		*rxb_m;
};

struct vic_txbuf {
	bus_dmamap_t		txb_map;
	struct mbuf		*txb_m;
};

struct vic_softc {
	void			*sc_ih;
	struct device		sc_dev;

	struct arpcom		sc_ac;
	struct ifmedia		sc_media;
	struct timeout		sc_timer;

	struct timeout		sc_poll;	/* XXX poll */
	u_int			sc_polling;

	int			(*sc_enable)(struct vic_softc *);
	void			(*sc_disable)(struct vic_softc *);
	void			(*sc_power)(struct vic_softc *, int);

	bus_space_tag_t		sc_st;
	bus_space_handle_t	sc_sh;
	bus_dma_tag_t		sc_dmat;

	u_int32_t		sc_ver_major, sc_ver_minor;
	u_int32_t		sc_cap, sc_feature;

	u_int8_t		sc_lladdr[ETHER_ADDR_LEN];

	u_int32_t		sc_nrxbuf, sc_ntxbuf;

	struct vic_rxdesc	*sc_rxq, *sc_rxq2;
	struct vic_rxbuf	sc_rxbuf[VIC_QUEUE_SIZE];

	struct vic_txdesc	*sc_txq;
	struct vic_txbuf	sc_txbuf[VIC_QUEUE_SIZE];
	int			sc_txpending;
	int			sc_txtimeout;

	struct vic_data		*sc_data;
	bus_addr_t		sc_physaddr;
	bus_dmamap_t		sc_map;
	bus_dma_segment_t	sc_seg;
	int			sc_nsegs;
	int			sc_size;
};

#endif /* _DEV_IC_VICVAR_H */
