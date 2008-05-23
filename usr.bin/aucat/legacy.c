/*	$OpenBSD: legacy.c,v 1.1 2008/05/23 07:15:46 ratchov Exp $	*/
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

#include <fcntl.h>
#include <unistd.h>
#include <err.h>

#include "file.h"
#include "aparams.h"
#include "dev.h"


/* headerless data files.  played at /dev/audio's defaults.
 */
#define FMT_RAW	0

/* Sun/NeXT .au files.  header is skipped and /dev/audio is configured
 * for monaural 8-bit ulaw @ 8kHz, the de facto format for .au files,
 * as well as the historical default configuration for /dev/audio.
 */
#define FMT_AU	1

/* RIFF WAV files.  header is parsed for format details which are
 * applied to /dev/audio.
 */
#define FMT_WAV	2


int
legacy_play(char *dev, char *aufile)
{
	struct audio_prinfo ai;
	struct audio_info info;
	struct aparams par;
	ssize_t rd;
	off_t datasz;
	char buf[5120];
	int afd, fd, fmt = FMT_RAW;
	u_int32_t pos = 0;
	char magic[4];

	if ((fd = open(aufile, O_RDONLY)) < 0) {
		warn("cannot open %s", aufile);
		return(1);
	}

	if (read(fd, magic, sizeof(magic)) != sizeof(magic)) {
		/* read() error, or the file is smaller than sizeof(magic).
		 * treat as a raw file, like previous versions of aucat.
		 */
	} else if (!strncmp(magic, ".snd", 4)) {
		fmt = FMT_AU;
		if (read(fd, &pos, sizeof(pos)) == sizeof(pos))
			pos = ntohl(pos);
	} else if (!strncmp(magic, "RIFF", 4) &&
		    wav_readhdr(fd, &par, &datasz)) {
			fmt = FMT_WAV;
	}

	/* seek to start of audio data.  wav_readhdr already took care
	 * of this for FMT_WAV.
	 */
	if (fmt == FMT_RAW || fmt == FMT_AU)
		if (lseek(fd, (off_t)pos, SEEK_SET) == -1)
			warn("lseek");

	if ((afd = open(dev, O_WRONLY)) < 0) {
		warn("can't open %s", dev);
		return(1);
	}

	AUDIO_INITINFO(&info);
	ai = info.play;

	switch(fmt) {
	case FMT_WAV:
		sun_partoinfo(&ai, &par);
		break;
	case FMT_AU:
		ai.encoding = AUDIO_ENCODING_ULAW;
		ai.precision = 8;
		ai.sample_rate = 8000;
		ai.channels = 1;
		break;
	case FMT_RAW:
	default:
		break;
	}

	info.play = ai;
	if (ioctl(afd, AUDIO_SETINFO, &info) < 0) {
		warn("%s", dev);
		/* only WAV could fail in previous aucat versions (unless
		 * the parameters returned by AUDIO_GETINFO would fail,
		 * which is unlikely)
		 */
		if (fmt == FMT_WAV)
			return(1);
	}

	/* parameters may be silently modified.  see audio(9)'s
	 * description of set_params.  for compatability with previous
	 * aucat versions, continue running if something doesn't match.
	 */
	(void) ioctl(afd, AUDIO_GETINFO, &info);
	if (info.play.encoding != ai.encoding ||
	    info.play.precision != ai.precision ||
	    info.play.channels != ai.channels ||
	    /* devices may return a very close rate, such as 44099 when
	     * 44100 was requested.  the difference is inaudible.  allow
	     * 2% deviation as an example of how to cope.
	     */
	    (info.play.sample_rate > ai.sample_rate * 1.02 ||
	    info.play.sample_rate < ai.sample_rate * 0.98)) {
		warnx("format not supported by %s", dev);
	}

	while ((rd = read(fd, buf, sizeof(buf))) > 0)
		if (write(afd, buf, rd) != rd)
			warn("write");
	if (rd == -1)
		warn("read");

	(void) close(afd);
	(void) close(fd);

	return(0);
}
