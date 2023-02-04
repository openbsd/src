/*	$OpenBSD: ufshcivar.h,v 1.1 2023/02/04 23:11:59 mglocker Exp $ */

/*
 * Copyright (c) 2022 Marcus Glocker <mglocker@openbsd.org>
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

#define UFSHCI_READ_4(sc, x) \
    bus_space_read_4((sc)->sc_iot, (sc)->sc_ioh, (x))
#define UFSHCI_WRITE_4(sc, x, y) \
    bus_space_write_4((sc)->sc_iot, (sc)->sc_ioh, (x), (y))

#define UFSHCI_DMA_MAP(_udm)	((_udm)->udm_map)
#define UFSHCI_DMA_LEN(_udm)	((_udm)->udm_map->dm_segs[0].ds_len)
#define UFSHCI_DMA_DVA(_udm)	((uint64_t)(_udm)->udm_map->dm_segs[0].ds_addr)
#define UFSHCI_DMA_KVA(_udm)	((void *)(_udm)->udm_kva)
struct ufshci_dmamem {
	bus_dmamap_t		udm_map;
	bus_dma_segment_t	udm_seg;
	size_t			udm_size;
	caddr_t			udm_kva;
};

struct ufshci_softc;

/* SCSI */
struct ufshci_ccb {
	SIMPLEQ_ENTRY(ufshci_ccb)	 ccb_entry;
	bus_dmamap_t			 ccb_dmamap;
	void				*ccb_cookie;
	int				 ccb_slot;
	void				 (*ccb_done)(struct ufshci_softc *,
					     struct ufshci_ccb *);
};
SIMPLEQ_HEAD(ufshci_ccb_list, ufshci_ccb);

struct ufshci_softc {
	struct device		 sc_dev;

	bus_space_tag_t		 sc_iot;
	bus_space_handle_t	 sc_ioh;
	bus_size_t		 sc_ios;
	bus_dma_tag_t		 sc_dmat;

	uint8_t			 sc_intraggr_enabled;

	uint32_t		 sc_ver;
	uint32_t		 sc_cap;
	uint32_t		 sc_hcpid;
	uint32_t		 sc_hcmid;
	uint8_t			 sc_nutmrs;
	uint8_t			 sc_rtt;
	uint8_t			 sc_nutrs;
	uint8_t			 sc_taskid;

	struct ufshci_dmamem	*sc_dmamem_utmrd;
	struct ufshci_dmamem	*sc_dmamem_utrd;
	struct ufshci_dmamem	*sc_dmamem_ucd;

	/* SCSI */
	struct scsi_iopool	 sc_iopool;
	struct mutex		 sc_ccb_mtx;
	struct ufshci_ccb_list	 sc_ccb_list;
	struct ufshci_ccb	*sc_ccbs;
};

int	ufshci_intr(void *);
void	ufshci_attach_hook(struct device *);	/* XXX: Only for testing */
int	ufshci_attach(struct ufshci_softc *);
