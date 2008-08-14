/*	$OpenBSD: dev_sun.c,v 1.5 2008/08/14 09:58:55 ratchov Exp $	*/
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

#include <sys/types.h>
#include <sys/ioctl.h>
#include <sys/audioio.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "conf.h"
#include "aparams.h"
#include "dev.h"

/*
 * Convert sun device parameters to struct aparams
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
 * Convert struct aparams to sun device parameters.
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
 * in "infr" and "onfd" we return the input and the output
 * block sizes respectively.
 */
int
sun_open(char *path, struct aparams *ipar, struct aparams *opar,
	 unsigned *infr, unsigned *onfr)
{
	int fd;
	int fullduplex;
	int flags;
	struct audio_info aui;	
	struct audio_bufinfo aubi;

	if (!ipar && !opar)
		errx(1, "%s: must at least play or record", path);

	if (ipar && opar) {
		flags = O_RDWR;
	} else  if (ipar) {
		flags = O_RDONLY;
	} else {
		flags = O_WRONLY;
	}
	fd = open(path, flags | O_NONBLOCK);
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
		warn("sun_open: getinfo");
		close(fd);
		return -1;
	}
	if (opar) {
		if (!sun_infotopar(&aui.play, opar)) {
			close(fd);
			return -1;
		}
		if (ioctl(fd, AUDIO_GETPRINFO, &aubi) < 0) {
			warn("%s: AUDIO_GETPRINFO", path);
			close(fd);
			return -1;
		}
		*onfr = aubi.blksize * aubi.hiwat /
		    (aui.play.channels * aui.play.precision / 8);
	}
	if (ipar) {
		if (!sun_infotopar(&aui.record, ipar)) {
			close(fd);
			return -1;
		}
		if (ioctl(fd, AUDIO_GETRRINFO, &aubi) < 0) {
			warn("%s: AUDIO_GETRRINFO", path);
			close(fd);
			return -1;
		}
		*infr = aubi.blksize * aubi.hiwat /
		    (aui.record.channels * aui.record.precision / 8);
	}
	return fd;
}

/*
 * Drain and close the device
 */
void
sun_close(int fd)
{
	close(fd);
	DPRINTF("sun_close: closed\n");
}

/*
 * Start play/record simultaneously. Play buffers must be filled.
 */
void
sun_start(int fd)
{
	audio_info_t aui;

	/*
	 * Just unpause the device. The kernel will start playback and record
	 * simultaneously. There must be samples already written.
	 */
	AUDIO_INITINFO(&aui);	
	aui.play.pause = aui.record.pause = 0;
	if (ioctl(fd, AUDIO_SETINFO, &aui) < 0)
		err(1, "sun_start: setinfo");

	DPRINTF("sun_start: play/rec started\n");
}

/*
 * Drain play buffers and then stop play/record simultaneously.
 */
void
sun_stop(int fd)
{
	audio_info_t aui;

	/*
	 * Sun API doesn't not allows us to drain and stop without
	 * loosing the sync between playback and record. So, for now we
	 * just pause the device until this problem is worked around.
	 *
	 * there are three possible workarounds:
	 *
	 *	1) stop depending on this, ie. make the rest of the code
	 *	   able to resynchronize playback to record. Then just
	 *	   close/reset the device to stop it.
	 *
	 *	2) send "hiwat" blocks of silence and schedule the
	 *	   very same amount of silence to drop.
	 *
	 *	3) modify the AUDIO_DRAIN ioctl(2) not to loose sync
	 * 
	 */
	AUDIO_INITINFO(&aui);
	aui.play.pause = aui.record.pause = 1;	
	if (ioctl(fd, AUDIO_SETINFO, &aui) < 0)
		err(1, "sun_stop: setinfo");

	DPRINTF("sun_stop: play/rec stopped\n");
}

struct devops devops_sun = {
	sun_open,
	sun_close,
	sun_start,
	sun_stop
};
