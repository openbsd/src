/*	$OpenBSD: mbr.c,v 1.26 2011/02/21 19:26:13 krw Exp $	*/

/*
 * Copyright (c) 1997 Tobias Weingartner
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

#include <err.h>
#include <errno.h>
#include <util.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <machine/param.h>
#include "disk.h"
#include "misc.h"
#include "mbr.h"
#include "part.h"


void
MBR_init(disk_t *disk, mbr_t *mbr)
{
	daddr64_t i;
	int adj;

	/* Fix up given mbr for this disk */
	mbr->part[0].flag = 0;
	mbr->part[1].flag = 0;
	mbr->part[2].flag = 0;

	mbr->part[3].flag = DOSACTIVE;
	mbr->signature = DOSMBR_SIGNATURE;

	/* Use whole disk, save for first head, on first cyl. */
	mbr->part[3].id = DOSPTYP_OPENBSD;
	mbr->part[3].scyl = 0;
	mbr->part[3].shead = 1;
	mbr->part[3].ssect = 1;

	/* Go right to the end */
	mbr->part[3].ecyl = disk->real->cylinders - 1;
	mbr->part[3].ehead = disk->real->heads - 1;
	mbr->part[3].esect = disk->real->sectors;

	/* Fix up start/length fields */
	PRT_fix_BN(disk, &mbr->part[3], 3);

#if defined(__powerpc__) || defined(__mips__)
	/* Now fix up for the MS-DOS boot partition on PowerPC. */
	mbr->part[0].flag = DOSACTIVE;	/* Boot from dos part */
	mbr->part[3].flag = 0;
	mbr->part[3].ns += mbr->part[3].bs;
	mbr->part[3].bs = mbr->part[0].bs + mbr->part[0].ns;
	mbr->part[3].ns -= mbr->part[3].bs;
	PRT_fix_CHS(disk, &mbr->part[3]);
	if ((mbr->part[3].shead != 1) || (mbr->part[3].ssect != 1)) {
		/* align the partition on a cylinder boundary */
		mbr->part[3].shead = 0;
		mbr->part[3].ssect = 1;
		mbr->part[3].scyl += 1;
	}
	/* Fix up start/length fields */
	PRT_fix_BN(disk, &mbr->part[3], 3);
#endif

	/* Start OpenBSD MBR partition on a power of 2 block number. */
	i = 1;
	while (i < DL_SECTOBLK(&dl, mbr->part[3].bs))
		i *= 2;
	i = DL_BLKTOSEC(&dl, i);
	adj = i - mbr->part[3].bs;
	mbr->part[3].bs += adj;
	mbr->part[3].ns -= adj; 
	PRT_fix_CHS(disk, &mbr->part[3]);
}

void
MBR_parse(disk_t *disk, char *mbr_buf, off_t offset, off_t reloff, mbr_t *mbr)
{
	int i;

	memcpy(mbr->code, mbr_buf, MBR_CODE_SIZE);
	mbr->offset = offset;
	mbr->reloffset = reloff;
	mbr->signature = getshort(&mbr_buf[MBR_SIG_OFF]);

	for (i = 0; i < NDOSPART; i++)
		PRT_parse(disk, &mbr_buf[MBR_PART_OFF + MBR_PART_SIZE * i],
		    offset, reloff, &mbr->part[i]);
}

void
MBR_make(mbr_t *mbr, char *mbr_buf)
{
	int i;

	memcpy(mbr_buf, mbr->code, MBR_CODE_SIZE);
	putshort(&mbr_buf[MBR_SIG_OFF], mbr->signature);

	for (i = 0; i < NDOSPART; i++)
		PRT_make(&mbr->part[i], mbr->offset, mbr->reloffset,
		    &mbr_buf[MBR_PART_OFF + MBR_PART_SIZE * i]);
}

void
MBR_print(mbr_t *mbr, char *units)
{
	int i;

	/* Header */
	printf("Signature: 0x%X\n",
	    (int)mbr->signature);
	PRT_print(0, NULL, units);

	/* Entries */
	for (i = 0; i < NDOSPART; i++)
		PRT_print(i, &mbr->part[i], units);
}

int
MBR_read(int fd, off_t where, char *buf)
{
	const int secsize = unit_types[SECTORS].conversion;
	ssize_t len;
	off_t off;
	char *secbuf;

	where *= secsize;
	off = lseek(fd, where, SEEK_SET);
	if (off != where)
		return (-1);

	secbuf = malloc(secsize);
	if (secbuf == NULL)
		return (-1);
	bzero(secbuf, secsize);

	len = read(fd, secbuf, secsize);
	bcopy(secbuf, buf, DEV_BSIZE);
	free(secbuf);

	if (len == -1)
		return (-1);
	if (len != secsize) {
		/* short read */
		errno = EIO;
		return (-1);
	}

	return (0);
}

int
MBR_write(int fd, off_t where, char *buf)
{
	const int secsize = unit_types[SECTORS].conversion;
	ssize_t len;
	off_t off;
	char *secbuf;

	/* Read the sector we want to store the MBR in. */
	where *= secsize;
	off = lseek(fd, where, SEEK_SET);
	if (off != where)
		return (-1);

	secbuf = malloc(secsize);
	if (secbuf == NULL)
		return (-1);
	bzero(secbuf, secsize);

	len = read(fd, secbuf, secsize);
	if (len == -1 || len != secsize)
		goto done;

	/*
	 * Place the new MBR in the first DEV_BSIZE bytes of the sector and
	 * write the sector back to "disk".
	 */
	bcopy(buf, secbuf, DEV_BSIZE);
	off = lseek(fd, where, SEEK_SET);
	if (off == where)
		len = write(fd, secbuf, secsize);
	else
		len = -1;

done:
	free(secbuf);
	if (len == -1)
		return (-1);
	if (len != secsize) {
		/* short read or write */
		errno = EIO;
		return (-1);
	}

	ioctl(fd, DIOCRLDINFO, 0);
	return (0);
}

/*
 * Copy partition table from the disk indicated
 * to the supplied mbr structure
 */
void
MBR_pcopy(disk_t *disk, mbr_t *mbr)
{
	int i, fd, error, offset = 0, reloff = 0;
	mbr_t mbrd;
	char mbr_disk[DEV_BSIZE];

	fd = DISK_open(disk->name, O_RDONLY);
	error = MBR_read(fd, offset, mbr_disk);
	close(fd);
	if (error == -1)
		return;
	MBR_parse(disk, mbr_disk, offset, reloff, &mbrd);
	for (i = 0; i < NDOSPART; i++) {
		PRT_parse(disk, &mbr_disk[MBR_PART_OFF +
		    MBR_PART_SIZE * i],
		    offset, reloff, &mbr->part[i]);
	}
}
