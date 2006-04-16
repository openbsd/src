/* $OpenBSD: mfivar.h,v 1.6 2006/04/16 14:40:00 marco Exp $ */
/*
 * Copyright (c) 2006 Marco Peereboom <marco@peereboom.us>
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

#define DEVNAME(_s)     ((_s)->sc_dev.dv_xname)

#define MFI_DEBUG
#ifdef MFI_DEBUG
extern uint32_t			mfi_debug;
#define DPRINTF(x...)		do { if (mfi_debug) printf(x); } while(0)
#define DNPRINTF(n,x...)	do { if (mfi_debug & n) printf(x); } while(0)
#define	MFI_D_CMD		0x0001
#define	MFI_D_INTR		0x0002
#define	MFI_D_MISC		0x0004
#define	MFI_D_DMA		0x0008
#define	MFI_D_IOCTL		0x0010
#define	MFI_D_RW		0x0020
#else
#define DPRINTF(x...)
#define DNPRINTF(n,x...)
#endif

struct mfi_mem {
	bus_dmamap_t		am_map;
	bus_dma_segment_t	am_seg;
	size_t			am_size;
	caddr_t			am_kva;
};

#define MFIMEM_MAP(_am)		((_am)->am_map)
#define MFIMEM_DVA(_am)		((_am)->am_map->dm_segs[0].ds_addr)
#define MFIMEM_KVA(_am)		((void *)(_am)->am_kva)

struct mfi_softc {
	struct device		sc_dev;
	void			*sc_ih;
	struct scsi_link	sc_link;

	u_int32_t		sc_flags;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_dma_tag_t		sc_dmat;

	/* firmware determined max and totals */
	uint32_t		sc_max_cmds;
	uint32_t		sc_max_sgl;

	/* reply queue */
	struct mfi_mem		*sc_reply_q;
};

struct mfi_ccb {
	struct mfi_softc	*ccb_sc;

	union mfi_frame		ccb_frame;
	paddr_t			ccb_pframe;

#if 0
	struct ami_iocmd	ccb_cmd;
	struct ami_passthrough	*ccb_pt;
	paddr_t			ccb_ptpa;
	struct ami_sgent	*ccb_sglist;
	paddr_t			ccb_sglistpa;
	int			ccb_offset;
	bus_dmamap_t		ccb_dmamap;
#endif
	struct scsi_xfer	*ccb_xs;
	void			(*ccb_done)(struct mfi_softc *sc,
				    struct mfi_ccb *ccb);

	volatile enum {
		MFI_CCB_FREE,
		MFI_CCB_READY,
		MFI_CCB_QUEUED,
		MFI_CCB_PREQUEUED
	}			ccb_state;
	uint32_t		ccb_flags;
#define MFI_CCB_F_ERR			(1<<0)
	TAILQ_ENTRY(MFI_ccb)	ccb_link;
};

int	mfi_attach(struct mfi_softc *sc);
int	mfi_intr(void *);
