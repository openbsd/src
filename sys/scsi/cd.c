/*	$OpenBSD: cd.c,v 1.49 1999/11/09 23:14:19 angelos Exp $	*/
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
#include <sys/cdio.h>
#include <sys/proc.h>
#include <sys/conf.h>
#include <sys/scsiio.h>
#include <sys/dvdio.h>

#include <scsi/scsi_all.h>
#include <scsi/cd.h>
#include <scsi/scsi_cd.h>
#include <scsi/scsi_disk.h>	/* rw_big and start_stop come from there */
#include <scsi/scsiconf.h>


#include <ufs/ffs/fs.h>			/* for BBSIZE and SBSIZE */

#include "cdvar.h"

#define	CDOUTSTANDING	4

#define	CDUNIT(z)			DISKUNIT(z)
#define	CDPART(z)			DISKPART(z)
#define	MAKECDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define MAXTRACK	99
#define CD_BLOCK_OFFSET	150
#define CD_FRAMES	75
#define CD_SECS		60

#define TOC_HEADER_LEN			0
#define TOC_HEADER_STARTING_TRACK	2
#define TOC_HEADER_ENDING_TRACK		3
#define TOC_HEADER_SZ			4

#define TOC_ENTRY_CONTROL_ADDR_TYPE	1
#define TOC_ENTRY_TRACK			2
#define TOC_ENTRY_MSF_LBA		4
#define TOC_ENTRY_SZ			8


struct cd_toc {
	struct ioc_toc_header header;
	struct cd_toc_entry entries[MAXTRACK+1]; /* One extra for the */
						 /* leadout */
};

#define	CDLABELDEV(dev)	(MAKECDDEV(major(dev), CDUNIT(dev), RAW_PART))

int	cdmatch __P((struct device *, void *, void *));
void	cdattach __P((struct device *, struct device *, void *));
int	cdlock __P((struct cd_softc *));
void	cdunlock __P((struct cd_softc *));
void	cdstart __P((void *));
void	cdminphys __P((struct buf *));
void	cdgetdisklabel __P((dev_t, struct cd_softc *, struct disklabel *,
			    struct cpu_disklabel *, int));
void	cddone __P((struct scsi_xfer *));
u_long	cd_size __P((struct cd_softc *, int));
void	lba2msf __P((u_long, u_char *, u_char *, u_char *));
u_long	msf2lba __P((u_char, u_char, u_char));
int	cd_play __P((struct cd_softc *, int, int));
int	cd_play_tracks __P((struct cd_softc *, int, int, int, int));
int	cd_play_msf __P((struct cd_softc *, int, int, int, int, int, int));
int	cd_pause __P((struct cd_softc *, int));
int	cd_reset __P((struct cd_softc *));
int	cd_read_subchannel __P((struct cd_softc *, int, int, int,
			        struct cd_sub_channel_info *, int ));
int	cd_read_toc __P((struct cd_softc *, int, int, void *,
			 int, int ));
int	cd_get_parms __P((struct cd_softc *, int));
int	cd_load_toc __P((struct cd_softc *, struct cd_toc *));

int    dvd_auth __P((struct cd_softc *, dvd_authinfo *));
int    dvd_read_physical __P((struct cd_softc *, dvd_struct *));
int    dvd_read_copyright __P((struct cd_softc *, dvd_struct *));
int    dvd_read_disckey __P((struct cd_softc *, dvd_struct *));
int    dvd_read_bca __P((struct cd_softc *, dvd_struct *));
int    dvd_read_manufact __P((struct cd_softc *, dvd_struct *));
int    dvd_read_struct __P((struct cd_softc *, dvd_struct *));

struct cfattach cd_ca = {
	sizeof(struct cd_softc), cdmatch, cdattach
};

struct cfdriver cd_cd = {
	NULL, "cd", DV_DISK
};

struct dkdriver cddkdriver = { cdstrategy };

struct scsi_device cd_switch = {
	NULL,			/* use default error handler */
	cdstart,		/* we have a queue, which is started by this */
	NULL,			/* we do not have an async handler */
	cddone,			/* deal with stats at interrupt time */
};

struct scsi_inquiry_pattern cd_patterns[] = {
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

extern struct cd_ops cd_atapibus_ops;
extern struct cd_ops cd_scsibus_ops;

int
cdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct scsibus_attach_args *sa = aux;
	int priority;

	(void)scsi_inqmatch(sa->sa_inqbuf,
	    (caddr_t)cd_patterns, sizeof(cd_patterns)/sizeof(cd_patterns[0]),
	    sizeof(cd_patterns[0]), &priority);
	return (priority);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 */
void
cdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct cd_softc *cd = (void *)self;
	struct scsibus_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("cdattach: "));

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

	dk_establish(&cd->sc_dk, &cd->sc_dev);
  
	/*
	 * Note if this device is ancient.  This is used in cdminphys().
	 */
	if (!(sc_link->flags & SDEV_ATAPI) &&
	    (sa->sa_inqbuf->version & SID_ANSII) == 0)
		cd->flags |= CDF_ANCIENT;

	if (sc_link->flags & SDEV_ATAPI) {
		cd->sc_ops = &cd_atapibus_ops;
	} else {
		cd->sc_ops = &cd_scsibus_ops;
	}

	printf("\n");
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
cdlock(cd)
	struct cd_softc *cd;
{
	int error;

	while ((cd->flags & CDF_LOCKED) != 0) {
		cd->flags |= CDF_WANTED;
		if ((error = tsleep(cd, PRIBIO | PCATCH, "cdlck", 0)) != 0)
			return error;
	}
	cd->flags |= CDF_LOCKED;
	return 0;
}

/*
 * Unlock and wake up any waiters.
 */
void
cdunlock(cd)
	struct cd_softc *cd;
{

	cd->flags &= ~CDF_LOCKED;
	if ((cd->flags & CDF_WANTED) != 0) {
		cd->flags &= ~CDF_WANTED;
		wakeup(cd);
	}
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
int 
cdopen(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct cd_softc *cd;
	struct scsi_link *sc_link;
	int unit, part;
	int error;

	unit = CDUNIT(dev);
	if (unit >= cd_cd.cd_ndevs)
		return ENXIO;
	cd = cd_cd.cd_devs[unit];
	if (!cd)
		return ENXIO;

	sc_link = cd->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("cdopen: dev=0x%x (unit %d (of %d), partition %d)\n", dev, unit,
	    cd_cd.cd_ndevs, CDPART(dev)));

	if ((error = cdlock(cd)) != 0)
		return error;

#ifdef CDDA
	/*
	 * We can't open the block device if the drive is in CDDA
	 * mode.  If we allow such opens, either the drive will get
	 * quite unhappy about undersized I/O or the BSD I/O subsystem
	 * will start trashing memory with disk transfers overrunning
	 * the end of their buffers.  Either way, it's Bad.
	 */
	if (cd->flags & CDF_CDDAMODE && fmt == S_IFBLK)
		return EBUSY;
#endif

	if (cd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
#ifdef CDDA
		/*
		 * If it's in CDDA mode, process may only open the
		 * raw partition
		 */
		if (cd->flags & CDF_CDDAMODE && part != RAW_PART) {
			error = EBUSY;
			goto bad3;
		}
#endif
	} else {
		/* Check that it is still responding and ok. */
		error = scsi_test_unit_ready(sc_link,
					     SCSI_IGNORE_ILLEGAL_REQUEST |
					     SCSI_IGNORE_MEDIA_CHANGE |
					     SCSI_IGNORE_NOT_READY);
		if (error)
			goto bad3;

		/* Start the pack spinning if necessary. */
		error = scsi_start(sc_link, SSS_START,
				   SCSI_IGNORE_ILLEGAL_REQUEST |
				   SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT);
		if (error)
			goto bad3;

		sc_link->flags |= SDEV_OPEN;

		/* Lock the pack in. */
		error = scsi_prevent(sc_link, PR_PREVENT,
				     SCSI_IGNORE_ILLEGAL_REQUEST |
				     SCSI_IGNORE_MEDIA_CHANGE);
		if (error)
			goto bad;

		if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
			sc_link->flags |= SDEV_MEDIA_LOADED;

			/* Load the physical device parameters. */
			if (cd_get_parms(cd, 0) != 0) {
				error = ENXIO;
				goto bad2;
			}
			SC_DEBUG(sc_link, SDEV_DB3, ("Params loaded "));

			/* Fabricate a disk label. */
			cdgetdisklabel(dev, cd, cd->sc_dk.dk_label,
			    cd->sc_dk.dk_cpulabel, 0);
			SC_DEBUG(sc_link, SDEV_DB3, ("Disklabel fabricated "));
		}
	}

	part = CDPART(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= cd->sc_dk.dk_label->d_npartitions ||
	     cd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		cd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		cd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	cd->sc_dk.dk_openmask = cd->sc_dk.dk_copenmask | cd->sc_dk.dk_bopenmask;

	SC_DEBUG(sc_link, SDEV_DB3, ("open complete\n"));
	cdunlock(cd);
	return 0;

bad2:
	sc_link->flags &= ~SDEV_MEDIA_LOADED;

bad:
	if (cd->sc_dk.dk_openmask == 0) {
		scsi_prevent(sc_link, PR_ALLOW,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE);
		sc_link->flags &= ~SDEV_OPEN;
	}

bad3:
	cdunlock(cd);
	return error;
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
int 
cdclose(dev, flag, fmt, p)
	dev_t dev;
	int flag, fmt;
	struct proc *p;
{
	struct cd_softc *cd = cd_cd.cd_devs[CDUNIT(dev)];
	int part = CDPART(dev);
	int error;

	if ((error = cdlock(cd)) != 0)
		return error;

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
		cd->sc_link->flags &= ~SDEV_OPEN;

		if (cd->sc_link->flags & SDEV_EJECTING) {
			scsi_start(cd->sc_link, SSS_STOP|SSS_LOEJ, 0);

			cd->sc_link->flags &= ~SDEV_EJECTING;
		}
	}

	cdunlock(cd);
	return 0;
}

/*
 * Actually translate the requested transfer into one the physical driver can
 * understand.  The transfer is described by a buf and will include only one
 * physical transfer.
 */
void
cdstrategy(bp)
	struct buf *bp;
{
	struct cd_softc *cd = cd_cd.cd_devs[CDUNIT(bp->b_dev)];
	int opri;

	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdstrategy "));
	SC_DEBUG(cd->sc_link, SDEV_DB1,
	    ("%ld bytes @ blk %d\n", bp->b_bcount, bp->b_blkno));
	/*
	 * The transfer must be a whole number of blocks.
	 */
	if ((bp->b_bcount % cd->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	/*
	 * If the device has been made invalid, error out
	 * maybe the media changed
	 */
	if ((cd->sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
		bp->b_error = EIO;
		goto bad;
	}
	/*
	 * If it's a null transfer, return immediately
	 */
	if (bp->b_bcount == 0)
		goto done;

#ifdef CDDA
	/*
	 * If in CDDA mode, return immediately (the user must issue
	 * SCSI read commands directly using SCIOCCOMMAND).  The BSD
	 * I/O subsystem just can't deal with bizzare block sizes like
	 * those used for CD-DA frames.
	 */
	if (cd->flags & CDF_CDDAMODE) {
		bp->b_error = EIO;
		goto bad;
	}
#endif
	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (CDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, cd->sc_dk.dk_label,
	    cd->sc_dk.dk_cpulabel,
	    (cd->flags & (CDF_WLABEL|CDF_LABELLING)) != 0) <= 0)
		goto done;

	opri = splbio();

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	disksort(&cd->buf_queue, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	cdstart(cd);

	splx(opri);
	return;

bad:
	bp->b_flags |= B_ERROR;
done:
	/*
	 * Correctly set the buf to indicate a completed xfer
	 */
	bp->b_resid = bp->b_bcount;
	biodone(bp);
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
 * cdstart() is called at splbio from cdstrategy and scsi_done
 */
void 
cdstart(v)
	register void *v;
{
	register struct cd_softc *cd = v;
	register struct scsi_link *sc_link = cd->sc_link;
	struct buf *bp = 0;
	struct buf *dp;
	struct scsi_rw_big cmd_big;
	struct scsi_rw cmd_small;
	struct scsi_generic *cmdp;
	int blkno, nblks, cmdlen;
	struct partition *p;

	SC_DEBUG(sc_link, SDEV_DB2, ("cdstart "));
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
		if (CDPART(bp->b_dev) != RAW_PART) {
		      p = &cd->sc_dk.dk_label->d_partitions[CDPART(bp->b_dev)];
		      blkno += p->p_offset;
		}
		nblks = howmany(bp->b_bcount, cd->sc_dk.dk_label->d_secsize);

		/*
		 *  Fill out the scsi command.  If the transfer will
		 *  fit in a "small" cdb, use it.
		 */
		if (!(sc_link->flags & SDEV_ATAPI) &&
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
		if (scsi_scsi_cmd(sc_link, cmdp, cmdlen,
		    (u_char *) bp->b_data, bp->b_bcount,
		    CDRETRIES, 30000, bp, SCSI_NOSLEEP |
		    ((bp->b_flags & B_READ) ? SCSI_DATA_IN : SCSI_DATA_OUT))) {
			disk_unbusy(&cd->sc_dk, 0);
			printf("%s: not queued", cd->sc_dev.dv_xname);
		}
	}
}

void
cddone(xs)
	struct scsi_xfer *xs;
{
	struct cd_softc *cd = xs->sc_link->device_softc;

	if (xs->bp != NULL)
		disk_unbusy(&cd->sc_dk, xs->bp->b_bcount - xs->bp->b_resid);
}

void
cdminphys(bp)
	struct buf *bp;
{
	struct cd_softc *cd = cd_cd.cd_devs[CDUNIT(bp->b_dev)];
	long max;

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
	if (cd->flags & CDF_ANCIENT) {
		max = cd->sc_dk.dk_label->d_secsize * 0xff;

		if (bp->b_bcount > max)
			bp->b_bcount = max;
	}

	(*cd->sc_link->adapter->scsi_minphys)(bp);
}

int
cdread(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(cdstrategy, NULL, dev, B_READ, cdminphys, uio));
}

int
cdwrite(dev, uio, ioflag)
	dev_t dev;
	struct uio *uio;
	int ioflag;
{

	return (physio(cdstrategy, NULL, dev, B_WRITE, cdminphys, uio));
}

/*
 * conversion between minute-seconde-frame and logical block adress
 * adresses format
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
cdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct cd_softc *cd = cd_cd.cd_devs[CDUNIT(dev)];
	int part = CDPART(dev);
	int error;

	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdioctl 0x%lx ", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if ((cd->sc_link->flags & SDEV_MEDIA_LOADED) == 0) {
		switch (cmd) {
		case DIOCWLABEL:
		case DIOCLOCK:
		case DIOCEJECT:
		case SCIOCIDENTIFY:
		case OSCIOCIDENTIFY:
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
			if (part == RAW_PART)
				break;
		/* FALLTHROUGH */
		default:
			if ((cd->sc_link->flags & SDEV_OPEN) == 0)
				return (ENODEV);
			else
				return (EIO);
		}
	}

	switch (cmd) {
	case DIOCRLDINFO:
		cdgetdisklabel(dev, cd, cd->sc_dk.dk_label,
		    cd->sc_dk.dk_cpulabel, 0);
		return 0;
	case DIOCGDINFO:
	case DIOCGPDINFO:
		*(struct disklabel *)addr = *(cd->sc_dk.dk_label);
		return 0;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = cd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &cd->sc_dk.dk_label->d_partitions[CDPART(dev)];
		return 0;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = cdlock(cd)) != 0)
			return error;
		cd->flags |= CDF_LABELLING;

		error = setdisklabel(cd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*cd->sc_dk.dk_openmask : */0,
		    cd->sc_dk.dk_cpulabel);
		if (error == 0) {
		}

		cd->flags &= ~CDF_LABELLING;
		cdunlock(cd);
		return error;

	case DIOCWLABEL:
		return EBADF;

	case CDIOCPLAYTRACKS: {
		struct ioc_play_track *args = (struct ioc_play_track *)addr;

		if ((error = (*cd->sc_ops->cdo_set_pa_immed)(cd, 0)) != 0)
			return (error);
		return (cd_play_tracks(cd, args->start_track,
		    args->start_index, args->end_track, args->end_index));
	}
	case CDIOCPLAYMSF: {
		struct ioc_play_msf *args = (struct ioc_play_msf *)addr;

		if ((error = (*cd->sc_ops->cdo_set_pa_immed)(cd, 0)) != 0)
			return (error);
		return (cd_play_msf(cd, args->start_m, args->start_s,
		    args->start_f, args->end_m, args->end_s, args->end_f));
	}
	case CDIOCPLAYBLOCKS: {
		struct ioc_play_blocks *args = (struct ioc_play_blocks *)addr;

		if ((error = (*cd->sc_ops->cdo_set_pa_immed)(cd, 0)) != 0)
			return (error);
		return (cd_play(cd, args->blk, args->len));
	}
	case CDIOCREADSUBCHANNEL: {
		struct ioc_read_subchannel *args
		= (struct ioc_read_subchannel *)addr;
		struct cd_sub_channel_info data;
		int len = args->data_len;
		if (len > sizeof(data) ||
		    len < sizeof(struct cd_sub_channel_header))
			return EINVAL;
		error = cd_read_subchannel(cd, args->address_format,
					   args->data_format, args->track,
					   &data, len);
		if (error)
			return error;
		len = min(len, _2btol(data.header.data_len) +
		    sizeof(struct cd_sub_channel_header));
		return copyout(&data, args->data, len);
	}
	case CDIOREADTOCHEADER: {
		struct ioc_toc_header th;

		if ((error = cd_read_toc(cd, 0, 0, &th, sizeof(th), 0)) != 0)
			return (error);
		if (cd->sc_link->quirks & ADEV_LITTLETOC) {
			th.len = letoh16(th.len);
		} else
			th.len = ntohs(th.len);
		bcopy(&th, addr, sizeof(th));
		return (0);
	}
	case CDIOREADTOCENTRYS:  {
		struct cd_toc *toc;
		struct ioc_read_toc_entry *te =
		    (struct ioc_read_toc_entry *)addr;
		struct ioc_toc_header *th;
		struct cd_toc_entry *cte;
		int len = te->data_len;
		int ntracks;
		int res;

		MALLOC (toc, struct cd_toc *, sizeof (struct cd_toc),
			M_DEVBUF, M_WAITOK);
			       
		th = &toc->header;

		if (len > sizeof(toc->entries) ||
		    len < sizeof(struct cd_toc_entry)) {
			FREE(toc, M_DEVBUF);
			return (EINVAL);
		}
		error = cd_read_toc(cd, te->address_format, te->starting_track,
		    toc, len + sizeof(struct ioc_toc_header), 0);
		if (error) {
			FREE(toc, M_DEVBUF);
			return (error);
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
					cte->addr.lba = ntohl(cte->addr.lba);
			}
		if (cd->sc_link->quirks & ADEV_LITTLETOC) {
			th->len = letoh16(th->len);
		} else
			th->len = ntohs(th->len);
		len = min(len, th->len - (sizeof(th->starting_track) +
		    sizeof(th->ending_track)));

		res = copyout(toc->entries, te->data, len);
		FREE(toc, M_DEVBUF);

		return (res);
	}
	case CDIOREADMSADDR: {
		struct cd_toc *toc;
		int sessno = *(int*)addr;
		struct cd_toc_entry *cte;

		if (sessno != 0)
			return (EINVAL);

		MALLOC (toc, struct cd_toc *, sizeof (struct cd_toc),
			M_DEVBUF, M_WAITOK);

		error = cd_read_toc(cd, 0, 0, toc,
		  sizeof(struct ioc_toc_header) + sizeof(struct cd_toc_entry),
		  0x40 /* control word for "get MS info" */);

		if (error) {
			FREE(toc, M_DEVBUF);
			return (error);
		}

		cte = &toc->entries[0];
		if (cd->sc_link->quirks & ADEV_LITTLETOC) {
#if BYTE_ORDER == BIG_ENDIAN
			swap16_multi((u_int16_t *)&cte->addr,
				     sizeof(cte->addr) / 2);
#endif
		} else
			cte->addr.lba = ntohl(cte->addr.lba);
		if (cd->sc_link->quirks & ADEV_LITTLETOC) {
			toc->header.len = letoh16(toc->header.len);
		} else
			toc->header.len = ntohs(toc->header.len);

		*(int*)addr = (toc->header.len >= 10 && cte->track > 1) ?
			cte->addr.lba : 0;
		FREE(toc, M_DEVBUF);
		return 0;
	}
	case CDIOCSETPATCH: {
		struct ioc_patch *arg = (struct ioc_patch *)addr;

		return ((*cd->sc_ops->cdo_setchan)(cd, arg->patch[0],
		    arg->patch[1], arg->patch[2], arg->patch[3], 0));
	}
	case CDIOCGETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;

		return ((*cd->sc_ops->cdo_getvol)(cd, arg, 0));
	}
	case CDIOCSETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;

		return ((*cd->sc_ops->cdo_setvol)(cd, arg, 0));
	}

	case CDIOCSETMONO:
		return ((*cd->sc_ops->cdo_setchan)(cd, BOTH_CHANNEL,
		    BOTH_CHANNEL, MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCSETSTEREO:
		return ((*cd->sc_ops->cdo_setchan)(cd, LEFT_CHANNEL,
		    RIGHT_CHANNEL, MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCSETMUTE:
		return ((*cd->sc_ops->cdo_setchan)(cd, MUTE_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCSETLEFT:
		return ((*cd->sc_ops->cdo_setchan)(cd, LEFT_CHANNEL,
		    LEFT_CHANNEL, MUTE_CHANNEL, MUTE_CHANNEL, 0));

	case CDIOCSETRIGHT:
		return ((*cd->sc_ops->cdo_setchan)(cd, RIGHT_CHANNEL,
		    RIGHT_CHANNEL, MUTE_CHANNEL, MUTE_CHANNEL, 0));
	case CDIOCRESUME:
		return cd_pause(cd, 1);
	case CDIOCPAUSE:
		return cd_pause(cd, 0);
	case CDIOCSTART:
		return scsi_start(cd->sc_link, SSS_START, 0);
	case CDIOCSTOP:
		return scsi_start(cd->sc_link, SSS_STOP, 0);
	case CDIOCCLOSE:
		return (scsi_start(cd->sc_link, SSS_START|SSS_LOEJ, 
		    SCSI_IGNORE_NOT_READY | SCSI_IGNORE_MEDIA_CHANGE));

	case MTIOCTOP:
		if (((struct mtop *)addr)->mt_op != MTOFFL)
			return EIO;
		/* FALLTHROUGH */
	case CDIOCEJECT: /* FALLTHROUGH */
	case DIOCEJECT:
		cd->sc_link->flags |= SDEV_EJECTING;
		return 0;
	case CDIOCALLOW:
		return scsi_prevent(cd->sc_link, PR_ALLOW, 0);
	case CDIOCPREVENT:
		return scsi_prevent(cd->sc_link, PR_PREVENT, 0);
	case DIOCLOCK:
		return scsi_prevent(cd->sc_link,
		    (*(int *)addr) ? PR_PREVENT : PR_ALLOW, 0);
	case CDIOCSETDEBUG:
		cd->sc_link->flags |= (SDEV_DB1 | SDEV_DB2);
		return 0;
	case CDIOCCLRDEBUG:
		cd->sc_link->flags &= ~(SDEV_DB1 | SDEV_DB2);
		return 0;
#ifdef CDDA
	case CDIOCSETCDDA: {
		int onoff = *(int *)addr;
		/* select CD-DA mode */
		struct scsi_cd_mode_data data;

		if (CDPART(dev) != RAW_PART)
			return ENOTTY;
		if ((error = cd_get_mode(cd, &data, AUDIO_PAGE)) != 0)
			return error;
		if (onoff) {
			/* turn it on */
			if (cd->flags & CDF_CDDAMODE)
				return EALREADY;
			/*
			 * do not permit changing of block size if the block
			 * device is open, or if some other partition (not
			 * a raw partition) is open.
			 */
			if (cd->sc_dk.dk_bopenmask ||
			    (cd->sc_dk.dk_copenmask & ~(1 << RAW_PART)))
				return EBUSY;

			data.blk_desc.density = CD_DA_DENSITY_CODE;
			data.blk_desc.blklen[0] = (CD_DA_BLKSIZ >> 16) & 0xff;
			data.blk_desc.blklen[1] = (CD_DA_BLKSIZ >> 8) & 0xff;
			data.blk_desc.blklen[2] = CD_DA_BLKSIZ & 0xff;

			if (cd_set_mode(cd, &data) != 0) 
				return EIO;

			cd->orig_params = cd->params;
			cd->flags |= CDF_CDDAMODE;
		} else {
			if (!(cd->flags & CDF_CDDAMODE))
				return EALREADY;
			/* turn it off */
			data.blk_desc.density = CD_NORMAL_DENSITY_CODE;
			data.blk_desc.blklen[0] = (cd->orig_params.blksize >> 16) & 0xff;
			data.blk_desc.blklen[1] = (cd->orig_params.blksize >> 8) & 0xff;
			data.blk_desc.blklen[2] = (cd->orig_params.blksize) & 0xff;

			if (cd_set_mode(cd, &data) != 0)
				return EIO;

			cd->flags &= ~CDF_CDDAMODE;
			cd->params = cd->orig_params;
		}
		if (cd_get_parms(cd, 0) != 0) {
			data.blk_desc.density = CD_NORMAL_DENSITY_CODE;
			data.blk_desc.blklen[0] = (cd->orig_params.blksize >> 16) & 0xff;
			data.blk_desc.blklen[1] = (cd->orig_params.blksize >> 8) & 0xff;
			data.blk_desc.blklen[2] = (cd->orig_params.blksize) & 0xff;
			(void) cd_set_mode(cd, &data); /* it better work...! */
			cd->params = cd->orig_params;
			return EIO;
		}
		return 0;
	}
#endif

	case CDIOCRESET:
	case SCIOCRESET:
		return (cd_reset(cd));
	case CDIOCLOADUNLOAD: {
		struct ioc_load_unload *args = (struct ioc_load_unload *)addr;

		return ((*cd->sc_ops->cdo_load_unload)(cd, args->options,
			args->slot));
	}

	case DVD_AUTH:
		return (dvd_auth(cd, (dvd_authinfo *)addr));

	case DVD_READ_STRUCT:
		return (dvd_read_struct(cd, (dvd_struct *)addr));
	
	default:
		if (CDPART(dev) != RAW_PART)
			return ENOTTY;
		return scsi_do_ioctl(cd->sc_link, dev, cmd, addr, flag, p);
	}

#ifdef DIAGNOSTIC
	panic("cdioctl: impossible");
#endif
}

/*
 * Load the label information on the named device
 * Actually fabricate a disklabel
 *
 * EVENTUALLY take information about different
 * data tracks from the TOC and put it in the disklabel
 */
void
cdgetdisklabel(dev, cd, lp, clp, spoofonly)
	dev_t dev;
	struct cd_softc *cd;
	struct disklabel *lp;
	struct cpu_disklabel *clp;
	int spoofonly;
{
	char *errstring;
	u_int8_t hdr[TOC_HEADER_SZ],  *ent, *toc = NULL;
	u_int32_t lba, nlba;
	int tocidx, i, n, len, is_data, data_track = -1;

	bzero(lp, sizeof(struct disklabel));
	bzero(clp, sizeof(struct cpu_disklabel));

	lp->d_secsize = cd->params.blksize;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_ncylinders = (cd->params.disksize / 100) + 1;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	if (lp->d_secpercyl == 0) {
		lp->d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it */
	}

	if (cd->sc_link->flags & SDEV_ATAPI) {
		strncpy(lp->d_typename, "ATAPI CD-ROM", sizeof(lp->d_typename) - 1);
		lp->d_type = DTYPE_ATAPI;
	} else {
		strncpy(lp->d_typename, "SCSI CD-ROM", sizeof(lp->d_typename) - 1);
		lp->d_type = DTYPE_SCSI;
	}

	strncpy(lp->d_packname, "fictitious", sizeof(lp->d_packname) - 1);
	lp->d_secperunit = cd->params.disksize;
	lp->d_rpm = 300;
	lp->d_interleave = 1;
	lp->d_flags = D_REMOVABLE;

	/* XXX - these values for BBSIZE and SBSIZE assume ffs */
	lp->d_bbsize = BBSIZE;
	lp->d_sbsize = SBSIZE;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/* The raw partition is special.  */
	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size = lp->d_secperunit;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;
	lp->d_npartitions = RAW_PART + 1;

	/*
	 * Read the TOC and loop throught the individual tracks and lay them
	 * out in our disklabel.  If there is a data track, call the generic
	 * disklabel read routine.  XXX should we move all data tracks up front
	 * before any other tracks?
	 */
	if (cd_read_toc(cd, 0, 0, hdr, TOC_HEADER_SZ, 0))
		return;
	n = hdr[TOC_HEADER_ENDING_TRACK] - hdr[TOC_HEADER_STARTING_TRACK] + 1;
	/* n + 1 because of leadout track */
	len = TOC_HEADER_SZ + (n + 1) * TOC_ENTRY_SZ;
	MALLOC(toc, u_int8_t *, len, M_TEMP, M_WAITOK);
	if (cd_read_toc (cd, CD_LBA_FORMAT, 0, toc, len, 0))
		goto done;

	/* Create the partition table.  */
	ent = toc + TOC_HEADER_SZ;
	lba = ((cd->sc_link->quirks & ADEV_LITTLETOC) ?
	    ent[TOC_ENTRY_MSF_LBA] | ent[TOC_ENTRY_MSF_LBA + 1] << 8 |
	    ent[TOC_ENTRY_MSF_LBA + 2] << 16 |
	    ent[TOC_ENTRY_MSF_LBA + 3] << 24 :
	    ent[TOC_ENTRY_MSF_LBA] << 24 | ent[TOC_ENTRY_MSF_LBA + 1] << 16 |
	    ent[TOC_ENTRY_MSF_LBA + 2] << 8 | ent[TOC_ENTRY_MSF_LBA + 3]);

	i = 0;
	for (tocidx = hdr[TOC_HEADER_STARTING_TRACK]; 
	     tocidx <= hdr[TOC_HEADER_ENDING_TRACK]; 
	     tocidx++) {
		is_data = ent[TOC_ENTRY_CONTROL_ADDR_TYPE] & 4;
		ent += TOC_ENTRY_SZ;
		nlba = ((cd->sc_link->quirks & ADEV_LITTLETOC) ?
			ent[TOC_ENTRY_MSF_LBA] |
			ent[TOC_ENTRY_MSF_LBA + 1] << 8 |
			ent[TOC_ENTRY_MSF_LBA + 2] << 16 |
			ent[TOC_ENTRY_MSF_LBA + 3] << 24 :
			ent[TOC_ENTRY_MSF_LBA] << 24 |
			ent[TOC_ENTRY_MSF_LBA + 1] << 16 |
			ent[TOC_ENTRY_MSF_LBA + 2] << 8 |
			ent[TOC_ENTRY_MSF_LBA + 3]);

		lp->d_partitions[i].p_fstype =
			is_data ? FS_UNUSED : FS_OTHER;
		lp->d_partitions[i].p_offset = lba;
		lp->d_partitions[i].p_size = nlba - lba;
		lba = nlba;

		if (is_data) { 
			if (data_track == -1)
				data_track = i;

			i++;
			if (i == RAW_PART)
				i++;

			if (i >= MAXPARTITIONS)
				break;
		}
	}

	if (i < MAXPARTITIONS)
		bzero(&lp->d_partitions[i], sizeof(lp->d_partitions[i]));

	lp->d_npartitions = max(RAW_PART, i - 1) + 1;

done:
	if (toc)
		FREE(toc, M_TEMP);

	/* We have a data track, look in there for a real disklabel.  */
	if (data_track != -1) {
#if 0
		/* This might not necessarily work */
		errstring = readdisklabel(MAKECDDEV(major(dev), CDUNIT(dev), 
		    data_track), cdstrategy, lp, clp, spoofonly);
#else
		errstring = readdisklabel(CDLABELDEV(dev),
		    cdstrategy, lp, clp, spoofonly);
#endif
		/*if (errstring)
			printf("%s: %s\n", cd->sc_dev.dv_xname, errstring);*/
	}
}

/*
 * Find out from the device what it's capacity is
 */
u_long
cd_size(cd, flags)
	struct cd_softc *cd;
	int flags;
{
	struct scsi_read_cd_cap_data rdcap;
	struct scsi_read_cd_capacity scsi_cmd;
	int blksize;
	u_long size;

	if (cd->sc_link->quirks & ADEV_NOCAPACITY) {
		/*
		 * the drive doesn't support the READ_CD_CAPACITY command
		 * use a fake size
		 */
		cd->params.blksize = 2048;
		cd->params.disksize = 400000;
		return (400000);
	}

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
		return (0);

	blksize = _4btol(rdcap.length);
	if ((blksize < 512) || ((blksize & 511) != 0))
		blksize = 2048;	/* some drives lie ! */
	cd->params.blksize = blksize;

	size = _4btol(rdcap.addr) + 1;
	if (size < 100)
		size = 400000;	/* ditto */
	cd->params.disksize = size;

	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cd_size: %d %ld\n", blksize, size));
	return (size);
}


/*
 * Get scsi driver to send a "start playing" command
 */
int
cd_play(cd, blkno, nblks)
	struct cd_softc *cd;
	int blkno, nblks;
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
cd_play_tracks(cd, strack, sindex, etrack, eindex)
	struct cd_softc *cd;
	int strack, sindex, etrack, eindex;
{
	struct cd_toc toc;
	int error;

	if (!etrack)
		return (EIO);
	if (strack > etrack)
		return (EINVAL);

	if ((error = cd_load_toc(cd, &toc)) != 0)
		return (error);

	if (++etrack > (toc.header.ending_track+1))
		etrack = toc.header.ending_track+1;

	strack -= toc.header.starting_track;
	etrack -= toc.header.starting_track;
	if (strack < 0)
		return (EINVAL);

	return (cd_play_msf(cd, toc.entries[strack].addr.msf.minute,
	    toc.entries[strack].addr.msf.second,
	    toc.entries[strack].addr.msf.frame,
	    toc.entries[etrack].addr.msf.minute,
	    toc.entries[etrack].addr.msf.second,
	    toc.entries[etrack].addr.msf.frame));
}

/*
 * Get scsi driver to send a "play msf" command
 */
int
cd_play_msf(cd, startm, starts, startf, endm, ends, endf)
	struct cd_softc *cd;
	int startm, starts, startf, endm, ends, endf;
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
cd_pause(cd, go)
	struct cd_softc *cd;
	int go;
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
cd_reset(cd)
	struct cd_softc *cd;
{

	return scsi_scsi_cmd(cd->sc_link, 0, 0, 0, 0, CDRETRIES, 2000, NULL,
	    SCSI_RESET);
}

/*
 * Read subchannel
 */
int
cd_read_subchannel(cd, mode, format, track, data, len)
	struct cd_softc *cd;
	int mode, format, track, len;
	struct cd_sub_channel_info *data;
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
cd_read_toc(cd, mode, start, data, len, control)
	struct cd_softc *cd;
	int mode, start, len, control;
	void *data;
{
	struct scsi_read_toc scsi_cmd;
	int ntoc;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
#if 0
	if (len!=sizeof(struct ioc_toc_header))
		ntoc=((len)-sizeof(struct ioc_toc_header))/sizeof(struct cd_toc_entry);
	else 
#endif
	ntoc = len;
	scsi_cmd.opcode = READ_TOC;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.from_track = start;
	_lto2b(ntoc, scsi_cmd.data_len);
	scsi_cmd.control = control;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(struct scsi_read_toc), (u_char *)data, len, CDRETRIES,
	    5000, NULL, SCSI_DATA_IN);
}

int
cd_load_toc(cd, toc)
	struct cd_softc *cd;
	struct cd_toc *toc;
{
	int ntracks, len, error;

	if ((error = cd_read_toc(cd, 0, 0, toc, sizeof(toc->header), 0)) != 0)
		return (error);

	ntracks = toc->header.ending_track - toc->header.starting_track + 1;
	len = (ntracks + 1) * sizeof(struct cd_toc_entry) +
	    sizeof(toc->header);
	if ((error = cd_read_toc(cd, CD_MSF_FORMAT, 0, toc, len, 0)) != 0)
		return (error);
	return (0);
}


/*
 * Get the scsi driver to send a full inquiry to the device and use the
 * results to fill out the disk parameter structure.
 */
int
cd_get_parms(cd, flags)
	struct cd_softc *cd;
	int flags;
{
	/*
	 * give a number of sectors so that sec * trks * cyls
	 * is <= disk_size
	 */
	if (cd_size(cd, flags) == 0)
		return (ENXIO);
	return (0);
}

int
cdsize(dev)
	dev_t dev;
{

	/* CD-ROMs are read-only. */
	return -1;
}

int
cddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}

#define	dvd_copy_key(dst, src)		bcopy((src), (dst), sizeof(dvd_key))
#define	dvd_copy_challenge(dst, src)	bcopy((src), (dst), sizeof(dvd_challenge))

int
dvd_auth(cd, a)
	struct cd_softc *cd;
	dvd_authinfo *a;
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
				      CDRETRIES, 30000, NULL,
				      SCSI_DATA_OUT|SCSI_DATA_IN);
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
				      CDRETRIES, 30000, NULL,
				      SCSI_DATA_OUT|SCSI_DATA_IN);
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

	default:
		return (ENOTTY);
	}
}

int
dvd_read_physical(cd, s)
	struct cd_softc *cd;
	dvd_struct *s;
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
	dvd_struct *s;
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
	dvd_struct *s;
{
	struct scsi_generic cmd;
	u_int8_t buf[4 + 2048];
	int error;

	bzero(cmd.bytes, sizeof(cmd.bytes));
	bzero(buf, sizeof(buf));
	cmd.opcode = GPCMD_READ_DVD_STRUCTURE;
	cmd.bytes[6] = s->type;
	_lto2b(sizeof(buf), &cmd.bytes[7]);

	cmd.bytes[9] = s->disckey.agid << 6;
	error = scsi_scsi_cmd(cd->sc_link, &cmd, sizeof(cmd), buf, sizeof(buf),
			      CDRETRIES, 30000, NULL, SCSI_DATA_IN);
	if (error)
		return (error);
	bcopy(&buf[4], s->disckey.value, 2048);
	return (0);
}

int
dvd_read_bca(cd, s)
	struct cd_softc *cd;
	dvd_struct *s;
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
	dvd_struct *s;
{
	struct scsi_generic cmd;
	u_int8_t buf[4 + 2048];
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
	s->manufact.len = _2btol(&buf[0]);
	if (s->manufact.len < 0 || s->manufact.len > 2048)
		return (EIO);
	bcopy(&buf[4], s->manufact.value, s->manufact.len);
	return (0);
}

int
dvd_read_struct(cd, s)
	struct cd_softc *cd;
	dvd_struct *s;
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
