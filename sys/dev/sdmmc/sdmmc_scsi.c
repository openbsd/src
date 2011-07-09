/*	$OpenBSD: sdmmc_scsi.c,v 1.28 2011/07/09 00:39:29 matthew Exp $	*/

/*
 * Copyright (c) 2006 Uwe Stuehler <uwe@openbsd.org>
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

/* A SCSI adapter emulation to access SD/MMC memory cards */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/kernel.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/systm.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/sdmmc/sdmmc_scsi.h>
#include <dev/sdmmc/sdmmcvar.h>

#define SDMMC_SCSIID_HOST	0x00
#define SDMMC_SCSIID_MAX	0x0f

#define SDMMC_SCSI_MAXCMDS	8

struct sdmmc_scsi_target {
	struct sdmmc_function *card;
};

struct sdmmc_ccb {
	struct sdmmc_scsi_softc *ccb_scbus;
	struct scsi_xfer *ccb_xs;
	int ccb_flags;
#define SDMMC_CCB_F_ERR		0x0001
	u_int32_t ccb_blockno;
	u_int32_t ccb_blockcnt;
	volatile enum {
		SDMMC_CCB_FREE,
		SDMMC_CCB_READY,
		SDMMC_CCB_QUEUED
	} ccb_state;
	struct sdmmc_command ccb_cmd;
	struct sdmmc_task ccb_task;
	TAILQ_ENTRY(sdmmc_ccb) ccb_link;
};

TAILQ_HEAD(sdmmc_ccb_list, sdmmc_ccb);

struct sdmmc_scsi_softc {
	struct scsi_adapter sc_adapter;
	struct scsi_link sc_link;
	struct device *sc_child;
	struct sdmmc_scsi_target *sc_tgt;
	int sc_ntargets;
	struct sdmmc_ccb *sc_ccbs;		/* allocated ccbs */
	struct sdmmc_ccb_list sc_ccb_freeq;	/* free ccbs */
	struct sdmmc_ccb_list sc_ccb_runq;	/* queued ccbs */
	struct mutex sc_ccb_mtx;
	struct scsi_iopool sc_iopool;
};

int	sdmmc_alloc_ccbs(struct sdmmc_scsi_softc *, int);
void	sdmmc_free_ccbs(struct sdmmc_scsi_softc *);
void	*sdmmc_ccb_alloc(void *);
void	sdmmc_ccb_free(void *, void *);

void	sdmmc_scsi_cmd(struct scsi_xfer *);
void	sdmmc_inquiry(struct scsi_xfer *);
void	sdmmc_start_xs(struct sdmmc_softc *, struct sdmmc_ccb *);
void	sdmmc_complete_xs(void *);
void	sdmmc_done_xs(struct sdmmc_ccb *);
void	sdmmc_stimeout(void *);
void	sdmmc_scsi_minphys(struct buf *, struct scsi_link *);

#ifdef SDMMC_DEBUG
#define DPRINTF(s)	printf s
#else
#define DPRINTF(s)	/**/
#endif

void
sdmmc_scsi_attach(struct sdmmc_softc *sc)
{
	struct sdmmc_attach_args saa;
	struct sdmmc_scsi_softc *scbus;
	struct sdmmc_function *sf;

	rw_assert_wrlock(&sc->sc_lock);

	scbus = malloc(sizeof *scbus, M_DEVBUF, M_WAITOK | M_ZERO);

	scbus->sc_tgt = malloc(sizeof(*scbus->sc_tgt) *
	    (SDMMC_SCSIID_MAX+1), M_DEVBUF, M_WAITOK | M_ZERO);

	/*
	 * Each card that sent us a CID in the identification stage
	 * gets a SCSI ID > 0, whether it is a memory card or not.
	 */
	scbus->sc_ntargets = 1;
	SIMPLEQ_FOREACH(sf, &sc->sf_head, sf_list) {
		if (scbus->sc_ntargets >= SDMMC_SCSIID_MAX+1)
			break;
		scbus->sc_tgt[scbus->sc_ntargets].card = sf;
		scbus->sc_ntargets++;
	}

	/* Preallocate some CCBs and initialize the CCB lists. */
	if (sdmmc_alloc_ccbs(scbus, SDMMC_SCSI_MAXCMDS) != 0) {
		printf("%s: can't allocate ccbs\n", sc->sc_dev.dv_xname);
		goto free_sctgt;
	}

	sc->sc_scsibus = scbus;

	scbus->sc_adapter.scsi_cmd = sdmmc_scsi_cmd;
	scbus->sc_adapter.scsi_minphys = sdmmc_scsi_minphys;

	scbus->sc_link.adapter_target = SDMMC_SCSIID_HOST;
	scbus->sc_link.adapter_buswidth = scbus->sc_ntargets;
	scbus->sc_link.adapter_softc = sc;
	scbus->sc_link.luns = 1;
	scbus->sc_link.openings = 1;
	scbus->sc_link.adapter = &scbus->sc_adapter;
	scbus->sc_link.pool = &scbus->sc_iopool;

	bzero(&saa, sizeof(saa));
	saa.saa.saa_sc_link = &scbus->sc_link;

	scbus->sc_child = config_found(&sc->sc_dev, &saa.saa, scsiprint);
	if (scbus->sc_child == NULL) {
		printf("%s: can't attach scsibus\n", sc->sc_dev.dv_xname);
		goto free_ccbs;
	}
	return;

 free_ccbs:
	sc->sc_scsibus = NULL;
	sdmmc_free_ccbs(scbus);
 free_sctgt:
	free(scbus->sc_tgt, M_DEVBUF);
	free(scbus, M_DEVBUF);
}

void
sdmmc_scsi_detach(struct sdmmc_softc *sc)
{
	struct sdmmc_scsi_softc *scbus;
	struct sdmmc_ccb *ccb;
	int s;

	rw_assert_wrlock(&sc->sc_lock);

	scbus = sc->sc_scsibus;
	if (scbus == NULL)
		return;

	/* Complete all open scsi xfers. */
	s = splbio();
	for (ccb = TAILQ_FIRST(&scbus->sc_ccb_runq); ccb != NULL;
	     ccb = TAILQ_FIRST(&scbus->sc_ccb_runq))
		sdmmc_stimeout(ccb);
	splx(s);

	if (scbus->sc_child != NULL)
		config_detach(scbus->sc_child, DETACH_FORCE);

	if (scbus->sc_tgt != NULL)
		free(scbus->sc_tgt, M_DEVBUF);

	sdmmc_free_ccbs(scbus);
	free(scbus, M_DEVBUF);
	sc->sc_scsibus = NULL;
}

/*
 * CCB management
 */

int
sdmmc_alloc_ccbs(struct sdmmc_scsi_softc *scbus, int nccbs)
{
	struct sdmmc_ccb *ccb;
	int i;

	scbus->sc_ccbs = malloc(sizeof(struct sdmmc_ccb) * nccbs,
	    M_DEVBUF, M_NOWAIT);
	if (scbus->sc_ccbs == NULL)
		return 1;

	TAILQ_INIT(&scbus->sc_ccb_freeq);
	TAILQ_INIT(&scbus->sc_ccb_runq);
	mtx_init(&scbus->sc_ccb_mtx, IPL_BIO);
	scsi_iopool_init(&scbus->sc_iopool, scbus, sdmmc_ccb_alloc,
	    sdmmc_ccb_free);

	for (i = 0; i < nccbs; i++) {
		ccb = &scbus->sc_ccbs[i];
		ccb->ccb_scbus = scbus;
		ccb->ccb_state = SDMMC_CCB_FREE;
		ccb->ccb_flags = 0;
		ccb->ccb_xs = NULL;

		TAILQ_INSERT_TAIL(&scbus->sc_ccb_freeq, ccb, ccb_link);
	}
	return 0;
}

void
sdmmc_free_ccbs(struct sdmmc_scsi_softc *scbus)
{
	if (scbus->sc_ccbs != NULL) {
		free(scbus->sc_ccbs, M_DEVBUF);
		scbus->sc_ccbs = NULL;
	}
}

void *
sdmmc_ccb_alloc(void *xscbus)
{
	struct sdmmc_scsi_softc *scbus = xscbus;
	struct sdmmc_ccb *ccb;

	mtx_enter(&scbus->sc_ccb_mtx);
	ccb = TAILQ_FIRST(&scbus->sc_ccb_freeq);
	if (ccb != NULL) {
		TAILQ_REMOVE(&scbus->sc_ccb_freeq, ccb, ccb_link);
		ccb->ccb_state = SDMMC_CCB_READY;
	}
	mtx_leave(&scbus->sc_ccb_mtx);

	return ccb;
}

void
sdmmc_ccb_free(void *xscbus, void *xccb)
{
	struct sdmmc_scsi_softc *scbus = xscbus;
	struct sdmmc_ccb *ccb = xccb;
	int s;

	s = splbio();
	if (ccb->ccb_state == SDMMC_CCB_QUEUED)
		TAILQ_REMOVE(&scbus->sc_ccb_runq, ccb, ccb_link);
	splx(s);

	ccb->ccb_state = SDMMC_CCB_FREE;
	ccb->ccb_flags = 0;
	ccb->ccb_xs = NULL;

	mtx_enter(&scbus->sc_ccb_mtx);
	TAILQ_INSERT_TAIL(&scbus->sc_ccb_freeq, ccb, ccb_link);
	mtx_leave(&scbus->sc_ccb_mtx);
}

/*
 * SCSI command emulation
 */

/* XXX move to some sort of "scsi emulation layer". */
static void
sdmmc_scsi_decode_rw(struct scsi_xfer *xs, u_int32_t *blocknop,
    u_int32_t *blockcntp)
{
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	
	if (xs->cmdlen == 6) {
		rw = (struct scsi_rw *)xs->cmd;
		*blocknop = _3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff);
		*blockcntp = rw->length ? rw->length : 0x100;
	} else {
		rwb = (struct scsi_rw_big *)xs->cmd;
		*blocknop = _4btol(rwb->addr);
		*blockcntp = _2btol(rwb->length);
	}
}

void
sdmmc_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct sdmmc_softc *sc = link->adapter_softc;
	struct sdmmc_scsi_softc *scbus = sc->sc_scsibus;
	struct sdmmc_scsi_target *tgt = &scbus->sc_tgt[link->target];
	struct scsi_read_cap_data rcd;
	u_int32_t blockno;
	u_int32_t blockcnt;
	struct sdmmc_ccb *ccb;

	if (link->target >= scbus->sc_ntargets || tgt->card == NULL ||
	    link->lun != 0) {
		DPRINTF(("%s: sdmmc_scsi_cmd: no target %d\n",
		    DEVNAME(sc), link->target));
		/* XXX should be XS_SENSE and sense filled out */
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;
	}

	DPRINTF(("%s: scsi cmd target=%d opcode=%#x proc=\"%s\" (poll=%#x)\n",
	    DEVNAME(sc), link->target, xs->cmd->opcode, curproc ?
	    curproc->p_comm : "", xs->flags & SCSI_POLL));

	xs->error = XS_NOERROR;

	switch (xs->cmd->opcode) {
	case READ_COMMAND:
	case READ_BIG:
	case WRITE_COMMAND:
	case WRITE_BIG:
		/* Deal with I/O outside the switch. */
		break;

	case INQUIRY:
		sdmmc_inquiry(xs);
		return;

	case TEST_UNIT_READY:
	case START_STOP:
	case SYNCHRONIZE_CACHE:
		scsi_done(xs);
		return;

	case READ_CAPACITY:
		bzero(&rcd, sizeof rcd);
		_lto4b(tgt->card->csd.capacity - 1, rcd.addr);
		_lto4b(tgt->card->csd.sector_size, rcd.length);
		bcopy(&rcd, xs->data, MIN(xs->datalen, sizeof rcd));
		scsi_done(xs);
		return;

	default:
		DPRINTF(("%s: unsupported scsi command %#x\n",
		    DEVNAME(sc), xs->cmd->opcode));
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;
	}

	/* A read or write operation. */
	sdmmc_scsi_decode_rw(xs, &blockno, &blockcnt);

	if (blockno >= tgt->card->csd.capacity ||
	    blockno + blockcnt > tgt->card->csd.capacity) {
		DPRINTF(("%s: out of bounds %u-%u >= %u\n", DEVNAME(sc),
		    blockno, blockcnt, tgt->card->csd.capacity));
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return;
	}

	ccb = xs->io;

	ccb->ccb_xs = xs;
	ccb->ccb_blockcnt = blockcnt;
	ccb->ccb_blockno = blockno;

	sdmmc_start_xs(sc, ccb);
}

void
sdmmc_inquiry(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct scsi_inquiry_data inq;
	struct scsi_inquiry *cdb = (struct scsi_inquiry *)xs->cmd;

        if (xs->cmdlen != sizeof(*cdb)) {
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

	if (ISSET(cdb->flags, SI_EVPD)) {
		xs->error = XS_DRIVER_STUFFUP;
		goto done;
	}

	bzero(&inq, sizeof inq);
	inq.device = T_DIRECT;
	inq.version = 2;
	inq.response_format = 2;
	inq.additional_length = 32;
	strlcpy(inq.vendor, "SD/MMC ", sizeof(inq.vendor));
	snprintf(inq.product, sizeof(inq.product),
	    "Drive #%02d", link->target);
	strlcpy(inq.revision, "   ", sizeof(inq.revision));

	bcopy(&inq, xs->data, MIN(xs->datalen, sizeof(inq)));

done:
	scsi_done(xs);
}

void
sdmmc_start_xs(struct sdmmc_softc *sc, struct sdmmc_ccb *ccb)
{
	struct sdmmc_scsi_softc *scbus = sc->sc_scsibus;
	struct scsi_xfer *xs = ccb->ccb_xs;
	int s;

	timeout_set(&xs->stimeout, sdmmc_stimeout, ccb);
	sdmmc_init_task(&ccb->ccb_task, sdmmc_complete_xs, ccb);

	s = splbio();
	TAILQ_INSERT_TAIL(&scbus->sc_ccb_runq, ccb, ccb_link);
	ccb->ccb_state = SDMMC_CCB_QUEUED;
	splx(s);

	if (ISSET(xs->flags, SCSI_POLL)) {
		sdmmc_complete_xs(ccb);
		return;
	}

	timeout_add_msec(&xs->stimeout, xs->timeout);
	sdmmc_add_task(sc, &ccb->ccb_task);
}

void
sdmmc_complete_xs(void *arg)
{
	struct sdmmc_ccb *ccb = arg;
	struct scsi_xfer *xs = ccb->ccb_xs;
	struct scsi_link *link = xs->sc_link;
	struct sdmmc_softc *sc = link->adapter_softc;
	struct sdmmc_scsi_softc *scbus = sc->sc_scsibus;
	struct sdmmc_scsi_target *tgt = &scbus->sc_tgt[link->target];
	int error;
	int s;

	DPRINTF(("%s: scsi cmd target=%d opcode=%#x proc=\"%s\" (poll=%#x)"
	    " complete\n", DEVNAME(sc), link->target, xs->cmd->opcode,
	    curproc ? curproc->p_comm : "", xs->flags & SCSI_POLL));

	s = splbio();

	if (ISSET(xs->flags, SCSI_DATA_IN))
		error = sdmmc_mem_read_block(tgt->card, ccb->ccb_blockno,
		    xs->data, ccb->ccb_blockcnt * DEV_BSIZE);
	else
		error = sdmmc_mem_write_block(tgt->card, ccb->ccb_blockno,
		    xs->data, ccb->ccb_blockcnt * DEV_BSIZE);

	if (error != 0)
		xs->error = XS_DRIVER_STUFFUP;

	sdmmc_done_xs(ccb);
	splx(s);
}

void
sdmmc_done_xs(struct sdmmc_ccb *ccb)
{
	struct scsi_xfer *xs = ccb->ccb_xs;
#ifdef SDMMC_DEBUG
	struct scsi_link *link = xs->sc_link;
	struct sdmmc_softc *sc = link->adapter_softc;
#endif

	timeout_del(&xs->stimeout);

	DPRINTF(("%s: scsi cmd target=%d opcode=%#x proc=\"%s\" (error=%#x)"
	    " done\n", DEVNAME(sc), link->target, xs->cmd->opcode,
	    curproc ? curproc->p_comm : "", xs->error));

	xs->resid = 0;

	if (ISSET(ccb->ccb_flags, SDMMC_CCB_F_ERR))
		xs->error = XS_DRIVER_STUFFUP;

	scsi_done(xs);
}

void
sdmmc_stimeout(void *arg)
{
	struct sdmmc_ccb *ccb = arg;
	int s;

	s = splbio();
	ccb->ccb_flags |= SDMMC_CCB_F_ERR;
	if (sdmmc_task_pending(&ccb->ccb_task)) {
		sdmmc_del_task(&ccb->ccb_task);
		sdmmc_done_xs(ccb);
	}
	splx(s);
}

void
sdmmc_scsi_minphys(struct buf *bp, struct scsi_link *sl)
{
	struct sdmmc_softc *sc = sl->adapter_softc;
	struct sdmmc_scsi_softc *scbus = sc->sc_scsibus;
	struct sdmmc_scsi_target *tgt = &scbus->sc_tgt[sl->target];
	struct sdmmc_function *sf = tgt->card;

	/* limit to max. transfer size supported by card/host */
	if (sc->sc_max_xfer != 0 &&
	    bp->b_bcount > sf->csd.sector_size * sc->sc_max_xfer)
		bp->b_bcount = sf->csd.sector_size * sc->sc_max_xfer;

	minphys(bp);
}
