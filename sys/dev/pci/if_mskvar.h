/*	$OpenBSD: if_mskvar.h,v 1.9 2010/09/20 07:40:38 deraadt Exp $	*/
/*	$NetBSD: if_skvar.h,v 1.6 2005/05/30 04:35:22 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */
/*	$OpenBSD: if_mskvar.h,v 1.9 2010/09/20 07:40:38 deraadt Exp $	*/

/*
 * Copyright (c) 1997, 1998, 1999, 2000
 *	Bill Paul <wpaul@ctr.columbia.edu>.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by Bill Paul.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY Bill Paul AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL Bill Paul OR THE VOICES IN HIS HEAD
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 *
 * $FreeBSD: /c/ncvs/src/sys/pci/if_skreg.h,v 1.9 2000/04/22 02:16:37 wpaul Exp $
 */

/*
 * Copyright (c) 2003 Nathan L. Binkert <binkertn@umich.edu>
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

#ifndef _DEV_PCI_IF_MSKVAR_H_
#define _DEV_PCI_IF_MSKVAR_H_

struct sk_chain {
	void			*sk_le;
	struct mbuf		*sk_mbuf;
	struct sk_chain		*sk_next;
};

/*
 * Number of DMA segments in a TxCB. Note that this is carefully
 * chosen to make the total struct size an even power of two. It's
 * critical that no TxCB be split across a page boundary since
 * no attempt is made to allocate physically contiguous memory.
 *
 */
#define SK_NTXSEG      30

struct sk_txmap_entry {
	bus_dmamap_t			dmamap;
	SIMPLEQ_ENTRY(sk_txmap_entry)	link;
};

struct msk_chain_data {
	struct sk_chain		sk_tx_chain[MSK_TX_RING_CNT];
	struct sk_chain		sk_rx_chain[MSK_RX_RING_CNT];
	struct sk_txmap_entry	*sk_tx_map[MSK_TX_RING_CNT];
	bus_dmamap_t		sk_rx_map[MSK_RX_RING_CNT];
	int			sk_tx_prod;
	int			sk_tx_cons;
	int			sk_tx_cnt;
	int			sk_rx_prod;
	int			sk_rx_cons;
	int			sk_rx_cnt;
};

struct msk_ring_data {
	struct msk_tx_desc	sk_tx_ring[MSK_TX_RING_CNT];
	struct msk_rx_desc	sk_rx_ring[MSK_RX_RING_CNT];
};

#define MSK_TX_RING_ADDR(sc, i) \
    ((sc)->sk_ring_map->dm_segs[0].ds_addr + \
     offsetof(struct msk_ring_data, sk_tx_ring[(i)]))

#define MSK_RX_RING_ADDR(sc, i) \
    ((sc)->sk_ring_map->dm_segs[0].ds_addr + \
     offsetof(struct msk_ring_data, sk_rx_ring[(i)]))

#define MSK_CDOFF(x)	offsetof(struct msk_ring_data, x)
#define MSK_CDTXOFF(x)	MSK_CDOFF(sk_tx_ring[(x)])
#define MSK_CDRXOFF(x)	MSK_CDOFF(sk_rx_ring[(x)])
#define MSK_CDSTOFF(x)	((x) * sizeof(struct msk_status_desc))

#define MSK_CDTXSYNC(sc, x, n, ops)					\
do {									\
	int __x, __n;							\
									\
	__x = (x);							\
	__n = (n);							\
									\
	/* If it will wrap around, sync to the end of the ring. */	\
	if ((__x + __n) > MSK_TX_RING_CNT) {				\
		bus_dmamap_sync((sc)->sk_softc->sc_dmatag,		\
		    (sc)->sk_ring_map, MSK_CDTXOFF(__x),		\
		    sizeof(struct msk_tx_desc) * (MSK_TX_RING_CNT - __x), \
		    (ops));						\
		__n -= (MSK_TX_RING_CNT - __x);				\
		__x = 0;						\
	}								\
									\
	/* Now sync whatever is left. */				\
	bus_dmamap_sync((sc)->sk_softc->sc_dmatag, (sc)->sk_ring_map,	\
	    MSK_CDTXOFF((__x)), sizeof(struct msk_tx_desc) * __n, (ops)); \
} while (/*CONSTCOND*/0)

#define MSK_CDRXSYNC(sc, x, ops)					\
do {									\
	bus_dmamap_sync((sc)->sk_softc->sc_dmatag, (sc)->sk_ring_map,	\
	    MSK_CDRXOFF((x)), sizeof(struct msk_rx_desc), (ops));	\
} while (/*CONSTCOND*/0)

#define MSK_CDSTSYNC(sc, x, ops)					\
do {									\
	bus_dmamap_sync((sc)->sc_dmatag, (sc)->sk_status_map,		\
	    MSK_CDSTOFF((x)), sizeof(struct msk_status_desc), (ops));	\
} while (/*CONSTCOND*/0)

#define SK_INC(x, y)	(x) = (x + 1) % y

/* Forward decl. */
struct sk_if_softc;

/* Softc for the Yukon-II controller. */
struct sk_softc {
	struct device		sk_dev;		/* generic device */
	bus_space_handle_t	sk_bhandle;	/* bus space handle */
	bus_space_tag_t		sk_btag;	/* bus space tag */
	bus_size_t		sk_bsize;	/* bus space size */
	void			*sk_intrhand;	/* irq handler handle */
	pci_chipset_tag_t	sk_pc;
	u_int8_t		sk_fibertype;
	u_int8_t		sk_type;
	u_int8_t		sk_rev;
	u_int8_t		sk_macs;	/* # of MACs */
	char			*sk_name;
	u_int32_t		sk_ramsize;	/* amount of RAM on NIC */
	u_int32_t		sk_intrmask;
	bus_dma_tag_t		sc_dmatag;
	struct sk_if_softc	*sk_if[2];
	struct msk_status_desc	*sk_status_ring;
	bus_dmamap_t		sk_status_map;
	bus_dma_segment_t	sk_status_seg;
	int			sk_status_nseg;
	int			sk_status_idx;
};

/* Softc for each logical interface */
struct sk_if_softc {
	struct device		sk_dev;		/* generic device */
	struct arpcom		arpcom;		/* interface info */
	struct mii_data		sk_mii;
	u_int8_t		sk_port;	/* port # on controller */
	u_int8_t		sk_xmac_rev;	/* XMAC chip rev (B2 or C1) */
	u_int32_t		sk_rx_ramstart;
	u_int32_t		sk_rx_ramend;
	u_int32_t		sk_tx_ramstart;
	u_int32_t		sk_tx_ramend;
	int			sk_pktlen;
	int			sk_link;
	struct timeout		sk_tick_ch;
	struct msk_chain_data	sk_cdata;
	struct msk_ring_data	*sk_rdata;
	bus_dmamap_t		sk_ring_map;
	bus_dma_segment_t	sk_ring_seg;
	int			sk_ring_nseg;
	int			sk_status_idx;
	struct sk_softc		*sk_softc;	/* parent controller */
	int			sk_tx_bmu;	/* TX BMU register */
	int			sk_if_flags;
	SIMPLEQ_HEAD(__sk_txmaphead, sk_txmap_entry)	sk_txmap_head;
};

struct skc_attach_args {
	u_int16_t	skc_port;
	u_int8_t	skc_type;
	u_int8_t	skc_rev;
};

#endif /* _DEV_PCI_IF_MSKVAR_H_ */
