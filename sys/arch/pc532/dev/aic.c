/*	$NetBSD: aic.c,v 1.4 1995/08/12 20:31:10 mycroft Exp $	*/

/* Written by Phil Nelson for the pc532.  Used source with the following
 * copyrights as a model.
 *
 *   aic.c: A Adaptec 6250 driver for the pc532.
 */
/*
 * (Mostly) Written by Julian Elischer (julian@tfs.com)
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
 */

/*
 * a FEW lines in this driver come from a MACH adaptec-disk driver
 * so the copyright below is included:
 *
 * Copyright 1990 by Open Software Foundation,
 * Grenoble, FRANCE
 *
 * 		All Rights Reserved
 *
 *   Permission to use, copy, modify, and distribute this software and
 * its documentation for any purpose and without fee is hereby granted,
 * provided that the above copyright notice appears in all copies and
 * that both the copyright notice and this permission notice appear in
 * supporting documentation, and that the name of OSF or Open Software
 * Foundation not be used in advertising or publicity pertaining to
 * distribution of the software without specific, written prior
 * permission.
 *
 *   OSF DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS,
 * IN NO EVENT SHALL OSF BE LIABLE FOR ANY SPECIAL, INDIRECT, OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN ACTION OF CONTRACT,
 * NEGLIGENCE, OR OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION
 * WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 */

#include "sys/types.h"
#include "sys/param.h"
#include "sys/systm.h"
#include "sys/errno.h"
#include "sys/ioctl.h"
#include "sys/buf.h"
#include "machine/stdarg.h"
#include "sys/proc.h"
#include "sys/user.h"
#include "sys/dkbad.h"
#include "sys/disklabel.h"
#include "scsi/scsi_all.h"
#include "scsi/scsiconf.h"

#include "device.h"

/* Some constants (may need to be changed!) */
#define AIC_NSEG		16

int aicprobe(struct pc532_device *);
int aicattach(struct pc532_device *);
int aic_scsi_cmd(struct scsi_xfer *);
void aicminphys(struct buf *);
long int aic_adapter_info(int);

struct scsidevs *
scsi_probe(int masunit, struct scsi_switch *sw, int physid, int type, int want);

struct	pc532_driver aicdriver = {
	aicprobe, aicattach, "aic",
};

struct scsi_switch dp_switch = {
	"aic",
	aic_scsi_cmd,
	aicminphys,
	0,
	0,
	aic_adapter_info,
	0, 0, 0
};

int aicprobe(struct pc532_device *dvp)
{
  return (0);	/* All pc532s should have one, but it is not working now. */
}


int aicattach(struct pc532_device *dvp)
{
	int r;

	r = scsi_attach(0, 7, &dp_switch,
		&dvp->pd_drive, &dvp->pd_unit, dvp->pd_flags);

	return(r);
}

void aicminphys(struct buf *bp)
{

	if(bp->b_bcount > ((AIC_NSEG - 1) * NBPG))
		bp->b_bcount = ((AIC_NSEG - 1) * NBPG);
	minphys(bp);
}

long int aic_adapter_info(int unit)
{
	return (1);    /* only 1 outstanding request. */
}


/* Do a scsi command. */

struct scsi_xfer *cur_xs;

int aic_scsi_cmd(struct scsi_xfer *xs)
{
printf ("aic_scsi_cmd: ... \n");
	cur_xs = xs;

	return (HAD_ERROR);
}

void aic_intr (struct intrframe frame)
{
}
