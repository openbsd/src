/*	$OpenBSD: if_nfevar.h,v 1.6 2006/02/04 09:46:48 damien Exp $	*/
/*
 * Copyright (c) 2005 Jonathan Gray <jsg@openbsd.org>
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

#define NFE_IFQ_MAXLEN	64

struct nfe_tx_data {
	bus_dmamap_t	map;
	bus_dmamap_t	active;
	struct mbuf	*m;
};

struct nfe_tx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct nfe_desc32	*desc32;
	struct nfe_desc64	*desc64;
	struct nfe_tx_data	data[NFE_TX_RING_COUNT];
	int			queued;
	int			cur;
	int			next;
};

struct nfe_rx_data {
	bus_dmamap_t	map;
	struct mbuf	*m;
};

struct nfe_rx_ring {
	bus_dmamap_t		map;
	bus_dma_segment_t	seg;
	bus_addr_t		physaddr;
	struct nfe_desc32	*desc32;
	struct nfe_desc64	*desc64;
	struct nfe_rx_data	data[NFE_RX_RING_COUNT];
	int			cur;
	int			next;
};

struct nfe_softc {
	struct device		sc_dev;
	struct arpcom		sc_arpcom;
	bus_space_handle_t	sc_memh;
	bus_space_tag_t		sc_memt;
	void			*sc_ih;
	bus_dma_tag_t		sc_dmat;
	struct mii_data		sc_mii;
	struct timeout		sc_timeout;
	void			*sc_powerhook;

	int			sc_if_flags;
	u_int			sc_flags;
#define NFE_JUMBO_SUP	0x01
#define NFE_40BIT_ADDR	0x02
#define NFE_HW_CSUM	0x04

	uint8_t			phyaddr;

	struct nfe_tx_ring	txq;
	struct nfe_rx_ring	rxq;
};
