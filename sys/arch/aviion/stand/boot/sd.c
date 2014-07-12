/*	$OpenBSD: sd.c,v 1.6 2014/07/12 18:44:41 tedu Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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


#include <sys/types.h>
#include <sys/param.h>
#include <stand.h>

#include "scsi.h"

int
sdopen(struct open_file *f, const char *ctrlname, int ctrl, int unit, int lun,
    int part)
{
	struct scsi_private *priv;
	struct vdm_label *vdl;
	int rc;
	char buf[DEV_BSIZE];
	char *msg;
	size_t z;

	f->f_devdata = priv = scsi_initialize(ctrlname, ctrl, unit, lun, part);
	if (priv == NULL)
		return ENXIO;

	/* send TUR */
	rc = scsi_tur(priv);
	if (rc != 0)
		return EIO;

	/* read disklabel. We expect a VDM label since this is the only way
	 * we can boot from disk.  */
	rc = scsi_read(priv, VDM_LABEL_SECTOR, sizeof buf, buf, &z);
	if (rc != 0 || z != sizeof buf)
		return EIO;

	vdl = (struct vdm_label *)(buf + VDM_LABEL_OFFSET);
	if (vdl->signature != VDM_LABEL_SIGNATURE)
		vdl = (struct vdm_label *)(buf + VDM_LABEL_OFFSET_ALT);
	if (vdl->signature != VDM_LABEL_SIGNATURE)
		return EINVAL;
	
	/* XXX ought to search for an OpenBSD vdmpart too. Too lazy for now */
	rc = scsi_read(priv, LABELSECTOR, sizeof buf, buf, &z);
	if (rc != 0 || z != sizeof buf)
		return EIO;

	msg = getdisklabel(buf, &priv->label);
	if (msg != NULL) {
		printf("%s\n", msg);
		return EINVAL;
	}

	return 0;
}

int
sdstrategy(void *devdata, int rw, daddr32_t blk, size_t size, void *buf,
    size_t *rsize)
{
	struct scsi_private *priv = devdata;

	if (rw != F_READ)
		return EROFS;

	blk += priv->label.d_partitions[priv->part].p_offset;

	return scsi_read(priv, blk, size, buf, rsize) != 0 ? EIO : 0;
}

int
sdclose(struct open_file *f)
{
	struct scsi_private *priv;

	if (f->f_devdata != NULL) {
		priv = (struct scsi_private *)f->f_devdata;
		if (priv->scsicookie != NULL)
			(*priv->scsidetach)(priv->scsicookie);
		free(priv, sizeof(struct scsi_private), 0);
		f->f_devdata = NULL;
	}

	return 0;
}
