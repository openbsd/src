/*	$NetBSD: ss_mustek.c,v 1.1 1996/02/18 20:32:47 mycroft Exp $	*/

/*
 * Copyright (c) 1995 Joachim Koenig-Baltes.  All rights reserved.
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
 *	This product includes software developed by Joachim Koenig-Baltes.
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
 * special driver for MUSTEK flatbed scanners MFS 06000CX and MFS 12000CX
 * these scanners come with their own scsi card, containing an NCR53C400
 * SCSI controller chip. I'm in the progress of writing a driver for this
 * card to work under NetBSD-current. I've hooked it up to a Seagate ST01
 * hostadapter in the meantime, giving 350KB/sec for higher resolutions!
 *
 * I tried to connect it to my Adaptec 1542B, but with no success. It seems,
 * it does not like synchronous negotiation between Hostadapter and other
 * targets, but I could not turn this off for the 1542B.
 *
 * There is also an other reason why you would not like to connect it to your
 * favourite SCSI host adapter: The Mustek DOES NOT DISCONNECT. It will block
 * other traffic from the bus while a transfer is active.
 */

#include <sys/types.h>
#include <sys/param.h>
#include <sys/kernel.h>
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

#define MUSTEK_RETRIES 4

int mustek_get_params __P((struct ss_softc *));
int mustek_set_params __P((struct ss_softc *, struct scan_io *));
int mustek_trigger_scanner __P((struct ss_softc *));
void mustek_minphys __P((struct ss_softc *, struct buf *));
int mustek_read __P((struct ss_softc *, struct buf *));
int mustek_rewind_scanner __P((struct ss_softc *));

/* only used internally */
int mustek_get_status __P((struct ss_softc *, int, int));
void mustek_compute_sizes __P((struct ss_softc *));

/*
 * structure for the special handlers
 */
struct ss_special mustek_special = {
	mustek_set_params,
	mustek_trigger_scanner,
	mustek_get_params,
	mustek_minphys,
	mustek_read,
	mustek_rewind_scanner,
	NULL,			/* no adf support right now */
	NULL			/* no adf support right now */
};

/*
 * mustek_attach: attach special functions to ss
 */
void
mustek_attach(ss, sa)
	struct ss_softc *ss;
	struct scsibus_attach_args *sa;
{
	struct scsi_link *sc_link = sa->sa_sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_attach: start\n"));
	ss->sio.scan_scanner_type = 0;

	/* first, check the model which determines resolutions */
	if (!bcmp(sa->sa_inqbuf->product, "MFS-06000CX", 11)) {
		ss->sio.scan_scanner_type = MUSTEK_06000CX;
		printf(": Mustek 6000CX Flatbed 3-pass color scanner, 3 - 600 dpi\n");
	}
	if (!bcmp(sa->sa_inqbuf->product, "MFS-12000CX", 11)) {
		ss->sio.scan_scanner_type = MUSTEK_12000CX;
		printf(": Mustek 12000CX Flatbed 3-pass color scanner, 6 - 1200 dpi\n");
	}

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_attach: scanner_type = %d\n",
	    ss->sio.scan_scanner_type));

	/* install special handlers */
	ss->special = &mustek_special;

	/*
	 * populate the scanio struct with legal values
	 * the default should come from user space
	 */
	ss->sio.scan_width		= 1200;
	ss->sio.scan_height		= 1200;
	ss->sio.scan_x_resolution	= 99;
	ss->sio.scan_y_resolution	= 99;
	ss->sio.scan_x_origin		= 0;
	ss->sio.scan_y_origin		= 0;
	ss->sio.scan_brightness		= 100;
	ss->sio.scan_contrast		= 100;
	ss->sio.scan_quality		= 100;
	ss->sio.scan_image_mode		= SIM_GRAYSCALE;

	mustek_compute_sizes(ss);
}

int
mustek_get_params (ss)
	struct ss_softc *ss;
{

	return (0);
}

/*
 * check the parameters if the mustek is capable of fulfilling it
 * but don't send the command to the scanner in case the user wants
 * to change parameters by more than one call
 */
int
mustek_set_params(ss, sio)
	struct ss_softc *ss;
	struct scan_io *sio;
{
	int error;

	/*
	 * if the scanner is triggered, then rewind it
	 */
	if (ss->flags & SSF_TRIGGERED) {
		error = mustek_rewind_scanner(ss);
		if (error)
			return (error);
	}

	/* size constraints: 8.5" horizontally and 14" vertically */
#ifdef MUSTEK_INCH_SPEC
	/* sizes must be a multiple of 1/8" */
	sio->scan_x_origin -= sio->scan_x_origin % 150;
	sio->scan_y_origin -= sio->scan_y_origin % 150;
	sio->scan_width -= sio->scan_width % 150;
	sio->scan_height -= sio->scan_height % 150;
#endif
	if (sio->scan_width == 0 ||
	    sio->scan_x_origin + sio->scan_width > 10200 ||
	    sio->scan_height == 0 ||
	    sio->scan_y_origin + sio->scan_height > 16800)
		return (EINVAL);

	/*
	 * for now, only realize the values for the MUSTEK_06000CX
	 * in the future, values for the MUSTEK_12000CX will be implemented
	 */

	/*
	 * resolution (dpi) must be <= 300 and a multiple of 3 or
	 * between 300 and 600 and a multiple of 30
	 */
	sio->scan_x_resolution -= sio->scan_x_resolution <= 300 ?
	    sio->scan_x_resolution % 3 : sio->scan_x_resolution % 30;
	sio->scan_y_resolution -= sio->scan_y_resolution <= 300 ?
	    sio->scan_y_resolution % 3 : sio->scan_y_resolution % 30;
	if (sio->scan_x_resolution < 3 || sio->scan_x_resolution > 600 ||
	    sio->scan_x_resolution != sio->scan_y_resolution)
		return (EINVAL);

	/* assume brightness values are between 64 and 136 in steps of 3 */
	sio->scan_brightness -= (sio->scan_brightness - 64) % 3;
	if (sio->scan_brightness < 64 || sio->scan_brightness > 136)
		return (EINVAL);

	/* contrast values must be between 16 and 184 in steps of 7 */
	sio->scan_contrast -= (sio->scan_contrast - 16) % 7;
	if (sio->scan_contrast < 16 || sio->scan_contrast > 184)
		return (EINVAL);

	/*
	 * velocity: between 0 (fast) and 4 (slow) which will be mapped
	 * to 100% = 4, 80% = 3, 60% = 2, 40% = 1, 20% = 0
	 * must be a multiple of 20
	 */
	sio->scan_quality -= sio->scan_quality % 20;
	if (sio->scan_quality < 20 || sio->scan_quality > 100)
		return (EINVAL);

	switch (sio->scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
	case SIM_DITHERED_MONOCHROME:
	case SIM_GRAYSCALE:
	case SIM_RED:
	case SIM_GREEN:
	case SIM_BLUE:
		break;
	default:
		return (EINVAL);
	}

	/* change ss_softc to the new values, but save ro-variables */
	sio->scan_scanner_type = ss->sio.scan_scanner_type;
	bcopy(sio, &ss->sio, sizeof(struct scan_io));

	mustek_compute_sizes(ss);

	return (0);
}

/*
 * trim the requested transfer to a multiple of the line size
 * this is called only from ssread() which guarantees, scanner is triggered
 * In the future, it will trim the transfer to not read to much at a time
 * because the mustek cannot disconnect. It will be calculated by the
 * resolution, the velocity and the number of bytes per line.
 */
void
mustek_minphys(ss, bp)
	struct ss_softc *ss;
	struct buf *bp;
{
	struct scsi_link *sc_link = ss->sc_link;

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_minphys: before: %d\n",
	    bp->b_bcount));
	bp->b_bcount -= bp->b_bcount %
	    ((ss->sio.scan_pixels_per_line * ss->sio.scan_bits_per_pixel) / 8);
	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_minphys: after:  %d\n",
	    bp->b_bcount));
}

/*
 * trigger the scanner to start a scan operation
 * this includes sending the mode- and window-data, starting the scanner
 * and getting the image size info
 */
int
mustek_trigger_scanner(ss)
	struct ss_softc *ss;
{
	struct mustek_mode_select_cmd mode_cmd;
	struct mustek_mode_select_data mode_data;
	struct mustek_set_window_cmd window_cmd;
	struct mustek_set_window_data window_data;
	struct mustek_start_scan_cmd start_scan_cmd;
	struct scsi_link *sc_link = ss->sc_link;
#ifndef MUSTEK_INCH_SPEC
	int pixel_tlx, pixel_tly, pixel_brx, pixel_bry, paperlength;
#endif
	int error;

	mustek_compute_sizes(ss);

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_trigger_scanner\n"));

	/*
	 * set the window params and send the scsi command
	 */
	bzero(&window_cmd, sizeof(window_cmd));
	window_cmd.opcode = MUSTEK_SET_WINDOW;
	window_cmd.length = sizeof(window_data);

	bzero(&window_data, sizeof(window_data));
	window_data.frame_header = MUSTEK_LINEART_BACKGROUND | MUSTEK_UNIT_SPEC;
#ifdef MUSTEK_INCH_SPEC
	/* the positional values are all 1 byte because 256 / 8 = 32" */
	window_data.frame_tl_x_0 = ss->sio.scan_x_origin / 150;
	window_data.frame_tl_x_1 = 0;
	window_data.frame_tl_y_0 = ss->sio.scan_y_origin / 150;
	window_data.frame_tl_y_1 = 0;
	window_data.frame_br_x_0 = (ss->sio.scan_x_origin +
				    ss->sio.scan_width) / 150;
	window_data.frame_br_x_1 = 0;
	window_data.frame_br_y_0 = (ss->sio.scan_y_origin +
				    ss->sio.scan_height) / 150;
	window_data.frame_br_y_1 = 0;
#else
	pixel_tlx = (ss->sio.scan_x_origin * ss->sio.scan_x_resolution) / 1200;
	window_data.frame_tl_x_0 = pixel_tlx & 0xff;
	window_data.frame_tl_x_1 = (pixel_tlx >> 8) & 0xff;
	pixel_tly = (ss->sio.scan_y_origin * ss->sio.scan_y_resolution) / 1200;
	window_data.frame_tl_y_0 = pixel_tly & 0xff;
	window_data.frame_tl_y_1 = (pixel_tly >> 8) & 0xff;
	pixel_brx = pixel_tlx +
	    (ss->sio.scan_width * ss->sio.scan_x_resolution) / 1200;
	window_data.frame_br_x_0 = pixel_brx & 0xff;
	window_data.frame_br_x_1 = (pixel_brx >> 8) & 0xff;
	pixel_bry = pixel_tly +
	    (ss->sio.scan_height * ss->sio.scan_y_resolution) / 1200;
	window_data.frame_br_y_0 = pixel_bry & 0xff;
	window_data.frame_br_y_1 = (pixel_bry >> 8) & 0xff;
#endif

#if MUSTEK_WINDOWS >= 1
	window_data.window1_header = MUSTEK_WINDOW_MASK | MUSTEK_UNIT_SPEC;
	window_data.window1_tl_x_0 = window_data.frame_tl_x_0;
	window_data.window1_tl_x_1 = window_data.frame_tl_x_1;
	window_data.window1_tl_y_0 = window_data.frame_tl_y_0;
	window_data.window1_tl_y_1 = window_data.frame_tl_y_1;
	window_data.window1_br_x_0 = window_data.frame_br_x_0;
	window_data.window1_br_x_1 = window_data.frame_br_x_1;
	window_data.window1_br_y_0 = window_data.frame_br_y_0;
	window_data.window1_br_y_1 = window_data.frame_br_y_1;
#endif

	/* send the set window command to the scanner */
	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_set_parms: set_window\n"));
	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *) &window_cmd,
	    sizeof(window_cmd), (u_char *) &window_data, sizeof(window_data),
	    MUSTEK_RETRIES, 5000, NULL, SCSI_DATA_OUT);
	if (error)
		return (error);

	/*
	 * do what it takes to actualize the mode
	 */
	bzero(&mode_cmd, sizeof(mode_cmd));
	mode_cmd.opcode = MUSTEK_MODE_SELECT;
	mode_cmd.length_0 = sizeof(mode_data);

	bzero(&mode_data, sizeof(mode_data));
	mode_data.mode =
	    MUSTEK_MODE_MASK | MUSTEK_HT_PATTERN_BUILTIN | MUSTEK_UNIT_SPEC;
	if (ss->sio.scan_x_resolution <= 300) {
		mode_data.resolution = ss->sio.scan_x_resolution / 3;
	} else {
		/*
		 * the resolution values is computed by modulo 100, but not
		 * for 600dpi, where the value is 100 (a bit tricky, but ...)
		 */
		mode_data.resolution =
		    ((ss->sio.scan_x_resolution - 1) % 100) + 1;
	}
	mode_data.brightness = (ss->sio.scan_brightness - 64) / 3;
	mode_data.contrast = (ss->sio.scan_contrast - 16) / 7;
	mode_data.grain = 0;
	mode_data.velocity = ss->sio.scan_quality / 20 - 1;
#ifdef MUSTEK_INCH_SPEC
	mode_data.paperlength_0 = 14 * 8;	/* 14" */
	mode_data.paperlength_1 = 0;
#else
	paperlength = 14 * ss->sio.scan_y_resolution; /* 14" */
	mode_data.paperlength_0 = paperlength & 0xff;
	mode_data.paperlength_1 = (paperlength >> 8) & 0xff;
#endif

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_trigger_scanner: mode_select\n"));
	/* send the command to the scanner */
	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *) &mode_cmd,
	    sizeof(mode_cmd), (u_char *) &mode_data, sizeof(mode_data),
	    MUSTEK_RETRIES, 5000, NULL, SCSI_DATA_OUT);
	if (error)
		return (error);

	/*
	 * now construct and send the start command
	 */
	bzero(&start_scan_cmd,sizeof(start_scan_cmd));
	start_scan_cmd.opcode = MUSTEK_START_STOP;
	start_scan_cmd.mode = MUSTEK_SCAN_START;
	if (ss->sio.scan_x_resolution <= 300)
		start_scan_cmd.mode |= MUSTEK_RES_STEP_1;
	else
		start_scan_cmd.mode |= MUSTEK_RES_STEP_10;
	switch (ss->sio.scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
	case SIM_DITHERED_MONOCHROME:
		start_scan_cmd.mode |= MUSTEK_BIT_MODE | MUSTEK_GRAY_FILTER;
		break;
	case SIM_GRAYSCALE:
		start_scan_cmd.mode |= MUSTEK_GRAY_MODE | MUSTEK_GRAY_FILTER;
		break;
	case SIM_RED:
		start_scan_cmd.mode |= MUSTEK_GRAY_MODE | MUSTEK_RED_FILTER;
		break;
	case SIM_GREEN:
		start_scan_cmd.mode |= MUSTEK_GRAY_MODE | MUSTEK_GREEN_FILTER;
		break;
	case SIM_BLUE:
		start_scan_cmd.mode |= MUSTEK_GRAY_MODE | MUSTEK_BLUE_FILTER;
		break;
	}

	/* send the command to the scanner */
	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_trigger_scanner: start_scan\n"));
	error = scsi_scsi_cmd(sc_link, (struct scsi_generic *) &start_scan_cmd,
	    sizeof(start_scan_cmd), NULL, 0,
	    MUSTEK_RETRIES, 5000, NULL, 0);
	if (error)
		return (error);

	/*
	 * now check if scanner ready this time with update of size info
	 * we wait here so that if the user issues a read directly afterwards,
	 * the scanner will respond directly (otherwise we had to sleep with
	 * a buffer locked in memory)
	 */
	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_trigger_scanner: get_status\n"));
	error = mustek_get_status(ss, 60, 1);
	if (error)
		return (error);

	return (0);
}

/*
 * stop a scan operation in progress
 */
int
mustek_rewind_scanner(ss)
	struct ss_softc *ss;
{
	struct mustek_start_scan_cmd cmd;
	struct scsi_link *sc_link = ss->sc_link;
	int error;

	if (ss->sio.scan_window_size != 0) {
		/*
		 * only if not all data has been read, the scanner has to be
		 * stopped
		 */
		bzero(&cmd, sizeof(cmd));
		cmd.opcode = MUSTEK_START_STOP;
		cmd.mode = MUSTEK_SCAN_STOP;

		/* send the command to the scanner */
		SC_DEBUG(sc_link, SDEV_DB1,
		    ("mustek_rewind_scanner: stop_scan\n"));
		error = scsi_scsi_cmd(sc_link, (struct scsi_generic *) &cmd,
		    sizeof(cmd), NULL, 0, MUSTEK_RETRIES, 5000, NULL, 0);
		if (error)
			return (error);
	}

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_rewind_scanner: end\n"));

	return (0);
}

/*
 * read the requested number of bytes/lines from the scanner
 */
int
mustek_read(ss, bp)
	struct ss_softc *ss;
	struct buf *bp;
{
	struct mustek_read_cmd cmd;
	struct scsi_link *sc_link = ss->sc_link;
	u_long lines_to_read;

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_read: start\n"));

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = MUSTEK_READ;

	/* instead of the bytes, the mustek wants the number of lines */
	lines_to_read = bp->b_bcount /
	    ((ss->sio.scan_pixels_per_line * ss->sio.scan_bits_per_pixel) / 8);
	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_read: read %d lines\n",
	    lines_to_read));
	cmd.length_0 = lines_to_read & 0xff;
	cmd.length_1 = (lines_to_read >> 8) & 0xff;
	cmd.length_2 = (lines_to_read >> 16) & 0xff;

	/*
	 * go ask the adapter to do all this for us
	 */
	if (scsi_scsi_cmd(sc_link, (struct scsi_generic *) &cmd, sizeof(cmd),
	    (u_char *) bp->b_data, bp->b_bcount, MUSTEK_RETRIES, 10000, bp,
	    SCSI_NOSLEEP | SCSI_DATA_IN) != SUCCESSFULLY_QUEUED)
		printf("%s: not queued\n", ss->sc_dev.dv_xname);
	else {
		ss->sio.scan_lines -= lines_to_read;
		ss->sio.scan_window_size -= bp->b_bcount;
	}

	return (0);
}

/*
 * check if the scanner is ready to take commands
 *   wait timeout seconds and try only every second
 *   if update, then update picture size info
 *
 *   returns EBUSY if scanner not ready
 */
int
mustek_get_status(ss, timeout, update)
	struct ss_softc *ss;
	int timeout, update;
{
	struct mustek_get_status_cmd cmd;
	struct mustek_get_status_data data;
	struct scsi_link *sc_link = ss->sc_link;
	int error, lines, bytes_per_line;

	bzero(&cmd, sizeof(cmd));
	cmd.opcode = MUSTEK_GET_STATUS;
	cmd.length = sizeof(data);

	while (1) {
		SC_DEBUG(sc_link, SDEV_DB1, ("mustek_get_status: stat_cmd\n"));
		error = scsi_scsi_cmd(sc_link, (struct scsi_generic *) &cmd,
		    sizeof(cmd), (u_char *) &data, sizeof(data), MUSTEK_RETRIES,
		    5000, NULL, SCSI_DATA_IN);
		if (error)
			return (error);
		if ((data.ready_busy == MUSTEK_READY) ||
		    (timeout-- <= 0))
			break;
		/* please wait a second */
		tsleep((caddr_t)mustek_get_status, PRIBIO + 1, "mtkrdy", hz);
	}

	if (update) {
		bytes_per_line =
		    (data.bytes_per_line_1 << 8) |
		    data.bytes_per_line_0;
		lines =
		    (data.lines_2 << 16) |
		    (data.lines_1 << 8) |
		    data.lines_0;
		if (lines != ss->sio.scan_lines) {
			printf("mustek: lines actual(%d) != computed(%d)\n",
			    lines, ss->sio.scan_lines);
			return (EIO);
		}
		if (bytes_per_line * lines != ss->sio.scan_window_size) {
			printf("mustek: win-size actual(%d) != computed(%d)\n",
			    bytes_per_line * lines, ss->sio.scan_window_size);
		    return (EIO);
		}

		SC_DEBUG(sc_link, SDEV_DB1,
		    ("mustek_get_size: bpl=%d, lines=%d\n",
		    (ss->sio.scan_pixels_per_line * ss->sio.scan_bits_per_pixel) / 8,
		    ss->sio.scan_lines));
		SC_DEBUG(sc_link, SDEV_DB1, ("window size = %d\n",
		    ss->sio.scan_window_size));
	}

	SC_DEBUG(sc_link, SDEV_DB1, ("mustek_get_status: end\n"));
	if (data.ready_busy == MUSTEK_READY)
		return (0);
	else
		return (EBUSY);
}

/*
 * mustek_compute_sizes: compute window_size and lines for the picture
 *   this function is called from different places in the code
 */
void
mustek_compute_sizes(ss)
	struct ss_softc *ss;
{

	switch (ss->sio.scan_image_mode) {
	case SIM_BINARY_MONOCHROME:
	case SIM_DITHERED_MONOCHROME:
		ss->sio.scan_bits_per_pixel = 1;
		break;
	case SIM_GRAYSCALE:
	case SIM_RED:
	case SIM_GREEN:
	case SIM_BLUE:
		ss->sio.scan_bits_per_pixel = 8;
		break;
	}

	/*
	 * horizontal number of bytes is always a multiple of 2,
	 * in 8-bit mode at least
	 */
	ss->sio.scan_pixels_per_line =
	    (ss->sio.scan_width * ss->sio.scan_x_resolution) / 1200;
	if (ss->sio.scan_bits_per_pixel == 1)
		/* make it a multiple of 16, and thus of 2 bytes */
		ss->sio.scan_pixels_per_line =
		    (ss->sio.scan_pixels_per_line + 15) & 0xfffffff0;
	else
		ss->sio.scan_pixels_per_line =
		    (ss->sio.scan_pixels_per_line + 1) & 0xfffffffe;

	ss->sio.scan_lines =
	    (ss->sio.scan_height * ss->sio.scan_y_resolution) / 1200;
	ss->sio.scan_window_size = ss->sio.scan_lines * 
	    ((ss->sio.scan_pixels_per_line * ss->sio.scan_bits_per_pixel) / 8);
}
