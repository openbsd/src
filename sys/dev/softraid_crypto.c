/* $OpenBSD: softraid_crypto.c,v 1.22 2008/06/13 18:26:59 hshoexer Exp $ */
/*
 * Copyright (c) 2007 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2008 Hans-Joerg Hoexer <hshoexer@openbsd.org>
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

#include <crypto/cryptodev.h>
#include <crypto/cryptosoft.h>
#include <crypto/rijndael.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

struct cryptop	*sr_crypto_getcryptop(struct sr_workunit *, int);
int		 sr_crypto_create_keys(struct sr_discipline *);
void		*sr_crypto_putcryptop(struct cryptop *);
int		 sr_crypto_get_kdf(struct bioc_createraid *,
		     struct sr_discipline *);
int		 sr_crypto_decrypt_key(struct sr_discipline *);
int		 sr_crypto_alloc_resources(struct sr_discipline *);
int		 sr_crypto_free_resources(struct sr_discipline *);
int		 sr_crypto_write(struct cryptop *);
int		 sr_crypto_rw(struct sr_workunit *);
int		 sr_crypto_rw2(struct sr_workunit *, struct cryptop *);
void		 sr_crypto_intr(struct buf *);
int		 sr_crypto_read(struct cryptop *);
void		 sr_crypto_finish_io(struct sr_workunit *);

#ifdef SR_DEBUG0
void		 sr_crypto_dumpkeys(struct sr_discipline *);
#endif

struct cryptop *
sr_crypto_getcryptop(struct sr_workunit *wu, int encrypt)
{
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_discipline	*sd = wu->swu_dis;
	struct cryptop		*crp;
	struct cryptodesc	*crd;
	struct uio		*uio;
	int			 flags, i, n;
	daddr64_t		 blk = 0;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_getcryptop wu: %p encrypt: %d\n",
	    DEVNAME(sd->sd_sc), wu, encrypt);

	/* XXX use pool */
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

	n = xs->datalen >> DEV_BSHIFT;
	flags = (encrypt ? CRD_F_ENCRYPT : 0) |
	    CRD_F_IV_PRESENT | CRD_F_IV_EXPLICIT;

	crp = crypto_getreq(n);
	if (crp == NULL)
		goto unwind;

	crp->crp_sid = sd->mds.mdd_crypto.scr_sid;
	crp->crp_ilen = xs->datalen;
	crp->crp_alloctype = M_DEVBUF;
	crp->crp_buf = uio;
	for (i = 0, crd = crp->crp_desc; crd; i++, blk++, crd = crd->crd_next) {
		crd->crd_skip = i << DEV_BSHIFT;
		crd->crd_len = DEV_BSIZE;
		crd->crd_inject = 0;
		crd->crd_flags = flags;
		crd->crd_alg = CRYPTO_AES_XTS;

		switch (sd->mds.mdd_crypto.scr_meta.scm_alg) {
		case SR_CRYPTOA_AES_XTS_128:
			crd->crd_klen = 256;
			break;
		case SR_CRYPTOA_AES_XTS_256:
			crd->crd_klen = 512;
			break;
		default:
			goto unwind;
		}
		crd->crd_key = sd->mds.mdd_crypto.scr_key[0];
		bcopy(&blk, crd->crd_iv, sizeof(blk));
	}

	return (crp);
unwind:
	if (crp)
		crypto_freereq(crp);
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
sr_crypto_get_kdf(struct bioc_createraid *bc, struct sr_discipline *sd)
{
	struct sr_crypto_kdfinfo	*kdfinfo;
	int				 rv = EINVAL;

	if (!(bc->bc_opaque_flags & BIOC_SOIN))
		return (rv);
	if (bc->bc_opaque == NULL)
		return (rv);
	if (bc->bc_opaque_size < sizeof(*kdfinfo))
		return (rv);

	kdfinfo = malloc(bc->bc_opaque_size, M_DEVBUF, M_WAITOK | M_ZERO);
	if (copyin(bc->bc_opaque, kdfinfo, bc->bc_opaque_size))
		goto out;

	if (kdfinfo->len != bc->bc_opaque_size)
		goto out;

	/* copy KDF hint to disk meta data */
	if (kdfinfo->flags & SR_CRYPTOKDF_HINT) {
		if (sizeof(sd->mds.mdd_crypto.scr_meta.scm_kdfhint) <
		    kdfinfo->kdfhint.len)
			goto out;
		bcopy(&kdfinfo->kdfhint,
		    sd->mds.mdd_crypto.scr_meta.scm_kdfhint,
		    kdfinfo->kdfhint.len);
	}

	/* copy mask key to run-time meta data */
	if ((kdfinfo->flags & SR_CRYPTOKDF_KEY)) {
		if (sizeof(sd->mds.mdd_crypto.scr_maskkey) <
		    sizeof(kdfinfo->maskkey))
			goto out;
		bcopy(&kdfinfo->maskkey, sd->mds.mdd_crypto.scr_maskkey,
		    sizeof(kdfinfo->maskkey));
	}

	rv = 0;
out:
	bzero(kdfinfo, bc->bc_opaque_size);
	free(kdfinfo, M_DEVBUF);

	return (rv);
}

int
sr_crypto_decrypt_key(struct sr_discipline *sd)
{
	rijndael_ctx	 ctx;
	u_char		*p, *c;
	size_t		 ksz;
	int		 i;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_decrypt_key\n", DEVNAME(sd->sd_sc));

	if (rijndael_set_key(&ctx, sd->mds.mdd_crypto.scr_maskkey, 128) != 0) {
		bzero(&ctx, sizeof(ctx));
		return (1);
	}
	/* we don't need the mask key anymore */
	bzero(&sd->mds.mdd_crypto.scr_maskkey,
	    sizeof(sd->mds.mdd_crypto.scr_maskkey));

	c = (u_char *)sd->mds.mdd_crypto.scr_meta.scm_key;
	p = (u_char *)sd->mds.mdd_crypto.scr_key;
	ksz = sizeof(sd->mds.mdd_crypto.scr_key);

	for (i = 0; i < ksz; i += RIJNDAEL128_BLOCK_LEN)
		rijndael_decrypt(&ctx, &c[i], &p[i]);
	bzero(&ctx, sizeof(ctx));

#ifdef SR_DEBUG0
	sr_crypto_dumpkeys(sd);
#endif
	return (0);
}

int
sr_crypto_create_keys(struct sr_discipline *sd)
{
	rijndael_ctx	 ctx;
	u_char		*p, *c;
	size_t		 ksz;
	int		 i;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_create_keys\n",
	    DEVNAME(sd->sd_sc));

	if (AES_MAXKEYBYTES < sizeof(sd->mds.mdd_crypto.scr_maskkey))
		return (1);

	/* generate crypto keys */
	arc4random_buf(sd->mds.mdd_crypto.scr_key,
	    sizeof(sd->mds.mdd_crypto.scr_key));

	/* XXX 128 for now */
	sd->mds.mdd_crypto.scr_meta.scm_alg = SR_CRYPTOA_AES_XTS_128;

	if (rijndael_set_key_enc_only(&ctx, sd->mds.mdd_crypto.scr_maskkey,
	    128) != 0) {
		bzero(sd->mds.mdd_crypto.scr_key,
		    sizeof(sd->mds.mdd_crypto.scr_key));
		bzero(&ctx, sizeof(ctx));
		return (1);
	}

	p = (u_char *)sd->mds.mdd_crypto.scr_key;
	c = (u_char *)sd->mds.mdd_crypto.scr_meta.scm_key;
	ksz = sizeof(sd->mds.mdd_crypto.scr_key);

	for (i = 0; i < ksz; i += RIJNDAEL128_BLOCK_LEN)
		rijndael_encrypt(&ctx, &p[i], &c[i]);
	bzero(&ctx, sizeof(ctx));
	bzero(sd->mds.mdd_crypto.scr_key, sizeof(sd->mds.mdd_crypto.scr_key));

#ifdef SR_DEBUG0
	sr_crypto_dumpkeys(sd);
#endif

	sd->mds.mdd_crypto.scr_meta.scm_flags = SR_CRYPTOF_KEY |
	    SR_CRYPTOF_KDFHINT;

	return (0);
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
		return (ENOMEM);		/* XXX */

	if (sr_crypto_decrypt_key(sd))
		return (1);			/* XXX */

	bzero(&cri, sizeof(cri));
	cri.cri_alg = CRYPTO_AES_XTS;
	switch (sd->mds.mdd_crypto.scr_meta.scm_alg) {
	case SR_CRYPTOA_AES_XTS_128:
		cri.cri_klen = 256;
		break;
	case SR_CRYPTOA_AES_XTS_256:
		cri.cri_klen = 512;
		break;
	default:
		return (1);			/* XXX */
	}
	cri.cri_key = sd->mds.mdd_crypto.scr_key[0];

	return (crypto_newsession(&sd->mds.mdd_crypto.scr_sid, &cri, 0));
}

int
sr_crypto_free_resources(struct sr_discipline *sd)
{
	int		rv = EINVAL;

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
	int			 s, rv = 0;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_rw wu: %p\n",
	    DEVNAME(wu->swu_dis->sd_sc), wu);

	if (wu->swu_xs->flags & SCSI_DATA_OUT) {
		crp = sr_crypto_getcryptop(wu, 1);
		crp->crp_callback = sr_crypto_write;
		crp->crp_opaque = wu;
		s = splvm();
		if (crypto_invoke(crp))
			rv = 1;
		else
			rv = crp->crp_etype;
		splx(s);
	} else
		rv = sr_crypto_rw2(wu, NULL);

	return (rv);
}

int
sr_crypto_write(struct cryptop *crp)
{
	int		 	 s;
	struct sr_workunit	*wu = crp->crp_opaque;

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

	return (sr_crypto_rw2(wu, crp));
}

int
sr_crypto_rw2(struct sr_workunit *wu, struct cryptop *crp)
{
	struct sr_discipline	*sd = wu->swu_dis;
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_ccb		*ccb;
	struct uio		*uio;
	int			 s;
	daddr64_t		 blk;

	if (sr_validate_io(wu, &blk, "sr_crypto_rw2"))
		goto bad;

	blk += SR_META_SIZE + SR_META_OFFSET;

	wu->swu_io_count = 1;

	ccb = sr_get_ccb(sd);
	if (!ccb) {
		/* should never happen but handle more gracefully */
		printf("%s: %s: too many ccbs queued\n",
		    DEVNAME(sd->sd_sc), sd->sd_vol.sv_meta.svm_devname);
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

	s = splbio();

	if (sr_check_io_collision(wu))
		goto queued;

	sr_raid_startwu(wu);

queued:
	splx(s);
	return (0);
bad:
	/* wu is unwound by sr_put_wu */
	if (crp)
		crp->crp_etype = EINVAL;
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
	int			 s, s2, pend;

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
				TAILQ_REMOVE(&sd->sd_wu_pendq, wu, swu_link);
				pend = 1;

				if (wu->swu_collider) {
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
	int			 s;
	struct sr_workunit	*wu = crp->crp_opaque;

	DNPRINTF(SR_D_INTR, "%s: sr_crypto_read: wu %x xs: %x\n",
	    DEVNAME(wu->swu_dis->sd_sc), wu, wu->swu_xs);

	if (crp->crp_etype)
		wu->swu_xs->error = XS_DRIVER_STUFFUP;

	s = splbio();
	sr_crypto_finish_io(wu);
	splx(s);

	return (0);
}

#ifdef SR_DEBUG0
void
sr_crypto_dumpkeys(struct sr_discipline *sd)
{
	int	i, j;

	printf("sr_crypto_dumpkeys:\n");
	for (i = 0; i < SR_CRYPTO_MAXKEYS; i++) {
		printf("\tscm_key[%d]: 0x", i);
		for (j = 0; j < SR_CRYPTO_KEYBYTES; j++) {
			printf("%02x",
			    sd->mds.mdd_crypto.scr_meta.scm_key[i][j]);
		}
		printf("\n");
	}
	printf("sr_crypto_dumpkeys: runtime data keys:\n");
	for (i = 0; i < SR_CRYPTO_MAXKEYS; i++) {
		printf("\tscr_key[%d]: 0x", i);
		for (j = 0; j < SR_CRYPTO_KEYBYTES; j++) {
			printf("%02x",
			    sd->mds.mdd_crypto.scr_key[i][j]);
		}
		printf("\n");
	}
}
#endif	/* SR_DEBUG */
