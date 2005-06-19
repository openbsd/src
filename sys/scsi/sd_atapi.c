/*	$OpenBSD: sd_atapi.c,v 1.10 2005/06/19 20:41:28 krw Exp $	*/
/*	$NetBSD: sd_atapi.c,v 1.3 1998/08/31 22:28:07 cgd Exp $	*/

/*
 * Copyright 1998
 * Digital Equipment Corporation. All rights reserved.
 *
 * This software is furnished under license and may be used and
 * copied only in accordance with the following terms and conditions.
 * Subject to these conditions, you may download, copy, install,
 * use, modify and distribute this software in source and/or binary
 * form. No title or ownership is transferred hereby.
 *
 * 1) Any source code used, modified or distributed must reproduce
 *    and retain this copyright notice and list of conditions as
 *    they appear in the source file.
 *
 * 2) No right is granted to use any trade name, trademark, or logo of
 *    Digital Equipment Corporation. Neither the "Digital Equipment
 *    Corporation" name nor any trademark or logo of Digital Equipment
 *    Corporation may be used to endorse or promote products derived
 *    from this software without the prior written permission of
 *    Digital Equipment Corporation.
 *
 * 3) This software is provided "AS-IS" and any express or implied
 *    warranties, including but not limited to, any implied warranties
 *    of merchantability, fitness for a particular purpose, or
 *    non-infringement are disclaimed. In no event shall DIGITAL be
 *    liable for any damages whatsoever, and in particular, DIGITAL
 *    shall not be liable for special, indirect, consequential, or
 *    incidental damages or damages for lost profits, loss of
 *    revenue or loss of use, whether such damages arise in contract,
 *    negligence, tort, under statute, in equity, at law or otherwise,
 *    even if advised of the possibility of such damage.
 */

/*
 * ATAPI disk attachment for the 'sd' driver.
 *
 * Chris Demetriou, January 10, 1998.
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
#include <scsi/atapi_disk.h>
#include <scsi/sdvar.h>

static int	sd_atapibus_get_parms(struct sd_softc *,
		    struct disk_parms *, int);

const struct sd_ops sd_atapibus_ops = {
	sd_atapibus_get_parms,
};

static int
sd_atapibus_get_parms(sd, dp, flags)
	struct sd_softc *sd;
	struct disk_parms *dp;
	int flags;
{
	struct atapi_read_format_capacities scsi_cmd;
	struct scsi_direct_blk_desc *descp;
	struct scsi_mode_sense_buf sense_data; 
	union scsi_disk_pages *sense_pages = NULL;
	char capacity_data[ATAPI_CAP_DESC_SIZE(1)];
	u_int16_t rpm;
	int error;

	bzero(&scsi_cmd, sizeof scsi_cmd);
	scsi_cmd.opcode = ATAPI_READ_FORMAT_CAPACITIES;
	_lto2b(ATAPI_CAP_DESC_SIZE(1), scsi_cmd.length);

	error = scsi_scsi_cmd(sd->sc_link,
	    (struct scsi_generic *)&scsi_cmd, sizeof(scsi_cmd),
	    (void *)capacity_data, ATAPI_CAP_DESC_SIZE(1), SDRETRIES, 20000,
	    NULL, flags | SCSI_DATA_IN);
	SC_DEBUG(sd->sc_link, SDEV_DB2,
	    ("sd_atapibus_get_parms: read format capacities error=%d\n",
	    error));
	if (error != 0)
		return (SDGP_RESULT_OFFLINE);

	descp = (struct scsi_direct_blk_desc *)
	    &capacity_data[ATAPI_CAP_DESC_OFFSET_DESC(0)];

	switch (descp->density & ATAPI_CAP_DESC_CODE_MASK) {
	case ATAPI_CAP_DESC_CODE_UNFORMATTED:
		return SDGP_RESULT_UNFORMATTED;

	case ATAPI_CAP_DESC_CODE_FORMATTED:
		break;

	case ATAPI_CAP_DESC_CODE_NONE:
		return SDGP_RESULT_OFFLINE;

	default:
#ifdef DIAGNOSTIC
		printf("%s: strange capacity descriptor density 0x%x\n",
		    sd->sc_dev.dv_xname, (u_int)descp->density);
#endif
		break;
	}

	dp->disksize = _4btol(descp->nblocks);
	if (dp->disksize == 0)
		return (SDGP_RESULT_OFFLINE);

	dp->blksize = _3btol(descp->blklen);

	error = scsi_do_mode_sense(sd->sc_link, ATAPI_FLEX_GEOMETRY_PAGE,
	    &sense_data, (void **)&sense_pages, NULL, NULL, NULL,
	    sizeof(sense_pages->flex_geometry), flags | SCSI_SILENT, NULL);
	if (error == 0 && sense_pages) {
		dp->heads = sense_pages->flex_geometry.nheads;
		dp->sectors = sense_pages->flex_geometry.ph_sec_tr;
		dp->cyls = _2btol(sense_pages->flex_geometry.ncyl);
		if (dp->blksize == 0)
			dp->blksize =
			     _2btol(sense_pages->flex_geometry.bytes_s);
		rpm = _2btol(sense_pages->flex_geometry.rpm);
		if (rpm)
			dp->rot_rate = rpm;
	}
	
	/*
	 * Use standard fake values if MODE SENSE did not provide better ones.
	 */
	if (dp->rot_rate == 0)
		dp->rot_rate = 3600;
	if (dp->blksize == 0)
		dp->blksize = 512;

	if (dp->heads == 0)
		dp->heads = 64;
	if (dp->sectors == 0)
		dp->sectors = 32;
	if (dp->cyls == 0)
		dp->cyls = dp->disksize / (dp->heads * dp->sectors);

	return (SDGP_RESULT_OK);
}
