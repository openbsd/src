/*	$OpenBSD: sd.c,v 1.54 2002/08/12 06:59:46 fgsch Exp $	*/
/*	$NetBSD: sd.c,v 1.111 1997/04/02 02:29:41 mycroft Exp $	*/

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
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/scsiio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_disk.h>
#include <scsi/scsiconf.h>
#include <scsi/sdvar.h>

#include <ufs/ffs/fs.h>			/* for BBSIZE and SBSIZE */

#include <sys/vnode.h>

#define	SDUNIT(dev)			DISKUNIT(dev)
#define	SDMINOR(unit, part)		DISKMINOR(unit, part)
#define	SDPART(dev)			DISKPART(dev)
#define	MAKESDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define	SDLABELDEV(dev)	(MAKESDDEV(major(dev), SDUNIT(dev), RAW_PART))

int	sdmatch(struct device *, void *, void *);
void	sdattach(struct device *, struct device *, void *);
int	sdactivate(struct device *, enum devact);
int	sddetach(struct device *, int);
void	sdzeroref(struct device *);

void	sdminphys(struct buf *);
void	sdgetdisklabel(dev_t, struct sd_softc *, struct disklabel *,
			    struct cpu_disklabel *, int);
void	sdstart(void *);
void	sddone(struct scsi_xfer *);
void	sd_shutdown(void *);
int	sd_reassign_blocks(struct sd_softc *, u_long);
int	sd_interpret_sense(struct scsi_xfer *);

void	viscpy(u_char *, u_char *, int);

struct cfattach sd_ca = {
	sizeof(struct sd_softc), sdmatch, sdattach,
	sddetach, sdactivate, sdzeroref
};

struct cfdriver sd_cd = {
	NULL, "sd", DV_DISK
};

struct dkdriver sddkdriver = { sdstrategy };

struct scsi_device sd_switch = {
	sd_interpret_sense,	/* check out error handler first */
	sdstart,		/* have a queue, served by this */
	NULL,			/* have no async handler */
	sddone,			/* deal with stats at interrupt time */
};

struct scsi_inquiry_pattern sd_patterns[] = {
	{T_DIRECT, T_FIXED,
	 "",         "",                 ""},
	{T_DIRECT, T_REMOV,
	 "",         "",                 ""},
	{T_OPTICAL, T_FIXED,
	 "",         "",                 ""},
	{T_OPTICAL, T_REMOV,
	 "",         "",                 ""},
};

extern struct sd_ops sd_scsibus_ops;
extern struct sd_ops sd_atapibus_ops;

#define sdlock(softc)   disk_lock(&(softc)->sc_dk)
#define sdunlock(softc) disk_unlock(&(softc)->sc_dk)
#define sdlookup(unit) (struct sd_softc *)device_lookup(&sd_cd, (unit))

int
sdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct scsibus_attach_args *sa = aux;
	int priority;

	(void)scsi_inqmatch(sa->sa_inqbuf,
	    (caddr_t)sd_patterns, sizeof(sd_patterns)/sizeof(sd_patterns[0]),
	    sizeof(sd_patterns[0]), &priority);
	return (priority);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * a device suitable for this driver.
 */
void
sdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	int error, result;
	struct sd_softc *sd = (void *)self;
	struct disk_parms *dp = &sd->params;
	struct scsibus_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("sdattach: "));

	/*
	 * Store information needed to contact our base driver
	 */
	sd->sc_link = sc_link;
	sd->type = (sa->sa_inqbuf->device & SID_TYPE);
	sc_link->device = &sd_switch;
	sc_link->device_softc = sd;

	/*
	 * Initialize and attach the disk structure.
	 */
	sd->sc_dk.dk_driver = &sddkdriver;
	sd->sc_dk.dk_name = sd->sc_dev.dv_xname;
	disk_attach(&sd->sc_dk);

	dk_establish(&sd->sc_dk, &sd->sc_dev);

	if (sc_link->flags & SDEV_ATAPI &&
	    (sc_link->flags & SDEV_REMOVABLE)) {
		sd->sc_ops = &sd_atapibus_ops;
	} else {
		sd->sc_ops = &sd_scsibus_ops;
	}

	/*
	 * Note if this device is ancient.  This is used in sdminphys().
	 */
	if (!(sc_link->flags & SDEV_ATAPI) &&
	    (sa->sa_inqbuf->version & SID_ANSII) == 0)
		sd->flags |= SDF_ANCIENT;

	/*
	 * Use the subdriver to request information regarding
	 * the drive. We cannot use interrupts yet, so the
	 * request must specify this.
	 */
	printf("\n");

	if ((sd->sc_link->quirks & SDEV_NOSTARTUNIT) == 0) {
		error = scsi_start(sd->sc_link, SSS_START,
				   scsi_autoconf | SCSI_IGNORE_ILLEGAL_REQUEST |
				   SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT);
	} else
		error = 0;

	/* Fill in name struct for spoofed label */
	viscpy(sd->name.vendor, sa->sa_inqbuf->vendor, 8);
	viscpy(sd->name.product, sa->sa_inqbuf->product, 16);
	viscpy(sd->name.revision, sa->sa_inqbuf->revision, 4);

	if (error)
		result = SDGP_RESULT_OFFLINE;
	else
		result = (*sd->sc_ops->sdo_get_parms)(sd, &sd->params,
		    scsi_autoconf);

	printf("%s: ", sd->sc_dev.dv_xname);
	switch (result) {
	case SDGP_RESULT_OK:
		printf("%ldMB, %d cyl, %d head, %d sec, %d bytes/sec, %ld sec total",
		    dp->disksize / (1048576 / dp->blksize), dp->cyls,
		    dp->heads, dp->sectors, dp->blksize, dp->disksize);
		break;

	case SDGP_RESULT_OFFLINE:
		printf("drive offline");
		break;

	case SDGP_RESULT_UNFORMATTED:
		printf("unformatted media");
		break;

#ifdef DIAGNOSTIC
	default:
		panic("sdattach: unknown result from get_parms");
		break;
#endif
	}
	printf("\n");

	/*
	 * Establish a shutdown hook so that we can ensure that
	 * our data has actually made it onto the platter at
	 * shutdown time.  Note that this relies on the fact
	 * that the shutdown hook code puts us at the head of
	 * the list (thus guaranteeing that our hook runs before
	 * our ancestors').
	 */
	if ((sd->sc_sdhook =
	    shutdownhook_establish(sd_shutdown, sd)) == NULL)
		printf("%s: WARNING: unable to establish shutdown hook\n",
		    sd->sc_dev.dv_xname);
}

int
sdactivate(self, act)
	struct device *self;
	enum devact act;
{
	int rv = 0;

	switch (act) {
	case DVACT_ACTIVATE:
		break;

	case DVACT_DEACTIVATE:
		/*
		 * Nothing to do; we key off the device's DVF_ACTIVATE.
		 */
		break;
	}
	return (rv);
}


int
sddetach(self, flags)
	struct device *self;
	int flags;
{
	struct sd_softc *sc = (struct sd_softc *)self;
	struct buf *dp, *bp;
	int s, bmaj, cmaj, mn;

	/* Remove unprocessed buffers from queue */
	s = splbio();
	for (dp = &sc->buf_queue; (bp = dp->b_actf) != NULL; ) {
		dp->b_actf = bp->b_actf;

		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
	}
	splx(s);

	/* locate the major number */
	mn = SDMINOR(self->dv_unit, 0);

	for (bmaj = 0; bmaj < nblkdev; bmaj++)
		if (bdevsw[bmaj].d_open == sdopen)
			vdevgone(bmaj, mn, mn + MAXPARTITIONS - 1, VBLK);
	for (cmaj = 0; cmaj < nchrdev; cmaj++)
		if (cdevsw[cmaj].d_open == sdopen)
			vdevgone(cmaj, mn, mn + MAXPARTITIONS - 1, VCHR);

	/* Get rid of the shutdown hook. */
	if (sc->sc_sdhook != NULL)
		shutdownhook_disestablish(sc->sc_sdhook);

#if NRND > 0
	/* Unhook the entropy source. */
	rnd_detach_source(&sc->rnd_source);
#endif

	return (0);
}

void
sdzeroref(self)
	struct device *self;
{
	struct sd_softc *sd = (struct sd_softc *)self;

	/* Detach disk. */
	disk_detach(&sd->sc_dk);
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
int
sdopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct sd_softc *sd;
	struct scsi_link *sc_link;
	int unit, part;
	int error;

	unit = SDUNIT(dev);
	sd = sdlookup(unit);
	if (sd == NULL)
		return ENXIO;

	sc_link = sd->sc_link;
	part = SDPART(dev);

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("sdopen: dev=0x%x (unit %d (of %d), partition %d)\n", dev, unit,
	    sd_cd.cd_ndevs, part));

	if ((error = sdlock(sd)) != 0) {
		device_unref(&sd->sc_dev);
		return error;
	}

	if (sd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		/* Check that it is still responding and ok. */
		error = scsi_test_unit_ready(sc_link,
		    SCSI_IGNORE_ILLEGAL_REQUEST |
		    SCSI_IGNORE_MEDIA_CHANGE |
		    SCSI_IGNORE_NOT_READY);
		if (error)
			goto bad3;

		/* Start the pack spinning if necessary. */
		if ((sc_link->quirks & SDEV_NOSTARTUNIT) == 0) {
			error = scsi_start(sc_link, SSS_START,
			    SCSI_IGNORE_ILLEGAL_REQUEST |
			    SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT);
			if (error)
				goto bad3;
		}

		sc_link->flags |= SDEV_OPEN;

		/* Lock the pack in. */
		error = scsi_prevent(sc_link, PR_PREVENT,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE);
		if (error)
			goto bad;

		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			sc_link->flags |= SDEV_MEDIA_LOADED;

			/* Load the physical device parameters. */
			if ((*sd->sc_ops->sdo_get_parms)(sd, &sd->params,
			    0) == SDGP_RESULT_OFFLINE) {
				error = ENXIO;
				goto bad2;
			}
			SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded "));

			/* Load the partition info if not already loaded. */
			sdgetdisklabel(dev, sd, sd->sc_dk.dk_label,
			    sd->sc_dk.dk_cpulabel, 0);
			SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel loaded "));
		}
	}

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= sd->sc_dk.dk_label->d_npartitions ||
	    sd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		sd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		sd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	sd->sc_dk.dk_openmask = sd->sc_dk.dk_copenmask | sd->sc_dk.dk_bopenmask;

	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));
	sdunlock(sd);
	device_unref(&sd->sc_dev);
	return 0;

bad2:
	sc_link->flags &= ~SDEV_MEDIA_LOADED;

bad:
	if (sd->sc_dk.dk_openmask == 0) {
		scsi_prevent(sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE);
		sc_link->flags &= ~SDEV_OPEN;
	}

bad3:
	sdunlock(sd);
	device_unref(&sd->sc_dev);
	return error;
}

/*
 * close the device.. only called if we are the LAST occurence of an open
 * device.  Convenient now but usually a pain.
 */
int
sdclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct sd_softc *sd;
	int part = SDPART(dev);
	int error;

	sd = sdlookup(SDUNIT(dev));
	if (sd == NULL)
		return ENXIO;

	if ((error = sdlock(sd)) != 0)
		return error;

	switch (fmt) {
	case S_IFCHR:
		sd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		sd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	sd->sc_dk.dk_openmask = sd->sc_dk.dk_copenmask | sd->sc_dk.dk_bopenmask;

	if (sd->sc_dk.dk_openmask == 0) {
		if ((sd->flags & SDF_DIRTY) != 0 &&
		    sd->sc_ops->sdo_flush != NULL)
			(*sd->sc_ops->sdo_flush)(sd, 0);

		scsi_prevent(sd->sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY);
		sd->sc_link->flags &= ~(SDEV_OPEN|SDEV_MEDIA_LOADED);

		if (sd->sc_link->flags & SDEV_EJECTING) {
			scsi_start(sd->sc_link, SSS_STOP|SSS_LOEJ, 0);

			sd->sc_link->flags &= ~SDEV_EJECTING;
		}
	}

	sdunlock(sd);
	device_unref(&sd->sc_dev);
	return 0;
}

/*
 * Actually translate the requested transfer into one the physical driver
 * can understand.  The transfer is described by a buf and will include
 * only one physical transfer.
 */
void
sdstrategy(bp)
	struct buf *bp;
{
	struct sd_softc *sd;
	int s;

	sd = sdlookup(SDUNIT(bp->b_dev));
	if (sd == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}

	SC_DEBUG(sd->sc_link, SDEV_DB2, ("sdstrategy "));
	SC_DEBUG(sd->sc_link, SDEV_DB1,
	    ("%ld bytes @ blk %d\n", bp->b_bcount, bp->b_blkno));
	/*
	 * The transfer must be a whole number of blocks.
	 */
	if ((bp->b_bcount % sd->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/*
	 * If the device has been made invalid, error out
	 */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
		if (sd->sc_link->flags & SDEV_OPEN)
			bp->b_error = EIO;
		else
			bp->b_error = ENODEV;
		goto bad;
	}
	/*
	 * If it's a null transfer, return immediatly
	 */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (SDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, sd->sc_dk.dk_label,
	    sd->sc_dk.dk_cpulabel,
	    (sd->flags & (SDF_WLABEL|SDF_LABELLING)) != 0) <= 0)
		goto done;

	s = splbio();

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	disksort(&sd->buf_queue, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	sdstart(sd);

	splx(s);

	device_unref(&sd->sc_dev);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	s = splbio();
	biodone(bp);
	splx(s);
	if (sd != NULL)
		device_unref(&sd->sc_dev);
}

/*
 * sdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (sdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * sdstart() is called at splbio from sdstrategy and scsi_done
 */
void
sdstart(v)
	register void *v;
{
	register struct sd_softc *sd = v;
	register struct	scsi_link *sc_link = sd->sc_link;
	struct buf *bp = 0;
	struct buf *dp;
	struct scsi_rw_big cmd_big;
	struct scsi_rw cmd_small;
	struct scsi_generic *cmdp;
	int blkno, nblks, cmdlen, error;
	struct partition *p;

	SC_DEBUG(sc_link, SDEV_DB2, ("sdstart "));

	splassert(IPL_BIO);

	/*
	 * Check if the device has room for another command
	 */
	while (sc_link->openings > 0) {
		/*
		 * there is excess capacity, but a special waits
		 * It'll need the adapter as soon as we clear out of the
		 * way and let it run (user level wait).
		 */
		if (sc_link->flags & SDEV_WAITING) {
			sc_link->flags &= ~SDEV_WAITING;
			wakeup((caddr_t)sc_link);
			return;
		}

		/*
		 * See if there is a buf with work for us to do..
		 */
		dp = &sd->buf_queue;
		if ((bp = dp->b_actf) == NULL)	/* yes, an assign */
			return;
		dp->b_actf = bp->b_actf;

		/*
		 * If the device has become invalid, abort all the
		 * reads and writes until all files have been closed and
		 * re-opened
		 */
		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
			continue;
		}

		/*
		 * We have a buf, now we should make a command
		 *
		 * First, translate the block to absolute and put it in terms
		 * of the logical blocksize of the device.
		 */
		blkno =
		    bp->b_blkno / (sd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
		if (SDPART(bp->b_dev) != RAW_PART) {
			p = &sd->sc_dk.dk_label->d_partitions[SDPART(bp->b_dev)];
			blkno += p->p_offset;
		}
		nblks = howmany(bp->b_bcount, sd->sc_dk.dk_label->d_secsize);

		/*
		 *  Fill out the scsi command.  If the transfer will
		 *  fit in a "small" cdb, use it.
		 */
		if (!(sc_link->flags & SDEV_ATAPI) &&
		    !(sc_link->quirks & SDEV_NOCDB6) && 
		    ((blkno & 0x1fffff) == blkno) &&
		    ((nblks & 0xff) == nblks)) {
			/*
			 * We can fit in a small cdb.
			 */
			bzero(&cmd_small, sizeof(cmd_small));
			cmd_small.opcode = (bp->b_flags & B_READ) ?
			    READ_COMMAND : WRITE_COMMAND;
			_lto3b(blkno, cmd_small.addr);
			cmd_small.length = nblks & 0xff;
			cmdlen = sizeof(cmd_small);
			cmdp = (struct scsi_generic *)&cmd_small;
		} else {
			/*
			 * Need a large cdb.
			 */
			bzero(&cmd_big, sizeof(cmd_big));
			cmd_big.opcode = (bp->b_flags & B_READ) ?
			    READ_BIG : WRITE_BIG;
			_lto4b(blkno, cmd_big.addr);
			_lto2b(nblks, cmd_big.length);
			cmdlen = sizeof(cmd_big);
			cmdp = (struct scsi_generic *)&cmd_big;
		}

		/* Instrumentation. */
		disk_busy(&sd->sc_dk);

		/*
		 * Mark the disk dirty so that the cache will be
		 * flushed on close.
		 */
		if ((bp->b_flags & B_READ) == 0)
			sd->flags |= SDF_DIRTY;


		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		error = scsi_scsi_cmd(sc_link, cmdp, cmdlen,
		    (u_char *)bp->b_data, bp->b_bcount,
		    SDRETRIES, 60000, bp, SCSI_NOSLEEP |
		    ((bp->b_flags & B_READ) ? SCSI_DATA_IN : SCSI_DATA_OUT));
		if (error) {
			disk_unbusy(&sd->sc_dk, 0);
			printf("%s: not queued, error %d\n",
			    sd->sc_dev.dv_xname, error);
		}
	}
}

void
sddone(xs)
	struct scsi_xfer *xs;
{
	struct sd_softc *sd = xs->sc_link->device_softc;

	if (sd->flags & SDF_FLUSHING) {
		/* Flush completed, no longer dirty. */
		sd->flags &= ~(SDF_FLUSHING|SDF_DIRTY);
	}

	if (xs->bp != NULL)
		disk_unbusy(&sd->sc_dk, (xs->bp->b_bcount - xs->bp->b_resid));
}

void
sdminphys(bp)
	struct buf *bp;
{
	struct sd_softc *sd;
	long max;

	sd = sdlookup(SDUNIT(bp->b_dev));
	if (sd == NULL)
		return;  /* XXX - right way to fail this? */

	/*
	 * If the device is ancient, we want to make sure that
	 * the transfer fits into a 6-byte cdb.
	 *
	 * XXX Note that the SCSI-I spec says that 256-block transfers
	 * are allowed in a 6-byte read/write, and are specified
	 * by settng the "length" to 0.  However, we're conservative
	 * here, allowing only 255-block transfers in case an
	 * ancient device gets confused by length == 0.  A length of 0
	 * in a 10-byte read/write actually means 0 blocks.
	 */
	if (sd->flags & SDF_ANCIENT) {
		max = sd->sc_dk.dk_label->d_secsize * 0xff;

		if (bp->b_bcount > max)
			bp->b_bcount = max;
	}

	(*sd->sc_link->adapter->scsi_minphys)(bp);

	device_unref(&sd->sc_dev);
}

int
sdread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(sdstrategy, NULL, dev, B_READ, sdminphys, uio));
}

int
sdwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(sdstrategy, NULL, dev, B_WRITE, sdminphys, uio));
}

/*
 * Perform special action on behalf of the user
 * Knows about the internals of this device
 */
int
sdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct sd_softc *sd;
	int error = 0;
	int part = SDPART(dev);

	sd = sdlookup(SDUNIT(dev));
	if (sd == NULL)
		return ENXIO;

	SC_DEBUG(sd->sc_link, SDEV_DB2, ("sdioctl 0x%lx ", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
		switch (cmd) {
		case DIOCWLABEL:
		case DIOCLOCK:
		case DIOCEJECT:
		case SCIOCIDENTIFY:
		case OSCIOCIDENTIFY:
		case SCIOCCOMMAND:
		case SCIOCDEBUG:
			if (part == RAW_PART)
				break;
		/* FALLTHROUGH */
		default:
			if ((sd->sc_link->flags & SDEV_OPEN) == 0) {
				error = ENODEV;
				goto exit;
			} else {
				error = EIO;
				goto exit;
			}
		}
	}

	switch (cmd) {
	case DIOCRLDINFO:
		sdgetdisklabel(dev, sd, sd->sc_dk.dk_label,
		    sd->sc_dk.dk_cpulabel, 0);
		goto exit;
	case DIOCGPDINFO: {
			struct cpu_disklabel osdep;

			sdgetdisklabel(dev, sd, (struct disklabel *)addr,
			    &osdep, 1);
			goto exit;
		}

	case DIOCGDINFO:
		*(struct disklabel *)addr = *(sd->sc_dk.dk_label);
		goto exit;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = sd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &sd->sc_dk.dk_label->d_partitions[SDPART(dev)];
		goto exit;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			goto exit;
		}

		if ((error = sdlock(sd)) != 0)
			goto exit;
		sd->flags |= SDF_LABELLING;

		error = setdisklabel(sd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*sd->sc_dk.dk_openmask : */0,
		    sd->sc_dk.dk_cpulabel);
		if (error == 0) {
			if (cmd == DIOCWDINFO)
				error = writedisklabel(SDLABELDEV(dev),
				    sdstrategy, sd->sc_dk.dk_label,
				    sd->sc_dk.dk_cpulabel);
		}

		sd->flags &= ~SDF_LABELLING;
		sdunlock(sd);
		goto exit;

	case DIOCWLABEL:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			goto exit;
		}
		if (*(int *)addr)
			sd->flags |= SDF_WLABEL;
		else
			sd->flags &= ~SDF_WLABEL;
		goto exit;

	case DIOCLOCK:
		error = scsi_prevent(sd->sc_link,
		    (*(int *)addr) ? PR_PREVENT : PR_ALLOW, 0);
		goto exit;

	case MTIOCTOP:
		if (((struct mtop *)addr)->mt_op != MTOFFL) {
			error = EIO;
			goto exit;
		}
		/* FALLTHROUGH */
	case DIOCEJECT:
		if ((sd->sc_link->flags & SDEV_REMOVABLE) == 0) {
			error = ENOTTY;
			goto exit;
		}
		sd->sc_link->flags |= SDEV_EJECTING;
		goto exit;

	case SCIOCREASSIGN:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			goto exit;
		}
		error = sd_reassign_blocks(sd, (*(int *)addr));
		goto exit;

	default:
		if (part != RAW_PART) {
			error = ENOTTY;
			goto exit;
		}
		error = scsi_do_ioctl(sd->sc_link, dev, cmd, addr, flag, p);
	}

 exit:
	device_unref(&sd->sc_dev);
	return (error);
}

/*
 * Load the label information on the named device
 */
void
sdgetdisklabel(dev, sd, lp, clp, spoofonly)
	dev_t dev;
	struct sd_softc *sd;
	struct disklabel *lp;
	struct cpu_disklabel *clp;
	int spoofonly;
{
	char *errstring;

	bzero(lp, sizeof(struct disklabel));
	bzero(clp, sizeof(struct cpu_disklabel));

	lp->d_secsize = sd->params.blksize;
	lp->d_ntracks = sd->params.heads;
	lp->d_nsectors = sd->params.sectors;
	lp->d_ncylinders = sd->params.cyls;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	if (lp->d_secpercyl == 0) {
		lp->d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it */
	}

	lp->d_type = DTYPE_SCSI;
	if (sd->type == T_OPTICAL)
		strncpy(lp->d_typename, "SCSI optical",
		    sizeof(lp->d_typename) - 1);
	else
		strncpy(lp->d_typename, "SCSI disk",
		    sizeof(lp->d_typename) - 1);

	if (strlen(sd->name.vendor) + strlen(sd->name.product) + 1 <
	    sizeof(lp->d_packname))
		sprintf(lp->d_packname, "%s %s", sd->name.vendor,
		    sd->name.product);
	else
		strncpy(lp->d_packname, sd->name.product,
		    sizeof(lp->d_packname) - 1);

	lp->d_secperunit = sd->params.disksize;
	lp->d_rpm = 3600;
	lp->d_interleave = 1;
	lp->d_flags = 0;

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

	/*
	 * Call the generic disklabel extraction routine
	 */
	errstring = readdisklabel(SDLABELDEV(dev), sdstrategy, lp, clp,
	    spoofonly);
	if (errstring) {
		/*printf("%s: %s\n", sd->sc_dev.dv_xname, errstring);*/
		return;
	}
}


void
sd_shutdown(arg)
	void *arg;
{
	struct sd_softc *sd = arg;

	/*
	 * If the disk cache needs to be flushed, and the disk supports
	 * it, flush it.  We're cold at this point, so we poll for
	 * completion.
	 */
	if ((sd->flags & SDF_DIRTY) != 0 && sd->sc_ops->sdo_flush != NULL)
		(*sd->sc_ops->sdo_flush)(sd, SCSI_AUTOCONF);
}

/*
 * Tell the device to map out a defective block
 */
int
sd_reassign_blocks(sd, blkno)
	struct sd_softc *sd;
	u_long blkno;
{
	struct scsi_reassign_blocks scsi_cmd;
	struct scsi_reassign_blocks_data rbdata;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(&rbdata, sizeof(rbdata));
	scsi_cmd.opcode = REASSIGN_BLOCKS;

	_lto2b(sizeof(rbdata.defect_descriptor[0]), rbdata.length);
	_lto4b(blkno, rbdata.defect_descriptor[0].dlbaddr);

	return scsi_scsi_cmd(sd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)&rbdata, sizeof(rbdata), SDRETRIES,
	    5000, NULL, SCSI_DATA_OUT);
}

/*
 * Check Errors
 */
int
sd_interpret_sense(xs)
	struct scsi_xfer *xs;
{
	struct scsi_link *sc_link = xs->sc_link;
	struct scsi_sense_data *sense = &xs->sense;
	struct sd_softc *sd = sc_link->device_softc;
	int retval = SCSIRET_CONTINUE;

	/*
	 * If the device is not open yet, let the generic code handle it.
	 */
	if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
		return (retval);
	}

	/*
	 * If it isn't a extended or extended/deferred error, let
	 * the generic code handle it.
	 */
	if ((sense->error_code & SSD_ERRCODE) != 0x70 &&
	    (sense->error_code & SSD_ERRCODE) != 0x71) {	/* DEFFERRED */
		return (retval);
	}

	if ((sense->flags & SSD_KEY) == SKEY_NOT_READY &&
	    sense->add_sense_code == 0x4) {
		if (sense->add_sense_code_qual == 0x01)	{
			printf("%s: ..is spinning up...waiting\n",
			    sd->sc_dev.dv_xname);
			/*
			 * I really need a sdrestart function I can call here.
			 */
			delay(1000000 * 5);	/* 5 seconds */
			retval = SCSIRET_RETRY;
		} else if ((sense->add_sense_code_qual == 0x2) &&
		    (sd->sc_link->quirks & SDEV_NOSTARTUNIT) == 0) {
			if (sd->sc_link->flags & SDEV_REMOVABLE) {
				printf(
				"%s: removable disk stopped - not restarting\n",
				    sd->sc_dev.dv_xname);
				retval = EIO;
			} else {
				printf("%s: respinning up disk\n",
				    sd->sc_dev.dv_xname);
				retval = scsi_start(sd->sc_link, SSS_START,
				    SCSI_URGENT | SCSI_NOSLEEP);
				if (retval != 0) {
					printf(
					    "%s: respin of disk failed - %d\n",
					    sd->sc_dev.dv_xname, retval);
					retval = EIO;
				} else {
					retval = SCSIRET_RETRY;
				}
			}
		}
	}
	return (retval);
}

int
sdsize(dev)
	dev_t dev;
{
	struct sd_softc *sd;
	int part, omask;
	int size;

	sd = sdlookup(SDUNIT(dev));
	if (sd == NULL)
		return -1;

	part = SDPART(dev);
	omask = sd->sc_dk.dk_openmask & (1 << part);

	if (omask == 0 && sdopen(dev, 0, S_IFBLK, NULL) != 0) {
		size = -1;
		goto exit;
	}
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) == 0)
		size = -1;
	else if (sd->sc_dk.dk_label->d_partitions[part].p_fstype != FS_SWAP)
		size = -1;
	else
		size = sd->sc_dk.dk_label->d_partitions[part].p_size *
			(sd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
	if (omask == 0 && sdclose(dev, 0, S_IFBLK, NULL) != 0)
		size = -1;

 exit:
	device_unref(&sd->sc_dev);
	return size;
}

/* #define SD_DUMP_NOT_TRUSTED if you just want to watch */
static struct scsi_xfer sx;
static int sddoingadump;

/*
 * dump all of physical memory into the partition specified, starting
 * at offset 'dumplo' into the partition.
 */
int
sddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	struct sd_softc *sd;	/* disk unit to do the I/O */
	struct disklabel *lp;	/* disk's disklabel */
	int	unit, part;
	int	sectorsize;	/* size of a disk sector */
	int	nsects;		/* number of sectors in partition */
	int	sectoff;	/* sector offset of partition */
	int	totwrt;		/* total number of sectors left to write */
	int	nwrt;		/* current number of sectors to write */
	struct scsi_rw_big cmd;	/* write command */
	struct scsi_xfer *xs;	/* ... convenience */
	int	retval;

	/* Check if recursive dump; if so, punt. */
	if (sddoingadump)
		return EFAULT;

	/* Mark as active early. */
	sddoingadump = 1;

	unit = SDUNIT(dev);	/* Decompose unit & partition. */
	part = SDPART(dev);

	/* Check for acceptable drive number. */
	if (unit >= sd_cd.cd_ndevs || (sd = sd_cd.cd_devs[unit]) == NULL)
		return ENXIO;

	/*
	 * XXX Can't do this check, since the media might have been
	 * XXX marked `invalid' by successful unmounting of all
	 * XXX filesystems.
	 */
#if 0
	/* Make sure it was initialized. */
	if ((sd->sc_link->flags & SDEV_MEDIA_LOADED) != SDEV_MEDIA_LOADED)
		return ENXIO;
#endif

	/* Convert to disk sectors.  Request must be a multiple of size. */
	lp = sd->sc_dk.dk_label;
	sectorsize = lp->d_secsize;
	if ((size % sectorsize) != 0)
		return EFAULT;
	totwrt = size / sectorsize;
	blkno = dbtob(blkno) / sectorsize;	/* blkno in DEV_BSIZE units */

	nsects = lp->d_partitions[part].p_size;
	sectoff = lp->d_partitions[part].p_offset;

	/* Check transfer bounds against partition size. */
	if ((blkno < 0) || ((blkno + totwrt) > nsects))
		return EINVAL;

	/* Offset block number to start of partition. */
	blkno += sectoff;

	xs = &sx;

	while (totwrt > 0) {
		nwrt = totwrt;		/* XXX */
#ifndef	SD_DUMP_NOT_TRUSTED
		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.opcode = WRITE_BIG;
		_lto4b(blkno, cmd.addr);
		_lto2b(nwrt, cmd.length);
		/*
		 * Fill out the scsi_xfer structure
		 *    Note: we cannot sleep as we may be an interrupt
		 * don't use scsi_scsi_cmd() as it may want
		 * to wait for an xs.
		 */
		bzero(xs, sizeof(sx));
		xs->flags |= SCSI_AUTOCONF | SCSI_DATA_OUT;
		xs->sc_link = sd->sc_link;
		xs->retries = SDRETRIES;
		xs->timeout = 10000;	/* 10000 millisecs for a disk ! */
		xs->cmd = (struct scsi_generic *)&cmd;
		xs->cmdlen = sizeof(cmd);
		xs->resid = nwrt * sectorsize;
		xs->error = XS_NOERROR;
		xs->bp = 0;
		xs->data = va;
		xs->datalen = nwrt * sectorsize;

		/*
		 * Pass all this info to the scsi driver.
		 */
		retval = (*(sd->sc_link->adapter->scsi_cmd)) (xs);
		if (retval != COMPLETE)
			return ENXIO;
#else	/* SD_DUMP_NOT_TRUSTED */
		/* Let's just talk about this first... */
		printf("sd%d: dump addr 0x%x, blk %d\n", unit, va, blkno);
		delay(500 * 1000);	/* half a second */
#endif	/* SD_DUMP_NOT_TRUSTED */

		/* update block count */
		totwrt -= nwrt;
		blkno += nwrt;
		va += sectorsize * nwrt;
	}
	sddoingadump = 0;
	return 0;
}

/*
 * Copy up to len chars from src to dst, ignoring non-printables.
 * Must be room for len+1 chars in dst so we can write the NUL.
 * Does not assume src is NUL-terminated.
 */
void
viscpy(dst, src, len)
	u_char *dst;
	u_char *src;
	int len;
{
	while (len > 0 && *src != '\0') {
		if (*src < 0x20 || *src >= 0x80) {
			src++;
			continue;
		}
		*dst++ = *src++;
		len--;
	}
	*dst = '\0';
}
