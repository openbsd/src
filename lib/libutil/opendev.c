/*	$OpenBSD: opendev.c,v 1.1 1996/06/17 06:35:34 downsj Exp $ */

/*
 * Copyright (c) 1996, Jason Downs.  All rights reserved.
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

#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/disklabel.h>
#include <paths.h>

#include "opendev.h"

/*
 * This routine is a generic rewrite of the original code found in
 * disklabel(8).
 */

int
opendev(path, oflags, dflags, mode, realpath)
	char *path;
	int oflags;
	int dflags;
	mode_t mode;
	char **realpath;
{
	int fd;
	static char namebuf[256];

	*realpath = path;

	fd = open(path, oflags, mode);
	if ((fd < 0) && (errno == ENOENT)) {
		if (path[0] != '/') {
			if (dflags & OPENDEV_PART) {
				/*
				 * First try raw partition (for removable
				 * drives)
				 */
				(void)snprintf(namebuf, sizeof(namebuf),
				    "%sr%s%c", _PATH_DEV, path, 'a' + RAW_PART);
				fd = open(namebuf, oflags, mode);
			}

			if ((dflags & OPENDEV_DRCT) && (fd < 0) &&
			    (errno == ENOENT)) {
				/* ..and now no partition (for tapes) */
				namebuf[strlen(namebuf) - 1] = '\0';
				fd = open(namebuf, oflags, mode);
			}

			*realpath = namebuf;
		}
	}
	if ((fd < 0) && (errno == ENOENT) && (path[0] != '/')) {
		(void)snprintf(namebuf, sizeof(namebuf), "%sr%s",
		    _PATH_DEV, path);
		fd = open(namebuf, oflags, mode);

		*realpath = namebuf;
	}

	return (fd);
}
