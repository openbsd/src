/*	$OpenBSD: aucat.c,v 1.11 2005/01/20 04:21:42 jaredy Exp $	*/
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
#include <machine/endian.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <err.h>

#define _PATH_AUDIO "/dev/audio"

/* 
 * aucat: concatenate and play Sun 8-bit .au files
 */

int	playfile(int, char *);
void	usage(void) __attribute__((__noreturn__));

/*
 * function playfile: given a file which is positioned at the beginning
 * of what is assumed to be an .au data stream copy it out to the audio
 * device.  Return 0 on success, -1 on failure.
 */
int
playfile(int fd, char *dev)
{
	static int afd = -1;
	ssize_t rd;
	char buf[5120];

	if (afd == -1 && (afd = open(dev, O_WRONLY)) < 0) {
		warn("can't open %s", dev);
		return(-1);
	}
	while ((rd = read(fd, buf, sizeof(buf))) > 0)
		if (write(afd, buf, rd) != rd)
			warn("write");
	if (rd == -1)
		warn("read");

	return (0);
}

int
main(int argc, char *argv[])
{
	int fd, ch;
	u_int32_t data;
	char magic[4];
	char *dev;

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
	while (argc) {
		if ((fd = open(*argv, O_RDONLY)) < 0)
			err(1, "cannot open %s", *argv);

		if (read(fd, magic, sizeof(magic)) != sizeof(magic) ||
		    strncmp(magic, ".snd", 4)) {
			/*
			 * not an .au file, bad header.
			 * Assume raw audio data since that's
			 * what /dev/audio generates by default.
			 */
			if (lseek(fd, 0, SEEK_SET) == -1)
				warn("lseek");
		} else {
			if (read(fd, &data, sizeof(data)) == sizeof(data)) {
				data = ntohl(data);
				if (lseek(fd, (off_t)data, SEEK_SET) == -1)
					warn("lseek");
			}
		}
		if (playfile(fd, dev) < 0)
			exit(1);
		(void) close(fd);
		argc--;
		argv++;
	}
	exit(0);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr, "usage: %s [-f device] file ...\n", __progname);
	exit(1);
}
