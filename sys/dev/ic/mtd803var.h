/* $OpenBSD: mtd803var.h,v 1.2 2003/08/19 04:03:53 mickey Exp $ */
/* $NetBSD: mtd803var.h,v 1.1 2002/11/07 21:57:00 martin Exp $ */

/*-
 * Copyright (c) 2002 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Peter Bex <Peter.Bex@student.kun.nl>.
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
 *      This product includes software developed by the NetBSD
 *      Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
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

#ifndef __DEV_IC_MTD803VAR_H__
#define __DEV_IC_MTD803VAR_H__

#include <dev/mii/miivar.h>

/* Number of Tx and Rx descriptors */
#define MTD_NUM_TXD		128
#define MTD_NUM_RXD		128
/* Tx and Rx buffer size */
#define MTD_RXBUF_SIZE		768
#define MTD_TXBUF_SIZE		768

/* DMA mem must be longword (4 bytes) aligned */
#define MTD_DMA_ALIGN		4


/* Descriptor structure */
struct mtd_desc {
	u_int32_t stat;			/* Status field */
	u_int32_t conf;			/* Config field */
	u_int32_t data;			/* Data buffer start address */
	u_int32_t next;			/* Next descriptor address */
};

/* Softc struct */
struct mtd_softc {
	struct device		dev;
	struct mii_data		mii;
	struct arpcom		arpcom;
	bus_space_tag_t		bus_tag;
	bus_space_handle_t	bus_handle;
	void *			sd_hook;
	volatile unsigned int	cur_tx;
	volatile unsigned int	cur_rx;

	bus_dma_tag_t		dma_tag;
	struct mtd_desc *	desc;
	bus_dmamap_t		desc_dma_map;
	caddr_t			buf;
	bus_dmamap_t		buf_dma_map;
};


/* Transmit descriptor layout */
	/* Status register */
#define MTD_TXD_OWNER		0x80000000	/* Owner bit */
#define MTD_TXD_RSRVD0		0x7fffc000	/* Bits [30:14] are reserved */
#define MTD_TXD_ABORT		0x00002000	/* Transmit aborted */
#define MTD_TXD_CSL		0x00001000	/* Carrier Sense Loss */
#define MTD_TXD_LCOL		0x00000800	/* Late collision */
#define MTD_TXD_EXCOL		0x00000400	/* Excessive collisions */
#define MTD_TXD_DFD		0x00000200	/* Deferred */
#define MTD_TXD_HBFAIL		0x00000100	/* Heart-beat failure */
#define MTD_TXD_NRC		0x000000ff	/* Collision Retry Count */
	/* Configuration register */
#define MTD_TXD_CONF_IRQC	0x80000000	/* Interrupt control */
#define MTD_TXD_CONF_EIRQC	0x40000000	/* Early interrupt control */
#define MTD_TXD_CONF_LSD	0x20000000	/* Last descriptor */
#define MTD_TXD_CONF_FSD	0x10000000	/* First descriptor */
#define MTD_TXD_CONF_CRC	0x08000000	/* CRC append */
#define MTD_TXD_CONF_PAD	0x04000000	/* Pad control */
#define MTD_TXD_CONF_RLCOL	0x02000000	/* Retry Late Collision */
#define MTD_TXD_CONF_RSRVD0	0x01c00000	/* Bits [24:22] are reserved */
#define MTD_TXD_CONF_PKTS	0x003ff800	/* Packet size */
#define MTD_TXD_CONF_BUFS	0x000007ff	/* Transmit buffer size */

#define MTD_TXD_PKTS_SHIFT	11

/* Receive descriptor layout */
	/* Status register */
#define MTD_RXD_OWNER		0x80000000	/* Owner bit */
#define MTD_RXD_RSRVD3		0x70000000	/* Bits [30:28] are reserved */
#define MTD_RXD_FLEN		0x0fff0000	/* Frame length */
#define MTD_RXD_RSRVD2		0x00008000	/* Bit 15 is reserved */
#define MTD_RXD_MAR		0x00004000	/* Multicast Address Received */
#define MTD_RXD_BAR		0x00002000	/* Broadcast Address Received */
#define MTD_RXD_PAR		0x00001000	/* Physical Address Received */
#define MTD_RXD_FSD		0x00000800	/* First Descriptor */
#define MTD_RXD_LSD		0x00000400	/* Last Descriptor */
#define MTD_RXD_RSRVD1		0x00000300	/* Bits [9:8] are reserved */
#define MTD_RXD_ERRSUM		0x00000080	/* Error summary */
#define MTD_RXD_RUNT		0x00000040	/* Runt packet received */
#define MTD_RXD_LONG		0x00000020	/* Long packet received */
#define MTD_RXD_FALERR		0x00000010	/* Frame alignment error */
#define MTD_RXD_CRC		0x00000008	/* CRC error. See manual :) */
#define MTD_RXD_RXERR		0x00000004	/* Receive error */
#define MTD_RXD_RSRVD0		0x00000003	/* Bits [1:0] are reserved */
	/* Configuration register */
#define MTD_RXD_CONF_RSRVD0	0xfffffc00	/* Bits [31:11] are reserved */
#define MTD_RXD_CONF_BUFS	0x000003ff	/* Receive buffer size */

#define MTD_RXD_FLEN_SHIFT	16

extern int mtd_config __P((struct mtd_softc *));
extern int mtd_irq_h __P((void *));

#endif	/* __DEV_IC_MTD803VAR_H__ */
