/*	$OpenBSD: disk.c,v 1.46 2015/03/27 16:06:00 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/stdint.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <err.h>
#include <util.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "disk.h"
#include "misc.h"

struct disk disk;
struct disklabel dl;

int
DISK_open(char *disk, int mode)
{
	struct stat st;
	int fd;

	fd = opendev(disk, mode, OPENDEV_PART, NULL);
	if (fd == -1)
		err(1, "%s", disk);
	if (fstat(fd, &st) == -1)
		err(1, "%s", disk);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "%s is not a character device or a regular file",
		    disk);

	return (fd);
}

void
DISK_getlabelgeometry(void)
{
	u_int64_t sz, spc;
	int fd;

	/* Get label geometry. */
	if ((fd = DISK_open(disk.name, O_RDONLY)) != -1) {
		if (ioctl(fd, DIOCGPDINFO, &dl) == -1) {
			warn("DIOCGPDINFO");
		} else {
			disk.cylinders = dl.d_ncylinders;
			disk.heads = dl.d_ntracks;
			disk.sectors = dl.d_nsectors;
			/* MBR handles only first UINT32_MAX sectors. */
			spc = (u_int64_t)disk.heads * disk.sectors;
			sz = DL_GETDSIZE(&dl);
			if (sz > UINT32_MAX) {
				disk.cylinders = UINT32_MAX / spc;
				disk.size = disk.cylinders * spc;
				warnx("disk too large (%llu sectors)."
				    " size truncated.", sz);
			} else
				disk.size = sz;
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
DISK_printgeometry(char *units)
{
	const int secsize = unit_types[SECTORS].conversion;
	double size;
	int i;

	i = unit_lookup(units);
	size = ((double)disk.size * secsize) / unit_types[i].conversion;
	printf("Disk: %s\t", disk.name);
	if (disk.size) {
		printf("geometry: %d/%d/%d [%.0f ", disk.cylinders,
		    disk.heads, disk.sectors, size);
		if (i == SECTORS && secsize != sizeof(struct dos_mbr))
			printf("%d-byte ", secsize);
		printf("%s]\n", unit_types[i].lname);
	} else
		printf("geometry: <none>\n");

	return (0);
}
