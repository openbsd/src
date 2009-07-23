/* $OpenBSD: softraid_raid6.c,v 1.1 2009/07/23 15:15:26 jordan Exp $ */
/*
 * Copyright (c) 2009 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2009 Jordan Hargrave <jordan@openbsd.org>
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

uint8_t	gf_pow[512], gf_log[256];

/* RAID 6 functions. */
int	sr_raid6_alloc_resources(struct sr_discipline *);
int	sr_raid6_free_resources(struct sr_discipline *);
int	sr_raid6_rw(struct sr_workunit *);
int	sr_raid6_openings(struct sr_discipline *);
void	sr_raid6_intr(struct buf *);
void	sr_raid6_recreate_wu(struct sr_workunit *);
void	sr_raid6_set_chunk_state(struct sr_discipline *, int, int);
void	sr_raid6_set_vol_state(struct sr_discipline *);

void	sr_raid6_xorp(void *, void *, int);
void	sr_raid6_xorq(void *, void *, int, int);
int	sr_raid6_addio(struct sr_workunit *wu, int, daddr64_t, daddr64_t,
	    void *, int, int, void *, void *, int);
void 	sr_dump(void *, int);
void	sr_raid6_scrub(struct sr_discipline *);

void	*sr_get_block(struct sr_discipline *, int);
void	sr_put_block(struct sr_discipline *, void *);

void	gf_init(void);
uint8_t	gf_mul(uint8_t, uint8_t);
uint8_t gf_inv(uint8_t);

#define SR_NOFAIL		0x00
#define SR_FAILX		(1L << 0)
#define SR_FAILY		(1L << 1)
#define SR_FAILP		(1L << 2)
#define SR_FAILQ		(1L << 3)

struct sr_raid6_opaque {
	int      gn;
	void	*pbuf;
	void	*qbuf;
};

/* discipline initialisation. */
void
sr_raid6_discipline_init(struct sr_discipline *sd)
{
	/* Initialize GF256 tables */
	gf_init();

	/* fill out discipline members. */
	sd->sd_max_ccb_per_wu = max(6, 2 * sd->sd_meta->ssdi.ssd_chunk_no); /* only if stripsize <= MAXPHYS */
	sd->sd_max_wu = SR_RAID6_NOWU;
	sd->sd_rebuild = 0;

	/* setup discipline pointers. */
	sd->sd_alloc_resources = sr_raid6_alloc_resources;
	sd->sd_free_resources = sr_raid6_free_resources;
	sd->sd_start_discipline = NULL;
	sd->sd_scsi_inquiry = sr_raid_inquiry;
	sd->sd_scsi_read_cap = sr_raid_read_cap;
	sd->sd_scsi_tur = sr_raid_tur;
	sd->sd_scsi_req_sense = sr_raid_request_sense;
	sd->sd_scsi_start_stop = sr_raid_start_stop;
	sd->sd_scsi_sync = sr_raid_sync;
	sd->sd_scsi_rw = sr_raid6_rw;
	sd->sd_set_chunk_state = sr_raid6_set_chunk_state;
	sd->sd_set_vol_state = sr_raid6_set_vol_state;
	sd->sd_openings = sr_raid6_openings;
}

int
sr_raid6_openings(struct sr_discipline *sd)
{
	return (sd->sd_max_wu >> 1); /* 2 wu's per IO */
}

int
sr_raid6_alloc_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_raid6_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	if (sr_wu_alloc(sd))
		goto bad;
	if (sr_ccb_alloc(sd))
		goto bad;

	/* setup runtime values */
	sd->mds.mdd_raid6.sr6_strip_bits =
	    sr_validate_stripsize(sd->sd_meta->ssdi.ssd_strip_size);
	if (sd->mds.mdd_raid6.sr6_strip_bits == -1)
		goto bad;

	rv = 0;
bad:
	return (rv);
}

int
sr_raid6_free_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_raid6_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_wu_free(sd);
	sr_ccb_free(sd);

	rv = 0;
	return (rv);
}

void
sr_raid6_set_chunk_state(struct sr_discipline *sd, int c, int new_state)
{
	int			old_state, s;

	/* XXX this is for RAID 0 */
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
		if (new_state == BIOC_SDREBUILD) {
			;
		} else
			goto die;
		break;

	case BIOC_SDSCRUB:
		switch (new_state) {
		case BIOC_SDONLINE:
		case BIOC_SDOFFLINE:
			break;
		default:
			goto die;
		}
		break;

	case BIOC_SDREBUILD:
		switch (new_state) {
		case BIOC_SDONLINE:
		case BIOC_SDOFFLINE:
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
sr_raid6_set_vol_state(struct sr_discipline *sd)
{
	int			states[SR_MAX_STATES];
	int			new_state, i, s, nd;
	int			old_state = sd->sd_vol_status;

	/* XXX this is for RAID 0 */

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
	else if (states[BIOC_SDONLINE] < nd - 2)
		new_state = BIOC_SVOFFLINE;
	else if (states[BIOC_SDOFFLINE] == nd - 2)
		new_state = BIOC_SVDEGRADED;
	else if (states[BIOC_SDSCRUB] != 0)
		new_state = BIOC_SVSCRUB;
	else if (states[BIOC_SDREBUILD] != 0)
		new_state = BIOC_SVREBUILD;
	else {
		printf("old_state = %d, ", old_state);
		for (i = 0; i < nd; i++)
			printf("%d = %d, ", i,
			    sd->sd_vol.sv_chunks[i]->src_meta.scm_status);
		panic("invalid new_state");
	}

	DNPRINTF(SR_D_STATE, "%s: %s: sr_raid_set_vol_state %d -> %d\n",
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
		panic("%s: %s: invalid volume state transition %d -> %d\n",
		    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
		    old_state, new_state);
		/* NOTREACHED */
	}

	sd->sd_vol_status = new_state;
}

/*  modes:
 *   readq: sr_raid6_addio(i, lba, length, NULL, SCSI_DATA_IN, 
 *	        SR_CCBF_FREEBUF, qbuf, NULL, 0);
 *   readp: sr_raid6_addio(i, lba, length, NULL, SCSI_DATA_IN,
 *		SR_CCBF_FREEBUF, pbuf, NULL, 0);
 *   readx: sr_raid6_addio(i, lba, length, NULL, SCSI_DATA_IN,
 *		SR_CCBF_FREEBUF, pbuf, qbuf, gf_pow[i]);
 */

int
sr_raid6_rw(struct sr_workunit *wu)
{
	struct sr_workunit	*wu_w = NULL;
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_chunk		*scp;
	int			s, fail, i;
	daddr64_t		blk, lbaoffs, strip_no, chunk, qchunk, pchunk, fchunk;
	daddr64_t		strip_size, no_chunk, lba, chunk_offs, phys_offs;
	daddr64_t		strip_bits, length, strip_offs, datalen;
	void		        *pbuf, *data, *qbuf;

	/* blk and scsi error will be handled by sr_validate_io */
	if (sr_validate_io(wu, &blk, "sr_raid6_rw"))
		goto bad;

	strip_size = sd->sd_meta->ssdi.ssd_strip_size;
	strip_bits = sd->mds.mdd_raid6.sr6_strip_bits;
	no_chunk = sd->sd_meta->ssdi.ssd_chunk_no - 2;

	data = xs->data;
	datalen = xs->datalen;
	lbaoffs	= blk << DEV_BSHIFT;

	if (xs->flags & SCSI_DATA_OUT)
		/* create write workunit */
		if ((wu_w = sr_wu_get(sd, 0)) == NULL) {
			printf("%s: can't get wu_w", DEVNAME(sd->sd_sc));
			goto bad;
		}

	wu->swu_blk_start = 0;
	while (datalen != 0) {
		strip_no = lbaoffs >> strip_bits;
		strip_offs = lbaoffs & (strip_size - 1);
		chunk_offs = (strip_no / no_chunk) << strip_bits;
		phys_offs = chunk_offs + strip_offs + 
		    ((SR_META_OFFSET + SR_META_SIZE) << DEV_BSHIFT);

		/* get size remaining in this stripe */
		length = MIN(strip_size - strip_offs, datalen);

		/* map disk offset to parity/data drive */	
		chunk = strip_no % no_chunk;

		qchunk = (no_chunk + 1) - ((strip_no / no_chunk) % (no_chunk+2));
		if (qchunk == 0)
			pchunk = no_chunk + 1;
		else
			pchunk = qchunk - 1;
		if (chunk >= pchunk)
			chunk++;
		if (chunk >= qchunk)
			chunk++;

		lba = phys_offs >> DEV_BSHIFT;
	
		/* XXX big hammer.. exclude I/O from entire stripe */
		if (wu->swu_blk_start == 0)
			wu->swu_blk_start = chunk_offs >> DEV_BSHIFT;
		wu->swu_blk_end = ((chunk_offs + (no_chunk << strip_bits)) >> DEV_BSHIFT) - 1;

		fail = 0;

		/* Get P-fail flag */
		scp = sd->sd_vol.sv_chunks[pchunk];
		switch (scp->src_meta.scm_status) {
		case BIOC_SDOFFLINE:
		case BIOC_SDREBUILD:
		case BIOC_SDHOTSPARE:
			fail |= SR_FAILP;
			break;
		}

		/* Get Q-fail flag */
		scp = sd->sd_vol.sv_chunks[qchunk];
		switch (scp->src_meta.scm_status) {
		case BIOC_SDOFFLINE:
		case BIOC_SDREBUILD:
		case BIOC_SDHOTSPARE:
			fail |= SR_FAILQ;
			break;
		}

		/* Get disk-fail flag */	
		scp = sd->sd_vol.sv_chunks[chunk];
		switch (scp->src_meta.scm_status) {
		case BIOC_SDOFFLINE:
		case BIOC_SDREBUILD:
		case BIOC_SDHOTSPARE:
			fail |= SR_FAILX;

			/* Check for dual-drive failure */
			if (!(fail & (SR_FAILP|SR_FAILQ)) && 
			    (sd->sd_vol_status == BIOC_SVDEGRADED))
			    fail |= SR_FAILY;
			break;
		}

		if (xs->flags & SCSI_DATA_IN) {	
			switch (fail) {
			case SR_NOFAIL:
				/* drive is good. issue single read request */
				if (sr_raid6_addio(wu, chunk, lba, length,
				    data, xs->flags, 0, NULL, NULL, 0))
					goto bad;
				break;
			case SR_FAILX:
			case SR_FAILX+SR_FAILQ:
				/* Dx, (Q) failed: Dx = Dz ^ P (same as RAID5) */
				printf("Disk %llx offline, "
				    "regenerating Dx+Q\n", chunk);

				/* Calculate: Dx = P^Dz
 				 *   P:  sr_raid6_xorp(data, ---, length); 
 				 *   Dz: sr_raid6_xorp(data, ---, length); 
				 */
				memset(data, 0, length);
				for (i = 0; i < no_chunk+2; i++) {
					if (i != chunk && i != qchunk) {
						/* Read Dz */
						if (sr_raid6_addio(wu, i, lba, length,
					    	    NULL, SCSI_DATA_IN, SR_CCBF_FREEBUF, 
					    	    data, NULL, 0))
	 				    	    goto bad;
					}
				}
				break;
			case SR_FAILX+SR_FAILP:
				/* Dx, P failed */
				printf("Disk %llx offline, "
				    "regenerating Dx+P\n", chunk);

				pbuf = sr_get_block(sd, length);
				if (pbuf == NULL)
					goto bad;

				/* Calculate: Dx*gx = Q^(Dz*gz)
				 *   Q:  sr_raid6_xorp(data, --, length);
				 *   Dz: sr_raid6_xorq(data, --, length, gf_pow[i]);
				 */
				memset(data, 0, length);
				for (i = 0; i < no_chunk+2; i++) {
					if  (i == qchunk) {
						/* Read Q */
						if (sr_raid6_addio(wu, i, lba, length,
						    NULL, SCSI_DATA_IN, SR_CCBF_FREEBUF,
						    pbuf, NULL, 0))
						    goto bad;
					} else if (i != chunk && i != pchunk) {
						/* Read Dz * gz */
						if (sr_raid6_addio(wu, i, lba, length,
						   NULL, SCSI_DATA_IN, SR_CCBF_FREEBUF,
						   NULL, pbuf, gf_pow[i]))
						   goto bad;
					}
				}

				/* XXX: bag of fail */
				wu->swu_flags |= SR_WUF_FAIL;
				sr_raid_startwu(wu);
				while ((wu->swu_flags & SR_WUF_FAILIOCOMP) == 0) {
					tsleep(wu, PRIBIO, "sr_getdata", 0);
				}

				/* On completion, pbuf = Dx*gx */
				sr_raid6_xorq(data, pbuf, length, gf_inv(gf_pow[chunk]));
				sr_put_block(sd, pbuf);

				sr_wu_put(wu);
				scsi_done(xs);
				return(0);

				break;
			case SR_FAILX+SR_FAILY:
				/* Dx, Dy failed */

				/* cheat.. get other failed drive */
				for (fchunk=0; fchunk<no_chunk+2; fchunk++) {
					if (fchunk != chunk && fchunk != qchunk && fchunk != pchunk)
						break;
				}

				printf("Disk %llx & %llx offline, "
				    "regenerating Dx+Dy\n", chunk, fchunk);
				qbuf = sr_get_block(sd, length);
				if (qbuf == NULL)
					goto bad;
				pbuf = sr_get_block(sd, length);
				if (pbuf == NULL)
					goto bad;

				/* Calculate: Dx*gx^Dy*gy = Q^(Dz*gz) ; Dx^Dy = P^Dz
				 *   Q:  sr_raid6_xorp(qbuf, --, length);
				 *   P:  sr_raid6_xorp(pbuf, --, length);
				 *   Dz: sr_raid6_xorp(pbuf, --, length);
				 *	 sr_raid6_xorq(qbuf, --, length, gf_pow[i]);
				 */
				memset(data, 0, length);
				for (i = 0; i < no_chunk+2; i++) {
					if (i == qchunk) {
						/* read Q */
						if (sr_raid6_addio(wu, i, lba, length,
						    NULL, SCSI_DATA_IN, SR_CCBF_FREEBUF,
						    qbuf, NULL, 0))
						    goto bad;
					} else if (i == pchunk) {
						/* read P */
						if (sr_raid6_addio(wu, i, lba, length,
						    NULL, SCSI_DATA_IN, SR_CCBF_FREEBUF,
						    pbuf, NULL, 0))
						    goto bad;
					} else if (i != chunk) {
						/* read Dz * gz */
						if (sr_raid6_addio(wu, i, lba, length,
						    NULL, SCSI_DATA_IN, SR_CCBF_FREEBUF,
						    pbuf, qbuf, gf_pow[i]))
						    goto bad;
					}
				}


				/* XXX: bag of fail */
				wu->swu_flags |= SR_WUF_FAIL;
				sr_raid_startwu(wu);
				while ((wu->swu_flags & SR_WUF_FAILIOCOMP) == 0) {
					tsleep(wu, PRIBIO, "sr_getdata", 0);
				}

				/* On completion, pbuf = Dx ^ Dy; qbuf = Dx*gx ^ Dy*gy */
				sr_raid6_xorq(data, qbuf, length, 
				    gf_inv(gf_pow[chunk] ^ gf_pow[fchunk]));
				sr_raid6_xorq(data, pbuf, length,
				    gf_inv(gf_pow[255+chunk-fchunk] ^ 1));	// Dx

				sr_put_block(sd, pbuf);
				sr_put_block(sd, qbuf);

				sr_wu_put(wu);
				scsi_done(xs);
				return(0);

				break;
			default:
				printf("%s: is offline, can't read\n",
				    DEVNAME(sd->sd_sc));
				goto bad;
			}
		} else {
			/* XXX handle writes to failed/offline disk? */
			if (scp->src_meta.scm_status == BIOC_SDOFFLINE)
				goto bad;

			/*
			 * initialize pbuf with contents of new data to be
			 * written. This will be XORed with old data and old
			 * parity in the intr routine. The result in pbuf
			 * is the new parity data.
			 */
			qbuf = sr_get_block(sd, length);
			if (qbuf == NULL)
				goto bad;

			pbuf = sr_get_block(sd, length);
			if (pbuf == NULL)
				goto bad;

			/* Calulate P = Dn; Q = gn * Dn */
			sr_raid6_xorp(pbuf, data, length);
			sr_raid6_xorq(qbuf, data, length, gf_pow[chunk]);

			/* Read old data: P ^= Dn' ; Q ^= (gn * Dn') */
			if (sr_raid6_addio(wu, chunk, lba, length, NULL,
				SCSI_DATA_IN, SR_CCBF_FREEBUF, pbuf, qbuf, 
				gf_pow[chunk]))
				goto bad;

			/* Read old xor-parity: P ^= P' */
			if (sr_raid6_addio(wu, pchunk, lba, length, NULL,
				SCSI_DATA_IN, SR_CCBF_FREEBUF, pbuf, NULL, 0))
				goto bad;

			/* Read old q-parity: Q ^= Q' */
			if (sr_raid6_addio(wu, qchunk, lba, length, NULL,
				SCSI_DATA_IN, SR_CCBF_FREEBUF, qbuf, NULL, 0))
				goto bad;

			/* write new data */
			if (sr_raid6_addio(wu_w, chunk, lba, length, data,
			    xs->flags, 0, NULL, NULL, 0))
				goto bad;

			/* write new xor-parity */
			if (sr_raid6_addio(wu_w, pchunk, lba, length, pbuf,
			    xs->flags, SR_CCBF_FREEBUF, NULL, NULL, 0))
				goto bad;

			/* write new q-parity */
			if (sr_raid6_addio(wu_w, qchunk, lba, length, qbuf,
			    xs->flags, SR_CCBF_FREEBUF, NULL, NULL, 0))
				goto bad;
		}

		/* advance to next block */
		lbaoffs += length;
		datalen -= length;
		data += length;
	}

	s = splbio();
	if (wu_w) {
		/* collide write request with reads */
		wu_w->swu_blk_start = wu->swu_blk_start;
		wu_w->swu_blk_end = wu->swu_blk_end;

		/*
		 * put xs block in write request (scsi_done not called till
		 * write completes)
		 */
		wu_w->swu_xs = wu->swu_xs;
		wu->swu_xs = NULL;

		wu_w->swu_state = SR_WU_DEFERRED;
		wu->swu_collider = wu_w;
		TAILQ_INSERT_TAIL(&sd->sd_wu_defq, wu_w, swu_link);
	}

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
	if (wu_w)
		sr_wu_put(wu_w);
	return (1);
}

void
sr_raid6_intr(struct buf *bp)
{
	struct sr_ccb		*ccb = (struct sr_ccb *)bp;
	struct sr_workunit	*wu = ccb->ccb_wu, *wup;
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_softc		*sc = sd->sd_sc;
	struct sr_raid6_opaque  *pq = ccb->ccb_opaque;
	int			s, pend;

	DNPRINTF(SR_D_INTR, "%s: sr_intr bp %p xs %p\n",
	    DEVNAME(sc), bp, xs);

	DNPRINTF(SR_D_INTR, "%s: sr_intr: b_bcount: %d b_resid: %d"
	    " b_flags: 0x%0x block: %lld target: %d\n", DEVNAME(sc),
	    ccb->ccb_buf.b_bcount, ccb->ccb_buf.b_resid, ccb->ccb_buf.b_flags,
	    ccb->ccb_buf.b_blkno, ccb->ccb_target);

	s = splbio();

	if (ccb->ccb_buf.b_flags & B_ERROR) {
		DNPRINTF(SR_D_INTR, "%s: i/o error on block %lld target: %d\n",
		    DEVNAME(sc), ccb->ccb_buf.b_blkno, ccb->ccb_target);
		printf("io error: disk %x\n", ccb->ccb_target);
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

		/* XOR data to result */
		if (pq) {
			if (pq->pbuf)
				/* Calculate xor-parity */
				sr_raid6_xorp(pq->pbuf, ccb->ccb_buf.b_data,
				    ccb->ccb_buf.b_bcount);
			if (pq->qbuf)
				/* Calculate q-parity */
				sr_raid6_xorq(pq->qbuf, ccb->ccb_buf.b_data,
				    ccb->ccb_buf.b_bcount, pq->gn);
			free(pq, M_DEVBUF);
			ccb->ccb_opaque = NULL;
		}
	}

	/* free allocated data buffer */
	if (ccb->ccb_flag & SR_CCBF_FREEBUF) {
		sr_put_block(sd, ccb->ccb_buf.b_data);
		ccb->ccb_buf.b_data = NULL;
	}
	wu->swu_ios_complete++;

	DNPRINTF(SR_D_INTR, "%s: sr_intr: comp: %d count: %d failed: %d\n",
	    DEVNAME(sc), wu->swu_ios_complete, wu->swu_io_count,
	    wu->swu_ios_failed);

	if (wu->swu_ios_complete >= wu->swu_io_count) {

		/* if all ios failed, retry reads and give up on writes */
		if (wu->swu_ios_failed == wu->swu_ios_complete) {
			if (xs->flags & SCSI_DATA_IN) {
				printf("%s: retrying read on block %lld\n",
				    DEVNAME(sc), ccb->ccb_buf.b_blkno);
				sr_ccb_put(ccb);
				TAILQ_INIT(&wu->swu_ccb);
				wu->swu_state = SR_WU_RESTART;
				if (sd->sd_scsi_rw(wu))
					goto bad;
				else
					goto retry;
			} else {
				printf("%s: permanently fail write on block "
				    "%lld\n", DEVNAME(sc),
				    ccb->ccb_buf.b_blkno);
				xs->error = XS_DRIVER_STUFFUP;
				goto bad;
			}
		}

		if (xs != NULL) {
			xs->error = XS_NOERROR;
			xs->resid = 0;
			xs->flags |= ITSDONE;
		}

		pend = 0;
		TAILQ_FOREACH(wup, &sd->sd_wu_pendq, swu_link) {
			if (wu == wup) {
				/* wu on pendq, remove */
				TAILQ_REMOVE(&sd->sd_wu_pendq, wu, swu_link);
				pend = 1;

				if (wu->swu_collider) {
					if (wu->swu_ios_failed)
						/* toss all ccbs and recreate */
						sr_raid6_recreate_wu(wu->swu_collider);

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

		if (wu->swu_flags & SR_WUF_FAIL) {
			wu->swu_flags |= SR_WUF_FAILIOCOMP;
			wakeup(wu);
		}
		else if (wu->swu_flags & SR_WUF_REBUILD) {
			if (wu->swu_xs->flags & SCSI_DATA_OUT) {
				wu->swu_flags |= SR_WUF_REBUILDIOCOMP;
				wakeup(wu);
			}
		} else {
			/* do not change the order of these 2 functions */
			sr_wu_put(wu);
			if (xs != NULL)
				scsi_done(xs);
		}

		if (sd->sd_sync && sd->sd_wu_pending == 0)
			wakeup(sd);
	}

retry:
	splx(s);
	return;
bad:
	xs->error = XS_DRIVER_STUFFUP;
	xs->flags |= ITSDONE;
	if (wu->swu_flags & SR_WUF_REBUILD) {
		wu->swu_flags |= SR_WUF_REBUILDIOCOMP;
		wakeup(wu);
	} else {
		/* do not change the order of these 2 functions */
		sr_wu_put(wu);
		scsi_done(xs);
	}

	splx(s);
}

void
sr_raid6_recreate_wu(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct sr_workunit	*wup = wu;
	struct sr_ccb		*ccb;

	do {
		DNPRINTF(SR_D_INTR, "%s: sr_raid6_recreate_wu: %p\n", wup);

		/* toss all ccbs */
		while ((ccb = TAILQ_FIRST(&wup->swu_ccb)) != NULL) {
			TAILQ_REMOVE(&wup->swu_ccb, ccb, ccb_link);
			sr_ccb_put(ccb);
		}
		TAILQ_INIT(&wup->swu_ccb);

		/* recreate ccbs */
		wup->swu_state = SR_WU_REQUEUE;
		if (sd->sd_scsi_rw(wup))
			panic("could not requeue io");

		wup = wup->swu_collider;
	} while (wup);
}

int
sr_raid6_addio(struct sr_workunit *wu, int dsk, daddr64_t blk, daddr64_t len,
    void *data, int flag, int ccbflag, void *pbuf, void *qbuf, int gn)
{
	struct sr_discipline 	*sd = wu->swu_dis;
	struct sr_ccb		*ccb;
	struct sr_raid6_opaque  *pqbuf;

	ccb = sr_ccb_get(sd);
	if (!ccb)
		return (-1);

	/* allocate temporary buffer */
	if (data == NULL) {
		data = sr_get_block(sd, len);
		if (data == NULL)
			return (-1);
	}

	DNPRINTF(0, "%sio: %d.%llx %llx %p:%p\n",
	    flag & SCSI_DATA_IN ? "read" : "write",
	    dsk, blk, len, pbuf, qbuf);

	ccb->ccb_flag = ccbflag;
	if (flag & SCSI_POLL) {
		ccb->ccb_buf.b_flags = 0;
		ccb->ccb_buf.b_iodone = NULL;
	} else {
		ccb->ccb_buf.b_flags = B_CALL;
		ccb->ccb_buf.b_iodone = sr_raid6_intr;
	}
	if (flag & SCSI_DATA_IN)
		ccb->ccb_buf.b_flags |= B_READ;
	else
		ccb->ccb_buf.b_flags |= B_WRITE;

	/* add offset for metadata */
	ccb->ccb_buf.b_flags |= B_PHYS;
	ccb->ccb_buf.b_blkno = blk;
	ccb->ccb_buf.b_bcount = len;
	ccb->ccb_buf.b_bufsize = len;
	ccb->ccb_buf.b_resid = len;
	ccb->ccb_buf.b_data = data;
	ccb->ccb_buf.b_error = 0;
	ccb->ccb_buf.b_proc = curproc;
	ccb->ccb_buf.b_dev = sd->sd_vol.sv_chunks[dsk]->src_dev_mm;
	ccb->ccb_buf.b_vp = NULL;

	ccb->ccb_wu = wu;
	ccb->ccb_target = dsk;
	if (pbuf || qbuf) {
		pqbuf = malloc(sizeof(struct sr_raid6_opaque), M_DEVBUF, M_CANFAIL);
		if (pqbuf == NULL) {
			sr_ccb_put(ccb);
			return (-1);
		}
		pqbuf->pbuf = pbuf;
		pqbuf->qbuf = qbuf;
		pqbuf->gn = gn;
		ccb->ccb_opaque = pqbuf;
	}

	LIST_INIT(&ccb->ccb_buf.b_dep);
	TAILQ_INSERT_TAIL(&wu->swu_ccb, ccb, ccb_link);

	DNPRINTF(SR_D_DIS, "%s: %s: sr_raid6: b_bcount: %d "
	    "b_blkno: %x b_flags 0x%0x b_data %p\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
	    ccb->ccb_buf.b_bcount, ccb->ccb_buf.b_blkno,
	    ccb->ccb_buf.b_flags, ccb->ccb_buf.b_data);

	wu->swu_io_count++;

	return (0);
}

/* Perform RAID6 parity calculation.
 *   P=xor parity, Q=GF256 parity, D=data, gn=disk# */
void
sr_raid6_xorp(void *p, void *d, int len)
{
	uint32_t	*pbuf = p, *data = d;

	/* Faster, X bytes at a time */
	len >>= 4;
	while (len--)
		pbuf[len] ^= data[len];
}

void
sr_raid6_xorq(void *q, void *d, int len, int gn)
{
	uint8_t		*qbuf = q, *data = d;

	/* Have to do this a byte at a time */
	while (len--)
		qbuf[len] ^= gf_mul(data[len], gn);
}

/* Create GF256 log/pow tables: polynomial = 0x11D */
void
gf_init(void)
{
	int i;
	uint8_t p = 1;

	/* use 2N pow table to avoid using % in multiply */
	for (i=0; i<256; i++) {
		gf_log[p] = i;
		gf_pow[i] = gf_pow[i+255] = p;
		p = ((p << 1) ^ ((p & 0x80) ? 0x1D : 0x00));
	}
}

/* GF256 multiplication using exponent/logarithm table */
uint8_t
gf_mul(uint8_t a, uint8_t b)
{
	/* g^a * g^b = g^(a+b) */
	if (!a || !b)
		return (0);
	return gf_pow[gf_log[a] + gf_log[b]];
}

uint8_t
gf_inv(uint8_t a)
{
	return gf_pow[255 - gf_log[a]];
}

