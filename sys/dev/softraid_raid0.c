/* $OpenBSD: softraid_raid0.c,v 1.20 2010/03/26 11:20:34 jsing Exp $ */
/*
 * Copyright (c) 2008 Marco Peereboom <marco@peereboom.us>
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
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/ioctl.h>
#include <sys/proc.h>
#include <sys/malloc.h>
#include <sys/kernel.h>
#include <sys/disk.h>
#include <sys/rwlock.h>
#include <sys/queue.h>
#include <sys/fcntl.h>
#include <sys/disklabel.h>
#include <sys/mount.h>
#include <sys/sensors.h>
#include <sys/stat.h>
#include <sys/conf.h>
#include <sys/uio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

/* RAID 0 functions. */
int	sr_raid0_create(struct sr_discipline *, struct bioc_createraid *,
	    int, int64_t);
int	sr_raid0_assemble(struct sr_discipline *, struct bioc_createraid *,
	    int);
int	sr_raid0_alloc_resources(struct sr_discipline *);
int	sr_raid0_free_resources(struct sr_discipline *);
int	sr_raid0_rw(struct sr_workunit *);
void	sr_raid0_intr(struct buf *);
void	sr_raid0_set_chunk_state(struct sr_discipline *, int, int);
void	sr_raid0_set_vol_state(struct sr_discipline *);

/* Discipline initialisation. */
void
sr_raid0_discipline_init(struct sr_discipline *sd)
{

	/* Fill out discipline members. */
	sd->sd_type = SR_MD_RAID0;
	sd->sd_capabilities = SR_CAP_SYSTEM_DISK | SR_CAP_AUTO_ASSEMBLE;
	sd->sd_max_wu = SR_RAID0_NOWU;

	/* Setup discipline pointers. */
	sd->sd_create = sr_raid0_create;
	sd->sd_assemble = sr_raid0_assemble;
	sd->sd_alloc_resources = sr_raid0_alloc_resources;
	sd->sd_free_resources = sr_raid0_free_resources;
	sd->sd_start_discipline = NULL;
	sd->sd_scsi_inquiry = sr_raid_inquiry;
	sd->sd_scsi_read_cap = sr_raid_read_cap;
	sd->sd_scsi_tur = sr_raid_tur;
	sd->sd_scsi_req_sense = sr_raid_request_sense;
	sd->sd_scsi_start_stop = sr_raid_start_stop;
	sd->sd_scsi_sync = sr_raid_sync;
	sd->sd_scsi_rw = sr_raid0_rw;
	sd->sd_set_chunk_state = sr_raid0_set_chunk_state;
	sd->sd_set_vol_state = sr_raid0_set_vol_state;
}

int
sr_raid0_create(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk, int64_t coerced_size)
{

	if (no_chunk < 2)
		return EINVAL;

	/*
	 * XXX add variable strip size later even though MAXPHYS is really
	 * the clever value, users like to tinker with that type of stuff.
	 */
	strlcpy(sd->sd_name, "RAID 0", sizeof(sd->sd_name));
	sd->sd_meta->ssdi.ssd_strip_size = MAXPHYS;
	sd->sd_meta->ssdi.ssd_size = (coerced_size &
	    ~((sd->sd_meta->ssdi.ssd_strip_size >> DEV_BSHIFT) - 1)) * no_chunk;

	sd->sd_max_ccb_per_wu =
	    (MAXPHYS / sd->sd_meta->ssdi.ssd_strip_size + 1) *
	    SR_RAID0_NOWU * no_chunk;

	return 0;
}

int
sr_raid0_assemble(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunks)
{

	sd->sd_max_ccb_per_wu =
	    (MAXPHYS / sd->sd_meta->ssdi.ssd_strip_size + 1) *
	    SR_RAID0_NOWU * sd->sd_meta->ssdi.ssd_chunk_no;

	return 0;
}

int
sr_raid0_alloc_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_raid0_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	if (sr_wu_alloc(sd))
		goto bad;
	if (sr_ccb_alloc(sd))
		goto bad;

	/* setup runtime values */
	sd->mds.mdd_raid0.sr0_strip_bits =
	    sr_validate_stripsize(sd->sd_meta->ssdi.ssd_strip_size);
	if (sd->mds.mdd_raid0.sr0_strip_bits == -1)
		goto bad;

	rv = 0;
bad:
	return (rv);
}

int
sr_raid0_free_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_raid0_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_wu_free(sd);
	sr_ccb_free(sd);

	rv = 0;
	return (rv);
}

void
sr_raid0_set_chunk_state(struct sr_discipline *sd, int c, int new_state)
{
	int			old_state, s;

	DNPRINTF(SR_D_STATE, "%s: %s: %s: sr_raid_set_chunk_state %d -> %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
	    sd->sd_vol.sv_chunks[c]->src_meta.scmi.scm_devname, c, new_state);

	/* ok to go to splbio since this only happens in error path */
	s = splbio();
	old_state = sd->sd_vol.sv_chunks[c]->src_meta.scm_status;

	/* multiple IOs to the same chunk that fail will come through here */
	if (old_state == new_state)
		goto done;

	switch (old_state) {
	case BIOC_SDONLINE:
		if (new_state == BIOC_SDOFFLINE)
			break;
		else
			goto die;
		break;

	case BIOC_SDOFFLINE:
		goto die;

	default:
die:
		splx(s); /* XXX */
		panic("%s: %s: %s: invalid chunk state transition "
		    "%d -> %d\n", DEVNAME(sd->sd_sc),
		    sd->sd_meta->ssd_devname,
		    sd->sd_vol.sv_chunks[c]->src_meta.scmi.scm_devname,
		    old_state, new_state);
		/* NOTREACHED */
	}

	sd->sd_vol.sv_chunks[c]->src_meta.scm_status = new_state;
	sd->sd_set_vol_state(sd);

	sd->sd_must_flush = 1;
	workq_add_task(NULL, 0, sr_meta_save_callback, sd, NULL);
done:
	splx(s);
}

void
sr_raid0_set_vol_state(struct sr_discipline *sd)
{
	int			states[SR_MAX_STATES];
	int			new_state, i, s, nd;
	int			old_state = sd->sd_vol_status;

	DNPRINTF(SR_D_STATE, "%s: %s: sr_raid_set_vol_state\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname);

	nd = sd->sd_meta->ssdi.ssd_chunk_no;

	for (i = 0; i < SR_MAX_STATES; i++)
		states[i] = 0;

	for (i = 0; i < nd; i++) {
		s = sd->sd_vol.sv_chunks[i]->src_meta.scm_status;
		if (s >= SR_MAX_STATES)
			panic("%s: %s: %s: invalid chunk state",
			    DEVNAME(sd->sd_sc),
			    sd->sd_meta->ssd_devname,
			    sd->sd_vol.sv_chunks[i]->src_meta.scmi.scm_devname);
		states[s]++;
	}

	if (states[BIOC_SDONLINE] == nd)
		new_state = BIOC_SVONLINE;
	else 
		new_state = BIOC_SVOFFLINE;

	DNPRINTF(SR_D_STATE, "%s: %s: sr_raid_set_vol_state %d -> %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
	    old_state, new_state);

	switch (old_state) {
	case BIOC_SVONLINE:
		if (new_state == BIOC_SVOFFLINE || new_state == BIOC_SVONLINE)
			break;
		else
			goto die;
		break;

	case BIOC_SVOFFLINE:
		/* XXX this might be a little too much */
		goto die;

	default:
die:
		panic("%s: %s: invalid volume state transition "
		    "%d -> %d\n", DEVNAME(sd->sd_sc),
		    sd->sd_meta->ssd_devname,
		    old_state, new_state);
		/* NOTREACHED */
	}

	sd->sd_vol_status = new_state;
}

int
sr_raid0_rw(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_ccb		*ccb;
	struct sr_chunk		*scp;
	int			s;
	daddr64_t		blk, lbaoffs, strip_no, chunk, stripoffs;
	daddr64_t		strip_size, no_chunk, chunkoffs, physoffs;
	daddr64_t		strip_bits, length, leftover;
	u_int8_t		*data;

	/* blk and scsi error will be handled by sr_validate_io */
	if (sr_validate_io(wu, &blk, "sr_raid0_rw"))
		goto bad;

	strip_size = sd->sd_meta->ssdi.ssd_strip_size;
	strip_bits = sd->mds.mdd_raid0.sr0_strip_bits;
	no_chunk = sd->sd_meta->ssdi.ssd_chunk_no;

	DNPRINTF(SR_D_DIS, "%s: %s: front end io: lba %lld size %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
	    blk, xs->datalen);

	/* all offs are in bytes */
	lbaoffs = blk << DEV_BSHIFT;
	strip_no = lbaoffs >> strip_bits;
	chunk = strip_no % no_chunk;
	stripoffs = lbaoffs & (strip_size - 1);
	chunkoffs = (strip_no / no_chunk) << strip_bits;
	physoffs = chunkoffs + stripoffs + (SR_DATA_OFFSET << DEV_BSHIFT);
	length = MIN(xs->datalen, strip_size - stripoffs);
	leftover = xs->datalen;
	data = xs->data;
	for (wu->swu_io_count = 1;; wu->swu_io_count++) {
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

		DNPRINTF(SR_D_DIS, "%s: %s raid io: lbaoffs: %lld "
		    "strip_no: %lld chunk: %lld stripoffs: %lld "
		    "chunkoffs: %lld physoffs: %lld length: %lld "
		    "leftover: %lld data: %p\n",
		    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname, lbaoffs,
		    strip_no, chunk, stripoffs, chunkoffs, physoffs, length,
		    leftover, data);

		ccb->ccb_buf.b_flags = B_CALL | B_PHYS;
		ccb->ccb_buf.b_iodone = sr_raid0_intr;
		ccb->ccb_buf.b_blkno = physoffs >> DEV_BSHIFT;
		ccb->ccb_buf.b_bcount = length;
		ccb->ccb_buf.b_bufsize = length;
		ccb->ccb_buf.b_resid = length;
		ccb->ccb_buf.b_data = data;
		ccb->ccb_buf.b_error = 0;
		ccb->ccb_buf.b_proc = curproc;
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

		DNPRINTF(SR_D_DIS, "%s: %s: sr_raid0: b_bcount: %d "
		    "b_blkno: %lld b_flags 0x%0x b_data %p\n",
		    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
		    ccb->ccb_buf.b_bcount, ccb->ccb_buf.b_blkno,
		    ccb->ccb_buf.b_flags, ccb->ccb_buf.b_data);

		leftover -= length;
		if (leftover == 0)
			break;

		data += length;
		if (++chunk > no_chunk - 1) {
			chunk = 0;
			physoffs += length;
		} else if (wu->swu_io_count == 1)
			physoffs -= stripoffs;
		length = MIN(leftover,strip_size);
	}

	s = splbio();

	if (sr_check_io_collision(wu))
		goto queued;

	sr_raid_startwu(wu);
queued:
	splx(s);
	return (0);
bad:
	/* wu is unwound by sr_wu_put */
	return (1);
}

void
sr_raid0_intr(struct buf *bp)
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

		/* do not change the order of these 2 functions */
		sr_wu_put(wu);
		sr_scsi_done(sd, xs);

		if (sd->sd_sync && sd->sd_wu_pending == 0)
			wakeup(sd);
	}

	splx(s);
	return;
bad:
	xs->error = XS_DRIVER_STUFFUP;
	sr_wu_put(wu);
	sr_scsi_done(sd, xs);
	splx(s);
}
