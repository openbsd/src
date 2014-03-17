/*	$OpenBSD: disk.c,v 1.40 2014/03/17 16:40:00 krw Exp $	*/

/*
 * Copyright (c) 1997, 2001 Tobias Weingartner
 * All rights reserved.
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
#include <sys/fcntl.h>
#include <sys/dkio.h>
#include <sys/stdint.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <err.h>
#include <util.h>
#include <stdio.h>
#include <stdlib.h>

#include "disk.h"
#include "misc.h"

struct disklabel dl;

int
DISK_open(char *disk, int mode)
{
	int fd;
	struct stat st;

	fd = opendev(disk, mode, OPENDEV_PART, NULL);
	if (fd == -1)
		err(1, "%s", disk);
	if (fstat(fd, &st) == -1)
		err(1, "%s", disk);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "%s is not a character device or a regular file", disk);
	return (fd);
}

void
DISK_getlabelgeometry(struct disk *disk)
{
	u_int64_t sz, spc;
	int fd;

	/* Get label geometry. */
	if ((fd = DISK_open(disk->name, O_RDONLY)) != -1) {
		if (ioctl(fd, DIOCGPDINFO, &dl) == -1) {
			warn("DIOCGPDINFO");
		} else {
			disk->cylinders = dl.d_ncylinders;
			disk->heads = dl.d_ntracks;
			disk->sectors = dl.d_nsectors;
			/* MBR handles only first UINT32_MAX sectors. */
			spc = (u_int64_t)disk->heads * disk->sectors;
			sz = DL_GETDSIZE(&dl);
			if (sz > UINT32_MAX) {
				disk->cylinders = UINT32_MAX / spc;
				disk->size = disk->cylinders * spc;
				warnx("disk too large (%llu sectors)."
				    " size truncated.", sz);
			} else
				disk->size = sz;
			unit_types[SECTORS].conversion = dl.d_secsize;
		}
		close(fd);
	}
}

/*
 * Print the disk geometry information. Take an optional modifier
 * to indicate the units that should be used for display.
 */
int
DISK_printgeometry(struct disk *disk, char *units)
{
	const int secsize = unit_types[SECTORS].conversion;
	double size;
	int i;

	i = unit_lookup(units);
	size = ((double)disk->size * secsize) / unit_types[i].conversion;
	printf("Disk: %s\t", disk->name);
	if (disk->size) {
		printf("geometry: %d/%d/%d [%.0f ", disk->cylinders,
		    disk->heads, disk->sectors, size);
		if (i == SECTORS && secsize != sizeof(struct dos_mbr))
			printf("%d-byte ", secsize);
		printf("%s]\n", unit_types[i].lname);
	} else
		printf("geometry: <none>\n");

	return (0);
}

