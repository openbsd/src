/*	$OpenBSD: ofdisk.c,v 1.9 2004/11/29 12:50:05 jsg Exp $	*/
/*	$NetBSD: ofdisk.c,v 1.3 1996/10/13 01:38:13 christos Exp $	*/

/*
 * Copyright (C) 1995, 1996 Wolfgang Solfrank.
 * Copyright (C) 1995, 1996 TooLs GmbH.
 * All rights reserved.
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
 *	This product includes software developed by TooLs GmbH.
 * 4. The name of TooLs GmbH may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY TOOLS GMBH ``AS IS'' AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
 * IN NO EVENT SHALL TOOLS GMBH BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <sys/param.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/systm.h>

#include <dev/ofw/openfirm.h>

struct ofd_softc {
	struct device sc_dev;
	int sc_phandle;
	int sc_unit;
	struct disk sc_dk;
	int sc_ihandle;
	u_long max_transfer;
	char sc_name[16];
};

static int ofdprobe(struct device *, void *, void *);
static void ofdattach(struct device *, struct device *, void *);

struct cfattach ofdisk_ca = {
	sizeof(struct ofd_softc), ofdprobe, ofdattach
};

struct cfdriver ofdisk_cd = {
	NULL, "ofdisk", DV_DISK
};

void ofdstrategy(struct buf *);

struct dkdriver ofdkdriver = { ofdstrategy };

static int
ofdprobe(struct device *parent, void *match, void *aux)
{
	struct ofprobe *ofp = aux;
	char type[8];
	int l;
	
	if ((l = OF_getprop(ofp->phandle, "device_type", type, sizeof type - 1)) < 0)
		return 0;
	if (l >= sizeof type)
		return 0;
	type[l] = 0;
	return !strcmp(type, "block");
}

static void
ofdattach(struct device *parent, struct device *self, void *aux)
{
	struct ofd_softc *of = (void *)self;
	struct ofprobe *ofp = aux;
	int l;
	
	of->sc_phandle = ofp->phandle;
	of->sc_unit = ofp->unit;
	of->sc_ihandle = 0;
	of->sc_dk.dk_driver = &ofdkdriver;
	of->sc_dk.dk_name = of->sc_name;
	strlcpy(of->sc_name, of->sc_dev.dv_xname, sizeof of->sc_name);
	disk_attach(&of->sc_dk);
	dk_establish(&of->sc_dk, self);				/* XXX */
	printf("\n");
}

int
ofdopen(dev_t dev, int flags, int fmt, struct proc *p)
{
	int unit = DISKUNIT(dev);
	struct ofd_softc *of;
	char path[256];
	struct disklabel *lp;
	int l;
	
	if (unit >= ofdisk_cd.cd_ndevs)
		return ENXIO;
	if (!(of = ofdisk_cd.cd_devs[unit]))
		return ENXIO;

	if (!of->sc_ihandle) {

		if ((l = OF_package_to_path(of->sc_phandle, path, sizeof path - 3)) < 0)
			return ENXIO;
		if (l >= sizeof path - 3)
			return ENXIO;
		path[l] = 0;

		/*
		 * This is for the benefit of SCSI/IDE disks that don't
		 * have all their childs in the device tree.
		 * YES, I DO THINK THIS IS A BUG IN OPENFIRMWARE!!!
		 * And yes, this is a very gross hack!					XXX
		 * See also ofscsi.c
		 */
		if (!strcmp(path + l - 4, "disk")) {
			path[l++] = '@';
			path[l++] = '0' + of->sc_unit;
			path[l] = 0;
		}

		strlcat(path, ":0", sizeof path);

		if (!(of->sc_ihandle = OF_open(path)))
			return ENXIO;

		/*
		 * Try to get characteristics of the disk.
		 */
		of->max_transfer = OF_call_method_1("max-transfer", of->sc_ihandle, 0);
		if (of->max_transfer > MAXPHYS)
			of->max_transfer = MAXPHYS;
		
		lp = of->sc_dk.dk_label;
		bzero(lp, sizeof *lp);
		
		lp->d_secsize = OF_call_method_1("block-size", of->sc_ihandle, 0);
		if (lp->d_secsize == (u_int32_t)-1 || lp->d_secsize > MAXBSIZE)
			lp->d_secsize = DEV_BSIZE;
		
		lp->d_secperunit = OF_call_method_1("#blocks", of->sc_ihandle, 0);
		if (lp->d_secperunit == (u_int32_t)-1)
			lp->d_secperunit = 0x7fffffff;
		
		lp->d_secpercyl = 1;
		lp->d_nsectors = 1;
		lp->d_ntracks = 1;
		lp->d_ncylinders = lp->d_secperunit;
			
		lp->d_partitions[RAW_PART].p_offset = 0;
		lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
		
		readdisklabel(MAKEDISKDEV(major(dev), unit, RAW_PART),
		    ofdstrategy, lp, of->sc_dk.dk_cpulabel, 0);
	}

	switch (fmt) {
	case S_IFCHR:
		of->sc_dk.dk_copenmask |= 1 << DISKPART(dev);
		break;
	case S_IFBLK:
		of->sc_dk.dk_bopenmask |= 1 << DISKPART(dev);
		break;
	}
	of->sc_dk.dk_openmask = of->sc_dk.dk_copenmask | of->sc_dk.dk_bopenmask;
	
	return 0;
}

int
ofdclose(dev_t dev, int flags, int fmt, struct proc *p)
{
	struct ofd_softc *of = ofdisk_cd.cd_devs[DISKUNIT(dev)];

	switch (fmt) {
	case S_IFCHR:
		of->sc_dk.dk_copenmask &= ~(1 << DISKPART(dev));
		break;
	case S_IFBLK:
		of->sc_dk.dk_bopenmask &= ~(1 << DISKPART(dev));
		break;
	}
	of->sc_dk.dk_openmask = of->sc_dk.dk_copenmask | of->sc_dk.dk_bopenmask;
	
#ifdef	FIREPOWERBUGS
	/*
	 * This is a hack to get the firmware to flush its buffers.
	 */
	OF_seek(of->sc_ihandle, 0);
#endif
	if (!of->sc_dk.dk_openmask) {
		OF_close(of->sc_ihandle);
		of->sc_ihandle = 0;
	}

	return 0;
}

void
ofdstrategy(struct buf *bp)
{
	struct ofd_softc *of = ofdisk_cd.cd_devs[DISKUNIT(bp->b_dev)];
	struct partition *p;
	u_quad_t off;
	int read;
	int (*OF_io)(int, void *, int);
	daddr_t blkno = bp->b_blkno;
	int s;
	
	bp->b_resid = 0;
	if (bp->b_bcount == 0)
		goto done;
	
	OF_io = bp->b_flags & B_READ ? OF_read : OF_write;

	if (DISKPART(bp->b_dev) != RAW_PART) {
		if (bounds_check_with_label(bp, of->sc_dk.dk_label,
		    of->sc_dk.dk_cpulabel, 0) <= 0) {
			bp->b_resid = bp->b_bcount;
			goto done;
		}
		p = &of->sc_dk.dk_label->d_partitions[DISKPART(bp->b_dev)];
		blkno = bp->b_blkno + p->p_offset;
	}
	
	disk_busy(&of->sc_dk);

	off = (u_quad_t)blkno * DEV_BSIZE;
	read = -1;
	do {
		if (OF_seek(of->sc_ihandle, off) < 0)
			break;
		read = OF_io(of->sc_ihandle, bp->b_data, bp->b_bcount);
	} while (read == -2);
	if (read < 0) {
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
	} else
		bp->b_resid = bp->b_bcount - read;

	disk_unbusy(&of->sc_dk, bp->b_bcount - bp->b_resid,
	    (bp->b_flags & B_READ));

done:
	s = splbio();
	biodone(bp);'
	splx(s);
}

static void
ofminphys(struct buf *bp)
{
	struct ofd_softc *of = ofdisk_cd.cd_devs[DISKUNIT(bp->b_dev)];
	
	if (bp->b_bcount > of->max_transfer)
		bp->b_bcount = of->max_transfer;
}

int
ofdread(dev_t dev, struct uio *uio)
{
	return physio(ofdstrategy, NULL, dev, B_READ, ofminphys, uio);
}

int
ofdwrite(dev_t dev, struct uio *uio)
{
	return physio(ofdstrategy, NULL, dev, B_WRITE, ofminphys, uio);
}

int
ofdioctl(dev_t dev, u_long cmd, caddr_t data, int flag, struct proc *p)
{
	struct ofd_softc *of = ofdisk_cd.cd_devs[DISKUNIT(dev)];
	int error;
	
	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)data = *of->sc_dk.dk_label;
		return 0;
		
	case DIOCGPART:
		((struct partinfo *)data)->disklab = of->sc_dk.dk_label;
		((struct partinfo *)data)->part =
			&of->sc_dk.dk_label->d_partitions[DISKPART(dev)];
		return 0;
		
	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;
		
		error = setdisklabel(of->sc_dk.dk_label,
				     (struct disklabel *)data, /*of->sc_dk.dk_openmask */0,
				     of->sc_dk.dk_cpulabel);
		if (error == 0 && cmd == DIOCWDINFO)
			error = writedisklabel(MAKEDISKDEV(major(dev),
							   DISKUNIT(dev), RAW_PART),
					       ofdstrategy,
					       of->sc_dk.dk_label,
					       of->sc_dk.dk_cpulabel);

		return error;
	default:
		return ENOTTY;
	}
}

int
ofddump(dev_t dev, daddr_t blkno, caddr_t va, size_t size)
{
	return EINVAL;
}

int
ofdsize(dev_t dev)
{
	struct ofd_softc *of;
	int part;
	int size;
	
	if (ofdopen(dev, 0, S_IFBLK) != 0)
		return -1;
	of = ofdisk_cd.cd_devs[DISKUNIT(dev)];
	part = DISKPART(dev);
	if (of->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = of->sc_dk.dk_label->d_partitions[part].p_size;
	if (ofdclose(dev, 0, S_IFBLK) != 0)
		return -1;
	return size;
}
