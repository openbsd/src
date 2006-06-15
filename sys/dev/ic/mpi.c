/*	$OpenBSD: mpi.c,v 1.42 2006/06/15 06:45:53 marco Exp $ */

/*
 * Copyright (c) 2005, 2006 David Gwynne <dlg@openbsd.org>
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>

#include <machine/bus.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>

#include <dev/ic/mpireg.h>
#include <dev/ic/mpivar.h>

#ifdef MPI_DEBUG
uint32_t	mpi_debug = 0
/*		    | MPI_D_CMD */
/*		    | MPI_D_INTR */
/*		    | MPI_D_MISC */
/*		    | MPI_D_DMA */
/*		    | MPI_D_IOCTL */
/*		    | MPI_D_RW */
/*		    | MPI_D_MEM */
/*		    | MPI_D_CCB */
/*		    | MPI_D_PPR */
		    | MPI_D_RAID
		;
#endif

struct cfdriver mpi_cd = {
	NULL, "mpi", DV_DULL
};

int			mpi_scsi_cmd(struct scsi_xfer *);
void			mpi_scsi_cmd_done(struct mpi_ccb *);
void			mpi_minphys(struct buf *bp);
int			mpi_scsi_ioctl(struct scsi_link *, u_long, caddr_t,
			    int, struct proc *);

struct scsi_adapter mpi_switch = {
	mpi_scsi_cmd, mpi_minphys, NULL, NULL, mpi_scsi_ioctl
};

struct scsi_device mpi_dev = {
	NULL, NULL, NULL, NULL
};

struct mpi_dmamem	*mpi_dmamem_alloc(struct mpi_softc *, size_t);
void			mpi_dmamem_free(struct mpi_softc *,
			    struct mpi_dmamem *);
int			mpi_alloc_ccbs(struct mpi_softc *);
struct mpi_ccb		*mpi_get_ccb(struct mpi_softc *);
void			mpi_put_ccb(struct mpi_softc *, struct mpi_ccb *);
int			mpi_alloc_replies(struct mpi_softc *);
void			mpi_push_replies(struct mpi_softc *);

void			mpi_start(struct mpi_softc *, struct mpi_ccb *);
int			mpi_complete(struct mpi_softc *, struct mpi_ccb *, int);
int			mpi_poll(struct mpi_softc *, struct mpi_ccb *, int);

void			mpi_run_ppr(struct mpi_softc *);
int			mpi_ppr(struct mpi_softc *, struct scsi_link *,
			    int, int, int);

void			mpi_timeout_xs(void *);
int			mpi_load_xs(struct mpi_ccb *);

u_int32_t		mpi_read(struct mpi_softc *, bus_size_t);
void			mpi_write(struct mpi_softc *, bus_size_t, u_int32_t);
int			mpi_wait_eq(struct mpi_softc *, bus_size_t, u_int32_t,
			    u_int32_t);
int			mpi_wait_ne(struct mpi_softc *, bus_size_t, u_int32_t,
			    u_int32_t);

int			mpi_init(struct mpi_softc *);
int			mpi_reset_soft(struct mpi_softc *);
int			mpi_reset_hard(struct mpi_softc *);

int			mpi_handshake_send(struct mpi_softc *, void *, size_t);
int			mpi_handshake_recv_dword(struct mpi_softc *,
			    u_int32_t *);
int			mpi_handshake_recv(struct mpi_softc *, void *, size_t);

void			mpi_empty_done(struct mpi_ccb *);

int			mpi_iocinit(struct mpi_softc *);
int			mpi_iocfacts(struct mpi_softc *);
int			mpi_portfacts(struct mpi_softc *);
int			mpi_eventnotify(struct mpi_softc *);
void			mpi_eventnotify_done(struct mpi_ccb *);
int			mpi_portenable(struct mpi_softc *);
void			mpi_get_raid(struct mpi_softc *);

int			mpi_cfg_header(struct mpi_softc *, u_int8_t, u_int8_t,
			    u_int32_t, struct mpi_cfg_hdr *);
int			mpi_cfg_page(struct mpi_softc *, u_int32_t,
			    struct mpi_cfg_hdr *, int, void *, size_t);

#define DEVNAME(s)		((s)->sc_dev.dv_xname)

#define	dwordsof(s)		(sizeof(s) / sizeof(u_int32_t))
#define	sizeofa(s)		(sizeof(s) / sizeof((s)[0]))

#define mpi_read_db(s)		mpi_read((s), MPI_DOORBELL)
#define mpi_write_db(s, v)	mpi_write((s), MPI_DOORBELL, (v))
#define mpi_read_intr(s)	mpi_read((s), MPI_INTR_STATUS)
#define mpi_write_intr(s, v)	mpi_write((s), MPI_INTR_STATUS, (v))
#define mpi_pop_reply(s)	mpi_read((s), MPI_REPLY_QUEUE)
#define mpi_push_reply(s, v)	mpi_write((s), MPI_REPLY_QUEUE, (v)) 

#define mpi_wait_db_int(s)	mpi_wait_ne((s), MPI_INTR_STATUS, \
				    MPI_INTR_STATUS_DOORBELL, 0)
#define mpi_wait_db_ack(s)	mpi_wait_eq((s), MPI_INTR_STATUS, \
				    MPI_INTR_STATUS_IOCDOORBELL, 0)

int
mpi_attach(struct mpi_softc *sc)
{
	struct mpi_ccb			*ccb;

	printf("\n");

	/* disable interrupts */
	mpi_write(sc, MPI_INTR_MASK,
	    MPI_INTR_MASK_REPLY | MPI_INTR_MASK_DOORBELL);

	if (mpi_init(sc) != 0) {
		printf("%s: unable to initialise\n", DEVNAME(sc));
		return (1);
	}

	if (mpi_iocfacts(sc) != 0) {
		printf("%s: unable to get iocfacts\n", DEVNAME(sc));
		return (1);
	}

	if (mpi_alloc_ccbs(sc) != 0) {
		/* error already printed */
		return (1);
	}

	if (mpi_alloc_replies(sc) != 0) {
		printf("%s: unable to allocate reply space\n", DEVNAME(sc));
		goto free_ccbs;
	}

	if (mpi_iocinit(sc) != 0) {
		printf("%s: unable to send iocinit\n", DEVNAME(sc));
		goto free_ccbs;
	}

	/* spin until we're operational */
	if (mpi_wait_eq(sc, MPI_DOORBELL, MPI_DOORBELL_STATE,
	    MPI_DOORBELL_STATE_OPER) != 0) {
		printf("%s: state: 0x%08x\n", DEVNAME(sc),
		    mpi_read_db(sc) & MPI_DOORBELL_STATE);
		printf("%s: operational state timeout\n", DEVNAME(sc));
		goto free_ccbs;
	}

	mpi_push_replies(sc);

	if (mpi_portfacts(sc) != 0) {
		printf("%s: unable to get portfacts\n", DEVNAME(sc));
		goto free_replies;
	}

#if notyet
	if (mpi_eventnotify(sc) != 0) {
		printf("%s: unable to get portfacts\n", DEVNAME(sc));
		goto free_replies;
	}
#endif

	if (mpi_portenable(sc) != 0) {
		printf("%s: unable to enable port\n", DEVNAME(sc));
		goto free_replies;
	}

	/* we should be good to go now, attach scsibus */
	sc->sc_link.device = &mpi_dev;
	sc->sc_link.adapter = &mpi_switch;
	sc->sc_link.adapter_softc = sc;
	sc->sc_link.adapter_target = sc->sc_target;
	sc->sc_link.adapter_buswidth = sc->sc_buswidth;
	sc->sc_link.openings = sc->sc_maxcmds / sc->sc_buswidth;

	config_found(&sc->sc_dev, &sc->sc_link, scsiprint);

	/* get raid pages */
	mpi_get_raid(sc);

	/* do domain validation */
	if (sc->sc_porttype == MPI_PORTFACTS_PORTTYPE_SCSI)
		mpi_run_ppr(sc);

	/* XXX enable interrupts */
	mpi_write(sc, MPI_INTR_MASK, MPI_INTR_MASK_DOORBELL);

	return (0);

free_replies:
	bus_dmamap_sync(sc->sc_dmat, MPI_DMA_MAP(sc->sc_replies),
	    0, PAGE_SIZE, BUS_DMASYNC_POSTREAD);
	mpi_dmamem_free(sc, sc->sc_replies);
free_ccbs:
	while ((ccb = mpi_get_ccb(sc)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);
	mpi_dmamem_free(sc, sc->sc_requests);
	free(sc->sc_ccbs, M_DEVBUF);

	return(1);
}

void
mpi_run_ppr(struct mpi_softc *sc)
{
	struct mpi_cfg_hdr		hdr;
	struct mpi_cfg_spi_port_pg0	pg;
	int				period, offset;

	struct device			*dev;
	struct scsibus_softc		*ssc;
	struct scsi_link		*link;
	int				i, tries;

	if (mpi_cfg_header(sc, MPI_CONFIG_REQ_PAGE_TYPE_SCSI_SPI_PORT, 0, 0x0,
	    &hdr) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_run_ppr unable to fetch header\n",
		    DEVNAME(sc));
		return;
	}

	if (mpi_cfg_page(sc, 0x0, &hdr, 1, &pg, sizeof(pg)) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_run_ppr unable to fetch page\n",
		    DEVNAME(sc));
		return;
	}

	period = MPI_CFG_SPI_PORT_0_CAPABILITIES_MIN_PERIOD(
	    letoh32(pg.capabilities));
	offset = MPI_CFG_SPI_PORT_0_CAPABILITIES_MAX_OFFSET(
	    letoh32(pg.capabilities));

	TAILQ_FOREACH(dev, &alldevs, dv_list) {
		if (dev->dv_parent == &sc->sc_dev)
			break;
	}

	/* im too nice to punish idiots who don't configure scsibus */
	if (dev == NULL)
		return;

	ssc = (struct scsibus_softc *)dev;
	for (i = 0; i < sc->sc_link.adapter_buswidth; i++) {
		link = ssc->sc_link[i][0];
		tries = 0;

		if (link == NULL)
			continue;

		while (mpi_ppr(sc, link, period, offset, tries) == EAGAIN)
			tries++;
	}
}


int
mpi_ppr(struct mpi_softc *sc, struct scsi_link *link, int period, int offset,
    int try)
{
	struct mpi_cfg_hdr		hdr0, hdr1;
	struct mpi_cfg_spi_dev_pg0	pg0;
	struct mpi_cfg_spi_dev_pg1	pg1;
	struct scsi_inquiry_data	inqbuf;
	u_int32_t			address;
	u_int32_t			params;

	DNPRINTF(MPI_D_PPR, "%s: mpi_ppr sc: %p link: %p period: %d "
	    "offset: %d try: %d\n", DEVNAME(sc), sc, link, period, offset, try);

	if (try >= 3)
		return (EIO);

	if ((link->inqdata.device & SID_TYPE) == T_PROCESSOR)
		return (EIO);

	address = link->target;

	if (mpi_cfg_header(sc, MPI_CONFIG_REQ_PAGE_TYPE_SCSI_SPI_DEV, 0,
	    address, &hdr0) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr unable to fetch header 0\n",
		    DEVNAME(sc));
		return (EIO);
	}

	if (mpi_cfg_header(sc, MPI_CONFIG_REQ_PAGE_TYPE_SCSI_SPI_DEV, 1,
	    address, &hdr1) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr unable to fetch header 1\n",
		    DEVNAME(sc));
		return (EIO);
	}

	if (mpi_cfg_page(sc, address, &hdr0, 1, &pg0, sizeof(pg0)) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr unable to fetch page 0\n",
		    DEVNAME(sc));
		return (EIO);
	}

	DNPRINTF(MPI_D_PPR, "%s: mpi_ppr neg_params: 0x%08x info: 0x%08x\n",
	    DEVNAME(sc), letoh32(pg0.neg_params), letoh32(pg0.information));

	if (mpi_cfg_page(sc, address, &hdr1, 1, &pg1, sizeof(pg1)) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr unable to fetch page 1\n",
		    DEVNAME(sc));
		return (EIO);
	}

	DNPRINTF(MPI_D_PPR, "%s: mpi_ppr pg 1 req_params: 0x%08x conf: "
	    "0x%08x\n", DEVNAME(sc), letoh32(pg1.req_params),
	    letoh32(pg1.configuration));

	params = letoh32(pg1.req_params);
	params &= ~(MPI_CFG_SPI_DEV_1_REQPARAMS_WIDTH |
	    MPI_CFG_SPI_DEV_1_REQPARAMS_XFER_PERIOD_MASK |
	    MPI_CFG_SPI_DEV_1_REQPARAMS_XFER_OFFSET_MASK |
	    MPI_CFG_SPI_DEV_1_REQPARAMS_DUALXFERS |
	    MPI_CFG_SPI_DEV_1_REQPARAMS_QAS |
	    MPI_CFG_SPI_DEV_1_REQPARAMS_PACKETIZED);

	if (!(link->quirks & SDEV_NOSYNC)) {
		params |= MPI_CFG_SPI_DEV_1_REQPARAMS_WIDTH_WIDE;

		switch (try) {
		case 0: /* U320 */
			break;
		case 1: /* U160 */
			period = 0x09;
			break;
		case 2: /* U80 */
			period = 0x0a;
			break;
		}

		if (period < 0x09) {
			/* Ultra320: enable QAS & PACKETIZED */
			params |= MPI_CFG_SPI_DEV_1_REQPARAMS_QAS |
			    MPI_CFG_SPI_DEV_1_REQPARAMS_PACKETIZED;
		}
		if (period < 0xa) {
			/* >= Ultra160: enable dual xfers */
			params |= MPI_CFG_SPI_DEV_1_REQPARAMS_DUALXFERS;
		}
		pg1.req_params = htole32(params |
		    MPI_CFG_SPI_DEV_1_REQPARAMS_XFER_PERIOD(period) |
		    MPI_CFG_SPI_DEV_1_REQPARAMS_XFER_OFFSET(offset));
	}

	DNPRINTF(MPI_D_PPR, "%s: mpi_ppr pg 1 req_params: 0x%08x conf: "
	    "0x%08x period: %0x address: %d\n", DEVNAME(sc),
	    letoh32(pg1.req_params), letoh32(pg1.configuration), period,
	    address);

	if (mpi_cfg_page(sc, address, &hdr1, 0, &pg1, sizeof(pg1)) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr unable to write page 1\n",
		    DEVNAME(sc));
		return (EIO);
	}

	if (mpi_cfg_page(sc, address, &hdr1, 1, &pg1, sizeof(pg1)) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr unable to read page 1\n",
		    DEVNAME(sc));
		return (EIO);
	}

	DNPRINTF(MPI_D_PPR, "%s: mpi_ppr pg 1 readback req_params: 0x%08x "
	    "conf: 0x%08x\n", DEVNAME(sc), letoh32(pg1.req_params),
	    letoh32(pg1.configuration));

	if (scsi_inquire(link, &inqbuf, SCSI_POLL) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr unable to do inquiry against "
		    "target %d\n", DEVNAME(sc), link->target);
		return (EIO);
	}

	if (mpi_cfg_page(sc, address, &hdr0, 1, &pg0, sizeof(pg0)) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr unable to read page 0 after "
		    "inquiry\n", DEVNAME(sc));
		return (EIO);
	}

	DNPRINTF(MPI_D_PPR, "%s: mpi_ppr neg_params: 0x%08x info: 0x%08x "
	    "try: %d\n",
	    DEVNAME(sc), letoh32(pg0.neg_params), letoh32(pg0.information),
	    try);

	if (!(letoh32(pg0.information) & 0x07) && (try == 0)) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr U320 ppr rejected\n",
		    DEVNAME(sc));
		return (EAGAIN);
	}

	if ((((letoh32(pg0.information) >> 8) & 0xff) > 0x09) && (try == 1)) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr U160 ppr rejected\n",
		    DEVNAME(sc));
		return (EAGAIN);
	}

	if (letoh32(pg0.information) & 0x0e) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_ppr ppr rejected: %0x\n",
		    DEVNAME(sc), letoh32(pg0.information));
		return (EAGAIN);
	}

	params = letoh32(pg0.neg_params);
	DNPRINTF(MPI_D_PPR, "%s: mpi_ppr params %08x\n", DEVNAME(sc), params);

	switch(MPI_CFG_SPI_DEV_0_NEGPARAMS_XFER_PERIOD(params)) {
	case 0x08:
		period = 160;
		break;
	case 0x09:
		period = 80;
		break;
	case 0x0a:
		period = 40;
		break;
	case 0x0b:
		period = 20;
		break;
	case 0x0c:
		period = 10;
		break;
	default:
		period = 0;
		break;
	}

	printf("%s: target %d %s at %dMHz width %dbit offset %d "
	    "QAS %d DT %d IU %d\n", DEVNAME(sc), link->target,
	    period ? "Sync" : "Async", period,
	    (params & MPI_CFG_SPI_DEV_0_NEGPARAMS_WIDTH_WIDE) ? 16 : 8,
	    MPI_CFG_SPI_DEV_0_NEGPARAMS_XFER_OFFSET(params),
	    (params & MPI_CFG_SPI_DEV_0_NEGPARAMS_QAS) ? 1 : 0,
	    (params & MPI_CFG_SPI_DEV_0_NEGPARAMS_DUALXFERS) ? 1 : 0,
	    (params & MPI_CFG_SPI_DEV_0_NEGPARAMS_PACKETIZED) ? 1 : 0);

	return (0);
}

void
mpi_detach(struct mpi_softc *sc)
{

}

int
mpi_intr(void *arg)
{
	struct mpi_softc		*sc = arg;
	struct mpi_ccb			*ccb;
	struct mpi_msg_reply		*reply;
	u_int32_t			reply_dva;
	char				*reply_addr;
	u_int32_t			reg, id;
	int				rv = 0;

	while ((reg = mpi_pop_reply(sc)) != 0xffffffff) {

		DNPRINTF(MPI_D_INTR, "%s: mpi_intr reply_queue: 0x%08x\n",
		    DEVNAME(sc), reg);

		if (reg & MPI_REPLY_QUEUE_ADDRESS) {
			bus_dmamap_sync(sc->sc_dmat,
			    MPI_DMA_MAP(sc->sc_replies), 0, PAGE_SIZE,
			    BUS_DMASYNC_POSTREAD);

			reply_dva = (reg & MPI_REPLY_QUEUE_ADDRESS_MASK) << 1;

			reply_addr = MPI_DMA_KVA(sc->sc_replies);
			reply_addr += reply_dva -
			    (u_int32_t)MPI_DMA_DVA(sc->sc_replies);
			reply = (struct mpi_msg_reply *)reply_addr;

			id = letoh32(reply->msg_context);

			bus_dmamap_sync(sc->sc_dmat,
			    MPI_DMA_MAP(sc->sc_replies), 0, PAGE_SIZE,
			    BUS_DMASYNC_PREREAD);
		} else {
			switch (reg & MPI_REPLY_QUEUE_TYPE_MASK) {
			case MPI_REPLY_QUEUE_TYPE_INIT:
				id = reg & MPI_REPLY_QUEUE_CONTEXT;
				break;

			default:
				panic("%s: unsupported context reply\n",
				    DEVNAME(sc));
			}

			reply = NULL;
		}

		DNPRINTF(MPI_D_INTR, "%s: mpi_intr id: %d reply: %p\n",
		    DEVNAME(sc), id, reply);

		ccb = &sc->sc_ccbs[id];

		bus_dmamap_sync(sc->sc_dmat, MPI_DMA_MAP(sc->sc_requests),
		    ccb->ccb_offset, MPI_REQUEST_SIZE,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		ccb->ccb_state = MPI_CCB_READY;
		ccb->ccb_reply = reply;
		ccb->ccb_reply_dva = reply_dva;

		ccb->ccb_done(ccb);
		rv = 1;
	}

	return (rv);
}

struct mpi_dmamem *
mpi_dmamem_alloc(struct mpi_softc *sc, size_t size)
{
	struct mpi_dmamem		*mdm;
	int				nsegs;

	mdm = malloc(sizeof(struct mpi_dmamem), M_DEVBUF, M_NOWAIT);
	if (mdm == NULL)
		return (NULL);

	bzero(mdm, sizeof(struct mpi_dmamem));
	mdm->mdm_size = size;

	if (bus_dmamap_create(sc->sc_dmat, size, 1, size, 0,
	    BUS_DMA_NOWAIT | BUS_DMA_ALLOCNOW, &mdm->mdm_map) != 0)
		goto mdmfree;

	if (bus_dmamem_alloc(sc->sc_dmat, size, PAGE_SIZE, 0, &mdm->mdm_seg,
	    1, &nsegs, BUS_DMA_NOWAIT) != 0)
		goto destroy;

	if (bus_dmamem_map(sc->sc_dmat, &mdm->mdm_seg, nsegs, size,
	    &mdm->mdm_kva, BUS_DMA_NOWAIT) != 0)
		goto free;

	if (bus_dmamap_load(sc->sc_dmat, mdm->mdm_map, mdm->mdm_kva, size,
	    NULL, BUS_DMA_NOWAIT) != 0)
		goto unmap;

	bzero(mdm->mdm_kva, size);

	DNPRINTF(MPI_D_MEM, "%s: mpi_dmamem_alloc size: %d mdm: %#x "
	    "map: %#x nsegs: %d segs: %#x kva: %x\n",
	    DEVNAME(sc), size, mdm->mdm_map, nsegs, mdm->mdm_seg, mdm->mdm_kva);

	return (mdm);

unmap:
	bus_dmamem_unmap(sc->sc_dmat, mdm->mdm_kva, size);
free:
	bus_dmamem_free(sc->sc_dmat, &mdm->mdm_seg, 1);
destroy:
	bus_dmamap_destroy(sc->sc_dmat, mdm->mdm_map);
mdmfree:
	free(mdm, M_DEVBUF);

	return (NULL);
}

void
mpi_dmamem_free(struct mpi_softc *sc, struct mpi_dmamem *mdm)
{
	DNPRINTF(MPI_D_MEM, "%s: mpi_dmamem_free %#x\n", DEVNAME(sc), mdm);

	bus_dmamap_unload(sc->sc_dmat, mdm->mdm_map);
	bus_dmamem_unmap(sc->sc_dmat, mdm->mdm_kva, mdm->mdm_size);
	bus_dmamem_free(sc->sc_dmat, &mdm->mdm_seg, 1);
	bus_dmamap_destroy(sc->sc_dmat, mdm->mdm_map);
	free(mdm, M_DEVBUF);
}

int
mpi_alloc_ccbs(struct mpi_softc *sc)
{
	struct mpi_ccb			*ccb;
	u_int8_t			*cmd;
	int				i;

	TAILQ_INIT(&sc->sc_ccb_free);

	sc->sc_ccbs = malloc(sizeof(struct mpi_ccb) * sc->sc_maxcmds,
	    M_DEVBUF, M_WAITOK);
	if (sc->sc_ccbs == NULL) {
		printf("%s: unable to allocate ccbs\n", DEVNAME(sc));
		return (1);
	}
	bzero(sc->sc_ccbs, sizeof(struct mpi_ccb) * sc->sc_maxcmds);

	sc->sc_requests = mpi_dmamem_alloc(sc,
	    MPI_REQUEST_SIZE * sc->sc_maxcmds);
	if (sc->sc_requests == NULL) {
		printf("%s: unable to allocate ccb dmamem\n", DEVNAME(sc));
		goto free_ccbs;
	}
	cmd = MPI_DMA_KVA(sc->sc_requests);
	bzero(cmd, MPI_REQUEST_SIZE * sc->sc_maxcmds);

	for (i = 0; i < sc->sc_maxcmds; i++) {
		ccb = &sc->sc_ccbs[i];

		if (bus_dmamap_create(sc->sc_dmat, MAXPHYS,
		    sc->sc_max_sgl_len, MAXPHYS, 0, 0,
		    &ccb->ccb_dmamap) != 0) {
			printf("%s: unable to create dma map\n", DEVNAME(sc));
			goto free_maps;
		}

		ccb->ccb_sc = sc;
		ccb->ccb_id = i;
		ccb->ccb_offset = MPI_REQUEST_SIZE * i;

		ccb->ccb_cmd = &cmd[ccb->ccb_offset];
		ccb->ccb_cmd_dva = (u_int32_t)MPI_DMA_DVA(sc->sc_requests) +
		    ccb->ccb_offset;

		DNPRINTF(MPI_D_CCB, "%s: mpi_alloc_ccbs(%d) ccb: %#x map: %#x "
		    "sc: %#x id: %#x offs: %#x cmd: %#x dva: %#x\n",
		    DEVNAME(sc), i, ccb, ccb->ccb_dmamap, ccb->ccb_sc,
		    ccb->ccb_id, ccb->ccb_offset, ccb->ccb_cmd,
		    ccb->ccb_cmd_dva);

		mpi_put_ccb(sc, ccb);
	}

	return (0);

free_maps:
	while ((ccb = mpi_get_ccb(sc)) != NULL)
		bus_dmamap_destroy(sc->sc_dmat, ccb->ccb_dmamap);

	mpi_dmamem_free(sc, sc->sc_requests);
free_ccbs:
	free(sc->sc_ccbs, M_DEVBUF);

	return (1);
}

struct mpi_ccb *
mpi_get_ccb(struct mpi_softc *sc)
{
	struct mpi_ccb			*ccb;

	ccb = TAILQ_FIRST(&sc->sc_ccb_free);
	if (ccb == NULL) {
		DNPRINTF(MPI_D_CCB, "%s: mpi_get_ccb == NULL\n", DEVNAME(sc));
		return (NULL);
	}

	TAILQ_REMOVE(&sc->sc_ccb_free, ccb, ccb_link);

	ccb->ccb_state = MPI_CCB_READY;

	DNPRINTF(MPI_D_CCB, "%s: mpi_get_ccb %#x\n", DEVNAME(sc), ccb);

	return (ccb);
}

void
mpi_put_ccb(struct mpi_softc *sc, struct mpi_ccb *ccb)
{
	DNPRINTF(MPI_D_CCB, "%s: mpi_put_ccb %#x\n", DEVNAME(sc), ccb);

	ccb->ccb_state = MPI_CCB_FREE;
	ccb->ccb_xs = NULL;
	ccb->ccb_done = NULL;
	bzero(ccb->ccb_cmd, MPI_REQUEST_SIZE);
	TAILQ_INSERT_TAIL(&sc->sc_ccb_free, ccb, ccb_link);
}

int
mpi_alloc_replies(struct mpi_softc *sc)
{
	DNPRINTF(MPI_D_MISC, "%s: mpi_alloc_replies\n", DEVNAME(sc));

	sc->sc_replies = mpi_dmamem_alloc(sc, PAGE_SIZE);
	if (sc->sc_replies == NULL)
		return (1);

	return (0);
}

void
mpi_push_replies(struct mpi_softc *sc)
{
	paddr_t				reply;
	int				i;

	bus_dmamap_sync(sc->sc_dmat, MPI_DMA_MAP(sc->sc_replies),
	    0, PAGE_SIZE, BUS_DMASYNC_PREREAD);

	for (i = 0; i < PAGE_SIZE / MPI_REPLY_SIZE; i++) {
		reply = (u_int32_t)MPI_DMA_DVA(sc->sc_replies) +
		    MPI_REPLY_SIZE * i;
		DNPRINTF(MPI_D_MEM, "%s: mpi_push_replies %#x\n", DEVNAME(sc),
		    reply);
		mpi_push_reply(sc, reply);
	}
}

void
mpi_start(struct mpi_softc *sc, struct mpi_ccb *ccb)
{
	DNPRINTF(MPI_D_RW, "%s: mpi_start %#x\n", DEVNAME(sc),
	    ccb->ccb_cmd_dva);

	bus_dmamap_sync(sc->sc_dmat, MPI_DMA_MAP(sc->sc_requests),
	    ccb->ccb_offset, MPI_REQUEST_SIZE,
	    BUS_DMASYNC_PREREAD | BUS_DMASYNC_PREWRITE);

	ccb->ccb_state = MPI_CCB_QUEUED;
	mpi_write(sc, MPI_REQ_QUEUE, ccb->ccb_cmd_dva);
}

int
mpi_complete(struct mpi_softc *sc, struct mpi_ccb *nccb, int timeout)
{
	struct mpi_ccb			*ccb;
	struct mpi_msg_reply		*reply;
	u_int32_t			reply_dva;
	char				*reply_addr;
	u_int32_t			reg, id = 0xffffffff;

	DNPRINTF(MPI_D_INTR, "%s: mpi_complete timeout %d\n", DEVNAME(sc),
	    timeout);

	do {
		reg = mpi_pop_reply(sc);
		if (reg == 0xffffffff) {
			if (timeout-- == 0)
				return (1);

			delay(1000);
			continue;
		}

		DNPRINTF(MPI_D_INTR, "%s: mpi_complete reply_queue: 0x%08x\n",
		    DEVNAME(sc), reg);

		if (reg & MPI_REPLY_QUEUE_ADDRESS) {
			bus_dmamap_sync(sc->sc_dmat,
			    MPI_DMA_MAP(sc->sc_replies), 0, PAGE_SIZE,
			    BUS_DMASYNC_POSTREAD);

			reply_dva = (reg & MPI_REPLY_QUEUE_ADDRESS_MASK) << 1;

			reply_addr = MPI_DMA_KVA(sc->sc_replies);
			reply_addr += reply_dva -
			    (u_int32_t)MPI_DMA_DVA(sc->sc_replies);
			reply = (struct mpi_msg_reply *)reply_addr;

			id = letoh32(reply->msg_context);

			bus_dmamap_sync(sc->sc_dmat,
			    MPI_DMA_MAP(sc->sc_replies), 0, PAGE_SIZE,
			    BUS_DMASYNC_PREREAD);
		} else {
			switch (reg & MPI_REPLY_QUEUE_TYPE_MASK) {
			case MPI_REPLY_QUEUE_TYPE_INIT:
				id = reg & MPI_REPLY_QUEUE_CONTEXT;
				break;

			default:
				panic("%s: unsupported context reply\n",
				    DEVNAME(sc));
			}

			reply = NULL;
		}

		DNPRINTF(MPI_D_INTR, "%s: mpi_complete id: %d\n",
		    DEVNAME(sc), id);

		ccb = &sc->sc_ccbs[id];

		bus_dmamap_sync(sc->sc_dmat, MPI_DMA_MAP(sc->sc_requests),
		    ccb->ccb_offset, MPI_REQUEST_SIZE,
		    BUS_DMASYNC_POSTREAD | BUS_DMASYNC_POSTWRITE);
		ccb->ccb_state = MPI_CCB_READY;
		ccb->ccb_reply = reply;
		ccb->ccb_reply_dva = reply_dva;

		ccb->ccb_done(ccb);

	} while (nccb->ccb_id != id);

	return (0);
}

int
mpi_poll(struct mpi_softc *sc, struct mpi_ccb *ccb, int timeout)
{
	int				error;
	int				s;

	DNPRINTF(MPI_D_CMD, "%s: mpi_poll\n", DEVNAME(sc));

	s = splbio();
	mpi_start(sc, ccb);
	error = mpi_complete(sc, ccb, timeout);
	splx(s);

	return (error);
}

int
mpi_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link		*link = xs->sc_link;
	struct mpi_softc		*sc = link->adapter_softc;
	struct mpi_ccb			*ccb;
	struct mpi_ccb_bundle		*mcb;
	struct mpi_msg_scsi_io		*io;
	int				s;

	DNPRINTF(MPI_D_CMD, "%s: mpi_scsi_cmd\n", DEVNAME(sc));

	if (xs->cmdlen > MPI_CDB_LEN) {
		DNPRINTF(MPI_D_CMD, "%s: CBD too big %d\n",
		    DEVNAME(sc), xs->cmdlen);
		bzero(&xs->sense, sizeof(xs->sense));
		xs->sense.error_code = SSD_ERRCODE_VALID | 0x70;
		xs->sense.flags = SKEY_ILLEGAL_REQUEST;
		xs->sense.add_sense_code = 0x20;
		xs->error = XS_SENSE;
		scsi_done(xs);
		return (COMPLETE);
	}

	s = splbio();
	ccb = mpi_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return (COMPLETE);
	}
	DNPRINTF(MPI_D_CMD, "%s: ccb_id: %d xs->flags: 0x%x\n",
	    DEVNAME(sc), ccb->ccb_id, xs->flags);

	ccb->ccb_xs = xs;
	ccb->ccb_done = mpi_scsi_cmd_done;

	mcb = ccb->ccb_cmd;
	io = &mcb->mcb_io;

	io->function = MPI_FUNCTION_SCSI_IO_REQUEST;
	/*
	 * bus is always 0
	 * io->bus = htole16(sc->sc_bus);
	 */
	io->target_id = link->target;

	io->cdb_length = xs->cmdlen;
	io->sense_buf_len = sizeof(xs->sense);
	io->msg_flags = MPI_SCSIIO_SENSE_BUF_ADDR_WIDTH_64;

	io->msg_context = htole32(ccb->ccb_id);

	io->lun[0] = htobe16(link->lun);

	switch (xs->flags & (SCSI_DATA_IN | SCSI_DATA_OUT)) {
	case SCSI_DATA_IN:
		io->control = htole32(MPI_SCSIIO_DATA_DIR_READ);
		break;
	case SCSI_DATA_OUT:
		io->control = htole32(MPI_SCSIIO_DATA_DIR_WRITE);
		break;
	default:
		io->control = htole32(MPI_SCSIIO_DATA_DIR_NONE);
		break;
	}

	bcopy(xs->cmd, io->cdb, xs->cmdlen);

	io->data_length = htole32(xs->datalen);

	io->sense_buf_low_addr = htole32(ccb->ccb_cmd_dva +
	    ((u_int8_t *)&mcb->mcb_sense - (u_int8_t *)mcb));

	if (mpi_load_xs(ccb) != 0) {
		s = splbio();
		mpi_put_ccb(sc, ccb);
		splx(s);
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return (COMPLETE);
	}

	timeout_set(&xs->stimeout, mpi_timeout_xs, ccb);

	if (xs->flags & SCSI_POLL) {
		if (mpi_poll(sc, ccb, xs->timeout) != 0)
			xs->error = XS_DRIVER_STUFFUP;
		return (COMPLETE);
	}

	mpi_start(sc, ccb);
	return (SUCCESSFULLY_QUEUED);
}

void
mpi_scsi_cmd_done(struct mpi_ccb *ccb)
{
	struct mpi_softc		*sc = ccb->ccb_sc;
	struct scsi_xfer		*xs = ccb->ccb_xs;
	struct mpi_ccb_bundle		*mcb = ccb->ccb_cmd;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;
	struct mpi_msg_scsi_io_error	*sie = ccb->ccb_reply;

	if (xs->datalen != 0) {
		bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
		    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_POSTREAD :
		    BUS_DMASYNC_POSTWRITE);

		bus_dmamap_unload(sc->sc_dmat, dmap);
	}

	/* timeout_del */
	xs->error = XS_NOERROR;
	xs->resid = 0;
	xs->flags |= ITSDONE;

	if (sie == NULL) {
		/* no scsi error, we're ok so drop out early */
		xs->status = SCSI_OK;
		mpi_put_ccb(sc, ccb);
		scsi_done(xs);
		return;
	}

	xs->status = sie->scsi_status;
	switch (letoh16(sie->ioc_status)) {
	case MPI_IOCSTATUS_SCSI_DATA_OVERRUN:
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_DATA_UNDERRUN:
		/*
		 * Yikes!  Tagged queue full comes through this path!
		 *
		 * So we'll change it to a status error and anything
		 * that returns status should probably be a status
		 * error as well.
		 */
		xs->resid = xs->datalen - letoh32(sie->transfer_count);
		if (sie->scsi_state & MPI_SCSIIO_ERR_STATE_NO_SCSI_STATUS) {
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		/* FALLTHROUGH */
	case MPI_IOCSTATUS_SUCCESS:
	case MPI_IOCSTATUS_SCSI_RECOVERED_ERROR:
		switch (xs->status) {
		case SCSI_OK:
			xs->resid = 0;
			break;

		case SCSI_CHECK:
			xs->error = XS_SENSE;
			break;

		case SCSI_BUSY:
			xs->error = XS_BUSY;
			break;

		case SCSI_QUEUE_FULL:
			xs->error = XS_TIMEOUT;
			xs->retries++;
			break;
		default:
			printf("%s: invalid status code %d\n", xs->status);
			xs->error = XS_DRIVER_STUFFUP;
			break;
		}
		break;

	case MPI_IOCSTATUS_BUSY:
	case MPI_IOCSTATUS_INSUFFICIENT_RESOURCES:
		xs->error = XS_BUSY;
		break;

	case MPI_IOCSTATUS_SCSI_INVALID_BUS:
	case MPI_IOCSTATUS_SCSI_INVALID_TARGETID:
	case MPI_IOCSTATUS_SCSI_DEVICE_NOT_THERE:
		xs->error = XS_SELTIMEOUT;
		break;

	case MPI_IOCSTATUS_SCSI_RESIDUAL_MISMATCH:
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_TASK_TERMINATED:
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_TASK_MGMT_FAILED:
		/* XXX */
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_IOC_TERMINATED:
		/* XXX */
		xs->error = XS_DRIVER_STUFFUP;
		break;

	case MPI_IOCSTATUS_SCSI_EXT_TERMINATED:
		/* XXX This is a bus-reset */
		xs->error = XS_DRIVER_STUFFUP;
		break;

	default:
		/* XXX unrecognized HBA error */
		xs->error = XS_DRIVER_STUFFUP;
		break;
	}

	if (sie->scsi_state & MPI_SCSIIO_ERR_STATE_AUTOSENSE_VALID)
		bcopy(&mcb->mcb_sense, &xs->sense, sizeof(xs->sense));
	else if (sie->scsi_state & MPI_SCSIIO_ERR_STATE_AUTOSENSE_FAILED) {
		/* This will cause the scsi layer to issue a REQUEST SENSE */
		if (xs->status == SCSI_CHECK)
			xs->error = XS_BUSY;
	}

	if (xs->error != XS_NOERROR && !cold) {
		printf("%s:  xs cmd: 0x%02x len: %d error: 0x%02x "
		    "flags 0x%x\n", DEVNAME(sc), xs->cmd->opcode, xs->datalen,
		    xs->error, xs->flags);
		printf("%s:  target_id: %d bus: %d msg_length: %d "
		    "function: 0x%02x\n", DEVNAME(sc), sie->target_id,
		    sie->bus, sie->msg_length, sie->function);
		printf("%s:  cdb_length: %d sense_buf_length: %d "
		    "msg_flags: 0x%02x\n", DEVNAME(sc), sie->cdb_length,
		    sie->bus, sie->msg_flags);
		printf("%s:  msg_context: 0x%08x\n", DEVNAME(sc),
		    letoh32(sie->msg_context));
		printf("%s:  scsi_status: 0x%02x scsi_state: 0x%02x "
		    "ioc_status: 0x%04x\n", DEVNAME(sc), sie->scsi_status,
		    sie->scsi_state, letoh16(sie->ioc_status));
		printf("%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
		    letoh32(sie->ioc_loginfo));
		printf("%s:  transfer_count: %d\n", DEVNAME(sc),
		    letoh32(sie->transfer_count));
		printf("%s:  sense_count: %d\n", DEVNAME(sc),
		    letoh32(sie->sense_count));
		printf("%s:  response_info: 0x%08x\n", DEVNAME(sc),
		    letoh32(sie->response_info));
		printf("%s:  tag: 0x%04x\n", DEVNAME(sc),
		    letoh16(sie->tag));
		printf("%s:  xs error: 0x%02x xs status: %d\n", DEVNAME(sc),
		    xs->error, xs->status);
	}

	mpi_push_reply(sc, ccb->ccb_reply_dva);
	mpi_put_ccb(sc, ccb);
	scsi_done(xs);
}

void
mpi_timeout_xs(void *arg)
{
	/* XXX */
}

int
mpi_load_xs(struct mpi_ccb *ccb)
{
	struct mpi_softc		*sc = ccb->ccb_sc;
	struct scsi_xfer		*xs = ccb->ccb_xs;
	struct mpi_ccb_bundle		*mcb = ccb->ccb_cmd;
	struct mpi_msg_scsi_io		*io = &mcb->mcb_io;
	struct mpi_sge			*sge, *nsge = &mcb->mcb_sgl[0];
	struct mpi_sge			*ce = NULL, *nce;
	u_int64_t			ce_dva;
	bus_dmamap_t			dmap = ccb->ccb_dmamap;
	u_int32_t			addr, flags;
	int				i, error;

	if (xs->datalen == 0) {
		nsge->sg_hdr = htole32(MPI_SGE_FL_TYPE_SIMPLE |
		    MPI_SGE_FL_LAST | MPI_SGE_FL_EOB | MPI_SGE_FL_EOL);
		return (0);
	}

	error = bus_dmamap_load(sc->sc_dmat, dmap,
	    xs->data, xs->datalen, NULL,
	    (xs->flags & SCSI_NOSLEEP) ? BUS_DMA_NOWAIT : BUS_DMA_WAITOK);
	if (error) {
		printf("%s: error %d loading dmamap\n", DEVNAME(sc), error);
		return (1);
	}

	flags = MPI_SGE_FL_TYPE_SIMPLE | MPI_SGE_FL_SIZE_64;
	if (xs->flags & SCSI_DATA_OUT)
		flags |= MPI_SGE_FL_DIR_OUT;

	if (dmap->dm_nsegs > sc->sc_first_sgl_len) {
		ce = &mcb->mcb_sgl[sc->sc_first_sgl_len - 1];
		io->chain_offset = ((u_int8_t *)ce - (u_int8_t *)io) / 4;
	}

	for (i = 0; i < dmap->dm_nsegs; i++) {

		if (nsge == ce) {
			nsge++;
			sge->sg_hdr |= htole32(MPI_SGE_FL_LAST);

			DNPRINTF(MPI_D_DMA, "%s:   - 0x%08x 0x%08x 0x%08x\n",
			    DEVNAME(sc), sge->sg_hdr,
			    sge->sg_hi_addr, sge->sg_lo_addr);

			if ((dmap->dm_nsegs - i) > sc->sc_chain_len) {
				nce = &nsge[sc->sc_chain_len - 1];
				addr = ((u_int8_t *)nce - (u_int8_t *)nsge) / 4;
				addr = addr << 16 |
				    sizeof(struct mpi_sge) * sc->sc_chain_len;
			} else {
				nce = NULL;
				addr = sizeof(struct mpi_sge) *
				    (dmap->dm_nsegs - i);
			}

			ce->sg_hdr = htole32(MPI_SGE_FL_TYPE_CHAIN |
			    MPI_SGE_FL_SIZE_64 | addr);

			ce_dva = ccb->ccb_cmd_dva +
			    ((u_int8_t *)nsge - (u_int8_t *)mcb);

			addr = (u_int32_t)(ce_dva >> 32);
			ce->sg_hi_addr = htole32(addr);
			addr = (u_int32_t)ce_dva;
			ce->sg_lo_addr = htole32(addr);

			DNPRINTF(MPI_D_DMA, "%s:  ce: 0x%08x 0x%08x 0x%08x\n",
			    DEVNAME(sc), ce->sg_hdr, ce->sg_hi_addr,
			    ce->sg_lo_addr);

			ce = nce;
		}

		DNPRINTF(MPI_D_DMA, "%s:  %d: %d 0x%016llx\n", DEVNAME(sc),
		    i, dmap->dm_segs[i].ds_len,
		    (u_int64_t)dmap->dm_segs[i].ds_addr);

		sge = nsge;

		sge->sg_hdr = htole32(flags | dmap->dm_segs[i].ds_len);
		addr = (u_int32_t)((u_int64_t)dmap->dm_segs[i].ds_addr >> 32);
		sge->sg_hi_addr = htole32(addr);
		addr = (u_int32_t)dmap->dm_segs[i].ds_addr;
		sge->sg_lo_addr = htole32(addr);

		DNPRINTF(MPI_D_DMA, "%s:  %d: 0x%08x 0x%08x 0x%08x\n",
		    DEVNAME(sc), i, sge->sg_hdr, sge->sg_hi_addr,
		    sge->sg_lo_addr);

		nsge = sge + 1;
	}

	/* terminate list */
	sge->sg_hdr |= htole32(MPI_SGE_FL_LAST | MPI_SGE_FL_EOB |
	    MPI_SGE_FL_EOL);

	bus_dmamap_sync(sc->sc_dmat, dmap, 0, dmap->dm_mapsize,
	    (xs->flags & SCSI_DATA_IN) ? BUS_DMASYNC_PREREAD :
	    BUS_DMASYNC_PREWRITE);

	return (0);
}

void
mpi_minphys(struct buf *bp)
{
	/* XXX */
	if (bp->b_bcount > MAXPHYS)
		bp->b_bcount = MAXPHYS;
	minphys(bp);
}

int
mpi_scsi_ioctl(struct scsi_link *a, u_long b, caddr_t c, int d, struct proc *e)
{
	return (0);
}

u_int32_t
mpi_read(struct mpi_softc *sc, bus_size_t r)
{
	u_int32_t			rv;

	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_READ);
	rv = bus_space_read_4(sc->sc_iot, sc->sc_ioh, r);

	DNPRINTF(MPI_D_RW, "%s: mpi_read %#x %#x\n", DEVNAME(sc), r, rv);

	return (rv);
}

void
mpi_write(struct mpi_softc *sc, bus_size_t r, u_int32_t v)
{
	DNPRINTF(MPI_D_RW, "%s: mpi_write %#x %#x\n", DEVNAME(sc), r, v);

	bus_space_write_4(sc->sc_iot, sc->sc_ioh, r, v);
	bus_space_barrier(sc->sc_iot, sc->sc_ioh, r, 4,
	    BUS_SPACE_BARRIER_WRITE);
}

int
mpi_wait_eq(struct mpi_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	DNPRINTF(MPI_D_RW, "%s: mpi_wait_eq %#x %#x %#x\n", DEVNAME(sc), r,
	    mask, target);

	for (i = 0; i < 10000; i++) {
		if ((mpi_read(sc, r) & mask) == target)
			return (0);
		delay(1000);
	}

	return (1);
}

int
mpi_wait_ne(struct mpi_softc *sc, bus_size_t r, u_int32_t mask,
    u_int32_t target)
{
	int				i;

	DNPRINTF(MPI_D_RW, "%s: mpi_wait_ne %#x %#x %#x\n", DEVNAME(sc), r,
	    mask, target);

	for (i = 0; i < 10000; i++) {
		if ((mpi_read(sc, r) & mask) != target)
			return (0);
		delay(1000);
	}

	return (1);
}

int
mpi_init(struct mpi_softc *sc)
{
	u_int32_t			db;
	int				i;

	/* spin until the IOC leaves the RESET state */
	if (mpi_wait_ne(sc, MPI_DOORBELL, MPI_DOORBELL_STATE,
	    MPI_DOORBELL_STATE_RESET) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_init timeout waiting to leave "
		    "reset state\n", DEVNAME(sc));
		return (1);
	}

	/* check current ownership */
	db = mpi_read_db(sc);
	if ((db & MPI_DOORBELL_WHOINIT) == MPI_DOORBELL_WHOINIT_PCIPEER) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_init initialised by pci peer\n",
		    DEVNAME(sc));
		return (0);
	}

	for (i = 0; i < 5; i++) {
		switch (db & MPI_DOORBELL_STATE) {
		case MPI_DOORBELL_STATE_READY:
			DNPRINTF(MPI_D_MISC, "%s: mpi_init ioc is ready\n",
			    DEVNAME(sc));
			return (0);

		case MPI_DOORBELL_STATE_OPER:
		case MPI_DOORBELL_STATE_FAULT:
			DNPRINTF(MPI_D_MISC, "%s: mpi_init ioc is being "
			    "reset\n" , DEVNAME(sc));
			if (mpi_reset_soft(sc) != 0)
				mpi_reset_hard(sc);
			break;

		case MPI_DOORBELL_STATE_RESET:
			DNPRINTF(MPI_D_MISC, "%s: mpi_init waiting to come "
			    "out of reset\n", DEVNAME(sc));
			if (mpi_wait_ne(sc, MPI_DOORBELL, MPI_DOORBELL_STATE,
			    MPI_DOORBELL_STATE_RESET) != 0)
				return (1);
			break;
		}
		db = mpi_read_db(sc);
	}

	return (1);
}

int
mpi_reset_soft(struct mpi_softc *sc)
{
	DNPRINTF(MPI_D_MISC, "%s: mpi_reset_soft\n", DEVNAME(sc));

	if (mpi_read_db(sc) & MPI_DOORBELL_INUSE)
		return (1);

	mpi_write_db(sc,
	    MPI_DOORBELL_FUNCTION(MPI_FUNCTION_IOC_MESSAGE_UNIT_RESET));
	if (mpi_wait_eq(sc, MPI_INTR_STATUS,
	    MPI_INTR_STATUS_IOCDOORBELL, 0) != 0)
		return (1);

	if (mpi_wait_eq(sc, MPI_DOORBELL, MPI_DOORBELL_STATE,
	    MPI_DOORBELL_STATE_READY) != 0)
		return (1);

	return (0);
}

int
mpi_reset_hard(struct mpi_softc *sc)
{
	DNPRINTF(MPI_D_MISC, "%s: mpi_reset_hard\n", DEVNAME(sc));

	/* enable diagnostic register */
	mpi_write(sc, MPI_WRITESEQ, 0xff);
	mpi_write(sc, MPI_WRITESEQ, MPI_WRITESEQ_1);
	mpi_write(sc, MPI_WRITESEQ, MPI_WRITESEQ_2);
	mpi_write(sc, MPI_WRITESEQ, MPI_WRITESEQ_3);
	mpi_write(sc, MPI_WRITESEQ, MPI_WRITESEQ_4);
	mpi_write(sc, MPI_WRITESEQ, MPI_WRITESEQ_5);

	/* reset ioc */
	mpi_write(sc, MPI_HOSTDIAG, MPI_HOSTDIAG_RESET_ADAPTER);

	delay(10000);

	/* disable diagnostic register */
	mpi_write(sc, MPI_WRITESEQ, 0xff);

	/* restore pci bits? */

	/* firmware bits? */
	return (0);
}

int
mpi_handshake_send(struct mpi_softc *sc, void *buf, size_t dwords)
{
	u_int32_t				*query = buf;
	int					i;

	/* make sure the doorbell is not in use. */
	if (mpi_read_db(sc) & MPI_DOORBELL_INUSE)
		return (1);

	/* clear pending doorbell interrupts */
	if (mpi_read_intr(sc) & MPI_INTR_STATUS_DOORBELL)
		mpi_write_intr(sc, 0);

	/*
	 * first write the doorbell with the handshake function and the
	 * dword count.
	 */
	mpi_write_db(sc, MPI_DOORBELL_FUNCTION(MPI_FUNCTION_HANDSHAKE) |
	    MPI_DOORBELL_DWORDS(dwords));

	/*
	 * the doorbell used bit will be set because a doorbell function has
	 * started. Wait for the interrupt and then ack it.
	 */
	if (mpi_wait_db_int(sc) != 0)
		return (1);
	mpi_write_intr(sc, 0);

	/* poll for the acknowledgement. */
	if (mpi_wait_db_ack(sc) != 0)
		return (1);

	/* write the query through the doorbell. */
	for (i = 0; i < dwords; i++) {
		mpi_write_db(sc, htole32(query[i]));
		if (mpi_wait_db_ack(sc) != 0)
			return (1);
	}

	return (0);
}

int
mpi_handshake_recv_dword(struct mpi_softc *sc, u_int32_t *dword)
{
	u_int16_t				*words = (u_int16_t *)dword;
	int					i;

	for (i = 0; i < 2; i++) {
		if (mpi_wait_db_int(sc) != 0)
			return (1);
		words[i] = letoh16(mpi_read_db(sc) & MPI_DOORBELL_DATA_MASK);
		mpi_write_intr(sc, 0);
	}

	return (0);
}

int
mpi_handshake_recv(struct mpi_softc *sc, void *buf, size_t dwords)
{
	struct mpi_msg_reply			*reply = buf;
	u_int32_t				*dbuf = buf, dummy;
	int					i;

	/* get the first dword so we can read the length out of the header. */
	if (mpi_handshake_recv_dword(sc, &dbuf[0]) != 0)
		return (1);

	DNPRINTF(MPI_D_CMD, "%s: mpi_handshake_recv dwords: %d reply: %d\n",
	    DEVNAME(sc), dwords, reply->msg_length);

	/*
	 * the total length, in dwords, is in the message length field of the
	 * reply header.
	 */
	for (i = 1; i < MIN(dwords, reply->msg_length); i++) {
		if (mpi_handshake_recv_dword(sc, &dbuf[i]) != 0)
			return (1);
	}

	/* if there's extra stuff to come off the ioc, discard it */
	while (i++ < reply->msg_length) {
		if (mpi_handshake_recv_dword(sc, &dummy) != 0)
			return (1);
		DNPRINTF(MPI_D_CMD, "%s: mpi_handshake_recv dummy read: "
		    "0x%08x\n", DEVNAME(sc), dummy);
	}

	/* wait for the doorbell used bit to be reset and clear the intr */
	if (mpi_wait_db_int(sc) != 0)
		return (1);
	mpi_write_intr(sc, 0);

	return (0);
}

void
mpi_empty_done(struct mpi_ccb *ccb)
{
	/* nothing to do */
}

int
mpi_iocfacts(struct mpi_softc *sc)
{
	struct mpi_msg_iocfacts_request		ifq;
	struct mpi_msg_iocfacts_reply		ifp;

	DNPRINTF(MPI_D_MISC, "%s: mpi_iocfacts\n", DEVNAME(sc));

	bzero(&ifq, sizeof(ifq));
	bzero(&ifp, sizeof(ifp));

	ifq.function = MPI_FUNCTION_IOC_FACTS;
	ifq.chain_offset = 0;
	ifq.msg_flags = 0;
	ifq.msg_context = htole32(0xdeadbeef);

	if (mpi_handshake_send(sc, &ifq, dwordsof(ifq)) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_iocfacts send failed\n",
		    DEVNAME(sc));
		return (1);
	}

	if (mpi_handshake_recv(sc, &ifp, dwordsof(ifp)) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_iocfacts recv failed\n",
		    DEVNAME(sc));
		return (1);
	}

	DNPRINTF(MPI_D_MISC, "%s:  func: 0x%02x len: %d msgver: %d.%d\n",
	    DEVNAME(sc), ifp.function, ifp.msg_length,
	    ifp.msg_version_maj, ifp.msg_version_min);

	DNPRINTF(MPI_D_MISC, "%s:  msgflags: 0x%02x iocnumber: 0x%02x "
	    "hdrver: %d.%d\n", DEVNAME(sc), ifp.msg_flags,
	    ifp.ioc_number, ifp.header_version_maj,
	    ifp.header_version_min);

	DNPRINTF(MPI_D_MISC, "%s:  message context: 0x%08x\n", DEVNAME(sc),
	    letoh32(ifp.msg_context));

	DNPRINTF(MPI_D_MISC, "%s:  iocstatus: 0x%04x ioexcept: 0x%04x\n",
	    DEVNAME(sc), letoh16(ifp.ioc_status),
	    letoh16(ifp.ioc_exceptions));

	DNPRINTF(MPI_D_MISC, "%s:  iocloginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(ifp.ioc_loginfo));

	DNPRINTF(MPI_D_MISC, "%s:  flags: 0x%02x blocksize: %d whoinit: 0x%02x "
	    "maxchdepth: %d\n", DEVNAME(sc), ifp.flags,
	    ifp.block_size, ifp.whoinit, ifp.max_chain_depth);

	DNPRINTF(MPI_D_MISC, "%s:  reqfrsize: %d replyqdepth: %d\n",
	    DEVNAME(sc), letoh16(ifp.request_frame_size),
	    letoh16(ifp.reply_queue_depth));

	DNPRINTF(MPI_D_MISC, "%s:  productid: 0x%04x\n", DEVNAME(sc),
	    letoh16(ifp.product_id));

	DNPRINTF(MPI_D_MISC, "%s:  hostmfahiaddr: 0x%08x\n", DEVNAME(sc),
	    letoh32(ifp.current_host_mfa_hi_addr));

	DNPRINTF(MPI_D_MISC, "%s:  event_state: 0x%02x number_of_ports: %d "
	    "global_credits: %d\n",
	    DEVNAME(sc), ifp.event_state, ifp.number_of_ports,
	    letoh16(ifp.global_credits));

	DNPRINTF(MPI_D_MISC, "%s:  sensebufhiaddr: 0x%08x\n", DEVNAME(sc),
	    letoh32(ifp.current_sense_buffer_hi_addr));

	DNPRINTF(MPI_D_MISC, "%s:  maxbus: %d maxdev: %d replyfrsize: %d\n",
	    DEVNAME(sc), ifp.max_buses, ifp.max_devices,
	    letoh16(ifp.current_reply_frame_size));

	DNPRINTF(MPI_D_MISC, "%s:  fw_image_size: %d\n", DEVNAME(sc),
	    letoh32(ifp.fw_image_size));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_capabilities: 0x%08x\n", DEVNAME(sc),
	    letoh32(ifp.ioc_capabilities));

	DNPRINTF(MPI_D_MISC, "%s:  fw_version: %d.%d fw_version_unit: 0x%02x "
	    "fw_version_dev: 0x%02x\n", DEVNAME(sc),
	    ifp.fw_version_maj, ifp.fw_version_min,
	    ifp.fw_version_unit, ifp.fw_version_dev);

	DNPRINTF(MPI_D_MISC, "%s:  hi_priority_queue_depth: 0x%04x\n",
	    DEVNAME(sc), letoh16(ifp.hi_priority_queue_depth));

	DNPRINTF(MPI_D_MISC, "%s:  host_page_buffer_sge: hdr: 0x%08x "
	    "addr 0x%08x %08x\n", DEVNAME(sc),
	    letoh32(ifp.host_page_buffer_sge.sg_hdr),
	    letoh32(ifp.host_page_buffer_sge.sg_hi_addr),
	    letoh32(ifp.host_page_buffer_sge.sg_lo_addr));

	sc->sc_maxcmds = letoh16(ifp.global_credits);
	sc->sc_buswidth = (ifp.max_devices == 0) ? 256 : ifp.max_devices;
	sc->sc_maxchdepth = ifp.max_chain_depth;

	/*
	 * you can fit sg elements on the end of the io cmd if they fit in the
	 * request frame size.
	 */
	sc->sc_first_sgl_len = ((letoh16(ifp.request_frame_size) * 4) - 
	    sizeof(struct mpi_msg_scsi_io)) / sizeof(struct mpi_sge);
	DNPRINTF(MPI_D_MISC, "%s:   first sgl len: %d\n", DEVNAME(sc),
	    sc->sc_first_sgl_len);

	sc->sc_chain_len = (letoh16(ifp.request_frame_size) * 4) /
	    sizeof(struct mpi_sge);
	DNPRINTF(MPI_D_MISC, "%s:   chain len: %d\n", DEVNAME(sc),
	    sc->sc_chain_len);

	/* the sgl tailing the io cmd loses an entry to the chain element. */
	sc->sc_max_sgl_len = MPI_MAX_SGL - 1;
	/* the sgl chains lose an entry for each chain element */
	sc->sc_max_sgl_len -= (MPI_MAX_SGL - sc->sc_first_sgl_len) /
	    sc->sc_chain_len;
	DNPRINTF(MPI_D_MISC, "%s:   max sgl len: %d\n", DEVNAME(sc),
	    sc->sc_max_sgl_len);

	/* XXX we're ignoring the max chain depth */

	return (0);
}

int
mpi_iocinit(struct mpi_softc *sc)
{
	struct mpi_msg_iocinit_request		iiq;
	struct mpi_msg_iocinit_reply		iip;
	u_int32_t				hi_addr;

	DNPRINTF(MPI_D_MISC, "%s: mpi_iocinit\n", DEVNAME(sc));

	bzero(&iiq, sizeof(iiq));
	bzero(&iip, sizeof(iip));

	iiq.function = MPI_FUNCTION_IOC_INIT;
	iiq.whoinit = MPI_WHOINIT_HOST_DRIVER;

	iiq.max_devices = (sc->sc_buswidth == 256) ? 0 : sc->sc_buswidth;
	iiq.max_buses = 1;

	iiq.msg_context = htole32(0xd00fd00f);

	iiq.reply_frame_size = htole16(MPI_REPLY_SIZE);

	hi_addr = (u_int32_t)((u_int64_t)MPI_DMA_DVA(sc->sc_requests) >> 32);
	iiq.host_mfa_hi_addr = htole32(hi_addr);
	iiq.sense_buffer_hi_addr = htole32(hi_addr);

	hi_addr = (u_int32_t)((u_int64_t)MPI_DMA_DVA(sc->sc_replies) >> 32);
	iiq.reply_fifo_host_signalling_addr = htole32(hi_addr);

	iiq.msg_version_maj = 0x01;
	iiq.msg_version_min = 0x02;

	iiq.hdr_version_unit = 0x0d;
	iiq.hdr_version_dev = 0x00;

	if (mpi_handshake_send(sc, &iiq, dwordsof(iiq)) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_iocinit send failed\n",
		    DEVNAME(sc));
		return (1);
	}

	if (mpi_handshake_recv(sc, &iip, dwordsof(iip)) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_iocinit recv failed\n",
		    DEVNAME(sc));
		return (1);
	}

	DNPRINTF(MPI_D_MISC, "%s:  function: 0x%02x msg_length: %d "
	    "whoinit: 0x%02x\n", DEVNAME(sc), iip.function,
	    iip.msg_length, iip.whoinit);

	DNPRINTF(MPI_D_MISC, "%s:  msg_flags: 0x%02x max_buses: %d "
	    "max_devices: %d flags: 0x%02x\n", DEVNAME(sc), iip.msg_flags,
	    iip.max_buses, iip.max_devices, iip.flags);

	DNPRINTF(MPI_D_MISC, "%s:  msg_context: 0x%08x\n", DEVNAME(sc),
	    letoh32(iip.msg_context));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(iip.ioc_status));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(iip.ioc_loginfo));

	return (0);
}

int
mpi_portfacts(struct mpi_softc *sc)
{
	struct mpi_ccb				*ccb;
	struct mpi_msg_portfacts_request	*pfq;
	volatile struct mpi_msg_portfacts_reply	*pfp;
	int					s, rv = 1;

	DNPRINTF(MPI_D_MISC, "%s: mpi_portfacts\n", DEVNAME(sc));

	s = splbio();
	ccb = mpi_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_portfacts ccb_get\n",
		    DEVNAME(sc));
		return (rv);
	}

	ccb->ccb_done = mpi_empty_done;
	pfq = ccb->ccb_cmd;

	pfq->function = MPI_FUNCTION_PORT_FACTS;
	pfq->chain_offset = 0;
	pfq->msg_flags = 0;
	pfq->port_number = 0;
	pfq->msg_context = htole32(ccb->ccb_id);

	if (mpi_poll(sc, ccb, 50000) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_portfacts poll\n", DEVNAME(sc));
		goto err;
	}

	pfp = ccb->ccb_reply;
	if (pfp == NULL) {
		DNPRINTF(MPI_D_MISC, "%s: empty portfacts reply\n",
		    DEVNAME(sc));
		goto err;
	}

	DNPRINTF(MPI_D_MISC, "%s:  function: 0x%02x msg_length: %d\n",
	    DEVNAME(sc), pfp->function, pfp->msg_length);

	DNPRINTF(MPI_D_MISC, "%s:  msg_flags: 0x%02x port_number: %d\n",
	    DEVNAME(sc), pfp->msg_flags, pfp->port_number);

	DNPRINTF(MPI_D_MISC, "%s:  msg_context: 0x%08x\n", DEVNAME(sc),
	    letoh32(pfp->msg_context));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(pfp->ioc_status));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(pfp->ioc_loginfo));

	DNPRINTF(MPI_D_MISC, "%s:  max_devices: %d port_type: 0x%02x\n",
	    DEVNAME(sc), letoh16(pfp->max_devices), pfp->port_type);

	DNPRINTF(MPI_D_MISC, "%s:  protocol_flags: 0x%04x port_scsi_id: %d\n",
	    DEVNAME(sc), letoh16(pfp->protocol_flags),
	    letoh16(pfp->port_scsi_id));

	DNPRINTF(MPI_D_MISC, "%s:  max_persistent_ids: %d "
	    "max_posted_cmd_buffers: %d\n", DEVNAME(sc),
	    letoh16(pfp->max_persistent_ids),
	    letoh16(pfp->max_posted_cmd_buffers));

	DNPRINTF(MPI_D_MISC, "%s:  max_lan_buckets: %d\n", DEVNAME(sc),
	    letoh16(pfp->max_lan_buckets));

	sc->sc_porttype = pfp->port_type;
	sc->sc_target = letoh16(pfp->port_scsi_id);

	mpi_push_reply(sc, ccb->ccb_reply_dva);
	rv = 0;
err:
	mpi_put_ccb(sc, ccb);

	return (rv);
}

int
mpi_eventnotify(struct mpi_softc *sc)
{
	struct mpi_ccb				*ccb;
	struct mpi_msg_event_request		*enq;
	int					s;

	s = splbio();
	ccb = mpi_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_eventnotify ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	ccb->ccb_done = mpi_eventnotify_done;
	enq = ccb->ccb_cmd;

	enq->function = MPI_FUNCTION_EVENT_NOTIFICATION;
	enq->chain_offset = 0;
	enq->ev_switch = 1;
	enq->msg_context = htole32(ccb->ccb_id);

	mpi_start(sc, ccb);

	return (0);
}

void
mpi_eventnotify_done(struct mpi_ccb *ccb)
{
	struct mpi_softc			*sc = ccb->ccb_sc;
	struct mpi_msg_event_reply		*enp = ccb->ccb_reply;
	u_int32_t				*data;
	int					i;

	printf("%s: %s\n", DEVNAME(sc), __func__);

	printf("%s:  function: 0x%02x msg_length: %d data_length: %d\n",
	    DEVNAME(sc), enp->function, enp->msg_length,
	    letoh16(enp->data_length));

	printf("%s:  ack_required: %d msg_flags 0x%02x\n", DEVNAME(sc),
	    enp->msg_flags, enp->msg_flags);

	printf("%s:  msg_context: 0x%08x\n", DEVNAME(sc),
	    letoh32(enp->msg_context));

	printf("%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(enp->ioc_status));

	printf("%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(enp->ioc_loginfo));

	data = ccb->ccb_reply;
	data += dwordsof(struct mpi_msg_event_reply);
	for (i = 0; i < letoh16(enp->data_length); i++) {
		printf("%s:  data[%d]: 0x%08x\n", DEVNAME(sc), i, data[i]);
	}
}

int
mpi_portenable(struct mpi_softc *sc)
{
	struct mpi_ccb				*ccb;
	struct mpi_msg_portenable_request	*peq;
	struct mpi_msg_portenable_repy		*pep;
	int					s;

	DNPRINTF(MPI_D_MISC, "%s: mpi_portenable\n", DEVNAME(sc));

	s = splbio();
	ccb = mpi_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_portenable ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	ccb->ccb_done = mpi_empty_done;
	peq = ccb->ccb_cmd;

	peq->function = MPI_FUNCTION_PORT_ENABLE;
	peq->port_number = 0;
	peq->msg_context = htole32(ccb->ccb_id);

	if (mpi_poll(sc, ccb, 50000) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_portenable poll\n", DEVNAME(sc));
		return (1);
	}

	pep = ccb->ccb_reply;
	if (pep == NULL) {
		DNPRINTF(MPI_D_MISC, "%s: empty portenable reply\n",
		    DEVNAME(sc));
		return (1);
	}

	mpi_push_reply(sc, ccb->ccb_reply_dva);
	mpi_put_ccb(sc, ccb);

	return (0);
}

void
mpi_get_raid(struct mpi_softc *sc)
{
	struct mpi_cfg_hdr		hdr;
	struct mpi_cfg_raid_vol		*raidvol;
	int				i;

	DNPRINTF(MPI_D_RAID, "%s: mpi_get_raid\n", DEVNAME(sc));

	if (mpi_cfg_header(sc, MPI_CONFIG_REQ_PAGE_TYPE_IOC, 2, 0, &hdr) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_get_raid unable to fetch header"
		    "for IOC page 2\n", DEVNAME(sc));
		return;
	}

	/* make page length bytes instead of dwords */
	sc->sc_ioc_pg2 = malloc(hdr.page_length * 4, M_DEVBUF, M_WAITOK);
	if (mpi_cfg_page(sc, 0, &hdr, 1, sc->sc_ioc_pg2,
	    hdr.page_length * 4) != 0) {
		DNPRINTF(MPI_D_RAID, "%s: mpi_get_raid unable to fetch IOC "
		    "page 2\n", DEVNAME(sc));
		return;
	}

	DNPRINTF(MPI_D_RAID, "%s:  capabilities: %x active vols %d "
	    "max vols: %d\n", DEVNAME(sc),
	    letoh32(sc->sc_ioc_pg2->capabilities),
	    sc->sc_ioc_pg2->no_active_vols, sc->sc_ioc_pg2->max_vols);
	DNPRINTF(MPI_D_RAID, "%s:  active phys disks: %d max disks: %d\n",
	    DEVNAME(sc), sc->sc_ioc_pg2->no_active_phys_disks,
	    sc->sc_ioc_pg2->max_phys_disks);

	for (i = 0; i < sc->sc_ioc_pg2->max_vols; i++) {
		raidvol = &sc->sc_ioc_pg2->raid_vol[i];
		DNPRINTF(MPI_D_RAID, "%s:   id: %#02x bus: %d ioc: %d page: %d "
		    "type: %#02x flags: %#02x\n", DEVNAME(sc), raidvol->vol_id,
		    raidvol->vol_bus, raidvol->vol_ioc, raidvol->vol_page,
		    raidvol->vol_type, raidvol->flags);

	}

	/* reuse hdr */
	if (mpi_cfg_header(sc, MPI_CONFIG_REQ_PAGE_TYPE_IOC, 3, 0, &hdr) != 0) {
		DNPRINTF(MPI_D_PPR, "%s: mpi_get_raid unable to fetch header"
		    "for IOC page 3\n", DEVNAME(sc));
		return;
	}

	/* make page length bytes instead of dwords */
	sc->sc_ioc_pg3 = malloc(hdr.page_length * 4, M_DEVBUF, M_WAITOK);
	if (mpi_cfg_page(sc, 0, &hdr, 1, sc->sc_ioc_pg3,
	    hdr.page_length * 4) != 0) {
		DNPRINTF(MPI_D_RAID, "%s: mpi_get_raid unable to fetch IOC "
		    "page 3\n", DEVNAME(sc));
		return;
	}

	for (i = 0; i < sc->sc_ioc_pg3->no_phys_disks; i++) {
		DNPRINTF(MPI_D_RAID, "%s:    id: %#02x bus: %d ioc: %d "
		    "num: %#02x\n", DEVNAME(sc),
		    sc->sc_ioc_pg3->phys_disks[i].phys_disk_id,
		    sc->sc_ioc_pg3->phys_disks[i].phys_disk_bus,
		    sc->sc_ioc_pg3->phys_disks[i].phys_disk_ioc,
		    sc->sc_ioc_pg3->phys_disks[i].phys_disk_num);
	}
}

int
mpi_cfg_header(struct mpi_softc *sc, u_int8_t type, u_int8_t number,
    u_int32_t address, struct mpi_cfg_hdr *hdr)
{
	struct mpi_ccb				*ccb;
	struct mpi_msg_config_request		*cq;
	struct mpi_msg_config_reply		*cp;
	int					s;

	DNPRINTF(MPI_D_MISC, "%s: mpi_cfg_header type: %#x number: %x "
	    "address: %d\n", DEVNAME(sc), type, number, address);

	s = splbio();
	ccb = mpi_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_cfg_header ccb_get\n",
		    DEVNAME(sc));
		return (1);
	}

	ccb->ccb_done = mpi_empty_done;
	cq = ccb->ccb_cmd;

	cq->function = MPI_FUNCTION_CONFIG;
	cq->msg_context = htole32(ccb->ccb_id);

	cq->action = MPI_CONFIG_REQ_ACTION_PAGE_HEADER;

	cq->config_header.page_number = number;
	cq->config_header.page_type = type;
	cq->page_address = htole32(address);
	cq->page_buffer.sg_hdr = htole32(MPI_SGE_FL_TYPE_SIMPLE |
	    MPI_SGE_FL_LAST | MPI_SGE_FL_EOB | MPI_SGE_FL_EOL);

	if (mpi_poll(sc, ccb, 50000) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_cfg_header poll\n", DEVNAME(sc));
		return (1);
	}

	cp = ccb->ccb_reply;
	if (cp == NULL)
		panic("%s: unable to fetch config header\n", DEVNAME(sc));

	DNPRINTF(MPI_D_MISC, "%s:  action: 0x%02x msg_length: %d function: "
	    "0x%02x\n", DEVNAME(sc), cp->action, cp->msg_length, cp->function);

	DNPRINTF(MPI_D_MISC, "%s:  ext_page_length: %d ext_page_type: 0x%02x "
	    "msg_flags: 0x%02x\n", DEVNAME(sc),
	    letoh16(cp->ext_page_length), cp->ext_page_type,
	    cp->msg_flags);

	DNPRINTF(MPI_D_MISC, "%s:  msg_context: 0x%08x\n", DEVNAME(sc),
	    letoh32(cp->msg_context));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(cp->ioc_status));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(cp->ioc_loginfo));

	DNPRINTF(MPI_D_MISC, "%s:  page_version: 0x%02x page_length: %d "
	    "page_number: 0x%02x page_type: 0x%02x\n", DEVNAME(sc),
	    cp->config_header.page_version, 
	    cp->config_header.page_length, 
	    cp->config_header.page_number, 
	    cp->config_header.page_type);

	*hdr = cp->config_header;

	mpi_push_reply(sc, ccb->ccb_reply_dva);
	mpi_put_ccb(sc, ccb);

	return (0);
}

int
mpi_cfg_page(struct mpi_softc *sc, u_int32_t address, struct mpi_cfg_hdr *hdr,
    int read, void *page, size_t len)
{
	struct mpi_ccb				*ccb;
	struct mpi_msg_config_request		*cq;
	struct mpi_msg_config_reply		*cp;
	u_int64_t				dva;
	char					*kva;
	int					s;

	DNPRINTF(MPI_D_MISC, "%s: mpi_cfg_page address: %d read: %d type: %x\n",
	    DEVNAME(sc), address, read, hdr->page_type);

	if (len > MPI_REQUEST_SIZE - sizeof(struct mpi_msg_config_request) ||
	    len < hdr->page_length * 4)
		return (1);

	s = splbio();
	ccb = mpi_get_ccb(sc);
	splx(s);
	if (ccb == NULL) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_cfg_page ccb_get\n", DEVNAME(sc));
		return (1);
	}

	ccb->ccb_done = mpi_empty_done;
	cq = ccb->ccb_cmd;

	cq->function = MPI_FUNCTION_CONFIG;
	cq->msg_context = htole32(ccb->ccb_id);

	cq->action = (read ? MPI_CONFIG_REQ_ACTION_PAGE_READ_CURRENT :
	    MPI_CONFIG_REQ_ACTION_PAGE_WRITE_CURRENT);

	cq->config_header = *hdr;
	cq->config_header.page_type &= MPI_CONFIG_REQ_PAGE_TYPE_MASK;
	cq->page_address = htole32(address);
	cq->page_buffer.sg_hdr = htole32(MPI_SGE_FL_TYPE_SIMPLE |
	    MPI_SGE_FL_LAST | MPI_SGE_FL_EOB | MPI_SGE_FL_EOL |
	    (hdr->page_length * 4) |
	    (read ? MPI_SGE_FL_DIR_IN : MPI_SGE_FL_DIR_OUT));

	/* bounce the page via the request space to avoid more bus_dma games */
	dva = ccb->ccb_cmd_dva + sizeof(struct mpi_msg_config_request);

	cq->page_buffer.sg_hi_addr = htole32((u_int32_t)(dva >> 32));
	cq->page_buffer.sg_lo_addr = htole32((u_int32_t)dva);

	kva = ccb->ccb_cmd;
	kva += sizeof(struct mpi_msg_config_request);
	if (!read)
		bcopy(page, kva, len);

	if (mpi_poll(sc, ccb, 50000) != 0) {
		DNPRINTF(MPI_D_MISC, "%s: mpi_cfg_page poll\n", DEVNAME(sc));
		return (1);
	}

	cp = ccb->ccb_reply;
	if (cp == NULL) {
		mpi_put_ccb(sc, ccb);
		return (1);
	}

	DNPRINTF(MPI_D_MISC, "%s:  action: 0x%02x msg_length: %d function: "
	    "0x%02x\n", DEVNAME(sc), cp->action, cp->msg_length, cp->function);

	DNPRINTF(MPI_D_MISC, "%s:  ext_page_length: %d ext_page_type: 0x%02x "
	    "msg_flags: 0x%02x\n", DEVNAME(sc),
	    letoh16(cp->ext_page_length), cp->ext_page_type,
	    cp->msg_flags);

	DNPRINTF(MPI_D_MISC, "%s:  msg_context: 0x%08x\n", DEVNAME(sc),
	    letoh32(cp->msg_context));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_status: 0x%04x\n", DEVNAME(sc),
	    letoh16(cp->ioc_status));

	DNPRINTF(MPI_D_MISC, "%s:  ioc_loginfo: 0x%08x\n", DEVNAME(sc),
	    letoh32(cp->ioc_loginfo));

	DNPRINTF(MPI_D_MISC, "%s:  page_version: 0x%02x page_length: %d "
	    "page_number: 0x%02x page_type: 0x%02x\n", DEVNAME(sc),
	    cp->config_header.page_version, 
	    cp->config_header.page_length, 
	    cp->config_header.page_number, 
	    cp->config_header.page_type);

	if (read)
		bcopy(kva, page, len);

	mpi_push_reply(sc, ccb->ccb_reply_dva);
	mpi_put_ccb(sc, ccb);

	return (0);
}
