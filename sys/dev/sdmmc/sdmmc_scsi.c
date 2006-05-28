/*	$OpenBSD: sdmmc_scsi.c,v 1.1 2006/05/28 17:21:14 uwe Exp $	*/

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

/* A SCSI bus emulation to access SD/MMC memory cards. */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/systm.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>

#include <dev/sdmmc/sdmmc_scsi.h>
#include <dev/sdmmc/sdmmcvar.h>

#undef SDMMC_DEBUG

#define SDMMC_SCSIID_HOST	0x00
#define SDMMC_SCSIID_MAX	0x0f

int	sdmmc_scsi_cmd(struct scsi_xfer *);
int	sdmmc_start_xs(struct sdmmc_softc *, struct scsi_xfer *);
int	sdmmc_done_xs(struct sdmmc_softc *, struct scsi_xfer *);
void	sdmmc_complete(struct sdmmc_softc *, struct scsi_xfer *);
void	sdmmc_scsi_minphys(struct buf *);

#ifdef SDMMC_DEBUG
#define DPRINTF(s)	printf s
#else
#define DPRINTF(s)	/**/
#endif

void
sdmmc_scsi_attach(struct sdmmc_softc *sc)
{
	struct sdmmc_scsi_softc *scbus;
	struct sdmmc_card *cs;

	MALLOC(scbus, struct sdmmc_scsi_softc *,
	    sizeof *scbus, M_DEVBUF, M_WAITOK);
	bzero(scbus, sizeof *scbus);

	MALLOC(scbus->sc_tgt, struct sdmmc_scsi_target *,
	    sizeof(*scbus->sc_tgt) * (SDMMC_SCSIID_MAX+1),
	    M_DEVBUF, M_WAITOK);
	bzero(scbus->sc_tgt, sizeof(*scbus->sc_tgt) * (SDMMC_SCSIID_MAX+1));

	/*
	 * Each card that sent us a CID in the identification stage
	 * gets a SCSI ID > 0, whether it is a memory card or not.
	 */
	scbus->sc_ntargets = 1;
	SIMPLEQ_FOREACH(cs, &sc->cs_head, cs_list) {
		if (scbus->sc_ntargets >= SDMMC_SCSIID_MAX+1)
			break;
		scbus->sc_tgt[scbus->sc_ntargets].card = cs;
		scbus->sc_ntargets++;
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

	scbus->sc_child = config_found(&sc->sc_dev, &scbus->sc_link,
	    scsiprint);
}

void
sdmmc_scsi_detach(struct sdmmc_softc *sc)
{
	struct sdmmc_scsi_softc *scbus;

	scbus = sc->sc_scsibus;

	if (scbus->sc_child != NULL)
		config_detach(scbus->sc_child, DETACH_FORCE);

	if (scbus->sc_tgt != NULL)
		FREE(scbus->sc_tgt, M_DEVBUF);

	FREE(scbus, M_DEVBUF);
	sc->sc_scsibus = NULL;
}

int
sdmmc_scsi_cmd(struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct sdmmc_softc *sc = link->adapter_softc;
	struct sdmmc_scsi_softc *scbus = sc->sc_scsibus;
	struct sdmmc_scsi_target *tgt = &scbus->sc_tgt[link->target];
	struct scsi_inquiry_data inq;
	struct scsi_read_cap_data rcd;

	if (link->target >= scbus->sc_ntargets || tgt->card == NULL ||
	    link->lun != 0) {
		DPRINTF(("%s: sdmmc_scsi_cmd: no target %d\n",
		    SDMMCDEVNAME(sc), link->target));
		/* XXX should be XS_SENSE and sense filled out */
		xs->error = XS_DRIVER_STUFFUP;
		xs->flags |= ITSDONE;
		scsi_done(xs);
		return COMPLETE;
	}

	DPRINTF(("%s: sdmmc_scsi_cmd: target=%d xs=%p cmd=%#x "
	    "datalen=%d (poll=%d)\n", SDMMCDEVNAME(sc), link->target,
	    xs, xs->cmd->opcode, xs->datalen, xs->flags & SCSI_POLL));

	xs->error = XS_NOERROR;

	switch (xs->cmd->opcode) {
	case READ_COMMAND:
	case READ_BIG:
	case WRITE_COMMAND:
	case WRITE_BIG:
		/* Deal with I/O outside the switch. */
		break;

	case INQUIRY:
		bzero(&inq, sizeof inq);
		inq.device = T_DIRECT;
		inq.version = 2;
		inq.response_format = 2;
		inq.additional_length = 32;
		strlcpy(inq.vendor, "SD/MMC ", sizeof(inq.vendor));
		snprintf(inq.product, sizeof(inq.product),
		    "Drive #%02d", link->target);
		strlcpy(inq.revision, "   ", sizeof(inq.revision));
		bcopy(&inq, xs->data, MIN(xs->datalen, sizeof inq));
		scsi_done(xs);
		return COMPLETE;

	case TEST_UNIT_READY:
	case START_STOP:
	case SYNCHRONIZE_CACHE:
		return COMPLETE;

	case READ_CAPACITY:
		bzero(&rcd, sizeof rcd);
		_lto4b(tgt->card->csd.capacity - 1, rcd.addr);
		_lto4b(tgt->card->csd.sector_size, rcd.length);
		bcopy(&rcd, xs->data, MIN(xs->datalen, sizeof rcd));
		scsi_done(xs);
		return COMPLETE;


	default:
		DPRINTF(("%s: unsupported scsi command %#x\n",
		    SDMMCDEVNAME(sc), xs->cmd->opcode));
		xs->error = XS_DRIVER_STUFFUP;
		scsi_done(xs);
		return COMPLETE;
	}

	/* XXX check bounds */

	return sdmmc_start_xs(sc, xs);
}

int
sdmmc_start_xs(struct sdmmc_softc *sc, struct scsi_xfer *xs)
{
	sdmmc_complete(sc, xs);
	return COMPLETE;
}

void
sdmmc_complete(struct sdmmc_softc *sc, struct scsi_xfer *xs)
{
	struct scsi_link *link = xs->sc_link;
	struct sdmmc_scsi_softc *scbus = sc->sc_scsibus;
	struct sdmmc_scsi_target *tgt = &scbus->sc_tgt[link->target];
	struct scsi_rw *rw;
	struct scsi_rw_big *rwb;
	u_int32_t blockno;
	u_int32_t blockcnt;
	int error;

	/* A read or write operation. */
	/* XXX move to some sort of "scsi emulation layer". */
	if (xs->cmdlen == 6) {
		rw = (struct scsi_rw *)xs->cmd;
		blockno = _3btol(rw->addr) & (SRW_TOPADDR << 16 | 0xffff);
		blockcnt = rw->length ? rw->length : 0x100;
	} else {
		rwb = (struct scsi_rw_big *)xs->cmd;
		blockno = _4btol(rwb->addr);
		blockcnt = _2btol(rwb->length);
	}

	if (ISSET(xs->flags, SCSI_DATA_IN))
		error = sdmmc_mem_read_block(sc, tgt->card, blockno,
		    xs->data, blockcnt * DEV_BSIZE);
	else
		error = sdmmc_mem_write_block(sc, tgt->card, blockno,
		    xs->data, blockcnt * DEV_BSIZE);
	if (error != 0)
		xs->error = XS_DRIVER_STUFFUP;

	xs->flags |= ITSDONE;
	xs->resid = 0;
	scsi_done(xs);
}

void
sdmmc_scsi_minphys(struct buf *bp)
{
	/* XXX limit to max. transfer size supported by card/host? */
	if (bp->b_bcount > DEV_BSIZE)
		bp->b_bcount = DEV_BSIZE;
	minphys(bp);
}
