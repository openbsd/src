/*	$OpenBSD: st.c,v 1.74 2007/06/01 18:44:48 krw Exp $	*/
/*	$NetBSD: st.c,v 1.71 1997/02/21 23:03:49 thorpej Exp $	*/

/*
 * Copyright (c) 1994 Charles Hannum.  All rights reserved.
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
 *	This product includes software developed by Charles Hannum.
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
 * major changes by Julian Elischer (julian@jules.dialix.oz.au) May 1993
 */

/*
 * To do:
 * work out some better way of guessing what a good timeout is going
 * to be depending on whether we expect to retension or not.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/timeout.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/mtio.h>
#include <sys/device.h>
#include <sys/conf.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_tape.h>
#include <scsi/scsiconf.h>

/* Defines for device specific stuff */
#define DEF_FIXED_BSIZE  512
#define	ST_RETRIES	4	/* only on non IO commands */

#define STMODE(z)	( minor(z)	 & 0x03)
#define STUNIT(z)	((minor(z) >> 4)       )

#define	ST_IO_TIME	(3 * 60 * 1000)		/* 3 minutes */
#define	ST_CTL_TIME	(30 * 1000)		/* 30 seconds */
#define	ST_SPC_TIME	(4 * 60 * 60 * 1000)	/* 4 hours */

/*
 * Maximum density code allowed in SCSI spec (SSC2R08f, Section 8.3).
 */
#define SCSI_MAX_DENSITY_CODE		0xff

/*
 * Define various devices that we know mis-behave in some way,
 * and note how they are bad, so we can correct for them
 */
struct modes {
	u_int quirks;			/* same definitions as in quirkdata */
	int blksize;
	u_int8_t density;
};

struct quirkdata {
	u_int quirks;
#define	ST_Q_FORCE_BLKSIZE	0x0001
#define	ST_Q_SENSE_HELP		0x0002	/* must do READ for good MODE SENSE */
#define	ST_Q_IGNORE_LOADS	0x0004
#define	ST_Q_BLKSIZE		0x0008	/* variable-block media_blksize > 0 */
#define	ST_Q_UNIMODAL		0x0010	/* unimode drive rejects mode select */
	struct modes modes;
};

struct st_quirk_inquiry_pattern {
	struct scsi_inquiry_pattern pattern;
	struct quirkdata quirkdata;
};

const struct st_quirk_inquiry_pattern st_quirk_patterns[] = {
	{{T_SEQUENTIAL, T_REMOV,
	 "        ", "                ", "    "}, {0,
		{ST_Q_FORCE_BLKSIZE, 512, 0}}},		/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "TANDBERG", " TDC 3600       ", ""},     {0,
		{0, 0, 0}}},				/* minor 0-3 */
 	{{T_SEQUENTIAL, T_REMOV,
 	 "TANDBERG", " TDC 3800       ", ""},     {0,
		{ST_Q_FORCE_BLKSIZE, 512, 0}}},		/* minor 0-3 */
	/*
	 * At least -005 and -007 need this.  I'll assume they all do unless I
	 * hear otherwise.  - mycroft, 31MAR1994
	 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 2525 25462", ""},     {0,
		{ST_Q_SENSE_HELP, 0, 0}}},		/* minor 0-3 */
	/*
	 * One user reports that this works for his tape drive.  It probably
	 * needs more work.  - mycroft, 09APR1994
	 */
	{{T_SEQUENTIAL, T_REMOV,
	 "SANKYO  ", "CP525           ", ""},    {0,
		{ST_Q_FORCE_BLKSIZE, 512, 0}}},		/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ANRITSU ", "DMT780          ", ""},     {0,
		{ST_Q_FORCE_BLKSIZE, 512, 0}}},		/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 150  21247", ""},     {0,
		{0, 0, 0}}},				/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "ARCHIVE ", "VIPER 150  21531", ""},     {0,
		{ST_Q_SENSE_HELP, 0, 0}}},		/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5099ES SCSI", ""},          {0,
		{ST_Q_FORCE_BLKSIZE, 512, 0}}},		/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5150ES SCSI", ""},          {0,
		{ST_Q_FORCE_BLKSIZE, 512, 0}}},		/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5525ES SCSI REV7", ""},     {0,
		{0, 0, 0}}},				/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "WangDAT ", "Model 1300      ", ""},     {0,
		{0, 0, 0}}},				/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "EXABYTE ", "EXB-8200        ", "263H"}, {0,
		{0, 0, 0}}},				/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "HP      ", "T4000s          ", ""},     {ST_Q_UNIMODAL,
		{0, 0, QIC_3095}}},			/* minor 0-3 */
#if 0
	{{T_SEQUENTIAL, T_REMOV,
	 "EXABYTE ", "EXB-8200        ", ""},     {0,
		{0, 0, 0}}},				/* minor 0-3 */
#endif
	{{T_SEQUENTIAL, T_REMOV,
	 "WANGTEK ", "5150ES SCSI FA15\0""01 A", "????"}, {0,
		{ST_Q_IGNORE_LOADS, 0, 0}}},		/* minor 0-3 */
	{{T_SEQUENTIAL, T_REMOV,
	 "TEAC    ", "MT-2ST/N50      ", ""},     {ST_Q_IGNORE_LOADS,
		{0, 0, 0}}},				/* minor 0-3 */
};

#define NOEJECT 0
#define EJECT 1

#define NOREWIND 0
#define DOREWIND 1

struct st_softc {
	struct device sc_dev;
/*--------------------present operating parameters, flags etc.----------------*/
	int flags;		/* see below                          */
	u_int quirks;		/* quirks for the open mode           */
	int blksize;		/* blksize we are using               */
	u_int8_t density;	/* present density                    */
	short mt_resid;		/* last (short) resid                 */
	short mt_erreg;		/* last error (sense key) seen        */
/*--------------------device/scsi parameters----------------------------------*/
	struct scsi_link *sc_link;	/* our link to the adpter etc.        */
/*--------------------parameters reported by the device ----------------------*/
	int blkmin;		/* min blk size                       */
	int blkmax;		/* max blk size                       */
	const struct quirkdata *quirkdata;	/* if we have a rogue entry */
/*--------------------parameters reported by the device for this media--------*/
	u_int64_t numblks;		/* nominal blocks capacity            */
	u_int32_t media_blksize;	/* 0 if not ST_FIXEDBLOCKS            */
	u_int32_t media_density;	/* this is what it said when asked    */
	int media_fileno;		/* relative to BOT. -1 means unknown. */
	int media_blkno;		/* relative to BOF. -1 means unknown. */
/*--------------------quirks for the whole drive------------------------------*/
	u_int drive_quirks;	/* quirks of this drive               */
/*--------------------How we should set up when opening each minor device----*/
	struct modes modes;	/* plus more for each mode            */
	u_int8_t  modeflags;	/* flags for the modes                */
#define DENSITY_SET_BY_USER	0x01
#define DENSITY_SET_BY_QUIRK	0x02
#define BLKSIZE_SET_BY_USER	0x04
#define BLKSIZE_SET_BY_QUIRK	0x08
/*--------------------storage for sense data returned by the drive------------*/
	struct buf buf_queue;		/* the queue of pending IO operations */
	struct timeout sc_timeout;
};


int	stmatch(struct device *, void *, void *);
void	stattach(struct device *, struct device *, void *);
void	st_identify_drive(struct st_softc *, struct scsi_inquiry_data *);
void	st_loadquirks(struct st_softc *);
int	st_mount_tape(dev_t, int);
void	st_unmount(struct st_softc *, int, int);
int	st_decide_mode(struct st_softc *, int);
void	ststart(void *);
void	strestart(void *);
int	st_read(struct st_softc *, char *, int, int);
int	st_read_block_limits(struct st_softc *, int);
int	st_mode_sense(struct st_softc *, int);
int	st_mode_select(struct st_softc *, int);
int	st_space(struct st_softc *, int, u_int, int);
int	st_write_filemarks(struct st_softc *, int, int);
int	st_check_eod(struct st_softc *, int, int *, int);
int	st_load(struct st_softc *, u_int, int);
int	st_rewind(struct st_softc *, u_int, int);
int	st_interpret_sense(struct scsi_xfer *);
int	st_touch_tape(struct st_softc *);
int	st_erase(struct st_softc *, int, int);

struct cfattach st_ca = {
	sizeof(struct st_softc), stmatch, stattach
};

struct cfdriver st_cd = {
	NULL, "st", DV_TAPE
};

struct scsi_device st_switch = {
	st_interpret_sense,
	ststart,
	NULL,
	NULL,
};

#define	ST_INFO_VALID	0x0001
#define	ST_BLOCK_SET	0x0002	/* block size, mode set by ioctl      */
#define	ST_WRITTEN	0x0004	/* data have been written, EOD needed */
#define	ST_FIXEDBLOCKS	0x0008
#define	ST_AT_FILEMARK	0x0010
#define	ST_EIO_PENDING	0x0020	/* we couldn't report it then (had data) */
#define	ST_READONLY	0x0080	/* st_mode_sense says write protected */
#define	ST_FM_WRITTEN	0x0100	/*
				 * EOF file mark written  -- used with
				 * ~ST_WRITTEN to indicate that multiple file
				 * marks have been written
				 */
#define	ST_BLANK_READ	0x0200	/* BLANK CHECK encountered already */
#define	ST_2FM_AT_EOD	0x0400	/* write 2 file marks at EOD */
#define	ST_MOUNTED	0x0800	/* Device is presently mounted */
#define	ST_DONTBUFFER	0x1000	/* Disable buffering/caching */

#define	ST_PER_ACTION	(ST_AT_FILEMARK | ST_EIO_PENDING | ST_BLANK_READ)
#define	ST_PER_MOUNT	(ST_INFO_VALID | ST_BLOCK_SET | ST_WRITTEN | \
			 ST_FIXEDBLOCKS | ST_READONLY | ST_FM_WRITTEN | \
			 ST_2FM_AT_EOD | ST_PER_ACTION)

const struct scsi_inquiry_pattern st_patterns[] = {
	{T_SEQUENTIAL, T_REMOV,
	 "",         "",                 ""},
};

int
stmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct scsi_attach_args *sa = aux;
	int priority;

	(void)scsi_inqmatch(sa->sa_inqbuf,
	    st_patterns, sizeof(st_patterns)/sizeof(st_patterns[0]),
	    sizeof(st_patterns[0]), &priority);
	return (priority);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 */
void
stattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct st_softc *st = (void *)self;
	struct scsi_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("stattach:\n"));

	/*
	 * Store information needed to contact our base driver
	 */
	st->sc_link = sc_link;
	sc_link->device = &st_switch;
	sc_link->device_softc = st;

	/*
	 * Check if the drive is a known criminal and take
	 * Any steps needed to bring it into line
	 */
	st_identify_drive(st, sa->sa_inqbuf);
	printf("\n");

	timeout_set(&st->sc_timeout, strestart, st);

	/*
	 * Set up the buf queue for this device
	 */
	st->buf_queue.b_active = 0;
	st->buf_queue.b_actf = 0;
	st->buf_queue.b_actb = &st->buf_queue.b_actf;

	/* Start up with media position unknown. */
	st->media_fileno = -1;
	st->media_blkno = -1;

	/*
	 * Reset the media loaded flag, sometimes the data
	 * acquired at boot time is not quite accurate.  This
	 * will be checked again at the first open.
	 */
	sc_link->flags &= ~SDEV_MEDIA_LOADED;
}

/*
 * Use the inquiry routine in 'scsi_base' to get drive info so we can
 * Further tailor our behaviour.
 */
void
st_identify_drive(st, inqbuf)
	struct st_softc *st;
	struct scsi_inquiry_data *inqbuf;
{
	const struct st_quirk_inquiry_pattern *finger;
	int priority;

	finger = (const struct st_quirk_inquiry_pattern *)scsi_inqmatch(inqbuf,
	    st_quirk_patterns,
	    sizeof(st_quirk_patterns)/sizeof(st_quirk_patterns[0]),
	    sizeof(st_quirk_patterns[0]), &priority);
	if (priority != 0) {
		st->quirkdata = &finger->quirkdata;
		st->drive_quirks = finger->quirkdata.quirks;
		st->quirks = finger->quirkdata.quirks;	/* start value */
		st_loadquirks(st);
	}
}

/*
 * initialise the subdevices to the default (QUIRK) state.
 * this will remove any setting made by the system operator or previous
 * operations.
 */
void
st_loadquirks(st)
	struct st_softc *st;
{
	const struct	modes *mode;
	struct	modes *mode2;

	mode = &st->quirkdata->modes;
	mode2 = &st->modes;
	bzero(mode2, sizeof(struct modes));
	st->modeflags &= ~(BLKSIZE_SET_BY_QUIRK |
	    DENSITY_SET_BY_QUIRK | BLKSIZE_SET_BY_USER |
	    DENSITY_SET_BY_USER);
	if ((mode->quirks | st->drive_quirks) & ST_Q_FORCE_BLKSIZE) {
		mode2->blksize = mode->blksize;
		st->modeflags |= BLKSIZE_SET_BY_QUIRK;
	}
	if (mode->density) {
		mode2->density = mode->density;
		st->modeflags |= DENSITY_SET_BY_QUIRK;
	}
}

/*
 * open the device.
 */
int
stopen(dev, flags, fmt, p)
	dev_t dev;
	int flags;
	int fmt;
	struct proc *p;
{
	struct scsi_link *sc_link;
	struct st_softc *st;
	int error = 0, unit;

	unit = STUNIT(dev);
	if (unit >= st_cd.cd_ndevs)
		return (ENXIO);
	st = st_cd.cd_devs[unit];
	if (!st)
		return (ENXIO);

	sc_link = st->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("open: dev=0x%x (unit %d (of %d))\n", dev,
	    unit, st_cd.cd_ndevs));

	/*
	 * Tape is an exclusive media. Only one open at a time.
	 */
	if (sc_link->flags & SDEV_OPEN) {
		SC_DEBUG(sc_link, SDEV_DB4, ("already open\n"));
		return (EBUSY);
	}

	/* Use st_interpret_sense() now. */
	sc_link->flags |= SDEV_OPEN;

	/*
	 * Check the unit status. This clears any outstanding errors and
	 * will ensure that media is present.
	 */
	error = scsi_test_unit_ready(sc_link, TEST_READY_RETRIES,
	    SCSI_SILENT | SCSI_IGNORE_MEDIA_CHANGE |
	    SCSI_IGNORE_ILLEGAL_REQUEST);

	/*
	 * Terminate any exising mount session if there is no media.
	 */
	if ((sc_link->flags & SDEV_MEDIA_LOADED) == 0)
		st_unmount(st, NOEJECT, DOREWIND);

	if (error) {
		sc_link->flags &= ~SDEV_OPEN;
		return (error);
	}

	if ((st->flags & ST_MOUNTED) == 0) {
		error = st_mount_tape(dev, flags);
		if (error) {
			sc_link->flags &= ~SDEV_OPEN;
			return (error);
		}
	}

	/*
	 * Make sure that a tape opened in write-only mode will have
	 * file marks written on it when closed, even if not written to.
	 * This is for SUN compatibility
	 */
	if ((flags & O_ACCMODE) == FWRITE)
		st->flags |= ST_WRITTEN;

	SC_DEBUG(sc_link, SDEV_DB2, ("open complete\n"));
	return (0);
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
int
stclose(dev, flags, mode, p)
	dev_t dev;
	int flags;
	int mode;
	struct proc *p;
{
	struct st_softc *st = st_cd.cd_devs[STUNIT(dev)];

	SC_DEBUG(st->sc_link, SDEV_DB1, ("closing\n"));
	if ((st->flags & (ST_WRITTEN | ST_FM_WRITTEN)) == ST_WRITTEN)
		st_write_filemarks(st, 1, 0);
	switch (STMODE(dev)) {
	case 0:		/* normal */
		st_unmount(st, NOEJECT, DOREWIND);
		break;
	case 3:		/* eject, no rewind */
		st_unmount(st, EJECT, NOREWIND);
		break;
	case 1:		/* no rewind */
		/* leave mounted unless media seems to have been removed */
		if (!(st->sc_link->flags & SDEV_MEDIA_LOADED))
			st_unmount(st, NOEJECT, NOREWIND);
		break;
	case 2:		/* rewind, eject */
		st_unmount(st, EJECT, DOREWIND);
		break;
	}
	st->sc_link->flags &= ~SDEV_OPEN;
	timeout_del(&st->sc_timeout);

	return 0;
}

/*
 * Start a new mount session.
 * Copy in all the default parameters from the selected device mode.
 * and try guess any that seem to be defaulted.
 */
int
st_mount_tape(dev, flags)
	dev_t dev;
	int flags;
{
	int unit;
	u_int mode;
	struct st_softc *st;
	struct scsi_link *sc_link;
	int error = 0;

	unit = STUNIT(dev);
	mode = STMODE(dev);
	st = st_cd.cd_devs[unit];
	sc_link = st->sc_link;

	if (st->flags & ST_MOUNTED)
		return 0;

	SC_DEBUG(sc_link, SDEV_DB1, ("mounting\n"));
	st->quirks = st->drive_quirks | st->modes.quirks;
	/*
	 * If the media is new, then make sure we give it a chance to
	 * to do a 'load' instruction.  (We assume it is new.)
	 */
	if ((error = st_load(st, LD_LOAD, 0)) != 0)
		return error;
	/*
	 * Throw another dummy instruction to catch
	 * 'Unit attention' errors. Some drives appear to give
	 * these after doing a Load instruction.
	 * (noteably some DAT drives)
	 */
	/* XXX */
	scsi_test_unit_ready(sc_link, TEST_READY_RETRIES, SCSI_SILENT);

	/*
	 * Some devices can't tell you much until they have been
	 * asked to look at the media. This quirk does this.
	 */
	if (st->quirks & ST_Q_SENSE_HELP)
		if ((error = st_touch_tape(st)) != 0)
			return error;
	/*
	 * Load the physical device parameters
	 * loads: blkmin, blkmax
	 */
	if (!(sc_link->flags & SDEV_ATAPI) &&
	    (error = st_read_block_limits(st, 0)) != 0) {
		return error;
	}

	/*
	 * Load the media dependent parameters
	 * includes: media_blksize,media_density,numblks
	 * As we have a tape in, it should be reflected here.
	 * If not you may need the "quirk" above.
	 */
	if ((error = st_mode_sense(st, 0)) != 0)
		return error;

	/*
	 * If we have gained a permanent density from somewhere,
	 * then use it in preference to the one supplied by
	 * default by the driver.
	 */
	if (st->modeflags & (DENSITY_SET_BY_QUIRK | DENSITY_SET_BY_USER))
		st->density = st->modes.density;
	else
		st->density = st->media_density;
	/*
	 * If we have gained a permanent blocksize
	 * then use it in preference to the one supplied by
	 * default by the driver.
	 */
	st->flags &= ~ST_FIXEDBLOCKS;
	if (st->modeflags & (BLKSIZE_SET_BY_QUIRK | BLKSIZE_SET_BY_USER)) {
		st->blksize = st->modes.blksize;
		if (st->blksize)
			st->flags |= ST_FIXEDBLOCKS;
	} else {
		if ((error = st_decide_mode(st, FALSE)) != 0)
			return error;
	}
	if ((error = st_mode_select(st, 0)) != 0) {
		printf("%s: cannot set selected mode\n", st->sc_dev.dv_xname);
		return error;
	}
	scsi_prevent(sc_link, PR_PREVENT,
	    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY);
	st->flags |= ST_MOUNTED;
	sc_link->flags |= SDEV_MEDIA_LOADED;	/* move earlier? */

	return 0;
}

/*
 * End the present mount session.
 * Rewind, and optionally eject the tape.
 * Reset various flags to indicate that all new
 * operations require another mount operation
 */
void
st_unmount(st, eject, rewind)
	struct st_softc *st;
	int eject, rewind;
{
	struct scsi_link *sc_link = st->sc_link;
	int nmarks;

	st->media_fileno = -1;
	st->media_blkno = -1;

	if (!(st->flags & ST_MOUNTED))
		return;
	SC_DEBUG(sc_link, SDEV_DB1, ("unmounting\n"));
	st_check_eod(st, FALSE, &nmarks, SCSI_IGNORE_NOT_READY);
	if (rewind)
		st_rewind(st, 0, SCSI_IGNORE_NOT_READY);
	scsi_prevent(sc_link, PR_ALLOW,
	    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_NOT_READY);
	if (eject)
		st_load(st, LD_UNLOAD, SCSI_IGNORE_NOT_READY);
	st->flags &= ~ST_MOUNTED;
	sc_link->flags &= ~SDEV_MEDIA_LOADED;
}

/*
 * Given all we know about the device, media, mode, 'quirks' and
 * initial operation, make a decision as to how we should be set
 * to run (regarding blocking and EOD marks)
 */
int
st_decide_mode(st, first_read)
	struct st_softc *st;
	int	first_read;
{
	struct scsi_link *sc_link = st->sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("starting block mode decision\n"));

	/* ATAPI tapes are always fixed blocksize. */
	if (sc_link->flags & SDEV_ATAPI) {
		st->flags |= ST_FIXEDBLOCKS;
		if (st->media_blksize > 0)
			st->blksize = st->media_blksize;
		else
			st->blksize = DEF_FIXED_BSIZE;
		goto done;
	}

	/*
	 * If the drive can only handle fixed-length blocks and only at
	 * one size, perhaps we should just do that.
	 */
	if (st->blkmin && (st->blkmin == st->blkmax)) {
		st->flags |= ST_FIXEDBLOCKS;
		st->blksize = st->blkmin;
		SC_DEBUG(sc_link, SDEV_DB3,
		    ("blkmin == blkmax of %d\n", st->blkmin));
		goto done;
	}
	/*
	 * If the tape density mandates (or even suggests) use of fixed
	 * or variable-length blocks, comply.
	 */
	switch (st->density) {
	case HALFINCH_800:
	case HALFINCH_1600:
	case HALFINCH_6250:
	case DDS:
		st->flags &= ~ST_FIXEDBLOCKS;
		st->blksize = 0;
		SC_DEBUG(sc_link, SDEV_DB3, ("density specified variable\n"));
		goto done;
	case QIC_11:
	case QIC_24:
	case QIC_120:
	case QIC_150:
	case QIC_525:
	case QIC_1320:
		st->flags |= ST_FIXEDBLOCKS;
		if (st->media_blksize > 0)
			st->blksize = st->media_blksize;
		else
			st->blksize = DEF_FIXED_BSIZE;
		SC_DEBUG(sc_link, SDEV_DB3, ("density specified fixed\n"));
		goto done;
	}
	/*
	 * If we're about to read the tape, perhaps we should choose
	 * fixed or variable-length blocks and block size according to
	 * what the drive found on the tape.
	 */
	if (first_read &&
	    (!(st->quirks & ST_Q_BLKSIZE) || (st->media_blksize == 0) ||
	    (st->media_blksize == DEF_FIXED_BSIZE) ||
	    (st->media_blksize == 1024))) {
		if (st->media_blksize > 0)
			st->flags |= ST_FIXEDBLOCKS;
		else
			st->flags &= ~ST_FIXEDBLOCKS;
		st->blksize = st->media_blksize;
		SC_DEBUG(sc_link, SDEV_DB3,
		    ("Used media_blksize of %d\n", st->media_blksize));
		goto done;
	}
	/*
	 * We're getting no hints from any direction.  Choose variable-
	 * length blocks arbitrarily.
	 */
	st->flags &= ~ST_FIXEDBLOCKS;
	st->blksize = 0;
	SC_DEBUG(sc_link, SDEV_DB3,
	    ("Give up and default to variable mode\n"));

done:
	/*
	 * Decide whether or not to write two file marks to signify end-
	 * of-data.  Make the decision as a function of density.  If
	 * the decision is not to use a second file mark, the SCSI BLANK
	 * CHECK condition code will be recognized as end-of-data when
	 * first read.
	 * (I think this should be a by-product of fixed/variable..julian)
	 */
	switch (st->density) {
/*      case 8 mm:   What is the SCSI density code for 8 mm, anyway? */
	case QIC_11:
	case QIC_24:
	case QIC_120:
	case QIC_150:
	case QIC_525:
	case QIC_1320:
		st->flags &= ~ST_2FM_AT_EOD;
		break;
	default:
		st->flags |= ST_2FM_AT_EOD;
	}
	return 0;
}

/*
 * Actually translate the requested transfer into
 * one the physical driver can understand
 * The transfer is described by a buf and will include
 * only one physical transfer.
 */
void
ststrategy(bp)
	struct buf *bp;
{
	struct st_softc *st = st_cd.cd_devs[STUNIT(bp->b_dev)];
	struct buf *dp;
	int s;

	SC_DEBUG(st->sc_link, SDEV_DB2, ("ststrategy: %ld bytes @ blk %d\n",
	    bp->b_bcount, bp->b_blkno));
	/*
	 * If it's a null transfer, return immediately.
	 */
	if (bp->b_bcount == 0)
		goto done;
	/*
	 * Odd sized request on fixed drives are verboten
	 */
	if (st->flags & ST_FIXEDBLOCKS) {
		if (bp->b_bcount % st->blksize) {
			printf("%s: bad request, must be multiple of %d\n",
			    st->sc_dev.dv_xname, st->blksize);
			bp->b_error = EIO;
			goto bad;
		}
	}
	/*
	 * as are out-of-range requests on variable drives.
	 */
	else if (bp->b_bcount < st->blkmin ||
		 (st->blkmax && bp->b_bcount > st->blkmax)) {
		printf("%s: bad request, must be between %d and %d\n",
		    st->sc_dev.dv_xname, st->blkmin, st->blkmax);
		bp->b_error = EIO;
		goto bad;
	}
	s = splbio();

	/*
	 * Place it in the queue of activities for this tape
	 * at the end (a bit silly because we only have on user..
	 * (but it could fork()))
	 */
	dp = &st->buf_queue;
	bp->b_actf = NULL;
	bp->b_actb = dp->b_actb;
	*dp->b_actb = bp;
	dp->b_actb = &bp->b_actf;

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 * (All a bit silly if we're only allowing 1 open but..)
	 */
	ststart(st);

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
}

/*
 * ststart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer required. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (ststrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 * ststart() is called at splbio from ststrategy, strestart and scsi_done()
 */
void
ststart(v)
	void *v;
{
	struct st_softc *st = v;
	struct scsi_link *sc_link = st->sc_link;
	struct buf *bp, *dp;
	struct scsi_rw_tape cmd;
	int flags, error;

	SC_DEBUG(sc_link, SDEV_DB2, ("ststart\n"));

	splassert(IPL_BIO);

	/*
	 * See if there is a buf to do and we are not already
	 * doing one
	 */
	while (sc_link->openings > 0) {
		/* if a special awaits, let it proceed first */
		if (sc_link->flags & SDEV_WAITING) {
			sc_link->flags &= ~SDEV_WAITING;
			wakeup((caddr_t)sc_link);
			return;
		}

		dp = &st->buf_queue;
		if ((bp = dp->b_actf) == NULL)
			return;
		if ((dp = bp->b_actf) != NULL)
			dp->b_actb = bp->b_actb;
		else
			st->buf_queue.b_actb = bp->b_actb;
		*bp->b_actb = dp;

		/*
		 * if the device has been unmounted by the user
		 * then throw away all requests until done
		 */
		if (!(st->flags & ST_MOUNTED) ||
		    !(sc_link->flags & SDEV_MEDIA_LOADED)) {
			/* make sure that one implies the other.. */
			sc_link->flags &= ~SDEV_MEDIA_LOADED;
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
			bp->b_error = EIO;
			biodone(bp);
			continue;
		}
		/*
		 * only FIXEDBLOCK devices have pending operations
		 */
		if (st->flags & ST_FIXEDBLOCKS) {
			/*
			 * If we are at a filemark but have not reported it yet
			 * then we should report it now
			 */
			if (st->flags & ST_AT_FILEMARK) {
				if ((bp->b_flags & B_READ) == B_WRITE) {
					/*
					 * Handling of ST_AT_FILEMARK in
					 * st_space will fill in the right file
					 * mark count.
					 * Back up over filemark
					 */
					if (st_space(st, 0, SP_FILEMARKS, 0)) {
						bp->b_flags |= B_ERROR;
						bp->b_resid = bp->b_bcount;
						bp->b_error = EIO;
						biodone(bp);
						continue;
					}
				} else {
					bp->b_resid = bp->b_bcount;
					bp->b_error = 0;
					bp->b_flags &= ~B_ERROR;
					st->flags &= ~ST_AT_FILEMARK;
					biodone(bp);
					continue;	/* seek more work */
				}
			}
			/*
			 * If we are at EIO (e.g. EOM) but have not reported it
			 * yet then we should report it now
			 */
			if (st->flags & ST_EIO_PENDING) {
				bp->b_resid = bp->b_bcount;
				bp->b_error = EIO;
				bp->b_flags |= B_ERROR;
				st->flags &= ~ST_EIO_PENDING;
				biodone(bp);
				continue;	/* seek more work */
			}
		}

		/*
		 *  Fill out the scsi command
		 */
		bzero(&cmd, sizeof(cmd));
		if ((bp->b_flags & B_READ) == B_WRITE) {
			cmd.opcode = WRITE;
			st->flags &= ~ST_FM_WRITTEN;
			st->flags |= ST_WRITTEN;
			flags = SCSI_DATA_OUT;
		} else {
			cmd.opcode = READ;
			flags = SCSI_DATA_IN;
		}

		/*
		 * Handle "fixed-block-mode" tape drives by using the
		 * block count instead of the length.
		 */
		if (st->flags & ST_FIXEDBLOCKS) {
			cmd.byte2 |= SRW_FIXED;
			_lto3b(bp->b_bcount / st->blksize, cmd.len);
		} else
			_lto3b(bp->b_bcount, cmd.len);

		if (st->media_blkno != -1) {
			/* Update block count now, errors will set it to -1. */
			if (st->flags & ST_FIXEDBLOCKS)
				st->media_blkno += _3btol(cmd.len);
			else if (cmd.len != 0)
				st->media_blkno++;
		}

		/*
		 * go ask the adapter to do all this for us
		 */
		error = scsi_scsi_cmd(sc_link, (struct scsi_generic *) &cmd,
		    sizeof(cmd), (u_char *) bp->b_data, bp->b_bcount, 0,
		    ST_IO_TIME, bp, flags | SCSI_NOSLEEP);
		switch (error) {
		case 0:
			timeout_del(&st->sc_timeout);
			break;
		case EAGAIN:
			/*
			 * The device can't start another i/o. Try again later.
			 */
			dp->b_actf = bp;
			timeout_add(&st->sc_timeout, 1);
			return;
		default:
			printf("%s: not queued\n", st->sc_dev.dv_xname);
			break;
		}
	} /* go back and see if we can cram more work in.. */
}

void
strestart(v)
	void *v;
{
	int s;

	s = splbio();
	ststart(v);
	splx(s);
}

int
stread(dev, uio, iomode)
	dev_t dev;
	struct uio *uio;
	int iomode;
{
	struct st_softc *st = st_cd.cd_devs[STUNIT(dev)];

	return (physio(ststrategy, NULL, dev, B_READ,
	    st->sc_link->adapter->scsi_minphys, uio));
}

int
stwrite(dev, uio, iomode)
	dev_t dev;
	struct uio *uio;
	int iomode;
{
	struct st_softc *st = st_cd.cd_devs[STUNIT(dev)];

	return (physio(ststrategy, NULL, dev, B_WRITE,
	    st->sc_link->adapter->scsi_minphys, uio));
}

/*
 * Perform special action on behalf of the user;
 * knows about the internals of this device
 */
int
stioctl(dev, cmd, arg, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t arg;
	int flag;
	struct proc *p;
{
	int error = 0;
	int unit;
	int nmarks;
	int flags;
	struct st_softc *st;
	int hold_blksize;
	u_int8_t hold_density;
	struct mtop *mt = (struct mtop *) arg;
	int number;

	/*
	 * Find the device that the user is talking about
	 */
	flags = 0;		/* give error messages, act on errors etc. */
	unit = STUNIT(dev);
	st = st_cd.cd_devs[unit];
	hold_blksize = st->blksize;
	hold_density = st->density;

	switch (cmd) {

	case MTIOCGET: {
		struct mtget *g = (struct mtget *) arg;

		/*
		 * (to get the current state of READONLY)
		 */
		error = st_mode_sense(st, SCSI_SILENT);
		if (error)
			break;

		SC_DEBUG(st->sc_link, SDEV_DB1, ("[ioctl: get status]\n"));
		bzero(g, sizeof(struct mtget));
		g->mt_type = 0x7;	/* Ultrix compat *//*? */
		g->mt_blksiz = st->blksize;
		g->mt_density = st->density;
 		g->mt_mblksiz = st->modes.blksize;
 		g->mt_mdensity = st->modes.density;
		if (st->flags & ST_READONLY)
			g->mt_dsreg |= MT_DS_RDONLY;
		if (st->flags & ST_MOUNTED)
			g->mt_dsreg |= MT_DS_MOUNTED;
		g->mt_resid = st->mt_resid;
		g->mt_erreg = st->mt_erreg;
		g->mt_fileno = st->media_fileno;
		g->mt_blkno = st->media_blkno;
		/*
		 * clear latched errors.
		 */
		st->mt_resid = 0;
		st->mt_erreg = 0;
		break;
	}
	case MTIOCTOP: {

		SC_DEBUG(st->sc_link, SDEV_DB1,
		    ("[ioctl: op=0x%x count=0x%x]\n", mt->mt_op, mt->mt_count));

		number = mt->mt_count;
		switch (mt->mt_op) {
		case MTWEOF:	/* write an end-of-file record */
			error = st_write_filemarks(st, number, flags);
			break;
		case MTBSF:	/* backward space file */
			number = -number;
		case MTFSF:	/* forward space file */
			error = st_check_eod(st, FALSE, &nmarks, flags);
			if (!error)
				error = st_space(st, number - nmarks,
				    SP_FILEMARKS, flags);
			break;
		case MTBSR:	/* backward space record */
			number = -number;
		case MTFSR:	/* forward space record */
			error = st_check_eod(st, TRUE, &nmarks, flags);
			if (!error)
				error = st_space(st, number, SP_BLKS, flags);
			break;
		case MTREW:	/* rewind */
			error = st_rewind(st, 0, flags);
			break;
		case MTOFFL:	/* rewind and put the drive offline */
			st_unmount(st, EJECT, DOREWIND);
			break;
		case MTNOP:	/* no operation, sets status only */
			break;
		case MTRETEN:	/* retension the tape */
			error = st_load(st, LD_RETENSION, flags);
			if (!error)
				error = st_load(st, LD_LOAD, flags);
			break;
		case MTEOM:	/* forward space to end of media */
			error = st_check_eod(st, FALSE, &nmarks, flags);
			if (!error)
				error = st_space(st, 1, SP_EOM, flags);
			break;
		case MTCACHE:	/* enable controller cache */
			st->flags &= ~ST_DONTBUFFER;
			goto try_new_value;
		case MTNOCACHE:	/* disable controller cache */
			st->flags |= ST_DONTBUFFER;
			goto try_new_value;
		case MTERASE:	/* erase volume */
			error = st_erase(st, number, flags);
			break;
		case MTSETBSIZ:	/* Set block size for device */
			if (number == 0) {
				st->flags &= ~ST_FIXEDBLOCKS;
			} else {
				if ((st->blkmin || st->blkmax) &&
				    (number < st->blkmin ||
				    number > st->blkmax)) {
					error = EINVAL;
					break;
				}
				st->flags |= ST_FIXEDBLOCKS;
			}
			st->blksize = number;
			st->flags |= ST_BLOCK_SET;	/*XXX */
			goto try_new_value;

		case MTSETDNSTY:	/* Set density for device and mode */
			if (number < 0 || number > SCSI_MAX_DENSITY_CODE) {
				error = EINVAL;
				break;
			} else
				st->density = number;
			goto try_new_value;

		default:
			error = EINVAL;
		}
		break;
	}
	case MTIOCIEOT:
	case MTIOCEEOT:
		break;

#if 0
	case MTIOCRDSPOS:
		error = st_rdpos(st, 0, (u_int32_t *) arg);
		break;

	case MTIOCRDHPOS:
		error = st_rdpos(st, 1, (u_int32_t *) arg);
		break;

	case MTIOCSLOCATE:
		error = st_setpos(st, 0, (u_int32_t *) arg);
		break;

	case MTIOCHLOCATE:
		error = st_setpos(st, 1, (u_int32_t *) arg);
		break;
#endif

	default:
		error = scsi_do_ioctl(st->sc_link, dev, cmd, arg, flag, p);
		break;
	}
	return error;
/*-----------------------------*/
try_new_value:
	/*
	 * Check that the mode being asked for is aggreeable to the
	 * drive. If not, put it back the way it was.
	 */
	if ((error = st_mode_select(st, 0)) != 0) {/* put it back as it was */
		printf("%s: cannot set selected mode\n", st->sc_dev.dv_xname);
		st->density = hold_density;
		st->blksize = hold_blksize;
		if (st->blksize)
			st->flags |= ST_FIXEDBLOCKS;
		else
			st->flags &= ~ST_FIXEDBLOCKS;
		return error;
	}
	/*
	 * As the drive liked it, if we are setting a new default,
	 * set it into the structures as such.
	 */
	switch (mt->mt_op) {
	case MTSETBSIZ:
		st->modes.blksize = st->blksize;
		st->modeflags |= BLKSIZE_SET_BY_USER;
		break;
	case MTSETDNSTY:
		st->modes.density = st->density;
		st->modeflags |= DENSITY_SET_BY_USER;
		break;
	}

	return 0;
}

/*
 * Do a synchronous read.
 */
int
st_read(st, buf, size, flags)
	struct st_softc *st;
	int size;
	int flags;
	char *buf;
{
	struct scsi_rw_tape cmd;

	/*
	 * If it's a null transfer, return immediately
	 */
	if (size == 0)
		return 0;
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = READ;
	if (st->flags & ST_FIXEDBLOCKS) {
		cmd.byte2 |= SRW_FIXED;
		_lto3b(size / (st->blksize ? st->blksize : DEF_FIXED_BSIZE),
		    cmd.len);
	} else
		_lto3b(size, cmd.len);
	return scsi_scsi_cmd(st->sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), (u_char *) buf, size, 0, ST_IO_TIME, NULL,
	    flags | SCSI_DATA_IN);
}

/*
 * Ask the drive what its min and max blk sizes are.
 */
int
st_read_block_limits(st, flags)
	struct st_softc *st;
	int flags;
{
	struct scsi_block_limits cmd;
	struct scsi_block_limits_data block_limits;
	struct scsi_link *sc_link = st->sc_link;
	int error;

	/*
	 * First check if we have it all loaded
	 */
	if ((sc_link->flags & SDEV_MEDIA_LOADED))
		return 0;

	/*
	 * do a 'Read Block Limits'
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = READ_BLOCK_LIMITS;

	/*
	 * do the command, update the global values
	 */
	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), (u_char *) &block_limits, sizeof(block_limits),
	    ST_RETRIES, ST_CTL_TIME, NULL, flags | SCSI_DATA_IN);
	if (error)
		return error;

	st->blkmin = _2btol(block_limits.min_length);
	st->blkmax = _3btol(block_limits.max_length);

	SC_DEBUG(sc_link, SDEV_DB3,
	    ("(%d <= blksize <= %d)\n", st->blkmin, st->blkmax));
	return 0;
}

/*
 * Get the scsi driver to send a full inquiry to the
 * device and use the results to fill out the global
 * parameter structure.
 *
 * called from:
 * attach
 * open
 * ioctl (to reset original blksize)
 */
int
st_mode_sense(st, flags)
	struct st_softc *st;
	int flags;
{
	union scsi_mode_sense_buf *data;
	struct scsi_link *sc_link = st->sc_link;
	u_int64_t block_count;
	u_int32_t density, block_size;
	u_char *page0 = NULL;
	u_int8_t dev_spec;
	int error, big;

	data = malloc(sizeof(*data), M_TEMP, M_NOWAIT);
	if (data == NULL)
		return (ENOMEM);

	/*
	 * Ask for page 0 (vendor specific) mode sense data.
	 */
	error = scsi_do_mode_sense(sc_link, 0, data, (void **)&page0,
	    &density, &block_count, &block_size, 1, flags | SCSI_SILENT, &big);
	if (error != 0) {
		free(data, M_TEMP);
		return (error);
	}

	/* It is valid for no page0 to be available. */

	if (big)
		dev_spec = data->hdr_big.dev_spec;
	else
		dev_spec = data->hdr.dev_spec;

	if (dev_spec & SMH_DSP_WRITE_PROT)
		st->flags |= ST_READONLY;
	else
		st->flags &= ~ST_READONLY;

	st->numblks = block_count;
	st->media_blksize = block_size;
	st->media_density = density;

	SC_DEBUG(sc_link, SDEV_DB3,
	    ("density code 0x%x, %d-byte blocks, write-%s, ",
	    st->media_density, st->media_blksize,
	    st->flags & ST_READONLY ? "protected" : "enabled"));
	SC_DEBUGN(sc_link, SDEV_DB3,
	    ("%sbuffered\n", dev_spec & SMH_DSP_BUFF_MODE ? "" : "un"));

	sc_link->flags |= SDEV_MEDIA_LOADED;

	free(data, M_TEMP);
	return (0);
}

/*
 * Send a filled out parameter structure to the drive to
 * set it into the desire modes etc.
 */
int
st_mode_select(st, flags)
	struct st_softc *st;
	int flags;
{
	union scsi_mode_sense_buf *inbuf, *outbuf;
	struct scsi_blk_desc general;
	struct scsi_link *sc_link = st->sc_link;
	u_int8_t *page0 = NULL;
	int error, big, page0_size;

	inbuf = malloc(sizeof(*inbuf), M_TEMP, M_NOWAIT);
	if (inbuf == NULL)
		return (ENOMEM);
	outbuf = malloc(sizeof(*outbuf), M_TEMP, M_NOWAIT);
	if (outbuf == NULL) {
		free(inbuf, M_TEMP);
		return (ENOMEM);
	}

	/*
	 * This quirk deals with drives that have only one valid mode and think
	 * this gives them license to reject all mode selects, even if the
	 * selected mode is the one that is supported.
	 */
	if (st->quirks & ST_Q_UNIMODAL) {
		SC_DEBUG(sc_link, SDEV_DB3,
		    ("not setting density 0x%x blksize 0x%x\n",
		    st->density, st->blksize));
		free(inbuf, M_TEMP);
		free(outbuf, M_TEMP);
		return (0);
	}

	if (sc_link->flags & SDEV_ATAPI) {
		free(inbuf, M_TEMP);
		free(outbuf, M_TEMP);
		return (0);
	}

	bzero(outbuf, sizeof(*outbuf));
	bzero(&general, sizeof(general));

	general.density = st->density;
	if (st->flags & ST_FIXEDBLOCKS)
		_lto3b(st->blksize, general.blklen);

	/*
	 * Ask for page 0 (vendor specific) mode sense data.
	 */
	error = scsi_do_mode_sense(sc_link, 0, inbuf, (void **)&page0, NULL,
	    NULL, NULL, 1, flags | SCSI_SILENT, &big);
	if (error != 0) {
		free(inbuf, M_TEMP);
		free(outbuf, M_TEMP);
		return (error);
	}

	if (page0 == NULL) {
		page0_size = 0;
	} else if (big == 0) {
		page0_size = inbuf->hdr.data_length +
		    sizeof(inbuf->hdr.data_length) - sizeof(inbuf->hdr) -
		    inbuf->hdr.blk_desc_len;
		memcpy(&outbuf->buf[sizeof(outbuf->hdr)+ sizeof(general)],
		    page0, page0_size);
	} else {
		page0_size = _2btol(inbuf->hdr_big.data_length) +
		    sizeof(inbuf->hdr_big.data_length) -
		    sizeof(inbuf->hdr_big) -
		   _2btol(inbuf->hdr_big.blk_desc_len);
		memcpy(&outbuf->buf[sizeof(outbuf->hdr_big) + sizeof(general)],
		    page0, page0_size);
	}

	/*
	 * Set up for a mode select.
	 */
	if (big == 0) {
		outbuf->hdr.data_length = sizeof(outbuf->hdr) +
		    sizeof(general) + page0_size -
		    sizeof(outbuf->hdr.data_length);
		if ((st->flags & ST_DONTBUFFER) == 0)
			outbuf->hdr.dev_spec = SMH_DSP_BUFF_MODE_ON;
		outbuf->hdr.blk_desc_len = sizeof(general);
		memcpy(&outbuf->buf[sizeof(outbuf->hdr)],
		    &general, sizeof(general));
		error = scsi_mode_select(st->sc_link, 0, &outbuf->hdr,
		    flags, ST_CTL_TIME);
		free(inbuf, M_TEMP);
		free(outbuf, M_TEMP);
		return (error);
	}

	/* MODE SENSE (10) header was returned, so use MODE SELECT (10). */
	_lto2b((sizeof(outbuf->hdr_big) + sizeof(general) + page0_size -
	    sizeof(outbuf->hdr_big.data_length)), outbuf->hdr_big.data_length);
	if ((st->flags & ST_DONTBUFFER) == 0)
		outbuf->hdr_big.dev_spec = SMH_DSP_BUFF_MODE_ON;
	_lto2b(sizeof(general), outbuf->hdr_big.blk_desc_len);
	memcpy(&outbuf->buf[sizeof(outbuf->hdr_big)], &general,
	    sizeof(general));

	error = scsi_mode_select_big(st->sc_link, 0, &outbuf->hdr_big,
	    flags, ST_CTL_TIME);
	free(inbuf, M_TEMP);
	free(outbuf, M_TEMP);
	return (error);
}

/*
 * issue an erase command
 */
int
st_erase(st, full, flags)
	struct st_softc *st;
	int full, flags;
{
	struct scsi_erase cmd;
	int tmo;

	/*
	 * Full erase means set LONG bit in erase command, which asks
	 * the drive to erase the entire unit.  Without this bit, we're
	 * asking the drive to write an erase gap.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = ERASE;
	if (full) {
		cmd.byte2 = SE_IMMED|SE_LONG;
		tmo = ST_SPC_TIME;
	} else {
		cmd.byte2 = SE_IMMED;
		tmo = ST_IO_TIME;
	}

	/*
	 * XXX We always do this asynchronously, for now.  How long should
	 * we wait if we want to (eventually) to it synchronously?
	 */
	return (scsi_scsi_cmd(st->sc_link, (struct scsi_generic *)&cmd,
	    sizeof(cmd), 0, 0, ST_RETRIES, tmo, NULL, flags));
}

/*
 * skip N blocks/filemarks/seq filemarks/eom
 */
int
st_space(st, number, what, flags)
	struct st_softc *st;
	u_int what;
	int flags;
	int number;
{
	struct scsi_space cmd;
	int error;

	switch (what) {
	case SP_BLKS:
		if (st->flags & ST_PER_ACTION) {
			if (number > 0) {
				st->flags &= ~ST_PER_ACTION;
				return EIO;
			} else if (number < 0) {
				if (st->flags & ST_AT_FILEMARK) {
					/*
					 * Handling of ST_AT_FILEMARK
					 * in st_space will fill in the
					 * right file mark count.
					 */
					error = st_space(st, 0, SP_FILEMARKS,
						flags);
					if (error)
						return error;
				}
				if (st->flags & ST_BLANK_READ) {
					st->flags &= ~ST_BLANK_READ;
					return EIO;
				}
				st->flags &= ~ST_EIO_PENDING;
			}
		}
		break;
	case SP_FILEMARKS:
		if (st->flags & ST_EIO_PENDING) {
			if (number > 0) {
				/* pretend we just discovered the error */
				st->flags &= ~ST_EIO_PENDING;
				return EIO;
			} else if (number < 0) {
				/* back away from the error */
				st->flags &= ~ST_EIO_PENDING;
			}
		}
		if (st->flags & ST_AT_FILEMARK) {
			st->flags &= ~ST_AT_FILEMARK;
			number--;
		}
		if ((st->flags & ST_BLANK_READ) && (number < 0)) {
			/* back away from unwritten tape */
			st->flags &= ~ST_BLANK_READ;
			number++;	/* XXX dubious */
		}
		break;
	case SP_EOM:
		if (st->flags & ST_EIO_PENDING) {
			/* pretend we just discovered the error */
			st->flags &= ~ST_EIO_PENDING;
			return EIO;
		}
		if (st->flags & ST_AT_FILEMARK)
			st->flags &= ~ST_AT_FILEMARK;
		break;
	}
	if (number == 0)
		return 0;

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = SPACE;
	cmd.byte2 = what;
	_lto3b(number, cmd.number);

	error = scsi_scsi_cmd(st->sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), 0, 0, 0, ST_SPC_TIME, NULL, flags);

	if (error != 0) {
		st->media_fileno = -1;
		st->media_blkno = -1;
	} else {
		switch (what) {
		case SP_BLKS:
			if (st->media_blkno != -1) {
				st->media_blkno += number;
				if (st->media_blkno < 0)
					st->media_blkno = -1;
			}
			break;
		case SP_FILEMARKS:
			if (st->media_fileno != -1) {
				st->media_fileno += number;
				st->media_blkno = 0;
			}
			break;
		default:
			st->media_fileno = -1;
			st->media_blkno = -1;
			break;
		}
	}

	return (error);
}

/*
 * write N filemarks
 */
int
st_write_filemarks(st, number, flags)
	struct st_softc *st;
	int flags;
	int number;
{
	struct scsi_write_filemarks cmd;
	int error;

	/*
	 * It's hard to write a negative number of file marks.
	 * Don't try.
	 */
	if (number < 0)
		return EINVAL;
	switch (number) {
	case 0:		/* really a command to sync the drive's buffers */
		break;
	case 1:
		if (st->flags & ST_FM_WRITTEN)	/* already have one down */
			st->flags &= ~ST_WRITTEN;
		else
			st->flags |= ST_FM_WRITTEN;
		st->flags &= ~ST_PER_ACTION;
		break;
	default:
		st->flags &= ~(ST_PER_ACTION | ST_WRITTEN);
	}

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = WRITE_FILEMARKS;
	_lto3b(number, cmd.number);

	error = scsi_scsi_cmd(st->sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), 0, 0, 0, ST_IO_TIME * 4, NULL, flags);

	if (error != 0) {
		st->media_fileno = -1;
		st->media_blkno = -1;
	} else if (st->media_fileno != -1) {
		st->media_fileno += number;
		st->media_blkno = 0;
	}

	return (error);
}

/*
 * Make sure the right number of file marks is on tape if the
 * tape has been written.  If the position argument is true,
 * leave the tape positioned where it was originally.
 *
 * nmarks returns the number of marks to skip (or, if position
 * true, which were skipped) to get back original position.
 */
int
st_check_eod(st, position, nmarks, flags)
	struct st_softc *st;
	int position;
	int *nmarks;
	int flags;
{
	int error;

	switch (st->flags & (ST_WRITTEN | ST_FM_WRITTEN | ST_2FM_AT_EOD)) {
	default:
		*nmarks = 0;
		return 0;
	case ST_WRITTEN:
	case ST_WRITTEN | ST_FM_WRITTEN | ST_2FM_AT_EOD:
		*nmarks = 1;
		break;
	case ST_WRITTEN | ST_2FM_AT_EOD:
		*nmarks = 2;
	}
	error = st_write_filemarks(st, *nmarks, flags);
	if (position && !error)
		error = st_space(st, -*nmarks, SP_FILEMARKS, flags);
	return error;
}

/*
 * load/unload/retension
 */
int
st_load(st, type, flags)
	struct st_softc *st;
	u_int type;
	int flags;
{
	struct scsi_load cmd;

	st->media_fileno = -1;
	st->media_blkno = -1;

	if (type != LD_LOAD) {
		int error;
		int nmarks;

		error = st_check_eod(st, FALSE, &nmarks, flags);
		if (error)
			return error;
	}
	if (st->quirks & ST_Q_IGNORE_LOADS) {
		if (type == LD_LOAD) {
			/*
			 * If we ignore loads, at least we should try a rewind.
			 */
			return st_rewind(st, 0, flags);
		}
		return (0);
	}


	bzero(&cmd, sizeof(cmd));
	cmd.opcode = LOAD;
	cmd.how = type;

	return scsi_scsi_cmd(st->sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), 0, 0, ST_RETRIES, ST_SPC_TIME, NULL, flags);
}

/*
 *  Rewind the device
 */
int
st_rewind(st, immediate, flags)
	struct st_softc *st;
	u_int immediate;
	int flags;
{
	struct scsi_rewind cmd;
	int error;
	int nmarks;

	error = st_check_eod(st, FALSE, &nmarks, flags);
	if (error)
		return error;
	st->flags &= ~ST_PER_ACTION;

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = REWIND;
	cmd.byte2 = immediate;

	error = scsi_scsi_cmd(st->sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), 0, 0, ST_RETRIES,
	    immediate ? ST_CTL_TIME: ST_SPC_TIME, NULL, flags);

	if (error == 0) {
		st->media_fileno = 0;
		st->media_blkno = 0;
	}

	return (error);
}

/*
 * Look at the returned sense and act on the error and detirmine
 * The unix error number to pass back... (0 = report no error)
 *                            (-1 = continue processing)
 */
int
st_interpret_sense(xs)
	struct scsi_xfer *xs;
{
	struct scsi_sense_data *sense = &xs->sense;
	struct scsi_link *sc_link = xs->sc_link;
	struct st_softc *st = sc_link->device_softc;
	struct buf *bp = xs->bp;
	u_int8_t serr = sense->error_code & SSD_ERRCODE;
	u_int8_t skey = sense->flags & SSD_KEY;
	int32_t info;

	if (((sc_link->flags & SDEV_OPEN) == 0) ||
	    (serr != SSD_ERRCODE_CURRENT && serr != SSD_ERRCODE_DEFERRED))
		return (EJUSTRETURN); /* let the generic code handle it */

	switch (skey) {

	/*
	 * We do custom processing in st for the unit becoming ready case.
	 * in this case we do not allow xs->retries to be decremented
	 * only on the "Unit Becoming Ready" case. This is because tape
	 * drives report "Unit Becoming Ready" when loading media, etc.
	 * and can take a long time.  Rather than having a massive timeout 
	 * for all operations (which would cause other problems) we allow
	 * operations to wait (but be interruptable with Ctrl-C) forever
	 * as long as the drive is reporting that it is becoming ready.
	 * all other cases are handled as per the default.
	 */

	case SKEY_NOT_READY:
		if ((xs->flags & SCSI_IGNORE_NOT_READY) != 0)
			return (0);
		switch (ASC_ASCQ(sense)) {
		case SENSE_NOT_READY_BECOMING_READY:
			SC_DEBUG(sc_link, SDEV_DB1, ("not ready: busy (%#x)\n",
			    sense->add_sense_code_qual));
			/* don't count this as a retry */
			xs->retries++;
			return (scsi_delay(xs, 1));
		default:
			return (EJUSTRETURN);
	}
	case SKEY_NO_SENSE:
	case SKEY_RECOVERED_ERROR:
	case SKEY_MEDIUM_ERROR:
	case SKEY_VOLUME_OVERFLOW:
	case SKEY_BLANK_CHECK:
		break;
	default:
		return (EJUSTRETURN);
	}

	/*
	 * Get the sense fields and work out what code
	 */
	if (sense->error_code & SSD_ERRCODE_VALID)
		info = _4btol(sense->info);
	else
		info = xs->datalen;	/* bad choice if fixed blocks */
	if (st->flags & ST_FIXEDBLOCKS) {
		xs->resid = info * st->blksize;
		if (sense->flags & SSD_EOM) {
			st->flags |= ST_EIO_PENDING;
			if (bp)
				bp->b_resid = xs->resid;
		}
		if (sense->flags & SSD_FILEMARK) {
			st->flags |= ST_AT_FILEMARK;
			if (st->media_fileno != -1) {
				st->media_fileno++;
				st->media_blkno = 0;
			}
			if (bp)
				bp->b_resid = xs->resid;
		}
		if (sense->flags & SSD_ILI) {
			st->flags |= ST_EIO_PENDING;
			if (bp)
				bp->b_resid = xs->resid;
			if (sense->error_code & SSD_ERRCODE_VALID &&
			    (xs->flags & SCSI_SILENT) == 0)
				printf("%s: block wrong size, %d blocks residual\n",
				    st->sc_dev.dv_xname, info);

			/*
			 * This quirk code helps the drive read
			 * the first tape block, regardless of
			 * format.  That is required for these
			 * drives to return proper MODE SENSE
			 * information.
			 */
			if ((st->quirks & ST_Q_SENSE_HELP) &&
			    !(sc_link->flags & SDEV_MEDIA_LOADED))
				st->blksize -= 512;
		}
		/*
		 * If no data was transferred, return immediately
		 */
		if (xs->resid >= xs->datalen) {
			if (st->flags & ST_EIO_PENDING)
				return EIO;
			if (st->flags & ST_AT_FILEMARK) {
				if (bp)
					bp->b_resid = xs->resid;
				return 0;
			}
		}
	} else {		/* must be variable mode */
		xs->resid = xs->datalen;	/* to be sure */
		if (sense->flags & SSD_EOM)
			return EIO;
		if (sense->flags & SSD_FILEMARK) {
			if (st->media_fileno != -1) {
				st->media_fileno++;
				st->media_blkno = 0;
			}
			if (bp)
				bp->b_resid = bp->b_bcount;
			return 0;
		}
		if (sense->flags & SSD_ILI) {
			if (info < 0) {
				/*
				 * the record was bigger than the read
				 */
				if ((xs->flags & SCSI_SILENT) == 0)
					printf("%s: %d-byte record too big\n",
					    st->sc_dev.dv_xname,
					    xs->datalen - info);
				return (EIO);
			} else if (info > xs->datalen) {
				/*
				 * huh? the residual is bigger than the request
				 */
				if ((xs->flags & SCSI_SILENT) == 0) {
					printf(
					    "%s: bad residual %d out of %d\n",
					    st->sc_dev.dv_xname, info,
					    xs->datalen);
					return (EIO);
				}
			}
			xs->resid = info;
			if (bp)
				bp->b_resid = info;
			return (0);
		}
	}

	if (skey == SKEY_BLANK_CHECK) {
		/*
		 * This quirk code helps the drive read the first tape block,
		 * regardless of format.  That is required for these drives to
		 * return proper MODE SENSE information.
		 */
		if ((st->quirks & ST_Q_SENSE_HELP) &&
		    !(sc_link->flags & SDEV_MEDIA_LOADED)) {
			/* still starting */
			st->blksize -= 512;
		} else if (!(st->flags & (ST_2FM_AT_EOD | ST_BLANK_READ))) {
			st->flags |= ST_BLANK_READ;
			xs->resid = xs->datalen;
			if (bp) {
				bp->b_resid = xs->resid;
				/* return an EOF */
			}
			return (0);
		}
	}

	return (EJUSTRETURN);
}

/*
 * The quirk here is that the drive returns some value to st_mode_sense
 * incorrectly until the tape has actually passed by the head.
 *
 * The method is to set the drive to large fixed-block state (user-specified
 * density and 1024-byte blocks), then read and rewind to get it to sense the
 * tape.  If that doesn't work, try 512-byte fixed blocks.  If that doesn't
 * work, as a last resort, try variable- length blocks.  The result will be
 * the ability to do an accurate st_mode_sense.
 *
 * We know we can do a rewind because we just did a load, which implies rewind.
 * Rewind seems preferable to space backward if we have a virgin tape.
 *
 * The rest of the code for this quirk is in ILI processing and BLANK CHECK
 * error processing, both part of st_interpret_sense.
 */
int
st_touch_tape(st)
	struct st_softc *st;
{
	char *buf;
	int readsize;
	int error;

	buf = malloc(1024, M_TEMP, M_NOWAIT);
	if (!buf)
		return ENOMEM;

	if ((error = st_mode_sense(st, 0)) != 0)
		goto bad;
	st->blksize = 1024;
	do {
		switch (st->blksize) {
		case 512:
		case 1024:
			readsize = st->blksize;
			st->flags |= ST_FIXEDBLOCKS;
			break;
		default:
			readsize = 1;
			st->flags &= ~ST_FIXEDBLOCKS;
		}
		if ((error = st_mode_select(st, 0)) != 0)
			goto bad;
		st_read(st, buf, readsize, SCSI_SILENT);	/* XXX */
		if ((error = st_rewind(st, 0, 0)) != 0) {
bad:			free(buf, M_TEMP);
			return error;
		}
	} while (readsize != 1 && readsize > st->blksize);

	free(buf, M_TEMP);
	return 0;
}

int
stdump(dev, blkno, va, size)
	dev_t dev;
	int blkno;
	caddr_t va;
	size_t size;
{

	/* Not implemented. */
	return ENXIO;
}
