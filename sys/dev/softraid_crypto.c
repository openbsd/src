/* $OpenBSD: softraid_crypto.c,v 1.3 2008/01/26 19:29:55 marco Exp $ */
/*
 * Copyright (c) 2007 Ted Unangst <tedu@openbsd.org>
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
/* RAID crypto functions */

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

#include <crypto/cryptodev.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

struct cryptop *	sr_crypto_getcryptop(struct sr_workunit *, int);
void *			sr_crypto_putcryptop(struct cryptop *);
int			sr_crypto_rw2(struct cryptop *);
void			sr_crypto_intr(struct buf *);
int			sr_crypto_intr2(struct cryptop *);

struct cryptop *
sr_crypto_getcryptop(struct sr_workunit *wu, int encrypt)
{
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_discipline	*sd = wu->swu_dis;
	struct cryptop		*crp;
	struct cryptodesc	*crd;
	struct uio		*uio;
	int			flags, i, n;
	int			blk = 0;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_getcryptop wu: %p encrypt: %d\n",
	    DEVNAME(sd->sd_sc), wu, encrypt);

	uio = malloc(sizeof(*uio), M_DEVBUF, M_WAITOK | M_ZERO);
	uio->uio_iov = malloc(sizeof(*uio->uio_iov), M_DEVBUF, M_WAITOK);
	uio->uio_iovcnt = 1;
	uio->uio_iov->iov_base = xs->data;
	uio->uio_iov->iov_len = xs->datalen;

	if (xs->cmdlen == 10)
		blk = _4btol(((struct scsi_rw_big *)xs->cmd)->addr);
	else if (xs->cmdlen == 16)
		blk = _8btol(((struct scsi_rw_16 *)xs->cmd)->addr);
	else if (xs->cmdlen == 6)
		blk = _3btol(((struct scsi_rw *)xs->cmd)->addr);

	n = xs->datalen >> 9;
	flags = (encrypt ? CRD_F_ENCRYPT : 0) |
	    CRD_F_IV_PRESENT | CRD_F_IV_EXPLICIT;

	crp = crypto_getreq(n);

	crp->crp_sid = sd->mds.mdd_crypto.src_sid;
	crp->crp_ilen = xs->datalen;
	crp->crp_alloctype = M_DEVBUF;
	crp->crp_buf = uio;
	for (i = 0, crd = crp->crp_desc; crd; i++, crd = crd->crd_next) {
		crd->crd_skip = 512 * i;
		crd->crd_len = 512;
		crd->crd_inject = 0;
		crd->crd_flags = flags;
		crd->crd_alg = CRYPTO_AES_CBC;
		crd->crd_klen = 256;
		crd->crd_rnd = 14;
		crd->crd_key = sd->mds.mdd_crypto.src_key;
		memset(crd->crd_iv, blk + i, sizeof(crd->crd_iv));
	}

	return (crp);
}

void *
sr_crypto_putcryptop(struct cryptop *crp)
{
	struct uio		*uio = crp->crp_buf;
	void			*opaque = crp->crp_opaque;

	DNPRINTF(SR_D_DIS, "sr_crypto_putcryptop crp: %p\n", crp);

	free(uio->uio_iov, M_DEVBUF);
	free(uio, M_DEVBUF);
	crypto_freereq(crp);

	return (opaque);
}

int
sr_crypto_alloc_resources(struct sr_discipline *sd)
{
	struct cryptoini	cri;

	if (!sd)
		return (EINVAL);

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	if (sr_alloc_wu(sd))
		return (ENOMEM);
	if (sr_alloc_ccb(sd))
		return (ENOMEM);

	/* XXX we need a real key later */
	memset(sd->mds.mdd_crypto.src_key, 'k',
	    sizeof sd->mds.mdd_crypto.src_key);

	bzero(&cri, sizeof(cri));
	cri.cri_alg = CRYPTO_AES_CBC;
	cri.cri_klen = 256;
	cri.cri_rnd = 14;
	cri.cri_key = sd->mds.mdd_crypto.src_key;

	return (crypto_newsession(&sd->mds.mdd_crypto.src_sid, &cri, 0));
}

int
sr_crypto_free_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_free_wu(sd);
	sr_free_ccb(sd);

	if (sd->sd_meta)
		free(sd->sd_meta, M_DEVBUF);

	rv = 0;
	return (rv);
}

int
sr_crypto_rw(struct sr_workunit *wu)
{
	struct cryptop		*crp;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_rw wu: %p\n",
	    DEVNAME(wu->swu_dis->sd_sc), wu);

	crp = sr_crypto_getcryptop(wu, 1);
	crp->crp_callback = sr_crypto_rw2;
	crp->crp_opaque = wu;
	crypto_dispatch(crp);

	return (0);
}

int
sr_crypto_rw2(struct cryptop *crp)
{
	struct sr_workunit	*wu = sr_crypto_putcryptop(crp);
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_workunit	*wup;
	struct sr_ccb		*ccb;
	struct sr_chunk		*scp;
	int			s, rt;
	daddr64_t		blk;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_rw2 0x%02x\n", DEVNAME(sd->sd_sc),
	    xs->cmd->opcode);

	if (sd->sd_vol.sv_meta.svm_status == BIOC_SVOFFLINE) {
		DNPRINTF(SR_D_DIS, "%s: sr_crypto_rw device offline\n",
		    DEVNAME(sd->sd_sc));
		goto bad;
	}

	if (xs->datalen == 0) {
		printf("%s: %s: illegal block count\n",
		    DEVNAME(sd->sd_sc), sd->sd_vol.sv_meta.svm_devname);
		goto bad;
	}

	if (xs->cmdlen == 10)
		blk = _4btol(((struct scsi_rw_big *)xs->cmd)->addr);
	else if (xs->cmdlen == 6)
		blk = _3btol(((struct scsi_rw *)xs->cmd)->addr);
	else {
		printf("%s: %s: illegal cmdlen\n", DEVNAME(sd->sd_sc),
		    sd->sd_vol.sv_meta.svm_devname);
		goto bad;
	}

	wu->swu_blk_start = blk;
	wu->swu_blk_end = blk + (xs->datalen >> 9) - 1;

	if (wu->swu_blk_end > sd->sd_vol.sv_meta.svm_size) {
		DNPRINTF(SR_D_DIS, "%s: sr_crypto_rw2 out of bounds start: %lld "
		    "end: %lld length: %d\n", wu->swu_blk_start,
		    wu->swu_blk_end, xs->datalen);

		sd->sd_scsi_sense.error_code = SSD_ERRCODE_CURRENT |
		    SSD_ERRCODE_VALID;
		sd->sd_scsi_sense.flags = SKEY_ILLEGAL_REQUEST;
		sd->sd_scsi_sense.add_sense_code = 0x21;
		sd->sd_scsi_sense.add_sense_code_qual = 0x00;
		sd->sd_scsi_sense.extra_len = 4;
		goto bad;
	}

	/* calculate physical block */
	blk += SR_META_SIZE + SR_META_OFFSET;

	wu->swu_io_count = 1;

	ccb = sr_get_ccb(sd);
	if (!ccb) {
		/* should never happen but handle more gracefully */
		printf("%s: %s: too many ccbs queued\n",
		    DEVNAME(sd->sd_sc),
		    sd->sd_vol.sv_meta.svm_devname);
		goto bad;
	}

	if (xs->flags & SCSI_POLL) {
		panic("not yet, crypto poll");
		ccb->ccb_buf.b_flags = 0;
		ccb->ccb_buf.b_iodone = NULL;
	} else {
		ccb->ccb_buf.b_flags = B_CALL;
		ccb->ccb_buf.b_iodone = sr_crypto_intr;
	}

	ccb->ccb_buf.b_blkno = blk;
	ccb->ccb_buf.b_bcount = xs->datalen;
	ccb->ccb_buf.b_bufsize = xs->datalen;
	ccb->ccb_buf.b_resid = xs->datalen;
	ccb->ccb_buf.b_data = xs->data;
	ccb->ccb_buf.b_error = 0;
	ccb->ccb_buf.b_proc = curproc;
	ccb->ccb_wu = wu;

	if (xs->flags & SCSI_DATA_IN) {
		rt = 0;
ragain:
		scp = sd->sd_vol.sv_chunks[0];
		switch (scp->src_meta.scm_status) {
		case BIOC_SDONLINE:
		case BIOC_SDSCRUB:
			ccb->ccb_buf.b_flags |= B_READ;
			break;

		case BIOC_SDOFFLINE:
		case BIOC_SDREBUILD:
		case BIOC_SDHOTSPARE:
			if (rt++ < sd->sd_vol.sv_meta.svm_no_chunk)
				goto ragain;

			/* FALLTHROUGH */
		default:
			/* volume offline */
			printf("%s: is offline, can't read\n",
			    DEVNAME(sd->sd_sc));
			sr_put_ccb(ccb);
			goto bad;
		}
	} else {
		scp = sd->sd_vol.sv_chunks[0];
		switch (scp->src_meta.scm_status) {
		case BIOC_SDONLINE:
		case BIOC_SDSCRUB:
		case BIOC_SDREBUILD:
			ccb->ccb_buf.b_flags |= B_WRITE;
			break;

		case BIOC_SDHOTSPARE: /* should never happen */
		case BIOC_SDOFFLINE:
			wu->swu_io_count--;
			sr_put_ccb(ccb);
			goto bad;

		default:
			goto bad;
		}

	}
	ccb->ccb_target = 0;
	ccb->ccb_buf.b_dev = sd->sd_vol.sv_chunks[0]->src_dev_mm;
	ccb->ccb_buf.b_vp = NULL;

	LIST_INIT(&ccb->ccb_buf.b_dep);

	TAILQ_INSERT_TAIL(&wu->swu_ccb, ccb, ccb_link);

	DNPRINTF(SR_D_DIS, "%s: %s: sr_crypto: b_bcount: %d "
	    "b_blkno: %x b_flags 0x%0x b_data %p\n",
	    DEVNAME(sd->sd_sc), sd->sd_vol.sv_meta.svm_devname,
	    ccb->ccb_buf.b_bcount, ccb->ccb_buf.b_blkno,
	    ccb->ccb_buf.b_flags, ccb->ccb_buf.b_data);


	/* walk queue backwards and fill in collider if we have one */
	s = splbio();
	TAILQ_FOREACH_REVERSE(wup, &sd->sd_wu_pendq, sr_wu_list, swu_link) {
		if (wu->swu_blk_end < wup->swu_blk_start ||
		    wup->swu_blk_end < wu->swu_blk_start)
			continue;

		/* we have an LBA collision, defer wu */
		wu->swu_state = SR_WU_DEFERRED;
		if (wup->swu_collider)
			/* wu is on deferred queue, append to last wu */
			while (wup->swu_collider)
				wup = wup->swu_collider;

		wup->swu_collider = wu;
		TAILQ_INSERT_TAIL(&sd->sd_wu_defq, wu, swu_link);
		sd->sd_wu_collisions++;
		goto queued;
	}

	/* XXX deal with polling */

	sr_raid_startwu(wu);

queued:
	splx(s);
	return (0);
bad:
	/* wu is unwound by sr_put_wu */
	return (1);
}

void
sr_crypto_intr(struct buf *bp)
{
	struct sr_ccb		*ccb = (struct sr_ccb *)bp;
	struct sr_workunit	*wu = ccb->ccb_wu;
	struct cryptop		*crp;
#ifdef SR_DEBUG
	struct sr_softc		*sc = wu->swu_dis->sd_sc;
#endif

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_intr bp: %x xs: %x\n",
	    DEVNAME(sc), bp, wu->swu_xs);

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_intr: b_bcount: %d b_resid: %d"
	    " b_flags: 0x%0x\n", DEVNAME(sc), ccb->ccb_buf.b_bcount,
	    ccb->ccb_buf.b_resid, ccb->ccb_buf.b_flags);

	crp = sr_crypto_getcryptop(wu, 0);
	crp->crp_callback = sr_crypto_intr2;
	crp->crp_opaque = bp;
	crypto_dispatch(crp);
}

int
sr_crypto_intr2(struct cryptop *crp)
{
	struct buf		*bp = sr_crypto_putcryptop(crp);
	struct sr_ccb		*ccb = (struct sr_ccb *)bp;
	struct sr_workunit	*wu = ccb->ccb_wu, *wup;
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_softc		*sc = sd->sd_sc;
	int			s, pend;

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_intr2 crp: %x xs: %x\n",
	    DEVNAME(sc), crp, xs);

	s = splbio();

	if (ccb->ccb_buf.b_flags & B_ERROR) {
		printf("%s: i/o error on block %lld\n", DEVNAME(sc),
		    ccb->ccb_buf.b_blkno);
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

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_intr2: comp: %d count: %d\n",
	    DEVNAME(sc), wu->swu_ios_complete, wu->swu_io_count);

	if (wu->swu_ios_complete == wu->swu_io_count) {
		if (wu->swu_ios_failed == wu->swu_ios_complete)
			xs->error = XS_DRIVER_STUFFUP;
		else
			xs->error = XS_NOERROR;

		xs->resid = 0;
		xs->flags |= ITSDONE;

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
		sr_put_wu(wu);
		scsi_done(xs);

		if (sd->sd_sync && sd->sd_wu_pending == 0)
			wakeup(sd);
	}

	splx(s);

	return (0);
}
