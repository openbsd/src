/*	$OpenBSD: aucat.c,v 1.10 2005/01/13 19:19:44 ian Exp $	*/
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

/* 
 * aucat: concatenate and play Sun 8-bit .au files
 */

int	playfile(int, char *);

/* function playfile: given a file which is positioned at the beginning
 * of what is assumed to be an .au data stream copy it out to the audio
 * device.  Return 0 on success, -1 on failure.
 */
int
playfile(int fd, char *dev)
{
	static int afd = -1;
	int rd;
	char buf[5120];

	if (afd == -1 && (afd = open(dev, O_WRONLY)) < 0) {
		warn("can't open %s", dev);
		return(-1);
	}
	while ((rd = read(fd, buf, sizeof(buf))) > 0) {
		write(afd, buf, rd);
		if (rd < sizeof(buf))
			break;
	}

	return (0);
}

int
main(int argc, char *argv[])
{
	int fd, ch;
	unsigned long data;
	char magic[5];
	char *dev;

	dev = getenv("AUDIODEVICE");
	if (dev == 0)
		dev = "/dev/audio";

	while ((ch = getopt(argc, argv, "f:")) != -1) {
		switch(ch) {
		case 'f':
			dev = optarg;
			break;
		}
	}
	argc -= optind;
	argv += optind;

	while (argc) {
		if ((fd = open(*argv, O_RDONLY)) < 0)
			err(1, "cannot open %s", *argv);

		read(fd, magic, 4);
		magic[4] = '\0';
		if (strcmp(magic, ".snd")) {
			/*
			 * not an .au file, bad header.
			 * Assume raw audio data since that's
			 * what /dev/audio generates by default.
			 */
			lseek(fd, 0, SEEK_SET);
		} else {
			read(fd, &data, sizeof(data));
			data = ntohl(data);
			lseek(fd, (off_t)data, SEEK_SET);
		}
		if (playfile(fd, dev) < 0)
			exit(1);
		(void) close(fd);
		argc--;
		argv++;
	}
	exit(0);
}
