/* $OpenBSD: imxenet.h,v 1.1 2013/09/06 20:45:53 patrick Exp $ */
/*
 * Copyright (c) 2012-2013 Patrick Wildt <patrick@blueri.se>
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

/* what should we use? */
#define ENET_MAX_TXD		32
#define ENET_MAX_RXD		32

#define ENET_MAX_PKT_SIZE	1536

#define ENET_ROUNDUP(size, unit) (((size) + (unit) - 1) & ~((unit) - 1))

/* buffer descriptor status bits */
#define ENET_RXD_EMPTY		(1 << 15)
#define ENET_RXD_WRAP		(1 << 13)
#define ENET_RXD_LAST		(1 << 11)
#define ENET_RXD_MISS		(1 << 8)
#define ENET_RXD_BC		(1 << 7)
#define ENET_RXD_MC		(1 << 6)
#define ENET_RXD_LG		(1 << 5)
#define ENET_RXD_NO		(1 << 4)
#define ENET_RXD_CR		(1 << 2)
#define ENET_RXD_OV		(1 << 1)
#define ENET_RXD_TR		(1 << 0)

#define ENET_TXD_READY		(1 << 15)
#define ENET_TXD_WRAP		(1 << 13)
#define ENET_TXD_LAST		(1 << 11)
#define ENET_TXD_TC		(1 << 10)
#define ENET_TXD_ABC		(1 << 9)
#define ENET_TXD_STATUS_MASK	0x3ff

#ifdef ENET_ENHANCED_BD
/* enhanced */
#define ENET_RXD_INT		(1 << 23)

#define ENET_TXD_INT		(1 << 30)
#endif


/*
 * Bus dma allocation structure used by
 * imxenet_dma_malloc and imxenet_dma_free.
 */
struct imxenet_dma_alloc {
	bus_addr_t		dma_paddr;
	caddr_t			dma_vaddr;
	bus_dma_tag_t		dma_tag;
	bus_dmamap_t		dma_map;
	bus_dma_segment_t	dma_seg;
	bus_size_t		dma_size;
	int			dma_nseg;
};

struct imxenet_buf_desc {
	uint16_t data_length;		/* payload's length in bytes */
	uint16_t status;		/* BD's status (see datasheet) */
	uint32_t data_pointer;		/* payload's buffer address */
#ifdef ENET_ENHANCED_BD
	uint32_t enhanced_status;	/* enhanced status with IEEE 1588 */
	uint32_t reserved0;		/* reserved */
	uint32_t update_done;		/* buffer descriptor update done */
	uint32_t timestamp;		/* IEEE 1588 timestamp */
	uint32_t reserved1[2];		/* reserved */
#endif
};

struct imxenet_buffer {
	uint8_t data[ENET_MAX_PKT_SIZE];
};
