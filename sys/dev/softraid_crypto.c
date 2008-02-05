/* $OpenBSD: softraid_crypto.c,v 1.7 2008/02/05 16:15:35 marco Exp $ */
/*
 * Copyright (c) 2007 Ted Unangst <tedu@openbsd.org>
 * Copyright (c) 2008 Marco Peereboom <marco@openbsd.org>
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
void			*sr_crypto_putcryptop(struct cryptop *);
int			sr_crypto_write(struct cryptop *);
int			sr_crypto_rw2(struct sr_workunit *, struct cryptop *);
void			sr_crypto_intr(struct buf *);
int			sr_crypto_read(struct cryptop *);
void			sr_crypto_finish_io(struct sr_workunit *);

struct cryptop *
sr_crypto_getcryptop(struct sr_workunit *wu, int encrypt)
{
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_discipline	*sd = wu->swu_dis;
	struct cryptop		*crp;
	struct cryptodesc	*crd;
	struct uio		*uio;
	int			flags, i, n;
	daddr64_t		blk = 0;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_getcryptop wu: %p encrypt: %d\n",
	    DEVNAME(sd->sd_sc), wu, encrypt);

	/* XXX eliminate all malloc here, make either pool or pre-alloc */
	uio = malloc(sizeof(*uio), M_DEVBUF, M_NOWAIT | M_ZERO);
	uio->uio_iov = malloc(sizeof(*uio->uio_iov), M_DEVBUF, M_NOWAIT);
	uio->uio_iovcnt = 1;
	uio->uio_iov->iov_len = xs->datalen;
	if (xs->flags & SCSI_DATA_OUT) {
		uio->uio_iov->iov_base = malloc(xs->datalen, M_DEVBUF,
		    M_NOWAIT);
		bcopy(xs->data, uio->uio_iov->iov_base, xs->datalen);
	} else
		uio->uio_iov->iov_base = xs->data;

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
	if (crp == NULL)
		goto unwind;

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
unwind:
	if (wu->swu_xs->flags & SCSI_DATA_OUT)
		free(uio->uio_iov->iov_base, M_DEVBUF);
	free(uio->uio_iov, M_DEVBUF);
	free(uio, M_DEVBUF);
	return (NULL);
}

void *
sr_crypto_putcryptop(struct cryptop *crp)
{
	struct uio		*uio = crp->crp_buf;
	struct sr_workunit	*wu = crp->crp_opaque;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_putcryptop crp: %p\n",
	    DEVNAME(wu->swu_dis->sd_sc), crp);

	if (wu->swu_xs->flags & SCSI_DATA_OUT)
		free(uio->uio_iov->iov_base, M_DEVBUF);
	free(uio->uio_iov, M_DEVBUF);
	free(uio, M_DEVBUF);
	crypto_freereq(crp);

	return (wu);
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
	int			s, rv = 0;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_rw wu: %p\n",
	    DEVNAME(wu->swu_dis->sd_sc), wu);

	if (wu->swu_xs->flags & SCSI_DATA_OUT) {
		crp = sr_crypto_getcryptop(wu, 1);
		crp->crp_callback = sr_crypto_write;
		crp->crp_opaque = wu;
		s = splvm();
		if (crypto_invoke(crp))
			rv = 1;
		splx(s);
	} else
		rv = sr_crypto_rw2(wu, NULL);

	return (rv);
}

int
sr_crypto_write(struct cryptop *crp)
{
	int			s;
#ifdef SR_DEBUG
	struct sr_workunit	*wu = crp->crp_opaque;
#endif /* SR_DEBUG */

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_write: wu %x xs: %x\n",
	    DEVNAME(wu->swu_dis->sd_sc), wu, wu->swu_xs);

	if (crp->crp_etype) {
		/* fail io */
		((struct sr_workunit *)(crp->crp_opaque))->swu_xs->error =
		    XS_DRIVER_STUFFUP;
		s = splbio();
		sr_crypto_finish_io(crp->crp_opaque);
		splx(s);
	}

	return (sr_crypto_rw2(crp->crp_opaque, crp));
}

int
sr_crypto_rw2(struct sr_workunit *wu, struct cryptop *crp)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_workunit	*wup;
	struct sr_ccb		*ccb;
	struct uio		*uio;
	int			s;
	daddr64_t		blk;

	/* blk and scsi error will be handled by sr_validate_io */
	if (sr_validate_io(wu, &blk, "sr_crypto_rw2"))
		goto bad;

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

	ccb->ccb_buf.b_flags = B_CALL;
	ccb->ccb_buf.b_iodone = sr_crypto_intr;
	ccb->ccb_buf.b_blkno = blk;
	ccb->ccb_buf.b_bcount = xs->datalen;
	ccb->ccb_buf.b_bufsize = xs->datalen;
	ccb->ccb_buf.b_resid = xs->datalen;

	if (xs->flags & SCSI_DATA_IN) {
		ccb->ccb_buf.b_flags |= B_READ;
		ccb->ccb_buf.b_data = xs->data;
	} else {
		uio = crp->crp_buf;
		ccb->ccb_buf.b_flags |= B_WRITE;
		ccb->ccb_buf.b_data = uio->uio_iov->iov_base;
		ccb->ccb_opaque = crp;
	}

	ccb->ccb_buf.b_error = 0;
	ccb->ccb_buf.b_proc = curproc;
	ccb->ccb_wu = wu;
	ccb->ccb_target = 0;
	ccb->ccb_buf.b_dev = sd->sd_vol.sv_chunks[0]->src_dev_mm;
	ccb->ccb_buf.b_vp = NULL;

	LIST_INIT(&ccb->ccb_buf.b_dep);

	TAILQ_INSERT_TAIL(&wu->swu_ccb, ccb, ccb_link);

	DNPRINTF(SR_D_DIS, "%s: %s: sr_crypto_rw2: b_bcount: %d "
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
	struct sr_workunit	*wu = ccb->ccb_wu, *wup;
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_softc		*sc = sd->sd_sc;
	struct cryptop		*crp;
	int			s, s2, pend;

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_intr bp: %x xs: %x\n",
	    DEVNAME(sc), bp, wu->swu_xs);

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_intr: b_bcount: %d b_resid: %d"
	    " b_flags: 0x%0x\n", DEVNAME(sc), ccb->ccb_buf.b_bcount,
	    ccb->ccb_buf.b_resid, ccb->ccb_buf.b_flags);

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

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_intr: comp: %d count: %d\n",
	    DEVNAME(sc), wu->swu_ios_complete, wu->swu_io_count);

	if (wu->swu_ios_complete == wu->swu_io_count) {
		if (wu->swu_ios_failed == wu->swu_ios_complete)
			xs->error = XS_DRIVER_STUFFUP;
		else
			xs->error = XS_NOERROR;

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

		/* do this after restarting other wus to shorten latency */
		if ((xs->flags & SCSI_DATA_IN) && (xs->error == XS_NOERROR)) {
			crp = sr_crypto_getcryptop(wu, 0);
			ccb->ccb_opaque = crp;
			crp->crp_callback = sr_crypto_read;
			crp->crp_opaque = wu;
			DNPRINTF(SR_D_INTR, "%s: sr_crypto_intr: crypto_invoke "
			    "%p\n", DEVNAME(sc), crp);
			s2 = splvm();
			crypto_invoke(crp);
			splx(s2);
			goto done;
		}
		
		sr_crypto_finish_io(wu);
	}

done:
	splx(s);
}

void
sr_crypto_finish_io(struct sr_workunit *wu)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_ccb		*ccb;
#ifdef SR_DEBUG
	struct sr_softc		*sc = sd->sd_sc;
#endif /* SR_DEBUG */
	splassert(IPL_BIO);

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_finish_io: wu %x xs: %x\n",
	    DEVNAME(sc), wu, xs);

	xs->resid = 0;
	xs->flags |= ITSDONE;

	TAILQ_FOREACH(ccb, &wu->swu_ccb, ccb_link) {
		if (ccb->ccb_opaque == NULL)
			continue;
		sr_crypto_putcryptop(ccb->ccb_opaque);
	}

	/* do not change the order of these 2 functions */
	sr_put_wu(wu);
	scsi_done(xs);

	if (sd->sd_sync && sd->sd_wu_pending == 0)
		wakeup(sd);
}

int
sr_crypto_read(struct cryptop *crp)
{
	int			s;
	struct sr_workunit	*wu = crp->crp_opaque;

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_read: wu %x xs: %x\n",
	    DEVNAME(wu->swu_dis->sd_sc), wu, wu->swu_xs);

	if (crp->crp_etype)
		wu->swu_xs->error = XS_DRIVER_STUFFUP;

	s = splbio();
	sr_crypto_finish_io(crp->crp_opaque);
	splx(s);

	return (0);
}
