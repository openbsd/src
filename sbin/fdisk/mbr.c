/*	$OpenBSD: mbr.c,v 1.43 2015/02/10 01:20:10 krw Exp $	*/

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

#include <sys/param.h>	/* DEV_BSIZE */
#include <sys/fcntl.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/disklabel.h>
#include <sys/dkio.h>
#include <err.h>
#include <errno.h>
#include <util.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <memory.h>

#include "disk.h"
#include "part.h"
#include "misc.h"
#include "mbr.h"

void
MBR_init_GPT(struct disk *disk, struct mbr *mbr)
{
	/* Initialize a protective MBR for GPT. */
	bzero(&mbr->part, sizeof(mbr->part));

	/* Use whole disk, starting after MBR. */
	mbr->part[0].id = DOSPTYP_EFI;
	mbr->part[0].bs = 1;
	mbr->part[0].ns = disk->size - 1;

	/* Fix up start/length fields. */
	PRT_fix_CHS(disk, &mbr->part[0]);
}

void
MBR_init(struct disk *disk, struct mbr *mbr)
{
	extern int g_flag;
	u_int64_t adj;
	daddr_t i;

	if (g_flag) {
		MBR_init_GPT(disk, mbr);
		return;
	}

	/* Fix up given mbr for this disk */
	mbr->part[0].flag = 0;
	mbr->part[1].flag = 0;
	mbr->part[2].flag = 0;

	mbr->part[3].flag = DOSACTIVE;
	mbr->signature = DOSMBR_SIGNATURE;

	/* Use whole disk. Reserve first track, or first cyl, if possible. */
	mbr->part[3].id = DOSPTYP_OPENBSD;
	if (disk->heads > 1)
		mbr->part[3].shead = 1;
	else
		mbr->part[3].shead = 0;
	if (disk->heads < 2 && disk->cylinders > 1)
		mbr->part[3].scyl = 1;
	else
		mbr->part[3].scyl = 0;
	mbr->part[3].ssect = 1;

	/* Go right to the end */
	mbr->part[3].ecyl = disk->cylinders - 1;
	mbr->part[3].ehead = disk->heads - 1;
	mbr->part[3].esect = disk->sectors;

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
	adj = DL_BLKTOSEC(&dl, i) - mbr->part[3].bs;
	mbr->part[3].bs += adj;
	mbr->part[3].ns -= adj; 
	PRT_fix_CHS(disk, &mbr->part[3]);
}

void
MBR_parse(struct disk *disk, struct dos_mbr *dos_mbr, off_t offset,
    off_t reloff, struct mbr *mbr)
{
	struct dos_partition dos_parts[NDOSPART];
	int i;

	memcpy(mbr->code, dos_mbr->dmbr_boot, sizeof(mbr->code));
	mbr->offset = offset;
	mbr->reloffset = reloff;
	mbr->signature = letoh16(dos_mbr->dmbr_sign);

	memcpy(dos_parts, dos_mbr->dmbr_parts, sizeof(dos_parts));

	for (i = 0; i < NDOSPART; i++)
		PRT_parse(disk, &dos_parts[i], offset, reloff, &mbr->part[i]);
}

void
MBR_make(struct mbr *mbr, struct dos_mbr *dos_mbr)
{
	struct dos_partition dos_partition;
	int i;

	memcpy(dos_mbr->dmbr_boot, mbr->code, sizeof(dos_mbr->dmbr_boot));
	dos_mbr->dmbr_sign = htole16(DOSMBR_SIGNATURE);

	for (i = 0; i < NDOSPART; i++) {
		PRT_make(&mbr->part[i], mbr->offset, mbr->reloffset,
		    &dos_partition);
		memcpy(&dos_mbr->dmbr_parts[i], &dos_partition,
		    sizeof(dos_mbr->dmbr_parts[i]));
	}
}

void
MBR_print(struct mbr *mbr, char *units)
{
	int i;

	/* Header */
	printf("Signature: 0x%X\n", (int)mbr->signature);
	PRT_print(0, NULL, units);

	/* Entries */
	for (i = 0; i < NDOSPART; i++)
		PRT_print(i, &mbr->part[i], units);
}

int
MBR_read(int fd, off_t where, struct dos_mbr *dos_mbr)
{
	char *secbuf;

	secbuf = MBR_readsector(fd, where);
	if (secbuf == NULL)
		return (-1);

	memcpy(dos_mbr, secbuf, sizeof(*dos_mbr));
	free(secbuf);

	return (0);
}

int
MBR_write(int fd, off_t where, struct dos_mbr *dos_mbr)
{
	char *secbuf;

	secbuf = MBR_readsector(fd, where);
	if (secbuf == NULL)
		return (-1);

	/*
	 * Place the new MBR at the start of the sector and
	 * write the sector back to "disk".
	 */
	memcpy(secbuf, dos_mbr, sizeof(*dos_mbr));
	MBR_writesector(fd, secbuf, where);
	ioctl(fd, DIOCRLDINFO, 0);

	free(secbuf);

	return (0);
}

/*
 * Parse the MBR partition table into 'mbr', leaving the rest of 'mbr'
 * untouched.
 */
void
MBR_pcopy(struct disk *disk, struct mbr *mbr)
{
	int i, fd, error;
	struct dos_mbr dos_mbr;
	struct dos_partition dos_parts[NDOSPART];

	fd = DISK_open(disk->name, O_RDONLY);
	error = MBR_read(fd, 0, &dos_mbr);
	close(fd);

	if (error == -1)
		return;

	memcpy(dos_parts, dos_mbr.dmbr_parts, sizeof(dos_parts));

	for (i = 0; i < NDOSPART; i++)
		PRT_parse(disk, &dos_parts[i], 0, 0, &mbr->part[i]);
}

/*
 * Read the sector at 'where' into a sector sized buf and return the latter.
 */
char *
MBR_readsector(int fd, off_t where)
{
	char *secbuf;
	const int secsize = unit_types[SECTORS].conversion;
	ssize_t len;
	off_t off;

	where *= secsize;
	off = lseek(fd, where, SEEK_SET);
	if (off != where)
		return (NULL);

	secbuf = calloc(1, secsize);
	if (secbuf == NULL)
		return (NULL);

	len = read(fd, secbuf, secsize);
	if (len == -1 || len != secsize) {
		free(secbuf);
		return (NULL);
	}

	return (secbuf);
}

/*
 * Write the sector sized 'secbuf' to the sector at 'where'.
 */
int
MBR_writesector(int fd, char *secbuf, off_t where)
{
	const int secsize = unit_types[SECTORS].conversion;
	ssize_t len;
	off_t off;

	len = -1;

	where *= secsize;
	off = lseek(fd, where, SEEK_SET);
	if (off == where)
		len = write(fd, secbuf, secsize);

	if (len == -1 || len != secsize) {
		/* short read or write */
		errno = EIO;
		return (-1);
	}

	return (0);
}

/*
 * If *dos_mbr has a 0xee or 0xef partition, nothing needs to happen. If no
 * such partition is present but the first or last sector on the disk has a
 * GPT, zero the GPT to ensure the MBR takes priority and fewer BIOSes get
 * confused.
 */
void
MBR_zapgpt(int fd, struct dos_mbr *dos_mbr, uint64_t lastsec)
{
	const int secsize = unit_types[SECTORS].conversion;
	struct dos_partition dos_parts[NDOSPART];
	char *secbuf;
	uint64_t sig;
	int i;

	memcpy(dos_parts, dos_mbr->dmbr_parts, sizeof(dos_parts));

	for (i = 0; i < NDOSPART; i++)
		if ((dos_parts[i].dp_typ == DOSPTYP_EFI) ||
		    (dos_parts[i].dp_typ == DOSPTYP_EFISYS))
			return;

	secbuf = MBR_readsector(fd, GPTSECTOR);
	if (secbuf == NULL)
		return;

	memcpy(&sig, secbuf, sizeof(sig));
	if (sig == GPTSIGNATURE) {
		memset(secbuf, 0, sizeof(sig));
		MBR_writesector(fd, secbuf, GPTSECTOR);
	}
	free(secbuf);

	secbuf = MBR_readsector(fd, lastsec);
	if (secbuf == NULL)
		return;

	memcpy(&sig, secbuf, sizeof(sig));
	if (sig == GPTSIGNATURE) {
		memset(secbuf, 0, sizeof(sig));
		MBR_writesector(fd, secbuf, lastsec);
	}
	free(secbuf);
}
