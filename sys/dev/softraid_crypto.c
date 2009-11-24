/* $OpenBSD: softraid_crypto.c,v 1.41 2009/11/24 01:03:54 jsing Exp $ */
/*
 * Copyright (c) 2007 Marco Peereboom <marco@peereboom.us>
 * Copyright (c) 2008 Hans-Joerg Hoexer <hshoexer@openbsd.org>
 * Copyright (c) 2008 Damien Miller <djm@mindrot.org>
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
#include <sys/pool.h>
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
#include <crypto/md5.h>
#include <crypto/sha1.h>
#include <crypto/sha2.h>
#include <crypto/hmac.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

struct cryptop	*sr_crypto_getcryptop(struct sr_workunit *, int);
int		sr_crypto_create_keys(struct sr_discipline *);
void		*sr_crypto_putcryptop(struct cryptop *);
int		sr_crypto_get_kdf(struct bioc_createraid *,
		    struct sr_discipline *);
int		sr_crypto_decrypt(u_char *, u_char *, u_char *, size_t, int);
int		sr_crypto_encrypt(u_char *, u_char *, u_char *, size_t, int);
int		sr_crypto_decrypt_key(struct sr_discipline *);
int		sr_crypto_alloc_resources(struct sr_discipline *);
int		sr_crypto_free_resources(struct sr_discipline *);
int		sr_crypto_write(struct cryptop *);
int		sr_crypto_rw(struct sr_workunit *);
int		sr_crypto_rw2(struct sr_workunit *, struct cryptop *);
void		sr_crypto_intr(struct buf *);
int		sr_crypto_read(struct cryptop *);
void		sr_crypto_finish_io(struct sr_workunit *);
void		sr_crypto_calculate_check_hmac_sha1(u_int8_t *, int,
		   u_int8_t *, int, u_char *);
void		sr_crypto_hotplug(struct sr_discipline *, struct disk *, int);

#ifdef SR_DEBUG0
void		 sr_crypto_dumpkeys(struct sr_discipline *);
#endif

/* Discipline initialisation. */
void
sr_crypto_discipline_init(struct sr_discipline *sd)
{
	/* Fill out discipline members. */
	sd->sd_type = SR_MD_CRYPTO;
	sd->sd_max_ccb_per_wu = sd->sd_meta->ssdi.ssd_chunk_no;
	sd->sd_max_wu = SR_CRYPTO_NOWU;

	/* Setup discipline pointers. */
	sd->sd_alloc_resources = sr_crypto_alloc_resources;
	sd->sd_free_resources = sr_crypto_free_resources;
	sd->sd_start_discipline = NULL;
	sd->sd_scsi_inquiry = sr_raid_inquiry;
	sd->sd_scsi_read_cap = sr_raid_read_cap;
	sd->sd_scsi_tur = sr_raid_tur;
	sd->sd_scsi_req_sense = sr_raid_request_sense;
	sd->sd_scsi_start_stop = sr_raid_start_stop;
	sd->sd_scsi_sync = sr_raid_sync;
	sd->sd_scsi_rw = sr_crypto_rw;
	/* XXX reuse raid 1 functions for now FIXME */
	sd->sd_set_chunk_state = sr_raid1_set_chunk_state;
	sd->sd_set_vol_state = sr_raid1_set_vol_state;
}

struct cryptop *
sr_crypto_getcryptop(struct sr_workunit *wu, int encrypt)
{
	struct scsi_xfer	*xs = wu->swu_xs;
	struct sr_discipline	*sd = wu->swu_dis;
	struct cryptop		*crp = NULL;
	struct cryptodesc	*crd;
	struct uio		*uio = NULL;
	int			flags, i, n, s;
	daddr64_t		blk = 0;
	u_int			keyndx;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_getcryptop wu: %p encrypt: %d\n",
	    DEVNAME(sd->sd_sc), wu, encrypt);

	s = splbio();
	uio = pool_get(&sd->mds.mdd_crypto.sr_uiopl, PR_ZERO);
	if (uio == NULL)
		goto unwind;
	uio->uio_iov = pool_get(&sd->mds.mdd_crypto.sr_iovpl, 0);
	if (uio->uio_iov == NULL)
		goto unwind;
	splx(s);

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

	/* Select crypto session based on block number */
	keyndx = blk >> SR_CRYPTO_KEY_BLKSHIFT;
	if (keyndx >= SR_CRYPTO_MAXKEYS)
		goto unwind;
	crp->crp_sid = sd->mds.mdd_crypto.scr_sid[keyndx];
	if (crp->crp_sid == (u_int64_t)-1)
		goto unwind;

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
	if (uio && uio->uio_iov)
		if (wu->swu_xs->flags & SCSI_DATA_OUT)
			free(uio->uio_iov->iov_base, M_DEVBUF);

	s = splbio();
	if (uio && uio->uio_iov)
		pool_put(&sd->mds.mdd_crypto.sr_iovpl, uio->uio_iov);
	if (uio)
		pool_put(&sd->mds.mdd_crypto.sr_uiopl, uio);
	splx(s);

	return (NULL);
}

void *
sr_crypto_putcryptop(struct cryptop *crp)
{
	struct uio		*uio = crp->crp_buf;
	struct sr_workunit	*wu = crp->crp_opaque;
	struct sr_discipline	*sd = wu->swu_dis;
	int			s;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_putcryptop crp: %p\n",
	    DEVNAME(wu->swu_dis->sd_sc), crp);

	if (wu->swu_xs->flags & SCSI_DATA_OUT)
		free(uio->uio_iov->iov_base, M_DEVBUF);
	s = splbio();
	pool_put(&sd->mds.mdd_crypto.sr_iovpl, uio->uio_iov);
	pool_put(&sd->mds.mdd_crypto.sr_uiopl, uio);
	splx(s);
	crypto_freereq(crp);

	return (wu);
}

int
sr_crypto_get_kdf(struct bioc_createraid *bc, struct sr_discipline *sd)
{
	int			rv = EINVAL;
	struct sr_crypto_kdfinfo *kdfinfo;

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
		    kdfinfo->genkdf.len)
			goto out;
		bcopy(&kdfinfo->genkdf,
		    sd->mds.mdd_crypto.scr_meta.scm_kdfhint,
		    kdfinfo->genkdf.len);
	}

	/* copy mask key to run-time meta data */
	if ((kdfinfo->flags & SR_CRYPTOKDF_KEY)) {
		if (sizeof(sd->mds.mdd_crypto.scr_maskkey) <
		    sizeof(kdfinfo->maskkey))
			goto out;
		bcopy(&kdfinfo->maskkey, sd->mds.mdd_crypto.scr_maskkey,
		    sizeof(kdfinfo->maskkey));
	}

	bc->bc_opaque_status = BIOC_SOINOUT_OK;
	rv = 0;
out:
	bzero(kdfinfo, bc->bc_opaque_size);
	free(kdfinfo, M_DEVBUF);

	return (rv);
}

int
sr_crypto_encrypt(u_char *p, u_char *c, u_char *key, size_t size, int alg)
{
	rijndael_ctx		ctx;
	int			i, rv = 1;

	switch (alg) {
	case SR_CRYPTOM_AES_ECB_256:
		if (rijndael_set_key_enc_only(&ctx, key, 256) != 0)
			goto out;
		for (i = 0; i < size; i += RIJNDAEL128_BLOCK_LEN)
			rijndael_encrypt(&ctx, &p[i], &c[i]);
		rv = 0;
		break;
	default:
		DNPRINTF(SR_D_DIS, "%s: unsupported encryption algorithm %u\n",
		    DEVNAME(sd->sd_sc), alg);
		rv = -1;
		goto out;
	}

out:
	bzero(&ctx, sizeof(ctx));
	return (rv);
}

int
sr_crypto_decrypt(u_char *c, u_char *p, u_char *key, size_t size, int alg)
{
	rijndael_ctx		ctx;
	int			i, rv = 1;

	switch (alg) {
	case SR_CRYPTOM_AES_ECB_256:
		if (rijndael_set_key(&ctx, key, 256) != 0)
			goto out;
		for (i = 0; i < size; i += RIJNDAEL128_BLOCK_LEN)
			rijndael_decrypt(&ctx, &c[i], &p[i]);
		rv = 0;
		break;
	default:
		DNPRINTF(SR_D_DIS, "%s: unsupported encryption algorithm %u\n",
		    DEVNAME(sd->sd_sc), alg);
		rv = -1;
		goto out;
	}

out:
	bzero(&ctx, sizeof(ctx));
	return (rv);
}

void
sr_crypto_calculate_check_hmac_sha1(u_int8_t *maskkey, int maskkey_size,
    u_int8_t *key, int key_size, u_char *check_digest)
{
	u_char			check_key[SHA1_DIGEST_LENGTH];
	HMAC_SHA1_CTX		hmacctx;
	SHA1_CTX		shactx;

	bzero(check_key, sizeof(check_key));
	bzero(&hmacctx, sizeof(hmacctx));
	bzero(&shactx, sizeof(shactx));

	/* k = SHA1(mask_key) */
	SHA1Init(&shactx);
	SHA1Update(&shactx, maskkey, maskkey_size);
	SHA1Final(check_key, &shactx);

	/* mac = HMAC_SHA1_k(unencrypted key) */
	HMAC_SHA1_Init(&hmacctx, check_key, sizeof(check_key));
	HMAC_SHA1_Update(&hmacctx, key, key_size);
	HMAC_SHA1_Final(check_digest, &hmacctx);

	bzero(check_key, sizeof(check_key));
	bzero(&hmacctx, sizeof(hmacctx));
	bzero(&shactx, sizeof(shactx));
}

int
sr_crypto_decrypt_key(struct sr_discipline *sd)
{
	u_char			check_digest[SHA1_DIGEST_LENGTH];
	int			rv = 1;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_decrypt_key\n", DEVNAME(sd->sd_sc));

	if (sd->mds.mdd_crypto.scr_meta.scm_check_alg != SR_CRYPTOC_HMAC_SHA1)
		goto out;

	if (sr_crypto_decrypt((u_char *)sd->mds.mdd_crypto.scr_meta.scm_key,
	    (u_char *)sd->mds.mdd_crypto.scr_key,
	    sd->mds.mdd_crypto.scr_maskkey, sizeof(sd->mds.mdd_crypto.scr_key),
	    sd->mds.mdd_crypto.scr_meta.scm_mask_alg) == -1)
		goto out;

#ifdef SR_DEBUG0
	sr_crypto_dumpkeys(sd);
#endif

	/* Check that the key decrypted properly. */
	sr_crypto_calculate_check_hmac_sha1(sd->mds.mdd_crypto.scr_maskkey,
	    sizeof(sd->mds.mdd_crypto.scr_maskkey),
	    (u_int8_t *)sd->mds.mdd_crypto.scr_key,
	    sizeof(sd->mds.mdd_crypto.scr_key),
	    check_digest);
	if (memcmp(sd->mds.mdd_crypto.scr_meta.chk_hmac_sha1.sch_mac,
	    check_digest, sizeof(check_digest)) != 0) {
		bzero(sd->mds.mdd_crypto.scr_key,
		    sizeof(sd->mds.mdd_crypto.scr_key));
		goto out;
	}

	rv = 0; /* Success */
out:
	/* we don't need the mask key anymore */
	bzero(&sd->mds.mdd_crypto.scr_maskkey,
	    sizeof(sd->mds.mdd_crypto.scr_maskkey));
	
	bzero(check_digest, sizeof(check_digest));

	return rv;
}

int
sr_crypto_create_keys(struct sr_discipline *sd)
{

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_create_keys\n",
	    DEVNAME(sd->sd_sc));

	if (AES_MAXKEYBYTES < sizeof(sd->mds.mdd_crypto.scr_maskkey))
		return (1);

	/* XXX allow user to specify */
	sd->mds.mdd_crypto.scr_meta.scm_alg = SR_CRYPTOA_AES_XTS_256;

	/* generate crypto keys */
	arc4random_buf(sd->mds.mdd_crypto.scr_key,
	    sizeof(sd->mds.mdd_crypto.scr_key));

	/* Mask the disk keys. */
	sd->mds.mdd_crypto.scr_meta.scm_mask_alg = SR_CRYPTOM_AES_ECB_256;
	sr_crypto_encrypt((u_char *)sd->mds.mdd_crypto.scr_key,
	    (u_char *)sd->mds.mdd_crypto.scr_meta.scm_key,
	    sd->mds.mdd_crypto.scr_maskkey, sizeof(sd->mds.mdd_crypto.scr_key),
	    sd->mds.mdd_crypto.scr_meta.scm_mask_alg);

	/* Prepare key decryption check code. */
	sd->mds.mdd_crypto.scr_meta.scm_check_alg = SR_CRYPTOC_HMAC_SHA1;
	sr_crypto_calculate_check_hmac_sha1(sd->mds.mdd_crypto.scr_maskkey,
	    sizeof(sd->mds.mdd_crypto.scr_maskkey),
	    (u_int8_t *)sd->mds.mdd_crypto.scr_key,
	    sizeof(sd->mds.mdd_crypto.scr_key),
	    sd->mds.mdd_crypto.scr_meta.chk_hmac_sha1.sch_mac);

	/* Erase the plaintext disk keys */
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
	u_int			num_keys, i;

	if (!sd)
		return (EINVAL);

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	pool_init(&sd->mds.mdd_crypto.sr_uiopl, sizeof(struct uio), 0, 0, 0,
	    "sr_uiopl", NULL);
	pool_init(&sd->mds.mdd_crypto.sr_iovpl, sizeof(struct iovec), 0, 0, 0,
	    "sr_iovpl", NULL);

	for (i = 0; i < SR_CRYPTO_MAXKEYS; i++)
		sd->mds.mdd_crypto.scr_sid[i] = (u_int64_t)-1;

	if (sr_wu_alloc(sd))
		return (ENOMEM);
	if (sr_ccb_alloc(sd))
		return (ENOMEM);
	if (sr_crypto_decrypt_key(sd))
		return (EPERM);

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
		return (EINVAL);
	}

	/* Allocate a session for every 2^SR_CRYPTO_KEY_BLKSHIFT blocks */
	num_keys = sd->sd_meta->ssdi.ssd_size >> SR_CRYPTO_KEY_BLKSHIFT;
	if (num_keys >= SR_CRYPTO_MAXKEYS)
		return (EFBIG);
	for (i = 0; i <= num_keys; i++) {
		cri.cri_key = sd->mds.mdd_crypto.scr_key[i];
		if (crypto_newsession(&sd->mds.mdd_crypto.scr_sid[i],
		    &cri, 0) != 0) {
			for (i = 0;
			     sd->mds.mdd_crypto.scr_sid[i] != (u_int64_t)-1;
			     i++) {
				crypto_freesession(
				    sd->mds.mdd_crypto.scr_sid[i]);
				sd->mds.mdd_crypto.scr_sid[i] = (u_int64_t)-1;
			}
			return (EINVAL);
		}
	}

	sr_hotplug_register(sd, sr_crypto_hotplug);

	return (0);
}

int
sr_crypto_free_resources(struct sr_discipline *sd)
{
	int			rv = EINVAL;
	u_int			i;

	if (!sd)
		return (rv);

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_free_resources\n",
	    DEVNAME(sd->sd_sc));

	sr_hotplug_unregister(sd, sr_crypto_hotplug);

	for (i = 0; sd->mds.mdd_crypto.scr_sid[i] != (u_int64_t)-1; i++) {
		crypto_freesession(
		    sd->mds.mdd_crypto.scr_sid[i]);
		sd->mds.mdd_crypto.scr_sid[i] = (u_int64_t)-1;
	}

	sr_wu_free(sd);
	sr_ccb_free(sd);

	pool_destroy(&sd->mds.mdd_crypto.sr_uiopl);
	pool_destroy(&sd->mds.mdd_crypto.sr_iovpl);

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
	int			s;
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
	int			s;
	daddr64_t		blk;

	if (sr_validate_io(wu, &blk, "sr_crypto_rw2"))
		goto bad;

	blk += SR_META_SIZE + SR_META_OFFSET;

	wu->swu_io_count = 1;

	ccb = sr_ccb_get(sd);
	if (!ccb) {
		/* should never happen but handle more gracefully */
		printf("%s: %s: too many ccbs queued\n",
		    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname);
		goto bad;
	}

	ccb->ccb_buf.b_flags = B_CALL | B_PHYS;
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
	ccb->ccb_buf.b_vp = sd->sd_vol.sv_chunks[0]->src_vn;
	if ((ccb->ccb_buf.b_flags & B_READ) == 0)
		ccb->ccb_buf.b_vp->v_numoutput++;

	LIST_INIT(&ccb->ccb_buf.b_dep);

	TAILQ_INSERT_TAIL(&wu->swu_ccb, ccb, ccb_link);

	DNPRINTF(SR_D_DIS, "%s: %s: sr_crypto_rw2: b_bcount: %d "
	    "b_blkno: %x b_flags 0x%0x b_data %p\n",
	    DEVNAME(sd->sd_sc), sd->sd_meta->ssd_devname,
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
	/* wu is unwound by sr_wu_put */
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
	sr_wu_put(wu);
	sr_scsi_done(sd, xs);

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
	sr_crypto_finish_io(wu);
	splx(s);

	return (0);
}

void
sr_crypto_hotplug(struct sr_discipline *sd, struct disk *diskp, int action)
{
	DNPRINTF(SR_D_MISC, "%s: sr_crypto_hotplug: %s %d\n",
	    DEVNAME(sd->sd_sc), diskp->dk_name, action);
}

#ifdef SR_DEBUG0
void
sr_crypto_dumpkeys(struct sr_discipline *sd)
{
	int			i, j;

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
