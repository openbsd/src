/*	$OpenBSD: scsi.c,v 1.1 2013/10/10 21:22:06 miod Exp $	*/

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
#include <scsi/scsi_disk.h>

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
