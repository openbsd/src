/*	$NetBSD: cd.c,v 1.79 1996/01/07 22:03:58 thorpej Exp $	*/

/*
 * Copyright (c) 1994, 1995 Charles M. Hannum.  All rights reserved.
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
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/malloc.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/cdio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_cd.h>
#include <scsi/scsi_disk.h>	/* rw_big and start_stop come from there */
#include <scsi/scsiconf.h>

#define	CDOUTSTANDING	2
#define	CDRETRIES	1

#define	CDUNIT(z)			DISKUNIT(z)
#define	CDPART(z)			DISKPART(z)
#define	MAKECDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

struct cd_softc {
	struct device sc_dev;
	struct disk sc_dk;

	int flags;
#define	CDF_LOCKED	0x01
#define	CDF_WANTED	0x02
#define	CDF_WLABEL	0x04		/* label is writable */
#define	CDF_LABELLING	0x08		/* writing label */
	struct scsi_link *sc_link;	/* contains our targ, lun, etc. */
	struct cd_parms {
		int blksize;
		u_long disksize;	/* total number sectors */
	} params;
	struct buf buf_queue;
};

int cdmatch __P((struct device *, void *, void *));
void cdattach __P((struct device *, struct device *, void *));

struct cfdriver cdcd = {
	NULL, "cd", cdmatch, cdattach, DV_DISK, sizeof(struct cd_softc)
};

void cdgetdisklabel __P((struct cd_softc *));
int cd_get_parms __P((struct cd_softc *, int));
void cdstrategy __P((struct buf *));
void cdstart __P((struct cd_softc *));
int cddone __P((struct scsi_xfer *));

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
#if 0
	{T_CDROM, T_REMOV, /* more luns */
	 "PIONEER ", "CD-ROM DRM-600  ", ""},
#endif
};

int
cdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct cfdata *cf = match;
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
	struct cd_parms *dp = &cd->params;
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

#if !defined(i386) || defined(NEWCONFIG)
	dk_establish(&cd->sc_dk, &cd->sc_dev);		/* XXX */
#endif

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
cdopen(dev, flag, fmt)
	dev_t dev;
	int flag, fmt;
{
	struct cd_softc *cd;
	struct scsi_link *sc_link;
	int unit, part;
	int error;

	unit = CDUNIT(dev);
	if (unit >= cdcd.cd_ndevs)
		return ENXIO;
	cd = cdcd.cd_devs[unit];
	if (!cd)
		return ENXIO;

	sc_link = cd->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1,
	    ("cdopen: dev=0x%x (unit %d (of %d), partition %d)\n", dev, unit,
	    cdcd.cd_ndevs, part));

	if (error = cdlock(cd))
		return error;

	if (cd->sc_dk.dk_openmask != 0) {
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
		if (error = scsi_test_unit_ready(sc_link,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE | SCSI_IGNORE_NOT_READY))
			goto bad3;

		/* Start the pack spinning if necessary. */
		if (error = scsi_start(sc_link, SSS_START,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE | SCSI_SILENT))
			goto bad3;

		sc_link->flags |= SDEV_OPEN;

		/* Lock the pack in. */
		if (error = scsi_prevent(sc_link, PR_PREVENT,
		    SCSI_IGNORE_ILLEGAL_REQUEST | SCSI_IGNORE_MEDIA_CHANGE))
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
			cdgetdisklabel(cd);
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
cdclose(dev, flag, fmt)
	dev_t dev;
	int flag, fmt;
{
	struct cd_softc *cd = cdcd.cd_devs[CDUNIT(dev)];
	int part = CDPART(dev);
	int error;

	if (error = cdlock(cd))
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
	struct cd_softc *cd = cdcd.cd_devs[CDUNIT(bp->b_dev)];
	int opri;

	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdstrategy "));
	SC_DEBUG(cd->sc_link, SDEV_DB1,
	    ("%d bytes @ blk %d\n", bp->b_bcount, bp->b_blkno));
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

	/*
	 * Do bounds checking, adjust transfer. if error, process.
	 * If end of partition, just return.
	 */
	if (CDPART(bp->b_dev) != RAW_PART &&
	    bounds_check_with_label(bp, cd->sc_dk.dk_label,
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
cdstart(cd)
	register struct cd_softc *cd;
{
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
		if (((blkno & 0x1fffff) == blkno) &&
		    ((nblks & 0xff) == nblks)) {
			/*
			 * We can fit in a small cdb.
			 */
			bzero(&cmd_small, sizeof(cmd_small));
			cmd_small.opcode = (bp->b_flags & B_READ) ?
			    READ_COMMAND : WRITE_COMMAND;
			cmd_small.addr_2 = (blkno >> 16) & 0x1f;
			cmd_small.addr_1 = (blkno >> 8) & 0xff;
			cmd_small.addr_0 = blkno & 0xff;
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
			cmd_big.addr_3 = (blkno >> 24) & 0xff;
			cmd_big.addr_2 = (blkno >> 16) & 0xff;
			cmd_big.addr_1 = (blkno >> 8) & 0xff;
			cmd_big.addr_0 = blkno & 0xff;
			cmd_big.length2 = (nblks >> 8) & 0xff;
			cmd_big.length1 = nblks & 0xff;
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
		    ((bp->b_flags & B_READ) ? SCSI_DATA_IN : SCSI_DATA_OUT)))
			printf("%s: not queued", cd->sc_dev.dv_xname);
	}
}

int
cddone(xs)
	struct scsi_xfer *xs;
{
	struct cd_softc *cd = xs->sc_link->device_softc;

	if (xs->bp != NULL)
		disk_unbusy(&cd->sc_dk, (xs->bp->b_bcount - xs->bp->b_resid));

	return (0);
}

int
cdread(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	struct cd_softc *cd = cdcd.cd_devs[CDUNIT(dev)];

	return (physio(cdstrategy, NULL, dev, B_READ,
		       cd->sc_link->adapter->scsi_minphys, uio));
}

int
cdwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{
	struct cd_softc *cd = cdcd.cd_devs[CDUNIT(dev)];

	return (physio(cdstrategy, NULL, dev, B_WRITE,
		       cd->sc_link->adapter->scsi_minphys, uio));
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
	struct cd_softc *cd = cdcd.cd_devs[CDUNIT(dev)];
	int error;

	SC_DEBUG(cd->sc_link, SDEV_DB2, ("cdioctl 0x%lx ", cmd));

	/*
	 * If the device is not valid.. abandon ship
	 */
	if ((cd->sc_link->flags & SDEV_MEDIA_LOADED) == 0)
		return EIO;

	switch (cmd) {
	case DIOCGDINFO:
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

		if (error = cdlock(cd))
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
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.flags &= ~CD_PA_SOTC;
		data.page.audio.flags |= CD_PA_IMMED;
		if (error = cd_set_mode(cd, &data))
			return error;
		return cd_play_tracks(cd, args->start_track, args->start_index,
		    args->end_track, args->end_index);
	}
	case CDIOCPLAYMSF: {
		struct ioc_play_msf *args
		= (struct ioc_play_msf *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.flags &= ~CD_PA_SOTC;
		data.page.audio.flags |= CD_PA_IMMED;
		if (error = cd_set_mode(cd, &data))
			return error;
		return cd_play_msf(cd, args->start_m, args->start_s,
		    args->start_f, args->end_m, args->end_s, args->end_f);
	}
	case CDIOCPLAYBLOCKS: {
		struct ioc_play_blocks *args
		= (struct ioc_play_blocks *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.flags &= ~CD_PA_SOTC;
		data.page.audio.flags |= CD_PA_IMMED;
		if (error = cd_set_mode(cd, &data))
			return error;
		return cd_play(cd, args->blk, args->len);
	}
	case CDIOCREADSUBCHANNEL: {
		struct ioc_read_subchannel *args
		= (struct ioc_read_subchannel *)addr;
		struct cd_sub_channel_info data;
		int len = args->data_len;
		if (len > sizeof(data) ||
		    len < sizeof(struct cd_sub_channel_header))
			return EINVAL;
		if (error = cd_read_subchannel(cd, args->address_format,
		    args->data_format, args->track, &data, len))
			return error;
		len = min(len, ((data.header.data_len[0] << 8) +
		    data.header.data_len[1] +
		    sizeof(struct cd_sub_channel_header)));
		return copyout(&data, args->data, len);
	}
	case CDIOREADTOCHEADER: {
		struct ioc_toc_header th;
		if (error = cd_read_toc(cd, 0, 0, &th, sizeof(th)))
			return error;
		th.len = ntohs(th.len);
		bcopy(&th, addr, sizeof(th));
		return 0;
	}
	case CDIOREADTOCENTRYS: {
		struct cd_toc {
			struct ioc_toc_header header;
			struct cd_toc_entry entries[65];
		} data;
		struct ioc_read_toc_entry *te =
		(struct ioc_read_toc_entry *)addr;
		struct ioc_toc_header *th;
		int len = te->data_len;
		th = &data.header;

		if (len > sizeof(data.entries) ||
		    len < sizeof(struct cd_toc_entry))
			return EINVAL;
		if (error = cd_read_toc(cd, te->address_format,
		    te->starting_track, (struct cd_toc_entry *)&data,
		    len + sizeof(struct ioc_toc_header)))
			return error;
		len = min(len, ntohs(th->len) - (sizeof(th->starting_track) +
		    sizeof(th->ending_track)));
		return copyout(data.entries, te->data, len);
	}
	case CDIOCSETPATCH: {
		struct ioc_patch *arg = (struct ioc_patch *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = arg->patch[0];
		data.page.audio.port[RIGHT_PORT].channels = arg->patch[1];
		data.page.audio.port[2].channels = arg->patch[2];
		data.page.audio.port[3].channels = arg->patch[3];
		return cd_set_mode(cd, &data);
	}
	case CDIOCGETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		arg->vol[LEFT_PORT] = data.page.audio.port[LEFT_PORT].volume;
		arg->vol[RIGHT_PORT] = data.page.audio.port[RIGHT_PORT].volume;
		arg->vol[2] = data.page.audio.port[2].volume;
		arg->vol[3] = data.page.audio.port[3].volume;
		return 0;
	}
	case CDIOCSETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = CHANNEL_0;
		data.page.audio.port[LEFT_PORT].volume = arg->vol[LEFT_PORT];
		data.page.audio.port[RIGHT_PORT].channels = CHANNEL_1;
		data.page.audio.port[RIGHT_PORT].volume = arg->vol[RIGHT_PORT];
		data.page.audio.port[2].volume = arg->vol[2];
		data.page.audio.port[3].volume = arg->vol[3];
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETMONO: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels =
		    LEFT_CHANNEL | RIGHT_CHANNEL | 4 | 8;
		data.page.audio.port[RIGHT_PORT].channels =
		    LEFT_CHANNEL | RIGHT_CHANNEL;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETSTEREO: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
		data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETMUTE: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = 0;
		data.page.audio.port[RIGHT_PORT].channels = 0;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETLEFT: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = LEFT_CHANNEL;
		data.page.audio.port[RIGHT_PORT].channels = LEFT_CHANNEL;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCSETRIGHT: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct cd_mode_data data;
		if (error = cd_get_mode(cd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.port[LEFT_PORT].channels = RIGHT_CHANNEL;
		data.page.audio.port[RIGHT_PORT].channels = RIGHT_CHANNEL;
		data.page.audio.port[2].channels = 0;
		data.page.audio.port[3].channels = 0;
		return cd_set_mode(cd, &data);
	}
	case CDIOCRESUME:
		return cd_pause(cd, 1);
	case CDIOCPAUSE:
		return cd_pause(cd, 0);
	case CDIOCSTART:
		return scsi_start(cd->sc_link, SSS_START, 0);
	case CDIOCSTOP:
		return scsi_start(cd->sc_link, SSS_STOP, 0);
	case CDIOCEJECT:
		return scsi_start(cd->sc_link, SSS_STOP|SSS_LOEJ, 0);
	case CDIOCALLOW:
		return scsi_prevent(cd->sc_link, PR_ALLOW, 0);
	case CDIOCPREVENT:
		return scsi_prevent(cd->sc_link, PR_PREVENT, 0);
	case CDIOCSETDEBUG:
		cd->sc_link->flags |= (SDEV_DB1 | SDEV_DB2);
		return 0;
	case CDIOCCLRDEBUG:
		cd->sc_link->flags &= ~(SDEV_DB1 | SDEV_DB2);
		return 0;
	case CDIOCRESET:
		return cd_reset(cd);

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
cdgetdisklabel(cd)
	struct cd_softc *cd;
{
	struct disklabel *lp = cd->sc_dk.dk_label;

	bzero(lp, sizeof(struct disklabel));
	bzero(cd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	lp->d_secsize = cd->params.blksize;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_ncylinders = (cd->params.disksize / 100) + 1;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "SCSI CD-ROM", 16);
	lp->d_type = DTYPE_SCSI;
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = cd->params.disksize;
	lp->d_rpm = 300;
	lp->d_interleave = 1;
	lp->d_flags = D_REMOVABLE;

	lp->d_partitions[0].p_offset = 0;
	lp->d_partitions[0].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[0].p_fstype = FS_ISO9660;
	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * (lp->d_secsize / DEV_BSIZE);
	lp->d_partitions[RAW_PART].p_fstype = FS_ISO9660;
	lp->d_npartitions = RAW_PART + 1;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);
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
	if (scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)&rdcap, sizeof(rdcap), CDRETRIES,
	    2000, NULL, flags | SCSI_DATA_IN) != 0)
		return 0;

	blksize = (rdcap.length_3 << 24) + (rdcap.length_2 << 16) +
	    (rdcap.length_1 << 8) + rdcap.length_0;
	if (blksize < 512)
		blksize = 2048;	/* some drives lie ! */
	cd->params.blksize = blksize;

	size = (rdcap.addr_3 << 24) + (rdcap.addr_2 << 16) +
	    (rdcap.addr_1 << 8) + rdcap.addr_0 + 1;
	if (size < 100)
		size = 400000;	/* ditto */
	cd->params.disksize = size;

	return size;
}

/*
 * Get the requested page into the buffer given
 */
int
cd_get_mode(cd, data, page)
	struct cd_softc *cd;
	struct cd_mode_data *data;
	int page;
{
	struct scsi_mode_sense scsi_cmd;
	int error;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	bzero(data, sizeof(*data));
	scsi_cmd.opcode = MODE_SENSE;
	scsi_cmd.page = page;
	scsi_cmd.length = sizeof(*data) & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)data, sizeof(*data), CDRETRIES, 20000,
	    NULL, SCSI_DATA_IN);
}

/*
 * Get the requested page into the buffer given
 */
int
cd_set_mode(cd, data)
	struct cd_softc *cd;
	struct cd_mode_data *data;
{
	struct scsi_mode_select scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SELECT;
	scsi_cmd.byte2 |= SMS_PF;
	scsi_cmd.length = sizeof(*data) & 0xff;
	data->header.data_length = 0;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)data, sizeof(*data), CDRETRIES, 20000,
	    NULL, SCSI_DATA_OUT);
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
	scsi_cmd.blk_addr[0] = (blkno >> 24) & 0xff;
	scsi_cmd.blk_addr[1] = (blkno >> 16) & 0xff;
	scsi_cmd.blk_addr[2] = (blkno >> 8) & 0xff;
	scsi_cmd.blk_addr[3] = blkno & 0xff;
	scsi_cmd.xfer_len[0] = (nblks >> 8) & 0xff;
	scsi_cmd.xfer_len[1] = nblks & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 200000, NULL, 0);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int
cd_play_big(cd, blkno, nblks)
	struct cd_softc *cd;
	int blkno, nblks;
{
	struct scsi_play_big scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PLAY_BIG;
	scsi_cmd.blk_addr[0] = (blkno >> 24) & 0xff;
	scsi_cmd.blk_addr[1] = (blkno >> 16) & 0xff;
	scsi_cmd.blk_addr[2] = (blkno >> 8) & 0xff;
	scsi_cmd.blk_addr[3] = blkno & 0xff;
	scsi_cmd.xfer_len[0] = (nblks >> 24) & 0xff;
	scsi_cmd.xfer_len[1] = (nblks >> 16) & 0xff;
	scsi_cmd.xfer_len[2] = (nblks >> 8) & 0xff;
	scsi_cmd.xfer_len[3] = nblks & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 20000, NULL, 0);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int
cd_play_tracks(cd, strack, sindex, etrack, eindex)
	struct cd_softc *cd;
	int strack, sindex, etrack, eindex;
{
	struct scsi_play_track scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PLAY_TRACK;
	scsi_cmd.start_track = strack;
	scsi_cmd.start_index = sindex;
	scsi_cmd.end_track = etrack;
	scsi_cmd.end_index = eindex;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 20000, NULL, 0);
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
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 2000, NULL, 0);
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
	int mode, format, len;
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
	scsi_cmd.data_len[0] = (len >> 8) & 0xff;
	scsi_cmd.data_len[1] = len & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(struct scsi_read_subchannel), (u_char *)data, len,
	    CDRETRIES, 5000, NULL, SCSI_DATA_IN);
}

/*
 * Read table of contents
 */
int
cd_read_toc(cd, mode, start, data, len)
	struct cd_softc *cd;
	int mode, start, len;
	struct cd_toc_entry *data;
{
	struct scsi_read_toc scsi_cmd;
	int ntoc;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	/*if (len!=sizeof(struct ioc_toc_header))
	 * ntoc=((len)-sizeof(struct ioc_toc_header))/sizeof(struct cd_toc_entry);
	 * else */
	ntoc = len;
	scsi_cmd.opcode = READ_TOC;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.from_track = start;
	scsi_cmd.data_len[0] = (ntoc >> 8) & 0xff;
	scsi_cmd.data_len[1] = ntoc & 0xff;
	return scsi_scsi_cmd(cd->sc_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(struct scsi_read_toc), (u_char *)data, len, CDRETRIES,
	    5000, NULL, SCSI_DATA_IN);
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
		return ENXIO;

	return 0;
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
