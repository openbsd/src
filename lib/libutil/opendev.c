/*	$OpenBSD: opendev.c,v 1.10 2010/06/28 19:12:29 chl Exp $	*/

/*
 * Copyright (c) 2000, Todd C. Miller.  All rights reserved.
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

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <paths.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <sys/limits.h>
#include <sys/disk.h>
#include <sys/dkio.h>

#include "util.h"

/* Returns 1 if a valid disklabel UID.  */
static int
valid_diskuid(const char *duid, int dflags)
{
	char c;
	int i;

	/* Basic format check. */
	if (!((strlen(duid) == 16 && (dflags & OPENDEV_PART)) ||
	    (strlen(duid) == 18 && duid[16] == '.')))
		return 0;

	/* Check UID. */
	for (i = 0; i < 16; i++) {
		c = duid[i];
		if ((c < '0' || c > '9') && (c < 'a' || c > 'f'))
			return 0;
	}

	return 1;
}

/*
 * This routine is a generic rewrite of the original code found in
 * disklabel(8).
 */
int
opendev(char *path, int oflags, int dflags, char **realpath)
{
	static char namebuf[PATH_MAX];
	struct dk_diskmap dm;
	char *slash, *prefix;
	int fd;

	/* Initial state */
	if (realpath)
		*realpath = path;
	fd = -1;
	errno = ENOENT;

	if (dflags & OPENDEV_BLCK)
		prefix = "";			/* block device */
	else
		prefix = "r";			/* character device */

	if ((slash = strchr(path, '/')))
		fd = open(path, oflags);
	else if (valid_diskuid(path, dflags)) {
		if ((fd = open("/dev/diskmap", oflags)) != -1) {
			bzero(&dm, sizeof(struct dk_diskmap));
			strlcpy(namebuf, path, sizeof(namebuf));
			dm.device = namebuf;
			dm.fd = fd;
			if (dflags & OPENDEV_PART)
				dm.flags |= DM_OPENPART;
			if (dflags & OPENDEV_BLCK)
				dm.flags |= DM_OPENBLCK;

			if (ioctl(fd, DIOCMAP, &dm) == -1) {
				close(fd);
				fd = -1;
				errno = ENOENT;
			} else if (realpath)
				*realpath = namebuf;
		} else if (errno != ENOENT) {
			errno = ENXIO;
			return -1;
		}
	}
	if (fd == -1 && errno == ENOENT && (dflags & OPENDEV_PART)) {
		/*
		 * First try raw partition (for removable drives)
		 */
		if (snprintf(namebuf, sizeof(namebuf), "%s%s%s%c",
		    _PATH_DEV, prefix, path, 'a' + getrawpartition())
		    < sizeof(namebuf)) {
			fd = open(namebuf, oflags);
			if (realpath)
				*realpath = namebuf;
		} else
			errno = ENAMETOOLONG;
	}
	if (!slash && fd == -1 && errno == ENOENT) {
		if (snprintf(namebuf, sizeof(namebuf), "%s%s%s",
		    _PATH_DEV, prefix, path) < sizeof(namebuf)) {
			fd = open(namebuf, oflags);
			if (realpath)
				*realpath = namebuf;
		} else
			errno = ENAMETOOLONG;
	}
	return (fd);
}
