/*	$OpenBSD: legacy.c,v 1.11 2010/04/05 19:52:42 jakemsr Exp $	*/
/*
 * Copyright (c) 1997 Kenneth Stailey.  All rights reserved.
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

#include <sndio.h>

#include <err.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "wav.h"

/*
 * Headerless data files.  Played at /dev/audio's defaults.
 */
#define FMT_RAW	0

/*
 * Sun/NeXT .au files.  Header is skipped and /dev/audio is configured
 * for monaural 8-bit ulaw @ 8kHz, the de facto format for .au files,
 * as well as the historical default configuration for /dev/audio.
 */
#define FMT_AU	1

/*
 * RIFF WAV files.  Header is parsed for format details which are
 * applied to /dev/audio.
 */
#define FMT_WAV	2


int
legacy_play(char *dev, char *aufile)
{
	struct sio_hdl *hdl;
	struct sio_par spar, par;
	struct aparams apar;
	ssize_t rd;
	off_t datasz;
	char buf[5120];
	size_t readsz;
	int fd, fmt = FMT_RAW;
	u_int32_t pos = 0, snd_fmt = 1, rate = 8000, chan = 1;
	char magic[4];
	short *map;

	if ((fd = open(aufile, O_RDONLY)) < 0) {
		warn("cannot open %s", aufile);
		return(1);
	}

	if (read(fd, magic, sizeof(magic)) != sizeof(magic)) {
		/*
		 * read() error, or the file is smaller than sizeof(magic).
		 * treat as a raw file, like previous versions of aucat.
		 */
	} else if (!strncmp(magic, ".snd", 4)) {
		fmt = FMT_AU;
		if (read(fd, &pos, sizeof(pos)) == sizeof(pos))
			pos = ntohl(pos);
		/* data size */
		if (lseek(fd, 4, SEEK_CUR) == -1)
			warn("lseek hdr");
		if (read(fd, &snd_fmt, sizeof(snd_fmt)) == sizeof(snd_fmt))
			snd_fmt = ntohl(snd_fmt);
		if (read(fd, &rate, sizeof(rate)) == sizeof(rate))
			rate = ntohl(rate);
		if (read(fd, &chan, sizeof(chan)) == sizeof(chan))
			chan = ntohl(chan);
	} else if (!strncmp(magic, "RIFF", 4) &&
		    wav_readhdr(fd, &apar, &datasz, &map)) {
			fmt = FMT_WAV;
	}

	/*
	 * Seek to start of audio data.  wav_readhdr already took care
	 * of this for FMT_WAV.
	 */
	if (fmt == FMT_RAW || fmt == FMT_AU)
		if (lseek(fd, (off_t)pos, SEEK_SET) == -1)
			warn("lseek");

	if ((hdl = sio_open(dev, SIO_PLAY, 0)) == NULL) {
		warnx("can't get sndio handle");
		return(1);
	}

	sio_initpar(&par);
	switch(fmt) {
	case FMT_WAV:
		par.rate = apar.rate;
		par.pchan = apar.cmax - apar.cmin + 1;
		par.sig = apar.sig;
		par.bits = apar.bits;
		par.le = apar.le;
		break;
	case FMT_AU:
		par.rate = rate;
		par.pchan = chan;
		par.sig = 1;
		par.bits = 16;
		par.le = SIO_LE_NATIVE;
		map = wav_ulawmap;
		if (snd_fmt == 27)
			map = wav_alawmap;
		break;
	case FMT_RAW:
	default:
		break;
	}
	spar = par;

	if (!sio_setpar(hdl, &par) || !sio_getpar(hdl, &par)) {
		warnx("can't set audio parameters");
		/*
		 * Only WAV could fail in previous aucat versions (unless
		 * the parameters returned by AUDIO_GETINFO would fail,
		 * which is unlikely).
		 */
		if (fmt == FMT_WAV)
			return(1);
	}

        /*
	 * Parameters may be silently modified.  See audio(9)'s
	 * description of set_params.  For compatability with previous
	 * aucat versions, continue running if something doesn't match.
	 */
	if (par.bits != spar.bits ||
	    par.sig != par.sig ||
	    par.le != spar.le ||
	    par.pchan != spar.pchan ||
	    /*
	     * Devices may return a very close rate, such as 44099 when
	     * 44100 was requested.  The difference is inaudible.  Allow
	     * 2% deviation as an example of how to cope.
	     */
	    (par.rate > spar.rate * 1.02 || par.rate < spar.rate * 0.98)) {
		warnx("format not supported");
	}
	if (!sio_start(hdl)) {
		warnx("could not start sndio");
		exit(1);
	}

	readsz = sizeof(buf);
	if (map)
		readsz /= 2;
	while ((rd = read(fd, buf, readsz)) > 0) {
		if (map) {
			wav_conv(buf, rd, map);
			rd *= 2;
		}
		if (sio_write(hdl, buf, rd) != rd)
			warnx("sio_write: short write");
	}
	if (rd == -1)
		warn("read");

	sio_close(hdl);
	close(fd);

	return(0);
}
