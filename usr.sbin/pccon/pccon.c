/*	$OpenBSD: pccon.c,v 1.2 2000/06/30 16:00:29 millert Exp $	*/

/*
 * Copyright (c) 1997, Jason Downs.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR(S) ``AS IS'' AND ANY EXPRESS
 * OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR(S) BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <sys/types.h>
#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <err.h>

#include <i386/pccons.h>

extern char *__progname;

int main(argc, argv)
	int argc;
	char *argv[];
{
	int ch, fd, blank;
	char *ep, *dev = _PATH_CONSOLE;

	blank = -1;
	while ((ch = getopt(argc, argv, "b:f:")) != -1) {
		switch (ch) {
		case 'b':
			blank = strtol(optarg, &ep, 10);
			if (ep == optarg)
				errx (1, "numeric argument expected");
			if (blank < 0)
				errx (1, "illegal blank value");
			break;

		case 'f':
			dev = optarg;
			break;

		default:
			fprintf (stderr,
				"usage: %s [-f device] [-b interval]\n",
				__progname);
			exit (1);
		}
	}

	fd = open (dev, O_RDWR);
	if (fd < 0)
		err (1, "%s", dev);

	if (blank >= 0) {
		if (ioctl(fd, CONSOLE_SET_BLANK, &blank) < 0)
			err (1, "ioctl");
	}

	close (fd);
	exit (0);
}
