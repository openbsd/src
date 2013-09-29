/*	$OpenBSD: installboot.c,v 1.1 2013/09/29 17:51:34 miod Exp $	*/

/*
 * Copyright (c) 2013 Miodrag Vallat.
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

#include <unistd.h>
#include <err.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>
#include <sys/disklabel.h>
#include <sys/stat.h>

int	nowrite, verbose;
char	*boot, *dev;

void	analyze_label_sector(uint8_t *, struct vdm_label **,
	    struct vdm_boot_info **);
void	initialize_boot_area(int);
void	read_sector(int, uint32_t, void *);
void	usage(void);
void	write_sector(int, uint32_t, void *);

#define	VDM_BLOCK_SIZE	0x200

uint8_t buf[VDM_BLOCK_SIZE];

void
usage()
{
	fprintf(stderr, "usage: installboot [-n] <boot> <device>\n");
	exit(1);
}

int
main(int argc, char *argv[])
{
	int c;
	int bootfd, devfd;
	size_t chunk;
	uint32_t blkno;
	struct stat stat;
	struct vdm_label *dl;
	struct vdm_boot_info *bi;

	while ((c = getopt(argc, argv, "nv")) != -1) {
		switch (c) {
		case 'n':
			nowrite = 1;
			break;
		case 'v':
			verbose = 1;
			break;
		default:
			usage();
		}
	}

	if (argc - optind != 2)
		usage();

	boot = argv[optind];
	dev = argv[optind + 1];

	bootfd = open(boot, O_RDONLY);
	if (bootfd < 0)
		err(1, "open(%s)", boot);

	if (fstat(bootfd, &stat) != 0)
		err(1, "fstat(%s)", boot);

	devfd = opendev(dev, nowrite ? O_RDONLY : O_RDWR, OPENDEV_PART, &dev);
	if (devfd < 0)
		err(1, "open(%s)", dev);

	/*
	 * Figure out the boot area span.
	 * If there is no VDM boot area, set up one.
	 */

	read_sector(devfd, VDM_LABEL_SECTOR, buf);
	analyze_label_sector(buf, &dl, &bi);

	if (bi == NULL) {
		if (verbose)
			printf("no boot area found\n");
		if (nowrite)
			return 0;
		if (verbose)
			printf("creating boot area\n");
		initialize_boot_area(devfd);

		read_sector(devfd, VDM_LABEL_SECTOR, buf);
		analyze_label_sector(buf, &dl, &bi);
	}

	if (bi != NULL) {
		if (verbose)
			printf("boot area: sectors %u-%u\n",
			   bi->boot_start, bi->boot_start + bi->boot_size - 1);
	} else {
		/* should not happen */
		return 1;
	}

	/*
	 * Write the file into the boot area.
	 */

	if (stat.st_size > bi->boot_size * VDM_BLOCK_SIZE)
		err(1, "boot file too large, boot area is only %u bytes",
		    bi->boot_size * VDM_BLOCK_SIZE);

	if (nowrite)
		return 0;

	blkno = bi->boot_start;
	if (verbose)
		printf("writing %lld bytes from sector %u onwards\n",
		    stat.st_size, blkno);
	while (stat.st_size != 0) {
		if (stat.st_size > sizeof buf)
			chunk = sizeof buf;
		else {
			chunk = stat.st_size;
			memset(buf, 0, sizeof buf);
		}
		if (read(bootfd, buf, chunk) != chunk)
			err(1, "read");
		write_sector(devfd, blkno++, buf);
		stat.st_size -= chunk;
	}

	close(bootfd);
	close(devfd);
	return 0;
}

void
read_sector(int fd, uint32_t secno, void *buf)
{
	if (lseek(fd, (off_t)secno * VDM_BLOCK_SIZE, SEEK_SET) == -1)
		err(1, "lseek");

	if (read(fd, buf, VDM_BLOCK_SIZE) != VDM_BLOCK_SIZE)
		err(1, "read(%d,%08x)", fd, secno);
}

void
write_sector(int fd, uint32_t secno, void *buf)
{
	if (lseek(fd, (off_t)secno * VDM_BLOCK_SIZE, SEEK_SET) == -1)
		err(1, "lseek");

	if (write(fd, buf, VDM_BLOCK_SIZE) != VDM_BLOCK_SIZE)
		err(1, "write(%d,%08x)", fd, secno);
}

void
analyze_label_sector(uint8_t *sector, struct vdm_label **dl,
    struct vdm_boot_info **dbi)
{
	struct vdm_label *l;
	struct vdm_boot_info *bi;

	l = (struct vdm_label *)(sector + VDM_LABEL_OFFSET);
	if (betoh32(l->signature) != VDM_LABEL_SIGNATURE) {
		l = (struct vdm_label *)(sector + VDM_LABEL_OFFSET_ALT);
		if (betoh32(l->signature) != VDM_LABEL_SIGNATURE)
			l = NULL;
	}

	if (l != NULL) {
		bi = (struct vdm_boot_info *)
		    (sector + VDM_BLOCK_SIZE - sizeof *bi);
		if (betoh32(bi->signature) != VDM_LABEL_SIGNATURE)
			bi = NULL;
	} else
		bi = NULL;

	*dl = l;
	*dbi = bi;
}

/*
 * Build a minimal VDM label and boot area.
 * Allows you to shoot yourself in the foot, badly.
 */
void
initialize_boot_area(int fd)
{
	struct vdm_label dl;
	struct vdm_boot_info bi;

	memset(buf, 0, sizeof buf);
	memset(&dl, 0, sizeof dl);
	memset(&bi, 0, sizeof bi);

	dl.signature = htobe32(VDM_LABEL_SIGNATURE);
	bi.signature = htobe32(VDM_LABEL_SIGNATURE);
	bi.boot_start = htobe32(LABELSECTOR + 1);
	bi.boot_size = htobe32(VDM_BOOT_DEFAULT_SIZE);
	bi.version = htobe32(VDM_BOOT_INFO_VERSION);

	memcpy(buf + VDM_LABEL_OFFSET_ALT, &dl, sizeof dl);
	memcpy(buf + VDM_BLOCK_SIZE - sizeof bi, &bi, sizeof bi);

	write_sector(fd, VDM_LABEL_SECTOR, buf);
}
