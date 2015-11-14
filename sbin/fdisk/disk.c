/*	$OpenBSD: disk.c,v 1.51 2015/11/14 21:17:08 krw Exp $	*/

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
#include <errno.h>
#include <util.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "disk.h"
#include "misc.h"

struct disk disk;
struct disklabel dl;

void
DISK_open(void)
{
	struct stat st;
	u_int64_t sz, spc;

	disk.fd = opendev(disk.name, O_RDWR, OPENDEV_PART, NULL);
	if (disk.fd == -1)
		err(1, "%s", disk.name);
	if (fstat(disk.fd, &st) == -1)
		err(1, "%s", disk.name);
	if (!S_ISCHR(st.st_mode) && !S_ISREG(st.st_mode))
		errx(1, "%s is not a character device or a regular file",
		    disk);

	/* Get label geometry. */
	if (ioctl(disk.fd, DIOCGPDINFO, &dl) == -1) {
		warn("DIOCGPDINFO");
	} else {
		unit_types[SECTORS].conversion = dl.d_secsize;
		if (disk.size == 0) {
			/* -l or -c/-h/-s not used. Use disklabel info. */
			disk.cylinders = dl.d_ncylinders;
			disk.heads = dl.d_ntracks;
			disk.sectors = dl.d_nsectors;
			/* MBR handles only first UINT32_MAX sectors. */
			spc = (u_int64_t)disk.heads * disk.sectors;
			sz = DL_GETDSIZE(&dl);
			if (sz > UINT32_MAX) {
				disk.cylinders = UINT32_MAX / spc;
				disk.size = disk.cylinders * spc;
			} else
				disk.size = sz;
		}
	}

	if (disk.size == 0 || disk.cylinders == 0 || disk.heads == 0 ||
	    disk.sectors == 0 || unit_types[SECTORS].conversion == 0)
		errx(1, "Can't get disk geometry, please use [-chs] or [-l]"
		    "to specify.");
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

/*
 * Read the sector at 'where' from the file descriptor 'fd' into newly
 * calloc'd memory. Return a pointer to the memory if it contains the
 * requested data, or NULL if it does not.
 *
 * The caller must free() the memory it gets.
 */
char *
DISK_readsector(off_t where)
{
	int secsize;
	char *secbuf;
	ssize_t len;
	off_t off;

	secsize = dl.d_secsize;

	where *= secsize;
	off = lseek(disk.fd, where, SEEK_SET);
	if (off != where)
		return (NULL);

	secbuf = calloc(1, secsize);
	if (secbuf == NULL)
		return (NULL);

	len = read(disk.fd, secbuf, secsize);
	if (len == -1 || len != secsize) {
		free(secbuf);
		return (NULL);
	}

	return (secbuf);
}

/*
 * Write the sector-sized 'secbuf' to the sector 'where' on the file
 * descriptor 'fd'. Return 0 if the write works. Return -1 and set
 * errno if the write fails.
 */
int
DISK_writesector(char *secbuf, off_t where)
{
	int secsize;
	ssize_t len;
	off_t off;

	len = -1;
	secsize = dl.d_secsize;

	where *= secsize;
	off = lseek(disk.fd, where, SEEK_SET);
	if (off == where)
		len = write(disk.fd, secbuf, secsize);

	if (len == -1 || len != secsize) {
		/* short read or write */
		errno = EIO;
		return (-1);
	}

	return (0);
}
