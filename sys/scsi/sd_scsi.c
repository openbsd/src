/*	$OpenBSD: sd_scsi.c,v 1.12 2005/05/25 20:52:41 krw Exp $	*/
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

struct sd_scsibus_mode_sense_data {
	struct scsi_mode_header header;
	struct scsi_blk_desc blk_desc;
	union scsi_disk_pages pages;
};

int	sd_scsibus_get_parms(struct sd_softc *,
	    struct disk_parms *, int);
int	sd_scsibus_get_optparms(struct sd_softc *,
	    struct disk_parms *, int);
void	sd_scsibus_flush(struct sd_softc *, int);

const struct sd_ops sd_scsibus_ops = {
	sd_scsibus_get_parms,
	sd_scsibus_flush,
};

int
sd_scsibus_get_optparms(sd, dp, flags)
	struct sd_softc *sd;
	struct disk_parms *dp;
	int flags;
{
	struct sd_scsibus_mode_sense_data scsi_sense;
	int error;

	dp->blksize = 512;
	dp->disksize = scsi_size(sd->sc_link, flags);
	if (dp->disksize == 0)
		return (SDGP_RESULT_OFFLINE);		/* XXX? */

	/* XXX
	 * It is better to get the following params from the
	 * mode sense page 6 only (optical device parameter page).
	 * However, there are stupid optical devices which does NOT
	 * support the page 6. Ask for all (0x3f) pages. Ghaa....
	 */
	error = scsi_mode_sense(sd->sc_link, 0, 0x3f,
	    (struct scsi_mode_header *)&scsi_sense, sizeof(scsi_sense), flags,
	    6000);
	if (error != 0)
		return (SDGP_RESULT_OFFLINE);		/* XXX? */

	dp->blksize = _3btol(scsi_sense.blk_desc.blklen);
	if (dp->blksize == 0) 
		dp->blksize = 512;

	/*
	 * Create a pseudo-geometry.
	 */
	dp->heads = 64;
	dp->sectors = 32;
	dp->cyls = dp->disksize / (dp->heads * dp->sectors);

	return (SDGP_RESULT_OK);
}

/*
 * Get the scsi driver to send a full inquiry to the * device and use the
 * results to fill out the disk parameter structure.
 */
int
sd_scsibus_get_parms(sd, dp, flags)
	struct sd_softc *sd;
	struct disk_parms *dp;
	int flags;
{
	struct sd_scsibus_mode_sense_data scsi_sense;
	struct scsi_mode_sense_buf buf;
	union scsi_disk_pages *sense_pages = NULL;
	u_int16_t rpm = 0;
	int page, error, blksize;

	dp->rot_rate = 3600;

	/*
	 * If offline, the SDEV_MEDIA_LOADED flag will be
	 * cleared by the caller if necessary.
	 */
	if (sd->type == T_OPTICAL)
		return (sd_scsibus_get_optparms(sd, dp, flags));

	error = scsi_do_mode_sense(sd->sc_link, page = 4, &buf,
	    (void **)&sense_pages, NULL, NULL, &blksize,
	    sizeof(sense_pages->rigid_geometry), flags | SCSI_SILENT);
	if (error == 0) {
		if (sense_pages) { 
			SC_DEBUG(sd->sc_link, SDEV_DB3,
			    ("%d cyls, %d heads, %d precomp, %d red_write,"
			     " %d land_zone\n",
			    _3btol(sense_pages->rigid_geometry.ncyl),
			    sense_pages->rigid_geometry.nheads,
			    _2btol(sense_pages->rigid_geometry.st_cyl_wp),
			    _2btol(sense_pages->rigid_geometry.st_cyl_rwc),
			    _2btol(sense_pages->rigid_geometry.land_zone)));
			/*
			 * KLUDGE!! (for zone recorded disks)
			 * give a number of sectors so that sec * trks * cyls
			 * is <= disk_size
			 * can lead to wasted space! THINK ABOUT THIS !
			 */
			dp->heads = sense_pages->rigid_geometry.nheads;
			dp->cyls = _3btol(sense_pages->rigid_geometry.ncyl);
			rpm = _2btol(sense_pages->rigid_geometry.rpm);
		}	
		dp->disksize = scsi_size(sd->sc_link, flags);

		if (dp->disksize == 0 || dp->heads == 0 || dp->cyls == 0)
			goto fake_it;

		/* XXX dubious on SCSI */
		dp->sectors = dp->disksize / (dp->heads * dp->cyls);

		if (rpm)
			dp->rot_rate = rpm;

		dp->blksize = (blksize == 0) ? 512 : blksize;

		return (SDGP_RESULT_OK);
	}

	error = scsi_do_mode_sense(sd->sc_link, page = 5, &buf,
	    (void **)&sense_pages, NULL, NULL, &blksize,
	    sizeof(sense_pages->flex_geometry), flags | SCSI_SILENT);
	if (error == 0) {
		if (sense_pages) {
			dp->heads = sense_pages->flex_geometry.nheads;
			dp->cyls = _2btol(sense_pages->flex_geometry.ncyl);
			dp->sectors = sense_pages->flex_geometry.ph_sec_tr;
			dp->blksize =
			    _2btol(sense_pages->flex_geometry.bytes_s);
			rpm = _2btol(scsi_sense.pages.flex_geometry.rpm);
		}	
		if (dp->cyls == 0 || dp->heads == 0 || dp->cyls == 0)
			goto fake_it;

		dp->disksize = dp->heads * dp->cyls * dp->sectors;
			
		if (rpm)
			dp->rot_rate = rpm;

		if (blksize)
			dp->blksize = blksize;
		else if (dp->blksize == 0)
			dp->blksize = 512;

		return (SDGP_RESULT_OK);
	}

	/* T_RDIRECT defines page 6. */
	if (sd->type != T_RDIRECT)
		goto fake_it;

	error = scsi_do_mode_sense(sd->sc_link, page = 6, &buf,
	    (void **)&sense_pages, NULL, NULL, &blksize,
	    sizeof(sense_pages->reduced_geometry), flags | SCSI_SILENT);
	if (error == 0) {
		dp->heads = 64;
		dp->sectors = 32;
		if (sense_pages) {
			dp->disksize =
			    _4btol(sense_pages->reduced_geometry.sectors+1);
			dp->blksize =
			    _2btol(sense_pages->reduced_geometry.bytes_s);
		    	dp->sectors = sense_pages->reduced_geometry.sectors[0];
		}
		if (dp->disksize == 0 || dp->sectors == 0)
			goto fake_it;

		dp->cyls = dp->disksize / (dp->heads * dp->sectors);

		if (blksize)
			dp->blksize = blksize;

		if (dp->blksize == 0)
			dp->blksize = 512;

		return (SDGP_RESULT_OK);
	}

fake_it:
	/* If we can get the disk size, fake a geometry. */
	dp->disksize = scsi_size(sd->sc_link, flags);
	if (dp->disksize == 0)
		return (SDGP_RESULT_OFFLINE);
	SC_DEBUG(sd->sc_link, SDEV_DB1, ("error %d on pg %d, fake geometry.\n",
	    error, page));

	/* Use adaptec standard fictitious geometry. */

	dp->heads = 64;
	dp->sectors = 32;
	dp->cyls = dp->disksize / (64 * 32);
	dp->blksize = 512;

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
