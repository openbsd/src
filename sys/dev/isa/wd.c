/*	$OpenBSD: wd.c,v 1.36 1998/10/01 04:23:54 millert Exp $	*/
/*	$NetBSD: wd.c,v 1.150 1996/05/12 23:54:03 mycroft Exp $ */

/*
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
 *
 * DMA and multi-sector PIO handling are derived from code contributed by
 * Onno van der Linden.
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

#include "isadma.h"

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/proc.h>

#include <vm/vm.h>

#include <machine/bus.h>
#include <machine/cpu.h>
#include <machine/intr.h>

#include <dev/isa/isavar.h>
#include <dev/isa/wdreg.h>
#include <dev/isa/wdlink.h>

#define	WDUNIT(dev)			DISKUNIT(dev)
#define	WDPART(dev)			DISKPART(dev)
#define	MAKEWDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define	WDLABELDEV(dev)	(MAKEWDDEV(major(dev), WDUNIT(dev), RAW_PART))

#ifdef WDDEBUG
#define WDDEBUG_PRINT(args)		printf args
#else
#define WDDEBUG_PRINT(args)
#endif

struct wd_softc {
	struct device sc_dev;
	struct disk sc_dk;
	struct wd_link *d_link;
	struct buf sc_q;
};

int	wdprobe		__P((struct device *, void *, void *));
void	wdattach	__P((struct device *, struct device *, void *));
int	wdprint		__P((void *, const char *));

struct cfattach wd_ca = {
	sizeof(struct wd_softc), wdprobe, wdattach
};

struct cfdriver wd_cd = {
	NULL, "wd", DV_DISK
};

void	wdgetdisklabel	__P((dev_t, struct wd_softc *));
int	wd_get_parms	__P((struct wd_softc *));
void	wdstrategy	__P((struct buf *));

struct dkdriver wddkdriver = { wdstrategy };

/* XXX: these should go elsewhere */
cdev_decl(wd);
bdev_decl(wd);

void	wdfinish	__P((struct wd_softc *, struct buf *));
int	wdsetctlr	__P((struct wd_link *));
#ifdef DKBAD
static void bad144intern __P((struct wd_softc *));
#endif
int	wdlock		__P((struct wd_link *));
void	wdunlock	__P((struct wd_link *));

int
wdprobe(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
	struct wd_link *d_link = aux;
	int drive;
	
	if (d_link == NULL)
		return 0;
	if (d_link->type != DRIVE)
		return 0;

	drive = d_link->sc_drive;
	if (cf->cf_loc[0] != -1 && cf->cf_loc[0] != drive)
		return 0;

	return 1;
}

void
wdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct wd_softc *wd = (void *)self;
	struct wd_link *d_link= aux;
	int i, blank;
	char buf[41], c, *p, *q;

	wd->d_link = d_link;
	d_link->openings = 1;
	d_link->wd_softc = (caddr_t)wd;

	/*
	 * Initialize and attach the disk structure.
	 */
	wd->sc_dk.dk_driver = &wddkdriver;
	wd->sc_dk.dk_name = wd->sc_dev.dv_xname;
	disk_attach(&wd->sc_dk);

	dk_establish(&wd->sc_dk, &wd->sc_dev);

	d_link->sc_lp = wd->sc_dk.dk_label;

	wdc_get_parms(d_link);
	for (blank = 0, p = d_link->sc_params.wdp_model, q = buf, i = 0;
	     i < sizeof(d_link->sc_params.wdp_model); i++) {
		c = *p++;
		if (c == '\0')
			break;
		if (c != ' ') {
			if (blank) {
				*q++ = ' ';
				blank = 0;
			}
			*q++ = c;
		} else
			blank = 1;
	}
	*q++ = '\0';

	printf(": <%s>\n", buf);
	if (d_link->sc_lp->d_type != DTYPE_ST506) {
		if ((d_link->sc_params.wdp_capabilities & WD_CAP_LBA) != 0) {
			printf("%s: %dMB, %d cyl, %d head, %d sec, %d bytes/sec, %d sec total\n",
				self->dv_xname,
				d_link->sc_params.wdp_lbacapacity / 2048,
				d_link->sc_params.wdp_cylinders,
				d_link->sc_params.wdp_heads,
				d_link->sc_params.wdp_sectors,
				DEV_BSIZE, /* XXX */
				d_link->sc_params.wdp_lbacapacity);
		} else {
			printf("%s: %dMB, %d cyl, %d head, %d sec, %d bytes/sec, %d sec total\n",
				self->dv_xname,
				d_link->sc_params.wdp_cylinders *
					(d_link->sc_params.wdp_heads *
					 d_link->sc_params.wdp_sectors) / (1048576 / DEV_BSIZE),
				d_link->sc_params.wdp_cylinders,
				d_link->sc_params.wdp_heads,
				d_link->sc_params.wdp_sectors,
				DEV_BSIZE, /* XXX */
				d_link->sc_params.wdp_cylinders *
					(d_link->sc_params.wdp_heads *
					 d_link->sc_params.wdp_sectors));
		}
	}

#if NISADMA > 0
	if ((d_link->sc_params.wdp_capabilities & WD_CAP_DMA) != 0 &&
	    d_link->sc_mode == WDM_DMA) {
		d_link->sc_mode = WDM_DMA;
	} else
#endif
		if (d_link->sc_params.wdp_maxmulti > 1) {
			d_link->sc_mode = WDM_PIOMULTI;
			d_link->sc_multiple = min(d_link->sc_params.wdp_maxmulti, 16);
		} else {
			d_link->sc_mode = WDM_PIOSINGLE;
			d_link->sc_multiple = 1;
		}

	printf("%s: using", wd->sc_dev.dv_xname);
#if NISADMA > 0
	if (d_link->sc_mode == WDM_DMA)
		printf(" dma transfers,");
	else
#endif
		printf(" %d-sector %d-bit pio transfers,",
		       d_link->sc_multiple,
		       (d_link->sc_flags & WDF_32BIT) == 0 ? 16 : 32);
	if ((d_link->sc_params.wdp_capabilities & WD_CAP_LBA) != 0)
		printf(" lba addressing");
	else
		printf(" chs addressing");
	if (d_link->sc_params.wdp_bufsize > 0)
		printf(" (%dKB cache)", d_link->sc_params.wdp_bufsize / 2);
	printf("\n");
}

/*
 * Read/write routine for a buffer.  Validates the arguments and schedules the
 * transfer.  Does not wait for the transfer to complete.
 */
void
wdstrategy(bp)
	struct buf *bp;
{
	struct wd_softc *wd = wd_cd.cd_devs[WDUNIT(bp->b_dev)];
	struct wd_link *d_link= wd->d_link;
	int s;
    
	/* Valid request?  */
	if (bp->b_blkno < 0 ||
	    (bp->b_bcount % wd->sc_dk.dk_label->d_secsize) != 0 ||
	    (bp->b_bcount / wd->sc_dk.dk_label->d_secsize) >= (1 << NBBY)) {
		bp->b_error = EINVAL;
		goto bad;
	}
    
	/* If device invalidated (e.g. media change, door open), error. */
	if ((d_link->sc_flags & WDF_LOADED) == 0) {
		bp->b_error = EIO;
		goto bad;
	}

	/* If it's a null transfer, return immediately. */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (WDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, wd->sc_dk.dk_label,
	    wd->sc_dk.dk_cpulabel,
	    (d_link->sc_flags & (WDF_WLABEL|WDF_LABELLING)) != 0) <= 0)
		goto done;
    
	/* Queue transfer on drive, activate drive and controller if idle. */
	s = splbio();
	disksort(&wd->sc_q, bp);
	wdstart(wd);
	splx(s);
	return;
    
bad:
	bp->b_flags |= B_ERROR;
done:
	/* Toss transfer; we're done early. */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
}

/*
 * Queue a drive for I/O.
 */
void
wdstart(vp)
	void *vp;
{
	struct wd_softc *wd = vp;
	struct buf *dp, *bp=0;
	struct wd_link *d_link = wd->d_link;
	struct wdc_link *ctlr_link = d_link->ctlr_link;
	struct wdc_xfer *xfer;
	u_int32_t p_offset; 

	while (d_link->openings > 0) {

		/* Is there a buf for us ? */
		dp = &wd->sc_q;
		if ((bp = dp->b_actf) == NULL)  /* yes, an assign */
                 	return;
		dp->b_actf = bp->b_actf;
		
		/* 
		 * Make the command. First lock the device
		 */
		d_link->openings--;
		if (WDPART(bp->b_dev) != RAW_PART)
			p_offset =
		  wd->sc_dk.dk_label->d_partitions[WDPART(bp->b_dev)].p_offset;
		else
			p_offset = 0;

		xfer = wdc_get_xfer(ctlr_link, 0);
		if (xfer == NULL)
			panic("wdc_xfer");

		xfer->d_link = d_link;
		xfer->c_bp = bp;
		xfer->c_p_offset = p_offset;
		xfer->databuf = bp->b_data;
		xfer->c_flags |= bp->b_flags & (B_READ|B_WRITE);
		xfer->c_skip = 0;
		xfer->c_errors = 0;
		/* count and blkno are filled in by wdcstart */

		/* Instrumentation. */
		disk_busy(&wd->sc_dk);
		wdc_exec_xfer(wd->d_link,xfer);
	}
}

int
wdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	WDDEBUG_PRINT(("wdread\n"));
	return (physio(wdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
wdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{

	WDDEBUG_PRINT(("wdwrite\n"));
	return (physio(wdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
wdlock(d_link)
	struct wd_link *d_link;
{
	int error;
	int s;

	WDDEBUG_PRINT(("wdlock\n"));

	s = splbio();

	while ((d_link->sc_flags & WDF_LOCKED) != 0) {
		d_link->sc_flags |= WDF_WANTED;
		if ((error = tsleep(d_link, PRIBIO | PCATCH,
		    "wdlck", 0)) != 0) {
			splx(s);
			return error;
		}
	}
	d_link->sc_flags |= WDF_LOCKED;
	splx(s);
	return 0;
}

/*
 * Unlock and wake up any waiters.
 */
void
wdunlock(d_link)
	struct wd_link *d_link;
{

	WDDEBUG_PRINT(("wdunlock"));

	d_link->sc_flags &= ~WDF_LOCKED;
	if ((d_link->sc_flags & WDF_WANTED) != 0) {
		d_link->sc_flags &= ~WDF_WANTED;
		wakeup(d_link);
	}
}

int
wdopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct wd_softc *wd;
	struct wd_link *d_link;
	int unit, part;
	int error;

	WDDEBUG_PRINT(("wdopen\n"));

	unit = WDUNIT(dev);
	if (unit >= wd_cd.cd_ndevs)
		return ENXIO;
	wd = wd_cd.cd_devs[unit];
	if (wd == 0)
		return ENXIO;
    
	d_link = wd->d_link;
	if ((error = wdlock(d_link)) != 0)
		return error;

	if (wd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((d_link->sc_flags & WDF_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		if ((d_link->sc_flags & WDF_LOADED) == 0) {
			d_link->sc_flags |= WDF_LOADED;

			/* Load the physical device parameters. */
			if (wdc_get_parms(d_link) != 0) {
				error = ENXIO;
				goto bad2;
			}

			/* Load the partition info if not already loaded. */
			wdgetdisklabel(dev, wd);
		}
	}

	part = WDPART(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= wd->sc_dk.dk_label->d_npartitions ||
	     wd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}
    
	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	wd->sc_dk.dk_openmask = wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	wdunlock(d_link);
	return 0;

bad2:
	d_link->sc_flags &= ~WDF_LOADED;

bad:
	if (wd->sc_dk.dk_openmask == 0) {
	}

bad3:
	wdunlock(d_link);
	return error;
}

int
wdclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct wd_softc *wd = wd_cd.cd_devs[WDUNIT(dev)];
	int part = WDPART(dev);
	int error;
    
	if ((error = wdlock(wd->d_link)) != 0)
		return error;

	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	wd->sc_dk.dk_openmask = wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	if (wd->sc_dk.dk_openmask == 0) {
		/* XXXX Must wait for I/O to complete! */
	}

	wdunlock(wd->d_link);
	return 0;
}

/*
 * Fabricate a default disk label, and try to read the correct one.
 */
void
wdgetdisklabel(dev, wd)
	dev_t dev;
	struct wd_softc *wd;
{
	struct disklabel *lp = wd->sc_dk.dk_label;
	struct wd_link *d_link = wd->d_link;
	char *errstring;

	WDDEBUG_PRINT(("wdgetdisklabel\n"));

	bzero(lp, sizeof(struct disklabel));
	bzero(wd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	lp->d_secsize = DEV_BSIZE;
	lp->d_ntracks = d_link->sc_params.wdp_heads;
	lp->d_nsectors = d_link->sc_params.wdp_sectors;
	lp->d_ncylinders = d_link->sc_params.wdp_cylinders;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	if (d_link->sc_params.wdp_config == WD_CFG_FIXED) {
		strncpy(lp->d_typename, "ST506/MFM/RLL disk", 16);
		lp->d_type = DTYPE_ST506;
	} else {
		strncpy(lp->d_typename, "ESDI/IDE disk", 16);
		lp->d_type = DTYPE_ESDI;
	}
	strncpy(lp->d_packname, d_link->sc_params.wdp_model, 16);
	if ((d_link->sc_params.wdp_capabilities & WD_CAP_LBA))
		lp->d_secperunit = d_link->sc_params.wdp_lbacapacity;
	else 
		lp->d_secperunit = lp->d_secpercyl * lp->d_ncylinders;

	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	d_link->sc_badsect[0] = -1;

	if (d_link->sc_state > RECAL)
		d_link->sc_state = RECAL;
	errstring = readdisklabel(WDLABELDEV(dev), wdstrategy, lp,
	    wd->sc_dk.dk_cpulabel);
	if (errstring) {
		/*
		 * This probably happened because the drive's default
		 * geometry doesn't match the DOS geometry.  We
		 * assume the DOS geometry is now in the label and try
		 * again.  XXX This is a kluge.
		 */
		if (d_link->sc_state > GEOMETRY)
			d_link->sc_state = GEOMETRY;
		errstring = readdisklabel(WDLABELDEV(dev), wdstrategy, lp,
		    wd->sc_dk.dk_cpulabel);
	}
	if (errstring) {
		/*printf("%s: %s\n", wd->sc_dev.dv_xname, errstring);*/
		return;
	}

	if (d_link->sc_state > GEOMETRY)
		d_link->sc_state = GEOMETRY;
#ifdef DKBAD
	if ((lp->d_flags & D_BADSECT) != 0)
		bad144intern(wd);
#endif
}


/*
 * Tell the drive what geometry to use.
 */
int
wdsetctlr(d_link)
	struct wd_link *d_link;
{
	struct wd_softc *wd=(struct wd_softc *)d_link->wd_softc;

	WDDEBUG_PRINT(("wd(%d,%d) C%dH%dS%d\n", wd->sc_dev.dv_unit,
	    d_link->sc_drive, wd->sc_dk.dk_label->d_ncylinders,
	    wd->sc_dk.dk_label->d_ntracks, wd->sc_dk.dk_label->d_nsectors));

	if (wdccommand(d_link, WDCC_IDP, d_link->sc_drive,
	    wd->sc_dk.dk_label->d_ncylinders,
	    wd->sc_dk.dk_label->d_ntracks - 1, 0,
	    wd->sc_dk.dk_label->d_nsectors) != 0) {
		wderror(d_link, NULL, "wdsetctlr: geometry upload failed");
		return -1;
	}

	return 0;
}

int
wdioctl(dev, xfer, addr, flag, p)
	dev_t dev;
	u_long xfer;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct wd_softc *wd = wd_cd.cd_devs[WDUNIT(dev)];
	struct wd_link *d_link = wd->d_link;
	int error;

	WDDEBUG_PRINT(("wdioctl\n"));

	if ((d_link->sc_flags & WDF_LOADED) == 0)
		return EIO;

	switch (xfer) {
#ifdef DKBAD
	case DIOCSBAD:
		if ((flag & FWRITE) == 0)
			return EBADF;
		DKBAD(wd->sc_dk.dk_cpulabel) = *(struct dkbad *)addr;
		wd->sc_dk.dk_label->d_flags |= D_BADSECT;
		bad144intern(wd);
		return 0;
#endif

	case DIOCGDINFO:
		*(struct disklabel *)addr = *(wd->sc_dk.dk_label);
		return 0;
	
	case DIOCGPART:
		((struct partinfo *)addr)->disklab = wd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &wd->sc_dk.dk_label->d_partitions[WDPART(dev)];
		return 0;
	
	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = wdlock(wd->d_link)) != 0)
			return error;
		d_link->sc_flags |= WDF_LABELLING;

		error = setdisklabel(wd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*wd->sc_dk.dk_openmask : */0,
		    wd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (d_link->sc_state > GEOMETRY)
				d_link->sc_state = GEOMETRY;
			if (xfer == DIOCWDINFO)
				error = writedisklabel(WDLABELDEV(dev),
				    wdstrategy, wd->sc_dk.dk_label,
				    wd->sc_dk.dk_cpulabel);
		}

		d_link->sc_flags &= ~WDF_LABELLING;
		wdunlock(d_link);
		return error;
	
	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return EBADF;
		if (*(int *)addr)
			d_link->sc_flags |= WDF_WLABEL;
		else
			d_link->sc_flags &= ~WDF_WLABEL;
		return 0;
	
#ifdef notyet
	case DIOCWFORMAT:
		if ((flag & FWRITE) == 0)
			return EBADF;
	{
		register struct format_op *fop;
		struct iovec aiov;
		struct uio auio;
	    
		fop = (struct format_op *)addr;
		aiov.iov_base = fop->df_buf;
		aiov.iov_len = fop->df_count;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_resid = fop->df_count;
		auio.uio_segflg = 0;
		auio.uio_offset =
		    fop->df_startblk * wd->sc_dk.dk_label->d_secsize;
		auio.uio_procp = p;
		error = physio(wdformat, NULL, dev, B_WRITE, minphys,
		    &auio);
		fop->df_count -= auio.uio_resid;
		fop->df_reg[0] = wdc->sc_status;
		fop->df_reg[1] = wdc->sc_error;
		return error;
	}
#endif
	
	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("wdioctl: impossible");
#endif
}

#ifdef B_FORMAT
int
wdformat(struct buf *bp)
{

	bp->b_flags |= B_FORMAT;
	return wdstrategy(bp);
}
#endif

int
wdsize(dev)
	dev_t dev;
{
	struct wd_softc *wd;
	int part;
	int size;

	WDDEBUG_PRINT(("wdsize\n"));

	if (wdopen(dev, 0, S_IFBLK, NULL) != 0)
		return -1;
	wd = wd_cd.cd_devs[WDUNIT(dev)];
	part = WDPART(dev);
	if (wd->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = wd->sc_dk.dk_label->d_partitions[part].p_size;
	if (wdclose(dev, 0, S_IFBLK, NULL) != 0)
		return -1;
	return size;
}


#ifndef __BDEVSW_DUMP_OLD_TYPE
/* #define WD_DUMP_NOT_TRUSTED if you just want to watch */
static int wddoingadump;
static int wddumprecalibrated;

/*
 * Dump core after a system crash.
 *
 * XXX:  This needs work!  Currently, it's a major hack: the
 * use of wdc_softc is very bad and should go away.
 */
int
wddump(dev, blkno, va, size)
        dev_t dev;
        daddr_t blkno;
        caddr_t va;
        size_t size;
{
	struct wd_softc *wd;	/* disk unit to do the I/O */
	struct wdc_softc *wdc;	/* disk controller to do the I/O */
	struct disklabel *lp;	/* disk's disklabel */
	struct wd_link *d_link;
	int	unit, part;
	int	nblks;		/* total number of sectors left to write */

	/* Check if recursive dump; if so, punt. */
	if (wddoingadump)
		return EFAULT;
	wddoingadump = 1;

	unit = WDUNIT(dev);
	if (unit >= wd_cd.cd_ndevs)
		return ENXIO;
	wd = wd_cd.cd_devs[unit];
	if (wd == (struct wd_softc *)0)
		return ENXIO;
	d_link = wd->d_link;

	part = WDPART(dev);

	/* Make sure it was initialized. */
	if (d_link->sc_state < READY)
		return ENXIO;

	wdc = (void *)wd->sc_dev.dv_parent;

        /* Convert to disk sectors.  Request must be a multiple of size. */
	lp = wd->sc_dk.dk_label;
	if ((size % lp->d_secsize) != 0)
		return EFAULT;
	nblks = size / lp->d_secsize;
	blkno = blkno / (lp->d_secsize / DEV_BSIZE);

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || ((blkno + nblks) > lp->d_partitions[part].p_size))
		return EINVAL;  

	/* Offset block number to start of partition. */
	blkno += lp->d_partitions[part].p_offset;

	/* Recalibrate, if first dump transfer. */
	if (wddumprecalibrated == 0) {
		wddumprecalibrated = 1;
		if (wdccommandshort(wdc, d_link->sc_drive, WDCC_RECAL) != 0 ||
		    wait_for_ready(wdc) != 0 || wdsetctlr(d_link) != 0 ||
		    wait_for_ready(wdc) != 0) {
			wderror(d_link, NULL, "wddump: recal failed");
			return EIO;
		}
	}
   
	while (nblks > 0) {
		daddr_t xlt_blkno = blkno;
		int cylin, head, sector;

		if ((lp->d_flags & D_BADSECT) != 0) {
			int blkdiff;
			int i;

			for (i = 0; (blkdiff = d_link->sc_badsect[i]) != -1; i++) {
				blkdiff -= xlt_blkno;
				if (blkdiff < 0)
					continue;
				if (blkdiff == 0) {
					/* Replace current block of transfer. */
					xlt_blkno = lp->d_secperunit -
					    lp->d_nsectors - i - 1;
				}
				break;
			}
			/* Tranfer is okay now. */
		}

		if ((d_link->sc_params.wdp_capabilities & WD_CAP_LBA) != 0) {
			sector = (xlt_blkno >> 0) & 0xff;
			cylin = (xlt_blkno >> 8) & 0xffff;
			head = (xlt_blkno >> 24) & 0xf;
			head |= WDSD_LBA;
		} else {
			sector = xlt_blkno % lp->d_nsectors;
			sector++;	/* Sectors begin with 1, not 0. */
			xlt_blkno /= lp->d_nsectors;
			head = xlt_blkno % lp->d_ntracks;
			xlt_blkno /= lp->d_ntracks;
			cylin = xlt_blkno;
			head |= WDSD_CHS;
		}

#ifndef WD_DUMP_NOT_TRUSTED
		if (wdccommand(d_link, WDCC_WRITE, d_link->sc_drive, cylin,
		    head, sector, 1) != 0 ||
		    wait_for_drq(wdc) != 0) {
			wderror(d_link, NULL, "wddump: write failed");
			return EIO;
		}
	
		/* XXX XXX XXX */
		bus_space_write_multi_2(wdc->sc_iot, wdc->sc_ioh, wd_data,
		    (u_int16_t *)va, lp->d_secsize >> 1);
	
		/* Check data request (should be done). */
		if (wait_for_ready(wdc) != 0) {
			wderror(d_link, NULL,
			    "wddump: timeout waiting for ready");
			return EIO;
		}
#else	/* WD_DUMP_NOT_TRUSTED */
		/* Let's just talk about this first... */
		printf("wd%d: dump addr 0x%x, cylin %d, head %d, sector %d\n",
		    unit, va, cylin, head, sector);
		delay(500 * 1000);	/* half a second */
#endif

		/* update block count */
		nblks -= 1;
		blkno += 1;
		va += lp->d_secsize;
	}

	wddoingadump = 0;
	return 0;
}
#else /* __BDEVSW_DUMP_NEW_TYPE */


int
wddump(dev, blkno, va, size)
        dev_t dev;
        daddr_t blkno;
        caddr_t va;
        size_t size;
{

	/* Not implemented. */
	return ENXIO;
}
#endif /* __BDEVSW_DUMP_NEW_TYPE */

#ifdef DKBAD
/*
 * Internalize the bad sector table.
 */
void
bad144intern(wd)
	struct wd_softc *wd;
{
	struct dkbad *bt = &DKBAD(wd->sc_dk.dk_cpulabel);
	struct disklabel *lp = wd->sc_dk.dk_label;
	struct wd_link *d_link = wd->d_link;
	int i = 0;

	WDDEBUG_PRINT(("bad144intern\n"));

	for (; i < 126; i++) {
		if (bt->bt_bad[i].bt_cyl == 0xffff)
			break;
		d_link->sc_badsect[i] =
		    bt->bt_bad[i].bt_cyl * lp->d_secpercyl +
		    (bt->bt_bad[i].bt_trksec >> 8) * lp->d_nsectors +
		    (bt->bt_bad[i].bt_trksec & 0xff);
	}
	for (; i < 127; i++)
		d_link->sc_badsect[i] = -1;
}
#endif

void
wderror(d_link, bp, msg)
	struct wd_link *d_link;
	struct buf *bp;
	char *msg;
{
	struct wd_softc *wd = (struct wd_softc *)d_link->wd_softc;

	if (bp) {
		diskerr(bp, "wd", msg, LOG_PRINTF, bp->b_bcount,
		    wd->sc_dk.dk_label);
		printf("\n");
	} else
		printf("%s: %s\n", wd->sc_dev.dv_xname, msg);
}

void
wddone(d_link, bp)
	struct wd_link *d_link;
	struct buf *bp;
{
	struct wd_softc *wd = (void *)d_link->wd_softc;

	disk_unbusy(&wd->sc_dk, (bp->b_bcount - bp->b_resid));
}
