/*	$OpenBSD: sd.c,v 1.16 1998/04/30 04:55:46 millert Exp $	*/
/*	$NetBSD: sd.c,v 1.34 1997/07/10 18:14:10 kleink Exp $	*/

/*
 * Copyright (c) 1996, 1997 Jason R. Thorpe.  All rights reserved.
 * Copyright (c) 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Van Jacobson of Lawrence Berkeley Laboratory.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)sd.c	8.5 (Berkeley) 5/19/94
 */

/*
 * SCSI CCS (Command Command Set) disk driver.
 */

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/buf.h>
#include <sys/device.h>
#include <sys/disk.h>
#include <sys/disklabel.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/proc.h>
#include <sys/stat.h>

#include <ufs/ffs/fs.h>			/* for BBSIZE and SBSIZE */

#include <hp300/dev/scsireg.h>
#include <hp300/dev/scsivar.h>
#include <hp300/dev/sdvar.h>

#include "opt_useleds.h"

#ifdef USELEDS
#include <hp300/hp300/leds.h>
#endif

/*
extern void disksort();
extern void biodone();
extern int physio();
extern void TBIS();
*/

int	sdmatch __P((struct device *, void *, void *));
void	sdattach __P((struct device *, struct device *, void *));

struct cfattach sd_ca = {
	sizeof(struct sd_softc), sdmatch, sdattach
};

struct cfdriver sd_cd = {
	NULL, "sd", DV_DISK
};

#ifdef DEBUG
int sddebug = 1;
#define SDB_ERROR	0x01
#define SDB_PARTIAL	0x02
#define SDB_CAPACITY	0x04
#endif

static struct scsi_fmt_cdb sd_read_cmd = { 10, { CMD_READ_EXT } };
static struct scsi_fmt_cdb sd_write_cmd = { 10, { CMD_WRITE_EXT } };

/*
 * Table of scsi commands users are allowed to access via "format"
 * mode.  0 means not legal.  1 means "immediate" (doesn't need dma).
 * -1 means needs dma and/or wait for intr.
 */
static char legal_cmds[256] = {
/*****  0   1   2   3   4   5   6   7     8   9   A   B   C   D   E   F */
/*00*/	0,  0,  0,  0, -1,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*10*/	0,  0,  1,  0,  0,  1,  0,  0,    0,  0,  1,  0,  0,  0,  0,  0,
/*20*/	0,  0,  0,  0,  0,  1,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*30*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*40*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*50*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*60*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*70*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*80*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*90*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*a0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*b0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*c0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*d0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*e0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
/*f0*/	0,  0,  0,  0,  0,  0,  0,  0,    0,  0,  0,  0,  0,  0,  0,  0,
};

/* bdev_decl(sd); */
/* cdev_decl(sd); */
/* XXX we should use macros to do these... */
int	sdopen __P((dev_t, int, int, struct proc *));
int	sdclose __P((dev_t, int, int, struct proc *));

int	sdioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int	sdread __P((dev_t, struct uio *, int));
void	sdreset __P((struct sd_softc *));
int	sdwrite __P((dev_t, struct uio *, int));

void	sdstrategy __P((struct buf *));
int	sddump __P((dev_t, daddr_t, caddr_t, size_t));
int	sdsize __P((dev_t));

static void 	sdgetgeom __P((struct sd_softc *));
static void	sdlblkstrat __P((struct buf *, int));
static int	sderror __P((struct sd_softc *, int));
static void	sdfinish __P((struct sd_softc *, struct buf *));

/*
 * Perform a mode-sense on page 0x04 (rigid geometry).
 */
static void
sdgetgeom(sc)
	struct sd_softc *sc;
{
	struct scsi_mode_sense_geom {
		struct scsi_modesense_hdr	header;
		struct scsi_geometry		geom;
	} sensebuf;
	struct scsi_fmt_cdb modesense_geom;
	int ctlr, slave, unit;

	/* XXX - if we try to do this in the declaration gcc uses memset() */
	bzero(&modesense_geom, sizeof(modesense_geom));
	modesense_geom.len = 6;
	modesense_geom.cdb[0] = CMD_MODE_SENSE;
	modesense_geom.cdb[2] = 0x04;
	modesense_geom.cdb[4] = sizeof(sensebuf);

	ctlr = sc->sc_dev.dv_parent->dv_unit;
	slave = sc->sc_target;
	unit = sc->sc_lun;

	scsi_delay(-1);		/* XXX */
	(void)scsi_immed_command(ctlr, slave, unit, &modesense_geom,
	    (u_char *)&sensebuf, sizeof(sensebuf), B_READ);
	scsi_delay(0);		/* XXX */

	sc->sc_heads = sensebuf.geom.heads;
	sc->sc_cyls = (sensebuf.geom.cyl_ub << 16) |
	    (sensebuf.geom.cyl_mb << 8) | sensebuf.geom.cyl_lb;
}

int
sdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct oscsi_attach_args *osa = aux;

	switch (osa->osa_inqbuf->type) {
	case 0:		/* disk */
	case 4:		/* WORM */
	case 5:		/* CD-ROM */
	case 7:		/* Magneto-optical */
		break;
	default:	/* not a disk */
		return 0;
	}

	return (1);
}

void
sdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct sd_softc *sc = (struct sd_softc *)self;
	struct oscsi_attach_args *osa = aux;

	/*
	 * XXX formerly 0 meant unused but now pid 0 can legitimately
	 * use this interface (sdgetcapacity).
	 */
	sc->sc_format_pid = -1;
	sc->sc_flags = 0;

	sc->sc_target = osa->osa_target;
	sc->sc_lun = osa->osa_lun;
	sc->sc_type = osa->osa_inqbuf->type;

	if (osa->osa_inqbuf->qual & 0x80)
		sc->sc_flags |= SDF_RMEDIA;

	printf("\n");

	/* Initialize the SCSI queue entry. */
	sc->sc_sq.sq_softc = sc;
	sc->sc_sq.sq_target = sc->sc_target;
	sc->sc_sq.sq_lun = sc->sc_lun;
	sc->sc_sq.sq_start = sdstart;
	sc->sc_sq.sq_go = sdgo;
	sc->sc_sq.sq_intr = sdintr;

	if (sdgetcapacity(sc, NODEV) < 0) {
		printf("%s: getcapacity failed!\n", sc->sc_dev.dv_xname);
		return;
	}

	/*
	 * Print out some additional information.
	 */
	printf("%s: ", sc->sc_dev.dv_xname);
	switch (sc->sc_type) {
	case 4:	
		printf("WORM, ");
		break;

	case 5:
		printf("CD-ROM, ");
		break;

	case 7:
		printf("Magneto-optical, ");
		break;

	default:
		printf("%d cylinders, %d heads, ",
		    sc->sc_cyls, sc->sc_heads);
	}
	if (sc->sc_blks)
		printf("%d blocks, %d bytes/block\n",
		    sc->sc_blks >> sc->sc_bshift, sc->sc_blksize);
	else
		printf("drive empty\n");

	/* Initialize the disk structure. */
	sc->sc_dkdev.dk_name = sc->sc_dev.dv_xname;

	/* Attach the disk. */
	disk_attach(&sc->sc_dkdev);

	dk_establish(&sc->sc_dkdev, &sc->sc_dev);	/* XXX */

	sc->sc_flags |= SDF_ALIVE;
}

void
sdreset(sc)
	struct sd_softc *sc;
{
	sc->sc_stats.sdresets++;
}

/*
 * Determine capacity of a drive.
 * Returns -1 on a failure, 0 on success, 1 on a failure that is probably
 * due to missing media.
 */
int
sdgetcapacity(sc, dev)
	struct sd_softc *sc;
	dev_t dev;
{
	static struct scsi_fmt_cdb cap = {
		10,
		{ CMD_READ_CAPACITY, 0, 0, 0, 0, 0, 0, 0, 0, 0 }
	};
	u_char *capbuf;
	int i, capbufsize;

	/*
	 * Cannot use stack space for this buffer since stack KVA may not
	 * be valid (i.e. in context of this process) when the operation
	 * actually starts.
	 */
	capbufsize = 8;
	capbuf = malloc(capbufsize, M_DEVBUF, M_WAITOK);

	if (dev == NODEV) {
		scsi_delay(-1);		/* XXX */
		i = scsi_immed_command(sc->sc_dev.dv_parent->dv_unit,
		    sc->sc_target, sc->sc_lun, &cap, capbuf,
		    capbufsize, B_READ);
		scsi_delay(0);		/* XXX */
	} else {
		struct buf *bp;

		/*
		 * XXX this is horrible
		 */
		if (sc->sc_format_pid >= 0)
			panic("sdgetcapacity");
		bp = malloc(sizeof *bp, M_DEVBUF, M_WAITOK);
		sc->sc_format_pid = curproc->p_pid;
		bcopy(&cap, &sc->sc_cmdstore, sizeof cap);
		bp->b_dev = dev;
		bp->b_flags = B_READ | B_BUSY;
		bp->b_un.b_addr = (caddr_t)capbuf;
		bp->b_bcount = capbufsize;
		sdstrategy(bp);
		i = biowait(bp) ? sc->sc_sensestore.status : 0;
		free(bp, M_DEVBUF);
		sc->sc_format_pid = -1;
	}
	if (i) {
		if (i != STS_CHECKCOND || (sc->sc_flags & SDF_RMEDIA) == 0) {
#ifdef DEBUG
			if (sddebug & SDB_CAPACITY)
				printf("%s: read_capacity returns %d\n",
				       sc->sc_dev.dv_xname, i);
#endif
			free(capbuf, M_DEVBUF);
			return (-1);
		}
		/*
		 * XXX assume unformatted or non-existant media
		 */
		sc->sc_blks = 0;
		sc->sc_blksize = DEV_BSIZE;
		sc->sc_bshift = 0;
#ifdef DEBUG
		if (sddebug & SDB_CAPACITY)
			printf("%s: removable media not present\n",
			       sc->sc_dev.dv_xname);
#endif
		free(capbuf, M_DEVBUF);
		return (1);
	}
	sc->sc_blks = *(u_int *)&capbuf[0];
	sc->sc_blksize = *(int *)&capbuf[4];
	free(capbuf, M_DEVBUF);
	sc->sc_bshift = 0;

	/* return value of read capacity is last valid block number */
	sc->sc_blks++;

	if (sc->sc_blksize != DEV_BSIZE) {
		if (sc->sc_blksize < DEV_BSIZE) {
			printf("%s: need at least %d byte blocks - %s\n",
			    sc->sc_dev.dv_xname, DEV_BSIZE, "drive ignored");
			return (-1);
		}
		for (i = sc->sc_blksize; i > DEV_BSIZE; i >>= 1)
			++sc->sc_bshift;
		sc->sc_blks <<= sc->sc_bshift;
	}
#ifdef DEBUG
	if (sddebug & SDB_CAPACITY)
		printf("%s: blks=%d, blksize=%d, bshift=%d\n",
		    sc->sc_dev.dv_xname, sc->sc_blks, sc->sc_blksize,
		    sc->sc_bshift);
#endif
	sdgetgeom(sc);
	return (0);
}

/*
 * Read or constuct a disklabel
 */
int
sdgetinfo(dev)
	dev_t dev;
{
	int unit = sdunit(dev);
	struct sd_softc *sc = sd_cd.cd_devs[unit];
	struct disklabel *lp = sc->sc_dkdev.dk_label;
	char *errstring;

	bzero((caddr_t)lp, sizeof *lp);
	errstring = NULL;

	/*
	 * If removable media or the size unavailable at boot time
	 * (i.e. unformatted hard disk), attempt to set the capacity
	 * now.
	 */
	if ((sc->sc_flags & SDF_RMEDIA) || sc->sc_blks == 0) {
		switch (sdgetcapacity(sc, dev)) {
		case 0:
			break;
		case -1:
			/*
			 * Hard error, just return (open will fail).
			 */
			return (EIO);
		case 1:
			/*
			 * XXX return 0 so open can continue just in case
			 * the media is unformatted and we want to format it.
			 * We set the error flag so they cannot do much else.
			 */
			sc->sc_flags |= SDF_ERROR;
			/* XXX set magic here or it will never be set */
			lp->d_magic = DISKMAGIC;
			lp->d_magic2 = DISKMAGIC;
			errstring = "unformatted/missing media";
			break;
		}
	}

	/*
	 * Create a default disk label based on scsi info.
	 * This will get overridden if there is a real label on the disk.
	 */
	if (errstring == NULL) {
		/* XXX we can open a device even without SDF_ALIVE */
		if (sc->sc_blksize == 0)
			sc->sc_blksize = DEV_BSIZE;

		/* Fill in info from disk geometry if it exists. */
		if (sc->sc_format_pid >= 0 && sc->sc_blks > 0 &&
		    sc->sc_heads > 0 && sc->sc_cyls > 0) {
			lp->d_secperunit = sc->sc_blks >> sc->sc_bshift;
			lp->d_ntracks = sc->sc_heads;
			lp->d_ncylinders = sc->sc_cyls;
			lp->d_nsectors = lp->d_secperunit / (lp->d_ntracks * lp->d_ncylinders);
		} else {
			lp->d_ntracks = 20;
			lp->d_ncylinders = 1;
			lp->d_nsectors = 32;
		}

		switch (sc->sc_type) {
		case 4:	
			strcpy(lp->d_typename, "SCSI WORM");
			break;
		case 5:
			strcpy(lp->d_typename, "SCSI CD-ROM");
			break;
		case 7:
			strcpy(lp->d_typename, "SCSI optical");
			break;
		default:
			strcpy(lp->d_typename, "SCSI disk");
			break;
		}
		lp->d_type = DTYPE_SCSI;
		strcpy(lp->d_packname, "fictitious");
		lp->d_secsize = sc->sc_blksize;
		lp->d_secpercyl = lp->d_nsectors * lp->d_ntracks;
		lp->d_rpm = 3600;
		lp->d_interleave = 1;

		/* XXX - these values for BBSIZE and SBSIZE assume ffs */
		lp->d_bbsize = BBSIZE;
		lp->d_sbsize = SBSIZE;

		lp->d_partitions[RAW_PART].p_offset = 0;
		lp->d_partitions[RAW_PART].p_size =
		    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
		lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
		lp->d_npartitions = RAW_PART + 1;

		lp->d_magic = DISKMAGIC;
		lp->d_magic2 = DISKMAGIC;
		lp->d_checksum = dkcksum(lp);

		errstring = readdisklabel(sdlabdev(dev), sdstrategy, lp, NULL);
	}

	if (errstring) {
		printf("%s: WARNING: %s, defining `c' partition as entire disk\n",
		    sc->sc_dev.dv_xname, errstring);
		/* XXX reset partition info as readdisklabel screws with it */
		lp->d_partitions[0].p_size = 0;
		lp->d_partitions[RAW_PART].p_offset = 0;
		lp->d_partitions[RAW_PART].p_size =
		    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
		lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
		lp->d_npartitions = RAW_PART + 1;
		lp->d_checksum = dkcksum(lp);
	}

	return(0);
}

int
sdopen(dev, flags, mode, p)
	dev_t dev;
	int flags, mode;
	struct proc *p;
{
	int unit = sdunit(dev);
	struct sd_softc *sc;
	int error, mask, part;

	if (unit >= sd_cd.cd_ndevs ||
	    (sc = sd_cd.cd_devs[unit]) == NULL ||
	    (sc->sc_flags & SDF_ALIVE) == 0)
		return (ENXIO);

	/*
	 * Wait for any pending opens/closes to complete
	 */
	while (sc->sc_flags & (SDF_OPENING|SDF_CLOSING))
		sleep((caddr_t)sc, PRIBIO);

	/*
	 * On first open, get label and partition info.
	 * We may block reading the label, so be careful
	 * to stop any other opens.
	 */
	if (sc->sc_dkdev.dk_openmask == 0) {
		sc->sc_flags |= SDF_OPENING;
		error = sdgetinfo(dev);
		sc->sc_flags &= ~SDF_OPENING;
		wakeup((caddr_t)sc);
		if (error)
			return(error);
	}

	part = sdpart(dev);
	mask = 1 << part;

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= sc->sc_dkdev.dk_label->d_npartitions ||
	     sc->sc_dkdev.dk_label->d_partitions[part].p_fstype == FS_UNUSED))
		return (ENXIO);

	/* Ensure only one open at a time. */
	switch (mode) {
	case S_IFCHR:
		sc->sc_dkdev.dk_copenmask |= mask;
		break;
	case S_IFBLK:
		sc->sc_dkdev.dk_bopenmask |= mask;
		break;
	}
	sc->sc_dkdev.dk_openmask =
	    sc->sc_dkdev.dk_copenmask | sc->sc_dkdev.dk_bopenmask;

	return(0);
}

int
sdclose(dev, flag, mode, p)
	dev_t dev;
	int flag, mode;
	struct proc *p;
{
	int unit = sdunit(dev);
	struct sd_softc *sc = sd_cd.cd_devs[unit];
	struct disk *dk = &sc->sc_dkdev;
	int mask, s;

	mask = 1 << sdpart(dev);
	if (mode == S_IFCHR)
		dk->dk_copenmask &= ~mask;
	else
		dk->dk_bopenmask &= ~mask;
	dk->dk_openmask = dk->dk_copenmask | dk->dk_bopenmask;
	/*
	 * On last close, we wait for all activity to cease since
	 * the label/parition info will become invalid.  Since we
	 * might sleep, we must block any opens while we are here.
	 * Note we don't have to about other closes since we know
	 * we are the last one.
	 */
	if (dk->dk_openmask == 0) {
		sc->sc_flags |= SDF_CLOSING;
		s = splbio();
		while (sc->sc_tab.b_active) {
			sc->sc_flags |= SDF_WANTED;
			sleep((caddr_t)&sc->sc_tab, PRIBIO);
		}
		splx(s);
		sc->sc_flags &= ~(SDF_CLOSING|SDF_WLABEL|SDF_ERROR);
		wakeup((caddr_t)sc);
	}
	sc->sc_format_pid = -1;
	return(0);
}

/*
 * This routine is called for partial block transfers and non-aligned
 * transfers (the latter only being possible on devices with a block size
 * larger than DEV_BSIZE).  The operation is performed in three steps
 * using a locally allocated buffer:
 *	1. transfer any initial partial block
 *	2. transfer full blocks
 *	3. transfer any final partial block
 */
static void
sdlblkstrat(bp, bsize)
	struct buf *bp;
	int bsize;
{
	struct sd_softc *sc = sd_cd.cd_devs[sdunit(bp->b_dev)];
	struct buf *cbp = (struct buf *)malloc(sizeof(struct buf),
							M_DEVBUF, M_WAITOK);
	caddr_t cbuf = (caddr_t)malloc(bsize, M_DEVBUF, M_WAITOK);
	int bn, resid;
	caddr_t addr;

	bzero((caddr_t)cbp, sizeof(*cbp));
	cbp->b_proc = curproc;		/* XXX */
	cbp->b_dev = bp->b_dev;
	bn = bp->b_blkno;
	resid = bp->b_bcount;
	addr = bp->b_un.b_addr;
#ifdef DEBUG
	if (sddebug & SDB_PARTIAL)
		printf("sdlblkstrat: bp %p flags %lx bn %x resid %x addr %p\n",
		       bp, bp->b_flags, bn, resid, addr);
#endif

	while (resid > 0) {
		int boff = dbtob(bn) & (bsize - 1);
		int count;

		if (boff || resid < bsize) {
			sc->sc_stats.sdpartials++;
			count = min(resid, bsize - boff);
			cbp->b_flags = B_BUSY | B_PHYS | B_READ;
			cbp->b_blkno = bn - btodb(boff);
			cbp->b_un.b_addr = cbuf;
			cbp->b_bcount = bsize;
#ifdef DEBUG
			if (sddebug & SDB_PARTIAL)
				printf(" readahead: bn %x cnt %x off %x addr %p\n",
				       cbp->b_blkno, count, boff, addr);
#endif
			sdstrategy(cbp);
			biowait(cbp);
			if (cbp->b_flags & B_ERROR) {
				bp->b_flags |= B_ERROR;
				bp->b_error = cbp->b_error;
				break;
			}
			if (bp->b_flags & B_READ) {
				bcopy(&cbuf[boff], addr, count);
				goto done;
			}
			bcopy(addr, &cbuf[boff], count);
#ifdef DEBUG
			if (sddebug & SDB_PARTIAL)
				printf(" writeback: bn %x cnt %x off %x addr %p\n",
				       cbp->b_blkno, count, boff, addr);
#endif
		} else {
			count = resid & ~(bsize - 1);
			cbp->b_blkno = bn;
			cbp->b_un.b_addr = addr;
			cbp->b_bcount = count;
#ifdef DEBUG
			if (sddebug & SDB_PARTIAL)
				printf(" fulltrans: bn %x cnt %x addr %p\n",
				       cbp->b_blkno, count, addr);
#endif
		}
		cbp->b_flags = B_BUSY | B_PHYS | (bp->b_flags & B_READ);
		sdstrategy(cbp);
		biowait(cbp);
		if (cbp->b_flags & B_ERROR) {
			bp->b_flags |= B_ERROR;
			bp->b_error = cbp->b_error;
			break;
		}
done:
		bn += btodb(count);
		resid -= count;
		addr += count;
#ifdef DEBUG
		if (sddebug & SDB_PARTIAL)
			printf(" done: bn %x resid %x addr %p\n",
			       bn, resid, addr);
#endif
	}
	free(cbuf, M_DEVBUF);
	free(cbp, M_DEVBUF);
}

void
sdstrategy(bp)
	struct buf *bp;
{
	int unit = sdunit(bp->b_dev);
	struct sd_softc *sc = sd_cd.cd_devs[unit];
	struct buf *dp = &sc->sc_tab;
	struct partition *pinfo;
	daddr_t bn;
	int sz, s;
	int offset;

	if (sc->sc_format_pid >= 0) {
		if (sc->sc_format_pid != curproc->p_pid) {	/* XXX */
			bp->b_error = EPERM;
			goto bad;
		}
		bp->b_cylin = 0;
	} else {
		if (sc->sc_flags & SDF_ERROR) {
			bp->b_error = EIO;
			goto bad;
		}
		bn = bp->b_blkno;
		sz = howmany(bp->b_bcount, DEV_BSIZE);
		pinfo = &sc->sc_dkdev.dk_label->d_partitions[sdpart(bp->b_dev)];

		/* Don't perform partition translation on RAW_PART. */
		offset = (sdpart(bp->b_dev) == RAW_PART) ? 0 : pinfo->p_offset;

		if (sdpart(bp->b_dev) != RAW_PART) {
			/*
			 * XXX This block of code belongs in
			 * XXX bounds_check_with_label()
			 */

			if (bn < 0 || bn + sz > pinfo->p_size) {
				sz = pinfo->p_size - bn;
				if (sz == 0) {
					bp->b_resid = bp->b_bcount;
					goto done;
				}
				if (sz < 0) {
					bp->b_error = EINVAL;
					goto bad;
				}
				bp->b_bcount = dbtob(sz);
			}
			/*
			 * Check for write to write protected label
			 */
			if (bn + offset <= LABELSECTOR &&
#if LABELSECTOR != 0
			    bn + offset + sz > LABELSECTOR &&
#endif
			    !(bp->b_flags & B_READ) &&
			    !(sc->sc_flags & SDF_WLABEL)) {
				bp->b_error = EROFS;
				goto bad;
			}
		}
		/*
		 * Non-aligned or partial-block transfers handled specially.
		 */
		s = sc->sc_blksize - 1;
		if ((dbtob(bn) & s) || (bp->b_bcount & s)) {
			sdlblkstrat(bp, sc->sc_blksize);
			goto done;
		}
		bp->b_cylin = (bn + offset) >> sc->sc_bshift;
	}
	s = splbio();
	disksort(dp, bp);
	if (dp->b_active == 0) {
		dp->b_active = 1;
		sdustart(unit);
	}
	splx(s);
	return;
bad:
	bp->b_flags |= B_ERROR;
done:
	biodone(bp);
}

void
sdustart(unit)
	int unit;
{
	struct sd_softc *sc = sd_cd.cd_devs[unit];

	if (scsireq(sc->sc_dev.dv_parent, &sc->sc_sq))
		sdstart(sc);
}

/*
 * Return:
 *	0	if not really an error
 *	<0	if we should do a retry
 *	>0	if a fatal error
 */
static int
sderror(sc, stat)
	struct sd_softc *sc;
	int stat;
{
	int cond = 1;

	sc->sc_sensestore.status = stat;
	if (stat & STS_CHECKCOND) {
		struct scsi_xsense *sp;

		scsi_request_sense(sc->sc_dev.dv_parent->dv_unit,
		    sc->sc_target, sc->sc_lun, sc->sc_sensestore.sense,
		    sizeof(sc->sc_sensestore.sense));
		sp = (struct scsi_xsense *)(sc->sc_sensestore.sense);
		printf("%s: scsi sense class %d, code %d", sc->sc_dev.dv_xname,
			sp->class, sp->code);
		if (sp->class == 7) {
			printf(", key %d", sp->key);
			if (sp->valid)
				printf(", blk %d", *(int *)&sp->info1);
			switch (sp->key) {
			/* no sense, try again */
			case 0:
				cond = -1;
				break;
			/* recovered error, not a problem */
			case 1:
				cond = 0;
				break;
			/* possible media change */
			case 6:
				/*
				 * For removable media, if we are doing the
				 * first open (i.e. reading the label) go
				 * ahead and retry, otherwise someone has
				 * changed the media out from under us and
				 * we should abort any further operations
				 * until a close is done.
				 */
				if (sc->sc_flags & SDF_RMEDIA) {
					if (sc->sc_flags & SDF_OPENING)
						cond = -1;
					else
						sc->sc_flags |= SDF_ERROR;
				}
				break;
			}
		}
		printf("\n");
	}
	return(cond);
}

static void
sdfinish(sc, bp)
	struct sd_softc *sc;
	struct buf *bp;
{
	struct buf *dp = &sc->sc_tab;

	dp->b_errcnt = 0;
	dp->b_actf = bp->b_actf;
	bp->b_resid = 0;
	biodone(bp);
	scsifree(sc->sc_dev.dv_parent, &sc->sc_sq);
	if (dp->b_actf)
		sdustart(sc->sc_dev.dv_unit);
	else {
		dp->b_active = 0;
		if (sc->sc_flags & SDF_WANTED) {
			sc->sc_flags &= ~SDF_WANTED;
			wakeup((caddr_t)dp);
		}
	}
}

void
sdstart(arg)
	void *arg;
{
	struct sd_softc *sc = arg;

	/*
	 * we have the SCSI bus -- in format mode, we may or may not need dma
	 * so check now.
	 */
	if (sc->sc_format_pid >= 0 && legal_cmds[sc->sc_cmdstore.cdb[0]] > 0) {
		struct buf *bp = sc->sc_tab.b_actf;
		int sts;

		sc->sc_tab.b_errcnt = 0;
		while (1) {
			sts = scsi_immed_command(sc->sc_dev.dv_parent->dv_unit,
			    sc->sc_target, sc->sc_lun, &sc->sc_cmdstore,
			    bp->b_un.b_addr, bp->b_bcount,
			    bp->b_flags & B_READ);
			sc->sc_sensestore.status = sts;
			if ((sts & 0xfe) == 0 ||
			    (sts = sderror(sc, sts)) == 0)
				break;
			if (sts > 0 || sc->sc_tab.b_errcnt++ >= SDRETRY) {
				bp->b_flags |= B_ERROR;
				bp->b_error = EIO;
				break;
			}
		}
		sdfinish(sc, bp);

	} else if (scsiustart(sc->sc_dev.dv_parent->dv_unit))
		sdgo(sc);
}

void
sdgo(arg)
	void *arg;
{
	struct sd_softc *sc = arg;
	struct buf *bp = sc->sc_tab.b_actf;
	int pad;
	struct scsi_fmt_cdb *cmd;

	if (sc->sc_format_pid >= 0) {
		cmd = &sc->sc_cmdstore;
		pad = 0;
	} else {
		/*
		 * Drive is in an error state, abort all operations
		 */
		if (sc->sc_flags & SDF_ERROR) {
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
			sdfinish(sc, bp);
			return;
		}
		cmd = bp->b_flags & B_READ? &sd_read_cmd : &sd_write_cmd;
		*(int *)(&cmd->cdb[2]) = bp->b_cylin;
		pad = howmany(bp->b_bcount, sc->sc_blksize);
		*(u_short *)(&cmd->cdb[7]) = pad;
		pad = (bp->b_bcount & (sc->sc_blksize - 1)) != 0;
#ifdef DEBUG
		if (pad)
			printf("%s: partial block xfer -- %lx bytes\n",
			       sc->sc_dev.dv_xname, bp->b_bcount);
#endif
		sc->sc_stats.sdtransfers++;
	}
#ifdef USELEDS
	ledcontrol(0, 0, LED_DISK);
#endif
	if (scsigo(sc->sc_dev.dv_parent->dv_unit, sc->sc_target, sc->sc_lun,
	    bp, cmd, pad) == 0) {
		/* Instrumentation. */
		disk_busy(&sc->sc_dkdev);
		sc->sc_dkdev.dk_seek++;		/* XXX */
		return;
	}
#ifdef DEBUG
	if (sddebug & SDB_ERROR)
		printf("%s: sdstart: %s adr %p blk %ld len %ld ecnt %ld\n",
		       sc->sc_dev.dv_xname,
		       bp->b_flags & B_READ? "read" : "write",
		       bp->b_un.b_addr, bp->b_cylin, bp->b_bcount,
		       sc->sc_tab.b_errcnt);
#endif
	bp->b_flags |= B_ERROR;
	bp->b_error = EIO;
	sdfinish(sc, bp);
}

void
sdintr(arg, stat)
	void *arg;
	int stat;
{
	struct sd_softc *sc = arg;
	struct buf *bp = sc->sc_tab.b_actf;
	int cond;
	
	if (bp == NULL) {
		printf("%s: bp == NULL\n", sc->sc_dev.dv_xname);
		return;
	}

	disk_unbusy(&sc->sc_dkdev, (bp->b_bcount - bp->b_resid));

	if (stat) {
#ifdef DEBUG
		if (sddebug & SDB_ERROR)
			printf("%s: sdintr: bad scsi status 0x%x\n",
				sc->sc_dev.dv_xname, stat);
#endif
		cond = sderror(sc, stat);
		if (cond) {
			if (cond < 0 && sc->sc_tab.b_errcnt++ < SDRETRY) {
#ifdef DEBUG
				if (sddebug & SDB_ERROR)
					printf("%s: retry #%ld\n",
					    sc->sc_dev.dv_xname,
					    sc->sc_tab.b_errcnt);
#endif
				sdstart(sc);
				return;
			}
			bp->b_flags |= B_ERROR;
			bp->b_error = EIO;
		}
	}
	sdfinish(sc, bp);
}

int
sdread(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = sdunit(dev);
	struct sd_softc *sc = sd_cd.cd_devs[unit];
	int pid;

	if ((pid = sc->sc_format_pid) >= 0 &&
	    pid != uio->uio_procp->p_pid)
		return (EPERM);
		
	return (physio(sdstrategy, NULL, dev, B_READ, minphys, uio));
}

int
sdwrite(dev, uio, flags)
	dev_t dev;
	struct uio *uio;
	int flags;
{
	int unit = sdunit(dev);
	struct sd_softc *sc = sd_cd.cd_devs[unit];
	int pid;

	if ((pid = sc->sc_format_pid) >= 0 &&
	    pid != uio->uio_procp->p_pid)
		return (EPERM);
		
	return (physio(sdstrategy, NULL, dev, B_WRITE, minphys, uio));
}

int
sdioctl(dev, cmd, data, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t data;
	int flag;
	struct proc *p;
{
	int unit = sdunit(dev);
	struct sd_softc *sc = sd_cd.cd_devs[unit];
	struct disklabel *lp = sc->sc_dkdev.dk_label;
	int error, flags;

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)data = *lp;
		return (0);

	case DIOCGPART:
		((struct partinfo *)data)->disklab = lp;
		((struct partinfo *)data)->part =
			&lp->d_partitions[sdpart(dev)];
		return (0);

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0)
			return (EBADF);
		if (*(int *)data)
			sc->sc_flags |= SDF_WLABEL;
		else
			sc->sc_flags &= ~SDF_WLABEL;
		return (0);

	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return (EBADF);
		error = setdisklabel(lp, (struct disklabel *)data,
				     (sc->sc_flags & SDF_WLABEL) ? 0
				     : sc->sc_dkdev.dk_openmask,
				     (struct cpu_disklabel *)0);
		return (error);

	case DIOCWDINFO:
		if ((flag & FWRITE) == 0)
			return (EBADF);
		error = setdisklabel(lp, (struct disklabel *)data,
				     (sc->sc_flags & SDF_WLABEL) ? 0
				     : sc->sc_dkdev.dk_openmask,
				     (struct cpu_disklabel *)0);
		if (error)
			return (error);
		flags = sc->sc_flags;
		sc->sc_flags = SDF_ALIVE | SDF_WLABEL;
		error = writedisklabel(sdlabdev(dev), sdstrategy, lp,
				      (struct cpu_disklabel *)0);
		sc->sc_flags = flags;
		return (error);

	case SDIOCSFORMAT:
		/* take this device into or out of "format" mode */
		if (suser(p->p_ucred, &p->p_acflag))
			return(EPERM);

		if (*(int *)data) {
			if (sc->sc_format_pid >= 0)
				return (EPERM);
			sc->sc_format_pid = p->p_pid;
		} else
			sc->sc_format_pid = -1;
		return (0);

	case SDIOCGFORMAT:
		/* find out who has the device in format mode */
		*(int *)data = sc->sc_format_pid;
		return (0);

	case SDIOCSCSICOMMAND:
		/*
		 * Save what user gave us as SCSI cdb to use with next
		 * read or write to the char device.
		 */
		if (sc->sc_format_pid != p->p_pid)
			return (EPERM);
		if (legal_cmds[((struct scsi_fmt_cdb *)data)->cdb[0]] == 0)
			return (EINVAL);
		bcopy(data, &sc->sc_cmdstore, sizeof(struct scsi_fmt_cdb));
		return (0);

	case SDIOCSENSE:
		/*
		 * return the SCSI sense data saved after the last
		 * operation that completed with "check condition" status.
		 */
		bcopy(&sc->sc_sensestore, data, sizeof(sc->sc_sensestore));
		return (0);

	default:
		return (EINVAL);
		
	}
	/*NOTREACHED*/
}

int
sdsize(dev)
	dev_t dev;
{
	int unit = sdunit(dev);
	struct sd_softc *sc = sd_cd.cd_devs[unit];
	int psize, didopen = 0;

	if (unit >= sd_cd.cd_ndevs ||
	    (sc = sd_cd.cd_devs[unit]) == NULL ||
	    (sc->sc_flags & SDF_ALIVE) == 0)
		return (-1);

	/*
	 * We get called very early on (via swapconf)
	 * without the device being open so we may need
	 * to handle it here.
	 */
	if (sc->sc_dkdev.dk_openmask == 0) {
		if (sdopen(dev, FREAD|FWRITE, S_IFBLK, NULL))
			return(-1);
		didopen = 1;
	}
	psize = sc->sc_dkdev.dk_label->d_partitions[sdpart(dev)].p_size *
	    (sc->sc_dkdev.dk_label->d_secsize / DEV_BSIZE);
	if (didopen)
		(void) sdclose(dev, FREAD|FWRITE, S_IFBLK, NULL);
	return (psize);
}

static int sddoingadump;	/* simple mutex */

/*
 * Non-interrupt driven, non-dma dump routine.
 */
int
sddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	int sectorsize;		/* size of a disk sector */
	int nsects;		/* number of sectors in partition */
	int sectoff;		/* sector offset of partition */
	int totwrt;		/* total number of sectors left to write */
	int nwrt;		/* current number of sectors to write */
	int unit, part;
	struct sd_softc *sc;
	struct disklabel *lp;
	char stat;

	/* Check for recursive dump; if so, punt. */
	if (sddoingadump)
		return (EFAULT);
	sddoingadump = 1;

	/* Decompose unit and partition. */
	unit = sdunit(dev);
	part = sdpart(dev);

	/* Make sure device is ok. */
	if (unit >= sd_cd.cd_ndevs ||
	    (sc = sd_cd.cd_devs[unit]) == NULL ||
	    (sc->sc_flags & SDF_ALIVE) == 0)
		return (ENXIO);

	/*
	 * Convert to disk sectors.  Request must be a multiple of size.
	 */
	lp = sc->sc_dkdev.dk_label;
	sectorsize = lp->d_secsize;
	if ((size % sectorsize) != 0)
		return (EFAULT);
	totwrt = size / sectorsize;
	blkno = dbtob(blkno) / sectorsize;	/* blkno in DEV_BSIZE units */

	nsects = lp->d_partitions[part].p_size;
	sectoff = lp->d_partitions[part].p_offset;

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || (blkno + totwrt) > nsects)
		return (EINVAL);

	/* Offset block number to start of partition. */
	blkno += sectoff;

	while (totwrt > 0) {
		nwrt = totwrt;		/* XXX */
#ifndef SD_DUMP_NOT_TRUSTED
		/*
		 * Send the data.  Note the `0' argument for bshift;
		 * we've done the necessary conversion above.
		 */
		stat = scsi_tt_write(sc->sc_dev.dv_parent->dv_unit,
		    sc->sc_target, sc->sc_lun, va, nwrt * sectorsize,
		    blkno, 0);
		if (stat) {
			printf("\nsddump: scsi write error 0x%x\n", stat);
			return (EIO);
		}
#else /* SD_DUMP_NOT_TRUSTED */
		/* Lets just talk about it first. */
		printf("%s: dump addr %p, blk %d\n", sc->sc_dev.dv_xname,
		    va, blkno);
		delay(500 * 1000);	/* half a second */
#endif /* SD_DUMP_NOT_TRUSTED */

		/* update block count */
		totwrt -= nwrt;
		blkno += nwrt;
		va += sectorsize * nwrt;
	}
	sddoingadump = 0;
	return (0);
}
