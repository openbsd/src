/*	$OpenBSD: ss.c,v 1.16 1997/03/08 20:43:09 kstailey Exp $	*/
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
#define SS_Q_NEEDS_WINDOW_DESC_LEN	0x0001 /* needs special WDL */
#define SS_Q_USES_HALFTONE		0x0002 /* uses non-zero halftone */
#define SS_Q_NEEDS_RIF_SET		0x0004
#define SS_Q_NEEDS_PADDING_TYPE		0x0008 /* does not pad to byte
						  boundary */
#define SS_Q_USES_BIT_ORDERING		0x0010 /* uses non-zero bit ordering */
#define SS_Q_VENDOR_UNIQUE_SETWINDOW	0x0020 /* 40 bytes of parms is not
						  enough */
#define SS_Q_GET_BUFFER_SIZE		0x0040 /* use GET_BUFFER_SIZE while
						  reading */

	long window_descriptor_length;
	u_int8_t halftone_pattern[2];
	int pad_type;
	long bit_ordering;
	int	(*vendor_unique_sw)__P((struct ss_softc *, struct scan_io *,
					struct scsi_set_window *, void *));
};

struct ss_quirk_inquiry_pattern {
	struct scsi_inquiry_pattern pattern;
	struct quirkdata quirkdata;
};

void    ssstrategy __P((struct buf *));
void    ssstart __P((void *));
void	ssminphys __P((struct buf *));

void	ss_identify_scanner __P((struct ss_softc *, struct scsi_inquiry_data*));
int	ss_set_window __P((struct ss_softc *, struct scan_io *));

int	ricoh_is410_sw __P((struct ss_softc *, struct scan_io *,
			    struct scsi_set_window *, void *));
int	umax_uc630_sw __P((struct ss_softc *, struct scan_io *,
			   struct scsi_set_window *, void *));
#ifdef NOTYET	/* for ADF support  */
int	fujitsu_m3096g_sw __P((struct ss_softc *, struct scan_io *,
			       struct scsi_set_window *, void *));
#endif


/*
 * WDL:
 *
 *  Ricoh IS-50 & IS-410 insist on 320 (even it transfer len is less.)
 *  Ricoh FS-1 accepts 256 (I haven't tested other values.)
 *  UMAX UC-630 accepts 46 (I haven't tested other values.)
 *  Fujitsu M3096G wants 40 <= x <= 248 (tested OK at 40 & 64.)
 */

struct ss_quirk_inquiry_pattern ss_quirk_patterns[] = {
	{{T_SCANNER, T_FIXED,
	 "RICOH   ", "IS410           ", "    "}, {
		 "Ricoh IS-410",
		 SS_Q_NEEDS_WINDOW_DESC_LEN |
		 SS_Q_USES_HALFTONE |
		 SS_Q_USES_BIT_ORDERING |
		 SS_Q_VENDOR_UNIQUE_SETWINDOW,
		 320, { 2, 0x0a }, 0, 7,
		 ricoh_is410_sw
	 }},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "UC630           ", "    "}, {
		 "UMAX UC-630",
		 SS_Q_NEEDS_WINDOW_DESC_LEN |
		 SS_Q_USES_HALFTONE |
		 SS_Q_VENDOR_UNIQUE_SETWINDOW,
		 0x2e, { 0, 1 }, 0, 0,
		 umax_uc630_sw
	 }},
	{{T_SCANNER, T_FIXED,
	 "UMAX    ", "UG630           ", "    "}, {
		 "UMAX UG-630",
		 SS_Q_NEEDS_WINDOW_DESC_LEN |
		 SS_Q_USES_HALFTONE |
		 SS_Q_VENDOR_UNIQUE_SETWINDOW,
		 0x2e, { 0, 1 }, 0, 0,
		 umax_uc630_sw
	 }},
#ifdef NOTYET			/* ADF version */
	{{T_SCANNER, T_FIXED,
	 "FUJITSU ", "M3096Gm         ", "    "}, {
		 "Fujitsu M3096G",
		 SS_Q_NEEDS_WINDOW_DESC_LEN |
		 SS_Q_USES_HALFTONE |
		 SS_Q_NEEDS_RIF_SET |
		 SS_Q_NEEDS_PADDING_TYPE,
		 64, { 0, 1 }, 0, 0,
		 fujistsu_m3096g_sw
	 }},
#else				/* flatbed-only version */
	{{T_SCANNER, T_FIXED,
	 "FUJITSU ", "M3096Gm         ", "    "}, {
		 "Fujitsu M3096G",
		 SS_Q_USES_HALFTONE |
		 SS_Q_NEEDS_PADDING_TYPE,
		 0, { 0, 1 }, 0, 0,
		 NULL
	 }},
#endif
};
       

int ssmatch __P((struct device *, void *, void *));
void ssattach __P((struct device *, struct device *, void *));

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

struct scsi_inquiry_pattern ss_patterns[] = {
	{T_SCANNER, T_FIXED,
	 "",         "",                 ""},
	{T_SCANNER, T_REMOV,
	 "",         "",                 ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C1750A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C2500A          ", ""},
	{T_PROCESSOR, T_FIXED,
	 "HP      ", "C1130A          ", ""},
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

	if (!bcmp(sa->sa_inqbuf->vendor, "MUSTEK", 6))
		mustek_attach(ss, sa);
	else if (!bcmp(sa->sa_inqbuf->vendor, "HP      ", 8))
		scanjet_attach(ss, sa);
	else
		ss_identify_scanner(ss, sa->sa_inqbuf);

	/*
	 * Set up the buf queue for this device
	 */
	ss->buf_queue.b_active = 0;
	ss->buf_queue.b_actf = 0;
	ss->buf_queue.b_actb = &ss->buf_queue.b_actf;
}

void
ss_identify_scanner(ss, inqbuf)
	struct ss_softc *ss;
	struct scsi_inquiry_data *inqbuf;
{
	struct ss_quirk_inquiry_pattern *finger;
	int priority;
	/*
	 * look for non-standard scanners with help of the quirk table
	 * and install functions for special handling
	 */
	finger = (struct ss_quirk_inquiry_pattern *)scsi_inqmatch(inqbuf,
	    (caddr_t)ss_quirk_patterns,
	    sizeof(ss_quirk_patterns)/sizeof(ss_quirk_patterns[0]),
	    sizeof(ss_quirk_patterns[0]), &priority);
	if (priority != 0) {
		ss->quirkdata = &finger->quirkdata;
		printf("%s\n", ss->quirkdata->name);
	} else {
		printf("generic scanner\n"); /* good luck 8c{)] */
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
	register struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(bp->b_dev)];

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
	struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(dev)];
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
	struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(bp->b_dev)];
	struct buf *dp;
	int s;

	SC_DEBUG(ss->sc_link, SDEV_DB1,
	    ("ssstrategy %ld bytes @ blk %d\n", bp->b_bcount, bp->b_blkno));

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
	struct ss_softc *ss = ss_cd.cd_devs[SSUNIT(dev)];
	int error = 0;
	struct scan_io *sio;

	switch (cmd) {
	case SCIOCGET:
		/* call special handler, if any */
		if (ss->special->get_params) {
			error = (ss->special->get_params)(ss);
			if (error)
				return (error);
		}
		bcopy(&ss->sio, addr, sizeof(struct scan_io));
		break;
	case SCIOCSET:
		sio = (struct scan_io *)addr;

		/* call special handler, if any */
		if (ss->special->set_params) {
			error = (ss->special->set_params)(ss, sio);
			if (error)
				return (error);
		} else {
			/* add routine to validate paramters */
			ss_set_window(ss, sio);
		}
		break;
	case SCIOCRESTART:
		/* call special handler, if any */
		if (ss->special->rewind_scanner ) {
			error = (ss->special->rewind_scanner)(ss);
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
		return (scsi_do_safeioctl(ss->sc_link, dev, cmd, addr,
		    flag, p));
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
	struct scsi_link	*sc_link = ss->sc_link;;

	/*
	 * The CDB for SET WINDOW goes in here.
	 * The two structures that follow are sent via data out.
	 */
	bzero(&window_cmd, sizeof(window_cmd));
	window_cmd.opcode = SET_WINDOW;
	_lto3l(sizeof(window_data), window_cmd.len);

	bzero(&window_data, sizeof(window_data));
	if (ss->quirkdata->quirks & SS_Q_NEEDS_WINDOW_DESC_LEN)
		_lto2l(ss->quirkdata->window_descriptor_length,
		       window_data.window_desc_len);
	else
		_lto2l(40L, window_data.window_desc_len);
	/* leave window id at zero */
	/* leave auto bit at zero */
	_lto2l(sio->scan_x_resolution, window_data.x_res);
	_lto2l(sio->scan_y_resolution, window_data.y_res);
	_lto4l(sio->scan_x_origin, window_data.x_org);
	_lto4l(sio->scan_y_origin, window_data.y_org);
	_lto4l(sio->scan_width,  window_data.width);
	_lto4l(sio->scan_height, window_data.length);
	window_data.brightness     = sio->scan_brightness;
	window_data.threshold      = sio->scan_brightness;
	window_data.contrast       = sio->scan_contrast;
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
	if (ss->quirkdata->quirks & SS_Q_USES_HALFTONE) {
		window_data.halftone_pattern[0] =
			ss->quirkdata->halftone_pattern[0];
		window_data.halftone_pattern[1] = 
			ss->quirkdata->halftone_pattern[1];
	} /* else leave halftone set to zero. */
	/* leave rif set to zero. */
	if (ss->quirkdata->quirks & SS_Q_NEEDS_PADDING_TYPE)
		window_data.pad_type = ss->quirkdata->pad_type;
	else
		window_data.pad_type = 3; /* 3 = pad to byte boundary */
	if (ss->quirkdata->quirks & SS_Q_USES_BIT_ORDERING)
		_lto2l(ss->quirkdata->bit_ordering, window_data.bit_ordering);
	/* else leave bit_ordering set to zero. */
	/* leave compression type & argument set to zero. */

#undef window_data

	if (ss->quirkdata->quirks &SS_Q_VENDOR_UNIQUE_SETWINDOW)
		return ((*ss->quirkdata->vendor_unique_sw)(ss, sio,
		    &window_cmd, (void*)&wd));
	else
		/* send the command to the scanner */
		return (scsi_scsi_cmd(sc_link,
		        (struct scsi_generic *)&window_cmd,
			sizeof(window_cmd), (u_char *) &wd.window_data,
			sizeof(wd.window_data), 4, 5000, NULL, SCSI_DATA_OUT));
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
		u_int8_t mrif:1; /* reverse image format (grayscale negative) */
		u_int8_t filtering:3;
		u_int8_t gamma_id:4;
	} *rwd = (struct ricoh_is410_window_data*)vwd;
	struct scsi_link *sc_link = ss->sc_link;

	rwd->mrif = 1;		/* force grayscale to match PGM */

	/* send the command to the scanner */
	return (scsi_scsi_cmd(sc_link, (struct scsi_generic *)wcmd,
	    sizeof(struct scsi_set_window), (u_char *)rwd,
	    sizeof(struct ricoh_is410_window_data), 4, 5000, NULL,
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
	    sizeof(struct umax_uc630_window_data), 4, 5000, NULL,
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
		u_int8_t paper_size_std:2;
		u_int8_t res3:1;
		u_int8_t paper_orientaton:1;
		u_int8_t paper_size_type:4;
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
	    sizeof(struct fujitsu_m3096g_window_data), 4, 5000, NULL,
	    SCSI_DATA_OUT));
}
#endif
