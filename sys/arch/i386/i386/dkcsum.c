/*	$OpenBSD: dkcsum.c,v 1.15 2005/01/01 03:07:08 millert Exp $	*/

/*-
 * Copyright (c) 1997 Niklas Hallqvist.  All rights reserved.
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
 * A checksumming pseudo device used to get unique labels of each disk
 * that needs to be matched to BIOS disks.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/conf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/proc.h>
#include <sys/reboot.h>
#include <sys/stat.h>
#include <sys/systm.h>

#include <machine/biosvar.h>

#include <lib/libz/zlib.h>

#define	b_cylin	b_resid

dev_t dev_rawpart(struct device *);	/* XXX */

extern u_int32_t bios_cksumlen;
extern bios_diskinfo_t *bios_diskinfo;
extern dev_t bootdev;

void
dkcsumattach()
{
	struct device *dv;
	struct buf *bp;
	struct bdevsw *bdsw;
	dev_t dev, pribootdev, altbootdev;
	int error;
	u_int32_t csum;
	bios_diskinfo_t *bdi, *hit;

	/* do nothing if no diskinfo passed from /boot, or a bad length */
	if (bios_diskinfo == NULL || bios_cksumlen * DEV_BSIZE > MAXBSIZE)
		return;

#ifdef DEBUG
	printf("dkcsum: bootdev=0x%x\n", bootdev);
#endif
	pribootdev = altbootdev = 0;

	/*
	 * XXX What if DEV_BSIZE is changed to something else than the BIOS
	 * blocksize?  Today, /boot doesn't cover that case so neither need
	 * I care here.
	 */
	bp = geteblk(bios_cksumlen * DEV_BSIZE);	/* XXX error check?  */

	TAILQ_FOREACH(dv, &alldevs, dv_list) {
		if (dv->dv_class != DV_DISK)
			continue;
		bp->b_dev = dev = dev_rawpart(dv);
		if (dev == NODEV)
			continue;
		bdsw = &bdevsw[major(dev)];

		/*
		 * This open operation guarantees a proper initialization
		 * of the device, for future strategy calls.
		 */
		error = (*bdsw->d_open)(dev, FREAD, S_IFCHR, curproc);
		if (error) {
			/* XXX What to do here? */
			if (error != EIO)
				printf("dkcsum: open of %s failed (%d)\n",
				    dv->dv_xname, error);
			continue;
		}

		/* Read blocks to cksum.  XXX maybe a d_read should be used. */
		bp->b_blkno = 0;
		bp->b_bcount = bios_cksumlen * DEV_BSIZE;
		bp->b_flags = B_BUSY | B_READ;
		bp->b_cylin = 0;
		(*bdsw->d_strategy)(bp);
		if (biowait(bp)) {
			/* XXX What to do here? */
			printf("dkcsum: read of %s failed (%d)\n",
			    dv->dv_xname, error);
			error = (*bdsw->d_close)(dev, 0, S_IFCHR, curproc);
			if (error)
				printf("dkcsum: close of %s failed (%d)\n",
				    dv->dv_xname, error);
			continue;
		}
		error = (*bdsw->d_close)(dev, FREAD, S_IFCHR, curproc);
		if (error) {
			/* XXX What to do here? */
			printf("dkcsum: close of %s failed (%d)\n",
			    dv->dv_xname, error);
			continue;
		}

		csum = adler32(0, bp->b_data, bios_cksumlen * DEV_BSIZE);
#ifdef DEBUG
		printf("dkcsum: checksum of %s is %x\n", dv->dv_xname, csum);
#endif

		/* Find the BIOS device */
		hit = 0;
		for (bdi = bios_diskinfo; bdi->bios_number != -1; bdi++) {
			/* Skip non-harddrives */
			if (!(bdi->bios_number & 0x80))
				continue;
#ifdef DEBUG
			printf("dkcsum: "
			    "attempting to match with BIOS drive %x csum %x\n",
			    bdi->bios_number, bdi->checksum);
#endif
			if (bdi->checksum == csum) {
				if (!hit && !(bdi->flags & BDI_PICKED))
					hit = bdi;
				else {
					/* XXX add other heuristics here.  */
					printf("dkcsum: warning: "
					    "dup BSD->BIOS disk mapping\n");
				}
			}
		}

		/*
		 * If we have no hit, that's OK, we can see a lot more devices
		 * than the BIOS can, so this case is pretty normal.
		 */
		if (hit) {
#ifdef DIAGNOSTIC
			printf("dkcsum: %s matched BIOS disk %x\n",
			    dv->dv_xname, hit->bios_number);
#endif
		} else {
#ifdef DIAGNOSTIC
			printf("dkcsum: %s had no matching BIOS disk\n",
			    dv->dv_xname);
#endif
			continue;
		}

		/*
		 * Fixup bootdev if units match.  This means that all of
		 * hd*, sd*, wd*, will be interpreted the same.  Not 100%
		 * backwards compatible, but sd* and wd* should be phased-
		 * out in the bootblocks.
		 */

		/* B_TYPE dependent hd unit counting bootblocks */ 
		if ((B_TYPE(bootdev) == B_TYPE(hit->bsd_dev)) &&
		    (B_UNIT(bootdev) == B_UNIT(hit->bsd_dev))) {
			int type, ctrl, adap, part, unit;

			type = major(bp->b_dev);
			adap = B_ADAPTOR(bootdev);
			ctrl = B_CONTROLLER(bootdev);
			unit = DISKUNIT(bp->b_dev);
			part = B_PARTITION(bootdev);

			pribootdev = MAKEBOOTDEV(type, ctrl, adap, unit, part);
#ifdef DEBUG
			printf("dkcsum: setting %s as primary boot disk\n",
			    dv->dv_xname);
#endif
		}
		/* B_TYPE independent hd unit counting bootblocks */
		if (B_UNIT(bootdev) == (hit->bios_number & 0x7F)) {
			int type, ctrl, adap, part, unit;

			type = major(bp->b_dev);
			adap = B_ADAPTOR(bootdev);
			ctrl = B_CONTROLLER(bootdev);
			unit = DISKUNIT(bp->b_dev);
			part = B_PARTITION(bootdev);

			altbootdev = MAKEBOOTDEV(type, ctrl, adap, unit, part);
#ifdef DEBUG
			printf("dkcsum: setting %s as alternate boot disk\n",
			    dv->dv_xname);
#endif
		}

		/* This will overwrite /boot's guess, just so you remember */
		hit->bsd_dev = MAKEBOOTDEV(major(bp->b_dev), 0, 0,
		    DISKUNIT(bp->b_dev), RAW_PART);
		hit->flags |= BDI_PICKED;
	}
	bootdev = pribootdev ? pribootdev : altbootdev ? altbootdev : bootdev;

	bp->b_flags |= B_INVAL;
	brelse(bp);
}
