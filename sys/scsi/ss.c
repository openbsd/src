/*	$OpenBSD: ss.c,v 1.74 2010/06/15 04:11:34 dlg Exp $	*/
/*	$NetBSD: ss.c,v 1.10 1996/05/05 19:52:55 christos Exp $	*/

/*
 * Copyright (c) 1995, 1997 Kenneth Stailey.  All rights reserved.
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

#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
#include <sys/buf.h>
#include <sys/proc.h>
#include <sys/user.h>
#include <sys/device.h>
#include <sys/conf.h>
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

struct quirkdata {
	char *name;
	u_int quirks;
#define SS_Q_WINDOW_DESC_LEN	0x0001 /* needs special WDL */
#define SS_Q_BRIGHTNESS		0x0002 /* needs special value for brightness */
#define SS_Q_REV_BRIGHTNESS	0x0004 /* reverse brightness control in s/w */
#define SS_Q_THRESHOLD		0x0008 /* needs special value for threshold */
#define SS_Q_MONO_THRESHOLD	0x0010 /* same as SS_Q_THRESHOLD but only
					* for monochrome image data */
#define SS_Q_CONTRAST		0x0020 /* needs special value for contrast */
#define SS_Q_REV_CONTRAST	0x0040 /* reverse contrast control in s/w */
#define SS_Q_HALFTONE		0x0080 /* uses non-zero halftone */
#define SS_Q_SET_RIF		0x0100 /* set RIF bit */
#define SS_Q_PADDING_TYPE	0x0200 /* does not truncate to byte boundary */
#define SS_Q_BIT_ORDERING	0x0400 /* needs non-zero bit ordering */
	long window_descriptor_length;
	u_int8_t brightness;
	u_int8_t threshold;
	u_int8_t contrast;
	u_int8_t halftone_pattern[2];
	int pad_type;
	long bit_ordering;
	u_int8_t scanner_type;
	/*
	 * To enable additional scanner options, point vendor_unique_sw
	 * at a function that adds more stuff to the SET_WINDOW parameters.
	 */
	int	(*vendor_unique_sw)(struct ss_softc *, struct scan_io *,
					struct scsi_set_window *, void *);
	/*
	 * If the scanner requires use of GET_BUFFER_STATUS before READ
	 * it can be called from ss_minphys().
	 */
	void	(*special_minphys)(struct ss_softc *, struct buf *);

	int	(*compute_sizes)(void);
};

struct ss_quirk_inquiry_pattern {
	struct scsi_inquiry_pattern pattern;
	struct quirkdata quirkdata;
};

struct  quirkdata ss_gen_quirks = {
	"generic", 0, 0, 0, 0, 0,
	{0, 0}, 0, 0, GENERIC_SCSI2,
	NULL, NULL, NULL
};

void    ssstrategy(struct buf *);
void    ssstart(void *);
void	ssdone(struct scsi_xfer *);
void	ssminphys(struct buf *);

void	ss_identify_scanner(struct ss_softc *, struct scsi_inquiry_data*);
int	ss_set_window(struct ss_softc *, struct scan_io *);

int	ricoh_is410_sw(struct ss_softc *, struct scan_io *,
			    struct scsi_set_window *, void *);
int	umax_uc630_sw(struct ss_softc *, struct scan_io *,
			   struct scsi_set_window *, void *);
#ifdef NOTYET	/* for ADF support  */
int	fujitsu_m3096g_sw(struct ss_softc *, struct scan_io *,
			       struct scsi_set_window *, void *);
#endif

void	get_buffer_status(struct ss_softc *, struct buf *);

/*
 * WDL:
 *
 *  Ricoh IS-50 & IS-410 insist on 320 (even it transfer len is less.)
 *  Ricoh FS-1 accepts 256 (I haven't tested other values.)
 *  UMAX UC-630 accepts 46 (I haven't tested other values.)
 *  Fujitsu M3096G wants 40 <= x <= 248 (tested OK at 40 & 64.)
 */

const struct ss_quirk_inquiry_pattern ss_quirk_patterns[] = {
	{{T_SCANNER, T_FIXED,
	 "ULTIMA  ", "AT3     1.60    ", "    "}, {
		 "Ultima AT3",
		 SS_Q_HALFTONE |
		 SS_Q_PADDING_TYPE,
		 0, 0, 0, 0, { 3, 0 }, 0, 0,
		 ULTIMA_AT3,
		 NULL, NULL, NULL
	 }},
	{{T_SCANNER, T_FIXED,
	 "ULTIMA  ", "A6000C PLUS     ", "    "}, {
		 "Ultima A6000C",
		 SS_Q_HALFTONE |
		 SS_Q_PADDING_TYPE,
		 0, 0, 0, 0, { 3, 0 }, 0, 0,
		 ULTIMA_AC6000C,
		 NULL, NULL, NULL
	 }},
	{{T_SCANNER, T_FIXED,
	 "RICOH   ", "IS50            ", "    "}, {
		 "Ricoh IS-50",
		 SS_Q_WINDOW_DESC_LEN |
		 SS_Q_REV_BRIGHTNESS |
		 SS_Q_THRESHOLD |
		 SS_Q_REV_CONTRAST |
		 SS_Q_HALFTONE |
		 SS_Q_BIT_ORDERING,
		 320, 0, 0, 0, { 2, 0x0a }, 0, 7,
		 RICOH_IS50,
		 ricoh_is410_sw, get_buffer_status, NULL
	 }},
	{{T_SCANNER, T_FIXED,
	 "RICOH   ", "IS410           ", "    "}, {
		 "Ricoh IS-410",
		 SS_Q_WINDOW_DESC_LEN |
		 SS_Q_THRESHOLD |
		 SS_Q_HALFTONE |
		 SS_Q_BIT_ORDERING,
		 320, 0, 0, 0, { 2, 0x0a }, 0, 7,
		 RICOH_IS410,
		 ricoh_is410_sw, get_buffer_status, NULL
	 }},
	{{T_SCANNER, T_FIXED,	       /* Ricoh IS-410 OEMed by IBM */
	 "IBM     ", "2456-001        ", "    "}, {
		 "IBM 2456",
		 SS_Q_WINDOW_DESC_LEN |
		 SS_Q_THRESHOLD |
		 SS_Q_HALFTONE |
		 SS_Q_BIT_ORDERING,
		 320, 0, 0, 0, { 2, 0x0a }, 0, 7,
		 RICOH_IS410,
		 ricoh_is410_sw, get_buffer_status, NULL
	 }},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "UC630           ", "    "}, {
		 "UMAX UC-630",
		 SS_Q_WINDOW_DESC_LEN |
		 SS_Q_HALFTONE,
		 0x2e, 0, 0, 0, { 0, 1 }, 0, 0,
		 UMAX_UC630,
		 umax_uc630_sw, NULL, NULL
	 }},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "UG630           ", "    "}, {
		 "UMAX UG-630",
		 SS_Q_WINDOW_DESC_LEN |
		 SS_Q_HALFTONE,
		 0x2e, 0, 0, 0, { 0, 1 }, 0, 0,
		 UMAX_UG630,
		 umax_uc630_sw, NULL, NULL
	 }},
#ifdef NOTYET			/* ADF version */
	{{T_SCANNER, T_FIXED,
	 "FUJITSU ", "M3096Gm         ", "    "}, {
		 "Fujitsu M3096G",
		 SS_Q_WINDOW_DESC_LEN |
		 SS_Q_BRIGHTNESS |
		 SS_Q_MONO_THRESHOLD |
		 SS_Q_HALFTONE |
		 SS_Q_SET_RIF |
		 SS_Q_PADDING_TYPE,
		 64, 0, 0, 0, { 0, 1 }, 0, 0,
		 FUJITSU_M3096G,
		 fujistsu_m3096g_sw, NULL, NULL
	 }},
#else				/* flatbed-only version */
	{{T_SCANNER, T_FIXED,
	 "FUJITSU ", "M3096Gm         ", "    "}, {
		 "Fujitsu M3096G",
		 SS_Q_BRIGHTNESS |
		 SS_Q_MONO_THRESHOLD |
		 SS_Q_CONTRAST |
		 SS_Q_HALFTONE |
		 SS_Q_PADDING_TYPE,
		 0, 0, 0, 0, { 0, 1 }, 0, 0,
		 FUJITSU_M3096G,
		 NULL, NULL, NULL
	 }},
#endif
};


int ssmatch(struct device *, void *, void *);
void ssattach(struct device *, struct device *, void *);

struct cfattach ss_ca = {
	sizeof(struct ss_softc), ssmatch, ssattach
};

struct cfdriver ss_cd = {
	NULL, "ss", DV_DULL
};

struct scsi_device ss_switch = {
	NULL,
	ssstart,
	NULL,
	NULL,
};

const struct scsi_inquiry_pattern ss_patterns[] = {
	{T_SCANNER, T_FIXED,
	 "",         "",                 ""},
	{T_SCANNER, T_REMOV,
	 "",         "",                 ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C1750A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C1790A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C2500A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C2570A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C2520A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C1130A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C5110A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C6290A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C5190A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C7190A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C6270A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C7670A          ", ""},
};

int
ssmatch(parent, match, aux)
	struct device *parent;
	void *match, *aux;
{
	struct scsi_attach_args *sa = aux;
	int priority;

	(void)scsi_inqmatch(sa->sa_inqbuf,
	    ss_patterns, sizeof(ss_patterns)/sizeof(ss_patterns[0]),
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
	struct scsi_attach_args *sa = aux;
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB2, ("ssattach:\n"));

	/*
	 * Store information needed to contact our base driver
	 */
	ss->sc_link = sc_link;
	sc_link->device = &ss_switch;
	sc_link->device_softc = ss;
	sc_link->openings = 1;

	if (!bcmp(sa->sa_inqbuf->vendor, "MUSTEK", 6))
		mustek_attach(ss, sa);
	else if (!bcmp(sa->sa_inqbuf->vendor, "HP      ", 8))
		scanjet_attach(ss, sa);
	else
		ss_identify_scanner(ss, sa->sa_inqbuf);

	/*
	 * populate the scanio struct with legal values
	 */
	ss->sio.scan_width		= 1200;
	ss->sio.scan_height		= 1200;
	ss->sio.scan_x_resolution	= 100;
	ss->sio.scan_y_resolution	= 100;
	ss->sio.scan_x_origin		= 0;
	ss->sio.scan_y_origin		= 0;
	ss->sio.scan_brightness		= 128;
	ss->sio.scan_contrast		= 128;
	ss->sio.scan_quality		= 100;
	ss->sio.scan_image_mode		= SIM_GRAYSCALE;

	/* XXX fill in the rest of the scan_io struct by calling the
	   compute_sizes routine */

	mtx_init(&ss->sc_start_mtx, IPL_BIO);

	timeout_set(&ss->timeout, ssstart, ss);

	/* Set up the buf queue for this device. */
	ss->sc_bufq = bufq_init(BUFQ_DEFAULT);
}

void
ss_identify_scanner(ss, inqbuf)
	struct ss_softc *ss;
	struct scsi_inquiry_data *inqbuf;
{
	const struct ss_quirk_inquiry_pattern *finger;
	int priority;
	/*
	 * look for non-standard scanners with help of the quirk table
	 * and install functions for special handling
	 */
	finger = (const struct ss_quirk_inquiry_pattern *)scsi_inqmatch(inqbuf,
	    ss_quirk_patterns,
	    sizeof(ss_quirk_patterns)/sizeof(ss_quirk_patterns[0]),
	    sizeof(ss_quirk_patterns[0]), &priority);
	if (priority != 0) {
		ss->quirkdata = &finger->quirkdata;
		if (ss->quirkdata->special_minphys != NULL) {
			ss->special.minphys = ss->quirkdata->special_minphys;
		}
		ss->sio.scan_scanner_type = ss->quirkdata->scanner_type;
		printf("\n%s: %s\n", ss->sc_dev.dv_xname, ss->quirkdata->name);
	} else {
		printf("\n%s: generic scanner\n", ss->sc_dev.dv_xname);
		bzero(&ss_gen_quirks, sizeof(ss_gen_quirks));
		ss->quirkdata = &ss_gen_quirks;
		ss->sio.scan_scanner_type = GENERIC_SCSI2;
	}
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
	if (unit >= ss_cd.cd_ndevs)
		return (ENXIO);
	ss = ss_cd.cd_devs[unit];
	if (!ss)
		return (ENXIO);

	ssmode = SSMODE(dev);
	sc_link = ss->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("open: dev=0x%x (unit %d (of %d))\n", dev,
	    unit, ss_cd.cd_ndevs));

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
	error = scsi_test_unit_ready(sc_link, TEST_READY_RETRIES,
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
 * occurrence of an open device
 */
int
ssclose(dev, flag, mode, p)
	dev_t dev;
	int flag;
	int mode;
	struct proc *p;
{
	struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(dev)];
	int error;

	SC_DEBUG(ss->sc_link, SDEV_DB1, ("closing\n"));

	if (SSMODE(dev) == MODE_REWIND) {
		if (ss->special.rewind_scanner) {
			/* call special handler to rewind/abort scan */
			error = (ss->special.rewind_scanner)(ss);
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
	struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(bp->b_dev)];

	(ss->sc_link->adapter->scsi_minphys)(bp, ss->sc_link);

	/*
	 * trim the transfer further for special devices this is
	 * because some scanners only read multiples of a line at a
	 * time, also some cannot disconnect, so the read must be
	 * short enough to happen quickly
	 */
	if (ss->special.minphys)
		(ss->special.minphys)(ss, bp);
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
	struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(dev)];
	int error;

	/* if the scanner has not yet been started, do it now */
	if (!(ss->flags & SSF_TRIGGERED)) {
		if (ss->special.trigger_scanner) {
			error = (ss->special.trigger_scanner)(ss);
			if (error)
				return (error);
		} else {
			struct scsi_start_stop trigger_cmd;
			bzero(&trigger_cmd, sizeof(trigger_cmd));
			trigger_cmd.opcode = START_STOP;
			trigger_cmd.how = SSS_START;
			scsi_scsi_cmd(ss->sc_link,
				(struct scsi_generic *)&trigger_cmd,
				sizeof(trigger_cmd), 0, 0, SCSI_RETRIES, 5000,
				NULL, 0);
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
	struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(bp->b_dev)];
	int s;

	SC_DEBUG(ss->sc_link, SDEV_DB2, ("ssstrategy: %ld bytes @ blk %d\n",
	    bp->b_bcount, bp->b_blkno));

	if (bp->b_bcount > ss->sio.scan_window_size)
		bp->b_bcount = ss->sio.scan_window_size;

	/*
	 * If it's a null transfer, return immediately
	 */
	if (bp->b_bcount == 0)
		goto done;

	/*
	 * Place it in the queue of activities for this scanner
	 * at the end (a bit silly because we only have on user..)
	 * (but it could fork() or dup())
	 */
	BUFQ_QUEUE(ss->sc_bufq, bp);

	/*
	 * Tell the device to get going on the transfer if it's
	 * not doing anything, otherwise just wait for completion
	 * (All a bit silly if we're only allowing 1 open but..)
	 */
	ssstart(ss);

	return;

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
	struct scsi_xfer *xs;
	struct buf *bp;
	struct scsi_r_scanner *cdb;

	SC_DEBUG(sc_link, SDEV_DB2, ("ssstart\n"));

	mtx_enter(&ss->sc_start_mtx);
	ss->sc_start_count++;
	if (ss->sc_start_count > 1) {
		mtx_leave(&ss->sc_start_mtx);
		return;
	}
	mtx_leave(&ss->sc_start_mtx);
	CLR(ss->flags, SSF_WAITING);
restart:
	while (!ISSET(ss->flags, SSF_WAITING) &&
	    (bp = BUFQ_DEQUEUE(ss->sc_bufq)) != NULL) {
		xs = scsi_xs_get(sc_link, SCSI_NOSLEEP);
		if (xs == NULL)
			break;

		if (ss->special.read) {
			(ss->special.read)(ss, xs, bp);
		} else {
			cdb = (struct scsi_r_scanner *)xs->cmd;
			xs->cmdlen = sizeof(*cdb);

			cdb->opcode = READ_BIG;
			_lto3b(bp->b_bcount, cdb->len);

			xs->data = bp->b_data;
			xs->datalen = bp->b_bcount;
			xs->flags |= SCSI_DATA_IN;
			xs->retries = 0;
			xs->timeout = 100000;
			xs->done = ssdone;
			xs->cookie = bp;

			scsi_xs_exec(xs);
		}
	}
	mtx_enter(&ss->sc_start_mtx);
	ss->sc_start_count--;
	if (ss->sc_start_count != 0) {
		ss->sc_start_count = 1;
		mtx_leave(&ss->sc_start_mtx);
		goto restart;
	}
	mtx_leave(&ss->sc_start_mtx);
}

void
ssdone(struct scsi_xfer *xs)
{
	struct ss_softc *ss = xs->sc_link->device_softc;
	struct buf *bp = xs->cookie;
	int error, s;

	switch (xs->error) {
	case XS_NOERROR:
		bp->b_error = 0;
		bp->b_resid = xs->resid;
		break;

	case XS_NO_CCB:
		/* The adapter is busy, requeue the buf and try it later. */
		BUFQ_REQUEUE(ss->sc_bufq, bp);
		scsi_xs_put(xs);
		SET(ss->flags, SSF_WAITING); /* break out of cdstart loop */
		timeout_add(&ss->timeout, 1);
		return;

	case XS_SENSE:
	case XS_SHORTSENSE:
		error = scsi_interpret_sense(xs);
		if (error == 0) {
			bp->b_error = 0;
			bp->b_resid = xs->resid;
			break;
		}
		if (error != ERESTART)
			xs->retries = 0;
		goto retry;

	case XS_BUSY:
		if (xs->retries) {
			if (scsi_delay(xs, 1) != ERESTART)
				xs->retries = 0;
		}
		goto retry;

	case XS_TIMEOUT:
retry:
		if (xs->retries--) {
			scsi_xs_exec(xs);
			return;
		}
		/* FALLTHROUGH */

	default:
		bp->b_error = EIO;
		bp->b_flags |= B_ERROR;
		bp->b_resid = bp->b_bcount;
		break;
	}

	s = splbio();
	biodone(bp);
	splx(s);
	scsi_xs_put(xs);
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
	struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(dev)];
	int error = 0;
	struct scan_io *sio;

	switch (cmd) {
	case SCIOCGET:
		/* call special handler, if any */
		if (ss->special.get_params) {
			error = (ss->special.get_params)(ss);
			if (error)
				return (error);
		}
		bcopy(&ss->sio, addr, sizeof(struct scan_io));
		break;
	case SCIOCSET:
		sio = (struct scan_io *)addr;

		/* call special handler, if any */
		if (ss->special.set_params) {
			error = (ss->special.set_params)(ss, sio);
			if (error)
				return (error);
		} else {
			/* XXX add routine to validate parameters */
			ss_set_window(ss, sio);
		}
		break;
	case SCIOCRESTART:
		/* call special handler, if any */
		if (ss->special.rewind_scanner ) {
			error = (ss->special.rewind_scanner)(ss);
			if (error)
				return (error);
		} else
			/* XXX add code for SCSI2 scanner, if any */
			return (EOPNOTSUPP);
		ss->flags &= ~SSF_TRIGGERED;
		break;
	case SCIOC_USE_ADF:
		/* XXX add Automatic Document Feeder Support */
		return (EOPNOTSUPP);
	default:
		if (SSMODE(dev) != MODE_CONTROL)
			return (ENOTTY);
		return (scsi_do_ioctl(ss->sc_link, cmd, addr, flag));
	}
	return (error);
}

int
ss_set_window(ss, sio)
	struct ss_softc *ss;
	struct scan_io *sio;
{
	struct scsi_set_window	window_cmd;
	struct {
		struct scsi_window_data	window_data;
		/* vendor_unique must provide enough space for worst case
		 * (currently Ricoh IS-410.)  40 + 280 = 320 which is the size
		 * of its window descriptor length
		 */
		u_int8_t vendor_unique[280];
	} wd;
#define window_data   wd.window_data
#define vendor_unique wd.vendor_unique
	struct scsi_link	*sc_link = ss->sc_link;

	/*
	 * The CDB for SET WINDOW goes in here.
	 * The two structures that follow are sent via data out.
	 */
	bzero(&window_cmd, sizeof(window_cmd));
	window_cmd.opcode = SET_WINDOW;
	_lto3l(sizeof(window_data), window_cmd.len);

	bzero(&window_data, sizeof(window_data));
	if (ss->quirkdata->quirks & SS_Q_WINDOW_DESC_LEN)
		_lto2l(ss->quirkdata->window_descriptor_length,
		    window_data.window_desc_len);
	else
		_lto2l(40L, window_data.window_desc_len);

	/* start of SET_WINDOW parameter block */

	/* leave window id at zero */
	/* leave auto bit at zero */
	_lto2l(sio->scan_x_resolution, window_data.x_res);
	_lto2l(sio->scan_y_resolution, window_data.y_res);
	_lto4l(sio->scan_x_origin, window_data.x_org);
	_lto4l(sio->scan_y_origin, window_data.y_org);
	_lto4l(sio->scan_width,  window_data.width);
	_lto4l(sio->scan_height, window_data.length);

	if (ss->quirkdata->quirks & SS_Q_REV_BRIGHTNESS)
		window_data.brightness = 256 - sio->scan_brightness;
	else if (ss->quirkdata->quirks & SS_Q_BRIGHTNESS)
		window_data.brightness = ss->quirkdata->brightness;
	else
		window_data.brightness = sio->scan_brightness;

	/*
	 * threshold: Default is to follow brightness.
	 * If SS_Q_MONO_THRESHOLD is set then the quirkdata contains a special
	 * value to be used instead of default when image data is monochrome.
	 * Otherwise if SS_Q_THRESHOLD is set then the quirkdata contains
	 * the threshold to always use.
	 * Both SS_Q_MONO_THRESHOLD and SS_Q_THRESHOLD should not be set at
	 * the same time.
	 */
	if (ss->quirkdata->quirks & SS_Q_MONO_THRESHOLD) {
		if (sio->scan_image_mode == SIM_BINARY_MONOCHROME ||
		    sio->scan_image_mode == SIM_DITHERED_MONOCHROME)
			window_data.threshold = ss->quirkdata->threshold;
		else
			window_data.threshold = sio->scan_brightness;
	} else if (ss->quirkdata->quirks & SS_Q_THRESHOLD)
		window_data.threshold = ss->quirkdata->threshold;
	else
		window_data.threshold = sio->scan_brightness;

	if (ss->quirkdata->quirks & SS_Q_REV_CONTRAST)
		window_data.contrast = 256 - sio->scan_contrast;
	else if (ss->quirkdata->quirks & SS_Q_CONTRAST)
		window_data.contrast = ss->quirkdata->contrast;
	else
		window_data.contrast = sio->scan_contrast;

	switch (sio->scan_image_mode) {
	case SIM_RED:
	case SIM_GREEN:
	case SIM_BLUE:
		window_data.image_comp = SIM_GRAYSCALE;
		break;
	default:
		window_data.image_comp = sio->scan_image_mode;
	}

	window_data.bits_per_pixel = sio->scan_bits_per_pixel;

	if (ss->quirkdata->quirks & SS_Q_HALFTONE) {
		window_data.halftone_pattern[0] =
			ss->quirkdata->halftone_pattern[0];
		window_data.halftone_pattern[1] = 
			ss->quirkdata->halftone_pattern[1];
	} /* else leave halftone set to zero. */

	if (ss->quirkdata->quirks & SS_Q_SET_RIF)
		window_data.rif = 1;

	if (ss->quirkdata->quirks & SS_Q_PADDING_TYPE)
		window_data.pad_type = ss->quirkdata->pad_type;
	else
		window_data.pad_type = 3; /* 3 = truncate to byte boundary */

	if (ss->quirkdata->quirks & SS_Q_BIT_ORDERING)
		_lto2l(ss->quirkdata->bit_ordering, window_data.bit_ordering);
	/* else leave bit_ordering set to zero. */

	/* leave compression type & argument set to zero. */

#undef window_data

	if (ss->quirkdata->vendor_unique_sw != NULL)
		return ((*ss->quirkdata->vendor_unique_sw)(ss, sio,
		    &window_cmd, (void *)&wd));
	else
		/* send the command to the scanner */
		return (scsi_scsi_cmd(sc_link,
		    (struct scsi_generic *)&window_cmd,
		    sizeof(window_cmd), (u_char *) &wd.window_data,
		    (ss->quirkdata->quirks & SS_Q_WINDOW_DESC_LEN) ?
		    ss->quirkdata->window_descriptor_length : 40,
		    SCSI_RETRIES, 5000, NULL, SCSI_DATA_OUT));
}

int
ricoh_is410_sw(ss, sio, wcmd, vwd)
	struct ss_softc *ss;
	struct scan_io *sio;
	struct scsi_set_window *wcmd;
	void *vwd;
{
	struct ricoh_is410_window_data {
		struct scsi_window_data	window_data;
		u_int8_t res1;
		u_int8_t res2;
		u_int    mrif:1; /* reverse image format (grayscale negative) */
		u_int    filtering:3;
		u_int    gamma_id:4;
	} *rwd = (struct ricoh_is410_window_data*)vwd;
	struct scsi_link *sc_link = ss->sc_link;

	rwd->mrif = 1;		/* force grayscale to match PGM */

	/* send the command to the scanner */
	return (scsi_scsi_cmd(sc_link, (struct scsi_generic *)wcmd,
	    sizeof(struct scsi_set_window), (u_char *)rwd,
	    sizeof(struct ricoh_is410_window_data), SCSI_RETRIES, 5000, NULL,
	    SCSI_DATA_OUT));
}

int
umax_uc630_sw(ss, sio, wcmd, vwd)
	struct ss_softc *ss;
	struct scan_io *sio;
	struct scsi_set_window *wcmd;
	void *vwd;
{
	struct umax_uc630_window_data {
		struct scsi_window_data	window_data;
		u_int8_t speed;
		u_int8_t select_color;
		u_int8_t highlight;
		u_int8_t shadow;
		u_int8_t paper_length[2];
	} *uwd = (struct umax_uc630_window_data*)vwd;
	struct scsi_link *sc_link = ss->sc_link;

	uwd->speed = 1;		/* speed: fastest speed that doesn't smear */
	switch (sio->scan_image_mode) {	/* UMAX has three-pass color. */
	case SIM_RED:			/* This selects which filter to use. */
		uwd->select_color = 0x80;
		break;
	case SIM_GREEN:
		uwd->select_color = 0x40;
		break;
	case SIM_BLUE:
		uwd->select_color = 0x20;
		break;
	}
	uwd->highlight = 50;		/* 50 = highest; 0 = lowest */
	/* leave shadow set to zero. */
	/* XXX paper length is for ADF */

	/* send the command to the scanner */
	return (scsi_scsi_cmd(sc_link, (struct scsi_generic *)wcmd,
	    sizeof(struct scsi_set_window), (u_char *)uwd,
	    sizeof(struct umax_uc630_window_data), SCSI_RETRIES, 5000, NULL,
	    SCSI_DATA_OUT));
}

#ifdef NOTYET /* for ADF support */
int
fujitsu_m3096g_sw(ss, sio, wcmd, vwd)
	struct ss_softc *ss;
	struct scan_io *sio;
	struct scsi_set_window *wcmd;
	void *vwd;
{
	struct fujitsu_m3096g_window_data {
		struct scsi_window_data	window_data;
		u_int8_t id;
		u_int8_t res1;
		u_int8_t outline;
		u_int8_t emphasis;
		u_int8_t mixed;
		u_int8_t mirroring;
		u_int8_t res2[5];
		u_int8_t subwindow_list[2];
		u_int    paper_size_std:2;
		u_int    res3:1;
		u_int    paper_orientaton:1;
		u_int    paper_size_type:4;
/* defines for Paper Size Type: */
#define FUJITSU_PST_A3			0x03
#define FUJITSU_PST_A4			0x04
#define FUJITSU_PST_A5			0x05
#define FUJITSU_PST_DOUBLE_LETTER	0x06
#define FUJITSU_PST_LETTER		0x07
#define FUJITSU_PST_B4			0x0C
#define FUJITSU_PST_B5			0x0D
#define FUJITSU_PST_LEGAL		0x0F
		u_int8_t paper_width_x[4];
		u_int8_t paper_width_y[4];
		u_int8_t res4[2];
	} *fwd = (struct fujitsu_m3096g_window_data*)vwd;
	struct scsi_link *sc_link = ss->sc_link;

	/* send the command to the scanner */
	return (scsi_scsi_cmd(sc_link, (struct scsi_generic *)wcmd,
	    sizeof(struct scsi_set_window), (u_char *)fwd,
	    sizeof(struct fujitsu_m3096g_window_data), SCSI_RETRIES, 5000, NULL,
	    SCSI_DATA_OUT));
}
#endif

void
get_buffer_status(ss, bp)
	struct ss_softc *ss;
	struct buf *bp;
{
	struct scsi_get_buffer_status gbs_cmd;
	struct scsi_link *sc_link = ss->sc_link;
	struct {
		u_int8_t stat_len[3];
		u_int8_t res1;
		u_int8_t window_id;
		u_int8_t res2;
		u_int8_t tgt_accept_buf_len[3];
		u_int8_t tgt_send_buf_len[3];
	} buf_sz_retn;
	int flags;

	bzero(&gbs_cmd, sizeof(gbs_cmd));
	gbs_cmd.opcode = GET_BUFFER_STATUS;
	_lto2b(12, gbs_cmd.len);
	flags = SCSI_DATA_IN;

	if (scsi_scsi_cmd(sc_link, (struct scsi_generic *) &gbs_cmd,
	    sizeof(gbs_cmd), (u_char *) &buf_sz_retn, sizeof(buf_sz_retn),
	    0, 100000, bp, flags | SCSI_NOSLEEP)) {
		printf("%s: not queued\n", ss->sc_dev.dv_xname);
	}
	bp->b_bcount = MIN(_3btol(buf_sz_retn.tgt_send_buf_len), bp->b_bcount);
}

#ifdef NOTYET
int
umax_compute_sizes(ss)
	struct ss_softc *ss;
{
	ss->sio.scan_lines = ;
	ss->sio.scan_window_size = ;
}

int
calc_umax_row_len(dpi, ww)
	int dpi;
	int ww;
{
	int st[301];
	int i;
	int rowB = 0;

	for (i = 1; i <= 300; i++)
		st[i] = 1;

	for (i = 1; i <= 300 - dpi; i++)
		st[i * 300 / (300 - dpi)] = 0;

	for (i = 1; i <= (ww % 1200) / 4; i++) {
		if (st[i])
			rowB++;
	}

	return ((ww / 1200) * dpi + rowB);
}
#endif
