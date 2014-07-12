/*	$OpenBSD: scsi.c,v 1.4 2014/07/12 19:01:49 tedu Exp $	*/

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

#include "libsa.h"
#include "prom.h"

#include "scsi.h"
#include <scsi/scsi_disk.h>

struct scsi_private *
scsi_initialize(const char *ctrlname, int ctrl, int unit, int lun, int part)
{
	struct scsi_private *priv;

	priv = alloc(sizeof(struct scsi_private));
	if (priv == NULL)
		return NULL;

	memset(priv, 0, sizeof(struct scsi_private));
	priv->part = part;

	/* provide default based upon system type */
	if (*ctrlname == '\0') {
		switch (cpuid()) {
		case AVIION_300_310:
		case AVIION_400_4000:
		case AVIION_410_4100:
		case AVIION_300C_310C:
		case AVIION_300CD_310CD:
		case AVIION_300D_310D:
		case AVIION_4300_25:
		case AVIION_4300_20:
		case AVIION_4300_16:
			ctrlname = "insc";
			break;
		case AVIION_4600_530:
			ctrlname = "ncsc";
			break;
		}
	}

	if (strcmp(ctrlname, "insc") == 0) {
		if (ctrl == 0) {
			*(volatile uint32_t *)0xfff840c0 = 0x6e;
			ctrl = 0xfff8a000;
		} else
			goto done;

		if (badaddr((void *)ctrl, 4) != 0)
			goto done;

		/* initialize controller */
		priv->scsicookie = oaic_attach(ctrl, unit, lun);
		priv->scsicmd = oaic_scsicmd;
		priv->scsidetach = oaic_detach;
	} else
	if (strcmp(ctrlname, "ncsc") == 0) {
		if (ctrl == 0)
			ctrl = 0xfffb0000;
		else if (ctrl == 1)
			ctrl = 0xfffb0080;
		else
			goto done;

		if (badaddr((void *)ctrl, 4) != 0)
			goto done;

		/* initialize controller */
		priv->scsicookie = oosiop_attach(ctrl, unit, lun);
		priv->scsicmd = oosiop_scsicmd;
		priv->scsidetach = oosiop_detach;
	}

done:
	if (priv->scsicookie == NULL) {
		free(priv, sizeof(struct scsi_private));
		priv = NULL;
	}

	return priv;
}

int
scsi_tur(struct scsi_private *priv)
{
	struct scsi_test_unit_ready cmd;
	int i, rc;

	for (i = TEST_READY_RETRIES; i != 0; i--) {
		memset(&cmd, 0, sizeof cmd);
		cmd.opcode = TEST_UNIT_READY;

		rc = (*priv->scsicmd)(priv->scsicookie,
		    &cmd, sizeof cmd, NULL, 0, NULL);
		if (rc == 0)
			break;
	}

	return rc;
}

int
scsi_read(struct scsi_private *priv, daddr32_t blk, size_t size, void *buf,
    size_t *rsize)
{
	union {
		struct scsi_rw rw;
		struct scsi_rw_big rw_big;
		struct scsi_rw_12 rw_12;
	} cmd;
	int nsecs;
	size_t cmdlen;
	int i, rc;

	nsecs = (size + DEV_BSIZE - 1) >> _DEV_BSHIFT;

	for (i = SCSI_RETRIES; i != 0; i--) {
		memset(&cmd, 0, sizeof cmd);

		/* XXX SDEV_ONLYBIG quirk */
		if ((blk & 0x1fffff) == blk && (nsecs & 0xff) == nsecs) {
			cmd.rw.opcode = READ_COMMAND;
			_lto3b(blk, cmd.rw.addr);
			cmd.rw.length = nsecs;
			cmdlen = sizeof cmd.rw;
		} else if ((nsecs & 0xffff) == nsecs) {
			cmd.rw_big.opcode = READ_BIG;
			_lto4b(blk, cmd.rw_big.addr);
			_lto2b(nsecs, cmd.rw_big.length);
			cmdlen = sizeof cmd.rw_big;
		} else {
			cmd.rw_12.opcode = READ_12;
			_lto4b(blk, cmd.rw_12.addr);
			_lto4b(nsecs, cmd.rw_12.length);
			cmdlen = sizeof cmd.rw_12;
		}

		rc = (*priv->scsicmd)(priv->scsicookie,
		    &cmd, sizeof cmd, buf, size, rsize);
		if (rc == 0)
			break;
	}

	return rc;
}
