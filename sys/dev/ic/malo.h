/*	$OpenBSD: malo.h,v 1.4 2006/10/17 19:40:39 claudio Exp $ */

/*
 * Copyright (c) 2006 Claudio Jeker <claudio@openbsd.org>
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

struct malo_rx_desc;
struct malo_rx_data;

struct malo_rx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct malo_rx_desc	*desc;
	struct malo_rx_data	*data;
	int			count;
	int			cur;
	int			next;
};

struct malo_tx_desc;
struct malo_tx_data;

struct malo_tx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct malo_tx_desc	*desc;
	struct malo_tx_data	*data;
	int			count;
	int			queued;
	int			cur;
	int			next;
	int			stat;
};

struct malo_softc {
	struct device		sc_dev;
	struct ieee80211com	sc_ic;
	struct malo_rx_ring	sc_rxring;
	struct malo_tx_ring	sc_txring;

	bus_dma_tag_t		sc_dmat;
	bus_space_tag_t		sc_mem1_bt;
	bus_space_tag_t		sc_mem2_bt;
	bus_space_handle_t	sc_mem1_bh;
	bus_space_handle_t	sc_mem2_bh;

	bus_dmamap_t		sc_cmd_dmam;
	bus_dma_segment_t	sc_cmd_dmas;
	void			*sc_cmd_mem;
	bus_addr_t		sc_cmd_dmaaddr;
	uint32_t		*sc_cookie;
	bus_addr_t		sc_cookie_dmaaddr;

	int			(*sc_newstate)
				(struct ieee80211com *,
				 enum ieee80211_state, int);

	int			(*sc_enable)(struct malo_softc *);
	void			(*sc_disable)(struct malo_softc *);
};

int malo_intr(void *arg);
int malo_attach(struct malo_softc *sc);
int malo_detach(void *arg);
