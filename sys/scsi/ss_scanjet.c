/*	$NetBSD: ss_scanjet.c,v 1.1 1996/02/18 20:32:49 mycroft Exp $	*/

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

#define SCANJET_RETRIES 4

int scanjet_get_params __P((struct ss_softc *));
int scanjet_set_params __P((struct ss_softc *, struct scan_io *));
int scanjet_trigger_scanner __P((struct ss_softc *));
int scanjet_read __P((struct ss_softc *, struct buf *));

/* only used internally */
int scanjet_write __P((struct ss_softc *ss, char *buf, u_int size, int flags));
int scanjet_set_window __P((struct ss_softc *ss));
void scanjet_compute_sizes __P((struct ss_softc *));

/*
 * structure for the special handlers
 */
struct ss_special scanjet_special = {
	scanjet_set_params,
	scanjet_trigger_scanner,
	scanjet_get_params,
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
	struct scsibus_attach_args *sa;
{
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("scanjet_attach: start\n"));
	ss->sio.scan_scanner_type = 0;

	/* first, check the model (which determines nothing yet) */

	if (!bcmp(sa->sa_inqbuf->product, "C1750A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf(": HP ScanJet IIc\n");
	}
	if (!bcmp(sa->sa_inqbuf->product, "C2500A", 6)) {
		ss->sio.scan_scanner_type = HP_SCANJET_IIC;
		printf(": HP ScanJet IIcx\n");
	}

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_attach: scanner_type = %d\n",
	    ss->sio.scan_scanner_type));

	/* now install special handlers */
	ss->special = &scanjet_special;

	/*
	 * populate the scanio struct with legal values
	 */
	ss->sio.scan_width		= 1200;
	ss->sio.scan_height		= 1200;
	ss->sio.scan_x_resolution	= 100;
	ss->sio.scan_y_resolution	= 100;
	ss->sio.scan_x_origin		= 0;
	ss->sio.scan_y_origin		= 0;
	ss->sio.scan_brightness		= 100;
	ss->sio.scan_contrast		= 100;
	ss->sio.scan_quality		= 100;
	ss->sio.scan_image_mode		= SIM_GRAYSCALE;

	scanjet_compute_sizes(ss);
}

int
scanjet_get_params(ss)
	struct ss_softc *ss;
{

	return (0);
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

	scanjet_compute_sizes(ss);

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
	char escape_codes[20];
	struct scsi_link *sc_link = ss->sc_link;
	int error;

	scanjet_compute_sizes(ss);

	/* send parameters */
	error = scanjet_set_window(ss);
	if (error) {
		uprintf("set window failed\n");
		return (error);
	}

	/* send "trigger" operation */
	strcpy(escape_codes, "\033*f0S");
	error = scanjet_write(ss, escape_codes, strlen(escape_codes), 0);
	if (error) {
		uprintf("trigger failed\n");
		return (error);
	}
	
	return (0);
}

int
scanjet_read(ss, bp)
	struct ss_softc *ss;
	struct buf *bp;
{
	struct scsi_rw_scanner cmd;
	struct scsi_link *sc_link = ss->sc_link;

	/*
	 *  Fill out the scsi command
	 */
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = READ;

	/*
	 * Handle "fixed-block-mode" tape drives by using the
	 * block count instead of the length.
	 */
	lto3b(bp->b_bcount, cmd.len);

	/*
	 * go ask the adapter to do all this for us
	 */
	if (scsi_scsi_cmd(sc_link, (struct scsi_generic *) &cmd, sizeof(cmd),
	    (u_char *) bp->b_data, bp->b_bcount, SCANJET_RETRIES, 100000, bp,
	    SCSI_NOSLEEP | SCSI_DATA_IN) != SUCCESSFULLY_QUEUED)
		printf("%s: not queued\n", ss->sc_dev.dv_xname);
	else {
		ss->sio.scan_window_size -= bp->b_bcount;
		if (ss->sio.scan_window_size < 0)
			ss->sio.scan_window_size = 0;
	}

	return (0);
}


/*
 * Do a synchronous write.  Used to send control messages.
 */
int 
scanjet_write(ss, buf, size, flags)
	struct ss_softc *ss;
	char *buf;
	u_int size;
	int flags;
{
	struct scsi_rw_scanner cmd;

	/*
	 * If it's a null transfer, return immediatly
	 */
	if (size == 0)
		return (0);
	bzero(&cmd, sizeof(cmd));
	cmd.opcode = WRITE;
	lto3b(size, cmd.len);
	return (scsi_scsi_cmd(ss->sc_link, (struct scsi_generic *) &cmd,
	    sizeof(cmd), (u_char *) buf, size, 0, 100000, NULL,
	    flags | SCSI_DATA_OUT));
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
scanjet_set_window(ss)
	struct ss_softc *ss;
{
	char escape_codes[128], *p;

	p = escape_codes;

	sprintf(p, "\033*f%dP", ss->sio.scan_width / 4);
	p += strlen(p);
	sprintf(p, "\033*f%dQ", ss->sio.scan_height / 4);
	p += strlen(p);
	sprintf(p, "\033*f%dX", ss->sio.scan_x_origin / 4);
	p += strlen(p);
	sprintf(p, "\033*f%dY", ss->sio.scan_y_origin / 4);
	p += strlen(p);
	sprintf(p, "\033*a%dR", ss->sio.scan_x_resolution);
	p += strlen(p);
	sprintf(p, "\033*a%dS", ss->sio.scan_y_resolution);
	p += strlen(p);
     
	switch (ss->sio.scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
		/* use "line art" mode */
		strcpy(p, "\033*a0T");
		p += strlen(p);
		/* make image data be "min-is-white ala PBM */
		strcpy(p, "\033*a0I");
		p += strlen(p);
		break;
	case SIM_DITHERED_MONOCHROME:
		/* use dithered mode */
		strcpy(p, "\033*a3T");
		p += strlen(p);
		/* make image data be "min-is-white ala PBM */
		strcpy(p, "\033*a0I");
		p += strlen(p);
		break;
	case SIM_GRAYSCALE:
		/* use grayscale mode */
		strcpy(p, "\033*a4T");
		p += strlen(p);
		/* make image data be "min-is-black ala PGM */
		strcpy(p, "\033*a1I");
		p += strlen(p);
		break;
	case SIM_COLOR:
		/* use RGB color mode */
		strcpy(p, "\033*a5T");
		p += strlen(p);
		/* make image data be "min-is-black ala PPM */
		strcpy(p, "\033*a1I");
		p += strlen(p);
		/* use pass-through matrix (disable NTSC) */
		strcpy(p, "\033*u2T");
		p += strlen(p);
	}

	sprintf(p, "\033*a%dG", ss->sio.scan_bits_per_pixel);
	p += strlen(p);
	sprintf(p, "\033*a%dL", (int)(ss->sio.scan_brightness) - 128);
	p += strlen(p);
	sprintf(p, "\033*a%dK", (int)(ss->sio.scan_contrast) - 128);
	p += strlen(p);

	return (scanjet_write(ss, escape_codes, p - escape_codes, 0));
}

void
scanjet_compute_sizes(ss)
	struct ss_softc *ss;
{

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
		ss->sio.scan_bits_per_pixel = 1;
		break;
	case SIM_GRAYSCALE:
		ss->sio.scan_bits_per_pixel = 8;
		break;
	case SIM_COLOR:
		ss->sio.scan_bits_per_pixel = 24;
		break;
	}

	ss->sio.scan_pixels_per_line =
	    (ss->sio.scan_width * ss->sio.scan_x_resolution) / 1200;
	if (ss->sio.scan_bits_per_pixel == 1)
		/* pad to byte boundary: */
		ss->sio.scan_pixels_per_line =
		    (ss->sio.scan_pixels_per_line + 7) & 0xfffffff8;

	ss->sio.scan_lines =
	    (ss->sio.scan_height * ss->sio.scan_y_resolution) / 1200;
	ss->sio.scan_window_size = ss->sio.scan_lines *
	    ((ss->sio.scan_pixels_per_line * ss->sio.scan_bits_per_pixel) / 8);
}
