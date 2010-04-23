/*	$OpenBSD: sgivol.c,v 1.16 2010/04/23 15:25:20 jsing Exp $	*/
/*	$NetBSD: sgivol.c,v 1.8 2003/11/08 04:59:00 sekiya Exp $	*/

/*-
 * Copyright (c) 2001 The NetBSD Foundation, Inc.
 * All rights reserved.
 *
 * This code is derived from software contributed to The NetBSD Foundation
 * by Michael Hitch and Hubert Feyrer.
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
 * THIS SOFTWARE IS PROVIDED BY THE NETBSD FOUNDATION, INC. AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE FOUNDATION OR CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <util.h>

#include <sys/disklabel.h>
#include <sys/endian.h>
#include <sys/ioctl.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <sys/types.h>

/*
 * Some IRIX man pages refer to the size being a multiple of whole cylinders.
 * Later ones only refer to the size being "typically" 2MB.  IRIX fx(1)
 * uses a default drive geometry if one can't be determined, suggesting
 * that "whole cylinder" multiples are not required.
 */

#define SGI_SIZE_VOLHDR	3135	/* Can be overridden via -h parameter. */

/*
 * Mode of operation can be one of:
 * -i	Initialise volume header.
 * -r	Read a file from volume header.
 * -w	Write a file to volume header.
 * -l	Link a file into the volume header.
 * -d	Delete a file from the volume header.
 * -p	Modify a partition.
 */

char	mode;
int	quiet;
int	fd;
int	partno, partfirst, partblocks, parttype;
struct	sgilabel *volhdr;
int32_t	checksum;

/* Volume header size in sectors. */
u_int32_t volhdr_size = SGI_SIZE_VOLHDR;

const char *vfilename = "";
const char *ufilename = "";

struct disklabel lbl;

unsigned char *buf;
unsigned int bufsize;

const char *sgi_types[] = {
	"Volume Header",
	"Repl Trks",
	"Repl Secs",
	"Raw",
	"BSD4.2",
	"SysV",
	"Volume",
	"EFS",
	"LVol",
	"RLVol",
	"XFS",
	"XSFLog",
	"XLV",
	"XVM"
};

void	display_vol(void);
void	init_volhdr(void);
void	read_file(void);
void	write_file(void);
void	link_file(void);
void	delete_file(void);
void	modify_partition(void);
void	write_volhdr(void);
int	allocate_space(int);
void	checksum_vol(void);
void	usage(void);

int
main(int argc, char *argv[])
{
	int ch, oflags;
	char fname[FILENAME_MAX];
	char *endp;

	quiet = 0;
	mode = ' ';

	while ((ch = getopt(argc, argv, "irwlpdqfh:")) != -1) {
		switch (ch) {
		case 'q':
			quiet = 1;
			break;
		case 'f':
			/* Legacy. Do nothing. */
			break;
		case 'i':
			mode = 'i';
			break;
		case 'h':
			volhdr_size = strtol(optarg, &endp, 0);
			if (*endp != '\0' || errno != 0)
				errx(1, "incorrect volume header size: %s",
				    optarg);
			break;
		case 'r':
			mode = 'r';
			break;
		case 'w':
			mode = 'w';
			break;
		case 'l':
			mode = 'l';
			break;
		case 'd':
			mode = 'd';
			break;
		case 'p':
			mode = 'p';
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;

	if (mode == 'r' || mode == 'w' || mode == 'l') {
		if (argc != 3)
			usage();
		vfilename = argv[0];
		ufilename = argv[1];
		argc -= 2;
		argv += 2;
	} else if (mode == 'd') {
		if (argc != 2)
			usage();
		vfilename = argv[0];
		argc--;
		argv++;
	} else if (mode == 'p') {
		if (argc != 5)
			usage();
		partno = strtol(argv[0], &endp, 0);
		if (*endp != '\0' || errno != 0 ||
		    partno < 0 || partno > SGI_SIZE_VOLDIR)
			errx(1, "invalid partition number: %s", argv[0]);
		partfirst = strtol(argv[1], &endp, 0);
		if (*endp != '\0' || errno != 0)
			errx(1, "invalid partition start: %s", argv[1]);
		partblocks = strtol(argv[2], &endp, 0);
		if (*endp != '\0' || errno != 0)
			errx(1, "invalid partition size: %s", argv[2]);
		parttype = strtol(argv[3], &endp, 0);
		if (*endp != '\0' || errno != 0)
			errx(1, "invalid partition type: %s", argv[3]);
		argc -= 4;
		argv += 4;
	}
	if (argc != 1)
		usage();

	oflags = ((mode == 'i' || mode == 'w' || mode == 'l' || mode == 'd'
	    || mode == 'p') ? O_RDWR : O_RDONLY);

	/* Open raw device. */
	if ((fd = open(argv[0], oflags)) < 0) {
		snprintf(fname, sizeof(fname), "/dev/r%s%c",
		    argv[0], 'a' + getrawpartition());
		if ((fd = open(fname, oflags)) < 0)
			err(1, "open %s", fname);
	}

	/* Get disklabel for device. */
	if (ioctl(fd, DIOCGDINFO, &lbl) == -1)
		err(1, "ioctl DIOCGDINFO");

	/* Allocate a buffer that matches the device sector size. */
	bufsize = lbl.d_secsize;
	if (bufsize < sizeof(struct sgilabel))
		errx(1, "sector size is smaller than SGI volume header!\n");
	if ((buf = malloc(bufsize)) == NULL)
		err(1, "failed to allocate buffer");

	/* Read SGI volume header. */
	if (read(fd, buf, bufsize) != bufsize)
		err(1, "read volhdr");
	volhdr = (struct sgilabel *)buf;

	if (mode == 'i') {
		init_volhdr();
		exit(0);
	}

	if (betoh32(volhdr->magic) != SGILABEL_MAGIC)
		errx(2, "no Volume Header found, magic=%x.  Use -i first.",
		    betoh32(volhdr->magic));

	if (mode == 'r')
		read_file();
	else if (mode == 'w')
		write_file();
	else if (mode == 'l')
		link_file();
	else if (mode == 'd')
		delete_file();
	else if (mode == 'p')
		modify_partition();
	else if (!quiet)
		display_vol();

	exit (0);
}

void
display_vol(void)
{
	int32_t *l;
	int i;

	l = (int32_t *)buf;
	checksum = 0;
	for (i = 0; i < sizeof(struct sgilabel) / sizeof(int32_t); ++i)
		checksum += betoh32(l[i]);

	printf("disklabel shows %d sectors with %d bytes per sector\n",
	    lbl.d_secperunit, lbl.d_secsize);
	printf("checksum: %08x%s\n", checksum, checksum == 0 ? "" : " *ERROR*");
	printf("root part: %d\n", betoh32(volhdr->root));
	printf("swap part: %d\n", betoh32(volhdr->swap));
	printf("bootfile: %s\n", volhdr->bootfile);

	/* volhdr->devparams[0..47] */
	printf("\nVolume header files:\n");
	for (i = 0; i < SGI_SIZE_VOLDIR; ++i) {
		if (volhdr->voldir[i].name[0] != '\0') {
			printf("%-8s offset %4d blocks, "
			    "length %8d bytes (%d blocks)\n",
			    volhdr->voldir[i].name,
			    betoh32(volhdr->voldir[i].block),
			    betoh32(volhdr->voldir[i].bytes),
			    howmany(betoh32(volhdr->voldir[i].bytes),
			    DEV_BSIZE));
		}
	}

	printf("\nSGI partitions:\n");
	for (i = 0; i < MAXPARTITIONS; ++i) {
		if (betoh32(volhdr->partitions[i].blocks) != 0) {
			printf("%2d:%c blocks %8d first %8d type %2d (%s)\n",
			    i, i + 'a', betoh32(volhdr->partitions[i].blocks),
			    betoh32(volhdr->partitions[i].first),
			    betoh32(volhdr->partitions[i].type),
			    betoh32(volhdr->partitions[i].type) >
			    (sizeof(sgi_types) / sizeof(sgi_types[0])) ?
			    "???" :
			    sgi_types[betoh32(volhdr->partitions[i].type)]);
		}
	}
}

void
init_volhdr(void)
{
	memset(volhdr, 0, sizeof(struct sgilabel));
	volhdr->magic = htobe32(SGILABEL_MAGIC);
	volhdr->root = htobe16(0);
	volhdr->swap = htobe16(1);
	strlcpy(volhdr->bootfile, "/bsd", sizeof(volhdr->bootfile));
	volhdr->dp.dp_skew = 1; /* XXX */
	volhdr->dp.dp_gap1 = 1; /* XXX */
	volhdr->dp.dp_gap2 = 1; /* XXX */
	volhdr->dp.dp_cyls = htobe16(lbl.d_ncylinders);
	volhdr->dp.dp_shd0 = 0;
	volhdr->dp.dp_trks0 = htobe16(lbl.d_ntracks);
	volhdr->dp.dp_secs = htobe16(lbl.d_nsectors);
	volhdr->dp.dp_secbytes = htobe16(lbl.d_secsize);
	volhdr->dp.dp_interleave = 1;
	volhdr->dp.dp_nretries = htobe32(22);
	volhdr->partitions[10].blocks =
	    htobe32(DL_SECTOBLK(&lbl, lbl.d_secperunit));
	volhdr->partitions[10].first = 0;
	volhdr->partitions[10].type = htobe32(SGI_PTYPE_VOLUME);
	volhdr->partitions[8].blocks = htobe32(DL_SECTOBLK(&lbl, volhdr_size));
	volhdr->partitions[8].first = 0;
	volhdr->partitions[8].type = htobe32(SGI_PTYPE_VOLHDR);
	volhdr->partitions[0].blocks =
	    htobe32(DL_SECTOBLK(&lbl, lbl.d_secperunit - volhdr_size));
	volhdr->partitions[0].first = htobe32(DL_SECTOBLK(&lbl, volhdr_size));
	volhdr->partitions[0].type = htobe32(SGI_PTYPE_BSD);
	write_volhdr();
}

void
read_file(void)
{
	FILE *fp;
	int i;

	if (!quiet)
		printf("Reading file %s\n", vfilename);
	for (i = 0; i < SGI_SIZE_VOLDIR; ++i) {
		if (strncmp(vfilename, volhdr->voldir[i].name,
		    strlen(volhdr->voldir[i].name)) == 0)
			break;
	}
	if (i >= SGI_SIZE_VOLDIR)
		errx(1, "%s: file not found", vfilename);
	/* XXX assumes volume header starts at 0? */
	lseek(fd, betoh32(volhdr->voldir[i].block) * DEV_BSIZE, SEEK_SET);
	if ((fp = fopen(ufilename, "w")) == NULL)
		err(1, "open %s", ufilename);
	i = betoh32(volhdr->voldir[i].bytes);
	while (i > 0) {
		if (read(fd, buf, bufsize) != bufsize)
			err(1, "read file");
		fwrite(buf, 1, i > bufsize ? bufsize : i, fp);
		i -= i > bufsize ? bufsize : i;
	}
	fclose(fp);
}

void
write_file(void)
{
	FILE *fp;
	int slot;
	int block, i, fsize, fbufsize;
	struct stat st;
	char *fbuf;

	if (!quiet)
		printf("Writing file %s\n", ufilename);

	if (stat(ufilename, &st) != 0)
		err(1, "stat %s", ufilename);
	if (st.st_size == 0)
		errx(1, "%s: file is empty", vfilename);

	if (!quiet)
		printf("File %s has %lld bytes\n", ufilename, st.st_size);
	slot = -1;
	for (i = 0; i < SGI_SIZE_VOLDIR; ++i) {
		if (volhdr->voldir[i].name[0] == '\0' && slot < 0)
			slot = i;
		if (strcmp(vfilename, volhdr->voldir[i].name) == 0) {
			slot = i;
			break;
		}
	}
	if (slot == -1)
		errx(1, "no more directory entries available");
	if (betoh32(volhdr->voldir[slot].block) > 0) {
		if (!quiet)
			printf("File %s exists, removing old file\n",
			    vfilename);
		volhdr->voldir[slot].name[0] = 0;
		volhdr->voldir[slot].block = volhdr->voldir[slot].bytes = 0;
	}
	/* XXX assumes volume header starts at 0? */
	block = allocate_space((int)st.st_size);
	if (block < 0)
		errx(1, "no more space available");

	/*
	 * Make sure the name in the volume header is max. 8 chars,
	 * NOT including NUL.
	 */
	if (strlen(vfilename) > sizeof(volhdr->voldir[slot].name))
		warnx("%s: filename is too long and will be truncated",
		    vfilename);
	strncpy(volhdr->voldir[slot].name, vfilename,
	    sizeof(volhdr->voldir[slot].name));

	volhdr->voldir[slot].block = htobe32(block);
	volhdr->voldir[slot].bytes = htobe32(st.st_size);

	write_volhdr();

	/* Write the file itself. */
	if (lseek(fd, block * DEV_BSIZE, SEEK_SET) == -1)
		err(1, "lseek write");
	fbufsize = volhdr->dp.dp_secbytes;
	if ((fbuf = malloc(fbufsize)) == NULL)
		err(1, "failed to allocate buffer");
	i = st.st_size;
	fp = fopen(ufilename, "r");
	while (i > 0) {
		bzero(fbuf, fbufsize);
		fsize = i > fbufsize ? fbufsize : i;
		if (fread(fbuf, 1, fsize, fp) != fsize)
			err(1, "reading file from disk");
		if (write(fd, fbuf, fbufsize) != fbufsize)
			err(1, "writing file to SGI volume header");
		i -= fsize;
	}
	fclose(fp);
	free(fbuf);
}

void
link_file(void)
{
	int slot, i;
	int32_t block, bytes;

	if (!quiet)
		printf("Linking file %s to %s\n", vfilename, ufilename);
	for (i = 0; i < SGI_SIZE_VOLDIR; ++i) {
		if (strncmp(vfilename, volhdr->voldir[i].name,
		    strlen(volhdr->voldir[i].name)) == 0)
			break;
	}
	if (i >= SGI_SIZE_VOLDIR)
		errx(1, "%s: file not found", vfilename);

	block = volhdr->voldir[i].block;
	bytes = volhdr->voldir[i].bytes;

	slot = -1;
	for (i = 0; i < SGI_SIZE_VOLDIR; ++i) {
		if (volhdr->voldir[i].name[0] == '\0' && slot < 0)
			slot = i;
		if (strcmp(ufilename, volhdr->voldir[i].name) == 0) {
			slot = i;
			break;
		}
	}
	if (slot == -1)
		errx(1, "no more directory entries available");

	/*
	 * Make sure the name in the volume header is max. 8 chars,
	 * NOT including NUL.
	 */
	if (strlen(ufilename) > sizeof(volhdr->voldir[slot].name))
		warnx("%s: filename is too long and will be truncated",
		    ufilename);
	strncpy(volhdr->voldir[slot].name, ufilename,
	    sizeof(volhdr->voldir[slot].name));

	volhdr->voldir[slot].block = block;
	volhdr->voldir[slot].bytes = bytes;
	write_volhdr();
}

void
delete_file(void)
{
	int i;

	for (i = 0; i < SGI_SIZE_VOLDIR; ++i) {
		if (strcmp(vfilename, volhdr->voldir[i].name) == 0) {
			break;
		}
	}
	if (i >= SGI_SIZE_VOLDIR)
		errx(1, "%s: file not found", vfilename);

	/* XXX: we don't compact the file space, so get fragmentation */
	volhdr->voldir[i].name[0] = '\0';
	volhdr->voldir[i].block = volhdr->voldir[i].bytes = 0;
	write_volhdr();
}

void
modify_partition(void)
{
	if (!quiet)
		printf("Modify partition %d start %d length %d\n",
		    partno, partfirst, partblocks);
	volhdr->partitions[partno].blocks = htobe32(partblocks);
	volhdr->partitions[partno].first = htobe32(partfirst);
	volhdr->partitions[partno].type = htobe32(parttype);
	write_volhdr();
}

void
write_volhdr(void)
{
	checksum_vol();

	if (!quiet)
		display_vol();
	if (lseek(fd, 0, SEEK_SET) == -1)
		err(1, "lseek 0");
	if (write(fd, buf, sizeof(struct sgilabel)) != sizeof(struct sgilabel))
		err(1, "write volhdr");
}

int
allocate_space(int size)
{
	int n, blocks;
	int first;

	blocks = howmany(size, DEV_BSIZE);
	first = roundup(2 * DEV_BSIZE, volhdr->dp.dp_secbytes) / DEV_BSIZE;

	for (n = 0; n < SGI_SIZE_VOLDIR;) {
		if (volhdr->voldir[n].name[0]) {
			if (first < (betoh32(volhdr->voldir[n].block) +
			    howmany(betoh32(volhdr->voldir[n].bytes),
			        DEV_BSIZE)) &&
			    (first + blocks) >
			        betoh32(volhdr->voldir[n].block)) {

				first = roundup(
				    betoh32(volhdr->voldir[n].block) +
				    howmany(betoh32(volhdr->voldir[n].bytes),
				        DEV_BSIZE),
				    volhdr->dp.dp_secbytes / DEV_BSIZE);
#if DEBUG
				printf("allocate: "
				    "n=%d first=%d blocks=%d size=%d\n",
				    n, first, blocks, size);
				printf("%s %d %d\n", volhdr->voldir[n].name,
				    volhdr->voldir[n].block,
				    volhdr->voldir[n].bytes);
				printf("first=%d block=%d last=%d end=%d\n",
				    first, volhdr->voldir[n].block,
				    first + blocks - 1,
				    volhdr->voldir[n].block +
				    howmany(volhdr->voldir[n].bytes,
				        DEV_BSIZE));
#endif
				n = 0;
				continue;
			}
		}
		++n;
	}
	if (first + blocks > lbl.d_secperunit)
		first = -1;
	/* XXX assumes volume header is partition 8 */
	/* XXX assumes volume header starts at 0? */
	if (first + blocks >= betoh32(volhdr->partitions[8].blocks))
		first = -1;
	return (first);
}

void
checksum_vol(void)
{
	int32_t *l;
	int i;

	volhdr->checksum = checksum = 0;
	l = (int32_t *)buf;
	for (i = 0; i < sizeof(struct sgilabel) / sizeof(int32_t); ++i)
		checksum += betoh32(l[i]);
	volhdr->checksum = htobe32(-checksum);
}

void
usage(void)
{
	extern char *__progname;

	fprintf(stderr,
	    "usage: %s [-q] disk\n"
	    "       %s [-q] -d vhfilename disk\n"
	    "       %s [-q] -i [-h vhsize] disk\n"
	    "       %s [-q] -l vhfilename1 vhfilename2 disk\n"
	    "       %s [-q] -r vhfilename diskfilename disk\n"
	    "       %s [-q] -w vhfilename diskfilename disk\n",
	    __progname, __progname, __progname, __progname, __progname,
	    __progname);

	exit(1);
}
