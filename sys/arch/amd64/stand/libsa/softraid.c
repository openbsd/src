/*	$OpenBSD: softraid.c,v 1.11 2015/07/21 03:30:51 krw Exp $	*/

/*
 * Copyright (c) 2012 Joel Sing <jsing@openbsd.org>
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

#include <sys/param.h>
#include <sys/queue.h>
#include <sys/disklabel.h>
#include <sys/reboot.h>

#include <dev/biovar.h>
#include <dev/softraidvar.h>

#include <lib/libsa/aes_xts.h>
#include <lib/libsa/hmac_sha1.h>
#include <lib/libsa/pbkdf2.h>
#include <lib/libsa/rijndael.h>

#include "libsa.h"
#include "biosdev.h"
#include "disk.h"
#include "softraid.h"

/* List of softraid volumes. */
struct sr_boot_volume_head sr_volumes;

/* Metadata from keydisks. */
struct sr_boot_keydisk {
	struct sr_uuid	kd_uuid;
	u_int8_t	kd_key[SR_CRYPTO_MAXKEYBYTES];
	SLIST_ENTRY(sr_boot_keydisk) kd_link;
};
SLIST_HEAD(sr_boot_keydisk_head, sr_boot_keydisk);
struct sr_boot_keydisk_head sr_keydisks;

void
srprobe_meta_opt_load(struct sr_metadata *sm, struct sr_meta_opt_head *som)
{
	struct sr_meta_opt_hdr	*omh;
	struct sr_meta_opt_item *omi;
#if 0
	u_int8_t checksum[MD5_DIGEST_LENGTH];
#endif
	int			i;

	/* Process optional metadata. */
	omh = (struct sr_meta_opt_hdr *)((u_int8_t *)(sm + 1) +
	    sizeof(struct sr_meta_chunk) * sm->ssdi.ssd_chunk_no);
	for (i = 0; i < sm->ssdi.ssd_opt_no; i++) {

#ifdef BIOS_DEBUG
		printf("Found optional metadata of type %u, length %u\n",
		    omh->som_type, omh->som_length);
#endif

		/* Unsupported old fixed length optional metadata. */
		if (omh->som_length == 0) {
			omh = (struct sr_meta_opt_hdr *)((void *)omh +
			    SR_OLD_META_OPT_SIZE);
			continue;
		}

		/* Load variable length optional metadata. */
		omi = alloc(sizeof(struct sr_meta_opt_item));
		bzero(omi, sizeof(struct sr_meta_opt_item));
		SLIST_INSERT_HEAD(som, omi, omi_link);
		omi->omi_som = alloc(omh->som_length);
		bzero(omi->omi_som, omh->som_length);
		bcopy(omh, omi->omi_som, omh->som_length);

#if 0
		/* XXX - Validate checksum. */
		bcopy(&omi->omi_som->som_checksum, &checksum,
		    MD5_DIGEST_LENGTH);
		bzero(&omi->omi_som->som_checksum, MD5_DIGEST_LENGTH);
		sr_checksum(sc, omi->omi_som,
		    &omi->omi_som->som_checksum, omh->som_length);
		if (bcmp(&checksum, &omi->omi_som->som_checksum,
		    sizeof(checksum)))
			panic("%s: invalid optional metadata checksum",
			    DEVNAME(sc));
#endif

		omh = (struct sr_meta_opt_hdr *)((void *)omh +
		    omh->som_length);
	}
}

void
srprobe_keydisk_load(struct sr_metadata *sm)
{
	struct sr_meta_opt_hdr	*omh;
	struct sr_meta_keydisk	*skm;
	struct sr_boot_keydisk	*kd;
	int i;

	/* Process optional metadata. */
	omh = (struct sr_meta_opt_hdr *)((u_int8_t *)(sm + 1) +
	    sizeof(struct sr_meta_chunk) * sm->ssdi.ssd_chunk_no);
	for (i = 0; i < sm->ssdi.ssd_opt_no; i++) {

		/* Unsupported old fixed length optional metadata. */
		if (omh->som_length == 0) {
			omh = (struct sr_meta_opt_hdr *)((void *)omh +
			    SR_OLD_META_OPT_SIZE);
			continue;
		}

		if (omh->som_type != SR_OPT_KEYDISK) {
			omh = (struct sr_meta_opt_hdr *)((void *)omh +
			    omh->som_length);
			continue;
		}

		kd = alloc(sizeof(struct sr_boot_keydisk));
		bcopy(&sm->ssdi.ssd_uuid, &kd->kd_uuid, sizeof(kd->kd_uuid));
		skm = (struct sr_meta_keydisk*)omh;
		bcopy(&skm->skm_maskkey, &kd->kd_key, sizeof(kd->kd_key));
		SLIST_INSERT_HEAD(&sr_keydisks, kd, kd_link);
	}
}

void
srprobe(void)
{
	struct sr_boot_volume *bv, *bv1, *bv2;
	struct sr_boot_chunk *bc, *bc1, *bc2;
	struct sr_meta_chunk *mc;
	struct sr_metadata *md;
	struct diskinfo *dip;
	struct partition *pp;
	int i, error, volno;
	dev_t bsd_dev;
	daddr_t off;

	/* Probe for softraid volumes. */
	SLIST_INIT(&sr_volumes);
	SLIST_INIT(&sr_keydisks);

	md = alloc(SR_META_SIZE * DEV_BSIZE);

	TAILQ_FOREACH(dip, &disklist, list) {

		/* Only check hard disks, skip those with I/O errors. */
		if ((dip->bios_info.bios_number & 0x80) == 0 ||
		    (dip->bios_info.flags & BDI_INVALID))
			continue;

		/* Make sure disklabel has been read. */
		if ((dip->bios_info.flags & (BDI_BADLABEL|BDI_GOODLABEL)) == 0)
			continue;

		for (i = 0; i < MAXPARTITIONS; i++) {

			pp = &dip->disklabel.d_partitions[i];
			if (pp->p_fstype != FS_RAID || pp->p_size == 0)
				continue;

			/* Read softraid metadata. */
			bzero(md, SR_META_SIZE * DEV_BSIZE);
			off = DL_SECTOBLK(&dip->disklabel, DL_GETPOFFSET(pp));
			off += SR_META_OFFSET;
			error = biosd_io(F_READ, &dip->bios_info, off,
			    SR_META_SIZE, md);
			if (error)
				continue;

			/* Is this valid softraid metadata? */
			if (md->ssdi.ssd_magic != SR_MAGIC)
				continue;

			/* XXX - validate checksum. */

			/* Handle key disks separately... */
			if (md->ssdi.ssd_level == SR_KEYDISK_LEVEL) {
				srprobe_keydisk_load(md);
				continue;
			}

			/* Locate chunk-specific metadata for this chunk. */
			mc = (struct sr_meta_chunk *)(md + 1);
			mc += md->ssdi.ssd_chunk_id;

			bc = alloc(sizeof(struct sr_boot_chunk));
			bc->sbc_diskinfo = dip;
			bc->sbc_disk = dip->bios_info.bios_number;
			bc->sbc_part = 'a' + i;

			bsd_dev = dip->bios_info.bsd_dev;
			bc->sbc_mm = MAKEBOOTDEV(B_TYPE(bsd_dev),
			    B_ADAPTOR(bsd_dev), B_CONTROLLER(bsd_dev),
			    B_UNIT(bsd_dev), bc->sbc_part - 'a');

			bc->sbc_chunk_id = md->ssdi.ssd_chunk_id;
			bc->sbc_ondisk = md->ssd_ondisk;
			bc->sbc_state = mc->scm_status;

			SLIST_FOREACH(bv, &sr_volumes, sbv_link) {
				if (bcmp(&md->ssdi.ssd_uuid, &bv->sbv_uuid,
				    sizeof(md->ssdi.ssd_uuid)) == 0)
					break;
			}

			if (bv == NULL) {
				bv = alloc(sizeof(struct sr_boot_volume));
				bzero(bv, sizeof(struct sr_boot_volume));
				bv->sbv_level = md->ssdi.ssd_level;
				bv->sbv_volid = md->ssdi.ssd_volid;
				bv->sbv_chunk_no = md->ssdi.ssd_chunk_no;
				bv->sbv_flags = md->ssdi.ssd_vol_flags;
				bv->sbv_size = md->ssdi.ssd_size;
				bv->sbv_data_blkno = md->ssd_data_blkno;
				bcopy(&md->ssdi.ssd_uuid, &bv->sbv_uuid,
				    sizeof(md->ssdi.ssd_uuid));
				SLIST_INIT(&bv->sbv_chunks);
				SLIST_INIT(&bv->sbv_meta_opt);

				/* Load optional metadata for this volume. */
				srprobe_meta_opt_load(md, &bv->sbv_meta_opt);

				/* Maintain volume order. */
				bv2 = NULL;
				SLIST_FOREACH(bv1, &sr_volumes, sbv_link) {
					if (bv1->sbv_volid > bv->sbv_volid)
						break;
					bv2 = bv1;
				}
				if (bv2 == NULL)
					SLIST_INSERT_HEAD(&sr_volumes, bv,
					    sbv_link);
				else
					SLIST_INSERT_AFTER(bv2, bv, sbv_link);
			}

			/* Maintain chunk order. */
			bc2 = NULL;
			SLIST_FOREACH(bc1, &bv->sbv_chunks, sbc_link) {
				if (bc1->sbc_chunk_id > bc->sbc_chunk_id)
					break;
				bc2 = bc1;
			}
			if (bc2 == NULL)
				SLIST_INSERT_HEAD(&bv->sbv_chunks,
				    bc, sbc_link);
			else
				SLIST_INSERT_AFTER(bc2, bc, sbc_link);

			bv->sbv_chunks_found++;
		}
	}

	/*
	 * Assemble RAID volumes.
	 */
	volno = 0;
	SLIST_FOREACH(bv, &sr_volumes, sbv_link) {

		/* Skip if this is a hotspare "volume". */
		if (bv->sbv_level == SR_HOTSPARE_LEVEL &&
		    bv->sbv_chunk_no == 1)
			continue;

		/* Determine current ondisk version. */
		bv->sbv_ondisk = 0;
		SLIST_FOREACH(bc, &bv->sbv_chunks, sbc_link) {
			if (bc->sbc_ondisk > bv->sbv_ondisk)
				bv->sbv_ondisk = bc->sbc_ondisk;
		}
		SLIST_FOREACH(bc, &bv->sbv_chunks, sbc_link) {
			if (bc->sbc_ondisk != bv->sbv_ondisk)
				bc->sbc_state = BIOC_SDOFFLINE;
		}

		/* XXX - Check for duplicate chunks. */

		/*
		 * Validate that volume has sufficient chunks for
		 * read-only access.
		 *
		 * XXX - check chunk states.
		 */
		bv->sbv_state = BIOC_SVOFFLINE;
		switch (bv->sbv_level) {
		case 0:
		case 'C':
		case 'c':
			if (bv->sbv_chunk_no == bv->sbv_chunks_found)
				bv->sbv_state = BIOC_SVONLINE;
			break;

		case 1:
			if (bv->sbv_chunk_no == bv->sbv_chunks_found)
				bv->sbv_state = BIOC_SVONLINE;
			else if (bv->sbv_chunks_found > 0)
				bv->sbv_state = BIOC_SVDEGRADED;
			break;
		}

		bv->sbv_unit = volno++;
		if (bv->sbv_state != BIOC_SVOFFLINE)
			printf(" sr%d%s", bv->sbv_unit,
			    bv->sbv_flags & BIOC_SCBOOTABLE ? "*" : "");
	}

	explicit_bzero(md, SR_META_SIZE * DEV_BSIZE);
	free(md, 0);
}

int
sr_strategy(struct sr_boot_volume *bv, int rw, daddr32_t blk, size_t size,
    void *buf, size_t *rsize)
{
	struct diskinfo *sr_dip, *dip;
	struct sr_boot_chunk *bc;
	struct aes_xts_ctx ctx;
	size_t i, j, nsect;
	daddr_t blkno;
	u_char iv[8];
	u_char *bp;
	int err;

	/* We only support read-only softraid. */
	if (rw != F_READ)
		return ENOTSUP;

	/* Partition offset within softraid volume. */
	sr_dip = (struct diskinfo *)bv->sbv_diskinfo;
	blk += sr_dip->disklabel.d_partitions[bv->sbv_part - 'a'].p_offset;

	if (bv->sbv_level == 0) {
		return ENOTSUP;
	} else if (bv->sbv_level == 1) {

		/* Select first online chunk. */
		SLIST_FOREACH(bc, &bv->sbv_chunks, sbc_link)
			if (bc->sbc_state == BIOC_SDONLINE)
				break;
		if (bc == NULL)
			return EIO;

		dip = (struct diskinfo *)bc->sbc_diskinfo;
		dip->bsddev = bc->sbc_mm;
		blk += bv->sbv_data_blkno;

		/* XXX - If I/O failed we should try another chunk... */
		return biosstrategy(dip, rw, blk, size, buf, rsize);

	} else if (bv->sbv_level == 'C') {

		/* Select first online chunk. */
		SLIST_FOREACH(bc, &bv->sbv_chunks, sbc_link)
			if (bc->sbc_state == BIOC_SDONLINE)
				break;
		if (bc == NULL)
			return EIO;

		dip = (struct diskinfo *)bc->sbc_diskinfo;
		dip->bsddev = bc->sbc_mm;

		/* XXX - select correct key. */
		aes_xts_setkey(&ctx, (u_char *)bv->sbv_keys, 64);

		nsect = (size + DEV_BSIZE - 1) / DEV_BSIZE;
		for (i = 0; i < nsect; i++) {
			blkno = blk + i;
			bp = ((u_char *)buf) + i * DEV_BSIZE;
			err = biosstrategy(dip, rw, bv->sbv_data_blkno + blkno,
			    DEV_BSIZE, bp, NULL);
			if (err != 0)
				return err;

			bcopy(&blkno, iv, sizeof(blkno));
			aes_xts_reinit(&ctx, iv);
			for (j = 0; j < DEV_BSIZE; j += AES_XTS_BLOCKSIZE)
				aes_xts_decrypt(&ctx, bp + j);
		}
		if (rsize != NULL)
			*rsize = nsect * DEV_BSIZE;

		return err;

	} else
		return ENOTSUP;
}

const char *
sr_getdisklabel(struct sr_boot_volume *bv, struct disklabel *label)
{
	struct dos_partition *dp;
	struct dos_mbr mbr;
	u_int start = 0;
	char *buf;
	int i;

	/* Check for MBR to determine partition offset. */
	bzero(&mbr, sizeof(mbr));
	sr_strategy(bv, F_READ, DOSBBSECTOR, sizeof(mbr), &mbr, NULL);
	if (mbr.dmbr_sign == DOSMBR_SIGNATURE) {

		/* Search for OpenBSD partition */
		for (i = 0; i < NDOSPART; i++) {
			dp = &mbr.dmbr_parts[i];
			if (!dp->dp_size)
				continue;
			if (dp->dp_typ == DOSPTYP_OPENBSD) {
				if (dp->dp_start > (dp->dp_start + DOSBBSECTOR))
					continue;
				start = dp->dp_start + DOSBBSECTOR;
			}
		}
	}

	start += LABELSECTOR;

	/* Read the disklabel. */
	buf = alloca(DEV_BSIZE);
	sr_strategy(bv, F_READ, start, sizeof(struct disklabel), buf, NULL);

#ifdef BIOS_DEBUG
	printf("sr_getdisklabel: magic %lx\n",
	    ((struct disklabel *)buf)->d_magic);
	for (i = 0; i < MAXPARTITIONS; i++)
		printf("part %c: type = %d, size = %d, offset = %d\n", 'a' + i,
		    (int)((struct disklabel *)buf)->d_partitions[i].p_fstype,
		    (int)((struct disklabel *)buf)->d_partitions[i].p_size,
		    (int)((struct disklabel *)buf)->d_partitions[i].p_offset);
#endif

	/* Fill in disklabel */
	return (getdisklabel(buf, label));
}


#define RIJNDAEL128_BLOCK_LEN     16
#define PASSPHRASE_LENGTH 1024

#define SR_CRYPTO_KEYBLOCK_BYTES SR_CRYPTO_MAXKEYS * SR_CRYPTO_KEYBYTES

#ifdef BIOS_DEBUG
void
printhex(const char *s, const u_int8_t *buf, size_t len)
{
	u_int8_t n1, n2;
	size_t i;

	printf("%s: ", s);
	for (i = 0; i < len; i++) {
		n1 = buf[i] & 0x0f;
		n2 = buf[i] >> 4;
		printf("%c", n2 > 9 ? n2 + 'a' - 10 : n2 + '0');
		printf("%c", n1 > 9 ? n1 + 'a' - 10 : n1 + '0');
	}
	printf("\n");
}
#endif

void
sr_clear_keys(void)
{
	struct sr_boot_volume *bv;
	struct sr_boot_keydisk *kd;

	SLIST_FOREACH(bv, &sr_volumes, sbv_link) {
		if (bv->sbv_level != 'C')
			continue;
		if (bv->sbv_keys != NULL) {
			explicit_bzero(bv->sbv_keys, SR_CRYPTO_KEYBLOCK_BYTES);
			free(bv->sbv_keys, 0);
			bv->sbv_keys = NULL;
		}
		if (bv->sbv_maskkey != NULL) {
			explicit_bzero(bv->sbv_maskkey, SR_CRYPTO_MAXKEYBYTES);
			free(bv->sbv_maskkey, 0);
			bv->sbv_maskkey = NULL;
		}
	}
	SLIST_FOREACH(kd, &sr_keydisks, kd_link) {
		explicit_bzero(kd, sizeof(*kd));
		free(kd, 0);
	}
}

void
sr_crypto_calculate_check_hmac_sha1(u_int8_t *maskkey, int maskkey_size,
    u_int8_t *key, int key_size, u_char *check_digest)
{
	u_int8_t check_key[SHA1_DIGEST_LENGTH];
	SHA1_CTX shactx;

	explicit_bzero(check_key, sizeof(check_key));
	explicit_bzero(&shactx, sizeof(shactx));

	/* k = SHA1(mask_key) */
	SHA1Init(&shactx);
	SHA1Update(&shactx, maskkey, maskkey_size);
	SHA1Final(check_key, &shactx);

	/* mac = HMAC_SHA1_k(unencrypted key) */
	hmac_sha1(key, key_size, check_key, sizeof(check_key), check_digest);

	explicit_bzero(check_key, sizeof(check_key));
	explicit_bzero(&shactx, sizeof(shactx));
}

int
sr_crypto_decrypt_keys(struct sr_boot_volume *bv)
{
	struct sr_meta_crypto *cm;
	struct sr_boot_keydisk	*kd;
	struct sr_meta_opt_item *omi;
	struct sr_crypto_kdf_pbkdf2 *kdfhint;
	struct sr_crypto_kdfinfo kdfinfo;
	char passphrase[PASSPHRASE_LENGTH];
	u_int8_t digest[SHA1_DIGEST_LENGTH];
	u_int8_t *keys = NULL;
	u_int8_t *kp, *cp;
	rijndael_ctx ctx;
	int rv = -1;
	int c, i;

	SLIST_FOREACH(omi, &bv->sbv_meta_opt, omi_link)
		if (omi->omi_som->som_type == SR_OPT_CRYPTO)
			break;

	if (omi == NULL) {
		printf("Crypto metadata not found!\n");
		goto done;
	}

	cm = (struct sr_meta_crypto *)omi->omi_som;
	kdfhint = (struct sr_crypto_kdf_pbkdf2 *)&cm->scm_kdfhint;

	switch (cm->scm_mask_alg) {
	case SR_CRYPTOM_AES_ECB_256:
		break;
	default:
		printf("unsupported encryption algorithm %u\n",
		    cm->scm_mask_alg);
		goto done;
	}

	SLIST_FOREACH(kd, &sr_keydisks, kd_link) {
		if (bcmp(&kd->kd_uuid, &bv->sbv_uuid, sizeof(kd->kd_uuid)) == 0)
			break;
	}
	if (kd) {
		bcopy(&kd->kd_key, &kdfinfo.maskkey, sizeof(kdfinfo.maskkey));
	} else {
		printf("Passphrase: ");
		for (i = 0; i < PASSPHRASE_LENGTH - 1; i++) {
			c = cngetc();
			if (c == '\r' || c == '\n')
				break;
			passphrase[i] = (c & 0xff);
		}
		passphrase[i] = 0;
		printf("\n");

#ifdef BIOS_DEBUG
		printf("Got passphrase: %s with len %d\n",
		    passphrase, strlen(passphrase));
#endif

		if (pkcs5_pbkdf2(passphrase, strlen(passphrase), kdfhint->salt,
		    sizeof(kdfhint->salt), kdfinfo.maskkey,
		    sizeof(kdfinfo.maskkey), kdfhint->rounds) != 0) {
			printf("pbkdf2 failed\n");
			goto done;
		}
	}

	/* kdfinfo->maskkey now has key. */

	/* Decrypt disk keys. */
	keys = alloc(SR_CRYPTO_KEYBLOCK_BYTES);
	bzero(keys, SR_CRYPTO_KEYBLOCK_BYTES);

	if (rijndael_set_key(&ctx, kdfinfo.maskkey, 256) != 0)
		goto done;

	cp = (u_int8_t *)cm->scm_key;
	kp = keys;
	for (i = 0; i < SR_CRYPTO_KEYBLOCK_BYTES; i += RIJNDAEL128_BLOCK_LEN)
		rijndael_decrypt(&ctx, (u_char *)(cp + i), (u_char *)(kp + i));

	/* Check that the key decrypted properly. */
	sr_crypto_calculate_check_hmac_sha1(kdfinfo.maskkey,
	    sizeof(kdfinfo.maskkey), keys, SR_CRYPTO_KEYBLOCK_BYTES, digest);

	if (bcmp(digest, cm->chk_hmac_sha1.sch_mac, sizeof(digest))) {
		printf("incorrect passphrase or keydisk\n");
		goto done;
	}

	/* Keys and keydisks will be cleared before boot and from _rtt. */
	bv->sbv_keys = keys;
	bv->sbv_maskkey = alloc(sizeof(kdfinfo.maskkey));
	bcopy(&kdfinfo.maskkey, bv->sbv_maskkey, sizeof(kdfinfo.maskkey));

	rv = 0;

done:
	explicit_bzero(passphrase, PASSPHRASE_LENGTH);
	explicit_bzero(&kdfinfo, sizeof(kdfinfo));
	explicit_bzero(digest, sizeof(digest));

	if (keys != NULL && rv != 0) {
		explicit_bzero(keys, SR_CRYPTO_KEYBLOCK_BYTES);
		free(keys, 0);
	}

	return (rv);
}
