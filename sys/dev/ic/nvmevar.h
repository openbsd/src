/*	$OpenBSD: nvmevar.h,v 1.1 2014/04/12 05:06:58 dlg Exp $ */

/*
 * Copyright (c) 2014 David Gwynne <dlg@openbsd.org>
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

struct nvme_dmamem {
	bus_dmamap_t		ndm_map;
	bus_dma_segment_t	ndm_seg;
	size_t			ndm_size;
	caddr_t			ndm_kva;
};
#define NVME_DMA_MAP(_ndm)	((_ndm)->ndm_map)
#define NVME_DMA_DVA(_ndm)	((u_int64_t)(_ndm)->ndm_map->dm_segs[0].ds_addr)
#define NVME_DMA_KVA(_ndm)	((void *)(_ndm)->ndm_kva)

struct nvme_queue {
	struct nvme_dmamem	*q_sq_dmamem;
	struct nvme_dmamem	*q_cq_dmamem;
	bus_size_t 		q_sqtdbl; /* submission queue tail doorbell */
	bus_size_t 		q_cqhdbl; /* completion queue head doorbell */
	u_int			q_entries;
	u_int			q_sq_head;
	u_int			q_cq_tail;
};

struct nvme_softc {
	struct device		sc_dev;

	bus_space_tag_t		sc_iot;
	bus_space_handle_t	sc_ioh;
	bus_size_t		sc_ios;
	bus_dma_tag_t		sc_dmat;

	void			*sc_ih;

	u_int			sc_rdy_to;

	struct nvme_queue	*sc_admin_q;
	struct nvme_queue	*sc_q;
};

int	nvme_attach(struct nvme_softc *);
int	nvme_intr(void *);

#define DEVNAME(_sc) ((_sc)->sc_dev.dv_xname)
