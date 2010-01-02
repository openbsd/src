/*	$OpenBSD: ss_scanjet.c,v 1.34 2010/01/02 23:28:51 dlg Exp $	*/
/*	$NetBSD: ss_scanjet.c,v 1.6 1996/05/18 22:58:01 christos Exp $	*/

/*
 * Copyright (c) 1995 Kenneth Stailey.  All rights reserved.
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
 *      This product includes software developed by Kenneth Stailey.
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
 * special functions for the HP ScanJet IIc and IIcx
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/systm.h>
#include <sys/fcntl.h>
#include <sys/errno.h>
#include <sys/ioctl.h>
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

int scanjet_set_params(struct ss_softc *, struct scan_io *);
int scanjet_trigger_scanner(struct ss_softc *);
int scanjet_read(struct ss_softc *, struct scsi_xfer *, struct buf *);
void scanjet_read_done(struct scsi_xfer *);

/* only used internally */
int scanjet_ctl_write(struct ss_softc *, char *, u_int, int);
int scanjet_ctl_read(struct ss_softc *, char *, u_int, int);
int scanjet_set_window(struct ss_softc *, int);
int scanjet_compute_sizes(struct ss_softc *, int);
/* Maybe move to libkern? */
#define atoi local_atoi
__inline static int atoi(const char *);


/*
 * structure for the special handlers
 */
struct ss_special scanjet_special = {
	scanjet_set_params,
	scanjet_trigger_scanner,
	NULL,
	NULL,			/* no special minphys */
	scanjet_read,		/* scsi 6-byte read */
	NULL,			/* no "rewind" code (yet?) */
	NULL,			/* no adf support right now */
	NULL			/* no adf support right now */
};

/*
 * scanjet_attach: attach special functions to ss
 */
void
scanjet_attach(ss, sa)
	struct ss_softc *ss;
	struct scsi_attach_args *sa;
{
#ifdef SCSIDEBUG
	struct scsi_link *sc_link = sa->sa_sc_link;
#endif
	int error;

	SC_DEBUG(sc_link, SDEV_DB1, ("scanjet_attach: start\n"));
	ss->sio.scan_scanner_type = 0;

	printf("\n%s: ", ss->sc_dev.dv_xname);

	/* first, check the model (which determines nothing yet) */

	if (!bcmp(sa->sa_inqbuf->product, "C1750A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet IIc");
	}
	/* The IIp is a grayscale-only HP SCL scanner */
	if (!bcmp(sa->sa_inqbuf->product, "C1790A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet IIp");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C2500A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet IIcx");
	}
	/* The 3p is a grayscale-only HP SCL scanner */
	if (!bcmp(sa->sa_inqbuf->product, "C2570A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 3p");
	}
	/* The 3c/4c/6100C report as the same? */
	if (!bcmp(sa->sa_inqbuf->product, "C2520A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 3c/4c/6100C");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C1130A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 4p");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C5110A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 5p");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C6290A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 4100C");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C5190A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 5100C");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C7190A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 5200C");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C6270A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 6200C");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C7670A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf("HP ScanJet 6300C");
	}

	SC_DEBUG(sc_link, SDEV_DB1, ("scanjet_attach: scanner_type = %d\n",
	    ss->sio.scan_scanner_type));

	/* now install special handlers */
	ss->special = scanjet_special;

	/*
	 * fill in the rest of the scan_io struct by calling
	 * set_window and compute_sizes routines
	 */
	error = scanjet_set_window(ss, SCSI_POLL);
	if (error) {
		printf(" set_window failed\n");
		return;
	}

	error = scanjet_compute_sizes(ss, SCSI_POLL);
	if (error) {
		printf(" compute_sizes failed\n");
		return;
	}

	printf("\n");
}

/*
 * check the parameters if the scanjet is capable of fulfilling it
 * but don't send the command to the scanner in case the user wants
 * to change parameters by more than one call
 */
int
scanjet_set_params(ss, sio)
	struct ss_softc *ss;
	struct scan_io *sio;
{
	int error;

#if 0
	/*
	 * if the scanner is triggered, then rewind it
	 */
	if (ss->flags & SSF_TRIGGERED) {
		error = scanjet_rewind_scanner(ss);
		if (error)
			return (error);
	}
#endif

	/* size constraints... */
	if (sio->scan_width == 0				 ||
	    sio->scan_x_origin + sio->scan_width > 10200 || /* 8.5" */
	    sio->scan_height == 0				 ||
	    sio->scan_y_origin + sio->scan_height > 16800)  /* 14" */
		return (EINVAL);

	/* resolution (dpi)... */
	if (sio->scan_x_resolution < 100 ||
	    sio->scan_x_resolution > 400 ||
	    sio->scan_y_resolution < 100 ||
	    sio->scan_y_resolution > 400)
		return (EINVAL);

	switch (sio->scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
	case SIM_DITHERED_MONOCHROME:
	case SIM_GRAYSCALE:
	case SIM_COLOR:
		break;
	default:
		return (EINVAL);
	}

	/* change ss_softc to the new values, but save ro-variables */
	sio->scan_scanner_type = ss->sio.scan_scanner_type;
	bcopy(sio, &ss->sio, sizeof(struct scan_io));

	error = scanjet_set_window(ss, 0);
	if (error) {
		uprintf("%s: set_window failed\n", ss->sc_dev.dv_xname);
		return (error);
	}
	error = scanjet_compute_sizes(ss, 0);
	if (error) {
		uprintf("%s: compute_sizes failed\n", ss->sc_dev.dv_xname);
		return (error);
	}

	return (0);
}

/*
 * trigger the scanner to start a scan operation
 * this includes sending the mode- and window-data,
 * and starting the scanner
 */
int
scanjet_trigger_scanner(ss)
	struct ss_softc *ss;
{
	static char *escape_codes = "\033*f0S";
	int error;

	error = scanjet_set_window(ss, 0);
	if (error) {
		uprintf("%s: set_window failed\n", ss->sc_dev.dv_xname);
		return (error);
	}
	error = scanjet_compute_sizes(ss, 0);
	if (error) {
		uprintf("%s: compute_sizes failed\n", ss->sc_dev.dv_xname);
		return (error);
	}

	/* send "trigger" operation */
	error = scanjet_ctl_write(ss, escape_codes, strlen(escape_codes), 0);
	if (error) {
		uprintf("%s: trigger_scanner failed\n", ss->sc_dev.dv_xname);
		return (error);
	}

	return (0);
}

int
scanjet_read(ss, xs, bp)
	struct ss_softc *ss;
	struct scsi_xfer *xs;
	struct buf *bp;
{
	struct scsi_rw_scanner *cdb;

	SC_DEBUG(sc_link, SDEV_DB1, ("scanjet_read: start\n"));

	cdb = (struct scsi_rw_scanner *)xs->cmd;
	xs->cmdlen = sizeof(*cdb);

	cdb->opcode = READ;
	_lto3b(bp->b_bcount, cdb->len);

	xs->data = bp->b_data;
	xs->datalen = bp->b_bcount;
	xs->flags |= SCSI_DATA_IN;
	xs->timeout = 100000;
	xs->done = scanjet_read_done;
	xs->cookie = bp;

	scsi_xs_exec(xs);

	return (0);
}

void
scanjet_read_done(struct scsi_xfer *xs)
{
	struct ss_softc *ss = xs->sc_link->device_softc;
	struct buf *bp = xs->cookie;

	splassert(IPL_BIO);

	switch (xs->error) {
	case XS_NOERROR:
		if (bp->b_bcount >= ss->sio.scan_window_size)
			ss->sio.scan_window_size = 0;
		else
			ss->sio.scan_window_size -= bp->b_bcount;

		bp->b_error = 0;
		bp->b_resid = xs->resid;
		break;

	case XS_NO_CCB:
		/* The adapter is busy, requeue the buf and try it later. */
                ss_buf_requeue(ss, bp);
                scsi_xs_put(xs);
		SET(ss->flags, SSF_WAITING); /* break out of cdstart loop */
		timeout_add(&ss->timeout, 1);
		return;

	case XS_SENSE:
	case XS_SHORTSENSE:
		if (scsi_interpret_sense(xs) != ERESTART)
			xs->retries = 0;

		/* FALLTHROUGH */
	case XS_BUSY:
	case XS_TIMEOUT:
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

	biodone(bp);
	scsi_xs_put(xs);
}

/*
 * Do a synchronous write.  Used to send control messages.
 */
int
scanjet_ctl_write(ss, buf, size, flags)
	struct ss_softc *ss;
	char *buf;
	u_int size;
	int flags;
{
	struct scsi_rw_scanner cmd;

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = WRITE;
	_lto3b(size, cmd.len);
	return (scsi_scsi_cmd(ss->sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), (u_char *) buf, size, 0, 100000, NULL,
	    flags | SCSI_DATA_OUT));
}


/*
 * Do a synchronous read.  Used to read responses to control messages.
 */
int
scanjet_ctl_read(ss, buf, size, flags)
	struct ss_softc *ss;
	char *buf;
	u_int size;
	int flags;
{
	struct scsi_rw_scanner cmd;

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = READ;
	_lto3b(size, cmd.len);
	return (scsi_scsi_cmd(ss->sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), (u_char *) buf, size, 0, 100000, NULL,
	    flags | SCSI_DATA_IN));
}


#ifdef SCANJETDEBUG
static void show_es(char *es)
{
	char *p = es;

	while (*p) {
		if (*p == '\033')
			printf("[Esc]");
		else
			printf("%c", *p);
		++p;
	}
	printf("\n");
}
#endif

/*
 * simulate SCSI_SET_WINDOW for ScanJets
 */
int
scanjet_set_window(ss, flags)
	struct ss_softc *ss;
	int flags;
{
	char escape_codes[128];
	size_t len;
	int n;

	snprintf(escape_codes, sizeof escape_codes,
	    "\033*f%ldP\033*f%ldQ\033*f%ldX\033*f%ldY\033*a%dR\033*a%dS",
	    ss->sio.scan_width / 4,
	    ss->sio.scan_height / 4,
	    ss->sio.scan_x_origin / 4,
	    ss->sio.scan_y_origin / 4,
	    ss->sio.scan_x_resolution,
	    ss->sio.scan_y_resolution);

	switch (ss->sio.scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
		ss->sio.scan_bits_per_pixel = 1;
		/*
		 * Use line art mode (\033*aoT) and make image data be
		 * min-is-white ala PBM (\033*a0I).
		 */
		strlcat(escape_codes, "\033*a0T\033*a0I", sizeof escape_codes);
		break;
	case SIM_DITHERED_MONOCHROME:
		ss->sio.scan_bits_per_pixel = 1;
		/* 
		 * Use dithered mode (\033*a3T) and make image data be
		 * min-is-white ala PBM (\033*a0I).
		 */
		strlcat(escape_codes, "\033*a3T\033*a0I", sizeof escape_codes);
		break;
	case SIM_GRAYSCALE:
		ss->sio.scan_bits_per_pixel = 8;
		/*
		 * Use grayscale mode (\033*a4T) and make image data be
		 * min-is-black ala PGM (\033*a1I)
		 */
		strlcat(escape_codes, "\033*a4T\033*a1I", sizeof escape_codes);
		break;
	case SIM_COLOR:
		ss->sio.scan_bits_per_pixel = 24;
		/*
		 * Use RGB color mode (\033*a5T), make image data be
		 * min-is-black ala PPM (\033*a1I) and use pass-through matrix,
		 * i.e. disable NTSC (\033*u2T).
		 */
		strlcat(escape_codes, "\033*a5T\033*a1I\033*u2T",
		    sizeof escape_codes);
		break;
	}

	/*
	 * If the escape sequence has been truncated at this point, appending
	 * the next sequence will also cause truncation, and this time we pay
	 * attention.
	 */
	len = strlen(escape_codes);
	n = snprintf(escape_codes + len, sizeof escape_codes - len,
	    "\033*a%dG\033*a%dL\033*a%dK",
	    ss->sio.scan_bits_per_pixel,
	    (int)(ss->sio.scan_brightness) - 128,
	    (int)(ss->sio.scan_contrast) - 128);

	if (n >= sizeof escape_codes - len)
		return (ENOMEM);
	len += n;

	return (scanjet_ctl_write(ss, escape_codes, len, flags));
}

/* atoi() is from /sys/arch/amiga/dev/ite.c
   and is only used in scanjet_compute_sizes */

__inline static int
atoi(cp)
	const char *cp;
{
	int n;

	for (n = 0; *cp && *cp >= '0' && *cp <= '9'; cp++)
		n = n * 10 + *cp - '0';

	return (n);
}

int
scanjet_compute_sizes(ss, flags)
	struct ss_softc *ss;
	int flags;
{
	int error;
	static char *wfail = "%s: interrogate write failed\n";
	static char *rfail = "%s: interrogate read failed\n";
	static char *dfail = "%s: bad data returned\n";
	static char *mono  = "\033*s1025E"; /* bytes wide  */
	static char *color = "\033*s1024E"; /* pixels wide */
	static char *high  = "\033*s1026E"; /* pixels high */
	char response[20];
	char *p;

	/*
	 * Deal with the fact that the HP ScanJet IIc uses 1/300" not 1/1200"
	 * as its base unit of measurement.  PINT uses 1/1200" (yes I know
	 * ScanJet II's use decipoints as well but 1200 % 720 != 0)
	 */
	ss->sio.scan_width = (ss->sio.scan_width + 3) & 0xfffffffc;
	ss->sio.scan_height = (ss->sio.scan_height + 3) & 0xfffffffc;

	switch (ss->sio.scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
	case SIM_DITHERED_MONOCHROME:
		error = scanjet_ctl_write(ss, mono, strlen(mono), flags);
		break;
	case SIM_GRAYSCALE:
	case SIM_COLOR:
		error = scanjet_ctl_write(ss, color, strlen(color), flags);
		break;
	default:
		error = EIO;
		break;
	}
	if (error) {
		uprintf(wfail, ss->sc_dev.dv_xname);
		return (error);
	}
	error = scanjet_ctl_read(ss, response, 20, flags);
	if (error) {
		uprintf(rfail, ss->sc_dev.dv_xname);
		return (error);
	}
	p = strchr(response, 'd');
	if (p == NULL) {
		uprintf(dfail, ss->sc_dev.dv_xname);
		return (EIO);
	}
	ss->sio.scan_pixels_per_line = atoi(p + 1);
	if (ss->sio.scan_image_mode < SIM_GRAYSCALE)
		ss->sio.scan_pixels_per_line *= 8;

	error = scanjet_ctl_write(ss, high, strlen(high), flags);
	if (error) {
		uprintf(wfail, ss->sc_dev.dv_xname);
		return (error);
	}
	error = scanjet_ctl_read(ss, response, 20, flags);
	if (error) {
		uprintf(rfail, ss->sc_dev.dv_xname);
		return (error);
	}
	p = strchr(response, 'd');
	if (p == NULL) {
		uprintf(dfail, ss->sc_dev.dv_xname);
		return (EIO);
	}
	ss->sio.scan_lines = atoi(p + 1);

	ss->sio.scan_window_size = ss->sio.scan_lines *
	    ((ss->sio.scan_pixels_per_line * ss->sio.scan_bits_per_pixel) / 8);

	return (0);
}
