/*	$OpenBSD: acd.c,v 1.4 1996/06/10 00:43:56 downsj Exp $	*/

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

#include <dev/atapi/atapilink.h>
#include <dev/atapi/atapi.h>

#define	CDUNIT(z)			DISKUNIT(z)
#define	CDPART(z)			DISKPART(z)
#define	MAKECDDEV(maj, unit, part)	MAKEDISKDEV(maj, unit, part)

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
	struct	at_dev_link *ad_link;	/* contains our drive number, etc ... */
	struct	cd_parms {
		int	blksize;
		u_long	disksize;	/* total number sectors */
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
void	acdstart __P((struct acd_softc *));
int	acd_pause __P((struct acd_softc *, int));
void	acdminphys __P((struct buf*));
u_long	acd_size __P((struct acd_softc*, int));
int	acddone __P((struct atapi_command_packet *));

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
	struct cfdata *cf = match;
	struct at_dev_link *sa = aux;

#ifdef ATAPI_DEBUG_PROBE
	printf("acdmatch: device %d\n",
	    sa->id.config.device_type & ATAPI_DEVICE_TYPE_MASK);
#endif

	if ((sa->id.config.device_type & ATAPI_DEVICE_TYPE_MASK) ==
	    ATAPI_DEVICE_TYPE_CD)
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
	struct mode_sense cmd;
	struct cappage cap;

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

	(void)atapi_test_unit_ready(sa, A_POLLED | A_SILENT); 
	delay(1000);
	(void)atapi_test_unit_ready(sa, A_POLLED | A_SILENT);

	/* To clear media change, etc ...*/
	bzero(&cmd, sizeof(cmd));
	cmd.operation_code = ATAPI_MODE_SENSE;
	cmd.page_code_control = CAP_PAGE;
	_lto2b(sizeof(cap), cmd.length);
	if (atapi_exec_cmd(sa, &cmd , sizeof(cmd), &cap, sizeof(cap),
	    B_READ, A_POLLED) != 0) {
		printf("%s: can't MODE SENSE: atapi_exec_cmd failed\n",
		    self->dv_xname);
		return;
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

	if ((error = atapi_test_unit_ready(ad_link,0)) != 0) {
		if (error != UNIT_ATTENTION)
			return EIO;
		if ((ad_link->flags & ADEV_OPEN) != 0)
			return EIO;
	}

	if (error = acdlock(acd))
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
		if (error = atapi_prevent(ad_link, PR_PREVENT))
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
acdstart(acd)
	struct acd_softc *acd;
{
	struct at_dev_link *ad_link;
	struct buf *bp = 0;
	struct buf *dp;
	struct read cmd;
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
			p =
			  &acd->sc_dk.dk_label->d_partitions[CDPART(bp->b_dev)];
			blkno += p->p_offset;
		}
		nblks = howmany(bp->b_bcount, acd->sc_dk.dk_label->d_secsize);

		ACD_DEBUG_PRINT(("acdstart: blkno %d nblk %d\n",
		    blkno, nblks));

		/*
		 *  Fill out the atapi command
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.operation_code = ATAPI_READ;
		_lto4b(blkno, cmd.lba);
		_lto2b(nblks, cmd.length);

		/* Instrumentation. */
		disk_busy(&acd->sc_dk);

		/*
		 * Call the routine that chats with the adapter.
		 * Note: we cannot sleep as we may be an interrupt
		 */
		 if (atapi_exec_io(ad_link, &cmd, sizeof(cmd), bp, A_NOSLEEP))
			printf("%s: not queued", acd->sc_dev.dv_xname);
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

		if (error = acdlock(acd))
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

#ifdef notyet
	case CDIOCPLAYTRACKS:
		{
		struct ioc_play_track *args = (struct ioc_play_track *)addr;
		struct acd_mode_data data;
		if (error = acd_get_mode(acd, &data, AUDIO_PAGE))
			return error;
		data.page.audio.flags &= ~CD_PA_SOTC;
		data.page.audio.flags |= CD_PA_IMMED;
		if (error = acd_set_mode(acd, &data))
			return error;
		return acd_play_tracks(acd, args->start_track,
		    args->start_index, args->end_track, args->end_index);
	}
#endif

#ifdef notyet
	case CDIOCPLAYMSF:
		{
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
#endif

#ifdef notyet
	case CDIOCPLAYBLOCKS:
	{
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
#endif

#ifdef notyet 
	case CDIOCREADSUBCHANNEL:
	{
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
#endif

#ifdef notyet
	case CDIOREADTOCHEADER:
	{
		struct ioc_toc_header th;
		if (error = cd_read_toc(cd, 0, 0, &th, sizeof(th)))
			return error;
		th.len = ntohs(th.len);
		bcopy(&th, addr, sizeof(th));
		return 0;
	}
#endif

#ifdef notyet
	case CDIOREADTOCENTRYS:
	{
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
#endif

#ifdef notyet
	case CDIOCSETPATCH:
	{
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
#endif

#ifdef notyet
	case CDIOCGETVOL:
	{
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
#endif

#ifdef notyet
	case CDIOCSETVOL:
	{
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
#endif

#ifdef notyet
	case CDIOCSETMONO:
	{
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
#endif

#ifdef notyet
	case CDIOCSETSTEREO:
	{
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
#endif

#ifdef notyet
	case CDIOCSETMUTE:
	{
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
#endif

#ifdef notyet
	case CDIOCSETLEFT:
	{
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
#endif

#ifdef notyet
	case CDIOCSETRIGHT:
	{
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
#endif

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
acdgetdisklabel(acd)
	struct acd_softc *acd;
{
	struct disklabel *lp = acd->sc_dk.dk_label;

	bzero(lp, sizeof(struct disklabel));
	bzero(acd->sc_dk.dk_cpulabel, sizeof(struct cpu_disklabel));

#if 0	/* XXX */
	lp->d_secsize = acd->params.blksize;
#endif
	lp->d_secsize = 2048;
	lp->d_ntracks = 1;
	lp->d_nsectors = 100;
	lp->d_ncylinders = (acd->params.disksize / 100) + 1;
	lp->d_secpercyl = lp->d_ntracks * lp->d_nsectors;

	strncpy(lp->d_typename, "ATAPI CD-ROM", 16);
	lp->d_type = DTYPE_SCSI;	/* XXX */
	strncpy(lp->d_packname, "fictitious", 16);
	lp->d_secperunit = acd->params.disksize;
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
acd_size(cd, flags)
	struct acd_softc *cd;
	int flags;
{
	struct read_cd_capacity_data rdcap;
	struct read_cd_capacity cmd;
	u_long blksize;
	u_long size;

	/*
	 * make up a atapi command and ask the atapi driver to do
	 * it for you.
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.operation_code = ATAPI_READ_CD_CAPACITY;
	cmd.len = sizeof(rdcap);

	/*
	 * If the command works, interpret the result as a 4 byte
	 * number of blocks and a blocksize
	 */
	if (atapi_exec_cmd(cd->ad_link, &cmd , sizeof(cmd),
	    &rdcap, sizeof(rdcap), B_READ, 0) != 0) {
		ATAPI_DEBUG_PRINT(("ATAPI_READ_CD_CAPACITY failed\n"));
		return 0;
	}

	blksize = ntohl(rdcap.blksize);
	if (blksize < 512)
		blksize = 2048;	/* some drives lie ! */
	cd->params.blksize = blksize;

	size = ntohl(rdcap.size);
	if (size < 100)
		size = 400000;	/* ditto */
	cd->params.disksize = size;

	ATAPI_DEBUG_PRINT(("acd_size: %ld %ld\n",blksize,size));
	return size;
}

#ifdef notyet
/*
 * Get the requested page into the buffer given
 */
int
cd_get_mode(cd, data, page)
	struct acd_softc *cd;
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
	return scsi_scsi_cmd(cd->ad_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)data, sizeof(*data), CDRETRIES, 20000,
	    NULL, SCSI_DATA_IN);
}

/*
 * Get the requested page into the buffer given
 */
int
cd_set_mode(cd, data)
	struct acd_softc *cd;
	struct cd_mode_data *data;
{
	struct scsi_mode_select scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = MODE_SELECT;
	scsi_cmd.byte2 |= SMS_PF;
	scsi_cmd.length = sizeof(*data) & 0xff;
	data->header.data_length = 0;
	return scsi_scsi_cmd(cd->ad_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), (u_char *)data, sizeof(*data), CDRETRIES, 20000,
	    NULL, SCSI_DATA_OUT);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int
cd_play(cd, blkno, nblks)
	struct acd_softc *cd;
	int blkno, nblks;
{
	struct scsi_play scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PLAY;
	_lto4b(blkno, scsi_cmd.blk_addr);
	_lto2b(nblks, scsi_cmd.xfer_len);

	return scsi_scsi_cmd(cd->ad_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 200000, NULL, 0);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int
cd_play_big(cd, blkno, nblks)
	struct acd_softc *cd;
	int blkno, nblks;
{
	struct scsi_play_big scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PLAY_BIG;
	_lto4b(blkno, scsi_cmd.blk_addr);
	_lto4b(nblks, scsi_cmd.xfer_len);

	return scsi_scsi_cmd(cd->ad_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 20000, NULL, 0);
}

/*
 * Get scsi driver to send a "start playing" command
 */
int
cd_play_tracks(cd, strack, sindex, etrack, eindex)
	struct acd_softc *cd;
	int strack, sindex, etrack, eindex;
{
	struct scsi_play_track scsi_cmd;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
	scsi_cmd.opcode = PLAY_TRACK;
	scsi_cmd.start_track = strack;
	scsi_cmd.start_index = sindex;
	scsi_cmd.end_track = etrack;
	scsi_cmd.end_index = eindex;

	return scsi_scsi_cmd(cd->ad_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 20000, NULL, 0);
}

/*
 * Get scsi driver to send a "play msf" command
 */
int
cd_play_msf(cd, startm, starts, startf, endm, ends, endf)
	struct acd_softc *cd;
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

	return scsi_scsi_cmd(cd->ad_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(scsi_cmd), 0, 0, CDRETRIES, 2000, NULL, 0);
}

#endif  /* notyet */

/*
 * Get atapi driver to send a "start up" command
 */
int
acd_pause(acd, go)
	struct acd_softc *acd;
	int go;
{
	struct pause_resume cmd;

	bzero(&cmd, sizeof(cmd));
	cmd.operation_code = ATAPI_PAUSE_RESUME;
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

#ifdef notyet
/*
 * Read subchannel
 */
int
cd_read_subchannel(cd, mode, format, track, data, len)
	struct acd_softc *cd;
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
	_lto2b(len, scsi_cmd.data_len);

	return scsi_scsi_cmd(cd->ad_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(struct scsi_read_subchannel), (u_char *)data, len,
	    CDRETRIES, 5000, NULL, SCSI_DATA_IN);
}

/*
 * Read table of contents
 */
int
cd_read_toc(cd, mode, start, data, len)
	struct acd_softc *cd;
	int mode, start, len;
	struct cd_toc_entry *data;
{
	struct scsi_read_toc scsi_cmd;
	int ntoc;

	bzero(&scsi_cmd, sizeof(scsi_cmd));
#if 0
	if (len != sizeof(struct ioc_toc_header))
		ntoc = ((len) - sizeof(struct ioc_toc_header)) /
		    sizeof(struct cd_toc_entry);
	else
#endif
		ntoc = len;

	scsi_cmd.opcode = READ_TOC;
	if (mode == CD_MSF_FORMAT)
		scsi_cmd.byte2 |= CD_MSF;
	scsi_cmd.from_track = start;
	_lto2b(ntoc, scsi_cmd.data_len);

	return scsi_scsi_cmd(cd->ad_link, (struct scsi_generic *)&scsi_cmd,
	    sizeof(struct scsi_read_toc), (u_char *)data, len, CDRETRIES,
	    5000, NULL, SCSI_DATA_IN);
}

#endif /* notyet */

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
acddone(acp)
	struct atapi_command_packet *acp;
{
	struct at_dev_link *ad_link = acp->ad_link;
	struct acd_softc *acd = ad_link->device_softc;

	if (acp->bp != NULL)
		disk_unbusy(&acd->sc_dk,
		    (acp->bp->b_bcount - acp->bp->b_resid));

	return (0);     
}
