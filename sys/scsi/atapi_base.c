/*	$OpenBSD: atapi_base.c,v 1.2 2004/05/09 14:08:11 krw Exp $	*/
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
