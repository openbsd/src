/*	$OpenBSD: acd.c,v 1.32 1998/07/12 01:20:20 deraadt Exp $	*/

/*
 * Copyright (c) 1996 Manuel Bouyer.  All rights reserved.
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
 *  This product includes software developed by Manuel Bouyer.
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

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/kernel.h>
#include <sys/conf.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/mtio.h>
#include <sys/buf.h>
#include <sys/uio.h>
#include <sys/errno.h>
#include <sys/device.h>
#include <sys/disklabel.h>
#include <sys/disk.h>
#include <sys/cdio.h>
#include <sys/proc.h>  

#include <dev/atapi/atapilink.h>
#include <dev/atapi/atapi.h>

#include <ufs/ffs/fs.h>			/* for BBSIZE and SBSIZE */

#define	CDUNIT(z)			DISKUNIT(z)
#define	CDPART(z)			DISKPART(z)
#define	MAKECDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

#define MAXTRACK	99
#define CD_BLOCK_OFFSET	150
#define CD_FRAMES	75
#define CD_SECS		60
struct cd_toc {
	struct ioc_toc_header hdr;
	struct cd_toc_entry tab[MAXTRACK+1];	/* One extra for the leadout */
};

#define TOC_HEADER_LEN			0
#define TOC_HEADER_STARTING_TRACK	2
#define TOC_HEADER_ENDING_TRACK		3
#define TOC_HEADER_SZ			4

#define TOC_ENTRY_CONTROL_ADDR_TYPE	1
#define TOC_ENTRY_TRACK			2
#define TOC_ENTRY_MSF_LBA		4
#define TOC_ENTRY_SZ			8

#ifdef ACD_DEBUG
#define ACD_DEBUG_PRINT(args)		printf args
#else
#define ACD_DEBUG_PRINT(args)
#endif

#ifdef ATAPI_DEBUG
#define ATAPI_DEBUG_PRINT(args)		printf args
#else
#define ATAPI_DEBUG_PRINT(args)
#endif

struct acd_softc {
	struct	device sc_dev;
	struct	disk sc_dk;

	int flags;
#define	CDF_LOCKED	0x01
#define	CDF_WANTED	0x02
#define	CDF_WLABEL	0x04		/* label is writable */
#define	CDF_LABELLING	0x08		/* writing label */
#define CDF_NOTREADY	0x10		/* not ready at boot */
	struct	at_dev_link *ad_link;	/* contains our drive number, etc ... */
	struct	atapi_mode_data mode_page;	/* drive capabilities */

	struct	cd_parms {
		int	blksize;
		u_int32_t disksize;	/* total number sectors */
	} params;
	struct	buf buf_queue;
};

int	acdmatch __P((struct device *, void *, void *));
void	acdattach __P((struct device *, struct device *, void *));

struct cfattach acd_ca = {
	sizeof(struct acd_softc), acdmatch, acdattach
};

struct cfdriver acd_cd = {
	NULL, "acd", DV_DISK
};

void	acdgetdisklabel __P((struct acd_softc *));
int	acd_get_parms __P((struct acd_softc *, int));
void	acdstrategy __P((struct buf *));
void	acdstart __P((void *));
int	acd_pause __P((struct acd_softc *, int));
void	acdminphys __P((struct buf*));
u_int32_t acd_size __P((struct acd_softc*, int));
int	acddone __P((void *));
int	acdlock __P((struct acd_softc *));
void	acdunlock __P((struct acd_softc *));
int	acdopen __P((dev_t, int, int));
int	acdclose __P((dev_t, int, int));
int	acdread __P((dev_t, struct uio*));
int	acdwrite __P((dev_t, struct uio*));
int	acdioctl __P((dev_t, u_long, caddr_t, int, struct proc *));
int	acd_reset __P((struct acd_softc *));
int	acdsize __P((dev_t));
int	acddump __P((dev_t, daddr_t, caddr_t, size_t));
int	acd_get_mode __P((struct acd_softc *, struct atapi_mode_data *, int,
	    int, int));
int	acd_set_mode __P((struct acd_softc *, struct atapi_mode_data *, int));
int	acd_setchan __P((struct acd_softc *, u_char, u_char, u_char, u_char));
int	acd_play_big __P((struct acd_softc *, int, int));
int	acd_load_toc __P((struct acd_softc *, struct cd_toc *));
int	acd_play_tracks __P((struct acd_softc *, int, int, int, int));
int	acd_play_msf __P((struct acd_softc *, int, int, int, int, int, int));
int	acd_read_subchannel __P((struct acd_softc *, int, int, int,
	    struct cd_sub_channel_info *, int));
int	acd_read_toc __P((struct acd_softc *, int, int, void *, int));
#if 0
/* Not used anywhere, left here in case that changes. */
static void lba2msf __P((u_int32_t, u_int8_t *, u_int8_t *, u_int8_t *));
#endif
static __inline u_int32_t msf2lba __P((u_int8_t, u_int8_t, u_int8_t));

struct dkdriver acddkdriver = { acdstrategy };

/*
 * Called by the low level atapi code to find the right driver
 * for a drive on the bus.
 */
int
acdmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct at_dev_link *sa = aux;

#ifdef ATAPI_DEBUG_PROBE
	printf("acdmatch: device %d\n",
	    sa->id.config.device_type & ATAPI_DEVICE_TYPE_MASK);
#endif

	if (((sa->id.config.device_type & ATAPI_DEVICE_TYPE_MASK) ==
	    ATAPI_DEVICE_TYPE_CD) || (sa->quirks & AQUIRK_CDROM))
		return 1;
	return 0;
}

/*
 * The routine called by the low level atapi routine when it discovers
 * A device suitable for this driver
 */
void
acdattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct acd_softc *acd = (void *)self;
	struct at_dev_link *sa = aux;
	struct atapi_cappage *cap;

	printf("\n");

	sa->device_softc = acd;
	sa->start = acdstart;
	sa->done = acddone;
	sa->flags |= ADEV_REMOVABLE;
	sa->openings = 1;
	acd->ad_link = sa;

	/*
	 * Initialize and attach the disk structure.
	 */
	acd->sc_dk.dk_driver = &acddkdriver;
	acd->sc_dk.dk_name = acd->sc_dev.dv_xname;
	disk_attach(&acd->sc_dk);

	dk_establish(&acd->sc_dk, &acd->sc_dev);   

	if (atapi_test_unit_ready(sa, A_POLLED | A_SILENT) != 0) {
		/* To clear media change, etc ...*/
		delay(1000);
		if (atapi_test_unit_ready(sa, A_POLLED | A_SILENT) != 0)
			acd->flags |= CDF_NOTREADY;
	}

	if (acd_get_mode(acd, &acd->mode_page, ATAPI_CAP_PAGE, CAPPAGESIZE,
	    A_POLLED) != 0) {
		printf("%s: can't MODE SENSE: acd_get_mode failed\n",
		    self->dv_xname);
		return;
	}

	/*
	 * Display useful information about the drive (not media!).
	 */
	cap = &acd->mode_page.page_cap;

	/* Don't print anything unless it looks valid. */
	if (cap->cur_speed > 0) {
		printf ("%s: ", self->dv_xname);
		if (cap->cur_speed != cap->max_speed)
			printf ("%d/", cap->cur_speed * 1000 / 1024);
		printf ("%dKb/sec", cap->max_speed * 1000 / 1024);
		if (cap->buf_size)
			printf (", %dKb cache", cap->buf_size);
		if (cap->format_cap & FORMAT_AUDIO_PLAY)
			printf (", audio play");
		if (cap->max_vol_levels)
			printf (", %d volume levels", cap->max_vol_levels);
		printf ("\n");
	}

	/* We can autoprobe AQUIRK_NODOORLOCK */
	if (!(acd->ad_link->quirks & AQUIRK_NODOORLOCK)) {
		if (atapi_prevent(acd->ad_link, PR_PREVENT)) {
			acd->ad_link->quirks |= AQUIRK_NODOORLOCK;
			printf ("%s: disabling door locks.\n", self->dv_xname);
		} else
			atapi_prevent(acd->ad_link, PR_ALLOW);
	}
}

/*
 * Wait interruptibly for an exclusive lock.
 *
 * XXX
 * Several drivers do this; it should be abstracted and made MP-safe.
 */
int
acdlock(acd)
	struct acd_softc *acd;
{
	int error;

	while ((acd->flags & CDF_LOCKED) != 0) {
		acd->flags |= CDF_WANTED;
		if ((error = tsleep(acd, PRIBIO | PCATCH, "acdlck", 0)) != 0)
			return error;
	}
	acd->flags |= CDF_LOCKED;
	return 0;
}

/*
 * Unlock and wake up any waiters.
 */
void
acdunlock(acd)
	struct acd_softc *acd;
{

	acd->flags &= ~CDF_LOCKED;
	if ((acd->flags & CDF_WANTED) != 0) {
		acd->flags &= ~CDF_WANTED;
		wakeup(acd);
	}
}

/*
 * open the device. Make sure the partition info is a up-to-date as can be.
 */
int
acdopen(dev, flag, fmt)
	dev_t dev;
	int flag, fmt;
{
	struct acd_softc *acd;
	struct at_dev_link *ad_link;
	int unit, part;
	int error;

	ACD_DEBUG_PRINT(("acd: open\n"));

	unit = CDUNIT(dev);
	if (unit >= acd_cd.cd_ndevs)
		return ENXIO;
	acd = acd_cd.cd_devs[unit];
	if (acd == NULL)
		return ENXIO;

	ad_link = acd->ad_link;

	error = atapi_test_unit_ready(ad_link, A_SILENT);
	if ((error != 0) && (acd->flags & CDF_NOTREADY)) {
		/* Do it again. */
		delay(1000);
		error = atapi_test_unit_ready(ad_link, A_SILENT);
	}

	if (error != 0) {
		if (error != UNIT_ATTENTION)
			return EIO;
		if ((ad_link->flags & ADEV_OPEN) != 0)
			return EIO;
	}

	error = acdlock(acd);
	if (error)
		return error;

	if (acd->sc_dk.dk_openmask != 0) {
		/*
		 * If any partition is open, but the disk has been invalidated,
		 * disallow further opens.
		 */
		if ((ad_link->flags & ADEV_MEDIA_LOADED) == 0) {
			error = EIO;
			goto bad3;
		}
	} else {
		ad_link->flags |= ADEV_OPEN;

		/* Lock the pack in. */
		if ((error = atapi_prevent(ad_link, PR_PREVENT)) != 0)
			goto bad;

		if ((ad_link->flags & ADEV_MEDIA_LOADED) == 0) {
			ad_link->flags |= ADEV_MEDIA_LOADED;

			/* Load the physical device parameters. */
			if (acd_get_parms(acd, 0) != 0) {
				error = ENXIO;
				goto bad2;
			}

			/* Fabricate a disk label. */
			acdgetdisklabel(acd);
		}
	}

	part = CDPART(dev);

	/* Check that the partition exists. */
	if (part != RAW_PART &&
	    (part >= acd->sc_dk.dk_label->d_npartitions ||
	    acd->sc_dk.dk_label->d_partitions[part].p_fstype == FS_UNUSED)) {
		error = ENXIO;
		goto bad;
	}

	/* Insure only one open at a time. */
	switch (fmt) {
	case S_IFCHR:
		acd->sc_dk.dk_copenmask |= (1 << part);
		break;
	case S_IFBLK:
		acd->sc_dk.dk_bopenmask |= (1 << part);
		break;
	}
	acd->sc_dk.dk_openmask =
	    acd->sc_dk.dk_copenmask | acd->sc_dk.dk_bopenmask;

	ACD_DEBUG_PRINT(("acd: open complete\n"));

	acdunlock(acd);
	return 0;

bad2:
	ad_link->flags &= ~ADEV_MEDIA_LOADED;

bad:
	if (acd->sc_dk.dk_openmask == 0) {
		atapi_prevent(ad_link, PR_ALLOW);
		ad_link->flags &= ~ADEV_OPEN;
	}

bad3:
	acdunlock(acd);
	return error;
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
int
acdclose(dev, flag, fmt)
	dev_t dev;
	int flag, fmt;
{
	struct acd_softc *acd = acd_cd.cd_devs[CDUNIT(dev)];
	int part = CDPART(dev);
	int error;

	if ((error = acdlock(acd)) != 0)
		return error;

	switch (fmt) {
	case S_IFCHR:
		acd->sc_dk.dk_copenmask &= ~(1 << part);
		break;
	case S_IFBLK:
		acd->sc_dk.dk_bopenmask &= ~(1 << part);
		break;
	}
	acd->sc_dk.dk_openmask =
	    acd->sc_dk.dk_copenmask | acd->sc_dk.dk_bopenmask;

	if (acd->sc_dk.dk_openmask == 0) {
		/* XXXX Must wait for I/O to complete! */

		atapi_prevent(acd->ad_link, PR_ALLOW);
		acd->ad_link->flags &= ~ADEV_OPEN;

		if (acd->ad_link->flags & ADEV_EJECTING) {
			atapi_start_stop(acd->ad_link, SSS_STOP|SSS_LOEJ, 0);

			acd->ad_link->flags &= ~ADEV_EJECTING;
		}
	}

	acdunlock(acd);
	return 0;
}

/*
 * Actually translate the requested transfer into one the physical driver can
 * understand.  The transfer is described by a buf and will include only one
 * physical transfer.
 */
void
acdstrategy(bp)
	struct buf *bp;
{
	struct acd_softc *acd = acd_cd.cd_devs[CDUNIT(bp->b_dev)];
	int opri;

	ACD_DEBUG_PRINT(("acdstrategy\n"));

	/*
	 * The transfer must be a whole number of blocks.
	 */
	if ((bp->b_bcount % acd->sc_dk.dk_label->d_secsize) != 0) {
		bp->b_error = EINVAL;
		goto bad;
	}
	if ((bp->b_flags & (B_READ|B_WRITE)) == B_WRITE) {
		bp->b_error = EROFS;
		goto bad;
	}
	/*
	 * If the device has been made invalid, error out
	 * maybe the media changed
	 */
	if ((acd->ad_link->flags & ADEV_MEDIA_LOADED) == 0) {
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
	    bounds_check_with_label(bp, acd->sc_dk.dk_label,
	    acd->sc_dk.dk_cpulabel,
	    (acd->flags & (CDF_WLABEL|CDF_LABELLING)) != 0) <= 0)
		goto done;

	opri = splbio();

	/*
	 * Place it in the queue of disk activities for this disk
	 */
	disksort(&acd->buf_queue, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 */
	acdstart(acd);

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
 * acdstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It deques the buf and creates a atapi command to perform the
 * transfer in the buf. The transfer request will call atapi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (cdstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the atapi driver, to ensure that the queue
 * continues to be drained.
 *
 * must be called at the correct (highish) spl level
 * cdstart() is called at splbio from cdstrategy and atapi_done
 */
void
acdstart(vp)
	void *vp;
{
	struct acd_softc *acd = vp;
	struct at_dev_link *ad_link;
	struct buf *bp = 0;
	struct buf *dp;
	struct atapi_read cmd;
	u_int32_t blkno, nblks;
	struct partition *p;

	ACD_DEBUG_PRINT(("acd: acdstart\n"));

#ifdef DIAGNOSTIC
	if (acd == NULL) {
		printf("acdstart: null acd\n");
		return;
	}
#endif

	ad_link = acd->ad_link;

#ifdef DIAGNOSTIC
	if (ad_link == NULL) {
		printf("acdstart: null ad_link\n");
		return;
	}
#endif
	/*
	 * Check if the device has room for another command
	 */
	while (ad_link->openings > 0) {
		/*
		 * there is excess capacity, but a special waits
		 * It'll need the adapter as soon as we clear out of the
		 * way and let it run (user level wait).
		 */
		if (ad_link->flags & ADEV_WAITING) {
			ATAPI_DEBUG_PRINT(("acdstart: waking up\n"));

			ad_link->flags &= ~ADEV_WAITING;
			wakeup((caddr_t)ad_link);
			return;
		}
		
		/*
		 * See if there is a buf with work for us to do..
		 */
		dp = &acd->buf_queue;
#ifdef ACD_DEBUG
		if (dp == NULL) {
			printf("acdstart: null dp\n");
			return;
		}
#endif
		if ((bp = dp->b_actf) == NULL)	/* yes, an assign */
			return;

		ACD_DEBUG_PRINT(("acdstart: a buf\n"));

		dp->b_actf = bp->b_actf;

		/*
		 * If the device has become invalid, abort all the
		 * reads and writes until all files have been closed and
		 * re-opened
		 */
		if ((ad_link->flags & ADEV_MEDIA_LOADED) == 0) {
			bp->b_error = EIO;
			bp->b_flags |= B_ERROR;
			bp->b_resid = bp->b_bcount;
			biodone(bp);
			continue;
		}

		/*
		 *
		 * First, translate the block to absolute and put it in terms
		 * of the logical blocksize of the device.
		 */
		blkno =
		    bp->b_blkno / (acd->sc_dk.dk_label->d_secsize / DEV_BSIZE);
		if (CDPART(bp->b_dev) != RAW_PART) {
			p = &acd->sc_dk.dk_label->d_partitions[
			    CDPART(bp->b_dev)];
			blkno += p->p_offset;
		}
		nblks = howmany(bp->b_bcount, acd->sc_dk.dk_label->d_secsize);

		ACD_DEBUG_PRINT(("acdstart: blkno %d nblk %d\n", blkno,
		    nblks));

		/*
		 *  Fill out the atapi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.opcode = ATAPI_READ;
		_lto4b(blkno, cmd.lba);
		_lto2b(nblks, cmd.length);

		/* Instrumentation. */
		disk_busy(&acd->sc_dk);

		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		if (atapi_exec_io(ad_link, &cmd, sizeof(cmd), bp, A_NOSLEEP)) {
		 	disk_unbusy(&acd->sc_dk, 0);
			printf("%s: not queued", acd->sc_dev.dv_xname);
		}
	}
}

int
acdread(dev, uio)
	dev_t dev;
	struct uio *uio;
{

	return (physio(acdstrategy, NULL, dev, B_READ, acdminphys, uio));
}

int
acdwrite(dev, uio)
	dev_t dev;
	struct uio *uio;
{

	return (physio(acdstrategy, NULL, dev, B_WRITE, acdminphys, uio));
}

#if 0
/* Not used anywhere, left here in case that changes. */

/*
 * conversion between minute-seconde-frame and logical block adress
 * adresses format
 */
static void
lba2msf (lba, m, s, f)
	u_int32_t lba;
	u_int8_t *m, *s, *f;
{
	u_int32_t tmp;
	tmp = lba + CD_BLOCK_OFFSET;	/* offset of first logical frame */
	tmp &= 0xffffff;		/* negative lbas use only 24 bits */
	*m = tmp / (CD_SECS * CD_FRAMES);
	tmp %= (CD_SECS * CD_FRAMES);
	*s = tmp / CD_FRAMES;
	*f = tmp % CD_FRAMES;
}
#endif

static __inline u_int32_t
msf2lba (m, s, f)
	u_int8_t m, s, f;
{
	return (((m * CD_SECS) + s) * CD_FRAMES + f) - CD_BLOCK_OFFSET;
}

/*
 * Perform special action on behalf of the user.
 * Knows about the internals of this device
 */
int
acdioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct acd_softc *acd = acd_cd.cd_devs[CDUNIT(dev)];
	int error;

	/*
	 * If the device is not valid.. abandon ship
	 */
	if ((acd->ad_link->flags & ADEV_MEDIA_LOADED) == 0)
		return EIO;

	switch (cmd) {
	case DIOCGDINFO:
		*(struct disklabel *)addr = *acd->sc_dk.dk_label;
		return 0;

	case DIOCGPART:
		((struct partinfo *)addr)->disklab = acd->sc_dk.dk_label;
		((struct partinfo *)addr)->part =
		    &acd->sc_dk.dk_label->d_partitions[CDPART(dev)];
		return 0;

	case DIOCWDINFO:
	case DIOCSDINFO:
		if ((flag & FWRITE) == 0)
			return EBADF;

		if ((error = acdlock(acd)) != 0)
			return error;
		acd->flags |= CDF_LABELLING;

		error = setdisklabel(acd->sc_dk.dk_label,
		    (struct disklabel *)addr, /*acd->sc_dk.dk_openmask : */0,
		    acd->sc_dk.dk_cpulabel);
		if (error == 0) {
			/* XXX ?? */
		}

		acd->flags &= ~CDF_LABELLING;
		acdunlock(acd);
		return error;

	case DIOCWLABEL:
		return EROFS;

	case CDIOCPLAYTRACKS: {
		struct ioc_play_track *args = (struct ioc_play_track *)addr;

		return acd_play_tracks(acd, args->start_track,
		    args->start_index, args->end_track, args->end_index);
	}

	case CDIOCPLAYMSF: {
		struct ioc_play_msf *args = (struct ioc_play_msf *)addr;

		return acd_play_msf(acd, args->start_m, args->start_s,
		    args->start_f, args->end_m, args->end_s, args->end_f);
	}

	case CDIOCPLAYBLOCKS: {
		struct ioc_play_blocks *args = (struct ioc_play_blocks *)addr;

		return acd_play_big(acd, args->blk, args->len);
	}

	case CDIOCREADSUBCHANNEL: {
		struct ioc_read_subchannel *args =
		    (struct ioc_read_subchannel *)addr;
		struct cd_sub_channel_info data;
		int len = args->data_len;

		if (len > (int)sizeof(data) ||
		    len < (int)sizeof(struct cd_sub_channel_header))
			return EINVAL;

		error = acd_read_subchannel(acd, args->address_format,
		    args->data_format, args->track, &data, len);
		if (error)
			return error;
		return copyout(&data, args->data, len);
	}

	/* XXX Remove endian dependency */
	case CDIOREADTOCHEADER: {
		struct ioc_toc_header hdr;

		error = acd_read_toc(acd, 0, 0, &hdr, sizeof(hdr));
		if (error)
			return error;
		if (acd->ad_link->quirks & AQUIRK_LITTLETOC)
			bswap((u_int8_t *)&hdr.len, sizeof(hdr.len));
		bcopy(&hdr, addr, sizeof(hdr));
		return 0;
	}

	/* XXX Remove endian dependency */
	case CDIOREADTOCENTRYS: {
		struct ioc_read_toc_entry *te =
		    (struct ioc_read_toc_entry *)addr;
		struct cd_toc toc;
		struct ioc_toc_header *th = &toc.hdr;
		int len = te->data_len;
		int ntracks;

		if (len > (int)sizeof(toc.tab) ||
		    len < (int)sizeof(struct cd_toc_entry))
			return EINVAL;

		error = acd_read_toc(acd, te->address_format,
		    te->starting_track, &toc,
		    len + sizeof(struct ioc_toc_header));
		if (error)
			return error;

		if (te->address_format == CD_LBA_FORMAT) {
			for (ntracks =
			    th->ending_track - th->starting_track + 1;
		            ntracks >= 0; ntracks--) {
				toc.tab[ntracks].addr_type = CD_LBA_FORMAT;
				if (acd->ad_link->quirks & AQUIRK_LITTLETOC)
					bswap((u_int8_t*)
					    &toc.tab[ntracks].addr.addr,
					    sizeof(toc.tab[ntracks].addr.addr));
			}
		}
		if (acd->ad_link->quirks & AQUIRK_LITTLETOC)
			bswap((u_int8_t*)&th->len, sizeof(th->len));

		len = min(len, ntohs(th->len) - sizeof(struct ioc_toc_header));
		return copyout(toc.tab, te->data, len);
	}

	case CDIOCSETPATCH: {
		struct ioc_patch *arg = (struct ioc_patch *)addr;

		return acd_setchan(acd, arg->patch[0], arg->patch[1],
		    arg->patch[2], arg->patch[3]);
	}

	case CDIOCGETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct atapi_mode_data data;

		error = acd_get_mode(acd, &data, ATAPI_AUDIO_PAGE,
		    AUDIOPAGESIZE, 0);
		if (error)
			return error;
		arg->vol[0] = data.page_audio.port[0].volume;
		arg->vol[1] = data.page_audio.port[1].volume;
		arg->vol[2] = data.page_audio.port[2].volume;
		arg->vol[3] = data.page_audio.port[3].volume;
		return 0;
	}

	case CDIOCSETVOL: {
		struct ioc_vol *arg = (struct ioc_vol *)addr;
		struct atapi_mode_data data, mask;

		error = acd_get_mode(acd, &data, ATAPI_AUDIO_PAGE,
		    AUDIOPAGESIZE, 0);
		if (error)
			return error;

		error = acd_get_mode(acd, &mask, ATAPI_AUDIO_PAGE_MASK,
		    AUDIOPAGESIZE, 0);
		if (error)
			return error;

		data.page_audio.port[0].volume = arg->vol[0] &
		    mask.page_audio.port[0].volume;
		data.page_audio.port[1].volume = arg->vol[1] &
		    mask.page_audio.port[1].volume;
		data.page_audio.port[2].volume = arg->vol[2] &
		    mask.page_audio.port[2].volume;
		data.page_audio.port[3].volume = arg->vol[3] &
		    mask.page_audio.port[3].volume;

		return acd_set_mode(acd, &data, AUDIOPAGESIZE);
	}

	case CDIOCSETMONO: {
		return acd_setchan(acd, BOTH_CHANNEL, BOTH_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL);
	}

	case CDIOCSETSTEREO: {
		return acd_setchan(acd, LEFT_CHANNEL, RIGHT_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL);
	}

	case CDIOCSETMUTE: {
		return acd_setchan(acd, MUTE_CHANNEL, MUTE_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL);
	}

	case CDIOCSETLEFT: {
		return acd_setchan(acd, LEFT_CHANNEL, LEFT_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL);
	}

	case CDIOCSETRIGHT: {
		return acd_setchan(acd, RIGHT_CHANNEL, RIGHT_CHANNEL,
		    MUTE_CHANNEL, MUTE_CHANNEL);
	}

	case CDIOCRESUME:
		return acd_pause(acd, PA_RESUME);

	case CDIOCPAUSE:
		return acd_pause(acd, PA_PAUSE);

	case CDIOCSTART:
		return atapi_start_stop(acd->ad_link, SSS_START, 0);

	case CDIOCSTOP:
		return atapi_start_stop(acd->ad_link, SSS_STOP, 0);

	case MTIOCTOP:
		if (((struct mtop *)addr)->mt_op != MTOFFL)
			return EIO;
		/* FALLTHROUGH */

	case CDIOCEJECT:	/* FALLTHROUGH */
	case DIOCEJECT:
		acd->ad_link->flags |= ADEV_EJECTING;
		return 0;

	case CDIOCALLOW:
		return atapi_prevent(acd->ad_link, PR_ALLOW);

	case CDIOCPREVENT:
		return atapi_prevent(acd->ad_link, PR_PREVENT);

	case DIOCLOCK:
		return atapi_prevent(acd->ad_link,
		    (*(int *)addr) ? PR_PREVENT : PR_ALLOW);

	case CDIOCRESET:
		return acd_reset(acd);

	default:
		return ENOTTY;
	}

#ifdef DIAGNOSTIC
	panic("acdioctl: impossible");
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
acdgetdisklabel(acd)
	struct acd_softc *acd;
{
	struct disklabel *lp = acd->sc_dk.dk_label;
	char *errstring;
	u_int8_t hdr[TOC_HEADER_SZ], *toc, *ent;
	u_int32_t lba, nlba;
	int i, n, len, is_data, data_track = -1;

	bzero(lp, sizeof(struct disklabel));
	bzero(acd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

	lp->d_secsize = acd->params.blksize;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_ncylinders = (acd->params.disksize / 100) + 1;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;
	if (lp->d_secpercyl == 0) {
		lp->d_secpercyl = 100;
		/* as long as it's not 0 - readdisklabel divides by it (?) */
	}

	strncpy(lp->d_typename, "ATAPI CD-ROM", 16);
	lp->d_type = DTYPE_SCSI;	/* XXX */
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = acd->params.disksize;
	lp->d_rpm = 300;
	lp->d_interleave = 1;
	lp->d_flags = D_REMOVABLE;

	lp->d_magic = DISKMAGIC;
	lp->d_magic2 = DISKMAGIC;
	lp->d_checksum = dkcksum(lp);

	/*
	 * Read the TOC and loop throught the individual tracks and lay them
	 * out in our disklabel.  If there is a data track, call the generic
	 * disklabel read routine.  XXX should we move all data tracks up front
	 * before any other tracks?
	 */
	if (acd_read_toc(acd, 0, 0, hdr, TOC_HEADER_SZ))
		return;
	n = min(hdr[TOC_HEADER_ENDING_TRACK] - hdr[TOC_HEADER_STARTING_TRACK] +
	   1, MAXPARTITIONS);
	len = TOC_HEADER_SZ + (n + 1) * TOC_ENTRY_SZ;
	MALLOC(toc, u_int8_t *, len, M_TEMP, M_WAITOK);
	if (acd_read_toc (acd, CD_LBA_FORMAT, 0, toc, len))
		goto done;

	/* XXX - these values for BBSIZE and SBSIZE assume ffs */
	lp->d_bbsize = BBSIZE;
	lp->d_sbsize = SBSIZE;

	/* The raw partition is special.  */
	lp->d_partitions[RAW_PART].p_offset = 0;
	lp->d_partitions[RAW_PART].p_size =
	    lp->d_secperunit * lp->d_secsize / DEV_BSIZE;
	lp->d_partitions[RAW_PART].p_fstype = FS_UNUSED;

	/* Create the partition table.  */
	lp->d_npartitions = max(RAW_PART, n) + 1;
	ent = toc + TOC_HEADER_SZ;
	lba = ((acd->ad_link->quirks & AQUIRK_LITTLETOC) ?
	    ent[TOC_ENTRY_MSF_LBA] | ent[TOC_ENTRY_MSF_LBA + 1] << 8 |
	    ent[TOC_ENTRY_MSF_LBA + 2] << 16 |
	    ent[TOC_ENTRY_MSF_LBA + 3] << 24 :
	    ent[TOC_ENTRY_MSF_LBA] << 24 | ent[TOC_ENTRY_MSF_LBA + 1] << 16 |
	    ent[TOC_ENTRY_MSF_LBA + 2] << 8 | ent[TOC_ENTRY_MSF_LBA + 3]) *
	    lp->d_secsize / DEV_BSIZE;

	for (i = 0; i < (n > RAW_PART + 1 ? n + 1 : n); i++) {
		/* The raw partition was specially handled above.  */
		if (i != RAW_PART) {
			is_data = toc[TOC_HEADER_SZ +
			    TOC_ENTRY_CONTROL_ADDR_TYPE] & 4;
			lp->d_partitions[i].p_fstype =
			    is_data ? FS_UNUSED : FS_OTHER;
			if (is_data && data_track == -1)
				data_track = i;
			ent += TOC_ENTRY_SZ;
			nlba = ((acd->ad_link->quirks & AQUIRK_LITTLETOC) ?
			    ent[TOC_ENTRY_MSF_LBA] |
			    ent[TOC_ENTRY_MSF_LBA + 1] << 8 |
			    ent[TOC_ENTRY_MSF_LBA + 2] << 16 |
			    ent[TOC_ENTRY_MSF_LBA + 3] << 24 :
			    ent[TOC_ENTRY_MSF_LBA] << 24 |
			    ent[TOC_ENTRY_MSF_LBA + 1] << 16 |
			    ent[TOC_ENTRY_MSF_LBA + 2] << 8 |
			    ent[TOC_ENTRY_MSF_LBA + 3]) * lp->d_secsize /
			    DEV_BSIZE;
			lp->d_partitions[i].p_offset = lba;
			lp->d_partitions[i].p_size = nlba - lba;
			lba = nlba;
		}
	}

	/* We have a data track, look in there for a real disklabel.  */
	if (data_track != -1) {
#ifdef notyet
		/*
		 * Reading a disklabel inside the track we setup above
		 * does not yet work, for unknown reasons.
		 */
		errstring = readdisklabel(MAKECDDEV(0, acd->sc_dev.dv_unit,
		    data_track), acdstrategy, lp, acd->sc_dk.dk_cpulabel);
#else
		errstring = readdisklabel(MAKECDDEV(0, acd->sc_dev.dv_unit,
		    RAW_PART), acdstrategy, lp, acd->sc_dk.dk_cpulabel);
#endif
		/*if (errstring)
			printf("%s: %s\n", acd->sc_dev.dv_xname, errstring);*/
	}

done:
	FREE(toc, M_TEMP);
}

/*
 * Find out from the device what it's capacity is
 */
u_int32_t
acd_size(acd, flags)
	struct acd_softc *acd;
	int flags;
{
	struct atapi_read_cd_capacity_data rdcap;
	struct atapi_read_cd_capacity cmd;
	int result;

	if (acd->ad_link->quirks & AQUIRK_NOCAPACITY) {
		/*
		 * the drive doesn't support the READ_CD_CAPACITY command
		 * use a fake size
		 */
		acd->params.blksize = 2048;
		acd->params.disksize = 400000;

		return 400000;
	}

	/*
	 * make up a atapi command and ask the atapi driver to do
	 * it for you.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = ATAPI_READ_CD_CAPACITY;
	cmd.len = sizeof(rdcap);

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks and a blocksize
	 */
	result = atapi_exec_cmd(acd->ad_link, &cmd, sizeof(cmd),
				&rdcap, sizeof(rdcap), B_READ, 0);
	if (result != 0) {
		u_int8_t error = result >> 8;
		/* Get the sense key and check for an illegal request */
		if ((error >> 4) == ATAPI_SK_ILLEGAL_REQUEST) {
			acd->ad_link->quirks |= AQUIRK_NOCAPACITY;
			return acd_size(acd, flags);
		}
		ATAPI_DEBUG_PRINT(("ATAPI_READ_CD_CAPACITY failed\n"));
		return 0;
	}

	acd->params.blksize = _4btol((u_int8_t*)&rdcap.blksize);
	if (acd->params.blksize < 512 || acd->params.blksize > 2048)
		acd->params.blksize = 2048;	/* some drives lie ! */
	acd->params.disksize = _4btol((u_int8_t*)&rdcap.size);

	ATAPI_DEBUG_PRINT(("acd_size: %ld %ld\n", acd->params.blksize,
	    acd->params.disksize));
	return acd->params.disksize;
}

/*
 * Get the requested page into the buffer given
 */
int
acd_get_mode(acd, data, page, len, flags)
	struct acd_softc *acd;
	struct atapi_mode_data *data;
	int page, len, flags;
{
	struct atapi_mode_sense atapi_cmd;
	int error;

	bzero(&atapi_cmd, sizeof(atapi_cmd));
	bzero(data, sizeof(struct atapi_mode_data));
	atapi_cmd.opcode = ATAPI_MODE_SENSE;
	atapi_cmd.page_code_control = page;
	_lto2b(len, atapi_cmd.length);

	error = atapi_exec_cmd(acd->ad_link, &atapi_cmd, sizeof(atapi_cmd),
	    data, len, B_READ, flags);
	if (!error) {
		switch(page) {
		case ATAPI_CAP_PAGE: {
			struct atapi_cappage *fix = &data->page_cap;

        		/*
	         	 * Fix cappage entries in place.
			 */
			fix->max_speed = _2btos((u_int8_t *)&fix->max_speed);
			fix->max_vol_levels =
			    _2btos((u_int8_t *)&fix->max_vol_levels);
			fix->buf_size = _2btos((u_int8_t *)&fix->buf_size);
			fix->cur_speed = _2btos((u_int8_t *)&fix->cur_speed);
			break;
		      }
		}
	}

	return(error);
}

/*
 * Get the requested page into the buffer given
 */
int
acd_set_mode(acd, data, len)
	struct acd_softc *acd;
	struct atapi_mode_data *data;
	int len;
{
	struct atapi_mode_select atapi_cmd;

	bzero(&data->header.length, sizeof(data->header.length));
	bzero(&atapi_cmd, sizeof(atapi_cmd));

	atapi_cmd.opcode = ATAPI_MODE_SELECT;
	atapi_cmd.flags |= MODE_BIT;
	atapi_cmd.page = data->page_code;
	_lto2b(len, atapi_cmd.length);

	return atapi_exec_cmd(acd->ad_link, &atapi_cmd, sizeof(atapi_cmd),
	    data, len, B_WRITE, 0);
}

int
acd_setchan(acd, c0, c1, c2, c3)
	struct acd_softc *acd;
	u_char c0, c1, c2, c3;
{
	struct atapi_mode_data data;
	int error;

	error = acd_get_mode(acd, &data, ATAPI_AUDIO_PAGE, AUDIOPAGESIZE, 0);
	if (error)
		return error;

	data.page_audio.port[0].channels = c0;
	data.page_audio.port[1].channels = c1;
	data.page_audio.port[2].channels = c2;
	data.page_audio.port[3].channels = c3;

	return acd_set_mode(acd, &data, AUDIOPAGESIZE);
}

/*
 * Get atapi driver to send a "start playing" command
 */
int
acd_play_big(acd, blkno, nblks)
	struct acd_softc *acd;
	int blkno, nblks;
{
	struct atapi_play_big atapi_cmd;

	bzero(&atapi_cmd, sizeof(atapi_cmd));
	atapi_cmd.opcode = ATAPI_PLAY_BIG;
	_lto4b(blkno, atapi_cmd.lba);
	_lto4b(nblks, atapi_cmd.length);

	return atapi_exec_cmd(acd->ad_link, &atapi_cmd, sizeof(atapi_cmd),
	    NULL, 0, 0, 0);
}

int
acd_load_toc(acd, toc)
	struct acd_softc *acd;
	struct cd_toc *toc;
{
	int i, ntracks, len, error;
	u_int32_t *lba;

	error = acd_read_toc(acd, 0, 0, toc, sizeof(toc->hdr));
	if (error)
		return error;

	ntracks = toc->hdr.ending_track-toc->hdr.starting_track + 1;
	len = (ntracks+1) * sizeof(struct cd_toc_entry) + sizeof(toc->hdr);
	error = acd_read_toc(acd, CD_MSF_FORMAT, 0, toc, len);
	if (error)
		return(error);
	for (i = 0; i <= ntracks; i++) {
		lba = (u_int32_t*)toc->tab[i].addr.addr;
		*lba = msf2lba(toc->tab[i].addr.addr[1],
		    toc->tab[i].addr.addr[2], toc->tab[i].addr.addr[3]);
	}
	return 0;
}

/*
 * Get atapi driver to send a "start playing" command
 */
int
acd_play_tracks(acd, strack, sindex, etrack, eindex)
	struct acd_softc *acd;
	int strack, sindex, etrack, eindex;
{
	u_int32_t *start, *end, len;
	struct cd_toc toc;
	int error;

	if (!etrack)
		return EIO;
	if (strack > etrack)
		return EINVAL;

	error = acd_load_toc(acd, &toc);
	if (error)
		return error;

	if (++etrack > (toc.hdr.ending_track + 1))
		etrack = toc.hdr.ending_track + 1;

	strack -= toc.hdr.starting_track;
	etrack -= toc.hdr.starting_track;
	if (strack < 0)
		return EINVAL;

	start = (u_int32_t*)toc.tab[strack].addr.addr;
	end = (u_int32_t*)toc.tab[etrack].addr.addr;
	len = *end - *start;

	return acd_play_big(acd, *start, len);
}

/*
 * Get atapi driver to send a "play msf" command
 */
int
acd_play_msf(acd, startm, starts, startf, endm, ends, endf)
	struct acd_softc *acd;
	int startm, starts, startf, endm, ends, endf;
{
	struct atapi_play_msf atapi_cmd;

	bzero(&atapi_cmd, sizeof(atapi_cmd));
	atapi_cmd.opcode = ATAPI_PLAY_MSF;
	atapi_cmd.start_m = startm;
	atapi_cmd.start_s = starts;
	atapi_cmd.start_f = startf;
	atapi_cmd.end_m = endm;
	atapi_cmd.end_s = ends;
	atapi_cmd.end_f = endf;

	return atapi_exec_cmd(acd->ad_link, (struct atapi_generic *)&atapi_cmd,
	    sizeof(atapi_cmd), NULL, 0, 0, 0);
}

/*
 * Get atapi driver to send a "start up" command
 */
int
acd_pause(acd, go)
	struct acd_softc *acd;
	int go;
{
	struct atapi_pause_resume cmd;

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = ATAPI_PAUSE_RESUME;
	cmd.resume = go & 0xff;

	return atapi_exec_cmd(acd->ad_link, &cmd , sizeof(cmd), 0, 0, 0, 0);
}

/*
 * Get atapi driver to send a "RESET" command
 */
int
acd_reset(acd)
	struct acd_softc *acd;
{
#ifdef notyet	
	return atapi_soft_reset(acd->ad_link);
#else
 	return 0;
#endif
}

/*
 * Read subchannel
 */
int
acd_read_subchannel(acd, mode, format, track, data, len)
	struct acd_softc *acd;
	int mode, format, len;
	struct cd_sub_channel_info *data;
{
	struct atapi_read_subchannel atapi_cmd;

	bzero(&atapi_cmd, sizeof(atapi_cmd));

	atapi_cmd.opcode = ATAPI_READ_SUBCHANNEL;
	if (mode == CD_MSF_FORMAT)
		atapi_cmd.flags[0] |= SUBCHAN_MSF;
	if (len > (int)sizeof(struct cd_sub_channel_header))
		atapi_cmd.flags[1] |= SUBCHAN_SUBQ;
	atapi_cmd.subchan_format = format;
	atapi_cmd.track = track;
	_lto2b(len, atapi_cmd.length);

	return atapi_exec_cmd(acd->ad_link, (struct atapi_generic *)&atapi_cmd,
	    sizeof(struct atapi_read_subchannel), (u_char *)data, len, B_READ,
	    0);
}

/*
 * Read table of contents
 */
int
acd_read_toc(acd, mode, start, data, len)
	struct acd_softc *acd;
	int mode, start, len;
	void *data;
{
	struct atapi_read_toc atapi_cmd;

	bzero(&atapi_cmd, sizeof(atapi_cmd));

	atapi_cmd.opcode = ATAPI_READ_TOC;
	if (mode == CD_MSF_FORMAT)
		atapi_cmd.flags |= TOC_MSF;
	atapi_cmd.track = start;
	_lto2b(len, atapi_cmd.length);

	return atapi_exec_cmd(acd->ad_link, (struct atapi_generic *)&atapi_cmd,
	    sizeof(struct atapi_read_toc), data, len, B_READ, 0);
}

/*
 * Get the atapi driver to send a full inquiry to the device and use the
 * results to fill out the disk parameter structure.
 */
int
acd_get_parms(acd, flags)
	struct acd_softc *acd;
	int flags;
{
	/*
	 * give a number of sectors so that sec * trks * cyls
	 * is <= disk_size
	 */
	if (acd_size(acd, flags) == 0)
		return ENXIO;

	return 0;
}

int
acdsize(dev)
	dev_t dev;
{
	/* CD-ROMs are read-only. */
	return -1;
}

void acdminphys(bp)
	struct buf *bp;
{
	minphys(bp);
}

int
acddump(dev, blkno, va, size)
	dev_t dev;
	daddr_t blkno;
	caddr_t va;
	size_t size;
{
	/* Not implemented. */
	return ENXIO;
}

int
acddone(vp)
	void *vp;
{
	struct atapi_command_packet *acp = vp;
	struct at_dev_link *ad_link = acp->ad_link;
	struct acd_softc *acd = ad_link->device_softc;

	if (acp->bp != NULL)
		disk_unbusy(&acd->sc_dk,
		    (acp->bp->b_bcount - acp->bp->b_resid));

	return (0);     
}
