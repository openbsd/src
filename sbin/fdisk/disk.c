/*	$OpenBSD: disk.c,v 1.67 2021/07/17 14:16:34 krw Exp $	*/

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

#include <sys/param.h>		/* DEV_BSIZE */
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
#include <string.h>
#include <unistd.h>
#include <util.h>

#include "part.h"
#include "disk.h"
#include "misc.h"

struct disk		disk;
struct disklabel	dl;

void
DISK_open(const char *name, const int oflags)
{
	struct stat		st;
	uint64_t		ns, bs, sz, spc;

	disk.dk_name = strdup(name);
	if (disk.dk_name == NULL)
		err(1, "DISK_Open('%s')", name);

	disk.dk_fd = opendev(disk.dk_name, oflags, OPENDEV_PART, NULL);
	if (disk.dk_fd == -1)
		err(1, "%s", disk.dk_name);
	if (fstat(disk.dk_fd, &st) == -1)
		err(1, "%s", disk.dk_name);
	if (!S_ISCHR(st.st_mode))
		errx(1, "%s is not a character device", disk.dk_name);
	if (ioctl(disk.dk_fd, DIOCGPDINFO, &dl) == -1)
		err(1, "DIOCGPDINFO");

	unit_types[SECTORS].ut_conversion = dl.d_secsize;

	/* Set geometry to use in MBR partitions. */
	if (disk.dk_size > 0) {
		/* -l has set disk size. */
		sz = DL_BLKTOSEC(&dl, disk.dk_size);
		disk.dk_heads = 1;
		disk.dk_sectors = 64;
	} else if (disk.dk_cylinders > 0) {
		/* -c/-h/-c has set disk geometry. */
		sz = disk.dk_cylinders * disk.dk_heads * disk.dk_sectors;
		sz = DL_BLKTOSEC(&dl, sz);
		disk.dk_sectors = DL_BLKTOSEC(&dl, disk.dk_sectors);
	} else {
		sz = DL_GETDSIZE(&dl);
		disk.dk_heads = dl.d_ntracks;
		disk.dk_sectors = dl.d_nsectors;
	}

	if (sz > UINT32_MAX)
		sz = UINT32_MAX;	/* MBR knows nothing > UINT32_MAX. */

	spc = disk.dk_heads * disk.dk_sectors;
	disk.dk_cylinders = sz / spc;
	disk.dk_size = disk.dk_cylinders * spc;

	if (disk.dk_size == 0)
		errx(1, "dk_size == 0");

	if (disk.dk_bootprt.prt_ns > 0) {
		ns = disk.dk_bootprt.prt_ns + DL_BLKSPERSEC(&dl) - 1;
		bs = disk.dk_bootprt.prt_bs + DL_BLKSPERSEC(&dl) - 1;
		disk.dk_bootprt.prt_ns = DL_BLKTOSEC(&dl, ns);
		disk.dk_bootprt.prt_bs = DL_BLKTOSEC(&dl, bs);
	}
}

void
DISK_printgeometry(const char *units)
{
	const int		secsize = unit_types[SECTORS].ut_conversion;
	double			size;
	int			i;

	i = unit_lookup(units);
	size = ((double)disk.dk_size * secsize) / unit_types[i].ut_conversion;
	printf("Disk: %s\t", disk.dk_name);
	if (disk.dk_size) {
		printf("geometry: %d/%d/%d [%.0f ", disk.dk_cylinders,
		    disk.dk_heads, disk.dk_sectors, size);
		if (i == SECTORS && secsize != sizeof(struct dos_mbr))
			printf("%d-byte ", secsize);
		printf("%s]\n", unit_types[i].ut_lname);
	} else
		printf("geometry: <none>\n");
}

/*
 * The caller must free() the returned memory!
 */
char *
DISK_readsector(const uint64_t sector)
{
	char			*secbuf;
	ssize_t			 len;
	off_t			 off, where;
	int			 secsize;

	secsize = dl.d_secsize;

	where = sector * secsize;
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

int
DISK_writesector(const char *secbuf, const uint64_t sector)
{
	int			secsize;
	ssize_t			len;
	off_t			off, where;

	len = -1;
	secsize = dl.d_secsize;

	where = secsize * sector;
	off = lseek(disk.dk_fd, where, SEEK_SET);
	if (off == where)
		len = write(disk.dk_fd, secbuf, secsize);

	if (len == -1 || len != secsize) {
		errno = EIO;
		return -1;
	}

	return 0;
}
