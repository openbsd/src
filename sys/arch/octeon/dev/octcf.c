/*	$OpenBSD: octcf.c,v 1.3 2011/06/03 21:14:11 matthew Exp $ */
/*	$NetBSD: wd.c,v 1.193 1999/02/28 17:15:27 explorer Exp $ */

/*
 * Copyright (c) 1998, 2001 Manuel Bouyer.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	notice, this list of conditions and the following disclaimer in the
 *	documentation and/or other materials provided with the distribution.
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

/*-
 * Copyright (c) 1998 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Charles M. Hannum and by Onno van der Linden.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mutex.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/syslog.h>
#include <sys/proc.h>
#include <sys/vnode.h>
#include <sys/dkio.h>

#include <uvm/uvm_extern.h>

#include <machine/intr.h>
#include <machine/bus.h>

#include <dev/ata/atareg.h>
#include <dev/ata/atavar.h>
#include <dev/ic/wdcreg.h>
#include <dev/ic/wdcvar.h>

#include <octeon/dev/iobusvar.h>
#include <octeon/dev/octeonreg.h>

#define OCTCF_REG_SIZE	8
#define ATAPARAMS_SIZE	512
#define SECTOR_SIZE	512
#define OCTCFDELAY	100 /* 100 microseconds */
#define NR_TRIES	1000

#define DEBUG_XFERS  0x02
#define DEBUG_FUNCS  0x08
#define DEBUG_PROBE  0x10

#ifdef OCTCFDEBUG
int octcfdebug_mask = 0xff;
#define OCTCFDEBUG_PRINT(args, level) do {	\
	if ((octcfdebug_mask & (level)) != 0)	\
		printf args;			\
} while (0)
#else
#define OCTCFDEBUG_PRINT(args, level)
#endif

struct octcf_softc {
	/* General disk infos */
	struct device sc_dev;
	struct disk sc_dk;
	struct buf sc_q;
	struct buf *sc_bp;
	struct ataparams sc_params;/* drive characteristics found */
	int sc_flags;
#define OCTCFF_WLABEL		0x04 /* label is writable */
#define OCTCFF_LABELLING	0x08 /* writing label */
#define OCTCFF_LOADED		0x10 /* parameters loaded */
	u_int64_t sc_capacity;
	bus_space_tag_t       sc_iot;
	bus_space_handle_t    sc_ioh;
};

int	octcfprobe(struct device *, void *, void *);
void	octcfattach(struct device *, struct device *, void *);
int	octcfdetach(struct device *, int);
int	octcfactivate(struct device *, int);
int	octcfprint(void *, char *);

struct cfattach octcf_ca = {
	sizeof(struct octcf_softc), octcfprobe, octcfattach,
	octcfdetach, octcfactivate
};

struct cfdriver octcf_cd = {
	NULL, "octcf", DV_DISK
};

void  octcfgetdefaultlabel(struct octcf_softc *, struct disklabel *);
int   octcfgetdisklabel(dev_t dev, struct octcf_softc *, struct disklabel *, int);
void  octcfstrategy(struct buf *);
void  octcfstart(void *);
void  _octcfstart(struct octcf_softc*, struct buf *);
void  octcfdone(void *);

cdev_decl(octcf);
bdev_decl(octcf);

#define octcflock(wd)  disk_lock(&(wd)->sc_dk)
#define octcfunlock(wd)  disk_unlock(&(wd)->sc_dk)

#define octcflookup(unit) (struct octcf_softc *)disk_lookup(&octcf_cd, (unit))

int	octcf_write_sectors(struct octcf_softc *, uint32_t, uint32_t, void *);
int	octcf_read_sectors(struct octcf_softc *, uint32_t, uint32_t, void *);
int	octcf_wait_busy(struct octcf_softc *);
void	octcf_command(struct octcf_softc *, uint32_t, uint8_t);
int 	octcf_get_params(struct octcf_softc *, struct ataparams *);

#define OCTCF_REG_READ(wd, reg) \
	bus_space_read_2(wd->sc_iot, wd->sc_ioh, reg & 0x6)
#define OCTCF_REG_WRITE(wd, reg, val) \
	bus_space_write_2(wd->sc_iot, wd->sc_ioh, reg & 0x6, val)

int
octcfprobe(struct device *parent, void *match_, void *aux)
{
	return 1;
}

void
octcfattach(struct device *parent, struct device *self, void *aux)
{
	struct octcf_softc *wd = (void *)self;
	struct iobus_attach_args *aa = aux;
	int i, blank;
	char buf[41], c, *p, *q;
	OCTCFDEBUG_PRINT(("octcfattach\n"), DEBUG_FUNCS | DEBUG_PROBE);
	uint8_t status;

	wd->sc_iot = aa->aa_bust;

	if (bus_space_map(wd->sc_iot, aa->aa_unit->addr,
	    OCTCF_REG_SIZE, BUS_SPACE_MAP_KSEG0, &wd->sc_ioh)) {
		printf(": couldn't map registers\n");
		return;
	}

	for (i = 0; i < 8; i++) {
		uint64_t cfg = 
		*(uint64_t *)PHYS_TO_XKPHYS(
			OCTEON_MIO_BOOT_BASE + MIO_BOOT_REG_CFG(i), CCA_NC);

		if ((cfg & BOOT_CFG_BASE_MASK) ==
			(OCTEON_CF_BASE >> BOOT_CFG_BASE_SHIFT)) {
			if ((cfg & BOOT_CFG_WIDTH_MASK) == 0)
				printf(": Doesn't support 8bit card\n",
					wd->sc_dev.dv_xname);
			break;
		}
	}

	/* Check if CF is inserted */
	i = 0;
	while ( (status = (OCTCF_REG_READ(wd, wdr_status)>>8)) & WDCS_BSY) {
		if ((i++) == NR_TRIES )     {
			printf(": card not present\n", wd->sc_dev.dv_xname);
			return;
               	}
		DELAY(OCTCFDELAY);
	}

	/* read our drive info */
	if (octcf_get_params(wd, &wd->sc_params) != 0) {
		printf(": IDENTIFY failed\n", wd->sc_dev.dv_xname);
		return;
	}

	for (blank = 0, p = wd->sc_params.atap_model, q = buf, i = 0;
	    i < sizeof(wd->sc_params.atap_model); i++) {
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
	printf("%s: %d-sector PIO,", 
		wd->sc_dev.dv_xname, wd->sc_params.atap_multi & 0xff);

	wd->sc_capacity =
		wd->sc_params.atap_cylinders *
		wd->sc_params.atap_heads *
		wd->sc_params.atap_sectors;
	printf(" CHS, %lluMB, %d cyl, %d head, %d sec, %llu sectors\n",
		wd->sc_capacity / (1048576 / DEV_BSIZE),
		wd->sc_params.atap_cylinders,
		wd->sc_params.atap_heads,
		wd->sc_params.atap_sectors,
		wd->sc_capacity);

	OCTCFDEBUG_PRINT(
		("%s: atap_dmatiming_mimi=%d, atap_dmatiming_recom=%d\n",
		self->dv_xname, wd->sc_params.atap_dmatiming_mimi,
		wd->sc_params.atap_dmatiming_recom), DEBUG_PROBE);

	/*
	 * Initialize disk structures.
	 */
	wd->sc_dk.dk_name = wd->sc_dev.dv_xname;

	/* Attach disk. */
	disk_attach(&wd->sc_dev, &wd->sc_dk);
}

int
octcfactivate(struct device *self, int act)
{
	return 0;
}

int
octcfdetach(struct device *self, int flags)
{
	struct octcf_softc *sc = (struct octcf_softc *)self;
	int bmaj, cmaj, mn;

	/* Locate the lowest minor number to be detached. */
	mn = DISKMINOR(self->dv_unit, 0);

	for (bmaj = 0; bmaj < nblkdev; bmaj++)
		if (bdevsw[bmaj].d_open == octcfopen)
			vdevgone(bmaj, mn, mn + MAXPARTITIONS - 1, VBLK);
	for (cmaj = 0; cmaj < nchrdev; cmaj++)
		if (cdevsw[cmaj].d_open == octcfopen)
			vdevgone(cmaj, mn, mn + MAXPARTITIONS - 1, VCHR);

	/* Detach disk. */
	disk_detach(&sc->sc_dk);

	return (0);
}

/*
 * Read/write routine for a buffer.  Validates the arguments and schedules the
 * transfer.  Does not wait for the transfer to complete.
 */
void
octcfstrategy(struct buf *bp)
{
	struct octcf_softc *wd;
	int s;

	wd = octcflookup(DISKUNIT(bp->b_dev));
	if (wd == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}
	OCTCFDEBUG_PRINT(("octcfstrategy (%s)\n", wd->sc_dev.dv_xname),
	    DEBUG_XFERS);
	/* Valid request?  */
	if (bp->b_blkno < 0 ||
	    (bp->b_bcount % wd->sc_dk.dk_label->d_secsize) != 0 ||
	    (bp->b_bcount / wd->sc_dk.dk_label->d_secsize) >= (1 << NBBY)) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/* If device invalidated (e.g. media change, door open), error. */
	if ((wd->sc_flags & OCTCFF_LOADED) == 0) {
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
	if (bounds_check_with_label(bp, wd->sc_dk.dk_label) <= 0)
		goto done;
	/* Queue transfer on drive, activate drive and controller if idle. */
	s = splbio();
	disksort(&wd->sc_q, bp);
	octcfstart(wd);
	splx(s);
	device_unref(&wd->sc_dev);
	return;
bad:
	bp->b_flags |= B_ERROR;
done:
	/* Toss transfer; we're done early. */
	bp->b_resid = bp->b_bcount;
	s = splbio();
	biodone(bp);
	splx(s);
	if (wd != NULL)
		device_unref(&wd->sc_dev);
}

/*
 * Queue a drive for I/O.
 */
void
octcfstart(void *arg)
{
	struct octcf_softc *wd = arg;
	struct buf *dp, *bp;

	OCTCFDEBUG_PRINT(("octcfstart %s\n", wd->sc_dev.dv_xname),
	    DEBUG_XFERS);
	while (1) {
		/* Remove the next buffer from the queue or stop. */
		dp = &wd->sc_q;
		bp = dp->b_actf;
		if (bp == NULL)
			return;
		dp->b_actf = bp->b_actf;

		/* Transfer this buffer now. */
		_octcfstart(wd, bp);
	}
}

void
_octcfstart(struct octcf_softc *wd, struct buf *bp)
{
	uint32_t blkno;
	uint32_t nblks;

	blkno = bp->b_blkno +
	    DL_GETPOFFSET(&wd->sc_dk.dk_label->d_partitions[DISKPART(bp->b_dev)]);
	blkno /= (SECTOR_SIZE / DEV_BSIZE);
	nblks = bp->b_bcount / SECTOR_SIZE;

	wd->sc_bp = bp;

	/* Instrumentation. */
	disk_busy(&wd->sc_dk);

	if (bp->b_flags & B_READ)
		bp->b_error = octcf_read_sectors(wd, nblks, blkno, bp->b_data);
	else
		bp->b_error = octcf_write_sectors(wd, nblks, blkno, bp->b_data);

	octcfdone(wd);
}

void
octcfdone(void *arg)
{
	struct octcf_softc *wd = arg;
	struct buf *bp = wd->sc_bp;

	if (bp->b_error == 0)
		bp->b_resid = 0;
	else
		bp->b_flags |= B_ERROR;

	disk_unbusy(&wd->sc_dk, (bp->b_bcount - bp->b_resid),
	    (bp->b_flags & B_READ));
	biodone(bp);
}

int
octcfread(dev_t dev, struct uio *uio, int flags)
{

	OCTCFDEBUG_PRINT(("wdread\n"), DEBUG_XFERS);
	return (physio(octcfstrategy, dev, B_READ, minphys, uio));
}

int
octcfwrite(dev_t dev, struct uio *uio, int flags)
{

	OCTCFDEBUG_PRINT(("octcfwrite\n"), DEBUG_XFERS);
	return (physio(octcfstrategy, dev, B_WRITE, minphys, uio));
}

int
octcfopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct octcf_softc *wd;
	int unit, part;
	int error;

	OCTCFDEBUG_PRINT(("octcfopen\n"), DEBUG_FUNCS);

	unit = DISKUNIT(dev);
	wd = octcflookup(unit);
	if (wd == NULL) 
		return ENXIO;

	/*
	 * If this is the first open of this device, add a reference
	 * to the adapter.
	 */
	if ((error = octcflock(wd)) != 0) 
		goto bad4;

	if (wd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((wd->sc_flags & OCTCFF_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		if ((wd->sc_flags & OCTCFF_LOADED) == 0) {
			wd->sc_flags |= OCTCFF_LOADED;

			/* Load the physical device parameters. */
			octcf_get_params(wd, &wd->sc_params);

			/* Load the partition info if not already loaded. */
			if (octcfgetdisklabel(dev, wd,
			    wd->sc_dk.dk_label, 0) == EIO) {
				error = EIO;
				goto bad;
			}
		}
	}

	part = DISKPART(dev);

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
	wd->sc_dk.dk_openmask =
	    wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	octcfunlock(wd);
	device_unref(&wd->sc_dev);
	return 0;

bad:
	if (wd->sc_dk.dk_openmask == 0) {
	}

bad3:
	octcfunlock(wd);
bad4:
	device_unref(&wd->sc_dev);
	return error;
}

int
octcfclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct octcf_softc *wd;
	int part = DISKPART(dev);
	int error = 0;

	wd = octcflookup(DISKUNIT(dev));
	if (wd == NULL)
		return ENXIO;

	OCTCFDEBUG_PRINT(("octcfclose\n"), DEBUG_FUNCS);
	if ((error = octcflock(wd)) != 0)
		goto exit;

	switch (fmt) {
	case S_IFCHR:
		wd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		wd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	wd->sc_dk.dk_openmask =
	    wd->sc_dk.dk_copenmask | wd->sc_dk.dk_bopenmask;

	octcfunlock(wd);

 exit:
	device_unref(&wd->sc_dev);
	return (error);
}

void
octcfgetdefaultlabel(struct octcf_softc *wd, struct disklabel *lp)
{
	OCTCFDEBUG_PRINT(("octcfgetdefaultlabel\n"), DEBUG_FUNCS);
	bzero(lp, sizeof(struct disklabel));

	lp->d_secsize = DEV_BSIZE;
	DL_SETDSIZE(lp, wd->sc_capacity);
	lp->d_ntracks = wd->sc_params.atap_heads;
	lp->d_nsectors = wd->sc_params.atap_sectors;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	lp->d_ncylinders = DL_GETDSIZE(lp) / lp->d_secpercyl;
 lp->d_type = DTYPE_ESDI;
	strncpy(lp->d_typename, "ESDI/IDE disk", sizeof lp->d_typename);

	/* XXX - user viscopy() like sd.c */
	strncpy(lp->d_packname, wd->sc_params.atap_model, sizeof lp->d_packname);
	lp->d_flags = 0;
	lp->d_version = 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
}

/*
 * Fabricate a default disk label, and try to read the correct one.
 */
int
octcfgetdisklabel(dev_t dev, struct octcf_softc *wd, struct disklabel *lp,
    int spoofonly)
{
	int error;

	OCTCFDEBUG_PRINT(("octcfgetdisklabel\n"), DEBUG_FUNCS);

	octcfgetdefaultlabel(wd, lp);
	error = readdisklabel(DISKLABELDEV(dev), octcfstrategy, lp,
	    spoofonly);
	return (error);
}

int
octcfioctl(dev_t dev, u_long xfer, caddr_t addr, int flag, struct proc *p)
{
	struct octcf_softc *wd;
	struct disklabel *lp;
	int error = 0;

	OCTCFDEBUG_PRINT(("octcfioctl\n"), DEBUG_FUNCS);

	wd = octcflookup(DISKUNIT(dev));
	if (wd == NULL)
		return ENXIO;

	if ((wd->sc_flags & OCTCFF_LOADED) == 0) {
		error = EIO;
		goto exit;
	}

	switch (xfer) {
	case DIOCRLDINFO:
		lp = malloc(sizeof(*lp), M_TEMP, M_WAITOK);
		octcfgetdisklabel(dev, wd, lp, 0);
		bcopy(lp, wd->sc_dk.dk_label, sizeof(*lp));
		free(lp, M_TEMP);
		goto exit;

	case DIOCGPDINFO:
		octcfgetdisklabel(dev, wd, (struct disklabel *)addr, 1);
		goto exit;

	case DIOCGDINFO:
		*(struct disklabel *)addr = *(wd->sc_dk.dk_label);
		goto exit;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = wd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &wd->sc_dk.dk_label->d_partitions[DISKPART(dev)];
		goto exit;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			goto exit;
		}

		if ((error = octcflock(wd)) != 0)
			goto exit;
		wd->sc_flags |= OCTCFF_LABELLING;

		error = setdisklabel(wd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*wd->sc_dk.dk_openmask : */0);
		if (error == 0) {
			if (xfer == DIOCWDINFO)
				error = writedisklabel(DISKLABELDEV(dev),
				    octcfstrategy, wd->sc_dk.dk_label);
		}

		wd->sc_flags &= ~OCTCFF_LABELLING;
		octcfunlock(wd);
		goto exit;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			goto exit;
		}

		if (*(int *)addr)
			wd->sc_flags |= OCTCFF_WLABEL;
		else
			wd->sc_flags &= ~OCTCFF_WLABEL;
		goto exit;

#ifdef notyet
	case DIOCWFORMAT:
		if ((flag & FWRITE) == 0)
			return EBADF;
		{
		struct format_op *fop;
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
		error = physio(wdformat, dev, B_WRITE, minphys, &auio);
		fop->df_count -= auio.uio_resid;
		fop->df_reg[0] = wdc->sc_status;
		fop->df_reg[1] = wdc->sc_error;
		goto exit;
		}
#endif

	default:
		error = ENOTTY;
		goto exit;
	}

#ifdef DIAGNOSTIC
	panic("octcfioctl: impossible");
#endif

 exit:
	device_unref(&wd->sc_dev);
	return (error);
}

#ifdef B_FORMAT
int
wdformat(struct buf *bp)
{

	bp->b_flags |= B_FORMAT;
	return octcfstrategy(bp);
}
#endif

daddr64_t
octcfsize(dev_t dev)
{
	struct octcf_softc *wd;
	int part, omask;
	int64_t size;

	OCTCFDEBUG_PRINT(("wdsize\n"), DEBUG_FUNCS);

	wd = octcflookup(DISKUNIT(dev));
	if (wd == NULL)
		return (-1);

	part = DISKPART(dev);
	omask = wd->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && octcfopen(dev, 0, S_IFBLK, NULL) != 0) {
		size = -1;
		goto exit;
	}

	size = DL_GETPSIZE(&wd->sc_dk.dk_label->d_partitions[part]) *
	    (wd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && octcfclose(dev, 0, S_IFBLK, NULL) != 0)
		size = -1;

 exit:
	device_unref(&wd->sc_dev);
	return (size);
}

/*
 * Dump core after a system crash.
 */
int
octcfdump(dev_t dev, daddr64_t blkno, caddr_t va, size_t size)
{
	return ENXIO;
}

int 
octcf_read_sectors(struct octcf_softc *wd, uint32_t nr_sectors, 
	uint32_t start_sector, void *buf)
{
	uint32_t count;
	uint16_t *ptr = (uint16_t*)buf;
	int error;
	uint8_t status;

	while (nr_sectors--) {
		while ((status = (OCTCF_REG_READ(wd, wdr_status)>>8)) & WDCS_BSY)
			DELAY(OCTCFDELAY);
		octcf_command(wd, start_sector++, WDCC_READ);
		error = octcf_wait_busy(wd);
		if (error != 0)
			return (error);

        	volatile uint16_t dummy;
		for (count = 0; count < SECTOR_SIZE; count+=2) {
			uint16_t temp;
			temp = OCTCF_REG_READ(wd, 0x0);
			*ptr++ = swap16(temp);
			if ((count & 0xf) == 0)
				dummy = OCTCF_REG_READ(wd, wdr_status);
		}
	}
	return (0);
}

int
octcf_write_sectors(struct octcf_softc *wd, uint32_t nr_sectors, 
	uint32_t start_sector, void *buf)
{
	uint32_t count;
	uint16_t *ptr = (uint16_t*)buf;
	int error;
	uint8_t status;

	while (nr_sectors--) {
		while ((status = (OCTCF_REG_READ(wd, wdr_status)>>8)) & WDCS_BSY)
			DELAY(OCTCFDELAY);
		octcf_command(wd, start_sector++, WDCC_WRITE);
		if((error = octcf_wait_busy(wd)))
			return (error);

	      	volatile uint16_t dummy;
		for (count = 0; count < SECTOR_SIZE; count+=2) {
			uint16_t temp = *ptr++;
			OCTCF_REG_WRITE(wd, 0x0, swap16(temp));
			if ((count & 0xf) == 0)
				dummy = OCTCF_REG_READ(wd, wdr_status);
		}
	}
	return (0);
}

void
octcf_command(struct octcf_softc *wd, uint32_t lba, uint8_t cmd)
{
	OCTCF_REG_WRITE(wd, wdr_seccnt, 1 | ((lba & 0xff) << 8));
	OCTCF_REG_WRITE(wd, wdr_cyl_lo, 
		((lba >> 8) & 0xff) | (((lba >> 16) & 0xff) << 8));
	OCTCF_REG_WRITE(wd, wdr_sdh, 
		(((lba >> 24) & 0xff) | 0xe0) | (cmd << 8));
}

int 
octcf_wait_busy(struct octcf_softc *wd)
{
	uint8_t status;

	status = OCTCF_REG_READ(wd, wdr_status)>>8;	
	while ((status & WDCS_BSY) == WDCS_BSY) {
		if ((status & WDCS_DWF) != 0)
			return (EIO);
		DELAY(OCTCFDELAY);
		status = (uint8_t)(OCTCF_REG_READ(wd, wdr_status)>>8);
	}

	if ((status & WDCS_DRQ) == 0) 
		return (ENXIO);

	return (0);
}

/* Get the disk's parameters */
int
octcf_get_params(struct octcf_softc *wd, struct ataparams *prms)
{
	char *tb;
	int i;
	u_int16_t *p;
	int count;
	uint8_t status;
	int error;

	OCTCFDEBUG_PRINT(("octcf_get_parms\n"), DEBUG_FUNCS);

	tb = malloc(ATAPARAMS_SIZE, M_DEVBUF, M_NOWAIT | M_ZERO);
	if (tb == NULL)
		return CMD_AGAIN;

	while ((status = (OCTCF_REG_READ(wd, wdr_status)>>8)) & WDCS_BSY)
		DELAY(OCTCFDELAY);

	OCTCF_REG_WRITE(wd, wdr_seccnt, 0);
	OCTCF_REG_WRITE(wd, wdr_cyl_lo, 0);
	OCTCF_REG_WRITE(wd, wdr_sdh, 0 | (WDCC_IDENTIFY<<8));

	error = octcf_wait_busy(wd);
	if (error == 0) {
		for (count = 0; count < SECTOR_SIZE; count+=2) {
			uint16_t temp;
			temp = OCTCF_REG_READ(wd, 0x0);

			/* endianess will be swapped below */
			tb[count]   = (temp & 0xff);
			tb[count+1] = (temp & 0xff00)>>8;
		}
	}

	if (error != 0) {
		printf("%s: identify failed: %d\n", __func__, error);
		free(tb, M_DEVBUF);
		return CMD_ERR;
	} else {
#if BYTE_ORDER == BIG_ENDIAN
		/* All the fields in the params structure are 16-bit
		   integers except for the ID strings which are char
		   strings.  The 16-bit integers are currently in
		   memory in little-endian, regardless of architecture.
		   So, they need to be swapped on big-endian architectures
		   before they are accessed through the ataparams structure.

		   The swaps below avoid touching the char strings.
		*/

		swap16_multi((u_int16_t *)tb, 10);
		swap16_multi((u_int16_t *)tb + 20, 3);
		swap16_multi((u_int16_t *)tb + 47, ATAPARAMS_SIZE / 2 - 47);
#endif
		/* Read in parameter block. */
		bcopy(tb, prms, sizeof(struct ataparams));

		/*
		 * Shuffle string byte order.
		 * ATAPI Mitsumi and NEC drives don't need this.
		 */
		if ((prms->atap_config & WDC_CFG_ATAPI_MASK) ==
		    WDC_CFG_ATAPI &&
		    ((prms->atap_model[0] == 'N' &&
			prms->atap_model[1] == 'E') ||
		     (prms->atap_model[0] == 'F' &&
			 prms->atap_model[1] == 'X'))) {
			free(tb, M_DEVBUF);
			return CMD_OK;
		}
		for (i = 0; i < sizeof(prms->atap_model); i += 2) {
			p = (u_short *)(prms->atap_model + i);
			*p = swap16(*p);
		}
		for (i = 0; i < sizeof(prms->atap_serial); i += 2) {
			p = (u_short *)(prms->atap_serial + i);
			*p = swap16(*p);
		}
		for (i = 0; i < sizeof(prms->atap_revision); i += 2) {
			p = (u_short *)(prms->atap_revision + i);
			*p = swap16(*p);
		}

		free(tb, M_DEVBUF);
		return CMD_OK;
	}
}
