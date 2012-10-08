/* $OpenBSD: softraid_concat.c,v 1.3 2012/10/08 14:22:41 jsing Exp $ */
/*
 * Copyright (c) 2008 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2011 Joel Sing <jsing@openbsd.org>
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

#include "bio.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/device.h>
#include <sys/buf.h>
#include <sys/queue.h>
#include <sys/sensors.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>

/* CONCAT functions. */
int	sr_concat_create(struct sr_discipline *, struct bioc_createraid *,
	    int, int64_t);
int	sr_concat_assemble(struct sr_discipline *, struct bioc_createraid *,
	    int, void *);
int	sr_concat_alloc_resources(struct sr_discipline *);
int	sr_concat_free_resources(struct sr_discipline *);
int	sr_concat_rw(struct sr_workunit *);
void	sr_concat_intr(struct buf *);

/* Discipline initialisation. */
void
sr_concat_discipline_init(struct sr_discipline *sd)
{

	/* Fill out discipline members. */
	sd->sd_type = SR_MD_CONCAT;
	sd->sd_capabilities = SR_CAP_SYSTEM_DISK | SR_CAP_AUTO_ASSEMBLE |
	    SR_CAP_NON_COERCED;
	sd->sd_max_wu = SR_CONCAT_NOWU;

	/* Setup discipline specific function pointers. */
	sd->sd_alloc_resources = sr_concat_alloc_resources;
	sd->sd_assemble = sr_concat_assemble;
	sd->sd_create = sr_concat_create;
	sd->sd_free_resources = sr_concat_free_resources;
	sd->sd_scsi_rw = sr_concat_rw;
}

int
sr_concat_create(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk, int64_t coerced_size)
{
	int			i;

	if (no_chunk < 2) {
		sr_error(sd->sd_sc, "CONCAT requires two or more chunks");
		return EINVAL;
        }

	strlcpy(sd->sd_name, "CONCAT", sizeof(sd->sd_name));

	sd->sd_meta->ssdi.ssd_size = 0;
	for (i = 0; i < no_chunk; i++)
		sd->sd_meta->ssdi.ssd_size +=
		    sd->sd_vol.sv_chunks[i]->src_size;
	sd->sd_max_ccb_per_wu = SR_CONCAT_NOWU * no_chunk;

	return 0;
}

int
sr_concat_assemble(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk, void *data)
{

	sd->sd_max_ccb_per_wu = SR_CONCAT_NOWU * no_chunk;

	return 0;
}

int
sr_concat_alloc_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_concat_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	if (sr_wu_alloc(sd))
		goto bad;
	if (sr_ccb_alloc(sd))
		goto bad;

	rv = 0;
bad:
	return (rv);
}

int
sr_concat_free_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_concat_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_wu_free(sd);
	sr_ccb_free(sd);

	rv = 0;
	return (rv);
}

int
sr_concat_rw(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_ccb		*ccb;
	struct sr_chunk		*scp;
	int			s;
	daddr64_t		blk, lbaoffs, chunk, chunksize;
	daddr64_t		no_chunk, chunkend, physoffs;
	daddr64_t		length, leftover;
	u_int8_t		*data;

	/* blk and scsi error will be handled by sr_validate_io */
	if (sr_validate_io(wu, &blk, "sr_concat_rw"))
		goto bad;

	no_chunk = sd->sd_meta->ssdi.ssd_chunk_no;

	DNPRINTF(SR_D_DIS, "%s: %s: front end io: lba %lld size %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
	    blk, xs->datalen);

	/* All offsets are in bytes. */
	lbaoffs = blk << DEV_BSHIFT;
	leftover = xs->datalen;
	data = xs->data;
	for (wu->swu_io_count = 1;; wu->swu_io_count++) {

		chunkend = 0;
		physoffs = lbaoffs;
		for (chunk = 0; chunk < no_chunk; chunk++) {
			chunksize = sd->sd_vol.sv_chunks[chunk]->src_size <<
			    DEV_BSHIFT;
			chunkend += chunksize;
			if (lbaoffs < chunkend)
				break;
			physoffs -= chunksize;
		}
		if (lbaoffs > chunkend)
			goto bad;

		length = MIN(MIN(leftover, chunkend - lbaoffs), MAXPHYS);
		physoffs += sd->sd_meta->ssd_data_offset << DEV_BSHIFT;

		/* make sure chunk is online */
		scp = sd->sd_vol.sv_chunks[chunk];
		if (scp->src_meta.scm_status != BIOC_SDONLINE) {
			goto bad;
		}

		ccb = sr_ccb_get(sd);
		if (!ccb) {
			/* should never happen but handle more gracefully */
			printf("%s: %s: too many ccbs queued\n",
			    DEVNAME(sd->sd_sc),
			    sd->sd_meta->ssd_devname);
			goto bad;
		}

		DNPRINTF(SR_D_DIS, "%s: %s concat io: lbaoffs: %lld "
		    "chunk: %lld chunkend: %lld physoffs: %lld length: %lld "
		    "leftover: %lld data: %p\n",
		    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname, lbaoffs,
		    chunk, chunkend, physoffs, length, leftover, data);

		ccb->ccb_buf.b_flags = B_CALL | B_PHYS;
		ccb->ccb_buf.b_iodone = sr_concat_intr;
		ccb->ccb_buf.b_blkno = physoffs >> DEV_BSHIFT;
		ccb->ccb_buf.b_bcount = length;
		ccb->ccb_buf.b_bufsize = length;
		ccb->ccb_buf.b_resid = length;
		ccb->ccb_buf.b_data = data;
		ccb->ccb_buf.b_error = 0;
		ccb->ccb_buf.b_proc = curproc;
		ccb->ccb_buf.b_bq = NULL;
		ccb->ccb_wu = wu;
		ccb->ccb_buf.b_flags |= xs->flags & SCSI_DATA_IN ?
		    B_READ : B_WRITE;
		ccb->ccb_target = chunk;
		ccb->ccb_buf.b_dev = sd->sd_vol.sv_chunks[chunk]->src_dev_mm;
		ccb->ccb_buf.b_vp = sd->sd_vol.sv_chunks[chunk]->src_vn;
		if ((ccb->ccb_buf.b_flags & B_READ) == 0)
			ccb->ccb_buf.b_vp->v_numoutput++;
		LIST_INIT(&ccb->ccb_buf.b_dep);
		TAILQ_INSERT_TAIL(&wu->swu_ccb, ccb, ccb_link);

		DNPRINTF(SR_D_DIS, "%s: %s: sr_concat: b_bcount: %d "
		    "b_blkno: %lld b_flags 0x%0x b_data %p\n",
		    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
		    ccb->ccb_buf.b_bcount, ccb->ccb_buf.b_blkno,
		    ccb->ccb_buf.b_flags, ccb->ccb_buf.b_data);

		leftover -= length;
		if (leftover == 0)
			break;
		data += length;
		lbaoffs += length;
	}

	s = splbio();

	if (!sr_check_io_collision(wu))
		sr_raid_startwu(wu);

	splx(s);
	return (0);
bad:
	/* wu is unwound by sr_wu_put */
	return (1);
}

void
sr_concat_intr(struct buf *bp)
{
	struct sr_ccb		*ccb = (struct sr_ccb *)bp;
	struct sr_workunit	*wu = ccb->ccb_wu, *wup;
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_softc		*sc = sd->sd_sc;
	int			s, pend;

	DNPRINTF(SR_D_INTR, "%s: sr_intr bp %x xs %x\n",
	    DEVNAME(sc), bp, xs);

	DNPRINTF(SR_D_INTR, "%s: sr_intr: b_bcount: %d b_resid: %d"
	    " b_flags: 0x%0x block: %lld target: %d\n", DEVNAME(sc),
	    ccb->ccb_buf.b_bcount, ccb->ccb_buf.b_resid, ccb->ccb_buf.b_flags,
	    ccb->ccb_buf.b_blkno, ccb->ccb_target);

	s = splbio();

	if (ccb->ccb_buf.b_flags & B_ERROR) {
		printf("%s: i/o error on block %lld target: %d b_error: %d\n",
		    DEVNAME(sc), ccb->ccb_buf.b_blkno, ccb->ccb_target,
		    ccb->ccb_buf.b_error);
		DNPRINTF(SR_D_INTR, "%s: i/o error on block %lld target: %d\n",
		    DEVNAME(sc), ccb->ccb_buf.b_blkno, ccb->ccb_target);
		wu->swu_ios_failed++;
		ccb->ccb_state = SR_CCB_FAILED;
		if (ccb->ccb_target != -1)
			sd->sd_set_chunk_state(sd, ccb->ccb_target,
			    BIOC_SDOFFLINE);
		else
			panic("%s: invalid target on wu: %p", DEVNAME(sc), wu);
	} else {
		ccb->ccb_state = SR_CCB_OK;
		wu->swu_ios_succeeded++;
	}
	wu->swu_ios_complete++;

	DNPRINTF(SR_D_INTR, "%s: sr_intr: comp: %d count: %d failed: %d\n",
	    DEVNAME(sc), wu->swu_ios_complete, wu->swu_io_count,
	    wu->swu_ios_failed);

	if (wu->swu_ios_complete >= wu->swu_io_count) {
		if (wu->swu_ios_failed)
			goto bad;

		xs->error = XS_NOERROR;
		xs->resid = 0;

		pend = 0;
		TAILQ_FOREACH(wup, &sd->sd_wu_pendq, swu_link) {
			if (wu == wup) {
				/* wu on pendq, remove */
				TAILQ_REMOVE(&sd->sd_wu_pendq, wu, swu_link);
				pend = 1;

				if (wu->swu_collider) {
					/* restart deferred wu */
					wu->swu_collider->swu_state =
					    SR_WU_INPROGRESS;
					TAILQ_REMOVE(&sd->sd_wu_defq,
					    wu->swu_collider, swu_link);
					sr_raid_startwu(wu->swu_collider);
				}
				break;
			}
		}

		if (!pend)
			printf("%s: wu: %p not on pending queue\n",
			    DEVNAME(sc), wu);

		sr_scsi_done(sd, xs);

		if (sd->sd_sync && sd->sd_wu_pending == 0)
			wakeup(sd);
	}

	splx(s);
	return;
bad:
	xs->error = XS_DRIVER_STUFFUP;
	sr_scsi_done(sd, xs);
	splx(s);
}
