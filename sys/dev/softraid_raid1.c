/* $OpenBSD: softraid_raid1.c,v 1.43 2013/03/27 14:30:11 jsing Exp $ */
/*
 * Copyright (c) 2007 Marco Peereboom <marco@peereboom.us>
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

/* RAID 1 functions. */
int	sr_raid1_create(struct sr_discipline *, struct bioc_createraid *,
	    int, int64_t);
int	sr_raid1_assemble(struct sr_discipline *, struct bioc_createraid *,
	    int, void *);
int	sr_raid1_alloc_resources(struct sr_discipline *);
int	sr_raid1_free_resources(struct sr_discipline *);
int	sr_raid1_rw(struct sr_workunit *);
void	sr_raid1_intr(struct buf *);
void	sr_raid1_set_chunk_state(struct sr_discipline *, int, int);
void	sr_raid1_set_vol_state(struct sr_discipline *);

/* Discipline initialisation. */
void
sr_raid1_discipline_init(struct sr_discipline *sd)
{
	/* Fill out discipline members. */
	sd->sd_type = SR_MD_RAID1;
	strlcpy(sd->sd_name, "RAID 1", sizeof(sd->sd_name));
	sd->sd_capabilities = SR_CAP_SYSTEM_DISK | SR_CAP_AUTO_ASSEMBLE |
	    SR_CAP_REBUILD | SR_CAP_REDUNDANT;
	sd->sd_max_wu = SR_RAID1_NOWU;

	/* Setup discipline specific function pointers. */
	sd->sd_alloc_resources = sr_raid1_alloc_resources;
	sd->sd_assemble = sr_raid1_assemble;
	sd->sd_create = sr_raid1_create;
	sd->sd_free_resources = sr_raid1_free_resources;
	sd->sd_scsi_rw = sr_raid1_rw;
	sd->sd_scsi_intr = sr_raid1_intr;
	sd->sd_set_chunk_state = sr_raid1_set_chunk_state;
	sd->sd_set_vol_state = sr_raid1_set_vol_state;
}

int
sr_raid1_create(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk, int64_t coerced_size)
{

	if (no_chunk < 2) {
		sr_error(sd->sd_sc, "RAID 1 requires two or more chunks");
		return EINVAL;
	}

	sd->sd_meta->ssdi.ssd_size = coerced_size;

	sd->sd_max_ccb_per_wu = no_chunk;

	return 0;
}

int
sr_raid1_assemble(struct sr_discipline *sd, struct bioc_createraid *bc,
    int no_chunk, void *data)
{

	sd->sd_max_ccb_per_wu = sd->sd_meta->ssdi.ssd_chunk_no;

	return 0;
}

int
sr_raid1_alloc_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_alloc_resources\n",
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
sr_raid1_free_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	DNPRINTF(SR_D_DIS, "%s: sr_raid1_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_wu_free(sd);
	sr_ccb_free(sd);

	rv = 0;
	return (rv);
}

void
sr_raid1_set_chunk_state(struct sr_discipline *sd, int c, int new_state)
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
		switch (new_state) {
		case BIOC_SDOFFLINE:
		case BIOC_SDSCRUB:
			break;
		default:
			goto die;
		}
		break;

	case BIOC_SDOFFLINE:
		switch (new_state) {
		case BIOC_SDREBUILD:
		case BIOC_SDHOTSPARE:
			break;
		default:
			goto die;
		}
		break;

	case BIOC_SDSCRUB:
		if (new_state == BIOC_SDONLINE) {
			;
		} else
			goto die;
		break;

	case BIOC_SDREBUILD:
		switch (new_state) {
		case BIOC_SDONLINE:
			break;
		case BIOC_SDOFFLINE:
			/* Abort rebuild since the rebuild chunk disappeared. */
			sd->sd_reb_abort = 1;
			break;
		default:
			goto die;
		}
		break;

	case BIOC_SDHOTSPARE:
		switch (new_state) {
		case BIOC_SDOFFLINE:
		case BIOC_SDREBUILD:
			break;
		default:
			goto die;
		}
		break;

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
sr_raid1_set_vol_state(struct sr_discipline *sd)
{
	int			states[SR_MAX_STATES];
	int			new_state, i, s, nd;
	int			old_state = sd->sd_vol_status;

	DNPRINTF(SR_D_STATE, "%s: %s: sr_raid_set_vol_state\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname);

	nd = sd->sd_meta->ssdi.ssd_chunk_no;

#ifdef SR_DEBUG
	for (i = 0; i < nd; i++)
		DNPRINTF(SR_D_STATE, "%s: chunk %d status = %u\n",
		    DEVNAME(sd->sd_sc), i,
		    sd->sd_vol.sv_chunks[i]->src_meta.scm_status);
#endif

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
	else if (states[BIOC_SDONLINE] == 0)
		new_state = BIOC_SVOFFLINE;
	else if (states[BIOC_SDSCRUB] != 0)
		new_state = BIOC_SVSCRUB;
	else if (states[BIOC_SDREBUILD] != 0)
		new_state = BIOC_SVREBUILD;
	else if (states[BIOC_SDOFFLINE] != 0)
		new_state = BIOC_SVDEGRADED;
	else {
		DNPRINTF(SR_D_STATE, "%s: invalid volume state, old state "
		    "was %d\n", DEVNAME(sd->sd_sc), old_state);
		panic("invalid volume state");
	}

	DNPRINTF(SR_D_STATE, "%s: %s: sr_raid1_set_vol_state %d -> %d\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
	    old_state, new_state);

	switch (old_state) {
	case BIOC_SVONLINE:
		switch (new_state) {
		case BIOC_SVONLINE: /* can go to same state */
		case BIOC_SVOFFLINE:
		case BIOC_SVDEGRADED:
		case BIOC_SVREBUILD: /* happens on boot */
			break;
		default:
			goto die;
		}
		break;

	case BIOC_SVOFFLINE:
		/* XXX this might be a little too much */
		goto die;

	case BIOC_SVSCRUB:
		switch (new_state) {
		case BIOC_SVONLINE:
		case BIOC_SVOFFLINE:
		case BIOC_SVDEGRADED:
		case BIOC_SVSCRUB: /* can go to same state */
			break;
		default:
			goto die;
		}
		break;

	case BIOC_SVBUILDING:
		switch (new_state) {
		case BIOC_SVONLINE:
		case BIOC_SVOFFLINE:
		case BIOC_SVBUILDING: /* can go to the same state */
			break;
		default:
			goto die;
		}
		break;

	case BIOC_SVREBUILD:
		switch (new_state) {
		case BIOC_SVONLINE:
		case BIOC_SVOFFLINE:
		case BIOC_SVDEGRADED:
		case BIOC_SVREBUILD: /* can go to the same state */
			break;
		default:
			goto die;
		}
		break;

	case BIOC_SVDEGRADED:
		switch (new_state) {
		case BIOC_SVOFFLINE:
		case BIOC_SVREBUILD:
		case BIOC_SVDEGRADED: /* can go to the same state */
			break;
		default:
			goto die;
		}
		break;

	default:
die:
		panic("%s: %s: invalid volume state transition "
		    "%d -> %d\n", DEVNAME(sd->sd_sc),
		    sd->sd_meta->ssd_devname,
		    old_state, new_state);
		/* NOTREACHED */
	}

	sd->sd_vol_status = new_state;

	/* If we have just become degraded, look for a hotspare. */
	if (new_state == BIOC_SVDEGRADED)
		workq_add_task(NULL, 0, sr_hotspare_rebuild_callback, sd, NULL);
}

int
sr_raid1_rw(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_ccb		*ccb;
	struct sr_chunk		*scp;
	int			ios, chunk, i, s, rt;
	daddr64_t		blk;

	/* blk and scsi error will be handled by sr_validate_io */
	if (sr_validate_io(wu, &blk, "sr_raid1_rw"))
		goto bad;

	/* calculate physical block */
	blk += sd->sd_meta->ssd_data_offset;

	if (xs->flags & SCSI_DATA_IN)
		ios = 1;
	else
		ios = sd->sd_meta->ssdi.ssd_chunk_no;

	for (i = 0; i < ios; i++) {
		if (xs->flags & SCSI_DATA_IN) {
			rt = 0;
ragain:
			/* interleave reads */
			chunk = sd->mds.mdd_raid1.sr1_counter++ %
			    sd->sd_meta->ssdi.ssd_chunk_no;
			scp = sd->sd_vol.sv_chunks[chunk];
			switch (scp->src_meta.scm_status) {
			case BIOC_SDONLINE:
			case BIOC_SDSCRUB:
				break;

			case BIOC_SDOFFLINE:
			case BIOC_SDREBUILD:
			case BIOC_SDHOTSPARE:
				if (rt++ < sd->sd_meta->ssdi.ssd_chunk_no)
					goto ragain;

				/* FALLTHROUGH */
			default:
				/* volume offline */
				printf("%s: is offline, cannot read\n",
				    DEVNAME(sd->sd_sc));
				goto bad;
			}
		} else {
			/* writes go on all working disks */
			chunk = i;
			scp = sd->sd_vol.sv_chunks[chunk];
			switch (scp->src_meta.scm_status) {
			case BIOC_SDONLINE:
			case BIOC_SDSCRUB:
			case BIOC_SDREBUILD:
				break;

			case BIOC_SDHOTSPARE: /* should never happen */
			case BIOC_SDOFFLINE:
				continue;

			default:
				goto bad;
			}
		}

		ccb = sr_ccb_rw(sd, chunk, blk, xs->datalen, xs->data,
		    xs->flags, 0);
		if (!ccb) {
			/* should never happen but handle more gracefully */
			printf("%s: %s: too many ccbs queued\n",
			    DEVNAME(sd->sd_sc),
			    sd->sd_meta->ssd_devname);
			goto bad;
		}
		sr_wu_enqueue_ccb(wu, ccb);
	}

	s = splbio();

	/* rebuild io, let rebuild routine deal with it */
	if (wu->swu_flags & SR_WUF_REBUILD)
		goto queued;

	/* current io failed, restart */
	if (wu->swu_state == SR_WU_RESTART)
		goto start;

	/* deferred io failed, don't restart */
	if (wu->swu_state == SR_WU_REQUEUE)
		goto queued;

	if (sr_check_io_collision(wu))
		goto queued;

start:
	sr_raid_startwu(wu);
queued:
	splx(s);
	return (0);
bad:
	/* wu is unwound by sr_wu_put */
	return (1);
}

void
sr_raid1_intr(struct buf *bp)
{
	struct sr_ccb		*ccb = (struct sr_ccb *)bp;
	struct sr_workunit	*wu = ccb->ccb_wu, *wup;
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_softc		*sc = sd->sd_sc;
	int			s;

	DNPRINTF(SR_D_INTR, "%s: sr_intr bp %x xs %x\n",
	    DEVNAME(sc), bp, xs);

	s = splbio();

	sr_ccb_done(ccb);

	DNPRINTF(SR_D_INTR, "%s: sr_intr: comp: %d count: %d failed: %d\n",
	    DEVNAME(sc), wu->swu_ios_complete, wu->swu_io_count,
	    wu->swu_ios_failed);

	if (wu->swu_ios_complete < wu->swu_io_count)
		goto done;

	xs->error = XS_NOERROR;

	/* if all ios failed, retry reads and give up on writes */
	if (wu->swu_ios_failed == wu->swu_ios_complete) {
		if (xs->flags & SCSI_DATA_IN) {
			printf("%s: retrying read on block %lld\n",
			    DEVNAME(sc), ccb->ccb_buf.b_blkno);
			sr_ccb_put(ccb);
			if (wu->swu_cb_active == 1)
				panic("%s: sr_raid1_intr_cb",
				    DEVNAME(sd->sd_sc));
			TAILQ_INIT(&wu->swu_ccb);
			wu->swu_state = SR_WU_RESTART;
			if (sd->sd_scsi_rw(wu) == 0)
				goto done;
			xs->error = XS_DRIVER_STUFFUP;
		} else {
			printf("%s: permanently failing write on block %lld\n",
			    DEVNAME(sc), ccb->ccb_buf.b_blkno);
			xs->error = XS_DRIVER_STUFFUP;
		}
	}

	TAILQ_FOREACH(wup, &sd->sd_wu_pendq, swu_link)
		if (wu == wup)
			break;

	if (wup == NULL)
		panic("%s: wu %p not on pending queue",
		    DEVNAME(sd->sd_sc), wu);

	/* wu on pendq, remove */
	TAILQ_REMOVE(&sd->sd_wu_pendq, wu, swu_link);

	if (wu->swu_collider) {
		if (wu->swu_ios_failed)
			sr_raid_recreate_wu(wu->swu_collider);

		/* XXX Should the collider be failed if this xs failed? */
		/* restart deferred wu */
		wu->swu_collider->swu_state = SR_WU_INPROGRESS;
		TAILQ_REMOVE(&sd->sd_wu_defq, wu->swu_collider, swu_link);
		sr_raid_startwu(wu->swu_collider);
	}

	if (wu->swu_flags & SR_WUF_REBUILD) {
		/* XXX - decouple from SCSI_DATA_OUT. */
		if (wu->swu_xs->flags & SCSI_DATA_OUT) {
			wu->swu_flags |= SR_WUF_REBUILDIOCOMP;
			wakeup(wu);
		}
	} else {
		sr_scsi_done(sd, xs);
	}

	if (sd->sd_sync && sd->sd_wu_pending == 0)
		wakeup(sd);

done:
	splx(s);
}
