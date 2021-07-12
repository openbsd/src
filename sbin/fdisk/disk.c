/*	$OpenBSD: disk.c,v 1.60 2021/07/12 14:06:19 krw Exp $	*/

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
#include <sys/ioctl.h>
#include <sys/dkio.h>
#include <sys/stat.h>
#include <sys/disklabel.h>

#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <util.h>

#include "disk.h"
#include "misc.h"

struct disk		disk;
struct disklabel	dl;

void
DISK_open(int rw)
{
	struct stat		st;
	uint64_t		sz, spc;

	disk.dk_fd = opendev(disk.dk_name, rw ? O_RDWR : O_RDONLY, OPENDEV_PART,
	    NULL);
	if (disk.dk_fd == -1)
		err(1, "%s", disk.dk_name);
	if (fstat(disk.dk_fd, &st) == -1)
		err(1, "%s", disk.dk_name);
	if (!S_ISCHR(st.st_mode))
		errx(1, "%s is not a character device", disk.dk_name);

	/* Get label geometry. */
	if (ioctl(disk.dk_fd, DIOCGPDINFO, &dl) == -1) {
		warn("DIOCGPDINFO");
	} else {
		unit_types[SECTORS].conversion = dl.d_secsize;
		if (disk.dk_size == 0) {
			/* -l or -c/-h/-s not used. Use disklabel info. */
			disk.dk_cylinders = dl.d_ncylinders;
			disk.dk_heads = dl.d_ntracks;
			disk.dk_sectors = dl.d_nsectors;
			/* MBR handles only first UINT32_MAX sectors. */
			spc = (uint64_t)disk.dk_heads * disk.dk_sectors;
			sz = DL_GETDSIZE(&dl);
			if (sz > UINT32_MAX) {
				disk.dk_cylinders = UINT32_MAX / spc;
				disk.dk_size = disk.dk_cylinders * spc;
			} else
				disk.dk_size = sz;
		}
	}

	if (disk.dk_size == 0 || disk.dk_cylinders == 0 || disk.dk_heads == 0 ||
	    disk.dk_sectors == 0 || unit_types[SECTORS].conversion == 0)
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
	const int		secsize = unit_types[SECTORS].conversion;
	double			size;
	int			i;

	i = unit_lookup(units);
	size = ((double)disk.dk_size * secsize) / unit_types[i].conversion;
	printf("Disk: %s\t", disk.dk_name);
	if (disk.dk_size) {
		printf("geometry: %d/%d/%d [%.0f ", disk.dk_cylinders,
		    disk.dk_heads, disk.dk_sectors, size);
		if (i == SECTORS && secsize != sizeof(struct dos_mbr))
			printf("%d-byte ", secsize);
		printf("%s]\n", unit_types[i].lname);
	} else
		printf("geometry: <none>\n");

	return 0;
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
	char			*secbuf;
	ssize_t			 len;
	off_t			 off;
	int			 secsize;

	secsize = dl.d_secsize;

	where *= secsize;
	off = lseek(disk.dk_fd, where, SEEK_SET);
	if (off != where)
		return NULL;

	secbuf = calloc(1, secsize);
	if (secbuf == NULL)
		return NULL;

	len = read(disk.dk_fd, secbuf, secsize);
	if (len == -1 || len != secsize) {
		free(secbuf);
		return NULL;
	}

	return secbuf;
}

/*
 * Write the sector-sized 'secbuf' to the sector 'where' on the file
 * descriptor 'fd'. Return 0 if the write works. Return -1 and set
 * errno if the write fails.
 */
int
DISK_writesector(char *secbuf, off_t where)
{
	int			secsize;
	ssize_t			len;
	off_t			off;

	len = -1;
	secsize = dl.d_secsize;

	where *= secsize;
	off = lseek(disk.dk_fd, where, SEEK_SET);
	if (off == where)
		len = write(disk.dk_fd, secbuf, secsize);

	if (len == -1 || len != secsize) {
		/* short read or write */
		errno = EIO;
		return -1;
	}

	return 0;
}
