/*	$OpenBSD: cd.c,v 1.133 2007/09/07 16:15:48 krw Exp $	*/
/*	$NetBSD: cd.c,v 1.100 1997/04/02 02:29:30 mycroft Exp $	*/

/*
 * Copyright (c) 1994, 1995, 1997 Charles M. Hannum.  All rights reserved.
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

/*
 * Originally written by Julian Elischer (julian@tfs.com)
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
 * Ported to run under 386BSD by Julian Elischer (julian@tfs.com) Sept 1992
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
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
#include <sys/cdio.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/scsiio.h>
#include <sys/vnode.h>

#include <scsi/scsi_all.h>
#include <scsi/cd.h>
#include <scsi/scsi_disk.h>	/* rw_big and start_stop come from there */
#include <scsi/scsiconf.h>


#include <ufs/ffs/fs.h>			/* for BBSIZE and SBSIZE */

#define	CDOUTSTANDING	4

#define MAXTRACK	99
#define CD_BLOCK_OFFSET	150
#define CD_FRAMES	75
#define CD_SECS		60

struct cd_toc {
	struct ioc_toc_header header;
	struct cd_toc_entry entries[MAXTRACK+1]; /* One extra for the */
						 /* leadout */
};

int	cdmatch(struct device *, void *, void *);
void	cdattach(struct device *, struct device *, void *);
int	cdactivate(struct device *, enum devact);
int	cddetach(struct device *, int);

void	cdstart(void *);
void	cdrestart(void *);
void	cdminphys(struct buf *);
void	cdgetdisklabel(dev_t, struct cd_softc *, struct disklabel *, int);
void	cddone(struct scsi_xfer *);
u_long	cd_size(struct cd_softc *, int);
void	cd_kill_buffers(struct cd_softc *);
void	lba2msf(u_long, u_char *, u_char *, u_char *);
u_long	msf2lba(u_char, u_char, u_char);
int	cd_setchan(struct cd_softc *, int, int, int, int, int);
int	cd_getvol(struct cd_softc *cd, struct ioc_vol *, int);
int	cd_setvol(struct cd_softc *, const struct ioc_vol *, int);
int	cd_load_unload(struct cd_softc *, int, int);
int	cd_set_pa_immed(struct cd_softc *, int);
int	cd_play(struct cd_softc *, int, int);
int	cd_play_tracks(struct cd_softc *, int, int, int, int);
int	cd_play_msf(struct cd_softc *, int, int, int, int, int, int);
int	cd_pause(struct cd_softc *, int);
int	cd_reset(struct cd_softc *);
int	cd_read_subchannel(struct cd_softc *, int, int, int,
	    struct cd_sub_channel_info *, int );
int	cd_read_toc(struct cd_softc *, int, int, void *, int, int);
int	cd_get_parms(struct cd_softc *, int);
int	cd_load_toc(struct cd_softc *, struct cd_toc *, int);
int	cd_interpret_sense(struct scsi_xfer *);

int    dvd_auth(struct cd_softc *, union dvd_authinfo *);
int    dvd_read_physical(struct cd_softc *, union dvd_struct *);
int    dvd_read_copyright(struct cd_softc *, union dvd_struct *);
int    dvd_read_disckey(struct cd_softc *, union dvd_struct *);
int    dvd_read_bca(struct cd_softc *, union dvd_struct *);
int    dvd_read_manufact(struct cd_softc *, union dvd_struct *);
int    dvd_read_struct(struct cd_softc *, union dvd_struct *);

void	cd_powerhook(int why, void *arg);

struct cfattach cd_ca = {
	sizeof(struct cd_softc), cdmatch, cdattach,
	cddetach, cdactivate
};

struct cfdriver cd_cd = {
	NULL, "cd", DV_DISK
};

struct dkdriver cddkdriver = { cdstrategy };

struct scsi_device cd_switch = {
	cd_interpret_sense,
	cdstart,		/* we have a queue, which is started by this */
	NULL,			/* we do not have an async handler */
	cddone,			/* deal with stats at interrupt time */
};

const struct scsi_inquiry_pattern cd_patterns[] = {
	{T_CDROM, T_REMOV,
	 "",         "",                 ""},
	{T_WORM, T_REMOV,
	 "",         "",                 ""},
	{T_DIRECT, T_REMOV,
	 "NEC                 CD-ROM DRIVE:260", "", ""},
#if 0
	{T_CDROM, T_REMOV, /* more luns */
	 "PIONEER ", "CD-ROM DRM-600  ", ""},
#endif
};

#define cdlock(softc)   disk_lock(&(softc)->sc_dk)
#define cdunlock(softc) disk_unlock(&(softc)->sc_dk)
#define cdlookup(unit) (struct cd_softc *)device_lookup(&cd_cd, (unit))

int
cdmatch(struct device *parent, void *match, void *aux)
{
	struct scsi_attach_args *sa = aux;
	int priority;

	scsi_inqmatch(sa->sa_inqbuf, cd_patterns,
	    sizeof(cd_patterns)/sizeof(cd_patterns[0]), sizeof(cd_patterns[0]),
	    &priority);
	return (priority);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 */
void
cdattach(struct device *parent, struct device *self, void *aux)
{
	struct scsi_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;
	struct cd_softc *cd = (struct cd_softc *)self;

	SC_DEBUG(sc_link, SDEV_DB2, ("cdattach:\n"));

	/*
	 * Store information needed to contact our base driver
	 */
	cd->sc_link = sc_link;
	sc_link->device = &cd_switch;
	sc_link->device_softc = cd;
	if (sc_link->openings > CDOUTSTANDING)
		sc_link->openings = CDOUTSTANDING;

	/*
	 * Initialize and attach the disk structure.
	 */
  	cd->sc_dk.dk_driver = &cddkdriver;
	cd->sc_dk.dk_name = cd->sc_dev.dv_xname;
	disk_attach(&cd->sc_dk);

	/*
	 * Note if this device is ancient.  This is used in cdminphys().
	 */
	if (!(sc_link->flags & SDEV_ATAPI) &&
	    SCSISPC(sa->sa_inqbuf->version) == 0)
		cd->flags |= CDF_ANCIENT;

	printf("\n");

	timeout_set(&cd->sc_timeout, cdrestart, cd);

	if ((cd->sc_cdpwrhook = powerhook_establish(cd_powerhook, cd)) == NULL)
		printf("%s: WARNING: unable to establish power hook\n",
		    cd->sc_dev.dv_xname);
}


int
cdactivate(struct device *self, enum devact act)
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
cddetach(struct device *self, int flags)
{
	struct cd_softc *cd = (struct cd_softc *)self;
	int bmaj, cmaj, mn;

	cd_kill_buffers(cd);

	/* Locate the lowest minor number to be detached. */
	mn = DISKMINOR(self->dv_unit, 0);

	for (bmaj = 0; bmaj < nblkdev; bmaj++)
		if (bdevsw[bmaj].d_open == cdopen)
			vdevgone(bmaj, mn, mn + MAXPARTITIONS - 1, VBLK);
	for (cmaj = 0; cmaj < nchrdev; cmaj++)
		if (cdevsw[cmaj].d_open == cdopen)
			vdevgone(cmaj, mn, mn + MAXPARTITIONS - 1, VCHR);

	/* Get rid of the power hook. */
	if (cd->sc_cdpwrhook != NULL)
		powerhook_disestablish(cd->sc_cdpwrhook);

	/* Detach disk. */
	disk_detach(&cd->sc_dk);

	return (0);
}

/*
 * Open the device. Make sure the partition info is as up-to-date as can be.
 */
int
cdopen(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct scsi_link *sc_link;
	struct cd_softc *cd;
	int error = 0, part, rawopen, unit;

	unit = DISKUNIT(dev);
	part = DISKPART(dev);

	rawopen = (part == RAW_PART) && (fmt == S_IFCHR);

	cd = cdlookup(unit);
	if (cd == NULL)
		return (ENXIO);

	sc_link = cd->sc_link;
	SC_DEBUG(sc_link, SDEV_DB1,
	    ("cdopen: dev=0x%x (unit %d (of %d), partition %d)\n", dev, unit,
	    cd_cd.cd_ndevs, part));

	if ((error = cdlock(cd)) != 0) {
		device_unref(&cd->sc_dev);
		return (error);
	}

	if (cd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			if (rawopen)
				goto out;
			error = EIO;
			goto bad;
		}
	} else {
		/*
		 * Check that it is still responding and ok.  Drive can be in
		 * progress of loading media so use increased retries number
		 * and don't ignore NOT_READY.
		 */

		/* Use cd_interpret_sense() now. */
		sc_link->flags |= SDEV_OPEN;

		error = scsi_test_unit_ready(sc_link, TEST_READY_RETRIES,
		    (rawopen ? SCSI_SILENT : 0) | SCSI_IGNORE_ILLEGAL_REQUEST |
		    SCSI_IGNORE_MEDIA_CHANGE);

		/* Start the cd spinning if necessary. */
		if (error == EIO)
			error = scsi_start(sc_link, SSS_START,
			    SCSI_IGNORE_ILLEGAL_REQUEST |
			    SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT);

		if (error) {
			if (rawopen) {
				error = 0;
				goto out;
			} else
				goto bad;
		}

		/* Lock the cd in. */
		error = scsi_prevent(sc_link, PR_PREVENT,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE);
		if (error)
			goto bad;

		/* Load the physical device parameters. */
		sc_link->flags |= SDEV_MEDIA_LOADED;
		if (cd_get_parms(cd, 0) != 0) {
			sc_link->flags &= ~SDEV_MEDIA_LOADED;
			error = ENXIO;
			goto bad;
		}
		SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded\n"));

		/* Fabricate a disk label. */
		cdgetdisklabel(dev, cd, cd->sc_dk.dk_label, 0);
		SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel fabricated\n"));
	}

	/* Check that the partition exists. */
	if (part != RAW_PART && (part >= cd->sc_dk.dk_label->d_npartitions ||
	    cd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

out:	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		cd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		cd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	cd->sc_dk.dk_openmask = cd->sc_dk.dk_copenmask | cd->sc_dk.dk_bopenmask;
	sc_link->flags |= SDEV_OPEN;
	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));

	/* It's OK to fall through because dk_openmask is now non-zero. */
bad:
	if (cd->sc_dk.dk_openmask == 0) {
		scsi_prevent(sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE);
		sc_link->flags &= ~(SDEV_OPEN | SDEV_MEDIA_LOADED);
	}

	cdunlock(cd);
	device_unref(&cd->sc_dev);
	return (error);
}

/*
 * Close the device. Only called if we are the last occurrence of an open
 * device.
 */
int
cdclose(dev_t dev, int flag, int fmt, struct proc *p)
{
	struct cd_softc *cd;
	int part = DISKPART(dev);
	int error;

	cd = cdlookup(DISKUNIT(dev));
	if (cd == NULL)
		return ENXIO;

	if ((error = cdlock(cd)) != 0) {
		device_unref(&cd->sc_dev);
		return error;
	}

	switch (fmt) {
	case S_IFCHR:
		cd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		cd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	cd->sc_dk.dk_openmask = cd->sc_dk.dk_copenmask | cd->sc_dk.dk_bopenmask;

	if (cd->sc_dk.dk_openmask == 0) {
		/* XXXX Must wait for I/O to complete! */

		scsi_prevent(cd->sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY);
		cd->sc_link->flags &= ~(SDEV_OPEN | SDEV_MEDIA_LOADED);

		if (cd->sc_link->flags & SDEV_EJECTING) {
			scsi_start(cd->sc_link, SSS_STOP|SSS_LOEJ, 0);

			cd->sc_link->flags &= ~SDEV_EJECTING;
		}

		timeout_del(&cd->sc_timeout);
	}

	cdunlock(cd);

	device_unref(&cd->sc_dev);
	return 0;
}

/*
 * Actually translate the requested transfer into one the physical driver can
 * understand.  The transfer is described by a buf and will include only one
 * physical transfer.
 */
void
cdstrategy(struct buf *bp)
{
	struct cd_softc *cd;
	int s;

	if ((cd = cdlookup(DISKUNIT(bp->b_dev))) == NULL) {
		bp->b_error = ENXIO;
		goto bad;
	}

	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdstrategy: %ld bytes @ blk %d\n",
	    bp->b_bcount, bp->b_blkno));
	/*
	 * If the device has been made invalid, error out
	 * maybe the media changed, or no media loaded
	 */
	if ((cd->sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
		bp->b_error = EIO;
		goto bad;
	}
	/*
	 * The transfer must be a whole number of blocks.
	 */
	if ((bp->b_bcount % cd->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/*
	 * If it's a null transfer, return immediately
	 */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (DISKPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, cd->sc_dk.dk_label,
	    (cd->flags & (CDF_WLABEL|CDF_LABELLING)) != 0) <= 0)
		goto done;

	s = splbio();

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	disksort(&cd->buf_queue, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	cdstart(cd);

	device_unref(&cd->sc_dev);
	splx(s);
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
	if (cd != NULL)
		device_unref(&cd->sc_dev);
}

/*
 * cdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It deques the buf and creates a scsi command to perform the
 * transfer in the buf. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (cdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * cdstart() is called at splbio from cdstrategy, cdrestart and scsi_done
 */
void
cdstart(void *v)
{
	struct cd_softc *cd = v;
	struct scsi_link *sc_link = cd->sc_link;
	struct buf *bp = 0;
	struct buf *dp;
	struct scsi_rw_big cmd_big;
	struct scsi_rw cmd_small;
	struct scsi_generic *cmdp;
	int blkno, nblks, cmdlen, error;
	struct partition *p;

	splassert(IPL_BIO);

	SC_DEBUG(sc_link, SDEV_DB2, ("cdstart\n"));
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
		dp = &cd->buf_queue;
		if ((bp = dp->b_actf) == NULL)	/* yes, an assign */
			return;
		dp->b_actf = bp->b_actf;

		/*
		 * If the deivce has become invalid, abort all the
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
		    bp->b_blkno / (cd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
		p = &cd->sc_dk.dk_label->d_partitions[DISKPART(bp->b_dev)];
		blkno += DL_GETPOFFSET(p);
		nblks = howmany(bp->b_bcount, cd->sc_dk.dk_label->d_secsize);

		/*
		 *  Fill out the scsi command.  If the transfer will
		 *  fit in a "small" cdb, use it.
		 */
		if (!(sc_link->flags & SDEV_ATAPI) &&
		    !(sc_link->quirks & SDEV_ONLYBIG) && 
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
		disk_busy(&cd->sc_dk);

		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		error = scsi_scsi_cmd(sc_link, cmdp, cmdlen,
		    (u_char *) bp->b_data, bp->b_bcount, CDRETRIES, 30000, bp,
		    SCSI_NOSLEEP | ((bp->b_flags & B_READ) ? SCSI_DATA_IN :
		    SCSI_DATA_OUT));
		switch (error) {
		case 0:
			timeout_del(&cd->sc_timeout);
			break;
		case EAGAIN:
			/*
			 * The device can't start another i/o. Try again later.
			 */
			dp->b_actf = bp;
			disk_unbusy(&cd->sc_dk, 0, 0);
			timeout_add(&cd->sc_timeout, 1);
			return;
		default:
			disk_unbusy(&cd->sc_dk, 0, 0);
			printf("%s: not queued, error %d\n",
			    cd->sc_dev.dv_xname, error);
			break;
		}
	}
}

void
cdrestart(void *v)
{
	int s;

	s = splbio();
	cdstart(v);
	splx(s);
}

void
cddone(struct scsi_xfer *xs)
{
	struct cd_softc *cd = xs->sc_link->device_softc;

	if (xs->bp != NULL)
		disk_unbusy(&cd->sc_dk, xs->bp->b_bcount - xs->bp->b_resid,
		    (xs->bp->b_flags & B_READ));
}

void
cdminphys(struct buf *bp)
{
	struct cd_softc *cd;
	long max;

	cd = cdlookup(DISKUNIT(bp->b_dev));
	if (cd == NULL)
		return;

	/*
	 * If the device is ancient, we want to make sure that
	 * the transfer fits into a 6-byte cdb.
	 *
	 * XXX Note that the SCSI-I spec says that 256-block transfers
	 * are allowed in a 6-byte read/write, and are specified
	 * by setting the "length" to 0.  However, we're conservative
	 * here, allowing only 255-block transfers in case an
	 * ancient device gets confused by length == 0.  A length of 0
	 * in a 10-byte read/write actually means 0 blocks.
	 */
	if (cd->flags & CDF_ANCIENT) {
		max = cd->sc_dk.dk_label->d_secsize * 0xff;

		if (bp->b_bcount > max)
			bp->b_bcount = max;
	}

	(*cd->sc_link->adapter->scsi_minphys)(bp);

	device_unref(&cd->sc_dev);
}

int
cdread(dev_t dev, struct uio *uio, int ioflag)
{

	return (physio(cdstrategy, NULL, dev, B_READ, cdminphys, uio));
}

int
cdwrite(dev_t dev, struct uio *uio, int ioflag)
{

	return (physio(cdstrategy, NULL, dev, B_WRITE, cdminphys, uio));
}

/*
 * conversion between minute-seconde-frame and logical block address
 * addresses format
 */
void
lba2msf (lba, m, s, f)
	u_long lba;
	u_char *m, *s, *f;
{
	u_long tmp;

	tmp = lba + CD_BLOCK_OFFSET;	/* offset of first logical frame */
	tmp &= 0xffffff;		/* negative lbas use only 24 bits */
	*m = tmp / (CD_SECS * CD_FRAMES);
	tmp %= (CD_SECS * CD_FRAMES);
	*s = tmp / CD_FRAMES;
	*f = tmp % CD_FRAMES;
}

u_long
msf2lba (m, s, f)
	u_char m, s, f;
{

	return ((((m * CD_SECS) + s) * CD_FRAMES + f) - CD_BLOCK_OFFSET);
}


/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
int
cdioctl(dev_t dev, u_long cmd, caddr_t addr, int flag, struct proc *p)
{
	struct cd_softc *cd;
	struct disklabel *lp;
	int part = DISKPART(dev);
	int error = 0;

	cd = cdlookup(DISKUNIT(dev));
	if (cd == NULL)
		return ENXIO;

	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdioctl 0x%lx\n", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if ((cd->sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
		switch (cmd) {
		case DIOCWLABEL:
		case DIOCLOCK:
		case DIOCEJECT:
		case SCIOCIDENTIFY:
		case SCIOCCOMMAND:
		case SCIOCDEBUG:
		case CDIOCLOADUNLOAD:
		case SCIOCRESET:
		case CDIOCGETVOL:
		case CDIOCSETVOL:
		case CDIOCSETMONO:
		case CDIOCSETSTEREO:
		case CDIOCSETMUTE:
		case CDIOCSETLEFT:
		case CDIOCSETRIGHT:
		case CDIOCCLOSE:
		case CDIOCEJECT:
		case CDIOCALLOW:
		case CDIOCPREVENT:
		case CDIOCSETDEBUG:
		case CDIOCCLRDEBUG:
		case CDIOCRESET:
		case DVD_AUTH:
		case DVD_READ_STRUCT:
		case MTIOCTOP:
			if (part == RAW_PART)
				break;
		/* FALLTHROUGH */
		default:
			if ((cd->sc_link->flags & SDEV_OPEN) == 0)
				error = ENODEV;
			else
				error = EIO;
			goto exit;
		}
	}

	switch (cmd) {
	case DIOCRLDINFO:
		lp = malloc(sizeof(*lp), M_TEMP, M_WAITOK);
		cdgetdisklabel(dev, cd, lp, 0);
		bcopy(lp, cd->sc_dk.dk_label, sizeof(*lp));
		free(lp, M_TEMP);
		break;
	case DIOCGDINFO:
	case DIOCGPDINFO:
		*(struct disklabel *)addr = *(cd->sc_dk.dk_label);
		break;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = cd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &cd->sc_dk.dk_label->d_partitions[DISKPART(dev)];
		break;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0) {
			error = EBADF;
			break;
		}

		if ((error = cdlock(cd)) != 0)
			break;

		cd->flags |= CDF_LABELLING;

		error = setdisklabel(cd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*cd->sc_dk.dk_openmask : */0);
		if (error == 0) {
		}

		cd->flags &= ~CDF_LABELLING;
		cdunlock(cd);
		break;

	case DIOCWLABEL:
		error = EBADF;
		break;

	case CDIOCPLAYTRACKS: {
		struct ioc_play_track *args = (struct ioc_play_track *)addr;

		if ((error = cd_set_pa_immed(cd, 0)) != 0)
			break;
		error = cd_play_tracks(cd, args->start_track,
		    args->start_index, args->end_track, args->end_index);
		break;
	}
	case CDIOCPLAYMSF: {
		struct ioc_play_msf *args = (struct ioc_play_msf *)addr;

		if ((error = cd_set_pa_immed(cd, 0)) != 0)
			break;
		error = cd_play_msf(cd, args->start_m, args->start_s,
		    args->start_f, args->end_m, args->end_s, args->end_f);
		break;
	}
	case CDIOCPLAYBLOCKS: {
		struct ioc_play_blocks *args = (struct ioc_play_blocks *)addr;

		if ((error = cd_set_pa_immed(cd, 0)) != 0)
			break;
		error = cd_play(cd, args->blk, args->len);
		break;
	}
	case CDIOCREADSUBCHANNEL: {
		struct ioc_read_subchannel *args
		= (struct ioc_read_subchannel *)addr;
		struct cd_sub_channel_info data;
		int len = args->data_len;
		if (len > sizeof(data) ||
		    len < sizeof(struct cd_sub_channel_header)) {
			error = EINVAL;
			break;
		}
		error = cd_read_subchannel(cd, args->address_format,
					   args->data_format, args->track,
					   &data, len);
		if (error)
			break;
		len = min(len, _2btol(data.header.data_len) +
		    sizeof(struct cd_sub_channel_header));
		error = copyout(&data, args->data, len);
		break;
	}
	case CDIOREADTOCHEADER: {
		struct ioc_toc_header th;

		if ((error = cd_read_toc(cd, 0, 0, &th, sizeof(th), 0)) != 0)
			break;
		if (cd->sc_link->quirks & ADEV_LITTLETOC)
			th.len = letoh16(th.len);
		else
			th.len = betoh16(th.len);
		bcopy(&th, addr, sizeof(th));
		break;
	}
	case CDIOREADTOCENTRYS:  {
		struct cd_toc *toc;
		struct ioc_read_toc_entry *te =
		    (struct ioc_read_toc_entry *)addr;
		struct ioc_toc_header *th;
		struct cd_toc_entry *cte;
		int len = te->data_len;
		int ntracks;

		MALLOC(toc, struct cd_toc *, sizeof(struct cd_toc), M_TEMP,
		    M_WAITOK);
		bzero(toc, sizeof(*toc));

		th = &toc->header;

		if (len > sizeof(toc->entries) ||
		    len < sizeof(struct cd_toc_entry)) {
			FREE(toc, M_TEMP);
			error = EINVAL;
			break;
		}
		error = cd_read_toc(cd, te->address_format, te->starting_track,
		    toc, len + sizeof(struct ioc_toc_header), 0);
		if (error) {
			FREE(toc, M_TEMP);
			break;
		}
		if (te->address_format == CD_LBA_FORMAT)
			for (ntracks =
			    th->ending_track - th->starting_track + 1;
			    ntracks >= 0; ntracks--) {
				cte = &toc->entries[ntracks];
				cte->addr_type = CD_LBA_FORMAT;
				if (cd->sc_link->quirks & ADEV_LITTLETOC) {
#if BYTE_ORDER == BIG_ENDIAN
					swap16_multi((u_int16_t *)&cte->addr,
					    sizeof(cte->addr) / 2);
#endif
				} else
					cte->addr.lba = betoh32(cte->addr.lba);
			}
		if (cd->sc_link->quirks & ADEV_LITTLETOC) {
			th->len = letoh16(th->len);
		} else
			th->len = betoh16(th->len);
		len = min(len, th->len - (sizeof(th->starting_track) +
		    sizeof(th->ending_track)));

		error = copyout(toc->entries, te->data, len);
		FREE(toc, M_TEMP);
		break;
	}
	case CDIOREADMSADDR: {
		struct cd_toc *toc;
		int sessno = *(int *)addr;
		struct cd_toc_entry *cte;

		if (sessno != 0) {
			error = EINVAL;
			break;
		}

		MALLOC(toc, struct cd_toc *, sizeof(struct cd_toc), M_TEMP,
		    M_WAITOK);
		bzero(toc, sizeof(*toc));

		error = cd_read_toc(cd, 0, 0, toc,
		  sizeof(struct ioc_toc_header) + sizeof(struct cd_toc_entry),
		  0x40 /* control word for "get MS info" */);

		if (error) {
			FREE(toc, M_TEMP);
			break;
		}

		cte = &toc->entries[0];
		if (cd->sc_link->quirks & ADEV_LITTLETOC) {
#if BYTE_ORDER == BIG_ENDIAN
			swap16_multi((u_int16_t *)&cte->addr,
			    sizeof(cte->addr) / 2);
#endif
		} else
			cte->addr.lba = betoh32(cte->addr.lba);
		if (cd->sc_link->quirks & ADEV_LITTLETOC)
			toc->header.len = letoh16(toc->header.len);
		else
			toc->header.len = betoh16(toc->header.len);

		*(int *)addr = (toc->header.len >= 10 && cte->track > 1) ?
			cte->addr.lba : 0;
		FREE(toc, M_TEMP);
		break;
	}
	case CDIOCSETPATCH: {
		struct ioc_patch *arg = (struct ioc_patch *)addr;

		error = cd_setchan(cd, arg->patch[0], arg->patch[1],
		    arg->patch[2], arg->patch[3], 0);
		break;
	}
	case CDIOCGETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;

		error = cd_getvol(cd, arg, 0);
		break;
	}
	case CDIOCSETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;

		error = cd_setvol(cd, arg, 0);
		break;
	}

	case CDIOCSETMONO:
		error = cd_setchan(cd, BOTH_CHANNEL, BOTH_CHANNEL, MUTE_CHANNEL,
		    MUTE_CHANNEL, 0);
		break;

	case CDIOCSETSTEREO:
		error = cd_setchan(cd, LEFT_CHANNEL, RIGHT_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL, 0);
		break;

	case CDIOCSETMUTE:
		error = cd_setchan(cd, MUTE_CHANNEL, MUTE_CHANNEL, MUTE_CHANNEL,
		    MUTE_CHANNEL, 0);
		break;

	case CDIOCSETLEFT:
		error = cd_setchan(cd, LEFT_CHANNEL, LEFT_CHANNEL, MUTE_CHANNEL,
		    MUTE_CHANNEL, 0);
		break;

	case CDIOCSETRIGHT:
		error = cd_setchan(cd, RIGHT_CHANNEL, RIGHT_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL, 0);
		break;

	case CDIOCRESUME:
		error = cd_pause(cd, 1);
		break;

	case CDIOCPAUSE:
		error = cd_pause(cd, 0);
		break;
	case CDIOCSTART:
		error = scsi_start(cd->sc_link, SSS_START, 0);
		break;

	case CDIOCSTOP:
		error = scsi_start(cd->sc_link, SSS_STOP, 0);
		break;

	close_tray:
	case CDIOCCLOSE:
		error = scsi_start(cd->sc_link, SSS_START|SSS_LOEJ,
		    SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE);
		break;

	case MTIOCTOP:
		if (((struct mtop *)addr)->mt_op == MTRETEN)
			goto close_tray;
		if (((struct mtop *)addr)->mt_op != MTOFFL) {
			error = EIO;
			break;
		}
		/* FALLTHROUGH */
	case CDIOCEJECT: /* FALLTHROUGH */
	case DIOCEJECT:
		cd->sc_link->flags |= SDEV_EJECTING;
		break;
	case CDIOCALLOW:
		error = scsi_prevent(cd->sc_link, PR_ALLOW, 0);
		break;
	case CDIOCPREVENT:
		error = scsi_prevent(cd->sc_link, PR_PREVENT, 0);
		break;
	case DIOCLOCK:
		error = scsi_prevent(cd->sc_link,
		    (*(int *)addr) ? PR_PREVENT : PR_ALLOW, 0);
		break;
	case CDIOCSETDEBUG:
		cd->sc_link->flags |= (SDEV_DB1 | SDEV_DB2);
		break;
	case CDIOCCLRDEBUG:
		cd->sc_link->flags &= ~(SDEV_DB1 | SDEV_DB2);
		break;
	case CDIOCRESET:
	case SCIOCRESET:
		error = cd_reset(cd);
		break;
	case CDIOCLOADUNLOAD: {
		struct ioc_load_unload *args = (struct ioc_load_unload *)addr;

		error = cd_load_unload(cd, args->options, args->slot);
		break;
	}

	case DVD_AUTH:
		error = dvd_auth(cd, (union dvd_authinfo *)addr);
		break;
	case DVD_READ_STRUCT:
		error = dvd_read_struct(cd, (union dvd_struct *)addr);
		break;
	default:
		if (DISKPART(dev) != RAW_PART) {
			error = ENOTTY;
			break;
		}
		error = scsi_do_ioctl(cd->sc_link, dev, cmd, addr, flag, p);
		break;
	}

 exit:

	device_unref(&cd->sc_dev);
	return (error);
}

/*
 * Load the label information on the named device
 * Actually fabricate a disklabel
 *
 * EVENTUALLY take information about different
 * data tracks from the TOC and put it in the disklabel
 */
void
cdgetdisklabel(dev_t dev, struct cd_softc *cd, struct disklabel *lp,
    int spoofonly)
{
	struct cd_toc *toc;
	char *errstring;
	int tocidx, n, audioonly = 1;

	bzero(lp, sizeof(struct disklabel));

	MALLOC(toc, struct cd_toc *, sizeof(struct cd_toc), M_TEMP, M_WAITOK);
	bzero(toc, sizeof(*toc));

	lp->d_secsize = cd->params.blksize;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_secpercyl = 100;
	lp->d_ncylinders = (cd->params.disksize / 100) + 1;

	if (cd->sc_link->flags & SDEV_ATAPI) {
		strncpy(lp->d_typename, "ATAPI CD-ROM", sizeof(lp->d_typename));
		lp->d_type = DTYPE_ATAPI;
	} else {
		strncpy(lp->d_typename, "SCSI CD-ROM", sizeof(lp->d_typename));
		lp->d_type = DTYPE_SCSI;
	}

	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname));
	DL_SETDSIZE(lp, cd->params.disksize);
	lp->d_rpm = 300;
	lp->d_interleave = 1;
	lp->d_version = 1;

	/* XXX - these values for BBSIZE and SBSIZE assume ffs */
	lp->d_bbsize = BBSIZE;
	lp->d_sbsize = SBSIZE;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	if (cd_load_toc(cd, toc, CD_LBA_FORMAT)) {
		audioonly = 0; /* No valid TOC found == not an audio CD. */
		goto done;
	}

	n = toc->header.ending_track - toc->header.starting_track + 1;
	for (tocidx = 0; tocidx < n; tocidx++)
		if (toc->entries[tocidx].control & 4) {
			audioonly = 0; /* Found a non-audio track. */
			goto done;
		}

done:
	free(toc, M_TEMP);

	if (!audioonly) {
		errstring = readdisklabel(DISKLABELDEV(dev), cdstrategy, lp,
		    spoofonly);
		/*if (errstring)
			printf("%s: %s\n", cd->sc_dev.dv_xname, errstring);*/
	}
}

/*
 * Find out from the device what its capacity is
 */
u_long
cd_size(struct cd_softc *cd, int flags)
{
	struct scsi_read_cd_cap_data rdcap;
	struct scsi_read_cd_capacity scsi_cmd;
	u_long size;
	int blksize;

	/* Reasonable defaults for drives that don't support
	   READ_CD_CAPACITY */
	cd->params.blksize = 2048;
	cd->params.disksize = 400000;

	if (cd->sc_link->quirks & ADEV_NOCAPACITY)
		goto exit;

	/*
	 * make up a scsi command and ask the scsi driver to do
	 * it for you.
	 */
	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = READ_CD_CAPACITY;

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks and a blocksize
	 */
	if (scsi_scsi_cmd(cd->sc_link,
	    (struct scsi_generic *)&scsi_cmd, sizeof(scsi_cmd),
	    (u_char *)&rdcap, sizeof(rdcap), CDRETRIES, 20000, NULL,
	    flags | SCSI_DATA_IN) != 0)
		goto exit;

	blksize = _4btol(rdcap.length);
	if ((blksize < 512) || ((blksize & 511) != 0))
		blksize = 2048;	/* some drives lie ! */
	cd->params.blksize = blksize;

	size = _4btol(rdcap.addr) + 1;
	if (size < 100)
		size = 400000;	/* ditto */
	cd->params.disksize = size;

 exit:
	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cd_size: %d %ld\n", blksize, size));
	return (cd->params.disksize);
}

int
cd_setchan(struct cd_softc *cd, int p0, int p1, int p2, int p3, int flags)
{
	union scsi_mode_sense_buf *data;
	struct cd_audio_page *audio = NULL;
	int error, big;

	data = malloc(sizeof(*data), M_TEMP, M_NOWAIT);
	if (data == NULL)
		return (ENOMEM);

	error = scsi_do_mode_sense(cd->sc_link, AUDIO_PAGE, data,
	    (void **)&audio, NULL, NULL, NULL, sizeof(*audio), flags, &big);
	if (error == 0 && audio == NULL)
		error = EIO;

	if (error == 0) {
		audio->port[LEFT_PORT].channels = p0;
		audio->port[RIGHT_PORT].channels = p1;
		audio->port[2].channels = p2;
		audio->port[3].channels = p3;
		if (big)
			error = scsi_mode_select_big(cd->sc_link, SMS_PF,
			    &data->hdr_big, flags, 20000);
		else
			error = scsi_mode_select(cd->sc_link, SMS_PF,
			    &data->hdr, flags, 20000);
	}

	free(data, M_TEMP);
	return (error);
}

int
cd_getvol(struct cd_softc *cd, struct ioc_vol *arg, int flags)
{
	union scsi_mode_sense_buf *data;
	struct cd_audio_page *audio = NULL;
	int error;

	data = malloc(sizeof(*data), M_TEMP, M_NOWAIT);
	if (data == NULL)
		return (ENOMEM);

	error = scsi_do_mode_sense(cd->sc_link, AUDIO_PAGE, data,
	    (void **)&audio, NULL, NULL, NULL, sizeof(*audio), flags, NULL);
	if (error == 0 && audio == NULL)
		error = EIO;

	if (error == 0) {
		arg->vol[0] = audio->port[0].volume;
		arg->vol[1] = audio->port[1].volume;
		arg->vol[2] = audio->port[2].volume;
		arg->vol[3] = audio->port[3].volume;
	}

	free(data, M_TEMP);
	return (0);
}

int
cd_setvol(struct cd_softc *cd, const struct ioc_vol *arg, int flags)
{
	union scsi_mode_sense_buf *data;
	struct cd_audio_page *audio = NULL;
	u_int8_t mask_volume[4];
	int error, big;

	data = malloc(sizeof(*data), M_TEMP, M_NOWAIT);
	if (data == NULL)
		return (ENOMEM);

	error = scsi_do_mode_sense(cd->sc_link,
	    AUDIO_PAGE | SMS_PAGE_CTRL_CHANGEABLE, data, (void **)&audio, NULL,
	    NULL, NULL, sizeof(*audio), flags, NULL);
	if (error == 0 && audio == NULL)
		error = EIO;
	if (error != 0) {
		free(data, M_TEMP);
		return (error);
	}

	mask_volume[0] = audio->port[0].volume;
	mask_volume[1] = audio->port[1].volume;
	mask_volume[2] = audio->port[2].volume;
	mask_volume[3] = audio->port[3].volume;

	error = scsi_do_mode_sense(cd->sc_link, AUDIO_PAGE, data,
	    (void **)&audio, NULL, NULL, NULL, sizeof(*audio), flags, &big);
	if (error == 0 && audio == NULL)
		error = EIO;
	if (error != 0) {
		free(data, M_TEMP);
		return (error);
	}

	audio->port[0].volume = arg->vol[0] & mask_volume[0];
	audio->port[1].volume = arg->vol[1] & mask_volume[1];
	audio->port[2].volume = arg->vol[2] & mask_volume[2];
	audio->port[3].volume = arg->vol[3] & mask_volume[3];

	if (big)
		error = scsi_mode_select_big(cd->sc_link, SMS_PF,
		    &data->hdr_big, flags, 20000);
	else
		error = scsi_mode_select(cd->sc_link, SMS_PF,
		    &data->hdr, flags, 20000);

	free(data, M_TEMP);
	return (error);
}

int
cd_load_unload(struct cd_softc *cd, int options, int slot)
{
	struct scsi_load_unload cmd;

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = LOAD_UNLOAD;
	cmd.options = options;    /* ioctl uses ATAPI values */
	cmd.slot = slot;

	return (scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), 0, 0, CDRETRIES, 200000, NULL, 0));
}

int
cd_set_pa_immed(struct cd_softc *cd, int flags)
{
	union scsi_mode_sense_buf *data;
	struct cd_audio_page *audio = NULL;
	int error, oflags, big;

	if (cd->sc_link->flags & SDEV_ATAPI)
		/* XXX Noop? */
		return (0);

	data = malloc(sizeof(*data), M_TEMP, M_NOWAIT);
	if (data == NULL)
		return (ENOMEM);

	error = scsi_do_mode_sense(cd->sc_link, AUDIO_PAGE, data,
	    (void **)&audio, NULL, NULL, NULL, sizeof(*audio), flags, &big);
	if (error == 0 && audio == NULL)
		error = EIO;

	if (error == 0) {
		oflags = audio->flags;
		audio->flags &= ~CD_PA_SOTC;
		audio->flags |= CD_PA_IMMED;
		if (audio->flags != oflags) {
			if (big)
				error = scsi_mode_select_big(cd->sc_link,
				    SMS_PF, &data->hdr_big, flags,
				    20000);
			else
				error = scsi_mode_select(cd->sc_link, SMS_PF,
				    &data->hdr, flags, 20000);
		}
	}

	free(data, M_TEMP);
	return (error);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int
cd_play(struct cd_softc *cd, int blkno, int nblks)
{
	struct scsi_play scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PLAY;
	_lto4b(blkno, scsi_cmd.blk_addr);
	_lto2b(nblks, scsi_cmd.xfer_len);
	return (scsi_scsi_cmd(cd->sc_link,
	    (struct scsi_generic *)&scsi_cmd, sizeof(scsi_cmd),
	    0, 0, CDRETRIES, 200000, NULL, 0));
}

/*
 * Get scsi driver to send a "start playing" command
 */
int
cd_play_tracks(struct cd_softc *cd, int strack, int sindex, int etrack,
    int eindex)
{
	struct cd_toc *toc;
	u_char endf, ends, endm;
	int error;

	if (!etrack)
		return (EIO);
	if (strack > etrack)
		return (EINVAL);

	MALLOC(toc, struct cd_toc *, sizeof(struct cd_toc), M_TEMP, M_WAITOK);
	bzero(toc, sizeof(*toc));

	if ((error = cd_load_toc(cd, toc, CD_MSF_FORMAT)) != 0)
		goto done;

	if (++etrack > (toc->header.ending_track+1))
		etrack = toc->header.ending_track+1;

	strack -= toc->header.starting_track;
	etrack -= toc->header.starting_track;
	if (strack < 0) {
		error = EINVAL;
		goto done;
	}

	/*
	 * The track ends one frame before the next begins.  The last track
	 * is taken care of by the leadoff track.
	 */
	endm = toc->entries[etrack].addr.msf.minute;
	ends = toc->entries[etrack].addr.msf.second;
	endf = toc->entries[etrack].addr.msf.frame;
	if (endf-- == 0) {
		endf = CD_FRAMES - 1;
		if (ends-- == 0) {
			ends = CD_SECS - 1;
			if (endm-- == 0) {
				error = EINVAL;
				goto done;
			}
		}
	}

	error = cd_play_msf(cd, toc->entries[strack].addr.msf.minute,
	    toc->entries[strack].addr.msf.second,
	    toc->entries[strack].addr.msf.frame,
	    endm, ends, endf);

done:
	FREE(toc, M_TEMP);
	return (error);
}

/*
 * Get scsi driver to send a "play msf" command
 */
int
cd_play_msf(struct cd_softc *cd, int startm, int starts, int startf, int endm,
    int ends, int endf)
{
	struct scsi_play_msf scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PLAY_MSF;
	scsi_cmd.start_m = startm;
	scsi_cmd.start_s = starts;
	scsi_cmd.start_f = startf;
	scsi_cmd.end_m = endm;
	scsi_cmd.end_s = ends;
	scsi_cmd.end_f = endf;
	return (scsi_scsi_cmd(cd->sc_link,
	    (struct scsi_generic *)&scsi_cmd, sizeof(scsi_cmd),
	    0, 0, CDRETRIES, 20000, NULL, 0));
}

/*
 * Get scsi driver to send a "start up" command
 */
int
cd_pause(struct cd_softc *cd, int go)
{
	struct scsi_pause scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PAUSE;
	scsi_cmd.resume = go;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 2000, NULL, 0);
}

/*
 * Get scsi driver to send a "RESET" command
 */
int
cd_reset(struct cd_softc *cd)
{
	return scsi_scsi_cmd(cd->sc_link, 0, 0, 0, 0, CDRETRIES, 2000, NULL,
	    SCSI_RESET);
}

/*
 * Read subchannel
 */
int
cd_read_subchannel(struct cd_softc *cd, int mode, int format, int track,
    struct cd_sub_channel_info *data, int len)
{
	struct scsi_read_subchannel scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = READ_SUBCHANNEL;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.byte3 = SRS_SUBQ;
	scsi_cmd.subchan_format = format;
	scsi_cmd.track = track;
	_lto2b(len, scsi_cmd.data_len);
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(struct scsi_read_subchannel), (u_char *)data, len,
	    CDRETRIES, 5000, NULL, SCSI_DATA_IN|SCSI_SILENT);
}

/*
 * Read table of contents
 */
int
cd_read_toc(struct cd_softc *cd, int mode, int start, void *data, int len,
    int control)
{
	struct scsi_read_toc scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(data, len);

	scsi_cmd.opcode = READ_TOC;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.from_track = start;
	_lto2b(len, scsi_cmd.data_len);
	scsi_cmd.control = control;

	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(struct scsi_read_toc), (u_char *)data, len, CDRETRIES,
	    5000, NULL, SCSI_DATA_IN | SCSI_IGNORE_ILLEGAL_REQUEST);
}

int
cd_load_toc(struct cd_softc *cd, struct cd_toc *toc, int fmt)
{
	int n, len, error;

	error = cd_read_toc(cd, 0, 0, toc, sizeof(toc->header), 0);

	if (error == 0) {
		if (toc->header.ending_track < toc->header.starting_track)
			return (EIO);
		/* +2 to account for leading out track. */
		n = toc->header.ending_track - toc->header.starting_track + 2;
		len = n * sizeof(struct cd_toc_entry) + sizeof(toc->header);
		error = cd_read_toc(cd, fmt, 0, toc, len, 0);
	}

	return (error);
}


/*
 * Get the scsi driver to send a full inquiry to the device and use the
 * results to fill out the disk parameter structure.
 */
int
cd_get_parms(struct cd_softc *cd, int flags)
{
	/*
	 * give a number of sectors so that sec * trks * cyls
	 * is <= disk_size
	 */
	if (cd_size(cd, flags) == 0)
		return (ENXIO);
	return (0);
}

daddr64_t
cdsize(dev_t dev)
{

	/* CD-ROMs are read-only. */
	return -1;
}

int
cddump(dev_t dev, daddr64_t blkno, caddr_t va, size_t size)
{
	/* Not implemented. */
	return ENXIO;
}

#define	dvd_copy_key(dst, src)		bcopy((src), (dst), DVD_KEY_SIZE)
#define	dvd_copy_challenge(dst, src)	bcopy((src), (dst), DVD_CHALLENGE_SIZE)

int
dvd_auth(struct cd_softc *cd, union dvd_authinfo *a)
{
	struct scsi_generic cmd;
	u_int8_t buf[20];
	int error;

	bzero(cmd.bytes, sizeof(cmd.bytes));
	bzero(buf, sizeof(buf));

	switch (a->type) {
	case DVD_LU_SEND_AGID:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 8;
		cmd.bytes[9] = 0 | (0 << 6);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 8,
		    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
		if (error)
			return (error);
		a->lsa.agid = buf[7] >> 6;
		return (0);

	case DVD_LU_SEND_CHALLENGE:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 16;
		cmd.bytes[9] = 1 | (a->lsc.agid << 6);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 16,
		    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
		if (error)
			return (error);
		dvd_copy_challenge(a->lsc.chal, &buf[4]);
		return (0);

	case DVD_LU_SEND_KEY1:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 12;
		cmd.bytes[9] = 2 | (a->lsk.agid << 6);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 12,
		    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
		if (error)
			return (error);
		dvd_copy_key(a->lsk.key, &buf[4]);
		return (0);

	case DVD_LU_SEND_TITLE_KEY:
		cmd.opcode = GPCMD_REPORT_KEY;
		_lto4b(a->lstk.lba, &cmd.bytes[1]);
		cmd.bytes[8] = 12;
		cmd.bytes[9] = 4 | (a->lstk.agid << 6);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 12,
		    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
		if (error)
			return (error);
		a->lstk.cpm = (buf[4] >> 7) & 1;
		a->lstk.cp_sec = (buf[4] >> 6) & 1;
		a->lstk.cgms = (buf[4] >> 4) & 3;
		dvd_copy_key(a->lstk.title_key, &buf[5]);
		return (0);

	case DVD_LU_SEND_ASF:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 8;
		cmd.bytes[9] = 5 | (a->lsasf.agid << 6);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 8,
		    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
		if (error)
			return (error);
		a->lsasf.asf = buf[7] & 1;
		return (0);

	case DVD_HOST_SEND_CHALLENGE:
		cmd.opcode = GPCMD_SEND_KEY;
		cmd.bytes[8] = 16;
		cmd.bytes[9] = 1 | (a->hsc.agid << 6);
		buf[1] = 14;
		dvd_copy_challenge(&buf[4], a->hsc.chal);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 16,
		    CDRETRIES, 30000, NULL, SCSI_DATA_OUT);
		if (error)
			return (error);
		a->type = DVD_LU_SEND_KEY1;
		return (0);

	case DVD_HOST_SEND_KEY2:
		cmd.opcode = GPCMD_SEND_KEY;
		cmd.bytes[8] = 12;
		cmd.bytes[9] = 3 | (a->hsk.agid << 6);
		buf[1] = 10;
		dvd_copy_key(&buf[4], a->hsk.key);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 12,
		    CDRETRIES, 30000, NULL, SCSI_DATA_OUT);
		if (error) {
			a->type = DVD_AUTH_FAILURE;
			return (error);
		}
		a->type = DVD_AUTH_ESTABLISHED;
		return (0);

	case DVD_INVALIDATE_AGID:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[9] = 0x3f | (a->lsa.agid << 6);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 16,
		    CDRETRIES, 30000, NULL, 0);
		if (error)
			return (error);
		return (0);

	case DVD_LU_SEND_RPC_STATE:
		cmd.opcode = GPCMD_REPORT_KEY;
		cmd.bytes[8] = 8;
		cmd.bytes[9] = 8 | (0 << 6);
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 8,
		    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
		if (error)
			return (error);
		a->lrpcs.type = (buf[4] >> 6) & 3;
		a->lrpcs.vra = (buf[4] >> 3) & 7;
		a->lrpcs.ucca = (buf[4]) & 7;
		a->lrpcs.region_mask = buf[5];
		a->lrpcs.rpc_scheme = buf[6];
		return (0);

	case DVD_HOST_SEND_RPC_STATE:
		cmd.opcode = GPCMD_SEND_KEY;
		cmd.bytes[8] = 8;
		cmd.bytes[9] = 6 | (0 << 6);
		buf[1] = 6;
		buf[4] = a->hrpcs.pdrc;
		error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, 8,
		    CDRETRIES, 30000, NULL, SCSI_DATA_OUT);
		if (error)
			return (error);
		return (0);

	default:
		return (ENOTTY);
	}
}

int
dvd_read_physical(cd, s)
	struct cd_softc *cd;
	union dvd_struct *s;
{
	struct scsi_generic cmd;
	u_int8_t buf[4 + 4 * 20], *bufp;
	int error;
	struct dvd_layer *layer;
	int i;

	bzero(cmd.bytes, sizeof(cmd.bytes));
	bzero(buf, sizeof(buf));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(sizeof(buf), &cmd.bytes[7]);

	cmd.bytes[5] = s->physical.layer_num;
	error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, sizeof(buf),
	    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
	if (error)
		return (error);
	for (i = 0, bufp = &buf[4], layer = &s->physical.layer[0]; i < 4;
	    i++, bufp += 20, layer++) {
		bzero(layer, sizeof(*layer));
		layer->book_version = bufp[0] & 0xf;
		layer->book_type = bufp[0] >> 4;
		layer->min_rate = bufp[1] & 0xf;
		layer->disc_size = bufp[1] >> 4;
		layer->layer_type = bufp[2] & 0xf;
		layer->track_path = (bufp[2] >> 4) & 1;
		layer->nlayers = (bufp[2] >> 5) & 3;
		layer->track_density = bufp[3] & 0xf;
		layer->linear_density = bufp[3] >> 4;
		layer->start_sector = _4btol(&bufp[4]);
		layer->end_sector = _4btol(&bufp[8]);
		layer->end_sector_l0 = _4btol(&bufp[12]);
		layer->bca = bufp[16] >> 7;
	}
	return (0);
}

int
dvd_read_copyright(cd, s)
	struct cd_softc *cd;
	union dvd_struct *s;
{
	struct scsi_generic cmd;
	u_int8_t buf[8];
	int error;

	bzero(cmd.bytes, sizeof(cmd.bytes));
	bzero(buf, sizeof(buf));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(sizeof(buf), &cmd.bytes[7]);

	cmd.bytes[5] = s->copyright.layer_num;
	error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, sizeof(buf),
	    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
	if (error)
		return (error);
	s->copyright.cpst = buf[4];
	s->copyright.rmi = buf[5];
	return (0);
}

int
dvd_read_disckey(cd, s)
	struct cd_softc *cd;
	union dvd_struct *s;
{
	struct scsi_read_dvd_structure cmd;
	struct scsi_read_dvd_structure_data *buf;
	int error;

	buf = malloc(sizeof(*buf), M_TEMP, M_WAITOK | M_ZERO);
	if (buf == NULL)
		return (ENOMEM);

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.format = s->type;
	cmd.agid = s->disckey.agid << 6;
	_lto2b(sizeof(*buf), cmd.length);

	error = scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), (u_char *)buf, sizeof(*buf), CDRETRIES, 30000, NULL,
	    SCSI_DATA_IN);
	if (error == 0)
		bcopy(buf->data, s->disckey.value, sizeof(s->disckey.value));

	free(buf, M_TEMP);
	return (error);
}

int
dvd_read_bca(cd, s)
	struct cd_softc *cd;
	union dvd_struct *s;
{
	struct scsi_generic cmd;
	u_int8_t buf[4 + 188];
	int error;

	bzero(cmd.bytes, sizeof(cmd.bytes));
	bzero(buf, sizeof(buf));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(sizeof(buf), &cmd.bytes[7]);

	error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, sizeof(buf),
	    CDRETRIES, 30000, NULL, SCSI_DATA_IN);
	if (error)
		return (error);
	s->bca.len = _2btol(&buf[0]);
	if (s->bca.len < 12 || s->bca.len > 188)
		return (EIO);
	bcopy(&buf[4], s->bca.value, s->bca.len);
	return (0);
}

int
dvd_read_manufact(cd, s)
	struct cd_softc *cd;
	union dvd_struct *s;
{
	struct scsi_read_dvd_structure cmd;
	struct scsi_read_dvd_structure_data *buf;
	int error;

	buf = malloc(sizeof(*buf), M_TEMP, M_WAITOK | M_ZERO);
	if (buf == NULL)
		return (ENOMEM);

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.format = s->type;
	_lto2b(sizeof(*buf), cmd.length);

	error = scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), (u_char *)buf, sizeof(*buf), CDRETRIES, 30000, NULL,
	    SCSI_DATA_IN);
	if (error == 0) {
		s->manufact.len = _2btol(buf->len);
		if (s->manufact.len >= 0 && s->manufact.len <= 2048)
			bcopy(buf->data, s->manufact.value, s->manufact.len);
		else
			error = EIO;
	}

	free(buf, M_TEMP);
	return (error);
}

int
dvd_read_struct(cd, s)
	struct cd_softc *cd;
	union dvd_struct *s;
{

	switch (s->type) {
	case DVD_STRUCT_PHYSICAL:
		return (dvd_read_physical(cd, s));
	case DVD_STRUCT_COPYRIGHT:
		return (dvd_read_copyright(cd, s));
	case DVD_STRUCT_DISCKEY:
		return (dvd_read_disckey(cd, s));
	case DVD_STRUCT_BCA:
		return (dvd_read_bca(cd, s));
	case DVD_STRUCT_MANUFACT:
		return (dvd_read_manufact(cd, s));
	default:
		return (EINVAL);
	}
}

void
cd_powerhook(int why, void *arg)
{
	struct cd_softc *cd = arg;

	/*
	 * When resuming, hardware may have forgotten we locked it. So if
	 * there are any open partitions, lock the CD.
	 */
	if (why == PWR_RESUME && cd->sc_dk.dk_openmask != 0)
		scsi_prevent(cd->sc_link, PR_PREVENT,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE);
}

int
cd_interpret_sense(struct scsi_xfer *xs)
{
	struct scsi_sense_data *sense = &xs->sense;
	struct scsi_link *sc_link = xs->sc_link;
	u_int8_t skey = sense->flags & SSD_KEY;
	u_int8_t serr = sense->error_code & SSD_ERRCODE;

	if (((sc_link->flags & SDEV_OPEN) == 0) ||
	    (serr != SSD_ERRCODE_CURRENT && serr != SSD_ERRCODE_DEFERRED))
		return (EJUSTRETURN); /* let the generic code handle it */

	/*
	 * We do custom processing in cd for the unit becoming ready
	 * case.  We do not allow xs->retries to be decremented on the
	 * "Unit Becoming Ready" case. This is because CD drives
	 * report "Unit Becoming Ready" when loading media and can
	 * take a long time.  Rather than having a massive timeout for
	 * all operations (which would cause other problems), we allow
	 * operations to wait (but be interruptable with Ctrl-C)
	 * forever as long as the drive is reporting that it is
	 * becoming ready.  All other cases of not being ready are
	 * handled by the default handler.
	 */
	switch(skey) {
	case SKEY_NOT_READY:
		if ((xs->flags & SCSI_IGNORE_NOT_READY) != 0)
			return (0);
		if (ASC_ASCQ(sense) == SENSE_NOT_READY_BECOMING_READY) {
		    	SC_DEBUG(sc_link, SDEV_DB1, ("not ready: busy (%#x)\n",
			    sense->add_sense_code_qual));
			/* don't count this as a retry */
			xs->retries++;
			return (scsi_delay(xs, 1));
		}
		break;
	/* XXX more to come here for a few other cases */
	default:
		break;
	}
	return (EJUSTRETURN); /* use generic handler in scsi_base */
}

/*
 * Remove unprocessed buffers from queue.
 */
void
cd_kill_buffers(struct cd_softc *cd)
{
	struct buf *dp, *bp;
	int s;

	s = splbio();
	for (dp = &cd->buf_queue; (bp = dp->b_actf) != NULL; ) {
		dp->b_actf = bp->b_actf;

		bp->b_error = ENXIO;
		bp->b_flags |= B_ERROR;
		biodone(bp);
	}
	splx(s);
}
