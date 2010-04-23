/*	$OpenBSD: wd.c,v 1.8 2010/04/23 15:25:20 jsing Exp $	*/
/*	$NetBSD: wd.c,v 1.5 2005/12/11 12:17:06 christos Exp $	*/

/*-
 * Copyright (c) 2003 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Manuel Bouyer.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
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
#include <sys/stdint.h>

#include <machine/param.h>

#include "libsa.h"
#include "wdvar.h"

void	wdprobe(void);
int	wd_get_params(struct wd_softc *wd);
int	wdgetdisklabel(struct wd_softc *wd);
void	wdgetdefaultlabel(struct wd_softc *wd, struct disklabel *lp);

struct wd_softc wd_devs[NUNITS];
int wd_ndevs = -1;

void
wdprobe(void)
{
	struct wd_softc *wd = wd_devs;
	u_int chan, drive, unit = 0;

	for (chan = 0; chan < PCIIDE_NUM_CHANNELS; chan++) {
		if (wdc_init(wd, chan) != 0)
			continue;
		for (drive = 0; drive < wd->sc_channel.ndrives; drive++) {
			wd->sc_unit = unit;
			wd->sc_drive = drive;

			if (wd_get_params(wd) != 0)
				continue;

			DPRINTF(("wd%d: channel %d drive %d\n",
				unit, chan, drive));
			unit++;
			wd++;
		}
	}

	wd_ndevs = unit;
}

/*
 * Get drive parameters through 'device identify' command.
 */
int
wd_get_params(wd)
	struct wd_softc *wd;
{
	int error;
	unsigned char buf[DEV_BSIZE];

	if ((error = wdc_exec_identify(wd, buf)) != 0)
		return (error);

	wd->sc_params = *(struct ataparams *)buf;

	/* 48-bit LBA addressing */
	if ((wd->sc_params.atap_cmd2_en & ATAPI_CMD2_48AD) != 0) {
		DPRINTF(("Drive supports LBA48.\n"));
#if defined(_ENABLE_LBA48)
		wd->sc_flags |= WDF_LBA48;
#endif
	}

	/* Prior to ATA-4, LBA was optional. */
	if ((wd->sc_params.atap_capabilities1 & WDC_CAP_LBA) != 0) {
		DPRINTF(("Drive supports LBA.\n"));
		wd->sc_flags |= WDF_LBA;
	}

	return (0);
}

/*
 * Initialize disk label to the default value.
 */
void
wdgetdefaultlabel(wd, lp)
	struct wd_softc *wd;
	struct disklabel *lp;
{
	memset(lp, 0, sizeof(struct disklabel));

	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = wd->sc_params.atap_heads;
	lp->d_nsectors = wd->sc_params.atap_sectors;
	lp->d_ncylinders = wd->sc_params.atap_cylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	if (strcmp(wd->sc_params.atap_model, "ST506") == 0)
		lp->d_type = DTYPE_ST506;
	else
		lp->d_type = DTYPE_ESDI;

	strncpy(lp->d_typename, wd->sc_params.atap_model, 16);
	strncpy(lp->d_packname, "fictitious", 16);
	if (wd->sc_capacity > UINT32_MAX)
		lp->d_secperunit = UINT32_MAX;
	else
		lp->d_secperunit = wd->sc_capacity;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = MAXPARTITIONS;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * Read disk label from the device.
 */
int
wdgetdisklabel(wd)
	struct wd_softc *wd;
{
	char *msg;
	int sector;
	size_t rsize;
	struct disklabel *lp;
	unsigned char buf[DEV_BSIZE];

	wdgetdefaultlabel(wd, &wd->sc_label);

	/*
	 * Find OpenBSD Partition in DOS partition table.
	 */
	sector = 0;
	if (wdstrategy(wd, F_READ, DOSBBSECTOR, DEV_BSIZE, buf, &rsize))
		return EOFFSET;

	if (*(u_int16_t *)&buf[DOSMBR_SIGNATURE_OFF] == DOSMBR_SIGNATURE) {
		int i;
		struct dos_partition *dp = (struct dos_partition *)buf;

		/*
		 * Lookup OpenBSD slice. If there is none, go ahead
		 * and try to read the disklabel off sector #0.
		 */
		
		memcpy(dp, &buf[DOSPARTOFF], NDOSPART * sizeof(*dp));
		for (i = 0; i < NDOSPART; i++) {
			if (dp[i].dp_typ == DOSPTYP_OPENBSD) {
				sector = letoh32(dp[i].dp_start);
				break;
			}
		}
	}

	if (wdstrategy(wd, F_READ, sector + LABELSECTOR, DEV_BSIZE,
				buf, &rsize))
		return EOFFSET;

	if ((msg = getdisklabel(buf + LABELOFFSET, &wd->sc_label)))
		printf("wd%d: getdisklabel: %s\n", wd->sc_unit, msg);

	lp = &wd->sc_label;

	/* check partition */
	if ((wd->sc_part >= lp->d_npartitions) ||
	    (lp->d_partitions[wd->sc_part].p_fstype == FS_UNUSED)) {
		DPRINTF(("illegal partition\n"));
		return (EPART);
	}

	DPRINTF(("label info: d_secsize %d, d_nsectors %d, d_ncylinders %d,"
				"d_ntracks %d, d_secpercyl %d\n",
				wd->sc_label.d_secsize,
				wd->sc_label.d_nsectors,
				wd->sc_label.d_ncylinders,
				wd->sc_label.d_ntracks,
				wd->sc_label.d_secpercyl));

	return (0);
}

/*
 * Open device (read drive parameters and disklabel)
 */
int
wdopen(struct open_file *f, ...)
{
	int error;
	va_list ap;
	u_int unit, part, drive;
	struct wd_softc *wd;

	va_start(ap, f);
	unit = va_arg(ap, u_int);
	part = va_arg(ap, u_int);
	va_end(ap);

	DPRINTF(("wdopen: wd%d%c\n", unit, 'a' + part));

	if (unit < 0 || unit >= NUNITS)
		return (ENXIO);

	if (wd_ndevs == -1)
		wdprobe();

	if (unit >= wd_ndevs)
		return (ENXIO);

	wd = &wd_devs[unit];
	wd->sc_part = part;

	if ((error = wdgetdisklabel(wd)) != 0)
		return (error);

	f->f_devdata = wd;
	return (0);
}

/*
 * Close device.
 */
int
wdclose(struct open_file *f)
{
	return 0;
}

/*
 * Read some data.
 */
int
wdstrategy(f, rw, dblk, size, buf, rsize)
	void *f;
	int rw;
	daddr_t dblk;
	size_t size;
	void *buf;
	size_t *rsize;
{
	int i, nsect;
	daddr_t blkno;
	struct wd_softc *wd = f;

	if (size == 0)
		return (0);
    
	if (rw != F_READ)
		return EOPNOTSUPP;

	nsect = howmany(size, wd->sc_label.d_secsize);
	blkno = dblk + wd->sc_label.d_partitions[wd->sc_part].p_offset;

	for (i = 0; i < nsect; i++, blkno++) {
		int error;

		if ((error = wdc_exec_read(wd, WDCC_READ, blkno, buf)) != 0)
			return (error);

		buf += wd->sc_label.d_secsize;
	}

	*rsize = size;
	return (0);
}
