/*	$OpenBSD: cd_scsi.c,v 1.5 2004/05/09 14:08:11 krw Exp $	*/
/*	$NetBSD: cd_scsi.c,v 1.14 1998/08/31 22:28:06 cgd Exp $	*/

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

/*
 * Originally written by Julian Elischer (julian@tfs.com)
 * for TRW Financial Systems for use under the MACH(2.5) operating system.
 *
 * TRW Financial Systems, in accordance with their agreement with Carnegie
 * Mellon University, makes this software available to CMU to distribute
 * or use in any manner that they see fit as long as this message is kept with
 * the software. For this reason TFS also grants any other persons or
 * organisations permission to use or modify this software.
 *
 * TFS supplies this software to be publicly redistributed
 * on the understanding that TFS is not responsible for the correct
 * functioning of this software in any circumstances.
 *
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/file.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/buf.h>
#include <sys/conf.h>
#if NRND > 0
#include <sys/rnd.h>
#endif

#include <sys/cdio.h>

#include <scsi/scsi_all.h>
#include <scsi/cd.h>
#include <scsi/scsi_cd.h>
#include <scsi/scsiconf.h>
#include <scsi/cdvar.h>

int	cd_scsibus_get_mode(struct cd_softc *,
	    struct scsi_cd_mode_data *, int, int, int);
int	cd_scsibus_set_mode(struct cd_softc *,
	    struct scsi_cd_mode_data *, int, int);

int	cd_scsibus_setchan(struct cd_softc *, int, int, int, int, int);
int	cd_scsibus_getvol(struct cd_softc *, struct ioc_vol *, int);
int	cd_scsibus_setvol(struct cd_softc *, const struct ioc_vol *,
	    int);
int	cd_scsibus_set_pa_immed(struct cd_softc *, int);
int	cd_scsibus_load_unload(struct cd_softc *, int, int);

const struct cd_ops cd_scsibus_ops = {
	cd_scsibus_setchan,
	cd_scsibus_getvol,
	cd_scsibus_setvol,
	cd_scsibus_set_pa_immed,
	cd_scsibus_load_unload,
};

/*
 * Get the requested page into the buffer given
 */
int
cd_scsibus_get_mode(cd, data, page, len, flags)
	struct cd_softc *cd;
	struct scsi_cd_mode_data *data;
	int page, len, flags;
{
	struct scsi_mode_sense scsi_cmd;

#ifdef DIAGNOSTIC
	if (len == 0 || len > 256) {
		printf ("cd_scsibus_get_mode: Mode page %02x request "
		    "bad size: %d bytes\n", page, len);
		return (EINVAL);
	}
#endif

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(data, sizeof(*data));
	scsi_cmd.opcode = MODE_SENSE;
	scsi_cmd.page = page;
	scsi_cmd.length = len & 0xff;
	return (scsi_scsi_cmd(cd->sc_link,
	    (struct scsi_generic *)&scsi_cmd, sizeof(scsi_cmd),
	    (u_char *)data, len, CDRETRIES, 20000, NULL,
	    SCSI_DATA_IN));
}

/*
 * Get the requested page into the buffer given
 */
int
cd_scsibus_set_mode(cd, data, len, flags)
	struct cd_softc *cd;
	struct scsi_cd_mode_data *data;
	int len, flags;
{
	struct scsi_mode_select scsi_cmd;

#ifdef DIAGNOSTIC
	if (len == 0 || len > 256) {
		printf ("cd_scsibus_set_mode: Set mode request "
		    "bad size: %d bytes\n", len);
		return (EINVAL);
	}
#endif

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SELECT;
	scsi_cmd.byte2 |= SMS_PF;
	scsi_cmd.length = len & 0xff;

	/* SPC-2 revision 16, section 8.3: Mode parameters
	   When used with the [MODE SELECT command], the data
	   length field is reserved. */
	data->header.data_length = 0;

	return (scsi_scsi_cmd(cd->sc_link,
	    (struct scsi_generic *)&scsi_cmd, sizeof(scsi_cmd),
	    (u_char *)data, len, CDRETRIES, 20000, NULL,
	    SCSI_DATA_OUT));
}

int
cd_scsibus_set_pa_immed(cd, flags)
	struct cd_softc *cd;
	int flags;
{
	struct scsi_cd_mode_data data;
	int error;

	if ((error = cd_scsibus_get_mode(cd, &data, AUDIO_PAGE,
	    AUDIOPAGESIZE, flags)) != 0)
		return (error);
	data.page.audio.flags &= ~CD_PA_SOTC;
	data.page.audio.flags |= CD_PA_IMMED;
	return (cd_scsibus_set_mode(cd, &data, AUDIOPAGESIZE, flags));
}

int
cd_scsibus_setchan(cd, p0, p1, p2, p3, flags)
	struct cd_softc *cd;
	int p0, p1, p2, p3;
	int flags;
{
	struct scsi_cd_mode_data data;
	int error;

	if ((error = cd_scsibus_get_mode(cd, &data, AUDIO_PAGE,
	    AUDIOPAGESIZE, flags)) != 0)
		return (error);
	data.page.audio.port[LEFT_PORT].channels = p0;
	data.page.audio.port[RIGHT_PORT].channels = p1;
	data.page.audio.port[2].channels = p2;
	data.page.audio.port[3].channels = p3;
	return (cd_scsibus_set_mode(cd, &data, AUDIOPAGESIZE, flags));
}

int
cd_scsibus_getvol(cd, arg, flags)
	struct cd_softc *cd;
	struct ioc_vol *arg;
	int flags;
{

	struct scsi_cd_mode_data data;
	int error;

	if ((error = cd_scsibus_get_mode(cd, &data, AUDIO_PAGE,
	    AUDIOPAGESIZE, flags)) != 0)
		return (error);
	arg->vol[LEFT_PORT] = data.page.audio.port[LEFT_PORT].volume;
	arg->vol[RIGHT_PORT] = data.page.audio.port[RIGHT_PORT].volume;
	arg->vol[2] = data.page.audio.port[2].volume;
	arg->vol[3] = data.page.audio.port[3].volume;
	return (0);
}

int
cd_scsibus_setvol(cd, arg, flags)
	struct cd_softc *cd;
	const struct ioc_vol *arg;
	int flags;
{
	struct scsi_cd_mode_data data;
	int error;

	if ((error = cd_scsibus_get_mode(cd, &data, AUDIO_PAGE,
	    AUDIOPAGESIZE, flags)) != 0)
		return (error);
	data.page.audio.port[LEFT_PORT].channels = CHANNEL_0;
	data.page.audio.port[LEFT_PORT].volume = arg->vol[LEFT_PORT];
	data.page.audio.port[RIGHT_PORT].channels = CHANNEL_1;
	data.page.audio.port[RIGHT_PORT].volume = arg->vol[RIGHT_PORT];
	data.page.audio.port[2].volume = arg->vol[2];
	data.page.audio.port[3].volume = arg->vol[3];
	return (cd_scsibus_set_mode(cd, &data, AUDIOPAGESIZE, flags));
}

int
cd_scsibus_load_unload(cd, options, slot)
	struct cd_softc *cd;
	int options, slot;
{
	/*
	 * Not supported on SCSI CDs that we know of (but we'll leave
	 * the hook here Just In Case).
	 */
	return (ENODEV);
}
