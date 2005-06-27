/*	$OpenBSD: sd_scsi.c,v 1.22 2005/06/27 23:50:43 krw Exp $	*/
/*	$NetBSD: sd_scsi.c,v 1.8 1998/10/08 20:21:13 thorpej Exp $	*/

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
 * Originally written by Julian Elischer (julian@dialix.oz.au)
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
 * Ported to run under 386BSD by Julian Elischer (julian@dialix.oz.au) Sept 1992
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/buf.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/conf.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>
#include <scsi/sdvar.h>

int	sd_scsibus_get_parms(struct sd_softc *,
	    struct disk_parms *, int);
void	sd_scsibus_flush(struct sd_softc *, int);

const struct sd_ops sd_scsibus_ops = {
	sd_scsibus_get_parms,
	sd_scsibus_flush,
};

/*
 * Fill out the disk parameter structure. Return SDGP_RESULT_OK if the
 * structure is correctly filled in, SDGP_RESULT_OFFLINE otherwise. The caller
 * is responsible for clearing the SDEV_MEDIA_LOADED flag if the structure
 * cannot be completed.
 */
int
sd_scsibus_get_parms(sd, dp, flags)
	struct sd_softc *sd;
	struct disk_parms *dp;
	int flags;
{
	struct scsi_mode_sense_buf buf;
	union scsi_disk_pages *sense_pages = NULL;
	u_int32_t heads = 0, sectors = 0, cyls = 0, blksize;
	u_int16_t rpm = 0;

	dp->disksize = scsi_size(sd->sc_link, flags, &blksize);

	switch (sd->type) {
	case T_OPTICAL:
		/* No more information needed or available. */
		break;

	case T_RDIRECT:
		/* T_RDIRECT only supports RBC Device Parameter Page (6). */
		scsi_do_mode_sense(sd->sc_link, 6, &buf, (void **)&sense_pages,
		    NULL, NULL, &blksize, sizeof(sense_pages->reduced_geometry),
		    flags | SCSI_SILENT, NULL);
		if (sense_pages) {
			if (dp->disksize == 0)
				dp->disksize = _5btol(sense_pages->
				    reduced_geometry.sectors);
			if (blksize == 0)
				blksize = _2btol(sense_pages->
				    reduced_geometry.bytes_s);
		}
		break;

	default:
		/*
		 * For other devices try mode sense page 4 (RIGID GEOMETRY) and
		 * if that doesn't work try page 5 (FLEX GEOMETRY).
		 */
		scsi_do_mode_sense(sd->sc_link, 4, &buf, (void **)&sense_pages,
		    NULL, NULL, &blksize, sizeof(sense_pages->rigid_geometry),
		    flags | SCSI_SILENT, NULL);
		if (sense_pages) { 
			heads = sense_pages->rigid_geometry.nheads;
			cyls = _3btol(sense_pages->rigid_geometry.ncyl);
			rpm = _2btol(sense_pages->rigid_geometry.rpm);
			if (heads * cyls > 0)
				sectors = dp->disksize / (heads * cyls);
		} else {
			scsi_do_mode_sense(sd->sc_link, 5, &buf,
			    (void **)&sense_pages, NULL, NULL, &blksize,
			    sizeof(sense_pages->flex_geometry),
			    flags | SCSI_SILENT, NULL);
			if (sense_pages) {
				sectors = sense_pages->flex_geometry.ph_sec_tr;
				heads = sense_pages->flex_geometry.nheads;
				cyls = _2btol(sense_pages->flex_geometry.ncyl);
				rpm = _2btol(sense_pages->flex_geometry.rpm);
				if (blksize == 0)
					blksize = _2btol(sense_pages->
					    flex_geometry.bytes_s);
				if (dp->disksize == 0)
					dp->disksize = heads * cyls * sectors;
			}	
		}
		break;
	}

	if (dp->disksize == 0)
		return (SDGP_RESULT_OFFLINE);

	/*
	 * Use Adaptec standard geometry values for anything we still don't
	 * know.
	 */

	dp->heads = (heads == 0) ? 64 : heads;
	dp->blksize = (blksize == 0) ? 512 : blksize;
	dp->sectors = (sectors == 0) ? 32 : sectors;
	dp->rot_rate = (rpm == 0) ? 3600 : rpm;

	/*
	 * XXX THINK ABOUT THIS!!  Using values such that sectors * heads *
	 * cyls is <= disk_size can lead to wasted space. We need a more
	 * careful calculation/validation to make everything work out
	 * optimally.
	 */
	dp->cyls = (cyls == 0) ? dp->disksize / (dp->heads * dp->sectors) :
	    cyls;

	return (SDGP_RESULT_OK);
}

void
sd_scsibus_flush(sd, flags)
	struct sd_softc *sd;
	int flags;
{
	struct scsi_link *sc_link = sd->sc_link;
	struct scsi_synchronize_cache sync_cmd;

	/*
	 * If the device is SCSI-2, issue a SYNCHRONIZE CACHE.
	 * We issue with address 0 length 0, which should be
	 * interpreted by the device as "all remaining blocks
	 * starting at address 0".  We ignore ILLEGAL REQUEST
	 * in the event that the command is not supported by
	 * the device, and poll for completion so that we know
	 * that the cache has actually been flushed.
	 *
	 * Unless, that is, the device can't handle the SYNCHRONIZE CACHE
	 * command, as indicated by our quirks flags.
	 *
	 * XXX What about older devices?
	 */
	if ((sc_link->scsi_version & SID_ANSII) >= 2 &&
	    (sc_link->quirks & SDEV_NOSYNCCACHE) == 0) {
		bzero(&sync_cmd, sizeof(sync_cmd));
		sync_cmd.opcode = SYNCHRONIZE_CACHE;

		if (scsi_scsi_cmd(sc_link,
		    (struct scsi_generic *)&sync_cmd, sizeof(sync_cmd),
		    NULL, 0, SDRETRIES, 100000, NULL,
		    flags|SCSI_IGNORE_ILLEGAL_REQUEST))
			printf("%s: WARNING: cache synchronization failed\n",
			    sd->sc_dev.dv_xname);
		else
			sd->flags |= SDF_FLUSHING;
	}
}
