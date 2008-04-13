/*	$OpenBSD: aucat.c,v 1.14 2008/04/13 22:39:29 jakemsr Exp $	*/
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

#include <sys/types.h>
#include <sys/audioio.h>
#include <sys/ioctl.h>

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#define _PATH_AUDIO "/dev/audio"

/*
 * aucat: concatenate and play Sun 8-bit .au files or 8/16-bit
 * uncompressed WAVE RIFF files
 */

int 		playfile(int, audio_info_t *);
int		readwaveheader(int, audio_info_t *);
__dead void	usage(void);

int afd;

/*
 * function playfile: given a file which is positioned at the beginning
 * of what is assumed to be an .au data stream copy it out to the audio
 * device.  Return 0 on success, -1 on failure.
 */
int
playfile(int fd, audio_info_t *audioinfo)
{
	ssize_t rd;
	char buf[5120];

	/*
	 * If we don't wait here, the AUDIO_SETINFO ioctl interrupts
	 * the playback of the previous file.
	 */
	if (ioctl(afd, AUDIO_DRAIN, NULL) == -1)
		warn("AUDIO_DRAIN");

	if (ioctl(afd, AUDIO_SETINFO, audioinfo) == -1) {
		warn("AUDIO_SETINFO");
		return -1;
	}

	while ((rd = read(fd, buf, sizeof(buf))) > 0)
		if (write(afd, buf, rd) != rd)
			warn("write");
	if (rd == -1)
		warn("read");

	return (0);
}

/*
 * function readwaveheader: given a file which is positioned at four
 * bytes into a RIFF file header, read the rest of the header, check
 * to see if it is a simple WAV file that we can handle, seek to the
 * beginning of the audio data, and set the playback parameters in
 * the audio_info_t structure.  Return 0 on success, -1 on failure.
 */
int
readwaveheader(int fd, audio_info_t *audioinfo)
{
	/*
	 * The simplest form of a RIFF file...
	 */
	struct {
	/*	u_int32_t riff_chunkid; -- this is read before in main()! */
		u_int32_t riff_chunksize;
		u_int32_t riff_format;

		u_int32_t fmt_subchunkid;
		u_int32_t fmt_subchunksize;

		u_int16_t fmt_format;		/* 1 = PCM uncompressed */
		u_int16_t fmt_channels;		/* 1 = mono, 2 = stereo */
		u_int32_t fmt_samplespersec;	/* 8000, 22050, 44100 etc. */
		u_int32_t fmt_byterate;		/* total bytes per second */
		u_int16_t fmt_blockalign;	/* channels * bitspersample/8 */
		u_int16_t fmt_bitspersample;	/* 8 = 8 bits, 16 = 16 bits etc. */
	} header;
	u_int datatag;
	char c;

	/*
	 * Is it an uncompressed wave file?
	 */
	if (read(fd, &header, sizeof(header)) != sizeof(header)) {
		warn("read");
		return -1;
	}
	if (strncmp((char *) &header.riff_format, "WAVE", 4) ||
	    letoh16(header.fmt_format) != 1 ||
	    strncmp((char *) &header.fmt_subchunkid, "fmt ", 4) ||
	    (letoh16(header.fmt_bitspersample) != 8 &&
	     letoh16(header.fmt_bitspersample) != 16))
		return -1;

	/*
	 * Seek to the data chunk.
	 */
	for (datatag = 0; datatag < 4; ) {
		if (read(fd, &c, 1) != 1) {
			warn("read");
			return -1;
		}

		switch(datatag) {
		case 0:
			if (c == 'd')
				++datatag;
			break;
		case 1:
			if (c == 'a')
				++datatag;
			break;
		case 2:
			if (c == 't')
				++datatag;
			break;
		case 3:
			if (c == 'a')
				++datatag;
			break;
		default:
			datatag = 0;
			break;
		}
	}
	if (datatag != 4) {
		warnx("no data chunk found in wave file");
		return -1;
	}

	/*
	 * Ignore the size of the data chunk.
	 */
	if (lseek(fd, 4, SEEK_CUR) == -1) {
		warn("lseek");
		return -1;
	}

	audioinfo->play.sample_rate = letoh32(header.fmt_samplespersec);
	audioinfo->play.channels    = letoh16(header.fmt_channels);
	audioinfo->play.precision   = letoh16(header.fmt_bitspersample);
	audioinfo->play.encoding    = audioinfo->play.precision == 8 ?
	    AUDIO_ENCODING_ULINEAR : AUDIO_ENCODING_SLINEAR_LE;
	return 0;
}

int
main(int argc, char *argv[])
{
	int fd, ch;
	u_int32_t data;
	char magic[4];
	char *dev;
	audio_info_t ai;
	audio_info_t ai_defaults;

	dev = getenv("AUDIODEVICE");
	if (dev == NULL)
		dev = _PATH_AUDIO;

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch (ch) {
		case 'f':
			dev = optarg;
			break;
		default:
			usage();
			/* NOTREACHED */
		}
	}
	argc -= optind;
	argv += optind;

	if (argc == 0)
		usage();

	if ((afd = open(dev, O_WRONLY)) < 0)
		err(1, "can't open %s", dev);

	if (ioctl(afd, AUDIO_GETINFO, &ai_defaults) == -1)
		err(1, "AUDIO_GETINFO");

	while (argc) {
		if ((fd = open(*argv, O_RDONLY)) < 0)
			err(1, "cannot open %s", *argv);

		AUDIO_INITINFO(&ai);

		ai.play.sample_rate = ai_defaults.play.sample_rate;
		ai.play.channels    = ai_defaults.play.channels;
		ai.play.encoding    = ai_defaults.play.encoding;
		ai.play.precision   = ai_defaults.play.precision;

		if (read(fd, magic, sizeof(magic)) != sizeof(magic) ||
		    strncmp(magic, ".snd", 4)) {
			/*
			 * not an .au file, bad header.
			 * Check if it could be a .wav file and set
			 * the playback parameters in ai.
			 */
			if (strncmp(magic, "RIFF", 4) ||
			    readwaveheader(fd, &ai)) {
				/*
				 * Assume raw audio data since that's
				 * what /dev/audio generates by default.
				 */
				if (lseek(fd, 0, SEEK_SET) == -1)
					warn("lseek");
			}
		} else {
			if (read(fd, &data, sizeof(data)) == sizeof(data)) {
				data = ntohl(data);
				if (lseek(fd, (off_t)data, SEEK_SET) == -1)
					warn("lseek");
			}
		}

		if (playfile(fd, &ai) < 0)
			exit(1);
		(void) close(fd);
		argc--;
		argv++;
	}
	exit(0);
}

__dead void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-f device] file ...\n", __progname);
	exit(1);
}
