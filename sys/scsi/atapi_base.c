/*	$OpenBSD: atapi_base.c,v 1.1 1999/07/20 06:21:59 csapuntz Exp $	*/
/*	$NetBSD: atapi_base.c,v 1.12 1999/06/25 18:58:54 thorpej Exp $	*/

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *        This product includes software developed by the NetBSD
 *        Foundation, Inc. and its contributors.
 * 4. Neither the name of The NetBSD Foundation nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/proc.h>

#include <scsi/scsi_all.h>
#include <scsi/atapi_all.h>
#include <scsi/scsiconf.h>

#ifndef __OpenBSD__
/*
 * Look at the returned sense and act on the error, determining
 * the unix error number to pass back.  (0 = report no error)
 *
 * THIS IS THE DEFAULT ERROR HANDLER
 */
int
atapi_interpret_sense(xs)
	struct scsipi_xfer *xs;
{
	int key, error;
	struct scsipi_link *sc_link = xs->sc_link;
	char *msg = NULL;

	/*
	 * If the device has it's own error handler, call it first.
	 * If it returns a legit error value, return that, otherwise
	 * it wants us to continue with normal error processing.
	 */
	if (sc_link->device->err_handler) {
		SC_DEBUG(sc_link, SDEV_DB2,
		    ("calling private err_handler()\n"));
		error = (*sc_link->device->err_handler) (xs);
		if (error != SCSIRET_CONTINUE)
			return (error);		/* error >= 0  better ? */
	}
	/*
	 * otherwise use the default, call the generic sense handler if we have
	 * more than the sense key
	 */
	if (xs->error == XS_SENSE)
		return (scsipi_interpret_sense(xs));

	key = (xs->sense.atapi_sense & 0xf0) >> 4;
	switch (key) {
		case SKEY_RECOVERED_ERROR:
			msg = "soft error (corrected)";
		case SKEY_NO_SENSE:
			if (xs->resid == xs->datalen)
				xs->resid = 0;  /* not short read */
			error = 0;
			break;
		case SKEY_NOT_READY:
			if ((sc_link->flags & SDEV_REMOVABLE) != 0)
				sc_link->flags &= ~SDEV_MEDIA_LOADED;
			if ((xs->flags & SCSI_IGNORE_NOT_READY) != 0)
				return (0);
			if ((xs->flags & SCSI_SILENT) != 0)
				return (EIO);
			msg = "not ready";
			error = EIO;
			break;
		case SKEY_MEDIUM_ERROR: /* MEDIUM ERROR */
			msg = "medium error";
			error = EIO;
			break;
		case SKEY_HARDWARE_ERROR:
			msg = "non-media hardware failure";
			error = EIO;
			break;
		case SKEY_ILLEGAL_REQUEST:
			if ((xs->flags & SCSI_IGNORE_ILLEGAL_REQUEST) != 0)
				return (0);
			if ((xs->flags & SCSI_SILENT) != 0)
				return (EIO);
			msg = "illegal request";
			error = EINVAL;
			break;
		case SKEY_UNIT_ATTENTION:
			if ((sc_link->flags & SDEV_REMOVABLE) != 0)
				sc_link->flags &= ~SDEV_MEDIA_LOADED;
			if ((xs->flags & SCSI_IGNORE_MEDIA_CHANGE) != 0 ||
			    /* XXX Should reupload any transient state. */
			    (sc_link->flags & SDEV_REMOVABLE) == 0)
				return (ERESTART);
			if ((xs->flags & SCSI_SILENT) != 0)
				return (EIO);
			msg = "unit attention";
			error = EIO;
			break;
		case SKEY_WRITE_PROTECT:
			msg = "readonly device";
			error = EROFS;
			break;
		case SKEY_ABORTED_COMMAND:
			msg = "command aborted";
			error = ERESTART;
			break;
		default:
			error = EIO;
			break;
	}

	if (!key) {
		if (xs->sense.atapi_sense & 0x01) {
			/* Illegal length indication */
			msg = "ATA illegal length indication";
			error = EIO;
		}
		if (xs->sense.atapi_sense & 0x02) { /* vol overflow */
			msg = "ATA volume overflow";
			error = ENOSPC;
		}
		if (xs->sense.atapi_sense & 0x04) { /* Aborted command */
			msg = "ATA command aborted";
			error = ERESTART;
		}
	}
	if (msg) {
		sc_link->sc_print_addr(sc_link);
		printf("%s\n", msg);
	} else {
		if (error) {
			sc_link->sc_print_addr(sc_link);
			printf("unknown error code %d\n",
			    xs->sense.atapi_sense);
		}
	}

	return (error);

}

/*
 * Utility routines often used in SCSI stuff
 */


/*
 * Print out the scsi_link structure's address info.
 */
void
atapi_print_addr(sc_link)
	struct scsipi_link *sc_link;
{

	printf("%s(%s:%d:%d): ",
	    sc_link->device_softc ?
	    ((struct device *)sc_link->device_softc)->dv_xname : "probe",
	    ((struct device *)sc_link->adapter_softc)->dv_xname,
	    sc_link->scsipi_atapi.channel, sc_link->scsipi_atapi.drive);
}

/*
 * ask the atapi driver to perform a command for us.
 * tell it where to read/write the data, and how
 * long the data is supposed to be. If we have  a buf
 * to associate with the transfer, we need that too.
 */
int
atapi_scsipi_cmd(sc_link, scsipi_cmd, cmdlen, data_addr, datalen,
    retries, timeout, bp, flags)
	struct scsipi_link *sc_link;
	struct scsipi_generic *scsipi_cmd;
	int cmdlen;
	u_char *data_addr;
	int datalen;
	int retries;
	int timeout;
	struct buf *bp;
	int flags;
{
	struct scsipi_xfer *xs;
	int error, s;

	SC_DEBUG(sc_link, SDEV_DB2, ("atapi_cmd\n"));

#ifdef DIAGNOSTIC
	if (bp != 0 && (flags & SCSI_NOSLEEP) == 0)
		panic("atapi_scsipi_cmd: buffer without nosleep");
#endif

	if ((xs = scsipi_make_xs(sc_link, scsipi_cmd, cmdlen, data_addr,
	    datalen, retries, timeout, bp, flags)) == NULL)
		return (ENOMEM);

	xs->cmdlen = (sc_link->scsipi_atapi.cap & ACAP_LEN) ? 16 : 12;

	if ((error = scsipi_execute_xs(xs)) == EJUSTRETURN)
		return (0);

	/*
	 * we have finished with the xfer stuct, free it and
	 * check if anyone else needs to be started up.
	 */
	s = splbio();
	scsipi_free_xs(xs, flags);
	splx(s);
	return (error);
}

#endif

int
atapi_mode_select(l, data, len, flags, retries, timeout)
	struct scsi_link *l;
	struct atapi_mode_header *data;
	int len, flags, retries, timeout;
{
	struct atapi_mode_select scsi_cmd;
	int error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = ATAPI_MODE_SELECT;
	scsi_cmd.byte2 = AMS_PF;
	_lto2b(len, scsi_cmd.length);

	/* length is reserved when doing mode select; zero it */
	_lto2l(0, data->length);

	error = scsi_scsi_cmd(l, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (void *)data, len, retries, timeout, NULL,
	    flags | SCSI_DATA_OUT);
	SC_DEBUG(l, SDEV_DB2, ("atapi_mode_select: error=%d\n", error));
	return (error);
}

int
atapi_mode_sense(l, page, data, len, flags, retries, timeout)
	struct scsi_link *l;
	int page, len, flags, retries, timeout;
	struct atapi_mode_header *data;
{
	struct atapi_mode_sense scsi_cmd;
	int error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = ATAPI_MODE_SENSE;
	scsi_cmd.page = page;
	_lto2b(len, scsi_cmd.length);

	error = scsi_scsi_cmd(l, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (void *)data, len, retries, timeout, NULL,
	    flags | SCSI_DATA_IN);
	SC_DEBUG(l, SDEV_DB2, ("atapi_mode_sense: error=%d\n", error));
	return (error);
}
