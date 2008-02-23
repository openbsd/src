/* $OpenBSD: softraid_crypto.c,v 1.18 2008/02/23 19:46:00 marco Exp $ */
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

#include <crypto/cryptodev.h>
#include <crypto/cryptosoft.h>

#include <scsi/scsi_all.h>
#include <scsi/scsiconf.h>
#include <scsi/scsi_disk.h>

#include <dev/softraidvar.h>
#include <dev/rndvar.h>

int			sr_crypto_decrypt_key(struct sr_discipline *);
int			sr_crypto_encrypt_key(struct sr_discipline *);

int
sr_crypto_decrypt_key(struct sr_discipline *sd)
{
	DNPRINTF(SR_D_DIS, "%s: sr_crypto_decrypt_key\n", DEVNAME(sd->sd_sc));

	return (1);
}

int
sr_crypto_encrypt_key(struct sr_discipline *sd)
{
	DNPRINTF(SR_D_DIS, "%s: sr_crypto_encrypt_key\n", DEVNAME(sd->sd_sc));

	return (1);
}

void
sr_crypto_create_keys(struct sr_discipline *sd)
{
	u_int32_t		*pk1, *pk2, sz;
	int			i, x;

	DNPRINTF(SR_D_DIS, "%s: sr_crypto_create_keys\n",
	    DEVNAME(sd->sd_sc));

	/* generate crypto keys */
	for (i = 0; i < SR_CRYPTO_MAXKEYS; i++) {
		sz = sizeof(sd->mds.mdd_crypto.scr_key1[i]) / 4;
		pk1 = (u_int32_t *)sd->mds.mdd_crypto.scr_key1[i];
		pk2 = (u_int32_t *)sd->mds.mdd_crypto.scr_key2[i];
		for (x = 0; x < sz; x++) {
			*pk1++ = arc4random();
			*pk2++ = arc4random();
		}
	}

	/* generate salt */
	sz = sizeof(sd->mds.mdd_crypto.scr_meta[0].scm_salt) / 4;
	pk1 = (u_int32_t *)sd->mds.mdd_crypto.scr_meta[0].scm_salt;
	for (i = 0; i < sz; i++)
		*pk1++ = arc4random();

	sd->mds.mdd_crypto.scr_meta[0].scm_flags =
	    SR_CRYPTOF_KEY | SR_CRYPTOF_SALT;

	strlcpy(sd->mds.mdd_crypto.scr_meta[1].scm_passphrase,
	    "my super secret passphrase ZOMGPASSWD",
	    sizeof(sd->mds.mdd_crypto.scr_meta[1].scm_passphrase));
	sd->mds.mdd_crypto.scr_meta[1].scm_flags =
	    SR_CRYPTOF_PASSPHRASE;
}

int
sr_crypto_alloc_resources(struct sr_discipline *sd)
{
	DNPRINTF(SR_D_DIS, "%s: sr_crypto_alloc_resources\n",
	    DEVNAME(sd->sd_sc));

	return (EINVAL);
}

int
sr_crypto_free_resources(struct sr_discipline *sd)
{
	DNPRINTF(SR_D_DIS, "%s: sr_crypto_free_resources\n",
	    DEVNAME(sd->sd_sc));

	return (EINVAL);
}

int
sr_crypto_rw(struct sr_workunit *wu)
{
	DNPRINTF(SR_D_DIS, "%s: sr_crypto_rw wu: %p\n",
	    DEVNAME(wu->swu_dis->sd_sc), wu);

	return (1);
}
