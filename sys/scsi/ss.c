/*	$NetBSD: ss.c,v 1.6 1996/02/19 00:06:07 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Kenneth Stailey.  All rights reserved.
 *   modified for configurable scanner support by Joachim Koenig
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
 *	This product includes software developed by Kenneth Stailey.
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
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/malloc.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>
#include <sys/conf.h>		/* for cdevsw */
#include <sys/scanio.h>

#include <scsi/scsi_all.h>
#include <scsi/scsi_scanner.h>
#include <scsi/scsiconf.h>
#include <scsi/ssvar.h>

#include <scsi/ss_mustek.h>

#define SSMODE(z)	( minor(z)       & 0x03)
#define SSUNIT(z)	((minor(z) >> 4)       )

/*
 * If the mode is 3 (e.g. minor = 3,7,11,15)
 * then the device has been openned to set defaults
 * This mode does NOT ALLOW I/O, only ioctls
 */
#define MODE_REWIND	0
#define MODE_NONREWIND	1
#define MODE_CONTROL	3

int ssmatch __P((struct device *, void *, void *));
void ssattach __P((struct device *, struct device *, void *));

struct cfdriver sscd = {
	NULL, "ss", ssmatch, ssattach, DV_DULL, sizeof(struct ss_softc)
};

void    ssstrategy __P((struct buf *));
void    ssstart __P((void *));

struct scsi_device ss_switch = {
	NULL,
	ssstart,
	NULL,
	NULL,
};

struct scsi_inquiry_pattern ss_patterns[] = {
	{T_SCANNER, T_FIXED,
	 "",         "",                 ""},
	{T_SCANNER, T_REMOV,
	 "",         "",                 ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C1750A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C2500A          ", ""},
};

int
ssmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct scsibus_attach_args *sa = aux;
	int priority;

	(void)scsi_inqmatch(sa->sa_inqbuf,
	    (caddr_t)ss_patterns, sizeof(ss_patterns)/sizeof(ss_patterns[0]),
	    sizeof(ss_patterns[0]), &priority);
	return (priority);
}

/*
 * The routine called by the low level scsi routine when it discovers
 * A device suitable for this driver
 * If it is a know special, call special attach routine to install
 * special handlers into the ss_softc structure
 */
void
ssattach(parent, self, aux)
	struct device *parent, *self;
	void *aux;
{
	struct ss_softc *ss = (void *)self;
	struct scsibus_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("ssattach: "));

	/*
	 * Store information needed to contact our base driver
	 */
	ss->sc_link = sc_link;
	sc_link->device = &ss_switch;
	sc_link->device_softc = ss;
	sc_link->openings = 1;

	/*
	 * look for non-standard scanners with help of the quirk table
	 * and install functions for special handling
	 */
	SC_DEBUG(sc_link, SDEV_DB2, ("ssattach:\n"));
	if (!bcmp(sa->sa_inqbuf->vendor, "MUSTEK  ", 8))
		mustek_attach(ss, sa);
	if (!bcmp(sa->sa_inqbuf->vendor, "HP      ", 8))
		scanjet_attach(ss, sa);
	if (ss->special == NULL) {
		/* XXX add code to restart a SCSI2 scanner, if any */
	}

	/*
	 * Set up the buf queue for this device
	 */
	ss->buf_queue.b_active = 0;
	ss->buf_queue.b_actf = 0;
	ss->buf_queue.b_actb = &ss->buf_queue.b_actf;
}

/*
 * open the device.
 */
int
ssopen(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	int unit;
	u_int ssmode;
	int error = 0;
	struct ss_softc *ss;
	struct scsi_link *sc_link;

	unit = SSUNIT(dev);
	if (unit >= sscd.cd_ndevs)
		return (ENXIO);
	ss = sscd.cd_devs[unit];
	if (!ss)
		return (ENXIO);

	ssmode = SSMODE(dev);
	sc_link = ss->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("open: dev=0x%x (unit %d (of %d))\n", dev,
	    unit, sscd.cd_ndevs));

	if (sc_link->flags & SDEV_OPEN) {
		printf("%s: already open\n", ss->sc_dev.dv_xname);
		return (EBUSY);
	}

	/*
	 * Catch any unit attention errors.
	 *
	 * SCSI_IGNORE_MEDIA_CHANGE: when you have an ADF, some scanners
	 * consider paper to be a changeable media
	 *
	 */
	error = scsi_test_unit_ready(sc_link,
	    SCSI_IGNORE_MEDIA_CHANGE | SCSI_IGNORE_ILLEGAL_REQUEST |
	    (ssmode == MODE_CONTROL ? SCSI_IGNORE_NOT_READY : 0));
	if (error)
		goto bad;

	sc_link->flags |= SDEV_OPEN;	/* unit attn are now errors */

	/*
	 * If the mode is 3 (e.g. minor = 3,7,11,15)
	 * then the device has been opened to set defaults
	 * This mode does NOT ALLOW I/O, only ioctls
	 */
	if (ssmode == MODE_CONTROL)
		return (0);

	SC_DEBUG(sc_link, SDEV_DB2, ("open complete\n"));
	return (0);

bad:
	sc_link->flags &= ~SDEV_OPEN;
	return (error);
}

/*
 * close the device.. only called if we are the LAST
 * occurence of an open device
 */
int
ssclose(dev)
	dev_t dev;
{
	struct ss_softc *ss = sscd.cd_devs[SSUNIT(dev)];
	int error;

	SC_DEBUG(ss->sc_link, SDEV_DB1, ("closing\n"));

	if (SSMODE(dev) == MODE_REWIND) {
		if (ss->special->rewind_scanner) {
			/* call special handler to rewind/abort scan */
			error = (ss->special->rewind_scanner)(ss);
			if (error)
				return (error);
		} else {
			/* XXX add code to restart a SCSI2 scanner, if any */
		}
		ss->sio.scan_window_size = 0;
		ss->flags &= ~SSF_TRIGGERED;
	}
	ss->sc_link->flags &= ~SDEV_OPEN;

	return (0);
}

/*
 * trim the size of the transfer if needed,
 * called by physio
 * basically the smaller of our min and the scsi driver's
 * minphys
 */
void
ssminphys(bp)
	struct buf *bp;
{
	register struct ss_softc *ss = sscd.cd_devs[SSUNIT(bp->b_dev)];

	(ss->sc_link->adapter->scsi_minphys)(bp);

	/*
	 * trim the transfer further for special devices this is
	 * because some scanners only read multiples of a line at a
	 * time, also some cannot disconnect, so the read must be
	 * short enough to happen quickly
	 */
	if (ss->special->minphys)
		(ss->special->minphys)(ss, bp);
}

/*
 * Do a read on a device for a user process.
 * Prime scanner at start of read, check uio values, call ssstrategy
 * via physio for the actual transfer.
 */
int
ssread(dev, uio, flag)
	dev_t dev;
	struct uio *uio;
	int flag;
{
	struct ss_softc *ss = sscd.cd_devs[SSUNIT(dev)];
	int error;

	/* if the scanner has not yet been started, do it now */
	if (!(ss->flags & SSF_TRIGGERED)) {
		if (ss->special->trigger_scanner) {
			error = (ss->special->trigger_scanner)(ss);
			if (error)
				return (error);
		}
		ss->flags |= SSF_TRIGGERED;
	}

	return (physio(ssstrategy, NULL, dev, B_READ, ssminphys, uio));
}

/*
 * Actually translate the requested transfer into one the physical
 * driver can understand The transfer is described by a buf and will
 * include only one physical transfer.
 */
void
ssstrategy(bp)
	struct buf *bp;
{
	struct ss_softc *ss = sscd.cd_devs[SSUNIT(bp->b_dev)];
	struct buf *dp;
	int s;

	SC_DEBUG(ss->sc_link, SDEV_DB1,
	    ("ssstrategy %d bytes @ blk %d\n", bp->b_bcount, bp->b_blkno));

	if (bp->b_bcount > ss->sio.scan_window_size)
		bp->b_bcount = ss->sio.scan_window_size;

	/*
	 * If it's a null transfer, return immediatly
	 */
	if (bp->b_bcount == 0)
		goto done;

	s = splbio();

	/*
	 * Place it in the queue of activities for this scanner
	 * at the end (a bit silly because we only have on user..
	 * (but it could fork()))
	 */
	dp = &ss->buf_queue;
	bp->b_actf = NULL;
	bp->b_actb = dp->b_actb;
	*dp->b_actb = bp;
	dp->b_actb = &bp->b_actf;

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 * (All a bit silly if we're only allowing 1 open but..)
	 */
	ssstart(ss);

	splx(s);
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
 * ssstart looks to see if there is a buf waiting for the device
 * and that the device is not already busy. If both are true,
 * It dequeues the buf and creates a scsi command to perform the
 * transfer required. The transfer request will call scsi_done
 * on completion, which will in turn call this routine again
 * so that the next queued transfer is performed.
 * The bufs are queued by the strategy routine (ssstrategy)
 *
 * This routine is also called after other non-queued requests
 * have been made of the scsi driver, to ensure that the queue
 * continues to be drained.
 * ssstart() is called at splbio
 */
void
ssstart(v)
	void *v;
{
	struct ss_softc *ss = v;
	struct scsi_link *sc_link = ss->sc_link;
	register struct buf *bp, *dp;

	SC_DEBUG(sc_link, SDEV_DB2, ("ssstart "));
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

		/*
		 * See if there is a buf with work for us to do..
		 */
		dp = &ss->buf_queue;
		if ((bp = dp->b_actf) == NULL)
			return;
		if ((dp = bp->b_actf) != NULL)
			dp->b_actb = bp->b_actb;
		else
			ss->buf_queue.b_actb = bp->b_actb;
		*bp->b_actb = dp;

		if (ss->special->read) {
			(ss->special->read)(ss, bp);
		} else {
			/* generic scsi2 scanner read */
			/* XXX add code for SCSI2 scanner read */
		}
	}
}

/*
 * Perform special action on behalf of the user;
 * knows about the internals of this device
 */
int
ssioctl(dev, cmd, addr, flag, p)
	dev_t dev;
	u_long cmd;
	caddr_t addr;
	int flag;
	struct proc *p;
{
	struct ss_softc *ss = sscd.cd_devs[SSUNIT(dev)];
	int error = 0;
	int unit;
	struct scan_io *sio;

	switch (cmd) {
	case SCIOCGET:
		if (ss->special->get_params) {
			/* call special handler */
			error = (ss->special->get_params)(ss);
			if (error)
				return (error);
		} else {
			/* XXX add code for SCSI2 scanner, if any */
			return (EOPNOTSUPP);
		}
		bcopy(&ss->sio, addr, sizeof(struct scan_io));
		break;
	case SCIOCSET:
		sio = (struct scan_io *)addr;

		if (ss->special->set_params) {
			/* call special handler */
			error = (ss->special->set_params)(ss, sio);
			if (error)
				return (error);
		} else {
			/* XXX add code for SCSI2 scanner, if any */
			return (EOPNOTSUPP);
		}
		break;
	case SCIOCRESTART:
		if (ss->special->rewind_scanner ) {
			/* call special handler */
			error = (ss->special->rewind_scanner)(ss);
			if (error)
				return (error);
		} else
			/* XXX add code for SCSI2 scanner, if any */
			return (EOPNOTSUPP);
		ss->flags &= ~SSF_TRIGGERED;
		break;
#ifdef NOTYET
	case SCAN_USE_ADF:
		break;
#endif
	default:
		if (SSMODE(dev) != MODE_CONTROL)
			return (ENOTTY);
		return (scsi_do_ioctl(ss->sc_link, dev, cmd, addr, flag, p));
	}
	return (error);
}
