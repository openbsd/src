/*	$OpenBSD: dev_sun.c,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
/*
 * Copyright (c) 2008 Alexandre Ratchov <alex@caoua.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "conf.h"
#include "aparams.h"
#include "dev.h"

/*
 * convert sun device parameters to struct params
 */
int
sun_infotopar(struct audio_prinfo *ai, struct aparams *par)
{
	par->rate = ai->sample_rate;
	par->bps = ai->precision / 8;
	par->bits = ai->precision;
	par->cmax = par->cmin + ai->channels - 1;
	if (par->cmax >= CHAN_MAX) {
		warnx("%u:%u: channel range out of bounds",
		    par->cmin, par->cmax);
		return 0;
	}
	par->msb = 1;
	switch (ai->encoding) {
	case AUDIO_ENCODING_SLINEAR_LE:
		par->le = 1;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_SLINEAR_BE:
		par->le = 0;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_ULINEAR_LE:
		par->le = 1;
		par->sig = 0;
		break;
	case AUDIO_ENCODING_ULINEAR_BE:
		par->le = 0;
		par->sig = 0;
		break;
	case AUDIO_ENCODING_SLINEAR:
		par->le = NATIVE_LE;
		par->sig = 1;
		break;
	case AUDIO_ENCODING_ULINEAR:
		par->le = NATIVE_LE;
		par->sig = 0;
		break;
	default:
		warnx("only linear encodings are supported for audio devices");
		return 0;
	}
	return 1;
}

/*
 * Convert struct params to sun device parameters.
 */
void
sun_partoinfo(struct audio_prinfo *ai, struct aparams *par)
{
	ai->sample_rate = par->rate;
	ai->precision = par->bps * 8;
	ai->channels = par->cmax - par->cmin + 1;
	if (par->le && par->sig) {
		ai->encoding = AUDIO_ENCODING_SLINEAR_LE;
	} else if (!par->le && par->sig) {
		ai->encoding = AUDIO_ENCODING_SLINEAR_BE;
	} else if (par->le && !par->sig) {
		ai->encoding = AUDIO_ENCODING_ULINEAR_LE;
	} else {
		ai->encoding = AUDIO_ENCODING_ULINEAR_BE;
	}
}

/*
 * Open the device and pause it, so later play and record
 * can be started simultaneously.
 *
 * int "infr" and "onfd" we return the input and the output
 * block sizes respectively.
 */
int
dev_init(char *path, struct aparams *ipar, struct aparams *opar,
	 unsigned *infr, unsigned *onfr)
{
	int fd;
	int fullduplex;
	struct audio_info aui;	

	if (!ipar && !opar)
		errx(1, "%s: must at least play or record", path);

	fd = open(path, O_RDWR | O_NONBLOCK);
	if (fd < 0) {
		warn("%s", path);
		return -1;
	}

	/*
	 * If both play and record are requested then
	 * set full duplex mode.
	 */
	if (ipar && opar) {
		fullduplex = 1;
		if (ioctl(fd, AUDIO_SETFD, &fullduplex) < 0) {
			warn("%s: can't set full-duplex", path);
			close(fd);
			return -1;
		}
	}
	
	/*
	 * Set parameters and pause the device. When paused, the write(2)
	 * syscall will queue samples but the the kernel will not start playing
	 * them. Setting the 'mode' and pausing the device must be done in a
	 * single ioctl() call, otherwise the sun driver will start the device
	 * and fill the record buffers.
	 */
	AUDIO_INITINFO(&aui);
	aui.mode = 0;
	aui.lowat = UINT_MAX / 2; /* will set lowat = hiwat - 1 */
	if (opar) {
		sun_partoinfo(&aui.play, opar);
		aui.play.pause = 1;
		aui.mode |= AUMODE_PLAY;
	}
	if (ipar) {
		sun_partoinfo(&aui.record, ipar);
		aui.record.pause = 1;
		aui.mode |= AUMODE_RECORD;
	}
	if (ioctl(fd, AUDIO_SETINFO, &aui) < 0) {
		fprintf(stderr, "%s: can't set audio params to ", path);
		if (ipar)
			aparams_print(ipar);
		if (opar) {
			if (ipar)
				fprintf(stderr, " and ");
			aparams_print(opar);
		}
		fprintf(stderr, ": %s\n", strerror(errno));
		close(fd);
		return -1;
	}
	if (ioctl(fd, AUDIO_GETINFO, &aui) < 0) {
		warn("dev_init: getinfo");
		close(fd);
		return -1;
	}
	if (opar) {
		/*
		 * We _must_ ensure that write() will accept at most
		 * one block when it unblocks. Here is our definition
		 * of the block size: the minimum amount of data
		 * write() accepts immediately when it unblocks. If
		 * write() accepts more that 1 block, then this means
		 * that we failed to provide the first block early
		 * enough thus underrun happened.
		 *
		 * If we fail to ensure that lowat = hiwat - 1, then
		 * we will trigger the underrun detection mechanism.
		 * Recording doesn't use the water mark non-sense.
		 */
		if (aui.lowat != aui.hiwat - 1) {
			warnx("%s: failed to disable lowat: hiwat = %u, "
			    "lowat = %u", path, aui.hiwat, aui.lowat);
			close(fd);
			return -1;
		}
		if (!sun_infotopar(&aui.play, opar)) {
			close(fd);
			return -1;
		}
		*onfr = aui.play.block_size /
		    (aui.play.channels * aui.play.precision / 8);
	}
	if (ipar) {
		if (!sun_infotopar(&aui.record, ipar)) {
			close(fd);
			return -1;
		}
		*infr = aui.record.block_size /
		    (aui.record.channels * aui.record.precision / 8);
	}
	return fd;
}

void
dev_done(int fd)
{
	close(fd);
}

/*
 * Start play/record.
 */
void
dev_start(int fd)
{
	audio_info_t aui;

	/*
	 * Just unpause the device. The kernel will start playback and record
	 * simultaneously. There must be samples already written.
	 */
	AUDIO_INITINFO(&aui);	
	aui.play.pause = aui.record.pause = 0;
	if (ioctl(fd, AUDIO_SETINFO, &aui) < 0)
		err(1, "dev_start: setinfo");

	DPRINTF("dev_start: play/rec started\n");
}

/*
 * Stop play/record and clear kernel buffers so that dev_start() can be called
 * again.
 */
void
dev_stop(int fd)
{
	audio_info_t aui;
	unsigned mode;

	if (ioctl(fd, AUDIO_DRAIN) < 0)
		err(1, "dev_stop: drain");

	/*
	 * The only way to clear kernel buffers and to pause the device
	 * simultaneously is to set the mode again (to the same value).
	 */
	if (ioctl(fd, AUDIO_GETINFO, &aui) < 0)
		err(1, "dev_stop: getinfo");

	mode = aui.mode;
	AUDIO_INITINFO(&aui);
	aui.mode = mode;
	aui.play.pause = aui.record.pause = 1;	
	if (ioctl(fd, AUDIO_SETINFO, &aui) < 0)
		err(1, "dev_stop: setinfo");

	DPRINTF("dev_stop: play/rec stopped\n");
}
