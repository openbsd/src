/*	$OpenBSD: cd_atapi.c,v 1.3 2004/05/09 14:08:11 krw Exp $	*/
/*	$NetBSD: cd_atapi.c,v 1.10 1998/08/31 22:28:06 cgd Exp $	*/

/*
 * Copyright (c) 1997 Manuel Bouyer.  All rights reserved.
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
 *	This product includes software developed by Charles M. Hannum.
 * 4. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
 * NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
#include <scsi/atapi_all.h>
#include <scsi/atapi_cd.h>
#include <scsi/cdvar.h>
#include <scsi/scsiconf.h>


static int cd_atapibus_setchan(struct cd_softc *, int, int, int, int, int);
static int cd_atapibus_getvol(struct cd_softc *, struct ioc_vol *, int);
static int cd_atapibus_setvol(struct cd_softc *, const struct ioc_vol *,
	    int);
static int cd_atapibus_set_pa_immed(struct cd_softc *, int);
static int cd_atapibus_load_unload(struct cd_softc *, int, int);

const struct cd_ops cd_atapibus_ops = {
	cd_atapibus_setchan,
	cd_atapibus_getvol,
	cd_atapibus_setvol,
	cd_atapibus_set_pa_immed,
	cd_atapibus_load_unload,
};

static int
cd_atapibus_setchan(cd, p0, p1, p2, p3, flags)
	struct cd_softc *cd;
	int p0, p1, p2, p3, flags;
{
	struct atapi_cd_mode_data data;
	int error;

	if ((error = atapi_mode_sense(cd->sc_link, ATAPI_AUDIO_PAGE,
	    (struct atapi_mode_header *)&data, AUDIOPAGESIZE, flags,
	    CDRETRIES, 20000)) != 0)
		return (error);
	data.pages.audio.port[LEFT_PORT].channels = p0;
	data.pages.audio.port[RIGHT_PORT].channels = p1;
	data.pages.audio.port[2].channels = p2;
	data.pages.audio.port[3].channels = p3;
	return (atapi_mode_select(cd->sc_link,
	    (struct atapi_mode_header *)&data, AUDIOPAGESIZE, flags,
	    CDRETRIES, 20000));
}

static int
cd_atapibus_getvol(cd, arg, flags)
	struct cd_softc *cd;
	struct ioc_vol *arg;
	int flags;
{
	struct atapi_cd_mode_data data;
	int error;

	if ((error = atapi_mode_sense(cd->sc_link, ATAPI_AUDIO_PAGE,
	    (struct atapi_mode_header *)&data, AUDIOPAGESIZE, flags,
	    CDRETRIES, 20000)) != 0)
		return (error);
	arg->vol[0] = data.pages.audio.port[0].volume;
	arg->vol[1] = data.pages.audio.port[1].volume;
	arg->vol[2] = data.pages.audio.port[2].volume;
	arg->vol[3] = data.pages.audio.port[3].volume;
	return (0);
}

static int
cd_atapibus_setvol(cd, arg, flags)
	struct cd_softc *cd;
	const struct ioc_vol *arg;
	int flags;
{
	struct atapi_cd_mode_data data, mask;
	int error;

	if ((error = atapi_mode_sense(cd->sc_link, ATAPI_AUDIO_PAGE,
	    (struct atapi_mode_header *)&data, AUDIOPAGESIZE, flags,
	    CDRETRIES, 20000)) != 0)
		return (error);
	if ((error = atapi_mode_sense(cd->sc_link, ATAPI_AUDIO_PAGE_MASK,
	    (struct atapi_mode_header *)&mask, AUDIOPAGESIZE, flags,
	    CDRETRIES, 20000)) != 0)
		return (error);

	data.pages.audio.port[0].volume = arg->vol[0] &
	    mask.pages.audio.port[0].volume;
	data.pages.audio.port[1].volume = arg->vol[1] &
	    mask.pages.audio.port[1].volume;
	data.pages.audio.port[2].volume = arg->vol[2] &
	    mask.pages.audio.port[2].volume;
	data.pages.audio.port[3].volume = arg->vol[3] &
	    mask.pages.audio.port[3].volume;

	return (atapi_mode_select(cd->sc_link,
	    (struct atapi_mode_header *)&data, AUDIOPAGESIZE,
	    flags, CDRETRIES, 20000));
}

static int
cd_atapibus_set_pa_immed(cd, flags)
	struct cd_softc *cd;
	int flags;
{

	/* Noop. */
	return (0);
}

static int
cd_atapibus_load_unload(cd, options, slot)
	struct cd_softc *cd;
	int options, slot;
{
	struct atapi_load_unload atapi_cmd;

	bzero(&atapi_cmd, sizeof(atapi_cmd));
	atapi_cmd.opcode = ATAPI_LOAD_UNLOAD;
	atapi_cmd.options = options;    /* ioctl uses ATAPI values */
	atapi_cmd.slot = slot;
	return (scsi_scsi_cmd(cd->sc_link,
		(struct scsi_generic *)&atapi_cmd, sizeof(atapi_cmd),
		0, 0, CDRETRIES, 200000, NULL, 0));
}
