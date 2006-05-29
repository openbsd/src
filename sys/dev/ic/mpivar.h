/*	$OpenBSD: mpivar.h,v 1.4 2006/05/29 19:55:37 dlg Exp $ */

/*
 * Copyright (c) 2005 David Gwynne <dlg@openbsd.org>
 * Copyright (c) 2005 Marco Peereboom <marco@openbsd.org>
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


#define MPI_REQUEST_SIZE	512
#define MPI_REPLY_SIZE		128

/*
 * this is the max number of sge's we can stuff in a request frame:
 * sizeof(scsi_io) + sizeof(sense) + sizeof(sge) * 32 = MPI_REQUEST_SIZE
 */
#define MPI_MAX_SGL		36

struct mpi_dmamem {
	bus_dmamap_t		mdm_map;
	bus_dma_segment_t	mdm_seg;
	size_t			mdm_size;
	caddr_t			mdm_kva;
};
#define MPI_DMA_MAP(_mdm)	((_mdm)->mdm_map)
#define MPI_DMA_DVA(_mdm)	((_mdm)->mdm_map->dm_segs[0].ds_addr)
#define MPI_DMA_KVA(_mdm)	((void *)(_mdm)->mdm_kva)

struct mpi_ccb_bundle {
	struct mpi_msg_scsi_io	mcb_io; /* sgl must follow */
	struct mpi_sge		mcb_sgl[MPI_MAX_SGL];
	struct scsi_sense_data	mcb_sense;
} __packed;

struct mpi_softc;

struct mpi_ccb {
	struct mpi_softc	*ccb_sc;
	int			ccb_id;

	struct scsi_xfer	*ccb_xs;
	bus_dmamap_t		ccb_dmamap;

	bus_addr_t		ccb_offset;
	void			*ccb_cmd;
	paddr_t			ccb_cmd_dva;

	volatile enum {
		MPI_CCB_FREE,
		MPI_CCB_READY,
		MPI_CCB_QUEUED
	}			ccb_state;
	void			(*ccb_done)(struct mpi_ccb *);
	void			*ccb_reply;
	paddr_t			ccb_reply_dva;

	TAILQ_ENTRY(mpi_ccb)	ccb_link;
};

TAILQ_HEAD(mpi_ccb_list, mpi_ccb);

struct mpi_softc {
	struct device		sc_dev;
	struct scsi_link	sc_link;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	int			sc_maxcmds;
	int			sc_first_sgl_len;
	int			sc_maxchdepth;
	u_int8_t		sc_porttype;

	int			sc_buswidth;
	int			sc_target;

	struct mpi_dmamem	*sc_requests;
	struct mpi_ccb		*sc_ccbs;
	struct mpi_ccb_list	sc_ccb_free;

	struct mpi_dmamem	*sc_replies;
};

int	mpi_attach(struct mpi_softc *);
void	mpi_detach(struct mpi_softc *);

int	mpi_intr(void *);
